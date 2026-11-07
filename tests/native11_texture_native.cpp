#include "../renderer/native11/renderer.hpp"
#include "../renderer/native11/assets.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace a2fo::native11;
static_assert(sizeof(A2FO_N11_TextureMip)==32&&offsetof(A2FO_N11_TextureMip,offset)==16);
static_assert(sizeof(A2FO_N11_Texture)==40&&offsetof(A2FO_N11_Texture,payload_bytes)==24);
static_assert(offsetof(A2FO_N11_Texture,payload)==32&&offsetof(A2FO_N11_Texture,mips)==36);
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void ok(HRESULT hr,const char* why){if(FAILED(hr)){std::fprintf(stderr,"FAIL %s HRESULT=%08lx\n",why,static_cast<unsigned long>(hr));std::exit(1);}}
void close(int actual,int expected,const char* why){if(std::abs(actual-expected)>3){std::fprintf(stderr,"FAIL %s actual=%d expected=%d\n",why,actual,expected);std::exit(1);}}
void put(std::vector<std::uint8_t>& b,std::size_t offset,std::uint32_t value){for(unsigned i=0;i<4;++i)b.at(offset+i)=(value>>(i*8))&255;}
std::vector<std::uint8_t> dds(std::uint32_t dxgi,std::uint32_t alpha,unsigned w,unsigned h,unsigned count,const std::vector<std::uint8_t>& payload){
    std::vector<std::uint8_t> b(148+payload.size());
    std::memcpy(b.data(),"DDS ",4);put(b,4,124);put(b,8,0x1007|(count>1?0x20000:0));
    put(b,12,h);put(b,16,w);put(b,28,count);put(b,76,32);put(b,80,4);
    std::memcpy(b.data()+84,"DX10",4);put(b,108,0x1000|(count>1?0x400008:0));
    put(b,128,dxgi);put(b,132,3);put(b,140,1);put(b,144,alpha);
    std::copy(payload.begin(),payload.end(),b.begin()+148);return b;
}
std::vector<std::uint8_t> bc1(unsigned colour){
    return {std::uint8_t(colour&255),std::uint8_t(colour>>8),0,0,0,0,0,0};
}
std::vector<std::uint8_t> channel(unsigned endpoint){return {std::uint8_t(endpoint),0,0,0,0,0,0,0};}
void append(std::vector<std::uint8_t>& a,const std::vector<std::uint8_t>& b){a.insert(a.end(),b.begin(),b.end());}
std::vector<std::uint8_t> bc7(unsigned red){
    // BC7 mode 6: 7-bit endpoint components, one p-bit per endpoint,
    // 4-bit indices (the first anchor index is 3 bits). Equal even endpoints.
    std::vector<std::uint8_t> b(16);unsigned bit=0;
    auto bits=[&](unsigned value,unsigned count){for(unsigned i=0;i<count;++i,++bit)if(value&(1u<<i))b[bit/8]|=1u<<(bit%8);};
    bits(64,7);for(unsigned value:{red/2,red/2,0u,0u,0u,0u,127u,127u})bits(value,7);
    bits(0,2);bits(0,3);for(unsigned i=1;i<16;++i)bits(0,4);
    need(bit==128,"BC7 mode6 block extent");return b;
}
struct Upload {Renderer* renderer;std::uint32_t handle=0,mips=0,format=0;std::uint64_t bytes=0;HRESULT hr=E_FAIL;unsigned visits=0;};
int upload(void* context,const A2FO_N11_Texture* image){
    auto& out=*static_cast<Upload*>(context);++out.visits;
    try{
        if(!image||!image->mips||image->mip_count>15||image->reserved)return 0;
        std::array<TextureMip,15> levels{};
        for(unsigned i=0;i<image->mip_count;++i){const auto& m=image->mips[i];levels[i]={m.width,m.height,m.row_pitch,m.slice_pitch,m.offset,m.byte_count};}
        TextureView view{static_cast<TextureFormat>(image->format),static_cast<TextureAlpha>(image->alpha_mode),image->width,image->height,
                         levels.data(),image->mip_count,image->payload,image->payload_bytes};
        out.hr=out.renderer->upload_texture(view,out.handle);out.mips=image->mip_count;out.format=image->format;out.bytes=image->payload_bytes;
        return SUCCEEDED(out.hr)?1:0;
    }catch(...){out.hr=E_FAIL;return 0;}
}
struct Window {HWND h=nullptr;~Window(){if(h)DestroyWindow(h);}};
struct Pixel {int r,g,b,a;};
Pixel center(Renderer& r){std::vector<std::uint8_t> pixels;std::uint32_t w=0,h=0;ok(r.readback(pixels,w,h),"texture readback");
    need(w==32&&h==32,"texture target extent");const auto* p=pixels.data()+(16*w+16)*4;return {p[2],p[1],p[0],p[3]};}
std::uint32_t mesh(Renderer& r,float uv_scale){
    Vertex vertices[3]={{{-1,-1,.2f},{0,0,1},{0,0}},{{3,-1,.2f},{0,0,1},{2*uv_scale,0}},{{-1,3,.2f},{0,0,1},{0,2*uv_scale}}};
    std::uint32_t indices[3]{0,1,2},handle=0;ok(r.upload_mesh({vertices,3,indices,3},handle),"texture sampling triangle");return handle;
}
void colour(Pixel value,int r,int g,int b,const char* name){close(value.r,r,name);close(value.g,g,name);close(value.b,b,name);}
void run(){
    Window window;window.h=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,"STATIC","private native11 texture fixture",WS_POPUP,
        0,0,32,32,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    need(window.h&&!IsWindowVisible(window.h),"private hidden texture HWND");
    Renderer renderer;ok(renderer.initialize(window.h,32,32),"native texture device");
    Draw draw{};draw.mesh=mesh(renderer,0);FrameView frame{};frame.draws=&draw;frame.draw_count=1;
    frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;frame.clear_colour[0]=frame.clear_colour[1]=frame.clear_colour[2]=0;
    auto sample=[&](){ok(renderer.render(frame,false),"sample native texture");return center(renderer);};
    std::vector<std::uint8_t> payload=bc1(0xf800);append(payload,bc1(0x07e0));append(payload,bc1(0x001f));
    auto file=dds(71,0,4,4,3,payload);Upload loaded{&renderer};
    need(a2fo_native11_parse_texture(file.data(),file.size(),upload,&loaded)==1,"cross-language authored BC1 chain");
    need(loaded.visits==1&&loaded.mips==3&&loaded.bytes==24&&loaded.format==5,"encoded chain preserved");
    draw.texture=loaded.handle;std::fill(file.begin(),file.end(),0); // Immutable GPU resource owns its source bytes.
    colour(sample(),255,0,0,"BC1 authored base level");
    draw.mesh=mesh(renderer,16);colour(sample(),0,255,0,"BC1 authored middle mip selected by derivatives");
    draw.mesh=mesh(renderer,1024);colour(sample(),0,0,255,"BC1 authored final mip selected");
    draw.mesh=mesh(renderer,0);
    struct FormatCase {std::uint32_t dxgi;std::vector<std::uint8_t> block;int r,g,b;};
    auto bc2=std::vector<std::uint8_t>(8,255);append(bc2,bc1(0xf800));
    auto bc3=channel(255);append(bc3,bc1(0x07e0));
    auto bc5=channel(255);append(bc5,channel(128));auto signed_bc5=channel(64);append(signed_bc5,channel(127));
    const std::array<FormatCase,12> cases{{
        {72,bc1(0xffff),255,255,255},{74,bc2,255,0,0},{75,bc2,255,0,0},
        {77,bc3,0,255,0},{78,bc3,0,255,0},{80,channel(128),128,0,0},
        {81,channel(64),129,0,0},{83,bc5,255,128,0},{84,signed_bc5,129,255,0},
        {98,bc7(254),254,0,0},{99,bc7(128),55,0,0},{71,bc1(0x001f),0,0,255}
    }};
    for(const auto& test:cases){
        file=dds(test.dxgi,0,4,4,1,test.block);Upload image{&renderer};
        need(a2fo_native11_parse_texture(file.data(),file.size(),upload,&image)==1,"DDS format upload");
        draw.texture=image.handle;colour(sample(),test.r,test.g,test.b,"native BC format channel/sRGB sample");
    }
    // BGRA/RGBA and colour-space tags are interpreted by the GPU view.
    for(unsigned format:{28u,29u,87u,91u}){
        std::vector<std::uint8_t> pixels(64);
        for(unsigned i=0;i<16;++i){pixels[i*4+(format==87||format==91?2:0)]=128;pixels[i*4+3]=255;}
        file=dds(format,0,4,4,1,pixels);Upload image{&renderer};
        need(a2fo_native11_parse_texture(file.data(),file.size(),upload,&image)==1,"raw DDS upload");
        draw.texture=image.handle;colour(sample(),format==29||format==91?55:128,0,0,"raw format channel/sRGB sample");
    }
    std::vector<std::uint8_t> associated(64);for(unsigned i=0;i<16;++i){associated[i*4]=128;associated[i*4+3]=128;}
    file=dds(28,2,4,4,1,associated);Upload premult{&renderer};
    need(a2fo_native11_parse_texture(file.data(),file.size(),upload,&premult)==1,"premultiplied DDS upload");
    draw.texture=premult.handle;draw.flags=two_sided|alpha_blend;colour(sample(),128,0,0,"premultiplied DDS avoids double alpha");
    draw.flags=two_sided|additive;colour(sample(),128,0,0,"additive keeps premultiplied coverage");
    std::vector<std::uint8_t> opaque(64);for(unsigned i=0;i<16;++i)opaque[i*4]=255;
    file=dds(28,3,4,4,1,opaque);Upload forced{&renderer};
    need(a2fo_native11_parse_texture(file.data(),file.size(),upload,&forced)==1,"opaque metadata upload");
    draw.texture=forced.handle;draw.flags=two_sided|alpha_blend;colour(sample(),255,0,0,"opaque metadata ignores stored zero alpha");
    file=dds(28,4,4,4,1,opaque);Upload custom{&renderer};
    need(a2fo_native11_parse_texture(file.data(),file.size(),upload,&custom)==0&&custom.visits==1&&custom.hr==E_NOTIMPL,"custom alpha explicitly unsupported");
    file=dds(28,0,4,4,1,opaque);Upload next{&renderer};
    need(a2fo_native11_parse_texture(file.data(),file.size(),upload,&next)==1&&next.handle==forced.handle+1,"failed upload does not consume image identity");
    TextureMip malformed{4,4,16,64,1,64};TextureView bad{TextureFormat::rgba8,TextureAlpha::unknown,4,4,&malformed,1,opaque.data(),64};
    std::uint32_t retained=123456;need(renderer.upload_texture(bad,retained)==E_INVALIDARG&&retained==123456,"bad mip metadata preserves handle");
    ok(renderer.resize(48,48),"texture resize");ok(renderer.resize(32,32),"texture resize back");
    draw.texture=loaded.handle;draw.flags=two_sided;colour(sample(),255,0,0,"compressed texture survives resize");
    ResourceUsage before{};ok(renderer.resource_usage(before),"logical resident resource usage");
    const auto released_texture=draw.texture;
    ok(renderer.release_texture(released_texture),"release compressed image");
    ResourceUsage after{};ok(renderer.resource_usage(after),"usage after image release");
    need(after.textures+1==before.textures&&after.texture_bytes+24==before.texture_bytes&&after.epoch==before.epoch,"released encoded payload leaves resident budget");
    need(renderer.release_texture(released_texture)==E_INVALIDARG&&renderer.release_texture(0)==E_INVALIDARG,"released identity and white-image guard");
    need(renderer.render(frame,false)==E_INVALIDARG,"released texture draw rejected");
    colour(center(renderer),255,0,0,"released texture rejection did not clear frame");
    file=dds(71,0,4,4,1,bc1(0x07e0));Upload replacement{&renderer};
    need(a2fo_native11_parse_texture(file.data(),file.size(),upload,&replacement)==1&&replacement.handle>next.handle,"image identities do not recycle released handles");
    draw.texture=replacement.handle;colour(sample(),0,255,0,"replacement compressed image");
    const auto released_mesh=draw.mesh;ok(renderer.release_mesh(released_mesh),"release native mesh");
    need(renderer.render(frame,false)==E_INVALIDARG&&renderer.release_mesh(released_mesh)==E_INVALIDARG,"released mesh rejected");
    colour(center(renderer),0,255,0,"released mesh rejection did not clear frame");
    draw.mesh=mesh(renderer,0);need(draw.mesh>released_mesh,"mesh identities do not recycle released handles");
    colour(sample(),0,255,0,"replacement mesh");
    const auto old_epoch=after.epoch;renderer.shutdown();
    ResourceUsage unavailable{};unavailable.epoch=123;
    need(renderer.resource_usage(unavailable)==E_UNEXPECTED&&unavailable.epoch==123,"unavailable usage preserves output");
    ok(renderer.initialize(window.h,32,32),"reinitialize texture device");
    ok(renderer.resource_usage(after),"new device usage");
    need(after.epoch==old_epoch+1&&after.meshes==0&&after.textures==1&&after.texture_bytes==4,"new device epoch and white-only residency");
    need(renderer.render(frame,false)==E_INVALIDARG,"retired device resources cannot alias after recreation");
    need(!GetModuleHandleA("d3d8.dll")&&!GetModuleHandleA("d3d9.dll")&&!GetModuleHandleA("D3DX81ab.dll")&&!GetModuleHandleA("d3dx9_43.dll"),"no legacy graphics modules");
    std::puts("PASS native11 DDS ABI + immutable BC1/2/3/4/5/7, signed and sRGB views, authored mip sampling, alpha metadata, release/epoch/resize and atomic upload rejection");
}
}
int main(){run();}
