#pragma once

#include "tetrisphere/controller_slot.h"

#include <SDL.h>

#include <array>
#include <mutex>
#include <string>

namespace tetrisphere {

// One immutable view of a port's identity and input from the same lock hold.
struct ControllerPortSnapshot {
    bool connected = false;
    SDL_JoystickID instance_id = -1;
    std::string name;
    ControllerSample sample{};
};

class ControllerPorts {
public:
    ControllerPorts() = default;
    ~ControllerPorts();
    ControllerPorts(const ControllerPorts&) = delete;
    ControllerPorts& operator=(const ControllerPorts&) = delete;

    // QA only: call before open_all(). Physical devices remain unassigned when
    // exact instance IDs are selected. No effect unless explicitly called.
    bool set_preferred_ids(std::array<SDL_JoystickID, 2> ids);
    void open_all();
    void close();
    // Returns the active port (0 or 1), or -1 for connection changes,
    // unassigned controllers, and axis noise.
    int handle_event(const SDL_Event& event);
    std::array<ControllerPortSnapshot, 2> snapshot() const;
    void rumble(int port, bool enabled);

private:
    bool open_index_locked(int index);
    void fill_empty_locked();
    int port_for_id_locked(SDL_JoystickID id) const;
    SDL_JoystickID id_locked(int port) const;
    mutable std::mutex mutex_;
    std::array<SDL_GameController*, 2> controllers_{};
    std::array<SDL_JoystickID, 2> preferred_ids_{{-1, -1}};
};

} // namespace tetrisphere
