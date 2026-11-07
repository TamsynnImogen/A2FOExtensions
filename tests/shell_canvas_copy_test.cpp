// Headless export fixture: HWNDs are tokens and all pixels stay in memory DIBs.
#include "../modules/A2FOShellDisplay/canvas.hpp"
#include "../sdk/include/a2fo_shell_canvas.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <stdexcept>

BOOL WINAPI host_GetClientRect(HWND, LPRECT);
#define GetClientRect host_GetClientRect
#include "../modules/A2FOShellDisplay/canvas.cpp"
#undef GetClientRect

namespace canvas = a2fo::shell_display::canvas;
namespace {
constexpr int kWidth = 80, kHeight = 60;
HWND g_window = reinterpret_cast<HWND>(0x1000);
BOOL WINAPI fake_client_rect(HWND window, LPRECT rect) {
    if (window != g_window || !rect) return FALSE;
    *rect = RECT{0, 0, 160, 120};
    return TRUE;
}

class Dib {
    HDC dc_{};
    HBITMAP bitmap_{};
    HGDIOBJ previous_{};
public:
    Dib(int width, int height) {
        dc_ = CreateCompatibleDC(nullptr);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels{};
        bitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        assert(dc_ && bitmap_ && pixels);
        previous_ = SelectObject(dc_, bitmap_);
        assert(previous_ && previous_ != HGDI_ERROR);
    }
    ~Dib() {
        SelectObject(dc_, previous_);
        DeleteObject(bitmap_);
        DeleteDC(dc_);
    }
    HDC dc() const { return dc_; }
};
int g_calls{};
bool g_handled{};
bool g_throw{};
void* g_seen_context{};
BOOL __cdecl present_overlay(HWND window, HDC physical, void* context) {
    assert(window == g_window);
    ++g_calls;
    g_seen_context = context;
    Dib snapshot(kWidth, kHeight);
    int width{}, height{};
    assert(A2FO_CopyShellCanvasV1(window, snapshot.dc(), kWidth, kHeight,
                                  &width, &height));
    assert(width == kWidth && height == kHeight &&
           GetPixel(snapshot.dc(), 7, 9) == RGB(11, 22, 33));
    if (g_throw) throw std::runtime_error("presenter failure");
    if (!g_handled) return FALSE;
    assert(SetPixelV(physical, 7, 9, RGB(99, 88, 77)));
    return TRUE;
}
BOOL __cdecl other_presenter(HWND, HDC, void*) { return TRUE; }
void finish_lease(const std::shared_ptr<canvas::Window>& record, HDC physical) {
    auto shared = std::make_unique<canvas::Surface>();
    HDC logical = shared->share(physical, record->surface);
    assert(logical);
    const int saved = SaveDC(logical);
    assert(saved);
    canvas::g_leases.push_back({record, physical, logical, {}, false, saved,
                                std::move(shared)});
    assert(A2FO_ShellCanvasWindowV1(logical)==g_window);
    canvas::g_mode={};assert(!A2FO_ShellCanvasWindowV1(logical));
    canvas::g_mode={1920,1080,32};
    assert(canvas::finish(g_window, logical, false, nullptr, nullptr));
    assert(!A2FO_ShellCanvasWindowV1(logical));
    assert(canvas::g_leases.empty());
}
}

BOOL WINAPI host_GetClientRect(HWND window, LPRECT rect) {
    return fake_client_rect(window, rect);
}

int main() {
    int width = -1, height = -1;
    auto copy = &A2FO_CopyShellCanvasV1;
    auto set_presenter = &A2FO_SetShellCanvasPresenterV1;
    assert(!A2FO_ShellCanvasWindowV1(nullptr));
    assert(!A2FO_ShellCanvasWindowV1(reinterpret_cast<HDC>(0x2222)));
    assert(!copy(g_window, nullptr, 0, 0, &width, &height));
    assert(!set_presenter(g_window, present_overlay, nullptr));
    assert(width == 0 && height == 0);

    auto record = std::make_shared<canvas::Window>();
    record->handle = g_window;
    record->width = kWidth;
    record->height = kHeight;
    canvas::g_windows[g_window] = record;
    canvas::g_ready.store(true);
    canvas::g_mode = {1920, 1080, 32};
    assert(!copy(g_window, nullptr, 0, 0, &width, &height));
    HDC source = record->surface.get(nullptr, kWidth, kHeight);
    assert(source);
    assert(SetPixelV(source, 7, 9, RGB(11, 22, 33)));
    assert(copy(g_window, nullptr, 0, 0, &width, &height));
    assert(width == kWidth && height == kHeight);
    assert(!copy(reinterpret_cast<HWND>(0x2000), nullptr, 0, 0, &width, &height));
    record->width = kWidth + 1;
    assert(!copy(g_window, nullptr, 0, 0, &width, &height));
    record->width = kWidth;
    assert(!copy(g_window, nullptr, 0, 0, nullptr, &height));
    assert(!copy(g_window, source, kWidth, kHeight, &width, &height));

    Dib small(kWidth - 1, kHeight);
    assert(!copy(g_window, small.dc(), kWidth, kHeight, &width, &height));
    Dib output(kWidth + 4, kHeight + 4);
    assert(!copy(g_window, output.dc(), kWidth - 1, kHeight, &width, &height));
    assert(!copy(g_window, output.dc(), kWidth + 5, kHeight + 4, &width, &height));
    assert(SetPixelV(output.dc(), 7, 9, RGB(1, 2, 3)));
    assert(SetPixelV(output.dc(), kWidth + 1, kHeight + 1, RGB(4, 5, 6)));
    const int map_mode = GetMapMode(output.dc());
    assert(copy(g_window, output.dc(), kWidth + 4, kHeight + 4, &width, &height));
    assert(GetPixel(output.dc(), 7, 9) == RGB(11, 22, 33));
    assert(GetPixel(output.dc(), kWidth + 1, kHeight + 1) == RGB(4, 5, 6));
    assert(GetMapMode(output.dc()) == map_mode);
    assert(GetPixel(source, 7, 9) == RGB(11, 22, 33));
    assert(SetPixelV(output.dc(), 7, 9, RGB(44, 55, 66)));
    assert(GetPixel(source, 7, 9) == RGB(11, 22, 33));

    int context{};
    int other_context{};
    assert(set_presenter(g_window, present_overlay, &context));
    assert(set_presenter(g_window, present_overlay, &context));
    assert(!set_presenter(g_window, present_overlay, &other_context));
    assert(!set_presenter(g_window, other_presenter, &context));
    assert(!set_presenter(g_window, nullptr, &other_context));
    assert(!set_presenter(reinterpret_cast<HWND>(0x2000), present_overlay, &context));

    g_handled = true;
    assert(SetPixelV(output.dc(), 7, 9, RGB(1, 2, 3)));
    finish_lease(record, output.dc());
    assert(g_calls == 1 && g_seen_context == &context);
    assert(GetPixel(output.dc(), 7, 9) == RGB(99, 88, 77));
    assert(GetPixel(source, 7, 9) == RGB(11, 22, 33));

    g_handled = false;
    assert(SetPixelV(output.dc(), 7, 9, RGB(1, 2, 3)));
    finish_lease(record, output.dc());
    assert(g_calls == 2 && GetPixel(output.dc(), 14, 18) == RGB(11, 22, 33));

    g_throw = true;
    assert(SetPixelV(output.dc(), 7, 9, RGB(1, 2, 3)));
    finish_lease(record, output.dc());
    assert(g_calls == 3 && GetPixel(output.dc(), 14, 18) == RGB(11, 22, 33));
    g_throw = false;
    assert(set_presenter(g_window, nullptr, &context));
    assert(set_presenter(g_window, nullptr, nullptr));
    assert(SetPixelV(output.dc(), 7, 9, RGB(1, 2, 3)));
    finish_lease(record, output.dc());
    assert(g_calls == 3 && GetPixel(output.dc(), 14, 18) == RGB(11, 22, 33));

    canvas::g_mode = {};
    assert(!copy(g_window, nullptr, 0, 0, &width, &height));
    assert(!set_presenter(g_window, present_overlay, &context));
    canvas::g_windows.clear();
    canvas::g_ready.store(false);
    std::puts("shell canvas copy export passed");
}
