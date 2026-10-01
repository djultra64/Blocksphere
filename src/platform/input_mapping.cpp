#include "tetrisphere/linux_platform.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace tetrisphere {
namespace {

ControllerBindings active_bindings{};
MagicControl active_magic = MagicControl::West;

std::string normalized(const char* value) {
    std::string result = value == nullptr ? "" : value;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

} // namespace

ControllerFamily detect_controller_family(const char* raw_name) {
    const auto name = normalized(raw_name);
    if (name.find("nintendo") != std::string::npos ||
        name.find("switch") != std::string::npos ||
        name.find("joy-con") != std::string::npos) {
        return ControllerFamily::NintendoSwitch;
    }
    if (name.find("playstation") != std::string::npos ||
        name.find("dualsense") != std::string::npos ||
        name.find("dualshock") != std::string::npos ||
        name.find("sony") != std::string::npos) {
        return ControllerFamily::PlayStation;
    }
    return name.empty() ? ControllerFamily::Keyboard : ControllerFamily::Xbox;
}

ControllerFamily select_controller_family(const char* detected_name,
                                          const char* manual_override) {
    const auto selected = normalized(manual_override);
    if (selected == "keyboard") return ControllerFamily::Keyboard;
    if (selected == "xbox") return ControllerFamily::Xbox;
    if (selected == "playstation") return ControllerFamily::PlayStation;
    if (selected == "nintendo" || selected == "nintendo-switch" ||
        selected == "switch") {
        return ControllerFamily::NintendoSwitch;
    }
    return detect_controller_family(detected_name);
}

const char* controller_family_name(ControllerFamily family) {
    switch (family) {
        case ControllerFamily::Keyboard: return "keyboard";
        case ControllerFamily::Xbox: return "xbox";
        case ControllerFamily::PlayStation: return "playstation";
        case ControllerFamily::NintendoSwitch: return "nintendo-switch";
    }
    return "unknown";
}

PhysicalButton default_physical_button(LogicalAction action) {
    switch (action) {
        case LogicalAction::Confirm: return PhysicalButton::South;
        case LogicalAction::Cancel: return PhysicalButton::East;
        case LogicalAction::FaceLeft: return PhysicalButton::West;
        case LogicalAction::FaceUp: return PhysicalButton::North;
    }
    return PhysicalButton::South;
}

PhysicalButton physical_button_for(const ControllerBindings& bindings,
                                   LogicalAction action) {
    switch (action) {
        case LogicalAction::Confirm: return bindings.confirm;
        case LogicalAction::Cancel: return bindings.cancel;
        case LogicalAction::FaceLeft: return bindings.face_left;
        case LogicalAction::FaceUp: return bindings.face_up;
    }
    return bindings.confirm;
}

void set_controller_bindings(const ControllerBindings& bindings) {
    active_bindings = bindings;
}

ControllerBindings controller_bindings() { return active_bindings; }

void set_magic_control(MagicControl control) { active_magic = control; }

MagicControl magic_control() { return active_magic; }

const char* magic_control_label(ControllerFamily family, MagicControl control) {
    constexpr const char* keyboard[] = {"K", "L", "J", "S", "Z", "X", "A", "I"};
    constexpr const char* xbox[] = {"LT", "RT", "LB", "RB", "A", "B", "X", "Y"};
    constexpr const char* playstation[] = {
        "L2", "R2", "L1", "R1", "Cross", "Circle", "Square", "Triangle"};
    constexpr const char* nintendo[] = {"ZL", "ZR", "L", "R", "B", "A", "Y", "X"};
    const auto index = static_cast<unsigned>(control);
    if (index >= 8) return "?";
    switch (family) {
        case ControllerFamily::Keyboard: return keyboard[index];
        case ControllerFamily::Xbox: return xbox[index];
        case ControllerFamily::PlayStation: return playstation[index];
        case ControllerFamily::NintendoSwitch: return nintendo[index];
    }
    return "?";
}

const char* physical_button_label(ControllerFamily family, PhysicalButton button) {
    constexpr const char* keyboard[] = {"Z", "X", "A", "I"};
    constexpr const char* xbox[] = {"A", "B", "X", "Y"};
    constexpr const char* playstation[] = {"Cross", "Circle", "Square", "Triangle"};
    constexpr const char* nintendo[] = {"B", "A", "Y", "X"};
    const auto index = static_cast<unsigned>(button);
    switch (family) {
        case ControllerFamily::Keyboard: return keyboard[index];
        case ControllerFamily::Xbox: return xbox[index];
        case ControllerFamily::PlayStation: return playstation[index];
        case ControllerFamily::NintendoSwitch: return nintendo[index];
    }
    return "?";
}

const char* logical_action_label(ControllerFamily family,
                                 const ControllerBindings& bindings,
                                 LogicalAction action) {
    return physical_button_label(family, physical_button_for(bindings, action));
}

} // namespace tetrisphere
