#pragma once

#include <SDL.h>
#include <cstdlib>
#include <filesystem>

namespace tetrisphere {

// Explicit overrides isolate QA sessions; ordinary packages never use user data.
inline std::filesystem::path application_data_directory() {
    if (const char* override = std::getenv("TETRISPHERE_DATA_DIR");
        override != nullptr && *override != '\0') {
        return std::filesystem::u8path(override);
    }
    char* base = SDL_GetBasePath();
    if (base == nullptr) return {};
    auto directory = std::filesystem::u8path(base);
    SDL_free(base);
    directory = directory.lexically_normal();
    if (directory.filename().empty()) directory = directory.parent_path();
    return directory / "data";
}

} // namespace tetrisphere
