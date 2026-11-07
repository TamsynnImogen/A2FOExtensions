#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../modules/A2FORefitMenus/loading_content.hpp"

using namespace a2fo::refit_menu;
namespace fs = std::filesystem;

static void put(const fs::path& file, const std::string& bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    assert(out);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    assert(out);
}
static void word(std::string& b, std::size_t at, std::uint32_t value, int count) {
    for (int i = 0; i < count; ++i) b[at + i] = static_cast<char>(value >> (i * 8));
}
static std::string bmp(bool top_down = false) {
    // Asymmetric 2x2, BGR: top red/green, bottom blue/white.
    std::string b(54 + 16, '\0');
    b[0] = 'B'; b[1] = 'M';
    word(b, 2, b.size(), 4); word(b, 10, 54, 4); word(b, 14, 40, 4);
    word(b, 18, 2, 4); word(b, 22, top_down ? 0xfffffffeu : 2u, 4);
    word(b, 26, 1, 2); word(b, 28, 24, 2); word(b, 34, 16, 4);
    const std::string top{"\0\0\xff\0\xff\0\0\0", 8};
    const std::string bottom{"\xff\0\0\xff\xff\xff\0\0", 8};
    b.replace(54, 8, top_down ? top : bottom);
    b.replace(62, 8, top_down ? bottom : top);
    return b;
}
static std::string indexed_bmp(bool top_down = false) {
    // 3x2, four BGRA palette entries; both rows have one byte of padding.
    std::string b(54 + 16 + 8, '\0');
    b[0] = 'B'; b[1] = 'M';
    word(b, 2, b.size(), 4); word(b, 10, 70, 4); word(b, 14, 40, 4);
    word(b, 18, 3, 4); word(b, 22, top_down ? 0xfffffffeu : 2u, 4);
    word(b, 26, 1, 2); word(b, 28, 8, 2); word(b, 34, 8, 4);
    word(b, 46, 4, 4);
    const std::string palette{"\0\0\xff\0\0\xff\0\0\xff\0\0\0\xff\xff\xff\0", 16};
    b.replace(54, 16, palette);
    const std::string top{"\0\1\2\0", 4}, bottom{"\2\3\0\0", 4};
    b.replace(70, 4, top_down ? top : bottom);
    b.replace(74, 4, top_down ? bottom : top);
    return b;
}

int main() {
    LoadingModel model;
    model.title = "Native title";
    model.mode = "Native mode";
    assert(parse_loading_objectives("\r\n Mission One \r\nFirst line\r\n second line\r\n\r\nNext para\r\n* First task \r\n*Second task\r\nHINT: Look here\r\n and there\r\n\r\nFinal note", model));
    assert(model.title == "Mission One");
    assert((model.briefing == std::vector<std::string>{"First line second line", "Next para"}));
    assert((model.objectives == std::vector<std::string>{"First task", "Second task"}));
    assert(model.hint == "Look here and there Final note");
    assert(model.mode == "Native mode");
    const std::string cp1252 = std::string("Cit") + char(0xe9);
    assert(parse_loading_objectives(cp1252 + "\n*" + cp1252, model));
    assert(model.title == cp1252 && model.objectives[0] == cp1252);
    assert(!parse_loading_objectives(std::string(65537, 'x'), model));
    assert(model.title == cp1252);
    assert(sanitize_loading_map_name("bzn\\Map Name.BZN") == "Map Name");
    for (const char* bad : {"../Map.bzn", "bzn/../Map.bzn", "Map.txt", ".bzn", "C:Map.bzn", "bad*name.bzn"})
        assert(sanitize_loading_map_name(bad).empty());

    const std::vector<std::uint32_t> expected_indexed{
        0xff0000, 0x00ff00, 0x0000ff, 0x0000ff, 0xffffff, 0xff0000};
    Artwork decoded;
    assert(decode_loading_bmp(indexed_bmp(), decoded));
    assert(decoded.width == 3 && decoded.height == 2 && decoded.pixels == expected_indexed);
    assert(decode_loading_bmp(indexed_bmp(true), decoded));
    assert(decoded.pixels == expected_indexed);
    auto full_palette = indexed_bmp();
    full_palette.resize(54 + 1024 + 8);
    full_palette.replace(54 + 1024, 8, indexed_bmp().substr(70, 8));
    word(full_palette, 2, full_palette.size(), 4);
    word(full_palette, 10, 54 + 1024, 4);
    word(full_palette, 46, 0, 4); // Zero clrUsed means the full 256-entry palette.
    assert(decode_loading_bmp(full_palette, decoded));
    assert(decoded.pixels == expected_indexed);
    const Artwork before = decoded;
    auto bad_bmp = indexed_bmp();
    bad_bmp.resize(bad_bmp.size() - 1);
    assert(!decode_loading_bmp(bad_bmp, decoded) && decoded.pixels == before.pixels);
    bad_bmp = indexed_bmp();
    word(bad_bmp, 10, 69, 4); // Palette overlaps pixel data.
    assert(!decode_loading_bmp(bad_bmp, decoded) && decoded.pixels == before.pixels);
    bad_bmp = indexed_bmp();
    word(bad_bmp, 46, 5, 4); // Declared palette exceeds the available entries.
    assert(!decode_loading_bmp(bad_bmp, decoded));
    bad_bmp = indexed_bmp();
    bad_bmp[70] = 4; // Pixel index is outside the declared palette.
    assert(!decode_loading_bmp(bad_bmp, decoded));
    bad_bmp = indexed_bmp();
    word(bad_bmp, 30, 1, 4); // RLE remains unsupported.
    assert(!decode_loading_bmp(bad_bmp, decoded));
    bad_bmp = indexed_bmp();
    word(bad_bmp, 28, 4, 2); // Packed indexed depths remain unsupported.
    assert(!decode_loading_bmp(bad_bmp, decoded));
    assert(decoded.pixels == before.pixels);

    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path base = fs::temp_directory_path() / ("refit-loading-content-" + std::to_string(suffix));
    const fs::path low = base / "low", high = base / "high";
    put(low / "objectives/Map_a.txt", "Low title\n*Low task");
    put(high / "objectives/Map_main.txt", "High title\n*High task");
    put(low / "bzn/Map.bmp", bmp());
    put(high / "RefitScene/Loading/background.bmp", bmp(true));
    Artwork preview, background;
    model.title = "Native title";
    assert(load_loading_content({low.string(), high.string()}, "bzn/Map.bzn", model, preview, background));
    assert(model.title == "High title" && model.objectives[0] == "High task");
    assert(model.show_preview && preview.width == 2 && preview.height == 2);
    assert(preview.pixels[0] == 0xff0000 && preview.pixels[1] == 0x00ff00);
    assert(preview.pixels[2] == 0x0000ff && preview.pixels[3] == 0xffffff);
    assert(background.pixels == preview.pixels);

    // A valid higher-precedence indexed preview wins over the loose 24-bit base.
    put(high / "bzn/Map.bmp", indexed_bmp());
    assert(load_loading_content({low.string(), high.string()}, "Map", model, preview, background));
    assert(preview.width == 3 && preview.pixels == expected_indexed);

    // Invalid higher-precedence images fall back without a large allocation.
    auto huge = bmp();
    word(huge, 18, 3841, 4);
    put(high / "bzn/Map.bmp", huge);
    assert(load_loading_content({low.string(), high.string()}, "Map", model, preview, background));
    assert(preview.pixels[0] == 0xff0000);

    LoadingModel missing;
    missing.title = "Native title";
    missing.show_preview = true;
    Artwork no_preview, no_background;
    assert(!load_loading_content({low.string()}, "Missing", missing, no_preview, no_background));
    assert(missing.title == "Native title" && !missing.show_preview && no_preview.pixels.empty());
    fs::remove_all(base);
}
