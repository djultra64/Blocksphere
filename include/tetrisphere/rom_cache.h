#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace tetrisphere {

using RomCacheValidator =
    std::function<bool(const std::vector<std::uint8_t>&)>;

// The application supplies only portable data beside the executable.
// A caller with TETRISPHERE_DATA_DIR set passes only its isolated data path.
std::optional<std::vector<std::uint8_t>> read_rom_cache(
    const std::vector<std::filesystem::path>& paths, std::size_t expected_size,
    const RomCacheValidator& validate, bool& invalid_seen);

// The caller must validate and normalize bytes before publishing them.
// Files are written to a sibling temporary path and renamed when complete.
bool write_rom_cache(const std::vector<std::filesystem::path>& paths,
                     const std::vector<std::uint8_t>& bytes, std::string& error);

} // namespace tetrisphere
