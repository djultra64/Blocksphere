#include "tetrisphere/controller_ports.h"

#include <cstdlib>

namespace tetrisphere {
namespace {
ControllerSample read_sample(SDL_GameController* controller) {
    ControllerSample result{};
    const auto button = [controller](SDL_GameControllerButton id) {
        return SDL_GameControllerGetButton(controller, id) != 0;
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
    result.left_x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    result.left_y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
    result.trigger_left = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    result.trigger_right = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    return result;
}
} // namespace

ControllerPorts::~ControllerPorts() { close(); }

SDL_JoystickID ControllerPorts::id_locked(int port) const {
    const auto* controller = controllers_[port];
    if (controller == nullptr) return -1;
    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controllers_[port]);
    return joystick == nullptr ? -1 : SDL_JoystickInstanceID(joystick);
}

int ControllerPorts::port_for_id_locked(SDL_JoystickID id) const {
    if (id < 0) return -1;
    for (int port = 0; port < 2; ++port)
        if (id_locked(port) == id) return port;
    return -1;
}

bool ControllerPorts::open_index_locked(int index) {
    if (index < 0 || !SDL_IsGameController(index)) return false;
    const auto id = SDL_JoystickGetDeviceInstanceID(index);
    if (id < 0 || port_for_id_locked(id) >= 0) return false;
    int free_port = -1;
    if (preferred_ids_[0] >= 0) {
        for (int port = 0; port < 2; ++port)
            if (preferred_ids_[port] == id && controllers_[port] == nullptr)
                free_port = port;
    } else {
        for (int port = 0; port < 2; ++port) {
            if (controllers_[port] == nullptr) { free_port = port; break; }
        }
    }
    if (free_port < 0) return false;
    SDL_GameController* controller = SDL_GameControllerOpen(index);
    if (controller == nullptr || !SDL_GameControllerGetAttached(controller)) {
        if (controller != nullptr) SDL_GameControllerClose(controller);
        return false;
    }
    // The enumeration can change between the index lookup and open.
    const auto opened_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
    if (opened_id < 0 || opened_id != id || port_for_id_locked(opened_id) >= 0) {
        SDL_GameControllerClose(controller);
        return false;
    }
    controllers_[free_port] = controller;
    return true;
}

bool ControllerPorts::set_preferred_ids(std::array<SDL_JoystickID, 2> ids) {
    std::lock_guard lock(mutex_);
    if (ids[0] < 0 || ids[1] < 0 || ids[0] == ids[1] ||
        controllers_[0] != nullptr || controllers_[1] != nullptr) return false;
    preferred_ids_ = ids;
    return true;
}

void ControllerPorts::fill_empty_locked() {
    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
        if (controllers_[0] != nullptr && controllers_[1] != nullptr) break;
        open_index_locked(index);
    }
}

void ControllerPorts::open_all() {
    std::lock_guard lock(mutex_);
    fill_empty_locked();
}

void ControllerPorts::close() {
    std::lock_guard lock(mutex_);
    for (auto& controller : controllers_) {
        if (controller != nullptr) SDL_GameControllerClose(controller);
        controller = nullptr;
    }
}

int ControllerPorts::handle_event(const SDL_Event& event) {
    std::lock_guard lock(mutex_);
    if (event.type == SDL_CONTROLLERDEVICEADDED) {
        // ADDED.which is a device enumeration index, unlike all other events.
        open_index_locked(event.cdevice.which);
        return -1;
    }
    if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
        const int port = port_for_id_locked(event.cdevice.which);
        if (port >= 0) {
            SDL_GameControllerClose(controllers_[port]);
            controllers_[port] = nullptr;
            // Fill only vacant ports; an occupied P2 never migrates into P1.
            fill_empty_locked();
        }
        return -1;
    }
    if (event.type == SDL_CONTROLLERBUTTONDOWN)
        return port_for_id_locked(event.cbutton.which);
    if (event.type == SDL_CONTROLLERAXISMOTION &&
        std::abs(static_cast<int>(event.caxis.value)) > 8192)
        return port_for_id_locked(event.caxis.which);
    return -1;
}

std::array<ControllerPortSnapshot, 2> ControllerPorts::snapshot() const {
    std::lock_guard lock(mutex_);
    std::array<ControllerPortSnapshot, 2> result{};
    for (int port = 0; port < 2; ++port) {
        auto* controller = controllers_[port];
        if (controller == nullptr || !SDL_GameControllerGetAttached(controller)) continue;
        auto& entry = result[port];
        entry.connected = true;
        entry.instance_id = id_locked(port);
        const char* name = SDL_GameControllerName(controller);
        if (name != nullptr) entry.name = name;
        entry.sample = read_sample(controller);
    }
    return result;
}

void ControllerPorts::rumble(int port, bool enabled) {
    if (port < 0 || port >= 2) return;
    std::lock_guard lock(mutex_);
    auto* controller = controllers_[port];
    if (controller != nullptr && SDL_GameControllerGetAttached(controller)) {
        SDL_GameControllerRumble(controller, enabled ? 0xffff : 0,
                                 enabled ? 0xffff : 0, enabled ? 250 : 0);
    }
}

} // namespace tetrisphere
