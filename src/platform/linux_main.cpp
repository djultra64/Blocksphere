#include "tetrisphere/linux_platform.h"
#include "tetrisphere/data_directory.h"
#include "tetrisphere/audio_length_pacer.h"
#include "tetrisphere/controller_ports.h"
#include "tetrisphere/keyboard_state.h"
#include "tetrisphere/port_input.h"
#include "tetrisphere/qa_virtual_controllers.h"
#include "tetrisphere/audio_output.h"
#include "tetrisphere/diagnostic_log.h"
#include "tetrisphere/close_game_menu.h"

#include "tetrisphere/rt64_context.h"
#include "tetrisphere/run_options.h"
#include "tetrisphere/run_identity.h"
#include "tetrisphere/runtime.h"
#include "tetrisphere/settings_store.h"
#include "tetrisphere/wav_capture.h"
#include "tetrisphere/window_mode_controller.h"

#include "librecomp/rsp.hpp"
#include "ultramodern/input.hpp"
#include "ultramodern/ultramodern.hpp"
#define XXH_INLINE_ALL
#include "xxhash.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#ifdef _WIN32
#include <SDL_syswm.h>
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

RspExitReason tetrisphere_audio_rsp(std::uint8_t*, std::uint32_t);
extern std::atomic_bool exited;
extern std::atomic<std::uint64_t> tetrisphere_ai_attempts;
extern std::atomic<std::uint64_t> tetrisphere_ai_fifo_rejections;
extern std::atomic<std::uint64_t> tetrisphere_ai_fifo_wait_events;
extern std::atomic<std::uint64_t> tetrisphere_ai_fifo_waits_ms;
extern std::atomic<std::uint64_t> tetrisphere_ai_length_caps;
extern moodycamel::LightweightSemaphore graphics_shutdown_ready;

namespace tetrisphere {
namespace {

SDL_Window* window = nullptr;
ControllerPorts controller_ports;
QaVirtualControllers qa_virtual_controllers;
bool qa_controllers_active = false;
KeyboardState keyboard_state;
std::mutex input_snapshot_mutex;
std::array<ControllerPortSnapshot, 2> input_snapshot{};
std::uint16_t keyboard_snapshot = 0;
AudioOutput audio_output;
std::mutex audio_mutex;
WavCapture audio_capture;
std::atomic<std::uint64_t> observed_vis{0};
std::atomic<std::uint64_t> input_polls{0};
std::atomic<std::uint64_t> audio_rsp_tasks{0};
std::atomic<std::uint64_t> audio_rsp_total_us{0};
std::atomic<std::uint64_t> audio_rsp_max_us{0};

std::atomic<ControllerFamily> active_family{ControllerFamily::Keyboard};
std::atomic<ControllerFamily> active_p2_family{ControllerFamily::Keyboard};
Settings active_settings{};
SDL_Rect windowed_bounds{};
std::unique_ptr<WindowModeController> window_mode_controller;

void remember_windowed_bounds() {
    SDL_GetWindowPosition(window, &windowed_bounds.x, &windowed_bounds.y);
    SDL_GetWindowSize(window, &windowed_bounds.w, &windowed_bounds.h);
}

bool enter_fullscreen(const char*& kind) {
    bool exact_uhd = false;
    if (active_settings.graphics.resolution == GraphicsResolution::P2160) {
        const int window_display = SDL_GetWindowDisplayIndex(window);
        SDL_DisplayMode selected{};
        int selected_display = -1;
        int best_difference = 1000;
        for (int display = 0; display < SDL_GetNumVideoDisplays(); ++display) {
            SDL_DisplayMode current{};
            const int current_refresh = SDL_GetCurrentDisplayMode(display, &current) == 0
                ? current.refresh_rate : 60;
            for (int index = 0; index < SDL_GetNumDisplayModes(display); ++index) {
                SDL_DisplayMode candidate{};
                if (SDL_GetDisplayMode(display, index, &candidate) != 0 ||
                    candidate.w != 3840 || candidate.h != 2160) continue;
                const int difference = std::abs(candidate.refresh_rate - current_refresh);
                if (selected_display < 0 ||
                    (display == window_display && selected_display != window_display) ||
                    ((display == window_display) == (selected_display == window_display) &&
                     difference < best_difference)) {
                    selected = candidate;
                    selected_display = display;
                    best_difference = difference;
                }
            }
        }
        if (selected_display >= 0) {
            SDL_SetWindowPosition(window,
                                  SDL_WINDOWPOS_CENTERED_DISPLAY(selected_display),
                                  SDL_WINDOWPOS_CENTERED_DISPLAY(selected_display));
        }
        if (selected_display >= 0 && SDL_SetWindowDisplayMode(window, &selected) == 0 &&
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN) == 0) {
            SDL_PumpEvents();
            SDL_DisplayMode applied{};
            int drawable_width = 0;
            int drawable_height = 0;
            SDL_Vulkan_GetDrawableSize(window, &drawable_width, &drawable_height);
            exact_uhd = SDL_GetWindowDisplayIndex(window) == selected_display &&
                SDL_GetCurrentDisplayMode(selected_display, &applied) == 0 &&
                applied.w == 3840 && applied.h == 2160 &&
                drawable_width == 3840 && drawable_height == 2160;
            if (exact_uhd) kind = "exclusive-uhd";
        }
    }
    if (!exact_uhd) {
        if (SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
            return false;
        kind = "desktop";
    }
    return true;
}

void report_graphics_surface(const char* fullscreen_kind) {
    int actual_window_width = 0;
    int actual_window_height = 0;
    int drawable_width = 0;
    int drawable_height = 0;
    SDL_GetWindowSize(window, &actual_window_width, &actual_window_height);
    SDL_Vulkan_GetDrawableSize(window, &drawable_width, &drawable_height);
    SDL_DisplayMode active_display{};
    const int display_index = SDL_GetWindowDisplayIndex(window);
    if (display_index >= 0) SDL_GetCurrentDisplayMode(display_index, &active_display);
    const auto requested_internal_scale =
        graphics_internal_scale(active_settings.graphics.resolution);
    std::fprintf(stdout,
                 "{\"event\":\"graphics_surface_dimensions\","
                 "\"run_id\":\"%s\",\"window_width\":%d,\"window_height\":%d,"
                 "\"drawable_width\":%d,\"drawable_height\":%d,"
                 "\"requested_internal_height\":%d,\"aspect\":\"%s\","
                 "\"window_mode\":\"%s\",\"fullscreen_kind\":\"%s\","
                 "\"display_index\":%d,\"display_width\":%d,\"display_height\":%d}\n",
                 run_id(), actual_window_width, actual_window_height,
                 drawable_width, drawable_height,
                 requested_internal_scale
                     ? static_cast<int>(*requested_internal_scale * 240.0) : 0,
                 graphics_aspect_name(active_settings.graphics.aspect),
                 graphics_window_mode_name(active_settings.graphics.window_mode),
                 fullscreen_kind, display_index, active_display.w, active_display.h);
    std::fflush(stdout);
}

bool apply_window_mode(bool fullscreen) {
    if (fullscreen) remember_windowed_bounds();
    const char* kind = fullscreen ? "desktop" : "none";
    if (fullscreen) {
        if (!enter_fullscreen(kind)) {
            SDL_SetWindowFullscreen(window, 0);
            SDL_SetWindowPosition(window, windowed_bounds.x, windowed_bounds.y);
            SDL_SetWindowSize(window, windowed_bounds.w, windowed_bounds.h);
            return false;
        }
    } else {
        if (SDL_SetWindowFullscreen(window, 0) != 0) return false;
        SDL_SetWindowDisplayMode(window, nullptr);
        SDL_SetWindowSize(window, windowed_bounds.w, windowed_bounds.h);
        SDL_SetWindowPosition(window, windowed_bounds.x, windowed_bounds.y);
    }
    active_settings.graphics.window_mode = fullscreen
        ? GraphicsWindowMode::Fullscreen : GraphicsWindowMode::Windowed;
    auto config = ultramodern::renderer::get_graphics_config();
    config.wm_option = fullscreen ? ultramodern::renderer::WindowMode::Fullscreen
                                  : ultramodern::renderer::WindowMode::Windowed;
    ultramodern::renderer::set_graphics_config(config);
    report_graphics_surface(kind);
    return true;
}

const char* family_override() {
    const char* temporary = std::getenv("TETRISPHERE_CONTROLLER_FAMILY");
    return temporary == nullptr ? active_settings.family_override.c_str() : temporary;
}

void poll_input() {
    input_polls.fetch_add(1, std::memory_order_relaxed);
    const auto controllers = controller_ports.snapshot();
    const auto keys = keyboard_state.buttons();
    std::lock_guard lock(input_snapshot_mutex);
    input_snapshot = controllers;
    keyboard_snapshot = keys;
}

void count_vi() {
    observed_vis.fetch_add(1, std::memory_order_relaxed);
}

bool get_input(int port, std::uint16_t* buttons, float* x, float* y) {
    if (port < 0 || port >= 2) return false;
    // The runtime asks for ports in order. Freeze both readings together so
    // an SDL event between get_input(0) and get_input(1) cannot mix cycles.
    struct ReadCycle {
        std::array<ControllerPortSnapshot, 2> controllers{};
        std::uint16_t keys = 0;
        bool valid = false;
    };
    thread_local ReadCycle cycle;
    if (port == 0 || !cycle.valid) {
        std::lock_guard lock(input_snapshot_mutex);
        cycle.controllers = input_snapshot;
        cycle.keys = keyboard_snapshot;
        cycle.valid = true;
    }
    const auto& controller = cycle.controllers[port];
    const auto keys = cycle.keys;
    if (port == 1) cycle.valid = false;
    if (port == 1 && !controller.connected) return false;
    const auto mapped = map_port_input(port, controller, keys,
                                       controller_bindings(), magic_control());
    *buttons = mapped.buttons;
    *x = mapped.x;
    *y = mapped.y;
    static std::array<std::uint16_t, 2> previous{};
    if (mapped.buttons != previous[port]) {
        const auto family = port == 0 ? active_family.load() : active_p2_family.load();
        std::fprintf(stdout,
                     "{\"event\":\"logical_input_sample\",\"run_id\":\"%s\",\"port\":%d,\"family\":\"%s\","
                     "\"buttons\":\"0x%04X\",\"prompt_confirm\":\"%s\"}\n",
                     run_id(), port, controller_family_name(family),
                     static_cast<unsigned>(mapped.buttons),
                     logical_action_label(family, controller_bindings(),
                                          LogicalAction::Confirm));
        std::fflush(stdout);
        previous[port] = mapped.buttons;
    }
    return true;
}

void set_rumble(int port, bool enabled) {
    controller_ports.rumble(port, enabled);
}

ultramodern::input::connected_device_info_t device_info(int port) {
    if (port < 0 || port >= 2)
        return {ultramodern::input::Device::None, ultramodern::input::Pak::None};
    std::lock_guard lock(input_snapshot_mutex);
    if (input_snapshot[port].connected)
        return {ultramodern::input::Device::Controller,
                ultramodern::input::Pak::RumblePak};
    if (port == 0)
        return {ultramodern::input::Device::Controller,
                ultramodern::input::Pak::None};
    return {ultramodern::input::Device::None, ultramodern::input::Pak::None};
}

void set_audio_frequency(std::uint32_t frequency) {
    std::lock_guard lock(audio_mutex);
    std::string error;
    if (!audio_output.open(frequency, error)) {
        diagnostic_log().error("audio_open_failed");
        std::fprintf(stderr, "{\"event\":\"fatal_audio\",\"detail\":\"%s\"}\n",
                     error.c_str());
        std::exit(70);
    }
    const char* capture_path = std::getenv("TETRISPHERE_AUDIO_CAPTURE_WAV");
    if (capture_path != nullptr && *capture_path != '\0' && !audio_capture.active()) {
        if (!audio_capture.open(capture_path, frequency)) {
            std::fprintf(stderr,
                         "{\"event\":\"fatal_audio_capture\","
                         "\"detail\":\"cannot_open_wav\"}\n");
            std::exit(70);
        }
        std::fprintf(stdout,
                     "{\"event\":\"audio_capture_started\","
                     "\"source\":\"queued_to_sdl_device\",\"frequency\":%u}\n",
                     frequency);
        std::fflush(stdout);
    }
}

void queue_audio(std::int16_t* samples, std::size_t count) {
    std::lock_guard lock(audio_mutex);
    std::string error;
    if (!audio_output.queue(samples, count, error)) {
        diagnostic_log().error("audio_queue_failed");
        std::fprintf(stderr,
                     "{\"event\":\"fatal_audio_queue\",\"detail\":\"%s\","
                     "\"sample_count\":%zu}\n", error.c_str(), count);
        std::exit(70);
    }
    if (audio_capture.active() && !audio_capture.append(samples, count)) {
        std::fprintf(stderr,
                     "{\"event\":\"fatal_audio_capture\","
                     "\"detail\":\"wav_write_failed\"}\n");
        std::exit(70);
    }
}

std::size_t audio_frames_remaining() {
    return audio_output.frames_remaining();
}

void rsp_init() { recomp::rsp::constants_init(); }

RspUcodeFunc* select_rsp_ucode(const OSTask* task) {
    constexpr std::uint32_t audio_ucode = 0x800DE7D0u;
    return task->t.type == M_AUDTASK &&
           static_cast<std::uint32_t>(task->t.ucode) == audio_ucode
        ? tetrisphere_audio_rsp : nullptr;
}

bool run_rsp(std::uint8_t* rdram, const OSTask* task) {
    constexpr std::uint32_t audio_ucode = 0x800DE7D0u;
    constexpr std::size_t audio_ucode_size = 0xDF0;
    constexpr std::uint64_t audio_ucode_hash = 0xE621BF918F29B135ULL;
    if (task->t.type != M_AUDTASK ||
        static_cast<std::uint32_t>(task->t.ucode) != audio_ucode ||
        XXH3_64bits(rdram + (audio_ucode & 0x7FFFFFu), audio_ucode_size) !=
            audio_ucode_hash) {
        return false;
    }
    recomp::rsp::set_callbacks({select_rsp_ucode});
    const auto start = std::chrono::steady_clock::now();
    const bool result = recomp::rsp::run_task(rdram, task);
    const auto elapsed_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count());
    audio_rsp_tasks.fetch_add(1, std::memory_order_relaxed);
    audio_rsp_total_us.fetch_add(elapsed_us, std::memory_order_relaxed);
    auto previous_max = audio_rsp_max_us.load(std::memory_order_relaxed);
    while (elapsed_us > previous_max && !audio_rsp_max_us.compare_exchange_weak(
               previous_max, elapsed_us, std::memory_order_relaxed)) {}
    return result;
}

void show_error(const char* message) {
    diagnostic_log().error("runtime_error");
    std::fprintf(stderr, "{\"event\":\"runtime_error\",\"detail\":\"%s\"}\n",
                 message == nullptr ? "unknown" : message);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Tetrisphere error",
        message == nullptr ? "Unknown error." : message, window);
}

} // namespace

extern "C" void tetrisphere_rt64_clear_keyboard_input() {
    SDL_Event focus_lost{};
    focus_lost.type = SDL_WINDOWEVENT;
    focus_lost.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    keyboard_state.handle_event(focus_lost);
    std::lock_guard lock(input_snapshot_mutex);
    keyboard_snapshot = 0;
}

ControllerFamily active_controller_family() { return active_family.load(); }

ControllerFamily active_controller_family_for_port(int port) {
    if (port == 0) return active_family.load();
    if (port == 1) return active_p2_family.load();
    return ControllerFamily::Keyboard;
}

bool linux_audio_fifo_full() {
    return audio_output.full();
}

std::size_t linux_audio_current_bytes() {
    return audio_output.current_bytes();
}

unsigned linux_audio_drain_wait_budget_ms() {
    const auto pacing = audio_output.pacing_snapshot();
    return audio_drain_wait_budget_ms(pacing.device_buffer_frames,
                                      pacing.obtained_frequency);
}

void initialize_linux_platform(std::uint8_t* rdram, const char* rom_sha256,
                               const RunOptions& run_options) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        show_error(SDL_GetError());
        std::exit(70);
    }
    const char* data_dir_override = std::getenv("TETRISPHERE_DATA_DIR");
    const auto user_path = application_data_directory();
    if (user_path.empty()) {
        show_error("Cannot determine the game data directory.");
        std::exit(74);
    }
    std::string eeprom_error;
    const auto eeprom_path = user_path / "tetrisphere-us-rev0.eep";
    if (!initialize_eeprom(eeprom_path, eeprom_error)) {
        show_error(eeprom_error.c_str());
        std::exit(74);
    }
    const auto settings_path = user_path / "settings.dat";
    SettingsStore settings_store(settings_path);
    std::string settings_error;
    if (!settings_store.load(active_settings, settings_error)) {
        show_error(settings_error.c_str());
        std::exit(74);
    }
    if (!std::filesystem::exists(settings_path) &&
        !settings_store.save(active_settings, settings_error)) {
        show_error(settings_error.c_str());
        std::exit(74);
    }
    if (run_options.fullscreen) {
        active_settings.graphics.window_mode = *run_options.fullscreen
            ? GraphicsWindowMode::Fullscreen : GraphicsWindowMode::Windowed;
    }
    set_controller_bindings(active_settings.bindings);
    set_magic_control(active_settings.magic);
    active_family.store(ControllerFamily::Keyboard);
    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto diagnostic_path = user_path / "diagnostics" /
        (std::to_string(timestamp) + "-" + run_id() + ".jsonl");
    std::string diagnostic_error;
    if (!diagnostic_log().start(diagnostic_path, run_id(), TETRISPHERE_BUILD_ID,
                                rom_sha256, active_settings, diagnostic_error)) {
        show_error(diagnostic_error.c_str());
        std::exit(74);
    }
    std::fprintf(stdout, "{\"event\":\"diagnostic_file_ready\",\"run_id\":\"%s\"}\n",
                 run_id());
    std::fflush(stdout);
    SDL_Rect usable_display{};
    if (SDL_GetDisplayUsableBounds(0, &usable_display) != 0)
        usable_display = SDL_Rect{0, 0, 960, 720};
    const auto [window_width, window_height] = graphics_window_size(
        active_settings.graphics, usable_display.w, usable_display.h);
    window = SDL_CreateWindow("Tetrisphere",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              window_width, window_height,
                              SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                                  SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == nullptr) {
        show_error(SDL_GetError());
        std::exit(70);
    }
    remember_windowed_bounds();
    const bool fullscreen_requested =
        active_settings.graphics.window_mode == GraphicsWindowMode::Fullscreen;
    const char* fullscreen_kind = "none";
    if (fullscreen_requested && !enter_fullscreen(fullscreen_kind)) {
        show_error(SDL_GetError());
        std::exit(70);
    }
    report_graphics_surface(fullscreen_kind);
    window_mode_controller = std::make_unique<WindowModeController>(
        fullscreen_requested, apply_window_mode);
    const char* qa_actions = std::getenv("TETRISPHERE_QA_CONTROLLERS");
    if (qa_actions != nullptr) {
        if (*qa_actions == '\0' || data_dir_override == nullptr ||
            *data_dir_override == '\0') {
            show_error("QA controllers require TETRISPHERE_DATA_DIR and an actions file");
            std::exit(74);
        }
        std::string qa_error;
        if (!qa_virtual_controllers.initialize(std::filesystem::u8path(qa_actions),
                                                user_path, qa_error) ||
            !controller_ports.set_preferred_ids(qa_virtual_controllers.instance_ids())) {
            qa_virtual_controllers.close();
            show_error(qa_error.empty() ? "QA controller port assignment failed" : qa_error.c_str());
            std::exit(74);
        }
        qa_controllers_active = true;
        const auto ids = qa_virtual_controllers.instance_ids();
        std::fprintf(stdout,
                     "{\"event\":\"qa_virtual_controllers\",\"run_id\":\"%s\","
                     "\"p1_instance_id\":%d,\"p2_instance_id\":%d}\n",
                     run_id(), static_cast<int>(ids[0]), static_cast<int>(ids[1]));
        std::fflush(stdout);
    }
    controller_ports.open_all();
    {
        const auto initial = controller_ports.snapshot();
        std::lock_guard lock(input_snapshot_mutex);
        input_snapshot = initial;
        keyboard_snapshot = keyboard_state.buttons();
        if (initial[0].connected)
            active_family.store(select_controller_family(initial[0].name.c_str(), family_override()));
        if (initial[1].connected)
            active_p2_family.store(select_controller_family(initial[1].name.c_str(), family_override()));
    }
    std::fprintf(stdout,
#ifdef _WIN32
                 "{\"event\":\"native_windows_started\",\"run_id\":\"%s\",\"backend\":\"rt64-vulkan\","
#else
                 "{\"event\":\"native_linux_started\",\"run_id\":\"%s\",\"backend\":\"rt64-vulkan\","
#endif
                 "\"controller_family\":\"%s\",\"prompt_confirm\":\"%s\"}\n",
                 run_id(),
                 controller_family_name(active_family.load()),
                 logical_action_label(active_family.load(), controller_bindings(),
                                      LogicalAction::Confirm));
    std::fflush(stdout);

    ultramodern::renderer::GraphicsConfig graphics{};
    graphics.developer_mode = false;
    graphics.res_option = ultramodern::renderer::Resolution::Auto;
    graphics.wm_option = fullscreen_requested
        ? ultramodern::renderer::WindowMode::Fullscreen
        : ultramodern::renderer::WindowMode::Windowed;
    graphics.hr_option = ultramodern::renderer::HUDRatioMode::Original;
    graphics.api_option = ultramodern::renderer::GraphicsApi::Vulkan;
    graphics.ar_option = active_settings.graphics.aspect == GraphicsAspect::Expand16x9
        ? ultramodern::renderer::AspectRatio::Manual
        : ultramodern::renderer::AspectRatio::Original;
    graphics.msaa_option = ultramodern::renderer::Antialiasing::None;
    set_rt64_run_options(resolve_graphics_run_options(active_settings.graphics, run_options));
    graphics.rr_option = active_settings.graphics.refresh == GraphicsRefresh::Original
        ? ultramodern::renderer::RefreshRate::Original
        : ultramodern::renderer::RefreshRate::Manual;
    graphics.hpfb_option = ultramodern::renderer::HighPrecisionFramebuffer::Auto;
    switch (active_settings.graphics.refresh) {
        case GraphicsRefresh::Fps30: graphics.rr_manual_value = 30; break;
        case GraphicsRefresh::Fps60: graphics.rr_manual_value = 60; break;
        case GraphicsRefresh::Fps120: graphics.rr_manual_value = 120; break;
        case GraphicsRefresh::Original: graphics.rr_manual_value = 60; break;
    }
    graphics.ds_option = 1;
    set_rt64_internal_resolution_scale(
        graphics_internal_scale(active_settings.graphics.resolution));
    ultramodern::renderer::set_graphics_config(graphics);

    ultramodern::set_callbacks(
        {rsp_init, run_rsp}, {create_rt64_context},
        {queue_audio, audio_frames_remaining, set_audio_frequency},
        {poll_input, get_input, set_rumble, device_info},
        {}, {count_vi, nullptr}, {show_error}, {});
#ifdef _WIN32
    SDL_SysWMinfo window_info{};
    SDL_VERSION(&window_info.version);
    if (SDL_GetWindowWMInfo(window, &window_info) != SDL_TRUE) {
        show_error(SDL_GetError());
        std::exit(70);
    }
    ultramodern::renderer::WindowHandle native_window{
        window_info.info.win.window, GetCurrentThreadId()};
    ultramodern::preinit(rdram, native_window);
#else
    ultramodern::preinit(rdram, window);
#endif
}

[[noreturn]] void close_game() {
    exited.store(true);
    graphics_shutdown_ready.signal();
    ultramodern::join_event_threads();
    ultramodern::join_thread_cleaner_thread();
    // RT64's event-filter userdata dies with its rendering thread. Controller
    // teardown can emit SDL events, so remove the filter before closing devices.
    SDL_SetEventFilter(nullptr, nullptr);
    controller_ports.close();
    qa_virtual_controllers.close();
    audio_output.close();
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::exit(0);
}

[[noreturn]] void run_linux_event_loop() {
    SDL_Event event{};
    const auto event_loop_start = std::chrono::steady_clock::now();
    auto next_audio_report = event_loop_start + std::chrono::seconds(10);
    const char* audio_metrics = std::getenv("TETRISPHERE_AUDIO_METRICS");
    const bool report_audio = audio_metrics != nullptr && std::strcmp(audio_metrics, "1") == 0;
    for (;;) {
        if (take_close_game_request()) {
            std::fprintf(stdout, "{\"event\":\"close_game_requested\"}\n");
            std::fflush(stdout);
            close_game();
        }
        if (qa_controllers_active) {
            std::vector<QaControllerAction> applied;
            std::string qa_error;
            if (!qa_virtual_controllers.pump(applied, qa_error)) {
                std::fprintf(stderr, "QA controller input failed: %s\n", qa_error.c_str());
                std::fflush(stderr);
                controller_ports.close();
                qa_virtual_controllers.close();
                std::exit(74);
            }
            for (const auto& action : applied) {
                std::fprintf(stdout,
                             "{\"event\":\"qa_virtual_action\",\"run_id\":\"%s\","
                             "\"seq\":%llu,\"port\":%d,\"button\":%d,\"pressed\":%s}\n",
                             run_id(), static_cast<unsigned long long>(action.seq),
                             action.port, static_cast<int>(action.button),
                             action.pressed ? "true" : "false");
            }
            if (!applied.empty()) std::fflush(stdout);
        }
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                close_game();
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                window_mode_controller->focus_lost();
            }
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                FullscreenKey key = FullscreenKey::Other;
                if (event.key.keysym.scancode == SDL_SCANCODE_F11)
                    key = FullscreenKey::F11;
                else if (event.key.keysym.scancode == SDL_SCANCODE_RETURN ||
                         event.key.keysym.scancode == SDL_SCANCODE_KP_ENTER)
                    key = FullscreenKey::Enter;
                if (window_mode_controller->handle_key(
                        key, event.type == SDL_KEYDOWN, event.key.repeat != 0,
                        (event.key.keysym.mod & KMOD_ALT) != 0)) continue;
            }
            const auto before_removal = event.type == SDL_CONTROLLERDEVICEREMOVED
                ? controller_ports.snapshot()
                : std::array<ControllerPortSnapshot, 2>{};
            const int active_port = controller_ports.handle_event(event);
            if (active_port >= 0) {
                const auto current = controller_ports.snapshot();
                const auto family = select_controller_family(current[active_port].name.c_str(),
                                                             family_override());
                if (active_port == 0) active_family.store(family);
                else active_p2_family.store(family);
            }
            std::string audio_error;
            if (audio_output.handle_event(event, audio_error)) {
                diagnostic_log().audio(audio_error.empty() ? "reopened" : "unavailable");
                std::fprintf(audio_error.empty() ? stdout : stderr,
                             "{\"event\":\"audio_device_change\",\"status\":\"%s\"}\n",
                             audio_error.empty() ? "reopened" : "unavailable");
                std::fflush(audio_error.empty() ? stdout : stderr);
            }
            if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                if (before_removal[0].instance_id == event.cdevice.which)
                    active_family.store(ControllerFamily::Keyboard);
                if (before_removal[1].instance_id == event.cdevice.which)
                    active_p2_family.store(ControllerFamily::Keyboard);
            }
            if (keyboard_state.handle_event(event)) {
                active_family.store(ControllerFamily::Keyboard);
            }
        }
        const auto now = std::chrono::steady_clock::now();
        if (report_audio && now >= next_audio_report) {
            const auto pacing = audio_output.pacing_snapshot();
            if (pacing.submissions != 0) {
                const auto uptime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - event_loop_start).count();
                std::fprintf(stdout,
                             "{\"event\":\"audio_pacing\",\"run_id\":\"%s\","
                             "\"uptime_ms\":%lld,\"requested_hz\":%u,\"obtained_hz\":%u,"
                             "\"device_buffer_frames\":%u,\"vis\":%llu,\"input_polls\":%llu,"
                             "\"audio_rsp_tasks\":%llu,\"audio_rsp_total_us\":%llu,\"audio_rsp_max_us\":%llu,"
                             "\"ai_attempts\":%llu,\"ai_fifo_rejections\":%llu,"
                             "\"ai_fifo_wait_events\":%llu,\"ai_fifo_waits_ms\":%llu,"
                             "\"ai_length_caps\":%llu,\"submissions\":%llu,"
                             "\"submitted_frames\":%llu,\"empty_software_queue_count\":%llu,"
                             "\"queued_frames\":%zu}\n",
                             run_id(), static_cast<long long>(uptime_ms),
                             pacing.requested_frequency, pacing.obtained_frequency,
                             static_cast<unsigned>(pacing.device_buffer_frames),
                             static_cast<unsigned long long>(observed_vis.load(std::memory_order_relaxed)),
                             static_cast<unsigned long long>(input_polls.load(std::memory_order_relaxed)),
                             static_cast<unsigned long long>(audio_rsp_tasks.load(std::memory_order_relaxed)),
                             static_cast<unsigned long long>(audio_rsp_total_us.load(std::memory_order_relaxed)),
                             static_cast<unsigned long long>(audio_rsp_max_us.load(std::memory_order_relaxed)),
                             static_cast<unsigned long long>(tetrisphere_ai_attempts.load(std::memory_order_relaxed)),
                             static_cast<unsigned long long>(tetrisphere_ai_fifo_rejections.load(std::memory_order_relaxed)),
                             static_cast<unsigned long long>(tetrisphere_ai_fifo_wait_events.load(std::memory_order_relaxed)),
                             static_cast<unsigned long long>(tetrisphere_ai_fifo_waits_ms.load(std::memory_order_relaxed)),
                             static_cast<unsigned long long>(tetrisphere_ai_length_caps.load(std::memory_order_relaxed)),
                             static_cast<unsigned long long>(pacing.submissions),
                             static_cast<unsigned long long>(pacing.submitted_frames),
                             static_cast<unsigned long long>(pacing.empty_software_queue_count),
                             pacing.current_software_queue_frames);
                std::fflush(stdout);
            }
            next_audio_report = now + std::chrono::seconds(10);
        }
        SDL_Delay(2);
    }
}

} // namespace tetrisphere
