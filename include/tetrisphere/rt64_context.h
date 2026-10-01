#pragma once

#include "ultramodern/renderer_context.hpp"

#include <cmath>
#include <memory>
#include <optional>

namespace tetrisphere {

struct RunOptions;

// Apply only options represented by RT64's current user configuration. This
// deliberately leaves window mode, HUD layout, VSync and backend unchanged.
// The template keeps the policy testable without creating a Vulkan device.
template <typename Rt64Config>
bool apply_rt64_graphics_config(const ultramodern::renderer::GraphicsConfig& host,
                               Rt64Config& config,
                               std::optional<double> internal_resolution_scale = std::nullopt) {
    namespace Host = ultramodern::renderer;
    if (host.ds_option < 1 || host.ds_option > 32 ||
        (internal_resolution_scale &&
         (!std::isfinite(*internal_resolution_scale) ||
          *internal_resolution_scale < 1.0 || *internal_resolution_scale > 32.0)) ||
        (host.rr_option == Host::RefreshRate::Manual &&
         (host.rr_manual_value < 10 || host.rr_manual_value > 1000))) return false;

    auto next = config;
    switch (host.res_option) {
        case Host::Resolution::Original:
            next.resolution = Rt64Config::Resolution::Original;
            break;
        case Host::Resolution::Original2x:
            next.resolution = Rt64Config::Resolution::Manual;
            next.resolutionMultiplier = 2.0;
            break;
        case Host::Resolution::Auto:
            next.resolution = Rt64Config::Resolution::WindowIntegerScale;
            break;
        default: return false;
    }
    if (internal_resolution_scale) {
        next.resolution = Rt64Config::Resolution::Manual;
        next.resolutionMultiplier = *internal_resolution_scale;
    }
    switch (host.ar_option) {
        case Host::AspectRatio::Original:
            next.aspectRatio = Rt64Config::AspectRatio::Original;
            next.extAspectRatio = Rt64Config::AspectRatio::Original;
            break;
        case Host::AspectRatio::Expand:
            next.aspectRatio = Rt64Config::AspectRatio::Expand;
            next.extAspectRatio = Rt64Config::AspectRatio::Expand;
            break;
        case Host::AspectRatio::Manual:
            next.aspectRatio = Rt64Config::AspectRatio::Manual;
            next.extAspectRatio = Rt64Config::AspectRatio::Manual;
            next.aspectTarget = 16.0 / 9.0;
            next.extAspectTarget = 16.0 / 9.0;
            break;
        default: return false;
    }
    switch (host.rr_option) {
        case Host::RefreshRate::Original:
            next.refreshRate = Rt64Config::RefreshRate::Original;
            break;
        case Host::RefreshRate::Display:
            next.refreshRate = Rt64Config::RefreshRate::Display;
            break;
        case Host::RefreshRate::Manual:
            next.refreshRate = Rt64Config::RefreshRate::Manual;
            next.refreshRateTarget = host.rr_manual_value;
            break;
        default: return false;
    }
    next.downsampleMultiplier = host.ds_option;
    config = next;
    return true;
}

// Set once before the renderer is created. nullopt keeps RT64's window based
// integer scaling; a finite multiplier selects an exact internal resolution.
void set_rt64_internal_resolution_scale(std::optional<double> multiplier);
void set_rt64_run_options(const RunOptions& options);

std::unique_ptr<ultramodern::renderer::RendererContext> create_rt64_context(
    std::uint8_t* rdram, ultramodern::renderer::WindowHandle window,
    bool developer_mode);

} // namespace tetrisphere
