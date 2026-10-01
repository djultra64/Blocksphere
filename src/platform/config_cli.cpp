#include "tetrisphere/config_gui.h"
#include "tetrisphere/settings_store.h"
#include "tetrisphere/data_directory.h"

#include <SDL.h>
#include <json.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* button_name(tetrisphere::PhysicalButton button) {
    switch (button) {
        case tetrisphere::PhysicalButton::South: return "south";
        case tetrisphere::PhysicalButton::East: return "east";
        case tetrisphere::PhysicalButton::West: return "west";
        case tetrisphere::PhysicalButton::North: return "north";
    }
    return "invalid";
}

bool parse_button(const std::string& text, tetrisphere::PhysicalButton& button) {
    if (text == "south") button = tetrisphere::PhysicalButton::South;
    else if (text == "east") button = tetrisphere::PhysicalButton::East;
    else if (text == "west") button = tetrisphere::PhysicalButton::West;
    else if (text == "north") button = tetrisphere::PhysicalButton::North;
    else return false;
    return true;
}

const char* magic_name(tetrisphere::MagicControl control) {
    switch (control) {
        case tetrisphere::MagicControl::TriggerLeft: return "trigger-left";
        case tetrisphere::MagicControl::TriggerRight: return "trigger-right";
        case tetrisphere::MagicControl::LeftShoulder: return "left-shoulder";
        case tetrisphere::MagicControl::RightShoulder: return "right-shoulder";
        case tetrisphere::MagicControl::South: return "south";
        case tetrisphere::MagicControl::East: return "east";
        case tetrisphere::MagicControl::West: return "west";
        case tetrisphere::MagicControl::North: return "north";
    }
    return "invalid";
}

bool parse_magic(const std::string& text, tetrisphere::MagicControl& control) {
    for (int value = 0; value <= 7; ++value) {
        const auto candidate = static_cast<tetrisphere::MagicControl>(value);
        if (text == magic_name(candidate)) {
            control = candidate;
            return true;
        }
    }
    return false;
}

tetrisphere::PhysicalButton* binding_for(tetrisphere::Settings& settings,
                                         const std::string& action) {
    if (action == "confirm") return &settings.bindings.confirm;
    if (action == "cancel") return &settings.bindings.cancel;
    if (action == "face-left") return &settings.bindings.face_left;
    if (action == "face-up") return &settings.bindings.face_up;
    return nullptr;
}

int usage() {
    std::cerr << "usage: tetrisphere-config [--data-dir PATH] show | family "
                 "auto|keyboard|xbox|playstation|nintendo-switch | bind "
                 "confirm|cancel|face-left|face-up south|east|west|north | bind "
                 "magic trigger-left|trigger-right|left-shoulder|right-shoulder|"
                 "south|east|west|north | graphics resolution "
                 "auto|240p|720p|1080p|1440p|2160p | graphics aspect 4:3|16:9 | "
                 "graphics refresh original|30|60|120 | "
                 "graphics window-mode windowed|fullscreen | graphics msaa off|2x|4x|8x |\n"
                 "graphics presentation-filter nearest|linear|pixel | graphics texture-filter linear|three-point\n"
                 "Graphics values are requests; RT64 may cap FPS, and VS 16:9 "
                 "gameplay is not yet verified.\n";
    return 64;
}

int show_desktop_dialog(const std::string& title, const std::string& message,
                        const std::vector<std::string>& labels) {
    std::vector<SDL_MessageBoxButtonData> buttons;
    buttons.reserve(labels.size());
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const unsigned flags = index + 1 == labels.size()
            ? SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT : 0;
        buttons.push_back({flags, static_cast<int>(index), labels[index].c_str()});
    }
    SDL_MessageBoxData box{};
    box.flags = SDL_MESSAGEBOX_INFORMATION | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;
    box.title = title.c_str();
    box.message = message.c_str();
    box.numbuttons = static_cast<int>(buttons.size());
    box.buttons = buttons.data();
    int selected = -1;
    if (SDL_ShowMessageBox(&box, &selected) != 0) {
        std::cerr << "cannot show settings window: " << SDL_GetError() << '\n';
        return -2;
    }
    return selected;
}

} // namespace

int main(int argc, char** argv) {
    int first = 1;
    std::filesystem::path data_dir;
    if (argc >= 4 && std::string(argv[1]) == "--data-dir") {
        data_dir = std::filesystem::u8path(argv[2]);
        first = 3;
    } else {
        data_dir = tetrisphere::application_data_directory();
        if (data_dir.empty()) {
            std::cerr << "cannot determine game data directory\n";
            return 74;
        }
    }
    tetrisphere::SettingsStore store(data_dir / "settings.dat");
    if (argc <= first)
        return tetrisphere::run_graphics_config_gui(store, show_desktop_dialog);
    tetrisphere::Settings settings;
    std::string error;
    if (!store.load(settings, error)) {
        std::cerr << error << '\n';
        return 74;
    }
    const std::string command = argv[first];
    if (command == "show" && argc == first + 1) {
        std::cout << nlohmann::json{
            {"schema_version", 6},
            {"family_override", settings.family_override},
            {"graphics", {
                {"resolution", tetrisphere::graphics_resolution_name(settings.graphics.resolution)},
                {"aspect", tetrisphere::graphics_aspect_name(settings.graphics.aspect)},
                {"refresh", tetrisphere::graphics_refresh_name(settings.graphics.refresh)},
                {"window_mode", tetrisphere::graphics_window_mode_name(settings.graphics.window_mode)},
                {"msaa_samples", settings.graphics.msaa_samples},
                {"presentation_filter", tetrisphere::graphics_presentation_filter_name(settings.graphics.presentation_filter)},
                {"texture_filter", tetrisphere::graphics_texture_filter_name(settings.graphics.three_point_filter)}}},
            {"bindings", {
                {"confirm", button_name(settings.bindings.confirm)},
                {"cancel", button_name(settings.bindings.cancel)},
                {"face-left", button_name(settings.bindings.face_left)},
                {"face-up", button_name(settings.bindings.face_up)},
                {"magic", magic_name(settings.magic)}}}}.dump() << '\n';
        return 0;
    }
    if (command == "family" && argc == first + 2) {
        settings.family_override = argv[first + 1];
    } else if (command == "bind" && argc == first + 3) {
        if (std::string(argv[first + 1]) == "magic") {
            if (!parse_magic(argv[first + 2], settings.magic)) return usage();
        } else {
            auto* binding = binding_for(settings, argv[first + 1]);
            if (binding == nullptr || !parse_button(argv[first + 2], *binding)) return usage();
        }
    } else if (command == "graphics" && argc == first + 3) {
        const std::string option = argv[first + 1];
        const std::string value = argv[first + 2];
        if (option == "resolution") {
            if (!tetrisphere::parse_graphics_resolution(value, settings.graphics.resolution))
                return usage();
        } else if (option == "aspect") {
            if (!tetrisphere::parse_graphics_aspect(value, settings.graphics.aspect))
                return usage();
        } else if (option == "refresh") {
            if (!tetrisphere::parse_graphics_refresh(value, settings.graphics.refresh))
                return usage();
        } else if (option == "window-mode") {
            if (!tetrisphere::parse_graphics_window_mode(value, settings.graphics.window_mode))
                return usage();
        } else if (option == "msaa") {
            if (!tetrisphere::parse_graphics_msaa(value, settings.graphics.msaa_samples)) return usage();
        } else if (option == "presentation-filter") {
            if (!tetrisphere::parse_graphics_presentation_filter(value, settings.graphics.presentation_filter)) return usage();
        } else if (option == "texture-filter") {
            if (!tetrisphere::parse_graphics_texture_filter(value, settings.graphics.three_point_filter)) return usage();
        } else return usage();
    } else {
        return usage();
    }
    if (!store.save(settings, error)) {
        std::cerr << error << '\n';
        return 65;
    }
    std::cout << "settings_saved\n";
    return 0;
}
