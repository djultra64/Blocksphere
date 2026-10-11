#include "tetrisphere/controls_sdl.h"
#include "tetrisphere/pc_settings_hook.h"
#include <cassert>
#include <vector>
using namespace tetrisphere;
int main() {
 open_controls_editor(Settings{},ControllerFamily::Xbox);
 SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_SCANCODE_Z;
 assert(handle_controls_event(event,false,ControllerFamily::Keyboard));
 assert(controls_editor_snapshot()->capturing());
 event.key.repeat=1;event.key.keysym.scancode=SDL_SCANCODE_I;
 handle_controls_event(event,false,ControllerFamily::Keyboard);
 assert(controls_editor_snapshot()->capturing()); // OS repeat cannot accidentally bind.
 event.key.repeat=0;handle_controls_event(event,false,ControllerFamily::Keyboard);
 assert(!controls_editor_snapshot()->capturing());
 assert(controls_editor_snapshot()->pending().bindings.confirm==PhysicalButton::North);
 assert(controls_editor_snapshot()->applied().bindings.confirm==PhysicalButton::South);
 event.key.keysym.scancode=SDL_SCANCODE_Z;handle_controls_event(event,false,ControllerFamily::Keyboard);
 assert(controls_editor_snapshot()->capturing()); // Old Confirm still navigates.
 event.type=SDL_CONTROLLERBUTTONDOWN;event.cbutton.button=SDL_CONTROLLER_BUTTON_B;
 handle_controls_event(event,false,ControllerFamily::NintendoSwitch);
 assert(controls_editor_snapshot()->capturing()); // Unassigned controller ignored.
 handle_controls_event(event,true,ControllerFamily::NintendoSwitch);
 assert(!controls_editor_snapshot()->capturing()); // Back physical position can be captured.
 assert(controls_editor_snapshot()->pending().bindings.confirm==PhysicalButton::East);
 assert(controls_editor_snapshot()->labels(ControllerFamily::NintendoSwitch)[0]=="A");
 event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_SCANCODE_RETURN;
 handle_controls_event(event,false,ControllerFamily::Keyboard);assert(controls_editor_snapshot()->capturing());
 event.key.keysym.scancode=SDL_SCANCODE_ESCAPE;handle_controls_event(event,false,ControllerFamily::Keyboard);
 assert(!controls_editor_snapshot()->capturing());
 // A trigger already held before capture must be released before it can bind.
 event.type=SDL_CONTROLLERAXISMOTION;event.caxis.which=7;
 event.caxis.axis=SDL_CONTROLLER_AXIS_TRIGGERLEFT;event.caxis.value=20000;
 handle_controls_event(event,true,ControllerFamily::Xbox);
 for(int n=0;n<3;++n)controls_editor_command(ControlsCommand::Next);
 controls_editor_command(ControlsCommand::Activate);assert(controls_editor_snapshot()->capturing());
 event.caxis.value=21000;handle_controls_event(event,true,ControllerFamily::Xbox);
 assert(controls_editor_snapshot()->capturing());
 event.caxis.value=0;handle_controls_event(event,true,ControllerFamily::Xbox);
 event.caxis.value=20000;handle_controls_event(event,true,ControllerFamily::Xbox);
 assert(!controls_editor_snapshot()->capturing());
 assert(controls_editor_snapshot()->pending().magic==MagicControl::TriggerLeft);
 // Graphics also follows the actual navigation device, including player two.
 initialize_pc_settings(Settings{});
 std::vector<std::uint8_t> memory(0x800000);auto* rdram=memory.data();recomp_context ctx{};
 MEM_H(0,0xFFFFFFFF800E121Cull)=3;MEM_H(0,0xFFFFFFFF800E1220ull)=4;
 tetrisphere_pc_settings_hook(rdram,&ctx,0x8004F680u);
 event.type=SDL_CONTROLLERBUTTONDOWN;event.cbutton.button=SDL_CONTROLLER_BUTTON_DPAD_DOWN;
 handle_controls_event(event,true,ControllerFamily::PlayStation);
 assert(pc_settings_snapshot()->focused()==1);
 assert(pc_settings_navigation_family()==ControllerFamily::PlayStation);
 handle_controls_event(event,false,ControllerFamily::NintendoSwitch);
 assert(pc_settings_snapshot()->focused()==1);
 assert(pc_settings_navigation_family()==ControllerFamily::PlayStation);
 event.type=SDL_KEYDOWN;event.key.repeat=0;event.key.keysym.scancode=SDL_SCANCODE_UP;
 handle_controls_event(event,false,ControllerFamily::Keyboard);
 assert(pc_settings_snapshot()->focused()==0);
 assert(pc_settings_navigation_family()==ControllerFamily::Keyboard);
}
