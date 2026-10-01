#pragma once

#include "tetrisphere/linux_platform.h"

#include <array>
#include <cstdint>

namespace tetrisphere {

using PromptAtlas = std::array<std::uint8_t, 788>;

PromptAtlas render_prompt_atlas(const PromptAtlas& original,
                                ControllerFamily family,
                                const ControllerBindings& bindings);

} // namespace tetrisphere
