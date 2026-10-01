#include "tetrisphere/diagnostics.h"

#include <cstdio>
#include <cstdlib>

namespace tetrisphere {

[[noreturn]] void fatal_diagnostic(const char* event, std::uint32_t target,
                                   std::uint32_t callsite, const char* phase) {
    std::fprintf(stderr,
                 "{\"event\":\"%s\",\"phase\":\"%s\","
                 "\"target\":\"0x%08X\",\"callsite\":\"0x%08X\"}\n",
                 event, phase, target, callsite);
    std::fflush(stderr);
    std::exit(70);
}

[[noreturn]] void dispatch_indirect(std::uint32_t target, std::uint32_t callsite) {
    if (target == 0x8007EF9Cu) {
        fatal_diagnostic("fatal_unimplemented_confirmed_indirect", target, callsite,
                         "m1-recompiled-entry");
    }
    fatal_diagnostic("fatal_unresolved_indirect", target, callsite,
                     "m1-recompiled-entry");
}

} // namespace tetrisphere
