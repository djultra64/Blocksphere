#include "tetrisphere/audio_output.h"
#include "tetrisphere/audio_length_pacer.h"

#include <limits>
#include <cstdlib>
#include <cstring>

namespace tetrisphere {

AudioOutput::~AudioOutput() { close(); }

bool AudioOutput::reopen_locked(std::string& error) {
    if (device_ != 0) SDL_CloseAudioDevice(device_);
    device_ = 0;
    const char* capacity_env = std::getenv("TETRISPHERE_AUDIO_FIFO_CAPACITY");
    fifo_capacity_ = capacity_env != nullptr &&
        std::strcmp(capacity_env, "2") == 0 ? 2u :
        capacity_env != nullptr && std::strcmp(capacity_env, "3") == 0 ? 3u :
        capacity_env != nullptr && std::strcmp(capacity_env, "4") == 0 ? 4u : 8u;
    fifo_ = AudioDmaFifo{fifo_capacity_};
    pacing_ = AudioPacingSnapshot{};
    pacing_.requested_frequency = frequency_;
    SDL_AudioSpec wanted{};
    wanted.freq = static_cast<int>(frequency_);
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 2;
    wanted.samples = 1024;
    SDL_AudioSpec obtained{};
    device_ = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, 0);
    if (device_ == 0) {
        error = std::string("cannot open audio output: ") + SDL_GetError();
        return false;
    }
    pacing_.obtained_frequency = static_cast<std::uint32_t>(obtained.freq);
    pacing_.device_buffer_frames = obtained.samples;
    SDL_PauseAudioDevice(device_, 0);
    error.clear();
    return true;
}

bool AudioOutput::open(std::uint32_t frequency, std::string& error) {
    std::lock_guard lock(mutex_);
    if (frequency == 0 || frequency > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        error = "invalid audio frequency";
        return false;
    }
    frequency_ = frequency;
    return reopen_locked(error);
}

bool AudioOutput::handle_event(const SDL_Event& event, std::string& error) {
    std::lock_guard lock(mutex_);
    if (event.type == SDL_AUDIODEVICEREMOVED && event.adevice.iscapture == 0 &&
        event.adevice.which == device_ && device_ != 0) {
        reopen_locked(error);
        return true;
    }
    if (event.type == SDL_AUDIODEVICEADDED && event.adevice.iscapture == 0 &&
        device_ == 0) {
        reopen_locked(error);
        return true;
    }
    return false;
}

void AudioOutput::synchronize_locked() {
    const auto queued = device_ == 0 ? 0 : SDL_GetQueuedAudioSize(device_) / 4;
    fifo_.synchronize(queued);
}

bool AudioOutput::queue(const std::int16_t* samples, std::size_t count,
                        std::string& error) {
    std::lock_guard lock(mutex_);
    if (device_ == 0 || SDL_GetAudioDeviceStatus(device_) == SDL_AUDIO_STOPPED) {
        if (!reopen_locked(error)) return false;
    }
    const auto queued_before = SDL_GetQueuedAudioSize(device_) / 4u;
    fifo_.synchronize(queued_before);
    if (samples == nullptr || (count & 1u) != 0u || !fifo_.submit(count / 2u)) {
        error = "AI DMA FIFO full, empty or unaligned";
        return false;
    }
    if (SDL_QueueAudio(device_, samples, count * sizeof(std::int16_t)) != 0) {
        error = std::string("SDL audio queue failed: ") + SDL_GetError();
        fifo_ = AudioDmaFifo{fifo_capacity_};
        return false;
    }
    if (pacing_.submissions > 0 && queued_before == 0)
        ++pacing_.empty_software_queue_count;
    ++pacing_.submissions;
    pacing_.submitted_frames += count / 2u;
    error.clear();
    return true;
}

std::size_t AudioOutput::frames_remaining() {
    std::lock_guard lock(mutex_);
    synchronize_locked();
    const auto queued = device_ == 0 ? 0u : SDL_GetQueuedAudioSize(device_) / 4u;
    return audio_feedback_frames(queued, pacing_.device_buffer_frames);
}

bool AudioOutput::full() {
    std::lock_guard lock(mutex_);
    // queue() will reopen a stopped device. Its old FIFO must not block that
    // recovery path before queue() is called.
    if (device_ == 0 || SDL_GetAudioDeviceStatus(device_) == SDL_AUDIO_STOPPED)
        return false;
    synchronize_locked();
    return fifo_.full();
}

std::size_t AudioOutput::current_bytes() {
    std::lock_guard lock(mutex_);
    synchronize_locked();
    return fifo_.current_frames() * 4u;
}

AudioPacingSnapshot AudioOutput::pacing_snapshot() const {
    std::lock_guard lock(mutex_);
    auto result = pacing_;
    if (device_ != 0)
        result.current_software_queue_frames = SDL_GetQueuedAudioSize(device_) / 4u;
    return result;
}

SDL_AudioDeviceID AudioOutput::id() const {
    std::lock_guard lock(mutex_);
    return device_;
}

void AudioOutput::close() {
    std::lock_guard lock(mutex_);
    if (device_ != 0) SDL_CloseAudioDevice(device_);
    device_ = 0;
    fifo_ = AudioDmaFifo{fifo_capacity_};
}

} // namespace tetrisphere
