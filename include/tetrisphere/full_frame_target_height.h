#pragma once

#include <cstdint>

namespace tetrisphere {

constexpr std::uint32_t full_frame_target_height(std::uint32_t draw_height,
                                                  std::uint32_t vi_width,
                                                  std::uint32_t vi_height,
                                                  std::uint32_t color_width,
                                                  bool has_full_perspective_scissor) {
    // RT64's draw bounding box can omit the final rows of a full-screen 3D
    // frame. Preserve the original VI extent only for the N64 320x240 path
    // whose perspective projection owns the complete scissor rectangle.
    if (vi_width == 320 && vi_height == 240 && color_width == vi_width &&
        has_full_perspective_scissor && draw_height >= 216 &&
        draw_height < vi_height) {
        return vi_height;
    }
    return draw_height;
}

} // namespace tetrisphere
