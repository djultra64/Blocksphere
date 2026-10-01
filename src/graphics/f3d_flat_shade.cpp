#include "tetrisphere/f3d_flat_shade.h"

#include <algorithm>
#include <cstring>

namespace tetrisphere {

F3DFlatShadePatch::F3DFlatShadePatch(std::uint8_t* rdram,
                                     std::size_t rdram_bytes,
                                     std::uint32_t display_list_address,
                                     std::size_t display_list_bytes)
    : rdram_(rdram) {
    constexpr std::size_t max_display_list_bytes = 0x10000;
    constexpr std::uint8_t tri1 = 0xBF;
    constexpr std::uint8_t end_dl = 0xB8;
    const std::size_t start = display_list_address & 0x03FFFFFFu;
    if (rdram_ == nullptr || start >= rdram_bytes) {
        return;
    }
    const std::size_t bytes = std::min({display_list_bytes,
                                        max_display_list_bytes,
                                        rdram_bytes - start});
    for (std::size_t offset = start; offset + 8 <= start + bytes; offset += 8) {
        std::uint32_t command = 0;
        std::memcpy(&command, rdram_ + offset, sizeof(command));
        const std::uint8_t opcode = command >> 24;
        if (opcode == end_dl) {
            break;
        }
        if (opcode != tri1) {
            continue;
        }
        std::uint32_t original = 0;
        std::memcpy(&original, rdram_ + offset + 4, sizeof(original));
        const std::uint8_t selector = original >> 24;
        if (selector != 1 && selector != 2) {
            continue;
        }
        const std::uint8_t a = (original >> 16) & 0xFFu;
        const std::uint8_t b = (original >> 8) & 0xFFu;
        const std::uint8_t c = original & 0xFFu;
        const std::uint32_t rotated = selector == 1
            ? (std::uint32_t(b) << 16) | (std::uint32_t(c) << 8) | a
            : (std::uint32_t(c) << 16) | (std::uint32_t(a) << 8) | b;
        changes_.push_back({offset + 4, original});
        std::memcpy(rdram_ + offset + 4, &rotated, sizeof(rotated));
    }
}

F3DFlatShadePatch::~F3DFlatShadePatch() {
    for (const Change& change : changes_) {
        std::memcpy(rdram_ + change.offset, &change.original,
                    sizeof(change.original));
    }
}

F3DMedalAlphaPatch::F3DMedalAlphaPatch(std::uint8_t* rdram,
                                       std::size_t rdram_bytes,
                                       std::uint32_t display_list_address,
                                       std::size_t display_list_bytes)
    : rdram_(rdram) {
    constexpr std::size_t max_display_list_bytes = 0x10000;
    constexpr std::uint32_t end_dl = 0xB8000000u;
    constexpr std::uint32_t medal_texture_command = 0xFD10002Bu;
    constexpr std::uint32_t medal_texture = 0x80195DC0u;
    constexpr std::uint32_t medal_other_mode_command = 0xB900031Du;
    constexpr std::uint32_t medal_other_mode = 0x00504240u;
    constexpr std::uint32_t medal_combine_command = 0xFC121824u;
    constexpr std::uint32_t medal_combine = 0xFF33FFFFu;
    constexpr std::uint32_t medal_vertex_command = 0x04300040u;
    constexpr std::size_t medal_quad_count = 14;
    if (!rdram_) return;
    const std::size_t list_start = display_list_address & 0x03FFFFFFu;
    if (list_start >= rdram_bytes) return;
    const std::size_t list_bytes = std::min({display_list_bytes,
                                             max_display_list_bytes,
                                             rdram_bytes - list_start});
    struct VertexBlock {
        std::uint32_t address;
        bool first_triangle = false;
        bool second_triangle = false;
    };
    std::vector<VertexBlock> blocks;
    bool texture = false, other_mode = false, combine = false;
    for (std::size_t offset = list_start;
         offset + 8 <= list_start + list_bytes; offset += 8) {
        std::uint32_t command = 0, argument = 0;
        std::memcpy(&command, rdram_ + offset, sizeof(command));
        std::memcpy(&argument, rdram_ + offset + 4, sizeof(argument));
        if (command == end_dl) break;
        texture |= command == medal_texture_command && argument == medal_texture;
        other_mode |= command == medal_other_mode_command &&
                      argument == medal_other_mode;
        combine |= command == medal_combine_command && argument == medal_combine;
        if (other_mode && combine && command == medal_vertex_command) {
            blocks.push_back({argument});
        } else if (!blocks.empty() && command == 0xBF000000u) {
            VertexBlock& block = blocks.back();
            if (argument == 0x01000A14u) block.first_triangle = true;
            if (argument == 0x000A141Eu && block.first_triangle) {
                block.second_triangle = true;
            }
        }
    }
    if (!(texture && other_mode && combine)) return;
    for (std::size_t first = 0; first + medal_quad_count <= blocks.size(); ++first) {
        const std::uint32_t address = blocks[first].address;
        const std::uint32_t segment = address & 0xFF000000u;
        if (segment != 0x80000000u && segment != 0xA0000000u) continue;
        const std::size_t quad_start = address & 0x00FFFFFFu;
        if (quad_start > rdram_bytes ||
            medal_quad_count * 64u > rdram_bytes - quad_start) continue;
        bool exact = true;
        for (std::size_t quad = 0; quad < medal_quad_count; ++quad) {
            const auto& block = blocks[first + quad];
            if (block.address != address + quad * 64u ||
                !block.first_triangle || !block.second_triangle) {
                exact = false;
                break;
            }
        }
        if (!exact) continue;
        for (std::size_t quad = 0; quad < medal_quad_count; ++quad) {
            for (unsigned vertex : {0u, 2u, 3u}) {
                const std::size_t offset = (quad_start + quad * 64u +
                                            vertex * 16u + 15u) ^ 3u;
                if (rdram_[offset]) {
                    changes_.push_back({offset, rdram_[offset]});
                    rdram_[offset] = 0;
                }
            }
        }
        first += medal_quad_count - 1;
    }
}

F3DMedalAlphaPatch::~F3DMedalAlphaPatch() {
    for (const Change& change : changes_) {
        rdram_[change.offset] = change.original;
    }
}

} // namespace tetrisphere
