#include <windows.h>

#include <cstddef>
#include <cstring>

namespace {

using SetHookIdFunction = void(__cdecl*)(HHOOK);
using LowLevelKeyboardProcFunction = LRESULT(__stdcall*)(int, WPARAM, LPARAM);

HMODULE g_original = nullptr;
SetHookIdFunction g_set_hook_id = nullptr;
LowLevelKeyboardProcFunction g_low_level_keyboard_proc = nullptr;

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
    g_set_hook_id = reinterpret_cast<SetHookIdFunction>(GetProcAddress(
        g_original, "?SetHookID@@YAXPAUHHOOK__@@@Z"));
    g_low_level_keyboard_proc =
        reinterpret_cast<LowLevelKeyboardProcFunction>(GetProcAddress(
            g_original, "?LowLevelKeyboardProc@@YGJHIJ@Z"));
    if (!g_set_hook_id || !g_low_level_keyboard_proc) {
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
    if (g_set_hook_id) {
        g_set_hook_id(hook);
    }
}

extern "C" LRESULT __stdcall a2fo_proxy_low_level_keyboard_proc(
    int code, WPARAM wparam, LPARAM lparam) {
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
