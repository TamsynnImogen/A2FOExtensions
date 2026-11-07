#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "game_monitor_policy.hpp"
#include "load_button_bounds.hpp"
#include "setup_details_line.hpp"
#include "scene_catalogue.hpp"
#include "scene_composition.hpp"
#include "skin.hpp"
#include "loading_theme.hpp"

namespace fs = std::filesystem;
namespace ia = a2fo::instant_action_settings;
namespace rm = a2fo::refit_menu;

static std::uint32_t next(std::uint32_t& seed) noexcept {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}
static std::string hex(std::string_view value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (unsigned char b : value) {
        result += digits[b >> 4];
        result += digits[b & 15];
    }
    return result;
}
static std::string hex32(std::uint32_t value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(8, '0');
    for (int i = 7; i >= 0; --i) {
        result[static_cast<std::size_t>(i)] = digits[value & 15u];
        value >>= 4;
    }
    return result;
}
static std::uint32_t pixel_hash(const rm::Artwork& artwork) noexcept {
    std::uint32_t result = 2166136261u;
    for (auto pixel : artwork.pixels) {
        result ^= pixel;
        result *= 16777619u;
    }
    return result;
}
static void bounds(const ia::Bounds& value) {
    std::cout << value.left << ',' << value.top << ','
              << value.right << ',' << value.bottom;
}
static void views(const std::vector<rm::menu_scene::SceneView>& value) {
    std::cout << value.size();
    for (const auto& view : value)
        std::cout << ',' << hex(view.id) << '=' << hex(view.label);
}

static void monitors() {
    std::vector<a2fo::GameMonitorDescriptor> set(3);
    set[0].device_name = "DISPLAY1";
    set[0].adapter_index = 0;
    set[1].device_name = "Display2";
    set[1].adapter_index = 7;
    set[1].primary = true;
    set[2].device_name = "display3";
    set[2].adapter_index = 9;
    for (const auto& configured : {"", "DISPLAY2", "absent"}) {
        const auto result = a2fo::resolve_game_monitor(set, configured, 9);
        std::cout << "monitor.base configured=" << hex(configured)
                  << " fallback=9 result=" << result.monitor_index << ','
                  << result.configured << ',' << result.configured_device_present
                  << '\n';
    }
    std::cout << "monitor.label " << hex(a2fo::game_monitor_label(set[1]))
              << '\n';
    for (int width : {-1, 0, 1, 63, 64, 640, 4096})
        std::cout << "monitor.legacy " << width << ' '
                  << a2fo::is_legacy_monitor_ordinal_display_width(width)
                  << '\n';
    std::uint32_t seed = 0x15d154a7u;
    for (int case_index = 0; case_index < 32; ++case_index) {
        std::vector<a2fo::GameMonitorDescriptor> items;
        const int count = static_cast<int>(next(seed) % 5u);
        for (int i = 0; i < count; ++i) {
            a2fo::GameMonitorDescriptor item;
            item.device_name = i == 2 && case_index % 7 == 0
                ? "DISPLAY1" : "Display" + std::to_string(i + 1);
            item.adapter_index = static_cast<int>(next(seed) % 8u);
            item.primary = i == case_index % 4;
            item.width = 640 + static_cast<int>(next(seed) % 3200u);
            item.height = 480 + static_cast<int>(next(seed) % 1800u);
            items.push_back(item);
        }
        const std::string configured = case_index % 4 == 0 ? "" :
            case_index % 4 == 1 ? "DISPLAY1" :
            case_index % 4 == 2 ? "display3" : "missing";
        const int fallback = case_index % 3 == 0 ? -1 :
            static_cast<int>(next(seed) % 8u);
        const auto result = a2fo::resolve_game_monitor(items, configured,
                                                        fallback);
        std::cout << "monitor.random " << case_index << " count=" << count
                  << " name=" << hex(configured) << " adapter=" << fallback
                  << " primary=";
        for (const auto& item : items)
            std::cout << item.adapter_index << ':' << item.primary << ',';
        std::cout << " result=" << result.monitor_index << ','
                  << result.configured << ',' << result.configured_device_present
                  << '\n';
    }
}

static void load_bounds() {
    const ia::Bounds save{10, 10, 110, 30};
    const std::array<ia::Bounds, 5> fixed{{
        {10, 36, 110, 56}, {10, 36, 0, 0}, {0, 0, 0, 0},
        {16384, 16384, 16384, 16384},
        {std::numeric_limits<int>::max(), 0, 0, 0}}};
    for (std::size_t i = 0; i < fixed.size(); ++i) {
        const auto result = ia::effective_load_bounds(fixed[i], save);
        std::cout << "bounds.base " << i << " input=";
        bounds(fixed[i]); std::cout << " result=" << result.valid << ','
            << result.repaired << ','; bounds(result.bounds); std::cout << '\n';
    }
    for (const auto& caption : {"&Load_Settings", "load settingz", "Load Settings",
                                "_Load settings", "&&LOAD SETTINGS", ""})
        std::cout << "bounds.caption " << hex(caption) << ' '
                  << ia::is_load_settings_caption(caption) << '\n';
    std::uint32_t seed = 0x42f0912bu;
    for (int i = 0; i < 32; ++i) {
        const int left = static_cast<int>(next(seed) % 4001u) - 2000;
        const int top = static_cast<int>(next(seed) % 4001u) - 2000;
        const int width = static_cast<int>(next(seed) % 160u) - 20;
        const int height = static_cast<int>(next(seed) % 120u) - 20;
        const ia::Bounds load{left, top, left + width, top + height};
        const ia::Bounds backup{left - 20, top - 20, left + 80, top + 30};
        const auto result = ia::effective_load_bounds(load, backup);
        const auto x = i % 2 == 0 ? load.left : load.right;
        const auto y = i % 3 == 0 ? load.top : load.bottom;
        std::cout << "bounds.random " << i << " load=";
        bounds(load); std::cout << " save="; bounds(backup);
        std::cout << " result=" << result.valid << ',' << result.repaired
                  << ','; bounds(result.bounds);
        std::cout << " contains=" << ia::contains(load, x, y) << '\n';
    }
    std::cout << "bounds.contains.edge "
              << ia::contains({10, 36, 110, 56}, 109, 55) << ','
              << ia::contains({10, 36, 110, 56}, 110, 55) << '\n';
}

static void setup_details() {
    const std::vector<std::string> fixed{
        "setupDetails = 0A11ff\r\nnext", "setupDetails=0A1Xff\n",
        "  setupDetails = 0A11ff trailing\n"};
    for (std::size_t i = 0; i < fixed.size(); ++i) {
        std::uint8_t output[4]{9, 9, 9, 0xee};
        const auto result = ia::decode_setup_details_line(
            fixed[i].data(), fixed[i].size(), output, 3);
        std::cout << "setup.base " << i << " text=" << hex(fixed[i])
                  << " result=" << result.decoded << ','
                  << result.next_line_offset << ',' << unsigned(output[0])
                  << ',' << unsigned(output[1]) << ',' << unsigned(output[2])
                  << ',' << unsigned(output[3]) << '\n';
    }
    static constexpr char digits[] = "0123456789ABCDEF";
    std::uint32_t seed = 0x927ee0b1u;
    for (int i = 0; i < 32; ++i) {
        std::string value = i % 4 == 0 ? "setupDetails = " :
            i % 4 == 1 ? "SETUPDETAILS=" :
            i % 4 == 2 ? "setupDetails=" : "otherDetails=";
        for (int digit = 0; digit < 6; ++digit)
            value += digits[next(seed) % 16u];
        if (i % 7 == 0) value[value.size() - 1] = 'X';
        if (i % 9 == 0) value.push_back('\0');
        value += i % 2 == 0 ? "\r\nnext" : "\nnext";
        std::uint8_t output[4]{9, 8, 7, 0xee};
        const auto result = ia::decode_setup_details_line(
            value.data(), value.size(), output, 3);
        std::cout << "setup.random " << i << " text=" << hex(value)
                  << " result=" << result.decoded << ','
                  << result.next_line_offset << ',' << unsigned(output[0])
                  << ',' << unsigned(output[1]) << ',' << unsigned(output[2])
                  << ',' << unsigned(output[3]) << '\n';
    }
}

static void catalogue() {
    std::vector<rm::menu_scene::SceneView> output{{"prior", "Prior"}};
    std::string error;
    const std::vector<std::string> fixed{
        "[View.Main]\nLabel= \"Launch\" ; comment\n[View.Other]\n",
        "[View.X]\n[View.x]\n", "[View.bad\n"};
    for (std::size_t i = 0; i < fixed.size(); ++i) {
        const bool ok = rm::menu_scene::parse_scene_catalogue(
            fixed[i], output, &error);
        std::cout << "catalogue.base " << i << " ok=" << ok
                  << " error=" << hex(error) << " views=";
        views(output); std::cout << '\n';
    }
    std::uint32_t seed = 0x892f4831u;
    for (int i = 0; i < 32; ++i) {
        const std::string id = "Node" + std::to_string(next(seed) % 32u);
        std::string source = "[View." + id + "]\nLabel= \"First" +
            std::to_string(i) + "\"\n";
        switch (i % 8) {
        case 0: source += "[View." + id + "]\n"; break;
        case 1: source += "[view." + id + "]\n"; break;
        case 2: source += "[View.Other]\nLabel=A\nLabel=B\n"; break;
        case 3: source += "[View.Other]\nLabel=\"\"\n"; break;
        case 4: source += "[View." + id + "]"; source.insert(
                    source.size() - 1, 1, '\0'); source += '\n'; break;
        case 5: source += "[View.Other\n"; break;
        case 6: source += "[not-a-view]\nLabel=Ignore\n"; break;
        default: source += "; comment\n[View.Other]\nLabel=Final\n"; break;
        }
        const bool ok = rm::menu_scene::parse_scene_catalogue(
            source, output, &error);
        std::cout << "catalogue.random " << i << " text=" << hex(source)
                  << " ok=" << ok << " error=" << hex(error) << " views=";
        views(output); std::cout << '\n';
    }
}

static void skins() {
    const std::vector<std::string> paths{
        "bitmaps\\main\\mainbkgr.png", "SharedButtons/Button1_hoover.bmp",
        "MissionSelect/MissionFieldDisabled_small.bmp",
        "multisetup/advanced/ADVtextbutthilitR.png", "../bad.png"};
    for (std::size_t i = 0; i < paths.size(); ++i) {
        const auto style = rm::skin::classify(paths[i]);
        std::cout << "skin.base " << i << " path=" << hex(paths[i])
                  << " style=" << int(style.role) << ',' << style.hot
                  << ',' << style.pressed << ',' << style.disabled << '\n';
    }
    std::uint32_t pixels[8]{0xff112233, 0x00aabbcc, 0xffff00ff,
        0xff808080, 0xffeeeeee, 0xff304050, 0x12345678, 0xdeadbeef};
    const auto hover = rm::skin::classify("SharedButtons/Button1_hoover.bmp");
    std::cout << "skin.apply.base " << rm::skin::apply(pixels, 4, 3, 2, hover);
    for (auto pixel : pixels) std::cout << ',' << hex32(pixel);
    std::cout << '\n';
    std::uint32_t seed = 0x6f51a490u;
    for (int i = 0; i < 32; ++i) {
        static constexpr const char* names[]{
            "main/mainbkgr.png", "SharedButtons/Button1_hoover.bmp",
            "SharedButtons/Button1_pressed.bmp", "MissionSelect/FieldDisabled.bmp",
            "multisetup/advanced/ADVtextbutthilitR.png", "../bad.png",
            "bitmaps/main/mainbkgr.png", "Main/MultiSetup.bmp"};
        const std::string path = names[next(seed) % 8u];
        const auto style = rm::skin::classify(path);
        std::array<std::uint32_t, 8> row{};
        for (auto& pixel : row) pixel = next(seed);
        const int width = i % 11 == 0 ? 0 : 3;
        const int height = i % 13 == 0 ? 0 : 2;
        const bool changed = rm::skin::apply(row.data(), 4, width, height,
                                              style);
        std::cout << "skin.random " << i << " path=" << hex(path)
                  << " style=" << int(style.role) << ',' << style.hot
                  << ',' << style.pressed << ',' << style.disabled
                  << " dimensions=" << width << ',' << height
                  << " changed=" << changed;
        for (auto pixel : row) std::cout << ',' << hex32(pixel);
        std::cout << '\n';
    }
}

static void compositions() {
    rm::Artwork base{64, 64, std::vector<std::uint32_t>(4096, 0x00112233)},
        current = base,
        live{2, 2, {0x00010203, 0x00040506, 0x00070809, 0x000a0b0c}},
        output;
    current.pixels[5] = 0x00ffffff;
    rm::scene::Backgrounds backgrounds;
    backgrounds.remember("multisetup/instactionbkg.bmp", base);
    const bool matches = backgrounds.matches(current);
    const std::string key = backgrounds.key(current);
    const bool composed = backgrounds.compose(current, live, output);
    std::cout << "scene.base match=" << matches << " key=" << hex(key)
              << " composed=" << composed << " size=" << output.width
              << 'x' << output.height << " pixels=" << hex32(output.pixels[0])
              << ',' << hex32(output.pixels[5]) << ','
              << hex32(output.pixels[4095]) << '\n';

    std::uint32_t seed = 0x8492be31u;
    for (int i = 0; i < 32; ++i) {
        rm::Artwork saved{4, 4, std::vector<std::uint32_t>(16)};
        for (auto& pixel : saved.pixels) pixel = next(seed) & 0x00ffffffu;
        rm::Artwork now = saved;
        if (i % 3 != 0) now.pixels[next(seed) % 16u] ^= 0x000f0f0fu;
        rm::Artwork overlay{4, 4, std::vector<std::uint32_t>(16)};
        for (auto& pixel : overlay.pixels) pixel = next(seed) & 0x00ffffffu;
        if (i % 9 == 0) overlay.width = 0;
        rm::scene::Backgrounds scenes;
        const std::string first_key = "scene" + std::to_string(i);
        scenes.remember(first_key, saved);
        if (i % 7 == 0) {
            rm::Artwork duplicate = saved;
            duplicate.pixels[0] ^= 0x00010101u;
            scenes.remember(first_key, duplicate);
        }
        rm::Artwork rendered{1, 1, {0x00abcdefu}};
        const bool found = scenes.matches(now);
        const std::string key_now = scenes.key(now);
        const bool ok = scenes.compose(now, overlay, rendered);
        std::cout << "scene.random " << i << " saved="
                  << hex32(pixel_hash(saved)) << " current="
                  << hex32(pixel_hash(now)) << " live="
                  << overlay.width << 'x' << overlay.height << ':'
                  << hex32(pixel_hash(overlay)) << " match=" << found
                  << " key=" << hex(key_now) << " composed=" << ok
                  << " out=" << rendered.width << 'x' << rendered.height
                  << ':' << hex32(pixel_hash(rendered)) << '\n';
    }
}

static void files_and_themes() {
    std::vector<rm::menu_scene::SceneView> output{{"prior", "Prior"}};
    std::string error;
    const auto temp = fs::temp_directory_path() / "a2fo-rust-ui-parity";
    fs::create_directories(temp / "low/Menu");
    { std::ofstream file(temp / "low/Menu/map.ini");
      file << "[View.saved]\nLabel=Saved\n"; }
    const bool loaded = rm::menu_scene::load_scene_catalogue(
        (temp / "low").string(), output, &error);
    std::cout << "file.catalogue.loaded " << loaded << " error="
              << hex(error) << " views="; views(output); std::cout << '\n';
    const bool missing = rm::menu_scene::load_scene_catalogue(
        (temp / "missing").string(), output, &error);
    std::cout << "file.catalogue.missing " << missing << " error="
              << hex(error) << " views="; views(output); std::cout << '\n';
    fs::create_directories(temp / "low/RefitScene/Loading");
    fs::create_directories(temp / "high/RefitScene/Loading");
    auto theme_text = [](const char* name, const char* accent) {
        std::string text = "[theme.shell]\nname=Shell\nform=shell\n";
        for (const char* key : {"panel_top", "panel", "panel_edge", "panel_hi",
                 "text", "text_dim", "title", "accent", "accent2", "fill", "good"})
            text += std::string(key) + "=ff010203\n";
        text += "corners=-1, 2, 3, -4\n[theme.card]\nname=" +
            std::string(name) + "\nform=arch\naliases=cardassian\n";
        for (const char* key : {"panel_top", "panel", "panel_edge", "panel_hi",
                 "text", "text_dim", "title", "accent", "accent2", "fill", "good"})
            text += std::string(key) + "=" +
                (std::string(key) == "accent" ? accent : "ff010203") + "\n";
        return text + "corners=-2, 3, 4, -5\n";
    };
    { std::ofstream file(temp / "low/RefitScene/Loading/themes.ini");
      file << theme_text("Card", "ff123456"); }
    { std::ofstream file(temp / "high/RefitScene/Loading/themes.ini");
      file << "[theme.card]\naccent=ffabcdef\n"; }
    const std::vector<std::string> roots{
        (temp / "low").string(), (temp / "high").string()};
    for (const char* race : {"", "cardassian", "card", "unknown"}) {
        const auto theme = rm::loading_theme(roots, race);
        std::cout << "file.theme race=" << hex(race) << " result="
                  << hex(theme.name) << ',' << hex(theme.race) << ','
                  << int(theme.form) << ',' << hex32(theme.accent) << ','
                  << theme.corners[0] << '\n';
    }
    fs::remove_all(temp);
}

int main() {
    monitors();
    load_bounds();
    setup_details();
    catalogue();
    skins();
    compositions();
    files_and_themes();
}
