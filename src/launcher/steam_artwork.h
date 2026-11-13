#pragma once

#include <span>
#include <string>
#include <string_view>

namespace tetrisphere::launcher {

struct SteamArtwork {
    unsigned category;
    std::string_view artist;
    std::string_view page;
    std::string_view image;
    std::string_view thumbnail;
};

std::span<const SteamArtwork> steam_artwork();
std::string download_steam_artwork(std::string_view path, bool thumbnail);

} // namespace tetrisphere::launcher
