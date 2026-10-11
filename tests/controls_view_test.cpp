#include "tetrisphere/controls_view.h"
#include <cassert>
#include <set>
using namespace tetrisphere;
int main() {
 // Actual guest endpoint formula must retain every source column and row.
 for(unsigned width=3;width<=20;++width)for(unsigned height:{16u,24u}) {
  const auto right=400+((width*4096+4095)/2048)-5;
  const auto bottom=200+(((height+1)*4096+4095)/2048)-9;
  const auto restored=complete_half_scale_glyph(0xE4000000u|(right<<12)|bottom);
  assert(((restored>>12)&0xFFF)==400+width*2);
  assert((restored&0xFFF)==200+height*2);
 }
 assert(complete_half_scale_glyph(0xF2000000u)==0xF2000000u);
 Settings settings;settings.family_override="nintendo-switch";
 ControlsEditor editor(settings,ControllerFamily::Xbox);
 auto view=compose_controls_view(editor,ControllerFamily::Keyboard);
 std::set<std::string> labels;
 for(const auto& t:view.texts) {
  assert(t.font==0 && t.scale_milli==500);
  assert(t.x>=0 && t.x<320 && t.y>=0 && t.y<240);labels.insert(t.text);
 }
 for(const auto& r:view.rectangles)assert(r.x>=0 && r.y>=0 && r.x+r.w<=320 && r.y+r.h<=240);
 for(auto label:{"DROP SELECT","SLIDE BACK","RESTART PUZZLE","MAGIC","ZOOM IN","ZOOM OUT","DEFAULTS","DISCARD","APPLY"})assert(labels.contains(label));
 assert(!labels.contains("C LEFT") && !labels.contains("C UP") && !labels.contains("C DOWN"));
 // Switch South is B, East is A. Keyboard navigation must not change the edited device.
 bool confirm_b=false,back_a=false;
 for(const auto& t:view.texts) {
  if(t.x>=240 && t.y==57 && t.text=="B")confirm_b=true;
  if(t.x>=240 && t.y==82 && t.text=="A")back_a=true;
 }
 assert(confirm_b && back_a);
 editor.select();view=compose_controls_view(editor,ControllerFamily::Xbox);
 bool waiting=false;for(const auto& t:view.texts)waiting|=t.text=="PRESS BUTTON";
 assert(waiting);
 // Reference labels match their physical device; controller symbols are drawn as geometry.
 for(auto family:{ControllerFamily::Keyboard,ControllerFamily::Xbox,ControllerFamily::PlayStation,ControllerFamily::NintendoSwitch}) {
  Settings manual;manual.family_override=controller_family_name(family);
  ControlsEditor device(manual,family);auto scene=compose_controls_view(device,family);
  std::set<std::string> text;for(const auto& t:scene.texts)text.insert(t.text);
  assert(text.contains(family==ControllerFamily::Keyboard ? "ARROWS":"D-PAD"));
  assert(text.contains(family==ControllerFamily::Keyboard ? "ENTER":family==ControllerFamily::PlayStation ? "OPTIONS":family==ControllerFamily::NintendoSwitch ? "PLUS":"MENU"));
  for(const auto& t:scene.texts)assert(t.max_width==0 || t.x+t.max_width<=320);
 }
 PCSettingsMenu graphics(Settings{},true);
 const auto graphics_scene=compose_graphics_view(graphics,ControllerFamily::Keyboard);
 std::set<std::string> graphics_labels;
 for(const auto& t:graphics_scene.texts) {
  assert(t.font==0 && t.scale_milli==500);
  graphics_labels.insert(t.text);
 }
 for(auto label:{"RESOLUTION","ASPECT","REFRESH","WINDOW","MSAA","PRESENT FILTER","TEXTURE FILTER"})assert(graphics_labels.contains(label));
 assert(!graphics_labels.contains("MORE"));
 Settings automatic;ControlsEditor p2(automatic,ControllerFamily::Keyboard);
 p2.set_device(ControllerFamily::NintendoSwitch);
 assert(p2.navigation_family()==ControllerFamily::NintendoSwitch);
 p2.set_device(ControllerFamily::Keyboard);
 assert(p2.navigation_family()==ControllerFamily::Keyboard);
 assert(p2.display_family()==ControllerFamily::NintendoSwitch);
}
