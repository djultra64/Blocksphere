#pragma once
#include "tetrisphere/controls_bridge.h"
#include <SDL.h>
namespace tetrisphere {
bool handle_controls_event(const SDL_Event&,bool assigned_controller,ControllerFamily detected);
}
