#pragma once

#include <optional>
#include <string>

namespace tetrisphere {

enum class PresentationFilter { Nearest, Linear, Pixel };

struct RunOptions {
    bool valid = true;
    bool help = false;
    std::string error;
    std::string rom_path;
    std::optional<bool> fullscreen;
    std::optional<int> msaa_samples;
    std::optional<PresentationFilter> presentation_filter;
    std::optional<bool> three_point_filter;
};

RunOptions parse_run_options(int argc, const char* const argv[]);
const char* run_options_help();

} // namespace tetrisphere
