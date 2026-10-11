#include "tetrisphere/controls_bridge.h"
#include <cassert>
using namespace tetrisphere;
int main() {
 open_controls_editor(Settings{},ControllerFamily::NintendoSwitch);
 assert(controls_editor_active() && controls_editor_blocks_game());
 controls_editor_command(ControlsCommand::Activate);
 controls_editor_capture(MagicControl::North,ControllerFamily::NintendoSwitch);
 auto view=controls_editor_snapshot();assert(view && view->pending().bindings.confirm==PhysicalButton::North);
 assert(view->labels(view->display_family())[0]=="X");
 controls_editor_command(ControlsCommand::Back);assert(controls_editor_snapshot()->exit_pending());
 controls_editor_command(ControlsCommand::Left); // Discard
 controls_editor_command(ControlsCommand::Activate);
 assert(!controls_editor_active() && controls_editor_blocks_game());
 controls_editor_release_guard(true);assert(controls_editor_blocks_game());
 controls_editor_release_guard(false);assert(!controls_editor_blocks_game());
 open_controls_editor(Settings{},ControllerFamily::Xbox);
 controls_editor_command(ControlsCommand::Activate);
 controls_editor_command(ControlsCommand::Disconnect);
 assert(!controls_editor_snapshot()->capturing() && !controls_editor_snapshot()->dirty());
}
