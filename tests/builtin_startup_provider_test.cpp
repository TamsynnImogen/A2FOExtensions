// DLL-only startup provider fixture. The executable loads stub providers from
// its own directory; it does not load the real core or game modules.
#include <windows.h>

#include <cstring>

#if defined(A2FO_STARTUP_PROVIDER_STANDALONE)
extern "C" __declspec(dllexport) int __cdecl A2FO_Test_One() { return 1; }
extern "C" __declspec(dllexport) int __cdecl A2FO_Test_Two() { return 2; }

#elif defined(A2FO_STARTUP_PROVIDER_CORE_FULL) || \
      defined(A2FO_STARTUP_PROVIDER_CORE_OLD) || \
      defined(A2FO_STARTUP_PROVIDER_CORE_PARTIAL)
extern "C" __declspec(dllexport) int __cdecl A2FO_Test_One() { return 1; }
#if !defined(A2FO_STARTUP_PROVIDER_CORE_PARTIAL)
extern "C" __declspec(dllexport) int __cdecl A2FO_Test_Two() { return 2; }
#endif
#if defined(A2FO_STARTUP_PROVIDER_CORE_FULL)
namespace { bool g_ready = false; }
extern "C" __declspec(dllexport) bool __cdecl
A2FOBuiltinModule_StartupReady(const char* name) noexcept {
    return name && std::strcmp(name, "A2FOStandalone.dll") == 0 && g_ready;
}
extern "C" __declspec(dllexport) void __cdecl
A2FO_Test_SetReady(bool value) noexcept {
    g_ready = value;
}
#endif

#else
#include "../sdk/include/a2fo_module_provider.hpp"

#include <cstdio>

namespace {
int fail(const char* message) {
    std::fprintf(stderr, "startup provider fixture: %s\n", message);
    return 1;
}
}

int main(int argc, char** argv) {
    if (argc != 2) return fail("expected full|old|partial|no-core");
    const char* mode = argv[1];
    const bool no_core = std::strcmp(mode, "no-core") == 0;
    const bool full = std::strcmp(mode, "full") == 0;
    const bool old = std::strcmp(mode, "old") == 0;
    const bool partial = std::strcmp(mode, "partial") == 0;
    if (!no_core && !full && !old && !partial)
        return fail("unknown mode");

    HMODULE standalone = LoadLibraryA("A2FOStandalone.dll");
    if (!standalone || !GetProcAddress(standalone, "A2FO_Test_Two"))
        return fail("standalone fixture did not load");
    HMODULE core = no_core ? nullptr : LoadLibraryA("A2FOExtensions.dll");
    if (!no_core && !core) return fail("core fixture did not load");

    const auto provider = []() noexcept {
        return a2fo::startup_module_provider(
            "A2FOStandalone.dll", {"A2FO_Test_One", "A2FO_Test_Two"});
    };
    if (full) {
        if (!GetProcAddress(core, "A2FOBuiltinModule_StartupReady"))
            return fail("full core readiness export absent");
        if (provider() != nullptr)
            return fail("disabled complete core fell back to stale DLL");
        using SetReady = void (__cdecl*)(bool) noexcept;
        const FARPROC address = GetProcAddress(core, "A2FO_Test_SetReady");
        SetReady set_ready = nullptr;
        static_assert(sizeof(set_ready) == sizeof(address));
        std::memcpy(&set_ready, &address, sizeof(set_ready));
        if (!set_ready) return fail("full core setter absent");
        set_ready(true);
        if (provider() != core) return fail("ready complete core not selected");
        set_ready(false);
        if (provider() != nullptr)
            return fail("disabled core returned a provider after readiness reset");
    } else if (provider() != (old ? core : standalone)) {
        return fail("old, partial, or absent core fallback incorrect");
    }
    std::printf("startup provider %s: passed\n", mode);
    return 0;
}
#endif
