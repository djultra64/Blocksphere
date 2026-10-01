#include "tetrisphere/keyboard_state.h"
#include "tetrisphere/linux_platform.h"

#include <SDL.h>

#include <iostream>
#include <string>

namespace {
int fail(const char* message) { std::cerr << message << '\n'; return 1; }
}

int main() {
    tetrisphere::KeyboardState state;
    SDL_Event down{};
    down.type = SDL_KEYDOWN;
    down.key.keysym.scancode = SDL_SCANCODE_Z;
    if (!state.handle_event(down) || (state.buttons() & 0x8000u) == 0) {
        return fail("Z did not activate logical Confirm");
    }
    down.key.keysym.scancode = SDL_SCANCODE_RETURN;
    state.handle_event(down);
    if ((state.buttons() & 0x1000u) == 0) return fail("Enter did not activate Start");
    SDL_Event focus{};
    focus.type = SDL_WINDOWEVENT;
    focus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    state.handle_event(focus);
    if (state.buttons() != 0) return fail("focus loss left keys pressed");

    down.key.keysym.scancode = SDL_SCANCODE_A;
    state.handle_event(down);
    if (state.buttons() != 0x0004u) return fail("default A did not activate Magic alone");
    if (std::string(tetrisphere::magic_control_label(tetrisphere::ControllerFamily::Keyboard,
                                                   tetrisphere::magic_control())) != "A")
        return fail("keyboard Magic prompt does not match A");
    state.handle_event(focus);
    down.key.keysym.scancode = SDL_SCANCODE_J;
    state.handle_event(down);
    if (state.buttons() != 0x0020u) return fail("J did not retain the displaced L shoulder");
    state.handle_event(focus);
    tetrisphere::set_magic_control(tetrisphere::MagicControl::TriggerLeft);
    tetrisphere::ControllerBindings remapped{};
    remapped.confirm = tetrisphere::PhysicalButton::East;
    remapped.cancel = tetrisphere::PhysicalButton::South;
    remapped.face_left = tetrisphere::PhysicalButton::North;
    remapped.face_up = tetrisphere::PhysicalButton::West;
    tetrisphere::set_controller_bindings(remapped);

    struct Case { SDL_Scancode key; std::uint16_t expected; const char* failure; };
    constexpr Case cases[] = {
        {SDL_SCANCODE_Z, 0x4000u, "Z did not follow remapped Cancel"},
        {SDL_SCANCODE_X, 0x8000u, "X did not follow remapped Confirm"},
        {SDL_SCANCODE_A, 0x0008u, "A did not follow remapped FaceUp"},
        {SDL_SCANCODE_I, 0x0002u, "I did not follow remapped FaceLeft"},
        {SDL_SCANCODE_RETURN, 0x1000u, "Enter did not retain Start"},
        {SDL_SCANCODE_UP, 0x0800u, "Up did not retain D-pad up"},
        {SDL_SCANCODE_C, 0x2000u, "C did not retain Z trigger"},
        {SDL_SCANCODE_J, 0x0020u, "J did not retain L shoulder"},
    };
    for (const auto& test : cases) {
        down.key.keysym.scancode = test.key;
        state.handle_event(down);
        if (state.buttons() != test.expected) return fail(test.failure);
        SDL_Event up = down;
        up.type = SDL_KEYUP;
        state.handle_event(up);
        if (state.buttons() != 0) return fail("key release left buttons pressed");
    }
    tetrisphere::set_magic_control(tetrisphere::MagicControl::East);
    down.key.keysym.scancode = SDL_SCANCODE_X;
    state.handle_event(down);
    if (state.buttons() != (0x8000u | 0x0004u)) {
        return fail("Magic remapped to East did not follow X and Confirm");
    }
    SDL_Event up = down;
    up.type = SDL_KEYUP;
    state.handle_event(up);
    down.key.keysym.scancode = SDL_SCANCODE_K;
    state.handle_event(down);
    if (state.buttons() != 0) return fail("K kept Magic after remap");
    up.key.keysym.scancode = SDL_SCANCODE_K;
    state.handle_event(up);
    tetrisphere::set_magic_control(tetrisphere::MagicControl::TriggerRight);
    down.key.keysym.scancode = SDL_SCANCODE_L;
    state.handle_event(down);
    if (state.buttons() != (0x0001u | 0x0004u)) {
        return fail("Magic remapped to right trigger did not follow L");
    }
    up.key.keysym.scancode = SDL_SCANCODE_L;
    state.handle_event(up);
    tetrisphere::set_magic_control(tetrisphere::MagicControl::TriggerLeft);
    tetrisphere::set_controller_bindings({});
    return 0;
}
