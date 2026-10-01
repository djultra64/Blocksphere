#include "tetrisphere/microcode.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#ifdef TETRISPHERE_MICROCODE_LIVE
#include "gbi/rt64_gbi.h"
#include "hle/rt64_interpreter.h"
#include "hle/rt64_workload_queue.h"
#include "librecomp/rsp.hpp"
#include "ultramodern/ultra64.h"
#include "xxHash/xxh3.h"

RspExitReason tetrisphere_audio_rsp(std::uint8_t*, std::uint32_t);

void ignore_interrupt() {}
std::size_t cpu_full_sync_count = 0;
std::size_t unsupported_gbi_count = 0;

void cpu_full_sync(RT64::State* state, RT64::DisplayList**) {
    // Preserve RT64's CPU draw-call submission, while deliberately leaving the
    // GPU upload/finalization stage to the renderer integration in Task 7.
    state->flush();
    state->submitFramebufferPair(
        RT64::FramebufferPair::FlushReason::ProcessDisplayListsEnd);
    ++cpu_full_sync_count;
}

void reject_unsupported_gbi(RT64::State* state, RT64::DisplayList** dl) {
    const auto opcode = static_cast<std::uint8_t>((*dl)->w0 >> 24);
    const auto offset = reinterpret_cast<const std::uint8_t*>(*dl) - state->RDRAM;
    std::cerr << "unsupported RT64 GBI opcode 0x" << std::hex
              << static_cast<unsigned>(opcode) << " at RDRAM+0x" << offset
              << std::dec << '\n';
    ++unsupported_gbi_count;
}
#endif

namespace {

int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

int unit_contract() {
    constexpr std::array<std::uint8_t, 4> supported{0x06, 0xBC, 0xE7, 0xFF};
    constexpr std::array<tetrisphere::MicrocodeCommand, 4> commands{{
        {0, 0xBC, 8}, {8, 0x06, 8}, {16, 0xE7, 8}, {24, 0xAA, 8},
    }};
    const auto graphics = tetrisphere::inspect_rt64_commands(
        "F3D_SM64_FINAL", commands, supported);
    if (graphics.accepted || graphics.command_count != 4 ||
        graphics.unsupported.size() != 1 || graphics.unsupported[0].offset != 24) {
        return fail("RT64 inventory must retain the unsupported command and reject task");
    }

    tetrisphere::AudioBridge audio;
    constexpr std::array<std::int16_t, 8> samples{0, 0, 1, -1, 0, 2, -3, 0};
    if (!audio.queue_samples(samples) || audio.callback_count() != 1 ||
        audio.sample_count() != 8 || audio.frame_count() != 4 ||
        audio.nonzero_sample_count() != 4) {
        return fail("audio bridge accounted samples or stereo frames incorrectly");
    }
    if (audio.queue_samples(std::span<const std::int16_t>{})) {
        return fail("audio bridge must reject an empty callback");
    }
    constexpr std::array<std::uint8_t, 32> audio_commands{
        0x08,0,0,0, 0,0,0x02,0x80,
        0x06,0,0,0, 0,0x2C,0x17,0x60,
        0x08,0,0,0, 0,0,0,0x40,
        0x06,0,0,0, 0,0x2C,0x21,0x60,
    };
    const auto ranges = tetrisphere::derive_audio_output_ranges(audio_commands);
    if (ranges.size() != 2 || ranges[0].address != 0x2C1760 ||
        ranges[0].bytes != 0x280 || ranges[1].address != 0x2C2160 ||
        ranges[1].bytes != 0x40) {
        return fail("audio output ranges do not follow SETBUFF/SAVEBUFF");
    }
    constexpr std::array<std::uint8_t, 7> unaligned_pcm{
        0xFF, 0x34, 0x12, 0xFE, 0xFF, 0xEE, 0xEE,
    };
    const auto decoded = tetrisphere::decode_pcm_samples(
        unaligned_pcm, {1, 4});
    if (!decoded || decoded->size() != 2 || (*decoded)[0] != 0x1234 ||
        (*decoded)[1] != -2) {
        return fail("PCM decoding must not depend on host alignment or aliasing");
    }
    if (tetrisphere::decode_pcm_samples(unaligned_pcm, {6, 4}) ||
        tetrisphere::decode_pcm_samples(unaligned_pcm, {1, 3})) {
        return fail("PCM decoding must reject every out-of-bounds or odd range");
    }
    return 0;
}

#ifdef TETRISPHERE_MICROCODE_LIVE
std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

void word_swap(std::vector<std::uint8_t>& bytes) {
    for (std::size_t index = 0; index < bytes.size(); index += 4) {
        std::swap(bytes[index], bytes[index + 3]);
        std::swap(bytes[index + 1], bytes[index + 2]);
    }
}

bool verified_audio_ucode = false;

RspUcodeFunc* select_audio_ucode(const OSTask* task) {
    constexpr std::uint32_t expected_ucode = 0x800DE7D0;
    return verified_audio_ucode && task->t.type == M_AUDTASK &&
           static_cast<std::uint32_t>(task->t.ucode) == expected_ucode
        ? tetrisphere_audio_rsp : nullptr;
}

bool verify_audio_ucode(std::span<const std::uint8_t> rdram,
                        const OSTask& task) {
    constexpr std::uint64_t expected_hash = 0xE621BF918F29B135ULL;
    constexpr std::size_t expected_size = 0xDF0;
    const auto address = static_cast<std::uint32_t>(task.t.ucode);
    if (task.t.type != M_AUDTASK || address < 0x80000000u) return false;
    const auto offset = static_cast<std::size_t>(address - 0x80000000u);
    return offset <= rdram.size() && expected_size <= rdram.size() - offset &&
           XXH3_64bits(rdram.data() + offset, expected_size) == expected_hash;
}

int live_contract(const char* graphics_snapshot_path,
                  const char* graphics_commands_path,
                  const char* audio_snapshot_path) {
    auto graphics_rdram = read_file(graphics_snapshot_path);
    const auto graphics_commands = read_file(graphics_commands_path);
    auto audio_rdram = read_file(audio_snapshot_path);
    if (graphics_rdram.size() != 0x800000 || audio_rdram.size() != 0x800000 ||
        graphics_commands.empty() || graphics_commands.size() % 8 != 0) {
        return fail("live inputs have invalid bounded sizes");
    }
    word_swap(graphics_rdram);
    constexpr std::size_t graphics_descriptor_offset = 0x0FACD0;
    if (graphics_descriptor_offset > graphics_rdram.size() ||
        sizeof(OSTask) > graphics_rdram.size() - graphics_descriptor_offset) {
        return fail("captured graphics descriptor is outside RDRAM");
    }
    OSTask graphics_task{};
    std::memcpy(&graphics_task,
                graphics_rdram.data() + graphics_descriptor_offset,
                sizeof(graphics_task));
    constexpr std::uint64_t expected_text_hash = 0x7D8EB8BDCAE7DF81ULL;
    constexpr std::uint64_t expected_data_hash = 0x880FCE853CE3E422ULL;
    if (XXH3_64bits(graphics_rdram.data() + 0x0DD3D0, 0x13F8) !=
            expected_text_hash ||
        XXH3_64bits(graphics_rdram.data() + 0x0F1570, 0x800) !=
            expected_data_hash) {
        return fail("captured graphics microcode is not F3D_SM64_FINAL");
    }
    std::uint32_t mi_interrupt = 0;
    RT64::State state(graphics_rdram.data(), &mi_interrupt, ignore_interrupt);
    RT64::Interpreter interpreter;
    RT64::WorkloadQueue workload_queue;
    RT64::UserConfiguration user_configuration;
    RT64::EmulatorConfiguration emulator_configuration;
    RT64::EnhancementConfiguration enhancement_configuration;
    RT64::State::External external{};
    external.interpreter = &interpreter;
    external.workloadQueue = &workload_queue;
    external.userConfig = &user_configuration;
    external.emulatorConfig = &emulator_configuration;
    external.enhancementConfig = &enhancement_configuration;
    state.ext = external;
    state.rdramCheckPending = false;
    interpreter.setup(&state);
    state.rdp->setGBI();
    interpreter.loadUCodeGBI(
        static_cast<std::uint32_t>(graphics_task.t.ucode),
        static_cast<std::uint32_t>(graphics_task.t.ucode_data), true);
    auto* gbi = interpreter.hleGBI;
    if (gbi == nullptr || gbi->ucode != RT64::GBIUCode::F3D) {
        return fail("pinned RT64 rejected captured graphics microcode");
    }
    constexpr std::uint8_t full_sync_opcode = 0xE9;
    gbi->map[full_sync_opcode] = cpu_full_sync;
    cpu_full_sync_count = 0;
    unsupported_gbi_count = 0;
    for (auto& handler : gbi->map) {
        if (handler == nullptr) handler = reject_unsupported_gbi;
    }
    const auto graphics_start = static_cast<std::uint32_t>(
        graphics_task.t.data_ptr) & 0x7FFFFF;
    const auto graphics_size = static_cast<std::size_t>(graphics_task.t.data_size);
    if (graphics_size != graphics_commands.size() ||
        graphics_start > graphics_rdram.size() ||
        graphics_size > graphics_rdram.size() - graphics_start) {
        return fail("captured graphics display list range is invalid");
    }
    interpreter.processDisplayLists(
        graphics_start,
        reinterpret_cast<RT64::DisplayList*>(graphics_rdram.data() + graphics_start));
    const auto& parsed_workload = workload_queue.workloads[workload_queue.writeCursor];
    if (unsupported_gbi_count != 0) {
        return fail("captured display-list graph contains unsupported RT64 commands");
    }
    if (state.displayListCounter != 1 || !state.returnAddressStack.empty() ||
        cpu_full_sync_count == 0 || parsed_workload.fbPairSubmitted == 0 ||
        parsed_workload.gameCallCount == 0 ||
        parsed_workload.drawData.faceIndices.empty()) {
        return fail("pinned RT64 did not execute the captured display-list graph");
    }

    constexpr std::size_t descriptor_offset = 0x2C16E8;
    const auto raw_audio = audio_rdram;
    word_swap(audio_rdram);
    const auto before = audio_rdram;
    if (descriptor_offset > audio_rdram.size() ||
        sizeof(OSTask) > audio_rdram.size() - descriptor_offset) {
        return fail("captured audio descriptor is outside RDRAM");
    }
    OSTask task{};
    std::memcpy(&task, audio_rdram.data() + descriptor_offset, sizeof(task));
    const auto command_offset = static_cast<std::size_t>(task.t.data_ptr & 0x7FFFFF);
    const auto command_size = static_cast<std::size_t>(task.t.data_size);
    if (command_size == 0 || command_size % 8 != 0 ||
        command_size > raw_audio.size() ||
        command_offset > raw_audio.size() - command_size) {
        return fail("captured audio command region is invalid");
    }
    const auto ranges = tetrisphere::derive_audio_output_ranges(
        std::span(raw_audio).subspan(command_offset, command_size));
    constexpr std::array<tetrisphere::AudioOutputRange, 5> expected_ranges{{
        {0x2C1760, 0x280}, {0x2C19E0, 0x280}, {0x2C1C60, 0x280},
        {0x2C1EE0, 0x280}, {0x2C2160, 0x80},
    }};
    for (const auto expected : expected_ranges) {
        const auto found = std::find_if(ranges.begin(), ranges.end(),
            [expected](const auto range) {
                return range.address == expected.address && range.bytes == expected.bytes;
            });
        if (found == ranges.end()) return fail("captured PCM save range is absent");
    }

    verified_audio_ucode = verify_audio_ucode(audio_rdram, task);
    if (!verified_audio_ucode || select_audio_ucode(&task) == nullptr) {
        return fail("captured audio microcode hash is not the RSPRecomp selector identity");
    }
    auto mutated_audio = audio_rdram;
    mutated_audio[static_cast<std::uint32_t>(task.t.ucode) & 0x7FFFFF] ^= 1;
    if (verify_audio_ucode(mutated_audio, task)) {
        return fail("mutated audio microcode was accepted at the captured address");
    }

    recomp::rsp::constants_init();
    recomp::rsp::set_callbacks({select_audio_ucode});
    if (!recomp::rsp::run_task(audio_rdram.data(), &task)) {
        return fail("RSPRecomp audio task did not exit with break");
    }
    tetrisphere::AudioBridge bridge;
    std::size_t changed_output_bytes = 0;
    for (const auto range : expected_ranges) {
        const auto decoded = tetrisphere::decode_pcm_samples(audio_rdram, range);
        if (!decoded || range.address > before.size() ||
            range.bytes > before.size() - range.address) {
            return fail("captured PCM output range is outside RDRAM");
        }
        for (std::size_t index = 0; index < range.bytes; ++index) {
            changed_output_bytes +=
                audio_rdram[range.address + index] != before[range.address + index];
        }
        if (!bridge.queue_samples(*decoded)) {
            return fail("audio bridge rejected captured PCM range");
        }
    }
    if (bridge.callback_count() != 5 || bridge.sample_count() != 1344 ||
        bridge.frame_count() != 672 || bridge.nonzero_sample_count() != 1343 ||
        changed_output_bytes != 2649) {
        return fail("captured RSP output PCM invariant changed");
    }
    std::cout << "rt64_ucode=F3D_SM64_FINAL rt64_display_lists="
              << state.displayListCounter << " rt64_face_indices="
              << parsed_workload.drawData.faceIndices.size()
              << " rt64_game_calls=" << parsed_workload.gameCallCount
              << " rt64_cpu_submissions=" << parsed_workload.fbPairSubmitted
              << " audio_callbacks=" << bridge.callback_count()
              << " pcm_frames=" << bridge.frame_count()
              << " pcm_nonzero_samples=" << bridge.nonzero_sample_count()
              << " pcm_changed_bytes=" << changed_output_bytes << '\n';
    return 0;
}
#endif

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--unit") {
        return unit_contract();
    }
#ifdef TETRISPHERE_MICROCODE_LIVE
    if (argc == 5 && std::string(argv[1]) == "--live") {
        return live_contract(argv[2], argv[3], argv[4]);
    }
#endif
    return fail("usage error");
}
