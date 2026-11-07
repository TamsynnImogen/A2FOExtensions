// Exercise the module API against the real Sept 30 PE identity while keeping
// the engine image unmapped for execution. Hook callbacks only record calls.
#include "../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <cstdio>
#include <string>

namespace {
HMODULE g_engine = nullptr;
unsigned g_inline_hooks = 0;
unsigned g_call_patches = 0;
std::string g_diagnostic;

void A2FO_CALL test_log(const char*, const char* message) {
    if (message) {
        if (!g_diagnostic.empty()) g_diagnostic.push_back('\n');
        g_diagnostic += message;
    }
}

void* A2FO_CALL test_armada() { return g_engine; }

bool A2FO_CALL test_install_inline_hook(void*, void*, std::size_t,
                                        const std::uint8_t*, A2FO_InlineHook*) {
    ++g_inline_hooks;
    return false;
}

bool A2FO_CALL test_patch_call(void*, void*, const std::uint8_t*, std::size_t) {
    ++g_call_patches;
    return false;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "Usage: roots_native_rotation_smoke.exe MODULE_DLL SEPT30_ENGINE_EXE\n");
        return 2;
    }

    HMODULE module = LoadLibraryA(argv[1]);
    if (!module) {
        std::fprintf(stderr, "Cannot load station rotation module (%lu)\n",
                     static_cast<unsigned long>(GetLastError()));
        return 3;
    }
    g_engine = LoadLibraryExA(argv[2], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!g_engine) {
        std::fprintf(stderr, "Cannot map Sept 30 engine without resolving its entry point (%lu)\n",
                     static_cast<unsigned long>(GetLastError()));
        FreeLibrary(module);
        return 4;
    }

    const auto init = reinterpret_cast<A2FO_ModuleInitFn>(
        GetProcAddress(module, "A2FO_ModuleInit"));
    const auto shutdown = reinterpret_cast<A2FO_ModuleShutdownFn>(
        GetProcAddress(module, "A2FO_ModuleShutdown"));
    if (!init || !shutdown) {
        std::fprintf(stderr, "Station module API exports are missing\n");
        FreeLibrary(g_engine);
        FreeLibrary(module);
        return 5;
    }

    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = test_log;
    api.armada_module = test_armada;
    api.install_inline_hook = test_install_inline_hook;
    api.patch_call = test_patch_call;

    const bool initialized = init(&api);
    const bool diagnostic = g_diagnostic.find("native station rotation") != std::string::npos;
    const bool ok = initialized && g_inline_hooks == 0 && g_call_patches == 0 && diagnostic;
    if (!ok) {
        std::fprintf(stderr,
                     "ModuleInit=%s inline-hooks=%u call-patches=%u native-diagnostic=%s\n%s\n",
                     initialized ? "true" : "false", g_inline_hooks, g_call_patches,
                     diagnostic ? "present" : "missing", g_diagnostic.c_str());
    } else {
        std::puts("Roots native station rotation: PASS (no extension hooks or patches installed)");
    }

    shutdown();
    FreeLibrary(g_engine);
    FreeLibrary(module);
    return ok ? 0 : 1;
}
