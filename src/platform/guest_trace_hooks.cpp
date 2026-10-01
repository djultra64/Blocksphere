#include "tetrisphere/guest_trace_hooks.h"

#include "tetrisphere/canonical_trace.h"

#ifdef TETRISPHERE_LINUX_DEMO

#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace {
constexpr std::size_t max_consumed_per_drain = 720; // 180 ring slots * 4 N64 ports
constexpr std::uint64_t default_max_frames = 16384;
constexpr std::uint64_t absolute_max_frames = 1000000;

struct Recorder {
    std::mutex mutex;
    std::ofstream output;
    std::atomic<std::uint64_t> drain_count{0};
    std::atomic<std::uint64_t> rng_write_count{0};
    std::uint64_t max_frames = default_max_frames;
    bool enabled = false;

    Recorder() {
        const char* path = std::getenv("TETRISPHERE_TRACE_PATH");
        if (path == nullptr || *path == '\0') return;
        const char* max_env = std::getenv("TETRISPHERE_TRACE_MAX_FRAMES");
        if (max_env != nullptr && *max_env != '\0') {
            char* end = nullptr;
            errno = 0;
            const unsigned long long parsed = std::strtoull(max_env, &end, 10);
            if (errno == 0 && end != max_env && *end == '\0' && parsed > 0 &&
                parsed <= absolute_max_frames) max_frames = parsed;
        }
        // A trace is an explicit local artifact. Never silently truncate one.
        if (std::filesystem::exists(path)) return;
        output.open(path, std::ios::out | std::ios::trunc);
        if (!output) return;
        output << "tetrisphere-guest-trace-v1\n";
        enabled = true;
    }
};

Recorder& recorder() {
    static Recorder instance;
    return instance;
}

struct PendingDrain {
    bool active = false;
    tetrisphere::CanonicalFrame frame;
};
thread_local PendingDrain pending;

std::uint16_t guest_halfword(std::uint8_t* rdram, std::uint32_t address) {
    const auto guest = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int32_t>(address)));
    return MEM_HU(0, guest);
}
std::uint32_t guest_word(std::uint8_t* rdram, std::uint32_t address) {
    const auto guest = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int32_t>(address)));
    return MEM_W(0, guest);
}
} // namespace

extern "C" void tetrisphere_trace_input_drain_begin(std::uint8_t* rdram,
                                                        recomp_context* ctx) {
    static_cast<void>(ctx);
    auto& state = recorder();
    if (!state.enabled) return;
    const auto ordinal = state.drain_count.fetch_add(1, std::memory_order_relaxed);
    if (ordinal >= state.max_frames) return;
    pending.frame = {};
    pending.active = true;
    pending.frame.tick = ordinal; // Input-drain ordinal; game tick equivalence is unverified.
    pending.frame.input_ring_read_cursor_begin = guest_halfword(rdram, 0x800E1710u);
    pending.frame.input_ring_write_cursor_begin = guest_halfword(rdram, 0x800E1714u);
}

extern "C" void tetrisphere_trace_input_consumed(std::uint8_t* rdram,
                                                    recomp_context* ctx) {
    if (!pending.active) return;
    auto& events = pending.frame.consumed_input;
    if (events.size() >= max_consumed_per_drain) {
        pending.frame.consumed_input_truncated = true;
        return;
    }
    tetrisphere::ConsumedInputEvent event{};
    event.dispatch_ordinal = static_cast<std::uint32_t>(events.size());
    event.port = static_cast<std::uint8_t>(ctx->r6);
    event.raw_word0 = static_cast<std::uint16_t>(ctx->r4);
    event.raw_word1 = static_cast<std::uint16_t>(ctx->r5);
    event.ring_read_cursor = guest_halfword(rdram, 0x800E1710u);
    event.ring_write_cursor = guest_halfword(rdram, 0x800E1714u);
    events.push_back(event);
}

extern "C" void tetrisphere_trace_input_drain_end(std::uint8_t* rdram,
                                                      recomp_context* ctx) {
    static_cast<void>(ctx);
    if (!pending.active) return;
    pending.active = false;
    auto& frame = pending.frame;
    frame.input_ring_read_cursor = guest_halfword(rdram, 0x800E1710u);
    frame.input_ring_write_cursor = guest_halfword(rdram, 0x800E1714u);
    frame.global_rng_word = guest_word(rdram, 0x800E2AA0u);
    frame.rng_update_count = recorder().rng_write_count.load(std::memory_order_relaxed);
    frame.context_steps_p1 = guest_word(rdram, 0x800E4478u);
    frame.dispatch_count = guest_word(rdram, 0x800E4480u);
    // These are raw words at statically confirmed addresses. Displayed score,
    // clock duration and P2 gameplay semantics still need runtime/reference QA.
    frame.player_rng_word[0] = guest_word(rdram, 0x801125BCu);
    frame.player_rng_word[1] = guest_word(rdram, 0x80115824u);
    frame.candidate_score_offset_2af8[0] = guest_word(rdram, 0x80112D18u);
    frame.candidate_score_offset_2af8[1] = guest_word(rdram, 0x80115F80u);
    frame.candidate_timer_offset_2b0c[0] = guest_word(rdram, 0x80112D2Cu);
    frame.candidate_timer_offset_2b0c[1] = guest_word(rdram, 0x80115F94u);
    frame.candidate_hud_word_800e241c = guest_word(rdram, 0x800E241Cu);
    frame.candidate_delta_word_80160c60 = guest_word(rdram, 0x80160C60u);
    auto& state = recorder();
    std::lock_guard lock(state.mutex);
    state.output << tetrisphere::serialize_canonical_frame(frame)
                 << "end-canonical-frame\n";
    state.output.flush();
}

extern "C" void tetrisphere_trace_rng_write(std::uint8_t* rdram,
                                               recomp_context* ctx) {
    static_cast<void>(rdram);
    static_cast<void>(ctx);
    if (recorder().enabled)
        recorder().rng_write_count.fetch_add(1, std::memory_order_relaxed);
}

#else
extern "C" void tetrisphere_trace_input_drain_begin(std::uint8_t*, recomp_context*) {}
extern "C" void tetrisphere_trace_input_consumed(std::uint8_t*, recomp_context*) {}
extern "C" void tetrisphere_trace_input_drain_end(std::uint8_t*, recomp_context*) {}
extern "C" void tetrisphere_trace_rng_write(std::uint8_t*, recomp_context*) {}
#endif
