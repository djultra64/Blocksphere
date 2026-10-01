#include "tetrisphere/eeprom_store.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace tetrisphere {
namespace {

constexpr std::array<std::uint8_t, 4> magic{'T', 'S', 'E', 'P'};
constexpr std::uint32_t version = 1;
constexpr std::size_t header_size = 12;
constexpr std::size_t image_size = header_size + 512;

void put_u32(std::uint8_t* out, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) out[i] = static_cast<std::uint8_t>(value >> (i * 8));
}

std::uint32_t get_u32(const std::uint8_t* in) {
    return std::uint32_t{in[0]} | (std::uint32_t{in[1]} << 8) |
           (std::uint32_t{in[2]} << 16) | (std::uint32_t{in[3]} << 24);
}

std::uint32_t checksum(const std::uint8_t* bytes, std::size_t size) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & -(crc & 1u));
        }
    }
    return ~crc;
}

bool read_valid(const std::filesystem::path& path,
                std::array<std::uint8_t, 512>& bytes) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::array<std::uint8_t, image_size> image{};
    file.read(reinterpret_cast<char*>(image.data()), image.size());
    if (file.gcount() != static_cast<std::streamsize>(image.size()) ||
        file.peek() != std::char_traits<char>::eof() ||
        !std::equal(magic.begin(), magic.end(), image.begin()) ||
        get_u32(image.data() + 4) != version ||
        get_u32(image.data() + 8) != checksum(image.data() + header_size, bytes.size())) {
        return false;
    }
    std::copy(image.begin() + header_size, image.end(), bytes.begin());
    return true;
}

bool write_durable(const std::filesystem::path& path,
                   const std::array<std::uint8_t, image_size>& image) {
#ifdef _WIN32
    const int fd = _wopen(path.c_str(), _O_CREAT | _O_TRUNC | _O_WRONLY | _O_BINARY,
                          _S_IREAD | _S_IWRITE);
    if (fd < 0) return false;
    const bool ok = _write(fd, image.data(), static_cast<unsigned>(image.size())) ==
                        static_cast<int>(image.size()) && _commit(fd) == 0;
    return _close(fd) == 0 && ok;
#else
    const int fd = open(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd < 0) return false;
    std::size_t written = 0;
    while (written < image.size()) {
        const auto count = write(fd, image.data() + written, image.size() - written);
        if (count <= 0) break;
        written += static_cast<std::size_t>(count);
    }
    const bool ok = written == image.size() && fsync(fd) == 0;
    return close(fd) == 0 && ok;
#endif
}

bool sync_directory(const std::filesystem::path& directory) {
#ifdef _WIN32
    (void)directory;
    return true;
#else
    const int fd = open(directory.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) return false;
    const bool ok = fsync(fd) == 0;
    return close(fd) == 0 && ok;
#endif
}

} // namespace

EepromStore::EepromStore(std::filesystem::path path) : path_(std::move(path)) {}

bool EepromStore::load(std::array<std::uint8_t, 512>& bytes, std::string& error) const {
    error.clear();
    if (read_valid(path_, bytes)) return true;
    const auto backup = std::filesystem::path(path_.string() + ".bak");
    if (read_valid(backup, bytes)) return true;
    std::error_code ec;
    const bool primary_exists = std::filesystem::exists(path_, ec);
    if (ec) { error = "cannot inspect EEPROM save: " + ec.message(); return false; }
    const bool backup_exists = std::filesystem::exists(backup, ec);
    if (ec) { error = "cannot inspect EEPROM backup: " + ec.message(); return false; }
    if (primary_exists || backup_exists) {
        error = "EEPROM save and backup are invalid; restore a valid copy";
        return false;
    }
    bytes.fill(0);
    return true;
}

bool EepromStore::save(const std::array<std::uint8_t, 512>& bytes,
                       std::string& error) const {
    error.clear();
    std::array<std::uint8_t, 512> previous{};
    if (!load(previous, error)) return false;
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    if (ec) { error = "cannot create EEPROM directory: " + ec.message(); return false; }
    std::array<std::uint8_t, image_size> image{};
    std::copy(magic.begin(), magic.end(), image.begin());
    put_u32(image.data() + 4, version);
    put_u32(image.data() + 8, checksum(bytes.data(), bytes.size()));
    std::copy(bytes.begin(), bytes.end(), image.begin() + header_size);
    const auto temporary = std::filesystem::path(path_.string() + ".tmp");
    const auto backup = std::filesystem::path(path_.string() + ".bak");
    if (!write_durable(temporary, image)) {
        error = "cannot durably write EEPROM temporary file";
        return false;
    }
    const bool primary_exists = std::filesystem::exists(path_, ec);
    if (ec) { error = ec.message(); return false; }
    if (primary_exists) {
        std::array<std::uint8_t, 512> valid_primary{};
        if (read_valid(path_, valid_primary)) {
            std::filesystem::remove(backup, ec);
            if (ec) { error = ec.message(); return false; }
            std::filesystem::rename(path_, backup, ec);
            if (ec) { error = ec.message(); return false; }
        } else {
            std::filesystem::remove(path_, ec);
            if (ec) { error = ec.message(); return false; }
        }
    }
    if (!sync_directory(path_.parent_path())) {
        error = "cannot sync EEPROM directory before commit";
        return false;
    }
    std::filesystem::rename(temporary, path_, ec);
    if (ec || !sync_directory(path_.parent_path())) {
        error = ec ? ec.message() : "cannot sync EEPROM directory after commit";
        return false;
    }
    return true;
}

} // namespace tetrisphere
