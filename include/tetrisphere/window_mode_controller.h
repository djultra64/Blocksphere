#pragma once

#include <functional>

namespace tetrisphere {

enum class FullscreenKey { F11, Enter, Other };

class WindowModeController {
public:
    using ApplyMode = std::function<bool(bool fullscreen)>;
    WindowModeController(bool initial_fullscreen, ApplyMode apply_mode);

    // Returns true when a shortcut event belongs to the window manager.
    bool handle_key(FullscreenKey key, bool down, bool repeat, bool alt);
    void focus_lost();
    bool fullscreen() const { return fullscreen_; }

private:
    void toggle();
    bool fullscreen_ = false;
    bool f11_down_ = false;
    bool enter_down_ = false;
    ApplyMode apply_mode_;
};

} // namespace tetrisphere
