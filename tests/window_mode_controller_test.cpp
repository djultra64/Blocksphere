#include "tetrisphere/window_mode_controller.h"

#include <cassert>
#include <vector>

int main() {
    using tetrisphere::FullscreenKey;
    std::vector<bool> transitions;
    bool fail_next = false;
    tetrisphere::WindowModeController controller(false, [&](bool fullscreen) {
        transitions.push_back(fullscreen);
        if (fail_next) { fail_next = false; return false; }
        return true;
    });
    assert(controller.handle_key(FullscreenKey::F11, true, false, false));
    assert(controller.fullscreen() && transitions.size() == 1);
    assert(controller.handle_key(FullscreenKey::F11, true, true, false));
    assert(controller.handle_key(FullscreenKey::F11, true, false, false));
    assert(transitions.size() == 1);
    assert(controller.handle_key(FullscreenKey::F11, false, false, false));
    assert(controller.handle_key(FullscreenKey::F11, true, false, false));
    assert(!controller.fullscreen() && transitions.size() == 2);
    controller.focus_lost();
    assert(controller.handle_key(FullscreenKey::Enter, true, false, true));
    assert(controller.fullscreen() && transitions.size() == 3);
    assert(controller.handle_key(FullscreenKey::Enter, true, true, true));
    assert(controller.handle_key(FullscreenKey::Enter, false, false, false));
    assert(transitions.size() == 3);
    fail_next = true;
    assert(controller.handle_key(FullscreenKey::Enter, true, false, true));
    assert(controller.fullscreen() && transitions.size() == 4);
    assert(controller.handle_key(FullscreenKey::Enter, false, false, false));
    assert(!controller.handle_key(FullscreenKey::Enter, true, false, false));
    return 0;
}
