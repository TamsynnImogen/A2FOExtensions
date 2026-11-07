/*
 * Compatibility mmm_loader.dll for A2FOMissions.
 *
 * Armada mission files still request missions/mmm_loader.dll.  Keep that ABI,
 * but forward the two MMM script entry points into the active Missions owner.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstddef>
#include <cstring>

namespace {

using ScriptExtendInterfaceFn = int (__cdecl*)(void*);
using ScriptInitFn = int (__cdecl*)(void*);
using MarkMissionUnloadedFn = void (__cdecl*)();
using StartupReadyFn = bool (__cdecl*)(const char*);

HINSTANCE g_self = nullptr;
HMODULE g_missions = nullptr;
ScriptExtendInterfaceFn g_extend = nullptr;
ScriptInitFn g_init = nullptr;
MarkMissionUnloadedFn g_mark_unloaded = nullptr;

void debug(const char* text) noexcept {
    OutputDebugStringA("mmm_loader shim: ");
    OutputDebugStringA(text ? text : "");
    OutputDebugStringA("\n");
}

bool append(char* buffer, std::size_t capacity, const char* suffix) noexcept {
    const std::size_t used = std::strlen(buffer);
    const std::size_t extra = std::strlen(suffix);
    if (used + extra + 1 > capacity) return false;
    std::memcpy(buffer + used, suffix, extra + 1);
    return true;
}


HMODULE load_from_core_root() noexcept {
    HMODULE core = GetModuleHandleA("A2FOExtensions.dll");
    if (!core) return nullptr;

    char path[MAX_PATH + 1]{};
    const DWORD count = GetModuleFileNameA(core, path, MAX_PATH);
    if (!count || count >= MAX_PATH) return nullptr;

    char* slash = std::strrchr(path, '\\');
    if (!slash) slash = std::strrchr(path, '/');
    if (!slash) return nullptr;
    *(slash + 1) = '\0';

    if (!append(path, sizeof(path), "modules\\A2FOMissions.dll")) return nullptr;
    return LoadLibraryA(path);
}

HMODULE load_relative_module() noexcept {
    if (!g_self) return nullptr;

    char path[MAX_PATH + 1]{};
    const DWORD count = GetModuleFileNameA(g_self, path, MAX_PATH);
    if (!count || count >= MAX_PATH) return nullptr;

    char* slash = std::strrchr(path, '\\');
    if (!slash) slash = std::strrchr(path, '/');
    if (!slash) return nullptr;
    *slash = '\0';  // .../missions

    slash = std::strrchr(path, '\\');
    if (!slash) slash = std::strrchr(path, '/');
    if (!slash) return nullptr;
    *(slash + 1) = '\0';  // installation root, including trailing slash

    if (!append(path, sizeof(path), "modules\\A2FOMissions.dll")) return nullptr;
    return LoadLibraryA(path);
}

bool resolve() noexcept {
    if (g_extend && g_init) return true;

    // A core that owns the startup-only Missions feature is authoritative,
    // including when the feature was disabled or failed initialization.
    HMODULE core = GetModuleHandleA("A2FOExtensions.dll");
    if (core) {
        auto ready = reinterpret_cast<StartupReadyFn>(
            GetProcAddress(core, "A2FOBuiltinModule_StartupReady"));
        FARPROC owner = GetProcAddress(core, "A2FOBuiltinMissions_Init");
        if (ready && owner) {
            auto extend = reinterpret_cast<ScriptExtendInterfaceFn>(
                GetProcAddress(core, "A2FOMissions_ScriptExtendInterface"));
            auto init = reinterpret_cast<ScriptInitFn>(
                GetProcAddress(core, "A2FOMissions_ScriptInit"));
            auto unloaded = reinterpret_cast<MarkMissionUnloadedFn>(
                GetProcAddress(core, "A2FOMissions_MarkMissionUnloaded"));
            if (!extend || !init || !unloaded || !ready("A2FOMissions")) {
                debug("built-in Missions unavailable");
                return false;
            }
            g_missions = core;
            g_extend = extend;
            g_init = init;
            g_mark_unloaded = unloaded;
            return true;
        }
    }

    // An older core may still use the standalone Missions module.
    g_missions = GetModuleHandleA("A2FOMissions.dll");
    if (!g_missions) {
        // Prefer the authoritative A2FOExtensions installation root.  This also
        // works if a mod carries its own missions/mmm_loader.dll copy.
        g_missions = load_from_core_root();
    }
    if (!g_missions) {
        // Final fallback makes direct MMM-style testing possible from the build
        // layout without depending on the process current working directory.
        g_missions = load_relative_module();
    }
    if (!g_missions) {
        debug("unable to locate modules/A2FOMissions.dll");
        return false;
    }

    g_extend = reinterpret_cast<ScriptExtendInterfaceFn>(
        GetProcAddress(g_missions, "A2FOMissions_ScriptExtendInterface"));
    g_init = reinterpret_cast<ScriptInitFn>(
        GetProcAddress(g_missions, "A2FOMissions_ScriptInit"));
    g_mark_unloaded = reinterpret_cast<MarkMissionUnloadedFn>(
        GetProcAddress(g_missions, "A2FOMissions_MarkMissionUnloaded"));

    if (!g_extend || !g_init) {
        debug("A2FOMissions.dll is missing its MMM compatibility exports");
        return false;
    }
    return true;
}

}  // namespace

extern "C" __declspec(dllexport) int __cdecl ScriptExtendInterface(void* param) {
    return resolve() ? g_extend(param) : 51;
}

extern "C" __declspec(dllexport) int __cdecl ScriptInit(void* param) {
    return resolve() ? g_init(param) : 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = instance;
        DisableThreadLibraryCalls(instance);
    } else if (reason == DLL_PROCESS_DETACH && reserved == nullptr) {
        // This export only sets an interlocked flag.  All real MMM cleanup is
        // deferred until the next mission, outside the Windows loader lock.
        if (g_mark_unloaded) g_mark_unloaded();
    }
    return TRUE;
}
