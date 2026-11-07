/*
 * File: core/module_loader.hpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Native DLL discovery and transactional loading API.
 */

#pragma once

#include <windows.h>

#include <string>
#include <vector>
#include <initializer_list>
#include <iterator>

#include "../sdk/include/a2fo_module_api.h"

namespace a2fo {

struct LoadedModule {
    HMODULE handle = nullptr;
    A2FO_ModuleShutdownFn shutdown = nullptr;
    std::string path;
};

// Owns module records and NUL-terminated paths in Rust. This class never
// invokes shutdown callbacks or releases library handles on destruction.
class LoadedModules {
public:
    LoadedModules() noexcept = default;
    LoadedModules(std::initializer_list<LoadedModule> entries);
    LoadedModules(const LoadedModules& other);
    LoadedModules& operator=(const LoadedModules& other);
    LoadedModules(LoadedModules&& other) noexcept;
    LoadedModules& operator=(LoadedModules&& other) noexcept;
    ~LoadedModules();

    std::size_t size() const noexcept;
    bool empty() const noexcept { return size() == 0; }
    void push_back(const LoadedModule& entry);
    void clear() noexcept;
    LoadedModule operator[](std::size_t index) const;

    class const_iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = LoadedModule;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = LoadedModule;
        const_iterator(const LoadedModules* owner, std::size_t index) noexcept
            : owner_(owner), index_(index) {}
        LoadedModule operator*() const { return (*owner_)[index_]; }
        const_iterator& operator++() noexcept { ++index_; return *this; }
        const_iterator operator++(int) noexcept { auto old = *this; ++*this; return old; }
        bool operator==(const const_iterator& other) const noexcept {
            return owner_ == other.owner_ && index_ == other.index_;
        }
        bool operator!=(const const_iterator& other) const noexcept { return !(*this == other); }
    private:
        const LoadedModules* owner_;
        std::size_t index_;
    };
    const_iterator begin() const noexcept { return {this, 0}; }
    const_iterator end() const noexcept { return {this, size()}; }

    // Borrowed until the next mutation or destruction of this owner.
    struct View { uintptr_t handle; uintptr_t shutdown; const uint8_t* path; std::size_t path_size; };
    View view(std::size_t index) const noexcept;
private:
    void* records_ = nullptr;
};

struct ModuleRegistrationObserver {
    void (*begin)(const std::string& path) = nullptr;
    void (*finish)(const std::string& path, bool initialized) = nullptr;
};

// True only for a successfully loaded FeaturePack whose queue hook owner
// confirms that it can publish Producer events. Older providers fail closed.
bool producer_dispatch_available(const LoadedModules& loaded) noexcept;

// Roots are ordered Data, oldest parent, ..., selected mod and are used only
// for info.ini policy. Built-in features and native DLLs from the first root's
// modules directory share deterministic name order. Inactive built-ins are
// prepared once for later behaviour-only menu toggles when supported. Startup-only
// builtins and separate DLLs retain next-process selection.
bool load_native_modules_from_roots(
    const std::vector<std::string>& roots,
    const A2FO_ModuleApi& api,
    LoadedModules& loaded,
    void (*log_line)(const std::string&),
    const ModuleRegistrationObserver& observer = {});

// Detaches the current generation before callbacks and retires it in reverse
// order. Reentrant operations see the public owner's new generation. Native
// logging failures are rethrown only after all retiring handles are released.
void unload_native_modules(LoadedModules& loaded,
                           void (*log_line)(const std::string&));

}  // namespace a2fo
