#include "tetrisphere/keyboard_state.h"
#include "tetrisphere/linux_platform.h"

namespace tetrisphere {
namespace {

constexpr std::uint32_t south = 0x10000;
constexpr std::uint32_t east = 0x20000;
constexpr std::uint32_t west = 0x40000;
constexpr std::uint32_t north = 0x80000;
constexpr std::uint32_t trigger_left = 0x100000;

std::uint32_t bit_for(SDL_Scancode key) {
    switch (key) {
        case SDL_SCANCODE_Z: return south;
        case SDL_SCANCODE_X: return east;
        case SDL_SCANCODE_C: return 0x2000;
        case SDL_SCANCODE_RETURN: return 0x1000;
        case SDL_SCANCODE_UP: return 0x0800;
        case SDL_SCANCODE_DOWN: return 0x0400;
        case SDL_SCANCODE_LEFT: return 0x0200;
        case SDL_SCANCODE_RIGHT: return 0x0100;
        case SDL_SCANCODE_A: return west;
        case SDL_SCANCODE_S: return 0x0010;
        case SDL_SCANCODE_I: return north;
        case SDL_SCANCODE_K: return trigger_left;
        case SDL_SCANCODE_J: return 0x0020;
        case SDL_SCANCODE_L: return 0x0001;
        default: return 0;
    }
}

bool pressed(std::uint32_t keys, PhysicalButton button) {
    switch (button) {
        case PhysicalButton::South: return (keys & south) != 0;
        case PhysicalButton::East: return (keys & east) != 0;
        case PhysicalButton::West: return (keys & west) != 0;
        case PhysicalButton::North: return (keys & north) != 0;
    }
    return false;
}

bool pressed(std::uint32_t keys, MagicControl control) {
    switch (control) {
        case MagicControl::TriggerLeft: return (keys & trigger_left) != 0;
        case MagicControl::TriggerRight: return (keys & 0x0001) != 0;
        case MagicControl::LeftShoulder: return (keys & 0x0020) != 0;
        case MagicControl::RightShoulder: return (keys & 0x0010) != 0;
        case MagicControl::South: return (keys & south) != 0;
        case MagicControl::East: return (keys & east) != 0;
        case MagicControl::West: return (keys & west) != 0;
        case MagicControl::North: return (keys & north) != 0;
    }
    return false;
}

} // namespace

std::uint16_t KeyboardState::buttons() const {
    const auto keys = buttons_.load();
    const auto bindings = controller_bindings();
    auto result = static_cast<std::uint16_t>(keys);
    if (pressed(keys, bindings.confirm)) result |= 0x8000;
    if (pressed(keys, bindings.cancel)) result |= 0x4000;
    const int magic_button = static_cast<int>(magic_control()) - static_cast<int>(MagicControl::South);
    if (static_cast<int>(bindings.face_up) != magic_button && pressed(keys, bindings.face_up))
        result |= 0x0008;
    if (static_cast<int>(bindings.face_left) != magic_button && pressed(keys, bindings.face_left))
        result |= 0x0002;
    if (pressed(keys, magic_control())) result |= 0x0004;
    return result;
}

bool KeyboardState::handle_event(const SDL_Event& event) {
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        buttons_.store(0);
        return false;
    }
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) return false;
    const auto bit = bit_for(event.key.keysym.scancode);
    if (bit == 0) return false;
    if (event.type == SDL_KEYDOWN) {
        buttons_.fetch_or(bit);
        return true;
    }
    buttons_.fetch_and(~bit);
    return false;
}

} // namespace tetrisphere
