#ifndef TETRISPHERE_DIAGNOSTICS_H
#define TETRISPHERE_DIAGNOSTICS_H

#include <cstdint>

namespace tetrisphere {

[[noreturn]] void fatal_diagnostic(const char* event, std::uint32_t target,
                                   std::uint32_t callsite, const char* phase);
[[noreturn]] void dispatch_indirect(std::uint32_t target, std::uint32_t callsite);

} // namespace tetrisphere

#endif
