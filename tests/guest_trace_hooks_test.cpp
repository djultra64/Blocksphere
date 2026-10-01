#include "tetrisphere/guest_trace_hooks.h"

#include <chrono>
#include <cstdlib>
#include <system_error>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
int fail(const char* why) { std::cerr << why << '\n'; return 1; }
struct TraceCleanup {
    std::filesystem::path path;
    ~TraceCleanup() {
        // The recorder owns an open stream until process exit. On Windows its
        // stream must close before the file can be removed.
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};
std::uint64_t guest(std::uint32_t address) {
    return static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int32_t>(address)));
}
void set_env(const char* key, const char* value) {
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
}
} // namespace

int main() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
        ("tetrisphere-guest-trace-" + std::to_string(stamp) + ".txt");
    static TraceCleanup cleanup{path};
    set_env("TETRISPHERE_TRACE_PATH", path.string().c_str());
    set_env("TETRISPHERE_TRACE_MAX_FRAMES", "1");
    std::vector<std::uint8_t> memory(0x900000);
    std::uint8_t* rdram = memory.data();
    recomp_context context{};
    MEM_H(0, guest(0x800E1710u)) = 3;
    MEM_H(0, guest(0x800E1714u)) = 5;
    MEM_W(0, guest(0x800E2AA0u)) = 0x12345678;
    MEM_W(0, guest(0x800E4478u)) = 7;
    MEM_W(0, guest(0x800E4480u)) = 8;
    MEM_W(0, guest(0x801125BCu)) = 101;
    MEM_W(0, guest(0x80115824u)) = 202;
    MEM_W(0, guest(0x80112D18u)) = 300;
    MEM_W(0, guest(0x80115F80u)) = 400;
    MEM_W(0, guest(0x80112D2Cu)) = 18059;
    MEM_W(0, guest(0x80115F94u)) = 7;
    MEM_W(0, guest(0x800E241Cu)) = 500;
    MEM_W(0, guest(0x80160C60u)) = 600;
    tetrisphere_trace_input_drain_begin(rdram, &context);
    context.r4 = 0x1000;
    context.r5 = 0x8000;
    context.r6 = 1;
    tetrisphere_trace_input_consumed(rdram, &context);
    tetrisphere_trace_rng_write(rdram, &context);
    MEM_H(0, guest(0x800E1710u)) = 4;
    tetrisphere_trace_input_drain_end(rdram, &context);
    tetrisphere_trace_input_drain_begin(rdram, &context);
    tetrisphere_trace_input_drain_end(rdram, &context);
    std::ifstream input(path);
    const std::string trace{std::istreambuf_iterator<char>(input),
                            std::istreambuf_iterator<char>()};
    if (trace.find("tetrisphere-guest-trace-v1\n") != 0 ||
        trace.find("input_ring_read_cursor_begin=0000000000000003") == std::string::npos ||
        trace.find("input_ring_read_cursor_end=0000000000000004") == std::string::npos ||
        trace.find("consumed_input[0].port=0000000000000001") == std::string::npos ||
        trace.find("consumed_input[0].raw_word1=0000000000008000") == std::string::npos ||
        trace.find("global_rng_word=0000000012345678") == std::string::npos ||
        trace.find("rng_update_count=0000000000000001") == std::string::npos ||
        trace.find("player_rng_word[1]=00000000000000ca") == std::string::npos ||
        trace.find("candidate_score_offset_2af8[1]=0000000000000190") == std::string::npos ||
        trace.find("candidate_timer_offset_2b0c[0]=000000000000468b") == std::string::npos ||
        trace.find("candidate_hud_word_800e241c=00000000000001f4") == std::string::npos ||
        trace.find("candidate_delta_word_80160c60=0000000000000258") == std::string::npos)
        return fail("guest hooks did not capture consumed input and observed state");
    if (trace.find("tetrisphere-canonical-v1", trace.find("tetrisphere-canonical-v1") + 1)
        != std::string::npos)
        return fail("trace frame cap was ignored");
    return 0;
}
