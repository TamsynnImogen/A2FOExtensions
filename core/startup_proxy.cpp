#include <windows.h>

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace {

// Armada imports these two decorated exports from Win2kDisableTaskSwitch.dll.
// The renamed original must still attach immediately because its DllMain loads
// FleetOpsHook.dll before Armada's entry point. A2FOExtensions must attach next
// so its proven early built-in hooks are present even when Armada never calls
// these task-switch exports. Third-party modules remain deferred to the core's
// post-attach worker.
using SetHookIdFunction = void(__cdecl*)(HHOOK);
using LowLevelKeyboardProcFunction = LRESULT(__stdcall*)(int, WPARAM, LPARAM);
using ExtensionInitializeFunction = bool(__cdecl*)();

HMODULE g_proxy = nullptr;
HMODULE g_original = nullptr;
HMODULE g_extension = nullptr;
SetHookIdFunction g_set_hook_id = nullptr;
LowLevelKeyboardProcFunction g_low_level_keyboard_proc = nullptr;

// 0 = not attempted, 1 = loading, 2 = ready, -1 = failed.
volatile LONG g_original_state = 0;
volatile LONG g_extension_state = 0;

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

void proxy_log(const char* text, DWORD error = ERROR_SUCCESS) {
    if (!g_proxy || !text) {
        return;
    }
    char path[32768]{};
    if (!sibling_path(g_proxy, "A2FOStartupProxy.log", path, sizeof(path))) {
        return;
    }
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    char line[768]{};
    const int length = error == ERROR_SUCCESS
        ? std::snprintf(line, sizeof(line), "%s\r\n", text)
        : std::snprintf(line, sizeof(line), "%s (GetLastError=%lu)\r\n",
                        text, static_cast<unsigned long>(error));
    if (length > 0) {
        DWORD written = 0;
        WriteFile(file, line,
                  static_cast<DWORD>(length < static_cast<int>(sizeof(line))
                                         ? length
                                         : sizeof(line) - 1),
                  &written, nullptr);
    }
    CloseHandle(file);
}

template <typename Function>
bool load_export(HMODULE module, const char* name, Function& output) {
    FARPROC address = GetProcAddress(module, name);
    if (!address) {
        return false;
    }
    static_assert(sizeof(output) == sizeof(address),
                  "function pointers must match FARPROC on 32-bit Windows");
    std::memcpy(&output, &address, sizeof(output));
    return true;
}

bool wait_for_state(volatile LONG* state) {
    // The exported functions are not expected to race during startup, but use
    // a bounded wait so a second caller never observes partially set pointers.
    for (unsigned attempt = 0; attempt < 5000; ++attempt) {
        const LONG value = InterlockedCompareExchange(state, 0, 0);
        if (value == 2) {
            return true;
        }
        if (value == -1) {
            return false;
        }
        Sleep(1);
    }
    proxy_log("Timed out waiting for startup DLL load");
    return false;
}

bool ensure_original_loaded() {
    const LONG previous = InterlockedCompareExchange(&g_original_state, 1, 0);
    if (previous == 2) {
        return true;
    }
    if (previous == -1) {
        return false;
    }
    if (previous == 1) {
        return wait_for_state(&g_original_state);
    }

    proxy_log("Loading Win2kDisableTaskSwitch.original.dll");
    char path[32768]{};
    if (!sibling_path(g_proxy, "Win2kDisableTaskSwitch.original.dll", path,
                      sizeof(path))) {
        proxy_log("Could not construct original DLL path");
        InterlockedExchange(&g_original_state, -1);
        return false;
    }

    SetLastError(ERROR_SUCCESS);
    g_original = LoadLibraryA(path);
    if (!g_original) {
        proxy_log("LoadLibrary failed for original startup DLL", GetLastError());
        InterlockedExchange(&g_original_state, -1);
        return false;
    }

    if (!load_export(g_original, "?SetHookID@@YAXPAUHHOOK__@@@Z",
                     g_set_hook_id)) {
        proxy_log("Original DLL is missing SetHookID", GetLastError());
        FreeLibrary(g_original);
        g_original = nullptr;
        InterlockedExchange(&g_original_state, -1);
        return false;
    }
    if (!load_export(g_original, "?LowLevelKeyboardProc@@YGJHIJ@Z",
                     g_low_level_keyboard_proc)) {
        proxy_log("Original DLL is missing LowLevelKeyboardProc", GetLastError());
        FreeLibrary(g_original);
        g_original = nullptr;
        g_set_hook_id = nullptr;
        InterlockedExchange(&g_original_state, -1);
        return false;
    }

    InterlockedExchange(&g_original_state, 2);
    proxy_log("Original startup DLL loaded");
    return true;
}

bool ensure_extension_loaded() {
    const LONG previous = InterlockedCompareExchange(&g_extension_state, 1, 0);
    if (previous == 2) {
        return true;
    }
    if (previous == -1) {
        return false;
    }
    if (previous == 1) {
        return wait_for_state(&g_extension_state);
    }

    // Load the original first because FleetOpsHook.dll must be present before
    // A2FOExtensions validates and patches Fleet Operations addresses.
    if (!ensure_original_loaded()) {
        proxy_log("Extension not loaded because original startup DLL failed");
        InterlockedExchange(&g_extension_state, -1);
        return false;
    }

    proxy_log("Loading A2FOExtensions.dll");
    char path[32768]{};
    if (!sibling_path(g_proxy, "A2FOExtensions.dll", path, sizeof(path))) {
        proxy_log("Could not construct extension DLL path");
        InterlockedExchange(&g_extension_state, -1);
        return false;
    }

    SetLastError(ERROR_SUCCESS);
    g_extension = LoadLibraryA(path);
    if (!g_extension) {
        proxy_log("LoadLibrary failed for A2FOExtensions.dll", GetLastError());
        InterlockedExchange(&g_extension_state, -1);
        return false;
    }

    ExtensionInitializeFunction initialize = nullptr;
    if (!load_export(g_extension, "A2FO_Initialize", initialize)) {
        proxy_log("A2FOExtensions.dll is missing A2FO_Initialize",
                  GetLastError());
        InterlockedExchange(&g_extension_state, -1);
        return false;
    }
    proxy_log("Calling A2FO_Initialize outside the loader lock");
    if (!initialize()) {
        proxy_log("A2FO_Initialize reported failure");
        InterlockedExchange(&g_extension_state, -1);
        return false;
    }

    InterlockedExchange(&g_extension_state, 2);
    proxy_log("A2FOExtensions.dll initialized");
    return true;
}

bool attach_extension_early() {
    proxy_log("Loading A2FOExtensions.dll for early built-in hooks");
    char path[32768]{};
    if (!sibling_path(g_proxy, "A2FOExtensions.dll", path, sizeof(path))) {
        proxy_log("Could not construct extension DLL path");
        InterlockedExchange(&g_extension_state, -1);
        return false;
    }

    SetLastError(ERROR_SUCCESS);
    g_extension = LoadLibraryA(path);
    if (!g_extension) {
        proxy_log("LoadLibrary failed for A2FOExtensions.dll", GetLastError());
        InterlockedExchange(&g_extension_state, -1);
        return false;
    }

    // The core's DllMain installs only the proven early built-in hooks and
    // schedules A2FO_Initialize on a worker. Windows serializes DLL attach, so
    // that worker cannot discover modules until the loader lock is released.
    InterlockedExchange(&g_extension_state, 2);
    proxy_log("A2FOExtensions.dll attached; deferred initialization scheduled");
    return true;
}

void ensure_startup_ready() {
    // Extension failure is deliberately non-fatal. Armada can still call the
    // original startup DLL, and the diagnostic log records why extensions did
    // not load instead of terminating the process from DllMain.
    ensure_original_loaded();
    ensure_extension_loaded();
}

}  // namespace

extern "C" void __cdecl a2fo_proxy_set_hook_id(HHOOK hook) {
    ensure_startup_ready();
    if (g_set_hook_id) {
        g_set_hook_id(hook);
    }
}

extern "C" LRESULT __stdcall a2fo_proxy_low_level_keyboard_proc(
    int code, WPARAM wparam, LPARAM lparam) {
    ensure_startup_ready();
    if (g_low_level_keyboard_proc) {
        return g_low_level_keyboard_proc(code, wparam, lparam);
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        g_proxy = instance;
        // This one nested load is required by the shipped startup contract:
        // Win2kDisableTaskSwitch.original.dll loads ..\FleetOpsHook.dll from
        // its own process-attach path. Loading our core or modules here would
        // add unsafe work under loader lock, so those stay deferred.
        if (!ensure_original_loaded()) {
            return FALSE;
        }
        if (!attach_extension_early()) {
            return FALSE;
        }
    }
    return TRUE;
}
