#include "../renderer/native11/texture_bgra.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* message){if(!yes){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
TextureView view(TextureFormat f,TextureAlpha a,std::uint32_t w,std::uint32_t h,
                 TextureMip& mip,const std::vector<std::uint8_t>& bytes){
    const bool raw=f==TextureFormat::bgra8||f==TextureFormat::rgba8||f==TextureFormat::bgra8_srgb||f==TextureFormat::rgba8_srgb;
    const unsigned block=(f==TextureFormat::bc1||f==TextureFormat::bc1_srgb)?8:16;
    const std::uint32_t row=raw?w*4:((w+3)/4)*block;
    const std::uint32_t size=row*(raw?h:(h+3)/4);
    mip={w,h,row,size,0,size};return {f,a,w,h,&mip,1,bytes.data(),bytes.size()};
}
void pixel(const std::vector<std::uint8_t>& out,unsigned w,unsigned x,unsigned y,
           std::array<std::uint8_t,4> expected,const char* why){
    const auto pos=(y*w+x)*4;need(pos+4<=out.size(),why);
    for(unsigned i=0;i<4;++i)need(out[pos+i]==expected[i],why);
}
void rejected(TextureView input,std::vector<std::uint8_t>& out,std::string& error,const char* why){
    out={19,23,29};need(!texture_base_to_bgra(input,false,out,error),why);
    need(out==std::vector<std::uint8_t>({19,23,29})&&!error.empty(),"failure atomicity and diagnostic");
}
}
int main(){
    TextureMip mip{};std::vector<std::uint8_t> out;std::string error;
    std::vector<std::uint8_t> raw={255,0,255,13, 1,2,3,4};
    auto input=view(TextureFormat::rgba8_srgb,TextureAlpha::straight,2,1,mip,raw);
    need(texture_base_to_bgra(input,true,out,error),"RGBA sRGB decode");
    pixel(out,2,0,0,{255,0,255,0},"raw magenta key");pixel(out,2,1,0,{3,2,1,4},"raw swizzle");
    input.format=TextureFormat::bgra8;input.alpha=TextureAlpha::opaque;
    need(texture_base_to_bgra(input,false,out,error),"BGRA opaque decode");
    pixel(out,2,0,0,{255,0,255,255},"opaque alpha forced");
    input.alpha=TextureAlpha::premultiplied;rejected(input,out,error,"premultiplied refused");
    input.alpha=TextureAlpha::custom;rejected(input,out,error,"custom refused");

    // BC1: red and blue 565 endpoints; each of the four palette indices.
    std::vector<std::uint8_t> bc1(8);bc1[0]=0x00;bc1[1]=0xf8;bc1[2]=0x1f;bc1[3]=0x00;bc1[4]=0xe4;
    input=view(TextureFormat::bc1,TextureAlpha::unknown,4,4,mip,bc1);
    need(texture_base_to_bgra(input,false,out,error),"BC1 four-colour decode");
    pixel(out,4,0,0,{0,0,255,255},"BC1 index 0");pixel(out,4,1,0,{255,0,0,255},"BC1 index 1");
    pixel(out,4,2,0,{85,0,170,255},"BC1 index 2");pixel(out,4,3,0,{170,0,85,255},"BC1 index 3");
    bc1[0]=0x1f;bc1[1]=0;bc1[2]=0;bc1[3]=0xf8;
    need(texture_base_to_bgra(input,false,out,error),"BC1 three-colour decode");
    pixel(out,4,2,0,{127,0,127,255},"BC1 half interpolation");pixel(out,4,3,0,{0,0,0,0},"BC1 transparent index");
    bc1[0]=0xe0;bc1[1]=0x07;bc1[2]=0;bc1[3]=0;bc1[4]=0;
    need(texture_base_to_bgra(input,false,out,error),"BC1 six-bit green endpoint");
    pixel(out,4,0,0,{0,255,0,255},"BC1 green bit replication");
    bc1[0]=0x1f;bc1[1]=0xf8;bc1[2]=0;bc1[3]=0;bc1[4]=0;
    need(texture_base_to_bgra(input,true,out,error),"BC1 keyed decode");
    pixel(out,4,0,0,{255,0,255,0},"BC1 magenta key");

    // BC2 colour endpoints are in reverse order yet always use four colours.
    std::vector<std::uint8_t> bc2(16);bc2[0]=0xf0;bc2[8]=0x1f;bc2[9]=0;bc2[10]=0;bc2[11]=0xf8;bc2[12]=0xe4;
    input=view(TextureFormat::bc2_srgb,TextureAlpha::straight,4,4,mip,bc2);
    need(texture_base_to_bgra(input,false,out,error),"BC2 decode");
    pixel(out,4,0,0,{255,0,0,0},"BC2 low alpha nibble");pixel(out,4,1,0,{0,0,255,255},"BC2 high alpha nibble");
    pixel(out,4,3,0,{85,0,170,0},"BC2 forced four-colour palette");

    std::vector<std::uint8_t> bc3(16);bc3[0]=255;bc3[1]=0;bc3[8]=0;bc3[9]=0xf8;bc3[10]=0x1f;bc3[11]=0;
    // Indices 0..15 use all eight values; the 3-bit index for pixel 2 crosses a byte.
    std::uint64_t bits=0;for(unsigned i=0;i<16;++i)bits|=std::uint64_t(i&7)<<(3*i);
    for(unsigned i=0;i<6;++i)bc3[2+i]=static_cast<std::uint8_t>(bits>>(8*i));
    input=view(TextureFormat::bc3,TextureAlpha::straight,4,4,mip,bc3);
    need(texture_base_to_bgra(input,false,out,error),"BC3 eight-alpha mode");
    pixel(out,4,0,0,{0,0,255,255},"BC3 alpha 0");pixel(out,4,1,0,{0,0,255,0},"BC3 alpha 1");
    pixel(out,4,2,0,{0,0,255,218},"BC3 cross-byte alpha index 2");
    bc3[0]=0;bc3[1]=255;
    need(texture_base_to_bgra(input,false,out,error),"BC3 six-alpha mode");
    pixel(out,4,2,0,{0,0,255,51},"BC3 six-mode interpolation");
    pixel(out,4,2,1,{0,0,255,0},"BC3 six-mode alpha index 6");
    pixel(out,4,3,1,{0,0,255,255},"BC3 six-mode alpha index 7");
    bc3[8]=0x1f;bc3[9]=0;bc3[10]=0;bc3[11]=0xf8;bc3[12]=3;
    need(texture_base_to_bgra(input,false,out,error),"BC3 reversed colour endpoints");
    pixel(out,4,0,0,{85,0,170,0},"BC3 colour always four-mode");

    // A 5x1 surface has a second block with only one valid column.
    std::vector<std::uint8_t> edge(16);edge[0]=0;edge[1]=0xf8;edge[8]=0x1f;edge[9]=0;
    input=view(TextureFormat::bc1,TextureAlpha::unknown,5,1,mip,edge);
    need(texture_base_to_bgra(input,false,out,error)&&out.size()==20,"BC edge crop");
    pixel(out,5,4,0,{255,0,0,255},"BC edge pixel");

    input=view(TextureFormat::bc4,TextureAlpha::unknown,4,4,mip,bc1);
    rejected(input,out,error,"unsupported BC4");
    input=view(TextureFormat::bc1,TextureAlpha::unknown,5,1,mip,edge);
    TextureMip chain[2]={mip,{2,1,8,8,17,8}};
    input.mips=chain;input.mip_count=2;input.byte_count=24;
    rejected(input,out,error,"malformed later mip rejected");
    input=view(TextureFormat::bgra8,TextureAlpha::unknown,16384,16384,mip,raw);
    rejected(input,out,error,"large invalid layout rejected");
    // Valid 8192x4097 layout exceeds the decoded cap without requiring payload allocation.
    TextureMip huge{8192,4097,32768,134250496,0,134250496};
    std::uint8_t one=0;input={TextureFormat::bgra8,TextureAlpha::unknown,8192,4097,&huge,1,&one,huge.byte_count};
    rejected(input,out,error,"decoded cap rejected");
    std::puts("PASS native11 BGRA decode: raw, BC1/2/3, alpha, key, edge, full validation and atomic failures");
}
