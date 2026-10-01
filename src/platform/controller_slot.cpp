#include "tetrisphere/controller_slot.h"

#include <cstdlib>

namespace tetrisphere {

ControllerSlot::~ControllerSlot() {
    close();
}

void ControllerSlot::close() {
    std::lock_guard lock(mutex_);
    if (controller_ != nullptr) SDL_GameControllerClose(controller_);
    controller_ = nullptr;
}

void ControllerSlot::open_first_locked() {
    if (controller_ != nullptr) return;
    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
        if (SDL_IsGameController(index)) {
            controller_ = SDL_GameControllerOpen(index);
            if (controller_ != nullptr) return;
        }
    }
}

void ControllerSlot::open_first() {
    std::lock_guard lock(mutex_);
    open_first_locked();
}

SDL_JoystickID ControllerSlot::instance_id_locked() const {
    if (controller_ == nullptr) return -1;
    return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_));
}

bool ControllerSlot::handle_event(const SDL_Event& event) {
    std::lock_guard lock(mutex_);
    if (event.type == SDL_CONTROLLERDEVICEADDED) {
        open_first_locked();
        return false;
    }
    if (event.type == SDL_CONTROLLERDEVICEREMOVED &&
        event.cdevice.which == instance_id_locked()) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
        open_first_locked();
        return false;
    }
    if (event.type == SDL_CONTROLLERBUTTONDOWN &&
        event.cbutton.which == instance_id_locked()) return true;
    if (event.type == SDL_CONTROLLERAXISMOTION &&
        event.caxis.which == instance_id_locked() &&
        std::abs(static_cast<int>(event.caxis.value)) > 8192) return true;
    return false;
}

bool ControllerSlot::connected() const {
    std::lock_guard lock(mutex_);
    return controller_ != nullptr && SDL_GameControllerGetAttached(controller_);
}

std::string ControllerSlot::name() const {
    std::lock_guard lock(mutex_);
    if (controller_ == nullptr || !SDL_GameControllerGetAttached(controller_)) return {};
    const char* value = SDL_GameControllerName(controller_);
    return value == nullptr ? "" : value;
}

ControllerSample ControllerSlot::sample() const {
    std::lock_guard lock(mutex_);
    ControllerSample result{};
    if (controller_ == nullptr || !SDL_GameControllerGetAttached(controller_)) return result;
    const auto button = [this](SDL_GameControllerButton id) {
        return SDL_GameControllerGetButton(controller_, id) != 0;
    };
    result.south = button(SDL_CONTROLLER_BUTTON_A);
    result.east = button(SDL_CONTROLLER_BUTTON_B);
    result.west = button(SDL_CONTROLLER_BUTTON_X);
    result.north = button(SDL_CONTROLLER_BUTTON_Y);
    result.back = button(SDL_CONTROLLER_BUTTON_BACK);
    result.start = button(SDL_CONTROLLER_BUTTON_START);
    result.dpad_up = button(SDL_CONTROLLER_BUTTON_DPAD_UP);
    result.dpad_down = button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    result.dpad_left = button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    result.dpad_right = button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    result.left_shoulder = button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    result.right_shoulder = button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    result.left_x = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTX);
    result.left_y = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTY);
    result.trigger_left = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    result.trigger_right = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    return result;
}

void ControllerSlot::rumble(bool enabled) {
    std::lock_guard lock(mutex_);
    if (controller_ != nullptr && SDL_GameControllerGetAttached(controller_)) {
        SDL_GameControllerRumble(controller_, enabled ? 0xffff : 0,
                                enabled ? 0xffff : 0, enabled ? 250 : 0);
    }
}

} // namespace tetrisphere
