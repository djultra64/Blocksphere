#include "tetrisphere/menu_medal_wrap.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
constexpr std::size_t dl=0x200, vertices=0x20000, scratch=0x8C0000;
constexpr std::size_t quad_bytes=40;
void put(std::vector<std::uint8_t>& m,std::size_t p,std::uint32_t v) {
 std::memcpy(m.data()+p,&v,4);
}
std::uint32_t word(const std::vector<std::uint8_t>& m,std::size_t p) {
 std::uint32_t v;std::memcpy(&v,m.data()+p,4);return v;
}
void half(std::vector<std::uint8_t>& m,std::size_t p,std::int16_t v) {
 std::memcpy(m.data()+(p^2),&v,2);
}
std::int16_t half(const std::vector<std::uint8_t>& m,std::size_t p) {
 std::int16_t v;std::memcpy(&v,m.data()+(p^2),2);return v;
}
std::vector<std::uint8_t> fixture() {
 std::vector<std::uint8_t> m(0x900000,0xA5);
 put(m,dl,0xB900031D);put(m,dl+4,0x00504240);
 put(m,dl+8,0xFC121824);put(m,dl+12,0xFF33FFFF);
 for(unsigned q=0;q<14;++q) {
  const auto p=dl+16+q*quad_bytes;
  put(m,p,0x04300040);put(m,p+4,0x80000000+vertices+q*64);
  put(m,p+8,0xFD10002B);put(m,p+12,0x80195DC0);
  put(m,p+16,0xE7000000);put(m,p+20,0);
  put(m,p+24,0xBF000000);put(m,p+28,0x000A1400);
  put(m,p+32,0xBF000000);put(m,p+36,0x000A141E);
  const auto x=static_cast<std::int16_t>(q==0?2879:q==1?-3200:-2000+q*200);
  for(unsigned v=0;v<4;++v) {
   const auto a=vertices+q*64+v*16;
   half(m,a,x+(v%2)*703);half(m,a+2,100+(v/2)*703);half(m,a+4,-4000);
   half(m,a+8,(v%2)*2752);half(m,a+10,(1-v/2)*2752);
   m[(a+15)^3]=v==1?48:0;
  }
 }
 const auto end=dl+16+14*quad_bytes;
 put(m,end,0xFA000000);put(m,end+4,0x11223344);
 put(m,end+8,0xB8000000);put(m,end+12,0);
 return m;
}
}
int main() {
 auto m=fixture();const auto original=m;
 {
  tetrisphere::MenuMedalWrapPatch patch(m.data(),m.size(),0x80000200,0x1000,
                                       16.0f/9.0f,0x808C0000,0x40000);
  assert(patch.duplicate_count()==2);
  assert(word(m,dl+16)==0x06010000);
  assert(word(m,dl+20)==0x808C0000);
  assert(word(m,dl+16+14*quad_bytes)==0xFA000000);
  // Execute the rewritten span's G_VTX commands: the originals are retained,
  // the left continuation is x=2879-6080, and the right copy is -3200+6080.
  unsigned loads=0,duplicates=0;bool left=false,right=false;
  for(std::size_t p=scratch;p<scratch+0x10000;p+=8) {
   const auto cmd=word(m,p),arg=word(m,p+4);
   if(cmd==0x06010000) {
    assert(arg==0x80000000+dl+16+14*quad_bytes);break;
   }
   if(cmd!=0x04300040)continue;
   ++loads;const auto a=arg&0xFFFFFF;
   if(a<scratch)continue;
   ++duplicates;const auto x=half(m,a);
   left|=x==-3201;right|=x==2880;
   assert(half(m,a+16)==x+703 && half(m,a+32)==x);
   // Every source byte other than X is preserved, including UV, Y, Z and alpha.
   const auto source=vertices+(x==-3201?0:64);
   for(unsigned v=0;v<4;++v)for(unsigned b=2;b<16;++b)
    assert(m[(a+v*16+b)^3]==original[(source+v*16+b)^3]);
  }
  assert(loads==16 && duplicates==2 && left && right);
 }
 assert(m==original);
 for(float aspect:{4.0f/3.0f,0.0f,-1.0f,std::nanf("")}) {
  tetrisphere::MenuMedalWrapPatch patch(m.data(),m.size(),0x80000200,0x1000,
                                       aspect,0x808C0000,0x40000);
  assert(patch.duplicate_count()==0 && m==original);
 }
 // Follow the same medal across the guest recycle. Its visible left edge
 // advances by one world unit rather than jumping across the screen, then
 // its clipped width decreases to zero at the actual widescreen edge.
 const float half_width=4500.0f*std::tan(3.14159265358979323846f/8)*16/9;
 float previous=1000;
 for(int phase:{0,6079,5600,5300,5264,5263}) {
  m=fixture();const auto x=phase-3200;
  for(unsigned v=0;v<4;++v)half(m,vertices+v*16,x+(v%2)*703);
  const auto before=m;
  {
   tetrisphere::MenuMedalWrapPatch p(m.data(),m.size(),0x80000200,0x1000,
                                     16.0f/9.0f,0x808C0000,0x40000);
   float visible=std::max(0.0f,std::min(float(x+703),half_width)-std::max(float(x),-half_width));
   for(std::size_t at=scratch;at<scratch+0x10000;at+=8) {
    const auto cmd=word(m,at),arg=word(m,at+4);
    if(cmd==0x06010000)break;
    if(cmd!=0x04300040 || (arg&0xFFFFFF)<scratch)continue;
    const auto a=arg&0xFFFFFF;
    if(half(m,a)!=x-6080)continue;
    const auto left=half(m,a);
    visible=std::max(0.0f,std::min(float(left+703),half_width)-std::max(float(left),-half_width));
   }
   // Before recycle, the original is the left copy. Afterwards its periodic
   // continuation replaces it; exclude the original right-hand re-entry.
   if(phase>0 && phase<=5263)visible=0;
   assert(visible<=previous+0.01f);
   if(phase==0 || phase==6079)assert(std::abs(visible-703.0f)<0.01f);
   if(phase==5264)assert(visible>0 && visible<1);
   if(phase==5263)assert(visible==0);
   previous=visible;
  }
  assert(m==before);
 }
 m=fixture();
 // Incomplete and unrelated geometry must be rejected before any write.
 put(m,dl+16+13*quad_bytes,0x00000000);const auto incomplete=m;
 {tetrisphere::MenuMedalWrapPatch p(m.data(),m.size(),0x80000200,0x1000,
                                    16.0f/9.0f,0x808C0000,0x40000);
  assert(p.duplicate_count()==0 && m==incomplete);}
 m=fixture();half(m,vertices+4,-3999);const auto wrong=m;
 {tetrisphere::MenuMedalWrapPatch p(m.data(),m.size(),0x80000200,0x1000,
                                    16.0f/9.0f,0x808C0000,0x40000);
  assert(p.duplicate_count()==0 && m==wrong);}
 m=fixture();
 for(unsigned q=0;q<14;++q)put(m,dl+16+q*quad_bytes+12,0x80123456);
 const auto unrelated=m;
 {tetrisphere::MenuMedalWrapPatch p(m.data(),m.size(),0x80000200,0x1000,
                                    16.0f/9.0f,0x808C0000,0x40000);
  assert(p.duplicate_count()==0 && m==unrelated);}
 m=fixture();const auto clean=m;
 for(auto bad_scratch:{0x80000200u,0x808FFF00u}) {
  tetrisphere::MenuMedalWrapPatch p(m.data(),m.size(),0x80000200,0x1000,
                                   16.0f/9.0f,bad_scratch,0x40000);
  assert(p.duplicate_count()==0 && m==clean);
 }
 {tetrisphere::MenuMedalWrapPatch p(m.data(),m.size(),0x80000200,0x1000,
                                    16.0f/9.0f,0x808C0000,8);
  assert(p.duplicate_count()==0 && m==clean);}
 std::cout<<"menu medal wrap tests passed\n";
}
