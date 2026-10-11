#pragma once
#include "tetrisphere/controls_editor.h"
namespace tetrisphere {
enum class ControlsCommand { Previous,Next,Left,Right,Activate,Back,Disconnect };
void open_controls_editor(const Settings&,ControllerFamily);
std::optional<ControlsEditor> controls_editor_snapshot();
bool controls_editor_active();
bool controls_editor_blocks_game();
void controls_editor_release_guard(bool any_input_held);
void controls_editor_command(ControlsCommand);
void controls_editor_capture(MagicControl,ControllerFamily);
void controls_editor_device(ControllerFamily);
std::optional<Settings> take_controls_apply();
void complete_controls_apply(bool success);
}
