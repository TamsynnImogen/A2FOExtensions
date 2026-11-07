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
#include "../../sdk/include/a2fo_faction_suffix.hpp"
#include "art_texture_suffix_config.hpp"
#include "../../rust/stage8_c_nebula_ffi.h"
#include "../../rust/stage10_nebula_text_ffi.h"
#include "../../rust/stage10_nebula_runtime_ffi.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
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
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kParameterDbGetFloatRva = 0x00134df0;
constexpr std::uintptr_t kParameterDbGetStringVectorRva = 0x00135e80;
constexpr std::uintptr_t kEngineOperatorDeleteRva = 0x002527d0;
constexpr std::uintptr_t kMeshGetTextureRva = 0x00231380;
constexpr std::uintptr_t kMeshSetTextureRva = 0x002313b0;
constexpr std::uintptr_t kMeshUpdateRva = 0x00231c10;
constexpr std::uintptr_t kTextureFileExistsRva = 0x002400d0;
constexpr std::uintptr_t kTextureFindRva = 0x00242870;
constexpr std::uintptr_t kDot3CanRenderDisableBranchRva = 0x00226f63;
constexpr std::uintptr_t kGraphicsEnginePointerRva = 0x003ad508;
constexpr std::size_t kGraphicsOptionsOffset = 0x0104;
// ST3D_Dot3_MeshVB::CanRender rejects the native fast path when this byte is
// nonzero. Preserve the user's persisted bump-off choice separately; the
// checked CanRender guard patch is preferred because this options object does
// not exist yet during deferred module initialization.
constexpr std::size_t kDisableDot3Offset = 0x0005;
constexpr std::size_t kCraftClassGeometryOffset = 0x01d8;
constexpr std::size_t kGeometryRootNodeOffset = 0x003c;
constexpr std::size_t kNodeNameOffset = 0x0008;
constexpr std::size_t kNodeSiblingOffset = 0x001c;
constexpr std::size_t kNodeChildOffset = 0x0020;
constexpr std::size_t kMaximumNodeCount = 4096;
constexpr std::size_t kMeshRenderFlagsOffset = 0x012c;
constexpr std::size_t kMeshRendererOffset = 0x0128;
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
constexpr std::array<const char*, 4> kTextureSubdirectories{
    "", "RGB", "Index8", "Compressed"};

constexpr std::array<std::uint8_t, 7> kExpectedMeshGetTexture{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x8b};
constexpr std::array<std::uint8_t, 7> kExpectedMeshSetTexture{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x10, 0x53};
constexpr std::array<std::uint8_t, 7> kExpectedMeshUpdate{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x53};
constexpr std::array<std::uint8_t, 6> kExpectedTextureFind{
    0x55, 0x8b, 0xec, 0x64, 0xa1, 0x00};
constexpr std::array<std::uint8_t, 2> kExpectedDot3DisableBranch{
    0x75, 0x15};
constexpr std::array<std::uint8_t, 2> kIgnoreDot3DisableBranch{
    0x90, 0x90};

using StatusFunction = int (__cdecl*)();
using SetEmissiveBumpMultiplierFunction = int (__cdecl*)(float);
using SetBumpLightBiasFunction = int (__cdecl*)(float);
using SetEmissiveDiffuseRestoreFunction = int (__cdecl*)(float);
using SetFastNonBumpEnabledFunction = int (__cdecl*)(int);
using RegisterFactionTextureSuffixFunction = int (__cdecl*)(const char*);
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
SetFastNonBumpEnabledFunction g_set_fast_nonbump_enabled = nullptr;
RegisterFactionTextureSuffixFunction g_register_faction_texture_suffix = nullptr;
RegisterDamageDecalClassFunction g_register_damage_decal_class = nullptr;
RegisterLogoDecalClassFunction g_register_logo_decal_class = nullptr;
TextureFileExistsFunction g_texture_file_exists = nullptr;
TextureFindFunction g_texture_find = nullptr;
void* g_mesh_get_texture = nullptr;
void* g_mesh_set_texture = nullptr;
void* g_mesh_update = nullptr;
bool g_mesh_texture_runtime_supported = false;
bool g_core_renderer_available = false;
bool g_neutral_bump_when_disabled_requested = true;
bool g_native_bump_disabled = false;
bool g_neutral_bump_compatibility_active = false;
bool g_fast_nonbump_core_enabled = false;
a2fo::nebula::ArtTextureSuffixConfig g_art_texture_suffix_config;
std::vector<std::string> g_extension_roots;
std::unordered_map<std::string, std::string> g_resolved_texture_cache;
std::unordered_map<std::string, bool> g_native_texture_exists_cache;
std::unordered_set<std::string> g_loose_texture_keys;
std::unordered_map<void*, std::string> g_race_texture_suffixes;

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

void A2FO_CALL race_loaded_handler(
    const A2FO_RaceLoadedEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) || !event->race) {
        return;
    }
    try {
        struct Context {
            const A2FO_RaceLoadedEvent* event;
            std::string raw;
            std::string suffix;
            std::exception_ptr error;
            std::uint8_t failed = 0;
        } context{event, {}, {}, {}, 0};
        const auto step = [](void* opaque, std::uint32_t operation,
                             std::size_t index,
                             A2foStage8NebulaBytes* output) noexcept
            -> std::uintptr_t {
            auto& c = *static_cast<Context*>(opaque);
            if (c.failed) return 0;
            try {
                switch (operation) {
                    case 0: {
                        if (!output) return 0;
                        const auto& field = c.event->odf_fields[index];
                        *output = {
                            reinterpret_cast<const std::uint8_t*>(
                                field.name.data), field.name.size};
                        return 1;
                    }
                    case 1:
                        return _strnicmp(
                            c.event->odf_fields[index].name.data,
                            a2fo::faction_suffix::kCommand,
                            std::strlen(
                                a2fo::faction_suffix::kCommand)) == 0;
                    case 2: {
                        if (!output) return 0;
                        const auto& field = c.event->odf_fields[index];
                        *output = {
                            reinterpret_cast<const std::uint8_t*>(
                                field.value.data), field.value.size};
                        return 1;
                    }
                    case 3: {
                        const auto& field = c.event->odf_fields[index];
                        c.raw.assign(
                            field.value.data ? field.value.data : "",
                            field.value.size);
                        return 1;
                    }
                    case 4: {
                        const bool valid =
                            a2fo::faction_suffix::normalize(
                                c.raw, &c.suffix);
                        if (output) *output = {
                            reinterpret_cast<const std::uint8_t*>(
                                c.suffix.data()), c.suffix.size()};
                        return valid;
                    }
                    case 5:
                        g_race_texture_suffixes[c.event->race] =
                            c.suffix;
                        return 1;
                    case 6:
                        return g_register_faction_texture_suffix != nullptr;
                    case 7:
                        g_register_faction_texture_suffix(
                            c.suffix.c_str());
                        return 1;
                    case 8:
                        g_race_texture_suffixes.erase(c.event->race);
                        return 1;
                    default:
                        return 0;
                }
            } catch (...) {
                c.error = std::current_exception();
                c.failed = 1;
                return 0;
            }
        };
        a2fo_rs_c_nebula_race_loaded(
            event->odf_field_count, event->odf_fields != nullptr,
            step, &context, &context.failed);
        if (context.error) std::rethrow_exception(context.error);
    } catch (...) {
        g_race_texture_suffixes.erase(event->race);
        log_line("Could not retain faction texture suffix for mapped materials");
    }
}

std::vector<std::string> faction_texture_suffixes() {
    // Stock Armada/Fleet Operations Borg alternate textures use `_b` even
    // when borg.odf does not declare factionTextureSuffix.
    std::vector<A2foStage8NebulaBytes> input;
    input.reserve(g_race_texture_suffixes.size());
    for (const auto& entry : g_race_texture_suffixes) {
        input.push_back({reinterpret_cast<const std::uint8_t*>(
                             entry.second.data()), entry.second.size()});
    }
    struct Context {
        std::vector<std::string> suffixes;
        std::exception_ptr error;
        std::uint8_t failed = 0;
    } context;
    const auto emit = [](void* opaque, const std::uint8_t* bytes,
                         std::size_t size) noexcept {
        auto& c = *static_cast<Context*>(opaque);
        if (c.failed) return;
        try {
            c.suffixes.emplace_back(
                reinterpret_cast<const char*>(bytes), size);
        } catch (...) {
            c.error = std::current_exception();
            c.failed = 1;
        }
    };
    a2fo_rs_c_nebula_suffixes(input.data(), input.size(), emit,
                              &context, &context.failed);
    if (context.error) std::rethrow_exception(context.error);
    return context.suffixes;
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
    const auto space = [](void* context, std::uint32_t, std::size_t index)
        noexcept -> std::uintptr_t {
        const auto& text = *static_cast<const std::string*>(context);
        return std::isspace(static_cast<unsigned char>(text[index])) != 0;
    };
    std::size_t first = 0, span = 0;
    a2fo_rs_stage10_nebula_trim(
        reinterpret_cast<std::uint8_t*>(value.data()), value.size(),
        space, &value, &first, &span);
    value = value.substr(first, span);
    a2fo_rs_stage10_nebula_slashes(
        reinterpret_cast<std::uint8_t*>(value.data()), value.size());
    return value;
}

std::string join_path(const std::string& left, const std::string& right) {
    const std::uint8_t branch = a2fo_rs_stage10_nebula_join(
        left.size(), right.size(),
        left.empty() ? 0 : static_cast<std::uint8_t>(left.back()));
    if (branch == 0) return right;
    if (branch == 1) return left;
    std::string result = left;
    if (branch == 3) result.push_back('\\');
    result += right;
    return result;
}

bool regular_file(const std::string& path) noexcept {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string resolve_texture(const std::string& requested) {
    struct Context {
        std::string key;
        std::string result;
        std::exception_ptr error;
        std::uint8_t failed = 0;
    } context;
    const auto step = [](void* opaque, std::uint32_t operation,
                         const std::uint8_t* bytes, std::size_t size,
                         std::uint32_t index,
                         A2foStage8NebulaBytes* output) noexcept -> std::uint32_t {
        auto& c = *static_cast<Context*>(opaque);
        if (c.failed) return 0;
        try {
            switch (operation) {
                case 0:
                    return std::isspace(static_cast<unsigned char>(*bytes)) != 0;
                case 1: {
                    c.key.assign(reinterpret_cast<const char*>(bytes), size);
                    const auto cached = g_resolved_texture_cache.find(c.key);
                    if (cached == g_resolved_texture_cache.end()) return 0;
                    c.result = cached->second;
                    return 1;
                }
                case 2:
                    return static_cast<std::uint32_t>(g_extension_roots.size());
                case 3: {
                    if (!output || index >= g_extension_roots.size()) return 0;
                    const auto& root = g_extension_roots[
                        g_extension_roots.size() - 1 - index];
                    *output = {reinterpret_cast<const std::uint8_t*>(
                        root.data()), root.size()};
                    return 1;
                }
                case 4:
                    return regular_file(std::string(
                        reinterpret_cast<const char*>(bytes), size));
                case 5:
                    c.result.assign(reinterpret_cast<const char*>(bytes), size);
                    g_resolved_texture_cache.emplace(c.key, c.result);
                    return 1;
                case 6:
                    return std::isalpha(static_cast<unsigned char>(*bytes)) != 0;
                case 7:
                    return static_cast<std::uint32_t>(std::tolower(
                        static_cast<unsigned char>(*bytes)));
                default:
                    return 0;
            }
        } catch (...) {
            c.error = std::current_exception();
            c.failed = 1;
            return 0;
        }
    };
    a2fo_rs_c_nebula_resolve_texture(
        &context, reinterpret_cast<const std::uint8_t*>(requested.data()),
        requested.size(), step, &context.failed);
    if (context.error) std::rethrow_exception(context.error);
    return context.result;
}

bool read_parameter_string(void* parameter_db, const char* command,
                           std::string* output) noexcept {
    if (!a2fo_rs_stage10_nebula_query_admit(
            parameter_db, reinterpret_cast<const std::uint8_t*>(command),
            output, g_armada)) return false;
    std::array<char, kMaximumTexturePath> value{};
    const std::uintptr_t found = a2fo_nebula_module_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(command),
        reinterpret_cast<std::uintptr_t>(value.data()),
        static_cast<std::uintptr_t>(value.size()),
        reinterpret_cast<std::uintptr_t>(""));
    value.back() = '\0';
    if (!a2fo_rs_stage10_nebula_query_found(
            found, static_cast<std::uint8_t>(value[0]))) return false;
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
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    if (!a2fo_rs_stage10_nebula_float_valid(found, bits)) return false;
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

void append_copied_byte(std::string& copied, char value) {
    // Release the owned copy during native unwinding, before the callback's
    // catch contains the exception at the Rust boundary.
    struct Rollback {
        std::string& value;
        bool completed = false;
        ~Rollback() noexcept {
            if (!completed) std::string{}.swap(value);
        }
    } rollback{copied};
    copied.push_back(value);
    rollback.completed = true;
}

bool bounded_texture_name(const char* source, std::string* output) noexcept {
    if (!source || !output) return false;
    struct Context {
        const char* source;
        std::string* output;
        std::string copied;
        std::uint8_t failed = 0;
    } context{source, output, {}};
    try {
        context.copied.reserve(128);
        const auto step = [](void* opaque, std::uint32_t op, std::size_t index)
            noexcept -> std::uintptr_t {
            auto& c = *static_cast<Context*>(opaque);
            if (c.failed) return 0;
            try {
                switch (op) {
                    case 0: return readable_range(c.source + index, 1);
                    case 1: return static_cast<unsigned char>(c.source[index]);
                    case 2: append_copied_byte(c.copied, c.source[index]); return 1;
                    case 3:
                        *c.output = trim(std::move(c.copied));
                        return !c.output->empty();
                }
            } catch (...) {
                c.failed = 1;
            }
            return 0;
        };
        return a2fo_rs_stage10_nebula_copy(
            step, &context, kMaximumStormTextureName, &context.failed) != 0;
    } catch (...) {}
    return false;
}

struct ClassMeshMaterial {
    void* mesh = nullptr;
    void* diffuse_texture = nullptr;
    std::string diffuse_name;
};

std::uint8_t lower_byte(std::uint8_t value) noexcept;

std::vector<ClassMeshMaterial> collect_class_mesh_materials(
    void* object_class) {
    std::vector<ClassMeshMaterial> materials;
    if (!object_class || !g_mesh_get_texture) return materials;
    void* geometry = read_at<void*>(
        object_class, kCraftClassGeometryOffset, nullptr);
    void* root = read_at<void*>(geometry, kGeometryRootNodeOffset, nullptr);
    if (!root) return materials;
    struct Context {
        std::vector<ClassMeshMaterial>* materials;
        std::exception_ptr error;
        std::uint8_t failed = 0;
    } context{&materials, {}, 0};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t node_address,
                         std::uintptr_t argument,
                         A2foStage8NebulaWalkOutput* output) noexcept
        -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (c.failed) return 0;
        void* node = reinterpret_cast<void*>(node_address);
        try {
            switch (operation) {
                case 0:
                    if (!output) return 0;
                    output->child = reinterpret_cast<std::uintptr_t>(
                        read_at<void*>(node, kNodeChildOffset, nullptr));
                    output->sibling = reinterpret_cast<std::uintptr_t>(
                        read_at<void*>(node, kNodeSiblingOffset, nullptr));
                    return 1;
                case 1: {
                    void* vtable = read_at<void*>(node, 0, nullptr);
                    void* get_type = read_at<void*>(
                        vtable, sizeof(void*), nullptr);
                    return get_type && readable_range(get_type, 1) &&
                        a2fo_nebula_module_call_thiscall_0(
                            get_type, node) == 1;
                }
                case 2:
                    return a2fo_nebula_module_call_thiscall_1(
                        g_mesh_get_texture, node, 0);
                case 3: {
                    void* diffuse = reinterpret_cast<void*>(argument);
                    const char* native_name = read_at<const char*>(
                        diffuse, kTextureNameOffset, nullptr);
                    std::string diffuse_name;
                    if (bounded_texture_name(
                            native_name, &diffuse_name)) {
                        c.materials->push_back({
                            node, diffuse, std::move(diffuse_name)});
                    }
                    return 1;
                }
                default:
                    return 0;
            }
        } catch (...) {
            c.error = std::current_exception();
            c.failed = 1;
            return 0;
        }
    };
    a2fo_rs_c_nebula_walk(
        reinterpret_cast<std::uintptr_t>(root), {nullptr, 0}, false,
        &lower_byte, step, &context, &context.failed);
    if (context.error) std::rethrow_exception(context.error);
    return materials;
}

bool normalized_node_name(void* node, std::string* output) noexcept {
    if (!node || !output) return false;
    const char* name = read_at<const char*>(node, kNodeNameOffset, nullptr);
    if (!name) return false;
    struct Context {
        const char* name;
        std::string* output;
        std::string copied;
        std::uint8_t failed = 0;
    } context{name, output, {}};
    const auto step = [](void* opaque, std::uint32_t op, std::size_t index)
        noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (c.failed) return 0;
        try {
            switch (op) {
                case 0: return readable_range(c.name + index, 1);
                case 1: return static_cast<unsigned char>(c.name[index]);
                case 2: append_copied_byte(c.copied, c.name[index]); return 1;
                case 3: return 1;
                case 4:
                    c.copied[index] = static_cast<char>(std::tolower(
                        static_cast<unsigned char>(c.copied[index])));
                    return 1;
            }
        } catch (...) {
            c.failed = 1;
        }
        return 0;
    };
    if (!a2fo_rs_stage10_nebula_copy(
            step, &context, 128, &context.failed)) return false;
    a2fo_rs_stage10_nebula_lower(
        step, &context, context.copied.size(), &context.failed);
    if (context.failed) return false;
    try {
        *output = std::move(context.copied);
        return !output->empty();
    } catch (...) {
        std::string{}.swap(context.copied);
        return false;
    }
}

void* find_class_node(void* object_class, const std::string& requested)
    noexcept {
    void* geometry = read_at<void*>(
        object_class, kCraftClassGeometryOffset, nullptr);
    void* root = read_at<void*>(geometry, kGeometryRootNodeOffset, nullptr);
    if (!root) return nullptr;
    struct Context {
        std::string name;
        std::uint8_t failed = 0;
    } context{};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t node_address, std::uintptr_t,
                         A2foStage8NebulaWalkOutput* output) noexcept
        -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        void* node = reinterpret_cast<void*>(node_address);
        switch (operation) {
            case 0:
                if (!output) return 0;
                output->child = reinterpret_cast<std::uintptr_t>(
                    read_at<void*>(node, kNodeChildOffset, nullptr));
                output->sibling = reinterpret_cast<std::uintptr_t>(
                    read_at<void*>(node, kNodeSiblingOffset, nullptr));
                return 1;
            case 4:
                if (!output || !normalized_node_name(node, &c.name))
                    return 0;
                output->name = {
                    reinterpret_cast<const std::uint8_t*>(
                        c.name.data()), c.name.size()};
                return 1;
            default:
                return 0;
        }
    };
    const std::uintptr_t result = a2fo_rs_c_nebula_walk(
        reinterpret_cast<std::uintptr_t>(root),
        {reinterpret_cast<const std::uint8_t*>(requested.data()),
         requested.size()}, true, &lower_byte, step, &context,
        &context.failed);
    return reinterpret_cast<void*>(result);
}

bool parse_float_values(const std::string& text, float* values,
                        std::size_t count) noexcept {
    struct Context {
        const char* text;
        float* values;
        float parsed = 0.0f;
    } context{text.c_str(), values};
    const auto step = [](void* opaque, std::uint32_t op, std::size_t index)
        noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
            case 0:
                return std::isspace(
                    static_cast<unsigned char>(c.text[index])) != 0;
            case 1: {
                char* end = nullptr;
                c.parsed = std::strtof(c.text + index, &end);
                return static_cast<std::size_t>(end - c.text);
            }
            case 2: return std::isfinite(c.parsed);
            case 3: c.values[index] = c.parsed; return 1;
        }
        return 0;
    };
    return a2fo_rs_stage10_nebula_floats(
        reinterpret_cast<const std::uint8_t*>(text.c_str()), values,
        count, step, &context) != 0;
}

void read_float_vector(void* parameter_db, const std::string& command,
                       float* values, std::size_t count) noexcept {
    struct Context {
        void* db;
        const std::string* command;
        float* values;
        std::size_t count;
        std::string text;
    } context{parameter_db, &command, values, count, {}};
    const auto step = [](void* opaque, std::uint32_t op, std::size_t)
        noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (op == 0) return read_parameter_string(
            c.db, c.command->c_str(), &c.text);
        if (op == 1) parse_float_values(c.text, c.values, c.count);
        return 0;
    };
    a2fo_rs_stage10_nebula_vector(step, &context);
}

template <typename Descriptor>
void apply_model_scale(Descriptor& descriptor, float scale) noexcept {
    std::uint32_t word;
    std::memcpy(&word, &scale, sizeof(word));
    // A float copy after the kernel can clear x87 C1 following overflow.
    // Keep the original stores in the descriptor as the final FP operations.
    a2fo_rs_stage10_nebula_scale(descriptor.offset, descriptor.size, word);
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
    using DeleteFunction = void (__cdecl*)(void*);
    struct Context {
        NativeStringVector* native;
        std::vector<std::string>* result;
        std::string copied;
        DeleteFunction engine_delete = nullptr;
        std::exception_ptr fatal_error;
        std::uint8_t copy_failed = 0;
        std::uint8_t fatal = 0;
    } context{&native, &result, {}, nullptr, {}, 0, 0};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::size_t index,
                         A2foStage8NebulaBytes* output) noexcept
        -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (c.fatal) return 0;
        try {
            switch (operation) {
                case 0:
                    return readable_range(
                        c.native->begin, index * sizeof(char*));
                case 1:
                    c.result->reserve(index);
                    return 1;
                case 2: {
                    if (!output) return 0;
                    const char* value = c.native->begin[index];
                    c.copied.clear();
                    if (value && readable_range(value, 1))
                        c.copied = value;
                    *output = {
                        reinterpret_cast<const std::uint8_t*>(
                            c.copied.data()), c.copied.size()};
                    return 1;
                }
                case 3:
                    c.result->push_back(std::move(c.copied));
                    return 1;
                case 4:
                    c.result->clear();
                    return 1;
                case 5:
                    if (c.native->begin[index])
                        c.engine_delete(c.native->begin[index]);
                    return 1;
                case 6:
                    if (c.native->begin)
                        c.engine_delete(c.native->begin);
                    return 1;
                case 7:
                    c.engine_delete = reinterpret_cast<DeleteFunction>(
                        at(g_armada, kEngineOperatorDeleteRva));
                    return c.engine_delete != nullptr;
                default:
                    return 0;
            }
        } catch (...) {
            if (operation == 1 || operation == 2 ||
                operation == 3) {
                c.copy_failed = 1;
            } else {
                c.fatal_error = std::current_exception();
                c.fatal = 1;
            }
            return 0;
        }
    };
    a2fo_rs_c_nebula_parameter_list(
        reinterpret_cast<std::uintptr_t>(native.begin),
        reinterpret_cast<std::uintptr_t>(native.end),
        preserve_empty_rows, step, &context, &context.copy_failed,
        &context.fatal);
    if (context.fatal_error) std::rethrow_exception(context.fatal_error);
    return result;
}

void cache_extension_roots() {
    g_extension_roots.clear();
    struct Context {
        std::exception_ptr error;
        std::uint8_t failed = 0;
    } context{};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::size_t index,
                         A2foStage8NebulaBytes* bytes) noexcept
        -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (c.failed) return 0;
        try {
            switch (operation) {
                case 0:
                    return g_api->extension_root_count();
                case 1: {
                    if (!bytes) return 0;
                    const char* root = g_api->extension_root(
                        static_cast<std::uint32_t>(index));
                    if (!root) return 0;
                    *bytes = {reinterpret_cast<const std::uint8_t*>(root),
                              std::strlen(root)};
                    return 1;
                }
                case 2:
                    if (!bytes) return 0;
                    g_extension_roots.emplace_back(
                        reinterpret_cast<const char*>(bytes->data),
                        bytes->size);
                    return 1;
                default:
                    return 0;
            }
        } catch (...) {
            c.error = std::current_exception();
            c.failed = 1;
            return 0;
        }
    };
    a2fo_rs_c_nebula_cache_roots(
        g_api && g_api->extension_root_count && g_api->extension_root,
        step, &context, &context.failed);
    if (context.error) std::rethrow_exception(context.error);
}

bool read_small_text_config(const std::string& path,
                            std::string* contents) {
    if (!contents) return false;
    contents->clear();
    struct Context {
        const std::string* path;
        std::string* contents;
        std::ifstream input;
        std::exception_ptr error;
        std::uint8_t failed = 0;
    } context{};
    context.path = &path;
    context.contents = contents;
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::int64_t* size) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (c.failed) return 0;
        try {
            switch (operation) {
                case 0:
                    c.input.open(*c.path, std::ios::binary);
                    return static_cast<bool>(c.input);
                case 1:
                    c.input.seekg(0, std::ios::end);
                    if (size) *size = static_cast<std::int64_t>(
                        c.input.tellg());
                    return 1;
                case 2:
                    log_line("Ignored oversized text configuration: " +
                             *c.path);
                    return 1;
                case 3:
                    c.input.seekg(0, std::ios::beg);
                    return 1;
                case 4: {
                    std::ostringstream stream;
                    stream << c.input.rdbuf();
                    *c.contents = stream.str();
                    return 1;
                }
                case 5:
                    return c.input.good() || c.input.eof();
                default:
                    return 0;
            }
        } catch (...) {
            c.error = std::current_exception();
            c.failed = 1;
            return 0;
        }
    };
    const bool result = a2fo_rs_c_nebula_read_text(
        step, &context, &context.failed);
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

void load_art_texture_suffix_config() {
    struct Context {
        std::string path;
        std::string source;
        a2fo::nebula::ArtTextureSuffixParseReport report{};
        std::exception_ptr error;
        std::uint8_t failed = 0;
    } context{};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::size_t index,
                         A2foStage8NebulaArtReport* output) noexcept
        -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (c.failed) return 0;
        try {
            switch (operation) {
                case 0:
                    g_art_texture_suffix_config = {};
                    return 1;
                case 1:
                    return g_extension_roots.size();
                case 2:
                    c.path = join_path(
                        g_extension_roots[index], kArtConfigFileName);
                    return read_small_text_config(c.path, &c.source);
                case 3:
                    c.report =
                        a2fo::nebula::parse_art_texture_suffix_config(
                            c.source, &g_art_texture_suffix_config);
                    if (output) *output = {
                        c.report.valid_assignments,
                        c.report.invalid_assignments};
                    return 1;
                case 4: {
                    char message[512]{};
                    std::snprintf(
                        message, sizeof(message),
                        "Applied %u global texture ART assignment%s from %s",
                        c.report.valid_assignments,
                        c.report.valid_assignments == 1 ? "" : "s",
                        c.path.c_str());
                    log_line(message);
                    return 1;
                }
                case 5: {
                    char message[512]{};
                    std::snprintf(
                        message, sizeof(message),
                        "Ignored %u invalid global texture ART assignment%s in %s",
                        c.report.invalid_assignments,
                        c.report.invalid_assignments == 1 ? "" : "s",
                        c.path.c_str());
                    log_line(message);
                    return 1;
                }
                case 6:
                    return !g_art_texture_suffix_config.emissive_suffix.empty() ||
                        !g_art_texture_suffix_config.bump_suffix.empty() ||
                        !g_art_texture_suffix_config.specular_suffix.empty();
                case 7: {
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
                    return 1;
                }
                default:
                    return 0;
            }
        } catch (...) {
            c.error = std::current_exception();
            c.failed = 1;
            return 0;
        }
    };
    a2fo_rs_c_nebula_load_art(step, &context, &context.failed);
    if (context.error) std::rethrow_exception(context.error);
}

void load_neutral_bump_compatibility_policy() noexcept {
    struct Context {
        std::exception_ptr error;
        std::uint8_t failed = 0;
    } context{};
    const auto step = [](void* opaque, std::uint32_t operation) noexcept
        -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (c.failed) return 0;
        try {
            switch (operation) {
                case 0:
                    g_neutral_bump_when_disabled_requested = true;
                    return 1;
                case 1:
                    return !g_extension_roots.empty();
                case 2: {
                    const std::string ini = join_path(
                        g_extension_roots.front(), "A2FORenderer.ini");
                    g_neutral_bump_when_disabled_requested =
                        GetPrivateProfileIntA(
                            "Compatibility", "NeutralBumpWhenDisabled",
                            1, ini.c_str()) != 0;
                    return 1;
                }
                default:
                    return 0;
            }
        } catch (...) {
            c.error = std::current_exception();
            c.failed = 1;
            return 0;
        }
    };
    a2fo_rs_c_nebula_load_neutral_policy(step, &context, &context.failed);
    if (context.error) std::rethrow_exception(context.error);
}

bool load_native_bump_disabled(bool* disabled) noexcept {
    if (!disabled) return false;
    try {
        struct Context {
            bool* disabled;
            std::array<char, 2048> directory{};
            std::exception_ptr error;
            std::uint8_t failed = 0;
        } context{};
        context.disabled = disabled;
        const auto step = [](void* opaque, std::uint32_t operation) noexcept
            -> std::uintptr_t {
            auto& c = *static_cast<Context*>(opaque);
            if (c.failed) return 0;
            try {
                const auto read_setting = [&c](const std::string& path) {
                    std::string source;
                    if (!read_small_text_config(path, &source)) return false;
                    bool setting_found = false;
                    const bool value =
                        a2fo::nebula::settings_disable_bump(
                            source, &setting_found);
                    if (!setting_found) return false;
                    *c.disabled = value;
                    log_line(
                        "Native bump setting read from " + path + ": " +
                        (value ? "disabled" : "enabled"));
                    return true;
                };
                switch (operation) {
                    case 0:
                        return g_api &&
                            A2FO_MODULE_API_HAS(
                                g_api, get_settings_directory) &&
                            g_api->get_settings_directory;
                    case 1:
                        return g_api->get_settings_directory(
                                   c.directory.data(),
                                   static_cast<std::uint32_t>(
                                       c.directory.size())) &&
                            c.directory[0] != '\0';
                    case 2:
                        return read_setting(join_path(
                            c.directory.data(), "Settings.xml"));
                    case 3:
                        return !g_extension_roots.empty();
                    case 4:
                        return read_setting(join_path(
                            g_extension_roots.front(),
                            "settings_saves_logs\\Settings.xml"));
                    default:
                        return 0;
                }
            } catch (...) {
                c.error = std::current_exception();
                c.failed = 1;
                return 0;
            }
        };
        return a2fo_rs_c_nebula_load_native_bump(
            step, &context, &context.failed);
    } catch (...) {
        return false;
    }
}

std::string texture_name_without_extension(std::string value) {
    value.resize(a2fo_rs_c_nebula_extension_end(
        reinterpret_cast<const std::uint8_t*>(value.data()),
        value.size()));
    return value;
}

std::uint8_t lower_byte(std::uint8_t value) noexcept {
    return static_cast<std::uint8_t>(std::tolower(value));
}

std::string normalized_material_key(std::string value) {
    std::string result(value.size(), '\0');
    const std::size_t size = a2fo_rs_c_nebula_material_key(
        reinterpret_cast<const std::uint8_t*>(value.data()), value.size(),
        &lower_byte, reinterpret_cast<std::uint8_t*>(result.data()));
    result.resize(size);
    return result;
}

struct FactionDiffuseName {
    std::string base_diffuse;
    std::string faction_suffix;
};

FactionDiffuseName split_faction_diffuse_name(
    const std::string& diffuse_name) {
    FactionDiffuseName result{};
    result.base_diffuse = texture_name_without_extension(diffuse_name);
    if (normalized_material_key(result.base_diffuse).empty()) return result;
    std::vector<std::string> suffixes = faction_texture_suffixes();
    std::sort(
        suffixes.begin(), suffixes.end(),
        [](const std::string& left, const std::string& right) {
            return a2fo_rs_stage10_nebula_suffix_before(
                left.size(), right.size()) != 0;
        });
    std::vector<A2foStage8NebulaBytes> spans;
    spans.reserve(suffixes.size());
    for (const std::string& suffix : suffixes) {
        spans.push_back({reinterpret_cast<const std::uint8_t*>(
                             suffix.data()), suffix.size()});
    }
    const A2foStage8NebulaSplit plan = a2fo_rs_c_nebula_split_name(
        reinterpret_cast<const std::uint8_t*>(diffuse_name.data()),
        diffuse_name.size(), spans.data(), spans.size(), &lower_byte);
    result.base_diffuse.assign(diffuse_name.data(), plan.base_size);
    if (plan.suffix_index < suffixes.size()) {
        result.faction_suffix = suffixes[plan.suffix_index];
    }
    return result;
}

void index_loose_texture_names() {
    g_loose_texture_keys.clear();
    try {
        struct Context {
            WIN32_FIND_DATAA entry{};
            HANDLE search = INVALID_HANDLE_VALUE;
            std::exception_ptr error;
            std::uint8_t failed = 0;
        } context{};
        const auto step = [](void* opaque, std::uint32_t operation,
                             std::size_t root, std::size_t directory,
                             const std::uint8_t* bytes, std::size_t size,
                             A2foStage8NebulaBytes* output) noexcept
            -> std::uintptr_t {
            auto& c = *static_cast<Context*>(opaque);
            if (c.failed) return 0;
            try {
                switch (operation) {
                    case 0: {
                        if (root >= g_extension_roots.size() ||
                            directory >= kTextureSubdirectories.size())
                            return 0;
                        std::string folder = join_path(
                            g_extension_roots[root], "Textures");
                        const char* subdirectory =
                            kTextureSubdirectories[directory];
                        if (subdirectory && *subdirectory)
                            folder = join_path(folder, subdirectory);
                        c.entry = {};
                        c.search = FindFirstFileA(
                            join_path(folder, "*").c_str(), &c.entry);
                        return c.search != INVALID_HANDLE_VALUE;
                    }
                    case 1:
                        if (!output) return 0;
                        *output = {
                            reinterpret_cast<const std::uint8_t*>(
                                c.entry.cFileName),
                            std::strlen(c.entry.cFileName)};
                        return (c.entry.dwFileAttributes &
                                FILE_ATTRIBUTE_DIRECTORY) != 0 ? 2u : 1u;
                    case 2:
                        return FindNextFileA(c.search, &c.entry) != 0;
                    case 3:
                        FindClose(c.search);
                        c.search = INVALID_HANDLE_VALUE;
                        return 1;
                    case 4:
                        g_loose_texture_keys.insert(std::string(
                            reinterpret_cast<const char*>(bytes), size));
                        return 1;
                    case 5: {
                        char message[192]{};
                        std::snprintf(
                            message, sizeof(message),
                            "Indexed %u loose global mapped-texture candidate name%s",
                            static_cast<unsigned>(
                                g_loose_texture_keys.size()),
                            g_loose_texture_keys.size() == 1 ? "" : "s");
                        log_line(message);
                        return 1;
                    }
                    default:
                        return 0;
                }
            } catch (...) {
                c.error = std::current_exception();
                c.failed = 1;
                return 0;
            }
        };
        const std::string& emissive =
            g_art_texture_suffix_config.emissive_suffix;
        const std::string& specular =
            g_art_texture_suffix_config.specular_suffix;
        a2fo_rs_c_nebula_index_loose(
            {reinterpret_cast<const std::uint8_t*>(emissive.data()),
             emissive.size()},
            {reinterpret_cast<const std::uint8_t*>(specular.data()),
             specular.size()},
            g_extension_roots.size(), &lower_byte, step, &context,
            &context.failed);
        if (context.error) std::rethrow_exception(context.error);
    } catch (...) {
        g_loose_texture_keys.clear();
        log_line("Could not index loose texture names; global mapped-texture discovery disabled");
    }
}

bool native_texture_exists(const std::string& texture_name,
                           const char* extension) noexcept {
    try {
        struct Context {
            std::exception_ptr error;
            std::uint8_t failed = 0;
        } context{};
        const auto step = [](void* opaque, std::uint32_t operation,
                             const std::uint8_t* bytes, std::size_t size,
                             std::uintptr_t value) noexcept -> std::uintptr_t {
            auto& c = *static_cast<Context*>(opaque);
            if (c.failed) return 0;
            try {
                const std::string path(
                    reinterpret_cast<const char*>(bytes), size);
                switch (operation) {
                    case 0: {
                        const auto cached =
                            g_native_texture_exists_cache.find(path);
                        if (cached == g_native_texture_exists_cache.end())
                            return 0;
                        return cached->second ? 2u : 1u;
                    }
                    case 1:
                        return g_texture_file_exists(path.c_str());
                    case 2:
                        g_native_texture_exists_cache.emplace(
                            path, value != 0);
                        return 1;
                    default:
                        return 0;
                }
            } catch (...) {
                c.error = std::current_exception();
                c.failed = 1;
                return 0;
            }
        };
        return a2fo_rs_c_nebula_native_exists(
            {reinterpret_cast<const std::uint8_t*>(texture_name.data()),
             texture_name.size()},
            {reinterpret_cast<const std::uint8_t*>(extension),
             extension ? std::strlen(extension) : 0},
            g_texture_file_exists != nullptr, &lower_byte, step,
            &context, &context.failed);
    } catch (...) {
        return false;
    }
}

bool arm_neutral_bump_compatibility() noexcept {
    struct Context {
        void* options = nullptr;
        std::uint8_t updated = 0xffu;
        std::exception_ptr error;
        std::uint8_t failed = 0;
    } context{};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t value) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (c.failed) return 0;
        try {
            switch (operation) {
                case 0:
                    log_line("Neutral bump-off compatibility lacks the preflighted fast shader runtime");
                    return 1;
                case 1:
                    return g_api &&
                        A2FO_MODULE_API_HAS(g_api, patch_bytes) &&
                        g_api->patch_bytes &&
                        g_api->patch_bytes(
                            at(g_armada, kDot3CanRenderDisableBranchRva),
                            kIgnoreDot3DisableBranch.data(),
                            kExpectedDot3DisableBranch.data(),
                            kExpectedDot3DisableBranch.size());
                case 2:
                    log_line("Neutral bump-off compatibility bypassed the checked DOT3 disable guard");
                    return 1;
                case 3: {
                    void* graphics = read_at<void*>(
                        at(g_armada, kGraphicsEnginePointerRva), 0, nullptr);
                    c.options = read_at<void*>(
                        graphics, kGraphicsOptionsOffset, nullptr);
                    return read_at<std::uint8_t>(
                        c.options, kDisableDot3Offset, 0xffu);
                }
                case 4:
                    log_line("Neutral bump-off compatibility could not read the runtime DOT3 option");
                    return 1;
                case 5: {
                    const std::uint8_t enabled = 0;
                    return write_at(c.options, kDisableDot3Offset, enabled);
                }
                case 6:
                    log_line("Neutral bump-off compatibility could not enable the runtime DOT3 path");
                    return 1;
                case 7:
                    c.updated = read_at<std::uint8_t>(
                        c.options, kDisableDot3Offset, 0xffu);
                    return c.updated;
                case 8: {
                    char message[128]{};
                    std::snprintf(
                        message, sizeof(message),
                        "Neutral bump-off runtime DOT3 option: %u -> %u",
                        static_cast<unsigned>(value),
                        static_cast<unsigned>(c.updated));
                    log_line(message);
                    return 1;
                }
                default:
                    return 0;
            }
        } catch (...) {
            c.error = std::current_exception();
            c.failed = 1;
            return 0;
        }
    };
    const bool armed = a2fo_rs_c_nebula_arm_neutral(
        g_neutral_bump_when_disabled_requested, g_native_bump_disabled,
        g_core_renderer_available, g_fast_nonbump_core_enabled,
        step, &context, &context.failed);
    if (context.error) std::rethrow_exception(context.error);
    return armed;
}

void* find_native_texture_variant(
    void* diffuse_texture, const std::string& requested) noexcept {
    try {
        struct Context {
            void* diffuse = nullptr;
            void* database = nullptr;
            std::exception_ptr error;
            std::uint8_t failed = 0;
        } context{};
        context.diffuse = diffuse_texture;
        const auto step = [](void* opaque, std::uint32_t operation,
                             const std::uint8_t* bytes, std::size_t size,
                             std::uintptr_t value) noexcept -> std::uintptr_t {
            auto& c = *static_cast<Context*>(opaque);
            if (c.failed) return 0;
            try {
                switch (operation) {
                    case 0:
                        return native_texture_exists(
                            std::string(
                                reinterpret_cast<const char*>(bytes), size),
                            value == 0 ? ".dds" : ".tga");
                    case 1:
                        c.database = read_at<void*>(
                            c.diffuse, kTextureDatabaseOffset, nullptr);
                        return read_at<std::uint32_t>(
                            c.diffuse, kTextureFlagsOffset, 0);
                    case 2:
                        return reinterpret_cast<std::uintptr_t>(
                            g_texture_find(
                                c.database,
                                std::string(reinterpret_cast<const char*>(
                                    bytes), size).c_str(),
                                static_cast<std::uint32_t>(value)));
                    default:
                        return 0;
                }
            } catch (...) {
                c.error = std::current_exception();
                c.failed = 1;
                return 0;
            }
        };
        const std::uintptr_t texture = a2fo_rs_c_nebula_native_variant(
            {reinterpret_cast<const std::uint8_t*>(requested.data()),
             requested.size()},
            diffuse_texture != nullptr && g_texture_find != nullptr,
            step, &context, &context.failed);
        return reinterpret_cast<void*>(texture);
    } catch (...) {
        return nullptr;
    }
}

void apply_global_bump_suffix(
    const std::vector<ClassMeshMaterial>& materials) noexcept {
    const std::string& suffix = g_art_texture_suffix_config.bump_suffix;
    if (suffix.empty() || !g_mesh_texture_runtime_supported) return;
    try {
        struct Context {
            const std::vector<ClassMeshMaterial>* materials;
            void* bump = nullptr;
            std::exception_ptr error;
            std::uint8_t failed = 0;
        } context{&materials, nullptr, {}, 0};
        const auto step = [](void* opaque, std::uint32_t operation,
                             std::size_t index, const std::uint8_t* bytes,
                             std::size_t size, std::uintptr_t value,
                             A2foStage8NebulaBytes* output) noexcept
            -> std::uintptr_t {
            auto& c = *static_cast<Context*>(opaque);
            if (c.failed || index >= c.materials->size()) return 0;
            try {
                const ClassMeshMaterial& material = (*c.materials)[index];
                switch (operation) {
                    case 0:
                        if (!material.mesh || !material.diffuse_texture ||
                            !output) return 0;
                        *output = {reinterpret_cast<const std::uint8_t*>(
                            material.diffuse_name.data()),
                            material.diffuse_name.size()};
                        return 1;
                    case 1:
                        return a2fo_nebula_module_call_thiscall_1(
                            g_mesh_get_texture, material.mesh, 1) != 0;
                    case 2:
                        c.bump = find_native_texture_variant(
                            material.diffuse_texture,
                            std::string(reinterpret_cast<const char*>(bytes),
                                        size));
                        return c.bump != nullptr;
                    case 3:
                        return read_at<std::uint32_t>(
                            material.mesh, kMeshRenderFlagsOffset, 0);
                    case 4:
                        return write_at(material.mesh, kMeshRenderFlagsOffset,
                                        static_cast<std::uint32_t>(value));
                    case 5:
                        return a2fo_nebula_module_call_thiscall_2(
                            g_mesh_set_texture, material.mesh,
                            reinterpret_cast<std::uintptr_t>(c.bump), 1);
                    case 6: {
                        // Update(0) rebuilds the DOT3 renderer from the
                        // selected material without changing source geometry.
                        const std::uintptr_t updated =
                            a2fo_nebula_module_call_thiscall_1(
                                g_mesh_update, material.mesh, 0);
                        const void* renderer = read_at<void*>(
                            material.mesh, kMeshRendererOffset, nullptr);
                        return updated != 0 || !renderer;
                    }
                    case 7:
                        return a2fo_nebula_module_call_thiscall_2(
                            g_mesh_set_texture, material.mesh, 0, 1);
                    case 8:
                        return write_at(material.mesh, kMeshRenderFlagsOffset,
                                        static_cast<std::uint32_t>(value));
                    case 9:
                        return a2fo_nebula_module_call_thiscall_1(
                            g_mesh_update, material.mesh, 0);
                    case 10: {
                        if (!output) return 0;
                        const auto& live =
                            g_art_texture_suffix_config.bump_suffix;
                        *output = {reinterpret_cast<const std::uint8_t*>(
                            live.data()), live.size()};
                        return 1;
                    }
                    default:
                        return 0;
                }
            } catch (...) {
                c.error = std::current_exception();
                c.failed = 1;
                return 0;
            }
        };
        const unsigned applied = a2fo_rs_c_nebula_apply_bump(
            true, g_mesh_texture_runtime_supported,
            materials.size(), step, &context, &context.failed);
        if (context.error) std::rethrow_exception(context.error);
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
        const std::vector<std::string> faction_suffixes =
            faction_texture_suffixes();
        std::vector<std::string> diffuse_names;
        std::vector<std::string> texture_paths;
        std::vector<A2foStage8NebulaBytes> material_spans;
        std::vector<A2foStage8NebulaBytes> faction_spans;
        material_spans.reserve(materials.size());
        faction_spans.reserve(faction_suffixes.size());
        for (const auto& material : materials) {
            material_spans.push_back({
                reinterpret_cast<const std::uint8_t*>(
                    material.diffuse_name.data()), material.diffuse_name.size()});
        }
        for (const auto& faction : faction_suffixes) {
            faction_spans.push_back({
                reinterpret_cast<const std::uint8_t*>(faction.data()),
                faction.size()});
        }
        struct Context {
            std::string base;
            std::string faction;
            std::string resolved;
            std::vector<std::string>* names;
            std::vector<std::string>* paths;
            std::exception_ptr error;
            std::uint8_t failed = 0;
        } context{{}, {}, {}, &diffuse_names, &texture_paths, {}, 0};
        const auto step = [](void* opaque, std::uint32_t operation,
                             const std::uint8_t* bytes, std::size_t size,
                             A2foStage8NebulaBytes* first,
                             A2foStage8NebulaBytes* second) noexcept
            -> std::uint32_t {
            auto& c = *static_cast<Context*>(opaque);
            if (c.failed) return 0;
            try {
                const std::string text(
                    reinterpret_cast<const char*>(bytes), size);
                switch (operation) {
                    case 0:
                        return g_loose_texture_keys.find(text) !=
                            g_loose_texture_keys.end();
                    case 1:
                        c.resolved = resolve_texture(text);
                        if (first) *first = {
                            reinterpret_cast<const std::uint8_t*>(
                                c.resolved.data()), c.resolved.size()};
                        return 1;
                    case 2: {
                        const FactionDiffuseName parsed =
                            split_faction_diffuse_name(text);
                        c.base = parsed.base_diffuse;
                        c.faction = parsed.faction_suffix;
                        if (first) *first = {
                            reinterpret_cast<const std::uint8_t*>(
                                c.base.data()), c.base.size()};
                        if (second) *second = {
                            reinterpret_cast<const std::uint8_t*>(
                                c.faction.data()), c.faction.size()};
                        return 1;
                    }
                    case 3: {
                        if (!first) return 0;
                        const auto& live =
                            g_art_texture_suffix_config.specular_suffix;
                        *first = {reinterpret_cast<const std::uint8_t*>(
                            live.data()), live.size()};
                        return 1;
                    }
                    default:
                        return 0;
                }
            } catch (...) {
                c.error = std::current_exception();
                c.failed = 1;
                return 0;
            }
        };
        const auto emit = [](void* opaque, const std::uint8_t* diffuse,
                             std::size_t diffuse_size,
                             const std::uint8_t* path,
                             std::size_t path_size) noexcept {
            auto& c = *static_cast<Context*>(opaque);
            if (c.failed) return;
            try {
                c.names->emplace_back(
                    reinterpret_cast<const char*>(diffuse), diffuse_size);
                c.paths->emplace_back(
                    reinterpret_cast<const char*>(path), path_size);
            } catch (...) {
                c.error = std::current_exception();
                c.failed = 1;
            }
        };
        a2fo_rs_c_nebula_specular(
            material_spans.data(), material_spans.size(),
            faction_spans.data(), faction_spans.size(),
            true, &lower_byte, step, emit, &context,
            &context.failed);
        if (context.error) std::rethrow_exception(context.error);
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
            char message[256]{};
            std::snprintf(
                message, sizeof(message),
                "Registered %u global/faction specular map binding%s for CraftClass %p",
                static_cast<unsigned>(diffuse_pointers.size()),
                diffuse_pointers.size() == 1 ? "" : "s", object_class);
            log_line(message);
        }
    } catch (...) {
        log_line("Could not register global/faction specular maps for a CraftClass");
    }
}

void register_damage_decals(void* object_class, void* parameter_db) noexcept {
    if (!g_register_damage_decal_class || !object_class || !parameter_db) {
        return;
    }
    try {
        struct PendingDecal {
            DamageDecalDescriptor descriptor;
            std::string texture;
        };
        struct Context {
            void* object_class;
            void* parameter_db;
            float scale = 1.0f;
            float threshold = 0.1f;
            bool preview = false;
            std::string value;
            std::string selected;
            std::string hardpoint;
            void* node = nullptr;
            std::vector<std::string> targets;
            std::vector<PendingDecal> pending;
            std::exception_ptr error;
            std::uint8_t failed = 0;
        } context{};
        context.object_class = object_class;
        context.parameter_db = parameter_db;
        const auto step = [](void* opaque, std::uint32_t operation,
                             std::uint32_t system, std::size_t index,
                             const std::uint8_t* bytes, std::size_t size,
                             A2foStage8NebulaBytes* output,
                             float* number) noexcept -> std::uintptr_t {
            auto& c = *static_cast<Context*>(opaque);
            if (c.failed) return 0;
            try {
                switch (operation) {
                    case 0:
                        if (!number) return 0;
                        *number = 1.0f;
                        read_parameter_float(c.parameter_db, "ScaleSOD", number);
                        return 1;
                    case 1:
                        if (!number) return 0;
                        *number = 0.1f;
                        read_parameter_float(c.parameter_db, "damageThreshold",
                                             number);
                        c.threshold = *number;
                        return 1;
                    case 2:
                        if (!number) return 0;
                        *number = 0.0f;
                        read_parameter_float(c.parameter_db, "damageDecalPreview",
                                             number);
                        return 1;
                    case 3: {
                        c.value.clear();
                        const std::string command(
                            reinterpret_cast<const char*>(bytes), size);
                        if (!read_parameter_string(c.parameter_db,
                                                   command.c_str(),
                                                   &c.value)) return 0;
                        if (output) *output = {
                            reinterpret_cast<const std::uint8_t*>(
                                c.value.data()), c.value.size()};
                        return 1;
                    }
                    case 4:
                        c.selected = resolve_texture(std::string(
                            reinterpret_cast<const char*>(bytes), size));
                        if (output) *output = {
                            reinterpret_cast<const std::uint8_t*>(
                                c.selected.data()), c.selected.size()};
                        return 1;
                    case 5:
                        c.hardpoint.assign(
                            reinterpret_cast<const char*>(bytes), size);
                        c.node = find_class_node(c.object_class, c.hardpoint);
                        return c.node != nullptr;
                    case 6:
                        c.targets = read_parameter_list(
                            c.parameter_db,
                            std::string(reinterpret_cast<const char*>(bytes),
                                        size).c_str());
                        return c.targets.size();
                    case 7:
                        if (!output || index >= c.targets.size()) return 0;
                        *output = {reinterpret_cast<const std::uint8_t*>(
                            c.targets[index].data()), c.targets[index].size()};
                        return 1;
                    case 8:
                    case 9: {
                        PendingDecal entry;
                        entry.descriptor.system_index = system;
                        entry.descriptor.threshold_index =
                            a2fo_rs_stage10_nebula_threshold(
                                c.preview, operation, index);
                        entry.descriptor.node = c.node;
                        entry.descriptor.size[0] = 6.0f;
                        entry.descriptor.size[1] = 6.0f;
                        if (operation == 8) {
                            const std::string base(
                                reinterpret_cast<const char*>(bytes), size);
                            read_float_vector(c.parameter_db, base + "Offset",
                                              entry.descriptor.offset, 3);
                            read_float_vector(c.parameter_db, base + "Rotation",
                                              entry.descriptor.rotation_degrees,
                                              3);
                            read_float_vector(c.parameter_db, base + "Size",
                                              entry.descriptor.size, 2);
                        }
                        apply_model_scale(entry.descriptor, c.scale);
                        entry.texture = c.selected;
                        c.pending.push_back(std::move(entry));
                        return 1;
                    }
                    case 10:
                        c.selected.assign(
                            reinterpret_cast<const char*>(bytes), size);
                        return 1;
                    case 11: {
                        const std::string base(
                            reinterpret_cast<const char*>(bytes), size);
                        char message[256]{};
                        std::snprintf(message, sizeof(message),
                                      "Ignored %s: %s%s", base.c_str(),
                                      c.node ? "texture missing" :
                                      "hardpoint missing",
                                      c.node ? "" : c.hardpoint.c_str());
                        log_line(message);
                        return 1;
                    }
                    case 12: {
                        std::vector<DamageDecalDescriptor> descriptors;
                        descriptors.reserve(c.pending.size());
                        for (PendingDecal& entry : c.pending) {
                            entry.descriptor.texture_path =
                                entry.texture.c_str();
                            descriptors.push_back(entry.descriptor);
                        }
                        if (g_register_damage_decal_class(
                                c.object_class, c.threshold,
                                descriptors.data(),
                                static_cast<std::uint32_t>(
                                    descriptors.size())) != 0) {
                            char message[192]{};
                            std::snprintf(
                                message, sizeof(message),
                                "Registered %u subsystem/hull damage decal%s at %.1f%% intervals",
                                static_cast<unsigned>(descriptors.size()),
                                descriptors.size() == 1 ? "" : "s",
                                static_cast<double>(
                                    c.threshold * 100.0f));
                            log_line(message);
                            if (c.preview) {
                                log_line("damageDecalPreview is active; registered decals will render without damage");
                            }
                        }
                        return 1;
                    }
                    case 14:
                        c.preview = system != 0;
                        c.scale = number ? *number : 1.0f;
                        c.pending.reserve(6 * kMaximumDamageDecalsPerSystem);
                        return 1;
                    default:
                        return 0;
                }
            } catch (...) {
                c.error = std::current_exception();
                c.failed = 1;
                return 0;
            }
        };
        a2fo_rs_c_nebula_damage(step, &context, &context.failed);
        if (context.error) std::rethrow_exception(context.error);
    } catch (...) {
        log_line("Could not register subsystem/hull damage decals");
    }
}

void register_logo_decals(void* object_class, void* parameter_db) noexcept {
    if (!g_register_logo_decal_class || !object_class || !parameter_db) {
        return;
    }
    try {
        struct PendingLogoDecal {
            LogoDecalDescriptor descriptor;
            std::vector<std::string> paths;
            std::vector<const char*> path_pointers;
        };
        struct Context {
            void* object_class;
            void* parameter_db;
            float scale = 1.0f;
            void* node = nullptr;
            std::string hardpoint;
            std::string value;
            std::string suffix;
            std::string selected;
            std::vector<std::string> logo_names;
            PendingLogoDecal current;
            std::vector<PendingLogoDecal> pending;
            std::exception_ptr error;
            std::uint8_t failed = 0;
        } context{};
        context.object_class = object_class;
        context.parameter_db = parameter_db;
        context.pending.reserve(kMaximumLogoDecals);
        const auto step = [](void* opaque, std::uint32_t operation,
                             std::uint32_t system, std::size_t index,
                             const std::uint8_t* bytes, std::size_t size,
                             A2foStage8NebulaBytes* output,
                             float* number) noexcept -> std::uintptr_t {
            auto& c = *static_cast<Context*>(opaque);
            if (c.failed) return 0;
            try {
                switch (operation) {
                    case 0:
                        if (!number) return 0;
                        *number = 1.0f;
                        read_parameter_float(c.parameter_db, "ScaleSOD", number);
                        return 1;
                    case 1:
                        c.scale = number ? *number : 1.0f;
                        return 1;
                    case 2:
                    case 8: {
                        c.value.clear();
                        const std::string command(
                            reinterpret_cast<const char*>(bytes), size);
                        const bool found = read_parameter_string(
                            c.parameter_db, command.c_str(), &c.value);
                        if (operation == 8) c.suffix = c.value;
                        if (!found) return 0;
                        if (output) *output = {
                            reinterpret_cast<const std::uint8_t*>(
                                c.value.data()), c.value.size()};
                        return 1;
                    }
                    case 3:
                        c.hardpoint.assign(
                            reinterpret_cast<const char*>(bytes), size);
                        c.node = find_class_node(c.object_class, c.hardpoint);
                        return c.node != nullptr;
                    case 4: {
                        const std::string base(
                            reinterpret_cast<const char*>(bytes), size);
                        log_line("Ignored " + base + ": hardpoint missing " +
                                 c.hardpoint);
                        return 1;
                    }
                    case 5: {
                        const std::string base(
                            reinterpret_cast<const char*>(bytes), size);
                        c.current = {};
                        c.current.descriptor.node = c.node;
                        c.current.descriptor.size[0] = 6.0f;
                        c.current.descriptor.size[1] = 6.0f;
                        read_float_vector(c.parameter_db, base + "Offset",
                                          c.current.descriptor.offset, 3);
                        read_float_vector(c.parameter_db, base + "Rotation",
                                          c.current.descriptor.rotation_degrees,
                                          3);
                        read_float_vector(c.parameter_db, base + "Size",
                                          c.current.descriptor.size, 2);
                        apply_model_scale(c.current.descriptor, c.scale);
                        return 1;
                    }
                    case 6: {
                        const std::string base(
                            reinterpret_cast<const char*>(bytes), size);
                        float colour_key[3]{};
                        std::string colour_key_text;
                        if (read_parameter_string(
                                c.parameter_db,
                                (base + "ColourKey").c_str(),
                                &colour_key_text) &&
                            parse_float_values(colour_key_text,
                                               colour_key, 3)) {
                            std::uint32_t words[3];
                            std::memcpy(words, colour_key, sizeof(words));
                            c.current.descriptor.use_colour_key = 1;
                            c.current.descriptor.colour_key =
                                a2fo_rs_stage10_nebula_colour_key(words);
                        }
                        return 1;
                    }
                    case 7: {
                        if (!number) return 0;
                        const std::string base(
                            reinterpret_cast<const char*>(bytes), size);
                        *number = 0.0f;
                        read_parameter_float(
                            c.parameter_db, (base + "FlipU").c_str(), number);
                        return 1;
                    }
                    case 9:
                        c.logo_names = read_parameter_list(
                            c.parameter_db, "logoFileNames", true);
                        return c.logo_names.size();
                    case 10:
                        if (!output || index >= c.logo_names.size()) return 0;
                        *output = {reinterpret_cast<const std::uint8_t*>(
                            c.logo_names[index].data()),
                            c.logo_names[index].size()};
                        return 1;
                    case 11:
                        c.selected = resolve_texture(std::string(
                            reinterpret_cast<const char*>(bytes), size));
                        if (output) *output = {
                            reinterpret_cast<const std::uint8_t*>(
                                c.selected.data()), c.selected.size()};
                        return 1;
                    case 12:
                        c.current.paths.emplace_back(
                            reinterpret_cast<const char*>(bytes), size);
                        return 1;
                    case 13:
                        c.pending.push_back(std::move(c.current));
                        return 1;
                    case 14: {
                        std::vector<LogoDecalDescriptor> descriptors;
                        descriptors.reserve(c.pending.size());
                        for (PendingLogoDecal& entry : c.pending) {
                            if (!entry.paths.empty()) {
                                entry.path_pointers.reserve(
                                    entry.paths.size());
                                for (const std::string& path : entry.paths) {
                                    entry.path_pointers.push_back(
                                        path.empty() ? nullptr :
                                        path.c_str());
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
                                c.object_class, descriptors.data(),
                                static_cast<std::uint32_t>(
                                    descriptors.size())) != 0) {
                            char message[192]{};
                            std::snprintf(
                                message, sizeof(message),
                                "Registered %u selected-name logo decal placement%s",
                                static_cast<unsigned>(descriptors.size()),
                                descriptors.size() == 1 ? "" : "s");
                            log_line(message);
                        }
                        return 1;
                    }
                    case 15: {
                        const std::string base(
                            reinterpret_cast<const char*>(bytes), size);
                        log_line("Ignored " + base +
                                 ": its suffix needs logoFileNames rows");
                        return 1;
                    }
                    case 16:
                        c.current.descriptor.flip_u = system != 0 ? 1u : 0u;
                        return 1;
                    case 17: {
                        const std::string base(
                            reinterpret_cast<const char*>(bytes), size);
                        log_line("Ignored " + base + ": no logoFileNames" +
                                 c.suffix +
                                 " textures were found as loose files");
                        return 1;
                    }
                    case 18:
                        c.current.paths.clear();
                        return 1;
                    default:
                        return 0;
                }
            } catch (...) {
                c.error = std::current_exception();
                c.failed = 1;
                return 0;
            }
        };
        a2fo_rs_c_nebula_logos(step, &context, &context.failed);
        if (context.error) std::rethrow_exception(context.error);
    } catch (...) {
        log_line("Could not register selected-name logo decals");
    }
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    struct Context {
        const A2FO_ModuleApi* api = nullptr;
        StatusFunction status = nullptr;
        std::exception_ptr error;
        std::uint8_t failed = 0;
    } context{};
    context.api = api;
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::intptr_t value) noexcept -> std::intptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (c.failed) return 0;
        try {
            switch (operation) {
                case 0: {
                    g_api = c.api;
                    g_armada = static_cast<HMODULE>(
                        c.api->armada_module());
                    HMODULE core = GetModuleHandleA(
                        "A2FOExtensions.dll");
                    c.status = imported_function<StatusFunction>(
                        core, "A2FO_NebulaRendererStatus");
                    g_set_emissive_bump_multiplier =
                        imported_function<
                            SetEmissiveBumpMultiplierFunction>(
                            core,
                            "A2FO_NebulaSetEmissiveBumpMultiplier");
                    g_set_bump_light_bias =
                        imported_function<SetBumpLightBiasFunction>(
                            core, "A2FO_NebulaSetBumpLightBias");
                    g_set_emissive_diffuse_restore =
                        imported_function<
                            SetEmissiveDiffuseRestoreFunction>(
                            core,
                            "A2FO_NebulaSetEmissiveDiffuseRestore");
                    g_set_fast_nonbump_enabled =
                        imported_function<
                            SetFastNonBumpEnabledFunction>(
                            core, "A2FO_NebulaSetFastNonBumpEnabled");
                    g_register_faction_texture_suffix =
                        imported_function<
                            RegisterFactionTextureSuffixFunction>(
                            core,
                            "A2FO_NebulaRegisterFactionTextureSuffix");
                    g_register_emissive_class =
                        imported_function<
                            RegisterEmissiveClassFunction>(
                            core, "A2FO_NebulaRegisterEmissiveClass");
                    g_register_emissive_materials =
                        imported_function<
                            RegisterEmissiveMaterialsFunction>(
                            core,
                            "A2FO_NebulaRegisterEmissiveMaterials");
                    g_register_specular_materials =
                        imported_function<
                            RegisterSpecularMaterialsFunction>(
                            core,
                            "A2FO_NebulaRegisterSpecularMaterials");
                    g_register_damage_decal_class =
                        imported_function<
                            RegisterDamageDecalClassFunction>(
                            core, "A2FO_DecalRegisterClass");
                    g_register_logo_decal_class =
                        imported_function<
                            RegisterLogoDecalClassFunction>(
                            core, "A2FO_LogoDecalRegisterClass");
                    return 1;
                }
                case 1:
                    return g_armada && c.status &&
                        g_set_emissive_bump_multiplier &&
                        g_set_bump_light_bias &&
                        g_set_emissive_diffuse_restore &&
                        g_set_fast_nonbump_enabled &&
                        g_register_emissive_class &&
                        g_register_emissive_materials &&
                        g_register_specular_materials &&
                        g_register_damage_decal_class &&
                        g_register_logo_decal_class;
                case 2:
                    cache_extension_roots();
                    return 1;
                case 3:
                    load_neutral_bump_compatibility_policy();
                    return 1;
                case 4:
                    return load_native_bump_disabled(
                        &g_native_bump_disabled);
                case 5:
                    load_art_texture_suffix_config();
                    return 1;
                case 6:
                    return g_set_emissive_bump_multiplier(
                        g_art_texture_suffix_config.
                            emissive_bump_multiplier);
                case 7:
                    return g_set_bump_light_bias(
                        g_art_texture_suffix_config.bump_light_bias);
                case 8:
                    return g_set_emissive_diffuse_restore(
                        g_art_texture_suffix_config.
                            emissive_diffuse_restore);
                case 9:
                    index_loose_texture_names();
                    return 1;
                case 10:
                    if (g_register_faction_texture_suffix)
                        g_register_faction_texture_suffix("_b");
                    else
                        log_line("Core faction-suffix mapped-lighting fallback export unavailable; exact material bindings only");
                    return 1;
                case 11:
                    return A2FO_MODULE_API_HAS(
                            c.api, register_race_loaded_handler) &&
                        (c.api->capabilities & A2FO_CAP_RACE_LOADED) != 0 &&
                        c.api->register_race_loaded_handler;
                case 12: {
                    const char* race_fields[] = {
                        a2fo::faction_suffix::kCommand};
                    return c.api->register_race_loaded_handler(
                        kModuleName, race_fields,
                        static_cast<std::uint32_t>(
                            std::size(race_fields)),
                        &race_loaded_handler, nullptr);
                }
                case 13:
                    if (value == 1)
                        log_line("Faction texture suffixes linked to mapped emissive/specular materials");
                    else if (value == 2)
                        log_line("Could not subscribe to factionTextureSuffix; stock Borg _b mapped-material compatibility remains available");
                    else
                        log_line("Race-loaded API unavailable; stock Borg _b mapped-material compatibility only");
                    return 1;
                case 14:
                    g_texture_file_exists =
                        function_from_address<
                            TextureFileExistsFunction>(
                            at(g_armada, kTextureFileExistsRva));
                    return 1;
                case 15:
                    if (signature_matches(
                            g_armada, kMeshGetTextureRva,
                            kExpectedMeshGetTexture) &&
                        signature_matches(
                            g_armada, kMeshSetTextureRva,
                            kExpectedMeshSetTexture) &&
                        signature_matches(
                            g_armada, kMeshUpdateRva,
                            kExpectedMeshUpdate) &&
                        signature_matches(
                            g_armada, kTextureFindRva,
                            kExpectedTextureFind)) {
                        g_mesh_get_texture = at(
                            g_armada, kMeshGetTextureRva);
                        g_mesh_set_texture = at(
                            g_armada, kMeshSetTextureRva);
                        g_mesh_update = at(
                            g_armada, kMeshUpdateRva);
                        g_texture_find =
                            function_from_address<TextureFindFunction>(
                                at(g_armada, kTextureFindRva));
                        g_mesh_texture_runtime_supported = true;
                        return 1;
                    }
                    return 0;
                case 16:
                    return !g_art_texture_suffix_config.
                               emissive_suffix.empty() ||
                        !g_art_texture_suffix_config.
                             bump_suffix.empty() ||
                        !g_art_texture_suffix_config.
                             specular_suffix.empty();
                case 17:
                    log_line("Global texture suffixes disabled because a checked Storm3D mesh/texture routine differs");
                    return 1;
                case 18:
                    return c.status();
                case 19:
                    g_core_renderer_available = value > 0;
                    return 1;
                case 20:
                    if (value == 2)
                        log_line("Core-owned early DX8 renderer is active");
                    else if (value == 1)
                        log_line("Core-owned DX8 renderer hooks are armed; waiting for the first DOT3 mesh");
                    else if (value == -1)
                        log_line("Core-owned DX8 renderer disabled by backend safety policy; mapped-material SOD/texture changes are inactive");
                    else
                        log_line("Core-owned early DX8 renderer hooks are unavailable");
                    return 1;
                case 21:
                    return (g_neutral_bump_when_disabled_requested ? 1 : 0) |
                        (g_native_bump_disabled ? 2 : 0);
                case 22:
                    g_fast_nonbump_core_enabled =
                        g_set_fast_nonbump_enabled(value != 0 ? 1 : 0) !=
                            0 && value != 0;
                    return g_fast_nonbump_core_enabled;
                case 23:
                    log_line("Core rejected the fast non-bump shader route; native bump-off rendering retained");
                    return 1;
                case 24:
                    g_neutral_bump_compatibility_active =
                        value != 0 && arm_neutral_bump_compatibility();
                    return g_neutral_bump_compatibility_active;
                case 25:
                    g_set_fast_nonbump_enabled(0);
                    g_fast_nonbump_core_enabled = false;
                    return 1;
                case 26:
                    switch (value) {
                        case 0:
                            log_line("Neutral bump-off compatibility disabled by NeutralBumpWhenDisabled=0");
                            break;
                        case 1:
                            log_line("Neutral bump-off compatibility could not read disable_bump from Settings.xml");
                            break;
                        case 2:
                            log_line("Native bump mapping is enabled; authored bump maps retained");
                            break;
                        case 3:
                            log_line("Native bump-off compatibility active: self-contained fast flat-normal shader selected");
                            break;
                        default:
                            log_line("Neutral bump-off compatibility could not arm; native bump-off rendering retained");
                            break;
                    }
                    return 1;
                case 27:
                    log_line("Mapped-lighting ODF/ART controller initialized");
                    return 1;
                case 28:
                    return g_fast_nonbump_core_enabled ? 1 : 0;
                case 29:
                    return (g_neutral_bump_when_disabled_requested ?
                                1 : 0) |
                        (g_native_bump_disabled ? 2 : 0) |
                        (g_neutral_bump_compatibility_active ? 4 : 0);
                case 30:
                    log_line("Updated core renderer exports are unavailable");
                    return 1;
                case 31:
                    log_line("Core rejected the ART emissive bump multiplier");
                    return 1;
                case 32:
                    log_line("Core rejected the ART bump light bias");
                    return 1;
                case 33:
                    log_line("Core rejected the ART emissive diffuse restore amount");
                    return 1;
                default:
                    return 0;
            }
        } catch (...) {
            c.error = std::current_exception();
            c.failed = 1;
            return 0;
        }
    };
    const bool valid_api =
        api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
        api->api_version == A2FO_MODULE_API_VERSION && api->log &&
        api->armada_module && api->extension_root_count &&
        api->extension_root;
    const bool result = a2fo_rs_c_nebula_init(
        valid_api, step, &context, &context.failed);
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FONebulaRenderer_RegisterClass(
    void* object_class, void* parameter_db) {
    struct Context {
        void* object_class = nullptr;
        void* parameter_db = nullptr;
        std::vector<ClassMeshMaterial> materials;
        std::vector<std::string> factions;
        std::string command;
        std::string requested;
        std::string resolved;
        std::string base;
        std::string faction;
        std::exception_ptr error;
        std::uint8_t failed = 0;
    } context{};
    context.object_class = object_class;
    context.parameter_db = parameter_db;
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::size_t index, std::size_t,
                         const std::uint8_t* bytes, std::size_t size,
                         A2foStage8NebulaBytes* first,
                         A2foStage8NebulaBytes* second) noexcept
        -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (c.failed) return 0;
        try {
            const auto text = [&]() {
                return std::string(
                    reinterpret_cast<const char*>(bytes), size);
            };
            switch (operation) {
                case 0:
                    register_damage_decals(
                        c.object_class, c.parameter_db);
                    return 1;
                case 1:
                    register_logo_decals(c.object_class, c.parameter_db);
                    return 1;
                case 2:
                    if (g_mesh_texture_runtime_supported) {
                        try {
                            c.materials = collect_class_mesh_materials(
                                c.object_class);
                        } catch (...) {
                            log_line("Could not inspect CraftClass SOD materials; global texture suffixes skipped for that class");
                        }
                    }
                    return 1;
                case 3:
                    if (c.materials.empty()) {
                        log_line("Late-loaded CraftClass refresh found no SOD materials yet");
                    } else {
                        char message[192]{};
                        std::snprintf(
                            message, sizeof(message),
                            "Late-loaded CraftClass refresh found %u SOD material%s",
                            static_cast<unsigned>(c.materials.size()),
                            c.materials.size() == 1 ? "" : "s");
                        log_line(message);
                    }
                    return 1;
                case 4:
                    apply_global_bump_suffix(c.materials);
                    return 1;
                case 5:
                    register_global_specular(
                        c.object_class, c.materials);
                    return 1;
                case 6:
                    c.command = text();
                    c.requested.clear();
                    if (!read_parameter_string(
                            c.parameter_db, c.command.c_str(),
                            &c.requested)) return 0;
                    if (first) *first = {
                        reinterpret_cast<const std::uint8_t*>(
                            c.requested.data()), c.requested.size()};
                    return 1;
                case 7:
                    c.resolved = resolve_texture(c.requested);
                    if (c.resolved.empty()) {
                        char message[1400]{};
                        std::snprintf(
                            message, sizeof(message),
                            "%s texture '%s' was not found as a loose file",
                            c.command.c_str(), c.requested.c_str());
                        log_line(message);
                    }
                    if (first) *first = {
                        reinterpret_cast<const std::uint8_t*>(
                            c.resolved.data()), c.resolved.size()};
                    return 1;
                case 8:
                    return g_loose_texture_keys.find(text()) !=
                        g_loose_texture_keys.end();
                case 9:
                    c.resolved = resolve_texture(text());
                    if (first) *first = {
                        reinterpret_cast<const std::uint8_t*>(
                            c.resolved.data()), c.resolved.size()};
                    return 1;
                case 10: {
                    if (!first) return 0;
                    const auto& suffix =
                        g_art_texture_suffix_config.emissive_suffix;
                    *first = {reinterpret_cast<const std::uint8_t*>(
                        suffix.data()), suffix.size()};
                    return 1;
                }
                case 11: {
                    const FactionDiffuseName parsed =
                        split_faction_diffuse_name(text());
                    c.base = parsed.base_diffuse;
                    c.faction = parsed.faction_suffix;
                    if (first) *first = {
                        reinterpret_cast<const std::uint8_t*>(
                            c.base.data()), c.base.size()};
                    if (second) *second = {
                        reinterpret_cast<const std::uint8_t*>(
                            c.faction.data()), c.faction.size()};
                    return 1;
                }
                case 12:
                    return c.materials.size();
                case 13:
                    if (!first || index >= c.materials.size()) return 0;
                    *first = {reinterpret_cast<const std::uint8_t*>(
                        c.materials[index].diffuse_name.data()),
                        c.materials[index].diffuse_name.size()};
                    return 1;
                case 14:
                    c.factions = faction_texture_suffixes();
                    return c.factions.size();
                case 15:
                    if (!first || index >= c.factions.size()) return 0;
                    *first = {reinterpret_cast<const std::uint8_t*>(
                        c.factions[index].data()),
                        c.factions[index].size()};
                    return 1;
                default:
                    return 0;
            }
        } catch (...) {
            c.error = std::current_exception();
            c.failed = 1;
            return 0;
        }
    };
    const auto registration = [](
            void* opaque, std::uint32_t operation,
            const A2foStage8NebulaClassRow* rows,
            std::size_t count, std::size_t texture_count) noexcept
        -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (c.failed) return 0;
        try {
            if (operation == 0 && count == 1) {
                std::array<std::string, 6> resolved{};
                std::array<const char*, 6> paths{};
                for (std::size_t index = 0; index < 6; ++index) {
                    const auto& span = rows[0].paths[index];
                    resolved[index].assign(
                        reinterpret_cast<const char*>(span.data),
                        span.size);
                    if (!resolved[index].empty())
                        paths[index] = resolved[index].c_str();
                }
                if (g_register_emissive_class(
                        c.object_class, paths.data(),
                        static_cast<std::uint32_t>(
                            paths.size())) != 0) {
                    char message[192]{};
                    std::snprintf(
                        message, sizeof(message),
                        "Registered %u explicit subsystem emissive texture%s for CraftClass %p",
                        static_cast<unsigned>(texture_count),
                        texture_count == 1 ? "" : "s",
                        c.object_class);
                    log_line(message);
                }
                return 1;
            }
            if (operation == 1) {
                std::vector<std::string> diffuse_names;
                std::vector<std::array<std::string, 6>> path_storage;
                diffuse_names.reserve(count);
                path_storage.reserve(count);
                for (std::size_t index = 0; index < count; ++index) {
                    diffuse_names.emplace_back(
                        reinterpret_cast<const char*>(
                            rows[index].diffuse.data),
                        rows[index].diffuse.size);
                    std::array<std::string, 6> paths{};
                    for (std::size_t system = 0; system < 6; ++system) {
                        const auto& span = rows[index].paths[system];
                        paths[system].assign(
                            reinterpret_cast<const char*>(span.data),
                            span.size);
                    }
                    path_storage.push_back(std::move(paths));
                }
                std::vector<const char*> names;
                std::vector<const char*> paths;
                names.reserve(count);
                paths.reserve(count * 6);
                for (std::size_t index = 0; index < count; ++index) {
                    names.push_back(diffuse_names[index].c_str());
                    for (const auto& path : path_storage[index]) {
                        paths.push_back(
                            path.empty() ? nullptr : path.c_str());
                    }
                }
                if (g_register_emissive_materials(
                        c.object_class, names.data(), paths.data(),
                        static_cast<std::uint32_t>(count), 6) != 0) {
                    char message[224]{};
                    std::snprintf(
                        message, sizeof(message),
                        "Registered %u subsystem emissive texture%s across %u "
                        "indexed diffuse material%s for CraftClass %p",
                        static_cast<unsigned>(texture_count),
                        texture_count == 1 ? "" : "s",
                        static_cast<unsigned>(count),
                        count == 1 ? "" : "s", c.object_class);
                    log_line(message);
                }
                return 1;
            }
            return 0;
        } catch (...) {
            c.error = std::current_exception();
            c.failed = 1;
            return 0;
        }
    };
    const bool ready =
        g_core_renderer_available && g_register_emissive_class &&
        g_register_emissive_materials && g_register_specular_materials &&
        g_register_logo_decal_class && object_class;
    a2fo_rs_c_nebula_register_class(
        ready, parameter_db != nullptr, &lower_byte, step,
        registration, &context, &context.failed);
    if (context.error) std::rethrow_exception(context.error);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // The core copies every path and owns renderer-lifetime policy/cache data.
    struct Context {
        std::exception_ptr error;
        std::uint8_t failed = 0;
    } context{};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::intptr_t) noexcept -> std::intptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (c.failed) return 0;
        try {
            switch (operation) {
                case 40:
                    if (g_set_fast_nonbump_enabled)
                        g_set_fast_nonbump_enabled(0);
                    break;
                case 41:
                    g_art_texture_suffix_config = {};
                    break;
                case 42:
                    g_extension_roots.clear();
                    break;
                case 43:
                    g_resolved_texture_cache.clear();
                    break;
                case 44:
                    g_native_texture_exists_cache.clear();
                    break;
                case 45:
                    g_loose_texture_keys.clear();
                    break;
                case 46:
                    g_race_texture_suffixes.clear();
                    break;
                case 47:
                    g_mesh_texture_runtime_supported = false;
                    break;
                case 48:
                    g_core_renderer_available = false;
                    break;
                case 49:
                    g_fast_nonbump_core_enabled = false;
                    break;
                case 50:
                    g_texture_file_exists = nullptr;
                    break;
                case 51:
                    g_texture_find = nullptr;
                    break;
                case 52:
                    g_register_specular_materials = nullptr;
                    break;
                case 53:
                    g_mesh_get_texture = nullptr;
                    break;
                case 54:
                    g_mesh_set_texture = nullptr;
                    break;
                case 55:
                    g_mesh_update = nullptr;
                    break;
                case 56:
                    g_set_fast_nonbump_enabled = nullptr;
                    break;
                case 57:
                    g_register_faction_texture_suffix = nullptr;
                    break;
                default:
                    return 0;
            }
            return 1;
        } catch (...) {
            c.error = std::current_exception();
            c.failed = 1;
            return 0;
        }
    };
    a2fo_rs_c_nebula_shutdown(step, &context, &context.failed);
    if (context.error) std::rethrow_exception(context.error);
}
