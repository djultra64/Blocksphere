#include "tetrisphere/controls_sdl.h"
#include "tetrisphere/pc_settings_hook.h"
#include <optional>
#include <array>
#include <map>
namespace tetrisphere {
namespace {
std::optional<MagicControl> key_control(SDL_Scancode key) {
 switch(key) {
 case SDL_SCANCODE_Z:return MagicControl::South;case SDL_SCANCODE_X:return MagicControl::East;
 case SDL_SCANCODE_A:return MagicControl::West;case SDL_SCANCODE_I:return MagicControl::North;
 case SDL_SCANCODE_K:return MagicControl::TriggerLeft;case SDL_SCANCODE_L:return MagicControl::TriggerRight;
 case SDL_SCANCODE_J:return MagicControl::LeftShoulder;case SDL_SCANCODE_S:return MagicControl::RightShoulder;
 default:return {};
 }
}
std::optional<MagicControl> button_control(Uint8 button) {
 switch(button) {
 case SDL_CONTROLLER_BUTTON_A:return MagicControl::South;case SDL_CONTROLLER_BUTTON_B:return MagicControl::East;
 case SDL_CONTROLLER_BUTTON_X:return MagicControl::West;case SDL_CONTROLLER_BUTTON_Y:return MagicControl::North;
 case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:return MagicControl::LeftShoulder;
 case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:return MagicControl::RightShoulder;
 default:return {};
 }
}
void press_control(const ControlsEditor& view,MagicControl control,ControllerFamily family) {
 if(view.capturing()) {controls_editor_capture(control,family);return;}
 const int code=static_cast<int>(control)-4;
 if(code==static_cast<int>(view.applied().bindings.confirm))controls_editor_command(ControlsCommand::Activate);
 else if(code==static_cast<int>(view.applied().bindings.cancel))controls_editor_command(ControlsCommand::Back);
}
}
bool handle_controls_event(const SDL_Event& event,bool assigned,ControllerFamily family) {
 static std::map<SDL_JoystickID,std::array<int,4>> axes;
 int previous_axis=0,current_axis=0;
 if(assigned && event.type==SDL_CONTROLLERAXISMOTION) {
  int slot=-1;
  switch(event.caxis.axis) {
  case SDL_CONTROLLER_AXIS_LEFTX:slot=0;break;case SDL_CONTROLLER_AXIS_LEFTY:slot=1;break;
  case SDL_CONTROLLER_AXIS_TRIGGERLEFT:slot=2;break;case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:slot=3;break;
  }
  if(slot>=0) {
   auto& previous=axes[event.caxis.which][slot];previous_axis=previous;
   current_axis=event.caxis.value>16000 ? 1:event.caxis.value< -16000 ? -1:0;
   previous=current_axis;
  }
 }
 if(event.type==SDL_CONTROLLERDEVICEREMOVED)axes.erase(event.cdevice.which);
 if(auto graphics=pc_settings_snapshot();graphics && !graphics->closed()) {
  auto send=[](ControlsCommand command){pc_settings_command(command);};
  auto face=[&](MagicControl control) {
   const int code=int(control)-4;
   if(code==int(graphics->settings().bindings.confirm))send(ControlsCommand::Activate);
   else if(code==int(graphics->settings().bindings.cancel))send(ControlsCommand::Back);
  };
  if(event.type==SDL_KEYDOWN && !event.key.repeat) {
   pc_settings_device(ControllerFamily::Keyboard);
   switch(event.key.keysym.scancode) {
   case SDL_SCANCODE_UP:send(ControlsCommand::Previous);break;
   case SDL_SCANCODE_DOWN:send(ControlsCommand::Next);break;
   case SDL_SCANCODE_LEFT:send(ControlsCommand::Left);break;
   case SDL_SCANCODE_RIGHT:send(ControlsCommand::Right);break;
   case SDL_SCANCODE_RETURN:send(ControlsCommand::Activate);break;
   case SDL_SCANCODE_ESCAPE:send(ControlsCommand::Back);break;
   default:if(auto control=key_control(event.key.keysym.scancode))face(*control);break;
   }
  } else if(assigned && event.type==SDL_CONTROLLERBUTTONDOWN) {
   pc_settings_device(family);
   switch(event.cbutton.button) {
   case SDL_CONTROLLER_BUTTON_DPAD_UP:send(ControlsCommand::Previous);break;
   case SDL_CONTROLLER_BUTTON_DPAD_DOWN:send(ControlsCommand::Next);break;
   case SDL_CONTROLLER_BUTTON_DPAD_LEFT:send(ControlsCommand::Left);break;
   case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:send(ControlsCommand::Right);break;
   case SDL_CONTROLLER_BUTTON_START:case SDL_CONTROLLER_BUTTON_BACK:send(ControlsCommand::Back);break;
   default:if(auto control=button_control(event.cbutton.button))face(*control);break;
   }
  } else if(assigned && event.type==SDL_CONTROLLERAXISMOTION && current_axis && previous_axis!=current_axis) {
   pc_settings_device(family);
   if(event.caxis.axis==SDL_CONTROLLER_AXIS_LEFTX)send(current_axis>0 ? ControlsCommand::Right:ControlsCommand::Left);
   if(event.caxis.axis==SDL_CONTROLLER_AXIS_LEFTY)send(current_axis>0 ? ControlsCommand::Next:ControlsCommand::Previous);
  }
  return true;
 }
 auto view=controls_editor_snapshot();
 if(!view)return false;
 if(event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_FOCUS_LOST) {
  controls_editor_command(ControlsCommand::Disconnect);axes.clear();return true;
 }
 if(event.type==SDL_CONTROLLERDEVICEREMOVED && assigned) {
  controls_editor_command(ControlsCommand::Disconnect);axes.clear();return true;
 }
 if(event.type==SDL_KEYDOWN) {
  if(event.key.repeat)return true;
  controls_editor_device(ControllerFamily::Keyboard);
  const auto key=event.key.keysym.scancode;
  if(key==SDL_SCANCODE_ESCAPE)controls_editor_command(ControlsCommand::Back);
  else if(view->capturing()) {if(auto control=key_control(key))controls_editor_capture(*control,ControllerFamily::Keyboard);}
  else if(key==SDL_SCANCODE_RETURN)controls_editor_command(ControlsCommand::Activate);
  else if(key==SDL_SCANCODE_UP)controls_editor_command(ControlsCommand::Previous);
  else if(key==SDL_SCANCODE_DOWN)controls_editor_command(ControlsCommand::Next);
  else if(key==SDL_SCANCODE_LEFT)controls_editor_command(ControlsCommand::Left);
  else if(key==SDL_SCANCODE_RIGHT)controls_editor_command(ControlsCommand::Right);
  else if(auto control=key_control(key))press_control(*view,*control,ControllerFamily::Keyboard);
  return true;
 }
 if(assigned && event.type==SDL_CONTROLLERBUTTONDOWN) {
  controls_editor_device(family);
  if(auto control=button_control(event.cbutton.button))press_control(*view,*control,family);
  else if(event.cbutton.button==SDL_CONTROLLER_BUTTON_START || event.cbutton.button==SDL_CONTROLLER_BUTTON_BACK)
   controls_editor_command(ControlsCommand::Back);
  else if(!view->capturing()) {
   switch(event.cbutton.button) {
   case SDL_CONTROLLER_BUTTON_DPAD_UP:controls_editor_command(ControlsCommand::Previous);break;
   case SDL_CONTROLLER_BUTTON_DPAD_DOWN:controls_editor_command(ControlsCommand::Next);break;
   case SDL_CONTROLLER_BUTTON_DPAD_LEFT:controls_editor_command(ControlsCommand::Left);break;
   case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:controls_editor_command(ControlsCommand::Right);break;
   }
  }
 }
 if(assigned && event.type==SDL_CONTROLLERAXISMOTION) {
  const auto axis=event.caxis.axis;
  if(view->capturing()) {
   if(current_axis==1 && previous_axis!=1 && (axis==SDL_CONTROLLER_AXIS_TRIGGERLEFT || axis==SDL_CONTROLLER_AXIS_TRIGGERRIGHT))
    controls_editor_capture(axis==SDL_CONTROLLER_AXIS_TRIGGERLEFT ? MagicControl::TriggerLeft:MagicControl::TriggerRight,family);
  } else if(axis==SDL_CONTROLLER_AXIS_LEFTX || axis==SDL_CONTROLLER_AXIS_LEFTY) {
   const int current=current_axis;
   if(current && previous_axis!=current) {
    controls_editor_device(family);
    controls_editor_command(axis==SDL_CONTROLLER_AXIS_LEFTX
      ? (current>0 ? ControlsCommand::Right:ControlsCommand::Left)
      : (current>0 ? ControlsCommand::Next:ControlsCommand::Previous));
   }
  }
 }
 return true;
}
}
