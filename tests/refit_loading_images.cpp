#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../modules/A2FORefitMenus/loading_content.hpp"

using namespace a2fo::refit_menu;
namespace fs = std::filesystem;

static void write_bytes(const fs::path& file, const std::string& bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    assert(out);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    assert(out);
}
static void word(std::string& bytes, std::size_t offset, std::uint32_t value, int count) {
    for (int i = 0; i < count; ++i) bytes[offset + i] = static_cast<char>(value >> (i * 8));
}
static std::string bmp() {
    std::string bytes(54 + 16, '\0');
    bytes[0] = 'B'; bytes[1] = 'M';
    word(bytes, 2, bytes.size(), 4); word(bytes, 10, 54, 4); word(bytes, 14, 40, 4);
    word(bytes, 18, 2, 4); word(bytes, 22, 2, 4); word(bytes, 26, 1, 2);
    word(bytes, 28, 24, 2); word(bytes, 34, 16, 4);
    // Bottom green, top blue. The PNG's top-left pixel is red.
    bytes.replace(54, 8, std::string("\0\xff\0\0\xff\0\0\0", 8));
    bytes.replace(62, 8, std::string("\xff\0\0\xff\0\0\0\0", 8));
    return bytes;
}
static CLSID encoder_for(const wchar_t* mime) {
    UINT count = 0, size = 0;
    assert(Gdiplus::GetImageEncodersSize(&count, &size) == Gdiplus::Ok && count && size);
    std::vector<unsigned char> buffer(size);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    assert(Gdiplus::GetImageEncoders(count, size, encoders) == Gdiplus::Ok);
    for (UINT i = 0; i < count; ++i) {
        if (std::wcscmp(encoders[i].MimeType, mime) == 0) return encoders[i].Clsid;
    }
    assert(false);
    return {};
}
static void encoded_image(const fs::path& file, UINT width, UINT height, const CLSID& encoder) {
    fs::create_directories(file.parent_path());
    Gdiplus::Bitmap image(width, height, PixelFormat32bppARGB);
    assert(image.GetLastStatus() == Gdiplus::Ok);
    assert(image.SetPixel(0, 0, Gdiplus::Color(255, 210, 40, 30)) == Gdiplus::Ok);
    assert(image.SetPixel(width - 1, height - 1, Gdiplus::Color(255, 20, 80, 220)) == Gdiplus::Ok);
    assert(image.Save(file.wstring().c_str(), &encoder, nullptr) == Gdiplus::Ok);
}
static Artwork load(const std::vector<std::string>& roots) {
    LoadingModel model;
    Artwork preview, background;
    assert(load_loading_content(roots, "bzn/Map.bzn", model, preview, background));
    assert(model.show_preview);
    return preview;
}

int main(int argc, char** argv) {
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput startup;
    assert(Gdiplus::GdiplusStartup(&token, &startup, nullptr) == Gdiplus::Ok);
    const CLSID png_encoder = encoder_for(L"image/png");
    const CLSID jpeg_encoder = encoder_for(L"image/jpeg");
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path base = fs::temp_directory_path() / ("refit-loading-images-" + std::to_string(stamp));
    const fs::path low = base / "low", high = base / "high";

    encoded_image(low / "bzn/Map.png", 160, 160, png_encoder);
    write_bytes(high / "bzn/Map.bmp", bmp());
    Artwork image = load({low.string(), high.string()});
    assert(image.width == 2 && image.height == 2 && image.pixels[0] == 0x0000ff);

    encoded_image(high / "bzn/Map.png", 160, 160, png_encoder);
    image = load({low.string(), high.string()});
    assert(image.width == 160 && image.height == 160);
    assert(image.pixels.front() == 0xd2281e && image.pixels.back() == 0x1450dc);

    write_bytes(high / "bzn/Map.png", "corrupt PNG");
    image = load({low.string(), high.string()});
    assert(image.width == 2 && image.pixels[0] == 0x0000ff);

    write_bytes(high / "bzn/Map.bmp", "corrupt BMP");
    image = load({low.string(), high.string()});
    assert(image.width == 160 && image.pixels.front() == 0xd2281e);

    encoded_image(high / "bzn/Map.png", 3841, 1, png_encoder);
    image = load({low.string(), high.string()});
    assert(image.width == 160 && image.pixels.front() == 0xd2281e);

    // All supported formats are searched within the higher-precedence root.
    encoded_image(high / "bzn/Map.jpg", 160, 160, jpeg_encoder);
    image = load({low.string(), high.string()});
    assert(image.width == 160 && image.height == 160 && image.pixels.front() != 0xd2281e);
    fs::remove(high / "bzn/Map.jpg");
    encoded_image(high / "bzn/Map.jpeg", 160, 160, jpeg_encoder);
    image = load({low.string(), high.string()});
    assert(image.width == 160 && image.height == 160);

    encoded_image(high / "RefitScene/Loading/background.png", 160, 160, png_encoder);
    LoadingModel background_model;
    Artwork preview, background;
    assert(load_loading_content({low.string(), high.string()}, "Map", background_model, preview,
                                background));
    assert(background.width == 160 && background.height == 160);
    assert(background.pixels.front() == 0xd2281e);

    if (argc > 1) {
        const fs::path installed(argv[1]);
        assert(installed.filename() == "first.png" && installed.parent_path().filename() == "bzn");
        LoadingModel model;
        Artwork preview, background;
        assert(load_loading_content({installed.parent_path().parent_path().string()}, "first", model,
                                    preview, background));
        assert(model.show_preview && preview.width == 160 && preview.height == 160);
        assert(preview.pixels.size() == 160u * 160u);
    }
    fs::remove_all(base);
    Gdiplus::GdiplusShutdown(token);
}
