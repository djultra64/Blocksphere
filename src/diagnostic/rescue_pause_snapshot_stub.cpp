#include "tetrisphere/recomp_support.h"

// The diagnostic executor has no RT64 renderer. The guest copy still runs and
// its native RAM bytes remain available to diagnostic tests.
extern "C" void tetrisphere_rescue_pause_snapshot_hook(std::uint8_t*,
                                                        recomp_context*) {}

// The headless diagnostic executor still recompiles every producer callsite.
// It has no RT64 draw calls to tag, so these hooks intentionally have no effect.
extern "C" void tetrisphere_ui_span_begin(std::uint8_t*, recomp_context*,
                                           std::uint32_t, std::uint32_t) {}
extern "C" void tetrisphere_ui_span_end(std::uint8_t*, recomp_context*,
                                         std::uint32_t) {}
