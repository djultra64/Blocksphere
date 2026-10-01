#include "tetrisphere/run_options.h"

#include <cassert>
#include <string>

using tetrisphere::parse_run_options;

int main() {
    {
        const char* argv[] = {"tetrisphere-m1", "--help"};
        const auto options = parse_run_options(2, argv);
        assert(options.valid && options.help && options.rom_path.empty());
    }
    {
        const char* argv[] = {"tetrisphere-m1", "--fullscreen", "--msaa2x",
                              "--msaa=8x", "--windowed", "--presentation-filter=pixel",
                              "--texture-filter=three-point", "Rom con ñ.zip"};
        const auto options = parse_run_options(8, argv);
        assert(options.valid && !options.help);
        assert(options.fullscreen && !*options.fullscreen);
        assert(options.msaa_samples && *options.msaa_samples == 8);
        assert(options.presentation_filter &&
               *options.presentation_filter == tetrisphere::PresentationFilter::Pixel);
        assert(options.three_point_filter && *options.three_point_filter);
        assert(options.rom_path == "Rom con ñ.zip");
    }
    {
        const char* argv[] = {"tetrisphere-m1", "--msaa=3", "rom.z64"};
        const auto options = parse_run_options(3, argv);
        assert(!options.valid && options.error.find("MSAA") != std::string::npos);
    }
    {
        const char* argv[] = {"tetrisphere-m1", "--presentation-filter", "rom.z64"};
        const auto options = parse_run_options(3, argv);
        assert(!options.valid && options.error.find("presentation") != std::string::npos);
    }
    {
        const char* argv[] = {"tetrisphere-m1", "rom.z64", "extra.z64"};
        const auto options = parse_run_options(3, argv);
        assert(!options.valid && options.error.find("ROM") != std::string::npos);
    }
    return 0;
}
