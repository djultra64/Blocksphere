// Compile the authored PNG bitmap into an embedded N64 RGBA16 sprite.
// Nearest sampling keeps the requested pixel edges; this never draws a controller.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#include <stb_image.h>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>

int main(int argc,char** argv) {
 if(argc!=3)return 2;
 int width=0,height=0,channels=0;
 auto* pixels=stbi_load(argv[1],&width,&height,&channels,4);
 if(!pixels || width<112 || height<84 || channels!=4) {
  std::cerr<<"Controls sprite must be a transparent RGBA PNG\n";return 1;
 }
 constexpr int w=112,h=84;
 std::array<std::uint8_t,8+w*h*2> image{};
 image[1]=w;image[3]=h; // Original image header: BE width, height, RGBA16 format 0.
 for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
  const int sx=(2*x+1)*width/(2*w),sy=(2*y+1)*height/(2*h);
  const auto* p=pixels+4*(sy*width+sx);
  const unsigned rgba=(p[0]>>3)<<11 | (p[1]>>3)<<6 | (p[2]>>3)<<1 | (p[3]>=128);
  image[8+2*(y*w+x)]=rgba>>8;image[9+2*(y*w+x)]=rgba;
 }
 stbi_image_free(pixels);
 std::ofstream out(argv[2]);
 out<<"#pragma once\n#include <array>\n#include <cstdint>\n"
       "inline constexpr std::array<std::uint8_t,"<<image.size()<<"> controls_sprite_data={\n";
 for(std::size_t n=0;n<image.size();++n) {
  out<<unsigned(image[n])<<',';if(n%32==31)out<<'\n';
 }
 out<<"};\n";return out ? 0:1;
}
