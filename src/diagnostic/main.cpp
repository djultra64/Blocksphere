#include "funcs.h"
#include "tetrisphere/runtime.h"
#include "tetrisphere/run_identity.h"
#include "tetrisphere/rom_input.h"
#include "tetrisphere/rom_cache.h"
#include "tetrisphere/rom_identity.h"
#include "tetrisphere/run_options.h"
#ifdef TETRISPHERE_LINUX_DEMO
#include "gui/rt64_file_dialog.h"
#ifdef _WIN32
#define SDL_MAIN_HANDLED
#endif
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

constexpr std::size_t kRdramSize = 9 * 1024 * 1024;
static_assert(kRdramSize >= 0x860100,
              "RDRAM backing must contain the private Training font arena");
constexpr std::size_t kInitialRom = 0x1000;
constexpr std::size_t kInitialRam = 0x25C50;
constexpr std::size_t kInitialSize = 0x100000;
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
    validated_sha256 = tetrisphere::rom_sha256(input.bytes);
    if (validated_sha256 != tetrisphere::supported_rom_sha256) {
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
#if defined(_WIN32) && defined(TETRISPHERE_LINUX_DEMO)
    SDL_SetMainReady();
#endif
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
            cache_paths, tetrisphere::supported_rom_size,
            [](const auto& bytes) { return tetrisphere::rom_sha256(bytes) == tetrisphere::supported_rom_sha256; },
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
                    [](const auto& bytes) { return tetrisphere::rom_sha256(bytes) == tetrisphere::supported_rom_sha256; });
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
            [](const auto& bytes) { return tetrisphere::rom_sha256(bytes) == tetrisphere::supported_rom_sha256; },
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
        validated_sha256 = tetrisphere::supported_rom_sha256;
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
        validated_sha256 = tetrisphere::supported_rom_sha256;
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
