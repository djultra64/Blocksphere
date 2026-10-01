#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tetrisphere {

// Raw words are deliberately unnamed beyond their observed order. Their
// button/axis semantics require comparison with the reference before use.
struct ConsumedInputEvent {
    std::uint32_t dispatch_ordinal = 0;
    std::uint8_t port = 0;
    std::uint16_t raw_word0 = 0;
    std::uint16_t raw_word1 = 0;
    std::uint16_t ring_read_cursor = 0;
    std::uint16_t ring_write_cursor = 0;
};

// Version 1 contains only raw observed words and counters with known addresses.
// Candidate probes retain offset names until their game semantics are verified.
struct CanonicalFrame {
    std::uint64_t tick = 0;
    std::uint32_t global_rng_word = 0;           // 0x800E2AA0
    std::uint64_t rng_update_count = 0;          // host count of func_80081E90 writes
    std::array<std::optional<std::uint32_t>, 2> player_rng_word{}; // context + 0x239C
    std::uint16_t input_ring_read_cursor_begin = 0;  // 0x800E1710 at drain entry
    std::uint16_t input_ring_write_cursor_begin = 0; // 0x800E1714 at drain entry
    std::uint16_t input_ring_read_cursor = 0;   // 0x800E1710 at drain exit
    std::uint16_t input_ring_write_cursor = 0;  // 0x800E1714 at drain exit
    std::uint32_t context_steps_p1 = 0;         // 0x800E4478
    std::uint32_t dispatch_count = 0;           // 0x800E4480
    std::vector<ConsumedInputEvent> consumed_input;
    bool consumed_input_truncated = false;
    std::array<std::optional<std::uint32_t>, 2> candidate_score_offset_2af8{};
    std::array<std::optional<std::uint32_t>, 2> candidate_timer_offset_2b0c{};
    std::optional<std::uint32_t> candidate_hud_word_800e241c;
    std::optional<std::uint32_t> candidate_delta_word_80160c60;
    std::array<std::optional<std::uint64_t>, 2> candidate_board_region_hash{};
};

struct CanonicalDifference {
    std::string field;
    std::uint64_t expected = 0;
    std::uint64_t actual = 0;
};

struct CanonicalSubsystemHashes {
    std::uint64_t input = 0;
    std::uint64_t rng = 0;
    std::uint64_t dispatch = 0;
    std::uint64_t candidates = 0;
};

std::string serialize_canonical_frame(const CanonicalFrame& frame);
std::optional<CanonicalDifference> first_canonical_divergence(
    const CanonicalFrame& expected, const CanonicalFrame& actual);
CanonicalSubsystemHashes hash_canonical_subsystems(const CanonicalFrame& frame);

} // namespace tetrisphere
