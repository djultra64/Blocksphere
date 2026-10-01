#include "tetrisphere/rom_input.h"

#include "miniz.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const char* message) { std::cerr << message << '\n'; return 1; }

void write(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return fail("expected private test directory");
    const auto dir = std::filesystem::path(argv[1]) /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(dir);
    std::vector<std::uint8_t> canonical(8u * 1024u * 1024u);
    canonical[0] = 0x80; canonical[1] = 0x37;
    canonical[2] = 0x12; canonical[3] = 0x40;
    canonical[100] = 0xa5; canonical[101] = 0x5a;
    write(dir / "game.z64", canonical);
    auto result = tetrisphere::read_rom_input(dir / "game.z64");
    if (!result.error.empty() || result.bytes != canonical) {
        return fail("canonical ROM bytes changed");
    }
    std::vector<std::uint8_t> v64 = canonical;
    for (std::size_t i = 0; i < v64.size(); i += 2) std::swap(v64[i], v64[i + 1]);
    write(dir / "game.v64", v64);
    result = tetrisphere::read_rom_input(dir / "game.v64");
    if (!result.error.empty() || result.bytes != canonical) {
        return fail("v64 word order was not normalized");
    }
    std::vector<std::uint8_t> n64 = canonical;
    for (std::size_t i = 0; i < n64.size(); i += 4) {
        std::reverse(n64.begin() + i, n64.begin() + i + 4);
    }
    write(dir / "game.n64", n64);
    result = tetrisphere::read_rom_input(dir / "game.n64");
    if (!result.error.empty() || result.bytes != canonical) {
        return fail("n64 word order was not normalized");
    }
    mz_zip_archive archive{};
    const auto zip_path = dir / "game.zip";
    if (!mz_zip_writer_init_file(&archive, zip_path.string().c_str(), 0) ||
        !mz_zip_writer_add_mem(&archive, "game.v64", v64.data(), v64.size(), MZ_BEST_SPEED) ||
        !mz_zip_writer_finalize_archive(&archive)) {
        return fail("could not create ZIP fixture");
    }
    mz_zip_writer_end(&archive);
    result = tetrisphere::read_rom_input(zip_path);
    if (!result.error.empty() || result.bytes != canonical) {
        return fail("ZIP ROM was not normalized");
    }
    write(dir / "broken.z64", {0x80, 0x37, 0x12, 0x40});
    if (tetrisphere::read_rom_input(dir / "broken.z64").error != "truncated_rom") {
        return fail("truncated ROM did not get an actionable error");
    }
    const auto validate = [&](const auto& bytes) { return bytes == canonical; };
    std::filesystem::create_directories(dir / "nested");
    write(dir / "nested" / "Tetrisphere.z64", canonical);
    if (tetrisphere::discover_rom_input(dir, validate))
        return fail("root discovery searched nested or unrelated files");
    write(dir / "TETRI-SPHERE 0.z64", {0x80, 0x37, 0x12, 0x40});
    auto wrong_revision = canonical;
    wrong_revision[100] = 0;
    write(dir / "TETRI-SPHERE 1.z64", wrong_revision);
    write(dir / "TETRI-SPHERE 2 (USA).V64", v64);
    const auto discovered = tetrisphere::discover_rom_input(dir, validate);
    if (!discovered || discovered->bytes != canonical)
        return fail("root discovery failed to skip invalid files and import a similar name");
    if (tetrisphere::discover_rom_input(dir / "missing", validate))
        return fail("missing discovery directory produced a ROM");
    unsigned selections = 0;
    std::vector<std::string> rejected;
    const auto accepted = tetrisphere::select_compatible_rom(
        dir / "TETRI-SPHERE 1.z64", validate,
        [&]() { ++selections; return dir / "game.v64"; },
        [&](const auto& error) { rejected.push_back(error); });
    if (!accepted || accepted->bytes != canonical || selections != 1 ||
        rejected != std::vector<std::string>{"unknown_revision_or_modified"})
        return fail("incompatible selection was not explained and retried");
    selections = 0; rejected.clear();
    const auto cancelled = tetrisphere::select_compatible_rom(
        dir / "broken.z64", validate,
        [&]() { ++selections; return std::filesystem::path{}; },
        [&](const auto& error) { rejected.push_back(error); });
    if (cancelled || selections != 1 || rejected != std::vector<std::string>{"truncated_rom"})
        return fail("cancel after a rejected selection did not exit without a ROM");
    return 0;
}
