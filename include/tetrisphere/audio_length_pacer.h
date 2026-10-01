#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>

namespace tetrisphere {

// SDL removes one callback of audio from its software queue before the
// device plays it. Report the remaining queued backlog to the guest while
// leaving that callback as a safety reserve. ultramodern applies its own
// half-VI offset after this callback returns.
constexpr std::size_t audio_feedback_frames(std::size_t queued_frames,
                                            std::size_t callback_frames) {
    return queued_frames > callback_frames ? queued_frames - callback_frames : 0;
}

constexpr std::uint32_t guest_safe_audio_length(std::uint32_t reported_bytes,
                                                 std::uint32_t base_frames) {
    const auto maximum = (static_cast<std::uint64_t>(base_frames) + 96u) * 4u;
    return reported_bytes > maximum ? static_cast<std::uint32_t>(maximum)
                                    : reported_bytes;
}

enum class AudioFifoDrainAction { Queue, Wait, Reject };

// A stalled device gets one bounded wait. Further guest submissions fail fast
// until the SDL queue drains or the device is reopened and its FIFO clears.
class AudioFifoDrainGate {
public:
    AudioFifoDrainAction observe(bool full) {
        if (!full) {
            timed_out_ = false;
            return AudioFifoDrainAction::Queue;
        }
        return timed_out_ ? AudioFifoDrainAction::Reject
                          : AudioFifoDrainAction::Wait;
    }
    void timed_out() { timed_out_ = true; }

private:
    bool timed_out_ = false;
};

template <typename Full, typename Wait>
bool wait_for_audio_fifo_slot(Full&& full, Wait&& wait, unsigned max_waits) {
    for (unsigned waits = 0;; ++waits) {
        if (!full()) return true;
        if (waits == max_waits) return false;
        wait();
    }
}

// SDL's queued-audio size falls when the device consumes a whole callback
// buffer. Permit two callback periods plus scheduler slack, with a hard cap.
constexpr unsigned audio_drain_wait_budget_ms(std::uint32_t buffer_frames,
                                               std::uint32_t frequency_hz) {
    if (buffer_frames == 0 || frequency_hz == 0) return 100u;
    const auto periods_ms =
        (2ull * buffer_frames * 1000ull + frequency_hz - 1ull) / frequency_hz;
    const auto requested = periods_ms + 5ull;
    return static_cast<unsigned>(requested < 16ull ? 16ull :
                                 requested > 100ull ? 100ull : requested);
}

// The guest sizes its next AI DMA from base_frames - (AI_LEN / 4) + 96.
// Let the real SDL queue drain before returning a length that would make its
// signed 16-bit descriptor negative. No audio bytes are changed or discarded.
template <typename ReadLength, typename Wait>
std::optional<std::uint32_t> wait_for_audio_length_budget(
    ReadLength&& read_length, Wait&& wait, std::uint32_t base_frames,
    unsigned max_waits) {
    const auto budget_bytes = (static_cast<std::uint64_t>(base_frames) + 96u) * 4u;
    for (unsigned waits = 0;; ++waits) {
        const auto reported = static_cast<std::uint32_t>(read_length());
        if (base_frames == 0 || reported <= budget_bytes) return reported;
        if (waits == max_waits) return std::nullopt;
        wait();
    }
}

} // namespace tetrisphere
