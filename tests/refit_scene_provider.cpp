// Headless provider ABI check. Never creates a window or D3D device.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

struct Init24 {
    std::uint32_t size;
    std::uint32_t version;
    void* device;
    const char* resource_root;
    const char* const* roots;
    std::uint32_t root_count;
};
static_assert(sizeof(Init24) == 24, "provider ABI requires Win32");

template <typename Function>
Function symbol(HMODULE module, const char* name) {
    FARPROC address = GetProcAddress(module, name);
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(address));
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

using SetDevice = std::uint32_t (__cdecl*)(void*);
using Scene = std::uint32_t (__cdecl*)(const char*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
struct Foreign { SetDevice setter; Scene scene; Scene warp; };
DWORD WINAPI foreign_set_device(void* parameter) {
    const auto& calls = *static_cast<Foreign*>(parameter);
    return calls.setter(nullptr) == 0 && calls.scene("borg", 0, 0, 320, 200) == 0 &&
        calls.warp("main", 0, 0, 320, 200) == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc < 4 || argc > 35) {
        std::fprintf(stderr, "usage: refit_scene_provider.exe DLL RESOURCE_ROOT ASSET_ROOT [ASSET_ROOT ...] (1..32 roots)\n");
        return 2;
    }
    for (int i = 2; i < argc; ++i) {
        const DWORD attributes = GetFileAttributesA(argv[i]);
        if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::fprintf(stderr, "resource or asset root missing: %s\n", argv[i]);
            return 2;
        }
    }
    HMODULE module = LoadLibraryExA(argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) {
        std::fprintf(stderr, "LoadLibraryExA failed: %lu\n", GetLastError());
        return 1;
    }
    using Version = std::uint32_t (__cdecl*)();
    using Initialize = std::uint32_t (__cdecl*)(const Init24*);
    using Draw = std::uint32_t (__cdecl*)(const char*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
    const auto version = symbol<Version>(module, "A2FOScene_ApiVersion");
    const auto initialize = symbol<Initialize>(module, "A2FOScene_Initialize");
    const auto set_device = symbol<SetDevice>(module, "A2FOScene_SetDevice");
    const auto draw = symbol<Draw>(module, "Render3D_MenuScene");
    const auto warp = symbol<Draw>(module, "Render3D_MenuWarp");
    if (!version || !initialize || !set_device || !draw || !warp || version() != 1) {
        std::fprintf(stderr, "provider exports or version mismatch\n");
        return 1;
    }
    const char* roots[32]{};
    for (int i = 3; i < argc; ++i) roots[i - 3] = argv[i];
    Init24 input{sizeof(Init24), 1, nullptr, argv[2], roots, static_cast<std::uint32_t>(argc - 3)};
    auto check = [](bool ok, const char* what) {
        if (!ok) std::fprintf(stderr, "FAIL: %s\n", what);
        return ok;
    };
    if (!check(initialize(nullptr) == 0, "null init rejected")) return 1;
    Init24 wrong = input;
    wrong.version = 2;
    if (!check(initialize(&wrong) == 0, "wrong version rejected")) return 1;
    wrong = input;
    wrong.size = 0;
    if (!check(initialize(&wrong) == 0, "wrong size rejected")) return 1;
    if (!check(initialize(&input) == 1, "valid null-device init")) return 1;
    if (!check(draw("main", 0, 0, 320, 200) == 0, "null-device draw rejected")) return 1;
    if (!check(set_device(nullptr) == 1, "owner-thread null SetDevice")) return 1;
    Foreign foreign_setter{set_device, draw, warp};
    HANDLE thread = CreateThread(nullptr, 0, foreign_set_device, &foreign_setter, 0, nullptr);
    if (!check(thread != nullptr, "foreign thread created")) return 1;
    const DWORD wait = WaitForSingleObject(thread, 30000);
    DWORD foreign_result = 1;
    const bool joined = wait == WAIT_OBJECT_0 && GetExitCodeThread(thread, &foreign_result) != 0;
    CloseHandle(thread);
    if (!check(joined && foreign_result == 0, "foreign-thread device/scene/warp calls rejected")) return 1;
    if (!check(initialize(&input) == 0, "repeat init rejected")) return 1;
    std::puts("PASS: provider ABI, init guards, null device, owner/foreign thread, repeat init");
    return 0;
}
