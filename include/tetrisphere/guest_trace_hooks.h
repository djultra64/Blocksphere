#pragma once

#include "recomp.h"

#include <cstdint>

// Hooks run inside generated N64Recomp functions. They only observe guest RAM;
// they never change registers or guest memory. Enabled by an explicit private
// TETRISPHERE_TRACE_PATH and bounded by TETRISPHERE_TRACE_MAX_FRAMES.
extern "C" {
void tetrisphere_trace_input_drain_begin(std::uint8_t* rdram, recomp_context* ctx);
void tetrisphere_trace_input_consumed(std::uint8_t* rdram, recomp_context* ctx);
void tetrisphere_trace_input_drain_end(std::uint8_t* rdram, recomp_context* ctx);
void tetrisphere_trace_rng_write(std::uint8_t* rdram, recomp_context* ctx);
}
