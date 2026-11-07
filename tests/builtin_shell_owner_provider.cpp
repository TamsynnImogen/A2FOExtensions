// Stub DLL for the native shell owner test. No game or window APIs are called.
#include <windows.h>

#include <cstring>
#include <cstdint>

namespace {
#if defined(A2FO_SHELL_OWNER_CORE)
bool g_startup_ready = false;
#endif
bool g_dialog_bridge_ready = false;
}

extern "C" __declspec(dllexport) HWND __cdecl
A2FO_ShellCanvasWindowV1(HDC) {
    return nullptr;
}

extern "C" __declspec(dllexport) BOOL __cdecl
A2FO_ShellDialogBridgeReadyV1(HWND window) {
    return g_dialog_bridge_ready &&
        reinterpret_cast<std::uintptr_t>(window) == 0x42u;
}

extern "C" __declspec(dllexport) void __cdecl
A2FO_Test_ArbitraryCallback() {}

#if defined(A2FO_SHELL_OWNER_CORE)
extern "C" __declspec(dllexport) bool __cdecl
A2FOBuiltinModule_StartupReady(const char* name) noexcept {
    return name && std::strcmp(name, "A2FOShellDisplay.dll") == 0 &&
        g_startup_ready;
}

extern "C" __declspec(dllexport) void __cdecl
A2FO_Test_SetShellReady(bool startup, bool bridge) noexcept {
    g_startup_ready = startup;
    g_dialog_bridge_ready = bridge;
}
#endif
