#ifdef NDEBUG
#undef NDEBUG
#endif
#include "tetrisphere/launcher_backend.h"
#include "miniz.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#ifndef _WIN32
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

namespace l = tetrisphere::launcher;
namespace fs = std::filesystem;

void write(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    assert(output.good());
}
std::string read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

std::string executable_zip(const std::string& name, const std::string& executable,
                           bool extra_member = false, unsigned level = MZ_BEST_COMPRESSION) {
    mz_zip_archive archive{};
    assert(mz_zip_writer_init_heap(&archive, 0, 0));
    assert(mz_zip_writer_add_mem(&archive, name.c_str(), executable.data(), executable.size(), level));
    if (extra_member) assert(mz_zip_writer_add_mem(&archive, "unexpected.txt", "extra", 5, level));
    void* output = nullptr;
    std::size_t size = 0;
    assert(mz_zip_writer_finalize_heap_archive(&archive, &output, &size));
    const std::string bytes(static_cast<const char*>(output), size);
    mz_free(output);
    assert(mz_zip_writer_end(&archive));
    return bytes;
}

#ifndef _WIN32
pid_t spawn(const fs::path& executable, const char* mode, const fs::path& directory) {
    auto program = executable.string(), operation = std::string(mode), path = directory.string();
    char* arguments[] = {program.data(), operation.data(), path.data(), nullptr};
    pid_t pid = 0;
    assert(posix_spawn(&pid, program.c_str(), nullptr, nullptr, arguments, environ) == 0);
    return pid;
}

void wait_for_file(const fs::path& path) {
    for (int i = 0; i < 300 && !fs::exists(path); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(fs::exists(path));
}

int fixture_process(const std::string& mode, const fs::path& directory) {
    if (mode == "--fixture-game") {
        write(directory / "game-ready", "ready");
        wait_for_file(directory / "game-stop");
        return 0;
    }
    if (mode == "--fixture-steam") {
        fs::remove(directory / "shutdown-request");
        fs::remove(directory / "helper-ready");
        const auto wrapper = spawn(directory / "srt-bwrap", "--fixture-wrapper", directory);
        wait_for_file(directory / "helper-ready");
        std::ofstream output(directory / "steam-starts", std::ios::app);
        output << "started\n";
        output.close();
        wait_for_file(directory / "shutdown-request");
        int status = 0;
        assert(waitpid(wrapper, &status, 0) == wrapper && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        return 0;
    }
    if (mode == "--fixture-wrapper") {
        const auto helper = spawn(directory / "steamwebhelper", "--fixture-helper", directory);
        int status = 0;
        assert(waitpid(helper, &status, 0) == helper && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        return 0;
    }
    assert(mode == "--fixture-helper");
    write(directory / "helper-ready", "ready");
    wait_for_file(directory / "shutdown-request");
    return 0;
}
#endif

int main(int argc, char** argv) {
#ifndef _WIN32
    if (argc == 3 && std::string_view(argv[1]).starts_with("--fixture-"))
        return fixture_process(argv[1], argv[2]);
#endif
    const bool install_only = argc == 3 && std::string_view(argv[2]) == "--install-only";
    assert(argc == 2 || install_only);
    const auto root = fs::absolute(argv[1]);
    auto profile = l::project_profile();
    profile.rom_sha256 = "2cc4b492c1c5ae0b6777840824bb930e230e0be243d76fb76f3d7a69853d186e";
    const auto source = root / "package";
    const auto destination = root / "installed folder";
    for (const auto& relative : profile.required_files) write(source / relative, "test library");
    const char png[] =
        "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52"
        "\x00\x00\x00\x01\x00\x00\x00\x01\x08\x06\x00\x00\x00\x1f\x15\xc4"
        "\x89\x00\x00\x00\x0d\x49\x44\x41\x54\x78\x9c\x63\x58\xb0\xe1\xc0"
        "\x7f\x00\x07\x14\x03\x10\x29\x35\xad\x44\x00\x00\x00\x00\x49\x45"
        "\x4e\x44\xae\x42\x60\x82";
    write(source / profile.icon_relative, std::string(png, sizeof(png) - 1));
#ifndef _WIN32
    fs::copy_file("/bin/true", source / profile.executable);
#else
    write(source / profile.executable, "MZ test artifact");
#endif
    const auto rom_path = root / "fixture.z64";
    std::string rom(profile.rom_size, '\0');
    rom.replace(0, 4, "\x80\x37\x12\x40", 4);
    write(rom_path, rom);
    assert(l::validate_rom(rom_path).result.error == l::Error::RomUnsupported);
    const auto selection = l::validate_rom(rom_path, profile);
    assert(selection.result);
    const auto prefs_path = root / "config" / "launcher.conf";
    assert(l::load_preferences(prefs_path).language == "en");
    assert(l::save_preferences(prefs_path, {"de", destination}));
    auto prefs = l::load_preferences(prefs_path);
    assert(prefs.language == "de" && prefs.destination == destination);
    assert(!l::installation_ready(destination, profile));
    std::vector<l::InstallStage> stages;
    assert(l::install(source, destination, selection, "es", [&](const auto& progress) {
        if (stages.empty() || stages.back() != progress.stage) stages.push_back(progress.stage);
        if (progress.stage == l::InstallStage::Complete) assert(l::installation_ready(destination, profile));
    }, profile));
    assert((stages == std::vector<l::InstallStage>{l::InstallStage::Copying, l::InstallStage::Preparing,
            l::InstallStage::Finalizing, l::InstallStage::Complete}));
    assert(read(destination / "data/language.txt") == "es\n");
    assert(read(destination / profile.rom_cache_relative) == rom);
    assert(read(rom_path) == rom);
    write(destination / "keep.txt", "private save");
    assert(l::install(source, destination, selection, "en", {}, profile).error == l::Error::DestinationNotEmpty);
    assert(read(destination / "keep.txt") == "private save");
    auto invalid = selection;
    invalid.bytes.back() = 1;
    assert(!l::install(source, root / "failed", invalid, "en", {}, profile));
    assert(!fs::exists(root / "failed"));
    assert(l::install(source, source, selection, "fr", {}, profile));
    assert(read(source / "data/language.txt") == "fr\n");
    assert(l::installation_ready(source, profile));
    // The installer ships the game only inside its single-member payload.
    // Installing must reproduce the game in the selected directory, preserve
    // the source ZIP/ROM, and exclude the payload from the installed files.
    const auto installer = root / "installer package";
    fs::copy(source, installer, fs::copy_options::recursive);
    fs::remove_all(installer / "data");
    const auto executable = read(installer / profile.executable);
    fs::remove(installer / profile.executable);
    const auto payload = installer / "installer/game.zip";
    const auto zipped = executable_zip(profile.executable.generic_string(), executable);
    write(payload, zipped);
    assert(!l::installation_ready(installer, profile));
    const auto unpacked = root / "installed from payload";
    std::uintmax_t previous_copy = 0, total_copy = 0;
    assert(l::install(installer, unpacked, selection, "de", [&](const auto& progress) {
        if (progress.stage == l::InstallStage::Copying) {
            assert(progress.completed_bytes >= previous_copy);
            assert(progress.completed_bytes <= progress.total_bytes);
            previous_copy = progress.completed_bytes;
            total_copy = progress.total_bytes;
        }
    }, profile));
    assert(previous_copy == total_copy && total_copy >= executable.size());
    assert(l::installation_ready(unpacked, profile));
    assert(read(unpacked / profile.executable) == executable);
    assert(read(unpacked / "data/language.txt") == "de\n");
    assert(!fs::exists(unpacked / "installer"));
    assert(!fs::exists(installer / profile.executable));
    assert(read(payload) == zipped && read(rom_path) == rom);
    assert(l::install(installer, installer, selection, "en", {}, profile).error == l::Error::InvalidDestination);
    assert(!fs::exists(installer / profile.executable) && !fs::exists(installer / "data"));
    assert(read(payload) == zipped);

    // CRC damage that leaves the executable header intact must not publish.
    auto crc_damage = executable_zip(profile.executable.generic_string(), executable, false, 0);
    const auto word16 = [&](std::size_t offset) {
        return static_cast<unsigned char>(crc_damage[offset]) |
               (unsigned(static_cast<unsigned char>(crc_damage[offset + 1])) << 8);
    };
    const auto data_offset = 30u + word16(26) + word16(28);
    crc_damage[data_offset + executable.size() - 1] ^= 1;
    auto oversized = zipped;
    const auto central = oversized.rfind(std::string("PK\x01\x02", 4));
    assert(central != std::string::npos);
    // 256 MiB + 1 expanded bytes, without actually allocating that much data.
    for (unsigned byte = 0; byte < 4; ++byte)
        oversized[central + 24 + byte] = static_cast<char>((0x10000001u >> (8 * byte)) & 0xffu);
    const std::vector<std::string> invalid_payloads{
        crc_damage, zipped.substr(0, zipped.size() / 2), oversized,
        executable_zip("../outside", executable),
        executable_zip(profile.executable.generic_string(), executable, true),
        executable_zip(profile.executable.generic_string(), "not an executable"),
    };
    for (std::size_t i = 0; i < invalid_payloads.size(); ++i) {
        write(payload, invalid_payloads[i]);
        const auto failed = root / ("payload failure " + std::to_string(i));
        assert(!l::install(installer, failed, selection, "en", {}, profile));
        assert(!fs::exists(failed));
        assert(read(payload) == invalid_payloads[i]);
        assert(!fs::exists(installer / profile.executable));
    }
    // Bound the compressed input too; sparse resizing avoids a large fixture.
    fs::resize_file(payload, 256u * 1024u * 1024u + 1);
    const auto oversized_destination = root / "oversized archive";
    assert(l::install(installer, oversized_destination, selection, "en", {}, profile).error == l::Error::PackageIncomplete);
    assert(!fs::exists(oversized_destination));
    write(payload, zipped);

    const auto blocked_destination = root / "appeared during install";
    assert(l::install(source, blocked_destination, selection, "en", [&](const auto& progress) {
        if (progress.stage == l::InstallStage::Finalizing) write(blocked_destination / "keep", "concurrent");
    }, profile).error == l::Error::DestinationNotEmpty);
    assert(read(blocked_destination / "keep") == "concurrent");

    // Permit isolated installer checks while the user's real Steam is running.
    if (install_only) return 0;

#ifndef _WIN32
    // Fixture home contains no real Steam data. Keep discovery and writes local.
    const auto home = root / "home";
    const auto fixture_bin = root / "fixture-bin";
    fs::create_directories(fixture_bin);
    setenv("HOME", home.c_str(), 1);
    setenv("PATH", fixture_bin.c_str(), 1);
    unsetenv("XDG_DATA_HOME");
    const auto steam = home / ".local/share/Steam";
    const auto config = steam / "userdata/4242/config";
    write(steam / "config/loginusers.vdf",
          "\"users\" { \"76561197960269970\" { \"AccountName\" \"fixture\" \"PersonaName\" \"Test Player\" } }");
    const auto vdf = config / "shortcuts.vdf";
    const std::string unrelated("\x00shortcuts\x00\x00" "0\x00\x01" "AppName\x00Unrelated\x00\x01" "Exe\x00/elsewhere/game\x00\x08\x08\x08",
        sizeof("\x00shortcuts\x00\x00" "0\x00\x01" "AppName\x00Unrelated\x00\x01" "Exe\x00/elsewhere/game\x00\x08\x08\x08") - 1);
    write(vdf, unrelated);
    auto accounts = l::discover_steam_accounts();
    assert(accounts.size() == 1 && accounts[0].name == "Test Player" && accounts[0].account_id == "4242");
    assert(read(vdf) == unrelated);
    l::ArtworkChoice invalid_art;
    invalid_art.hero = rom_path;
    assert(l::add_to_steam(accounts[0], destination, invalid_art, profile).error == l::Error::ArtworkInvalid);
    assert(read(vdf) == unrelated);
    assert(l::add_to_steam(accounts[0], destination, {}, profile));
    auto added = read(vdf);
    assert(added.compare(0, unrelated.size() - 2, unrelated, 0, unrelated.size() - 2) == 0);
    assert(added.find((destination / profile.executable).string()) != std::string::npos);
    assert(l::add_to_steam(accounts[0], destination, {}, profile));
    assert(read(vdf) == added);
    write(vdf, "corrupt");
    assert(l::add_to_steam(accounts[0], destination, {}, profile).error == l::Error::SteamShortcutInvalid);
    assert(read(vdf) == "corrupt");
    write(vdf, added);
    assert(l::play(destination, "pt", profile));
    assert(read(destination / "data/language.txt") == "pt\n");

    // Only fixture executables are on PATH. No action can launch desktop Steam.
    const auto process_fixture = steam / "process-fixture";
    const auto steam_binary = process_fixture / "steam";
    fs::create_directories(process_fixture);
    const auto self = fs::read_symlink("/proc/self/exe");
    for (const auto* name : {"steam", "srt-bwrap", "steamwebhelper", "tetrisphere"})
        fs::copy_file(self, process_fixture / name);
    const auto command = fixture_bin / "steam";
    write(command, "#!/bin/sh\n"
                   "if [ \"$1\" = -shutdown ]; then\n"
                   "  : > \"$STEAM_FIXTURE_DIR/shutdown-request\"\n"
                   "else\n"
                   "  exec \"$STEAM_FIXTURE_DIR/steam\" --fixture-steam \"$STEAM_FIXTURE_DIR\"\n"
                   "fi\n");
    const auto executable_permissions = fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec;
    fs::permissions(command, executable_permissions);
    setenv("STEAM_FIXTURE_DIR", process_fixture.c_str(), 1);
    const auto game = spawn(process_fixture / "tetrisphere", "--fixture-game", process_fixture);
    wait_for_file(process_fixture / "game-ready");
    assert(l::add_to_steam(accounts[0], destination, {}, profile).error == l::Error::SteamGameRunning);
    assert(read(vdf) == added);
    write(process_fixture / "game-stop", "stop");
    int status = 0;
    assert(waitpid(game, &status, 0) == game && WIFEXITED(status) && WEXITSTATUS(status) == 0);

    const auto steam_pid = spawn(steam_binary, "--fixture-steam", process_fixture);
    wait_for_file(process_fixture / "steam-starts");
    fs::permissions(command, fs::perms::owner_read | fs::perms::owner_write);
    assert(l::add_to_steam(accounts[0], destination, {}, profile).error == l::Error::SteamShutdownFailed);
    assert(read(vdf) == added);
    assert(!fs::exists(process_fixture / "shutdown-request"));
    fs::permissions(command, executable_permissions);
    assert(l::add_to_steam(accounts[0], destination, {}, profile));
    assert(waitpid(steam_pid, &status, 0) == steam_pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(read(process_fixture / "steam-starts") == "started\nstarted\n");
    assert(read(vdf) == added);
    write(vdf, "corrupt");
    assert(l::add_to_steam(accounts[0], destination, {}, profile).error == l::Error::SteamShortcutInvalid);
    assert(read(process_fixture / "steam-starts") == "started\nstarted\nstarted\n");
    assert(read(vdf) == "corrupt");
    write(process_fixture / "shutdown-request", "stop");
#endif
}
