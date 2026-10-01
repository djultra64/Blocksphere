#include "tetrisphere/rt64_context.h"

#include <iostream>

namespace {
struct MockRt64Config {
    enum class Resolution { Original, WindowIntegerScale, Manual };
    enum class AspectRatio { Original, Expand, Manual };
    enum class RefreshRate { Original, Display, Manual };
    Resolution resolution = Resolution::WindowIntegerScale;
    double resolutionMultiplier = 7.0;
    AspectRatio aspectRatio = AspectRatio::Original;
    AspectRatio extAspectRatio = AspectRatio::Original;
    double aspectTarget = 16.0 / 9.0;
    double extAspectTarget = 16.0 / 9.0;
    RefreshRate refreshRate = RefreshRate::Original;
    int refreshRateTarget = 60;
    int downsampleMultiplier = 1;
};

int fail(const char* reason) { std::cerr << reason << '\n'; return 1; }
}

int main() {
    using namespace ultramodern::renderer;
    GraphicsConfig host{};
    host.res_option = Resolution::Auto;
    host.ar_option = AspectRatio::Original;
    host.rr_option = RefreshRate::Original;
    host.rr_manual_value = 60;
    host.ds_option = 1;
    MockRt64Config rt{};
    if (!tetrisphere::apply_rt64_graphics_config(host, rt) ||
        rt.resolution != MockRt64Config::Resolution::WindowIntegerScale ||
        rt.aspectRatio != MockRt64Config::AspectRatio::Original ||
        rt.extAspectRatio != MockRt64Config::AspectRatio::Original ||
        rt.refreshRate != MockRt64Config::RefreshRate::Original ||
        rt.downsampleMultiplier != 1)
        return fail("default graphics behavior changed");

    host.res_option = Resolution::Original;
    host.rr_option = RefreshRate::Manual;
    host.rr_manual_value = 30;
    if (!tetrisphere::apply_rt64_graphics_config(host, rt) ||
        rt.resolution != MockRt64Config::Resolution::Original ||
        rt.refreshRate != MockRt64Config::RefreshRate::Manual ||
        rt.refreshRateTarget != 30)
        return fail("original resolution/30 Hz mapping failed");

    host.res_option = Resolution::Original2x;
    host.ar_option = AspectRatio::Expand;
    host.rr_manual_value = 120;
    host.ds_option = 2;
    if (!tetrisphere::apply_rt64_graphics_config(host, rt) ||
        rt.resolution != MockRt64Config::Resolution::Manual ||
        rt.resolutionMultiplier != 2.0 ||
        rt.aspectRatio != MockRt64Config::AspectRatio::Expand ||
        rt.extAspectRatio != MockRt64Config::AspectRatio::Expand ||
        rt.refreshRateTarget != 120 || rt.downsampleMultiplier != 2)
        return fail("2x/expanded/120 Hz mapping failed");

    host.ar_option = AspectRatio::Manual;
    host.rr_option = RefreshRate::Display;
    rt.aspectTarget = 1.0;
    rt.extAspectTarget = 1.0;
    if (!tetrisphere::apply_rt64_graphics_config(host, rt) ||
        rt.aspectRatio != MockRt64Config::AspectRatio::Manual ||
        rt.extAspectRatio != MockRt64Config::AspectRatio::Manual ||
        rt.aspectTarget != 16.0 / 9.0 ||
        rt.extAspectTarget != 16.0 / 9.0 ||
        rt.refreshRate != MockRt64Config::RefreshRate::Display)
        return fail("exact 16:9 aspect/display rate mapping failed");

    host.res_option = Resolution::Auto;
    if (!tetrisphere::apply_rt64_graphics_config(host, rt, 4.5) ||
        rt.resolution != MockRt64Config::Resolution::Manual ||
        rt.resolutionMultiplier != 4.5)
        return fail("exact 1080p internal scale mapping failed");

    const auto before = rt;
    host.rr_option = RefreshRate::Manual;
    host.rr_manual_value = 0;
    if (tetrisphere::apply_rt64_graphics_config(host, rt) ||
        rt.refreshRate != before.refreshRate ||
        rt.refreshRateTarget != before.refreshRateTarget)
        return fail("invalid refresh rate changed RT64 configuration");
    host.rr_manual_value = 60;
    if (tetrisphere::apply_rt64_graphics_config(host, rt, 0.0) ||
        rt.resolutionMultiplier != before.resolutionMultiplier)
        return fail("invalid internal scale changed RT64 configuration");
    return 0;
}
