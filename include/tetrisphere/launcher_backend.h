#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace tetrisphere::launcher {
namespace fs = std::filesystem;

enum class Error {
    None, RomUnreadable, RomInvalid, RomUnsupported, InvalidDestination,
    DestinationNotEmpty, PackageIncomplete, CopyFailed, PreparationFailed,
    PreferencesFailed, NotInstalled, LaunchFailed, SteamUnavailable,
    SteamGameRunning, SteamShutdownFailed, SteamShortcutInvalid,
    ArtworkInvalid, ArtworkDownloadFailed, SteamWriteFailed, SteamReopenFailed,
};

struct Result {
    Error error = Error::None;
    std::string detail;
    explicit operator bool() const { return error == Error::None; }
};

struct ProjectProfile {
    std::string display_name;
    fs::path executable;
    std::vector<fs::path> required_files;
    std::vector<fs::path> optional_files;
    std::size_t rom_size = 0;
    std::string rom_sha256;
    fs::path icon_relative;
    fs::path rom_cache_relative;
    std::string github_repository;
    // Distribution-only payload; never copied into an installed game.
    fs::path installer_payload_relative = "installer/game.zip";
};

ProjectProfile project_profile();

struct Preferences {
    std::string language = "en";
    fs::path destination;
};

fs::path default_preferences_path();
fs::path default_installation_path();
Preferences load_preferences(const fs::path& path);
Result save_preferences(const fs::path& path, const Preferences& preferences);

struct RomSelection {
    fs::path path;
    std::vector<std::uint8_t> bytes;
    Result result;
};

RomSelection validate_rom(const fs::path& path, const ProjectProfile& profile = project_profile());
bool installation_ready(const fs::path& root, const ProjectProfile& profile = project_profile());
Result apply_language(const fs::path& root, const std::string& language);

enum class InstallStage { Copying, Preparing, Finalizing, Complete };
struct InstallProgress {
    InstallStage stage;
    std::uintmax_t completed_bytes = 0;
    std::uintmax_t total_bytes = 0;
};
using ProgressCallback = std::function<void(const InstallProgress&)>;

Result install(const fs::path& source_root, const fs::path& destination,
               const RomSelection& rom, const std::string& language,
               ProgressCallback progress = {}, const ProjectProfile& profile = project_profile());
Result play(const fs::path& root, const std::string& language,
            const ProjectProfile& profile = project_profile());

enum class SteamInstallation { Native, Flatpak, Windows };
struct SteamAccount {
    fs::path steam_root;
    std::string account_id;
    std::string name;
    SteamInstallation installation = SteamInstallation::Native;
};

struct ArtworkChoice {
    fs::path portrait;
    fs::path landscape;
    fs::path hero;
    fs::path logo;
    fs::path icon;
    std::array<std::string, 5> downloaded;
};

std::vector<SteamAccount> discover_steam_accounts();
Result add_to_steam(const SteamAccount& account, const fs::path& installed_root,
                    const ArtworkChoice& artwork, const ProjectProfile& profile = project_profile());

} // namespace tetrisphere::launcher
