#include "tetrisphere/runtime.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

namespace tetrisphere {
namespace {

std::atomic_flag terminal_claimed = ATOMIC_FLAG_INIT;

std::string json_escape(std::string_view value) {
    std::string result;
    for (const char character : value) {
        switch (character) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += character; break;
        }
    }
    return result;
}

} // namespace

[[noreturn]] void fatal_unsupported(std::string_view symbol, std::uint32_t address,
                                    std::uint32_t callsite, std::string_view phase) {
    const std::string safe_symbol = json_escape(symbol);
    const std::string safe_phase = json_escape(phase);
    if (terminal_claimed.test_and_set(std::memory_order_acq_rel)) {
        for (;;) {
            std::this_thread::yield();
        }
    }
    std::fprintf(stderr,
                 "{\"event\":\"fatal_unsupported\",\"symbol\":\"%s\","
                 "\"address\":\"0x%08X\",\"callsite\":\"0x%08X\","
                 "\"phase\":\"%s\"}\n",
                 safe_symbol.c_str(), address, callsite, safe_phase.c_str());
    std::fflush(stderr);
    std::_Exit(71);
}

} // namespace tetrisphere
