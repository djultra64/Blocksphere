#include "tetrisphere/settings_store.h"
#include "tetrisphere/eeprom_store.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
int fail(const char* message) { std::cerr << message << '\n'; return 1; }
}

int main(int argc, char** argv) {
    if (argc != 2) return fail("expected private directory");
    const auto dir = std::filesystem::path(argv[1]) /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(dir);
    tetrisphere::SettingsStore store(dir / "settings.dat");
    tetrisphere::Settings loaded;
    std::string error;
    if (!store.load(loaded, error) || loaded.family_override != "auto" ||
        loaded.bindings.confirm != tetrisphere::PhysicalButton::South ||
        loaded.magic != tetrisphere::MagicControl::West ||
        loaded.graphics.resolution != tetrisphere::GraphicsResolution::Auto ||
        loaded.graphics.aspect != tetrisphere::GraphicsAspect::Expand16x9 ||
        loaded.graphics.refresh != tetrisphere::GraphicsRefresh::Original ||
        loaded.graphics.window_mode != tetrisphere::GraphicsWindowMode::Windowed) {
        return fail("new settings did not load defaults");
    }
    std::array<std::uint8_t, 512> quality_record{};
    const std::string quality_json = R"({"schema_version":5,"family_override":"auto","bindings":[0,1,2,3],"magic":0,"graphics":{"resolution":"auto","aspect":"4:3","refresh":"original","window_mode":"windowed","msaa_samples":8,"presentation_filter":"nearest","texture_filter":"linear"}})";
    std::copy(quality_json.begin(), quality_json.end(), quality_record.begin());
    if (!tetrisphere::EepromStore(dir / "quality.dat").save(quality_record, error))
        return fail("could not seed image quality settings");
    tetrisphere::SettingsStore quality_store(dir / "quality.dat");
    if (!quality_store.load(loaded, error))
        return fail("saved image quality settings cannot be loaded");
    if (loaded.graphics.msaa_samples != 8 ||
        loaded.graphics.presentation_filter != tetrisphere::PresentationFilter::Nearest ||
        loaded.graphics.three_point_filter)
        return fail("saved image quality values were ignored");
    if (loaded.magic != tetrisphere::MagicControl::West)
        return fail("old left-trigger default did not migrate to west");
    loaded.magic = tetrisphere::MagicControl::TriggerLeft;
    if (!quality_store.save(loaded, error) || !quality_store.load(loaded, error) ||
        loaded.magic != tetrisphere::MagicControl::TriggerLeft)
        return fail("explicit left-trigger mapping was not retained after migration");
    const auto effective = tetrisphere::resolve_graphics_run_options(loaded.graphics, {});
    if (effective.msaa_samples != 8 ||
        effective.presentation_filter != tetrisphere::PresentationFilter::Nearest ||
        effective.three_point_filter != false)
        return fail("renderer options ignored saved image quality");
    tetrisphere::RunOptions overrides;
    overrides.msaa_samples = 0;
    overrides.presentation_filter = tetrisphere::PresentationFilter::Linear;
    overrides.three_point_filter = true;
    const auto overridden = tetrisphere::resolve_graphics_run_options(loaded.graphics, overrides);
    if (overridden.msaa_samples != 0 ||
        overridden.presentation_filter != tetrisphere::PresentationFilter::Linear ||
        overridden.three_point_filter != true || loaded.graphics.msaa_samples != 8)
        return fail("run options did not override saved image quality without changing it");
    tetrisphere::Settings changed;
    changed.family_override = "nintendo-switch";
    changed.bindings.confirm = tetrisphere::PhysicalButton::East;
    changed.bindings.cancel = tetrisphere::PhysicalButton::South;
    changed.magic = tetrisphere::MagicControl::RightShoulder;
    changed.graphics.resolution = tetrisphere::GraphicsResolution::P2160;
    changed.graphics.aspect = tetrisphere::GraphicsAspect::Expand16x9;
    changed.graphics.refresh = tetrisphere::GraphicsRefresh::Fps120;
    changed.graphics.window_mode = tetrisphere::GraphicsWindowMode::Fullscreen;
    changed.graphics.msaa_samples = 4;
    changed.graphics.presentation_filter = tetrisphere::PresentationFilter::Linear;
    changed.graphics.three_point_filter = false;
    if (!store.save(changed, error) || !store.load(loaded, error) ||
        loaded.family_override != "nintendo-switch" ||
        loaded.bindings.confirm != tetrisphere::PhysicalButton::East ||
        loaded.bindings.cancel != tetrisphere::PhysicalButton::South ||
        loaded.magic != tetrisphere::MagicControl::RightShoulder ||
        loaded.graphics.resolution != tetrisphere::GraphicsResolution::P2160 ||
        loaded.graphics.aspect != tetrisphere::GraphicsAspect::Expand16x9 ||
        loaded.graphics.refresh != tetrisphere::GraphicsRefresh::Fps120 ||
        loaded.graphics.window_mode != tetrisphere::GraphicsWindowMode::Fullscreen ||
        loaded.graphics.msaa_samples != 4 ||
        loaded.graphics.presentation_filter != tetrisphere::PresentationFilter::Linear ||
        loaded.graphics.three_point_filter) {
        return fail("settings changes were not durable");
    }
    tetrisphere::Settings newer = changed;
    newer.family_override = "playstation";
    if (!store.save(newer, error)) return fail("second settings commit failed");
    {
        std::ofstream torn(dir / "settings.dat", std::ios::binary | std::ios::trunc);
        torn << "partial";
    }
    if (!store.load(loaded, error) || loaded.family_override != "nintendo-switch") {
        return fail("interrupted settings write lost the last valid configuration");
    }
    std::array<std::uint8_t, 512> legacy{};
    const std::string v1 = "{\"schema_version\":1,\"family_override\":\"xbox\",\"bindings\":[0,1,2,3]}";
    std::copy(v1.begin(), v1.end(), legacy.begin());
    if (!tetrisphere::EepromStore(dir / "legacy.dat").save(legacy, error)) {
        return fail("could not create legacy fixture");
    }
    tetrisphere::SettingsStore legacy_store(dir / "legacy.dat");
    if (!legacy_store.load(loaded, error) || loaded.family_override != "xbox" ||
        loaded.magic != tetrisphere::MagicControl::West ||
        loaded.graphics.resolution != tetrisphere::GraphicsResolution::Auto) {
        return fail("schema v1 did not migrate Magic to the west face button");
    }
    std::array<std::uint8_t, 512> old_v2{};
    const std::string v2 = "{\"schema_version\":2,\"family_override\":\"auto\",\"bindings\":[0,1,2,3],\"magic\":3}";
    std::copy(v2.begin(), v2.end(), old_v2.begin());
    if (!tetrisphere::EepromStore(dir / "old-v2.dat").save(old_v2, error))
        return fail("could not create v2 fixture");
    tetrisphere::SettingsStore old_v2_store(dir / "old-v2.dat");
    if (!old_v2_store.load(loaded, error) ||
        loaded.graphics.resolution != tetrisphere::GraphicsResolution::Auto ||
        loaded.graphics.aspect != tetrisphere::GraphicsAspect::Original4x3 ||
        loaded.graphics.refresh != tetrisphere::GraphicsRefresh::Original)
        return fail("schema v2 did not migrate graphics defaults");
    std::array<std::uint8_t, 512> old_v3{};
    const std::string v3 = "{\"schema_version\":3,\"family_override\":\"auto\",\"bindings\":[0,1,2,3],\"magic\":0,\"graphics\":{\"resolution\":\"2160p\",\"aspect\":\"16:9\",\"refresh\":\"60\"}}";
    std::copy(v3.begin(), v3.end(), old_v3.begin());
    if (!tetrisphere::EepromStore(dir / "old-v3.dat").save(old_v3, error))
        return fail("could not create v3 fixture");
    tetrisphere::SettingsStore old_v3_store(dir / "old-v3.dat");
    if (!old_v3_store.load(loaded, error) ||
        loaded.graphics.window_mode != tetrisphere::GraphicsWindowMode::Windowed ||
        loaded.graphics.resolution != tetrisphere::GraphicsResolution::P2160)
        return fail("schema v3 did not migrate window mode");
    std::array<std::uint8_t, 512> old_v4{};
    const std::string v4 = R"({"schema_version":4,"family_override":"playstation","bindings":[1,0,2,3],"magic":3,"graphics":{"resolution":"1080p","aspect":"16:9","refresh":"60","window_mode":"fullscreen"}})";
    std::copy(v4.begin(), v4.end(), old_v4.begin());
    if (!tetrisphere::EepromStore(dir / "old-v4.dat").save(old_v4, error))
        return fail("could not create v4 fixture");
    tetrisphere::SettingsStore old_v4_store(dir / "old-v4.dat");
    if (!old_v4_store.load(loaded, error) || loaded.graphics.msaa_samples != 0 ||
        loaded.graphics.presentation_filter != tetrisphere::PresentationFilter::Pixel ||
        !loaded.graphics.three_point_filter || loaded.family_override != "playstation" ||
        loaded.bindings.confirm != tetrisphere::PhysicalButton::East ||
        loaded.graphics.window_mode != tetrisphere::GraphicsWindowMode::Fullscreen)
        return fail("schema v4 migration lost existing settings or changed image quality defaults");
    // Restore the legacy state used by the invalid-record checks below.
    if (!old_v3_store.load(loaded, error)) return fail("could not reload v3 fixture");
    std::array<std::uint8_t, 512> invalid_v3{};
    const std::string bad_graphics = "{\"schema_version\":3,\"family_override\":\"auto\",\"bindings\":[0,1,2,3],\"magic\":0,\"graphics\":{\"resolution\":\"500p\",\"aspect\":\"4:3\",\"refresh\":\"60\"}}";
    std::copy(bad_graphics.begin(), bad_graphics.end(), invalid_v3.begin());
    if (!tetrisphere::EepromStore(dir / "invalid-v3.dat").save(invalid_v3, error))
        return fail("could not create invalid v3 fixture");
    tetrisphere::SettingsStore invalid_v3_store(dir / "invalid-v3.dat");
    if (invalid_v3_store.load(loaded, error) || error != "invalid graphics settings" ||
        loaded.graphics.resolution != tetrisphere::GraphicsResolution::P2160)
        return fail("invalid v3 graphics changed loaded settings");
    std::array<std::uint8_t, 512> invalid_v4{};
    const std::string bad_window_mode = "{\"schema_version\":4,\"family_override\":\"auto\",\"bindings\":[0,1,2,3],\"magic\":0,\"graphics\":{\"resolution\":\"2160p\",\"aspect\":\"16:9\",\"refresh\":\"60\",\"window_mode\":\"invalid\"}}";
    std::copy(bad_window_mode.begin(), bad_window_mode.end(), invalid_v4.begin());
    if (!tetrisphere::EepromStore(dir / "invalid-v4.dat").save(invalid_v4, error))
        return fail("could not create invalid v4 fixture");
    tetrisphere::SettingsStore invalid_v4_store(dir / "invalid-v4.dat");
    if (invalid_v4_store.load(loaded, error) || error != "invalid graphics settings" ||
        loaded.graphics.window_mode != tetrisphere::GraphicsWindowMode::Windowed)
        return fail("invalid v4 window mode changed loaded settings");
    auto invalid = changed;
    invalid.magic = static_cast<tetrisphere::MagicControl>(8);
    if (store.save(invalid, error) || error != "invalid magic binding") {
        return fail("invalid magic binding was accepted");
    }
    invalid = changed;
    invalid.graphics.resolution = static_cast<tetrisphere::GraphicsResolution>(99);
    if (store.save(invalid, error) || error != "invalid graphics settings")
        return fail("invalid graphics resolution was accepted");
    invalid = changed;
    invalid.graphics.window_mode = static_cast<tetrisphere::GraphicsWindowMode>(99);
    if (store.save(invalid, error) || error != "invalid graphics settings")
        return fail("invalid window mode was accepted");
    invalid = changed;
    invalid.graphics.msaa_samples = 3;
    if (store.save(invalid, error)) return fail("unsupported MSAA count was saved");
    invalid = changed;
    invalid.graphics.presentation_filter = static_cast<tetrisphere::PresentationFilter>(99);
    if (store.save(invalid, error)) return fail("invalid presentation filter was saved");
    const auto dimensions = tetrisphere::graphics_window_size(changed.graphics, 3840, 2160);
    if (dimensions.first != 3840 || dimensions.second != 2160)
        return fail("2160p expanded window dimensions are incorrect");
    changed.graphics.aspect = tetrisphere::GraphicsAspect::Original4x3;
    if (tetrisphere::graphics_window_size(changed.graphics, 3840, 2160).first != 2880)
        return fail("2160p 4:3 window dimensions are incorrect");
    changed.graphics.aspect = tetrisphere::GraphicsAspect::Expand16x9;
    const auto small_display = tetrisphere::graphics_window_size(changed.graphics, 1920, 1080);
    if (small_display.first != 1920 || small_display.second != 1080)
        return fail("2160p internal setting overflowed a 1080p window");
    if (tetrisphere::graphics_internal_scale(tetrisphere::GraphicsResolution::Auto) ||
        tetrisphere::graphics_internal_scale(tetrisphere::GraphicsResolution::P240) != 1.0 ||
        tetrisphere::graphics_internal_scale(tetrisphere::GraphicsResolution::P720) != 3.0 ||
        tetrisphere::graphics_internal_scale(tetrisphere::GraphicsResolution::P1080) != 4.5 ||
        tetrisphere::graphics_internal_scale(tetrisphere::GraphicsResolution::P1440) != 6.0 ||
        tetrisphere::graphics_internal_scale(tetrisphere::GraphicsResolution::P2160) != 9.0)
        return fail("internal resolution scales are incorrect");
    std::array<std::uint8_t, 512> unsupported{};
    const std::string future = "{\"schema_version\":7,\"family_override\":\"auto\",\"bindings\":[0,1,2,3],\"magic\":0}";
    std::copy(future.begin(), future.end(), unsupported.begin());
    if (!tetrisphere::EepromStore(dir / "future.dat").save(unsupported, error)) {
        return fail("could not create future-version fixture");
    }
    tetrisphere::SettingsStore future_store(dir / "future.dat");
    if (future_store.load(loaded, error) || error != "unsupported settings version") {
        return fail("future settings version was silently accepted");
    }
    return 0;
}
