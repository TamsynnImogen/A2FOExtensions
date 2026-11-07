#include "../../rust/native_hooks_ffi.h"
/*
 * ODF-driven captain names and craft registries for Fleet Operations.
 *
 * Craft classes may declare possibleCaptainNames and possibleCraftRegistry.
 * Both companion lists use the exact entry Fleet Operations selected from
 * possibleCraftNames, including after save/load or a native rename. The
 * existing selected-object captain rectangle is activated and a matching
 * registry rectangle is added beside it.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_module_provider.hpp"
#include "../A2FODirectionalShields/api.hpp"
#include "directional_shield_display_config.hpp"
#include "directional_shield_fill.hpp"
#include "extended_weapon_icons.hpp"
#include "identity_selection.hpp"
#include "object_editor.hpp"
#include "selected_panel_anchor.hpp"
#include "system_icon_state.hpp"
#include "../../rust/stage9_craft_runtime_ffi.h"
#include "../../rust/stage9_craft_sprite_ffi.h"
#include "../../rust/stage10_identity_completion_ffi.h"

#include <windows.h>
#include <d3d8.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

extern "C" {
std::uintptr_t __cdecl a2fo_identity_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_identity_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
std::uintptr_t __cdecl a2fo_identity_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
std::uintptr_t __cdecl a2fo_identity_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
std::uintptr_t __cdecl a2fo_identity_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
std::uintptr_t __cdecl a2fo_identity_call_thiscall_7(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4, std::uintptr_t argument5,
    std::uintptr_t argument6, std::uintptr_t argument7);
std::uintptr_t __cdecl a2fo_identity_call_delphi_2(
    void* function, std::uintptr_t argument_eax,
    std::uintptr_t argument_edx);
void a2fo_identity_fo_sprite_set_colour(
    void* function, void* sprite, const void* colour);
void a2fo_identity_fo_sprite_draw_scaled_2d(
    void* function, void* sprite, const void* position,
    float display_width, float display_height);
void a2fo_identity_system_icon_set_colour_bridge();
void a2fo_identity_weapon_icon_set_colour_bridge();
void a2fo_identity_system_text_set_colour_bridge();
void a2fo_identity_value_text_draw_bridge();
void* a2fo_identity_value_text_draw_original = nullptr;
}

namespace {

using ShieldClassObserver = void (A2FO_CALL *)(
    void* object_class, void* parameter_db);
using NebulaClassObserver = void (A2FO_CALL *)(
    void* object_class, void* parameter_db);
using TextureVariantsClassObserver = void (A2FO_CALL *)(
    void* object_class, void* parameter_db);

constexpr char kModuleName[] = "A2FOCraftIdentity";
constexpr char kArtConfigFileName[] = "ART_CFG.h";
constexpr std::streamoff kMaximumArtConfigSize = 2 * 1024 * 1024;
constexpr char kCraftNameCommand[] = "possibleCraftNames";
constexpr char kCaptainCommand[] = "possibleCaptainNames";
constexpr char kRegistryCommand[] = "possibleCraftRegistry";
constexpr char kShieldTooltipCommand[] = "shieldTooltip";
constexpr char kShieldVerboseTooltipCommand[] = "shieldVerboseTooltip";
constexpr char kAlwaysShowShieldsModuleName[] =
    "A2FOAlwaysShowShields.dll";
constexpr char kShieldClassObserverExport[] =
    "A2FOAlwaysShowShields_RegisterClass";
constexpr char kNebulaRendererModuleName[] = "A2FONebulaRenderer.dll";
constexpr char kNebulaClassObserverExport[] =
    "A2FONebulaRenderer_RegisterClass";
constexpr char kTextureVariantsModuleName[] =
    "A2FOTextureVariants.dll";
constexpr char kTextureVariantsClassObserverExport[] =
    "A2FOTextureVariants_RegisterClass";
constexpr char kEnergySystemsModuleName[] = "A2FOEnergySystems.dll";
constexpr char kGetPhotonTorpedoesExport[] =
    "A2FOEnergySystems_GetPhotonTorpedoes";
constexpr char kGetQuantumTorpedoesExport[] =
    "A2FOEnergySystems_GetQuantumTorpedoes";
constexpr char kGetShuttleCraftExport[] =
    "A2FOEnergySystems_GetShuttleCraft";
constexpr char kGetMaximumPhotonTorpedoesExport[] =
    "A2FOEnergySystems_GetMaximumPhotonTorpedoes";
constexpr char kGetMaximumQuantumTorpedoesExport[] =
    "A2FOEnergySystems_GetMaximumQuantumTorpedoes";
constexpr char kGetMaximumShuttleCraftExport[] =
    "A2FOEnergySystems_GetMaximumShuttleCraft";
constexpr char kGetPhotonTorpedoReloadSecondsExport[] =
    "A2FOEnergySystems_GetPhotonTorpedoReloadSeconds";
constexpr char kGetQuantumTorpedoReloadSecondsExport[] =
    "A2FOEnergySystems_GetQuantumTorpedoReloadSeconds";
constexpr char kGetShuttleCraftReloadSecondsExport[] =
    "A2FOEnergySystems_GetShuttleCraftReloadSeconds";
constexpr char kGetResupplyWeaponStateExport[] =
    "A2FOEnergySystems_GetResupplyWeaponState";
// Default resupplyweapon icon tint while a valid receiver is in range.
constexpr float kDefaultResupplyWeaponIconColour[3]{0.10f, 1.00f, 0.10f};
constexpr char kDirectionalShieldsModuleName[] =
    "A2FODirectionalShields.dll";
constexpr char kDirectionalShieldsIsEnabledExport[] =
    "A2FODirectionalShields_IsEnabled";
constexpr char kDirectionalShieldsGetCurrentExport[] =
    "A2FODirectionalShields_GetCurrent";
constexpr char kDirectionalShieldsGetMaximumExport[] =
    "A2FODirectionalShields_GetMaximum";
constexpr char kDirectionalShieldsGetModeExport[] =
    "A2FODirectionalShields_GetMode";
constexpr std::size_t kMaximumIdentityEntries = 4096;
constexpr std::size_t kMaximumIdentityLength = 512;

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kCraftClassConstructorRva = 0x000bf090;
constexpr std::uintptr_t kSelectedInfoUpdateRva = 0x000f2ff0;
constexpr std::uintptr_t kSelectedBuilderInfoRenderRva = 0x000f3560;
constexpr std::uintptr_t kSelectedInfoRenderRva = 0x000f3770;
constexpr std::uintptr_t kSpriteSetColourRva = 0x0023a4d0;
constexpr std::uintptr_t kGameObjectClassGetOdfNameRva = 0x000ce370;
constexpr std::uintptr_t kParameterDbGetRectangleRva = 0x001358f0;
constexpr std::uintptr_t kParameterDbGetColorRva = 0x00135ba0;
constexpr std::uintptr_t kParameterDbGetIntRva = 0x00134bf0;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kParameterDbGetStringVectorRva = 0x00135e80;
constexpr std::uintptr_t kDisplayInterfaceDrawTextInRectangleRva = 0x0011b160;
constexpr std::uintptr_t kEngineOperatorDeleteRva = 0x002527d0;
constexpr std::uintptr_t kGuiParameterDbPointerRva = 0x0036502c;
constexpr std::uintptr_t kInterfaceSpriteDatabasePointerRva = 0x00365030;
constexpr std::uintptr_t kInterfaceSpriteDatabaseGetRva = 0x00220750;
constexpr std::uintptr_t kLocalizationLookupRva = 0x00081c90;
constexpr std::uintptr_t kLocalizationManagerPointerRva = 0x003379fc;
constexpr std::uintptr_t kInsertCStringIatRva = 0x003b7dec;
constexpr std::uintptr_t kCursorXRva = 0x00365018;
constexpr std::uintptr_t kCursorYRva = 0x0036501c;
constexpr std::uintptr_t kSt3DOffsetXRva = 0x003ad6e0;
constexpr std::uintptr_t kSt3DOffsetYRva = 0x003ad6e4;
constexpr std::uintptr_t kSt3DScaleXRva = 0x003ad6e8;
constexpr std::uintptr_t kSt3DScaleYRva = 0x003ad6ec;
constexpr std::uintptr_t kStandardComponentSetTooltipTextRva = 0x0010c040;
constexpr std::uintptr_t kStandardComponentSetVerboseTooltipTextRva =
    0x0010c080;
constexpr std::uintptr_t kTooltipManagerShowRva = 0x00105a80;
constexpr std::uintptr_t kTooltipManagerClearRva = 0x00105d40;
constexpr std::uintptr_t kTooltipManagerRenderRva = 0x00105380;
constexpr std::uintptr_t kTooltipManagerObjectRva = 0x003643a8;
constexpr std::uintptr_t kTooltipModeRva = 0x00364484;
constexpr std::uintptr_t kActiveInterfaceOwnerRva = 0x003643cc;
constexpr std::uintptr_t kStandardComponentVtableRva = 0x002b56b4;
constexpr std::uintptr_t kWireframeIconVtableRva = 0x002b4f18;
constexpr std::uintptr_t kStandardComponentUpdateRva = 0x0010be60;
constexpr std::uintptr_t kStandardComponentTooltipRva = 0x0010c0c0;
constexpr std::uintptr_t kStandardComponentVerboseTooltipRva = 0x0010c100;
constexpr std::uintptr_t kSystemValueVtableRva = 0x002b4c88;
constexpr std::uintptr_t kCrewNumTextVtableRva = 0x002b4b18;
constexpr std::uintptr_t kHullTextVtableRva = 0x002b4b88;
constexpr std::uintptr_t kShieldTextVtableRva = 0x002b4b50;
constexpr std::uintptr_t kEnergyTextVtableRva = 0x002b4adc;
constexpr std::uintptr_t kOfficerTextAndSpriteVtableRva = 0x002b4c50;

// FleetOpsHook's Delphi wrappers around Armada's ST3D sprite methods. These
// are presentation-only and validated independently from the selected-panel
// hook. GUI sprite lookup itself uses Armada's interfaceDB rather than Fleet
// Operations' separate world-sprite database.
constexpr std::uintptr_t kFoSpriteSetColourRva = 0x001e34b4;
constexpr std::uintptr_t kFoSpriteDrawScaled2DRva = 0x001e3498;
constexpr std::uintptr_t kFoShipSystemIconRenderRva = 0x001ed458;
constexpr std::uintptr_t kFoShipSystemIconVtableRva = 0x0021110c;
constexpr std::uintptr_t kFoTechnologyTreeAllowsProjectRva = 0x00120680;
constexpr std::uintptr_t kFoTechnologyItemShouldShowButtonRva = 0x0011eb6c;
constexpr std::uintptr_t kFoTeamTechnologyTreesPointerRva = 0x00212f08;
// The weaponIconX positions belong to Armada's WireframeIcon component. Its
// two virtual tooltip callbacks are also the native hover route for that
// component, so intercept them rather than Fleet Operations' unrelated
// SelectionDisplay "Analysis of..." formatting wrappers.
constexpr std::uintptr_t kWireframeIconTooltipRva = 0x000f80b0;
constexpr std::uintptr_t kWireframeIconVerboseTooltipRva = 0x000f8240;
constexpr std::size_t kSpriteFrameListOffset = 0x1c;
constexpr std::size_t kSpriteColourOffset = 0x24;
constexpr std::size_t kSpriteTextureXOffset = 0x38;
constexpr std::size_t kSpriteTextureYOffset = 0x3c;
constexpr std::size_t kSpriteTextureWidthOffset = 0x40;
constexpr std::size_t kSpriteTextureHeightOffset = 0x44;
constexpr std::size_t kSpriteTextureObjectOffset = 0x58;
constexpr std::uintptr_t kGraphicsEnginePointerRva = 0x003ad508;
constexpr std::size_t kCurrentDeviceIndexOffset = 0xc0;
constexpr std::uint32_t kMaximumStormDeviceCount = 2;
constexpr std::size_t kStorm3DTextureDeviceArrayOffset = 0x40;
constexpr std::size_t kStorm3DDeviceTextureNativeOffset = 0x04;
constexpr float kDirectionalShieldTextureDimension = 128.0f;
constexpr DWORD kDirectionalShieldSpriteStabilizationMs = 500;

constexpr std::array<const char*, 2> kCompactQuadShieldSpriteNames{{
    "dsq",
    "dsqs",
}};

constexpr std::array<const char*, 4> kDirectionalShieldSpriteNames{{
    "dsb",
    "dsf",
    "dsl",
    "dsr",
}};

constexpr std::array<const char*, 2> kTriShieldSpriteNames{{
    "dstc",
    "dsts",
}};

constexpr char kHemisphereShieldSpriteName[] = "dsh";
constexpr char kHemisphereSideShieldSpriteName[] = "dshs";

constexpr std::array<const char*, 4> kDirectionalShieldPositionCommands{{
    "forwardShieldPos",
    "aftShieldPos",
    "portShieldPos",
    "starboardShieldPos",
}};

constexpr char kDirectionalShieldValueDisplayModeCommand[] =
    "directionalShieldValueDisplayMode";
constexpr std::array<const char*, 4>
    kDirectionalShieldValueRectangleCommands{{
        "infoSingleDirectionalShieldsForwardValueTextArea",
        "infoSingleDirectionalShieldsAftValueTextArea",
        "infoSingleDirectionalShieldsPortValueTextArea",
        "infoSingleDirectionalShieldsStarboardValueTextArea",
    }};

constexpr std::array<const char*, 3> kAmmunitionDisplayModeCommands{{
    "photonTorpedoDisplayMode",
    "quantumTorpedoDisplayMode",
    "shuttleCraftDisplayMode",
}};
constexpr std::array<const char*, 3> kAmmunitionValueDisplayModeCommands{{
    "photonTorpedoValueDisplayMode",
    "quantumTorpedoValueDisplayMode",
    "shuttleCraftValueDisplayMode",
}};
constexpr std::array<const char*, 3> kAmmunitionLabelCommands{{
    "photonTorpedoLabel",
    "quantumTorpedoLabel",
    "shuttleCraftLabel",
}};
constexpr std::array<const char*, 3> kAmmunitionTooltipCommands{{
    "photonTorpedoTooltip",
    "quantumTorpedoTooltip",
    "shuttleCraftTooltip",
}};
constexpr std::array<const char*, 3> kAmmunitionVerboseTooltipCommands{{
    "photonTorpedoVerboseTooltip",
    "quantumTorpedoVerboseTooltip",
    "shuttleCraftVerboseTooltip",
}};
constexpr std::array<const char*, 3> kAmmunitionIconCommands{{
    "photonTorpedoIcon",
    "quantumTorpedoIcon",
    "shuttleCraftIcon",
}};
constexpr std::array<const char*, 3> kAmmunitionIconPositionCommands{{
    "photonTorpedoIconPos",
    "quantumTorpedoIconPos",
    "shuttleCraftIconPos",
}};

constexpr std::array<const char*, 5> kSystemIconColourCommands{{
    "systemIconHealthyColor",
    "systemIconLowColor",
    "systemIconCriticalColor",
    "systemIconDisabledColor",
    "systemIconDestroyedColor",
}};

constexpr std::array<const char*, 5> kWeaponIconColourCommands{{
    "weaponIconColor",
    "weaponIconLowColor",
    "weaponIconCriticalColor",
    "weaponIconDisabledColor",
    "weaponIconDestroyedColor",
}};

constexpr std::array<const char*, 4> kDirectionalShieldTooltipKeys{{
    "GUI_SD_DIRSHIELD_FORWARD_TOOLTIP",
    "GUI_SD_DIRSHIELD_AFT_TOOLTIP",
    "GUI_SD_DIRSHIELD_PORT_TOOLTIP",
    "GUI_SD_DIRSHIELD_STARBOARD_TOOLTIP",
}};

constexpr std::array<const char*, 4> kDirectionalShieldVerboseTooltipKeys{{
    "GUI_SD_DIRSHIELD_FORWARD_VTOOLTIP",
    "GUI_SD_DIRSHIELD_AFT_VTOOLTIP",
    "GUI_SD_DIRSHIELD_PORT_VTOOLTIP",
    "GUI_SD_DIRSHIELD_STARBOARD_VTOOLTIP",
}};

constexpr std::array<const char*, 4> kDirectionalShieldTooltipFallbacks{{
    "Forward Shields",
    "Aft Shields",
    "Port Shields",
    "Starboard Shields",
}};

constexpr std::array<const char*, 4>
    kDirectionalShieldVerboseTooltipFallbacks{{
        "Forward shields protect the vessel's forward arc.",
        "Aft shields protect the vessel's rear arc.",
        "Port shields protect the vessel's left arc.",
        "Starboard shields protect the vessel's right arc.",
    }};
constexpr char kDirectionalShieldStrengthKey[] =
    "GUI_SD_DIRSHIELD_STRENGTH";
constexpr char kDirectionalShieldStrengthFallback[] = "Current strength";

// Fleet Operations detours CraftClass(ParameterDB) before extension modules
// load. Chain through the one supported handler just as the turret module
// chains Fleet Operations' GameObjectClass and Craft::Simulate handlers.
constexpr std::uintptr_t kFoCraftClassConstructorHandlerRva = 0x0010d6e4;

constexpr std::size_t kObjectHandleOffset = 0x28;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kCraftNameIndexOffset = 0x218;
// Fleet Operations attaches one CraftEnhancement record to ranked Craft.
// Its live XP is compared against the current class record's next-rank XP.
constexpr std::size_t kCraftEnhancementOffset = 0x1a4;
constexpr std::size_t kEnhancementClassOffset = 0x04;
constexpr std::size_t kEnhancementCurrentXpOffset = 0x10;
constexpr std::size_t kEnhancementNextRankXpOffset = 0xb0;
constexpr std::size_t kCurrentShieldsOffset = 0x1c8;
constexpr std::size_t kMaximumShieldsOffset = 0x1cc;
constexpr std::size_t kCraftSystemsOffset = 0x1e0;
constexpr std::size_t kCraftSystemSize = 0x30;
constexpr std::size_t kCraftSystemOperationalOffset = 0x00;
constexpr std::size_t kCraftSystemForcedDisabledOffset = 0x01;
constexpr std::size_t kCraftSystemMaximumHitpointsOffset = 0x04;
constexpr std::size_t kCraftSystemCurrentHitpointsOffset = 0x18;
constexpr std::size_t kCraftSystemDisableTimeOffset = 0x28;
constexpr std::size_t kInfoDisplaySelectedCraftOffset = 0x1e8;
// WireframeIcon is a compact StandardComponent. Its base occupies +0x00..27,
// followed by the selected craft at +0x28; InfoDisplay stores it at +0xac.
// It is rendered above the middle background and therefore wins hover routing
// throughout the stock wireframe rectangle where our four arcs are drawn.
constexpr std::size_t kInfoDisplayWireframeOffset = 0xac;
constexpr std::size_t kInfoDisplayCaptainTextOffset = 0xbc;
constexpr std::size_t kInfoDisplayClassTextOffset = 0x90;
constexpr std::size_t kInfoDisplayBuilderClassTextOffset = 0x100;
constexpr std::size_t kInfoDisplayBuilderNameTextOffset = 0x104;
constexpr std::size_t kTextComponentLiveRectangleOffset = 0x58;
// Fleet Operations applies nameTextColor to these two GUIText instances in
// its SelectionDisplay enhancement. Armada's older shipNameColor only reaches
// the narrow mouse-over bar, so bridge it to both selected-name variants while
// leaving the neighbouring class components at +0x90/+0x100 untouched.
constexpr std::array<std::size_t, 2> kInfoDisplayNameTextOffsets{{
    0x94, 0x104}};
constexpr std::size_t kTextComponentColourOffset = 0x70;
constexpr std::size_t kSystemIconCraftOffset = 0x28;
constexpr std::size_t kSystemIconIndexOffset = 0x2c;
// Fleet Operations' ShipSystemIcon is the per-weapon component created by
// weaponXiconpos. It retains the selected craft at +0x28; all such icons
// represent Armada's weapons CraftSystem record (index 2).
constexpr std::size_t kWeaponIconCraftOffset = 0x28;
constexpr std::size_t kWeaponIconIndexOffset = 0x30;
constexpr std::size_t kWeaponSystemOnCraftOffset = 0x128;
constexpr std::size_t kWeaponVectorBeginOffset = 0x0c;
constexpr std::size_t kWeaponVectorEndOffset = 0x10;
constexpr std::size_t kWeaponClassOnWeaponOffset = 0x04;
constexpr std::size_t kWeaponClassSpecialOffset = 0x1b4;
constexpr std::size_t kWeaponClassProjectIdOffset = 0x208;
constexpr std::size_t kGameObjectTeamOffset = 0xec;
constexpr std::size_t kTechnologyTreeItemsOffset = 0x0c;
constexpr std::size_t kShipSystemIconRenderVtableOffset = 0x10;
constexpr std::int32_t kMaximumTeamIndex = 63;
constexpr std::int32_t kWeaponsSystemIndex = 2;
constexpr std::size_t kSystemTextCraftOffset = 0x2c;
constexpr std::size_t kSystemTextIndexOffset = 0x12c;
constexpr std::size_t kValuePercentageOffset = 0x108;

constexpr std::uint8_t kExpectedCraftClassConstructor[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uint8_t kExpectedSelectedInfoRender[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x08};
constexpr std::uint8_t kExpectedSelectedInfoUpdate[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x08,
    0xa1, 0x38, 0x13, 0x76, 0x00};
constexpr std::uint8_t kExpectedSpriteSetColour[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x83, 0xc1, 0x24};
constexpr std::uint8_t kExpectedFoCraftClassConstructorHandler[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf8, 0x53};
constexpr std::uint8_t kExpectedParameterDbGetRectangle[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x04, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedParameterDbGetColor[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedDisplayInterfaceDrawTextInRectangle[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};
constexpr std::uint8_t kExpectedGameObjectClassGetOdfName[] = {
    0x8b, 0x89, 0xcc, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedInterfaceSpriteDatabaseGet[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x53, 0x56, 0x57};
constexpr std::uint8_t kExpectedFoSpriteSetColour[] = {
    0x55, 0x8b, 0xec, 0x51, 0x89, 0x45, 0xfc};
constexpr std::uint8_t kExpectedFoSpriteDrawScaled2D[] = {
    0x55, 0x8b, 0xec, 0x51, 0x89, 0x45, 0xfc};
constexpr std::uint8_t kExpectedFoTechnologyTreeAllowsProject[] = {
    0x53, 0x51, 0x89, 0x14, 0x24, 0x33, 0xd2, 0x8b,
    0x40, 0x0c, 0x8b, 0x0c, 0x24, 0x8b, 0x5c, 0x88};
constexpr std::uint8_t kExpectedFoTechnologyItemShouldShowButton[] = {
    0x53, 0x56, 0x57, 0x55, 0x51, 0x8b, 0xf0, 0xb3,
    0x01, 0x80, 0x7e, 0x09, 0x00, 0x74, 0x47, 0x8b};
// ShipSystemIcon already has a complete disabled rendering branch for an
// unavailable special weapon. Its early `test dl, al` hides the component
// before that branch; test only the resolved sprite pointer so the later
// native availability decision can select disabled presentation instead.
constexpr std::uintptr_t kFoSpecialWeaponVisibilityTestRva = 0x001ed6d6;
constexpr std::array<std::uint8_t, 2>
    kExpectedFoSpecialWeaponVisibilityTest{{0x84, 0xc2}};
constexpr std::array<std::uint8_t, 2>
    kReplacementFoSpecialWeaponVisibilityTest{{0x84, 0xd2}};
constexpr std::uint8_t kExpectedWireframeIconTooltip[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x08};
constexpr std::uint8_t kExpectedWireframeIconVerboseTooltip[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x14};
constexpr std::uint8_t kExpectedSetTooltipText[] = {
    0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1, 0x8b, 0x46};
constexpr std::uint8_t kExpectedStandardComponentUpdate[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x08};

struct CheckedCallSite {
    std::uintptr_t rva;
    std::array<std::uint8_t, 5> expected;
};

// SystemIcon::Render has five direct colour writes, and its repair/health
// animation helper has one more. Redirect only these calls; every other ST3D
// sprite in the game retains the untouched setter.
constexpr std::array<CheckedCallSite, 6> kSystemIconColourCallSites{{
    {0x000eed2e, {{0xe8, 0x9d, 0xb7, 0x14, 0x00}}},
    {0x000eedd7, {{0xe8, 0xf4, 0xb6, 0x14, 0x00}}},
    {0x000eee43, {{0xe8, 0x88, 0xb6, 0x14, 0x00}}},
    {0x000eef00, {{0xe8, 0xcb, 0xb5, 0x14, 0x00}}},
    {0x000eefd8, {{0xe8, 0xf3, 0xb4, 0x14, 0x00}}},
    {0x000ef24e, {{0xe8, 0x7d, 0xb2, 0x14, 0x00}}},
}};
// ShipSystemIcon::Render writes a dim/background layer and then the visible
// weapon layer through Fleet Operations' Delphi sprite-colour helper. Patch
// these calls rather than the render entry, which A2FOFireArcs also detours.
constexpr std::array<CheckedCallSite, 2> kWeaponIconColourCallSites{{
    {0x001ed8f7, {{0xe8, 0xb8, 0x5b, 0xff, 0xff}}},
    {0x001ed99d, {{0xe8, 0x12, 0x5b, 0xff, 0xff}}},
}};
constexpr CheckedCallSite kSystemValueIconColourCallSite{
    0x000ec748, {{0xe8, 0x83, 0xdd, 0x14, 0x00}}};
constexpr CheckedCallSite kValueTextDrawCallSite{
    0x0010c393, {{0xe8, 0xc8, 0xed, 0x00, 0x00}}};
// TooltipManager::Show begins with three complete instructions occupying ten
// bytes.  The inline-hook gateway copies this sequence verbatim, so the hook
// length must include the full `push 0x0069feab` instruction.
constexpr std::uint8_t kExpectedTooltipManagerShow[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff,
    0x68, 0xab, 0xfe, 0x69, 0x00};
constexpr std::uint8_t kExpectedTooltipManagerClear[] = {
    0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff};
constexpr std::uint8_t kExpectedTooltipManagerRender[] = {
    0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x38};

struct NativeStringVector {
    std::uint32_t allocator = 0;
    char** begin = nullptr;
    char** end = nullptr;
    char** capacity = nullptr;
};
static_assert(sizeof(NativeStringVector) == 16,
              "Armada string-vector ABI must occupy sixteen bytes");

struct ClassIdentityPolicy {
    std::string odf_name;
    std::vector<std::string> craft_names;
    std::vector<std::string> captain_names;
    std::vector<std::string> craft_registries;
};

struct CraftIdentity {
    void* object_class = nullptr;
    std::int32_t craft_name_index = -1;
    std::uint64_t editor_revision = 0;
    std::string captain_name;
    std::string craft_registry;
};

using a2fo::craft_identity::RawRectangle;

struct DirectionalShieldUiPolicy {
    std::array<bool, 4> position_found{};
    std::array<RawRectangle, 4> positions{};
    bool value_display_mode_found = false;
    a2fo::craft_identity::DirectionalShieldValueDisplayMode
        value_display_mode =
            a2fo::craft_identity::DirectionalShieldValueDisplayMode::none;
};

enum class AmmunitionDisplayMode : std::int32_t {
    text = 1,
    icon = 2,
};

enum class AmmunitionValueDisplayMode : std::int32_t {
    percent = 0,
    amount = 1,
    reload = 2,
    bar = 3,
};

struct AmmunitionPresentation {
    AmmunitionDisplayMode display_mode = AmmunitionDisplayMode::text;
    AmmunitionValueDisplayMode value_display_mode =
        AmmunitionValueDisplayMode::percent;
    std::string label;
    std::string tooltip;
    std::string verbose_tooltip;
    std::string icon;
    bool icon_position_found = false;
    RawRectangle icon_position{};
};

struct AmmunitionUiPolicy {
    // Photon Torpedoes, Quantum Torpedoes, Shuttle Craft.
    std::array<AmmunitionPresentation, 3> stores{};
};

struct SelectedStatusUiPolicy {
    std::string shield_tooltip;
    std::string shield_verbose_tooltip;
};

using a2fo::craft_identity::NativeRectangle;
using a2fo::craft_identity::usable_native_rectangle;
using a2fo::craft_identity::translated_rectangle;

using FloatRectangle = a2fo::craft_identity::RectangleF;

// Immutable alpha-bearing bounds inside the four supplied 128x128 textures,
// ordered as top, bottom, left, right to match
// kDirectionalShieldSpriteNames. Logical facings may be placed on any edge by
// ODF, so drawing selects one of these masks from the destination rectangle
// rather than assuming forward is always at the top.
constexpr std::array<FloatRectangle, 4> kDirectionalShieldSourceBounds{{
    FloatRectangle{26.0f, 0.0f, 76.0f, 20.0f},
    FloatRectangle{26.0f, 108.0f, 76.0f, 20.0f},
    FloatRectangle{0.0f, 26.0f, 20.0f, 76.0f},
    FloatRectangle{108.0f, 26.0f, 20.0f, 76.0f},
}};

std::size_t directional_shield_sprite_index_for_destination(
    const std::array<FloatRectangle, 4>& destinations,
    std::size_t destination_index, float container_width,
    float container_height) noexcept {
    const FloatRectangle& destination = destinations[destination_index];
    if (destination.width >= destination.height) {
        const float centre_y = destination.y + destination.height * 0.5f;
        float minimum_y = centre_y;
        float maximum_y = centre_y;
        for (const FloatRectangle& candidate : destinations) {
            if (candidate.width < candidate.height) continue;
            const float candidate_y =
                candidate.y + candidate.height * 0.5f;
            minimum_y = std::min(minimum_y, candidate_y);
            maximum_y = std::max(maximum_y, candidate_y);
        }
        const float divider = maximum_y > minimum_y
            ? (minimum_y + maximum_y) * 0.5f
            : container_height * 0.5f;
        return centre_y < divider
            ? 0u : 1u;  // top : bottom
    }
    const float centre_x = destination.x + destination.width * 0.5f;
    float minimum_x = centre_x;
    float maximum_x = centre_x;
    for (const FloatRectangle& candidate : destinations) {
        if (candidate.width >= candidate.height) continue;
        const float candidate_x =
            candidate.x + candidate.width * 0.5f;
        minimum_x = std::min(minimum_x, candidate_x);
        maximum_x = std::max(maximum_x, candidate_x);
    }
    const float divider = maximum_x > minimum_x
        ? (minimum_x + maximum_x) * 0.5f
        : container_width * 0.5f;
    return centre_x < divider
        ? 2u : 3u;  // left : right
}

// ODF rectangles describe where those visible source bounds should land
// inside the selected panel's directional-shield graphic area.
constexpr std::array<FloatRectangle, 4>
    kDefaultDirectionalShieldPositions{{
        FloatRectangle{26.0f, 0.0f, 76.0f, 20.0f},
        FloatRectangle{26.0f, 108.0f, 76.0f, 20.0f},
        FloatRectangle{0.0f, 26.0f, 20.0f, 76.0f},
        FloatRectangle{108.0f, 26.0f, 20.0f, 76.0f},
    }};

// Default tri-forward placement for the full-canvas dstc/dsts masks. Mods may
// override each facing independently with the existing facing Area controls
// or per-Craft *ShieldPos commands. Aft is inactive in mode 2.
constexpr std::array<FloatRectangle, 4>
    kDefaultTriForwardShieldPositions{{
        FloatRectangle{10.0f, 0.0f, 108.0f, 64.0f},
        FloatRectangle{},
        FloatRectangle{0.0f, 48.0f, 72.0f, 80.0f},
        FloatRectangle{56.0f, 48.0f, 72.0f, 80.0f},
    }};

// Tri-rear mirrors the tri-forward canvas vertically. Forward is inactive;
// the aft centre arc occupies the lower half while port/starboard meet at the
// front. Existing per-facing GUI/ODF placement overrides still take priority.
constexpr std::array<FloatRectangle, 4>
    kDefaultTriRearShieldPositions{{
        FloatRectangle{},
        FloatRectangle{10.0f, 64.0f, 108.0f, 64.0f},
        FloatRectangle{0.0f, 0.0f, 72.0f, 80.0f},
        FloatRectangle{56.0f, 0.0f, 72.0f, 80.0f},
    }};

// Forward/rear hemisphere mode reuses one full-canvas upper-half mask. The
// aft copy is vertically mirrored at draw time. Port/starboard are inactive.
constexpr std::array<FloatRectangle, 4>
    kDefaultHemiForwardRearShieldPositions{{
        FloatRectangle{0.0f, 0.0f, 128.0f, 128.0f},
        FloatRectangle{0.0f, 0.0f, 128.0f, 128.0f},
        FloatRectangle{},
        FloatRectangle{},
    }};

// Port/starboard hemisphere mode uses a separately rotated full-canvas left
// half mask. The starboard copy is mirrored horizontally at draw time.
// Forward/aft are inactive.
constexpr std::array<FloatRectangle, 4>
    kDefaultHemiPortStarboardShieldPositions{{
        FloatRectangle{},
        FloatRectangle{},
        FloatRectangle{0.0f, 0.0f, 128.0f, 128.0f},
        FloatRectangle{0.0f, 0.0f, 128.0f, 128.0f},
    }};

struct SpriteVector {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Colour {
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
};
static_assert(sizeof(Colour) == 12,
              "ST3D_Colour must contain three floats");

// Each field independently inherits from medium to tall. Legacy palettes and
// automatic placement remain the final fallback.
enum class PanelElement : std::size_t {
    CaptainText,
    RegistryText,
    PhotonTorpedoesText,
    PhotonTorpedoesLabelText,
    PhotonTorpedoesValueText,
    PhotonTorpedoesIcon,
    PhotonTorpedoesBar,
    QuantumTorpedoesText,
    QuantumTorpedoesLabelText,
    QuantumTorpedoesValueText,
    QuantumTorpedoesIcon,
    QuantumTorpedoesBar,
    ShuttleCraftText,
    ShuttleCraftLabelText,
    ShuttleCraftValueText,
    ShuttleCraftIcon,
    ShuttleCraftBar,
    DirectionalShieldsGraphic,
    DirectionalShieldsForwardAftText,
    DirectionalShieldsPortStarboardText,
    DirectionalShieldsForward,
    DirectionalShieldsForwardValueText,
    DirectionalShieldsAft,
    DirectionalShieldsAftValueText,
    DirectionalShieldsPort,
    DirectionalShieldsPortValueText,
    DirectionalShieldsStarboard,
    DirectionalShieldsStarboardValueText,
    ExperienceBar,
    count
};
constexpr std::array<const char*, static_cast<std::size_t>(PanelElement::count)>
    kPanelElementNames{{
    "CaptainText",
    "RegistryText",
    "PhotonTorpedoesText",
    "PhotonTorpedoesLabelText",
    "PhotonTorpedoesValueText",
    "PhotonTorpedoesIcon",
    "PhotonTorpedoesBar",
    "QuantumTorpedoesText",
    "QuantumTorpedoesLabelText",
    "QuantumTorpedoesValueText",
    "QuantumTorpedoesIcon",
    "QuantumTorpedoesBar",
    "ShuttleCraftText",
    "ShuttleCraftLabelText",
    "ShuttleCraftValueText",
    "ShuttleCraftIcon",
    "ShuttleCraftBar",
    "DirectionalShieldsGraphic",
    "DirectionalShieldsForwardAftText",
    "DirectionalShieldsPortStarboardText",
    "DirectionalShieldsForward",
    "DirectionalShieldsForwardValueText",
    "DirectionalShieldsAft",
    "DirectionalShieldsAftValueText",
    "DirectionalShieldsPort",
    "DirectionalShieldsPortValueText",
    "DirectionalShieldsStarboard",
    "DirectionalShieldsStarboardValueText",
    "ExperienceBar",
}};
struct PanelElementStyle {
    bool area_found = false;
    RawRectangle area{};
    bool colour_found = false, low_found = false, critical_found = false;
    Colour colour{}, low{}, critical{};
    bool background_found = false;
    Colour background{};
    std::string sprite;
    std::string background_sprite;
};
using PanelStyles = std::array<PanelElementStyle,
    static_cast<std::size_t>(PanelElement::count)>;
std::array<PanelStyles, 2> g_panel_styles{};
std::size_t g_active_panel = 0;
void* g_active_info_display = nullptr;

struct UiConfiguration {
    void* parameter_db = nullptr;
    bool loaded = false;
    bool single_name_rectangle_found = false;
    bool single_class_rectangle_found = false;
    bool builder_name_rectangle_found = false;
    bool builder_class_rectangle_found = false;
    bool captain_rectangle_found = false;
    bool registry_rectangle_found = false;
    bool photon_rectangle_found = false;
    bool quantum_rectangle_found = false;
    bool shuttle_craft_rectangle_found = false;
    bool directional_forward_aft_rectangle_found = false;
    bool directional_port_starboard_rectangle_found = false;
    bool directional_graphic_rectangle_found = false;
    std::array<bool, 4> directional_value_rectangle_found{};
    bool shield_bar_rectangle_found = false;
    bool builder_shield_bar_rectangle_found = false;
    bool experience_bar_rectangle_found = false;
    RawRectangle single_name_rectangle{};
    RawRectangle single_class_rectangle{};
    RawRectangle builder_name_rectangle{};
    RawRectangle builder_class_rectangle{};
    RawRectangle captain_rectangle{};
    RawRectangle registry_rectangle{};
    RawRectangle photon_rectangle{};
    RawRectangle quantum_rectangle{};
    RawRectangle shuttle_craft_rectangle{};
    RawRectangle directional_forward_aft_rectangle{};
    RawRectangle directional_port_starboard_rectangle{};
    RawRectangle directional_graphic_rectangle{};
    std::array<RawRectangle, 4> directional_value_rectangles{};
    RawRectangle shield_bar_rectangle{};
    RawRectangle builder_shield_bar_rectangle{};
    RawRectangle experience_bar_rectangle{};
    bool shared_text_colour_found = false;
    bool ship_name_colour_found = false;
    bool captain_colour_found = false;
    bool registry_colour_found = false;
    bool photon_colour_found = false;
    bool quantum_colour_found = false;
    bool shuttle_craft_colour_found = false;
    bool photon_low_colour_found = false;
    bool quantum_low_colour_found = false;
    bool shuttle_craft_low_colour_found = false;
    bool photon_critical_colour_found = false;
    bool quantum_critical_colour_found = false;
    bool shuttle_craft_critical_colour_found = false;
    bool directional_shield_colour_found = false;
    bool directional_shield_low_colour_found = false;
    bool directional_shield_critical_colour_found = false;
    bool directional_shield_value_colour_found = false;
    bool directional_shield_value_low_colour_found = false;
    bool directional_shield_value_critical_colour_found = false;
    std::array<bool, 5> system_icon_colour_found{};
    std::array<bool, 5> weapon_icon_colour_found{};
    bool passive_weapon_icon_colour_found = false;
    bool resupply_weapon_icon_colour_found = false;
    bool special_energy_icon_colour_found = false;
    bool officer_icon_colour_found = false;
    bool experience_bar_colour_found = false;
    bool experience_bar_background_colour_found = false;
    Colour shared_text_colour{};
    Colour ship_name_colour{};
    Colour captain_colour{};
    Colour registry_colour{};
    Colour photon_colour{};
    Colour quantum_colour{};
    Colour shuttle_craft_colour{};
    Colour photon_low_colour{};
    Colour quantum_low_colour{};
    Colour shuttle_craft_low_colour{};
    Colour photon_critical_colour{};
    Colour quantum_critical_colour{};
    Colour shuttle_craft_critical_colour{};
    Colour directional_shield_colour{};
    Colour directional_shield_low_colour{};
    Colour directional_shield_critical_colour{};
    Colour directional_shield_value_colour{};
    Colour directional_shield_value_low_colour{};
    Colour directional_shield_value_critical_colour{};
    std::array<Colour, 5> system_icon_colours{};
    std::array<Colour, 5> weapon_icon_colours{};
    Colour passive_weapon_icon_colour{};
    Colour resupply_weapon_icon_colour{};
    Colour special_energy_icon_colour{};
    Colour officer_icon_colour{};
    Colour experience_bar_colour{};
    Colour experience_bar_background_colour{};
};

using EnergyAmountGetter = float (A2FO_CALL *)(void* craft);
// -1: not a resupplyweapon, 0: no valid receiver, 1: valid receiver in range.
using ResupplyWeaponStateGetter = std::int32_t (A2FO_CALL *)(void* weapon);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
ShieldClassObserver g_shield_class_observer = nullptr;
NebulaClassObserver g_nebula_class_observer = nullptr;
TextureVariantsClassObserver g_texture_variants_class_observer = nullptr;
EnergyAmountGetter g_get_photon_torpedoes = nullptr;
EnergyAmountGetter g_get_quantum_torpedoes = nullptr;
EnergyAmountGetter g_get_shuttle_craft = nullptr;
EnergyAmountGetter g_get_maximum_photon_torpedoes = nullptr;
EnergyAmountGetter g_get_maximum_quantum_torpedoes = nullptr;
EnergyAmountGetter g_get_maximum_shuttle_craft = nullptr;
EnergyAmountGetter g_get_photon_torpedo_reload_seconds = nullptr;
EnergyAmountGetter g_get_quantum_torpedo_reload_seconds = nullptr;
EnergyAmountGetter g_get_shuttle_craft_reload_seconds = nullptr;
ResupplyWeaponStateGetter g_get_resupply_weapon_state = nullptr;
bool g_resupply_weapon_state_resolution_attempted = false;
A2FO_DirectionalShieldsIsEnabledFn g_directional_shields_is_enabled = nullptr;
A2FO_DirectionalShieldsGetValueFn g_directional_shields_get_current = nullptr;
A2FO_DirectionalShieldsGetValueFn g_directional_shields_get_maximum = nullptr;
A2FO_DirectionalShieldsGetModeFn g_directional_shields_get_mode = nullptr;
bool g_runtime_ready = false;
bool g_chained_fo_craft_class_constructor = false;
bool g_energy_api_resolution_attempted = false;
bool g_directional_shields_api_resolution_attempted = false;
bool g_directional_shield_component_tooltips_available = false;
bool g_directional_shield_native_tooltip_available = false;
bool g_directional_shield_wireframe_tooltips_available = false;
void* g_craft_class_constructor_original = nullptr;
void* g_wireframe_update_original = nullptr;
void* g_ship_system_icon_render_original = nullptr;
A2FO_InlineHook g_craft_class_constructor_hook{};
A2FO_InlineHook g_selected_info_update_hook{};
A2FO_InlineHook g_selected_builder_info_render_hook{};
A2FO_InlineHook g_selected_info_render_hook{};
A2FO_InlineHook g_wireframe_tooltip_hook{};
A2FO_InlineHook g_wireframe_verbose_tooltip_hook{};
A2FO_InlineHook g_tooltip_manager_show_hook{};
A2FO_InlineHook g_tooltip_manager_render_hook{};
A2FO_InlineHook g_standard_component_update_hook{};
std::unordered_map<void*, ClassIdentityPolicy> g_class_policies;
std::unordered_set<void*> g_passive_weapon_classes;
std::unordered_map<void*, DirectionalShieldUiPolicy>
    g_directional_shield_ui_policies;
std::unordered_map<void*, AmmunitionUiPolicy> g_ammunition_ui_policies;
std::unordered_map<void*, SelectedStatusUiPolicy>
    g_selected_status_ui_policies;
std::unordered_map<std::uint32_t, CraftIdentity> g_craft_identities;
UiConfiguration g_ui_configuration{};
a2fo::craft_identity::DirectionalShieldDisplayConfig
    g_directional_shield_display_config{};
LONG g_assignment_report_count = 0;
LONG g_draw_report_count = 0;
LONG g_ship_name_colour_report_count = 0;
LONG g_system_icon_colour_report_count = 0;
LONG g_weapon_icon_colour_report_count = 0;
LONG g_passive_weapon_icon_report_count = 0;
LONG g_hidden_weapon_icon_report_count = 0;
LONG g_disabled_weapon_icon_report_count = 0;
thread_local void* g_rendering_weapon_icon = nullptr;
thread_local void* g_rendering_weapon_craft = nullptr;
thread_local a2fo::craft_identity::WeaponIconPresentation
    g_rendering_weapon_presentation =
        a2fo::craft_identity::WeaponIconPresentation::live_status;
LONG g_system_value_icon_colour_report_count = 0;
LONG g_system_text_colour_report_count = 0;
LONG g_crew_icon_colour_report_count = 0;
LONG g_crew_text_colour_report_count = 0;
LONG g_mouseover_status_icon_colour_report_count = 0;
LONG g_mouseover_status_text_colour_report_count = 0;
LONG g_special_energy_icon_colour_report_count = 0;
LONG g_special_energy_text_colour_report_count = 0;
LONG g_officer_icon_colour_report_count = 0;
LONG g_officer_text_colour_report_count = 0;
LONG g_builder_panel_anchor_report_count = 0;
LONG g_ammunition_draw_report_count = 0;
LONG g_ammunition_icon_failure_report_count = 0;
LONG g_ammunition_hover_component_report_count = 0;
LONG g_ammunition_hover_presented_report_count = 0;
LONG g_selected_status_draw_report_count = 0;
LONG g_selected_status_hover_presented_report_count = 0;
LONG g_directional_shields_draw_report_count = 0;
LONG g_directional_shields_graphic_report_count = 0;
LONG g_directional_shield_sprite_failure_report_count = 0;
LONG g_directional_shield_tooltip_query_report_count = 0;
LONG g_directional_shield_tooltip_hit_report_count = 0;
LONG g_directional_shield_component_assignment_report_count = 0;
LONG g_directional_shield_native_dispatch_report_count = 0;
LONG g_directional_shield_direct_tooltip_report_count = 0;
LONG g_directional_shield_native_render_report_count = 0;
LONG g_directional_shield_show_substitution_report_count = 0;
LONG g_directional_shield_hover_rectangle_report_count = 0;
LONG g_directional_shield_hover_component_report_count = 0;
LONG g_directional_shield_hover_presented_report_count = 0;

struct DirectionalShieldSpriteRuntime {
    bool resolution_attempted = false;
    bool ready = false;
    bool compact = false;
    void* database = nullptr;
    std::array<void*, 4> sprites{};
};

struct TriShieldSpriteRuntime {
    bool resolution_attempted = false;
    bool ready = false;
    void* database = nullptr;
    std::array<void*, 2> sprites{};
};

struct HemisphereShieldSpriteRuntime {
    bool resolution_attempted = false;
    bool ready = false;
    void* database = nullptr;
    void* sprite = nullptr;
};

struct DirectionalShieldAnimation {
    void* craft = nullptr;
    DWORD last_tick = 0;
    bool initialized = false;
    std::array<float, 4> displayed_ratio{};
};

struct DirectionalShieldTooltipRuntime {
    bool active = false;
    void* info_display = nullptr;
    void* craft = nullptr;
    std::array<FloatRectangle, 4> hit_rectangles{};
    std::array<float, 4> current{};
    std::array<float, 4> maximum{};
};

struct DirectionalShieldTooltipBinding {
    void* info_display = nullptr;
    void* component = nullptr;
    void* craft = nullptr;
    int facing = -1;
    float current = -1.0f;
    float maximum = -1.0f;
};

struct DirectionalShieldNativeTooltip {
    void* component = nullptr;
    void* craft = nullptr;
    int facing = -1;
    DWORD hover_started = 0;
    bool presented = false;
};

// TooltipManager only requires the StandardComponent tooltip-provider ABI:
// a vtable followed by the normal and verbose C-string pointers at +0x20 and
// +0x24. Keeping this provider separate prevents WireframeIcon's stock
// "Analysis of..." overrides from reclaiming the generated tooltip text.
struct DirectionalShieldNativeTooltipProvider {
    void* vtable = nullptr;
    std::array<std::uint8_t, 0x1c> padding{};
    const char* normal = nullptr;
    const char* verbose = nullptr;
    std::array<char, 768> normal_storage{};
    std::array<char, 768> verbose_storage{};
};
static_assert(
    offsetof(DirectionalShieldNativeTooltipProvider, normal) == 0x20,
    "native normal-tooltip pointer must be at StandardComponent +0x20");
static_assert(
    offsetof(DirectionalShieldNativeTooltipProvider, verbose) == 0x24,
    "native verbose-tooltip pointer must be at StandardComponent +0x24");

// Exact in-memory prefix used by Armada's StandardComponent. These four
// persistent components give the sprite-only shield arcs the same native
// hover ownership, timing, and tooltip-manager path as weaponIconX elements.
struct DirectionalShieldHoverComponent {
    void* vtable = nullptr;
    void* owner = nullptr;
    NativeRectangle rectangle{};
    std::uint8_t hovered = 0;
    std::uint8_t presented = 0;
    std::uint16_t padding = 0;
    float hover_reset_time = -1.0f;
    const char* normal = nullptr;
    const char* verbose = nullptr;
    std::array<char, 768> normal_storage{};
    std::array<char, 768> verbose_storage{};
};
static_assert(
    offsetof(DirectionalShieldHoverComponent, owner) == 0x04,
    "native StandardComponent owner must be at +0x04");
static_assert(
    offsetof(DirectionalShieldHoverComponent, rectangle) == 0x08,
    "native StandardComponent rectangle must be at +0x08");
static_assert(
    offsetof(DirectionalShieldHoverComponent, normal) == 0x20,
    "native StandardComponent normal tooltip must be at +0x20");
static_assert(
    offsetof(DirectionalShieldHoverComponent, verbose) == 0x24,
    "native StandardComponent verbose tooltip must be at +0x24");

struct DirectionalShieldHoverComponents {
    void* info_display = nullptr;
    void* craft = nullptr;
    void* owner = nullptr;
    std::array<float, 4> current{{-1.0f, -1.0f, -1.0f, -1.0f}};
    std::array<float, 4> maximum{{-1.0f, -1.0f, -1.0f, -1.0f}};
    std::array<DirectionalShieldHoverComponent, 4> components{};
};

struct AmmunitionTooltipRuntime {
    bool active = false;
    void* info_display = nullptr;
    void* craft = nullptr;
    std::array<bool, 3> visible{};
    std::array<FloatRectangle, 3> hit_rectangles{};
    std::array<float, 3> current{};
    std::array<float, 3> maximum{};
};

struct AmmunitionHoverComponents {
    void* info_display = nullptr;
    void* craft = nullptr;
    void* owner = nullptr;
    std::array<float, 3> current{{-1.0f, -1.0f, -1.0f}};
    std::array<float, 3> maximum{{-1.0f, -1.0f, -1.0f}};
    std::array<DirectionalShieldHoverComponent, 3> components{};
};

enum class SelectedStatusIndex : std::size_t {
    shields = 0,
    experience = 1,
};

struct SelectedStatusTooltipRuntime {
    bool active = false;
    void* info_display = nullptr;
    void* craft = nullptr;
    std::array<bool, 2> visible{};
    std::array<FloatRectangle, 2> hit_rectangles{};
    std::array<float, 2> current{};
    std::array<float, 2> maximum{};
};

struct SelectedStatusHoverComponents {
    void* info_display = nullptr;
    void* craft = nullptr;
    void* owner = nullptr;
    std::array<bool, 2> visible{};
    std::array<float, 2> current{{-1.0f, -1.0f}};
    std::array<float, 2> maximum{{-1.0f, -1.0f}};
    std::array<DirectionalShieldHoverComponent, 2> components{};
};

struct AmmunitionIconCache {
    void* database = nullptr;
    std::array<std::string, 3> names{};
    std::array<void*, 3> sprites{};
    std::array<bool, 3> attempted{};
};

struct AmmunitionBarCache {
    void* database = nullptr;
    void* sprite = nullptr;
    bool attempted = false;
    std::string name;
};

DirectionalShieldSpriteRuntime g_directional_shield_sprites{};
TriShieldSpriteRuntime g_tri_shield_sprites{};
HemisphereShieldSpriteRuntime g_hemisphere_shield_sprite{};
HemisphereShieldSpriteRuntime g_hemisphere_side_shield_sprite{};
DirectionalShieldAnimation g_directional_shield_animation{};
DirectionalShieldTooltipRuntime g_directional_shield_tooltip{};
DirectionalShieldTooltipBinding g_directional_shield_tooltip_binding{};
DirectionalShieldHoverComponents g_directional_shield_hover_components{};
DirectionalShieldNativeTooltip g_directional_shield_native_tooltip{};
DirectionalShieldNativeTooltipProvider
    g_directional_shield_native_tooltip_provider{};
AmmunitionTooltipRuntime g_ammunition_tooltip{};
AmmunitionHoverComponents g_ammunition_hover_components{};
SelectedStatusTooltipRuntime g_selected_status_tooltip{};
SelectedStatusHoverComponents g_selected_status_hover_components{};
AmmunitionIconCache g_ammunition_icon_cache{};
std::array<std::array<AmmunitionBarCache, 2>,
    static_cast<std::size_t>(PanelElement::count)> g_ammunition_bar_cache{};
DWORD g_directional_shield_sprite_retry_after = 0;
void* g_directional_shield_sprite_retry_database = nullptr;

void reset_directional_shield_sprite_runtime() noexcept {
    g_directional_shield_sprites = {};
    g_tri_shield_sprites = {};
    g_hemisphere_shield_sprite = {};
    g_hemisphere_side_shield_sprite = {};
    g_directional_shield_animation = {};
    g_directional_shield_tooltip = {};
    // Interface teardown owns the old component and its copied strings. Drop
    // our non-owning pointer without calling into an object that may already
    // have been destroyed.
    g_directional_shield_tooltip_binding = {};
    g_directional_shield_native_tooltip = {};
    g_ammunition_tooltip = {};
    g_ammunition_hover_components = {};
    g_selected_status_tooltip = {};
    g_selected_status_hover_components = {};
    g_ammunition_icon_cache = {};
    g_ammunition_bar_cache = {};
    g_directional_shield_sprite_retry_database = nullptr;
    g_directional_shield_sprite_retry_after =
        GetTickCount() + kDirectionalShieldSpriteStabilizationMs;
}

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

std::string join_path(const std::string& left, const std::string& right) {
    struct Context {
        const std::string* left;
        const std::string* right;
        std::string result;
        std::exception_ptr failure;
    } context{&left, &right, {}, {}};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        try {
            if (action == 0) return ctx.left->empty();
            if (action == 1) return ctx.right->empty();
            if (action == 2)
                return ctx.left->back() == '\\' || ctx.left->back() == '/';
            if (action == 3) ctx.result = *ctx.right;
            else if (action == 4) ctx.result = *ctx.left;
            else if (action == 5) ctx.result = *ctx.left + *ctx.right;
            else if (action == 6) ctx.result = *ctx.left + "\\" + *ctx.right;
            return 0;
        } catch (...) {
            ctx.failure = std::current_exception();
            return 3;
        }
    };
    a2fo_rs_stage9_craft_join_path(step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
    return std::move(context.result);
}

bool read_small_art_config(
    const std::string& path, std::string& contents) {
    struct Context {
        const std::string* path;
        std::string* contents;
        std::optional<std::ifstream> input;
        std::optional<std::ostringstream> stream;
        std::streamoff size = 0;
        std::exception_ptr failure;
    } context{&path, &contents, {}, {}, 0, {}};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        try {
            if (action == 0) {
                ctx.contents->clear();
                ctx.input.emplace(*ctx.path, std::ios::binary);
                return static_cast<bool>(*ctx.input);
            }
            if (action == 1) {
                ctx.input->seekg(0, std::ios::end);
                ctx.size = ctx.input->tellg();
            } else if (action == 2) {
                return ctx.size < 0 || ctx.size > kMaximumArtConfigSize;
            } else if (action == 3) {
                log_line("Ignored oversized ART configuration: " + *ctx.path);
            } else if (action == 4) {
                ctx.input->seekg(0, std::ios::beg);
                ctx.stream.emplace();
                *ctx.stream << ctx.input->rdbuf();
            } else if (action == 5) {
                *ctx.contents = ctx.stream->str();
            } else if (action == 6) {
                return ctx.input->good() || ctx.input->eof();
            }
            return 0;
        } catch (...) {
            ctx.failure = std::current_exception();
            return 3;
        }
    };
    const bool read = a2fo_rs_stage9_craft_read_art_file(step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
    return read;
}

void load_directional_shield_display_config() {
    using a2fo::craft_identity::parse_directional_shield_display_config;
    using a2fo::craft_identity::valid_directional_shield_position_mapping;

    g_directional_shield_display_config = {};
    const std::uint32_t root_count = g_api->extension_root_count();
    struct Row {
        std::string path;
        std::string contents;
        a2fo::craft_identity::DirectionalShieldDisplayParseReport report{};
    };
    struct Context {
        std::optional<Row> row;
        std::exception_ptr failure;
    } context{};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        try {
            if (action == 0) {
                log_line("Extension-root count is invalid; directional-shield ART settings retain defaults");
            } else if (action == 1) {
                const char* root = g_api->extension_root(
                    static_cast<std::uint32_t>(index));
                if (!root || !*root) return 0;
                ctx.row.emplace(Row{join_path(root, kArtConfigFileName), {}, {}});
                return 1;
            } else if (action == 2) {
                return read_small_art_config(
                    ctx.row->path, ctx.row->contents);
            } else if (action == 3) {
                ctx.row->report = parse_directional_shield_display_config(
                    ctx.row->contents, &g_directional_shield_display_config);
            } else if (action == 4) {
                if (ctx.row->report.valid_assignments != 0) {
                    char message[384]{};
                    std::snprintf(message, sizeof(message),
                        "Applied %u directional-shield ART assignment(s) from %s",
                        ctx.row->report.valid_assignments, ctx.row->path.c_str());
                    log_line(message);
                }
            } else if (action == 5) {
                if (ctx.row->report.invalid_assignments != 0) {
                    char message[384]{};
                    std::snprintf(message, sizeof(message),
                        "Ignored %u invalid directional-shield ART assignment(s) in %s",
                        ctx.row->report.invalid_assignments, ctx.row->path.c_str());
                    log_line(message);
                }
            } else if (action == 6) ctx.row.reset();
            else if (action == 7) {
                const auto& config = g_directional_shield_display_config;
                return config.position_mapping_configured &&
                    !valid_directional_shield_position_mapping(config);
            } else if (action == 8) {
                g_directional_shield_display_config.position_mapping_configured = false;
                log_line("Directional-shield ART positions contain duplicates; existing ODF positions retained");
            } else if (action == 9) {
                const auto& config = g_directional_shield_display_config;
                char message[256]{};
                std::snprintf(message, sizeof(message),
                    "Directional-shield ART display: mode=%d positions=%s F=%d A=%d P=%d S=%d",
                    config.display_mode,
                    config.position_mapping_configured ? "configured" : "ODF",
                    config.facing_positions[0], config.facing_positions[1],
                    config.facing_positions[2], config.facing_positions[3]);
                log_line(message);
            }
            return 0;
        } catch (...) {
            ctx.failure = std::current_exception();
            return 3;
        }
    };
    a2fo_rs_stage9_craft_load_art(step, &context, root_count);
    if (context.failure) std::rethrow_exception(context.failure);
}

bool resolve_energy_amount_api() noexcept {
    struct Context {
        HMODULE energy = nullptr;
        std::array<FARPROC, 9> exports{};
    } context{};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        static constexpr std::array<const char*, 9> names{{
            kGetPhotonTorpedoesExport, kGetQuantumTorpedoesExport,
            kGetShuttleCraftExport, kGetMaximumPhotonTorpedoesExport,
            kGetMaximumQuantumTorpedoesExport, kGetMaximumShuttleCraftExport,
            kGetPhotonTorpedoReloadSecondsExport,
            kGetQuantumTorpedoReloadSecondsExport,
            kGetShuttleCraftReloadSecondsExport}};
        if (action == 0) return g_energy_api_resolution_attempted;
        if (action == 1) return g_get_photon_torpedoes &&
            g_get_quantum_torpedoes && g_get_maximum_photon_torpedoes &&
            g_get_maximum_quantum_torpedoes;
        if (action == 2) {
            g_energy_api_resolution_attempted = true;
            ctx.energy = a2fo::module_provider(kEnergySystemsModuleName, {
                kGetPhotonTorpedoesExport, kGetQuantumTorpedoesExport,
                kGetShuttleCraftExport, kGetMaximumPhotonTorpedoesExport,
                kGetMaximumQuantumTorpedoesExport,
                kGetMaximumShuttleCraftExport,
                kGetPhotonTorpedoReloadSecondsExport,
                kGetQuantumTorpedoReloadSecondsExport,
                kGetShuttleCraftReloadSecondsExport,
                kGetResupplyWeaponStateExport});
            return ctx.energy != nullptr;
        }
        if (action == 3) {
            ctx.exports[index] = GetProcAddress(ctx.energy, names[index]);
        } else if (action == 4) {
            static_assert(sizeof(FARPROC) == sizeof(g_get_photon_torpedoes),
                "unexpected function-pointer size");
            switch (index) {
                case 0: std::memcpy(&g_get_photon_torpedoes, &ctx.exports[0],
                    sizeof(g_get_photon_torpedoes)); break;
                case 1: std::memcpy(&g_get_quantum_torpedoes, &ctx.exports[1],
                    sizeof(g_get_quantum_torpedoes)); break;
                case 2: std::memcpy(&g_get_shuttle_craft, &ctx.exports[2],
                    sizeof(g_get_shuttle_craft)); break;
                case 3: std::memcpy(&g_get_maximum_photon_torpedoes, &ctx.exports[3],
                    sizeof(g_get_maximum_photon_torpedoes)); break;
                case 4: std::memcpy(&g_get_maximum_quantum_torpedoes, &ctx.exports[4],
                    sizeof(g_get_maximum_quantum_torpedoes)); break;
                case 5: std::memcpy(&g_get_maximum_shuttle_craft, &ctx.exports[5],
                    sizeof(g_get_maximum_shuttle_craft)); break;
                case 6: std::memcpy(&g_get_photon_torpedo_reload_seconds, &ctx.exports[6],
                    sizeof(g_get_photon_torpedo_reload_seconds)); break;
                case 7: std::memcpy(&g_get_quantum_torpedo_reload_seconds, &ctx.exports[7],
                    sizeof(g_get_quantum_torpedo_reload_seconds)); break;
                case 8: std::memcpy(&g_get_shuttle_craft_reload_seconds, &ctx.exports[8],
                    sizeof(g_get_shuttle_craft_reload_seconds)); break;
                default: break;
            }
        } else if (action == 5) {
            return g_get_photon_torpedoes && g_get_quantum_torpedoes &&
                g_get_maximum_photon_torpedoes &&
                g_get_maximum_quantum_torpedoes;
        } else if (action == 6) {
            log_line(index
                ? "Ammunition display linked to A2FOEnergySystems value exports"
                : "A2FOEnergySystems value exports unavailable; ammunition display disabled");
        } else if (action == 7) {
            return !g_get_photon_torpedo_reload_seconds ||
                !g_get_quantum_torpedo_reload_seconds;
        } else if (action == 8) {
            log_line("Ammunition reload-time exports unavailable; value display mode 2 will show Resupply while below full");
        } else if (action == 9) {
            return !g_get_shuttle_craft || !g_get_maximum_shuttle_craft;
        } else if (action == 10) {
            log_line("Shuttle Craft value exports unavailable; Photon and Quantum display remains active");
        } else if (action == 11) {
            return !g_get_shuttle_craft_reload_seconds;
        } else if (action == 12) {
            log_line("Shuttle Craft reload-time export unavailable; value display mode 2 will show Resupply while below full");
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_resolve_energy(step, &context,
        context.exports.size());
}

bool resolve_directional_shields_api() noexcept {
    struct Context {
        HMODULE directional = nullptr;
        std::array<FARPROC, 4> exports{};
    } context{};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        static constexpr std::array<const char*, 4> names{{
            kDirectionalShieldsIsEnabledExport,
            kDirectionalShieldsGetCurrentExport,
            kDirectionalShieldsGetMaximumExport,
            kDirectionalShieldsGetModeExport}};
        if (action == 0) return g_directional_shields_api_resolution_attempted;
        if (action == 1) return g_directional_shields_is_enabled &&
            g_directional_shields_get_current &&
            g_directional_shields_get_maximum;
        if (action == 2) {
            g_directional_shields_api_resolution_attempted = true;
            ctx.directional = a2fo::module_provider(
                kDirectionalShieldsModuleName,
                {kDirectionalShieldsIsEnabledExport,
                 kDirectionalShieldsGetCurrentExport,
                 kDirectionalShieldsGetMaximumExport,
                 kDirectionalShieldsGetModeExport});
            return ctx.directional != nullptr;
        }
        if (action == 3) {
            ctx.exports[index] = GetProcAddress(ctx.directional, names[index]);
        } else if (action == 4) {
            static_assert(sizeof(FARPROC) ==
                sizeof(g_directional_shields_is_enabled),
                "unexpected function-pointer size");
            static_assert(sizeof(FARPROC) ==
                sizeof(g_directional_shields_get_mode),
                "unexpected function-pointer size");
            switch (index) {
                case 0: std::memcpy(&g_directional_shields_is_enabled,
                    &ctx.exports[0], sizeof(g_directional_shields_is_enabled)); break;
                case 1: std::memcpy(&g_directional_shields_get_current,
                    &ctx.exports[1], sizeof(g_directional_shields_get_current)); break;
                case 2: std::memcpy(&g_directional_shields_get_maximum,
                    &ctx.exports[2], sizeof(g_directional_shields_get_maximum)); break;
                case 3: std::memcpy(&g_directional_shields_get_mode,
                    &ctx.exports[3], sizeof(g_directional_shields_get_mode)); break;
                default: break;
            }
        } else if (action == 5) {
            return g_directional_shields_is_enabled &&
                g_directional_shields_get_current &&
                g_directional_shields_get_maximum;
        } else if (action == 6) {
            log_line(index
                ? (g_directional_shields_get_mode
                    ? "Directional-shield display linked to facing-value/mode exports"
                    : "Directional-shield display linked to legacy facing-value exports")
                : "Directional-shield value exports unavailable; facing display disabled");
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_resolve_directional(step, &context,
        context.exports.size());
}

void resolve_shield_class_observer() noexcept {
    struct Context { HMODULE provider = nullptr; FARPROC exported = nullptr; } context{};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            ctx.provider = GetModuleHandleA("A2FOExtensions.dll");
            ctx.exported = ctx.provider
                ? GetProcAddress(ctx.provider, kShieldClassObserverExport)
                : nullptr;
            return ctx.exported != nullptr;
        }
        if (action == 1) {
            ctx.provider = GetModuleHandleA(kAlwaysShowShieldsModuleName);
            ctx.exported = ctx.provider
                ? GetProcAddress(ctx.provider, kShieldClassObserverExport)
                : nullptr;
        } else if (action == 2) {
            static_assert(sizeof(FARPROC) == sizeof(g_shield_class_observer),
                "unexpected function-pointer size");
            std::memcpy(&g_shield_class_observer, &ctx.exported,
                sizeof(g_shield_class_observer));
        } else if (action == 3) return g_shield_class_observer != nullptr;
        else if (action == 4)
            log_line("Shield class registration linked through A2FOAlwaysShowShields");
        return 0;
    };
    a2fo_rs_stage9_craft_observer(step, &context, 1);
}

void resolve_nebula_class_observer() noexcept {
    struct Context { HMODULE renderer = nullptr; FARPROC exported = nullptr; } context{};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            ctx.renderer = a2fo::module_provider(
                kNebulaRendererModuleName, {kNebulaClassObserverExport});
            ctx.exported = ctx.renderer
                ? GetProcAddress(ctx.renderer, kNebulaClassObserverExport)
                : nullptr;
            return ctx.exported != nullptr;
        }
        if (action == 2) {
            static_assert(sizeof(FARPROC) == sizeof(g_nebula_class_observer),
                "unexpected function-pointer size");
            std::memcpy(&g_nebula_class_observer, &ctx.exported,
                sizeof(g_nebula_class_observer));
        } else if (action == 3) return g_nebula_class_observer != nullptr;
        else if (action == 4)
            log_line("Emissive-map class registration linked through A2FONebulaRenderer");
        return 0;
    };
    a2fo_rs_stage9_craft_observer(step, &context, 0);
}

void resolve_texture_variants_class_observer() noexcept {
    struct Context { HMODULE variants = nullptr; FARPROC exported = nullptr; } context{};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            ctx.variants = a2fo::module_provider(
                kTextureVariantsModuleName,
                {kTextureVariantsClassObserverExport});
            ctx.exported = ctx.variants
                ? GetProcAddress(ctx.variants, kTextureVariantsClassObserverExport)
                : nullptr;
            return ctx.exported != nullptr;
        }
        if (action == 2) {
            static_assert(sizeof(FARPROC) ==
                sizeof(g_texture_variants_class_observer),
                "unexpected function-pointer size");
            std::memcpy(&g_texture_variants_class_observer, &ctx.exported,
                sizeof(g_texture_variants_class_observer));
        } else if (action == 3)
            return g_texture_variants_class_observer != nullptr;
        else if (action == 4)
            log_line("Subsystem mesh class registration linked through A2FOTextureVariants");
        return 0;
    };
    a2fo_rs_stage9_craft_observer(step, &context, 0);
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

bool readable_range(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress);
    return start >= base && size <= information.RegionSize - (start - base);
}

bool executable_address_in_module(
    HMODULE module, const void* address) noexcept {
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

bool writable_range(void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const DWORD protection = information.Protect & 0xffu;
    const bool writable = protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
    if (!writable) return false;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress);
    return start >= base && size <= information.RegionSize - (start - base);
}

template <typename T>
T read_at(const void* object, std::size_t offset, T fallback = T{}) noexcept {
    if (!object || !readable_range(
            static_cast<const std::uint8_t*>(object) + offset,
            sizeof(T))) {
        return fallback;
    }
    T value{};
    std::memcpy(&value,
                static_cast<const std::uint8_t*>(object) + offset,
                sizeof(value));
    return value;
}

bool event_field_value(
    const A2FO_OdfFieldView* fields, std::uint32_t field_count,
    const char* name, std::string_view* output) noexcept {
    if (output) *output = {};
    if (!name || !output || (!fields && field_count != 0)) return false;
    const std::size_t name_size = std::strlen(name);
    struct Context {
        const A2FO_OdfFieldView* fields;
        const char* name;
        std::size_t name_size;
        std::string_view* output;
    } context{fields, name, name_size, output};
    const auto step = +[](void* opaque, std::uint32_t,
                          std::size_t index) noexcept -> std::uint32_t {
        auto& ctx = *static_cast<Context*>(opaque);
        const A2FO_OdfFieldView& field = ctx.fields[index];
        if (!field.name.data || field.name.size != ctx.name_size ||
            _strnicmp(field.name.data, ctx.name, ctx.name_size) != 0 ||
            (!field.value.data && field.value.size != 0)) return 0;
        *ctx.output = std::string_view(
            field.value.data ? field.value.data : "", field.value.size);
        return 1;
    };
    return a2fo_rs_stage10_identity_first_match(
        field_count, step, &context);
}

void A2FO_CALL weapon_class_loaded_handler(
    const A2FO_WeaponClassLoadedEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) ||
        !event->weapon_class) {
        return;
    }
    std::string_view classlabel;
    const bool passive = event_field_value(
        event->odf_fields, event->odf_field_count,
        "classLabel", &classlabel) &&
        a2fo::craft_identity::is_passive_weapon_classlabel(classlabel);
    try {
        if (passive) {
            g_passive_weapon_classes.insert(event->weapon_class);
        } else {
            g_passive_weapon_classes.erase(event->weapon_class);
        }
    } catch (...) {
        log_line("Could not retain a passive UtilityWeapon classification");
    }
}

bool weapon_vector_for_craft(
    void* craft, void*** begin_output,
    std::size_t* count_output) noexcept {
    if (begin_output) *begin_output = nullptr;
    if (count_output) *count_output = 0;
    void* weapon_system = read_at<void*>(
        craft, kWeaponSystemOnCraftOffset, nullptr);
    void** begin = read_at<void**>(
        weapon_system, kWeaponVectorBeginOffset, nullptr);
    void** end = read_at<void**>(
        weapon_system, kWeaponVectorEndOffset, nullptr);
    const std::uintptr_t begin_address =
        reinterpret_cast<std::uintptr_t>(begin);
    const std::uintptr_t end_address =
        reinterpret_cast<std::uintptr_t>(end);
    const std::uintptr_t byte_count = end_address >= begin_address
        ? end_address - begin_address : 0;
    const std::size_t weapon_count = static_cast<std::size_t>(
        byte_count / sizeof(void*));
    if (!begin || !end || end_address < begin_address ||
        byte_count % sizeof(void*) != 0 || weapon_count == 0 ||
        weapon_count > 256 ||
        !readable_range(begin, weapon_count * sizeof(void*))) {
        return false;
    }
    if (begin_output) *begin_output = begin;
    if (count_output) *count_output = weapon_count;
    return true;
}

void* weapon_for_ship_system_icon(
    void* icon, void** craft_output = nullptr) noexcept {
    if (craft_output) *craft_output = nullptr;
    if (!icon) return nullptr;
    void* craft = read_at<void*>(
        icon, kWeaponIconCraftOffset, nullptr);
    if (craft_output) *craft_output = craft;
    const std::int32_t weapon_index = read_at<std::int32_t>(
        icon, kWeaponIconIndexOffset, -1);
    void** begin = nullptr;
    std::size_t weapon_count = 0;
    if (weapon_index < 0 ||
        !weapon_vector_for_craft(craft, &begin, &weapon_count) ||
        static_cast<std::size_t>(weapon_index) >= weapon_count) {
        return nullptr;
    }
    return read_at<void*>(
        begin, static_cast<std::size_t>(weapon_index) * sizeof(void*),
        nullptr);
}

std::int32_t resupply_weapon_state(void* weapon) noexcept {
    if (!g_resupply_weapon_state_resolution_attempted) {
        g_resupply_weapon_state_resolution_attempted = true;
        HMODULE energy = a2fo::module_provider(
            kEnergySystemsModuleName, {kGetPhotonTorpedoesExport,
            kGetQuantumTorpedoesExport, kGetShuttleCraftExport,
            kGetMaximumPhotonTorpedoesExport,
            kGetMaximumQuantumTorpedoesExport,
            kGetMaximumShuttleCraftExport,
            kGetPhotonTorpedoReloadSecondsExport,
            kGetQuantumTorpedoReloadSecondsExport,
            kGetShuttleCraftReloadSecondsExport,
            kGetResupplyWeaponStateExport});
        FARPROC exported = energy
            ? GetProcAddress(energy, kGetResupplyWeaponStateExport)
            : nullptr;
        static_assert(sizeof(exported) == sizeof(g_get_resupply_weapon_state),
                      "unexpected function-pointer size");
        std::memcpy(&g_get_resupply_weapon_state, &exported,
                    sizeof(g_get_resupply_weapon_state));
    }
    return weapon && g_get_resupply_weapon_state
        ? g_get_resupply_weapon_state(weapon) : -1;
}

a2fo::craft_identity::WeaponIconKind weapon_icon_kind(
    void* weapon_class) noexcept {
    if (weapon_class &&
        g_passive_weapon_classes.find(weapon_class) !=
            g_passive_weapon_classes.end()) {
        return a2fo::craft_identity::WeaponIconKind::passive;
    }
    return read_at<std::uint8_t>(
               weapon_class, kWeaponClassSpecialOffset, 0) != 0
        ? a2fo::craft_identity::WeaponIconKind::special
        : a2fo::craft_identity::WeaponIconKind::normal;
}

struct WeaponTechnologyPresentation {
    a2fo::craft_identity::WeaponTechnologyState state =
        a2fo::craft_identity::WeaponTechnologyState::unknown;
    bool hide_when_unavailable = false;
};

WeaponTechnologyPresentation weapon_technology_presentation(
    void* craft, void* weapon_class) noexcept {
    using a2fo::craft_identity::WeaponTechnologyState;
    if (!craft || !weapon_class || !g_fleet_ops) {
        return {};
    }
    const void* project_id_object = read_at<const void*>(
        weapon_class, kWeaponClassProjectIdOffset, nullptr);
    const std::uint32_t project_id = read_at<std::uint32_t>(
        project_id_object, 0, 0);
    if (project_id == 0 || project_id > 0x7fffffffu) {
        return {};
    }
    const std::int32_t team_index = read_at<std::int32_t>(
        craft, kGameObjectTeamOffset, -1);
    if (team_index < 0 || team_index > kMaximumTeamIndex) {
        return {};
    }
    void* technology_trees = read_at<void*>(
        at(g_fleet_ops, kFoTeamTechnologyTreesPointerRva), 0, nullptr);
    void* team_tree = read_at<void*>(
        technology_trees,
        static_cast<std::size_t>(team_index) * sizeof(void*), nullptr);
    void* technology_items = read_at<void*>(
        team_tree, kTechnologyTreeItemsOffset, nullptr);
    if (!team_tree || !technology_items) {
        return {};
    }
    const std::uintptr_t items_address =
        reinterpret_cast<std::uintptr_t>(technology_items);
    const std::uintptr_t item_offset =
        static_cast<std::uintptr_t>(project_id - 1) * sizeof(void*);
    const std::uintptr_t item_address = items_address + item_offset;
    if (item_address < items_address || !readable_range(
            reinterpret_cast<const void*>(item_address), sizeof(void*))) {
        return {};
    }
    void* technology_item = read_at<void*>(
        reinterpret_cast<const void*>(item_address), 0, nullptr);
    if (!technology_item) {
        return {WeaponTechnologyState::available, false};
    }

    const bool available =
        (a2fo_identity_call_delphi_2(
             at(g_fleet_ops, kFoTechnologyTreeAllowsProjectRva),
             reinterpret_cast<std::uintptr_t>(team_tree),
             static_cast<std::uintptr_t>(project_id)) &
         0xffu) != 0;
    if (available) {
        return {WeaponTechnologyState::available, false};
    }

    // This is Fleet Operations' own buttonHideUnavailable evaluator. It
    // checks only requirements carrying that XML flag, so weaponXiconpos uses
    // exactly the same hidden-versus-disabled decision as ordinary buttons.
    const bool should_show_button =
        (a2fo_identity_call_delphi_2(
             at(g_fleet_ops, kFoTechnologyItemShouldShowButtonRva),
             reinterpret_cast<std::uintptr_t>(technology_item), 0) &
         0xffu) != 0;
    return {WeaponTechnologyState::unavailable, !should_show_button};
}

a2fo::craft_identity::WeaponIconPresentation weapon_icon_presentation(
    void* icon, void** craft_output = nullptr) noexcept {
    using a2fo::craft_identity::WeaponIconPresentation;
    void* craft = nullptr;
    void* weapon = weapon_for_ship_system_icon(icon, &craft);
    if (craft_output) *craft_output = craft;
    void* weapon_class = read_at<void*>(
        weapon, kWeaponClassOnWeaponOffset, nullptr);
    if (!weapon_class) return WeaponIconPresentation::live_status;
    const auto kind = weapon_icon_kind(weapon_class);
    const auto technology = kind ==
            a2fo::craft_identity::WeaponIconKind::passive
        ? WeaponTechnologyPresentation{
              a2fo::craft_identity::WeaponTechnologyState::available, false}
        : weapon_technology_presentation(craft, weapon_class);
    const auto presentation = a2fo::craft_identity::weapon_icon_presentation(
        kind, technology.state, technology.hide_when_unavailable);
    // Research-locked resupply weapons keep the disabled/hidden policy.
    if (presentation != WeaponIconPresentation::live_status) {
        return presentation;
    }
    const std::int32_t resupply_state = resupply_weapon_state(weapon);
    if (resupply_state < 0) return presentation;
    return resupply_state > 0
        ? WeaponIconPresentation::resupply_ready
        : WeaponIconPresentation::passive_neutral;
}

bool native_identity_is_space(std::uint8_t value) noexcept {
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

void trim_string(std::string* value) {
    if (!value) return;
    A2foIdentityTrimSpan span{};
    if (a2fo_rs_stage10_identity_trim(
            reinterpret_cast<const std::uint8_t*>(value->data()),
            value->size(), &native_identity_is_space, &span)) {
        *value = value->substr(span.begin, span.end - span.begin);
    }
}

bool copy_native_string(const char* source, std::string* output) {
    struct Context {
        const char* source;
        std::string* output;
        std::exception_ptr failure;
    } context{source, output, {}};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t length) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) return ctx.source && ctx.output;
        if (action == 1) return readable_range(ctx.source + length, 1);
        if (action == 2) return ctx.source[length] == '\0';
        if (action == 3) {
            try {
                ctx.output->assign(ctx.source, length);
                trim_string(ctx.output);
            } catch (...) {
                ctx.failure = std::current_exception();
                return 3;
            }
        }
        return 0;
    };
    const bool copied = a2fo_rs_stage9_craft_copy_native_string(
        step, &context, kMaximumIdentityLength);
    if (context.failure) std::rethrow_exception(context.failure);
    return copied;
}

bool valid_vector_bounds(const NativeStringVector& values,
                         std::size_t* count) noexcept {
    if (count) *count = 0;
    if (!values.begin && !values.end) return true;
    if (!values.begin || !values.end) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(values.begin);
    const auto end = reinterpret_cast<std::uintptr_t>(values.end);
    if (end < begin || (end - begin) % sizeof(char*) != 0) return false;
    const std::size_t entries = (end - begin) / sizeof(char*);
    if (entries > kMaximumIdentityEntries) return false;
    if (entries != 0 && !readable_range(
            values.begin, entries * sizeof(char*))) {
        return false;
    }
    if (count) *count = entries;
    return true;
}

void release_native_vector(NativeStringVector* values) noexcept {
    if (!values || !g_armada) return;
    std::size_t count = 0;
    if (!valid_vector_bounds(*values, &count)) return;
    using DeleteFn = void (__cdecl*)(void*);
    const auto engine_delete = reinterpret_cast<DeleteFn>(
        at(g_armada, kEngineOperatorDeleteRva));
    if (!engine_delete) return;
    struct Context {
        NativeStringVector* values;
        DeleteFn engine_delete;
    } context{values, engine_delete};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            if (ctx.values->begin[index])
                ctx.engine_delete(ctx.values->begin[index]);
        } else if (action == 1) {
            if (ctx.values->begin) ctx.engine_delete(ctx.values->begin);
        } else if (action == 2) *ctx.values = NativeStringVector{};
        return 0;
    };
    a2fo_rs_stage9_craft_release_native_vector(step, &context, count);
}

bool read_identity_list(void* parameter_db, const char* command,
                        std::vector<std::string>* output) noexcept {
    if (!parameter_db || !command || !output || !g_armada) return false;
    NativeStringVector native_values{};
    const std::uintptr_t found = a2fo_identity_call_thiscall_3(
        at(g_armada, kParameterDbGetStringVectorRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(command),
        reinterpret_cast<std::uintptr_t>(&native_values), 0);
    bool copied = false;
    try {
        std::size_t count = 0;
        if (valid_vector_bounds(native_values, &count)) {
            output->resize(count);
            struct CopyContext {
                const NativeStringVector* values;
                std::vector<std::string>* output;
                std::exception_ptr failure;
            } copy{&native_values, output, {}};
            const auto step = +[](void* opaque, std::uint32_t, std::uintptr_t index) noexcept -> std::uintptr_t {
                auto& ctx = *static_cast<CopyContext*>(opaque);
                try {
                    copy_native_string(ctx.values->begin[index], &(*ctx.output)[index]);
                } catch (...) { ctx.failure = std::current_exception(); return 3; }
                return 0;
            };
            a2fo_rs_stage9_craft_copy_identity_rows(step, &copy, count);
            if (copy.failure) std::rethrow_exception(copy.failure);
            copied = !output->empty();
        }
    } catch (...) {
        output->clear();
        log_line(std::string("Could not retain ") + command + " values");
    }
    release_native_vector(&native_values);
    return (found & 0xffu) != 0 && copied;
}

std::string class_odf_name(void* object_class) {
    if (!object_class || !g_armada) return {};
    const char* name = reinterpret_cast<const char*>(
        a2fo_identity_call_thiscall_0(
            at(g_armada, kGameObjectClassGetOdfNameRva), object_class));
    std::string copied;
    if (copy_native_string(name, &copied)) return copied;
    return {};
}

bool read_ui_rectangle(void* parameter_db, const char* key,
                       RawRectangle* output) noexcept;

bool read_parameter_string(
    void* parameter_db, const char* key, std::string* output) noexcept {
    if (output) output->clear();
    if (!parameter_db || !key || !*key || !output || !g_armada) {
        return false;
    }
    std::array<char, kMaximumIdentityLength> value{};
    const std::uintptr_t found = a2fo_identity_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(value.data()),
        static_cast<std::uintptr_t>(value.size()),
        reinterpret_cast<std::uintptr_t>(""));
    value.back() = '\0';
    if ((found & 0xffu) == 0) return false;
    try {
        *output = value.data();
        trim_string(output);
        return true;
    } catch (...) {
        output->clear();
        return false;
    }
}

bool parse_display_mode(
    const std::string& value, AmmunitionDisplayMode* mode) noexcept {
    if (!mode || value.empty()) return false;
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (!end || !a2fo_rs_stage10_identity_display_tail(
            reinterpret_cast<const std::uint8_t*>(end),
            value.c_str() + value.size() - end + 1,
            parsed, &native_identity_is_space)) return false;
    *mode = static_cast<AmmunitionDisplayMode>(parsed);
    return true;
}

bool read_parameter_int(
    void* parameter_db, const char* key, int* output) noexcept {
    if (!parameter_db || !key || !*key || !output || !g_armada) {
        return false;
    }
    int value = *output;
    const std::uintptr_t found = a2fo_identity_call_thiscall_3(
        at(g_armada, kParameterDbGetIntRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(&value),
        static_cast<std::uintptr_t>(*output));
    if ((found & 0xffu) == 0) return false;
    *output = value;
    return true;
}

void register_selected_status_ui_policy(
    void* object_class, void* parameter_db) noexcept {
    struct Context {
        void* object_class;
        void* parameter_db;
        SelectedStatusUiPolicy policy{};
    } context{object_class, parameter_db, {}};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) return ctx.object_class && ctx.parameter_db;
        if (action == 1) return read_parameter_string(
            ctx.parameter_db, kShieldTooltipCommand, &ctx.policy.shield_tooltip);
        if (action == 2) return read_parameter_string(
            ctx.parameter_db, kShieldVerboseTooltipCommand, &ctx.policy.shield_verbose_tooltip);
        if (action == 3) g_selected_status_ui_policies.erase(ctx.object_class);
        else if (action == 4) {
            try { g_selected_status_ui_policies[ctx.object_class] = std::move(ctx.policy); }
            catch (...) {
                g_selected_status_ui_policies.erase(ctx.object_class);
                log_line("Could not retain selected-status tooltip policy");
            }
        }
        return 0;
    };
    a2fo_rs_stage9_craft_register_selected_status(step, &context);
}

void register_ammunition_ui_policy(
    void* object_class, void* parameter_db) noexcept {
    struct Context {
        void* object_class;
        void* parameter_db;
        AmmunitionUiPolicy policy{};
        bool any_found = false;
        int numeric_mode = 0;
    } context{object_class, parameter_db, {}, false, 0};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t packed) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) return ctx.object_class && ctx.parameter_db;
        const std::size_t index = action == 5 ? packed / 4 : packed;
        if (action >= 1 && action <= 6) {
            AmmunitionPresentation& store = ctx.policy.stores[index];
            if (action == 1) {
                ctx.numeric_mode = static_cast<int>(store.display_mode);
                return read_parameter_int(ctx.parameter_db,
                    kAmmunitionDisplayModeCommands[index], &ctx.numeric_mode);
            }
            if (action == 2) {
                if (ctx.numeric_mode < static_cast<int>(AmmunitionDisplayMode::text) ||
                    ctx.numeric_mode > static_cast<int>(AmmunitionDisplayMode::icon)) {
                    log_line(std::string("Ignored invalid ") +
                             kAmmunitionDisplayModeCommands[index] +
                             "; use 1 for text or 2 for icon");
                } else {
                    store.display_mode = static_cast<AmmunitionDisplayMode>(ctx.numeric_mode);
                    ctx.any_found = true;
                }
            } else if (action == 3) {
                std::string mode;
                if (read_parameter_string(ctx.parameter_db,
                        kAmmunitionDisplayModeCommands[index], &mode)) {
                    if (!parse_display_mode(mode, &store.display_mode)) {
                        log_line(std::string("Ignored invalid ") +
                                 kAmmunitionDisplayModeCommands[index] +
                                 "; use 1 for text or 2 for icon");
                    } else ctx.any_found = true;
                }
            } else if (action == 4) {
                int value_mode = static_cast<int>(store.value_display_mode);
                if (read_parameter_int(ctx.parameter_db,
                        kAmmunitionValueDisplayModeCommands[index], &value_mode)) {
                    if (value_mode < static_cast<int>(AmmunitionValueDisplayMode::percent) ||
                        value_mode > static_cast<int>(AmmunitionValueDisplayMode::bar)) {
                        log_line(std::string("Ignored invalid ") +
                                 kAmmunitionValueDisplayModeCommands[index] +
                                 "; use 0 for percent, 1 for current/maximum, 2 for reload status, or 3 for a capacity bar");
                    } else {
                        store.value_display_mode = static_cast<AmmunitionValueDisplayMode>(value_mode);
                        ctx.any_found = true;
                    }
                }
            } else if (action == 5) {
                const std::size_t field = packed % 4;
                const char* commands[]{kAmmunitionLabelCommands[index],
                    kAmmunitionTooltipCommands[index],
                    kAmmunitionVerboseTooltipCommands[index],
                    kAmmunitionIconCommands[index]};
                std::string* values[]{&store.label, &store.tooltip,
                    &store.verbose_tooltip, &store.icon};
                ctx.any_found = read_parameter_string(
                    ctx.parameter_db, commands[field], values[field]) || ctx.any_found;
            } else if (action == 6) {
                RawRectangle icon_position{};
                if (read_ui_rectangle(ctx.parameter_db,
                        kAmmunitionIconPositionCommands[index], &icon_position)) {
                    if (icon_position.width <= 0 || icon_position.height <= 0) {
                        log_line(std::string("Ignored invalid ") +
                                 kAmmunitionIconPositionCommands[index] +
                                 "; width and height must be positive");
                    } else {
                        store.icon_position_found = true;
                        store.icon_position = icon_position;
                        ctx.any_found = true;
                    }
                }
            }
            return 0;
        }
        if (action == 7) return ctx.any_found;
        if (action == 8) g_ammunition_ui_policies.erase(ctx.object_class);
        else if (action == 9) {
            try {
                g_ammunition_ui_policies[ctx.object_class] = std::move(ctx.policy);
                const std::string odf = class_odf_name(ctx.object_class);
                const AmmunitionUiPolicy& retained = g_ammunition_ui_policies[ctx.object_class];
                char message[640]{};
                std::snprintf(message, sizeof(message),
                    "Registered ammunition UI on '%s': Photon=%s/value%d%s Quantum=%s/value%d%s Shuttle Craft=%s/value%d%s",
                    odf.empty() ? "<unknown>" : odf.c_str(),
                    retained.stores[0].display_mode == AmmunitionDisplayMode::icon ? "icon" : "text",
                    static_cast<int>(retained.stores[0].value_display_mode),
                    retained.stores[0].label.empty() ? "" : "/label",
                    retained.stores[1].display_mode == AmmunitionDisplayMode::icon ? "icon" : "text",
                    static_cast<int>(retained.stores[1].value_display_mode),
                    retained.stores[1].label.empty() ? "" : "/label",
                    retained.stores[2].display_mode == AmmunitionDisplayMode::icon ? "icon" : "text",
                    static_cast<int>(retained.stores[2].value_display_mode),
                    retained.stores[2].label.empty() ? "" : "/label");
                log_line(message);
            } catch (...) {
                g_ammunition_ui_policies.erase(ctx.object_class);
                log_line("Could not retain ammunition UI policy");
            }
        }
        return 0;
    };
    a2fo_rs_stage9_craft_register_ammunition(step, &context, context.policy.stores.size());
}

void register_directional_shield_ui_policy(
    void* object_class, void* parameter_db) noexcept {
    struct Context {
        void* object_class;
        void* parameter_db;
        DirectionalShieldUiPolicy policy{};
        bool any_found = false;
        int value_display_mode = 0;
    } context{object_class, parameter_db, {}, false, 0};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) return ctx.object_class && ctx.parameter_db;
        if (action == 1) {
            if (read_parameter_int(ctx.parameter_db,
                    kDirectionalShieldValueDisplayModeCommand, &ctx.value_display_mode)) {
                if (ctx.value_display_mode < 0 || ctx.value_display_mode > 2) {
                    log_line(std::string("Ignored invalid ") +
                             kDirectionalShieldValueDisplayModeCommand + "; expected 0, 1, or 2");
                } else {
                    ctx.policy.value_display_mode_found = true;
                    ctx.policy.value_display_mode = static_cast<
                        a2fo::craft_identity::DirectionalShieldValueDisplayMode>(ctx.value_display_mode);
                    ctx.any_found = true;
                }
            }
        } else if (action == 2) {
            RawRectangle rectangle{};
            if (!read_ui_rectangle(ctx.parameter_db,
                    kDirectionalShieldPositionCommands[index], &rectangle)) return 0;
            if (rectangle.width <= 0 || rectangle.height <= 0) {
                log_line(std::string("Ignored invalid ") +
                         kDirectionalShieldPositionCommands[index] +
                         "; width and height must be positive");
                return 0;
            }
            ctx.policy.position_found[index] = true;
            ctx.policy.positions[index] = rectangle;
            ctx.any_found = true;
        } else if (action == 3) return ctx.any_found;
        else if (action == 4) g_directional_shield_ui_policies.erase(ctx.object_class);
        else if (action == 5) {
            try {
                g_directional_shield_ui_policies[ctx.object_class] = ctx.policy;
                const std::string odf = class_odf_name(ctx.object_class);
                char message[384]{};
                std::snprintf(message, sizeof(message),
                    "Registered directional-shield UI on '%s': values=%s F=%s A=%s P=%s S=%s",
                    odf.empty() ? "<unknown>" : odf.c_str(),
                    ctx.policy.value_display_mode_found
                        ? (ctx.value_display_mode == 0 ? "off" :
                           ctx.value_display_mode == 1 ? "percent" : "amount") : "legacy",
                    ctx.policy.position_found[0] ? "ODF" : "default",
                    ctx.policy.position_found[1] ? "ODF" : "default",
                    ctx.policy.position_found[2] ? "ODF" : "default",
                    ctx.policy.position_found[3] ? "ODF" : "default");
                log_line(message);
            } catch (...) {
                g_directional_shield_ui_policies.erase(ctx.object_class);
                log_line("Could not retain directional-shield UI policy");
            }
        }
        return 0;
    };
    a2fo_rs_stage9_craft_register_directional(
        step, &context, kDirectionalShieldPositionCommands.size());
}

void register_class_policy(void* object_class, void* parameter_db) noexcept {
    struct Context {
        void* object_class;
        void* parameter_db;
        ClassIdentityPolicy policy{};
        std::string odf;
        std::size_t craft_name_count = 0, captain_count = 0, registry_count = 0;
        std::exception_ptr failure;
    } context{object_class, parameter_db, {}, {}, 0, 0, 0, {}};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) return g_runtime_ready && ctx.object_class && ctx.parameter_db;
        if (action == 1) { register_directional_shield_ui_policy(ctx.object_class, ctx.parameter_db); return 0; }
        if (action == 2) { register_ammunition_ui_policy(ctx.object_class, ctx.parameter_db); return 0; }
        if (action == 3) { register_selected_status_ui_policy(ctx.object_class, ctx.parameter_db); return 0; }
        if (action == 4) { read_identity_list(ctx.parameter_db, kCraftNameCommand, &ctx.policy.craft_names); return 0; }
        if (action == 5) { read_identity_list(ctx.parameter_db, kCaptainCommand, &ctx.policy.captain_names); return 0; }
        if (action == 6) { read_identity_list(ctx.parameter_db, kRegistryCommand, &ctx.policy.craft_registries); return 0; }
        if (action == 7) return !ctx.policy.captain_names.empty() || !ctx.policy.craft_registries.empty();
        try {
            if (action == 8) {
                ctx.policy.odf_name = class_odf_name(ctx.object_class);
                ctx.craft_name_count = ctx.policy.craft_names.size();
                ctx.captain_count = ctx.policy.captain_names.size();
                ctx.registry_count = ctx.policy.craft_registries.size();
                ctx.odf = ctx.policy.odf_name;
            } else if (action == 9) {
                g_class_policies[ctx.object_class] = std::move(ctx.policy);
            } else if (action == 10) {
                char message[320]{};
                std::snprintf(message, sizeof(message),
                    "Registered identity rows on '%s': %lu craft name%s, "
                    "%lu captain%s, %lu registr%s",
                    ctx.odf.empty() ? "<unknown>" : ctx.odf.c_str(),
                    static_cast<unsigned long>(ctx.craft_name_count),
                    ctx.craft_name_count == 1 ? "" : "s",
                    static_cast<unsigned long>(ctx.captain_count),
                    ctx.captain_count == 1 ? "" : "s",
                    static_cast<unsigned long>(ctx.registry_count),
                    ctx.registry_count == 1 ? "y" : "ies");
                log_line(message);
            } else if (action == 11) return ctx.craft_name_count == 0;
            else if (action == 12) {
                log_line(std::string("Identity rows on '") +
                         (ctx.odf.empty() ? "<unknown>" : ctx.odf) +
                         "' cannot be selected without possibleCraftNames");
            } else if (action == 13) {
                return (!g_class_policies[ctx.object_class].captain_names.empty() &&
                        ctx.captain_count != ctx.craft_name_count) ||
                       (!g_class_policies[ctx.object_class].craft_registries.empty() &&
                        ctx.registry_count != ctx.craft_name_count);
            } else if (action == 14) {
                log_line(std::string("Identity row-count mismatch on '") +
                         (ctx.odf.empty() ? "<unknown>" : ctx.odf) +
                         "'; out-of-range companion rows will remain blank");
            }
        } catch (...) { ctx.failure = std::current_exception(); return 3; }
        return 0;
    };
    a2fo_rs_stage9_craft_register_class(step, &context);
    if (context.failure) log_line("Could not retain a craft identity class policy");
}

std::uintptr_t __attribute__((fastcall)) craft_class_constructor_hook(
    void* self, void*, void* parent_class, void* parameter_db) noexcept {
    const std::uintptr_t result = a2fo_identity_call_thiscall_2(
        g_craft_class_constructor_original, self,
        reinterpret_cast<std::uintptr_t>(parent_class),
        reinterpret_cast<std::uintptr_t>(parameter_db));
    register_class_policy(self, parameter_db);
    // CraftClass is the common completed-ODF boundary for both ships and
    // stations. Forward it to the optional shield module because the more
    // general GameObjectClass constructor is not entered by every derived
    // Fleet Operations class path.
    if (g_shield_class_observer) {
        g_shield_class_observer(self, parameter_db);
    }
    // A2FONebulaRenderer loads after this module alphabetically. Resolve its
    // optional observer lazily at the first completed CraftClass rather than
    // requiring a fragile module order or a second constructor hook.
    if (!g_nebula_class_observer) resolve_nebula_class_observer();
    if (g_nebula_class_observer) {
        g_nebula_class_observer(self, parameter_db);
    }
    // TextureVariants is alphabetically later than this module, so resolve
    // its optional completed-CraftClass observer lazily as well.
    if (!g_texture_variants_class_observer) {
        resolve_texture_variants_class_observer();
    }
    if (g_texture_variants_class_observer) {
        g_texture_variants_class_observer(self, parameter_db);
    }
    return result;
}

const CraftIdentity* ensure_craft_identity(void* craft) noexcept {
    if (!craft) return nullptr;
    const std::uint32_t handle = read_at<std::uint32_t>(
        craft, kObjectHandleOffset, 0);
    void* object_class = read_at<void*>(craft, kObjectClassOffset, nullptr);
    const std::int32_t craft_name_index = read_at<std::int32_t>(
        craft, kCraftNameIndexOffset, -1);
    const auto* manual = a2fo::object_editor::overrides(craft);
    const bool has_manual = manual &&
        (manual->flags & (a2fo::object_editor::kCaptain | a2fo::object_editor::kRegistry));
    const auto editor_revision = a2fo::object_editor::revision(craft);
    struct Context {
        std::uint32_t handle;
        void* object_class;
        std::int32_t craft_name_index;
        const a2fo::object_editor::SavedState* manual;
        bool has_manual;
        std::uint64_t editor_revision;
        decltype(g_class_policies)::iterator policy{};
        const ClassIdentityPolicy* class_policy = nullptr;
        CraftIdentity identity{};
        std::size_t aligned_index = 0;
        const CraftIdentity* result = nullptr;
        std::exception_ptr failure;
    } context{handle, object_class, craft_name_index, manual, has_manual,
              editor_revision, {}, nullptr, {}, 0, nullptr, {}};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) return ctx.handle != 0 && ctx.object_class &&
            (ctx.craft_name_index >= 0 || ctx.has_manual);
        if (action == 1) {
            ctx.policy = g_class_policies.find(ctx.object_class);
            if (ctx.policy == g_class_policies.end() && !ctx.has_manual) {
                g_craft_identities.erase(ctx.handle);
                return 0;
            }
            return 1;
        }
        if (action == 2) {
            const auto present = g_craft_identities.find(ctx.handle);
            if (present != g_craft_identities.end() &&
                present->second.object_class == ctx.object_class &&
                present->second.craft_name_index == ctx.craft_name_index &&
                present->second.editor_revision == ctx.editor_revision) {
                ctx.result = &present->second;
                return 1;
            }
            return 0;
        }
        try {
            if (action == 3) {
                ctx.identity.object_class = ctx.object_class;
                ctx.identity.craft_name_index = ctx.craft_name_index;
                ctx.identity.editor_revision = ctx.editor_revision;
                static const ClassIdentityPolicy empty_policy{};
                ctx.class_policy = ctx.policy == g_class_policies.end()
                    ? &empty_policy : &ctx.policy->second;
            } else if (action == 4) {
                if (a2fo::craft_identity::aligned_identity_index(
                        ctx.craft_name_index, ctx.class_policy->captain_names.size(),
                        &ctx.aligned_index))
                    ctx.identity.captain_name = ctx.class_policy->captain_names[ctx.aligned_index];
            } else if (action == 5) {
                if (a2fo::craft_identity::aligned_identity_index(
                        ctx.craft_name_index, ctx.class_policy->craft_registries.size(),
                        &ctx.aligned_index))
                    ctx.identity.craft_registry = ctx.class_policy->craft_registries[ctx.aligned_index];
            } else if (action == 6) {
                if (ctx.manual && (ctx.manual->flags & a2fo::object_editor::kCaptain))
                    ctx.identity.captain_name = ctx.manual->captain.data();
            } else if (action == 7) {
                if (ctx.manual && (ctx.manual->flags & a2fo::object_editor::kRegistry))
                    ctx.identity.craft_registry = ctx.manual->registry.data();
            } else if (action == 8) {
                auto inserted = g_craft_identities.insert_or_assign(
                    ctx.handle, std::move(ctx.identity));
                ctx.result = &inserted.first->second;
            } else if (action == 9) {
                const LONG report = InterlockedIncrement(&g_assignment_report_count);
                if (report <= 32) {
                    char message[512]{};
                    std::snprintf(message, sizeof(message),
                        "Aligned craft %lu from '%s' to native name row %ld "
                        "('%s'): captain='%s', registry='%s'",
                        static_cast<unsigned long>(ctx.handle),
                        ctx.class_policy->odf_name.empty()
                            ? "<unknown>" : ctx.class_policy->odf_name.c_str(),
                        static_cast<long>(ctx.craft_name_index),
                        static_cast<std::size_t>(ctx.craft_name_index) <
                                ctx.class_policy->craft_names.size()
                            ? ctx.class_policy->craft_names[
                                  static_cast<std::size_t>(ctx.craft_name_index)].c_str()
                            : "<out-of-range>",
                        ctx.result->captain_name.empty()
                            ? "<none>" : ctx.result->captain_name.c_str(),
                        ctx.result->craft_registry.empty()
                            ? "<none>" : ctx.result->craft_registry.c_str());
                    log_line(message);
                } else if (report == 33) {
                    log_line("Further per-craft identity assignment reports suppressed");
                }
            }
        } catch (...) { ctx.failure = std::current_exception(); return 3; }
        return 0;
    };
    a2fo_rs_stage9_craft_ensure_identity(step, &context);
    if (context.failure) {
        g_craft_identities.erase(handle);
        log_line("Could not allocate per-craft identity state");
        return nullptr;
    }
    return context.result;
}

void* gui_parameter_db() noexcept {
    return read_at<void*>(at(g_armada, kGuiParameterDbPointerRva), 0,
                          nullptr);
}

bool read_ui_rectangle(void* parameter_db, const char* key,
                       RawRectangle* output) noexcept {
    if (!parameter_db || !key || !output) return false;
    const RawRectangle fallback{};
    const std::uintptr_t found = a2fo_identity_call_thiscall_3(
        at(g_armada, kParameterDbGetRectangleRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(output),
        reinterpret_cast<std::uintptr_t>(&fallback));
    return (found & 0xffu) != 0;
}

bool read_ui_colour(void* parameter_db, const char* key,
                    Colour* output) noexcept {
    if (!parameter_db || !key || !output) return false;
    const Colour fallback{};
    const std::uintptr_t found = a2fo_identity_call_thiscall_3(
        at(g_armada, kParameterDbGetColorRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(output),
        reinterpret_cast<std::uintptr_t>(&fallback));
    return (found & 0xffu) != 0;
}

void read_editor_identity_defaults(void* craft, char* captain, char* registry,
                                   std::size_t capacity) noexcept {
    if (!captain || !registry || capacity == 0) return;
    captain[0] = registry[0] = 0;
    const auto found = g_class_policies.find(
        read_at<void*>(craft, kObjectClassOffset, nullptr));
    if (found == g_class_policies.end()) return;
    const auto name_index = read_at<std::int32_t>(craft, kCraftNameIndexOffset, -1);
    std::size_t index = 0;
    if (a2fo::craft_identity::aligned_identity_index(
            name_index, found->second.captain_names.size(), &index))
        std::snprintf(captain, capacity, "%s", found->second.captain_names[index].c_str());
    if (a2fo::craft_identity::aligned_identity_index(
            name_index, found->second.craft_registries.size(), &index))
        std::snprintf(registry, capacity, "%s", found->second.craft_registries[index].c_str());
}

const PanelElementStyle& panel_style(PanelElement element) noexcept {
    return g_panel_styles[g_active_panel][static_cast<std::size_t>(element)];
}
NativeRectangle panel_rectangle(PanelElement element,
                                NativeRectangle fallback) noexcept {
    const auto& style = panel_style(element);
    if (!style.area_found) return fallback;
    // Match LoadRectangleWithOffset and its inclusive right/bottom edges.
    const auto& r = style.area;
    const auto offset = read_at<std::int32_t>(g_active_info_display, 0x1f0, 0);
    return {r.x, r.y - offset, r.x + r.width - 1,
            r.y - offset + r.height - 1};
}
Colour panel_colour(PanelElement element, Colour fallback,
                    float ratio = 1.0f) noexcept {
    const auto& style = panel_style(element);
    if (ratio <= 0.25f && style.critical_found) return style.critical;
    if (ratio <= 0.5f && style.low_found) return style.low;
    return style.colour_found ? style.colour : fallback;
}
constexpr std::array<PanelElement, 3> kAmmoRows{{
    PanelElement::PhotonTorpedoesText, PanelElement::QuantumTorpedoesText,
    PanelElement::ShuttleCraftText}};
constexpr std::array<PanelElement, 3> kAmmoLabels{{
    PanelElement::PhotonTorpedoesLabelText, PanelElement::QuantumTorpedoesLabelText,
    PanelElement::ShuttleCraftLabelText}};
constexpr std::array<PanelElement, 3> kAmmoValues{{
    PanelElement::PhotonTorpedoesValueText, PanelElement::QuantumTorpedoesValueText,
    PanelElement::ShuttleCraftValueText}};
constexpr std::array<PanelElement, 3> kAmmoIcons{{
    PanelElement::PhotonTorpedoesIcon, PanelElement::QuantumTorpedoesIcon,
    PanelElement::ShuttleCraftIcon}};
constexpr std::array<PanelElement, 3> kAmmoBars{{
    PanelElement::PhotonTorpedoesBar, PanelElement::QuantumTorpedoesBar,
    PanelElement::ShuttleCraftBar}};
constexpr std::array<PanelElement, 4> kShieldSegments{{
    PanelElement::DirectionalShieldsForward, PanelElement::DirectionalShieldsAft,
    PanelElement::DirectionalShieldsPort, PanelElement::DirectionalShieldsStarboard}};
constexpr std::array<PanelElement, 4> kShieldValues{{
    PanelElement::DirectionalShieldsForwardValueText, PanelElement::DirectionalShieldsAftValueText,
    PanelElement::DirectionalShieldsPortValueText, PanelElement::DirectionalShieldsStarboardValueText}};

void load_panel_styles(void* db) noexcept {
    struct Row {
        PanelElementStyle* style;
        std::string prefix, value;
    };
    struct Context {
        void* db;
        std::size_t panel = 0;
        std::optional<Row> row;
        std::exception_ptr failure;
    } context{db, 0, {}, {}};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        try {
            if (action == 0) g_panel_styles = {};
            else if (action == 1) return ctx.db != nullptr;
            else if (action == 2) {
                ctx.panel = index;
                if (index == 1) g_panel_styles[1] = g_panel_styles[0];
            } else if (action == 3) {
                ctx.row.emplace(Row{
                    &g_panel_styles[ctx.panel][index],
                    std::string(ctx.panel ? "infoBuild" : "infoSingle") + kPanelElementNames[index],
                    {}});
            } else if (action == 4) {
                RawRectangle area{};
                if (read_ui_rectangle(ctx.db, (ctx.row->prefix + "Area").c_str(), &area) &&
                    area.x >= -32768 && area.x <= 32767 &&
                    area.y >= -32768 && area.y <= 32767 &&
                    area.width >= 0 && area.width <= 32767 &&
                    area.height >= 0 && area.height <= 32767) {
                    ctx.row->style->area = area;
                    ctx.row->style->area_found = true;
                }
            } else if (action == 5) {
                static constexpr const char* suffixes[]{"Color", "LowColor", "CriticalColor", "BackgroundColor"};
                bool* found[]{&ctx.row->style->colour_found, &ctx.row->style->low_found,
                              &ctx.row->style->critical_found, &ctx.row->style->background_found};
                Colour* target[]{&ctx.row->style->colour, &ctx.row->style->low,
                                 &ctx.row->style->critical, &ctx.row->style->background};
                Colour value{};
                if (read_ui_colour(ctx.db, (ctx.row->prefix + suffixes[index]).c_str(), &value) &&
                    std::isfinite(value.red) && std::isfinite(value.green) &&
                    std::isfinite(value.blue)) {
                    *target[index] = value;
                    *found[index] = true;
                }
            } else if (action == 6) {
                if (index == 0) {
                    if (read_parameter_string(ctx.db, (ctx.row->prefix + "Sprite").c_str(),
                                              &ctx.row->value))
                        ctx.row->style->sprite = ctx.row->value;
                } else if (read_parameter_string(ctx.db,
                        (ctx.row->prefix + "BackgroundSprite").c_str(), &ctx.row->value)) {
                    ctx.row->style->background_sprite = ctx.row->value;
                }
            } else if (action == 7) ctx.row.reset();
            return 0;
        } catch (...) { ctx.failure = std::current_exception(); return 3; }
    };
    a2fo_rs_stage9_craft_panel_styles(step, &context, kPanelElementNames.size());
    if (context.failure) std::rethrow_exception(context.failure);
}

void refresh_ui_configuration(void* parameter_db) noexcept {
    // Armada rebuilds GUI and ST3D sprite objects between missions. The old
    // pointers remain readable long enough to look plausible, but their
    // internal frame list has already been released. Drop every presentation
    // cache whenever the companion GUI database changes.
    UiConfiguration loaded{};
    loaded.parameter_db = parameter_db;
    loaded.loaded = true;
    struct RectangleRead {
        const char* key;
        bool UiConfiguration::*found;
        RawRectangle UiConfiguration::*rectangle;
    };
    static constexpr std::array<RectangleRead, 15> rectangle_reads{{
        {"infoSingleNameTextArea", &UiConfiguration::single_name_rectangle_found,
         &UiConfiguration::single_name_rectangle},
        {"infoSingleClassTextArea", &UiConfiguration::single_class_rectangle_found,
         &UiConfiguration::single_class_rectangle},
        {"infoBuildName", &UiConfiguration::builder_name_rectangle_found,
         &UiConfiguration::builder_name_rectangle},
        {"infoBuildClass", &UiConfiguration::builder_class_rectangle_found,
         &UiConfiguration::builder_class_rectangle},
        {"infoSingleCaptainTextArea", &UiConfiguration::captain_rectangle_found,
         &UiConfiguration::captain_rectangle},
        {"infoSingleRegistryTextArea", &UiConfiguration::registry_rectangle_found,
         &UiConfiguration::registry_rectangle},
        {"infoSinglePhotonTorpedoesTextArea", &UiConfiguration::photon_rectangle_found,
         &UiConfiguration::photon_rectangle},
        {"infoSingleQuantumTorpedoesTextArea", &UiConfiguration::quantum_rectangle_found,
         &UiConfiguration::quantum_rectangle},
        {"infoSingleShuttleCraftTextArea", &UiConfiguration::shuttle_craft_rectangle_found,
         &UiConfiguration::shuttle_craft_rectangle},
        {"infoSingleDirectionalShieldsForwardAftTextArea",
         &UiConfiguration::directional_forward_aft_rectangle_found,
         &UiConfiguration::directional_forward_aft_rectangle},
        {"infoSingleDirectionalShieldsPortStarboardTextArea",
         &UiConfiguration::directional_port_starboard_rectangle_found,
         &UiConfiguration::directional_port_starboard_rectangle},
        {"infoSingleDirectionalShieldsGraphicArea",
         &UiConfiguration::directional_graphic_rectangle_found,
         &UiConfiguration::directional_graphic_rectangle},
        {"infoSingleShieldBarArea", &UiConfiguration::shield_bar_rectangle_found,
         &UiConfiguration::shield_bar_rectangle},
        {"infoBuildShieldBar", &UiConfiguration::builder_shield_bar_rectangle_found,
         &UiConfiguration::builder_shield_bar_rectangle},
        {"infoSingleExperienceBarArea", &UiConfiguration::experience_bar_rectangle_found,
         &UiConfiguration::experience_bar_rectangle},
    }};
    static constexpr const char* rectangle_fallbacks[]{
        "infoBuildNameTextArea", "infoBuildClassTextArea", "captainName", "shipRegistry"};
    struct ColourRead {
        const char* key;
        bool UiConfiguration::*found;
        Colour UiConfiguration::*colour;
    };
    static constexpr std::array<ColourRead, 19> colour_reads{{
        {"infoTextColor", &UiConfiguration::shared_text_colour_found, &UiConfiguration::shared_text_colour},
        {"shipNameColor", &UiConfiguration::ship_name_colour_found, &UiConfiguration::ship_name_colour},
        {"captainNameColor", &UiConfiguration::captain_colour_found, &UiConfiguration::captain_colour},
        {"shipRegistryColor", &UiConfiguration::registry_colour_found, &UiConfiguration::registry_colour},
        {"photonTorpedoColor", &UiConfiguration::photon_colour_found, &UiConfiguration::photon_colour},
        {"quantumTorpedoColor", &UiConfiguration::quantum_colour_found, &UiConfiguration::quantum_colour},
        {"shuttleCraftColor", &UiConfiguration::shuttle_craft_colour_found, &UiConfiguration::shuttle_craft_colour},
        {"photonTorpedoLowColor", &UiConfiguration::photon_low_colour_found, &UiConfiguration::photon_low_colour},
        {"quantumTorpedoLowColor", &UiConfiguration::quantum_low_colour_found, &UiConfiguration::quantum_low_colour},
        {"shuttleCraftLowColor", &UiConfiguration::shuttle_craft_low_colour_found, &UiConfiguration::shuttle_craft_low_colour},
        {"photonTorpedoCriticalColor", &UiConfiguration::photon_critical_colour_found, &UiConfiguration::photon_critical_colour},
        {"quantumTorpedoCriticalColor", &UiConfiguration::quantum_critical_colour_found, &UiConfiguration::quantum_critical_colour},
        {"shuttleCraftCriticalColor", &UiConfiguration::shuttle_craft_critical_colour_found, &UiConfiguration::shuttle_craft_critical_colour},
        {"directionalShieldColor", &UiConfiguration::directional_shield_colour_found, &UiConfiguration::directional_shield_colour},
        {"directionalShieldLowColor", &UiConfiguration::directional_shield_low_colour_found, &UiConfiguration::directional_shield_low_colour},
        {"directionalShieldCriticalColor", &UiConfiguration::directional_shield_critical_colour_found, &UiConfiguration::directional_shield_critical_colour},
        {"directionalShieldValueColor", &UiConfiguration::directional_shield_value_colour_found, &UiConfiguration::directional_shield_value_colour},
        {"directionalShieldValueLowColor", &UiConfiguration::directional_shield_value_low_colour_found, &UiConfiguration::directional_shield_value_low_colour},
        {"directionalShieldValueCriticalColor", &UiConfiguration::directional_shield_value_critical_colour_found, &UiConfiguration::directional_shield_value_critical_colour},
    }};
    static constexpr std::array<ColourRead, 6> extra_colour_reads{{
        {"passiveWeaponIconColor", &UiConfiguration::passive_weapon_icon_colour_found, &UiConfiguration::passive_weapon_icon_colour},
        {"resupplyWeaponIconColor", &UiConfiguration::resupply_weapon_icon_colour_found, &UiConfiguration::resupply_weapon_icon_colour},
        {"specialEnergyIconColor", &UiConfiguration::special_energy_icon_colour_found, &UiConfiguration::special_energy_icon_colour},
        {"officerIconColor", &UiConfiguration::officer_icon_colour_found, &UiConfiguration::officer_icon_colour},
        {"experienceBarColor", &UiConfiguration::experience_bar_colour_found, &UiConfiguration::experience_bar_colour},
        {"experienceBarBackgroundColor", &UiConfiguration::experience_bar_background_colour_found, &UiConfiguration::experience_bar_background_colour},
    }};
    struct RefreshContext { void* db; UiConfiguration* loaded; } context{parameter_db, &loaded};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<RefreshContext*>(opaque);
        auto& result = *ctx.loaded;
        if (action == 0) {
            if (g_ui_configuration.loaded && g_ui_configuration.parameter_db != ctx.db)
                reset_directional_shield_sprite_runtime();
            return ctx.db != nullptr;
        }
        if (action == 1) {
            const auto& read = rectangle_reads[index];
            result.*(read.found) = read_ui_rectangle(
                ctx.db, read.key, &(result.*(read.rectangle)));
            return result.*(read.found);
        }
        if (action == 2) {
            const auto& read = rectangle_reads[index];
            result.*(read.found) = read_ui_rectangle(
                ctx.db, rectangle_fallbacks[index - 2], &(result.*(read.rectangle)));
        } else if (action == 3) {
            result.directional_value_rectangle_found[index] = read_ui_rectangle(
                ctx.db, kDirectionalShieldValueRectangleCommands[index],
                &result.directional_value_rectangles[index]);
        } else if (action == 4 || action == 7) {
            const auto& read = action == 4 ? colour_reads[index] : extra_colour_reads[index];
            result.*(read.found) = read_ui_colour(
                ctx.db, read.key, &(result.*(read.colour)));
        } else if (action == 5) {
            result.system_icon_colour_found[index] = read_ui_colour(
                ctx.db, kSystemIconColourCommands[index],
                &result.system_icon_colours[index]);
        } else if (action == 6) {
            result.weapon_icon_colour_found[index] = read_ui_colour(
                ctx.db, kWeaponIconColourCommands[index],
                &result.weapon_icon_colours[index]);
        }
        return 0;
    };
    a2fo_rs_stage9_craft_refresh_ui(step, &context,
        kDirectionalShieldValueRectangleCommands.size(),
        kSystemIconColourCommands.size());
    g_ui_configuration = loaded;
    load_panel_styles(parameter_db);

    const std::size_t system_icon_colour_count = static_cast<std::size_t>(
        std::count(loaded.system_icon_colour_found.begin(),
                   loaded.system_icon_colour_found.end(), true));
    char system_icon_colours[24]{};
    std::snprintf(system_icon_colours, sizeof(system_icon_colours),
                  "%u/5",
                  static_cast<unsigned>(system_icon_colour_count));
    const std::size_t weapon_icon_colour_count = static_cast<std::size_t>(
        std::count(loaded.weapon_icon_colour_found.begin(),
                   loaded.weapon_icon_colour_found.end(), true));
    char weapon_icon_colours[24]{};
    std::snprintf(weapon_icon_colours, sizeof(weapon_icon_colours),
                  "%u/5",
                  static_cast<unsigned>(weapon_icon_colour_count));
    const std::size_t directional_value_rectangle_count =
        static_cast<std::size_t>(std::count(
            loaded.directional_value_rectangle_found.begin(),
            loaded.directional_value_rectangle_found.end(), true));
    char message[768]{};
    std::snprintf(
        message, sizeof(message),
        "Selected UI: anchors=%s/%s, captain=%s, registry=%s, ship-name colour=%s, system-icon colours=%s, weapon-icon colours=%s, passive-weapon colour=%s, special-energy colour=%s, officer colour=%s, Photon=%s, Quantum=%s, Shuttle Craft=%s, directional shields=%s/%s, shield graphic=%s, shield value rectangles=%u/4, status bars=%s/%s",
        (loaded.single_name_rectangle_found ||
         loaded.single_class_rectangle_found) ? "single" : "no-single",
        (loaded.builder_name_rectangle_found ||
         loaded.builder_class_rectangle_found) ? "builder" : "no-builder",
        loaded.captain_rectangle_found ? "configured" : "absent",
        loaded.registry_rectangle_found ? "configured" : "absent",
        loaded.ship_name_colour_found ? "configured" : "native",
        system_icon_colours,
        weapon_icon_colours,
        loaded.passive_weapon_icon_colour_found
            ? "configured" : "neutral",
        loaded.special_energy_icon_colour_found ? "configured" : "native",
        loaded.officer_icon_colour_found ? "configured" : "native",
        loaded.photon_rectangle_found ? "configured" : "automatic",
        loaded.quantum_rectangle_found ? "configured" : "automatic",
        loaded.shuttle_craft_rectangle_found
            ? "configured" : "automatic",
        loaded.directional_forward_aft_rectangle_found
            ? "configured" : "automatic",
        loaded.directional_port_starboard_rectangle_found
            ? "configured" : "automatic",
        loaded.directional_graphic_rectangle_found
            ? "configured" : "automatic",
        static_cast<unsigned>(directional_value_rectangle_count),
        loaded.shield_bar_rectangle_found ? "shield" : "no-shield",
        loaded.experience_bar_rectangle_found ? "experience" : "no-experience");
    log_line(message);
}

bool configured_system_state_colour(
    a2fo::craft_identity::SystemIconState state,
    Colour* output) noexcept {
    if (!output) return false;
    const std::size_t state_index = static_cast<std::size_t>(state);
    if (state_index >= g_ui_configuration.system_icon_colours.size() ||
        !g_ui_configuration.system_icon_colour_found[state_index]) {
        return false;
    }
    *output = g_ui_configuration.system_icon_colours[state_index];
    return true;
}

bool configured_weapon_state_colour(
    a2fo::craft_identity::SystemIconState state,
    Colour* output) noexcept {
    if (!output) return false;
    const std::size_t state_index = static_cast<std::size_t>(state);
    if (state_index >= g_ui_configuration.weapon_icon_colours.size()) {
        return false;
    }
    const auto source = a2fo::craft_identity::weapon_icon_colour_source(
        g_ui_configuration.weapon_icon_colour_found[state_index],
        g_ui_configuration.system_icon_colour_found[state_index]);
    if (source ==
            a2fo::craft_identity::WeaponIconColourSource::weapon) {
        *output = g_ui_configuration.weapon_icon_colours[state_index];
        return true;
    }
    if (source ==
            a2fo::craft_identity::WeaponIconColourSource::system_fallback) {
        *output = g_ui_configuration.system_icon_colours[state_index];
        return true;
    }
    return false;
}

bool live_system_icon_state(
    void* craft, std::int32_t system_index,
    a2fo::craft_identity::SystemIconState* state) noexcept {
    if (!craft || !state) return false;
    if (system_index < 0 || system_index >= 5) return false;

    void* systems = read_at<void*>(craft, kCraftSystemsOffset, nullptr);
    if (!systems) return false;
    const auto* system = static_cast<const std::uint8_t*>(systems) +
        static_cast<std::size_t>(system_index) * kCraftSystemSize;

    return a2fo::craft_identity::classify_system_icon_state(
        read_at<std::uint8_t>(
            system, kCraftSystemOperationalOffset, 1) != 0,
        read_at<std::uint8_t>(
            system, kCraftSystemForcedDisabledOffset, 0) != 0,
        read_at<std::int32_t>(
            system, kCraftSystemMaximumHitpointsOffset, 0),
        read_at<double>(
            system, kCraftSystemCurrentHitpointsOffset, 0.0),
        read_at<float>(system, kCraftSystemDisableTimeOffset, 0.0f),
        state);
}

bool configured_system_colour(
    void* craft, std::int32_t system_index, Colour* output) noexcept {
    a2fo::craft_identity::SystemIconState state{};
    return live_system_icon_state(craft, system_index, &state) &&
        configured_system_state_colour(state, output);
}

bool configured_weapon_colour(
    void* craft, Colour* output) noexcept {
    a2fo::craft_identity::SystemIconState state{};
    return live_system_icon_state(craft, kWeaponsSystemIndex, &state) &&
        configured_weapon_state_colour(state, output);
}

bool configured_percentage_colour(void* component, Colour* output) noexcept {
    if (!component || !output ||
        !read_at<void*>(component, kSystemTextCraftOffset, nullptr)) {
        return false;
    }
    const float percentage = read_at<float>(
        component, kValuePercentageOffset,
        std::numeric_limits<float>::quiet_NaN());
    a2fo::craft_identity::SystemIconState state{};
    if (!a2fo::craft_identity::classify_system_icon_state(
            true, false, 100, static_cast<double>(percentage), 0.0f,
            &state)) {
        return false;
    }
    const std::size_t state_index = static_cast<std::size_t>(state);
    if (state_index >= g_ui_configuration.system_icon_colours.size() ||
        !g_ui_configuration.system_icon_colour_found[state_index]) {
        return false;
    }
    *output = g_ui_configuration.system_icon_colours[state_index];
    return true;
}

enum class ValueComponentKind {
    unrelated,
    system,
    crew,
    mouseover_status,
    special_energy,
    officer,
};

ValueComponentKind value_component_kind(const void* component) noexcept {
    const void* vtable = read_at<const void*>(component, 0, nullptr);
    if (vtable == at(g_armada, kSystemValueVtableRva)) {
        return ValueComponentKind::system;
    }
    if (vtable == at(g_armada, kCrewNumTextVtableRva)) {
        return ValueComponentKind::crew;
    }
    if (vtable == at(g_armada, kHullTextVtableRva) ||
        vtable == at(g_armada, kShieldTextVtableRva)) {
        return ValueComponentKind::mouseover_status;
    }
    if (vtable == at(g_armada, kEnergyTextVtableRva)) {
        return ValueComponentKind::special_energy;
    }
    if (vtable == at(g_armada, kOfficerTextAndSpriteVtableRva)) {
        return ValueComponentKind::officer;
    }
    return ValueComponentKind::unrelated;
}

bool configured_value_colour(
    void* component, Colour* output,
    ValueComponentKind* kind_output = nullptr) noexcept {
    struct Context {
        void* component;
        Colour* output;
        ValueComponentKind* kind_output;
        ValueComponentKind kind = ValueComponentKind::unrelated;
    } context{component, output, kind_output};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            ctx.kind = value_component_kind(ctx.component);
            if (ctx.kind_output) *ctx.kind_output = ctx.kind;
            return static_cast<std::uintptr_t>(ctx.kind);
        }
        if (action == 1) {
            return configured_system_colour(
                read_at<void*>(ctx.component, kSystemTextCraftOffset, nullptr),
                read_at<std::int32_t>(ctx.component, kSystemTextIndexOffset, -1),
                ctx.output);
        }
        if (action == 2) return configured_percentage_colour(
            ctx.component, ctx.output);
        if (action == 3) {
            if (g_ui_configuration.special_energy_icon_colour_found && ctx.output) {
                *ctx.output = g_ui_configuration.special_energy_icon_colour;
                return 1;
            }
        } else if (action == 4) {
            if (g_ui_configuration.officer_icon_colour_found && ctx.output) {
                *ctx.output = g_ui_configuration.officer_icon_colour;
                return 1;
            }
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_value_colour(step, &context);
}

void system_colour_set_colour_from_context(
    void* sprite, void* craft, std::int32_t system_index,
    const Colour* native_colour, LONG* report_count,
    const char* report_message) noexcept {
    struct Context {
        void* sprite;
        void* craft;
        std::int32_t system_index;
        const Colour* native_colour;
        LONG* report_count;
        const char* report_message;
        const Colour* applied_colour;
        Colour configured_colour{}, tinted{};
        bool applied = false;
    } context{sprite, craft, system_index, native_colour, report_count,
              report_message, native_colour, {}, {}, false};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            if (g_runtime_ready && ctx.craft) {
                void* parameter_db = gui_parameter_db();
                if (!g_ui_configuration.loaded ||
                    g_ui_configuration.parameter_db != parameter_db)
                    refresh_ui_configuration(parameter_db);
                ctx.applied = configured_system_colour(
                    ctx.craft, ctx.system_index, &ctx.configured_colour);
            }
        } else if (action == 1) {
            return ctx.applied && ctx.native_colour &&
                readable_range(ctx.native_colour, sizeof(*ctx.native_colour));
        } else if (action == 2) {
            Colour native{};
            std::memcpy(&native, ctx.native_colour, sizeof(native));
            const auto result = a2fo::craft_identity::tint_system_icon_colour(
                {{native.red, native.green, native.blue}},
                {{ctx.configured_colour.red, ctx.configured_colour.green,
                  ctx.configured_colour.blue}});
            ctx.tinted = Colour{result[0], result[1], result[2]};
            ctx.applied_colour = &ctx.tinted;
        } else if (action == 3) {
            a2fo_identity_call_thiscall_1(
                at(g_armada, kSpriteSetColourRva), ctx.sprite,
                reinterpret_cast<std::uintptr_t>(ctx.applied_colour));
        } else if (action == 4) {
            if (ctx.applied && ctx.report_count && ctx.report_message &&
                InterlockedCompareExchange(ctx.report_count, 1, 0) == 0)
                log_line(ctx.report_message);
        }
        return 0;
    };
    a2fo_rs_stage9_craft_colour_apply(step, &context);
}

void system_icon_set_colour_from_context(
    void* sprite, void* icon, const Colour* native_colour) noexcept {
    system_colour_set_colour_from_context(
        sprite,
        read_at<void*>(icon, kSystemIconCraftOffset, nullptr),
        read_at<std::int32_t>(icon, kSystemIconIndexOffset, -1),
        native_colour, &g_system_icon_colour_report_count,
        "Configured live-health colours applied to the native selected-panel system icons");
}

void weapon_icon_set_colour_from_context(
    void* sprite, void* icon, const Colour* native_colour) noexcept {
    struct Context {
        void* sprite;
        void* icon;
        const Colour* native_colour;
        const Colour* applied_colour;
        Colour configured_colour{}, tinted{};
        bool applied = false, passive_colour_configured = false;
        void* craft = nullptr;
        a2fo::craft_identity::WeaponIconPresentation presentation =
            a2fo::craft_identity::WeaponIconPresentation::live_status;
    } context{sprite, icon, native_colour, native_colour, {}, {}, false, false,
              nullptr, a2fo::craft_identity::WeaponIconPresentation::live_status};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        using Presentation = a2fo::craft_identity::WeaponIconPresentation;
        if (action == 0) {
            if (!g_runtime_ready || !ctx.icon) return 0;
            void* parameter_db = gui_parameter_db();
            if (!g_ui_configuration.loaded ||
                g_ui_configuration.parameter_db != parameter_db)
                refresh_ui_configuration(parameter_db);
            if (g_rendering_weapon_icon == ctx.icon) {
                ctx.craft = g_rendering_weapon_craft;
                ctx.presentation = g_rendering_weapon_presentation;
            } else ctx.presentation = weapon_icon_presentation(ctx.icon, &ctx.craft);
            switch (ctx.presentation) {
                case Presentation::passive_neutral: return 1;
                case Presentation::resupply_ready: return 2;
                case Presentation::disabled: return 3;
                case Presentation::hidden: return 4;
                case Presentation::live_status: return 5;
            }
            return 0;
        }
        if (action == 1) {
            ctx.passive_colour_configured = g_ui_configuration.passive_weapon_icon_colour_found;
            ctx.configured_colour = ctx.passive_colour_configured
                ? g_ui_configuration.passive_weapon_icon_colour : Colour{1.0f, 1.0f, 1.0f};
            ctx.applied = true;
        } else if (action == 2) {
            ctx.configured_colour = g_ui_configuration.resupply_weapon_icon_colour_found
                ? g_ui_configuration.resupply_weapon_icon_colour
                : Colour{kDefaultResupplyWeaponIconColour[0],
                         kDefaultResupplyWeaponIconColour[1],
                         kDefaultResupplyWeaponIconColour[2]};
            ctx.applied = true;
        } else if (action == 3) {
            ctx.applied = configured_weapon_state_colour(
                a2fo::craft_identity::SystemIconState::disabled, &ctx.configured_colour);
        } else if (action == 5) {
            ctx.applied = configured_weapon_colour(ctx.craft, &ctx.configured_colour);
        } else if (action == 6) {
            return ctx.applied && ctx.native_colour &&
                readable_range(ctx.native_colour, sizeof(*ctx.native_colour));
        } else if (action == 7) {
            Colour native{};
            std::memcpy(&native, ctx.native_colour, sizeof(native));
            const auto result = a2fo::craft_identity::tint_system_icon_colour(
                {{native.red, native.green, native.blue}},
                {{ctx.configured_colour.red, ctx.configured_colour.green,
                  ctx.configured_colour.blue}});
            ctx.tinted = Colour{result[0], result[1], result[2]};
            ctx.applied_colour = &ctx.tinted;
        } else if (action == 8) {
            a2fo_identity_fo_sprite_set_colour(
                at(g_fleet_ops, kFoSpriteSetColourRva), ctx.sprite, ctx.applied_colour);
        } else if (action == 9) {
            if (ctx.applied && InterlockedCompareExchange(
                    &g_weapon_icon_colour_report_count, 1, 0) == 0)
                log_line("WeaponXiconpos colour policy applied to native icon layers");
        } else if (action == 10) {
            if (InterlockedCompareExchange(&g_passive_weapon_icon_report_count, 1, 0) == 0)
                log_line(ctx.passive_colour_configured
                    ? "passiveWeaponIconColor applied to UtilityWeapon icons"
                    : "Passive UtilityWeapon icon rendered in neutral white/grey");
        } else if (action == 11) {
            if (InterlockedCompareExchange(&g_disabled_weapon_icon_report_count, 1, 0) == 0)
                log_line("Research-locked weapon icon rendered disabled");
        }
        return 0;
    };
    a2fo_rs_stage9_craft_weapon_colour(step, &context);
}

void __attribute__((fastcall)) ship_system_icon_render_policy_hook(
    void* icon, void*) noexcept {
    struct Context {
        void* icon;
        void* craft = nullptr;
        a2fo::craft_identity::WeaponIconPresentation presentation =
            a2fo::craft_identity::WeaponIconPresentation::live_status;
        void* previous_icon = nullptr;
        void* previous_craft = nullptr;
        a2fo::craft_identity::WeaponIconPresentation previous_presentation =
            a2fo::craft_identity::WeaponIconPresentation::live_status;
    } context{icon};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) return g_ship_system_icon_render_original != nullptr;
        if (action == 1) {
            ctx.presentation = g_runtime_ready && ctx.icon
                ? weapon_icon_presentation(ctx.icon, &ctx.craft)
                : a2fo::craft_identity::WeaponIconPresentation::live_status;
        } else if (action == 2) {
            return ctx.presentation ==
                a2fo::craft_identity::WeaponIconPresentation::hidden;
        } else if (action == 3) {
            if (InterlockedCompareExchange(
                    &g_hidden_weapon_icon_report_count, 1, 0) == 0)
                log_line("WeaponXiconpos icon hidden by buttonHideUnavailable");
        } else if (action == 4) {
            ctx.previous_icon = g_rendering_weapon_icon;
            ctx.previous_craft = g_rendering_weapon_craft;
            ctx.previous_presentation = g_rendering_weapon_presentation;
        } else if (action == 5) {
            g_rendering_weapon_icon = ctx.icon;
            g_rendering_weapon_craft = ctx.craft;
            g_rendering_weapon_presentation = ctx.presentation;
        } else if (action == 6) {
            a2fo_identity_call_thiscall_0(
                g_ship_system_icon_render_original, ctx.icon);
        } else if (action == 7) {
            g_rendering_weapon_icon = ctx.previous_icon;
            g_rendering_weapon_craft = ctx.previous_craft;
            g_rendering_weapon_presentation = ctx.previous_presentation;
        }
        return 0;
    };
    a2fo_rs_stage9_craft_ship_icon_render(step, &context);
}

void system_text_set_colour_from_context(
    void* sprite, void* text, const Colour* native_colour) noexcept {
    struct Context {
        void* sprite;
        void* text;
        const Colour* native_colour;
        const Colour* applied_colour;
        Colour configured_colour{}, tinted{};
        ValueComponentKind kind = ValueComponentKind::unrelated;
        bool applied = false;
    } context{sprite, text, native_colour, native_colour, {}, {},
              ValueComponentKind::unrelated, false};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            if (g_runtime_ready && ctx.text) {
                void* parameter_db = gui_parameter_db();
                if (!g_ui_configuration.loaded ||
                    g_ui_configuration.parameter_db != parameter_db)
                    refresh_ui_configuration(parameter_db);
                ctx.applied = configured_value_colour(
                    ctx.text, &ctx.configured_colour, &ctx.kind);
            }
        } else if (action == 1) {
            return ctx.applied && ctx.native_colour &&
                readable_range(ctx.native_colour, sizeof(*ctx.native_colour));
        } else if (action == 2) {
            Colour native{};
            std::memcpy(&native, ctx.native_colour, sizeof(native));
            const auto result = a2fo::craft_identity::tint_system_icon_colour(
                {{native.red, native.green, native.blue}},
                {{ctx.configured_colour.red, ctx.configured_colour.green,
                  ctx.configured_colour.blue}});
            ctx.tinted = Colour{result[0], result[1], result[2]};
            ctx.applied_colour = &ctx.tinted;
        } else if (action == 3) {
            a2fo_identity_call_thiscall_1(
                at(g_armada, kSpriteSetColourRva), ctx.sprite,
                reinterpret_cast<std::uintptr_t>(ctx.applied_colour));
        } else if (action == 4) {
            LONG* report_count = &g_system_value_icon_colour_report_count;
            const char* report_message =
                "Configured live-health colours applied to the native selected-panel system value icons";
            if (ctx.kind == ValueComponentKind::crew) {
                report_count = &g_crew_icon_colour_report_count;
                report_message = "Configured live-health colour applied to the native crew icon";
            } else if (ctx.kind == ValueComponentKind::mouseover_status) {
                report_count = &g_mouseover_status_icon_colour_report_count;
                report_message = "Configured live-health colours applied to native mouse-over hull/shield icons";
            } else if (ctx.kind == ValueComponentKind::special_energy) {
                report_count = &g_special_energy_icon_colour_report_count;
                report_message = "specialEnergyIconColor applied to the native special-energy icon";
            } else if (ctx.kind == ValueComponentKind::officer) {
                report_count = &g_officer_icon_colour_report_count;
                report_message = "officerIconColor applied to the native officer icon";
            }
            if (ctx.applied && InterlockedCompareExchange(report_count, 1, 0) == 0)
                log_line(report_message);
        }
        return 0;
    };
    a2fo_rs_stage9_craft_colour_apply(step, &context);
}

void value_text_colour_from_context(
    void* component, void* text_record) noexcept {
    struct Context {
        void* component;
        void* text_record;
        Colour configured_colour{};
        ValueComponentKind kind = ValueComponentKind::unrelated;
    } context{component, text_record};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) return g_runtime_ready && ctx.component &&
            writable_range(ctx.text_record, sizeof(Colour));
        if (action == 1) {
            void* parameter_db = gui_parameter_db();
            if (!g_ui_configuration.loaded ||
                g_ui_configuration.parameter_db != parameter_db)
                refresh_ui_configuration(parameter_db);
        } else if (action == 2) {
            return configured_value_colour(
                ctx.component, &ctx.configured_colour, &ctx.kind);
        } else if (action == 3) {
            std::memcpy(ctx.text_record, &ctx.configured_colour,
                sizeof(ctx.configured_colour));
        } else if (action == 4) {
            LONG* report_count = &g_system_text_colour_report_count;
            const char* report_message =
                "Configured live-health colours applied to the native selected-panel system value text";
            if (ctx.kind == ValueComponentKind::crew) {
                report_count = &g_crew_text_colour_report_count;
                report_message = "Configured live-health colour applied to the native crew value text";
            } else if (ctx.kind == ValueComponentKind::mouseover_status) {
                report_count = &g_mouseover_status_text_colour_report_count;
                report_message = "Configured live-health colours applied to native mouse-over hull/shield value text";
            } else if (ctx.kind == ValueComponentKind::special_energy) {
                report_count = &g_special_energy_text_colour_report_count;
                report_message = "specialEnergyIconColor applied to the native special-energy value text";
            } else if (ctx.kind == ValueComponentKind::officer) {
                report_count = &g_officer_text_colour_report_count;
                report_message = "officerIconColor applied to the native officer value text";
            }
            if (InterlockedCompareExchange(report_count, 1, 0) == 0)
                log_line(report_message);
        }
        return 0;
    };
    a2fo_rs_stage9_craft_value_text_colour(step, &context);
}

Colour text_component_colour(const void* text_component) noexcept {
    Colour colour = read_at<Colour>(
        text_component, kTextComponentColourOffset, Colour{});
    if (!std::isfinite(colour.red) || !std::isfinite(colour.green) ||
        !std::isfinite(colour.blue)) {
        return Colour{};
    }
    return colour;
}

struct TextColourOverride {
    void* address = nullptr;
    Colour saved{};
};

TextColourOverride override_text_component_colour(
    void* info_display, std::size_t component_offset,
    const Colour& colour) noexcept {
    TextColourOverride result{};
    void* component = read_at<void*>(
        info_display, component_offset, nullptr);
    if (!component) return result;

    void* address = static_cast<std::uint8_t*>(component) +
        kTextComponentColourOffset;
    if (!readable_range(address, sizeof(Colour)) ||
        !writable_range(address, sizeof(Colour))) {
        return result;
    }
    result.address = address;
    std::memcpy(&result.saved, address, sizeof(result.saved));
    std::memcpy(address, &colour, sizeof(colour));
    return result;
}

void restore_text_component_colour(
    const TextColourOverride& override) noexcept {
    if (!override.address ||
        !writable_range(override.address, sizeof(override.saved))) {
        return;
    }
    std::memcpy(override.address, &override.saved, sizeof(override.saved));
}

bool draw_identity_text(const std::string& value,
                        const NativeRectangle& render_rectangle,
                        const Colour& colour,
                        void* text_component) noexcept {
    if (value.empty() || !usable_native_rectangle(render_rectangle) ||
        !text_component) {
        return false;
    }

    // Reuse the live native captain component's display/font state. Calling
    // the same rectangle-aware path as GUIText installs the requested
    // rectangle and preserves Fleet Operations' font, scaling and clipping
    // behaviour.
    void* display_interface = read_at<void*>(
        text_component, 0x04, nullptr);
    if (!display_interface) return false;
    void* display_override = nullptr;
    void* display_slot = read_at<void*>(text_component, 0x28, nullptr);
    if (display_slot) {
        display_override = read_at<void*>(display_slot, 0, nullptr);
    }
    const std::int32_t text_flags = read_at<std::int32_t>(
        text_component, 0x68, 9);
    const std::uint8_t constrain_to_rectangle = read_at<std::uint8_t>(
        text_component, 0x6c, 0);
    void* font_state = static_cast<std::uint8_t*>(text_component) + 0x7c;
    if (!readable_range(font_state, 12)) return false;

    a2fo_identity_call_thiscall_7(
        at(g_armada, kDisplayInterfaceDrawTextInRectangleRva),
        display_interface,
        reinterpret_cast<std::uintptr_t>(value.c_str()),
        reinterpret_cast<std::uintptr_t>(&render_rectangle),
        static_cast<std::uintptr_t>(text_flags),
        reinterpret_cast<std::uintptr_t>(&colour),
        reinterpret_cast<std::uintptr_t>(display_override),
        static_cast<std::uintptr_t>(constrain_to_rectangle),
        reinterpret_cast<std::uintptr_t>(font_state));
    return true;
}

int whole_ammunition_amount(float value) noexcept {
    if (!std::isfinite(value) || value <= 0.0f) return 0;
    const float maximum = static_cast<float>(
        std::numeric_limits<int>::max());
    return static_cast<int>(std::floor(std::min(value, maximum) + 0.0001f));
}

const char* localized_directional_shield_text(
    const char* key, const char* fallback) noexcept;
float clamped_ratio(float current, float maximum) noexcept;
bool draw_ammunition_icon(
    std::size_t store_index, const std::string& sprite_name,
    const RawRectangle& source_rectangle,
    const NativeRectangle& rectangle, const Colour& colour,
    std::int32_t display_origin_x,
    std::int32_t display_origin_y) noexcept;
bool draw_ammunition_value_bar(
    const NativeRectangle& rectangle, float ratio, const Colour& colour,
    std::int32_t display_origin_x,
    std::int32_t display_origin_y,
    const Colour* background_colour = nullptr,
    PanelElement element = PanelElement::ExperienceBar) noexcept;

const AmmunitionUiPolicy* ammunition_ui_policy(void* craft) noexcept {
    void* object_class = read_at<void*>(craft, kObjectClassOffset, nullptr);
    const auto found = g_ammunition_ui_policies.find(object_class);
    return found == g_ammunition_ui_policies.end() ? nullptr : &found->second;
}

NativeRectangle union_rectangle(
    const NativeRectangle& first, const NativeRectangle& second) noexcept {
    if (!usable_native_rectangle(first)) return second;
    if (!usable_native_rectangle(second)) return first;
    return NativeRectangle{
        std::min(first.left, second.left),
        std::min(first.top, second.top),
        std::max(first.right, second.right),
        std::max(first.bottom, second.bottom)};
}

void draw_ammunition_rows(
    void* info_display, void* craft, void* text_component,
    const NativeRectangle& live_anchor,
    const Colour&) noexcept {
    struct Row {
        const AmmunitionPresentation* presentation = nullptr;
        const char* label = nullptr;
        int displayed_current = 0, displayed_maximum = 0;
        float ratio = 0.0f, seconds = -1.0f;
        Colour status_colour{};
        AmmunitionValueDisplayMode value_mode = AmmunitionValueDisplayMode::percent;
        char value_text[128]{}, text[640]{};
        NativeRectangle hit_rectangle{}, amount_rectangle{}, bar_rectangle{};
        bool icon_drawn = false, text_drawn = false, bar_drawn = false;
        bool requested_icon = false, separate = false;
    };
    struct Context {
        void* info_display;
        void* craft;
        void* text_component;
        const NativeRectangle* live_anchor;
        std::array<float, 3> maximum{}, current{};
        std::array<NativeRectangle, 3> configured_rectangles{}, rectangles{};
        std::array<EnergyAmountGetter, 3> reload_getters{};
        const AmmunitionUiPolicy* policy = nullptr;
        std::int32_t origin_x = 0, origin_y = 0;
        AmmunitionTooltipRuntime updated{};
        std::array<bool, 3> drawn{};
        Row row{};
    } context{info_display, craft, text_component, &live_anchor};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        static constexpr std::array<const char*, 3> default_labels{{
            "Photon Torpedoes", "Quantum Torpedoes", "Shuttle Craft"}};
        if (action == 0) return ctx.info_display && ctx.craft &&
            ctx.text_component && resolve_energy_amount_api();
        if (action == 1) {
            ctx.maximum = {{g_get_maximum_photon_torpedoes(ctx.craft),
                g_get_maximum_quantum_torpedoes(ctx.craft),
                g_get_maximum_shuttle_craft
                    ? g_get_maximum_shuttle_craft(ctx.craft) : 0.0f}};
        } else if (action == 2) {
            return std::isfinite(ctx.maximum[index]) && ctx.maximum[index] > 0.0f;
        } else if (action == 3) {
            ctx.configured_rectangles = {{*ctx.live_anchor, *ctx.live_anchor,
                *ctx.live_anchor}};
            ctx.configured_rectangles[0].top += 16;
            ctx.configured_rectangles[0].bottom += 16;
            ctx.configured_rectangles[1].top += 40;
            ctx.configured_rectangles[1].bottom += 40;
            ctx.configured_rectangles[2].top += 64;
            ctx.configured_rectangles[2].bottom += 64;
        } else if (action == 4) {
            return g_ui_configuration.captain_rectangle_found &&
                (index == 0 ? g_ui_configuration.photon_rectangle_found :
                 index == 1 ? g_ui_configuration.quantum_rectangle_found :
                              g_ui_configuration.shuttle_craft_rectangle_found);
        } else if (action == 5) {
            const RawRectangle& configured = index == 0
                ? g_ui_configuration.photon_rectangle
                : index == 1 ? g_ui_configuration.quantum_rectangle
                             : g_ui_configuration.shuttle_craft_rectangle;
            ctx.configured_rectangles[index] = translated_rectangle(
                *ctx.live_anchor, g_ui_configuration.captain_rectangle,
                configured);
        } else if (action == 6) {
            ctx.rectangles[index] = panel_rectangle(
                kAmmoRows[index], ctx.configured_rectangles[index]);
        } else if (action == 7) {
            ctx.current = {{g_get_photon_torpedoes(ctx.craft),
                g_get_quantum_torpedoes(ctx.craft),
                g_get_shuttle_craft ? g_get_shuttle_craft(ctx.craft) : 0.0f}};
            ctx.reload_getters = {{g_get_photon_torpedo_reload_seconds,
                g_get_quantum_torpedo_reload_seconds,
                g_get_shuttle_craft_reload_seconds}};
            ctx.policy = ammunition_ui_policy(ctx.craft);
        } else if (action == 8) {
            void* display_interface = read_at<void*>(
                ctx.text_component, 0x04, nullptr);
            if (!display_interface) return 0;
            ctx.origin_x = read_at<std::int32_t>(
                display_interface, 0x04, 0);
            ctx.origin_y = read_at<std::int32_t>(
                display_interface, 0x08, 0);
            return 1;
        } else if (action == 9) {
            ctx.updated.info_display = ctx.info_display;
            ctx.updated.craft = ctx.craft;
        } else if (action == 10) {
            return std::isfinite(ctx.maximum[index]) &&
                ctx.maximum[index] > 0.0f && std::isfinite(ctx.current[index]);
        } else if (action == 11) {
            ctx.row = {};
            auto& row = ctx.row;
            row.presentation = ctx.policy ? &ctx.policy->stores[index] : nullptr;
            const char* configured_label = row.presentation &&
                !row.presentation->label.empty()
                ? row.presentation->label.c_str() : default_labels[index];
            row.label = localized_directional_shield_text(
                configured_label, configured_label);
            row.displayed_current = whole_ammunition_amount(ctx.current[index]);
            row.displayed_maximum = whole_ammunition_amount(ctx.maximum[index]);
            row.ratio = clamped_ratio(ctx.current[index], ctx.maximum[index]);
            Colour healthy{0.0f, 1.0f, 0.0f};
            Colour low{1.0f, 1.0f, 0.0f};
            Colour critical{1.0f, 0.0f, 0.0f};
            if (index == 0) {
                if (g_ui_configuration.photon_colour_found)
                    healthy = g_ui_configuration.photon_colour;
                if (g_ui_configuration.photon_low_colour_found)
                    low = g_ui_configuration.photon_low_colour;
                if (g_ui_configuration.photon_critical_colour_found)
                    critical = g_ui_configuration.photon_critical_colour;
            } else if (index == 1) {
                if (g_ui_configuration.quantum_colour_found)
                    healthy = g_ui_configuration.quantum_colour;
                if (g_ui_configuration.quantum_low_colour_found)
                    low = g_ui_configuration.quantum_low_colour;
                if (g_ui_configuration.quantum_critical_colour_found)
                    critical = g_ui_configuration.quantum_critical_colour;
            } else {
                if (g_ui_configuration.shuttle_craft_colour_found)
                    healthy = g_ui_configuration.shuttle_craft_colour;
                if (g_ui_configuration.shuttle_craft_low_colour_found)
                    low = g_ui_configuration.shuttle_craft_low_colour;
                if (g_ui_configuration.shuttle_craft_critical_colour_found)
                    critical = g_ui_configuration.shuttle_craft_critical_colour;
            }
            row.status_colour = panel_colour(kAmmoRows[index],
                row.ratio <= 0.25f ? critical : row.ratio <= 0.50f ? low : healthy,
                row.ratio);
            row.value_mode = row.presentation
                ? row.presentation->value_display_mode
                : AmmunitionValueDisplayMode::percent;
        } else if (action == 12) {
            return static_cast<std::uintptr_t>(ctx.row.value_mode);
        } else if (action == 13) {
            std::snprintf(ctx.row.value_text, sizeof(ctx.row.value_text),
                "%d/%d", ctx.row.displayed_current, ctx.row.displayed_maximum);
        } else if (action == 14) {
            return ctx.row.ratio >= 0.9999f;
        } else if (action == 15) {
            const char* ready = localized_directional_shield_text(
                "GUI_SD_SPE_READY", "Ready");
            std::snprintf(ctx.row.value_text, sizeof(ctx.row.value_text),
                "%s", ready);
        } else if (action == 16) {
            EnergyAmountGetter reload_getter = ctx.reload_getters[index];
            ctx.row.seconds = reload_getter
                ? reload_getter(ctx.craft) : -1.0f;
        } else if (action == 17) {
            return std::isfinite(ctx.row.seconds) && ctx.row.seconds >= 0.0f;
        } else if (action == 18) {
            const int displayed_seconds = std::max(
                1, static_cast<int>(std::ceil(ctx.row.seconds)));
            std::snprintf(ctx.row.value_text, sizeof(ctx.row.value_text),
                "%d s", displayed_seconds);
        } else if (action == 19) {
            const char* waiting = localized_directional_shield_text(
                "GUI_SD_AMMO_WAITING", "Resupply");
            std::snprintf(ctx.row.value_text, sizeof(ctx.row.value_text),
                "%s", waiting);
        } else if (action == 20) {
            const int percent = static_cast<int>(std::floor(
                ctx.row.ratio * 100.0f + 0.5f));
            std::snprintf(ctx.row.value_text, sizeof(ctx.row.value_text),
                "%d%%", percent);
        } else if (action == 21) {
            ctx.row.amount_rectangle = ctx.rectangles[index];
            ctx.row.requested_icon = ctx.row.presentation &&
                ctx.row.presentation->display_mode == AmmunitionDisplayMode::icon;
            return ctx.row.requested_icon && !ctx.row.presentation->icon.empty() &&
                ctx.row.presentation->icon_position_found;
        } else if (action == 22) {
            auto& row = ctx.row;
            const RawRectangle& configured = row.presentation->icon_position;
            const std::int32_t row_height =
                ctx.rectangles[index].bottom - ctx.rectangles[index].top;
            const NativeRectangle icon_rectangle = panel_rectangle(
                kAmmoIcons[index], NativeRectangle{
                    ctx.rectangles[index].left,
                    ctx.rectangles[index].top +
                        (row_height - configured.height) / 2,
                    ctx.rectangles[index].left + configured.width,
                    ctx.rectangles[index].top +
                        (row_height - configured.height) / 2 + configured.height});
            row.icon_drawn = draw_ammunition_icon(index,
                row.presentation->icon, configured, icon_rectangle,
                panel_colour(kAmmoIcons[index], row.status_colour, row.ratio),
                ctx.origin_x, ctx.origin_y);
            if (row.icon_drawn) {
                row.hit_rectangle = union_rectangle(
                    row.hit_rectangle, icon_rectangle);
                if (!panel_style(kAmmoIcons[index]).area_found)
                    row.amount_rectangle.left = std::max(
                        row.amount_rectangle.left, icon_rectangle.right + 4);
            }
        } else if (action == 23) {
            ctx.row.bar_rectangle = ctx.row.amount_rectangle;
        } else if (action == 24) {
            return !ctx.row.requested_icon &&
                usable_native_rectangle(ctx.row.amount_rectangle);
        } else if (action == 25) {
            auto& row = ctx.row;
            const std::int32_t width =
                row.amount_rectangle.right - row.amount_rectangle.left;
            NativeRectangle label_rectangle = row.amount_rectangle;
            label_rectangle.right = row.amount_rectangle.left +
                std::max<std::int32_t>(1, width * 3 / 5);
            row.bar_rectangle.left = std::min(
                row.amount_rectangle.right, label_rectangle.right + 4);
            char label_text[512]{};
            std::snprintf(label_text, sizeof(label_text), "%s:", row.label);
            row.text_drawn = draw_identity_text(label_text,
                panel_rectangle(kAmmoLabels[index], label_rectangle),
                panel_colour(kAmmoLabels[index], row.status_colour, row.ratio),
                ctx.text_component);
            if (row.text_drawn) row.hit_rectangle = union_rectangle(
                row.hit_rectangle,
                panel_rectangle(kAmmoLabels[index], label_rectangle));
        } else if (action == 26) {
            return usable_native_rectangle(ctx.row.bar_rectangle) ||
                panel_style(kAmmoBars[index]).area_found;
        } else if (action == 27) {
            auto& row = ctx.row;
            const std::int32_t row_height =
                row.bar_rectangle.bottom - row.bar_rectangle.top;
            const std::int32_t bar_height = std::max<std::int32_t>(
                1, std::min<std::int32_t>(8, row_height));
            row.bar_rectangle.top += (row_height - bar_height) / 2;
            row.bar_rectangle.bottom = row.bar_rectangle.top + bar_height;
            row.bar_rectangle = panel_rectangle(kAmmoBars[index], row.bar_rectangle);
            row.bar_drawn = draw_ammunition_value_bar(
                row.bar_rectangle, row.ratio, row.status_colour,
                ctx.origin_x, ctx.origin_y, nullptr, kAmmoBars[index]);
            if (row.bar_drawn) row.hit_rectangle = union_rectangle(
                row.hit_rectangle, row.bar_rectangle);
        } else if (action == 28) {
            ctx.row.separate = panel_style(kAmmoLabels[index]).area_found ||
                panel_style(kAmmoValues[index]).area_found ||
                panel_style(kAmmoLabels[index]).colour_found ||
                panel_style(kAmmoValues[index]).colour_found ||
                panel_style(kAmmoLabels[index]).low_found ||
                panel_style(kAmmoLabels[index]).critical_found ||
                panel_style(kAmmoValues[index]).low_found ||
                panel_style(kAmmoValues[index]).critical_found;
        } else if (action == 29) {
            return ctx.row.separate && !ctx.row.requested_icon;
        } else if (action == 31) {
            auto& row = ctx.row;
            NativeRectangle label_rectangle = row.amount_rectangle;
            label_rectangle.right = label_rectangle.left +
                (label_rectangle.right - label_rectangle.left) * 3 / 5;
            row.amount_rectangle.left = label_rectangle.right + 4;
            label_rectangle = panel_rectangle(kAmmoLabels[index], label_rectangle);
            row.text_drawn = draw_identity_text(std::string(row.label) + ":",
                label_rectangle,
                panel_colour(kAmmoLabels[index], row.status_colour, row.ratio),
                ctx.text_component);
            if (row.text_drawn) row.hit_rectangle = union_rectangle(
                row.hit_rectangle, label_rectangle);
        } else if (action == 32) {
            auto& row = ctx.row;
            if (row.requested_icon || row.separate)
                std::snprintf(row.text, sizeof(row.text), "%s", row.value_text);
            else
                std::snprintf(row.text, sizeof(row.text), "%s: %s",
                    row.label, row.value_text);
            row.amount_rectangle = panel_rectangle(
                kAmmoValues[index], row.amount_rectangle);
        } else if (action == 33) {
            auto& row = ctx.row;
            const bool value_drawn = draw_identity_text(row.text,
                row.amount_rectangle,
                panel_colour(kAmmoValues[index], row.status_colour, row.ratio),
                ctx.text_component);
            if (value_drawn) row.hit_rectangle = union_rectangle(
                row.hit_rectangle, row.amount_rectangle);
            row.text_drawn = row.text_drawn || value_drawn;
        } else if (action == 34) {
            const auto& row = ctx.row;
            ctx.drawn[index] = row.icon_drawn || row.text_drawn || row.bar_drawn;
            return ctx.drawn[index];
        } else if (action == 37) {
            const auto& row = ctx.row;
            ctx.updated.visible[index] = true;
            ctx.updated.current[index] = ctx.current[index];
            ctx.updated.maximum[index] = ctx.maximum[index];
            ctx.updated.hit_rectangles[index] = FloatRectangle{
                static_cast<float>(row.hit_rectangle.left + ctx.origin_x),
                static_cast<float>(row.hit_rectangle.top + ctx.origin_y),
                static_cast<float>(row.hit_rectangle.right - row.hit_rectangle.left),
                static_cast<float>(row.hit_rectangle.bottom - row.hit_rectangle.top)};
            ctx.updated.active = true;
        } else if (action == 35) {
            if (ctx.updated.active) g_ammunition_tooltip = ctx.updated;
        } else if (action == 36) {
            if (a2fo_rs_stage10_identity_any(
                    ctx.drawn.size(),
                    +[](void* opaque, std::uint32_t,
                        std::size_t item) noexcept -> std::uint32_t {
                        return (*static_cast<decltype(ctx.drawn)*>(opaque))[item];
                    }, &ctx.drawn) &&
                InterlockedCompareExchange(
                    &g_ammunition_draw_report_count, 1, 0) == 0) {
                char message[384]{};
                std::snprintf(message, sizeof(message),
                    "First ammunition UI draw submitted: Photon=%s at %ld,%ld; "
                    "Quantum=%s at %ld,%ld; Shuttle Craft=%s at %ld,%ld",
                    ctx.drawn[0] ? "yes" : "no",
                    static_cast<long>(ctx.configured_rectangles[0].left),
                    static_cast<long>(ctx.configured_rectangles[0].top),
                    ctx.drawn[1] ? "yes" : "no",
                    static_cast<long>(ctx.configured_rectangles[1].left),
                    static_cast<long>(ctx.configured_rectangles[1].top),
                    ctx.drawn[2] ? "yes" : "no",
                    static_cast<long>(ctx.configured_rectangles[2].left),
                    static_cast<long>(ctx.configured_rectangles[2].top));
                log_line(message);
            }
        }
        return 0;
    };
    a2fo_rs_stage9_craft_draw_ammo_rows(step, &context, context.drawn.size());
}

bool read_experience_progress(
    void* craft, float* current, float* next_rank) noexcept {
    struct Context {
        void* craft;
        float* current;
        float* next_rank;
        double pooled = 0.0, required = 0.0;
        float live_current = 0.0f, live_next = 0.0f;
    } context{craft, current, next_rank};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            if (ctx.current) *ctx.current = 0.0f;
            if (ctx.next_rank) *ctx.next_rank = 0.0f;
        } else if (action == 1) {
            return ctx.craft && ctx.current && ctx.next_rank;
        } else if (action == 2) {
            // Optional provider uses the selected craft's owner, including AI selections.
            using CollectiveProgress = bool (__cdecl*)(unsigned,unsigned*,double*,double*);
            HMODULE collective = a2fo::module_provider(
                "A2FOCollectiveXP.dll", {"A2FOCollectiveXP_GetProgress"});
            auto progress = collective ? reinterpret_cast<CollectiveProgress>(
                reinterpret_cast<void*>(GetProcAddress(
                    collective,"A2FOCollectiveXP_GetProgress"))) : nullptr;
            return progress && progress(read_at<unsigned>(ctx.craft,0xec,~0u),
                nullptr,&ctx.pooled,&ctx.required);
        } else if (action == 3) {
            return std::isfinite(ctx.pooled) && std::isfinite(ctx.required) &&
                ctx.required > 0.0 &&
                ctx.required <= std::numeric_limits<float>::max();
        } else if (action == 4) {
            *ctx.current = static_cast<float>(std::min(ctx.pooled,ctx.required));
            *ctx.next_rank = static_cast<float>(ctx.required);
        } else if (action == 5) {
            void* enhancement = read_at<void*>(
                ctx.craft, kCraftEnhancementOffset, nullptr);
            void* enhancement_class = read_at<void*>(
                enhancement, kEnhancementClassOffset, nullptr);
            ctx.live_current = read_at<float>(
                enhancement, kEnhancementCurrentXpOffset,
                std::numeric_limits<float>::quiet_NaN());
            ctx.live_next = read_at<float>(
                enhancement_class, kEnhancementNextRankXpOffset,
                std::numeric_limits<float>::quiet_NaN());
        } else if (action == 6) {
            return std::isfinite(ctx.live_current) && ctx.live_current >= 0.0f &&
                std::isfinite(ctx.live_next) && ctx.live_next > 0.0f;
        } else if (action == 7) {
            *ctx.current = ctx.live_current;
            *ctx.next_rank = ctx.live_next;
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_experience_progress(step, &context);
}

void draw_selected_status_bars(
    void* info_display, void* craft, void* text_component,
    const NativeRectangle& live_anchor) noexcept {
    struct Context {
        void* info_display;
        void* craft;
        void* text_component;
        const NativeRectangle* live_anchor;
        std::int32_t origin_x = 0, origin_y = 0;
        SelectedStatusTooltipRuntime updated{};
        NativeRectangle shield_rectangle{}, experience_rectangle{};
        float current_shields = 0.0f, maximum_shields = 0.0f;
        float current_xp = 0.0f, next_rank_xp = 0.0f;
        bool builder_shield = false;
        bool experience_drawn = false;
    } context{info_display, craft, text_component, &live_anchor};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) return ctx.info_display && ctx.craft && ctx.text_component;
        if (action == 1) {
            void* display_interface = read_at<void*>(ctx.text_component, 0x04, nullptr);
            if (!display_interface) return 0;
            ctx.origin_x = read_at<std::int32_t>(display_interface, 0x04, 0);
            ctx.origin_y = read_at<std::int32_t>(display_interface, 0x08, 0);
            ctx.updated.info_display = ctx.info_display;
            ctx.updated.craft = ctx.craft;
            return 1;
        }
        if (action == 2) {
            ctx.builder_shield = g_active_panel == 1 &&
                g_ui_configuration.builder_shield_bar_rectangle_found;
            return ctx.builder_shield || g_ui_configuration.shield_bar_rectangle_found;
        }
        if (action == 3) {
            const auto& configured = ctx.builder_shield
                ? g_ui_configuration.builder_shield_bar_rectangle
                : g_ui_configuration.shield_bar_rectangle;
            const auto offset = read_at<std::int32_t>(ctx.info_display, 0x1f0, 0);
            ctx.shield_rectangle = NativeRectangle{configured.x, configured.y - offset,
                configured.x + configured.width - 1,
                configured.y - offset + configured.height - 1};
            ctx.current_shields = read_at<float>(ctx.craft, kCurrentShieldsOffset,
                std::numeric_limits<float>::quiet_NaN());
            ctx.maximum_shields = read_at<float>(ctx.craft, kMaximumShieldsOffset,
                std::numeric_limits<float>::quiet_NaN());
        } else if (action == 4) {
            return std::isfinite(ctx.current_shields) && ctx.current_shields >= 0.0f &&
                std::isfinite(ctx.maximum_shields) && ctx.maximum_shields > 0.0f;
        } else if (action == 6) {
            return g_ui_configuration.experience_bar_rectangle_found ||
                panel_style(PanelElement::ExperienceBar).area_found;
        } else if (action == 7) {
            return read_experience_progress(
                ctx.craft, &ctx.current_xp, &ctx.next_rank_xp);
        } else if (action == 8) {
            ctx.experience_rectangle = panel_rectangle(PanelElement::ExperienceBar,
                translated_rectangle(*ctx.live_anchor,
                    g_ui_configuration.captain_rectangle,
                    g_ui_configuration.experience_bar_rectangle));
            const Colour experience_colour =
                g_ui_configuration.experience_bar_colour_found
                ? g_ui_configuration.experience_bar_colour
                : Colour{0.2f, 0.65f, 1.0f};
            ctx.experience_drawn = draw_ammunition_value_bar(
                ctx.experience_rectangle,
                clamped_ratio(ctx.current_xp, ctx.next_rank_xp), experience_colour,
                ctx.origin_x, ctx.origin_y,
                g_ui_configuration.experience_bar_background_colour_found
                    ? &g_ui_configuration.experience_bar_background_colour : nullptr);
            return ctx.experience_drawn;
        } else if (action == 10) {
            if (ctx.updated.active) g_selected_status_tooltip = ctx.updated;
        } else if (action == 11) {
            if (ctx.experience_drawn && InterlockedCompareExchange(
                    &g_selected_status_draw_report_count, 1, 0) == 0) {
                char message[192]{};
                std::snprintf(message, sizeof(message),
                    "First selected-object XP bar submitted: %.0f / %.0f",
                    static_cast<double>(ctx.current_xp),
                    static_cast<double>(ctx.next_rank_xp));
                log_line(message);
            }
        }
        if (action == 5 || action == 9) {
            const auto status = action == 5
                ? SelectedStatusIndex::shields : SelectedStatusIndex::experience;
            const auto& rectangle = action == 5
                ? ctx.shield_rectangle : ctx.experience_rectangle;
            if (!usable_native_rectangle(rectangle)) return 0;
            const std::size_t index = static_cast<std::size_t>(status);
            ctx.updated.visible[index] = true;
            ctx.updated.current[index] = action == 5
                ? ctx.current_shields : ctx.current_xp;
            ctx.updated.maximum[index] = action == 5
                ? ctx.maximum_shields : ctx.next_rank_xp;
            ctx.updated.hit_rectangles[index] = FloatRectangle{
                static_cast<float>(rectangle.left + ctx.origin_x),
                static_cast<float>(rectangle.top + ctx.origin_y),
                static_cast<float>(rectangle.right - rectangle.left),
                static_cast<float>(rectangle.bottom - rectangle.top)};
            ctx.updated.active = true;
        }
        return 0;
    };
    a2fo_rs_stage9_craft_draw_status_bars(step, &context);
}

float clamped_ratio(float current, float maximum) noexcept {
    if (!std::isfinite(current) || !std::isfinite(maximum) ||
        maximum <= 0.0f) {
        return 0.0f;
    }
    return std::max(0.0f, std::min(1.0f, current / maximum));
}

Colour directional_shield_status_colour(float ratio) noexcept {
    constexpr Colour healthy_default{0.1f, 1.0f, 0.1f};
    constexpr Colour low_default{1.0f, 0.5f, 0.0f};
    constexpr Colour critical_default{1.0f, 0.05f, 0.02f};
    if (ratio <= 0.25f) {
        return g_ui_configuration.directional_shield_critical_colour_found
            ? g_ui_configuration.directional_shield_critical_colour
            : critical_default;
    }
    if (ratio <= 0.5f) {
        return g_ui_configuration.directional_shield_low_colour_found
            ? g_ui_configuration.directional_shield_low_colour
            : low_default;
    }
    return g_ui_configuration.directional_shield_colour_found
        ? g_ui_configuration.directional_shield_colour
        : healthy_default;
}

Colour directional_shield_value_status_colour(
    float ratio, const Colour& shared_colour) noexcept {
    if (ratio <= 0.25f) {
        if (g_ui_configuration.
                directional_shield_value_critical_colour_found) {
            return g_ui_configuration.
                directional_shield_value_critical_colour;
        }
    } else if (ratio <= 0.5f) {
        if (g_ui_configuration.directional_shield_value_low_colour_found) {
            return g_ui_configuration.directional_shield_value_low_colour;
        }
    } else if (g_ui_configuration.directional_shield_value_colour_found) {
        return g_ui_configuration.directional_shield_value_colour;
    }
    return g_ui_configuration.directional_shield_value_colour_found
        ? g_ui_configuration.directional_shield_value_colour
        : shared_colour;
}

Colour dimmed_colour(const Colour& colour) noexcept {
    return Colour{
        colour.red * 0.16f,
        colour.green * 0.16f,
        colour.blue * 0.16f};
}

float move_towards(float displayed, float target,
                   float maximum_step) noexcept {
    if (target > displayed) return std::min(target, displayed + maximum_step);
    return std::max(target, displayed - maximum_step);
}

std::array<float, 4> animated_directional_shield_ratios(
    void* craft, const std::array<float, 4>& current,
    const std::array<float, 4>& maximum) noexcept {
    struct Context {
        void* craft;
        const std::array<float, 4>* current;
        const std::array<float, 4>* maximum;
        std::array<float, 4> target{}, result{};
        DWORD now = 0, elapsed = 0;
        float maximum_step = 0.0f;
    } context{craft, &current, &maximum};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            ctx.target[index] = clamped_ratio(
                (*ctx.current)[index], (*ctx.maximum)[index]);
        } else if (action == 1) {
            ctx.now = GetTickCount();
            ctx.elapsed = ctx.now - g_directional_shield_animation.last_tick;
            return !g_directional_shield_animation.initialized ||
                g_directional_shield_animation.craft != ctx.craft ||
                ctx.elapsed > 1000u;
        } else if (action == 2) {
            g_directional_shield_animation.craft = ctx.craft;
            g_directional_shield_animation.last_tick = ctx.now;
            g_directional_shield_animation.initialized = true;
            g_directional_shield_animation.displayed_ratio = ctx.target;
            ctx.result = ctx.target;
        } else if (action == 3) {
            ctx.maximum_step = std::min(
                1.0f, static_cast<float>(ctx.elapsed) / 450.0f);
        } else if (action == 4) {
            g_directional_shield_animation.displayed_ratio[index] = move_towards(
                g_directional_shield_animation.displayed_ratio[index],
                ctx.target[index], ctx.maximum_step);
        } else if (action == 5) {
            g_directional_shield_animation.last_tick = ctx.now;
            ctx.result = g_directional_shield_animation.displayed_ratio;
        }
        return 0;
    };
    a2fo_rs_stage9_craft_animate_shield_ratios(step, &context,
        context.target.size());
    return context.result;
}

bool usable_float_rectangle(const FloatRectangle& rectangle) noexcept {
    return std::isfinite(rectangle.x) && std::isfinite(rectangle.y) &&
        std::isfinite(rectangle.width) && std::isfinite(rectangle.height) &&
        rectangle.width > 0.0f && rectangle.height > 0.0f;
}

void* find_interface_sprite(void* database, const char* name) noexcept {
    if (!database || !name || !*name) return nullptr;
    return reinterpret_cast<void*>(a2fo_identity_call_thiscall_2(
        at(g_armada, kInterfaceSpriteDatabaseGetRva), database,
        reinterpret_cast<std::uintptr_t>(name), 0));
}

bool directional_shield_sprite_is_drawable(void* sprite) noexcept {
    if (!sprite || !writable_range(
            static_cast<std::uint8_t*>(sprite) + kSpriteColourOffset,
            sizeof(Colour))) {
        return false;
    }
    // ST3D_Sprite::DrawScaled2D begins by loading the linked frame-list
    // sentinel from sprite+0x1c and immediately dereferencing it. Destruction
    // clears that pointer, which is the abort/new-mission crash signature at
    // Armada VA 0x0063adbf. Validate the sentinel before entering native code.
    void* frame_list = read_at<void*>(
        sprite, kSpriteFrameListOffset, nullptr);
    return frame_list && readable_range(frame_list, sizeof(void*));
}

void* ammunition_icon_sprite(
    void* database, const std::string& reference) noexcept {
    void* sprite = find_interface_sprite(database, reference.c_str());
    if (sprite) return sprite;

    // GUI ODF authors naturally identify a sprite sheet by its texture name,
    // while interfaceDB is keyed by the first (sprite-name) column in .spr
    // files. Borrow a stable sprite already backed by each stock atlas. Its
    // source window is restored immediately after the ammunition icon draw.
    struct AtlasCarrier {
        const char* atlas;
        const char* sprite;
    };
    constexpr std::array<AtlasCarrier, 5> carriers{{
        {"all_interface", "energy_amt"},
        {"all_interface2", "gravmine_icon"},
        {"all_interface_races", "borg_icon_small"},
        {"all_interface_ranks", "select_rank_1"},
        {"all_interface_ranks2", "select_rank_6"},
    }};
    struct Context {
        void* database;
        const std::string& reference;
        const std::array<AtlasCarrier, 5>& carriers;
        void* sprite = nullptr;
    } context{database, reference, carriers};
    const auto step = +[](void* opaque, std::uint32_t,
                          std::size_t index) noexcept -> std::uint32_t {
        auto& ctx = *static_cast<Context*>(opaque);
        const AtlasCarrier& carrier = ctx.carriers[index];
        if (_stricmp(ctx.reference.c_str(), carrier.atlas) != 0) return 0;
        ctx.sprite = find_interface_sprite(ctx.database, carrier.sprite);
        return 1;
    };
    a2fo_rs_stage10_identity_first_match(carriers.size(), step, &context);
    return context.sprite;
}

bool ammunition_icon_texture_dimensions(
    void* sprite, UINT* width, UINT* height) noexcept {
    if (width) *width = 0;
    if (height) *height = 0;
    if (!sprite || !width || !height || !g_armada) return false;

    void* renderer = read_at<void*>(
        at(g_armada, kGraphicsEnginePointerRva), 0, nullptr);
    const std::uint32_t device_index = read_at<std::uint32_t>(
        renderer, kCurrentDeviceIndexOffset, kMaximumStormDeviceCount);
    if (device_index >= kMaximumStormDeviceCount) return false;
    void* storm_texture = read_at<void*>(
        sprite, kSpriteTextureObjectOffset, nullptr);
    void* device_texture = read_at<void*>(
        storm_texture, kStorm3DTextureDeviceArrayOffset +
            device_index * sizeof(void*), nullptr);
    IDirect3DTexture8* native_texture = read_at<IDirect3DTexture8*>(
        device_texture, kStorm3DDeviceTextureNativeOffset, nullptr);
    if (!native_texture || !readable_range(native_texture, sizeof(void*))) {
        return false;
    }
    void* vtable = read_at<void*>(native_texture, 0, nullptr);
    if (!readable_range(vtable, 18 * sizeof(void*))) return false;

    D3DSURFACE_DESC description{};
    if (FAILED(native_texture->GetLevelDesc(0, &description)) ||
        description.Width == 0 || description.Height == 0) {
        return false;
    }
    *width = description.Width;
    *height = description.Height;
    return true;
}

// The icon uses a live reference to its cache slot throughout draw/restore;
// the bar layer below deliberately snapshots its sprite before native draws.
template<class Context, class Step>
bool craft_sprite_policy(std::uint32_t kind, Context& context, Step step) {
    struct Call { Context& context; Step& step; std::exception_ptr error; std::uint8_t failed = 0; };
    Call call{context, step, {}, 0};
    const auto bridge = +[](void* opaque, std::uint32_t op, std::uintptr_t arg) -> std::uintptr_t {
        auto& c = *static_cast<Call*>(opaque);
        try { return c.step(c.context, op, arg); }
        catch (...) { c.error = std::current_exception(); c.failed = 1; return 0; }
    };
    const bool result = a2fo_rs_stage9_craft_sprite_flow(kind, bridge, &call, &call.failed) != 0;
    if (call.error) std::rethrow_exception(call.error);
    return result;
}

bool draw_ammunition_icon(
    std::size_t store_index, const std::string& sprite_name,
    const RawRectangle& source_rectangle,
    const NativeRectangle& rectangle, const Colour& colour,
    std::int32_t display_origin_x,
    std::int32_t display_origin_y) noexcept {
    struct Context {
        std::size_t index; void* database = nullptr; AmmunitionIconCache* cache = nullptr;
        std::uint8_t *x = nullptr, *y = nullptr, *width = nullptr, *height = nullptr;
        UINT texture_width = 0, texture_height = 0;
        Colour saved_colour{}; FloatRectangle saved_window{}; SpriteVector position{};
        void*& sprite() { return cache->sprites[index]; }
    } c{store_index};
    return craft_sprite_policy(0, c, [&](Context& ctx, std::uint32_t op, std::uintptr_t) -> std::uintptr_t {
        switch (op) {
        case 0: {

    if (store_index >= g_ammunition_icon_cache.sprites.size() ||
        sprite_name.empty() || !usable_native_rectangle(rectangle) ||
        source_rectangle.x < 0 || source_rectangle.y < 0 ||
        source_rectangle.width <= 0 || source_rectangle.height <= 0 ||
        !g_fleet_ops ||
        !readable_range(at(g_armada, kInterfaceSpriteDatabaseGetRva),
                        sizeof(kExpectedInterfaceSpriteDatabaseGet)) ||
        std::memcmp(at(g_armada, kInterfaceSpriteDatabaseGetRva),
                    kExpectedInterfaceSpriteDatabaseGet,
                    sizeof(kExpectedInterfaceSpriteDatabaseGet)) != 0 ||
        !readable_range(at(g_fleet_ops, kFoSpriteSetColourRva),
                        sizeof(kExpectedFoSpriteSetColour)) ||
        std::memcmp(at(g_fleet_ops, kFoSpriteSetColourRva),
                    kExpectedFoSpriteSetColour,
                    sizeof(kExpectedFoSpriteSetColour)) != 0 ||
        !readable_range(at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
                        sizeof(kExpectedFoSpriteDrawScaled2D)) ||
        std::memcmp(at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
                    kExpectedFoSpriteDrawScaled2D,
                    sizeof(kExpectedFoSpriteDrawScaled2D)) != 0) {
        return false;
    }

            return 1;
        }
        case 1: ctx.database = read_at<void*>(at(g_armada, kInterfaceSpriteDatabasePointerRva), 0, nullptr); ctx.cache = &g_ammunition_icon_cache; return ctx.database != nullptr;
        case 2: return ctx.cache->database == ctx.database;
        case 3: *ctx.cache = {}; ctx.cache->database = ctx.database; return 0;
        case 4: return ctx.cache->names[store_index] == sprite_name;
        case 5: ctx.cache->names[store_index] = sprite_name; ctx.sprite() = nullptr; ctx.cache->attempted[store_index] = false; return 0;
        case 6: case 9: return ctx.sprite() != nullptr;
        case 7: case 13: return directional_shield_sprite_is_drawable(ctx.sprite());
        case 8: ctx.sprite() = nullptr; ctx.cache->attempted[store_index] = false; return 0;
        case 10: return ctx.cache->attempted[store_index];
        case 11: ctx.cache->attempted[store_index] = true; return 0;
        case 12: ctx.sprite() = ammunition_icon_sprite(ctx.database, sprite_name); return 0;
        case 14: case 23: return static_cast<std::uint32_t>(InterlockedCompareExchange(&g_ammunition_icon_failure_report_count, 1, 0));
        case 15: log_line(std::string("Ammunition icon sprite '") + sprite_name + "' was not found or draw-ready; using the compact value without a label"); return 0;
        case 16:
            ctx.x = static_cast<std::uint8_t*>(ctx.sprite()) + kSpriteTextureXOffset;
            ctx.y = static_cast<std::uint8_t*>(ctx.sprite()) + kSpriteTextureYOffset;
            ctx.width = static_cast<std::uint8_t*>(ctx.sprite()) + kSpriteTextureWidthOffset;
            ctx.height = static_cast<std::uint8_t*>(ctx.sprite()) + kSpriteTextureHeightOffset; return 0;
        case 17: return writable_range(ctx.x, sizeof(float));
        case 18: return writable_range(ctx.y, sizeof(float));
        case 19: return writable_range(ctx.width, sizeof(float));
        case 20: return writable_range(ctx.height, sizeof(float));
        case 21: return ammunition_icon_texture_dimensions(ctx.sprite(), &ctx.texture_width, &ctx.texture_height);
        case 22: return static_cast<std::uint64_t>(source_rectangle.x) + static_cast<std::uint64_t>(source_rectangle.width) <= ctx.texture_width &&
            static_cast<std::uint64_t>(source_rectangle.y) + static_cast<std::uint64_t>(source_rectangle.height) <= ctx.texture_height;
        case 24: log_line(std::string("Ammunition icon crop for '") + sprite_name + "' is outside its texture or its dimensions are unavailable"); return 0;
        case 25: ctx.saved_colour = read_at<Colour>(ctx.sprite(), kSpriteColourOffset, Colour{}); return 0;
        case 26: ctx.saved_window = FloatRectangle{
            read_at<float>(ctx.sprite(), kSpriteTextureXOffset, 0.0f),
            read_at<float>(ctx.sprite(), kSpriteTextureYOffset, 0.0f),
            read_at<float>(ctx.sprite(), kSpriteTextureWidthOffset, 0.0f),
            read_at<float>(ctx.sprite(), kSpriteTextureHeightOffset, 0.0f)}; return 0;
        case 27: return usable_float_rectangle(ctx.saved_window);
        case 28: {
            // Preserve the original x87 division/conversion and write order.
            const float x = static_cast<float>(source_rectangle.x) / static_cast<float>(ctx.texture_width);
            const float y = static_cast<float>(source_rectangle.y) / static_cast<float>(ctx.texture_height);
            const float width = static_cast<float>(source_rectangle.width) / static_cast<float>(ctx.texture_width);
            const float height = static_cast<float>(source_rectangle.height) / static_cast<float>(ctx.texture_height);
            std::memcpy(ctx.x, &x, sizeof(x)); std::memcpy(ctx.y, &y, sizeof(y));
            std::memcpy(ctx.width, &width, sizeof(width)); std::memcpy(ctx.height, &height, sizeof(height));
            ctx.position = SpriteVector{static_cast<float>(rectangle.left + display_origin_x), static_cast<float>(rectangle.top + display_origin_y), 0.0f}; return 0;
        }
        case 29: a2fo_identity_fo_sprite_set_colour(at(g_fleet_ops, kFoSpriteSetColourRva), ctx.sprite(), &colour); return 0;
        case 30: a2fo_identity_fo_sprite_draw_scaled_2d(at(g_fleet_ops, kFoSpriteDrawScaled2DRva), ctx.sprite(), &ctx.position,
            static_cast<float>(rectangle.right - rectangle.left), static_cast<float>(rectangle.bottom - rectangle.top)); return 0;
        case 31: a2fo_identity_fo_sprite_set_colour(at(g_fleet_ops, kFoSpriteSetColourRva), ctx.sprite(), &ctx.saved_colour); return 0;
        case 32:
            std::memcpy(ctx.x, &ctx.saved_window.x, sizeof(ctx.saved_window.x)); std::memcpy(ctx.y, &ctx.saved_window.y, sizeof(ctx.saved_window.y));
            std::memcpy(ctx.width, &ctx.saved_window.width, sizeof(ctx.saved_window.width)); std::memcpy(ctx.height, &ctx.saved_window.height, sizeof(ctx.saved_window.height)); return 0;
        default: return 0;
        }
    });
}

bool draw_ammunition_bar_layer(
    void* database, std::size_t layer, const std::string& requested,
    float fill, const Colour& tint, const NativeRectangle& rectangle,
    std::int32_t display_origin_x, std::int32_t display_origin_y,
    PanelElement element) noexcept {
    struct Context {
        AmmunitionBarCache* cache = nullptr; std::string name;
        void* sprite = nullptr; std::uint8_t* width_address = nullptr;
        float saved_width = 0.0f; Colour saved_colour{}; SpriteVector position{};
    } c;
    return craft_sprite_policy(1, c, [&](Context& ctx, std::uint32_t op, std::uintptr_t) -> std::uintptr_t {
        switch (op) {
        case 0: ctx.cache = &g_ammunition_bar_cache[static_cast<std::size_t>(element)][layer]; ctx.name = requested.empty() ? "large_shield_bar" : requested; return 0;
        case 1: return ctx.cache->database == database;
        case 2: return ctx.cache->name == ctx.name;
        case 3: *ctx.cache = {}; ctx.cache->database = database; ctx.cache->name = ctx.name; return 0;
        case 4: return ctx.cache->sprite != nullptr;
        case 5: case 10: return directional_shield_sprite_is_drawable(ctx.cache->sprite);
        case 6: ctx.cache->sprite = nullptr; ctx.cache->attempted = false; return 0;
        case 7: return ctx.cache->attempted;
        case 8: ctx.cache->attempted = true; return 0;
        case 9: ctx.cache->sprite = find_interface_sprite(database, ctx.name.c_str()); return 0;
        case 11: return requested.empty();
        case 12: log_line(std::string("Panel bar sprite '") + ctx.name + "' unavailable; using large_shield_bar"); return 0;
        case 13: ctx.cache->sprite = find_interface_sprite(database, "large_shield_bar"); return 0;
        case 14: ctx.sprite = ctx.cache->sprite; return 0;
        case 15: return directional_shield_sprite_is_drawable(ctx.sprite);
        case 16: ctx.width_address = static_cast<std::uint8_t*>(ctx.sprite) + kSpriteTextureWidthOffset; return writable_range(ctx.width_address, sizeof(float));
        case 17: ctx.saved_width = read_at<float>(ctx.sprite, kSpriteTextureWidthOffset, 0.0f); return 0;
        case 18: return std::isfinite(ctx.saved_width) && ctx.saved_width > 0.0f;
        case 19: ctx.saved_colour = read_at<Colour>(ctx.sprite, kSpriteColourOffset, Colour{}); return 0;
        case 20: {
            const float cropped_width = ctx.saved_width * fill;
            std::memcpy(ctx.width_address, &cropped_width, sizeof(cropped_width));
            ctx.position = SpriteVector{static_cast<float>(rectangle.left + display_origin_x), static_cast<float>(rectangle.top + display_origin_y), 0.0f}; return 0;
        }
        case 21: a2fo_identity_fo_sprite_set_colour(at(g_fleet_ops, kFoSpriteSetColourRva), ctx.sprite, &tint); return 0;
        case 22: a2fo_identity_fo_sprite_draw_scaled_2d(at(g_fleet_ops, kFoSpriteDrawScaled2DRva), ctx.sprite, &ctx.position,
            (rectangle.right - rectangle.left) * fill, static_cast<float>(rectangle.bottom - rectangle.top)); return 0;
        case 23: std::memcpy(ctx.width_address, &ctx.saved_width, sizeof(ctx.saved_width)); return 0;
        case 24: a2fo_identity_fo_sprite_set_colour(at(g_fleet_ops, kFoSpriteSetColourRva), ctx.sprite, &ctx.saved_colour); return 0;
        default: return 0;
        }
    });
}

bool draw_ammunition_value_bar(
    const NativeRectangle& rectangle, float ratio, const Colour& colour,
    std::int32_t display_origin_x,
    std::int32_t display_origin_y,
    const Colour* background_colour, PanelElement element) noexcept {
    struct Context { void* database = nullptr; const PanelElementStyle* style = nullptr; Colour background{}; float fill = 0.0f; } c;
    return craft_sprite_policy(2, c, [&](Context& ctx, std::uint32_t op, std::uintptr_t) -> std::uintptr_t {
        switch (op) {
        case 0: {

    if (!usable_native_rectangle(rectangle) || !g_armada || !g_fleet_ops ||
        !readable_range(at(g_armada, kInterfaceSpriteDatabaseGetRva),
                        sizeof(kExpectedInterfaceSpriteDatabaseGet)) ||
        std::memcmp(at(g_armada, kInterfaceSpriteDatabaseGetRva),
                    kExpectedInterfaceSpriteDatabaseGet,
                    sizeof(kExpectedInterfaceSpriteDatabaseGet)) != 0 ||
        !readable_range(at(g_fleet_ops, kFoSpriteSetColourRva),
                        sizeof(kExpectedFoSpriteSetColour)) ||
        std::memcmp(at(g_fleet_ops, kFoSpriteSetColourRva),
                    kExpectedFoSpriteSetColour,
                    sizeof(kExpectedFoSpriteSetColour)) != 0 ||
        !readable_range(at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
                        sizeof(kExpectedFoSpriteDrawScaled2D)) ||
        std::memcmp(at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
                    kExpectedFoSpriteDrawScaled2D,
                    sizeof(kExpectedFoSpriteDrawScaled2D)) != 0) {
        return false;
    }

            return 1;
        }
        case 1: ctx.database = read_at<void*>(at(g_armada, kInterfaceSpriteDatabasePointerRva), 0, nullptr); return ctx.database != nullptr;
        case 2: ctx.style = &panel_style(element); return 0;
        case 3: ctx.background = ctx.style->background_found ? ctx.style->background :
            background_colour ? *background_colour : Colour{0.08f, 0.08f, 0.08f}; return 0;
        case 4: return draw_ammunition_bar_layer(ctx.database, 0,
            ctx.style->background_sprite.empty() ? ctx.style->sprite : ctx.style->background_sprite,
            1.0f, ctx.background, rectangle, display_origin_x, display_origin_y, element);
        case 5: ctx.fill = std::isfinite(ratio) ? std::clamp(ratio, 0.0f, 1.0f) : 0.0f; return 0;
        case 6: return ctx.fill > 0.0f;
        case 7: {
            const Colour tint = panel_colour(element, colour, ctx.fill);
            return draw_ammunition_bar_layer(ctx.database, 1, ctx.style->sprite, ctx.fill, tint,
                rectangle, display_origin_x, display_origin_y, element);
        }
        default: return 0;
        }
    });
}

bool directional_shield_sprite_cache_is_live(void* database) noexcept {
    if (!g_directional_shield_sprites.ready || !database ||
        g_directional_shield_sprites.database != database) {
        return false;
    }
    const std::size_t count = g_directional_shield_sprites.compact ? 2u : 4u;
    const auto step = +[](void*, std::uint32_t, std::uintptr_t index) noexcept -> std::uintptr_t {
        return directional_shield_sprite_is_drawable(
            g_directional_shield_sprites.sprites[index]);
    };
    return a2fo_rs_stage9_craft_all_sprites_live(step, nullptr, count);
}

bool resolve_directional_shield_sprites() noexcept {
    struct Context { void* database = nullptr; } context{};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            if (!g_fleet_ops ||
                !readable_range(at(g_armada, kInterfaceSpriteDatabaseGetRva),
                                sizeof(kExpectedInterfaceSpriteDatabaseGet)) ||
                std::memcmp(at(g_armada, kInterfaceSpriteDatabaseGetRva),
                            kExpectedInterfaceSpriteDatabaseGet,
                            sizeof(kExpectedInterfaceSpriteDatabaseGet)) != 0 ||
                !readable_range(at(g_fleet_ops, kFoSpriteSetColourRva),
                                sizeof(kExpectedFoSpriteSetColour)) ||
                std::memcmp(at(g_fleet_ops, kFoSpriteSetColourRva),
                            kExpectedFoSpriteSetColour,
                            sizeof(kExpectedFoSpriteSetColour)) != 0 ||
                !readable_range(at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
                                sizeof(kExpectedFoSpriteDrawScaled2D)) ||
                std::memcmp(at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
                            kExpectedFoSpriteDrawScaled2D,
                            sizeof(kExpectedFoSpriteDrawScaled2D)) != 0) {
                log_line("Directional-shield sprite helpers are unavailable; numeric display retained");
                return 0;
            }
            return 1;
        }
        if (action == 1) {
            ctx.database = read_at<void*>(
                at(g_armada, kInterfaceSpriteDatabasePointerRva), 0, nullptr);
        } else if (action == 2) {
            if (g_directional_shield_sprite_retry_after != 0) {
                const DWORD now = GetTickCount();
                if (g_directional_shield_sprite_retry_database != ctx.database) {
                    g_directional_shield_sprite_retry_database = ctx.database;
                    g_directional_shield_sprite_retry_after =
                        now + kDirectionalShieldSpriteStabilizationMs;
                }
                if (static_cast<LONG>(now - g_directional_shield_sprite_retry_after) < 0)
                    return 0;
                g_directional_shield_sprite_retry_after = 0;
                g_directional_shield_sprite_retry_database = nullptr;
            }
            return 1;
        } else if (action == 3) {
            if (g_directional_shield_sprites.resolution_attempted) {
                if (directional_shield_sprite_cache_is_live(ctx.database)) return 1;
                if (!g_directional_shield_sprites.ready &&
                    g_directional_shield_sprites.database == ctx.database) {
                    g_directional_shield_sprites.resolution_attempted = false;
                    g_directional_shield_sprites.sprites = {};
                    return 2;
                }
                reset_directional_shield_sprite_runtime();
                return 3;
            }
        } else if (action == 4) {
            g_directional_shield_sprites.resolution_attempted = true;
            g_directional_shield_sprites.database = ctx.database;
            if (!ctx.database) {
                log_line("Directional-shield sprite database is unavailable; numeric display retained");
                return 0;
            }
            return 1;
        } else if (action == 5) {
            g_directional_shield_sprites.sprites[index] = find_interface_sprite(
                ctx.database, kCompactQuadShieldSpriteNames[index]);
        } else if (action == 6) {
            return directional_shield_sprite_is_drawable(
                g_directional_shield_sprites.sprites[index]);
        } else if (action == 7) {
            g_directional_shield_sprites.compact = true;
            g_directional_shield_sprites.ready = true;
            log_line("Compact quad directional-shield sprites resolved (dsq/dsqs)");
        } else if (action == 8) {
            g_directional_shield_sprites.sprites = {};
            g_directional_shield_sprites.compact = false;
        } else if (action == 9) {
            g_directional_shield_sprites.sprites[index] = find_interface_sprite(
                ctx.database, kDirectionalShieldSpriteNames[index]);
        } else if (action == 10) {
            return directional_shield_sprite_is_drawable(
                g_directional_shield_sprites.sprites[index]);
        } else if (action == 11) {
            void* sprite = g_directional_shield_sprites.sprites[index];
            void* frame_list = sprite ? read_at<void*>(
                sprite, kSpriteFrameListOffset, nullptr) : nullptr;
            if (InterlockedCompareExchange(
                    &g_directional_shield_sprite_failure_report_count, 1, 0) == 0) {
                char message[448]{};
                std::snprintf(message, sizeof(message),
                    "Compact quad sprites dsq/dsqs were unavailable and legacy directional-shield sprite '%s' was missing or not draw-ready (sprite=%p frameList=%p database=%p); retrying after interface quiet period",
                    kDirectionalShieldSpriteNames[index], sprite,
                    frame_list, ctx.database);
                log_line(message);
            }
            g_directional_shield_sprite_retry_database = ctx.database;
            g_directional_shield_sprite_retry_after =
                GetTickCount() + kDirectionalShieldSpriteStabilizationMs;
        } else if (action == 12) {
            g_directional_shield_sprites.ready = true;
            log_line("Legacy four-sprite directional-shield ring resolved");
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_resolve_shield_sprites(
        step, &context, kCompactQuadShieldSpriteNames.size(),
        kDirectionalShieldSpriteNames.size());
}

bool tri_shield_sprite_cache_is_live(void* database) noexcept {
    if (!g_tri_shield_sprites.ready || !database ||
        g_tri_shield_sprites.database != database) {
        return false;
    }
    const auto step = +[](void*, std::uint32_t, std::uintptr_t index) noexcept -> std::uintptr_t {
        return directional_shield_sprite_is_drawable(g_tri_shield_sprites.sprites[index]);
    };
    return a2fo_rs_stage9_craft_all_sprites_live(
        step, nullptr, g_tri_shield_sprites.sprites.size());
}

bool resolve_tri_shield_sprites() noexcept {
    if (!g_fleet_ops ||
        !readable_range(at(g_armada, kInterfaceSpriteDatabaseGetRva),
                        sizeof(kExpectedInterfaceSpriteDatabaseGet)) ||
        std::memcmp(at(g_armada, kInterfaceSpriteDatabaseGetRva),
                    kExpectedInterfaceSpriteDatabaseGet,
                    sizeof(kExpectedInterfaceSpriteDatabaseGet)) != 0 ||
        !readable_range(at(g_fleet_ops, kFoSpriteSetColourRva),
                        sizeof(kExpectedFoSpriteSetColour)) ||
        std::memcmp(at(g_fleet_ops, kFoSpriteSetColourRva),
                    kExpectedFoSpriteSetColour,
                    sizeof(kExpectedFoSpriteSetColour)) != 0 ||
        !readable_range(at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
                        sizeof(kExpectedFoSpriteDrawScaled2D)) ||
        std::memcmp(at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
                    kExpectedFoSpriteDrawScaled2D,
                    sizeof(kExpectedFoSpriteDrawScaled2D)) != 0) {
        return false;
    }

    struct TriContext { void* database = nullptr; } context;
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<TriContext*>(opaque);
        if (action == 0) {
            ctx.database = read_at<void*>(
                at(g_armada, kInterfaceSpriteDatabasePointerRva), 0, nullptr);
        } else if (action == 1) {
            if (g_directional_shield_sprite_retry_after != 0) {
                const DWORD now = GetTickCount();
                if (g_directional_shield_sprite_retry_database != ctx.database) {
                    g_directional_shield_sprite_retry_database = ctx.database;
                    g_directional_shield_sprite_retry_after =
                        now + kDirectionalShieldSpriteStabilizationMs;
                }
                if (static_cast<LONG>(
                        now - g_directional_shield_sprite_retry_after) < 0) return 0;
                g_directional_shield_sprite_retry_after = 0;
                g_directional_shield_sprite_retry_database = nullptr;
            }
            return 1;
        } else if (action == 2) {
            if (g_tri_shield_sprites.resolution_attempted) {
                if (tri_shield_sprite_cache_is_live(ctx.database)) return 1;
                if (!g_tri_shield_sprites.ready &&
                    g_tri_shield_sprites.database == ctx.database) {
                    g_tri_shield_sprites.resolution_attempted = false;
                    g_tri_shield_sprites.sprites = {};
                    return 2;
                }
                reset_directional_shield_sprite_runtime();
                return 3;
            }
        } else if (action == 3) {
            g_tri_shield_sprites.resolution_attempted = true;
            g_tri_shield_sprites.database = ctx.database;
            return ctx.database != nullptr;
        } else if (action == 4) {
            g_tri_shield_sprites.sprites[index] = find_interface_sprite(
                ctx.database, kTriShieldSpriteNames[index]);
        } else if (action == 5) {
            return directional_shield_sprite_is_drawable(
                g_tri_shield_sprites.sprites[index]);
        } else if (action == 6) {
            if (InterlockedCompareExchange(
                    &g_directional_shield_sprite_failure_report_count, 1, 0) == 0) {
                char message[320]{};
                std::snprintf(message, sizeof(message),
                    "Tri shield sprite '%s' is missing or not draw-ready; numeric display retained",
                    kTriShieldSpriteNames[index]);
                log_line(message);
            }
            g_directional_shield_sprite_retry_database = ctx.database;
            g_directional_shield_sprite_retry_after =
                GetTickCount() + kDirectionalShieldSpriteStabilizationMs;
        } else if (action == 7) {
            g_tri_shield_sprites.ready = true;
            log_line("Tri directional-shield sprites resolved");
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_resolve_simple_sprites(
        step, &context, kTriShieldSpriteNames.size());
}

bool hemisphere_shield_sprite_cache_is_live(void* database) noexcept {
    return g_hemisphere_shield_sprite.ready && database &&
        g_hemisphere_shield_sprite.database == database &&
        directional_shield_sprite_is_drawable(
            g_hemisphere_shield_sprite.sprite);
}

bool hemisphere_side_shield_sprite_cache_is_live(void* database) noexcept {
    return g_hemisphere_side_shield_sprite.ready && database &&
        g_hemisphere_side_shield_sprite.database == database &&
        directional_shield_sprite_is_drawable(
            g_hemisphere_side_shield_sprite.sprite);
}

bool resolve_hemisphere_sprite_runtime(
    HemisphereShieldSpriteRuntime* runtime,
    bool (*cache_is_live)(void*) noexcept,
    const char* sprite_name, const char* failure_format,
    const char* resolved_message) noexcept {
    struct Context {
        HemisphereShieldSpriteRuntime* runtime;
        bool (*cache_is_live)(void*) noexcept;
        const char* sprite_name;
        const char* failure_format;
        const char* resolved_message;
        void* database = nullptr;
    } context{runtime, cache_is_live, sprite_name, failure_format, resolved_message};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            ctx.database = read_at<void*>(
                at(g_armada, kInterfaceSpriteDatabasePointerRva), 0, nullptr);
        } else if (action == 1) {
            if (g_directional_shield_sprite_retry_after != 0) {
                const DWORD now = GetTickCount();
                if (g_directional_shield_sprite_retry_database != ctx.database) {
                    g_directional_shield_sprite_retry_database = ctx.database;
                    g_directional_shield_sprite_retry_after =
                        now + kDirectionalShieldSpriteStabilizationMs;
                }
                if (static_cast<LONG>(
                        now - g_directional_shield_sprite_retry_after) < 0) return 0;
                g_directional_shield_sprite_retry_after = 0;
                g_directional_shield_sprite_retry_database = nullptr;
            }
            return 1;
        } else if (action == 2) {
            if (ctx.runtime->resolution_attempted) {
                if (ctx.cache_is_live(ctx.database)) return 1;
                if (!ctx.runtime->ready && ctx.runtime->database == ctx.database) {
                    ctx.runtime->resolution_attempted = false;
                    ctx.runtime->sprite = nullptr;
                    return 2;
                }
                reset_directional_shield_sprite_runtime();
                return 3;
            }
        } else if (action == 3) {
            ctx.runtime->resolution_attempted = true;
            ctx.runtime->database = ctx.database;
            return ctx.database != nullptr;
        } else if (action == 4) {
            ctx.runtime->sprite = find_interface_sprite(ctx.database, ctx.sprite_name);
        } else if (action == 5) {
            return directional_shield_sprite_is_drawable(ctx.runtime->sprite);
        } else if (action == 6) {
            if (InterlockedCompareExchange(
                    &g_directional_shield_sprite_failure_report_count, 1, 0) == 0) {
                char message[320]{};
                std::snprintf(message, sizeof(message),
                    ctx.failure_format, ctx.sprite_name);
                log_line(message);
            }
            g_directional_shield_sprite_retry_database = ctx.database;
            g_directional_shield_sprite_retry_after =
                GetTickCount() + kDirectionalShieldSpriteStabilizationMs;
        } else if (action == 7) {
            ctx.runtime->ready = true;
            log_line(ctx.resolved_message);
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_resolve_simple_sprites(step, &context, 1);
}

bool resolve_hemisphere_shield_sprite() noexcept {
    if (!g_fleet_ops ||
        !readable_range(at(g_armada, kInterfaceSpriteDatabaseGetRva),
                        sizeof(kExpectedInterfaceSpriteDatabaseGet)) ||
        std::memcmp(at(g_armada, kInterfaceSpriteDatabaseGetRva),
                    kExpectedInterfaceSpriteDatabaseGet,
                    sizeof(kExpectedInterfaceSpriteDatabaseGet)) != 0 ||
        !readable_range(at(g_fleet_ops, kFoSpriteSetColourRva),
                        sizeof(kExpectedFoSpriteSetColour)) ||
        std::memcmp(at(g_fleet_ops, kFoSpriteSetColourRva),
                    kExpectedFoSpriteSetColour,
                    sizeof(kExpectedFoSpriteSetColour)) != 0 ||
        !readable_range(at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
                        sizeof(kExpectedFoSpriteDrawScaled2D)) ||
        std::memcmp(at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
                    kExpectedFoSpriteDrawScaled2D,
                    sizeof(kExpectedFoSpriteDrawScaled2D)) != 0) {
        return false;
    }

    return resolve_hemisphere_sprite_runtime(
        &g_hemisphere_shield_sprite, hemisphere_shield_sprite_cache_is_live,
        kHemisphereShieldSpriteName,
        "Hemisphere shield sprite '%s' is missing or not draw-ready; numeric display retained",
        "Hemisphere directional-shield sprite resolved");
}

bool resolve_hemisphere_side_shield_sprite() noexcept {
    if (!g_fleet_ops ||
        !readable_range(at(g_armada, kInterfaceSpriteDatabaseGetRva),
                        sizeof(kExpectedInterfaceSpriteDatabaseGet)) ||
        std::memcmp(at(g_armada, kInterfaceSpriteDatabaseGetRva),
                    kExpectedInterfaceSpriteDatabaseGet,
                    sizeof(kExpectedInterfaceSpriteDatabaseGet)) != 0 ||
        !readable_range(at(g_fleet_ops, kFoSpriteSetColourRva),
                        sizeof(kExpectedFoSpriteSetColour)) ||
        std::memcmp(at(g_fleet_ops, kFoSpriteSetColourRva),
                    kExpectedFoSpriteSetColour,
                    sizeof(kExpectedFoSpriteSetColour)) != 0 ||
        !readable_range(at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
                        sizeof(kExpectedFoSpriteDrawScaled2D)) ||
        std::memcmp(at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
                    kExpectedFoSpriteDrawScaled2D,
                    sizeof(kExpectedFoSpriteDrawScaled2D)) != 0) {
        return false;
    }

    return resolve_hemisphere_sprite_runtime(
        &g_hemisphere_side_shield_sprite,
        hemisphere_side_shield_sprite_cache_is_live,
        kHemisphereSideShieldSpriteName,
        "Side hemisphere shield sprite '%s' is missing or not draw-ready; numeric display retained",
        "Side hemisphere directional-shield sprite resolved");
}

void update_directional_shield_tooltip_runtime(
    void* info_display, void* craft,
    const std::array<float, 4>& current,
    const std::array<float, 4>& maximum,
    const NativeRectangle& display_rectangle,
    const std::array<FloatRectangle, 4>& segment_positions,
    const std::array<bool, 4>& segment_drawn) noexcept {
    struct Context {
        void* info_display;
        void* craft;
        const std::array<float, 4>* current;
        const std::array<float, 4>* maximum;
        const NativeRectangle* display_rectangle;
        const std::array<FloatRectangle, 4>* positions;
        const std::array<bool, 4>* drawn;
        DirectionalShieldTooltipRuntime updated{};
    } context{info_display, craft, &current, &maximum,
              &display_rectangle, &segment_positions, &segment_drawn};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) g_directional_shield_tooltip = {};
        else if (action == 1) {
            if (!ctx.info_display || !ctx.craft ||
                !usable_native_rectangle(*ctx.display_rectangle)) return 0;
            ctx.updated.info_display = ctx.info_display;
            ctx.updated.craft = ctx.craft;
            ctx.updated.current = *ctx.current;
            ctx.updated.maximum = *ctx.maximum;
            return 1;
        } else if (action == 2) return (*ctx.drawn)[index];
        else if (action == 3) {
            const auto& position = (*ctx.positions)[index];
            const FloatRectangle logical_rectangle{
                static_cast<float>(ctx.display_rectangle->left) + position.x,
                static_cast<float>(ctx.display_rectangle->top) + position.y,
                position.width, position.height};
            // Hover coordinates remain in native virtual UI space.
            ctx.updated.hit_rectangles[index] = logical_rectangle;
            ctx.updated.active = ctx.updated.active || usable_float_rectangle(
                ctx.updated.hit_rectangles[index]);
        } else if (action == 4) {
            if (ctx.updated.active) g_directional_shield_tooltip = ctx.updated;
        }
        return 0;
    };
    a2fo_rs_stage9_craft_update_shield_tip_rects(step, &context,
        segment_positions.size());
}

NativeRectangle directional_shield_graphic_rectangle(
    const NativeRectangle& live_anchor) noexcept {
    NativeRectangle rectangle = live_anchor;
    rectangle.left -= 360;
    rectangle.top -= 74;
    rectangle.right = rectangle.left + 128;
    rectangle.bottom = rectangle.top + 128;
    if (g_ui_configuration.captain_rectangle_found &&
        g_ui_configuration.directional_graphic_rectangle_found) {
        rectangle = translated_rectangle(
            live_anchor, g_ui_configuration.captain_rectangle,
            g_ui_configuration.directional_graphic_rectangle);
    }
    return panel_rectangle(PanelElement::DirectionalShieldsGraphic, rectangle);
}

bool draw_directional_shield_graphic(
    void* info_display, void* craft,
    const std::array<float, 4>& current,
    const std::array<float, 4>& maximum, std::uint32_t shield_mode,
    void* text_component, const NativeRectangle& live_anchor) noexcept {
    struct Segment {
        bool compact = false, tri = false, hemisphere = false;
        bool colour_only = false;
        std::size_t physical_quad_index = 0, sprite_index = 0;
        void* sprite = nullptr;
        std::uint8_t* texture_x = nullptr;
        std::uint8_t* texture_y = nullptr;
        std::uint8_t* texture_width = nullptr;
        std::uint8_t* texture_height = nullptr;
        Colour saved_colour{}, colour{};
        FloatRectangle saved_window{}, source{};
        const FloatRectangle* target = nullptr;
        float ratio = 0.0f;
    };
    struct Context {
        void* info_display;
        void* craft;
        const std::array<float, 4>* current;
        const std::array<float, 4>* maximum;
        std::uint32_t mode;
        void* text_component;
        const NativeRectangle* live_anchor;
        bool quad_ready = false, tri_ready = false;
        bool hemisphere_ready = false, hemisphere_side_ready = false;
        NativeRectangle graphic_rectangle{}, display_rectangle{};
        std::int32_t origin_x = 0, origin_y = 0;
        std::array<float, 4> ratios{};
        std::array<FloatRectangle, 4> positions{}, remapped_positions{};
        std::array<std::size_t, 4> remapped_indices{}, sprite_indices{};
        std::array<bool, 4> authored{}, original_authored{}, segment_drawn{};
        const DirectionalShieldUiPolicy* policy = nullptr;
        float graphic_width = 0.0f, graphic_height = 0.0f;
        bool drew = false;
        Segment segment{};
    } context{info_display, craft, &current, &maximum, shield_mode,
              text_component, &live_anchor};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        auto& segment = ctx.segment;
        if (action == 0) return ctx.craft && ctx.text_component;
        if (action == 1) return ctx.mode;
        if (action == 13) ctx.quad_ready = resolve_directional_shield_sprites();
        else if (action == 14) ctx.tri_ready = resolve_tri_shield_sprites();
        else if (action == 15) ctx.hemisphere_ready = resolve_hemisphere_shield_sprite();
        else if (action == 16) ctx.hemisphere_side_ready =
            resolve_hemisphere_side_shield_sprite();
        else if (action == 2) {
            ctx.graphic_rectangle = directional_shield_graphic_rectangle(
                *ctx.live_anchor);
            return usable_native_rectangle(ctx.graphic_rectangle);
        } else if (action == 3) {
            void* display_interface = read_at<void*>(
                ctx.text_component, 0x04, nullptr);
            if (!display_interface) return 0;
            ctx.origin_x = read_at<std::int32_t>(display_interface, 0x04, 0);
            ctx.origin_y = read_at<std::int32_t>(display_interface, 0x08, 0);
            ctx.display_rectangle = ctx.graphic_rectangle;
            ctx.display_rectangle.left += ctx.origin_x;
            ctx.display_rectangle.right += ctx.origin_x;
            ctx.display_rectangle.top += ctx.origin_y;
            ctx.display_rectangle.bottom += ctx.origin_y;
            return usable_native_rectangle(ctx.display_rectangle);
        } else if (action == 4) {
            ctx.ratios = animated_directional_shield_ratios(
                ctx.craft, *ctx.current, *ctx.maximum);
            ctx.positions = ctx.mode == 2u ? kDefaultTriForwardShieldPositions :
                ctx.mode == 3u ? kDefaultTriRearShieldPositions :
                ctx.mode == 4u ? kDefaultHemiForwardRearShieldPositions :
                ctx.mode == 5u ? kDefaultHemiPortStarboardShieldPositions :
                kDefaultDirectionalShieldPositions;
            void* object_class = read_at<void*>(
                ctx.craft, kObjectClassOffset, nullptr);
            const auto configured = g_directional_shield_ui_policies.find(object_class);
            ctx.policy = configured == g_directional_shield_ui_policies.end()
                ? nullptr : &configured->second;
        } else if (action == 5) {
            return ctx.policy && ctx.policy->position_found[index];
        } else if (action == 6) {
            ctx.authored[index] = true;
            const RawRectangle& configured = ctx.policy->positions[index];
            ctx.positions[index] = FloatRectangle{
                static_cast<float>(configured.x),
                static_cast<float>(configured.y),
                static_cast<float>(configured.width),
                static_cast<float>(configured.height)};
        } else if (action == 7) {
            if (ctx.mode != 1u ||
                !a2fo::craft_identity::remap_directional_shield_positions(
                    g_directional_shield_display_config, ctx.positions,
                    &ctx.remapped_positions, &ctx.remapped_indices)) return 0;
            ctx.positions = ctx.remapped_positions;
            ctx.original_authored = ctx.authored;
            return 1;
        } else if (action == 8) {
            ctx.authored[index] = ctx.original_authored[
                ctx.remapped_indices[index]];
        } else if (action == 9) {
            if (index == 0) {
                ctx.graphic_width = static_cast<float>(
                    ctx.graphic_rectangle.right - ctx.graphic_rectangle.left);
                ctx.graphic_height = static_cast<float>(
                    ctx.graphic_rectangle.bottom - ctx.graphic_rectangle.top);
            }
            ctx.sprite_indices[index] = directional_shield_sprite_index_for_destination(
                ctx.positions, index, 128.0f, 128.0f);
        } else if (action == 10) {
            a2fo::craft_identity::scale_directional_shield_default_positions(
                &ctx.positions, ctx.authored,
                ctx.graphic_width, ctx.graphic_height);
        } else if (action == 11) {
            return panel_style(kShieldSegments[index]).area_found;
        } else if (action == 12) {
            auto& target = ctx.positions[index];
            const auto r = panel_rectangle(kShieldSegments[index], {});
            target = {static_cast<float>(r.left - ctx.graphic_rectangle.left),
                static_cast<float>(r.top - ctx.graphic_rectangle.top),
                static_cast<float>(r.right - r.left),
                static_cast<float>(r.bottom - r.top)};
        } else if (action == 17) {
            return std::isfinite((*ctx.maximum)[index]) &&
                (*ctx.maximum)[index] > 0.0f;
        } else if (action == 18) {
            return !panel_style(kShieldSegments[index]).sprite.empty();
        } else if (action == 19) {
            const auto& target = ctx.positions[index];
            const NativeRectangle destination{
                ctx.graphic_rectangle.left + static_cast<std::int32_t>(target.x),
                ctx.graphic_rectangle.top + static_cast<std::int32_t>(target.y),
                ctx.graphic_rectangle.left + static_cast<std::int32_t>(target.x + target.width),
                ctx.graphic_rectangle.top + static_cast<std::int32_t>(target.y + target.height)};
            ctx.segment_drawn[index] = draw_ammunition_value_bar(destination,
                ctx.ratios[index], directional_shield_status_colour(ctx.ratios[index]),
                ctx.origin_x, ctx.origin_y, nullptr, kShieldSegments[index]);
            ctx.drew = ctx.segment_drawn[index] || ctx.drew;
        } else if (action == 20) {
            segment.compact = ctx.mode == 1u && g_directional_shield_sprites.compact;
            segment.tri = ctx.mode == 2u || ctx.mode == 3u;
            segment.hemisphere = ctx.mode == 4u || ctx.mode == 5u;
            return !((!segment.tri && !segment.hemisphere && !ctx.quad_ready) ||
                (segment.tri && !ctx.tri_ready) ||
                (ctx.mode == 4u && !ctx.hemisphere_ready) ||
                (ctx.mode == 5u && !ctx.hemisphere_side_ready));
        } else if (action == 21) {
            segment.physical_quad_index = ctx.sprite_indices[index];
            segment.sprite_index = segment.tri
                ? ((ctx.mode == 2u && index == 0u) ||
                   (ctx.mode == 3u && index == 1u) ? 0u : 1u)
                : segment.compact
                    ? (segment.physical_quad_index < 2u ? 0u : 1u)
                    : segment.physical_quad_index;
            segment.sprite = ctx.mode == 4u
                ? g_hemisphere_shield_sprite.sprite
                : ctx.mode == 5u
                    ? g_hemisphere_side_shield_sprite.sprite
                    : segment.tri
                        ? g_tri_shield_sprites.sprites[segment.sprite_index]
                        : g_directional_shield_sprites.sprites[segment.sprite_index];
            segment.texture_x = segment.sprite
                ? static_cast<std::uint8_t*>(segment.sprite) + kSpriteTextureXOffset : nullptr;
            segment.texture_y = segment.sprite
                ? static_cast<std::uint8_t*>(segment.sprite) + kSpriteTextureYOffset : nullptr;
            segment.texture_width = segment.sprite
                ? static_cast<std::uint8_t*>(segment.sprite) + kSpriteTextureWidthOffset : nullptr;
            segment.texture_height = segment.sprite
                ? static_cast<std::uint8_t*>(segment.sprite) + kSpriteTextureHeightOffset : nullptr;
            return segment.sprite && writable_range(
                    static_cast<std::uint8_t*>(segment.sprite) + kSpriteColourOffset,
                    sizeof(Colour)) &&
                writable_range(segment.texture_x, sizeof(float)) &&
                writable_range(segment.texture_y, sizeof(float)) &&
                writable_range(segment.texture_width, sizeof(float)) &&
                writable_range(segment.texture_height, sizeof(float));
        } else if (action == 22) {
            segment.saved_colour = read_at<Colour>(
                segment.sprite, kSpriteColourOffset, Colour{});
            segment.saved_window = FloatRectangle{
                read_at<float>(segment.sprite, kSpriteTextureXOffset, 0.0f),
                read_at<float>(segment.sprite, kSpriteTextureYOffset, 0.0f),
                read_at<float>(segment.sprite, kSpriteTextureWidthOffset, 0.0f),
                read_at<float>(segment.sprite, kSpriteTextureHeightOffset, 0.0f)};
            if (!usable_float_rectangle(segment.saved_window)) return 0;
            segment.source = (segment.compact || segment.tri || segment.hemisphere)
                ? FloatRectangle{0.0f, 0.0f, 128.0f, 128.0f}
                : kDirectionalShieldSourceBounds[segment.sprite_index];
            segment.target = &ctx.positions[index];
            return usable_float_rectangle(segment.source) &&
                usable_float_rectangle(*segment.target);
        } else if (action == 23) {
            segment.ratio = ctx.ratios[index];
            segment.colour_only =
                g_directional_shield_display_config.display_mode == 2;
            segment.colour = segment.colour_only && segment.ratio <= 0.0f
                ? Colour{0.0f, 0.0f, 0.0f}
                : panel_colour(kShieldSegments[index],
                    panel_colour(PanelElement::DirectionalShieldsGraphic,
                        directional_shield_status_colour(segment.ratio),
                        segment.ratio), segment.ratio);
            return segment.colour_only;
        } else if (action == 26) return segment.ratio > 0.0f;
        else if (action == 28) {
            a2fo_identity_fo_sprite_set_colour(
                at(g_fleet_ops, kFoSpriteSetColourRva), segment.sprite,
                &segment.saved_colour);
            std::memcpy(segment.texture_x, &segment.saved_window.x,
                        sizeof(segment.saved_window.x));
            std::memcpy(segment.texture_y, &segment.saved_window.y,
                        sizeof(segment.saved_window.y));
            std::memcpy(segment.texture_width, &segment.saved_window.width,
                        sizeof(segment.saved_window.width));
            std::memcpy(segment.texture_height, &segment.saved_window.height,
                        sizeof(segment.saved_window.height));
        } else if (action == 29) return ctx.drew;
        else if (action == 30) {
            update_directional_shield_tooltip_runtime(
                ctx.info_display, ctx.craft, *ctx.current, *ctx.maximum,
                ctx.display_rectangle, ctx.positions, ctx.segment_drawn);
        } else if (action == 31) {
            if (ctx.drew && InterlockedCompareExchange(
                    &g_directional_shields_graphic_report_count, 1, 0) == 0) {
                char message[640]{};
                std::snprintf(message, sizeof(message),
                    "First directional-shield ring draw submitted at local=%ld,%ld origin=%ld,%ld display=%ld,%ld: F=%.2f A=%.2f P=%.2f S=%.2f positions F=%.0f,%.0f,%.0f,%.0f A=%.0f,%.0f,%.0f,%.0f P=%.0f,%.0f,%.0f,%.0f S=%.0f,%.0f,%.0f,%.0f",
                    static_cast<long>(ctx.graphic_rectangle.left),
                    static_cast<long>(ctx.graphic_rectangle.top),
                    static_cast<long>(ctx.origin_x), static_cast<long>(ctx.origin_y),
                    static_cast<long>(ctx.display_rectangle.left),
                    static_cast<long>(ctx.display_rectangle.top),
                    static_cast<double>(ctx.ratios[0]),
                    static_cast<double>(ctx.ratios[1]),
                    static_cast<double>(ctx.ratios[2]),
                    static_cast<double>(ctx.ratios[3]),
                    static_cast<double>(ctx.positions[0].x),
                    static_cast<double>(ctx.positions[0].y),
                    static_cast<double>(ctx.positions[0].width),
                    static_cast<double>(ctx.positions[0].height),
                    static_cast<double>(ctx.positions[1].x),
                    static_cast<double>(ctx.positions[1].y),
                    static_cast<double>(ctx.positions[1].width),
                    static_cast<double>(ctx.positions[1].height),
                    static_cast<double>(ctx.positions[2].x),
                    static_cast<double>(ctx.positions[2].y),
                    static_cast<double>(ctx.positions[2].width),
                    static_cast<double>(ctx.positions[2].height),
                    static_cast<double>(ctx.positions[3].x),
                    static_cast<double>(ctx.positions[3].y),
                    static_cast<double>(ctx.positions[3].width),
                    static_cast<double>(ctx.positions[3].height));
                log_line(message);
            }
        } else if (action == 32) return ctx.drew;
        else if (action == 24 || action == 25 || action == 27) {
            const bool flip_x = (segment.compact && segment.physical_quad_index == 3u) ||
                (segment.tri && index == 3u) ||
                (ctx.mode == 5u && index == 3u);
            const bool flip_y = (segment.compact && segment.physical_quad_index == 1u) ||
                (ctx.mode == 3u && segment.tri) ||
                (ctx.mode == 4u && index == 1u);
            const auto draw_texture_window = [&](const FloatRectangle& source_window,
                const FloatRectangle& destination,
                const Colour& draw_colour) noexcept {
                if (!usable_float_rectangle(source_window) ||
                    !usable_float_rectangle(destination)) return false;
                float texture_x = segment.saved_window.x +
                    segment.saved_window.width *
                        (source_window.x / kDirectionalShieldTextureDimension);
                float texture_y = segment.saved_window.y +
                    segment.saved_window.height *
                        (source_window.y / kDirectionalShieldTextureDimension);
                float texture_width = segment.saved_window.width *
                    (source_window.width / kDirectionalShieldTextureDimension);
                float texture_height = segment.saved_window.height *
                    (source_window.height / kDirectionalShieldTextureDimension);
                if (!std::isfinite(texture_x) || !std::isfinite(texture_y) ||
                    !std::isfinite(texture_width) || texture_width == 0.0f ||
                    !std::isfinite(texture_height) || texture_height == 0.0f)
                    return false;
                if (flip_x) { texture_x += texture_width; texture_width = -texture_width; }
                if (flip_y) { texture_y += texture_height; texture_height = -texture_height; }
                std::memcpy(segment.texture_x, &texture_x, sizeof(texture_x));
                std::memcpy(segment.texture_y, &texture_y, sizeof(texture_y));
                std::memcpy(segment.texture_width, &texture_width, sizeof(texture_width));
                std::memcpy(segment.texture_height, &texture_height, sizeof(texture_height));
                const SpriteVector position{
                    static_cast<float>(ctx.display_rectangle.left) + destination.x,
                    static_cast<float>(ctx.display_rectangle.top) + destination.y,
                    0.0f};
                a2fo_identity_fo_sprite_set_colour(
                    at(g_fleet_ops, kFoSpriteSetColourRva), segment.sprite,
                    &draw_colour);
                a2fo_identity_fo_sprite_draw_scaled_2d(
                    at(g_fleet_ops, kFoSpriteDrawScaled2DRva), segment.sprite,
                    &position, destination.width, destination.height);
                return true;
            };
            if (action == 24) {
                ctx.segment_drawn[index] = draw_texture_window(
                    segment.source, *segment.target, segment.colour);
                ctx.drew = ctx.segment_drawn[index] || ctx.drew;
            } else if (action == 25) {
                const Colour depleted_colour = dimmed_colour(segment.colour);
                ctx.segment_drawn[index] = draw_texture_window(
                    segment.source, *segment.target, depleted_colour);
                ctx.drew = ctx.segment_drawn[index] || ctx.drew;
            } else {
                const auto fill =
                    a2fo::craft_identity::centered_directional_shield_fill(
                        segment.source, *segment.target, segment.ratio,
                        segment.compact
                            ? segment.physical_quad_index < 2u
                            : segment.tri
                                ? (index == 0u || index == 1u)
                                : ctx.mode == 4u
                                    ? true
                                    : ctx.mode == 5u
                                        ? false
                                        : segment.target->width >= segment.target->height);
                ctx.drew = draw_texture_window(fill.source, fill.destination,
                    segment.colour) || ctx.drew;
            }
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_draw_shield_graphic(step, &context, 4);
}

std::array<NativeRectangle, 4> directional_shield_value_rectangles(
    const NativeRectangle& live_anchor) noexcept {
    struct Context {
        const NativeRectangle* live_anchor;
        std::array<NativeRectangle, 4> rectangles{};
    } context{&live_anchor};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            const NativeRectangle graphic = directional_shield_graphic_rectangle(
                *ctx.live_anchor);
            const std::int32_t width = graphic.right - graphic.left;
            const std::int32_t height = graphic.bottom - graphic.top;
            const auto rectangle = [&](std::int32_t left, std::int32_t top,
                                       std::int32_t right,
                                       std::int32_t bottom) noexcept {
                return NativeRectangle{
                    graphic.left + width * left / 128,
                    graphic.top + height * top / 128,
                    graphic.left + width * right / 128,
                    graphic.top + height * bottom / 128};
            };
            ctx.rectangles = {{
                rectangle(32, 22, 96, 40),
                rectangle(32, 88, 96, 106),
                rectangle(20, 55, 64, 73),
                rectangle(64, 55, 108, 73),
            }};
        } else if (action == 1) {
            return g_ui_configuration.captain_rectangle_found;
        } else if (action == 2) {
            return g_ui_configuration.directional_value_rectangle_found[index];
        } else if (action == 3) {
            ctx.rectangles[index] = translated_rectangle(
                *ctx.live_anchor, g_ui_configuration.captain_rectangle,
                g_ui_configuration.directional_value_rectangles[index]);
        }
        return 0;
    };
    a2fo_rs_stage9_craft_value_rectangles(step, &context,
        context.rectangles.size());
    return context.rectangles;
}

void draw_directional_shield_rows(
    void* info_display, void* craft, void* text_component,
    const NativeRectangle& live_anchor,
    const Colour& shared_colour) noexcept {
    struct Context {
        void* info_display;
        void* craft;
        void* text_component;
        const NativeRectangle* live_anchor;
        const Colour* shared_colour;
        std::array<float, 4> current{}, maximum{};
        bool any_active = false;
        std::uint32_t shield_mode = 1;
        const DirectionalShieldUiPolicy* policy = nullptr;
        bool graphic_drawn = false;
        std::array<NativeRectangle, 4> rectangles{};
        std::array<bool, 4> drawn{};
        char value[64]{};
        NativeRectangle forward_aft_rectangle{}, port_starboard_rectangle{};
        Colour colour{};
        char forward_aft[160]{}, port_starboard[160]{};
        bool forward_aft_drawn = false, port_starboard_drawn = false;
    } context{info_display, craft, text_component, &live_anchor, &shared_colour};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        constexpr std::array<std::uint32_t, 4> facings{{
            A2FO_DIRECTIONAL_SHIELD_FORWARD, A2FO_DIRECTIONAL_SHIELD_AFT,
            A2FO_DIRECTIONAL_SHIELD_PORT, A2FO_DIRECTIONAL_SHIELD_STARBOARD}};
        if (action == 0) return ctx.craft && ctx.text_component &&
            resolve_directional_shields_api() &&
            g_directional_shields_is_enabled(ctx.craft);
        if (action == 1) {
            ctx.maximum[index] = g_directional_shields_get_maximum(
                ctx.craft, facings[index]);
            ctx.current[index] = g_directional_shields_get_current(
                ctx.craft, facings[index]);
            if (!std::isfinite(ctx.maximum[index]) || ctx.maximum[index] < 0.0f ||
                !std::isfinite(ctx.current[index]) || ctx.current[index] < 0.0f ||
                (ctx.maximum[index] == 0.0f && ctx.current[index] != 0.0f) ||
                (ctx.maximum[index] > 0.0f &&
                 ctx.current[index] > ctx.maximum[index])) return 0;
            ctx.any_active = ctx.any_active || ctx.maximum[index] > 0.0f;
            return 1;
        }
        if (action == 2) return ctx.any_active;
        if (action == 3) {
            ctx.shield_mode = g_directional_shields_get_mode
                ? g_directional_shields_get_mode(ctx.craft) : 1u;
            void* object_class = read_at<void*>(
                ctx.craft, kObjectClassOffset, nullptr);
            const auto configured_policy =
                g_directional_shield_ui_policies.find(object_class);
            ctx.policy = configured_policy == g_directional_shield_ui_policies.end()
                ? nullptr : &configured_policy->second;
        } else if (action == 4) {
            ctx.graphic_drawn = draw_directional_shield_graphic(
                ctx.info_display, ctx.craft, ctx.current, ctx.maximum,
                ctx.shield_mode, ctx.text_component, *ctx.live_anchor);
        } else if (action == 5) {
            if (ctx.policy && ctx.policy->value_display_mode_found) {
                return ctx.policy->value_display_mode ==
                    a2fo::craft_identity::DirectionalShieldValueDisplayMode::none ? 1 : 2;
            }
        } else if (action == 6) {
            ctx.rectangles = directional_shield_value_rectangles(*ctx.live_anchor);
        } else if (action == 7) {
            std::memset(ctx.value, 0, sizeof(ctx.value));
            return a2fo::craft_identity::format_directional_shield_value(
                ctx.policy->value_display_mode, ctx.current[index],
                ctx.maximum[index], ctx.value, sizeof(ctx.value));
        } else if (action == 8) {
            const float ratio = clamped_ratio(
                ctx.current[index], ctx.maximum[index]);
            const Colour colour = directional_shield_value_status_colour(
                ratio, *ctx.shared_colour);
            ctx.drawn[index] = draw_identity_text(ctx.value,
                panel_rectangle(kShieldValues[index], ctx.rectangles[index]),
                panel_colour(kShieldValues[index], colour,
                    clamped_ratio(ctx.current[index], ctx.maximum[index])),
                ctx.text_component);
        } else if (action == 9) {
            if (a2fo_rs_stage10_identity_any(
                    ctx.drawn.size(),
                    +[](void* opaque, std::uint32_t,
                        std::size_t item) noexcept -> std::uint32_t {
                        return (*static_cast<decltype(ctx.drawn)*>(opaque))[item];
                    }, &ctx.drawn) &&
                InterlockedCompareExchange(
                    &g_directional_shields_draw_report_count, 1, 0) == 0) {
                char message[384]{};
                std::snprintf(message, sizeof(message),
                    "First directional-shield value-label draw submitted: mode=%d F=%s A=%s P=%s S=%s",
                    static_cast<int>(ctx.policy->value_display_mode),
                    ctx.drawn[0] ? "yes" : "no", ctx.drawn[1] ? "yes" : "no",
                    ctx.drawn[2] ? "yes" : "no", ctx.drawn[3] ? "yes" : "no");
                log_line(message);
            }
        } else if (action == 10) return ctx.graphic_drawn;
        else if (action == 11) {
            ctx.forward_aft_rectangle = *ctx.live_anchor;
            ctx.port_starboard_rectangle = *ctx.live_anchor;
            ctx.forward_aft_rectangle.top += 112;
            ctx.forward_aft_rectangle.bottom += 112;
            ctx.port_starboard_rectangle.top += 132;
            ctx.port_starboard_rectangle.bottom += 132;
            if (g_ui_configuration.captain_rectangle_found &&
                g_ui_configuration.directional_forward_aft_rectangle_found) {
                ctx.forward_aft_rectangle = translated_rectangle(
                    *ctx.live_anchor, g_ui_configuration.captain_rectangle,
                    g_ui_configuration.directional_forward_aft_rectangle);
            }
            if (g_ui_configuration.captain_rectangle_found &&
                g_ui_configuration.directional_port_starboard_rectangle_found) {
                ctx.port_starboard_rectangle = translated_rectangle(
                    *ctx.live_anchor, g_ui_configuration.captain_rectangle,
                    g_ui_configuration.directional_port_starboard_rectangle);
            }
            ctx.colour = g_ui_configuration.directional_shield_colour_found
                ? g_ui_configuration.directional_shield_colour : *ctx.shared_colour;
        } else if (action == 12) return ctx.shield_mode;
        else if (action == 13) {
            std::snprintf(ctx.forward_aft, sizeof(ctx.forward_aft),
                "Shields  F %.0f/%.0f",
                static_cast<double>(ctx.current[0]),
                static_cast<double>(ctx.maximum[0]));
        } else if (action == 14) {
            std::snprintf(ctx.forward_aft, sizeof(ctx.forward_aft),
                "Shields  A %.0f/%.0f",
                static_cast<double>(ctx.current[1]),
                static_cast<double>(ctx.maximum[1]));
        } else if (action == 15) {
            std::snprintf(ctx.forward_aft, sizeof(ctx.forward_aft),
                "Shields  F %.0f/%.0f   A %.0f/%.0f",
                static_cast<double>(ctx.current[0]),
                static_cast<double>(ctx.maximum[0]),
                static_cast<double>(ctx.current[1]),
                static_cast<double>(ctx.maximum[1]));
        } else if (action == 16) {
            std::snprintf(ctx.port_starboard, sizeof(ctx.port_starboard),
                "         P %.0f/%.0f   S %.0f/%.0f",
                static_cast<double>(ctx.current[2]),
                static_cast<double>(ctx.maximum[2]),
                static_cast<double>(ctx.current[3]),
                static_cast<double>(ctx.maximum[3]));
        } else if (action == 17) {
            ctx.forward_aft_drawn = draw_identity_text(ctx.forward_aft,
                panel_rectangle(PanelElement::DirectionalShieldsForwardAftText,
                    ctx.forward_aft_rectangle),
                panel_colour(PanelElement::DirectionalShieldsForwardAftText,
                    ctx.colour), ctx.text_component);
        } else if (action == 18) {
            ctx.port_starboard_drawn = draw_identity_text(ctx.port_starboard,
                panel_rectangle(PanelElement::DirectionalShieldsPortStarboardText,
                    ctx.port_starboard_rectangle),
                panel_colour(PanelElement::DirectionalShieldsPortStarboardText,
                    ctx.colour), ctx.text_component);
        } else if (action == 19) {
            if ((ctx.forward_aft_drawn || ctx.port_starboard_drawn) &&
                InterlockedCompareExchange(
                    &g_directional_shields_draw_report_count, 1, 0) == 0) {
                char message[320]{};
                std::snprintf(message, sizeof(message),
                    "First directional-shield UI draw submitted: F/A=%s at %ld,%ld; P/S=%s at %ld,%ld",
                    ctx.forward_aft_drawn ? "yes" : "no",
                    static_cast<long>(ctx.forward_aft_rectangle.left),
                    static_cast<long>(ctx.forward_aft_rectangle.top),
                    ctx.port_starboard_drawn ? "yes" : "no",
                    static_cast<long>(ctx.port_starboard_rectangle.left),
                    static_cast<long>(ctx.port_starboard_rectangle.top));
                log_line(message);
            }
        }
        return 0;
    };
    a2fo_rs_stage9_craft_draw_shield_rows(step, &context, 4);
}

using InsertCString = void* (__cdecl*)(void*, const char*);

InsertCString insert_c_string() noexcept {
    void* function = read_at<void*>(
        at(g_armada, kInsertCStringIatRva), 0, nullptr);
    return reinterpret_cast<InsertCString>(function);
}

const char* localized_directional_shield_text(
    const char* key, const char* fallback) noexcept {
    if (!key || !fallback) return "";
    void* manager = read_at<void*>(
        at(g_armada, kLocalizationManagerPointerRva), 0, nullptr);
    if (!manager || !readable_range(
            at(g_armada, kLocalizationLookupRva), 1)) {
        return fallback;
    }
    const char* localized = reinterpret_cast<const char*>(
        a2fo_identity_call_thiscall_1(
            at(g_armada, kLocalizationLookupRva), manager,
            reinterpret_cast<std::uintptr_t>(key)));
    if (!localized || !*localized || std::strcmp(localized, key) == 0) {
        return fallback;
    }
    return localized;
}

bool format_directional_shield_tooltip(
    int facing, bool verbose, char* output,
    std::size_t output_size) noexcept {
    struct Context {
        int facing;
        bool verbose;
        char* output;
        std::size_t output_size;
        const char* presentation = nullptr;
        const char* strength = nullptr;
        int written = 0;
    } context{facing, verbose, output, output_size};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        const auto& runtime = g_directional_shield_tooltip;
        if (action == 0)
            return ctx.output && ctx.output_size != 0 &&
                ctx.facing >= 0 && ctx.facing < 4 && runtime.active;
        if (action == 1) {
            const std::size_t index = static_cast<std::size_t>(ctx.facing);
            ctx.presentation = localized_directional_shield_text(
                ctx.verbose ? kDirectionalShieldVerboseTooltipKeys[index]
                            : kDirectionalShieldTooltipKeys[index],
                ctx.verbose ? kDirectionalShieldVerboseTooltipFallbacks[index]
                            : kDirectionalShieldTooltipFallbacks[index]);
        } else if (action == 2) {
            ctx.strength = localized_directional_shield_text(
                kDirectionalShieldStrengthKey, kDirectionalShieldStrengthFallback);
        } else if (action == 3) {
            const std::size_t index = static_cast<std::size_t>(ctx.facing);
            ctx.written = ctx.verbose
                ? std::snprintf(ctx.output, ctx.output_size, "%s\n%s: %.0f / %.0f",
                    ctx.presentation, ctx.strength,
                    static_cast<double>(runtime.current[index]),
                    static_cast<double>(runtime.maximum[index]))
                : std::snprintf(ctx.output, ctx.output_size, "%s: %.0f / %.0f",
                    ctx.presentation,
                    static_cast<double>(runtime.current[index]),
                    static_cast<double>(runtime.maximum[index]));
        } else if (action == 4) {
            return ctx.written > 0 &&
                static_cast<std::size_t>(ctx.written) < ctx.output_size;
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_format_directional_tip(step, &context);
}

bool format_ammunition_tooltip(
    std::size_t store_index, bool verbose, char* output,
    std::size_t output_size) noexcept {
    static constexpr std::array<const char*, 3> default_tooltips{{
        "Photon Torpedo Ammunition",
        "Quantum Torpedo Ammunition",
        "Shuttle Craft",
    }};
    static constexpr std::array<const char*, 3> default_verbose_tooltips{{
        "Photon torpedo magazine and recharge reserve.",
        "Quantum torpedo magazine and recharge reserve.",
        "Embarked shuttlecraft complement and replenishment reserve.",
    }};
    struct Context {
        std::size_t index;
        bool verbose;
        char* output;
        std::size_t output_size;
        const AmmunitionTooltipRuntime* runtime;
        const AmmunitionPresentation* configured = nullptr;
        const std::string* configured_text = nullptr;
        const char* presentation = nullptr;
    } context{store_index, verbose, output, output_size, &g_ammunition_tooltip,
              nullptr, nullptr, nullptr};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) return ctx.output && ctx.output_size != 0 &&
            ctx.index < ctx.runtime->visible.size() && ctx.runtime->active &&
            ctx.runtime->visible[ctx.index] && ctx.runtime->craft;
        if (action == 1) {
            const AmmunitionUiPolicy* policy = ammunition_ui_policy(ctx.runtime->craft);
            ctx.configured = policy ? &policy->stores[ctx.index] : nullptr;
        } else if (action == 2) {
            if (ctx.configured && ctx.verbose && !ctx.configured->verbose_tooltip.empty()) {
                ctx.configured_text = &ctx.configured->verbose_tooltip;
                return 1;
            }
            return 0;
        } else if (action == 3) {
            if (ctx.configured && !ctx.configured->tooltip.empty())
                ctx.configured_text = &ctx.configured->tooltip;
        } else if (action == 4) {
            const char* fallback = ctx.verbose
                ? default_verbose_tooltips[ctx.index] : default_tooltips[ctx.index];
            const char* key = ctx.configured_text ? ctx.configured_text->c_str() : fallback;
            ctx.presentation = localized_directional_shield_text(key, key);
        } else if (action == 5) {
            const int written = ctx.verbose
                ? std::snprintf(ctx.output, ctx.output_size, "%s\nAmmunition: %d/%d",
                    ctx.presentation,
                    whole_ammunition_amount(ctx.runtime->current[ctx.index]),
                    whole_ammunition_amount(ctx.runtime->maximum[ctx.index]))
                : std::snprintf(ctx.output, ctx.output_size, "%s: %d/%d",
                    ctx.presentation,
                    whole_ammunition_amount(ctx.runtime->current[ctx.index]),
                    whole_ammunition_amount(ctx.runtime->maximum[ctx.index]));
            return written > 0 && static_cast<std::size_t>(written) < ctx.output_size;
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_format_ammunition_tip(step, &context);
}

bool format_selected_status_tooltip(
    std::size_t status_index, bool verbose, char* output,
    std::size_t output_size) noexcept {
    struct Context {
        std::size_t index;
        bool verbose;
        char* output;
        std::size_t output_size;
        const SelectedStatusTooltipRuntime* runtime;
        const SelectedStatusUiPolicy* policy = nullptr;
        const std::string* configured = nullptr;
        const char* presentation = nullptr;
    } context{status_index, verbose, output, output_size,
              &g_selected_status_tooltip, nullptr, nullptr, nullptr};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) return ctx.output && ctx.output_size != 0 && ctx.index < 2 &&
            ctx.runtime->active && ctx.runtime->visible[ctx.index] && ctx.runtime->craft;
        if (action == 1) return ctx.index == static_cast<std::size_t>(SelectedStatusIndex::shields);
        if (action == 2) {
            void* object_class = read_at<void*>(ctx.runtime->craft, kObjectClassOffset, nullptr);
            const auto found = g_selected_status_ui_policies.find(object_class);
            if (found != g_selected_status_ui_policies.end()) ctx.policy = &found->second;
        } else if (action == 3) {
            if (ctx.policy && ctx.verbose && !ctx.policy->shield_verbose_tooltip.empty()) {
                ctx.configured = &ctx.policy->shield_verbose_tooltip;
                return 1;
            }
            return 0;
        } else if (action == 4) {
            if (ctx.policy && !ctx.policy->shield_tooltip.empty())
                ctx.configured = &ctx.policy->shield_tooltip;
        } else if (action == 5) {
            const char* default_key = ctx.verbose
                ? "GUI_SD_SHIELD_VTOOLTIP" : "GUI_SD_SHIELD_TOOLTIP";
            const char* key = ctx.configured ? ctx.configured->c_str() : default_key;
            ctx.presentation = localized_directional_shield_text(
                key, ctx.configured ? key : "Shield Integrity at");
        } else if (action == 6) {
            const int percentage = static_cast<int>(std::lround(
                100.0f * clamped_ratio(ctx.runtime->current[ctx.index],
                                        ctx.runtime->maximum[ctx.index])));
            // Armada's second formatting pass consumes two percent bytes as one.
            const int written = std::snprintf(ctx.output, ctx.output_size,
                "%s %d%%%% %.0f/%.0f", ctx.presentation, percentage,
                static_cast<double>(ctx.runtime->current[ctx.index]),
                static_cast<double>(ctx.runtime->maximum[ctx.index]));
            return written > 0 && static_cast<std::size_t>(written) < ctx.output_size;
        } else if (action == 7) {
            const char* key = ctx.verbose
                ? "GUI_SD_EXPERIENCE_VTOOLTIP" : "GUI_SD_EXPERIENCE_TOOLTIP";
            const char* fallback = ctx.verbose
                ? "Experience progress toward the vessel's next rank." : "Experience";
            ctx.presentation = localized_directional_shield_text(key, fallback);
        } else if (action == 8) {
            const int written = ctx.verbose
                ? std::snprintf(ctx.output, ctx.output_size, "%s\nExperience: %.0f / %.0f",
                    ctx.presentation,
                    static_cast<double>(ctx.runtime->current[ctx.index]),
                    static_cast<double>(ctx.runtime->maximum[ctx.index]))
                : std::snprintf(ctx.output, ctx.output_size, "%s: %.0f / %.0f",
                    ctx.presentation,
                    static_cast<double>(ctx.runtime->current[ctx.index]),
                    static_cast<double>(ctx.runtime->maximum[ctx.index]));
            return written > 0 && static_cast<std::size_t>(written) < ctx.output_size;
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_format_selected_status_tip(step, &context);
}

bool configure_directional_shield_native_tooltip_provider(
    int facing) noexcept {
    struct Context { int facing; char normal[768]{}, verbose[768]{}; } context{facing};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        auto& provider = g_directional_shield_native_tooltip_provider;
        if (action == 0) return format_directional_shield_tooltip(
            ctx.facing, false, ctx.normal, sizeof(ctx.normal));
        if (action == 1) return format_directional_shield_tooltip(
            ctx.facing, true, ctx.verbose, sizeof(ctx.verbose));
        if (action == 2) {
            provider.vtable = at(g_armada, kStandardComponentVtableRva);
            std::snprintf(provider.normal_storage.data(),
                provider.normal_storage.size(), "%s", ctx.normal);
            std::snprintf(provider.verbose_storage.data(),
                provider.verbose_storage.size(), "%s", ctx.verbose);
            provider.normal = provider.normal_storage.data();
            provider.verbose = provider.verbose_storage.data();
        } else if (action == 3) return readable_range(provider.vtable, 0x08);
        return 0;
    };
    return a2fo_rs_stage9_craft_configure_native_tip(step, &context);
}

void draw_directional_shield_hover_tooltip(
    void* info_display, void* craft, void* text_component,
    const NativeRectangle& live_anchor,
    const Colour& shared_colour) noexcept {
    struct Context {
        void* info_display;
        void* craft;
        void* text_component;
        const NativeRectangle* live_anchor;
        const Colour* shared_colour;
        std::int32_t cursor_x = std::numeric_limits<std::int32_t>::min();
        std::int32_t cursor_y = std::numeric_limits<std::int32_t>::min();
        std::int32_t tooltip_mode = 0;
        int facing = -1;
        bool verbose = false;
        bool drawn = false;
        char text[768]{};
        NativeRectangle tooltip_rectangle{};
    } context{info_display, craft, text_component, &live_anchor, &shared_colour};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        const auto& runtime = g_directional_shield_tooltip;
        if (action == 0) return !g_directional_shield_wireframe_tooltips_available;
        if (action == 1) {
            return g_runtime_ready && ctx.info_display && ctx.craft &&
                ctx.text_component && runtime.active &&
                runtime.info_display == ctx.info_display &&
                runtime.craft == ctx.craft &&
                read_at<void*>(ctx.info_display,
                    kInfoDisplaySelectedCraftOffset, nullptr) == ctx.craft;
        }
        if (action == 2) {
            ctx.cursor_x = read_at<std::int32_t>(
                at(g_armada, kCursorXRva), 0, ctx.cursor_x);
            ctx.cursor_y = read_at<std::int32_t>(
                at(g_armada, kCursorYRva), 0, ctx.cursor_y);
            ctx.facing = a2fo::craft_identity::directional_shield_segment_at(
                runtime.hit_rectangles,
                static_cast<float>(ctx.cursor_x), static_cast<float>(ctx.cursor_y));
        } else if (action == 3) {
            return ctx.facing >= 0 && ctx.facing < 4;
        } else if (action == 4) {
            ctx.tooltip_mode = read_at<std::int32_t>(
                at(g_armada, kTooltipModeRva), 0, 0);
            ctx.verbose = ctx.tooltip_mode == 1;
            return format_directional_shield_tooltip(
                ctx.facing, ctx.verbose, ctx.text, sizeof(ctx.text));
        } else if (action == 5) {
            ctx.tooltip_rectangle = *ctx.live_anchor;
            ctx.tooltip_rectangle.top += 108;
            ctx.tooltip_rectangle.bottom += 108;
            if (g_ui_configuration.captain_rectangle_found &&
                g_ui_configuration.directional_forward_aft_rectangle_found) {
                ctx.tooltip_rectangle = translated_rectangle(
                    *ctx.live_anchor, g_ui_configuration.captain_rectangle,
                    g_ui_configuration.directional_forward_aft_rectangle);
            }
            ctx.tooltip_rectangle.right = ctx.tooltip_rectangle.left + 520;
            if (ctx.verbose) ctx.tooltip_rectangle.top -= 20;
            ctx.tooltip_rectangle.bottom = ctx.tooltip_rectangle.top +
                (ctx.verbose ? 58 : 24);
        } else if (action == 6) {
            const Colour shadow_colour{0.02f, 0.02f, 0.02f};
            NativeRectangle shadow_rectangle = ctx.tooltip_rectangle;
            ++shadow_rectangle.left;
            ++shadow_rectangle.right;
            ++shadow_rectangle.top;
            ++shadow_rectangle.bottom;
            draw_identity_text(ctx.text, shadow_rectangle,
                shadow_colour, ctx.text_component);
        } else if (action == 7) {
            ctx.drawn = draw_identity_text(ctx.text, ctx.tooltip_rectangle,
                *ctx.shared_colour, ctx.text_component);
        } else if (action == 8) {
            if (ctx.drawn && InterlockedCompareExchange(
                    &g_directional_shield_direct_tooltip_report_count,
                    1, 0) == 0) {
                char message[320]{};
                std::snprintf(message, sizeof(message),
                    "First direct directional-shield tooltip draw submitted: facing=%d mode=%ld cursor=%ld,%ld rectangle=%ld,%ld,%ld,%ld text='%s'",
                    ctx.facing, static_cast<long>(ctx.tooltip_mode),
                    static_cast<long>(ctx.cursor_x), static_cast<long>(ctx.cursor_y),
                    static_cast<long>(ctx.tooltip_rectangle.left),
                    static_cast<long>(ctx.tooltip_rectangle.top),
                    static_cast<long>(ctx.tooltip_rectangle.right),
                    static_cast<long>(ctx.tooltip_rectangle.bottom), ctx.text);
                log_line(message);
            }
        }
        return 0;
    };
    a2fo_rs_stage9_craft_draw_directional_tip(step, &context);
}

void set_standard_component_text(
    void* component, std::uintptr_t setter_rva,
    const char* text) noexcept {
    if (!g_directional_shield_component_tooltips_available || !component ||
        !readable_range(component, 0x28) ||
        !readable_range(at(g_armada, setter_rva), 1)) {
        return;
    }
    a2fo_identity_call_thiscall_1(
        at(g_armada, setter_rva), component,
        reinterpret_cast<std::uintptr_t>(text));
}

void clear_directional_shield_native_tooltip() noexcept {
    const auto step = +[](void*, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        if (action == 0) return g_directional_shield_native_tooltip.presented &&
            g_directional_shield_native_tooltip_available;
        if (action == 1) {
            a2fo_identity_call_thiscall_0(
                at(g_armada, kTooltipManagerClearRva),
                at(g_armada, kTooltipManagerObjectRva));
        } else if (action == 2) g_directional_shield_native_tooltip = {};
        return 0;
    };
    a2fo_rs_stage9_craft_clear_native_tip(step, nullptr);
}

void present_directional_shield_native_tooltip(
    void* component, void* craft, int facing) noexcept {
    struct Context {
        void* component;
        void* craft;
        int facing;
        DWORD now = 0;
        void* owner = nullptr;
        bool presented = false;
    } context{component, craft, facing};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        auto& native = g_directional_shield_native_tooltip;
        if (action == 0) {
            return g_directional_shield_native_tooltip_available &&
                ctx.component && ctx.craft && ctx.facing >= 0 &&
                ctx.facing < 4 && readable_range(ctx.component, 0x08);
        }
        if (action == 1) clear_directional_shield_native_tooltip();
        else if (action == 2) ctx.now = GetTickCount();
        else if (action == 3) {
            return native.component != ctx.component || native.craft != ctx.craft ||
                native.facing != ctx.facing;
        } else if (action == 4) {
            clear_directional_shield_native_tooltip();
            native.component = ctx.component;
            native.craft = ctx.craft;
            native.facing = ctx.facing;
            native.hover_started = ctx.now;
        } else if (action == 5) {
            ctx.owner = read_at<void*>(ctx.component, 0x04, nullptr);
            void* standard_component_vtable =
                at(g_armada, kStandardComponentVtableRva);
            return ctx.owner && readable_range(standard_component_vtable, 0x08) &&
                g_directional_shield_native_tooltip_provider.vtable ==
                    standard_component_vtable &&
                g_directional_shield_native_tooltip_provider.normal &&
                g_directional_shield_native_tooltip_provider.verbose;
        } else if (action == 6) {
            // Keep the native manager's proven completed-hover age bits.
            constexpr float elapsed = 3600.0f;
            std::uint32_t elapsed_bits = 0;
            std::memcpy(&elapsed_bits, &elapsed, sizeof(elapsed_bits));
            ctx.presented = (a2fo_identity_call_thiscall_3(
                at(g_armada, kTooltipManagerShowRva),
                at(g_armada, kTooltipManagerObjectRva), elapsed_bits,
                reinterpret_cast<std::uintptr_t>(
                    &g_directional_shield_native_tooltip_provider),
                reinterpret_cast<std::uintptr_t>(ctx.owner)) & 0xffu) != 0;
            native.presented = ctx.presented;
        } else if (action == 7) {
            if (ctx.presented && InterlockedCompareExchange(
                    &g_directional_shield_native_dispatch_report_count,
                    1, 0) == 0) {
                char message[224]{};
                std::snprintf(message, sizeof(message),
                    "First directional-shield tooltip presented by Armada's native manager: facing=%d provider=%p owner=%p elapsed=%.2f normal='%s'",
                    ctx.facing,
                    static_cast<void*>(
                        &g_directional_shield_native_tooltip_provider),
                    ctx.owner, static_cast<double>(3600.0f),
                    g_directional_shield_native_tooltip_provider.normal);
                log_line(message);
            }
        }
        return 0;
    };
    a2fo_rs_stage9_craft_present_native_tip(step, &context);
}

void update_directional_shield_component_tooltip(
    void* info_display) noexcept {
    struct Context {
        void* info_display;
        void* component = nullptr;
        int facing = -1;
        std::int32_t cursor_x = std::numeric_limits<std::int32_t>::min();
        std::int32_t cursor_y = std::numeric_limits<std::int32_t>::min();
        char normal[768]{}, verbose[768]{};
    } context{info_display};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        auto& binding = g_directional_shield_tooltip_binding;
        const auto& runtime = g_directional_shield_tooltip;
        if (action == 0)
            return g_directional_shield_component_tooltips_available && ctx.info_display;
        if (action == 1) {
            ctx.component = read_at<void*>(
                ctx.info_display, kInfoDisplayWireframeOffset, nullptr);
            if (!ctx.component || !readable_range(ctx.component, 0x28)) {
                binding = {};
                return 0;
            }
            return 1;
        }
        if (action == 2) {
            if (binding.info_display != ctx.info_display ||
                binding.component != ctx.component) {
                // The discarded component is never dereferenced to clear old text.
                binding = {};
                binding.info_display = ctx.info_display;
                binding.component = ctx.component;
            }
        } else if (action == 3) {
            if (runtime.active && runtime.info_display == ctx.info_display &&
                runtime.craft &&
                read_at<void*>(ctx.info_display,
                    kInfoDisplaySelectedCraftOffset, nullptr) == runtime.craft) {
                ctx.cursor_x = read_at<std::int32_t>(
                    at(g_armada, kCursorXRva), 0, ctx.cursor_x);
                ctx.cursor_y = read_at<std::int32_t>(
                    at(g_armada, kCursorYRva), 0, ctx.cursor_y);
                ctx.facing = a2fo::craft_identity::directional_shield_segment_at(
                    runtime.hit_rectangles,
                    static_cast<float>(ctx.cursor_x),
                    static_cast<float>(ctx.cursor_y));
            }
        } else if (action == 4) {
            return ctx.facing >= 0 && ctx.facing < 4;
        } else if (action == 5) {
            if (binding.facing >= 0) {
                set_standard_component_text(
                    ctx.component, kStandardComponentSetTooltipTextRva, nullptr);
                set_standard_component_text(
                    ctx.component, kStandardComponentSetVerboseTooltipTextRva, nullptr);
            }
        } else if (action == 6) {
            binding.craft = runtime.craft;
            binding.facing = -1;
            binding.current = -1.0f;
            binding.maximum = -1.0f;
        } else if (action == 7) {
            const std::size_t index = static_cast<std::size_t>(ctx.facing);
            return binding.craft == runtime.craft && binding.facing == ctx.facing &&
                binding.current == runtime.current[index] &&
                binding.maximum == runtime.maximum[index];
        } else if (action == 8) {
            return format_directional_shield_tooltip(
                ctx.facing, false, ctx.normal, sizeof(ctx.normal));
        } else if (action == 9) {
            return format_directional_shield_tooltip(
                ctx.facing, true, ctx.verbose, sizeof(ctx.verbose));
        } else if (action == 10) {
            set_standard_component_text(
                ctx.component, kStandardComponentSetTooltipTextRva, ctx.normal);
        } else if (action == 11) {
            set_standard_component_text(
                ctx.component, kStandardComponentSetVerboseTooltipTextRva,
                ctx.verbose);
        } else if (action == 12) {
            const std::size_t index = static_cast<std::size_t>(ctx.facing);
            binding.craft = runtime.craft;
            binding.facing = ctx.facing;
            binding.current = runtime.current[index];
            binding.maximum = runtime.maximum[index];
            if (InterlockedCompareExchange(
                    &g_directional_shield_component_assignment_report_count,
                    1, 0) == 0) {
                char message[224]{};
                std::snprintf(message, sizeof(message),
                    "First directional-shield component tooltip assigned: facing=%lu cursor=%ld,%ld component=%p",
                    static_cast<unsigned long>(index),
                    static_cast<long>(ctx.cursor_x), static_cast<long>(ctx.cursor_y),
                    ctx.component);
                log_line(message);
            }
        }
        return 0;
    };
    a2fo_rs_stage9_craft_component_tooltip(step, &context);
}

bool append_directional_shield_tooltip(
    void* stream_wrapper, bool verbose) noexcept {
    struct Context {
        void* stream_wrapper;
        bool verbose;
        std::int32_t cursor_x = std::numeric_limits<std::int32_t>::min();
        std::int32_t cursor_y = std::numeric_limits<std::int32_t>::min();
        int facing = -1;
        char text[768]{};
        InsertCString insert = nullptr;
    } context{stream_wrapper, verbose};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        const auto& runtime = g_directional_shield_tooltip;
        if (action == 0) {
            return g_runtime_ready && runtime.active && runtime.info_display &&
                runtime.craft && ctx.stream_wrapper &&
                read_at<void*>(runtime.info_display,
                    kInfoDisplaySelectedCraftOffset, nullptr) == runtime.craft;
        }
        if (action == 1) {
            ctx.cursor_x = read_at<std::int32_t>(
                at(g_armada, kCursorXRva), 0, ctx.cursor_x);
            ctx.cursor_y = read_at<std::int32_t>(
                at(g_armada, kCursorYRva), 0, ctx.cursor_y);
            ctx.facing = a2fo::craft_identity::directional_shield_segment_at(
                runtime.hit_rectangles,
                static_cast<float>(ctx.cursor_x), static_cast<float>(ctx.cursor_y));
        } else if (action == 2) {
            if (InterlockedCompareExchange(
                    &g_directional_shield_tooltip_query_report_count, 1, 0) == 0) {
                char message[640]{};
                std::snprintf(message, sizeof(message),
                    "First directional-shield tooltip query: cursor=%ld,%ld facing=%d; F=%.0f,%.0f,%.0f,%.0f A=%.0f,%.0f,%.0f,%.0f P=%.0f,%.0f,%.0f,%.0f S=%.0f,%.0f,%.0f,%.0f",
                    static_cast<long>(ctx.cursor_x), static_cast<long>(ctx.cursor_y), ctx.facing,
                    static_cast<double>(runtime.hit_rectangles[0].x),
                    static_cast<double>(runtime.hit_rectangles[0].y),
                    static_cast<double>(runtime.hit_rectangles[0].width),
                    static_cast<double>(runtime.hit_rectangles[0].height),
                    static_cast<double>(runtime.hit_rectangles[1].x),
                    static_cast<double>(runtime.hit_rectangles[1].y),
                    static_cast<double>(runtime.hit_rectangles[1].width),
                    static_cast<double>(runtime.hit_rectangles[1].height),
                    static_cast<double>(runtime.hit_rectangles[2].x),
                    static_cast<double>(runtime.hit_rectangles[2].y),
                    static_cast<double>(runtime.hit_rectangles[2].width),
                    static_cast<double>(runtime.hit_rectangles[2].height),
                    static_cast<double>(runtime.hit_rectangles[3].x),
                    static_cast<double>(runtime.hit_rectangles[3].y),
                    static_cast<double>(runtime.hit_rectangles[3].width),
                    static_cast<double>(runtime.hit_rectangles[3].height));
                log_line(message);
            }
        } else if (action == 3) {
            return ctx.facing >= 0 && ctx.facing < 4;
        } else if (action == 4) {
            const std::size_t index = static_cast<std::size_t>(ctx.facing);
            if (InterlockedCompareExchange(
                    &g_directional_shield_tooltip_hit_report_count, 1, 0) == 0) {
                char message[192]{};
                std::snprintf(message, sizeof(message),
                    "First directional-shield tooltip hit: facing=%lu cursor=%ld,%ld",
                    static_cast<unsigned long>(index),
                    static_cast<long>(ctx.cursor_x), static_cast<long>(ctx.cursor_y));
                log_line(message);
            }
        } else if (action == 5) {
            return format_directional_shield_tooltip(
                ctx.facing, ctx.verbose, ctx.text, sizeof(ctx.text));
        } else if (action == 6) {
            ctx.insert = insert_c_string();
            return ctx.insert && readable_range(ctx.stream_wrapper, 0x0c);
        } else if (action == 7) {
            // Armada's ostream subobject begins at +8 in this wrapper.
            ctx.insert(static_cast<std::uint8_t*>(ctx.stream_wrapper) + 0x08,
                ctx.text);
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_append_directional_tip(step, &context);
}

[[maybe_unused]] void __attribute__((fastcall)) wireframe_tooltip_hook(
    void* wireframe_icon, void*, void* stream_wrapper) noexcept {
    if (!append_directional_shield_tooltip(stream_wrapper, false) &&
        g_wireframe_tooltip_hook.gateway) {
        a2fo_identity_call_thiscall_1(
            g_wireframe_tooltip_hook.gateway, wireframe_icon,
            reinterpret_cast<std::uintptr_t>(stream_wrapper));
    }
}

[[maybe_unused]] void __attribute__((fastcall)) wireframe_verbose_tooltip_hook(
    void* wireframe_icon, void*, void* stream_wrapper) noexcept {
    if (!append_directional_shield_tooltip(stream_wrapper, true) &&
        g_wireframe_verbose_tooltip_hook.gateway) {
        a2fo_identity_call_thiscall_1(
            g_wireframe_verbose_tooltip_hook.gateway, wireframe_icon,
            reinterpret_cast<std::uintptr_t>(stream_wrapper));
    }
}

bool apply_directional_shield_hover_rectangle(
    void* wireframe_icon, NativeRectangle* saved_rectangle) noexcept {
    struct Context {
        void* wireframe_icon;
        NativeRectangle* saved_rectangle;
        std::int32_t cursor_x = std::numeric_limits<std::int32_t>::min();
        std::int32_t cursor_y = std::numeric_limits<std::int32_t>::min();
        int facing = -1;
        void* owner = nullptr;
        void* owner_vtable = nullptr;
        void* get_owner_rectangle = nullptr;
        std::int32_t owner_x = 0, owner_y = 0;
        NativeRectangle hover_rectangle{};
    } context{wireframe_icon, saved_rectangle};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        const auto& runtime = g_directional_shield_tooltip;
        if (action == 0) {
            return ctx.wireframe_icon && ctx.saved_rectangle && runtime.active &&
                runtime.info_display && runtime.craft &&
                read_at<void*>(runtime.info_display,
                    kInfoDisplayWireframeOffset, nullptr) == ctx.wireframe_icon &&
                read_at<void*>(runtime.info_display,
                    kInfoDisplaySelectedCraftOffset, nullptr) == runtime.craft &&
                readable_range(ctx.wireframe_icon, 0x28);
        }
        if (action == 1) {
            ctx.cursor_x = read_at<std::int32_t>(
                at(g_armada, kCursorXRva), 0, ctx.cursor_x);
            ctx.cursor_y = read_at<std::int32_t>(
                at(g_armada, kCursorYRva), 0, ctx.cursor_y);
            ctx.facing = a2fo::craft_identity::directional_shield_segment_at(
                runtime.hit_rectangles,
                static_cast<float>(ctx.cursor_x),
                static_cast<float>(ctx.cursor_y));
        } else if (action == 2) {
            return ctx.facing >= 0 && ctx.facing < 4;
        } else if (action == 3) {
            ctx.owner = read_at<void*>(ctx.wireframe_icon, 0x04, nullptr);
            ctx.owner_vtable = read_at<void*>(ctx.owner, 0, nullptr);
            ctx.get_owner_rectangle = read_at<void*>(
                ctx.owner_vtable, 0x64, nullptr);
            return ctx.owner && ctx.owner_vtable && ctx.get_owner_rectangle;
        } else if (action == 4) {
            const void* owner_rectangle = reinterpret_cast<const void*>(
                a2fo_identity_call_thiscall_0(ctx.get_owner_rectangle, ctx.owner));
            if (!readable_range(owner_rectangle, 0x08)) return 0;
            ctx.owner_x = read_at<std::int32_t>(owner_rectangle, 0, 0);
            ctx.owner_y = read_at<std::int32_t>(owner_rectangle, 4, 0);
            return 1;
        } else if (action == 5) {
            *ctx.saved_rectangle = read_at<NativeRectangle>(
                ctx.wireframe_icon, 0x08, NativeRectangle{});
            const FloatRectangle& hit = runtime.hit_rectangles[
                static_cast<std::size_t>(ctx.facing)];
            ctx.hover_rectangle = NativeRectangle{
                static_cast<std::int32_t>(std::floor(hit.x)) - ctx.owner_x,
                static_cast<std::int32_t>(std::floor(hit.y)) - ctx.owner_y,
                static_cast<std::int32_t>(std::ceil(hit.x + hit.width)) -
                    ctx.owner_x - 1,
                static_cast<std::int32_t>(std::ceil(hit.y + hit.height)) -
                    ctx.owner_y - 1};
            std::memcpy(static_cast<std::uint8_t*>(ctx.wireframe_icon) + 0x08,
                &ctx.hover_rectangle, sizeof(ctx.hover_rectangle));
        } else if (action == 6) {
            if (InterlockedCompareExchange(
                    &g_directional_shield_hover_rectangle_report_count,
                    1, 0) == 0) {
                char message[320]{};
                std::snprintf(message, sizeof(message),
                    "First directional-shield arc routed through WireframeIcon's native hover rectangle: facing=%d cursor=%ld,%ld ownerOrigin=%ld,%ld local=%ld,%ld,%ld,%ld",
                    ctx.facing, static_cast<long>(ctx.cursor_x),
                    static_cast<long>(ctx.cursor_y), static_cast<long>(ctx.owner_x),
                    static_cast<long>(ctx.owner_y),
                    static_cast<long>(ctx.hover_rectangle.left),
                    static_cast<long>(ctx.hover_rectangle.top),
                    static_cast<long>(ctx.hover_rectangle.right),
                    static_cast<long>(ctx.hover_rectangle.bottom));
                log_line(message);
            }
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_apply_hover_rectangle(step, &context);
}

[[maybe_unused]] void __attribute__((fastcall)) wireframe_update_hook(
    void* wireframe_icon, void*) noexcept {
    struct Context {
        void* wireframe_icon;
        NativeRectangle saved_rectangle{};
        bool overridden = false;
    } context{wireframe_icon};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            ctx.overridden = apply_directional_shield_hover_rectangle(
                ctx.wireframe_icon, &ctx.saved_rectangle);
        } else if (action == 1) {
            if (g_wireframe_update_original)
                a2fo_identity_call_thiscall_0(
                    g_wireframe_update_original, ctx.wireframe_icon);
        } else if (action == 2) {
            return ctx.overridden && readable_range(ctx.wireframe_icon, 0x18);
        } else if (action == 3) {
            std::memcpy(static_cast<std::uint8_t*>(ctx.wireframe_icon) + 0x08,
                &ctx.saved_rectangle, sizeof(ctx.saved_rectangle));
        }
        return 0;
    };
    a2fo_rs_stage9_craft_wireframe_update(step, &context);
}

void reset_directional_shield_hover_components() noexcept {
    const auto step = +[](void*, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        if (action == 0)
            return g_directional_shield_hover_components.components[index].presented != 0;
        if (action == 1 && readable_range(
                at(g_armada, kTooltipManagerClearRva), 1)) {
            a2fo_identity_call_thiscall_0(
                at(g_armada, kTooltipManagerClearRva),
                at(g_armada, kTooltipManagerObjectRva));
        } else if (action == 2) g_directional_shield_hover_components = {};
        return 0;
    };
    a2fo_rs_stage9_craft_reset_hover(step, nullptr,
        g_directional_shield_hover_components.components.size());
}

void update_directional_shield_hover_components(
    void* info_display) noexcept {
    const DirectionalShieldTooltipRuntime& runtime =
        g_directional_shield_tooltip;
    if (!g_directional_shield_wireframe_tooltips_available ||
        !info_display || !runtime.active ||
        runtime.info_display != info_display || !runtime.craft ||
        read_at<void*>(info_display,
                       kInfoDisplaySelectedCraftOffset, nullptr) !=
            runtime.craft) {
        reset_directional_shield_hover_components();
        return;
    }

    void* wireframe_icon = read_at<void*>(
        info_display, kInfoDisplayWireframeOffset, nullptr);
    // StandardComponent::Update only performs its hit test when component
    // +0x04 exactly matches Armada's current active interface owner. The
    // WireframeIcon may retain a nested SelectionDisplay owner in Fleet Ops,
    // so synthetic arc components must use the live global owner directly.
    void* owner = read_at<void*>(
        at(g_armada, kActiveInterfaceOwnerRva), 0, nullptr);
    if (!wireframe_icon || !owner) {
        reset_directional_shield_hover_components();
        return;
    }
    void* owner_vtable = read_at<void*>(owner, 0, nullptr);
    void* get_owner_rectangle = read_at<void*>(
        owner_vtable, 0x64, nullptr);
    if (!owner_vtable || !get_owner_rectangle) {
        reset_directional_shield_hover_components();
        return;
    }
    const void* owner_rectangle = reinterpret_cast<const void*>(
        a2fo_identity_call_thiscall_0(get_owner_rectangle, owner));
    if (!readable_range(owner_rectangle, 0x08)) {
        reset_directional_shield_hover_components();
        return;
    }
    const std::int32_t owner_x = read_at<std::int32_t>(
        owner_rectangle, 0, 0);
    const std::int32_t owner_y = read_at<std::int32_t>(
        owner_rectangle, 4, 0);

    DirectionalShieldHoverComponents& hover =
        g_directional_shield_hover_components;
    if (hover.info_display != info_display ||
        hover.craft != runtime.craft || hover.owner != owner) {
        reset_directional_shield_hover_components();
        hover.info_display = info_display;
        hover.craft = runtime.craft;
        hover.owner = owner;
    }

    auto visit = [&](std::uint32_t operation,
                     std::size_t index) noexcept -> std::uint32_t {
        if (operation == 0) return 1;
        DirectionalShieldHoverComponent& component =
            hover.components[index];
        component.vtable = at(g_armada, kStandardComponentVtableRva);
        component.owner = owner;
        const FloatRectangle& hit = runtime.hit_rectangles[index];
        component.rectangle = NativeRectangle{
            static_cast<std::int32_t>(std::floor(hit.x)) - owner_x,
            static_cast<std::int32_t>(std::floor(hit.y)) - owner_y,
            static_cast<std::int32_t>(std::ceil(hit.x + hit.width)) -
                owner_x - 1,
            static_cast<std::int32_t>(std::ceil(hit.y + hit.height)) -
                owner_y - 1,
        };
        if (hover.current[index] != runtime.current[index] ||
            hover.maximum[index] != runtime.maximum[index] ||
            !component.normal || !component.verbose) {
            if (!format_directional_shield_tooltip(
                    static_cast<int>(index), false,
                    component.normal_storage.data(),
                    component.normal_storage.size()) ||
                !format_directional_shield_tooltip(
                    static_cast<int>(index), true,
                    component.verbose_storage.data(),
                    component.verbose_storage.size())) {
                return 0;
            }
            component.normal = component.normal_storage.data();
            component.verbose = component.verbose_storage.data();
            hover.current[index] = runtime.current[index];
            hover.maximum[index] = runtime.maximum[index];
        }
        return 1;
    };
    const auto visit_step = +[](void* opaque, std::uint32_t operation,
                          std::size_t index) noexcept -> std::uint32_t {
        return (*static_cast<decltype(visit)*>(opaque))(operation, index);
    };
    if (!a2fo_rs_stage10_identity_each_visible(
            hover.components.size(), visit_step, &visit)) {
        reset_directional_shield_hover_components();
        return;
    }

    const std::int32_t cursor_x = read_at<std::int32_t>(
        at(g_armada, kCursorXRva), 0,
        std::numeric_limits<std::int32_t>::min());
    const std::int32_t cursor_y = read_at<std::int32_t>(
        at(g_armada, kCursorYRva), 0,
        std::numeric_limits<std::int32_t>::min());
    const int facing =
        a2fo::craft_identity::directional_shield_segment_at(
            runtime.hit_rectangles,
            static_cast<float>(cursor_x),
            static_cast<float>(cursor_y));
    if (InterlockedCompareExchange(
            &g_directional_shield_hover_component_report_count,
            1, 0) == 0) {
        char message[768]{};
        std::snprintf(
            message, sizeof(message),
            "Directional-shield native hover components active: cursor=%ld,%ld facing=%d owner=%p activeOwner=%p ownerOrigin=%ld,%ld screenRects F=%.0f,%.0f,%.0f,%.0f A=%.0f,%.0f,%.0f,%.0f P=%.0f,%.0f,%.0f,%.0f S=%.0f,%.0f,%.0f,%.0f",
            static_cast<long>(cursor_x), static_cast<long>(cursor_y),
            facing, owner,
            read_at<void*>(at(g_armada, kActiveInterfaceOwnerRva),
                           0, nullptr),
            static_cast<long>(owner_x), static_cast<long>(owner_y),
            static_cast<double>(runtime.hit_rectangles[0].x),
            static_cast<double>(runtime.hit_rectangles[0].y),
            static_cast<double>(runtime.hit_rectangles[0].width),
            static_cast<double>(runtime.hit_rectangles[0].height),
            static_cast<double>(runtime.hit_rectangles[1].x),
            static_cast<double>(runtime.hit_rectangles[1].y),
            static_cast<double>(runtime.hit_rectangles[1].width),
            static_cast<double>(runtime.hit_rectangles[1].height),
            static_cast<double>(runtime.hit_rectangles[2].x),
            static_cast<double>(runtime.hit_rectangles[2].y),
            static_cast<double>(runtime.hit_rectangles[2].width),
            static_cast<double>(runtime.hit_rectangles[2].height),
            static_cast<double>(runtime.hit_rectangles[3].x),
            static_cast<double>(runtime.hit_rectangles[3].y),
            static_cast<double>(runtime.hit_rectangles[3].width),
            static_cast<double>(runtime.hit_rectangles[3].height));
        log_line(message);
    }

    if (facing >= 0 && facing < 4 &&
        InterlockedCompareExchange(
            &g_directional_shield_tooltip_hit_report_count,
            1, 0) == 0) {
        const FloatRectangle& hit = runtime.hit_rectangles[
            static_cast<std::size_t>(facing)];
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "First directional-shield native component hit: facing=%d cursor=%ld,%ld screenRect=%.0f,%.0f,%.0f,%.0f",
            facing, static_cast<long>(cursor_x),
            static_cast<long>(cursor_y),
            static_cast<double>(hit.x),
            static_cast<double>(hit.y),
            static_cast<double>(hit.width),
            static_cast<double>(hit.height));
        log_line(message);
    }

    void* update = g_standard_component_update_hook.gateway;
    if (!update) return;
    struct UpdateContext { DirectionalShieldHoverComponents* hover; void* update; int facing; };
    UpdateContext update_context{&hover, update, facing};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<UpdateContext*>(opaque);
        if (action == 0) return 1;
        DirectionalShieldHoverComponent& component = ctx.hover->components[index];
        a2fo_identity_call_thiscall_0(ctx.update, &component);
        if (action == 2 && component.presented != 0 &&
            InterlockedCompareExchange(&g_directional_shield_hover_presented_report_count,
                                       1, 0) == 0) {
            char message[256]{};
            std::snprintf(message, sizeof(message),
                "First directional-shield native hover component accepted by TooltipManager: facing=%d normal='%s'",
                ctx.facing, component.normal ? component.normal : "");
            log_line(message);
        }
        return 0;
    };
    a2fo_rs_stage9_craft_hover_update(step, &update_context, hover.components.size(), facing);
}

void reset_ammunition_hover_components() noexcept {
    const auto step = +[](void*, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        if (action == 0)
            return g_ammunition_hover_components.components[index].presented != 0;
        if (action == 1 && readable_range(
                at(g_armada, kTooltipManagerClearRva), 1)) {
            a2fo_identity_call_thiscall_0(
                at(g_armada, kTooltipManagerClearRva),
                at(g_armada, kTooltipManagerObjectRva));
        } else if (action == 2) g_ammunition_hover_components = {};
        return 0;
    };
    a2fo_rs_stage9_craft_reset_hover(step, nullptr,
        g_ammunition_hover_components.components.size());
}

int ammunition_store_at(
    const AmmunitionTooltipRuntime& runtime,
    float cursor_x, float cursor_y) noexcept {
    if (!std::isfinite(cursor_x) || !std::isfinite(cursor_y)) return -1;
    struct Context {
        const AmmunitionTooltipRuntime& runtime;
        float cursor_x;
        float cursor_y;
    } context{runtime, cursor_x, cursor_y};
    const auto step = +[](void* opaque, std::uint32_t,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        const auto& ctx = *static_cast<Context*>(opaque);
        if (!ctx.runtime.visible[index]) return 0;
        const FloatRectangle& rectangle = ctx.runtime.hit_rectangles[index];
        return usable_float_rectangle(rectangle) &&
            ctx.cursor_x >= rectangle.x &&
            ctx.cursor_x <= rectangle.x + rectangle.width &&
            ctx.cursor_y >= rectangle.y &&
            ctx.cursor_y <= rectangle.y + rectangle.height;
    };
    return a2fo_rs_stage9_craft_find_hover_row(step, &context,
        runtime.hit_rectangles.size());
}

void update_ammunition_hover_components(void* info_display) noexcept {
    const AmmunitionTooltipRuntime& runtime = g_ammunition_tooltip;
    if (!g_directional_shield_wireframe_tooltips_available ||
        !info_display || !runtime.active ||
        runtime.info_display != info_display || !runtime.craft ||
        read_at<void*>(info_display,
                       kInfoDisplaySelectedCraftOffset, nullptr) !=
            runtime.craft) {
        reset_ammunition_hover_components();
        return;
    }

    void* owner = read_at<void*>(
        at(g_armada, kActiveInterfaceOwnerRva), 0, nullptr);
    void* owner_vtable = read_at<void*>(owner, 0, nullptr);
    void* get_owner_rectangle = read_at<void*>(
        owner_vtable, 0x64, nullptr);
    if (!owner || !owner_vtable || !get_owner_rectangle) {
        reset_ammunition_hover_components();
        return;
    }
    const void* owner_rectangle = reinterpret_cast<const void*>(
        a2fo_identity_call_thiscall_0(get_owner_rectangle, owner));
    if (!readable_range(owner_rectangle, 0x08)) {
        reset_ammunition_hover_components();
        return;
    }
    const std::int32_t owner_x = read_at<std::int32_t>(
        owner_rectangle, 0, 0);
    const std::int32_t owner_y = read_at<std::int32_t>(
        owner_rectangle, 4, 0);

    AmmunitionHoverComponents& hover = g_ammunition_hover_components;
    if (hover.info_display != info_display ||
        hover.craft != runtime.craft || hover.owner != owner) {
        reset_ammunition_hover_components();
        hover.info_display = info_display;
        hover.craft = runtime.craft;
        hover.owner = owner;
    }

    auto visit = [&](std::uint32_t operation,
                     std::size_t index) noexcept -> std::uint32_t {
        if (operation == 0) return runtime.visible[index];
        DirectionalShieldHoverComponent& component = hover.components[index];
        component.vtable = at(g_armada, kStandardComponentVtableRva);
        component.owner = owner;
        const FloatRectangle& hit = runtime.hit_rectangles[index];
        component.rectangle = NativeRectangle{
            static_cast<std::int32_t>(std::floor(hit.x)) - owner_x,
            static_cast<std::int32_t>(std::floor(hit.y)) - owner_y,
            static_cast<std::int32_t>(std::ceil(hit.x + hit.width)) -
                owner_x - 1,
            static_cast<std::int32_t>(std::ceil(hit.y + hit.height)) -
                owner_y - 1,
        };
        if (hover.current[index] != runtime.current[index] ||
            hover.maximum[index] != runtime.maximum[index] ||
            !component.normal || !component.verbose) {
            if (!format_ammunition_tooltip(
                    index, false, component.normal_storage.data(),
                    component.normal_storage.size()) ||
                !format_ammunition_tooltip(
                    index, true, component.verbose_storage.data(),
                    component.verbose_storage.size())) {
                return 0;
            }
            component.normal = component.normal_storage.data();
            component.verbose = component.verbose_storage.data();
            hover.current[index] = runtime.current[index];
            hover.maximum[index] = runtime.maximum[index];
        }
        return 1;
    };
    const auto visit_step = +[](void* opaque, std::uint32_t operation,
                          std::size_t index) noexcept -> std::uint32_t {
        return (*static_cast<decltype(visit)*>(opaque))(operation, index);
    };
    if (!a2fo_rs_stage10_identity_each_visible(
            hover.components.size(), visit_step, &visit)) {
        reset_ammunition_hover_components();
        return;
    }

    const std::int32_t cursor_x = read_at<std::int32_t>(
        at(g_armada, kCursorXRva), 0,
        std::numeric_limits<std::int32_t>::min());
    const std::int32_t cursor_y = read_at<std::int32_t>(
        at(g_armada, kCursorYRva), 0,
        std::numeric_limits<std::int32_t>::min());
    const int selected = ammunition_store_at(
        runtime, static_cast<float>(cursor_x),
        static_cast<float>(cursor_y));

    if (InterlockedCompareExchange(
            &g_ammunition_hover_component_report_count, 1, 0) == 0) {
        char message[512]{};
        std::snprintf(
            message, sizeof(message),
            "Ammunition native hover components active: cursor=%ld,%ld store=%d owner=%p Photon=%.0f,%.0f,%.0f,%.0f Quantum=%.0f,%.0f,%.0f,%.0f Shuttle=%.0f,%.0f,%.0f,%.0f",
            static_cast<long>(cursor_x), static_cast<long>(cursor_y),
            selected, owner,
            static_cast<double>(runtime.hit_rectangles[0].x),
            static_cast<double>(runtime.hit_rectangles[0].y),
            static_cast<double>(runtime.hit_rectangles[0].width),
            static_cast<double>(runtime.hit_rectangles[0].height),
            static_cast<double>(runtime.hit_rectangles[1].x),
            static_cast<double>(runtime.hit_rectangles[1].y),
            static_cast<double>(runtime.hit_rectangles[1].width),
            static_cast<double>(runtime.hit_rectangles[1].height),
            static_cast<double>(runtime.hit_rectangles[2].x),
            static_cast<double>(runtime.hit_rectangles[2].y),
            static_cast<double>(runtime.hit_rectangles[2].width),
            static_cast<double>(runtime.hit_rectangles[2].height));
        log_line(message);
    }

    void* update = g_standard_component_update_hook.gateway;
    if (!update) return;
    struct UpdateContext {
        AmmunitionHoverComponents* hover;
        const AmmunitionTooltipRuntime* runtime;
        void* update;
        int selected;
    } update_context{&hover, &runtime, update, selected};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<UpdateContext*>(opaque);
        if (action == 0) return ctx.runtime->visible[index];
        DirectionalShieldHoverComponent& component = ctx.hover->components[index];
        a2fo_identity_call_thiscall_0(ctx.update, &component);
        if (action == 2 && component.presented != 0 &&
            InterlockedCompareExchange(&g_ammunition_hover_presented_report_count, 1, 0) == 0) {
            char message[256]{};
            std::snprintf(message, sizeof(message),
                "First ammunition native hover component accepted by TooltipManager: store=%d normal='%s'",
                ctx.selected, component.normal ? component.normal : "");
            log_line(message);
        }
        return 0;
    };
    a2fo_rs_stage9_craft_hover_update(step, &update_context, hover.components.size(), selected);
}

void reset_selected_status_hover_components() noexcept {
    const auto step = +[](void*, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        if (action == 0)
            return g_selected_status_hover_components.components[index].presented != 0;
        if (action == 1 && readable_range(
                at(g_armada, kTooltipManagerClearRva), 1)) {
            a2fo_identity_call_thiscall_0(
                at(g_armada, kTooltipManagerClearRva),
                at(g_armada, kTooltipManagerObjectRva));
        } else if (action == 2) g_selected_status_hover_components = {};
        return 0;
    };
    a2fo_rs_stage9_craft_reset_hover(step, nullptr,
        g_selected_status_hover_components.components.size());
}

int selected_status_at(
    const SelectedStatusTooltipRuntime& runtime,
    float cursor_x, float cursor_y) noexcept {
    if (!std::isfinite(cursor_x) || !std::isfinite(cursor_y)) return -1;
    struct Context {
        const SelectedStatusTooltipRuntime& runtime;
        float cursor_x;
        float cursor_y;
    } context{runtime, cursor_x, cursor_y};
    const auto step = +[](void* opaque, std::uint32_t,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        const auto& ctx = *static_cast<Context*>(opaque);
        if (!ctx.runtime.visible[index]) return 0;
        const FloatRectangle& rectangle = ctx.runtime.hit_rectangles[index];
        return usable_float_rectangle(rectangle) &&
            ctx.cursor_x >= rectangle.x &&
            ctx.cursor_x <= rectangle.x + rectangle.width &&
            ctx.cursor_y >= rectangle.y &&
            ctx.cursor_y <= rectangle.y + rectangle.height;
    };
    return a2fo_rs_stage9_craft_find_hover_row(step, &context,
        runtime.hit_rectangles.size());
}

void update_selected_status_hover_components(void* info_display) noexcept {
    const SelectedStatusTooltipRuntime& runtime =
        g_selected_status_tooltip;
    if (!g_directional_shield_wireframe_tooltips_available ||
        !info_display || !runtime.active ||
        runtime.info_display != info_display || !runtime.craft ||
        read_at<void*>(info_display,
                       kInfoDisplaySelectedCraftOffset, nullptr) !=
            runtime.craft) {
        reset_selected_status_hover_components();
        return;
    }

    void* owner = read_at<void*>(
        at(g_armada, kActiveInterfaceOwnerRva), 0, nullptr);
    void* owner_vtable = read_at<void*>(owner, 0, nullptr);
    void* get_owner_rectangle = read_at<void*>(
        owner_vtable, 0x64, nullptr);
    if (!owner || !owner_vtable || !get_owner_rectangle) {
        reset_selected_status_hover_components();
        return;
    }
    const void* owner_rectangle = reinterpret_cast<const void*>(
        a2fo_identity_call_thiscall_0(get_owner_rectangle, owner));
    if (!readable_range(owner_rectangle, 0x08)) {
        reset_selected_status_hover_components();
        return;
    }
    const std::int32_t owner_x = read_at<std::int32_t>(
        owner_rectangle, 0, 0);
    const std::int32_t owner_y = read_at<std::int32_t>(
        owner_rectangle, 4, 0);

    SelectedStatusHoverComponents& hover =
        g_selected_status_hover_components;
    if (hover.info_display != info_display ||
        hover.craft != runtime.craft || hover.owner != owner ||
        hover.visible != runtime.visible) {
        reset_selected_status_hover_components();
        hover.info_display = info_display;
        hover.craft = runtime.craft;
        hover.owner = owner;
        hover.visible = runtime.visible;
    }

    auto visit = [&](std::uint32_t operation,
                     std::size_t index) noexcept -> std::uint32_t {
        if (operation == 0) return runtime.visible[index];
        DirectionalShieldHoverComponent& component = hover.components[index];
        component.vtable = at(g_armada, kStandardComponentVtableRva);
        component.owner = owner;
        const FloatRectangle& hit = runtime.hit_rectangles[index];
        component.rectangle = NativeRectangle{
            static_cast<std::int32_t>(std::floor(hit.x)) - owner_x,
            static_cast<std::int32_t>(std::floor(hit.y)) - owner_y,
            static_cast<std::int32_t>(std::ceil(hit.x + hit.width)) -
                owner_x - 1,
            static_cast<std::int32_t>(std::ceil(hit.y + hit.height)) -
                owner_y - 1,
        };
        if (hover.current[index] != runtime.current[index] ||
            hover.maximum[index] != runtime.maximum[index] ||
            !component.normal || !component.verbose) {
            if (!format_selected_status_tooltip(
                    index, false, component.normal_storage.data(),
                    component.normal_storage.size()) ||
                !format_selected_status_tooltip(
                    index, true, component.verbose_storage.data(),
                    component.verbose_storage.size())) {
                return 0;
            }
            component.normal = component.normal_storage.data();
            component.verbose = component.verbose_storage.data();
            hover.current[index] = runtime.current[index];
            hover.maximum[index] = runtime.maximum[index];
        }
        return 1;
    };
    const auto visit_step = +[](void* opaque, std::uint32_t operation,
                          std::size_t index) noexcept -> std::uint32_t {
        return (*static_cast<decltype(visit)*>(opaque))(operation, index);
    };
    if (!a2fo_rs_stage10_identity_each_visible(
            hover.components.size(), visit_step, &visit)) {
        reset_selected_status_hover_components();
        return;
    }

    const std::int32_t cursor_x = read_at<std::int32_t>(
        at(g_armada, kCursorXRva), 0,
        std::numeric_limits<std::int32_t>::min());
    const std::int32_t cursor_y = read_at<std::int32_t>(
        at(g_armada, kCursorYRva), 0,
        std::numeric_limits<std::int32_t>::min());
    const int selected = selected_status_at(
        runtime, static_cast<float>(cursor_x),
        static_cast<float>(cursor_y));

    void* update = g_standard_component_update_hook.gateway;
    if (!update) return;
    struct UpdateContext {
        SelectedStatusHoverComponents* hover;
        const SelectedStatusTooltipRuntime* runtime;
        void* update;
        int selected;
    } update_context{&hover, &runtime, update, selected};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<UpdateContext*>(opaque);
        if (action == 0) return ctx.runtime->visible[index];
        DirectionalShieldHoverComponent& component = ctx.hover->components[index];
        a2fo_identity_call_thiscall_0(ctx.update, &component);
        if (action == 2 && component.presented != 0 &&
            InterlockedCompareExchange(&g_selected_status_hover_presented_report_count,
                                       1, 0) == 0) {
            char message[256]{};
            std::snprintf(message, sizeof(message),
                "First selected-status tooltip accepted: status=%d normal='%s'",
                ctx.selected, component.normal ? component.normal : "");
            log_line(message);
        }
        return 0;
    };
    a2fo_rs_stage9_craft_hover_update(step, &update_context, hover.components.size(), selected);
}

void __attribute__((fastcall)) standard_component_update_hook(
    void* component, void*) noexcept {
    if (g_standard_component_update_hook.gateway) {
        a2fo_identity_call_thiscall_0(
            g_standard_component_update_hook.gateway, component);
    }
}

void update_directional_shield_native_tooltip_phase(
    void* info_display) noexcept {
    struct Context {
        void* info_display;
        int facing = -1;
        void* component = nullptr;
    } context{info_display};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        const auto& runtime = g_directional_shield_tooltip;
        if (action == 0) return !g_directional_shield_wireframe_tooltips_available;
        if (action == 1) {
            return g_runtime_ready && ctx.info_display && runtime.active &&
                runtime.info_display == ctx.info_display && runtime.craft &&
                read_at<void*>(ctx.info_display,
                    kInfoDisplaySelectedCraftOffset, nullptr) == runtime.craft;
        }
        if (action == 2) clear_directional_shield_native_tooltip();
        else if (action == 3) {
            const std::int32_t cursor_x = read_at<std::int32_t>(
                at(g_armada, kCursorXRva), 0,
                std::numeric_limits<std::int32_t>::min());
            const std::int32_t cursor_y = read_at<std::int32_t>(
                at(g_armada, kCursorYRva), 0,
                std::numeric_limits<std::int32_t>::min());
            ctx.facing = a2fo::craft_identity::directional_shield_segment_at(
                runtime.hit_rectangles,
                static_cast<float>(cursor_x), static_cast<float>(cursor_y));
            ctx.component = read_at<void*>(
                ctx.info_display, kInfoDisplayWireframeOffset, nullptr);
        } else if (action == 4) {
            return ctx.facing >= 0 && ctx.facing < 4 && ctx.component;
        } else if (action == 5) {
            return configure_directional_shield_native_tooltip_provider(
                ctx.facing);
        } else if (action == 6) {
            set_standard_component_text(ctx.component,
                kStandardComponentSetTooltipTextRva,
                g_directional_shield_native_tooltip_provider.normal);
        } else if (action == 7) {
            set_standard_component_text(ctx.component,
                kStandardComponentSetVerboseTooltipTextRva,
                g_directional_shield_native_tooltip_provider.verbose);
        } else if (action == 8) {
            present_directional_shield_native_tooltip(
                ctx.component, runtime.craft, ctx.facing);
        }
        return 0;
    };
    a2fo_rs_stage9_craft_native_tip_phase(step, &context);
}

[[maybe_unused]] std::uintptr_t __attribute__((fastcall)) tooltip_manager_show_hook(
    void* tooltip_manager, void*, std::uintptr_t elapsed,
    void* provider, void* owner) noexcept {
    struct Context {
        void* tooltip_manager;
        std::uintptr_t elapsed;
        void* provider;
        void* owner;
        int facing = -1;
    } context{tooltip_manager, elapsed, provider, owner};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        const auto& runtime = g_directional_shield_tooltip;
        if (action == 0) {
            return ctx.tooltip_manager == at(g_armada, kTooltipManagerObjectRva) &&
                runtime.active && runtime.info_display && runtime.craft &&
                read_at<void*>(runtime.info_display,
                    kInfoDisplaySelectedCraftOffset, nullptr) == runtime.craft;
        }
        if (action == 1) {
            const std::int32_t cursor_x = read_at<std::int32_t>(
                at(g_armada, kCursorXRva), 0,
                std::numeric_limits<std::int32_t>::min());
            const std::int32_t cursor_y = read_at<std::int32_t>(
                at(g_armada, kCursorYRva), 0,
                std::numeric_limits<std::int32_t>::min());
            ctx.facing = a2fo::craft_identity::directional_shield_segment_at(
                runtime.hit_rectangles,
                static_cast<float>(cursor_x), static_cast<float>(cursor_y));
        } else if (action == 2) {
            return ctx.facing >= 0 && ctx.facing < 4;
        } else if (action == 3) {
            return configure_directional_shield_native_tooltip_provider(ctx.facing);
        } else if (action == 4) {
            ctx.provider = &g_directional_shield_native_tooltip_provider;
            if (InterlockedCompareExchange(
                    &g_directional_shield_show_substitution_report_count,
                    1, 0) == 0) {
                char message[256]{};
                std::snprintf(message, sizeof(message),
                    "First stock tooltip request replaced at creation: facing=%d provider=%p normal='%s'",
                    ctx.facing, ctx.provider,
                    g_directional_shield_native_tooltip_provider.normal);
                log_line(message);
            }
        } else if (action == 5) {
            return g_tooltip_manager_show_hook.gateway != nullptr;
        } else if (action == 6) {
            return a2fo_identity_call_thiscall_3(
                g_tooltip_manager_show_hook.gateway, ctx.tooltip_manager,
                ctx.elapsed, reinterpret_cast<std::uintptr_t>(ctx.provider),
                reinterpret_cast<std::uintptr_t>(ctx.owner));
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_tooltip_show(step, &context);
}

[[maybe_unused]] void __attribute__((fastcall)) tooltip_manager_render_hook(
    void* tooltip_manager, void*) noexcept {
    struct Context { void* tooltip_manager; } context{tooltip_manager};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        const auto& runtime = g_directional_shield_tooltip;
        if (action == 0) return ctx.tooltip_manager ==
            at(g_armada, kTooltipManagerObjectRva) && runtime.active &&
            runtime.info_display;
        if (action == 1) {
            // Native manager owns the global framing, placement and render pass.
            update_directional_shield_native_tooltip_phase(runtime.info_display);
        } else if (action == 2) {
            return g_tooltip_manager_render_hook.gateway != nullptr;
        } else if (action == 3) {
            a2fo_identity_call_thiscall_0(
                g_tooltip_manager_render_hook.gateway, ctx.tooltip_manager);
        } else if (action == 4) {
            if (g_directional_shield_native_tooltip.presented &&
                InterlockedCompareExchange(
                    &g_directional_shield_native_render_report_count,
                    1, 0) == 0)
                log_line("First directional-shield tooltip passed through Armada's top-level framed render stage");
        }
        return 0;
    };
    a2fo_rs_stage9_craft_tooltip_render(step, &context);
}

std::uintptr_t __attribute__((fastcall)) selected_info_update_hook(
    void* info_display, void*, std::uintptr_t argument) noexcept {
    const std::uintptr_t result = a2fo_identity_call_thiscall_1(
        g_selected_info_update_hook.gateway, info_display, argument);
    return result;
}

void __attribute__((fastcall)) selected_info_render_hook(
    void* info_display, void*) noexcept;

enum class SelectedPanelKind {
    single,
    builder,
};

using a2fo::craft_identity::SelectedPanelTextAnchor;

SelectedPanelTextAnchor selected_panel_text_anchor(
    void* info_display, SelectedPanelKind panel_kind) noexcept {
    if (!info_display) return {};
    const auto candidate = [info_display](
            std::size_t component_offset,
            const RawRectangle* configured_rectangle) noexcept {
        a2fo::craft_identity::SelectedPanelAnchorCandidate result{};
        result.component = read_at<void*>(
            info_display, component_offset, nullptr);
        result.rectangle = read_at<NativeRectangle>(
            result.component, kTextComponentLiveRectangleOffset, {});
        result.configured_rectangle = configured_rectangle;
        return result;
    };
    const auto& ui = g_ui_configuration;
    const bool builder = panel_kind == SelectedPanelKind::builder;
    const RawRectangle* name_rectangle = builder
        ? (ui.builder_name_rectangle_found ? &ui.builder_name_rectangle : nullptr)
        : (ui.single_name_rectangle_found ? &ui.single_name_rectangle : nullptr);
    const RawRectangle* class_rectangle = builder
        ? (ui.builder_class_rectangle_found ? &ui.builder_class_rectangle : nullptr)
        : (ui.single_class_rectangle_found ? &ui.single_class_rectangle : nullptr);
    // Keep the original native argument expressions together. The three
    // candidates are fully captured before Rust selects/rebases an anchor.
    std::array<a2fo::craft_identity::SelectedPanelAnchorCandidate, 3> native{};
    const RawRectangle* configured_captain = nullptr;
    const auto capture = [&](const a2fo::craft_identity::SelectedPanelAnchorCandidate& captain,
                             const RawRectangle* configured,
                             const std::array<a2fo::craft_identity::SelectedPanelAnchorCandidate, 2>& fallbacks) {
        native[0] = captain; configured_captain = configured;
        native[1] = fallbacks[0]; native[2] = fallbacks[1];
    };
    capture(
        candidate(kInfoDisplayCaptainTextOffset, nullptr),
        ui.captain_rectangle_found ? &ui.captain_rectangle : nullptr,
        {{candidate(builder ? kInfoDisplayBuilderNameTextOffset
                            : kInfoDisplayNameTextOffsets[0], name_rectangle),
          candidate(builder ? kInfoDisplayBuilderClassTextOffset
                            : kInfoDisplayClassTextOffset, class_rectangle)}});
    static_assert(sizeof(RawRectangle) == 4 * sizeof(std::int32_t), "anchor POD rectangle");
    static_assert(sizeof(NativeRectangle) == 4 * sizeof(std::int32_t), "anchor POD output");
    const auto pod = [](const a2fo::craft_identity::SelectedPanelAnchorCandidate& value) {
        return A2foRsAnchorCandidate{reinterpret_cast<std::uintptr_t>(value.component),
            {value.rectangle.left, value.rectangle.top, value.rectangle.right, value.rectangle.bottom},
            value.configured_rectangle ? &value.configured_rectangle->x : nullptr};
    };
    const auto captain = pod(native[0]);
    const A2foRsAnchorCandidate fallbacks[]{pod(native[1]), pod(native[2])};
    SelectedPanelTextAnchor result{};
    result.component = reinterpret_cast<void*>(a2fo_rs_stage9_craft_anchor(&captain,
        configured_captain ? &configured_captain->x : nullptr, fallbacks, &result.captain_rectangle.left));
    struct Context { SelectedPanelTextAnchor& result; void* captain = nullptr; const RawRectangle* raw = nullptr; std::int32_t offset = 0; } c{result};
    craft_sprite_policy(3, c, [&](Context& ctx, std::uint32_t op, std::uintptr_t) -> std::uintptr_t {
        switch (op) {
        case 0: return ctx.result.component != nullptr;
        case 1: ctx.captain = read_at<void*>(info_display, kInfoDisplayCaptainTextOffset, nullptr); return 0;
        case 2: return ctx.captain != nullptr;
        case 3: return ui.captain_rectangle_found;
        case 4: ctx.raw = &ui.captain_rectangle; ctx.offset = read_at<std::int32_t>(info_display, 0x1f0, 0); return 0;
        case 5: ctx.result.component = ctx.captain; a2fo_rs_stage9_craft_anchor_fallback(&ctx.raw->x, ctx.offset, &ctx.result.captain_rectangle.left); return 0;
        default: return 0;
        }
    });
    return result;
}

void render_selected_info_panel(
    void* info_display, void* native_renderer,
    SelectedPanelKind panel_kind) noexcept {
    struct PanelContext {
        std::size_t panel = g_active_panel;
        void* display = g_active_info_display;
        ~PanelContext() { g_active_panel = panel; g_active_info_display = display; }
    } context;
    g_active_panel = panel_kind == SelectedPanelKind::builder ? 1 : 0;
    g_active_info_display = info_display;
    g_directional_shield_tooltip = {};
    g_ammunition_tooltip = {};
    g_selected_status_tooltip = {};

    struct RenderContext {
        void* info_display;
        void* native_renderer;
        SelectedPanelKind panel_kind;
        std::array<TextColourOverride, 2> overrides{};
        bool name_colour_applied = false;
        void* craft = nullptr;
        SelectedPanelTextAnchor anchor{};
        void* text_component = nullptr;
        NativeRectangle captain_rectangle{}, registry_rectangle{};
        Colour native_colour{}, shared_colour{}, captain_colour{}, registry_colour{};
        const CraftIdentity* identity = nullptr;
        bool captain_drawn = false, registry_drawn = false;
    } render{info_display, native_renderer, panel_kind};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<RenderContext*>(opaque);
        if (action == 1 || action == 8) return g_runtime_ready && ctx.info_display;
        if (action == 2 || action == 11) {
            void* parameter_db = gui_parameter_db();
            if (!g_ui_configuration.loaded || g_ui_configuration.parameter_db != parameter_db)
                refresh_ui_configuration(parameter_db);
        } else if (action == 3) return g_ui_configuration.ship_name_colour_found;
        else if (action == 4) {
            ctx.overrides[index] = override_text_component_colour(
                ctx.info_display, kInfoDisplayNameTextOffsets[index],
                g_ui_configuration.ship_name_colour);
            ctx.name_colour_applied = ctx.name_colour_applied || ctx.overrides[index].address != nullptr;
        } else if (action == 5) {
            if (ctx.name_colour_applied && InterlockedCompareExchange(
                    &g_ship_name_colour_report_count, 1, 0) == 0)
                log_line("shipNameColor applied to the native selected ship-name text components");
        } else if (action == 6) {
            if (ctx.native_renderer)
                a2fo_identity_call_thiscall_0(ctx.native_renderer, ctx.info_display);
        } else if (action == 7) restore_text_component_colour(ctx.overrides[index]);
        else if (action == 9) {
            ctx.craft = read_at<void*>(ctx.info_display, kInfoDisplaySelectedCraftOffset, nullptr);
            return ctx.craft != nullptr;
        } else if (action == 10) {
            reset_directional_shield_hover_components();
            reset_ammunition_hover_components();
            reset_selected_status_hover_components();
            update_directional_shield_component_tooltip(ctx.info_display);
        } else if (action == 12) {
            ctx.anchor = selected_panel_text_anchor(ctx.info_display, ctx.panel_kind);
            if (!ctx.anchor.component || !usable_native_rectangle(ctx.anchor.captain_rectangle))
                return 0;
            ctx.text_component = ctx.anchor.component;
            ctx.captain_rectangle = ctx.anchor.captain_rectangle;
            return 1;
        } else if (action == 13) {
            update_directional_shield_hover_components(ctx.info_display);
            update_ammunition_hover_components(ctx.info_display);
            reset_selected_status_hover_components();
            update_directional_shield_component_tooltip(ctx.info_display);
        } else if (action == 14) {
            if (ctx.panel_kind == SelectedPanelKind::builder &&
                InterlockedCompareExchange(&g_builder_panel_anchor_report_count, 1, 0) == 0) {
                char message[288]{};
                std::snprintf(message, sizeof(message),
                    "Tall producer selected-panel extension anchor active: component=%p captain-relative rectangle=%ld,%ld,%ld,%ld",
                    ctx.text_component,
                    static_cast<long>(ctx.captain_rectangle.left),
                    static_cast<long>(ctx.captain_rectangle.top),
                    static_cast<long>(ctx.captain_rectangle.right),
                    static_cast<long>(ctx.captain_rectangle.bottom));
                log_line(message);
            }
            ctx.native_colour = text_component_colour(ctx.text_component);
            ctx.shared_colour = g_ui_configuration.shared_text_colour_found
                ? g_ui_configuration.shared_text_colour : ctx.native_colour;
            ctx.captain_colour = g_ui_configuration.captain_colour_found
                ? g_ui_configuration.captain_colour : ctx.shared_colour;
            ctx.registry_colour = g_ui_configuration.registry_colour_found
                ? g_ui_configuration.registry_colour : ctx.shared_colour;
        } else if (action == 15) draw_ammunition_rows(
            ctx.info_display, ctx.craft, ctx.text_component,
            ctx.captain_rectangle, ctx.shared_colour);
        else if (action == 16) draw_directional_shield_rows(
            ctx.info_display, ctx.craft, ctx.text_component,
            ctx.captain_rectangle, ctx.shared_colour);
        else if (action == 17) draw_selected_status_bars(
            ctx.info_display, ctx.craft, ctx.text_component, ctx.captain_rectangle);
        else if (action == 18) update_ammunition_hover_components(ctx.info_display);
        else if (action == 19) update_selected_status_hover_components(ctx.info_display);
        else if (action == 20) update_directional_shield_hover_components(ctx.info_display);
        else if (action == 21) update_directional_shield_component_tooltip(ctx.info_display);
        else if (action == 22) {
            ctx.identity = ensure_craft_identity(ctx.craft);
            return ctx.identity != nullptr;
        } else if (action == 23 || action == 28) {
            draw_directional_shield_hover_tooltip(ctx.info_display, ctx.craft,
                ctx.text_component, ctx.captain_rectangle, ctx.shared_colour);
        } else if (action == 24) {
            ctx.registry_rectangle = ctx.captain_rectangle;
            if (g_ui_configuration.captain_rectangle_found &&
                g_ui_configuration.registry_rectangle_found)
                ctx.registry_rectangle = translated_rectangle(ctx.captain_rectangle,
                    g_ui_configuration.captain_rectangle, g_ui_configuration.registry_rectangle);
        } else if (action == 25) {
            if (g_ui_configuration.captain_rectangle_found ||
                panel_style(PanelElement::CaptainText).area_found)
                ctx.captain_drawn = draw_identity_text(ctx.identity->captain_name,
                    panel_rectangle(PanelElement::CaptainText, ctx.captain_rectangle),
                    panel_colour(PanelElement::CaptainText, ctx.captain_colour), ctx.text_component);
        } else if (action == 26) {
            if ((g_ui_configuration.captain_rectangle_found &&
                 g_ui_configuration.registry_rectangle_found) ||
                panel_style(PanelElement::RegistryText).area_found)
                ctx.registry_drawn = draw_identity_text(ctx.identity->craft_registry,
                    panel_rectangle(PanelElement::RegistryText, ctx.registry_rectangle),
                    panel_colour(PanelElement::RegistryText, ctx.registry_colour), ctx.text_component);
        } else if (action == 27) {
            if ((ctx.captain_drawn || ctx.registry_drawn) &&
                InterlockedCompareExchange(&g_draw_report_count, 1, 0) == 0) {
                char message[320]{};
                std::snprintf(message, sizeof(message),
                    "First selected identity draw submitted on %s panel for native name row %ld: "
                    "captain=%s at %ld,%ld; registry=%s at %ld,%ld",
                    ctx.panel_kind == SelectedPanelKind::builder ? "builder" : "single",
                    static_cast<long>(ctx.identity->craft_name_index),
                    ctx.captain_drawn ? "yes" : "no",
                    static_cast<long>(ctx.captain_rectangle.left),
                    static_cast<long>(ctx.captain_rectangle.top),
                    ctx.registry_drawn ? "yes" : "no",
                    static_cast<long>(ctx.registry_rectangle.left),
                    static_cast<long>(ctx.registry_rectangle.top));
                log_line(message);
            }
        }
        return 0;
    };
    a2fo_rs_stage9_craft_render_panel(step, &render, kInfoDisplayNameTextOffsets.size());
}

void __attribute__((fastcall)) selected_builder_info_render_hook(
    void* info_display, void*) noexcept {
    render_selected_info_panel(
        info_display, g_selected_builder_info_render_hook.gateway,
        SelectedPanelKind::builder);
}

void __attribute__((fastcall)) selected_info_render_hook(
    void* info_display, void*) noexcept {
    render_selected_info_panel(
        info_display, g_selected_info_render_hook.gateway,
        SelectedPanelKind::single);
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    const void* address = at(module, rva);
    return readable_range(address, Size) &&
        std::memcmp(address, expected, Size) == 0;
}

template <std::size_t Size>
bool exact_bytes_match(
    HMODULE module, std::uintptr_t rva,
    const std::array<std::uint8_t, Size>& expected) noexcept {
    const void* address = at(module, rva);
    return readable_range(address, Size) &&
        std::memcmp(address, expected.data(), Size) == 0;
}

template <std::size_t Size>
bool patch_exact_bytes(
    HMODULE module, std::uintptr_t rva,
    const std::array<std::uint8_t, Size>& expected,
    const std::array<std::uint8_t, Size>& replacement) noexcept {
    void* address = at(module, rva);
    if (!address || !readable_range(address, Size)) return false;
    // Both byte checks, including the reread after VirtualProtect, are Rust-owned.
    return a2fo_rs_windows_patch_double(address,replacement.data(),expected.data(),Size)!=0;
}

template <std::size_t Size>
bool checked_armada_signature(
    const char* name, std::uintptr_t rva,
    const std::uint8_t (&expected)[Size]) noexcept {
    if (signature_matches(g_armada, rva, expected)) return true;
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "%s signature mismatch at Armada RVA 0x%08lX",
                  name, static_cast<unsigned long>(rva));
    log_line(message);
    return false;
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
    if (readable_range(site, 6) && bytes[0] == 0x68 && bytes[5] == 0xc3) {
        std::uint32_t destination = 0;
        std::memcpy(&destination, bytes + 1, sizeof(destination));
        if (patch_length) *patch_length = 6;
        return reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(destination));
    }
    return nullptr;
}

void* direct_call_destination(const void* site) noexcept {
    if (!site || !readable_range(site, 5)) return nullptr;
    const auto* bytes = static_cast<const std::uint8_t*>(site);
    if (bytes[0] != 0xe8) return nullptr;
    std::int32_t displacement = 0;
    std::memcpy(&displacement, bytes + 1, sizeof(displacement));
    return const_cast<std::uint8_t*>(bytes + 5 + displacement);
}

bool value_text_draw_call_supported() noexcept {
    const void* site = at(g_armada, kValueTextDrawCallSite.rva);
    void* destination = direct_call_destination(site);
    const bool stock = readable_range(
            site, kValueTextDrawCallSite.expected.size()) &&
        std::memcmp(site, kValueTextDrawCallSite.expected.data(),
                    kValueTextDrawCallSite.expected.size()) == 0 &&
        destination == at(
            g_armada, kDisplayInterfaceDrawTextInRectangleRva);
    const bool fleet_ops = !stock &&
        executable_address_in_module(g_fleet_ops, destination);
    if (!stock && !fleet_ops) {
        std::array<std::uint8_t, 5> actual{};
        if (readable_range(site, actual.size())) {
            std::memcpy(actual.data(), site, actual.size());
        }
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "Value text draw call at Armada RVA 0x0010C393 has unsupported bytes %02X %02X %02X %02X %02X (destination=%p)",
            actual[0], actual[1], actual[2], actual[3], actual[4],
            destination);
        log_line(message);
        return false;
    }
    a2fo_identity_value_text_draw_original = destination;
    if (fleet_ops) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "Value text colour bridge will preserve Fleet Operations' live draw handler at %p",
            destination);
        log_line(message);
    }
    return true;
}

bool supported_fleet_ops_craft_class_detour(
    void** destination, std::size_t* patch_length) noexcept {
    void* resolved = existing_detour_destination(
        at(g_armada, kCraftClassConstructorRva), patch_length);
    void* expected = at(g_fleet_ops,
                        kFoCraftClassConstructorHandlerRva);
    const bool supported = resolved == expected &&
        signature_matches(
            g_fleet_ops, kFoCraftClassConstructorHandlerRva,
            kExpectedFoCraftClassConstructorHandler);
    if (destination) *destination = supported ? resolved : nullptr;
    return supported;
}

bool craft_class_constructor_supported() noexcept {
    if (signature_matches(
            g_armada, kCraftClassConstructorRva,
            kExpectedCraftClassConstructor)) {
        return true;
    }
    void* destination = nullptr;
    std::size_t patch_length = 0;
    if (supported_fleet_ops_craft_class_detour(
            &destination, &patch_length)) {
        return true;
    }
    char message[320]{};
    std::snprintf(
        message, sizeof(message),
        "CraftClass constructor has neither the stock prologue nor "
        "Fleet Ops' supported detour (destination=%p, expected=%p)",
        existing_detour_destination(
            at(g_armada, kCraftClassConstructorRva), nullptr),
        at(g_fleet_ops, kFoCraftClassConstructorHandlerRva));
    log_line(message);
    return false;
}

bool preflight_signatures() noexcept {
    bool supported = craft_class_constructor_supported();
    supported = checked_armada_signature(
        "InfoDisplay::UpdateSelected", kSelectedInfoUpdateRva,
        kExpectedSelectedInfoUpdate) && supported;
    supported = checked_armada_signature(
        "InfoDisplay::RenderSelectedBuilder",
        kSelectedBuilderInfoRenderRva,
        kExpectedSelectedInfoRender) && supported;
    supported = checked_armada_signature(
        "InfoDisplay::RenderSelected", kSelectedInfoRenderRva,
        kExpectedSelectedInfoRender) && supported;
    supported = checked_armada_signature(
        "ST3D_Sprite::SetColour", kSpriteSetColourRva,
        kExpectedSpriteSetColour) && supported;
    if (!signature_matches(
            g_fleet_ops, kFoSpriteSetColourRva,
            kExpectedFoSpriteSetColour)) {
        log_line(
            "Fleet Operations sprite-colour helper signature mismatch at RVA 0x001E34B4");
        supported = false;
    }
    if (!signature_matches(
            g_fleet_ops, kFoTechnologyTreeAllowsProjectRva,
            kExpectedFoTechnologyTreeAllowsProject)) {
        log_line(
            "Fleet Operations technology-tree evaluator signature mismatch at RVA 0x00120680");
        supported = false;
    }
    if (!signature_matches(
            g_fleet_ops, kFoTechnologyItemShouldShowButtonRva,
            kExpectedFoTechnologyItemShouldShowButton)) {
        log_line(
            "Fleet Operations buttonHideUnavailable evaluator signature mismatch at RVA 0x0011EB6C");
        supported = false;
    }
    if (!exact_bytes_match(
            g_fleet_ops, kFoSpecialWeaponVisibilityTestRva,
            kExpectedFoSpecialWeaponVisibilityTest)) {
        log_line(
            "ShipSystemIcon special-weapon visibility signature mismatch at FleetOpsHook RVA 0x001ED6D6");
        supported = false;
    }
    auto** ship_system_render_slot = reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(
            at(g_fleet_ops, kFoShipSystemIconVtableRva)) +
        kShipSystemIconRenderVtableOffset);
    if (!readable_range(ship_system_render_slot, sizeof(void*)) ||
        *ship_system_render_slot !=
            at(g_fleet_ops, kFoShipSystemIconRenderRva)) {
        log_line(
            "ShipSystemIcon render vtable signature mismatch at FleetOpsHook RVA 0x0021111C");
        supported = false;
    }
    for (const CheckedCallSite& site : kSystemIconColourCallSites) {
        const void* address = at(g_armada, site.rva);
        if (!readable_range(address, site.expected.size()) ||
            std::memcmp(address, site.expected.data(),
                        site.expected.size()) != 0) {
            char message[256]{};
            std::snprintf(
                message, sizeof(message),
                "SystemIcon colour call signature mismatch at Armada RVA 0x%08lX",
                static_cast<unsigned long>(site.rva));
            log_line(message);
            supported = false;
        }
    }
    for (const CheckedCallSite& site : kWeaponIconColourCallSites) {
        const void* address = at(g_fleet_ops, site.rva);
        if (!readable_range(address, site.expected.size()) ||
            std::memcmp(address, site.expected.data(),
                        site.expected.size()) != 0) {
            char message[256]{};
            std::snprintf(
                message, sizeof(message),
                "ShipSystemIcon colour call signature mismatch at FleetOpsHook RVA 0x%08lX",
                static_cast<unsigned long>(site.rva));
            log_line(message);
            supported = false;
        }
    }
    {
        const CheckedCallSite& site = kSystemValueIconColourCallSite;
        const void* address = at(g_armada, site.rva);
        if (!readable_range(address, site.expected.size()) ||
            std::memcmp(address, site.expected.data(),
                        site.expected.size()) != 0) {
            log_line(
                "System value-icon colour call signature mismatch at Armada RVA 0x000EC748");
            supported = false;
        }
    }
    supported = value_text_draw_call_supported() && supported;
    supported = checked_armada_signature(
        "ParameterDB::Get(DBRectangle)", kParameterDbGetRectangleRva,
        kExpectedParameterDbGetRectangle) && supported;
    supported = checked_armada_signature(
        "ParameterDB::Get(DBColor)", kParameterDbGetColorRva,
        kExpectedParameterDbGetColor) && supported;
    supported = checked_armada_signature(
        "DisplayInterface::DrawText(rectangle)",
        kDisplayInterfaceDrawTextInRectangleRva,
        kExpectedDisplayInterfaceDrawTextInRectangle) && supported;
    supported = checked_armada_signature(
        "GameObjectClass::GetOdfName", kGameObjectClassGetOdfNameRva,
        kExpectedGameObjectClassGetOdfName) && supported;
    supported = readable_range(
        at(g_armada, kParameterDbGetStringVectorRva), 5) && supported;
    supported = readable_range(
        at(g_armada, kParameterDbGetStringRva), 5) && supported;
    supported = readable_range(
        at(g_armada, kParameterDbGetIntRva), 5) && supported;
    supported = readable_range(
        at(g_armada, kEngineOperatorDeleteRva), 5) && supported;
    return supported;
}

bool install_craft_class_constructor_hook(
    const A2FO_ModuleApi* api) noexcept {
    struct Context {
        const A2FO_ModuleApi* api;
        void* site = nullptr;
        void* destination = nullptr;
        std::size_t patch_length = 0;
        std::uint8_t expected_detour[6]{};
    } context{api};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) {
            ctx.site = at(g_armada, kCraftClassConstructorRva);
            return signature_matches(g_armada, kCraftClassConstructorRva,
                kExpectedCraftClassConstructor);
        }
        if (action == 1) {
            return ctx.api->install_inline_hook(ctx.site,
                reinterpret_cast<void*>(&craft_class_constructor_hook),
                sizeof(kExpectedCraftClassConstructor),
                kExpectedCraftClassConstructor,
                &g_craft_class_constructor_hook);
        }
        if (action == 2) {
            g_craft_class_constructor_original =
                g_craft_class_constructor_hook.gateway;
            g_chained_fo_craft_class_constructor = false;
            return g_craft_class_constructor_original != nullptr;
        }
        if (action == 3) {
            return supported_fleet_ops_craft_class_detour(
                    &ctx.destination, &ctx.patch_length) &&
                ctx.patch_length <= 6;
        }
        if (action == 4) {
            std::memcpy(ctx.expected_detour, ctx.site, ctx.patch_length);
        } else if (action == 5) {
            return ctx.api->patch_jump(ctx.site,
                reinterpret_cast<void*>(&craft_class_constructor_hook),
                ctx.expected_detour, ctx.patch_length);
        } else if (action == 6) {
            g_craft_class_constructor_original = ctx.destination;
            g_chained_fo_craft_class_constructor = true;
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_install_constructor(step, &context);
}

bool install_system_icon_colour_hooks(
    const A2FO_ModuleApi* api) noexcept {
    struct Context {
        const A2FO_ModuleApi* api;
        std::array<std::uint8_t, 5> value_text_expected{};
        const void* value_text_site = nullptr;
    } context{api};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        if (action == 0) return ctx.api && ctx.api->patch_call;
        if (action == 1) {
            const CheckedCallSite& site = kSystemIconColourCallSites[index];
            return ctx.api->patch_call(
                at(g_armada, site.rva),
                reinterpret_cast<void*>(
                    &a2fo_identity_system_icon_set_colour_bridge),
                site.expected.data(), site.expected.size());
        }
        if (action == 2) {
            const CheckedCallSite& site = kWeaponIconColourCallSites[index];
            return ctx.api->patch_call(
                at(g_fleet_ops, site.rva),
                reinterpret_cast<void*>(
                    &a2fo_identity_weapon_icon_set_colour_bridge),
                site.expected.data(), site.expected.size());
        }
        if (action == 3) {
            return ctx.api->patch_call(
                at(g_armada, kSystemValueIconColourCallSite.rva),
                reinterpret_cast<void*>(
                    &a2fo_identity_system_text_set_colour_bridge),
                kSystemValueIconColourCallSite.expected.data(),
                kSystemValueIconColourCallSite.expected.size());
        }
        if (action == 4) {
            ctx.value_text_site = at(g_armada, kValueTextDrawCallSite.rva);
            return readable_range(ctx.value_text_site,
                    ctx.value_text_expected.size()) &&
                a2fo_identity_value_text_draw_original;
        }
        if (action == 5) {
            std::memcpy(ctx.value_text_expected.data(), ctx.value_text_site,
                ctx.value_text_expected.size());
        } else if (action == 6) {
            return ctx.api->patch_call(
                const_cast<void*>(ctx.value_text_site),
                reinterpret_cast<void*>(
                    &a2fo_identity_value_text_draw_bridge),
                ctx.value_text_expected.data(),
                ctx.value_text_expected.size());
        }
        return 0;
    };
    return a2fo_rs_stage9_craft_install_icon_hooks(step, &context,
        kSystemIconColourCallSites.size(), kWeaponIconColourCallSites.size());
}

bool install_ship_system_icon_render_policy_hook() noexcept {
    if (!g_fleet_ops) return false;
    auto** slot = reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(
            at(g_fleet_ops, kFoShipSystemIconVtableRva)) +
        kShipSystemIconRenderVtableOffset);
    void* const native_render = at(
        g_fleet_ops, kFoShipSystemIconRenderRva);
    if (!readable_range(slot, sizeof(void*)) ||
        *slot != native_render) {
        return false;
    }
    DWORD old_protect = 0;
    if (!VirtualProtect(
            slot, sizeof(void*), PAGE_READWRITE, &old_protect)) {
        return false;
    }
    void* const previous = InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(slot),
        reinterpret_cast<void*>(&ship_system_icon_render_policy_hook));
    DWORD restored = 0;
    VirtualProtect(slot, sizeof(void*), old_protect, &restored);
    if (previous != native_render) {
        DWORD rollback_protect = 0;
        if (VirtualProtect(
                slot, sizeof(void*), PAGE_READWRITE,
                &rollback_protect)) {
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(slot), previous);
            DWORD rollback_restored = 0;
            VirtualProtect(
                slot, sizeof(void*), rollback_protect,
                &rollback_restored);
        }
        return false;
    }
    g_ship_system_icon_render_original = previous;
    return true;
}

bool install_special_weapon_disabled_presentation_patch() noexcept {
    return patch_exact_bytes(
        g_fleet_ops, kFoSpecialWeaponVisibilityTestRva,
        kExpectedFoSpecialWeaponVisibilityTest,
        kReplacementFoSpecialWeaponVisibilityTest);
}

void install_directional_shield_tooltip_hooks(
    const A2FO_ModuleApi* api) noexcept {
    struct Context { const A2FO_ModuleApi* api; } context{api};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        const auto* api = static_cast<Context*>(opaque)->api;
        if (action == 0) {
            // Genuine component callbacks supersede experimental tooltip paths.
            g_directional_shield_native_tooltip_available = false;
            g_directional_shield_component_tooltips_available = false;
            g_directional_shield_wireframe_tooltips_available = false;
        } else if (action == 1) {
            void* standard_component_vtable = at(
                g_armada, kStandardComponentVtableRva);
            return api && api->install_inline_hook && signature_matches(
                    g_armada, kStandardComponentUpdateRva,
                    kExpectedStandardComponentUpdate) &&
                readable_range(standard_component_vtable, 0x08) &&
                read_at<void*>(standard_component_vtable, 0, nullptr) ==
                    at(g_armada, kStandardComponentTooltipRva) &&
                read_at<void*>(standard_component_vtable, 4, nullptr) ==
                    at(g_armada, kStandardComponentVerboseTooltipRva) &&
                insert_c_string();
        } else if (action == 2) {
            log_line("Armada StandardComponent hover path was unavailable; directional-shield arc tooltips disabled");
        } else if (action == 3) {
            return api->install_inline_hook(
                at(g_armada, kStandardComponentUpdateRva),
                reinterpret_cast<void*>(&standard_component_update_hook),
                sizeof(kExpectedStandardComponentUpdate),
                kExpectedStandardComponentUpdate,
                &g_standard_component_update_hook);
        } else if (action == 4) {
            log_line("Armada StandardComponent update hook could not be installed; directional-shield arc tooltips disabled");
        } else if (action == 5) {
            g_directional_shield_wireframe_tooltips_available = true;
            log_line("Four directional-shield arcs registered through the selected-panel StandardComponent update path");
        }
        return 0;
    };
    a2fo_rs_stage9_craft_install_tooltip_hook(step, &context);
}

bool install_runtime_hooks(const A2FO_ModuleApi* api) noexcept {
    struct Context { const A2FO_ModuleApi* api; } context{api};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        const auto* api = static_cast<Context*>(opaque)->api;
        if (action == 0) {
            return api && api->install_inline_hook && api->patch_jump &&
                api->patch_call && g_armada && g_fleet_ops;
        }
        if (action == 1) return preflight_signatures();
        if (action == 2) {
            log_line("Supported ArmadaL signatures were not found; runtime disabled");
        } else if (action == 3) return install_craft_class_constructor_hook(api);
        else if (action == 4) return install_system_icon_colour_hooks(api);
        else if (action == 5) return api->install_inline_hook(
            at(g_armada, kSelectedInfoUpdateRva),
            reinterpret_cast<void*>(&selected_info_update_hook),
            sizeof(kExpectedSelectedInfoUpdate), kExpectedSelectedInfoUpdate,
            &g_selected_info_update_hook);
        else if (action == 6) return api->install_inline_hook(
            at(g_armada, kSelectedBuilderInfoRenderRva),
            reinterpret_cast<void*>(&selected_builder_info_render_hook),
            sizeof(kExpectedSelectedInfoRender), kExpectedSelectedInfoRender,
            &g_selected_builder_info_render_hook);
        else if (action == 7) return api->install_inline_hook(
            at(g_armada, kSelectedInfoRenderRva),
            reinterpret_cast<void*>(&selected_info_render_hook),
            sizeof(kExpectedSelectedInfoRender), kExpectedSelectedInfoRender,
            &g_selected_info_render_hook);
        else if (action == 8) return a2fo::craft_identity::install_extended_weapon_icons(
            api, g_armada, g_fleet_ops);
        else if (action == 9) return install_ship_system_icon_render_policy_hook();
        else if (action == 10)
            return install_special_weapon_disabled_presentation_patch();
        else if (action == 11) install_directional_shield_tooltip_hooks(api);
        else if (action == 12)
            log_line("A craft identity hook could not be installed; hooks fail closed");
        return 0;
    };
    return a2fo_rs_stage9_craft_install_hooks(step, &context);
}

}  // namespace

extern "C" void __cdecl a2fo_identity_system_icon_set_colour_hook_cpp(
    void* sprite, void* icon, const void* native_colour) noexcept {
    system_icon_set_colour_from_context(
        sprite, icon, static_cast<const Colour*>(native_colour));
}

extern "C" void __cdecl a2fo_identity_weapon_icon_set_colour_hook_cpp(
    void* sprite, void* icon, const void* native_colour) noexcept {
    weapon_icon_set_colour_from_context(
        sprite, icon, static_cast<const Colour*>(native_colour));
}

extern "C" void __cdecl a2fo_identity_system_text_set_colour_hook_cpp(
    void* sprite, void* text, const void* native_colour) noexcept {
    system_text_set_colour_from_context(
        sprite, text, static_cast<const Colour*>(native_colour));
}

extern "C" void __cdecl a2fo_identity_value_text_colour_hook_cpp(
    void* component, void* text_record) noexcept {
    value_text_colour_from_context(component, text_record);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    struct Context {
        const A2FO_ModuleApi* api;
        std::exception_ptr failure;
    } context{api, {}};
    const auto step = +[](void* opaque, std::uint32_t action,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        const auto* api = ctx.api;
        try {
        if (action == 0) {
            return api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
                api->api_version == A2FO_MODULE_API_VERSION && api->log &&
                api->armada_module && api->fleetops_module &&
                api->install_inline_hook && api->patch_jump && api->patch_call &&
                api->extension_root_count && api->extension_root;
        }
        if (action == 1) {
            g_api = api;
            g_armada = static_cast<HMODULE>(api->armada_module());
            g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
            return g_armada && g_fleet_ops;
        }
        if (action == 2) {
            constexpr const char* kPassiveWeaponFields[]{"classLabel"};
            return A2FO_MODULE_API_HAS(api, register_weapon_class_loaded_handler) &&
                api->register_weapon_class_loaded_handler &&
                (api->capabilities & A2FO_CAP_WEAPON_CLASS_LOADED) != 0 &&
                api->register_weapon_class_loaded_handler(
                    kModuleName, kPassiveWeaponFields,
                    static_cast<std::uint32_t>(
                        sizeof(kPassiveWeaponFields) /
                        sizeof(kPassiveWeaponFields[0])),
                    &weapon_class_loaded_handler, nullptr);
        }
        if (action == 3) {
            log_line("Shared WeaponClass loader is unavailable; UtilityWeapon icons cannot use passive neutral presentation");
        } else if (action == 4) load_directional_shield_display_config();
        else if (action == 5) resolve_shield_class_observer();
        else if (action == 6) {
            g_runtime_ready = install_runtime_hooks(api);
            return g_runtime_ready;
        } else if (action == 7) {
            a2fo::object_editor::initialize(
                api, g_armada, &read_editor_identity_defaults);
        } else if (action == 8) {
            log_line("Directional-shield ODF rectangles retain native UI placement; only built-in defaults scale");
        } else if (action == 9) {
            log_line(g_chained_fo_craft_class_constructor
                ? "Selected-panel craft identity runtime initialized and chained "
                  "through Fleet Operations CraftClass construction"
                : "Selected-panel craft identity runtime initialized");
        } else if (action == 10) {
            log_line("Craft identity module loaded with runtime disabled");
        }
        return 0;
        } catch (...) {
            ctx.failure = std::current_exception();
            return 3;
        }
    };
    // Core-owned inline hooks remain installed for process lifetime after failure.
    const bool initialized = a2fo_rs_stage9_craft_module_init(step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
    return initialized;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Process-lifetime hooks are owned by the core and intentionally remain.
}
