/*
 * File: core/module_loader.hpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Native DLL discovery and transactional loading API.
 */

#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "../sdk/include/a2fo_module_api.h"

namespace a2fo {

struct LoadedModule {
    HMODULE handle = nullptr;
    A2FO_ModuleShutdownFn shutdown = nullptr;
    std::string path;
};

struct ModuleRegistrationObserver {
    void (*begin)(const std::string& path) = nullptr;
    void (*finish)(const std::string& path, bool initialized) = nullptr;
};

// Roots are ordered Data, oldest parent, ..., selected mod and are used only
// for info.ini policy. Native DLLs are discovered exclusively in the first
// root's modules directory, then selected in deterministic filename order.
bool load_native_modules_from_roots(
    const std::vector<std::string>& roots,
    const A2FO_ModuleApi& api,
    std::vector<LoadedModule>& loaded,
    void (*log_line)(const std::string&),
    const ModuleRegistrationObserver& observer = {});

void unload_native_modules(std::vector<LoadedModule>& loaded,
                           void (*log_line)(const std::string&));

}  // namespace a2fo
