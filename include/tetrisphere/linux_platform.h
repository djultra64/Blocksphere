#pragma once

#include <cstdint>
#include <cstddef>
#include <deque>

namespace tetrisphere {

struct RunOptions;

enum class ControllerFamily { Keyboard, Xbox, PlayStation, NintendoSwitch };
enum class LogicalAction { Confirm, Cancel, FaceLeft, FaceUp };
enum class PhysicalButton { South, East, West, North };
enum class MagicControl {
    TriggerLeft, TriggerRight, LeftShoulder, RightShoulder,
    South, East, West, North
};

struct ControllerBindings {
    PhysicalButton confirm = PhysicalButton::South;
    PhysicalButton cancel = PhysicalButton::East;
    PhysicalButton face_left = PhysicalButton::West;
    PhysicalButton face_up = PhysicalButton::North;
};

ControllerFamily detect_controller_family(const char* name);
ControllerFamily active_controller_family();
ControllerFamily active_controller_family_for_port(int port);
ControllerFamily select_controller_family(const char* detected_name,
                                          const char* manual_override);
const char* controller_family_name(ControllerFamily family);
PhysicalButton default_physical_button(LogicalAction action);
PhysicalButton physical_button_for(const ControllerBindings& bindings,
                                   LogicalAction action);
void set_controller_bindings(const ControllerBindings& bindings);
ControllerBindings controller_bindings();
void set_magic_control(MagicControl control);
MagicControl magic_control();
const char* magic_control_label(ControllerFamily family, MagicControl control);
const char* physical_button_label(ControllerFamily family,
                                  PhysicalButton button);
const char* logical_action_label(ControllerFamily family,
                                 const ControllerBindings& bindings,
                                 LogicalAction action);

class AudioDmaFifo {
public:
    explicit AudioDmaFifo(std::size_t capacity = 2) : capacity_(capacity) {}
    void synchronize(std::size_t total_queued_frames) {
        std::size_t tracked = 0;
        for (const auto frames : buffers_) tracked += frames;
        if (total_queued_frames >= tracked) return;
        auto consumed = tracked - total_queued_frames;
        while (!buffers_.empty() && consumed >= buffers_.front()) {
            consumed -= buffers_.front();
            buffers_.pop_front();
        }
        if (!buffers_.empty()) buffers_.front() -= consumed;
    }

    bool submit(std::size_t frames) {
        if (frames == 0 || buffers_.size() >= capacity_) return false;
        buffers_.push_back(frames);
        return true;
    }

    bool full() const { return buffers_.size() >= capacity_; }
    std::size_t current_frames() const {
        return buffers_.empty() ? 0 : buffers_.front();
    }

private:
    std::size_t capacity_;
    std::deque<std::size_t> buffers_;
};

bool linux_audio_fifo_full();
std::size_t linux_audio_current_bytes();
unsigned linux_audio_drain_wait_budget_ms();

void initialize_linux_platform(std::uint8_t* rdram, const char* rom_sha256,
                               const RunOptions& run_options);
[[noreturn]] void run_linux_event_loop();

} // namespace tetrisphere
