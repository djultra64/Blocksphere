#include "tetrisphere/linux_platform.h"
#include "tetrisphere/audio_length_pacer.h"

#include "recomp.h"
#include "tetrisphere/runtime.h"
#include "ultramodern/ultramodern.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

std::atomic<std::uint64_t> tetrisphere_ai_attempts{0};
std::atomic<std::uint64_t> tetrisphere_ai_fifo_rejections{0};
std::atomic<std::uint64_t> tetrisphere_ai_fifo_wait_events{0};
std::atomic<std::uint64_t> tetrisphere_ai_fifo_waits_ms{0};
std::atomic<std::uint64_t> tetrisphere_ai_length_caps{0};

namespace {
bool audio_transition_probe_enabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("TETRISPHERE_AUDIO_TRANSITION_PROBE");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

std::atomic<std::uint32_t> current_ai_frequency{0};
std::atomic<unsigned> transition_samples_remaining{0};
std::atomic<unsigned> transition_dma_remaining{0};
std::atomic<std::uint64_t> audio_probe_ordinal{0};
tetrisphere::AudioFifoDrainGate fifo_drain_gate;
}

extern "C" {

void osAiSetFrequency_recomp(std::uint8_t* rdram, recomp_context* ctx) {
    const auto frequency = static_cast<std::uint32_t>(ctx->r4);
    if (frequency < 8'000u || frequency > 96'000u) {
        tetrisphere::fatal_unsupported("osAiSetFrequency", frequency,
                                       0u, "linux-audio-frequency");
    }
    if (audio_transition_probe_enabled()) {
        const auto previous = current_ai_frequency.load(std::memory_order_relaxed);
        const auto ordinal = audio_probe_ordinal.fetch_add(1, std::memory_order_relaxed);
        std::fprintf(stderr,
                     "{\"event\":\"audio_transition_frequency\",\"ordinal\":%llu,\"old_hz\":%u,"
                     "\"new_hz\":%u,\"old_head_bytes\":%zu,\"guest_base_frames\":%u,"
                     "\"guest_min_frames\":%u}\n",
                     static_cast<unsigned long long>(ordinal), previous, frequency,
                     tetrisphere::linux_audio_current_bytes(),
                     static_cast<unsigned>(MEM_W(0, S32(0x80106F68u))),
                     static_cast<unsigned>(MEM_W(0, S32(0x80106F64u))));
        transition_samples_remaining.store(32, std::memory_order_relaxed);
        transition_dma_remaining.store(8, std::memory_order_relaxed);
    }
    ultramodern::set_audio_frequency(frequency);
    current_ai_frequency.store(frequency, std::memory_order_relaxed);
    ctx->r2 = frequency;
}

void osAiSetNextBuffer_recomp(std::uint8_t* rdram, recomp_context* ctx) {
    tetrisphere_ai_attempts.fetch_add(1, std::memory_order_relaxed);
    const auto byte_count = static_cast<std::uint32_t>(ctx->r5);
    if (audio_transition_probe_enabled()) {
        const auto remaining = transition_dma_remaining.load(std::memory_order_relaxed);
        if (remaining > 0 || (byte_count & 3u) != 0u || byte_count > 0x10000u) {
            const auto ordinal = audio_probe_ordinal.fetch_add(1, std::memory_order_relaxed);
            if (remaining > 0) transition_dma_remaining.fetch_sub(1, std::memory_order_relaxed);
            std::fprintf(stderr,
                         "{\"event\":\"audio_transition_dma\",\"ordinal\":%llu,"
                         "\"byte_count\":%u,\"signed_byte_count\":%d,"
                         "\"frequency_hz\":%u,\"head_bytes\":%zu,"
                         "\"guest_base_frames\":%u,\"guest_min_frames\":%u}\n",
                         static_cast<unsigned long long>(ordinal), byte_count,
                         static_cast<std::int32_t>(byte_count),
                         current_ai_frequency.load(std::memory_order_relaxed),
                         tetrisphere::linux_audio_current_bytes(),
                         static_cast<unsigned>(MEM_W(0, S32(0x80106F68u))),
                         static_cast<unsigned>(MEM_W(0, S32(0x80106F64u))));
        }
    }
    if ((byte_count & 3u) != 0u || byte_count > 0x10000u) {
        tetrisphere::fatal_unsupported("osAiSetNextBuffer", byte_count,
                                       0x8007EDC8u, "linux-audio-dma-size");
    }
    const auto fifo_action = fifo_drain_gate.observe(tetrisphere::linux_audio_fifo_full());
    if (fifo_action == tetrisphere::AudioFifoDrainAction::Reject) {
        tetrisphere_ai_fifo_rejections.fetch_add(1, std::memory_order_relaxed);
        ctx->r2 = static_cast<gpr>(-1);
        return;
    }
    if (fifo_action == tetrisphere::AudioFifoDrainAction::Wait) {
        tetrisphere_ai_fifo_wait_events.fetch_add(1, std::memory_order_relaxed);
        unsigned waited = 0;
        const auto max_waits = tetrisphere::linux_audio_drain_wait_budget_ms();
        const bool ready = tetrisphere::wait_for_audio_fifo_slot(
            [] { return tetrisphere::linux_audio_fifo_full(); },
            [&] {
                ++waited;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }, max_waits);
        tetrisphere_ai_fifo_waits_ms.fetch_add(waited, std::memory_order_relaxed);
        if (!ready) {
            fifo_drain_gate.timed_out();
            tetrisphere_ai_fifo_rejections.fetch_add(1, std::memory_order_relaxed);
            std::fprintf(stderr,
                         "{\"event\":\"audio_fifo_drain_timeout\",\"frequency_hz\":%u,"
                         "\"head_bytes\":%zu,\"waits_ms\":%u,\"max_waits_ms\":%u}\n",
                         current_ai_frequency.load(std::memory_order_relaxed),
                         tetrisphere::linux_audio_current_bytes(), waited, max_waits);
            ctx->r2 = static_cast<gpr>(-1);
            return;
        }
        fifo_drain_gate.observe(false);
    }
    ultramodern::queue_audio_buffer(rdram, ctx->r4, byte_count);
    ctx->r2 = 0;
}

void osAiGetLength_recomp(std::uint8_t* rdram, recomp_context* ctx) {
    static const bool use_raw_length = [] {
        const char* mode = std::getenv("TETRISPHERE_AUDIO_LENGTH_MODE");
        return mode != nullptr && std::strcmp(mode, "raw") == 0;
    }();
    const auto base = static_cast<std::uint32_t>(MEM_W(0, S32(0x80106F68u)));
    // Keep the explicit raw diagnostic mode tied to the FIFO head.
    const auto reported = use_raw_length
        ? static_cast<std::uint32_t>(tetrisphere::linux_audio_current_bytes())
        : ultramodern::get_remaining_audio_bytes();
    const auto length = base == 0 || use_raw_length
        ? reported : tetrisphere::guest_safe_audio_length(reported, base);
    if (length != reported)
        tetrisphere_ai_length_caps.fetch_add(1, std::memory_order_relaxed);
    ctx->r2 = length;
    if (audio_transition_probe_enabled()) {
        const auto reported = static_cast<std::uint32_t>(ctx->r2);
        const auto before = transition_samples_remaining.load(std::memory_order_relaxed);
        if (before > 0) transition_samples_remaining.fetch_sub(1, std::memory_order_relaxed);
        if (before > 0 || reported / 4u > base + 96u) {
            const auto ordinal = audio_probe_ordinal.fetch_add(1, std::memory_order_relaxed);
            std::fprintf(stderr,
                         "{\"event\":\"audio_transition_length\",\"ordinal\":%llu,"
                         "\"frequency_hz\":%u,\"guest_base_frames\":%u,"
                         "\"guest_min_frames\":%u,\"head_bytes\":%zu,"
                         "\"reported_bytes\":%u,\"raw_mode\":%s}\n",
                         static_cast<unsigned long long>(ordinal),
                         current_ai_frequency.load(std::memory_order_relaxed),
                         base, static_cast<unsigned>(MEM_W(0, S32(0x80106F64u))),
                         tetrisphere::linux_audio_current_bytes(), reported,
                         use_raw_length ? "true" : "false");
        }
    }
}

} // extern "C"
