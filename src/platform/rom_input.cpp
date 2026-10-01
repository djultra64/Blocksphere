#include "tetrisphere/rom_input.h"

#include "miniz.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <string_view>

namespace tetrisphere {
namespace {

constexpr std::size_t rom_size = 8u * 1024u * 1024u;
constexpr std::size_t max_zip_size = 32u * 1024u * 1024u;

std::string extension(std::string_view filename) {
    const auto dot = filename.find_last_of('.');
    std::string result = dot == std::string_view::npos ? "" : std::string(filename.substr(dot));
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

RomInput normalize(std::vector<std::uint8_t> bytes) {
    if (bytes.size() < 64) return {{}, {}, "truncated_rom"};
    const std::array<std::uint8_t, 4> header{bytes[0], bytes[1], bytes[2], bytes[3]};
    const std::array<std::uint8_t, 4> z64{0x80, 0x37, 0x12, 0x40};
    const std::array<std::uint8_t, 4> v64{0x37, 0x80, 0x40, 0x12};
    const std::array<std::uint8_t, 4> n64{0x40, 0x12, 0x37, 0x80};
    if (header != z64 && header != v64 && header != n64) {
        return {{}, {}, "invalid_header"};
    }
    if (bytes.size() != rom_size) {
        return {{}, {}, bytes.size() < rom_size ? "truncated_rom" : "unexpected_size"};
    }
    if (header == v64) {
        for (std::size_t i = 0; i < bytes.size(); i += 2) std::swap(bytes[i], bytes[i + 1]);
        return {std::move(bytes), "v64", {}};
    }
    if (header == n64) {
        for (std::size_t i = 0; i < bytes.size(); i += 4) {
            std::reverse(bytes.begin() + i, bytes.begin() + i + 4);
        }
        return {std::move(bytes), "n64", {}};
    }
    return {std::move(bytes), "z64", {}};
}

RomInput load_zip(const std::vector<std::uint8_t>& archive_bytes) {
    mz_zip_archive archive{};
    if (!mz_zip_reader_init_mem(&archive, archive_bytes.data(), archive_bytes.size(), 0)) {
        return {{}, {}, "invalid_archive"};
    }
    int selected = -1;
    std::string error;
    const auto count = mz_zip_reader_get_num_files(&archive);
    for (mz_uint index = 0; index < count; ++index) {
        mz_zip_archive_file_stat info{};
        if (!mz_zip_reader_file_stat(&archive, index, &info)) {
            error = "invalid_archive";
            break;
        }
        if (info.m_is_directory) continue;
        const auto ext = extension(info.m_filename);
        if (ext != ".z64" && ext != ".v64" && ext != ".n64") continue;
        if (selected >= 0) { error = "ambiguous_archive"; break; }
        if (info.m_is_encrypted) { error = "encrypted_archive"; break; }
        if (!info.m_is_supported) { error = "unsupported_archive_method"; break; }
        if (info.m_uncomp_size != rom_size) {
            error = info.m_uncomp_size < rom_size ? "truncated_rom" : "unexpected_size";
            break;
        }
        selected = static_cast<int>(index);
    }
    if (error.empty() && selected < 0) error = "no_rom_in_archive";
    RomInput result;
    if (error.empty()) {
        result.bytes.resize(rom_size);
        if (!mz_zip_reader_extract_to_mem(&archive, static_cast<mz_uint>(selected),
                                           result.bytes.data(), result.bytes.size(), 0)) {
            error = "invalid_archive";
        }
    }
    mz_zip_reader_end(&archive);
    if (!error.empty()) return {{}, {}, error};
    return normalize(std::move(result.bytes));
}

} // namespace

std::optional<RomInput> discover_rom_input(
    const std::filesystem::path& root,
    const std::function<bool(const std::vector<std::uint8_t>&)>& validate) {
    // Bound both directory work and ROM reads so a crowded package folder cannot
    // turn first launch into an unbounded scan. Never descend into subfolders.
    constexpr std::size_t max_entries = 512;
    constexpr std::size_t max_candidates = 16;
    std::vector<std::filesystem::path> candidates;
    std::error_code ec;
    std::filesystem::directory_iterator it(root, ec), end;
    for (std::size_t seen = 0; !ec && it != end && seen < max_entries;
         it.increment(ec), ++seen) {
        std::error_code file_error;
        if (!it->is_regular_file(file_error) || file_error) continue;
        const auto path = it->path();
        const auto ext = extension(path.extension().string());
        if (ext != ".z64" && ext != ".v64" && ext != ".n64" && ext != ".zip") continue;
        std::string name;
        for (const auto raw : path.stem().u8string()) {
            const auto c = static_cast<unsigned char>(raw);
            if (c < 128 && std::isalnum(c)) name += static_cast<char>(std::tolower(c));
        }
        if (name.find("tetrisphere") != std::string::npos) candidates.push_back(path);
    }
    std::sort(candidates.begin(), candidates.end());
    if (candidates.size() > max_candidates) candidates.resize(max_candidates);
    for (const auto& path : candidates) {
        auto input = read_rom_input(path);
        if (input.error.empty() && validate(input.bytes)) return input;
    }
    return std::nullopt;
}

std::optional<RomInput> select_compatible_rom(
    std::filesystem::path path,
    const std::function<bool(const std::vector<std::uint8_t>&)>& validate,
    const std::function<std::filesystem::path()>& select,
    const std::function<void(const std::string&)>& report_error) {
    for (;;) {
        if (path.empty()) path = select();
        if (path.empty()) return std::nullopt;
        auto input = read_rom_input(path);
        if (input.error.empty() && !validate(input.bytes))
            input.error = "unknown_revision_or_modified";
        if (input.error.empty()) return input;
        report_error(input.error);
        path.clear();
    }
}

RomInput read_rom_input(const std::filesystem::path& path) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) return {{}, {}, "io_error"};
    if (size > max_zip_size) return {{}, {}, "unexpected_size"};
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {{}, {}, "io_error"};
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
        return {{}, {}, "io_error"};
    }
    const bool zip_magic = bytes.size() >= 4 && bytes[0] == 'P' && bytes[1] == 'K';
    if (zip_magic || extension(path.extension().string()) == ".zip") {
        return load_zip(bytes);
    }
    return normalize(std::move(bytes));
}

} // namespace tetrisphere
