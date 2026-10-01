#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace tetrisphere {

class EepromStore {
public:
    explicit EepromStore(std::filesystem::path path);
    bool load(std::array<std::uint8_t, 512>& bytes, std::string& error) const;
    bool save(const std::array<std::uint8_t, 512>& bytes, std::string& error) const;

private:
    std::filesystem::path path_;
};

} // namespace tetrisphere
