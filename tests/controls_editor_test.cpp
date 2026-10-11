#include "tetrisphere/controls_editor.h"
#include <cassert>
using namespace tetrisphere;
int main() {
 // A shared Magic button suppresses the low-level Restart input; reject that
 // combination instead of promising a working Restart action.
 ControlsEditor collision(Settings{},ControllerFamily::Xbox);
 collision.focus(ControlsEditor::Magic);collision.select();
 assert(collision.capture(MagicControl::North));
 collision.focus(ControlsEditor::Apply);collision.select();
 assert(!collision.take_apply() && collision.dirty() && !collision.saving());
 assert(collision.status()=="RESTART AND MAGIC MUST DIFFER");
 assert(collision.applied().magic==MagicControl::West);
 Settings original; original.graphics.msaa_samples=4;
 original.magic=MagicControl::RightShoulder;original.bindings.face_left=PhysicalButton::East;
 ControlsEditor editor(original,ControllerFamily::Xbox);
 editor.focus(4); editor.select(); assert(editor.capturing());
 assert(editor.capture(MagicControl::LeftShoulder));
 assert(editor.dirty() && editor.pending().magic==MagicControl::LeftShoulder);
 assert(editor.applied().magic==MagicControl::RightShoulder);
 editor.focus(7);editor.select();auto request=editor.take_apply();
 assert(request && request->magic==MagicControl::LeftShoulder && editor.saving());
 editor.complete_apply(false);assert(editor.dirty() && !editor.saving());
 assert(editor.applied().magic==MagicControl::RightShoulder && !editor.status().empty());
 editor.select();assert(editor.take_apply());editor.complete_apply(true);
 assert(!editor.dirty() && editor.applied().magic==MagicControl::LeftShoulder);
 // Confirm and Back remain usable while a new navigation map is staged.
 editor.focus(1);editor.select();assert(editor.capture(MagicControl::East));
 assert(editor.applied().bindings.confirm==PhysicalButton::South);
 editor.focus(7);editor.select();assert(!editor.take_apply()); // Same Confirm/Back is ambiguous.
 editor.focus(2);editor.select();assert(editor.capture(MagicControl::South));
 editor.focus(7);editor.select();request=editor.take_apply();assert(request);
 assert(request->bindings.confirm==PhysicalButton::East && request->bindings.cancel==PhysicalButton::South);
 editor.complete_apply(true);
 // Unsupported non-face capture stays in capture; Escape/back explicitly cancels.
 editor.focus(3);editor.select();assert(!editor.capture(MagicControl::TriggerLeft));
 assert(editor.capturing());editor.back();assert(!editor.capturing());
 editor.focus(5);editor.select();assert(editor.dirty()); // Defaults are only staged.
 assert(editor.applied().bindings.confirm==PhysicalButton::East);
 assert(editor.pending().bindings.face_left==PhysicalButton::East);
 editor.focus(6);editor.select();assert(!editor.dirty());
 assert(editor.pending().graphics.msaa_samples==4);
 // Device labels represent physical positions, independent of navigation device.
 assert(editor.labels(ControllerFamily::NintendoSwitch)[0]=="A");
 assert(editor.labels(ControllerFamily::Xbox)[0]=="B");
 assert(editor.labels(ControllerFamily::PlayStation)[0]=="Circle");
 editor.set_device(ControllerFamily::PlayStation);
 editor.set_device(ControllerFamily::Keyboard);assert(editor.display_family()==ControllerFamily::PlayStation);
 editor.focus(0);editor.cycle_family(1);assert(editor.pending().family_override=="keyboard");
 editor.cycle_family(1);assert(editor.display_family()==ControllerFamily::Xbox);
 // Unsaved exit stays inline until a decision is made; saving blocks editing.
 editor.back();assert(editor.exit_pending() && !editor.closed());
 editor.focus(7);editor.select();assert(editor.take_apply());
 editor.focus(5);editor.select();assert(editor.saving());
 editor.complete_apply(false);assert(!editor.closed());
 editor.focus(6);editor.select();assert(editor.closed());
}
