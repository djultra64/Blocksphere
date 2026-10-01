#pragma once

#include "tetrisphere/linux_platform.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace tetrisphere {

// N64Recomp MEM_* expects a sign-extended KSEG address in a 64-bit register.
constexpr std::uint64_t guest_virtual_address(std::uint32_t address) {
    return static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int32_t>(address)));
}

enum class TrainingPrompt { None, Confirm, Cancel, Magic };

class TrainingPromptMarks {
public:
    void clear() { count_ = 0; }
    bool record(std::uint32_t destination, TrainingPrompt prompt);
    TrainingPrompt at(std::uint32_t destination) const;

private:
    struct Mark {
        std::uint32_t destination;
        TrainingPrompt prompt;
    };
    std::array<Mark, 32> entries_{};
    std::size_t count_ = 0;
};

struct TrainingMagicText {
    std::uint8_t first;
    std::uint8_t second;
    bool icon;
};

TrainingPrompt training_prompt_at(std::uint32_t source_address);
TrainingMagicText training_magic_text(ControllerFamily family, MagicControl control);
std::optional<std::size_t> training_font_variant_slot(
    std::uint64_t source_hash, ControllerFamily family, PhysicalButton button);
bool paint_training_prompt(std::uint8_t* image, std::size_t image_size,
                           std::uint32_t source_y, std::uint32_t cell_height,
                           std::uint32_t glyph_width,
                           TrainingPrompt prompt, ControllerFamily family,
                           const ControllerBindings& bindings,
                           std::uint32_t glyph_scale = 1,
                           std::uint32_t visible_width = 0);
bool make_training_font_copy(const std::uint8_t* original, std::size_t size,
                             std::uint8_t* destination, std::size_t destination_size,
                             ControllerFamily family, PhysicalButton button,
                             std::uint32_t glyph_width = 7,
                             std::uint32_t glyph_scale = 1);

} // namespace tetrisphere
