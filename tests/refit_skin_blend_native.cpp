#include "../modules/A2FORefitMenus/setup_skin.cpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

using namespace a2fo::refit_menu;
using namespace a2fo::refit_menu::setup_skin;

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Dib {
    HDC dc{};HBITMAP bitmap{};HGDIOBJ previous{};BYTE* bits{};
    int width{},height{};
    Dib(int w,int h,int depth=32,bool top_down=true):width(w),height(h){
        dc=CreateCompatibleDC(nullptr);require(dc,"CreateCompatibleDC");
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=top_down?-h:h;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=depth;
        info.bmiHeader.biCompression=BI_RGB;
        void* data{};bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&data,nullptr,0);
        require(bitmap&&data,"CreateDIBSection");bits=static_cast<BYTE*>(data);
        previous=SelectObject(dc,bitmap);require(previous&&previous!=HGDI_ERROR,"SelectObject");
    }
    ~Dib(){if(dc&&previous)SelectObject(dc,previous);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
    Dib(const Dib&)=delete;Dib& operator=(const Dib&)=delete;
};
std::uint32_t seed=0x5a219bc7;
unsigned random_byte(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed&255;}
void compare(int width,int height,unsigned state,unsigned background,int iteration){
    Dib reference(width+17,height+11),candidate(width+17,height+11),scalar(width+17,height+11);
    std::vector<BYTE> atlas_pixels(128*256*4);
    for(std::size_t i=0;i<atlas_pixels.size();i+=4){
        const unsigned alpha=iteration==0?0:iteration==1?255:random_byte();
        atlas_pixels[i]=static_cast<BYTE>(random_byte()%(alpha+1));
        atlas_pixels[i+1]=static_cast<BYTE>(random_byte()%(alpha+1));
        atlas_pixels[i+2]=static_cast<BYTE>(random_byte()%(alpha+1));
        atlas_pixels[i+3]=static_cast<BYTE>(alpha);
    }
    Atlas atlas{};atlas.pixels=atlas_pixels.data();atlas.attempted=true;
    Tile tile{};require(tile.create(atlas,state,width,height,4),"cached atlas tile");
    for(std::size_t i=0;i<static_cast<std::size_t>(reference.width)*reference.height*4;i+=4){
        for(int c=0;c<4;++c){
            const BYTE value=background==0?0:background==1?255:
                background==2?static_cast<BYTE>((i/4+c*37+state*23)&255):
                static_cast<BYTE>(random_byte());
            reference.bits[i+c]=candidate.bits[i+c]=scalar.bits[i+c]=value;
        }
    }
    const Rect r{3,5,width,height};
    const BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    require(AlphaBlend(reference.dc,r.x,r.y,width,height,tile.dc,0,0,width,height,blend),"AlphaBlend oracle");
    require(GdiFlush(),"oracle GdiFlush");
    if(state==0&&background==0&&iteration==0&&width==17){
        DIBSECTION source_info{},target_info{};
        const int sn=GetObjectW(tile.bitmap,sizeof(source_info),&source_info);
        const int tn=GetObjectW(candidate.bitmap,sizeof(target_info),&target_info);
        RECT clip{};const int clip_type=GetClipBox(candidate.dc,&clip);
        POINT viewport{},window{};GetViewportOrgEx(candidate.dc,&viewport);GetWindowOrgEx(candidate.dc,&window);
        std::fprintf(stderr,"DIB probe source: n=%d bits=%p expected=%p bm=%ldx%ld pitch=%ld bi=%ldx%ld depth=%u compression=%lu section=%p offset=%lu\n",
            sn,source_info.dsBm.bmBits,tile.bits,source_info.dsBm.bmWidth,source_info.dsBm.bmHeight,
            source_info.dsBm.bmWidthBytes,source_info.dsBmih.biWidth,source_info.dsBmih.biHeight,
            source_info.dsBmih.biBitCount,source_info.dsBmih.biCompression,source_info.dshSection,source_info.dsOffset);
        std::fprintf(stderr,"DIB probe target: n=%d bits=%p expected=%p bm=%ldx%ld pitch=%ld bi=%ldx%ld depth=%u compression=%lu section=%p offset=%lu; DC type=%lu map=%d graphics=%d layout=%lu clip=%d [%ld,%ld,%ld,%ld] viewport=%ld,%ld window=%ld,%ld\n",
            tn,target_info.dsBm.bmBits,candidate.bits,target_info.dsBm.bmWidth,target_info.dsBm.bmHeight,
            target_info.dsBm.bmWidthBytes,target_info.dsBmih.biWidth,target_info.dsBmih.biHeight,
            target_info.dsBmih.biBitCount,target_info.dsBmih.biCompression,target_info.dshSection,target_info.dsOffset,
            GetObjectType(candidate.dc),GetMapMode(candidate.dc),GetGraphicsMode(candidate.dc),GetLayout(candidate.dc),
            clip_type,clip.left,clip.top,clip.right,clip.bottom,viewport.x,viewport.y,window.x,window.y);
    }
    DirectTargetScope owned(candidate.dc,candidate.width,candidate.height,candidate.bits);
    require(direct_paint(candidate.dc,r,tile),"direct_paint eligible DIB");
    for(int y=0;y<height;++y)
        blend_row_scalar(scalar.bits+(static_cast<std::size_t>(r.y+y)*scalar.width+r.x)*4,
                         tile.bits+static_cast<std::size_t>(y)*tile.stride,width);
    require(GdiFlush(),"candidate GdiFlush");
    const auto count=static_cast<std::size_t>(reference.width)*reference.height*4;
    for(std::size_t i=0;i<count;++i)if(reference.bits[i]!=candidate.bits[i]||
                                       reference.bits[i]!=scalar.bits[i]){
        std::fprintf(stderr,"mismatch state=%u background=%u iteration=%d size=%dx%d pixel=%zu channel=%zu SIMD=%u scalar=%u expected=%u\n",
            state,background,iteration,width,height,i/4,i%4,candidate.bits[i],scalar.bits[i],reference.bits[i]);
        throw std::runtime_error("direct blend differs from AlphaBlend");
    }
}
void fallback_checks(){
    Dib destination(51,39),bottom_up(51,39,32,false),rgb24(51,39,24);
    std::array<BYTE,17*13*4> src{};
    Tile tile{};tile.bits=src.data();tile.width=17;tile.height=13;tile.stride=17*4;
    const Rect rect{2,3,17,13};
    require(!direct_paint(destination.dc,rect,tile),"unregistered DIB fallback");
    DirectTargetScope owned(destination.dc,destination.width,destination.height,destination.bits);
    require(direct_paint(destination.dc,rect,tile),"registered top-down DIB");
    {
        DirectTargetScope wrong_bits(destination.dc,destination.width,destination.height,src.data());
        require(!direct_paint(destination.dc,rect,tile),"mismatched bits fallback");
    }
    {
        DirectTargetScope wrong_size(destination.dc,destination.width-1,destination.height,destination.bits);
        require(!direct_paint(destination.dc,rect,tile),"mismatched dimensions fallback");
    }
    require(direct_paint(destination.dc,rect,tile),"nested scope restoration");
    require(!direct_paint(destination.dc,{-1,3,17,13},tile),"negative coordinate fallback");
    require(!direct_paint(destination.dc,{49,3,17,13},tile),"outside DIB fallback");
    require(!direct_paint(bottom_up.dc,rect,tile),"bottom-up fallback");
    require(!direct_paint(rgb24.dc,rect,tile),"24-bit fallback");
    require(SetViewportOrgEx(destination.dc,1,0,nullptr),"viewport origin setup");
    require(!direct_paint(destination.dc,rect,tile),"viewport fallback");
    require(SetViewportOrgEx(destination.dc,0,0,nullptr),"viewport origin restore");
    require(SetMapMode(destination.dc,MM_ANISOTROPIC),"map mode setup");
    require(!direct_paint(destination.dc,rect,tile),"mapping fallback");
    require(SetMapMode(destination.dc,MM_TEXT),"map mode restore");
    require(IntersectClipRect(destination.dc,4,4,12,12)!=ERROR,"simple clip setup");
    require(!direct_paint(destination.dc,rect,tile),"partial clip fallback");
    SelectClipRgn(destination.dc,nullptr);
    HRGN first=CreateRectRgn(0,0,10,10),second=CreateRectRgn(12,0,51,39),both=CreateRectRgn(0,0,1,1);
    require(first&&second&&both,"clip regions");
    require(CombineRgn(both,first,second,RGN_OR)==COMPLEXREGION,"complex region setup");
    require(SelectClipRgn(destination.dc,both)==COMPLEXREGION,"complex clip selection");
    require(!direct_paint(destination.dc,rect,tile),"complex clip fallback");
    SelectClipRgn(destination.dc,nullptr);DeleteObject(first);DeleteObject(second);DeleteObject(both);
}
void tint_checks(){
    Dib source(1,1),reference(17,13),candidate(17,13),scalar(17,13);
    const Rect rect{2,3,11,7};
    require(!tint_dib(candidate.dc,rect,0x00030a18u,125),"unregistered tint fallback");
    DirectTargetScope owned(candidate.dc,candidate.width,candidate.height,candidate.bits);
    for(unsigned alpha=0;alpha<256;++alpha)
        for(unsigned color:{0x00030a18u,0x00000000u,0x00ffffffu,0x00b4712cu})
            for(unsigned background=0;background<3;++background){
                *reinterpret_cast<std::uint32_t*>(source.bits)=color;
                for(std::size_t i=0;i<static_cast<std::size_t>(reference.width)*reference.height*4;++i){
                    const BYTE value=background==0?0:background==1?255:
                        static_cast<BYTE>(random_byte());
                    reference.bits[i]=candidate.bits[i]=scalar.bits[i]=value;
                }
                const BLENDFUNCTION blend{AC_SRC_OVER,0,static_cast<BYTE>(alpha),0};
                require(AlphaBlend(reference.dc,rect.x,rect.y,rect.width,rect.height,
                                   source.dc,0,0,1,1,blend),"constant-alpha oracle");
                require(GdiFlush(),"constant-alpha oracle flush");
                require(tint_dib(candidate.dc,rect,color,static_cast<BYTE>(alpha)),"eligible tint");
                const BYTE raw[4]={static_cast<BYTE>(color&255u),
                                   static_cast<BYTE>((color>>8)&255u),
                                   static_cast<BYTE>((color>>16)&255u),0};
                for(int y=0;y<rect.height;++y)
                    blend_constant_scalar(scalar.bits+(static_cast<std::size_t>(rect.y+y)*scalar.width+rect.x)*4,
                                          raw,static_cast<BYTE>(alpha),rect.width);
                require(GdiFlush(),"constant-alpha candidate flush");
                const auto count=static_cast<std::size_t>(reference.width)*reference.height*4;
                for(std::size_t i=0;i<count;++i)if(reference.bits[i]!=candidate.bits[i]||
                                                   reference.bits[i]!=scalar.bits[i]){
                    std::fprintf(stderr,"tint mismatch alpha=%u color=%06x background=%u pixel=%zu channel=%zu SIMD=%u scalar=%u expected=%u\n",
                                 alpha,color,background,i/4,i%4,candidate.bits[i],scalar.bits[i],reference.bits[i]);
                    throw std::runtime_error("constant-alpha tint differs from AlphaBlend");
                }
            }
    std::puts("refit tint: 3072 exact RGBA constant-alpha comparisons OK");
}
void benchmark(){
    constexpr int width=1920,height=1080,frames=20;
    std::vector<BYTE> atlas_pixels(128*256*4);
    for(std::size_t i=0;i<atlas_pixels.size();i+=4){
        const unsigned alpha=random_byte();
        for(int c=0;c<3;++c)atlas_pixels[i+c]=static_cast<BYTE>(random_byte()%(alpha+1));
        atlas_pixels[i+3]=static_cast<BYTE>(alpha);
    }
    Atlas atlas{};atlas.pixels=atlas_pixels.data();atlas.attempted=true;
    Tile tile{};require(tile.create(atlas,0,width,height,8),"benchmark tile");
    Dib reference(width,height),candidate(width,height);
    const Rect rect{0,0,width,height};
    const BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    LARGE_INTEGER frequency{};require(QueryPerformanceFrequency(&frequency),"performance frequency");
    std::vector<double> gdi_times,direct_times;
    DirectTargetScope owned(candidate.dc,width,height,candidate.bits);
    for(int frame=0;frame<frames;++frame){
        std::fill_n(reference.bits,static_cast<std::size_t>(width)*height*4,static_cast<BYTE>(frame*11));
        std::fill_n(candidate.bits,static_cast<std::size_t>(width)*height*4,static_cast<BYTE>(frame*11));
        LARGE_INTEGER start{},end{};
        require(QueryPerformanceCounter(&start),"GDI timer start");
        require(AlphaBlend(reference.dc,0,0,width,height,tile.dc,0,0,width,height,blend),"benchmark AlphaBlend");
        require(GdiFlush(),"benchmark GDI flush");
        require(QueryPerformanceCounter(&end),"GDI timer end");
        gdi_times.push_back(1000.0*(end.QuadPart-start.QuadPart)/frequency.QuadPart);
        require(QueryPerformanceCounter(&start),"direct timer start");
        require(direct_paint(candidate.dc,rect,tile),"benchmark direct paint");
        require(GdiFlush(),"benchmark direct flush");
        require(QueryPerformanceCounter(&end),"direct timer end");
        direct_times.push_back(1000.0*(end.QuadPart-start.QuadPart)/frequency.QuadPart);
        require(std::equal(reference.bits,reference.bits+static_cast<std::size_t>(width)*height*4,
                           candidate.bits),"benchmark output mismatch");
    }
    std::sort(gdi_times.begin(),gdi_times.end());std::sort(direct_times.begin(),direct_times.end());
    std::printf("1920x1080 %d frames: AlphaBlend median %.3f ms, direct median %.3f ms\n",
                frames,gdi_times[frames/2],direct_times[frames/2]);
}
}
int main(){
    try{
        for(unsigned state=0;state<8;++state)
            for(const auto size:{std::array<int,2>{17,13},std::array<int,2>{64,33},std::array<int,2>{123,47}})
                for(unsigned background=0;background<4;++background)
                    for(int iteration=0;iteration<6;++iteration)
                        compare(size[0],size[1],state,background,iteration);
        fallback_checks();
        std::puts("refit skin blend: 576 exact RGBA AlphaBlend comparisons and DC fallbacks OK");
        tint_checks();
        benchmark();
        return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"refit skin blend: %s\n",error.what());return 1;}
}
