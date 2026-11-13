#include "tetrisphere/launcher_backend.h"

#include "tetrisphere/rom_cache.h"
#include "tetrisphere/rom_identity.h"
#include "tetrisphere/rom_input.h"
#include "miniz.h"

#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <cwchar>
#include <exception>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string_view>
#include <thread>
#include <tuple>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#else
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace tetrisphere::launcher {
namespace {

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
constexpr std::string_view language_file = "data/language.txt";
constexpr std::uintmax_t config_limit = 16u * 1024u * 1024u;
constexpr std::uintmax_t artwork_limit = 32u * 1024u * 1024u;

std::string utf8(const fs::path& path) {
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
}

Result error(Error code, const fs::path& path) { return {code, utf8(path)}; }

std::string language_code(const std::string& language) {
    static constexpr std::array<std::string_view, 6> supported{"en", "es", "de", "fr", "it", "pt"};
    return std::find(supported.begin(), supported.end(), language) != supported.end() ? language : "en";
}

fs::path environment_path(const char* name) {
#ifdef _WIN32
    const std::wstring wide(name, name + std::char_traits<char>::length(name));
    const auto size = GetEnvironmentVariableW(wide.c_str(), nullptr, 0);
    if (size == 0) return {};
    std::wstring value(size, L'\0');
    const auto copied = GetEnvironmentVariableW(wide.c_str(), value.data(), size);
    if (copied == 0 || copied >= size) return {};
    value.resize(copied);
    return fs::path(value);
#else
    const auto* value = std::getenv(name);
    return value == nullptr ? fs::path{} : fs::u8path(value);
#endif
}

std::optional<std::string> read_file(const fs::path& path, std::uintmax_t limit = config_limit) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec || size > limit) return std::nullopt;
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;
    std::string bytes(static_cast<std::size_t>(size), '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!input || input.peek() != std::char_traits<char>::eof()) return std::nullopt;
    return bytes;
}

bool write_file(const fs::path& path, std::string_view bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    return static_cast<bool>(output);
}

bool regular_file(const fs::path& path) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    return !ec && size > 0 && fs::is_regular_file(path, ec) && !ec;
}

bool path_entry_exists(const fs::path& path, std::error_code& ec) {
    auto status = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory) ec.clear();
    return !ec && status.type() != fs::file_type::not_found;
}

bool safe_relative(const fs::path& path) {
    if (path.empty() || path.has_root_path()) return false;
    for (const auto& part : path) {
        if (part == ".." || part == "." || part.empty()) return false;
    }
    return true;
}

// The root is canonicalized by callers. Reject links in writable subpaths.
bool safe_subpath(const fs::path& root, const fs::path& relative) {
    if (!safe_relative(relative)) return false;
    auto path = root;
    for (const auto& component : relative) {
        path /= component;
        std::error_code ec;
        const auto status = fs::symlink_status(path, ec);
        if (ec == std::errc::no_such_file_or_directory) continue;
        if (ec || fs::is_symlink(status)) return false;
    }
    return true;
}

std::string unique_suffix() {
    return std::to_string(Clock::now().time_since_epoch().count()) + "-" +
           std::to_string(std::random_device{}());
}

class TemporaryDirectory {
public:
    TemporaryDirectory(const fs::path& parent, std::string_view prefix) {
        std::error_code ec;
        if (parent.empty()) return;
        fs::create_directories(parent, ec);
        if (ec) return;
        auto candidate = parent / (std::string(prefix) + unique_suffix());
        if (!fs::create_directory(candidate, ec) || ec) return;
        path_ = std::move(candidate);
#ifndef _WIN32
        fs::permissions(path_, fs::perms::owner_all, fs::perm_options::replace, ec);
        if (ec) cleanup();
#endif
    }
    ~TemporaryDirectory() { cleanup(); }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    const fs::path& path() const { return path_; }
    void preserve() { path_.clear(); }
private:
    void cleanup() {
        if (path_.empty()) return;
        std::error_code ignored;
        fs::remove_all(path_, ignored);
        path_.clear();
    }
    fs::path path_;
};

bool replace_file(const fs::path& source, const fs::path& destination) {
#ifdef _WIN32
    return MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code ec;
    fs::rename(source, destination, ec);
    return !ec;
#endif
}

bool atomic_text(const fs::path& destination, std::string_view bytes) {
    if (destination.empty()) return false;
    std::error_code ec;
    auto target = fs::absolute(destination, ec);
    if (ec) return false;
    const auto status = fs::symlink_status(target, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) return false;
    if (fs::exists(status) && !fs::is_regular_file(status)) return false;
    TemporaryDirectory stage(target.parent_path(), ".tetrisphere-write-");
    if (stage.path().empty()) return false;
    const auto temp = stage.path() / "new";
    return write_file(temp, bytes) && replace_file(temp, target);
}

bool files_equal(const fs::path& left, const fs::path& right) {
    std::error_code ec;
    const auto size = fs::file_size(left, ec);
    if (ec || fs::file_size(right, ec) != size || ec) return false;
    std::ifstream a(left, std::ios::binary), b(right, std::ios::binary);
    if (!a || !b) return false;
    std::array<char, 65536> a_buffer{}, b_buffer{};
    for (auto remaining = size; remaining != 0;) {
        const auto chunk = static_cast<std::streamsize>(std::min<std::uintmax_t>(remaining, a_buffer.size()));
        a.read(a_buffer.data(), chunk);
        b.read(b_buffer.data(), chunk);
        if (!a || !b || !std::equal(a_buffer.begin(), a_buffer.begin() + chunk, b_buffer.begin())) return false;
        remaining -= static_cast<std::uintmax_t>(chunk);
    }
    return a.peek() == std::char_traits<char>::eof() && b.peek() == std::char_traits<char>::eof();
}

bool native_executable(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::array<unsigned char, 4> magic{};
    file.read(reinterpret_cast<char*>(magic.data()), magic.size());
    if (!file) return false;
#ifdef _WIN32
    return magic[0] == 'M' && magic[1] == 'Z';
#else
    return magic == std::array<unsigned char, 4>{0x7f, 'E', 'L', 'F'} && access(path.c_str(), X_OK) == 0;
#endif
}

bool valid_rom(const std::vector<std::uint8_t>& bytes, const ProjectProfile& profile) {
    return bytes.size() == profile.rom_size && rom_sha256(bytes) == profile.rom_sha256;
}

bool package_files_complete(const fs::path& root, const ProjectProfile& profile) {
    return std::all_of(profile.required_files.begin(), profile.required_files.end(), [&](const auto& file) {
        return safe_relative(file) && regular_file(root / file);
    });
}

bool package_complete(const fs::path& root, const ProjectProfile& profile) {
    return safe_relative(profile.executable) && native_executable(root / profile.executable) &&
           package_files_complete(root, profile);
}

// Keep one open archive throughout installation. Only its exact executable
// member can be written, inside our transaction directory, never into the source.
class GamePayload {
public:
    ~GamePayload() { if (initialized_) mz_zip_reader_end(&archive_); }
    GamePayload() = default;
    GamePayload(const GamePayload&) = delete;
    GamePayload& operator=(const GamePayload&) = delete;

    bool open(const fs::path& path, const fs::path& executable) {
        constexpr std::uintmax_t limit = 256u * 1024u * 1024u;
        std::error_code ec;
        archive_bytes_ = fs::file_size(path, ec);
        if (ec || archive_bytes_ == 0 || archive_bytes_ > limit) return false;
        input_.open(path, std::ios::binary);
        if (!input_) return false;
        archive_.m_pIO_opaque = this;
        archive_.m_pRead = [](void* opaque, mz_uint64 offset, void* bytes, std::size_t size) -> std::size_t {
            auto& self = *static_cast<GamePayload*>(opaque);
            if (offset > self.archive_bytes_ || size > self.archive_bytes_ - offset) return 0;
            self.input_.clear();
            self.input_.seekg(static_cast<std::streamoff>(offset));
            self.input_.read(static_cast<char*>(bytes), static_cast<std::streamsize>(size));
            return static_cast<std::size_t>(self.input_.gcount());
        };
        initialized_ = mz_zip_reader_init(&archive_, archive_bytes_, 0);
        if (!initialized_ || mz_zip_reader_get_num_files(&archive_) != 1 ||
            !mz_zip_reader_file_stat(&archive_, 0, &entry_)) return false;
        const auto name = executable.generic_string();
        const auto type = (entry_.m_external_attr >> 16) & 0170000u;
        return mz_zip_reader_get_filename(&archive_, 0, nullptr, 0) == name.size() + 1 &&
            name == entry_.m_filename && !entry_.m_is_directory && !entry_.m_is_encrypted &&
            entry_.m_is_supported && (type == 0 || type == 0100000u) &&
            entry_.m_uncomp_size >= 4 && entry_.m_uncomp_size <= limit;
    }

    std::uintmax_t size() const { return entry_.m_uncomp_size; }

    bool extract(const fs::path& destination, std::uintmax_t& completed,
                 std::uintmax_t total, Clock::time_point& last_update,
                 const ProgressCallback& progress) {
        std::error_code ec;
        fs::create_directories(destination.parent_path(), ec);
        if (ec) return false;
        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        struct Extraction {
            std::ofstream& output;
            std::uintmax_t expected, written = 0;
            std::uintmax_t& completed;
            std::uintmax_t total;
            Clock::time_point& last_update;
            const ProgressCallback& progress;
            mz_ulong crc = MZ_CRC32_INIT;
        } state{output, size(), 0, completed, total, last_update, progress};
        const auto write = [](void* opaque, mz_uint64 offset, const void* bytes,
                              std::size_t size) -> std::size_t {
            auto& state = *static_cast<Extraction*>(opaque);
            if (offset != state.written || offset > state.expected || size > state.expected - offset) return 0;
            state.output.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(size));
            if (!state.output) return 0;
            state.crc = mz_crc32(state.crc, static_cast<const unsigned char*>(bytes), size);
            state.written += size;
            state.completed += size;
            const auto now = Clock::now();
            if (state.progress && (now - state.last_update >= 35ms || state.completed == state.total)) {
                state.progress({InstallStage::Copying, state.completed, state.total});
                state.last_update = now;
            }
            return size;
        };
        const bool extracted = mz_zip_reader_extract_to_callback(&archive_, 0, write, &state, 0);
        output.close();
        if (!extracted || !output || state.written != size() || state.crc != entry_.m_crc32) return false;
#ifndef _WIN32
        fs::permissions(destination, fs::perms::owner_all | fs::perms::group_read |
            fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec,
            fs::perm_options::replace, ec);
        if (ec) return false;
#endif
        return native_executable(destination);
    }

private:
    std::ifstream input_;
    mz_zip_archive archive_{};
    mz_zip_archive_file_stat entry_{};
    std::uintmax_t archive_bytes_ = 0;
    bool initialized_ = false;
};

bool cache_ready(const fs::path& root, const ProjectProfile& profile) {
    if (!safe_subpath(root, profile.rom_cache_relative)) return false;
    bool invalid_seen = false;
    return read_rom_cache({root / profile.rom_cache_relative}, profile.rom_size,
                          [&](const auto& bytes) { return valid_rom(bytes, profile); }, invalid_seen).has_value();
}

struct FileEdit {
    fs::path relative;
    std::optional<fs::path> replacement;
};

// Stage every replacement and backup before the first publish. A failed publish
// restores previously published files. Keep recoverable originals if rollback
// itself fails or the Steam transaction requests persistent backups.
Result publish_files(const fs::path& root, const std::vector<FileEdit>& edits,
                     Error error_code, bool keep_backup,
                     const std::function<bool()>& may_publish = {}) {
    TemporaryDirectory stage(root, ".tetrisphere-publish-");
    if (stage.path().empty()) return error(error_code, root);
    struct PreparedEdit {
        fs::path target, staged, backup;
        bool existed;
    };
    std::vector<PreparedEdit> prepared;
    std::error_code ec;
    for (const auto& edit : edits) {
        const auto target = root / edit.relative;
        if (!safe_subpath(root, edit.relative)) return error(error_code, target);
        const bool existed = path_entry_exists(target, ec);
        if (ec) return error(error_code, target);
        if (existed && (!fs::is_regular_file(target, ec) || ec)) return error(error_code, target);
        if (!edit.replacement && !existed) continue;
        if (edit.replacement && existed && files_equal(*edit.replacement, target)) continue;
        const auto staged = stage.path() / "new" / edit.relative;
        const auto backup = stage.path() / "backup" / edit.relative;
        if (existed) {
            fs::create_directories(backup.parent_path(), ec);
            if (ec || !fs::copy_file(target, backup, ec) || ec) return error(error_code, target);
        }
        if (edit.replacement) {
            fs::create_directories(staged.parent_path(), ec);
            if (ec || !fs::copy_file(*edit.replacement, staged, ec) || ec) return error(error_code, target);
            if (!files_equal(*edit.replacement, staged)) return error(error_code, target);
        }
        prepared.push_back({target, edit.replacement ? staged : fs::path{}, backup, existed});
    }
    if (prepared.empty()) return {};
    if (may_publish && !may_publish()) return {error_code, "Steam or a game started before the write"};
    fs::path persistent_backup;
    if (keep_backup && fs::is_directory(stage.path() / "backup", ec)) {
        const auto backups = root / "tetrisphere-backups";
        if (!safe_subpath(root, "tetrisphere-backups")) return error(error_code, backups);
        fs::create_directories(backups, ec);
        if (ec) return error(error_code, backups);
        persistent_backup = backups / unique_suffix();
        fs::copy(stage.path() / "backup", persistent_backup, fs::copy_options::recursive, ec);
        if (ec) return error(error_code, backups);
    }
    std::size_t published = 0;
    for (const auto& edit : prepared) {
        if (!safe_subpath(root, edit.target.lexically_relative(root))) break;
        fs::create_directories(edit.target.parent_path(), ec);
        if (ec) break;
        const bool ok = edit.staged.empty() ? fs::remove(edit.target, ec) && !ec
                                            : replace_file(edit.staged, edit.target);
        if (!ok) break;
        ++published;
    }
    if (published == prepared.size()) return {};
    const auto failed = prepared[published].target;
    bool restored = true;
    while (published > 0) {
        const auto& edit = prepared[--published];
        if (edit.existed) restored = replace_file(edit.backup, edit.target) && restored;
        else restored = fs::remove(edit.target, ec) && !ec && restored;
    }
    if (restored) return error(error_code, failed);
    const auto recovery = stage.path() / "backup";
    stage.preserve();
    return {error_code, utf8(failed) + " | Backup: " + utf8(recovery)};
}

bool copy_with_progress(const fs::path& source, const fs::path& destination,
                        std::uintmax_t& completed, std::uintmax_t total,
                        Clock::time_point& last_update, const ProgressCallback& progress) {
    std::error_code ec;
    fs::create_directories(destination.parent_path(), ec);
    if (ec) return false;
    std::ifstream input(source, std::ios::binary);
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!input || !output) return false;
    std::array<char, 65536> buffer{};
    while (input) {
        input.read(buffer.data(), buffer.size());
        const auto bytes = input.gcount();
        output.write(buffer.data(), bytes);
        if (!output) return false;
        completed += static_cast<std::uintmax_t>(bytes);
        const auto now = Clock::now();
        if (progress && (now - last_update >= 35ms || completed == total)) {
            progress({InstallStage::Copying, completed, total});
            last_update = now;
        }
    }
    output.close();
    if (!input.eof() || !output) return false;
#ifndef _WIN32
    const auto permissions = fs::status(source, ec).permissions();
    if (ec) return false;
    fs::permissions(destination, permissions & fs::perms::all, fs::perm_options::replace, ec);
    if (ec) return false;
#endif
    return true;
}

#ifdef _WIN32
std::wstring quote_arg(const std::wstring& value) {
    std::wstring quoted = L"\"";
    std::size_t slashes = 0;
    for (const auto c : value) {
        if (c == L'\\') { ++slashes; continue; }
        quoted.append(c == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        quoted += c;
    }
    quoted.append(slashes * 2, L'\\');
    return quoted + L'\"';
}
#endif

bool launch_process(const fs::path& executable, const std::vector<std::string>& arguments,
                    const fs::path& working_directory, const fs::path& data_root = {}) {
#ifdef _WIN32
    std::wstring command = quote_arg(executable.wstring());
    for (const auto& argument : arguments) command += L" " + quote_arg(fs::u8path(argument).wstring());
    std::vector<wchar_t> environment;
    if (!data_root.empty()) {
        auto* inherited = GetEnvironmentStringsW();
        if (!inherited) return false;
        constexpr auto key = L"TETRISPHERE_DATA_DIR=";
        for (auto* entry = inherited; *entry; entry += std::wcslen(entry) + 1) {
            if (_wcsnicmp(entry, key, std::wcslen(key)) != 0)
                environment.insert(environment.end(), entry, entry + std::wcslen(entry) + 1);
        }
        FreeEnvironmentStringsW(inherited);
        const auto value = std::wstring(key) + data_root.wstring();
        environment.insert(environment.end(), value.begin(), value.end());
        environment.insert(environment.end(), 2, L'\0');
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                        environment.empty() ? nullptr : environment.data(),
                        working_directory.empty() ? nullptr : working_directory.c_str(), &startup, &child)) return false;
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    return true;
#else
    std::vector<std::string> owned_arguments{utf8(executable)};
    owned_arguments.insert(owned_arguments.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    for (auto& argument : owned_arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    std::vector<char*> child_environment;
    auto entry = std::string("TETRISPHERE_DATA_DIR=") + utf8(data_root);
    if (!data_root.empty()) {
        for (char** item = environ; *item; ++item) {
            if (std::string_view(*item).rfind("TETRISPHERE_DATA_DIR=", 0) != 0) child_environment.push_back(*item);
        }
        child_environment.push_back(entry.data());
        child_environment.push_back(nullptr);
    }
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) return false;
    const auto chdir_status = working_directory.empty() ? 0 :
        posix_spawn_file_actions_addchdir_np(&actions, working_directory.c_str());
    pid_t pid = 0;
    const auto status = chdir_status != 0 ? chdir_status :
        posix_spawnp(&pid, executable.c_str(), &actions, nullptr, argv.data(),
                     child_environment.empty() ? environ : child_environment.data());
    posix_spawn_file_actions_destroy(&actions);
    if (status != 0) return false;
    std::thread([pid] {
        int status = 0;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    }).detach();
    return true;
#endif
}

} // namespace

ProjectProfile project_profile() {
    ProjectProfile profile;
    profile.display_name = "Blocksphere";
#ifdef _WIN32
    profile.executable = "tetrisphere.exe";
    profile.required_files = {"blocksphere-launcher.exe", "lib/SDL2.dll", "lib/dxcompiler.dll", "lib/dxil.dll"};
    profile.optional_files = {
        "tetrisphere-config.exe", "licenses/README.md", "licenses/DXC-LICENSE.TXT",
        "licenses/DXC-1.8.2505.1-LICENSE-LLVM.txt", "licenses/DXC-1.8.2505.1-LICENSE-MIT.txt",
        "licenses/DXC-1.8.2505.1-LICENSE-MS.txt", "licenses/JSON-3.12.0-LICENSE.MIT",
        "licenses/SDL2-2.26.3-LICENSE.txt", "licenses/SDL2-2.32.0-LICENSE.txt",
    };
#else
    profile.executable = "tetrisphere";
    profile.required_files = {"blocksphere-launcher", "lib/libSDL2-2.0.so.0"};
    profile.optional_files = {"tetrisphere-config"};
#endif
    profile.icon_relative = "launcher-assets/icon.png";
    profile.required_files.insert(profile.required_files.end(),
        {profile.icon_relative, "launcher-assets/Inter.ttf", "launcher-assets/OFL-Inter.txt"});
    profile.optional_files.insert(profile.optional_files.end(), {
        "gamecontrollerdb.txt", "README.md", "BUILD.json", "SHA256SUMS",
        "LICENSE", "RELEASE_NOTES.md", "THIRD_PARTY_NOTICES.md", "licenses/SDL2-LICENSE.txt",
        "licenses/JSON-LICENSE.MIT", "assets/README.md", "docs/BUILD.json", "docs/SHA256SUMS",
        "docs/CONTROLS.md", "docs/TROUBLESHOOTING.md", "docs/RELEASE-NOTES.md",
        "licenses/THIRD_PARTY_NOTICES.md", "licenses/controller-db/LICENSE",
        "licenses/controller-db/provenance.json",
    });
    profile.rom_size = supported_rom_size;
    profile.rom_sha256 = supported_rom_sha256;
    profile.rom_cache_relative = "data/tetrisphere-us-rev0/rom.z64";
    profile.github_repository = "djultra64/Blocksphere";
    return profile;
}

fs::path default_preferences_path() {
#ifdef _WIN32
    auto root = environment_path("LOCALAPPDATA");
#else
    auto root = environment_path("XDG_CONFIG_HOME");
    if (!root.is_absolute()) {
        root = environment_path("HOME");
        if (!root.empty()) root /= ".config";
    }
#endif
    return root.empty() ? fs::path{} : root / "Blocksphere" / "launcher.conf";
}

fs::path default_installation_path() {
#ifdef _WIN32
    const auto root = environment_path("LOCALAPPDATA");
    return root.empty() ? fs::path{} : root / "Programs" / "Blocksphere";
#else
    const auto root = environment_path("HOME");
    return root.empty() ? fs::path{} : root / "Games" / "Blocksphere";
#endif
}

Preferences load_preferences(const fs::path& path) {
    const auto bytes = read_file(path, 65536);
    if (!bytes) return {};
    std::istringstream input(*bytes);
    std::string version, language, destination;
    if (!(input >> version >> language >> std::quoted(destination)) || version != "blocksphere-launcher-1") return {};
    return {language_code(language), fs::u8path(destination)};
}

Result save_preferences(const fs::path& path, const Preferences& preferences) {
    std::ostringstream output;
    output << "blocksphere-launcher-1\n" << language_code(preferences.language) << '\n'
           << std::quoted(utf8(preferences.destination)) << '\n';
    return atomic_text(path, output.str()) ? Result{} : error(Error::PreferencesFailed, path);
}

RomSelection validate_rom(const fs::path& path, const ProjectProfile& profile) {
    auto input = read_rom_input(path);
    if (!input.error.empty()) {
        const auto code = input.error == "io_error" ? Error::RomUnreadable : Error::RomInvalid;
        return {path, {}, {code, input.error}};
    }
    if (!valid_rom(input.bytes, profile)) return {path, {}, {Error::RomUnsupported, {}}};
    return {path, std::move(input.bytes), {}};
}

bool installation_ready(const fs::path& root, const ProjectProfile& profile) {
    return !root.empty() && package_complete(root, profile) && cache_ready(root, profile);
}

Result apply_language(const fs::path& root, const std::string& language) {
    if (root.empty() || !safe_subpath(root, fs::path(language_file))) return error(Error::PreferencesFailed, root);
    const auto file = root / language_file;
    return atomic_text(file, language_code(language) + "\n") ? Result{} : error(Error::PreferencesFailed, file);
}

Result install(const fs::path& source_root, const fs::path& destination,
               const RomSelection& rom, const std::string& language,
               ProgressCallback progress, const ProjectProfile& profile) {
    if (!rom.result) return rom.result;
    if (!valid_rom(rom.bytes, profile)) return {Error::RomUnsupported, {}};
    if (!safe_relative(profile.executable) || !package_files_complete(source_root, profile))
        return error(Error::PackageIncomplete, source_root);
    if (!safe_relative(profile.rom_cache_relative) || destination.empty()) return error(Error::InvalidDestination, destination);
    std::error_code ec;
    const auto source = fs::canonical(source_root, ec);
    if (ec) return error(Error::PackageIncomplete, source_root);
    if (!safe_subpath(source, profile.installer_payload_relative)) return error(Error::PackageIncomplete, source);
    const auto payload_path = source / profile.installer_payload_relative;
    const bool packed = path_entry_exists(payload_path, ec);
    if (ec) return error(Error::PackageIncomplete, payload_path);
    GamePayload payload;
    if (packed ? !payload.open(payload_path, profile.executable) : !native_executable(source / profile.executable))
        return error(Error::PackageIncomplete, packed ? payload_path : source / profile.executable);
    auto target = fs::absolute(destination, ec).lexically_normal();
    if (ec || target == target.root_path()) return error(Error::InvalidDestination, destination);
    if (target.filename().empty()) target = target.parent_path();
    const bool in_place = fs::equivalent(source, target, ec) && !ec;
    ec.clear();
    if (in_place && packed) return error(Error::InvalidDestination, target);
    if (in_place) target = source;
    if (!in_place && path_entry_exists(target, ec)) {
        if (!fs::is_directory(fs::symlink_status(target, ec)) || ec) return error(Error::InvalidDestination, target);
        if (!fs::is_empty(target, ec) || ec) return error(Error::DestinationNotEmpty, target);
    }
    if (ec) return error(Error::InvalidDestination, target);
    TemporaryDirectory stage(in_place ? source : target.parent_path(), ".tetrisphere-install-");
    if (stage.path().empty()) return error(Error::CopyFailed, target);
    std::vector<fs::path> files;
    if (!in_place) {
        files = profile.required_files;
        if (!packed) files.push_back(profile.executable);
        for (const auto& relative : profile.optional_files) {
            if (relative != profile.installer_payload_relative && safe_relative(relative) &&
                regular_file(source / relative)) files.push_back(relative);
        }
        std::sort(files.begin(), files.end());
        files.erase(std::unique(files.begin(), files.end()), files.end());
        std::uintmax_t total = packed ? payload.size() : 0;
        for (const auto& relative : files) {
            total += fs::file_size(source / relative, ec);
            if (ec) return error(Error::CopyFailed, source / relative);
        }
        if (progress) progress({InstallStage::Copying, 0, total});
        std::uintmax_t copied = 0;
        auto last = Clock::now();
        for (const auto& relative : files) {
            if (!copy_with_progress(source / relative, stage.path() / relative, copied, total, last, progress))
                return error(Error::CopyFailed, relative);
        }
        if (packed && !payload.extract(stage.path() / profile.executable, copied, total, last, progress))
            return error(Error::PreparationFailed, payload_path);
    }
    if (progress) progress({InstallStage::Preparing});
    // Recompiled game code is shipped ahead of time. Preparing verifies the
    // portable artifact and publishes only the user's normalized local ROM.
    const auto game_root = in_place ? source : stage.path();
    if (!package_complete(game_root, profile)) return error(Error::PreparationFailed, game_root);
    for (const auto& relative : files) {
        if (!files_equal(source / relative, stage.path() / relative)) return error(Error::PreparationFailed, relative);
    }
    if (!apply_language(stage.path(), language)) return error(Error::PreparationFailed, stage.path() / language_file);
    std::string cache_error;
    const auto cache = stage.path() / profile.rom_cache_relative;
    if (!write_rom_cache({cache}, rom.bytes, cache_error) || !cache_ready(stage.path(), profile))
        return {Error::PreparationFailed, cache_error.empty() ? utf8(cache) : cache_error};
    if (progress) progress({InstallStage::Finalizing});
    if (in_place) {
        const auto result = publish_files(source, {{fs::path(language_file), stage.path() / language_file},
            {profile.rom_cache_relative, cache}}, Error::PreparationFailed, false);
        if (!result) return result;
    } else {
        const bool destination_exists = path_entry_exists(target, ec);
        if (ec) return error(Error::CopyFailed, target);
        if (!destination_exists) {
            fs::rename(stage.path(), target, ec);
            if (ec) return error(Error::CopyFailed, target);
        } else {
            // Retain the original directory until the replacement is published.
            // Recheck after moving it so concurrent files are never discarded.
            if (!fs::is_directory(fs::symlink_status(target, ec)) || ec || !fs::is_empty(target, ec) || ec)
                return error(Error::DestinationNotEmpty, target);
            TemporaryDirectory rollback(target.parent_path(), ".tetrisphere-rollback-");
            if (rollback.path().empty()) return error(Error::CopyFailed, target);
            const auto original = rollback.path() / "original";
            fs::rename(target, original, ec);
            if (ec) return error(Error::CopyFailed, target);
            const auto restore = [&](Error code) {
                std::error_code restore_error;
                fs::rename(original, target, restore_error);
                if (!restore_error) return error(code, target);
                rollback.preserve();
                return Result{code, utf8(target) + " | Backup: " + utf8(original)};
            };
            if (!fs::is_directory(fs::symlink_status(original, ec)) || ec || !fs::is_empty(original, ec) || ec)
                return restore(Error::DestinationNotEmpty);
            fs::rename(stage.path(), target, ec);
            if (ec) return restore(Error::CopyFailed);
        }
        stage.preserve();
    }
    if (progress) progress({InstallStage::Complete});
    return {};
}

Result play(const fs::path& root, const std::string& language, const ProjectProfile& profile) {
    if (!installation_ready(root, profile)) return error(Error::NotInstalled, root);
    std::error_code ec;
    const auto installed = fs::canonical(root, ec);
    if (ec) return error(Error::NotInstalled, root);
    if (const auto result = apply_language(installed, language); !result) return result;
    return launch_process(installed / profile.executable, {}, installed, installed / "data") ?
        Result{} : error(Error::LaunchFailed, installed / profile.executable);
}

namespace {

struct SteamLocation {
    fs::path root;
    SteamInstallation installation;
};

std::string lower_ascii(std::string_view value) {
    std::string lowered(value);
    for (auto& character : lowered) {
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
    }
    return lowered;
}

std::optional<std::uint64_t> unsigned_number(std::string_view value) {
    std::uint64_t number = 0;
    if (value.empty()) return std::nullopt;
    const auto end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(), end, number);
    if (parsed.ec != std::errc{} || parsed.ptr != end) return std::nullopt;
    return number;
}

std::vector<SteamLocation> steam_locations() {
    std::vector<SteamLocation> candidates;
#ifdef _WIN32
    std::array<wchar_t, 32768> path{};
    DWORD bytes = static_cast<DWORD>(path.size() * sizeof(wchar_t));
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ,
                    nullptr, path.data(), &bytes) == ERROR_SUCCESS && path[0] != 0)
        candidates.push_back({fs::path(path.data()), SteamInstallation::Windows});
    for (const auto* variable : {"ProgramFiles(x86)", "ProgramFiles"}) {
        const auto folder = environment_path(variable);
        if (!folder.empty()) candidates.push_back({folder / "Steam", SteamInstallation::Windows});
    }
#else
    const auto home = environment_path("HOME");
    if (home.is_absolute()) {
        candidates.push_back({home / ".local/share/Steam", SteamInstallation::Native});
        candidates.push_back({home / ".steam/steam", SteamInstallation::Native});
        candidates.push_back({home / ".steam/root", SteamInstallation::Native});
        candidates.push_back({home / ".var/app/com.valvesoftware.Steam/.local/share/Steam", SteamInstallation::Flatpak});
        candidates.push_back({home / ".var/app/com.valvesoftware.Steam/data/Steam", SteamInstallation::Flatpak});
    }
    const auto xdg_data = environment_path("XDG_DATA_HOME");
    if (xdg_data.is_absolute()) candidates.push_back({xdg_data / "Steam", SteamInstallation::Native});
#endif
    std::set<fs::path> seen;
    std::vector<SteamLocation> locations;
    for (auto& candidate : candidates) {
        std::error_code ec;
        auto root = fs::canonical(candidate.root, ec);
        if (ec || !regular_file(root / "config/loginusers.vdf") || !fs::is_directory(root / "userdata", ec) || ec) continue;
        if (seen.insert(root).second) locations.push_back({std::move(root), candidate.installation});
    }
    return locations;
}

class LoginUsersReader {
public:
    explicit LoginUsersReader(std::string_view bytes) : bytes_(bytes) {
        if (bytes_.substr(0, 3) == "\xef\xbb\xbf") position_ = 3;
    }

    std::vector<SteamAccount> accounts(const SteamLocation& location) {
        if (const auto root = next(); root.kind != Kind::String || lower_ascii(root.value) != "users") return {};
        if (next().kind != Kind::Open) return {};
        std::vector<SteamAccount> result;
        for (;;) {
            auto id = next();
            if (id.kind == Kind::Close) break;
            if (id.kind != Kind::String || next().kind != Kind::Open) return {};
            std::string account_name, persona_name;
            for (;;) {
                auto key = next();
                if (key.kind == Kind::Close) break;
                if (key.kind != Kind::String) return {};
                auto value = next();
                if (value.kind == Kind::Open) {
                    if (!skip_object(0)) return {};
                } else if (value.kind == Kind::String) {
                    const auto field = lower_ascii(key.value);
                    if (field == "personaname") persona_name = std::move(value.value);
                    if (field == "accountname") account_name = std::move(value.value);
                } else return {};
            }
            constexpr std::uint64_t steam_id_base = 76561197960265728ull;
            const auto steam_id = unsigned_number(id.value);
            if (!steam_id || *steam_id <= steam_id_base || *steam_id - steam_id_base > UINT32_MAX) continue;
            const auto account_id = std::to_string(*steam_id - steam_id_base);
            const auto relative = fs::path("userdata") / account_id;
            std::error_code ec;
            if (!safe_subpath(location.root, relative) || !fs::is_directory(location.root / relative, ec) || ec) continue;
            if (persona_name.empty()) persona_name = account_name.empty() ? account_id : account_name;
            result.push_back({location.root, account_id, std::move(persona_name), location.installation});
        }
        if (next().kind != Kind::End) return {};
        return result;
    }
private:
    enum class Kind { String, Open, Close, End, Invalid };
    struct Token { Kind kind; std::string value; };

    Token next() {
        while (position_ < bytes_.size()) {
            const auto character = bytes_[position_];
            if (character == ' ' || character == '\t' || character == '\r' || character == '\n') {
                ++position_;
            } else if (bytes_.substr(position_, 2) == "//") {
                const auto end = bytes_.find('\n', position_ + 2);
                position_ = end == std::string_view::npos ? bytes_.size() : end + 1;
            } else break;
        }
        if (position_ == bytes_.size()) return {Kind::End, {}};
        auto character = bytes_[position_++];
        if (character == '{') return {Kind::Open, {}};
        if (character == '}') return {Kind::Close, {}};
        if (character != '"') return {Kind::Invalid, {}};
        std::string value;
        while (position_ < bytes_.size()) {
            character = bytes_[position_++];
            if (character == '"') return {Kind::String, std::move(value)};
            if (character == '\0') return {Kind::Invalid, {}};
            if (character == '\\' && position_ < bytes_.size() &&
                (bytes_[position_] == '"' || bytes_[position_] == '\\')) character = bytes_[position_++];
            value += character;
        }
        return {Kind::Invalid, {}};
    }

    bool skip_object(unsigned depth) {
        if (depth >= 16) return false;
        for (;;) {
            auto key = next();
            if (key.kind == Kind::Close) return true;
            if (key.kind != Kind::String) return false;
            const auto value = next();
            if (value.kind == Kind::Open && skip_object(depth + 1)) continue;
            if (value.kind != Kind::String) return false;
        }
    }
    std::string_view bytes_;
    std::size_t position_ = 0;
};

bool same_path(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    if (fs::equivalent(a, b, ec) && !ec) return true;
#ifdef _WIN32
    const auto left = a.lexically_normal().wstring();
    const auto right = b.lexically_normal().wstring();
    return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
    return a.lexically_normal() == b.lexically_normal();
#endif
}

bool within_path(const fs::path& candidate, const fs::path& root) {
    if (candidate.empty()) return false;
    auto component = candidate.begin();
    for (const auto& part : root) {
        if (component == candidate.end()) return false;
#ifdef _WIN32
        if (CompareStringOrdinal(component->c_str(), -1, part.c_str(), -1, TRUE) != CSTR_EQUAL) return false;
#else
        if (*component != part) return false;
#endif
        ++component;
    }
    return true;
}

struct Process {
    std::uint64_t id, parent;
    std::string name;
    fs::path executable;
};

std::optional<std::vector<Process>> process_list() {
    std::vector<Process> processes;
#ifdef _WIN32
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return std::nullopt;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot, &entry)) { CloseHandle(snapshot); return std::nullopt; }
    do {
        fs::path executable;
        const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        if (process) {
            std::wstring path(32768, L'\0');
            DWORD length = static_cast<DWORD>(path.size());
            if (QueryFullProcessImageNameW(process, 0, path.data(), &length)) {
                path.resize(length);
                executable = path;
            }
            CloseHandle(process);
        }
        processes.push_back({entry.th32ProcessID, entry.th32ParentProcessID,
                             lower_ascii(utf8(fs::path(entry.szExeFile))), std::move(executable)});
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
#else
    std::error_code ec;
    fs::directory_iterator iterator("/proc", ec), end;
    if (ec) return std::nullopt;
    for (; iterator != end; iterator.increment(ec)) {
        if (ec) return std::nullopt;
        const auto id = unsigned_number(utf8(iterator->path().filename()));
        if (!id) continue;
        std::ifstream stat(iterator->path() / "stat");
        std::string line;
        if (!std::getline(stat, line)) continue;
        const auto open = line.find('('), close = line.rfind(')');
        if (open == std::string::npos || close == std::string::npos || close <= open) continue;
        std::istringstream fields(line.substr(close + 1));
        char state = 0;
        std::uint64_t parent = 0;
        if (!(fields >> state >> parent) || state == 'Z' || state == 'X') continue;
        std::error_code link_error;
        auto executable = fs::read_symlink(iterator->path() / "exe", link_error);
        auto name = lower_ascii(std::string_view(line).substr(open + 1, close - open - 1));
        processes.push_back({*id, parent, std::move(name), std::move(executable)});
    }
    if (ec) return std::nullopt;
#endif
    return processes;
}

struct SteamStatus {
    bool inspected = false;
    bool primary_running = false;
    bool helper_running = false;
    bool other_steam = false;
    bool game_running = false;
};

bool steam_primary(std::string_view name) { return name == "steam" || name == "steam.exe"; }
bool steam_primary_process(const Process& process) {
    if (!steam_primary(process.name)) return false;
    // A shell script invoked as `steam` has that comm while the real binary
    // starts or shuts down. The shell itself is not a second Steam instance.
    return process.executable.empty() || steam_primary(lower_ascii(utf8(process.executable.filename())));
}
bool steam_helper(std::string_view name) { return name == "steamwebhelper" || name == "steamwebhelper.exe"; }

SteamStatus inspect_steam(const SteamAccount& account, const fs::path& game) {
    SteamStatus status;
    const auto processes = process_list();
    if (!processes) return status;
    status.inspected = true;
    const auto game_name = lower_ascii(utf8(game.filename()));
    std::set<std::uint64_t> steam_pids;
    std::map<std::uint64_t, const Process*> by_id;
    for (const auto& process : *processes) {
        by_id.emplace(process.id, &process);
        if (process.name == game_name || (!process.executable.empty() && same_path(process.executable, game)))
            status.game_running = true;
        if (steam_primary_process(process)) {
            if (within_path(process.executable, account.steam_root)) {
                status.primary_running = true;
                steam_pids.insert(process.id);
            } else status.other_steam = true;
        }
        if (steam_helper(process.name) && within_path(process.executable, account.steam_root))
            status.helper_running = true;
    }
    // Steam on Linux starts Chromium inside runtime wrappers such as srt-bwrap.
    // Recognize those wrappers through the web helper they actually launched.
    std::set<std::uint64_t> ui_wrappers;
    for (const auto& process : *processes) {
        if (!steam_helper(process.name)) continue;
        std::vector<std::uint64_t> chain;
        auto parent = process.parent;
        for (unsigned depth = 0; parent != 0 && depth < 64; ++depth) {
            if (steam_pids.count(parent)) {
                ui_wrappers.insert(chain.begin(), chain.end());
                status.helper_running = true;
                break;
            }
            const auto found = by_id.find(parent);
            if (found == by_id.end() || found->second->parent == parent) break;
            chain.push_back(parent);
            parent = found->second->parent;
        }
    }
    // Unknown descendants can be games. Chromium renderers and the identified
    // UI wrappers belong to Steam itself, even if they use its container runtime.
    for (const auto& process : *processes) {
        if (ui_wrappers.count(process.id) || steam_primary(process.name) || steam_helper(process.name) ||
            process.name == "steamservice.exe" || process.name == "steamerrorreporter" ||
            process.name == "steamerrorreporter.exe" || process.name == "crashpad_handler" ||
            process.name == "steam-runtime-l" || process.name == "steam-runtime-launcher-service") continue;
        auto parent = process.parent;
        for (unsigned depth = 0; parent != 0 && depth < 64; ++depth) {
            if (steam_pids.count(parent)) { status.game_running = true; break; }
            const auto found = by_id.find(parent);
            if (found == by_id.end() || steam_helper(found->second->name) || found->second->parent == parent) break;
            parent = found->second->parent;
        }
    }
    return status;
}

bool launch_steam(const SteamAccount& account, bool shutdown) {
    std::vector<std::string> arguments;
    fs::path executable;
#ifdef _WIN32
    executable = account.steam_root / "steam.exe";
#else
    if (account.installation == SteamInstallation::Flatpak) {
        executable = "flatpak";
        arguments = {"run", "com.valvesoftware.Steam"};
    } else executable = "steam";
#endif
    if (shutdown) arguments.push_back("-shutdown");
    return launch_process(executable, arguments, account.steam_root);
}

bool wait_for_steam(const SteamAccount& account, const fs::path& game, bool running) {
    const auto deadline = Clock::now() + (running ? 15s : 20s);
    unsigned consecutive = 0;
    do {
        const auto status = inspect_steam(account, game);
        if (!status.inspected || status.other_steam) return false;
        const bool expected = running ? status.primary_running : (!status.primary_running && !status.helper_running);
        consecutive = expected ? consecutive + 1 : 0;
        if (consecutive >= 2) return true;
        std::this_thread::sleep_for(200ms);
    } while (Clock::now() < deadline);
    return false;
}

struct Shortcut {
    std::uint64_t index = 0;
    std::uint32_t app_id = 0;
    std::string name, executable, icon;
    std::size_t icon_begin = 0, icon_end = 0, object_end = 0;
};

class ShortcutReader {
public:
    explicit ShortcutReader(std::string_view bytes) : bytes_(bytes) {}

    bool parse(std::vector<Shortcut>& shortcuts, std::size_t& root_end) {
        std::string_view key;
        if (byte() != 0 || !text(key) || lower_ascii(key) != "shortcuts") return false;
        std::set<std::uint64_t> indexes;
        while (peek() != 8) {
            if (byte() != 0 || !text(key)) return false;
            const auto index = unsigned_number(key);
            if (!index || !indexes.insert(*index).second) return false;
            Shortcut shortcut;
            shortcut.index = *index;
            if (!object(0, &shortcut)) return false;
            shortcuts.push_back(std::move(shortcut));
        }
        root_end = position_++;
        if (peek() == 8) ++position_;
        return position_ == bytes_.size();
    }
private:
    int peek() const { return position_ < bytes_.size() ? static_cast<unsigned char>(bytes_[position_]) : -1; }
    int byte() { const auto value = peek(); if (value >= 0) ++position_; return value; }
    bool text(std::string_view& value) {
        const auto end = bytes_.find('\0', position_);
        if (end == std::string_view::npos) return false;
        value = bytes_.substr(position_, end - position_);
        position_ = end + 1;
        return true;
    }
    bool object(unsigned depth, Shortcut* shortcut = nullptr) {
        if (depth >= 32) return false;
        for (;;) {
            const auto type = byte();
            if (type == 8) {
                if (shortcut) shortcut->object_end = position_ - 1;
                return true;
            }
            if (type < 0) return false;
            std::string_view key;
            if (!text(key)) return false;
            if (type == 0) {
                if (!object(depth + 1)) return false;
            } else if (type == 1) {
                const auto begin = position_;
                std::string_view value;
                if (!text(value)) return false;
                if (!shortcut) continue;
                const auto field = lower_ascii(key);
                if (field == "appname") shortcut->name = value;
                if (field == "exe") shortcut->executable = value;
                if (field == "icon") {
                    shortcut->icon = value;
                    shortcut->icon_begin = begin;
                    shortcut->icon_end = position_ - 1;
                }
            } else if (type == 2 || type == 3 || type == 4 || type == 6 || type == 7) {
                const std::size_t length = type == 7 ? 8 : 4;
                if (bytes_.size() - position_ < length) return false;
                if (shortcut && type == 2 && lower_ascii(key) == "appid") {
                    for (unsigned i = 0; i < 4; ++i)
                        shortcut->app_id |= std::uint32_t(static_cast<unsigned char>(bytes_[position_ + i])) << (8 * i);
                }
                position_ += length;
            } else return false;
        }
    }
    std::string_view bytes_;
    std::size_t position_ = 0;
};

void vdf_string(std::string& bytes, std::string_view value) { bytes.append(value); bytes.push_back('\0'); }
void vdf_field(std::string& bytes, std::string_view key, std::string_view value) {
    bytes.push_back(1);
    vdf_string(bytes, key);
    vdf_string(bytes, value);
}
void vdf_field(std::string& bytes, std::string_view key, std::uint32_t value) {
    bytes.push_back(2);
    vdf_string(bytes, key);
    for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<char>((value >> (8 * i)) & 0xffu));
}

std::uint32_t shortcut_id(std::string_view executable, std::string_view name) {
    auto crc = mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const unsigned char*>(executable.data()), executable.size());
    crc = mz_crc32(crc, reinterpret_cast<const unsigned char*>(name.data()), name.size());
    return static_cast<std::uint32_t>(crc) | 0x80000000u;
}

std::string shortcut_bytes(std::uint64_t index, std::uint32_t id, std::string_view name,
                           std::string_view game, std::string_view root, std::string_view icon) {
    std::string bytes(1, '\0');
    vdf_string(bytes, std::to_string(index));
    vdf_field(bytes, "appid", id);
    vdf_field(bytes, "AppName", name);
    vdf_field(bytes, "Exe", game);
    vdf_field(bytes, "StartDir", root);
    vdf_field(bytes, "icon", icon);
    vdf_field(bytes, "ShortcutPath", std::string_view{});
    vdf_field(bytes, "LaunchOptions", std::string_view{});
    vdf_field(bytes, "IsHidden", 0u);
    vdf_field(bytes, "AllowDesktopConfig", 1u);
    vdf_field(bytes, "AllowOverlay", 1u);
    vdf_field(bytes, "OpenVR", 0u);
    vdf_field(bytes, "Devkit", 0u);
    vdf_field(bytes, "DevkitGameID", std::string_view{});
    vdf_field(bytes, "DevkitOverrideAppID", 0u);
    vdf_field(bytes, "LastPlayTime", 0u);
    vdf_field(bytes, "FlatpakAppID", std::string_view{});
    bytes.push_back(0);
    vdf_string(bytes, "tags");
    bytes.append(2, '\x08');
    return bytes;
}

std::string_view unquote(std::string_view path) {
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') return path.substr(1, path.size() - 2);
    return path;
}

struct PreparedArtwork {
    std::string suffix;
    std::string extension;
    fs::path source;
};

std::optional<std::string> image_extension(std::string_view bytes) {
    const bool png = bytes.substr(0, 8) == "\x89PNG\r\n\x1a\n";
    const bool jpeg = bytes.substr(0, 3) == "\xff\xd8\xff";
    if (!png && !jpeg) return std::nullopt;
    const auto* data = reinterpret_cast<const stbi_uc*>(bytes.data());
    const auto length = static_cast<int>(bytes.size());
    int width = 0, height = 0, channels = 0;
    if (!stbi_info_from_memory(data, length, &width, &height, &channels) ||
        width <= 0 || height <= 0 || width > 8192 || height > 8192 ||
        std::uint64_t(width) * std::uint64_t(height) > 32u * 1024u * 1024u) return std::nullopt;
    auto* pixels = stbi_load_from_memory(data, length, &width, &height, &channels, 0);
    if (!pixels) return std::nullopt;
    stbi_image_free(pixels);
    return png ? ".png" : ".jpg";
}

Result prepare_artwork(const ArtworkChoice& choice, const fs::path& fallback_icon,
                       const fs::path& staging, std::vector<PreparedArtwork>& prepared) {
    const std::array<std::pair<std::string_view, fs::path>, 5> categories{{
        {"p", choice.portrait}, {"", choice.landscape}, {"_hero", choice.hero}, {"_logo", choice.logo},
        {"_icon", choice.icon.empty() ? fallback_icon : choice.icon},
    }};
    for (std::size_t index = 0; index < categories.size(); ++index) {
        const auto& [suffix, path] = categories[index];
        const auto& downloaded = choice.downloaded[index];
        if (path.empty() && downloaded.empty()) continue;
        const auto local = downloaded.empty() ? read_file(path, artwork_limit) : std::nullopt;
        const std::string_view bytes = local ? std::string_view(*local) : std::string_view(downloaded);
        const auto extension = bytes.size() <= artwork_limit ? image_extension(bytes) : std::nullopt;
        if (!extension) return error(Error::ArtworkInvalid, path);
        const auto file = staging / ("asset" + std::string(suffix) + *extension);
        if (!write_file(file, bytes)) return error(Error::ArtworkInvalid, path);
        prepared.push_back({std::string(suffix), *extension, file});
    }
    return {};
}

class SteamWriteLock {
public:
    explicit SteamWriteLock(fs::path config) : path_(std::move(config) / ".blocksphere-steam.lock") {
        std::error_code ec;
        acquired_ = fs::create_directory(path_, ec) && !ec;
    }
    ~SteamWriteLock() { if (acquired_) { std::error_code ignored; fs::remove(path_, ignored); } }
    bool acquired() const { return acquired_; }
private:
    fs::path path_;
    bool acquired_ = false;
};

} // namespace

std::vector<SteamAccount> discover_steam_accounts() {
    std::vector<SteamAccount> accounts;
    for (const auto& location : steam_locations()) {
        const auto users = read_file(location.root / "config/loginusers.vdf");
        if (!users) continue;
        for (auto& account : LoginUsersReader(*users).accounts(location)) accounts.push_back(std::move(account));
    }
    std::sort(accounts.begin(), accounts.end(), [](const auto& a, const auto& b) {
        return std::tie(a.name, a.account_id, a.steam_root) < std::tie(b.name, b.account_id, b.steam_root);
    });
    return accounts;
}

Result add_to_steam(const SteamAccount& account, const fs::path& installed_root,
                    const ArtworkChoice& artwork, const ProjectProfile& profile) {
    if (!installation_ready(installed_root, profile)) return error(Error::NotInstalled, installed_root);
    const auto accounts = discover_steam_accounts();
    const auto selected = std::find_if(accounts.begin(), accounts.end(), [&](const auto& current) {
        return current.account_id == account.account_id && current.installation == account.installation &&
               same_path(current.steam_root, account.steam_root);
    });
    if (selected == accounts.end()) return error(Error::SteamUnavailable, account.steam_root);
    const auto account_config = fs::path("userdata") / selected->account_id / "config";
    const auto config = selected->steam_root / account_config;
    if (!safe_subpath(selected->steam_root, account_config / "shortcuts.vdf") ||
        !safe_subpath(selected->steam_root, account_config / "grid")) return error(Error::SteamUnavailable, config);
    std::error_code ec;
    const auto root = fs::canonical(installed_root, ec);
    if (ec) return error(Error::NotInstalled, installed_root);
    const auto game = root / profile.executable;
    const auto game_text = utf8(game), root_text = utf8(root);
    if (game_text.find('"') != std::string::npos || root_text.find('"') != std::string::npos)
        return error(Error::SteamUnavailable, root);
    const auto stage_root = fs::temp_directory_path(ec);
    if (ec) return error(Error::ArtworkInvalid, installed_root);
    TemporaryDirectory stage(stage_root, "blocksphere-steam-");
    if (stage.path().empty()) return error(Error::ArtworkInvalid, stage_root);
    std::vector<PreparedArtwork> prepared_art;
    const auto fallback = safe_relative(profile.icon_relative) && regular_file(root / profile.icon_relative)
        ? root / profile.icon_relative : fs::path{};
    if (const auto prepared = prepare_artwork(artwork, fallback, stage.path(), prepared_art); !prepared) return prepared;

    const auto initial = inspect_steam(*selected, game);
    if (!initial.inspected || initial.other_steam) return error(Error::SteamUnavailable, config);
    if (initial.game_running) return error(Error::SteamGameRunning, game);
    if (initial.helper_running && !initial.primary_running) return error(Error::SteamShutdownFailed, config);
    const bool reopen = initial.primary_running;
    if (reopen && (!launch_steam(*selected, true) || !wait_for_steam(*selected, game, false)))
        return error(Error::SteamShutdownFailed, config);

    auto write = [&]() -> Result {
        const auto current = inspect_steam(*selected, game);
        if (!current.inspected || current.other_steam || current.primary_running || current.helper_running)
            return error(Error::SteamShutdownFailed, config);
        if (current.game_running) return error(Error::SteamGameRunning, game);
        fs::create_directories(config, ec);
        if (ec) return error(Error::SteamWriteFailed, config);
        SteamWriteLock lock(config);
        if (!lock.acquired()) return error(Error::SteamWriteFailed, config);
        const auto shortcut_file = config / "shortcuts.vdf";
        std::string bytes("\0shortcuts\0\x08\x08", 13);
        if (path_entry_exists(shortcut_file, ec)) {
            const auto existing = read_file(shortcut_file);
            if (!existing) return error(Error::SteamShortcutInvalid, shortcut_file);
            bytes = *existing;
        }
        if (ec) return error(Error::SteamShortcutInvalid, shortcut_file);
        std::vector<Shortcut> shortcuts;
        std::size_t root_end = 0;
        if (!ShortcutReader(bytes).parse(shortcuts, root_end)) return error(Error::SteamShortcutInvalid, shortcut_file);
        const auto existing = std::find_if(shortcuts.begin(), shortcuts.end(), [&](const auto& shortcut) {
            const auto stored = unquote(shortcut.executable);
            return !stored.empty() && same_path(fs::u8path(stored), game);
        });
        const auto quoted_game = '"' + game_text + '"';
        const auto quoted_root = '"' + root_text + '"';
        const bool new_shortcut = existing == shortcuts.end();
        const auto app_id = new_shortcut ? shortcut_id(quoted_game, profile.display_name) :
            (existing->app_id == 0 ? shortcut_id(existing->executable, existing->name) : existing->app_id | 0x80000000u);
        for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
            if (it == existing) continue;
            const auto other_id = it->app_id == 0 ? shortcut_id(it->executable, it->name) : it->app_id | 0x80000000u;
            if (app_id == other_id) return error(Error::SteamShortcutInvalid, shortcut_file);
        }
        const auto id = std::to_string(app_id);
        std::string icon;
        std::vector<FileEdit> edits;
        for (const auto& asset : prepared_art) {
            if (asset.suffix == "_icon" && artwork.icon.empty() && artwork.downloaded[4].empty() &&
                !new_shortcut && !existing->icon.empty()) continue;
            const auto base = id + asset.suffix;
            const auto path = fs::path("grid") / (base + asset.extension);
            edits.push_back({path, asset.source});
            // Steam may prefer an older image with a different extension.
            for (const std::string_view extension : {".png", ".jpg", ".jpeg"}) {
                if (extension != asset.extension) edits.push_back({fs::path("grid") / (base + std::string(extension)), std::nullopt});
            }
            if (asset.suffix == "_icon") icon = utf8(config / path);
        }
        if (new_shortcut) {
            std::set<std::uint64_t> used;
            for (const auto& shortcut : shortcuts) used.insert(shortcut.index);
            std::uint64_t index = 0;
            while (used.count(index)) ++index;
            bytes.insert(root_end, shortcut_bytes(index, app_id, profile.display_name, quoted_game, quoted_root, icon));
        } else if (!icon.empty() && existing->icon != icon) {
            if (existing->icon_begin != 0) bytes.replace(existing->icon_begin, existing->icon_end - existing->icon_begin, icon);
            else {
                std::string field;
                vdf_field(field, "icon", icon);
                bytes.insert(existing->object_end, field);
            }
        }
        const auto updated = stage.path() / "shortcuts.vdf";
        if (!write_file(updated, bytes)) return error(Error::SteamWriteFailed, shortcut_file);
        edits.push_back({"shortcuts.vdf", updated});
        return publish_files(config, edits, Error::SteamWriteFailed, true, [&]() {
            const auto status = inspect_steam(*selected, game);
            return status.inspected && !status.primary_running && !status.helper_running &&
                   !status.other_steam && !status.game_running;
        });
    };
    Result result;
    try { result = write(); }
    catch (const std::exception&) { result = error(Error::SteamWriteFailed, config); }
    if (reopen && (!launch_steam(*selected, false) || !wait_for_steam(*selected, game, true))) {
        if (result) return error(Error::SteamReopenFailed, selected->steam_root);
        result.detail += " | Steam could not reopen";
    }
    return result;
}

} // namespace tetrisphere::launcher
