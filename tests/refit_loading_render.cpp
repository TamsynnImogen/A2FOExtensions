#include "../modules/A2FORefitMenus/loading_presentation.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef _WIN32
#error This offscreen fixture requires Win32 GDI.
#endif

using namespace a2fo::refit_menu;

static void check(bool good, const char* why) {
    if (!good) throw std::runtime_error(why);
}

static void save(const char* path, int width, int height, const std::uint32_t* pixels) {
    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER image{};
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(image);
    file.bfSize = file.bfOffBits + width * height * 4;
    image.biSize = sizeof(image);
    image.biWidth = width;
    image.biHeight = -height;
    image.biPlanes = 1;
    image.biBitCount = 32;
    image.biSizeImage = width * height * 4;
    std::ofstream output(path, std::ios::binary);
    check(bool(output), "opening BMP failed");
    output.write(reinterpret_cast<const char*>(&file), sizeof(file));
    output.write(reinterpret_cast<const char*>(&image), sizeof(image));
    output.write(reinterpret_cast<const char*>(pixels), image.biSizeImage);
    check(bool(output), "writing BMP failed");
}

static void render(const char* path, int width, int height, const char* race,
                   const std::string& theme_root) {
    HDC dc = CreateCompatibleDC(nullptr);
    check(dc != nullptr, "CreateCompatibleDC failed");
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* data = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &data, nullptr, 0);
    check(bitmap && data, "CreateDIBSection failed");
    HGDIOBJ old = SelectObject(dc, bitmap);
    check(old && old != HGDI_ERROR, "SelectObject failed");

    LoadingModel model;
    model.title = "Nebula Crossroads";
    model.mode = "Instant Action";
    model.status = "Preparing ships and stations";
    model.briefing = {
        "A patrol has reached the outer nebula. Investigate the signal and report what lies beyond the sensor horizon.",
        "Keep the ships together while the task force approaches the station."
    };
    model.objectives = {"Explore the nebula", "Keep the outpost secure"};
    model.hint = "The local sensors have limited range.";
    model.players = {{"Player One", "Federation", 0, true, 0.63f},
                     {"Computer", "Random", 1, false, -1.0f}};
    for (int player = 2; player < 8; ++player)
        model.players.emplace_back("Long Player Name " + std::to_string(player + 1),
                                   "Species 8472 Undine faction", player, false, 0.5f);
    model.progress = 0.63f;
    model.time_seconds = 2.4;
    model.departure_shown = true;
    model.kind = LoadingMode::instant_action;
    model.theme = loading_theme({theme_root}, race);
    check(model.theme.form != LoadingForm::shell, "fixture theme did not load");
    Artwork backdrop{4, 4, std::vector<std::uint32_t>(16, 0x00306489u)};
    Artwork preview{2, 2, {0x00e24d43u, 0x003fca72u, 0x003a5ad9u, 0x00e1bd4bu}};
    model.bar_track={width / 20,height - 30,width * 9 / 10,5};
    const Rect native_tip{width == 1280 ? 457 : 685, 76,
                          width == 1280 ? 520 : 780, height - 76};
    model.protected_regions = {native_tip,
        {model.bar_track.x - 4, model.bar_track.y - 4,
         model.bar_track.width + 8, model.bar_track.height + 8},
        {width == 1280 ? 188 : 282, 104, width == 1280 ? 190 : 285, 180}};
    const LoadingLayout layout = loading_layout(width, height, [&] {
        LoadingModel placed = model;
        placed.show_preview = true;
        return placed;
    }());
    check(layout.valid, "layout invalid");
    check(paint_loading(dc, width, height, model, &backdrop, &preview), "paint_loading failed");
    check(GdiFlush() != 0, "GdiFlush failed");
    const auto* pixels = static_cast<const std::uint32_t*>(data);
    const auto sample = [&](int x, int y) { return pixels[static_cast<std::size_t>(y) * width + x] & 0xffffffu; };
    const int cx = native_tip.x + native_tip.width / 2;
    const int cy = native_tip.y + native_tip.height / 2;
    const auto excluded = sample(cx, cy);
    const auto visible = sample(layout.preview.x + 3, layout.preview.y + 3);
    check(excluded != visible, "native tip and preview have same pixels");
    check((pixels[static_cast<std::size_t>(cy) * width + cx] & 0xff000000u) == 0xff000000u,
          "background pixel is not opaque");
    check(sample(layout.preview.x + layout.preview.width - 4,
                 layout.preview.y + layout.preview.height - 4) != visible,
          "preview orientation lost");
    std::vector<std::uint32_t> instant_briefing;
    instant_briefing.reserve(static_cast<std::size_t>(layout.briefing.width) * layout.briefing.height);
    // The Instant Action layout has no mission column; sample the title's lower
    // left region where the same fixture will paint campaign briefing text.
    const Rect mission_area{layout.title.x, layout.title.y + layout.title.height + 40,
                            layout.title.width, std::min(160, height - layout.title.y - layout.title.height - 40)};
    for (int y = mission_area.y; y < mission_area.y + mission_area.height; ++y)
        for (int x = mission_area.x; x < mission_area.x + mission_area.width; ++x)
            instant_briefing.push_back(sample(x, y));
    const auto preview_pixel = sample(layout.preview.x + 3, layout.preview.y + 3);
    save(path, width, height, pixels);
    check(paint_loading(dc, width, height, model, &backdrop, &preview, true),
          "transparent overlay paint failed");
    check(GdiFlush() != 0, "transparent GdiFlush failed");
    check(pixels[static_cast<std::size_t>(cy) * width + cx] == 0,
          "transparent native tip area was not cleared to zero");
    check(sample(layout.players.x + 8, layout.players.y + 8) != 0,
          "transparent overlay lost player panel");
    check(sample(model.bar_track.x + 8, model.bar_track.y + 1) != 0,
          "transparent overlay lost native bar frame");
    check(sample(layout.preview.x + 3, layout.preview.y + 3) == preview_pixel,
          "transparent overlay changed preview pixels");
    model.kind = LoadingMode::campaign;
    model.briefing = {"Mission briefing."};
    model.objectives = {"Defend the outpost", "Explore the nebula"};
    check(paint_loading(dc, width, height, model, &backdrop, &preview), "campaign paint failed");
    check(GdiFlush() != 0, "campaign GdiFlush failed");
    bool briefing_changed = false;
    bool objective_painted = false;
    std::size_t i = 0;
    for (int y = mission_area.y; y < mission_area.y + mission_area.height; ++y)
        for (int x = mission_area.x; x < mission_area.x + mission_area.width; ++x) {
            const bool changed = sample(x, y) != instant_briefing[i++];
            briefing_changed |= changed;
            objective_painted |= changed && y >= mission_area.y + 50;
        }
    check(briefing_changed, "campaign briefing was not painted");
    check(objective_painted, "campaign objectives were not painted");
    check(sample(layout.preview.x + 3, layout.preview.y + 3) != preview_pixel,
          "campaign still painted minimap");
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: refit_loading_render.exe 1280.bmp 1920.bmp\n";
        return 2;
    }
    try {
        const auto root = std::filesystem::temp_directory_path() / "refit_loading_render_theme";
        std::filesystem::create_directories(root / "RefitScene" / "Loading");
        std::filesystem::copy_file("assets/refit-menus/loading/themes.ini",
            root / "RefitScene" / "Loading" / "themes.ini",
            std::filesystem::copy_options::overwrite_existing);
        render(argv[1], 1280, 800, "cardassian", root.string());
        render(argv[2], 1920, 800, "species8472", root.string());
        std::filesystem::remove_all(root);
        std::cout << "refit loading GDI render OK\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
