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

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <vector>

extern "C" {
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
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kParameterDbGetFloatRva = 0x00134df0;
constexpr std::uintptr_t kParameterDbGetStringVectorRva = 0x00135e80;
constexpr std::uintptr_t kEngineOperatorDeleteRva = 0x002527d0;
constexpr std::size_t kCraftClassGeometryOffset = 0x01d8;
constexpr std::size_t kGeometryRootNodeOffset = 0x003c;
constexpr std::size_t kNodeNameOffset = 0x0008;
constexpr std::size_t kNodeSiblingOffset = 0x001c;
constexpr std::size_t kNodeChildOffset = 0x0020;
constexpr std::size_t kMaximumNodeCount = 4096;
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
RegisterDamageDecalClassFunction g_register_damage_decal_class = nullptr;
RegisterLogoDecalClassFunction g_register_logo_decal_class = nullptr;
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
    g_register_emissive_class =
        imported_function<RegisterEmissiveClassFunction>(
            core, "A2FO_NebulaRegisterEmissiveClass");
    g_register_emissive_materials =
        imported_function<RegisterEmissiveMaterialsFunction>(
            core, "A2FO_NebulaRegisterEmissiveMaterials");
    g_register_damage_decal_class =
        imported_function<RegisterDamageDecalClassFunction>(
            core, "A2FO_DecalRegisterClass");
    g_register_logo_decal_class =
        imported_function<RegisterLogoDecalClassFunction>(
            core, "A2FO_LogoDecalRegisterClass");
    if (!g_armada || !status || !g_register_emissive_class ||
        !g_register_emissive_materials ||
        !g_register_damage_decal_class || !g_register_logo_decal_class) {
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
        !g_register_logo_decal_class || !object_class || !parameter_db) {
        return;
    }

    register_damage_decals(object_class, parameter_db);
    register_logo_decals(object_class, parameter_db);

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
