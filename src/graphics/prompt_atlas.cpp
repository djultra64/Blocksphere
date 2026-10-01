#include "tetrisphere/prompt_atlas.h"

#include <cstddef>

namespace tetrisphere {
namespace {

constexpr int atlas_width = 65;
constexpr int atlas_header = 8;
constexpr int icon_width = 12;

struct Glyph {
    const char* const* rows = nullptr;
    int width = 0;
    int original_x = -1;
};

constexpr const char* cross[] = {"10001", "01010", "00100", "01010", "10001"};
constexpr const char* circle[] = {"01110", "10001", "10001", "10001", "01110"};
constexpr const char* square[] = {"11111", "10001", "10001", "10001", "11111"};
constexpr const char* triangle[] = {"00100", "01010", "01010", "10001", "11111"};
constexpr const char* letter_x[] = {"10001", "01010", "00100", "01010", "10001"};
constexpr const char* letter_y[] = {"10001", "01010", "00100", "00100", "00100"};
constexpr const char* letter_z[] = {"1111", "0001", "0010", "0100", "1111"};
constexpr const char* letter_a[] = {"01110", "10001", "11111", "10001", "10001"};
constexpr const char* letter_i[] = {"11111", "00100", "00100", "00100", "11111"};
constexpr const char* keyboard_x[] = {"1001", "1001", "0110", "1001", "1001"};

Glyph glyph_for(ControllerFamily family, PhysicalButton button) {
    switch (family) {
        case ControllerFamily::Xbox:
            switch (button) {
                case PhysicalButton::South: return {nullptr, 0, 0};
                case PhysicalButton::East: return {nullptr, 0, 53};
                case PhysicalButton::West: return {letter_x, 5, -1};
                case PhysicalButton::North: return {letter_y, 5, -1};
            }
            break;
        case ControllerFamily::NintendoSwitch:
            switch (button) {
                case PhysicalButton::South: return {nullptr, 0, 53};
                case PhysicalButton::East: return {nullptr, 0, 0};
                case PhysicalButton::West: return {letter_y, 5, -1};
                case PhysicalButton::North: return {letter_x, 5, -1};
            }
            break;
        case ControllerFamily::PlayStation:
            switch (button) {
                case PhysicalButton::South: return {cross, 5, -1};
                case PhysicalButton::East: return {circle, 5, -1};
                case PhysicalButton::West: return {square, 5, -1};
                case PhysicalButton::North: return {triangle, 5, -1};
            }
            break;
        case ControllerFamily::Keyboard:
            switch (button) {
                case PhysicalButton::South: return {letter_z, 4, -1};
                case PhysicalButton::East: return {keyboard_x, 4, -1};
                case PhysicalButton::West: return {letter_a, 5, -1};
                case PhysicalButton::North: return {letter_i, 5, -1};
            }
            break;
    }
    return {nullptr, 0, 0};
}

void write_icon(PromptAtlas& output, const PromptAtlas& original,
                int destination_x, Glyph glyph) {
    const int source_x = glyph.original_x >= 0 ? glyph.original_x : 0;
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < icon_width; ++x) {
            output[atlas_header + y * atlas_width + destination_x + x] =
                original[atlas_header + y * atlas_width + source_x + x];
        }
    }
    if (glyph.rows == nullptr) return;
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            const bool ink = x < glyph.width && glyph.rows[y][x] == '1';
            output[atlas_header + (y + 3) * atlas_width + destination_x + x + 3] =
                ink ? 0x0F : 0xFF;
        }
    }
}

} // namespace

PromptAtlas render_prompt_atlas(const PromptAtlas& original,
                                ControllerFamily family,
                                const ControllerBindings& bindings) {
    auto output = original;
    write_icon(output, original, 0, glyph_for(family, bindings.confirm));
    write_icon(output, original, 53, glyph_for(family, bindings.cancel));
    return output;
}

} // namespace tetrisphere
