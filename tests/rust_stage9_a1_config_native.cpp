// Actual frozen/current A1 unit. Engine and rendering symbols are trapped by runner.
#include A1_SOURCE
#include <cstdio>
#include <cstdlib>
#include <new>
#include <utility>

static int allocation_budget = -1;
__attribute__((noinline)) void* operator new(std::size_t size) {
    if (allocation_budget == 0) {
        allocation_budget = -1;
        throw std::bad_alloc();
    }
    if (allocation_budget > 0) --allocation_budget;
    if (void* result = std::malloc(size ? size : 1)) return result;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* value) noexcept { ::operator delete(value); }
void operator delete[](void* value, std::size_t) noexcept { ::operator delete(value); }

static const char* roots[2] = {"a1config-low", "a1config-high"};
static std::uint32_t root_count = 2;
static std::uint32_t root_reads = 0;
static bool mutate_count = false;
static bool throw_high_root = false;
static unsigned root_failure_destructions = 0;
struct HighRootFailure {
    ~HighRootFailure() { ++root_failure_destructions; }
};
static std::uint32_t A2FO_CALL count_roots() {
    const auto result = root_count;
    if (mutate_count) root_count = 0;
    return result;
}
static const char* A2FO_CALL get_root(std::uint32_t index) {
    ++root_reads;
    if (throw_high_root && index == 1) throw HighRootFailure{};
    return roots[index];
}
static void write_file(const char* path, const char* body) {
    FILE* file = std::fopen(path, "wb");
    if (!file) std::exit(11);
    const auto length = std::strlen(body);
    if (std::fwrite(body, 1, length, file) != length) std::exit(12);
    std::fclose(file);
}
static void show_area(const char* name, const a1compat::LegacyUiArea& area,
                      bool found) {
    std::printf("%s:%u:%ld,%ld,%ld,%ld\n", name, found,
        static_cast<long>(area.x), static_cast<long>(area.y),
        static_cast<long>(area.width), static_cast<long>(area.height));
}
int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    for (const char* text : {"0", " 1.5f ", "1e8", "100000001",
                             "-1", "NaN", "inf", "0x10", "1fF",
                             "1 f", "1.5e-3", "1.5e309", "", "f",
                             "0.5", "1.5", "2.5", "-0.5", "-1.5",
                             "99999999.5", "100000000.0", "100000000.1"}) {
        const std::string statement = std::string("x=") + text;
        std::int32_t value = -77;
        const bool valid = parse_starting_resource_literal(
            statement, 1, value);
        std::printf("literal:%s:%u:%ld\n", text, valid,
                    static_cast<long>(value));
    }
    LegacyStartingResourceDefaults defaults{};
    const std::string grants =
        "SHOWMETHEMONEY_CREW = 1.5f; SHOWMETHEMONEY_CREW=3;"
        "SHOWMETHEMONEY_METAL=0x10; BAD=22;/*x*/"
        "SHOWMETHEMONEY_SUPPLIES = 100000001;"
        "SHOWMETHEMONEY_DILITHIUM = 7 // comment\n;";
    const auto applied = apply_starting_resource_config(grants, defaults);
    std::printf("grant:%lu", static_cast<unsigned long>(applied));
    for (auto value : defaults.normal) std::printf(":%ld", static_cast<long>(value));
    std::puts("");
    for (const char* text : {"", "[A1Compat]\nSafeMode=true\n",
                             "[Other]\nSafeMode=on\n[A1COMPAT ]\nSafeMode=off\n",
                             "[a1compat]\r\nSafeMode = invalid\nSafeMode=yes\n",
                             "[a1compat]\nSafeMode=TRUE ;bad\n"}) {
        A1CompatSettings settings{true};
        const bool found = parse_a1compat_ini(text, settings);
        std::printf("ini:%u:%u\n", found, settings.safe_mode);
    }
    for (const auto& input : {
             std::string("controlPanelArea = 1 2 3 4\n"),
             std::string("controlPanelArea = 1 2 3 4\ncontrolPanelArea=5 6 7 8\n"),
             std::string("controlPanelArea = 1 2 3 4\ncontrolPanelArea=5 6 bad 8\n"),
             std::string("controlPanelArea = -1 2147483647 0 2\n"),
             std::string("controlPanelArea = 2147483648 1 2 3\n"),
             std::string("//controlPanelArea=9 9 9 9\ncontrolPanelArea = 3 4 5 6\n")}) {
        a1compat::LegacyUiArea area{77,77,77,77};
        const bool found = parse_gui_rectangle_assignment(
            input, "controlPanelArea", &area);
        show_area("rectangle", area, found);
    }
    const std::string long_text =
        "controlPanelArea=1 2 3 4\n" + std::string(100, 'x') + "\n";
    for (int budget = 0; budget < 1; ++budget) {
        a1compat::LegacyUiArea area{77,77,77,77};
        allocation_budget = budget;
        const bool found = parse_gui_rectangle_assignment(
            long_text, "controlPanelArea", &area);
        allocation_budget = -1;
        std::printf("rect-alloc:%d:", budget);
        show_area("", area, found);
    }
    CreateDirectoryA(roots[0], nullptr);
    CreateDirectoryA(roots[1], nullptr);
    CreateDirectoryA("a1config-high\\misc", nullptr);
    write_file("a1config-low\\RTS_CFG.h",
               "SHOWMETHEMONEY_CREW=5;SHOWMETHEMONEY_METAL=7;");
    write_file("a1config-high\\RTS_CFG.h",
               "SHOWMETHEMONEY_CREW=9;");
    write_file("a1config-low\\a1compat.ini",
               "[a1compat]\nSafeMode=true\n");
    write_file("a1config-high\\a1compat.ini",
               "[a1compat]\nSafeMode=false\n");
    A2FO_ModuleApi api{};
    api.extension_root_count = count_roots;
    api.extension_root = get_root;
    g_api = &api;
    defaults = load_starting_resource_defaults();
    std::printf("roots-default:%lu", static_cast<unsigned long>(root_reads));
    for (auto value : defaults.normal) std::printf(":%ld", static_cast<long>(value));
    for (auto value : defaults.lots) std::printf(":%ld", static_cast<long>(value));
    std::puts("");
    A1CompatSettings settings{};
    root_reads = 0;
    const bool parsed_ini = read_a1compat_settings(settings);
    std::printf("roots-ini:%u:%u:%lu\n", parsed_ini, settings.safe_mode,
                static_cast<unsigned long>(root_reads));
    for (const auto& variant : {
             std::pair<const char*, const char*>{
                 "[a1compat]\nSafeMode=true\n",
                 "[a1compat]\nSafeMode=invalid\n"},
             std::pair<const char*, const char*>{
                 "[a1compat]\nSafeMode=invalid\n",
                 "[a1compat]\nSafeMode=false\n"},
             std::pair<const char*, const char*>{
                 "[a1compat]\nSafeMode=true\n",
                 "[a1compat]\nSafeMode=invalid\nSafeMode=false\n"},
             std::pair<const char*, const char*>{
                 "[a1compat]\nSafeMode=invalid\n",
                 "[a1compat]\nSafeMode=invalid\n"}}) {
        write_file("a1config-low\\a1compat.ini", variant.first);
        write_file("a1config-high\\a1compat.ini", variant.second);
        settings = A1CompatSettings{true};
        root_reads = 0;
        const bool found = read_a1compat_settings(settings);
        std::printf("ini-roots:%u:%u:%lu\n", found, settings.safe_mode,
                    static_cast<unsigned long>(root_reads));
    }
    root_reads = 0;
    mutate_count = true;
    root_count = 2;
    const auto mutated = load_starting_resource_defaults();
    mutate_count = false;
    std::printf("roots-mutate:%lu:%ld\n",
                static_cast<unsigned long>(root_reads),
                static_cast<long>(mutated.normal[0]));
    root_count = 2;
    root_reads = 0;
    root_count = 4097;
    const auto invalid = load_starting_resource_defaults();
    std::printf("roots-invalid:%lu:%ld\n",
                static_cast<unsigned long>(root_reads),
                static_cast<long>(invalid.normal[0]));
    root_count = 2;
    const char* complete =
        "controlPanelArea=1 2 3 4\ncinematicPanelArea=5 6 7 8\n"
        "cinematicBackgroundPanelArea=9 10 11 12\n"
        "cinematicBackgroundArea=13 14 15 16\n"
        "cinematicDisplayArea=17 18 19 20\n"
        "infoSingleBackgroundArea=1 1 2 2\n"
        "infoSingleBackground_0=2 2 3 3\n"
        "infoMultiShipIcon_0=3 3 4 4\n";
    write_file("a1config-low\\gui.cfg", complete);
    write_file("a1config-high\\misc\\gui.cfg", complete);
    LegacyGameplayConfigAreas areas{};
    root_reads = 0;
    bool active = load_active_legacy_gameplay_config("gui.cfg", &areas);
    std::printf("gui-root:%u:%lu:%s:%lu:%u:%u\n", active,
                static_cast<unsigned long>(root_reads),
                areas.source_path.c_str(),
                static_cast<unsigned long>(areas.single_background_piece_count),
                areas.has_single_background, areas.has_multi_ship_layout);
    write_file("a1config-high\\gui.cfg", "controlPanelArea=1 2 3 4\n");
    areas = {};
    root_reads = 0;
    active = load_active_legacy_gameplay_config("gui.cfg", &areas);
    std::printf("gui-incomplete:%u:%lu:%s\n", active,
                static_cast<unsigned long>(root_reads),
                areas.source_path.c_str());
    const auto expanded_gui = [](bool gaps, bool primary) {
        std::string text =
            "controlPanelArea=1 2 3 4\ncinematicPanelArea=5 6 7 8\n"
            "cinematicBackgroundPanelArea=9 10 11 12\n"
            "cinematicBackgroundArea=13 14 15 16\n"
            "cinematicDisplayArea=17 18 19 20\n"
            "cinematicMenuButtonArea=21 22 23 24\n"
            "cinematicCommButtonArea=25 26 27 28\n"
            "infoSingleWireframeIconArea=29 30 31 32\n"
            "infoSingleRaceIconArea=33 34 35 36\n"
            "infoSingleRaceIconDisplayArea=37 38 39 40\n"
            "infoSingleBackgroundArea=41 42 43 44\n"
            "infoSingleNameTextBackgroundArea=45 46 47 48\n"
            "infoSingleCrewTextBackgroundArea=49 50 51 52\n"
            "infoSingleOfficerTextBackgroundArea=53 54 55 56\n"
            "infoSingleClassTextArea=57 58 59 60\n"
            "infoSingleNameTextArea=61 62 63 64\n"
            "infoSingleCrewTextInformationArea=65 66 67 68\n"
            "infoSingleOfficerTextInformationArea=69 70 71 72\n"
            "infoSingleCrewTextLabelArea=73 74 75 76\n"
            "infoSingleOfficerTextLabelArea=77 78 79 80\n"
            "infoSingleConstructionBarArea=81 82 83 84\n"
            "infoSingleEnergyBarArea=85 86 87 88\n"
            "infoMultiShieldBarArea=89 90 91 92\n"
            "infoMultiEnergyBarArea=93 94 95 96\n"
            "infoMultiCrewDotArea=97 98 99 100\n"
            "infoMultiWireframeIconArea=101 102 103 104\n";
        if (primary) {
            text += "infoSingleClassTextInformationArea=105 106 107 108\n";
            text += "infoSingleNameTextInformationArea=109 110 111 112\n";
        }
        for (std::size_t index = 0; index < 16; ++index) {
            if (gaps && index == 5) continue;
            text += "infoSingleBackground_" + std::to_string(index) +
                "=" + std::to_string(200 + index) + " 1 2 3\n";
        }
        for (std::size_t index = 0; index < 8; ++index) {
            if (gaps && index == 3) continue;
            text += "infoMultiShipIcon_" + std::to_string(index) +
                "=" + std::to_string(300 + index) + " 1 2 3\n";
        }
        return text;
    };
    for (const auto& variant : {
             std::pair<const char*, std::string>{"gui-all", expanded_gui(false, false)},
             std::pair<const char*, std::string>{"gui-gaps", expanded_gui(true, false)},
             std::pair<const char*, std::string>{"gui-primary", expanded_gui(false, true)}}) {
        write_file("a1config-high\\gui.cfg", variant.second.c_str());
        areas = {};
        root_reads = 0;
        active = load_active_legacy_gameplay_config("gui.cfg", &areas);
        std::printf("%s:%u:%lu:%lu",
                    variant.first, active, static_cast<unsigned long>(root_reads),
                    static_cast<unsigned long>(areas.single_background_piece_count));
        for (bool flag : {areas.has_cinematic_buttons, areas.has_single_wireframe,
                          areas.has_single_race_icon, areas.has_single_background,
                          areas.has_single_name_background,
                          areas.has_single_crew_background,
                          areas.has_single_officer_background,
                          areas.has_single_identity_layout,
                          areas.has_single_identity_labels,
                          areas.has_single_construction_bar,
                          areas.has_single_energy_bar,
                          areas.has_multi_ship_layout})
            std::printf(":%u", flag);
        std::printf(":%ld:%ld", static_cast<long>(areas.single_class_text.x),
                    static_cast<long>(areas.single_name_text.x));
        for (const auto& piece : areas.single_background_pieces)
            std::printf(":%ld", static_cast<long>(piece.x));
        for (const auto& tile : areas.multi_ship_tiles)
            std::printf(":%ld", static_cast<long>(tile.x));
        std::puts("");
    }
    root_reads = 0;
    root_failure_destructions = 0;
    throw_high_root = true;
    areas.source_path = "preserved";
    active = load_active_legacy_gameplay_config("gui.cfg", &areas);
    throw_high_root = false;
    std::printf("gui-high-root-exception:%u:%lu:%u:%s\n", active,
                static_cast<unsigned long>(root_reads),
                root_failure_destructions, areas.source_path.c_str());
    g_api = nullptr;
}
