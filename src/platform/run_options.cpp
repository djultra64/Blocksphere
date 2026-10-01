#include "tetrisphere/run_options.h"

#include <string_view>

namespace tetrisphere {
namespace {

bool parse_msaa(std::string_view value, int& samples) {
    if (value == "off" || value == "0") samples = 0;
    else if (value == "2" || value == "2x") samples = 2;
    else if (value == "4" || value == "4x") samples = 4;
    else if (value == "8" || value == "8x") samples = 8;
    else return false;
    return true;
}

RunOptions invalid(const std::string& message) {
    RunOptions result;
    result.valid = false;
    result.error = message;
    return result;
}

} // namespace

RunOptions parse_run_options(int argc, const char* const argv[]) {
    RunOptions result;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            result.help = true;
            continue;
        }
        if (argument == "--fullscreen") {
            result.fullscreen = true;
            continue;
        }
        if (argument == "--windowed") {
            result.fullscreen = false;
            continue;
        }
        if (argument == "--msaa2x" || argument == "--msaa4x" || argument == "--msaa8x") {
            result.msaa_samples = argument[6] - '0';
            continue;
        }
        if (argument == "--msaa-off") {
            result.msaa_samples = 0;
            continue;
        }
        if (argument == "--msaa" || argument.starts_with("--msaa=")) {
            const auto value = argument == "--msaa"
                ? (index + 1 < argc ? std::string_view(argv[++index]) : std::string_view{})
                : argument.substr(7);
            int samples = 0;
            if (!parse_msaa(value, samples)) return invalid("MSAA must be off, 2x, 4x, or 8x");
            result.msaa_samples = samples;
            continue;
        }
        if (argument == "--presentation-filter" ||
            argument.starts_with("--presentation-filter=")) {
            const auto value = argument == "--presentation-filter"
                ? (index + 1 < argc ? std::string_view(argv[++index]) : std::string_view{})
                : argument.substr(sizeof("--presentation-filter=") - 1);
            if (value == "nearest") result.presentation_filter = PresentationFilter::Nearest;
            else if (value == "linear") result.presentation_filter = PresentationFilter::Linear;
            else if (value == "pixel") result.presentation_filter = PresentationFilter::Pixel;
            else return invalid("presentation filter must be nearest, linear, or pixel");
            continue;
        }
        if (argument == "--texture-filter" || argument.starts_with("--texture-filter=")) {
            const auto value = argument == "--texture-filter"
                ? (index + 1 < argc ? std::string_view(argv[++index]) : std::string_view{})
                : argument.substr(sizeof("--texture-filter=") - 1);
            if (value == "three-point") result.three_point_filter = true;
            else if (value == "linear") result.three_point_filter = false;
            else return invalid("texture filter must be three-point or linear");
            continue;
        }
        if (argument.starts_with("-")) return invalid("unknown option: " + std::string(argument));
        if (!result.rom_path.empty()) return invalid("provide at most one ROM path");
        result.rom_path = argument;
    }
    return result;
}

const char* run_options_help() {
    return "Usage: tetrisphere [options] [ROM.z64|ROM.zip]\n"
           "Without a cached ROM, check beside the executable for a Tetrisphere ROM, then open the selector.\n"
           "  --fullscreen | --windowed       Override saved window mode for this run\n"
           "  --msaa=off|2x|4x|8x            Multisample antialiasing (also --msaa2x, etc.)\n"
           "  --presentation-filter=nearest|linear|pixel\n"
           "                                   Filter the final presented image\n"
           "  --texture-filter=linear|three-point\n"
           "                                   Filter N64 textures, not only 2D elements\n"
           "  --help                           Show this help\n"
           "Examples: tetrisphere --fullscreen --msaa4x\n"
           "          tetrisphere --windowed --presentation-filter=pixel game.zip\n"
           "Options for this run do not overwrite saved preferences.\n";
}

} // namespace tetrisphere
