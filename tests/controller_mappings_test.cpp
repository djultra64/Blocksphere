#ifdef _WIN32
#define SDL_MAIN_HANDLED
#endif

#include "tetrisphere/controller_mappings.h"
#include "tetrisphere/controller_ports.h"
#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
#ifdef _WIN32
    SDL_SetMainReady();
#endif
    if (argc < 2 || argc > 3) return 2;
    const bool explicit_override = argc == 3;
    auto root = std::filesystem::u8path(argv[1]);
    std::filesystem::create_directories(root);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) return 3;
    // A stable USB-style identity avoids SDL 2.26's name-only virtual GUID
    // normalization mismatch when adding a community mapping on Windows.
    auto attach = [] {
        SDL_VirtualJoystickDesc desc{};
        desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
        desc.type = SDL_JOYSTICK_TYPE_UNKNOWN;
        desc.nbuttons = 15;
        desc.nhats = 1;
        desc.vendor_id = 0xF00D;
        desc.product_id = 0x0001;
        desc.name = "Arcade mapping regression";
        return SDL_JoystickAttachVirtualEx(&desc);
    };
    int index = attach();
    if (index < 0 || SDL_IsGameController(index)) return 4;
    char guid[33];
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof guid);
#ifdef _WIN32
    const char* platform = "Windows";
#else
    const char* platform = "Linux";
#endif
    auto bundled = root / "gamecontrollerdb.txt";
    auto local = root / "personal.txt";
    auto mapping = [&](int confirm) {
        return std::string(guid) + ",Arcade regression,a:b" + std::to_string(confirm) +
            ",b:b2,x:b0,y:b3,start:b9,lefttrigger:b14,righttrigger:b7,"
            "dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,platform:" + platform + ",\n";
    };
    { std::ofstream out(bundled); out << mapping(1); }
    { std::ofstream out(local); out << mapping(4); }
    const auto original = mapping(4);
    SDL_JoystickDetachVirtual(index);
    SDL_Quit();
    auto receipts = tetrisphere::load_controller_mappings(bundled, local);
    if (receipts.size() != 2 || receipts[0].status != "loaded" || receipts[1].status != "loaded") return 5;
    if (explicit_override) SDL_SetHint(SDL_HINT_GAMECONTROLLERCONFIG, mapping(5).c_str());
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) return 6;
    int a = attach();
    int b = attach();
    if (a < 0 || b < 0 || !SDL_IsGameController(a) || !SDL_IsGameController(b)) {
        char attached_guid[33];
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(a), attached_guid, sizeof attached_guid);
        std::cerr << "Mapping fixture recognition failed: initial GUID=" << guid
                  << ", attached GUID=" << attached_guid << ", indices=" << a << ',' << b
                  << ", mappings added=" << receipts[0].added << ',' << receipts[1].added
                  << ", SDL error=" << SDL_GetError() << '\n';
        return 7;
    }
    tetrisphere::ControllerPorts ports; ports.open_all();
    auto* first = SDL_JoystickOpen(a); auto* second = SDL_JoystickOpen(b);
    SDL_JoystickSetVirtualButton(first, explicit_override ? 5 : 4, 1);
    SDL_JoystickSetVirtualButton(first, 9, 1);
    SDL_JoystickSetVirtualHat(first, 0, SDL_HAT_RIGHTUP);
    SDL_JoystickSetVirtualButton(first, 14, 1);
    SDL_JoystickSetVirtualButton(first, 7, 1);
    SDL_JoystickSetVirtualButton(second, 2, 1);
    SDL_JoystickUpdate();
    auto pair = ports.snapshot();
    if (!pair[0].sample.south || !pair[0].sample.start || !pair[0].sample.dpad_up ||
        !pair[0].sample.dpad_right || pair[0].sample.trigger_left != 32767 ||
        pair[0].sample.trigger_right != 32767 || pair[1].sample.south || !pair[1].sample.east) return 8;
    SDL_Event event{}; while (SDL_PollEvent(&event)) ports.handle_event(event);
    auto again = ports.snapshot();
    if (again[0].instance_id != pair[0].instance_id || again[1].instance_id != pair[1].instance_id) return 9;
    ports.close(); SDL_JoystickClose(first); SDL_JoystickClose(second);
    SDL_JoystickDetachVirtual(b); SDL_JoystickDetachVirtual(a); SDL_Quit();
    std::ifstream in(local); std::string bytes((std::istreambuf_iterator<char>(in)), {});
    if (bytes != original) return 10;
    auto optional = tetrisphere::load_controller_mappings(bundled, root / "missing.txt");
    if (optional[1].status != "missing") return 11;
    { std::ofstream out(local); out << "malformed\n"; }
    if (tetrisphere::load_controller_mappings(bundled, local)[1].status != "loaded") return 12;
    { std::ofstream out(local, std::ios::binary); out.seekp(4*1024*1024); out.put('x'); }
    if (tetrisphere::load_controller_mappings(bundled, local)[1].status != "error") return 13;
    SDL_Quit();
    std::cout << "PASS: mapping recognition, local precedence, raw inputs, independent ports, preserved bytes and optional failures\n";
    return 0;
}
