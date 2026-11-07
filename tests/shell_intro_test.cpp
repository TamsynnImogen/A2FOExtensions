// Real movie hook implementation with a synthetic USER32/VCL host. No HWNDs
// are created and no original game, VCL or D3D methods are executed.
#include "../modules/A2FOShellDisplay/bink_video.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>

BOOL WINAPI host_IsWindow(HWND);
BOOL WINAPI host_GetClientRect(HWND, LPRECT);
LONG_PTR WINAPI host_GetWindowLongPtrA(HWND, int);
LONG_PTR WINAPI host_SetWindowLongPtrA(HWND, int, LONG_PTR);
BOOL WINAPI host_AdjustWindowRectEx(LPRECT, DWORD, BOOL, DWORD);
HMONITOR WINAPI host_MonitorFromWindow(HWND, DWORD);
BOOL WINAPI host_GetMonitorInfoA(HMONITOR, LPMONITORINFO);
BOOL WINAPI host_SetWindowPos(HWND, HWND, int, int, int, int, UINT);
BOOL WINAPI host_EnumDisplayDevicesA(LPCSTR, DWORD, PDISPLAY_DEVICEA, DWORD);
BOOL WINAPI host_EnumDisplaySettingsA(LPCSTR, DWORD, DEVMODEA*);
#define IsWindow host_IsWindow
#define GetClientRect host_GetClientRect
#undef GetWindowLongPtrA
#undef SetWindowLongPtrA
#define GetWindowLongPtrA host_GetWindowLongPtrA
#define SetWindowLongPtrA host_SetWindowLongPtrA
#define AdjustWindowRectEx host_AdjustWindowRectEx
#define MonitorFromWindow host_MonitorFromWindow
#define GetMonitorInfoA host_GetMonitorInfoA
#define SetWindowPos host_SetWindowPos
#define EnumDisplayDevicesA host_EnumDisplayDevicesA
#define EnumDisplaySettingsA host_EnumDisplaySettingsA
#include "../modules/A2FOShellDisplay/bink_video.cpp"

namespace movie = a2fo::shell_display;
namespace {
HMODULE fo{}, armada{};
HWND window = reinterpret_cast<HWND>(0x1234);
LONG_PTR style = WS_CAPTION | WS_SYSMENU, ex_style = WS_EX_WINDOWEDGE;
int client_width = 1200, client_height = 900, pos_x{}, pos_y{};
unsigned moves{}, setter_calls{}, getter_calls{}, play_calls{};
unsigned display_device_calls{}, display_settings_calls{};
bool fail_size{}, play_result = true;
bool fail_monitor{};
RECT monitor_bounds{0, 0, 1920, 1080};
void* played_object{};
std::string logs;
std::vector<void*> calls;
unsigned fail_call{}, byte_calls{}, fail_byte_call{};
std::map<void*, std::vector<std::uint8_t>> simulated_bytes;
std::array<std::uint8_t, 0x1b8> form{};
std::array<std::uint8_t, 0xb0> settings{};
std::array<std::uint8_t, 8> original_radar_uv{};

template<class T> void put(void* target, T value) { std::memcpy(target, &value, sizeof(value)); }
void assert_radar_uv_unchanged() {
    assert(std::memcmp(movie::at(armada, movie::kArmadaBinkTextureUvScaleRva),
                       original_radar_uv.data(), original_radar_uv.size()) == 0);
    assert(simulated_bytes.count(movie::at(armada, movie::kArmadaBinkTextureUvScaleRva)) == 0);
}
void assert_simulated_bytes_restored() {
    for (const auto& entry : simulated_bytes)
        assert(std::memcmp(entry.first, entry.second.data(), entry.second.size()) == 0);
    assert_radar_uv_unchanged();
}
void seed_mapped_uv_instruction(std::uintptr_t instruction_rva,
                                std::uintptr_t global_rva) {
    auto* instruction = static_cast<std::uint8_t*>(movie::at(armada, instruction_rva));
    assert(instruction[0] == 0xd8 && instruction[1] == 0x0d);
    std::uint32_t operand{};
    std::memcpy(&operand, instruction + 2, sizeof(operand));
    const auto mapped = reinterpret_cast<std::uintptr_t>(movie::at(armada, global_rva));
    assert(operand == 0x400000u + global_rva || operand == mapped);
    // DONT_RESOLVE_DLL_REFERENCES may leave this instruction's absolute operand
    // at its preferred base. Normalize only the private, data-only mapping.
    const std::uint32_t mapped_operand = static_cast<std::uint32_t>(mapped);
    DWORD old{}, ignored{};
    assert(VirtualProtect(instruction, 6, PAGE_EXECUTE_READWRITE, &old));
    put(instruction + 2, mapped_operand);
    assert(VirtualProtect(instruction, 6, old, &ignored));
}
void assert_private_bink_uv() {
    auto* size = movie::at(armada, movie::kArmadaBinkTextureSizeRva);
    const auto width_offset = movie::kArmadaBinkTextureWidthUvInstructionRva -
                              movie::kArmadaBinkTextureSizeRva;
    const auto height_offset = movie::kArmadaBinkTextureHeightUvInstructionRva -
                               movie::kArmadaBinkTextureSizeRva;
    const auto& patched = simulated_bytes.at(size);
    const auto* original = static_cast<const std::uint8_t*>(size);
    assert(simulated_bytes.size() == 1 && byte_calls == 1 &&
           patched.size() == movie::kBinkTextureSetupPatchSize &&
           height_offset + 6 <= patched.size());
    assert(std::memcmp(patched.data(),
                       movie::kReplacementArmadaBinkTextureSize,
                       sizeof(movie::kReplacementArmadaBinkTextureSize)) == 0);
    const auto assert_operand = [](const std::uint8_t* bytes,
                                   const float* expected, float reciprocal) {
        assert(bytes[0] == 0xd8 && bytes[1] == 0x0d);
        std::uint32_t address{};
        std::memcpy(&address, bytes + 2, sizeof(address));
        assert(address == reinterpret_cast<std::uintptr_t>(expected));
        assert(*expected == reciprocal);
    };
    assert_operand(patched.data() + width_offset, &movie::kBinkTextureWidthReciprocal,
                   1.0f / 512.0f);
    assert_operand(patched.data() + height_offset, &movie::kBinkTextureHeightReciprocal,
                   1.0f / 256.0f);
    for (std::size_t i = 0; i < patched.size(); ++i) {
        const bool changed_site = i < sizeof(movie::kReplacementArmadaBinkTextureSize) ||
            (i >= width_offset && i < width_offset + 6) ||
            (i >= height_offset && i < height_offset + 6);
        if (!changed_site) assert(patched[i] == original[i]);
    }
    assert(&movie::kBinkTextureWidthReciprocal !=
           reinterpret_cast<const float*>(movie::at(armada, movie::kArmadaBinkTextureUvScaleRva + 4)));
    assert(&movie::kBinkTextureHeightReciprocal !=
           reinterpret_cast<const float*>(movie::at(armada, movie::kArmadaBinkTextureUvScaleRva)));
    assert_radar_uv_unchanged();
}
void A2FO_CALL log(const char*, const char* message) { logs += message; logs += '\n'; }
void* A2FO_CALL get_fo() { return fo; }
bool A2FO_CALL patch(void* target, void*, const std::uint8_t* expected, std::size_t size) {
    assert(std::memcmp(target, expected, size) == 0);
    calls.push_back(target);
    return calls.size() != fail_call; // never execute or modify the images
}
bool A2FO_CALL bytes(void* target, const std::uint8_t* replacement, const std::uint8_t* expected, std::size_t size) {
    assert(target == movie::at(armada, movie::kArmadaBinkTextureSizeRva) &&
           size == movie::kBinkTextureSetupPatchSize);
    ++byte_calls;
    auto& current = simulated_bytes[target];
    if (current.empty()) {
        const auto* original = static_cast<const std::uint8_t*>(target);
        current.assign(original, original + size);
    }
    assert(current.size() == size && std::memcmp(current.data(), expected, size) == 0);
    if (byte_calls == fail_byte_call) return false;
    current.assign(replacement, replacement + size);
    return true;
}
HWND __cdecl fake_window() { ++getter_calls; return window; }
bool __attribute__((regparm(1))) fake_play(void* object) {
    ++play_calls; played_object = object; return play_result;
}
void selection(int width, int height, bool windowed) {
    movie::presentation::clear();
    put(settings.data() + 0x30, width);
    put(settings.data() + 0x34, height);
    put(settings.data() + 0x38, 32);
    settings[0x72] = windowed;
}
}

BOOL WINAPI host_IsWindow(HWND value) { return value == window; }
BOOL WINAPI host_GetClientRect(HWND value, LPRECT rect) {
    if (value != window) return FALSE;
    *rect = RECT{0, 0, client_width, client_height}; return TRUE;
}
LONG_PTR WINAPI host_GetWindowLongPtrA(HWND value, int index) {
    assert(value == window); return index == GWL_STYLE ? style : ex_style;
}
LONG_PTR WINAPI host_SetWindowLongPtrA(HWND value, int index, LONG_PTR bits) {
    assert(value == window);
    auto& field = index == GWL_STYLE ? style : ex_style;
    auto old = field; field = bits; return old;
}
BOOL WINAPI host_AdjustWindowRectEx(LPRECT rect, DWORD, BOOL, DWORD) {
    rect->left -= 4; rect->right += 4; rect->top -= 24; rect->bottom += 4; return TRUE;
}
HMONITOR WINAPI host_MonitorFromWindow(HWND, DWORD) { return reinterpret_cast<HMONITOR>(1); }
BOOL WINAPI host_GetMonitorInfoA(HMONITOR, LPMONITORINFO info) {
    if (fail_monitor) return FALSE;
    info->rcMonitor = monitor_bounds;
    info->rcWork = RECT{0, 0, 1920, 1040}; return TRUE;
}
BOOL WINAPI host_SetWindowPos(HWND value, HWND, int x, int y, int width, int height, UINT flags) {
    assert(value == window);
    assert(flags & SWP_NOACTIVATE);
    assert(flags & SWP_NOZORDER);
    ++moves; pos_x = x; pos_y = y;
    if (!fail_size) {
        const bool framed = (style & WS_CAPTION) != 0;
        client_width = width - (framed ? 8 : 0);
        client_height = height - (framed ? 28 : 0);
    }
    return TRUE;
}
BOOL WINAPI host_EnumDisplayDevicesA(LPCSTR, DWORD index, PDISPLAY_DEVICEA display,
                                     DWORD) {
    ++display_device_calls;
    if (index != 0 || !display) return FALSE;
    std::strcpy(display->DeviceName, "\\\\.\\DISPLAY1");
    return TRUE;
}
BOOL WINAPI host_EnumDisplaySettingsA(LPCSTR device, DWORD mode, DEVMODEA* settings) {
    ++display_settings_calls;
    assert(device && std::strcmp(device, "\\\\.\\DISPLAY1") == 0);
    assert(mode == ENUM_CURRENT_SETTINGS && settings);
    settings->dmPelsWidth = 1920;
    settings->dmPelsHeight = 1080;
    settings->dmBitsPerPel = 32;
    settings->dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    return TRUE;
}
extern "C" void __cdecl a2fo_shell_set_position(void* target, void* value, int size) {
    assert(value == form.data()); ++setter_calls;
    if (fail_size) return;
    if (target == movie::at(fo, 0xad390)) client_height = size;
    else { assert(target == movie::at(fo, 0xad35c)); client_width = size; }
}

int main(int argc, char** argv) {
    assert(argc == 3);
    armada = LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    fo = LoadLibraryExA(argv[2], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    assert(armada && fo);
    std::memcpy(original_radar_uv.data(),
                movie::at(armada, movie::kArmadaBinkTextureUvScaleRva),
                original_radar_uv.size());
    assert(std::memcmp(original_radar_uv.data(),
                       movie::kExpectedArmadaBinkTextureUvScale,
                       original_radar_uv.size()) == 0);
    float radar_height_reciprocal{}, radar_width_reciprocal{};
    std::memcpy(&radar_height_reciprocal, original_radar_uv.data(), sizeof(float));
    std::memcpy(&radar_width_reciprocal, original_radar_uv.data() + 4, sizeof(float));
    assert(radar_height_reciprocal == 1.0f / 128.0f &&
           radar_width_reciprocal == 1.0f / 256.0f);
    seed_mapped_uv_instruction(movie::kArmadaBinkTextureWidthUvInstructionRva,
                               movie::kArmadaBinkTextureUvScaleRva + 4);
    seed_mapped_uv_instruction(movie::kArmadaBinkTextureHeightUvInstructionRva,
                               movie::kArmadaBinkTextureUvScaleRva);
    DWORD old{};
    assert(VirtualProtect(movie::at(fo, 0x20e644), sizeof(void*), PAGE_READWRITE, &old));
    put(movie::at(fo, 0x20e644), settings.data());
    assert(VirtualProtect(movie::at(fo, 0x24485c), sizeof(void*), PAGE_READWRITE, &old));
    put(movie::at(fo, 0x24485c), form.data());
    put(form.data() + 0x1b4, window);
    assert(VirtualProtect(movie::at(armada, movie::kArmadaShellStateRva), 4, PAGE_READWRITE, &old));
    put(movie::at(armada, movie::kArmadaShellStateRva), std::int32_t{0});
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.log = log; api.fleetops_module = get_fo;
    api.patch_call = patch; api.patch_bytes = bytes;
    assert(movie::initialize_bink_video_scaling(&api, armada));
    assert(calls.size() == 6);
    assert(calls[0] == movie::at(fo, movie::kBinkWindowCallRva));
    assert(calls[1] == movie::at(fo, movie::kBinkPlayCallRva));
    assert_private_bink_uv();
    assert(400.0f * movie::kBinkTextureWidthReciprocal == 0.78125f &&
           180.0f * movie::kBinkTextureHeightReciprocal == 0.703125f);
    movie::g_bink_window = fake_window; movie::g_bink_play = fake_play;

    selection(1280, 720, false);
    assert(movie::bink_window_hook() == window);
    assert(client_width == 1920 && client_height == 1080 && setter_calls == 2);
    assert(style & WS_POPUP); assert(!(style & WS_CAPTION));
    assert(pos_x == 0 && pos_y == 0);
    // The same snapshot skips redundant native geometry work for every frame.
    assert(movie::bink_window_hook() == window && moves == 1);
    client_width = 1200; client_height = 900; // native reset between movie calls
    assert(movie::bink_window_hook() == window && moves == 2);
    assert(client_width == 1920 && client_height == 1080 && setter_calls == 4);
    std::array<std::uint8_t, 0x58> intro{};
    assert(movie::bink_play_hook(intro.data()));
    assert(played_object == intro.data() && play_calls == 1);
    assert(logs.find("renderer=native engine") != std::string::npos);
    intro[4] = 1; play_result = false;
    assert(!movie::bink_play_hook(intro.data()));
    assert(play_calls == 2 && logs.find("renderer=D3D9") != std::string::npos);

    // Before the native shell publishes its final presentation, the intro
    // path can see a stale 1280x960 snapshot. Re-read saved FoSettings at both
    // renderer start hooks and apply the configured desktop mode instead.
    movie::presentation::State state;
    selection(1920, 1080, false);
    movie::presentation::publish({1280, 960, 32}, true, nullptr, 1280, 960);
    client_width = 1280; client_height = 960;
    intro[4] = 0;
    play_result = true;
    assert(movie::bink_play_hook(intro.data()));
    assert(client_width == 1920 && client_height == 1080);
    assert(movie::presentation::snapshot(state) && state.width == 1920 &&
           state.height == 1080 && state.depth == 32 && state.borderless &&
           state.client_width == 1920 && state.client_height == 1080);

    // Repeat through the D3D9 start path with another transient snapshot.
    movie::presentation::publish({1280, 960, 32}, true, nullptr, 1280, 960);
    client_width = 1280; client_height = 960;
    intro[4] = 1; play_result = false;
    assert(!movie::bink_play_hook(intro.data()));
    assert(client_width == 1920 && client_height == 1080);
    assert(movie::presentation::snapshot(state) && state.width == 1920 &&
           state.height == 1080 && state.client_width == 1920 &&
           state.client_height == 1080);

    // Fleet Ops stores the adapter desktop choice as 0x0. The fake USER32
    // enumeration above resolves it deterministically to 1920x1080x32.
    selection(0, 0, false);
    movie::presentation::publish({1280, 960, 32}, true, nullptr, 1280, 960);
    client_width = 1280; client_height = 960;
    play_result = true;
    assert(movie::bink_play_hook(intro.data()));
    assert(display_device_calls == 1 && display_settings_calls == 1);
    assert(client_width == 1920 && client_height == 1080);
    assert(movie::presentation::snapshot(state) && state.width == 1920 &&
           state.height == 1080 && state.depth == 32 &&
           state.client_width == 1920 && state.client_height == 1080);

    // A smaller saved selection is authoritative too; the transient larger
    // presentation must not win simply because it was published most recently.
    selection(1600, 900, false);
    movie::presentation::publish({1280, 960, 32}, true, nullptr, 1280, 960);
    client_width = 1280; client_height = 960;
    play_result = true;
    assert(movie::bink_play_hook(intro.data()));
    assert(client_width == 1920 && client_height == 1080);
    assert(movie::presentation::snapshot(state) && state.width == 1600 &&
           state.height == 900 && state.client_width == 1920 &&
           state.client_height == 1080);

    selection(1600, 900, true);
    const unsigned setters_before_windowed = setter_calls;
    assert(movie::bink_window_hook() == window);
    assert(client_width == 1600 && client_height == 900 && (style & WS_CAPTION));
    assert(pos_x == 156 && pos_y == 56 && setter_calls == setters_before_windowed + 2);
    // A clamped HWND is not cached as successfully matched: the next movie
    // call retries the genuine setters and positioning.
    selection(800, 600, false); fail_size = true;
    const unsigned before = moves;
    movie::bink_window_hook(); movie::bink_window_hook();
    assert(moves == before + 2);
    fail_size = false; movie::bink_window_hook();
    assert(client_width == 1920 && client_height == 1080);
    // Borderless follows monitor coordinates, including displays left of the
    // primary. The selected render dimensions remain intact in the snapshot.
    monitor_bounds = RECT{-2560, -200, 0, 1240};
    selection(1280, 800, false);
    movie::bink_window_hook();
    assert(client_width == 2560 && client_height == 1440 && pos_x == -2560 && pos_y == -200);
    assert(movie::presentation::snapshot(state) && state.width == 1280 && state.height == 800 &&
           state.client_width == 2560 && state.client_height == 1440);
    // Missing monitor information retains the selected geometry.
    fail_monitor = true;
    selection(1280, 800, false);
    movie::bink_window_hook();
    assert(client_width == 1280 && client_height == 800);
    fail_monitor = false;
    monitor_bounds = RECT{0, 0, 1920, 1080};
    const unsigned nested_moves = moves;
    put(movie::at(armada, movie::kArmadaShellStateRva), std::int32_t{2});
    selection(1920, 1080, false);
    movie::bink_play_hook(intro.data()); movie::bink_window_hook();
    assert(moves == nested_moves); // authored nested menus pass through
    movie::g_movie_ready = false;
    put(movie::at(armada, movie::kArmadaShellStateRva), std::int32_t{0});
    movie::bink_play_hook(intro.data());
    assert(moves == nested_moves && play_calls == 8);
    unsigned width{}, height{};
    movie::fit_dimensions(640, 480, 1280, 720, width, height);
    assert(width == 960 && height == 720);

    // A failed call install leaves every installed callback inert and backing
    // untouched. The single setup-span patch either changes all three sites
    // together or leaves every byte and the shared radar reciprocals intact.
    calls.clear(); simulated_bytes.clear(); byte_calls = 0; fail_call = 3;
    assert(!movie::initialize_bink_video_scaling(&api, armada));
    assert(!movie::g_movie_ready && byte_calls == 0);
    assert_radar_uv_unchanged();
    movie::g_bink_window = fake_window; movie::g_bink_play = fake_play;
    movie::bink_window_hook(); movie::bink_play_hook(intro.data());
    assert(moves == nested_moves);
    fail_call = 0;
    calls.clear(); simulated_bytes.clear(); byte_calls = 0; fail_byte_call = 1;
    assert(!movie::initialize_bink_video_scaling(&api, armada));
    assert(!movie::g_movie_ready && calls.size() == 6 &&
           byte_calls == 1 && simulated_bytes.size() == 1);
    assert_simulated_bytes_restored();
    fail_byte_call = 0;
    // Both FMUL instructions must reject an unexpected opcode or operand
    // before installing any calls or byte patches.
    for (const auto instruction_rva : std::array<std::uintptr_t, 2>{
             movie::kArmadaBinkTextureWidthUvInstructionRva,
             movie::kArmadaBinkTextureHeightUvInstructionRva}) {
        auto* instruction = static_cast<std::uint8_t*>(movie::at(armada, instruction_rva));
        for (const unsigned byte : {0u, 2u}) {
            DWORD prior{}, ignored{};
            assert(VirtualProtect(instruction, 6, PAGE_EXECUTE_READWRITE, &prior));
            instruction[byte] ^= 1u;
            assert(VirtualProtect(instruction, 6, prior, &ignored));
            calls.clear(); simulated_bytes.clear(); byte_calls = 0;
            assert(!movie::initialize_bink_video_scaling(&api, armada));
            assert(!movie::g_movie_ready && calls.empty() && byte_calls == 0);
            assert_radar_uv_unchanged();
            assert(VirtualProtect(instruction, 6, PAGE_EXECUTE_READWRITE, &prior));
            instruction[byte] ^= 1u;
            assert(VirtualProtect(instruction, 6, prior, &ignored));
        }
    }
    // Reject a competing owner before any patch is attempted.
    void* call_site = movie::at(fo, movie::kBinkPlayCallRva);
    assert(VirtualProtect(call_site, 5, PAGE_EXECUTE_READWRITE, &old));
    put(call_site, std::uint8_t{0x90}); calls.clear(); byte_calls = 0;
    assert(!movie::initialize_bink_video_scaling(&api, armada));
    assert(calls.empty() && byte_calls == 0);
    put(call_site, std::uint8_t{0xe8});
    std::puts("shell intro synthetic window, startup calls and renderer forwarding passed");
}
