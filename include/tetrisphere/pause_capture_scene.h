#pragma once

#include <cstdint>

namespace tetrisphere {

// The verified guest framebuffer-copy routine serves menus and gameplay.
// Scene numbers cannot establish provenance: buffer bounds, the source writer
// and exact RGBA16 tile loads establish it at capture and consumption.
constexpr bool pause_capture_scene_eligible(std::int16_t scene_gate) {
    (void)scene_gate;
    return true;
}

constexpr bool transition_capture_buffers_eligible(std::uint32_t source,std::uint32_t destination) {
    constexpr std::uint32_t bytes=320*240*2,limit=8*1024*1024;
    return source!=0 && destination!=0 && !(source&1) && !(destination&1) &&
           source<=limit-bytes && destination<=limit-bytes &&
           (source+bytes<=destination || destination+bytes<=source);
}

} // namespace tetrisphere
