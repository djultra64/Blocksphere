#include "tetrisphere/window_mode_controller.h"

#include <utility>

namespace tetrisphere {

WindowModeController::WindowModeController(bool initial_fullscreen, ApplyMode apply_mode)
    : fullscreen_(initial_fullscreen), apply_mode_(std::move(apply_mode)) {}

void WindowModeController::toggle() {
    const bool next = !fullscreen_;
    if (apply_mode_(next)) fullscreen_ = next;
}

bool WindowModeController::handle_key(FullscreenKey key, bool down,
                                      bool repeat, bool alt) {
    if (key == FullscreenKey::F11) {
        if (down && !repeat && !f11_down_) toggle();
        f11_down_ = down;
        return true;
    }
    if (key == FullscreenKey::Enter) {
        if (down && (alt || enter_down_)) {
            if (!repeat && !enter_down_) toggle();
            enter_down_ = true;
            return true;
        }
        if (!down && enter_down_) {
            enter_down_ = false;
            return true;
        }
    }
    return false;
}

void WindowModeController::focus_lost() {
    f11_down_ = false;
    enter_down_ = false;
}

} // namespace tetrisphere
