#include "tetrisphere/pc_settings_hook.h"
#include <cassert>
#include "tetrisphere/controls_bridge.h"
#include <vector>
namespace tetrisphere { ControllerFamily active_controller_family() { return ControllerFamily::NintendoSwitch; } }
int main() {
 using namespace tetrisphere;
 Settings saved; saved.bindings.confirm=PhysicalButton::East;
 initialize_pc_settings(saved);
 std::vector<std::uint8_t> memory(0x800000,0xCD);auto* rdram=memory.data();recomp_context ctx{};
 ctx.r29=static_cast<std::int32_t>(0x807FF000u);
 MEM_H(0,0xFFFFFFFF800E121Cull)=3; MEM_H(0,0xFFFFFFFF800E1220ull)=0;
 MEM_W(0,0xFFFFFFFF800E1218ull)=-1;
 // Options entry animates before its submenu selection becomes nonnegative.
 MEM_H(0,0xFFFFFFFF800E1220ull)=-1;MEM_W(0,0xFFFFFFFF800E1218ull)=15;
 ctx.r24=4;ctx.r14=91;tetrisphere_pc_settings_hook(rdram,&ctx,0x8004F224u);
 assert(ctx.r24==5 && ctx.r14==91);
 // Graphics must have its final text throughout every incoming measure/draw path.
 MEM_W(0x6C,ctx.r29)=4;
 for(auto site:{0x8004E7D0u,0x8004E8E4u,0x8004EAFCu,0x8004EC18u}) {
  ctx.r4=0x800EC9D8;tetrisphere_pc_settings_hook(rdram,&ctx,site);
  for(unsigned i=0;i<sizeof("GRAPHICS");++i)assert(MEM_BU(i,ctx.r4)=="GRAPHICS"[i]);
 }
 for(auto site:{0x8004E8BCu,0x8004E9CCu,0x8004EABCu,0x8004EBF0u,0x8004ED08u,0x8004EE00u}) {
  ctx.r7=0x800EC9D8;tetrisphere_pc_settings_hook(rdram,&ctx,site);
  for(unsigned i=0;i<sizeof("GRAPHICS");++i)assert(MEM_BU(i,ctx.r7)=="GRAPHICS"[i]);
 }
 // Other main selections keep their original incoming row count.
 MEM_H(0,0xFFFFFFFF800E121Cull)=4;ctx.r24=3;
 tetrisphere_pc_settings_hook(rdram,&ctx,0x8004F224u);assert(ctx.r24==3);
 MEM_H(0,0xFFFFFFFF800E121Cull)=3;MEM_H(0,0xFFFFFFFF800E1220ull)=0;
 MEM_W(0,0xFFFFFFFF800E1218ull)=-1;
 ctx.r14=4;tetrisphere_pc_settings_hook(rdram,&ctx,0x8004CC0Cu);assert(ctx.r14==5);
 ctx.r10=4;ctx.r14=91;tetrisphere_pc_settings_hook(rdram,&ctx,0x8004F38Cu);assert(ctx.r10==5 && ctx.r14==91);
 for (int row=0;row<3;++row) {
  MEM_H(0,0xFFFFFFFF800E1220ull)=row;ctx.r16=3;
  tetrisphere_pc_settings_hook(rdram,&ctx,0x8004F680u);assert(ctx.r16==3);
 }
 MEM_H(0,0xFFFFFFFF800E1220ull)=4;MEM_W(0x6C,ctx.r29)=4;ctx.r4=0x800EC9D8;
 tetrisphere_pc_settings_hook(rdram,&ctx,0x8004E7D0u);
 const char label[]="GRAPHICS";
 for(unsigned i=0;i<sizeof label;++i)assert(MEM_BU(i,ctx.r4)==label[i]);
 assert(MEM_BU(0x47,ctx.r29)==0xCD && MEM_BU(0x53,ctx.r29)==0xCD && MEM_BU(0x54,ctx.r29)==0xCD);
 ctx.r16=3;tetrisphere_pc_settings_hook(rdram,&ctx,0x8004F680u);assert(ctx.r16==6);
 assert(MEM_H(0,0xFFFFFFFF800E1220ull)==4);
 ctx.r14=4;tetrisphere_pc_settings_hook(rdram,&ctx,0x8004CC0Cu);assert(ctx.r14==5);
 assert(pc_settings_active() && pc_settings_snapshot()->rows(ControllerFamily::NintendoSwitch).size()==8);
 assert(pc_settings_navigation_family()==ControllerFamily::NintendoSwitch);
 pc_settings_device(ControllerFamily::PlayStation);
 assert(pc_settings_navigation_family()==ControllerFamily::PlayStation);
 pc_settings_device(ControllerFamily::Keyboard);
 assert(pc_settings_navigation_family()==ControllerFamily::Keyboard);
 auto manual=saved;manual.family_override="nintendo-switch";
 acknowledge_controls_settings(manual);pc_settings_device(ControllerFamily::Xbox);
 assert(pc_settings_navigation_family()==ControllerFamily::NintendoSwitch);
 pc_settings_device(ControllerFamily::Keyboard);
 assert(pc_settings_navigation_family()==ControllerFamily::Keyboard);
 acknowledge_controls_settings(saved);
 MEM_H(0,0xFFFFFFFF800E075Cull)=8; // The original input loop remembers PC selections.
 // Cancel from Bindings returns one level without cancelling original Options.
 pc_settings_command(ControlsCommand::Back);tetrisphere_pc_settings_hook(rdram,&ctx,0);
 assert(!pc_settings_active());
 pc_settings_release_guard(false);assert(!pc_settings_blocks_game());
 assert(MEM_H(0,0xFFFFFFFF800E1220ull)==4);
 assert(MEM_H(0,0xFFFFFFFF800E075Cull)==4);
 ctx.r14=4;tetrisphere_pc_settings_hook(rdram,&ctx,0x8004CC0Cu);assert(ctx.r14==5);
 // Existing Options/Controls enters the dedicated view directly.
 MEM_H(0,0xFFFFFFFF800E1220ull)=3;ctx.r16=3;
 tetrisphere_pc_settings_hook(rdram,&ctx,0x8004F680u);
 assert(ctx.r16==6 && controls_editor_active());
 assert(controls_editor_snapshot()->pending().bindings.confirm==PhysicalButton::East);
 controls_editor_command(ControlsCommand::Back);
 assert(!controls_editor_active());
 // A main menu cancellation must remain untouched.
 MEM_H(0,0xFFFFFFFF800E1220ull)=-1;MEM_W(0,0xFFFFFFFF800DFD98ull)=0x4000;
 tetrisphere_pc_settings_hook(rdram,&ctx,0x8004F408u);assert(MEM_W(0,0xFFFFFFFF800DFD98ull)==0x4000);
}
