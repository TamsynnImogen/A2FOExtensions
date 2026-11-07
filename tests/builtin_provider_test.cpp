#include <windows.h>

#if defined(A2FO_PROVIDER_FIXTURE_CORE)
extern "C" __declspec(dllexport) int __cdecl A2FO_Test_CoreLoaded() { return 1; }
extern "C" __declspec(dllexport) int __cdecl A2FO_Test_One() { return 1; }
#if defined(A2FO_PROVIDER_FIXTURE_CORE_COMPLETE)
extern "C" __declspec(dllexport) int __cdecl A2FO_Test_Two() { return 2; }
#endif
#elif defined(A2FO_PROVIDER_FIXTURE_STANDALONE)
extern "C" __declspec(dllexport) int __cdecl A2FO_Test_StandaloneLoaded() { return 1; }
extern "C" __declspec(dllexport) int __cdecl A2FO_Test_One() { return 1; }
extern "C" __declspec(dllexport) int __cdecl A2FO_Test_Two() { return 2; }
#else
#include "../sdk/include/a2fo_module_provider.hpp"

#include <cstdio>

extern "C" __declspec(dllimport) int __cdecl A2FO_Test_CoreLoaded();
extern "C" __declspec(dllimport) int __cdecl A2FO_Test_StandaloneLoaded();

int main() {
    if (A2FO_Test_CoreLoaded() != 1 ||
        A2FO_Test_StandaloneLoaded() != 1) return 1;
    HMODULE core = GetModuleHandleA("A2FOExtensions.dll");
    HMODULE standalone = GetModuleHandleA("A2FOStandalone.dll");
    if (!core || !standalone) return 1;

    const HMODULE expected = GetProcAddress(core, "A2FO_Test_Two")
        ? core : standalone;
    if (a2fo::module_provider("A2FOStandalone.dll",
                              {"A2FO_Test_One", "A2FO_Test_Two"}) !=
        expected) {
        std::puts("provider selection failed");
        return 2;
    }
    if (a2fo::module_provider("A2FOStandalone.dll",
                              {"A2FO_Test_Missing"}) != standalone) {
        std::puts("missing export fallback failed");
        return 3;
    }
    std::puts(expected == core ? "complete core provider: PASS"
                               : "partial core fallback: PASS");
    return 0;
}
#endif
