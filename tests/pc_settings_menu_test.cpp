#include "tetrisphere/pc_settings_menu.h"
#include <cassert>
#include <set>
using namespace tetrisphere;
int main() {
 Settings original;original.bindings.confirm=PhysicalButton::East;
 original.magic=MagicControl::RightShoulder;original.graphics.msaa_samples=4;
 PCSettingsMenu expanded(original,true);
 const auto rows=expanded.rows(ControllerFamily::Keyboard);
 assert(rows.size()==8);
 for(const auto& row:rows)assert(row!="MORE" && row!="DISPLAY" && row!="QUALITY");
 // Every field is editable without entering another page. Values cycle fully
 // in both directions; unrelated remaps and settings survive each save.
 constexpr int counts[]{6,2,4,2,4,3,2};
 for(int field=0;field<7;++field) {
  PCSettingsMenu menu(original,true);
  for(int row=0;row<field;++row)menu.move(1);
  const auto initial=menu.value(field);
  std::set<std::string> seen;
  Settings persisted=original;
  for(int choice=0;choice<counts[field];++choice) {
   menu.adjust(1);
   assert(menu.changed_field()==field && menu.saving());
   auto request=menu.take_change();assert(request && !menu.take_change());
   const auto requested=menu.value(field);
   menu.adjust(1);assert(menu.value(field)==requested); // In-flight save is serialized.
   persisted=merge_pc_settings_change(persisted,*request,field);
   assert(persisted.bindings.confirm==PhysicalButton::East && persisted.magic==MagicControl::RightShoulder);
   menu.update_settings(persisted);assert(!menu.saving());
   seen.insert(menu.value(field));assert(menu.rows(ControllerFamily::Xbox)==rows);
  }
  assert(seen.size()==counts[field] && menu.value(field)==initial);
  menu.adjust(-1);auto change=menu.take_change();assert(change);
  menu.update_settings(original);assert(menu.value(field)==initial && menu.status()=="SAVE FAILED TRY AGAIN");
 }
 PCSettingsMenu navigation(original,true);navigation.move(-1);assert(navigation.focused()==7);
 navigation.select(7);assert(navigation.closed() && !navigation.take_change());
 PCSettingsMenu cancel(original,true);cancel.back();assert(cancel.closed() && !cancel.take_change());
 PCSettingsMenu controls(original);controls.select(1);
 assert(controls.take_controls_request() && !controls.take_controls_request());
 Settings persisted,stale;persisted.graphics.resolution=GraphicsResolution::P2160;
 stale.bindings.confirm=PhysicalButton::West;
 auto remap=merge_pc_settings_change(persisted,stale,13);
 assert(remap.graphics.resolution==GraphicsResolution::P2160 && remap.bindings.confirm==PhysicalButton::West);
 stale.graphics.three_point_filter=false;
 auto graphics=merge_pc_settings_change(remap,stale,6);
 assert(graphics.graphics.resolution==GraphicsResolution::P2160 && graphics.bindings.confirm==PhysicalButton::West && !graphics.graphics.three_point_filter);
}
