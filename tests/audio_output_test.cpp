#include "tetrisphere/audio_output.h"
#include "tetrisphere/audio_length_pacer.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool verify_transition_length_budget() {
    // func_8007ED88 stores this aligned result as a signed 16-bit DMA length.
    const auto guest_next_frames = [](std::uint32_t ai_length_bytes) {
        const auto value = (512 - static_cast<int>(ai_length_bytes / 4u) + 96) & 0xFFF0;
        return static_cast<std::int16_t>(value);
    };
    const auto mediated_length = [](std::uint32_t head_frames) {
        // N64ModernRuntime reports half a VI less at 30 kHz: 500 bytes.
        return head_frames * 4u - 500u;
    };
    if (guest_next_frames(mediated_length(720)) < 0 ||
        guest_next_frames(mediated_length(733)) < 0 ||
        guest_next_frames(mediated_length(734)) != -16 ||
        guest_next_frames(mediated_length(832)) != -112) {
        std::cerr << "guest 30 kHz DMA threshold differs from observed fatal\n";
        return false;
    }

    tetrisphere::AudioDmaFifo fifo{4};
    if (!fifo.submit(832) || !fifo.submit(496)) return false;
    std::size_t queued_frames = 832 + 496;
    unsigned waits = 0;
    const auto result = tetrisphere::wait_for_audio_length_budget(
        [&] { return mediated_length(fifo.current_frames()); },
        [&] {
            queued_frames -= waits == 0 ? 52u : 47u;
            fifo.synchronize(queued_frames);
            ++waits;
        }, 512u, 16u);
    if (!result || *result != mediated_length(733) || waits != 2 ||
        fifo.current_frames() != 733 ||
        guest_next_frames(*result) < 0) {
        std::cerr << "AI length pacer altered length or missed natural drain\n";
        return false;
    }
    waits = 0;
    const auto stuck = tetrisphere::wait_for_audio_length_budget(
        [&] { return mediated_length(832); }, [&] { ++waits; }, 512u, 3u);
    if (stuck || waits != 3) {
        std::cerr << "AI length pacer did not stop after bounded no-drain wait\n";
        return false;
    }
    // SDL consumes queued audio in device-buffer periods. A 1024-frame
    // buffer at 30 kHz can remain unchanged for about 34 ms even when healthy.
    const auto callback_budget = tetrisphere::audio_drain_wait_budget_ms(1024, 30'000);
    if (callback_budget < 70 || callback_budget > 100) {
        std::cerr << "AI length wait shorter than two SDL callbacks\n";
        return false;
    }
    waits = 0;
    const auto delayed = tetrisphere::wait_for_audio_length_budget(
        [&] { return mediated_length(waits < 34 ? 809u : 560u); },
        [&] { ++waits; }, 512u, callback_budget);
    if (!delayed || waits != 34 || *delayed != mediated_length(560)) {
        std::cerr << "AI length pacer timed out before SDL callback drain\n";
        return false;
    }
    return true;
}

bool verify_queued_audio_feedback() {
    // One SDL callback is already on the device side of the software queue.
    // With 2144 frames queued, 1120 should participate in the guest's
    // feedback; ultramodern then applies its existing half-VI offset.
    if (tetrisphere::audio_feedback_frames(2144, 1024) != 1120 ||
        tetrisphere::audio_feedback_frames(800, 1024) != 0) {
        std::cerr << "AI feedback did not account for the whole SDL queue\n";
        return false;
    }
    if (tetrisphere::guest_safe_audio_length(9000, 736) != 3328 ||
        tetrisphere::guest_safe_audio_length(1500, 736) != 1500) {
        std::cerr << "AI feedback exceeded signed guest DMA budget\n";
        return false;
    }
    unsigned waits = 0;
    const auto drained = tetrisphere::wait_for_audio_fifo_slot(
        [&] { return waits < 2; }, [&] { ++waits; }, 3u);
    if (!drained || waits != 2) {
        std::cerr << "AI FIFO did not wait for ordered DMA completion\n";
        return false;
    }
    waits = 0;
    const auto stalled = tetrisphere::wait_for_audio_fifo_slot(
        [] { return true; }, [&] { ++waits; }, 3u);
    if (stalled || waits != 3) {
        std::cerr << "AI FIFO did not stop at stalled-device timeout\n";
        return false;
    }

    for (const auto frequency : {30'000u, 44'100u, 40'000u}) {
      for (const bool jitter : {false, true}) {
        const auto base = frequency == 44'100u ? 736u :
                          frequency == 40'000u ? 672u : 512u;
        const auto minimum = base - 16u;
        tetrisphere::AudioDmaFifo fifo{8};
        std::size_t queued = 0;
        std::uint64_t submitted = 0;
        std::uint64_t consumed = 0;
        std::uint64_t callback_accumulator = 0;
        std::uint64_t callback_index = 0;
        std::uint64_t vi_accumulator = 0;
        std::size_t peak = 0;
        unsigned dropped = 0;
        unsigned waited_ms = 0;
        std::vector<std::size_t> depths;
        // Exact integer clock: one millisecond per step, with 1024-frame
        // device callbacks and 60 Hz guest VI events. This catches feedback
        // that repeatedly fills the DMA FIFO or starves nominal production.
        for (unsigned millisecond = 0; millisecond < 100'000; ++millisecond) {
            callback_accumulator += frequency;
            for (;;) {
                const auto callback_threshold = !jitter ? 1'024'000u :
                    callback_index % 83u == 0u ? 1'424'000u :
                    callback_index % 83u == 1u ? 624'000u : 1'024'000u;
                if (callback_accumulator < callback_threshold) break;
                callback_accumulator -= callback_threshold;
                ++callback_index;
                consumed += std::min<std::size_t>(queued, 1024);
                queued = queued > 1024 ? queued - 1024 : 0;
                fifo.synchronize(queued);
            }
            vi_accumulator += 60;
            if (vi_accumulator >= 1000) {
                vi_accumulator -= 1000;
                const auto feedback = tetrisphere::audio_feedback_frames(queued, 1024);
                const auto offset_bytes = frequency / 60u;
                const auto mediated_bytes = feedback * 4u > offset_bytes
                    ? feedback * 4u - offset_bytes : 0u;
                const auto reported_frames = std::min<std::size_t>(
                    mediated_bytes / 4u, base + 96u);
                const auto suggested = (base - reported_frames + 96u) & ~15u;
                const auto dma = std::max<std::size_t>(suggested, minimum);
                if (dma != 0 && fifo.full()) {
                    // A rare full FIFO waits for the next device callback;
                    // subsequent guest VI events move by the same delay.
                    const auto threshold = !jitter ? 1'024'000u :
                        callback_index % 83u == 0u ? 1'424'000u :
                        callback_index % 83u == 1u ? 624'000u : 1'024'000u;
                    const auto delay = static_cast<unsigned>(
                        (threshold - callback_accumulator + frequency - 1u) / frequency);
                    waited_ms += delay;
                    millisecond += delay;
                    vi_accumulator += delay * 60u;
                    callback_accumulator = 0;
                    ++callback_index;
                    consumed += std::min<std::size_t>(queued, 1024);
                    queued = queued > 1024 ? queued - 1024 : 0;
                    fifo.synchronize(queued);
                }
                if (dma != 0 && !fifo.submit(dma)) ++dropped;
                else {
                    queued += dma;
                    submitted += dma;
                    peak = std::max(peak, queued);
                }
                depths.push_back(queued);
            }
        }
        const auto nominal = static_cast<std::uint64_t>(frequency) * 100u;
        std::nth_element(depths.begin(), depths.begin() + depths.size() / 2,
                         depths.end());
        const auto median_ms = depths[depths.size() / 2] * 1000u / frequency;
        const auto peak_ms = peak * 1000u / frequency;
        if (dropped != 0 || submitted < nominal * 99u / 100u ||
            submitted > nominal * 101u / 100u || consumed < nominal * 99u / 100u ||
            submitted != consumed + queued || median_ms > 100u || peak_ms > 120u ||
            waited_ms > 500u) {
            std::cerr << "queued AI feedback lost DMA or drifted at " << frequency
                      << " Hz: dropped=" << dropped << " submitted=" << submitted
                      << " peak=" << peak << " median_ms=" << median_ms
                      << " peak_ms=" << peak_ms << " consumed=" << consumed
                      << " jitter=" << jitter << " waited_ms=" << waited_ms << '\n';
            return false;
        }
        std::cout << "AI model " << frequency << "Hz jitter=" << jitter
                  << " submitted=" << submitted << " consumed=" << consumed
                  << " median_ms=" << median_ms << " peak_ms=" << peak_ms
                  << " waited_ms=" << waited_ms << '\n';
      }
    }
    return true;
}

bool verify_stalled_fifo_recovers() {
    tetrisphere::AudioFifoDrainGate gate;
    if (gate.observe(true) != tetrisphere::AudioFifoDrainAction::Wait) {
        std::cerr << "first full FIFO did not allow bounded drain wait\n";
        return false;
    }
    gate.timed_out();
    if (gate.observe(true) != tetrisphere::AudioFifoDrainAction::Reject ||
        gate.observe(true) != tetrisphere::AudioFifoDrainAction::Reject) {
        std::cerr << "stalled FIFO repeatedly blocked the guest\n";
        return false;
    }
    if (gate.observe(false) != tetrisphere::AudioFifoDrainAction::Queue ||
        gate.observe(true) != tetrisphere::AudioFifoDrainAction::Wait) {
        std::cerr << "FIFO drain did not re-enable normal submission\n";
        return false;
    }
    return true;
}

} // namespace

int main(int, char**) {
    if (!verify_transition_length_budget()) return 1;
    if (!verify_queued_audio_feedback()) return 1;
    if (!verify_stalled_fifo_recovers()) return 1;
    tetrisphere::AudioDmaFifo expanded_fifo{3};
    if (!expanded_fifo.submit(720) || !expanded_fifo.submit(720) ||
        !expanded_fifo.submit(720) || !expanded_fifo.full() ||
        expanded_fifo.submit(720)) {
        std::cerr << "expanded DMA FIFO did not retain three burst buffers\n";
        return 1;
    }
    expanded_fifo.synchronize(720);
    if (expanded_fifo.full() || !expanded_fifo.submit(720)) {
        std::cerr << "expanded DMA FIFO did not free an elapsed buffer\n";
        return 1;
    }
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_AUDIO) != 0) return 1;
    {
        tetrisphere::AudioOutput feedback_output;
        std::string feedback_error;
        std::array<std::int16_t, 1664> dma{};
        if (!feedback_output.open(44'100, feedback_error)) return 1;
        SDL_PauseAudioDevice(feedback_output.id(), 1);
        if (!feedback_output.queue(dma.data(), dma.size(), feedback_error) ||
            !feedback_output.queue(dma.data(), dma.size(), feedback_error) ||
            feedback_output.frames_remaining() != 640 ||
            feedback_output.current_bytes() != 3328) {
            std::cerr << "SDL callback feedback differs from queued DMA backlog\n";
            return 1;
        }
        for (unsigned i = 0; i < 6; ++i) {
            if (!feedback_output.queue(dma.data(), dma.size(), feedback_error)) {
                std::cerr << "default AI FIFO cannot absorb eight DMA bursts\n";
                return 1;
            }
        }
        if (!feedback_output.full()) {
            std::cerr << "default AI FIFO exceeded eight DMA slots\n";
            return 1;
        }
        tetrisphere::AudioFifoDrainGate live_gate;
        if (live_gate.observe(feedback_output.full()) !=
            tetrisphere::AudioFifoDrainAction::Wait) return 1;
        live_gate.timed_out();
        if (live_gate.observe(feedback_output.full()) !=
            tetrisphere::AudioFifoDrainAction::Reject) return 1;
        SDL_PauseAudioDevice(feedback_output.id(), 0);
        for (unsigned elapsed_ms = 0; elapsed_ms < 150 && feedback_output.full();
             elapsed_ms += 2) SDL_Delay(2);
        if (live_gate.observe(feedback_output.full()) !=
            tetrisphere::AudioFifoDrainAction::Queue) {
            std::cerr << "resumed SDL device did not clear stalled FIFO gate\n";
            return 1;
        }
        // A stopped SDL device can happen without a removal event. Its stale
        // FIFO must not prevent queue() from reopening the output stream.
        SDL_CloseAudioDevice(feedback_output.id());
        if (feedback_output.full() ||
            !feedback_output.queue(dma.data(), dma.size(), feedback_error) ||
            feedback_output.pacing_snapshot().submissions != 1) {
            std::cerr << "stopped output did not reopen before FIFO-full check\n";
            return 1;
        }
        if (!feedback_output.open(30'000, feedback_error)) return 1;
        SDL_PauseAudioDevice(feedback_output.id(), 1);
        if (feedback_output.frames_remaining() != 0 ||
            feedback_output.current_bytes() != 0 ||
            feedback_output.pacing_snapshot().obtained_frequency != 30'000 ||
            !feedback_output.queue(dma.data(), dma.size(), feedback_error)) {
            std::cerr << "frequency transition kept stale queued audio\n";
            return 1;
        }
    }
    tetrisphere::AudioOutput output;
    std::string error;
    if (!output.open(32'000, error) || output.id() == 0) {
        std::cerr << "open failed: " << error << '\n';
        return 1;
    }
    const auto original = output.id();
    std::array<std::int16_t, 512> samples{};
    if (!output.queue(samples.data(), samples.size(), error)) {
        std::cerr << "queue failed: " << error << '\n';
        return 1;
    }
    const auto pacing = output.pacing_snapshot();
    if (pacing.submissions != 1 || pacing.submitted_frames != 256 ||
        pacing.empty_software_queue_count != 0 ||
        pacing.requested_frequency != 32'000 ||
        pacing.obtained_frequency != 32'000 ||
        pacing.device_buffer_frames == 0) {
        std::cerr << "audio pacing counters did not describe first DMA\n";
        return 1;
    }
    SDL_Event unrelated{};
    unrelated.type = SDL_AUDIODEVICEREMOVED;
    unrelated.adevice.which = original + 100;
    if (output.handle_event(unrelated, error) || output.id() != original) {
        std::cerr << "unrelated device removed active output\n";
        return 1;
    }
    SDL_Event removed{};
    removed.type = SDL_AUDIODEVICEREMOVED;
    removed.adevice.which = original;
    removed.adevice.iscapture = 0;
    const bool handled = output.handle_event(removed, error);
    if (!handled || output.id() == 0 || output.full() ||
        SDL_GetAudioDeviceStatus(output.id()) != SDL_AUDIO_PLAYING) {
        std::cerr << "device removal did not recover a fresh output: " << error
                  << " handled=" << handled << " old=" << original
                  << " current=" << output.id() << " full=" << output.full() << '\n';
        return 1;
    }
    if (!output.queue(samples.data(), samples.size(), error)) {
        std::cerr << "recovered output rejected audio: " << error << '\n';
        return 1;
    }
    output.close();
    SDL_Quit();
    return 0;
}
