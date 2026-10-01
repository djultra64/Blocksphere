#pragma once

#include "recomp.h"
#include <cstdint>

namespace tetrisphere {
bool take_close_game_request();
}

extern "C" void tetrisphere_close_game_menu_hook(std::uint8_t* rdram,
                                                recomp_context* ctx,
                                                std::uint32_t callsite);
