#include "tetrisphere/prompt_atlas.h"

#include <iostream>

namespace {

int fail(const char* message) { std::cerr << message << '\n'; return 1; }

std::uint8_t pixel(const tetrisphere::PromptAtlas& atlas, int x, int y) {
    return atlas[8 + y * 65 + x];
}

} // namespace

int main() {
    tetrisphere::PromptAtlas original{};
    original[1] = 65;
    original[3] = 12;
    original[5] = 2;
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 65; ++x) {
            original[8 + y * 65 + x] = x < 12 ? 0xA0 : x >= 53 ? 0xB0 : 0x73;
        }
    }
    tetrisphere::ControllerBindings defaults{};
    if (tetrisphere::render_prompt_atlas(
            original, tetrisphere::ControllerFamily::Xbox, defaults) != original) {
        return fail("default Xbox atlas changed original A/B icons");
    }
    const auto switch_atlas = tetrisphere::render_prompt_atlas(
        original, tetrisphere::ControllerFamily::NintendoSwitch, defaults);
    for (std::size_t i = 0; i < original.size(); ++i) {
        const bool icon_pixel = i >= 8 &&
            ((i - 8) % 65 < 12 || (i - 8) % 65 >= 53);
        if (!icon_pixel && switch_atlas[i] != original[i]) {
            return fail("atlas patch changed non-icon bytes or header");
        }
    }
    if (pixel(switch_atlas, 0, 0) != 0xB0 ||
        pixel(switch_atlas, 53, 0) != 0xA0 ||
        pixel(switch_atlas, 30, 6) != 0x73) {
        return fail("Switch A/B swap did not stay inside icon cells");
    }
    tetrisphere::ControllerBindings remapped{};
    remapped.confirm = tetrisphere::PhysicalButton::West;
    remapped.cancel = tetrisphere::PhysicalButton::North;
    const auto xbox = tetrisphere::render_prompt_atlas(
        original, tetrisphere::ControllerFamily::Xbox, remapped);
    if (pixel(xbox, 3, 3) != 0x0F || pixel(xbox, 4, 3) != 0xFF ||
        pixel(xbox, 7, 3) != 0x0F || pixel(xbox, 3, 4) != 0xFF ||
        pixel(xbox, 4, 4) != 0x0F || pixel(xbox, 56, 3) != 0x0F ||
        pixel(xbox, 57, 3) != 0xFF || pixel(xbox, 58, 5) != 0x0F ||
        pixel(xbox, 0, 0) != 0xA0 || pixel(xbox, 53, 0) != 0xA0 ||
        pixel(xbox, 30, 6) != 0x73) {
        return fail("West/North remap did not draw X/Y within native cells");
    }
    const auto playstation = tetrisphere::render_prompt_atlas(
        original, tetrisphere::ControllerFamily::PlayStation, defaults);
    if (pixel(playstation, 3, 3) != 0x0F ||
        pixel(playstation, 56, 3) != 0xFF ||
        pixel(playstation, 57, 3) != 0x0F ||
        pixel(playstation, 59, 5) != 0xFF) {
        return fail("PlayStation Cross/Circle symbols were not drawn");
    }
    const auto keyboard = tetrisphere::render_prompt_atlas(
        original, tetrisphere::ControllerFamily::Keyboard, defaults);
    if (pixel(keyboard, 3, 3) != 0x0F ||
        pixel(keyboard, 6, 3) != 0x0F ||
        pixel(keyboard, 3, 4) != 0xFF ||
        pixel(keyboard, 56, 3) != 0x0F ||
        pixel(keyboard, 59, 3) != 0x0F) {
        return fail("keyboard Z/X symbols were not drawn");
    }
    return 0;
}
