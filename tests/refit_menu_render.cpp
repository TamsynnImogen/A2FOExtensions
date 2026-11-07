#include "../modules/A2FORefitMenus/presentation.hpp"

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef _WIN32
#error This fixture requires Win32 GDI.
#endif

namespace {

constexpr int width = 1280;
constexpr int height = 720;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Canvas {
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ original = nullptr;
    std::uint32_t* pixels = nullptr;

    Canvas() {
        try {
            dc = CreateCompatibleDC(nullptr);
            require(dc != nullptr, "CreateCompatibleDC failed");
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width;
            info.bmiHeader.biHeight = -height; // top-down, 32-bit BI_RGB
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            void* data = nullptr;
            bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &data, nullptr, 0);
            require(bitmap != nullptr && data != nullptr, "CreateDIBSection failed");
            pixels = static_cast<std::uint32_t*>(data);
            original = SelectObject(dc, bitmap);
            require(original != nullptr && original != HGDI_ERROR, "SelectObject failed");
        } catch (...) {
            if (bitmap) DeleteObject(bitmap);
            if (dc) DeleteDC(dc);
            throw;
        }
    }

    ~Canvas() {
        if (original && original != HGDI_ERROR) SelectObject(dc, original);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
    }

    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    std::uint32_t at(int x, int y) const {
        return pixels[y * width + x] & 0x00ffffffu;
    }

    void reset() {
        std::fill_n(pixels, width * height, 0x00554433u);
    }
};

void write_bmp(const char* path, const Canvas& canvas) {
    static_assert(sizeof(BITMAPFILEHEADER) == 14);
    static_assert(sizeof(BITMAPINFOHEADER) == 40);
    BITMAPFILEHEADER file_header{};
    BITMAPINFOHEADER info_header{};
    constexpr DWORD pixel_bytes = width * height * 4;
    file_header.bfType = 0x4d42; // BM
    file_header.bfOffBits = sizeof(file_header) + sizeof(info_header);
    file_header.bfSize = file_header.bfOffBits + pixel_bytes;
    info_header.biSize = sizeof(info_header);
    info_header.biWidth = width;
    info_header.biHeight = -height;
    info_header.biPlanes = 1;
    info_header.biBitCount = 32;
    info_header.biCompression = BI_RGB;
    info_header.biSizeImage = pixel_bytes;

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "cannot open output BMP");
    output.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
    output.write(reinterpret_cast<const char*>(&info_header), sizeof(info_header));
    output.write(reinterpret_cast<const char*>(canvas.pixels), pixel_bytes);
    require(static_cast<bool>(output), "BMP write failed");
}

void render_checked(Canvas& canvas, const a2fo::refit_menu::Snapshot& snapshot,
                    int hot, int pressed,
                    const a2fo::refit_menu::Artwork* artwork = nullptr) {
    canvas.reset();
    require(a2fo::refit_menu::paint(canvas.dc, width, height, snapshot, hot,
                                   pressed, artwork),
            "paint reported failure");
    require(GdiFlush() != 0, "GdiFlush failed");
}

a2fo::refit_menu::Artwork load_artwork_bmp(const char* path) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "cannot open input background BMP");
    BITMAPFILEHEADER file_header{};
    BITMAPINFOHEADER source_header{};
    input.read(reinterpret_cast<char*>(&file_header), sizeof(file_header));
    input.read(reinterpret_cast<char*>(&source_header), sizeof(source_header));
    require(static_cast<bool>(input) && file_header.bfType == 0x4d42 &&
            source_header.biSize >= sizeof(BITMAPINFOHEADER) &&
            source_header.biPlanes == 1,
            "invalid input background BMP header");
    const std::int64_t source_height = source_header.biHeight < 0
        ? -static_cast<std::int64_t>(source_header.biHeight)
        : source_header.biHeight;
    require(source_header.biWidth > 0 && source_header.biWidth <= 8192 &&
            source_height > 0 && source_height <= 8192 &&
            static_cast<std::uint64_t>(source_header.biWidth) * source_height <= 8000000,
            "input background BMP exceeds dimension or pixel limits");

    HBITMAP bitmap = static_cast<HBITMAP>(LoadImageA(nullptr, path, IMAGE_BITMAP,
        0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION));
    require(bitmap != nullptr, "LoadImageA could not decode input background BMP");
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) {
        DeleteObject(bitmap);
        throw std::runtime_error("CreateCompatibleDC failed for input background BMP");
    }
    try {
        BITMAP dimensions{};
        require(GetObject(bitmap, sizeof(dimensions), &dimensions) == sizeof(dimensions) &&
                dimensions.bmWidth > 0 && dimensions.bmHeight > 0 &&
                dimensions.bmWidth <= 8192 && dimensions.bmHeight <= 8192 &&
                static_cast<std::uint64_t>(dimensions.bmWidth) *
                    dimensions.bmHeight <= 8000000,
                "decoded input background BMP has invalid dimensions");
        a2fo::refit_menu::Artwork result;
        result.width = dimensions.bmWidth;
        result.height = dimensions.bmHeight;
        result.pixels.resize(static_cast<std::size_t>(result.width) * result.height);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = result.width;
        info.bmiHeader.biHeight = -result.height; // top-down 0x00RRGGBB
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        require(GetDIBits(dc, bitmap, 0, static_cast<UINT>(result.height),
                          result.pixels.data(), &info, DIB_RGB_COLORS) == result.height,
                "GetDIBits failed for input background BMP");
        const bool released_dc = DeleteDC(dc) != 0;
        const bool released_bitmap = DeleteObject(bitmap) != 0;
        require(released_dc && released_bitmap,
                "could not release input background BMP resources");
        return result;
    } catch (...) {
        DeleteDC(dc);
        DeleteObject(bitmap);
        throw;
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        std::cerr << "usage: refit_menu_render.exe output.bmp [input-background.bmp]\n";
        return 2;
    }
    try {
        using namespace a2fo::refit_menu;
        Canvas canvas;
        Snapshot snapshot = default_snapshot();
        require(!paint(nullptr, width, height, snapshot, -1, -1),
                "null HDC should fail");
        require(!paint(canvas.dc, 0, height, snapshot, -1, -1),
                "zero width should fail");
        require(!paint(canvas.dc, width, -1, snapshot, -1, -1),
                "negative height should fail");
        snapshot.entries[static_cast<std::size_t>(Action::single_player)].visible = false;
        snapshot.version = "A2FORefitMenus preview";
        const Layout geometry = layout(width, height, snapshot);
        require(geometry.scale > 0.0f, "layout failed");
        require(geometry.rows[0].width == 0, "hidden row retained geometry");
        const Rect& hot_row = geometry.rows[static_cast<std::size_t>(Action::instant_action)];
        const Rect& normal_row = geometry.rows[static_cast<std::size_t>(Action::multiplayer)];
        const int hot_x = hot_row.x + hot_row.width - 40;
        const int hot_y = hot_row.y + hot_row.height / 2;
        const int normal_x = normal_row.x + normal_row.width - 40;
        const int normal_y = normal_row.y + normal_row.height / 2;

        render_checked(canvas, snapshot, -1, -1);
        const std::uint32_t idle_hot = canvas.at(hot_x, hot_y);
        const std::uint32_t idle_normal = canvas.at(normal_x, normal_y);
        const Rect& editor = geometry.rows[static_cast<std::size_t>(Action::map_editor)];
        std::vector<std::uint32_t> editor_pixels;
        for (int y = editor.y; y < editor.y + editor.height; ++y)
            for (int x = editor.x; x < editor.x + editor.width; ++x)
                editor_pixels.push_back(canvas.at(x, y));
        snapshot.entries[static_cast<std::size_t>(Action::map_editor)].caption = "Map Editor";
        render_checked(canvas, snapshot, -1, -1);
        std::size_t pixel = 0;
        for (int y = editor.y; y < editor.y + editor.height; ++y)
            for (int x = editor.x; x < editor.x + editor.width; ++x)
                require(canvas.at(x, y) == editor_pixels[pixel++],
                        "mixed-case native caption did not display uppercase");
        require(snapshot.entries[static_cast<std::size_t>(Action::map_editor)].caption ==
                "Map Editor", "display uppercasing changed native caption");
        require(canvas.at(normal_row.x + normal_row.width - 6, normal_y) ==
                canvas.at(normal_row.x + normal_row.width + 6, normal_y),
                "idle row has a button fill or border");
        const int rule_y = geometry.title.y + static_cast<int>(74 * geometry.scale);
        require(canvas.at(geometry.title.x, rule_y) == 0xf2a93bu,
                "orange title rule missing");
        // paint() checks GetTextFaceW for each actual selected face. Private
        // memory fonts need not appear in a Windows font enumeration.

        render_checked(canvas, snapshot, static_cast<int>(Action::instant_action), -1);
        require(canvas.at(0, 0) == 0x03060cu, "top background token mismatch");
        require(canvas.at(width - 1, height - 1) == 0x0a1422u,
                "bottom background token mismatch");
        require(canvas.at(hot_row.x + 1, hot_y) == 0xf2a93bu,
                "hover accent bar missing");
        require(canvas.at(hot_x, hot_y) != idle_hot, "hover fade missing");
        require(canvas.at(normal_x, normal_y) == idle_normal,
                "normal row changed under hover");
        bool title_changed = false;
        for (int y = geometry.title.y; y < geometry.title.y + geometry.title.height; ++y)
            for (int x = geometry.title.x;
                 x < geometry.title.x + geometry.title.width; ++x)
                title_changed |= canvas.at(x, y) != canvas.at(width - 1, y);
        require(title_changed, "title did not paint");
        write_bmp(argv[1], canvas);

        render_checked(canvas, snapshot, static_cast<int>(Action::instant_action),
                       static_cast<int>(Action::instant_action));
        require(canvas.at(hot_x, hot_y) != idle_hot,
                "pressed row lost its active state");
        snapshot.entries[static_cast<std::size_t>(Action::instant_action)].enabled = false;
        render_checked(canvas, snapshot, static_cast<int>(Action::instant_action), -1);
        require(canvas.at(hot_x, hot_y) == idle_hot,
                "disabled row retained hover fade");
        require(canvas.at(hot_row.x + 1, hot_y) != 0xf2a93bu,
                "disabled row retained accent bar");

        Artwork artwork;
        artwork.width = 320;
        artwork.height = 180;
        artwork.pixels.resize(320 * 180, 0x00ffffffu);
        for (int y = 0; y < artwork.height; ++y)
            for (int x = 200; x < artwork.width; ++x)
                if ((x / 20) % 2 != 0)
                    artwork.pixels[y * artwork.width + x] = 0x00808080u;
        render_checked(canvas, snapshot, -1, -1, &artwork);
        require(canvas.at(0, 200) == 0x474747u,
                "artwork left contrast gradient mismatch");
        require(canvas.at(1000, 200) == 0xffffffu,
                "artwork right visibility mismatch");
        require(canvas.at(900, 200) != canvas.at(1000, 200),
                "artwork right-side pattern disappeared");
        const std::uint32_t edge = canvas.at(879, 200) & 0xffu;
        require(edge > 128 && edge < 255,
                "artwork edge did not interpolate between source pixels");
        require(canvas.at(normal_row.x + normal_row.width - 6, normal_y) ==
                canvas.at(normal_row.x + normal_row.width - 6, normal_row.y - 3),
                "artwork row gained a button fill");
        bool artwork_title_changed = false;
        for (int y = geometry.title.y; y < geometry.title.y + geometry.title.height; ++y)
            for (int x = geometry.title.x;
                 x < geometry.title.x + geometry.title.width; ++x)
                artwork_title_changed |= canvas.at(x, y) != 0x474747u;
        require(artwork_title_changed, "artwork title did not paint");

        Artwork native_art{width, height, std::vector<std::uint32_t>(width * height, 0x00204060u)};
        native_art.pixels[200 * width + 1000] = 0x00ffffffu;
        native_art.pixels[200 * width + 1001] = 0;
        render_checked(canvas, snapshot, -1, -1, &native_art);
        require(canvas.at(1000, 200) == 0x00ffffffu && canvas.at(1001, 200) == 0,
                "native-resolution artwork lost adjacent pixel detail");

        Artwork wide_art;
        wide_art.width = 800;
        wide_art.height = 100;
        wide_art.pixels.resize(800 * 100);
        for (int y = 0; y < wide_art.height; ++y)
            for (int x = 0; x < wide_art.width; ++x)
                wide_art.pixels[y * wide_art.width + x] =
                    x < 400 ? 0x00ff0000u : 0x000000ffu;
        render_checked(canvas, snapshot, -1, -1, &wide_art);
        require((canvas.at(0, 200) & 0xff0000u) != 0 &&
                (canvas.at(width - 1, 200) & 0x0000ffu) != 0,
                "wide artwork center crop failed");

        Artwork portrait_art;
        portrait_art.width = 100;
        portrait_art.height = 320;
        portrait_art.pixels.resize(100 * 320);
        for (int y = 0; y < portrait_art.height; ++y)
            for (int x = 0; x < portrait_art.width; ++x)
                portrait_art.pixels[y * portrait_art.width + x] =
                    y < 160 ? 0x00ff0000u : 0x000000ffu;
        render_checked(canvas, snapshot, -1, -1, &portrait_art);
        require((canvas.at(width - 1, 0) & 0xff0000u) != 0 &&
                (canvas.at(width - 1, height - 1) & 0x0000ffu) != 0,
                "portrait artwork center crop failed");

        Artwork invalid_art;
        invalid_art.width = 4;
        invalid_art.height = 4;
        invalid_art.pixels.resize(15);
        render_checked(canvas, snapshot, -1, -1, &invalid_art);
        require(canvas.at(0, 0) == 0x03060cu &&
                canvas.at(width - 1, height - 1) == 0x0a1422u,
                "invalid artwork did not use gradient fallback");
        if (argc == 3) {
            Artwork input_artwork = load_artwork_bmp(argv[2]);
            Snapshot preview = default_snapshot();
            render_checked(canvas, preview, -1, -1, &input_artwork);
            write_bmp(argv[1], canvas);
        }
        std::cout << "GDI render checks passed; BMP: " << argv[1] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "GDI render fixture failed: " << error.what() << '\n';
        return 1;
    }
}
