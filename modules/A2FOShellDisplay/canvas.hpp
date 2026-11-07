#pragma once
#include "policy.hpp"
#include "../../sdk/include/a2fo_module_api.h"
#include <windows.h>

namespace a2fo::shell_display::canvas {
// Native dialogs retain their authored coordinates. Only their HWNDs and
// completed GDI canvas cross this boundary into physical window coordinates.
struct Scale {
    int logical_width{}, logical_height{}, physical_width{}, physical_height{};
    bool valid() const noexcept {
        return logical_width > 0 && logical_height > 0 && physical_width > 0 && physical_height > 0 &&
            logical_width <= 16384 && logical_height <= 16384 && physical_width <= 16384 && physical_height <= 16384;
    }
    int x(int value) const noexcept { return MulDiv(value, physical_width, logical_width); }
    int y(int value) const noexcept { return MulDiv(value, physical_height, logical_height); }
    int logical_x(int value) const noexcept { return MulDiv(value, logical_width, physical_width); }
    int logical_y(int value) const noexcept { return MulDiv(value, logical_height, physical_height); }
    RECT bounds(RECT value) const noexcept { return {x(value.left), y(value.top), x(value.right), y(value.bottom)}; }
    RECT logical_bounds(RECT value, int guard = 0) const noexcept {
        if (!valid()) return {};
        // Dirty device pixels must cover complete logical pixels, including
        // rounded stretch edges. Client paint/update rectangles are nonnegative.
        const auto edge = [](int v, int physical, int logical, bool upper) {
            if (v < 0) v = 0;
            if (v > physical) v = physical;
            return static_cast<int>((static_cast<long long>(v) * logical +
                (upper ? physical - 1 : 0)) / physical);
        };
        RECT result{edge(value.left, physical_width, logical_width, false),
                    edge(value.top, physical_height, logical_height, false),
                    edge(value.right, physical_width, logical_width, true),
                    edge(value.bottom, physical_height, logical_height, true)};
        if (result.right > result.left && result.bottom > result.top && guard > 0) {
            result.left = result.left > guard ? result.left - guard : 0;
            result.top = result.top > guard ? result.top - guard : 0;
            result.right = result.right < logical_width - guard ? result.right + guard : logical_width;
            result.bottom = result.bottom < logical_height - guard ? result.bottom + guard : logical_height;
        }
        return result;
    }
    LPARAM mouse(LPARAM value) const noexcept {
        return MAKELPARAM(static_cast<short>(logical_x(static_cast<short>(LOWORD(value)))),
                          static_cast<short>(logical_y(static_cast<short>(HIWORD(value)))));
    }
};

// A persistent backing canvas preserves partial paints. COLORONCOLOR keeps
// bitmap colour keys and native masks crisp; hit testing uses logical pixels.
class Surface {
    HDC dc_{};
    HBITMAP bitmap_{};
    HGDIOBJ previous_{};
    HANDLE mapping_{};
    int width_{}, height_{};
public:
    Surface() = default;
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    ~Surface() { reset(); }
    HDC cached(int width, int height) const noexcept {
        return width == width_ && height == height_ ? dc_ : nullptr;
    }
    void reset() noexcept {
        if (dc_ && previous_) SelectObject(dc_, previous_);
        if (bitmap_) DeleteObject(bitmap_);
        if (dc_) DeleteDC(dc_);
        if (mapping_) CloseHandle(mapping_);
        dc_ = nullptr; bitmap_ = nullptr; previous_ = nullptr; width_ = height_ = 0;
        mapping_ = nullptr;
    }
    HDC get(HDC compatible, int width, int height) noexcept {
        if (width == width_ && height == height_ && dc_) return dc_;
        reset();
        // Limit an individual canvas to 64 MiB. Typical authored menus are
        // 800x600 or 1200x900, irrespective of the selected output resolution.
        if (width <= 0 || height <= 0 || static_cast<long long>(width) * height > 16777216) return nullptr;
        dc_ = CreateCompatibleDC(compatible);
        mapping_ = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
            static_cast<DWORD>(static_cast<long long>(width) * height * 4), nullptr);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        void* pixels{};
        if (mapping_) bitmap_ = CreateDIBSection(compatible, &info, DIB_RGB_COLORS, &pixels, mapping_, 0);
        if (!dc_ || !bitmap_) { reset(); return nullptr; }
        previous_ = SelectObject(dc_, bitmap_);
        if (!previous_ || previous_ == HGDI_ERROR) { previous_ = nullptr; reset(); return nullptr; }
        width_ = width; height_ = height;
        PatBlt(dc_, 0, 0, width, height, BLACKNESS);
        return dc_;
    }
    HDC share(HDC compatible, const Surface& source) noexcept {
        reset();
        GdiFlush();
        if (!source.mapping_ || !DuplicateHandle(GetCurrentProcess(), source.mapping_, GetCurrentProcess(),
            &mapping_, 0, FALSE, DUPLICATE_SAME_ACCESS)) return nullptr;
        dc_ = CreateCompatibleDC(compatible);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = source.width_; info.bmiHeader.biHeight = -source.height_;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        void* pixels{};
        bitmap_ = CreateDIBSection(compatible, &info, DIB_RGB_COLORS, &pixels, mapping_, 0);
        if (!dc_ || !bitmap_) { reset(); return nullptr; }
        previous_ = SelectObject(dc_, bitmap_);
        if (!previous_ || previous_ == HGDI_ERROR) { previous_ = nullptr; reset(); return nullptr; }
        width_ = source.width_; height_ = source.height_;
        return dc_;
    }
    bool present(HDC output, const Scale& scale) noexcept {
        if (!dc_ || !output || !scale.valid()) return false;
        GdiFlush();
        const int saved = SaveDC(output);
        if (!saved) return false;
        SetStretchBltMode(output, COLORONCOLOR);
        const bool result = StretchBlt(output, 0, 0, scale.physical_width, scale.physical_height,
            dc_, 0, 0, scale.logical_width, scale.logical_height, SRCCOPY) != FALSE;
        RestoreDC(output, saved);
        return result;
    }
};

bool initialize(const A2FO_ModuleApi* api, HMODULE armada, HMODULE fleetops);
void active_mode(const Mode* mode);
void shutdown();
} // namespace a2fo::shell_display::canvas
