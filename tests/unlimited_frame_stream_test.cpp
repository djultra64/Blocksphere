#include "tetrisphere/unlimited_frame_stream.h"
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <thread>

using namespace std::chrono_literals;
int main() {
    tetrisphere::UnlimitedFrameStream stream;
    std::atomic<bool> running{true};
    const auto stopped = [&] { return !running.load(); };
    auto generation = stream.begin(true, stopped);
    assert(generation != 0);
    for (unsigned frame = 0; frame < 3; ++frame) {
        assert(stream.wait_for_slot(generation, frame, std::chrono::steady_clock::now() + 1s, stopped));
        assert(stream.publish(generation));
    }
    auto blocked = std::async(std::launch::async, [&] {
        return stream.wait_for_slot(generation, 3, std::chrono::steady_clock::now() + 1s, stopped);
    });
    assert(blocked.wait_for(10ms) == std::future_status::timeout);
    assert(stream.wait_for_frame(generation, 0, stopped));
    stream.consume(generation, 0);
    assert(blocked.get());
    // A fixed-size ring must support arbitrarily many frames over its lifetime.
    for (unsigned frame = 3; frame < 10000; ++frame) {
        assert(stream.wait_for_slot(generation, frame, std::chrono::steady_clock::now() + 1s, stopped));
        assert(stream.publish(generation));
        assert(stream.wait_for_frame(generation, frame - 2, stopped));
        stream.consume(generation, frame - 2);
    }
    auto draining = std::async(std::launch::async, [&] { return stream.wait_until_drained(stopped); });
    assert(draining.wait_for(10ms) == std::future_status::timeout);
    stream.finish(generation);
    stream.consume(generation, 9999);
    assert(draining.get());
    assert(!stream.wait_for_frame(generation, 10000, stopped));
    assert(!stream.claim().active);
    assert(!stream.wait_for_frame(generation, 0, stopped));
    auto old_generation = generation;
    generation = stream.begin(true, stopped);
    assert(generation != old_generation);
    assert(!stream.wait_for_frame(old_generation, 0, stopped));
    // A workload without a VI/present event must not block the next workload.
    assert(stream.publish(generation));
    auto replacement = std::async(std::launch::async, [&] { return stream.begin(true, stopped); });
    const bool unclaimed_replaced = replacement.wait_for(100ms) == std::future_status::ready;
    if (!unclaimed_replaced) stream.cancel();
    generation = replacement.get();
    assert(unclaimed_replaced);
    assert(stream.claim().active);
    assert(!stream.claim().active);
    assert(stream.publish(generation));
    assert(stream.publish(generation));
    assert(stream.publish(generation));
    assert(!stream.wait_for_slot(generation, 3, std::chrono::steady_clock::now() + 2ms, stopped));
    auto cancelled = std::async(std::launch::async, [&] {
        return stream.wait_for_slot(generation, 3, std::chrono::steady_clock::now() + 1s, stopped);
    });
    stream.cancel();
    assert(!cancelled.get());
    generation = stream.begin(true, stopped);
    auto shutdown = std::async(std::launch::async, [&] {
        return stream.wait_for_frame(generation, 0, stopped);
    });
    running = false;
    assert(!shutdown.get());
    tetrisphere::UnlimitedFrameStream concurrent;
    running = true;
    const auto concurrent_generation = concurrent.begin(true, stopped);
    assert(concurrent.claim().active);
    std::array<std::uint64_t, 3> targets{};
    auto consumer = std::async(std::launch::async, [&] {
        for (std::uint64_t frame = 0; concurrent.wait_for_frame(concurrent_generation, frame, stopped); ++frame) {
            assert(targets[frame % targets.size()] == frame);
            if (frame % 31 == 0) std::this_thread::sleep_for(10us);
            concurrent.consume(concurrent_generation, frame);
        }
    });
    for (std::uint64_t frame = 0; frame < 5000; ++frame) {
        assert(concurrent.wait_for_slot(concurrent_generation, frame,
            std::chrono::steady_clock::now() + 1s, stopped));
        targets[frame % targets.size()] = frame;
        assert(concurrent.publish(concurrent_generation));
    }
    concurrent.finish(concurrent_generation);
    consumer.get();
    assert(tetrisphere::unlimited_frame_weight(0, 1000, 10000) == 0.1f);
    assert(tetrisphere::unlimited_frame_weight(9000, 2000, 10000) == 1.0f);
    assert(tetrisphere::unlimited_frame_weight(0, 1000, 0) == 1.0f);
}
