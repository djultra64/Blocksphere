#include "tetrisphere/controller_ports.h"
#include "tetrisphere/qa_virtual_controllers.h"

#include <SDL.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
int fail(const char* message) { std::cerr << message << '\n'; return 1; }
struct TempDirectory {
    std::filesystem::path path;
    TempDirectory() {
        const auto base = std::filesystem::temp_directory_path();
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            auto candidate = base / ("tetrisphere-qa-virtual-test-" +
                                     std::to_string(nonce) + "-" + std::to_string(attempt));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) {
                path = std::move(candidate);
                return;
            }
        }
    }
    ~TempDirectory() {
        if (!path.empty()) {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    }
};
void append(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary | std::ios::app);
    file << text;
}
}

int main() {
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0)
        return fail("SDL initialization failed");
    TempDirectory temp;
    if (temp.path.empty()) return fail("unique test directory unavailable");
    const auto& data = temp.path;
    const auto script = data / "actions.jsonl";
    std::ofstream(script, std::ios::trunc).close();
    const int physical_a = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 15, 2);
    const int physical_b = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 15, 2);
    if (physical_a < 0 || physical_b < 0) return fail("test devices failed to attach");
    tetrisphere::QaVirtualControllers qa;
    std::string error;
    if (!qa.initialize(script, data, error)) return fail("QA initialization failed");
    const auto ids = qa.instance_ids();
    if (ids[0] < 0 || ids[1] < 0 || ids[0] == ids[1]) return fail("QA IDs invalid");
    tetrisphere::ControllerPorts ports;
    if (!ports.set_preferred_ids(ids)) return fail("preferred IDs rejected");
    ports.open_all();
    auto pair = ports.snapshot();
    if (pair[0].instance_id != ids[0] || pair[1].instance_id != ids[1])
        return fail("QA IDs did not win over earlier devices");

    std::vector<tetrisphere::QaControllerAction> actions;
    append(script, R"({"seq":1,"port":1,"button":"south","pressed":true})");
    if (!qa.pump(actions, error) || !actions.empty() || ports.snapshot()[1].sample.south) {
        std::cerr << error << '\n';
        return fail("partial JSONL action became visible");
    }
    append(script, "\n");
    if (!qa.pump(actions, error) || actions.size() != 1 || actions[0].port != 1 ||
        !ports.snapshot()[1].sample.south || ports.snapshot()[0].sample.south)
        return fail("P2 south press did not reach independent snapshot");
    if (!qa.pump(actions, error) || !actions.empty())
        return fail("action was consumed more than once");
    append(script, R"({"seq":2,"port":1,"button":"south","pressed":false})" "\n");
    if (!qa.pump(actions, error) || actions.size() != 1 || ports.snapshot()[1].sample.south)
        return fail("P2 south release did not reach snapshot");
    append(script, R"({"seq":3,"port":1,"button":"south","pressed":true})" "\n");
    if (!qa.pump(actions, error) || !ports.snapshot()[1].sample.south)
        return fail("P2 did not press before parser failure");
    append(script, R"({"seq":3,"port":1,"button":"south","pressed":true})" "\n");
    if (qa.pump(actions, error) || error.empty() || ports.snapshot()[1].sample.south)
        return fail("repeated sequence did not fail closed");
    ports.close();
    qa.close();

    std::ofstream(script, std::ios::trunc).close();
    error.clear();
    tetrisphere::QaVirtualControllers qa_invalid;
    if (!qa_invalid.initialize(script, data, error)) return fail("second QA initialization failed");
    append(script, R"({"seq":1,"port":2,"button":"south","pressed":true})" "\n");
    if (qa_invalid.pump(actions, error) || error.empty()) return fail("invalid port accepted");
    qa_invalid.close();
    std::ofstream(script, std::ios::trunc).close();
    error.clear();
    tetrisphere::QaVirtualControllers qa_large_port;
    if (!qa_large_port.initialize(script, data, error)) return fail("large port test initialization failed");
    append(script, R"({"seq":1,"port":4294967296,"button":"south","pressed":true})" "\n");
    if (qa_large_port.pump(actions, error) || error.empty())
        return fail("large port wrapped into a valid port");
    qa_large_port.close();
    std::ofstream(script, std::ios::trunc).close();
    error.clear();
    tetrisphere::QaVirtualControllers qa_large_seq;
    if (!qa_large_seq.initialize(script, data, error)) return fail("large sequence test initialization failed");
    append(script, R"({"seq":18446744073709551615,"port":0,"button":"south","pressed":true})" "\n");
    if (qa_large_seq.pump(actions, error) || error.empty())
        return fail("large sequence accepted or caused an exception");
    qa_large_seq.close();
    std::ofstream(script, std::ios::trunc).close();
    error.clear();
    tetrisphere::QaVirtualControllers qa_batch;
    if (!qa_batch.initialize(script, data, error)) return fail("batch test initialization failed");
    append(script, R"({"seq":1,"port":1,"button":"south","pressed":true})" "\n"
                   R"({"seq":2,"port":1,"button":"south","pressed":false})" "\n");
    if (qa_batch.pump(actions, error) || error.empty())
        return fail("queued down and up were accepted in one pump");
    qa_batch.close();
    std::ofstream(script, std::ios::trunc).close();
    error.clear();
    tetrisphere::QaVirtualControllers qa_button;
    if (!qa_button.initialize(script, data, error)) return fail("button test initialization failed");
    append(script, R"({"seq":1,"port":0,"button":"unknown","pressed":true})" "\n");
    if (qa_button.pump(actions, error) || error.empty()) return fail("invalid button accepted");
    qa_button.close();
    std::ofstream(script, std::ios::trunc).close();
    error.clear();
    tetrisphere::QaVirtualControllers qa_replace;
    if (!qa_replace.initialize(script, data, error)) return fail("third QA initialization failed");
    append(script, R"({"seq":1,"port":0,"button":"south","pressed":true})" "\n");
    if (!qa_replace.pump(actions, error)) return fail("valid prefix rejected");
    std::ofstream(script, std::ios::trunc) << "{}\n";
    if (qa_replace.pump(actions, error) || error.empty()) return fail("truncated file accepted");
    qa_replace.close();

    SDL_Quit();
    return 0;
}
