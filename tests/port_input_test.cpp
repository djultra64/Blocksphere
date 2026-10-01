#include "tetrisphere/port_input.h"

#include <iostream>

namespace {
int fail(const char* why) { std::cerr << why << '\n'; return 1; }
} // namespace

int main() {
    tetrisphere::ControllerBindings bindings{};
    tetrisphere::ControllerPortSnapshot p1{};
    tetrisphere::ControllerPortSnapshot p2{};
    p1.connected = true;
    p1.sample.south = true;
    p2.connected = true;
    p2.sample.east = true;
    p2.sample.left_x = -16384;
    const auto first = tetrisphere::map_port_input(0, p1, 0x1000, bindings,
                                                   tetrisphere::MagicControl::TriggerLeft);
    const auto second = tetrisphere::map_port_input(1, p2, 0x1000, bindings,
                                                    tetrisphere::MagicControl::TriggerLeft);
    if ((first.buttons & 0x9000) != 0x9000 || (second.buttons & 0x4000) == 0 ||
        (second.buttons & 0x1000) != 0 || second.x >= -0.4f)
        return fail("P1 keyboard leaked to P2 or controller mapping failed");
    bindings.confirm = tetrisphere::PhysicalButton::East;
    const auto remapped = tetrisphere::map_port_input(1, p2, 0, bindings,
                                                      tetrisphere::MagicControl::TriggerLeft);
    if ((remapped.buttons & 0x8000) == 0)
        return fail("P2 did not use logical remapping");
    p2.connected = false;
    const auto detached = tetrisphere::map_port_input(1, p2, 0xffff, bindings,
                                                      tetrisphere::MagicControl::TriggerLeft);
    if (detached.buttons != 0 || detached.x != 0.0f || detached.y != 0.0f)
        return fail("detached P2 was not neutral");
    struct Direction { std::int16_t x, y; bool up, down, left, right; };
    constexpr Direction directions[] = {
        {-20000, 0, false, false, true, false},
        {20000, 0, false, false, false, true},
        {0, -20000, true, false, false, false},
        {0, 20000, false, true, false, false},
        {-20000, -20000, true, false, true, false},
    };
    for (int port : {0, 1}) for (const auto& d : directions) {
        tetrisphere::ControllerPortSnapshot stick{}, pad{};
        stick.connected = pad.connected = true;
        stick.sample.left_x = d.x; stick.sample.left_y = d.y;
        pad.sample.dpad_up = d.up; pad.sample.dpad_down = d.down;
        pad.sample.dpad_left = d.left; pad.sample.dpad_right = d.right;
        const auto from_stick = tetrisphere::map_port_input(port, stick, 0, {}, tetrisphere::MagicControl::West);
        const auto from_pad = tetrisphere::map_port_input(port, pad, 0, {}, tetrisphere::MagicControl::West);
        if (from_stick.buttons != from_pad.buttons || from_stick.x != from_pad.x ||
            from_stick.y != from_pad.y || (from_stick.x == 0 && from_stick.y == 0))
            return fail("left stick and D-pad did not produce identical game directions");
    }
    p1.sample = {};
    p1.sample.left_x = 4000; p1.sample.left_y = -4000;
    const auto drift = tetrisphere::map_port_input(0, p1, 0, {}, tetrisphere::MagicControl::West);
    if (drift.buttons || drift.x != 0 || drift.y != 0)
        return fail("stick drift moved the game");
    p1.sample = {};
    p1.sample.west = true;
    const auto magic = tetrisphere::map_port_input(0, p1, 0, {}, tetrisphere::MagicControl::West);
    if (magic.buttons != 0x0004u)
        return fail("west Magic also activated a spare C direction");
    return 0;
}
