/*
 * File: modules/A2FOFeaturePack/upgrade_pods.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Upgradeable ship-system pod parser and runtime progression engine with safe extended-tier sidecars.
 */

#include "upgrade_pods.hpp"

#include "hybrid_bridge_client.hpp"
#include "upgrade_pod_config.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace a2fo {
namespace {

constexpr const char* kModuleName = "A2FOFeaturePack";
constexpr std::uint32_t kNativeMaximumTier = 3;
constexpr std::uint32_t kDefaultMaximumTier = 6;
constexpr std::uint32_t kHardMaximumTier = 16;
constexpr const char* kRtsConfigFileName = "RTS_CFG.h";
constexpr std::streamoff kMaximumRtsConfigSize = 2 * 1024 * 1024;
constexpr std::uint32_t kResearchPodClassTag = 0x52535250u;
constexpr std::size_t kShipUpgradeSystemCount = 5;

// ArmadaL.exe RVAs from the supported Armada 1.1 symbol map/PDB.
constexpr std::uintptr_t kResearchPodDetachRva = 0x0b95a0;
constexpr std::uintptr_t kResearchPodAttachRva = 0x0b95f0;
constexpr std::uintptr_t kResearchStationConstructorRva = 0x0b99b0;
constexpr std::uintptr_t kResearchStationDestructorRva = 0x0b9b50;
constexpr std::uintptr_t kTeamManagerForTeamRva = 0x096340;
constexpr std::uintptr_t kTeamSetUpgradeMultiplierRva = 0x0987d0;
constexpr std::uintptr_t kGameObjectClassFindRva = 0x0cd370;
constexpr std::uintptr_t kGameObjectClassFindProjectIdRva = 0x0cd1f0;
constexpr std::uintptr_t kParameterDbGetProjectIdRva = 0x135200;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x135350;

// FleetOpsHook.dll map offsets include a 0x1000 .text RVA. Fleet Ops already
// owns these callbacks, so A2FO chains them rather than patching Armada's
// original class parsers a second time.
constexpr std::uintptr_t kFoResearchPodClassCallbackRva = 0x10c5e4;
constexpr std::uintptr_t kFoResearchPodClassDtorRva = 0x10c618;
constexpr std::uintptr_t kFoResearchStationClassCallbackRva = 0x1e3e00;
constexpr std::uintptr_t kFoResearchStationCanBuildRva = 0x1e3ea0;
constexpr std::uintptr_t kFoResearchPodSameTypeRva = 0x1fcffc;

constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kObjectTeamOffset = 0xec;
constexpr std::size_t kResearchPodStationOffset = 0x250;
constexpr std::size_t kClassTagOffset = 0x70;
constexpr std::size_t kPodIsUpgradeOffset = 0x450;
constexpr std::size_t kPodUpgradeTierOffset = 0x454;
constexpr std::size_t kPodUpgradeSystemOffset = 0x458;
constexpr std::size_t kPodUpgradeMultiplierOffset = 0x45c;
constexpr std::size_t kProducerBuildItemsOffset = 0x450;
constexpr std::size_t kStationSecondaryBuildItemsOffset = 0x494;
constexpr std::size_t kStationSecondaryFlagsOffset = 0x4cc;
constexpr std::size_t kStationInstanceSecondaryItemsOffset = 0x2c0;
constexpr std::size_t kStationInstancePodCountOffset = 0x2ac;
constexpr std::size_t kStationInstancePodsOffset = 0x2b0;
constexpr std::size_t kProducerBuildItemCapacity = 57;
constexpr std::size_t kSecondaryBuildItemCapacity = 58;
constexpr std::size_t kStationTierListCount = kHardMaximumTier - 1;

const std::uint8_t kExpectedResearchPodDetach[] =
    {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7e, 0x40};
const std::uint8_t kExpectedResearchPodAttach[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x53, 0x56};
const std::uint8_t kExpectedResearchStationConstructor[] =
    {0x55, 0x8b, 0xec, 0x56, 0x57, 0x8b, 0x7d, 0x08};
const std::uint8_t kExpectedResearchStationDestructor[] =
    {0x55, 0x8b, 0xec, 0x6a, 0xff};
const std::uint8_t kExpectedResearchPodClassCallback[] =
    {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf8, 0x53};
const std::uint8_t kExpectedResearchPodClassDtor[] =
    {0x55, 0x8b, 0xec, 0x51, 0x53};
const std::uint8_t kExpectedResearchStationClassCallback[] =
    {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf8, 0x53};
const std::uint8_t kExpectedResearchStationCanBuild[] =
    {0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x8b, 0x75, 0x08};
const std::uint8_t kExpectedResearchPodSameType[] =
    {0x55, 0x8b, 0xec, 0x51, 0x53};
const std::uint8_t kExpectedTeamManagerForTeam[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
const std::uint8_t kExpectedTeamSetUpgradeMultiplier[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x55, 0x08, 0x8b, 0x45, 0x0c};
const std::uint8_t kExpectedGameObjectClassFind[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
const std::uint8_t kExpectedGameObjectClassFindProjectId[] =
    {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68};
const std::uint8_t kExpectedParameterDbGetProjectId[] =
    {0x55, 0x8b, 0xec, 0x81, 0xec, 0x40, 0x01, 0x00, 0x00};

extern "C" std::uintptr_t a2fo_call_thiscall_0(
    void* function, void* self);
extern "C" std::uintptr_t a2fo_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument);
extern "C" std::uintptr_t a2fo_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
extern "C" std::uintptr_t a2fo_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
extern "C" std::uintptr_t a2fo_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);

using FindClassFunction = void* (__cdecl*)(const char* name);
using FindClassByProjectIdFunction = void* (__cdecl*)(
    const std::uint32_t* project_id);
using TeamManagerFunction = void* (__cdecl*)(std::uint32_t team);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
CRITICAL_SECTION g_upgrade_lock;
bool g_upgrade_lock_ready = false;
bool g_upgrade_hooks_ready = false;
volatile LONG g_extended_compare_log_count = 0;
volatile LONG g_chain_bridge_log_count = 0;
volatile LONG g_station_progression_log_count = 0;
std::uint32_t g_configured_maximum_tier = kDefaultMaximumTier;

A2FO_InlineHook g_pod_class_hook{};
A2FO_InlineHook g_pod_class_dtor_hook{};
A2FO_InlineHook g_station_class_hook{};
A2FO_InlineHook g_station_can_build_hook{};
A2FO_InlineHook g_same_type_hook{};
A2FO_InlineHook g_pod_attach_hook{};
A2FO_InlineHook g_pod_detach_hook{};
A2FO_InlineHook g_station_constructor_hook{};
A2FO_InlineHook g_station_destructor_hook{};

struct PodInstanceState {
    void* manager = nullptr;
    std::uint32_t system = 0;
    std::uint32_t tier = kNativeMaximumTier;
    float multiplier = 1.0f;
};

struct StationTierList {
    bool supplied = false;
    std::array<bool, kSecondaryBuildItemCapacity> present{};
    std::array<void*, kSecondaryBuildItemCapacity> items{};
};

struct StationClassState {
    std::array<void*, kSecondaryBuildItemCapacity> legacy_secondary{};
    std::array<StationTierList, kStationTierListCount> tiers{};
    bool has_extended_tiers = false;
};

struct StationInstanceState {
    void* station_class = nullptr;
    std::array<void*, kSecondaryBuildItemCapacity> base_secondary{};
    std::array<void*, kSecondaryBuildItemCapacity> active_secondary{};
};

std::unordered_map<void*, std::uint32_t> g_declared_tiers;
std::unordered_map<void*, PodInstanceState> g_pod_instances;
std::unordered_map<void*, StationClassState> g_station_classes;
std::unordered_map<void*, std::unique_ptr<StationInstanceState>>
    g_station_instances;

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) {
    return reinterpret_cast<T*>(
        reinterpret_cast<std::uint8_t*>(module) + rva);
}

std::uint8_t* bytes(void* value) {
    return static_cast<std::uint8_t*>(value);
}

bool readable_range(const void* pointer, std::size_t size) noexcept {
    return pointer && size != 0 && !IsBadReadPtr(pointer, size);
}

bool writable_range(void* pointer, std::size_t size) noexcept {
    return pointer && size != 0 && !IsBadWritePtr(pointer, size);
}

bool read_class_tag(void* object_class, std::uint32_t& tag) noexcept {
    if (!object_class) return false;
    const void* address = bytes(object_class) + kClassTagOffset;
    if (!readable_range(address, sizeof(tag))) return false;
    tag = *static_cast<const std::uint32_t*>(address);
    return true;
}

void log_message(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

std::uint32_t configured_maximum_tier() noexcept {
    return g_configured_maximum_tier;
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

bool read_small_text_file(const std::string& path, std::string& contents) {
    contents.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || size > kMaximumRtsConfigSize) {
        log_message("Ignored oversized RTS configuration: " + path);
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::ostringstream stream;
    stream << input.rdbuf();
    contents = stream.str();
    return input.good() || input.eof();
}

void load_upgrade_pod_maximum_tier() {
    g_configured_maximum_tier = kDefaultMaximumTier;
    bool configured = false;
    const std::uint32_t root_count = g_api->extension_root_count();
    if (root_count > 4096) {
        log_message(
            "Extension-root count is invalid; upgrade-pod maximum remains 6");
        return;
    }
    for (std::uint32_t index = 0; index < root_count; ++index) {
        const char* root = g_api->extension_root(index);
        if (!root || !*root) continue;
        const std::string path = join_path(root, kRtsConfigFileName);
        std::string contents;
        if (!read_small_text_file(path, contents)) continue;

        std::uint32_t candidate = g_configured_maximum_tier;
        const auto status = a2fo::upgrade_pods::parse_maximum_tier_setting(
            contents, &candidate);
        if (status ==
            a2fo::upgrade_pods::MaximumTierSettingStatus::valid) {
            g_configured_maximum_tier = candidate;
            configured = true;
            log_message(
                "Applied upgradePodMaximumTier=" +
                std::to_string(g_configured_maximum_tier) + " from " + path);
        } else if (status ==
                   a2fo::upgrade_pods::MaximumTierSettingStatus::invalid) {
            log_message(
                "Ignored invalid upgradePodMaximumTier in " + path +
                " (valid range 3-16)");
        }
    }
    log_message(
        "RTS_CFG.h upgrade-pod maximum: " +
        std::to_string(g_configured_maximum_tier) +
        (configured ? "" : " (default)"));
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) {
    return module &&
        std::memcmp(at(module, rva), expected, Size) == 0;
}

std::uint32_t raw_class_tier(void* object_class) noexcept {
    if (!object_class) return 0;
    const void* address = bytes(object_class) + kPodUpgradeTierOffset;
    return readable_range(address, sizeof(std::uint32_t))
        ? *static_cast<const std::uint32_t*>(address) : 0;
}

std::uint32_t declared_class_tier(void* object_class) noexcept {
    if (!object_class) return 0;
    if (g_upgrade_lock_ready) EnterCriticalSection(&g_upgrade_lock);
    const auto found = g_declared_tiers.find(object_class);
    const std::uint32_t tier = found != g_declared_tiers.end()
        ? found->second : raw_class_tier(object_class);
    if (g_upgrade_lock_ready) LeaveCriticalSection(&g_upgrade_lock);
    return tier;
}

bool is_research_upgrade_pod(void* object_class) noexcept {
    std::uint32_t tag = 0;
    if (!read_class_tag(object_class, tag) ||
        tag != kResearchPodClassTag) {
        return false;
    }
    const void* is_upgrade = bytes(object_class) + kPodIsUpgradeOffset;
    return readable_range(is_upgrade, sizeof(std::uint8_t)) &&
           *static_cast<const std::uint8_t*>(is_upgrade) != 0;
}

void set_effective_multiplier(void* manager, std::uint32_t system,
                              float multiplier) noexcept {
    if (!manager || system > 4) return;
    std::uint32_t bits = 0;
    std::memcpy(&bits, &multiplier, sizeof(bits));
    a2fo_call_thiscall_3(
        at(g_armada, kTeamSetUpgradeMultiplierRva), manager,
        kNativeMaximumTier, system, bits);
}

void recompute_multiplier(void* manager, std::uint32_t system) noexcept {
    if (!manager || system > 4 || !g_upgrade_lock_ready) return;
    PodInstanceState selected;
    bool have_selected = false;
    EnterCriticalSection(&g_upgrade_lock);
    for (const auto& item : g_pod_instances) {
        const PodInstanceState& candidate = item.second;
        if (candidate.manager != manager || candidate.system != system) {
            continue;
        }
        if (!have_selected || candidate.tier > selected.tier) {
            selected = candidate;
            have_selected = true;
        }
    }
    LeaveCriticalSection(&g_upgrade_lock);
    if (have_selected) {
        set_effective_multiplier(manager, system, selected.multiplier);
    }
}

void remember_attached_pod(void* pod) noexcept {
    if (!pod || !g_upgrade_lock_ready) return;
    void* object_class = *reinterpret_cast<void**>(
        bytes(pod) + kObjectClassOffset);
    if (!is_research_upgrade_pod(object_class)) return;
    const std::uint32_t tier = declared_class_tier(object_class);
    if (tier < kNativeMaximumTier) return;
    const std::uint32_t system = *reinterpret_cast<const std::uint32_t*>(
        bytes(object_class) + kPodUpgradeSystemOffset);
    if (system > 4) return;
    const std::uint32_t team = *reinterpret_cast<const std::uint32_t*>(
        bytes(pod) + kObjectTeamOffset);
    auto team_manager = reinterpret_cast<TeamManagerFunction>(
        at(g_armada, kTeamManagerForTeamRva));
    void* manager = team_manager(team);
    if (!manager) return;

    PodInstanceState state;
    state.manager = manager;
    state.system = system;
    state.tier = tier;
    state.multiplier = *reinterpret_cast<const float*>(
        bytes(object_class) + kPodUpgradeMultiplierOffset);
    EnterCriticalSection(&g_upgrade_lock);
    g_pod_instances[pod] = state;
    LeaveCriticalSection(&g_upgrade_lock);
    if (tier > kNativeMaximumTier) {
        char message[160];
        std::snprintf(message, sizeof(message),
                      "Extended upgrade pod attached: upgradeLevel %lu, system %lu, multiplier %.3f",
                      static_cast<unsigned long>(tier),
                      static_cast<unsigned long>(system),
                      static_cast<double>(state.multiplier));
        log_message(message);
    }
    recompute_multiplier(manager, system);
}

std::string trim_odf_name(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
    const std::size_t slash = value.find_last_of("\\/");
    if (slash != std::string::npos) value.erase(0, slash + 1);
    if (value.size() > 4 &&
        _stricmp(value.c_str() + value.size() - 4, ".odf") == 0) {
        value.resize(value.size() - 4);
    }
    return value;
}

bool read_parameter_string(void* parameter_db, const std::string& key,
                           std::string& value) noexcept {
    std::array<char, 512> output{};
    const std::uintptr_t found = a2fo_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key.c_str()),
        reinterpret_cast<std::uintptr_t>(output.data()),
        static_cast<std::uintptr_t>(output.size()),
        reinterpret_cast<std::uintptr_t>(""));
    output.back() = '\0';
    value.assign(output.data());
    return (found & 0xffu) != 0;
}

bool read_parameter_project_id(void* parameter_db, const std::string& key,
                               std::uint32_t& project_id) noexcept {
    project_id = 0;
    const std::uintptr_t found = a2fo_call_thiscall_3(
        at(g_armada, kParameterDbGetProjectIdRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key.c_str()),
        reinterpret_cast<std::uintptr_t>(&project_id), 0);
    return (found & 0xffu) != 0;
}

bool apply_indexed_tier_items(void** items, std::uint8_t* flags,
                              std::size_t capacity,
                              const StationTierList& tier_items) {
    if (!items || capacity == 0 ||
        !writable_range(items, capacity * sizeof(void*))) {
        log_message("Upgrade station build-list storage is unavailable; "
                    "existing list preserved");
        return false;
    }
    if (flags && !writable_range(flags, capacity)) {
        log_message("Upgrade station build-list flags are unavailable; "
                    "existing list preserved");
        return false;
    }
    for (std::size_t index = 0; index < capacity; ++index) {
        if (!tier_items.present[index]) continue;
        items[index] = tier_items.items[index];
        if (flags) flags[index] = 0;
    }
    return true;
}

void load_tier_build_items(void* station_class,
                           void* parameter_db) noexcept {
    if (!station_class || !parameter_db) return;
    if (!readable_range(
            station_class,
            kStationSecondaryFlagsOffset + sizeof(void*))) {
        log_message("Upgrade station class storage is unavailable; "
                    "tier build lists were not changed");
        return;
    }
    // ProducerClassEnhancement publishes its 57-slot command-panel list at
    // class+0x450.  Its allocation is exactly 0xe8 bytes, with the last bytes
    // reserved for Producer state, so iteration must stop at index 56.
    //
    // Do not mirror these pointers into ResearchStationClassEnhancement's
    // allocation at class+0x454.  Extension+0x04 is Fleet Ops' 58-entry
    // providedBuildItem state table, not a GameObjectClass list; overwriting
    // it makes pod completion and prerequisite bookkeeping fail.
    void** producer_primary = *reinterpret_cast<void***>(
        bytes(station_class) + kProducerBuildItemsOffset);
    void** secondary = *reinterpret_cast<void***>(
        bytes(station_class) + kStationSecondaryBuildItemsOffset);
    std::uint8_t* secondary_flags = *reinterpret_cast<std::uint8_t**>(
        bytes(station_class) + kStationSecondaryFlagsOffset);
    auto find_class = reinterpret_cast<FindClassFunction>(
        at(g_armada, kGameObjectClassFindRva));
    auto find_class_by_project_id =
        reinterpret_cast<FindClassByProjectIdFunction>(
            at(g_armada, kGameObjectClassFindProjectIdRva));

    StationClassState station_state;

    const std::uint32_t maximum_upgrade_level = configured_maximum_tier();
    // Station tier commands are zero-based additions beyond the vessel's
    // built-in level-1 systems: tier0 -> upgradeLevel 2, tier1 -> level 3,
    // tier2 -> level 4, and so on.
    for (std::uint32_t command_tier = 0;
         command_tier + 2 <= maximum_upgrade_level; ++command_tier) {
        const std::uint32_t upgrade_level = command_tier + 2;
        const std::size_t capacity = upgrade_level == 2
            // The ResearchStation mirror has one more slot, but the Producer
            // list that can actually publish buttons stops at index 56.
            ? kProducerBuildItemCapacity
            : kSecondaryBuildItemCapacity;
        bool supplied = false;
        std::vector<void*> replacements;
        StationTierList tier_items;
        for (std::size_t index = 0; index < capacity; ++index) {
            char key[48];
            std::snprintf(key, sizeof(key), "tier%luBuildItem%lu",
                          static_cast<unsigned long>(command_tier),
                          static_cast<unsigned long>(index));
            std::string value;
            if (!read_parameter_string(parameter_db, key, value)) continue;
            supplied = true;
            const std::string name = trim_odf_name(std::move(value));
            if (name.empty()) {
                tier_items.present[index] = true;
                continue;
            }
            // Use the same ParameterDB -> cPrjID -> GameObjectClass path as
            // Armada's native buildItem/secondaryBuildItem parser. Fleet Ops
            // resolves the cPrjID through its live FOFS project array; the
            // convenience name overload does not reliably lazy-load newly
            // registered recursive ODF basenames.
            std::uint32_t project_id = 0;
            const bool have_project_id = read_parameter_project_id(
                parameter_db, key, project_id);
            void* item_class = have_project_id && project_id != 0
                ? find_class_by_project_id(&project_id) : nullptr;
            if (!item_class) item_class = find_class(name.c_str());
            if (!item_class) {
                log_message(std::string(key) + " ignored: " + name +
                            " could not be loaded (project ID " +
                            std::to_string(project_id) + ")");
                continue;
            }
            if (!is_research_upgrade_pod(item_class)) {
                log_message(std::string(key) + " ignored: " + name +
                            " is not a ResearchPod ship upgrade");
                continue;
            }
            if (declared_class_tier(item_class) != upgrade_level) {
                log_message(std::string(key) + " ignored: " + name +
                            " has a different upgradeLevel");
                continue;
            }
            tier_items.present[index] = true;
            tier_items.items[index] = item_class;
            if (std::find(replacements.begin(), replacements.end(),
                          item_class) == replacements.end()) {
                replacements.push_back(item_class);
            }
        }
        if (!supplied) continue;
        const bool has_indexed_change = std::find(
            tier_items.present.begin(), tier_items.present.end(), true) !=
            tier_items.present.end();
        if (!has_indexed_change) {
            log_message("Upgrade station tier " +
                        std::to_string(command_tier) +
                        " had no valid items; existing list preserved");
            continue;
        }
        tier_items.supplied = supplied;
        bool replaced = false;
        if (upgrade_level == 2) {
            replaced = apply_indexed_tier_items(
                producer_primary, nullptr, kProducerBuildItemCapacity,
                tier_items);
        } else if (upgrade_level == 3) {
            replaced = apply_indexed_tier_items(
                secondary, secondary_flags, kSecondaryBuildItemCapacity,
                tier_items);
        } else {
            // Higher tiers are kept in the sidecar. Publishing them in the
            // shared Fleet Ops secondary table either leaves the level-3
            // item in front of them or moves unrelated research hardpoints.
            replaced = true;
        }
        if (!replaced) continue;
        if (command_tier < station_state.tiers.size()) {
            station_state.tiers[command_tier] = tier_items;
            if (command_tier >= 2 && !replacements.empty()) {
                station_state.has_extended_tiers = true;
            }
        }
        char message[144];
        std::snprintf(message, sizeof(message),
                      "Upgrade station tier %lu (upgradeLevel %lu) build list: %lu item%s",
                      static_cast<unsigned long>(command_tier),
                      static_cast<unsigned long>(upgrade_level),
                      static_cast<unsigned long>(replacements.size()),
                      replacements.size() == 1 ? "" : "s");
        log_message(message);
    }
    if (station_state.has_extended_tiers) {
        if (!secondary || !readable_range(
                secondary,
                kSecondaryBuildItemCapacity * sizeof(void*))) {
            log_message("Upgrade station secondary table is unavailable; "
                        "extended tier progression was not registered");
            return;
        }
        std::copy_n(secondary, kSecondaryBuildItemCapacity,
                    station_state.legacy_secondary.begin());
        EnterCriticalSection(&g_upgrade_lock);
        g_station_classes[station_class] = std::move(station_state);
        LeaveCriticalSection(&g_upgrade_lock);
        log_message("Upgrade station per-hardpoint tier progression registered");
    }
}

void initialize_station_progression(void* station,
                                    void* station_class) noexcept {
    if (!station || !g_upgrade_lock_ready ||
        !writable_range(bytes(station) +
                            kStationInstanceSecondaryItemsOffset,
                        sizeof(void*)) || !station_class) {
        return;
    }

    auto state = std::make_unique<StationInstanceState>();
    state->station_class = station_class;
    void** active_items = nullptr;
    EnterCriticalSection(&g_upgrade_lock);
    const auto class_found = g_station_classes.find(station_class);
    if (class_found != g_station_classes.end()) {
        const StationClassState& class_state = class_found->second;
        state->base_secondary = class_state.legacy_secondary;
        state->active_secondary = state->base_secondary;
        active_items = state->active_secondary.data();
        g_station_instances[station] = std::move(state);
    }
    LeaveCriticalSection(&g_upgrade_lock);
    if (!active_items) return;

    *reinterpret_cast<void***>(
        bytes(station) + kStationInstanceSecondaryItemsOffset) =
        active_items;
    log_message("Upgrade station instance received a per-hardpoint tier list");
}

void refresh_station_progression(void* station) noexcept {
    if (!station || !g_upgrade_lock_ready) return;
    EnterCriticalSection(&g_upgrade_lock);
    const bool tracked = g_station_instances.find(station) !=
                         g_station_instances.end();
    LeaveCriticalSection(&g_upgrade_lock);
    if (!tracked ||
        !readable_range(bytes(station) + kStationInstancePodsOffset,
                        sizeof(void*)) ||
        !readable_range(bytes(station) + kStationInstancePodCountOffset,
                        sizeof(std::int32_t))) {
        return;
    }

    const std::int32_t raw_count = *reinterpret_cast<const std::int32_t*>(
        bytes(station) + kStationInstancePodCountOffset);
    if (raw_count < 0) return;
    const std::size_t count = std::min<std::size_t>(
        static_cast<std::size_t>(raw_count),
        kSecondaryBuildItemCapacity);
    void** pods = *reinterpret_cast<void***>(
        bytes(station) + kStationInstancePodsOffset);
    if (count != 0 &&
        (!pods || !readable_range(pods, count * sizeof(void*)))) {
        return;
    }

    // ResearchStation's pod array is populated in construction order. It is
    // not a stable hardpoint/build-item index: building shields first can put
    // the shield pod at array index zero. Track the highest attached pod by
    // its declared upgradeSystem instead.
    std::array<void*, kShipUpgradeSystemCount> attached_classes{};
    std::array<std::uint32_t, kShipUpgradeSystemCount> attached_tiers{};
    for (std::size_t index = 0; index < count; ++index) {
        void* pod = pods[index];
        if (!pod || !readable_range(
                bytes(pod) + kObjectClassOffset, sizeof(void*))) {
            continue;
        }
        void* pod_class = *reinterpret_cast<void**>(
            bytes(pod) + kObjectClassOffset);
        if (!is_research_upgrade_pod(pod_class)) continue;
        const std::uint32_t system =
            *reinterpret_cast<const std::uint32_t*>(
                bytes(pod_class) + kPodUpgradeSystemOffset);
        if (system >= kShipUpgradeSystemCount) continue;
        const std::uint32_t tier = declared_class_tier(pod_class);
        if (!attached_classes[system] || tier > attached_tiers[system]) {
            attached_classes[system] = pod_class;
            attached_tiers[system] = tier;
        }
    }

    std::size_t logged_slot = kSecondaryBuildItemCapacity;
    std::uint32_t logged_from = 0;
    std::uint32_t logged_to = 0;
    EnterCriticalSection(&g_upgrade_lock);
    const auto instance_found = g_station_instances.find(station);
    if (instance_found != g_station_instances.end()) {
        StationInstanceState& instance = *instance_found->second;
        const auto class_found = g_station_classes.find(
            instance.station_class);
        if (class_found != g_station_classes.end()) {
            const StationClassState& class_state = class_found->second;
            for (std::size_t slot = 0;
                 slot < kSecondaryBuildItemCapacity; ++slot) {
                void* candidate = instance.base_secondary[slot];
                std::uint32_t system = kShipUpgradeSystemCount;
                if (is_research_upgrade_pod(candidate)) {
                    system = *reinterpret_cast<const std::uint32_t*>(
                        bytes(candidate) + kPodUpgradeSystemOffset);
                }
                if (system >= kShipUpgradeSystemCount) {
                    for (const StationTierList& tier_list :
                         class_state.tiers) {
                        void* configured = tier_list.present[slot]
                            ? tier_list.items[slot] : nullptr;
                        if (!is_research_upgrade_pod(configured)) continue;
                        system = *reinterpret_cast<const std::uint32_t*>(
                            bytes(configured) + kPodUpgradeSystemOffset);
                        break;
                    }
                }
                const std::uint32_t attached_tier =
                    system < kShipUpgradeSystemCount
                    ? attached_tiers[system] : 0;
                if (attached_tier >= 2) {
                    if (attached_tier >= 3) candidate = nullptr;
                    const std::size_t next_command_tier =
                        static_cast<std::size_t>(attached_tier - 1);
                    if (next_command_tier < class_state.tiers.size()) {
                        const StationTierList& next =
                            class_state.tiers[next_command_tier];
                        if (next.present[slot]) candidate = next.items[slot];
                    }
                    if (!candidate && attached_tier >= 3) {
                        candidate = attached_classes[system];
                    }
                }
                instance.active_secondary[slot] = candidate;

                std::uint32_t candidate_tier = 0;
                const auto tier_found = g_declared_tiers.find(candidate);
                if (tier_found != g_declared_tiers.end()) {
                    candidate_tier = tier_found->second;
                } else if (candidate) {
                    candidate_tier = raw_class_tier(candidate);
                }
                if (candidate_tier > kNativeMaximumTier &&
                    logged_slot == kSecondaryBuildItemCapacity) {
                    logged_slot = slot;
                    logged_from = attached_tier;
                    logged_to = candidate_tier;
                }
            }
            *reinterpret_cast<void***>(
                bytes(station) +
                kStationInstanceSecondaryItemsOffset) =
                instance.active_secondary.data();
        }
    }
    LeaveCriticalSection(&g_upgrade_lock);

    if (logged_slot != kSecondaryBuildItemCapacity) {
        const LONG log_number = InterlockedIncrement(
            &g_station_progression_log_count);
        if (log_number <= 12) {
            char message[176];
            std::snprintf(
                message, sizeof(message),
                "Upgrade station slot %lu advanced from level %lu to level %lu",
                static_cast<unsigned long>(logged_slot),
                static_cast<unsigned long>(logged_from),
                static_cast<unsigned long>(logged_to));
            log_message(message);
        }
    }
}

bool pod_belongs_to_extended_station(void* pod) noexcept {
    if (!pod || !g_upgrade_lock_ready || !readable_range(
            bytes(pod) + kResearchPodStationOffset, sizeof(void*))) {
        return false;
    }
    void* station = *reinterpret_cast<void**>(
        bytes(pod) + kResearchPodStationOffset);
    if (!station) return false;
    EnterCriticalSection(&g_upgrade_lock);
    const bool tracked = g_station_instances.find(station) !=
                         g_station_instances.end();
    LeaveCriticalSection(&g_upgrade_lock);
    return tracked;
}

void forget_station_progression(void* station) noexcept {
    if (!station || !g_upgrade_lock_ready) return;
    EnterCriticalSection(&g_upgrade_lock);
    g_station_instances.erase(station);
    LeaveCriticalSection(&g_upgrade_lock);
}

std::uintptr_t __attribute__((fastcall)) pod_class_hook(
    void* object_class, void*, std::uintptr_t argument1,
    void* parameter_db) noexcept {
    const std::uintptr_t result = a2fo_call_thiscall_2(
        g_pod_class_hook.gateway, object_class, argument1,
        reinterpret_cast<std::uintptr_t>(parameter_db));
    if (!object_class) return result;
    try {
        const std::uint32_t requested_tier = raw_class_tier(object_class);
        const std::uint32_t maximum_tier = configured_maximum_tier();
        std::uint32_t declared_tier = requested_tier;
        if (requested_tier > maximum_tier) {
            declared_tier = kNativeMaximumTier;
            log_message("ResearchPod upgradeLevel " +
                        std::to_string(requested_tier) +
                        " exceeds configured maximum " +
                        std::to_string(maximum_tier) +
                        "; using tier 3");
        }
        if (requested_tier > kNativeMaximumTier) {
            *reinterpret_cast<std::uint32_t*>(
                bytes(object_class) + kPodUpgradeTierOffset) =
                kNativeMaximumTier;
        }
        EnterCriticalSection(&g_upgrade_lock);
        g_declared_tiers[object_class] = declared_tier;
        LeaveCriticalSection(&g_upgrade_lock);
        if (declared_tier > kNativeMaximumTier) {
            log_message("Extended ResearchPod class loaded: upgradeLevel " +
                        std::to_string(declared_tier));
        }
    } catch (...) {
        log_message("Upgrade-pod class sidecar failed; using native tier");
    }
    return result;
}

std::uintptr_t __attribute__((fastcall)) pod_class_dtor_hook(
    void* object_class, void*, std::uintptr_t flags) noexcept {
    if (object_class && g_upgrade_lock_ready) {
        EnterCriticalSection(&g_upgrade_lock);
        g_declared_tiers.erase(object_class);
        LeaveCriticalSection(&g_upgrade_lock);
    }
    return a2fo_call_thiscall_1(g_pod_class_dtor_hook.gateway,
                                object_class, flags);
}

std::uintptr_t __attribute__((fastcall)) station_class_hook(
    void* station_class, void*, std::uintptr_t argument1,
    void* parameter_db) noexcept {
    const std::uintptr_t result = a2fo_call_thiscall_2(
        g_station_class_hook.gateway, station_class, argument1,
        reinterpret_cast<std::uintptr_t>(parameter_db));
    try {
        load_tier_build_items(station_class, parameter_db);
    } catch (...) {
        log_message("Upgrade station tier build-list parsing failed");
    }
    try {
        register_research_station_hybrid_lists(
            station_class, parameter_db);
        publish_research_station_hybrid_items(station_class);
    } catch (...) {
        log_message("Hybrid ResearchStation build-list publication failed");
    }
    return result;
}

std::uintptr_t __attribute__((fastcall)) station_can_build_hook(
    void* station_class, void*, void* item_class) noexcept {
    bool extended_item = false;
    if (station_class && item_class && g_upgrade_lock_ready) {
        EnterCriticalSection(&g_upgrade_lock);
        const auto found = g_station_classes.find(station_class);
        if (found != g_station_classes.end()) {
            const StationClassState& state = found->second;
            for (std::size_t tier = 2;
                 tier < state.tiers.size() && !extended_item; ++tier) {
                const StationTierList& list = state.tiers[tier];
                for (std::size_t slot = 0;
                     slot < list.items.size(); ++slot) {
                    if (list.present[slot] &&
                        list.items[slot] == item_class) {
                        extended_item = true;
                        break;
                    }
                }
            }
        }
        LeaveCriticalSection(&g_upgrade_lock);
    }
    if (extended_item) return 1;
    return a2fo_call_thiscall_1(
        g_station_can_build_hook.gateway, station_class,
        reinterpret_cast<std::uintptr_t>(item_class));
}

std::uintptr_t __attribute__((fastcall)) station_constructor_hook(
    void* station, void*, void* station_class) noexcept {
    const std::uintptr_t result = a2fo_call_thiscall_1(
        g_station_constructor_hook.gateway, station,
        reinterpret_cast<std::uintptr_t>(station_class));
    try {
        initialize_station_progression(station, station_class);
    } catch (...) {
        log_message("Upgrade station instance tier-list setup failed");
    }
    return result;
}

std::uintptr_t __attribute__((fastcall)) station_destructor_hook(
    void* station, void*) noexcept {
    cleanup_hybrid_construction(station);
    cleanup_hybrid_cocoon(station);
    const std::uintptr_t result = a2fo_call_thiscall_0(
        g_station_destructor_hook.gateway, station);
    forget_station_progression(station);
    return result;
}

std::uintptr_t __attribute__((fastcall)) same_type_hook(
    void* pod, void*, void* other_class) noexcept {
    if (pod && other_class) {
        void* own_class = *reinterpret_cast<void**>(
            bytes(pod) + kObjectClassOffset);
        const std::uint32_t own_tier = declared_class_tier(own_class);
        const std::uint32_t other_tier = declared_class_tier(other_class);
        if (is_research_upgrade_pod(own_class) &&
            is_research_upgrade_pod(other_class)) {
            const std::uint32_t own_system =
                *reinterpret_cast<const std::uint32_t*>(
                    bytes(own_class) + kPodUpgradeSystemOffset);
            const std::uint32_t other_system =
                *reinterpret_cast<const std::uint32_t*>(
                    bytes(other_class) + kPodUpgradeSystemOffset);
            // Once the live secondary candidate advances from level 3 to
            // level 4, Armada's native two-tier state machine no longer sees
            // the attached level-3 pod in either its primary (level 2) or
            // secondary slot. Treat that level-3+ pod as satisfying the
            // primary link, so the engine presents the sidecar next tier
            // instead of offering level 2 again.
            if (own_tier >= kNativeMaximumTier && other_tier == 2 &&
                own_system == other_system &&
                pod_belongs_to_extended_station(pod)) {
                const LONG log_number = InterlockedIncrement(
                    &g_chain_bridge_log_count);
                if (log_number <= 12) {
                    char message[176];
                    std::snprintf(
                        message, sizeof(message),
                        "Extended pod chain: attached level %lu/system %lu satisfies native level-2 prerequisite",
                        static_cast<unsigned long>(own_tier),
                        static_cast<unsigned long>(own_system));
                    log_message(message);
                }
                return 1;
            }
        }
        if (own_tier > kNativeMaximumTier ||
            other_tier > kNativeMaximumTier) {
            if (!is_research_upgrade_pod(own_class) ||
                !is_research_upgrade_pod(other_class)) {
                return 0;
            }
            const std::uint32_t own_system =
                *reinterpret_cast<const std::uint32_t*>(
                    bytes(own_class) + kPodUpgradeSystemOffset);
            const std::uint32_t other_system =
                *reinterpret_cast<const std::uint32_t*>(
                    bytes(other_class) + kPodUpgradeSystemOffset);
            const bool same = own_tier == other_tier &&
                              own_system == other_system;
            // A small, process-lifetime diagnostic budget confirms whether
            // the station's live button-selection pass actually reaches an
            // extended candidate.  This is deliberately capped because the
            // comparison runs while the research-station UI is refreshed.
            const LONG log_number = InterlockedIncrement(
                &g_extended_compare_log_count);
            if (log_number <= 12) {
                char message[192];
                std::snprintf(
                    message, sizeof(message),
                    "Extended pod comparison: attached tier %lu/system %lu, candidate tier %lu/system %lu -> %s",
                    static_cast<unsigned long>(own_tier),
                    static_cast<unsigned long>(own_system),
                    static_cast<unsigned long>(other_tier),
                    static_cast<unsigned long>(other_system),
                    same ? "same" : "different");
                log_message(message);
            }
            return same;
        }
    }
    return a2fo_call_thiscall_1(
        g_same_type_hook.gateway, pod,
        reinterpret_cast<std::uintptr_t>(other_class));
}

std::uintptr_t __attribute__((fastcall)) pod_attach_hook(
    void* pod, void*, std::uintptr_t argument) noexcept {
    const std::uintptr_t result = a2fo_call_thiscall_1(
        g_pod_attach_hook.gateway, pod, argument);
    try {
        remember_attached_pod(pod);
        refresh_station_progression(reinterpret_cast<void*>(argument));
    } catch (...) {
        log_message("Upgrade-pod attach tracking failed");
    }
    return result;
}

std::uintptr_t __attribute__((fastcall)) pod_detach_hook(
    void* pod, void*) noexcept {
    PodInstanceState previous;
    void* station = nullptr;
    bool tracked = false;
    if (pod && readable_range(
            bytes(pod) + kResearchPodStationOffset, sizeof(void*))) {
        station = *reinterpret_cast<void**>(
            bytes(pod) + kResearchPodStationOffset);
    }
    if (pod && g_upgrade_lock_ready) {
        EnterCriticalSection(&g_upgrade_lock);
        const auto found = g_pod_instances.find(pod);
        if (found != g_pod_instances.end()) {
            previous = found->second;
            tracked = true;
            g_pod_instances.erase(found);
        }
        LeaveCriticalSection(&g_upgrade_lock);
    }
    const std::uintptr_t result = a2fo_call_thiscall_0(
        g_pod_detach_hook.gateway, pod);
    if (tracked) recompute_multiplier(previous.manager, previous.system);
    try {
        refresh_station_progression(station);
    } catch (...) {
        log_message("Upgrade station tier-list rollback failed");
    }
    return result;
}

bool install_hook(void* target, void* replacement, std::size_t length,
                  const std::uint8_t* expected,
                  A2FO_InlineHook& hook) {
    return g_api->install_inline_hook(
        target, replacement, length, expected, &hook);
}

}  // namespace

bool initialize_upgrade_pods(const A2FO_ModuleApi* api,
                             HMODULE armada,
                             HMODULE fleet_ops) {
    if (!api || !armada || !fleet_ops || !api->install_inline_hook ||
        !api->extension_root_count || !api->extension_root) {
        if (api && api->log) {
            api->log(kModuleName,
                     "Configurable upgrade pods unavailable in this core");
        }
        return false;
    }

    const bool signatures_match =
        signature_matches(armada, kResearchPodDetachRva,
                          kExpectedResearchPodDetach) &&
        signature_matches(armada, kResearchPodAttachRva,
                          kExpectedResearchPodAttach) &&
        signature_matches(armada, kResearchStationConstructorRva,
                          kExpectedResearchStationConstructor) &&
        signature_matches(armada, kResearchStationDestructorRva,
                          kExpectedResearchStationDestructor) &&
        signature_matches(armada, kTeamManagerForTeamRva,
                          kExpectedTeamManagerForTeam) &&
        signature_matches(armada, kTeamSetUpgradeMultiplierRva,
                          kExpectedTeamSetUpgradeMultiplier) &&
        signature_matches(armada, kGameObjectClassFindRva,
                          kExpectedGameObjectClassFind) &&
        signature_matches(armada, kGameObjectClassFindProjectIdRva,
                          kExpectedGameObjectClassFindProjectId) &&
        signature_matches(armada, kParameterDbGetProjectIdRva,
                          kExpectedParameterDbGetProjectId) &&
        signature_matches(fleet_ops, kFoResearchPodClassCallbackRva,
                          kExpectedResearchPodClassCallback) &&
        signature_matches(fleet_ops, kFoResearchPodClassDtorRva,
                          kExpectedResearchPodClassDtor) &&
        signature_matches(fleet_ops, kFoResearchStationClassCallbackRva,
                          kExpectedResearchStationClassCallback) &&
        signature_matches(fleet_ops, kFoResearchStationCanBuildRva,
                          kExpectedResearchStationCanBuild) &&
        signature_matches(fleet_ops, kFoResearchPodSameTypeRva,
                          kExpectedResearchPodSameType);
    if (!signatures_match) {
        api->log(kModuleName,
                 "Configurable upgrade-pod signatures mismatch; disabled");
        return false;
    }

    g_api = api;
    g_armada = armada;
    load_upgrade_pod_maximum_tier();
    InitializeCriticalSection(&g_upgrade_lock);
    g_upgrade_lock_ready = true;

    // Validate every target before this point, then install all hooks as one
    // supported-build feature. A failed installation leaves the module loaded
    // for safety; native modules are process-lifetime components.
    const bool installed =
        install_hook(at(fleet_ops, kFoResearchPodClassCallbackRva),
                     reinterpret_cast<void*>(&pod_class_hook),
                     sizeof(kExpectedResearchPodClassCallback),
                     kExpectedResearchPodClassCallback, g_pod_class_hook) &&
        install_hook(at(fleet_ops, kFoResearchPodClassDtorRva),
                     reinterpret_cast<void*>(&pod_class_dtor_hook),
                     sizeof(kExpectedResearchPodClassDtor),
                     kExpectedResearchPodClassDtor, g_pod_class_dtor_hook) &&
        install_hook(at(fleet_ops, kFoResearchStationClassCallbackRva),
                     reinterpret_cast<void*>(&station_class_hook),
                     sizeof(kExpectedResearchStationClassCallback),
                     kExpectedResearchStationClassCallback,
                     g_station_class_hook) &&
        install_hook(at(fleet_ops, kFoResearchStationCanBuildRva),
                     reinterpret_cast<void*>(&station_can_build_hook),
                     sizeof(kExpectedResearchStationCanBuild),
                     kExpectedResearchStationCanBuild,
                     g_station_can_build_hook) &&
        install_hook(at(fleet_ops, kFoResearchPodSameTypeRva),
                     reinterpret_cast<void*>(&same_type_hook),
                     sizeof(kExpectedResearchPodSameType),
                     kExpectedResearchPodSameType, g_same_type_hook) &&
        install_hook(at(armada, kResearchPodAttachRva),
                     reinterpret_cast<void*>(&pod_attach_hook),
                     sizeof(kExpectedResearchPodAttach),
                     kExpectedResearchPodAttach, g_pod_attach_hook) &&
        install_hook(at(armada, kResearchPodDetachRva),
                     reinterpret_cast<void*>(&pod_detach_hook),
                     sizeof(kExpectedResearchPodDetach),
                     kExpectedResearchPodDetach, g_pod_detach_hook) &&
        install_hook(at(armada, kResearchStationConstructorRva),
                     reinterpret_cast<void*>(&station_constructor_hook),
                     sizeof(kExpectedResearchStationConstructor),
                     kExpectedResearchStationConstructor,
                     g_station_constructor_hook) &&
        install_hook(at(armada, kResearchStationDestructorRva),
                     reinterpret_cast<void*>(&station_destructor_hook),
                     sizeof(kExpectedResearchStationDestructor),
                     kExpectedResearchStationDestructor,
                     g_station_destructor_hook);
    if (!installed) {
        api->log(kModuleName,
                 "Configurable upgrade-pod hook installation failed");
        return false;
    }
    g_upgrade_hooks_ready = true;
    log_message(
        "Configurable upgrade pods enabled (RTS_CFG.h maximum " +
        std::to_string(g_configured_maximum_tier) + ")");
    return true;
}

}  // namespace a2fo
