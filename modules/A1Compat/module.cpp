/*
 * File: modules/A1Compat/module.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Armada 1 classlabels, officer progression, and compatibility shims.
 */

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>

extern "C" {
void* g_a2fo_a1_rtime_load_name_gateway = nullptr;
void* g_a2fo_a1_standard_text_sprite_gateway = nullptr;
void* g_a2fo_a1_craft_level_up_race_gateway = nullptr;
void* g_a2fo_a1_starbase_initialize_geometry_gateway = nullptr;
void a2fo_a1_rtime_load_name_hook();
void a2fo_a1_standard_text_sprite_hook();
void a2fo_a1_craft_level_up_race_hook();
void a2fo_a1_starbase_initialize_geometry_hook();
void __cdecl a2fo_a1_log_rtime_class_name(
    const char* serialized_name, const void* return_address,
    const void* file_reader, std::uintptr_t second_read_result);
void __cdecl a2fo_a1_log_standard_text_sprite_lookup(
    const void* configuration_string, const void* sprite_string,
    const void* return_address, std::uintptr_t item_index,
    const void* lookup_result);
void __cdecl a2fo_a1_log_craft_level_up_race(
    const void* craft, const void* return_address,
    std::uintptr_t force_level_up, const void* race);
void __cdecl a2fo_a1_prepare_starbase_officer_quarters(void* starbase);
}

namespace {

constexpr const char* kModuleName = "A1Compat";
constexpr std::size_t kMaximumIniTextSize = 1024 * 1024;
constexpr char kA1CompatIniFileName[] = "a1compat.ini";
constexpr char kA1CompatSafeModeKey[] = "safemode";

// Armada 2's native Wingman class supplied these implicitly. The compatibility
// alias deliberately builds a Craft instead, so retain the values from the
// STA1 Classic a2craft.odf as missing-only ParameterDB defaults. Explicit
// commands and values inherited through #include always take precedence.
constexpr std::array<A2FO_ClasslabelOdfDefault, 13>
    kWingmanOdfDefaults{{
        {"enginesHitPercent", "5.0f"},
        {"lifeSupportHitPercent", "8.5f"},
        {"weaponsHitPercent", "5.0f"},
        {"shieldGeneratorHitPercent", "8.0f"},
        {"sensorsHitPercent", "8.0f"},
        {"crewHitPercent", "8.5f"},
        {"hullHitPercent", "57.0f"},
        {"ship", "1"},
        {"has_hitpoints", "1"},
        {"has_crew", "1"},
        {"transporter", "1"},
        {"SHOW_MOVEMENT_AUTONOMY", "1"},
        {"can_explore", "1"},
    }};

// ConstructionRig has a native Armada 2 implementation, but A1 constructor
// ODFs expect these common commands to come from a2const.odf. Supply the same
// missing-only policy without changing the classlabel or replacing values
// declared by a constructor or one of its included parents.
constexpr std::array<A2FO_ClasslabelOdfDefault, 6>
    kConstructionRigOdfDefaults{{
        {"shipclass", "construction"},
        {"builder_facility", "1"},
        {"SHOW_MOVEMENT_AUTONOMY", "1"},
        {"SHOW_SW_AUTONOMY", "1"},
        {"shipType", "N"},
        {"hotkeyLabel", "HOTKEY_F1"},
    }};

// A1 freighters inherit these mining/resource identity commands from the
// shared a2freight.odf template. Preserve them for freighter ODFs which omit
// that include while leaving local and inherited values authoritative.
constexpr std::array<A2FO_ClasslabelOdfDefault, 7>
    kFreighterOdfDefaults{{
        {"shipclass", "mining"},
        {"maxDilithium", "150"},
        {"alert", "1"},
        {"miner", "1"},
        {"SHOW_MOVEMENT_AUTONOMY", "1"},
        {"resourcesCanHandle", "dilithium"},
        {"hotkeyLabel", "HOTKEY_F2"},
    }};

// ArmadaL.exe 1.1/Fleet Operations Roots. The stock Armada 2 symbol is
// NebulaClass::s_SetTexturesRecursive(ST3D_Node*). The Fleet Operations build
// places it at RVA 0x0009dd40 (absolute 0x0049dd40 at its preferred base).
constexpr std::uintptr_t kNebulaSetTexturesRecursiveRva = 0x0009dd40;
constexpr std::size_t kNebulaSetTexturesRecursiveHookLength = 7;
constexpr std::uint8_t kExpectedNebulaSetTexturesRecursive[] = {
    0x55, 0x8b, 0xec, 0x56, 0x8b, 0x75, 0x08};

// ST3D_Node layout recovered from the stock Armada 2 PDB and matching runtime
// disassembly. Type 3 is ST3D_SpriteNode. Its type-specific data pointer is
// the value dereferenced by the failing Armada instruction at RVA 0x0009dd62.
constexpr std::size_t kNodeNameOffset = 0x08;
constexpr std::size_t kNodeParentOffset = 0x18;
constexpr std::size_t kNodeFirstChildOffset = 0x1c;
constexpr std::size_t kNodeNextSiblingOffset = 0x20;
constexpr std::size_t kNodeFlagsOffset = 0xbc;
constexpr std::size_t kSpriteNodeDataOffset = 0xc0;
constexpr std::int32_t kSpriteNodeType = 3;
constexpr std::uint32_t kNodeHiddenFlag = 0x00000001;

// Armada 2 retained the A1 UpgradeClass representation but removed the
// Starbase gameplay which consumes it. These RVAs and layouts are named in
// the stock Armada 2 PDB and byte-identical in the supported FO executable.
constexpr std::uintptr_t kStarbaseClassBuildClassRva = 0x000ab710;
constexpr std::uintptr_t kStarbaseFinishBuildRva = 0x000bbd90;
constexpr std::uintptr_t kStarbaseVtableRva = 0x002b3834;
constexpr std::size_t kStarbaseFinishBuildVtableOffset = 0x184;
constexpr std::uintptr_t kStarbaseClearTeamRva = 0x000bda30;
constexpr std::uintptr_t kStarbaseSetTeamRva = 0x000bda70;
constexpr std::uintptr_t kStarbaseLoadRva = 0x000bdaa0;
constexpr std::uintptr_t kStarbaseSaveRva = 0x000bdae0;
// Use the already validated ParameterDB::GetString entry for A1 numeric
// commands. RVA 0x00135200 is ParameterDB::GetProjectId, not GetInt; detouring
// it corrupts class project-ID registration and must never be used here.
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kFileOutBytesRva = 0x0012c680;
constexpr std::uintptr_t kFileInBytesRva = 0x0012d7a0;
constexpr std::uintptr_t kOfficerUpgradeClassBuildClassRva = 0x000ce910;
constexpr std::uintptr_t kOfficerUpgradeClassVtableRva = 0x002b4144;
constexpr std::uintptr_t kProducerPopBuildQueueItemRva = 0x000b79b0;
constexpr std::size_t kOfficerUpgradeGainOffset = 0x1e0;
constexpr std::size_t kTeamMaximumOfficersOffset = 0x164;
constexpr std::size_t kMaximumProducerQueueWalk = 10;
constexpr std::size_t kProducerClassBuildItemsOffset = 0x450;
constexpr std::size_t kProducerClassBuildItemCapacity = 57;
constexpr std::size_t kObjectClassProjectIdOffset = 0x1cc;
constexpr std::size_t kGameObjectClassMenuCapabilitiesOffset = 0x1d4;
constexpr std::uint32_t kBuilderShipMenuCapability = 0x00000080u;
constexpr std::size_t kMaximumLoggedStarbaseBuildItems = 12;

// Fleet Ops filters each parsed Producer slot immediately before binding its
// palette button. This register-ABI helper receives EAX=ProducerClass,
// EDX=team index, and ECX=build-item slot. Keep this as a bounded diagnostic:
// it reports the native decision and the associated TTechItem state without
// changing visibility.
constexpr std::uintptr_t kProducerBuildButtonVisibleRva = 0x0011d8f8;
constexpr std::size_t kProducerBuildButtonVisibleHookLength = 5;
constexpr std::uint8_t kExpectedProducerBuildButtonVisible[] = {
    0x53, 0x56, 0x51, 0x8b, 0xf1};
constexpr std::uintptr_t kTeamTechnologyTreesPointerRva = 0x00212f08;
constexpr std::size_t kTechnologyTreeItemsOffset = 0x0c;
constexpr std::size_t kTechnologyItemEnabledOffset = 0x09;
constexpr std::size_t kTechnologyItemActiveBuildsOffset = 0x18;
constexpr std::size_t kTechnologyItemRequirementsOffset = 0x20;
constexpr std::size_t kTechnologyRequirementCountOffset = 0x08;
constexpr std::size_t kMaximumPaletteTeamIndex = 63;
constexpr LONG kMaximumPaletteVisibilityReports = 96;
constexpr std::size_t kProducerCurrentBuildClassOffset = 0x254;
constexpr std::size_t kProducerLastBuiltHandleOffset = 0x26c;
constexpr std::size_t kProducerCurrentQueueIdOffset = 0x2a0;
constexpr std::size_t kProducerStopConstructionEffectVtableOffset = 0x178;

constexpr std::uint8_t kExpectedStarbaseClassBuildClass[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uint8_t kExpectedStarbaseFinishBuild[] = {
    0x53, 0x56, 0x57, 0x8b, 0xf1};
constexpr std::uint8_t kExpectedStarbaseClearTeam[] = {
    0x55, 0x8b, 0xec, 0x51, 0x56};
constexpr std::uint8_t kExpectedStarbaseSetTeam[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedStarbaseLoad[] = {
    0x55, 0x8b, 0xec, 0x56, 0x57};
constexpr std::uint8_t kExpectedStarbaseSave[] = {
    0x55, 0x8b, 0xec, 0x56, 0x57};
constexpr std::uint8_t kExpectedOfficerUpgradeClassBuildClass[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uint8_t kExpectedProducerPopBuildQueueItem[] = {
    0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb};

constexpr std::size_t kQueueHeadOffset = 0x270;
constexpr std::size_t kQueueItemNextOffset = 0x08;
constexpr std::size_t kCraftTeamPointerOffset = 0xf0;

constexpr std::uint32_t kOfficerSaveMagic = 0x514f3141u; // "A1OQ"
constexpr std::uint32_t kOfficerSaveVersion = 1;
constexpr char kOfficerSaveLabel[] = "A1Compat officer quarters";
constexpr std::uintptr_t kMinimumSafeAddress = 0x1000;

// Fleet Operations retains Armada 2's Starbase::InitializeGeometry entry but
// not Armada 1's officer-quarter preparation. A1 toggles flag bit 0 on oq1,
// oq2, ... in the shared class hierarchy immediately before the geometry is
// cloned for the new Starbase. Reintroducing that preparation at the matching
// FO entry prevents an unupgraded A1 starbase from rendering every quarter.
constexpr std::uintptr_t kStarbaseInitializeGeometryRva = 0x000bda00;
constexpr std::size_t kStarbaseInitializeGeometryHookLength = 6;
constexpr std::uint8_t kExpectedStarbaseInitializeGeometry[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::size_t kGameObjectClassGeometryDatabaseOffset = 0x1d8;
constexpr std::size_t kGeometryDatabaseHierarchyRootOffset = 0x3c;
constexpr std::size_t kMaximumOqTraversalDepth = 256;
constexpr std::size_t kMaximumOqTraversalNodes = 16384;

// Fleet Operations' ArmadaL.exe version of the stock Armada 2 symbols
// RtimeClass::Find(char const*) and RtimeClass::Load(FileReader&). The latter
// has just read its 40-byte serialized type name at the diagnostic site. Hook
// after the preceding absolute-address load so the checked bytes contain no
// image-base-dependent operand.
constexpr std::uintptr_t kRtimeClassFindRva = 0x0013c1a0;
constexpr std::uintptr_t kRtimeClassLoadNameRva = 0x0013c2da;
constexpr std::size_t kRtimeClassLoadNameHookLength = 5;
constexpr std::uint8_t kExpectedRtimeClassLoadName[] = {
    0x83, 0xc4, 0x18, 0x33, 0xc0};
constexpr std::size_t kSerializedRtimeClassNameSize = 40;
constexpr std::size_t kFileReaderInspectionSize = 0x60;

// StandardText::InitializeConfiguration performs an ST3D sprite lookup and
// assumes it succeeded. A missing GUI sprite therefore becomes a null read at
// RVA 0x0010ad39. Hook immediately after the lookup, while both custom string
// objects are still live, to report the exact configuration and sprite name.
// The gateway retains the native temporary-string cleanup and failure.
constexpr std::uintptr_t kStandardTextSpriteLookupRva = 0x0010ad23;
constexpr std::size_t kStandardTextSpriteLookupHookLength = 5;
constexpr std::uint8_t kExpectedStandardTextSpriteLookup[] = {
    0x6a, 0x01, 0x8d, 0x4d, 0xd8};
constexpr std::size_t kTStringCharacterPointerOffset = 0x04;

// CraftEnhancement.Craft_mLevelUp obtains the craft's Side at +0xf0, then
// that Side's Race at +0x244, before consulting RaceEnhancement's canGainXP
// flag at Race+0x634. An A1 map currently reaches this read with a null Race.
// Diagnose the exact craft and caller without changing the native result or
// failure so the incompatible data/initialization contract can be repaired.
constexpr std::uintptr_t kCraftLevelUpRaceRva = 0x001dbdcb;
constexpr std::size_t kCraftLevelUpRaceHookLength = 7;
constexpr std::uint8_t kExpectedCraftLevelUpRace[] = {
    0x0f, 0xb6, 0x80, 0x34, 0x06, 0x00, 0x00};

constexpr std::size_t kGameObjectClassOffset = 0x40;
constexpr std::size_t kGameObjectHandleOffset = 0x28;
constexpr std::size_t kGameObjectTeamOffset = 0xec;
constexpr std::size_t kCraftSideOffset = 0xf0;
constexpr std::size_t kCraftEnhancementOffset = 0x1a4;
constexpr std::size_t kSideRaceOffset = 0x244;
constexpr std::uintptr_t kGameObjectClassGetOdfNameRva = 0x000ce370;
constexpr std::uint8_t kExpectedGameObjectClassGetOdfName[] = {
    0x8b, 0x89, 0xcc, 0x01, 0x00, 0x00,
    0xe9, 0x25, 0xb0, 0x18, 0x00};

const A2FO_ModuleApi* g_api = nullptr;
void* g_armada = nullptr;
void* g_fleet_ops = nullptr;
A2FO_InlineHook g_nebula_set_textures_recursive_hook{};
A2FO_InlineHook g_rtime_class_load_name_hook{};
A2FO_InlineHook g_standard_text_sprite_lookup_hook{};
A2FO_InlineHook g_craft_level_up_race_hook{};
A2FO_InlineHook g_starbase_initialize_geometry_hook{};
A2FO_InlineHook g_starbase_class_build_class_hook{};
A2FO_InlineHook g_starbase_clear_team_hook{};
A2FO_InlineHook g_starbase_set_team_hook{};
A2FO_InlineHook g_starbase_load_hook{};
A2FO_InlineHook g_starbase_save_hook{};
A2FO_InlineHook g_officer_upgrade_class_build_class_hook{};
A2FO_InlineHook g_producer_build_button_visible_hook{};
void** g_starbase_finish_build_vtable_slot = nullptr;
void* g_starbase_finish_build_original = nullptr;
bool g_starbase_finish_build_vtable_hook_installed = false;
volatile LONG g_invalid_nebula_node_count = 0;
volatile LONG g_missing_rtime_class_count = 0;
volatile LONG g_missing_standard_text_sprite_count = 0;
volatile LONG g_missing_craft_race_count = 0;
volatile LONG g_officer_quarter_prepare_count = 0;
volatile LONG g_officer_upgrade_completion_count = 0;
volatile LONG g_officer_upgrade_rejection_count = 0;
volatile LONG g_officer_upgrade_class_count = 0;
volatile LONG g_officer_upgrade_consumed_count = 0;
volatile LONG g_officer_upgrade_effect_suppression_count = 0;
volatile LONG g_palette_visibility_count = 0;
volatile LONG g_constructor_menu_capability_count = 0;

CRITICAL_SECTION g_officer_state_lock;
bool g_officer_state_lock_ready = false;
bool g_officer_upgrade_system_ready = false;
bool g_officer_upgrade_identity_ready = false;
bool g_officer_upgrade_completion_ready = false;
bool g_producer_events_ready = false;

struct StarbaseClassPolicy {
    std::int32_t maximum_upgrades = 6;
    std::int32_t base_officer_gain = 20;
    std::array<char, 64> race{};
};

struct StarbaseOfficerState {
    std::uint32_t completed_upgrades = 0;
    std::int32_t upgrade_officer_gain = 0;
    void* credited_team = nullptr;
    std::int32_t credited_officer_gain = 0;
};

struct OfficerSaveState {
    std::uint32_t magic = kOfficerSaveMagic;
    std::uint32_t version = kOfficerSaveVersion;
    std::uint32_t completed_upgrades = 0;
    std::int32_t upgrade_officer_gain = 0;
};

struct A1CompatSettings {
    bool safe_mode = false;
};

std::unordered_map<void*, StarbaseClassPolicy> g_starbase_class_policies;
std::unordered_map<void*, StarbaseOfficerState> g_starbase_officer_states;
std::unordered_map<void*, std::array<char, 64>> g_officer_upgrade_races;

using NebulaSetTexturesRecursiveFn = void (__cdecl*)(void* node);
using RtimeClassFindFn = void* (__cdecl*)(const char* name);
bool read_parameter_classlabel(
    void* parameter_db, std::array<char, 64>* output) noexcept;
bool read_parameter_race(
    void* parameter_db, std::array<char, 64>* output) noexcept;
bool contains_ci_substring(const char* source, const char* token) noexcept;
void* read_pointer_at(const void* object, std::size_t offset) noexcept;
bool is_valid_parameter_db(void* parameter_db) noexcept;
std::string join_path(const char* root, const char* name);

extern "C" std::uintptr_t a2fo_a1_call_thiscall_0(
    void* function, void* self);
extern "C" std::uintptr_t a2fo_a1_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument);
extern "C" std::uintptr_t a2fo_a1_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
extern "C" std::uintptr_t a2fo_a1_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
extern "C" std::uintptr_t a2fo_a1_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);

void log_line(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message);
}

template <typename T = void>
T* at(void* module, std::uintptr_t rva) noexcept {
    return reinterpret_cast<T*>(
        reinterpret_cast<std::uint8_t*>(module) + rva);
}

bool readable_range(const void* pointer, std::size_t size) noexcept {
    if (!pointer || size == 0) return false;
    const auto target = reinterpret_cast<std::uintptr_t>(pointer);
    if (target < kMinimumSafeAddress || size > std::numeric_limits<std::size_t>::max() - target) {
        return false;
    }
    return !IsBadReadPtr(pointer, size);
}

bool is_executable_pointer(void* pointer) noexcept {
    return pointer && !IsBadCodePtr(reinterpret_cast<FARPROC>(pointer));
}

bool is_valid_parameter_db(void* parameter_db) noexcept {
    // ParameterDB is not a GameObjectClass and must not be validated as though
    // its first field were a virtual-function table. BuildClass supplies this
    // pointer directly from native code, so a readable non-null object is the
    // appropriate preflight before calling its validated accessors.
    return parameter_db && readable_range(parameter_db, sizeof(void*));
}

bool is_plausible_object_class(void* object_class) noexcept {
    if (!readable_range(object_class, sizeof(void*))) {
        return false;
    }
    const void* vtable = read_pointer_at(object_class, 0);
    return vtable && readable_range(vtable, sizeof(void*)) &&
        is_executable_pointer(read_pointer_at(vtable, 0));
}

void* read_pointer_at(const void* object, std::size_t offset) noexcept {
    if (!object || !readable_range(
            static_cast<const std::uint8_t*>(object) + offset,
            sizeof(void*))) {
        return nullptr;
    }
    void* value = nullptr;
    std::memcpy(
        &value, static_cast<const std::uint8_t*>(object) + offset,
        sizeof(value));
    return value;
}

bool writable_range(void* pointer, std::size_t size) noexcept {
    if (!pointer || size == 0) return false;
    const auto target = reinterpret_cast<std::uintptr_t>(pointer);
    if (target < kMinimumSafeAddress || size > std::numeric_limits<std::size_t>::max() - target) {
        return false;
    }
    return !IsBadWritePtr(pointer, size);
}

class OfficerStateLockGuard {
public:
    OfficerStateLockGuard() noexcept {
        EnterCriticalSection(&g_officer_state_lock);
    }
    ~OfficerStateLockGuard() {
        LeaveCriticalSection(&g_officer_state_lock);
    }

    OfficerStateLockGuard(const OfficerStateLockGuard&) = delete;
    OfficerStateLockGuard& operator=(const OfficerStateLockGuard&) = delete;
};

std::int32_t read_int32_at(const void* object, std::size_t offset,
                           std::int32_t fallback = 0) noexcept {
    if (!object || !readable_range(
            static_cast<const std::uint8_t*>(object) + offset,
            sizeof(std::int32_t))) {
        return fallback;
    }
    std::int32_t value = fallback;
    std::memcpy(&value,
                static_cast<const std::uint8_t*>(object) + offset,
                sizeof(value));
    return value;
}

bool write_int32_at(void* object, std::size_t offset,
                    std::int32_t value) noexcept {
    if (!object || !writable_range(
            static_cast<std::uint8_t*>(object) + offset,
            sizeof(value))) {
        return false;
    }
    std::memcpy(static_cast<std::uint8_t*>(object) + offset,
                &value, sizeof(value));
    return true;
}

bool is_officer_upgrade_class(void* object_class) noexcept {
    if (!object_class || !g_armada || !is_plausible_object_class(object_class)) {
        return false;
    }
    const bool by_vtable =
        *reinterpret_cast<void**>(object_class) ==
        at(g_armada, kOfficerUpgradeClassVtableRva);
    if (by_vtable) return true;

    std::array<char, 64> class_label{};
    if (!read_parameter_classlabel(object_class, &class_label)) return false;
    return _stricmp(class_label.data(), "upgrade") == 0;
}

bool is_starbase_label(const char* label) noexcept {
    if (!label || !*label) return false;
    return _stricmp(label, "constructionrig") == 0 ||
        _stricmp(label, "starbase") == 0 ||
        _stricmp(label, "construction") == 0;
}

bool producer_class_has_officer_upgrade_build_item(void* object_class) noexcept {
    if (!is_plausible_object_class(object_class)) return false;
    void** items = reinterpret_cast<void**>(read_pointer_at(
        object_class, kProducerClassBuildItemsOffset));
    if (!items || !readable_range(
            items, kProducerClassBuildItemCapacity * sizeof(void*))) {
        return false;
    }
    for (std::size_t slot = 0; slot < kProducerClassBuildItemCapacity; ++slot) {
        void* target_class = nullptr;
        std::memcpy(&target_class, items + slot, sizeof(target_class));
        if (target_class && is_plausible_object_class(target_class) &&
            is_officer_upgrade_class(target_class)) {
            return true;
        }
    }
    return false;
}

bool infer_starbase_race_from_object_name(
    const char* odf_name, std::array<char, 64>* race) noexcept {
    if (!odf_name || !race) return false;
    race->fill('\0');

    const std::pair<const char*, const char*> known_races[] = {
        {"fbase", "federation"}, {"fbasehq", "federation"}, {"fconst", "federation"},
        {"bbase", "borg"},      {"bconst", "borg"},
        {"kbase", "klingon"},   {"kconst", "klingon"},
        {"rbase", "romulan"},   {"rconst", "romulan"},
    };
    for (const auto& known : known_races) {
        if (contains_ci_substring(odf_name, known.first)) {
            std::snprintf(race->data(), race->size(), "%s", known.second);
            return true;
        }
    }

    if (contains_ci_substring(odf_name, "zclon") ||
        contains_ci_substring(odf_name, "dominion")) {
        std::snprintf(race->data(), race->size(), "dominion");
        return true;
    }
    return false;
}

bool officer_upgrade_matches_race(
    const std::array<char, 64>& race, void* target_class) noexcept {
    if (!g_officer_state_lock_ready || race[0] == '\0' ||
        !is_officer_upgrade_class(target_class)) {
        return false;
    }
    OfficerStateLockGuard lock;
    const auto found = g_officer_upgrade_races.find(target_class);
    return found != g_officer_upgrade_races.end() &&
        found->second[0] != '\0' &&
        _stricmp(race.data(), found->second.data()) == 0;
}

bool officer_upgrade_matches_registered_starbase(
    void* producer_class, void* target_class) noexcept {
    if (!g_officer_state_lock_ready || !producer_class || !target_class) {
        return false;
    }
    OfficerStateLockGuard lock;
    const auto policy = g_starbase_class_policies.find(producer_class);
    const auto target = g_officer_upgrade_races.find(target_class);
    if (target == g_officer_upgrade_races.end()) {
        return false;
    }
    if (!is_officer_upgrade_class(target_class) &&
        (target->second[0] == '\0')) {
        return false;
    }
    if (policy == g_starbase_class_policies.end() ||
        policy->second.race[0] == '\0') {
        return false;
    }
    return
        target != g_officer_upgrade_races.end() &&
        policy->second.race[0] != '\0' && target->second[0] != '\0' &&
        _stricmp(
            policy->second.race.data(), target->second.data()) == 0;
}

bool starbase_policy(void* starbase, StarbaseClassPolicy* output,
                     void** object_class_output = nullptr) noexcept {
    if (!g_officer_state_lock_ready || !starbase || !output) return false;
    void* object_class = read_pointer_at(starbase, kGameObjectClassOffset);
    if (!object_class) return false;
    OfficerStateLockGuard lock;
    const auto found = g_starbase_class_policies.find(object_class);
    if (found == g_starbase_class_policies.end()) return false;
    *output = found->second;
    if (object_class_output) *object_class_output = object_class;
    return true;
}

std::uint32_t completed_officer_upgrades(void* starbase) noexcept {
    if (!g_officer_state_lock_ready || !starbase) return 0;
    OfficerStateLockGuard lock;
    const auto found = g_starbase_officer_states.find(starbase);
    return found == g_starbase_officer_states.end()
        ? 0u : found->second.completed_upgrades;
}

void adjust_team_maximum_officers(void* team, std::int32_t delta) noexcept {
    if (!team || delta == 0) return;
    const std::int32_t current = read_int32_at(
        team, kTeamMaximumOfficersOffset, 0);
    const std::int64_t adjusted =
        static_cast<std::int64_t>(current) + delta;
    const std::int32_t clamped = static_cast<std::int32_t>(
        adjusted < 0 ? 0 :
        adjusted > std::numeric_limits<std::int32_t>::max()
            ? std::numeric_limits<std::int32_t>::max()
            : adjusted);
    write_int32_at(team, kTeamMaximumOfficersOffset, clamped);
}

std::uintptr_t read_uintptr_at(
    const void* object, std::size_t offset) noexcept {
    if (!object || !readable_range(
            static_cast<const std::uint8_t*>(object) + offset,
            sizeof(std::uintptr_t))) {
        return 0;
    }
    std::uintptr_t value = 0;
    std::memcpy(
        &value, static_cast<const std::uint8_t*>(object) + offset,
        sizeof(value));
    return value;
}

void copy_printable_string(
    const char* source, char* output, std::size_t output_size) noexcept {
    if (!output || output_size == 0) return;
    output[0] = '\0';
    if (!source) return;

    std::size_t length = 0;
    while (length + 1 < output_size && readable_range(source + length, 1)) {
        const char value = source[length];
        if (value == '\0') break;
        const unsigned char byte = static_cast<unsigned char>(value);
        output[length++] = (byte >= 0x20 && byte < 0x7f) ? value : '?';
    }
    output[length] = '\0';
}

void normalize_race_name(
    const char* source, std::array<char, 64>* output) noexcept {
    if (!output) return;
    output->fill('\0');
    if (!source) return;

    while (*source == ' ' || *source == '\t' ||
           *source == '\r' || *source == '\n') {
        ++source;
    }
    std::size_t length = 0;
    while (source[length] != '\0' &&
           length + 1 < output->size()) {
        ++length;
    }
    while (length != 0 &&
           (source[length - 1] == ' ' || source[length - 1] == '\t' ||
            source[length - 1] == '\r' || source[length - 1] == '\n')) {
        --length;
    }
    for (std::size_t index = 0; index < length; ++index) {
        char value = source[index];
        if (value >= 'A' && value <= 'Z') {
            value = static_cast<char>(value - 'A' + 'a');
        }
        (*output)[index] = value;
    }
}

bool read_parameter_race(
    void* parameter_db, std::array<char, 64>* output) noexcept {
    if (!parameter_db || !output || !g_armada ||
        !is_valid_parameter_db(parameter_db)) {
        return false;
    }
    std::array<char, 128> value{};
    const std::uintptr_t found = a2fo_a1_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>("race"),
        reinterpret_cast<std::uintptr_t>(value.data()),
        static_cast<std::uintptr_t>(value.size()),
        reinterpret_cast<std::uintptr_t>(""));
    value.back() = '\0';
    normalize_race_name(value.data(), output);
    return (found & 0xffu) != 0 && (*output)[0] != '\0';
}

bool read_parameter_int(
    void* parameter_db, const char* key, std::int32_t* output) noexcept {
    if (!parameter_db || !key || !*key || !output || !g_armada ||
        !is_valid_parameter_db(parameter_db)) {
        return false;
    }

    std::array<char, 64> value{};
    const std::uintptr_t found = a2fo_a1_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(value.data()),
        static_cast<std::uintptr_t>(value.size()),
        reinterpret_cast<std::uintptr_t>(""));
    value.back() = '\0';
    if ((found & 0xffu) == 0 || value[0] == '\0') return false;

    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(value.data(), &end, 10);
    if (end == value.data() || errno == ERANGE) return false;
    while (end && *end != '\0' &&
           std::isspace(static_cast<unsigned char>(*end))) {
        ++end;
    }
    if (!end || *end != '\0' ||
        parsed < std::numeric_limits<std::int32_t>::min() ||
        parsed > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    *output = static_cast<std::int32_t>(parsed);
    return true;
}

void normalize_command_token(
    const char* source, char* output, std::size_t output_size) noexcept {
    if (!output || output_size == 0) return;
    output[0] = '\0';
    if (!source) return;

    while (*source != '\0' &&
           std::isspace(static_cast<unsigned char>(*source))) {
        ++source;
    }

    const char* end = source;
    while (*end != '\0') ++end;
    while (end > source &&
           std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }

    if ((end - source) >= 2 && ((source[0] == '"' && end[-1] == '"') ||
                                (source[0] == '\'' && end[-1] == '\''))) {
        ++source;
        --end;
        while (source < end &&
               std::isspace(static_cast<unsigned char>(*source))) {
            ++source;
        }
        while (end > source &&
               std::isspace(static_cast<unsigned char>(*(end - 1)))) {
            --end;
        }
    }

    std::size_t length = 0;
    while (source < end && length + 1 < output_size) {
        char value = *source++;
        if (value >= 'A' && value <= 'Z') {
            value = static_cast<char>(value - 'A' + 'a');
        }
        output[length++] = value;
    }
    output[length] = '\0';
}

bool parse_bool_config_value(std::string_view value, bool& out_value) noexcept {
    std::string normalized(value);
    while (!normalized.empty() &&
           std::isspace(static_cast<unsigned char>(normalized.front()))) {
        normalized.erase(normalized.begin());
    }
    while (!normalized.empty() &&
           std::isspace(static_cast<unsigned char>(normalized.back()))) {
        normalized.pop_back();
    }
    if (normalized.empty()) return false;
    for (char& ch : normalized) {
        ch = static_cast<char>(std::tolower(
            static_cast<unsigned char>(ch)));
    }
    if (normalized == "1" || normalized == "true" ||
        normalized == "yes" || normalized == "on" ||
        normalized == "enabled") {
        out_value = true;
        return true;
    }
    if (normalized == "0" || normalized == "false" ||
        normalized == "no" || normalized == "off" ||
        normalized == "disabled") {
        out_value = false;
        return true;
    }
    return false;
}

std::string read_small_text_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(kMaximumIniTextSize)) {
        return {};
    }
    input.seekg(0, std::ios::beg);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

bool parse_a1compat_ini(const std::string& contents,
                        A1CompatSettings& settings) noexcept {
    bool in_section = false;
    bool found = false;
    std::istringstream stream(contents);
    for (std::string line; std::getline(stream, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::size_t cursor = 0;
        while (cursor < line.size() &&
               std::isspace(static_cast<unsigned char>(line[cursor]))) {
            ++cursor;
        }
        line.erase(0, cursor);
        while (!line.empty() &&
               std::isspace(static_cast<unsigned char>(line.back()))) {
            line.pop_back();
        }

        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.size() >= 2 && line.front() == '[' &&
            line.back() == ']') {
            std::string section = line.substr(1, line.size() - 2);
            for (char& ch : section) {
                ch = static_cast<char>(std::tolower(
                    static_cast<unsigned char>(ch)));
            }
            while (!section.empty() &&
                   std::isspace(static_cast<unsigned char>(section.back()))) {
                section.pop_back();
            }
            while (!section.empty() &&
                   std::isspace(static_cast<unsigned char>(section.front()))) {
                section.erase(0, 1);
            }
            in_section = (section == "a1compat");
            continue;
        }

        if (!in_section) continue;

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;
        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        for (char& ch : key) {
            ch = static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        }
        while (!key.empty() &&
               std::isspace(static_cast<unsigned char>(key.back()))) {
            key.pop_back();
        }
        while (!key.empty() &&
               std::isspace(static_cast<unsigned char>(key.front()))) {
            key.erase(0, 1);
        }

        if (key == kA1CompatSafeModeKey) {
            bool safe_mode = settings.safe_mode;
            if (parse_bool_config_value(value, safe_mode)) {
                settings.safe_mode = safe_mode;
                found = true;
            }
        }
    }
    return found;
}

bool read_a1compat_settings(A1CompatSettings& settings) noexcept {
    settings = {};
    if (!g_api || !g_api->extension_root_count || !g_api->extension_root) {
        return false;
    }
    const std::uint32_t count = g_api->extension_root_count();
    if (count == 0 || count > 4096) return false;

    bool any_parsed = false;
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto marker =
            join_path(g_api->extension_root(index), kA1CompatIniFileName);
        if (marker.empty()) continue;

        const DWORD attributes = GetFileAttributesA(marker.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        const std::string contents = read_small_text_file(marker);
        if (contents.empty()) continue;
        if (parse_a1compat_ini(contents, settings)) {
            any_parsed = true;
        }
    }
    return any_parsed;
}

bool read_parameter_classlabel(
    void* parameter_db, std::array<char, 64>* output) noexcept {
    if (!parameter_db || !output || !g_armada ||
        !is_valid_parameter_db(parameter_db)) {
        return false;
    }
    if (g_api && g_api->get_original_classlabel &&
        A2FO_MODULE_API_HAS(g_api, get_original_classlabel)) {
        if (!g_api->get_original_classlabel(
                parameter_db, output->data(),
                static_cast<std::uint32_t>(output->size()))) {
            output->front() = '\0';
            return false;
        }
        normalize_command_token(output->data(), output->data(), output->size());
        return (*output)[0] != '\0';
    }

    const char* classlabel_keys[] = {"classlabel", "classLabel"};
    bool found = false;
    for (const char* key : classlabel_keys) {
        std::array<char, 128> value{};
        const std::uintptr_t key_found = a2fo_a1_call_thiscall_4(
            at(g_armada, kParameterDbGetStringRva), parameter_db,
            reinterpret_cast<std::uintptr_t>(key),
            reinterpret_cast<std::uintptr_t>(value.data()),
            static_cast<std::uintptr_t>(value.size()),
            reinterpret_cast<std::uintptr_t>(""));
        value.back() = '\0';
        if ((key_found & 0xffu) != 0 && value[0] != '\0') {
            normalize_command_token(value.data(), output->data(), output->size());
            found = output->data()[0] != '\0';
            if (found) break;
        }
    }
    return found;
}

void copy_tstring(
    const void* string_object, char* output,
    std::size_t output_size) noexcept {
    if (!output || output_size == 0) return;
    output[0] = '\0';
    if (!readable_range(
            string_object,
            kTStringCharacterPointerOffset + sizeof(const char*))) {
        return;
    }

    const char* source = nullptr;
    std::memcpy(
        &source,
        static_cast<const std::uint8_t*>(string_object) +
            kTStringCharacterPointerOffset,
        sizeof(source));
    copy_printable_string(source, output, output_size);
}

void copy_node_name(void* node, char* output, std::size_t output_size) noexcept {
    if (!output || output_size == 0) return;
    output[0] = '\0';
    if (!readable_range(node, kNodeNameOffset + sizeof(const char*))) return;

    const char* source = *reinterpret_cast<const char* const*>(
        reinterpret_cast<const std::uint8_t*>(node) + kNodeNameOffset);
    copy_printable_string(source, output, output_size);
}

std::int32_t node_type(void* node) noexcept {
    if (!readable_range(node, sizeof(void*))) return -1;
    void** vtable = *reinterpret_cast<void***>(node);
    if (!readable_range(vtable, 2 * sizeof(void*)) || !vtable[1]) return -1;
    return static_cast<std::int32_t>(
        a2fo_a1_call_thiscall_0(vtable[1], node));
}

bool parse_officer_quarter_index(
    const char* name, std::uint32_t* output) noexcept {
    if (!name || !output ||
        (name[0] != 'o' && name[0] != 'O') ||
        (name[1] != 'q' && name[1] != 'Q')) {
        return false;
    }

    const char* cursor = name + 2;
    if (*cursor < '0' || *cursor > '9') return false;

    std::uint32_t value = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        const std::uint32_t digit =
            static_cast<std::uint32_t>(*cursor - '0');
        if (value > (UINT32_MAX - digit) / 10u) return false;
        value = (value * 10) + digit;
        ++cursor;
    }
    if (*cursor != '\0' || value == 0) return false;

    *output = value;
    return true;
}

struct OfficerQuarterTraversal {
    std::uint32_t visible_upgrades = 0;
    std::uint32_t matches = 0;
    std::uint32_t changed = 0;
    std::uint32_t highest_index = 0;
    std::size_t remaining_nodes = kMaximumOqTraversalNodes;
    bool truncated = false;
};

void prepare_officer_quarter_nodes(
    void* node, std::size_t depth,
    OfficerQuarterTraversal* traversal) noexcept {
    if (!node || !traversal) return;
    if (depth > kMaximumOqTraversalDepth ||
        traversal->remaining_nodes == 0) {
        traversal->truncated = true;
        return;
    }
    --traversal->remaining_nodes;

    char name[96]{};
    copy_node_name(node, name, sizeof(name));
    std::uint32_t index = 0;
    if (parse_officer_quarter_index(name, &index)) {
        ++traversal->matches;
        if (index > traversal->highest_index) {
            traversal->highest_index = index;
        }

        auto* flags_address = reinterpret_cast<std::uint32_t*>(
            static_cast<std::uint8_t*>(node) + kNodeFlagsOffset);
        if (writable_range(flags_address, sizeof(*flags_address))) {
            std::uint32_t flags = 0;
            std::memcpy(&flags, flags_address, sizeof(flags));
            const std::uint32_t updated =
                index <= traversal->visible_upgrades
                    ? flags & ~kNodeHiddenFlag
                    : flags | kNodeHiddenFlag;
            if (updated != flags) {
                std::memcpy(flags_address, &updated, sizeof(updated));
                ++traversal->changed;
            }
        }
    }

    void* child = read_pointer_at(node, kNodeFirstChildOffset);
    while (child && traversal->remaining_nodes != 0) {
        void* next = read_pointer_at(child, kNodeNextSiblingOffset);
        prepare_officer_quarter_nodes(child, depth + 1, traversal);
        child = next;
    }
    if (child) traversal->truncated = true;
}

void prepare_starbase_officer_quarters(void* starbase) noexcept {
    void* object_class = read_pointer_at(starbase, kGameObjectClassOffset);
    void* geometry_database = read_pointer_at(
        object_class, kGameObjectClassGeometryDatabaseOffset);
    void* hierarchy_root = read_pointer_at(
        geometry_database, kGeometryDatabaseHierarchyRootOffset);
    if (!hierarchy_root) return;

    OfficerQuarterTraversal traversal{};
    traversal.visible_upgrades = completed_officer_upgrades(starbase);
    prepare_officer_quarter_nodes(hierarchy_root, 0, &traversal);
    if (traversal.matches == 0) return;

    const LONG incident =
        InterlockedIncrement(&g_officer_quarter_prepare_count);
    if (incident <= 32) {
        char odf_name[160]{};
        const auto* getter = at<std::uint8_t>(
            g_armada, kGameObjectClassGetOdfNameRva);
        if (object_class && getter &&
            readable_range(
                getter, sizeof(kExpectedGameObjectClassGetOdfName)) &&
            std::memcmp(
                getter, kExpectedGameObjectClassGetOdfName,
                sizeof(kExpectedGameObjectClassGetOdfName)) == 0) {
            const auto result = a2fo_a1_call_thiscall_0(
                const_cast<std::uint8_t*>(getter), object_class);
            copy_printable_string(
                reinterpret_cast<const char*>(result), odf_name,
                sizeof(odf_name));
        }

        char message[448]{};
        std::snprintf(
            message, sizeof(message),
            "Prepared A1 officer quarters #%ld: odf='%s', starbase=%p, "
            "nodes=%lu, highest=oq%lu, visible=%lu, changed=%lu%s",
            static_cast<long>(incident),
            odf_name[0] ? odf_name : "<unavailable>", starbase,
            static_cast<unsigned long>(traversal.matches),
            static_cast<unsigned long>(traversal.highest_index),
            static_cast<unsigned long>(traversal.visible_upgrades),
            static_cast<unsigned long>(traversal.changed),
            traversal.truncated ? ", traversal-truncated" : "");
        log_line(message);
    } else if (incident == 33) {
        log_line("Further A1 officer-quarter preparation reports suppressed");
    }
}

void copy_object_class_odf_name(
    void* object_class, char* output, std::size_t output_size) noexcept {
    if (!output || output_size == 0) return;
    output[0] = '\0';
    if (!is_plausible_object_class(object_class) ||
        !g_armada) {
        return;
    }
    const auto* getter = at<std::uint8_t>(
        g_armada, kGameObjectClassGetOdfNameRva);
    if (!getter ||
        !readable_range(
            getter, sizeof(kExpectedGameObjectClassGetOdfName)) ||
        std::memcmp(
            getter, kExpectedGameObjectClassGetOdfName,
            sizeof(kExpectedGameObjectClassGetOdfName)) != 0) {
        return;
    }
    const auto result = a2fo_a1_call_thiscall_0(
        const_cast<std::uint8_t*>(getter), object_class);
    copy_printable_string(
        reinterpret_cast<const char*>(result), output, output_size);
}

bool is_registered_a1_starbase_class(void* object_class) noexcept {
    if (!g_officer_state_lock_ready || !object_class) return false;
    OfficerStateLockGuard lock;
    return g_starbase_class_policies.find(object_class) !=
        g_starbase_class_policies.end();
}

using ProducerBuildButtonVisibleFn =
    bool (__attribute__((regparm(3))) *)(
        void* producer_class, std::uintptr_t team_index,
        std::uintptr_t slot_index);

bool __attribute__((regparm(3))) producer_build_button_visible_hook(
    void* producer_class, std::uintptr_t team_index,
    std::uintptr_t slot_index) noexcept {
    const auto original = reinterpret_cast<ProducerBuildButtonVisibleFn>(
        g_producer_build_button_visible_hook.gateway);
    if (!original ||
        !is_plausible_object_class(producer_class)) {
        return true;
    }
    void** items = reinterpret_cast<void**>(
        read_pointer_at(
            producer_class, kProducerClassBuildItemsOffset));
    if (!items || !readable_range(
            items, kProducerClassBuildItemCapacity * sizeof(void*))) {
        return false;
    }

    const bool visible = original(producer_class, team_index, slot_index);
    if (slot_index >= kProducerClassBuildItemCapacity) {
        return visible;
    }

    void* target_class = nullptr;
    if (readable_range(items + slot_index, sizeof(target_class))) {
        std::memcpy(&target_class, items + slot_index,
                    sizeof(target_class));
    }

    // A1 puts all stock officer-upgrade classes in each Starbase build list,
    // then selects exactly one through the owning race. Fleet Operations no
    // longer consumes that race command. Hide non-matching upgrade buttons so
    // only a race-correct option can be selected for a known starbase.
    bool officer_upgrade = is_officer_upgrade_class(target_class);
    if (!officer_upgrade && g_officer_state_lock_ready) {
        OfficerStateLockGuard lock;
        officer_upgrade = g_officer_upgrade_races.find(target_class) !=
            g_officer_upgrade_races.end();
    }

    bool race_match = !officer_upgrade;
    if (officer_upgrade) {
        if (is_registered_a1_starbase_class(producer_class)) {
            race_match = officer_upgrade_matches_registered_starbase(
                producer_class, target_class);
        } else if (producer_class_has_officer_upgrade_build_item(
                       producer_class)) {
            char producer_name[64]{};
            copy_object_class_odf_name(
                producer_class, producer_name, sizeof(producer_name));
            if (producer_name[0]) {
                std::array<char, 64> fallback_race{};
                if (infer_starbase_race_from_object_name(
                        producer_name, &fallback_race)) {
                    bool target_matches = false;
                    if (g_officer_state_lock_ready) {
                        OfficerStateLockGuard lock;
                        const auto target =
                            g_officer_upgrade_races.find(target_class);
                        target_matches =
                            target != g_officer_upgrade_races.end() &&
                            target->second[0] != '\0' &&
                            _stricmp(
                                fallback_race.data(), target->second.data()) == 0;
                    }
                    race_match = target_matches;
                } else {
                    race_match = true;
                }
            } else {
                race_match = true;
            }
        }
    }
    const bool filtered_visible = visible && race_match;

    const LONG incident = InterlockedIncrement(&g_palette_visibility_count);
    if (incident > kMaximumPaletteVisibilityReports) {
        if (incident == kMaximumPaletteVisibilityReports + 1) {
            log_line("Further A1 Producer palette visibility reports "
                     "suppressed");
        }
        return filtered_visible;
    }

    const void* project_id_object = read_pointer_at(
        target_class, kObjectClassProjectIdOffset);
    const std::uintptr_t project_id = read_uintptr_at(project_id_object, 0);

    void* technology_item = nullptr;
    if (g_fleet_ops && project_id > 0 &&
        team_index <= kMaximumPaletteTeamIndex) {
        void* technology_trees = read_pointer_at(
            at(g_fleet_ops, kTeamTechnologyTreesPointerRva), 0);
        void* team_tree = read_pointer_at(
            technology_trees, team_index * sizeof(void*));
        void* technology_items = read_pointer_at(
            team_tree, kTechnologyTreeItemsOffset);
        technology_item = read_pointer_at(
            technology_items, (project_id - 1) * sizeof(void*));
    }

    int enabled = -1;
    if (readable_range(
            static_cast<std::uint8_t*>(technology_item) +
                kTechnologyItemEnabledOffset,
            1)) {
        enabled = *(static_cast<std::uint8_t*>(technology_item) +
                    kTechnologyItemEnabledOffset) != 0 ? 1 : 0;
    }
    void* requirements = read_pointer_at(
        technology_item, kTechnologyItemRequirementsOffset);
    const std::int32_t requirement_count = read_int32_at(
        requirements, kTechnologyRequirementCountOffset, -1);

    char producer_name[128]{};
    char target_name[128]{};
    copy_object_class_odf_name(
        producer_class, producer_name, sizeof(producer_name));
    if (is_plausible_object_class(target_class)) {
        copy_object_class_odf_name(
            target_class, target_name, sizeof(target_name));
    }
    char message[512]{};
    std::snprintf(
        message, sizeof(message),
        "A1 Producer palette visibility #%ld: producer='%s', team=%lu, "
        "slot=%lu, target='%s', projectId=%lu, techItem=%p, enabled=%d, "
        "requirements=%ld, native=%s, raceMatch=%s, result=%s",
        static_cast<long>(incident),
        producer_name[0] ? producer_name : "<unavailable>",
        static_cast<unsigned long>(team_index),
        static_cast<unsigned long>(slot_index),
        target_name[0] ? target_name : "<unavailable>",
        static_cast<unsigned long>(project_id), technology_item, enabled,
        static_cast<long>(requirement_count),
        visible ? "visible" : "hidden",
        race_match ? "yes" : "no",
        filtered_visible ? "visible" : "hidden");
    log_line(message);
    return filtered_visible;
}

bool install_producer_palette_visibility_diagnostic(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_fleet_ops || !api->install_inline_hook) return false;
    return api->install_inline_hook(
        at(g_fleet_ops, kProducerBuildButtonVisibleRva),
        reinterpret_cast<void*>(&producer_build_button_visible_hook),
        kProducerBuildButtonVisibleHookLength,
        kExpectedProducerBuildButtonVisible,
        &g_producer_build_button_visible_hook);
}

void log_starbase_build_items(void* object_class) noexcept {
    void** items = reinterpret_cast<void**>(read_pointer_at(
        object_class, kProducerClassBuildItemsOffset));
    if (!items || !readable_range(
            items, kProducerClassBuildItemCapacity * sizeof(void*))) {
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "A1 Starbase Producer table unavailable: class=%p, table=%p",
            object_class, static_cast<void*>(items));
        log_line(message);
        return;
    }

    std::size_t logged = 0;
    for (std::size_t slot = 0;
         slot < kProducerClassBuildItemCapacity &&
         logged < kMaximumLoggedStarbaseBuildItems;
         ++slot) {
        void* target_class = nullptr;
        std::memcpy(&target_class, items + slot, sizeof(target_class));
        if (!target_class) continue;

        char odf_name[128]{};
        copy_object_class_odf_name(
            target_class, odf_name, sizeof(odf_name));
        void* vtable = read_pointer_at(target_class, 0);
        void* project_id_object = read_pointer_at(
            target_class, kObjectClassProjectIdOffset);
        const std::uintptr_t project_id = read_uintptr_at(
            project_id_object, 0);
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 Starbase buildItem%lu: odf='%s', class=%p, vtable=%p, "
            "projectIdObject=%p, projectId=%lu%s",
            static_cast<unsigned long>(slot),
            odf_name[0] ? odf_name : "<unavailable>", target_class,
            vtable, project_id_object,
            static_cast<unsigned long>(project_id),
            is_officer_upgrade_class(target_class)
                ? ", officerUpgrade=yes" : "");
        log_line(message);
        ++logged;
    }
    if (logged == 0) {
        char message[160]{};
        std::snprintf(
            message, sizeof(message),
            "A1 Starbase Producer table is empty: class=%p", object_class);
        log_line(message);
    }
}

bool producer_class_has_build_items(void* object_class) noexcept {
    if (!is_plausible_object_class(object_class)) return false;
    void** items = reinterpret_cast<void**>(read_pointer_at(
        object_class, kProducerClassBuildItemsOffset));
    if (!items || !readable_range(
            items, kProducerClassBuildItemCapacity * sizeof(void*))) {
        return false;
    }
    for (std::size_t slot = 0;
         slot < kProducerClassBuildItemCapacity; ++slot) {
        void* target_class = nullptr;
        std::memcpy(&target_class, items + slot, sizeof(target_class));
        if (target_class && is_plausible_object_class(target_class)) {
            return true;
        }
    }
    return false;
}

bool contains_ci_substring(const char* source, const char* token) noexcept {
    if (!source || !token || !*token) return false;
    const std::size_t token_length = std::strlen(token);
    if (token_length == 0) return false;

    for (std::size_t index = 0; source[index] != '\0'; ++index) {
        bool match = true;
        for (std::size_t offset = 0; offset < token_length; ++offset) {
            const char current_source = source[index + offset];
            if (!current_source) {
                match = false;
                break;
            }
            if (static_cast<char>(std::tolower(
                    static_cast<unsigned char>(current_source))) !=
                static_cast<char>(
                    std::tolower(static_cast<unsigned char>(token[offset])))) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

[[maybe_unused]] bool likely_constructionrig_class(void* object_class) noexcept {
    char odf_name[64]{};
    copy_object_class_odf_name(object_class, odf_name, sizeof(odf_name));
    if (!odf_name[0]) return false;
    return contains_ci_substring(odf_name, "constructionrig") ||
           contains_ci_substring(odf_name, "construct") ||
           contains_ci_substring(odf_name, "const");
}

void ensure_starbase_build_menu_capability(void* object_class) noexcept {
    if (!is_plausible_object_class(object_class) ||
        !producer_class_has_build_items(object_class)) return;

    char odf_name[64]{};
    copy_object_class_odf_name(
        object_class, odf_name, sizeof(odf_name));

    std::uint32_t menu_capabilities = static_cast<std::uint32_t>(
        read_int32_at(
            object_class, kGameObjectClassMenuCapabilitiesOffset, 0));
    if ((menu_capabilities & kBuilderShipMenuCapability) == 0) {
        const std::uint32_t updated =
            menu_capabilities | kBuilderShipMenuCapability;
        if (!write_int32_at(
                object_class, kGameObjectClassMenuCapabilitiesOffset,
                static_cast<std::int32_t>(updated))) {
            return;
        }
        menu_capabilities = updated;

        const LONG incident =
            InterlockedIncrement(&g_constructor_menu_capability_count);
        if (incident <= 64) {
            char message[320]{};
            std::snprintf(
                message, sizeof(message),
                "A1 Starbase build menu capability adjusted: odf='%s', "
                "class=%p, menuCapabilities=0x%08lx, applied builder_ship",
                odf_name[0] ? odf_name : "<unavailable>",
                object_class,
                static_cast<unsigned long>(menu_capabilities));
            log_line(message);
        } else if (incident == 65) {
            log_line("Further A1 Starbase build menu capability updates suppressed");
        }
    }
}

void register_starbase_class_policy(void* object_class,
                                    void* parameter_db) noexcept {
    if (!is_plausible_object_class(object_class) || !parameter_db ||
        !g_armada ||
        !g_officer_state_lock_ready) {
        return;
    }

    std::int32_t maximum_upgrades = 6;
    std::int32_t base_officer_gain = 20;
    std::array<char, 64> race{};
    std::array<char, 64> class_label{};
    const bool has_maximum = read_parameter_int(
        parameter_db, "maximumUpgrades", &maximum_upgrades);
    const bool has_gain = read_parameter_int(
        parameter_db, "officerGain", &base_officer_gain);
    const bool has_classlabel =
        read_parameter_classlabel(parameter_db, &class_label) &&
        is_starbase_label(class_label.data());
    const bool has_officer_upgrade_build_items =
        producer_class_has_officer_upgrade_build_item(object_class);
    const bool is_constructor_like =
        has_classlabel || has_officer_upgrade_build_items ||
        likely_constructionrig_class(object_class);
    if (!is_constructor_like && !has_maximum && !has_gain) return;

    if (!read_parameter_race(parameter_db, &race)) {
        char odf_name[64]{};
        copy_object_class_odf_name(object_class, odf_name, sizeof(odf_name));
        infer_starbase_race_from_object_name(odf_name, &race);
    }

    if (maximum_upgrades < 0) maximum_upgrades = 0;
    if (maximum_upgrades > 256) maximum_upgrades = 256;
    if (base_officer_gain < 0) base_officer_gain = 0;
    if (base_officer_gain > 1000000) base_officer_gain = 1000000;

    // A1 predates A2's context-sensitive builder_ship command.  Only add the
    // confirmed A2 capability after the class has passed the Starbase policy
    // path; applying it globally to arbitrary Producer subclasses can expose
    // menu code that their A1 data was never structured to drive.
    ensure_starbase_build_menu_capability(object_class);

    if (has_maximum || has_gain || race[0] != '\0') {
        try {
            OfficerStateLockGuard lock;
            g_starbase_class_policies[object_class] =
                StarbaseClassPolicy{maximum_upgrades, base_officer_gain, race};
        } catch (...) {
            log_line("Could not retain an A1 Starbase officer policy");
            return;
        }
    }

    char message[384]{};
    std::snprintf(
        message, sizeof(message),
        "Registered A1 Starbase policy: class=%p, hasPolicy=%s, "
        "maximumUpgrades=%ld, officerGain=%ld, classLabel='%s', race='%s', "
        "menuCapabilities=0x%08lx",
        object_class,
        (has_maximum || has_gain || race[0] != '\0') ? "yes" : "no",
        static_cast<long>(maximum_upgrades),
        static_cast<long>(base_officer_gain),
        class_label.data(),
        race[0] ? race.data() : "<unavailable>",
        static_cast<unsigned long>(read_int32_at(
            object_class, kGameObjectClassMenuCapabilitiesOffset, 0)));
    log_line(message);
    log_starbase_build_items(object_class);
}

void* __attribute__((fastcall)) starbase_class_build_class_hook(
    void* builder, void*, void* parameter_db) noexcept {
    void* object_class = reinterpret_cast<void*>(a2fo_a1_call_thiscall_1(
        g_starbase_class_build_class_hook.gateway, builder,
        reinterpret_cast<std::uintptr_t>(parameter_db)));
    if (g_officer_upgrade_system_ready) {
        register_starbase_class_policy(object_class, parameter_db);
    }
    return object_class;
}

void* __attribute__((fastcall)) officer_upgrade_class_build_class_hook(
    void* builder, void*, void* parameter_db) noexcept {
    void* object_class = reinterpret_cast<void*>(a2fo_a1_call_thiscall_1(
        g_officer_upgrade_class_build_class_hook.gateway, builder,
        reinterpret_cast<std::uintptr_t>(parameter_db)));
    if (!object_class || !parameter_db || !g_officer_state_lock_ready) {
        return object_class;
    }

    std::array<char, 64> race{};
    if (!read_parameter_race(parameter_db, &race)) return object_class;
    try {
        OfficerStateLockGuard lock;
        g_officer_upgrade_races[object_class] = race;
    } catch (...) {
        log_line("Could not retain A1 officer-upgrade race identity");
        return object_class;
    }

    const LONG incident = InterlockedIncrement(
        &g_officer_upgrade_class_count);
    if (incident <= 16) {
        char odf_name[128]{};
        copy_object_class_odf_name(
            object_class, odf_name, sizeof(odf_name));
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "Registered A1 officer-upgrade target #%ld: odf='%s', "
            "class=%p, race='%s'",
            static_cast<long>(incident),
            odf_name[0] ? odf_name : "<unavailable>", object_class,
            race.data());
        log_line(message);
    }
    return object_class;
}

bool consume_officer_upgrade_completion(
    void* producer, void* target_class) noexcept {
    if (!g_officer_upgrade_completion_ready ||
        !producer || !target_class) {
        return false;
    }
    StarbaseClassPolicy policy{};
    void* current_class = read_pointer_at(
        producer, kProducerCurrentBuildClassOffset);
    if (!starbase_policy(producer, &policy) ||
        !is_officer_upgrade_class(target_class) ||
        (current_class && current_class != target_class)) {
        return false;
    }

    // A2 retained OfficerUpgradeClass::Build() as a null-returning stub, but
    // its ordinary Starbase completion path assumes every class creates a
    // renderable GameObject. The A1-scoped Starbase::FinishBuild hook calls
    // this helper before A2's derived post-processing. Consume the in-place
    // upgrade there so no null object reaches OutputQueueManager.

    const std::int32_t team_index = read_int32_at(
        producer, kGameObjectTeamOffset, -1);
    const void* project_id_object = read_pointer_at(
        target_class, kObjectClassProjectIdOffset);
    const std::uintptr_t project_id = read_uintptr_at(
        project_id_object, 0);
    if (g_fleet_ops && team_index >= 0 &&
        team_index <= static_cast<std::int32_t>(kMaximumPaletteTeamIndex) &&
        project_id > 0 && project_id <= 8192u) {
        void* technology_trees = read_pointer_at(
            at(g_fleet_ops, kTeamTechnologyTreesPointerRva), 0);
        void* team_tree = read_pointer_at(
            technology_trees,
            static_cast<std::size_t>(team_index) * sizeof(void*));
        if (!team_tree || !readable_range(team_tree, kTechnologyItemActiveBuildsOffset)) {
            return false;
        }
        void* technology_items = read_pointer_at(
            team_tree, kTechnologyTreeItemsOffset);
        if (!technology_items) return false;
        void* technology_item = read_pointer_at(
            technology_items, (project_id - 1) * sizeof(void*));
        if (!technology_item ||
            !readable_range(technology_item,
                            kTechnologyItemActiveBuildsOffset +
                                sizeof(std::int32_t))) {
            return false;
        }
        std::int32_t active_builds = read_int32_at(
            technology_item, kTechnologyItemActiveBuildsOffset, 0);
        if (active_builds > 0) {
            write_int32_at(
                technology_item, kTechnologyItemActiveBuildsOffset,
                active_builds - 1);
        }
    }

    a2fo_a1_call_thiscall_0(
        at(g_armada, kProducerPopBuildQueueItemRva), producer);
    write_int32_at(producer, kProducerCurrentBuildClassOffset, 0);
    write_int32_at(producer, kProducerCurrentQueueIdOffset, 0);
    write_int32_at(producer, kProducerLastBuiltHandleOffset, 0);

    void* vtable = read_pointer_at(producer, 0);
    void* stop_effect = read_pointer_at(
        vtable, kProducerStopConstructionEffectVtableOffset);
    if (stop_effect) {
        a2fo_a1_call_thiscall_0(stop_effect, producer);
    }

    const LONG incident = InterlockedIncrement(
        &g_officer_upgrade_consumed_count);
    if (incident <= 32) {
        char target_name[128]{};
        copy_object_class_odf_name(
            target_class, target_name, sizeof(target_name));
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "Consumed A1 in-place officer upgrade #%ld: starbase=%p, "
            "target='%s', raceMatch=%s",
            static_cast<long>(incident), producer,
            target_name[0] ? target_name : "<unavailable>",
            officer_upgrade_matches_race(policy.race, target_class)
                ? "yes" : "no");
        log_line(message);
    }

    return true;
}

bool suppress_officer_upgrade_construction_effect(
    void* producer, void* target_class) noexcept {
    if (!g_officer_upgrade_completion_ready ||
        !producer || !target_class) {
        return false;
    }
    StarbaseClassPolicy policy{};
    if (!starbase_policy(producer, &policy) ||
        !is_officer_upgrade_class(target_class) ||
        !officer_upgrade_matches_race(policy.race, target_class)) {
        return false;
    }

    const LONG incident = InterlockedIncrement(
        &g_officer_upgrade_effect_suppression_count);
    if (incident <= 32) {
        char target_name[128]{};
        copy_object_class_odf_name(
            target_class, target_name, sizeof(target_name));
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "Suppressed unsafe A1 officer construction effect #%ld: "
            "starbase=%p, target='%s'",
            static_cast<long>(incident), producer,
            target_name[0] ? target_name : "<unavailable>");
        log_line(message);
    }
    return true;
}

void apply_starbase_team_credit(void* starbase) noexcept {
    StarbaseClassPolicy policy{};
    if (!starbase_policy(starbase, &policy)) return;
    void* team = read_pointer_at(starbase, kCraftTeamPointerOffset);
    if (!team) return;

    void* previous_team = nullptr;
    std::int32_t previous_gain = 0;
    std::int32_t new_gain = 0;
    try {
        OfficerStateLockGuard lock;
        StarbaseOfficerState& state =
            g_starbase_officer_states[starbase];
        if (state.credited_team == team) return;
        previous_team = state.credited_team;
        previous_gain = state.credited_officer_gain;
        const std::int64_t total =
            static_cast<std::int64_t>(policy.base_officer_gain) +
            state.upgrade_officer_gain;
        new_gain = static_cast<std::int32_t>(
            total > std::numeric_limits<std::int32_t>::max()
                ? std::numeric_limits<std::int32_t>::max()
                : total);
        state.credited_team = team;
        state.credited_officer_gain = new_gain;
    } catch (...) {
        log_line("Could not retain A1 Starbase team officer state");
        return;
    }
    adjust_team_maximum_officers(previous_team, -previous_gain);
    adjust_team_maximum_officers(team, new_gain);
}

void clear_starbase_team_credit(void* starbase, bool erase_state) noexcept {
    if (!g_officer_state_lock_ready || !starbase) return;
    void* credited_team = nullptr;
    std::int32_t credited_gain = 0;
    try {
        OfficerStateLockGuard lock;
        const auto found = g_starbase_officer_states.find(starbase);
        if (found == g_starbase_officer_states.end()) return;
        credited_team = found->second.credited_team;
        credited_gain = found->second.credited_officer_gain;
        if (erase_state) {
            g_starbase_officer_states.erase(found);
        } else {
            found->second = StarbaseOfficerState{};
        }
    } catch (...) {
        log_line("Could not clear A1 Starbase team officer state");
        return;
    }
    adjust_team_maximum_officers(credited_team, -credited_gain);
    if (!erase_state) prepare_starbase_officer_quarters(starbase);
}

std::uintptr_t __attribute__((fastcall)) starbase_set_team_hook(
    void* starbase, void*, std::uintptr_t team_index) noexcept {
    const std::uintptr_t result = a2fo_a1_call_thiscall_1(
        g_starbase_set_team_hook.gateway, starbase, team_index);
    if (g_officer_upgrade_system_ready) {
        apply_starbase_team_credit(starbase);
    }
    return result;
}

std::uintptr_t __attribute__((fastcall)) starbase_clear_team_hook(
    void* starbase, void*) noexcept {
    if (g_officer_upgrade_system_ready) {
        clear_starbase_team_credit(starbase, false);
    }
    return a2fo_a1_call_thiscall_0(
        g_starbase_clear_team_hook.gateway, starbase);
}

std::uint32_t queued_officer_upgrades(void* producer) noexcept {
    if (!producer) return 0;
    void* item = read_pointer_at(producer, kQueueHeadOffset);
    std::uint32_t count = 0;
    for (std::size_t visited = 0;
         item && visited < kMaximumProducerQueueWalk; ++visited) {
        void* queued_class = read_pointer_at(item, 0);
        if (is_officer_upgrade_class(queued_class)) ++count;
        item = read_pointer_at(item, kQueueItemNextOffset);
    }
    return count;
}

bool admit_officer_upgrade(void* starbase, void* target_class) noexcept {
    if (!is_officer_upgrade_class(target_class)) return true;
    StarbaseClassPolicy policy{};
    if (!starbase_policy(starbase, &policy)) return true;
    if (!officer_upgrade_matches_race(policy.race, target_class)) {
        const LONG incident =
            InterlockedIncrement(&g_officer_upgrade_rejection_count);
        if (incident <= 16) {
            char target_name[128]{};
            copy_object_class_odf_name(
                target_class, target_name, sizeof(target_name));
            char message[288]{};
            std::snprintf(
                message, sizeof(message),
                "Rejected A1 wrong-race officer-quarter order #%ld: "
                "starbase=%p, starbaseRace='%s', target='%s'",
                static_cast<long>(incident), starbase,
                policy.race[0] ? policy.race.data() : "<unavailable>",
                target_name[0] ? target_name : "<unavailable>");
            log_line(message);
        }
        return false;
    }
    const std::uint32_t completed = completed_officer_upgrades(starbase);
    const std::uint32_t queued = queued_officer_upgrades(starbase);
    if (completed + queued <
        static_cast<std::uint32_t>(policy.maximum_upgrades)) {
        return true;
    }

    const LONG incident =
        InterlockedIncrement(&g_officer_upgrade_rejection_count);
    if (incident <= 16) {
        char message[224]{};
        std::snprintf(
            message, sizeof(message),
            "Rejected A1 officer-quarter order #%ld: starbase=%p, "
            "completed=%lu, queued=%lu, maximum=%ld",
            static_cast<long>(incident), starbase,
            static_cast<unsigned long>(completed),
            static_cast<unsigned long>(queued),
            static_cast<long>(policy.maximum_upgrades));
        log_line(message);
    }
    return false;
}

void finish_officer_upgrade(void* starbase, void* target_class) noexcept {
    if (!is_officer_upgrade_class(target_class)) return;
    StarbaseClassPolicy policy{};
    if (!starbase_policy(starbase, &policy)) return;
    if (!officer_upgrade_matches_race(policy.race, target_class)) {
        log_line("Ignored a completed A1 officer upgrade with a mismatched "
                 "race identity");
        return;
    }

    std::int32_t gain = read_int32_at(
        target_class, kOfficerUpgradeGainOffset, 15);
    if (gain < 0) gain = 0;
    if (gain > 1000000) gain = 1000000;
    void* current_team = read_pointer_at(starbase, kCraftTeamPointerOffset);
    void* previous_team = nullptr;
    std::int32_t previous_credit = 0;
    std::int32_t current_team_delta = 0;
    std::uint32_t completed = 0;
    try {
        OfficerStateLockGuard lock;
        StarbaseOfficerState& state =
            g_starbase_officer_states[starbase];
        if (state.completed_upgrades >=
            static_cast<std::uint32_t>(policy.maximum_upgrades)) {
            return;
        }
        previous_team = state.credited_team;
        previous_credit = state.credited_officer_gain;
        ++state.completed_upgrades;
        if (state.upgrade_officer_gain <=
            std::numeric_limits<std::int32_t>::max() - gain) {
            state.upgrade_officer_gain += gain;
        } else {
            state.upgrade_officer_gain =
                std::numeric_limits<std::int32_t>::max();
        }
        const std::int64_t desired =
            static_cast<std::int64_t>(policy.base_officer_gain) +
            state.upgrade_officer_gain;
        const std::int32_t desired_credit = static_cast<std::int32_t>(
            desired > std::numeric_limits<std::int32_t>::max()
                ? std::numeric_limits<std::int32_t>::max()
                : desired);
        if (current_team == previous_team) {
            current_team_delta = desired_credit - previous_credit;
            previous_team = nullptr;
            previous_credit = 0;
        } else {
            current_team_delta = desired_credit;
        }
        state.credited_team = current_team;
        state.credited_officer_gain = current_team ? desired_credit : 0;
        completed = state.completed_upgrades;
    } catch (...) {
        log_line("Could not retain completed A1 officer-quarter state");
        return;
    }

    adjust_team_maximum_officers(previous_team, -previous_credit);
    adjust_team_maximum_officers(current_team, current_team_delta);
    prepare_starbase_officer_quarters(starbase);

    const LONG incident =
        InterlockedIncrement(&g_officer_upgrade_completion_count);
    if (incident <= 32) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "Completed A1 officer quarter #%ld: starbase=%p, "
            "upgrade=%lu/%ld, officerGain=%ld",
            static_cast<long>(incident), starbase,
            static_cast<unsigned long>(completed),
            static_cast<long>(policy.maximum_upgrades),
            static_cast<long>(gain));
        log_line(message);
    }
}

std::uintptr_t __attribute__((fastcall)) starbase_finish_build_hook(
    void* starbase, void*) noexcept {
    void* target_class = read_pointer_at(
        starbase, kProducerCurrentBuildClassOffset);
    if (g_officer_upgrade_completion_ready && target_class &&
        consume_officer_upgrade_completion(starbase, target_class)) {
        // Armada 1 handles OfficerUpgradeClass in Starbase::FinishBuild,
        // applies the in-place gain, and then invokes only the Producer queue
        // cleanup branch. Armada 2 removed that override: its replacement
        // assumes Producer::FinishBuild returned a real GameObject and sends
        // the null OfficerUpgradeClass result to OutputQueueManager. Claim the
        // completion at the original Starbase boundary so that derived A2/FO
        // post-processing never sees a fictitious built object.
        finish_officer_upgrade(starbase, target_class);
        return 0;
    }
    if (!g_starbase_finish_build_original) return 0;
    return a2fo_a1_call_thiscall_0(
        g_starbase_finish_build_original, starbase);
}

bool install_starbase_finish_build_vtable_hook() noexcept {
    if (!g_armada) return false;

    auto* starbase_vtable = at<void**>(g_armada, kStarbaseVtableRva);
    if (!readable_range(starbase_vtable,
                        kStarbaseFinishBuildVtableOffset + sizeof(void*))) {
        log_line("A1 Starbase FinishBuild vtable slot inaccessible");
        return false;
    }

    auto* finish_slot = reinterpret_cast<void**>(
        reinterpret_cast<std::uint8_t*>(starbase_vtable) +
        kStarbaseFinishBuildVtableOffset);
    if (!readable_range(finish_slot, sizeof(void*))) {
        log_line("A1 Starbase FinishBuild vtable slot not readable");
        return false;
    }

    const void* expected_original = at(g_armada, kStarbaseFinishBuildRva);
    void* current_original = *finish_slot;
    if (!readable_range(current_original, sizeof(kExpectedStarbaseFinishBuild))) {
        char message[240]{};
        std::snprintf(
            message, sizeof(message),
            "A1 Starbase FinishBuild function pointer is unreadable: %p",
            current_original);
        log_line(message);
        return false;
    }
    if (std::memcmp(
            current_original, kExpectedStarbaseFinishBuild,
            sizeof(kExpectedStarbaseFinishBuild)) != 0) {
        char message[240]{};
        std::snprintf(
            message, sizeof(message),
            "A1 Starbase FinishBuild function signature mismatch: %p",
            current_original);
        log_line(message);
        return false;
    }

    if (current_original != expected_original) {
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "A1 Starbase FinishBuild vtable mismatch: expected=%p found=%p",
            expected_original, current_original);
        log_line(message);
        return false;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(
            finish_slot, sizeof(void*), PAGE_READWRITE, &old_protect)) {
        log_line("A1 Starbase FinishBuild vtable patch protection change failed");
        return false;
    }

    const auto previous = static_cast<void*>(
        InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(finish_slot),
            reinterpret_cast<PVOID>(&starbase_finish_build_hook)));
    if (previous != current_original) {
        InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(finish_slot), previous);
        DWORD restore_after_mismatch = 0;
        VirtualProtect(
            finish_slot, sizeof(void*), old_protect, &restore_after_mismatch);
        log_line("A1 Starbase FinishBuild vtable changed while patching");
        return false;
    }
    VirtualProtect(finish_slot, sizeof(void*), old_protect, &old_protect);

    g_starbase_finish_build_vtable_slot = finish_slot;
    g_starbase_finish_build_original = previous;
    g_starbase_finish_build_vtable_hook_installed = true;
    log_line("A1 Starbase FinishBuild vtable hook installed");
    return true;
}

bool A2FO_CALL officer_producer_event_handler(
    const A2FO_ProducerEvent* event, void*) noexcept {
    if (!g_officer_upgrade_system_ready || !event ||
        event->struct_size < sizeof(*event) || !event->producer) {
        return true;
    }
    switch (event->kind) {
        case A2FO_PRODUCER_EVENT_ADMIT:
            if (is_officer_upgrade_class(event->target_class) &&
                !g_officer_upgrade_completion_ready) {
                return false;
            }
            return admit_officer_upgrade(
                event->producer, event->target_class);
        case A2FO_PRODUCER_EVENT_FINISHING:
            // A1Compat owns Starbase::FinishBuild directly. Claiming the
            // later generic Producer callback would still let Armada 2's
            // Starbase post-processing enqueue a null finished object.
            return true;
        case A2FO_PRODUCER_EVENT_STARTING_EFFECT:
            return !suppress_officer_upgrade_construction_effect(
                event->producer, event->target_class);
        case A2FO_PRODUCER_EVENT_FINISHED:
            return true;
        case A2FO_PRODUCER_EVENT_DESTROYING:
            clear_starbase_team_credit(event->producer, true);
            return true;
        default:
            return true;
    }
}

using FileOutBytesFn = bool (__cdecl*)(
    void* writer, void* data, std::uint32_t size, const char* label);
using FileInBytesFn = bool (__cdecl*)(
    void* reader, void* data, std::uint32_t size);

std::uintptr_t __attribute__((fastcall)) starbase_save_hook(
    void* starbase, void*, void* writer) noexcept {
    StarbaseClassPolicy policy{};
    if (!g_officer_upgrade_system_ready ||
        !starbase_policy(starbase, &policy)) {
        return a2fo_a1_call_thiscall_1(
            g_starbase_save_hook.gateway, starbase,
            reinterpret_cast<std::uintptr_t>(writer));
    }

    OfficerSaveState saved{};
    try {
        OfficerStateLockGuard lock;
        const auto found = g_starbase_officer_states.find(starbase);
        if (found != g_starbase_officer_states.end()) {
            saved.completed_upgrades = found->second.completed_upgrades;
            saved.upgrade_officer_gain =
                found->second.upgrade_officer_gain;
        }
    } catch (...) {
        return 0;
    }
    const auto out = reinterpret_cast<FileOutBytesFn>(
        at(g_armada, kFileOutBytesRva));
    const bool compatibility_saved = out(
        writer, &saved, sizeof(saved), kOfficerSaveLabel);
    const bool native_saved = a2fo_a1_call_thiscall_1(
        g_starbase_save_hook.gateway, starbase,
        reinterpret_cast<std::uintptr_t>(writer)) != 0;
    return compatibility_saved && native_saved;
}

std::uintptr_t __attribute__((fastcall)) starbase_load_hook(
    void* starbase, void*, void* reader) noexcept {
    StarbaseClassPolicy policy{};
    if (!g_officer_upgrade_system_ready ||
        !starbase_policy(starbase, &policy)) {
        return a2fo_a1_call_thiscall_1(
            g_starbase_load_hook.gateway, starbase,
            reinterpret_cast<std::uintptr_t>(reader));
    }

    OfficerSaveState saved{};
    const auto in = reinterpret_cast<FileInBytesFn>(
        at(g_armada, kFileInBytesRva));
    const bool compatibility_loaded =
        in(reader, &saved, sizeof(saved));
    const bool native_loaded = a2fo_a1_call_thiscall_1(
        g_starbase_load_hook.gateway, starbase,
        reinterpret_cast<std::uintptr_t>(reader)) != 0;
    if (!compatibility_loaded || saved.magic != kOfficerSaveMagic ||
        saved.version != kOfficerSaveVersion) {
        log_line("A1 officer-quarter save state is missing or invalid");
        return 0;
    }

    if (saved.completed_upgrades >
        static_cast<std::uint32_t>(policy.maximum_upgrades)) {
        saved.completed_upgrades =
            static_cast<std::uint32_t>(policy.maximum_upgrades);
    }
    if (saved.upgrade_officer_gain < 0) {
        saved.upgrade_officer_gain = 0;
    }
    void* team = read_pointer_at(starbase, kCraftTeamPointerOffset);
    try {
        OfficerStateLockGuard lock;
        StarbaseOfficerState& state =
            g_starbase_officer_states[starbase];
        state.completed_upgrades = saved.completed_upgrades;
        state.upgrade_officer_gain = saved.upgrade_officer_gain;
        state.credited_team = team;
        const std::int64_t credited =
            static_cast<std::int64_t>(policy.base_officer_gain) +
            saved.upgrade_officer_gain;
        state.credited_officer_gain = team
            ? static_cast<std::int32_t>(
                credited > std::numeric_limits<std::int32_t>::max()
                    ? std::numeric_limits<std::int32_t>::max()
                    : credited)
            : 0;
    } catch (...) {
        return 0;
    }
    prepare_starbase_officer_quarters(starbase);
    return native_loaded;
}

template <std::size_t Size>
bool a1_signature_matches(std::uintptr_t rva,
                          const std::uint8_t (&expected)[Size]) noexcept {
    const void* address = at(g_armada, rva);
    return readable_range(address, Size) &&
        std::memcmp(address, expected, Size) == 0;
}

bool install_officer_upgrade_system(const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->install_inline_hook) return false;

    auto install_starbase_hook = [&](const char* signature_name,
                                     std::uintptr_t rva,
                                     void* replacement,
                                     std::size_t length,
                                     const std::uint8_t* expected,
                                     A2FO_InlineHook* hook_state) noexcept {
        const bool installed = api->install_inline_hook(
            at(g_armada, rva), replacement, length, expected, hook_state);
        if (!installed) {
            char message[192]{};
            std::snprintf(
                message, sizeof(message),
                "A1 Starbase %s hook installation failed", signature_name);
            log_line(message);
        }
        return installed;
    };

    const bool class_hook_installed = install_starbase_hook(
        "ClassBuildClass", kStarbaseClassBuildClassRva,
        reinterpret_cast<void*>(&starbase_class_build_class_hook),
        sizeof(kExpectedStarbaseClassBuildClass),
        kExpectedStarbaseClassBuildClass, &g_starbase_class_build_class_hook);
    if (!class_hook_installed) {
        log_line("A1 Starbase BuildClass signature mismatch: "
                 "StarbaseClass::BuildClass");
        return false;
    }

    install_starbase_hook(
        "ClearTeam", kStarbaseClearTeamRva,
        reinterpret_cast<void*>(&starbase_clear_team_hook),
        sizeof(kExpectedStarbaseClearTeam),
        kExpectedStarbaseClearTeam, &g_starbase_clear_team_hook);
    if (!a1_signature_matches(kStarbaseClearTeamRva,
                              kExpectedStarbaseClearTeam)) {
        log_line("A1 Starbase policy signature mismatch: "
                 "Starbase::ClearTeam");
    }

    install_starbase_hook(
        "SetTeam", kStarbaseSetTeamRva,
        reinterpret_cast<void*>(&starbase_set_team_hook),
        sizeof(kExpectedStarbaseSetTeam),
        kExpectedStarbaseSetTeam, &g_starbase_set_team_hook);
    if (!a1_signature_matches(kStarbaseSetTeamRva,
                              kExpectedStarbaseSetTeam)) {
        log_line("A1 Starbase policy signature mismatch: "
                 "Starbase::SetTeam");
    }

    install_starbase_hook(
        "Load", kStarbaseLoadRva,
        reinterpret_cast<void*>(&starbase_load_hook),
        sizeof(kExpectedStarbaseLoad), kExpectedStarbaseLoad,
        &g_starbase_load_hook);
    if (!a1_signature_matches(kStarbaseLoadRva, kExpectedStarbaseLoad)) {
        log_line("A1 Starbase policy signature mismatch: "
                 "Starbase::Load");
    }

    install_starbase_hook(
        "Save", kStarbaseSaveRva,
        reinterpret_cast<void*>(&starbase_save_hook),
        sizeof(kExpectedStarbaseSave), kExpectedStarbaseSave,
        &g_starbase_save_hook);
    if (!a1_signature_matches(kStarbaseSaveRva, kExpectedStarbaseSave)) {
        log_line("A1 Starbase policy signature mismatch: "
                 "Starbase::Save");
    }

    g_officer_upgrade_system_ready = class_hook_installed;
    if (!g_officer_upgrade_system_ready) {
        log_line("A1 Starbase policy hook installation failed");
        return false;
    }
    const bool identity_signature_valid = a1_signature_matches(
        kOfficerUpgradeClassBuildClassRva,
        kExpectedOfficerUpgradeClassBuildClass);
    if (!identity_signature_valid) {
        log_line("A1 officer target signature mismatch: "
                 "OfficerUpgradeClass::BuildClass");
    }
    if (identity_signature_valid) {
        g_officer_upgrade_identity_ready = api->install_inline_hook(
            at(g_armada, kOfficerUpgradeClassBuildClassRva),
            reinterpret_cast<void*>(
                &officer_upgrade_class_build_class_hook),
            sizeof(kExpectedOfficerUpgradeClassBuildClass),
            kExpectedOfficerUpgradeClassBuildClass,
            &g_officer_upgrade_class_build_class_hook);
        if (!g_officer_upgrade_identity_ready) {
            log_line("A1 officer target hook installation failed");
        }
    }

    const bool pop_signature_valid = a1_signature_matches(
        kProducerPopBuildQueueItemRva,
        kExpectedProducerPopBuildQueueItem);
    if (!pop_signature_valid) {
        log_line("A1 officer completion signature mismatch: "
                 "Producer::PopBuildQueueItem");
    }
    const bool completion_hook_installed =
        g_producer_events_ready && g_officer_upgrade_identity_ready &&
        pop_signature_valid &&
        install_starbase_finish_build_vtable_hook();
    g_officer_upgrade_completion_ready = completion_hook_installed;
    if (!g_officer_upgrade_completion_ready) {
        log_line("A1 officer completion bridge unavailable; ordinary "
                 "Starbase construction remains enabled");
    }
    return true;
}

bool install_starbase_officer_quarter_compatibility(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->install_inline_hook) return false;
    const bool installed = api->install_inline_hook(
        at(g_armada, kStarbaseInitializeGeometryRva),
        reinterpret_cast<void*>(&a2fo_a1_starbase_initialize_geometry_hook),
        kStarbaseInitializeGeometryHookLength,
        kExpectedStarbaseInitializeGeometry,
        &g_starbase_initialize_geometry_hook);
    if (installed) {
        g_a2fo_a1_starbase_initialize_geometry_gateway =
            g_starbase_initialize_geometry_hook.gateway;
    }
    return installed;
}

void __cdecl nebula_set_textures_recursive_hook(void* node) noexcept {
    const auto original = reinterpret_cast<NebulaSetTexturesRecursiveFn>(
        g_nebula_set_textures_recursive_hook.gateway);
    if (!original || !node) return;

    if (node_type(node) == kSpriteNodeType &&
        readable_range(node, kSpriteNodeDataOffset + sizeof(void*))) {
        void* sprite_data = *reinterpret_cast<void**>(
            reinterpret_cast<std::uint8_t*>(node) + kSpriteNodeDataOffset);
        if (!readable_range(sprite_data, 0x30)) {
            char node_name[96]{};
            char parent_name[96]{};
            copy_node_name(node, node_name, sizeof(node_name));

            void* parent = *reinterpret_cast<void**>(
                reinterpret_cast<std::uint8_t*>(node) + kNodeParentOffset);
            copy_node_name(parent, parent_name, sizeof(parent_name));

            const LONG incident =
                InterlockedIncrement(&g_invalid_nebula_node_count);
            if (incident <= 128) {
                char message[384]{};
                std::snprintf(
                    message, sizeof(message),
                    "Guarded invalid nebula sprite node #%ld: "
                    "name='%s', parent='%s', node=%p, spriteData=%p",
                    static_cast<long>(incident),
                    node_name[0] ? node_name : "<unnamed>",
                    parent_name[0] ? parent_name : "<unnamed>",
                    node, sprite_data);
                log_line(message);
            } else if (incident == 129) {
                log_line("Further invalid nebula sprite-node reports suppressed");
            }

            // The native function dereferences sprite_data+0x2c without a
            // null check. Returning skips only this invalid node's subtree;
            // its caller continues with the node's next sibling.
            return;
        }
    }

    original(node);
}

bool install_nebula_node_guard(const A2FO_ModuleApi* api) noexcept {
    if (!api || !api->armada_module || !api->install_inline_hook) return false;
    void* armada = api->armada_module();
    if (!armada) return false;
    return api->install_inline_hook(
        at(armada, kNebulaSetTexturesRecursiveRva),
        reinterpret_cast<void*>(&nebula_set_textures_recursive_hook),
        kNebulaSetTexturesRecursiveHookLength,
        kExpectedNebulaSetTexturesRecursive,
        &g_nebula_set_textures_recursive_hook);
}

void log_rtime_class_name(
    const char* serialized_name, const void* return_address,
    const void* file_reader, std::uintptr_t second_read_result) noexcept {
    if (!g_armada || !serialized_name ||
        !readable_range(serialized_name, kSerializedRtimeClassNameSize)) {
        return;
    }

    char name[kSerializedRtimeClassNameSize + 1]{};
    std::memcpy(name, serialized_name, kSerializedRtimeClassNameSize);
    name[kSerializedRtimeClassNameSize] = '\0';
    for (std::size_t index = 0;
         index < kSerializedRtimeClassNameSize && name[index]; ++index) {
        const unsigned char byte = static_cast<unsigned char>(name[index]);
        if (byte < 0x20 || byte >= 0x7f) name[index] = '?';
    }

    const auto find_class = reinterpret_cast<RtimeClassFindFn>(
        at(g_armada, kRtimeClassFindRva));
    if (!find_class || find_class(name)) return;

    const LONG incident = InterlockedIncrement(&g_missing_rtime_class_count);
    if (incident <= 32) {
        char bytes[(kSerializedRtimeClassNameSize * 3) + 1]{};
        std::size_t cursor = 0;
        for (std::size_t index = 0;
             index < kSerializedRtimeClassNameSize; ++index) {
            const int written = std::snprintf(
                bytes + cursor, sizeof(bytes) - cursor,
                index + 1 == kSerializedRtimeClassNameSize ? "%02X" : "%02X ",
                static_cast<unsigned char>(serialized_name[index]));
            if (written <= 0 ||
                static_cast<std::size_t>(written) >= sizeof(bytes) - cursor) {
                break;
            }
            cursor += static_cast<std::size_t>(written);
        }

        std::uintptr_t caller_rva = 0;
        if (return_address && g_armada) {
            const auto caller = reinterpret_cast<std::uintptr_t>(return_address);
            const auto base = reinterpret_cast<std::uintptr_t>(g_armada);
            if (caller >= base) caller_rva = caller - base;
        }

        unsigned binary_mode = 0;
        unsigned labelled_binary = 0;
        unsigned reader_status = 0;
        std::uintptr_t buffer_base = 0;
        std::uintptr_t buffer_size = 0;
        std::uintptr_t cursor_address = 0;
        std::uintptr_t end_address = 0;
        std::uintptr_t cursor_offset = 0;
        std::uintptr_t remaining = 0;
        char stream_bytes[193]{};

        if (readable_range(file_reader, kFileReaderInspectionSize)) {
            const auto* reader =
                static_cast<const std::uint8_t*>(file_reader);
            reader_status = reader[5];
            binary_mode = reader[6];
            labelled_binary = reader[7];
            std::memcpy(&buffer_base, reader + 0x0c, sizeof(buffer_base));
            std::memcpy(&buffer_size, reader + 0x10, sizeof(buffer_size));
            std::memcpy(&cursor_address, reader + 0x54,
                        sizeof(cursor_address));
            std::memcpy(&end_address, reader + 0x58, sizeof(end_address));

            if (buffer_base && cursor_address >= buffer_base) {
                cursor_offset = cursor_address - buffer_base;
            }
            if (end_address >= cursor_address) {
                remaining = end_address - cursor_address;
            }

            // A labelled binary read consumes its eight-byte type/size header
            // even when the header is wrong. RtimeClass::Load performs two
            // reads before this hook, so retain up to the preceding 16 bytes
            // and enough following data to identify the unexpected record.
            if (buffer_base && cursor_address >= buffer_base &&
                end_address >= cursor_address) {
                const std::uintptr_t available_before =
                    cursor_address - buffer_base;
                const std::uintptr_t before =
                    available_before < 16 ? available_before : 16;
                const std::uintptr_t available_after =
                    end_address - cursor_address;
                const std::uintptr_t after =
                    available_after < 48 ? available_after : 48;
                const auto* stream_start = reinterpret_cast<const char*>(
                    cursor_address - before);
                const std::size_t stream_size =
                    static_cast<std::size_t>(before + after);
                if (stream_size && readable_range(stream_start, stream_size)) {
                    std::size_t stream_cursor = 0;
                    for (std::size_t index = 0; index < stream_size; ++index) {
                        const int written = std::snprintf(
                            stream_bytes + stream_cursor,
                            sizeof(stream_bytes) - stream_cursor,
                            index + 1 == stream_size ? "%02X" : "%02X ",
                            static_cast<unsigned char>(stream_start[index]));
                        if (written <= 0 || static_cast<std::size_t>(written) >=
                                                sizeof(stream_bytes) -
                                                    stream_cursor) {
                            break;
                        }
                        stream_cursor += static_cast<std::size_t>(written);
                    }
                }
            }
        }

        char message[1024]{};
        std::snprintf(
            message, sizeof(message),
            "Missing serialized RtimeClass #%ld: name='%s', "
            "callerRva=0x%08lx, bytes=%s, reader=%p, "
            "status=%u, binary=%u, labelled=%u, secondReadAl=%u, "
            "bufferSize=%lu, cursor=%lu, remaining=%lu, stream=%s",
            static_cast<long>(incident), name[0] ? name : "<empty>",
            static_cast<unsigned long>(caller_rva), bytes, file_reader,
            reader_status, binary_mode, labelled_binary,
            static_cast<unsigned>(second_read_result & 0xff),
            static_cast<unsigned long>(buffer_size),
            static_cast<unsigned long>(cursor_offset),
            static_cast<unsigned long>(remaining),
            stream_bytes[0] ? stream_bytes : "<unavailable>");
        log_line(message);
    }
}

bool install_rtime_class_diagnostic(const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->install_inline_hook) return false;
    const bool installed = api->install_inline_hook(
        at(g_armada, kRtimeClassLoadNameRva),
        reinterpret_cast<void*>(&a2fo_a1_rtime_load_name_hook),
        kRtimeClassLoadNameHookLength,
        kExpectedRtimeClassLoadName,
        &g_rtime_class_load_name_hook);
    if (installed) {
        g_a2fo_a1_rtime_load_name_gateway =
            g_rtime_class_load_name_hook.gateway;
    }
    return installed;
}

void log_standard_text_sprite_lookup(
    const void* configuration_string, const void* sprite_string,
    const void* return_address, std::uintptr_t item_index,
    const void* lookup_result) noexcept {
    if (lookup_result) return;

    const LONG incident =
        InterlockedIncrement(&g_missing_standard_text_sprite_count);
    if (incident > 32) {
        if (incident == 33) {
            log_line("Further missing StandardText sprite reports suppressed");
        }
        return;
    }

    char configuration[160]{};
    char sprite[160]{};
    copy_tstring(configuration_string, configuration, sizeof(configuration));
    copy_tstring(sprite_string, sprite, sizeof(sprite));

    std::uintptr_t caller_rva = 0;
    if (return_address && g_armada) {
        const auto caller = reinterpret_cast<std::uintptr_t>(return_address);
        const auto base = reinterpret_cast<std::uintptr_t>(g_armada);
        if (caller >= base) caller_rva = caller - base;
    }

    char message[512]{};
    std::snprintf(
        message, sizeof(message),
        "Missing StandardText sprite #%ld: configuration='%s', "
        "sprite='%s', item=%lu, callerRva=0x%08lx",
        static_cast<long>(incident),
        configuration[0] ? configuration : "<unavailable>",
        sprite[0] ? sprite : "<empty>",
        static_cast<unsigned long>(item_index),
        static_cast<unsigned long>(caller_rva));
    log_line(message);
}

bool install_standard_text_sprite_diagnostic(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->install_inline_hook) return false;
    const bool installed = api->install_inline_hook(
        at(g_armada, kStandardTextSpriteLookupRva),
        reinterpret_cast<void*>(&a2fo_a1_standard_text_sprite_hook),
        kStandardTextSpriteLookupHookLength,
        kExpectedStandardTextSpriteLookup,
        &g_standard_text_sprite_lookup_hook);
    if (installed) {
        g_a2fo_a1_standard_text_sprite_gateway =
            g_standard_text_sprite_lookup_hook.gateway;
    }
    return installed;
}

void log_craft_level_up_race(
    const void* craft, const void* return_address,
    std::uintptr_t force_level_up, const void* race) noexcept {
    if (race) return;

    const LONG incident = InterlockedIncrement(&g_missing_craft_race_count);
    if (incident > 32) {
        if (incident == 33) {
            log_line("Further null Craft_mLevelUp race reports suppressed");
        }
        return;
    }

    void* object_class = read_pointer_at(craft, kGameObjectClassOffset);
    void* side = read_pointer_at(craft, kCraftSideOffset);
    void* side_race = read_pointer_at(side, kSideRaceOffset);
    void* craft_enhancement =
        read_pointer_at(craft, kCraftEnhancementOffset);
    const std::uintptr_t handle =
        read_uintptr_at(craft, kGameObjectHandleOffset);
    const std::uintptr_t team = read_uintptr_at(craft, kGameObjectTeamOffset);

    char odf_name[160]{};
    const auto* getter = at<std::uint8_t>(
        g_armada, kGameObjectClassGetOdfNameRva);
    if (object_class && getter &&
        readable_range(getter, sizeof(kExpectedGameObjectClassGetOdfName)) &&
        std::memcmp(
            getter, kExpectedGameObjectClassGetOdfName,
            sizeof(kExpectedGameObjectClassGetOdfName)) == 0) {
        const auto result = a2fo_a1_call_thiscall_0(
            const_cast<std::uint8_t*>(getter), object_class);
        copy_printable_string(
            reinterpret_cast<const char*>(result), odf_name,
            sizeof(odf_name));
    }

    std::uintptr_t caller_rva = 0;
    if (return_address && g_fleet_ops) {
        const auto caller = reinterpret_cast<std::uintptr_t>(return_address);
        const auto base = reinterpret_cast<std::uintptr_t>(g_fleet_ops);
        if (caller >= base) caller_rva = caller - base;
    }

    char message[640]{};
    std::snprintf(
        message, sizeof(message),
        "Null Craft_mLevelUp race #%ld: odf='%s', craft=%p, handle=%lu, "
        "team=%lu, side=%p, sideRace=%p, class=%p, craftEnhancement=%p, "
        "force=%lu, callerFleetOpsRva=0x%08lx",
        static_cast<long>(incident),
        odf_name[0] ? odf_name : "<unavailable>", craft,
        static_cast<unsigned long>(handle),
        static_cast<unsigned long>(team), side, side_race, object_class,
        craft_enhancement, static_cast<unsigned long>(force_level_up),
        static_cast<unsigned long>(caller_rva));
    log_line(message);
}

bool install_craft_level_up_race_diagnostic(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_fleet_ops || !api->install_inline_hook) return false;
    const bool installed = api->install_inline_hook(
        at(g_fleet_ops, kCraftLevelUpRaceRva),
        reinterpret_cast<void*>(&a2fo_a1_craft_level_up_race_hook),
        kCraftLevelUpRaceHookLength, kExpectedCraftLevelUpRace,
        &g_craft_level_up_race_hook);
    if (installed) {
        g_a2fo_a1_craft_level_up_race_gateway =
            g_craft_level_up_race_hook.gateway;
    }
    return installed;
}

std::string join_path(const char* root, const char* name) {
    if (!root || !*root) return {};
    std::string path(root);
    if (path.back() != '\\' && path.back() != '/') path.push_back('\\');
    path += name;
    return path;
}

bool enabled_in_mod_chain() noexcept {
    if (!g_api || !g_api->extension_root_count || !g_api->extension_root) {
        return false;
    }
    const std::uint32_t count = g_api->extension_root_count();
    if (count == 0 || count > 4096) return false;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::string marker =
            join_path(g_api->extension_root(index), "a1compat.ini");
        if (marker.empty()) continue;
        const DWORD attributes = GetFileAttributesA(marker.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

extern "C" void __cdecl a2fo_a1_log_rtime_class_name(
    const char* serialized_name, const void* return_address,
    const void* file_reader, std::uintptr_t second_read_result) {
    log_rtime_class_name(serialized_name, return_address, file_reader,
                         second_read_result);
}

extern "C" void __cdecl a2fo_a1_log_standard_text_sprite_lookup(
    const void* configuration_string, const void* sprite_string,
    const void* return_address, std::uintptr_t item_index,
    const void* lookup_result) {
    log_standard_text_sprite_lookup(
        configuration_string, sprite_string, return_address, item_index,
        lookup_result);
}

extern "C" void __cdecl a2fo_a1_log_craft_level_up_race(
    const void* craft, const void* return_address,
    std::uintptr_t force_level_up, const void* race) {
    log_craft_level_up_race(
        craft, return_address, force_level_up, race);
}

extern "C" void __cdecl a2fo_a1_prepare_starbase_officer_quarters(
    void* starbase) {
    prepare_starbase_officer_quarters(starbase);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->install_inline_hook ||
        !api->extension_root_count || !api->extension_root ||
        !api->register_classlabel_alias ||
        !A2FO_MODULE_API_HAS(api, register_classlabel_odf_defaults) ||
        !A2FO_MODULE_API_HAS(api, register_odf_overlay_directory) ||
        !A2FO_MODULE_API_HAS(api, register_producer_event_handler) ||
        api->api_revision < 10 ||
        (api->capabilities & A2FO_CAP_CLASSLABEL_ODF_DEFAULTS) == 0 ||
        (api->capabilities & A2FO_CAP_ODF_OVERLAY_DIRECTORIES) == 0 ||
        (api->capabilities & A2FO_CAP_PRODUCER_EVENTS) == 0 ||
        !api->register_classlabel_odf_defaults ||
        !api->register_odf_overlay_directory ||
        !api->register_producer_event_handler) {
        return false;
    }

    g_api = api;
    g_armada = api->armada_module();
    g_fleet_ops = api->fleetops_module();
    if (!enabled_in_mod_chain()) {
        log_line("STA1 Classic marker not found; A1 policy remains inactive");
        g_api = nullptr;
        return false;
    }
    if (!api->register_classlabel_alias(
            kModuleName, "wingman", "craft")) {
        log_line("wingman -> craft registration failed");
        g_api = nullptr;
        return false;
    }
    if (!api->register_classlabel_odf_defaults(
            kModuleName, "wingman", kWingmanOdfDefaults.data(),
            static_cast<std::uint32_t>(kWingmanOdfDefaults.size()))) {
        log_line("wingman ODF-default registration failed");
        g_api = nullptr;
        return false;
    }
    if (!api->register_classlabel_odf_defaults(
            kModuleName, "constructionrig",
            kConstructionRigOdfDefaults.data(),
            static_cast<std::uint32_t>(
                kConstructionRigOdfDefaults.size()))) {
        log_line("constructionrig ODF-default registration failed");
        g_api = nullptr;
        return false;
    }
    if (!api->register_classlabel_odf_defaults(
            kModuleName, "freighter", kFreighterOdfDefaults.data(),
            static_cast<std::uint32_t>(kFreighterOdfDefaults.size()))) {
        log_line("freighter ODF-default registration failed");
        g_api = nullptr;
        return false;
    }
    if (!api->register_odf_overlay_directory(
            kModuleName, "Addon", A2FO_ODF_OVERLAY_OVERRIDE)) {
        log_line("Addon ODF overlay registration failed");
        g_api = nullptr;
        return false;
    }
    A1CompatSettings settings{};
    const bool has_settings = read_a1compat_settings(settings);
    if (has_settings) {
        char safe_mode_message[256];
        std::snprintf(
            safe_mode_message, sizeof(safe_mode_message),
            "A1Compat settings loaded (%s): SafeMode=%s",
            kA1CompatIniFileName,
            settings.safe_mode ? "true" : "false");
        log_line(safe_mode_message);
    }
    g_constructor_menu_capability_count = 0;
    const bool safe_mode_enabled = settings.safe_mode;
    if (safe_mode_enabled) {
        log_line("A1Compat safe mode active; risky hooks disabled");
    }
    InitializeCriticalSection(&g_officer_state_lock);
    g_officer_state_lock_ready = true;
    try {
        g_starbase_class_policies.reserve(64);
        g_starbase_officer_states.reserve(256);
        g_officer_upgrade_races.reserve(32);
    } catch (...) {
        log_line("A1 officer state preallocation failed; dynamic allocation "
                 "will be used");
    }
    bool nebula_guard_enabled = false;
    bool rtime_diagnostic_enabled = false;
    bool standard_text_sprite_diagnostic_enabled = false;
    bool craft_level_up_race_diagnostic_enabled = false;
    bool officer_quarter_compatibility_enabled = false;
    bool starbase_policy_system_enabled = false;
    bool palette_visibility_diagnostic_enabled = false;

    if (!safe_mode_enabled) {
        const bool producer_events_registered =
            api->register_producer_event_handler(
                kModuleName, &officer_producer_event_handler, nullptr);
        g_producer_events_ready = producer_events_registered;
        if (!producer_events_registered) {
            log_line("A1 Producer event registration failed");
        }
        nebula_guard_enabled = install_nebula_node_guard(api);
        if (!nebula_guard_enabled) {
            log_line("Nebula sprite-node diagnostic guard installation failed");
        }
        rtime_diagnostic_enabled = install_rtime_class_diagnostic(api);
        if (!rtime_diagnostic_enabled) {
            log_line("Serialized RtimeClass diagnostic installation failed");
        }
        standard_text_sprite_diagnostic_enabled =
            install_standard_text_sprite_diagnostic(api);
        if (!standard_text_sprite_diagnostic_enabled) {
            log_line("StandardText sprite diagnostic installation failed");
        }
        craft_level_up_race_diagnostic_enabled =
            install_craft_level_up_race_diagnostic(api);
        if (!craft_level_up_race_diagnostic_enabled) {
            log_line("Craft_mLevelUp race diagnostic installation failed");
        }
        officer_quarter_compatibility_enabled =
            install_starbase_officer_quarter_compatibility(api);
        if (!officer_quarter_compatibility_enabled) {
            log_line("A1 officer-quarter compatibility installation failed");
        }

        starbase_policy_system_enabled = install_officer_upgrade_system(api);
        if (!starbase_policy_system_enabled) {
            log_line("A1 Starbase policy/menu installation failed");
        }
        palette_visibility_diagnostic_enabled =
            install_producer_palette_visibility_diagnostic(api);
        if (!palette_visibility_diagnostic_enabled) {
            log_line("A1 Producer palette visibility diagnostic installation "
                     "failed");
        }
    } else {
        g_producer_events_ready = false;
    }

    char message[640]{};
    std::snprintf(
        message, sizeof(message),
        "Armada 1 compatibility initialized: wingman -> craft; Addon ODF "
        "overlay; nebula sprite-node guard %s; serialized RtimeClass "
        "diagnostic %s; StandardText sprite diagnostic %s; Craft_mLevelUp "
        "race diagnostic %s; oqN visibility %s; "
        "Starbase policy/menu %s; officer target identity %s; officer "
        "completion bridge %s; "
        "Producer palette visibility diagnostic %s; invalid GetProjectId "
        "detour removed",
        nebula_guard_enabled ? "enabled" : "unavailable",
        rtime_diagnostic_enabled ? "enabled" : "unavailable",
        standard_text_sprite_diagnostic_enabled ? "enabled" : "unavailable",
        craft_level_up_race_diagnostic_enabled ? "enabled" : "unavailable",
        officer_quarter_compatibility_enabled ? "enabled" : "unavailable",
        starbase_policy_system_enabled ? "enabled" : "unavailable",
        g_officer_upgrade_identity_ready ? "enabled" : "unavailable",
        g_officer_upgrade_completion_ready ? "enabled" : "unavailable",
        palette_visibility_diagnostic_enabled ? "enabled" : "unavailable");
    log_line(message);
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    log_line("Armada 1 compatibility module shutting down");
    if (g_starbase_finish_build_vtable_hook_installed &&
        g_starbase_finish_build_vtable_slot &&
        g_starbase_finish_build_original &&
        readable_range(g_starbase_finish_build_vtable_slot, sizeof(void*)) &&
        *g_starbase_finish_build_vtable_slot ==
            reinterpret_cast<void*>(&starbase_finish_build_hook)) {
        DWORD old_protect = 0;
        if (VirtualProtect(
                g_starbase_finish_build_vtable_slot, sizeof(void*),
                PAGE_READWRITE, &old_protect)) {
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(
                    g_starbase_finish_build_vtable_slot),
                g_starbase_finish_build_original);
            DWORD restored = 0;
            VirtualProtect(
                g_starbase_finish_build_vtable_slot, sizeof(void*),
                old_protect, &restored);
        }
    }
    g_starbase_finish_build_vtable_slot = nullptr;
    g_starbase_finish_build_original = nullptr;
    g_starbase_finish_build_vtable_hook_installed = false;
    g_officer_upgrade_completion_ready = false;
    g_officer_upgrade_identity_ready = false;
    g_producer_events_ready = false;
    g_officer_upgrade_system_ready = false;
    g_constructor_menu_capability_count = 0;
    g_api = nullptr;
    g_armada = nullptr;
    g_fleet_ops = nullptr;
}
