#include "tetrisphere/runtime.h"
#include "tetrisphere/linux_platform.h"
#include "tetrisphere/wav_capture.h"
#include "recomp.h"
#include "ultramodern/input.hpp"
#include "ultramodern/rsp.hpp"
#include "ultramodern/ultramodern.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

extern "C" void tetrisphere_osPiRawReadIo(std::uint8_t*, recomp_context*);
extern "C" void osEepromProbe_recomp(std::uint8_t*, recomp_context*);
extern "C" void osEepromRead_recomp(std::uint8_t*, recomp_context*);
extern "C" void osEepromWrite_recomp(std::uint8_t*, recomp_context*);
extern "C" void tetrisphere_wait_retraces(std::uint8_t*, recomp_context*);

std::size_t audio_samples = 0;
std::uint32_t audio_frequency = 0;
bool rsp_called = false;

void queue_samples(std::int16_t*, std::size_t count) { audio_samples += count; }
std::size_t remaining_frames() { return 0; }
void set_frequency(std::uint32_t frequency) { audio_frequency = frequency; }
void rsp_init() {}
bool run_rsp_task(std::uint8_t*, const OSTask* task) {
    rsp_called = task->t.type == M_GFXTASK;
    return rsp_called;
}

int fail(const std::string& message) {
    std::cerr << message << '\n';
    return 1;
}

int verify_registry(const char* table_path) {
    using tetrisphere::RuntimeCategory;
    using tetrisphere::RuntimeStatus;

    const std::array required_categories{
        RuntimeCategory::Boot,
        RuntimeCategory::Thread,
        RuntimeCategory::MessageQueue,
        RuntimeCategory::Timer,
        RuntimeCategory::PiDma,
        RuntimeCategory::Controller,
        RuntimeCategory::Vi,
        RuntimeCategory::Rsp,
        RuntimeCategory::Audio,
    };
    std::set<RuntimeCategory> seen;
    for (const auto& binding : tetrisphere::runtime_bindings()) {
        seen.insert(binding.category);
        if (binding.name.empty() || binding.provenance.empty()) {
            return fail("binding missing name or provenance");
        }
        if (binding.status == RuntimeStatus::Unsupported && binding.detail.empty()) {
            return fail("unsupported binding missing diagnostic reason");
        }
        if (!binding.has_guest_address && binding.status == RuntimeStatus::Implemented) {
            return fail("unmapped binding must not be marked implemented");
        }
        if (!binding.has_guest_address &&
            (binding.status != RuntimeStatus::Unsupported || !binding.host_binding.empty())) {
            return fail("unmapped guest route must be unsupported without a host binding");
        }
    }
    for (const auto category : required_categories) {
        if (!seen.contains(category)) {
            return fail("required runtime category missing");
        }
    }

    constexpr std::array connected{
        "osCreateMesgQueue", "osSetTimer", "osPiStartDma", "osContInit",
        "osCreateViManager", "osSpTaskStartGo", "osAiSetNextBuffer",
    };
    for (const auto name : connected) {
        const auto binding = std::find_if(
            tetrisphere::runtime_bindings().begin(),
            tetrisphere::runtime_bindings().end(),
            [name](const auto& candidate) { return candidate.name == name; });
        if (binding == tetrisphere::runtime_bindings().end() ||
            !binding->has_guest_address || binding->address == 0u ||
            binding->status == RuntimeStatus::Unsupported ||
            binding->host_binding.empty()) {
            return fail(std::string("connected runtime route is stale: ") + name);
        }
    }

    const auto* init = tetrisphere::find_runtime_binding(0x800D1DC0u);
    const auto* pio = tetrisphere::find_runtime_binding(0x800D2050u);
    const auto* create = tetrisphere::find_runtime_binding(0x800D1250u);
    const auto* start = tetrisphere::find_runtime_binding(0x800D13A0u);
    const auto* yield = tetrisphere::find_runtime_binding(0x800D1D40u);
    if (init == nullptr || init->name != "osInitialize" ||
        init->status != RuntimeStatus::Implemented) {
        return fail("osInitialize binding is not implemented at the observed address");
    }
    if (pio == nullptr || pio->name != "osPiRawReadIo" ||
        pio->status != RuntimeStatus::Implemented) {
        return fail("osPiRawReadIo binding is not implemented at the observed address");
    }
    if (create == nullptr || create->name != "osCreateThread" ||
        create->status != RuntimeStatus::Delegated) {
        return fail("osCreateThread binding is not delegated at the observed address");
    }
    if (start == nullptr || start->name != "osStartThread" ||
        start->status != RuntimeStatus::Delegated) {
        return fail("osStartThread binding is not delegated at the observed address");
    }
    if (yield == nullptr || yield->name != "osSpTaskYield" ||
        yield->host_binding != "osSpTaskYield_recomp" ||
        yield->status != RuntimeStatus::Implemented) {
        return fail("osSpTaskYield is not mapped to the host RSP task contract");
    }

    std::ifstream input(table_path, std::ios::binary);
    const std::string serialized(std::istreambuf_iterator<char>(input), {});
    std::string schema_error;
    if (!tetrisphere::validate_runtime_table_json(serialized, schema_error)) {
        return fail("runtime-symbols.json rejected: " + schema_error);
    }
    return 0;
}

int verify_pio(const char* rom_path) {
    std::ifstream input(rom_path, std::ios::binary);
    std::vector<std::uint8_t> rom(std::istreambuf_iterator<char>(input), {});
    if (rom.size() != 8u * 1024u * 1024u) {
        return fail("PIO fixture is not the private 8 MiB ROM");
    }
    std::uint32_t value = 0;
    if (!tetrisphere::pi_raw_read_io(rom, 0x00FFB000u, value)) {
        return fail("mirrored cartridge PIO read was rejected");
    }
    const std::size_t offset = 0x7FB000u;
    const std::uint32_t expected = (std::uint32_t{rom[offset]} << 24u)
                                 | (std::uint32_t{rom[offset + 1]} << 16u)
                                 | (std::uint32_t{rom[offset + 2]} << 8u)
                                 | std::uint32_t{rom[offset + 3]};
    if (value != expected) {
        return fail("mirrored cartridge PIO returned the wrong big-endian word");
    }
    if (tetrisphere::pi_raw_read_io(rom, 0x00FFB002u, value)) {
        return fail("unaligned cartridge PIO must be rejected explicitly");
    }
    if (tetrisphere::pi_raw_read_io(rom, 0x00001000u, value)) {
        return fail("unobserved cartridge PIO range must remain unsupported");
    }
    return 0;
}

int verify_initial_image_layout() {
    const std::array<std::uint8_t, 8> rom{0x12, 0x34, 0x56, 0x78,
                                          0x9A, 0xBC, 0xDE, 0xF0};
    std::array<std::uint8_t, 12> rdram{};
    if (!tetrisphere::load_initial_image(rom, rdram, 0, 4, rom.size())) {
        return fail("valid initial image was rejected");
    }
    const auto first = *reinterpret_cast<const std::uint32_t*>(rdram.data() + 4);
    const auto second = *reinterpret_cast<const std::uint32_t*>(rdram.data() + 8);
    if (first != 0x12345678u || second != 0x9ABCDEF0u) {
        return fail("initial image is not in N64Recomp word-swapped layout");
    }
    if (tetrisphere::load_initial_image(rom, rdram, 0, 3, 4)) {
        return fail("unaligned initial image destination was accepted");
    }
    return 0;
}

int verify_ipl3_state() {
    std::vector<std::uint8_t> memory(8u * 1024u * 1024u);
    std::uint8_t* rdram = memory.data();
    tetrisphere::initialize_ipl3_state(rdram);
    if (static_cast<std::uint32_t>(MEM_W(0, 0xFFFFFFFF80000300ull)) != 1u ||
        static_cast<std::uint32_t>(MEM_W(0, 0xFFFFFFFF80000308ull)) != 0xB0000000u ||
        static_cast<std::uint32_t>(MEM_W(0, 0xFFFFFFFF8000030Cull)) != 0u ||
        static_cast<std::uint32_t>(MEM_W(0, 0xFFFFFFFF80000318ull)) != 8u * 1024u * 1024u) {
        return fail("IPL3 compatibility state does not describe NTSC 8 MiB hardware");
    }
    return 0;
}

int verify_eeprom_probe() {
    recomp_context context{};
    osEepromProbe_recomp(nullptr, &context);
    if (context.r2 != 1) {
        return fail("NTPE EEPROM probe did not report the required 4-Kbit device");
    }
    return 0;
}

int verify_eeprom_roundtrip() {
    std::vector<std::uint8_t> memory(8u * 1024u * 1024u);
    std::uint8_t* rdram = memory.data();
    constexpr gpr buffer = 0xFFFFFFFF80001000ull;
    constexpr std::array<std::uint8_t, 8> expected{
        0x54, 0x45, 0x54, 0x52, 0x49, 0x53, 0x50, 0x48,
    };
    for (std::size_t index = 0; index < expected.size(); ++index) {
        MEM_B(index, buffer) = expected[index];
    }

    recomp_context context{};
    context.r5 = 3;
    context.r6 = buffer;
    osEepromWrite_recomp(rdram, &context);
    if (static_cast<std::int32_t>(context.r2) != 0) {
        return fail("valid 4-Kbit EEPROM write failed");
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        MEM_B(index, buffer) = 0;
    }
    osEepromRead_recomp(rdram, &context);
    if (static_cast<std::int32_t>(context.r2) != 0) {
        return fail("valid 4-Kbit EEPROM read failed");
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (MEM_B(index, buffer) != expected[index]) {
            return fail("4-Kbit EEPROM did not preserve an eight-byte block");
        }
    }

    context.r5 = 64;
    osEepromRead_recomp(rdram, &context);
    if (static_cast<std::int32_t>(context.r2) != -1) {
        return fail("out-of-range 4-Kbit EEPROM block was accepted");
    }
    return 0;
}

int verify_eeprom_persistence(const char* directory) {
    const auto path = std::filesystem::path(directory) / "game.eep";
    std::string error;
    if (!tetrisphere::initialize_eeprom(path, error)) {
        return fail("could not initialize EEPROM: " + error);
    }
    std::vector<std::uint8_t> memory(8u * 1024u * 1024u);
    std::uint8_t* rdram = memory.data();
    constexpr gpr buffer = 0xFFFFFFFF80001000ull;
    constexpr std::array<std::uint8_t, 8> expected{1, 2, 3, 4, 5, 6, 7, 8};
    for (std::size_t i = 0; i < expected.size(); ++i) MEM_B(i, buffer) = expected[i];
    recomp_context context{};
    context.r5 = 7;
    context.r6 = buffer;
    osEepromWrite_recomp(rdram, &context);
    if (context.r2 != 0) return fail("EEPROM write rejected valid block");
    if (!tetrisphere::initialize_eeprom(path, error)) {
        return fail("could not reload EEPROM: " + error);
    }
    for (std::size_t i = 0; i < expected.size(); ++i) MEM_B(i, buffer) = 0;
    osEepromRead_recomp(rdram, &context);
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (MEM_B(i, buffer) != expected[i]) {
            return fail("EEPROM write did not survive runtime reinitialization");
        }
    }
    return 0;
}

int verify_sdl_audio_pacing() {
    tetrisphere::AudioDmaFifo fifo;
    if (!fifo.submit(992) || fifo.full() || fifo.current_frames() != 992) {
        return fail("first AI DMA was not tracked as the current buffer");
    }
    fifo.synchronize(600);
    if (fifo.current_frames() != 600 || !fifo.submit(880) || !fifo.full()) {
        return fail("AI DMA did not preserve distinct current/next buffers");
    }
    if (fifo.submit(112)) {
        return fail("AI DMA accepted a third buffer while the FIFO was full");
    }
    if (fifo.current_frames() != 600) {
        return fail("AI FIFO head unexpectedly included the queued next buffer");
    }
    fifo.synchronize(700);
    if (fifo.current_frames() != 700 || fifo.full()) {
        return fail("AI DMA did not advance to the next buffer on completion");
    }
    return 0;
}

int verify_wav_capture(const char* output_path) {
    constexpr std::array<std::int16_t, 8> samples{
        100, -100, 200, -200, 300, -300, 400, -400,
    };
    {
        tetrisphere::WavCapture capture;
        if (!capture.open(output_path, 32'000) ||
            !capture.append(samples.data(), samples.size())) {
            return fail("PCM capture could not write queued SDL samples");
        }
    }
    std::ifstream input(output_path, std::ios::binary);
    const std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(input), {});
    if (bytes.size() != 44 + samples.size() * sizeof(std::int16_t) ||
        std::string(bytes.begin(), bytes.begin() + 4) != "RIFF" ||
        std::string(bytes.begin() + 8, bytes.begin() + 12) != "WAVE" ||
        std::string(bytes.begin() + 36, bytes.begin() + 40) != "data") {
        return fail("PCM capture did not produce a bounded WAV container");
    }
    auto little_u32 = [&bytes](std::size_t offset) {
        return std::uint32_t{bytes[offset]} |
               (std::uint32_t{bytes[offset + 1]} << 8u) |
               (std::uint32_t{bytes[offset + 2]} << 16u) |
               (std::uint32_t{bytes[offset + 3]} << 24u);
    };
    if (little_u32(24) != 32'000u || little_u32(40) != samples.size() * 2u) {
        return fail("PCM capture WAV header does not match queued samples");
    }
    return 0;
}

int verify_zero_retrace_wait() {
    std::vector<std::uint8_t> memory(8u * 1024u * 1024u);
    std::uint8_t* rdram = memory.data();
    recomp_context context{};
    context.r4 = 0;
    tetrisphere_wait_retraces(rdram, &context);
    constexpr gpr countdown = 0xFFFFFFFF800DFE90ull;
    if (MEM_H(0, countdown) != 0) {
        return fail("zero-retrace compatibility wait left a stale countdown");
    }
    return 0;
}

int verify_controller_prompts() {
    using tetrisphere::ControllerFamily;
    using tetrisphere::LogicalAction;
    using tetrisphere::PhysicalButton;
    if (tetrisphere::detect_controller_family("Xbox Wireless Controller") !=
            ControllerFamily::Xbox ||
        tetrisphere::detect_controller_family("DualSense Wireless Controller") !=
            ControllerFamily::PlayStation ||
        tetrisphere::detect_controller_family("Nintendo Switch Pro Controller") !=
            ControllerFamily::NintendoSwitch) {
        return fail("controller family detection did not identify common pads");
    }
    if (tetrisphere::select_controller_family("Xbox Controller", "nintendo-switch") !=
            ControllerFamily::NintendoSwitch ||
        tetrisphere::select_controller_family("DualSense", "auto") !=
            ControllerFamily::PlayStation) {
        return fail("manual controller-family override is not deterministic");
    }
    if (tetrisphere::select_controller_family("", "xbox") !=
            ControllerFamily::Xbox ||
        tetrisphere::select_controller_family(nullptr, "nintendo-switch") !=
            ControllerFamily::NintendoSwitch) {
        return fail("manual override did not resolve an unnamed controller");
    }
    using tetrisphere::MagicControl;
    if (std::string(tetrisphere::magic_control_label(ControllerFamily::Keyboard,
                                                    MagicControl::TriggerLeft)) != "K" ||
        std::string(tetrisphere::magic_control_label(ControllerFamily::Xbox,
                                                    MagicControl::TriggerLeft)) != "LT" ||
        std::string(tetrisphere::magic_control_label(ControllerFamily::PlayStation,
                                                    MagicControl::TriggerLeft)) != "L2" ||
        std::string(tetrisphere::magic_control_label(ControllerFamily::NintendoSwitch,
                                                    MagicControl::TriggerLeft)) != "ZL" ||
        std::string(tetrisphere::magic_control_label(ControllerFamily::NintendoSwitch,
                                                    MagicControl::East)) != "A" ||
        std::string(tetrisphere::magic_control_label(ControllerFamily::PlayStation,
                                                    MagicControl::West)) != "Square") {
        return fail("Magic prompt does not name the active physical control");
    }
    const auto confirm = tetrisphere::default_physical_button(LogicalAction::Confirm);
    const auto cancel = tetrisphere::default_physical_button(LogicalAction::Cancel);
    if (confirm != PhysicalButton::South || cancel != PhysicalButton::East ||
        std::string(tetrisphere::physical_button_label(ControllerFamily::Xbox, confirm)) != "A" ||
        std::string(tetrisphere::physical_button_label(ControllerFamily::PlayStation, confirm)) != "Cross" ||
        std::string(tetrisphere::physical_button_label(ControllerFamily::NintendoSwitch, confirm)) != "B" ||
        std::string(tetrisphere::physical_button_label(ControllerFamily::Xbox, cancel)) != "B" ||
        std::string(tetrisphere::physical_button_label(ControllerFamily::NintendoSwitch, cancel)) != "A") {
        return fail("prompt label does not follow the physical face-button position");
    }
    tetrisphere::ControllerBindings remapped{};
    remapped.confirm = PhysicalButton::East;
    remapped.cancel = PhysicalButton::South;
    tetrisphere::set_controller_bindings(remapped);
    const auto active = tetrisphere::controller_bindings();
    if (tetrisphere::physical_button_for(active, LogicalAction::Confirm) !=
            PhysicalButton::East ||
        std::string(tetrisphere::logical_action_label(
            ControllerFamily::NintendoSwitch, active,
            LogicalAction::Confirm)) != "A") {
        return fail("optional logical-action remapping is not preserved for prompts");
    }
    tetrisphere::ControllerBindings defaults{};
    if (std::string(tetrisphere::logical_action_label(
            ControllerFamily::Keyboard, defaults, LogicalAction::Confirm)) != "Z" ||
        std::string(tetrisphere::logical_action_label(
            ControllerFamily::PlayStation, defaults, LogicalAction::Confirm)) != "Cross") {
        return fail("runtime prompt interface does not share logical input bindings");
    }
    tetrisphere::set_controller_bindings({});
    return 0;
}

[[noreturn]] void invoke_invalid_pio_destination(const char* rom_path) {
    std::ifstream input(rom_path, std::ios::binary);
    static std::vector<std::uint8_t> rom(std::istreambuf_iterator<char>(input), {});
    static std::vector<std::uint8_t> rdram(8u * 1024u * 1024u);
    tetrisphere::set_runtime_rom(rom);
    recomp_context context{};
    context.r4 = 0x00FFB000u;
    context.r5 = 0x80800000u;
    tetrisphere_osPiRawReadIo(rdram.data(), &context);
    std::abort();
}

[[noreturn]] void invoke_concurrent_fatal() {
    constexpr std::size_t thread_count = 32;
    std::mutex mutex;
    std::condition_variable ready_condition;
    std::size_t ready = 0;
    bool release = false;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (std::size_t index = 0; index < thread_count; ++index) {
        threads.emplace_back([&] {
            {
                std::unique_lock lock(mutex);
                ++ready;
                ready_condition.notify_all();
                ready_condition.wait(lock, [&] { return release; });
            }
            tetrisphere::fatal_unsupported("func_80029674", 0x80029674u,
                                           0x800295A8u, "boot-thread-entry");
        });
    }
    {
        std::unique_lock lock(mutex);
        ready_condition.wait(lock, [&] { return ready == thread_count; });
        release = true;
    }
    ready_condition.notify_all();
    for (auto& thread : threads) {
        thread.join();
    }
    std::abort();
}

int verify_modern_runtime_backends() {
    std::vector<std::uint8_t> memory(8u * 1024u * 1024u);
    std::uint8_t* rdram = memory.data();
    constexpr std::int32_t queue_address = static_cast<std::int32_t>(0x80001000u);
    constexpr std::int32_t messages_address = static_cast<std::int32_t>(0x80001100u);
    constexpr std::int32_t output_address = static_cast<std::int32_t>(0x80001200u);
    constexpr std::int32_t timer_address = static_cast<std::int32_t>(0x80001300u);
    constexpr std::int32_t status_address = static_cast<std::int32_t>(0x80001400u);

    ultramodern::set_entrypoint_thread();
    osCreateMesgQueue(rdram, queue_address, messages_address, 2);
    if (osSendMesg(rdram, queue_address, 0x12345678, OS_MESG_NOBLOCK) != 0 ||
        osRecvMesg(rdram, queue_address, output_address, OS_MESG_NOBLOCK) != 0 ||
        static_cast<std::uint32_t>(MEM_W(0, output_address)) != 0x12345678u) {
        return fail("pinned message queue did not preserve a nonblocking message");
    }

    ultramodern::init_timers(rdram);
    if (osSetTimer(rdram, timer_address, 46'875u, 0, queue_address,
                   0x24681357) != 0) {
        return fail("pinned timer rejected a valid one-shot timer");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (osRecvMesg(rdram, queue_address, output_address, OS_MESG_NOBLOCK) != 0 ||
        static_cast<std::uint32_t>(MEM_W(0, output_address)) != 0x24681357u) {
        return fail("pinned timer did not deliver its queue message");
    }

    ultramodern::input::set_callbacks({nullptr, nullptr, nullptr, nullptr});
    std::uint8_t pattern = 0xFF;
    if (osContInit(rdram, queue_address, &pattern, status_address) != 0 || pattern != 0) {
        return fail("pinned controller initialization did not report disconnected ports");
    }

    ultramodern::rsp::set_callbacks({rsp_init, run_rsp_task});
    OSTask task{};
    task.t.type = M_GFXTASK;
    if (!ultramodern::rsp::run_task(rdram, &task) || !rsp_called) {
        return fail("pinned RSP backend did not delegate a real task descriptor");
    }

    ultramodern::set_audio_callbacks({queue_samples, remaining_frames, set_frequency});
    ultramodern::set_audio_frequency(32'000);
    ultramodern::queue_audio_buffer(rdram, 0x80002000u, 8);
    if (audio_frequency != 32'000 || audio_samples != 4) {
        return fail("pinned audio backend did not expose frequency and sample callbacks");
    }
    return 0;
}

} // namespace

extern "C" recomp_func_t* get_function(std::int32_t) {
    return nullptr;
}

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--registry") {
        return verify_registry(argv[2]);
    }
    if (argc == 3 && std::string(argv[1]) == "--pio") {
        return verify_pio(argv[2]);
    }
    if (argc == 3 && std::string(argv[1]) == "--wav-capture") {
        return verify_wav_capture(argv[2]);
    }
    if (argc == 3 && std::string(argv[1]) == "--invalid-pio-destination") {
        invoke_invalid_pio_destination(argv[2]);
    }
    if (argc == 2 && std::string(argv[1]) == "--backends") {
        return verify_modern_runtime_backends();
    }
    if (argc == 2 && std::string(argv[1]) == "--initial-image") {
        return verify_initial_image_layout();
    }
    if (argc == 2 && std::string(argv[1]) == "--ipl3-state") {
        return verify_ipl3_state();
    }
    if (argc == 2 && std::string(argv[1]) == "--eeprom-probe") {
        return verify_eeprom_probe();
    }
    if (argc == 2 && std::string(argv[1]) == "--eeprom-roundtrip") {
        return verify_eeprom_roundtrip();
    }
    if (argc == 3 && std::string(argv[1]) == "--eeprom-persistence") {
        return verify_eeprom_persistence(argv[2]);
    }
    if (argc == 2 && std::string(argv[1]) == "--sdl-audio-pacing") {
        return verify_sdl_audio_pacing();
    }
    if (argc == 2 && std::string(argv[1]) == "--zero-retrace-wait") {
        return verify_zero_retrace_wait();
    }
    if (argc == 2 && std::string(argv[1]) == "--controller-prompts") {
        return verify_controller_prompts();
    }
    if (argc == 2 && std::string(argv[1]) == "--unsupported") {
        tetrisphere::fatal_unsupported("osPiStartDma", 0x80000000u,
                                       0x80001234u, "runtime-contract-test");
    }
    if (argc == 2 && std::string(argv[1]) == "--unknown") {
        tetrisphere::fatal_unsupported("unknown_system_call", 0x81234567u,
                                       0x80004321u, "runtime-contract-test");
    }
    if (argc == 2 && std::string(argv[1]) == "--concurrent-fatal") {
        invoke_concurrent_fatal();
    }
    return fail("usage error");
}
