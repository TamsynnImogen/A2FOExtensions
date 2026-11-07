#include "setup_skin.hpp"
#include "../../rust/refit_native_ffi.h"

#ifdef _WIN32
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <tuple>

namespace a2fo::refit_menu::setup_skin {
namespace {
constexpr int cell=64,resource_id=301;
constexpr std::array<std::uint32_t,8> format{{0x31534955,128,256,cell,cell,8,0,0}};
struct Atlas {
    const BYTE* pixels{};bool attempted{};
    bool load(){
        if(attempted)return pixels!=nullptr;
        attempted=true;
        HMODULE module{};
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&available),&module))return false;
        HRSRC resource=FindResourceW(module,MAKEINTRESOURCEW(resource_id),MAKEINTRESOURCEW(10));
        if(!resource||!a2fo_refit_skin_atlas_size_valid(SizeofResource(module,resource)))return false;
        HGLOBAL loaded=LoadResource(module,resource);
        const auto* data=loaded?static_cast<const BYTE*>(LockResource(loaded)):nullptr;
        if(!data)return false;
        if(!a2fo_refit_skin_atlas_valid(data,SizeofResource(module,resource)))return false;
        pixels=data+sizeof(format);return true;
    }
};
struct Tile {
    HDC dc{};HBITMAP bitmap{};HGDIOBJ previous{};
    const BYTE* bits{};int width{},height{},stride{};
    ~Tile(){if(dc&&previous)SelectObject(dc,previous);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
    bool create(const Atlas& atlas,unsigned index,int width,int height,int corner){
        if(!a2fo_refit_skin_tile_valid(width,height))return false;
        dc=CreateCompatibleDC(nullptr);if(!dc)return false;
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* pixels{};bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        if(!bitmap||!pixels)return false;
        const auto selected=SelectObject(dc,bitmap);
        if(!selected||selected==HGDI_ERROR)return false;
        previous=selected;
        bits=static_cast<const BYTE*>(pixels);this->width=width;this->height=height;
        stride=width*4;
        return a2fo_refit_skin_fill_tile(atlas.pixels,static_cast<BYTE*>(pixels),index,
                                         width,height,corner)!=0;
    }
};
struct Cache {
    Atlas atlas;
    using Key=std::tuple<unsigned,int,int,int>;
    std::map<Key,std::unique_ptr<Tile>> tiles;
    std::size_t pixels{};
    Tile* get(unsigned part,int width,int height,int corner){
        const Key key{part,width,height,corner};
        const auto found=tiles.find(key);if(found!=tiles.end())return found->second.get();
        const auto count=static_cast<std::uint64_t>(width)*height;
        const int action=a2fo_refit_skin_cache_decision(width,height,tiles.size(),pixels);
        if(!action)return nullptr;
        if(action==2){GdiFlush();tiles.clear();pixels=0;}
        auto tile=std::make_unique<Tile>();if(!tile->create(atlas,part,width,height,corner))return nullptr;
        auto* result=tile.get();tiles.emplace(key,std::move(tile));pixels+=static_cast<std::size_t>(count);
        return result;
    }
};
thread_local Cache cache;
struct DirectTarget {HDC dc{};int width{},height{};void* bits{};};
thread_local DirectTarget direct_target;
[[maybe_unused]] void blend_row_scalar(BYTE* output,const BYTE* source,int width){
    a2fo_refit_skin_blend(output,source,width);
}
[[maybe_unused]] void blend_constant_scalar(BYTE* output,const BYTE* source,BYTE alpha,int width){
    const unsigned rgb=static_cast<unsigned>(source[0])|
                       (static_cast<unsigned>(source[1])<<8)|
                       (static_cast<unsigned>(source[2])<<16);
    a2fo_refit_skin_tint(output,rgb,alpha,width);
}
// Only a private, unmapped, top-down BGRA DIB with identity pixel coordinates is
// safe to write directly. Other DCs retain AlphaBlend's clipping and conversion.
struct DirectPixels {BYTE* bits{};int stride{};};
bool direct_pixels(HDC target,const Rect& r,DirectPixels& view){
    A2FORefitSkinDirect info{};
    info.registered=(direct_target.dc==target&&direct_target.bits&&
                     direct_target.width>0&&direct_target.height>0);
    const A2FORefitNativeRect rectangle{r.x,r.y,r.width,r.height};
    if(!a2fo_refit_skin_direct_precheck(target!=nullptr,info.registered,
        direct_target.width,direct_target.height,rectangle))return false;
    info.dc_kind=GetObjectType(target);
    if(!a2fo_refit_skin_direct_property(0,info.dc_kind))return false;
    info.map_mode=GetMapMode(target);
    if(!a2fo_refit_skin_direct_property(1,info.map_mode))return false;
    info.graphics_mode=GetGraphicsMode(target);
    if(!a2fo_refit_skin_direct_property(2,info.graphics_mode))return false;
    info.layout=GetLayout(target);
    if(!a2fo_refit_skin_direct_property(3,info.layout))return false;
    POINT viewport{},window{};
    if(!GetViewportOrgEx(target,&viewport)||!GetWindowOrgEx(target,&window))return false;
    info.viewport_x=viewport.x;info.viewport_y=viewport.y;
    info.window_x=window.x;info.window_y=window.y;
    if(!a2fo_refit_skin_direct_origins(info.viewport_x,info.viewport_y,
                                        info.window_x,info.window_y))return false;
    RECT clip{};info.clip_kind=GetClipBox(target,&clip);
    info.clip_left=clip.left;info.clip_top=clip.top;
    info.clip_right=clip.right;info.clip_bottom=clip.bottom;
    if(!a2fo_refit_skin_direct_clip(rectangle,info.clip_kind,info.clip_left,info.clip_top,
                                     info.clip_right,info.clip_bottom))return false;
    const auto selected=GetCurrentObject(target,OBJ_BITMAP);
    DIBSECTION section{};
    info.object_valid=selected&&GetObjectW(selected,sizeof(section),&section)==sizeof(section);
    if(info.object_valid){
        info.bits_match=section.dsBm.bmBits==direct_target.bits&&
                        section.dsBm.bmWidth==direct_target.width&&
                        section.dsBm.bmHeight==direct_target.height;
        info.section_null=section.dshSection==nullptr;
        info.section_offset=static_cast<int32_t>(section.dsOffset);
        info.header_size=static_cast<int32_t>(section.dsBmih.biSize);
        info.planes=section.dsBmih.biPlanes;info.bit_count=section.dsBmih.biBitCount;
        info.compression=static_cast<int32_t>(section.dsBmih.biCompression);
        info.header_width=section.dsBmih.biWidth;info.header_height=section.dsBmih.biHeight;
        info.width=section.dsBm.bmWidth;info.height=section.dsBm.bmHeight;
        info.stride=section.dsBm.bmWidthBytes;
        info.bitmap_planes=section.dsBm.bmPlanes;
        info.bitmap_bit_count=section.dsBm.bmBitsPixel;
        info.bits=static_cast<BYTE*>(section.dsBm.bmBits);
    }
    if(!a2fo_refit_skin_direct_valid(rectangle,&info)||!GdiFlush())return false;
    view={info.bits,info.stride};return true;
}
bool direct_paint(HDC target,const Rect& r,const Tile& tile){
    if(!a2fo_refit_skin_tile_matches({r.x,r.y,r.width,r.height},tile.bits!=nullptr,
                                      tile.width,tile.height))return false;
    DirectPixels view{};if(!direct_pixels(target,r,view))return false;
    static const bool use_sse2=IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE)!=0;
    for(int y=0;y<r.height;++y){
        auto* output=view.bits+(static_cast<std::size_t>(r.y+y)*view.stride+
                          static_cast<std::size_t>(r.x)*4);
        const auto* source=tile.bits+static_cast<std::size_t>(y)*tile.stride;
        a2fo_refit_skin_blend_fast(output,source,r.width,use_sse2);
    }
    return true;
}
}
DirectTargetScope::DirectTargetScope(HDC target,int width,int height,void* bits){
    previous_target_=direct_target.dc;previous_width_=direct_target.width;
    previous_height_=direct_target.height;previous_bits_=direct_target.bits;
    direct_target={target,width,height,bits};
}
DirectTargetScope::~DirectTargetScope(){
    direct_target={previous_target_,previous_width_,previous_height_,previous_bits_};
}
bool available(){return cache.atlas.load();}
bool tint_dib(HDC target,const Rect& r,unsigned rgb,BYTE alpha){
    DirectPixels view{};if(!direct_pixels(target,r,view))return false;
    static const bool use_sse2=IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE)!=0;
    for(int y=0;y<r.height;++y){
        auto* output=view.bits+(static_cast<std::size_t>(r.y+y)*view.stride+
                                 static_cast<std::size_t>(r.x)*4);
        a2fo_refit_skin_tint_fast(output,rgb,alpha,r.width,use_sse2);
    }
    return true;
}
bool paint(HDC target,const Rect& r,Part part,int corner){
    const auto index=static_cast<unsigned>(part);
    struct PaintContext {HDC target;const Rect* r;unsigned index;int corner;Tile* tile;};
    PaintContext context{target,&r,index,corner,nullptr};
    const auto operation=[](void* raw,int op)->int {
        try {
        auto& c=*static_cast<PaintContext*>(raw);
        switch(op){
        case 0:return available();
        case 1:c.tile=cache.get(c.index,c.r->width,c.r->height,c.corner);return c.tile!=nullptr;
        case 2:return direct_paint(c.target,*c.r,*c.tile);
        case 3:{const BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
            return AlphaBlend(c.target,c.r->x,c.r->y,c.r->width,c.r->height,
                              c.tile->dc,0,0,c.r->width,c.r->height,blend)!=FALSE;}
        default:return 0;
        }
        }catch(...){return 0;}
    };
    return a2fo_refit_skin_paint(target!=nullptr,{r.x,r.y,r.width,r.height},
                                 index,corner,operation,&context)!=0;
}
} // namespace a2fo::refit_menu::setup_skin
#endif
