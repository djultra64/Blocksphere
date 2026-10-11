#include "tetrisphere/recomp_support.h"
#include "tetrisphere/controls_view.h"
#include "tetrisphere/controls_bridge.h"
#include "tetrisphere/pc_settings_hook.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#ifdef TETRISPHERE_LINUX_DEMO
#include "controls_sprite_data.h"
extern "C" void func_800335F0(std::uint8_t*,recomp_context*);
extern "C" void func_8003396C(std::uint8_t*,recomp_context*);
extern "C" void func_80038550(std::uint8_t*,recomp_context*);
extern "C" bool tetrisphere_rt64_ui_tag_record(std::uint8_t*,std::uint32_t,std::uint32_t,std::uint32_t);
namespace {
constexpr auto pointer=0xFFFFFFFF800F22B4ull;
thread_local std::uint32_t pool_base=0,pool_end=0;
[[noreturn]] void invalid(const char* reason) {
 std::fprintf(stderr,"Controls view render error: %s\n",reason);std::exit(70);
}
void reserve(std::uint8_t* rdram,std::size_t bytes) {
 const auto cursor=static_cast<std::uint32_t>(MEM_W(0,pointer));
 if(cursor<pool_base || cursor>pool_end || bytes>pool_end-cursor || (cursor&7))invalid("active display list pool capacity");
}
void command(std::uint8_t* rdram,std::uint32_t w0,std::uint32_t w1) {
 reserve(rdram,8);
 const auto address=static_cast<std::uint32_t>(MEM_W(0,pointer));
 if(address<0x80000000u || address>=0x807FFFF8u || (address&7))invalid("display list bounds");
 MEM_W(0,static_cast<std::int32_t>(address))=w0;
 MEM_W(4,static_cast<std::int32_t>(address))=w1;
 MEM_W(0,pointer)=address+8;
}
void draw_text(std::uint8_t* rdram,const recomp_context& parent,const tetrisphere::ControlsText& text) {
 // Private call frame below the original 0x70-byte frame. Never change parent's registers.
 recomp_context call=parent;call.r29=ADD32(parent.r29,-0x200);
 if(text.text.size()>63)invalid("text length");
 const auto string=ADD32(call.r29,0x40);
 for(std::size_t n=0;n<=text.text.size();++n)MEM_B(n,string)=n==text.text.size()?0:text.text[n];
 call.r4=static_cast<std::int32_t>(pointer);call.r5=text.font;call.r6=text.x*4;call.r7=text.y*4;
 MEM_W(0x10,call.r29)=string;
 MEM_W(0x14,call.r29)=(text.color>>16)&255;
 MEM_W(0x18,call.r29)=(text.color>>8)&255;
 MEM_W(0x1C,call.r29)=text.color&255;MEM_W(0x20,call.r29)=255;
 // The original scaled text renderer reads a reciprocal scale (1024 is unity; positions use quarter pixels).
 // Measure using the same private ROM font metrics at the fixed pixel scale.
 const auto font_base=0xFFFFFFFF80102D48ull+text.font*136;
 int width=0;
 for(unsigned char c:text.text)width+=MEM_H(2*c-0x3C,font_base);
 if(width<=0)invalid("font metrics");
 double scale=text.scale_milli/1000.0;
 // Use the same scale for every string; fitting is a layout concern.
 if(text.max_width>0 && width*scale>text.max_width+1)invalid("text exceeds layout width");
 const auto scale_address=0xFFFFFFFF800DFEACull;
 const auto original_scale=MEM_H(0,scale_address);
 const int reciprocal=std::ceil(1024.0/scale);
 if(reciprocal>32767 || reciprocal<1024)invalid("font scale");
 MEM_H(0,scale_address)=reciprocal;
 reserve(rdram,text.text.size()*512);
 const auto begin=static_cast<std::uint32_t>(MEM_W(0,pointer));
 func_80038550(rdram,&call);
 const auto end=static_cast<std::uint32_t>(MEM_W(0,pointer));
 // Complete only these glyph rectangles; the guest scaled blitter trims
 // a pixel from the right and 1.5 pixels from the bottom at half scale.
 for(auto address=begin;address<end;address+=8) {
  const auto word=static_cast<std::uint32_t>(MEM_W(0,static_cast<std::int32_t>(address)));
  if((word>>24)==0xE4) {
   if(text.scale_milli!=500 || ((word>>12)&0xFFF)>1276 || (word&0xFFF)>954)
    invalid("glyph rectangle bounds");
   MEM_W(0,static_cast<std::int32_t>(address))=tetrisphere::complete_half_scale_glyph(word);
  }
 }
 MEM_H(0,scale_address)=original_scale;
}
}
#endif
extern "C" bool tetrisphere_controls_view_hook(std::uint8_t* rdram,recomp_context* ctx) {
#ifdef TETRISPHERE_LINUX_DEMO
 tetrisphere_pc_settings_hook(rdram,ctx,0);
 auto editor=tetrisphere::controls_editor_snapshot();
 auto graphics=tetrisphere::pc_settings_snapshot();
 if(!editor && !graphics)return false;
 if(MEM_H(0,0xFFFFFFFF800E121Cull)!=3 || MEM_H(0,0xFFFFFFFF800E1220ull)<0)
  invalid("unexpected guest menu state");
 // Original menu allocator uses two equal command pools. Keep caller's footer free.
 const auto a=static_cast<std::uint32_t>(MEM_W(0,0xFFFFFFFF800FC5A8ull));
 const auto b=static_cast<std::uint32_t>(MEM_W(0,0xFFFFFFFF800FDEC0ull));
 const auto descriptor=static_cast<std::uint32_t>(MEM_W(0,0xFFFFFFFF801028C8ull));
 if(descriptor<0x80000000u || descriptor>0x807FE714u || a<0x80000000u || b<=a || b>0x80800000u)
  invalid("guest menu pool descriptor");
 pool_base=static_cast<std::uint32_t>(MEM_W(0x18E8,static_cast<std::int32_t>(descriptor)));
 const auto size=b-a;
 if((pool_base!=a && pool_base!=b) || size!=0x18000u || std::uint64_t(b)+size>0x80800000ull)
  invalid("guest menu pool allocation");
 std::size_t footer=16;
 const auto diagnostic=static_cast<std::uint32_t>(MEM_W(0,0xFFFFFFFF800E22D0ull));
 if(diagnostic) {
  if(diagnostic<0x80000000u || diagnostic>0x807FFF80u)invalid("diagnostic text bounds");
  std::size_t n=0;while(n<128 && MEM_BU(n,static_cast<std::int32_t>(diagnostic)))++n;
  if(n==128)invalid("diagnostic text length");
  footer+=96+688*n+32;
 }
 if(footer>=size)invalid("caller footer budget");
 pool_end=pool_base+size-footer;
 const auto first=static_cast<std::uint32_t>(MEM_W(0,pointer));
 reserve(rdram,96);
 recomp_context bitmap=*ctx;bitmap.r29=ADD32(ctx->r29,-0x200);bitmap.r4=static_cast<std::int32_t>(pointer);
 func_800335F0(rdram,&bitmap);
 if(editor) {
 // Copy the same immutable authored bitmap into unused extended RDRAM.
 // The training font variants start at 80810000; this sprite ends before that.
 constexpr std::uint32_t sprite=0x80800000u;
 static_assert(controls_sprite_data.size()<0x10000);
 for(std::size_t n=0;n<controls_sprite_data.size();++n)MEM_B(n,static_cast<std::int32_t>(sprite))=controls_sprite_data[n];
 bitmap=*ctx;bitmap.r29=ADD32(ctx->r29,-0x200);bitmap.r4=static_cast<std::int32_t>(pointer);
 bitmap.r5=static_cast<std::int32_t>(sprite);bitmap.r6=29;bitmap.r7=50;
 for(int n=0;n<4;++n)MEM_W(0x10+4*n,bitmap.r29)=255;
 command(rdram,0xBA000C02,0); // Point sample the pixel art.
 reserve(rdram,0x8000); // Whole-image blitter tile loop, bounded 112x84 RGBA16 input.
 func_8003396C(rdram,&bitmap);
 }
 const auto scene=editor ? tetrisphere::compose_controls_view(*editor,editor->navigation_family())
  : tetrisphere::compose_graphics_view(*graphics,tetrisphere::pc_settings_navigation_family());
 command(rdram,0xE7000000,0);command(rdram,0xBA001402,0x00300000);
 command(rdram,0xB900031D,0);command(rdram,0xED000000,(320u<<14)|(240u<<2));
 unsigned previous=0xFFFFFFFF;
 for(const auto& rect:scene.rectangles) {
  if(rect.color!=previous) {
   const unsigned rgb=((rect.color>>19)&31)<<11 | ((rect.color>>11)&31)<<6 | ((rect.color>>3)&31)<<1 | 1;
   command(rdram,0xE7000000,0);command(rdram,0xF7000000,rgb|(rgb<<16));previous=rect.color;
  }
  command(rdram,0xF6000000u | ((rect.x+rect.w-1)<<14) | ((rect.y+rect.h-1)<<2),
          (rect.x<<14)|(rect.y<<2));
 }
 command(rdram,0xE7000000,0);
 recomp_context setup=*ctx;setup.r29=ADD32(ctx->r29,-0x200);setup.r4=static_cast<std::int32_t>(pointer);
 reserve(rdram,96);
 func_800335F0(rdram,&setup); // Original transparent UI mode and blitter cache reset.
 command(rdram,0xBA000C02,0); // Point sampling applies only to PC menu glyphs.
 for(const auto& text:scene.texts)draw_text(rdram,*ctx,text);
 const auto past=static_cast<std::uint32_t>(MEM_W(0,pointer));
 if(past<first || past>pool_end)invalid("display list budget");
 tetrisphere_rt64_ui_tag_record(rdram,first,past,3);
 return true;
#else
 (void)rdram;(void)ctx;return false;
#endif
}
