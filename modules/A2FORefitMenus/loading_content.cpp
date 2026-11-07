#include "loading_content.hpp"
#include "../../rust/refit_present_ffi.h"

// Copyright (c) 2026 Cedric Haase (original MIT objective-reading algorithm).
// Adapted from armada2-refit ui/src/loading.rs::read_objectives; permission notice
// retained in third_party/armada2-refit/LICENSE.txt. Local bounded reader below.
// File bytes remain in the game's ANSI encoding.
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#endif

namespace a2fo::refit_menu {
namespace {
constexpr std::size_t max_objectives_bytes = 64 * 1024;
constexpr std::uint64_t max_pixels = 8'000'000;
constexpr std::uint64_t max_bmp_bytes = 64 * 1024 * 1024;


bool read_text(const std::string& filename, std::string& out) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto size = file.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(max_objectives_bytes)) return false;
    out.resize(static_cast<std::size_t>(size));
    file.seekg(0);
    return out.empty() || static_cast<bool>(file.read(out.data(), size));
}
#ifndef _WIN32
bool read_bmp(const std::string& filename, Artwork& out) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto length = file.tellg();
    if (length < 54 || static_cast<std::uint64_t>(length) > max_bmp_bytes) return false;
    std::string bytes(static_cast<std::size_t>(length), '\0');
    file.seekg(0);
    if (!file.read(bytes.data(), length)) return false;
    return decode_loading_bmp(bytes, out);
}
#endif
#ifdef _WIN32
bool read_image(const std::string& filename, Artwork& out) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto length = file.tellg();
    if (length <= 0 || static_cast<std::uint64_t>(length) > max_bmp_bytes) return false;
    file.close();

    const int wide_length = MultiByteToWideChar(CP_ACP, 0, filename.c_str(), -1, nullptr, 0);
    if (wide_length <= 0) return false;
    std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
    if (!MultiByteToWideChar(CP_ACP, 0, filename.c_str(), -1, wide.data(), wide_length)) return false;

    struct GdiplusSession {
        ULONG_PTR token = 0;
        GdiplusSession() {
            Gdiplus::GdiplusStartupInput input;
            if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) token = 0;
        }
        ~GdiplusSession() { if (token) Gdiplus::GdiplusShutdown(token); }
    } session;
    if (!session.token) return false;

    Gdiplus::Bitmap bitmap(wide.c_str());
    if (bitmap.GetLastStatus() != Gdiplus::Ok) return false;
    const auto width = bitmap.GetWidth(), height = bitmap.GetHeight();
    if (!width || width > 3840 || !height || height > 2160 ||
        static_cast<std::uint64_t>(width) * height > max_pixels) return false;

    Gdiplus::Rect rect(0, 0, static_cast<INT>(width), static_cast<INT>(height));
    Gdiplus::BitmapData data{};
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Gdiplus::Ok)
        return false;
    Artwork image;
    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    image.pixels.resize(static_cast<std::size_t>(width) * height);
    for (unsigned y = 0; y < height; ++y) {
        const auto* row = static_cast<const std::uint8_t*>(data.Scan0) +
                          static_cast<std::ptrdiff_t>(y) * data.Stride;
        for (unsigned x = 0; x < width; ++x) {
            const auto* pixel = row + static_cast<std::size_t>(x) * 4;
            image.pixels[static_cast<std::size_t>(y) * width + x] =
                (static_cast<std::uint32_t>(pixel[2]) << 16) |
                (static_cast<std::uint32_t>(pixel[1]) << 8) | pixel[0];
        }
    }
    if (bitmap.UnlockBits(&data) != Gdiplus::Ok) return false;
    out = std::move(image);
    return true;
}
#else
bool read_image(const std::string& filename, Artwork& out) {
    return read_bmp(filename, out);
}
#endif


} // namespace

bool decode_loading_bmp(std::string_view bytes, Artwork& out) {
    int width = 0, height = 0;
    const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
    if (!a2fo_refit_decode_bmp(data, bytes.size(), nullptr, 0, &width, &height)) return false;
    Artwork image;
    image.width = width;
    image.height = height;
    image.pixels.resize(static_cast<std::size_t>(width) * height);
    if (!a2fo_refit_decode_bmp(data, bytes.size(), image.pixels.data(),
                               image.pixels.size(), &width, &height)) return false;
    out = std::move(image);
    return true;
}

bool parse_loading_objectives(std::string_view text, LoadingModel& model) {
    struct Parsed {
        std::string title, hint;
        std::vector<std::string> briefing, objectives;
        bool failed = false;
    } parsed;
    const auto sink = [](void* context, int field, const std::uint8_t* bytes,
                         std::size_t length) noexcept -> int {
        auto& result = *static_cast<Parsed*>(context);
        try {
            const std::string value(length ? reinterpret_cast<const char*>(bytes) : "", length);
            switch (field) {
            case 0: result.title = value; break;
            case 1: result.briefing.push_back(value); break;
            case 2: result.objectives.push_back(value); break;
            case 3: result.hint = value; break;
            default: result.failed = true; return 0;
            }
            return 1;
        } catch (...) { result.failed = true; return 0; }
    };
    if (!a2fo_refit_parse_objectives(reinterpret_cast<const std::uint8_t*>(text.data()),
                                     text.size(), sink, &parsed) || parsed.failed) return false;
    model.title = std::move(parsed.title);
    model.briefing = std::move(parsed.briefing);
    model.objectives = std::move(parsed.objectives);
    model.hint = std::move(parsed.hint);
    return true;
}

std::string sanitize_loading_map_name(std::string_view input) {
    std::array<std::uint8_t, 128> output{};
    const auto size = a2fo_refit_sanitize_map(
        reinterpret_cast<const std::uint8_t*>(input.data()), input.size(),
        output.data(), output.size());
    return std::string(reinterpret_cast<const char*>(output.data()), size);
}

bool load_loading_content(const std::vector<std::string>& roots, std::string_view map,
                          LoadingModel& model, Artwork& preview, Artwork& background) {
    preview = {};
    background = {};
    model.show_preview = false;
    struct Context { LoadingModel& model; Artwork& preview; Artwork& background; } context{model, preview, background};
    std::vector<A2FORefitBytes> views;
    views.reserve(roots.size());
    for (const auto& root : roots)
        views.push_back({reinterpret_cast<const std::uint8_t*>(root.data()), root.size()});
    const auto read = [](void* raw, int kind, const std::uint8_t* path,
                         std::size_t length) noexcept -> int {
        try {
            auto& context = *static_cast<Context*>(raw);
            const std::string filename(reinterpret_cast<const char*>(path), length);
            if (kind == 0) {
                std::string content;
                return read_text(filename, content) &&
                       parse_loading_objectives(content, context.model);
            }
            if (kind == 1) return read_image(filename, context.preview);
            if (kind == 2) return read_image(filename, context.background);
            return 0;
        } catch (...) { return 0; }
    };
    int show_preview = 0;
#ifdef _WIN32
    constexpr int windows_images = 1;
#else
    constexpr int windows_images = 0;
#endif
    const bool loaded = a2fo_refit_load_content(views.data(), views.size(),
        reinterpret_cast<const std::uint8_t*>(map.data()), map.size(),
        windows_images, read, &context, &show_preview) != 0;
    model.show_preview = show_preview != 0;
    return loaded;
}
} // namespace a2fo::refit_menu
