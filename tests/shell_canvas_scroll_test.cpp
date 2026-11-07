// Scroll and dirty-paint regression fixture. All HWNDs are tokens and all
// raster output goes to memory DIBs; no desktop window is created or touched.
#include "../modules/A2FOShellDisplay/canvas.hpp"
#include <windows.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <vector>

namespace {
BOOL WINAPI host_GetClientRect(HWND, LPRECT);
HDC WINAPI host_GetDC(HWND);
int WINAPI host_ReleaseDC(HWND, HDC);
HDC WINAPI host_BeginPaint(HWND, LPPAINTSTRUCT);
BOOL WINAPI host_EndPaint(HWND, const PAINTSTRUCT*);
int WINAPI host_ScrollWindowEx(HWND, int, int, const RECT*, const RECT*, HRGN, LPRECT, UINT);
BOOL WINAPI host_InvalidateRect(HWND, const RECT*, BOOL);
BOOL WINAPI host_InvalidateRgn(HWND, HRGN, BOOL);
}
#undef GetClientRect
#undef GetDC
#undef ReleaseDC
#undef BeginPaint
#undef EndPaint
#undef ScrollWindowEx
#undef InvalidateRect
#undef InvalidateRgn
#define GetClientRect host_GetClientRect
#define GetDC host_GetDC
#define ReleaseDC host_ReleaseDC
#define BeginPaint host_BeginPaint
#define EndPaint host_EndPaint
#define ScrollWindowEx host_ScrollWindowEx
#define InvalidateRect host_InvalidateRect
#define InvalidateRgn host_InvalidateRgn
#include "../modules/A2FOShellDisplay/canvas.cpp"
#undef GetClientRect
#undef GetDC
#undef ReleaseDC
#undef BeginPaint
#undef EndPaint
#undef ScrollWindowEx
#undef InvalidateRect
#undef InvalidateRgn

namespace canvas = a2fo::shell_display::canvas;
namespace {
constexpr int kLogicalWidth = 800, kLogicalHeight = 600;
constexpr int kPhysicalWidth = 1600, kPhysicalHeight = 900;
HWND g_master = reinterpret_cast<HWND>(0x1000);
HWND g_child = reinterpret_cast<HWND>(0x2000);
HWND g_untracked = reinterpret_cast<HWND>(0x3000);
HDC g_output_dc{};
int g_native_calls{}, g_dc_calls{}, g_release_calls{}, g_begin_calls{}, g_end_calls{};
int g_invalidate_rect_calls{}, g_invalidate_region_calls{};
int g_native_result = SIMPLEREGION;
RECT g_native_update{0, 0, 20, 15};
int g_native_dx{}, g_native_dy{};
RECT g_native_scroll{}, g_native_clip{};
bool g_had_scroll{}, g_had_clip{}, g_input_output_alias{};
UINT g_native_flags{};
RECT g_invalid_rect{}, g_invalid_region{};
bool g_had_invalid_rect{}, g_had_invalid_region{}, g_invalid_erase{};
RECT g_paint_rect{};
int g_paint_saved{};
std::map<HWND, RECT> g_clients;

COLORREF row_color(int y) {
    const BYTE v = static_cast<BYTE>((y % 200) + 20);
    return RGB(v, v, v);
}

BOOL WINAPI host_GetClientRect(HWND window, LPRECT rect) {
    const auto it = g_clients.find(window);
    if (it == g_clients.end() || !rect) return FALSE;
    *rect = it->second;
    return TRUE;
}

HDC WINAPI host_GetDC(HWND window) {
    if (window != g_master) return nullptr;
    ++g_dc_calls;
    return g_output_dc;
}

int WINAPI host_ReleaseDC(HWND window, HDC dc) {
    assert(window == g_master && dc == g_output_dc);
    ++g_release_calls;
    return 1;
}

HDC WINAPI host_BeginPaint(HWND window, LPPAINTSTRUCT paint) {
    if (window != g_master || !paint) return nullptr;
    ++g_begin_calls;
    paint->hdc = g_output_dc;
    paint->fErase = FALSE;
    paint->rcPaint = g_paint_rect;
    g_paint_saved = SaveDC(g_output_dc);
    assert(g_paint_saved);
    assert(IntersectClipRect(g_output_dc, g_paint_rect.left, g_paint_rect.top,
        g_paint_rect.right, g_paint_rect.bottom) != ERROR);
    return g_output_dc;
}

BOOL WINAPI host_EndPaint(HWND window, const PAINTSTRUCT* paint) {
    assert(window == g_master && paint && paint->hdc == g_output_dc &&
           EqualRect(&paint->rcPaint, &g_paint_rect));
    ++g_end_calls;
    if (g_paint_saved) RestoreDC(g_output_dc, g_paint_saved);
    g_paint_saved = 0;
    return TRUE;
}

int WINAPI host_ScrollWindowEx(HWND window, int dx, int dy, const RECT* scroll,
                               const RECT* clip, HRGN update, LPRECT update_rect,
                               UINT flags) {
    ++g_native_calls;
    assert(window == g_master || window == g_child || window == g_untracked);
    g_native_dx = dx; g_native_dy = dy; g_native_flags = flags;
    g_had_scroll = scroll != nullptr; g_had_clip = clip != nullptr;
    if (scroll) g_native_scroll = *scroll;
    if (clip) g_native_clip = *clip;
    g_input_output_alias = update_rect && clip == update_rect;
    if (g_native_result == ERROR) return ERROR;
    if (update) assert(SetRectRgn(update, g_native_update.left, g_native_update.top,
                                  g_native_update.right, g_native_update.bottom));
    if (update_rect) *update_rect = g_native_update;
    return g_native_result;
}

BOOL WINAPI host_InvalidateRect(HWND window, const RECT* rect, BOOL erase) {
    assert(window == g_master || window == g_child);
    ++g_invalidate_rect_calls;
    g_had_invalid_rect = rect != nullptr;
    if (rect) g_invalid_rect = *rect;
    g_invalid_erase = erase != FALSE;
    return TRUE;
}

BOOL WINAPI host_InvalidateRgn(HWND window, HRGN region, BOOL erase) {
    assert(window == g_master || window == g_child);
    ++g_invalidate_region_calls;
    g_had_invalid_region = region != nullptr;
    if (region) assert(GetRgnBox(region, &g_invalid_region) != ERROR);
    g_invalid_erase = erase != FALSE;
    return TRUE;
}

struct OutputDib {
    HDC dc{};
    HBITMAP bitmap{};
    HGDIOBJ previous{};
    std::uint8_t* bits{};
    OutputDib() {
        dc = CreateCompatibleDC(nullptr);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = kPhysicalWidth;
        info.bmiHeader.biHeight = -kPhysicalHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS,
            reinterpret_cast<void**>(&bits), nullptr, 0);
        assert(dc && bitmap && bits);
        previous = SelectObject(dc, bitmap);
        assert(previous && previous != HGDI_ERROR);
        g_output_dc = dc;
    }
    ~OutputDib() {
        g_output_dc = nullptr;
        SelectObject(dc, previous);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }
    COLORREF pixel(int x, int y) const { return GetPixel(dc, x, y); }
    void clear(COLORREF color) {
        HBRUSH brush = CreateSolidBrush(color);
        RECT all{0, 0, kPhysicalWidth, kPhysicalHeight};
        assert(FillRect(dc, &all, brush));
        DeleteObject(brush);
    }
};

void reset_native_observation() {
    g_native_calls = 0;
    g_native_dx = g_native_dy = 0;
    g_had_scroll = g_had_clip = g_input_output_alias = false;
    g_native_scroll = g_native_clip = RECT{};
    g_native_flags = 0;
}

void seed_rows(HDC dc) {
    for (int y = 0; y < kLogicalHeight; ++y) {
        HBRUSH brush = CreateSolidBrush(row_color(y));
        RECT row{0, y, kLogicalWidth, y + 1};
        assert(FillRect(dc, &row, brush));
        DeleteObject(brush);
    }
    assert(SetPixelV(dc, 100, 50, RGB(255, 0, 255)));
    assert(SetPixelV(dc, 200, 200, RGB(0, 255, 255)));
    GdiFlush();
}

bool equal_rect(const RECT& a, const RECT& b) {
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

void scroll_success(const RECT* scroll, const RECT* clip, HRGN update,
                    LPRECT update_rect, int dx, int dy, UINT flags) {
    assert(canvas::scroll_window_ex(g_master, dx, dy, scroll, clip,
                                    update, update_rect, flags) != ERROR);
}
}

int main() {
    OutputDib output;
    output.clear(RGB(3, 4, 5));
    g_clients[g_master] = RECT{0, 0, kPhysicalWidth, kPhysicalHeight};
    g_clients[g_child] = RECT{0, 0, 400, 300};
    g_clients[g_untracked] = RECT{0, 0, kPhysicalWidth, kPhysicalHeight};

    canvas::g_ready.store(true, std::memory_order_release);
    canvas::g_mode = {1920, 1080, 32};
    auto master = std::make_shared<canvas::Window>();
    master->handle = g_master; master->root = g_master; master->owner = g_master;
    master->width = kLogicalWidth; master->height = kLogicalHeight;
    master->root_scale = {kLogicalWidth, kLogicalHeight, kPhysicalWidth, kPhysicalHeight};
    canvas::g_windows[g_master] = master;
    auto child = std::make_shared<canvas::Window>();
    child->handle = g_child; child->root = g_master; child->parent = g_master;
    child->owner = g_master; child->width = 400; child->height = 300;
    child->root_scale = master->root_scale;
    canvas::g_windows[g_child] = child;

    // A first scroll has no backing pixels yet. Forward the native operation
    // and invalidate the complete affected client so painting initializes it.
    reset_native_observation();
    g_native_update = RECT{20, 30, 1580, 45};
    RECT first_update{};
    scroll_success(nullptr, nullptr, nullptr, &first_update, 0, 10, SW_INVALIDATE);
    assert(g_native_calls == 1 && g_native_dx == 0 && g_native_dy == 15);
    assert(g_invalidate_rect_calls == 1 && g_had_invalid_rect &&
           equal_rect(g_invalid_rect, RECT{0, 0, kPhysicalWidth, kPhysicalHeight}));

    HDC logical = master->surface.get(g_output_dc, kLogicalWidth, kLogicalHeight);
    assert(logical);
    seed_rows(logical);
    const canvas::Scale scale{kLogicalWidth, kLogicalHeight,
                              kPhysicalWidth, kPhysicalHeight};
    assert(master->surface.present(g_output_dc, scale));
    assert(output.pixel(200, 150) == row_color(100));

    // NextGrid aliases clip input with update output. The hook must snapshot
    // its logical clip, call USER32 with scaled rectangles, and return logical
    // rectangle and region outputs.
    RECT scroll{10, 20, 790, 580};
    RECT alias_clip{0, 0, 800, 600};
    HRGN update_region = CreateRectRgn(0, 0, 0, 0);
    assert(update_region);
    reset_native_observation();
    g_native_update = RECT{20, 30, 1580, 45};
    const int before_dc = g_dc_calls, before_release = g_release_calls;
    scroll_success(&scroll, &alias_clip, update_region, &alias_clip,
                   0, 10, SW_INVALIDATE);
    assert(g_native_calls == 1 && g_native_dx == 0 && g_native_dy == 15 &&
           g_native_flags == SW_INVALIDATE && g_had_scroll && g_had_clip &&
           !g_input_output_alias);
    assert(equal_rect(g_native_scroll, RECT{20, 30, 1580, 870}) &&
           equal_rect(g_native_clip, RECT{0, 0, 1600, 900}));
    assert(equal_rect(alias_clip, RECT{10, 20, 790, 30}));
    RECT region_bounds{};
    assert(GetRgnBox(update_region, &region_bounds) != ERROR &&
           equal_rect(region_bounds, RECT{10, 20, 790, 30}));
    assert(g_dc_calls == before_dc + 1 && g_release_calls == before_release + 1);
    assert(g_invalidate_region_calls > 0 && g_had_invalid_region &&
           equal_rect(g_invalid_region, RECT{20, 30, 1580, 45}));
    assert(GetPixel(logical, 100, 40) == row_color(30));
    assert(output.pixel(200, 60) == row_color(30));
    DeleteObject(update_region);

    // Partial paint gets an outward-rounded logical dirty box plus one pixel.
    // Filling its entire returned DC must still leave cached and screen pixels
    // outside the stripe unchanged.
    const COLORREF untouched_logical = GetPixel(logical, 100, 100);
    const COLORREF untouched_screen = output.pixel(200, 150);
    g_paint_rect = RECT{20, 30, 1580, 45};
    PAINTSTRUCT paint{};
    const HDC paint_dc = canvas::begin_paint(g_master, &paint);
    assert(paint_dc && paint_dc != g_output_dc &&
           equal_rect(paint.rcPaint, RECT{9, 19, 791, 31}));
    RECT clip_box{};
    assert(GetClipBox(paint_dc, &clip_box) == SIMPLEREGION &&
           equal_rect(clip_box, RECT{9, 19, 791, 31}));
    assert(PatBlt(paint_dc, 0, 0, kLogicalWidth, kLogicalHeight, WHITENESS));
    assert(canvas::end_paint(g_master, &paint));
    assert(g_begin_calls == 1 && g_end_calls == 1);
    assert(GetPixel(logical, 100, 100) == untouched_logical);
    assert(output.pixel(200, 150) == untouched_screen);
    assert(output.pixel(200, 35) == RGB(255, 255, 255));

    // Negative vertical and horizontal scrolls move the cached logical rows
    // and markers in the same direction as the native callback.
    seed_rows(logical);
    RECT full{0, 0, 800, 600};
    g_native_update = RECT{0, 885, 1600, 900};
    reset_native_observation();
    scroll_success(&full, &full, nullptr, nullptr, 0, -10, SW_INVALIDATE);
    assert(g_native_dy == -15 && GetPixel(logical, 100, 20) == row_color(30));

    seed_rows(logical);
    g_native_update = RECT{0, 0, 26, 900};
    reset_native_observation();
    scroll_success(&full, &full, nullptr, nullptr, 13, 0, SW_INVALIDATE);
    assert(g_native_dx == 26 && GetPixel(logical, 113, 50) == RGB(255, 0, 255));

    // Native failure leaves the cached surface untouched and does not publish
    // native update outputs to the caller.
    seed_rows(logical);
    const COLORREF before_failure = GetPixel(logical, 100, 50);
    RECT failed_update{7, 8, 9, 10};
    g_native_result = ERROR;
    reset_native_observation();
    const int failed = canvas::scroll_window_ex(g_master, 5, 7, &full, &full,
                                                 nullptr, &failed_update, SW_INVALIDATE);
    g_native_result = SIMPLEREGION;
    assert(failed == ERROR && g_native_calls == 1 &&
           GetPixel(logical, 100, 50) == before_failure &&
           equal_rect(failed_update, RECT{7, 8, 9, 10}));

    // Inactive, unknown-window, child-scroll and smooth-scroll calls preserve
    // their original native arguments and bypass the logical cache.
    const int calls_before_passthrough = g_native_calls;
    const int dc_before_passthrough = g_dc_calls;
    canvas::g_ready.store(false, std::memory_order_release);
    RECT pass_rect{2, 3, 40, 50};
    canvas::scroll_window_ex(g_master, 7, -9, &pass_rect, &pass_rect,
                             nullptr, &pass_rect, SW_INVALIDATE);
    assert(g_native_calls == calls_before_passthrough + 1 && g_native_dx == 7 &&
           g_native_dy == -9 && g_input_output_alias &&
           equal_rect(g_native_clip, RECT{2, 3, 40, 50}) && g_dc_calls == dc_before_passthrough);
    canvas::g_ready.store(true, std::memory_order_release);
    canvas::scroll_window_ex(g_untracked, 3, 4, &pass_rect, nullptr,
                             nullptr, nullptr, SW_INVALIDATE);
    assert(g_native_dx == 3 && g_native_dy == 4 && g_had_scroll && !g_had_clip &&
           equal_rect(g_native_scroll, pass_rect));
    canvas::scroll_window_ex(g_master, 5, 6, &pass_rect, nullptr,
                             nullptr, nullptr, SW_SCROLLCHILDREN);
    assert(g_native_dx == 5 && g_native_dy == 6 && g_native_flags == SW_SCROLLCHILDREN);
    canvas::scroll_window_ex(g_master, 1, 2, nullptr, nullptr,
                             nullptr, nullptr, SW_SMOOTHSCROLL | SW_INVALIDATE);
    assert(g_native_dx == 1 && g_native_dy == 2 &&
           g_native_flags == (SW_SMOOTHSCROLL | SW_INVALIDATE));
    canvas::shutdown();
    std::puts("shell canvas scroll, cached rows and dirty paint passed");
}
