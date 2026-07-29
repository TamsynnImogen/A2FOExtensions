#include <windows.h>

#include <cstddef>
#include <cstring>

namespace {

// Armada imports two decorated exports from Win2kDisableTaskSwitch.dll very
// early in startup. This proxy preserves those exports, forwards them to the
// renamed original DLL, and loads A2FOExtensions before class registration.
using SetHookIdFunction = void(__cdecl*)(HHOOK);
using LowLevelKeyboardProcFunction = LRESULT(__stdcall*)(int, WPARAM, LPARAM);

HMODULE g_original = nullptr;
SetHookIdFunction g_set_hook_id = nullptr;
LowLevelKeyboardProcFunction g_low_level_keyboard_proc = nullptr;

template <typename Function>
bool load_export(HMODULE module, const char* name, Function& output) {
    FARPROC address = GetProcAddress(module, name);
    if (!address) {
        return false;
    }
    static_assert(sizeof(output) == sizeof(address),
                  "function pointers must match FARPROC on 32-bit Windows");
    // Copying the representation avoids GCC's warning for converting the
    // generic FARPROC signature to each export's real calling convention.
    std::memcpy(&output, &address, sizeof(output));
    return true;
}

bool sibling_path(HMODULE module, const char* filename, char* output,
                  std::size_t output_size) {
    if (!module || !filename || !output || output_size == 0) {
        return false;
    }
    const DWORD length = GetModuleFileNameA(
        module, output, static_cast<DWORD>(output_size));
    if (length == 0 || length >= output_size) {
        return false;
    }
    char* slash = std::strrchr(output, '\\');
    if (!slash) {
        slash = std::strrchr(output, '/');
    }
    if (!slash) {
        return false;
    }
    const std::size_t prefix = static_cast<std::size_t>(slash + 1 - output);
    const std::size_t suffix = std::strlen(filename);
    if (prefix + suffix + 1 > output_size) {
        return false;
    }
    std::memcpy(output + prefix, filename, suffix + 1);
    return true;
}

bool load_original_and_extension(HMODULE proxy) {
    char path[32768]{};
    if (!sibling_path(proxy, "Win2kDisableTaskSwitch.original.dll", path,
                      sizeof(path))) {
        return false;
    }
    g_original = LoadLibraryA(path);
    if (!g_original) {
        return false;
    }
    if (!load_export(g_original, "?SetHookID@@YAXPAUHHOOK__@@@Z",
                     g_set_hook_id) ||
        !load_export(g_original, "?LowLevelKeyboardProc@@YGJHIJ@Z",
                     g_low_level_keyboard_proc)) {
        return false;
    }

    if (!sibling_path(proxy, "A2FOExtensions.dll", path, sizeof(path))) {
        return false;
    }
    // The original startup DLL has now loaded FleetOpsHook.dll. Loading the
    // companion second lets it see Fleet Ops' patches while still running
    // before Armada's entry point and object-class database initialization.
    return LoadLibraryA(path) != nullptr;
}

}  // namespace

extern "C" void __cdecl a2fo_proxy_set_hook_id(HHOOK hook) {
    // Exported as Armada's original ?SetHookID@@YAXPAUHHOOK__@@@Z symbol by
    // startup_proxy.def.
    if (g_set_hook_id) {
        g_set_hook_id(hook);
    }
}

extern "C" LRESULT __stdcall a2fo_proxy_low_level_keyboard_proc(
    int code, WPARAM wparam, LPARAM lparam) {
    // Exported under the original decorated keyboard-procedure name. The
    // proxy never installs or drives input itself; it only forwards Armada's
    // existing call to the shipped startup DLL.
    if (g_low_level_keyboard_proc) {
        return g_low_level_keyboard_proc(code, wparam, lparam);
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        return load_original_and_extension(instance) ? TRUE : FALSE;
    }
    return TRUE;
}
