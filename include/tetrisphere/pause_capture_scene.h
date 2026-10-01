#pragma once

#include <cstdint>

namespace tetrisphere {

// The guest uses -1 while a game is active. This rejects the ordinary menu
// transitions; the source-workload and 5x8 grid checks scope the copy further.
constexpr bool pause_capture_scene_eligible(std::int16_t scene_gate) {
    return scene_gate == -1;
}

} // namespace tetrisphere
