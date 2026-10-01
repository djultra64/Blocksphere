#include "tetrisphere/port_input.h"

namespace tetrisphere {
namespace {
constexpr std::uint16_t n64_a = 0x8000;
constexpr std::uint16_t n64_b = 0x4000;
constexpr std::uint16_t n64_z = 0x2000;
constexpr std::uint16_t n64_start = 0x1000;
constexpr std::uint16_t n64_up = 0x0800;
constexpr std::uint16_t n64_down = 0x0400;
constexpr std::uint16_t n64_left = 0x0200;
constexpr std::uint16_t n64_right = 0x0100;
constexpr std::uint16_t n64_l = 0x0020;
constexpr std::uint16_t n64_r = 0x0010;
constexpr std::uint16_t n64_c_up = 0x0008;
constexpr std::uint16_t n64_c_down = 0x0004;
constexpr std::uint16_t n64_c_left = 0x0002;
constexpr std::uint16_t n64_c_right = 0x0001;

bool pressed(const ControllerSample& sample, PhysicalButton button) {
    switch (button) {
        case PhysicalButton::South: return sample.south;
        case PhysicalButton::East: return sample.east;
        case PhysicalButton::West: return sample.west;
        case PhysicalButton::North: return sample.north;
    }
    return false;
}

bool pressed(const ControllerSample& sample, MagicControl control) {
    switch (control) {
        case MagicControl::TriggerLeft: return sample.trigger_left > 8192;
        case MagicControl::TriggerRight: return sample.trigger_right > 8192;
        case MagicControl::LeftShoulder: return sample.left_shoulder;
        case MagicControl::RightShoulder: return sample.right_shoulder;
        case MagicControl::South: return sample.south;
        case MagicControl::East: return sample.east;
        case MagicControl::West: return sample.west;
        case MagicControl::North: return sample.north;
    }
    return false;
}
} // namespace

PortInput map_port_input(int port, const ControllerPortSnapshot& controller,
                         std::uint16_t keyboard_buttons,
                         const ControllerBindings& bindings,
                         MagicControl magic) {
    PortInput result{};
    if (port < 0 || port >= 2) return result;
    const auto keys = port == 0 ? keyboard_buttons : 0;
    result.buttons = keys;
    if (controller.connected) {
        const auto& sample = controller.sample;
        if (pressed(sample, bindings.confirm)) result.buttons |= n64_a;
        if (pressed(sample, bindings.cancel)) result.buttons |= n64_b;
        if (sample.back) result.buttons |= n64_z;
        if (sample.start) result.buttons |= n64_start;
        // Both host inputs feed the same game directions, including digital
        // navigation and N64 stick movement. Small stick drift remains neutral.
        constexpr int deadzone = 8192;
        const bool left = sample.dpad_left || sample.left_x < -deadzone;
        const bool right = sample.dpad_right || sample.left_x > deadzone;
        const bool up = sample.dpad_up || sample.left_y < -deadzone;
        const bool down = sample.dpad_down || sample.left_y > deadzone;
        result.x = static_cast<float>(int(right) - int(left));
        result.y = static_cast<float>(int(up) - int(down));
        if (result.x < 0) result.buttons |= n64_left;
        if (result.x > 0) result.buttons |= n64_right;
        if (result.y > 0) result.buttons |= n64_up;
        if (result.y < 0) result.buttons |= n64_down;
        if (sample.left_shoulder) result.buttons |= n64_l;
        if (sample.right_shoulder) result.buttons |= n64_r;
        // A face button assigned to Magic must not also emit a spare C input.
        const int magic_button = static_cast<int>(magic) - static_cast<int>(MagicControl::South);
        if (static_cast<int>(bindings.face_up) != magic_button && pressed(sample, bindings.face_up))
            result.buttons |= n64_c_up;
        if (static_cast<int>(bindings.face_left) != magic_button && pressed(sample, bindings.face_left))
            result.buttons |= n64_c_left;
        if (sample.trigger_right > 8192) result.buttons |= n64_c_right;
        if (pressed(sample, magic)) result.buttons |= n64_c_down;
    }
    if (keys & n64_left) result.x = -1.0f;
    if (keys & n64_right) result.x = 1.0f;
    if (keys & n64_up) result.y = 1.0f;
    if (keys & n64_down) result.y = -1.0f;
    return result;
}

} // namespace tetrisphere
