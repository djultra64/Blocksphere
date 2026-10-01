#pragma once

#include <SDL.h>

#include <atomic>
#include <cstdint>

namespace tetrisphere {

class KeyboardState {
public:
    bool handle_event(const SDL_Event& event);
    std::uint16_t buttons() const;

private:
    std::atomic<std::uint32_t> buttons_{0};
};

} // namespace tetrisphere
