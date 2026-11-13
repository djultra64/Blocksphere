#include "tetrisphere/pc_settings_menu.h"

namespace tetrisphere {
namespace {
bool same_graphics(const GraphicsSettings& a, const GraphicsSettings& b) {
    return a.resolution == b.resolution && a.aspect == b.aspect &&
        a.refresh == b.refresh && a.window_mode == b.window_mode &&
        a.msaa_samples == b.msaa_samples && a.presentation_filter == b.presentation_filter &&
        a.three_point_filter == b.three_point_filter;
}
}

Settings merge_pc_settings_change(const Settings& persisted, const Settings& requested, int field) {
    auto result = persisted;
    switch (field) {
        case 0: result.graphics.resolution = requested.graphics.resolution; break;
        case 1: result.graphics.aspect = requested.graphics.aspect; break;
        case 2: result.graphics.refresh = requested.graphics.refresh; break;
        case 3: result.graphics.window_mode = requested.graphics.window_mode; break;
        case 4: result.graphics.msaa_samples = requested.graphics.msaa_samples; break;
        case 5: result.graphics.presentation_filter = requested.graphics.presentation_filter; break;
        case 6: result.graphics.three_point_filter = requested.graphics.three_point_filter; break;
        case pc_graphics_apply_field: result.graphics = requested.graphics; break;
        case 13:
            result.family_override = requested.family_override;
            result.bindings = requested.bindings;
            result.magic = requested.magic;
            break;
    }
    return result;
}

bool PCSettingsMenu::dirty() const {
    return !same_graphics(applied_.graphics, settings_.graphics);
}

bool PCSettingsMenu::restart_changes() const {
    const auto& before = applied_.graphics;
    const auto& after = settings_.graphics;
    return before.resolution != after.resolution || before.aspect != after.aspect ||
        before.refresh != after.refresh || before.msaa_samples != after.msaa_samples;
}

bool PCSettingsMenu::restart_required(int field) const {
    return field == Resolution || field == Aspect || field == FrameRate || field == Msaa;
}

std::vector<std::string> PCSettingsMenu::rows(ControllerFamily) const {
    if (page_ == Page::Root) return {"GRAPHICS", "CONTROLS", "BACK"};
    if (page_ == Page::Closed) return {};
    std::vector<std::string> result{
        "RESOLUTION", "ASPECT", "FRAME RATE", "WINDOW", "MSAA",
        "PRESENT FILTER", "TEXTURE FILTER", "APPLY",
    };
    if (exit_pending_) {
        result.emplace_back("DISCARD");
        result.emplace_back("KEEP EDITING");
    } else {
        result.emplace_back("BACK");
    }
    return result;
}

std::string PCSettingsMenu::value(int field) const {
    const auto& graphics = settings_.graphics;
    switch (field) {
        case Resolution: {
            constexpr const char* values[]{"AUTO", "240P", "720P", "1080P", "1440P", "2160P"};
            return values[int(graphics.resolution)];
        }
        case Aspect: return graphics.aspect == GraphicsAspect::Original4x3 ? "4:3" : "16:9";
        case FrameRate: {
            constexpr const char* values[]{"ORIGINAL", "30 FPS", "60 FPS", "120 FPS", "UNLIMITED"};
            return values[int(graphics.refresh)];
        }
        case Window:
            return graphics.window_mode == GraphicsWindowMode::Windowed ? "WINDOWED" : "FULLSCREEN";
        case Msaa: return graphics.msaa_samples ? std::to_string(graphics.msaa_samples) + "X" : "OFF";
        case PresentFilter: {
            constexpr const char* values[]{"NEAREST", "LINEAR", "PIXEL"};
            return values[int(graphics.presentation_filter)];
        }
        case TextureFilter: return graphics.three_point_filter ? "3-POINT" : "LINEAR";
        default: return {};
    }
}

void PCSettingsMenu::move(int direction) {
    if (saving_ || page_ == Page::Closed || direction == 0) return;
    const int first = exit_pending_ ? Apply : 0;
    const int count = page_ == Page::Root ? 3 : exit_pending_ ? 3 : 9;
    focused_ = first + (focused_ - first + (direction < 0 ? -1 : 1) + count) % count;
}

void PCSettingsMenu::adjust(int direction) {
    if (page_ != Page::Graphics || saving_ || direction == 0) return;
    if (exit_pending_) {
        move(direction);
        return;
    }
    if (focused_ >= Apply) {
        focused_ = focused_ == Apply ? BackOrDiscard : Apply;
        return;
    }
    constexpr int counts[]{6, 2, 5, 2, 4, 3, 2};
    const int count = counts[focused_];
    const auto cycle = [count, direction](int index) {
        return (index + (direction < 0 ? -1 : 1) + count) % count;
    };
    auto& graphics = settings_.graphics;
    switch (focused_) {
        case Resolution: graphics.resolution = GraphicsResolution(cycle(int(graphics.resolution))); break;
        case Aspect: graphics.aspect = GraphicsAspect(cycle(int(graphics.aspect))); break;
        case FrameRate: graphics.refresh = GraphicsRefresh(cycle(int(graphics.refresh))); break;
        case Window: graphics.window_mode = GraphicsWindowMode(cycle(int(graphics.window_mode))); break;
        case Msaa: {
            constexpr int samples[]{0, 2, 4, 8};
            const int index = graphics.msaa_samples == 8 ? 3 :
                graphics.msaa_samples == 4 ? 2 : graphics.msaa_samples == 2 ? 1 : 0;
            graphics.msaa_samples = samples[cycle(index)];
            break;
        }
        case PresentFilter:
            graphics.presentation_filter = PresentationFilter(cycle(int(graphics.presentation_filter)));
            break;
        case TextureFilter: graphics.three_point_filter = !graphics.three_point_filter; break;
    }
    status_.clear();
}

void PCSettingsMenu::select(int row) {
    if (saving_ || page_ == Page::Closed || row < 0) return;
    if (page_ == Page::Root) {
        if (row == 0) {
            page_ = Page::Graphics;
            focused_ = 0;
        } else if (row == 1) {
            focused_ = row;
            controls_request_ = true;
        } else if (row == 2) {
            back();
        }
        return;
    }
    if (exit_pending_ && row < Apply) return;
    if (row > (exit_pending_ ? KeepEditing : BackOrDiscard)) return;
    focused_ = row;
    if (row == Apply) {
        apply();
    } else if (row == BackOrDiscard) {
        if (exit_pending_) {
            settings_ = applied_;
            leave_graphics();
        } else {
            back();
        }
    } else if (row == KeepEditing) {
        keep_editing();
    } else {
        adjust(1);
    }
}

void PCSettingsMenu::apply() {
    if (!dirty()) return;
    change_ = settings_;
    saving_ = true;
    status_ = "SAVING";
}

void PCSettingsMenu::update_settings(const Settings& settings) {
    const auto pending_graphics = settings_.graphics;
    const bool had_edits = dirty();
    const bool restart = restart_changes();
    applied_ = settings;
    settings_ = settings;
    if (!saving_) {
        if (had_edits) settings_.graphics = pending_graphics;
        return;
    }
    saving_ = false;
    if (same_graphics(settings.graphics, pending_graphics)) {
        status_ = restart ? "SAVED RESTART REQUIRED" : "SAVED";
        if (exit_pending_) leave_graphics();
    } else {
        settings_.graphics = pending_graphics;
        status_ = "SAVE FAILED TRY AGAIN";
    }
}

void PCSettingsMenu::back() {
    if (saving_ || page_ == Page::Closed) return;
    if (page_ == Page::Root) {
        page_ = Page::Closed;
    } else if (exit_pending_) {
        keep_editing();
    } else if (dirty()) {
        previous_focus_ = focused_;
        focused_ = Apply;
        exit_pending_ = true;
        status_.clear();
    } else {
        leave_graphics();
    }
}

void PCSettingsMenu::leave_graphics() {
    exit_pending_ = false;
    page_ = graphics_only_ ? Page::Closed : Page::Root;
    focused_ = 0;
}

void PCSettingsMenu::keep_editing() {
    exit_pending_ = false;
    focused_ = previous_focus_;
    status_.clear();
}

std::optional<Settings> PCSettingsMenu::take_change() {
    auto result = std::move(change_);
    change_.reset();
    return result;
}
}
