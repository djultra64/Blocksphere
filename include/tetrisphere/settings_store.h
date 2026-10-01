#pragma once

#include "tetrisphere/linux_platform.h"
#include "tetrisphere/run_options.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace tetrisphere {

enum class GraphicsResolution { Auto, P240, P720, P1080, P1440, P2160 };
enum class GraphicsAspect { Original4x3, Expand16x9 };
enum class GraphicsRefresh { Original, Fps30, Fps60, Fps120 };
enum class GraphicsWindowMode { Windowed, Fullscreen };

struct GraphicsSettings {
    GraphicsResolution resolution = GraphicsResolution::Auto;
    GraphicsAspect aspect = GraphicsAspect::Expand16x9;
    GraphicsRefresh refresh = GraphicsRefresh::Original;
    GraphicsWindowMode window_mode = GraphicsWindowMode::Windowed;
    int msaa_samples = 0;
    PresentationFilter presentation_filter = PresentationFilter::Pixel;
    bool three_point_filter = true;
};

constexpr const char* graphics_resolution_name(GraphicsResolution value) {
    switch (value) {
        case GraphicsResolution::Auto: return "auto";
        case GraphicsResolution::P240: return "240p";
        case GraphicsResolution::P720: return "720p";
        case GraphicsResolution::P1080: return "1080p";
        case GraphicsResolution::P1440: return "1440p";
        case GraphicsResolution::P2160: return "2160p";
    }
    return nullptr;
}
constexpr const char* graphics_aspect_name(GraphicsAspect value) {
    switch (value) {
        case GraphicsAspect::Original4x3: return "4:3";
        case GraphicsAspect::Expand16x9: return "16:9";
    }
    return nullptr;
}
constexpr const char* graphics_refresh_name(GraphicsRefresh value) {
    switch (value) {
        case GraphicsRefresh::Original: return "original";
        case GraphicsRefresh::Fps30: return "30";
        case GraphicsRefresh::Fps60: return "60";
        case GraphicsRefresh::Fps120: return "120";
    }
    return nullptr;
}
constexpr const char* graphics_window_mode_name(GraphicsWindowMode value) {
    switch (value) {
        case GraphicsWindowMode::Windowed: return "windowed";
        case GraphicsWindowMode::Fullscreen: return "fullscreen";
    }
    return nullptr;
}
constexpr const char* graphics_msaa_name(int samples) {
    switch (samples) {
        case 0: return "off";
        case 2: return "2x";
        case 4: return "4x";
        case 8: return "8x";
    }
    return nullptr;
}
constexpr const char* graphics_presentation_filter_name(PresentationFilter value) {
    switch (value) {
        case PresentationFilter::Nearest: return "nearest";
        case PresentationFilter::Linear: return "linear";
        case PresentationFilter::Pixel: return "pixel";
    }
    return nullptr;
}
constexpr const char* graphics_texture_filter_name(bool three_point) {
    return three_point ? "three-point" : "linear";
}
bool parse_graphics_msaa(std::string_view name, int& samples);
bool parse_graphics_presentation_filter(std::string_view name, PresentationFilter& value);
bool parse_graphics_texture_filter(std::string_view name, bool& three_point);
RunOptions resolve_graphics_run_options(const GraphicsSettings& saved, RunOptions options);
bool parse_graphics_resolution(std::string_view name, GraphicsResolution& value);
bool parse_graphics_aspect(std::string_view name, GraphicsAspect& value);
bool parse_graphics_refresh(std::string_view name, GraphicsRefresh& value);
bool parse_graphics_window_mode(std::string_view name, GraphicsWindowMode& value);
std::pair<int, int> graphics_window_size(const GraphicsSettings& settings,
                                        int available_width, int available_height);
std::optional<double> graphics_internal_scale(GraphicsResolution resolution);

struct Settings {
    std::string family_override = "auto";
    ControllerBindings bindings{};
    MagicControl magic = MagicControl::West;
    GraphicsSettings graphics{};
};

class SettingsStore {
public:
    explicit SettingsStore(std::filesystem::path path);
    bool load(Settings& settings, std::string& error) const;
    bool save(const Settings& settings, std::string& error) const;

private:
    std::filesystem::path path_;
};

} // namespace tetrisphere
