#include "funcs.h"
#include "tetrisphere/runtime.h"
#include "tetrisphere/run_identity.h"
#include "tetrisphere/rom_input.h"
#include "tetrisphere/rom_cache.h"
#include "tetrisphere/run_options.h"
#ifdef TETRISPHERE_LINUX_DEMO
#include "gui/rt64_file_dialog.h"
#include <SDL.h>
#include "tetrisphere/data_directory.h"
#endif
#ifdef TETRISPHERE_LINUX_DEMO
#include "tetrisphere/linux_platform.h"
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kRomSize = 8 * 1024 * 1024;
constexpr std::size_t kRdramSize = 9 * 1024 * 1024;
static_assert(kRdramSize >= 0x860100,
              "RDRAM backing must contain the private Training font arena");
constexpr std::size_t kInitialRom = 0x1000;
constexpr std::size_t kInitialRam = 0x25C50;
constexpr std::size_t kInitialSize = 0x100000;
constexpr char kExpectedSha256[] =
    "f7cbc93ac273488bfb89955ab6ae9b60c730907872107a67a56b945e8579ef87";

std::string sha256(const std::vector<std::uint8_t>& bytes) {
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

[[noreturn]] void fatal_rom(const char* reason) {
    const char* hint = "Choose a local NTPE rev0 ROM in z64, v64, n64 or ZIP format.";
    if (std::strcmp(reason, "io_error") == 0) {
        hint = "Check that the ROM path exists and is readable; pass it as the first argument.";
    } else if (std::strcmp(reason, "unknown_revision_or_modified") == 0) {
        hint = "Use an unmodified US NTPE rev0 dump; other revisions are not supported.";
    } else if (std::strcmp(reason, "ambiguous_archive") == 0) {
        hint = "Choose a ZIP containing exactly one z64, v64 or n64 ROM.";
    } else if (std::strcmp(reason, "rom_selection_cancelled") == 0) {
        hint = "Select a local ROM or pass its path as the first argument.";
    } else if (std::strcmp(reason, "rom_cache_unwritable") == 0) {
        hint = "Cannot write to the game's data folder. Move the complete game folder to a writable location and try again.";
    }
    std::fprintf(stderr,
                 "{\"event\":\"fatal_rom_validation\",\"phase\":\"startup\","
                 "\"reason\":\"%s\",\"hint\":\"%s\"}\n",
                 reason, hint);
#ifdef TETRISPHERE_LINUX_DEMO
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Tetrisphere error", hint, nullptr);
#endif
    std::exit(65);
}

std::vector<std::uint8_t> read_and_validate_rom(const std::filesystem::path& path,
                                                std::string& validated_sha256) {
    auto input = tetrisphere::read_rom_input(path);
    if (!input.error.empty()) fatal_rom(input.error.c_str());
    validated_sha256 = sha256(input.bytes);
    if (validated_sha256 != kExpectedSha256) {
        fatal_rom("unknown_revision_or_modified");
    }
    return std::move(input.bytes);
}

#ifdef TETRISPHERE_LINUX_DEMO
std::vector<std::filesystem::path> rom_cache_paths() {
    constexpr auto relative = "tetrisphere-us-rev0/rom.z64";
    const auto directory = tetrisphere::application_data_directory();
    if (directory.empty()) fatal_rom("rom_cache_unwritable");
    return {directory / relative};
}
#endif

} // namespace

extern "C" recomp_func_t* get_function(std::int32_t vram) {
    switch (static_cast<std::uint32_t>(vram)) {
#include "dispatch.inc"
    }
    tetrisphere::dispatch_indirect(static_cast<std::uint32_t>(vram), 0x80025C80u);
}

int main(int argc, char** argv) {
    std::filesystem::path rom_path;
    std::vector<std::uint8_t> rom;
    const std::vector<const char*> arguments(argv, argv + argc);
    const auto run_options = tetrisphere::parse_run_options(argc, arguments.data());
    if (!run_options.valid) {
        std::cerr << "Error: " << run_options.error << "\n\n"
                  << tetrisphere::run_options_help();
        return 64;
    }
    if (run_options.help) {
        std::cout << tetrisphere::run_options_help();
        return 0;
    }
#ifdef TETRISPHERE_LINUX_DEMO
    const auto cache_paths = rom_cache_paths();
    if (run_options.rom_path.empty()) {
        bool invalid_cache = false;
        auto cached = tetrisphere::read_rom_cache(
            cache_paths, kRomSize,
            [](const auto& bytes) { return sha256(bytes) == kExpectedSha256; },
            invalid_cache);
        if (cached) {
            rom = std::move(*cached);
        } else {
            if (invalid_cache) {
                std::cerr << "{\"event\":\"rom_cache_invalid\"}\n";
            }
            if (char* base = SDL_GetBasePath()) {
                const auto root = std::filesystem::u8path(base);
                SDL_free(base);
                auto discovered = tetrisphere::discover_rom_input(root,
                    [](const auto& bytes) { return sha256(bytes) == kExpectedSha256; });
                if (discovered) {
                    rom = std::move(discovered->bytes);
                    std::cerr << "{\"event\":\"rom_discovered_in_package_root\"}\n";
                    std::string cache_error;
                    if (!tetrisphere::write_rom_cache(cache_paths, rom, cache_error))
                        fatal_rom(cache_error.c_str());
                }
            }
        }
    } else {
        rom_path = std::filesystem::u8path(run_options.rom_path);
    }
#else
    if (!run_options.rom_path.empty()) {
        rom_path = std::filesystem::u8path(run_options.rom_path);
    } else {
        fatal_rom("usage_requires_rom_path");
    }
#endif
    std::string validated_sha256;
    if (rom.empty()) {
#ifdef TETRISPHERE_LINUX_DEMO
        auto selected = tetrisphere::select_compatible_rom(rom_path,
            [](const auto& bytes) { return sha256(bytes) == kExpectedSha256; },
            [] {
                RT64::FileDialog::initialize();
                auto path = RT64::FileDialog::getOpenFilename(
                    {{"Nintendo 64 ROM or ZIP", "z64,v64,n64,zip"}});
                RT64::FileDialog::finish();
                return path;
            },
            [](const std::string& error) {
                std::cerr << "{\"event\":\"rom_selection_rejected\",\"reason\":\""
                          << error << "\"}\n";
                const bool incompatible = error == "unknown_revision_or_modified";
                const char* message = incompatible
                    ? "This ROM is not compatible. Tetrisphere requires an unmodified USA ROM (NTPE rev0). European ROMs and other revisions are not supported.\n\nChoose a compatible ROM in the next window."
                    : "The selected ROM could not be read. Choose a complete, readable .z64, .v64, .n64 file or a ZIP containing exactly one ROM.\n\nChoose another file in the next window.";
                if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                        incompatible ? "ROM not supported" : "ROM could not be opened",
                        message, nullptr) != 0) fatal_rom(error.c_str());
            });
        if (!selected) return 0;
        rom = std::move(selected->bytes);
        validated_sha256 = kExpectedSha256;
#else
        rom = read_and_validate_rom(rom_path, validated_sha256);
#endif
#ifdef TETRISPHERE_LINUX_DEMO
        std::string cache_error;
        if (!tetrisphere::write_rom_cache(cache_paths, rom, cache_error)) {
            fatal_rom(cache_error.c_str());
        }
#endif
    } else {
        validated_sha256 = kExpectedSha256;
    }
    std::cout << "{\"event\":\"rom_validated\",\"rom_id\":\"tetrisphere-us-rev0\",\"run_id\":\""
              << tetrisphere::run_id() << "\",\"sha256\":\""
              << validated_sha256 << "\"}\n";
    std::cout << "{\"event\":\"diagnostic_start\",\"build_id\":\""
              << TETRISPHERE_BUILD_ID << "\",\"map_id\":\"" << TETRISPHERE_MAP_ID
              << "\",\"run_id\":\"" << tetrisphere::run_id() << "\"}\n";
    std::cout.flush();

    std::vector<std::uint8_t> rdram(kRdramSize);
    if (!tetrisphere::load_initial_image(rom, rdram, kInitialRom,
                                         kInitialRam, kInitialSize)) {
        fatal_rom("initial_image_bounds");
    }
    tetrisphere::initialize_ipl3_state(rdram.data());
    tetrisphere::set_runtime_rom(rom);
#ifdef TETRISPHERE_LINUX_DEMO
    tetrisphere::initialize_linux_platform(rdram.data(), validated_sha256.c_str(), run_options);
#endif
    recomp_context context{};
    context.f_odd = &context.f0.u32h;
    recomp_entrypoint(rdram.data(), &context);
#ifdef TETRISPHERE_LINUX_DEMO
    tetrisphere::run_linux_event_loop();
#else
    tetrisphere::await_runtime_handoff();
#endif
}
