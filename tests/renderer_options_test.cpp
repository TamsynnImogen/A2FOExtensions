// Keep synthetic hook callbacks distinct from the real Rust archive symbols.
#define a2fo_rs_windows_hook_install fixture_windows_hook_install
#define a2fo_rs_windows_patch_jump fixture_windows_patch_jump
#define a2fo_rs_windows_patch_call fixture_windows_patch_call
#define a2fo_rs_windows_patch_bytes fixture_windows_patch_bytes
// Headless regression for the Fleet Ops Graphics Options integration.
#include <windows.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <map>
#include <string>
#include <vector>

BOOL WINAPI fake_IsWindow(HWND);
LONG_PTR WINAPI fake_SetWindowLongPtrA(HWND, int, LONG_PTR);
LRESULT WINAPI fake_SendMessageA(HWND, UINT, WPARAM, LPARAM);
UINT_PTR WINAPI fake_SetTimer(HWND, UINT_PTR, UINT, TIMERPROC);
BOOL WINAPI fake_KillTimer(HWND, UINT_PTR);

#define IsWindow fake_IsWindow
#undef SetWindowLongPtrA
#define SetWindowLongPtrA fake_SetWindowLongPtrA
#define SendMessageA fake_SendMessageA
#define SetTimer fake_SetTimer
#define KillTimer fake_KillTimer
#include "../core/renderer_options.cpp"
#undef IsWindow
#undef SetWindowLongPtrA
#undef SendMessageA
#undef SetTimer
#undef KillTimer

namespace {
std::vector<std::string> lines;
std::vector<std::unique_ptr<std::uint8_t[]>> controls;
std::map<void*, HWND> windows;
std::map<HWND, int> selection;
std::map<void*, std::string> captions;
int installed_hooks = 0;
int original_shows = 0;
int constructor_calls = 0;
int effect_set_bounds_calls = 0;
int native_layout_resets = 0;
void* native_bump_fixture = nullptr;
const auto form_hwnd = reinterpret_cast<HWND>(0x1234);
std::uint32_t checkbox_class = 0x12345678;
std::uint32_t label_class = 0x23456789;
std::uint32_t combo_class = 0x3456789a;

void log_line(const std::string& line) { lines.push_back(line); }
void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
void set_pointer(void* object, std::size_t offset, void* value) {
    *reinterpret_cast<void**>(static_cast<std::uint8_t*>(object) + offset) = value;
}
void set_int(void* object, std::size_t offset, int value) {
    *reinterpret_cast<int*>(static_cast<std::uint8_t*>(object) + offset) = value;
}
int get_int(void* object, std::size_t offset) {
    return *reinterpret_cast<int*>(static_cast<std::uint8_t*>(object) + offset);
}
void require_inside(void* control, int width, int height, const char* name) {
    const int left = get_int(control, a2fo::kControlLeftOffset);
    const int top = get_int(control, a2fo::kControlTopOffset);
    const int control_width = get_int(control, a2fo::kControlWidthOffset);
    const int control_height = get_int(control, a2fo::kControlHeightOffset);
    if (left < 0 || top < 0 || control_width <= 0 || control_height <= 0 ||
        left + control_width > width || top + control_height > height) {
        std::fprintf(stderr, "FAIL: %s bounds (%d,%d %dx%d) outside %dx%d\n",
                     name, left, top, control_width, control_height,
                     width, height);
        std::exit(1);
    }
}
void original_show(void*, void*) {
    ++original_shows;
    if (original_shows == 2 && a2fo::g_emissive_checkbox &&
        a2fo::g_specular_checkbox) {
        // Model a late native layout pass moving the existing checkboxes.
        const int top = get_int(native_bump_fixture, a2fo::kControlTopOffset);
        set_int(a2fo::g_emissive_checkbox, a2fo::kControlTopOffset, top);
        set_int(a2fo::g_specular_checkbox, a2fo::kControlTopOffset, top);
        ++native_layout_resets;
    }
}
void require_effect_placement(void* bump, int gamma_top, int expected_top) {
    void* emissive = a2fo::g_emissive_checkbox;
    void* specular = a2fo::g_specular_checkbox;
    const int bump_bottom = get_int(bump, a2fo::kControlTopOffset) +
                            get_int(bump, a2fo::kControlHeightOffset);
    const int emissive_top = get_int(emissive, a2fo::kControlTopOffset);
    const int specular_top = get_int(specular, a2fo::kControlTopOffset);
    require(emissive_top == expected_top && specular_top == expected_top,
            "effect controls follow native bump top");
    require(emissive_top >= bump_bottom + 8 &&
            specular_top >= bump_bottom + 8,
            "effect controls clear Bump Mapping by eight pixels");
    require(emissive_top + get_int(emissive, a2fo::kControlHeightOffset) <=
                gamma_top &&
            specular_top + get_int(specular, a2fo::kControlHeightOffset) <=
                gamma_top,
            "effect controls clear Gamma label");
    const int emissive_right = get_int(emissive, a2fo::kControlLeftOffset) +
                               get_int(emissive, a2fo::kControlWidthOffset);
    require(emissive_right <= get_int(specular, a2fo::kControlLeftOffset),
            "effect controls do not overlap horizontally");
}
void original_checked(void* control, void* state) {
    static_cast<std::uint8_t*>(control)[a2fo::kJvgCheckBoxCheckedOffset] =
        reinterpret_cast<std::uintptr_t>(state) != 0;
}
std::string temp_root() {
    char path[MAX_PATH]{};
    require(GetTempPathA(MAX_PATH, path) != 0, "temp path");
    char name[MAX_PATH]{};
    require(GetTempFileNameA(path, "rot", 0, name) != 0, "temp filename");
    DeleteFileA(name);
    require(CreateDirectoryA(name, nullptr) != 0, "temp directory");
    return name;
}
} // namespace

BOOL WINAPI fake_IsWindow(HWND window) {
    if (window == form_hwnd) return TRUE;
    for (const auto& item : windows) if (item.second == window) return TRUE;
    return FALSE;
}
LONG_PTR WINAPI fake_SetWindowLongPtrA(HWND window, int index, LONG_PTR) {
    return window == form_hwnd && index == GWLP_WNDPROC
        ? reinterpret_cast<LONG_PTR>(&DefWindowProcA) : 0;
}
LRESULT WINAPI fake_SendMessageA(HWND window, UINT message, WPARAM wparam, LPARAM) {
    if (message == CB_GETCURSEL) return selection[window];
    if (message == CB_SETCURSEL) return selection[window] = static_cast<int>(wparam);
    if (message == CB_GETCOUNT) return 2;
    return 0;
}
UINT_PTR WINAPI fake_SetTimer(HWND, UINT_PTR id, UINT, TIMERPROC) { return id; }
BOOL WINAPI fake_KillTimer(HWND, UINT_PTR) { return TRUE; }

namespace a2fo {
std::vector<GameMonitorDescriptor> enumerate_game_monitors() { return {}; }
std::string game_monitor_label(const GameMonitorDescriptor&) { return {}; }
bool read_configured_game_monitor(const std::string&, std::string&) { return false; }
bool write_configured_game_monitor(const std::string&, const std::string&) { return false; }
GameMonitorResolution resolve_configured_game_monitor(
    const std::string&, const std::vector<GameMonitorDescriptor>&, int,
    std::string*) { return {}; }

#if A2FO_DIRECT_HOOK_BINDINGS
extern "C" std::uint8_t a2fo_rs_windows_hook_install(void* target, void*, std::size_t length,
                         const std::uint8_t* expected, A2foRsInlineHook* hook) {
#else
bool install_inline_hook(void* target, void*, std::size_t length,
                         const std::uint8_t* expected, InlineHook& hook) {
#endif
    require(std::memcmp(target, expected, length) == 0, "hook signature");
    ++installed_hooks;
#if A2FO_DIRECT_HOOK_BINDINGS
    hook->target = target;
    hook->gateway = installed_hooks % 2
        ? reinterpret_cast<void*>(&original_checked)
        : reinterpret_cast<void*>(&original_show);
    hook->length = length;
#else
    hook.target = target;
    hook.gateway = installed_hooks % 2
        ? reinterpret_cast<void*>(&original_checked)
        : reinterpret_cast<void*>(&original_show);
    hook.length = length;
#endif
    return true;
}

extern "C" void a2fo_graphics_options_form_show_bridge() {}
extern "C" void a2fo_jvg_checkbox_set_checked_bridge() {}
extern "C" void a2fo_renderer_combo_change_bridge() {}
extern "C" void a2fo_call_delphi_set_bounds(void* function, void* control,
                                              int left, int top, int width,
                                              int height) {
    require(function == at(0x0c4b80), "checked TControl.SetBounds RVA");
    require(control == g_emissive_checkbox || control == g_specular_checkbox,
            "SetBounds called only for effect checkboxes");
    set_int(control, kControlLeftOffset, left);
    set_int(control, kControlTopOffset, top);
    set_int(control, kControlWidthOffset, width);
    set_int(control, kControlHeightOffset, height);
    ++effect_set_bounds_calls;
}
extern "C" void* a2fo_call_delphi_constructor(void* function, void* klass,
                                                void*) {
    require(function == at(kJvgCheckBoxCreateRva) ||
            function == at(kCustomLabelCreateRva) ||
            function == at(kJvCustomHTComboBoxCreateRva), "known constructor RVA");
    auto memory = std::make_unique<std::uint8_t[]>(0x400);
    std::memset(memory.get(), 0, 0x400);
    set_pointer(memory.get(), 0, klass);
    if (function == at(kJvCustomHTComboBoxCreateRva)) {
        // TCustomComboBox.Items must refer to a readable TStrings fixture.
        static std::uint32_t strings_vmt = 0x456789ab;
        set_pointer(memory.get(), kComboItemsOffset, &strings_vmt);
    }
    void* control = memory.get();
    controls.push_back(std::move(memory));
    ++constructor_calls;
    return control;
}
extern "C" void* a2fo_call_delphi_one_register(void* function, void* arg) {
    if (function == reinterpret_cast<void*>(&original_show)) {
        original_show(arg, nullptr);
    } else if (function == at(kLongStringClearRva)) {
        *static_cast<char**>(arg) = nullptr;
    } else if (function == at(kWinControlGetHandleRva)) {
        auto found = windows.find(arg);
        if (found != windows.end()) return found->second;
        HWND window = reinterpret_cast<HWND>(0x3000 + windows.size());
        windows.emplace(arg, window);
        return window;
    }
    return nullptr;
}
extern "C" void* a2fo_call_delphi_two_registers(void* function, void* arg,
                                                  void* second) {
    if (function == reinterpret_cast<void*>(&original_show)) {
        original_show(arg, second);
    } else if (function == reinterpret_cast<void*>(&original_checked)) {
        original_checked(arg, second);
    } else if (function == at(kComponentInsertRva)) {
        set_pointer(second, kComponentOwnerOffset, arg);
    } else if (function == at(kControlSetParentRva)) {
        set_pointer(arg, kControlParentOffset, second);
    } else if (function == at(kControlSetVisibleRva)) {
        static_cast<std::uint8_t*>(arg)[0x10] = second != nullptr;
    } else if (function == at(kLongStringFromPcharRva)) {
        *static_cast<char**>(arg) = static_cast<char*>(second);
    } else if (function == at(kControlSetTextRva)) {
        captions[arg] = second ? static_cast<char*>(second) : "";
        if (arg == g_emissive_checkbox || arg == g_specular_checkbox) {
            // Simulated native skin/caption side effect, not a claimed cause
            // of the observed overlap. The final SetBounds must repair it.
            set_int(arg, kControlTopOffset,
                    get_int(native_bump_fixture, kControlTopOffset));
            ++native_layout_resets;
        }
    } else if (function == at(kCustomComboSetItemIndexRva)) {
        selection[windows[arg]] = static_cast<int>(reinterpret_cast<std::uintptr_t>(second));
    }
    return nullptr;
}
} // namespace a2fo

int main(int argc, char** argv) {
    require(argc == 3 || argc == 4,
            "expected two FleetOpsHook.dll paths and optional --clean-layout");
    const bool clean_layout = argc == 4;
    require(!clean_layout || std::strcmp(argv[3], "--clean-layout") == 0,
            "unknown layout option");
    const int logical_width = clean_layout ? 800 : 1200;
    const int logical_height = clean_layout ? 600 : 900;
    const int bump_left = clean_layout ? 440 : 660;
    const int bump_top = clean_layout ? 288 : 432;
    const int combo_height = clean_layout ? 22 : 30;
    require(a2fo::kFormHandleOffset == 0x1b4, "TWinControl handle offset");
    const std::string data_root = temp_root();
    for (int input = 1; input <= 2; ++input) {
        HMODULE image = LoadLibraryExA(argv[input], nullptr,
                                       DONT_RESOLVE_DLL_REFERENCES);
        require(image != nullptr, "map FleetOpsHook.dll without execution");
        require(a2fo::install_renderer_options(image, data_root, log_line),
                "FleetOpsHook Graphics signatures");
        require(a2fo::install_renderer_options(image, data_root, log_line),
                "repeat installation accepted without duplicate hooks");
        require(installed_hooks == input * 2,
                "repeat installation installed no extra hooks");
        a2fo::g_form_show_hook = {};
        a2fo::g_jvg_checkbox_set_checked_hook = {};
        FreeLibrary(image);
    }
    require(installed_hooks == 4, "both DLLs passed complete signature gate");

    std::vector<std::uint8_t> foreign(0x200000);
    require(!a2fo::install_renderer_options(
                reinterpret_cast<HMODULE>(foreign.data()), data_root, log_line),
            "foreign image rejected");
    require(installed_hooks == 4, "foreign rejection happened before hooks");

    std::array<std::uint8_t, 0x500> form{};
    std::array<std::uint8_t, 0x400> bump{};
    std::array<std::uint8_t, 0x400> native_label{};
    std::array<std::uint8_t, 0x400> native_combo{};
    set_pointer(bump.data(), 0, &checkbox_class);
    set_pointer(native_label.data(), 0, &label_class);
    set_pointer(native_combo.data(), 0, &combo_class);
    set_int(bump.data(), a2fo::kControlLeftOffset, bump_left);
    set_int(bump.data(), a2fo::kControlTopOffset, bump_top);
    set_int(bump.data(), a2fo::kControlWidthOffset, 297);
    set_int(bump.data(), a2fo::kControlHeightOffset, 17);
    set_int(native_combo.data(), a2fo::kControlWidthOffset,
            clean_layout ? 273 : 408);
    set_int(native_combo.data(), a2fo::kControlHeightOffset, combo_height);
    set_pointer(form.data(), a2fo::kBumpMappingCheckOffset, bump.data());
    native_bump_fixture = bump.data();
    set_pointer(form.data(), a2fo::kFirstNativeLabelOffset, native_label.data());
    set_pointer(form.data(), a2fo::kPrimaryDeviceComboOffset, native_combo.data());
    set_pointer(form.data(), 0x1b4, form_hwnd);
    set_pointer(form.data(), 0x1c4, reinterpret_cast<void*>(0x9999));
    a2fo::g_form_show_hook.gateway = reinterpret_cast<void*>(&original_show);
    a2fo::g_jvg_checkbox_set_checked_hook.gateway =
        reinterpret_cast<void*>(&original_checked);
    a2fo::a2fo_graphics_options_form_show_hook_cpp(form.data(), nullptr);
    require(original_shows == 1, "original FormShow called");
    require(constructor_calls == 7, "effect and selector controls created at true HWND");
    require(a2fo::g_emissive_checkbox && a2fo::g_specular_checkbox,
            "effect controls retained");
    require(a2fo::g_renderer_label && a2fo::g_renderer_combo_control &&
            a2fo::g_restart_label, "Renderer selector controls created");
    require(captions[a2fo::g_renderer_label] == "Renderer", "Renderer label");
    require(captions[a2fo::g_emissive_checkbox] == "Emissive Maps" &&
            captions[a2fo::g_specular_checkbox] == "Specular Maps",
            "effect labels");
    require(a2fo::field<void*>(a2fo::g_emissive_checkbox,
                               a2fo::kComponentOwnerOffset) == form.data(),
            "effect control owned by form");
    require(a2fo::field<void*>(a2fo::g_specular_checkbox,
                               a2fo::kControlParentOffset) == form.data(),
            "effect control parented to form");
    require(a2fo::field<void*>(a2fo::g_renderer_combo_control,
                               a2fo::kComponentOwnerOffset) == form.data() &&
            a2fo::field<void*>(a2fo::g_renderer_combo_control,
                               a2fo::kControlParentOffset) == form.data(),
            "Renderer selector owned and parented");
    require(a2fo::g_renderer_combo_window &&
            fake_IsWindow(a2fo::g_renderer_combo_window),
            "Renderer selector has synthetic HWND");
    require(static_cast<std::uint8_t*>(a2fo::g_renderer_combo_control)[0x10] &&
            static_cast<std::uint8_t*>(a2fo::g_emissive_checkbox)[0x10] &&
            static_cast<std::uint8_t*>(a2fo::g_specular_checkbox)[0x10],
            "Renderer and effect controls made visible");
    require(native_layout_resets == 2,
            "skin/caption fixture reset both effect positions");
    require(effect_set_bounds_calls == 2,
            "both effect controls received final native SetBounds");
    require_effect_placement(bump.data(), clean_layout ? 336 : 504,
                             bump_top + 17 + 8);
    require_inside(a2fo::g_renderer_label, logical_width, logical_height,
                   "Renderer label");
    require_inside(a2fo::g_renderer_combo_control, logical_width,
                   logical_height, "Renderer combo");
    require_inside(a2fo::g_restart_label, logical_width, logical_height,
                   "restart label");
    require_inside(a2fo::g_emissive_checkbox, logical_width, logical_height,
                   "Emissive Maps checkbox");
    require_inside(a2fo::g_specular_checkbox, logical_width, logical_height,
                   "Specular Maps checkbox");
    require(get_int(a2fo::g_emissive_checkbox, a2fo::kControlLeftOffset) ==
                bump_left &&
            get_int(a2fo::g_specular_checkbox, a2fo::kControlLeftOffset) ==
                bump_left + a2fo::kEffectWidth + a2fo::kEffectGap,
            "effect controls anchored to native Bump Mapping");
    require(get_int(a2fo::g_renderer_combo_control,
                    a2fo::kControlHeightOffset) == combo_height,
            "Renderer combo uses positive native combo height");
    require(get_int(a2fo::g_monitor_combo_control,
                    a2fo::kControlHeightOffset) == combo_height,
            "monitor combo uses positive native combo height");
    // Clean and Roots DFM GraphicDetail/GammaCorrection slider bottoms.
    const int slider_bottom = clean_layout ? 400 : 572;
    require(get_int(a2fo::g_restart_label, a2fo::kControlTopOffset) >
                slider_bottom,
            "restart label is below native sliders");
    set_int(bump.data(), a2fo::kControlTopOffset, bump_top + 2);
    a2fo::a2fo_graphics_options_form_show_hook_cpp(form.data(), nullptr);
    require(constructor_calls == 7, "second FormShow reuses controls");
    require(original_shows == 2, "second original FormShow called");
    require(native_layout_resets == 3, "second native layout reset simulated");
    require(effect_set_bounds_calls == 4,
            "reuse reapplies final bounds to both effect controls");
    require_effect_placement(bump.data(), clean_layout ? 336 : 504,
                             bump_top + 2 + 17 + 8);

    a2fo::a2fo_jvg_checkbox_set_checked_hook_cpp(a2fo::g_emissive_checkbox,
                                                  nullptr);
    a2fo::a2fo_jvg_checkbox_set_checked_hook_cpp(a2fo::g_specular_checkbox,
                                                  nullptr);
    require(GetPrivateProfileIntA("Effects", "EmissiveMaps", 9,
                                   a2fo::renderer_ini_path().c_str()) == 0,
            "emissive toggle saved");
    require(GetPrivateProfileIntA("Effects", "SpecularMaps", 9,
                                   a2fo::renderer_ini_path().c_str()) == 0,
            "specular toggle saved");
    a2fo::a2fo_jvg_checkbox_set_checked_hook_cpp(bump.data(), nullptr);
    require(GetPrivateProfileIntA("Effects", "BumpMapping", 9,
                                   a2fo::renderer_ini_path().c_str()) == 9,
            "foreign checkbox did not persist");
    require(GetPrivateProfileIntA("Effects", "EmissiveMaps", 9,
                                   a2fo::renderer_ini_path().c_str()) == 0 &&
            GetPrivateProfileIntA("Effects", "SpecularMaps", 9,
                                   a2fo::renderer_ini_path().c_str()) == 0,
            "foreign checkbox left effect settings untouched");
    require(!a2fo::renderer_emissive_maps_enabled() &&
            !a2fo::renderer_specular_maps_enabled(), "cached effects updated");
    a2fo::a2fo_jvg_checkbox_set_checked_hook_cpp(a2fo::g_emissive_checkbox,
                                                  reinterpret_cast<void*>(1));
    a2fo::a2fo_jvg_checkbox_set_checked_hook_cpp(a2fo::g_specular_checkbox,
                                                  reinterpret_cast<void*>(1));
    require(GetPrivateProfileIntA("Effects", "EmissiveMaps", 9,
                                   a2fo::renderer_ini_path().c_str()) == 1 &&
            GetPrivateProfileIntA("Effects", "SpecularMaps", 9,
                                   a2fo::renderer_ini_path().c_str()) == 1,
            "both effect enable toggles saved");
    require(a2fo::renderer_emissive_maps_enabled() &&
            a2fo::renderer_specular_maps_enabled(), "enabled effects cached");
    std::printf("renderer options: %dx%d layout, signatures, foreign guard, HWND, effect lifecycle and persistence passed\n",
                logical_width, logical_height);
    return 0;
}
