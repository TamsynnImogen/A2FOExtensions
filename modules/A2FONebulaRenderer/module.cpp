/*
 * ODF controller for the core-owned early DX8 Nebula renderer.
 *
 * A2FOExtensions must own the renderer hooks because Armada creates its
 * shared DOT3 shader before deferred modules load. This optional module keeps
 * mod policy out of the core: it reads CraftClass ODF commands, resolves loose
 * texture assets through the active extension-root stack, and registers the
 * resulting class policy with the core renderer.
 */

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
std::uintptr_t __cdecl a2fo_nebula_module_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
}

namespace {

constexpr char kModuleName[] = "A2FONebulaRenderer";
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::size_t kMaximumTexturePath = 1024;
constexpr std::array<const char*, 6> kEmissiveCommands{
    "emissiveWarp",
    "emissiveImpulse",
    "emissiveShields",
    "emissiveLifeSupport",
    "emissiveSensors",
    "emissiveWeapons",
};
constexpr std::array<const char*, 6> kEmissiveSuffixes{
    "Warp", "Impulse", "Shields", "LifeSupport", "Sensors", "Weapons"};
constexpr std::size_t kMaximumIndexedTextures = 64;
constexpr std::array<const char*, 4> kTextureSubdirectories{
    "", "RGB", "Index8", "Compressed"};
constexpr std::array<const char*, 5> kTextureExtensions{
    "", ".dds", ".tga", ".png", ".bmp"};

using StatusFunction = int (__cdecl*)();
using RegisterEmissiveClassFunction = int (__cdecl*)(
    void* object_class, const char* const* texture_paths,
    std::uint32_t texture_path_count);
using RegisterEmissiveMaterialsFunction = int (__cdecl*)(
    void* object_class, const char* const* diffuse_names,
    const char* const* texture_paths, std::uint32_t material_count,
    std::uint32_t texture_paths_per_material);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
RegisterEmissiveClassFunction g_register_emissive_class = nullptr;
RegisterEmissiveMaterialsFunction g_register_emissive_materials = nullptr;
std::vector<std::string> g_extension_roots;

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

template <typename Function>
Function imported_function(HMODULE module, const char* name) noexcept {
    FARPROC exported = module ? GetProcAddress(module, name) : nullptr;
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(exported),
                  "32-bit function pointers must match FARPROC");
    std::memcpy(&function, &exported, sizeof(function));
    return function;
}

std::string trim(std::string value) {
    std::size_t first = 0;
    while (first < value.size() && std::isspace(
               static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first && std::isspace(
               static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    value = value.substr(first, last - first);
    for (char& character : value) {
        if (character == '/') character = '\\';
    }
    return value;
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    std::string result = left;
    if (result.back() != '\\' && result.back() != '/') result.push_back('\\');
    result += right;
    return result;
}

bool regular_file(const std::string& path) noexcept {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool absolute_path(const std::string& path) noexcept {
    const bool drive_path = path.size() >= 3 && std::isalpha(
        static_cast<unsigned char>(path[0])) && path[1] == ':' &&
        (path[2] == '\\' || path[2] == '/');
    const bool unc_path = path.size() >= 2 &&
        path[0] == '\\' && path[1] == '\\';
    return drive_path || unc_path;
}

bool has_extension(const std::string& path) noexcept {
    const std::size_t slash = path.find_last_of("\\/");
    const std::size_t dot = path.find_last_of('.');
    return dot != std::string::npos &&
        (slash == std::string::npos || dot > slash + 0);
}

bool starts_with_textures(const std::string& value) noexcept {
    constexpr char prefix[] = "textures\\";
    if (value.size() < sizeof(prefix) - 1) return false;
    for (std::size_t index = 0; index < sizeof(prefix) - 1; ++index) {
        if (std::tolower(static_cast<unsigned char>(value[index])) !=
            prefix[index]) {
            return false;
        }
    }
    return true;
}

std::string resolve_texture(const std::string& requested) {
    const std::string value = trim(requested);
    if (value.empty()) return {};

    const bool append_extension = !has_extension(value);
    const auto try_candidate = [&](const std::string& base) -> std::string {
        const std::size_t first_extension = append_extension ? 1 : 0;
        const std::size_t last_extension = append_extension
            ? kTextureExtensions.size() : 1;
        for (std::size_t index = first_extension;
             index < last_extension; ++index) {
            const std::string candidate = base + kTextureExtensions[index];
            if (regular_file(candidate)) return candidate;
        }
        if (regular_file(base)) return base;
        return {};
    };

    if (absolute_path(value)) return try_candidate(value);

    // The API orders roots from Data to the active mod. Search backwards so a
    // selected-mod texture wins over a parent or stock texture of the same
    // name, matching ordinary Fleet Operations asset precedence.
    for (auto root = g_extension_roots.rbegin();
         root != g_extension_roots.rend(); ++root) {
        if (starts_with_textures(value)) {
            std::string found = try_candidate(join_path(*root, value));
            if (!found.empty()) return found;
        } else {
            for (const char* directory : kTextureSubdirectories) {
                std::string base = join_path(*root, "Textures");
                if (directory && *directory) base = join_path(base, directory);
                std::string found = try_candidate(join_path(base, value));
                if (!found.empty()) return found;
            }
            // Also accept an explicitly root-relative path for debug assets.
            std::string found = try_candidate(join_path(*root, value));
            if (!found.empty()) return found;
        }
    }
    return {};
}

bool read_parameter_string(void* parameter_db, const char* command,
                           std::string* output) noexcept {
    if (!parameter_db || !command || !output || !g_armada) return false;
    std::array<char, kMaximumTexturePath> value{};
    const std::uintptr_t found = a2fo_nebula_module_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(command),
        reinterpret_cast<std::uintptr_t>(value.data()),
        static_cast<std::uintptr_t>(value.size()),
        reinterpret_cast<std::uintptr_t>(""));
    value.back() = '\0';
    if ((found & 0xffu) == 0 || value[0] == '\0') return false;
    try {
        *output = trim(value.data());
        return !output->empty();
    } catch (...) {
        return false;
    }
}

void cache_extension_roots() {
    g_extension_roots.clear();
    if (!g_api || !g_api->extension_root_count || !g_api->extension_root) {
        return;
    }
    const std::uint32_t count = g_api->extension_root_count();
    for (std::uint32_t index = 0; index < count; ++index) {
        const char* root = g_api->extension_root(index);
        if (root && *root) g_extension_roots.emplace_back(root);
    }
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->extension_root_count ||
        !api->extension_root) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    HMODULE core = GetModuleHandleA("A2FOExtensions.dll");
    const StatusFunction status = imported_function<StatusFunction>(
        core, "A2FO_NebulaRendererStatus");
    g_register_emissive_class =
        imported_function<RegisterEmissiveClassFunction>(
            core, "A2FO_NebulaRegisterEmissiveClass");
    g_register_emissive_materials =
        imported_function<RegisterEmissiveMaterialsFunction>(
            core, "A2FO_NebulaRegisterEmissiveMaterials");
    if (!g_armada || !status || !g_register_emissive_class ||
        !g_register_emissive_materials) {
        log_line("Updated core renderer exports are unavailable");
        return false;
    }
    cache_extension_roots();

    switch (status()) {
        case 2:
            log_line("Core-owned early DX8 renderer is active");
            break;
        case 1:
            log_line("Core-owned DX8 renderer hooks are armed; waiting for the first DOT3 mesh");
            break;
        case -1:
            log_line("Core-owned DX8 renderer disabled during early activation; emissive maps remain inactive");
            break;
        default:
            log_line("Core-owned early DX8 renderer hooks are unavailable");
            break;
    }
    log_line("Subsystem emissive ODF controller initialized");
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FONebulaRenderer_RegisterClass(
    void* object_class, void* parameter_db) {
    if (!g_register_emissive_class || !g_register_emissive_materials ||
        !object_class || !parameter_db) {
        return;
    }

    const auto resolve_command = [parameter_db](
                                     const char* command,
                                     std::string* resolved) {
        std::string requested;
        if (!read_parameter_string(parameter_db, command, &requested)) {
            return false;
        }
        *resolved = resolve_texture(requested);
        if (resolved->empty()) {
            char message[1400]{};
            std::snprintf(message, sizeof(message),
                          "%s texture '%s' was not found as a loose file",
                          command, requested.c_str());
            log_line(message);
            return false;
        }
        return true;
    };

    struct IndexedMaterial {
        std::string diffuse_name;
        std::array<std::string, kEmissiveSuffixes.size()> resolved{};
    };
    std::vector<IndexedMaterial> indexed_materials;
    bool indexed_mode = false;
    for (std::size_t odf_index = 0;
         odf_index < kMaximumIndexedTextures; ++odf_index) {
        char texture_command[32]{};
        std::snprintf(texture_command, sizeof(texture_command),
                      "texture%u", static_cast<unsigned>(odf_index));
        std::string diffuse_name;
        if (!read_parameter_string(
                parameter_db, texture_command, &diffuse_name)) {
            continue;
        }
        indexed_mode = true;

        IndexedMaterial material{};
        material.diffuse_name = diffuse_name;
        bool has_emissive = false;
        for (std::size_t system_index = 0;
             system_index < kEmissiveSuffixes.size(); ++system_index) {
            char emissive_command[64]{};
            std::snprintf(
                emissive_command, sizeof(emissive_command),
                "emissive%u%s", static_cast<unsigned>(odf_index),
                kEmissiveSuffixes[system_index]);
            if (resolve_command(
                    emissive_command,
                    &material.resolved[system_index])) {
                has_emissive = true;
            }
        }
        if (has_emissive) {
            indexed_materials.push_back(std::move(material));
        }
    }

    if (indexed_mode) {
        if (indexed_materials.empty()) return;
        std::vector<const char*> diffuse_names;
        std::vector<const char*> paths;
        diffuse_names.reserve(indexed_materials.size());
        paths.reserve(indexed_materials.size() * kEmissiveSuffixes.size());
        unsigned texture_count = 0;
        for (const IndexedMaterial& material : indexed_materials) {
            diffuse_names.push_back(material.diffuse_name.c_str());
            for (const std::string& path : material.resolved) {
                paths.push_back(path.empty() ? nullptr : path.c_str());
                if (!path.empty()) ++texture_count;
            }
        }
        if (g_register_emissive_materials(
                object_class, diffuse_names.data(), paths.data(),
                static_cast<std::uint32_t>(indexed_materials.size()),
                static_cast<std::uint32_t>(kEmissiveSuffixes.size())) != 0) {
            char message[224]{};
            std::snprintf(
                message, sizeof(message),
                "Registered %u subsystem emissive texture%s across %u "
                "indexed diffuse material%s for CraftClass %p",
                texture_count, texture_count == 1 ? "" : "s",
                static_cast<unsigned>(indexed_materials.size()),
                indexed_materials.size() == 1 ? "" : "s", object_class);
            log_line(message);
        }
        return;
    }

    std::array<std::string, kEmissiveCommands.size()> resolved{};
    std::array<const char*, kEmissiveCommands.size()> paths{};
    bool has_policy = false;
    for (std::size_t index = 0; index < kEmissiveCommands.size(); ++index) {
        if (!resolve_command(kEmissiveCommands[index], &resolved[index])) {
            continue;
        }
        paths[index] = resolved[index].c_str();
        has_policy = true;
    }
    if (!has_policy) return;

    if (g_register_emissive_class(
            object_class, paths.data(),
            static_cast<std::uint32_t>(paths.size())) != 0) {
        unsigned texture_count = 0;
        for (const char* path : paths) {
            if (path && *path) ++texture_count;
        }
        char message[192]{};
        std::snprintf(message, sizeof(message),
                      "Registered %u subsystem emissive texture%s for CraftClass %p",
                      texture_count, texture_count == 1 ? "" : "s",
                      object_class);
        log_line(message);
    }
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // The core copies every path and owns renderer-lifetime policy/cache data.
}
