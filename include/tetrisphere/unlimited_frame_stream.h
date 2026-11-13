#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

extern "C" bool tetrisphere_rt64_unlimited_frame_rate();

namespace tetrisphere {

// Three reusable GPU targets bound memory independently of the number of frames
// rendered during a native game interval. Sequence numbers include its first
// (non-MSAA) frame, which still uses RT64's original framebuffer.
class UnlimitedFrameStream {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::uint64_t capacity = 3;

    struct Snapshot {
        std::uint64_t generation;
        bool active;
    };

    template<class Stopped>
    std::uint64_t begin(bool active, Stopped stopped) {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!cancelled_ && consumer_claimed_ && consumed_ < produced_ && !stopped()) {
            changed_.wait_for(lock, std::chrono::milliseconds(1));
        }
        if (stopped()) return 0;
        ++generation_;
        active_ = active;
        finished_ = !active;
        cancelled_ = false;
        consumer_claimed_ = false;
        produced_ = consumed_ = 0;
        changed_.notify_all();
        return generation_;
    }

    template<class Stopped>
    bool wait_until_drained(Stopped stopped) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!consumer_claimed_) {
            // No VI consumed this workload. Retire it before targets disappear.
            cancelled_ = true;
            finished_ = true;
            consumed_ = produced_;
            changed_.notify_all();
        }
        while (!cancelled_ && consumed_ < produced_ && !stopped()) {
            changed_.wait_for(lock, std::chrono::milliseconds(1));
        }
        return !stopped();
    }

    Snapshot claim() {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool active = active_ && !cancelled_ && !consumer_claimed_ &&
                            (!finished_ || consumed_ < produced_);
        consumer_claimed_ = consumer_claimed_ || active;
        return {generation_, active};
    }

    template<class Stopped>
    bool wait_for_slot(std::uint64_t generation, std::uint64_t frame,
                       Clock::time_point deadline, Stopped stopped) {
        std::unique_lock<std::mutex> lock(mutex_);
        while (valid(generation) && frame >= consumed_ + capacity && !stopped()) {
            if (Clock::now() >= deadline) return false;
            changed_.wait_until(lock, std::min(deadline, Clock::now() + std::chrono::milliseconds(1)));
        }
        return valid(generation) && !stopped() && Clock::now() < deadline;
    }

    bool publish(std::uint64_t generation) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!valid(generation)) return false;
        ++produced_;
        changed_.notify_all();
        return true;
    }

    template<class Stopped>
    bool wait_for_frame(std::uint64_t generation, std::uint64_t frame, Stopped stopped) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (valid(generation)) consumer_claimed_ = true;
        while (valid(generation) && produced_ <= frame && !finished_ && !stopped()) {
            changed_.wait_for(lock, std::chrono::milliseconds(1));
        }
        return valid(generation) && frame >= consumed_ && produced_ > frame && !stopped();
    }

    void consume(std::uint64_t generation, std::uint64_t frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!valid(generation)) return;
        consumed_ = std::min(produced_, std::max(consumed_, frame + 1));
        changed_.notify_all();
    }

    void finish(std::uint64_t generation) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (generation != generation_) return;
        finished_ = true;
        changed_.notify_all();
    }

    void cancel() {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled_ = true;
        finished_ = true;
        consumed_ = produced_;
        changed_.notify_all();
    }

private:
    bool valid(std::uint64_t generation) const {
        return active_ && !cancelled_ && generation != 0 && generation == generation_;
    }
    std::mutex mutex_;
    std::condition_variable changed_;
    std::uint64_t generation_ = 0;
    std::uint64_t produced_ = 0;
    std::uint64_t consumed_ = 0;
    bool active_ = false;
    bool finished_ = true;
    bool cancelled_ = false;
    bool consumer_claimed_ = false;
};

// The counter address identifies RT64's two independently consumed epochs.
// Shared ownership keeps a consumer epoch stable during queue shutdown.
inline std::shared_ptr<UnlimitedFrameStream> unlimited_frame_stream(const void* counter) {
    static std::mutex mutex;
    static std::map<const void*, std::shared_ptr<UnlimitedFrameStream>> streams;
    std::lock_guard<std::mutex> lock(mutex);
    auto& stream = streams[counter];
    if (!stream) stream = std::make_shared<UnlimitedFrameStream>();
    return stream;
}

inline float unlimited_frame_weight(std::int64_t elapsed_microseconds,
                                    std::int64_t estimated_render_microseconds,
                                    std::int64_t native_interval_microseconds) {
    if (native_interval_microseconds <= 0) return 1.0f;
    return std::clamp(float(elapsed_microseconds + estimated_render_microseconds) /
                      float(native_interval_microseconds), 0.0f, 1.0f);
}

} // namespace tetrisphere
