/*
 * Ownership-aware hull texture variants and the native Borg DDS preflight
 * repair for Armada II / Fleet Operations Roots.
 *
 * The render hook deliberately chains through CraftClass's native
 * SetBorgMeshTextures method first. Fleet Operations' Jan_B patch inside that
 * method therefore remains authoritative for Borg diffuse/bump handling.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "texture_variants.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

extern "C" {
std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);

void a2fo_texture_variants_render_swap_hook();
int __cdecl a2fo_texture_variants_borg_filename_hook(
    void* generated_filename, const char* texture_name) noexcept;
void __cdecl a2fo_texture_variants_apply_render_textures(
    void* craft_instance, void* object_class,
    std::uint32_t use_borg_textures) noexcept;
}

void __attribute__((fastcall)) craft_instance_update_hook(
    void* craft_instance, void*, void* object) noexcept;
void A2FO_CALL race_loaded_handler(
    const A2FO_RaceLoadedEvent* event, void* user_data);
void A2FO_CALL craft_cleanup_event_handler(
    const A2FO_CraftEvent* event, void* user_data);

namespace {

using a2fo::texture_variants::faction_texture_name;
using a2fo::texture_variants::faction_node_flags;
using a2fo::texture_variants::normalize_faction_node_name;
using a2fo::texture_variants::normalize_faction_suffix;
using a2fo::texture_variants::subsystem_condition;
using a2fo::texture_variants::subsystem_mesh_choice;
using a2fo::texture_variants::subsystem_rebuild_scale;
using a2fo::texture_variants::subsystem_repair_sample;
using a2fo::texture_variants::subsystem_damage_policy_active;
using a2fo::texture_variants::SubsystemCondition;
using a2fo::texture_variants::texture_asset_path;

constexpr char kModuleName[] = "A2FOTextureVariants";
constexpr char kFactionSuffixCommand[] = "factionTextureSuffix";
constexpr char kFactionNameCommand[] = "name";
constexpr char kRepairParticleName[] = "xspark";
constexpr char kCraftIdentityModuleName[] = "A2FOCraftIdentity.dll";
constexpr std::size_t kMaximumParameterLength = 64;
constexpr std::size_t kMaximumNodeCount = 4096;
constexpr std::size_t kSubsystemCount = 5;
constexpr std::size_t kMaximumScorchEntries = 16;
constexpr std::size_t kMaximumSubsystemMeshEntries = 64;

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kCraftClassSetBorgTexturesRva = 0x000bfb70;
constexpr std::uintptr_t kCraftClassConstructorRva = 0x000bf090;
constexpr std::uintptr_t kCraftRenderBorgSwapCallRva = 0x000cb2ab;
constexpr std::uintptr_t kCraftRenderInstanceCallRva = 0x000cb318;
constexpr std::uintptr_t kCraftInstanceRenderRva = 0x000d59b0;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kParameterDbGetProjectIdRva = 0x00135200;
constexpr std::uintptr_t kMeshGetTextureRva = 0x00231380;
constexpr std::uintptr_t kMeshSetTextureRva = 0x002313b0;
constexpr std::uintptr_t kBorgGenerateFilenameCallRva = 0x0023307e;
constexpr std::uintptr_t kGenerateTextureFilenameRva = 0x002423c0;
constexpr std::uintptr_t kTextureFileExistsRva = 0x002400d0;
constexpr std::uintptr_t kTextureFindRva = 0x00242870;
constexpr std::uintptr_t kExplosionClassFindRva = 0x00064ce0;
constexpr std::uintptr_t kEntityGetTransformRva = 0x000cfd50;
constexpr std::uintptr_t kEntityGetWorldTransformRva = 0x000cff90;
constexpr std::uintptr_t kEntityGetPhysicalDimensionsRva = 0x000cfd70;
constexpr std::uintptr_t kNodeParticleAddRva = 0x000733c0;
constexpr std::uintptr_t kPositionParticleAddRva = 0x000734c0;

// FleetOpsHook.dll RVAs. The map offsets omit the PE .text section's 0x1000
// RVA, so these constants are resolved from the verified runtime VAs.
constexpr std::uintptr_t kCraftInstanceUpdateCallbackRva = 0x001fb250;
constexpr std::uintptr_t kFoCraftClassConstructorHandlerRva = 0x0010d6e4;

constexpr std::size_t kObjectRaceOffset = 0x00fc;
constexpr std::size_t kObjectHandleOffset = 0x0028;
constexpr std::size_t kObjectClassOffset = 0x0040;
constexpr std::size_t kCraftSystemsOffset = 0x01e0;
constexpr std::size_t kCraftSystemSize = 0x0030;
constexpr std::size_t kCraftSystemOperationalOffset = 0x0000;
constexpr std::size_t kCraftSystemForcedDisabledOffset = 0x0001;
constexpr std::size_t kCraftSystemMaximumHitpointsOffset = 0x0004;
constexpr std::size_t kCraftSystemCurrentHitpointsOffset = 0x0018;
constexpr std::size_t kCraftSystemDisableTimeOffset = 0x0028;
constexpr std::size_t kCraftClassGeometryOffset = 0x01d8;
constexpr std::size_t kGeometryRootNodeOffset = 0x003c;
constexpr std::size_t kNodeNameOffset = 0x0008;
constexpr std::size_t kNodeSiblingOffset = 0x001c;
constexpr std::size_t kNodeChildOffset = 0x0020;
constexpr std::size_t kNodeScaleOffset = 0x0054;
constexpr std::size_t kNodeFlagsOffset = 0x00bc;
constexpr std::size_t kMeshLocalBoundsMinimumOffset = 0x00e4;
constexpr std::size_t kMeshLocalBoundsMaximumOffset = 0x00f0;
constexpr std::size_t kMeshBorgTextureOffset = 0x013c;
constexpr std::size_t kTextureDatabaseOffset = 0x0004;
constexpr std::size_t kTextureNameOffset = 0x0008;
constexpr std::size_t kTextureFlagsOffset = 0x0018;

constexpr std::array<std::uint8_t, 5> kExpectedBorgGenerateCall{
    0xe8, 0x3d, 0xf3, 0x00, 0x00};
constexpr std::array<std::uint8_t, 5> kExpectedRenderBorgSwapCall{
    0xe8, 0xc0, 0x48, 0xff, 0xff};
constexpr std::array<std::uint8_t, 5> kExpectedCraftRenderInstanceCall{
    0xe8, 0x93, 0xa6, 0x00, 0x00};
constexpr std::array<std::uint8_t, 6> kExpectedCraftInstanceRender{
    0x55, 0x8b, 0xec, 0x51, 0x8b, 0x81};
constexpr std::array<std::uint8_t, 7> kExpectedCraftInstanceUpdateCallback{
    0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x57};
constexpr std::array<std::uint8_t, 7> kExpectedGenerateTextureFilename{
    0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x57};
constexpr std::array<std::uint8_t, 5> kExpectedSetBorgTextures{
    0x55, 0x8b, 0xec, 0x56, 0x57};
constexpr std::array<std::uint8_t, 5> kExpectedCraftClassConstructor{
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::array<std::uint8_t, 7>
    kExpectedFoCraftClassConstructorHandler{
        0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf8, 0x53};
constexpr std::array<std::uint8_t, 7> kExpectedMeshGetTexture{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x8b};
constexpr std::array<std::uint8_t, 7> kExpectedMeshSetTexture{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x10, 0x53};
constexpr std::array<std::uint8_t, 6> kExpectedTextureFind{
    0x55, 0x8b, 0xec, 0x64, 0xa1, 0x00};
constexpr std::array<std::uint8_t, 9> kExpectedParameterDbGetProjectId{
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x40, 0x01, 0x00, 0x00};
constexpr std::array<std::uint8_t, 9> kExpectedExplosionClassFind{
    0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0xf8, 0xaf, 0x69};
constexpr std::array<std::uint8_t, 9> kExpectedEntityGetWorldTransform{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0c, 0x8b, 0x49, 0x04};
constexpr std::array<std::uint8_t, 7> kExpectedEntityGetTransform{
    0x8b, 0x41, 0x04, 0x83, 0xc0, 0x44, 0xc3};
constexpr std::array<std::uint8_t, 6> kExpectedEntityGetPhysicalDimensions{
    0x8b, 0x41, 0x04, 0x83, 0xc0, 0x34};
constexpr std::array<std::uint8_t, 6> kExpectedNodeParticleAdd{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x34};
constexpr std::array<std::uint8_t, 6> kExpectedPositionParticleAdd{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x34};

using GenerateTextureFilename = int (__cdecl*)(void*, const char*);
using TextureFileExists = bool (__cdecl*)(const char*);
using TextureFind = void* (__cdecl*)(void*, const char*, std::uint32_t);
using ExplosionClassFind = void* (__cdecl*)(const std::uint32_t*);

struct Matrix34 {
    float values[12]{};
};

static_assert(sizeof(Matrix34) == 48,
              "Storm3D Matrix34 must contain twelve floats");

struct Colour {
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
};
static_assert(sizeof(Colour) == 12,
              "ST3D_Colour must contain three floats");

using NodeParticleAdd = void (__cdecl*)(
    const char*, const Colour*, void*, void*, float, float);

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};
static_assert(sizeof(Vector3) == 12,
              "Armada Vector3 must contain three floats");

struct SubsystemDefinition {
    const char* command_prefix = nullptr;
    const char* display_name = nullptr;
};

constexpr std::array<SubsystemDefinition, kSubsystemCount>
    kSubsystemDefinitions{{
        {"sensor", "sensors"},
        {"engine", "engines"},
        {"weapon", "weapons"},
        {"lifeSupport", "life support"},
        {"shieldGenerator", "shield generator"},
    }};

struct MeshRecord {
    void* mesh = nullptr;
    void* original_texture = nullptr;
};

struct FactionNodeRecord {
    void* node = nullptr;
    std::string name;
};

struct SubsystemMeshEntry {
    std::string node_name;
    void* node = nullptr;
    std::vector<void*> subtree_nodes;
    std::uint32_t explosion_project_id = 0;
    float original_scale = 1.0f;
};

struct SavedNodeRenderState {
    void* node = nullptr;
    std::uint32_t flags = 0;
    float scale = 1.0f;
    bool restore_scale = false;
};

struct DamageRenderOverride {
    std::vector<SavedNodeRenderState> nodes;
};

struct ClassDamageVisualPolicy {
    std::array<std::vector<SubsystemMeshEntry>, kSubsystemCount> systems;
    std::array<std::vector<std::string>, kSubsystemCount> target_hardpoints;
    std::vector<std::string> scorch_effects;
    float damage_threshold = 0.0f;
    bool nodes_resolved = false;
};

struct SubsystemDamageVisualState {
    SubsystemCondition previous_condition =
        SubsystemCondition::operational;
    double previous_hitpoints = 0.0;
    std::uint32_t destruction_count = 0;
    std::uint32_t repair_particle_sequence = 0;
    std::uint32_t scorch_count = 0;
    std::int32_t selected_entry = -1;
    float rebuild_scale = 0.0f;
    bool initialized = false;
};

struct CraftDamageVisualState {
    void* object = nullptr;
    void* object_class = nullptr;
    std::uint32_t handle = 0;
    std::array<SubsystemDamageVisualState, kSubsystemCount> systems;
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
bool g_runtime_alive = false;
bool g_borg_dds_enabled = false;
bool g_faction_variants_enabled = false;
bool g_capture_enabled = false;
bool g_subsystem_visuals_enabled = false;
bool g_subsystem_classes_use_identity_observer = false;
A2FO_InlineHook g_craft_instance_update_hook{};
A2FO_InlineHook g_craft_class_constructor_hook{};
void* g_craft_class_constructor_original = nullptr;

GenerateTextureFilename g_generate_texture_filename = nullptr;
TextureFileExists g_texture_file_exists = nullptr;
TextureFind g_texture_find = nullptr;
ExplosionClassFind g_explosion_class_find = nullptr;
NodeParticleAdd g_node_particle_add = nullptr;
NodeParticleAdd g_position_particle_add = nullptr;
void* g_set_borg_textures = nullptr;
void* g_get_mesh_texture = nullptr;
void* g_set_mesh_texture = nullptr;
void* g_craft_instance_render = nullptr;

std::unordered_map<void*, std::string> g_race_suffixes;
std::unordered_map<void*, std::string> g_race_node_names;
std::unordered_set<std::string> g_known_race_node_names;
struct CraftRaceState {
    void* object = nullptr;
    void* race = nullptr;
};
std::unordered_map<void*, CraftRaceState> g_craft_races;
std::unordered_map<void*, std::vector<MeshRecord>> g_class_meshes;
std::unordered_map<void*, std::vector<FactionNodeRecord>>
    g_class_faction_nodes;
std::unordered_map<void*, std::unordered_map<std::string, void*>>
    g_variant_textures;
std::unordered_map<void*, ClassDamageVisualPolicy> g_class_damage_visuals;
std::unordered_map<void*, CraftDamageVisualState> g_craft_damage_visuals;

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

bool readable_range(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) !=
            sizeof(information) ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto end = reinterpret_cast<std::uintptr_t>(information.BaseAddress) +
        information.RegionSize;
    return start <= end && size <= end - start;
}

bool writable_range(void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) !=
            sizeof(information) ||
        information.State != MEM_COMMIT ||
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
    const auto end = reinterpret_cast<std::uintptr_t>(information.BaseAddress) +
        information.RegionSize;
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

template <typename Value>
bool write_at(void* object, std::size_t offset, const Value& value) noexcept {
    if (!object) return false;
    auto* address = static_cast<std::uint8_t*>(object) + offset;
    if (!writable_range(address, sizeof(value))) return false;
    std::memcpy(address, &value, sizeof(value));
    return true;
}

template <typename Function>
Function function_from_address(void* address) noexcept {
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(address),
                  "32-bit function pointer must match data pointer");
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::array<std::uint8_t, Size>& expected) noexcept {
    void* address = at(module, rva);
    return readable_range(address, expected.size()) &&
        std::memcmp(address, expected.data(), expected.size()) == 0;
}

bool texture_exists(const std::string& texture_name,
                    const char* extension) noexcept {
    if (!g_texture_file_exists || texture_name.empty() || !extension) {
        return false;
    }
    try {
        const std::string base = faction_texture_name(texture_name, {});
        const std::string path = texture_asset_path(base, extension);
        return !path.empty() && g_texture_file_exists(path.c_str());
    } catch (...) {
        return false;
    }
}

void set_mesh_texture(void* mesh, void* texture) noexcept {
    if (!mesh || !texture || !g_set_mesh_texture) return;
    a2fo_texture_variants_call_thiscall_2(
        g_set_mesh_texture, mesh,
        reinterpret_cast<std::uintptr_t>(texture), 0);
}

bool copy_normalized_node_name(void* node, std::string* normalized) noexcept {
    if (!normalized) return false;
    const char* source = read_at<const char*>(
        node, kNodeNameOffset, nullptr);
    if (!source) return false;
    try {
        std::array<char,
                   a2fo::texture_variants::kMaximumFactionNodeNameLength + 1>
            value{};
        bool terminated = false;
        for (std::size_t index = 0; index < value.size(); ++index) {
            if (!readable_range(source + index, 1)) return false;
            value[index] = source[index];
            if (value[index] == '\0') {
                terminated = true;
                break;
            }
        }
        return terminated &&
            normalize_faction_node_name(value.data(), normalized);
    } catch (...) {
        return false;
    }
}

void rebuild_known_race_node_names() {
    g_known_race_node_names.clear();
    for (const auto& policy : g_race_node_names) {
        if (!policy.second.empty()) {
            g_known_race_node_names.insert(policy.second);
        }
    }
    // A Race can be registered after a CraftClass was first encountered.
    // Re-enumerate class nodes when the known-name set changes.
    g_class_faction_nodes.clear();
}

void collect_class_faction_nodes(void* object_class) {
    if (!object_class || g_class_faction_nodes.find(object_class) !=
            g_class_faction_nodes.end()) {
        return;
    }

    std::vector<FactionNodeRecord> faction_nodes;
    void* geometry = read_at<void*>(
        object_class, kCraftClassGeometryOffset, nullptr);
    void* root = read_at<void*>(geometry, kGeometryRootNodeOffset, nullptr);
    std::vector<void*> pending;
    std::unordered_set<void*> visited;
    if (root) pending.push_back(root);

    while (!pending.empty() && visited.size() < kMaximumNodeCount) {
        void* node = pending.back();
        pending.pop_back();
        if (!node || !visited.insert(node).second) continue;

        void* child = read_at<void*>(node, kNodeChildOffset, nullptr);
        void* sibling = read_at<void*>(node, kNodeSiblingOffset, nullptr);
        if (child) pending.push_back(child);
        if (sibling) pending.push_back(sibling);

        std::string name;
        if (copy_normalized_node_name(node, &name) && name != "borg" &&
            g_known_race_node_names.find(name) !=
                g_known_race_node_names.end()) {
            faction_nodes.push_back({node, std::move(name)});
        }
    }

    g_class_faction_nodes.emplace(object_class, std::move(faction_nodes));
}

std::vector<void*> collect_node_subtree(void* root) {
    std::vector<void*> nodes;
    if (!root) return nodes;

    std::vector<void*> pending{root};
    std::unordered_set<void*> queued{root};
    while (!pending.empty() && nodes.size() < kMaximumNodeCount) {
        void* node = pending.back();
        pending.pop_back();
        if (!node) continue;
        nodes.push_back(node);

        // A node's sibling belongs to its parent's child list, not to the
        // node's own subtree. Walk sibling links only while enumerating this
        // node's direct children so the configured part cannot claim an
        // unrelated sibling branch.
        void* child = read_at<void*>(node, kNodeChildOffset, nullptr);
        std::size_t sibling_count = 0;
        while (child && queued.size() < kMaximumNodeCount &&
               sibling_count++ < kMaximumNodeCount) {
            if (queued.insert(child).second) pending.push_back(child);
            child = read_at<void*>(child, kNodeSiblingOffset, nullptr);
        }
    }
    return nodes;
}

void resolve_class_damage_nodes(void* object_class) {
    if (!object_class) return;
    const auto found = g_class_damage_visuals.find(object_class);
    if (found == g_class_damage_visuals.end() ||
        found->second.nodes_resolved) {
        return;
    }

    ClassDamageVisualPolicy& policy = found->second;
    void* geometry = read_at<void*>(
        object_class, kCraftClassGeometryOffset, nullptr);
    void* root = read_at<void*>(geometry, kGeometryRootNodeOffset, nullptr);
    if (!root) return;

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
        if (!copy_normalized_node_name(node, &name)) continue;
        for (auto& system : policy.systems) {
            for (SubsystemMeshEntry& entry : system) {
                if (!entry.node && entry.node_name == name) {
                    entry.node = node;
                    entry.subtree_nodes = collect_node_subtree(node);
                    const float scale = read_at<float>(
                        node, kNodeScaleOffset, 1.0f);
                    entry.original_scale =
                        std::isfinite(scale) && scale > 0.0001f
                        ? scale : 1.0f;
                }
            }
        }
    }
    policy.nodes_resolved = true;

    for (std::size_t system_index = 0;
         system_index < kSubsystemCount; ++system_index) {
        for (const SubsystemMeshEntry& entry :
             policy.systems[system_index]) {
            if (entry.node) continue;
            char message[256]{};
            std::snprintf(
                message, sizeof(message),
                "Could not find %s damage mesh node '%s' on CraftClass %p",
                kSubsystemDefinitions[system_index].display_name,
                entry.node_name.c_str(), object_class);
            log_line(message);
        }
    }
}

void begin_subsystem_mesh_render_override(
    void* craft_instance, DamageRenderOverride* render_override) noexcept {
    if (!g_runtime_alive || !g_subsystem_visuals_enabled ||
        !craft_instance || !render_override) {
        return;
    }
    try {
        render_override->nodes.clear();
        const auto state = g_craft_damage_visuals.find(craft_instance);
        if (state == g_craft_damage_visuals.end()) return;
        const CraftDamageVisualState& craft_state = state->second;
        void* object_class = craft_state.object_class;
        resolve_class_damage_nodes(object_class);
        const auto policy = g_class_damage_visuals.find(object_class);
        if (policy == g_class_damage_visuals.end()) return;

        struct PendingNodeOverride {
            bool force_hidden = false;
            bool change_scale = false;
            float desired_scale = 1.0f;
        };
        std::unordered_map<void*, PendingNodeOverride> pending;

        for (std::size_t system_index = 0;
             system_index < kSubsystemCount; ++system_index) {
            const SubsystemDamageVisualState& visual =
                craft_state.systems[system_index];
            const std::int32_t selected = visual.selected_entry;
            const auto& entries = policy->second.systems[system_index];
            if (selected < 0 ||
                static_cast<std::size_t>(selected) >= entries.size()) {
                continue;
            }
            const SubsystemMeshEntry& entry =
                entries[static_cast<std::size_t>(selected)];
            if (!entry.node) continue;

            const float rebuild_scale = std::clamp(
                visual.rebuild_scale, 0.0f, 1.0f);
            const auto& subtree = entry.subtree_nodes;
            if (subtree.empty()) continue;
            for (std::size_t index = 0; index < subtree.size(); ++index) {
                void* node = subtree[index];
                if (!node) continue;
                PendingNodeOverride& node_override = pending[node];
                // The selected part itself may grow back. Every attachment
                // below it stays suppressed until reconstruction completes:
                // otherwise hardpoint sprites, emitters, Borg geometry, and
                // native damage geometry can hang in the missing space.
                node_override.force_hidden =
                    node_override.force_hidden || index != 0 ||
                    rebuild_scale <= 0.0001f;
            }
            PendingNodeOverride& root_override = pending[entry.node];
            const float desired_scale =
                entry.original_scale * rebuild_scale;
            if (!root_override.change_scale) {
                root_override.change_scale = true;
                root_override.desired_scale = desired_scale;
            } else {
                root_override.desired_scale = std::min(
                    root_override.desired_scale, desired_scale);
            }
        }

        render_override->nodes.reserve(pending.size());
        for (const auto& row : pending) {
            void* node = row.first;
            const PendingNodeOverride& node_override = row.second;
            SavedNodeRenderState saved;
            saved.node = node;
            saved.flags = read_at<std::uint32_t>(
                node, kNodeFlagsOffset, 0);
            saved.restore_scale = node_override.change_scale;
            if (saved.restore_scale) {
                saved.scale = read_at<float>(
                    node, kNodeScaleOffset, 1.0f);
            }
            render_override->nodes.push_back(saved);

            if (node_override.force_hidden) {
                const std::uint32_t hidden = faction_node_flags(
                    saved.flags, false);
                if (hidden != saved.flags) {
                    write_at(node, kNodeFlagsOffset, hidden);
                }
            }
            if (node_override.change_scale &&
                std::isfinite(node_override.desired_scale) &&
                node_override.desired_scale >= 0.0f &&
                (!std::isfinite(saved.scale) ||
                 std::fabs(saved.scale - node_override.desired_scale) >
                     0.0001f)) {
                write_at(
                    node, kNodeScaleOffset, node_override.desired_scale);
            }
        }
    } catch (...) {
        log_line("Could not apply subsystem damage-subtree visibility");
    }
}

void restore_subsystem_mesh_render_override(
    DamageRenderOverride* render_override) noexcept {
    if (!render_override) return;
    for (auto iterator = render_override->nodes.rbegin();
         iterator != render_override->nodes.rend(); ++iterator) {
        if (!iterator->node) continue;
        write_at(iterator->node, kNodeFlagsOffset, iterator->flags);
        if (iterator->restore_scale) {
            write_at(iterator->node, kNodeScaleOffset, iterator->scale);
        }
    }
    render_override->nodes.clear();
}

std::uintptr_t __attribute__((fastcall)) craft_instance_render_call_hook(
    void* craft_instance, void*, std::uintptr_t render_context) noexcept {
    if (!g_craft_instance_render) return 0;
    DamageRenderOverride render_override;
    begin_subsystem_mesh_render_override(
        craft_instance, &render_override);
    const std::uintptr_t result =
        a2fo_texture_variants_call_thiscall_1(
            g_craft_instance_render, craft_instance, render_context);
    restore_subsystem_mesh_render_override(&render_override);
    return result;
}

void collect_class_meshes(void* object_class) {
    if (!object_class || g_class_meshes.find(object_class) !=
            g_class_meshes.end()) {
        return;
    }

    std::vector<MeshRecord> meshes;
    void* geometry = read_at<void*>(
        object_class, kCraftClassGeometryOffset, nullptr);
    void* root = read_at<void*>(geometry, kGeometryRootNodeOffset, nullptr);
    std::vector<void*> pending;
    std::unordered_set<void*> visited;
    if (root) pending.push_back(root);

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
        if (!get_type || !readable_range(get_type, 1)) continue;
        const std::uintptr_t node_type =
            a2fo_texture_variants_call_thiscall_0(get_type, node);
        if (node_type != 1) continue;

        void* texture = reinterpret_cast<void*>(
            a2fo_texture_variants_call_thiscall_1(
                g_get_mesh_texture, node, 0));
        if (texture) meshes.push_back({node, texture});
    }

    g_class_meshes.emplace(object_class, std::move(meshes));
}

void apply_faction_node_visibility(void* craft_instance,
                                   void* object_class) noexcept {
    if (!g_runtime_alive || !g_faction_variants_enabled ||
        !craft_instance || !object_class ||
        g_known_race_node_names.empty()) {
        return;
    }
    try {
        const auto nodes = g_class_faction_nodes.find(object_class);
        if (nodes == g_class_faction_nodes.end()) return;

        std::string owner_name;
        const auto craft_race = g_craft_races.find(craft_instance);
        if (craft_race != g_craft_races.end()) {
            const auto race_name = g_race_node_names.find(
                craft_race->second.race);
            if (race_name != g_race_node_names.end()) {
                owner_name = race_name->second;
            }
        }

        for (const FactionNodeRecord& record : nodes->second) {
            const std::uint32_t current = read_at<std::uint32_t>(
                record.node, kNodeFlagsOffset, 0);
            const std::uint32_t selected = faction_node_flags(
                current, !owner_name.empty() && record.name == owner_name);
            if (selected != current) {
                write_at(record.node, kNodeFlagsOffset, selected);
            }
        }
    } catch (...) {
        log_line("Could not apply faction model-node visibility");
    }
}

void* find_variant_texture(void* original_texture,
                           const std::string& suffix) noexcept {
    if (!original_texture || suffix.empty() || !g_texture_find) return nullptr;
    try {
        auto& by_suffix = g_variant_textures[original_texture];
        const auto cached = by_suffix.find(suffix);
        if (cached != by_suffix.end()) return cached->second;

        const char* original_name = read_at<const char*>(
            original_texture, kTextureNameOffset, nullptr);
        std::string variant_name = original_name
            ? faction_texture_name(original_name, suffix)
            : std::string{};
        void* variant = nullptr;
        const bool dds_exists = texture_exists(variant_name, ".dds");
        const bool tga_exists = !dds_exists &&
            texture_exists(variant_name, ".tga");
        if (!variant_name.empty() && (dds_exists || tga_exists)) {
            void* database = read_at<void*>(
                original_texture, kTextureDatabaseOffset, nullptr);
            const std::uint32_t flags = read_at<std::uint32_t>(
                original_texture, kTextureFlagsOffset, 0);
            variant = g_texture_find(
                database, variant_name.c_str(), flags);
            if (variant) {
                char message[512]{};
                std::snprintf(
                    message, sizeof(message),
                    "Resolved faction texture '%s' from %s",
                    variant_name.c_str(), dds_exists ? "DDS" : "TGA");
                log_line(message);
            }
        }
        by_suffix.emplace(suffix, variant);
        return variant;
    } catch (...) {
        return nullptr;
    }
}

void apply_faction_textures(void* craft_instance,
                            void* object_class) noexcept {
    if (!g_runtime_alive || !g_faction_variants_enabled ||
        !craft_instance || !object_class || g_race_suffixes.empty()) {
        return;
    }
    try {
        const auto meshes = g_class_meshes.find(object_class);
        if (meshes == g_class_meshes.end()) return;

        std::string suffix;
        const auto craft_race = g_craft_races.find(craft_instance);
        if (craft_race != g_craft_races.end()) {
            const auto race_suffix = g_race_suffixes.find(
                craft_race->second.race);
            if (race_suffix != g_race_suffixes.end()) {
                suffix = race_suffix->second;
            }
        }

        for (const MeshRecord& record : meshes->second) {
            void* variant = suffix.empty()
                ? nullptr
                : find_variant_texture(record.original_texture, suffix);
            if (variant) {
                set_mesh_texture(record.mesh, variant);
                continue;
            }

            // Native SetBorgMeshTextures has just selected the correct base or
            // Borg texture for meshes with a Borg alternate. Meshes outside
            // that native table must be explicitly restored after another
            // owner's faction variant was rendered from the shared model.
            if (!read_at<void*>(
                    record.mesh, kMeshBorgTextureOffset, nullptr)) {
                set_mesh_texture(record.mesh, record.original_texture);
            }
        }
    } catch (...) {
        log_line("Could not apply a faction texture variant; native textures retained");
    }
}

bool install_borg_dds_fix(const A2FO_ModuleApi* api) noexcept {
    if (!api || !api->patch_call ||
        !signature_matches(g_armada, kBorgGenerateFilenameCallRva,
                           kExpectedBorgGenerateCall) ||
        !signature_matches(g_armada, kGenerateTextureFilenameRva,
                           kExpectedGenerateTextureFilename)) {
        return false;
    }
    g_borg_dds_enabled = true;
    if (!api->patch_call(
            at(g_armada, kBorgGenerateFilenameCallRva),
            reinterpret_cast<void*>(&a2fo_texture_variants_borg_filename_hook),
            kExpectedBorgGenerateCall.data(),
            kExpectedBorgGenerateCall.size())) {
        g_borg_dds_enabled = false;
        return false;
    }
    return true;
}

bool faction_signatures_supported() noexcept {
    return signature_matches(g_armada, kCraftRenderBorgSwapCallRva,
                             kExpectedRenderBorgSwapCall) &&
        signature_matches(g_armada, kCraftRenderInstanceCallRva,
                          kExpectedCraftRenderInstanceCall) &&
        signature_matches(g_armada, kCraftInstanceRenderRva,
                          kExpectedCraftInstanceRender) &&
        signature_matches(g_armada, kCraftClassSetBorgTexturesRva,
                          kExpectedSetBorgTextures) &&
        signature_matches(g_armada, kMeshGetTextureRva,
                          kExpectedMeshGetTexture) &&
        signature_matches(g_armada, kMeshSetTextureRva,
                          kExpectedMeshSetTexture) &&
        signature_matches(g_armada, kTextureFindRva,
                          kExpectedTextureFind) &&
        signature_matches(g_fleet_ops, kCraftInstanceUpdateCallbackRva,
                          kExpectedCraftInstanceUpdateCallback);
}

bool subsystem_visual_signatures_supported() noexcept {
    bool supported = true;
    const auto require_signature = [&](std::uintptr_t rva,
                                       const auto& expected,
                                       const char* name) {
        if (signature_matches(g_armada, rva, expected)) return;
        char message[192]{};
        std::snprintf(message, sizeof(message),
                      "%s signature is unsupported", name);
        log_line(message);
        supported = false;
    };
    require_signature(
        kParameterDbGetProjectIdRva, kExpectedParameterDbGetProjectId,
        "ParameterDB::GetProjectId");
    require_signature(
        kExplosionClassFindRva, kExpectedExplosionClassFind,
        "ExplosionClass::Find");
    require_signature(
        kEntityGetTransformRva, kExpectedEntityGetTransform,
        "Entity::GetTransform");
    require_signature(
        kEntityGetWorldTransformRva, kExpectedEntityGetWorldTransform,
        "Entity::GetWorldTransform");
    require_signature(
        kEntityGetPhysicalDimensionsRva,
        kExpectedEntityGetPhysicalDimensions,
        "Entity::GetPhysicalDimensions");
    require_signature(
        kNodeParticleAddRva, kExpectedNodeParticleAdd,
        "NodeParticleEffect::AddParticle(node)");
    require_signature(
        kPositionParticleAddRva, kExpectedPositionParticleAdd,
        "NodeParticleEffect::AddParticle(position)");
    return supported;
}

bool install_faction_variants(const A2FO_ModuleApi* api) noexcept {
    if (!api || !api->patch_call || !api->install_inline_hook ||
        !faction_signatures_supported()) {
        return false;
    }

    if (!A2FO_MODULE_API_HAS(api, register_race_loaded_handler) ||
        (api->capabilities & A2FO_CAP_RACE_LOADED) == 0 ||
        !api->register_race_loaded_handler) {
        return false;
    }
    const char* race_fields[] = {
        kFactionNameCommand, kFactionSuffixCommand};
    if (!api->register_race_loaded_handler(
            kModuleName, race_fields,
            static_cast<std::uint32_t>(std::size(race_fields)),
            &race_loaded_handler, nullptr)) {
        return false;
    }
    g_capture_enabled = true;
    if (!api->install_inline_hook(
            at(g_fleet_ops, kCraftInstanceUpdateCallbackRva),
            reinterpret_cast<void*>(&craft_instance_update_hook),
            kExpectedCraftInstanceUpdateCallback.size(),
            kExpectedCraftInstanceUpdateCallback.data(),
            &g_craft_instance_update_hook)) {
        return false;
    }
    if (!api->patch_call(
            at(g_armada, kCraftRenderBorgSwapCallRva),
            reinterpret_cast<void*>(&a2fo_texture_variants_render_swap_hook),
            kExpectedRenderBorgSwapCall.data(),
            kExpectedRenderBorgSwapCall.size())) {
        return false;
    }
    g_craft_instance_render = at(g_armada, kCraftInstanceRenderRva);
    if (!api->patch_call(
            at(g_armada, kCraftRenderInstanceCallRva),
            reinterpret_cast<void*>(&craft_instance_render_call_hook),
            kExpectedCraftRenderInstanceCall.data(),
            kExpectedCraftRenderInstanceCall.size())) {
        return false;
    }
    g_faction_variants_enabled = true;
    return true;
}

bool parameter_db_string(void* parameter_db, const char* command,
                         std::string* value) {
    if (!parameter_db || !command || !value) return false;
    std::array<char, kMaximumParameterLength> buffer{};
    const std::uintptr_t found =
        a2fo_texture_variants_call_thiscall_4(
            at(g_armada, kParameterDbGetStringRva), parameter_db,
            reinterpret_cast<std::uintptr_t>(command),
            reinterpret_cast<std::uintptr_t>(buffer.data()),
            static_cast<std::uintptr_t>(buffer.size()),
            reinterpret_cast<std::uintptr_t>(""));
    buffer.back() = '\0';
    if ((found & 0xffu) == 0) {
        value->clear();
        return false;
    }
    value->assign(buffer.data());
    return true;
}

bool parameter_db_project_id(void* parameter_db, const char* command,
                             std::uint32_t* project_id) noexcept {
    if (!parameter_db || !command || !project_id) return false;
    *project_id = 0;
    const std::uintptr_t found =
        a2fo_texture_variants_call_thiscall_3(
            at(g_armada, kParameterDbGetProjectIdRva), parameter_db,
            reinterpret_cast<std::uintptr_t>(command),
            reinterpret_cast<std::uintptr_t>(project_id), 0);
    return (found & 0xffu) != 0;
}

void register_class_damage_visuals(void* object_class,
                                   void* parameter_db) noexcept {
    if (!g_runtime_alive || !g_subsystem_visuals_enabled ||
        !object_class || !parameter_db) {
        return;
    }
    try {
        ClassDamageVisualPolicy policy;
        std::string raw_threshold;
        if (parameter_db_string(parameter_db, "damageThreshold", &raw_threshold)) {
            try {
                const float value = std::stof(raw_threshold);
                if (std::isfinite(value) && value > 0.0f && value <= 1.0f) {
                    policy.damage_threshold = value;
                }
            } catch (...) {}
        }
        for (std::size_t system_index = 0; system_index < kSubsystemCount; ++system_index) {
            const char* prefix = kSubsystemDefinitions[system_index].command_prefix;
            const std::string command = std::string(prefix) + "TargetHardpoints";
            std::string raw;
            if (parameter_db_string(parameter_db, command.c_str(), &raw)) {
                std::string token;
                for (char ch : raw) {
                    if (ch == ' ' || ch == ',' || ch == '\t') {
                        std::string normalized;
                        if (!token.empty() && normalize_faction_node_name(token, &normalized))
                            policy.target_hardpoints[system_index].push_back(std::move(normalized));
                        token.clear();
                    } else token.push_back(ch);
                }
                std::string normalized;
                if (!token.empty() && normalize_faction_node_name(token, &normalized))
                    policy.target_hardpoints[system_index].push_back(std::move(normalized));
            }
        }
        for (std::size_t index = 1; index <= kMaximumScorchEntries; ++index) {
            std::string raw;
            const std::string command = "scorchTexture" + std::to_string(index);
            if (!parameter_db_string(parameter_db, command.c_str(), &raw)) continue;
            if (!raw.empty()) policy.scorch_effects.push_back(raw);
        }
        // Numeric ODF fields are not exposed by the string accessor on some
        // Fleet Ops builds. If scorch settings are present but the numeric
        // read was unavailable, use the documented 10% default.
        if (policy.damage_threshold <= 0.0f &&
            !policy.scorch_effects.empty()) {
            policy.damage_threshold = 0.1f;
        }
        std::size_t total_entries = 0;
        for (std::size_t system_index = 0;
             system_index < kSubsystemCount; ++system_index) {
            const SubsystemDefinition& definition =
                kSubsystemDefinitions[system_index];
            auto& entries = policy.systems[system_index];
            for (std::size_t index = 1;
                 index <= kMaximumSubsystemMeshEntries; ++index) {
                const std::string command =
                    std::string(definition.command_prefix) + "Mesh" +
                    std::to_string(index);
                std::string raw_name;
                if (!parameter_db_string(
                        parameter_db, command.c_str(), &raw_name)) {
                    continue;
                }

                std::string node_name;
                if (!normalize_faction_node_name(raw_name, &node_name)) {
                    char message[256]{};
                    std::snprintf(
                        message, sizeof(message),
                        "Rejected invalid %s value '%s' on CraftClass %p",
                        command.c_str(), raw_name.c_str(), object_class);
                    log_line(message);
                    continue;
                }

                std::uint32_t explosion_project_id = 0;
                const std::string explosion_command =
                    command + "explosion";
                parameter_db_project_id(
                    parameter_db, explosion_command.c_str(),
                    &explosion_project_id);
                entries.push_back({
                    std::move(node_name), nullptr, {},
                    explosion_project_id});
                ++total_entries;
            }
        }

        std::size_t target_hardpoint_count = 0;
        for (const auto& targets : policy.target_hardpoints) {
            target_hardpoint_count += targets.size();
        }
        const bool has_scorch_policy =
            std::isfinite(policy.damage_threshold) &&
            policy.damage_threshold > 0.0f &&
            !policy.scorch_effects.empty() &&
            target_hardpoint_count != 0;
        if (!subsystem_damage_policy_active(
                total_entries, policy.damage_threshold,
                policy.scorch_effects.size(), target_hardpoint_count)) {
            g_class_damage_visuals.erase(object_class);
            return;
        }
        if (!has_scorch_policy) {
            policy.damage_threshold = 0.0f;
            std::vector<std::string>().swap(policy.scorch_effects);
            for (auto& targets : policy.target_hardpoints) {
                std::vector<std::string>().swap(targets);
            }
        } else {
            char message[192]{};
            std::snprintf(message, sizeof(message),
                          "Registered damage threshold %.3f with %u scorch effect(s)",
                          policy.damage_threshold,
                          static_cast<unsigned>(policy.scorch_effects.size()));
            log_line(message);
        }
        g_class_damage_visuals[object_class] = std::move(policy);
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "Registered %lu subsystem damage mesh%s on CraftClass %p",
            static_cast<unsigned long>(total_entries),
            total_entries == 1 ? "" : "es", object_class);
        log_line(message);
    } catch (...) {
        log_line("Could not retain subsystem damage-mesh ODF commands");
    }
}

SubsystemMeshEntry* select_damage_mesh(
    ClassDamageVisualPolicy* policy, std::size_t system_index,
    std::uint32_t craft_handle,
    SubsystemDamageVisualState* state) noexcept {
    if (!policy || !state || system_index >= kSubsystemCount) return nullptr;
    auto& entries = policy->systems[system_index];
    std::vector<std::size_t> available;
    try {
        for (std::size_t index = 0; index < entries.size(); ++index) {
            if (entries[index].node) available.push_back(index);
        }
    } catch (...) {
        return nullptr;
    }
    if (available.empty()) {
        state->selected_entry = -1;
        return nullptr;
    }
    const std::size_t choice = subsystem_mesh_choice(
        craft_handle, system_index, state->destruction_count,
        available.size());
    ++state->destruction_count;
    const std::size_t selected = available[choice];
    state->selected_entry = static_cast<std::int32_t>(selected);
    return &entries[selected];
}

SubsystemMeshEntry* selected_damage_mesh(
    ClassDamageVisualPolicy* policy, std::size_t system_index,
    const SubsystemDamageVisualState& state) noexcept {
    if (!policy || system_index >= kSubsystemCount ||
        state.selected_entry < 0) {
        return nullptr;
    }
    auto& entries = policy->systems[system_index];
    const std::size_t selected =
        static_cast<std::size_t>(state.selected_entry);
    return selected < entries.size() ? &entries[selected] : nullptr;
}

void spawn_damage_explosion(void* craft,
                            const SubsystemMeshEntry& entry) noexcept {
    if (!craft || !entry.node || entry.explosion_project_id == 0 ||
        !g_explosion_class_find) {
        return;
    }
    try {
        Matrix34 transform{};
        const std::uintptr_t transformed =
            a2fo_texture_variants_call_thiscall_2(
                at(g_armada, kEntityGetWorldTransformRva), craft,
                reinterpret_cast<std::uintptr_t>(&transform),
                reinterpret_cast<std::uintptr_t>(entry.node));
        if (transformed != reinterpret_cast<std::uintptr_t>(&transform)) {
            return;
        }

        void* explosion_class =
            g_explosion_class_find(&entry.explosion_project_id);
        void* vtable = read_at<void*>(explosion_class, 0, nullptr);
        void* build = read_at<void*>(vtable, 0x08, nullptr);
        if (!build || !readable_range(build, 1)) return;

        constexpr float scale = 1.0f;
        std::uint32_t scale_bits = 0;
        std::memcpy(&scale_bits, &scale, sizeof(scale_bits));
        a2fo_texture_variants_call_thiscall_2(
            build, explosion_class,
            reinterpret_cast<std::uintptr_t>(&transform), scale_bits);
    } catch (...) {
        log_line("Could not build a subsystem damage explosion");
    }
}

bool finite_vector(const Vector3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

Vector3 transform_point(const Matrix34& matrix,
                        const Vector3& point) noexcept {
    return {
        matrix.values[0] * point.x +
            matrix.values[3] * point.y +
            matrix.values[6] * point.z + matrix.values[9],
        matrix.values[1] * point.x +
            matrix.values[4] * point.y +
            matrix.values[7] * point.z + matrix.values[10],
        matrix.values[2] * point.x +
            matrix.values[5] * point.y +
            matrix.values[8] * point.z + matrix.values[11],
    };
}

Vector3 cross_product(const Vector3& left,
                      const Vector3& right) noexcept {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

float dot_product(const Vector3& left,
                  const Vector3& right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

bool inverse_transform_point(const Matrix34& matrix,
                             const Vector3& world,
                             Vector3* local) noexcept {
    if (!local) return false;
    const Vector3 basis_x{
        matrix.values[0], matrix.values[1], matrix.values[2]};
    const Vector3 basis_y{
        matrix.values[3], matrix.values[4], matrix.values[5]};
    const Vector3 basis_z{
        matrix.values[6], matrix.values[7], matrix.values[8]};
    const Vector3 relative{
        world.x - matrix.values[9],
        world.y - matrix.values[10],
        world.z - matrix.values[11]};
    const Vector3 yz = cross_product(basis_y, basis_z);
    const float determinant = dot_product(basis_x, yz);
    if (!std::isfinite(determinant) ||
        std::fabs(determinant) <= 0.000001f) {
        return false;
    }
    const float reciprocal = 1.0f / determinant;
    *local = {
        dot_product(relative, yz) * reciprocal,
        dot_product(relative, cross_product(basis_z, basis_x)) * reciprocal,
        dot_product(relative, cross_product(basis_x, basis_y)) * reciprocal,
    };
    return finite_vector(*local);
}

bool mesh_local_bounds(void* node, Vector3* minimum,
                       Vector3* maximum) noexcept {
    if (!node || !minimum || !maximum) return false;
    void* vtable = read_at<void*>(node, 0, nullptr);
    void* get_type = read_at<void*>(vtable, sizeof(void*), nullptr);
    if (!get_type || !readable_range(get_type, 1)) return false;
    const std::uintptr_t type =
        a2fo_texture_variants_call_thiscall_0(get_type, node);
    if (type != 1u && type != 9u) return false;

    *minimum = read_at<Vector3>(
        node, kMeshLocalBoundsMinimumOffset, {});
    *maximum = read_at<Vector3>(
        node, kMeshLocalBoundsMaximumOffset, {});
    if (!finite_vector(*minimum) || !finite_vector(*maximum) ||
        maximum->x < minimum->x || maximum->y < minimum->y ||
        maximum->z < minimum->z) {
        return false;
    }
    const float largest_extent = std::max({
        maximum->x - minimum->x,
        maximum->y - minimum->y,
        maximum->z - minimum->z});
    return std::isfinite(largest_extent) &&
        largest_extent > 0.0001f && largest_extent < 100000.0f;
}

bool repair_particle_position(
    void* craft, const SubsystemMeshEntry& entry,
    std::uint32_t craft_handle, std::size_t system_index,
    std::uint32_t sequence, float rebuild_scale,
    Vector3* craft_local, float* particle_size) noexcept {
    if (!craft || !entry.node || !craft_local || !particle_size ||
        !g_position_particle_add) {
        return false;
    }
    Vector3 minimum{};
    Vector3 maximum{};
    if (!mesh_local_bounds(entry.node, &minimum, &maximum)) return false;

    Vector3 point{
        minimum.x + (maximum.x - minimum.x) * subsystem_repair_sample(
            craft_handle, system_index, sequence, 0),
        minimum.y + (maximum.y - minimum.y) * subsystem_repair_sample(
            craft_handle, system_index, sequence, 1),
        minimum.z + (maximum.z - minimum.z) * subsystem_repair_sample(
            craft_handle, system_index, sequence, 2),
    };
    const std::size_t surface_axis = static_cast<std::size_t>(
        subsystem_repair_sample(
            craft_handle, system_index, sequence, 3) * 3.0f) % 3u;
    const bool upper_surface = subsystem_repair_sample(
        craft_handle, system_index, sequence, 4) >= 0.5f;
    float* coordinate = surface_axis == 0 ? &point.x
        : surface_axis == 1 ? &point.y : &point.z;
    const float lower = surface_axis == 0 ? minimum.x
        : surface_axis == 1 ? minimum.y : minimum.z;
    const float upper = surface_axis == 0 ? maximum.x
        : surface_axis == 1 ? maximum.y : maximum.z;
    *coordinate = upper_surface ? upper : lower;

    const float current_scale = read_at<float>(
        entry.node, kNodeScaleOffset, entry.original_scale);
    const float desired_scale = entry.original_scale *
        std::clamp(rebuild_scale, 0.0f, 1.0f);
    const float scale_adjustment =
        std::isfinite(current_scale) && std::fabs(current_scale) > 0.0001f
        ? desired_scale / current_scale : rebuild_scale;
    point.x *= scale_adjustment;
    point.y *= scale_adjustment;
    point.z *= scale_adjustment;

    Matrix34 node_world{};
    const std::uintptr_t transformed =
        a2fo_texture_variants_call_thiscall_2(
            at(g_armada, kEntityGetWorldTransformRva), craft,
            reinterpret_cast<std::uintptr_t>(&node_world),
            reinterpret_cast<std::uintptr_t>(entry.node));
    if (transformed != reinterpret_cast<std::uintptr_t>(&node_world)) {
        return false;
    }
    const auto* craft_transform = reinterpret_cast<const Matrix34*>(
        a2fo_texture_variants_call_thiscall_0(
            at(g_armada, kEntityGetTransformRva), craft));
    if (!readable_range(craft_transform, sizeof(*craft_transform))) {
        return false;
    }
    const Vector3 world = transform_point(node_world, point);
    if (!finite_vector(world) || !inverse_transform_point(
            *craft_transform, world, craft_local)) {
        return false;
    }

    const float largest_extent = std::max({
        maximum.x - minimum.x,
        maximum.y - minimum.y,
        maximum.z - minimum.z});
    *particle_size = std::clamp(
        largest_extent * std::max(desired_scale, 0.08f) * 0.16f,
        0.2f, 8.0f);
    return true;
}

void add_repair_particle(
    void* craft, const SubsystemMeshEntry& entry,
    std::uint32_t craft_handle, std::size_t system_index,
    std::uint32_t sequence, float rebuild_scale) noexcept {
    if (!craft || !entry.node || !g_node_particle_add) return;
    try {
        static const Colour white{};
        Vector3 position{};
        float size = 1.0f;
        if (repair_particle_position(
                craft, entry, craft_handle, system_index, sequence,
                rebuild_scale, &position, &size)) {
            g_position_particle_add(
                kRepairParticleName, &white, craft, &position,
                size, 0.35f);
            return;
        }

        const auto* dimensions = reinterpret_cast<const float*>(
            a2fo_texture_variants_call_thiscall_0(
                at(g_armada, kEntityGetPhysicalDimensionsRva), craft));
        if (readable_range(dimensions, sizeof(float) * 4)) {
            const float radius = dimensions[3];
            if (std::isfinite(radius) && radius > 0.0f) {
                size = std::clamp(radius * 0.08f, 0.25f, 12.0f);
            }
        }
        g_node_particle_add(
            kRepairParticleName, &white, craft, entry.node, size, 0.35f);
    } catch (...) {
        log_line("Could not add a subsystem repair particle");
    }
}

void add_scorch_marker(void* craft, void* object_class,
                       ClassDamageVisualPolicy& policy,
                       SubsystemDamageVisualState& visual,
                       std::uint32_t handle, std::size_t system_index,
                       double current_hitpoints,
                       std::int32_t maximum_hitpoints) noexcept {
    if (!craft || !object_class || policy.damage_threshold <= 0.0f ||
        policy.target_hardpoints[system_index].empty() ||
        policy.scorch_effects.empty() || maximum_hitpoints <= 0) return;
    const double fraction = std::clamp(
        current_hitpoints / static_cast<double>(maximum_hitpoints), 0.0, 1.0);
    const std::uint32_t band = static_cast<std::uint32_t>(
        std::floor((1.0 - fraction) / policy.damage_threshold));
    if (band <= visual.scorch_count || band == 0) return;
    const std::string& wanted = policy.target_hardpoints[system_index][
        (band - 1u) % policy.target_hardpoints[system_index].size()];
    void* geometry = read_at<void*>(object_class, kCraftClassGeometryOffset, nullptr);
    void* root = read_at<void*>(geometry, kGeometryRootNodeOffset, nullptr);
    std::vector<void*> pending{root};
    std::unordered_set<void*> visited;
    void* target = nullptr;
    while (!pending.empty() && visited.size() < kMaximumNodeCount) {
        void* node = pending.back(); pending.pop_back();
        if (!node || !visited.insert(node).second) continue;
        void* child = read_at<void*>(node, kNodeChildOffset, nullptr);
        void* sibling = read_at<void*>(node, kNodeSiblingOffset, nullptr);
        if (child) pending.push_back(child);
        if (sibling) pending.push_back(sibling);
        std::string name;
        if (copy_normalized_node_name(node, &name) && name == wanted) { target = node; break; }
    }
    if (!target) return;
    SubsystemMeshEntry marker;
    marker.node = target;
    marker.original_scale = 1.0f;
    add_repair_particle(craft, marker, handle, system_index, band, 1.0f);
    visual.scorch_count = band;
    char message[192]{};
    std::snprintf(message, sizeof(message), "Applied scorch marker %u at hardpoint '%s' (%s)",
                  band, wanted.c_str(), policy.scorch_effects[(band - 1u) % policy.scorch_effects.size()].c_str());
    log_line(message);
}

void update_subsystem_damage_visuals(void* craft_instance,
                                     void* craft) noexcept {
    if (!g_runtime_alive || !g_subsystem_visuals_enabled ||
        !craft_instance || !craft) {
        return;
    }
    try {
        void* object_class = read_at<void*>(
            craft, kObjectClassOffset, nullptr);
        const auto policy_row = g_class_damage_visuals.find(object_class);
        if (policy_row == g_class_damage_visuals.end()) {
            g_craft_damage_visuals.erase(craft_instance);
            return;
        }
        resolve_class_damage_nodes(object_class);
        ClassDamageVisualPolicy& policy = policy_row->second;

        void* systems = read_at<void*>(craft, kCraftSystemsOffset, nullptr);
        const std::uint32_t handle = read_at<std::uint32_t>(
            craft, kObjectHandleOffset, 0);
        if (!systems || handle == 0) return;

        CraftDamageVisualState& craft_state =
            g_craft_damage_visuals[craft_instance];
        if (craft_state.object != craft ||
            craft_state.object_class != object_class ||
            craft_state.handle != handle) {
            craft_state = {};
            craft_state.object = craft;
            craft_state.object_class = object_class;
            craft_state.handle = handle;
        }

        for (std::size_t system_index = 0;
             system_index < kSubsystemCount; ++system_index) {
            auto* system = static_cast<std::uint8_t*>(systems) +
                system_index * kCraftSystemSize;
            const bool operational = read_at<std::uint8_t>(
                system, kCraftSystemOperationalOffset, 1) != 0;
            const bool forced_disabled = read_at<std::uint8_t>(
                system, kCraftSystemForcedDisabledOffset, 0) != 0;
            const std::int32_t maximum_hitpoints = read_at<std::int32_t>(
                system, kCraftSystemMaximumHitpointsOffset, 0);
            const double current_hitpoints = read_at<double>(
                system, kCraftSystemCurrentHitpointsOffset, 0.0);
            const float disable_time = read_at<float>(
                system, kCraftSystemDisableTimeOffset, 0.0f);
            const SubsystemCondition condition = subsystem_condition(
                operational, forced_disabled, maximum_hitpoints,
                current_hitpoints, disable_time);

            SubsystemDamageVisualState& visual =
                craft_state.systems[system_index];
            if (policy.damage_threshold > 0.0f) {
                add_scorch_marker(craft, object_class, policy, visual, handle,
                                  system_index, current_hitpoints,
                                  maximum_hitpoints);
            }
            if (!visual.initialized) {
                visual.initialized = true;
                visual.previous_condition = condition;
                visual.previous_hitpoints = current_hitpoints;
                if (condition == SubsystemCondition::destroyed) {
                    select_damage_mesh(
                        &policy, system_index, handle, &visual);
                    visual.rebuild_scale = subsystem_rebuild_scale(
                        maximum_hitpoints, current_hitpoints);
                }
                continue;
            }

            SubsystemMeshEntry* selected = selected_damage_mesh(
                &policy, system_index, visual);
            if (condition == SubsystemCondition::destroyed &&
                visual.previous_condition !=
                    SubsystemCondition::destroyed) {
                selected = select_damage_mesh(
                    &policy, system_index, handle, &visual);
                visual.rebuild_scale = subsystem_rebuild_scale(
                    maximum_hitpoints, current_hitpoints);
                if (selected) spawn_damage_explosion(craft, *selected);
            } else if (condition == SubsystemCondition::destroyed &&
                       !selected) {
                selected = select_damage_mesh(
                    &policy, system_index, handle, &visual);
                visual.rebuild_scale = subsystem_rebuild_scale(
                    maximum_hitpoints, current_hitpoints);
            }

            if (selected) {
                visual.rebuild_scale = subsystem_rebuild_scale(
                    maximum_hitpoints, current_hitpoints);
                if (std::isfinite(current_hitpoints) &&
                    std::isfinite(visual.previous_hitpoints) &&
                    current_hitpoints >
                        visual.previous_hitpoints + 0.0001) {
                    add_repair_particle(
                        craft, *selected, handle, system_index,
                        visual.repair_particle_sequence++,
                        visual.rebuild_scale);
                }
            }
            if (condition == SubsystemCondition::operational) {
                if (selected && visual.previous_condition !=
                        SubsystemCondition::operational) {
                    for (std::size_t burst = 0; burst < 3; ++burst) {
                        add_repair_particle(
                            craft, *selected, handle, system_index,
                            visual.repair_particle_sequence++, 1.0f);
                    }
                }
                visual.selected_entry = -1;
                visual.rebuild_scale = 1.0f;
            }
            visual.previous_condition = condition;
            visual.previous_hitpoints = current_hitpoints;
        }
    } catch (...) {
        log_line("Could not update subsystem damage visuals");
    }
}

std::uintptr_t __attribute__((fastcall)) craft_class_constructor_hook(
    void* self, void*, void* parent_class, void* parameter_db) noexcept {
    if (!g_craft_class_constructor_original) return 0;
    const std::uintptr_t result =
        a2fo_texture_variants_call_thiscall_2(
            g_craft_class_constructor_original, self,
            reinterpret_cast<std::uintptr_t>(parent_class),
            reinterpret_cast<std::uintptr_t>(parameter_db));
    register_class_damage_visuals(self, parameter_db);
    return result;
}

void* existing_detour_destination(const void* site,
                                  std::size_t* patch_length) noexcept {
    if (patch_length) *patch_length = 0;
    if (!site || !readable_range(site, 5)) return nullptr;
    const auto* bytes = static_cast<const std::uint8_t*>(site);
    if (bytes[0] == 0xe9) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, bytes + 1, sizeof(displacement));
        if (patch_length) *patch_length = 5;
        return const_cast<std::uint8_t*>(bytes + 5 + displacement);
    }
    if (readable_range(site, 6) && bytes[0] == 0x68 &&
        bytes[5] == 0xc3) {
        std::uint32_t destination = 0;
        std::memcpy(&destination, bytes + 1, sizeof(destination));
        if (patch_length) *patch_length = 6;
        return reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(destination));
    }
    return nullptr;
}

bool install_subsystem_class_registration(
    const A2FO_ModuleApi* api) noexcept {
    if (GetModuleHandleA(kCraftIdentityModuleName)) {
        g_subsystem_classes_use_identity_observer = true;
        return true;
    }
    if (!api || !api->install_inline_hook || !api->patch_jump) return false;

    void* site = at(g_armada, kCraftClassConstructorRva);
    if (signature_matches(
            g_armada, kCraftClassConstructorRva,
            kExpectedCraftClassConstructor)) {
        if (!api->install_inline_hook(
                site,
                reinterpret_cast<void*>(&craft_class_constructor_hook),
                kExpectedCraftClassConstructor.size(),
                kExpectedCraftClassConstructor.data(),
                &g_craft_class_constructor_hook)) {
            return false;
        }
        g_craft_class_constructor_original =
            g_craft_class_constructor_hook.gateway;
        g_subsystem_classes_use_identity_observer = false;
        return g_craft_class_constructor_original != nullptr;
    }

    std::size_t patch_length = 0;
    void* destination = existing_detour_destination(site, &patch_length);
    if (destination != at(
            g_fleet_ops, kFoCraftClassConstructorHandlerRva) ||
        patch_length < 5 || patch_length > 6 ||
        !signature_matches(
            g_fleet_ops, kFoCraftClassConstructorHandlerRva,
            kExpectedFoCraftClassConstructorHandler)) {
        return false;
    }

    std::array<std::uint8_t, 6> expected{};
    std::memcpy(expected.data(), site, patch_length);
    if (!api->patch_jump(
            site, reinterpret_cast<void*>(&craft_class_constructor_hook),
            expected.data(), patch_length)) {
        return false;
    }
    g_craft_class_constructor_original = destination;
    g_subsystem_classes_use_identity_observer = false;
    return true;
}

void erase_race_node_name(void* race) {
    if (g_race_node_names.erase(race) != 0) {
        rebuild_known_race_node_names();
    }
}

void set_race_node_name(void* race, const std::string& name) {
    const auto current = g_race_node_names.find(race);
    if (current != g_race_node_names.end() && current->second == name) return;
    g_race_node_names[race] = name;
    rebuild_known_race_node_names();
}

}  // namespace

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOTextureVariants_RegisterClass(
    void* object_class, void* parameter_db) {
    register_class_damage_visuals(object_class, parameter_db);
}

extern "C" int __cdecl a2fo_texture_variants_borg_filename_hook(
    void* generated_filename, const char* texture_name) noexcept {
    if (!g_generate_texture_filename) return 1;
    const int native_result =
        g_generate_texture_filename(generated_filename, texture_name);
    if (native_result == 0 || !g_runtime_alive || !g_borg_dds_enabled ||
        !texture_name) {
        return native_result;
    }
    try {
        return texture_exists(texture_name, ".dds") ? 0 : native_result;
    } catch (...) {
        return native_result;
    }
}

void apply_race_rendering_policy(
    void* race, bool name_found, const std::string& raw_name,
    bool suffix_found, const std::string& raw_suffix) {
        std::string node_name;
        if (name_found &&
            normalize_faction_node_name(raw_name, &node_name)) {
            set_race_node_name(race, node_name);
            char message[192]{};
            std::snprintf(message, sizeof(message),
                          "Registered faction model node '%s' on Race %p",
                          node_name.c_str(), race);
            log_line(message);
        } else {
            erase_race_node_name(race);
            if (name_found) {
                char message[224]{};
                std::snprintf(
                    message, sizeof(message),
                    "Rejected Race name '%s' for faction model nodes on "
                    "Race %p; use up to 63 ASCII letters, digits, '_' or '-'",
                    raw_name.c_str(), race);
                log_line(message);
            }
        }

        if (!suffix_found) {
            g_race_suffixes.erase(race);
            return;
        }

        std::string suffix;
        if (!normalize_faction_suffix(raw_suffix, &suffix)) {
            char message[192]{};
            std::snprintf(
                message, sizeof(message),
                "Rejected invalid factionTextureSuffix on Race %p; use up "
                "to 32 ASCII letters, digits, '_' or '-'",
                race);
            log_line(message);
            g_race_suffixes.erase(race);
            return;
        }
        if (suffix.empty()) {
            g_race_suffixes.erase(race);
            return;
        }
        g_race_suffixes[race] = suffix;
        char message[192]{};
        std::snprintf(message, sizeof(message),
                      "Registered factionTextureSuffix '%s' on Race %p",
                      suffix.c_str(), race);
        log_line(message);
}

bool race_event_field(const A2FO_OdfFieldView* fields,
                      std::uint32_t count, const char* name,
                      std::string* value) {
    if (!fields || !name || !value) return false;
    const std::size_t name_size = std::strlen(name);
    for (std::uint32_t index = 0; index < count; ++index) {
        const A2FO_OdfFieldView& field = fields[index];
        if (field.name.size != name_size || !field.name.data ||
            _strnicmp(field.name.data, name, name_size) != 0 ||
            (!field.value.data && field.value.size != 0)) {
            continue;
        }
        value->assign(field.value.data ? field.value.data : "",
                      field.value.size);
        return true;
    }
    return false;
}

void A2FO_CALL race_loaded_handler(
    const A2FO_RaceLoadedEvent* event, void*) {
    if (!g_runtime_alive || !g_capture_enabled || !event ||
        event->struct_size < sizeof(*event) || !event->race) {
        return;
    }
    try {
        std::string raw_name;
        std::string raw_suffix;
        const bool name_found = race_event_field(
            event->odf_fields, event->odf_field_count,
            kFactionNameCommand, &raw_name);
        const bool suffix_found = race_event_field(
            event->odf_fields, event->odf_field_count,
            kFactionSuffixCommand, &raw_suffix);
        apply_race_rendering_policy(
            event->race, name_found, raw_name, suffix_found, raw_suffix);
    } catch (...) {
        log_line("Could not retain faction Race rendering policies");
    }
}

void A2FO_CALL craft_cleanup_event_handler(
    const A2FO_CraftEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) || !event->craft ||
        event->kind != A2FO_CRAFT_EVENT_CLEANUP) {
        return;
    }
    for (auto entry = g_craft_races.begin(); entry != g_craft_races.end();) {
        if (entry->second.object == event->craft) {
            entry = g_craft_races.erase(entry);
        } else {
            ++entry;
        }
    }
    for (auto entry = g_craft_damage_visuals.begin();
         entry != g_craft_damage_visuals.end();) {
        if (entry->second.object == event->craft) {
            entry = g_craft_damage_visuals.erase(entry);
        } else {
            ++entry;
        }
    }
}

extern "C" void __cdecl a2fo_texture_variants_apply_render_textures(
    void* craft_instance, void* object_class,
    std::uint32_t use_borg_textures) noexcept {
    if (object_class) {
        try {
            if (g_faction_variants_enabled && !g_race_suffixes.empty()) {
                collect_class_meshes(object_class);
            }
            if (g_faction_variants_enabled &&
                !g_known_race_node_names.empty()) {
                collect_class_faction_nodes(object_class);
            }
            if (g_subsystem_visuals_enabled &&
                !g_class_damage_visuals.empty()) {
                resolve_class_damage_nodes(object_class);
            }
        } catch (...) {
            log_line("Could not enumerate a CraftClass model hierarchy");
        }
    }

    // Always preserve the original call, even after module shutdown or a
    // partial setup. This includes Fleet Operations' in-function Jan_B patch.
    if (g_set_borg_textures && object_class) {
        a2fo_texture_variants_call_thiscall_1(
            g_set_borg_textures, object_class,
            static_cast<std::uintptr_t>(
                (use_borg_textures & 0xffu) != 0));
    }
    apply_faction_textures(craft_instance, object_class);
    apply_faction_node_visibility(craft_instance, object_class);
    // The subsystem subtree override is deliberately deferred until the
    // second checked render call. Armada applies its native damage-node and
    // Borg-node flags after this point; the later scoped override therefore
    // wins for the current craft and restores the shared SOD after drawing.
}

void __attribute__((fastcall)) craft_instance_update_hook(
    void* craft_instance, void*, void* object) noexcept {
    if (g_craft_instance_update_hook.gateway) {
        a2fo_texture_variants_call_thiscall_1(
            g_craft_instance_update_hook.gateway, craft_instance,
            reinterpret_cast<std::uintptr_t>(object));
    }
    if (!g_runtime_alive || !craft_instance) return;
    try {
        if (g_capture_enabled &&
            (!g_race_suffixes.empty() || !g_race_node_names.empty())) {
            void* race = read_at<void*>(object, kObjectRaceOffset, nullptr);
            if (race) {
                g_craft_races[craft_instance] = CraftRaceState{object, race};
            } else {
                g_craft_races.erase(craft_instance);
            }
        }
        if (!g_class_damage_visuals.empty()) {
            update_subsystem_damage_visuals(craft_instance, object);
        }
    } catch (...) {
        // The native update already completed; missing sidecar rows simply
        // leave this render on its ordinary intact/base appearance.
    }
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->patch_call || !api->patch_jump ||
        !api->install_inline_hook) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !g_fleet_ops) return false;

    g_generate_texture_filename =
        function_from_address<GenerateTextureFilename>(
            at(g_armada, kGenerateTextureFilenameRva));
    g_texture_file_exists = function_from_address<TextureFileExists>(
        at(g_armada, kTextureFileExistsRva));
    g_texture_find = function_from_address<TextureFind>(
        at(g_armada, kTextureFindRva));
    g_explosion_class_find = function_from_address<ExplosionClassFind>(
        at(g_armada, kExplosionClassFindRva));
    g_node_particle_add = function_from_address<NodeParticleAdd>(
        at(g_armada, kNodeParticleAddRva));
    g_position_particle_add = function_from_address<NodeParticleAdd>(
        at(g_armada, kPositionParticleAddRva));
    g_set_borg_textures = at(g_armada, kCraftClassSetBorgTexturesRva);
    g_get_mesh_texture = at(g_armada, kMeshGetTextureRva);
    g_set_mesh_texture = at(g_armada, kMeshSetTextureRva);
    g_runtime_alive = true;

    bool cleanup_registered = false;
    if ((api->capabilities & A2FO_CAP_CRAFT_EVENTS) != 0 &&
        A2FO_MODULE_API_HAS(
            api, register_craft_event_handler_masked) &&
        api->register_craft_event_handler_masked) {
        cleanup_registered = api->register_craft_event_handler_masked(
            kModuleName, A2FO_CRAFT_EVENT_MASK_CLEANUP,
            &craft_cleanup_event_handler, nullptr);
    } else if (A2FO_MODULE_API_HAS(api, register_craft_event_handler) &&
               (api->capabilities & A2FO_CAP_CRAFT_EVENTS) != 0 &&
               api->register_craft_event_handler) {
        cleanup_registered = api->register_craft_event_handler(
            kModuleName, &craft_cleanup_event_handler, nullptr);
    }
    if (!cleanup_registered) {
        log_line("Craft cleanup dispatch unavailable; render sidecars use "
                 "pointer-reuse replacement only");
    }

    if (install_borg_dds_fix(api)) {
        log_line("Native Borg alternate preflight now accepts DDS as well as TGA");
    } else {
        log_line("Borg DDS preflight hook unavailable; native behaviour retained");
    }
    if (install_faction_variants(api)) {
        log_line(
            "Ownership-aware faction textures and Race-name model nodes "
            "initialized");
    } else {
        log_line(
            "Faction render hooks unavailable; native base/Borg rendering "
            "retained");
    }
    if (subsystem_visual_signatures_supported() &&
        install_subsystem_class_registration(api)) {
        g_subsystem_visuals_enabled = true;
        log_line(
            "ODF-driven subsystem damage meshes, explosions, and repair "
            "sparks initialized");
        log_line(g_subsystem_classes_use_identity_observer
            ? "Subsystem CraftClass policies use A2FOCraftIdentity's "
              "cooperative observer"
            : "Subsystem CraftClass policies use the validated fallback "
              "constructor chain");
    } else {
        log_line(
            "Subsystem damage visuals unavailable; configured meshes remain "
            "intact");
    }

    // Hooks are process-lifetime patches. Keep a partially initialized module
    // resident so every installed route remains a safe native pass-through.
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    g_subsystem_visuals_enabled = false;
    g_faction_variants_enabled = false;
    g_borg_dds_enabled = false;
    g_capture_enabled = false;
    g_runtime_alive = false;
    g_craft_races.clear();
    g_craft_damage_visuals.clear();
    g_race_suffixes.clear();
    g_race_node_names.clear();
    g_known_race_node_names.clear();
    g_class_meshes.clear();
    g_class_faction_nodes.clear();
    g_variant_textures.clear();
    g_class_damage_visuals.clear();
}
