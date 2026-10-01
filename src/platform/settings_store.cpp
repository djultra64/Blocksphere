#include "tetrisphere/settings_store.h"

#include "tetrisphere/eeprom_store.h"

#include <algorithm>
#include <array>
#include <json.hpp>
#include <string_view>

namespace tetrisphere {
namespace {

bool valid_family(std::string_view value) {
    return value == "auto" || value == "keyboard" || value == "xbox" ||
           value == "playstation" || value == "nintendo-switch";
}

bool valid_button(int value) { return value >= 0 && value <= 3; }
bool valid_magic(int value) { return value >= 0 && value <= 7; }

} // namespace

bool parse_graphics_msaa(std::string_view name, int& samples) {
    for (const int candidate : {0, 2, 4, 8}) {
        if (name == graphics_msaa_name(candidate)) { samples = candidate; return true; }
    }
    return false;
}

bool parse_graphics_presentation_filter(std::string_view name, PresentationFilter& value) {
    for (const auto candidate : {PresentationFilter::Nearest, PresentationFilter::Linear,
                                 PresentationFilter::Pixel}) {
        if (name == graphics_presentation_filter_name(candidate)) {
            value = candidate; return true;
        }
    }
    return false;
}

bool parse_graphics_texture_filter(std::string_view name, bool& three_point) {
    if (name == "three-point") three_point = true;
    else if (name == "linear") three_point = false;
    else return false;
    return true;
}

RunOptions resolve_graphics_run_options(const GraphicsSettings& saved, RunOptions options) {
    if (!options.msaa_samples) options.msaa_samples = saved.msaa_samples;
    if (!options.presentation_filter) options.presentation_filter = saved.presentation_filter;
    if (!options.three_point_filter) options.three_point_filter = saved.three_point_filter;
    return options;
}

bool parse_graphics_resolution(std::string_view name, GraphicsResolution& value) {
    for (int raw = 0; raw <= static_cast<int>(GraphicsResolution::P2160); ++raw) {
        const auto candidate = static_cast<GraphicsResolution>(raw);
        if (name == graphics_resolution_name(candidate)) { value = candidate; return true; }
    }
    return false;
}

bool parse_graphics_aspect(std::string_view name, GraphicsAspect& value) {
    for (int raw = 0; raw <= static_cast<int>(GraphicsAspect::Expand16x9); ++raw) {
        const auto candidate = static_cast<GraphicsAspect>(raw);
        if (name == graphics_aspect_name(candidate)) { value = candidate; return true; }
    }
    return false;
}

bool parse_graphics_refresh(std::string_view name, GraphicsRefresh& value) {
    for (int raw = 0; raw <= static_cast<int>(GraphicsRefresh::Fps120); ++raw) {
        const auto candidate = static_cast<GraphicsRefresh>(raw);
        if (name == graphics_refresh_name(candidate)) { value = candidate; return true; }
    }
    return false;
}

bool parse_graphics_window_mode(std::string_view name, GraphicsWindowMode& value) {
    for (int raw = 0; raw <= static_cast<int>(GraphicsWindowMode::Fullscreen); ++raw) {
        const auto candidate = static_cast<GraphicsWindowMode>(raw);
        if (name == graphics_window_mode_name(candidate)) { value = candidate; return true; }
    }
    return false;
}

std::pair<int, int> graphics_window_size(const GraphicsSettings& settings,
                                        int available_width, int available_height) {
    int height = 720; // Keep the existing 960×720 default when resolution is Auto.
    switch (settings.resolution) {
        case GraphicsResolution::Auto: break;
        case GraphicsResolution::P240: height = 240; break;
        case GraphicsResolution::P720: height = 720; break;
        case GraphicsResolution::P1080: height = 1080; break;
        case GraphicsResolution::P1440: height = 1440; break;
        case GraphicsResolution::P2160: height = 2160; break;
    }
    // Output window size is independent of the RT64 internal render scale.
    // Keep it inside the usable display even if 2160p is selected on a 1080p monitor.
    const int max_width = available_width > 0 ? available_width : 960;
    const int max_height = available_height > 0 ? available_height : 720;
    const bool expanded = settings.aspect == GraphicsAspect::Expand16x9;
    height = std::max(1, std::min(height,
        std::min(max_height, expanded ? max_width * 9 / 16 : max_width * 3 / 4)));
    const int width = expanded ? (height * 16 + 4) / 9 : height * 4 / 3;
    return {width, height};
}

std::optional<double> graphics_internal_scale(GraphicsResolution resolution) {
    switch (resolution) {
        case GraphicsResolution::Auto: return std::nullopt;
        case GraphicsResolution::P240: return 1.0;
        case GraphicsResolution::P720: return 3.0;
        case GraphicsResolution::P1080: return 4.5;
        case GraphicsResolution::P1440: return 6.0;
        case GraphicsResolution::P2160: return 9.0;
    }
    return std::nullopt;
}

SettingsStore::SettingsStore(std::filesystem::path path) : path_(std::move(path)) {}

bool SettingsStore::load(Settings& settings, std::string& error) const {
    std::array<std::uint8_t, 512> record{};
    if (!EepromStore(path_).load(record, error)) return false;
    if (record[0] == 0) {
        settings = Settings{};
        return true;
    }
    const auto end = std::find(record.begin(), record.end(), 0);
    if (end == record.end() ||
        std::any_of(end, record.end(), [](std::uint8_t byte) { return byte != 0; })) {
        error = "settings data is malformed";
        return false;
    }
    try {
        const auto document = nlohmann::json::parse(
            std::string(record.begin(), end));
        const auto version = document.at("schema_version").get<int>();
        if (version < 1 || version > 6) {
            error = "unsupported settings version";
            return false;
        }
        Settings candidate;
        // Preserve the original aspect for older saved records without graphics.
        if (version < 3) candidate.graphics.aspect = GraphicsAspect::Original4x3;
        candidate.family_override = document.at("family_override").get<std::string>();
        const auto& buttons = document.at("bindings");
        if (!valid_family(candidate.family_override) || !buttons.is_array() ||
            buttons.size() != 4) {
            error = "invalid settings values";
            return false;
        }
        std::array<int, 4> values{};
        for (std::size_t i = 0; i < values.size(); ++i) {
            values[i] = buttons.at(i).get<int>();
            if (!valid_button(values[i])) {
                error = "invalid controller binding";
                return false;
            }
        }
        candidate.bindings = {static_cast<PhysicalButton>(values[0]),
                              static_cast<PhysicalButton>(values[1]),
                              static_cast<PhysicalButton>(values[2]),
                              static_cast<PhysicalButton>(values[3])};
        if (version >= 2) {
            const auto magic = document.at("magic").get<int>();
            if (!valid_magic(magic)) {
                error = "invalid magic binding";
                return false;
            }
            candidate.magic = static_cast<MagicControl>(magic);
        }
        if (version >= 3) {
            const auto& graphics = document.at("graphics");
            if (!graphics.is_object() ||
                !parse_graphics_resolution(graphics.at("resolution").get<std::string>(),
                                           candidate.graphics.resolution) ||
                !parse_graphics_aspect(graphics.at("aspect").get<std::string>(),
                                       candidate.graphics.aspect) ||
                !parse_graphics_refresh(graphics.at("refresh").get<std::string>(),
                                        candidate.graphics.refresh)) {
                error = "invalid graphics settings";
                return false;
            }
            if (version >= 4 &&
                !parse_graphics_window_mode(graphics.at("window_mode").get<std::string>(),
                                            candidate.graphics.window_mode)) {
                error = "invalid graphics settings";
                return false;
            }
        }
        if (version >= 5) {
            const auto& graphics = document.at("graphics");
            candidate.graphics.msaa_samples = graphics.at("msaa_samples").get<int>();
            if (!graphics_msaa_name(candidate.graphics.msaa_samples) ||
                !parse_graphics_presentation_filter(
                    graphics.at("presentation_filter").get<std::string>(),
                    candidate.graphics.presentation_filter) ||
                !parse_graphics_texture_filter(graphics.at("texture_filter").get<std::string>(),
                                               candidate.graphics.three_point_filter)) {
                error = "invalid graphics settings";
                return false;
            }
        }
        // Versions through 5 used LT as the default. Move that default to the
        // west face button once, retaining other saved Magic assignments.
        if (version <= 5 && candidate.magic == MagicControl::TriggerLeft)
            candidate.magic = MagicControl::West;
        settings = std::move(candidate);
        error.clear();
        return true;
    } catch (const std::exception&) {
        error = "settings data is malformed";
        return false;
    }
}

bool SettingsStore::save(const Settings& settings, std::string& error) const {
    if (!valid_family(settings.family_override)) {
        error = "invalid controller family";
        return false;
    }
    const std::array<int, 4> buttons{
        static_cast<int>(settings.bindings.confirm),
        static_cast<int>(settings.bindings.cancel),
        static_cast<int>(settings.bindings.face_left),
        static_cast<int>(settings.bindings.face_up)};
    if (!std::all_of(buttons.begin(), buttons.end(), valid_button)) {
        error = "invalid controller binding";
        return false;
    }
    if (!valid_magic(static_cast<int>(settings.magic))) {
        error = "invalid magic binding";
        return false;
    }
    const char* resolution = graphics_resolution_name(settings.graphics.resolution);
    const char* aspect = graphics_aspect_name(settings.graphics.aspect);
    const char* refresh = graphics_refresh_name(settings.graphics.refresh);
    const char* window_mode = graphics_window_mode_name(settings.graphics.window_mode);
    const char* presentation_filter =
        graphics_presentation_filter_name(settings.graphics.presentation_filter);
    if (!graphics_msaa_name(settings.graphics.msaa_samples) || !presentation_filter ||
        resolution == nullptr || aspect == nullptr || refresh == nullptr ||
        window_mode == nullptr) {
        error = "invalid graphics settings";
        return false;
    }
    const auto serialized = nlohmann::json{
        {"schema_version", 6},
        {"family_override", settings.family_override},
        {"bindings", buttons},
        {"magic", static_cast<int>(settings.magic)},
        {"graphics", {{"resolution", resolution}, {"aspect", aspect},
                      {"refresh", refresh}, {"window_mode", window_mode},
                      {"msaa_samples", settings.graphics.msaa_samples},
                      {"presentation_filter", presentation_filter},
                      {"texture_filter", graphics_texture_filter_name(
                          settings.graphics.three_point_filter)}}}}.dump();
    if (serialized.size() >= 512) {
        error = "settings exceed storage limit";
        return false;
    }
    std::array<std::uint8_t, 512> record{};
    std::copy(serialized.begin(), serialized.end(), record.begin());
    return EepromStore(path_).save(record, error);
}

} // namespace tetrisphere
