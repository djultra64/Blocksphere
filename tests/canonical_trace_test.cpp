#include "tetrisphere/canonical_trace.h"

#include <iostream>

namespace {
int fail(const char* why) { std::cerr << why << '\n'; return 1; }
} // namespace

int main() {
    using namespace tetrisphere;
    CanonicalFrame left{};
    left.tick = 42;
    left.global_rng_word = 0x12345678;
    left.player_rng_word = {11, 22};
    left.input_ring_read_cursor = 17;
    left.input_ring_write_cursor = 19;
    left.context_steps_p1 = 8;
    left.dispatch_count = 9;
    left.consumed_input.push_back({0, 0, 0x1000, 0x8000, 17, 19});
    left.consumed_input.push_back({1, 1, 0x2000, 0x4000, 18, 19});
    left.candidate_score_offset_2af8[0] = 123;
    left.candidate_timer_offset_2b0c[1] = 18059;
    left.candidate_hud_word_800e241c = 77;

    const auto serialized = serialize_canonical_frame(left);
    if (serialized.find("tetrisphere-canonical-v1\n") != 0 ||
        serialized.find("consumed_input[1].raw_word1=0000000000004000\n") == std::string::npos ||
        serialized.find("candidate_score_offset_2af8[0]=000000000000007b\n") == std::string::npos ||
        serialized.find("candidate_timer_offset_2b0c[1]=000000000000468b\n") == std::string::npos)
        return fail("canonical serialization lacks stable version, ordering or names");
    const auto hashes = hash_canonical_subsystems(left);
    if (hashes.input == 0 || hashes.rng == 0 || hashes.dispatch == 0)
        return fail("subsystem hashes were not produced");
    auto right = left;
    if (first_canonical_divergence(left, right).has_value() ||
        serialize_canonical_frame(right) != serialized)
        return fail("identical frames diverged");
    right.consumed_input[1].raw_word1 ^= 1;
    auto difference = first_canonical_divergence(left, right);
    if (!difference || difference->field != "consumed_input[1].raw_word1" ||
        difference->expected != 0x4000 || difference->actual != 0x4001)
        return fail("first consumed-input divergence was not identified");
    if (hash_canonical_subsystems(right).input == hashes.input ||
        hash_canonical_subsystems(right).rng != hashes.rng)
        return fail("input divergence affected wrong subsystem hash");
    right = left;
    right.candidate_score_offset_2af8[0].reset();
    difference = first_canonical_divergence(left, right);
    if (!difference || difference->field != "candidate_score_offset_2af8[0].present")
        return fail("candidate probe presence mismatch was hidden");
    right = left;
    right.candidate_timer_offset_2b0c[1] = 18058;
    difference = first_canonical_divergence(left, right);
    if (!difference || difference->field != "candidate_timer_offset_2b0c[1]" ||
        hash_canonical_subsystems(right).candidates == hashes.candidates)
        return fail("candidate timer difference was hidden");
    right = left;
    right.consumed_input.pop_back();
    difference = first_canonical_divergence(left, right);
    if (!difference || difference->field != "consumed_input.count")
        return fail("input-event count mismatch was hidden");
    return 0;
}
