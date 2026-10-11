#include "tetrisphere/controls_view.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
namespace tetrisphere {
namespace {
constexpr unsigned white=0xFFFFFF,ivory=0xF4F0DE,orange=0xE6611E,yellow=0xF4DA32,cyan=0x49C8D6,teal=0x4E6263;
struct Point {int x,y;};
void rect(ControlsScene& s,int x,int y,int w,int h,unsigned color) {
 const int right=std::min(320,x+w),bottom=std::min(240,y+h);
 x=std::max(0,x);y=std::max(0,y);
 if(right>x && bottom>y)s.rectangles.push_back({x,y,right-x,bottom-y,color});
}
void line(ControlsScene& s,Point a,Point b,unsigned color,int width=1) {
 const int steps=std::max(std::abs(b.x-a.x),std::abs(b.y-a.y));
 for(int n=0;n<=steps;++n) {
  const double f=steps ? double(n)/steps:0;
  rect(s,std::lround(a.x+(b.x-a.x)*f),std::lround(a.y+(b.y-a.y)*f),width,width,color);
 }
}
void polygon(ControlsScene& s,std::initializer_list<Point> points,unsigned color) {
 const std::vector<Point> p(points);int top=240,bottom=0;
 for(auto v:p) {top=std::min(top,v.y);bottom=std::max(bottom,v.y);}
 for(int y=top;y<=bottom;++y) {
  std::vector<int> crossings;
  for(std::size_t i=0;i<p.size();++i) {
   auto a=p[i],b=p[(i+1)%p.size()];
   if((a.y<=y && b.y>y) || (b.y<=y && a.y>y))crossings.push_back(a.x+(y-a.y)*(b.x-a.x)/(b.y-a.y));
  }
  std::sort(crossings.begin(),crossings.end());
  for(std::size_t i=1;i<crossings.size();i+=2)rect(s,crossings[i-1],y,crossings[i]-crossings[i-1]+1,1,color);
 }
}
void circle(ControlsScene& s,int x,int y,int radius,unsigned color) {
 for(int dy=-radius;dy<=radius;++dy) {
  int dx=std::sqrt(radius*radius-dy*dy);rect(s,x-dx,y+dy,2*dx+1,1,color);
 }
}
std::string upper(std::string value) {
 for(auto& c:value)c=std::toupper(static_cast<unsigned char>(c));return value;
}
void text(ControlsScene& s,int x,int y,std::string value,unsigned color=ivory,int font=0,int width=0,int scale=650) {
 // All PC menu text uses the original main-menu font at a fixed half size.
 // Layout owns available space; never squeeze a long label to fit.
 (void)font;(void)scale;
 s.texts.push_back({x,y,0,color,std::move(value),width,500});
}
void brackets(ControlsScene& s,int left,int top,int right,int bottom) {
 for(int x:{left,right-3})for(int y:{top,bottom-1})rect(s,x,y,4,2,yellow);
 for(int x:{left,right-1})for(int y:{top,bottom-3})rect(s,x,y,2,4,yellow);
}
void page_frame(ControlsScene& s,const char* title) {
 const std::array<Point,8> frame{{{26,22},{294,22},{300,28},{300,208},
 {294,214},{26,214},{20,208},{20,28}}};
 for(std::size_t n=0;n<frame.size();++n) {
  auto a=frame[n],b=frame[(n+1)%frame.size()];
  line(s,{a.x+1,a.y+1},{b.x+1,b.y+1},teal);line(s,a,b,ivory);
 }
 line(s,{28,44},{292,44},teal);line(s,{28,195},{292,195},teal);
 text(s,31,30,title,cyan,0,100);
}
void footer_action(ControlsScene& s,int x,int width,const char* label,bool selected) {
 text(s,x,200,label,selected ? white:ivory,0,width);
 if(selected)brackets(s,x-4,197,x+width+4,211);
}
}
ControlsScene compose_controls_view(const ControlsEditor& e,ControllerFamily navigation_family) {
 ControlsScene s;
 page_frame(s,"CONTROLS");
 // Every device uses the same authored pixel bitmap; labels remain family-aware.
 const auto family=e.display_family();
 text(s,155,30,"LAYOUT",e.focused()==0 ? white:ivory,2,42);
 std::string layout=e.pending().family_override=="auto" ? "AUTO ":"";
 layout+=family==ControllerFamily::Keyboard ? "KEYS":family==ControllerFamily::Xbox ? "XBOX":
   family==ControllerFamily::PlayStation ? "PS":"SWITCH";
 text(s,209,30,layout,e.focused()==0 ? white:yellow,2,72);
 if(e.focused()==0) {
  brackets(s,205,27,282,41);
  polygon(s,{{200,30},{195,34},{200,38}},yellow);polygon(s,{{286,30},{291,34},{286,38}},yellow);
 }
 const auto values=e.labels(family);
 const std::array<int,4> controls{{4+int(e.pending().bindings.confirm),4+int(e.pending().bindings.cancel),
 4+int(e.pending().bindings.face_up),int(e.pending().magic)}};
 constexpr const char* actions[]{"DROP SELECT","SLIDE BACK","RESTART PUZZLE","MAGIC"};
 for(int n=0;n<4;++n) {
  const int y=57+n*25;const bool selected=e.focused()==n+1;
  text(s,149,y,actions[n],selected ? white:ivory,0,87);
  text(s,240,y,upper(values[n]),selected ? white:yellow,0,54);
  if(selected) {
   brackets(s,236,y-3,296,y+11);
   polygon(s,{{139,y},{144,y+4},{139,y+8}},yellow);
  }
 }
 if(e.focused()>=1 && e.focused()<=ControlsEditor::Magic) {
  const int control=controls[e.focused()-1];
  const std::array<Point,8> points{{{58,55},{110,55},{58,62},{110,62},{112,89},{119,82},{104,82},{112,74}}};
  const auto start=points[control];
  for(int x=start.x;x<140;x+=5)rect(s,x,47,1,1,teal);
  for(int y=47;y<57+(e.focused()-1)*25;y+=5)rect(s,140,y,1,1,teal);
  line(s,start,{start.x,47},teal);
  circle(s,start.x,start.y,3,yellow);
 }
 text(s,32,148,"ZOOM IN",ivory,2,43,580);
 text(s,78,148,upper(magic_control_label(family,MagicControl::LeftShoulder)),yellow,2,65,580);
 text(s,32,160,"ZOOM OUT",ivory,2,43,580);
 text(s,78,160,upper(magic_control_label(family,MagicControl::RightShoulder)),yellow,2,65,580);
 text(s,32,172,"MOVE",ivory,2,43,580);text(s,78,172,family==ControllerFamily::Keyboard ? "ARROWS":"D-PAD",yellow,2,65,580);
 text(s,32,184,"PAUSE",ivory,2,43,580);
 if(family==ControllerFamily::NintendoSwitch) {rect(s,78,187,7,1,yellow);rect(s,81,184,1,7,yellow);text(s,89,184,"PLUS",yellow,0,64,500);}
 else text(s,78,184,family==ControllerFamily::Keyboard ? "ENTER":family==ControllerFamily::PlayStation ? "OPTIONS":"MENU",yellow,2,65,580);
 footer_action(s,35,64,"DEFAULTS",e.focused()==ControlsEditor::Defaults);
 footer_action(s,139,52,"DISCARD",e.focused()==ControlsEditor::Discard);
 footer_action(s,249,36,"APPLY",e.focused()==ControlsEditor::Apply);
 std::string status=e.saving() ? "SAVING":e.capturing() ? "PRESS BUTTON":e.status();
 if(status.empty()) {
  if(e.focused()>=1 && e.focused()<=ControlsEditor::Magic && std::count(controls.begin(),controls.end(),controls[e.focused()-1])>1)
   status="SHARED BUTTON";
  else if(e.focused()==ControlsEditor::RestartPuzzle && !e.dirty())status="PUZZLE ONLY";
  else status=e.dirty() ? "CHANGES NOT APPLIED":"SELECT ACTION";
 }
 const auto status_color=status.find("FAILED")!=std::string::npos || status.find("MUST DIFFER")!=std::string::npos
  ? orange:e.dirty() ? yellow:cyan;
 // Status wraps at word boundaries instead of scaling individual strings.
 if(status.size()>19) {
  auto split=status.rfind(' ',19);
  if(split!=std::string::npos) {text(s,149,174,status.substr(0,split),status_color,0,150);status=status.substr(split+1);}
 }
 text(s,149,184,status,status_color,0,150);
 const auto& applied=e.applied().bindings;
 text(s,31,223,upper(logical_action_label(navigation_family,applied,LogicalAction::Confirm))+" SELECT",yellow,0,160,500);
 text(s,206,223,upper(logical_action_label(navigation_family,applied,LogicalAction::Cancel))+" BACK",yellow,0,92,500);
 return s;
}
ControlsScene compose_graphics_view(const PCSettingsMenu& menu,ControllerFamily family) {
 ControlsScene s;
 page_frame(s,"GRAPHICS");
 const auto rows=menu.rows(family);
 for(int row=0;row<7;++row) {
  const int y=51+row*19;
  const bool selected=menu.focused()==row;
  text(s,35,y,rows[row],selected ? white:ivory,0,136);
  text(s,188,y,menu.value(row),selected ? white:yellow,0,102);
  if(selected) {
   brackets(s,183,y-3,297,y+11);
   polygon(s,{{27,y},{32,y+4},{27,y+8}},yellow);
   polygon(s,{{179,y},{174,y+4},{179,y+8}},yellow);
   polygon(s,{{290,y},{295,y+4},{290,y+8}},yellow);
  }
 }
 footer_action(s,249,36,"BACK",menu.focused()==7);
 std::string status=menu.status();
 if(status.empty())status=menu.restart_required(menu.focused()) ? "APPLIES AFTER RESTART":"APPLIES NOW";
 const auto status_color=status.find("FAILED")!=std::string::npos ? orange
  :status=="APPLIES AFTER RESTART" ? yellow:cyan;
 text(s,31,183,status,status_color,0,250);
 text(s,31,222,"LEFT RIGHT CHANGE",yellow,0,138);
 text(s,206,222,upper(logical_action_label(family,menu.settings().bindings,LogicalAction::Cancel))+" BACK",yellow,0,92);
 return s;
}

}
