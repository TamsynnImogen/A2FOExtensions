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
constexpr std::array<const char*, 11> kLegacyMenuCapabilityOdfFields{{
    "scout", "combat", "alert", "can_sandd", "can_explore",
    "is_starbase", "facility", "has_crew", "has_hitpoints",
    "spatial_object", "has_resource"}};

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
// forward-only resynchronization to the unique EmptyMission marker.
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
constexpr char kSerializedEmptyMissionName[] = "EmptyMission";

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
constexpr std::size_t kParameterDbScreenWidthOffset = 0x2c;
constexpr std::size_t kParameterDbScreenHeightOffset = 0x30;

// Fleet Operations' supported ArmadaL build exposes the common gameplay-UI
// rectangle loader at this RVA (the retail Armada II map uses a different
// address). Its hidden first argument is the returned RECT storage. A narrow
// alias layer lets A2's three-height ShipDisplay consume A1's one-panel keys.
constexpr std::uintptr_t kDisplayInterfaceLoadRectangleRva = 0x0011b430;
constexpr std::size_t kDisplayInterfaceLoadRectangleHookLength = 9;
constexpr std::uint8_t kExpectedDisplayInterfaceLoadRectangle[] = {
    0x55, 0x8b, 0xec, 0x8a, 0x0d, 0xb8, 0x4e, 0x76, 0x00};
constexpr std::size_t kParameterDbGetStringHookLength = 9;
constexpr std::uint8_t kExpectedParameterDbGetStringHook[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};
constexpr LONG kMaximumLegacyShipDisplayAliasReports = 16;

// Fleet Operations lays PopupPalette controls out using its A2 palette
// geometry immediately before ControlButton::Render. Raw A1 configurations
// instead provide twelve race-specific rectangles relative to
// controlPanelArea. Reapply those rectangles at the final rendering boundary:
// this composes with Fleet Ops' compaction and HybridBuild's mode binding
// without taking ownership of either module's popup-update hook.
constexpr std::uintptr_t kControlButtonRenderRva = 0x000e64e0;
constexpr std::size_t kControlButtonRenderHookLength = 6;
constexpr std::uint8_t kExpectedControlButtonRender[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x34};
constexpr std::uintptr_t kFoPopupButtonPointerArrayRva = 0x00247ef4;
constexpr std::size_t kFoPopupButtonCapacity = 64;
constexpr std::size_t kControlButtonRectangleOffset = 0x08;

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
A2FO_InlineHook g_control_button_render_hook{};
A2FO_InlineHook g_display_interface_load_rectangle_hook{};
A2FO_InlineHook g_parameter_db_get_string_hook{};
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
volatile LONG g_legacy_gameplay_ui_active = 0;
volatile LONG g_legacy_control_button_rect_count = 0;
volatile LONG g_legacy_control_button_adapter_reported = 0;
volatile LONG g_legacy_ship_display_alias_report_count = 0;
PVOID volatile g_legacy_gui_parameter_db = nullptr;
volatile LONG g_missing_craft_race_count = 0;
volatile LONG g_officer_quarter_prepare_count = 0;
volatile LONG g_officer_upgrade_completion_count = 0;
volatile LONG g_officer_upgrade_rejection_count = 0;
volatile LONG g_officer_upgrade_class_count = 0;
volatile LONG g_officer_upgrade_consumed_count = 0;
volatile LONG g_officer_upgrade_effect_suppression_count = 0;
volatile LONG g_constructor_menu_capability_count = 0;
volatile LONG g_legacy_scout_menu_capability_count = 0;
volatile LONG g_legacy_station_recrew_capability_count = 0;
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

using NebulaSetTexturesRecursiveFn = void (__cdecl*)(void* node);
using FileInFixedCharsFn = bool (__cdecl*)(
    void* file_reader, void* output, std::uint32_t size);
using FileReaderLoadFn = bool (__cdecl*)(void* file_reader);
using AiMissionLoadFn = FileReaderLoadFn;
using MapDetailsFactoryLoadFn = void* (__cdecl*)(const char* filename);
using TeamColorInitFn = void (__cdecl*)();
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
        bytes + kCommandInfoMenuOffset) = 0;

    const LONG incident = InterlockedIncrement(
        &g_legacy_explore_command_collision_count);
    if (incident <= 4) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 scout/Explore command basename collision repaired #%ld: "
            "odf='%s', commandInfo=%p, commandId=%ld, source=0x%08lx",
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
    constexpr std::array<MissingMenuCapability, 3> station_capabilities{{
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
                      &g_legacy_station_recrew_capability_count,
                      "station Recrew");
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
    if (producer && target_class &&
        is_officer_upgrade_class(target_class)) {
        if (!g_officer_upgrade_completion_ready ||
            !admit_officer_upgrade(producer, target_class)) {
            return 0;
        }
    }
    if (!g_producer_push_target_original) return 0;
    return a2fo_a1_call_thiscall_1(
        g_producer_push_target_original, producer,
        reinterpret_cast<std::uintptr_t>(target_class));
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
        constexpr std::size_t kMissionRecordPrefixSize =
            8 + sizeof(kSerializedEmptyMissionName);
        const auto is_empty_mission = [](const std::uint8_t* candidate,
                                         const std::uint8_t* stream_end) {
            if (!candidate || !stream_end || stream_end < candidate ||
                static_cast<std::size_t>(stream_end - candidate) <
                    kMissionRecordPrefixSize) {
                return false;
            }
            std::uint32_t field_type = 0;
            std::uint32_t field_size = 0;
            std::memcpy(&field_type, candidate, sizeof(field_type));
            std::memcpy(&field_size, candidate + 4, sizeof(field_size));
            return (field_type & 0xffu) == 2u &&
                   field_size == a1compat::kA2SerializedRtimeClassNameSize &&
                   std::memcmp(
                       candidate + 8, kSerializedEmptyMissionName,
                       sizeof(kSerializedEmptyMissionName)) == 0;
        };

        if (is_empty_mission(cursor, end)) {
            mission_marker_count = 1;
        } else {
            const std::uintptr_t scan_limit = std::min<std::uintptr_t>(
                mission_cursor.remaining, 1024u * 1024u);
            std::uintptr_t replacement_cursor = 0;
            for (std::uintptr_t distance = 1;
                 distance + kMissionRecordPrefixSize <= scan_limit;
                 ++distance) {
                if (!is_empty_mission(cursor + distance, end)) continue;
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

void configure_gui_parameter_db(
    void* parameter_db, const char*) noexcept {
    if (!g_armada || !parameter_db) return;

    InterlockedExchangePointer(&g_legacy_gui_parameter_db, nullptr);
    InterlockedExchange(&g_legacy_gameplay_ui_active, 0);
    InterlockedExchange(&g_legacy_control_button_rect_count, 0);
    InterlockedExchange(&g_legacy_control_button_adapter_reported, 0);
    InterlockedExchange(&g_legacy_ship_display_alias_report_count, 0);
    g_legacy_control_button_rects = {};

    const a1compat::LegacyUiEvidence evidence{
        gui_parameter_rectangle(parameter_db, "speedPanelArea"),
        gui_parameter_rectangle(parameter_db, "controlPanelArea"),
        gui_parameter_declares_int(parameter_db, "screenWidth"),
        gui_parameter_declares_int(parameter_db, "screenHeight")};
    const a1compat::LegacyUiDecision decision =
        a1compat::decide_legacy_gameplay_ui(evidence);

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
                    a1compat::place_legacy_control_button(
                        control_panel, local_button);
                ++captured_button_count;
            }
        }
    }

    // Publish only after the full rectangle table is ready. The UI normally
    // loads on the game thread, but this also prevents a render callback from
    // observing a partially rebuilt race layout.
    InterlockedExchange(
        &g_legacy_control_button_rect_count, captured_button_count);
    InterlockedExchangePointer(
        &g_legacy_gui_parameter_db,
        decision.legacy_layout ? parameter_db : nullptr);
    InterlockedExchange(
        &g_legacy_gameplay_ui_active, decision.legacy_layout ? 1 : 0);

}

void apply_legacy_control_button_rectangle(void* control_button) noexcept {
    const LONG rectangle_count = InterlockedCompareExchange(
        &g_legacy_control_button_rect_count, 0, 0);
    if (!control_button || !g_fleet_ops || rectangle_count <= 0 ||
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) == 0) {
        return;
    }

    void** popup_buttons = at<void*>(
        g_fleet_ops, kFoPopupButtonPointerArrayRva);
    if (!readable_range(
            popup_buttons, kFoPopupButtonCapacity * sizeof(void*))) {
        return;
    }

    std::size_t slot = kFoPopupButtonCapacity;
    for (std::size_t index = 0; index < kFoPopupButtonCapacity; ++index) {
        if (popup_buttons[index] == control_button) {
            slot = index;
            break;
        }
    }
    if (slot >= static_cast<std::size_t>(rectangle_count) ||
        slot >= g_legacy_control_button_rects.size()) {
        return;
    }

    const a1compat::NativeUiRectangle rectangle =
        g_legacy_control_button_rects[slot];
    const std::array<std::int32_t, 4> native_rectangle{
        rectangle.left, rectangle.top, rectangle.right, rectangle.bottom};
    auto* destination = static_cast<std::uint8_t*>(control_button) +
        kControlButtonRectangleOffset;
    if (!writable_range(destination, sizeof(native_rectangle))) return;
    std::memcpy(destination, native_rectangle.data(), sizeof(native_rectangle));

    if (InterlockedCompareExchange(
            &g_legacy_control_button_adapter_reported, 1, 0) == 0) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "A1 ControlPanel adapter active: %ld race-defined slots; "
            "first rendered slot=%lu at [%ld,%ld,%ld,%ld]",
            static_cast<long>(rectangle_count),
            static_cast<unsigned long>(slot + 1),
            static_cast<long>(rectangle.left),
            static_cast<long>(rectangle.top),
            static_cast<long>(rectangle.right),
            static_cast<long>(rectangle.bottom));
        log_line(message);
    }
}

void __attribute__((fastcall)) control_button_render_hook(
    void* control_button, void*) noexcept {
    apply_legacy_control_button_rectangle(control_button);
    if (g_control_button_render_hook.gateway) {
        a2fo_a1_call_thiscall_0(
            g_control_button_render_hook.gateway, control_button);
    }
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
    const char* resolved = key;
    if (key && InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) != 0) {
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

std::uintptr_t __attribute__((fastcall)) parameter_db_get_string_hook(
    void* parameter_db, void*, const char* key, char* output,
    std::uintptr_t output_size, const char* default_value) noexcept {
    const char* resolved = key;
    const void* legacy_parameter_db = InterlockedCompareExchangePointer(
        &g_legacy_gui_parameter_db, nullptr, nullptr);
    if (key && parameter_db == legacy_parameter_db &&
        InterlockedCompareExchange(
            &g_legacy_gameplay_ui_active, 0, 0) != 0) {
        if (const char* alias =
                a1compat::legacy_ship_display_string_alias(key)) {
            resolved = alias;
            report_legacy_ship_display_alias("string", key, alias);
        }
    }
    if (!g_parameter_db_get_string_hook.gateway) return 0;
    return a2fo_a1_call_thiscall_4(
        g_parameter_db_get_string_hook.gateway, parameter_db,
        reinterpret_cast<std::uintptr_t>(resolved),
        reinterpret_cast<std::uintptr_t>(output), output_size,
        reinterpret_cast<std::uintptr_t>(default_value));
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

bool install_legacy_control_panel_adapter(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !g_fleet_ops ||
        !api->install_inline_hook) {
        return false;
    }
    return api->install_inline_hook(
        at(g_armada, kControlButtonRenderRva),
        reinterpret_cast<void*>(&control_button_render_hook),
        kControlButtonRenderHookLength,
        kExpectedControlButtonRender,
        &g_control_button_render_hook);
}

bool install_legacy_ship_display_adapter(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !g_armada || !api->install_inline_hook) return false;
    if (!a1_native_entry_supported(
            "DisplayInterface::LoadRectangle",
            kDisplayInterfaceLoadRectangleRva,
            kExpectedDisplayInterfaceLoadRectangle) ||
        !a1_native_entry_supported(
            "ParameterDB::GetString ShipDisplay alias",
            kParameterDbGetStringRva,
            kExpectedParameterDbGetStringHook)) {
        return false;
    }

    const bool rectangle_installed = api->install_inline_hook(
        at(g_armada, kDisplayInterfaceLoadRectangleRva),
        reinterpret_cast<void*>(&display_interface_load_rectangle_hook),
        kDisplayInterfaceLoadRectangleHookLength,
        kExpectedDisplayInterfaceLoadRectangle,
        &g_display_interface_load_rectangle_hook);
    const bool string_installed = api->install_inline_hook(
        at(g_armada, kParameterDbGetStringRva),
        reinterpret_cast<void*>(&parameter_db_get_string_hook),
        kParameterDbGetStringHookLength,
        kExpectedParameterDbGetStringHook,
        &g_parameter_db_get_string_hook);
    if (rectangle_installed != string_installed) {
        log_line("A1 ShipDisplay adapter installed only one of its two "
                 "key-alias boundaries");
    }
    return rectangle_installed && string_installed;
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

extern "C" std::uintptr_t __cdecl a2fo_a1_load_gui_sprite_tables(
    void* parser, void* database, const char* primary_filename) {
    return load_gui_sprite_tables(parser, database, primary_filename);
}

extern "C" void __cdecl a2fo_a1_configure_gui_parameter_db(
    void* parameter_db, const char* configuration_filename) {
    configure_gui_parameter_db(parameter_db, configuration_filename);
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
    g_legacy_station_recrew_capability_count = 0;
    g_legacy_moon_menu_capability_count = 0;
    g_legacy_explore_command_collision_count = 0;
    g_gui_sprite_table_load_count = 0;
    g_legacy_gameplay_ui_active = 0;
    g_legacy_control_button_rect_count = 0;
    g_legacy_control_button_adapter_reported = 0;
    g_legacy_ship_display_alias_report_count = 0;
    g_legacy_gui_parameter_db = nullptr;
    g_legacy_control_button_rects = {};
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
    const bool legacy_gameplay_ui_scaling_enabled =
        install_legacy_gameplay_ui_scaling(api);
    if (!legacy_gameplay_ui_scaling_enabled) {
        log_line("A1 gameplay UI 640x480 scaling bridge installation "
                 "failed; raw legacy CFG rectangles will retain A2's "
                 "1600x1200 reference size");
    }
    const bool legacy_control_panel_adapter_enabled =
        install_legacy_control_panel_adapter(api);
    if (!legacy_control_panel_adapter_enabled) {
        log_line("A1 ControlPanel button-layout adapter installation "
                 "failed; PopupPalette controls will retain the Fleet "
                 "Operations layout");
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

    // Install this last: the adapter owns ParameterDB::GetString's entry and
    // every earlier compatibility installer must preflight the native entry
    // before that checked detour is present.
    const bool legacy_ship_display_adapter_enabled =
        install_legacy_ship_display_adapter(api);
    if (!legacy_ship_display_adapter_enabled) {
        log_line("A1 ShipDisplay one-panel key adapter installation failed; "
                 "the A2 low/middle/tall layouts may remain displaced");
    }

    char message[1320]{};
    std::snprintf(
        message, sizeof(message),
        "Armada 1 compatibility initialized: wingman -> craft; A2 scout "
        "and station menu defaults registered; A1 scout/Explore command "
        "collision bridge %s; Addon ODF overlay; essential "
        "a2_gui_global.spr loader %s; A1 gameplay UI scaling %s; A1 "
        "ControlPanel adapter %s; A1 ShipDisplay adapter %s; legacy "
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
        legacy_gameplay_ui_scaling_enabled ? "enabled" : "unavailable",
        legacy_control_panel_adapter_enabled ? "enabled" : "unavailable",
        legacy_ship_display_adapter_enabled ? "enabled" : "unavailable",
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
    g_legacy_station_recrew_capability_count = 0;
    g_legacy_moon_menu_capability_count = 0;
    g_legacy_explore_command_collision_count = 0;
    g_legacy_gameplay_ui_active = 0;
    g_legacy_control_button_rect_count = 0;
    g_legacy_control_button_adapter_reported = 0;
    g_legacy_ship_display_alias_report_count = 0;
    g_legacy_gui_parameter_db = nullptr;
    g_legacy_control_button_rects = {};
    g_race_menu_callback_ready = false;
    g_project_id_get_odf_name = nullptr;
    g_legacy_team_color_palette = {};
    g_legacy_team_color_palette_active = false;
    g_legacy_team_color_apply_count = 0;
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
