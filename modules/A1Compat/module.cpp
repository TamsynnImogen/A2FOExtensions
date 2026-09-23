/*
 * File: modules/A1Compat/module.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Armada 1 classlabels, officer progression, and compatibility shims.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "a1_bzn_policy.hpp"
#include "a1_ui_policy.hpp"
#include "race_menu_policy.hpp"
#include "team_color_policy.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
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
#include <vector>

extern "C" {
void* g_a2fo_a1_aip_lookup_gateway = nullptr;
void* g_a2fo_a1_craft_level_up_race_gateway = nullptr;
void* g_a2fo_a1_craft_level_up_race_continuation = nullptr;
void* g_a2fo_a1_starbase_initialize_geometry_gateway = nullptr;
void* g_a2fo_a1_to_the_death_check_gateway = nullptr;
void* g_a2fo_a1_aip_technology_unit_continuation = nullptr;
void* g_a2fo_a1_aip_technology_unit_skip = nullptr;
void* g_a2fo_a1_gui_parameter_db_post_construct_gateway = nullptr;
void a2fo_a1_gui_sprite_read_table_hook();
void a2fo_a1_gui_parameter_db_post_construct_hook();
void a2fo_a1_game_object_resource_lookup_hook();
void a2fo_a1_physics_model_lookup_hook();
void a2fo_a1_race_count_lookup_hook();
void a2fo_a1_race_entry_lookup_hook();
void a2fo_a1_aip_lookup_hook();
void a2fo_a1_craft_level_up_race_hook();
void a2fo_a1_starbase_initialize_geometry_hook();
void a2fo_a1_to_the_death_check_hook();
void a2fo_a1_aip_technology_unit_guard_hook();
void a2fo_a1_energy_bar_colour_hook();
void a2fo_a1_energy_bar_values_hook();
std::uintptr_t __cdecl a2fo_a1_load_gui_sprite_tables(
    void* parser, void* database, const char* primary_filename);
void __cdecl a2fo_a1_configure_gui_parameter_db(
    void* parameter_db, const char* configuration_filename);
std::uintptr_t __cdecl a2fo_a1_resolve_race_count(
    void* parameter_db, const char* key, std::int32_t* output,
    std::int32_t default_value);
std::uintptr_t __cdecl a2fo_a1_resolve_race_entry(
    void* parameter_db, const char* key, char* output,
    std::uintptr_t output_size, const char* default_value);
std::uintptr_t __cdecl a2fo_a1_resolve_physics_model(
    void* parameter_db, const char* key, char* output,
    std::uintptr_t output_size, const char* default_value);
std::uintptr_t __cdecl a2fo_a1_resolve_physics_speed(
    void* parameter_db, std::uintptr_t context, const char* key,
    float* output, float default_value);
void __cdecl a2fo_a1_translate_smooth_float(
    void* primary_db, void* fallback_db, const char* key, float* output);
void __cdecl a2fo_a1_translate_smooth_integer(
    void* primary_db, void* fallback_db, const char* key,
    std::int32_t* output);
std::uintptr_t __cdecl a2fo_a1_read_rtime_class_name(
    void* file_reader, void* output, std::uint32_t requested_size);
std::uintptr_t __cdecl a2fo_a1_load_game_objects(void* file_reader);
std::uintptr_t __cdecl a2fo_a1_load_a2_craft_class_table(
    void* file_reader);
std::uintptr_t __cdecl a2fo_a1_load_ai_mission(void* file_reader);
std::uintptr_t __cdecl a2fo_a1_load_map_details(const char* filename);
std::uintptr_t __cdecl a2fo_a1_load_selected_map_details(
    const char* filename);
std::uintptr_t __cdecl a2fo_a1_resolve_aip_lookup(
    void* manager, const char* requested_name);
void __cdecl a2fo_a1_report_missing_aip_technology_unit(
    const char* aip_name, const char* unit_name);
void __cdecl a2fo_a1_log_craft_level_up_race(
    const void* craft, const void* return_address,
    std::uintptr_t force_level_up, const void* race);
void __cdecl a2fo_a1_prepare_starbase_officer_quarters(void* starbase);
void __cdecl a2fo_a1_run_to_the_death_check(
    void* game_type, const void* return_address);
}

namespace {

constexpr const char* kModuleName = "A1Compat";
// Match A2FOCheats' accepted RTS_CFG.h size so both consumers resolve the
// same authoritative showmethemoney values.
constexpr std::size_t kMaximumConfigTextSize = 2 * 1024 * 1024;
constexpr char kA1CompatIniFileName[] = "a1compat.ini";
constexpr char kA1CompatSafeModeKey[] = "safemode";
constexpr char kRtsConfigFileName[] = "RTS_CFG.h";
constexpr std::int32_t kDefaultStartingResourceAmount = 10000;
constexpr double kMaximumStartingResourceAmount = 100000000.0;

// Raw A1 RTS_CFG.h files use cfgMax* names. When such a file shadows the
// complete A2 configuration, Armada 2 leaves these native float maxima at
// zero; every later resource mutation is therefore clamped to zero.
constexpr std::uintptr_t kMaximumDilithiumRva = 0x00338df0;
constexpr std::uintptr_t kMaximumCrewRva = 0x00338dec;
constexpr std::uintptr_t kMaximumOfficersRva = 0x00338de8;
constexpr std::uintptr_t kTeamAddDilithiumRva = 0x00096e30;
constexpr std::uintptr_t kTeamAddCrewRva = 0x00096f20;
constexpr std::uintptr_t kTeamAddOfficersRva = 0x00097010;
constexpr std::uint8_t kExpectedTeamResourceMutator[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x8a};

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
constexpr std::array<A2FO_ClasslabelOdfDefault, 10>
    kConstructionRigOdfDefaults{{
        {"shipclass", "construction"},
        {"builder_facility", "1"},
        {"ship", "1"},
        {"has_hitpoints", "1"},
        {"has_crew", "1"},
        {"transporter", "1"},
        {"SHOW_MOVEMENT_AUTONOMY", "1"},
        {"SHOW_SW_AUTONOMY", "1"},
        {"shipType", "N"},
        {"hotkeyLabel", "HOTKEY_F1"},
    }};

// A1 freighters inherit these mining/resource identity commands from the
// shared a2freight.odf template. Preserve them for freighter ODFs which omit
// that include while leaving local and inherited values authoritative.
constexpr std::array<A2FO_ClasslabelOdfDefault, 11>
    kFreighterOdfDefaults{{
        {"shipclass", "mining"},
        {"maxDilithium", "150"},
        {"alert", "1"},
        {"miner", "1"},
        {"ship", "1"},
        {"has_hitpoints", "1"},
        {"has_crew", "1"},
        {"transporter", "1"},
        {"SHOW_MOVEMENT_AUTONOMY", "1"},
        {"resourcesCanHandle", "dilithium"},
        {"hotkeyLabel", "HOTKEY_F2"},
    }};

// Armada 2 makes the resource accepted by a mining station explicit. Raw A1
// mining ODFs rely on the class' implicit Dilithium behaviour instead, so the
// freighter can dock but its cargo is not credited by the A2 implementation.
// Restore the corresponding A2 mining-station contract as missing-only data.
constexpr std::array<A2FO_ClasslabelOdfDefault, 3>
    kMiningOdfDefaults{{
        {"transporter", "1"},
        {"alert", "0"},
        {"resourcesCanHandle", "dilithium"},
    }};

// Armada 1 shipyards predate the context-menu capability commands added by
// Armada 2. Their native shipyard class still owns the construction logic,
// but Fleet Operations will not expose its ship-production palette unless
// these missing A2 capabilities are present. Match the common A2 shipyard
// contract without replacing values supplied by a converted mod.
constexpr std::array<A2FO_ClasslabelOdfDefault, 4>
    kShipyardOdfDefaults{{
        {"builder_ship", "1"},
        {"transporter", "1"},
        {"SHOW_MOVEMENT_AUTONOMY", "1"},
        {"SHOW_SW_AUTONOMY", "1"},
    }};

// A2 research stations add the context-menu capability and transporter flag
// to the otherwise shared Research class contract. Keep both as missing-only
// classlabel defaults so raw A1 research facilities expose their native pod
// list through Fleet Operations while explicit converted-mod settings remain
// authoritative.
constexpr std::array<A2FO_ClasslabelOdfDefault, 2>
    kResearchOdfDefaults{{
        {"research", "1"},
        {"transporter", "1"},
    }};

// `scout = 1` and `is_starbase = 1` survive from A1 as behavioural markers,
// but their shared A1 base ODFs predate A2's context-menu capability fields.
// A1 resource moons likewise lack A2's spatial_object and has_resource target
// flags. Capture the original declarations from each completed include chain
// so the compatibility callback can add only genuinely missing A2 values.
constexpr std::array<const char*, 17> kLegacyMenuCapabilityOdfFields{{
    "scout", "combat", "alert", "can_sandd", "can_explore",
    "is_starbase", "transporter", "facility", "has_crew", "has_hitpoints",
    "spatial_object", "has_resource", "builder_ship",
    "maximumUpgrades", "officerGain", "race", "classLabel"}};

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
// Starbase overrides Producer::mStartConstructionEffect at RVA 0x000bbe90:
// the live vtable slot checks its derived state and then tail-jumps to the
// generic Producer routine. OfficerUpgradeClass is an in-place upgrade and
// has no renderable Craft/SOD, so sending it through that path creates an
// invalid cosmetic instance which later crashes in CraftInstance rendering.
// Patch only the live Starbase override slot. Ordinary Starbase calls chain
// through the override, including any optional HybridBuild detour installed
// on the generic Producer entry.
constexpr std::uintptr_t kStarbaseStartConstructionEffectRva = 0x000bbe90;
constexpr std::size_t kStarbaseStartConstructionEffectVtableOffset = 0x16c;
constexpr std::uintptr_t kStarbaseClearTeamRva = 0x000bda30;
constexpr std::uintptr_t kStarbaseSetTeamRva = 0x000bda70;
constexpr std::uintptr_t kStarbaseLoadRva = 0x000bdaa0;
constexpr std::uintptr_t kStarbaseSaveRva = 0x000bdae0;
// Use the already validated ParameterDB::GetString entry for A1 numeric
// commands. RVA 0x00135200 is ParameterDB::GetProjectId, not GetInt; detouring
// it corrupts class project-ID registration and must never be used here.
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kParameterDbGetIntRva = 0x00134bf0;
constexpr std::uintptr_t kParameterDbGetFloatRva = 0x00136070;
constexpr std::uint8_t kExpectedParameterDbGetInt[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00};
constexpr std::uint8_t kExpectedParameterDbGetString[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00};
constexpr std::uint8_t kExpectedParameterDbGetFloat[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x02, 0x00, 0x00};
// PhysicsClass rejects a freshly parsed physics file when its A2-only
// combatSpeed is zero, and selects its movement implementation through the
// A2-only `physics` string. Raw A1 files instead have one impulseSpeed and an
// optional borgPhysics boolean. Preserve declared/inherited A2 values, but
// translate those two legacy omissions at their exact native lookup sites.
constexpr std::uintptr_t kPhysicsClassCombatSpeedValidationCallRva = 0x000c073f;
constexpr std::uint8_t kExpectedPhysicsClassCombatSpeedValidationCall[] = {
    0xe8, 0x2c, 0x59, 0x07, 0x00};
// PhysicsClass first validates combatSpeed above, then performs a second
// lookup into the actual CraftClass field. Both calls must use the A1
// translation: patching only the validation lets the file load but leaves
// CraftClass::combatSpeed at its inherited -1 sentinel. SmoothPhysics then
// scales acceleration and turning by -1 and the craft cannot move.
constexpr std::uintptr_t kPhysicsClassCombatSpeedValueCallRva = 0x000c0774;
constexpr std::uint8_t kExpectedPhysicsClassCombatSpeedValueCall[] = {
    0xe8, 0xf7, 0x58, 0x07, 0x00};
constexpr std::uintptr_t kPhysicsClassImpulseSpeedLookupCallRva = 0x000c078f;
constexpr std::uint8_t kExpectedPhysicsClassImpulseSpeedLookupCall[] = {
    0xe8, 0xdc, 0x58, 0x07, 0x00};
constexpr std::uintptr_t kPhysicsClassWarpSpeedLookupCallRva = 0x000c07aa;
constexpr std::uint8_t kExpectedPhysicsClassWarpSpeedLookupCall[] = {
    0xe8, 0xc1, 0x58, 0x07, 0x00};
constexpr std::uintptr_t kPhysicsClassModelLookupCallRva = 0x000c07c7;
constexpr std::uint8_t kExpectedPhysicsClassModelLookupCall[] = {
    0xe8, 0x84, 0x4b, 0x07, 0x00};
// Raw A1 physics files do not contain the SmoothPhysics controller fields
// used by A2's Federation and other ordinary craft. Translate those missing
// fields at the exact SmoothPhysics::ReadParameters cascade calls. Profiles
// are selected by the shared physics-file family, with the A2 Federation
// destroyer profile as the safe default.
constexpr std::uintptr_t kPhysicsFloatCascadeRva = 0x00136360;
constexpr std::uint8_t kExpectedPhysicsFloatCascade[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x04, 0x02, 0x00, 0x00};
constexpr std::uintptr_t kPhysicsIntCascadeRva = 0x001368f0;
constexpr std::uint8_t kExpectedPhysicsIntCascade[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};
struct PhysicsParameterCallPatch {
    std::uintptr_t rva;
    std::array<std::uint8_t, 5> expected;
};
constexpr std::array<PhysicsParameterCallPatch, 15>
    kSmoothFloatCallPatches{{
        {0x000a9399, {0xe8, 0xc2, 0xcf, 0x08, 0x00}},
        {0x000a93a9, {0xe8, 0xb2, 0xcf, 0x08, 0x00}},
        {0x000a93ce, {0xe8, 0x8d, 0xcf, 0x08, 0x00}},
        {0x000a93de, {0xe8, 0x7d, 0xcf, 0x08, 0x00}},
        {0x000a9406, {0xe8, 0x55, 0xcf, 0x08, 0x00}},
        {0x000a9416, {0xe8, 0x45, 0xcf, 0x08, 0x00}},
        {0x000a943b, {0xe8, 0x20, 0xcf, 0x08, 0x00}},
        {0x000a9457, {0xe8, 0x04, 0xcf, 0x08, 0x00}},
        {0x000a946a, {0xe8, 0xf1, 0xce, 0x08, 0x00}},
        {0x000a947a, {0xe8, 0xe1, 0xce, 0x08, 0x00}},
        {0x000a948a, {0xe8, 0xd1, 0xce, 0x08, 0x00}},
        {0x000a949a, {0xe8, 0xc1, 0xce, 0x08, 0x00}},
        {0x000a94c1, {0xe8, 0x9a, 0xce, 0x08, 0x00}},
        {0x000a94ed, {0xe8, 0x6e, 0xce, 0x08, 0x00}},
        {0x000a950f, {0xe8, 0x4c, 0xce, 0x08, 0x00}},
    }};
constexpr PhysicsParameterCallPatch kSmoothIntegerCallPatch{
    0x000a94dd, {0xe8, 0x0e, 0xd4, 0x08, 0x00}};
constexpr LONG kMaximumLegacyPhysicsDefaultReports = 64;
// GameObjectClass asks ParameterDB for the optional A2 `resource` command at
// this one direct CALL. Raw A1 moon ODFs predate that command, while inherited
// A2 maps serialize the corresponding resource object. Supply the A2 Classic
// value only for the matching Scrap/moon ODF family after normal lookup fails.
constexpr std::uintptr_t kGameObjectClassResourceLookupCallRva = 0x000ccebb;
constexpr std::uint8_t kExpectedGameObjectClassResourceLookupCall[] = {
    0xe8, 0x90, 0x84, 0x06, 0x00};
constexpr std::size_t kParameterDbProjectIdOffset = 0x34;
constexpr std::uintptr_t kProjectIdGetOdfNameRva = 0x002593a0;
constexpr std::uint8_t kExpectedProjectIdGetOdfName[] = {
    0x8b, 0x01, 0x85, 0xc0, 0x75, 0x06};
constexpr LONG kMaximumLegacyMoonResourceReports = 32;
constexpr std::uintptr_t kFileOutBytesRva = 0x0012c680;
constexpr std::uintptr_t kFileInBytesRva = 0x0012d7a0;
constexpr std::uintptr_t kOfficerUpgradeClassBuildClassRva = 0x000ce910;
constexpr std::uintptr_t kOfficerUpgradeClassVtableRva = 0x002b4144;
constexpr std::uintptr_t kProducerPushBuildQueueItemRva = 0x000b7930;
constexpr std::uintptr_t kProducerPopBuildQueueItemRva = 0x000b79b0;
constexpr std::size_t kOfficerUpgradeGainOffset = 0x1e0;
constexpr std::size_t kTeamMaximumOfficersOffset = 0x164;
constexpr std::size_t kMaximumProducerQueueWalk = 10;
constexpr std::size_t kProducerClassBuildItemsOffset = 0x450;
constexpr std::size_t kProducerClassBuildItemCapacity = 57;
constexpr std::size_t kObjectClassProjectIdOffset = 0x1cc;
constexpr std::size_t kGameObjectClassMenuCapabilitiesOffset = 0x1d4;
constexpr std::uint32_t kBuilderShipMenuCapability = 0x00000080u;
constexpr std::uint32_t kCombatMenuCapability = 0x00000001u;
constexpr std::uint32_t kFacilityMenuCapability = 0x00000004u;
constexpr std::uint32_t kAlertMenuCapability = 0x00000008u;
constexpr std::uint32_t kTransporterMenuCapability = 0x00000010u;
constexpr std::uint32_t kSpatialObjectMenuCapability = 0x00000200u;
constexpr std::uint32_t kHasResourceMenuCapability = 0x00000400u;
constexpr std::uint32_t kHasHitpointsMenuCapability = 0x00000800u;
constexpr std::uint32_t kHasCrewMenuCapability = 0x00001000u;
constexpr std::uint32_t kSearchAndDestroyMenuCapability = 0x04000000u;
constexpr std::uint32_t kExploreMenuCapability = 0x08000000u;
constexpr std::size_t kMaximumLoggedStarbaseBuildItems = 12;

// A1's shared scout ship base is named scout.odf, colliding with A2's
// allcommands entry of the same basename. Fleet Operations' flat ODF lookup
// can therefore construct the Explore CommandInfo from the ship ODF. Repair
// only that missing command after normal parsing; a real command definition
// with a non-empty buttonName remains authoritative.
constexpr std::uintptr_t kCommandInfoBuildClassRva = 0x00119070;
constexpr std::uint8_t kExpectedCommandInfoBuildClass[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uintptr_t kCommandNameToIdRva = 0x0004e1d0;
constexpr std::uint8_t kExpectedCommandNameToId[] = {
    0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x57};
constexpr std::size_t kCommandInfoButtonNameOffset = 0x20;
constexpr std::size_t kCommandInfoTooltipOffset = 0x60;
constexpr std::size_t kCommandInfoVerboseOffset = 0xa0;
constexpr std::size_t kCommandInfoCommandIdOffset = 0x1a0;
constexpr std::size_t kCommandInfoSourceTypeOrOffset = 0x1a5;
constexpr std::size_t kCommandInfoNeedsTargetOffset = 0x1a4;
constexpr std::size_t kCommandInfoParamOffset = 0x1a8;
constexpr std::size_t kCommandInfoDisplayTradeOffset = 0x1ac;
constexpr std::size_t kCommandInfoIsBuyOffset = 0x1ad;
constexpr std::size_t kCommandInfoIsToggleOffset = 0x1ae;
constexpr std::size_t kCommandInfoPreferredPositionOffset = 0x1b0;
constexpr std::size_t kCommandInfoSourceOffset = 0x1c0;
constexpr std::size_t kCommandInfoSourceNotOffset = 0x1c4;
constexpr std::size_t kCommandInfoDestinationOffset = 0x1c8;
constexpr std::size_t kCommandInfoMenuOffset = 0x1cc;

// Fleet Ops filters each parsed Producer slot immediately before binding its
// palette button. This register-ABI helper receives EAX=ProducerClass,
// EDX=team index, and ECX=build-item slot. Preserve the native decision while
// hiding officer upgrades that do not match the owning A1 starbase race.
constexpr std::uintptr_t kProducerBuildButtonVisibleRva = 0x0011d8f8;
constexpr std::size_t kProducerBuildButtonVisibleHookLength = 5;
constexpr std::uint8_t kExpectedProducerBuildButtonVisible[] = {
    0x53, 0x56, 0x51, 0x8b, 0xf1};

// Fleet Operations' checked Producer queue path calls the native Armada
// Producer::PushBuildQueueItem through this writable import-target cell. Hook
// the cell rather than either public function prologue: FeaturePack can still
// own its global command wrapper when present, while an A1-only module chain
// gains the same wrong-race and maximum-upgrade admission policy before the
// native queue insertion and resource charge.
constexpr std::uintptr_t kFoProducerPushTargetCarrierRva = 0x00212c44;
constexpr std::uintptr_t kFoProducerPushTargetSlotRva = 0x00210d40;

// Stock Armada 1 configuration files commonly retain relative directory
// values such as ".\\AI". Fleet Operations extracts that prefix unchanged
// before asking TFOFS for a virtual directory, although its registry contains
// the canonical "AI\\..." name. Normalize the managed lookup string at the
// common boundary so all native callers retain their original paths while A1
// data remains compatible. FleetOpsHook.map lists this routine at 0x108cc8;
// the PE .text section contributes the additional 0x1000 RVA.
constexpr std::uintptr_t kFofsGetVirtualDirectoryRva = 0x00109cc8;
constexpr std::size_t kFofsGetVirtualDirectoryHookLength = 6;
constexpr std::uint8_t kExpectedFofsGetVirtualDirectory[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf8};
constexpr std::uintptr_t kFleetOpsLStrFromPCharRva = 0x000058b0;
constexpr std::uintptr_t kFleetOpsLStrClearRva = 0x000056b8;
constexpr LONG kMaximumVirtualDirectoryNormalizationReports = 32;
constexpr std::uintptr_t kTeamTechnologyTreesPointerRva = 0x00212f08;
constexpr std::size_t kTechnologyTreeItemsOffset = 0x0c;
constexpr std::size_t kTechnologyItemActiveBuildsOffset = 0x18;
constexpr std::size_t kMaximumPaletteTeamIndex = 63;
constexpr std::size_t kProducerCurrentBuildClassOffset = 0x254;
constexpr std::size_t kProducerLastBuiltHandleOffset = 0x26c;
constexpr std::size_t kProducerCurrentQueueIdOffset = 0x2a0;
constexpr std::size_t kProducerStopConstructionEffectVtableOffset = 0x178;

constexpr std::uint8_t kExpectedStarbaseClassBuildClass[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uint8_t kExpectedStarbaseFinishBuild[] = {
    0x53, 0x56, 0x57, 0x8b, 0xf1};
constexpr std::uint8_t kExpectedStarbaseStartConstructionEffect[] = {
    0x8b, 0x81, 0xc0, 0x02, 0x00, 0x00, 0x85, 0xc0};
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
constexpr std::uint8_t kExpectedProducerPushBuildQueueItem[] = {
    0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1};
constexpr std::uint8_t kExpectedProducerPopBuildQueueItem[] = {
    0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb};

constexpr std::size_t kQueueHeadOffset = 0x270;
constexpr std::size_t kQueueItemNextOffset = 0x08;
constexpr std::size_t kCraftTeamPointerOffset = 0xf0;

constexpr std::uint32_t kOfficerSaveMagic = 0x514f3141u; // "A1OQ"
constexpr std::uint32_t kOfficerSaveVersion = 1;
constexpr char kOfficerSaveLabel[] = "A1Compat officer quarters";
constexpr std::uintptr_t kMinimumSafeAddress = 0x1000;

// Stock Armada 2 Race layout and globals retained by the supported
// ArmadaL.exe. Fleet Operations uses instantActionSlot at Race+0x44c and
// bounds Race::FindInstantActionSlot through the global count at 0x737cb4.
// The native displayKey buffer is populated with a 0x40-byte GetString call.
constexpr std::size_t kRaceDisplayKeyOffset = 0x54;
constexpr std::size_t kRaceDisplayKeyCapacity = 0x40;
constexpr std::size_t kRaceIdentifierOffset = 0x448;
constexpr std::size_t kRaceInstantActionSlotOffset = 0x44c;
// RaceLoader stores two rows (normal, lots) of Armada's six native resources
// at Race+0x5dc. The resource columns are Crew, Officers, Dilithium, Latinum,
// Metal, and Biomatter. A2FOResources consumes Tritanium and Supply from the
// same Race-loaded ODF snapshot rather than this native matrix.
constexpr std::size_t kRaceStartingResourcesOffset = 0x5dc;
constexpr std::size_t kNativeStartingResourceCount = 6;
constexpr std::size_t kNativeCrewResourceIndex = 0;
constexpr std::size_t kNativeDilithiumResourceIndex = 2;
constexpr std::size_t kNativeMetalResourceIndex = 4;
constexpr std::uintptr_t kRaceNumberOfInstantActionSlotsRva = 0x00337cb4;
constexpr std::uintptr_t kRaceNumberOfRacesRva = 0x00337cb8;
constexpr std::int32_t kMaximumLegacyRaceRecords = 4096;
constexpr std::uintptr_t kRaceInitAllNumberOfRacesCallRva = 0x0008ac70;
constexpr std::uint8_t kExpectedRaceInitAllNumberOfRacesCall[] = {
    0xe8, 0x7b, 0x9f, 0x0a, 0x00};
constexpr std::uintptr_t kRaceInitAllRaceEntryCallRva = 0x0008acc5;
constexpr std::uint8_t kExpectedRaceInitAllRaceEntryCall[] = {
    0xe8, 0x86, 0xa6, 0x0a, 0x00};
constexpr char kNeutralRaceOdfName[] = "norace.odf";
constexpr std::array<const char*, 14> kLegacyRaceFields{{
    "instantActionSlot", "interfaceConfiguration", "displayKey",
    "displayName", "normalCrew", "normalDilithium", "normalMetal",
    "normalTritanium", "normalSupply", "lotsCrew", "lotsDilithium",
    "lotsMetal", "lotsTritanium", "lotsSupply",
}};

constexpr std::array<const char*, 5> kNormalStartingResourceCommands{{
    "normalCrew", "normalDilithium", "normalMetal", "normalTritanium",
    "normalSupply",
}};
constexpr std::array<const char*, 5> kLotsStartingResourceCommands{{
    "lotsCrew", "lotsDilithium", "lotsMetal", "lotsTritanium",
    "lotsSupply",
}};

// Fleet Operations' initNewTeamColors patch changes Armada's native
// TeamColor_Init table from the A1 names (white, red, blue, ...) to empty plus
// mpcolor01..16 and eight FO race names. It also redirects the initializer and
// all IA/minimap consumers from Armada's original array to FOTeamColor in
// FleetOpsHook.dll. A raw A1 teamcolor.odf therefore resolves normally but
// leaves every live player colour at the parser's black default. Chain the
// native initializer, then copy a child-over-parent palette whose A1 named
// entries have been translated to their original ordinal slots.
constexpr std::uintptr_t kTeamColorInitRva = 0x000954b0;
constexpr std::size_t kTeamColorInitHookLength = 5;
constexpr std::uint8_t kExpectedTeamColorInit[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uintptr_t kArmadaTeamColorPaletteRva = 0x00338a88;
// FleetOpsFunctionsHook.FOTeamColor at map symbol 0004:00030860. The .bss
// section begins at image RVA 0x00214000, producing the live RVA 0x00244860.
constexpr std::uintptr_t kFleetOpsTeamColorPaletteRva = 0x00244860;
constexpr std::size_t kNativePlayerColorFirstIndex = 1;
constexpr std::size_t kNativeTeamColorEntryCount = 25;
constexpr std::array<const char*, 5> kTeamColorRelativePaths{{
    "Addon\\teamcolor.odf",
    "odf\\system\\teamcolor.odf",
    "odf\\other\\teamcolor.odf",
    "odf\\teamcolor.odf",
    "teamcolor.odf",
}};

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

// Fleet Operations asks FileReader for A2's 40-byte serialized runtime-class
// name. The checked call bridge below reads A1's declared width and zero-pads
// the destination without changing native A2 streams.
constexpr std::uintptr_t kRtimeClassLoadReadNameCallRva = 0x0013c2c3;
constexpr std::uint8_t kExpectedRtimeClassLoadReadNameCall[] = {
    0xe8, 0x18, 0x14, 0xff, 0xff};
constexpr std::uintptr_t kFileInFixedCharsRva = 0x0012d6e0;
constexpr std::uint8_t kExpectedFileInFixedChars[] = {
    0x55, 0x8b, 0xec, 0x56, 0x8b, 0x75, 0x08};
constexpr std::size_t kSerializedRtimeClassNameSize =
    a1compat::kA2SerializedRtimeClassNameSize;
constexpr std::size_t kFileReaderInspectionSize = 0x60;

// Hook the game-object stage before A2's additional craft-class table. The
// bridge consumes the A1 object tail while native A2 streams stay unchanged.
constexpr std::uintptr_t kLoadGameObjectsLoadCallRva = 0x00202581;
constexpr std::uint8_t kExpectedLoadGameObjectsLoadCall[] = {
    0xe8, 0x8a, 0x01, 0xed, 0xff};
constexpr std::uintptr_t kGameObjectsLoadRva = 0x000d2710;
constexpr std::uint8_t kExpectedGameObjectsLoad[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0xd8, 0x00, 0x00, 0x00};

// LoadGame_MainLoad asks the AiMission subsystem to read a polymorphic mission
// object near the end of a BZN. Immediately before that call, A2 loads an
// additional craft-class table which does not exist in A1's load sequence.
// Let A2/FO maps retain that native loader, but skip it for positively
// identified A1 streams. Some A1 object tails still remain unread before this
// boundary, so the mission wrapper below also performs a guarded,
// forward-only resynchronization to the unique A1 AiMission marker.
constexpr std::uintptr_t kLoadGameA2CraftClassTableCallRva = 0x002025c0;
constexpr std::uint8_t kExpectedLoadGameA2CraftClassTableCall[] = {
    0xe8, 0xdb, 0x41, 0xe7, 0xff};
constexpr std::uintptr_t kA2CraftClassTableLoadRva = 0x000767a0;
constexpr std::uint8_t kExpectedA2CraftClassTableLoad[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68};
constexpr std::uintptr_t kLoadGameAiMissionLoadCallRva = 0x00202608;
constexpr std::uint8_t kExpectedLoadGameAiMissionLoadCall[] = {
    0xe8, 0xf3, 0xed, 0xdf, 0xff};
constexpr std::uintptr_t kAiMissionLoadRva = 0x00001400;
constexpr std::uint8_t kExpectedAiMissionLoad[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x0d, 0xdc, 0x47};
constexpr std::uintptr_t kAiMissionCurrentRva = 0x003347dc;

// KnownMaps::AddMapsWithMask asks MapDetailsFactory to inspect each BZN before
// displaying it in Instant Action. GameSetup independently loads the selected
// map before launch. A1's front matter predates MPDMinExtent, MPDSize, and A2's
// embedded start-location records, so both checked calls must pass through the
// compatibility bridge. A2 maps and native MapDetails loading remain untouched.
constexpr std::uintptr_t kMapDetailsFactoryLoadRva = 0x0014ba00;
constexpr std::uint8_t kExpectedMapDetailsFactoryLoad[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68};
constexpr std::uintptr_t kGameSetupLoadMapDetailsCallRva = 0x00147a59;
constexpr std::uint8_t kExpectedGameSetupLoadMapDetailsCall[] = {
    0xe8, 0xa2, 0x3f, 0x00, 0x00};
constexpr std::uintptr_t kKnownMapsLoadMapDetailsCallRva = 0x001b7da9;
constexpr std::uint8_t kExpectedKnownMapsLoadMapDetailsCall[] = {
    0xe8, 0x52, 0x3c, 0xf9, 0xff};
constexpr std::size_t kMapDetailsMinimumExtentOffset = 0x6c;
constexpr std::size_t kMapDetailsSizeOffset = 0x78;
constexpr std::size_t kMapDetailsStartLocationCountOffset = 0xc0;
constexpr std::size_t kMapDetailsStartLocationArrayOffset = 0xc4;
constexpr std::size_t kMapDetailsRequiredSize = 0xc8;
constexpr std::size_t kStartLocationDetailsSize = 0x60;
constexpr std::size_t kStartLocationPositionOffset = 0x00;
constexpr std::size_t kStartLocationTypeOffset = 0x0c;
// These are PE RVAs, not linker-map offsets. Segment 0003 begins at image
// RVA 0x002ec000 in the supported ArmadaL.exe.
constexpr std::uintptr_t kStartLocationTypeEmptyRva = 0x0036b760;
constexpr std::uintptr_t kStartLocationTypePlayerRva = 0x0036b770;
constexpr std::uintptr_t kGameSetupSlotCountRva = 0x002b6b0c;
// MSVCP60 basic_string<char>::operator=(basic_string const&) IAT slot.
constexpr std::uintptr_t kNativeStringAssignIatRva = 0x003b7c6c;
constexpr std::size_t kMaximumA1MdfFileSize = 16 * 1024;

// GameTypeToTheDeath::CheckAll decides whether each occupied start location
// still owns a viable non-planet object. A1 maps keep their start metadata in
// MDF rather than BZN records and can overwrite the relationships A2 derived
// from the IA slot teams. Restore A2's own setup rule at this boundary, then
// always chain through the original checked prologue.
constexpr std::uintptr_t kToTheDeathCheckAllRva = 0x0007dad0;
constexpr std::size_t kToTheDeathCheckAllHookLength = 6;
constexpr std::uint8_t kExpectedToTheDeathCheckAll[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c};
constexpr std::uintptr_t kTransportCurrentRva = 0x0036b8d4;
constexpr std::uintptr_t kTransportGetGameSetupRva = 0x00157940;
constexpr std::uintptr_t kGameSetupIsActiveSlotRva = 0x00146b90;
constexpr std::uintptr_t kGameSetupGetSideForSlotRva = 0x00146550;
constexpr std::uintptr_t kGameSetupGetAllianceTeamForSlotRva = 0x00146610;
constexpr std::uintptr_t kSideLookupRva = 0x00096340;
constexpr std::uintptr_t kSideSetRelationshipRva = 0x000971d0;

// Fleet Operations requests <race>_instant_action_build_list (and numbered
// variants) from AIP_Manager. Armada 1 races conventionally expose the same
// plan as <race>_build_list. If an A1 mod layer supplies only the legacy name,
// prefer it over the incompatible Data-layer Fleet Ops plan. Explicit modern
// files in the same or a higher-priority mod layer remain authoritative.
constexpr std::uintptr_t kAipManagerLookUpNewAipRva = 0x00025a50;
constexpr std::size_t kAipManagerLookUpNewAipHookLength = 5;
constexpr std::uint8_t kExpectedAipManagerLookUpNewAip[] = {
    0x55, 0x8b, 0xec, 0x51, 0x53};
constexpr char kInstantActionBuildListSuffix[] =
    "_instant_action_build_list";
constexpr char kLegacyBuildListSuffix[] = "_build_list";

// AIP::m_UpdateTechnologyLevel retries an unresolved Build_List_Element class,
// prints "AIP Error: Unit ... not found", and then dereferences the still-null
// pointer at RVA 0x000248a9. Legacy plans can legally name a unit absent from
// the active A2 tech tree. Skip only that unresolved element; every resolved
// entry retains the native update path.
constexpr std::uintptr_t kAipTechnologyUnitDerefRva = 0x000248a6;
constexpr std::size_t kAipTechnologyUnitDerefLength = 6;
constexpr std::uint8_t kExpectedAipTechnologyUnitDeref[] = {
    0x8b, 0x4d, 0xf8, 0x8b, 0x42, 0x6c};
constexpr std::uintptr_t kAipTechnologyUnitSkipRva = 0x00024903;

// Armada's live map bounds are min XYZ followed by max XYZ. The supported
// ArmadaL.exe uses these globals throughout spatial setup and camera/map
// calculations. A1 header parsing supplies the equivalent values before the
// first polymorphic object record is consumed.
constexpr std::uintptr_t kWorldSerializedMinimumExtentRva = 0x00339578;
constexpr std::uintptr_t kWorldSerializedMaximumExtentRva = 0x00339584;
constexpr std::uintptr_t kWorldMinimumExtentRva = 0x00339590;
constexpr std::uintptr_t kWorldMaximumExtentRva = 0x0033959c;
constexpr std::uintptr_t kWorldExtentRemapEnabledRva = 0x003395b4;

// DisplayInterface::PostLoadAll constructs a fresh interface sprite database,
// then reads gui_global.spr before any interface components are initialized.
// A child mod can replace that table by basename and accidentally hide the A2
// GUI records retained by the A1 compatibility parent. Replace only this
// checked CALL so a2_gui_global.spr is loaded into the same database first;
// the winning gui_global.spr is still read second and remains authoritative.
constexpr std::uintptr_t kDisplayInterfaceGuiSpriteReadTableCallRva =
    0x0011a776;
constexpr std::uint8_t kExpectedDisplayInterfaceGuiSpriteReadTableCall[] = {
    0xe8, 0x85, 0x5f, 0x12, 0x00};
constexpr std::uintptr_t kSt3dTextFileParserReadTableRva = 0x00240700;
constexpr std::uint8_t kExpectedSt3dTextFileParserReadTable[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uintptr_t kSt3dDatabaseFindRva = 0x00220750;
constexpr std::uint8_t kExpectedSt3dDatabaseFind[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr char kEssentialGuiSpriteTableName[] = "a2_gui_global.spr";
constexpr char kEssentialGuiSpriteSentinelName[] =
    "buttonBackgroundPanel.0";
// UI components resolve their configured default cursor through this shared
// helper after the world-sprite database exists. For an incompatible A1
// standard_cursor, return an already-loaded 32x32 cursor record instead of
// reparsing or mutating any sprite table.
constexpr std::uintptr_t kDefaultCursorLookupRva = 0x0011b3f0;
constexpr std::size_t kDefaultCursorLookupHookLength = 6;
constexpr std::uint8_t kExpectedDefaultCursorLookup[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0c};
constexpr std::uintptr_t kWorldSpriteOwnerPointerRva = 0x003ad508;
constexpr std::size_t kWorldSpriteDatabaseOffset = 0x44;
constexpr char kLegacyCursorFallbackSpriteName[] = "c_build";

// The tactical/world view does not use the GUI default-cursor resolver. Its
// Fleet Operations-owned initializer populates two static c_select cache
// slots, and Armada's cursor selector copies one of those pointers into the
// active visual cursor global. Repair those three pointers from an existing
// late A1 UI-render boundary rather than competing for either native entry.
constexpr std::array<std::uintptr_t, 2>
    kTacticalSelectCursorCacheSlotRvas{{0x0030fbe4, 0x0030fc0c}};
constexpr std::uintptr_t kActiveCursorSpritePointerRva = 0x003643d4;

// DisplayInterface::PostLoadAll publishes the newly constructed gameplay
// ParameterDB immediately before loading the selected Race's SPR table. Hook
// that stable publication instruction rather than the preceding constructor
// CALL, which Fleet Operations may already redirect before deferred modules
// load. Raw A1 CFGs contain the legacy SpeedRail and ControlPanel rectangles
// but no A2 screenWidth/screenHeight keys; give only that unmistakable layout
// its original 640x480 reference size before any component reads a rectangle.
constexpr std::uintptr_t kDisplayInterfaceGuiParameterDbPostConstructRva =
    0x0011a80f;
constexpr std::size_t kDisplayInterfaceGuiParameterDbPostConstructLength = 5;
constexpr std::uint8_t
    kExpectedDisplayInterfaceGuiParameterDbPostConstruct[] = {
        0xa3, 0x2c, 0x50, 0x76, 0x00};
constexpr std::uintptr_t kParameterDbGetRectangleRva = 0x001358f0;
constexpr std::uint8_t kExpectedParameterDbGetRectangle[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x04, 0x01, 0x00, 0x00};
constexpr std::uintptr_t kParameterDbGetVectorRva = 0x00135ba0;
constexpr std::uint8_t kExpectedParameterDbGetVector[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};
constexpr std::size_t kParameterDbScreenWidthOffset = 0x2c;
constexpr std::size_t kParameterDbScreenHeightOffset = 0x30;

// A2's Tooltip adds a framed, fixed-location verbose renderer whose layout and
// colour keys do not exist in A1 race CFGs. Fleet Operations owns the A2
// configuration entry, so restore the legacy presentation at the two stable
// render boundaries instead: apply A1's exact #808080/black colours before
// either path, and route only a missing-frame verbose tooltip through A2's
// native cursor-relative popup helper.
constexpr std::uintptr_t kTooltipRenderRva = 0x00105380;
constexpr std::size_t kTooltipRenderHookLength = 6;
constexpr std::uint8_t kExpectedTooltipRender[] = {
    0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x38};
constexpr std::uintptr_t kTooltipDrawPopupRva = 0x00105430;
constexpr std::uint8_t kExpectedTooltipDrawPopup[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uintptr_t kTooltipRenderVerboseRva = 0x00105720;
constexpr std::size_t kTooltipRenderVerboseHookLength = 6;
constexpr std::uint8_t kExpectedTooltipRenderVerbose[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x58};
constexpr std::size_t kTooltipVerboseTextOffset = 0x38;
constexpr std::size_t kTooltipBackgroundColourOffset = 0x40;
constexpr std::size_t kTooltipTextColourOffset = 0x4c;
constexpr std::uintptr_t kTooltipPopupFlags = 0x226;

// Fleet Operations' supported ArmadaL build exposes the common gameplay-UI
// rectangle loader at this RVA (the retail Armada II map uses a different
// address). Its hidden first argument is the returned RECT storage. A narrow
// alias layer lets A2's three-height ShipDisplay consume A1's one-panel keys.
constexpr std::uintptr_t kDisplayInterfaceLoadRectangleRva = 0x0011b430;
constexpr std::size_t kDisplayInterfaceLoadRectangleHookLength = 9;
constexpr std::uint8_t kExpectedDisplayInterfaceLoadRectangle[] = {
    0x55, 0x8b, 0xec, 0x8a, 0x0d, 0xb8, 0x4e, 0x76, 0x00};
constexpr LONG kMaximumLegacyShipDisplayAliasReports = 20;
constexpr LONG kMaximumLegacyOfficerRootRepairReports = 8;
constexpr std::uintptr_t kDisplayInterfaceDrawTextInRectangleRva =
    0x0011b160;
constexpr std::uint8_t kExpectedDisplayInterfaceDrawTextInRectangle[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};
constexpr std::uintptr_t kLocalizationLookupRva = 0x00081c90;
constexpr std::uintptr_t kLocalizationManagerPointerRva = 0x003379fc;

// ShipDisplay retains three local black-mask rectangles even when A1's one
// panel key is aliased during PostLoad. Fleet Operations can subsequently
// preserve independent A2 values in those fields. Restore all three from the
// active A1 infoBlackArea at the common render entry, before the native mode
// dispatcher selects low, middle, or tall presentation.
constexpr std::uintptr_t kShipDisplayRenderRva = 0x000f2bb0;
constexpr std::size_t kShipDisplayRenderHookLength = 8;
constexpr std::uint8_t kExpectedShipDisplayRender[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x53, 0x56};
constexpr std::uintptr_t kShipDisplayCursorOverRva = 0x000f2df0;
constexpr std::size_t kShipDisplayCursorOverHookLength = 5;
constexpr std::uint8_t kExpectedShipDisplayCursorOver[] = {
    0xa1, 0x18, 0x50, 0x76, 0x00};
constexpr std::uintptr_t kInterfaceCursorXRva = 0x00365018;
constexpr std::uintptr_t kInterfaceCursorYRva = 0x0036501c;
constexpr std::array<std::size_t, 3> kShipDisplayBlackRectangleOffsets{{
    0x188, 0x198, 0x1a8}};
constexpr std::uintptr_t kShipDisplayVtableRva = 0x002b4bcc;
constexpr std::size_t kShipDisplayRectangleOffset = 0x04;
constexpr std::size_t kShipDisplaySelectedObjectPointerOffset = 0x1e8;
constexpr std::size_t kShipDisplayLayoutIndexOffset = 0x1f4;
constexpr std::size_t kSelectedObjectCurrentCrewOffset = 0x1dc;
constexpr std::size_t kSelectedObjectMaximumCrewOffset = 0x1c4;
constexpr std::size_t kSelectedObjectMaximumSpecialEnergyOffset = 0x168;
constexpr std::size_t kSelectedObjectCurrentSpecialEnergyOffset = 0x16c;
constexpr std::size_t kObjectClassInitialCrewOffset = 0x084;
constexpr std::size_t kObjectClassOfficerCountOffset = 0x088;
// ShipDisplay owns separate ordinary and build/station children. Armada I
// supplies one identity/wireframe layout for both paths. DisplayComponent
// stores its parent at +0x04 and rectangle at +0x08; verify the native child
// type and parent identity before touching either path.
constexpr std::size_t kShipDisplaySingleWireframePointerOffset = 0x0ac;
constexpr std::size_t kShipDisplayBuildWireframePointerOffset = 0x114;
// The native RaceIcon at ShipDisplay+0x9c owns the race_icon_bar component and
// a separate +0x30 display rectangle for the smaller live insignia. A1's
// renderer draws the stored background sprite first, tinted with the selected
// object's team colour; A2's retained implementation no longer draws that
// first layer. Raw A1 CFGs use the same rectangle keys as A2, so restore both
// fields directly and reproduce the missing background pass.
constexpr std::size_t kShipDisplayRaceIconPointerOffset = 0x09c;
constexpr std::uintptr_t kRaceIconVtableRva = 0x002b4a6c;
constexpr std::size_t kRaceIconBackgroundSpritePointerOffset = 0x02c;
constexpr std::size_t kRaceIconDisplayRectangleOffset = 0x030;
constexpr std::uintptr_t kGameObjectTeamColourRva = 0x000d5040;
constexpr std::size_t kGameObjectTeamColourPointerOffset = 0x0ec;
constexpr std::uint8_t kExpectedGameObjectTeamColour[] = {
    0x8b, 0x81, 0xec, 0x00, 0x00, 0x00};
constexpr std::array<std::size_t, 2> kShipDisplayClassPointerOffsets{{
    0x090, 0x100}};
constexpr std::array<std::size_t, 2> kShipDisplayNamePointerOffsets{{
    0x094, 0x104}};
constexpr std::array<std::size_t, 2> kShipDisplayCrewPointerOffsets{{
    0x088, 0x10c}};
constexpr std::array<std::size_t, 2> kShipDisplayOfficerPointerOffsets{{
    0x098, 0x108}};
constexpr std::size_t kDisplayComponentParentPointerOffset = 0x04;
constexpr std::size_t kDisplayComponentRectangleOffset = 0x08;
constexpr std::uintptr_t kWireframeIconVtableRva = 0x002b4f18;
constexpr std::uintptr_t kShipClassTextVtableRva = 0x002b4e44;
constexpr std::uintptr_t kShipNameTextVtableRva = 0x002b4e74;
constexpr std::uintptr_t kShipCrewDisplayVtableRva = 0x002b4b18;
constexpr std::uintptr_t kShipOfficerDisplayVtableRva = 0x002b4c50;
constexpr std::size_t kShipTextRenderRectangleOffset = 0x104;
constexpr std::size_t kShipAmountSpritePointerOffset = 0x104;
constexpr std::size_t kShipAmountRenderRectangleOffset = 0x11c;
constexpr std::size_t kShipAmountIconRectangleOffset = 0x10c;
constexpr std::size_t kTextComponentDisplayOverrideSlotOffset = 0x028;
constexpr std::size_t kTextComponentFlagsOffset = 0x068;
constexpr std::size_t kTextComponentConstrainOffset = 0x06c;
constexpr std::size_t kTextComponentColourOffset = 0x070;
constexpr std::size_t kTextComponentFontStateOffset = 0x07c;
// ShipDisplay's PostLoad constructs the ordinary SystemValue children into
// +0x74..+0x84 and the build/station SystemValue children into +0xec..+0xfc.
// The adjacent +0xc4..+0xd4 and +0xd8..+0xe8 arrays are SystemIcon children;
// treating those icon arrays as values was both ineffective and prevented us
// from noticing that the station path needs its own infoBuildSystemIcon alias.
constexpr std::array<std::size_t, 2>
    kShipDisplaySystemValuePointerOffsets{{0x074, 0x0ec}};
constexpr std::size_t kShipDisplaySystemValueCount = 5;
constexpr std::uintptr_t kSystemValueVtableRva = 0x002b4c88;
// SystemValue does not use DisplayComponent's generic rectangle when the
// ShipDisplay invokes its specialised two-argument renderer. That renderer
// copies this embedded text rectangle to DisplayComponent's live draw area
// immediately before drawing the numeric value.
constexpr std::size_t kSystemValueRenderRectangleOffset = 0x11c;
// Armada II exposes ten construction-queue children at ShipDisplay+0x120.
// Armada I's SpeedRail has only five queue positions, so retain the native
// controls and construction behaviour while relocating its first five icons
// into the five leading slots and suppressing the five A2-only extras.
constexpr std::size_t kShipDisplayBuildQueuePointerOffset = 0x120;
constexpr std::size_t kShipDisplayBuildQueueCount = 10;
constexpr std::uintptr_t kBuildQueueIconVtableRva = 0x002b4994;
// Armada I owns eight MultiShipIcon controls at its ShipDisplay+0x70 and
// composes each tile from infoMultiShipIcon_N plus nested shield, energy,
// crew-dot, and wireframe areas. Armada II/Fleet Operations instead owns 16
// controls through a pointer-array field at +0x148, moves the shield into an
// A2-only absolute rectangle, and lets inherited A2 values win over same-named
// raw-A1 tile rectangles.
// Restore the first eight native controls from the active A1 CFG and park the
// eight A2-only controls off-screen. The retained A2 object still supplies
// selection, clicking, the live shield bar, and the live wireframe.
constexpr std::size_t kShipDisplayMultiShipPointerOffset = 0x148;
constexpr std::size_t kShipDisplayMultiShipCount = 16;
constexpr std::size_t kLegacyMultiShipCount = 8;
constexpr std::size_t kMultiShipShieldPointerOffset = 0x03c;
constexpr std::size_t kMultiShipWireframePointerOffset = 0x030;
// A2 removed A1's StandardBar energy child from both ShipDisplay modes. The
// retained shield bar is nevertheless a fully initialized StandardBar, so it
// can serve as a strictly temporary rendering scratch object after the native
// ShipDisplay pass. Restore every field immediately after each energy draw.
constexpr std::size_t kStandardBarObjectPointerOffset = 0x428;
constexpr std::size_t kStandardBarSpritePointerOffset = 0x42c;
constexpr std::size_t kStandardBarDisabledColourOffset = 0x430;
constexpr std::size_t kStandardBarRequiredSize = 0x43c;
constexpr std::size_t kStandardBarVirtualMethodCount = 10;
constexpr std::size_t kStandardBarActiveColourVtableIndex = 8;
constexpr std::size_t kStandardBarValuesVtableIndex = 9;
// MultiShipIcon and its nested WireframeIcon both request
// infoMultiShipIcon_N. This exact return address identifies the nested
// request so the rectangle loader can supply infoMultiWireframeIconArea
// instead of stretching the wireframe child across the whole tile.
constexpr std::uintptr_t kMultiShipWireframeRectangleLoadReturnRva =
    0x000f7127;
// ShipDisplay owns two adjacent but unrelated controls: EnergyText at +0x110
// and the actual ConstructionBar at +0xb0. Both occupy A1's shared lower bar
// rectangle, but only ConstructionBar follows an active producer/build job.
// Its native RTTI vtable is distinct from EnergyText's vtable.
constexpr std::size_t kShipDisplayEnergyPointerOffset = 0x110;
constexpr std::uintptr_t kEnergyTextVtableRva = 0x002b4adc;
constexpr std::size_t kShipDisplayConstructionBarPointerOffset = 0x0b0;
constexpr std::uintptr_t kConstructionBarVtableRva = 0x002b4d48;
// ConstructionBar normally receives the selected object at +0x428. A
// ConstructionRig instead owns the ID of the ConstructionObject it is
// building at +0x2b4, so bind the bar to that live object at its native render
// boundary. The native bar continues to calculate and draw all progress.
constexpr std::uintptr_t kConstructionBarRenderRva = 0x0010afd0;
constexpr std::size_t kConstructionBarRenderHookLength = 6;
constexpr std::uint8_t kExpectedConstructionBarRender[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x28};
constexpr std::size_t kConstructionBarObjectPointerOffset = 0x428;
constexpr std::uintptr_t kConstructionRigVtableRva = 0x002b22ec;
constexpr std::size_t kConstructionRigConstructionObjectIdOffset = 0x2b4;
constexpr std::uintptr_t kConstructionRigGetConstructionObjectRva = 0x000afe80;
constexpr std::uint8_t kExpectedConstructionRigGetConstructionObject[] = {
    0x8b, 0x89, 0xb4, 0x02, 0x00, 0x00};
constexpr std::uintptr_t kFindGameObjectByIdRva = 0x000cfff0;
constexpr std::uint8_t kExpectedFindGameObjectById[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x4d, 0x08, 0x8b, 0xc1};
constexpr std::size_t kMaximumLegacyShipDisplayBackgroundPieces = 16;
static_assert(
    kDisplayComponentRectangleOffset ==
        kDisplayComponentParentPointerOffset + sizeof(void*),
    "DisplayComponent rectangle must follow its 32-bit parent pointer");

// CinematicView keeps the frame, default-background, and live 3D viewport as
// separate rectangles. Fleet Operations can retain an A2-sized live viewport
// even after the raw A1 frame has loaded correctly, which lets the rendered
// object escape into the neighbouring ShipDisplay. Restore the parent and its
// two content rectangles immediately before the native render boundary.
constexpr std::uintptr_t kCinematicViewRenderRva = 0x000e5d30;
constexpr std::size_t kCinematicViewRenderHookLength = 6;
constexpr std::uint8_t kExpectedCinematicViewRender[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x24};
constexpr std::size_t kCinematicViewRectangleOffset = 0x04;
constexpr std::size_t kCinematicViewBackgroundRectangleOffset = 0x38;
constexpr std::size_t kCinematicViewDisplayRectangleOffset = 0x48;

// Armada II retained A1's COMM and MENU actions in ButtonPanel, but moved
// them to the resource strip at the top of the screen. Raw A1 race CFGs still
// describe the two controls as children of CinematicView. Relocate the live
// StandardButton instances (rather than drawing inert replacements) so their
// native callbacks, keyboard focus, hover state, and localized labels remain
// intact. The remaining A2-only ButtonPanel children are parked off-screen.
constexpr std::uintptr_t kButtonPanelFocusGameSimulateRva = 0x000fef80;
constexpr std::size_t kButtonPanelFocusGameSimulateHookLength = 6;
constexpr std::uint8_t kExpectedButtonPanelFocusGameSimulate[] = {
    0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x28};
constexpr std::uintptr_t kButtonPanelRenderRva = 0x000fefe0;
constexpr std::size_t kButtonPanelRenderHookLength = 6;
constexpr std::uint8_t kExpectedButtonPanelRender[] = {
    0xa1, 0xcc, 0x43, 0x76, 0x00, 0x56};
constexpr std::uintptr_t kButtonPanelCursorOverRva = 0x000ff050;
constexpr std::size_t kButtonPanelCursorOverHookLength = 5;
constexpr std::uint8_t kExpectedButtonPanelCursorOver[] = {
    0xa1, 0x18, 0x50, 0x76, 0x00};
constexpr std::uintptr_t kButtonPanelVtableRva = 0x002b50e4;
constexpr std::uintptr_t kStandardButtonVtableRva = 0x002b5684;
constexpr std::size_t kButtonPanelRectangleOffset = 0x04;
constexpr std::size_t kButtonPanelBackgroundPointerOffset = 0x28;
constexpr std::size_t kButtonPanelMenuButtonPointerOffset = 0x2c;
constexpr std::size_t kButtonPanelCommButtonPointerOffset = 0x30;
constexpr std::array<std::size_t, 6> kButtonPanelA2OnlyButtonOffsets{{
    0x34, 0x38, 0x3c, 0x40, 0x44, 0x48}};
constexpr std::size_t kButtonPanelExpandedFlagOffset = 0x4c;
constexpr std::uintptr_t kDisplayInterfaceLoadSpriteRva = 0x0011b3b0;
constexpr std::uint8_t kExpectedDisplayInterfaceLoadSprite[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0c, 0x8b, 0x4d, 0x08};
constexpr std::uintptr_t kStandardButtonSetSpritesRva = 0x0010ba80;
constexpr std::uint8_t kExpectedStandardButtonSetSprites[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x8b, 0x55, 0x0c};
constexpr a1compat::NativeUiRectangle kHiddenLegacyButtonPanelRectangle{
    -32768, -32768, -32767, -32767};

// Raw A1 stores controlPanelArea in screen coordinates and its twelve
// controlButton rectangles relative to that panel. Fleet Operations rebuilds
// PopupPalette from its A2 palette geometry, so restore the complete A1
// parent/child contract immediately before every native input and rendering
// boundary. PopupPalette itself has no A1 ControlPanel background; attach a
// native StandardBackground configured from controlBackgroundPanel as well.
constexpr std::uintptr_t kPopupPalettePostLoadRva = 0x000fa990;
constexpr std::size_t kPopupPalettePostLoadHookLength = 5;
constexpr std::uint8_t kExpectedPopupPalettePostLoad[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
// Fleet Operations replaces PopupPalette's virtual FocusGameSimulate entry
// with a Delphi implementation which walks the expanded 64-button array. The
// native Armada implementation remains in the image but is no longer the live
// input path. Hook the replacement itself so the A1 geometry is applied before
// StandardButton::Simulate performs its parent-local hit test.
constexpr std::uintptr_t kFoPopupPaletteFocusGameSimulateRva = 0x001e7970;
constexpr std::size_t kFoPopupPaletteFocusGameSimulateHookLength = 6;
constexpr std::uint8_t kExpectedFoPopupPaletteFocusGameSimulate[] = {
    0x55, 0x8b, 0xec, 0x53, 0x56, 0x57};
constexpr std::uintptr_t kPopupPaletteRenderRva = 0x000fbce0;
constexpr std::size_t kPopupPaletteRenderHookLength = 6;
constexpr std::uint8_t kExpectedPopupPaletteRender[] = {
    0x39, 0x0d, 0xcc, 0x43, 0x76, 0x00};
constexpr std::uintptr_t kPopupPaletteCursorOverRva = 0x000fbd30;
constexpr std::size_t kPopupPaletteCursorOverHookLength = 6;
constexpr std::uint8_t kExpectedPopupPaletteCursorOver[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x08};
constexpr std::uintptr_t kFoPopupButtonPointerArrayRva = 0x00247ef4;
constexpr std::uintptr_t kFoPopupPaletteSetCurrentMenuRva = 0x001e232c;
constexpr std::uint8_t kExpectedFoPopupPaletteSetCurrentMenu[] = {
    0x55, 0x8b, 0xec, 0x51, 0x89, 0x45, 0xfc, 0x8b, 0x45, 0xfc};
constexpr std::uintptr_t kFoControlButtonStateModeInfoRva = 0x001e23ec;
constexpr std::uint8_t kExpectedFoControlButtonStateModeInfo[] = {
    0x55, 0x8b, 0xec, 0x51, 0x53, 0x8b, 0xd9, 0x89, 0x45, 0xfc};
constexpr std::size_t kFoPopupButtonCapacity = 64;
constexpr std::size_t kPopupPaletteRectangleOffset = 0x04;
constexpr std::size_t kPopupPaletteNativeButtonPointerOffset = 0x28;
constexpr std::size_t kPopupPaletteNativeButtonCount = 21;
constexpr std::size_t kPopupPaletteCurrentMenuOffset = 0x124;
constexpr std::size_t kControlButtonRectangleOffset = 0x08;
constexpr std::size_t kControlButtonStateOffset = 0x34;
constexpr std::uintptr_t kControlButtonVtableRva = 0x002b484c;
constexpr std::size_t kControlButtonPressVtableOffset = 0x20;
constexpr std::uintptr_t kControlButtonPressRva = 0x000e69e0;
constexpr std::uint8_t kExpectedControlButtonPress[] = {
    0x8b, 0xc1, 0x56, 0x8b, 0x90, 0x88, 0x00, 0x00, 0x00};
constexpr std::uintptr_t kControlButtonClearRva = 0x000e6ad0;
constexpr std::uint8_t kExpectedControlButtonClear[] = {
    0x56, 0x8b, 0xf1, 0x8b, 0x8e, 0x84, 0x00, 0x00, 0x00};
// The Fleet Operations popup array is compacted dynamically. Identify the
// Transport action through either of the two native ControlButton
// bindings rather than an array index, then move that native button after the
// five queues and separator. Armada uses +0x88 for a direct Action and +0x84
// for ModeInfo; Fleet Operations uses both forms in the same live palette.
constexpr std::size_t kControlButtonModeInfoPointerOffset = 0x84;
constexpr std::size_t kControlButtonDirectActionPointerOffset = 0x88;
constexpr std::size_t kModeInfoTypeOffset = 0x04;
constexpr std::size_t kModeInfoActionPointerOffset = 0x10;
constexpr std::size_t kModeInfoTargetClassOffset = 0x0c;
constexpr std::size_t kModeInfoActionIndexOffset = 0x14;
constexpr std::size_t kActionAiCommandOffset = 0x1a0;
constexpr std::uint32_t kActionModeInfoType = 2;
// Armada's native command-name table is zero-based. A1 SpeedRail is a shared
// strip: special attacks occupy the first five cells, Transport sits
// after the separator, and producer selections reuse the first five cells for
// their queue. Relocate the already-bound native ControlButtons by command so
// their callbacks, tooltips, and enabled state remain engine-owned.
constexpr std::int32_t kSpecialAttackAiCommand = 6;
constexpr std::int32_t kTransportAiCommand = 12;
constexpr std::int32_t kTransportSpecialAiCommand = 13;
constexpr std::uint32_t kLegacyPopupRootMenu = 0;
constexpr std::uint32_t kLegacyPopupOrdersMenu =
    static_cast<std::uint32_t>(a1compat::kLegacyOrdersMenu);
constexpr std::uint32_t kLegacyPopupBuildMenu = 2;
constexpr std::size_t kLegacyOfficerRootControlSlot = 8;
constexpr std::size_t kModeInfoSize = 0x18;
// DisplayInterface's native helper turns a parent-local rectangle into the
// current viewport and draws it with the supplied RGB colour and opacity.
// Fleet Operations' ShipDisplay deliberately supplies 0.5 for its translucent
// A2 masks; Armada I's equivalent fill is opaque, so legacy panels use 1.0.
constexpr std::uintptr_t kDisplayInterfaceDrawRectangleRva = 0x0011b2c0;
constexpr std::uint8_t kExpectedDisplayInterfaceDrawRectangle[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x34};
constexpr std::uintptr_t kInterfaceBlackColourRva = 0x003a8f98;
constexpr std::uintptr_t kInterfaceRectangleOpaque = 0x3f800000;
constexpr std::size_t kStandardBackgroundSize = 0x34;
constexpr std::uintptr_t kStandardBackgroundConstructorRva = 0x0010a750;
constexpr std::uint8_t kExpectedStandardBackgroundConstructor[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uintptr_t kStandardBackgroundDestructorRva = 0x0010a7c0;
constexpr std::uint8_t kExpectedStandardBackgroundDestructor[] = {
    0x53, 0x56, 0x57, 0x8b, 0xf9};
constexpr std::uintptr_t kStandardBackgroundRenderRva = 0x0010a820;
constexpr std::size_t kStandardBackgroundRenderHookLength = 6;
constexpr std::uint8_t kExpectedStandardBackgroundRender[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x2c};
constexpr std::uintptr_t
    kStandardBackgroundInitializeConfigurationRva = 0x0010aaa0;
constexpr std::size_t
    kStandardBackgroundInitializeConfigurationHookLength = 5;
constexpr std::uint8_t
    kExpectedStandardBackgroundInitializeConfiguration[] = {
        0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uintptr_t kNativeStringTidyIatRva = 0x003b7dcc;
constexpr std::uintptr_t kNativeStringAssignCharsIatRva = 0x003b7dd8;
constexpr std::uintptr_t kNativeStringDestructorIatRva = 0x003b7dc0;
constexpr std::uintptr_t kNativeStringCStrIatRva = 0x003b7c64;
constexpr std::size_t kNativeStringStorageSize = 0x20;
constexpr char kLegacyControlBackgroundPrefix[] =
    "controlBackgroundPanel";
constexpr std::size_t kLegacyResourcePanelCount = 3;
constexpr std::size_t kNativeResourceDisplayCount = 6;
constexpr std::size_t kMaximumLegacyResourceBackgroundPieces = 16;
constexpr std::size_t kMaximumLegacySpeedBackgroundPieces = 16;
constexpr char kLegacySpeedPanelAreaKey[] = "speedPanelArea";
constexpr char kLegacySpeedBackgroundPrefix[] = "speedBackgroundPanel";
constexpr std::size_t kResourcePanelRectangleOffset = 0x04;
constexpr std::uintptr_t kResourcePanelRenderRva = 0x000ffa40;
constexpr std::uint8_t kExpectedResourcePanelRender[] = {
    0xa1, 0xcc, 0x43, 0x76, 0x00, 0x56, 0x8b, 0xf1, 0x57};
constexpr std::uintptr_t kResourcePanelVtableRva = 0x002b5174;
constexpr std::size_t kResourcePanelRenderVtableOffset = 0x58;
constexpr std::size_t kResourcePanelFirstDisplayPointerOffset = 0x2c;
constexpr std::size_t kResourceDisplayTextPointerOffset = 0x2c;
constexpr std::size_t kSpriteFrameListOffset = 0x1c;
constexpr std::size_t kSpriteColourOffset = 0x24;
constexpr std::uintptr_t kInterfaceSpriteDatabasePointerRva = 0x00365030;
constexpr std::uintptr_t kInterfaceSpriteDatabaseGetRva = 0x00220750;
constexpr std::uintptr_t kFoSpriteSetColourRva = 0x001e34b4;
constexpr std::uintptr_t kFoSpriteDrawScaled2DRva = 0x001e3498;
constexpr std::uint8_t kExpectedInterfaceSpriteDatabaseGet[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x53, 0x56, 0x57};
constexpr std::uint8_t kExpectedFoSpriteSetColour[] = {
    0x55, 0x8b, 0xec, 0x51, 0x89, 0x45, 0xfc};
constexpr std::uint8_t kExpectedFoSpriteDrawScaled2D[] = {
    0x55, 0x8b, 0xec, 0x51, 0x89, 0x45, 0xfc};
// Native ResourcePanel indices are Crew, Officers, Dilithium. Keep this
// ordering so the original A1 displays can use native values and formatting.
constexpr std::array<const char*, kLegacyResourcePanelCount>
    kLegacyResourcePanelAreaKeys{{
        "crewPanelArea", "officerPanelArea", "dilithiumPanelArea"}};
constexpr std::array<const char*, kLegacyResourcePanelCount>
    kLegacyResourceBackgroundPrefixes{{
        "crewBackgroundPanel", "officerBackgroundPanel",
        "dilithiumBackgroundPanel"}};
constexpr std::array<const char*, kLegacyResourcePanelCount>
    kLegacyResourceTextAreaKeys{{
        "crewTextArea", "officerTextArea", "dilithiumTextArea"}};
constexpr std::array<const char*, kLegacyResourcePanelCount>
    kLegacyResourceIconAreaKeys{{
        "crewIconArea", "officerIconArea", "dilithiumIconArea"}};
constexpr std::array<const char*, kLegacyResourcePanelCount>
    kLegacyResourceIconKeys{{
        "crewIcon", "officerIcon", "dilithiumIcon"}};
constexpr a1compat::NativeUiRectangle kHiddenLegacyResourceRectangle{
    -32768, -32768, -32767, -32767};

// CraftEnhancement.Craft_mLevelUp obtains the craft's Side at +0xf0, then
// that Side's Race at +0x244, before consulting RaceEnhancement's canGainXP
// flag at Race+0x634. A1 races.odf files predate A2's required neutral
// `norace` record, so neutral scenery can legitimately reach this read with a
// null Race. Preserve the native read for every valid Race, but supply A2
// Classic's missing-only `canGainXP = false` result for the null case.
constexpr std::uintptr_t kCraftLevelUpRaceRva = 0x001dbdcb;
constexpr std::size_t kCraftLevelUpRaceHookLength = 7;
constexpr std::uint8_t kExpectedCraftLevelUpRace[] = {
    0x0f, 0xb6, 0x80, 0x34, 0x06, 0x00, 0x00};

constexpr std::size_t kGameObjectClassOffset = 0x40;
constexpr std::size_t kGameObjectHandleOffset = 0x28;
constexpr std::size_t kGameObjectTeamOffset = 0xec;
constexpr std::size_t kCraftSideOffset = 0xf0;
constexpr std::size_t kCraftEnhancementOffset = 0x1a4;
constexpr std::size_t kCraftPhysicsControlOffset = 0x1b0;
constexpr std::size_t kCraftClassPhysicsModelOffset = 0x1e8;
constexpr std::size_t kSideRaceOffset = 0x244;
constexpr std::uintptr_t kGameObjectClassGetOdfNameRva = 0x000ce370;
constexpr std::uint8_t kExpectedGameObjectClassGetOdfName[] = {
    0x8b, 0x89, 0xcc, 0x01, 0x00, 0x00,
    0xe9, 0x25, 0xb0, 0x18, 0x00};

const A2FO_ModuleApi* g_api = nullptr;
void* g_armada = nullptr;
void* g_fleet_ops = nullptr;
A2FO_InlineHook g_nebula_set_textures_recursive_hook{};
A2FO_InlineHook g_aip_lookup_hook{};
A2FO_InlineHook g_craft_level_up_race_hook{};
A2FO_InlineHook g_starbase_initialize_geometry_hook{};
A2FO_InlineHook g_to_the_death_check_hook{};
A2FO_InlineHook g_starbase_class_build_class_hook{};
A2FO_InlineHook g_starbase_clear_team_hook{};
A2FO_InlineHook g_starbase_set_team_hook{};
A2FO_InlineHook g_starbase_load_hook{};
A2FO_InlineHook g_starbase_save_hook{};
A2FO_InlineHook g_officer_upgrade_class_build_class_hook{};
A2FO_InlineHook g_producer_build_button_visible_hook{};
A2FO_InlineHook g_fofs_get_virtual_directory_hook{};
A2FO_InlineHook g_team_color_init_hook{};
A2FO_InlineHook g_command_info_build_class_hook{};
A2FO_InlineHook g_gui_parameter_db_post_construct_hook{};
A2FO_InlineHook g_tooltip_render_hook{};
A2FO_InlineHook g_tooltip_render_verbose_hook{};
A2FO_InlineHook g_popup_palette_post_load_hook{};
A2FO_InlineHook g_fo_popup_palette_focus_game_simulate_hook{};
A2FO_InlineHook g_popup_palette_render_hook{};
A2FO_InlineHook g_popup_palette_cursor_over_hook{};
A2FO_InlineHook g_cinematic_view_render_hook{};
A2FO_InlineHook g_button_panel_focus_game_simulate_hook{};
A2FO_InlineHook g_button_panel_render_hook{};
A2FO_InlineHook g_button_panel_cursor_over_hook{};
A2FO_InlineHook g_ship_display_render_hook{};
A2FO_InlineHook g_ship_display_cursor_over_hook{};
A2FO_InlineHook g_construction_bar_render_hook{};
A2FO_InlineHook g_display_interface_load_rectangle_hook{};
A2FO_InlineHook g_standard_background_render_hook{};
A2FO_InlineHook g_standard_background_initialize_hook{};
void** g_resource_panel_render_vtable_slot = nullptr;
void* g_resource_panel_render_original = nullptr;
bool g_resource_panel_render_vtable_hook_installed = false;
void** g_control_button_press_vtable_slot = nullptr;
void* g_control_button_press_original = nullptr;
bool g_control_button_press_vtable_hook_installed = false;
void** g_starbase_finish_build_vtable_slot = nullptr;
void* g_starbase_finish_build_original = nullptr;
bool g_starbase_finish_build_vtable_hook_installed = false;
void** g_starbase_start_effect_vtable_slot = nullptr;
void* g_starbase_start_effect_original = nullptr;
bool g_starbase_start_effect_vtable_hook_installed = false;
void** g_producer_push_target_slot = nullptr;
void* g_producer_push_target_original = nullptr;
bool g_producer_push_target_hook_installed = false;
volatile LONG g_invalid_nebula_node_count = 0;
volatile LONG g_a1_bzn_runtime_class_read_count = 0;
volatile LONG g_a1_bzn_runtime_class_read_failure_count = 0;
volatile LONG g_a1_bzn_object_tail_load_count = 0;
volatile LONG g_a1_bzn_a2_craft_class_table_skip_count = 0;
volatile LONG g_a1_bzn_ai_mission_load_count = 0;
volatile LONG g_a1_bzn_map_details_count = 0;
volatile LONG g_a1_mdf_start_location_count = 0;
volatile LONG g_a1_bzn_world_bounds_count = 0;
volatile LONG g_a1_selected_map_active = 0;
volatile LONG g_a1_relationship_restore_count = 0;
volatile LONG g_legacy_aip_name_fallback_count = 0;
volatile LONG g_missing_aip_technology_unit_count = 0;
volatile LONG g_gui_sprite_table_load_count = 0;
volatile LONG g_legacy_cursor_fallback_count = 0;
volatile LONG g_legacy_tactical_cursor_fallback_count = 0;
volatile LONG g_legacy_cursor_fallback_failure_reported = 0;
bool g_legacy_cursor_override_required = false;
bool g_legacy_tactical_cursor_override_required = false;
std::string g_legacy_cursor_override_source;
std::array<void*, 2> g_legacy_tactical_native_overview_cursors{};
A2FO_InlineHook g_legacy_default_cursor_lookup_hook{};
volatile LONG g_legacy_gameplay_ui_active = 0;
volatile LONG g_legacy_tooltip_background_fallback = 0;
volatile LONG g_legacy_tooltip_text_fallback = 0;
volatile LONG g_legacy_tooltip_frame_fallback = 0;
volatile LONG g_legacy_tooltip_colour_reported = 0;
volatile LONG g_legacy_control_button_rect_count = 0;
volatile LONG g_legacy_control_button_adapter_reported = 0;
volatile LONG g_legacy_control_background_available = 0;
volatile LONG g_legacy_control_black_mask_available = 0;
volatile LONG g_legacy_control_black_mask_reported = 0;
volatile LONG g_legacy_ship_display_black_mask_available = 0;
volatile LONG g_legacy_ship_display_black_mask_reported = 0;
volatile LONG g_legacy_ship_display_wireframe_available = 0;
volatile LONG g_legacy_ship_display_wireframe_reported = 0;
volatile LONG g_legacy_ship_display_race_icon_available = 0;
volatile LONG g_legacy_ship_display_race_icon_reported = 0;
volatile LONG g_legacy_ship_display_race_icon_background_reported = 0;
volatile LONG g_legacy_ship_display_identity_available = 0;
volatile LONG g_legacy_ship_display_identity_reported = 0;
volatile LONG g_legacy_ship_display_identity_text_available = 0;
volatile LONG g_legacy_ship_display_identity_text_reported = 0;
volatile LONG g_legacy_ship_display_identity_background_available = 0;
volatile LONG g_legacy_ship_display_identity_background_reported = 0;
volatile LONG g_legacy_ship_display_system_strip_reported = 0;
volatile LONG g_legacy_ship_display_progress_available = 0;
volatile LONG g_legacy_ship_display_progress_reported = 0;
volatile LONG g_legacy_ship_display_multi_layout_available = 0;
volatile LONG g_legacy_ship_display_multi_layout_reported = 0;
volatile LONG g_legacy_ship_display_multi_constructor_reported = 0;
volatile LONG g_legacy_ship_display_multi_crew_reported = 0;
volatile LONG g_legacy_ship_display_energy_available = 0;
volatile LONG g_legacy_ship_display_energy_reported = 0;
volatile LONG g_legacy_speed_control_layout_available = 0;
volatile LONG g_legacy_speed_queue_layout_reported = 0;
volatile LONG g_legacy_speed_special_layout_reported = 0;
volatile LONG g_legacy_speed_special_layout_count = 0;
volatile LONG g_legacy_speed_transport_layout_reported = 0;
volatile LONG g_legacy_popup_control_diagnostic_reported = 0;
volatile LONG g_legacy_officer_root_control_reported = 0;
volatile LONG g_legacy_officer_root_repair_report_count = 0;
volatile LONG g_legacy_officer_root_order_latched_reported = 0;
volatile LONG g_legacy_officer_root_return_reported = 0;
volatile LONG g_legacy_officer_root_direct_dispatch_reported = 0;
volatile LONG g_legacy_officer_build_control_hidden_reported = 0;
volatile LONG g_legacy_speed_separator_available = 0;
volatile LONG g_legacy_speed_cursor_reported = 0;
volatile LONG g_legacy_construction_bar_target_reported = 0;
volatile LONG g_legacy_construction_rig_progress_diagnostic_reported = 0;
volatile LONG g_legacy_ship_display_alias_report_count = 0;
volatile LONG g_missing_craft_race_count = 0;
volatile LONG g_officer_quarter_prepare_count = 0;
volatile LONG g_officer_upgrade_completion_count = 0;
volatile LONG g_officer_upgrade_rejection_count = 0;
volatile LONG g_officer_upgrade_class_count = 0;
volatile LONG g_officer_upgrade_consumed_count = 0;
volatile LONG g_officer_upgrade_effect_suppression_count = 0;
volatile LONG g_constructor_menu_capability_count = 0;
volatile LONG g_legacy_scout_menu_capability_count = 0;
volatile LONG g_legacy_station_menu_capability_count = 0;
volatile LONG g_legacy_moon_menu_capability_count = 0;
volatile LONG g_legacy_explore_command_collision_count = 0;
volatile LONG g_virtual_directory_normalization_count = 0;
volatile LONG g_legacy_moon_resource_default_count = 0;
volatile LONG g_legacy_physics_combat_speed_default_count = 0;
volatile LONG g_legacy_physics_impulse_speed_translation_count = 0;
volatile LONG g_legacy_physics_warp_speed_translation_count = 0;
volatile LONG g_legacy_physics_model_default_count = 0;
volatile LONG g_legacy_smooth_profile_translation_count = 0;
volatile LONG g_synthetic_neutral_race_index = -1;
volatile LONG g_neutral_race_registry_default_count = 0;
volatile LONG g_legacy_team_color_apply_count = 0;
volatile LONG g_legacy_resource_maximum_post_config_pending = 0;

CRITICAL_SECTION g_officer_state_lock;
bool g_officer_state_lock_ready = false;
bool g_officer_upgrade_system_ready = false;
bool g_officer_upgrade_identity_ready = false;
bool g_officer_upgrade_admission_ready = false;
bool g_officer_upgrade_completion_ready = false;
bool g_producer_events_ready = false;
bool g_race_menu_callback_ready = false;
bool g_legacy_race_fallback_logged = false;
void* g_project_id_get_odf_name = nullptr;

bool apply_legacy_native_resource_limits();

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

// Field order matches kNormalStartingResourceCommands and
// kLotsStartingResourceCommands: Crew, Dilithium, Metal, Tritanium, Supply.
struct LegacyStartingResourceDefaults {
    std::array<std::int32_t, 5> normal{{
        kDefaultStartingResourceAmount, kDefaultStartingResourceAmount,
        kDefaultStartingResourceAmount, kDefaultStartingResourceAmount,
        kDefaultStartingResourceAmount}};
    std::array<std::int32_t, 5> lots{{
        kDefaultStartingResourceAmount * 3 / 2,
        kDefaultStartingResourceAmount * 3 / 2,
        kDefaultStartingResourceAmount * 3 / 2,
        kDefaultStartingResourceAmount * 3 / 2,
        kDefaultStartingResourceAmount * 3 / 2}};
};

struct LegacyRaceRuntimeRecord {
    void* race = nullptr;
    a1compat::LegacyRaceMenuInput input;
};

std::unordered_map<void*, StarbaseClassPolicy> g_starbase_class_policies;
std::unordered_map<void*, StarbaseOfficerState> g_starbase_officer_states;
std::unordered_map<void*, std::array<char, 64>> g_officer_upgrade_races;
std::vector<LegacyRaceRuntimeRecord> g_legacy_race_records;
LegacyStartingResourceDefaults g_legacy_starting_resource_defaults;
a1compat::TeamColorPalettePolicy g_legacy_team_color_palette;
bool g_legacy_team_color_palette_active = false;
std::array<a1compat::NativeUiRectangle,
           a1compat::kLegacyControlButtonCount>
    g_legacy_control_button_rects{};
a1compat::NativeUiRectangle g_legacy_control_panel_rect{};
a1compat::NativeUiRectangle g_legacy_control_background_rect{};
a1compat::NativeUiRectangle g_legacy_control_black_rect{};
alignas(void*) std::array<std::uint8_t, kStandardBackgroundSize>
    g_legacy_control_background_storage{};
void* g_legacy_control_background_parent = nullptr;
bool g_legacy_control_background_constructed = false;
struct LegacyBackgroundPiece {
    a1compat::NativeUiRectangle rectangle{};
    std::array<char, 96> sprite_name{};
};
a1compat::NativeUiRectangle g_legacy_speed_panel_rect{};
std::array<a1compat::NativeUiRectangle,
           a1compat::kLegacySpeedQueueSlotCount>
    g_legacy_speed_queue_screen_rects{};
a1compat::NativeUiRectangle g_legacy_speed_transport_screen_rect{};
LegacyBackgroundPiece g_legacy_speed_separator_piece{};
std::array<LegacyBackgroundPiece, kMaximumLegacySpeedBackgroundPieces>
    g_legacy_speed_background_pieces{};
std::size_t g_legacy_speed_background_piece_count = 0;
volatile LONG g_legacy_speed_panel_available = 0;
volatile LONG g_legacy_speed_panel_reported = 0;
bool g_legacy_speed_panel_runtime_ready = false;
a1compat::NativeUiRectangle g_legacy_cinematic_panel_rect{};
a1compat::NativeUiRectangle g_legacy_cinematic_background_panel_rect{};
a1compat::NativeUiRectangle g_legacy_cinematic_background_rect{};
a1compat::NativeUiRectangle g_legacy_cinematic_display_rect{};
a1compat::NativeUiRectangle g_legacy_cinematic_menu_button_rect{};
a1compat::NativeUiRectangle g_legacy_cinematic_comm_button_rect{};
std::array<char, 96> g_legacy_cinematic_menu_button_sprite_name{};
std::array<char, 96> g_legacy_cinematic_menu_border_sprite_name{};
std::array<char, 96> g_legacy_cinematic_comm_button_sprite_name{};
std::array<char, 96> g_legacy_cinematic_comm_border_sprite_name{};
volatile LONG g_legacy_cinematic_layout_available = 0;
volatile LONG g_legacy_cinematic_layout_reported = 0;
volatile LONG g_legacy_cinematic_buttons_available = 0;
volatile LONG g_legacy_cinematic_buttons_reported = 0;
void* g_legacy_cinematic_button_art_owner = nullptr;
bool g_legacy_cinematic_button_runtime_ready = false;
a1compat::NativeUiRectangle g_legacy_ship_display_black_rect{};
a1compat::NativeUiRectangle g_legacy_ship_display_wireframe_rect{};
a1compat::NativeUiRectangle g_legacy_ship_display_race_icon_rect{};
a1compat::NativeUiRectangle g_legacy_ship_display_race_icon_display_rect{};
a1compat::NativeUiRectangle g_legacy_ship_display_class_rect{};
a1compat::NativeUiRectangle g_legacy_ship_display_name_rect{};
a1compat::NativeUiRectangle g_legacy_ship_display_crew_rect{};
a1compat::NativeUiRectangle g_legacy_ship_display_crew_dot_rect{};
a1compat::NativeUiRectangle g_legacy_ship_display_officer_rect{};
a1compat::NativeUiRectangle g_legacy_ship_display_crew_label_rect{};
a1compat::NativeUiRectangle g_legacy_ship_display_officer_label_rect{};
a1compat::NativeUiRectangle g_legacy_ship_display_progress_rect{};
a1compat::NativeUiRectangle g_legacy_ship_display_single_energy_rect{};
std::array<a1compat::NativeUiRectangle, kLegacyMultiShipCount>
    g_legacy_ship_display_multi_tile_rects{};
std::array<a1compat::NativeUiRectangle, kLegacyMultiShipCount>
    g_legacy_ship_display_multi_shield_rects{};
std::array<a1compat::NativeUiRectangle, kLegacyMultiShipCount>
    g_legacy_ship_display_multi_wireframe_rects{};
std::array<a1compat::NativeUiRectangle, kLegacyMultiShipCount>
    g_legacy_ship_display_multi_crew_rects{};
std::array<a1compat::NativeUiRectangle, kLegacyMultiShipCount>
    g_legacy_ship_display_multi_energy_rects{};
std::array<char, 96> g_legacy_ship_display_single_energy_sprite_name{};
std::array<char, 96> g_legacy_ship_display_multi_energy_sprite_name{};
std::array<char, 96> g_legacy_ship_display_multi_crew_sprite_name{};
std::array<char, 96> g_legacy_ship_display_crew_label_key{};
std::array<char, 96> g_legacy_ship_display_officer_label_key{};
std::array<char, 96> g_legacy_ship_display_crew_dot_sprite_name{};
std::array<LegacyBackgroundPiece,
           kMaximumLegacyShipDisplayBackgroundPieces>
    g_legacy_ship_display_identity_background_pieces{};
std::size_t g_legacy_ship_display_identity_background_piece_count = 0;
a1compat::NativeUiRectangle g_legacy_resource_panel_rect{};
std::array<a1compat::NativeUiRectangle, kNativeResourceDisplayCount>
    g_legacy_resource_text_rects{};
std::array<a1compat::NativeUiRectangle, kLegacyResourcePanelCount>
    g_legacy_resource_icon_rects{};
std::array<std::array<char, 64>, kLegacyResourcePanelCount>
    g_legacy_resource_icon_names{};
std::array<std::array<
    LegacyBackgroundPiece,
    kMaximumLegacyResourceBackgroundPieces>,
    kLegacyResourcePanelCount> g_legacy_resource_background_pieces{};
std::array<std::size_t, kLegacyResourcePanelCount>
    g_legacy_resource_background_piece_counts{};
volatile LONG g_legacy_resource_panel_available = 0;
volatile LONG g_legacy_resource_panel_reported = 0;
volatile LONG g_legacy_resource_background_suppression_reported = 0;
volatile LONG g_legacy_resource_render_bridge_active = 0;
volatile LONG g_legacy_cinematic_caption_reported = 0;
void* g_legacy_caption_text_component = nullptr;
bool g_legacy_resource_panel_runtime_ready = false;
bool g_legacy_control_black_mask_runtime_ready = false;
bool g_legacy_ship_display_render_runtime_ready = false;
bool g_legacy_ship_display_background_runtime_ready = false;
bool g_legacy_ship_display_opaque_black_runtime_ready = false;
bool g_legacy_ship_display_race_icon_background_runtime_ready = false;
bool g_legacy_ship_display_text_runtime_ready = false;
bool g_legacy_construction_bar_target_runtime_ready = false;
bool g_legacy_officer_root_control_runtime_ready = false;
void* g_legacy_selected_object = nullptr;
void* g_legacy_officer_root_button = nullptr;
void* g_legacy_officer_root_target = nullptr;
bool g_legacy_officer_root_enabled = false;
void* g_legacy_officer_root_dispatch_producer = nullptr;
void* g_legacy_officer_root_dispatch_target = nullptr;
void* volatile g_legacy_officer_root_pending_producer = nullptr;
bool g_legacy_officer_root_direct_dispatch_active = false;
alignas(void*) std::array<std::uint8_t, kModeInfoSize>
    g_legacy_officer_root_mode_info{};

struct SpriteVector {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

SpriteVector g_legacy_tooltip_background_colour{};
SpriteVector g_legacy_ship_display_single_energy_colour{1.0f, 1.0f, 0.0f};
SpriteVector g_legacy_ship_display_single_energy_disabled_colour{
    0.3f, 0.3f, 0.3f};
SpriteVector g_legacy_ship_display_multi_energy_colour{1.0f, 1.0f, 0.0f};
SpriteVector g_legacy_ship_display_multi_energy_disabled_colour{
    0.3f, 0.3f, 0.3f};
SpriteVector g_legacy_energy_bar_active_colour{};
float g_legacy_energy_bar_current = 0.0f;
float g_legacy_energy_bar_maximum = 0.0f;
std::array<void*, kStandardBarVirtualMethodCount>
    g_legacy_energy_bar_vtable{};

bool draw_legacy_ship_text(
    const char* value, const a1compat::NativeUiRectangle& rectangle,
    const SpriteVector& colour, void* text_component,
    void* display_interface_override = nullptr,
    std::int32_t flags_override =
        std::numeric_limits<std::int32_t>::min()) noexcept;

using NebulaSetTexturesRecursiveFn = void (__cdecl*)(void* node);
using FileInFixedCharsFn = bool (__cdecl*)(
    void* file_reader, void* output, std::uint32_t size);
using FileReaderLoadFn = bool (__cdecl*)(void* file_reader);
using AiMissionLoadFn = FileReaderLoadFn;
using MapDetailsFactoryLoadFn = void* (__cdecl*)(const char* filename);
using TeamColorInitFn = void (__cdecl*)();
using ControlButtonStateModeInfoFn =
    std::uintptr_t (__attribute__((regparm(3))) *)(
        void* button, void* mode_info, std::uintptr_t enabled);
using PopupPaletteSetCurrentMenuFn =
    void (__attribute__((regparm(2))) *)(
        void* popup, std::uintptr_t menu);
bool read_parameter_classlabel(
    void* parameter_db, std::array<char, 64>* output) noexcept;
bool read_parameter_race(
    void* parameter_db, std::array<char, 64>* output) noexcept;
bool contains_ci_substring(const char* source, const char* token) noexcept;
void* read_pointer_at(const void* object, std::size_t offset) noexcept;
bool is_valid_parameter_db(void* parameter_db) noexcept;
std::string join_path(const char* root, const char* name);
bool matches_numbered_odf_family(
    const char* odf_name, const char* family) noexcept;
void ensure_starbase_build_menu_capability(
    void* object_class, const char* known_odf_name = nullptr) noexcept;
void register_completed_starbase_class_policy(
    const A2FO_GameObjectClassLoadedEvent* event,
    const char* odf_name) noexcept;

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
extern "C" std::uintptr_t a2fo_a1_call_thiscall_6(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4, std::uintptr_t argument5,
    std::uintptr_t argument6);
extern "C" std::uintptr_t a2fo_a1_call_thiscall_7(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4, std::uintptr_t argument5,
    std::uintptr_t argument6, std::uintptr_t argument7);
extern "C" void a2fo_a1_fo_sprite_set_colour(
    void* function, void* sprite, const void* colour);
extern "C" void a2fo_a1_fo_sprite_draw_scaled_2d(
    void* function, void* sprite, const void* position,
    float display_width, float display_height);

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
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    if (begin < kMinimumSafeAddress ||
        size > std::numeric_limits<std::size_t>::max() - begin) {
        return false;
    }
    const std::uintptr_t end = begin + size;
    std::uintptr_t cursor = begin;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION information{};
        if (VirtualQuery(
                reinterpret_cast<const void*>(cursor), &information,
                sizeof(information)) != sizeof(information) ||
            information.State != MEM_COMMIT ||
            (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
            return false;
        }
        const auto region_begin = reinterpret_cast<std::uintptr_t>(
            information.BaseAddress);
        if (information.RegionSize >
            std::numeric_limits<std::uintptr_t>::max() - region_begin) {
            return false;
        }
        const std::uintptr_t region_end =
            region_begin + information.RegionSize;
        if (cursor < region_begin || region_end <= cursor) return false;
        cursor = std::min(region_end, end);
    }
    return true;
}

bool inspect_a1_bzn_reader(
    const void* file_reader, a1compat::A1BznHeader* header = nullptr) noexcept {
    if (!readable_range(file_reader, kFileReaderInspectionSize)) return false;

    const auto* reader = static_cast<const std::uint8_t*>(file_reader);
    if (reader[6] == 0 || reader[7] == 0) return false;

    std::uint32_t reader_version = 0;
    std::uintptr_t buffer_address = 0;
    std::uint32_t buffer_size = 0;
    std::memcpy(&reader_version, reader + 0x08, sizeof(reader_version));
    std::memcpy(&buffer_address, reader + 0x0c, sizeof(buffer_address));
    std::memcpy(&buffer_size, reader + 0x10, sizeof(buffer_size));
    if (reader_version < a1compat::kMinimumSupportedA1BznVersion ||
        reader_version > a1compat::kMaximumSupportedA1BznVersion ||
        !buffer_address || !buffer_size) {
        return false;
    }

    // The complete map can be many megabytes. The original 512-byte prefix is
    // sufficient to identify the validated A1 front matter and mission name,
    // but the final extent fields can cross that boundary. FileReader does not
    // guarantee that an arbitrary 4 KiB span from its current backing window
    // is readable, so never make extended extent inspection a prerequisite for
    // recognizing the stream. This preserves A1 object/AiMission loading even
    // when only the compact prefix is currently exposed.
    constexpr std::size_t kMinimumHeaderInspectionSize = 512;
    const std::size_t recognition_size = std::min<std::size_t>(
        buffer_size, kMinimumHeaderInspectionSize);
    const auto* buffer = reinterpret_cast<const std::uint8_t*>(buffer_address);
    if (!readable_range(buffer, recognition_size)) return false;

    a1compat::A1BznHeader parsed;
    if (!a1compat::parse_a1_bzn_header(
            buffer, recognition_size, &parsed) ||
        parsed.version != reader_version) {
        return false;
    }

    const std::size_t extended_size = std::min<std::size_t>(
        buffer_size, a1compat::kMaximumA1BznHeaderInspectionSize);
    if (extended_size > recognition_size &&
        readable_range(buffer, extended_size)) {
        a1compat::A1BznHeader extended;
        if (a1compat::parse_a1_bzn_header(
                buffer, extended_size, &extended) &&
            extended.version == reader_version) {
            parsed = extended;
        }
    }
    if (header) *header = parsed;
    return true;
}

struct FileReaderCursorState {
    std::uintptr_t base = 0;
    std::uintptr_t cursor = 0;
    std::uintptr_t end = 0;
    std::uint32_t buffer_size = 0;
    std::uintptr_t offset = 0;
    std::uintptr_t remaining = 0;
};

bool inspect_file_reader_cursor(
    const void* file_reader, FileReaderCursorState* state) noexcept {
    if (!state ||
        !readable_range(file_reader, kFileReaderInspectionSize)) {
        return false;
    }
    FileReaderCursorState inspected;
    const auto* reader = static_cast<const std::uint8_t*>(file_reader);
    std::memcpy(&inspected.base, reader + 0x0c, sizeof(inspected.base));
    std::memcpy(
        &inspected.buffer_size, reader + 0x10,
        sizeof(inspected.buffer_size));
    std::memcpy(
        &inspected.cursor, reader + 0x54, sizeof(inspected.cursor));
    std::memcpy(&inspected.end, reader + 0x58, sizeof(inspected.end));
    if (!inspected.base || inspected.cursor < inspected.base ||
        inspected.end < inspected.cursor ||
        inspected.end - inspected.base > inspected.buffer_size) {
        return false;
    }
    inspected.offset = inspected.cursor - inspected.base;
    inspected.remaining = inspected.end - inspected.cursor;
    *state = inspected;
    return true;
}

bool is_executable_pointer(void* pointer) noexcept {
    return pointer && !IsBadCodePtr(reinterpret_cast<FARPROC>(pointer));
}

bool executable_address_in_module(
    void* module, const void* address) noexcept {
    if (!module || !address) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) !=
            sizeof(information) ||
        information.State != MEM_COMMIT ||
        information.AllocationBase != module ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    switch (information.Protect & 0xffu) {
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
    }
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

bool restore_a1_instant_action_relationships(
    void* game_setup) noexcept {
    if (!g_armada || !game_setup ||
        InterlockedCompareExchange(
            &g_a1_relationship_restore_count, 0, 0) != 0) {
        return game_setup != nullptr;
    }

    void* is_active = at(g_armada, kGameSetupIsActiveSlotRva);
    void* get_side = at(g_armada, kGameSetupGetSideForSlotRva);
    void* get_alliance_team = at(
        g_armada, kGameSetupGetAllianceTeamForSlotRva);
    void* set_relationship = at(g_armada, kSideSetRelationshipRva);
    using SideLookupFn = void* (__cdecl*)(std::int32_t);
    const auto side_lookup = reinterpret_cast<SideLookupFn>(
        at(g_armada, kSideLookupRva));
    if (!is_executable_pointer(is_active) ||
        !is_executable_pointer(get_side) ||
        !is_executable_pointer(get_alliance_team) ||
        !is_executable_pointer(reinterpret_cast<void*>(side_lookup)) ||
        !is_executable_pointer(set_relationship)) {
        log_line("A1 Instant Action relationship restore rejected an "
                 "unsupported native helper");
        return false;
    }

    try {
        // This is the native A2 SetupGame relationship rule at Armada RVA
        // 0x00085323: inactive sides receive distinct sentinel team IDs,
        // active sides use GameSetup::GetTeam, side zero remains neutral,
        // equal teams are allied, and all other teams are enemies.
        std::array<std::int32_t, 9> alliance_team{};
        std::array<bool, 9> active_side{};
        for (std::size_t side = 0; side < alliance_team.size(); ++side) {
            alliance_team[side] = static_cast<std::int32_t>(side + 9);
        }

        std::ostringstream slots;
        bool first_slot = true;
        std::size_t active_count = 0;
        for (std::size_t slot = 0; slot < 8; ++slot) {
            const bool active =
                (a2fo_a1_call_thiscall_1(
                    is_active, game_setup, slot) & 0xffu) != 0;
            if (!active) continue;
            const auto side = static_cast<std::int32_t>(
                a2fo_a1_call_thiscall_1(
                    get_side, game_setup, slot));
            const auto team = static_cast<std::int32_t>(
                a2fo_a1_call_thiscall_1(
                    get_alliance_team, game_setup, slot));
            if (side < 1 || side >=
                    static_cast<std::int32_t>(alliance_team.size())) {
                continue;
            }
            alliance_team[static_cast<std::size_t>(side)] = team;
            active_side[static_cast<std::size_t>(side)] = true;
            ++active_count;
            if (!first_slot) slots << ',';
            first_slot = false;
            slots << slot << "=(side " << side << ",team " << team << ')';
        }
        if (active_count == 0) {
            log_line("A1 Instant Action relationship restore found no "
                     "active setup slots");
            return false;
        }

        std::array<void*, 9> sides{};
        for (std::size_t side = 0; side < sides.size(); ++side) {
            sides[side] = side_lookup(static_cast<std::int32_t>(side));
        }

        std::ostringstream changed_pairs;
        bool first_pair = true;
        for (std::size_t side = 0; side < sides.size(); ++side) {
            if (!sides[side]) continue;
            for (std::size_t other = 0; other < sides.size(); ++other) {
                const std::int32_t relationship = side == 0 || other == 0
                    ? 1
                    : (alliance_team[side] == alliance_team[other] ? 2 : 0);
                std::int32_t previous = -1;
                const auto* relation =
                    static_cast<const std::uint8_t*>(sides[side]) +
                    0x170 + other * sizeof(std::int32_t);
                if (readable_range(relation, sizeof(previous))) {
                    std::memcpy(&previous, relation, sizeof(previous));
                }
                a2fo_a1_call_thiscall_2(
                    set_relationship, sides[side], other,
                    static_cast<std::uintptr_t>(relationship));
                if (side < other && active_side[side] && active_side[other]) {
                    if (!first_pair) changed_pairs << ',';
                    first_pair = false;
                    changed_pairs << side << '-' << other << ':'
                                  << previous << "->" << relationship;
                }
            }
        }

        InterlockedExchange(&g_a1_relationship_restore_count, 1);
        std::ostringstream message;
        message << "A1 Instant Action relationships restored from setup: "
                << "slots=" << (first_slot ? "<none>" : slots.str())
                << "; activePairs="
                << (first_pair ? "<none>" : changed_pairs.str());
        log_line(message.str().c_str());
        return true;
    } catch (...) {
        log_line("A1 Instant Action relationship restore failed safely");
        return false;
    }
}

void restore_a1_relationships_before_elimination() noexcept {
    if (!g_armada ||
        InterlockedCompareExchange(
            &g_a1_selected_map_active, 0, 0) == 0) {
        return;
    }

    auto** transport_slot = at<void*>(g_armada, kTransportCurrentRva);
    if (!readable_range(transport_slot, sizeof(*transport_slot)) ||
        !*transport_slot) {
        return;
    }
    void* get_game_setup = at(g_armada, kTransportGetGameSetupRva);
    if (!is_executable_pointer(get_game_setup)) return;

    void* game_setup = reinterpret_cast<void*>(
        a2fo_a1_call_thiscall_0(get_game_setup, *transport_slot));
    if (game_setup) {
        restore_a1_instant_action_relationships(game_setup);
    }
}

void run_to_the_death_check(
    void* game_type, const void*) noexcept {
    restore_a1_relationships_before_elimination();
    void* gateway = g_a2fo_a1_to_the_death_check_gateway;
    if (game_type && is_executable_pointer(gateway)) {
        a2fo_a1_call_thiscall_0(gateway, game_type);
    }
}

bool read_a1_bzn_header_file(const std::string& path,
                             a1compat::A1BznHeader& header) noexcept {
    HANDLE file = CreateFileA(
        path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    std::array<std::uint8_t,
               a1compat::kMaximumA1BznHeaderInspectionSize> bytes{};
    DWORD read = 0;
    const bool succeeded = ReadFile(
        file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    CloseHandle(file);
    return succeeded && read != 0 &&
        a1compat::parse_a1_bzn_header(bytes.data(), read, &header);
}

bool find_a1_bzn_header(const char* filename,
                        a1compat::A1BznHeader& header,
                        std::string* source_path = nullptr) noexcept {
    if (!filename || !*filename) {
        return false;
    }
    try {
        std::string supplied(filename);
        if (supplied.size() > 1024) return false;
        for (char& character : supplied) {
            if (character == '/') character = '\\';
        }

        const auto try_path = [&](const std::string& path) noexcept {
            if (path.empty()) return false;
            a1compat::A1BznHeader parsed;
            if (!read_a1_bzn_header_file(path, parsed)) return false;
            header = parsed;
            if (source_path) *source_path = path;
            return true;
        };

        // GameSetup may supply an already-qualified virtual or physical path.
        // Trying it as-is also avoids prepending an extension root to a drive
        // path such as Z:\\...\\bzn\\2blue.bzn under Wine.
        if (try_path(supplied)) return true;

        if (!g_api || !g_api->extension_root_count ||
            !g_api->extension_root) {
            return false;
        }

        std::string relative = supplied;
        while (relative.size() >= 2 && relative[0] == '.' &&
               relative[1] == '\\') {
            relative.erase(0, 2);
        }
        const std::size_t separator = relative.find_last_of("\\/");
        const std::string basename = separator == std::string::npos
            ? relative : relative.substr(separator + 1);

        const std::uint32_t root_count = g_api->extension_root_count();
        if (root_count == 0 || root_count > 4096) return false;
        for (std::uint32_t position = root_count; position != 0; --position) {
            const char* root = g_api->extension_root(position - 1);
            if (!root || !*root) continue;
            const std::array<std::string, 3> candidates{{
                join_path(root, relative.c_str()),
                join_path(root,
                          (std::string("bzn\\") + basename).c_str()),
                join_path(root, basename.c_str())}};
            for (const std::string& path : candidates) {
                if (try_path(path)) return true;
            }
        }
    } catch (...) {
        return false;
    }
    return false;
}

bool read_a1_companion_mdf(
    const std::string& bzn_path, a1compat::A1MdfData& mdf,
    std::string* source_path = nullptr) noexcept {
    if (bzn_path.empty()) return false;
    try {
        std::string path = bzn_path;
        const std::size_t separator = path.find_last_of("\\/");
        const std::size_t extension = path.find_last_of('.');
        if (extension == std::string::npos ||
            (separator != std::string::npos && extension < separator)) {
            path += ".mdf";
        } else {
            path.replace(extension, std::string::npos, ".mdf");
        }

        HANDLE file = CreateFileA(
            path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;

        LARGE_INTEGER file_size{};
        const bool valid_size = GetFileSizeEx(file, &file_size) &&
            file_size.QuadPart > 0 &&
            file_size.QuadPart <=
                static_cast<LONGLONG>(kMaximumA1MdfFileSize);
        std::array<char, kMaximumA1MdfFileSize> bytes{};
        DWORD read = 0;
        const bool succeeded = valid_size && ReadFile(
            file, bytes.data(), static_cast<DWORD>(file_size.QuadPart),
            &read, nullptr);
        CloseHandle(file);
        if (!succeeded || read != static_cast<DWORD>(file_size.QuadPart) ||
            !a1compat::parse_a1_mdf(bytes.data(), read, &mdf)) {
            return false;
        }
        if (source_path) *source_path = path;
        return true;
    } catch (...) {
        return false;
    }
}

bool apply_a1_mdf_start_locations(
    void* details, const a1compat::A1BznHeader& header,
    const a1compat::A1MdfData& mdf) noexcept {
    if (!g_armada || !details || !header.has_map_bounds ||
        !writable_range(details, kMapDetailsRequiredSize)) {
        return false;
    }

    const auto* slot_count_address =
        at<const std::int32_t>(g_armada, kGameSetupSlotCountRva);
    if (!readable_range(slot_count_address, sizeof(*slot_count_address))) {
        return false;
    }
    const std::int32_t slot_count = *slot_count_address;
    if (slot_count < 1 || slot_count > 32 ||
        mdf.start_location_count >
            static_cast<std::size_t>(slot_count - 1)) {
        return false;
    }

    void* start_location_array = read_pointer_at(
        details, kMapDetailsStartLocationArrayOffset);
    if (!start_location_array || !readable_range(
            start_location_array,
            static_cast<std::size_t>(slot_count) * sizeof(void*))) {
        return false;
    }

    auto** assign_slot = at<void*>(g_armada, kNativeStringAssignIatRva);
    if (!readable_range(assign_slot, sizeof(*assign_slot)) ||
        !is_executable_pointer(*assign_slot)) {
        return false;
    }
    void* type_empty = at(g_armada, kStartLocationTypeEmptyRva);
    void* type_player = at(g_armada, kStartLocationTypePlayerRva);
    if (!readable_range(type_empty, 0x10) ||
        !readable_range(type_player, 0x10)) {
        return false;
    }

    // Validate every native record before making any changes.
    for (std::int32_t index = 1; index < slot_count; ++index) {
        void* location = read_pointer_at(
            start_location_array,
            static_cast<std::size_t>(index) * sizeof(void*));
        if (!location || !writable_range(
                location, kStartLocationDetailsSize)) {
            return false;
        }
    }

    for (std::int32_t index = 1; index < slot_count; ++index) {
        void* location = read_pointer_at(
            start_location_array,
            static_cast<std::size_t>(index) * sizeof(void*));
        const bool active = static_cast<std::size_t>(index) <=
            mdf.start_location_count;
        a2fo_a1_call_thiscall_1(
            *assign_slot,
            static_cast<std::uint8_t*>(location) +
                kStartLocationTypeOffset,
            reinterpret_cast<std::uintptr_t>(
                active ? type_player : type_empty));
        if (!active) continue;

        float position[3]{};
        if (!a1compat::a1_mdf_world_position(
                header, mdf.start_locations[index - 1], position)) {
            return false;
        }
        std::memcpy(
            static_cast<std::uint8_t*>(location) +
                kStartLocationPositionOffset,
            position, sizeof(position));
    }

    const std::int32_t native_count =
        static_cast<std::int32_t>(mdf.start_location_count);
    std::memcpy(
        static_cast<std::uint8_t*>(details) +
            kMapDetailsStartLocationCountOffset,
        &native_count, sizeof(native_count));
    return true;
}

bool regular_file_exists(const std::string& path) noexcept {
    if (path.empty()) return false;
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool build_legacy_aip_name(const char* requested_name,
                           std::string& legacy_name) {
    legacy_name.clear();
    if (!requested_name || !*requested_name) return false;

    std::string normalized(requested_name);
    if (normalized.size() > 192) return false;
    for (char& character : normalized) {
        const unsigned char value = static_cast<unsigned char>(character);
        if (!std::isalnum(value) && character != '_' && character != '-') {
            return false;
        }
        character = static_cast<char>(std::tolower(value));
    }

    std::size_t stem_end = normalized.size();
    while (stem_end != 0 && std::isdigit(
            static_cast<unsigned char>(normalized[stem_end - 1]))) {
        --stem_end;
    }
    constexpr std::size_t suffix_size =
        sizeof(kInstantActionBuildListSuffix) - 1;
    if (stem_end <= suffix_size ||
        normalized.compare(stem_end - suffix_size, suffix_size,
                           kInstantActionBuildListSuffix) != 0) {
        return false;
    }

    legacy_name.assign(normalized, 0, stem_end - suffix_size);
    legacy_name += kLegacyBuildListSuffix;
    return true;
}

bool aip_file_exists_in_root(const char* root,
                             const std::string& aip_name) {
    if (!root || !*root || aip_name.empty()) return false;
    const std::string relative =
        std::string("AI\\AIPs\\") + aip_name + ".aip";
    return regular_file_exists(join_path(root, relative.c_str()));
}

bool mod_chain_prefers_legacy_aip(
    const std::string& requested_name,
    const std::string& legacy_name) noexcept {
    if (!g_api || !g_api->extension_root_count ||
        !g_api->extension_root) {
        return false;
    }

    try {
        const std::uint32_t root_count = g_api->extension_root_count();
        if (root_count == 0 || root_count > 4096) return false;

        std::uint32_t a1_root = root_count;
        for (std::uint32_t index = 0; index < root_count; ++index) {
            const char* root = g_api->extension_root(index);
            if (regular_file_exists(join_path(root, kA1CompatIniFileName))) {
                a1_root = index;
                break;
            }
        }
        if (a1_root == root_count) return false;

        // Resolve the highest A1-layer file explicitly, rather than allowing
        // Data or an A2 compatibility parent to select a same-named FO plan.
        for (std::uint32_t position = root_count;
             position > a1_root; --position) {
            const char* root = g_api->extension_root(position - 1);
            const bool requested_exists =
                aip_file_exists_in_root(root, requested_name);
            const bool legacy_exists =
                aip_file_exists_in_root(root, legacy_name);
            if (requested_exists || legacy_exists) {
                return legacy_exists && !requested_exists;
            }
        }
    } catch (...) {
        return false;
    }
    return false;
}

void* resolve_aip_lookup(void* manager,
                         const char* requested_name) noexcept {
    void* gateway = g_a2fo_a1_aip_lookup_gateway;
    if (!gateway || !manager || !requested_name || !*requested_name) {
        return nullptr;
    }

    try {
        std::string legacy_name;
        const std::string requested(requested_name);
        if (build_legacy_aip_name(requested_name, legacy_name) &&
            mod_chain_prefers_legacy_aip(requested, legacy_name)) {
            void* legacy_aip = reinterpret_cast<void*>(
                a2fo_a1_call_thiscall_1(
                    gateway, manager,
                    reinterpret_cast<std::uintptr_t>(legacy_name.c_str())));
            const LONG count = InterlockedIncrement(
                &g_legacy_aip_name_fallback_count);
            if (count <= 32) {
                char message[512]{};
                std::snprintf(
                    message, sizeof(message),
                    "Armada 1 AIP name fallback #%ld: '%s' -> '%s' "
                    "(loaded=%s)",
                    static_cast<long>(count), requested_name,
                    legacy_name.c_str(), legacy_aip ? "yes" : "no");
                log_line(message);
            } else if (count == 33) {
                log_line("Further Armada 1 AIP name fallbacks suppressed");
            }
            if (legacy_aip) return legacy_aip;
        }
    } catch (...) {
        // A compatibility lookup must never prevent the native name lookup.
    }

    return reinterpret_cast<void*>(a2fo_a1_call_thiscall_1(
        gateway, manager,
        reinterpret_cast<std::uintptr_t>(requested_name)));
}

void report_missing_aip_technology_unit(
    const char* aip_name, const char* unit_name) noexcept {
    const LONG count = InterlockedIncrement(
        &g_missing_aip_technology_unit_count);
    if (count > 64) {
        if (count == 65) {
            log_line("Further unresolved A1/A2 AIP unit reports suppressed");
        }
        return;
    }

    const char* safe_aip = aip_name && !IsBadStringPtrA(aip_name, 256)
        ? aip_name : "<unavailable>";
    const char* safe_unit = unit_name && !IsBadStringPtrA(unit_name, 128)
        ? unit_name : "<unavailable>";
    char message[640]{};
    std::snprintf(
        message, sizeof(message),
        "Skipped unresolved AIP technology unit #%ld: aip='%s', unit='%s'; "
        "native A2 would dereference null at RVA 0x000248a9",
        static_cast<long>(count), safe_aip, safe_unit);
    log_line(message);
}

bool apply_a1_world_bounds(
    const a1compat::A1BznHeader& header) noexcept {
    if (!g_armada || !header.has_map_bounds) return false;
    const auto* serialized_minimum =
        at<float>(g_armada, kWorldSerializedMinimumExtentRva);
    const auto* serialized_maximum =
        at<float>(g_armada, kWorldSerializedMaximumExtentRva);
    const auto* remap_enabled =
        at<std::uint8_t>(g_armada, kWorldExtentRemapEnabledRva);
    auto* minimum = at<float>(g_armada, kWorldMinimumExtentRva);
    auto* maximum = at<float>(g_armada, kWorldMaximumExtentRva);
    if (!writable_range(minimum, sizeof(float) * 3) ||
        !writable_range(maximum, sizeof(float) * 3)) {
        return false;
    }

    float previous_minimum[3]{};
    float previous_maximum[3]{};
    float source_minimum[3]{};
    float source_maximum[3]{};
    std::memcpy(previous_minimum, minimum, sizeof(previous_minimum));
    std::memcpy(previous_maximum, maximum, sizeof(previous_maximum));
    const bool have_source =
        readable_range(serialized_minimum, sizeof(source_minimum)) &&
        readable_range(serialized_maximum, sizeof(source_maximum));
    if (have_source) {
        std::memcpy(source_minimum, serialized_minimum, sizeof(source_minimum));
        std::memcpy(source_maximum, serialized_maximum, sizeof(source_maximum));
    }
    const bool remapping =
        readable_range(remap_enabled, sizeof(*remap_enabled)) &&
        *remap_enabled != 0;

    float target_minimum[3]{};
    float target_size[3]{};
    if (!a1compat::a2_compatible_map_bounds(
            header, target_minimum, target_size)) {
        return false;
    }
    float computed_maximum[3]{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        computed_maximum[axis] = target_minimum[axis] + target_size[axis];
    }
    std::memcpy(minimum, target_minimum, sizeof(target_minimum));
    std::memcpy(maximum, computed_maximum, sizeof(computed_maximum));

    const LONG count = InterlockedIncrement(&g_a1_bzn_world_bounds_count);
    if (count <= 8) {
        char message[640]{};
        std::snprintf(
            message, sizeof(message),
            "Armada 1 BZN live world bounds #%ld: remap=%s, "
            "sourceMin=(%.2f, %.2f, %.2f), sourceMax=(%.2f, %.2f, %.2f), "
            "previousMin=(%.2f, %.2f, %.2f), "
            "previousMax=(%.2f, %.2f, %.2f), "
            "targetMin=(%.2f, %.2f, %.2f), targetSize=(%.2f, %.2f, %.2f)",
            static_cast<long>(count), remapping ? "yes" : "no",
            source_minimum[0], source_minimum[1], source_minimum[2],
            source_maximum[0], source_maximum[1], source_maximum[2],
            previous_minimum[0], previous_minimum[1], previous_minimum[2],
            previous_maximum[0], previous_maximum[1], previous_maximum[2],
            target_minimum[0], target_minimum[1], target_minimum[2],
            target_size[0], target_size[1], target_size[2]);
        log_line(message);
    } else if (count == 9) {
        log_line("Further Armada 1 BZN live-bound reports suppressed");
    }
    return true;
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

bool is_neutral_race_odf(const char* value) noexcept {
    if (!value || !*value) return false;
    const char* basename = value;
    for (const char* cursor = value; *cursor != '\0'; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') basename = cursor + 1;
    }
    const std::size_t length = std::strlen(basename);
    if (length == std::strlen(kNeutralRaceOdfName) &&
        _stricmp(basename, kNeutralRaceOdfName) == 0) {
        return true;
    }
    return length == std::strlen("norace") &&
        _stricmp(basename, "norace") == 0;
}

std::uintptr_t native_race_entry_lookup(
    void* parameter_db, const char* key, char* output,
    std::uintptr_t output_size, const char* default_value) noexcept {
    return a2fo_a1_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(output), output_size,
        reinterpret_cast<std::uintptr_t>(default_value));
}

std::uintptr_t resolve_race_count(
    void* parameter_db, const char* key, std::int32_t* output,
    std::int32_t default_value) noexcept {
    const std::uintptr_t found = a2fo_a1_call_thiscall_3(
        at(g_armada, kParameterDbGetIntRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(output),
        static_cast<std::uintptr_t>(default_value));
    InterlockedExchange(&g_synthetic_neutral_race_index, -1);
    if (!key || _stricmp(key, "numberOfRaces") != 0 || !output ||
        !writable_range(output, sizeof(*output))) {
        return found;
    }

    const std::int32_t declared_count = *output;
    if (declared_count <= 0 ||
        declared_count >= kMaximumLegacyRaceRecords - 1) {
        return found;
    }

    char field_name[32]{};
    char value[160]{};
    for (std::int32_t index = 0; index < declared_count; ++index) {
        std::snprintf(field_name, sizeof(field_name), "race%ld",
                      static_cast<long>(index));
        value[0] = '\0';
        native_race_entry_lookup(
            parameter_db, field_name, value, sizeof(value), "");
        value[sizeof(value) - 1] = '\0';
        if (is_neutral_race_odf(value)) return found;
    }

    // Do not overwrite a dormant declaration immediately beyond the stated
    // count. The supported A1 registry has no race10; this check keeps the
    // missing-only rule conservative for other parent/child combinations.
    std::snprintf(field_name, sizeof(field_name), "race%ld",
                  static_cast<long>(declared_count));
    value[0] = '\0';
    const std::uintptr_t next_found = native_race_entry_lookup(
        parameter_db, field_name, value, sizeof(value), "");
    value[sizeof(value) - 1] = '\0';
    if ((next_found & 0xffu) != 0 && value[0] != '\0' &&
        !is_neutral_race_odf(value)) {
        log_line("A2 Classic neutral Race registry default skipped: next "
                 "undeclared race entry is already occupied");
        return found;
    }

    *output = declared_count + 1;
    if (!is_neutral_race_odf(value)) {
        InterlockedExchange(
            &g_synthetic_neutral_race_index, declared_count);
    }

    const LONG incident = InterlockedIncrement(
        &g_neutral_race_registry_default_count);
    if (incident <= 16) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A2 Classic neutral Race registry default #%ld: appended "
            "race%ld='%s' after %ld A1 records",
            static_cast<long>(incident),
            static_cast<long>(declared_count), kNeutralRaceOdfName,
            static_cast<long>(declared_count));
        log_line(message);
    } else if (incident == 17) {
        log_line("Further A2 Classic neutral Race registry defaults "
                 "suppressed");
    }
    return found;
}

std::uintptr_t resolve_race_entry(
    void* parameter_db, const char* key, char* output,
    std::uintptr_t output_size, const char* default_value) noexcept {
    const std::uintptr_t found = native_race_entry_lookup(
        parameter_db, key, output, output_size, default_value);
    const LONG synthetic_index = InterlockedCompareExchange(
        &g_synthetic_neutral_race_index, -1, -1);
    if (synthetic_index < 0 || !key || !output) return found;

    char expected_key[32]{};
    std::snprintf(expected_key, sizeof(expected_key), "race%ld",
                  static_cast<long>(synthetic_index));
    if (_stricmp(key, expected_key) != 0 ||
        (found & 0xffu) != 0) {
        return found;
    }

    const std::size_t required = sizeof(kNeutralRaceOdfName);
    if (output_size < required || !writable_range(output, required)) {
        return found;
    }
    std::memcpy(output, kNeutralRaceOdfName, required);
    return 1;
}

bool race_event_field(const A2FO_OdfFieldView* fields,
                      std::uint32_t count, const char* name,
                      std::string* value = nullptr) {
    if (!fields || !name) return false;
    const std::size_t name_size = std::strlen(name);
    for (std::uint32_t index = 0; index < count; ++index) {
        const A2FO_OdfFieldView& field = fields[index];
        if (!field.name.data || field.name.size != name_size ||
            _strnicmp(field.name.data, name, name_size) != 0 ||
            (!field.value.data && field.value.size != 0)) {
            continue;
        }
        if (value) {
            value->assign(field.value.data ? field.value.data : "",
                          field.value.size);
        }
        return true;
    }
    return false;
}

bool parse_nonnegative_int32(
    const std::string& text, std::int32_t& value) noexcept {
    if (text.empty()) return false;
    errno = 0;
    char* end = nullptr;
    const long long parsed = std::strtoll(text.c_str(), &end, 10);
    while (end && (*end == ' ' || *end == '\t' ||
                   *end == '\r' || *end == '\n')) {
        ++end;
    }
    if (!end || end == text.c_str() || *end != '\0' || errno == ERANGE ||
        parsed < 0 || parsed > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    value = static_cast<std::int32_t>(parsed);
    return true;
}

std::int32_t race_starting_resource_value(
    const A2FO_RaceLoadedEvent* event, const char* command,
    std::int32_t fallback) {
    std::string text;
    if (!event || !race_event_field(
            event->odf_fields, event->odf_field_count, command, &text)) {
        return fallback;
    }
    std::int32_t value = fallback;
    return parse_nonnegative_int32(text, value) ? value : fallback;
}

bool apply_legacy_native_starting_resources(
    const A2FO_RaceLoadedEvent* event) noexcept {
    if (!event || !event->race) return false;
    const std::array<std::size_t, 3> native_indices{{
        kNativeCrewResourceIndex, kNativeDilithiumResourceIndex,
        kNativeMetalResourceIndex}};
    bool applied = true;
    for (std::size_t command_index = 0;
         command_index < native_indices.size(); ++command_index) {
        const std::int32_t normal = race_starting_resource_value(
            event, kNormalStartingResourceCommands[command_index],
            g_legacy_starting_resource_defaults.normal[command_index]);
        const std::int32_t lots = race_starting_resource_value(
            event, kLotsStartingResourceCommands[command_index],
            g_legacy_starting_resource_defaults.lots[command_index]);
        const std::size_t native_index = native_indices[command_index];
        applied = write_int32_at(
            event->race,
            kRaceStartingResourcesOffset +
                native_index * sizeof(std::int32_t),
            normal) && applied;
        applied = write_int32_at(
            event->race,
            kRaceStartingResourcesOffset +
                (kNativeStartingResourceCount + native_index) *
                    sizeof(std::int32_t),
            lots) && applied;
    }
    return applied;
}

bool write_race_display_key(void* race,
                            std::string_view display_key) noexcept {
    if (!race) return false;
    auto* destination = static_cast<std::uint8_t*>(race) +
        kRaceDisplayKeyOffset;
    if (!writable_range(destination, kRaceDisplayKeyCapacity)) {
        return false;
    }
    std::memset(destination, 0, kRaceDisplayKeyCapacity);
    const std::size_t maximum = kRaceDisplayKeyCapacity - 1;
    const std::size_t copy_size =
        display_key.size() < maximum ? display_key.size() : maximum;
    if (copy_size != 0) {
        std::memcpy(destination, display_key.data(), copy_size);
    }
    return true;
}

void reset_legacy_race_menu_state() noexcept {
    g_legacy_race_records.clear();
    g_legacy_race_fallback_logged = false;
}

void apply_legacy_race_menu_plan(
    const a1compat::LegacyRaceMenuPlan& plan) noexcept {
    if (!plan.fallback_active ||
        plan.entries.size() != g_legacy_race_records.size()) {
        return;
    }

    bool applied = true;
    for (std::size_t index = 0; index < plan.entries.size(); ++index) {
        const LegacyRaceRuntimeRecord& source =
            g_legacy_race_records[index];
        const a1compat::LegacyRaceMenuEntry& entry = plan.entries[index];
        applied = write_int32_at(
            source.race, kRaceInstantActionSlotOffset,
            entry.instant_action_slot) && applied;
        if (entry.replace_display_key) {
            applied = write_race_display_key(
                source.race, entry.display_key) && applied;
        }
    }
    applied = write_int32_at(
        g_armada, kRaceNumberOfInstantActionSlotsRva,
        plan.playable_count) && applied;
    if (!applied) {
        log_line("Legacy race-menu fallback could not update every native "
                 "Race field");
    }
}

void A2FO_CALL legacy_race_loaded_handler(
    const A2FO_RaceLoadedEvent* event, void*) {
    if (!g_race_menu_callback_ready || !g_armada || !event ||
        event->struct_size < sizeof(*event) || !event->race) {
        return;
    }

    try {
        const std::int32_t declared_race_count = read_int32_at(
            g_armada, kRaceNumberOfRacesRva, 0);
        if (declared_race_count <= 0 ||
            declared_race_count > kMaximumLegacyRaceRecords) {
            log_line("Legacy race-menu fallback ignored an invalid "
                     "numberOfRaces value");
            return;
        }

        const std::int32_t race_identifier = read_int32_at(
            event->race, kRaceIdentifierOffset, -1);
        if (race_identifier < 0 ||
            race_identifier >= declared_race_count) {
            // Race objects outside Race::InitAll's active races.odf sequence
            // are not declarations and must not influence the IA dropdown.
            return;
        }
        // Modules are initialized before Armada parses the effective
        // RTS_CFG.h. A raw A1 file does not define the A2 MAX_* commands, so
        // that later parse resets these globals to zero after our startup
        // validation/write. The first real Race-loaded event is the earliest
        // stable post-configuration boundary shared by shell and gameplay.
        if (InterlockedCompareExchange(
                &g_legacy_resource_maximum_post_config_pending, 0, 1) == 1) {
            if (apply_legacy_native_resource_limits()) {
                log_line("A1 legacy resource maxima reaffirmed after "
                         "RTS configuration");
            } else {
                log_line("A1 legacy resource-maximum post-configuration "
                         "bridge failed");
                InterlockedExchange(
                    &g_legacy_resource_maximum_post_config_pending, 1);
            }
        }
        const LONG synthetic_neutral_index = InterlockedCompareExchange(
            &g_synthetic_neutral_race_index, -1, -1);
        if (race_identifier != synthetic_neutral_index &&
            !apply_legacy_native_starting_resources(event)) {
            log_line("A1 Race starting-resource defaults could not update "
                     "the native Race matrix");
        }
        if (!g_legacy_race_records.empty() && race_identifier == 0) {
            reset_legacy_race_menu_state();
        }
        if (static_cast<std::size_t>(race_identifier) !=
            g_legacy_race_records.size()) {
            log_line("Legacy race-menu fallback ignored a non-sequential "
                     "Race-loaded event");
            return;
        }
        if (g_legacy_race_records.empty()) {
            g_legacy_race_records.reserve(
                static_cast<std::size_t>(declared_race_count));
        }

        LegacyRaceRuntimeRecord record;
        record.race = event->race;
        record.input.has_instant_action_slot = race_event_field(
            event->odf_fields, event->odf_field_count,
            "instantActionSlot");
        record.input.instant_action_slot = read_int32_at(
            event->race, kRaceInstantActionSlotOffset, -1);
        record.input.has_interface_configuration = race_event_field(
            event->odf_fields, event->odf_field_count,
            "interfaceConfiguration");
        record.input.has_display_key = race_event_field(
            event->odf_fields, event->odf_field_count, "displayKey");
        record.input.has_display_name = race_event_field(
            event->odf_fields, event->odf_field_count, "displayName",
            &record.input.display_name);
        g_legacy_race_records.push_back(std::move(record));

        std::vector<a1compat::LegacyRaceMenuInput> inputs;
        inputs.reserve(g_legacy_race_records.size());
        for (const LegacyRaceRuntimeRecord& loaded :
             g_legacy_race_records) {
            inputs.push_back(loaded.input);
        }
        const a1compat::LegacyRaceMenuPlan plan =
            a1compat::build_legacy_race_menu_plan(
                inputs, static_cast<std::size_t>(declared_race_count));
        if (plan.fallback_active) {
            if (!g_legacy_race_fallback_logged) {
                char message[256]{};
                std::snprintf(
                    message, sizeof(message),
                    "Legacy race-menu fallback activated at race%lu; "
                    "playability follows interfaceConfiguration",
                    static_cast<unsigned long>(race_identifier));
                log_line(message);
                g_legacy_race_fallback_logged = true;
            }
            apply_legacy_race_menu_plan(plan);
        }

        if (g_legacy_race_records.size() ==
            static_cast<std::size_t>(declared_race_count)) {
            char message[256]{};
            if (plan.fallback_active) {
                std::snprintf(
                    message, sizeof(message),
                    "Legacy race-menu fallback finalized: %ld Race "
                    "records, %d playable entries",
                    static_cast<long>(declared_race_count),
                    plan.playable_count);
            } else {
                std::snprintf(
                    message, sizeof(message),
                    "Race menu retains native FO slot/displayKey policy: "
                    "%ld valid Race records",
                    static_cast<long>(declared_race_count));
            }
            log_line(message);
        }
    } catch (...) {
        log_line("Legacy race-menu fallback skipped after an unexpected "
                 "C++ exception");
    }
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
        {"fedoff", "federation"},
        {"bbase", "borg"},      {"bconst", "borg"},
        {"borgoff", "borg"},
        {"kbase", "klingon"},   {"kconst", "klingon"},
        {"klingoff", "klingon"},
        {"rbase", "romulan"},   {"rconst", "romulan"},
        {"romoff", "romulan"},
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

bool command_info_path_is_scout(const char* odf_name) noexcept {
    if (!odf_name) return false;
    const char* end = static_cast<const char*>(
        std::memchr(odf_name, '\0', 260));
    if (!end) return false;
    const char* basename = odf_name;
    for (const char* cursor = odf_name; cursor != end; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') basename = cursor + 1;
    }
    return _stricmp(basename, "scout.odf") == 0;
}

void* __attribute__((fastcall)) command_info_build_class_hook(
    void* command_info, void*, const char* odf_name) noexcept {
    void* built = reinterpret_cast<void*>(a2fo_a1_call_thiscall_1(
        g_command_info_build_class_hook.gateway, command_info,
        reinterpret_cast<std::uintptr_t>(odf_name)));
    auto* bytes = static_cast<std::uint8_t*>(built ? built : command_info);
    if (!bytes || !command_info_path_is_scout(odf_name) ||
        !readable_range(bytes, kCommandInfoMenuOffset + sizeof(std::int32_t)) ||
        !writable_range(bytes, kCommandInfoMenuOffset + sizeof(std::int32_t))) {
        return built;
    }

    // A valid selected/parent command named scout.odf wins. The A1 collision
    // is uniquely missing buttonName because the selected file is a ship base,
    // not a CommandInfo ODF.
    auto* button_name = reinterpret_cast<char*>(
        bytes + kCommandInfoButtonNameOffset);
    if (button_name[0] != '\0' || !g_armada ||
        std::memcmp(at(g_armada, kCommandNameToIdRva),
                    kExpectedCommandNameToId,
                    sizeof(kExpectedCommandNameToId)) != 0) {
        return built;
    }

    using CommandNameToId = std::int32_t (__cdecl*)(const char*);
    const auto command_name_to_id = reinterpret_cast<CommandNameToId>(
        at(g_armada, kCommandNameToIdRva));
    const std::int32_t scout_command_id = command_name_to_id("SCOUT");
    if (scout_command_id < 0 || scout_command_id >= 57) return built;

    const auto write_text = [&](std::size_t offset, std::size_t capacity,
                                const char* value) {
        char* destination = reinterpret_cast<char*>(bytes + offset);
        std::memset(destination, 0, capacity);
        const std::size_t copied = std::min(
            std::strlen(value), capacity - 1);
        std::memcpy(destination, value, copied);
    };
    write_text(kCommandInfoButtonNameOffset, 0x40, "scout");
    write_text(kCommandInfoTooltipOffset, 0x40, "GUI_CP_SCOUT_TOOLTIP");
    write_text(kCommandInfoVerboseOffset, 0x100, "GUI_CP_SCOUT_VTOOLTIP");

    *reinterpret_cast<std::int32_t*>(
        bytes + kCommandInfoCommandIdOffset) = scout_command_id;
    *reinterpret_cast<std::uint8_t*>(
        bytes + kCommandInfoSourceTypeOrOffset) = 0;
    *reinterpret_cast<std::int32_t*>(
        bytes + kCommandInfoNeedsTargetOffset) = 0;
    *reinterpret_cast<std::int32_t*>(
        bytes + kCommandInfoParamOffset) = -1;
    *reinterpret_cast<std::uint8_t*>(
        bytes + kCommandInfoDisplayTradeOffset) = 0;
    *reinterpret_cast<std::uint8_t*>(
        bytes + kCommandInfoIsBuyOffset) = 0;
    *reinterpret_cast<std::uint8_t*>(
        bytes + kCommandInfoIsToggleOffset) = 0;
    auto* position = reinterpret_cast<std::int32_t*>(
        bytes + kCommandInfoPreferredPositionOffset);
    position[0] = 1;
    position[1] = 1;
    position[2] = 0;
    position[3] = 0;
    *reinterpret_cast<std::uint32_t*>(
        bytes + kCommandInfoSourceOffset) =
        0x00000002u | kExploreMenuCapability;
    *reinterpret_cast<std::uint32_t*>(
        bytes + kCommandInfoSourceNotOffset) = 0;
    *reinterpret_cast<std::uint32_t*>(
        bytes + kCommandInfoDestinationOffset) = 0;
    *reinterpret_cast<std::int32_t*>(
        bytes + kCommandInfoMenuOffset) =
        static_cast<std::int32_t>(kLegacyPopupOrdersMenu);

    const LONG incident = InterlockedIncrement(
        &g_legacy_explore_command_collision_count);
    if (incident <= 4) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 scout/Explore command basename collision repaired #%ld: "
            "odf='%s', commandInfo=%p, commandId=%ld, source=0x%08lx, "
            "menu=orders",
            static_cast<long>(incident), odf_name, bytes,
            static_cast<long>(scout_command_id),
            static_cast<unsigned long>(
                0x00000002u | kExploreMenuCapability));
        log_line(message);
    }
    return built;
}

bool install_legacy_explore_command_bridge(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->install_inline_hook) return false;
    return api->install_inline_hook(
        at(g_armada, kCommandInfoBuildClassRva),
        reinterpret_cast<void*>(&command_info_build_class_hook),
        sizeof(kExpectedCommandInfoBuildClass),
        kExpectedCommandInfoBuildClass, &g_command_info_build_class_hook);
}

void A2FO_CALL legacy_menu_capability_class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    if (!event || !event->object_class) return;

    char odf_name[128]{};
    if (event->source_odf.data && event->source_odf.size != 0) {
        const std::size_t copied = std::min<std::size_t>(
            event->source_odf.size, sizeof(odf_name) - 1);
        std::memcpy(odf_name, event->source_odf.data, copied);
        odf_name[copied] = '\0';
    }

    std::string scout_text;
    bool legacy_scout = false;
    legacy_scout =
        race_event_field(event->odf_fields, event->odf_field_count,
                         "scout", &scout_text) &&
        parse_bool_config_value(scout_text, legacy_scout) && legacy_scout;

    std::string starbase_text;
    bool legacy_station = false;
    legacy_station =
        race_event_field(event->odf_fields, event->odf_field_count,
                         "is_starbase", &starbase_text) &&
        parse_bool_config_value(starbase_text, legacy_station) &&
        legacy_station;
    std::string class_label_text;
    const bool legacy_starbase_producer =
        race_event_field(event->odf_fields, event->odf_field_count,
                         "classLabel", &class_label_text) &&
        is_starbase_label(class_label_text.c_str());
    const bool legacy_moon =
        matches_numbered_odf_family(odf_name, "mdmoon") ||
        matches_numbered_odf_family(odf_name, "mmooninf");
    if (!legacy_scout && !legacy_station && !legacy_moon) return;

    struct MissingMenuCapability {
        const char* command;
        std::uint32_t bit;
    };
    constexpr std::array<MissingMenuCapability, 4> scout_capabilities{{
        {"combat", kCombatMenuCapability},
        {"alert", kAlertMenuCapability},
        {"can_sandd", kSearchAndDestroyMenuCapability},
        {"can_explore", kExploreMenuCapability},
    }};
    constexpr std::array<MissingMenuCapability, 4> station_capabilities{{
        {"transporter", kTransporterMenuCapability},
        {"facility", kFacilityMenuCapability},
        {"has_crew", kHasCrewMenuCapability},
        {"has_hitpoints", kHasHitpointsMenuCapability},
    }};
    constexpr std::array<MissingMenuCapability, 2> moon_capabilities{{
        {"spatial_object", kSpatialObjectMenuCapability},
        {"has_resource", kHasResourceMenuCapability},
    }};

    auto* menu_capabilities = reinterpret_cast<volatile LONG*>(
        static_cast<std::uint8_t*>(event->object_class) +
        kGameObjectClassMenuCapabilitiesOffset);
    if (!readable_range(
            const_cast<const LONG*>(menu_capabilities), sizeof(LONG)) ||
        !writable_range(
            const_cast<LONG*>(menu_capabilities), sizeof(LONG))) {
        return;
    }

    const auto apply_missing = [&](const auto& capabilities,
                                   volatile LONG* counter,
                                   const char* description) {
        std::uint32_t missing_mask = 0;
        std::string applied;
        for (const auto& capability : capabilities) {
            // Any explicit/inherited declaration, including zero, is
            // authoritative. Only a genuinely absent command receives A2's
            // base-class value.
            if (race_event_field(event->odf_fields, event->odf_field_count,
                                 capability.command)) {
                continue;
            }
            missing_mask |= capability.bit;
            if (!applied.empty()) applied += ",";
            applied += capability.command;
        }
        if (missing_mask == 0) return;

        const LONG previous = InterlockedOr(
            menu_capabilities, static_cast<LONG>(missing_mask));
        const LONG updated = previous | static_cast<LONG>(missing_mask);
        const LONG incident = InterlockedIncrement(counter);
        if (incident <= 32) {
            char message[512]{};
            std::snprintf(
                message, sizeof(message),
                "A2 Classic %s defaults #%ld: odf='%s', class=%p, "
                "capabilities=0x%08lx->0x%08lx, applied=%s",
                description, static_cast<long>(incident),
                odf_name[0] ? odf_name : "<unavailable>",
                event->object_class, static_cast<unsigned long>(previous),
                static_cast<unsigned long>(updated), applied.c_str());
            log_line(message);
        } else if (incident == 33) {
            char message[160]{};
            std::snprintf(
                message, sizeof(message),
                "Further A2 Classic %s defaults suppressed", description);
            log_line(message);
        }
    };

    if (legacy_scout) {
        apply_missing(scout_capabilities,
                      &g_legacy_scout_menu_capability_count,
                      "scout menu");
    }
    if (legacy_station) {
        apply_missing(station_capabilities,
                      &g_legacy_station_menu_capability_count,
                      "station command");
        if (legacy_starbase_producer && !race_event_field(
                event->odf_fields, event->odf_field_count,
                "builder_ship")) {
            ensure_starbase_build_menu_capability(
                event->object_class, odf_name);
        }
        if (legacy_starbase_producer) {
            register_completed_starbase_class_policy(event, odf_name);
        }
    }
    if (legacy_moon) {
        apply_missing(moon_capabilities,
                      &g_legacy_moon_menu_capability_count,
                      "moon target");
    }
}

std::string read_small_text_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 ||
        size > static_cast<std::streamoff>(kMaximumConfigTextSize)) {
        return {};
    }
    input.seekg(0, std::ios::beg);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

bool detect_incompatible_legacy_cursor_table(
    std::string& source_path,
    bool* tactical_override_required) noexcept {
    source_path.clear();
    if (tactical_override_required) {
        *tactical_override_required = false;
    }
    if (!g_api || !g_api->extension_root_count || !g_api->extension_root) {
        return false;
    }
    try {
        const std::uint32_t count = g_api->extension_root_count();
        if (count == 0 || count > 4096) return false;

        // Roots are ordered from lowest to highest precedence. Retain the
        // last cursor.spr found so this examines the table the game will
        // actually include from sprites.spr.
        for (std::uint32_t index = 0; index < count; ++index) {
            const char* root = g_api->extension_root(index);
            if (!root || !*root) continue;
            const std::array<std::string, 2> candidates{{
                join_path(root, "Sprites\\cursor.spr"),
                join_path(root, "sprites\\cursor.spr")}};
            for (const std::string& candidate : candidates) {
                if (regular_file_exists(candidate)) {
                    source_path = candidate;
                    break;
                }
            }
        }
        if (source_path.empty()) return false;

        const std::string contents = read_small_text_file(source_path);
        if (contents.empty()) return false;
        bool standard_cursor_found = false;
        bool standard_cursor_invalid = false;
        bool select_cursor_found = false;
        bool select_cursor_invalid = false;
        std::istringstream lines(contents);
        std::string line;
        while (std::getline(lines, line)) {
            const std::size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos || line[first] == '#') continue;

            std::istringstream fields(line.substr(first));
            std::string name;
            std::string texture;
            std::int32_t u = 0;
            std::int32_t v = 0;
            std::int32_t width = 0;
            std::int32_t height = 0;
            if (!(fields >> name >> texture >> u >> v >> width >> height)) {
                continue;
            }
            std::transform(
                name.begin(), name.end(), name.begin(),
                [](unsigned char value) {
                    return static_cast<char>(std::tolower(value));
                });
            if (name != "standard_cursor" && name != "c_select") {
                continue;
            }

            std::string normalized = line;
            std::transform(
                normalized.begin(), normalized.end(), normalized.begin(),
                [](unsigned char value) {
                    return static_cast<char>(std::tolower(value));
                });
            const bool cursor_flag =
                normalized.find("@cursor") != std::string::npos;
            const bool invalid =
                !cursor_flag || width > 40 || height > 40 ||
                width <= 0 || height <= 0;
            if (name == "standard_cursor") {
                standard_cursor_found = true;
                standard_cursor_invalid = invalid;
            } else {
                select_cursor_found = true;
                select_cursor_invalid = invalid;
            }
        }
        if (tactical_override_required) {
            *tactical_override_required =
                !select_cursor_found || select_cursor_invalid;
        }
        return !standard_cursor_found || standard_cursor_invalid;
    } catch (...) {
        source_path.clear();
        if (tactical_override_required) {
            *tactical_override_required = false;
        }
    }
    return false;
}

std::string find_team_color_odf_in_root(const char* root) {
    if (!root || !*root) return {};
    for (const char* relative : kTeamColorRelativePaths) {
        const std::string path = join_path(root, relative);
        if (path.empty()) continue;
        const DWORD attributes = GetFileAttributesA(path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            return path;
        }
    }
    return {};
}

bool load_legacy_team_color_palette() noexcept {
    g_legacy_team_color_palette = {};
    g_legacy_team_color_palette_active = false;
    if (!g_api || !g_api->extension_root_count || !g_api->extension_root) {
        return false;
    }

    try {
        const std::uint32_t root_count = g_api->extension_root_count();
        if (root_count == 0 || root_count > 4096) return false;
        std::uint32_t parsed_files = 0;
        std::uint32_t indexed_values = 0;
        std::uint32_t legacy_aliases = 0;
        for (std::uint32_t index = 0; index < root_count; ++index) {
            const std::string path = find_team_color_odf_in_root(
                g_api->extension_root(index));
            if (path.empty()) continue;
            const std::string contents = read_small_text_file(path);
            if (contents.empty()) continue;
            const a1compat::TeamColorMergeResult merged =
                a1compat::merge_team_color_odf(
                    contents, g_legacy_team_color_palette);
            indexed_values += merged.indexed_values;
            legacy_aliases += merged.legacy_aliases;
            ++parsed_files;
        }
        g_legacy_team_color_palette_active = legacy_aliases != 0;
        if (!g_legacy_team_color_palette_active) return false;

        std::uint32_t resolved_colors = 0;
        for (bool present : g_legacy_team_color_palette.present) {
            if (present) ++resolved_colors;
        }
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 team-colour palette resolved: %lu/16 player colours from "
            "%lu teamcolor.odf layer(s), %lu indexed values and %lu legacy "
            "aliases",
            static_cast<unsigned long>(resolved_colors),
            static_cast<unsigned long>(parsed_files),
            static_cast<unsigned long>(indexed_values),
            static_cast<unsigned long>(legacy_aliases));
        log_line(message);
        return true;
    } catch (...) {
        g_legacy_team_color_palette = {};
        g_legacy_team_color_palette_active = false;
        log_line("A1 team-colour palette resolution failed");
        return false;
    }
}

bool apply_legacy_team_color_palette() noexcept {
    static_assert(sizeof(a1compat::TeamColorRgb) == 3 * sizeof(float),
                  "native team-colour entries are packed RGB triples");
    if (!g_legacy_team_color_palette_active || !g_armada || !g_fleet_ops) {
        return false;
    }
    auto* native_palette = at<a1compat::TeamColorRgb>(
        g_fleet_ops, kFleetOpsTeamColorPaletteRva);
    if (!writable_range(
            native_palette,
            kNativeTeamColorEntryCount * sizeof(*native_palette))) {
        return false;
    }
    // Keep Armada's abandoned original array synchronized for any unpatched
    // fallback consumer, but do not make that mirror a prerequisite: Fleet
    // Ops' FOTeamColor array above is the authoritative live palette.
    auto* armada_palette = at<a1compat::TeamColorRgb>(
        g_armada, kArmadaTeamColorPaletteRva);
    const bool mirror_armada_palette = writable_range(
        armada_palette,
        kNativeTeamColorEntryCount * sizeof(*armada_palette));

    std::uint32_t applied = 0;
    for (std::size_t index = 0;
         index < a1compat::kPlayerTeamColorCount; ++index) {
        if (!g_legacy_team_color_palette.present[index]) continue;
        native_palette[kNativePlayerColorFirstIndex + index] =
            g_legacy_team_color_palette.colors[index];
        if (mirror_armada_palette) {
            armada_palette[kNativePlayerColorFirstIndex + index] =
                g_legacy_team_color_palette.colors[index];
        }
        ++applied;
    }
    const LONG application =
        InterlockedIncrement(&g_legacy_team_color_apply_count);
    if (application <= 8) {
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "Applied A1 team-colour translation #%ld to %lu live Fleet Ops "
            "IA/minimap palette slots (Armada mirror=%s)",
            static_cast<long>(application),
            static_cast<unsigned long>(applied),
            mirror_armada_palette ? "yes" : "no");
        log_line(message);
    }
    return applied != 0;
}

void __cdecl team_color_init_hook() noexcept {
    const auto original = reinterpret_cast<TeamColorInitFn>(
        g_team_color_init_hook.gateway);
    if (original) original();
    apply_legacy_team_color_palette();
}

bool install_legacy_team_color_translation(
    const A2FO_ModuleApi* api) noexcept {
    if (!load_legacy_team_color_palette()) {
        log_line("No legacy A1 player-colour names found; TeamColor_Init "
                 "translation not required");
        return true;
    }
    if (!api || !g_armada || !api->install_inline_hook ||
        !api->install_inline_hook(
            at(g_armada, kTeamColorInitRva),
            reinterpret_cast<void*>(&team_color_init_hook),
            kTeamColorInitHookLength, kExpectedTeamColorInit,
            &g_team_color_init_hook)) {
        return false;
    }
    // TeamColor_Init may have run once before deferred modules load. Updating
    // the static palette now fixes that instance; the hook reapplies it after
    // every later system/graphics reset.
    return apply_legacy_team_color_palette();
}

std::string strip_c_comments(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    bool line_comment = false;
    bool block_comment = false;
    for (std::size_t index = 0; index < input.size(); ++index) {
        const char current = input[index];
        const char next = index + 1 < input.size() ? input[index + 1] : '\0';
        if (line_comment) {
            if (current == '\n' || current == '\r') {
                line_comment = false;
                output.push_back(current);
            } else {
                output.push_back(' ');
            }
            continue;
        }
        if (block_comment) {
            if (current == '*' && next == '/') {
                output.append("  ");
                ++index;
                block_comment = false;
            } else {
                output.push_back(
                    current == '\n' || current == '\r' ? current : ' ');
            }
            continue;
        }
        if (current == '/' && next == '/') {
            output.append("  ");
            ++index;
            line_comment = true;
        } else if (current == '/' && next == '*') {
            output.append("  ");
            ++index;
            block_comment = true;
        } else {
            output.push_back(current);
        }
    }
    return output;
}

bool config_identifier_character(char value) noexcept {
    return (value >= 'a' && value <= 'z') ||
        (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9') || value == '_';
}

std::string assignment_identifier(
    const std::string& statement, std::size_t equals) {
    std::size_t end = equals;
    while (end != 0 && !config_identifier_character(statement[end - 1])) {
        --end;
    }
    std::size_t begin = end;
    while (begin != 0 &&
           config_identifier_character(statement[begin - 1])) {
        --begin;
    }
    return statement.substr(begin, end - begin);
}

struct LegacyGameplayConfigAreas {
    a1compat::LegacyUiArea control_panel{};
    a1compat::LegacyUiArea cinematic_panel{};
    a1compat::LegacyUiArea cinematic_background_panel{};
    a1compat::LegacyUiArea cinematic_background{};
    a1compat::LegacyUiArea cinematic_display{};
    a1compat::LegacyUiArea cinematic_menu_button{};
    a1compat::LegacyUiArea cinematic_comm_button{};
    a1compat::LegacyUiArea single_wireframe{};
    a1compat::LegacyUiArea single_race_icon{};
    a1compat::LegacyUiArea single_race_icon_display{};
    a1compat::LegacyUiArea single_background{};
    std::array<a1compat::LegacyUiArea,
               kMaximumLegacyShipDisplayBackgroundPieces>
        single_background_pieces{};
    std::size_t single_background_piece_count = 0;
    a1compat::LegacyUiArea single_name_background{};
    a1compat::LegacyUiArea single_crew_background{};
    a1compat::LegacyUiArea single_officer_background{};
    a1compat::LegacyUiArea single_class_text{};
    a1compat::LegacyUiArea single_name_text{};
    a1compat::LegacyUiArea single_crew_text{};
    a1compat::LegacyUiArea single_officer_text{};
    a1compat::LegacyUiArea single_crew_label{};
    a1compat::LegacyUiArea single_officer_label{};
    a1compat::LegacyUiArea single_energy_bar{};
    a1compat::LegacyUiArea single_construction_bar{};
    std::array<a1compat::LegacyUiArea, kLegacyMultiShipCount>
        multi_ship_tiles{};
    a1compat::LegacyUiArea multi_shield_bar{};
    a1compat::LegacyUiArea multi_energy_bar{};
    a1compat::LegacyUiArea multi_crew_dot{};
    a1compat::LegacyUiArea multi_wireframe{};
    bool has_single_wireframe = false;
    bool has_single_race_icon = false;
    bool has_single_background = false;
    bool has_single_name_background = false;
    bool has_single_crew_background = false;
    bool has_single_officer_background = false;
    bool has_single_identity_layout = false;
    bool has_single_identity_labels = false;
    bool has_single_energy_bar = false;
    bool has_single_construction_bar = false;
    bool has_multi_ship_layout = false;
    bool has_cinematic_buttons = false;
    std::string source_path;
};

bool parse_gui_rectangle_assignment(
    const std::string& contents, const char* key,
    a1compat::LegacyUiArea* output) noexcept {
    if (!key || !*key || !output) return false;
    try {
        const std::string source = strip_c_comments(contents);
        std::istringstream lines(source);
        std::string line;
        bool found = false;
        while (std::getline(lines, line)) {
            const std::size_t equals = line.find('=');
            if (equals == std::string::npos ||
                assignment_identifier(line, equals) != key) {
                continue;
            }
            const char* cursor = line.c_str() + equals + 1;
            std::array<std::int32_t, 4> values{};
            bool valid = true;
            for (std::size_t index = 0; index < values.size(); ++index) {
                while (*cursor == ' ' || *cursor == '\t' ||
                       *cursor == '\r' || *cursor == '\n') {
                    ++cursor;
                }
                errno = 0;
                char* end = nullptr;
                const long parsed = std::strtol(cursor, &end, 10);
                if (end == cursor || errno == ERANGE ||
                    parsed < std::numeric_limits<std::int32_t>::min() ||
                    parsed > std::numeric_limits<std::int32_t>::max()) {
                    valid = false;
                    break;
                }
                values[index] = static_cast<std::int32_t>(parsed);
                cursor = end;
            }
            if (!valid) continue;
            *output = a1compat::LegacyUiArea{
                values[0], values[1], values[2], values[3]};
            found = true;
        }
        return found;
    } catch (...) {
        return false;
    }
}

bool load_active_legacy_gameplay_config(
    const char* configuration_filename,
    LegacyGameplayConfigAreas* output) noexcept {
    if (!configuration_filename || !*configuration_filename || !output ||
        !g_api || !g_api->extension_root_count || !g_api->extension_root) {
        return false;
    }
    try {
        std::string supplied(configuration_filename);
        if (supplied.empty() || supplied.size() > 1024) return false;
        const std::size_t separator = supplied.find_last_of("\\/");
        const std::string basename = separator == std::string::npos
            ? supplied : supplied.substr(separator + 1);
        const std::uint32_t root_count = g_api->extension_root_count();
        if (root_count == 0 || root_count > 4096) return false;

        for (std::uint32_t position = root_count; position != 0; --position) {
            const char* root = g_api->extension_root(position - 1);
            if (!root || !*root) continue;
            const std::array<std::string, 3> candidates{{
                join_path(root, supplied.c_str()),
                join_path(
                    root, (std::string("misc\\") + basename).c_str()),
                join_path(root, basename.c_str())}};
            for (const std::string& path : candidates) {
                if (path.empty()) continue;
                const std::string contents = read_small_text_file(path);
                if (contents.empty()) continue;

                LegacyGameplayConfigAreas parsed{};
                const bool complete = parse_gui_rectangle_assignment(
                        contents, "controlPanelArea",
                        &parsed.control_panel) &&
                    parse_gui_rectangle_assignment(
                        contents, "cinematicPanelArea",
                        &parsed.cinematic_panel) &&
                    parse_gui_rectangle_assignment(
                        contents, "cinematicBackgroundPanelArea",
                        &parsed.cinematic_background_panel) &&
                    parse_gui_rectangle_assignment(
                        contents, "cinematicBackgroundArea",
                        &parsed.cinematic_background) &&
                    parse_gui_rectangle_assignment(
                        contents, "cinematicDisplayArea",
                        &parsed.cinematic_display);
                if (!complete) return false;
                parsed.has_cinematic_buttons =
                    parse_gui_rectangle_assignment(
                        contents, "cinematicMenuButtonArea",
                        &parsed.cinematic_menu_button) &&
                    parse_gui_rectangle_assignment(
                        contents, "cinematicCommButtonArea",
                        &parsed.cinematic_comm_button);
                parsed.has_single_wireframe =
                    parse_gui_rectangle_assignment(
                        contents, "infoSingleWireframeIconArea",
                        &parsed.single_wireframe);
                parsed.has_single_race_icon =
                    parse_gui_rectangle_assignment(
                        contents, "infoSingleRaceIconArea",
                        &parsed.single_race_icon) &&
                    parse_gui_rectangle_assignment(
                        contents, "infoSingleRaceIconDisplayArea",
                        &parsed.single_race_icon_display);
                parsed.has_single_background =
                    parse_gui_rectangle_assignment(
                        contents, "infoSingleBackgroundArea",
                        &parsed.single_background);
                for (std::size_t index = 0;
                     parsed.has_single_background &&
                     index < parsed.single_background_pieces.size();
                     ++index) {
                    char key[64]{};
                    std::snprintf(
                        key, sizeof(key), "infoSingleBackground_%lu",
                        static_cast<unsigned long>(index));
                    if (!parse_gui_rectangle_assignment(
                            contents, key,
                            &parsed.single_background_pieces[index])) {
                        break;
                    }
                    ++parsed.single_background_piece_count;
                }
                parsed.has_single_name_background =
                    parse_gui_rectangle_assignment(
                        contents, "infoSingleNameTextBackgroundArea",
                        &parsed.single_name_background);
                parsed.has_single_crew_background =
                    parse_gui_rectangle_assignment(
                        contents, "infoSingleCrewTextBackgroundArea",
                        &parsed.single_crew_background);
                parsed.has_single_officer_background =
                    parse_gui_rectangle_assignment(
                        contents, "infoSingleOfficerTextBackgroundArea",
                        &parsed.single_officer_background);
                bool has_class_text = parse_gui_rectangle_assignment(
                    contents, "infoSingleClassTextInformationArea",
                    &parsed.single_class_text);
                if (!has_class_text) {
                    has_class_text = parse_gui_rectangle_assignment(
                        contents, "infoSingleClassTextArea",
                        &parsed.single_class_text);
                }
                bool has_name_text = parse_gui_rectangle_assignment(
                    contents, "infoSingleNameTextInformationArea",
                    &parsed.single_name_text);
                if (!has_name_text) {
                    has_name_text = parse_gui_rectangle_assignment(
                        contents, "infoSingleNameTextArea",
                        &parsed.single_name_text);
                }
                const bool has_crew_text = parse_gui_rectangle_assignment(
                    contents, "infoSingleCrewTextInformationArea",
                    &parsed.single_crew_text);
                const bool has_officer_text =
                    parse_gui_rectangle_assignment(
                        contents, "infoSingleOfficerTextInformationArea",
                        &parsed.single_officer_text);
                parsed.has_single_identity_layout = has_class_text &&
                    has_name_text && has_crew_text && has_officer_text;
                parsed.has_single_identity_labels =
                    parse_gui_rectangle_assignment(
                        contents, "infoSingleCrewTextLabelArea",
                        &parsed.single_crew_label) &&
                    parse_gui_rectangle_assignment(
                        contents, "infoSingleOfficerTextLabelArea",
                        &parsed.single_officer_label);
                parsed.has_single_construction_bar =
                    parse_gui_rectangle_assignment(
                        contents, "infoSingleConstructionBarArea",
                        &parsed.single_construction_bar);
                parsed.has_single_energy_bar =
                    parse_gui_rectangle_assignment(
                        contents, "infoSingleEnergyBarArea",
                        &parsed.single_energy_bar);
                parsed.has_multi_ship_layout = true;
                for (std::size_t index = 0;
                     index < parsed.multi_ship_tiles.size(); ++index) {
                    char key[64]{};
                    std::snprintf(
                        key, sizeof(key), "infoMultiShipIcon_%lu",
                        static_cast<unsigned long>(index));
                    if (!parse_gui_rectangle_assignment(
                            contents, key,
                            &parsed.multi_ship_tiles[index])) {
                        parsed.has_multi_ship_layout = false;
                        break;
                    }
                }
                parsed.has_multi_ship_layout =
                    parsed.has_multi_ship_layout &&
                    parse_gui_rectangle_assignment(
                        contents, "infoMultiShieldBarArea",
                        &parsed.multi_shield_bar) &&
                    parse_gui_rectangle_assignment(
                        contents, "infoMultiEnergyBarArea",
                        &parsed.multi_energy_bar) &&
                    parse_gui_rectangle_assignment(
                        contents, "infoMultiCrewDotArea",
                        &parsed.multi_crew_dot) &&
                    parse_gui_rectangle_assignment(
                        contents, "infoMultiWireframeIconArea",
                        &parsed.multi_wireframe);
                parsed.source_path = path;
                *output = std::move(parsed);
                return true;
            }
        }
    } catch (...) {
        return false;
    }
    return false;
}

a1compat::LegacyUiArea scale_legacy_gui_area(
    const a1compat::LegacyUiArea& area,
    double scale_x, double scale_y) noexcept {
    return a1compat::LegacyUiArea{
        static_cast<std::int32_t>(std::lround(area.x * scale_x)),
        static_cast<std::int32_t>(std::lround(area.y * scale_y)),
        static_cast<std::int32_t>(std::lround(area.width * scale_x)),
        static_cast<std::int32_t>(std::lround(area.height * scale_y))};
}

bool parse_starting_resource_literal(
    const std::string& statement, std::size_t equals,
    std::int32_t& value) noexcept {
    const char* begin = statement.c_str() + equals + 1;
    while (*begin == ' ' || *begin == '\t' ||
           *begin == '\r' || *begin == '\n') {
        ++begin;
    }
    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(begin, &end);
    if (end == begin || errno == ERANGE || !std::isfinite(parsed)) {
        return false;
    }
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') {
        ++end;
    }
    if (*end == 'f' || *end == 'F') {
        ++end;
        while (*end == ' ' || *end == '\t' ||
               *end == '\r' || *end == '\n') {
            ++end;
        }
    }
    if (*end != '\0' || parsed < 0.0 ||
        parsed > kMaximumStartingResourceAmount) {
        return false;
    }
    const long long rounded = std::llround(parsed);
    if (rounded < 0 ||
        rounded > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    value = static_cast<std::int32_t>(rounded);
    return true;
}

struct LegacyNativeResourceLimit {
    const char* legacy_command;
    const char* modern_command;
    std::uintptr_t maximum_rva;
    std::uintptr_t mutator_rva;
    std::size_t maximum_operand_offset;
};

constexpr std::array<LegacyNativeResourceLimit, 3>
    kLegacyNativeResourceLimits{{
        {"cfgMaxDilithium", "MAX_DILITHIUM", kMaximumDilithiumRva,
         kTeamAddDilithiumRva, 0x32},
        {"cfgMaxCrew", "MAX_CREW", kMaximumCrewRva,
         kTeamAddCrewRva, 0x32},
        {"cfgMaxOfficers", "MAX_OFFICERS", kMaximumOfficersRva,
         kTeamAddOfficersRva, 0x35},
    }};

bool native_resource_limit_layout_matches(
    const LegacyNativeResourceLimit& limit) noexcept {
    const auto* mutator = static_cast<const std::uint8_t*>(
        at(g_armada, limit.mutator_rva));
    if (!mutator || !readable_range(
            mutator, sizeof(kExpectedTeamResourceMutator)) ||
        std::memcmp(mutator, kExpectedTeamResourceMutator,
                    sizeof(kExpectedTeamResourceMutator)) != 0 ||
        !readable_range(
            mutator + limit.maximum_operand_offset - 1,
            1 + sizeof(std::uint32_t)) ||
        mutator[limit.maximum_operand_offset - 1] != 0xa1) {
        return false;
    }
    std::uint32_t operand = 0;
    std::memcpy(&operand, mutator + limit.maximum_operand_offset,
                sizeof(operand));
    return operand == static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(
            at(g_armada, limit.maximum_rva)));
}

bool apply_legacy_native_resource_limits() {
    if (!g_api || !g_api->extension_root_count ||
        !g_api->extension_root || !g_armada) {
        return false;
    }

    std::string effective_path;
    std::string effective_contents;
    const std::uint32_t count = g_api->extension_root_count();
    if (count > 4096) return false;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::string path = join_path(
            g_api->extension_root(index), kRtsConfigFileName);
        if (path.empty()) continue;
        std::ifstream candidate(path, std::ios::binary);
        if (!candidate) continue;
        effective_path = path;
        effective_contents = read_small_text_file(path);
    }
    if (effective_path.empty()) return true;

    std::array<std::int32_t, kLegacyNativeResourceLimits.size()> values{};
    std::array<bool, kLegacyNativeResourceLimits.size()> legacy_found{};
    std::array<bool, kLegacyNativeResourceLimits.size()> modern_found{};
    const std::string source = strip_c_comments(effective_contents);
    std::size_t begin = 0;
    while (begin < source.size()) {
        const std::size_t semicolon = source.find(';', begin);
        const std::size_t end = semicolon == std::string::npos
            ? source.size() : semicolon;
        const std::string statement = source.substr(begin, end - begin);
        const std::size_t equals = statement.find('=');
        if (equals != std::string::npos) {
            const std::string identifier =
                assignment_identifier(statement, equals);
            for (std::size_t index = 0;
                 index < kLegacyNativeResourceLimits.size(); ++index) {
                const auto& limit = kLegacyNativeResourceLimits[index];
                if (identifier == limit.modern_command) {
                    modern_found[index] = true;
                    break;
                }
                if (identifier != limit.legacy_command) continue;
                std::int32_t value = 0;
                if (parse_starting_resource_literal(
                        statement, equals, value)) {
                    values[index] = value;
                    legacy_found[index] = true;
                }
                break;
            }
        }
        if (semicolon == std::string::npos) break;
        begin = semicolon + 1;
    }

    std::size_t applied = 0;
    for (std::size_t index = 0;
         index < kLegacyNativeResourceLimits.size(); ++index) {
        if (!legacy_found[index] || modern_found[index]) continue;
        const auto& limit = kLegacyNativeResourceLimits[index];
        void* maximum = at(g_armada, limit.maximum_rva);
        if (!native_resource_limit_layout_matches(limit) ||
            !writable_range(maximum, sizeof(float))) {
            log_line("A1 legacy resource-maximum layout validation failed");
            return false;
        }
        const float value = static_cast<float>(values[index]);
        std::memcpy(maximum, &value, sizeof(value));
        ++applied;
    }

    if (applied != 0) {
        char message[384]{};
        std::snprintf(
            message, sizeof(message),
            "A1 legacy resource maxima bridged from '%s': "
            "Dilithium=%s%ld, Crew=%s%ld, Officers=%s%ld",
            effective_path.c_str(),
            legacy_found[0] && !modern_found[0] ? "" : "unchanged/",
            static_cast<long>(values[0]),
            legacy_found[1] && !modern_found[1] ? "" : "unchanged/",
            static_cast<long>(values[1]),
            legacy_found[2] && !modern_found[2] ? "" : "unchanged/",
            static_cast<long>(values[2]));
        log_line(message);
    }
    return true;
}

std::uint32_t apply_starting_resource_config(
    const std::string& contents,
    LegacyStartingResourceDefaults& defaults) {
    struct GrantCommand {
        const char* name;
        std::size_t resource_index;
    };
    constexpr std::array<GrantCommand, 5> commands{{
        {"SHOWMETHEMONEY_CREW", 0},
        {"SHOWMETHEMONEY_DILITHIUM", 1},
        {"SHOWMETHEMONEY_METAL", 2},
        {"SHOWMETHEMONEY_TRITANIUM", 3},
        {"SHOWMETHEMONEY_SUPPLIES", 4},
    }};

    const std::string source = strip_c_comments(contents);
    std::uint32_t applied = 0;
    std::size_t begin = 0;
    while (begin < source.size()) {
        const std::size_t semicolon = source.find(';', begin);
        const std::size_t end = semicolon == std::string::npos
            ? source.size() : semicolon;
        const std::string statement = source.substr(begin, end - begin);
        const std::size_t equals = statement.find('=');
        if (equals != std::string::npos) {
            const std::string identifier =
                assignment_identifier(statement, equals);
            for (const GrantCommand& command : commands) {
                if (identifier != command.name) continue;
                std::int32_t value = 0;
                if (parse_starting_resource_literal(
                        statement, equals, value)) {
                    defaults.normal[command.resource_index] = value;
                    ++applied;
                }
                break;
            }
        }
        if (semicolon == std::string::npos) break;
        begin = semicolon + 1;
    }
    return applied;
}

LegacyStartingResourceDefaults load_starting_resource_defaults() {
    LegacyStartingResourceDefaults defaults;
    std::uint32_t configured_fields = 0;
    if (g_api && g_api->extension_root_count && g_api->extension_root) {
        const std::uint32_t count = g_api->extension_root_count();
        if (count <= 4096) {
            for (std::uint32_t index = 0; index < count; ++index) {
                const std::string path = join_path(
                    g_api->extension_root(index), kRtsConfigFileName);
                if (path.empty()) continue;
                const std::string contents = read_small_text_file(path);
                if (contents.empty()) continue;
                configured_fields +=
                    apply_starting_resource_config(contents, defaults);
            }
        }
    }
    for (std::size_t index = 0; index < defaults.normal.size(); ++index) {
        const long long lots = std::llround(
            static_cast<double>(defaults.normal[index]) * 1.5);
        defaults.lots[index] = static_cast<std::int32_t>(lots);
    }

    char message[384]{};
    std::snprintf(
        message, sizeof(message),
        "A1 Race starting-resource defaults: normal Crew=%ld, "
        "Dilithium=%ld, Metal=%ld, Tritanium=%ld, Supply=%ld; "
        "lots=1.5x [%ld, %ld, %ld, %ld, %ld] (%s)",
        static_cast<long>(defaults.normal[0]),
        static_cast<long>(defaults.normal[1]),
        static_cast<long>(defaults.normal[2]),
        static_cast<long>(defaults.normal[3]),
        static_cast<long>(defaults.normal[4]),
        static_cast<long>(defaults.lots[0]),
        static_cast<long>(defaults.lots[1]),
        static_cast<long>(defaults.lots[2]),
        static_cast<long>(defaults.lots[3]),
        static_cast<long>(defaults.lots[4]),
        configured_fields == 0 ? "built-in showmethemoney defaults" :
                                 "RTS_CFG.h");
    log_line(message);
    return defaults;
}

bool register_starting_resource_defaults(
    const A2FO_ModuleApi* api,
    const LegacyStartingResourceDefaults& defaults) {
    if (!api || !api->register_race_odf_defaults) return false;
    std::array<std::string, 10> values;
    std::array<A2FO_RaceOdfDefault, 10> registrations{};
    for (std::size_t index = 0; index < 5; ++index) {
        values[index] = std::to_string(defaults.normal[index]);
        values[index + 5] = std::to_string(defaults.lots[index]);
        registrations[index] = A2FO_RaceOdfDefault{
            kNormalStartingResourceCommands[index], values[index].c_str()};
        registrations[index + 5] = A2FO_RaceOdfDefault{
            kLotsStartingResourceCommands[index],
            values[index + 5].c_str()};
    }
    return api->register_race_odf_defaults(
        kModuleName, registrations.data(),
        static_cast<std::uint32_t>(registrations.size()));
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
        normalize_command_token(
            output->data(), output->data(), output->size());
        return (*output)[0] != '\0';
    }
    output->front() = '\0';
    return false;
}

bool matches_numbered_odf_family(
    const char* odf_name, const char* family) noexcept {
    if (!odf_name || !*odf_name || !family || !*family) return false;

    const char* basename = odf_name;
    for (const char* cursor = odf_name; *cursor != '\0'; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') basename = cursor + 1;
    }

    char normalized[128]{};
    std::size_t length = 0;
    while (basename[length] != '\0' && length + 1 < sizeof(normalized)) {
        char value = basename[length];
        if (value >= 'A' && value <= 'Z') {
            value = static_cast<char>(value - 'A' + 'a');
        }
        normalized[length++] = value;
    }
    normalized[length] = '\0';
    if (length > 4 && _stricmp(normalized + length - 4, ".odf") == 0) {
        length -= 4;
        normalized[length] = '\0';
    }

    const std::size_t family_length = std::strlen(family);
    if (length < family_length ||
        _strnicmp(normalized, family, family_length) != 0) {
        return false;
    }
    for (std::size_t index = family_length; index < length; ++index) {
        if (normalized[index] < '0' || normalized[index] > '9') {
            return false;
        }
    }
    return true;
}

const char* legacy_moon_resource_default(
    void* parameter_db, char* odf_name,
    std::size_t odf_name_size) noexcept {
    if (!parameter_db || !odf_name || odf_name_size == 0 || !g_armada ||
        !is_valid_parameter_db(parameter_db)) {
        return nullptr;
    }
    odf_name[0] = '\0';

    void* project_id = read_pointer_at(
        parameter_db, kParameterDbProjectIdOffset);
    void* getter = g_project_id_get_odf_name;
    if (!project_id || !getter || !is_executable_pointer(getter)) {
        return nullptr;
    }

    const std::uintptr_t result = a2fo_a1_call_thiscall_0(
        getter, project_id);
    copy_printable_string(
        reinterpret_cast<const char*>(result), odf_name, odf_name_size);
    if (matches_numbered_odf_family(odf_name, "mmooninf")) {
        return "ResourceMoonInf";
    }
    if (matches_numbered_odf_family(odf_name, "mdmoon")) {
        return "ResourceMoon";
    }
    return nullptr;
}

std::uintptr_t resolve_game_object_resource(
    void* parameter_db, const char* key, char* output,
    std::uintptr_t output_size, const char* default_value) noexcept {
    if (!g_armada) return 0;

    const std::uintptr_t found = a2fo_a1_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(output), output_size,
        reinterpret_cast<std::uintptr_t>(default_value));
    if ((found & 0xffu) != 0 || !key || _stricmp(key, "resource") != 0) {
        return found;
    }

    char odf_name[160]{};
    const char* resource = legacy_moon_resource_default(
        parameter_db, odf_name, sizeof(odf_name));
    if (!resource) return found;

    const std::size_t resource_size = std::strlen(resource) + 1;
    if (output_size < resource_size ||
        !writable_range(output, resource_size)) {
        return found;
    }
    std::memcpy(output, resource, resource_size);

    const LONG incident = InterlockedIncrement(
        &g_legacy_moon_resource_default_count);
    if (incident <= kMaximumLegacyMoonResourceReports) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A2 Classic missing-code default #%ld: odf='%s', resource='%s'",
            static_cast<long>(incident), odf_name, resource);
        log_line(message);
    } else if (incident == kMaximumLegacyMoonResourceReports + 1) {
        log_line("Further A2 Classic moon resource defaults suppressed");
    }
    return 1;
}

using ParameterDbGetFloatFn = std::uintptr_t (__cdecl*)(
    void* parameter_db, std::uintptr_t context, const char* key,
    float* output, float default_value);

std::uintptr_t resolve_physics_speed(
    void* parameter_db, std::uintptr_t context, const char* key,
    float* output, float default_value) noexcept {
    if (!g_armada) return 0;

    const auto get_float = reinterpret_cast<ParameterDbGetFloatFn>(
        at(g_armada, kParameterDbGetFloatRva));
    const std::uintptr_t found = get_float(
        parameter_db, context, key, output, default_value);
    if (!key) return found;

    const bool combat_lookup = _stricmp(key, "combatSpeed") == 0;
    const bool impulse_lookup = _stricmp(key, "impulseSpeed") == 0;
    const bool warp_lookup = _stricmp(key, "warpSpeed") == 0;
    if (!combat_lookup && !impulse_lookup && !warp_lookup) return found;

    // A declared/inherited combatSpeed identifies an A2/FO physics contract.
    // Preserve every native value in that case, including an explicit zero.
    if (!combat_lookup) {
        float combat_speed = 0.0f;
        const std::uintptr_t combat_found = get_float(
            parameter_db, context, "combatSpeed", &combat_speed, 0.0f);
        if ((combat_found & 0xffu) != 0) return found;
    } else if ((found & 0xffu) != 0) {
        return found;
    }

    float legacy_impulse_speed = 0.0f;
    float legacy_warp_speed = 0.0f;
    const std::uintptr_t impulse_found = get_float(
        parameter_db, context, "impulseSpeed", &legacy_impulse_speed, 0.0f);
    const std::uintptr_t warp_found = get_float(
        parameter_db, context, "warpSpeed", &legacy_warp_speed, 0.0f);
    const bool has_impulse = (impulse_found & 0xffu) != 0 &&
        std::isfinite(legacy_impulse_speed) && legacy_impulse_speed > 0.0f;
    const bool has_warp = (warp_found & 0xffu) != 0 &&
        std::isfinite(legacy_warp_speed) && legacy_warp_speed > 0.0f;
    if (!writable_range(output, sizeof(*output))) return found;

    float translated_speed = 0.0f;
    volatile LONG* report_counter = nullptr;
    const char* report_kind = nullptr;
    const char* report_source = nullptr;
    if (combat_lookup) {
        if (!has_impulse && !has_warp) return found;
        translated_speed = has_impulse ? legacy_impulse_speed : 0.0f;
        if (has_warp) {
            translated_speed = (std::max)(
                translated_speed, legacy_warp_speed * 0.5f);
        }
        report_counter = &g_legacy_physics_combat_speed_default_count;
        report_kind = "combat-speed default";
        report_source = "legacy impulse/warp speeds";
    } else if (impulse_lookup) {
        if (!has_warp) return found;
        // A1 warpSpeed is its ordinary aligned/cruise speed. A2 renamed that
        // tier to impulseSpeed and reserved warpSpeed for strategic warp.
        translated_speed = legacy_warp_speed;
        report_counter = &g_legacy_physics_impulse_speed_translation_count;
        report_kind = "cruise-speed translation";
        report_source = "legacy warpSpeed";
    } else {
        translated_speed = 0.0f;
        report_counter = &g_legacy_physics_warp_speed_translation_count;
        report_kind = "warp-speed translation";
        report_source = "legacy strategic-warp disable";
    }
    std::memcpy(output, &translated_speed, sizeof(translated_speed));

    const LONG incident = InterlockedIncrement(report_counter);
    if (incident <= kMaximumLegacyPhysicsDefaultReports) {
        char message[288]{};
        std::snprintf(
            message, sizeof(message),
            "A1 physics %s #%ld: %s=%.3f from %s",
            report_kind, static_cast<long>(incident), key,
            static_cast<double>(translated_speed), report_source);
        log_line(message);
    } else if (incident == kMaximumLegacyPhysicsDefaultReports + 1) {
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "Further A1 physics %s reports suppressed", report_kind);
        log_line(message);
    }
    return 1;
}

enum class LegacySmoothProfile {
    destroyer,
    construction,
    battle,
};

struct SmoothFloatDefault {
    const char* key;
    float destroyer;
    float construction;
    float battle;
    float destroyer_combat;
    float battle_combat;
};

// These are the controller values used by STA2 Classic's Federation-facing
// sdestphys, sconstphys and sbattphys families. The raw A1 physics file names
// are shared by races, so the ordinary fallback deliberately follows these
// A2 Federation movement profiles rather than attempting a per-ship race
// guess which PhysicsClass cannot make.
constexpr std::array<SmoothFloatDefault, 15> kLegacySmoothDefaults{{
    {"forwardAccel", 1.0f, 1.0f, 3.0f, 0.5f, 0.5f},
    {"backwardAccel", 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    {"turnOmega", 1.0f, 0.8f, 1.0f, 0.1f, 0.1f},
    {"turnAlpha", 1.0f, 0.8f, 1.0f, 0.5f, 1.0f},
    {"pitchOmega", 1.0f, 1.0f, 1.0f, 0.1f, 0.1f},
    {"pitchAlpha", 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    {"rollCoupling", 2.0f, 0.5f, 3.0f, 2.0f, 3.0f},
    {"turnOmegaFractionAtRest", 0.2f, 0.8f, 0.0f, 0.2f, 0.0f},
    {"turnAlphaFractionAtRest", 0.2f, 0.8f, 0.0f, 0.2f, 0.0f},
    {"pitchOmegaFractionAtRest", 0.2f, 0.8f, 0.0f, 0.2f, 0.0f},
    {"pitchAlphaFractionAtRest", 0.2f, 0.8f, 0.0f, 0.2f, 0.0f},
    {"pitchDefault", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {"pitchDefaultSpeed", -1.0f, 0.5f, 1.0f, -1.0f, 1.0f},
    {"turnControlAngle", 20.0f, 5.0f, 30.0f, 20.0f, 30.0f},
    {"forwardControlDistance", 70.0f, 10.0f, 100.0f, 70.0f, 100.0f},
}};

bool parameter_odf_name(
    void* parameter_db, char* output, std::size_t output_size) noexcept {
    if (!parameter_db || !output || output_size == 0 ||
        !is_valid_parameter_db(parameter_db)) {
        return false;
    }
    output[0] = '\0';
    void* project_id = read_pointer_at(
        parameter_db, kParameterDbProjectIdOffset);
    void* getter = g_project_id_get_odf_name;
    if (!project_id || !getter || !is_executable_pointer(getter)) {
        return false;
    }
    const std::uintptr_t result = a2fo_a1_call_thiscall_0(
        getter, project_id);
    copy_printable_string(
        reinterpret_cast<const char*>(result), output, output_size);
    return output[0] != '\0';
}

bool legacy_physics_database(void* parameter_db) noexcept {
    if (!parameter_db || !g_armada) return false;
    const auto get_float = reinterpret_cast<ParameterDbGetFloatFn>(
        at(g_armada, kParameterDbGetFloatRva));
    float value = 0.0f;
    if ((get_float(
            parameter_db, 0, "combatSpeed", &value, 0.0f) & 0xffu) != 0) {
        return false;
    }
    const bool has_impulse = (get_float(
        parameter_db, 0, "impulseSpeed", &value, 0.0f) & 0xffu) != 0;
    const bool has_warp = (get_float(
        parameter_db, 0, "warpSpeed", &value, 0.0f) & 0xffu) != 0;
    return has_impulse || has_warp;
}

LegacySmoothProfile legacy_smooth_profile(
    void* parameter_db, char* odf_name,
    std::size_t odf_name_size) noexcept {
    if (odf_name && odf_name_size != 0) {
        odf_name[0] = '\0';
        parameter_odf_name(parameter_db, odf_name, odf_name_size);
    }
    const char* name = odf_name && odf_name[0] != '\0' ? odf_name : "";
    const char* basename = name;
    for (const char* cursor = name; *cursor != '\0'; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') basename = cursor + 1;
    }
    if (_strnicmp(basename, "cnstphys", 8) == 0 ||
        _strnicmp(basename, "constphys", 9) == 0) {
        return LegacySmoothProfile::construction;
    }
    if (_strnicmp(basename, "battphys", 8) == 0) {
        return LegacySmoothProfile::battle;
    }
    return LegacySmoothProfile::destroyer;
}

const char* smooth_profile_name(LegacySmoothProfile profile) noexcept {
    switch (profile) {
        case LegacySmoothProfile::construction: return "construction";
        case LegacySmoothProfile::battle: return "battle";
        default: return "federation-destroyer";
    }
}

float smooth_default_value(
    const SmoothFloatDefault& value, LegacySmoothProfile profile,
    bool combat_controller) noexcept {
    switch (profile) {
        case LegacySmoothProfile::construction:
            return value.construction;
        case LegacySmoothProfile::battle:
            return combat_controller ? value.battle_combat : value.battle;
        default:
            return combat_controller ?
                value.destroyer_combat : value.destroyer;
    }
}

void translate_smooth_float(
    void* primary_db, void* fallback_db, const char* key,
    float* output) noexcept {
    if (!g_armada) return;
    using PhysicsFloatCascadeFn = void (__cdecl*)(
        void*, void*, const char*, float*);
    const auto native = reinterpret_cast<PhysicsFloatCascadeFn>(
        at(g_armada, kPhysicsFloatCascadeRva));
    native(primary_db, fallback_db, key, output);
    if (!key || !writable_range(output, sizeof(*output))) return;

    void* legacy_db = legacy_physics_database(primary_db) ? primary_db :
        (legacy_physics_database(fallback_db) ? fallback_db : nullptr);
    if (!legacy_db) return;

    // A partial modern override remains authoritative over the compatibility
    // profile, whether it appears in the primary combat DB or its fallback.
    const auto get_float = reinterpret_cast<ParameterDbGetFloatFn>(
        at(g_armada, kParameterDbGetFloatRva));
    float declared_value = 0.0f;
    if ((primary_db && (get_float(
            primary_db, 0, key, &declared_value, 0.0f) & 0xffu) != 0) ||
        (fallback_db && (get_float(
            fallback_db, 0, key, &declared_value, 0.0f) & 0xffu) != 0)) {
        return;
    }

    const auto definition = std::find_if(
        kLegacySmoothDefaults.begin(), kLegacySmoothDefaults.end(),
        [key](const SmoothFloatDefault& candidate) {
            return _stricmp(candidate.key, key) == 0;
        });
    if (definition == kLegacySmoothDefaults.end()) return;

    char odf_name[160]{};
    const LegacySmoothProfile profile = legacy_smooth_profile(
        legacy_db, odf_name, sizeof(odf_name));
    const bool combat_controller = fallback_db == legacy_db;
    const float translated = smooth_default_value(
        *definition, profile, combat_controller);
    std::memcpy(output, &translated, sizeof(translated));

    if (_stricmp(key, "forwardAccel") == 0) {
        const LONG incident = InterlockedIncrement(
            &g_legacy_smooth_profile_translation_count);
        if (incident <= kMaximumLegacyPhysicsDefaultReports) {
            char message[320]{};
            std::snprintf(
                message, sizeof(message),
                "A1 smooth movement profile #%ld: odf='%s', profile='%s', "
                "controller='%s'",
                static_cast<long>(incident),
                odf_name[0] != '\0' ? odf_name : "unknown",
                smooth_profile_name(profile),
                combat_controller ? "combat" : "normal");
            log_line(message);
        } else if (incident == kMaximumLegacyPhysicsDefaultReports + 1) {
            log_line("Further A1 smooth movement profile reports suppressed");
        }
    }
}

void translate_smooth_integer(
    void* primary_db, void* fallback_db, const char* key,
    std::int32_t* output) noexcept {
    if (!g_armada) return;
    using PhysicsIntCascadeFn = void (__cdecl*)(
        void*, void*, const char*, std::int32_t*);
    const auto native = reinterpret_cast<PhysicsIntCascadeFn>(
        at(g_armada, kPhysicsIntCascadeRva));
    native(primary_db, fallback_db, key, output);
    if (!key || _stricmp(key, "turnControlSquared") != 0 ||
        !writable_range(output, sizeof(*output))) {
        return;
    }
    void* legacy_db = legacy_physics_database(primary_db) ? primary_db :
        (legacy_physics_database(fallback_db) ? fallback_db : nullptr);
    if (!legacy_db) return;

    std::int32_t declared_value = 0;
    const bool primary_declared = primary_db &&
        (a2fo_a1_call_thiscall_3(
            at(g_armada, kParameterDbGetIntRva), primary_db,
            reinterpret_cast<std::uintptr_t>(key),
            reinterpret_cast<std::uintptr_t>(&declared_value), 0) & 0xffu) != 0;
    const bool fallback_declared = fallback_db &&
        (a2fo_a1_call_thiscall_3(
            at(g_armada, kParameterDbGetIntRva), fallback_db,
            reinterpret_cast<std::uintptr_t>(key),
            reinterpret_cast<std::uintptr_t>(&declared_value), 0) & 0xffu) != 0;
    if (primary_declared || fallback_declared) return;
    const std::int32_t translated = 0;
    std::memcpy(output, &translated, sizeof(translated));
}

std::uintptr_t resolve_physics_model(
    void* parameter_db, const char* key, char* output,
    std::uintptr_t output_size, const char* default_value) noexcept {
    if (!g_armada) return 0;

    const std::uintptr_t found = native_race_entry_lookup(
        parameter_db, key, output, output_size, default_value);
    if ((found & 0xffu) != 0 || !key ||
        _stricmp(key, "physics") != 0) {
        return found;
    }

    std::int32_t borg_physics = 0;
    const std::uintptr_t borg_found = a2fo_a1_call_thiscall_3(
        at(g_armada, kParameterDbGetIntRva), parameter_db,
        reinterpret_cast<std::uintptr_t>("borgPhysics"),
        reinterpret_cast<std::uintptr_t>(&borg_physics), 0);
    const char* model =
        (borg_found & 0xffu) != 0 && borg_physics != 0 ? "borg" : "smooth";
    const std::size_t model_size = std::strlen(model) + 1;
    if (output_size < model_size || !writable_range(output, model_size)) {
        return found;
    }
    std::memcpy(output, model, model_size);

    const LONG incident = InterlockedIncrement(
        &g_legacy_physics_model_default_count);
    if (incident <= kMaximumLegacyPhysicsDefaultReports) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A1 physics model default #%ld: physics='%s' "
            "(borgPhysics=%s)",
            static_cast<long>(incident), model,
            (borg_found & 0xffu) == 0 ? "missing" :
                (borg_physics != 0 ? "1" : "0"));
        log_line(message);
    } else if (incident == kMaximumLegacyPhysicsDefaultReports + 1) {
        log_line("Further A1 physics model defaults suppressed");
    }
    return 1;
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

using FofsGetVirtualDirectoryFn =
    void* (__attribute__((regparm(2))) *)(
        void* file_system, void* delphi_directory_name);
using DelphiLStrFromPCharFn =
    void (__attribute__((regparm(2))) *)(
        void** destination, const char* source);
using DelphiLStrClearFn =
    void (__attribute__((regparm(1))) *)(void** value);
using DefaultCursorLookupFn =
    void* (__cdecl*)(const char* configuration_key,
                     const char* default_sprite);

void* __attribute__((regparm(2))) fofs_get_virtual_directory_hook(
    void* file_system, void* delphi_directory_name) noexcept {
    const auto original = reinterpret_cast<FofsGetVirtualDirectoryFn>(
        g_fofs_get_virtual_directory_hook.gateway);
    if (!original) return nullptr;

    const auto* requested = static_cast<const char*>(delphi_directory_name);
    const char* normalized = requested;
    std::size_t prefix_count = 0;
    while (prefix_count < 16 && readable_range(normalized, 3) &&
           normalized[0] == '.' &&
           (normalized[1] == '\\' || normalized[1] == '/') &&
           normalized[2] != '\0') {
        normalized += 2;
        ++prefix_count;
    }
    if (normalized == requested) {
        return original(file_system, delphi_directory_name);
    }

    void* managed_normalized = nullptr;
    const auto from_pchar = reinterpret_cast<DelphiLStrFromPCharFn>(
        at(g_fleet_ops, kFleetOpsLStrFromPCharRva));
    const auto clear_string = reinterpret_cast<DelphiLStrClearFn>(
        at(g_fleet_ops, kFleetOpsLStrClearRva));
    from_pchar(&managed_normalized, normalized);
    if (!managed_normalized) {
        return original(file_system, delphi_directory_name);
    }

    const LONG incident = InterlockedIncrement(
        &g_virtual_directory_normalization_count);
    if (incident <= kMaximumVirtualDirectoryNormalizationReports) {
        char requested_copy[192]{};
        char normalized_copy[192]{};
        copy_printable_string(
            requested, requested_copy, sizeof(requested_copy));
        copy_printable_string(
            normalized, normalized_copy, sizeof(normalized_copy));
        char message[480]{};
        std::snprintf(
            message, sizeof(message),
            "Normalized legacy virtual directory #%ld: '%s' -> '%s'",
            static_cast<long>(incident),
            requested_copy[0] ? requested_copy : "<unavailable>",
            normalized_copy[0] ? normalized_copy : "<unavailable>");
        log_line(message);
    } else if (incident ==
               kMaximumVirtualDirectoryNormalizationReports + 1) {
        log_line("Further legacy virtual-directory normalization reports "
                 "suppressed");
    }

    void* result = original(file_system, managed_normalized);
    clear_string(&managed_normalized);
    return result;
}

void* resolve_loaded_legacy_cursor_fallback() noexcept {
    if (!g_armada) return nullptr;
    void* owner = nullptr;
    void* const owner_slot = at(
        g_armada, kWorldSpriteOwnerPointerRva);
    if (readable_range(owner_slot, sizeof(owner))) {
        std::memcpy(&owner, owner_slot, sizeof(owner));
    }
    void* const database = read_pointer_at(
        owner, kWorldSpriteDatabaseOffset);
    void* fallback = nullptr;
    if (database) {
        fallback = reinterpret_cast<void*>(a2fo_a1_call_thiscall_2(
            at(g_armada, kSt3dDatabaseFindRva), database,
            reinterpret_cast<std::uintptr_t>(
                kLegacyCursorFallbackSpriteName),
            0));
    }
    if (!fallback) {
        if (InterlockedCompareExchange(
                &g_legacy_cursor_fallback_failure_reported, 1, 0) == 0) {
            log_line("A1 cursor fallback could not resolve the "
                     "loaded c_build cursor; retaining the native result");
        }
    }
    return fallback;
}

void repair_legacy_tactical_cursor_pointers() noexcept {
    if (!g_legacy_tactical_cursor_override_required || !g_armada) return;
    void* const fallback = resolve_loaded_legacy_cursor_fallback();
    if (!fallback) return;

    auto** const active_slot = reinterpret_cast<void**>(
        at(g_armada, kActiveCursorSpritePointerRva));
    if (!readable_range(active_slot, sizeof(*active_slot)) ||
        !writable_range(active_slot, sizeof(*active_slot))) {
        return;
    }
    void* active_cursor = nullptr;
    std::memcpy(&active_cursor, active_slot, sizeof(active_cursor));
    bool active_replaced = false;
    unsigned cache_replacements = 0;

    for (std::size_t index = 0;
         index < kTacticalSelectCursorCacheSlotRvas.size(); ++index) {
        auto** const cache_slot = reinterpret_cast<void**>(at(
            g_armada, kTacticalSelectCursorCacheSlotRvas[index]));
        if (!readable_range(cache_slot, sizeof(*cache_slot)) ||
            !writable_range(cache_slot, sizeof(*cache_slot))) {
            continue;
        }
        void* cached_cursor = nullptr;
        std::memcpy(&cached_cursor, cache_slot, sizeof(cached_cursor));
        if (cached_cursor && cached_cursor != fallback &&
            !g_legacy_tactical_native_overview_cursors[index]) {
            g_legacy_tactical_native_overview_cursors[index] = cached_cursor;
        }
        void* const native_cursor =
            g_legacy_tactical_native_overview_cursors[index];
        if (!active_replaced && native_cursor &&
            active_cursor == native_cursor) {
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(active_slot), fallback);
            active_replaced = true;
        }
        if (cached_cursor != fallback) {
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(cache_slot), fallback);
            ++cache_replacements;
        }
    }

    if ((active_replaced || cache_replacements != 0) &&
        InterlockedIncrement(&g_legacy_tactical_cursor_fallback_count) <= 8) {
        char message[400]{};
        std::snprintf(
            message, sizeof(message),
            "A1 tactical cursor pointer repair: cache=%u/2, "
            "active=%s, priorActive=%p -> '%s'=%p",
            cache_replacements, active_replaced ? "replaced" : "unchanged",
            active_cursor, kLegacyCursorFallbackSpriteName, fallback);
        log_line(message);
    }
}

void* __cdecl legacy_default_cursor_lookup_hook(
    const char* configuration_key,
    const char* default_sprite) noexcept {
    const auto original = reinterpret_cast<DefaultCursorLookupFn>(
        g_legacy_default_cursor_lookup_hook.gateway);
    void* const native_result = original
        ? original(configuration_key, default_sprite) : nullptr;
    if (!g_legacy_cursor_override_required || !g_armada) {
        return native_result;
    }

    void* const fallback = resolve_loaded_legacy_cursor_fallback();
    if (!fallback) return native_result;
    repair_legacy_tactical_cursor_pointers();

    const LONG incident = InterlockedIncrement(
        &g_legacy_cursor_fallback_count);
    if (incident <= 16) {
        char key[96]{};
        char requested[96]{};
        copy_printable_string(configuration_key, key, sizeof(key));
        copy_printable_string(default_sprite, requested, sizeof(requested));
        char message[560]{};
        std::snprintf(
            message, sizeof(message),
            "A1 default cursor fallback #%ld: key='%s', requested='%s', "
            "native=%p -> '%s'=%p",
            static_cast<long>(incident),
            key[0] ? key : "<unavailable>",
            requested[0] ? requested : "<unavailable>",
            native_result, kLegacyCursorFallbackSpriteName, fallback);
        log_line(message);
    }
    return fallback;
}

bool install_legacy_virtual_directory_normalizer(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_fleet_ops || !api->install_inline_hook ||
        !readable_range(
            at(g_fleet_ops, kFofsGetVirtualDirectoryRva),
            sizeof(kExpectedFofsGetVirtualDirectory)) ||
        std::memcmp(
            at(g_fleet_ops, kFofsGetVirtualDirectoryRva),
            kExpectedFofsGetVirtualDirectory,
            sizeof(kExpectedFofsGetVirtualDirectory)) != 0) {
        return false;
    }
    return api->install_inline_hook(
        at(g_fleet_ops, kFofsGetVirtualDirectoryRva),
        reinterpret_cast<void*>(&fofs_get_virtual_directory_hook),
        kFofsGetVirtualDirectoryHookLength,
        kExpectedFofsGetVirtualDirectory,
        &g_fofs_get_virtual_directory_hook);
}

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
    return filtered_visible;
}

bool install_producer_palette_race_filter(
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

void ensure_starbase_build_menu_capability(
    void* object_class, const char* known_odf_name) noexcept {
    if (!is_plausible_object_class(object_class) ||
        !producer_class_has_build_items(object_class)) return;

    char odf_name[64]{};
    if (known_odf_name && *known_odf_name) {
        std::snprintf(odf_name, sizeof(odf_name), "%s", known_odf_name);
    } else {
        copy_object_class_odf_name(
            object_class, odf_name, sizeof(odf_name));
    }

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

void register_completed_starbase_class_policy(
    const A2FO_GameObjectClassLoadedEvent* event,
    const char* odf_name) noexcept {
    if (!event || !event->object_class || !g_officer_state_lock_ready ||
        !g_officer_upgrade_system_ready) {
        return;
    }

    std::int32_t maximum_upgrades = 6;
    std::int32_t base_officer_gain = 20;
    std::string maximum_text;
    std::string gain_text;
    const bool has_maximum = race_event_field(
            event->odf_fields, event->odf_field_count,
            "maximumUpgrades", &maximum_text) &&
        parse_nonnegative_int32(maximum_text, maximum_upgrades);
    const bool has_gain = race_event_field(
            event->odf_fields, event->odf_field_count,
            "officerGain", &gain_text) &&
        parse_nonnegative_int32(gain_text, base_officer_gain);
    if (!has_maximum && !has_gain) return;

    if (maximum_upgrades > 256) maximum_upgrades = 256;
    if (base_officer_gain > 1000000) base_officer_gain = 1000000;

    std::array<char, 64> race{};
    std::string race_text;
    if (race_event_field(
            event->odf_fields, event->odf_field_count,
            "race", &race_text)) {
        normalize_race_name(race_text.c_str(), &race);
    }
    if (race[0] == '\0') {
        infer_starbase_race_from_object_name(odf_name, &race);
    }

    bool inserted = false;
    try {
        OfficerStateLockGuard lock;
        if (g_starbase_class_policies.find(event->object_class) ==
            g_starbase_class_policies.end()) {
            g_starbase_class_policies[event->object_class] =
                StarbaseClassPolicy{
                    maximum_upgrades, base_officer_gain, race};
            inserted = true;
        }
    } catch (...) {
        log_line("Could not retain a completed-class A1 Starbase policy");
        return;
    }
    if (!inserted) return;

    char message[448]{};
    std::snprintf(
        message, sizeof(message),
        "Recovered A1 Starbase policy at completed-class boundary: "
        "odf='%s', class=%p, maximumUpgrades=%ld, officerGain=%ld, "
        "race='%s', menuCapabilities=0x%08lx",
        odf_name && *odf_name ? odf_name : "<unavailable>",
        event->object_class, static_cast<long>(maximum_upgrades),
        static_cast<long>(base_officer_gain),
        race[0] ? race.data() : "<unavailable>",
        static_cast<unsigned long>(read_int32_at(
            event->object_class,
            kGameObjectClassMenuCapabilitiesOffset, 0)));
    log_line(message);
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
    bool inferred_race = false;
    if (!read_parameter_race(parameter_db, &race)) {
        char odf_name[128]{};
        copy_object_class_odf_name(
            object_class, odf_name, sizeof(odf_name));
        if (!infer_starbase_race_from_object_name(odf_name, &race)) {
            return object_class;
        }
        inferred_race = true;
    }
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
            "class=%p, race='%s'%s",
            static_cast<long>(incident),
            odf_name[0] ? odf_name : "<unavailable>", object_class,
            race.data(), inferred_race ? " (filename default)" : "");
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

std::uintptr_t __attribute__((fastcall))
producer_push_build_queue_item_hook(
    void* producer, void*, void* target_class) noexcept {
    bool accepted_root_order = false;
    if (producer && target_class &&
        is_officer_upgrade_class(target_class)) {
        if (!g_officer_upgrade_completion_ready ||
            !admit_officer_upgrade(producer, target_class)) {
            return 0;
        }
        accepted_root_order =
            producer == g_legacy_officer_root_dispatch_producer &&
            target_class == g_legacy_officer_root_dispatch_target;
    }
    if (!g_producer_push_target_original) return 0;
    const std::uintptr_t result = a2fo_a1_call_thiscall_1(
        g_producer_push_target_original, producer,
        reinterpret_cast<std::uintptr_t>(target_class));
    if (accepted_root_order &&
        !g_legacy_officer_root_direct_dispatch_active) {
        InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(
                &g_legacy_officer_root_pending_producer),
            producer);
        if (InterlockedCompareExchange(
                &g_legacy_officer_root_order_latched_reported,
                1, 0) == 0) {
            log_line("A1 officer root order accepted by the producer; "
                     "Root palette restoration is pending");
        }
    }
    return result;
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

std::uintptr_t __attribute__((fastcall))
starbase_start_construction_effect_hook(
    void* starbase, void*) noexcept {
    void* target_class = read_pointer_at(
        starbase, kProducerCurrentBuildClassOffset);
    if (target_class && suppress_officer_upgrade_construction_effect(
            starbase, target_class)) {
        // Native Producer::mStartConstructionEffect returns true on success.
        // The effect is cosmetic, so report success while deliberately
        // leaving the synchronized officer build untouched.
        return 1;
    }
    if (!g_starbase_start_effect_original) return 1;
    return a2fo_a1_call_thiscall_0(
        g_starbase_start_effect_original, starbase);
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

bool install_starbase_start_effect_vtable_hook() noexcept {
    if (!g_armada) return false;

    auto* starbase_vtable = at<void**>(g_armada, kStarbaseVtableRva);
    if (!readable_range(
            starbase_vtable,
            kStarbaseStartConstructionEffectVtableOffset + sizeof(void*))) {
        log_line("A1 Starbase construction-effect vtable slot inaccessible");
        return false;
    }

    auto* effect_slot = reinterpret_cast<void**>(
        reinterpret_cast<std::uint8_t*>(starbase_vtable) +
        kStarbaseStartConstructionEffectVtableOffset);
    if (!readable_range(effect_slot, sizeof(void*))) {
        log_line("A1 Starbase construction-effect vtable slot not readable");
        return false;
    }

    void* expected_original = at(
        g_armada, kStarbaseStartConstructionEffectRva);
    void* current_original = *effect_slot;
    if (current_original != expected_original ||
        !readable_range(
            current_original,
            sizeof(kExpectedStarbaseStartConstructionEffect)) ||
        std::memcmp(
            current_original, kExpectedStarbaseStartConstructionEffect,
            sizeof(kExpectedStarbaseStartConstructionEffect)) != 0) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A1 Starbase construction-effect vtable mismatch: "
            "expected=%p found=%p",
            expected_original, current_original);
        log_line(message);
        return false;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(
            effect_slot, sizeof(void*), PAGE_READWRITE, &old_protect)) {
        log_line("A1 Starbase construction-effect vtable protection change "
                 "failed");
        return false;
    }
    const auto previous = static_cast<void*>(
        InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(effect_slot),
            reinterpret_cast<PVOID>(
                &starbase_start_construction_effect_hook)));
    if (previous != current_original) {
        InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(effect_slot), previous);
        DWORD restored = 0;
        VirtualProtect(
            effect_slot, sizeof(void*), old_protect, &restored);
        log_line("A1 Starbase construction-effect vtable changed while "
                 "patching");
        return false;
    }
    DWORD restored = 0;
    VirtualProtect(effect_slot, sizeof(void*), old_protect, &restored);

    g_starbase_start_effect_vtable_slot = effect_slot;
    g_starbase_start_effect_original = previous;
    g_starbase_start_effect_vtable_hook_installed = true;
    log_line("A1 Starbase construction-effect vtable hook installed");
    return true;
}

template <std::size_t Size>
bool a1_native_entry_supported(
    const char* name, std::uintptr_t rva,
    const std::uint8_t (&expected)[Size]) noexcept;

bool install_producer_push_target_hook() noexcept {
    if (!g_armada || !g_fleet_ops) return false;

    auto** carrier = at<void*>(
        g_fleet_ops, kFoProducerPushTargetCarrierRva);
    auto** expected_slot = at<void*>(
        g_fleet_ops, kFoProducerPushTargetSlotRva);
    if (!readable_range(carrier, sizeof(void*)) ||
        *carrier != static_cast<void*>(expected_slot) ||
        !readable_range(expected_slot, sizeof(void*)) ||
        !writable_range(expected_slot, sizeof(void*))) {
        log_line("A1 Producer queue-admission target cell unavailable");
        return false;
    }

    void* const expected_original = at(
        g_armada, kProducerPushBuildQueueItemRva);
    void* const current_original = *expected_slot;
    if (current_original != expected_original ||
        !a1_native_entry_supported(
            "Producer queue insertion",
            kProducerPushBuildQueueItemRva,
            kExpectedProducerPushBuildQueueItem)) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A1 Producer queue-admission target mismatch: "
            "expected=%p found=%p",
            expected_original, current_original);
        log_line(message);
        return false;
    }

    const auto previous = static_cast<void*>(
        InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(expected_slot),
            reinterpret_cast<PVOID>(
                &producer_push_build_queue_item_hook)));
    if (previous != current_original) {
        InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(expected_slot), previous);
        log_line("A1 Producer queue-admission target changed while patching");
        return false;
    }

    g_producer_push_target_slot = expected_slot;
    g_producer_push_target_original = previous;
    g_producer_push_target_hook_installed = true;
    log_line("A1 Producer queue-admission target hook installed");
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

template <std::size_t Size>
bool a1_native_entry_supported(
    const char* name, std::uintptr_t rva,
    const std::uint8_t (&expected)[Size]) noexcept {
    if (a1_signature_matches(rva, expected)) return true;

    const auto* bytes = at<const std::uint8_t>(g_armada, rva);
    void* destination = nullptr;
    if (readable_range(bytes, 5) && bytes[0] == 0xe9) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, bytes + 1, sizeof(displacement));
        destination = const_cast<std::uint8_t*>(
            bytes + 5 + displacement);
    } else if (readable_range(bytes, 6) &&
               bytes[0] == 0x68 && bytes[5] == 0xc3) {
        std::uint32_t absolute_destination = 0;
        std::memcpy(
            &absolute_destination, bytes + 1,
            sizeof(absolute_destination));
        destination = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(absolute_destination));
    }
    void* extensions_module = reinterpret_cast<void*>(
        GetModuleHandleA("A2FOExtensions.dll"));
    const bool fleet_ops_detour =
        executable_address_in_module(g_fleet_ops, destination);
    const bool extensions_detour =
        executable_address_in_module(extensions_module, destination);
    if (fleet_ops_detour || extensions_detour) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A1Compat dependency chained existing %s %s "
            "entry at Armada RVA 0x%08lx",
            fleet_ops_detour ? "Fleet Ops" : "A2FOExtensions",
            name, static_cast<unsigned long>(rva));
        log_line(message);
        return true;
    }

    std::uint8_t actual[6]{};
    if (readable_range(bytes, sizeof(actual))) {
        std::memcpy(actual, bytes, sizeof(actual));
    }
    char message[320]{};
    std::snprintf(
        message, sizeof(message),
        "A1Compat dependency rejected %s entry at Armada RVA "
        "0x%08lx (bytes=%02X %02X %02X %02X %02X %02X, "
        "destination=%p)",
        name, static_cast<unsigned long>(rva), actual[0], actual[1],
        actual[2], actual[3], actual[4], actual[5], destination);
    log_line(message);
    return false;
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

    g_officer_upgrade_admission_ready =
        g_officer_upgrade_identity_ready &&
        install_producer_push_target_hook();
    if (!g_officer_upgrade_admission_ready) {
        log_line("A1 officer queue-admission cap unavailable");
    }

    const bool pop_signature_valid = a1_signature_matches(
        kProducerPopBuildQueueItemRva,
        kExpectedProducerPopBuildQueueItem);
    if (!pop_signature_valid) {
        log_line("A1 officer completion signature mismatch: "
                 "Producer::PopBuildQueueItem");
    }
    const bool finish_hook_installed =
        g_producer_events_ready && g_officer_upgrade_identity_ready &&
        pop_signature_valid &&
        install_starbase_finish_build_vtable_hook();
    // Set this temporarily so the direct Starbase effect wrapper can use the
    // same strict policy predicate while it is being installed. Runtime
    // readiness requires both halves: suppressing the non-renderable effect
    // and consuming the null-returning completion.
    g_officer_upgrade_completion_ready = finish_hook_installed;
    const bool effect_hook_installed =
        finish_hook_installed &&
        install_starbase_start_effect_vtable_hook();
    g_officer_upgrade_completion_ready =
        finish_hook_installed && effect_hook_installed;
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

std::uintptr_t read_rtime_class_name(
    void* file_reader, void* output, std::uint32_t requested_size) noexcept {
    if (!g_armada || !file_reader || !output) return 0;
    const auto original = reinterpret_cast<FileInFixedCharsFn>(
        at(g_armada, kFileInFixedCharsRva));
    if (!original) return 0;

    a1compat::A1BznHeader header;
    const bool a1_stream =
        requested_size == a1compat::kA2SerializedRtimeClassNameSize &&
        inspect_a1_bzn_reader(file_reader, &header);
    std::uint32_t serialized_size = requested_size;
    std::uint32_t declared_size = 0;

    if (a1_stream && readable_range(file_reader, kFileReaderInspectionSize)) {
        std::uintptr_t cursor_address = 0;
        std::memcpy(
            &cursor_address,
            static_cast<const std::uint8_t*>(file_reader) + 0x54,
            sizeof(cursor_address));
        const auto* cursor = reinterpret_cast<const std::uint8_t*>(
            cursor_address);
        if (readable_range(cursor, 8) && cursor[0] == 0x02) {
            std::memcpy(&declared_size, cursor + 4, sizeof(declared_size));
            if (declared_size ==
                    a1compat::kLegacySerializedRtimeClassNameSize ||
                declared_size == a1compat::kA2SerializedRtimeClassNameSize) {
                serialized_size = declared_size;
            }
        }
    }

    if (a1_stream) {
        apply_a1_world_bounds(header);
    }
    if (a1_stream && serialized_size < requested_size) {
        // RtimeClass::Load owns a 40-byte A2 stack buffer. Zero-pad only when
        // the live labelled field itself declares the shorter legacy width.
        std::memset(output, 0, requested_size);
    }
    const bool result = original(file_reader, output, serialized_size);

    if (a1_stream) {
        const LONG pass = InterlockedIncrement(
            &g_a1_bzn_runtime_class_read_count);
        if (pass <= 16) {
            char message[384]{};
            std::snprintf(
                message, sizeof(message),
                "Armada 1 BZN runtime-class bridge active: version=%lu, "
                "declaredNameBytes=%lu, readBytes=%lu (A2 requested %lu)",
                static_cast<unsigned long>(header.version),
                static_cast<unsigned long>(declared_size),
                static_cast<unsigned long>(serialized_size),
                static_cast<unsigned long>(requested_size));
            log_line(message);
        } else if (pass == 17) {
            log_line("Further Armada 1 BZN runtime-class reports suppressed");
        }
        if (!result) {
            const LONG failure = InterlockedIncrement(
                &g_a1_bzn_runtime_class_read_failure_count);
            if (failure <= 8) {
                char message[256]{};
                std::snprintf(
                    message, sizeof(message),
                    "Armada 1 BZN runtime-class read failed #%ld: "
                    "version=%lu, width=%lu",
                    static_cast<long>(failure),
                    static_cast<unsigned long>(header.version),
                    static_cast<unsigned long>(serialized_size));
                log_line(message);
            }
        }
    }
    return result ? 1u : 0u;
}

void* load_map_details(const char* filename,
                       bool selected_map = false) noexcept {
    if (!g_armada || !filename) return nullptr;
    const auto original = reinterpret_cast<MapDetailsFactoryLoadFn>(
        at(g_armada, kMapDetailsFactoryLoadRva));
    void* details = original ? original(filename) : nullptr;
    if (!details) return nullptr;

    try {
        a1compat::A1BznHeader header;
        std::string source_path;
        const bool found_header = find_a1_bzn_header(
            filename, header, &source_path);
        if (selected_map) {
            InterlockedExchange(
                &g_a1_selected_map_active, found_header ? 1 : 0);
            InterlockedExchange(&g_a1_relationship_restore_count, 0);
        }
        if (!found_header || !header.has_map_bounds ||
            !writable_range(details, kMapDetailsRequiredSize)) {
            return details;
        }

        float target_minimum[3]{};
        float target_size[3]{};
        if (!a1compat::a2_compatible_map_bounds(
                header, target_minimum, target_size)) {
            return details;
        }

        auto* bytes = static_cast<std::uint8_t*>(details);
        std::memcpy(bytes + kMapDetailsMinimumExtentOffset,
                    target_minimum, sizeof(target_minimum));
        std::memcpy(bytes + kMapDetailsSizeOffset, target_size,
                    sizeof(target_size));

        a1compat::A1MdfData mdf;
        std::string mdf_path;
        const bool found_mdf = read_a1_companion_mdf(
            source_path, mdf, &mdf_path);
        const bool applied_mdf = found_mdf &&
            apply_a1_mdf_start_locations(details, header, mdf);

        const LONG count = InterlockedIncrement(&g_a1_bzn_map_details_count);
        if (count <= 32) {
            char message[640]{};
            std::snprintf(
                message, sizeof(message),
                "Armada 1 BZN MapDetails #%ld: '%s' -> "
                "min=(%.2f, %.2f, %.2f), size=(%.2f, %.2f, %.2f)",
                static_cast<long>(count), source_path.c_str(),
                target_minimum[0], target_minimum[1], target_minimum[2],
                target_size[0], target_size[1], target_size[2]);
            log_line(message);
        } else if (count == 33) {
            log_line("Further Armada 1 BZN MapDetails reports suppressed");
        }

        if (applied_mdf) {
            const LONG mdf_count = InterlockedIncrement(
                &g_a1_mdf_start_location_count);
            if (mdf_count <= 32) {
                char message[768]{};
                const auto& first = mdf.start_locations[0];
                const auto& last = mdf.start_locations[
                    mdf.start_location_count == 0
                        ? 0 : mdf.start_location_count - 1];
                std::snprintf(
                    message, sizeof(message),
                    "Armada 1 MDF start bridge #%ld: '%s', starts=%lu, "
                    "first=(%lu,%lu), last=(%lu,%lu)",
                    static_cast<long>(mdf_count), mdf_path.c_str(),
                    static_cast<unsigned long>(mdf.start_location_count),
                    static_cast<unsigned long>(first.x),
                    static_cast<unsigned long>(first.y),
                    static_cast<unsigned long>(last.x),
                    static_cast<unsigned long>(last.y));
                log_line(message);
            } else if (mdf_count == 33) {
                log_line("Further Armada 1 MDF start reports suppressed");
            }
        } else {
            char message[640]{};
            std::snprintf(
                message, sizeof(message),
                "Armada 1 MDF start bridge unavailable for '%s': "
                "companion=%s, parsed=%s",
                source_path.c_str(),
                mdf_path.empty() ? "<not found>" : mdf_path.c_str(),
                found_mdf ? "yes" : "no");
            log_line(message);
        }
    } catch (...) {
        // Preserve the native MapDetails object if metadata inspection fails.
    }
    return details;
}

std::uintptr_t load_game_objects(void* file_reader) noexcept {
    // Object positions are converted from the serialized map extents to the
    // live world extents inside the native loader. Publish A1's parsed bounds
    // before that conversion starts; doing this later at the first polymorphic
    // mission record leaves every already-created object transformed against
    // A2's previous/default map bounds.
    a1compat::A1BznHeader header;
    const bool a1_stream = inspect_a1_bzn_reader(file_reader, &header);
    std::array<std::uint8_t, 12> count_record{};
    bool have_count_record = false;
    if (a1_stream) {
        apply_a1_world_bounds(header);
        FileReaderCursorState initial_state;
        if (inspect_file_reader_cursor(file_reader, &initial_state) &&
            initial_state.remaining >= count_record.size() &&
            readable_range(
                reinterpret_cast<const void*>(initial_state.cursor),
                count_record.size())) {
            std::memcpy(
                count_record.data(),
                reinterpret_cast<const void*>(initial_state.cursor),
                count_record.size());
            std::uint32_t type = 0;
            std::uint32_t size = 0;
            std::memcpy(&type, count_record.data(), sizeof(type));
            std::memcpy(&size, count_record.data() + 4, sizeof(size));
            have_count_record = (type & 0xffu) == 4u && size == 4u;
        }
    }
    const auto original = reinterpret_cast<FileReaderLoadFn>(
        at(g_armada, kGameObjectsLoadRva));
    const std::uintptr_t primary_loaded =
        original && original(file_reader) ? 1u : 0u;
    if (!primary_loaded || !a1_stream || !have_count_record || !g_armada) {
        return primary_loaded;
    }

    FileReaderCursorState tail_state;
    a1compat::A1BznObjectTailLayout tail_layout;
    if (!inspect_file_reader_cursor(file_reader, &tail_state)) {
        return primary_loaded;
    }
    const std::size_t scan_size = static_cast<std::size_t>(
        std::min<std::uintptr_t>(tail_state.remaining, 1024u * 1024u));
    const auto* scan_data = reinterpret_cast<const std::uint8_t*>(
        tail_state.cursor);
    if (!scan_size || !readable_range(scan_data, scan_size) ||
        !a1compat::locate_a1_bzn_object_tail(
            scan_data, scan_size, &tail_layout) ||
        tail_state.cursor < count_record.size()) {
        return primary_loaded;
    }

    const std::uintptr_t injected_count_cursor =
        tail_state.cursor + tail_layout.first_object_offset -
        count_record.size();
    auto* injected_count =
        reinterpret_cast<std::uint8_t*>(injected_count_cursor);
    auto* cursor_slot = static_cast<std::uint8_t*>(file_reader) + 0x54;
    if (!writable_range(injected_count, count_record.size()) ||
        !writable_range(cursor_slot, sizeof(injected_count_cursor))) {
        return primary_loaded;
    }

    std::array<std::uint8_t, 12> displaced_bytes{};
    std::memcpy(
        displaced_bytes.data(), injected_count, displaced_bytes.size());
    std::memcpy(injected_count, count_record.data(), count_record.size());
    std::memcpy(
        injected_count + 8, &tail_layout.object_count,
        sizeof(tail_layout.object_count));
    std::memcpy(
        cursor_slot, &injected_count_cursor, sizeof(injected_count_cursor));

    const auto tail_loader = reinterpret_cast<FileReaderLoadFn>(
        at(g_armada, kGameObjectsLoadRva));
    const bool tail_loaded = tail_loader && tail_loader(file_reader);
    FileReaderCursorState after_tail;
    const bool have_after =
        inspect_file_reader_cursor(file_reader, &after_tail);
    std::memcpy(
        injected_count, displaced_bytes.data(), displaced_bytes.size());

    const LONG pass = InterlockedIncrement(&g_a1_bzn_object_tail_load_count);
    if (pass <= 16) {
        char message[512]{};
        std::snprintf(
            message, sizeof(message),
            "Armada 1 BZN object-tail bridge #%ld: objects=%lu, "
            "leadingResidual=%lu, missionDistance=%lu, nativeLoaded=%s, "
            "before=%lu, after=%lu, consumed=%lu",
            static_cast<long>(pass),
            static_cast<unsigned long>(tail_layout.object_count),
            static_cast<unsigned long>(tail_layout.first_object_offset),
            static_cast<unsigned long>(tail_layout.mission_offset),
            tail_loaded ? "yes" : "no",
            static_cast<unsigned long>(tail_state.offset),
            static_cast<unsigned long>(have_after ? after_tail.offset : 0),
            static_cast<unsigned long>(
                have_after && after_tail.offset >= tail_state.offset
                    ? after_tail.offset - tail_state.offset : 0));
        log_line(message);
    } else if (pass == 17) {
        log_line("Further Armada 1 BZN object-tail reports suppressed");
    }
    return primary_loaded;
}

std::uintptr_t load_ai_mission(void* file_reader) noexcept {
    if (!g_armada || !file_reader) return 0;

    a1compat::A1BznHeader header;
    if (!inspect_a1_bzn_reader(file_reader, &header)) {
        const auto original = reinterpret_cast<AiMissionLoadFn>(
            at(g_armada, kAiMissionLoadRva));
        return original && original(file_reader) ? 1u : 0u;
    }

    const char* header_mission_name = header.mission_name[0]
        ? header.mission_name : "<unavailable>";
    FileReaderCursorState mission_cursor;
    std::uintptr_t realigned_bytes = 0;
    std::uintptr_t mission_marker_count = 0;
    if (inspect_file_reader_cursor(file_reader, &mission_cursor)) {
        const auto* cursor = reinterpret_cast<const std::uint8_t*>(
            mission_cursor.cursor);
        const auto* end = reinterpret_cast<const std::uint8_t*>(
            mission_cursor.end);
        constexpr std::size_t kMissionRecordSize =
            8 + a1compat::kA2SerializedRtimeClassNameSize;

        if (a1compat::serialized_a1_mission_record_at(cursor, end)) {
            mission_marker_count = 1;
        } else {
            const std::uintptr_t scan_limit = std::min<std::uintptr_t>(
                mission_cursor.remaining, 1024u * 1024u);
            std::uintptr_t replacement_cursor = 0;
            for (std::uintptr_t distance = 1;
                 distance + kMissionRecordSize <= scan_limit;
                 ++distance) {
                if (!a1compat::serialized_a1_mission_record_at(
                        cursor + distance, end)) {
                    continue;
                }
                ++mission_marker_count;
                replacement_cursor = mission_cursor.cursor + distance;
                if (mission_marker_count > 1) break;
            }
            if (mission_marker_count == 1) {
                auto* cursor_slot =
                    static_cast<std::uint8_t*>(file_reader) + 0x54;
                if (writable_range(cursor_slot, sizeof(replacement_cursor))) {
                    std::memcpy(
                        cursor_slot, &replacement_cursor,
                        sizeof(replacement_cursor));
                    realigned_bytes = replacement_cursor -
                        mission_cursor.cursor;
                }
            }
        }
    }
    std::uintptr_t cursor_before = 0;
    if (readable_range(file_reader, kFileReaderInspectionSize)) {
        std::memcpy(
            &cursor_before,
            static_cast<const std::uint8_t*>(file_reader) + 0x54,
            sizeof(cursor_before));
    }
    const auto original = reinterpret_cast<AiMissionLoadFn>(
        at(g_armada, kAiMissionLoadRva));
    const bool loaded = original && original(file_reader);
    std::uintptr_t cursor_after = 0;
    if (readable_range(file_reader, kFileReaderInspectionSize)) {
        std::memcpy(
            &cursor_after,
            static_cast<const std::uint8_t*>(file_reader) + 0x54,
            sizeof(cursor_after));
    }
    auto** current_slot = at<void*>(g_armada, kAiMissionCurrentRva);
    void* mission = readable_range(current_slot, sizeof(*current_slot))
        ? *current_slot : nullptr;
    const LONG pass = InterlockedIncrement(&g_a1_bzn_ai_mission_load_count);
    if (pass <= 16) {
        char message[512]{};
        std::snprintf(
            message, sizeof(message),
            "Armada 1 BZN AiMission bridge #%ld: version=%lu, "
            "headerMission='%s', nativeLoaded=%s, mission=%p, "
            "missionMarkers=%lu, realignedBytes=%lu, consumedBytes=%lu",
            static_cast<long>(pass),
            static_cast<unsigned long>(header.version), header_mission_name,
            loaded ? "yes" : "no", mission,
            static_cast<unsigned long>(mission_marker_count),
            static_cast<unsigned long>(realigned_bytes),
            static_cast<unsigned long>(
                cursor_after >= cursor_before
                    ? cursor_after - cursor_before : 0));
        log_line(message);
    } else if (pass == 17) {
        log_line("Further Armada 1 BZN AiMission bridge reports suppressed");
    }
    return loaded ? 1u : 0u;
}

std::uintptr_t load_a2_craft_class_table(void* file_reader) noexcept {
    if (!g_armada || !file_reader) return 0;

    a1compat::A1BznHeader header;
    if (!inspect_a1_bzn_reader(file_reader, &header)) {
        const auto original = reinterpret_cast<FileReaderLoadFn>(
            at(g_armada, kA2CraftClassTableLoadRva));
        return original && original(file_reader) ? 1u : 0u;
    }

    std::uintptr_t cursor = 0;
    if (readable_range(file_reader, kFileReaderInspectionSize)) {
        std::memcpy(
            &cursor,
            static_cast<const std::uint8_t*>(file_reader) + 0x54,
            sizeof(cursor));
    }
    const LONG pass = InterlockedIncrement(
        &g_a1_bzn_a2_craft_class_table_skip_count);
    if (pass <= 16) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "Armada 1 BZN load-order bridge #%ld: version=%lu, "
            "skipped A2-only craft-class table at cursor=%p",
            static_cast<long>(pass),
            static_cast<unsigned long>(header.version),
            reinterpret_cast<void*>(cursor));
        log_line(message);
    } else if (pass == 17) {
        log_line("Further Armada 1 BZN load-order reports suppressed");
    }
    return 1u;
}

bool install_a1_bzn_ai_mission_bridge(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->patch_call) return false;
    if (!a1_native_entry_supported(
            "game-object loader", kGameObjectsLoadRva,
            kExpectedGameObjectsLoad)) {
        return false;
    }
    if (!a1_native_entry_supported(
            "A2 craft-class table loader", kA2CraftClassTableLoadRva,
            kExpectedA2CraftClassTableLoad)) {
        return false;
    }
    if (!a1_native_entry_supported(
            "AiMission::LoadMission", kAiMissionLoadRva,
            kExpectedAiMissionLoad)) {
        return false;
    }
    const bool objects_patched = api->patch_call(
        at(g_armada, kLoadGameObjectsLoadCallRva),
        reinterpret_cast<void*>(&a2fo_a1_load_game_objects),
        kExpectedLoadGameObjectsLoadCall,
        sizeof(kExpectedLoadGameObjectsLoadCall));
    if (!objects_patched) return false;
    const bool load_order_patched = api->patch_call(
        at(g_armada, kLoadGameA2CraftClassTableCallRva),
        reinterpret_cast<void*>(&a2fo_a1_load_a2_craft_class_table),
        kExpectedLoadGameA2CraftClassTableCall,
        sizeof(kExpectedLoadGameA2CraftClassTableCall));
    if (!load_order_patched) return false;
    return api->patch_call(
        at(g_armada, kLoadGameAiMissionLoadCallRva),
        reinterpret_cast<void*>(&a2fo_a1_load_ai_mission),
        kExpectedLoadGameAiMissionLoadCall,
        sizeof(kExpectedLoadGameAiMissionLoadCall));
}

bool install_a1_bzn_runtime_class_bridge(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->patch_call) return false;
    if (!a1_native_entry_supported(
            "FileReader fixed-character input", kFileInFixedCharsRva,
            kExpectedFileInFixedChars)) {
        return false;
    }
    return api->patch_call(
        at(g_armada, kRtimeClassLoadReadNameCallRva),
        reinterpret_cast<void*>(&a2fo_a1_read_rtime_class_name),
        kExpectedRtimeClassLoadReadNameCall,
        sizeof(kExpectedRtimeClassLoadReadNameCall));
}

bool install_a1_bzn_map_bounds_bridge(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->patch_call) return false;
    if (!a1_native_entry_supported(
            "MapDetailsFactory::Load", kMapDetailsFactoryLoadRva,
            kExpectedMapDetailsFactoryLoad)) {
        return false;
    }
    const bool game_setup_installed = api->patch_call(
        at(g_armada, kGameSetupLoadMapDetailsCallRva),
        reinterpret_cast<void*>(&a2fo_a1_load_selected_map_details),
        kExpectedGameSetupLoadMapDetailsCall,
        sizeof(kExpectedGameSetupLoadMapDetailsCall));
    const bool known_maps_installed = api->patch_call(
        at(g_armada, kKnownMapsLoadMapDetailsCallRva),
        reinterpret_cast<void*>(&a2fo_a1_load_map_details),
        kExpectedKnownMapsLoadMapDetailsCall,
        sizeof(kExpectedKnownMapsLoadMapDetailsCall));
    return game_setup_installed && known_maps_installed;
}

bool install_a1_relationship_bridge(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->install_inline_hook) return false;
    const bool installed = api->install_inline_hook(
        at(g_armada, kToTheDeathCheckAllRva),
        reinterpret_cast<void*>(&a2fo_a1_to_the_death_check_hook),
        kToTheDeathCheckAllHookLength,
        kExpectedToTheDeathCheckAll, &g_to_the_death_check_hook);
    g_a2fo_a1_to_the_death_check_gateway = installed
        ? g_to_the_death_check_hook.gateway : nullptr;
    return installed;
}

bool install_legacy_aip_name_bridge(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->install_inline_hook) return false;
    const bool installed = api->install_inline_hook(
        at(g_armada, kAipManagerLookUpNewAipRva),
        reinterpret_cast<void*>(&a2fo_a1_aip_lookup_hook),
        kAipManagerLookUpNewAipHookLength,
        kExpectedAipManagerLookUpNewAip, &g_aip_lookup_hook);
    g_a2fo_a1_aip_lookup_gateway = installed
        ? g_aip_lookup_hook.gateway : nullptr;
    return installed;
}

bool install_legacy_aip_missing_unit_guard(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->patch_jump) return false;
    g_a2fo_a1_aip_technology_unit_continuation = at(
        g_armada,
        kAipTechnologyUnitDerefRva + kAipTechnologyUnitDerefLength);
    g_a2fo_a1_aip_technology_unit_skip = at(
        g_armada, kAipTechnologyUnitSkipRva);
    const bool installed = api->patch_jump(
        at(g_armada, kAipTechnologyUnitDerefRva),
        reinterpret_cast<void*>(&a2fo_a1_aip_technology_unit_guard_hook),
        kExpectedAipTechnologyUnitDeref,
        sizeof(kExpectedAipTechnologyUnitDeref));
    if (!installed) {
        g_a2fo_a1_aip_technology_unit_continuation = nullptr;
        g_a2fo_a1_aip_technology_unit_skip = nullptr;
    }
    return installed;
}

std::uintptr_t load_gui_sprite_tables(
    void* parser, void* database, const char* primary_filename) noexcept {
    if (!g_armada || !parser || !database) {
        log_line("Essential GUI sprite preload received an invalid native "
                 "parser/database");
        return 0;
    }

    void* read_table = at(g_armada, kSt3dTextFileParserReadTableRva);
    void* find_sprite = at(g_armada, kSt3dDatabaseFindRva);
    const std::uintptr_t essential_result = a2fo_a1_call_thiscall_2(
        read_table, parser,
        reinterpret_cast<std::uintptr_t>(database),
        reinterpret_cast<std::uintptr_t>(kEssentialGuiSpriteTableName));
    const void* essential_sentinel = reinterpret_cast<const void*>(
        a2fo_a1_call_thiscall_2(
            find_sprite, database,
            reinterpret_cast<std::uintptr_t>(
                kEssentialGuiSpriteSentinelName),
            0));

    // Preserve the exact native second-stage load even when the essential
    // table is missing or malformed. This keeps the active mod's GUI table
    // authoritative. The sentinel checks below report an incomplete
    // compatibility package without intercepting individual sprite lookups.
    const std::uintptr_t primary_result = a2fo_a1_call_thiscall_2(
        read_table, parser,
        reinterpret_cast<std::uintptr_t>(database),
        reinterpret_cast<std::uintptr_t>(primary_filename));
    const void* final_sentinel = reinterpret_cast<const void*>(
        a2fo_a1_call_thiscall_2(
            find_sprite, database,
            reinterpret_cast<std::uintptr_t>(
                kEssentialGuiSpriteSentinelName),
            0));

    const LONG pass = InterlockedIncrement(&g_gui_sprite_table_load_count);
    char message[640]{};
    std::snprintf(
        message, sizeof(message),
        "Essential GUI sprite preload #%ld: table='%s', readResult=%lu, "
        "essentialSentinel=%s; primary='%s', readResult=%lu, "
        "finalSentinel=%s",
        static_cast<long>(pass), kEssentialGuiSpriteTableName,
        static_cast<unsigned long>(essential_result),
        essential_sentinel ? "present" : "missing",
        primary_filename && *primary_filename ? primary_filename : "<null>",
        static_cast<unsigned long>(primary_result),
        final_sentinel ? "present" : "missing");
    log_line(message);
    if (!final_sentinel) {
        log_line("Essential A2 GUI sprite registration failed: "
                 "buttonBackgroundPanel.0 remains unavailable after both "
                 "sprite tables");
    }
    return primary_result;
}

bool gui_parameter_declares_int(
    void* parameter_db, const char* key) noexcept {
    if (!g_armada || !parameter_db || !key) return false;
    std::int32_t value = 0;
    return (a2fo_a1_call_thiscall_3(
        at(g_armada, kParameterDbGetIntRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(&value), 0) & 0xffu) != 0;
}

bool gui_parameter_int(
    void* parameter_db, const char* key,
    std::int32_t* output) noexcept {
    if (!g_armada || !parameter_db || !key || !output) return false;
    *output = 0;
    return (a2fo_a1_call_thiscall_3(
        at(g_armada, kParameterDbGetIntRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(output), 0) & 0xffu) != 0;
}

bool gui_parameter_rectangle(
    void* parameter_db, const char* key,
    a1compat::LegacyUiArea* output = nullptr) noexcept {
    if (!g_armada || !parameter_db || !key) return false;
    const std::array<std::int32_t, 4> fallback{};
    std::array<std::int32_t, 4> value{};
    const bool declared = (a2fo_a1_call_thiscall_3(
        at(g_armada, kParameterDbGetRectangleRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(value.data()),
        reinterpret_cast<std::uintptr_t>(fallback.data())) & 0xffu) != 0;
    if (declared && output) {
        *output = a1compat::LegacyUiArea{
            value[0], value[1], value[2], value[3]};
    }
    return declared;
}

bool gui_parameter_vector(
    void* parameter_db, const char* key,
    SpriteVector* output = nullptr) noexcept {
    if (!g_armada || !parameter_db || !key ||
        !readable_range(
            at(g_armada, kParameterDbGetVectorRva),
            sizeof(kExpectedParameterDbGetVector)) ||
        std::memcmp(
            at(g_armada, kParameterDbGetVectorRva),
            kExpectedParameterDbGetVector,
            sizeof(kExpectedParameterDbGetVector)) != 0) {
        return false;
    }
    const SpriteVector fallback{};
    SpriteVector value{};
    const bool declared = (a2fo_a1_call_thiscall_3(
        at(g_armada, kParameterDbGetVectorRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(&value),
        reinterpret_cast<std::uintptr_t>(&fallback)) & 0xffu) != 0;
    if (declared && output) *output = value;
    return declared;
}

void apply_legacy_tooltip_colours(void* tooltip) noexcept {
    if (!tooltip || InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0) {
        return;
    }

    const bool repair_background = InterlockedCompareExchange(
        &g_legacy_tooltip_background_fallback, 0, 0) != 0;
    const bool repair_text = InterlockedCompareExchange(
        &g_legacy_tooltip_text_fallback, 0, 0) != 0;
    if (!repair_background && !repair_text) return;
    auto* bytes = static_cast<std::uint8_t*>(tooltip);
    if (!writable_range(
            bytes + kTooltipBackgroundColourOffset,
            sizeof(SpriteVector) * 2)) {
        return;
    }
    if (repair_background) {
        std::memcpy(
            bytes + kTooltipBackgroundColourOffset,
            &g_legacy_tooltip_background_colour,
            sizeof(g_legacy_tooltip_background_colour));
    }
    if (repair_text) {
        const SpriteVector black{};
        std::memcpy(
            bytes + kTooltipTextColourOffset, &black, sizeof(black));
    }
    if (InterlockedCompareExchange(
            &g_legacy_tooltip_colour_reported, 1, 0) == 0) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 tooltip presentation restored: background=%s "
            "(%.3f,%.3f,%.3f), text=%s, verbose=%s",
            repair_background ? "solid A1 grey fallback" : "configured",
            static_cast<double>(g_legacy_tooltip_background_colour.x),
            static_cast<double>(g_legacy_tooltip_background_colour.y),
            static_cast<double>(g_legacy_tooltip_background_colour.z),
            repair_text ? "black fallback" : "configured",
            InterlockedCompareExchange(
                &g_legacy_tooltip_frame_fallback, 0, 0) != 0
                ? "cursor-relative A1 popup" : "configured A2 frame");
        log_line(message);
    }
}

void __attribute__((fastcall)) tooltip_render_hook(
    void* tooltip, void*) noexcept {
    apply_legacy_tooltip_colours(tooltip);
    if (g_tooltip_render_hook.gateway) {
        a2fo_a1_call_thiscall_0(
            g_tooltip_render_hook.gateway, tooltip);
    }
}

void __attribute__((fastcall)) tooltip_render_verbose_hook(
    void* tooltip, void*) noexcept {
    const bool use_legacy_popup = tooltip && g_armada &&
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) != 0 &&
        InterlockedCompareExchange(
            &g_legacy_tooltip_frame_fallback, 0, 0) != 0;
    if (!use_legacy_popup) {
        if (g_tooltip_render_verbose_hook.gateway) {
            a2fo_a1_call_thiscall_0(
                g_tooltip_render_verbose_hook.gateway, tooltip);
        }
        return;
    }

    apply_legacy_tooltip_colours(tooltip);
    const char* text = static_cast<const char*>(read_pointer_at(
        tooltip, kTooltipVerboseTextOffset));
    if (!text || !readable_range(text, 1)) return;

    const std::int32_t cursor_x = read_int32_at(
        at(g_armada, kInterfaceCursorXRva), 0, 0);
    const std::int32_t cursor_y = read_int32_at(
        at(g_armada, kInterfaceCursorYRva), 0, 0);
    a2fo_a1_call_thiscall_6(
        at(g_armada, kTooltipDrawPopupRva), tooltip,
        static_cast<std::uintptr_t>(cursor_x),
        static_cast<std::uintptr_t>(cursor_y),
        reinterpret_cast<std::uintptr_t>(text), kTooltipPopupFlags, 0, 0);
}

bool gui_parameter_string(
    void* parameter_db, const char* key, char* output,
    std::size_t output_size) noexcept {
    if (!g_armada || !parameter_db || !key || !output || output_size == 0) {
        return false;
    }
    output[0] = '\0';
    const bool declared = (a2fo_a1_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(output), output_size,
        reinterpret_cast<std::uintptr_t>("")) & 0xffu) != 0;
    output[output_size - 1] = '\0';
    return declared && output[0] != '\0';
}

bool initialize_standard_background_configuration(
    void* background, const char* prefix,
    void* initialize_function) noexcept {
    if (!background || !prefix || !initialize_function || !g_armada) {
        return false;
    }
    void** tidy_slot = at<void*>(g_armada, kNativeStringTidyIatRva);
    void** assign_slot = at<void*>(
        g_armada, kNativeStringAssignCharsIatRva);
    void** destructor_slot = at<void*>(
        g_armada, kNativeStringDestructorIatRva);
    if (!readable_range(tidy_slot, sizeof(*tidy_slot)) ||
        !readable_range(assign_slot, sizeof(*assign_slot)) ||
        !readable_range(destructor_slot, sizeof(*destructor_slot)) ||
        !is_executable_pointer(*tidy_slot) ||
        !is_executable_pointer(*assign_slot) ||
        !is_executable_pointer(*destructor_slot)) {
        return false;
    }

    // The executable uses MSVCP60's basic_string ABI. Build the short-lived
    // configuration prefix through its imported methods instead of passing a
    // MinGW std::string across the runtime boundary.
    alignas(void*) std::array<std::uint8_t, kNativeStringStorageSize>
        native_prefix{};
    a2fo_a1_call_thiscall_1(
        *tidy_slot, native_prefix.data(), 0);
    a2fo_a1_call_thiscall_2(
        *assign_slot, native_prefix.data(),
        reinterpret_cast<std::uintptr_t>(prefix), std::strlen(prefix));
    a2fo_a1_call_thiscall_1(
        initialize_function, background,
        reinterpret_cast<std::uintptr_t>(native_prefix.data()));
    a2fo_a1_call_thiscall_0(
        *destructor_slot, native_prefix.data());
    return true;
}

void destroy_legacy_control_background() noexcept {
    if (g_legacy_control_background_constructed && g_armada) {
        a2fo_a1_call_thiscall_0(
            at(g_armada, kStandardBackgroundDestructorRva),
            g_legacy_control_background_storage.data());
    }
    g_legacy_control_background_constructed = false;
    g_legacy_control_background_parent = nullptr;
    g_legacy_control_background_storage = {};
}

bool initialize_legacy_control_background(void* popup) noexcept {
    if (!popup || !g_armada ||
        !g_legacy_control_black_mask_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_control_background_available, 0, 0) == 0) {
        return false;
    }
    if (g_legacy_control_background_constructed) {
        if (g_legacy_control_background_parent == popup) return true;
        destroy_legacy_control_background();
    }

    const a1compat::NativeUiRectangle rectangle =
        g_legacy_control_background_rect;
    const std::array<std::int32_t, 4> native_rectangle{
        rectangle.left, rectangle.top, rectangle.right, rectangle.bottom};
    a2fo_a1_call_thiscall_2(
        at(g_armada, kStandardBackgroundConstructorRva),
        g_legacy_control_background_storage.data(),
        reinterpret_cast<std::uintptr_t>(popup),
        reinterpret_cast<std::uintptr_t>(native_rectangle.data()));
    g_legacy_control_background_constructed = true;
    g_legacy_control_background_parent = popup;
    if (initialize_standard_background_configuration(
            g_legacy_control_background_storage.data(),
            kLegacyControlBackgroundPrefix,
            at(g_armada,
               kStandardBackgroundInitializeConfigurationRva))) {
        return true;
    }
    destroy_legacy_control_background();
    return false;
}

void destroy_legacy_resource_backgrounds() noexcept {
    g_legacy_resource_background_pieces = {};
    g_legacy_resource_background_piece_counts = {};
}

void destroy_legacy_speed_background() noexcept {
    g_legacy_speed_background_pieces = {};
    g_legacy_speed_background_piece_count = 0;
    g_legacy_speed_separator_piece = {};
    InterlockedExchange(&g_legacy_speed_separator_available, 0);
}

void* interface_sprite_database() noexcept {
    if (!g_armada) return nullptr;
    void** database_slot = at<void*>(
        g_armada, kInterfaceSpriteDatabasePointerRva);
    if (!readable_range(database_slot, sizeof(*database_slot))) {
        return nullptr;
    }
    return *database_slot;
}

bool draw_legacy_gui_sprite_instance(
    void* sprite,
    const a1compat::NativeUiRectangle& parent_rectangle,
    const a1compat::NativeUiRectangle& rectangle,
    const SpriteVector& colour) noexcept {
    if (!sprite || !g_fleet_ops || !readable_range(
            static_cast<std::uint8_t*>(sprite) + kSpriteFrameListOffset,
            sizeof(void*)) ||
        !*reinterpret_cast<void**>(
            static_cast<std::uint8_t*>(sprite) + kSpriteFrameListOffset) ||
        !writable_range(
            static_cast<std::uint8_t*>(sprite) + kSpriteColourOffset,
            sizeof(SpriteVector))) {
        return false;
    }
    const float width = static_cast<float>(
        rectangle.right - rectangle.left + 1);
    const float height = static_cast<float>(
        rectangle.bottom - rectangle.top + 1);
    if (width <= 0.0f || height <= 0.0f) return false;

    SpriteVector saved_colour{};
    std::memcpy(
        &saved_colour,
        static_cast<std::uint8_t*>(sprite) + kSpriteColourOffset,
        sizeof(saved_colour));
    const SpriteVector position{
        static_cast<float>(
            parent_rectangle.left + rectangle.left),
        static_cast<float>(
            parent_rectangle.top + rectangle.top),
        0.0f};
    a2fo_a1_fo_sprite_set_colour(
        at(g_fleet_ops, kFoSpriteSetColourRva), sprite, &colour);
    a2fo_a1_fo_sprite_draw_scaled_2d(
        at(g_fleet_ops, kFoSpriteDrawScaled2DRva), sprite, &position,
        width, height);
    a2fo_a1_fo_sprite_set_colour(
        at(g_fleet_ops, kFoSpriteSetColourRva), sprite, &saved_colour);
    return true;
}

void draw_legacy_gui_sprite(
    void* database, const char* name,
    const a1compat::NativeUiRectangle& parent_rectangle,
    const a1compat::NativeUiRectangle& rectangle) noexcept {
    if (!database || !name || !*name || !g_armada || !g_fleet_ops) {
        return;
    }
    void* sprite = reinterpret_cast<void*>(a2fo_a1_call_thiscall_2(
        at(g_armada, kInterfaceSpriteDatabaseGetRva), database,
        reinterpret_cast<std::uintptr_t>(name), 0));
    const SpriteVector white{1.0f, 1.0f, 1.0f};
    draw_legacy_gui_sprite_instance(
        sprite, parent_rectangle, rectangle, white);
}

void draw_legacy_speed_background() noexcept {
    if (!g_legacy_speed_panel_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_speed_panel_available, 0, 0) == 0) {
        return;
    }
    void* database = interface_sprite_database();
    if (!database) return;
    const std::size_t piece_count = std::min(
        g_legacy_speed_background_piece_count,
        g_legacy_speed_background_pieces.size());
    for (std::size_t index = 0; index < piece_count; ++index) {
        const LegacyBackgroundPiece& piece =
            g_legacy_speed_background_pieces[index];
        draw_legacy_gui_sprite(
            database, piece.sprite_name.data(),
            g_legacy_speed_panel_rect, piece.rectangle);
    }
    if (InterlockedCompareExchange(
            &g_legacy_speed_separator_available, 0, 0) != 0) {
        draw_legacy_gui_sprite(
            database,
            g_legacy_speed_separator_piece.sprite_name.data(),
            g_legacy_speed_panel_rect,
            g_legacy_speed_separator_piece.rectangle);
    }
    if (piece_count > 0 && InterlockedCompareExchange(
            &g_legacy_speed_panel_reported, 1, 0) == 0) {
        const a1compat::NativeUiRectangle rectangle =
            g_legacy_speed_panel_rect;
        const bool separator_drawn = InterlockedCompareExchange(
            &g_legacy_speed_separator_available, 0, 0) != 0;
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 SpeedRail artwork active: panel=[%ld,%ld,%ld,%ld]; "
            "%lu background pieces rendered; separator=%s",
            static_cast<long>(rectangle.left),
            static_cast<long>(rectangle.top),
            static_cast<long>(rectangle.right),
            static_cast<long>(rectangle.bottom),
            static_cast<unsigned long>(piece_count),
            separator_drawn ? "yes" : "no");
        log_line(message);
    }
}

void draw_legacy_resource_backgrounds() noexcept {
    if (!g_legacy_resource_panel_runtime_ready) return;
    void* database = interface_sprite_database();
    if (!database) return;
    for (std::size_t panel_index = 0;
         panel_index < g_legacy_resource_background_pieces.size();
         ++panel_index) {
        const std::size_t piece_count = std::min(
            g_legacy_resource_background_piece_counts[panel_index],
            g_legacy_resource_background_pieces[panel_index].size());
        for (std::size_t piece_index = 0;
             piece_index < piece_count; ++piece_index) {
            const LegacyBackgroundPiece& piece =
                g_legacy_resource_background_pieces[panel_index][piece_index];
            draw_legacy_gui_sprite(
                database, piece.sprite_name.data(),
                g_legacy_resource_panel_rect, piece.rectangle);
        }
    }
}

void draw_legacy_resource_icons() noexcept {
    if (!g_legacy_resource_panel_runtime_ready ||
        !g_armada || !g_fleet_ops) return;
    void* database = interface_sprite_database();
    if (!database) return;
    for (std::size_t index = 0;
         index < g_legacy_resource_icon_names.size(); ++index) {
        draw_legacy_gui_sprite(
            database, g_legacy_resource_icon_names[index].data(),
            g_legacy_resource_panel_rect,
            g_legacy_resource_icon_rects[index]);
    }
}

void cache_legacy_caption_text_component(void* panel) noexcept {
    if (!panel) return;
    void* resource_display = read_pointer_at(
        panel, kResourcePanelFirstDisplayPointerOffset);
    void* text_component = read_pointer_at(
        resource_display, kResourceDisplayTextPointerOffset);
    if (!text_component ||
        !readable_range(
            static_cast<std::uint8_t*>(text_component) +
                kTextComponentFontStateOffset,
            12)) {
        return;
    }
    g_legacy_caption_text_component = text_component;
}

bool render_legacy_resource_panel(void* panel) noexcept {
    if (!panel || InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_resource_panel_available, 0, 0) == 0) {
        return false;
    }
    cache_legacy_caption_text_component(panel);
    // If A2FOResources owns the native function-entry detour, its hook calls
    // this exported bridge after the vtable wrapper below has already drawn
    // the A1 artwork. Report the legacy panel as handled without drawing it a
    // second time.
    if (InterlockedCompareExchange(
            &g_legacy_resource_render_bridge_active, 0, 0) != 0) {
        return true;
    }
    const a1compat::NativeUiRectangle rectangle =
        g_legacy_resource_panel_rect;
    const std::array<std::int32_t, 4> native_rectangle{
        rectangle.left, rectangle.top, rectangle.right, rectangle.bottom};
    void* destination = static_cast<std::uint8_t*>(panel) +
        kResourcePanelRectangleOffset;
    if (!writable_range(destination, sizeof(native_rectangle))) return true;
    std::memcpy(destination, native_rectangle.data(), sizeof(native_rectangle));

    draw_legacy_resource_backgrounds();
    draw_legacy_resource_icons();
    if (InterlockedCompareExchange(
            &g_legacy_resource_panel_reported, 1, 0) == 0) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A1 resource strip active: panel=[%ld,%ld,%ld,%ld]; "
            "Crew/Officers/Dilithium mapped to native displays",
            static_cast<long>(rectangle.left),
            static_cast<long>(rectangle.top),
            static_cast<long>(rectangle.right),
            static_cast<long>(rectangle.bottom));
        log_line(message);
    }
    return true;
}

void __attribute__((fastcall)) resource_panel_render_vtable_hook(
    void* panel, void*) noexcept {
    render_legacy_resource_panel(panel);
    InterlockedIncrement(&g_legacy_resource_render_bridge_active);
    if (g_resource_panel_render_original) {
        a2fo_a1_call_thiscall_0(
            g_resource_panel_render_original, panel);
    }
    InterlockedDecrement(&g_legacy_resource_render_bridge_active);
}

bool install_legacy_resource_panel_adapter() noexcept {
    if (!g_armada || g_resource_panel_render_vtable_hook_installed) {
        return g_resource_panel_render_vtable_hook_installed;
    }
    auto** slot = at<void*>(
        g_armada,
        kResourcePanelVtableRva + kResourcePanelRenderVtableOffset);
    void* const native_render = at(g_armada, kResourcePanelRenderRva);
    if (!readable_range(slot, sizeof(void*)) ||
        !readable_range(native_render, 1) || *slot != native_render) {
        log_line("A1 ResourcePanel vtable signature mismatch");
        return false;
    }
    if (!readable_range(
            native_render, sizeof(kExpectedResourcePanelRender)) ||
        std::memcmp(
            native_render, kExpectedResourcePanelRender,
            sizeof(kExpectedResourcePanelRender)) != 0) {
        // A2FOResources may already own a chainable function-entry detour.
        // The exact vtable target above remains the stable ownership check.
        log_line("A1 ResourcePanel native entry already detoured; chaining "
                 "through the vtable boundary");
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(
            slot, sizeof(void*), PAGE_READWRITE, &old_protect)) {
        log_line("A1 ResourcePanel vtable protection change failed");
        return false;
    }
    void* const original = InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(slot),
        reinterpret_cast<void*>(&resource_panel_render_vtable_hook));
    DWORD restored = 0;
    VirtualProtect(slot, sizeof(void*), old_protect, &restored);
    if (original != native_render) {
        DWORD rollback_protect = 0;
        if (VirtualProtect(
                slot, sizeof(void*), PAGE_READWRITE,
                &rollback_protect)) {
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(slot), original);
            DWORD rollback_restored = 0;
            VirtualProtect(
                slot, sizeof(void*), rollback_protect,
                &rollback_restored);
        }
        log_line("A1 ResourcePanel vtable changed during installation");
        return false;
    }
    g_resource_panel_render_vtable_slot = slot;
    g_resource_panel_render_original = original;
    g_resource_panel_render_vtable_hook_installed = true;
    return true;
}

void configure_gui_parameter_db(
    void* parameter_db, const char* configuration_filename) noexcept {
    if (!g_armada || !parameter_db) return;

    destroy_legacy_control_background();
    destroy_legacy_speed_background();
    destroy_legacy_resource_backgrounds();
    InterlockedExchange(&g_legacy_gameplay_ui_active, 0);
    InterlockedExchange(&g_legacy_tooltip_background_fallback, 0);
    InterlockedExchange(&g_legacy_tooltip_text_fallback, 0);
    InterlockedExchange(&g_legacy_tooltip_frame_fallback, 0);
    InterlockedExchange(&g_legacy_tooltip_colour_reported, 0);
    g_legacy_tooltip_background_colour = {};
    InterlockedExchange(&g_legacy_control_button_rect_count, 0);
    InterlockedExchange(&g_legacy_control_button_adapter_reported, 0);
    InterlockedExchange(&g_legacy_control_background_available, 0);
    InterlockedExchange(&g_legacy_control_black_mask_available, 0);
    InterlockedExchange(&g_legacy_control_black_mask_reported, 0);
    InterlockedExchange(&g_legacy_ship_display_black_mask_available, 0);
    InterlockedExchange(&g_legacy_ship_display_black_mask_reported, 0);
    InterlockedExchange(&g_legacy_ship_display_wireframe_available, 0);
    InterlockedExchange(&g_legacy_ship_display_wireframe_reported, 0);
    InterlockedExchange(&g_legacy_ship_display_race_icon_available, 0);
    InterlockedExchange(&g_legacy_ship_display_race_icon_reported, 0);
    InterlockedExchange(
        &g_legacy_ship_display_race_icon_background_reported, 0);
    InterlockedExchange(&g_legacy_ship_display_identity_available, 0);
    InterlockedExchange(&g_legacy_ship_display_identity_reported, 0);
    InterlockedExchange(&g_legacy_ship_display_identity_text_available, 0);
    InterlockedExchange(&g_legacy_ship_display_identity_text_reported, 0);
    InterlockedExchange(
        &g_legacy_ship_display_identity_background_available, 0);
    InterlockedExchange(
        &g_legacy_ship_display_identity_background_reported, 0);
    InterlockedExchange(&g_legacy_ship_display_system_strip_reported, 0);
    InterlockedExchange(&g_legacy_ship_display_progress_available, 0);
    InterlockedExchange(&g_legacy_ship_display_progress_reported, 0);
    InterlockedExchange(
        &g_legacy_ship_display_multi_layout_available, 0);
    InterlockedExchange(
        &g_legacy_ship_display_multi_layout_reported, 0);
    InterlockedExchange(
        &g_legacy_ship_display_multi_constructor_reported, 0);
    InterlockedExchange(
        &g_legacy_ship_display_multi_crew_reported, 0);
    InterlockedExchange(&g_legacy_ship_display_energy_available, 0);
    InterlockedExchange(&g_legacy_ship_display_energy_reported, 0);
    InterlockedExchange(&g_legacy_speed_control_layout_available, 0);
    InterlockedExchange(&g_legacy_speed_queue_layout_reported, 0);
    InterlockedExchange(&g_legacy_speed_special_layout_reported, 0);
    InterlockedExchange(&g_legacy_speed_special_layout_count, 0);
    InterlockedExchange(&g_legacy_speed_transport_layout_reported, 0);
    InterlockedExchange(&g_legacy_popup_control_diagnostic_reported, 0);
    InterlockedExchange(&g_legacy_officer_root_control_reported, 0);
    InterlockedExchange(&g_legacy_officer_root_repair_report_count, 0);
    InterlockedExchange(
        &g_legacy_officer_root_order_latched_reported, 0);
    InterlockedExchange(&g_legacy_officer_root_return_reported, 0);
    InterlockedExchange(
        &g_legacy_officer_root_direct_dispatch_reported, 0);
    InterlockedExchange(
        &g_legacy_officer_build_control_hidden_reported, 0);
    InterlockedExchange(&g_legacy_speed_separator_available, 0);
    InterlockedExchange(&g_legacy_speed_cursor_reported, 0);
    InterlockedExchange(&g_legacy_construction_bar_target_reported, 0);
    InterlockedExchange(
        &g_legacy_construction_rig_progress_diagnostic_reported, 0);
    InterlockedExchange(&g_legacy_speed_panel_available, 0);
    InterlockedExchange(&g_legacy_speed_panel_reported, 0);
    InterlockedExchange(&g_legacy_cinematic_layout_available, 0);
    InterlockedExchange(&g_legacy_cinematic_layout_reported, 0);
    InterlockedExchange(&g_legacy_cinematic_buttons_available, 0);
    InterlockedExchange(&g_legacy_cinematic_buttons_reported, 0);
    InterlockedExchange(&g_legacy_cinematic_caption_reported, 0);
    g_legacy_cinematic_button_art_owner = nullptr;
    g_legacy_caption_text_component = nullptr;
    g_legacy_selected_object = nullptr;
    g_legacy_officer_root_button = nullptr;
    g_legacy_officer_root_target = nullptr;
    g_legacy_officer_root_enabled = false;
    g_legacy_officer_root_dispatch_producer = nullptr;
    g_legacy_officer_root_dispatch_target = nullptr;
    InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(
            &g_legacy_officer_root_pending_producer),
        nullptr);
    g_legacy_officer_root_direct_dispatch_active = false;
    g_legacy_officer_root_mode_info = {};
    InterlockedExchange(&g_legacy_resource_panel_available, 0);
    InterlockedExchange(&g_legacy_resource_panel_reported, 0);
    InterlockedExchange(
        &g_legacy_resource_background_suppression_reported, 0);
    InterlockedExchange(&g_legacy_ship_display_alias_report_count, 0);
    g_legacy_control_button_rects = {};
    g_legacy_control_panel_rect = {};
    g_legacy_control_background_rect = {};
    g_legacy_control_black_rect = {};
    g_legacy_speed_panel_rect = {};
    g_legacy_speed_queue_screen_rects = {};
    g_legacy_speed_transport_screen_rect = {};
    g_legacy_speed_separator_piece = {};
    g_legacy_cinematic_panel_rect = {};
    g_legacy_cinematic_background_panel_rect = {};
    g_legacy_cinematic_background_rect = {};
    g_legacy_cinematic_display_rect = {};
    g_legacy_cinematic_menu_button_rect = {};
    g_legacy_cinematic_comm_button_rect = {};
    g_legacy_cinematic_menu_button_sprite_name = {};
    g_legacy_cinematic_menu_border_sprite_name = {};
    g_legacy_cinematic_comm_button_sprite_name = {};
    g_legacy_cinematic_comm_border_sprite_name = {};
    g_legacy_ship_display_black_rect = {};
    g_legacy_ship_display_wireframe_rect = {};
    g_legacy_ship_display_race_icon_rect = {};
    g_legacy_ship_display_race_icon_display_rect = {};
    g_legacy_ship_display_class_rect = {};
    g_legacy_ship_display_name_rect = {};
    g_legacy_ship_display_crew_rect = {};
    g_legacy_ship_display_crew_dot_rect = {};
    g_legacy_ship_display_officer_rect = {};
    g_legacy_ship_display_crew_label_rect = {};
    g_legacy_ship_display_officer_label_rect = {};
    g_legacy_ship_display_progress_rect = {};
    g_legacy_ship_display_single_energy_rect = {};
    g_legacy_ship_display_multi_tile_rects = {};
    g_legacy_ship_display_multi_shield_rects = {};
    g_legacy_ship_display_multi_wireframe_rects = {};
    g_legacy_ship_display_multi_crew_rects = {};
    g_legacy_ship_display_multi_energy_rects = {};
    g_legacy_ship_display_single_energy_sprite_name = {};
    g_legacy_ship_display_multi_energy_sprite_name = {};
    g_legacy_ship_display_single_energy_colour =
        SpriteVector{1.0f, 1.0f, 0.0f};
    g_legacy_ship_display_single_energy_disabled_colour =
        SpriteVector{0.3f, 0.3f, 0.3f};
    g_legacy_ship_display_multi_energy_colour =
        SpriteVector{1.0f, 1.0f, 0.0f};
    g_legacy_ship_display_multi_energy_disabled_colour =
        SpriteVector{0.3f, 0.3f, 0.3f};
    g_legacy_ship_display_multi_crew_sprite_name = {};
    g_legacy_ship_display_crew_label_key = {};
    g_legacy_ship_display_officer_label_key = {};
    g_legacy_ship_display_crew_dot_sprite_name = {};
    g_legacy_ship_display_identity_background_pieces = {};
    g_legacy_ship_display_identity_background_piece_count = 0;
    g_legacy_resource_panel_rect = {};
    g_legacy_resource_text_rects = {};
    g_legacy_resource_icon_rects = {};
    g_legacy_resource_icon_names = {};

    const a1compat::LegacyUiEvidence evidence{
        gui_parameter_rectangle(parameter_db, "speedPanelArea"),
        gui_parameter_rectangle(parameter_db, "controlPanelArea"),
        gui_parameter_declares_int(parameter_db, "screenWidth"),
        gui_parameter_declares_int(parameter_db, "screenHeight")};
    const a1compat::LegacyUiDecision decision =
        a1compat::decide_legacy_gameplay_ui(evidence);

    if (decision.legacy_layout) {
        SpriteVector configured_background{};
        const bool has_background = gui_parameter_vector(
            parameter_db, "tooltipBackgroundColor",
            &configured_background);
        const bool has_text = gui_parameter_vector(
            parameter_db, "tooltipTextColor");
        g_legacy_tooltip_background_colour = has_background
            ? configured_background
            : SpriteVector{0.5f, 0.5f, 0.5f};
        InterlockedExchange(
            &g_legacy_tooltip_background_fallback,
            has_background ? 0 : 1);
        InterlockedExchange(
            &g_legacy_tooltip_text_fallback, has_text ? 0 : 1);

        // Armada I renders verbose text as the same cursor-relative popup as
        // its short tooltip. A2 instead requires a seven-key framed layout;
        // use that A2 path only when the active mod actually supplies the
        // complete contract.
        constexpr std::array<const char*, 7> frame_keys{{
            "frameLocation", "tooltipFrameBottomRect_0",
            "tooltipFrameBottomRect_1", "tooltipFrameTopRect_0",
            "tooltipFrameTopRect_1", "tooltipFrameLeftRect",
            "tooltipFrameRightRect"}};
        bool has_complete_frame = true;
        for (const char* key : frame_keys) {
            has_complete_frame =
                gui_parameter_rectangle(parameter_db, key) &&
                has_complete_frame;
        }
        InterlockedExchange(
            &g_legacy_tooltip_frame_fallback,
            has_complete_frame ? 0 : 1);
    }

    LegacyGameplayConfigAreas raw_legacy_config{};
    const bool has_raw_legacy_config = decision.legacy_layout &&
        load_active_legacy_gameplay_config(
            configuration_filename, &raw_legacy_config);
    double legacy_ui_scale_x = 0.0;
    double legacy_ui_scale_y = 0.0;

    if (decision.apply_reference_resolution && writable_range(
            static_cast<std::uint8_t*>(parameter_db) +
                kParameterDbScreenWidthOffset,
            sizeof(std::int32_t) * 2)) {
        const std::array<std::int32_t, 2> resolution{
            decision.reference_width, decision.reference_height};
        std::memcpy(
            static_cast<std::uint8_t*>(parameter_db) +
                kParameterDbScreenWidthOffset,
            resolution.data(), sizeof(resolution));
    }

    std::int32_t captured_button_count = 0;
    if (decision.legacy_layout) {
        a1compat::LegacyUiArea control_panel{};
        if (gui_parameter_rectangle(
                parameter_db, "controlPanelArea", &control_panel)) {
            if (has_raw_legacy_config &&
                a1compat::usable_legacy_ui_area(
                    raw_legacy_config.control_panel)) {
                const a1compat::LegacyUiArea& raw_control =
                    raw_legacy_config.control_panel;
                const std::int32_t raw_right =
                    raw_control.x + raw_control.width;
                const std::int32_t raw_bottom =
                    raw_control.y + raw_control.height;
                if (raw_right == a1compat::kLegacyUiReferenceWidth) {
                    legacy_ui_scale_x = static_cast<double>(
                        control_panel.x + control_panel.width) /
                        a1compat::kLegacyUiReferenceWidth;
                } else {
                    legacy_ui_scale_x = static_cast<double>(
                        control_panel.width) / raw_control.width;
                }
                if (raw_bottom == a1compat::kLegacyUiReferenceHeight) {
                    legacy_ui_scale_y = static_cast<double>(
                        control_panel.y + control_panel.height) /
                        a1compat::kLegacyUiReferenceHeight;
                } else {
                    legacy_ui_scale_y = static_cast<double>(
                        control_panel.height) / raw_control.height;
                }
            }
            g_legacy_control_panel_rect =
                a1compat::legacy_area_to_native_rectangle(control_panel);
            for (std::uint32_t index = 0;
                 index < a1compat::kLegacyControlButtonCount; ++index) {
                char key[32]{};
                std::snprintf(
                    key, sizeof(key), "controlButton%lu",
                    static_cast<unsigned long>(index + 1));
                a1compat::LegacyUiArea local_button{};
                if (!gui_parameter_rectangle(
                        parameter_db, key, &local_button)) {
                    break;
                }
                g_legacy_control_button_rects[index] =
                    a1compat::legacy_area_to_native_rectangle(
                        local_button);
                ++captured_button_count;
            }

            a1compat::LegacyUiArea background_area{
                0, 0, control_panel.width, control_panel.height};
            gui_parameter_rectangle(
                parameter_db, "controlBackgroundPanelArea",
                &background_area);
            g_legacy_control_background_rect =
                a1compat::legacy_area_to_native_rectangle(
                    background_area);
            InterlockedExchange(
                &g_legacy_control_background_available, 1);
        }

        a1compat::LegacyUiArea control_black{};
        if (gui_parameter_rectangle(
                parameter_db, "controlBlackArea", &control_black) &&
            a1compat::usable_legacy_ui_area(control_black)) {
            g_legacy_control_black_rect =
                a1compat::legacy_area_to_native_rectangle(control_black);
            InterlockedExchange(
                &g_legacy_control_black_mask_available, 1);
        }

        a1compat::LegacyUiArea info_black{};
        if (gui_parameter_rectangle(
                parameter_db, "infoBlackArea", &info_black) &&
            a1compat::usable_legacy_ui_area(info_black)) {
            g_legacy_ship_display_black_rect =
                a1compat::legacy_area_to_native_rectangle(info_black);
            InterlockedExchange(
                &g_legacy_ship_display_black_mask_available, 1);
        }

        a1compat::LegacyUiArea info_single_wireframe{};
        if (gui_parameter_rectangle(
                parameter_db, "infoSingleWireframeIconArea",
                &info_single_wireframe) &&
            a1compat::usable_legacy_ui_area(info_single_wireframe)) {
            g_legacy_ship_display_wireframe_rect =
                a1compat::legacy_area_to_native_rectangle(
                    info_single_wireframe);
            InterlockedExchange(
                &g_legacy_ship_display_wireframe_available, 1);
        }

        a1compat::LegacyUiArea info_single_crew_dot{};
        if (gui_parameter_rectangle(
                parameter_db, "infoSingleCrewDotArea",
                &info_single_crew_dot) &&
            a1compat::usable_legacy_ui_area(info_single_crew_dot)) {
            g_legacy_ship_display_crew_dot_rect =
                a1compat::legacy_area_to_native_rectangle(
                    info_single_crew_dot);
            gui_parameter_string(
                parameter_db, "infoSingleCrewDot",
                g_legacy_ship_display_crew_dot_sprite_name.data(),
                g_legacy_ship_display_crew_dot_sprite_name.size());
        }

        a1compat::LegacyUiArea speed_panel{};
        a1compat::LegacyUiArea speed_background{};
        std::array<a1compat::LegacyUiArea,
                   kMaximumLegacySpeedBackgroundPieces>
            speed_background_pieces{};
        std::array<LegacyBackgroundPiece,
                   kMaximumLegacySpeedBackgroundPieces>
            captured_speed_pieces{};
        std::array<char, 64> speed_background_sprite_base{};
        std::int32_t speed_background_piece_count = 0;
        const bool has_speed_panel =
            gui_parameter_rectangle(
                parameter_db, kLegacySpeedPanelAreaKey, &speed_panel) &&
            a1compat::usable_legacy_ui_area(speed_panel);
        bool complete_speed_background = has_speed_panel &&
            gui_parameter_rectangle(
                parameter_db, "speedBackgroundPanelArea",
                &speed_background) &&
            a1compat::usable_legacy_ui_area(speed_background) &&
            gui_parameter_string(
                parameter_db, kLegacySpeedBackgroundPrefix,
                speed_background_sprite_base.data(),
                speed_background_sprite_base.size()) &&
            gui_parameter_int(
                parameter_db, "speedBackgroundPanelSize",
                &speed_background_piece_count) &&
            speed_background_piece_count > 0 &&
            speed_background_piece_count <= static_cast<std::int32_t>(
                kMaximumLegacySpeedBackgroundPieces);
        for (std::int32_t index = 0;
             complete_speed_background &&
             index < speed_background_piece_count; ++index) {
            char rectangle_key[96]{};
            std::snprintf(
                rectangle_key, sizeof(rectangle_key), "%s_%ld",
                kLegacySpeedBackgroundPrefix, static_cast<long>(index));
            if (!gui_parameter_rectangle(
                    parameter_db, rectangle_key,
                    &speed_background_pieces[
                        static_cast<std::size_t>(index)])) {
                complete_speed_background = false;
                break;
            }
            LegacyBackgroundPiece& piece =
                captured_speed_pieces[static_cast<std::size_t>(index)];
            piece.rectangle = a1compat::legacy_area_to_native_rectangle(
                a1compat::legacy_child_area_in_parent(
                    speed_background,
                    speed_background_pieces[
                        static_cast<std::size_t>(index)]));
            std::snprintf(
                piece.sprite_name.data(), piece.sprite_name.size(),
                "%s.%ld", speed_background_sprite_base.data(),
                static_cast<long>(index));
        }

        a1compat::LegacyUiArea speed_single_button{};
        a1compat::LegacyUiArea speed_separator{};
        std::array<char, 96> speed_separator_sprite{};
        const bool has_speed_separator_sprite = gui_parameter_string(
            parameter_db, "speedSeparator",
            speed_separator_sprite.data(),
            speed_separator_sprite.size());
        std::int32_t speed_single_button_gap = 0;
        const bool complete_speed_controls = has_speed_panel &&
            gui_parameter_rectangle(
                parameter_db, "speedSingleButtonArea",
                &speed_single_button) &&
            a1compat::usable_legacy_ui_area(speed_single_button) &&
            gui_parameter_rectangle(
                parameter_db, "speedSeparatorArea", &speed_separator) &&
            a1compat::usable_legacy_ui_area(speed_separator) &&
            gui_parameter_int(
                parameter_db, "speedSingleButtonGap",
                &speed_single_button_gap) &&
            legacy_ui_scale_x > 0.0 &&
            std::isfinite(legacy_ui_scale_x);
        if (complete_speed_controls) {
            const std::int32_t scaled_gap = static_cast<std::int32_t>(
                std::lround(
                    static_cast<double>(speed_single_button_gap) *
                    legacy_ui_scale_x));
            for (std::size_t index = 0;
                 index < g_legacy_speed_queue_screen_rects.size(); ++index) {
                const a1compat::LegacyUiArea local_slot =
                    a1compat::legacy_speed_rail_button_slot(
                        speed_single_button, speed_separator, scaled_gap,
                        a1compat::kLegacySpeedQueueFirstSlot +
                            static_cast<std::uint32_t>(index));
                g_legacy_speed_queue_screen_rects[index] =
                    a1compat::legacy_area_to_native_rectangle(
                        a1compat::legacy_child_area_in_parent(
                            speed_panel, local_slot));
            }
            const a1compat::LegacyUiArea transport_slot =
                a1compat::legacy_speed_rail_button_slot(
                    speed_single_button, speed_separator, scaled_gap,
                    a1compat::kLegacySpeedTransportSlot);
            g_legacy_speed_transport_screen_rect =
                a1compat::legacy_area_to_native_rectangle(
                    a1compat::legacy_child_area_in_parent(
                        speed_panel, transport_slot));
            const a1compat::LegacyUiArea separator_slot =
                a1compat::legacy_speed_rail_button_slot(
                    speed_single_button, speed_separator, scaled_gap,
                    a1compat::kLegacySpeedSeparatorSlot);
            if (has_speed_separator_sprite) {
                g_legacy_speed_separator_piece.rectangle =
                    a1compat::legacy_area_to_native_rectangle(
                        separator_slot);
                std::snprintf(
                    g_legacy_speed_separator_piece.sprite_name.data(),
                    g_legacy_speed_separator_piece.sprite_name.size(),
                    "%s", speed_separator_sprite.data());
                InterlockedExchange(
                    &g_legacy_speed_separator_available, 1);
            }
            InterlockedExchange(
                &g_legacy_speed_control_layout_available, 1);
        }
        if (complete_speed_background &&
            g_legacy_speed_panel_runtime_ready) {
            g_legacy_speed_panel_rect =
                a1compat::legacy_area_to_native_rectangle(speed_panel);
            g_legacy_speed_background_pieces = captured_speed_pieces;
            g_legacy_speed_background_piece_count =
                static_cast<std::size_t>(speed_background_piece_count);
            InterlockedExchange(&g_legacy_speed_panel_available, 1);
        }

        a1compat::LegacyUiArea cinematic_panel{};
        a1compat::LegacyUiArea cinematic_background_panel{};
        a1compat::LegacyUiArea cinematic_background{};
        a1compat::LegacyUiArea cinematic_display{};
        a1compat::LegacyUiArea cinematic_menu_button{};
        a1compat::LegacyUiArea cinematic_comm_button{};
        const bool has_cinematic_panel = gui_parameter_rectangle(
                parameter_db, "cinematicPanelArea", &cinematic_panel) &&
            a1compat::usable_legacy_ui_area(cinematic_panel);
        if (has_raw_legacy_config && has_cinematic_panel &&
            (legacy_ui_scale_x <= 0.0 || legacy_ui_scale_y <= 0.0) &&
            a1compat::usable_legacy_ui_area(
                raw_legacy_config.cinematic_panel)) {
            legacy_ui_scale_x = static_cast<double>(cinematic_panel.width) /
                raw_legacy_config.cinematic_panel.width;
            legacy_ui_scale_y = static_cast<double>(cinematic_panel.height) /
                raw_legacy_config.cinematic_panel.height;
        }
        const bool use_raw_cinematic_rectangles =
            has_raw_legacy_config && legacy_ui_scale_x > 0.0 &&
            legacy_ui_scale_y > 0.0;
        if (use_raw_cinematic_rectangles) {
            cinematic_background_panel = scale_legacy_gui_area(
                raw_legacy_config.cinematic_background_panel,
                legacy_ui_scale_x, legacy_ui_scale_y);
            cinematic_background = scale_legacy_gui_area(
                raw_legacy_config.cinematic_background,
                legacy_ui_scale_x, legacy_ui_scale_y);
            cinematic_display = scale_legacy_gui_area(
                raw_legacy_config.cinematic_display,
                legacy_ui_scale_x, legacy_ui_scale_y);
            if (raw_legacy_config.has_cinematic_buttons) {
                cinematic_menu_button = scale_legacy_gui_area(
                    raw_legacy_config.cinematic_menu_button,
                    legacy_ui_scale_x, legacy_ui_scale_y);
                cinematic_comm_button = scale_legacy_gui_area(
                    raw_legacy_config.cinematic_comm_button,
                    legacy_ui_scale_x, legacy_ui_scale_y);
            }

            gui_parameter_string(
                parameter_db, "infoSingleEnergyBar",
                g_legacy_ship_display_single_energy_sprite_name.data(),
                g_legacy_ship_display_single_energy_sprite_name.size());
            if (g_legacy_ship_display_single_energy_sprite_name[0] ==
                    '\0') {
                std::snprintf(
                    g_legacy_ship_display_single_energy_sprite_name.data(),
                    g_legacy_ship_display_single_energy_sprite_name.size(),
                    "%s", "large_energy_bar");
            }
            gui_parameter_string(
                parameter_db, "infoMultiEnergyBar",
                g_legacy_ship_display_multi_energy_sprite_name.data(),
                g_legacy_ship_display_multi_energy_sprite_name.size());
            if (g_legacy_ship_display_multi_energy_sprite_name[0] == '\0') {
                std::snprintf(
                    g_legacy_ship_display_multi_energy_sprite_name.data(),
                    g_legacy_ship_display_multi_energy_sprite_name.size(),
                    "%s", "small_energy_bar");
            }
            gui_parameter_vector(
                parameter_db, "infoSingleEnergyBarColor",
                &g_legacy_ship_display_single_energy_colour);
            gui_parameter_vector(
                parameter_db, "infoSingleEnergyBarDisableColor",
                &g_legacy_ship_display_single_energy_disabled_colour);
            gui_parameter_vector(
                parameter_db, "infoMultiEnergyBarColor",
                &g_legacy_ship_display_multi_energy_colour);
            gui_parameter_vector(
                parameter_db, "infoMultiEnergyBarDisableColor",
                &g_legacy_ship_display_multi_energy_disabled_colour);

            if (raw_legacy_config.has_single_energy_bar) {
                const a1compat::LegacyUiArea scaled_energy =
                    scale_legacy_gui_area(
                        raw_legacy_config.single_energy_bar,
                        legacy_ui_scale_x, legacy_ui_scale_y);
                if (a1compat::usable_legacy_ui_area(scaled_energy)) {
                    g_legacy_ship_display_single_energy_rect =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_energy);
                    InterlockedExchange(
                        &g_legacy_ship_display_energy_available, 1);
                }
            }

            if (raw_legacy_config.has_single_wireframe) {
                const a1compat::LegacyUiArea scaled_wireframe =
                    scale_legacy_gui_area(
                        raw_legacy_config.single_wireframe,
                        legacy_ui_scale_x, legacy_ui_scale_y);
                if (a1compat::usable_legacy_ui_area(scaled_wireframe)) {
                    g_legacy_ship_display_wireframe_rect =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_wireframe);
                    InterlockedExchange(
                        &g_legacy_ship_display_wireframe_available, 1);
                }
            }

            if (raw_legacy_config.has_single_race_icon) {
                const a1compat::LegacyUiArea scaled_race_icon =
                    scale_legacy_gui_area(
                        raw_legacy_config.single_race_icon,
                        legacy_ui_scale_x, legacy_ui_scale_y);
                const a1compat::LegacyUiArea scaled_race_icon_display =
                    scale_legacy_gui_area(
                        raw_legacy_config.single_race_icon_display,
                        legacy_ui_scale_x, legacy_ui_scale_y);
                if (a1compat::usable_legacy_ui_area(scaled_race_icon) &&
                    a1compat::usable_legacy_ui_area(
                        scaled_race_icon_display)) {
                    const a1compat::LegacyRaceIconLayout layout =
                        a1compat::legacy_race_icon_layout(
                            scaled_race_icon, scaled_race_icon_display);
                    g_legacy_ship_display_race_icon_rect = layout.component;
                    g_legacy_ship_display_race_icon_display_rect =
                        layout.display;
                    InterlockedExchange(
                        &g_legacy_ship_display_race_icon_available, 1);
                }
            }

            if (raw_legacy_config.has_single_identity_layout) {
                const a1compat::LegacyUiArea scaled_class =
                    scale_legacy_gui_area(
                        raw_legacy_config.single_class_text,
                        legacy_ui_scale_x, legacy_ui_scale_y);
                const a1compat::LegacyUiArea scaled_name =
                    scale_legacy_gui_area(
                        raw_legacy_config.single_name_text,
                        legacy_ui_scale_x, legacy_ui_scale_y);
                const a1compat::LegacyUiArea scaled_crew =
                    scale_legacy_gui_area(
                        raw_legacy_config.single_crew_text,
                        legacy_ui_scale_x, legacy_ui_scale_y);
                const a1compat::LegacyUiArea scaled_officer =
                    scale_legacy_gui_area(
                        raw_legacy_config.single_officer_text,
                        legacy_ui_scale_x, legacy_ui_scale_y);
                if (a1compat::usable_legacy_ui_area(scaled_class) &&
                    a1compat::usable_legacy_ui_area(scaled_name) &&
                    a1compat::usable_legacy_ui_area(scaled_crew) &&
                    a1compat::usable_legacy_ui_area(scaled_officer)) {
                    g_legacy_ship_display_class_rect =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_class);
                    g_legacy_ship_display_name_rect =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_name);
                    g_legacy_ship_display_crew_rect =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_crew);
                    g_legacy_ship_display_officer_rect =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_officer);
                    InterlockedExchange(
                        &g_legacy_ship_display_identity_available, 1);
                }
            }

            if (raw_legacy_config.has_single_identity_layout &&
                raw_legacy_config.has_single_identity_labels) {
                const a1compat::LegacyUiArea scaled_crew_label =
                    scale_legacy_gui_area(
                        raw_legacy_config.single_crew_label,
                        legacy_ui_scale_x, legacy_ui_scale_y);
                const a1compat::LegacyUiArea scaled_officer_label =
                    scale_legacy_gui_area(
                        raw_legacy_config.single_officer_label,
                        legacy_ui_scale_x, legacy_ui_scale_y);
                if (a1compat::usable_legacy_ui_area(scaled_crew_label) &&
                    a1compat::usable_legacy_ui_area(scaled_officer_label)) {
                    g_legacy_ship_display_crew_label_rect =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_crew_label);
                    g_legacy_ship_display_officer_label_rect =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_officer_label);
                    if (!gui_parameter_string(
                            parameter_db, "infoSingleCrewTextLabel",
                            g_legacy_ship_display_crew_label_key.data(),
                            g_legacy_ship_display_crew_label_key.size())) {
                        std::snprintf(
                            g_legacy_ship_display_crew_label_key.data(),
                            g_legacy_ship_display_crew_label_key.size(),
                            "%s", "GUI_SD_TEXT_CREW_LABEL");
                    }
                    if (!gui_parameter_string(
                            parameter_db, "infoSingleOfficerTextLabel",
                            g_legacy_ship_display_officer_label_key.data(),
                            g_legacy_ship_display_officer_label_key.size())) {
                        std::snprintf(
                            g_legacy_ship_display_officer_label_key.data(),
                            g_legacy_ship_display_officer_label_key.size(),
                            "%s", "GUI_SD_DEF_OFF_LABEL");
                    }
                    InterlockedExchange(
                        &g_legacy_ship_display_identity_text_available, 1);
                }
            }

            if (raw_legacy_config.has_single_construction_bar) {
                const a1compat::LegacyUiArea scaled_progress =
                    scale_legacy_gui_area(
                        raw_legacy_config.single_construction_bar,
                        legacy_ui_scale_x, legacy_ui_scale_y);
                if (a1compat::usable_legacy_ui_area(scaled_progress)) {
                    g_legacy_ship_display_progress_rect =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_progress);
                    InterlockedExchange(
                        &g_legacy_ship_display_progress_available, 1);
                }
            }

            if (raw_legacy_config.has_multi_ship_layout) {
                std::array<a1compat::NativeUiRectangle,
                           kLegacyMultiShipCount> multi_tiles{};
                std::array<a1compat::NativeUiRectangle,
                           kLegacyMultiShipCount> multi_shields{};
                std::array<a1compat::NativeUiRectangle,
                           kLegacyMultiShipCount> multi_wireframes{};
                std::array<a1compat::NativeUiRectangle,
                           kLegacyMultiShipCount> multi_crew_dots{};
                std::array<a1compat::NativeUiRectangle,
                           kLegacyMultiShipCount> multi_energy_bars{};
                bool complete_multi_layout = true;
                for (std::size_t index = 0;
                     index < kLegacyMultiShipCount; ++index) {
                    const a1compat::LegacyUiArea scaled_tile =
                        scale_legacy_gui_area(
                            raw_legacy_config.multi_ship_tiles[index],
                            legacy_ui_scale_x, legacy_ui_scale_y);
                    const a1compat::LegacyUiArea scaled_shield =
                        scale_legacy_gui_area(
                            a1compat::legacy_child_area_in_parent(
                                raw_legacy_config.multi_ship_tiles[index],
                                raw_legacy_config.multi_shield_bar),
                            legacy_ui_scale_x, legacy_ui_scale_y);
                    const a1compat::LegacyUiArea scaled_wireframe =
                        scale_legacy_gui_area(
                            a1compat::legacy_child_area_in_parent(
                                raw_legacy_config.multi_ship_tiles[index],
                                raw_legacy_config.multi_wireframe),
                            legacy_ui_scale_x, legacy_ui_scale_y);
                    const a1compat::LegacyUiArea scaled_crew_dot =
                        scale_legacy_gui_area(
                            a1compat::legacy_child_area_in_parent(
                                raw_legacy_config.multi_ship_tiles[index],
                                raw_legacy_config.multi_crew_dot),
                            legacy_ui_scale_x, legacy_ui_scale_y);
                    const a1compat::LegacyUiArea scaled_energy =
                        scale_legacy_gui_area(
                            a1compat::legacy_child_area_in_parent(
                                raw_legacy_config.multi_ship_tiles[index],
                                raw_legacy_config.multi_energy_bar),
                            legacy_ui_scale_x, legacy_ui_scale_y);
                    if (!a1compat::usable_legacy_ui_area(scaled_tile) ||
                        !a1compat::usable_legacy_ui_area(scaled_shield) ||
                        !a1compat::usable_legacy_ui_area(
                            scaled_wireframe) ||
                        !a1compat::usable_legacy_ui_area(
                            scaled_crew_dot) ||
                        !a1compat::usable_legacy_ui_area(scaled_energy)) {
                        complete_multi_layout = false;
                        break;
                    }
                    multi_tiles[index] =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_tile);
                    multi_shields[index] =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_shield);
                    multi_wireframes[index] =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_wireframe);
                    multi_crew_dots[index] =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_crew_dot);
                    multi_energy_bars[index] =
                        a1compat::legacy_area_to_native_rectangle(
                            scaled_energy);
                }
                if (complete_multi_layout) {
                    g_legacy_ship_display_multi_tile_rects = multi_tiles;
                    g_legacy_ship_display_multi_shield_rects =
                        multi_shields;
                    g_legacy_ship_display_multi_wireframe_rects =
                        multi_wireframes;
                    g_legacy_ship_display_multi_crew_rects =
                        multi_crew_dots;
                    g_legacy_ship_display_multi_energy_rects =
                        multi_energy_bars;
                    gui_parameter_string(
                        parameter_db, "infoMultiCrewDot",
                        g_legacy_ship_display_multi_crew_sprite_name.data(),
                        g_legacy_ship_display_multi_crew_sprite_name.size());
                    if (g_legacy_ship_display_multi_crew_sprite_name[0] ==
                            '\0') {
                        std::snprintf(
                            g_legacy_ship_display_multi_crew_sprite_name.data(),
                            g_legacy_ship_display_multi_crew_sprite_name.size(),
                            "%s", "small_crew_dot");
                    }
                    InterlockedExchange(
                        &g_legacy_ship_display_multi_layout_available, 1);
                    InterlockedExchange(
                        &g_legacy_ship_display_energy_available, 1);
                }
            }

            std::size_t identity_background_count = 0;
            std::array<char, 96> single_background_sprite_base{};
            if (raw_legacy_config.has_single_background &&
                raw_legacy_config.single_background_piece_count > 0 &&
                gui_parameter_string(
                    parameter_db, "infoSingleBackground",
                    single_background_sprite_base.data(),
                    single_background_sprite_base.size())) {
                const std::size_t piece_count = std::min(
                    raw_legacy_config.single_background_piece_count,
                    g_legacy_ship_display_identity_background_pieces.size());
                for (std::size_t index = 0; index < piece_count; ++index) {
                    LegacyBackgroundPiece& piece =
                        g_legacy_ship_display_identity_background_pieces[
                            identity_background_count++];
                    piece.rectangle =
                        a1compat::legacy_area_to_native_rectangle(
                            scale_legacy_gui_area(
                                raw_legacy_config
                                    .single_background_pieces[index],
                                legacy_ui_scale_x, legacy_ui_scale_y));
                    std::snprintf(
                        piece.sprite_name.data(), piece.sprite_name.size(),
                        "%s.%lu", single_background_sprite_base.data(),
                        static_cast<unsigned long>(index));
                }
            }
            const auto append_identity_background = [
                parameter_db, legacy_ui_scale_x, legacy_ui_scale_y,
                &identity_background_count](
                    bool available,
                    const a1compat::LegacyUiArea& raw_rectangle,
                    const char* sprite_key) {
                if (!available ||
                    identity_background_count >=
                        g_legacy_ship_display_identity_background_pieces
                            .size()) {
                    return;
                }
                LegacyBackgroundPiece& piece =
                    g_legacy_ship_display_identity_background_pieces[
                        identity_background_count];
                if (!gui_parameter_string(
                        parameter_db, sprite_key,
                        piece.sprite_name.data(),
                        piece.sprite_name.size())) {
                    return;
                }
                piece.rectangle =
                    a1compat::legacy_area_to_native_rectangle(
                        scale_legacy_gui_area(
                            raw_rectangle,
                            legacy_ui_scale_x, legacy_ui_scale_y));
                ++identity_background_count;
            };
            append_identity_background(
                raw_legacy_config.has_single_name_background,
                raw_legacy_config.single_name_background,
                "infoSingleNameTextBackground");
            append_identity_background(
                raw_legacy_config.has_single_crew_background,
                raw_legacy_config.single_crew_background,
                "infoSingleCrewTextBackground");
            append_identity_background(
                raw_legacy_config.has_single_officer_background,
                raw_legacy_config.single_officer_background,
                "infoSingleOfficerTextBackground");
            if (identity_background_count > 0 &&
                g_legacy_ship_display_background_runtime_ready) {
                g_legacy_ship_display_identity_background_piece_count =
                    identity_background_count;
                InterlockedExchange(
                    &g_legacy_ship_display_identity_background_available,
                    1);
            }
        }
        const bool complete_cinematic_layout = has_cinematic_panel &&
            ((use_raw_cinematic_rectangles) ||
             (gui_parameter_rectangle(
                  parameter_db, "cinematicBackgroundPanelArea",
                  &cinematic_background_panel) &&
              gui_parameter_rectangle(
                  parameter_db, "cinematicBackgroundArea",
                  &cinematic_background) &&
              gui_parameter_rectangle(
                  parameter_db, "cinematicDisplayArea",
                  &cinematic_display))) &&
            a1compat::usable_legacy_ui_area(cinematic_background_panel) &&
            a1compat::usable_legacy_ui_area(cinematic_background) &&
            a1compat::usable_legacy_ui_area(cinematic_display);
        if (complete_cinematic_layout) {
            g_legacy_cinematic_panel_rect =
                a1compat::legacy_area_to_native_rectangle(cinematic_panel);
            g_legacy_cinematic_background_panel_rect =
                a1compat::legacy_area_to_native_rectangle(
                    cinematic_background_panel);
            g_legacy_cinematic_background_rect =
                a1compat::legacy_area_to_native_rectangle(
                    cinematic_background);
            g_legacy_cinematic_display_rect =
                a1compat::legacy_area_to_native_rectangle(cinematic_display);
            InterlockedExchange(
                &g_legacy_cinematic_layout_available, 1);
            if (use_raw_cinematic_rectangles) {
                char message[640]{};
                std::snprintf(
                    message, sizeof(message),
                    "A1 gameplay child rectangles resolved from '%s': "
                    "scale=%.4fx%.4f, raw cinematic display="
                    "[%ld,%ld,%ld,%ld], raw wireframe=[%ld,%ld,%ld,%ld]",
                    raw_legacy_config.source_path.c_str(),
                    legacy_ui_scale_x, legacy_ui_scale_y,
                    static_cast<long>(
                        raw_legacy_config.cinematic_display.x),
                    static_cast<long>(
                        raw_legacy_config.cinematic_display.y),
                    static_cast<long>(
                        raw_legacy_config.cinematic_display.width),
                    static_cast<long>(
                        raw_legacy_config.cinematic_display.height),
                    static_cast<long>(
                        raw_legacy_config.single_wireframe.x),
                    static_cast<long>(
                        raw_legacy_config.single_wireframe.y),
                    static_cast<long>(
                        raw_legacy_config.single_wireframe.width),
                    static_cast<long>(
                        raw_legacy_config.single_wireframe.height));
                log_line(message);
            }
        }

        const bool has_cinematic_button_rectangles =
            (use_raw_cinematic_rectangles &&
             raw_legacy_config.has_cinematic_buttons) ||
            (!use_raw_cinematic_rectangles &&
             gui_parameter_rectangle(
                 parameter_db, "cinematicMenuButtonArea",
                 &cinematic_menu_button) &&
             gui_parameter_rectangle(
                 parameter_db, "cinematicCommButtonArea",
                 &cinematic_comm_button));
        const bool complete_cinematic_buttons =
            complete_cinematic_layout &&
            has_cinematic_button_rectangles &&
            a1compat::usable_legacy_ui_area(cinematic_menu_button) &&
            a1compat::usable_legacy_ui_area(cinematic_comm_button) &&
            gui_parameter_string(
                parameter_db, "cinematicMenuButton",
                g_legacy_cinematic_menu_button_sprite_name.data(),
                g_legacy_cinematic_menu_button_sprite_name.size()) &&
            gui_parameter_string(
                parameter_db, "cinematicMenuBorder",
                g_legacy_cinematic_menu_border_sprite_name.data(),
                g_legacy_cinematic_menu_border_sprite_name.size()) &&
            gui_parameter_string(
                parameter_db, "cinematicCommButton",
                g_legacy_cinematic_comm_button_sprite_name.data(),
                g_legacy_cinematic_comm_button_sprite_name.size()) &&
            gui_parameter_string(
                parameter_db, "cinematicCommBorder",
                g_legacy_cinematic_comm_border_sprite_name.data(),
                g_legacy_cinematic_comm_border_sprite_name.size());
        if (complete_cinematic_buttons &&
            g_legacy_cinematic_button_runtime_ready) {
            g_legacy_cinematic_menu_button_rect =
                a1compat::legacy_area_to_native_rectangle(
                    cinematic_menu_button);
            g_legacy_cinematic_comm_button_rect =
                a1compat::legacy_area_to_native_rectangle(
                    cinematic_comm_button);
            InterlockedExchange(
                &g_legacy_cinematic_buttons_available, 1);
        }

        std::array<a1compat::LegacyUiArea, kLegacyResourcePanelCount>
            resource_panels{};
        std::array<a1compat::LegacyUiArea, kLegacyResourcePanelCount>
            resource_texts{};
        std::array<a1compat::LegacyUiArea, kLegacyResourcePanelCount>
            resource_icons{};
        std::array<std::array<
            a1compat::LegacyUiArea,
            kMaximumLegacyResourceBackgroundPieces>,
            kLegacyResourcePanelCount> resource_background_pieces{};
        std::array<std::size_t, kLegacyResourcePanelCount>
            resource_background_piece_counts{};
        bool complete_resource_strip = true;
        for (std::size_t index = 0;
             index < kLegacyResourcePanelCount; ++index) {
            if (!gui_parameter_rectangle(
                    parameter_db, kLegacyResourcePanelAreaKeys[index],
                    &resource_panels[index]) ||
                !gui_parameter_rectangle(
                    parameter_db, kLegacyResourceTextAreaKeys[index],
                    &resource_texts[index]) ||
                !gui_parameter_rectangle(
                    parameter_db, kLegacyResourceIconAreaKeys[index],
                    &resource_icons[index]) ||
                !gui_parameter_string(
                    parameter_db, kLegacyResourceIconKeys[index],
                    g_legacy_resource_icon_names[index].data(),
                    g_legacy_resource_icon_names[index].size())) {
                complete_resource_strip = false;
                break;
            }
            std::array<char, 64> background_sprite_base{};
            char size_key[96]{};
            std::snprintf(
                size_key, sizeof(size_key), "%sSize",
                kLegacyResourceBackgroundPrefixes[index]);
            std::int32_t background_piece_count = 0;
            if (!gui_parameter_string(
                    parameter_db,
                    kLegacyResourceBackgroundPrefixes[index],
                    background_sprite_base.data(),
                    background_sprite_base.size()) ||
                !gui_parameter_int(
                    parameter_db, size_key, &background_piece_count) ||
                background_piece_count <= 0 ||
                background_piece_count > static_cast<std::int32_t>(
                    kMaximumLegacyResourceBackgroundPieces)) {
                complete_resource_strip = false;
                break;
            }
            resource_background_piece_counts[index] =
                static_cast<std::size_t>(background_piece_count);
            for (std::size_t piece_index = 0;
                 piece_index < resource_background_piece_counts[index];
                 ++piece_index) {
                char rectangle_key[96]{};
                std::snprintf(
                    rectangle_key, sizeof(rectangle_key), "%s_%lu",
                    kLegacyResourceBackgroundPrefixes[index],
                    static_cast<unsigned long>(piece_index));
                if (!gui_parameter_rectangle(
                        parameter_db, rectangle_key,
                        &resource_background_pieces[index][piece_index])) {
                    complete_resource_strip = false;
                    break;
                }
                std::snprintf(
                    g_legacy_resource_background_pieces[index][piece_index]
                        .sprite_name.data(),
                    g_legacy_resource_background_pieces[index][piece_index]
                        .sprite_name.size(),
                    "%s.%lu", background_sprite_base.data(),
                    static_cast<unsigned long>(piece_index));
            }
            if (!complete_resource_strip) break;
        }
        if (complete_resource_strip &&
            g_legacy_resource_panel_runtime_ready) {
            const a1compat::LegacyUiArea resource_union =
                a1compat::legacy_ui_area_union(
                    resource_panels[0], resource_panels[1],
                    resource_panels[2]);
            g_legacy_resource_panel_rect =
                a1compat::legacy_area_to_native_rectangle(resource_union);
            std::fill(
                g_legacy_resource_text_rects.begin(),
                g_legacy_resource_text_rects.end(),
                kHiddenLegacyResourceRectangle);
            for (std::size_t index = 0;
                 index < kLegacyResourcePanelCount; ++index) {
                g_legacy_resource_text_rects[index] =
                    a1compat::legacy_area_to_native_rectangle(
                        a1compat::legacy_child_area_in_union(
                            resource_panels[index], resource_texts[index],
                            resource_union));
                g_legacy_resource_icon_rects[index] =
                    a1compat::legacy_area_to_native_rectangle(
                        a1compat::legacy_child_area_in_union(
                            resource_panels[index], resource_icons[index],
                            resource_union));
                g_legacy_resource_background_piece_counts[index] =
                    resource_background_piece_counts[index];
                for (std::size_t piece_index = 0;
                     piece_index < resource_background_piece_counts[index];
                     ++piece_index) {
                    g_legacy_resource_background_pieces[index][piece_index]
                        .rectangle =
                        a1compat::legacy_area_to_native_rectangle(
                            a1compat::legacy_child_area_in_union(
                                resource_panels[index],
                                resource_background_pieces[index][piece_index],
                                resource_union));
                }
            }
            InterlockedExchange(&g_legacy_resource_panel_available, 1);
        }
    }

    // Publish only after the full rectangle table is ready. The UI normally
    // loads on the game thread, but this also prevents a render callback from
    // observing a partially rebuilt race layout.
    InterlockedExchange(
        &g_legacy_control_button_rect_count, captured_button_count);
    InterlockedExchange(
        &g_legacy_gameplay_ui_active, decision.legacy_layout ? 1 : 0);

}

bool write_component_rectangle(
    void* component, std::size_t offset,
    const a1compat::NativeUiRectangle& rectangle) noexcept {
    if (!component) return false;
    void* destination = static_cast<std::uint8_t*>(component) + offset;
    if (!writable_range(destination, sizeof(rectangle))) return false;
    std::memcpy(destination, &rectangle, sizeof(rectangle));
    return true;
}

void apply_legacy_cinematic_layout(void* cinematic_view) noexcept {
    if (!cinematic_view ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_cinematic_layout_available, 0, 0) == 0) {
        return;
    }

    a1compat::NativeUiRectangle previous_panel{};
    a1compat::NativeUiRectangle previous_display{};
    const void* panel_source = static_cast<const std::uint8_t*>(
        cinematic_view) + kCinematicViewRectangleOffset;
    const void* display_source = static_cast<const std::uint8_t*>(
        cinematic_view) + kCinematicViewDisplayRectangleOffset;
    if (readable_range(panel_source, sizeof(previous_panel))) {
        std::memcpy(&previous_panel, panel_source, sizeof(previous_panel));
    }
    if (readable_range(display_source, sizeof(previous_display))) {
        std::memcpy(
            &previous_display, display_source, sizeof(previous_display));
    }

    const bool panel_applied = write_component_rectangle(
        cinematic_view, kCinematicViewRectangleOffset,
        g_legacy_cinematic_panel_rect);
    const bool background_applied = write_component_rectangle(
        cinematic_view, kCinematicViewBackgroundRectangleOffset,
        g_legacy_cinematic_background_rect);
    const bool display_applied = write_component_rectangle(
        cinematic_view, kCinematicViewDisplayRectangleOffset,
        g_legacy_cinematic_display_rect);

    if (InterlockedCompareExchange(
            &g_legacy_cinematic_layout_reported, 1, 0) == 0) {
        const a1compat::NativeUiRectangle& panel =
            g_legacy_cinematic_panel_rect;
        const a1compat::NativeUiRectangle& display =
            g_legacy_cinematic_display_rect;
        char message[640]{};
        std::snprintf(
            message, sizeof(message),
            "A1 CinematicView layout active: prior panel="
            "[%ld,%ld,%ld,%ld], display=[%ld,%ld,%ld,%ld]; "
            "restored panel=[%ld,%ld,%ld,%ld], local display="
            "[%ld,%ld,%ld,%ld], absolute display=[%ld,%ld,%ld,%ld]; "
            "writes=%d/%d/%d",
            static_cast<long>(previous_panel.left),
            static_cast<long>(previous_panel.top),
            static_cast<long>(previous_panel.right),
            static_cast<long>(previous_panel.bottom),
            static_cast<long>(previous_display.left),
            static_cast<long>(previous_display.top),
            static_cast<long>(previous_display.right),
            static_cast<long>(previous_display.bottom),
            static_cast<long>(panel.left),
            static_cast<long>(panel.top),
            static_cast<long>(panel.right),
            static_cast<long>(panel.bottom),
            static_cast<long>(display.left),
            static_cast<long>(display.top),
            static_cast<long>(display.right),
            static_cast<long>(display.bottom),
            static_cast<long>(panel.left + display.left),
            static_cast<long>(panel.top + display.top),
            static_cast<long>(panel.left + display.right),
            static_cast<long>(panel.top + display.bottom),
            panel_applied ? 1 : 0,
            background_applied ? 1 : 0,
            display_applied ? 1 : 0);
        log_line(message);
    }
}

bool legacy_button_panel_child(
    void* button_panel, void* child,
    bool require_standard_button) noexcept {
    if (!button_panel || !child ||
        read_pointer_at(child, kDisplayComponentParentPointerOffset) !=
            button_panel) {
        return false;
    }
    return !require_standard_button ||
        read_pointer_at(child, 0) ==
            at(g_armada, kStandardButtonVtableRva);
}

bool configure_legacy_cinematic_button_art(
    void* button_panel, void* menu_button, void* comm_button) noexcept {
    if (!button_panel || !menu_button || !comm_button ||
        g_legacy_cinematic_button_art_owner == button_panel ||
        !g_armada) {
        return g_legacy_cinematic_button_art_owner == button_panel;
    }

    using LoadSpriteFn = void* (__cdecl*)(const char*, const char*);
    const auto load_sprite = reinterpret_cast<LoadSpriteFn>(
        at(g_armada, kDisplayInterfaceLoadSpriteRva));
    if (!load_sprite) return false;

    void* menu_surface = load_sprite(
        "cinematicMenuButton",
        g_legacy_cinematic_menu_button_sprite_name.data());
    void* menu_border = load_sprite(
        "cinematicMenuBorder",
        g_legacy_cinematic_menu_border_sprite_name.data());
    void* comm_surface = load_sprite(
        "cinematicCommButton",
        g_legacy_cinematic_comm_button_sprite_name.data());
    void* comm_border = load_sprite(
        "cinematicCommBorder",
        g_legacy_cinematic_comm_border_sprite_name.data());
    if (!menu_surface || !menu_border || !comm_surface || !comm_border) {
        return false;
    }

    void* set_sprites = at(g_armada, kStandardButtonSetSpritesRva);
    a2fo_a1_call_thiscall_2(
        set_sprites, menu_button,
        reinterpret_cast<std::uintptr_t>(menu_border),
        reinterpret_cast<std::uintptr_t>(menu_surface));
    a2fo_a1_call_thiscall_2(
        set_sprites, comm_button,
        reinterpret_cast<std::uintptr_t>(comm_border),
        reinterpret_cast<std::uintptr_t>(comm_surface));
    g_legacy_cinematic_button_art_owner = button_panel;
    return true;
}

void apply_legacy_cinematic_button_panel_layout(
    void* button_panel) noexcept {
    if (!button_panel || !g_armada ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_cinematic_buttons_available, 0, 0) == 0 ||
        read_pointer_at(button_panel, 0) !=
            at(g_armada, kButtonPanelVtableRva)) {
        return;
    }

    void* menu_button = read_pointer_at(
        button_panel, kButtonPanelMenuButtonPointerOffset);
    void* comm_button = read_pointer_at(
        button_panel, kButtonPanelCommButtonPointerOffset);
    if (!legacy_button_panel_child(
            button_panel, menu_button, true) ||
        !legacy_button_panel_child(
            button_panel, comm_button, true)) {
        return;
    }

    const auto& panel = g_legacy_cinematic_panel_rect;
    const auto& local_menu = g_legacy_cinematic_menu_button_rect;
    const auto& local_comm = g_legacy_cinematic_comm_button_rect;
    const a1compat::NativeUiRectangle screen_menu{
        panel.left + local_menu.left,
        panel.top + local_menu.top,
        panel.left + local_menu.right,
        panel.top + local_menu.bottom};
    const a1compat::NativeUiRectangle screen_comm{
        panel.left + local_comm.left,
        panel.top + local_comm.top,
        panel.left + local_comm.right,
        panel.top + local_comm.bottom};

    if (!write_component_rectangle(
            button_panel, kButtonPanelRectangleOffset,
            panel) ||
        !write_component_rectangle(
            menu_button, kDisplayComponentRectangleOffset,
            screen_menu) ||
        !write_component_rectangle(
            comm_button, kDisplayComponentRectangleOffset,
            screen_comm)) {
        return;
    }

    // ButtonPanel normally hides MENU in its compact top-bar state. A1 keeps
    // both cinematic controls available, so retain the native expanded path
    // while suppressing every unrelated A2-only child below.
    void* expanded_flag = static_cast<std::uint8_t*>(button_panel) +
        kButtonPanelExpandedFlagOffset;
    if (writable_range(expanded_flag, sizeof(std::uint8_t))) {
        *static_cast<std::uint8_t*>(expanded_flag) = 1;
    }

    void* background = read_pointer_at(
        button_panel, kButtonPanelBackgroundPointerOffset);
    if (legacy_button_panel_child(
            button_panel, background, false)) {
        write_component_rectangle(
            background, kDisplayComponentRectangleOffset,
            kHiddenLegacyButtonPanelRectangle);
    }
    for (const std::size_t offset : kButtonPanelA2OnlyButtonOffsets) {
        void* child = read_pointer_at(button_panel, offset);
        if (!legacy_button_panel_child(
                button_panel, child, true)) {
            continue;
        }
        write_component_rectangle(
            child, kDisplayComponentRectangleOffset,
            kHiddenLegacyButtonPanelRectangle);
    }

    const bool artwork_configured =
        configure_legacy_cinematic_button_art(
            button_panel, menu_button, comm_button);
    if (InterlockedCompareExchange(
            &g_legacy_cinematic_buttons_reported, 1, 0) == 0) {
        char message[640]{};
        std::snprintf(
            message, sizeof(message),
            "A1 CinematicView COMM/MENU active: panel="
            "[%ld,%ld,%ld,%ld]; COMM local=[%ld,%ld,%ld,%ld] "
            "screen=[%ld,%ld,%ld,%ld]; MENU local="
            "[%ld,%ld,%ld,%ld] screen=[%ld,%ld,%ld,%ld]; "
            "A1 artwork=%s",
            static_cast<long>(panel.left),
            static_cast<long>(panel.top),
            static_cast<long>(panel.right),
            static_cast<long>(panel.bottom),
            static_cast<long>(local_comm.left),
            static_cast<long>(local_comm.top),
            static_cast<long>(local_comm.right),
            static_cast<long>(local_comm.bottom),
            static_cast<long>(screen_comm.left),
            static_cast<long>(screen_comm.top),
            static_cast<long>(screen_comm.right),
            static_cast<long>(screen_comm.bottom),
            static_cast<long>(local_menu.left),
            static_cast<long>(local_menu.top),
            static_cast<long>(local_menu.right),
            static_cast<long>(local_menu.bottom),
            static_cast<long>(screen_menu.left),
            static_cast<long>(screen_menu.top),
            static_cast<long>(screen_menu.right),
            static_cast<long>(screen_menu.bottom),
            artwork_configured ? "yes" : "no");
        log_line(message);
    }
}

void draw_legacy_cinematic_button_labels(void* button_panel) noexcept {
    if (!button_panel || !g_legacy_caption_text_component ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_cinematic_buttons_available, 0, 0) == 0) {
        return;
    }
    const SpriteVector black{0.0f, 0.0f, 0.0f};
    const bool comm_drawn = draw_legacy_ship_text(
        "COMM", g_legacy_cinematic_comm_button_rect, black,
        g_legacy_caption_text_component, button_panel, 10);
    const bool menu_drawn = draw_legacy_ship_text(
        "MENU", g_legacy_cinematic_menu_button_rect, black,
        g_legacy_caption_text_component, button_panel, 10);
    if (comm_drawn && menu_drawn && InterlockedCompareExchange(
            &g_legacy_cinematic_caption_reported, 1, 0) == 0) {
        log_line("A1 CinematicView COMM/MENU captions active");
    }
}

void __attribute__((fastcall)) cinematic_view_render_hook(
    void* cinematic_view, void*) noexcept {
    apply_legacy_cinematic_layout(cinematic_view);
    if (g_cinematic_view_render_hook.gateway) {
        a2fo_a1_call_thiscall_0(
            g_cinematic_view_render_hook.gateway, cinematic_view);
    }
}

void __attribute__((fastcall)) button_panel_focus_game_simulate_hook(
    void* button_panel, void*,
    std::uintptr_t simulation_context) noexcept {
    apply_legacy_cinematic_button_panel_layout(button_panel);
    if (g_button_panel_focus_game_simulate_hook.gateway) {
        a2fo_a1_call_thiscall_1(
            g_button_panel_focus_game_simulate_hook.gateway,
            button_panel, simulation_context);
    }
}

void __attribute__((fastcall)) button_panel_render_hook(
    void* button_panel, void*) noexcept {
    apply_legacy_cinematic_button_panel_layout(button_panel);
    if (g_button_panel_render_hook.gateway) {
        a2fo_a1_call_thiscall_0(
            g_button_panel_render_hook.gateway, button_panel);
    }
    // Native ButtonPanel rendering restores its compact top-bar parent after
    // drawing the retained buttons. Reapply the A1 parent before the added
    // captions so DrawTextInRectangle transforms their parent-local CFG
    // rectangles through the same bottom CinematicView origin.
    apply_legacy_cinematic_button_panel_layout(button_panel);
    draw_legacy_cinematic_button_labels(button_panel);
}

bool __attribute__((fastcall)) button_panel_cursor_over_hook(
    void* button_panel, void*) noexcept {
    apply_legacy_cinematic_button_panel_layout(button_panel);
    return g_button_panel_cursor_over_hook.gateway &&
        (a2fo_a1_call_thiscall_0(
             g_button_panel_cursor_over_hook.gateway,
             button_panel) & 0xffu) != 0;
}

void draw_legacy_control_black_mask(void* popup) noexcept {
    if (!popup || !g_armada ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_control_black_mask_available, 0, 0) == 0) {
        return;
    }
    a2fo_a1_call_thiscall_3(
        at(g_armada, kDisplayInterfaceDrawRectangleRva), popup,
        reinterpret_cast<std::uintptr_t>(&g_legacy_control_black_rect),
        reinterpret_cast<std::uintptr_t>(
            at(g_armada, kInterfaceBlackColourRva)),
        kInterfaceRectangleOpaque);

    if (InterlockedCompareExchange(
            &g_legacy_control_black_mask_reported, 1, 0) == 0) {
        const a1compat::NativeUiRectangle& rectangle =
            g_legacy_control_black_rect;
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ControlPanel opaque black mask active: local="
            "[%ld,%ld,%ld,%ld]",
            static_cast<long>(rectangle.left),
            static_cast<long>(rectangle.top),
            static_cast<long>(rectangle.right),
            static_cast<long>(rectangle.bottom));
        log_line(message);
    }
}

void apply_legacy_ship_display_black_mask(void* ship_display) noexcept {
    if (!ship_display ||
        !g_legacy_ship_display_render_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_ship_display_black_mask_available, 0, 0) == 0) {
        return;
    }
    std::size_t applied_count = 0;
    for (const std::size_t offset : kShipDisplayBlackRectangleOffsets) {
        if (write_component_rectangle(
                ship_display, offset,
                g_legacy_ship_display_black_rect)) {
            ++applied_count;
        }
    }
    if (applied_count > 0 && InterlockedCompareExchange(
            &g_legacy_ship_display_black_mask_reported, 1, 0) == 0) {
        const a1compat::NativeUiRectangle& rectangle =
            g_legacy_ship_display_black_rect;
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ShipDisplay black mask active: local="
            "[%ld,%ld,%ld,%ld]; %lu/3 modes restored; opaque fill=%s",
            static_cast<long>(rectangle.left),
            static_cast<long>(rectangle.top),
            static_cast<long>(rectangle.right),
            static_cast<long>(rectangle.bottom),
            static_cast<unsigned long>(applied_count),
            g_legacy_ship_display_opaque_black_runtime_ready
                ? "enabled" : "unavailable");
        log_line(message);
    }
}

void* checked_ship_display_child(
    void* ship_display, std::size_t pointer_offset,
    std::uintptr_t expected_vtable_rva) noexcept {
    if (!ship_display || !g_armada) return nullptr;
    void* child = nullptr;
    const void* pointer_source = static_cast<const std::uint8_t*>(
        ship_display) + pointer_offset;
    if (!readable_range(pointer_source, sizeof(child))) return nullptr;
    std::memcpy(&child, pointer_source, sizeof(child));
    if (!child || !readable_range(child, sizeof(void*))) return nullptr;

    void* vtable = nullptr;
    std::memcpy(&vtable, child, sizeof(vtable));
    if (vtable != at(g_armada, expected_vtable_rva)) return nullptr;

    void* parent = nullptr;
    const void* parent_source = static_cast<const std::uint8_t*>(child) +
        kDisplayComponentParentPointerOffset;
    if (!readable_range(parent_source, sizeof(parent))) return nullptr;
    std::memcpy(&parent, parent_source, sizeof(parent));
    return parent == ship_display ? child : nullptr;
}

void* checked_structural_ship_display_child(
    void* child, void* ship_display) noexcept {
    if (!child || !ship_display || !readable_range(
            child, kDisplayComponentRectangleOffset +
                sizeof(a1compat::NativeUiRectangle))) {
        return nullptr;
    }
    void* vtable = read_pointer_at(child, 0);
    if (!vtable || !readable_range(vtable, 0x14) ||
        !is_executable_pointer(read_pointer_at(vtable, 0x10)) ||
        read_pointer_at(child, kDisplayComponentParentPointerOffset) !=
            ship_display) {
        return nullptr;
    }
    return child;
}

void* checked_multi_ship_tile(
    void* ship_display, std::size_t index) noexcept {
    if (!ship_display || index >= kShipDisplayMultiShipCount) return nullptr;

    // Fleet Operations stores the live 16-entry MultiShipIcon array behind
    // this ShipDisplay field. Treating +0x148 itself as an inline pointer array
    // instead interprets the array allocation as tile zero and silently skips
    // every nested control.
    void* tile_array = read_pointer_at(
        ship_display, kShipDisplayMultiShipPointerOffset);
    if (!tile_array || !readable_range(
            tile_array, kShipDisplayMultiShipCount * sizeof(void*))) {
        return nullptr;
    }
    return checked_structural_ship_display_child(
        read_pointer_at(tile_array, index * sizeof(void*)), ship_display);
}

void* checked_nested_ship_display_child(
    void* holder, std::size_t pointer_offset,
    void* ship_display) noexcept {
    if (!holder) return nullptr;
    return checked_structural_ship_display_child(
        read_pointer_at(holder, pointer_offset), ship_display);
}

void apply_legacy_ship_display_multi_layout(
    void* ship_display) noexcept {
    if (!ship_display || !g_armada ||
        !g_legacy_ship_display_render_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_ship_display_multi_layout_available, 0, 0) == 0) {
        return;
    }

    std::size_t placed_tiles = 0;
    std::size_t placed_shields = 0;
    std::size_t placed_wireframes = 0;
    std::size_t hidden_tiles = 0;
    for (std::size_t index = 0;
         index < kShipDisplayMultiShipCount; ++index) {
        void* tile = checked_multi_ship_tile(ship_display, index);
        if (!tile) continue;

        void* shield = checked_nested_ship_display_child(
            tile, kMultiShipShieldPointerOffset, ship_display);
        void* wireframe = checked_nested_ship_display_child(
            tile, kMultiShipWireframePointerOffset, ship_display);

        if (index < kLegacyMultiShipCount) {
            if (write_component_rectangle(
                    tile, kDisplayComponentRectangleOffset,
                    g_legacy_ship_display_multi_tile_rects[index])) {
                ++placed_tiles;
            }
            if (shield && write_component_rectangle(
                    shield, kDisplayComponentRectangleOffset,
                    g_legacy_ship_display_multi_shield_rects[index])) {
                ++placed_shields;
            }
            if (wireframe && write_component_rectangle(
                    wireframe, kDisplayComponentRectangleOffset,
                    g_legacy_ship_display_multi_wireframe_rects[index])) {
                ++placed_wireframes;
            }
            continue;
        }

        if (write_component_rectangle(
                tile, kDisplayComponentRectangleOffset,
                kHiddenLegacyResourceRectangle)) {
            ++hidden_tiles;
        }
        if (shield) {
            write_component_rectangle(
                shield, kDisplayComponentRectangleOffset,
                kHiddenLegacyResourceRectangle);
        }
        if (wireframe) {
            write_component_rectangle(
                wireframe, kDisplayComponentRectangleOffset,
                kHiddenLegacyResourceRectangle);
        }
    }

    if (placed_tiles > 0 && InterlockedCompareExchange(
            &g_legacy_ship_display_multi_layout_reported, 1, 0) == 0) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ShipDisplay multi-selection layout active: "
            "%lu/8 tiles, %lu/8 shield bars, %lu/8 wireframes restored; "
            "%lu/8 A2-only tiles hidden",
            static_cast<unsigned long>(placed_tiles),
            static_cast<unsigned long>(placed_shields),
            static_cast<unsigned long>(placed_wireframes),
            static_cast<unsigned long>(hidden_tiles));
        log_line(message);
    }
}

SpriteVector legacy_multi_ship_crew_colour(
    const void* selected_object) noexcept {
    const SpriteVector healthy{0.0f, 1.0f, 0.0f};
    const SpriteVector low{1.0f, 1.0f, 0.0f};
    const SpriteVector critical{1.0f, 0.0f, 0.0f};
    if (!selected_object) return healthy;

    float current_crew = 0.0f;
    float maximum_crew = 0.0f;
    const void* current_source =
        static_cast<const std::uint8_t*>(selected_object) +
        kSelectedObjectCurrentCrewOffset;
    const void* maximum_source =
        static_cast<const std::uint8_t*>(selected_object) +
        kSelectedObjectMaximumCrewOffset;
    if (!readable_range(current_source, sizeof(current_crew))) {
        return healthy;
    }
    std::memcpy(&current_crew, current_source, sizeof(current_crew));
    if (readable_range(maximum_source, sizeof(maximum_crew))) {
        std::memcpy(&maximum_crew, maximum_source, sizeof(maximum_crew));
    }
    if (!std::isfinite(current_crew)) return healthy;
    if (!std::isfinite(maximum_crew) || maximum_crew <= 0.0f) {
        void* object_class = read_pointer_at(
            selected_object, kGameObjectClassOffset);
        maximum_crew = static_cast<float>(std::max<std::int32_t>(
            0, read_int32_at(
                object_class, kObjectClassInitialCrewOffset, 0)));
    }

    switch (a1compat::legacy_crew_health(
            current_crew, maximum_crew)) {
        case a1compat::LegacyCrewHealth::healthy:
            return healthy;
        case a1compat::LegacyCrewHealth::low:
            return low;
        case a1compat::LegacyCrewHealth::critical:
            return critical;
    }
    return healthy;
}

void* legacy_energy_bar_colour_value(
    void*, void* output) noexcept {
    if (!output || !writable_range(output, sizeof(SpriteVector))) {
        return output;
    }
    std::memcpy(
        output, &g_legacy_energy_bar_active_colour,
        sizeof(g_legacy_energy_bar_active_colour));
    return output;
}

void* legacy_energy_bar_values_value(
    void*, void* output) noexcept {
    if (!output || !writable_range(output, sizeof(float) * 2)) {
        return output;
    }
    const std::array<float, 2> values{{
        g_legacy_energy_bar_current, g_legacy_energy_bar_maximum}};
    std::memcpy(output, values.data(), sizeof(values));
    return output;
}

bool legacy_special_energy_values(
    const void* selected_object, float* current,
    float* maximum) noexcept {
    if (!selected_object || !current || !maximum) return false;
    const void* maximum_source =
        static_cast<const std::uint8_t*>(selected_object) +
        kSelectedObjectMaximumSpecialEnergyOffset;
    const void* current_source =
        static_cast<const std::uint8_t*>(selected_object) +
        kSelectedObjectCurrentSpecialEnergyOffset;
    if (!readable_range(maximum_source, sizeof(*maximum)) ||
        !readable_range(current_source, sizeof(*current))) {
        return false;
    }
    std::memcpy(maximum, maximum_source, sizeof(*maximum));
    std::memcpy(current, current_source, sizeof(*current));
    if (!std::isfinite(*maximum) || *maximum <= 0.0f ||
        !std::isfinite(*current)) {
        return false;
    }
    *current = std::clamp(*current, 0.0f, *maximum);
    return true;
}

void* legacy_energy_bar_scratch(void* ship_display) noexcept {
    for (std::size_t index = 0;
         index < kShipDisplayMultiShipCount; ++index) {
        void* tile = checked_multi_ship_tile(ship_display, index);
        void* shield = checked_nested_ship_display_child(
            tile, kMultiShipShieldPointerOffset, ship_display);
        if (shield && readable_range(shield, kStandardBarRequiredSize) &&
            writable_range(shield, kStandardBarRequiredSize)) {
            return shield;
        }
    }
    return nullptr;
}

bool draw_legacy_special_energy_bar(
    void* scratch_bar, void* sprite, void* selected_object,
    const a1compat::NativeUiRectangle& rectangle,
    const SpriteVector& active_colour,
    const SpriteVector& disabled_colour) noexcept {
    if (!scratch_bar || !sprite || !selected_object ||
        !g_construction_bar_render_hook.gateway ||
        rectangle.right <= rectangle.left ||
        rectangle.bottom < rectangle.top) {
        return false;
    }

    float current = 0.0f;
    float maximum = 0.0f;
    if (!legacy_special_energy_values(
            selected_object, &current, &maximum)) {
        return false;
    }

    void* original_vtable = read_pointer_at(scratch_bar, 0);
    if (!original_vtable || !readable_range(
            original_vtable,
            g_legacy_energy_bar_vtable.size() * sizeof(void*))) {
        return false;
    }
    std::memcpy(
        g_legacy_energy_bar_vtable.data(), original_vtable,
        g_legacy_energy_bar_vtable.size() * sizeof(void*));
    g_legacy_energy_bar_vtable[kStandardBarActiveColourVtableIndex] =
        reinterpret_cast<void*>(&a2fo_a1_energy_bar_colour_hook);
    g_legacy_energy_bar_vtable[kStandardBarValuesVtableIndex] =
        reinterpret_cast<void*>(&a2fo_a1_energy_bar_values_hook);

    a1compat::NativeUiRectangle original_rectangle{};
    void* original_object = nullptr;
    void* original_sprite = nullptr;
    SpriteVector original_disabled_colour{};
    std::memcpy(
        &original_rectangle,
        static_cast<std::uint8_t*>(scratch_bar) +
            kDisplayComponentRectangleOffset,
        sizeof(original_rectangle));
    std::memcpy(
        &original_object,
        static_cast<std::uint8_t*>(scratch_bar) +
            kStandardBarObjectPointerOffset,
        sizeof(original_object));
    std::memcpy(
        &original_sprite,
        static_cast<std::uint8_t*>(scratch_bar) +
            kStandardBarSpritePointerOffset,
        sizeof(original_sprite));
    std::memcpy(
        &original_disabled_colour,
        static_cast<std::uint8_t*>(scratch_bar) +
            kStandardBarDisabledColourOffset,
        sizeof(original_disabled_colour));

    g_legacy_energy_bar_active_colour = active_colour;
    g_legacy_energy_bar_current = current;
    g_legacy_energy_bar_maximum = maximum;
    void* compatibility_vtable = g_legacy_energy_bar_vtable.data();
    std::memcpy(scratch_bar, &compatibility_vtable, sizeof(void*));
    std::memcpy(
        static_cast<std::uint8_t*>(scratch_bar) +
            kDisplayComponentRectangleOffset,
        &rectangle, sizeof(rectangle));
    std::memcpy(
        static_cast<std::uint8_t*>(scratch_bar) +
            kStandardBarObjectPointerOffset,
        &selected_object, sizeof(selected_object));
    std::memcpy(
        static_cast<std::uint8_t*>(scratch_bar) +
            kStandardBarSpritePointerOffset,
        &sprite, sizeof(sprite));
    std::memcpy(
        static_cast<std::uint8_t*>(scratch_bar) +
            kStandardBarDisabledColourOffset,
        &disabled_colour, sizeof(disabled_colour));

    a2fo_a1_call_thiscall_0(
        g_construction_bar_render_hook.gateway, scratch_bar);

    std::memcpy(scratch_bar, &original_vtable, sizeof(void*));
    std::memcpy(
        static_cast<std::uint8_t*>(scratch_bar) +
            kDisplayComponentRectangleOffset,
        &original_rectangle, sizeof(original_rectangle));
    std::memcpy(
        static_cast<std::uint8_t*>(scratch_bar) +
            kStandardBarObjectPointerOffset,
        &original_object, sizeof(original_object));
    std::memcpy(
        static_cast<std::uint8_t*>(scratch_bar) +
            kStandardBarSpritePointerOffset,
        &original_sprite, sizeof(original_sprite));
    std::memcpy(
        static_cast<std::uint8_t*>(scratch_bar) +
            kStandardBarDisabledColourOffset,
        &original_disabled_colour, sizeof(original_disabled_colour));
    return true;
}

void draw_legacy_ship_display_energy_bars(
    void* ship_display) noexcept {
    if (!ship_display || !g_armada ||
        !g_legacy_ship_display_render_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_ship_display_energy_available, 0, 0) == 0) {
        return;
    }

    void* scratch_bar = legacy_energy_bar_scratch(ship_display);
    void* database = interface_sprite_database();
    if (!scratch_bar || !database) return;

    void* selected_object = read_pointer_at(
        ship_display, kShipDisplaySelectedObjectPointerOffset);
    std::size_t drawn_count = 0;
    const char* mode = "single";
    if (selected_object) {
        void* sprite = reinterpret_cast<void*>(a2fo_a1_call_thiscall_2(
            at(g_armada, kInterfaceSpriteDatabaseGetRva), database,
            reinterpret_cast<std::uintptr_t>(
                g_legacy_ship_display_single_energy_sprite_name.data()),
            0));
        drawn_count += draw_legacy_special_energy_bar(
            scratch_bar, sprite, selected_object,
            g_legacy_ship_display_single_energy_rect,
            g_legacy_ship_display_single_energy_colour,
            g_legacy_ship_display_single_energy_disabled_colour)
            ? 1u : 0u;
    } else if (read_int32_at(
                   ship_display, kShipDisplayLayoutIndexOffset, 0) != 0) {
        mode = "multi";
        void* sprite = reinterpret_cast<void*>(a2fo_a1_call_thiscall_2(
            at(g_armada, kInterfaceSpriteDatabaseGetRva), database,
            reinterpret_cast<std::uintptr_t>(
                g_legacy_ship_display_multi_energy_sprite_name.data()),
            0));
        std::size_t active_count = 0;
        for (std::size_t index = 0;
             index < kLegacyMultiShipCount; ++index) {
            void* tile = checked_multi_ship_tile(ship_display, index);
            void* object = read_pointer_at(tile, 0x28);
            if (!object) continue;
            ++active_count;
            if (draw_legacy_special_energy_bar(
                    scratch_bar, sprite, object,
                    g_legacy_ship_display_multi_energy_rects[index],
                    g_legacy_ship_display_multi_energy_colour,
                    g_legacy_ship_display_multi_energy_disabled_colour)) {
                ++drawn_count;
            }
        }
        if (active_count < 2) return;
    }

    if (drawn_count > 0 && InterlockedCompareExchange(
            &g_legacy_ship_display_energy_reported, 1, 0) == 0) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ShipDisplay special-energy bars active: mode=%s "
            "drawn=%lu, singleSprite='%s', multiSprite='%s'",
            mode, static_cast<unsigned long>(drawn_count),
            g_legacy_ship_display_single_energy_sprite_name.data(),
            g_legacy_ship_display_multi_energy_sprite_name.data());
        log_line(message);
    }
}

void draw_legacy_ship_display_multi_crew_dots(
    void* ship_display) noexcept {
    if (!ship_display || !g_armada || !g_fleet_ops ||
        !g_legacy_ship_display_render_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_ship_display_multi_layout_available, 0, 0) == 0 ||
        read_int32_at(
            ship_display, kShipDisplayLayoutIndexOffset, 0) == 0 ||
        read_pointer_at(
            ship_display, kShipDisplaySelectedObjectPointerOffset)) {
        return;
    }

    std::array<void*, kLegacyMultiShipCount> active_tiles{};
    std::array<void*, kLegacyMultiShipCount> active_objects{};
    std::size_t active_count = 0;
    for (std::size_t index = 0; index < active_tiles.size(); ++index) {
        void* tile = checked_multi_ship_tile(ship_display, index);
        if (!tile || !readable_range(
                tile, kMultiShipWireframePointerOffset + sizeof(void*))) {
            continue;
        }
        void* selected_object = read_pointer_at(tile, 0x28);
        if (!selected_object) {
            continue;
        }
        active_tiles[index] = tile;
        active_objects[index] = selected_object;
        ++active_count;
    }
    if (active_count < 2) return;

    a1compat::NativeUiRectangle parent_rectangle{};
    const void* rectangle_source = static_cast<const std::uint8_t*>(
        ship_display) + kShipDisplayRectangleOffset;
    const bool has_parent_rectangle = readable_range(
        rectangle_source, sizeof(parent_rectangle));
    if (has_parent_rectangle) {
        std::memcpy(
            &parent_rectangle, rectangle_source, sizeof(parent_rectangle));
    }

    void* database = interface_sprite_database();
    void* sprite = nullptr;
    if (database &&
        g_legacy_ship_display_multi_crew_sprite_name[0] != '\0') {
        sprite = reinterpret_cast<void*>(a2fo_a1_call_thiscall_2(
            at(g_armada, kInterfaceSpriteDatabaseGetRva), database,
            reinterpret_cast<std::uintptr_t>(
                g_legacy_ship_display_multi_crew_sprite_name.data()),
            0));
    }
    std::size_t wireframe_count = 0;
    std::size_t crew_dot_count = 0;
    for (std::size_t index = 0; index < active_tiles.size(); ++index) {
        void* tile = active_tiles[index];
        if (!tile) continue;

        // Fleet Operations can replace the concrete MultiShipIcon vtable,
        // so follow its retained child pointer and call that child's own
        // verified render slot instead of requiring Armada's exact class.
        void* wireframe = read_pointer_at(
            tile, kMultiShipWireframePointerOffset);
        void* wireframe_vtable = read_pointer_at(wireframe, 0);
        void* wireframe_render = wireframe_vtable && readable_range(
                wireframe_vtable, 0x14)
            ? read_pointer_at(wireframe_vtable, 0x10)
            : nullptr;
        if (wireframe && wireframe_render &&
            is_executable_pointer(wireframe_render) &&
            write_component_rectangle(
                wireframe, kDisplayComponentRectangleOffset,
                g_legacy_ship_display_multi_wireframe_rects[index])) {
            void* object_slot = static_cast<std::uint8_t*>(wireframe) + 0x28;
            if (writable_range(object_slot, sizeof(void*))) {
                std::memcpy(
                    object_slot, &active_objects[index], sizeof(void*));
            }
            a2fo_a1_call_thiscall_0(wireframe_render, wireframe);
            ++wireframe_count;
        }

        if (has_parent_rectangle && sprite &&
            draw_legacy_gui_sprite_instance(
                sprite, parent_rectangle,
                g_legacy_ship_display_multi_crew_rects[index],
                legacy_multi_ship_crew_colour(
                    active_objects[index]))) {
            ++crew_dot_count;
        }
    }
    if (InterlockedCompareExchange(
            &g_legacy_ship_display_multi_crew_reported, 1, 0) == 0) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ShipDisplay multi-selection post-pass: "
            "%lu/%lu wireframes, %lu/%lu Crew dots from '%s'",
            static_cast<unsigned long>(wireframe_count),
            static_cast<unsigned long>(active_count),
            static_cast<unsigned long>(crew_dot_count),
            static_cast<unsigned long>(active_count),
            g_legacy_ship_display_multi_crew_sprite_name.data());
        log_line(message);
    }
}

void apply_legacy_ship_display_wireframe_layout(
    void* ship_display) noexcept {
    if (!ship_display || !g_armada ||
        !g_legacy_ship_display_render_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_ship_display_wireframe_available, 0, 0) == 0) {
        return;
    }

    constexpr std::array<std::size_t, 2> wireframe_pointer_offsets{{
        kShipDisplaySingleWireframePointerOffset,
        kShipDisplayBuildWireframePointerOffset}};
    std::size_t applied_count = 0;
    a1compat::NativeUiRectangle previous{};
    for (const std::size_t pointer_offset : wireframe_pointer_offsets) {
        void* wireframe = checked_ship_display_child(
            ship_display, pointer_offset, kWireframeIconVtableRva);
        if (!wireframe) continue;
        const void* rectangle_source = static_cast<const std::uint8_t*>(
            wireframe) + kDisplayComponentRectangleOffset;
        if (applied_count == 0 &&
            readable_range(rectangle_source, sizeof(previous))) {
            std::memcpy(&previous, rectangle_source, sizeof(previous));
        }
        if (write_component_rectangle(
                wireframe, kDisplayComponentRectangleOffset,
                g_legacy_ship_display_wireframe_rect)) {
            ++applied_count;
        }
    }
    if (applied_count == 0) return;

    if (InterlockedCompareExchange(
            &g_legacy_ship_display_wireframe_reported, 1, 0) == 0) {
        const a1compat::NativeUiRectangle& rectangle =
            g_legacy_ship_display_wireframe_rect;
        char message[384]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ShipDisplay wireframe layout active: prior local="
            "[%ld,%ld,%ld,%ld]; restored local=[%ld,%ld,%ld,%ld]; "
            "%lu/2 normal/build paths",
            static_cast<long>(previous.left),
            static_cast<long>(previous.top),
            static_cast<long>(previous.right),
            static_cast<long>(previous.bottom),
            static_cast<long>(rectangle.left),
            static_cast<long>(rectangle.top),
            static_cast<long>(rectangle.right),
            static_cast<long>(rectangle.bottom),
            static_cast<unsigned long>(applied_count));
        log_line(message);
    }
}

void apply_legacy_ship_display_race_icon_layout(
    void* ship_display) noexcept {
    if (!ship_display || !g_armada ||
        !g_legacy_ship_display_render_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_ship_display_race_icon_available, 0, 0) == 0) {
        return;
    }

    void* race_icon = checked_ship_display_child(
        ship_display, kShipDisplayRaceIconPointerOffset,
        kRaceIconVtableRva);
    if (!race_icon) return;
    const bool component_applied = write_component_rectangle(
        race_icon, kDisplayComponentRectangleOffset,
        g_legacy_ship_display_race_icon_rect);
    const bool display_applied = write_component_rectangle(
        race_icon, kRaceIconDisplayRectangleOffset,
        g_legacy_ship_display_race_icon_display_rect);
    if ((!component_applied || !display_applied) ||
        InterlockedCompareExchange(
            &g_legacy_ship_display_race_icon_reported, 1, 0) != 0) {
        return;
    }

    const a1compat::NativeUiRectangle& component =
        g_legacy_ship_display_race_icon_rect;
    const a1compat::NativeUiRectangle& display =
        g_legacy_ship_display_race_icon_display_rect;
    char message[384]{};
    std::snprintf(
        message, sizeof(message),
        "A1 ShipDisplay RaceIcon layout active: race bar local="
        "[%ld,%ld,%ld,%ld]; insignia local=[%ld,%ld,%ld,%ld]",
        static_cast<long>(component.left),
        static_cast<long>(component.top),
        static_cast<long>(component.right),
        static_cast<long>(component.bottom),
        static_cast<long>(display.left),
        static_cast<long>(display.top),
        static_cast<long>(display.right),
        static_cast<long>(display.bottom));
    log_line(message);
}

std::size_t apply_legacy_ship_text_layout(
    void* ship_display,
    const std::array<std::size_t, 2>& pointer_offsets,
    std::uintptr_t expected_vtable_rva,
    const a1compat::NativeUiRectangle& rectangle,
    std::size_t render_rectangle_offset) noexcept {
    std::size_t applied_count = 0;
    for (const std::size_t pointer_offset : pointer_offsets) {
        void* child = checked_ship_display_child(
            ship_display, pointer_offset, expected_vtable_rva);
        if (!child) continue;
        const bool component_applied = write_component_rectangle(
            child, kDisplayComponentRectangleOffset, rectangle);
        const bool render_applied = write_component_rectangle(
            child, render_rectangle_offset, rectangle);
        if (component_applied && render_applied) ++applied_count;
    }
    return applied_count;
}

a1compat::NativeUiRectangle native_ui_rectangle_union(
    const a1compat::NativeUiRectangle& first,
    const a1compat::NativeUiRectangle& second) noexcept {
    return a1compat::NativeUiRectangle{
        std::min(first.left, second.left),
        std::min(first.top, second.top),
        std::max(first.right, second.right),
        std::max(first.bottom, second.bottom)};
}

std::size_t apply_legacy_ship_crew_layout(
    void* ship_display, bool custom_identity_text) noexcept {
    const bool has_crew_dot =
        g_legacy_ship_display_crew_dot_rect.right >
            g_legacy_ship_display_crew_dot_rect.left &&
        g_legacy_ship_display_crew_dot_rect.bottom >
            g_legacy_ship_display_crew_dot_rect.top;
    const a1compat::NativeUiRectangle component_rectangle = has_crew_dot
        ? native_ui_rectangle_union(
              g_legacy_ship_display_crew_rect,
              g_legacy_ship_display_crew_dot_rect)
        : g_legacy_ship_display_crew_rect;
    const a1compat::NativeUiRectangle text_rectangle = custom_identity_text
        ? kHiddenLegacyResourceRectangle
        : g_legacy_ship_display_crew_rect;
    void* crew_dot_sprite = nullptr;
    if (has_crew_dot &&
        g_legacy_ship_display_crew_dot_sprite_name[0] != '\0') {
        void* database = interface_sprite_database();
        if (database) {
            crew_dot_sprite = reinterpret_cast<void*>(
                a2fo_a1_call_thiscall_2(
                    at(g_armada, kInterfaceSpriteDatabaseGetRva), database,
                    reinterpret_cast<std::uintptr_t>(
                        g_legacy_ship_display_crew_dot_sprite_name.data()),
                    0));
            if (!crew_dot_sprite || !readable_range(
                    static_cast<std::uint8_t*>(crew_dot_sprite) +
                        kSpriteFrameListOffset,
                    sizeof(void*)) ||
                !*reinterpret_cast<void**>(
                    static_cast<std::uint8_t*>(crew_dot_sprite) +
                        kSpriteFrameListOffset)) {
                crew_dot_sprite = nullptr;
            }
        }
    }
    std::size_t applied_count = 0;
    for (const std::size_t pointer_offset :
         kShipDisplayCrewPointerOffsets) {
        void* child = checked_ship_display_child(
            ship_display, pointer_offset, kShipCrewDisplayVtableRva);
        if (!child) continue;
        const bool component_applied = write_component_rectangle(
            child, kDisplayComponentRectangleOffset,
            component_rectangle);
        const bool text_applied = write_component_rectangle(
            child, kShipAmountRenderRectangleOffset,
            text_rectangle);
        const bool icon_applied = !has_crew_dot ||
            write_component_rectangle(
                child, kShipAmountIconRectangleOffset,
                g_legacy_ship_display_crew_dot_rect);
        bool sprite_applied = crew_dot_sprite == nullptr;
        if (crew_dot_sprite) {
            void* destination = static_cast<std::uint8_t*>(child) +
                kShipAmountSpritePointerOffset;
            if (writable_range(destination, sizeof(crew_dot_sprite))) {
                std::memcpy(
                    destination, &crew_dot_sprite,
                    sizeof(crew_dot_sprite));
                sprite_applied = true;
            }
        }
        if (component_applied && text_applied && icon_applied &&
            sprite_applied) {
            ++applied_count;
        }
    }
    return applied_count;
}

bool usable_native_ui_rectangle(
    const a1compat::NativeUiRectangle& rectangle) noexcept {
    return rectangle.right > rectangle.left &&
        rectangle.bottom > rectangle.top;
}

SpriteVector ship_text_component_colour(
    const void* text_component, const SpriteVector& fallback) noexcept {
    if (!text_component) return fallback;
    SpriteVector colour = fallback;
    const void* source = static_cast<const std::uint8_t*>(text_component) +
        kTextComponentColourOffset;
    if (readable_range(source, sizeof(colour))) {
        std::memcpy(&colour, source, sizeof(colour));
    }
    if (!std::isfinite(colour.x) || !std::isfinite(colour.y) ||
        !std::isfinite(colour.z)) {
        return fallback;
    }
    return colour;
}

bool draw_legacy_ship_text(
    const char* value, const a1compat::NativeUiRectangle& rectangle,
    const SpriteVector& colour, void* text_component,
    void* display_interface_override,
    std::int32_t flags_override) noexcept {
    if (!value || !*value || !text_component || !g_armada ||
        !g_legacy_ship_display_text_runtime_ready ||
        !usable_native_ui_rectangle(rectangle)) {
        return false;
    }
    void* display_interface = display_interface_override
        ? display_interface_override
        : read_pointer_at(
              text_component, kDisplayComponentParentPointerOffset);
    if (!display_interface) return false;
    void* display_override = nullptr;
    void* display_slot = read_pointer_at(
        text_component, kTextComponentDisplayOverrideSlotOffset);
    if (display_slot) display_override = read_pointer_at(display_slot, 0);

    const std::int32_t flags =
        flags_override == std::numeric_limits<std::int32_t>::min()
        ? read_int32_at(text_component, kTextComponentFlagsOffset, 9)
        : flags_override;
    std::uint8_t constrain = 0;
    const void* constrain_source =
        static_cast<const std::uint8_t*>(text_component) +
        kTextComponentConstrainOffset;
    if (readable_range(constrain_source, sizeof(constrain))) {
        std::memcpy(&constrain, constrain_source, sizeof(constrain));
    }
    void* font_state = static_cast<std::uint8_t*>(text_component) +
        kTextComponentFontStateOffset;
    if (!readable_range(font_state, 12)) return false;

    a2fo_a1_call_thiscall_7(
        at(g_armada, kDisplayInterfaceDrawTextInRectangleRva),
        display_interface,
        reinterpret_cast<std::uintptr_t>(value),
        reinterpret_cast<std::uintptr_t>(&rectangle),
        static_cast<std::uintptr_t>(flags),
        reinterpret_cast<std::uintptr_t>(&colour),
        reinterpret_cast<std::uintptr_t>(display_override),
        static_cast<std::uintptr_t>(constrain),
        reinterpret_cast<std::uintptr_t>(font_state));
    return true;
}

void localized_ship_label(
    const char* key, const char* fallback,
    char* output, std::size_t output_size) noexcept {
    if (!output || output_size == 0) return;
    output[0] = '\0';
    const char* resolved = nullptr;
    void* manager = g_armada
        ? read_pointer_at(at(g_armada, kLocalizationManagerPointerRva), 0)
        : nullptr;
    if (manager && key && *key) {
        resolved = reinterpret_cast<const char*>(
            a2fo_a1_call_thiscall_1(
                at(g_armada, kLocalizationLookupRva), manager,
                reinterpret_cast<std::uintptr_t>(key)));
        if (!resolved || IsBadStringPtrA(resolved, 512) || !*resolved ||
            (_stricmp(resolved, key) == 0 &&
             _strnicmp(key, "GUI_", 4) == 0)) {
            resolved = nullptr;
        }
    }
    std::snprintf(
        output, output_size, "%s",
        resolved ? resolved : (fallback ? fallback : ""));
}

void* preferred_ship_text_child(
    void* ship_display,
    const std::array<std::size_t, 2>& pointer_offsets,
    std::uintptr_t expected_vtable_rva) noexcept {
    // The second child is the build/station presentation seen in A1's tall
    // status panel. Both variants use the same raw-A1 geometry and provide a
    // safe fallback for ordinary craft if Fleet Operations omits one path.
    void* child = checked_ship_display_child(
        ship_display, pointer_offsets[1], expected_vtable_rva);
    return child ? child : checked_ship_display_child(
        ship_display, pointer_offsets[0], expected_vtable_rva);
}

void draw_legacy_ship_display_identity_text(void* ship_display) noexcept {
    if (!ship_display || !g_armada ||
        !g_legacy_ship_display_text_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_ship_display_identity_text_available, 0, 0) == 0) {
        return;
    }
    void* selected_object = read_pointer_at(
        ship_display, kShipDisplaySelectedObjectPointerOffset);
    void* object_class = read_pointer_at(
        selected_object, kGameObjectClassOffset);
    if (!selected_object || !is_plausible_object_class(object_class)) return;

    float current_crew_value = 0.0f;
    const void* current_crew_source =
        static_cast<const std::uint8_t*>(selected_object) +
        kSelectedObjectCurrentCrewOffset;
    if (!readable_range(current_crew_source, sizeof(current_crew_value))) {
        return;
    }
    std::memcpy(
        &current_crew_value, current_crew_source,
        sizeof(current_crew_value));
    if (!std::isfinite(current_crew_value)) return;
    const std::int32_t current_crew = static_cast<std::int32_t>(
        std::lround(std::clamp(
            static_cast<double>(current_crew_value), 0.0,
            static_cast<double>(std::numeric_limits<std::int32_t>::max()))));
    float maximum_crew_value = 0.0f;
    const void* maximum_crew_source =
        static_cast<const std::uint8_t*>(selected_object) +
        kSelectedObjectMaximumCrewOffset;
    if (readable_range(maximum_crew_source, sizeof(maximum_crew_value))) {
        std::memcpy(
            &maximum_crew_value, maximum_crew_source,
            sizeof(maximum_crew_value));
    }
    std::int32_t maximum_crew = 0;
    if (std::isfinite(maximum_crew_value) && maximum_crew_value > 0.0f) {
        maximum_crew = static_cast<std::int32_t>(std::lround(std::clamp(
            static_cast<double>(maximum_crew_value), 0.0,
            static_cast<double>(
                std::numeric_limits<std::int32_t>::max()))));
    } else {
        maximum_crew = std::max<std::int32_t>(
            0, read_int32_at(
                object_class, kObjectClassInitialCrewOffset, 0));
    }
    const std::int32_t officers = std::max<std::int32_t>(
        0, read_int32_at(
            object_class, kObjectClassOfficerCountOffset, 0));

    void* crew_text = preferred_ship_text_child(
        ship_display, kShipDisplayCrewPointerOffsets,
        kShipCrewDisplayVtableRva);
    void* officer_text = preferred_ship_text_child(
        ship_display, kShipDisplayOfficerPointerOffsets,
        kShipOfficerDisplayVtableRva);
    if (!crew_text || !officer_text) return;
    if (!g_legacy_caption_text_component) {
        g_legacy_caption_text_component = crew_text;
    }

    char crew_label[128]{};
    char officer_label[128]{};
    char crew_amount[64]{};
    char officer_amount[32]{};
    localized_ship_label(
        g_legacy_ship_display_crew_label_key.data(), "CREW",
        crew_label, sizeof(crew_label));
    localized_ship_label(
        g_legacy_ship_display_officer_label_key.data(), "OFFICERS",
        officer_label, sizeof(officer_label));
    if (maximum_crew > 0) {
        std::snprintf(
            crew_amount, sizeof(crew_amount), "%ld/%ld",
            static_cast<long>(current_crew),
            static_cast<long>(maximum_crew));
    } else {
        std::snprintf(
            crew_amount, sizeof(crew_amount), "%ld",
            static_cast<long>(current_crew));
    }
    std::snprintf(
        officer_amount, sizeof(officer_amount), "%ld",
        static_cast<long>(officers));

    const SpriteVector crew_label_colour = ship_text_component_colour(
        crew_text, SpriteVector{0.0f, 0.0f, 0.0f});
    const SpriteVector officer_label_colour = ship_text_component_colour(
        officer_text, SpriteVector{0.0f, 0.0f, 0.0f});
    // A2's inherited CrewNumText/Officer text colour is black because its
    // amount sits on a light bar. In the raw A1 status panel the amount is to
    // the right of that bar on a dark field, where the inherited colour makes
    // valid values look absent. A1 renders both live amounts in status green.
    const SpriteVector crew_colour{0.0f, 1.0f, 0.0f};
    const SpriteVector officer_colour{0.0f, 1.0f, 0.0f};

    const bool crew_label_drawn = draw_legacy_ship_text(
        crew_label, g_legacy_ship_display_crew_label_rect,
        crew_label_colour, crew_text, nullptr, 10);
    const bool officer_label_drawn = draw_legacy_ship_text(
        officer_label, g_legacy_ship_display_officer_label_rect,
        officer_label_colour, officer_text, nullptr, 10);
    const bool crew_amount_drawn = draw_legacy_ship_text(
        crew_amount, g_legacy_ship_display_crew_rect,
        crew_colour, crew_text);
    const bool officer_amount_drawn = draw_legacy_ship_text(
        officer_amount, g_legacy_ship_display_officer_rect,
        officer_colour, officer_text);
    if (crew_label_drawn && officer_label_drawn && crew_amount_drawn &&
        officer_amount_drawn && InterlockedCompareExchange(
            &g_legacy_ship_display_identity_text_reported, 1, 0) == 0) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ShipDisplay identity text active: crew=%ld/%ld "
            "officers=%ld; localized labels restored",
            static_cast<long>(current_crew),
            static_cast<long>(maximum_crew),
            static_cast<long>(officers));
        log_line(message);
    }
}

void apply_legacy_ship_display_identity_layout(
    void* ship_display) noexcept {
    if (!ship_display || !g_armada ||
        !g_legacy_ship_display_render_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_ship_display_identity_available, 0, 0) == 0) {
        return;
    }

    const std::size_t class_count = apply_legacy_ship_text_layout(
        ship_display, kShipDisplayClassPointerOffsets,
        kShipClassTextVtableRva, g_legacy_ship_display_class_rect,
        kShipTextRenderRectangleOffset);
    const std::size_t name_count = apply_legacy_ship_text_layout(
        ship_display, kShipDisplayNamePointerOffsets,
        kShipNameTextVtableRva, g_legacy_ship_display_name_rect,
        kShipTextRenderRectangleOffset);
    const bool custom_identity_text =
        g_legacy_ship_display_text_runtime_ready &&
        InterlockedCompareExchange(
            &g_legacy_ship_display_identity_text_available, 0, 0) != 0;
    const std::size_t crew_count = apply_legacy_ship_crew_layout(
        ship_display, custom_identity_text);
    const std::size_t officer_count = apply_legacy_ship_text_layout(
        ship_display, kShipDisplayOfficerPointerOffsets,
        kShipOfficerDisplayVtableRva,
        custom_identity_text
            ? kHiddenLegacyResourceRectangle
            : g_legacy_ship_display_officer_rect,
        kShipAmountRenderRectangleOffset);
    if ((class_count + name_count + crew_count + officer_count) > 0 &&
        InterlockedCompareExchange(
            &g_legacy_ship_display_identity_reported, 1, 0) == 0) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ShipDisplay identity layout active: normal/build "
            "class=%lu/2 name=%lu/2 crew=%lu/2 officers=%lu/2",
            static_cast<unsigned long>(class_count),
            static_cast<unsigned long>(name_count),
            static_cast<unsigned long>(crew_count),
            static_cast<unsigned long>(officer_count));
        log_line(message);
    }
}

void apply_legacy_ship_display_system_strip(void* ship_display) noexcept {
    if (!ship_display || !g_armada ||
        !g_legacy_ship_display_render_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0) {
        return;
    }

    std::size_t hidden_count = 0;
    for (const std::size_t base_offset :
         kShipDisplaySystemValuePointerOffsets) {
        for (std::size_t index = 0;
             index < kShipDisplaySystemValueCount; ++index) {
            void* system_value = checked_ship_display_child(
                ship_display, base_offset + index * sizeof(void*),
                kSystemValueVtableRva);
            if (!system_value) continue;
            const bool component_hidden = write_component_rectangle(
                system_value, kDisplayComponentRectangleOffset,
                kHiddenLegacyResourceRectangle);
            const bool render_hidden = write_component_rectangle(
                system_value, kSystemValueRenderRectangleOffset,
                kHiddenLegacyResourceRectangle);
            if (component_hidden && render_hidden) {
                ++hidden_count;
            }
        }
    }

    if (hidden_count > 0 && InterlockedCompareExchange(
            &g_legacy_ship_display_system_strip_reported, 1, 0) == 0) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ShipDisplay system strip active: %lu/%lu redundant A2 "
            "normal/build numeric values hidden; native A1 icons retained",
            static_cast<unsigned long>(hidden_count),
            static_cast<unsigned long>(
                kShipDisplaySystemValueCount *
                kShipDisplaySystemValuePointerOffsets.size()));
        log_line(message);
    }
}

void apply_legacy_ship_display_progress_layout(
    void* ship_display) noexcept {
    if (!ship_display || !g_armada ||
        !g_legacy_ship_display_render_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_ship_display_progress_available, 0, 0) == 0) {
        return;
    }
    void* construction_bar = checked_ship_display_child(
        ship_display, kShipDisplayConstructionBarPointerOffset,
        kConstructionBarVtableRva);
    void* energy_text = checked_ship_display_child(
        ship_display, kShipDisplayEnergyPointerOffset,
        kEnergyTextVtableRva);
    const bool construction_applied = construction_bar &&
        write_component_rectangle(
            construction_bar, kDisplayComponentRectangleOffset,
            g_legacy_ship_display_progress_rect);
    const bool energy_applied = energy_text &&
        write_component_rectangle(
            energy_text, kDisplayComponentRectangleOffset,
            g_legacy_ship_display_progress_rect);
    if (!construction_applied && !energy_applied) return;
    if (InterlockedCompareExchange(
            &g_legacy_ship_display_progress_reported, 1, 0) == 0) {
        const a1compat::NativeUiRectangle& rectangle =
            g_legacy_ship_display_progress_rect;
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ShipDisplay construction progress active: native "
            "ConstructionBar=%s EnergyText=%s restored to "
            "local=[%ld,%ld,%ld,%ld]",
            construction_applied ? "yes" : "no",
            energy_applied ? "yes" : "no",
            static_cast<long>(rectangle.left),
            static_cast<long>(rectangle.top),
            static_cast<long>(rectangle.right),
            static_cast<long>(rectangle.bottom));
        log_line(message);
    }
}

bool bind_legacy_construction_bar_target(
    void* construction_bar, void* preferred_rig = nullptr) noexcept {
    if (!construction_bar || !g_armada ||
        !g_legacy_construction_bar_target_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0) {
        return false;
    }

    void* construction_rig = preferred_rig;
    void* rig_vtable = read_pointer_at(construction_rig, 0);
    void* rig_class = read_pointer_at(
        construction_rig, kGameObjectClassOffset);
    bool exact_construction_rig = construction_rig &&
        rig_vtable == at(g_armada, kConstructionRigVtableRva);
    if (!construction_rig ||
        (!exact_construction_rig &&
         (!is_plausible_object_class(rig_class) ||
          !likely_constructionrig_class(rig_class)))) {
        construction_rig = read_pointer_at(
            construction_bar, kConstructionBarObjectPointerOffset);
        rig_vtable = read_pointer_at(construction_rig, 0);
        rig_class = read_pointer_at(
            construction_rig, kGameObjectClassOffset);
        exact_construction_rig = construction_rig &&
            rig_vtable == at(g_armada, kConstructionRigVtableRva);
    }
    if (!construction_rig ||
        (!exact_construction_rig &&
         (!is_plausible_object_class(rig_class) ||
          !likely_constructionrig_class(rig_class)))) {
        return false;
    }

    const std::uint32_t construction_object_id =
        static_cast<std::uint32_t>(read_int32_at(
            construction_rig,
            kConstructionRigConstructionObjectIdOffset, 0));
    // ConstructionRig uses Armada's zero-valued null object ID. Live object
    // IDs are not required to have the high bit set.
    void* construction_target = exact_construction_rig
        ? reinterpret_cast<void*>(a2fo_a1_call_thiscall_0(
              at(g_armada, kConstructionRigGetConstructionObjectRva),
              construction_rig))
        : nullptr;

    // Retain a direct lookup fallback for a compatible ConstructionRig whose
    // vtable has been wrapped by another module but whose native ID tail is
    // still authoritative.
    if (!construction_target && construction_object_id != 0) {
        using FindGameObjectByIdFn = void* (__cdecl*)(std::uint32_t);
        const auto find_game_object = reinterpret_cast<FindGameObjectByIdFn>(
            at(g_armada, kFindGameObjectByIdRva));
        construction_target = find_game_object(construction_object_id);
    }
    if (InterlockedCompareExchange(
            &g_legacy_construction_rig_progress_diagnostic_reported,
            1, 0) == 0) {
        char rig_name[96]{};
        copy_object_class_odf_name(rig_class, rig_name, sizeof(rig_name));
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ConstructionRig progress probe: rig='%s' exactVtable=%s "
            "id=0x%08lx target=%p",
            rig_name[0] ? rig_name : "<unavailable>",
            exact_construction_rig ? "yes" : "no",
            static_cast<unsigned long>(construction_object_id),
            construction_target);
        log_line(message);
    }
    void* target_class = read_pointer_at(
        construction_target, kGameObjectClassOffset);
    if (!construction_target || construction_target == construction_rig ||
        !is_plausible_object_class(target_class)) {
        return false;
    }

    void* destination = static_cast<std::uint8_t*>(construction_bar) +
        kConstructionBarObjectPointerOffset;
    if (!writable_range(destination, sizeof(construction_target))) {
        return false;
    }
    std::memcpy(destination, &construction_target, sizeof(construction_target));

    if (InterlockedCompareExchange(
            &g_legacy_construction_bar_target_reported, 1, 0) == 0) {
        char rig_name[96]{};
        char target_name[96]{};
        copy_object_class_odf_name(
            rig_class, rig_name, sizeof(rig_name));
        copy_object_class_odf_name(
            target_class, target_name, sizeof(target_name));
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ConstructionBar target bridge active: rig='%s' target='%s' "
            "id=0x%08lx",
            rig_name[0] ? rig_name : "<unavailable>",
            target_name[0] ? target_name : "<unavailable>",
            static_cast<unsigned long>(construction_object_id));
        log_line(message);
    }
    return true;
}

void __attribute__((fastcall)) construction_bar_render_hook(
    void* construction_bar, void*) noexcept {
    bind_legacy_construction_bar_target(construction_bar);
    if (g_construction_bar_render_hook.gateway) {
        a2fo_a1_call_thiscall_0(
            g_construction_bar_render_hook.gateway, construction_bar);
    }
}

void apply_legacy_ship_display_build_queue_layout(
    void* ship_display) noexcept {
    if (!ship_display || !g_armada ||
        !g_legacy_ship_display_render_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_speed_control_layout_available, 0, 0) == 0) {
        return;
    }

    void* ship_display_vtable = nullptr;
    if (!readable_range(ship_display, sizeof(ship_display_vtable))) return;
    std::memcpy(
        &ship_display_vtable, ship_display, sizeof(ship_display_vtable));
    if (ship_display_vtable != at(g_armada, kShipDisplayVtableRva)) return;

    a1compat::NativeUiRectangle ship_display_rectangle{};
    const void* ship_display_rectangle_source =
        static_cast<const std::uint8_t*>(ship_display) +
        kShipDisplayRectangleOffset;
    if (!readable_range(
            ship_display_rectangle_source,
            sizeof(ship_display_rectangle))) {
        return;
    }
    std::memcpy(
        &ship_display_rectangle, ship_display_rectangle_source,
        sizeof(ship_display_rectangle));

    std::size_t placed_count = 0;
    std::size_t hidden_count = 0;
    for (std::size_t index = 0;
         index < kShipDisplayBuildQueueCount; ++index) {
        void* queue_icon = checked_ship_display_child(
            ship_display,
            kShipDisplayBuildQueuePointerOffset + index * sizeof(void*),
            kBuildQueueIconVtableRva);
        if (!queue_icon) continue;

        if (index < g_legacy_speed_queue_screen_rects.size()) {
            const a1compat::NativeUiRectangle& screen_rectangle =
                g_legacy_speed_queue_screen_rects[index];
            const a1compat::NativeUiRectangle local_rectangle{
                screen_rectangle.left - ship_display_rectangle.left,
                screen_rectangle.top - ship_display_rectangle.top,
                screen_rectangle.right - ship_display_rectangle.left,
                screen_rectangle.bottom - ship_display_rectangle.top};
            if (write_component_rectangle(
                    queue_icon, kDisplayComponentRectangleOffset,
                    local_rectangle)) {
                ++placed_count;
            }
        } else if (write_component_rectangle(
                       queue_icon, kDisplayComponentRectangleOffset,
                       kHiddenLegacyResourceRectangle)) {
            ++hidden_count;
        }
    }

    if (placed_count > 0 && InterlockedCompareExchange(
            &g_legacy_speed_queue_layout_reported, 1, 0) == 0) {
        const a1compat::NativeUiRectangle& first =
            g_legacy_speed_queue_screen_rects.front();
        const a1compat::NativeUiRectangle& last =
            g_legacy_speed_queue_screen_rects.back();
        char message[384]{};
        std::snprintf(
            message, sizeof(message),
            "A1 SpeedRail producer queue active: %lu/5 native slots "
            "placed at screen=[%ld,%ld]-[%ld,%ld]; %lu/5 A2-only "
            "slots hidden",
            static_cast<unsigned long>(placed_count),
            static_cast<long>(first.left),
            static_cast<long>(first.top),
            static_cast<long>(last.right),
            static_cast<long>(last.bottom),
            static_cast<unsigned long>(hidden_count));
        log_line(message);
    }
}

void* checked_ship_display_background_parent(void* background) noexcept {
    if (!background || !g_armada) return nullptr;
    void* ship_display = read_pointer_at(
        background, kDisplayComponentParentPointerOffset);
    if (!ship_display ||
        read_pointer_at(ship_display, 0) !=
            at(g_armada, kShipDisplayVtableRva)) {
        return nullptr;
    }
    return ship_display;
}

void draw_legacy_ship_display_opaque_black_mask(
    void* background) noexcept {
    if (!g_legacy_ship_display_opaque_black_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_ship_display_black_mask_available, 0, 0) == 0) {
        return;
    }
    void* ship_display = checked_ship_display_background_parent(background);
    if (!ship_display) return;
    a2fo_a1_call_thiscall_3(
        at(g_armada, kDisplayInterfaceDrawRectangleRva), ship_display,
        reinterpret_cast<std::uintptr_t>(
            &g_legacy_ship_display_black_rect),
        reinterpret_cast<std::uintptr_t>(
            at(g_armada, kInterfaceBlackColourRva)),
        kInterfaceRectangleOpaque);
}

void draw_legacy_ship_display_identity_background(
    void* background) noexcept {
    if (!background || !g_armada || !g_fleet_ops ||
        !g_legacy_ship_display_background_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_ship_display_identity_background_available,
            0, 0) == 0) {
        return;
    }

    void* ship_display = checked_ship_display_background_parent(background);
    if (!ship_display) return;

    void* selected_object = nullptr;
    const void* selected_source = static_cast<const std::uint8_t*>(
        ship_display) + kShipDisplaySelectedObjectPointerOffset;
    if (!readable_range(selected_source, sizeof(selected_object))) return;
    std::memcpy(&selected_object, selected_source, sizeof(selected_object));
    if (!selected_object) return;

    a1compat::NativeUiRectangle parent_rectangle{};
    const void* rectangle_source = static_cast<const std::uint8_t*>(
        ship_display) + kDisplayComponentParentPointerOffset;
    if (!readable_range(rectangle_source, sizeof(parent_rectangle))) return;
    std::memcpy(
        &parent_rectangle, rectangle_source, sizeof(parent_rectangle));

    void* database = interface_sprite_database();
    if (!database) return;
    const std::size_t piece_count = std::min(
        g_legacy_ship_display_identity_background_piece_count,
        g_legacy_ship_display_identity_background_pieces.size());
    for (std::size_t index = 0; index < piece_count; ++index) {
        const LegacyBackgroundPiece& piece =
            g_legacy_ship_display_identity_background_pieces[index];
        draw_legacy_gui_sprite(
            database, piece.sprite_name.data(), parent_rectangle,
            piece.rectangle);
    }
    if (piece_count > 0 && InterlockedCompareExchange(
            &g_legacy_ship_display_identity_background_reported,
            1, 0) == 0) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ShipDisplay identity artwork active: %lu background/label "
            "pieces rendered for single selections",
            static_cast<unsigned long>(piece_count));
        log_line(message);
    }
}

void draw_legacy_ship_display_race_icon_background(
    void* background) noexcept {
    if (!background || !g_armada || !g_fleet_ops ||
        !g_legacy_ship_display_race_icon_background_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_legacy_ship_display_race_icon_available, 0, 0) == 0) {
        return;
    }

    void* ship_display = checked_ship_display_background_parent(background);
    if (!ship_display) return;
    void* selected_object = read_pointer_at(
        ship_display, kShipDisplaySelectedObjectPointerOffset);
    if (!selected_object || !readable_range(
            static_cast<const std::uint8_t*>(selected_object) +
                kGameObjectTeamColourPointerOffset,
            sizeof(void*))) {
        return;
    }
    void* race_icon = checked_ship_display_child(
        ship_display, kShipDisplayRaceIconPointerOffset,
        kRaceIconVtableRva);
    if (!race_icon) return;
    void* sprite = read_pointer_at(
        race_icon, kRaceIconBackgroundSpritePointerOffset);
    const auto* team_colour = reinterpret_cast<const SpriteVector*>(
        a2fo_a1_call_thiscall_0(
            at(g_armada, kGameObjectTeamColourRva), selected_object));
    if (!team_colour || !readable_range(
            team_colour, sizeof(SpriteVector))) {
        return;
    }

    a1compat::NativeUiRectangle parent_rectangle{};
    const void* rectangle_source = static_cast<const std::uint8_t*>(
        ship_display) + kShipDisplayRectangleOffset;
    if (!readable_range(rectangle_source, sizeof(parent_rectangle))) return;
    std::memcpy(
        &parent_rectangle, rectangle_source, sizeof(parent_rectangle));
    SpriteVector colour{};
    std::memcpy(&colour, team_colour, sizeof(colour));
    if (!draw_legacy_gui_sprite_instance(
            sprite, parent_rectangle,
            g_legacy_ship_display_race_icon_rect, colour)) {
        return;
    }

    if (InterlockedCompareExchange(
            &g_legacy_ship_display_race_icon_background_reported,
            1, 0) == 0) {
        const a1compat::NativeUiRectangle& rectangle =
            g_legacy_ship_display_race_icon_rect;
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ShipDisplay team-colour race_icon_bar active: local="
            "[%ld,%ld,%ld,%ld]",
            static_cast<long>(rectangle.left),
            static_cast<long>(rectangle.top),
            static_cast<long>(rectangle.right),
            static_cast<long>(rectangle.bottom));
        log_line(message);
    }
}

void __attribute__((fastcall)) standard_background_render_hook(
    void* background, void*) noexcept {
    // Fleet Operations' ShipDisplay has already drawn its native 0.5-opacity
    // mask by this boundary. Replace it with A1's opaque fill before the
    // StandardBackground frame and the remaining children are rendered.
    draw_legacy_ship_display_opaque_black_mask(background);
    if (g_standard_background_render_hook.gateway) {
        a2fo_a1_call_thiscall_0(
            g_standard_background_render_hook.gateway, background);
    }
    draw_legacy_ship_display_identity_background(background);
    draw_legacy_ship_display_race_icon_background(background);
}

void __attribute__((fastcall)) ship_display_render_hook(
    void* ship_display, void*) noexcept {
    repair_legacy_tactical_cursor_pointers();
    g_legacy_selected_object = read_pointer_at(
        ship_display, kShipDisplaySelectedObjectPointerOffset);
    // ShipDisplay owns the queue children, so A1's removed SpeedRail shell
    // must be drawn before its native renderer. Drawing the shell from the
    // later PopupPalette pass covered every correctly positioned queue icon.
    draw_legacy_speed_background();
    apply_legacy_ship_display_black_mask(ship_display);
    apply_legacy_ship_display_multi_layout(ship_display);
    apply_legacy_ship_display_wireframe_layout(ship_display);
    apply_legacy_ship_display_race_icon_layout(ship_display);
    apply_legacy_ship_display_identity_layout(ship_display);
    apply_legacy_ship_display_system_strip(ship_display);
    apply_legacy_ship_display_progress_layout(ship_display);
    apply_legacy_ship_display_build_queue_layout(ship_display);
    if (g_ship_display_render_hook.gateway) {
        a2fo_a1_call_thiscall_0(
            g_ship_display_render_hook.gateway, ship_display);
    }
    draw_legacy_ship_display_energy_bars(ship_display);
    draw_legacy_ship_display_multi_crew_dots(ship_display);
    draw_legacy_ship_display_identity_text(ship_display);
}

bool interface_cursor_in_rectangle(
    const a1compat::NativeUiRectangle& rectangle) noexcept {
    if (!g_armada || rectangle.right < rectangle.left ||
        rectangle.bottom < rectangle.top) {
        return false;
    }
    const std::int32_t cursor_x = read_int32_at(
        at(g_armada, kInterfaceCursorXRva), 0,
        std::numeric_limits<std::int32_t>::min());
    const std::int32_t cursor_y = read_int32_at(
        at(g_armada, kInterfaceCursorYRva), 0,
        std::numeric_limits<std::int32_t>::min());
    return cursor_x >= rectangle.left && cursor_x <= rectangle.right &&
        cursor_y >= rectangle.top && cursor_y <= rectangle.bottom;
}

bool __attribute__((fastcall)) ship_display_cursor_over_hook(
    void* ship_display, void*) noexcept {
    apply_legacy_ship_display_build_queue_layout(ship_display);
    if (g_ship_display_cursor_over_hook.gateway &&
        (a2fo_a1_call_thiscall_0(
             g_ship_display_cursor_over_hook.gateway,
             ship_display) & 0xffu) != 0) {
        return true;
    }
    const bool over_speed_rail =
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) != 0 &&
        InterlockedCompareExchange(
            &g_legacy_speed_control_layout_available, 0, 0) != 0 &&
        interface_cursor_in_rectangle(g_legacy_speed_panel_rect);
    if (over_speed_rail && InterlockedCompareExchange(
            &g_legacy_speed_cursor_reported, 1, 0) == 0) {
        log_line("A1 SpeedRail input boundary active; rail clicks are "
                 "owned by ShipDisplay");
    }
    return over_speed_rail;
}

bool control_button_ai_command(
    void* control_button, std::int32_t* output,
    void** resolved_action = nullptr) noexcept {
    if (!control_button || !output) return false;
    // ControlButton owns mutually exclusive native bindings: a direct Action
    // at +0x88, or ModeInfo at +0x84. Fleet Operations binds ordinary
    // Transport directly for some selections and through type-2 ModeInfo for
    // others, so accept both without interpreting either as a callback.
    void* action = read_pointer_at(
        control_button, kControlButtonDirectActionPointerOffset);
    if (!action) {
        void* mode_info = read_pointer_at(
            control_button, kControlButtonModeInfoPointerOffset);
        if (!mode_info) return false;
        const std::uint32_t mode_info_type = static_cast<std::uint32_t>(
            read_int32_at(mode_info, kModeInfoTypeOffset, -1));
        if (mode_info_type != kActionModeInfoType) return false;
        action = read_pointer_at(
            mode_info, kModeInfoActionPointerOffset);
    }
    if (!action) return false;
    std::int32_t ai_command = 0;
    const void* command_source = static_cast<const std::uint8_t*>(action) +
        kActionAiCommandOffset;
    if (!readable_range(command_source, sizeof(ai_command))) return false;
    std::memcpy(&ai_command, command_source, sizeof(ai_command));
    *output = ai_command;
    if (resolved_action) *resolved_action = action;
    return true;
}

bool is_native_transport_control_button(
    void* popup, void* control_button) noexcept {
    if (!popup || !control_button) return false;
    std::int32_t ai_command = -1;
    return control_button_ai_command(control_button, &ai_command) &&
        (ai_command == kTransportAiCommand ||
         ai_command == kTransportSpecialAiCommand);
}

bool is_native_speed_special_control_button(
    void* popup, void* control_button) noexcept {
    if (!popup || !control_button) return false;
    std::int32_t ai_command = -1;
    if (!control_button_ai_command(control_button, &ai_command) ||
        ai_command == kTransportAiCommand ||
        ai_command == kTransportSpecialAiCommand) {
        return false;
    }

    // Fleet Operations binds contextual special weapons (the construction
    // ship's Tractor Beam is one example) through type-2 Action ModeInfo even
    // when that Action's generic AiCommand is not SPECIAL_ATTACK. Ordinary
    // command-panel controls use type-3 ModeInfo or direct Actions. Accept the
    // native type-2 binding, plus the explicit direct SPECIAL_ATTACK command.
    void* mode_info = read_pointer_at(
        control_button, kControlButtonModeInfoPointerOffset);
    if (mode_info && static_cast<std::uint32_t>(read_int32_at(
            mode_info, kModeInfoTypeOffset, -1)) == kActionModeInfoType) {
        return true;
    }
    return ai_command == kSpecialAttackAiCommand;
}

bool is_native_speed_control_button(
    void* popup, void* control_button) noexcept {
    return is_native_transport_control_button(popup, control_button) ||
        is_native_speed_special_control_button(popup, control_button);
}

std::int32_t native_popup_button_slot(
    void* popup, void* control_button) noexcept {
    if (!popup || !control_button) return -1;
    for (std::size_t index = 0;
         index < kPopupPaletteNativeButtonCount; ++index) {
        if (read_pointer_at(
                popup,
                kPopupPaletteNativeButtonPointerOffset +
                    index * sizeof(void*)) == control_button) {
            return static_cast<std::int32_t>(index);
        }
    }
    return -1;
}

void report_legacy_popup_controls(
    void* popup, void* const* popup_buttons) noexcept {
    if (!popup || !popup_buttons) return;
    std::size_t non_null = 0;
    std::size_t bound = 0;
    for (std::size_t index = 0; index < kFoPopupButtonCapacity; ++index) {
        void* control_button = popup_buttons[index];
        if (!control_button) continue;
        ++non_null;
        if (read_pointer_at(
                control_button, kControlButtonModeInfoPointerOffset) ||
            read_pointer_at(
                control_button, kControlButtonDirectActionPointerOffset)) {
            ++bound;
        }
    }
    // Fleet Operations allocates all 64 controls before it binds the selected
    // object's palette. Do not consume the one-shot report on that empty
    // startup frame.
    if (non_null == 0 || bound == 0 || InterlockedCompareExchange(
            &g_legacy_popup_control_diagnostic_reported, 1, 0) != 0) {
        return;
    }

    char header[192]{};
    std::snprintf(
        header, sizeof(header),
        "A1 ControlPanel live-control diagnostic: %lu/%lu populated, "
        "%lu bound; "
        "Transport AiCommands=%ld/%ld",
        static_cast<unsigned long>(non_null),
        static_cast<unsigned long>(kFoPopupButtonCapacity),
        static_cast<unsigned long>(bound),
        static_cast<long>(kTransportAiCommand),
        static_cast<long>(kTransportSpecialAiCommand));
    log_line(header);

    std::size_t reported = 0;
    constexpr std::size_t kMaximumReportedControls = 32;
    for (std::size_t index = 0;
         index < kFoPopupButtonCapacity &&
         reported < kMaximumReportedControls; ++index) {
        void* control_button = popup_buttons[index];
        if (!control_button) continue;

        a1compat::NativeUiRectangle rectangle{};
        const void* rectangle_source =
            static_cast<const std::uint8_t*>(control_button) +
            kControlButtonRectangleOffset;
        if (readable_range(rectangle_source, sizeof(rectangle))) {
            std::memcpy(&rectangle, rectangle_source, sizeof(rectangle));
        }
        void* mode_info = read_pointer_at(
            control_button, kControlButtonModeInfoPointerOffset);
        void* direct_action = read_pointer_at(
            control_button, kControlButtonDirectActionPointerOffset);
        if (!mode_info && !direct_action) continue;
        const std::int32_t mode_type = mode_info
            ? read_int32_at(mode_info, kModeInfoTypeOffset, -1)
            : -1;
        void* mode_target = mode_info
            ? read_pointer_at(mode_info, kModeInfoTargetClassOffset)
            : nullptr;
        void* mode_action = mode_info
            ? read_pointer_at(mode_info, kModeInfoActionPointerOffset)
            : nullptr;
        const std::int32_t mode_action_index = mode_info
            ? read_int32_at(mode_info, kModeInfoActionIndexOffset, -1)
            : -1;
        std::int32_t ai_command = -1;
        void* resolved_action = nullptr;
        const bool has_ai_command = control_button_ai_command(
            control_button, &ai_command, &resolved_action);
        const bool officer_target = mode_type == 1 && mode_target &&
            is_officer_upgrade_class(mode_target);
        char message[640]{};
        std::snprintf(
            message, sizeof(message),
            "A1 control[%lu]: button=%p nativeSlot=%ld state=%ld "
            "rect=[%ld,%ld,%ld,%ld] mode=%p type=%ld target=%p "
            "modeAction=%p actionIndex=%ld directAction=%p "
            "resolvedAction=%p aiCommand=%ld officerTarget=%s",
            static_cast<unsigned long>(index), control_button,
            static_cast<long>(native_popup_button_slot(
                popup, control_button)),
            static_cast<long>(read_int32_at(
                control_button, kControlButtonStateOffset, -1)),
            static_cast<long>(rectangle.left),
            static_cast<long>(rectangle.top),
            static_cast<long>(rectangle.right),
            static_cast<long>(rectangle.bottom),
            mode_info, static_cast<long>(mode_type), mode_target,
            mode_action, static_cast<long>(mode_action_index),
            direct_action, resolved_action,
            static_cast<long>(has_ai_command ? ai_command : -1),
            officer_target ? "yes" : "no");
        log_line(message);
        ++reported;
    }
}

bool popup_control_is_bound(void* control_button) noexcept {
    return control_button &&
        (read_pointer_at(
             control_button, kControlButtonModeInfoPointerOffset) ||
         read_pointer_at(
             control_button, kControlButtonDirectActionPointerOffset));
}

void clear_legacy_officer_root_bindings(
    void* const* popup_buttons) noexcept {
    if (g_armada && g_legacy_officer_root_control_runtime_ready) {
        if (popup_buttons) {
            for (std::size_t index = 0;
                 index < kFoPopupButtonCapacity; ++index) {
                void* button = popup_buttons[index];
                if (read_pointer_at(
                        button, kControlButtonModeInfoPointerOffset) !=
                    g_legacy_officer_root_mode_info.data()) {
                    continue;
                }
                a2fo_a1_call_thiscall_0(
                    at(g_armada, kControlButtonClearRva), button);
            }
        } else if (g_legacy_officer_root_button && read_pointer_at(
                       g_legacy_officer_root_button,
                       kControlButtonModeInfoPointerOffset) ==
                       g_legacy_officer_root_mode_info.data()) {
            a2fo_a1_call_thiscall_0(
                at(g_armada, kControlButtonClearRva),
                g_legacy_officer_root_button);
        }
    }
    g_legacy_officer_root_button = nullptr;
    g_legacy_officer_root_target = nullptr;
    g_legacy_officer_root_enabled = false;
}

void* selected_legacy_officer_upgrade_target(
    bool* enabled) noexcept {
    if (enabled) *enabled = false;
    void* selected_object = g_legacy_selected_object;
    StarbaseClassPolicy policy{};
    void* producer_class = nullptr;
    if (!selected_object ||
        !starbase_policy(selected_object, &policy, &producer_class) ||
        !producer_class) {
        return nullptr;
    }
    void** items = reinterpret_cast<void**>(read_pointer_at(
        producer_class, kProducerClassBuildItemsOffset));
    if (!items || !readable_range(
            items, kProducerClassBuildItemCapacity * sizeof(void*))) {
        return nullptr;
    }
    void* target = nullptr;
    for (std::size_t slot = 0;
         slot < kProducerClassBuildItemCapacity; ++slot) {
        void* candidate = nullptr;
        std::memcpy(&candidate, items + slot, sizeof(candidate));
        if (!candidate || !is_officer_upgrade_class(candidate) ||
            !officer_upgrade_matches_race(policy.race, candidate)) {
            continue;
        }
        target = candidate;
        break;
    }
    if (!target) return nullptr;
    if (enabled) {
        const std::uint32_t completed =
            completed_officer_upgrades(selected_object);
        const std::uint32_t queued =
            queued_officer_upgrades(selected_object);
        const std::uint32_t maximum = static_cast<std::uint32_t>(
            std::max<std::int32_t>(0, policy.maximum_upgrades));
        *enabled = completed < maximum &&
            queued < maximum - completed;
    }
    return target;
}

void hide_legacy_officer_build_controls(
    void* const* popup_buttons) noexcept {
    if (!popup_buttons || !g_armada ||
        !g_legacy_officer_root_control_runtime_ready) {
        return;
    }
    std::size_t hidden = 0;
    for (std::size_t index = 0; index < kFoPopupButtonCapacity; ++index) {
        void* button = popup_buttons[index];
        void* mode_info = read_pointer_at(
            button, kControlButtonModeInfoPointerOffset);
        if (!mode_info ||
            read_int32_at(mode_info, kModeInfoTypeOffset, -1) != 1) {
            continue;
        }
        void* target = read_pointer_at(
            mode_info, kModeInfoTargetClassOffset);
        if (!target || !is_officer_upgrade_class(target)) continue;
        a2fo_a1_call_thiscall_0(
            at(g_armada, kControlButtonClearRva), button);
        ++hidden;
    }
    if (hidden > 0 && InterlockedCompareExchange(
            &g_legacy_officer_build_control_hidden_reported,
            1, 0) == 0) {
        log_line("A1 officer upgrade removed from the Build submenu; "
                 "the native root command owns it");
    }
}

void apply_legacy_officer_root_control(
    void* popup, void* const* popup_buttons) noexcept {
    if (!popup || !popup_buttons || !g_fleet_ops ||
        !g_legacy_officer_root_control_runtime_ready ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0) {
        return;
    }
    const std::uint32_t menu = static_cast<std::uint32_t>(read_int32_at(
        popup, kPopupPaletteCurrentMenuOffset, -1));
    if (menu == kLegacyPopupBuildMenu) {
        clear_legacy_officer_root_bindings(popup_buttons);
        hide_legacy_officer_build_controls(popup_buttons);
        return;
    }
    if (menu != kLegacyPopupRootMenu) {
        clear_legacy_officer_root_bindings(popup_buttons);
        return;
    }

    bool enabled = false;
    void* target = selected_legacy_officer_upgrade_target(&enabled);
    if (!target) {
        clear_legacy_officer_root_bindings(popup_buttons);
        return;
    }

    const LONG rectangle_count = InterlockedCompareExchange(
        &g_legacy_control_button_rect_count, 0, 0);
    const std::size_t usable_count = std::min<std::size_t>(
        static_cast<std::size_t>(std::max<LONG>(rectangle_count, 0)),
        g_legacy_control_button_rects.size());
    if (kLegacyOfficerRootControlSlot >= usable_count) return;

    void* previous_button = g_legacy_officer_root_button;
    void* button = nullptr;
    std::size_t button_index = kFoPopupButtonCapacity;
    std::size_t duplicate_count = 0;
    // Fleet Operations compacts the live palette with ControlButton::CopyModes.
    // Type-1 ModeInfo is copied by pointer, so follow our ModeInfo identity
    // across button objects instead of assuming the originally claimed object
    // still owns it. Remove any duplicate left by an earlier failed repair.
    for (std::size_t index = 0;
         index < kFoPopupButtonCapacity; ++index) {
        void* candidate = popup_buttons[index];
        if (read_pointer_at(
                candidate, kControlButtonModeInfoPointerOffset) !=
            g_legacy_officer_root_mode_info.data()) {
            continue;
        }
        if (!button && index < usable_count) {
            button = candidate;
            button_index = index;
        } else {
            a2fo_a1_call_thiscall_0(
                at(g_armada, kControlButtonClearRva), candidate);
            ++duplicate_count;
        }
    }
    if (!button) {
        // Any compatibility-owned occurrence outside the twelve A1 slots was
        // cleared above. Claim the first genuinely free A1 control.
        g_legacy_officer_root_button = nullptr;
        g_legacy_officer_root_target = nullptr;
        g_legacy_officer_root_enabled = false;
        for (std::size_t index = 0; index < usable_count; ++index) {
            if (!popup_control_is_bound(popup_buttons[index])) {
                button = popup_buttons[index];
                button_index = index;
                break;
            }
        }
    }
    if (!button || button_index >= usable_count) return;

    const bool binding_changed =
        g_legacy_officer_root_button != button ||
        g_legacy_officer_root_target != target ||
        g_legacy_officer_root_enabled != enabled ||
        read_pointer_at(
            button, kControlButtonModeInfoPointerOffset) !=
            g_legacy_officer_root_mode_info.data();
    const bool payload_changed =
        read_int32_at(
            g_legacy_officer_root_mode_info.data(),
            kModeInfoTypeOffset, -1) != 1 ||
        read_pointer_at(
            g_legacy_officer_root_mode_info.data(),
            kModeInfoTargetClassOffset) != target;
    // Rebuild the compatibility-owned payload on every pass. The live native
    // palette is allowed to consume or copy ModeInfo state, while this buffer
    // remains the stable identity used to recover its current owner.
    g_legacy_officer_root_mode_info.fill(0);
    write_int32_at(
        g_legacy_officer_root_mode_info.data(),
        kModeInfoTypeOffset, 1);
    void* target_slot =
        g_legacy_officer_root_mode_info.data() +
        kModeInfoTargetClassOffset;
    if (!writable_range(target_slot, sizeof(target))) return;
    std::memcpy(target_slot, &target, sizeof(target));
    g_legacy_officer_root_button = button;
    g_legacy_officer_root_target = target;
    g_legacy_officer_root_enabled = enabled;

    // Reassert the native state every pass so the A1 officer command remains
    // present while further upgrades are available; the state flag still
    // follows the live completed/queued cap.
    const auto state_mode_info =
        reinterpret_cast<ControlButtonStateModeInfoFn>(
            at(g_fleet_ops, kFoControlButtonStateModeInfoRva));
    state_mode_info(
        button, g_legacy_officer_root_mode_info.data(),
        enabled ? 1u : 0u);

    // A1 reserves row three, column three for the officer-quarter command.
    // If Fleet Ops has compacted another live control into that physical slot,
    // swap its presentation rectangle with the free control we just claimed.
    void* displaced = popup_buttons[kLegacyOfficerRootControlSlot];
    if (button_index != kLegacyOfficerRootControlSlot && displaced &&
        popup_control_is_bound(displaced) &&
        !is_native_speed_control_button(popup, displaced)) {
        write_component_rectangle(
            displaced, kControlButtonRectangleOffset,
            g_legacy_control_button_rects[button_index]);
    }
    write_component_rectangle(
        button, kControlButtonRectangleOffset,
        g_legacy_control_button_rects[kLegacyOfficerRootControlSlot]);

    const bool relocated = previous_button && previous_button != button;
    if ((binding_changed || relocated || duplicate_count > 0 ||
         payload_changed) && InterlockedCompareExchange(
            &g_legacy_officer_root_control_reported, 0, 0) != 0) {
        const LONG report = InterlockedIncrement(
            &g_legacy_officer_root_repair_report_count);
        if (report <= kMaximumLegacyOfficerRootRepairReports) {
            char message[320]{};
            std::snprintf(
                message, sizeof(message),
                "A1 officer root binding repair #%ld: previous=%p "
                "current=%p control=%lu relocated=%s duplicates=%lu "
                "payload=%s state=%ld",
                static_cast<long>(report), previous_button, button,
                static_cast<unsigned long>(button_index),
                relocated ? "yes" : "no",
                static_cast<unsigned long>(duplicate_count),
                payload_changed ? "rebuilt" : "intact",
                static_cast<long>(read_int32_at(
                    button, kControlButtonStateOffset, -1)));
            log_line(message);
        }
    }

    if (InterlockedCompareExchange(
            &g_legacy_officer_root_control_reported, 1, 0) == 0) {
        char target_name[96]{};
        copy_object_class_odf_name(
            target, target_name, sizeof(target_name));
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 officer root command active: control=%lu A1slot=%lu "
            "target='%s' enabled=%s",
            static_cast<unsigned long>(button_index),
            static_cast<unsigned long>(
                kLegacyOfficerRootControlSlot + 1),
            target_name[0] ? target_name : "<unavailable>",
            enabled ? "yes" : "no");
        log_line(message);
    }
}

void restore_pending_legacy_officer_root_menu(void* popup) noexcept {
    if (!popup || !g_fleet_ops ||
        !g_legacy_officer_root_control_runtime_ready) {
        return;
    }
    void* pending_producer = InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID volatile*>(
            &g_legacy_officer_root_pending_producer),
        nullptr, nullptr);
    if (!pending_producer) return;
    if (pending_producer != g_legacy_selected_object) {
        InterlockedCompareExchangePointer(
            reinterpret_cast<PVOID volatile*>(
                &g_legacy_officer_root_pending_producer),
            nullptr, pending_producer);
        return;
    }
    const std::uint32_t current_menu = static_cast<std::uint32_t>(
        read_int32_at(popup, kPopupPaletteCurrentMenuOffset, -1));
    if (!a1compat::should_restore_legacy_officer_root_menu(
            true, true, current_menu)) {
        return;
    }
    const auto set_current_menu =
        reinterpret_cast<PopupPaletteSetCurrentMenuFn>(
            at(g_fleet_ops, kFoPopupPaletteSetCurrentMenuRva));
    set_current_menu(popup, kLegacyPopupRootMenu);
    InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID volatile*>(
            &g_legacy_officer_root_pending_producer),
        nullptr, pending_producer);
    if (InterlockedCompareExchange(
            &g_legacy_officer_root_return_reported, 1, 0) == 0) {
        log_line("A1 officer upgrade accepted on Root; restored the "
                 "Root command palette after Fleet Ops entered Build");
    }
}

void __attribute__((fastcall)) control_button_press_vtable_hook(
    void* button, void*) noexcept {
    const bool officer_root_button =
        button && button == g_legacy_officer_root_button &&
        g_legacy_officer_root_enabled &&
        g_legacy_selected_object && g_legacy_officer_root_target &&
        read_pointer_at(
            button, kControlButtonModeInfoPointerOffset) ==
            g_legacy_officer_root_mode_info.data();
    if (officer_root_button && g_producer_push_target_original) {
        // A native type-1 ModeInfo click is routed through PopupPalette's
        // current submenu and therefore treats this synthetic Root control as
        // a request to enter Build. Dispatch the already-resolved producer
        // target directly through the installed admission wrapper instead.
        // StandardButton has already handled its press animation/state.
        g_legacy_officer_root_direct_dispatch_active = true;
        producer_push_build_queue_item_hook(
            g_legacy_selected_object, nullptr,
            g_legacy_officer_root_target);
        g_legacy_officer_root_direct_dispatch_active = false;
        InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(
                &g_legacy_officer_root_pending_producer),
            nullptr);
        if (InterlockedCompareExchange(
                &g_legacy_officer_root_direct_dispatch_reported,
                1, 0) == 0) {
            log_line("A1 officer root button dispatched its producer "
                     "target directly without entering Build");
        }
        return;
    }
    if (g_control_button_press_original) {
        a2fo_a1_call_thiscall_0(
            g_control_button_press_original, button);
    }
}

bool install_legacy_officer_root_button_dispatch() noexcept {
    if (!g_armada || g_control_button_press_vtable_hook_installed) {
        return g_control_button_press_vtable_hook_installed;
    }
    auto** slot = at<void*>(
        g_armada,
        kControlButtonVtableRva + kControlButtonPressVtableOffset);
    void* const native_press = at(g_armada, kControlButtonPressRva);
    if (!readable_range(slot, sizeof(void*)) ||
        !readable_range(
            native_press, sizeof(kExpectedControlButtonPress)) ||
        *slot != native_press ||
        std::memcmp(
            native_press, kExpectedControlButtonPress,
            sizeof(kExpectedControlButtonPress)) != 0) {
        log_line("A1 officer root-button press boundary unavailable");
        return false;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(
            slot, sizeof(void*), PAGE_READWRITE, &old_protect)) {
        log_line("A1 officer root-button vtable protection change failed");
        return false;
    }
    void* const original = InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(slot),
        reinterpret_cast<void*>(&control_button_press_vtable_hook));
    DWORD restored = 0;
    VirtualProtect(slot, sizeof(void*), old_protect, &restored);
    if (original != native_press) {
        DWORD rollback_protect = 0;
        if (VirtualProtect(
                slot, sizeof(void*), PAGE_READWRITE,
                &rollback_protect)) {
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(slot), original);
            DWORD rollback_restored = 0;
            VirtualProtect(
                slot, sizeof(void*), rollback_protect,
                &rollback_restored);
        }
        log_line("A1 officer root-button vtable changed during installation");
        return false;
    }
    g_control_button_press_vtable_slot = slot;
    g_control_button_press_original = original;
    g_control_button_press_vtable_hook_installed = true;
    return true;
}

std::size_t apply_legacy_speed_special_layout(
    void* popup, void* const* popup_buttons,
    const a1compat::NativeUiRectangle& panel) noexcept {
    InterlockedExchange(&g_legacy_speed_special_layout_count, 0);
    if (!popup || !popup_buttons || InterlockedCompareExchange(
            &g_legacy_speed_control_layout_available, 0, 0) == 0) {
        return 0;
    }

    std::size_t placed = 0;
    for (std::size_t index = 0;
         index < kFoPopupButtonCapacity &&
         placed < g_legacy_speed_queue_screen_rects.size(); ++index) {
        void* control_button = popup_buttons[index];
        if (!is_native_speed_special_control_button(
                popup, control_button)) {
            continue;
        }
        const a1compat::NativeUiRectangle& screen_rectangle =
            g_legacy_speed_queue_screen_rects[placed];
        const a1compat::NativeUiRectangle local_rectangle{
            screen_rectangle.left - panel.left,
            screen_rectangle.top - panel.top,
            screen_rectangle.right - panel.left,
            screen_rectangle.bottom - panel.top};
        if (!write_component_rectangle(
                control_button, kControlButtonRectangleOffset,
                local_rectangle)) {
            continue;
        }
        ++placed;
    }
    InterlockedExchange(
        &g_legacy_speed_special_layout_count,
        static_cast<LONG>(placed));
    if (placed > 0 && InterlockedCompareExchange(
            &g_legacy_speed_special_layout_reported, 1, 0) == 0) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A1 SpeedRail special-weapon strip active: %lu native "
            "type-2/SPECIAL_ATTACK control(s) in slots 1-%lu",
            static_cast<unsigned long>(placed),
            static_cast<unsigned long>(placed));
        log_line(message);
    }
    return placed;
}

bool apply_legacy_speed_transport_layout(
    void* popup, void* const* popup_buttons,
    const a1compat::NativeUiRectangle& panel) noexcept {
    if (!popup || !popup_buttons || InterlockedCompareExchange(
            &g_legacy_speed_control_layout_available, 0, 0) == 0) {
        return false;
    }

    const a1compat::NativeUiRectangle& screen_rectangle =
        g_legacy_speed_transport_screen_rect;
    const a1compat::NativeUiRectangle local_rectangle{
        screen_rectangle.left - panel.left,
        screen_rectangle.top - panel.top,
        screen_rectangle.right - panel.left,
        screen_rectangle.bottom - panel.top};
    for (std::size_t index = 0; index < kFoPopupButtonCapacity; ++index) {
        void* control_button = popup_buttons[index];
        if (!is_native_transport_control_button(popup, control_button)) {
            continue;
        }
        if (!write_component_rectangle(
                control_button, kControlButtonRectangleOffset,
                local_rectangle)) {
            return false;
        }
        if (InterlockedCompareExchange(
                &g_legacy_speed_transport_layout_reported, 1, 0) == 0) {
            char message[320]{};
            std::snprintf(
                message, sizeof(message),
                "A1 SpeedRail Transport active: popup slot=%lu; "
                "screen=[%ld,%ld,%ld,%ld]; local=[%ld,%ld,%ld,%ld]",
                static_cast<unsigned long>(index),
                static_cast<long>(screen_rectangle.left),
                static_cast<long>(screen_rectangle.top),
                static_cast<long>(screen_rectangle.right),
                static_cast<long>(screen_rectangle.bottom),
                static_cast<long>(local_rectangle.left),
                static_cast<long>(local_rectangle.top),
                static_cast<long>(local_rectangle.right),
                static_cast<long>(local_rectangle.bottom));
            log_line(message);
        }
        return true;
    }
    report_legacy_popup_controls(popup, popup_buttons);
    return false;
}

void apply_legacy_control_panel_layout(void* popup) noexcept {
    const LONG rectangle_count = InterlockedCompareExchange(
        &g_legacy_control_button_rect_count, 0, 0);
    if (!popup || !g_fleet_ops || rectangle_count <= 0 ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0) {
        return;
    }

    restore_pending_legacy_officer_root_menu(popup);

    const a1compat::NativeUiRectangle panel =
        g_legacy_control_panel_rect;
    const std::array<std::int32_t, 4> native_panel{
        panel.left, panel.top, panel.right, panel.bottom};
    auto* panel_destination = static_cast<std::uint8_t*>(popup) +
        kPopupPaletteRectangleOffset;
    if (!writable_range(panel_destination, sizeof(native_panel))) return;
    std::memcpy(
        panel_destination, native_panel.data(), sizeof(native_panel));

    void** popup_buttons = at<void*>(
        g_fleet_ops, kFoPopupButtonPointerArrayRva);
    if (!readable_range(
            popup_buttons, kFoPopupButtonCapacity * sizeof(void*))) {
        return;
    }

    std::size_t applied_count = 0;
    for (std::size_t slot = 0;
         slot < static_cast<std::size_t>(rectangle_count) &&
         slot < g_legacy_control_button_rects.size(); ++slot) {
        void* control_button = popup_buttons[slot];
        if (!control_button) continue;
        const a1compat::NativeUiRectangle rectangle =
            g_legacy_control_button_rects[slot];
        const std::array<std::int32_t, 4> native_rectangle{
            rectangle.left, rectangle.top,
            rectangle.right, rectangle.bottom};
        auto* destination = static_cast<std::uint8_t*>(control_button) +
            kControlButtonRectangleOffset;
        if (!writable_range(destination, sizeof(native_rectangle))) continue;
        std::memcpy(
            destination, native_rectangle.data(),
            sizeof(native_rectangle));
        ++applied_count;
    }

    apply_legacy_speed_special_layout(popup, popup_buttons, panel);
    apply_legacy_speed_transport_layout(popup, popup_buttons, panel);
    apply_legacy_officer_root_control(popup, popup_buttons);

    if (applied_count > 0 && InterlockedCompareExchange(
            &g_legacy_control_button_adapter_reported, 1, 0) == 0) {
        const a1compat::NativeUiRectangle first =
            g_legacy_control_button_rects[0];
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ControlPanel adapter active: panel="
            "[%ld,%ld,%ld,%ld]; %lu/%ld local slots applied; "
            "first=[%ld,%ld,%ld,%ld]",
            static_cast<long>(panel.left),
            static_cast<long>(panel.top),
            static_cast<long>(panel.right),
            static_cast<long>(panel.bottom),
            static_cast<unsigned long>(applied_count),
            static_cast<long>(rectangle_count),
            static_cast<long>(first.left),
            static_cast<long>(first.top),
            static_cast<long>(first.right),
            static_cast<long>(first.bottom));
        log_line(message);
    }
}

void __attribute__((fastcall)) popup_palette_post_load_hook(
    void* popup, void*) noexcept {
    if (g_popup_palette_post_load_hook.gateway) {
        a2fo_a1_call_thiscall_0(
            g_popup_palette_post_load_hook.gateway, popup);
    }
    apply_legacy_control_panel_layout(popup);
    initialize_legacy_control_background(popup);
}

void __attribute__((fastcall)) popup_palette_focus_game_simulate_hook(
    void* popup, void*, std::uintptr_t simulation_context) noexcept {
    // The pre-pass supplies A1 geometry and compatibility-owned bindings to
    // Fleet Operations' native input dispatch. A successful class-target
    // click can consume and compact those bindings inside the gateway, so a
    // matching post-pass presents the next officer upgrade immediately
    // instead of waiting for a menu change to rebuild the page.
    apply_legacy_control_panel_layout(popup);
    const std::uint32_t menu_before = static_cast<std::uint32_t>(
        read_int32_at(popup, kPopupPaletteCurrentMenuOffset, -1));
    if (menu_before == kLegacyPopupRootMenu &&
        g_legacy_officer_root_enabled &&
        g_legacy_selected_object && g_legacy_officer_root_target) {
        // Snapshot the compatibility binding before native ControlButton
        // dispatch can consume its ModeInfo. Producer queue admission uses
        // this identity to distinguish Officer Upgrade from the genuine
        // Root-to-Build command transition.
        g_legacy_officer_root_dispatch_producer =
            g_legacy_selected_object;
        g_legacy_officer_root_dispatch_target =
            g_legacy_officer_root_target;
    } else {
        g_legacy_officer_root_dispatch_producer = nullptr;
        g_legacy_officer_root_dispatch_target = nullptr;
    }
    if (g_fo_popup_palette_focus_game_simulate_hook.gateway) {
        a2fo_a1_call_thiscall_1(
            g_fo_popup_palette_focus_game_simulate_hook.gateway, popup,
            simulation_context);
    }
    apply_legacy_control_panel_layout(popup);
}

void __attribute__((fastcall)) popup_palette_render_hook(
    void* popup, void*) noexcept {
    apply_legacy_control_panel_layout(popup);
    // A2 removed A1 ControlPanel's dedicated solid backing rectangle. Draw
    // the data-defined black mask first so frame sprites and buttons remain
    // above it.
    draw_legacy_control_black_mask(popup);
    if (initialize_legacy_control_background(popup)) {
        a2fo_a1_call_thiscall_0(
            at(g_armada, kStandardBackgroundRenderRva),
            g_legacy_control_background_storage.data());
    }
    if (g_popup_palette_render_hook.gateway) {
        a2fo_a1_call_thiscall_0(
            g_popup_palette_render_hook.gateway, popup);
    }
}

bool __attribute__((fastcall)) popup_palette_cursor_over_hook(
    void* popup, void*) noexcept {
    apply_legacy_control_panel_layout(popup);
    if (g_popup_palette_cursor_over_hook.gateway) {
        if ((a2fo_a1_call_thiscall_0(
                 g_popup_palette_cursor_over_hook.gateway,
                 popup) & 0xffu) != 0) {
            return true;
        }
    }
    if (InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0) {
        return false;
    }
    const LONG special_count = std::clamp<LONG>(
        InterlockedCompareExchange(
            &g_legacy_speed_special_layout_count, 0, 0),
        0, static_cast<LONG>(g_legacy_speed_queue_screen_rects.size()));
    for (LONG index = 0; index < special_count; ++index) {
        if (interface_cursor_in_rectangle(
                g_legacy_speed_queue_screen_rects[
                    static_cast<std::size_t>(index)])) {
            return true;
        }
    }
    return InterlockedCompareExchange(
               &g_legacy_speed_transport_layout_reported, 0, 0) != 0 &&
        interface_cursor_in_rectangle(g_legacy_speed_transport_screen_rect);
}

void report_legacy_ship_display_alias(
    const char* category, const char* requested,
    const char* resolved) noexcept {
    const LONG report = InterlockedIncrement(
        &g_legacy_ship_display_alias_report_count);
    if (report > kMaximumLegacyShipDisplayAliasReports) {
        if (report == kMaximumLegacyShipDisplayAliasReports + 1) {
            log_line("Further A1 ShipDisplay key-alias reports suppressed");
        }
        return;
    }
    char message[320]{};
    std::snprintf(
        message, sizeof(message),
        "A1 ShipDisplay %s alias #%ld: '%s' -> '%s'",
        category ? category : "key", static_cast<long>(report),
        requested ? requested : "<null>",
        resolved ? resolved : "<null>");
    log_line(message);
}

std::int32_t* __cdecl display_interface_load_rectangle_hook(
    std::int32_t* output, const char* key) noexcept {
    const std::uintptr_t request_return_address =
        reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
    const char* resolved = key;
    if (key && InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) != 0) {
        const a1compat::NativeUiRectangle* legacy_resource_rectangle =
            nullptr;
        const a1compat::NativeUiRectangle* legacy_multiship_rectangle =
            nullptr;
        if (output && InterlockedCompareExchange(
                &g_legacy_ship_display_multi_layout_available, 0, 0) != 0) {
            const std::int32_t tile_index =
                a1compat::legacy_indexed_ui_key(
                    key, "infoMultiShipIcon_",
                    static_cast<std::int32_t>(
                        kShipDisplayMultiShipCount - 1));
            const std::int32_t shield_index =
                a1compat::legacy_indexed_ui_key(
                    key, "infoMultiShipShield_",
                    static_cast<std::int32_t>(
                        kShipDisplayMultiShipCount - 1));
            if (tile_index >= 0) {
                legacy_multiship_rectangle =
                    static_cast<std::size_t>(tile_index) <
                            kLegacyMultiShipCount
                        ? (request_return_address ==
                                   reinterpret_cast<std::uintptr_t>(at(
                                       g_armada,
                                       kMultiShipWireframeRectangleLoadReturnRva))
                               ? &g_legacy_ship_display_multi_wireframe_rects[
                                     static_cast<std::size_t>(tile_index)]
                               : &g_legacy_ship_display_multi_tile_rects[
                                     static_cast<std::size_t>(tile_index)])
                        : &kHiddenLegacyResourceRectangle;
            } else if (shield_index >= 0) {
                legacy_multiship_rectangle =
                    static_cast<std::size_t>(shield_index) <
                            kLegacyMultiShipCount
                        ? &g_legacy_ship_display_multi_shield_rects[
                              static_cast<std::size_t>(shield_index)]
                        : &kHiddenLegacyResourceRectangle;
            }
        }
        if (InterlockedCompareExchange(
                &g_legacy_resource_panel_available, 0, 0) != 0) {
            if (_stricmp(key, "resourcePanelArea") == 0) {
                legacy_resource_rectangle = &g_legacy_resource_panel_rect;
            } else if (std::strlen(key) == 10 &&
                       _strnicmp(key, "resource_", 9) == 0 &&
                       key[9] >= '0' && key[9] <= '5') {
                legacy_resource_rectangle =
                    &g_legacy_resource_text_rects[
                        static_cast<std::size_t>(key[9] - '0')];
            }
        }
        const a1compat::NativeUiRectangle* direct_rectangle =
            legacy_multiship_rectangle
                ? legacy_multiship_rectangle
                : legacy_resource_rectangle;
        if (direct_rectangle && output) {
            const std::array<std::int32_t, 4> rectangle{
                direct_rectangle->left,
                direct_rectangle->top,
                direct_rectangle->right,
                direct_rectangle->bottom};
            std::memcpy(output, rectangle.data(), sizeof(rectangle));
            if (legacy_multiship_rectangle &&
                InterlockedCompareExchange(
                    &g_legacy_ship_display_multi_constructor_reported,
                    1, 0) == 0) {
                log_line("A1 ShipDisplay multi-selection constructor "
                         "rectangles active: exact 8-tile grid with nested "
                         "shield bars and wireframes; A2-only slots hidden");
            }
            return output;
        }
        if (const char* alias =
                a1compat::legacy_ship_display_rectangle_alias(key)) {
            resolved = alias;
            report_legacy_ship_display_alias("rectangle", key, alias);
        }
    }
    using LoadRectangleFn = std::int32_t* (__cdecl*)(
        std::int32_t*, const char*);
    if (g_display_interface_load_rectangle_hook.gateway) {
        return reinterpret_cast<LoadRectangleFn>(
            g_display_interface_load_rectangle_hook.gateway)(
                output, resolved);
    }
    if (output) std::fill_n(output, 4, 0);
    return output;
}

const char* native_string_c_str(const void* native_string) noexcept {
    if (!native_string || !g_armada) return nullptr;
    void** c_str_slot = at<void*>(g_armada, kNativeStringCStrIatRva);
    if (!readable_range(c_str_slot, sizeof(*c_str_slot)) ||
        !is_executable_pointer(*c_str_slot)) {
        return nullptr;
    }
    return reinterpret_cast<const char*>(a2fo_a1_call_thiscall_0(
        *c_str_slot, const_cast<void*>(native_string)));
}

void __attribute__((fastcall)) standard_background_initialize_hook(
    void* background, void*, const void* native_prefix) noexcept {
    if (!g_standard_background_initialize_hook.gateway) return;

    const char* requested = native_string_c_str(native_prefix);
    if (requested && InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) != 0) {
        if (_stricmp(requested, "resourcePanel") == 0 &&
            InterlockedCompareExchange(
                &g_legacy_resource_panel_available, 0, 0) != 0) {
            if (InterlockedCompareExchange(
                    &g_legacy_resource_background_suppression_reported,
                    1, 0) == 0) {
                log_line("A1 resource strip suppressed the A2 composite "
                         "ResourcePanel background");
            }
            return;
        }
        if (const char* alias =
                a1compat::legacy_ship_display_string_alias(requested)) {
            report_legacy_ship_display_alias(
                "background prefix", requested, alias);
            if (initialize_standard_background_configuration(
                    background, alias,
                    g_standard_background_initialize_hook.gateway)) {
                return;
            }
        }
    }
    a2fo_a1_call_thiscall_1(
        g_standard_background_initialize_hook.gateway, background,
        reinterpret_cast<std::uintptr_t>(native_prefix));
}

bool install_essential_gui_sprite_loader(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->patch_call) return false;
    if (!a1_native_entry_supported(
            "ST3D_TextFileParser::ReadTable",
            kSt3dTextFileParserReadTableRva,
            kExpectedSt3dTextFileParserReadTable) ||
        !a1_native_entry_supported(
            "ST3D_Database::Find", kSt3dDatabaseFindRva,
            kExpectedSt3dDatabaseFind)) {
        return false;
    }
    return api->patch_call(
        at(g_armada, kDisplayInterfaceGuiSpriteReadTableCallRva),
        reinterpret_cast<void*>(&a2fo_a1_gui_sprite_read_table_hook),
        kExpectedDisplayInterfaceGuiSpriteReadTableCall,
        sizeof(kExpectedDisplayInterfaceGuiSpriteReadTableCall));
}

bool install_legacy_default_cursor_fallback(
    const A2FO_ModuleApi* api) noexcept {
    if (!g_legacy_cursor_override_required) return true;
    if (!api || !g_armada || !api->install_inline_hook) {
        return false;
    }
    if (!a1_native_entry_supported(
            "ST3D_Database::Find", kSt3dDatabaseFindRva,
            kExpectedSt3dDatabaseFind)) {
        return false;
    }
    return api->install_inline_hook(
        at(g_armada, kDefaultCursorLookupRva),
        reinterpret_cast<void*>(&legacy_default_cursor_lookup_hook),
        kDefaultCursorLookupHookLength,
        kExpectedDefaultCursorLookup,
        &g_legacy_default_cursor_lookup_hook);
}

bool install_legacy_tactical_cursor_fallback(
    const A2FO_ModuleApi* api) noexcept {
    if (!g_legacy_tactical_cursor_override_required) return true;
    if (!api || !g_armada) {
        return false;
    }
    if (!a1_native_entry_supported(
            "ST3D_Database::Find", kSt3dDatabaseFindRva,
            kExpectedSt3dDatabaseFind)) {
        return false;
    }
    for (const std::uintptr_t slot_rva :
         kTacticalSelectCursorCacheSlotRvas) {
        void* const slot = at(g_armada, slot_rva);
        if (!readable_range(slot, sizeof(void*)) ||
            !writable_range(slot, sizeof(void*))) {
            return false;
        }
    }
    void* const active_slot = at(
        g_armada, kActiveCursorSpritePointerRva);
    return readable_range(active_slot, sizeof(void*)) &&
        writable_range(active_slot, sizeof(void*));
}

bool install_legacy_gameplay_ui_scaling(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->install_inline_hook) return false;
    if (!a1_native_entry_supported(
            "ParameterDB::GetInt", kParameterDbGetIntRva,
            kExpectedParameterDbGetInt) ||
        !a1_native_entry_supported(
            "ParameterDB::Get(DBRectangle)",
            kParameterDbGetRectangleRva,
            kExpectedParameterDbGetRectangle)) {
        return false;
    }
    const bool installed = api->install_inline_hook(
        at(g_armada, kDisplayInterfaceGuiParameterDbPostConstructRva),
        reinterpret_cast<void*>(
            &a2fo_a1_gui_parameter_db_post_construct_hook),
        kDisplayInterfaceGuiParameterDbPostConstructLength,
        kExpectedDisplayInterfaceGuiParameterDbPostConstruct,
        &g_gui_parameter_db_post_construct_hook);
    g_a2fo_a1_gui_parameter_db_post_construct_gateway = installed
        ? g_gui_parameter_db_post_construct_hook.gateway : nullptr;
    return installed;
}

bool install_legacy_tooltip_adapter(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->install_inline_hook) return false;
    if (!a1_native_entry_supported(
            "ParameterDB::Get(Vector) A1 tooltip colours",
            kParameterDbGetVectorRva, kExpectedParameterDbGetVector) ||
        !a1_native_entry_supported(
            "Tooltip::Render", kTooltipRenderRva,
            kExpectedTooltipRender) ||
        !a1_native_entry_supported(
            "Tooltip popup renderer", kTooltipDrawPopupRva,
            kExpectedTooltipDrawPopup) ||
        !a1_native_entry_supported(
            "Tooltip::RenderVerbose", kTooltipRenderVerboseRva,
            kExpectedTooltipRenderVerbose)) {
        return false;
    }
    const bool render_installed = api->install_inline_hook(
        at(g_armada, kTooltipRenderRva),
        reinterpret_cast<void*>(&tooltip_render_hook),
        kTooltipRenderHookLength, kExpectedTooltipRender,
        &g_tooltip_render_hook);
    const bool verbose_installed = api->install_inline_hook(
        at(g_armada, kTooltipRenderVerboseRva),
        reinterpret_cast<void*>(&tooltip_render_verbose_hook),
        kTooltipRenderVerboseHookLength,
        kExpectedTooltipRenderVerbose,
        &g_tooltip_render_verbose_hook);
    return render_installed && verbose_installed;
}

bool install_legacy_cinematic_view_adapter(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->install_inline_hook) return false;
    if (!a1_native_entry_supported(
            "CinematicView::Render",
            kCinematicViewRenderRva,
            kExpectedCinematicViewRender) ||
        !a1_native_entry_supported(
            "ButtonPanel::FocusGameSimulate",
            kButtonPanelFocusGameSimulateRva,
            kExpectedButtonPanelFocusGameSimulate) ||
        !a1_native_entry_supported(
            "ButtonPanel::Render",
            kButtonPanelRenderRva,
            kExpectedButtonPanelRender) ||
        !a1_native_entry_supported(
            "ButtonPanel::CursorOver",
            kButtonPanelCursorOverRva,
            kExpectedButtonPanelCursorOver) ||
        !a1_native_entry_supported(
            "DisplayInterface::LoadSprite",
            kDisplayInterfaceLoadSpriteRva,
            kExpectedDisplayInterfaceLoadSprite) ||
        !a1_native_entry_supported(
            "StandardButton::SetSprites",
            kStandardButtonSetSpritesRva,
            kExpectedStandardButtonSetSprites)) {
        return false;
    }
    const bool cinematic_installed = api->install_inline_hook(
        at(g_armada, kCinematicViewRenderRva),
        reinterpret_cast<void*>(&cinematic_view_render_hook),
        kCinematicViewRenderHookLength,
        kExpectedCinematicViewRender,
        &g_cinematic_view_render_hook);
    const bool simulate_installed = api->install_inline_hook(
        at(g_armada, kButtonPanelFocusGameSimulateRva),
        reinterpret_cast<void*>(
            &button_panel_focus_game_simulate_hook),
        kButtonPanelFocusGameSimulateHookLength,
        kExpectedButtonPanelFocusGameSimulate,
        &g_button_panel_focus_game_simulate_hook);
    const bool render_installed = api->install_inline_hook(
        at(g_armada, kButtonPanelRenderRva),
        reinterpret_cast<void*>(&button_panel_render_hook),
        kButtonPanelRenderHookLength,
        kExpectedButtonPanelRender,
        &g_button_panel_render_hook);
    const bool cursor_installed = api->install_inline_hook(
        at(g_armada, kButtonPanelCursorOverRva),
        reinterpret_cast<void*>(&button_panel_cursor_over_hook),
        kButtonPanelCursorOverHookLength,
        kExpectedButtonPanelCursorOver,
        &g_button_panel_cursor_over_hook);
    g_legacy_cinematic_button_runtime_ready =
        simulate_installed && render_installed && cursor_installed;
    if (!g_legacy_cinematic_button_runtime_ready) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A1 CinematicView COMM/MENU adapter incomplete "
            "(simulate=%d render=%d cursor=%d)",
            simulate_installed ? 1 : 0,
            render_installed ? 1 : 0,
            cursor_installed ? 1 : 0);
        log_line(message);
    }
    return cinematic_installed &&
        g_legacy_cinematic_button_runtime_ready;
}

bool install_legacy_control_panel_adapter(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !g_fleet_ops ||
        !api->install_inline_hook) {
        return false;
    }
    if (!a1_native_entry_supported(
            "PopupPaletteImp::PostLoad",
            kPopupPalettePostLoadRva,
            kExpectedPopupPalettePostLoad) ||
        !readable_range(
            at(g_fleet_ops, kFoPopupPaletteFocusGameSimulateRva),
            sizeof(kExpectedFoPopupPaletteFocusGameSimulate)) ||
        std::memcmp(
            at(g_fleet_ops, kFoPopupPaletteFocusGameSimulateRva),
            kExpectedFoPopupPaletteFocusGameSimulate,
            sizeof(kExpectedFoPopupPaletteFocusGameSimulate)) != 0 ||
        !a1_native_entry_supported(
            "PopupPaletteImp::Render",
            kPopupPaletteRenderRva,
            kExpectedPopupPaletteRender) ||
        !a1_native_entry_supported(
            "PopupPaletteImp::CursorOver",
            kPopupPaletteCursorOverRva,
            kExpectedPopupPaletteCursorOver) ||
        !a1_native_entry_supported(
            "StandardBackground::StandardBackground",
            kStandardBackgroundConstructorRva,
            kExpectedStandardBackgroundConstructor) ||
        !a1_native_entry_supported(
            "StandardBackground::~StandardBackground",
            kStandardBackgroundDestructorRva,
            kExpectedStandardBackgroundDestructor) ||
        !a1_native_entry_supported(
            "StandardBackground::Render",
            kStandardBackgroundRenderRva,
            kExpectedStandardBackgroundRender) ||
        !a1_native_entry_supported(
            "StandardBackground::InitializeConfiguration",
            kStandardBackgroundInitializeConfigurationRva,
            kExpectedStandardBackgroundInitializeConfiguration)) {
        return false;
    }

    g_legacy_control_black_mask_runtime_ready =
        a1_native_entry_supported(
            "DisplayInterface::DrawRectangle",
            kDisplayInterfaceDrawRectangleRva,
            kExpectedDisplayInterfaceDrawRectangle) &&
        readable_range(
            at(g_armada, kInterfaceBlackColourRva),
            sizeof(SpriteVector));
    if (!g_legacy_control_black_mask_runtime_ready) {
        log_line("A1 ControlPanel black-mask renderer unavailable; "
                 "panel geometry, artwork, and buttons remain enabled");
    }
    g_legacy_officer_root_control_runtime_ready =
        a1_native_entry_supported(
            "ControlButton::Clear A1 officer root command",
            kControlButtonClearRva, kExpectedControlButtonClear) &&
        readable_range(
            at(g_fleet_ops, kFoPopupPaletteSetCurrentMenuRva),
            sizeof(kExpectedFoPopupPaletteSetCurrentMenu)) &&
        std::memcmp(
            at(g_fleet_ops, kFoPopupPaletteSetCurrentMenuRva),
            kExpectedFoPopupPaletteSetCurrentMenu,
            sizeof(kExpectedFoPopupPaletteSetCurrentMenu)) == 0 &&
        readable_range(
            at(g_fleet_ops, kFoControlButtonStateModeInfoRva),
            sizeof(kExpectedFoControlButtonStateModeInfo)) &&
        std::memcmp(
            at(g_fleet_ops, kFoControlButtonStateModeInfoRva),
            kExpectedFoControlButtonStateModeInfo,
            sizeof(kExpectedFoControlButtonStateModeInfo)) == 0 &&
        install_legacy_officer_root_button_dispatch();
    if (!g_legacy_officer_root_control_runtime_ready) {
        log_line("A1 officer root-command bridge unavailable; officer "
                 "upgrades retain the Fleet Ops Build submenu");
    }

    const bool post_load_installed = api->install_inline_hook(
        at(g_armada, kPopupPalettePostLoadRva),
        reinterpret_cast<void*>(&popup_palette_post_load_hook),
        kPopupPalettePostLoadHookLength,
        kExpectedPopupPalettePostLoad,
        &g_popup_palette_post_load_hook);
    const bool simulate_installed = api->install_inline_hook(
        at(g_fleet_ops, kFoPopupPaletteFocusGameSimulateRva),
        reinterpret_cast<void*>(
            &popup_palette_focus_game_simulate_hook),
        kFoPopupPaletteFocusGameSimulateHookLength,
        kExpectedFoPopupPaletteFocusGameSimulate,
        &g_fo_popup_palette_focus_game_simulate_hook);
    const bool render_installed = api->install_inline_hook(
        at(g_armada, kPopupPaletteRenderRva),
        reinterpret_cast<void*>(&popup_palette_render_hook),
        kPopupPaletteRenderHookLength,
        kExpectedPopupPaletteRender,
        &g_popup_palette_render_hook);
    const bool cursor_installed = api->install_inline_hook(
        at(g_armada, kPopupPaletteCursorOverRva),
        reinterpret_cast<void*>(&popup_palette_cursor_over_hook),
        kPopupPaletteCursorOverHookLength,
        kExpectedPopupPaletteCursorOver,
        &g_popup_palette_cursor_over_hook);
    const bool live_boundaries_installed =
        simulate_installed && render_installed && cursor_installed;
    if (!post_load_installed && live_boundaries_installed) {
        log_line("A1 ControlPanel PostLoad boundary remains Fleet Ops-owned; "
                 "render/input paths will initialize the legacy panel lazily");
    } else if (!live_boundaries_installed) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ControlPanel adapter installed only part of its popup "
            "boundaries (postLoad=%d simulate=%d render=%d cursor=%d)",
            post_load_installed ? 1 : 0,
            simulate_installed ? 1 : 0,
            render_installed ? 1 : 0,
            cursor_installed ? 1 : 0);
        log_line(message);
    }
    return live_boundaries_installed;
}

bool install_legacy_ship_display_adapter(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->install_inline_hook) return false;
    if (!a1_native_entry_supported(
            "DisplayInterface::LoadRectangle",
            kDisplayInterfaceLoadRectangleRva,
            kExpectedDisplayInterfaceLoadRectangle) ||
        !a1_native_entry_supported(
            "StandardBackground::InitializeConfiguration "
            "ShipDisplay alias",
            kStandardBackgroundInitializeConfigurationRva,
            kExpectedStandardBackgroundInitializeConfiguration)) {
        return false;
    }

    const bool render_supported = a1_native_entry_supported(
        "ShipDisplay::Render", kShipDisplayRenderRva,
        kExpectedShipDisplayRender);
    const bool cursor_supported = a1_native_entry_supported(
        "ShipDisplay::CursorOver", kShipDisplayCursorOverRva,
        kExpectedShipDisplayCursorOver);
    const bool construction_bar_render_supported = a1_native_entry_supported(
        "ConstructionBar::Render A1 construction-rig target",
        kConstructionBarRenderRva,
        kExpectedConstructionBarRender);
    const bool construction_rig_get_supported = a1_native_entry_supported(
        "ConstructionRig::GetConstructionObject A1 progress target",
        kConstructionRigGetConstructionObjectRva,
        kExpectedConstructionRigGetConstructionObject);
    const bool find_game_object_supported = a1_native_entry_supported(
        "GameObject::FindById A1 construction-rig target",
        kFindGameObjectByIdRva,
        kExpectedFindGameObjectById);
    const bool background_render_supported = a1_native_entry_supported(
        "StandardBackground::Render ShipDisplay artwork",
        kStandardBackgroundRenderRva,
        kExpectedStandardBackgroundRender);
    const bool rectangle_draw_supported = a1_native_entry_supported(
        "DisplayInterface::DrawRectangle A1 opaque ShipDisplay mask",
        kDisplayInterfaceDrawRectangleRva,
        kExpectedDisplayInterfaceDrawRectangle) &&
        readable_range(
            at(g_armada, kInterfaceBlackColourRva),
            sizeof(SpriteVector));
    const bool team_colour_supported = a1_native_entry_supported(
        "GameObject::GetTeamColour A1 RaceIcon background",
        kGameObjectTeamColourRva,
        kExpectedGameObjectTeamColour);
    g_legacy_ship_display_text_runtime_ready =
        a1_native_entry_supported(
            "DisplayInterface::DrawTextInRectangle A1 identity text",
            kDisplayInterfaceDrawTextInRectangleRva,
            kExpectedDisplayInterfaceDrawTextInRectangle) &&
        readable_range(
            at(g_armada, kLocalizationLookupRva), 1) &&
        readable_range(
            at(g_armada, kLocalizationManagerPointerRva),
            sizeof(void*));
    if (!g_legacy_ship_display_text_runtime_ready) {
        log_line("A1 ShipDisplay localized Crew/Officer renderer "
                 "unavailable; native amount text remains enabled");
    }
    const bool render_installed = render_supported &&
        api->install_inline_hook(
            at(g_armada, kShipDisplayRenderRva),
            reinterpret_cast<void*>(&ship_display_render_hook),
            kShipDisplayRenderHookLength,
            kExpectedShipDisplayRender,
            &g_ship_display_render_hook);
    const bool cursor_installed = cursor_supported &&
        api->install_inline_hook(
            at(g_armada, kShipDisplayCursorOverRva),
            reinterpret_cast<void*>(&ship_display_cursor_over_hook),
            kShipDisplayCursorOverHookLength,
            kExpectedShipDisplayCursorOver,
            &g_ship_display_cursor_over_hook);
    const bool construction_bar_installed =
        construction_bar_render_supported &&
        construction_rig_get_supported &&
        find_game_object_supported && api->install_inline_hook(
            at(g_armada, kConstructionBarRenderRva),
            reinterpret_cast<void*>(&construction_bar_render_hook),
            kConstructionBarRenderHookLength,
            kExpectedConstructionBarRender,
            &g_construction_bar_render_hook);
    g_legacy_construction_bar_target_runtime_ready =
        construction_bar_installed;
    if (!construction_bar_installed) {
        log_line("A1 construction-rig progress target bridge unavailable; "
                 "ordinary native ConstructionBar behaviour retained");
    }
    g_legacy_ship_display_render_runtime_ready = render_installed;
    if (!render_installed) {
        log_line("A1 ShipDisplay black-mask render boundary unavailable; "
                 "one-panel rectangle/background aliases remain enabled");
    }
    const bool background_render_installed =
        background_render_supported && api->install_inline_hook(
            at(g_armada, kStandardBackgroundRenderRva),
            reinterpret_cast<void*>(&standard_background_render_hook),
            kStandardBackgroundRenderHookLength,
            kExpectedStandardBackgroundRender,
            &g_standard_background_render_hook);
    g_legacy_ship_display_background_runtime_ready =
        background_render_installed;
    g_legacy_ship_display_opaque_black_runtime_ready =
        background_render_installed && rectangle_draw_supported;
    g_legacy_ship_display_race_icon_background_runtime_ready =
        background_render_installed &&
        g_legacy_resource_panel_runtime_ready &&
        team_colour_supported;
    if (!background_render_installed) {
        log_line("A1 ShipDisplay background-layer render boundary "
                 "unavailable; identity/RaceIcon geometry aliases remain "
                 "enabled");
    } else {
        if (!g_legacy_ship_display_opaque_black_runtime_ready) {
            log_line("A1 ShipDisplay opaque black-mask renderer "
                     "unavailable; Fleet Ops translucency remains");
        }
        if (!g_legacy_ship_display_race_icon_background_runtime_ready) {
            log_line("A1 ShipDisplay team-colour race_icon_bar renderer "
                     "unavailable; native insignia remains enabled");
        }
    }
    const bool rectangle_installed = api->install_inline_hook(
        at(g_armada, kDisplayInterfaceLoadRectangleRva),
        reinterpret_cast<void*>(&display_interface_load_rectangle_hook),
        kDisplayInterfaceLoadRectangleHookLength,
        kExpectedDisplayInterfaceLoadRectangle,
        &g_display_interface_load_rectangle_hook);
    const bool background_installed = api->install_inline_hook(
        at(g_armada,
           kStandardBackgroundInitializeConfigurationRva),
        reinterpret_cast<void*>(&standard_background_initialize_hook),
        kStandardBackgroundInitializeConfigurationHookLength,
        kExpectedStandardBackgroundInitializeConfiguration,
        &g_standard_background_initialize_hook);
    if (!rectangle_installed || !background_installed ||
        !render_installed || !cursor_installed ||
        !background_render_installed) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ShipDisplay adapter installed only part of its render/key "
            "boundaries (render=%d cursor=%d constructionBar=%d artwork=%d "
            "rectangle=%d background=%d)",
            render_installed ? 1 : 0,
            cursor_installed ? 1 : 0,
            construction_bar_installed ? 1 : 0,
            background_render_installed ? 1 : 0,
            rectangle_installed ? 1 : 0,
            background_installed ? 1 : 0);
        log_line(message);
    }
    return rectangle_installed && background_installed;
}

bool install_legacy_neutral_race_registry(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->patch_call) return false;
    InterlockedExchange(&g_synthetic_neutral_race_index, -1);
    if (!a1_native_entry_supported(
            "ParameterDB::GetInt", kParameterDbGetIntRva,
            kExpectedParameterDbGetInt) ||
        !a1_native_entry_supported(
            "ParameterDB::GetString", kParameterDbGetStringRva,
            kExpectedParameterDbGetString)) {
        return false;
    }

    // Patch the entry lookup first. If the count patch cannot be installed,
    // the wrapper stays inert because no synthetic index is ever published.
    if (!api->patch_call(
            at(g_armada, kRaceInitAllRaceEntryCallRva),
            reinterpret_cast<void*>(&a2fo_a1_race_entry_lookup_hook),
            kExpectedRaceInitAllRaceEntryCall,
            sizeof(kExpectedRaceInitAllRaceEntryCall))) {
        return false;
    }
    return api->patch_call(
        at(g_armada, kRaceInitAllNumberOfRacesCallRva),
        reinterpret_cast<void*>(&a2fo_a1_race_count_lookup_hook),
        kExpectedRaceInitAllNumberOfRacesCall,
        sizeof(kExpectedRaceInitAllNumberOfRacesCall));
}

bool install_legacy_moon_resource_defaults(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->patch_call) return false;
    if (!a1_native_entry_supported(
            "cPrjID::GetOdfName", kProjectIdGetOdfNameRva,
            kExpectedProjectIdGetOdfName)) {
        log_line("A2 Classic moon defaults rejected cPrjID::GetOdfName "
                 "signature");
        return false;
    }
    g_project_id_get_odf_name = at(g_armada, kProjectIdGetOdfNameRva);
    const bool installed = api->patch_call(
        at(g_armada, kGameObjectClassResourceLookupCallRva),
        reinterpret_cast<void*>(&a2fo_a1_game_object_resource_lookup_hook),
        kExpectedGameObjectClassResourceLookupCall,
        sizeof(kExpectedGameObjectClassResourceLookupCall));
    if (!installed) g_project_id_get_odf_name = nullptr;
    return installed;
}

bool install_legacy_physics_defaults(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->patch_call) return false;
    if (!a1_native_entry_supported(
            "ParameterDB float lookup", kParameterDbGetFloatRva,
            kExpectedParameterDbGetFloat) ||
        !a1_native_entry_supported(
            "ParameterDB::GetInt", kParameterDbGetIntRva,
            kExpectedParameterDbGetInt) ||
        !a1_native_entry_supported(
            "ParameterDB::GetString", kParameterDbGetStringRva,
            kExpectedParameterDbGetString) ||
        !a1_native_entry_supported(
            "physics float cascade", kPhysicsFloatCascadeRva,
            kExpectedPhysicsFloatCascade) ||
        !a1_native_entry_supported(
            "physics integer cascade", kPhysicsIntCascadeRva,
            kExpectedPhysicsIntCascade) ||
        !a1_native_entry_supported(
            "cPrjID::GetOdfName", kProjectIdGetOdfNameRva,
            kExpectedProjectIdGetOdfName) ||
        !a1_signature_matches(
            kPhysicsClassCombatSpeedValidationCallRva,
            kExpectedPhysicsClassCombatSpeedValidationCall) ||
        !a1_signature_matches(
            kPhysicsClassCombatSpeedValueCallRva,
            kExpectedPhysicsClassCombatSpeedValueCall) ||
        !a1_signature_matches(
            kPhysicsClassImpulseSpeedLookupCallRva,
            kExpectedPhysicsClassImpulseSpeedLookupCall) ||
        !a1_signature_matches(
            kPhysicsClassWarpSpeedLookupCallRva,
            kExpectedPhysicsClassWarpSpeedLookupCall) ||
        !a1_signature_matches(
            kPhysicsClassModelLookupCallRva,
            kExpectedPhysicsClassModelLookupCall)) {
        log_line("A1 physics defaults rejected an unsupported native "
                 "signature");
        return false;
    }
    for (const auto& patch : kSmoothFloatCallPatches) {
        const void* address = at(g_armada, patch.rva);
        if (!readable_range(address, patch.expected.size()) ||
            std::memcmp(
                address, patch.expected.data(), patch.expected.size()) != 0) {
            log_line("A1 smooth physics defaults rejected an unsupported "
                     "parameter-call signature");
            return false;
        }
    }
    const void* integer_call = at(
        g_armada, kSmoothIntegerCallPatch.rva);
    if (!readable_range(
            integer_call, kSmoothIntegerCallPatch.expected.size()) ||
        std::memcmp(
            integer_call, kSmoothIntegerCallPatch.expected.data(),
            kSmoothIntegerCallPatch.expected.size()) != 0) {
        log_line("A1 smooth physics defaults rejected an unsupported "
                 "integer-call signature");
        return false;
    }
    g_project_id_get_odf_name = at(g_armada, kProjectIdGetOdfNameRva);

    if (!api->patch_call(
            at(g_armada, kPhysicsClassCombatSpeedValidationCallRva),
            reinterpret_cast<void*>(&a2fo_a1_resolve_physics_speed),
            kExpectedPhysicsClassCombatSpeedValidationCall,
            sizeof(kExpectedPhysicsClassCombatSpeedValidationCall))) {
        return false;
    }
    if (!api->patch_call(
            at(g_armada, kPhysicsClassCombatSpeedValueCallRva),
            reinterpret_cast<void*>(&a2fo_a1_resolve_physics_speed),
            kExpectedPhysicsClassCombatSpeedValueCall,
            sizeof(kExpectedPhysicsClassCombatSpeedValueCall))) {
        return false;
    }
    if (!api->patch_call(
            at(g_armada, kPhysicsClassImpulseSpeedLookupCallRva),
            reinterpret_cast<void*>(&a2fo_a1_resolve_physics_speed),
            kExpectedPhysicsClassImpulseSpeedLookupCall,
            sizeof(kExpectedPhysicsClassImpulseSpeedLookupCall)) ||
        !api->patch_call(
            at(g_armada, kPhysicsClassWarpSpeedLookupCallRva),
            reinterpret_cast<void*>(&a2fo_a1_resolve_physics_speed),
            kExpectedPhysicsClassWarpSpeedLookupCall,
            sizeof(kExpectedPhysicsClassWarpSpeedLookupCall))) {
        return false;
    }
    if (!api->patch_call(
        at(g_armada, kPhysicsClassModelLookupCallRva),
        reinterpret_cast<void*>(&a2fo_a1_physics_model_lookup_hook),
        kExpectedPhysicsClassModelLookupCall,
        sizeof(kExpectedPhysicsClassModelLookupCall))) {
        return false;
    }
    for (const auto& patch : kSmoothFloatCallPatches) {
        if (!api->patch_call(
                at(g_armada, patch.rva),
                reinterpret_cast<void*>(&a2fo_a1_translate_smooth_float),
                patch.expected.data(), patch.expected.size())) {
            return false;
        }
    }
    if (!api->patch_call(
            at(g_armada, kSmoothIntegerCallPatch.rva),
            reinterpret_cast<void*>(&a2fo_a1_translate_smooth_integer),
            kSmoothIntegerCallPatch.expected.data(),
            kSmoothIntegerCallPatch.expected.size())) {
        return false;
    }
    return true;
}

void log_craft_level_up_race_default(
    const void* craft, const void* return_address,
    std::uintptr_t force_level_up, const void* race) noexcept {
    if (race) return;

    const LONG incident = InterlockedIncrement(&g_missing_craft_race_count);
    if (incident > 32) {
        if (incident == 33) {
            log_line("Further A2 Classic neutral Race defaults suppressed");
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
        "A2 Classic neutral Race default #%ld: odf='%s', craft=%p, "
        "handle=%lu, "
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

bool install_craft_level_up_race_default(
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
        g_a2fo_a1_craft_level_up_race_continuation =
            at(g_fleet_ops,
               kCraftLevelUpRaceRva + kCraftLevelUpRaceHookLength);
    } else {
        g_a2fo_a1_craft_level_up_race_gateway = nullptr;
        g_a2fo_a1_craft_level_up_race_continuation = nullptr;
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

extern "C" void* __cdecl a2fo_a1_energy_bar_colour_value(
    void* bar, void* output) {
    return legacy_energy_bar_colour_value(bar, output);
}

extern "C" void* __cdecl a2fo_a1_energy_bar_values_value(
    void* bar, void* output) {
    return legacy_energy_bar_values_value(bar, output);
}

extern "C" std::uintptr_t __cdecl a2fo_a1_load_gui_sprite_tables(
    void* parser, void* database, const char* primary_filename) {
    return load_gui_sprite_tables(parser, database, primary_filename);
}

extern "C" void __cdecl a2fo_a1_configure_gui_parameter_db(
    void* parameter_db, const char* configuration_filename) {
    configure_gui_parameter_db(parameter_db, configuration_filename);
}

extern "C" __declspec(dllexport)
bool __cdecl a2fo_a1_render_legacy_resource_panel(void* panel) {
    return render_legacy_resource_panel(panel);
}

extern "C" std::uintptr_t __cdecl a2fo_a1_resolve_race_count(
    void* parameter_db, const char* key, std::int32_t* output,
    std::int32_t default_value) {
    return resolve_race_count(parameter_db, key, output, default_value);
}

extern "C" std::uintptr_t __cdecl a2fo_a1_resolve_race_entry(
    void* parameter_db, const char* key, char* output,
    std::uintptr_t output_size, const char* default_value) {
    return resolve_race_entry(
        parameter_db, key, output, output_size, default_value);
}

extern "C" std::uintptr_t __cdecl a2fo_a1_resolve_game_object_resource(
    void* parameter_db, const char* key, char* output,
    std::uintptr_t output_size, const char* default_value) {
    return resolve_game_object_resource(
        parameter_db, key, output, output_size, default_value);
}

extern "C" std::uintptr_t __cdecl a2fo_a1_resolve_physics_speed(
    void* parameter_db, std::uintptr_t context, const char* key,
    float* output, float default_value) {
    return resolve_physics_speed(
        parameter_db, context, key, output, default_value);
}

extern "C" std::uintptr_t __cdecl a2fo_a1_resolve_physics_model(
    void* parameter_db, const char* key, char* output,
    std::uintptr_t output_size, const char* default_value) {
    return resolve_physics_model(
        parameter_db, key, output, output_size, default_value);
}

extern "C" void __cdecl a2fo_a1_translate_smooth_float(
    void* primary_db, void* fallback_db, const char* key, float* output) {
    translate_smooth_float(primary_db, fallback_db, key, output);
}

extern "C" void __cdecl a2fo_a1_translate_smooth_integer(
    void* primary_db, void* fallback_db, const char* key,
    std::int32_t* output) {
    translate_smooth_integer(primary_db, fallback_db, key, output);
}

extern "C" std::uintptr_t __cdecl a2fo_a1_read_rtime_class_name(
    void* file_reader, void* output, std::uint32_t requested_size) {
    return read_rtime_class_name(file_reader, output, requested_size);
}

extern "C" std::uintptr_t __cdecl a2fo_a1_load_game_objects(
    void* file_reader) {
    return load_game_objects(file_reader);
}

extern "C" std::uintptr_t __cdecl a2fo_a1_load_a2_craft_class_table(
    void* file_reader) {
    return load_a2_craft_class_table(file_reader);
}

extern "C" std::uintptr_t __cdecl a2fo_a1_load_ai_mission(
    void* file_reader) {
    return load_ai_mission(file_reader);
}

extern "C" std::uintptr_t __cdecl a2fo_a1_load_map_details(
    const char* filename) {
    return reinterpret_cast<std::uintptr_t>(load_map_details(filename));
}

extern "C" std::uintptr_t __cdecl a2fo_a1_load_selected_map_details(
    const char* filename) {
    return reinterpret_cast<std::uintptr_t>(
        load_map_details(filename, true));
}

extern "C" std::uintptr_t __cdecl a2fo_a1_resolve_aip_lookup(
    void* manager, const char* requested_name) {
    return reinterpret_cast<std::uintptr_t>(
        resolve_aip_lookup(manager, requested_name));
}

extern "C" void __cdecl a2fo_a1_report_missing_aip_technology_unit(
    const char* aip_name, const char* unit_name) {
    report_missing_aip_technology_unit(aip_name, unit_name);
}

extern "C" void __cdecl a2fo_a1_log_craft_level_up_race(
    const void* craft, const void* return_address,
    std::uintptr_t force_level_up, const void* race) {
    log_craft_level_up_race_default(
        craft, return_address, force_level_up, race);
}

extern "C" void __cdecl a2fo_a1_prepare_starbase_officer_quarters(
    void* starbase) {
    prepare_starbase_officer_quarters(starbase);
}

extern "C" void __cdecl a2fo_a1_run_to_the_death_check(
    void* game_type, const void* return_address) {
    run_to_the_death_check(game_type, return_address);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->install_inline_hook || !api->patch_call || !api->patch_jump ||
        !api->extension_root_count || !api->extension_root ||
        !api->register_classlabel_alias ||
        !A2FO_MODULE_API_HAS(api, register_classlabel_odf_defaults) ||
        !A2FO_MODULE_API_HAS(api, register_odf_overlay_directory) ||
        !A2FO_MODULE_API_HAS(api, register_producer_event_handler) ||
        !A2FO_MODULE_API_HAS(
            api, register_game_object_class_loaded_handler) ||
        !A2FO_MODULE_API_HAS(api, register_race_loaded_handler) ||
        !A2FO_MODULE_API_HAS(api, register_race_odf_defaults) ||
        api->api_revision < 17 ||
        (api->capabilities & A2FO_CAP_CLASSLABEL_ODF_DEFAULTS) == 0 ||
        (api->capabilities & A2FO_CAP_ODF_OVERLAY_DIRECTORIES) == 0 ||
        (api->capabilities & A2FO_CAP_PRODUCER_EVENTS) == 0 ||
        (api->capabilities & A2FO_CAP_GAME_OBJECT_CLASS_LOADED) == 0 ||
        (api->capabilities & A2FO_CAP_RACE_LOADED) == 0 ||
        (api->capabilities & A2FO_CAP_RACE_ODF_DEFAULTS) == 0 ||
        !api->register_classlabel_odf_defaults ||
        !api->register_odf_overlay_directory ||
        !api->register_producer_event_handler ||
        !api->register_game_object_class_loaded_handler ||
        !api->register_race_loaded_handler ||
        !api->register_race_odf_defaults) {
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
    if (!api->register_classlabel_odf_defaults(
            kModuleName, "mining", kMiningOdfDefaults.data(),
            static_cast<std::uint32_t>(kMiningOdfDefaults.size()))) {
        log_line("mining ODF-default registration failed");
        g_api = nullptr;
        return false;
    }
    if (!api->register_classlabel_odf_defaults(
            kModuleName, "shipyard", kShipyardOdfDefaults.data(),
            static_cast<std::uint32_t>(kShipyardOdfDefaults.size()))) {
        log_line("shipyard ODF-default registration failed");
        g_api = nullptr;
        return false;
    }
    if (!api->register_classlabel_odf_defaults(
            kModuleName, "research", kResearchOdfDefaults.data(),
            static_cast<std::uint32_t>(kResearchOdfDefaults.size()))) {
        log_line("research ODF-default registration failed");
        g_api = nullptr;
        return false;
    }
    if (!api->register_game_object_class_loaded_handler(
            kModuleName, kLegacyMenuCapabilityOdfFields.data(),
            static_cast<std::uint32_t>(
                kLegacyMenuCapabilityOdfFields.size()),
            &legacy_menu_capability_class_loaded_handler, nullptr)) {
        log_line("legacy menu-capability default registration failed");
        g_api = nullptr;
        return false;
    }
    if (!api->register_odf_overlay_directory(
            kModuleName, "Addon", A2FO_ODF_OVERLAY_OVERRIDE)) {
        log_line("Addon ODF overlay registration failed");
        g_api = nullptr;
        return false;
    }
    if (!apply_legacy_native_resource_limits()) {
        log_line("A1 legacy resource-maximum bridge failed");
        g_api = nullptr;
        return false;
    }
    InterlockedExchange(
        &g_legacy_resource_maximum_post_config_pending, 1);
    g_legacy_starting_resource_defaults =
        load_starting_resource_defaults();
    if (!register_starting_resource_defaults(
            api, g_legacy_starting_resource_defaults)) {
        log_line("A1 Race starting-resource default registration failed");
        g_api = nullptr;
        return false;
    }
    if (!api->register_race_loaded_handler(
            kModuleName, kLegacyRaceFields.data(),
            static_cast<std::uint32_t>(kLegacyRaceFields.size()),
            &legacy_race_loaded_handler, nullptr)) {
        log_line("Legacy race-menu callback registration failed");
        g_api = nullptr;
        return false;
    }
    reset_legacy_race_menu_state();
    g_race_menu_callback_ready = true;
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
    g_legacy_scout_menu_capability_count = 0;
    g_legacy_station_menu_capability_count = 0;
    g_legacy_moon_menu_capability_count = 0;
    g_legacy_explore_command_collision_count = 0;
    g_gui_sprite_table_load_count = 0;
    g_legacy_cursor_fallback_count = 0;
    g_legacy_tactical_cursor_fallback_count = 0;
    g_legacy_cursor_fallback_failure_reported = 0;
    g_legacy_cursor_override_required = false;
    g_legacy_tactical_cursor_override_required = false;
    g_legacy_cursor_override_source.clear();
    g_legacy_tactical_native_overview_cursors = {};
    g_legacy_gameplay_ui_active = 0;
    g_legacy_tooltip_background_fallback = 0;
    g_legacy_tooltip_text_fallback = 0;
    g_legacy_tooltip_frame_fallback = 0;
    g_legacy_tooltip_colour_reported = 0;
    g_legacy_tooltip_background_colour = {};
    g_legacy_control_button_rect_count = 0;
    g_legacy_control_button_adapter_reported = 0;
    g_legacy_control_background_available = 0;
    g_legacy_control_black_mask_available = 0;
    g_legacy_control_black_mask_reported = 0;
    g_legacy_ship_display_black_mask_available = 0;
    g_legacy_ship_display_black_mask_reported = 0;
    g_legacy_ship_display_wireframe_available = 0;
    g_legacy_ship_display_wireframe_reported = 0;
    g_legacy_ship_display_race_icon_available = 0;
    g_legacy_ship_display_race_icon_reported = 0;
    g_legacy_ship_display_race_icon_background_reported = 0;
    g_legacy_ship_display_identity_available = 0;
    g_legacy_ship_display_identity_reported = 0;
    g_legacy_ship_display_identity_text_available = 0;
    g_legacy_ship_display_identity_text_reported = 0;
    g_legacy_ship_display_identity_background_available = 0;
    g_legacy_ship_display_identity_background_reported = 0;
    g_legacy_ship_display_system_strip_reported = 0;
    g_legacy_ship_display_progress_available = 0;
    g_legacy_ship_display_progress_reported = 0;
    g_legacy_ship_display_multi_layout_available = 0;
    g_legacy_ship_display_multi_layout_reported = 0;
    g_legacy_ship_display_multi_constructor_reported = 0;
    g_legacy_ship_display_multi_crew_reported = 0;
    g_legacy_ship_display_energy_available = 0;
    g_legacy_ship_display_energy_reported = 0;
    g_legacy_speed_control_layout_available = 0;
    g_legacy_speed_queue_layout_reported = 0;
    g_legacy_speed_special_layout_reported = 0;
    g_legacy_speed_special_layout_count = 0;
    g_legacy_speed_transport_layout_reported = 0;
    g_legacy_popup_control_diagnostic_reported = 0;
    g_legacy_officer_root_control_reported = 0;
    g_legacy_officer_root_repair_report_count = 0;
    g_legacy_officer_root_order_latched_reported = 0;
    g_legacy_officer_root_return_reported = 0;
    g_legacy_officer_root_direct_dispatch_reported = 0;
    g_legacy_officer_build_control_hidden_reported = 0;
    g_legacy_speed_separator_available = 0;
    g_legacy_speed_cursor_reported = 0;
    g_legacy_construction_bar_target_reported = 0;
    g_legacy_construction_rig_progress_diagnostic_reported = 0;
    g_legacy_control_black_mask_runtime_ready = false;
    g_legacy_ship_display_render_runtime_ready = false;
    g_legacy_ship_display_background_runtime_ready = false;
    g_legacy_ship_display_opaque_black_runtime_ready = false;
    g_legacy_ship_display_race_icon_background_runtime_ready = false;
    g_legacy_ship_display_text_runtime_ready = false;
    g_legacy_construction_bar_target_runtime_ready = false;
    g_legacy_cinematic_button_runtime_ready = false;
    g_legacy_cinematic_caption_reported = 0;
    g_legacy_caption_text_component = nullptr;
    g_legacy_officer_root_control_runtime_ready = false;
    g_legacy_selected_object = nullptr;
    g_legacy_officer_root_button = nullptr;
    g_legacy_officer_root_target = nullptr;
    g_legacy_officer_root_enabled = false;
    g_legacy_officer_root_dispatch_producer = nullptr;
    g_legacy_officer_root_dispatch_target = nullptr;
    g_legacy_officer_root_pending_producer = nullptr;
    g_legacy_officer_root_direct_dispatch_active = false;
    g_legacy_officer_root_mode_info = {};
    g_legacy_resource_render_bridge_active = 0;
    g_legacy_ship_display_alias_report_count = 0;
    g_legacy_control_button_rects = {};
    g_legacy_control_panel_rect = {};
    g_legacy_control_background_rect = {};
    g_legacy_control_black_rect = {};
    g_legacy_control_background_storage = {};
    g_legacy_control_background_parent = nullptr;
    g_legacy_control_background_constructed = false;
    g_legacy_speed_panel_rect = {};
    g_legacy_speed_queue_screen_rects = {};
    g_legacy_speed_transport_screen_rect = {};
    g_legacy_speed_separator_piece = {};
    g_legacy_speed_background_pieces = {};
    g_legacy_speed_background_piece_count = 0;
    g_legacy_speed_panel_available = 0;
    g_legacy_speed_panel_reported = 0;
    g_legacy_speed_panel_runtime_ready = false;
    g_legacy_cinematic_panel_rect = {};
    g_legacy_cinematic_background_panel_rect = {};
    g_legacy_cinematic_background_rect = {};
    g_legacy_cinematic_display_rect = {};
    g_legacy_cinematic_menu_button_rect = {};
    g_legacy_cinematic_comm_button_rect = {};
    g_legacy_cinematic_menu_button_sprite_name = {};
    g_legacy_cinematic_menu_border_sprite_name = {};
    g_legacy_cinematic_comm_button_sprite_name = {};
    g_legacy_cinematic_comm_border_sprite_name = {};
    g_legacy_cinematic_button_art_owner = nullptr;
    g_legacy_ship_display_black_rect = {};
    g_legacy_ship_display_wireframe_rect = {};
    g_legacy_ship_display_race_icon_rect = {};
    g_legacy_ship_display_race_icon_display_rect = {};
    g_legacy_ship_display_class_rect = {};
    g_legacy_ship_display_name_rect = {};
    g_legacy_ship_display_crew_rect = {};
    g_legacy_ship_display_crew_dot_rect = {};
    g_legacy_ship_display_officer_rect = {};
    g_legacy_ship_display_crew_label_rect = {};
    g_legacy_ship_display_officer_label_rect = {};
    g_legacy_ship_display_progress_rect = {};
    g_legacy_ship_display_crew_label_key = {};
    g_legacy_ship_display_officer_label_key = {};
    g_legacy_ship_display_crew_dot_sprite_name = {};
    g_legacy_ship_display_identity_background_pieces = {};
    g_legacy_ship_display_identity_background_piece_count = 0;
    g_legacy_cinematic_layout_available = 0;
    g_legacy_cinematic_layout_reported = 0;
    g_legacy_cinematic_buttons_available = 0;
    g_legacy_cinematic_buttons_reported = 0;
    g_legacy_resource_panel_rect = {};
    g_legacy_resource_text_rects = {};
    g_legacy_resource_icon_rects = {};
    g_legacy_resource_icon_names = {};
    g_legacy_resource_background_pieces = {};
    g_legacy_resource_background_piece_counts = {};
    g_legacy_resource_panel_available = 0;
    g_legacy_resource_panel_reported = 0;
    g_legacy_resource_background_suppression_reported = 0;
    g_legacy_resource_panel_runtime_ready = false;
    g_virtual_directory_normalization_count = 0;
    g_legacy_moon_resource_default_count = 0;
    g_legacy_physics_combat_speed_default_count = 0;
    g_legacy_physics_impulse_speed_translation_count = 0;
    g_legacy_physics_warp_speed_translation_count = 0;
    g_legacy_physics_model_default_count = 0;
    g_legacy_smooth_profile_translation_count = 0;
    g_neutral_race_registry_default_count = 0;
    g_legacy_team_color_apply_count = 0;
    g_a1_bzn_map_details_count = 0;
    g_a1_mdf_start_location_count = 0;
    g_a1_bzn_world_bounds_count = 0;
    g_a1_bzn_object_tail_load_count = 0;
    g_a1_selected_map_active = 0;
    g_a1_relationship_restore_count = 0;
    g_legacy_aip_name_fallback_count = 0;
    g_missing_aip_technology_unit_count = 0;
    InterlockedExchange(&g_synthetic_neutral_race_index, -1);
    g_project_id_get_odf_name = nullptr;
    const bool safe_mode_enabled = settings.safe_mode;
    if (safe_mode_enabled) {
        log_line("A1Compat safe mode active; risky hooks disabled; legacy "
                 "race-menu fallback retained");
    }
    const bool essential_gui_sprite_loader_enabled =
        install_essential_gui_sprite_loader(api);
    if (!essential_gui_sprite_loader_enabled) {
        log_line("Essential a2_gui_global.spr loader installation failed; "
                 "A1Compat cannot guarantee the required A2 interface "
                 "sprite database");
        g_race_menu_callback_ready = false;
        reset_legacy_race_menu_state();
        g_api = nullptr;
        g_armada = nullptr;
        g_fleet_ops = nullptr;
        return false;
    }
    g_legacy_cursor_override_required =
        detect_incompatible_legacy_cursor_table(
            g_legacy_cursor_override_source,
            &g_legacy_tactical_cursor_override_required);
    if (g_legacy_cursor_override_required ||
        g_legacy_tactical_cursor_override_required) {
        char message[720]{};
        std::snprintf(
            message, sizeof(message),
            "Detected incompatible legacy cursor overview entries in %s; "
            "scheduling loaded-cursor fallbacks (GUI=%s, tactical=%s)",
            g_legacy_cursor_override_source.c_str(),
            g_legacy_cursor_override_required ? "yes" : "no",
            g_legacy_tactical_cursor_override_required ? "yes" : "no");
        log_line(message);
    }
    const bool legacy_default_cursor_fallback_enabled =
        install_legacy_default_cursor_fallback(api);
    if (!legacy_default_cursor_fallback_enabled) {
        log_line("A1 legacy default-cursor fallback installation failed; "
                 "an unflagged or oversized standard cursor may remain "
                 "invisible");
    }
    const bool legacy_tactical_cursor_fallback_enabled =
        install_legacy_tactical_cursor_fallback(api);
    if (!legacy_tactical_cursor_fallback_enabled) {
        log_line("A1 legacy tactical-cursor fallback installation failed; "
                 "the main map overview cursor may remain invisible");
    }
    const bool legacy_gameplay_ui_scaling_enabled =
        install_legacy_gameplay_ui_scaling(api);
    if (!legacy_gameplay_ui_scaling_enabled) {
        log_line("A1 gameplay UI 640x480 scaling bridge installation "
                 "failed; raw legacy CFG rectangles will retain A2's "
                 "1600x1200 reference size");
    }
    const bool legacy_tooltip_adapter_enabled =
        install_legacy_tooltip_adapter(api);
    if (!legacy_tooltip_adapter_enabled) {
        log_line("A1 tooltip presentation adapter installation failed; "
                 "raw legacy CFGs may retain an unreadable or collapsed "
                 "A2 tooltip frame");
    }
    const bool legacy_cinematic_view_adapter_enabled =
        install_legacy_cinematic_view_adapter(api);
    if (!legacy_cinematic_view_adapter_enabled) {
        log_line("A1 CinematicView layout adapter installation failed; "
                 "the live 3D viewport may retain Fleet Operations "
                 "geometry");
    }
    const bool legacy_control_panel_adapter_enabled =
        install_legacy_control_panel_adapter(api);
    if (!legacy_control_panel_adapter_enabled) {
        log_line("A1 ControlPanel button-layout adapter installation "
                 "failed; PopupPalette controls will retain the Fleet "
                 "Operations layout");
    }
    const bool legacy_resource_panel_render_enabled =
        install_legacy_resource_panel_adapter();
    if (!legacy_resource_panel_render_enabled) {
        log_line("A1 ResourcePanel render adapter installation failed; "
                 "legacy resource artwork will remain unavailable");
    }
    g_legacy_resource_panel_runtime_ready =
        legacy_control_panel_adapter_enabled &&
        legacy_resource_panel_render_enabled &&
        readable_range(
            at(g_armada, kInterfaceSpriteDatabaseGetRva),
            sizeof(kExpectedInterfaceSpriteDatabaseGet)) &&
        std::memcmp(
            at(g_armada, kInterfaceSpriteDatabaseGetRva),
            kExpectedInterfaceSpriteDatabaseGet,
            sizeof(kExpectedInterfaceSpriteDatabaseGet)) == 0 &&
        readable_range(
            at(g_fleet_ops, kFoSpriteSetColourRva),
            sizeof(kExpectedFoSpriteSetColour)) &&
        std::memcmp(
            at(g_fleet_ops, kFoSpriteSetColourRva),
            kExpectedFoSpriteSetColour,
            sizeof(kExpectedFoSpriteSetColour)) == 0 &&
        readable_range(
            at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
            sizeof(kExpectedFoSpriteDrawScaled2D)) &&
        std::memcmp(
            at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
            kExpectedFoSpriteDrawScaled2D,
            sizeof(kExpectedFoSpriteDrawScaled2D)) == 0;
    g_legacy_speed_panel_runtime_ready =
        g_legacy_resource_panel_runtime_ready;
    if (!g_legacy_speed_panel_runtime_ready) {
        log_line("A1 SpeedRail artwork adapter unavailable");
    }
    const bool legacy_explore_command_bridge_enabled =
        install_legacy_explore_command_bridge(api);
    if (!legacy_explore_command_bridge_enabled) {
        log_line("A1 scout/Explore command basename-collision bridge "
                 "installation failed; Explore may remain unavailable");
    }
    const bool legacy_team_color_translation_available =
        install_legacy_team_color_translation(api);
    if (!legacy_team_color_translation_available) {
        log_line("A1 team-colour translation installation failed; legacy "
                 "IA/minimap colours remain unavailable");
    }
    const bool legacy_virtual_directory_normalizer_enabled =
        install_legacy_virtual_directory_normalizer(api);
    if (!legacy_virtual_directory_normalizer_enabled) {
        log_line("Legacy .\\ virtual-directory normalizer installation "
                 "failed; dotted Armada 1 AI paths remain unsupported");
    }
    const bool legacy_moon_resource_defaults_enabled =
        install_legacy_moon_resource_defaults(api);
    if (!legacy_moon_resource_defaults_enabled) {
        log_line("A2 Classic Scrap/moon missing-code defaults installation "
                 "failed; inherited A2 maps may remain incompatible");
    }
    const bool legacy_physics_defaults_enabled =
        install_legacy_physics_defaults(api);
    if (!legacy_physics_defaults_enabled) {
        log_line("A1 physics missing-code defaults installation failed; "
                 "legacy craft may remain stationary");
    }
    const bool legacy_neutral_race_registry_enabled =
        install_legacy_neutral_race_registry(api);
    if (!legacy_neutral_race_registry_enabled) {
        log_line("A2 Classic neutral Race registry default installation "
                 "failed; A1 neutral sides may remain raceless");
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
    bool legacy_aip_name_bridge_enabled = false;
    bool legacy_aip_missing_unit_guard_enabled = false;
    bool a1_bzn_map_bounds_bridge_enabled = false;
    bool a1_bzn_ai_mission_bridge_enabled = false;
    bool a1_bzn_runtime_class_bridge_enabled = false;
    bool a1_relationship_bridge_enabled = false;
    bool craft_level_up_race_default_enabled = false;
    bool officer_quarter_compatibility_enabled = false;
    bool starbase_policy_system_enabled = false;
    bool palette_race_filter_enabled = false;

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
            log_line("Nebula sprite-node guard installation failed");
        }
        legacy_aip_name_bridge_enabled =
            install_legacy_aip_name_bridge(api);
        if (!legacy_aip_name_bridge_enabled) {
            log_line("Armada 1 Instant Action AIP name bridge installation "
                     "failed");
        }
        legacy_aip_missing_unit_guard_enabled =
            install_legacy_aip_missing_unit_guard(api);
        if (!legacy_aip_missing_unit_guard_enabled) {
            log_line("A1/A2 unresolved AIP technology-unit guard "
                     "installation failed");
        }
        a1_bzn_map_bounds_bridge_enabled =
            install_a1_bzn_map_bounds_bridge(api);
        if (!a1_bzn_map_bounds_bridge_enabled) {
            log_line("Armada 1 BZN map-bounds bridge installation failed; "
                     "Instant Action may report a 0x0 map size");
        }
        a1_bzn_ai_mission_bridge_enabled =
            install_a1_bzn_ai_mission_bridge(api);
        if (!a1_bzn_ai_mission_bridge_enabled) {
            log_line("Armada 1 BZN AiMission bridge installation failed");
        }
        a1_bzn_runtime_class_bridge_enabled =
            install_a1_bzn_runtime_class_bridge(api);
        if (!a1_bzn_runtime_class_bridge_enabled) {
            log_line("Armada 1 BZN runtime-class width bridge installation "
                     "failed");
        }
        a1_relationship_bridge_enabled =
            install_a1_relationship_bridge(api);
        if (!a1_relationship_bridge_enabled) {
            log_line("A1 Instant Action relationship bridge installation "
                     "failed");
        }
        craft_level_up_race_default_enabled =
            install_craft_level_up_race_default(api);
        if (!craft_level_up_race_default_enabled) {
            log_line("Craft_mLevelUp neutral Race default installation "
                     "failed");
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
        palette_race_filter_enabled =
            install_producer_palette_race_filter(api);
        if (!palette_race_filter_enabled) {
            log_line("A1 Producer palette race filter installation failed");
        }
    } else {
        g_producer_events_ready = false;
    }

    // Install this last so every earlier UI adapter can preflight the native
    // StandardBackground initializer before the checked prefix-alias detour
    // is present.
    const bool legacy_ship_display_adapter_enabled =
        install_legacy_ship_display_adapter(api);
    if (!legacy_ship_display_adapter_enabled) {
        log_line("A1 ShipDisplay one-panel key adapter installation failed; "
                 "the A2 low/middle/tall layouts may remain displaced");
    }
    g_legacy_resource_panel_runtime_ready =
        g_legacy_resource_panel_runtime_ready &&
        legacy_ship_display_adapter_enabled;
    if (!g_legacy_resource_panel_runtime_ready) {
        log_line("A1 resource-strip adapter unavailable");
    }

    char message[1500]{};
    std::snprintf(
        message, sizeof(message),
        "Armada 1 compatibility initialized: wingman -> craft; A2 scout "
        "and station menu defaults registered; A1 scout/Explore command "
        "collision bridge %s; Addon ODF overlay; essential "
        "a2_gui_global.spr loader %s; legacy cursor fallback GUI/tactical "
        "%s/%s; A1 "
        "gameplay UI scaling %s; A1 "
        "ControlPanel adapter %s; A1 SpeedRail artwork %s; A1 "
        "CinematicView adapter %s; A1 ShipDisplay adapter %s; A1 "
        "resource strip %s; legacy "
        "team-colour "
        "translation %s; legacy .\\ virtual "
        "directories %s; A2 Scrap/moon missing-code defaults %s; A1 physics "
        "missing-code defaults %s; A2 neutral "
        "Race registry default %s; legacy race-menu "
        "fallback registered; nebula sprite-node guard %s; legacy Instant "
        "Action AIP names %s; unresolved AIP units %s; A1 BZN "
        "map-bounds bridge %s; AiMission bridge %s; runtime-class width "
        "bridge %s; Instant Action relationship bridge %s; "
        "Craft_mLevelUp "
        "neutral Race default %s; oqN visibility %s; "
        "Starbase policy/menu %s; officer target identity %s; officer "
        "queue-admission cap %s; officer completion bridge %s; "
        "Producer palette race filter %s; invalid GetProjectId "
        "detour removed",
        legacy_explore_command_bridge_enabled ? "enabled" : "unavailable",
        essential_gui_sprite_loader_enabled ? "enabled" : "unavailable",
        !g_legacy_cursor_override_required
            ? "not needed"
            : (legacy_default_cursor_fallback_enabled
                   ? "enabled" : "unavailable"),
        !g_legacy_tactical_cursor_override_required
            ? "not needed"
            : (legacy_tactical_cursor_fallback_enabled
                   ? "enabled" : "unavailable"),
        legacy_gameplay_ui_scaling_enabled ? "enabled" : "unavailable",
        legacy_control_panel_adapter_enabled ? "enabled" : "unavailable",
        g_legacy_speed_panel_runtime_ready ? "enabled" : "unavailable",
        legacy_cinematic_view_adapter_enabled ? "enabled" : "unavailable",
        legacy_ship_display_adapter_enabled ? "enabled" : "unavailable",
        g_legacy_resource_panel_runtime_ready ? "enabled" : "unavailable",
        !legacy_team_color_translation_available ? "unavailable" :
            (g_legacy_team_color_palette_active ? "enabled" : "not needed"),
        legacy_virtual_directory_normalizer_enabled
            ? "normalized" : "unavailable",
        legacy_moon_resource_defaults_enabled ? "enabled" : "unavailable",
        legacy_physics_defaults_enabled ? "enabled" : "unavailable",
        legacy_neutral_race_registry_enabled ? "enabled" : "unavailable",
        nebula_guard_enabled ? "enabled" : "unavailable",
        legacy_aip_name_bridge_enabled ? "enabled" : "unavailable",
        legacy_aip_missing_unit_guard_enabled ? "guarded" : "unavailable",
        a1_bzn_map_bounds_bridge_enabled ? "enabled" : "unavailable",
        a1_bzn_ai_mission_bridge_enabled ? "enabled" : "unavailable",
        a1_bzn_runtime_class_bridge_enabled ? "enabled" : "unavailable",
        a1_relationship_bridge_enabled ? "enabled" : "unavailable",
        craft_level_up_race_default_enabled ? "enabled" : "unavailable",
        officer_quarter_compatibility_enabled ? "enabled" : "unavailable",
        starbase_policy_system_enabled ? "enabled" : "unavailable",
        g_officer_upgrade_identity_ready ? "enabled" : "unavailable",
        g_officer_upgrade_admission_ready ? "enabled" : "unavailable",
        g_officer_upgrade_completion_ready ? "enabled" : "unavailable",
        palette_race_filter_enabled ? "enabled" : "unavailable");
    log_line(message);
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    log_line("Armada 1 compatibility module shutting down");
    destroy_legacy_control_background();
    destroy_legacy_speed_background();
    destroy_legacy_resource_backgrounds();
    if (g_resource_panel_render_vtable_hook_installed &&
        g_resource_panel_render_vtable_slot &&
        g_resource_panel_render_original &&
        readable_range(
            g_resource_panel_render_vtable_slot, sizeof(void*)) &&
        *g_resource_panel_render_vtable_slot ==
            reinterpret_cast<void*>(
                &resource_panel_render_vtable_hook)) {
        DWORD old_protect = 0;
        if (VirtualProtect(
                g_resource_panel_render_vtable_slot, sizeof(void*),
                PAGE_READWRITE, &old_protect)) {
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(
                    g_resource_panel_render_vtable_slot),
                g_resource_panel_render_original);
            DWORD restored = 0;
            VirtualProtect(
                g_resource_panel_render_vtable_slot, sizeof(void*),
                old_protect, &restored);
        }
    }
    g_resource_panel_render_vtable_slot = nullptr;
    g_resource_panel_render_original = nullptr;
    g_resource_panel_render_vtable_hook_installed = false;
    if (g_control_button_press_vtable_hook_installed &&
        g_control_button_press_vtable_slot &&
        g_control_button_press_original &&
        readable_range(
            g_control_button_press_vtable_slot, sizeof(void*)) &&
        *g_control_button_press_vtable_slot ==
            reinterpret_cast<void*>(
                &control_button_press_vtable_hook)) {
        DWORD old_protect = 0;
        if (VirtualProtect(
                g_control_button_press_vtable_slot, sizeof(void*),
                PAGE_READWRITE, &old_protect)) {
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(
                    g_control_button_press_vtable_slot),
                g_control_button_press_original);
            DWORD restored = 0;
            VirtualProtect(
                g_control_button_press_vtable_slot, sizeof(void*),
                old_protect, &restored);
        }
    }
    g_control_button_press_vtable_slot = nullptr;
    g_control_button_press_original = nullptr;
    g_control_button_press_vtable_hook_installed = false;
    if (g_producer_push_target_hook_installed &&
        g_producer_push_target_slot &&
        g_producer_push_target_original &&
        readable_range(g_producer_push_target_slot, sizeof(void*)) &&
        writable_range(g_producer_push_target_slot, sizeof(void*)) &&
        *g_producer_push_target_slot ==
            reinterpret_cast<void*>(
                &producer_push_build_queue_item_hook)) {
        InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(
                g_producer_push_target_slot),
            g_producer_push_target_original);
    }
    g_producer_push_target_slot = nullptr;
    g_producer_push_target_original = nullptr;
    g_producer_push_target_hook_installed = false;
    if (g_starbase_start_effect_vtable_hook_installed &&
        g_starbase_start_effect_vtable_slot &&
        g_starbase_start_effect_original &&
        readable_range(
            g_starbase_start_effect_vtable_slot, sizeof(void*)) &&
        *g_starbase_start_effect_vtable_slot ==
            reinterpret_cast<void*>(
                &starbase_start_construction_effect_hook)) {
        DWORD old_protect = 0;
        if (VirtualProtect(
                g_starbase_start_effect_vtable_slot, sizeof(void*),
                PAGE_READWRITE, &old_protect)) {
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(
                    g_starbase_start_effect_vtable_slot),
                g_starbase_start_effect_original);
            DWORD restored = 0;
            VirtualProtect(
                g_starbase_start_effect_vtable_slot, sizeof(void*),
                old_protect, &restored);
        }
    }
    g_starbase_start_effect_vtable_slot = nullptr;
    g_starbase_start_effect_original = nullptr;
    g_starbase_start_effect_vtable_hook_installed = false;
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
    g_officer_upgrade_admission_ready = false;
    g_officer_upgrade_identity_ready = false;
    g_producer_events_ready = false;
    g_officer_upgrade_system_ready = false;
    g_constructor_menu_capability_count = 0;
    g_legacy_scout_menu_capability_count = 0;
    g_legacy_station_menu_capability_count = 0;
    g_legacy_moon_menu_capability_count = 0;
    g_legacy_explore_command_collision_count = 0;
    g_gui_sprite_table_load_count = 0;
    g_legacy_cursor_fallback_count = 0;
    g_legacy_tactical_cursor_fallback_count = 0;
    g_legacy_cursor_fallback_failure_reported = 0;
    g_legacy_cursor_override_required = false;
    g_legacy_tactical_cursor_override_required = false;
    g_legacy_cursor_override_source.clear();
    g_legacy_tactical_native_overview_cursors = {};
    g_legacy_gameplay_ui_active = 0;
    g_legacy_tooltip_background_fallback = 0;
    g_legacy_tooltip_text_fallback = 0;
    g_legacy_tooltip_frame_fallback = 0;
    g_legacy_tooltip_colour_reported = 0;
    g_legacy_tooltip_background_colour = {};
    g_legacy_control_button_rect_count = 0;
    g_legacy_control_button_adapter_reported = 0;
    g_legacy_control_background_available = 0;
    g_legacy_control_black_mask_available = 0;
    g_legacy_control_black_mask_reported = 0;
    g_legacy_ship_display_black_mask_available = 0;
    g_legacy_ship_display_black_mask_reported = 0;
    g_legacy_ship_display_wireframe_available = 0;
    g_legacy_ship_display_wireframe_reported = 0;
    g_legacy_ship_display_race_icon_available = 0;
    g_legacy_ship_display_race_icon_reported = 0;
    g_legacy_ship_display_race_icon_background_reported = 0;
    g_legacy_ship_display_identity_available = 0;
    g_legacy_ship_display_identity_reported = 0;
    g_legacy_ship_display_identity_text_available = 0;
    g_legacy_ship_display_identity_text_reported = 0;
    g_legacy_ship_display_identity_background_available = 0;
    g_legacy_ship_display_identity_background_reported = 0;
    g_legacy_ship_display_system_strip_reported = 0;
    g_legacy_ship_display_progress_available = 0;
    g_legacy_ship_display_progress_reported = 0;
    g_legacy_ship_display_multi_layout_available = 0;
    g_legacy_ship_display_multi_layout_reported = 0;
    g_legacy_ship_display_multi_constructor_reported = 0;
    g_legacy_ship_display_multi_crew_reported = 0;
    g_legacy_ship_display_energy_available = 0;
    g_legacy_ship_display_energy_reported = 0;
    g_legacy_speed_control_layout_available = 0;
    g_legacy_speed_queue_layout_reported = 0;
    g_legacy_speed_special_layout_reported = 0;
    g_legacy_speed_special_layout_count = 0;
    g_legacy_speed_transport_layout_reported = 0;
    g_legacy_popup_control_diagnostic_reported = 0;
    g_legacy_officer_root_control_reported = 0;
    g_legacy_officer_root_repair_report_count = 0;
    g_legacy_officer_root_order_latched_reported = 0;
    g_legacy_officer_root_return_reported = 0;
    g_legacy_officer_root_direct_dispatch_reported = 0;
    g_legacy_officer_build_control_hidden_reported = 0;
    g_legacy_control_black_mask_runtime_ready = false;
    g_legacy_ship_display_render_runtime_ready = false;
    g_legacy_ship_display_background_runtime_ready = false;
    g_legacy_ship_display_opaque_black_runtime_ready = false;
    g_legacy_ship_display_race_icon_background_runtime_ready = false;
    g_legacy_ship_display_text_runtime_ready = false;
    g_legacy_construction_bar_target_runtime_ready = false;
    g_legacy_construction_bar_target_reported = 0;
    g_legacy_construction_rig_progress_diagnostic_reported = 0;
    g_legacy_cinematic_button_runtime_ready = false;
    g_legacy_cinematic_caption_reported = 0;
    g_legacy_caption_text_component = nullptr;
    g_legacy_officer_root_control_runtime_ready = false;
    g_legacy_selected_object = nullptr;
    g_legacy_officer_root_button = nullptr;
    g_legacy_officer_root_target = nullptr;
    g_legacy_officer_root_enabled = false;
    g_legacy_officer_root_dispatch_producer = nullptr;
    g_legacy_officer_root_dispatch_target = nullptr;
    g_legacy_officer_root_pending_producer = nullptr;
    g_legacy_officer_root_direct_dispatch_active = false;
    g_legacy_officer_root_mode_info = {};
    g_legacy_resource_render_bridge_active = 0;
    g_legacy_speed_panel_available = 0;
    g_legacy_speed_panel_reported = 0;
    g_legacy_speed_panel_runtime_ready = false;
    g_legacy_cinematic_layout_available = 0;
    g_legacy_cinematic_layout_reported = 0;
    g_legacy_cinematic_buttons_available = 0;
    g_legacy_cinematic_buttons_reported = 0;
    g_legacy_resource_panel_available = 0;
    g_legacy_resource_panel_reported = 0;
    g_legacy_resource_background_suppression_reported = 0;
    g_legacy_resource_panel_runtime_ready = false;
    g_legacy_ship_display_alias_report_count = 0;
    g_legacy_control_button_rects = {};
    g_legacy_control_panel_rect = {};
    g_legacy_control_background_rect = {};
    g_legacy_control_black_rect = {};
    g_legacy_speed_panel_rect = {};
    g_legacy_speed_queue_screen_rects = {};
    g_legacy_speed_transport_screen_rect = {};
    g_legacy_speed_background_pieces = {};
    g_legacy_speed_background_piece_count = 0;
    g_legacy_cinematic_panel_rect = {};
    g_legacy_cinematic_background_panel_rect = {};
    g_legacy_cinematic_background_rect = {};
    g_legacy_cinematic_display_rect = {};
    g_legacy_cinematic_menu_button_rect = {};
    g_legacy_cinematic_comm_button_rect = {};
    g_legacy_cinematic_menu_button_sprite_name = {};
    g_legacy_cinematic_menu_border_sprite_name = {};
    g_legacy_cinematic_comm_button_sprite_name = {};
    g_legacy_cinematic_comm_border_sprite_name = {};
    g_legacy_cinematic_button_art_owner = nullptr;
    g_legacy_ship_display_black_rect = {};
    g_legacy_ship_display_wireframe_rect = {};
    g_legacy_ship_display_race_icon_rect = {};
    g_legacy_ship_display_race_icon_display_rect = {};
    g_legacy_ship_display_class_rect = {};
    g_legacy_ship_display_name_rect = {};
    g_legacy_ship_display_crew_rect = {};
    g_legacy_ship_display_crew_dot_rect = {};
    g_legacy_ship_display_officer_rect = {};
    g_legacy_ship_display_crew_label_rect = {};
    g_legacy_ship_display_officer_label_rect = {};
    g_legacy_ship_display_progress_rect = {};
    g_legacy_ship_display_single_energy_rect = {};
    g_legacy_ship_display_multi_tile_rects = {};
    g_legacy_ship_display_multi_shield_rects = {};
    g_legacy_ship_display_multi_wireframe_rects = {};
    g_legacy_ship_display_multi_crew_rects = {};
    g_legacy_ship_display_multi_energy_rects = {};
    g_legacy_ship_display_single_energy_sprite_name = {};
    g_legacy_ship_display_multi_energy_sprite_name = {};
    g_legacy_ship_display_multi_crew_sprite_name = {};
    g_legacy_ship_display_crew_label_key = {};
    g_legacy_ship_display_officer_label_key = {};
    g_legacy_ship_display_crew_dot_sprite_name = {};
    g_legacy_ship_display_identity_background_pieces = {};
    g_legacy_ship_display_identity_background_piece_count = 0;
    g_legacy_resource_panel_rect = {};
    g_legacy_resource_text_rects = {};
    g_legacy_resource_icon_rects = {};
    g_legacy_resource_icon_names = {};
    g_legacy_resource_background_pieces = {};
    g_legacy_resource_background_piece_counts = {};
    g_race_menu_callback_ready = false;
    g_project_id_get_odf_name = nullptr;
    g_legacy_team_color_palette = {};
    g_legacy_team_color_palette_active = false;
    g_legacy_team_color_apply_count = 0;
    InterlockedExchange(
        &g_legacy_resource_maximum_post_config_pending, 0);
    g_legacy_physics_combat_speed_default_count = 0;
    g_legacy_physics_impulse_speed_translation_count = 0;
    g_legacy_physics_warp_speed_translation_count = 0;
    g_legacy_physics_model_default_count = 0;
    g_legacy_smooth_profile_translation_count = 0;
    reset_legacy_race_menu_state();
    g_api = nullptr;
    g_armada = nullptr;
    g_fleet_ops = nullptr;
}
