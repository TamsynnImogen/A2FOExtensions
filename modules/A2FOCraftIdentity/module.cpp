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
#include "../A2FODirectionalShields/api.hpp"
#include "directional_shield_display_config.hpp"
#include "directional_shield_fill.hpp"
#include "extended_weapon_icons.hpp"
#include "identity_selection.hpp"
#include "object_editor.hpp"
#include "selected_panel_anchor.hpp"
#include "system_icon_state.hpp"

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
#include <fstream>
#include <limits>
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
constexpr char kDirectionalShieldsModuleName[] =
    "A2FODirectionalShields.dll";
constexpr char kDirectionalShieldsIsEnabledExport[] =
    "A2FODirectionalShields_IsEnabled";
constexpr char kDirectionalShieldsGetCurrentExport[] =
    "A2FODirectionalShields_GetCurrent";
constexpr char kDirectionalShieldsGetMaximumExport[] =
    "A2FODirectionalShields_GetMaximum";
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

constexpr std::array<const char*, 4> kDirectionalShieldSpriteNames{{
    "dsb",
    "dsf",
    "dsl",
    "dsr",
}};

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
    Colour special_energy_icon_colour{};
    Colour officer_icon_colour{};
    Colour experience_bar_colour{};
    Colour experience_bar_background_colour{};
};

using EnergyAmountGetter = float (A2FO_CALL *)(void* craft);

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
A2FO_DirectionalShieldsIsEnabledFn g_directional_shields_is_enabled = nullptr;
A2FO_DirectionalShieldsGetValueFn g_directional_shields_get_current = nullptr;
A2FO_DirectionalShieldsGetValueFn g_directional_shields_get_maximum = nullptr;
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
    void* database = nullptr;
    std::array<void*, 4> sprites{};
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
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

bool read_small_art_config(
    const std::string& path, std::string& contents) {
    contents.clear();
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
    contents = stream.str();
    return input.good() || input.eof();
}

void load_directional_shield_display_config() {
    using a2fo::craft_identity::parse_directional_shield_display_config;
    using a2fo::craft_identity::valid_directional_shield_position_mapping;

    g_directional_shield_display_config = {};
    const std::uint32_t root_count = g_api->extension_root_count();
    if (root_count > 4096) {
        log_line("Extension-root count is invalid; directional-shield ART settings retain defaults");
        return;
    }
    for (std::uint32_t index = 0; index < root_count; ++index) {
        const char* root = g_api->extension_root(index);
        if (!root || !*root) continue;
        const std::string path = join_path(root, kArtConfigFileName);
        std::string contents;
        if (!read_small_art_config(path, contents)) continue;
        const auto report = parse_directional_shield_display_config(
            contents, &g_directional_shield_display_config);
        if (report.valid_assignments != 0) {
            char message[384]{};
            std::snprintf(
                message, sizeof(message),
                "Applied %u directional-shield ART assignment(s) from %s",
                report.valid_assignments, path.c_str());
            log_line(message);
        }
        if (report.invalid_assignments != 0) {
            char message[384]{};
            std::snprintf(
                message, sizeof(message),
                "Ignored %u invalid directional-shield ART assignment(s) in %s",
                report.invalid_assignments, path.c_str());
            log_line(message);
        }
    }

    auto& config = g_directional_shield_display_config;
    if (config.position_mapping_configured &&
        !valid_directional_shield_position_mapping(config)) {
        config.position_mapping_configured = false;
        log_line("Directional-shield ART positions contain duplicates; existing ODF positions retained");
    }
    char message[256]{};
    std::snprintf(
        message, sizeof(message),
        "Directional-shield ART display: mode=%d positions=%s F=%d A=%d P=%d S=%d",
        config.display_mode,
        config.position_mapping_configured ? "configured" : "ODF",
        config.facing_positions[0], config.facing_positions[1],
        config.facing_positions[2], config.facing_positions[3]);
    log_line(message);
}

bool resolve_energy_amount_api() noexcept {
    if (g_energy_api_resolution_attempted) {
        return g_get_photon_torpedoes && g_get_quantum_torpedoes &&
            g_get_maximum_photon_torpedoes &&
            g_get_maximum_quantum_torpedoes;
    }
    g_energy_api_resolution_attempted = true;
    HMODULE energy = GetModuleHandleA(kEnergySystemsModuleName);
    if (!energy) return false;

    FARPROC photon = GetProcAddress(energy, kGetPhotonTorpedoesExport);
    FARPROC quantum = GetProcAddress(energy, kGetQuantumTorpedoesExport);
    FARPROC shuttle_craft = GetProcAddress(
        energy, kGetShuttleCraftExport);
    FARPROC maximum_photon = GetProcAddress(
        energy, kGetMaximumPhotonTorpedoesExport);
    FARPROC maximum_quantum = GetProcAddress(
        energy, kGetMaximumQuantumTorpedoesExport);
    FARPROC maximum_shuttle_craft = GetProcAddress(
        energy, kGetMaximumShuttleCraftExport);
    FARPROC photon_reload = GetProcAddress(
        energy, kGetPhotonTorpedoReloadSecondsExport);
    FARPROC quantum_reload = GetProcAddress(
        energy, kGetQuantumTorpedoReloadSecondsExport);
    FARPROC shuttle_craft_reload = GetProcAddress(
        energy, kGetShuttleCraftReloadSecondsExport);
    static_assert(sizeof(photon) == sizeof(g_get_photon_torpedoes),
                  "unexpected function-pointer size");
    std::memcpy(&g_get_photon_torpedoes, &photon,
                sizeof(g_get_photon_torpedoes));
    std::memcpy(&g_get_quantum_torpedoes, &quantum,
                sizeof(g_get_quantum_torpedoes));
    std::memcpy(&g_get_shuttle_craft, &shuttle_craft,
                sizeof(g_get_shuttle_craft));
    std::memcpy(&g_get_maximum_photon_torpedoes, &maximum_photon,
                sizeof(g_get_maximum_photon_torpedoes));
    std::memcpy(&g_get_maximum_quantum_torpedoes, &maximum_quantum,
                sizeof(g_get_maximum_quantum_torpedoes));
    std::memcpy(&g_get_maximum_shuttle_craft, &maximum_shuttle_craft,
                sizeof(g_get_maximum_shuttle_craft));
    std::memcpy(&g_get_photon_torpedo_reload_seconds, &photon_reload,
                sizeof(g_get_photon_torpedo_reload_seconds));
    std::memcpy(&g_get_quantum_torpedo_reload_seconds, &quantum_reload,
                sizeof(g_get_quantum_torpedo_reload_seconds));
    std::memcpy(&g_get_shuttle_craft_reload_seconds, &shuttle_craft_reload,
                sizeof(g_get_shuttle_craft_reload_seconds));
    const bool ready = g_get_photon_torpedoes &&
        g_get_quantum_torpedoes && g_get_maximum_photon_torpedoes &&
        g_get_maximum_quantum_torpedoes;
    log_line(ready
        ? "Ammunition display linked to A2FOEnergySystems value exports"
        : "A2FOEnergySystems value exports unavailable; ammunition display disabled");
    if (ready && (!g_get_photon_torpedo_reload_seconds ||
                  !g_get_quantum_torpedo_reload_seconds)) {
        log_line("Ammunition reload-time exports unavailable; value display mode 2 will show Resupply while below full");
    }
    if (ready && (!g_get_shuttle_craft ||
                  !g_get_maximum_shuttle_craft)) {
        log_line("Shuttle Craft value exports unavailable; Photon and Quantum display remains active");
    } else if (ready && !g_get_shuttle_craft_reload_seconds) {
        log_line("Shuttle Craft reload-time export unavailable; value display mode 2 will show Resupply while below full");
    }
    return ready;
}

bool resolve_directional_shields_api() noexcept {
    if (g_directional_shields_api_resolution_attempted) {
        return g_directional_shields_is_enabled &&
            g_directional_shields_get_current &&
            g_directional_shields_get_maximum;
    }
    g_directional_shields_api_resolution_attempted = true;
    HMODULE directional = GetModuleHandleA(kDirectionalShieldsModuleName);
    if (!directional) return false;

    FARPROC is_enabled = GetProcAddress(
        directional, kDirectionalShieldsIsEnabledExport);
    FARPROC get_current = GetProcAddress(
        directional, kDirectionalShieldsGetCurrentExport);
    FARPROC get_maximum = GetProcAddress(
        directional, kDirectionalShieldsGetMaximumExport);
    static_assert(sizeof(is_enabled) ==
                      sizeof(g_directional_shields_is_enabled),
                  "unexpected function-pointer size");
    std::memcpy(&g_directional_shields_is_enabled, &is_enabled,
                sizeof(g_directional_shields_is_enabled));
    std::memcpy(&g_directional_shields_get_current, &get_current,
                sizeof(g_directional_shields_get_current));
    std::memcpy(&g_directional_shields_get_maximum, &get_maximum,
                sizeof(g_directional_shields_get_maximum));
    const bool ready = g_directional_shields_is_enabled &&
        g_directional_shields_get_current &&
        g_directional_shields_get_maximum;
    log_line(ready
        ? "Directional-shield display linked to facing-value exports"
        : "Directional-shield value exports unavailable; facing display disabled");
    return ready;
}

void resolve_shield_class_observer() noexcept {
    HMODULE shields = GetModuleHandleA(kAlwaysShowShieldsModuleName);
    FARPROC exported = shields
        ? GetProcAddress(shields, kShieldClassObserverExport)
        : nullptr;
    static_assert(sizeof(exported) == sizeof(g_shield_class_observer),
                  "unexpected function-pointer size");
    std::memcpy(&g_shield_class_observer, &exported,
                sizeof(g_shield_class_observer));
    if (g_shield_class_observer) {
        log_line(
            "Shield class registration linked through "
            "A2FOAlwaysShowShields");
    }
}

void resolve_nebula_class_observer() noexcept {
    HMODULE renderer = GetModuleHandleA(kNebulaRendererModuleName);
    FARPROC exported = renderer
        ? GetProcAddress(renderer, kNebulaClassObserverExport)
        : nullptr;
    static_assert(sizeof(exported) == sizeof(g_nebula_class_observer),
                  "unexpected function-pointer size");
    std::memcpy(&g_nebula_class_observer, &exported,
                sizeof(g_nebula_class_observer));
    if (g_nebula_class_observer) {
        log_line(
            "Emissive-map class registration linked through "
            "A2FONebulaRenderer");
    }
}

void resolve_texture_variants_class_observer() noexcept {
    HMODULE variants = GetModuleHandleA(kTextureVariantsModuleName);
    FARPROC exported = variants
        ? GetProcAddress(variants, kTextureVariantsClassObserverExport)
        : nullptr;
    static_assert(
        sizeof(exported) == sizeof(g_texture_variants_class_observer),
        "unexpected function-pointer size");
    std::memcpy(&g_texture_variants_class_observer, &exported,
                sizeof(g_texture_variants_class_observer));
    if (g_texture_variants_class_observer) {
        log_line(
            "Subsystem mesh class registration linked through "
            "A2FOTextureVariants");
    }
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
    for (std::uint32_t index = 0; index < field_count; ++index) {
        const A2FO_OdfFieldView& field = fields[index];
        if (!field.name.data || field.name.size != name_size ||
            _strnicmp(field.name.data, name, name_size) != 0 ||
            (!field.value.data && field.value.size != 0)) {
            continue;
        }
        *output = std::string_view(
            field.value.data ? field.value.data : "", field.value.size);
        return true;
    }
    return false;
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
    return a2fo::craft_identity::weapon_icon_presentation(
        kind, technology.state, technology.hide_when_unavailable);
}

void trim_string(std::string* value) {
    if (!value) return;
    std::size_t begin = 0;
    while (begin < value->size() && std::isspace(
               static_cast<unsigned char>((*value)[begin]))) {
        ++begin;
    }
    std::size_t end = value->size();
    while (end > begin && std::isspace(
               static_cast<unsigned char>((*value)[end - 1]))) {
        --end;
    }
    *value = value->substr(begin, end - begin);
}

bool copy_native_string(const char* source, std::string* output) {
    if (!source || !output) return false;
    std::size_t length = 0;
    while (length < kMaximumIdentityLength) {
        if (!readable_range(source + length, 1)) return false;
        if (source[length] == '\0') {
            output->assign(source, length);
            trim_string(output);
            return true;
        }
        ++length;
    }
    return false;
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
    for (std::size_t index = 0; index < count; ++index) {
        if (values->begin[index]) engine_delete(values->begin[index]);
    }
    if (values->begin) engine_delete(values->begin);
    *values = NativeStringVector{};
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
            for (std::size_t index = 0; index < count; ++index) {
                copy_native_string(native_values.begin[index],
                                   &(*output)[index]);
            }
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
    while (end && *end != '\0' && std::isspace(
               static_cast<unsigned char>(*end))) {
        ++end;
    }
    if (!end || *end != '\0' || parsed < 1 || parsed > 2) return false;
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
    if (!object_class || !parameter_db) return;
    SelectedStatusUiPolicy policy{};
    bool any_found = read_parameter_string(
        parameter_db, kShieldTooltipCommand,
        &policy.shield_tooltip);
    any_found = read_parameter_string(
        parameter_db, kShieldVerboseTooltipCommand,
        &policy.shield_verbose_tooltip) || any_found;
    if (!any_found) {
        g_selected_status_ui_policies.erase(object_class);
        return;
    }
    try {
        g_selected_status_ui_policies[object_class] = std::move(policy);
    } catch (...) {
        g_selected_status_ui_policies.erase(object_class);
        log_line("Could not retain selected-status tooltip policy");
    }
}

void register_ammunition_ui_policy(
    void* object_class, void* parameter_db) noexcept {
    if (!object_class || !parameter_db) return;
    AmmunitionUiPolicy policy{};
    bool any_found = false;
    for (std::size_t index = 0; index < policy.stores.size(); ++index) {
        AmmunitionPresentation& store = policy.stores[index];
        int numeric_mode = static_cast<int>(store.display_mode);
        if (read_parameter_int(
                parameter_db, kAmmunitionDisplayModeCommands[index],
                &numeric_mode)) {
            if (numeric_mode < static_cast<int>(AmmunitionDisplayMode::text) ||
                numeric_mode > static_cast<int>(AmmunitionDisplayMode::icon)) {
                log_line(std::string("Ignored invalid ") +
                         kAmmunitionDisplayModeCommands[index] +
                         "; use 1 for text or 2 for icon");
            } else {
                store.display_mode =
                    static_cast<AmmunitionDisplayMode>(numeric_mode);
                any_found = true;
            }
        } else {
            // Also accept a quoted numeric value for compatibility with the
            // initial development build which queried this field as text.
            std::string mode;
            if (read_parameter_string(
                    parameter_db, kAmmunitionDisplayModeCommands[index],
                    &mode)) {
                if (!parse_display_mode(mode, &store.display_mode)) {
                    log_line(std::string("Ignored invalid ") +
                             kAmmunitionDisplayModeCommands[index] +
                             "; use 1 for text or 2 for icon");
                } else {
                    any_found = true;
                }
            }
        }
        int value_mode = static_cast<int>(store.value_display_mode);
        if (read_parameter_int(
                parameter_db,
                kAmmunitionValueDisplayModeCommands[index],
                &value_mode)) {
            if (value_mode <
                    static_cast<int>(AmmunitionValueDisplayMode::percent) ||
                value_mode >
                    static_cast<int>(AmmunitionValueDisplayMode::bar)) {
                log_line(std::string("Ignored invalid ") +
                         kAmmunitionValueDisplayModeCommands[index] +
                         "; use 0 for percent, 1 for current/maximum, 2 for reload status, or 3 for a capacity bar");
            } else {
                store.value_display_mode =
                    static_cast<AmmunitionValueDisplayMode>(value_mode);
                any_found = true;
            }
        }
        any_found = read_parameter_string(
            parameter_db, kAmmunitionLabelCommands[index],
            &store.label) || any_found;
        any_found = read_parameter_string(
            parameter_db, kAmmunitionTooltipCommands[index],
            &store.tooltip) || any_found;
        any_found = read_parameter_string(
            parameter_db, kAmmunitionVerboseTooltipCommands[index],
            &store.verbose_tooltip) || any_found;
        any_found = read_parameter_string(
            parameter_db, kAmmunitionIconCommands[index],
            &store.icon) || any_found;
        RawRectangle icon_position{};
        if (read_ui_rectangle(
                parameter_db, kAmmunitionIconPositionCommands[index],
                &icon_position)) {
            if (icon_position.width <= 0 || icon_position.height <= 0) {
                log_line(std::string("Ignored invalid ") +
                         kAmmunitionIconPositionCommands[index] +
                         "; width and height must be positive");
            } else {
                store.icon_position_found = true;
                store.icon_position = icon_position;
                any_found = true;
            }
        }
    }
    if (!any_found) {
        g_ammunition_ui_policies.erase(object_class);
        return;
    }
    try {
        g_ammunition_ui_policies[object_class] = std::move(policy);
        const std::string odf = class_odf_name(object_class);
        const AmmunitionUiPolicy& retained =
            g_ammunition_ui_policies[object_class];
        char message[640]{};
        std::snprintf(
            message, sizeof(message),
            "Registered ammunition UI on '%s': Photon=%s/value%d%s Quantum=%s/value%d%s Shuttle Craft=%s/value%d%s",
            odf.empty() ? "<unknown>" : odf.c_str(),
            retained.stores[0].display_mode == AmmunitionDisplayMode::icon
                ? "icon" : "text",
            static_cast<int>(retained.stores[0].value_display_mode),
            retained.stores[0].label.empty() ? "" : "/label",
            retained.stores[1].display_mode == AmmunitionDisplayMode::icon
                ? "icon" : "text",
            static_cast<int>(retained.stores[1].value_display_mode),
            retained.stores[1].label.empty() ? "" : "/label",
            retained.stores[2].display_mode == AmmunitionDisplayMode::icon
                ? "icon" : "text",
            static_cast<int>(retained.stores[2].value_display_mode),
            retained.stores[2].label.empty() ? "" : "/label");
        log_line(message);
    } catch (...) {
        g_ammunition_ui_policies.erase(object_class);
        log_line("Could not retain ammunition UI policy");
    }
}

void register_directional_shield_ui_policy(
    void* object_class, void* parameter_db) noexcept {
    if (!object_class || !parameter_db) return;
    DirectionalShieldUiPolicy policy{};
    bool any_found = false;
    int value_display_mode = 0;
    if (read_parameter_int(
            parameter_db, kDirectionalShieldValueDisplayModeCommand,
            &value_display_mode)) {
        if (value_display_mode < 0 || value_display_mode > 2) {
            log_line(std::string("Ignored invalid ") +
                     kDirectionalShieldValueDisplayModeCommand +
                     "; expected 0, 1, or 2");
        } else {
            policy.value_display_mode_found = true;
            policy.value_display_mode = static_cast<
                a2fo::craft_identity::DirectionalShieldValueDisplayMode>(
                    value_display_mode);
            any_found = true;
        }
    }
    for (std::size_t index = 0;
         index < kDirectionalShieldPositionCommands.size(); ++index) {
        RawRectangle rectangle{};
        if (!read_ui_rectangle(
                parameter_db, kDirectionalShieldPositionCommands[index],
                &rectangle)) {
            continue;
        }
        if (rectangle.width <= 0 || rectangle.height <= 0) {
            log_line(std::string("Ignored invalid ") +
                     kDirectionalShieldPositionCommands[index] +
                     "; width and height must be positive");
            continue;
        }
        policy.position_found[index] = true;
        policy.positions[index] = rectangle;
        any_found = true;
    }
    if (!any_found) {
        g_directional_shield_ui_policies.erase(object_class);
        return;
    }
    try {
        g_directional_shield_ui_policies[object_class] = policy;
        const std::string odf = class_odf_name(object_class);
        char message[384]{};
        std::snprintf(
            message, sizeof(message),
            "Registered directional-shield UI on '%s': values=%s F=%s A=%s P=%s S=%s",
            odf.empty() ? "<unknown>" : odf.c_str(),
            policy.value_display_mode_found
                ? (value_display_mode == 0 ? "off" :
                   value_display_mode == 1 ? "percent" : "amount")
                : "legacy",
            policy.position_found[0] ? "ODF" : "default",
            policy.position_found[1] ? "ODF" : "default",
            policy.position_found[2] ? "ODF" : "default",
            policy.position_found[3] ? "ODF" : "default");
        log_line(message);
    } catch (...) {
        g_directional_shield_ui_policies.erase(object_class);
        log_line("Could not retain directional-shield UI policy");
    }
}

void register_class_policy(void* object_class, void* parameter_db) noexcept {
    if (!g_runtime_ready || !object_class || !parameter_db) return;
    register_directional_shield_ui_policy(object_class, parameter_db);
    register_ammunition_ui_policy(object_class, parameter_db);
    register_selected_status_ui_policy(object_class, parameter_db);
    ClassIdentityPolicy policy{};
    read_identity_list(parameter_db, kCraftNameCommand,
                       &policy.craft_names);
    read_identity_list(parameter_db, kCaptainCommand,
                       &policy.captain_names);
    read_identity_list(parameter_db, kRegistryCommand,
                       &policy.craft_registries);
    if (policy.captain_names.empty() && policy.craft_registries.empty()) {
        return;
    }
    try {
        policy.odf_name = class_odf_name(object_class);
        const std::size_t craft_name_count = policy.craft_names.size();
        const std::size_t captain_count = policy.captain_names.size();
        const std::size_t registry_count = policy.craft_registries.size();
        const std::string odf = policy.odf_name;
        g_class_policies[object_class] = std::move(policy);
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "Registered identity rows on '%s': %lu craft name%s, "
            "%lu captain%s, %lu registr%s",
            odf.empty() ? "<unknown>" : odf.c_str(),
            static_cast<unsigned long>(craft_name_count),
            craft_name_count == 1 ? "" : "s",
            static_cast<unsigned long>(captain_count),
            captain_count == 1 ? "" : "s",
            static_cast<unsigned long>(registry_count),
            registry_count == 1 ? "y" : "ies");
        log_line(message);
        if (craft_name_count == 0) {
            log_line(std::string("Identity rows on '") +
                     (odf.empty() ? "<unknown>" : odf) +
                     "' cannot be selected without possibleCraftNames");
        } else if ((!g_class_policies[object_class].captain_names.empty() &&
                    captain_count != craft_name_count) ||
                   (!g_class_policies[object_class].craft_registries.empty() &&
                    registry_count != craft_name_count)) {
            log_line(std::string("Identity row-count mismatch on '") +
                     (odf.empty() ? "<unknown>" : odf) +
                     "'; out-of-range companion rows will remain blank");
        }
    } catch (...) {
        log_line("Could not retain a craft identity class policy");
    }
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
    if (handle == 0 || !object_class || (craft_name_index < 0 && !has_manual)) return nullptr;

    const auto policy = g_class_policies.find(object_class);
    if (policy == g_class_policies.end() && !has_manual) {
        g_craft_identities.erase(handle);
        return nullptr;
    }
    const auto present = g_craft_identities.find(handle);
    if (present != g_craft_identities.end() &&
        present->second.object_class == object_class &&
        present->second.craft_name_index == craft_name_index &&
        present->second.editor_revision == editor_revision) {
        return &present->second;
    }

    try {
        CraftIdentity identity{};
        identity.object_class = object_class;
        identity.craft_name_index = craft_name_index;
        identity.editor_revision = editor_revision;
        static const ClassIdentityPolicy empty_policy{};
        const ClassIdentityPolicy& class_policy = policy == g_class_policies.end()
            ? empty_policy : policy->second;
        std::size_t aligned_index = 0;
        if (a2fo::craft_identity::aligned_identity_index(
                craft_name_index, class_policy.captain_names.size(),
                &aligned_index)) {
            identity.captain_name =
                class_policy.captain_names[aligned_index];
        }
        if (a2fo::craft_identity::aligned_identity_index(
                craft_name_index, class_policy.craft_registries.size(),
                &aligned_index)) {
            identity.craft_registry =
                class_policy.craft_registries[aligned_index];
        }
        if (manual && (manual->flags & a2fo::object_editor::kCaptain))
            identity.captain_name = manual->captain.data();
        if (manual && (manual->flags & a2fo::object_editor::kRegistry))
            identity.craft_registry = manual->registry.data();
        auto inserted = g_craft_identities.insert_or_assign(
            handle, std::move(identity));
        const LONG report = InterlockedIncrement(&g_assignment_report_count);
        if (report <= 32) {
            char message[512]{};
            std::snprintf(
                message, sizeof(message),
                "Aligned craft %lu from '%s' to native name row %ld "
                "('%s'): captain='%s', registry='%s'",
                static_cast<unsigned long>(handle),
                class_policy.odf_name.empty()
                    ? "<unknown>" : class_policy.odf_name.c_str(),
                static_cast<long>(craft_name_index),
                static_cast<std::size_t>(craft_name_index) <
                        class_policy.craft_names.size()
                    ? class_policy.craft_names[
                          static_cast<std::size_t>(craft_name_index)].c_str()
                    : "<out-of-range>",
                inserted.first->second.captain_name.empty()
                    ? "<none>" : inserted.first->second.captain_name.c_str(),
                inserted.first->second.craft_registry.empty()
                    ? "<none>" : inserted.first->second.craft_registry.c_str());
            log_line(message);
        } else if (report == 33) {
            log_line("Further per-craft identity assignment reports suppressed");
        }
        return &inserted.first->second;
    } catch (...) {
        g_craft_identities.erase(handle);
        log_line("Could not allocate per-craft identity state");
        return nullptr;
    }
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
    g_panel_styles = {};
    if (!db) return;
    for (std::size_t panel = 0; panel < 2; ++panel) {
        if (panel == 1) g_panel_styles[1] = g_panel_styles[0];
        for (std::size_t i = 0; i < kPanelElementNames.size(); ++i) {
            auto& style = g_panel_styles[panel][i];
            const std::string prefix = std::string(panel ? "infoBuild" : "infoSingle") +
                kPanelElementNames[i];
            RawRectangle area{};
            if (read_ui_rectangle(db, (prefix + "Area").c_str(), &area) &&
                area.x >= -32768 && area.x <= 32767 &&
                area.y >= -32768 && area.y <= 32767 &&
                area.width >= 0 && area.width <= 32767 &&
                area.height >= 0 && area.height <= 32767) {
                // Zero-size rectangles intentionally hide just this element.
                style.area = area;
                style.area_found = true;
            }
            const auto colour = [&](const char* suffix, bool& found, Colour& target) {
                Colour value{};
                if (read_ui_colour(db, (prefix + suffix).c_str(), &value) &&
                    std::isfinite(value.red) && std::isfinite(value.green) &&
                    std::isfinite(value.blue)) {
                    target = value;
                    found = true;
                }
            };
            colour("Color", style.colour_found, style.colour);
            colour("LowColor", style.low_found, style.low);
            colour("CriticalColor", style.critical_found, style.critical);
            colour("BackgroundColor", style.background_found, style.background);
            std::string value;
            if (read_parameter_string(db, (prefix + "Sprite").c_str(), &value))
                style.sprite = value;
            if (read_parameter_string(db, (prefix + "BackgroundSprite").c_str(), &value))
                style.background_sprite = value;
        }
    }
}

void refresh_ui_configuration(void* parameter_db) noexcept {
    // Armada rebuilds GUI and ST3D sprite objects between missions. The old
    // pointers remain readable long enough to look plausible, but their
    // internal frame list has already been released. Drop every presentation
    // cache whenever the companion GUI database changes.
    if (g_ui_configuration.loaded &&
        g_ui_configuration.parameter_db != parameter_db) {
        reset_directional_shield_sprite_runtime();
    }
    UiConfiguration loaded{};
    loaded.parameter_db = parameter_db;
    loaded.loaded = true;
    if (parameter_db) {
        loaded.single_name_rectangle_found = read_ui_rectangle(
            parameter_db, "infoSingleNameTextArea",
            &loaded.single_name_rectangle);
        loaded.single_class_rectangle_found = read_ui_rectangle(
            parameter_db, "infoSingleClassTextArea",
            &loaded.single_class_rectangle);
        loaded.builder_name_rectangle_found = read_ui_rectangle(
            parameter_db, "infoBuildName",
            &loaded.builder_name_rectangle);
        if (!loaded.builder_name_rectangle_found) {
            loaded.builder_name_rectangle_found = read_ui_rectangle(
                parameter_db, "infoBuildNameTextArea",
                &loaded.builder_name_rectangle);
        }
        loaded.builder_class_rectangle_found = read_ui_rectangle(
            parameter_db, "infoBuildClass",
            &loaded.builder_class_rectangle);
        if (!loaded.builder_class_rectangle_found) {
            loaded.builder_class_rectangle_found = read_ui_rectangle(
                parameter_db, "infoBuildClassTextArea",
                &loaded.builder_class_rectangle);
        }
        loaded.captain_rectangle_found = read_ui_rectangle(
            parameter_db, "infoSingleCaptainTextArea",
            &loaded.captain_rectangle);
        if (!loaded.captain_rectangle_found) {
            loaded.captain_rectangle_found = read_ui_rectangle(
                parameter_db, "captainName", &loaded.captain_rectangle);
        }
        loaded.registry_rectangle_found = read_ui_rectangle(
            parameter_db, "infoSingleRegistryTextArea",
            &loaded.registry_rectangle);
        if (!loaded.registry_rectangle_found) {
            loaded.registry_rectangle_found = read_ui_rectangle(
                parameter_db, "shipRegistry", &loaded.registry_rectangle);
        }
        loaded.photon_rectangle_found = read_ui_rectangle(
            parameter_db, "infoSinglePhotonTorpedoesTextArea",
            &loaded.photon_rectangle);
        loaded.quantum_rectangle_found = read_ui_rectangle(
            parameter_db, "infoSingleQuantumTorpedoesTextArea",
            &loaded.quantum_rectangle);
        loaded.shuttle_craft_rectangle_found = read_ui_rectangle(
            parameter_db, "infoSingleShuttleCraftTextArea",
            &loaded.shuttle_craft_rectangle);
        loaded.directional_forward_aft_rectangle_found = read_ui_rectangle(
            parameter_db,
            "infoSingleDirectionalShieldsForwardAftTextArea",
            &loaded.directional_forward_aft_rectangle);
        loaded.directional_port_starboard_rectangle_found = read_ui_rectangle(
            parameter_db,
            "infoSingleDirectionalShieldsPortStarboardTextArea",
            &loaded.directional_port_starboard_rectangle);
        loaded.directional_graphic_rectangle_found = read_ui_rectangle(
            parameter_db,
            "infoSingleDirectionalShieldsGraphicArea",
            &loaded.directional_graphic_rectangle);
        for (std::size_t index = 0;
             index < kDirectionalShieldValueRectangleCommands.size();
             ++index) {
            loaded.directional_value_rectangle_found[index] =
                read_ui_rectangle(
                    parameter_db,
                    kDirectionalShieldValueRectangleCommands[index],
                    &loaded.directional_value_rectangles[index]);
        }
        loaded.shield_bar_rectangle_found = read_ui_rectangle(
            parameter_db, "infoSingleShieldBarArea",
            &loaded.shield_bar_rectangle);
        loaded.builder_shield_bar_rectangle_found = read_ui_rectangle(
            parameter_db, "infoBuildShieldBar", &loaded.builder_shield_bar_rectangle);
        loaded.experience_bar_rectangle_found = read_ui_rectangle(
            parameter_db, "infoSingleExperienceBarArea",
            &loaded.experience_bar_rectangle);
        loaded.shared_text_colour_found = read_ui_colour(
            parameter_db, "infoTextColor", &loaded.shared_text_colour);
        loaded.ship_name_colour_found = read_ui_colour(
            parameter_db, "shipNameColor", &loaded.ship_name_colour);
        loaded.captain_colour_found = read_ui_colour(
            parameter_db, "captainNameColor", &loaded.captain_colour);
        loaded.registry_colour_found = read_ui_colour(
            parameter_db, "shipRegistryColor", &loaded.registry_colour);
        loaded.photon_colour_found = read_ui_colour(
            parameter_db, "photonTorpedoColor", &loaded.photon_colour);
        loaded.quantum_colour_found = read_ui_colour(
            parameter_db, "quantumTorpedoColor", &loaded.quantum_colour);
        loaded.shuttle_craft_colour_found = read_ui_colour(
            parameter_db, "shuttleCraftColor",
            &loaded.shuttle_craft_colour);
        loaded.photon_low_colour_found = read_ui_colour(
            parameter_db, "photonTorpedoLowColor",
            &loaded.photon_low_colour);
        loaded.quantum_low_colour_found = read_ui_colour(
            parameter_db, "quantumTorpedoLowColor",
            &loaded.quantum_low_colour);
        loaded.shuttle_craft_low_colour_found = read_ui_colour(
            parameter_db, "shuttleCraftLowColor",
            &loaded.shuttle_craft_low_colour);
        loaded.photon_critical_colour_found = read_ui_colour(
            parameter_db, "photonTorpedoCriticalColor",
            &loaded.photon_critical_colour);
        loaded.quantum_critical_colour_found = read_ui_colour(
            parameter_db, "quantumTorpedoCriticalColor",
            &loaded.quantum_critical_colour);
        loaded.shuttle_craft_critical_colour_found = read_ui_colour(
            parameter_db, "shuttleCraftCriticalColor",
            &loaded.shuttle_craft_critical_colour);
        loaded.directional_shield_colour_found = read_ui_colour(
            parameter_db, "directionalShieldColor",
            &loaded.directional_shield_colour);
        loaded.directional_shield_low_colour_found = read_ui_colour(
            parameter_db, "directionalShieldLowColor",
            &loaded.directional_shield_low_colour);
        loaded.directional_shield_critical_colour_found = read_ui_colour(
            parameter_db, "directionalShieldCriticalColor",
            &loaded.directional_shield_critical_colour);
        loaded.directional_shield_value_colour_found = read_ui_colour(
            parameter_db, "directionalShieldValueColor",
            &loaded.directional_shield_value_colour);
        loaded.directional_shield_value_low_colour_found = read_ui_colour(
            parameter_db, "directionalShieldValueLowColor",
            &loaded.directional_shield_value_low_colour);
        loaded.directional_shield_value_critical_colour_found =
            read_ui_colour(
                parameter_db, "directionalShieldValueCriticalColor",
                &loaded.directional_shield_value_critical_colour);
        for (std::size_t index = 0;
             index < kSystemIconColourCommands.size(); ++index) {
            loaded.system_icon_colour_found[index] = read_ui_colour(
                parameter_db, kSystemIconColourCommands[index],
                &loaded.system_icon_colours[index]);
            loaded.weapon_icon_colour_found[index] = read_ui_colour(
                parameter_db, kWeaponIconColourCommands[index],
                &loaded.weapon_icon_colours[index]);
        }
        loaded.passive_weapon_icon_colour_found = read_ui_colour(
            parameter_db, "passiveWeaponIconColor",
            &loaded.passive_weapon_icon_colour);
        loaded.special_energy_icon_colour_found = read_ui_colour(
            parameter_db, "specialEnergyIconColor",
            &loaded.special_energy_icon_colour);
        loaded.officer_icon_colour_found = read_ui_colour(
            parameter_db, "officerIconColor",
            &loaded.officer_icon_colour);
        loaded.experience_bar_colour_found = read_ui_colour(
            parameter_db, "experienceBarColor",
            &loaded.experience_bar_colour);
        loaded.experience_bar_background_colour_found = read_ui_colour(
            parameter_db, "experienceBarBackgroundColor",
            &loaded.experience_bar_background_colour);
    }
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
    const ValueComponentKind kind = value_component_kind(component);
    if (kind_output) *kind_output = kind;
    if (kind == ValueComponentKind::system) {
        return configured_system_colour(
            read_at<void*>(component, kSystemTextCraftOffset, nullptr),
            read_at<std::int32_t>(
                component, kSystemTextIndexOffset, -1),
            output);
    }
    if (kind == ValueComponentKind::crew ||
        kind == ValueComponentKind::mouseover_status) {
        return configured_percentage_colour(component, output);
    }
    if (kind == ValueComponentKind::special_energy &&
        g_ui_configuration.special_energy_icon_colour_found && output) {
        *output = g_ui_configuration.special_energy_icon_colour;
        return true;
    }
    if (kind == ValueComponentKind::officer &&
        g_ui_configuration.officer_icon_colour_found && output) {
        *output = g_ui_configuration.officer_icon_colour;
        return true;
    }
    return false;
}

void system_colour_set_colour_from_context(
    void* sprite, void* craft, std::int32_t system_index,
    const Colour* native_colour, LONG* report_count,
    const char* report_message) noexcept {
    const Colour* applied_colour = native_colour;
    Colour configured_colour{};
    Colour tinted{};
    bool applied = false;
    if (g_runtime_ready && craft) {
        void* parameter_db = gui_parameter_db();
        if (!g_ui_configuration.loaded ||
            g_ui_configuration.parameter_db != parameter_db) {
            refresh_ui_configuration(parameter_db);
        }
        applied = configured_system_colour(
            craft, system_index, &configured_colour);
    }
    if (applied && native_colour &&
        readable_range(native_colour, sizeof(*native_colour))) {
        Colour native{};
        std::memcpy(&native, native_colour, sizeof(native));
        const auto result = a2fo::craft_identity::tint_system_icon_colour(
            {{native.red, native.green, native.blue}},
            {{configured_colour.red, configured_colour.green,
              configured_colour.blue}});
        tinted = Colour{result[0], result[1], result[2]};
        applied_colour = &tinted;
    }
    a2fo_identity_call_thiscall_1(
        at(g_armada, kSpriteSetColourRva), sprite,
        reinterpret_cast<std::uintptr_t>(applied_colour));
    if (applied && report_count && report_message &&
        InterlockedCompareExchange(report_count, 1, 0) == 0) {
        log_line(report_message);
    }
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
    const Colour* applied_colour = native_colour;
    Colour configured_colour{};
    Colour tinted{};
    bool applied = false;
    bool passive_colour_configured = false;
    auto presentation =
        a2fo::craft_identity::WeaponIconPresentation::live_status;
    if (g_runtime_ready && icon) {
        void* parameter_db = gui_parameter_db();
        if (!g_ui_configuration.loaded ||
            g_ui_configuration.parameter_db != parameter_db) {
            refresh_ui_configuration(parameter_db);
        }
        void* craft = nullptr;
        if (g_rendering_weapon_icon == icon) {
            craft = g_rendering_weapon_craft;
            presentation = g_rendering_weapon_presentation;
        } else {
            presentation = weapon_icon_presentation(icon, &craft);
        }
        switch (presentation) {
            case a2fo::craft_identity::WeaponIconPresentation::passive_neutral:
                passive_colour_configured =
                    g_ui_configuration.passive_weapon_icon_colour_found;
                configured_colour = passive_colour_configured
                    ? g_ui_configuration.passive_weapon_icon_colour
                    : Colour{1.0f, 1.0f, 1.0f};
                applied = true;
                break;
            case a2fo::craft_identity::WeaponIconPresentation::disabled:
                applied = configured_weapon_state_colour(
                    a2fo::craft_identity::SystemIconState::disabled,
                    &configured_colour);
                break;
            case a2fo::craft_identity::WeaponIconPresentation::hidden:
                break;
            case a2fo::craft_identity::WeaponIconPresentation::live_status:
                applied = configured_weapon_colour(
                    craft, &configured_colour);
                break;
        }
    }
    if (applied && native_colour &&
        readable_range(native_colour, sizeof(*native_colour))) {
        Colour native{};
        std::memcpy(&native, native_colour, sizeof(native));
        const auto result = a2fo::craft_identity::tint_system_icon_colour(
            {{native.red, native.green, native.blue}},
            {{configured_colour.red, configured_colour.green,
              configured_colour.blue}});
        tinted = Colour{result[0], result[1], result[2]};
        applied_colour = &tinted;
    }
    a2fo_identity_fo_sprite_set_colour(
        at(g_fleet_ops, kFoSpriteSetColourRva), sprite, applied_colour);
    if (applied && InterlockedCompareExchange(
            &g_weapon_icon_colour_report_count, 1, 0) == 0) {
        log_line(
            "WeaponXiconpos colour policy applied to native icon layers");
    }
    if (presentation ==
            a2fo::craft_identity::WeaponIconPresentation::passive_neutral &&
        InterlockedCompareExchange(
            &g_passive_weapon_icon_report_count, 1, 0) == 0) {
        log_line(passive_colour_configured
            ? "passiveWeaponIconColor applied to UtilityWeapon icons"
            : "Passive UtilityWeapon icon rendered in neutral white/grey");
    }
    if (presentation ==
            a2fo::craft_identity::WeaponIconPresentation::disabled &&
        InterlockedCompareExchange(
            &g_disabled_weapon_icon_report_count, 1, 0) == 0) {
        log_line(
            "Research-locked weapon icon rendered disabled");
    }
}

void __attribute__((fastcall)) ship_system_icon_render_policy_hook(
    void* icon, void*) noexcept {
    if (!g_ship_system_icon_render_original) return;
    void* craft = nullptr;
    const auto presentation = g_runtime_ready && icon
        ? weapon_icon_presentation(icon, &craft)
        : a2fo::craft_identity::WeaponIconPresentation::live_status;
    if (presentation ==
            a2fo::craft_identity::WeaponIconPresentation::hidden) {
        if (InterlockedCompareExchange(
                &g_hidden_weapon_icon_report_count, 1, 0) == 0) {
            log_line(
                "WeaponXiconpos icon hidden by buttonHideUnavailable");
        }
        return;
    }
    void* const previous_icon = g_rendering_weapon_icon;
    void* const previous_craft = g_rendering_weapon_craft;
    const auto previous_presentation = g_rendering_weapon_presentation;
    g_rendering_weapon_icon = icon;
    g_rendering_weapon_craft = craft;
    g_rendering_weapon_presentation = presentation;
    a2fo_identity_call_thiscall_0(
        g_ship_system_icon_render_original, icon);
    g_rendering_weapon_icon = previous_icon;
    g_rendering_weapon_craft = previous_craft;
    g_rendering_weapon_presentation = previous_presentation;
}

void system_text_set_colour_from_context(
    void* sprite, void* text, const Colour* native_colour) noexcept {
    const Colour* applied_colour = native_colour;
    Colour configured_colour{};
    Colour tinted{};
    ValueComponentKind kind = ValueComponentKind::unrelated;
    bool applied = false;
    if (g_runtime_ready && text) {
        void* parameter_db = gui_parameter_db();
        if (!g_ui_configuration.loaded ||
            g_ui_configuration.parameter_db != parameter_db) {
            refresh_ui_configuration(parameter_db);
        }
        applied = configured_value_colour(
            text, &configured_colour, &kind);
    }
    if (applied && native_colour &&
        readable_range(native_colour, sizeof(*native_colour))) {
        Colour native{};
        std::memcpy(&native, native_colour, sizeof(native));
        const auto result = a2fo::craft_identity::tint_system_icon_colour(
            {{native.red, native.green, native.blue}},
            {{configured_colour.red, configured_colour.green,
              configured_colour.blue}});
        tinted = Colour{result[0], result[1], result[2]};
        applied_colour = &tinted;
    }
    a2fo_identity_call_thiscall_1(
        at(g_armada, kSpriteSetColourRva), sprite,
        reinterpret_cast<std::uintptr_t>(applied_colour));
    LONG* report_count = &g_system_value_icon_colour_report_count;
    const char* report_message =
        "Configured live-health colours applied to the native selected-panel system value icons";
    if (kind == ValueComponentKind::crew) {
        report_count = &g_crew_icon_colour_report_count;
        report_message =
            "Configured live-health colour applied to the native crew icon";
    } else if (kind == ValueComponentKind::mouseover_status) {
        report_count = &g_mouseover_status_icon_colour_report_count;
        report_message =
            "Configured live-health colours applied to native mouse-over hull/shield icons";
    } else if (kind == ValueComponentKind::special_energy) {
        report_count = &g_special_energy_icon_colour_report_count;
        report_message =
            "specialEnergyIconColor applied to the native special-energy icon";
    } else if (kind == ValueComponentKind::officer) {
        report_count = &g_officer_icon_colour_report_count;
        report_message =
            "officerIconColor applied to the native officer icon";
    }
    if (applied && InterlockedCompareExchange(
            report_count, 1, 0) == 0) {
        log_line(report_message);
    }
}

void value_text_colour_from_context(
    void* component, void* text_record) noexcept {
    if (!g_runtime_ready || !component ||
        !writable_range(text_record, sizeof(Colour))) {
        return;
    }
    void* parameter_db = gui_parameter_db();
    if (!g_ui_configuration.loaded ||
        g_ui_configuration.parameter_db != parameter_db) {
        refresh_ui_configuration(parameter_db);
    }
    Colour configured_colour{};
    ValueComponentKind kind = ValueComponentKind::unrelated;
    if (!configured_value_colour(
            component, &configured_colour, &kind)) {
        return;
    }
    std::memcpy(text_record, &configured_colour, sizeof(configured_colour));
    LONG* report_count = &g_system_text_colour_report_count;
    const char* report_message =
        "Configured live-health colours applied to the native selected-panel system value text";
    if (kind == ValueComponentKind::crew) {
        report_count = &g_crew_text_colour_report_count;
        report_message =
            "Configured live-health colour applied to the native crew value text";
    } else if (kind == ValueComponentKind::mouseover_status) {
        report_count = &g_mouseover_status_text_colour_report_count;
        report_message =
            "Configured live-health colours applied to native mouse-over hull/shield value text";
    } else if (kind == ValueComponentKind::special_energy) {
        report_count = &g_special_energy_text_colour_report_count;
        report_message =
            "specialEnergyIconColor applied to the native special-energy value text";
    } else if (kind == ValueComponentKind::officer) {
        report_count = &g_officer_text_colour_report_count;
        report_message =
            "officerIconColor applied to the native officer value text";
    }
    if (InterlockedCompareExchange(report_count, 1, 0) == 0) {
        log_line(report_message);
    }
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
    if (!info_display || !craft || !text_component ||
        !resolve_energy_amount_api()) return;

    const std::array<float, 3> maximum{{
        g_get_maximum_photon_torpedoes(craft),
        g_get_maximum_quantum_torpedoes(craft),
        g_get_maximum_shuttle_craft
            ? g_get_maximum_shuttle_craft(craft) : 0.0f}};
    bool any_store = false;
    for (float value : maximum) {
        any_store = any_store || (std::isfinite(value) && value > 0.0f);
    }
    if (!any_store) return;

    NativeRectangle photon_rectangle = live_anchor;
    NativeRectangle quantum_rectangle = live_anchor;
    NativeRectangle shuttle_craft_rectangle = live_anchor;
    photon_rectangle.top += 16;
    photon_rectangle.bottom += 16;
    quantum_rectangle.top += 40;
    quantum_rectangle.bottom += 40;
    shuttle_craft_rectangle.top += 64;
    shuttle_craft_rectangle.bottom += 64;
    if (g_ui_configuration.captain_rectangle_found &&
        g_ui_configuration.photon_rectangle_found) {
        photon_rectangle = translated_rectangle(
            live_anchor, g_ui_configuration.captain_rectangle,
            g_ui_configuration.photon_rectangle);
    }
    if (g_ui_configuration.captain_rectangle_found &&
        g_ui_configuration.quantum_rectangle_found) {
        quantum_rectangle = translated_rectangle(
            live_anchor, g_ui_configuration.captain_rectangle,
            g_ui_configuration.quantum_rectangle);
    }
    if (g_ui_configuration.captain_rectangle_found &&
        g_ui_configuration.shuttle_craft_rectangle_found) {
        shuttle_craft_rectangle = translated_rectangle(
            live_anchor, g_ui_configuration.captain_rectangle,
            g_ui_configuration.shuttle_craft_rectangle);
    }

    const std::array<NativeRectangle, 3> rectangles{{
        panel_rectangle(kAmmoRows[0], photon_rectangle),
        panel_rectangle(kAmmoRows[1], quantum_rectangle),
        panel_rectangle(kAmmoRows[2], shuttle_craft_rectangle)}};
    constexpr std::array<const char*, 3> default_labels{{
        "Photon Torpedoes", "Quantum Torpedoes", "Shuttle Craft"}};
    const std::array<float, 3> current{{
        g_get_photon_torpedoes(craft),
        g_get_quantum_torpedoes(craft),
        g_get_shuttle_craft ? g_get_shuttle_craft(craft) : 0.0f}};
    const std::array<EnergyAmountGetter, 3> reload_getters{{
        g_get_photon_torpedo_reload_seconds,
        g_get_quantum_torpedo_reload_seconds,
        g_get_shuttle_craft_reload_seconds}};
    const AmmunitionUiPolicy* policy = ammunition_ui_policy(craft);

    void* display_interface = read_at<void*>(text_component, 0x04, nullptr);
    if (!display_interface) return;
    const std::int32_t display_origin_x = read_at<std::int32_t>(
        display_interface, 0x04, 0);
    const std::int32_t display_origin_y = read_at<std::int32_t>(
        display_interface, 0x08, 0);

    AmmunitionTooltipRuntime updated{};
    updated.info_display = info_display;
    updated.craft = craft;
    std::array<bool, 3> drawn{};
    for (std::size_t index = 0; index < drawn.size(); ++index) {
        if (!std::isfinite(maximum[index]) || maximum[index] <= 0.0f ||
            !std::isfinite(current[index])) {
            continue;
        }
        const AmmunitionPresentation* presentation = policy
            ? &policy->stores[index] : nullptr;
        const char* configured_label = presentation &&
                !presentation->label.empty()
            ? presentation->label.c_str() : default_labels[index];
        const char* label = localized_directional_shield_text(
            configured_label, configured_label);
        const int displayed_current = whole_ammunition_amount(current[index]);
        const int displayed_maximum = whole_ammunition_amount(maximum[index]);
        const float ratio = clamped_ratio(current[index], maximum[index]);
        Colour healthy{0.0f, 1.0f, 0.0f};
        Colour low{1.0f, 1.0f, 0.0f};
        Colour critical{1.0f, 0.0f, 0.0f};
        if (index == 0) {
            if (g_ui_configuration.photon_colour_found) {
                healthy = g_ui_configuration.photon_colour;
            }
            if (g_ui_configuration.photon_low_colour_found) {
                low = g_ui_configuration.photon_low_colour;
            }
            if (g_ui_configuration.photon_critical_colour_found) {
                critical = g_ui_configuration.photon_critical_colour;
            }
        } else if (index == 1) {
            if (g_ui_configuration.quantum_colour_found) {
                healthy = g_ui_configuration.quantum_colour;
            }
            if (g_ui_configuration.quantum_low_colour_found) {
                low = g_ui_configuration.quantum_low_colour;
            }
            if (g_ui_configuration.quantum_critical_colour_found) {
                critical = g_ui_configuration.quantum_critical_colour;
            }
        } else {
            if (g_ui_configuration.shuttle_craft_colour_found) {
                healthy = g_ui_configuration.shuttle_craft_colour;
            }
            if (g_ui_configuration.shuttle_craft_low_colour_found) {
                low = g_ui_configuration.shuttle_craft_low_colour;
            }
            if (g_ui_configuration.shuttle_craft_critical_colour_found) {
                critical = g_ui_configuration.shuttle_craft_critical_colour;
            }
        }
        const Colour status_colour = panel_colour(kAmmoRows[index],
            ratio <= 0.25f ? critical : ratio <= 0.50f ? low : healthy, ratio);

        AmmunitionValueDisplayMode value_mode = presentation
            ? presentation->value_display_mode
            : AmmunitionValueDisplayMode::percent;
        char value_text[128]{};
        if (value_mode == AmmunitionValueDisplayMode::amount) {
            std::snprintf(value_text, sizeof(value_text), "%d/%d",
                          displayed_current, displayed_maximum);
        } else if (value_mode == AmmunitionValueDisplayMode::reload) {
            if (ratio >= 0.9999f) {
                const char* ready = localized_directional_shield_text(
                    "GUI_SD_SPE_READY", "Ready");
                std::snprintf(value_text, sizeof(value_text), "%s", ready);
            } else {
                EnergyAmountGetter reload_getter = reload_getters[index];
                const float seconds = reload_getter
                    ? reload_getter(craft) : -1.0f;
                if (std::isfinite(seconds) && seconds >= 0.0f) {
                    const int displayed_seconds = std::max(
                        1, static_cast<int>(std::ceil(seconds)));
                    std::snprintf(value_text, sizeof(value_text), "%d s",
                                  displayed_seconds);
                } else {
                    const char* waiting = localized_directional_shield_text(
                        "GUI_SD_AMMO_WAITING", "Resupply");
                    std::snprintf(value_text, sizeof(value_text), "%s",
                                  waiting);
                }
            }
        } else {
            const int percent = static_cast<int>(std::floor(
                ratio * 100.0f + 0.5f));
            std::snprintf(value_text, sizeof(value_text), "%d%%", percent);
        }
        NativeRectangle hit_rectangle{};
        bool icon_drawn = false;
        NativeRectangle amount_rectangle = rectangles[index];
        const bool requested_icon = presentation &&
            presentation->display_mode == AmmunitionDisplayMode::icon;
        if (requested_icon && !presentation->icon.empty() &&
            presentation->icon_position_found) {
            const RawRectangle& configured = presentation->icon_position;
            const std::int32_t row_height =
                rectangles[index].bottom - rectangles[index].top;
            const NativeRectangle icon_rectangle = panel_rectangle(kAmmoIcons[index], NativeRectangle{
                rectangles[index].left,
                rectangles[index].top +
                    (row_height - configured.height) / 2,
                rectangles[index].left + configured.width,
                rectangles[index].top +
                    (row_height - configured.height) / 2 +
                    configured.height});
            icon_drawn = draw_ammunition_icon(
                index, presentation->icon, configured, icon_rectangle,
                panel_colour(kAmmoIcons[index], status_colour, ratio),
                display_origin_x, display_origin_y);
            if (icon_drawn) {
                hit_rectangle = union_rectangle(
                    hit_rectangle, icon_rectangle);
                if (!panel_style(kAmmoIcons[index]).area_found)
                    amount_rectangle.left = std::max(
                        amount_rectangle.left, icon_rectangle.right + 4);
            }
        }

        bool text_drawn = false;
        bool bar_drawn = false;
        if (value_mode == AmmunitionValueDisplayMode::bar) {
            NativeRectangle bar_rectangle = amount_rectangle;
            if (!requested_icon && usable_native_rectangle(amount_rectangle)) {
                const std::int32_t width =
                    amount_rectangle.right - amount_rectangle.left;
                NativeRectangle label_rectangle = amount_rectangle;
                label_rectangle.right = amount_rectangle.left +
                    std::max<std::int32_t>(1, width * 3 / 5);
                bar_rectangle.left = std::min(
                    amount_rectangle.right, label_rectangle.right + 4);
                char label_text[512]{};
                std::snprintf(
                    label_text, sizeof(label_text), "%s:", label);
                text_drawn = draw_identity_text(
                    label_text, panel_rectangle(kAmmoLabels[index], label_rectangle),
                    panel_colour(kAmmoLabels[index], status_colour, ratio), text_component);
                if (text_drawn) hit_rectangle = union_rectangle(hit_rectangle,
                    panel_rectangle(kAmmoLabels[index], label_rectangle));
            }
            if (usable_native_rectangle(bar_rectangle) || panel_style(kAmmoBars[index]).area_found) {
                const std::int32_t row_height =
                    bar_rectangle.bottom - bar_rectangle.top;
                const std::int32_t bar_height = std::max<std::int32_t>(
                    1, std::min<std::int32_t>(8, row_height));
                bar_rectangle.top += (row_height - bar_height) / 2;
                bar_rectangle.bottom = bar_rectangle.top + bar_height;
                bar_rectangle = panel_rectangle(kAmmoBars[index], bar_rectangle);
                bar_drawn = draw_ammunition_value_bar(
                    bar_rectangle, ratio, status_colour,
                    display_origin_x, display_origin_y, nullptr, kAmmoBars[index]);
                if (bar_drawn) {
                    hit_rectangle = union_rectangle(
                        hit_rectangle, bar_rectangle);
                }
            }
        } else {
            char text[640]{};
            const bool separate = panel_style(kAmmoLabels[index]).area_found ||
                panel_style(kAmmoValues[index]).area_found ||
                panel_style(kAmmoLabels[index]).colour_found ||
                panel_style(kAmmoValues[index]).colour_found ||
                panel_style(kAmmoLabels[index]).low_found ||
                panel_style(kAmmoLabels[index]).critical_found ||
                panel_style(kAmmoValues[index]).low_found ||
                panel_style(kAmmoValues[index]).critical_found;
            if (separate && !requested_icon) {
                NativeRectangle label_rectangle = amount_rectangle;
                label_rectangle.right = label_rectangle.left +
                    (label_rectangle.right - label_rectangle.left) * 3 / 5;
                amount_rectangle.left = label_rectangle.right + 4;
                label_rectangle = panel_rectangle(kAmmoLabels[index], label_rectangle);
                text_drawn = draw_identity_text(std::string(label) + ":",
                    label_rectangle, panel_colour(kAmmoLabels[index], status_colour, ratio),
                    text_component);
                if (text_drawn) hit_rectangle = union_rectangle(hit_rectangle, label_rectangle);
            }
            if (requested_icon || separate) {
                std::snprintf(text, sizeof(text), "%s", value_text);
            } else {
                std::snprintf(
                    text, sizeof(text), "%s: %s", label, value_text);
            }
            amount_rectangle = panel_rectangle(kAmmoValues[index], amount_rectangle);
            const bool value_drawn = draw_identity_text(text, amount_rectangle,
                panel_colour(kAmmoValues[index], status_colour, ratio), text_component);
            if (value_drawn) hit_rectangle = union_rectangle(hit_rectangle, amount_rectangle);
            text_drawn = text_drawn || value_drawn;
        }
        drawn[index] = icon_drawn || text_drawn || bar_drawn;
        if (!drawn[index]) continue;

        updated.visible[index] = true;
        updated.current[index] = current[index];
        updated.maximum[index] = maximum[index];
        updated.hit_rectangles[index] = FloatRectangle{
            static_cast<float>(hit_rectangle.left + display_origin_x),
            static_cast<float>(hit_rectangle.top + display_origin_y),
            static_cast<float>(hit_rectangle.right - hit_rectangle.left),
            static_cast<float>(hit_rectangle.bottom - hit_rectangle.top)};
        updated.active = true;
    }
    if (updated.active) g_ammunition_tooltip = updated;

    if (std::any_of(drawn.begin(), drawn.end(), [](bool value) {
            return value;
        }) &&
        InterlockedCompareExchange(
            &g_ammunition_draw_report_count, 1, 0) == 0) {
        char message[384]{};
        std::snprintf(
            message, sizeof(message),
            "First ammunition UI draw submitted: Photon=%s at %ld,%ld; "
            "Quantum=%s at %ld,%ld; Shuttle Craft=%s at %ld,%ld",
            drawn[0] ? "yes" : "no",
            static_cast<long>(photon_rectangle.left),
            static_cast<long>(photon_rectangle.top),
            drawn[1] ? "yes" : "no",
            static_cast<long>(quantum_rectangle.left),
            static_cast<long>(quantum_rectangle.top),
            drawn[2] ? "yes" : "no",
            static_cast<long>(shuttle_craft_rectangle.left),
            static_cast<long>(shuttle_craft_rectangle.top));
        log_line(message);
    }
}

bool read_experience_progress(
    void* craft, float* current, float* next_rank) noexcept {
    if (current) *current = 0.0f;
    if (next_rank) *next_rank = 0.0f;
    if (!craft || !current || !next_rank) return false;
    void* enhancement = read_at<void*>(
        craft, kCraftEnhancementOffset, nullptr);
    void* enhancement_class = read_at<void*>(
        enhancement, kEnhancementClassOffset, nullptr);
    const float live_current = read_at<float>(
        enhancement, kEnhancementCurrentXpOffset,
        std::numeric_limits<float>::quiet_NaN());
    const float live_next = read_at<float>(
        enhancement_class, kEnhancementNextRankXpOffset,
        std::numeric_limits<float>::quiet_NaN());
    if (!std::isfinite(live_current) || live_current < 0.0f ||
        !std::isfinite(live_next) || live_next <= 0.0f) {
        return false;
    }
    *current = live_current;
    *next_rank = live_next;
    return true;
}

void draw_selected_status_bars(
    void* info_display, void* craft, void* text_component,
    const NativeRectangle& live_anchor) noexcept {
    if (!info_display || !craft || !text_component) {
        return;
    }
    void* display_interface = read_at<void*>(text_component, 0x04, nullptr);
    if (!display_interface) return;
    const std::int32_t display_origin_x = read_at<std::int32_t>(
        display_interface, 0x04, 0);
    const std::int32_t display_origin_y = read_at<std::int32_t>(
        display_interface, 0x08, 0);

    SelectedStatusTooltipRuntime updated{};
    updated.info_display = info_display;
    updated.craft = craft;

    auto add_hit_rectangle = [&](SelectedStatusIndex status,
                                 const NativeRectangle& rectangle,
                                 float current, float maximum) {
        if (!usable_native_rectangle(rectangle)) return;
        const std::size_t index = static_cast<std::size_t>(status);
        updated.visible[index] = true;
        updated.current[index] = current;
        updated.maximum[index] = maximum;
        updated.hit_rectangles[index] = FloatRectangle{
            static_cast<float>(rectangle.left + display_origin_x),
            static_cast<float>(rectangle.top + display_origin_y),
            static_cast<float>(rectangle.right - rectangle.left),
            static_cast<float>(rectangle.bottom - rectangle.top)};
        updated.active = true;
    };

    const bool builder_shield = g_active_panel == 1 &&
        g_ui_configuration.builder_shield_bar_rectangle_found;
    if (builder_shield || g_ui_configuration.shield_bar_rectangle_found) {
        const auto& configured = builder_shield
            ? g_ui_configuration.builder_shield_bar_rectangle
            : g_ui_configuration.shield_bar_rectangle;
        const auto offset = read_at<std::int32_t>(info_display, 0x1f0, 0);
        const NativeRectangle shield_rectangle{configured.x, configured.y - offset,
            configured.x + configured.width - 1,
            configured.y - offset + configured.height - 1};
        const float current_shields = read_at<float>(
            craft, kCurrentShieldsOffset,
            std::numeric_limits<float>::quiet_NaN());
        const float maximum_shields = read_at<float>(
            craft, kMaximumShieldsOffset,
            std::numeric_limits<float>::quiet_NaN());
        if (std::isfinite(current_shields) && current_shields >= 0.0f &&
            std::isfinite(maximum_shields) && maximum_shields > 0.0f) {
            // Native shield bars already expose position/size and an SPR skin.
            // Keep their disabled-system rendering; add only the missing hover.
            add_hit_rectangle(
                SelectedStatusIndex::shields, shield_rectangle,
                current_shields, maximum_shields);
        }
    }

    bool experience_drawn = false;
    float current_xp = 0.0f;
    float next_rank_xp = 0.0f;
    if ((g_ui_configuration.experience_bar_rectangle_found ||
         panel_style(PanelElement::ExperienceBar).area_found) &&
        read_experience_progress(craft, &current_xp, &next_rank_xp)) {
        const NativeRectangle experience_rectangle = panel_rectangle(PanelElement::ExperienceBar,
            translated_rectangle(live_anchor, g_ui_configuration.captain_rectangle,
                                 g_ui_configuration.experience_bar_rectangle));
        const Colour experience_colour =
            g_ui_configuration.experience_bar_colour_found
            ? g_ui_configuration.experience_bar_colour
            : Colour{0.2f, 0.65f, 1.0f};
        experience_drawn = draw_ammunition_value_bar(
            experience_rectangle,
            clamped_ratio(current_xp, next_rank_xp), experience_colour,
            display_origin_x, display_origin_y,
            g_ui_configuration.experience_bar_background_colour_found
                ? &g_ui_configuration.experience_bar_background_colour
                : nullptr);
        if (experience_drawn) {
            add_hit_rectangle(
                SelectedStatusIndex::experience, experience_rectangle,
                current_xp, next_rank_xp);
        }
    }
    if (updated.active) g_selected_status_tooltip = updated;
    if (experience_drawn && InterlockedCompareExchange(
            &g_selected_status_draw_report_count, 1, 0) == 0) {
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "First selected-object XP bar submitted: %.0f / %.0f",
            static_cast<double>(current_xp),
            static_cast<double>(next_rank_xp));
        log_line(message);
    }
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
    std::array<float, 4> target{};
    for (std::size_t index = 0; index < target.size(); ++index) {
        target[index] = clamped_ratio(current[index], maximum[index]);
    }

    const DWORD now = GetTickCount();
    const DWORD elapsed = now - g_directional_shield_animation.last_tick;
    if (!g_directional_shield_animation.initialized ||
        g_directional_shield_animation.craft != craft || elapsed > 1000u) {
        g_directional_shield_animation.craft = craft;
        g_directional_shield_animation.last_tick = now;
        g_directional_shield_animation.initialized = true;
        g_directional_shield_animation.displayed_ratio = target;
        return target;
    }

    // A full-to-empty visual transition takes about 450 ms. Gameplay values
    // remain immediate; only the selected-panel presentation eases toward the
    // live value so a large hit reads as a drain rather than a single-frame
    // pop.
    const float maximum_step = std::min(
        1.0f, static_cast<float>(elapsed) / 450.0f);
    for (std::size_t index = 0; index < target.size(); ++index) {
        g_directional_shield_animation.displayed_ratio[index] = move_towards(
            g_directional_shield_animation.displayed_ratio[index],
            target[index], maximum_step);
    }
    g_directional_shield_animation.last_tick = now;
    return g_directional_shield_animation.displayed_ratio;
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
    for (const AtlasCarrier& carrier : carriers) {
        if (_stricmp(reference.c_str(), carrier.atlas) != 0) continue;
        return find_interface_sprite(database, carrier.sprite);
    }
    return nullptr;
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

bool draw_ammunition_icon(
    std::size_t store_index, const std::string& sprite_name,
    const RawRectangle& source_rectangle,
    const NativeRectangle& rectangle, const Colour& colour,
    std::int32_t display_origin_x,
    std::int32_t display_origin_y) noexcept {
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

    void* database = read_at<void*>(
        at(g_armada, kInterfaceSpriteDatabasePointerRva), 0, nullptr);
    AmmunitionIconCache& cache = g_ammunition_icon_cache;
    if (!database) return false;
    if (cache.database != database) {
        cache = {};
        cache.database = database;
    }
    if (cache.names[store_index] != sprite_name) {
        cache.names[store_index] = sprite_name;
        cache.sprites[store_index] = nullptr;
        cache.attempted[store_index] = false;
    }
    void*& sprite = cache.sprites[store_index];
    if (sprite && !directional_shield_sprite_is_drawable(sprite)) {
        // Do not enter the database lookup in the same frame that exposed a
        // stale sprite. The interface may still be tearing down.
        sprite = nullptr;
        cache.attempted[store_index] = false;
        return false;
    }
    if (!sprite && !cache.attempted[store_index]) {
        cache.attempted[store_index] = true;
        sprite = ammunition_icon_sprite(database, sprite_name);
    }
    if (!directional_shield_sprite_is_drawable(sprite)) {
        if (InterlockedCompareExchange(
                &g_ammunition_icon_failure_report_count, 1, 0) == 0) {
            log_line(std::string("Ammunition icon sprite '") + sprite_name +
                     "' was not found or draw-ready; using the compact value without a label");
        }
        return false;
    }

    auto* texture_x_address =
        static_cast<std::uint8_t*>(sprite) + kSpriteTextureXOffset;
    auto* texture_y_address =
        static_cast<std::uint8_t*>(sprite) + kSpriteTextureYOffset;
    auto* texture_width_address =
        static_cast<std::uint8_t*>(sprite) + kSpriteTextureWidthOffset;
    auto* texture_height_address =
        static_cast<std::uint8_t*>(sprite) + kSpriteTextureHeightOffset;
    if (!writable_range(texture_x_address, sizeof(float)) ||
        !writable_range(texture_y_address, sizeof(float)) ||
        !writable_range(texture_width_address, sizeof(float)) ||
        !writable_range(texture_height_address, sizeof(float))) {
        return false;
    }
    UINT texture_width = 0;
    UINT texture_height = 0;
    if (!ammunition_icon_texture_dimensions(
            sprite, &texture_width, &texture_height) ||
        static_cast<std::uint64_t>(source_rectangle.x) +
                static_cast<std::uint64_t>(source_rectangle.width) >
            texture_width ||
        static_cast<std::uint64_t>(source_rectangle.y) +
                static_cast<std::uint64_t>(source_rectangle.height) >
            texture_height) {
        if (InterlockedCompareExchange(
                &g_ammunition_icon_failure_report_count, 1, 0) == 0) {
            log_line(std::string("Ammunition icon crop for '") +
                     sprite_name +
                     "' is outside its texture or its dimensions are unavailable");
        }
        return false;
    }

    const Colour saved_colour = read_at<Colour>(
        sprite, kSpriteColourOffset, Colour{});
    const FloatRectangle saved_texture_window{
        read_at<float>(sprite, kSpriteTextureXOffset, 0.0f),
        read_at<float>(sprite, kSpriteTextureYOffset, 0.0f),
        read_at<float>(sprite, kSpriteTextureWidthOffset, 0.0f),
        read_at<float>(sprite, kSpriteTextureHeightOffset, 0.0f)};
    if (!usable_float_rectangle(saved_texture_window)) return false;
    const float texture_x = static_cast<float>(source_rectangle.x) /
        static_cast<float>(texture_width);
    const float texture_y = static_cast<float>(source_rectangle.y) /
        static_cast<float>(texture_height);
    const float cropped_texture_width =
        static_cast<float>(source_rectangle.width) /
        static_cast<float>(texture_width);
    const float cropped_texture_height =
        static_cast<float>(source_rectangle.height) /
        static_cast<float>(texture_height);
    std::memcpy(texture_x_address, &texture_x, sizeof(texture_x));
    std::memcpy(texture_y_address, &texture_y, sizeof(texture_y));
    std::memcpy(texture_width_address, &cropped_texture_width,
                sizeof(cropped_texture_width));
    std::memcpy(texture_height_address, &cropped_texture_height,
                sizeof(cropped_texture_height));
    const SpriteVector position{
        static_cast<float>(rectangle.left + display_origin_x),
        static_cast<float>(rectangle.top + display_origin_y),
        0.0f};
    a2fo_identity_fo_sprite_set_colour(
        at(g_fleet_ops, kFoSpriteSetColourRva), sprite, &colour);
    a2fo_identity_fo_sprite_draw_scaled_2d(
        at(g_fleet_ops, kFoSpriteDrawScaled2DRva), sprite, &position,
        static_cast<float>(rectangle.right - rectangle.left),
        static_cast<float>(rectangle.bottom - rectangle.top));
    a2fo_identity_fo_sprite_set_colour(
        at(g_fleet_ops, kFoSpriteSetColourRva), sprite, &saved_colour);
    std::memcpy(texture_x_address, &saved_texture_window.x,
                sizeof(saved_texture_window.x));
    std::memcpy(texture_y_address, &saved_texture_window.y,
                sizeof(saved_texture_window.y));
    std::memcpy(texture_width_address, &saved_texture_window.width,
                sizeof(saved_texture_window.width));
    std::memcpy(texture_height_address, &saved_texture_window.height,
                sizeof(saved_texture_window.height));
    return true;
}

bool draw_ammunition_value_bar(
    const NativeRectangle& rectangle, float ratio, const Colour& colour,
    std::int32_t display_origin_x,
    std::int32_t display_origin_y,
    const Colour* background_colour, PanelElement element) noexcept {
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

    void* database = read_at<void*>(
        at(g_armada, kInterfaceSpriteDatabasePointerRva), 0, nullptr);
    if (!database) return false;
    const auto& style = panel_style(element);
    const auto draw_layer = [&](std::size_t layer, const std::string& requested,
                                float fill, const Colour& tint) noexcept {
        auto& cache = g_ammunition_bar_cache[static_cast<std::size_t>(element)][layer];
        const std::string name = requested.empty() ? "large_shield_bar" : requested;
        if (cache.database != database || cache.name != name) {
            cache = {};
            cache.database = database;
            cache.name = name;
        }
        if (cache.sprite && !directional_shield_sprite_is_drawable(cache.sprite)) {
            cache.sprite = nullptr;
            cache.attempted = false;
            return false;
        }
        if (!cache.attempted) {
            cache.attempted = true;
            cache.sprite = find_interface_sprite(database, name.c_str());
            if (!directional_shield_sprite_is_drawable(cache.sprite) && !requested.empty()) {
                log_line(std::string("Panel bar sprite '") + name + "' unavailable; using large_shield_bar");
                cache.sprite = find_interface_sprite(database, "large_shield_bar");
            }
        }
        void* sprite = cache.sprite;
        if (!directional_shield_sprite_is_drawable(sprite)) return false;
        auto* width_address = static_cast<std::uint8_t*>(sprite) + kSpriteTextureWidthOffset;
        if (!writable_range(width_address, sizeof(float))) return false;
        const float saved_width = read_at<float>(sprite, kSpriteTextureWidthOffset, 0.0f);
        if (!std::isfinite(saved_width) || saved_width <= 0.0f) return false;
        const Colour saved_colour = read_at<Colour>(sprite, kSpriteColourOffset, Colour{});
        const float cropped_width = saved_width * fill;
        std::memcpy(width_address, &cropped_width, sizeof(cropped_width));
        const SpriteVector position{static_cast<float>(rectangle.left + display_origin_x),
            static_cast<float>(rectangle.top + display_origin_y), 0.0f};
        a2fo_identity_fo_sprite_set_colour(at(g_fleet_ops, kFoSpriteSetColourRva), sprite, &tint);
        a2fo_identity_fo_sprite_draw_scaled_2d(at(g_fleet_ops, kFoSpriteDrawScaled2DRva), sprite,
            &position, (rectangle.right - rectangle.left) * fill,
            static_cast<float>(rectangle.bottom - rectangle.top));
        std::memcpy(width_address, &saved_width, sizeof(saved_width));
        a2fo_identity_fo_sprite_set_colour(at(g_fleet_ops, kFoSpriteSetColourRva), sprite, &saved_colour);
        return true;
    };
    const Colour background = style.background_found ? style.background :
        background_colour ? *background_colour : Colour{0.08f, 0.08f, 0.08f};
    const bool track = draw_layer(0,
        style.background_sprite.empty() ? style.sprite : style.background_sprite, 1.0f, background);
    const float fill = std::isfinite(ratio) ? std::clamp(ratio, 0.0f, 1.0f) : 0.0f;
    const bool foreground = fill > 0.0f && draw_layer(1, style.sprite, fill,
        panel_colour(element, colour, fill));
    return track || foreground;
}

bool directional_shield_sprite_cache_is_live(void* database) noexcept {
    if (!g_directional_shield_sprites.ready || !database ||
        g_directional_shield_sprites.database != database) {
        return false;
    }
    for (void* sprite : g_directional_shield_sprites.sprites) {
        if (!directional_shield_sprite_is_drawable(sprite)) return false;
    }
    return true;
}

bool resolve_directional_shield_sprites() noexcept {
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
        return false;
    }

    void* database = read_at<void*>(
        at(g_armada, kInterfaceSpriteDatabasePointerRva), 0, nullptr);
    if (g_directional_shield_sprite_retry_after != 0) {
        const DWORD now = GetTickCount();
        if (g_directional_shield_sprite_retry_database != database) {
            // A changing pointer means the menu/interface transition is still
            // active. Restart the quiet period without entering native lookup.
            g_directional_shield_sprite_retry_database = database;
            g_directional_shield_sprite_retry_after =
                now + kDirectionalShieldSpriteStabilizationMs;
        }
        if (static_cast<LONG>(
                now - g_directional_shield_sprite_retry_after) < 0) {
            return false;
        }
        g_directional_shield_sprite_retry_after = 0;
        g_directional_shield_sprite_retry_database = nullptr;
    }
    if (g_directional_shield_sprites.resolution_attempted) {
        if (directional_shield_sprite_cache_is_live(database)) return true;
        if (!g_directional_shield_sprites.ready &&
            g_directional_shield_sprites.database == database) {
            // Fleet Operations can publish a newly parsed interface sprite
            // before its texture/frame list is ready. The previous one-shot
            // failure cache made that transient state permanent for the rest
            // of the mission. The quiet-period gate above has now elapsed, so
            // permit another lookup against the same live database.
            g_directional_shield_sprites.resolution_attempted = false;
            g_directional_shield_sprites.sprites = {};
        } else {
            // The interface database changed, or one of its former sprites
            // has been destructed in place. Do not call its lookup routine
            // during the same frame: menu teardown can leave the pointer
            // temporarily non-null while its internal tree is already invalid.
            reset_directional_shield_sprite_runtime();
            return false;
        }
    }
    g_directional_shield_sprites.resolution_attempted = true;
    g_directional_shield_sprites.database = database;
    if (!database) {
        log_line("Directional-shield sprite database is unavailable; numeric display retained");
        return false;
    }

    for (std::size_t index = 0;
         index < kDirectionalShieldSpriteNames.size(); ++index) {
        g_directional_shield_sprites.sprites[index] = find_interface_sprite(
            database, kDirectionalShieldSpriteNames[index]);
        if (!directional_shield_sprite_is_drawable(
                g_directional_shield_sprites.sprites[index])) {
            void* sprite = g_directional_shield_sprites.sprites[index];
            void* frame_list = sprite ? read_at<void*>(
                sprite, kSpriteFrameListOffset, nullptr) : nullptr;
            if (InterlockedCompareExchange(
                    &g_directional_shield_sprite_failure_report_count,
                    1, 0) == 0) {
                char message[384]{};
                std::snprintf(
                    message, sizeof(message),
                    "Directional-shield sprite '%s' was missing or not draw-ready (sprite=%p frameList=%p database=%p); retrying after interface quiet period",
                    kDirectionalShieldSpriteNames[index], sprite,
                    frame_list, database);
                log_line(message);
            }
            g_directional_shield_sprite_retry_database = database;
            g_directional_shield_sprite_retry_after =
                GetTickCount() + kDirectionalShieldSpriteStabilizationMs;
            return false;
        }
    }
    g_directional_shield_sprites.ready = true;
    log_line("Directional-shield sprite ring resolved");
    return true;
}

void update_directional_shield_tooltip_runtime(
    void* info_display, void* craft,
    const std::array<float, 4>& current,
    const std::array<float, 4>& maximum,
    const NativeRectangle& display_rectangle,
    const std::array<FloatRectangle, 4>& segment_positions,
    const std::array<bool, 4>& segment_drawn) noexcept {
    g_directional_shield_tooltip = {};
    if (!info_display || !craft ||
        !usable_native_rectangle(display_rectangle)) {
        return;
    }

    DirectionalShieldTooltipRuntime updated{};
    updated.info_display = info_display;
    updated.craft = craft;
    updated.current = current;
    updated.maximum = maximum;
    for (std::size_t index = 0;
         index < segment_positions.size(); ++index) {
        if (!segment_drawn[index]) continue;
        const FloatRectangle logical_rectangle{
            static_cast<float>(display_rectangle.left) +
                segment_positions[index].x,
            static_cast<float>(display_rectangle.top) +
                segment_positions[index].y,
            segment_positions[index].width,
            segment_positions[index].height};
        // Both StandardComponent::Update and Armada's cursor globals use the
        // virtual 1600x1200 UI coordinate space. DrawScaled2D applies the
        // physical viewport transform later, during submission; applying it
        // here would move the hover rectangles away from their sprites.
        updated.hit_rectangles[index] = logical_rectangle;
        updated.active = updated.active || usable_float_rectangle(
            updated.hit_rectangles[index]);
    }
    if (updated.active) g_directional_shield_tooltip = updated;
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
    const std::array<float, 4>& maximum, void* text_component,
    const NativeRectangle& live_anchor) noexcept {
    if (!craft || !text_component) return false;
    const bool ring_ready = resolve_directional_shield_sprites();

    const NativeRectangle graphic_rectangle =
        directional_shield_graphic_rectangle(live_anchor);
    if (!usable_native_rectangle(graphic_rectangle)) return false;

    // The GUI rectangles above are local to the selected InfoDisplay. Native
    // DisplayInterface::DrawTextInRectangle adds its origin at +4/+8 before
    // submitting geometry; direct ST3D sprite drawing must apply the same
    // translation explicitly.
    void* display_interface = read_at<void*>(
        text_component, 0x04, nullptr);
    if (!display_interface) return false;
    const std::int32_t display_origin_x = read_at<std::int32_t>(
        display_interface, 0x04, 0);
    const std::int32_t display_origin_y = read_at<std::int32_t>(
        display_interface, 0x08, 0);
    NativeRectangle display_rectangle = graphic_rectangle;
    display_rectangle.left += display_origin_x;
    display_rectangle.right += display_origin_x;
    display_rectangle.top += display_origin_y;
    display_rectangle.bottom += display_origin_y;
    if (!usable_native_rectangle(display_rectangle)) return false;

    const std::array<float, 4> ratios =
        animated_directional_shield_ratios(craft, current, maximum);
    std::array<FloatRectangle, 4> segment_positions =
        kDefaultDirectionalShieldPositions;
    void* object_class = read_at<void*>(
        craft, kObjectClassOffset, nullptr);
    const auto configured_positions =
        g_directional_shield_ui_policies.find(object_class);
    if (configured_positions != g_directional_shield_ui_policies.end()) {
        for (std::size_t index = 0; index < segment_positions.size(); ++index) {
            if (!configured_positions->second.position_found[index]) continue;
            const RawRectangle& configured =
                configured_positions->second.positions[index];
            segment_positions[index] = FloatRectangle{
                static_cast<float>(configured.x),
                static_cast<float>(configured.y),
                static_cast<float>(configured.width),
                static_cast<float>(configured.height)};
        }
    }
    std::array<FloatRectangle, 4> remapped_positions{};
    if (a2fo::craft_identity::remap_directional_shield_positions(
            g_directional_shield_display_config, segment_positions,
            &remapped_positions)) {
        segment_positions = remapped_positions;
    }

    bool drew = false;
    std::array<bool, 4> segment_drawn{};
    const float graphic_width = static_cast<float>(
        graphic_rectangle.right - graphic_rectangle.left);
    const float graphic_height = static_cast<float>(
        graphic_rectangle.bottom - graphic_rectangle.top);
    std::array<std::size_t, 4> sprite_indices{};
    for (std::size_t i = 0; i < sprite_indices.size(); ++i)
        sprite_indices[i] = directional_shield_sprite_index_for_destination(
            segment_positions, i, 128.0f, 128.0f);
    for (std::size_t facing_index = 0;
         facing_index < segment_positions.size(); ++facing_index) {
        auto& target = segment_positions[facing_index];
        target.x *= graphic_width / 128.0f;
        target.y *= graphic_height / 128.0f;
        target.width *= graphic_width / 128.0f;
        target.height *= graphic_height / 128.0f;
        if (panel_style(kShieldSegments[facing_index]).area_found) {
            const auto r = panel_rectangle(kShieldSegments[facing_index], {});
            target = {static_cast<float>(r.left - graphic_rectangle.left),
                static_cast<float>(r.top - graphic_rectangle.top),
                static_cast<float>(r.right - r.left), static_cast<float>(r.bottom - r.top)};
        }
    }
    for (std::size_t facing_index = 0;
         facing_index < segment_positions.size(); ++facing_index) {
        const auto& target = segment_positions[facing_index];
        const auto& style = panel_style(kShieldSegments[facing_index]);
        if (!style.sprite.empty()) {
            const NativeRectangle destination{
                graphic_rectangle.left + static_cast<std::int32_t>(target.x),
                graphic_rectangle.top + static_cast<std::int32_t>(target.y),
                graphic_rectangle.left + static_cast<std::int32_t>(target.x + target.width),
                graphic_rectangle.top + static_cast<std::int32_t>(target.y + target.height)};
            segment_drawn[facing_index] = draw_ammunition_value_bar(destination,
                ratios[facing_index], directional_shield_status_colour(ratios[facing_index]),
                display_origin_x, display_origin_y, nullptr, kShieldSegments[facing_index]);
            drew = segment_drawn[facing_index] || drew;
            continue;
        }
        if (!ring_ready) continue;
        const std::size_t sprite_index = sprite_indices[facing_index];
        void* sprite = g_directional_shield_sprites.sprites[sprite_index];
        auto* texture_x_address = sprite
            ? static_cast<std::uint8_t*>(sprite) + kSpriteTextureXOffset
            : nullptr;
        auto* texture_y_address = sprite
            ? static_cast<std::uint8_t*>(sprite) + kSpriteTextureYOffset
            : nullptr;
        auto* texture_width_address = sprite
            ? static_cast<std::uint8_t*>(sprite) +
                kSpriteTextureWidthOffset
            : nullptr;
        auto* texture_height_address = sprite
            ? static_cast<std::uint8_t*>(sprite) +
                kSpriteTextureHeightOffset
            : nullptr;
        if (!sprite || !writable_range(
                static_cast<std::uint8_t*>(sprite) + kSpriteColourOffset,
                sizeof(Colour)) ||
            !writable_range(texture_x_address, sizeof(float)) ||
            !writable_range(texture_y_address, sizeof(float)) ||
            !writable_range(texture_width_address, sizeof(float)) ||
            !writable_range(texture_height_address, sizeof(float))) {
            continue;
        }
        const Colour saved_colour = read_at<Colour>(
            sprite, kSpriteColourOffset, Colour{});
        const FloatRectangle saved_texture_window{
            read_at<float>(sprite, kSpriteTextureXOffset, 0.0f),
            read_at<float>(sprite, kSpriteTextureYOffset, 0.0f),
            read_at<float>(sprite, kSpriteTextureWidthOffset, 0.0f),
            read_at<float>(sprite, kSpriteTextureHeightOffset, 0.0f)};
        if (!usable_float_rectangle(saved_texture_window)) continue;
        const FloatRectangle& source =
            kDirectionalShieldSourceBounds[sprite_index];
        if (!usable_float_rectangle(source) ||
            !usable_float_rectangle(target)) continue;

        const auto restore_sprite = [&]() noexcept {
            a2fo_identity_fo_sprite_set_colour(
                at(g_fleet_ops, kFoSpriteSetColourRva), sprite,
                &saved_colour);
            std::memcpy(texture_x_address, &saved_texture_window.x,
                        sizeof(saved_texture_window.x));
            std::memcpy(texture_y_address, &saved_texture_window.y,
                        sizeof(saved_texture_window.y));
            std::memcpy(texture_width_address,
                        &saved_texture_window.width,
                        sizeof(saved_texture_window.width));
            std::memcpy(texture_height_address,
                        &saved_texture_window.height,
                        sizeof(saved_texture_window.height));
        };
        const auto draw_texture_window = [&, sprite](
            const FloatRectangle& source_window,
            const FloatRectangle& destination,
            const Colour& draw_colour) noexcept {
            if (!usable_float_rectangle(source_window) ||
                !usable_float_rectangle(destination)) return false;
            const float texture_x = saved_texture_window.x +
                saved_texture_window.width *
                    (source_window.x /
                     kDirectionalShieldTextureDimension);
            const float texture_y = saved_texture_window.y +
                saved_texture_window.height *
                    (source_window.y /
                     kDirectionalShieldTextureDimension);
            const float texture_width = saved_texture_window.width *
                (source_window.width /
                 kDirectionalShieldTextureDimension);
            const float texture_height = saved_texture_window.height *
                (source_window.height /
                 kDirectionalShieldTextureDimension);
            if (!std::isfinite(texture_x) ||
                !std::isfinite(texture_y) ||
                !std::isfinite(texture_width) || texture_width <= 0.0f ||
                !std::isfinite(texture_height) || texture_height <= 0.0f) {
                return false;
            }
            std::memcpy(texture_x_address, &texture_x,
                        sizeof(texture_x));
            std::memcpy(texture_y_address, &texture_y,
                        sizeof(texture_y));
            std::memcpy(texture_width_address, &texture_width,
                        sizeof(texture_width));
            std::memcpy(texture_height_address, &texture_height,
                        sizeof(texture_height));
            const SpriteVector position{
                static_cast<float>(display_rectangle.left) +
                    destination.x,
                static_cast<float>(display_rectangle.top) +
                    destination.y,
                0.0f};
            a2fo_identity_fo_sprite_set_colour(
                at(g_fleet_ops, kFoSpriteSetColourRva), sprite,
                &draw_colour);
            a2fo_identity_fo_sprite_draw_scaled_2d(
                at(g_fleet_ops, kFoSpriteDrawScaled2DRva), sprite,
                &position, destination.width, destination.height);
            return true;
        };

        const float ratio = ratios[facing_index];
        const bool colour_only_mode =
            g_directional_shield_display_config.display_mode == 2;
        const Colour colour = colour_only_mode && ratio <= 0.0f
            ? Colour{0.0f, 0.0f, 0.0f}
            : panel_colour(kShieldSegments[facing_index],
                panel_colour(PanelElement::DirectionalShieldsGraphic,
                    directional_shield_status_colour(ratio), ratio), ratio);
        if (colour_only_mode) {
            // The entire segment remains present. Health is communicated only
            // by colour, with an exactly depleted facing drawn black.
            segment_drawn[facing_index] = draw_texture_window(
                source, target, colour);
            drew = segment_drawn[facing_index] || drew;
        } else {
            const Colour depleted_colour = dimmed_colour(colour);
            segment_drawn[facing_index] = draw_texture_window(
                source, target, depleted_colour);
            drew = segment_drawn[facing_index] || drew;

            if (ratio > 0.0f) {
                const auto fill =
                    a2fo::craft_identity::centered_directional_shield_fill(
                        source, target, ratio,
                        target.width >= target.height);
                drew = draw_texture_window(
                    fill.source, fill.destination, colour) || drew;
            }
        }
        restore_sprite();
    }

    if (drew) {
        update_directional_shield_tooltip_runtime(
            info_display, craft, current, maximum, display_rectangle,
            segment_positions, segment_drawn);
    }

    if (drew && InterlockedCompareExchange(
            &g_directional_shields_graphic_report_count, 1, 0) == 0) {
        char message[640]{};
        std::snprintf(
            message, sizeof(message),
            "First directional-shield ring draw submitted at local=%ld,%ld origin=%ld,%ld display=%ld,%ld: F=%.2f A=%.2f P=%.2f S=%.2f positions F=%.0f,%.0f,%.0f,%.0f A=%.0f,%.0f,%.0f,%.0f P=%.0f,%.0f,%.0f,%.0f S=%.0f,%.0f,%.0f,%.0f",
            static_cast<long>(graphic_rectangle.left),
            static_cast<long>(graphic_rectangle.top),
            static_cast<long>(display_origin_x),
            static_cast<long>(display_origin_y),
            static_cast<long>(display_rectangle.left),
            static_cast<long>(display_rectangle.top),
            static_cast<double>(ratios[0]),
            static_cast<double>(ratios[1]),
            static_cast<double>(ratios[2]),
            static_cast<double>(ratios[3]),
            static_cast<double>(segment_positions[0].x),
            static_cast<double>(segment_positions[0].y),
            static_cast<double>(segment_positions[0].width),
            static_cast<double>(segment_positions[0].height),
            static_cast<double>(segment_positions[1].x),
            static_cast<double>(segment_positions[1].y),
            static_cast<double>(segment_positions[1].width),
            static_cast<double>(segment_positions[1].height),
            static_cast<double>(segment_positions[2].x),
            static_cast<double>(segment_positions[2].y),
            static_cast<double>(segment_positions[2].width),
            static_cast<double>(segment_positions[2].height),
            static_cast<double>(segment_positions[3].x),
            static_cast<double>(segment_positions[3].y),
            static_cast<double>(segment_positions[3].width),
            static_cast<double>(segment_positions[3].height));
        log_line(message);
    }
    return drew;
}

std::array<NativeRectangle, 4> directional_shield_value_rectangles(
    const NativeRectangle& live_anchor) noexcept {
    const NativeRectangle graphic =
        directional_shield_graphic_rectangle(live_anchor);
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
    std::array<NativeRectangle, 4> rectangles{{
        rectangle(32, 22, 96, 40),
        rectangle(32, 88, 96, 106),
        rectangle(20, 55, 64, 73),
        rectangle(64, 55, 108, 73),
    }};
    if (g_ui_configuration.captain_rectangle_found) {
        for (std::size_t index = 0; index < rectangles.size(); ++index) {
            if (!g_ui_configuration.
                    directional_value_rectangle_found[index]) {
                continue;
            }
            rectangles[index] = translated_rectangle(
                live_anchor, g_ui_configuration.captain_rectangle,
                g_ui_configuration.directional_value_rectangles[index]);
        }
    }
    return rectangles;
}

void draw_directional_shield_rows(
    void* info_display, void* craft, void* text_component,
    const NativeRectangle& live_anchor,
    const Colour& shared_colour) noexcept {
    if (!craft || !text_component || !resolve_directional_shields_api() ||
        !g_directional_shields_is_enabled(craft)) {
        return;
    }

    constexpr std::array<std::uint32_t, 4> facings{{
        A2FO_DIRECTIONAL_SHIELD_FORWARD,
        A2FO_DIRECTIONAL_SHIELD_AFT,
        A2FO_DIRECTIONAL_SHIELD_PORT,
        A2FO_DIRECTIONAL_SHIELD_STARBOARD,
    }};
    std::array<float, 4> current{};
    std::array<float, 4> maximum{};
    for (std::size_t index = 0; index < facings.size(); ++index) {
        maximum[index] = g_directional_shields_get_maximum(
            craft, facings[index]);
        current[index] = g_directional_shields_get_current(
            craft, facings[index]);
        if (!std::isfinite(maximum[index]) || maximum[index] <= 0.0f ||
            !std::isfinite(current[index])) {
            return;
        }
    }

    void* object_class = read_at<void*>(craft, kObjectClassOffset, nullptr);
    const auto configured_policy =
        g_directional_shield_ui_policies.find(object_class);
    const DirectionalShieldUiPolicy* policy =
        configured_policy == g_directional_shield_ui_policies.end()
            ? nullptr : &configured_policy->second;
    const bool graphic_drawn = draw_directional_shield_graphic(
        info_display, craft, current, maximum,
        text_component, live_anchor);

    if (policy && policy->value_display_mode_found) {
        if (policy->value_display_mode ==
            a2fo::craft_identity::DirectionalShieldValueDisplayMode::none) {
            return;
        }
        const std::array<NativeRectangle, 4> rectangles =
            directional_shield_value_rectangles(live_anchor);
        std::array<bool, 4> drawn{};
        for (std::size_t index = 0; index < rectangles.size(); ++index) {
            char value[64]{};
            if (!a2fo::craft_identity::format_directional_shield_value(
                    policy->value_display_mode, current[index],
                    maximum[index], value, sizeof(value))) {
                continue;
            }
            const Colour colour = directional_shield_value_status_colour(
                clamped_ratio(current[index], maximum[index]),
                shared_colour);
            drawn[index] = draw_identity_text(
                value, panel_rectangle(kShieldValues[index], rectangles[index]),
                panel_colour(kShieldValues[index], colour,
                    clamped_ratio(current[index], maximum[index])), text_component);
        }
        if (std::any_of(drawn.begin(), drawn.end(),
                        [](bool value) { return value; }) &&
            InterlockedCompareExchange(
                &g_directional_shields_draw_report_count, 1, 0) == 0) {
            char message[384]{};
            std::snprintf(
                message, sizeof(message),
                "First directional-shield value-label draw submitted: mode=%d F=%s A=%s P=%s S=%s",
                static_cast<int>(policy->value_display_mode),
                drawn[0] ? "yes" : "no", drawn[1] ? "yes" : "no",
                drawn[2] ? "yes" : "no", drawn[3] ? "yes" : "no");
            log_line(message);
        }
        return;
    }

    if (graphic_drawn) return;

    NativeRectangle forward_aft_rectangle = live_anchor;
    NativeRectangle port_starboard_rectangle = live_anchor;
    forward_aft_rectangle.top += 112;
    forward_aft_rectangle.bottom += 112;
    port_starboard_rectangle.top += 132;
    port_starboard_rectangle.bottom += 132;
    if (g_ui_configuration.captain_rectangle_found &&
        g_ui_configuration.directional_forward_aft_rectangle_found) {
        forward_aft_rectangle = translated_rectangle(
            live_anchor, g_ui_configuration.captain_rectangle,
            g_ui_configuration.directional_forward_aft_rectangle);
    }
    if (g_ui_configuration.captain_rectangle_found &&
        g_ui_configuration.directional_port_starboard_rectangle_found) {
        port_starboard_rectangle = translated_rectangle(
            live_anchor, g_ui_configuration.captain_rectangle,
            g_ui_configuration.directional_port_starboard_rectangle);
    }

    const Colour colour = g_ui_configuration.directional_shield_colour_found
        ? g_ui_configuration.directional_shield_colour : shared_colour;
    char forward_aft[160]{};
    char port_starboard[160]{};
    std::snprintf(
        forward_aft, sizeof(forward_aft),
        "Shields  F %.0f/%.0f   A %.0f/%.0f",
        static_cast<double>(current[0]),
        static_cast<double>(maximum[0]),
        static_cast<double>(current[1]),
        static_cast<double>(maximum[1]));
    std::snprintf(
        port_starboard, sizeof(port_starboard),
        "         P %.0f/%.0f   S %.0f/%.0f",
        static_cast<double>(current[2]),
        static_cast<double>(maximum[2]),
        static_cast<double>(current[3]),
        static_cast<double>(maximum[3]));
    const bool forward_aft_drawn = draw_identity_text(
        forward_aft, panel_rectangle(PanelElement::DirectionalShieldsForwardAftText, forward_aft_rectangle),
        panel_colour(PanelElement::DirectionalShieldsForwardAftText, colour), text_component);
    const bool port_starboard_drawn = draw_identity_text(
        port_starboard, panel_rectangle(PanelElement::DirectionalShieldsPortStarboardText, port_starboard_rectangle),
        panel_colour(PanelElement::DirectionalShieldsPortStarboardText, colour), text_component);
    if ((forward_aft_drawn || port_starboard_drawn) &&
        InterlockedCompareExchange(
            &g_directional_shields_draw_report_count, 1, 0) == 0) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "First directional-shield UI draw submitted: F/A=%s at %ld,%ld; P/S=%s at %ld,%ld",
            forward_aft_drawn ? "yes" : "no",
            static_cast<long>(forward_aft_rectangle.left),
            static_cast<long>(forward_aft_rectangle.top),
            port_starboard_drawn ? "yes" : "no",
            static_cast<long>(port_starboard_rectangle.left),
            static_cast<long>(port_starboard_rectangle.top));
        log_line(message);
    }
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
    const DirectionalShieldTooltipRuntime& runtime =
        g_directional_shield_tooltip;
    if (!output || output_size == 0 || facing < 0 || facing >= 4 ||
        !runtime.active) {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(facing);
    const char* presentation = localized_directional_shield_text(
        verbose ? kDirectionalShieldVerboseTooltipKeys[index]
                : kDirectionalShieldTooltipKeys[index],
        verbose ? kDirectionalShieldVerboseTooltipFallbacks[index]
                : kDirectionalShieldTooltipFallbacks[index]);
    const char* strength = localized_directional_shield_text(
        kDirectionalShieldStrengthKey,
        kDirectionalShieldStrengthFallback);
    const int written = verbose
        ? std::snprintf(
            output, output_size, "%s\n%s: %.0f / %.0f",
            presentation, strength,
            static_cast<double>(runtime.current[index]),
            static_cast<double>(runtime.maximum[index]))
        : std::snprintf(
            output, output_size, "%s: %.0f / %.0f",
            presentation,
            static_cast<double>(runtime.current[index]),
            static_cast<double>(runtime.maximum[index]));
    return written > 0 && static_cast<std::size_t>(written) < output_size;
}

bool format_ammunition_tooltip(
    std::size_t store_index, bool verbose, char* output,
    std::size_t output_size) noexcept {
    constexpr std::array<const char*, 3> default_tooltips{{
        "Photon Torpedo Ammunition",
        "Quantum Torpedo Ammunition",
        "Shuttle Craft",
    }};
    constexpr std::array<const char*, 3> default_verbose_tooltips{{
        "Photon torpedo magazine and recharge reserve.",
        "Quantum torpedo magazine and recharge reserve.",
        "Embarked shuttlecraft complement and replenishment reserve.",
    }};
    const AmmunitionTooltipRuntime& runtime = g_ammunition_tooltip;
    if (!output || output_size == 0 ||
        store_index >= runtime.visible.size() ||
        !runtime.active || !runtime.visible[store_index] || !runtime.craft) {
        return false;
    }

    const AmmunitionUiPolicy* policy = ammunition_ui_policy(runtime.craft);
    const AmmunitionPresentation* configured = policy
        ? &policy->stores[store_index] : nullptr;
    const std::string* configured_text = nullptr;
    if (configured) {
        if (verbose && !configured->verbose_tooltip.empty()) {
            configured_text = &configured->verbose_tooltip;
        } else if (!configured->tooltip.empty()) {
            configured_text = &configured->tooltip;
        }
    }
    const char* fallback = verbose
        ? default_verbose_tooltips[store_index]
        : default_tooltips[store_index];
    const char* key = configured_text
        ? configured_text->c_str() : fallback;
    const char* presentation = localized_directional_shield_text(
        key, key);
    const int written = verbose
        ? std::snprintf(
            output, output_size, "%s\nAmmunition: %d/%d",
            presentation,
            whole_ammunition_amount(runtime.current[store_index]),
            whole_ammunition_amount(runtime.maximum[store_index]))
        : std::snprintf(
            output, output_size, "%s: %d/%d", presentation,
            whole_ammunition_amount(runtime.current[store_index]),
            whole_ammunition_amount(runtime.maximum[store_index]));
    return written > 0 && static_cast<std::size_t>(written) < output_size;
}

bool format_selected_status_tooltip(
    std::size_t status_index, bool verbose, char* output,
    std::size_t output_size) noexcept {
    const SelectedStatusTooltipRuntime& runtime =
        g_selected_status_tooltip;
    if (!output || output_size == 0 || status_index >= 2 ||
        !runtime.active || !runtime.visible[status_index] ||
        !runtime.craft) {
        return false;
    }

    if (status_index ==
        static_cast<std::size_t>(SelectedStatusIndex::shields)) {
        const SelectedStatusUiPolicy* policy = nullptr;
        void* object_class = read_at<void*>(
            runtime.craft, kObjectClassOffset, nullptr);
        const auto found = g_selected_status_ui_policies.find(object_class);
        if (found != g_selected_status_ui_policies.end()) {
            policy = &found->second;
        }
        const std::string* configured = nullptr;
        if (policy) {
            if (verbose && !policy->shield_verbose_tooltip.empty()) {
                configured = &policy->shield_verbose_tooltip;
            } else if (!policy->shield_tooltip.empty()) {
                configured = &policy->shield_tooltip;
            }
        }
        const char* default_key = verbose
            ? "GUI_SD_SHIELD_VTOOLTIP" : "GUI_SD_SHIELD_TOOLTIP";
        const char* fallback = "Shield Integrity at";
        const char* key = configured ? configured->c_str() : default_key;
        const char* presentation = localized_directional_shield_text(
            key, configured ? key : fallback);
        const int percentage = static_cast<int>(std::lround(
            100.0f * clamped_ratio(
                runtime.current[status_index],
                runtime.maximum[status_index])));
        const int written = std::snprintf(
            // Armada's tooltip renderer performs a second formatting pass.
            // Retain two percent bytes here so that pass displays one.
            output, output_size, "%s %d%%%% %.0f/%.0f", presentation,
            percentage,
            static_cast<double>(runtime.current[status_index]),
            static_cast<double>(runtime.maximum[status_index]));
        return written > 0 &&
            static_cast<std::size_t>(written) < output_size;
    }

    const char* key = verbose
        ? "GUI_SD_EXPERIENCE_VTOOLTIP" : "GUI_SD_EXPERIENCE_TOOLTIP";
    const char* fallback = verbose
        ? "Experience progress toward the vessel's next rank."
        : "Experience";
    const char* presentation = localized_directional_shield_text(
        key, fallback);
    const int written = verbose
        ? std::snprintf(
            output, output_size, "%s\nExperience: %.0f / %.0f",
            presentation,
            static_cast<double>(runtime.current[status_index]),
            static_cast<double>(runtime.maximum[status_index]))
        : std::snprintf(
            output, output_size, "%s: %.0f / %.0f", presentation,
            static_cast<double>(runtime.current[status_index]),
            static_cast<double>(runtime.maximum[status_index]));
    return written > 0 && static_cast<std::size_t>(written) < output_size;
}

bool configure_directional_shield_native_tooltip_provider(
    int facing) noexcept {
    char normal[768]{};
    char verbose[768]{};
    if (!format_directional_shield_tooltip(
            facing, false, normal, sizeof(normal)) ||
        !format_directional_shield_tooltip(
            facing, true, verbose, sizeof(verbose))) {
        return false;
    }
    DirectionalShieldNativeTooltipProvider& provider =
        g_directional_shield_native_tooltip_provider;
    provider.vtable = at(g_armada, kStandardComponentVtableRva);
    std::snprintf(provider.normal_storage.data(),
                  provider.normal_storage.size(), "%s", normal);
    std::snprintf(provider.verbose_storage.data(),
                  provider.verbose_storage.size(), "%s", verbose);
    provider.normal = provider.normal_storage.data();
    provider.verbose = provider.verbose_storage.data();
    return readable_range(provider.vtable, 0x08);
}

void draw_directional_shield_hover_tooltip(
    void* info_display, void* craft, void* text_component,
    const NativeRectangle& live_anchor,
    const Colour& shared_colour) noexcept {
    // When the WireframeIcon callbacks are linked, Armada's real tooltip
    // manager owns timing, framing, placement, and normal/verbose switching.
    // Do not also draw the old experimental text overlay.
    if (g_directional_shield_wireframe_tooltips_available) return;
    const DirectionalShieldTooltipRuntime& runtime =
        g_directional_shield_tooltip;
    if (!g_runtime_ready || !info_display || !craft || !text_component ||
        !runtime.active || runtime.info_display != info_display ||
        runtime.craft != craft ||
        read_at<void*>(info_display,
                       kInfoDisplaySelectedCraftOffset, nullptr) != craft) {
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
    if (facing < 0 || facing >= 4) return;

    // Armada stores zero for its normal tooltip and one for the expanded
    // verbose presentation.  Fleet Operations does not reliably submit a
    // native tooltip object for the sprite-only shield arcs, so render the
    // selected localization in the already-active selected-panel pass.
    const std::int32_t tooltip_mode = read_at<std::int32_t>(
        at(g_armada, kTooltipModeRva), 0, 0);
    const bool verbose = tooltip_mode == 1;
    char text[768]{};
    if (!format_directional_shield_tooltip(
            facing, verbose, text, sizeof(text))) {
        return;
    }

    // Reuse the configured directional-shield text row.  Unlike the stock
    // systems/name region, this row belongs to the selected panel's proven
    // text clip and remains free while the graphical arc ring is active.
    NativeRectangle tooltip_rectangle = live_anchor;
    tooltip_rectangle.top += 108;
    tooltip_rectangle.bottom += 108;
    if (g_ui_configuration.captain_rectangle_found &&
        g_ui_configuration.directional_forward_aft_rectangle_found) {
        tooltip_rectangle = translated_rectangle(
            live_anchor, g_ui_configuration.captain_rectangle,
            g_ui_configuration.directional_forward_aft_rectangle);
    }
    tooltip_rectangle.right = tooltip_rectangle.left + 520;
    if (verbose) tooltip_rectangle.top -= 20;
    tooltip_rectangle.bottom = tooltip_rectangle.top +
        (verbose ? 58 : 24);
    const Colour shadow_colour{0.02f, 0.02f, 0.02f};
    NativeRectangle shadow_rectangle = tooltip_rectangle;
    ++shadow_rectangle.left;
    ++shadow_rectangle.right;
    ++shadow_rectangle.top;
    ++shadow_rectangle.bottom;
    draw_identity_text(
        text, shadow_rectangle, shadow_colour, text_component);
    const bool drawn = draw_identity_text(
        text, tooltip_rectangle, shared_colour, text_component);
    if (drawn && InterlockedCompareExchange(
            &g_directional_shield_direct_tooltip_report_count,
            1, 0) == 0) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "First direct directional-shield tooltip draw submitted: facing=%d mode=%ld cursor=%ld,%ld rectangle=%ld,%ld,%ld,%ld text='%s'",
            facing, static_cast<long>(tooltip_mode),
            static_cast<long>(cursor_x), static_cast<long>(cursor_y),
            static_cast<long>(tooltip_rectangle.left),
            static_cast<long>(tooltip_rectangle.top),
            static_cast<long>(tooltip_rectangle.right),
            static_cast<long>(tooltip_rectangle.bottom), text);
        log_line(message);
    }
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
    if (g_directional_shield_native_tooltip.presented &&
        g_directional_shield_native_tooltip_available) {
        a2fo_identity_call_thiscall_0(
            at(g_armada, kTooltipManagerClearRva),
            at(g_armada, kTooltipManagerObjectRva));
    }
    g_directional_shield_native_tooltip = {};
}

void present_directional_shield_native_tooltip(
    void* component, void* craft, int facing) noexcept {
    if (!g_directional_shield_native_tooltip_available || !component ||
        !craft || facing < 0 || facing >= 4 ||
        !readable_range(component, 0x08)) {
        clear_directional_shield_native_tooltip();
        return;
    }

    DirectionalShieldNativeTooltip& native =
        g_directional_shield_native_tooltip;
    const DWORD now = GetTickCount();
    if (native.component != component || native.craft != craft ||
        native.facing != facing) {
        clear_directional_shield_native_tooltip();
        native.component = component;
        native.craft = craft;
        native.facing = facing;
        native.hover_started = now;
    }

    void* owner = read_at<void*>(component, 0x04, nullptr);
    void* standard_component_vtable =
        at(g_armada, kStandardComponentVtableRva);
    if (!owner ||
        !readable_range(standard_component_vtable, 0x08) ||
        g_directional_shield_native_tooltip_provider.vtable !=
            standard_component_vtable ||
        !g_directional_shield_native_tooltip_provider.normal ||
        !g_directional_shield_native_tooltip_provider.verbose) {
        clear_directional_shield_native_tooltip();
        return;
    }
    // The selected-panel render happens after StandardComponent::Update has
    // already reset the wireframe's native hover clock for the sprite-only
    // arc.  Supplying a completed native hover age avoids that unrelated
    // rectangle reset while the exact per-arc hit test above remains the
    // authority for whether a tooltip may be shown.
    constexpr float elapsed = 3600.0f;
    std::uint32_t elapsed_bits = 0;
    std::memcpy(&elapsed_bits, &elapsed, sizeof(elapsed_bits));
    const bool presented =
        (a2fo_identity_call_thiscall_3(
             at(g_armada, kTooltipManagerShowRva),
             at(g_armada, kTooltipManagerObjectRva),
             elapsed_bits,
             reinterpret_cast<std::uintptr_t>(
                 &g_directional_shield_native_tooltip_provider),
             reinterpret_cast<std::uintptr_t>(owner)) & 0xffu) != 0;
    native.presented = presented;
    if (presented && InterlockedCompareExchange(
            &g_directional_shield_native_dispatch_report_count,
            1, 0) == 0) {
        char message[224]{};
        std::snprintf(
            message, sizeof(message),
            "First directional-shield tooltip presented by Armada's native manager: facing=%d provider=%p owner=%p elapsed=%.2f normal='%s'",
            facing,
            static_cast<void*>(
                &g_directional_shield_native_tooltip_provider), owner,
            static_cast<double>(elapsed),
            g_directional_shield_native_tooltip_provider.normal);
        log_line(message);
    }
}

void update_directional_shield_component_tooltip(
    void* info_display) noexcept {
    if (!g_directional_shield_component_tooltips_available ||
        !info_display) {
        return;
    }
    void* component = read_at<void*>(
        info_display, kInfoDisplayWireframeOffset, nullptr);
    if (!component || !readable_range(component, 0x28)) {
        g_directional_shield_tooltip_binding = {};
        return;
    }

    DirectionalShieldTooltipBinding& binding =
        g_directional_shield_tooltip_binding;
    if (binding.info_display != info_display ||
        binding.component != component) {
        // The previous component may have belonged to a discarded interface.
        // Never dereference it merely to clear our old copied text.
        binding = {};
        binding.info_display = info_display;
        binding.component = component;
    }

    const DirectionalShieldTooltipRuntime& runtime =
        g_directional_shield_tooltip;
    int facing = -1;
    std::int32_t cursor_x = std::numeric_limits<std::int32_t>::min();
    std::int32_t cursor_y = std::numeric_limits<std::int32_t>::min();
    if (runtime.active && runtime.info_display == info_display &&
        runtime.craft &&
        read_at<void*>(info_display,
                       kInfoDisplaySelectedCraftOffset, nullptr) ==
            runtime.craft) {
        cursor_x = read_at<std::int32_t>(
            at(g_armada, kCursorXRva), 0, cursor_x);
        cursor_y = read_at<std::int32_t>(
            at(g_armada, kCursorYRva), 0, cursor_y);
        facing = a2fo::craft_identity::directional_shield_segment_at(
            runtime.hit_rectangles,
            static_cast<float>(cursor_x),
            static_cast<float>(cursor_y));
    }

    if (facing < 0 || facing >= 4) {
        if (binding.facing >= 0) {
            set_standard_component_text(
                component, kStandardComponentSetTooltipTextRva, nullptr);
            set_standard_component_text(
                component, kStandardComponentSetVerboseTooltipTextRva,
                nullptr);
        }
        binding.craft = runtime.craft;
        binding.facing = -1;
        binding.current = -1.0f;
        binding.maximum = -1.0f;
        return;
    }

    const std::size_t index = static_cast<std::size_t>(facing);
    if (binding.craft == runtime.craft && binding.facing == facing &&
        binding.current == runtime.current[index] &&
        binding.maximum == runtime.maximum[index]) {
        return;
    }
    char normal[768]{};
    char verbose[768]{};
    if (!format_directional_shield_tooltip(
            facing, false, normal, sizeof(normal)) ||
        !format_directional_shield_tooltip(
            facing, true, verbose, sizeof(verbose))) {
        return;
    }
    set_standard_component_text(
        component, kStandardComponentSetTooltipTextRva, normal);
    set_standard_component_text(
        component, kStandardComponentSetVerboseTooltipTextRva, verbose);
    binding.craft = runtime.craft;
    binding.facing = facing;
    binding.current = runtime.current[index];
    binding.maximum = runtime.maximum[index];
    if (InterlockedCompareExchange(
            &g_directional_shield_component_assignment_report_count,
            1, 0) == 0) {
        char message[224]{};
        std::snprintf(
            message, sizeof(message),
            "First directional-shield component tooltip assigned: facing=%lu cursor=%ld,%ld component=%p",
            static_cast<unsigned long>(index),
            static_cast<long>(cursor_x), static_cast<long>(cursor_y),
            component);
        log_line(message);
    }
}

bool append_directional_shield_tooltip(
    void* stream_wrapper, bool verbose) noexcept {
    const DirectionalShieldTooltipRuntime& runtime =
        g_directional_shield_tooltip;
    if (!g_runtime_ready || !runtime.active || !runtime.info_display ||
        !runtime.craft || !stream_wrapper ||
        read_at<void*>(runtime.info_display,
                       kInfoDisplaySelectedCraftOffset, nullptr) !=
            runtime.craft) {
        return false;
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
            static_cast<float>(cursor_x), static_cast<float>(cursor_y));
    if (InterlockedCompareExchange(
            &g_directional_shield_tooltip_query_report_count, 1, 0) == 0) {
        char message[640]{};
        std::snprintf(
            message, sizeof(message),
            "First directional-shield tooltip query: cursor=%ld,%ld facing=%d; F=%.0f,%.0f,%.0f,%.0f A=%.0f,%.0f,%.0f,%.0f P=%.0f,%.0f,%.0f,%.0f S=%.0f,%.0f,%.0f,%.0f",
            static_cast<long>(cursor_x), static_cast<long>(cursor_y), facing,
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
    if (facing < 0 || facing >= 4) return false;
    const std::size_t index = static_cast<std::size_t>(facing);
    if (InterlockedCompareExchange(
            &g_directional_shield_tooltip_hit_report_count, 1, 0) == 0) {
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "First directional-shield tooltip hit: facing=%lu cursor=%ld,%ld",
            static_cast<unsigned long>(index),
            static_cast<long>(cursor_x), static_cast<long>(cursor_y));
        log_line(message);
    }

    char text[768]{};
    if (!format_directional_shield_tooltip(
            facing, verbose, text, sizeof(text))) return false;

    InsertCString insert = insert_c_string();
    if (!insert || !readable_range(stream_wrapper, 0x0c)) {
        return false;
    }
    // WireframeIcon::GetTooltip and GetVerboseTooltip receive Armada's stream
    // wrapper. The actual ostream subobject consumed by operator<< begins at
    // +8, exactly as it does in the stock WireframeIcon implementations.
    insert(static_cast<std::uint8_t*>(stream_wrapper) + 0x08, text);
    return true;
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
    const DirectionalShieldTooltipRuntime& runtime =
        g_directional_shield_tooltip;
    if (!wireframe_icon || !saved_rectangle || !runtime.active ||
        !runtime.info_display || !runtime.craft ||
        read_at<void*>(runtime.info_display,
                       kInfoDisplayWireframeOffset, nullptr) !=
            wireframe_icon ||
        read_at<void*>(runtime.info_display,
                       kInfoDisplaySelectedCraftOffset, nullptr) !=
            runtime.craft ||
        !readable_range(wireframe_icon, 0x28)) {
        return false;
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
    if (facing < 0 || facing >= 4) return false;

    void* owner = read_at<void*>(wireframe_icon, 0x04, nullptr);
    void* owner_vtable = read_at<void*>(owner, 0, nullptr);
    void* get_owner_rectangle = read_at<void*>(
        owner_vtable, 0x64, nullptr);
    if (!owner || !owner_vtable || !get_owner_rectangle) return false;
    const void* owner_rectangle = reinterpret_cast<const void*>(
        a2fo_identity_call_thiscall_0(get_owner_rectangle, owner));
    if (!readable_range(owner_rectangle, 0x08)) return false;
    const std::int32_t owner_x = read_at<std::int32_t>(
        owner_rectangle, 0, 0);
    const std::int32_t owner_y = read_at<std::int32_t>(
        owner_rectangle, 4, 0);

    *saved_rectangle = read_at<NativeRectangle>(
        wireframe_icon, 0x08, NativeRectangle{});
    const FloatRectangle& hit = runtime.hit_rectangles[
        static_cast<std::size_t>(facing)];
    const NativeRectangle hover_rectangle{
        static_cast<std::int32_t>(std::floor(hit.x)) - owner_x,
        static_cast<std::int32_t>(std::floor(hit.y)) - owner_y,
        static_cast<std::int32_t>(std::ceil(hit.x + hit.width)) -
            owner_x - 1,
        static_cast<std::int32_t>(std::ceil(hit.y + hit.height)) -
            owner_y - 1,
    };
    std::memcpy(
        static_cast<std::uint8_t*>(wireframe_icon) + 0x08,
        &hover_rectangle, sizeof(hover_rectangle));

    if (InterlockedCompareExchange(
            &g_directional_shield_hover_rectangle_report_count,
            1, 0) == 0) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "First directional-shield arc routed through WireframeIcon's native hover rectangle: facing=%d cursor=%ld,%ld ownerOrigin=%ld,%ld local=%ld,%ld,%ld,%ld",
            facing, static_cast<long>(cursor_x),
            static_cast<long>(cursor_y), static_cast<long>(owner_x),
            static_cast<long>(owner_y),
            static_cast<long>(hover_rectangle.left),
            static_cast<long>(hover_rectangle.top),
            static_cast<long>(hover_rectangle.right),
            static_cast<long>(hover_rectangle.bottom));
        log_line(message);
    }
    return true;
}

[[maybe_unused]] void __attribute__((fastcall)) wireframe_update_hook(
    void* wireframe_icon, void*) noexcept {
    NativeRectangle saved_rectangle{};
    const bool rectangle_overridden =
        apply_directional_shield_hover_rectangle(
            wireframe_icon, &saved_rectangle);
    if (g_wireframe_update_original) {
        a2fo_identity_call_thiscall_0(
            g_wireframe_update_original, wireframe_icon);
    }
    if (rectangle_overridden &&
        readable_range(wireframe_icon, 0x18)) {
        std::memcpy(
            static_cast<std::uint8_t*>(wireframe_icon) + 0x08,
            &saved_rectangle, sizeof(saved_rectangle));
    }
}

void reset_directional_shield_hover_components() noexcept {
    bool presented = false;
    for (const DirectionalShieldHoverComponent& component :
         g_directional_shield_hover_components.components) {
        presented = presented || component.presented != 0;
    }
    if (presented && readable_range(
            at(g_armada, kTooltipManagerClearRva), 1)) {
        a2fo_identity_call_thiscall_0(
            at(g_armada, kTooltipManagerClearRva),
            at(g_armada, kTooltipManagerObjectRva));
    }
    g_directional_shield_hover_components = {};
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

    for (std::size_t index = 0; index < hover.components.size(); ++index) {
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
                reset_directional_shield_hover_components();
                return;
            }
            component.normal = component.normal_storage.data();
            component.verbose = component.verbose_storage.data();
            hover.current[index] = runtime.current[index];
            hover.maximum[index] = runtime.maximum[index];
        }
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
    for (std::size_t index = 0; index < hover.components.size(); ++index) {
        if (static_cast<int>(index) == facing) continue;
        a2fo_identity_call_thiscall_0(
            update, &hover.components[index]);
    }
    if (facing >= 0 && facing < 4) {
        DirectionalShieldHoverComponent& component =
            hover.components[static_cast<std::size_t>(facing)];
        a2fo_identity_call_thiscall_0(update, &component);
        if (component.presented != 0 &&
            InterlockedCompareExchange(
                &g_directional_shield_hover_presented_report_count,
                1, 0) == 0) {
            char message[256]{};
            std::snprintf(
                message, sizeof(message),
                "First directional-shield native hover component accepted by TooltipManager: facing=%d normal='%s'",
                facing, component.normal ? component.normal : "");
            log_line(message);
        }
    }
}

void reset_ammunition_hover_components() noexcept {
    bool presented = false;
    for (const DirectionalShieldHoverComponent& component :
         g_ammunition_hover_components.components) {
        presented = presented || component.presented != 0;
    }
    if (presented && readable_range(
            at(g_armada, kTooltipManagerClearRva), 1)) {
        a2fo_identity_call_thiscall_0(
            at(g_armada, kTooltipManagerClearRva),
            at(g_armada, kTooltipManagerObjectRva));
    }
    g_ammunition_hover_components = {};
}

int ammunition_store_at(
    const AmmunitionTooltipRuntime& runtime,
    float cursor_x, float cursor_y) noexcept {
    if (!std::isfinite(cursor_x) || !std::isfinite(cursor_y)) return -1;
    for (std::size_t index = 0; index < runtime.hit_rectangles.size();
         ++index) {
        if (!runtime.visible[index]) continue;
        const FloatRectangle& rectangle = runtime.hit_rectangles[index];
        if (usable_float_rectangle(rectangle) &&
            cursor_x >= rectangle.x &&
            cursor_x <= rectangle.x + rectangle.width &&
            cursor_y >= rectangle.y &&
            cursor_y <= rectangle.y + rectangle.height) {
            return static_cast<int>(index);
        }
    }
    return -1;
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

    for (std::size_t index = 0; index < hover.components.size(); ++index) {
        if (!runtime.visible[index]) continue;
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
                reset_ammunition_hover_components();
                return;
            }
            component.normal = component.normal_storage.data();
            component.verbose = component.verbose_storage.data();
            hover.current[index] = runtime.current[index];
            hover.maximum[index] = runtime.maximum[index];
        }
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
    for (std::size_t index = 0; index < hover.components.size(); ++index) {
        if (!runtime.visible[index] ||
            static_cast<int>(index) == selected) continue;
        a2fo_identity_call_thiscall_0(update, &hover.components[index]);
    }
    if (selected >= 0 && selected < 2) {
        DirectionalShieldHoverComponent& component =
            hover.components[static_cast<std::size_t>(selected)];
        a2fo_identity_call_thiscall_0(update, &component);
        if (component.presented != 0 &&
            InterlockedCompareExchange(
                &g_ammunition_hover_presented_report_count, 1, 0) == 0) {
            char message[256]{};
            std::snprintf(
                message, sizeof(message),
                "First ammunition native hover component accepted by TooltipManager: store=%d normal='%s'",
                selected, component.normal ? component.normal : "");
            log_line(message);
        }
    }
}

void reset_selected_status_hover_components() noexcept {
    bool presented = false;
    for (const DirectionalShieldHoverComponent& component :
         g_selected_status_hover_components.components) {
        presented = presented || component.presented != 0;
    }
    if (presented && readable_range(
            at(g_armada, kTooltipManagerClearRva), 1)) {
        a2fo_identity_call_thiscall_0(
            at(g_armada, kTooltipManagerClearRva),
            at(g_armada, kTooltipManagerObjectRva));
    }
    g_selected_status_hover_components = {};
}

int selected_status_at(
    const SelectedStatusTooltipRuntime& runtime,
    float cursor_x, float cursor_y) noexcept {
    if (!std::isfinite(cursor_x) || !std::isfinite(cursor_y)) return -1;
    for (std::size_t index = 0; index < runtime.hit_rectangles.size();
         ++index) {
        if (!runtime.visible[index]) continue;
        const FloatRectangle& rectangle = runtime.hit_rectangles[index];
        if (usable_float_rectangle(rectangle) &&
            cursor_x >= rectangle.x &&
            cursor_x <= rectangle.x + rectangle.width &&
            cursor_y >= rectangle.y &&
            cursor_y <= rectangle.y + rectangle.height) {
            return static_cast<int>(index);
        }
    }
    return -1;
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

    for (std::size_t index = 0; index < hover.components.size(); ++index) {
        if (!runtime.visible[index]) continue;
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
                reset_selected_status_hover_components();
                return;
            }
            component.normal = component.normal_storage.data();
            component.verbose = component.verbose_storage.data();
            hover.current[index] = runtime.current[index];
            hover.maximum[index] = runtime.maximum[index];
        }
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
    for (std::size_t index = 0; index < hover.components.size(); ++index) {
        if (!runtime.visible[index] ||
            static_cast<int>(index) == selected) continue;
        a2fo_identity_call_thiscall_0(update, &hover.components[index]);
    }
    if (selected >= 0 && selected < 2) {
        DirectionalShieldHoverComponent& component =
            hover.components[static_cast<std::size_t>(selected)];
        a2fo_identity_call_thiscall_0(update, &component);
        if (component.presented != 0 &&
            InterlockedCompareExchange(
                &g_selected_status_hover_presented_report_count,
                1, 0) == 0) {
            char message[256]{};
            std::snprintf(
                message, sizeof(message),
                "First selected-status tooltip accepted: status=%d normal='%s'",
                selected, component.normal ? component.normal : "");
            log_line(message);
        }
    }
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
    if (g_directional_shield_wireframe_tooltips_available) return;
    const DirectionalShieldTooltipRuntime& runtime =
        g_directional_shield_tooltip;
    if (!g_runtime_ready || !info_display || !runtime.active ||
        runtime.info_display != info_display || !runtime.craft ||
        read_at<void*>(info_display,
                       kInfoDisplaySelectedCraftOffset, nullptr) !=
            runtime.craft) {
        clear_directional_shield_native_tooltip();
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
    void* component = read_at<void*>(
        info_display, kInfoDisplayWireframeOffset, nullptr);
    if (facing < 0 || facing >= 4 || !component) {
        clear_directional_shield_native_tooltip();
        return;
    }
    if (!configure_directional_shield_native_tooltip_provider(facing)) {
        clear_directional_shield_native_tooltip();
        return;
    }
    DirectionalShieldNativeTooltipProvider& provider =
        g_directional_shield_native_tooltip_provider;
    set_standard_component_text(
        component, kStandardComponentSetTooltipTextRva, provider.normal);
    set_standard_component_text(
        component, kStandardComponentSetVerboseTooltipTextRva,
        provider.verbose);
    present_directional_shield_native_tooltip(
        component, runtime.craft, facing);
}

[[maybe_unused]] std::uintptr_t __attribute__((fastcall)) tooltip_manager_show_hook(
    void* tooltip_manager, void*, std::uintptr_t elapsed,
    void* provider, void* owner) noexcept {
    const DirectionalShieldTooltipRuntime& runtime =
        g_directional_shield_tooltip;
    int facing = -1;
    if (tooltip_manager == at(g_armada, kTooltipManagerObjectRva) &&
        runtime.active && runtime.info_display && runtime.craft &&
        read_at<void*>(runtime.info_display,
                       kInfoDisplaySelectedCraftOffset, nullptr) ==
            runtime.craft) {
        const std::int32_t cursor_x = read_at<std::int32_t>(
            at(g_armada, kCursorXRva), 0,
            std::numeric_limits<std::int32_t>::min());
        const std::int32_t cursor_y = read_at<std::int32_t>(
            at(g_armada, kCursorYRva), 0,
            std::numeric_limits<std::int32_t>::min());
        facing = a2fo::craft_identity::directional_shield_segment_at(
            runtime.hit_rectangles,
            static_cast<float>(cursor_x),
            static_cast<float>(cursor_y));
        if (facing >= 0 && facing < 4 &&
            configure_directional_shield_native_tooltip_provider(facing)) {
            provider = &g_directional_shield_native_tooltip_provider;
            if (InterlockedCompareExchange(
                    &g_directional_shield_show_substitution_report_count,
                    1, 0) == 0) {
                char message[256]{};
                std::snprintf(
                    message, sizeof(message),
                    "First stock tooltip request replaced at creation: facing=%d provider=%p normal='%s'",
                    facing, provider,
                    g_directional_shield_native_tooltip_provider.normal);
                log_line(message);
            }
        }
    }
    if (!g_tooltip_manager_show_hook.gateway) return 0;
    return a2fo_identity_call_thiscall_3(
        g_tooltip_manager_show_hook.gateway, tooltip_manager,
        elapsed, reinterpret_cast<std::uintptr_t>(provider),
        reinterpret_cast<std::uintptr_t>(owner));
}

[[maybe_unused]] void __attribute__((fastcall)) tooltip_manager_render_hook(
    void* tooltip_manager, void*) noexcept {
    const DirectionalShieldTooltipRuntime& runtime =
        g_directional_shield_tooltip;
    if (tooltip_manager == at(g_armada, kTooltipManagerObjectRva) &&
        runtime.active && runtime.info_display) {
        // Acquire/update the arc tooltip immediately before Armada's own
        // top-level tooltip render. This is the global screen-space context;
        // drawing it from InfoDisplay leaves the tooltip inside that panel's
        // origin and scissor and Fleet Operations clips it away.
        update_directional_shield_native_tooltip_phase(
            runtime.info_display);
    }
    if (g_tooltip_manager_render_hook.gateway) {
        a2fo_identity_call_thiscall_0(
            g_tooltip_manager_render_hook.gateway, tooltip_manager);
    }
    if (g_directional_shield_native_tooltip.presented &&
        InterlockedCompareExchange(
            &g_directional_shield_native_render_report_count,
            1, 0) == 0) {
        log_line(
            "First directional-shield tooltip passed through Armada's top-level framed render stage");
    }
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
    auto result = a2fo::craft_identity::resolve_selected_panel_anchor(
        candidate(kInfoDisplayCaptainTextOffset, nullptr),
        ui.captain_rectangle_found ? &ui.captain_rectangle : nullptr,
        {{candidate(builder ? kInfoDisplayBuilderNameTextOffset
                            : kInfoDisplayNameTextOffsets[0], name_rectangle),
          candidate(builder ? kInfoDisplayBuilderClassTextOffset
                            : kInfoDisplayClassTextOffset, class_rectangle)}});
    if (result.component) return result;
    // Hiding the captain rectangle must not hide independently configured
    // ammunition/registry/XP. The native captain still supplies font/display
    // state even when its own rectangle is empty.
    void* captain = read_at<void*>(info_display, kInfoDisplayCaptainTextOffset, nullptr);
    if (captain && ui.captain_rectangle_found) {
        const auto& raw = ui.captain_rectangle;
        const auto offset = read_at<std::int32_t>(info_display, 0x1f0, 0);
        return {captain, {raw.x, raw.y - offset,
            raw.x + std::max<std::int32_t>(2, raw.width) - 1,
            raw.y - offset + std::max<std::int32_t>(2, raw.height) - 1}};
    }
    return {};
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

    std::array<TextColourOverride, 2> ship_name_colour_overrides{};
    if (g_runtime_ready && info_display) {
        void* parameter_db = gui_parameter_db();
        if (!g_ui_configuration.loaded ||
            g_ui_configuration.parameter_db != parameter_db) {
            refresh_ui_configuration(parameter_db);
        }
        if (g_ui_configuration.ship_name_colour_found) {
            bool applied = false;
            for (std::size_t index = 0;
                 index < kInfoDisplayNameTextOffsets.size(); ++index) {
                ship_name_colour_overrides[index] =
                    override_text_component_colour(
                        info_display, kInfoDisplayNameTextOffsets[index],
                        g_ui_configuration.ship_name_colour);
                applied = applied ||
                    ship_name_colour_overrides[index].address != nullptr;
            }
            if (applied && InterlockedCompareExchange(
                    &g_ship_name_colour_report_count, 1, 0) == 0) {
                log_line(
                    "shipNameColor applied to the native selected ship-name text components");
            }
        }
    }
    if (native_renderer) {
        a2fo_identity_call_thiscall_0(native_renderer, info_display);
    }
    for (auto iterator = ship_name_colour_overrides.rbegin();
         iterator != ship_name_colour_overrides.rend(); ++iterator) {
        restore_text_component_colour(*iterator);
    }
    if (!g_runtime_ready || !info_display) return;

    // The selected-info render pass resolves the one selected object into
    // +0x1e8 before drawing its middle/tall panel. Multiple selection and no
    // selection both leave this null, so identities never leak back into
    // SDInfoBar mouseover. Drawing here also retains the selected panel's live
    // display/scissor state; the later generic info-display pass has already
    // closed that context.
    void* craft = read_at<void*>(
        info_display, kInfoDisplaySelectedCraftOffset, nullptr);
    if (!craft) {
        reset_directional_shield_hover_components();
        reset_ammunition_hover_components();
        reset_selected_status_hover_components();
        update_directional_shield_component_tooltip(info_display);
        return;
    }

    void* parameter_db = gui_parameter_db();
    if (!g_ui_configuration.loaded ||
        g_ui_configuration.parameter_db != parameter_db) {
        refresh_ui_configuration(parameter_db);
    }

    const SelectedPanelTextAnchor anchor = selected_panel_text_anchor(
        info_display, panel_kind);
    if (!anchor.component ||
        !usable_native_rectangle(anchor.captain_rectangle)) {
        update_directional_shield_hover_components(info_display);
        update_ammunition_hover_components(info_display);
        reset_selected_status_hover_components();
        update_directional_shield_component_tooltip(info_display);
        return;
    }

    void* text_component = anchor.component;
    const NativeRectangle captain_native_rectangle =
        anchor.captain_rectangle;
    if (panel_kind == SelectedPanelKind::builder &&
        InterlockedCompareExchange(
            &g_builder_panel_anchor_report_count, 1, 0) == 0) {
        char message[288]{};
        std::snprintf(
            message, sizeof(message),
            "Tall producer selected-panel extension anchor active: component=%p captain-relative rectangle=%ld,%ld,%ld,%ld",
            text_component,
            static_cast<long>(captain_native_rectangle.left),
            static_cast<long>(captain_native_rectangle.top),
            static_cast<long>(captain_native_rectangle.right),
            static_cast<long>(captain_native_rectangle.bottom));
        log_line(message);
    }
    const Colour native_colour = text_component_colour(text_component);
    const Colour shared_colour = g_ui_configuration.shared_text_colour_found
        ? g_ui_configuration.shared_text_colour : native_colour;
    const Colour captain_colour = g_ui_configuration.captain_colour_found
        ? g_ui_configuration.captain_colour : shared_colour;
    const Colour registry_colour = g_ui_configuration.registry_colour_found
        ? g_ui_configuration.registry_colour : shared_colour;

    draw_ammunition_rows(
        info_display, craft, text_component,
        captain_native_rectangle, shared_colour);
    draw_directional_shield_rows(
        info_display, craft, text_component,
        captain_native_rectangle, shared_colour);
    draw_selected_status_bars(
        info_display, craft, text_component, captain_native_rectangle);
    // Fleet Operations' selected wireframe uses its own Update override and
    // therefore does not reliably pass through StandardComponent::Update.
    // The shield ring is finalised in this proven selected-panel render pass,
    // so update the four synthetic StandardComponents here while their live
    // screen rectangles and owning interface are current. Armada's native
    // manager still owns hover timing, normal/verbose selection and drawing.
    update_ammunition_hover_components(info_display);
    update_selected_status_hover_components(info_display);
    update_directional_shield_hover_components(info_display);
    update_directional_shield_component_tooltip(info_display);
    // Producers (including repair-only shipyards) are still Craft. Their
    // native name row and optional identity strings use the same selection
    // policy and configured rectangles as the ordinary selected panel.
    const CraftIdentity* identity = ensure_craft_identity(craft);
    if (!identity) {
        draw_directional_shield_hover_tooltip(
            info_display, craft, text_component,
            captain_native_rectangle, shared_colour);
        return;
    }

    NativeRectangle registry_native_rectangle = captain_native_rectangle;
    if (g_ui_configuration.captain_rectangle_found &&
        g_ui_configuration.registry_rectangle_found) {
        registry_native_rectangle = translated_rectangle(
            captain_native_rectangle,
            g_ui_configuration.captain_rectangle,
            g_ui_configuration.registry_rectangle);
    }
    bool captain_drawn = false;
    bool registry_drawn = false;

    if (g_ui_configuration.captain_rectangle_found ||
        panel_style(PanelElement::CaptainText).area_found) {
        captain_drawn = draw_identity_text(
            identity->captain_name,
            panel_rectangle(PanelElement::CaptainText, captain_native_rectangle),
            panel_colour(PanelElement::CaptainText, captain_colour), text_component);
    }
    if ((g_ui_configuration.captain_rectangle_found &&
         g_ui_configuration.registry_rectangle_found) ||
        panel_style(PanelElement::RegistryText).area_found) {
        registry_drawn = draw_identity_text(
            identity->craft_registry,
            panel_rectangle(PanelElement::RegistryText, registry_native_rectangle),
            panel_colour(PanelElement::RegistryText, registry_colour), text_component);
    }
    if ((captain_drawn || registry_drawn) &&
        InterlockedCompareExchange(&g_draw_report_count, 1, 0) == 0) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "First selected identity draw submitted on %s panel for native name row %ld: "
            "captain=%s at %ld,%ld; registry=%s at %ld,%ld",
            panel_kind == SelectedPanelKind::builder ? "builder" : "single",
            static_cast<long>(identity->craft_name_index),
            captain_drawn ? "yes" : "no",
            static_cast<long>(captain_native_rectangle.left),
            static_cast<long>(captain_native_rectangle.top),
            registry_drawn ? "yes" : "no",
            static_cast<long>(registry_native_rectangle.left),
            static_cast<long>(registry_native_rectangle.top));
        log_line(message);
    }
    draw_directional_shield_hover_tooltip(
        info_display, craft, text_component,
        captain_native_rectangle, shared_colour);
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
    if (!address || !exact_bytes_match(module, rva, expected)) return false;
    DWORD old_protect = 0;
    if (!VirtualProtect(
            address, Size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return false;
    }
    bool changed = std::memcmp(address, expected.data(), Size) == 0;
    if (changed) {
        std::memcpy(address, replacement.data(), Size);
        FlushInstructionCache(GetCurrentProcess(), address, Size);
    }
    DWORD restored = 0;
    VirtualProtect(address, Size, old_protect, &restored);
    return changed;
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
    void* site = at(g_armada, kCraftClassConstructorRva);
    if (signature_matches(
            g_armada, kCraftClassConstructorRva,
            kExpectedCraftClassConstructor)) {
        if (!api->install_inline_hook(
                site,
                reinterpret_cast<void*>(&craft_class_constructor_hook),
                sizeof(kExpectedCraftClassConstructor),
                kExpectedCraftClassConstructor,
                &g_craft_class_constructor_hook)) {
            return false;
        }
        g_craft_class_constructor_original =
            g_craft_class_constructor_hook.gateway;
        g_chained_fo_craft_class_constructor = false;
        return g_craft_class_constructor_original != nullptr;
    }

    void* destination = nullptr;
    std::size_t patch_length = 0;
    if (!supported_fleet_ops_craft_class_detour(
            &destination, &patch_length) || patch_length > 6) {
        return false;
    }
    std::uint8_t expected_detour[6]{};
    std::memcpy(expected_detour, site, patch_length);
    if (!api->patch_jump(
            site, reinterpret_cast<void*>(&craft_class_constructor_hook),
            expected_detour, patch_length)) {
        return false;
    }
    g_craft_class_constructor_original = destination;
    g_chained_fo_craft_class_constructor = true;
    return true;
}

bool install_system_icon_colour_hooks(
    const A2FO_ModuleApi* api) noexcept {
    if (!api || !api->patch_call) return false;
    bool installed = true;
    for (const CheckedCallSite& site : kSystemIconColourCallSites) {
        installed = api->patch_call(
            at(g_armada, site.rva),
            reinterpret_cast<void*>(
                &a2fo_identity_system_icon_set_colour_bridge),
            site.expected.data(), site.expected.size()) && installed;
    }
    for (const CheckedCallSite& site : kWeaponIconColourCallSites) {
        installed = api->patch_call(
            at(g_fleet_ops, site.rva),
            reinterpret_cast<void*>(
                &a2fo_identity_weapon_icon_set_colour_bridge),
            site.expected.data(), site.expected.size()) && installed;
    }
    installed = api->patch_call(
        at(g_armada, kSystemValueIconColourCallSite.rva),
        reinterpret_cast<void*>(
            &a2fo_identity_system_text_set_colour_bridge),
        kSystemValueIconColourCallSite.expected.data(),
        kSystemValueIconColourCallSite.expected.size()) && installed;
    std::array<std::uint8_t, 5> value_text_expected{};
    const void* value_text_site = at(
        g_armada, kValueTextDrawCallSite.rva);
    if (!readable_range(value_text_site, value_text_expected.size()) ||
        !a2fo_identity_value_text_draw_original) {
        return false;
    }
    std::memcpy(value_text_expected.data(), value_text_site,
                value_text_expected.size());
    installed = api->patch_call(
        const_cast<void*>(value_text_site),
        reinterpret_cast<void*>(
            &a2fo_identity_value_text_draw_bridge),
        value_text_expected.data(), value_text_expected.size()) && installed;
    return installed;
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
    // The older setter, synthetic TooltipManager::Show, and direct-render
    // experiments must stay disabled when using the component's genuine
    // tooltip callbacks. Otherwise they replace or clear the provider that
    // Armada selected from the hovered weapon/wireframe icon.
    g_directional_shield_native_tooltip_available = false;
    g_directional_shield_component_tooltips_available = false;
    g_directional_shield_wireframe_tooltips_available = false;
    void* standard_component_vtable = at(
        g_armada, kStandardComponentVtableRva);
    if (!api || !api->install_inline_hook || !signature_matches(
            g_armada, kStandardComponentUpdateRva,
            kExpectedStandardComponentUpdate) ||
        !readable_range(standard_component_vtable, 0x08) ||
        read_at<void*>(standard_component_vtable, 0, nullptr) !=
            at(g_armada, kStandardComponentTooltipRva) ||
        read_at<void*>(standard_component_vtable, 4, nullptr) !=
            at(g_armada, kStandardComponentVerboseTooltipRva) ||
        !insert_c_string()) {
        log_line(
            "Armada StandardComponent hover path was unavailable; "
            "directional-shield arc tooltips disabled");
        return;
    }
    if (!api->install_inline_hook(
            at(g_armada, kStandardComponentUpdateRva),
            reinterpret_cast<void*>(&standard_component_update_hook),
            sizeof(kExpectedStandardComponentUpdate),
            kExpectedStandardComponentUpdate,
            &g_standard_component_update_hook)) {
        log_line(
            "Armada StandardComponent update hook could not be installed; "
            "directional-shield arc tooltips disabled");
        return;
    }
    g_directional_shield_wireframe_tooltips_available = true;
    log_line(
        "Four directional-shield arcs registered through the selected-panel StandardComponent update path");
}

bool install_runtime_hooks(const A2FO_ModuleApi* api) noexcept {
    if (!api || !api->install_inline_hook || !api->patch_jump ||
        !api->patch_call ||
        !g_armada || !g_fleet_ops) {
        return false;
    }
    if (!preflight_signatures()) {
        log_line("Supported ArmadaL signatures were not found; runtime disabled");
        return false;
    }
    bool installed = install_craft_class_constructor_hook(api);
    installed = install_system_icon_colour_hooks(api) && installed;
    installed = api->install_inline_hook(
        at(g_armada, kSelectedInfoUpdateRva),
        reinterpret_cast<void*>(&selected_info_update_hook),
        sizeof(kExpectedSelectedInfoUpdate), kExpectedSelectedInfoUpdate,
        &g_selected_info_update_hook) && installed;
    installed = api->install_inline_hook(
        at(g_armada, kSelectedBuilderInfoRenderRva),
        reinterpret_cast<void*>(&selected_builder_info_render_hook),
        sizeof(kExpectedSelectedInfoRender), kExpectedSelectedInfoRender,
        &g_selected_builder_info_render_hook) && installed;
    installed = api->install_inline_hook(
        at(g_armada, kSelectedInfoRenderRva),
        reinterpret_cast<void*>(&selected_info_render_hook),
        sizeof(kExpectedSelectedInfoRender), kExpectedSelectedInfoRender,
        &g_selected_info_render_hook) && installed;
    installed = a2fo::craft_identity::install_extended_weapon_icons(
        api, g_armada, g_fleet_ops) && installed;
    installed = install_ship_system_icon_render_policy_hook() && installed;
    // This is the final required mutation. If an earlier hook failed, leave
    // the native special-weapon early exit untouched rather than exposing a
    // partial policy.
    if (installed) {
        installed =
            install_special_weapon_disabled_presentation_patch();
    }
    if (installed) install_directional_shield_tooltip_hooks(api);
    if (!installed) {
        log_line("A craft identity hook could not be installed; hooks fail closed");
    }
    return installed;
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
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->install_inline_hook || !api->patch_jump || !api->patch_call ||
        !api->extension_root_count || !api->extension_root) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !g_fleet_ops) return false;

    constexpr const char* kPassiveWeaponFields[]{"classLabel"};
    const bool passive_weapon_registration =
        A2FO_MODULE_API_HAS(api, register_weapon_class_loaded_handler) &&
        api->register_weapon_class_loaded_handler &&
        (api->capabilities & A2FO_CAP_WEAPON_CLASS_LOADED) != 0 &&
        api->register_weapon_class_loaded_handler(
            kModuleName, kPassiveWeaponFields,
            static_cast<std::uint32_t>(
                sizeof(kPassiveWeaponFields) /
                sizeof(kPassiveWeaponFields[0])),
            &weapon_class_loaded_handler, nullptr);
    if (!passive_weapon_registration) {
        log_line(
            "Shared WeaponClass loader is unavailable; UtilityWeapon icons cannot use passive neutral presentation");
    }

    load_directional_shield_display_config();
    resolve_shield_class_observer();
    g_runtime_ready = install_runtime_hooks(api);
    if (g_runtime_ready) {
        a2fo::object_editor::initialize(api, g_armada, &read_editor_identity_defaults);
        log_line(g_chained_fo_craft_class_constructor
            ? "Selected-panel craft identity runtime initialized and chained "
              "through Fleet Operations CraftClass construction"
            : "Selected-panel craft identity runtime initialized");
    } else {
        log_line("Craft identity module loaded with runtime disabled");
    }
    // Inline hooks are process-lifetime patches. Keep a partially installed
    // module resident and make each hook a pass-through when setup failed.
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Process-lifetime hooks are owned by the core and intentionally remain.
}
