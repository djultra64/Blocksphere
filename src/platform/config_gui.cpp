#include "tetrisphere/config_gui.h"

#include <string>
#include <utility>
#include <vector>

namespace tetrisphere {

int run_graphics_config_gui(const SettingsStore& store, const ConfigGuiDialog& dialog) {
    Settings settings;
    std::string error;
    if (!store.load(settings, error)) {
        dialog("Tetrisphere settings error", error, {"Close"});
        return 74;
    }

    for (;;) {
        const std::string summary =
            std::string("Resolution: ") + graphics_resolution_name(settings.graphics.resolution) +
            "\nAspect ratio: " + graphics_aspect_name(settings.graphics.aspect) +
            "\nRefresh request: " + graphics_refresh_name(settings.graphics.refresh) +
            "\nWindow mode: " + graphics_window_mode_name(settings.graphics.window_mode) +
            "\nMSAA: " + graphics_msaa_name(settings.graphics.msaa_samples) +
            "\nPresentation filter: " + graphics_presentation_filter_name(settings.graphics.presentation_filter) +
            "\nTexture filter: " + graphics_texture_filter_name(settings.graphics.three_point_filter) +
            "\n\nChoose a setting to change. Changes save immediately and may require "
            "restarting the game.";
        const int field = dialog("Tetrisphere graphics settings", summary,
                                 {"Resolution", "Aspect ratio", "Refresh",
                                  "Window mode", "MSAA", "Presentation", "Textures", "Close"});
        if (field == -2) return 74;
        if (field < 0 || field >= 7) return 0;

        Settings next = settings;
        int choice = -1;
        switch (field) {
            case 0: {
                static const std::vector<std::string> options{
                    "Auto", "240p", "720p", "1080p", "1440p", "2160p", "Back"};
                choice = dialog("Resolution", "Current: " +
                    std::string(graphics_resolution_name(settings.graphics.resolution)), options);
                if (choice >= 0 && choice < 6)
                    next.graphics.resolution = static_cast<GraphicsResolution>(choice);
                break;
            }
            case 1: {
                static const std::vector<std::string> options{
                    "Original (4:3)", "Widescreen (16:9)", "Back"};
                choice = dialog("Aspect ratio", "Current: " +
                    std::string(graphics_aspect_name(settings.graphics.aspect)), options);
                if (choice >= 0 && choice < 2)
                    next.graphics.aspect = static_cast<GraphicsAspect>(choice);
                break;
            }
            case 2: {
                static const std::vector<std::string> options{
                    "Original", "30 FPS", "60 FPS", "120 FPS", "Back"};
                choice = dialog("Refresh request",
                    "The game may cap this request. Current: " +
                    std::string(graphics_refresh_name(settings.graphics.refresh)), options);
                if (choice >= 0 && choice < 4)
                    next.graphics.refresh = static_cast<GraphicsRefresh>(choice);
                break;
            }
            case 3: {
                static const std::vector<std::string> options{
                    "Windowed", "Fullscreen", "Back"};
                choice = dialog("Window mode", "Current: " +
                    std::string(graphics_window_mode_name(settings.graphics.window_mode)), options);
                if (choice >= 0 && choice < 2)
                    next.graphics.window_mode = static_cast<GraphicsWindowMode>(choice);
                break;
            }
            case 4: {
                choice = dialog("MSAA", "Current: " +
                    std::string(graphics_msaa_name(settings.graphics.msaa_samples)),
                    {"Off", "2x", "4x", "8x", "Back"});
                constexpr int samples[] = {0, 2, 4, 8};
                if (choice >= 0 && choice < 4) next.graphics.msaa_samples = samples[choice];
                break;
            }
            case 5: {
                choice = dialog("Presentation filter", "Filters the final image. Current: " +
                    std::string(graphics_presentation_filter_name(settings.graphics.presentation_filter)),
                    {"Nearest", "Linear", "Pixel", "Back"});
                if (choice >= 0 && choice < 3)
                    next.graphics.presentation_filter = static_cast<PresentationFilter>(choice);
                break;
            }
            case 6: {
                choice = dialog("Texture filter", "Filters N64 textures. Current: " +
                    std::string(graphics_texture_filter_name(settings.graphics.three_point_filter)),
                    {"Linear", "Three-point", "Back"});
                if (choice >= 0 && choice < 2) next.graphics.three_point_filter = choice == 1;
                break;
            }
        }
        constexpr int back_indices[] = {6, 2, 4, 2, 4, 3, 2};
        const int back = back_indices[field];
        if (choice == -2) return 74;
        if (choice < 0 || choice >= back) continue;
        if (!store.save(next, error)) {
            dialog("Tetrisphere settings error", error, {"Close"});
            return 65;
        }
        settings = std::move(next);
    }
}

} // namespace tetrisphere
