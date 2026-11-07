// Headless GDI contract for the authored shell canvas. Memory DIBs only.
#include "../modules/A2FOShellDisplay/canvas.hpp"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace {
using a2fo::shell_display::canvas::Scale;
using a2fo::shell_display::canvas::Surface;
constexpr int kLogicalWidth = 1200, kLogicalHeight = 900;
constexpr COLORREF kRed = RGB(255, 0, 0), kGreen = RGB(0, 255, 0);
constexpr COLORREF kBlue = RGB(0, 0, 255), kYellow = RGB(255, 255, 0);
constexpr COLORREF kMagenta = RGB(255, 0, 255), kWhite = RGB(255, 255, 255);

bool check(bool condition, const char* message, int width = 0, int height = 0) {
    if (!condition) std::fprintf(stderr, "FAIL %dx%d: %s\n", width, height, message);
    return condition;
}
std::uint32_t dib_pixel(COLORREF colour) {
    return (static_cast<std::uint32_t>(GetRValue(colour)) << 16) |
           (static_cast<std::uint32_t>(GetGValue(colour)) << 8) |
           static_cast<std::uint32_t>(GetBValue(colour));
}
BITMAPINFO bitmap_info(int width, int height) {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    return info;
}
class MemoryDib {
    HDC dc_{};
    HBITMAP bitmap_{};
    HGDIOBJ previous_{};
public:
    void* pixels{};
    MemoryDib(int width, int height) {
        dc_ = CreateCompatibleDC(nullptr);
        auto info = bitmap_info(width, height);
        bitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (dc_ && bitmap_) previous_ = SelectObject(dc_, bitmap_);
    }
    ~MemoryDib() {
        if (dc_ && previous_) SelectObject(dc_, previous_);
        if (bitmap_) DeleteObject(bitmap_);
        if (dc_) DeleteDC(dc_);
    }
    MemoryDib(const MemoryDib&) = delete;
    MemoryDib& operator=(const MemoryDib&) = delete;
    bool valid() const { return dc_ && bitmap_ && previous_ && previous_ != HGDI_ERROR && pixels; }
    HDC dc() const { return dc_; }
};
void rect(std::vector<std::uint32_t>& pixels, int left, int top, int right, int bottom,
          COLORREF colour) {
    const auto value = dib_pixel(colour);
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
            pixels[static_cast<std::size_t>(y) * kLogicalWidth + x] = value;
}
bool close_enough(int actual, int expected, int distance = 1) {
    return std::abs(actual - expected) <= distance;
}
bool scale_contract(const Scale& scale) {
    const RECT source{100, 50, 101, 51};
    const RECT mapped = scale.bounds(source);
    const RECT adjacent = scale.bounds(RECT{101, 51, 102, 52});
    if (!check(mapped.right == adjacent.left && mapped.bottom == adjacent.top &&
               mapped.left <= mapped.right && mapped.top <= mapped.bottom,
               "adjacent bounds share rounded edges", scale.physical_width, scale.physical_height)) return false;
    const LPARAM physical = MAKELPARAM(static_cast<short>(scale.x(350)),
                                       static_cast<short>(scale.y(240)));
    const LPARAM logical = scale.mouse(physical);
    if (!check(close_enough(static_cast<short>(LOWORD(logical)), 350) &&
               close_enough(static_cast<short>(HIWORD(logical)), 240),
               "rendered control and inverse click agree", scale.physical_width, scale.physical_height)) return false;
    const LPARAM negative = MAKELPARAM(static_cast<short>(scale.x(-17)),
                                       static_cast<short>(scale.y(-9)));
    const LPARAM roundtrip = scale.mouse(negative);
    return check(close_enough(static_cast<short>(LOWORD(roundtrip)), -17) &&
                 close_enough(static_cast<short>(HIWORD(roundtrip)), -9),
                 "negative mouse coordinates retain sign", scale.physical_width, scale.physical_height);
}
bool render_case(Surface& surface, int width, int height) {
    const Scale scale{kLogicalWidth, kLogicalHeight, width, height};
    if (!scale_contract(scale)) return false;
    MemoryDib output(width, height);
    if (!check(output.valid(), "physical memory DIB allocated", width, height)) return false;
    const int original_mode = SetStretchBltMode(output.dc(), BLACKONWHITE);
    if (!check(original_mode != 0 && surface.present(output.dc(), scale),
               "StretchBlt to memory DIB succeeded", width, height)) return false;
    if (!check(GetStretchBltMode(output.dc()) == BLACKONWHITE,
               "present restored output DC stretch mode", width, height)) return false;
    const auto pixel = [&](int x, int y) { return GetPixel(output.dc(), x, y); };
    if (!check(pixel(0, 0) == kRed && pixel(width - 1, 0) == kGreen &&
               pixel(0, height - 1) == kBlue && pixel(width - 1, height - 1) == kYellow,
               "asymmetric corner markers reach physical last row and column", width, height)) return false;
    const int control_x = scale.x(350), control_y = scale.y(240);
    if (!check(pixel(control_x, control_y) == kMagenta,
               "control region follows scale bounds", width, height)) return false;
    if (!check(pixel(scale.x(705), scale.y(505)) == RGB(0, 255, 255),
               "partial repaint survives presentation", width, height)) return false;
    const RECT text_area = scale.bounds(RECT{510, 310, 600, 335});
    bool ink = false;
    for (int y = text_area.top; y < text_area.bottom && !ink; ++y)
        for (int x = text_area.left; x < text_area.right; ++x)
            if (pixel(x, y) != kWhite) { ink = true; break; }
    return check(ink, "GDI text/control region persists through stretch", width, height);
}
bool shared_surface_contract() {
    Surface master;
    HDC master_dc = master.get(nullptr, 64, 48);
    if (!check(master_dc && SetPixelV(master_dc, 2, 2, kRed),
               "shared master starts with a marker")) return false;
    auto first = std::make_unique<Surface>();
    Surface second;
    HDC first_dc = first->share(nullptr, master);
    HDC second_dc = second.share(nullptr, master);
    if (!check(first_dc && second_dc && first_dc != second_dc &&
               first_dc != master_dc && second_dc != master_dc,
               "aliases have distinct logical DCs")) return false;
    GdiFlush();
    if (!check(GetPixel(first_dc, 2, 2) == kRed && GetPixel(second_dc, 2, 2) == kRed,
               "both aliases see the master's original pixels")) return false;
    const int saved = SaveDC(first_dc);
    POINT second_origin{};
    GetViewportOrgEx(second_dc, &second_origin);
    const COLORREF second_text = GetTextColor(second_dc);
    if (!check(saved && SetViewportOrgEx(first_dc, 5, 6, nullptr) &&
               SetTextColor(first_dc, kBlue) != CLR_INVALID,
               "first alias changes its own saved DC state")) return false;
    POINT unchanged{};
    GetViewportOrgEx(second_dc, &unchanged);
    if (!check(unchanged.x == second_origin.x && unchanged.y == second_origin.y &&
               GetTextColor(second_dc) == second_text &&
               SetPixelV(first_dc, 3, 3, kBlue) &&
               SetPixelV(second_dc, 20, 15, kGreen),
               "second alias keeps independent state and both draw")) return false;
    GdiFlush();
    if (!check(GetPixel(master_dc, 8, 9) == kBlue &&
               GetPixel(master_dc, 20, 15) == kGreen &&
               RestoreDC(first_dc, saved),
               "master observes both flushed alias markers")) return false;
    first.reset();
    if (!check(SetPixelV(second_dc, 30, 20, kYellow),
               "second alias remains usable after first is destroyed")) return false;
    GdiFlush();
    if (!check(GetPixel(master_dc, 2, 2) == kRed &&
               GetPixel(master_dc, 8, 9) == kBlue &&
               GetPixel(master_dc, 20, 15) == kGreen &&
               GetPixel(master_dc, 30, 20) == kYellow,
               "destroying first alias retains shared pixels")) return false;
    master.reset();
    GdiFlush();
    if (!check(GetPixel(second_dc, 2, 2) == kRed &&
               GetPixel(second_dc, 8, 9) == kBlue &&
               GetPixel(second_dc, 20, 15) == kGreen &&
               GetPixel(second_dc, 30, 20) == kYellow,
               "alias retains mapping after master reset")) return false;
    Surface successor;
    HDC successor_dc = successor.share(nullptr, second);
    if (!check(successor_dc && successor_dc != second_dc &&
               SetPixelV(successor_dc, 40, 30, kMagenta),
               "surviving alias can share mapping again")) return false;
    GdiFlush();
    return check(GetPixel(second_dc, 40, 30) == kMagenta &&
                 GetPixel(successor_dc, 2, 2) == kRed,
                 "mapping survives chained DuplicateHandle ownership");
}
} // namespace

int main() {
    Surface surface;
    HDC canvas = surface.get(nullptr, kLogicalWidth, kLogicalHeight);
    if (!check(canvas != nullptr, "logical memory canvas allocated")) return 1;
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(kLogicalWidth) * kLogicalHeight,
                                      dib_pixel(kWhite));
    rect(pixels, 0, 0, 8, 8, kRed);
    rect(pixels, kLogicalWidth - 8, 0, kLogicalWidth, 8, kGreen);
    rect(pixels, 0, kLogicalHeight - 8, 8, kLogicalHeight, kBlue);
    rect(pixels, kLogicalWidth - 8, kLogicalHeight - 8,
         kLogicalWidth, kLogicalHeight, kYellow);
    rect(pixels, 300, 200, 401, 281, kMagenta);
    const auto info = bitmap_info(kLogicalWidth, kLogicalHeight);
    if (!check(SetDIBitsToDevice(canvas, 0, 0, kLogicalWidth, kLogicalHeight,
                                  0, 0, 0, kLogicalHeight, pixels.data(), &info,
                                  DIB_RGB_COLORS) == static_cast<unsigned>(kLogicalHeight),
               "native-style SetDIBitsToDevice populated logical canvas")) return 1;
    SetBkMode(canvas, TRANSPARENT);
    SetTextColor(canvas, RGB(0, 0, 0));
    if (!check(TextOutA(canvas, 510, 310, "Options", 7) != FALSE,
               "text painted on logical canvas")) return 1;
    if (!check(surface.get(nullptr, kLogicalWidth, kLogicalHeight) == canvas &&
               GetPixel(canvas, 0, 0) == kRed,
               "repeated get retains canvas pixels")) return 1;
    // A partial native paint must leave previously completed menu regions.
    HBRUSH cyan = CreateSolidBrush(RGB(0, 255, 255));
    const RECT patch{700, 500, 710, 510};
    const bool painted = cyan && FillRect(canvas, &patch, cyan) != 0;
    if (cyan) DeleteObject(cyan);
    if (!check(painted && surface.get(nullptr, kLogicalWidth, kLogicalHeight) == canvas &&
               GetPixel(canvas, 0, 0) == kRed &&
               GetPixel(canvas, 705, 505) == RGB(0, 255, 255),
               "partial repaint preserves prior content")) return 1;
    for (const auto& size : {SIZE{1920, 1080}, SIZE{1600, 900}, SIZE{800, 600}, SIZE{640, 480}})
        if (!render_case(surface, size.cx, size.cy)) return 1;
    const Scale invalid{1200, 900, 0, 600};
    const Scale oversize{1200, 900, 16385, 600};
    MemoryDib small(32, 32);
    if (!check(small.valid() && !invalid.valid() && !oversize.valid() &&
               !surface.present(small.dc(), invalid) && !surface.present(nullptr, Scale{1200,900,800,600}),
               "invalid scale and output DC are rejected")) return 1;
    if (!check(surface.get(nullptr, 4097, 4097) == nullptr &&
               surface.get(nullptr, 0, 1) == nullptr &&
               !surface.present(small.dc(), Scale{1200,900,800,600}),
               "oversized and zero-sized allocations fail safely")) return 1;
    if (!shared_surface_contract()) return 1;
    std::puts("Shell canvas headless GDI test: PASS");
    return 0;
}
