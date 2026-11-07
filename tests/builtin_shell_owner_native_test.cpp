// Exercise production legacy_window with synthetic HWND and owner queries.
// No real windows are created or inspected.
#include <windows.h>
#include "../sdk/include/a2fo_module_provider.hpp"

#include <cstdint>
#include <cstring>
#include <cstdio>

namespace shell_owner_fixture {
HMODULE owner = nullptr;
LONG_PTR callback = 1;
bool dialog_class = true;
bool owning_thread = true;

DWORD WINAPI window_thread(HWND, LPDWORD) {
    return owning_thread ? GetCurrentThreadId() : GetCurrentThreadId() + 1;
}
int WINAPI class_name(HWND, LPSTR output, int capacity) {
    if (!output || capacity < 8) return 0;
    const char* name = dialog_class ? "#32770" : "Other";
    std::strcpy(output, name);
    return static_cast<int>(std::strlen(name));
}
LONG_PTR WINAPI window_callback(HWND, int index) {
    return index == DWLP_DLGPROC ? callback : 0;
}
BOOL WINAPI callback_owner(DWORD flags, LPCSTR address, HMODULE* output) {
    if (flags != (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT) ||
        !output || reinterpret_cast<LONG_PTR>(address) != callback || !owner)
        return FALSE;
    *output = owner;
    return TRUE;
}
}

#undef GetWindowThreadProcessId
#undef GetClassNameA
#undef GetWindowLongPtrA
#undef GetModuleHandleExA
#define GetWindowThreadProcessId shell_owner_fixture::window_thread
#define GetClassNameA shell_owner_fixture::class_name
#define GetWindowLongPtrA shell_owner_fixture::window_callback
#define GetModuleHandleExA shell_owner_fixture::callback_owner
#include "../modules/A2FORefitMenus/skin_native.cpp"
#undef GetWindowThreadProcessId
#undef GetClassNameA
#undef GetWindowLongPtrA
#undef GetModuleHandleExA

namespace {
void* A2FO_CALL fake_armada_module() {
    return reinterpret_cast<void*>(0x11000000u);
}

int fail(const char* message) {
    std::fprintf(stderr, "shell owner fixture: %s\n", message);
    return 1;
}
}

int main() {
    using namespace a2fo::refit_menu::skin_native;
    const auto dialog = reinterpret_cast<HWND>(0x42u);
    const auto other_dialog = reinterpret_cast<HWND>(0x43u);
    HMODULE core = LoadLibraryA("A2FOExtensions.dll");
    HMODULE stale_shell = LoadLibraryA("A2FOShellDisplay.dll");
    if (!core || !stale_shell) return fail("stub DLLs did not load");

    using SetReady = void (__cdecl*)(bool, bool) noexcept;
    FARPROC address = GetProcAddress(core, "A2FO_Test_SetShellReady");
    SetReady set_ready = nullptr;
    static_assert(sizeof(set_ready) == sizeof(address));
    std::memcpy(&set_ready, &address, sizeof(set_ready));
    address = GetProcAddress(core, "A2FO_Test_ArbitraryCallback");
    if (!set_ready || !address) return fail("core fixture exports absent");
    shell_owner_fixture::callback = reinterpret_cast<LONG_PTR>(address);

    A2FO_ModuleApi module_api{};
    module_api.armada_module = &fake_armada_module;
    api = &module_api;
    image = reinterpret_cast<HMODULE>(0x12000000u);

    shell_owner_fixture::owner = image;
    if (!legacy_window(dialog)) return fail("Fleet Ops owner rejected");
    shell_owner_fixture::owner = static_cast<HMODULE>(module_api.armada_module());
    if (!legacy_window(dialog)) return fail("Armada owner rejected");
    shell_owner_fixture::owner = reinterpret_cast<HMODULE>(0x13000000u);
    if (legacy_window(dialog)) return fail("unknown owner admitted");

    shell_owner_fixture::owner = core;
    set_ready(false, true);
    if (legacy_window(dialog)) return fail("startup-off core callback admitted");
    shell_owner_fixture::owner = stale_shell;
    if (legacy_window(dialog)) return fail("stale ShellDisplay DLL admitted");

    shell_owner_fixture::owner = core;
    set_ready(true, false);
    if (legacy_window(dialog)) return fail("core callback without dialog bridge admitted");
    set_ready(true, true);
    if (legacy_window(other_dialog)) return fail("unrecognized core dialog admitted");
    if (!legacy_window(dialog)) return fail("exact active shell dialog rejected");
    shell_owner_fixture::owner = stale_shell;
    if (legacy_window(dialog)) return fail("stale DLL admitted while core owns bridge");

    shell_owner_fixture::owner = core;
    shell_owner_fixture::dialog_class = false;
    if (legacy_window(dialog)) return fail("non-dialog class admitted");
    shell_owner_fixture::dialog_class = true;
    shell_owner_fixture::owning_thread = false;
    if (legacy_window(dialog)) return fail("foreign thread admitted");
    shell_owner_fixture::owning_thread = true;
    shell_owner_fixture::callback = 0;
    if (legacy_window(dialog)) return fail("null callback admitted");

    std::puts("shell owner fixture: passed");
    return 0;
}
