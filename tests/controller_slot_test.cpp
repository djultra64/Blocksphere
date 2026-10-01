#include "tetrisphere/controller_slot.h"

#include <SDL.h>

#include <iostream>

namespace {
int fail(const char* message) { std::cerr << message << '\n'; return 1; }
}

int main(int, char**) {
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        return fail("SDL virtual controller initialization failed");
    }
    const int first = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 15, 1);
    if (first < 0) return fail("could not attach SDL virtual controller");
    tetrisphere::ControllerSlot slot;
    slot.open_first();
    if (!slot.connected()) return fail("initial controller was not opened");
    SDL_Joystick* joystick = SDL_JoystickOpen(first);
    SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_A, 1);
    SDL_PumpEvents();
    if (!slot.sample().south) return fail("south button was not sampled");
    SDL_JoystickClose(joystick);
    SDL_JoystickDetachVirtual(first);
    SDL_Event event{};
    while (SDL_PollEvent(&event)) slot.handle_event(event);
    if (slot.connected()) return fail("removed controller remained connected");
    const int second = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 15, 1);
    if (second < 0) return fail("could not reattach SDL virtual controller");
    while (SDL_PollEvent(&event)) slot.handle_event(event);
    if (!slot.connected()) return fail("reconnected controller was not opened");
    SDL_JoystickDetachVirtual(second);
    while (SDL_PollEvent(&event)) slot.handle_event(event);
    SDL_Quit();
    return 0;
}
