#include "setup_presentation.hpp"
#include "../../rust/refit_present_ffi.h"

// Layout proportions follow the locally vendored MIT-licensed armada2-refit
// reference (third_party/armada2-refit/SOURCE.md); this is a native C++ layout.


#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace a2fo::refit_menu::setup {
namespace {
static_assert(sizeof(Rect) == sizeof(A2FORefitRect));
A2FORefitRect ffi(Rect r) { return {r.x,r.y,r.width,r.height}; }
Rect native(A2FORefitRect r) { return {r.x,r.y,r.width,r.height}; }
std::vector<A2FORefitSetupControl> ffi_controls(const Model& model) {
    std::vector<A2FORefitSetupControl> result;
    result.reserve(model.controls.size());
    for (const Control& c : model.controls)
        result.push_back({c.key, static_cast<int>(c.kind), ffi(c.source),
            {reinterpret_cast<const std::uint8_t*>(c.text.data()),c.text.size()},
            c.message,c.visible,c.enabled,c.mode,c.open,c.down,c.item_height,
            ffi(c.pull),c.items.size(),nullptr,c.selected});
    return result;
}
std::vector<A2FORefitPlacement> ffi_placements(const Layout& layout) {
    std::vector<A2FORefitPlacement> result;
    result.reserve(layout.controls.size());
    for (const Placement& p : layout.controls)
        result.push_back({p.key,ffi(p.source),ffi(p.target),static_cast<int>(p.kind),p.item});
    return result;
}
A2FORefitSetupLayout ffi_layout(const Layout& layout) {
    return {layout.valid,layout.scale,ffi(layout.title),ffi(layout.players),
            ffi(layout.map),ffi(layout.preview),layout.controls.size()};
}
} // namespace
std::uintptr_t launch_at(const Model& model,const Layout& layout,int x,int y) {
    const auto controls=ffi_controls(model);
    const auto placed=ffi_placements(layout);
    const auto geometry=ffi_layout(layout);
    return a2fo_refit_setup_launch_at(controls.data(),controls.size(),&geometry,
                                      placed.data(),x,y);
}

Layout arrange(int width, int height, const Model& model, int popup_scroll) {
    const auto controls=ffi_controls(model);
    A2FORefitSetupLayout raw{};
    if (!a2fo_refit_setup_arrange(width,height,controls.data(),controls.size(),
                                  popup_scroll,&raw,nullptr,0)) return {};
    std::vector<A2FORefitPlacement> placed(raw.placement_count);
    if (!a2fo_refit_setup_arrange(width,height,controls.data(),controls.size(),
                                  popup_scroll,&raw,placed.data(),placed.size())) return {};
    Layout out;
    out.valid=raw.valid!=0;out.scale=raw.scale;
    out.title=native(raw.title);out.players=native(raw.players);
    out.map=native(raw.map);out.preview=native(raw.preview);
    for(const auto& p:placed)
        out.controls.push_back({p.key,native(p.source),native(p.target),
                                static_cast<Kind>(p.kind),p.item});
    return out;
}

NativeLayout arrange_native(int width,int height,int canvas_width,int canvas_height,
                            const Model& raw,int popup_scroll) {
    const auto controls=ffi_controls(raw);
    const int native_width=a2fo_refit_setup_native_width(width,height,canvas_width,canvas_height,
        controls.data(),controls.size(),popup_scroll);
    if(!native_width)return {};
    Model normalized=raw;
    auto native_controls=ffi_controls(normalized);
    if(!a2fo_refit_setup_normalize(native_width,native_controls.data(),native_controls.size()))return {};
    for(std::size_t i=0;i<normalized.controls.size();++i) {
        normalized.controls[i].source=native(native_controls[i].source);
        normalized.controls[i].pull=native(native_controls[i].pull);
        normalized.controls[i].item_height=native_controls[i].item_height;
    }
    auto geometry=arrange(width,height,normalized,popup_scroll);
    return {std::move(normalized),std::move(geometry),native_width,native_width*3/4};
}

bool map_point(const Layout& layout,int x,int y,int& sx,int& sy) {
    const auto placed=ffi_placements(layout);
    const auto geometry=ffi_layout(layout);
    return a2fo_refit_setup_map_point(&geometry,placed.data(),x,y,&sx,&sy)!=0;
}

Artwork loading_preview_snapshot(const Artwork& native_art,const Layout& layout) {
    const auto placed=ffi_placements(layout);
    const auto geometry=ffi_layout(layout);
    const A2FORefitArtwork art{native_art.width,native_art.height,
                               native_art.pixels.data(),native_art.pixels.size()};
    int width=0,height=0;
    if(!a2fo_refit_setup_preview(art,&geometry,placed.data(),nullptr,0,&width,&height))return {};
    Artwork result;
    result.width=width;result.height=height;
    result.pixels.resize(static_cast<std::size_t>(width)*height);
    if(!a2fo_refit_setup_preview(art,&geometry,placed.data(),result.pixels.data(),
                                 result.pixels.size(),&width,&height))return {};
    return result;
}
} // namespace a2fo::refit_menu::setup

#ifdef _WIN32
#include "setup_skin.hpp"
#include <windows.h>
#include <climits>
#include <cwchar>

namespace a2fo::refit_menu::setup {
namespace {
bool good(const Rect& r) { return r.width > 0 && r.height > 0; }
COLORREF color(unsigned v) { return RGB((v>>16)&255,(v>>8)&255,v&255); }
RECT wr(const Rect& r) { return {r.x,r.y,r.x+r.width,r.y+r.height}; }
bool fill(HDC dc,const Rect& r,unsigned c) {
    HBRUSH b=CreateSolidBrush(color(c));if(!b)return false;
    RECT q=wr(r);bool ok=FillRect(dc,&q,b)!=0;DeleteObject(b);return ok;
}
bool chamfer(HDC dc,const Rect& r,unsigned c,unsigned edge,int cut) {
    if(!good(r))return false;
    POINT p[8]={{r.x+cut,r.y},{r.x+r.width-cut,r.y},{r.x+r.width,r.y+cut},
        {r.x+r.width,r.y+r.height-cut},{r.x+r.width-cut,r.y+r.height},
        {r.x+cut,r.y+r.height},{r.x,r.y+r.height-cut},{r.x,r.y+cut}};
    HBRUSH b=CreateSolidBrush(color(c));HPEN pen=CreatePen(PS_SOLID,1,color(edge));
    if(!b||!pen){if(b)DeleteObject(b);if(pen)DeleteObject(pen);return false;}
    HGDIOBJ ob=SelectObject(dc,b),op=SelectObject(dc,pen);
    bool ok=ob&&ob!=HGDI_ERROR&&op&&op!=HGDI_ERROR&&Polygon(dc,p,8)!=0;
    if(op&&op!=HGDI_ERROR)SelectObject(dc,op);
    if(ob&&ob!=HGDI_ERROR)SelectObject(dc,ob);
    DeleteObject(pen);DeleteObject(b);return ok;
}
bool wide(const std::string& s,std::wstring& w) {
    w.clear();if(s.empty())return true;if(s.size()>INT_MAX)return false;
    int n=MultiByteToWideChar(CP_ACP,0,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(n<=0)return false;
    w.resize(n);
    return MultiByteToWideChar(CP_ACP,0,s.data(),static_cast<int>(s.size()),w.data(),n)==n;
}
bool text(HDC dc,const std::string& ansi,const Rect& r,int px,unsigned c,bool heavy=false,UINT align=DT_LEFT) {
    if(ansi.empty())return true;
    std::wstring value;if(!wide(ansi,value))return false;
    const wchar_t* face=heavy?L"Noto Sans Condensed SemiBold":L"Noto Sans Condensed Medium";
    HFONT font=CreateFontW(-std::max(1,px),0,0,0,heavy?FW_SEMIBOLD:FW_MEDIUM,
        FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,face);
    if(!font)return false;
    HGDIOBJ old=SelectObject(dc,font);
    wchar_t actual[LF_FACESIZE]{};
    bool ok=old&&old!=HGDI_ERROR&&GetTextFaceW(dc,LF_FACESIZE,actual)>0&&
        _wcsicmp(actual,face)==0&&SetBkMode(dc,TRANSPARENT)!=0&&
        SetTextColor(dc,color(c))!=CLR_INVALID;
    RECT rr=wr(r);
    ok=ok&&DrawTextW(dc,value.c_str(),static_cast<int>(value.size()),&rr,
                    align|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX)!=0;
    if(old&&old!=HGDI_ERROR)SelectObject(dc,old);
    DeleteObject(font);return ok;
}
bool valid(const Artwork* a) {
    return a&&a->width>0&&a->height>0&&
        static_cast<unsigned long long>(a->width)*a->height==a->pixels.size();
}
bool art(HDC dc,const Artwork& a,const Rect& dst,const Rect* src=nullptr,bool dim=false) {
    if(!good(dst)||!valid(&a))return false;
    Rect region=src?*src:Rect{0,0,a.width,a.height};
    if(!good(region)||region.x<0||region.y<0||region.width>a.width||region.height>a.height||
       region.x>a.width-region.width||region.y>a.height-region.height)return false;
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=a.width;
    bi.bmiHeader.biHeight=-region.height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    bi.bmiHeader.biCompression=BI_RGB;
    // Describe only the requested rows, retaining the full source row stride.
    // ySrc=0 avoids Wine's nonzero-source-Y crop behavior for top-down DIBs.
    const auto* rows=a.pixels.data()+static_cast<std::size_t>(region.y)*a.width;
    int result=StretchDIBits(dc,dst.x,dst.y,dst.width,dst.height,region.x,0,
        region.width,region.height,rows,&bi,DIB_RGB_COLORS,SRCCOPY);
    if(!result||result==static_cast<int>(GDI_ERROR))return false;
    if(dim) {
        if(setup_skin::tint_dib(dc,dst,0x00030a18u,125))return true;
        HDC layer=CreateCompatibleDC(dc);if(!layer)return false;
        BITMAPINFO overlay{};overlay.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        overlay.bmiHeader.biWidth=1;overlay.bmiHeader.biHeight=-1;overlay.bmiHeader.biPlanes=1;
        overlay.bmiHeader.biBitCount=32;overlay.bmiHeader.biCompression=BI_RGB;
        void* pixel=nullptr;HBITMAP bitmap=CreateDIBSection(layer,&overlay,DIB_RGB_COLORS,&pixel,nullptr,0);
        if(!bitmap||!pixel){if(bitmap)DeleteObject(bitmap);DeleteDC(layer);return false;}
        *static_cast<std::uint32_t*>(pixel)=0x00030a18u;
        HGDIOBJ old=SelectObject(layer,bitmap);
        BLENDFUNCTION blend{AC_SRC_OVER,0,125,0};
        bool ok=old&&old!=HGDI_ERROR&&AlphaBlend(dc,dst.x,dst.y,dst.width,dst.height,layer,0,0,1,1,blend)!=0;
        if(old&&old!=HGDI_ERROR)SelectObject(layer,old);
        DeleteObject(bitmap);DeleteDC(layer);
        return ok;
    }
    return true;
}

}
bool wrapped_text(HDC target,const std::string& ansi,const Rect& target_box,int px,unsigned rgb) {
    std::wstring value;if(!wide(ansi,value))return false;
    HFONT font=CreateFontW(-px,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
        DEFAULT_PITCH|FF_SWISS,L"Noto Sans Condensed Medium");
    if(!font)return false;
    HGDIOBJ old=SelectObject(target,font);wchar_t face[LF_FACESIZE]{};
    bool face_ok=old&&old!=HGDI_ERROR&&GetTextFaceW(target,LF_FACESIZE,face)>0&&
        _wcsicmp(face,L"Noto Sans Condensed Medium")==0;
    RECT area=wr(target_box);
    if(face_ok&&!value.empty()) {
        SetBkMode(target,TRANSPARENT);SetTextColor(target,color(rgb));
        face_ok=DrawTextW(target,value.c_str(),static_cast<int>(value.size()),&area,
            DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX)!=0;
    }
    if(old&&old!=HGDI_ERROR)SelectObject(target,old);
    DeleteObject(font);
    return face_ok;
}
struct SetupDrawContext { HDC dc; const Artwork* background; const Artwork* native; };
int setup_draw(void* raw,int op,A2FORefitRect box,A2FORefitRect source,
               std::uint32_t rgb,std::uint32_t edge,int size,int flags,
               A2FORefitBytes bytes,A2FORefitArtwork artwork) noexcept {
    try {
        auto& context=*static_cast<SetupDrawContext*>(raw);
        HDC dc=context.dc;
        const Rect r{box.x,box.y,box.width,box.height};
        if(op==0)return fill(dc,r,rgb);
        if(op==1)return setup_skin::paint(dc,r,static_cast<setup_skin::Part>(flags),size);
        if(op==2)return chamfer(dc,r,rgb,edge,size);
        if(op==3||op==5) {
            if(bytes.length&&!bytes.data)return false;
            const std::string value(bytes.length ? reinterpret_cast<const char*>(bytes.data) : "",
                                    bytes.length);
            if(op==5)return wrapped_text(dc,value,r,size,rgb);
            return text(dc,value,r,size,rgb,(flags&1)!=0,(flags&2)?DT_CENTER:DT_LEFT);
        }
        if(op==4) {
            const Artwork* native=context.background&&context.background->pixels.data()==artwork.pixels
                ?context.background:context.native&&context.native->pixels.data()==artwork.pixels
                ?context.native:nullptr;
            if(!native)return false;
            const Rect src{source.x,source.y,source.width,source.height};
            return art(dc,*native,r,&src,(flags&1)!=0);
        }
        return false;
    } catch (...) { return false; }
}
bool paint(HDC target,int width,int height,const Model& model,const Layout& layout,
           const Artwork* background,const Artwork* native_canvas,int hot_x,int hot_y) {
    if(!target)return false;
    auto controls=ffi_controls(model);
    std::vector<std::vector<A2FORefitBytes>> items(model.controls.size());
    for(std::size_t i=0;i<model.controls.size();++i) {
        for(const auto& item:model.controls[i].items)
            items[i].push_back({reinterpret_cast<const std::uint8_t*>(item.data()),item.size()});
        controls[i].items=items[i].data();
    }
    const auto placements=ffi_placements(layout);
    const auto geometry=ffi_layout(layout);
    const auto art=[](const Artwork* a)->A2FORefitArtwork {
        return a?A2FORefitArtwork{a->width,a->height,a->pixels.data(),a->pixels.size()}:
                 A2FORefitArtwork{0,0,nullptr,0};
    };
    SetupDrawContext context{target,background,native_canvas};
    return a2fo_refit_paint_setup(width,height,controls.data(),controls.size(),&geometry,
        placements.data(),art(background),art(native_canvas),background!=nullptr,
        native_canvas!=nullptr,hot_x,hot_y,setup_draw,&context)!=0;
}
} // namespace a2fo::refit_menu::setup
#endif
