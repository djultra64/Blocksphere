#pragma once

#include "tetrisphere/linux_platform.h"

#include <SDL.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace tetrisphere {

struct AudioPacingSnapshot {
    std::uint32_t requested_frequency = 0;
    std::uint32_t obtained_frequency = 0;
    std::uint16_t device_buffer_frames = 0;
    std::uint64_t submissions = 0;
    std::uint64_t submitted_frames = 0;
    // SDL's software queue being empty is a starvation risk, not proof that
    // the hardware buffer has already run dry.
    std::uint64_t empty_software_queue_count = 0;
    std::size_t current_software_queue_frames = 0;
};

class AudioOutput {
public:
    ~AudioOutput();
    bool open(std::uint32_t frequency, std::string& error);
    // Returns true only when an output-device event changed this stream.
    bool handle_event(const SDL_Event& event, std::string& error);
    bool queue(const std::int16_t* samples, std::size_t count, std::string& error);
    std::size_t frames_remaining();
    bool full();
    std::size_t current_bytes();
    AudioPacingSnapshot pacing_snapshot() const;
    SDL_AudioDeviceID id() const;
    void close();

private:
    bool reopen_locked(std::string& error);
    void synchronize_locked();

    mutable std::mutex mutex_;
    SDL_AudioDeviceID device_ = 0;
    std::uint32_t frequency_ = 48'000;
    AudioPacingSnapshot pacing_{};
    std::size_t fifo_capacity_ = 4;
    AudioDmaFifo fifo_;
};

} // namespace tetrisphere
