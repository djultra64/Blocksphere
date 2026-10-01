#include "tetrisphere/controller_ports.h"

#include <SDL.h>

#include <cstdlib>
#include <iostream>

namespace {
int fail(const char* why) { std::cerr << why << '\n'; return 1; }

int attach() {
    return SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 15, 2);
}

void drain(tetrisphere::ControllerPorts& ports) {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) ports.handle_event(event);
}

void detach_id(SDL_JoystickID id) {
    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
        if (SDL_JoystickGetDeviceInstanceID(index) == id) {
            SDL_JoystickDetachVirtual(index);
            return;
        }
    }
}
} // namespace

int main(int, char**) {
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0)
        return fail("SDL initialization failed");
    const int first = attach();
    const int second = attach();
    if (first < 0 || second < 0) return fail("virtual controller attachment failed");
    const auto first_id = SDL_JoystickGetDeviceInstanceID(first);
    const auto second_id = SDL_JoystickGetDeviceInstanceID(second);
    tetrisphere::ControllerPorts ports;
    ports.open_all();
    auto pair = ports.snapshot();
    if (pair[0].instance_id != first_id || pair[1].instance_id != second_id)
        return fail("initial controllers were not assigned independently");
    if (!pair[0].connected || !pair[1].connected || first_id == second_id)
        return fail("initial snapshots are inconsistent");
    SDL_Joystick* first_joystick = SDL_JoystickOpen(first);
    SDL_Joystick* second_joystick = SDL_JoystickOpen(second);
    if (first_joystick == nullptr || second_joystick == nullptr)
        return fail("could not open virtual joysticks for sampling");
    SDL_JoystickSetVirtualButton(first_joystick, SDL_CONTROLLER_BUTTON_A, 1);
    SDL_JoystickSetVirtualButton(second_joystick, SDL_CONTROLLER_BUTTON_B, 1);
    SDL_PumpEvents();
    pair = ports.snapshot();
    if (!pair[0].sample.south || pair[0].sample.east ||
        !pair[1].sample.east || pair[1].sample.south ||
        pair[0].instance_id != first_id || pair[1].instance_id != second_id)
        return fail("samples and identities were not returned per port");
    SDL_JoystickClose(first_joystick);
    SDL_JoystickClose(second_joystick);
    drain(ports); // Duplicate ADDED events must not change assignments.
    pair = ports.snapshot();
    if (pair[0].instance_id != first_id || pair[1].instance_id != second_id)
        return fail("duplicate add reassigned a controller");

    SDL_Event event{};
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.which = second_id;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_A;
    if (ports.handle_event(event) != 1) return fail("P2 activity mapped to wrong port");
    event.cbutton.which = first_id;
    if (ports.handle_event(event) != 0) return fail("P1 activity mapped to wrong port");
    event.type = SDL_CONTROLLERAXISMOTION;
    event.caxis.which = second_id;
    event.caxis.value = 100;
    if (ports.handle_event(event) != -1) return fail("axis noise counted as activity");
    event.caxis.value = 10000;
    if (ports.handle_event(event) != 1) return fail("meaningful P2 axis ignored");

    const int third = attach();
    if (third < 0) return fail("third attachment failed");
    const auto third_id = SDL_JoystickGetDeviceInstanceID(third);
    drain(ports);
    pair = ports.snapshot();
    if (pair[0].instance_id != first_id || pair[1].instance_id != second_id)
        return fail("third device disturbed occupied ports");
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.which = third_id;
    if (ports.handle_event(event) != -1) return fail("unassigned third device drove a port");

    SDL_JoystickDetachVirtual(first);
    drain(ports);
    pair = ports.snapshot();
    if (pair[1].instance_id != second_id || !pair[1].connected)
        return fail("P2 changed when P1 was removed");
    if (pair[0].instance_id == second_id)
        return fail("P2 was duplicated into P1");

    // Device-added event carries a current enumeration INDEX, not an instance ID.
    // Reattach and dispatch the actual SDL event after indices have shifted.
    const int fourth = attach();
    if (fourth < 0) return fail("fourth attachment failed");
    const auto fourth_id = SDL_JoystickGetDeviceInstanceID(fourth);
    drain(ports);
    pair = ports.snapshot();
    if (pair[1].instance_id != second_id ||
        (pair[0].instance_id != third_id && pair[0].instance_id != fourth_id))
        return fail("vacant P1 was not filled using SDL device index");

    ports.close();
    detach_id(second_id);
    detach_id(third_id);
    detach_id(fourth_id);

    // QA chooses exact virtual instance IDs even when two other controllers
    // were enumerated first. This path must not change the normal assignment.
    const int existing_a = attach();
    const int existing_b = attach();
    const int qa_a = attach();
    const int qa_b = attach();
    if (existing_a < 0 || existing_b < 0 || qa_a < 0 || qa_b < 0)
        return fail("could not attach priority test devices");
    const auto existing_a_id = SDL_JoystickGetDeviceInstanceID(existing_a);
    const auto existing_b_id = SDL_JoystickGetDeviceInstanceID(existing_b);
    const auto qa_a_id = SDL_JoystickGetDeviceInstanceID(qa_a);
    const auto qa_b_id = SDL_JoystickGetDeviceInstanceID(qa_b);
    tetrisphere::ControllerPorts qa_ports;
    qa_ports.set_preferred_ids({qa_a_id, qa_b_id});
    qa_ports.open_all();
    drain(qa_ports);
    auto qa_pair = qa_ports.snapshot();
    if (qa_pair[0].instance_id != qa_a_id || qa_pair[1].instance_id != qa_b_id)
        return fail("QA controllers did not occupy exact requested ports");
    SDL_Joystick* qa_joystick = SDL_JoystickOpen(qa_b);
    if (qa_joystick == nullptr) return fail("could not open P2 virtual joystick");
    SDL_JoystickSetVirtualButton(qa_joystick, SDL_CONTROLLER_BUTTON_A, 1);
    SDL_JoystickUpdate();
    qa_pair = qa_ports.snapshot();
    if (!qa_pair[1].sample.south || qa_pair[0].sample.south)
        return fail("P2 button appeared on wrong QA port");
    SDL_JoystickSetVirtualButton(qa_joystick, SDL_CONTROLLER_BUTTON_A, 0);
    SDL_JoystickUpdate();
    qa_pair = qa_ports.snapshot();
    if (qa_pair[1].sample.south) return fail("P2 virtual release was not sampled");
    SDL_JoystickClose(qa_joystick);
    qa_ports.close();
    detach_id(existing_a_id);
    detach_id(existing_b_id);
    detach_id(qa_a_id);
    detach_id(qa_b_id);
    SDL_Quit();
    return 0;
}
