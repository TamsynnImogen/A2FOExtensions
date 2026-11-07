#pragma once

#include <windows.h>

#include <initializer_list>
#include <cstring>

namespace a2fo {

// Prefer the startup core only when it provides the complete bridge contract.
// Neither candidate is loaded by this lookup.
inline HMODULE module_provider(
    const char* standalone_name,
    std::initializer_list<const char*> required_exports) noexcept {
    HMODULE core = GetModuleHandleA("A2FOExtensions.dll");
    if (core) {
        bool complete = true;
        for (const char* name : required_exports) {
            if (!name || !GetProcAddress(core, name)) {
                complete = false;
                break;
            }
        }
        if (complete) return core;
    }
    return standalone_name ? GetModuleHandleA(standalone_name) : nullptr;
}

// Startup-only providers in the consolidated core keep their symbols while
// off. Complete ownership suppresses stale standalone fallback even when the
// feature is disabled or its initializer has not succeeded yet.
inline HMODULE startup_module_provider(
    const char* standalone_name,
    std::initializer_list<const char*> required_exports) noexcept {
    HMODULE core = GetModuleHandleA("A2FOExtensions.dll");
    if (core) {
        bool complete = required_exports.size() != 0;
        for (const char* name : required_exports)
            if (!name || !GetProcAddress(core, name)) complete = false;
        if (complete) {
            using Ready = bool (__cdecl *)(const char*);
            const auto address = GetProcAddress(core, "A2FOBuiltinModule_StartupReady");
            Ready ready = nullptr;
            static_assert(sizeof(ready) == sizeof(address));
            // Avoid compiler-dependent function-pointer casts.
            std::memcpy(&ready, &address, sizeof(ready));
            return !ready || ready(standalone_name) ? core : nullptr;
        }
    }
    return standalone_name ? GetModuleHandleA(standalone_name) : nullptr;
}

}  // namespace a2fo
