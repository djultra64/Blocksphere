#include "tetrisphere/f3d_flat_shade.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
void put(std::array<std::uint8_t, 128>& memory, std::size_t offset,
         std::uint32_t value) {
    std::memcpy(memory.data() + offset, &value, sizeof(value));
}
std::uint32_t get(const std::array<std::uint8_t, 128>& memory,
                  std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, memory.data() + offset, sizeof(value));
    return value;
}
}

int main() {
    std::array<std::uint8_t, 128> memory{};
    // The menu medals use flag 1 for the first half and flag 0 for the
    // second. Both halves must then provoke the shared vertex at index 1.
    put(memory, 64, 0xBF000000u);
    put(memory, 68, 0x01000A14u);
    put(memory, 72, 0xBF000000u);
    put(memory, 76, 0x000A141Eu);
    put(memory, 80, 0xBF000000u);
    put(memory, 84, 0x02141E00u);
    put(memory, 88, 0xB8000000u);
    {
        tetrisphere::F3DFlatShadePatch patch(memory.data(), memory.size(),
                                              0x80000040u, 32);
        if (patch.count() != 2 || get(memory, 68) != 0x000A1400u ||
            get(memory, 76) != 0x000A141Eu ||
            get(memory, 84) != 0x0000141Eu) {
            std::cerr << "F3D flat-shading vertex flag was not preserved\n";
            return 1;
        }
    }
    if (get(memory, 68) != 0x01000A14u ||
        get(memory, 84) != 0x02141E00u) {
        std::cerr << "F3D commands were not restored after rendering\n";
        return 1;
    }
    {
        tetrisphere::F3DFlatShadePatch patch(memory.data(), memory.size(),
                                              0x8000007Cu, 32);
        if (patch.count() != 0) {
            std::cerr << "out-of-range display list was modified\n";
            return 1;
        }
    }
    std::vector<std::uint8_t> medal_memory(0x300000u);
    const auto put_word = [&](std::size_t offset, std::uint32_t value) {
        std::memcpy(medal_memory.data() + offset, &value, sizeof(value));
    };
    const std::uint32_t base = 0x80268000u;
    const std::size_t quad_start = (base + 0xFC0u) & 0x00FFFFFFu;
    put_word(0x1028D4u, 0x80001000u);
    // At render submission the guest has already advanced to the next
    // double-buffered arena. The submitted display list still points here.
    put_word(0x1000u + 0x1884u, 0x80266000u);
    put_word(0x200u, 0xFD10002Bu);
    put_word(0x204u, 0x80195DC0u);
    put_word(0x208u, 0xB900031Du);
    put_word(0x20Cu, 0x00504240u);
    put_word(0x210u, 0xFC121824u);
    put_word(0x214u, 0xFF33FFFFu);
    for (unsigned quad = 0; quad < 14; ++quad) {
        const std::size_t command = 0x218u + quad * 24u;
        put_word(command, 0x04300040u);
        put_word(command + 4u, base + 0xFC0u + quad * 64u);
        put_word(command + 8u, 0xBF000000u);
        put_word(command + 12u, 0x01000A14u);
        put_word(command + 16u, 0xBF000000u);
        put_word(command + 20u, 0x000A141Eu);
    }
    put_word(0x218u + 14u * 24u, 0xB8000000u);
    for (unsigned quad = 0; quad < 14; ++quad) {
        for (unsigned vertex = 0; vertex < 4; ++vertex) {
            const auto alpha = quad_start + quad * 64u + vertex * 16u + 15u;
            medal_memory[alpha ^ 3u] = vertex == 1 ? 48 :
                                           static_cast<std::uint8_t>(9 + vertex);
        }
    }
    {
        tetrisphere::F3DMedalAlphaPatch patch(medal_memory.data(),
                                               medal_memory.size(),
                                               0x80000200u, 32u + 14u * 24u);
        if (patch.count() != 42) {
            std::cerr << "medal alpha patch did not identify exact material\n";
            return 1;
        }
        for (unsigned quad = 0; quad < 14; ++quad) {
            for (unsigned vertex = 0; vertex < 4; ++vertex) {
                const auto alpha = quad_start + quad * 64u + vertex * 16u + 15u;
                if (medal_memory[alpha ^ 3u] != (vertex == 1 ? 48 : 0)) {
                    std::cerr << "medal alpha was not restored to initial layout\n";
                    return 1;
                }
            }
        }
    }
    if (medal_memory[(quad_start + 15u) ^ 3u] != 9 ||
        medal_memory[(quad_start + 31u) ^ 3u] != 48 ||
        medal_memory[(quad_start + 47u) ^ 3u] != 11 ||
        medal_memory[(quad_start + 63u) ^ 3u] != 12) {
        std::cerr << "medal alpha bytes were not restored after rendering\n";
        return 1;
    }
    put_word(0x200u, 0xFD10002Au);
    {
        tetrisphere::F3DMedalAlphaPatch patch(medal_memory.data(),
                                               medal_memory.size(),
                                               0x80000200u, 32u + 14u * 24u);
        if (patch.count() != 0 || medal_memory[(quad_start + 63u) ^ 3u] != 12) {
            std::cerr << "unrelated texture was modified\n";
            return 1;
        }
    }
    put_word(0x200u, 0xFD10002Bu);
    put_word(0x21Cu, 0x808FF000u);
    {
        tetrisphere::F3DMedalAlphaPatch patch(medal_memory.data(),
                                               medal_memory.size(),
                                               0x80000200u, 32u + 14u * 24u);
        if (patch.count() != 0 || medal_memory[(quad_start + 63u) ^ 3u] != 12) {
            std::cerr << "invalid medal vertex address was modified\n";
            return 1;
        }
    }
}
