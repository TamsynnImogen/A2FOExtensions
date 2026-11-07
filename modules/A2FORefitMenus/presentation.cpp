#include "presentation.hpp"
#include "../../rust/refit_present_ffi.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <limits>
#include <vector>

#if defined(_WIN32) && defined(__GNUC__)
#include <emmintrin.h>
#endif

namespace a2fo::refit_menu {

bool Rect::contains(int px, int py) const {
    return width > 0 && height > 0 && px >= x && py >= y &&
           static_cast<long long>(px) < static_cast<long long>(x) + width &&
           static_cast<long long>(py) < static_cast<long long>(y) + height;
}

Snapshot default_snapshot() {
    Snapshot snapshot;
    snapshot.entries = {{{"SINGLE PLAYER"}, {"INSTANT ACTION"}, {"MULTIPLAYER"},
                         {"OPTIONS"}, {"MAP EDITOR"}, {"CREDITS"}, {"EXIT"}}};
    return snapshot;
}

namespace {
std::array<A2FORefitMenuEntry, entry_count> ffi_entries(const Snapshot& snapshot) {
    std::array<A2FORefitMenuEntry, entry_count> entries{};
    for (std::size_t i = 0; i < entry_count; ++i) {
        const auto& entry = snapshot.entries[i];
        entries[i] = {{reinterpret_cast<const std::uint8_t*>(entry.caption.data()),
                       entry.caption.size()},
                      static_cast<std::uint8_t>(entry.visible),
                      static_cast<std::uint8_t>(entry.enabled)};
    }
    return entries;
}
A2FORefitMenuLayout ffi_layout(const Layout& source) {
    const auto convert = [](const Rect& r) {
        return A2FORefitRect{r.x, r.y, r.width, r.height};
    };
    A2FORefitMenuLayout out{};
    out.title = convert(source.title);
    for (std::size_t i = 0; i < entry_count; ++i)
        out.rows[i] = convert(source.rows[i]);
    out.footer = convert(source.footer);
    out.scale = source.scale;
    return out;
}
} // namespace

Layout layout(int width, int height, const Snapshot& snapshot) {
    const auto entries = ffi_entries(snapshot);
    A2FORefitMenuLayout raw{};
    a2fo_refit_menu_layout(width, height, entries.data(), &raw);
    Layout result;
    const auto convert = [](A2FORefitRect r) {
        return Rect{r.x, r.y, r.width, r.height};
    };
    result.title = convert(raw.title);
    for (std::size_t i = 0; i < entry_count; ++i)
        result.rows[i] = convert(raw.rows[i]);
    result.footer = convert(raw.footer);
    result.scale = raw.scale;
    return result;
}

int hit_test(const Layout& geometry, const Snapshot& snapshot, int x, int y) {
    const auto entries = ffi_entries(snapshot);
    const auto raw = ffi_layout(geometry);
    return a2fo_refit_menu_hit(&raw, entries.data(), x, y);
}

int next_enabled(const Snapshot& snapshot, int current, int direction) {
    const auto entries = ffi_entries(snapshot);
    return a2fo_refit_menu_next(entries.data(), current, direction);
}

#ifdef _WIN32
namespace {

constexpr int medium_font_id = 101;
constexpr int semibold_font_id = 102;
constexpr wchar_t medium_face[] = L"Noto Sans Condensed Medium";
constexpr wchar_t semibold_face[] = L"Noto Sans Condensed SemiBold";

bool register_fonts() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&register_fonts), &module)) return false;
    for (int id : {medium_font_id, semibold_font_id}) {
        HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
        if (!resource) return false;
        HGLOBAL loaded = LoadResource(module, resource);
        if (!loaded) return false;
        void* bytes = LockResource(loaded);
        const DWORD size = SizeofResource(module, resource);
        DWORD count = 0;
        if (!bytes || !size || !AddFontMemResourceEx(bytes, size, nullptr, &count) || !count)
            return false;
    }
    return true;
}

bool fonts_ready() {
    static const bool ready = register_fonts();
    return ready;
}

COLORREF rgb(unsigned color) {
    return RGB((color >> 16) & 0xffu, (color >> 8) & 0xffu, color & 0xffu);
}

bool fill(HDC dc, const RECT& rect, unsigned color) {
    HBRUSH brush = CreateSolidBrush(rgb(color));
    if (!brush) return false;
    const bool drawn = FillRect(dc, &rect, brush) != 0;
    const bool released = DeleteObject(brush) != 0;
    return drawn && released;
}

RECT as_win32(const Rect& rect) {
    return {rect.x, rect.y, rect.x + rect.width, rect.y + rect.height};
}

bool wide(const std::string& ansi, std::wstring& result) {
    result.clear();
    if (ansi.empty()) return true;
    if (ansi.size() > static_cast<std::size_t>(INT_MAX)) return false;
    // Native Delphi AnsiString captions use the process ANSI code page.
    const int length = MultiByteToWideChar(CP_ACP, 0,
                                          ansi.data(), static_cast<int>(ansi.size()),
                                          nullptr, 0);
    if (length <= 0) return false;
    result.resize(static_cast<std::size_t>(length));
    return MultiByteToWideChar(CP_ACP, 0, ansi.data(),
                               static_cast<int>(ansi.size()), result.data(),
                               length) == length;
}

bool uppercase(std::wstring& text) {
    if (text.empty()) return true;
    const int size = LCMapStringW(LOCALE_USER_DEFAULT, LCMAP_UPPERCASE,
                                  text.data(), static_cast<int>(text.size()),
                                  nullptr, 0);
    if (size <= 0) return false;
    std::wstring converted(static_cast<std::size_t>(size), L'\0');
    if (LCMapStringW(LOCALE_USER_DEFAULT, LCMAP_UPPERCASE,
                     text.data(), static_cast<int>(text.size()),
                     converted.data(), size) != size) return false;
    text.swap(converted);
    return true;
}

bool label(HDC dc, const std::wstring& text, RECT rect, int points,
           unsigned color, UINT align, int weight = FW_SEMIBOLD) {
    if (text.empty()) return true;
    const wchar_t* face = weight == FW_MEDIUM ? medium_face : semibold_face;
    HFONT font = CreateFontW(-std::max(1, points), 0, 0, 0, weight,
                             FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, face);
    if (!font) return false;
    HGDIOBJ old_font = SelectObject(dc, font);
    if (!old_font || old_font == HGDI_ERROR) {
        DeleteObject(font);
        return false;
    }
    wchar_t actual[LF_FACESIZE]{};
    const bool state = GetTextFaceW(dc, LF_FACESIZE, actual) > 0 &&
                       _wcsicmp(actual, face) == 0 &&
                       SetBkMode(dc, TRANSPARENT) != 0 &&
                       SetTextColor(dc, rgb(color)) != CLR_INVALID;
    const bool drawn = state && DrawTextW(dc, text.c_str(),
        static_cast<int>(text.size()), &rect,
        align | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX) != 0;
    const HGDIOBJ selected_font = SelectObject(dc, old_font);
    const bool restored = selected_font && selected_font != HGDI_ERROR;
    const bool released = DeleteObject(font) != 0;
    return drawn && restored && released;
}

// Native operations are deliberately synchronous: Rust chooses their order and
// owns generated pixels; this thunk retains GDI handles and ANSI conversion.
int menu_draw(void* raw, int op, A2FORefitRect box, std::uint32_t color,
              int size, int flags, const std::uint8_t* bytes, std::size_t length,
              const std::uint32_t* pixels, std::size_t count) noexcept {
    try {
        HDC dc = static_cast<HDC>(raw);
        const Rect r{box.x, box.y, box.width, box.height};
        if (op == 0) return fill(dc, as_win32(r), color);
        if (op == 1) {
            if (length && !bytes) return false;
            std::wstring value;
            const std::string ansi(length ? reinterpret_cast<const char*>(bytes) : "", length);
            if (!wide(ansi, value) || ((flags & 4) && !uppercase(value))) return false;
            return label(dc, value, as_win32(r), size, color,
                         (flags & 1) ? DT_RIGHT : DT_LEFT,
                         (flags & 2) ? FW_MEDIUM : FW_SEMIBOLD);
        }
        if ((op != 2 && op != 3) || !pixels || r.width <= 0 || r.height <= 0 ||
            static_cast<std::uint64_t>(r.width) * r.height != count) return false;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = r.width;
        info.bmiHeader.biHeight = -r.height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        if (op == 2)
            return SetDIBitsToDevice(dc, r.x, r.y, r.width, r.height, 0, 0, 0,
                                     static_cast<UINT>(r.height), pixels, &info,
                                     DIB_RGB_COLORS) == r.height;
        HDC source = CreateCompatibleDC(dc);
        if (!source) return false;
        void* target_bytes = nullptr;
        HBITMAP bitmap = CreateDIBSection(source, &info, DIB_RGB_COLORS,
                                          &target_bytes, nullptr, 0);
        if (!bitmap || !target_bytes) {
            if (bitmap) DeleteObject(bitmap);
            DeleteDC(source);
            return false;
        }
        std::copy_n(pixels, count, static_cast<std::uint32_t*>(target_bytes));
        HGDIOBJ old = SelectObject(source, bitmap);
        const BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        const bool drawn = old && old != HGDI_ERROR &&
            AlphaBlend(dc, r.x, r.y, r.width, r.height, source, 0, 0,
                       r.width, r.height, blend) != 0;
        if (old && old != HGDI_ERROR) SelectObject(source, old);
        const bool released = DeleteObject(bitmap) && DeleteDC(source);
        return drawn && released;
    } catch (...) { return false; }
}

bool paint_canvas(HDC dc, int width, int height, const Snapshot& snapshot,
                  int hot, int pressed, const Artwork* artwork) {
    if (layout(width, height, snapshot).scale <= 0.0f) return false;
    if (!fonts_ready()) return false;
    const auto entries = ffi_entries(snapshot);
    const A2FORefitArtwork art{artwork ? artwork->width : 0,
        artwork ? artwork->height : 0,
        artwork ? artwork->pixels.data() : nullptr,
        artwork ? artwork->pixels.size() : 0};
    return a2fo_refit_paint_menu(width, height, entries.data(),
        {reinterpret_cast<const std::uint8_t*>(snapshot.version.data()),
         snapshot.version.size()}, hot, pressed, art,
        IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE) != 0,
        menu_draw, dc) != 0;
}

struct Offscreen {
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ old = nullptr;

    bool finish() {
        bool okay = true;
        if (old && old != HGDI_ERROR) {
            const HGDIOBJ selected = SelectObject(dc, old);
            if (!selected || selected == HGDI_ERROR) okay = false;
            else old = nullptr;
        }
        if (bitmap && !old) {
            if (DeleteObject(bitmap)) bitmap = nullptr;
            else okay = false;
        }
        if (dc && !old && !bitmap) {
            if (DeleteDC(dc)) dc = nullptr;
            else okay = false;
        }
        return okay;
    }

    ~Offscreen() { finish(); }
};

} // namespace

bool paint(HDC target, int width, int height, const Snapshot& snapshot,
           int hot, int pressed, const Artwork* artwork) {
    if (!target || width <= 0 || height <= 0) return false;
    try {
        Offscreen buffer;
        buffer.dc = CreateCompatibleDC(target);
        if (!buffer.dc) return false;
        buffer.bitmap = CreateCompatibleBitmap(target, width, height);
        if (!buffer.bitmap) return false;
        buffer.old = SelectObject(buffer.dc, buffer.bitmap);
        if (!buffer.old || buffer.old == HGDI_ERROR) {
            buffer.old = nullptr;
            return false;
        }
        const bool rendered = paint_canvas(buffer.dc, width, height,
                                           snapshot, hot, pressed, artwork);
        const bool blitted = rendered &&
            BitBlt(target, 0, 0, width, height, buffer.dc, 0, 0, SRCCOPY) != 0;
        const bool released = buffer.finish();
        return rendered && blitted && released;
    } catch (...) {
        return false;
    }
}
#endif

} // namespace a2fo::refit_menu
