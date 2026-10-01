#pragma once

#include <SDL.h>

#include <cstdint>
#include <mutex>
#include <string>

namespace tetrisphere {

struct ControllerSample {
    bool south = false;
    bool east = false;
    bool west = false;
    bool north = false;
    bool back = false;
    bool start = false;
    bool dpad_up = false;
    bool dpad_down = false;
    bool dpad_left = false;
    bool dpad_right = false;
    bool left_shoulder = false;
    bool right_shoulder = false;
    std::int16_t left_x = 0;
    std::int16_t left_y = 0;
    std::int16_t trigger_left = 0;
    std::int16_t trigger_right = 0;
};

class ControllerSlot {
public:
    ControllerSlot() = default;
    ~ControllerSlot();
    ControllerSlot(const ControllerSlot&) = delete;
    ControllerSlot& operator=(const ControllerSlot&) = delete;

    void open_first();
    void close();
    bool handle_event(const SDL_Event& event);
    bool connected() const;
    std::string name() const;
    ControllerSample sample() const;
    void rumble(bool enabled);

private:
    void open_first_locked();
    SDL_JoystickID instance_id_locked() const;
    mutable std::mutex mutex_;
    SDL_GameController* controller_ = nullptr;
};

} // namespace tetrisphere
