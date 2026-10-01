#include "tetrisphere/rom_cache.h"

#include <chrono>
#include <fstream>
#include <random>
#include <system_error>

namespace tetrisphere {
namespace {

std::optional<std::vector<std::uint8_t>> read_exact(
    const std::filesystem::path& path, std::size_t expected_size) {
    std::error_code ec;
    if (std::filesystem::file_size(path, ec) != expected_size || ec)
        return std::nullopt;
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    std::vector<std::uint8_t> bytes(expected_size);
    file.read(reinterpret_cast<char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (!file || file.gcount() != static_cast<std::streamsize>(bytes.size()))
        return std::nullopt;
    return bytes;
}

std::filesystem::path temporary_path(const std::filesystem::path& target) {
    std::random_device random;
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return target.parent_path() /
        (target.filename().string() + ".tmp-" + std::to_string(now) + "-" +
         std::to_string(random()));
}

bool publish_at(const std::filesystem::path& target,
                const std::vector<std::uint8_t>& bytes) {
    if (target.empty() || bytes.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) return false;
#ifndef _WIN32
    // The directory contains a full copy of the user's ROM.
    std::filesystem::permissions(target.parent_path(),
                                 std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, ec);
    if (ec) return false;
#endif
    const auto temp = temporary_path(target);
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file) return false;
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        file.flush();
        if (!file) {
            std::filesystem::remove(temp, ec);
            return false;
        }
    }
#ifndef _WIN32
    std::filesystem::permissions(
        temp, std::filesystem::perms::owner_read |
                  std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
#endif
    std::filesystem::rename(temp, target, ec);
    if (!ec) return true;

    // Windows cannot rename over an existing file. Preserve a corrupt old
    // cache until the complete replacement is ready, then restore on failure.
    ec.clear();
    if (!std::filesystem::is_regular_file(target, ec) || ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
    const auto backup = temporary_path(target).string() + ".old";
    ec.clear();
    std::filesystem::rename(target, backup, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
    std::filesystem::rename(temp, target, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::rename(backup, target, ignored);
        std::filesystem::remove(temp, ignored);
        return false;
    }
    std::filesystem::remove(backup, ec);
    return true;
}

} // namespace

std::optional<std::vector<std::uint8_t>> read_rom_cache(
    const std::vector<std::filesystem::path>& paths, std::size_t expected_size,
    const RomCacheValidator& validate, bool& invalid_seen) {
    invalid_seen = false;
    for (const auto& path : paths) {
        std::error_code ec;
        if (path.empty() || !std::filesystem::exists(path, ec)) continue;
        auto bytes = read_exact(path, expected_size);
        if (bytes && validate(*bytes)) return bytes;
        invalid_seen = true;
    }
    return std::nullopt;
}

bool write_rom_cache(const std::vector<std::filesystem::path>& paths,
                     const std::vector<std::uint8_t>& bytes, std::string& error) {
    error.clear();
    for (const auto& path : paths) {
        if (publish_at(path, bytes)) return true;
    }
    error = "rom_cache_unwritable";
    return false;
}

} // namespace tetrisphere
