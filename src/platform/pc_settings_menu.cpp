#include "tetrisphere/pc_settings_menu.h"
#include <algorithm>
#include <cctype>
namespace tetrisphere {
Settings merge_pc_settings_change(const Settings& persisted,const Settings& requested,int field) {
 auto result=persisted;
 switch(field) {
 case 0:result.graphics.resolution=requested.graphics.resolution;break;
 case 1:result.graphics.aspect=requested.graphics.aspect;break;
 case 2:result.graphics.refresh=requested.graphics.refresh;break;
 case 3:result.graphics.window_mode=requested.graphics.window_mode;break;
 case 4:result.graphics.msaa_samples=requested.graphics.msaa_samples;break;
 case 5:result.graphics.presentation_filter=requested.graphics.presentation_filter;break;
 case 6:result.graphics.three_point_filter=requested.graphics.three_point_filter;break;
 case 13:result.family_override=requested.family_override;result.bindings=requested.bindings;result.magic=requested.magic;break;
 }
 return result;
}

std::vector<std::string> PCSettingsMenu::choices(ControllerFamily) const {
 switch(field_) {
 case 0:return {"AUTO","240P","720P","1080P","1440P","2160P"};
 case 1:return {"4:3","16:9"};
 case 2:return {"ORIGINAL","30 FPS","60 FPS","120 FPS"};
 case 3:return {"WINDOWED","FULLSCRN"};
 case 4:return {"OFF","2X","4X","8X"};
 case 5:return {"NEAREST","LINEAR","PIXEL"};
 case 6:return {"LINEAR","3-POINT"};
 default:return {};
 }
}
bool PCSettingsMenu::restart_required(int field) const {return field==0 || field==1 || field==2 || field==4;}
std::vector<std::string> PCSettingsMenu::rows(ControllerFamily) const {
 if(page_==Page::Root)return {"GRAPHICS","CONTROLS","BACK"};
 if(page_==Page::Closed)return {};
 return {"RESOLUTION","ASPECT","REFRESH","WINDOW","MSAA","PRESENT FILTER","TEXTURE FILTER","BACK"};
}
std::string PCSettingsMenu::value(int field) const {
 switch(field) {
 case 0:{constexpr const char* values[]{"AUTO","240P","720P","1080P","1440P","2160P"};return values[int(settings_.graphics.resolution)];}
 case 1:return settings_.graphics.aspect==GraphicsAspect::Original4x3 ? "4:3":"16:9";
 case 2:{constexpr const char* values[]{"ORIGINAL","30 FPS","60 FPS","120 FPS"};return values[int(settings_.graphics.refresh)];}
 case 3:return settings_.graphics.window_mode==GraphicsWindowMode::Windowed ? "WINDOWED":"FULLSCREEN";
 case 4:return settings_.graphics.msaa_samples ? std::to_string(settings_.graphics.msaa_samples)+"X":"OFF";
 case 5:{constexpr const char* values[]{"NEAREST","LINEAR","PIXEL"};return values[int(settings_.graphics.presentation_filter)];}
 case 6:return settings_.graphics.three_point_filter ? "3-POINT":"LINEAR";
 default:return {};
 }
}
void PCSettingsMenu::move(int direction) {
 const int count=rows(ControllerFamily::Keyboard).size();
 if(count && !saving_)focused_=(focused_+(direction<0 ? -1:1)+count)%count;
}
void PCSettingsMenu::adjust(int direction) {
 if(page_!=Page::Graphics || saving_ || focused_>=7 || direction==0)return;
 field_=focused_;
 const int count=choices(ControllerFamily::Keyboard).size();
 int index=0;
 switch(field_) {
 case 0:index=int(settings_.graphics.resolution);break;
 case 1:index=int(settings_.graphics.aspect);break;
 case 2:index=int(settings_.graphics.refresh);break;
 case 3:index=int(settings_.graphics.window_mode);break;
 case 4:index=settings_.graphics.msaa_samples==8 ? 3:settings_.graphics.msaa_samples==4 ? 2:settings_.graphics.msaa_samples==2 ? 1:0;break;
 case 5:index=int(settings_.graphics.presentation_filter);break;
 case 6:index=settings_.graphics.three_point_filter ? 1:0;break;
 }
 index=(index+(direction<0 ? -1:1)+count)%count;
 switch(field_) {
 case 0:settings_.graphics.resolution=static_cast<GraphicsResolution>(index);break;
 case 1:settings_.graphics.aspect=static_cast<GraphicsAspect>(index);break;
 case 2:settings_.graphics.refresh=static_cast<GraphicsRefresh>(index);break;
 case 3:settings_.graphics.window_mode=static_cast<GraphicsWindowMode>(index);break;
 case 4:{constexpr int samples[]{0,2,4,8};settings_.graphics.msaa_samples=samples[index];break;}
 case 5:settings_.graphics.presentation_filter=static_cast<PresentationFilter>(index);break;
 case 6:settings_.graphics.three_point_filter=index==1;break;
 }
 change_=settings_;saving_=true;status_="SAVING";
}
void PCSettingsMenu::select(int row) {
 const auto shown=rows(ControllerFamily::Keyboard);
 if(row<0 || std::size_t(row)>=shown.size() || saving_)return;
 focused_=row;
 if(std::size_t(row)==shown.size()-1) {back();return;}
 if(page_==Page::Root) {
  if(row==1)controls_request_=true;
  else {page_=Page::Graphics;focused_=0;}
 } else adjust(1);
}
void PCSettingsMenu::update_settings(const Settings& settings) {
 if(saving_) {
  PCSettingsMenu actual(settings,true);
  status_=value(field_)==actual.value(field_) ? "SAVED":"SAVE FAILED TRY AGAIN";
 }
 settings_=settings;saving_=false;
}
void PCSettingsMenu::back() {
 if(saving_)return;
 if(page_==Page::Graphics && !graphics_only_) {page_=Page::Root;focused_=0;}
 else page_=Page::Closed;
}
std::optional<Settings> PCSettingsMenu::take_change() {
 auto result=std::move(change_);change_.reset();return result;
}
}
