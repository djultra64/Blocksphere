#pragma once

#include "tetrisphere/controller_ports.h"
#include "tetrisphere/linux_platform.h"

#include <cstdint>

namespace tetrisphere {

struct PortInput {
    std::uint16_t buttons = 0;
    float x = 0.0f;
    float y = 0.0f;
};

// Keyboard input belongs to P1. Each controller is mapped with the same
// logical bindings regardless of the family printed on its physical buttons.
PortInput map_port_input(int port, const ControllerPortSnapshot& controller,
                         std::uint16_t keyboard_buttons,
                         const ControllerBindings& bindings,
                         MagicControl magic);

} // namespace tetrisphere
