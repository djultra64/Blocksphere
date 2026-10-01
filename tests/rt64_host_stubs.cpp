// microcode_live_test links RT64 without the game host. Its fixtures exercise
// RSP/RDP execution, not Tetrisphere's UI provenance or pause snapshot.
#include <cstddef>
#include <cstdint>

extern "C" bool tetrisphere_rt64_pause_snapshot_peek(
    std::uint32_t*, std::uint32_t*, std::uint64_t*, std::uint64_t*) {
    return false;
}

extern "C" void tetrisphere_rt64_pause_snapshot_ack(std::uint64_t) {}
extern "C" void tetrisphere_rt64_clear_keyboard_input() {}

extern "C" void tetrisphere_rt64_ui_tag_update(
    std::uint32_t, const std::uint32_t*, std::size_t) {}

extern "C" std::uint32_t tetrisphere_rt64_ui_tag_current() {
    return 0;
}
