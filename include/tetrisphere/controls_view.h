#pragma once
#include "tetrisphere/controls_editor.h"
#include "tetrisphere/pc_settings_menu.h"
#include <vector>
#include <cstdint>
namespace tetrisphere {
struct ControlsRect {int x,y,w,h;unsigned color;};
struct ControlsText {int x,y,font;unsigned color;std::string text;int max_width=0;int scale_milli=1000;};
struct ControlsScene {std::vector<ControlsRect> rectangles;std::vector<ControlsText> texts;};
ControlsScene compose_graphics_view(const PCSettingsMenu&,ControllerFamily navigation_family);
ControlsScene compose_controls_view(const ControlsEditor&,ControllerFamily navigation_family);
// 80036FE4 trims the scaled glyph's exclusive edges. At reciprocal 2048,
// restore W/2 and H/2 without changing the ROM image or texture coordinates.
constexpr std::uint32_t complete_half_scale_glyph(std::uint32_t word) {
 if((word>>24)!=0xE4)return word;
 const auto right=((word>>12)&0xFFF)+4,bottom=(word&0xFFF)+6;
 return (word&0xFF000000u)|(right<<12)|bottom;
}
}
