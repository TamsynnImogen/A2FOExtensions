/*
 * File: core/module_loader.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Native DLL discovery, transactional initialization, and shutdown.
 */

#include "module_loader.hpp"
#include "module_policy.hpp"

#include <algorithm>
#include <cstring>

namespace a2fo {
namespace {

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

bool directory_exists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

}  // namespace

bool load_native_modules_from_roots(
    const std::vector<std::string>& roots,
    const A2FO_ModuleApi& api,
    std::vector<LoadedModule>& loaded,
    void (*log_line)(const std::string&),
    const ModuleRegistrationObserver& observer) {
    if (roots.empty()) {
        log_line("Module loader: no extension roots available");
        return true;
    }

    // Native code has one authoritative installation directory. Mod-local
    // DLL folders are intentionally ignored: info.ini selects global modules
    // by name, so a mod cannot silently replace executable code.
    std::vector<std::string> diagnostics;
    const std::vector<InstalledModule> installed =
        discover_installed_modules(roots.front(), &diagnostics);
    for (const std::string& diagnostic : diagnostics) {
        log_line("Module loader: " + diagnostic);
    }
    for (std::size_t index = 1; index < roots.size(); ++index) {
        const std::string directory = join_path(roots[index], "modules");
        if (directory_exists(directory)) {
            log_line("Module loader: ignored mod-local module directory: " +
                     directory);
        }
    }
    if (installed.empty()) {
        log_line("Module loader: no native modules found");
        return true;
    }

    const ModulePolicy policy = evaluate_module_policy(roots, installed);
    for (const std::string& diagnostic : policy.diagnostics) {
        log_line("Module policy: " + diagnostic);
    }
    log_line(policy.managed
                 ? "Module policy: managed selection enabled"
                 : "Module policy: no [modules] section; legacy load-all mode");

    bool all_ok = policy.valid;
    for (const ModulePolicyEntry& selected : policy.entries) {
        if (selected.state != ModulePolicyState::active &&
            selected.state != ModulePolicyState::required) {
            if (selected.installed) {
                log_line("Module loader: skipped " + selected.filename);
            }
            continue;
        }
        const std::string& path = selected.path;
        HMODULE module = LoadLibraryA(path.c_str());
        if (!module) {
            log_line("Module loader: LoadLibrary failed (error " +
                     std::to_string(GetLastError()) + "): " + path);
            all_ok = false;
            continue;
        }

        A2FO_ModuleInitFn init = nullptr;
        FARPROC init_address = GetProcAddress(module, "A2FO_ModuleInit");
        static_assert(sizeof(init) == sizeof(init_address),
                      "module init pointer must match FARPROC on 32-bit Windows");
        std::memcpy(&init, &init_address, sizeof(init));
        if (!init) {
            log_line("Module loader: missing A2FO_ModuleInit: " + path);
            FreeLibrary(module);
            all_ok = false;
            continue;
        }

        bool initialized = false;
        if (observer.begin) observer.begin(path);
        try {
            initialized = init(&api);
        } catch (...) {
            log_line("Module loader: module threw during initialization: " + path);
        }
        if (observer.finish) observer.finish(path, initialized);
        if (!initialized) {
            log_line("Module loader: module rejected initialization: " + path);
            FreeLibrary(module);
            all_ok = false;
            continue;
        }

        LoadedModule record;
        record.handle = module;
        FARPROC shutdown_address =
            GetProcAddress(module, "A2FO_ModuleShutdown");
        static_assert(sizeof(record.shutdown) == sizeof(shutdown_address),
                      "module shutdown pointer must match FARPROC on 32-bit Windows");
        std::memcpy(&record.shutdown, &shutdown_address,
                    sizeof(record.shutdown));
        record.path = path;
        loaded.push_back(record);
        log_line("Module loader: loaded " + path);
    }
    return all_ok;
}

void unload_native_modules(std::vector<LoadedModule>& loaded,
                           void (*log_line)(const std::string&)) {
    for (auto iterator = loaded.rbegin(); iterator != loaded.rend(); ++iterator) {
        if (iterator->shutdown) {
            try {
                iterator->shutdown();
            } catch (...) {
                log_line("Module loader: module threw during shutdown: " + iterator->path);
            }
        }
        if (iterator->handle) {
            FreeLibrary(iterator->handle);
        }
    }
    loaded.clear();
}

}  // namespace a2fo
