#include "tetrisphere/rt64_context.h"
#include "tetrisphere/diagnostic_log.h"
#include "tetrisphere/dl_ui_tags.h"
#include "tetrisphere/f3d_flat_shade.h"
#include "tetrisphere/menu_medal_trace.h"
#include "tetrisphere/pause_capture_scene.h"
#include "tetrisphere/run_identity.h"
#include "tetrisphere/run_options.h"
#include "tetrisphere/recomp_support.h"

#ifdef _WIN32
// ultramodern's public renderer header opts into WIN32_LEAN_AND_MEAN before
// RT64's DXC declarations are reached. Supply the COM types DXC requires.
#include <ObjIdl.h>
#include <OleAuto.h>
#include <Unknwn.h>
#endif

#include "hle/rt64_application.h"
#include "ultramodern/config.hpp"
#include "ultramodern/ultramodern.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace {
// The guest copy finishes after its source display list has been submitted.
// The wide renderer consumes this event in workload order; the CPU RAM copy
// remains authoritative for the native renderer and for 4:3.
std::atomic<std::uint64_t> rescue_snapshot_source_workload{0};
std::mutex rescue_snapshot_mutex;
struct RescueSnapshotEvent {
    std::uint32_t source = 0;
    std::uint32_t destination = 0;
    std::uint64_t workload = 0;
    std::uint64_t generation = 0;
};
std::deque<RescueSnapshotEvent> rescue_snapshot_events;
std::uint64_t rescue_snapshot_generation = 0;
std::mutex ui_tag_mutex;
tetrisphere::DlUiTagQueue ui_tag_queue;
thread_local tetrisphere::DlUiTagBatch ui_active_batch;
thread_local std::uint32_t ui_current_tag = 0;
struct UiProducerScope {
    std::uint32_t callsite = 0;
    std::uint32_t first = 0;
    std::uint32_t anchor = 0;
};
thread_local std::vector<UiProducerScope> ui_producer_scopes;
}

// Guest producer hooks record the commands emitted by a verified UI callsite.
// A Center span can explicitly override an ancestor display-list tag.
extern "C" bool tetrisphere_rt64_ui_tag_record(std::uint8_t* rdram,
                                                std::uint32_t first,
                                                std::uint32_t past_last,
                                                std::uint32_t anchor) {
    if (anchor < 1 || anchor > 7) return false;
    const std::lock_guard<std::mutex> lock(ui_tag_mutex);
    return ui_tag_queue.record(first & 0x00FFFFFFu, past_last & 0x00FFFFFFu,
        static_cast<tetrisphere::DlUiAnchor>(anchor), rdram, 9437184);
}

extern "C" void tetrisphere_rt64_ui_tag_update(
    std::uint32_t command, const std::uint32_t* parents, std::size_t count) {
    ui_current_tag = static_cast<std::uint32_t>(
        ui_active_batch.lookup(command, parents, count));
}

extern "C" std::uint32_t tetrisphere_rt64_ui_tag_current() {
    return ui_current_tag;
}

extern "C" void tetrisphere_ui_span_begin(std::uint8_t* rdram,
                                           recomp_context* ctx,
                                           std::uint32_t callsite,
                                           std::uint32_t anchor) {
    static_cast<void>(ctx);
    if (anchor < 1 || anchor > 7 || ui_producer_scopes.size() >= 32) return;
    const std::uint32_t first =
        MEM_W(0, S32(0x800F22B4u)) & 0x00FFFFFFu;
    ui_producer_scopes.push_back({callsite, first, anchor});
}

extern "C" void tetrisphere_ui_span_end(std::uint8_t* rdram,
                                         recomp_context* ctx,
                                         std::uint32_t callsite) {
    static_cast<void>(ctx);
    if (ui_producer_scopes.empty() ||
        ui_producer_scopes.back().callsite != callsite) return;
    const UiProducerScope scope = ui_producer_scopes.back();
    ui_producer_scopes.pop_back();
    const std::uint32_t past_last =
        MEM_W(0, S32(0x800F22B4u)) & 0x00FFFFFFu;
    if (past_last <= scope.first ||
        !tetrisphere_rt64_ui_tag_record(rdram, scope.first, past_last, scope.anchor)) return;
    static std::atomic<unsigned> logged{0};
    if (logged.fetch_add(1, std::memory_order_relaxed) < 8) {
        std::fprintf(stderr,
                     "{\"event\":\"rt64_probe_ui_tag_span\","
                     "\"callsite\":%u,\"first\":%u,"
                     "\"past_last\":%u,\"anchor\":%u}\n",
                     callsite, scope.first, past_last, scope.anchor);
    }
}

extern "C" void tetrisphere_rescue_pause_snapshot_hook(std::uint8_t* rdram,
                                                        recomp_context* ctx) {
    static_cast<void>(ctx);
    std::int16_t scene_gate = 0;
    std::memcpy(&scene_gate, rdram + (0x000E121Cu ^ 2u), sizeof(scene_gate));
    if (!tetrisphere::pause_capture_scene_eligible(scene_gate)) return;
    const std::int64_t context = MEM_W(0, S32(0x801028CCu));
    const std::uint32_t source = MEM_W(0x1910, context) & 0x00FFFFFFu;
    const std::uint32_t destination = MEM_W(0, S32(0x800DFF00u)) & 0x00FFFFFFu;
    if (source == 0 || destination == 0 || source == destination) return;
    const auto source_workload = rescue_snapshot_source_workload.load(std::memory_order_acquire);
    {
        const std::lock_guard<std::mutex> lock(rescue_snapshot_mutex);
        if (rescue_snapshot_events.size() == 32) {
            std::fprintf(stderr, "{\"event\":\"rt64_rescue_snapshot_queue_overflow\",\"action\":\"reject_newest\"}\n");
            return;
        }
        rescue_snapshot_events.push_back({source, destination, source_workload,
                                          ++rescue_snapshot_generation});
    }
    if (const char* probe = std::getenv("TETRISPHERE_RT64_PROBE");
        probe != nullptr && probe[0] == '1' && probe[1] == '\0') {
        std::fprintf(stderr,
                     "{\"event\":\"rt64_probe_rescue_snapshot_hook\","
                     "\"source\":%u,\"destination\":%u,\"source_workload\":%llu}\n",
                     source, destination,
                     static_cast<unsigned long long>(source_workload));
    }
}

extern "C" bool tetrisphere_rt64_pause_snapshot_peek(std::uint32_t* source,
                                                       std::uint32_t* destination,
                                                       std::uint64_t* workload,
                                                       std::uint64_t* generation) {
    const std::lock_guard<std::mutex> lock(rescue_snapshot_mutex);
    if (rescue_snapshot_events.empty()) return false;
    const auto& event = rescue_snapshot_events.front();
    *source = event.source;
    *destination = event.destination;
    *workload = event.workload;
    *generation = event.generation;
    return true;
}

extern "C" void tetrisphere_rt64_pause_snapshot_ack(std::uint64_t generation) {
    const std::lock_guard<std::mutex> lock(rescue_snapshot_mutex);
    if (!rescue_snapshot_events.empty() &&
        rescue_snapshot_events.front().generation == generation) {
        rescue_snapshot_events.pop_front();
    }
}

namespace tetrisphere {
namespace {

std::array<std::uint8_t, 0x40> rom_header{};
std::array<std::uint8_t, 0x1000> dmem{};
std::array<std::uint8_t, 0x1000> imem{};
std::uint32_t mi_intr = 0;
std::uint32_t dpc_start = 0;
std::uint32_t dpc_end = 0;
std::uint32_t dpc_current = 0;
std::uint32_t dpc_status = 0;
std::uint32_t dpc_clock = 0;
std::uint32_t dpc_bufbusy = 0;
std::uint32_t dpc_pipebusy = 0;
std::uint32_t dpc_tmem = 0;
std::atomic_uint64_t display_list_count = 0;
std::atomic_uint64_t present_count = 0;
std::optional<double> internal_resolution_scale;
tetrisphere::RunOptions run_options;

void check_interrupts() {}

ultramodern::renderer::SetupResult setup_result(RT64::Application::SetupResult value) {
    using Host = ultramodern::renderer::SetupResult;
    using Rt = RT64::Application::SetupResult;
    switch (value) {
        case Rt::Success: return Host::Success;
        case Rt::DynamicLibrariesNotFound: return Host::DynamicLibrariesNotFound;
        case Rt::InvalidGraphicsAPI: return Host::InvalidGraphicsAPI;
        case Rt::GraphicsAPINotFound: return Host::GraphicsAPINotFound;
        case Rt::GraphicsDeviceNotFound: return Host::GraphicsDeviceNotFound;
    }
    std::abort();
}

class Context final : public ultramodern::renderer::RendererContext {
public:
    Context(std::uint8_t* rdram, ultramodern::renderer::WindowHandle window,
            bool developer_mode) {
        (void)developer_mode;
        RT64::Application::Core core{};
#ifdef _WIN32
        core.window = window.window;
#else
        core.window = window;
#endif
        core.checkInterrupts = check_interrupts;
        core.HEADER = rom_header.data();
        core.RDRAM = rdram;
        core.DMEM = dmem.data();
        core.IMEM = imem.data();
        core.MI_INTR_REG = &mi_intr;
        core.DPC_START_REG = &dpc_start;
        core.DPC_END_REG = &dpc_end;
        core.DPC_CURRENT_REG = &dpc_current;
        core.DPC_STATUS_REG = &dpc_status;
        core.DPC_CLOCK_REG = &dpc_clock;
        core.DPC_BUFBUSY_REG = &dpc_bufbusy;
        core.DPC_PIPEBUSY_REG = &dpc_pipebusy;
        core.DPC_TMEM_REG = &dpc_tmem;
        const auto* vi = ultramodern::renderer::get_vi_regs();
        core.VI_STATUS_REG = const_cast<std::uint32_t*>(&vi->VI_STATUS_REG);
        core.VI_ORIGIN_REG = const_cast<std::uint32_t*>(&vi->VI_ORIGIN_REG);
        core.VI_WIDTH_REG = const_cast<std::uint32_t*>(&vi->VI_WIDTH_REG);
        core.VI_INTR_REG = const_cast<std::uint32_t*>(&vi->VI_INTR_REG);
        core.VI_V_CURRENT_LINE_REG = const_cast<std::uint32_t*>(&vi->VI_V_CURRENT_LINE_REG);
        core.VI_TIMING_REG = const_cast<std::uint32_t*>(&vi->VI_TIMING_REG);
        core.VI_V_SYNC_REG = const_cast<std::uint32_t*>(&vi->VI_V_SYNC_REG);
        core.VI_H_SYNC_REG = const_cast<std::uint32_t*>(&vi->VI_H_SYNC_REG);
        core.VI_LEAP_REG = const_cast<std::uint32_t*>(&vi->VI_LEAP_REG);
        core.VI_H_START_REG = const_cast<std::uint32_t*>(&vi->VI_H_START_REG);
        core.VI_V_START_REG = const_cast<std::uint32_t*>(&vi->VI_V_START_REG);
        core.VI_V_BURST_REG = const_cast<std::uint32_t*>(&vi->VI_V_BURST_REG);
        core.VI_X_SCALE_REG = const_cast<std::uint32_t*>(&vi->VI_X_SCALE_REG);
        core.VI_Y_SCALE_REG = const_cast<std::uint32_t*>(&vi->VI_Y_SCALE_REG);

        RT64::ApplicationConfiguration config{};
        config.appId = "tetrisphere-m1";
        config.useConfigurationFile = false;
        app_ = std::make_unique<RT64::Application>(core, config);
        app_->userConfig.graphicsAPI = RT64::UserConfiguration::GraphicsAPI::Vulkan;
        app_->userConfig.resolution = RT64::UserConfiguration::Resolution::WindowIntegerScale;
        app_->userConfig.aspectRatio = RT64::UserConfiguration::AspectRatio::Original;
        app_->userConfig.displayBuffering = RT64::UserConfiguration::DisplayBuffering::Triple;
        app_->userConfig.developerMode = true;
        if (run_options.msaa_samples) {
            switch (*run_options.msaa_samples) {
                case 0: app_->userConfig.antialiasing = RT64::UserConfiguration::Antialiasing::None; break;
                case 2: app_->userConfig.antialiasing = RT64::UserConfiguration::Antialiasing::MSAA2X; break;
                case 4: app_->userConfig.antialiasing = RT64::UserConfiguration::Antialiasing::MSAA4X; break;
                case 8: app_->userConfig.antialiasing = RT64::UserConfiguration::Antialiasing::MSAA8X; break;
            }
        }
        if (run_options.presentation_filter) {
            switch (*run_options.presentation_filter) {
                case PresentationFilter::Nearest:
                    app_->userConfig.filtering = RT64::UserConfiguration::Filtering::Nearest; break;
                case PresentationFilter::Linear:
                    app_->userConfig.filtering = RT64::UserConfiguration::Filtering::Linear; break;
                case PresentationFilter::Pixel:
                    app_->userConfig.filtering = RT64::UserConfiguration::Filtering::AntiAliasedPixelScaling; break;
            }
        }
        if (run_options.three_point_filter)
            app_->userConfig.threePointFiltering = *run_options.three_point_filter;
        // Apply the host settings before RT64 creates its render targets.
        host_graphics_ = ultramodern::renderer::get_graphics_config();
        if (!apply_rt64_graphics_config(host_graphics_,
                                        app_->userConfig, internal_resolution_scale)) {
            diagnostic_log().error("rt64_graphics_config_invalid");
        }
        app_->enhancementConfig.presentation.mode =
            RT64::EnhancementConfiguration::Presentation::Mode::Console;
        setup_result = ::tetrisphere::setup_result(app_->setup(0));
        if (setup_result == ultramodern::renderer::SetupResult::Success) {
            const unsigned requested_samples = run_options.msaa_samples
                ? static_cast<unsigned>(*run_options.msaa_samples == 0
                    ? 1 : *run_options.msaa_samples) : 0;
            if (requested_samples != 0 &&
                app_->userConfig.msaaSampleCount() != requested_samples) {
                std::fprintf(stderr,
                             "{\"event\":\"rt64_msaa_fallback\",\"requested_samples\":%u,"
                             "\"effective_samples\":%u}\n",
                             requested_samples, app_->userConfig.msaaSampleCount());
            }
            std::fprintf(stdout,
                         "{\"event\":\"rt64_effective_filters\",\"msaa_samples\":%u,"
                         "\"presentation_filter\":%d,\"three_point_texture_filter\":%s}\n",
                         app_->userConfig.msaaSampleCount(),
                         static_cast<int>(app_->userConfig.filtering),
                         app_->userConfig.threePointFiltering ? "true" : "false");
            std::fflush(stdout);
        }
        chosen_api = ultramodern::renderer::GraphicsApi::Vulkan;
        if (setup_result != ultramodern::renderer::SetupResult::Success) {
            diagnostic_log().error("rt64_setup_failed");
            app_.reset();
        } else {
            diagnostic_log().gpu(app_->device->getDescription().name);
            std::fprintf(stdout,
                         "{\"event\":\"rt64_device_ready\",\"run_id\":\"%s\",\"api\":\"vulkan\"}\n",
                         run_id());
            std::fflush(stdout);
        }
    }

    bool valid() override { return app_ != nullptr; }
    bool update_config(const ultramodern::renderer::GraphicsConfig& old_config,
                       const ultramodern::renderer::GraphicsConfig& new_config) override {
        if (app_ == nullptr) return false;
        // These controls still need their own RT64 integration. Do not report
        // them as applied just because the supported fields were copied.
        if (old_config.hr_option != new_config.hr_option ||
            old_config.api_option != new_config.api_option ||
            old_config.msaa_option != new_config.msaa_option ||
            old_config.hpfb_option != new_config.hpfb_option ||
            old_config.developer_mode != new_config.developer_mode) return false;

        auto updated = app_->userConfig;
        if (!apply_rt64_graphics_config(new_config, updated,
                                        internal_resolution_scale)) return false;
        const bool framebuffer_change =
            updated.resolution != app_->userConfig.resolution ||
            updated.resolutionMultiplier != app_->userConfig.resolutionMultiplier ||
            updated.aspectRatio != app_->userConfig.aspectRatio ||
            updated.extAspectRatio != app_->userConfig.extAspectRatio ||
            updated.aspectTarget != app_->userConfig.aspectTarget ||
            updated.extAspectTarget != app_->userConfig.extAspectTarget ||
            updated.downsampleMultiplier != app_->userConfig.downsampleMultiplier;
        const bool rate_change =
            updated.refreshRate != app_->userConfig.refreshRate ||
            updated.refreshRateTarget != app_->userConfig.refreshRateTarget;
        if (framebuffer_change || rate_change) {
            app_->userConfig = updated;
            app_->updateUserConfig(framebuffer_change);
        }
        host_graphics_ = new_config;
        return true;
    }
    void enable_instant_present() override {
        app_->enhancementConfig.presentation.mode =
            RT64::EnhancementConfiguration::Presentation::Mode::PresentEarly;
        app_->updateEnhancementConfig();
    }
    void send_dl(const OSTask* task) override {
        std::int16_t scene_gate = 0;
        std::memcpy(&scene_gate, app_->core.RDRAM + (0x000E121Cu ^ 2u),
                    sizeof(scene_gate));
        const bool next_menu_path = scene_gate >= 0;
        if (!scene_seen_ || next_menu_path != menu_path_) {
            scene_seen_ = true;
            menu_path_ = next_menu_path;
            std::fprintf(stdout,
                         "{\"event\":\"guest_scene_hint\",\"run_id\":\"%s\","
                         "\"raw\":%d,\"menu_path\":%s}\n",
                         run_id(), static_cast<int>(scene_gate),
                         menu_path_ ? "true" : "false");
            std::fflush(stdout);
            if (host_graphics_.ar_option == ultramodern::renderer::AspectRatio::Manual) {
                std::fprintf(stdout,
                             "{\"event\":\"graphics_effective_aspect\","
                             "\"run_id\":\"%s\",\"mode\":\"%s\"}\n",
                             run_id(), "16:9");
                std::fflush(stdout);
            }
        }
        app_->state->rsp->reset();
        app_->interpreter->loadUCodeGBI(task->t.ucode & 0x3FFFFFFu,
                                       task->t.ucode_data & 0x3FFFFFFu, true);
        if (display_list_count.load() == 0) {
            const auto segment_zero = app_->state->rsp->segments[0];
            const bool extended = app_->state->extended.extendRDRAM;
            if (segment_zero != 0 || extended) {
                diagnostic_log().error("rt64_memory_mode_invalid");
                std::fprintf(stderr,
                             "{\"event\":\"fatal_rt64_memory_mode\","
                             "\"segment_zero\":\"0x%08X\",\"extended\":%s}\n",
                             segment_zero, extended ? "true" : "false");
                std::exit(70);
            }
            std::fprintf(stdout,
                         "{\"event\":\"rt64_memory_mode\","
                         "\"segment_zero\":\"0x00000000\",\"extended\":false,"
                         "\"rdram_backing_bytes\":9437184}\n");
            std::fflush(stdout);
        }
        {
            // Compare producer bytes before the rendering fixes temporarily
            // mutate RDRAM commands; the producer fingerprint is pre-patch.
            {
                const std::lock_guard<std::mutex> lock(ui_tag_mutex);
                ui_active_batch = ui_tag_queue.take(task->t.data_ptr & 0x00FFFFFFu,
                                                    task->t.data_size,
                                                    app_->core.RDRAM, 9437184);
            }
            F3DMedalAlphaPatch medal_alpha(app_->core.RDRAM,
                                           9437184, task->t.data_ptr,
                                           task->t.data_size);
            F3DFlatShadePatch flat_shade(app_->core.RDRAM, 9437184,
                                         task->t.data_ptr,
                                         task->t.data_size);
            if (!ui_active_batch.spans.empty()) {
                static std::atomic<unsigned> logged{0};
                if (logged.fetch_add(1, std::memory_order_relaxed) < 8) {
                    std::fprintf(stderr,
                        "{\"event\":\"rt64_probe_ui_tag_task\","
                        "\"first\":%u,\"size\":%u,\"generation\":%llu,"
                        "\"spans\":%zu}\n",
                        task->t.data_ptr & 0x00FFFFFFu, task->t.data_size,
                        static_cast<unsigned long long>(ui_active_batch.task_generation),
                        ui_active_batch.spans.size());
                }
            }
            ui_current_tag = 0;
            app_->processDisplayLists(app_->core.RDRAM,
                                      task->t.data_ptr & 0x3FFFFFFu, 0, true);
            ui_active_batch = {};
            ui_current_tag = 0;
            rescue_snapshot_source_workload.store(app_->state->workloadId,
                                                  std::memory_order_release);
            if (const auto sample = medal_trace_.observe(menu_path_, flat_shade.count())) {
                std::fprintf(stdout,
                             "{\"event\":\"menu_medal_trace\",\"run_id\":\"%s\","
                             "\"sequence\":%zu,\"patched_triangles\":%zu}\n",
                             run_id(), sample->sequence, sample->patched_triangles);
                std::fflush(stdout);
            }
            static std::size_t max_reported_flat_shade = 0;
            if (flat_shade.count() > max_reported_flat_shade) {
                max_reported_flat_shade = flat_shade.count();
                std::fprintf(stdout,
                             "{\"event\":\"f3d_flat_shade_patch\",\"triangles\":%zu,\"list_size\":%u}\n",
                             flat_shade.count(),
                             static_cast<unsigned>(task->t.data_size));
            }
        }
        const auto count = ++display_list_count;
        if (count <= 3) {
            std::fprintf(stdout,
                         "{\"event\":\"rt64_display_list\",\"run_id\":\"%s\",\"sequence\":%llu,"
                         "\"ucode\":\"0x%08X\",\"data\":\"0x%08X\"}\n",
                         run_id(), static_cast<unsigned long long>(count),
                         static_cast<unsigned>(task->t.ucode),
                         static_cast<unsigned>(task->t.data_ptr));
            std::fflush(stdout);
        }
    }
    void send_dummy_workload(std::uint32_t framebuffer) override {
        app_->state->listProcessBegin();
        app_->state->rdp->setColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                                        framebuffer);
        app_->state->rdp->setOtherMode(0x382C30, 0);
        app_->state->rdp->fillRect(0, 0, 320 << 2, 240 << 2);
        app_->state->fullSync();
        app_->state->listProcessEnd();
    }
    void update_screen() override {
        app_->updateScreen();
        if (display_list_count.load() != 0) {
            const auto count = ++present_count;
            if (count <= 3) {
                std::fprintf(stdout,
                             "{\"event\":\"rt64_present\",\"run_id\":\"%s\",\"sequence\":%llu}\n",
                             run_id(), static_cast<unsigned long long>(count));
                std::fflush(stdout);
            }
        }
    }
    void shutdown() override { app_->end(); }
    std::uint32_t get_display_framerate() const override {
        return app_->presentQueue->ext.sharedResources->swapChainRate;
    }
    float get_resolution_scale() const override {
        return 1.0f;
    }

private:
    std::unique_ptr<RT64::Application> app_;
    ultramodern::renderer::GraphicsConfig host_graphics_{};
    bool scene_seen_ = false;
    bool menu_path_ = true;
    MenuMedalTrace medal_trace_{[] {
        const char* value = std::getenv("TETRISPHERE_TRACE_MENU_MEDALS");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }()};
};

} // namespace

void set_rt64_internal_resolution_scale(std::optional<double> multiplier) {
    internal_resolution_scale = multiplier;
}

void set_rt64_run_options(const RunOptions& options) {
    run_options = options;
}

std::unique_ptr<ultramodern::renderer::RendererContext> create_rt64_context(
    std::uint8_t* rdram, ultramodern::renderer::WindowHandle window,
    bool developer_mode) {
    return std::make_unique<Context>(rdram, window, developer_mode);
}

} // namespace tetrisphere
