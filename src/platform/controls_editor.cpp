#include "tetrisphere/controls_editor.h"
#include <algorithm>
namespace tetrisphere {
namespace {
constexpr const char* families[]{"auto","keyboard","xbox","playstation","nintendo-switch"};
bool same(const Settings& a,const Settings& b) {
 return a.family_override==b.family_override && a.magic==b.magic &&
 a.bindings.confirm==b.bindings.confirm && a.bindings.cancel==b.bindings.cancel &&
 a.bindings.face_left==b.bindings.face_left && a.bindings.face_up==b.bindings.face_up;
}
}
ControlsEditor::ControlsEditor(Settings settings,ControllerFamily device)
 : applied_(settings),pending_(std::move(settings)),device_(device),navigation_device_(device) {}
bool ControlsEditor::dirty() const {return !same(applied_,pending_);}
void ControlsEditor::focus(int row) {if(!capturing_ && !saving_)focused_=std::clamp(row,0,int(Apply));}
void ControlsEditor::move(int direction) {focus((focused_+direction+RowCount)%RowCount);}
void ControlsEditor::set_device(ControllerFamily device) {
 navigation_device_=device;
 // Keyboard navigation does not replace the controller diagram being edited.
 if(device!=ControllerFamily::Keyboard)device_=device;
}
ControllerFamily ControlsEditor::display_family() const {
 if(pending_.family_override=="auto")return device_;
 return select_controller_family("",pending_.family_override.c_str());
}
ControllerFamily ControlsEditor::navigation_family() const {
 if(navigation_device_==ControllerFamily::Keyboard || applied_.family_override=="auto")return navigation_device_;
 return select_controller_family("",applied_.family_override.c_str());
}
void ControlsEditor::cycle_family(int direction) {
 if(capturing_ || saving_)return;
 int index=0;for(int i=0;i<5;++i)if(pending_.family_override==families[i])index=i;
 pending_.family_override=families[(index+direction+5)%5];status_.clear();
}
void ControlsEditor::select() {
 if(capturing_ || saving_)return;
 status_.clear();
 if(focused_==Layout) {cycle_family(1);return;}
 if(focused_<=Magic) {capturing_=true;exit_pending_=false;return;}
 if(focused_==Defaults) {
  // Keep the unexposed legacy input intact when resetting visible actions.
  const auto legacy_left=pending_.bindings.face_left;
  pending_.bindings=ControllerBindings{};pending_.bindings.face_left=legacy_left;pending_.magic=MagicControl::West;
  pending_.family_override="auto";exit_pending_=false;return;
 }
 if(focused_==Discard) {pending_=applied_;closed_=exit_pending_;exit_pending_=false;return;}
 if(pending_.bindings.confirm==pending_.bindings.cancel) {
  status_="SELECT AND BACK MUST DIFFER";return;
 }
 if(static_cast<int>(pending_.magic)==4+static_cast<int>(pending_.bindings.face_up)) {
  status_="RESTART AND MAGIC MUST DIFFER";return;
 }
 if(!dirty()) {closed_=exit_pending_;exit_pending_=false;return;}
 request_=pending_;saving_=true;
}
void ControlsEditor::back() {
 if(saving_)return;
 if(capturing_) {capturing_=false;status_.clear();return;}
 if(exit_pending_) {exit_pending_=false;return;}
 if(!dirty()) {closed_=true;return;}
 exit_pending_=true;focused_=Apply;status_="APPLY OR DISCARD TO LEAVE";
}
bool ControlsEditor::capture(MagicControl control) {
 if(!capturing_ || saving_)return false;
 const int code=static_cast<int>(control);
 if(code<0 || code>7 || (focused_<Magic && code<4)) {
  status_="USE A FACE BUTTON";return false;
 }
 if(focused_==Magic)pending_.magic=control;
 else {
  const auto button=static_cast<PhysicalButton>(code-4);
  switch(focused_) {
   case Drop:pending_.bindings.confirm=button;break;
   case Slide:pending_.bindings.cancel=button;break;
   case RestartPuzzle:pending_.bindings.face_up=button;break;
   default:return false;
  }
 }
 capturing_=false;status_.clear();return true;
}
std::optional<Settings> ControlsEditor::take_apply() {auto value=std::move(request_);request_.reset();return value;}
void ControlsEditor::complete_apply(bool success) {
 if(!saving_)return;
 saving_=false;
 if(success) {applied_=pending_;closed_=exit_pending_;exit_pending_=false;status_="SAVED";}
 else status_="SAVE FAILED TRY AGAIN";
}
std::array<std::string,4> ControlsEditor::labels(ControllerFamily family) const {
 return {physical_button_label(family,pending_.bindings.confirm),
 physical_button_label(family,pending_.bindings.cancel),
 physical_button_label(family,pending_.bindings.face_up),magic_control_label(family,pending_.magic)};
}
}
