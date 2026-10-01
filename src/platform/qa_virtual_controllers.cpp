#include "tetrisphere/qa_virtual_controllers.h"

#include <json.hpp>

#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace tetrisphere {
namespace {
constexpr std::size_t max_file_bytes = 65536;
constexpr std::size_t max_line_bytes = 1024;
constexpr std::size_t max_actions = 512;

SDL_GameControllerButton parse_button(const std::string& name) {
    if (name == "south") return SDL_CONTROLLER_BUTTON_A;
    if (name == "east") return SDL_CONTROLLER_BUTTON_B;
    if (name == "west") return SDL_CONTROLLER_BUTTON_X;
    if (name == "north") return SDL_CONTROLLER_BUTTON_Y;
    if (name == "start") return SDL_CONTROLLER_BUTTON_START;
    if (name == "back") return SDL_CONTROLLER_BUTTON_BACK;
    if (name == "up") return SDL_CONTROLLER_BUTTON_DPAD_UP;
    if (name == "down") return SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    if (name == "left") return SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    if (name == "right") return SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    if (name == "left-shoulder") return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    if (name == "right-shoulder") return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    return SDL_CONTROLLER_BUTTON_INVALID;
}

bool inside_directory(const std::filesystem::path& child,
                      const std::filesystem::path& directory) {
    const auto relative = child.lexically_relative(directory);
    if (relative.empty() || relative == ".") return false;
    for (const auto& part : relative)
        if (part == "..") return false;
    return !relative.is_absolute();
}
} // namespace

QaVirtualControllers::~QaVirtualControllers() { close(); }

bool QaVirtualControllers::initialize(const std::filesystem::path& actions_file,
                                      const std::filesystem::path& data_dir,
                                      std::string& error) {
    error.clear();
    if (active_ || failed_) { error = "QA controllers already initialized"; return false; }
    if (!actions_file.is_absolute() || !data_dir.is_absolute()) {
        error = "QA requires absolute actions and data directory paths";
        return false;
    }
    std::error_code ec;
    const auto canonical_data = std::filesystem::canonical(data_dir, ec);
    if (ec) { error = "QA data directory unavailable"; return false; }
    const auto canonical_file = std::filesystem::canonical(actions_file, ec);
    if (ec || !std::filesystem::is_regular_file(canonical_file) ||
        !inside_directory(canonical_file, canonical_data)) {
        error = "QA actions file must be inside the explicit data directory";
        return false;
    }
    const auto size = std::filesystem::file_size(canonical_file, ec);
    if (ec || size > max_file_bytes) { error = "QA actions file exceeds limit"; return false; }
    path_ = canonical_file;
    for (int port = 0; port < 2; ++port) {
        const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 15, 2);
        if (index >= 0) ids_[port] = SDL_JoystickGetDeviceInstanceID(index);
        if (index < 0 || ids_[port] < 0 || !SDL_IsGameController(index)) {
            if (index >= 0 && ids_[port] < 0) SDL_JoystickDetachVirtual(index);
            error = "QA virtual gamecontroller attachment failed";
            close();
            return false;
        }
        joysticks_[port] = SDL_JoystickOpen(index);
        if (ids_[port] < 0 || joysticks_[port] == nullptr) {
            error = "QA virtual joystick open failed";
            close();
            return false;
        }
    }
    active_ = true;
    return true;
}

bool QaVirtualControllers::fail(const std::string& message, std::string& error) {
    error = message;
    failed_ = true;
    for (auto* joystick : joysticks_) {
        if (joystick == nullptr) continue;
        for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; ++button)
            SDL_JoystickSetVirtualButton(joystick, button, 0);
    }
    SDL_JoystickUpdate();
    return false;
}

bool QaVirtualControllers::pump(std::vector<QaControllerAction>& applied,
                                std::string& error) {
    applied.clear();
    error.clear();
    if (!active_ || failed_) return fail("QA controllers are not active", error);
    std::error_code ec;
    const auto size = std::filesystem::file_size(path_, ec);
    if (ec || size > max_file_bytes) return fail("QA actions file missing or too large", error);
    std::ifstream stream(path_, std::ios::binary);
    if (!stream) return fail("QA actions file unreadable", error);
    const std::string contents((std::istreambuf_iterator<char>(stream)),
                               std::istreambuf_iterator<char>());
    if (stream.bad() || contents.size() > max_file_bytes ||
        contents.size() < consumed_prefix_.size() ||
        contents.compare(0, consumed_prefix_.size(), consumed_prefix_) != 0)
        return fail("QA actions file was truncated or replaced", error);
    std::size_t cursor = consumed_prefix_.size();
    const auto first_complete = contents.find('\n', cursor);
    if (first_complete != std::string::npos &&
        contents.find('\n', first_complete + 1) != std::string::npos)
        return fail("QA actions file has multiple unacknowledged commands", error);
    while (cursor < contents.size()) {
        const auto newline = contents.find('\n', cursor);
        if (newline == std::string::npos) {
            if (contents.size() - cursor > max_line_bytes)
                return fail("QA partial action exceeds line limit", error);
            break;
        }
        if (newline - cursor > max_line_bytes)
            return fail("QA action exceeds line limit", error);
        std::string line = contents.substr(cursor, newline - cursor);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto value = nlohmann::json::parse(line, nullptr, false);
        if (value.is_discarded() || !value.is_object() || value.size() != 4 ||
            !value.contains("seq") || !value["seq"].is_number_integer() ||
            !value.contains("port") || !value["port"].is_number_integer() ||
            !value.contains("button") || !value["button"].is_string() ||
            !value.contains("pressed") || !value["pressed"].is_boolean())
            return fail("QA action JSONL shape invalid", error);
        const auto button = parse_button(value["button"].get<std::string>());
        const auto pressed = value["pressed"].get<bool>();
        if (value["seq"] != next_seq_ || action_count_ >= max_actions ||
            (value["port"] != 0 && value["port"] != 1) ||
            button == SDL_CONTROLLER_BUTTON_INVALID)
            return fail("QA action sequence, port, or button invalid", error);
        const int port = value["port"] == 0 ? 0 : 1;
        if (SDL_JoystickSetVirtualButton(joysticks_[port], button, pressed ? 1 : 0) != 0)
            return fail("QA virtual button update failed", error);
        applied.push_back({next_seq_, port, button, pressed});
        ++next_seq_;
        ++action_count_;
        cursor = newline + 1;
    }
    consumed_prefix_.assign(contents, 0, cursor);
    SDL_JoystickUpdate();
    SDL_PumpEvents();
    return true;
}

void QaVirtualControllers::close() {
    if (joysticks_[0] == nullptr && joysticks_[1] == nullptr &&
        ids_[0] < 0 && ids_[1] < 0) return;
    for (auto* joystick : joysticks_) {
        if (joystick == nullptr) continue;
        for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; ++button)
            SDL_JoystickSetVirtualButton(joystick, button, 0);
    }
    SDL_JoystickUpdate();
    for (auto& joystick : joysticks_) {
        if (joystick != nullptr) SDL_JoystickClose(joystick);
        joystick = nullptr;
    }
    for (const auto id : ids_) {
        if (id < 0) continue;
        for (int index = 0; index < SDL_NumJoysticks(); ++index) {
            if (SDL_JoystickGetDeviceInstanceID(index) == id) {
                SDL_JoystickDetachVirtual(index);
                break;
            }
        }
    }
    ids_ = {{-1, -1}};
    active_ = false;
}

} // namespace tetrisphere
