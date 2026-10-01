#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace tetrisphere {

struct RomInput {
    std::vector<std::uint8_t> bytes;
    std::string format;
    std::string error;
};

// Searches only this directory; returned bytes are normalized and validated.
std::optional<RomInput> discover_rom_input(
    const std::filesystem::path& root,
    const std::function<bool(const std::vector<std::uint8_t>&)>& validate);

RomInput read_rom_input(const std::filesystem::path& path);

// Invalid selections are reported before reopening the picker. Cancellation
// returns no ROM and never publishes bytes to a cache.
std::optional<RomInput> select_compatible_rom(
    std::filesystem::path initial,
    const std::function<bool(const std::vector<std::uint8_t>&)>& validate,
    const std::function<std::filesystem::path()>& select,
    const std::function<void(const std::string&)>& report_error);

} // namespace tetrisphere
