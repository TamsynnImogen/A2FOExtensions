#include "module_loader.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>

namespace a2fo {
namespace {

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

bool has_dll_extension(const std::string& name) {
    if (name.size() < 4) return false;
    const std::string extension = name.substr(name.size() - 4);
    return _stricmp(extension.c_str(), ".dll") == 0;
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
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

    // The Data-level directory is part of the installation layout and may be
    // created automatically. Never create directories inside a mod merely by
    // selecting it.
    CreateDirectoryA(join_path(roots.front(), "modules").c_str(), nullptr);

    std::map<std::string, std::string> selected_paths;
    for (const std::string& root : roots) {
        const std::string directory = join_path(root, "modules");
        WIN32_FIND_DATAA data{};
        HANDLE search = FindFirstFileA(
            join_path(directory, "*.dll").c_str(), &data);
        if (search == INVALID_HANDLE_VALUE) continue;
        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
                !has_dll_extension(data.cFileName)) {
                continue;
            }
            const std::string path = join_path(directory, data.cFileName);
            const std::string key = lower_ascii(data.cFileName);
            const auto previous = selected_paths.find(key);
            if (previous != selected_paths.end()) {
                log_line("Module loader: " + path + " overrides " +
                         previous->second);
            }
            selected_paths[key] = path;
        } while (FindNextFileA(search, &data));
        FindClose(search);
    }

    if (selected_paths.empty()) {
        log_line("Module loader: no native modules found");
        return true;
    }

    bool all_ok = true;
    for (const auto& selected : selected_paths) {
        const std::string& path = selected.second;
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
