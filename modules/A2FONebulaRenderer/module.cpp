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
#include "art_texture_suffix_config.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern "C" {
std::uintptr_t __cdecl a2fo_nebula_module_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_nebula_module_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
std::uintptr_t __cdecl a2fo_nebula_module_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
std::uintptr_t __cdecl a2fo_nebula_module_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
std::uintptr_t __cdecl a2fo_nebula_module_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
}

namespace {

constexpr char kModuleName[] = "A2FONebulaRenderer";
constexpr char kArtConfigFileName[] = "ART_CFG.h";
constexpr std::streamoff kMaximumArtConfigSize = 2 * 1024 * 1024;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kParameterDbGetFloatRva = 0x00134df0;
constexpr std::uintptr_t kParameterDbGetStringVectorRva = 0x00135e80;
constexpr std::uintptr_t kEngineOperatorDeleteRva = 0x002527d0;
constexpr std::uintptr_t kMeshGetTextureRva = 0x00231380;
constexpr std::uintptr_t kMeshSetTextureRva = 0x002313b0;
constexpr std::uintptr_t kMeshUpdateRva = 0x00231c10;
constexpr std::uintptr_t kTextureFileExistsRva = 0x002400d0;
constexpr std::uintptr_t kTextureFindRva = 0x00242870;
constexpr std::size_t kCraftClassGeometryOffset = 0x01d8;
constexpr std::size_t kGeometryRootNodeOffset = 0x003c;
constexpr std::size_t kNodeNameOffset = 0x0008;
constexpr std::size_t kNodeSiblingOffset = 0x001c;
constexpr std::size_t kNodeChildOffset = 0x0020;
constexpr std::size_t kMaximumNodeCount = 4096;
constexpr std::size_t kMeshRenderFlagsOffset = 0x012c;
constexpr std::size_t kMeshRendererOffset = 0x0128;
constexpr std::uint32_t kMeshDot3RenderFlag = 0x00000002u;
// Armada 2 v1.93 SODs store 0x200 on the second texture of a native
// diffuse+bump material.  Passing the diffuse texture's ordinary load flags
// here leaves a tangent-space normal map as visible blue RGB in legacy
// fallback passes instead of creating Storm3D's bump texture representation.
constexpr std::uint32_t kBumpTextureLoadFlag = 0x00000200u;
constexpr std::size_t kTextureDatabaseOffset = 0x0004;
constexpr std::size_t kTextureNameOffset = 0x0008;
constexpr std::size_t kTextureFlagsOffset = 0x0018;
constexpr std::size_t kMaximumStormTextureName = 1024;
constexpr std::size_t kMaximumDamageDecalsPerSystem = 64;
constexpr std::size_t kMaximumLogoDecals = 64;

struct NativeStringVector {
    std::uint32_t allocator = 0;
    char** begin = nullptr;
    char** end = nullptr;
    char** capacity = nullptr;
};
static_assert(sizeof(NativeStringVector) == 16,
              "Armada string-vector ABI must occupy sixteen bytes");
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
constexpr std::array<const char*, 6> kGlobalEmissiveSystemTokens{
    "warp", "impulse", "shields", "life", "sensor", "weapons"};
constexpr std::size_t kMaximumIndexedTextures = 64;
constexpr std::array<const char*, 4> kTextureSubdirectories{
    "", "RGB", "Index8", "Compressed"};
constexpr std::array<const char*, 5> kTextureExtensions{
    "", ".dds", ".tga", ".png", ".bmp"};

constexpr std::array<std::uint8_t, 7> kExpectedMeshGetTexture{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x8b};
constexpr std::array<std::uint8_t, 7> kExpectedMeshSetTexture{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x10, 0x53};
constexpr std::array<std::uint8_t, 7> kExpectedMeshUpdate{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x53};
constexpr std::array<std::uint8_t, 6> kExpectedTextureFind{
    0x55, 0x8b, 0xec, 0x64, 0xa1, 0x00};

using StatusFunction = int (__cdecl*)();
using SetEmissiveBumpMultiplierFunction = int (__cdecl*)(float);
using SetBumpLightBiasFunction = int (__cdecl*)(float);
using SetEmissiveDiffuseRestoreFunction = int (__cdecl*)(float);
using TextureFileExistsFunction = bool (__cdecl*)(const char*);
using TextureFindFunction = void* (__cdecl*)(
    void* database, const char* texture_name, std::uint32_t flags);
using RegisterEmissiveClassFunction = int (__cdecl*)(
    void* object_class, const char* const* texture_paths,
    std::uint32_t texture_path_count);
using RegisterEmissiveMaterialsFunction = int (__cdecl*)(
    void* object_class, const char* const* diffuse_names,
    const char* const* texture_paths, std::uint32_t material_count,
    std::uint32_t texture_paths_per_material);
using RegisterSpecularMaterialsFunction = int (__cdecl*)(
    void* object_class, const char* const* diffuse_names,
    const char* const* texture_paths, std::uint32_t material_count);

struct DamageDecalDescriptor {
    std::uint32_t struct_size = sizeof(DamageDecalDescriptor);
    std::uint32_t system_index = 0;
    std::uint32_t threshold_index = 1;
    void* node = nullptr;
    const char* texture_path = nullptr;
    float offset[3]{};
    float rotation_degrees[3]{};
    float size[2]{1.0f, 1.0f};
};

using RegisterDamageDecalClassFunction = int (__cdecl*)(
    void* object_class, float damage_threshold,
    const DamageDecalDescriptor* descriptors,
    std::uint32_t descriptor_count);

struct LogoDecalDescriptor {
    std::uint32_t struct_size = sizeof(LogoDecalDescriptor);
    void* node = nullptr;
    const char* const* texture_paths = nullptr;
    std::uint32_t texture_path_count = 0;
    std::uint32_t use_colour_key = 0;
    std::uint32_t colour_key = 0;
    std::uint32_t flip_u = 0;
    float offset[3]{};
    float rotation_degrees[3]{};
    float size[2]{1.0f, 1.0f};
};

using RegisterLogoDecalClassFunction = int (__cdecl*)(
    void* object_class, const LogoDecalDescriptor* descriptors,
    std::uint32_t descriptor_count);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
RegisterEmissiveClassFunction g_register_emissive_class = nullptr;
RegisterEmissiveMaterialsFunction g_register_emissive_materials = nullptr;
RegisterSpecularMaterialsFunction g_register_specular_materials = nullptr;
SetEmissiveBumpMultiplierFunction g_set_emissive_bump_multiplier = nullptr;
SetBumpLightBiasFunction g_set_bump_light_bias = nullptr;
SetEmissiveDiffuseRestoreFunction g_set_emissive_diffuse_restore = nullptr;
RegisterDamageDecalClassFunction g_register_damage_decal_class = nullptr;
RegisterLogoDecalClassFunction g_register_logo_decal_class = nullptr;
TextureFileExistsFunction g_texture_file_exists = nullptr;
TextureFindFunction g_texture_find = nullptr;
void* g_mesh_get_texture = nullptr;
void* g_mesh_set_texture = nullptr;
void* g_mesh_update = nullptr;
bool g_mesh_texture_runtime_supported = false;
bool g_core_renderer_available = false;
a2fo::nebula::ArtTextureSuffixConfig g_art_texture_suffix_config;
std::vector<std::string> g_extension_roots;
std::unordered_map<std::string, std::string> g_resolved_texture_cache;
std::unordered_map<std::string, bool> g_native_texture_exists_cache;
std::unordered_set<std::string> g_loose_texture_keys;

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

template <typename Function>
Function function_from_address(void* address) noexcept {
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(address),
                  "32-bit function pointers must match data pointers");
    std::memcpy(&function, &address, sizeof(function));
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

std::string append_texture_suffix(
    const std::string& source, const std::string& suffix) {
    if (suffix.empty()) return source;
    const std::size_t slash = source.find_last_of("\\/");
    const std::size_t dot = source.find_last_of('.');
    if (dot == std::string::npos ||
        (slash != std::string::npos && dot <= slash + 1)) {
        return source + suffix;
    }
    std::string result = source;
    result.insert(dot, suffix);
    return result;
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
    const auto cached = g_resolved_texture_cache.find(value);
    if (cached != g_resolved_texture_cache.end()) return cached->second;

    const auto remember = [&value](std::string result) {
        g_resolved_texture_cache.emplace(value, result);
        return result;
    };

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

    if (absolute_path(value)) return remember(try_candidate(value));

    // The API orders roots from Data to the active mod. Search backwards so a
    // selected-mod texture wins over a parent or stock texture of the same
    // name, matching ordinary Fleet Operations asset precedence.
    for (auto root = g_extension_roots.rbegin();
         root != g_extension_roots.rend(); ++root) {
        if (starts_with_textures(value)) {
            std::string found = try_candidate(join_path(*root, value));
            if (!found.empty()) return remember(std::move(found));
        } else {
            for (const char* directory : kTextureSubdirectories) {
                std::string base = join_path(*root, "Textures");
                if (directory && *directory) base = join_path(base, directory);
                std::string found = try_candidate(join_path(base, value));
                if (!found.empty()) return remember(std::move(found));
            }
            // Also accept an explicitly root-relative path for debug assets.
            std::string found = try_candidate(join_path(*root, value));
            if (!found.empty()) return remember(std::move(found));
        }
    }
    return remember({});
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

bool read_parameter_float(void* parameter_db, const char* command,
                          float* output) noexcept {
    if (!parameter_db || !command || !output || !g_armada) return false;
    float value = *output;
    std::uint32_t fallback = 0;
    std::memcpy(&fallback, &value, sizeof(fallback));
    const std::uintptr_t found = a2fo_nebula_module_call_thiscall_3(
        at(g_armada, kParameterDbGetFloatRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(command),
        reinterpret_cast<std::uintptr_t>(&value), fallback);
    if ((found & 0xffu) == 0 || !std::isfinite(value)) return false;
    *output = value;
    return true;
}

bool readable_range(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) !=
            sizeof(information) || information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto end = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress) + information.RegionSize;
    return start <= end && size <= end - start;
}

template <typename Value>
Value read_at(const void* object, std::size_t offset,
              Value fallback = Value{}) noexcept {
    if (!object) return fallback;
    const auto* address = static_cast<const std::uint8_t*>(object) + offset;
    if (!readable_range(address, sizeof(Value))) return fallback;
    Value value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

bool writable_range(void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) !=
            sizeof(information) || information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD protection = information.Protect & 0xffu;
    if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY &&
        protection != PAGE_EXECUTE_READWRITE &&
        protection != PAGE_EXECUTE_WRITECOPY) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto end = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress) + information.RegionSize;
    return start <= end && size <= end - start;
}

template <typename Value>
bool write_at(void* object, std::size_t offset, const Value& value) noexcept {
    if (!object) return false;
    auto* address = static_cast<std::uint8_t*>(object) + offset;
    if (!writable_range(address, sizeof(value))) return false;
    std::memcpy(address, &value, sizeof(value));
    return true;
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::array<std::uint8_t, Size>& expected)
    noexcept {
    const void* address = at(module, rva);
    return readable_range(address, expected.size()) &&
        std::memcmp(address, expected.data(), expected.size()) == 0;
}

bool bounded_texture_name(const char* source, std::string* output) noexcept {
    if (!source || !output) return false;
    try {
        std::string copied;
        copied.reserve(128);
        for (std::size_t index = 0;
             index < kMaximumStormTextureName; ++index) {
            if (!readable_range(source + index, 1)) return false;
            if (source[index] == '\0') {
                *output = trim(std::move(copied));
                return !output->empty();
            }
            copied.push_back(source[index]);
        }
    } catch (...) {}
    return false;
}

struct ClassMeshMaterial {
    void* mesh = nullptr;
    void* diffuse_texture = nullptr;
    std::string diffuse_name;
};

std::vector<ClassMeshMaterial> collect_class_mesh_materials(
    void* object_class) {
    std::vector<ClassMeshMaterial> materials;
    if (!object_class || !g_mesh_get_texture) return materials;
    void* geometry = read_at<void*>(
        object_class, kCraftClassGeometryOffset, nullptr);
    void* root = read_at<void*>(geometry, kGeometryRootNodeOffset, nullptr);
    if (!root) return materials;

    std::vector<void*> pending{root};
    std::unordered_set<void*> visited;
    while (!pending.empty() && visited.size() < kMaximumNodeCount) {
        void* node = pending.back();
        pending.pop_back();
        if (!node || !visited.insert(node).second) continue;
        void* child = read_at<void*>(node, kNodeChildOffset, nullptr);
        void* sibling = read_at<void*>(node, kNodeSiblingOffset, nullptr);
        if (child) pending.push_back(child);
        if (sibling) pending.push_back(sibling);

        void* vtable = read_at<void*>(node, 0, nullptr);
        void* get_type = read_at<void*>(vtable, sizeof(void*), nullptr);
        if (!get_type || !readable_range(get_type, 1) ||
            a2fo_nebula_module_call_thiscall_0(get_type, node) != 1) {
            continue;
        }
        void* diffuse = reinterpret_cast<void*>(
            a2fo_nebula_module_call_thiscall_1(
                g_mesh_get_texture, node, 0));
        const char* native_name = read_at<const char*>(
            diffuse, kTextureNameOffset, nullptr);
        std::string diffuse_name;
        if (diffuse && bounded_texture_name(native_name, &diffuse_name)) {
            materials.push_back({node, diffuse, std::move(diffuse_name)});
        }
    }
    return materials;
}

bool normalized_node_name(void* node, std::string* output) noexcept {
    if (!node || !output) return false;
    const char* name = read_at<const char*>(node, kNodeNameOffset, nullptr);
    if (!name) return false;
    try {
        std::string copied;
        for (std::size_t index = 0; index < 128; ++index) {
            if (!readable_range(name + index, 1)) return false;
            if (name[index] == '\0') {
                for (char& character : copied) {
                    character = static_cast<char>(std::tolower(
                        static_cast<unsigned char>(character)));
                }
                *output = std::move(copied);
                return !output->empty();
            }
            copied.push_back(name[index]);
        }
    } catch (...) {}
    return false;
}

void* find_class_node(void* object_class, const std::string& requested)
    noexcept {
    void* geometry = read_at<void*>(
        object_class, kCraftClassGeometryOffset, nullptr);
    void* root = read_at<void*>(geometry, kGeometryRootNodeOffset, nullptr);
    if (!root) return nullptr;
    std::string wanted = requested;
    for (char& character : wanted) {
        character = static_cast<char>(std::tolower(
            static_cast<unsigned char>(character)));
    }
    try {
        std::vector<void*> pending{root};
        std::unordered_set<void*> visited;
        while (!pending.empty() && visited.size() < kMaximumNodeCount) {
            void* node = pending.back();
            pending.pop_back();
            if (!node || !visited.insert(node).second) continue;
            void* child = read_at<void*>(node, kNodeChildOffset, nullptr);
            void* sibling = read_at<void*>(node, kNodeSiblingOffset, nullptr);
            if (child) pending.push_back(child);
            if (sibling) pending.push_back(sibling);
            std::string name;
            if (normalized_node_name(node, &name) && name == wanted) {
                return node;
            }
        }
    } catch (...) {}
    return nullptr;
}

bool parse_float_values(const std::string& text, float* values,
                        std::size_t count) noexcept {
    if (!values || count == 0) return false;
    const char* cursor = text.c_str();
    for (std::size_t index = 0; index < count; ++index) {
        while (*cursor && (std::isspace(
                   static_cast<unsigned char>(*cursor)) || *cursor == ',')) {
            ++cursor;
        }
        if (!*cursor) return false;
        char* end = nullptr;
        const float value = std::strtof(cursor, &end);
        if (end == cursor || !std::isfinite(value)) return false;
        values[index] = value;
        cursor = end;
    }
    return true;
}

void read_float_vector(void* parameter_db, const std::string& command,
                       float* values, std::size_t count) noexcept {
    std::string text;
    if (read_parameter_string(
            parameter_db, command.c_str(), &text)) {
        parse_float_values(text, values, count);
    }
}

float model_scale(void* parameter_db) noexcept {
    float scale = 1.0f;
    read_parameter_float(parameter_db, "ScaleSOD", &scale);
    return std::isfinite(scale) && scale > 0.0f ? scale : 1.0f;
}

template <typename Descriptor>
void apply_model_scale(Descriptor& descriptor, float scale) noexcept {
    for (float& component : descriptor.offset) component *= scale;
    for (float& component : descriptor.size) component *= scale;
}

std::vector<std::string> read_parameter_list(
    void* parameter_db, const char* command,
    bool preserve_empty_rows = false) noexcept {
    std::vector<std::string> result;
    if (!parameter_db || !command) return result;
    NativeStringVector native{};
    a2fo_nebula_module_call_thiscall_3(
        at(g_armada, kParameterDbGetStringVectorRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(command),
        reinterpret_cast<std::uintptr_t>(&native), 0);
    const auto begin = reinterpret_cast<std::uintptr_t>(native.begin);
    const auto end = reinterpret_cast<std::uintptr_t>(native.end);
    std::size_t count = 0;
    if (end >= begin && (end - begin) % sizeof(char*) == 0) {
        count = (end - begin) / sizeof(char*);
    }
    try {
        if (count <= 256 && (count == 0 || readable_range(
                native.begin, count * sizeof(char*)))) {
            result.reserve(count);
            for (std::size_t index = 0; index < count; ++index) {
                const char* value = native.begin[index];
                std::string copied;
                if (value && readable_range(value, 1)) copied = value;
                if (preserve_empty_rows || !copied.empty()) {
                    result.push_back(std::move(copied));
                }
            }
        }
    } catch (...) { result.clear(); }
    using DeleteFunction = void (__cdecl*)(void*);
    const auto engine_delete = reinterpret_cast<DeleteFunction>(
        at(g_armada, kEngineOperatorDeleteRva));
    if (engine_delete && count <= 256) {
        for (std::size_t index = 0; index < count; ++index) {
            if (native.begin[index]) engine_delete(native.begin[index]);
        }
        if (native.begin) engine_delete(native.begin);
    }
    return result;
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

bool read_small_art_config(const std::string& path,
                           std::string* contents) {
    if (!contents) return false;
    contents->clear();
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || size > kMaximumArtConfigSize) {
        log_line("Ignored oversized ART configuration: " + path);
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::ostringstream stream;
    stream << input.rdbuf();
    *contents = stream.str();
    return input.good() || input.eof();
}

void load_art_texture_suffix_config() {
    g_art_texture_suffix_config = {};
    for (const std::string& root : g_extension_roots) {
        const std::string path = join_path(root, kArtConfigFileName);
        std::string source;
        if (!read_small_art_config(path, &source)) continue;
        const auto report = a2fo::nebula::parse_art_texture_suffix_config(
            source, &g_art_texture_suffix_config);
        if (report.valid_assignments != 0) {
            char message[512]{};
            std::snprintf(
                message, sizeof(message),
                "Applied %u global texture ART assignment%s from %s",
                report.valid_assignments,
                report.valid_assignments == 1 ? "" : "s", path.c_str());
            log_line(message);
        }
        if (report.invalid_assignments != 0) {
            char message[512]{};
            std::snprintf(
                message, sizeof(message),
                "Ignored %u invalid global texture ART assignment%s in %s",
                report.invalid_assignments,
                report.invalid_assignments == 1 ? "" : "s", path.c_str());
            log_line(message);
        }
    }
    if (!g_art_texture_suffix_config.emissive_suffix.empty() ||
        !g_art_texture_suffix_config.bump_suffix.empty() ||
        !g_art_texture_suffix_config.specular_suffix.empty()) {
        char message[384]{};
        std::snprintf(
            message, sizeof(message),
            "Global texture settings: emissive='%s' bump='%s' specular='%s' "
            "bump-emissive=%.2fx bump-light-bias=%.2f "
            "emissive-diffuse-restore=%.2f",
            g_art_texture_suffix_config.emissive_suffix.c_str(),
            g_art_texture_suffix_config.bump_suffix.c_str(),
            g_art_texture_suffix_config.specular_suffix.c_str(),
            g_art_texture_suffix_config.emissive_bump_multiplier,
            g_art_texture_suffix_config.bump_light_bias,
            g_art_texture_suffix_config.emissive_diffuse_restore);
        log_line(message);
    }
}

std::string texture_name_without_extension(std::string value) {
    const std::size_t slash = value.find_last_of("\\/");
    const std::size_t dot = value.find_last_of('.');
    if (dot != std::string::npos &&
        (slash == std::string::npos || dot > slash + 1)) {
        value.resize(dot);
    }
    return value;
}

std::string normalized_material_key(std::string value) {
    value = texture_name_without_extension(std::move(value));
    const std::size_t slash = value.find_last_of("\\/");
    if (slash != std::string::npos) value.erase(0, slash + 1);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

void index_loose_texture_names() {
    g_loose_texture_keys.clear();
    if (g_art_texture_suffix_config.emissive_suffix.empty() &&
        g_art_texture_suffix_config.specular_suffix.empty()) return;
    try {
        std::array<std::string, 2> lowered_suffixes{{
            g_art_texture_suffix_config.emissive_suffix,
            g_art_texture_suffix_config.specular_suffix}};
        for (std::string& suffix : lowered_suffixes) {
            std::transform(
                suffix.begin(), suffix.end(), suffix.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                });
        }
        for (const std::string& root : g_extension_roots) {
            for (const char* directory : kTextureSubdirectories) {
                std::string folder = join_path(root, "Textures");
                if (directory && *directory) {
                    folder = join_path(folder, directory);
                }
                WIN32_FIND_DATAA entry{};
                HANDLE search = FindFirstFileA(
                    join_path(folder, "*").c_str(), &entry);
                if (search == INVALID_HANDLE_VALUE) continue;
                do {
                    if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) !=
                        0) {
                        continue;
                    }
                    std::string key = normalized_material_key(entry.cFileName);
                    const bool mapped_candidate = std::any_of(
                        lowered_suffixes.begin(), lowered_suffixes.end(),
                        [&key](const std::string& suffix) {
                            return !suffix.empty() &&
                                key.find(suffix) != std::string::npos;
                        });
                    if (!key.empty() && mapped_candidate) {
                        g_loose_texture_keys.insert(std::move(key));
                    }
                } while (FindNextFileA(search, &entry));
                FindClose(search);
            }
        }
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "Indexed %u loose global mapped-texture candidate name%s",
            static_cast<unsigned>(g_loose_texture_keys.size()),
            g_loose_texture_keys.size() == 1 ? "" : "s");
        log_line(message);
    } catch (...) {
        g_loose_texture_keys.clear();
        log_line("Could not index loose texture names; global mapped-texture discovery disabled");
    }
}

std::string native_texture_asset_path(
    const std::string& texture_name, const char* extension) {
    std::string result = texture_name_without_extension(texture_name);
    for (char& character : result) {
        if (character == '/') character = '\\';
    }
    if (!starts_with_textures(result)) result.insert(0, "Textures\\");
    if (extension) result += extension;
    return result;
}

bool native_texture_exists(const std::string& texture_name,
                           const char* extension) noexcept {
    if (!g_texture_file_exists || texture_name.empty() || !extension) {
        return false;
    }
    try {
        const std::string path = native_texture_asset_path(
            texture_name, extension);
        const auto cached = g_native_texture_exists_cache.find(path);
        if (cached != g_native_texture_exists_cache.end()) {
            return cached->second;
        }
        const bool exists = g_texture_file_exists(path.c_str());
        g_native_texture_exists_cache.emplace(path, exists);
        return exists;
    } catch (...) {
        return false;
    }
}

void* find_native_texture_variant(
    void* diffuse_texture, const std::string& requested) noexcept {
    if (!diffuse_texture || requested.empty() || !g_texture_find) {
        return nullptr;
    }
    try {
        const std::string base = texture_name_without_extension(requested);
        if (!native_texture_exists(base, ".dds") &&
            !native_texture_exists(base, ".tga")) {
            return nullptr;
        }
        void* database = read_at<void*>(
            diffuse_texture, kTextureDatabaseOffset, nullptr);
        const std::uint32_t flags = read_at<std::uint32_t>(
            diffuse_texture, kTextureFlagsOffset, 0) |
            kBumpTextureLoadFlag;
        return g_texture_find(database, base.c_str(), flags);
    } catch (...) {
        return nullptr;
    }
}

void apply_global_bump_suffix(
    const std::vector<ClassMeshMaterial>& materials) noexcept {
    const std::string& suffix = g_art_texture_suffix_config.bump_suffix;
    if (suffix.empty() || !g_mesh_texture_runtime_supported) return;
    try {
        unsigned applied = 0;
        for (const ClassMeshMaterial& material : materials) {
            if (!material.mesh || !material.diffuse_texture) continue;
            void* existing = reinterpret_cast<void*>(
                a2fo_nebula_module_call_thiscall_1(
                    g_mesh_get_texture, material.mesh, 1));
            if (existing) continue;

            const std::string requested =
                a2fo::nebula::texture_name_with_suffix(
                    material.diffuse_name, suffix);
            void* bump = find_native_texture_variant(
                material.diffuse_texture, requested);
            if (!bump) continue;

            const std::uint32_t current_flags = read_at<std::uint32_t>(
                material.mesh, kMeshRenderFlagsOffset, 0);
            const std::uint32_t dot3_flags =
                current_flags | kMeshDot3RenderFlag;
            if (!write_at(
                    material.mesh, kMeshRenderFlagsOffset, dot3_flags)) {
                continue;
            }
            a2fo_nebula_module_call_thiscall_2(
                g_mesh_set_texture, material.mesh,
                reinterpret_cast<std::uintptr_t>(bump), 1);
            // Update(0) discards any already-created standard MeshVB and
            // rebuilds it from the newly selected DOT3 material without
            // recalculating or modifying the model's source geometry.
            const std::uintptr_t update_result =
                a2fo_nebula_module_call_thiscall_1(
                    g_mesh_update, material.mesh, 0);
            const void* rebuilt_renderer = read_at<void*>(
                material.mesh, kMeshRendererOffset, nullptr);
            if (update_result != 0 || !rebuilt_renderer) {
                // Leave the original material usable if Storm3D cannot build
                // its DOT3 renderer for this mesh.
                a2fo_nebula_module_call_thiscall_2(
                    g_mesh_set_texture, material.mesh, 0, 1);
                write_at(material.mesh, kMeshRenderFlagsOffset, current_flags);
                a2fo_nebula_module_call_thiscall_1(
                    g_mesh_update, material.mesh, 0);
                continue;
            }
            ++applied;
        }
        if (applied != 0) {
            char message[192]{};
            std::snprintf(
                message, sizeof(message),
                "Applied global bump suffix with native 0x200 load flags to %u SOD material%s",
                applied, applied == 1 ? "" : "s");
            log_line(message);
        }
    } catch (...) {
        log_line("Could not apply the global bump suffix; native materials retained");
    }
}

void register_global_specular(
    void* object_class,
    const std::vector<ClassMeshMaterial>& materials) noexcept {
    const std::string& suffix =
        g_art_texture_suffix_config.specular_suffix;
    if (!object_class || suffix.empty() ||
        !g_register_specular_materials) {
        return;
    }
    try {
        std::unordered_set<std::string> registered_keys;
        std::vector<std::string> diffuse_names;
        std::vector<std::string> texture_paths;
        diffuse_names.reserve(materials.size());
        texture_paths.reserve(materials.size());
        for (const ClassMeshMaterial& material : materials) {
            const std::string key = normalized_material_key(
                material.diffuse_name);
            if (key.empty() || !registered_keys.insert(key).second) continue;
            const std::string requested =
                a2fo::nebula::texture_name_with_suffix(
                    material.diffuse_name, suffix);
            if (g_loose_texture_keys.find(
                    normalized_material_key(requested)) ==
                g_loose_texture_keys.end()) {
                continue;
            }
            std::string resolved = resolve_texture(requested);
            if (resolved.empty()) continue;
            diffuse_names.push_back(material.diffuse_name);
            texture_paths.push_back(std::move(resolved));
        }
        if (diffuse_names.empty()) return;

        std::vector<const char*> diffuse_pointers;
        std::vector<const char*> path_pointers;
        diffuse_pointers.reserve(diffuse_names.size());
        path_pointers.reserve(texture_paths.size());
        for (std::size_t index = 0; index < diffuse_names.size(); ++index) {
            diffuse_pointers.push_back(diffuse_names[index].c_str());
            path_pointers.push_back(texture_paths[index].c_str());
        }
        if (g_register_specular_materials(
                object_class, diffuse_pointers.data(), path_pointers.data(),
                static_cast<std::uint32_t>(diffuse_pointers.size())) != 0) {
            char message[224]{};
            std::snprintf(
                message, sizeof(message),
                "Registered %u global specular map%s for CraftClass %p",
                static_cast<unsigned>(diffuse_pointers.size()),
                diffuse_pointers.size() == 1 ? "" : "s", object_class);
            log_line(message);
        }
    } catch (...) {
        log_line("Could not register global specular maps for a CraftClass");
    }
}

void register_damage_decals(void* object_class, void* parameter_db) noexcept {
    if (!g_register_damage_decal_class || !object_class || !parameter_db) {
        return;
    }
    try {
        const float scale = model_scale(parameter_db);
        float threshold = 0.1f;
        read_parameter_float(parameter_db, "damageThreshold", &threshold);
        if (!std::isfinite(threshold) || threshold <= 0.0f ||
            threshold > 1.0f) return;
        float preview_value = 0.0f;
        read_parameter_float(
            parameter_db, "damageDecalPreview", &preview_value);
        const bool preview = preview_value != 0.0f;

        constexpr std::array<const char*, 6> prefixes{{
            "sensors", "engines", "weapons", "lifeSupport",
            "shieldGenerator", "hull"}};
        constexpr std::array<const char*, 6> target_commands{{
            "sensorsTargetHardpoints", "enginesTargetHardpoints",
            "weaponsTargetHardpoints", "lifeSupportTargetHardpoints",
            "shieldGeneratorTargetHardpoints", "hullTargetHardpoints"}};

        struct PendingDecal {
            DamageDecalDescriptor descriptor;
            std::string texture;
        };
        std::vector<PendingDecal> pending;
        pending.reserve(prefixes.size() * kMaximumDamageDecalsPerSystem);
        std::vector<std::string> global_textures;
        for (std::size_t index = 1;
             index <= kMaximumDamageDecalsPerSystem; ++index) {
            const std::string command =
                "scorchTexture" + std::to_string(index);
            std::string requested;
            if (!read_parameter_string(
                    parameter_db, command.c_str(), &requested)) continue;
            std::string resolved = resolve_texture(requested);
            if (!resolved.empty()) global_textures.push_back(
                std::move(resolved));
        }

        for (std::size_t system = 0; system < prefixes.size(); ++system) {
            bool explicit_entries = false;
            for (std::size_t index = 1;
                 index <= kMaximumDamageDecalsPerSystem; ++index) {
                const std::string base = std::string(prefixes[system]) +
                    "Scorch" + std::to_string(index);
                std::string requested;
                if (!read_parameter_string(
                        parameter_db, base.c_str(), &requested)) continue;
                explicit_entries = true;
                std::string hardpoint;
                if (!read_parameter_string(
                        parameter_db, (base + "Hardpoint").c_str(),
                        &hardpoint)) continue;
                void* node = find_class_node(object_class, hardpoint);
                std::string resolved = resolve_texture(requested);
                if (!node || resolved.empty()) {
                    char message[256]{};
                    std::snprintf(message, sizeof(message),
                                  "Ignored %s: %s%s",
                                  base.c_str(), node ? "texture missing" :
                                  "hardpoint missing", node ? "" :
                                  hardpoint.c_str());
                    log_line(message);
                    continue;
                }
                PendingDecal entry;
                entry.descriptor.system_index =
                    static_cast<std::uint32_t>(system);
                entry.descriptor.threshold_index =
                    preview ? 0u : static_cast<std::uint32_t>(index);
                entry.descriptor.node = node;
                entry.descriptor.size[0] = 6.0f;
                entry.descriptor.size[1] = 6.0f;
                read_float_vector(parameter_db, base + "Offset",
                                  entry.descriptor.offset, 3);
                read_float_vector(parameter_db, base + "Rotation",
                                  entry.descriptor.rotation_degrees, 3);
                read_float_vector(parameter_db, base + "Size",
                                  entry.descriptor.size, 2);
                apply_model_scale(entry.descriptor, scale);
                entry.texture = std::move(resolved);
                pending.push_back(std::move(entry));
            }
            if (explicit_entries || global_textures.empty()) continue;

            const std::vector<std::string> targets = read_parameter_list(
                parameter_db, target_commands[system]);
            for (std::size_t index = 0;
                 index < targets.size() &&
                 index < kMaximumDamageDecalsPerSystem; ++index) {
                void* node = find_class_node(object_class, targets[index]);
                if (!node) continue;
                PendingDecal entry;
                entry.descriptor.system_index =
                    static_cast<std::uint32_t>(system);
                entry.descriptor.threshold_index =
                    preview ? 0u : static_cast<std::uint32_t>(index + 1);
                entry.descriptor.node = node;
                entry.descriptor.size[0] = 6.0f;
                entry.descriptor.size[1] = 6.0f;
                apply_model_scale(entry.descriptor, scale);
                entry.texture = global_textures[
                    index % global_textures.size()];
                pending.push_back(std::move(entry));
            }
        }
        if (pending.empty()) return;
        std::vector<DamageDecalDescriptor> descriptors;
        descriptors.reserve(pending.size());
        for (PendingDecal& entry : pending) {
            entry.descriptor.texture_path = entry.texture.c_str();
            descriptors.push_back(entry.descriptor);
        }
        if (g_register_damage_decal_class(
                object_class, threshold, descriptors.data(),
                static_cast<std::uint32_t>(descriptors.size())) != 0) {
            char message[192]{};
            std::snprintf(message, sizeof(message),
                          "Registered %u subsystem/hull damage decal%s at %.1f%% intervals",
                          static_cast<unsigned>(descriptors.size()),
                          descriptors.size() == 1 ? "" : "s",
                          static_cast<double>(threshold * 100.0f));
            log_line(message);
            if (preview) {
                log_line("damageDecalPreview is active; registered decals will render without damage");
            }
        }
    } catch (...) {
        log_line("Could not register subsystem/hull damage decals");
    }
}

void register_logo_decals(void* object_class, void* parameter_db) noexcept {
    if (!g_register_logo_decal_class || !object_class || !parameter_db) {
        return;
    }
    try {
        const float scale = model_scale(parameter_db);
        std::vector<std::string> logo_names;
        bool logo_names_loaded = false;
        struct PendingLogoDecal {
            LogoDecalDescriptor descriptor;
            std::vector<std::string> paths;
            std::vector<const char*> path_pointers;
        };
        std::vector<PendingLogoDecal> pending;
        pending.reserve(kMaximumLogoDecals);

        for (std::size_t index = 1; index <= kMaximumLogoDecals; ++index) {
            const std::string base =
                "logoDecal" + std::to_string(index);
            std::string hardpoint;
            if (!read_parameter_string(
                    parameter_db, (base + "Hardpoint").c_str(),
                    &hardpoint)) {
                continue;
            }
            void* node = find_class_node(object_class, hardpoint);
            if (!node) {
                log_line("Ignored " + base + ": hardpoint missing " +
                         hardpoint);
                continue;
            }

            PendingLogoDecal entry;
            entry.descriptor.node = node;
            entry.descriptor.size[0] = 6.0f;
            entry.descriptor.size[1] = 6.0f;
            read_float_vector(parameter_db, base + "Offset",
                              entry.descriptor.offset, 3);
            read_float_vector(parameter_db, base + "Rotation",
                              entry.descriptor.rotation_degrees, 3);
            read_float_vector(parameter_db, base + "Size",
                              entry.descriptor.size, 2);
            apply_model_scale(entry.descriptor, scale);
            float colour_key[3]{};
            std::string colour_key_text;
            if (read_parameter_string(
                    parameter_db, (base + "ColourKey").c_str(),
                    &colour_key_text) &&
                parse_float_values(colour_key_text, colour_key, 3)) {
                const auto channel = [](float value) {
                    return static_cast<std::uint32_t>(
                        std::clamp(value, 0.0f, 255.0f) + 0.5f);
                };
                entry.descriptor.use_colour_key = 1;
                entry.descriptor.colour_key =
                    (channel(colour_key[0]) << 16u) |
                    (channel(colour_key[1]) << 8u) |
                    channel(colour_key[2]);
            }
            float flip_u = 0.0f;
            read_parameter_float(
                parameter_db, (base + "FlipU").c_str(), &flip_u);
            entry.descriptor.flip_u = flip_u != 0.0f ? 1u : 0u;

            std::string suffix;
            read_parameter_string(
                parameter_db, (base + "Suffix").c_str(), &suffix);
            if (!logo_names_loaded) {
                logo_names = read_parameter_list(
                    parameter_db, "logoFileNames", true);
                logo_names_loaded = true;
            }
            if (!suffix.empty() && logo_names.empty()) {
                log_line("Ignored " + base +
                         ": its suffix needs logoFileNames rows");
                continue;
            }
            if (!logo_names.empty()) {
                entry.paths.reserve(logo_names.size());
                std::size_t resolved_count = 0;
                for (const std::string& logo_name : logo_names) {
                    std::string resolved;
                    if (!logo_name.empty()) {
                        resolved = resolve_texture(
                            append_texture_suffix(logo_name, suffix));
                    }
                    if (!resolved.empty()) ++resolved_count;
                    entry.paths.push_back(std::move(resolved));
                }
                if (!suffix.empty() && resolved_count == 0) {
                    log_line("Ignored " + base + ": no logoFileNames" +
                             suffix + " textures were found as loose files");
                    continue;
                }
                if (resolved_count == 0) entry.paths.clear();
            }
            pending.push_back(std::move(entry));
        }
        if (pending.empty()) return;

        std::vector<LogoDecalDescriptor> descriptors;
        descriptors.reserve(pending.size());
        for (PendingLogoDecal& entry : pending) {
            if (!entry.paths.empty()) {
                entry.path_pointers.reserve(entry.paths.size());
                for (const std::string& path : entry.paths) {
                    entry.path_pointers.push_back(
                        path.empty() ? nullptr : path.c_str());
                }
                entry.descriptor.texture_paths =
                    entry.path_pointers.data();
                entry.descriptor.texture_path_count =
                    static_cast<std::uint32_t>(
                        entry.path_pointers.size());
            }
            descriptors.push_back(entry.descriptor);
        }
        if (g_register_logo_decal_class(
                object_class, descriptors.data(),
                static_cast<std::uint32_t>(descriptors.size())) != 0) {
            char message[192]{};
            std::snprintf(
                message, sizeof(message),
                "Registered %u selected-name logo decal placement%s",
                static_cast<unsigned>(descriptors.size()),
                descriptors.size() == 1 ? "" : "s");
            log_line(message);
        }
    } catch (...) {
        log_line("Could not register selected-name logo decals");
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
    g_set_emissive_bump_multiplier =
        imported_function<SetEmissiveBumpMultiplierFunction>(
            core, "A2FO_NebulaSetEmissiveBumpMultiplier");
    g_set_bump_light_bias = imported_function<SetBumpLightBiasFunction>(
        core, "A2FO_NebulaSetBumpLightBias");
    g_set_emissive_diffuse_restore =
        imported_function<SetEmissiveDiffuseRestoreFunction>(
            core, "A2FO_NebulaSetEmissiveDiffuseRestore");
    g_register_emissive_class =
        imported_function<RegisterEmissiveClassFunction>(
            core, "A2FO_NebulaRegisterEmissiveClass");
    g_register_emissive_materials =
        imported_function<RegisterEmissiveMaterialsFunction>(
            core, "A2FO_NebulaRegisterEmissiveMaterials");
    g_register_specular_materials =
        imported_function<RegisterSpecularMaterialsFunction>(
            core, "A2FO_NebulaRegisterSpecularMaterials");
    g_register_damage_decal_class =
        imported_function<RegisterDamageDecalClassFunction>(
            core, "A2FO_DecalRegisterClass");
    g_register_logo_decal_class =
        imported_function<RegisterLogoDecalClassFunction>(
            core, "A2FO_LogoDecalRegisterClass");
    if (!g_armada || !status || !g_set_emissive_bump_multiplier ||
        !g_set_bump_light_bias ||
        !g_set_emissive_diffuse_restore ||
        !g_register_emissive_class ||
        !g_register_emissive_materials ||
        !g_register_specular_materials ||
        !g_register_damage_decal_class || !g_register_logo_decal_class) {
        log_line("Updated core renderer exports are unavailable");
        return false;
    }
    cache_extension_roots();
    load_art_texture_suffix_config();
    if (g_set_emissive_bump_multiplier(
            g_art_texture_suffix_config.emissive_bump_multiplier) == 0) {
        log_line("Core rejected the ART emissive bump multiplier");
        return false;
    }
    if (g_set_bump_light_bias(
            g_art_texture_suffix_config.bump_light_bias) == 0) {
        log_line("Core rejected the ART bump light bias");
        return false;
    }
    if (g_set_emissive_diffuse_restore(
            g_art_texture_suffix_config.emissive_diffuse_restore) == 0) {
        log_line("Core rejected the ART emissive diffuse restore amount");
        return false;
    }
    index_loose_texture_names();

    g_texture_file_exists =
        function_from_address<TextureFileExistsFunction>(
            at(g_armada, kTextureFileExistsRva));
    if (signature_matches(
            g_armada, kMeshGetTextureRva, kExpectedMeshGetTexture) &&
        signature_matches(
            g_armada, kMeshSetTextureRva, kExpectedMeshSetTexture) &&
        signature_matches(g_armada, kMeshUpdateRva, kExpectedMeshUpdate) &&
        signature_matches(
            g_armada, kTextureFindRva, kExpectedTextureFind)) {
        g_mesh_get_texture = at(g_armada, kMeshGetTextureRva);
        g_mesh_set_texture = at(g_armada, kMeshSetTextureRva);
        g_mesh_update = at(g_armada, kMeshUpdateRva);
        g_texture_find = function_from_address<TextureFindFunction>(
            at(g_armada, kTextureFindRva));
        g_mesh_texture_runtime_supported = true;
    } else if (!g_art_texture_suffix_config.emissive_suffix.empty() ||
               !g_art_texture_suffix_config.bump_suffix.empty() ||
               !g_art_texture_suffix_config.specular_suffix.empty()) {
        log_line("Global texture suffixes disabled because a checked Storm3D mesh/texture routine differs");
    }

    const int renderer_status = status();
    g_core_renderer_available = renderer_status > 0;
    switch (renderer_status) {
        case 2:
            log_line("Core-owned early DX8 renderer is active");
            break;
        case 1:
            log_line("Core-owned DX8 renderer hooks are armed; waiting for the first DOT3 mesh");
            break;
        case -1:
            log_line("Core-owned DX8 renderer disabled by backend safety "
                     "policy; mapped-material SOD/texture changes are "
                     "inactive");
            break;
        default:
            log_line("Core-owned early DX8 renderer hooks are unavailable");
            break;
    }
    log_line("Mapped-lighting ODF/ART controller initialized");
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FONebulaRenderer_RegisterClass(
    void* object_class, void* parameter_db) {
    if (!g_core_renderer_available ||
        !g_register_emissive_class || !g_register_emissive_materials ||
        !g_register_specular_materials ||
        !g_register_logo_decal_class || !object_class || !parameter_db) {
        return;
    }

    register_damage_decals(object_class, parameter_db);
    register_logo_decals(object_class, parameter_db);

    std::vector<ClassMeshMaterial> class_materials;
    if (g_mesh_texture_runtime_supported) {
        try {
            class_materials = collect_class_mesh_materials(object_class);
        } catch (...) {
            log_line("Could not inspect CraftClass SOD materials; global texture suffixes skipped for that class");
        }
    }
    apply_global_bump_suffix(class_materials);
    register_global_specular(object_class, class_materials);

    const auto resolve_command = [parameter_db](
                                     const char* command,
                                     std::string* resolved,
                                     bool* declared = nullptr) {
        std::string requested;
        if (!read_parameter_string(parameter_db, command, &requested)) {
            if (declared) *declared = false;
            return false;
        }
        if (declared) *declared = true;
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

    const auto resolve_global_emissive = [](
                                             const std::string& diffuse,
                                             std::size_t system_index) {
        if (g_art_texture_suffix_config.emissive_suffix.empty() ||
            system_index >= kGlobalEmissiveSystemTokens.size()) {
            return std::string{};
        }
        const std::string requested = a2fo::nebula::emissive_texture_name(
            texture_name_without_extension(diffuse),
            g_art_texture_suffix_config.emissive_suffix,
            kGlobalEmissiveSystemTokens[system_index]);
        if (g_loose_texture_keys.find(normalized_material_key(requested)) ==
            g_loose_texture_keys.end()) {
            return std::string{};
        }
        return resolve_texture(requested);
    };

    struct IndexedMaterial {
        std::string diffuse_name;
        std::array<std::string, kEmissiveSuffixes.size()> resolved{};
        std::array<bool, kEmissiveSuffixes.size()> explicitly_declared{};
    };
    std::vector<IndexedMaterial> indexed_materials;
    bool odf_indexed_mode = false;
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
        odf_indexed_mode = true;

        IndexedMaterial material{};
        material.diffuse_name = diffuse_name;
        for (std::size_t system_index = 0;
             system_index < kEmissiveSuffixes.size(); ++system_index) {
            char emissive_command[64]{};
            std::snprintf(
                emissive_command, sizeof(emissive_command),
                "emissive%u%s", static_cast<unsigned>(odf_index),
                kEmissiveSuffixes[system_index]);
            resolve_command(
                emissive_command, &material.resolved[system_index],
                &material.explicitly_declared[system_index]);
            if (!material.explicitly_declared[system_index]) {
                material.resolved[system_index] =
                    resolve_global_emissive(diffuse_name, system_index);
            }
        }
        indexed_materials.push_back(std::move(material));
    }

    // Preserve the legacy wildcard contract: when no textureX row exists,
    // any explicit unnumbered command remains authoritative for the class.
    if (!odf_indexed_mode) {
        std::array<std::string, kEmissiveCommands.size()> resolved{};
        std::array<const char*, kEmissiveCommands.size()> paths{};
        bool any_declared = false;
        bool has_policy = false;
        for (std::size_t index = 0;
             index < kEmissiveCommands.size(); ++index) {
            bool declared = false;
            if (resolve_command(
                    kEmissiveCommands[index], &resolved[index],
                    &declared)) {
                paths[index] = resolved[index].c_str();
                has_policy = true;
            }
            any_declared = any_declared || declared;
        }
        if (any_declared) {
            if (!has_policy) return;
            if (g_register_emissive_class(
                    object_class, paths.data(),
                    static_cast<std::uint32_t>(paths.size())) != 0) {
                unsigned texture_count = 0;
                for (const char* path : paths) {
                    if (path && *path) ++texture_count;
                }
                char message[192]{};
                std::snprintf(
                    message, sizeof(message),
                    "Registered %u explicit subsystem emissive texture%s for CraftClass %p",
                    texture_count, texture_count == 1 ? "" : "s",
                    object_class);
                log_line(message);
            }
            return;
        }
    }

    // ART_CFG.h discovery uses the actual diffuse texture stored by every SOD
    // mesh, so single- and multi-material ships need no textureX ODF rows.
    if (!g_art_texture_suffix_config.emissive_suffix.empty()) {
        std::unordered_set<std::string> indexed_keys;
        for (const IndexedMaterial& material : indexed_materials) {
            indexed_keys.insert(normalized_material_key(
                material.diffuse_name));
        }
        for (const ClassMeshMaterial& source : class_materials) {
            const std::string key = normalized_material_key(
                source.diffuse_name);
            if (key.empty() || !indexed_keys.insert(key).second) continue;
            IndexedMaterial material{};
            material.diffuse_name = source.diffuse_name;
            for (std::size_t system_index = 0;
                 system_index < material.resolved.size(); ++system_index) {
                material.resolved[system_index] =
                    resolve_global_emissive(
                        source.diffuse_name, system_index);
            }
            indexed_materials.push_back(std::move(material));
        }
    }

    indexed_materials.erase(
        std::remove_if(
            indexed_materials.begin(), indexed_materials.end(),
            [](const IndexedMaterial& material) {
                return std::none_of(
                    material.resolved.begin(), material.resolved.end(),
                    [](const std::string& path) { return !path.empty(); });
            }),
        indexed_materials.end());

    if (odf_indexed_mode || !indexed_materials.empty()) {
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
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // The core copies every path and owns renderer-lifetime policy/cache data.
    g_art_texture_suffix_config = {};
    g_extension_roots.clear();
    g_resolved_texture_cache.clear();
    g_native_texture_exists_cache.clear();
    g_loose_texture_keys.clear();
    g_mesh_texture_runtime_supported = false;
    g_core_renderer_available = false;
    g_texture_file_exists = nullptr;
    g_texture_find = nullptr;
    g_register_specular_materials = nullptr;
    g_mesh_get_texture = nullptr;
    g_mesh_set_texture = nullptr;
    g_mesh_update = nullptr;
}
