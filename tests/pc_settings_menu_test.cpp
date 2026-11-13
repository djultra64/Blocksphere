#include "tetrisphere/pc_settings_menu.h"
#include <cassert>
#include <set>

using namespace tetrisphere;

int main() {
    Settings original;
    original.bindings.confirm = PhysicalButton::East;
    original.magic = MagicControl::RightShoulder;
    original.graphics.msaa_samples = 4;
    PCSettingsMenu expanded(original, true);
    const auto rows = expanded.rows(ControllerFamily::Keyboard);
    assert(rows.size() == 9);
    assert(rows[2] == "FRAME RATE");
    assert(rows[7] == "APPLY" && rows[8] == "BACK");
    for (const auto& row : rows)
        assert(row != "MORE" && row != "DISPLAY" && row != "QUALITY");

    constexpr int counts[]{6, 2, 5, 2, 4, 3, 2};
    for (int field = 0; field < 7; ++field) {
        PCSettingsMenu menu(original, true);
        for (int row = 0; row < field; ++row) menu.move(1);
        const auto initial = menu.value(field);
        std::set<std::string> seen;
        for (int choice = 0; choice < counts[field]; ++choice) {
            menu.adjust(1);
            assert(!menu.saving() && !menu.take_change());
            seen.insert(menu.value(field));
        }
        assert(seen.size() == static_cast<std::size_t>(counts[field]) && menu.value(field) == initial);
        if (field == 2)
            assert(seen.contains("60 FPS") && seen.contains("120 FPS") &&
                   seen.contains("UNLIMITED"));
        menu.back();
        assert(menu.closed() && !menu.take_change());
    }

    PCSettingsMenu pending(original, true);
    pending.adjust(1);
    pending.move(1);
    pending.adjust(1);
    pending.select(7);
    assert(pending.changed_field() == 12 && pending.saving());
    auto request = pending.take_change();
    assert(request && !pending.take_change());
    assert(request->graphics.resolution != original.graphics.resolution);
    assert(request->graphics.aspect != original.graphics.aspect);
    pending.adjust(1);
    pending.back();
    assert(!pending.closed() && pending.saving());
    Settings persisted = original;
    persisted.magic = MagicControl::LeftShoulder;
    auto saved = merge_pc_settings_change(persisted, *request, 12);
    assert(saved.graphics.resolution == request->graphics.resolution);
    assert(saved.graphics.aspect == request->graphics.aspect);
    assert(saved.bindings.confirm == PhysicalButton::East);
    assert(saved.magic == MagicControl::LeftShoulder);
    pending.update_settings(saved);
    assert(!pending.saving() && !pending.closed() && pending.status() == "SAVED RESTART REQUIRED");
    pending.back();
    assert(pending.closed() && !pending.take_change());

    PCSettingsMenu dirty(original, true);
    dirty.adjust(-1);
    const auto edited = dirty.value(0);
    dirty.back();
    assert(!dirty.closed() && dirty.rows(ControllerFamily::Keyboard).size() == 10);
    dirty.adjust(-1);
    assert(dirty.focused() == 9);
    dirty.select(9);
    assert(!dirty.closed() && dirty.value(0) == edited && dirty.focused() == 0);
    dirty.back();
    dirty.select(7);
    assert(dirty.saving() && dirty.take_change());
    dirty.update_settings(original);
    assert(!dirty.closed() && !dirty.saving());
    assert(dirty.value(0) == edited && dirty.status() == "SAVE FAILED TRY AGAIN");
    dirty.select(8);
    assert(dirty.closed() && dirty.value(0) == expanded.value(0) && !dirty.take_change());

    PCSettingsMenu apply_and_exit(original, true);
    apply_and_exit.adjust(1);
    apply_and_exit.back();
    apply_and_exit.select(7);
    const auto exit_request = apply_and_exit.take_change();
    assert(exit_request);
    apply_and_exit.update_settings(*exit_request);
    assert(apply_and_exit.closed());

    PCSettingsMenu navigation(original, true);
    navigation.move(-1);
    assert(navigation.focused() == 8);
    navigation.select(8);
    assert(navigation.closed() && !navigation.take_change());
    PCSettingsMenu controls(original);
    controls.select(1);
    assert(controls.take_controls_request() && !controls.take_controls_request());

    Settings stale;
    stale.bindings.confirm = PhysicalButton::West;
    persisted.graphics.resolution = GraphicsResolution::P2160;
    auto remap = merge_pc_settings_change(persisted, stale, 13);
    assert(remap.graphics.resolution == GraphicsResolution::P2160 &&
           remap.bindings.confirm == PhysicalButton::West);
}
