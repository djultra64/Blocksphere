#include "tetrisphere/pc_settings_hook.h"
#include "tetrisphere/controls_bridge.h"
#include <deque>
#include <memory>
#include <mutex>
#include <atomic>
namespace {
tetrisphere::Settings saved;
std::unique_ptr<tetrisphere::PCSettingsMenu> menu;
std::mutex pending_mutex;
std::deque<tetrisphere::PCSettingsRequest> pending;
std::atomic<bool> active{false},blocked{false};
std::atomic<tetrisphere::ControllerFamily> navigation_family{tetrisphere::ControllerFamily::Keyboard};
tetrisphere::ControllerFamily family() {
#if defined(TETRISPHERE_LINUX_DEMO) || defined(TETRISPHERE_PC_SETTINGS_TEST)
 return tetrisphere::active_controller_family();
#else
 return tetrisphere::ControllerFamily::Keyboard;
#endif
}
bool options(std::uint8_t* rdram) {
 return MEM_H(0,0xFFFFFFFF800E121Cull)==3 && MEM_H(0,0xFFFFFFFF800E1220ull)>=0;
}
bool options_label_site(std::uint32_t site) {
 switch(site) {
 case 0x8004E7D0u:case 0x8004E8E4u:case 0x8004EAFCu:case 0x8004EC18u:
 case 0x8004E8BCu:case 0x8004E9CCu:case 0x8004EABCu:case 0x8004EBF0u:case 0x8004ED08u:case 0x8004EE00u:
  return true;
 default:return false;
 }
}
void after_navigation(std::uint8_t* rdram) {
 if(menu && menu->closed()) {menu.reset();active.store(false);MEM_H(0,0xFFFFFFFF800E1220ull)=4;MEM_H(0,0xFFFFFFFF800E075Cull)=4;}
 else if(!menu) MEM_H(0,0xFFFFFFFF800E1220ull)=0;
 if(menu) {
  if(menu->take_controls_request())tetrisphere::open_controls_editor(saved,family());
  if(auto change=menu->take_change()) {
   pending.push_back({*change,menu->changed_field()});
  }
 }
 MEM_W(0,0xFFFFFFFF800E12FCull)=5;
}
gpr text(std::uint8_t* rdram,recomp_context* ctx,const std::string& label) {
 // Proven unused 0x48..0x52 of this frame. Leave 0x53 and 0x54 untouched.
 if(label.size()>10) std::abort();
 const auto address=ADD32(ctx->r29,0x48);
 for(unsigned i=0;i<=label.size();++i) MEM_B(i,address)=i==label.size()?0:label[i];
 return address;
}
}
namespace tetrisphere {
std::optional<PCSettingsMenu> pc_settings_snapshot() {std::lock_guard lock(pending_mutex);return menu ? std::optional{*menu}:std::nullopt;}
bool pc_settings_active() {return active.load();}
bool pc_settings_blocks_game() {return blocked.load();}
void pc_settings_device(ControllerFamily device) {navigation_family.store(device);}
ControllerFamily pc_settings_navigation_family() {
 std::lock_guard lock(pending_mutex);
 const auto device=navigation_family.load();
 return device==ControllerFamily::Keyboard || saved.family_override=="auto"
  ? device:select_controller_family("",saved.family_override.c_str());
}
void pc_settings_release_guard(bool held) {std::lock_guard lock(pending_mutex);if(!active.load() && !held)blocked.store(false);}
void pc_settings_command(ControlsCommand command) {
 std::lock_guard lock(pending_mutex);if(!menu)return;
 switch(command) {
 case ControlsCommand::Previous:menu->move(-1);break;
 case ControlsCommand::Next:menu->move(1);break;
 case ControlsCommand::Left:menu->adjust(-1);break;
 case ControlsCommand::Right:menu->adjust(1);break;
 case ControlsCommand::Activate:menu->select(menu->focused());break;
 case ControlsCommand::Back:menu->back();break;
 case ControlsCommand::Disconnect:break;
 }
 if(auto change=menu->take_change())pending.push_back({*change,menu->changed_field()});
}
void initialize_pc_settings(const Settings& settings) {std::lock_guard lock(pending_mutex);saved=settings;menu.reset();pending.clear();active.store(false);blocked.store(false);}
std::optional<PCSettingsRequest> take_pc_settings_change() {
 std::lock_guard lock(pending_mutex);if(pending.empty()) return {};
 auto result=pending.front();pending.pop_front();return result;
}
void acknowledge_controls_settings(const Settings& settings) {
 std::lock_guard lock(pending_mutex);saved=settings;if(menu)menu->update_settings(saved);
}
}
extern "C" void tetrisphere_pc_settings_hook(std::uint8_t* rdram,recomp_context* ctx,std::uint32_t site) {
 std::lock_guard lock(pending_mutex);
 if(menu && menu->closed())after_navigation(rdram);
 if(site==0)return;
 if(MEM_H(0,0xFFFFFFFF800E121Cull)!=3) {menu.reset();active.store(false);return;}
 const auto count=5;
 if(site==0x8004CC0Cu) {ctx->r14=count;return;}
 if(site==0x8004F38Cu) {ctx->r10=count;return;}
 if(site==0x8004F224u) {ctx->r24=count;return;}
 // During entry the guest selection is still -1. Its animated Options rows
 // must already use the final labels/count; input remains gated as before.
 const bool animated_label=options_label_site(site) && MEM_W(0,0xFFFFFFFF800E1218ull)>=0;
 if(!options(rdram) && !animated_label) {menu.reset();active.store(false);return;}
 switch(site) {
 case 0x8004F408u:
  if(menu && MEM_W(0,0xFFFFFFFF800E1218ull)<0 && (MEM_W(0,0xFFFFFFFF800DFD98ull)&0x4000)) {
   MEM_W(0,0xFFFFFFFF800DFD98ull)&=~0x4000;menu->back();after_navigation(rdram);
  }return;
 case 0x8004F680u:
  if(!menu && MEM_H(0,0xFFFFFFFF800E1220ull)==3) {
   tetrisphere::open_controls_editor(saved,family());ctx->r16=6;return;
  }
  if(menu || MEM_H(0,0xFFFFFFFF800E1220ull)==4) {
   if(!menu) {menu=std::make_unique<tetrisphere::PCSettingsMenu>(saved,true);navigation_family.store(family());active.store(true);blocked.store(true);}
   else menu->select(MEM_H(0,0xFFFFFFFF800E1220ull));
   after_navigation(rdram);ctx->r16=6; // Skip all original submenu dispatches.
  }return;
 }
 const auto row=MEM_W(0x6C,ctx->r29);
 std::string label;
 if(menu) {
  auto rows=menu->rows(family());
  if(row<0 || static_cast<std::size_t>(row)>=rows.size()) return;
  label=rows[row];
 } else if(row==4) label="GRAPHICS";
 else return;
 switch(site) {
 case 0x8004E7D0u:case 0x8004E8E4u:case 0x8004EAFCu:case 0x8004EC18u:
  ctx->r4=text(rdram,ctx,label);return;
 case 0x8004E8BCu:case 0x8004E9CCu:case 0x8004EABCu:case 0x8004EBF0u:case 0x8004ED08u:case 0x8004EE00u:
  ctx->r7=text(rdram,ctx,label);return;
 }
}
