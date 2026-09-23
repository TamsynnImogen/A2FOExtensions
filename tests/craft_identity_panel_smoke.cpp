// Headless regression for the production selected-panel render hooks.
// Map a private Armada fixture without running its entry point. Execute its
// real rectangle loader, GUIText constructor and rectangle-to-screen drawing;
// replace only configuration lookup and the final graphics submission.
#include "../modules/A2FOCraftIdentity/module.cpp"
#include "../core/hook.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>

namespace {
std::uint8_t* fixture;
std::array<std::uint8_t, 0x210> panel{};
std::array<std::uint8_t, 0x300> craft{};
std::array<std::uint8_t, 0x104> captain{}, build_name{}, build_class{};
std::array<std::uint8_t, 0x20> font{};
void* font_slot = font.data();
int object_class;
std::map<std::string, RawRectangle> rectangles{
    {"captain", {675, 91, 240, 20}},
    {"registry", {675, 115, 240, 20}},
    {"photon", {700, 40, 103, 10}},
    {"quantum", {700, 68, 103, 10}},
    {"shuttle", {700, 96, 103, 10}},
    {"build_name", {29, 21, 240, 20}},
    {"build_class", {29, 36, 240, 20}},
};
struct Submission { std::string text; FloatRectangle rectangle; };
std::vector<Submission> submissions;

template<class T> void put(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}
void jump(std::uintptr_t rva, void* function) {
    std::array<std::uint8_t, 5> original{};
    std::memcpy(original.data(), fixture + rva, original.size());
    assert(a2fo::patch_jump(fixture + rva, function, original.data(), original.size()));
}
void map_fixture(const char* path) {
    std::ifstream input(path, std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
    assert(bytes.size() > sizeof(IMAGE_DOS_HEADER));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(bytes.data() + dos->e_lfanew);
    fixture = static_cast<std::uint8_t*>(VirtualAlloc(nullptr,
        nt->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    assert(fixture);
    std::memcpy(fixture, bytes.data(), nt->OptionalHeader.SizeOfHeaders);
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const auto& s = sections[i];
        assert(s.PointerToRawData + s.SizeOfRawData <= bytes.size());
        std::memcpy(fixture + s.VirtualAddress, bytes.data() + s.PointerToRawData, s.SizeOfRawData);
    }
    const auto delta = reinterpret_cast<std::uintptr_t>(fixture) - nt->OptionalHeader.ImageBase;
    const auto directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    for (unsigned offset = 0; offset < directory.Size;) {
        const auto* block = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(
            fixture + directory.VirtualAddress + offset);
        assert(block->SizeOfBlock >= sizeof(*block));
        const auto* entries = reinterpret_cast<const std::uint16_t*>(block + 1);
        for (unsigned i = 0; i < (block->SizeOfBlock - sizeof(*block)) / 2; ++i) {
            if ((entries[i] >> 12) == IMAGE_REL_BASED_HIGHLOW) {
                auto* address = fixture + block->VirtualAddress + (entries[i] & 0xfff);
                std::uint32_t value;
                std::memcpy(&value, address, sizeof(value));
                put(address, 0, value + delta);
            } else assert((entries[i] >> 12) == IMAGE_REL_BASED_ABSOLUTE);
        }
        offset += block->SizeOfBlock;
    }
}
bool __fastcall get_rectangle(void*, void*, const char* key,
                              RawRectangle* result, const RawRectangle*) {
    const auto found = rectangles.find(key);
    if (found == rectangles.end()) return false;
    *result = found->second;
    return true;
}
void __cdecl submit_text(void* display, const FloatRectangle* rectangle,
                         std::uint32_t, float, void*, const char* value) {
    assert(display == font.data());
    submissions.push_back({value, *rectangle});
}
std::map<std::string, Colour> colours;
std::map<std::string, std::string> strings;
bool __fastcall get_colour(void*, void*, const char* key, Colour* result, const Colour*) {
    const auto found = colours.find(key);
    if (found == colours.end()) return false;
    *result = found->second;
    return true;
}
bool __fastcall get_string(void*, void*, const char* key, char* result, unsigned size, const char*) {
    const auto found = strings.find(key);
    if (found == strings.end()) return false;
    std::snprintf(result, size, "%s", found->second.c_str());
    return true;
}
std::array<std::uint8_t, 0x100> fill_sprite{}, track_sprite{};
std::map<std::string, void*> sprites;
struct SpriteSubmission { void* sprite; SpriteVector position; float width, height, uv_width; Colour colour; };
std::vector<SpriteSubmission> sprite_submissions;
void* __fastcall get_sprite(void*, void*, const char* key, int) {
    const auto found = sprites.find(key);
    return found == sprites.end() ? nullptr : found->second;
}
void __attribute__((regparm(2))) set_sprite_colour(void* sprite, const Colour* colour) {
    put(sprite, kSpriteColourOffset, *colour);
}
void __attribute__((regparm(2))) submit_sprite(void* sprite, const SpriteVector* position,
                                              float height, float width) {
    sprite_submissions.push_back({sprite, *position, width, height,
        read_at<float>(sprite, kSpriteTextureWidthOffset, 0),
        read_at<Colour>(sprite, kSpriteColourOffset, {})});
}
void prepare_sprite_renderer() {
    auto* fo = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x210000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    assert(fo);
    g_fleet_ops = reinterpret_cast<HMODULE>(fo);
    // Preserve the validated prologue, undo its frame, then route the ABI to
    // an in-memory capture. No native graphics or game entry point runs.
    const auto stub = [](std::uint8_t* site, const std::uint8_t* prefix, unsigned size,
                         const std::vector<std::uint8_t>& unwind, void* target) {
        std::memcpy(site, prefix, size);
        std::memcpy(site + size, unwind.data(), unwind.size());
        auto* jump_site = site + size + unwind.size();
        jump_site[0] = 0xe9;
        put(jump_site, 1, static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(target) - reinterpret_cast<std::uintptr_t>(jump_site + 5)));
    };
    stub(fo + kFoSpriteSetColourRva, kExpectedFoSpriteSetColour,
        sizeof(kExpectedFoSpriteSetColour), {0xc9}, reinterpret_cast<void*>(&set_sprite_colour));
    stub(fo + kFoSpriteDrawScaled2DRva, kExpectedFoSpriteDrawScaled2D,
        sizeof(kExpectedFoSpriteDrawScaled2D), {0xc9}, reinterpret_cast<void*>(&submit_sprite));
    stub(fixture + kInterfaceSpriteDatabaseGetRva, kExpectedInterfaceSpriteDatabaseGet,
        sizeof(kExpectedInterfaceSpriteDatabaseGet), {0x5f, 0x5e, 0x5b, 0xc9},
        reinterpret_cast<void*>(&get_sprite));
    put(fixture, kInterfaceSpriteDatabasePointerRva, &object_class);
    for (auto* sprite : {fill_sprite.data(), track_sprite.data()}) {
        put(sprite, kSpriteFrameListOffset, &object_class);
        put(sprite, kSpriteTextureWidthOffset, 0.75f);
        put(sprite, kSpriteColourOffset, Colour{0.3f, 0.4f, 0.5f});
    }
    sprites = {{"test_fill", fill_sprite.data()}, {"test_track", track_sprite.data()},
               {"large_shield_bar", fill_sprite.data()}};
}
float A2FO_CALL maximum(void*) { return 100.0f; }
float A2FO_CALL current(void*) { return 50.0f; }

void construct_text(void* component, const char* key) {
    NativeRectangle rectangle{};
    a2fo_identity_call_thiscall_2(fixture + 0xf3e70, panel.data(),
        reinterpret_cast<std::uintptr_t>(&rectangle), reinterpret_cast<std::uintptr_t>(key));
    a2fo_identity_call_thiscall_3(fixture + 0x10c1f0, component,
        reinterpret_cast<std::uintptr_t>(panel.data()),
        reinterpret_cast<std::uintptr_t>(&rectangle),
        reinterpret_cast<std::uintptr_t>(&font_slot));
    const auto live = read_at<NativeRectangle>(component, 0x58, {});
    const auto& cfg = rectangles.at(key);
    assert(live.left == cfg.x && live.top == cfg.y - 10);
    assert(live.right == cfg.x + cfg.width - 1);
    assert(live.bottom == cfg.y + cfg.height - 11);
}
void draw(bool builder) {
    submissions.clear();
    if (builder) selected_builder_info_render_hook(panel.data(), nullptr);
    else selected_info_render_hook(panel.data(), nullptr);
}
void check(const char* text, const char* key) {
    const auto found = std::find_if(submissions.begin(), submissions.end(),
        [text](const Submission& entry) { return entry.text == text; });
    if (found == submissions.end()) std::fprintf(stderr, "Missing draw: %s\n", text);
    assert(found != submissions.end());
    const auto& cfg = rectangles.at(key);
    assert(found->rectangle.x == cfg.x + read_at<int>(panel.data(), 4, 0));
    assert(found->rectangle.y == cfg.y - 10 + read_at<int>(panel.data(), 8, 0));
    assert(found->rectangle.width == cfg.width - 1);
    assert(found->rectangle.height == cfg.height - 1);
}
void check_ammunition() {
    check("Photon Torpedoes: 50%", "photon");
    check("Quantum Torpedoes: 50%", "quantum");
    check("Shuttle Craft: 50%", "shuttle");
    for (unsigned i = 0; i < 3; ++i) {
        const auto& hit = g_ammunition_tooltip.hit_rectangles[i];
        assert(g_ammunition_tooltip.visible[i]);
        assert(hit.x == submissions[i].rectangle.x && hit.y == submissions[i].rectangle.y);
    }
}
} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    map_fixture(argv[1]);
    g_armada = reinterpret_cast<HMODULE>(fixture);
    // Suppress static-initializer registration; bind the one Win32 import
    // used by the real rectangle loader. SetRect only writes a RECT.
    put(fixture, 0x364eb8, std::uint8_t(1));
    put(fixture, 0x3b816c, &SetRect);
    put(fixture, kLocalizationManagerPointerRva, static_cast<void*>(nullptr));
    jump(0x1358f0, reinterpret_cast<void*>(&get_rectangle));
    jump(0x228990, reinterpret_cast<void*>(&submit_text));
    put(panel.data(), 0x1f0, 10); // Native infoPanelOffset, not a font adjustment.
    construct_text(captain.data(), "captain");
    construct_text(build_name.data(), "build_name");
    construct_text(build_class.data(), "build_class");
    put(panel.data(), kInfoDisplayCaptainTextOffset, captain.data());
    put(panel.data(), kInfoDisplayBuilderNameTextOffset, build_name.data());
    put(panel.data(), kInfoDisplayBuilderClassTextOffset, build_class.data());
    put(panel.data(), kInfoDisplaySelectedCraftOffset, craft.data());
    put(craft.data(), kObjectHandleOffset, std::uint32_t(42));
    put(craft.data(), kObjectClassOffset, &object_class);
    put(craft.data(), kCraftNameIndexOffset, std::int32_t(0));
    g_class_policies[&object_class] = {"test_yard", {"Yard"}, {"Command"}, {"Repair facility"}};

    auto& ui = g_ui_configuration;
    ui.loaded = true;
    ui.parameter_db = gui_parameter_db();
    ui.captain_rectangle_found = ui.registry_rectangle_found = true;
    ui.photon_rectangle_found = ui.quantum_rectangle_found = ui.shuttle_craft_rectangle_found = true;
    ui.builder_name_rectangle_found = ui.builder_class_rectangle_found = true;
    ui.captain_rectangle = rectangles.at("captain");
    ui.registry_rectangle = rectangles.at("registry");
    ui.photon_rectangle = rectangles.at("photon");
    ui.quantum_rectangle = rectangles.at("quantum");
    ui.shuttle_craft_rectangle = rectangles.at("shuttle");
    ui.builder_name_rectangle = rectangles.at("build_name");
    ui.builder_class_rectangle = rectangles.at("build_class");
    g_energy_api_resolution_attempted = true;
    g_get_photon_torpedoes = g_get_quantum_torpedoes = g_get_shuttle_craft = current;
    g_get_maximum_photon_torpedoes = g_get_maximum_quantum_torpedoes = g_get_maximum_shuttle_craft = maximum;
    g_directional_shields_api_resolution_attempted = true;
    g_runtime_ready = true;

    // Select a yard first: the captain component has never rendered or been
    // updated for an ordinary ship. Repeat across panel origins/movement.
    for (const auto& origin : std::array<std::array<int, 2>, 3>{{{0, 500}, {0, 600}, {43, 337}}}) {
        put(panel.data(), 4, origin[0]);
        put(panel.data(), 8, origin[1]);
        for (bool builder : {true, false, true}) {
            draw(builder);
            check_ammunition();
            check("Command", "captain");
            check("Repair facility", "registry");
            assert(submissions.size() == 5);
        }
    }
    // Hidden or unrelated producer labels do not shift custom text.
    put(build_name.data(), 0x58, NativeRectangle{29, 2000, 29, 2000});
    put(build_class.data(), 0x58, NativeRectangle{900, 900, 999, 919});
    draw(true); check_ammunition(); check("Repair facility", "registry");
    // Fallback uses the matching build-name CFG when captain is unavailable.
    put(panel.data(), kInfoDisplayCaptainTextOffset, static_cast<void*>(nullptr));
    construct_text(build_name.data(), "build_name");
    draw(true); check_ammunition(); check("Repair facility", "registry");
    put(panel.data(), kInfoDisplayCaptainTextOffset, captain.data());
    // Registry strings are free-form; a captain-name ODF list is not required.
    g_class_policies[&object_class].captain_names.clear(); g_craft_identities.clear();
    draw(true); check_ammunition(); check("Repair facility", "registry");
    assert(submissions.size() == 4);
    // Optional/missing/out-of-range identities stay blank; ammo stays visible.
    ui.registry_rectangle_found = false;
    draw(true); check_ammunition(); assert(submissions.size() == 3);
    ui.registry_rectangle_found = true;
    put(craft.data(), kCraftNameIndexOffset, std::int32_t(4));
    draw(true); check_ammunition(); assert(submissions.size() == 3);
    put(panel.data(), kInfoDisplaySelectedCraftOffset, static_cast<void*>(nullptr));
    draw(true); assert(submissions.empty() && !g_ammunition_tooltip.active);
    // Real CFG lookup contract: tall inherits each absent field separately.
    jump(kParameterDbGetColorRva, reinterpret_cast<void*>(&get_colour));
    jump(kParameterDbGetStringRva, reinterpret_cast<void*>(&get_string));
    rectangles["infoSingleRegistryTextArea"] = {80, 95, 160, 18};
    rectangles["infoBuildRegistryTextArea"] = {310, 155, 290, 26};
    rectangles["infoBuildPhotonTorpedoesLabelTextArea"] = {40, 180, 130, 18};
    rectangles["infoBuildPhotonTorpedoesValueTextArea"] = {220, 180, 80, 24};
    rectangles["infoBuildExperienceBarArea"] = {50, 240, 301, 13};
    colours["infoSingleRegistryTextColor"] = {0.1f, 0.2f, 0.3f};
    colours["infoBuildPhotonTorpedoesBarColor"] = {0.7f, 0.8f, 0.9f};
    colours["infoBuildExperienceBarColor"] = {0.8f, 0.6f, 0.4f};
    strings["infoSingleExperienceBarSprite"] = "test_fill";
    strings["infoBuildExperienceBarBackgroundSprite"] = "test_track";
    load_panel_styles(&object_class);
    const auto registry_index = static_cast<std::size_t>(PanelElement::RegistryText);
    assert(std::fabs(g_panel_styles[1][registry_index].colour.red - 0.1f) < 0.00001f);
    assert(g_panel_styles[1][registry_index].area.x == 310);
    put(panel.data(), kInfoDisplaySelectedCraftOffset, craft.data());
    put(craft.data(), kCraftNameIndexOffset, std::int32_t(0));
    g_craft_identities.clear();
    draw(true);
    check("Repair facility", "infoBuildRegistryTextArea");
    check("Photon Torpedoes:", "infoBuildPhotonTorpedoesLabelTextArea");
    check("50%", "infoBuildPhotonTorpedoesValueTextArea");
    draw(false);
    check("Repair facility", "infoSingleRegistryTextArea");
    check("Photon Torpedoes: 50%", "photon");
    assert(g_active_info_display == nullptr && g_active_panel == 0);
    // An empty captain rectangle must not disable independently placed rows.
    const auto saved_captain = read_at<NativeRectangle>(captain.data(), 0x58, {});
    put(captain.data(), 0x58, NativeRectangle{});
    const auto saved_cfg_captain = ui.captain_rectangle;
    ui.captain_rectangle.width = ui.captain_rectangle.height = 0;
    draw(true);
    check("Repair facility", "infoBuildRegistryTextArea");
    check("50%", "infoBuildPhotonTorpedoesValueTextArea");
    ui.captain_rectangle = saved_cfg_captain;
    put(captain.data(), 0x58, saved_captain);

    // Tall native shield hover follows its own stock rectangle, independent
    // of the medium bar and of captain geometry.
    ui.shield_bar_rectangle_found = ui.builder_shield_bar_rectangle_found = true;
    ui.shield_bar_rectangle = {20, 100, 100, 10};
    ui.builder_shield_bar_rectangle = {300, 190, 200, 14};
    put(craft.data(), kCurrentShieldsOffset, 75.0f);
    put(craft.data(), kMaximumShieldsOffset, 100.0f);
    draw(true);
    assert(g_selected_status_tooltip.hit_rectangles[0].width == 199);
    assert(g_selected_status_tooltip.hit_rectangles[0].x ==
        300 + read_at<int>(panel.data(), 4, 0));
    draw(false);
    assert(g_selected_status_tooltip.hit_rectangles[0].width == 99);

    prepare_sprite_renderer();
    std::array<std::uint8_t, 0x30> enhancement{};
    std::array<std::uint8_t, 0xc0> enhancement_class{};
    put(enhancement.data(), kEnhancementClassOffset, enhancement_class.data());
    put(enhancement.data(), kEnhancementCurrentXpOffset, 25.0f);
    put(enhancement_class.data(), kEnhancementNextRankXpOffset, 100.0f);
    put(craft.data(), kCraftEnhancementOffset, enhancement.data());
    draw(true);
    assert(sprite_submissions.size() == 2);
    assert(sprite_submissions[0].sprite == track_sprite.data());
    assert(sprite_submissions[0].width == 300.0f && sprite_submissions[0].height == 12.0f);
    assert(sprite_submissions[1].sprite == fill_sprite.data());
    assert(sprite_submissions[1].width == 75.0f && sprite_submissions[1].uv_width == 0.1875f);
    assert(std::fabs(sprite_submissions[1].colour.red - 0.8f) < 0.00001f);
    assert(g_selected_status_tooltip.visible[1]);
    assert(g_selected_status_tooltip.hit_rectangles[1].width == 300.0f);
    assert(read_at<float>(fill_sprite.data(), kSpriteTextureWidthOffset, 0) == 0.75f);
    assert(std::fabs(read_at<Colour>(fill_sprite.data(), kSpriteColourOffset, {}).red - 0.3f) < 0.00001f);
    sprite_submissions.clear();
    draw(false); // no medium XP area: tall-only geometry must not leak
    assert(sprite_submissions.empty());

    g_active_panel = 1;
    g_active_info_display = panel.data();
    auto& xp_style = g_panel_styles[1][static_cast<std::size_t>(PanelElement::ExperienceBar)];
    for (float ratio : {0.0f, 1.0f, 2.0f, -1.0f}) {
        sprite_submissions.clear();
        assert(draw_ammunition_value_bar({0, 0, 200, 10}, ratio, Colour{}, 0, 0));
        assert(sprite_submissions.size() == (ratio > 0 ? 2u : 1u));
        if (ratio > 0) assert(sprite_submissions.back().width == 200.0f);
    }
    // Individually moved/resized facings remain independently drawable even
    // when the optional curved-ring sprite set is absent.
    sprite_submissions.clear();
    for (unsigned i = 0; i < 4; ++i) {
        auto& facing = g_panel_styles[1][static_cast<std::size_t>(kShieldSegments[i])];
        facing.area_found = true;
        facing.area = {100 + static_cast<int>(i) * 110, 270, 101, 11};
        facing.sprite = "test_fill";
    }
    assert(draw_directional_shield_graphic(panel.data(), craft.data(),
        {{25, 50, 75, 100}}, {{100, 100, 100, 100}}, captain.data(),
        read_at<NativeRectangle>(captain.data(), 0x58, {})));
    assert(sprite_submissions.size() == 8);
    for (unsigned i = 0; i < 4; ++i) {
        assert(g_directional_shield_tooltip.hit_rectangles[i].x ==
            100 + i * 110 + read_at<int>(panel.data(), 4, 0));
        assert(g_directional_shield_tooltip.hit_rectangles[i].width == 100);
        assert(sprite_submissions[i * 2 + 1].width == (i + 1) * 25);
    }
    xp_style.sprite = "missing_sprite";
    sprite_submissions.clear();
    assert(draw_ammunition_value_bar({0, 0, 200, 10}, 0.5f, Colour{}, 0, 0));
    assert(sprite_submissions.back().sprite == fill_sprite.data());
    put(fill_sprite.data(), kSpriteFrameListOffset, static_cast<void*>(nullptr));
    sprite_submissions.clear();
    assert(draw_ammunition_value_bar({0, 0, 200, 10}, 0.5f, Colour{}, 0, 0));
    assert(sprite_submissions.size() == 1); // stale fill not submitted, track remains
    load_panel_styles(nullptr);
    assert(!panel_style(PanelElement::ExperienceBar).area_found);
    std::puts("Craft identity panel smoke passed: native geometry, medium/tall inheritance, independent ammo parts, XP sprites, crop/colour restoration and stale-sprite rejection");
}
