#pragma once

#include <SDL.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tetrisphere {

struct QaControllerAction {
    std::uint64_t seq = 0;
    int port = -1;
    SDL_GameControllerButton button = SDL_CONTROLLER_BUTTON_INVALID;
    bool pressed = false;
};

// Private opt-in QA input, activated only by an explicit data directory and
// JSONL file under that directory. All calls occur on the SDL event thread.
class QaVirtualControllers {
public:
    QaVirtualControllers() = default;
    ~QaVirtualControllers();
    QaVirtualControllers(const QaVirtualControllers&) = delete;
    QaVirtualControllers& operator=(const QaVirtualControllers&) = delete;

    bool initialize(const std::filesystem::path& actions_file,
                    const std::filesystem::path& data_dir, std::string& error);
    bool pump(std::vector<QaControllerAction>& applied, std::string& error);
    std::array<SDL_JoystickID, 2> instance_ids() const { return ids_; }
    void close();

private:
    bool fail(const std::string& message, std::string& error);
    std::filesystem::path path_;
    std::string consumed_prefix_;
    std::array<SDL_Joystick*, 2> joysticks_{};
    std::array<SDL_JoystickID, 2> ids_{{-1, -1}};
    std::uint64_t next_seq_ = 1;
    std::size_t action_count_ = 0;
    bool active_ = false;
    bool failed_ = false;
};

} // namespace tetrisphere
