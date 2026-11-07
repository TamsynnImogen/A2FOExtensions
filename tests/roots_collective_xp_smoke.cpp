// Headless integration preflight for the rebuilt Collective XP DLL. Real
// Armada/Fleet Ops images are mapped as data; hook APIs validate signatures
// and record calls without writing patches or executing engine code.
#include "../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {
constexpr std::uintptr_t kAwardRva = 0x001dcfc3;
HMODULE g_armada = nullptr;
HMODULE g_fleetops = nullptr;
unsigned g_inline_calls = 0;
unsigned g_jump_calls = 0;
unsigned g_bad_signatures = 0;
unsigned g_race_registrations = 0;
unsigned g_class_registrations = 0;
unsigned g_craft_registrations = 0;
std::string g_diagnostics;

void A2FO_CALL test_log(const char*, const char* message) {
    if (!message) return;
    if (!g_diagnostics.empty()) g_diagnostics.push_back('\n');
    g_diagnostics += message;
}
void* A2FO_CALL test_armada() { return g_armada; }
void* A2FO_CALL test_fleetops() { return g_fleetops; }

bool A2FO_CALL test_register_race(
    const char*, const char* const*, std::uint32_t,
    A2FO_RaceLoadedHandler, void*) {
    ++g_race_registrations;
    return true;
}
bool A2FO_CALL test_register_class(
    const char*, const char* const*, std::uint32_t,
    A2FO_GameObjectClassLoadedHandler, void*) {
    ++g_class_registrations;
    return true;
}
bool A2FO_CALL test_register_craft(
    const char*, std::uint32_t, A2FO_CraftEventHandler, void*) {
    ++g_craft_registrations;
    return true;
}

bool matches(const void* target, const std::uint8_t* expected, std::size_t length) {
    if (!target || !expected || !length ||
        std::memcmp(target, expected, length) != 0) {
        ++g_bad_signatures;
        return false;
    }
    return true;
}

bool A2FO_CALL test_install_inline_hook(
    void* target, void*, std::size_t length, const std::uint8_t* expected,
    A2FO_InlineHook* hook) {
    ++g_inline_calls;
    if (!matches(target, expected, length) || !hook) return false;
    hook->target = target;
    hook->gateway = target;
    hook->length = length;
    return true;
}

bool A2FO_CALL test_patch_jump(
    void* target, void*, const std::uint8_t* expected, std::size_t length) {
    ++g_jump_calls;
    return matches(target, expected, length);
}

void reset_observations() {
    g_inline_calls = 0;
    g_jump_calls = 0;
    g_bad_signatures = 0;
    g_race_registrations = 0;
    g_class_registrations = 0;
    g_craft_registrations = 0;
    g_diagnostics.clear();
}

template<class Function>
Function find_export(HMODULE module, const char* name) {
    Function function{};
    const FARPROC address = GetProcAddress(module, name);
    static_assert(sizeof(function) == sizeof(address), "Win32 function pointer size mismatch");
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

bool contains(const char* phrase) {
    return g_diagnostics.find(phrase) != std::string::npos;
}

void close_images(HMODULE module) {
    if (g_fleetops) FreeLibrary(g_fleetops);
    if (g_armada) FreeLibrary(g_armada);
    if (module) FreeLibrary(module);
    g_fleetops = nullptr;
    g_armada = nullptr;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr,
            "Usage: roots_collective_xp_smoke.exe COLLECTIVE_XP_DLL NEW_ARMADA_EXE NEW_FLEETOPS_DLL\n");
        return 2;
    }

    HMODULE module = LoadLibraryA(argv[1]);
    if (!module) {
        std::fprintf(stderr, "Cannot load rebuilt Collective XP DLL (%lu)\n",
                     static_cast<unsigned long>(GetLastError()));
        return 3;
    }
    g_armada = LoadLibraryExA(argv[2], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    g_fleetops = LoadLibraryExA(argv[3], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!g_armada || !g_fleetops) {
        std::fprintf(stderr, "Cannot map Armada/Fleet Ops images without resolving entry points (%lu)\n",
                     static_cast<unsigned long>(GetLastError()));
        close_images(module);
        return 4;
    }

    using ModuleInit = bool (__cdecl*)(const A2FO_ModuleApi*);
    using ModuleShutdown = void (__cdecl*)();
    const auto init = find_export<ModuleInit>(module, "A2FO_ModuleInit");
    const auto shutdown = find_export<ModuleShutdown>(module, "A2FO_ModuleShutdown");
    if (!init || !shutdown) {
        std::fprintf(stderr, "Collective XP module API exports are missing\n");
        close_images(module);
        return 5;
    }

    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = test_log;
    api.armada_module = test_armada;
    api.fleetops_module = test_fleetops;
    api.install_inline_hook = test_install_inline_hook;
    api.patch_jump = test_patch_jump;
    api.register_race_loaded_handler = test_register_race;
    api.register_game_object_class_loaded_handler = test_register_class;
    api.register_craft_event_handler_masked = test_register_craft;

    reset_observations();
    const bool initialized = init(&api);
    const bool hooks_ok = initialized && g_inline_calls == 4 && g_jump_calls == 3 &&
        g_bad_signatures == 0 && g_race_registrations == 1 &&
        g_class_registrations == 1 && g_craft_registrations == 1 &&
        contains("Native award") && contains("hooks installed");
    if (!hooks_ok) {
        std::fprintf(stderr,
            "Supported-profile preflight failed: init=%s inline=%u jumps=%u bad-bytes=%u registrations=%u/%u/%u\n%s\n",
            initialized ? "true" : "false", g_inline_calls, g_jump_calls,
            g_bad_signatures, g_race_registrations, g_class_registrations,
            g_craft_registrations, g_diagnostics.c_str());
        shutdown();
        close_images(module);
        return 1;
    }
    shutdown();

    auto* award_site = reinterpret_cast<std::uint8_t*>(g_fleetops) + kAwardRva;
    DWORD old_protection = 0;
    if (!VirtualProtect(award_site, 1, PAGE_EXECUTE_READWRITE, &old_protection)) {
        std::fprintf(stderr, "Cannot make mapped award byte writable (%lu)\n",
                     static_cast<unsigned long>(GetLastError()));
        close_images(module);
        return 6;
    }
    const std::uint8_t original_award_byte = *award_site;
    *award_site ^= 1;
    FlushInstructionCache(GetCurrentProcess(), award_site, 1);

    reset_observations();
    const bool changed_signature_initialized = init(&api);
    const bool rejected_signature = changed_signature_initialized &&
        g_inline_calls == 0 && g_jump_calls == 0 && g_bad_signatures == 0 &&
        contains("stock/pass-through");

    *award_site = original_award_byte;
    FlushInstructionCache(GetCurrentProcess(), award_site, 1);
    DWORD ignored_protection = 0;
    const bool protection_restored = VirtualProtect(
        award_site, 1, old_protection, &ignored_protection) != 0;
    shutdown();

    const bool ok = rejected_signature && protection_restored;
    if (!ok) {
        std::fprintf(stderr,
            "Changed-signature preflight failed: init=%s inline=%u jumps=%u stock-diagnostic=%s protection-restored=%s\n%s\n",
            changed_signature_initialized ? "true" : "false", g_inline_calls,
            g_jump_calls, contains("stock/pass-through") ? "present" : "missing",
            protection_restored ? "yes" : "no", g_diagnostics.c_str());
    } else {
        std::puts("Collective XP rebuilt-DLL integration preflight passed; no engine patches were written");
    }
    close_images(module);
    return ok ? 0 : 1;
}
