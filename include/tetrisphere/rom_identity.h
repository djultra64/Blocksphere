#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace tetrisphere {
inline constexpr std::size_t supported_rom_size = 8 * 1024 * 1024;
inline constexpr char supported_rom_sha256[] =
    "f7cbc93ac273488bfb89955ab6ae9b60c730907872107a67a56b945e8579ef87";

inline std::string rom_sha256(const std::vector<std::uint8_t>& bytes) {
    constexpr std::array<std::uint32_t, 64> constants = {
        0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u, 0x3956C25Bu, 0x59F111F1u,
        0x923F82A4u, 0xAB1C5ED5u, 0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u,
        0x72BE5D74u, 0x80DEB1FEu, 0x9BDC06A7u, 0xC19BF174u, 0xE49B69C1u, 0xEFBE4786u,
        0x0FC19DC6u, 0x240CA1CCu, 0x2DE92C6Fu, 0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu,
        0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u, 0xC6E00BF3u, 0xD5A79147u,
        0x06CA6351u, 0x14292967u, 0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu, 0x53380D13u,
        0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u, 0xA2BFE8A1u, 0xA81A664Bu,
        0xC24B8B70u, 0xC76C51A3u, 0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u,
        0x19A4C116u, 0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u, 0x391C0CB3u, 0x4ED8AA4Au,
        0x5B9CCA4Fu, 0x682E6FF3u, 0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u,
        0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u, 0xC67178F2u,
    };
    std::array<std::uint32_t, 8> state = {
        0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au,
        0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u,
    };
    std::vector<std::uint8_t> padded = bytes;
    const std::uint64_t bit_length = static_cast<std::uint64_t>(bytes.size()) * 8u;
    padded.push_back(0x80);
    while ((padded.size() % 64) != 56) {
        padded.push_back(0);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        padded.push_back(static_cast<std::uint8_t>(bit_length >> shift));
    }
    auto rotate_right = [](std::uint32_t value, unsigned int count) {
        return (value >> count) | (value << (32u - count));
    };
    for (std::size_t offset = 0; offset < padded.size(); offset += 64) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t at = offset + i * 4;
            words[i] = (static_cast<std::uint32_t>(padded[at]) << 24)
                     | (static_cast<std::uint32_t>(padded[at + 1]) << 16)
                     | (static_cast<std::uint32_t>(padded[at + 2]) << 8)
                     | static_cast<std::uint32_t>(padded[at + 3]);
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotate_right(words[i - 15], 7)
                                   ^ rotate_right(words[i - 15], 18)
                                   ^ (words[i - 15] >> 3);
            const std::uint32_t s1 = rotate_right(words[i - 2], 17)
                                   ^ rotate_right(words[i - 2], 19)
                                   ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }
        auto [a, b, c, d, e, f, g, h] = state;
        for (std::size_t i = 0; i < 64; ++i) {
            const std::uint32_t sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
            const std::uint32_t choice = (e & f) ^ (~e & g);
            const std::uint32_t temp1 = h + sum1 + choice + constants[i] + words[i];
            const std::uint32_t sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + sum0 + majority;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (std::uint32_t word : state) {
        result << std::setw(8) << word;
    }
    return result.str();
}

} // namespace tetrisphere
