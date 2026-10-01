#include "tetrisphere/runtime.h"
#include "tetrisphere/run_identity.h"
#include "tetrisphere/eeprom_store.h"

#include "recomp.h"
#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"
#include "lightweightsemaphore.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <json.hpp>
#include <mutex>
#include <memory>
#include <thread>

namespace tetrisphere {
namespace {

using namespace std::literals;

constexpr std::array kBindings{
    RuntimeBinding{0x800D1DC0u, true, "osInitialize"sv, RuntimeCategory::Boot,
                   RuntimeStatus::Implemented, "tetrisphere_osInitialize"sv,
                   "observed call 0x8002952C; inspected NTPE rev0 body and pinned wrapper"sv,
                   "pinned host implementation"sv},
    RuntimeBinding{0x800D2050u, true, "osPiRawReadIo"sv, RuntimeCategory::PiDma,
                   RuntimeStatus::Implemented, "tetrisphere_osPiRawReadIo"sv,
                   "observed calls 0x80029554 in a 16-word boot loop; PI status/read body inspected"sv,
                   "bounded aligned cartridge PIO with 8 MiB NTPE mirror"sv},
    RuntimeBinding{0x800D1250u, true, "osCreateThread"sv, RuntimeCategory::Thread,
                   RuntimeStatus::Delegated, "osCreateThread"sv,
                   "observed call 0x800295A8; six-argument OSThread initialization body inspected"sv,
                   "pinned scheduler implementation"sv},
    RuntimeBinding{0x800D13A0u, true, "osStartThread"sv, RuntimeCategory::Thread,
                   RuntimeStatus::Delegated, "osStartThread"sv,
                   "observed call 0x800295B4; OSThread state/queue body inspected"sv,
                   "pinned scheduler implementation"sv},
    RuntimeBinding{0x800D0D00u, true, "osCreateMesgQueue"sv, RuntimeCategory::MessageQueue,
                   RuntimeStatus::Delegated, "osCreateMesgQueue_recomp"sv,
                   "cold-boot trace and three-argument queue constructor body"sv,
                   "pinned scheduler message-queue implementation"sv},
    RuntimeBinding{0x800D9ED0u, true, "osSetTimer"sv, RuntimeCategory::Timer,
                   RuntimeStatus::Delegated, "osSetTimer_recomp"sv,
                   "mapped NTPE timer body and generated runtime boundary"sv,
                   "pinned host timer implementation"sv},
    RuntimeBinding{0x800D2E90u, true, "osPiStartDma"sv, RuntimeCategory::PiDma,
                   RuntimeStatus::Implemented, "osPiStartDma_recomp"sv,
                   "cold-boot trace before first OSTask; six-argument DMA body"sv,
                   "bounded ROM-to-RDRAM copy with PI completion message"sv},
    RuntimeBinding{0x800D31E0u, true, "osContInit"sv, RuntimeCategory::Controller,
                   RuntimeStatus::Delegated, "osContInit_recomp"sv,
                   "cold-boot trace and controller initialization body"sv,
                   "pinned input backend connected to SDL callbacks"sv},
    RuntimeBinding{0x800D0D30u, true, "osCreateViManager"sv, RuntimeCategory::Vi,
                   RuntimeStatus::Delegated, "osCreateViManager_recomp"sv,
                   "cold-boot trace before scheduler construction"sv,
                   "host VI/event threads are created by ultramodern preinit"sv},
    RuntimeBinding{0x800D1C4Cu, true, "osSpTaskStartGo"sv, RuntimeCategory::Rsp,
                   RuntimeStatus::Implemented, "osSpTaskStartGo_recomp"sv,
                   "cold-boot trace reaches launcher and observed audio/GFX descriptors"sv,
                   "submits verified tasks to RT64 or RSPRecomp"sv},
    RuntimeBinding{0x800D1D40u, true, "osSpTaskYield"sv, RuntimeCategory::Rsp,
                   RuntimeStatus::Implemented, "osSpTaskYield_recomp"sv,
                   "resize-triggered task scheduler call and inspected NTPE rev0 body"sv,
                   "yield ignored because host tasks complete before yield; no SP/DP event fabricated"sv},
    RuntimeBinding{0x800D5930u, true, "osAiSetNextBuffer"sv, RuntimeCategory::Audio,
                   RuntimeStatus::Implemented, "osAiSetNextBuffer_recomp"sv,
                   "mapped NTPE AI body and native playable-run audio submissions"sv,
                   "validated two-entry AI DMA FIFO feeding SDL output"sv},
};

std::span<const std::uint8_t> runtime_rom;
std::mutex handoff_mutex;
std::condition_variable handoff_condition;
bool thread_entry_reached = false;
std::array<std::uint8_t, 512> eeprom{};
std::mutex eeprom_mutex;
std::unique_ptr<EepromStore> eeprom_store;

const char* category_name(RuntimeCategory category) {
    switch (category) {
        case RuntimeCategory::Boot: return "boot";
        case RuntimeCategory::Thread: return "thread";
        case RuntimeCategory::MessageQueue: return "message_queue";
        case RuntimeCategory::Timer: return "timer";
        case RuntimeCategory::PiDma: return "pi_dma";
        case RuntimeCategory::Controller: return "controller";
        case RuntimeCategory::Vi: return "vi";
        case RuntimeCategory::Rsp: return "rsp";
        case RuntimeCategory::Audio: return "audio";
    }
    return "invalid";
}

const char* status_name(RuntimeStatus status) {
    switch (status) {
        case RuntimeStatus::Implemented: return "implemented";
        case RuntimeStatus::Delegated: return "delegated";
        case RuntimeStatus::Unsupported: return "unsupported";
    }
    return "invalid";
}

} // namespace

std::span<const RuntimeBinding> runtime_bindings() {
    return kBindings;
}

const RuntimeBinding* find_runtime_binding(std::uint32_t address) {
    const auto found = std::find_if(kBindings.begin(), kBindings.end(),
                                    [address](const RuntimeBinding& binding) {
                                        return binding.has_guest_address && binding.address == address;
                                    });
    return found == kBindings.end() ? nullptr : &*found;
}

bool validate_runtime_table_json(std::string_view serialized, std::string& error) {
    try {
        const auto root = nlohmann::json::parse(serialized);
        if (root.at("schema_version") != 1 || !root.at("bindings").is_array()) {
            error = "unsupported schema";
            return false;
        }
        if (root.at("bindings").size() != kBindings.size()) {
            error = "binding count differs from compiled registry";
            return false;
        }
        for (std::size_t index = 0; index < kBindings.size(); ++index) {
            const auto& actual = root.at("bindings").at(index);
            const auto& expected = kBindings[index];
            const bool address_matches = expected.has_guest_address
                ? actual.at("address") == ("0x" + [&expected] {
                    constexpr char digits[] = "0123456789ABCDEF";
                    std::string result(8, '0');
                    for (unsigned shift = 0; shift < 8; ++shift) {
                        result[7 - shift] = digits[(expected.address >> (shift * 4)) & 0xFu];
                    }
                    return result;
                }())
                : actual.at("address").is_null();
            if (!address_matches ||
                actual.at("name").get<std::string>() != expected.name ||
                actual.at("category").get<std::string>() != category_name(expected.category) ||
                actual.at("status").get<std::string>() != status_name(expected.status) ||
                actual.at("host_binding").get<std::string>() != expected.host_binding ||
                actual.at("provenance").get<std::string>() != expected.provenance ||
                actual.at("detail").get<std::string>() != expected.detail) {
                error = "binding mismatch at index " + std::to_string(index);
                return false;
            }
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool pi_raw_read_io(std::span<const std::uint8_t> rom, std::uint32_t device_address,
                    std::uint32_t& value) {
    constexpr std::size_t expected_rom_size = 8u * 1024u * 1024u;
    constexpr std::uint32_t observed_begin = 0x00FFB000u;
    constexpr std::uint32_t observed_end = observed_begin + 16u * sizeof(std::uint32_t);
    if (rom.size() != expected_rom_size || (device_address & 3u) != 0u ||
        device_address < observed_begin || device_address >= observed_end) {
        return false;
    }
    const std::size_t offset = device_address - expected_rom_size;
    if (offset + 4u > rom.size()) {
        return false;
    }
    value = (std::uint32_t{rom[offset]} << 24u)
          | (std::uint32_t{rom[offset + 1]} << 16u)
          | (std::uint32_t{rom[offset + 2]} << 8u)
          | std::uint32_t{rom[offset + 3]};
    return true;
}

bool load_initial_image(std::span<const std::uint8_t> rom,
                        std::span<std::uint8_t> rdram,
                        std::size_t rom_offset, std::size_t ram_offset,
                        std::size_t size) {
    if ((ram_offset & 3u) != 0u || (size & 3u) != 0u ||
        rom_offset > rom.size() || size > rom.size() - rom_offset ||
        ram_offset > rdram.size() || size > rdram.size() - ram_offset) {
        return false;
    }
    for (std::size_t index = 0; index < size; ++index) {
        rdram[ram_offset + (index ^ 3u)] = rom[rom_offset + index];
    }
    return true;
}

void initialize_ipl3_state(std::uint8_t* rdram) {
    // These words are populated by IPL3 on hardware. The native entrypoint
    // starts after IPL3, so preserve the same boot contract explicitly.
    MEM_W(0, 0xFFFFFFFF80000300ull) = 1; // OS_TV_NTSC
    MEM_W(0, 0xFFFFFFFF80000308ull) = static_cast<std::int32_t>(0xB0000000u);
    MEM_W(0, 0xFFFFFFFF8000030Cull) = 0; // cold reset
    MEM_W(0, 0xFFFFFFFF80000318ull) = 8 * 1024 * 1024;
}

void set_runtime_rom(std::span<const std::uint8_t> rom) {
    runtime_rom = rom;
}

bool initialize_eeprom(const std::filesystem::path& path, std::string& error) {
    auto store = std::make_unique<EepromStore>(path);
    std::array<std::uint8_t, 512> loaded{};
    if (!store->load(loaded, error)) return false;
    std::lock_guard lock(eeprom_mutex);
    eeprom = loaded;
    eeprom_store = std::move(store);
    return true;
}

[[noreturn]] void await_runtime_handoff() {
    using namespace std::chrono_literals;
    std::unique_lock lock(handoff_mutex);
    if (!handoff_condition.wait_for(lock, 2s, [] { return thread_entry_reached; })) {
        fatal_unsupported("boot_thread_start_timeout", 0x80029674u,
                          0x800295B4u, "boot-thread-start");
    }
    for (;;) {
        handoff_condition.wait(lock);
    }
}

} // namespace tetrisphere

extern "C" {
void yield_self_1ms(std::uint8_t* rdram);

void tetrisphere_osInitialize(std::uint8_t*, recomp_context*) {
    // ultramodern's pinned osInitialize is intentionally empty: the host runtime
    // owns hardware initialization. Keep the same observable semantics without
    // pulling renderer preinitialization into this pre-graphics diagnostic.
}

void tetrisphere_osPiRawReadIo(std::uint8_t* rdram, recomp_context* ctx) {
    std::uint32_t value = 0;
    if (!tetrisphere::pi_raw_read_io(tetrisphere::runtime_rom,
                                     static_cast<std::uint32_t>(ctx->r4), value)) {
        tetrisphere::fatal_unsupported("osPiRawReadIo", 0x800D2050u,
                                       0x80029554u, "boot-cartridge-pio");
    }
    constexpr std::uint64_t rdram_begin = 0xFFFFFFFF80000000ull;
    constexpr std::uint64_t rdram_last_word = 0xFFFFFFFF807FFFFCull;
    const std::uint64_t destination = ctx->r5;
    if ((destination & 3u) != 0u || destination < rdram_begin ||
        destination > rdram_last_word) {
        tetrisphere::fatal_unsupported("osPiRawReadIo_destination",
                                       static_cast<std::uint32_t>(destination),
                                       0x80029554u, "boot-cartridge-pio");
    }
    MEM_W(0, destination) = value;
    ctx->r2 = 0;
}

void tetrisphere_osCreateThread(std::uint8_t* rdram, recomp_context* ctx) {
    osCreateThread(rdram, static_cast<std::int32_t>(ctx->r4),
                   static_cast<OSId>(ctx->r5), static_cast<std::int32_t>(ctx->r6),
                   static_cast<std::int32_t>(ctx->r7),
                   static_cast<std::int32_t>(MEM_W(0x10, ctx->r29)),
                   static_cast<OSPri>(MEM_W(0x14, ctx->r29)));
}

void tetrisphere_osStartThread(std::uint8_t* rdram, recomp_context* ctx) {
    osStartThread(rdram, static_cast<std::int32_t>(ctx->r4));
}

void tetrisphere_wait_retraces(std::uint8_t* rdram, recomp_context* ctx) {
    constexpr gpr countdown = 0xFFFFFFFF800DFE90ull;
    const auto requested = static_cast<std::int16_t>(ctx->r4);
    MEM_H(0, countdown) = requested;
    if (requested <= 0) return;

    // The original routine busy-waits while the scheduler's VI handler
    // decrements this counter. N64ModernRuntime schedules guest threads
    // cooperatively, so periodically enter its scheduler: this lets the real
    // guest VI handler run and preserves all side effects associated with the
    // ten retraces instead of replacing them with a host sleep.
    while (MEM_H(0, countdown) > 0) yield_self_1ms(rdram);
}

void tetrisphere_osSetWatchLo(std::uint8_t*, recomp_context*) {
    // CP0 WatchLo/WatchHi only configure a hardware debugger trap. The host
    // runtime has no corresponding guest-visible state or success value.
}

void tetrisphere_osClearWatch(std::uint8_t*, recomp_context*) {
    // Clearing CP0 watch registers has no guest-visible result on the host.
}

void tetrisphere_osProbeTLB(std::uint8_t*, recomp_context*) {
    tetrisphere::fatal_unsupported("osProbeTLB", 0x800D6CE0u, 0u,
                                   "guest-tlb-probe");
}

void tetrisphere_osExceptionDispatch(std::uint8_t*, recomp_context*) {
    tetrisphere::fatal_unsupported("exception_dispatch", 0x800D7D2Cu, 0u,
                                   "guest-exception-dispatch");
}

void tetrisphere_osExceptionResume(std::uint8_t*, recomp_context*) {
    tetrisphere::fatal_unsupported("exception_resume", 0x800D7E84u, 0u,
                                   "guest-exception-resume");
}

void tetrisphere_osSetCompare(std::uint8_t*, recomp_context*) {
    tetrisphere::fatal_unsupported("osSetCompare", 0x800DB830u, 0u,
                                   "guest-cp0-timer");
}

void osCreatePiManager_recomp(std::uint8_t*, recomp_context*) {
    // Host PI transfers are synchronous and completion is posted by the
    // adapter below, so no separate guest device-manager thread is needed.
}

void osPiStartDma_recomp(std::uint8_t* rdram, recomp_context* ctx) {
    const auto direction = static_cast<std::uint32_t>(ctx->r6);
    const auto device = static_cast<std::uint32_t>(ctx->r7);
    const auto destination = static_cast<std::uint32_t>(MEM_W(0x10, ctx->r29));
    const auto destination_offset = destination & 0x7FFFFFu;
    const auto size = static_cast<std::uint32_t>(MEM_W(0x14, ctx->r29));
    const auto queue = static_cast<std::int32_t>(MEM_W(0x18, ctx->r29));
    if (direction != 0u || device > tetrisphere::runtime_rom.size() ||
        size > tetrisphere::runtime_rom.size() - device ||
        (destination & 0xFF800000u) != 0x80000000u ||
        size > 0x800000u - destination_offset) {
        tetrisphere::fatal_unsupported("osPiStartDma", 0x800D2E90u,
                                       0u, "rom-to-rdram-dma");
    }
    for (std::uint32_t index = 0; index < size; ++index) {
        rdram[(destination_offset + index) ^ 3u] =
            tetrisphere::runtime_rom[device + index];
    }
    ultramodern::enqueue_external_message_src(
        queue, 0, false, ultramodern::EventMessageSource::Pi);
    ctx->r2 = 0;
}

void osSpTaskLoad_recomp(std::uint8_t*, recomp_context*) {}

void osSpTaskStartGo_recomp(std::uint8_t* rdram, recomp_context* ctx) {
    static std::atomic_uint task_count = 0;
    const auto descriptor = static_cast<std::uint32_t>(ctx->r4);
    const auto descriptor_offset = descriptor & 0x7FFFFFu;
    const auto sequence = ++task_count;
    if (sequence <= 12 && descriptor_offset <= 0x800000u - sizeof(OSTask)) {
        const auto* task = reinterpret_cast<const OSTask*>(rdram + descriptor_offset);
        std::fprintf(stdout,
                     "{\"event\":\"rsp_task_submit\",\"run_id\":\"%s\",\"sequence\":%u,"
                     "\"type\":%u,\"descriptor\":\"0x%08X\","
                     "\"ucode\":\"0x%08X\",\"data\":\"0x%08X\"}\n",
                     tetrisphere::run_id(), sequence,
                     static_cast<unsigned>(task->t.type), descriptor,
                     static_cast<unsigned>(task->t.ucode),
                     static_cast<unsigned>(task->t.data_ptr));
        std::fflush(stdout);
    }
    ultramodern::submit_rsp_task(rdram, static_cast<std::int32_t>(ctx->r4));
}

void osSpTaskYield_recomp(std::uint8_t*, recomp_context*) {
    // Host GFX tasks send SP completion before the display list is processed;
    // audio tasks send it when their RSP execution completes. Neither path
    // can yield. The guest scheduler checks osSpTaskYielded after the normal
    // completion event, and that adapter returns zero below.
    static std::atomic_uint yield_requests{0};
    const auto sequence = ++yield_requests;
    if (sequence <= 4) {
        std::fprintf(stdout,
                     "{\"event\":\"rsp_task_yield_ignored\",\"run_id\":\"%s\","
                     "\"sequence\":%u}\n",
                     tetrisphere::run_id(), sequence);
        std::fflush(stdout);
    }
}

void osSpTaskYielded_recomp(std::uint8_t*, recomp_context* ctx) {
    ctx->r2 = 0;
    static std::atomic_uint yielded_checks{0};
    const auto sequence = ++yielded_checks;
    if (sequence <= 4) {
        std::fprintf(stdout,
                     "{\"event\":\"rsp_task_yielded_check\",\"run_id\":\"%s\","
                     "\"sequence\":%u,\"yielded\":false}\n",
                     tetrisphere::run_id(), sequence);
        std::fflush(stdout);
    }
}

void osDpSetNextBuffer_recomp(std::uint8_t*, recomp_context*) {
    // RT64 consumes the RDP command range submitted with the OSTask. No
    // separate host DP DMA is initiated here.
}

void osEepromProbe_recomp(std::uint8_t*, recomp_context* ctx) {
    // NTPE uses the standard 4-Kbit EEPROM. The original 0x800D2D70 body was
    // identified from its osEepromProbe signature and __osEepStatus call
    // sequence; executing that body would touch the N64 SI MMIO registers.
    ctx->r2 = 1; // EEPROM_TYPE_4K
}

void osEepromRead_recomp(std::uint8_t* rdram, recomp_context* ctx) {
    constexpr std::size_t block_size = 8;
    constexpr std::size_t block_count = tetrisphere::eeprom.size() / block_size;
    const auto block = static_cast<std::uint32_t>(ctx->r5);
    if (block >= block_count) {
        ctx->r2 = static_cast<gpr>(-1);
        return;
    }
    std::lock_guard lock(tetrisphere::eeprom_mutex);
    for (std::size_t index = 0; index < block_size; ++index) {
        MEM_B(index, ctx->r6) = tetrisphere::eeprom[block * block_size + index];
    }
    ctx->r2 = 0;
}

void osEepromWrite_recomp(std::uint8_t* rdram, recomp_context* ctx) {
    constexpr std::size_t block_size = 8;
    constexpr std::size_t block_count = tetrisphere::eeprom.size() / block_size;
    const auto block = static_cast<std::uint32_t>(ctx->r5);
    if (block >= block_count) {
        ctx->r2 = static_cast<gpr>(-1);
        return;
    }
    std::lock_guard lock(tetrisphere::eeprom_mutex);
    for (std::size_t index = 0; index < block_size; ++index) {
        tetrisphere::eeprom[block * block_size + index] = MEM_B(index, ctx->r6);
    }
    if (tetrisphere::eeprom_store != nullptr) {
        std::string error;
        if (!tetrisphere::eeprom_store->save(tetrisphere::eeprom, error)) {
            tetrisphere::fatal_unsupported("eeprom_persist_failed", 0x800D2330u,
                                           0u, "eeprom-write");
        }
    }
    ctx->r2 = 0;
}

void __osAiDeviceBusy_recomp(std::uint8_t*, recomp_context* ctx) {
    ctx->r2 = ultramodern::get_remaining_audio_bytes() == 0 ? 0 : 1;
}

void cop0_status_write(recomp_context* ctx, gpr value) {
    constexpr std::uint32_t float_register_mode = 0x04000000u;
    const auto old_status = ctx->status_reg;
    const auto new_status = static_cast<std::uint32_t>(value);
    if (((old_status ^ new_status) & ~float_register_mode) != 0u) {
        tetrisphere::fatal_unsupported("cop0_status_write", new_status, 0u,
                                       "guest-cp0-status");
    }
    ctx->status_reg = new_status;
    ctx->mips3_float_mode = (new_status & float_register_mode) != 0u;
    ctx->f_odd = ctx->mips3_float_mode ? &ctx->f1.u32l : &ctx->f0.u32h;
}

gpr cop0_status_read(recomp_context* ctx) {
    return static_cast<gpr>(static_cast<std::int32_t>(ctx->status_reg));
}

void switch_error(const char* symbol, std::uint32_t address,
                  std::uint32_t jump_table) {
    tetrisphere::fatal_unsupported(symbol, address, jump_table,
                                   "jump-table-out-of-bounds");
}

void do_break(std::uint32_t address) {
    tetrisphere::fatal_unsupported("mips_break", address, 0u,
                                   "guest-break-instruction");
}
}

std::atomic_bool exited = false;
moodycamel::LightweightSemaphore graphics_shutdown_ready;

namespace ultramodern {
bool is_game_started() {
    return true;
}
} // namespace ultramodern

void run_thread_function(std::uint8_t* rdram, std::uint64_t address,
                         std::uint64_t stack, std::uint64_t argument) {
    const auto narrowed = static_cast<std::uint32_t>(address);
    {
        std::lock_guard lock(tetrisphere::handoff_mutex);
        tetrisphere::thread_entry_reached = true;
    }
    tetrisphere::handoff_condition.notify_all();
    recomp_context context{};
    context.r29 = stack;
    context.r4 = argument;
    context.f_odd = &context.f0.u32h;
    get_function(static_cast<std::int32_t>(narrowed))(rdram, &context);
}
