#include "tetrisphere/menu_medal_wrap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace tetrisphere {
namespace {
constexpr std::size_t quad_count = 14;
constexpr std::int32_t period = 6080, quad_width = 703;
constexpr std::uint32_t vertex_command = 0x04300040u;
constexpr std::uint32_t branch_command = 0x06010000u;

std::uint32_t word(const std::uint8_t* rdram, std::size_t offset) {
    std::uint32_t value;
    std::memcpy(&value, rdram + offset, sizeof(value));
    return value;
}
std::int16_t half(const std::uint8_t* rdram, std::size_t offset) {
    std::int16_t value;
    std::memcpy(&value, rdram + (offset ^ 2u), sizeof(value));
    return value;
}
bool valid_address(std::uint32_t address) {
    const auto segment = address & 0xFF000000u;
    return segment == 0x80000000u || segment == 0xA0000000u;
}
bool overlap(std::size_t first, std::size_t size,
             std::size_t other, std::size_t other_size) {
    return first < other + other_size && other < first + size;
}
struct Block {
    std::size_t command, last_triangle = 0;
    std::uint32_t address;
    std::size_t first_triangle = 0;
};
struct Duplicate {
    std::size_t argument_word;
    std::size_t original;
    std::int32_t shift;
};

bool valid_quad(const std::uint8_t* rdram, std::size_t offset) {
    const auto x = half(rdram, offset), y = half(rdram, offset + 2);
    for (int vertex = 0; vertex < 4; ++vertex) {
        const auto p = offset + vertex * 16;
        if (half(rdram, p) != x + (vertex % 2) * quad_width ||
            half(rdram, p + 2) != y + (vertex / 2) * quad_width ||
            half(rdram, p + 4) != -4000 ||
            half(rdram, p + 8) != (vertex % 2) * 2752 ||
            half(rdram, p + 10) != (1 - vertex / 2) * 2752) return false;
    }
    return true;
}
} // namespace

MenuMedalWrapPatch::MenuMedalWrapPatch(
    std::uint8_t* rdram, std::size_t rdram_bytes,
    std::uint32_t display_list_address, std::size_t display_list_bytes,
    float aspect_ratio, std::uint32_t scratch_address, std::size_t scratch_bytes) {
    if (!rdram || !std::isfinite(aspect_ratio) ||
        aspect_ratio <= 4.0f / 3.0f + 0.001f || aspect_ratio > 4.0f ||
        !valid_address(display_list_address) || !valid_address(scratch_address) ||
        (display_list_address & 7u) || (scratch_address & 15u)) return;
    const std::size_t list = display_list_address & 0x00FFFFFFu;
    const std::size_t scratch = scratch_address & 0x00FFFFFFu;
    if (list >= rdram_bytes || scratch >= rdram_bytes ||
        scratch_bytes > rdram_bytes - scratch) return;
    const auto bytes = std::min({display_list_bytes, std::size_t(0x10000),
                                 rdram_bytes - list});
    if (overlap(scratch, scratch_bytes, list, bytes)) return;

    std::vector<Block> blocks;
    bool other_mode = false, combine = false, texture = false;
    for (std::size_t p = list; p + 8 <= list + bytes; p += 8) {
        const auto cmd = word(rdram, p), arg = word(rdram, p + 4);
        if (cmd == 0xB8000000u) break;
        other_mode |= cmd == 0xB900031Du && arg == 0x00504240u;
        combine |= cmd == 0xFC121824u && arg == 0xFF33FFFFu;
        texture |= cmd == 0xFD10002Bu && arg == 0x80195DC0u;
        if (other_mode && combine && cmd == vertex_command) {
            blocks.push_back({p, 0, arg, 0});
        } else if (!blocks.empty() && cmd == 0xBF000000u) {
            auto& block = blocks.back();
            // Both forms occur depending on whether flat-shade correction
            // has already rotated the original first triangle's flag.
            if (arg == 0x01000A14u || arg == 0x000A1400u)
                block.first_triangle = p;
            if (arg == 0x000A141Eu && block.first_triangle + 8 == p)
                block.last_triangle = p;
        }
    }
    if (!texture || !other_mode || !combine) return;
    std::array<Block, quad_count> medals{};
    bool found = false;
    for (std::size_t first = 0; first + quad_count <= blocks.size(); ++first) {
        const auto address = blocks[first].address;
        if (!valid_address(address) || (address & 15u)) continue;
        const std::size_t vertices = address & 0x00FFFFFFu;
        if (vertices >= rdram_bytes || quad_count * 64 > rdram_bytes - vertices ||
            overlap(scratch, scratch_bytes, vertices, quad_count * 64)) continue;
        bool exact = true;
        for (std::size_t q = 0; q < quad_count; ++q) {
            const auto& block = blocks[first + q];
            if (block.address != address + q * 64 || !block.last_triangle ||
                !valid_quad(rdram, vertices + q * 64) ||
                (q + 1 < quad_count && block.last_triangle >= blocks[first + q + 1].command)) {
                exact = false;
                break;
            }
        }
        if (!exact) continue;
        // Ambiguous signatures must leave the task untouched.
        if (found) return;
        std::copy_n(blocks.begin() + first, quad_count, medals.begin());
        found = true;
    }
    if (!found) return;

    // func_80038A88 builds a 45-degree perspective and translates the
    // z=-4000 medal plane by -500. Native wrapping at 6080 is hidden in 4:3,
    // but crosses the visible 16:9 edge. Continue the periodic plane without
    // modifying its phase, vertical motion, tint, texture or original quads.
    const float half_width = 4500.0f * std::tan(3.14159265358979323846f / 8.0f) * aspect_ratio;
    std::vector<std::uint32_t> commands;
    std::vector<Duplicate> duplicates;
    const auto first_command = medals.front().command;
    const auto past_command = medals.back().last_triangle + 8;
    std::size_t current = 0;
    for (std::size_t p = first_command; p < past_command; p += 8) {
        commands.push_back(word(rdram, p));
        commands.push_back(word(rdram, p + 4));
        if (current >= quad_count || p != medals[current].last_triangle) continue;
        const auto vertex_offset = medals[current].address & 0x00FFFFFFu;
        const std::int32_t x = half(rdram, vertex_offset);
        for (std::int32_t shift : {-period, period}) {
            if (x + shift >= half_width || x + shift + quad_width <= -half_width) continue;
            const auto argument = commands.size() + 1;
            commands.insert(commands.end(), {vertex_command, 0,
                0xBF000000u, word(rdram, medals[current].last_triangle - 4),
                0xBF000000u, word(rdram, medals[current].last_triangle + 4)});
            duplicates.push_back({argument, vertex_offset, shift});
        }
        ++current;
    }
    if (duplicates.empty()) return;
    commands.insert(commands.end(), {branch_command,
        0x80000000u | static_cast<std::uint32_t>(past_command)});
    const auto vertex_start = (commands.size() * 4 + 15) & ~std::size_t(15);
    const auto used = vertex_start + duplicates.size() * 64;
    if (used > scratch_bytes) return;
    std::vector<std::uint8_t> replacement(used);
    for (std::size_t i = 0; i < duplicates.size(); ++i) {
        const auto& copy = duplicates[i];
        const auto offset = vertex_start + i * 64;
        commands[copy.argument_word] = (scratch_address & 0xFF000000u) |
            static_cast<std::uint32_t>(scratch + offset);
        std::memcpy(replacement.data() + offset, rdram + copy.original, 64);
        for (std::size_t v = 0; v < 4; ++v) {
            const auto x = static_cast<std::int16_t>(half(rdram, copy.original + v * 16) + copy.shift);
            std::memcpy(replacement.data() + ((offset + v * 16) ^ 2u), &x, 2);
        }
    }
    std::memcpy(replacement.data(), commands.data(), commands.size() * 4);
    // Allocate the restoration copy before the first write. No allocation or
    // unchecked range computation occurs while guest memory is changed.
    scratch_original_.assign(rdram + scratch, rdram + scratch + used);
    rdram_ = rdram;
    redirect_offset_ = first_command;
    scratch_offset_ = scratch;
    original_command_ = word(rdram, first_command);
    original_argument_ = word(rdram, first_command + 4);
    std::memcpy(rdram + scratch, replacement.data(), used);
    std::memcpy(rdram + first_command, &branch_command, 4);
    std::memcpy(rdram + first_command + 4, &scratch_address, 4);
    duplicates_ = duplicates.size();
}

MenuMedalWrapPatch::~MenuMedalWrapPatch() {
    if (!rdram_) return;
    std::memcpy(rdram_ + redirect_offset_, &original_command_, 4);
    std::memcpy(rdram_ + redirect_offset_ + 4, &original_argument_, 4);
    std::memcpy(rdram_ + scratch_offset_, scratch_original_.data(), scratch_original_.size());
}

} // namespace tetrisphere
