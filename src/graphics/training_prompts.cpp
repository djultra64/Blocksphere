#include "tetrisphere/training_prompts.h"

#include <array>
#include <cstring>

namespace tetrisphere {

bool TrainingPromptMarks::record(std::uint32_t destination, TrainingPrompt prompt) {
    for (std::size_t i = 0; i < count_; ++i) {
        if (entries_[i].destination != destination) continue;
        if (prompt == TrainingPrompt::None) {
            entries_[i] = entries_[--count_];
        } else {
            entries_[i].prompt = prompt;
        }
        return true;
    }
    if (prompt == TrainingPrompt::None) return true;
    if (count_ == entries_.size()) return false;
    entries_[count_++] = {destination, prompt};
    return true;
}

TrainingPrompt TrainingPromptMarks::at(std::uint32_t destination) const {
    for (std::size_t i = 0; i < count_; ++i)
        if (entries_[i].destination == destination) return entries_[i].prompt;
    return TrainingPrompt::None;
}

namespace {

struct Pattern {
    std::array<const char*, 5> rows;
};

constexpr Pattern a{{"01110", "10001", "11111", "10001", "10001"}};
constexpr Pattern b{{"11110", "10001", "11110", "10001", "11110"}};
constexpr Pattern x{{"10001", "01010", "00100", "01010", "10001"}};
constexpr Pattern y{{"10001", "01010", "00100", "00100", "00100"}};
constexpr Pattern z{{"11111", "00010", "00100", "01000", "11111"}};
constexpr Pattern i{{"11111", "00100", "00100", "00100", "11111"}};
constexpr Pattern circle{{"01110", "10001", "10001", "10001", "01110"}};
constexpr Pattern square{{"11111", "10001", "10001", "10001", "11111"}};
constexpr Pattern triangle{{"00100", "01010", "01010", "10001", "11111"}};

const Pattern& pattern_for(ControllerFamily family, PhysicalButton button) {
    switch (family) {
        case ControllerFamily::Xbox:
            switch (button) {
                case PhysicalButton::South: return a;
                case PhysicalButton::East: return b;
                case PhysicalButton::West: return x;
                case PhysicalButton::North: return y;
            }
            break;
        case ControllerFamily::NintendoSwitch:
            switch (button) {
                case PhysicalButton::South: return b;
                case PhysicalButton::East: return a;
                case PhysicalButton::West: return y;
                case PhysicalButton::North: return x;
            }
            break;
        case ControllerFamily::PlayStation:
            switch (button) {
                case PhysicalButton::South: return x;
                case PhysicalButton::East: return circle;
                case PhysicalButton::West: return square;
                case PhysicalButton::North: return triangle;
            }
            break;
        case ControllerFamily::Keyboard:
            switch (button) {
                case PhysicalButton::South: return z;
                case PhysicalButton::East: return x;
                case PhysicalButton::West: return a;
                case PhysicalButton::North: return i;
            }
            break;
    }
    return a;
}

} // namespace

TrainingPrompt training_prompt_at(std::uint32_t source_address) {
    switch (source_address) {
        case 0x800EE7CBu:
        case 0x800EEBCAu:
        case 0x800EFB9Cu: return TrainingPrompt::Confirm;
        case 0x800EEA30u:
        case 0x800EF404u: return TrainingPrompt::Cancel;
        case 0x800EF294u: return TrainingPrompt::Magic;
        default: return TrainingPrompt::None;
    }
}

TrainingMagicText training_magic_text(ControllerFamily family, MagicControl control) {
    if (control >= MagicControl::South && control <= MagicControl::North)
        return {'C', ' ', true};
    const char* label = magic_control_label(family, control);
    if (label[0] == 0 || label[1] == 0) return {
        static_cast<std::uint8_t>(label[0]), ' ', false};
    return {static_cast<std::uint8_t>(label[0]),
            static_cast<std::uint8_t>(label[1]), false};
}

std::optional<std::size_t> training_font_variant_slot(
    std::uint64_t source_hash, ControllerFamily family, PhysicalButton button) {
    // Exact identities observed in guest memory: the Training font and the
    // alternate font copied over the same address by Hide & Seek.
    constexpr std::uint64_t training_hash = 0x3e34707868992666ULL;
    constexpr std::uint64_t hide_hash = 0x8ed3320bee952129ULL;
    std::size_t source = 0;
    if (source_hash == hide_hash) source = 1;
    else if (source_hash != training_hash) return std::nullopt;
    const auto family_index = static_cast<unsigned>(family);
    const auto button_index = static_cast<unsigned>(button);
    if (family_index >= 4 || button_index >= 4) return std::nullopt;
    return source * 16 + family_index * 4 + button_index;
}

bool paint_training_prompt(std::uint8_t* image, std::size_t image_size,
                           std::uint32_t source_y, std::uint32_t cell_height,
                           std::uint32_t glyph_width,
                           TrainingPrompt prompt, ControllerFamily family,
                           const ControllerBindings& bindings,
                           std::uint32_t glyph_scale,
                           std::uint32_t visible_width) {
    if (image == nullptr || prompt == TrainingPrompt::None || image_size < 8) return false;
    const std::uint32_t width = (std::uint32_t(image[0]) << 8) | image[1];
    const std::uint32_t height = (std::uint32_t(image[2]) << 8) | image[3];
    if (visible_width == 0) visible_width = glyph_width;
    if (image[4] != 0 || image[5] != 2 || image[6] != 0 || image[7] != 0 ||
        width < 5 || height == 0 || glyph_scale == 0 || glyph_scale > 4 ||
        cell_height < 5 * glyph_scale ||
        glyph_width < 5 * glyph_scale || glyph_width > width ||
        visible_width < 5 * glyph_scale || visible_width > glyph_width ||
        source_y > height || cell_height > height - source_y ||
        std::size_t(width) * height > image_size - 8) return false;
    LogicalAction action = LogicalAction::Confirm;
    if (prompt == TrainingPrompt::Cancel) action = LogicalAction::Cancel;
    const auto& glyph = pattern_for(family, physical_button_for(bindings, action));
    const auto x0 = (visible_width - 5 * glyph_scale) / 2;
    const auto y0 = source_y + (cell_height - 5 * glyph_scale) / 2;
    for (std::uint32_t y = source_y; y < source_y + cell_height; ++y) {
        for (std::uint32_t x = 0; x < glyph_width; ++x) {
            image[8 + y * width + x] = 0;
        }
    }
    for (std::uint32_t y = 0; y < 5; ++y) {
        for (std::uint32_t x = 0; x < 5; ++x) {
            if (glyph.rows[y][x] == '1') {
                for (std::uint32_t yy = 0; yy < glyph_scale; ++yy)
                    for (std::uint32_t xx = 0; xx < glyph_scale; ++xx)
                        image[8 + (y0 + y * glyph_scale + yy) * width +
                              x0 + x * glyph_scale + xx] = 0xFF;
            }
        }
    }
    return true;
}

bool make_training_font_copy(const std::uint8_t* original, std::size_t size,
                             std::uint8_t* destination, std::size_t destination_size,
                             ControllerFamily family, PhysicalButton button,
                             std::uint32_t glyph_width, std::uint32_t glyph_scale) {
    constexpr std::size_t font_size = 8 + 20 * 1024;
    if (original == nullptr || destination == nullptr || size != font_size ||
        destination_size < font_size || original == destination ||
        original[0] != 0 || original[1] != 20 || original[2] != 4 ||
        original[3] != 0 || original[4] != 0 || original[5] != 2 ||
        original[6] != 0 || original[7] != 0) return false;
    std::memcpy(destination, original, font_size);
    ControllerBindings bindings{button, button, button, button};
    // Hide & Seek stores 20 columns per cell, but the guest draws only the
    // first 11 columns of A/B and the first 10 of C. Clear the full source
    // cell while placing the icon inside those visible columns.
    const bool hide_wide_font = glyph_width == 20 && glyph_scale == 2;
    const auto ab_width = hide_wide_font ? 11u : glyph_width;
    const auto c_width = hide_wide_font ? 10u : glyph_width;
    return paint_training_prompt(destination, font_size, 33 * 16, 16, glyph_width,
                                 TrainingPrompt::Confirm, family, bindings,
                                 glyph_scale, ab_width) &&
           paint_training_prompt(destination, font_size, 34 * 16, 16, glyph_width,
                                 TrainingPrompt::Cancel, family, bindings,
                                 glyph_scale, ab_width) &&
           paint_training_prompt(destination, font_size, 35 * 16, 16, glyph_width,
                                 TrainingPrompt::Magic, family, bindings,
                                 glyph_scale, c_width);
}

} // namespace tetrisphere
