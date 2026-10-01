#include "tetrisphere/eeprom_store.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

std::array<std::uint8_t, 512> image(std::uint8_t value) {
    std::array<std::uint8_t, 512> bytes{};
    bytes[0] = value;
    bytes[511] = static_cast<std::uint8_t>(value ^ 0x5a);
    return bytes;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return fail("expected private test directory");
    const std::filesystem::path dir = std::filesystem::path(argv[1]) /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(dir);
    const auto path = dir / "progress.eep";
    tetrisphere::EepromStore store(path);
    std::array<std::uint8_t, 512> loaded{};
    std::string error;
    if (!store.load(loaded, error) || loaded != std::array<std::uint8_t, 512>{}) {
        return fail("new EEPROM did not start blank");
    }
    if (!store.save(image(0x12), error) || !store.save(image(0x34), error)) {
        return fail("EEPROM commits failed");
    }
    if (!store.load(loaded, error) || loaded != image(0x34)) {
        return fail("latest complete EEPROM commit was not loaded");
    }
    // A process can die after writing the temp file. The last committed
    // primary must still be selected on restart.
    {
        std::ofstream temporary(path.string() + ".tmp", std::ios::binary);
        temporary << "partial";
    }
    if (!store.load(loaded, error) || loaded != image(0x34)) {
        return fail("interrupted temp write replaced the valid primary");
    }
    // A process can also die with a torn/corrupted primary while an older
    // backup is intact. Recovery must use that complete backup.
    {
        std::ofstream primary(path, std::ios::binary | std::ios::trunc);
        primary << "partial";
    }
    if (!store.load(loaded, error) || loaded != image(0x12)) {
        return fail("invalid primary did not recover the previous valid EEPROM");
    }
    {
        std::ofstream backup(path.string() + ".bak", std::ios::binary | std::ios::trunc);
        backup << "partial";
    }
    if (store.load(loaded, error) || error.empty()) {
        return fail("two invalid save copies were silently accepted");
    }
    return 0;
}
