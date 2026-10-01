#include "tetrisphere/rom_cache.h"

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
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return fail("expected test directory");
    const auto dir = std::filesystem::path(argv[1]) /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(dir);
    const auto portable = dir / "portable" / "data" / "tetrisphere-us-rev0" / "rom.z64";
    const auto user = dir / "user" / "data" / "tetrisphere-us-rev0" / "rom.z64";
    const std::vector<std::uint8_t> canonical{0x80, 0x37, 0x12, 0x40, 0xa5, 0x5a};
    const auto validate = [&canonical](const auto& bytes) { return bytes == canonical; };
    bool invalid = false;
    if (tetrisphere::read_rom_cache({portable, user}, canonical.size(), validate, invalid))
        return fail("cache must be absent before first import");
    std::string error;
    if (!tetrisphere::write_rom_cache({portable, user}, canonical, error) ||
        !std::filesystem::is_regular_file(portable))
        return fail("first import did not publish portable cache");
    const auto cached = tetrisphere::read_rom_cache(
        {portable, user}, canonical.size(), validate, invalid);
    if (!cached || *cached != canonical || invalid)
        return fail("second launch did not load the imported bytes");

    write(portable, {0x80, 0x37, 0x12, 0x40, 0xa5, 0x5b});
    invalid = false;
    if (tetrisphere::read_rom_cache({portable}, canonical.size(), validate, invalid) ||
        !invalid)
        return fail("same-size corrupt cache was accepted or not reported");

    write(portable, {0x80, 0x37});
    invalid = false;
    if (tetrisphere::read_rom_cache({portable, user}, canonical.size(), validate, invalid) ||
        !invalid)
        return fail("truncated cache was accepted or not reported");

    std::filesystem::create_directories(user.parent_path());
    write(user, canonical);
    const auto fallback = tetrisphere::read_rom_cache(
        {portable, user}, canonical.size(), validate, invalid);
    if (!fallback || *fallback != canonical)
        return fail("valid user cache was ignored after portable corruption");

    const auto blocked = dir / "blocked";
    write(blocked, {1}); // A file cannot contain a portable cache directory.
    const auto unreachable = blocked / "data" / "tetrisphere-us-rev0" / "rom.z64";
    const auto writable = dir / "fallback" / "data" / "tetrisphere-us-rev0" / "rom.z64";
    if (!tetrisphere::write_rom_cache({unreachable, writable}, canonical, error) ||
        !std::filesystem::is_regular_file(writable))
        return fail("read-only portable location did not fall back to user data");
    write(writable.parent_path() / "rom.z64.tmp-interrupted", {0x80});
    invalid = false;
    const auto recovered = tetrisphere::read_rom_cache(
        {unreachable, writable}, canonical.size(), validate, invalid);
    if (!recovered || *recovered != canonical || invalid)
        return fail("temporary interrupted import affected the valid cache");
    return 0;
}
