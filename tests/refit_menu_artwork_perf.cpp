#include "../modules/A2FORefitMenus/presentation.hpp"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef _WIN32
#error This fixture requires Win32 GDI.
#endif

int main(int argc, char** argv) {
    if (argc > 2 || (argc == 2 && std::string(argv[1]) != "odd")) {
        std::cerr << "usage: refit_menu_artwork_perf.exe [odd]\n";
        return 2;
    }
    const bool odd = argc == 2;
    const int width = odd ? 257 : 1920;
    const int height = odd ? 127 : 1080;
    for (unsigned light = 0; light <= 100; ++light)
        for (unsigned channel = 0; channel <= 255; ++channel) {
            const unsigned product = channel * light;
            if (product / 100 != ((product * 5243u) >> 19)) {
                std::cerr << "SSE2 quotient mismatch: channel=" << channel
                          << " light=" << light << '\n';
                return 1;
            }
        }
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) {
        std::cerr << "CreateCompatibleDC failed: " << GetLastError() << '\n';
        return 1;
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* data = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &data, nullptr, 0);
    if (!bitmap || !data) {
        std::cerr << "CreateDIBSection failed: " << GetLastError() << '\n';
        return 1;
    }
    HGDIOBJ old = SelectObject(dc, bitmap);
    if (!old || old == HGDI_ERROR) {
        std::cerr << "SelectObject failed: " << GetLastError() << '\n';
        return 1;
    }

    a2fo::refit_menu::Artwork artwork;
    artwork.width = width;
    artwork.height = height;
    artwork.pixels.resize(static_cast<std::size_t>(width) * height);
    for (std::size_t i = 0; i < artwork.pixels.size(); ++i)
        artwork.pixels[i] = static_cast<std::uint32_t>(i * 2654435761u);
    const auto snapshot = a2fo::refit_menu::default_snapshot();
    LARGE_INTEGER frequency{};
    if (!QueryPerformanceFrequency(&frequency)) {
        std::cerr << "QueryPerformanceFrequency failed: " << GetLastError() << '\n';
        return 1;
    }
    std::vector<std::int64_t> times;
    const auto* pixels = static_cast<const std::uint32_t*>(data);
    const auto frame_hash = [&] {
        std::uint64_t hash = 14695981039346656037ull;
        for (std::size_t i = 0; i < artwork.pixels.size(); ++i) {
            hash ^= pixels[i] & 0xffffffu;
            hash *= 1099511628211ull;
        }
        return hash;
    };
    std::uint64_t first_hash = 0;
    for (int iteration = 0; iteration < 52; ++iteration) {
        LARGE_INTEGER start{}, end{};
        QueryPerformanceCounter(&start);
        if (!a2fo::refit_menu::paint(dc, width, height, snapshot, -1, -1, &artwork)) {
            std::cerr << "paint failed at iteration " << iteration
                      << ": GDI error " << GetLastError() << '\n';
            return 1;
        }
        if (!GdiFlush()) {
            std::cerr << "GdiFlush failed at iteration " << iteration
                      << ": GDI error " << GetLastError() << '\n';
            return 1;
        }
        QueryPerformanceCounter(&end);
        if (iteration == 0) first_hash = frame_hash();
        if (iteration > 1) times.push_back(
            (end.QuadPart - start.QuadPart) * 1000000 / frequency.QuadPart);
    }
    std::sort(times.begin(), times.end());
    const std::uint64_t hash = frame_hash();
    const auto geometry = a2fo::refit_menu::layout(width, height, snapshot);
    const int fade_start = geometry.title.x;
    const int fade_end = geometry.title.x + geometry.title.width +
                         std::max(48, static_cast<int>(80 * geometry.scale));
    const int fade_width = std::max(1, fade_end - fade_start);
    for (int x = 0; x < width; ++x) {
        const double blend = std::clamp(static_cast<double>(x - fade_start) /
            fade_width, 0.0, 1.0);
        const int light = 28 + static_cast<int>(72 * blend);
        const auto source = artwork.pixels[static_cast<std::size_t>(x)];
        const auto expected = light == 100 ? source & 0x00ffffffu :
            ((((source >> 16) & 255u) * light / 100) << 16) |
            ((((source >> 8) & 255u) * light / 100) << 8) |
            ((source & 255u) * light / 100);
        if ((pixels[x] & 0x00ffffffu) != expected) {
            std::cerr << "background row mismatch at x=" << x
                      << " actual=" << (pixels[x] & 0x00ffffffu)
                      << " expected=" << expected << '\n';
            return 1;
        }
    }
    if (hash != first_hash) {
        std::cerr << "frame changed between identical paints: first=" << first_hash
                  << " final=" << hash << '\n';
        return 1;
    }
    std::cout << width << 'x' << height << " native artwork: min=" << times.front()
              << "us median=" << times[times.size() / 2]
              << "us max=" << times.back() << "us hash=" << hash
              << " sse2=" << IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE)
              << '\n';
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
}
