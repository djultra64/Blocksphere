#include "steam_artwork.h"

#include <array>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace tetrisphere::launcher {
namespace {

constexpr std::string_view cdn = "https://cdn2.steamgriddb.com";
constexpr std::size_t thumbnail_limit = 2 * 1024 * 1024;
constexpr std::size_t image_limit = 24 * 1024 * 1024;

// Curated from the public Tetrisphere gallery (game 29858).
// Entries use its observed ordering, with the first available image suggested.
constexpr SteamArtwork artwork[] = {
#include "tetrisphere_steam_artwork.inc"
};

#ifdef _WIN32
struct HttpHandle {
    HINTERNET handle = nullptr;
    ~HttpHandle() { if (handle) WinHttpCloseHandle(handle); }
    explicit operator bool() const { return handle != nullptr; }
};

std::string download(std::string_view path, std::size_t limit) {
    const HttpHandle session{WinHttpOpen(L"Blocksphere", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session || !WinHttpSetTimeouts(session.handle, 3500, 3500, 3500, 3500)) return {};
    const HttpHandle server{WinHttpConnect(session.handle, L"cdn2.steamgriddb.com",
                                          INTERNET_DEFAULT_HTTPS_PORT, 0)};
    if (!server) return {};
    const std::wstring object(path.begin(), path.end());
    const HttpHandle request{WinHttpOpenRequest(server.handle, L"GET", object.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
    if (!request) return {};
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!WinHttpSetOption(request.handle, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)) ||
        !WinHttpSendRequest(request.handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.handle, nullptr)) return {};
    DWORD status = 0;
    DWORD size = sizeof(status);
    if (!WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) ||
        status != 200) return {};
    std::string data;
    std::array<char, 16384> buffer{};
    DWORD count = 0;
    do {
        if (!WinHttpReadData(request.handle, buffer.data(), static_cast<DWORD>(buffer.size()), &count) ||
            count > limit - data.size()) return {};
        data.append(buffer.data(), count);
    } while (count != 0);
    return data;
}
#else
std::string download(std::string_view path, std::size_t limit) {
    constexpr const char* program = "/usr/bin/curl";
    std::array arguments{std::string(program), std::string("-q"), std::string("--fail"),
        std::string("--silent"), std::string("--proto"), std::string("=https"),
        std::string("--max-time"), std::string("5"), std::string("--max-filesize"),
        std::to_string(limit), std::string(cdn) + std::string(path)};
    std::array<char*, arguments.size() + 1> argv{};
    for (std::size_t i = 0; i < arguments.size(); ++i) argv[i] = arguments[i].data();
    int descriptors[2]{};
    if (pipe2(descriptors, O_CLOEXEC) != 0) return {};
    posix_spawn_file_actions_t actions{};
    const int init = posix_spawn_file_actions_init(&actions);
    pid_t child = 0;
    int started = init;
    if (init == 0) {
        started = posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDOUT_FILENO);
        if (started == 0) started = posix_spawn(&child, program, &actions, nullptr, argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
    }
    close(descriptors[1]);
    if (started != 0) {
        close(descriptors[0]);
        return {};
    }
    std::string data;
    std::array<char, 16384> buffer{};
    ssize_t count = 0;
    for (;;) {
        count = read(descriptors[0], buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0 || static_cast<std::size_t>(count) > limit - data.size()) break;
        data.append(buffer.data(), static_cast<std::size_t>(count));
    }
    close(descriptors[0]);
    int status = 0;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    return count == 0 && waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0
        ? std::move(data) : std::string{};
}
#endif

} // namespace

std::span<const SteamArtwork> steam_artwork() { return artwork; }

std::string download_steam_artwork(std::string_view path, bool thumbnail) {
    for (const auto& asset : artwork) {
        if (path != (thumbnail ? asset.thumbnail : asset.image)) continue;
        auto data = download(path, thumbnail ? thumbnail_limit : image_limit);
        if (data.starts_with("\x89PNG\r\n\x1a\n") || data.starts_with("\xff\xd8\xff")) return data;
        break;
    }
    return {};
}

} // namespace tetrisphere::launcher
