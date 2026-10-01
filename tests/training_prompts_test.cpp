#include "tetrisphere/training_prompts.h"

#include <array>
#include <cstdint>
#include <iostream>

int main() {
    using namespace tetrisphere;
    if (guest_virtual_address(0x800EE7CBu) != 0xFFFFFFFF800EE7CBULL ||
        guest_virtual_address(0x80810000u) != 0xFFFFFFFF80810000ULL) return 20;
    TrainingPromptMarks marks;
    if (!marks.record(0x80100000u, TrainingPrompt::Confirm) ||
        marks.at(0x80100000u) != TrainingPrompt::Confirm) return 16;
    // The parser reuses the same output buffer for the next word. Ordinary
    // text must clear an earlier prompt mark at that destination.
    if (!marks.record(0x80100000u, TrainingPrompt::None) ||
        marks.at(0x80100000u) != TrainingPrompt::None) return 17;
    if (!marks.record(0x80100000u, TrainingPrompt::Cancel) ||
        marks.at(0x80100000u) != TrainingPrompt::Cancel) return 18;
    marks.clear();
    if (marks.at(0x80100000u) != TrainingPrompt::None) return 19;
    constexpr std::array confirm{0x800EE7CBu, 0x800EEBCAu, 0x800EFB9Cu};
    constexpr std::array cancel{0x800EEA30u, 0x800EF404u};
    for (auto address : confirm) {
        if (training_prompt_at(address) != TrainingPrompt::Confirm) return 1;
    }
    for (auto address : cancel) {
        if (training_prompt_at(address) != TrainingPrompt::Cancel) return 2;
    }
    if (training_prompt_at(0x800EE7CAu) != TrainingPrompt::None ||
        training_prompt_at(0x800EE7CCu) != TrainingPrompt::None) return 3;
    if (training_prompt_at(0x800EF294u) != TrainingPrompt::Magic) return 4;
    const auto xbox_trigger = training_magic_text(ControllerFamily::Xbox,
                                                   MagicControl::TriggerLeft);
    const auto ps_trigger = training_magic_text(ControllerFamily::PlayStation,
                                                MagicControl::TriggerLeft);
    const auto switch_trigger = training_magic_text(ControllerFamily::NintendoSwitch,
                                                    MagicControl::TriggerLeft);
    const auto keyboard_trigger = training_magic_text(ControllerFamily::Keyboard,
                                                      MagicControl::TriggerLeft);
    const auto ps_face = training_magic_text(ControllerFamily::PlayStation,
                                             MagicControl::South);
    if (xbox_trigger.first != 'L' || xbox_trigger.second != 'T' || xbox_trigger.icon ||
        ps_trigger.first != 'L' || ps_trigger.second != '2' || ps_trigger.icon ||
        switch_trigger.first != 'Z' || switch_trigger.second != 'L' || switch_trigger.icon ||
        keyboard_trigger.first != 'K' || keyboard_trigger.second != ' ' || keyboard_trigger.icon ||
        ps_face.first != 'C' || ps_face.second != ' ' || !ps_face.icon) return 15;
    // A 12x12 IA8 font cell must show the bound physical button and preserve
    // image geometry. The expected 5x5 PlayStation cross has both diagonals.
    std::array<std::uint8_t, 8 + 20 * 1024> image{};
    image[1] = 20; image[2] = 4; image[3] = 0; image[5] = 2;
    image[8 + 400 * 20 + 19] = 0xA5;
    ControllerBindings bindings{};
    if (!paint_training_prompt(image.data(), image.size(), 400, 16, 7,
                               TrainingPrompt::Confirm,
                               ControllerFamily::PlayStation, bindings)) return 5;
    if (image[8 + 405 * 20 + 1] != 0xFF ||
        image[8 + 405 * 20 + 5] != 0xFF ||
        image[8 + 407 * 20 + 3] != 0xFF ||
        image[8 + 400 * 20 + 19] != 0xA5) return 6;

    bindings.confirm = PhysicalButton::East;
    if (!paint_training_prompt(image.data(), image.size(), 400, 16, 7,
                               TrainingPrompt::Confirm,
                               ControllerFamily::NintendoSwitch, bindings)) return 7;
    // Nintendo east is A; no diagonal crossing at the top-left of the icon.
    if (image[8 + 405 * 20 + 1] == 0xFF) return 8;
    if (paint_training_prompt(image.data(), image.size() - 1, 400, 16, 7,
                              TrainingPrompt::Confirm,
                              ControllerFamily::Xbox, bindings)) return 9;
    std::array<std::uint8_t, 8 + 20 * 1024> original{};
    original[1] = 20; original[2] = 4; original[5] = 2;
    original[8 + 33 * 16 * 20 + 19] = 0xA5;
    std::array<std::uint8_t, 8 + 20 * 1024> cross_copy{};
    std::array<std::uint8_t, 8 + 20 * 1024> circle_copy{};
    if (!make_training_font_copy(original.data(), original.size(),
                                 cross_copy.data(), cross_copy.size(),
                                 ControllerFamily::PlayStation, PhysicalButton::South)) return 10;
    if (!make_training_font_copy(original.data(), original.size(),
                                 circle_copy.data(), circle_copy.size(),
                                 ControllerFamily::PlayStation, PhysicalButton::East)) return 11;
    if (original[8 + 33 * 16 * 20 + 19] != 0xA5 ||
        original[8 + (33 * 16 + 5) * 20 + 1] != 0 ||
        cross_copy[8 + 33 * 16 * 20 + 19] != 0xA5 ||
        cross_copy[8 + (33 * 16 + 5) * 20 + 1] != 0xFF ||
        circle_copy[8 + (33 * 16 + 5) * 20 + 1] == 0xFF ||
        cross_copy[8 + (35 * 16 + 5) * 20 + 1] != 0xFF) return 12;
    const auto saved = cross_copy;
    if (!make_training_font_copy(original.data(), original.size(),
                                 circle_copy.data(), circle_copy.size(),
                                 ControllerFamily::Xbox, PhysicalButton::West)) return 13;
    if (cross_copy != saved) return 14;
    // Both verified IA8 source images can appear at the same guest address.
    // Their copied glyph variants must occupy different guest slots.
    constexpr std::uint64_t training_source = 0x3e34707868992666ULL;
    constexpr std::uint64_t hide_source = 0x8ed3320bee952129ULL;
    const auto training_slot = training_font_variant_slot(
        training_source, ControllerFamily::Keyboard, PhysicalButton::South);
    const auto hide_slot = training_font_variant_slot(
        hide_source, ControllerFamily::Keyboard, PhysicalButton::South);
    if (!training_slot || !hide_slot || *training_slot == *hide_slot ||
        training_font_variant_slot(0x1234ULL, ControllerFamily::Keyboard,
                                   PhysicalButton::South)) return 21;
    std::array<bool, 32> slots{};
    for (auto hash : {training_source, hide_source}) {
        for (auto family : {ControllerFamily::Xbox, ControllerFamily::PlayStation,
                            ControllerFamily::NintendoSwitch, ControllerFamily::Keyboard}) {
            for (auto button : {PhysicalButton::South, PhysicalButton::East,
                                PhysicalButton::West, PhysicalButton::North}) {
                const auto slot = training_font_variant_slot(hash, family, button);
                if (!slot || *slot >= slots.size() || slots[*slot]) return 22;
                slots[*slot] = true;
            }
        }
    }
    for (bool visited : slots) if (!visited) return 23;
    // Hide & Seek uses a wider font: its original A/B/C occupy more than
    // seven columns. Replacing only seven leaves a second glyph on screen.
    auto wide_original = original;
    for (auto row : {33u, 34u, 35u}) {
        for (std::size_t y = 0; y < 16; ++y)
            for (std::size_t x = 0; x < 20; ++x)
                wide_original[8 + (row * 16 + y) * 20 + x] = 0xA5;
    }
    // The guest crops A/B to 11 columns and C to 10. Compare the resulting
    // visible raster with hand-checked 5x5 letters at 2x scale; old ink must
    // also be gone from the rest of each 20-column source cell.
    struct VisibleGlyph {
        ControllerFamily family;
        std::array<const char*, 5> rows;
        PhysicalButton button = PhysicalButton::South;
    };
    constexpr std::array visible_glyphs{
        VisibleGlyph{ControllerFamily::Xbox,
                     {"01110", "10001", "11111", "10001", "10001"}},
        VisibleGlyph{ControllerFamily::PlayStation,
                     {"10001", "01010", "00100", "01010", "10001"}},
        VisibleGlyph{ControllerFamily::NintendoSwitch,
                     {"11110", "10001", "11110", "10001", "11110"}},
        VisibleGlyph{ControllerFamily::Keyboard,
                     {"11111", "00010", "00100", "01000", "11111"}},
        VisibleGlyph{ControllerFamily::Keyboard,
                     {"01110", "10001", "11111", "10001", "10001"}, PhysicalButton::West},
    };
    for (const auto& expected : visible_glyphs) {
        std::array<std::uint8_t, 8 + 20 * 1024> wide_copy{};
        if (!make_training_font_copy(wide_original.data(), wide_original.size(),
                                     wide_copy.data(), wide_copy.size(),
                                     expected.family, expected.button,
                                     20, 2)) return 24;
        for (auto row : {33u, 34u, 35u}) {
            const std::size_t visible_width = row == 35 ? 10 : 11;
            for (std::size_t y = 0; y < 16; ++y) {
                for (std::size_t x = 0; x < 20; ++x) {
                    const auto pixel = wide_copy[8 + (row * 16 + y) * 20 + x];
                    if (x >= visible_width) {
                        if (pixel != 0) return 25;
                    } else if (x < 10 && y >= 3 && y < 13) {
                        const auto want = expected.rows[(y - 3) / 2][x / 2] == '1'
                            ? 0xFF : 0;
                        if (pixel != want) return 26;
                    } else if (pixel != 0) {
                        return 27;
                    }
                }
            }
        }
    }
    if (wide_original[8 + (33 * 16 + 3) * 20 + 5] != 0xA5) return 28;
    std::cout << "training prompts ok\n";
}
