#include "tetrisphere/config_gui.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
int fail(const char* message) { std::cerr << message << '\n'; return 1; }
}

int main(int argc, char** argv) {
    if (argc != 2) return fail("expected private directory");
    const auto dir = std::filesystem::path(argv[1]) /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(dir);
    tetrisphere::SettingsStore store(dir / "settings.dat");
    tetrisphere::Settings original;
    original.family_override = "nintendo-switch";
    original.bindings.confirm = tetrisphere::PhysicalButton::East;
    std::string error;
    if (!store.save(original, error)) return fail("could not seed settings");

    const std::vector<int> selections{0, 5, 1, 1, 2, 2, 3, 1, 4, 3, 5, 0, 6, 0, 7};
    std::size_t next = 0;
    bool saw_updated_summary = false;
    const auto dialog = [&](const std::string&, const std::string& message,
                            const std::vector<std::string>& buttons) {
        if (message.find("Resolution: 2160p") != std::string::npos)
            saw_updated_summary = true;
        if (next >= selections.size()) return -1;
        const int selected = selections[next++];
        return selected < static_cast<int>(buttons.size()) ? selected : -1;
    };
    if (tetrisphere::run_graphics_config_gui(store, dialog) != 0 ||
        next != selections.size() || !saw_updated_summary)
        return fail("graphics dialog flow did not complete");

    tetrisphere::Settings saved;
    if (!store.load(saved, error) ||
        saved.graphics.resolution != tetrisphere::GraphicsResolution::P2160 ||
        saved.graphics.aspect != tetrisphere::GraphicsAspect::Expand16x9 ||
        saved.graphics.refresh != tetrisphere::GraphicsRefresh::Fps60 ||
        saved.graphics.window_mode != tetrisphere::GraphicsWindowMode::Fullscreen ||
        saved.graphics.msaa_samples != 8 ||
        saved.graphics.presentation_filter != tetrisphere::PresentationFilter::Nearest ||
        saved.graphics.three_point_filter ||
        saved.family_override != "nintendo-switch" ||
        saved.bindings.confirm != tetrisphere::PhysicalButton::East)
        return fail("GUI did not persist graphics while preserving controls");

    const std::vector<int> cancel{0, -1, 7};
    next = 0;
    const auto cancel_dialog = [&](const std::string&, const std::string&,
                                   const std::vector<std::string>&) {
        return next < cancel.size() ? cancel[next++] : -1;
    };
    if (tetrisphere::run_graphics_config_gui(store, cancel_dialog) != 0 ||
        !store.load(saved, error) ||
        saved.graphics.resolution != tetrisphere::GraphicsResolution::P2160)
        return fail("dismissing a selector changed settings");
    return 0;
}
