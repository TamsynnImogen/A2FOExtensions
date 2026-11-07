#include "loading_presentation.hpp"
#include "../../rust/refit_present_ffi.h"

// Copyright (c) 2026 Cedric Haase (original MIT loading-screen design).
// Layout adapted from armada2-refit ui/src/loading.rs; permission notice retained
// in third_party/armada2-refit/LICENSE.txt. Local implementation differs:
// left mission column, optional right preview/players, and bottom load status.
#include <algorithm>
#include <cmath>

namespace a2fo::refit_menu {
LoadingLayout loading_layout(int width, int height, const LoadingModel& model) {
    static_assert(sizeof(Rect) == sizeof(A2FORefitRect));
    A2FORefitLoadingLayout raw{};
    const A2FORefitLoadingInput input{
        static_cast<int>(model.kind), static_cast<int>(model.show_preview),
        model.players.size(),
        reinterpret_cast<const A2FORefitRect*>(model.protected_regions.data()),
        model.protected_regions.size()};
    a2fo_refit_loading_layout(width, height, input, &raw);
    LoadingLayout result;
    result.valid = raw.valid != 0;
    result.scale = raw.scale;
    const auto rect = [](A2FORefitRect r) { return Rect{r.x, r.y, r.width, r.height}; };
    result.kicker = rect(raw.kicker);
    result.title = rect(raw.title);
    result.briefing = rect(raw.briefing);
    result.objectives = rect(raw.objectives);
    result.hint = rect(raw.hint);
    result.preview = rect(raw.preview);
    result.players = rect(raw.players);
    result.status = rect(raw.status);
    result.percentage = rect(raw.percentage);
    return result;
}
} // namespace a2fo::refit_menu

#ifdef _WIN32
#include <windows.h>
#include <climits>
#include <cstdint>
#include <cwchar>
#include <limits>
#include <vector>

namespace a2fo::refit_menu {
namespace {
constexpr wchar_t medium_face[] = L"Noto Sans Condensed Medium";
constexpr wchar_t semibold_face[] = L"Noto Sans Condensed SemiBold";

bool fonts_ready() {
    static const bool ready = [] {
        HMODULE module = nullptr;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&fonts_ready), &module)) return false;
        for (int id : {101, 102}) {
            HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
            if (!resource) return false;
            HGLOBAL loaded = LoadResource(module, resource);
            if (!loaded) return false;
            void* data = LockResource(loaded);
            DWORD count = 0;
            if (!data || !SizeofResource(module, resource) ||
                !AddFontMemResourceEx(data, SizeofResource(module, resource), nullptr, &count) ||
                !count) return false;
        }
        return true;
    }();
    return ready;
}

COLORREF rgb(unsigned color) {
    return RGB((color >> 16) & 255u, (color >> 8) & 255u, color & 255u);
}
RECT win_rect(const Rect& r) {
    return {r.x, r.y, r.x + r.width, r.y + r.height};
}
bool wide(const std::string& text, std::wstring& out) {
    out.clear();
    if (text.empty()) return true;
    if (text.size() > static_cast<std::size_t>(INT_MAX)) return false;
    const int count = MultiByteToWideChar(CP_ACP, 0, text.data(),
                                         static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) return false;
    out.resize(count);
    return MultiByteToWideChar(CP_ACP, 0, text.data(),
                               static_cast<int>(text.size()), out.data(), count) == count;
}
bool fill(HDC dc, const Rect& r, unsigned color) {
    if (r.width <= 0 || r.height <= 0) return true;
    HBRUSH brush = CreateSolidBrush(rgb(color));
    if (!brush) return false;
    RECT box = win_rect(r);
    const bool ok = FillRect(dc, &box, brush) != 0;
    DeleteObject(brush);
    return ok;
}
bool text(HDC dc, const std::string& value, const Rect& bounds, int size,
          unsigned color, bool bold, UINT flags, int* used = nullptr) {
    if (used) *used = 0;
    if (value.empty() || bounds.width <= 0 || bounds.height <= 0) return true;
    std::wstring w;
    if (!wide(value, w)) return false;
    const int weight = bold ? FW_SEMIBOLD : FW_MEDIUM;
    const wchar_t* face = bold ? semibold_face : medium_face;
    HFONT font = CreateFontW(-std::max(1, size), 0, 0, 0, weight,
                             FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, face);
    if (!font) return false;
    HGDIOBJ old = SelectObject(dc, font);
    if (!old || old == HGDI_ERROR) { DeleteObject(font); return false; }
    wchar_t actual[LF_FACESIZE]{};
    bool ok = GetTextFaceW(dc, LF_FACESIZE, actual) > 0 &&
              _wcsicmp(actual, face) == 0 &&
              SetBkMode(dc, TRANSPARENT) != 0 &&
              SetTextColor(dc, rgb(color)) != CLR_INVALID;
    RECT box = win_rect(bounds);
    if (ok) {
        RECT measure = box;
        const int measured = DrawTextW(dc, w.c_str(), static_cast<int>(w.size()), &measure,
                                       flags | DT_CALCRECT | DT_NOPREFIX);
        if (used) *used = std::max(0, std::min(measured, bounds.height));
        ok = DrawTextW(dc, w.c_str(), static_cast<int>(w.size()), &box,
                       flags | DT_NOPREFIX) != 0;
    }
    SelectObject(dc, old);
    DeleteObject(font);
    return ok;
}
int loading_draw(void* raw,int op,A2FORefitRect box,std::uint32_t color,int size,int flags,
                 A2FORefitBytes value,const std::uint32_t* pixels,std::size_t pixel_count,
                 const A2FORefitPoint* points,std::size_t point_count,int* used) noexcept {
    try {
        HDC dc=static_cast<HDC>(raw);
        const Rect r{box.x,box.y,box.width,box.height};
        if(op==0)return fill(dc,r,color);
        if(op==1) {
            if(value.length&&!value.data)return false;
            const std::string ansi(value.length ? reinterpret_cast<const char*>(value.data) : "",
                                   value.length);
            UINT native=0;
            if(flags&2)native|=DT_SINGLELINE;
            if(flags&4)native|=DT_END_ELLIPSIS;
            if(flags&8)native|=DT_WORDBREAK;
            if(flags&16)native|=DT_EDITCONTROL;
            if(flags&32)native|=DT_CENTER;
            if(flags&64)native|=DT_VCENTER;
            if(flags&128)native|=DT_RIGHT;
            return text(dc,ansi,r,size,color,(flags&1)!=0,native,used);
        }
        if(op==3)return PatBlt(dc,r.x,r.y,r.width,r.height,BLACKNESS)!=0;
        if(op==4)return SaveDC(dc);
        if(op==5)return RestoreDC(dc,size)!=0;
        if(op==6)return ExcludeClipRect(dc,r.x,r.y,r.x+r.width,r.y+r.height)!=ERROR;
        if(op==7)return IntersectClipRect(dc,r.x,r.y,r.x+r.width,r.y+r.height)!=ERROR;
        if((op==2||op==8)&&(!pixels||r.width<=0||r.height<=0||
            static_cast<std::uint64_t>(r.width)*r.height!=pixel_count))return false;
        BITMAPINFO info{};
        info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=r.width;
        info.bmiHeader.biHeight=-r.height;
        info.bmiHeader.biPlanes=1;
        info.bmiHeader.biBitCount=32;
        info.bmiHeader.biCompression=BI_RGB;
        if(op==2)return StretchDIBits(dc,r.x,r.y,r.width,r.height,0,0,r.width,r.height,
            pixels,&info,DIB_RGB_COLORS,SRCCOPY)!=static_cast<int>(GDI_ERROR);
        if(op!=8||!points||point_count<3||point_count>64)return false;
        static_assert(sizeof(POINT)==sizeof(A2FORefitPoint));
        const POINT* native=reinterpret_cast<const POINT*>(points);
        HRGN shape=CreatePolygonRgn(native,static_cast<int>(point_count),WINDING);
        if(!shape)return false;
        const int saved=SaveDC(dc);
        bool ok=saved&&ExtSelectClipRgn(dc,shape,RGN_AND)!=ERROR;
        if(ok)ok=StretchDIBits(dc,r.x,r.y,r.width,r.height,0,0,r.width,r.height,
            pixels,&info,DIB_RGB_COLORS,SRCCOPY)!=static_cast<int>(GDI_ERROR);
        if(saved)RestoreDC(dc,saved);
        DeleteObject(shape);
        HPEN pen=CreatePen(PS_SOLID,std::max(1,size),rgb(color));
        if(!pen)return false;
        HGDIOBJ old=SelectObject(dc,pen);
        if(!old||old==HGDI_ERROR)ok=false;
        else {
            std::vector<POINT> closed(native,native+point_count);
            closed.push_back(native[0]);
            ok=Polyline(dc,closed.data(),static_cast<int>(closed.size()))!=0&&ok;
            SelectObject(dc,old);
        }
        DeleteObject(pen);
        return ok;
    } catch(...) {return false;}
}
} // namespace

bool paint_loading(HDC target, int width, int height, const LoadingModel& model,
                   const Artwork* backdrop, const Artwork* preview,
                   bool transparent_overlay) {
    LoadingModel placed=model;
    placed.show_preview=preview!=nullptr;
    const LoadingLayout geometry=loading_layout(width,height,placed);
    if(!target||!geometry.valid||!fonts_ready())return false;
    const auto bytes=[](const std::string& text)->A2FORefitBytes {
        return {reinterpret_cast<const std::uint8_t*>(text.data()),text.size()};
    };
    std::vector<A2FORefitBytes> briefing,objectives;
    briefing.reserve(model.briefing.size());objectives.reserve(model.objectives.size());
    for(const auto& text:model.briefing)briefing.push_back(bytes(text));
    for(const auto& text:model.objectives)objectives.push_back(bytes(text));
    std::vector<A2FORefitPlayer> players;
    players.reserve(model.players.size());
    for(const auto& player:model.players)
        players.push_back({bytes(player.name),bytes(player.detail),player.team,player.local,player.fraction});
    const auto theme=[&] {
        A2FORefitLoadingTheme result{static_cast<int>(model.theme.form),model.theme.panel_top,
            model.theme.panel,model.theme.panel_edge,model.theme.panel_hi,model.theme.text,
            model.theme.text_dim,model.theme.title,model.theme.accent,model.theme.accent2,
            model.theme.fill,model.theme.good,{}};
        std::copy(model.theme.corners.begin(),model.theme.corners.end(),result.corners);
        return result;
    }();
    static_assert(sizeof(Rect)==sizeof(A2FORefitRect));
    const A2FORefitLoadingPaintInput input{static_cast<int>(model.kind),backdrop!=nullptr,
        preview!=nullptr,bytes(model.title),bytes(model.mode),bytes(model.status),bytes(model.hint),
        briefing.data(),briefing.size(),objectives.data(),objectives.size(),players.data(),players.size(),
        model.progress,model.time_seconds,model.departure_shown,
        {model.bar_track.x,model.bar_track.y,model.bar_track.width,model.bar_track.height},
        reinterpret_cast<const A2FORefitRect*>(model.protected_regions.data()),
        model.protected_regions.size(),theme};
    const auto art=[](const Artwork* a)->A2FORefitArtwork {
        return a?A2FORefitArtwork{a->width,a->height,a->pixels.data(),a->pixels.size()}:
                 A2FORefitArtwork{0,0,nullptr,0};
    };
    return a2fo_refit_paint_loading(width,height,input,art(backdrop),art(preview),
        transparent_overlay,loading_draw,target)!=0;
}
} // namespace a2fo::refit_menu
#endif
