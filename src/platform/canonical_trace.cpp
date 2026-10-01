#include "tetrisphere/canonical_trace.h"

#include <utility>

namespace tetrisphere {
namespace {
enum class Group { Input, Rng, Dispatch, Candidate };
struct Field {
    std::string name;
    std::uint64_t value;
    Group group;
};

std::vector<Field> fields(const CanonicalFrame& frame) {
    std::vector<Field> result;
    result.reserve(12 + frame.consumed_input.size() * 6);
    const auto add = [&result](std::string name, std::uint64_t value, Group group) {
        result.push_back({std::move(name), value, group});
    };
    add("tick", frame.tick, Group::Dispatch);
    add("global_rng_word", frame.global_rng_word, Group::Rng);
    add("rng_update_count", frame.rng_update_count, Group::Rng);
    for (int port = 0; port < 2; ++port) {
        const auto name = "player_rng_word[" + std::to_string(port) + "]";
        add(name + ".present", frame.player_rng_word[port].has_value(), Group::Rng);
        if (frame.player_rng_word[port])
            add(name, *frame.player_rng_word[port], Group::Rng);
    }
    add("input_ring_read_cursor_begin", frame.input_ring_read_cursor_begin, Group::Input);
    add("input_ring_write_cursor_begin", frame.input_ring_write_cursor_begin, Group::Input);
    add("input_ring_read_cursor_end", frame.input_ring_read_cursor, Group::Input);
    add("input_ring_write_cursor_end", frame.input_ring_write_cursor, Group::Input);
    add("context_steps_p1", frame.context_steps_p1, Group::Dispatch);
    add("dispatch_count", frame.dispatch_count, Group::Dispatch);
    add("consumed_input.count", frame.consumed_input.size(), Group::Input);
    add("consumed_input.truncated", frame.consumed_input_truncated, Group::Input);
    for (std::size_t i = 0; i < frame.consumed_input.size(); ++i) {
        const auto prefix = "consumed_input[" + std::to_string(i) + "].";
        const auto& event = frame.consumed_input[i];
        add(prefix + "dispatch_ordinal", event.dispatch_ordinal, Group::Input);
        add(prefix + "port", event.port, Group::Input);
        add(prefix + "raw_word0", event.raw_word0, Group::Input);
        add(prefix + "raw_word1", event.raw_word1, Group::Input);
        add(prefix + "ring_read_cursor", event.ring_read_cursor, Group::Input);
        add(prefix + "ring_write_cursor", event.ring_write_cursor, Group::Input);
    }
    for (int port = 0; port < 2; ++port) {
        const auto score = "candidate_score_offset_2af8[" + std::to_string(port) + "]";
        add(score + ".present", frame.candidate_score_offset_2af8[port].has_value(),
            Group::Candidate);
        if (frame.candidate_score_offset_2af8[port])
            add(score, *frame.candidate_score_offset_2af8[port], Group::Candidate);
        const auto timer = "candidate_timer_offset_2b0c[" + std::to_string(port) + "]";
        add(timer + ".present", frame.candidate_timer_offset_2b0c[port].has_value(),
            Group::Candidate);
        if (frame.candidate_timer_offset_2b0c[port])
            add(timer, *frame.candidate_timer_offset_2b0c[port], Group::Candidate);
        const auto board = "candidate_board_region_hash[" + std::to_string(port) + "]";
        add(board + ".present", frame.candidate_board_region_hash[port].has_value(),
            Group::Candidate);
        if (frame.candidate_board_region_hash[port])
            add(board, *frame.candidate_board_region_hash[port], Group::Candidate);
    }
    add("candidate_hud_word_800e241c.present",
        frame.candidate_hud_word_800e241c.has_value(), Group::Candidate);
    if (frame.candidate_hud_word_800e241c)
        add("candidate_hud_word_800e241c", *frame.candidate_hud_word_800e241c,
            Group::Candidate);
    add("candidate_delta_word_80160c60.present",
        frame.candidate_delta_word_80160c60.has_value(), Group::Candidate);
    if (frame.candidate_delta_word_80160c60)
        add("candidate_delta_word_80160c60", *frame.candidate_delta_word_80160c60,
            Group::Candidate);
    return result;
}

void append_hex(std::string& output, std::uint64_t value) {
    constexpr char digits[] = "0123456789abcdef";
    for (int shift = 60; shift >= 0; shift -= 4)
        output += digits[(value >> shift) & 0xf];
}

std::uint64_t fnv_field(std::uint64_t hash, const Field& field) {
    constexpr std::uint64_t prime = 1099511628211ull;
    for (unsigned char byte : field.name) { hash ^= byte; hash *= prime; }
    hash ^= 0; hash *= prime;
    for (int shift = 0; shift < 64; shift += 8) {
        hash ^= static_cast<std::uint8_t>(field.value >> shift);
        hash *= prime;
    }
    return hash;
}
} // namespace

std::string serialize_canonical_frame(const CanonicalFrame& frame) {
    std::string output = "tetrisphere-canonical-v1\n";
    for (const auto& field : fields(frame)) {
        output += field.name;
        output += '=';
        append_hex(output, field.value);
        output += '\n';
    }
    return output;
}

std::optional<CanonicalDifference> first_canonical_divergence(
    const CanonicalFrame& expected, const CanonicalFrame& actual) {
    const auto left = fields(expected);
    const auto right = fields(actual);
    const auto common = left.size() < right.size() ? left.size() : right.size();
    for (std::size_t i = 0; i < common; ++i) {
        if (left[i].name != right[i].name)
            return CanonicalDifference{left[i].name + " (field order)", i, i};
        if (left[i].value != right[i].value)
            return CanonicalDifference{left[i].name, left[i].value, right[i].value};
    }
    if (left.size() != right.size())
        return CanonicalDifference{"field.count", left.size(), right.size()};
    return std::nullopt;
}

CanonicalSubsystemHashes hash_canonical_subsystems(const CanonicalFrame& frame) {
    constexpr std::uint64_t basis = 14695981039346656037ull;
    CanonicalSubsystemHashes result{basis, basis, basis, basis};
    for (const auto& field : fields(frame)) {
        switch (field.group) {
            case Group::Input: result.input = fnv_field(result.input, field); break;
            case Group::Rng: result.rng = fnv_field(result.rng, field); break;
            case Group::Dispatch: result.dispatch = fnv_field(result.dispatch, field); break;
            case Group::Candidate: result.candidates = fnv_field(result.candidates, field); break;
        }
    }
    return result;
}

} // namespace tetrisphere
