#pragma once
#include "recomp.h"
#include "tetrisphere/controls_bridge.h"
#include "tetrisphere/pc_settings_menu.h"
namespace tetrisphere {
std::optional<PCSettingsMenu> pc_settings_snapshot();
bool pc_settings_active();
bool pc_settings_blocks_game();
void pc_settings_release_guard(bool held);
void pc_settings_command(ControlsCommand command);
void pc_settings_device(ControllerFamily family);
ControllerFamily pc_settings_navigation_family();
void initialize_pc_settings(const Settings& settings);
struct PCSettingsRequest { Settings settings; int field; };
std::optional<PCSettingsRequest> take_pc_settings_change();
void acknowledge_controls_settings(const Settings& settings);
}
extern "C" void tetrisphere_pc_settings_hook(std::uint8_t*,recomp_context*,std::uint32_t);
