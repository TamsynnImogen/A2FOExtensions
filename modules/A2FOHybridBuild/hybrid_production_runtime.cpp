#include "hybrid_production_runtime.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
void* a2fo_hybrid_build_position_load_continue = nullptr;
void a2fo_hybrid_build_position_load_dispatch();
void* __cdecl a2fo_hybrid_build_position_for_command(
    void* producer, void* command) noexcept;
}

namespace a2fo {
namespace {

constexpr const char* kModuleName = "A2FOHybridBuild";

// ArmadaL.exe RVAs from the supported Armada 1.1 PDB plus the .text RVA.
constexpr std::uintptr_t kParameterDbGetProjectIdRva = 0x135200;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x135350;
constexpr std::uintptr_t kGameObjectClassFindProjectIdRva = 0x0cd1f0;
constexpr std::uintptr_t kProducerIsBusyRva = 0x0b0210;
constexpr std::uintptr_t kBuildCommandCleanupIsBusyQueryRva = 0x031194;
constexpr std::uintptr_t kBuildCommandCleanupIsBusyReturnRva = 0x0311a1;
constexpr std::uintptr_t kBuildCommandAdmitIsBusyQueryRva = 0x031495;
constexpr std::uintptr_t kBuildCommandAdmitIsBusyReturnRva = 0x03149f;
constexpr std::uintptr_t kBuildCommandCleanupPopQueryRva = 0x0311a5;
constexpr std::uintptr_t kBuildCommandCleanupPopReturnRva = 0x0311ac;
constexpr std::uintptr_t kBuildCommandReplacePopQueryRva = 0x031525;
constexpr std::uintptr_t kBuildCommandReplacePopReturnRva = 0x03152c;
constexpr std::uintptr_t kBuildCommandPositionInterfaceLoadRva = 0x0314d4;
constexpr std::uintptr_t kBuildCommandPositionInterfaceContinueRva =
    0x0314da;
constexpr std::uintptr_t kProducerGetActionRva = 0x0b80f0;
constexpr std::uintptr_t kConstructionRigGetActionRva = 0x0afa30;
constexpr std::uintptr_t kConstructionRigGetActionBusyReturnRva = 0x0afa3f;
constexpr std::uintptr_t kConstructionRigStartRva = 0x0afbc0;
constexpr std::uintptr_t kConstructionRigStartHardpointCallRva = 0x0afbe8;
constexpr std::uintptr_t kConstructionRigBuildHardpointsRva = 0x075860;
constexpr std::uintptr_t kConstructionRigCancelRva = 0x0aff00;
constexpr std::uintptr_t kConstructionRigFinishRva = 0x0aff90;
constexpr std::uintptr_t kConstructionRigRemoveObjectRva = 0x0afea0;
constexpr std::uintptr_t kConstructionRigConstructionMatrixRva = 0x0afba0;
constexpr std::uintptr_t kConstructionRigVtableRva = 0x2b22ec;
constexpr std::uintptr_t kBuildPositionInterfaceConstructorRva = 0x0adc40;
constexpr std::uintptr_t kBuildPositionInterfaceDestructorRva = 0x0adc70;
constexpr std::uintptr_t kPlaceholderRenderInternalRva = 0x073aa0;
constexpr std::uintptr_t kGameOperatorNewRva = 0x252710;
constexpr std::uintptr_t kGameOperatorDeleteRva = 0x2527d0;
constexpr std::uintptr_t kNullObjectIdGlobalRva = 0x2b41d8;
constexpr std::uintptr_t kProducerConstructionMatrixRva = 0x0b9170;
constexpr std::uintptr_t kProducerStartConstructionEffectRva = 0x0b8140;
constexpr std::uintptr_t kProducerCancelConstructionEffectRva = 0x0b8470;
constexpr std::uintptr_t kProducerUpdateConstructionEffectRva = 0x0b8dd0;
constexpr std::uintptr_t kProducerStopConstructionEffectRva = 0x0b8f30;
constexpr std::uintptr_t kProducerUpdateBuildButtonsRva = 0x0b8c10;
constexpr std::uintptr_t kResearchStationStartRva = 0x0ba0e0;
constexpr std::uintptr_t kResearchStationCancelRva = 0x0ba1b0;
constexpr std::uintptr_t kResearchStationCanBuildRva = 0x0ba280;
constexpr std::uintptr_t kResearchStationItemConflictRva = 0x0ba4a0;
constexpr std::uintptr_t kResearchStationConstructionMatrixRva = 0x0babd0;
constexpr std::uintptr_t kEvolverSwapObjectsRva = 0x0b0e10;
constexpr std::uintptr_t kEvolverConstructionMatrixRva = 0x0b1150;
constexpr std::uintptr_t kEvolverStartConstructionEffectRva = 0x0b04f0;
constexpr std::uintptr_t kEvolverDoRemoveConstructionEffectRva = 0x0b0770;
constexpr std::uintptr_t kEvolverCancelConstructionEffectRva = 0x0b08d0;
constexpr std::uintptr_t kEvolverStopConstructionEffectRva = 0x0b0970;
constexpr std::uintptr_t kEvolverUpdateConstructionEffectRva = 0x0b0a10;
constexpr std::uintptr_t kEvolverRenderInternalRva = 0x0b1170;
constexpr std::uintptr_t kDebriefingDestroyShipRva = 0x1f17b0;
constexpr std::uintptr_t kDebriefingDataGlobalRva = 0x3a86b4;
constexpr std::uintptr_t kControlButtonPressRva = 0x0e69e0;
constexpr std::uintptr_t kModeInfoBuildButtonNameRva = 0x0e7950;
constexpr std::uintptr_t kRaceIconRenderRva = 0x0ee530;
constexpr std::uintptr_t kShipDisplaySingleObjectDisplayCallRva = 0x0f2c49;
constexpr std::uintptr_t kShipDisplaySingleObjectSimulateCallRva = 0x0f29e4;

// FleetOpsHook.map offsets plus the PE .text RVA.
constexpr std::uintptr_t kFoProducerStartRva = 0x1222c0;
constexpr std::uintptr_t kFoProducerCancelRva = 0x122514;
constexpr std::uintptr_t kFoProducerFinishRva = 0x12255c;
constexpr std::uintptr_t kFoProducerPopCheckedRva = 0x122b04;
constexpr std::uintptr_t kFoResearchStationFinishRva = 0x1e9b3c;
constexpr std::uintptr_t kFoPopupUpdateButtonsRva = 0x1e6c70;
constexpr std::uintptr_t kFoControlButtonStateModeInfoRva = 0x1e23ec;
constexpr std::uintptr_t kFoPopupBuildButtonBindCallRva = 0x1e6d97;
constexpr std::uintptr_t kFoPopupEvolveButtonBindCallRva = 0x1e6e3f;
constexpr std::uintptr_t kFoPopupAiButtonBindCallRva = 0x1e6f41;
constexpr std::uintptr_t kFoSelectionDisplayDrawProducerWireframeRva =
    0x1e7f74;
constexpr std::uintptr_t kFoSelectionDisplayDrawProducerWireframeCallRva =
    0x1e8572;
constexpr std::uintptr_t kFoCraftRenderInternalCallbackRva = 0x1dc1bc;
constexpr std::uintptr_t kFoSpriteDatabaseGlobalRva = 0x212e10;
constexpr std::uintptr_t kFoScreenDimensionGlobalRva = 0x212350;
constexpr std::uintptr_t kFoDatabaseFindElementRva = 0x1e32d4;
constexpr std::uintptr_t kFoSpriteSetColourRva = 0x1e34b4;
constexpr std::uintptr_t kFoSpriteDrawScaled2DRva = 0x1e3498;
constexpr std::uintptr_t kFoShipDisplaySingleObjectDisplayRva = 0x1ee868;
constexpr std::uintptr_t kFoShipDisplaySingleObjectSimulateRva = 0x1eeb14;
constexpr std::uintptr_t kFoObjectControlButtonPressRva = 0x10ad40;
constexpr std::uintptr_t kFoObjectControlButtonPressVtableSlotRva = 0x240c00;
// Fleet Ops expands PopupPalette's native control array to 64 entries. Root
// mode binds Build, Research, Evolve, and Trade to entry 7 because those
// capabilities are normally mutually exclusive. Entry 6 is deliberately
// unused by root mode. Entry 8 sits between the production group and the
// entry-9 AI control, but is natively owned by capability 0x2. Prefer entry 8
// only when that capability is absent; entry 10 remains the collision-free
// Evolve fallback. For hybrid roots Construction takes entry 9 and the AI
// binding moves to entry 11, keeping every production category together while
// preserving the ordinary Fleet Ops order for non-hybrid selections.
constexpr std::uintptr_t kFoPopupSpareRootButtonPointerRva = 0x247f0c;
constexpr std::uintptr_t kFoPopupPreferredEvolveButtonPointerRva = 0x247f14;
constexpr std::uintptr_t kFoPopupFallbackEvolveButtonPointerRva = 0x247f1c;
constexpr std::uintptr_t kFoPopupConstructionButtonPointerRva = 0x247f18;
constexpr std::uintptr_t kFoPopupHybridAiButtonPointerRva = 0x247f20;
constexpr std::uintptr_t kFoPopupButtonPointerArrayRva = 0x247ef4;

constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kObjectFlagsOffset = 0x14;
constexpr std::size_t kClassBaseNameOffset = 0x7c;
constexpr std::size_t kClassProjectIdOffset = 0x1cc;
constexpr std::size_t kClassMenuCapabilitiesOffset = 0x1d4;
constexpr std::size_t kCurrentBuildClassOffset = 0x254;
constexpr std::size_t kQueueHeadOffset = 0x270;
constexpr std::size_t kQueueCountOffset = 0x274;
constexpr std::size_t kQueueItemNextOffset = 0x08;
constexpr std::size_t kQueueItemIdOffset = 0x0c;
constexpr std::size_t kCurrentQueueIdOffset = 0x2a0;
constexpr std::size_t kNextQueueIdOffset = 0x2a8;
constexpr std::size_t kProducerConstructionEffectOffset = 0x268;
constexpr std::size_t kProducerBuildItemsOffset = 0x450;
constexpr std::size_t kProducerButtonListOffset = 0x130;
constexpr std::size_t kButtonListEnabledMaskOffset = 0x08;
constexpr std::size_t kControlButtonStateOffset = 0x34;
constexpr std::size_t kControlButtonModeInfoOffset = 0x84;
constexpr std::size_t kModeInfoSize = 0x18;
constexpr std::size_t kModeInfoTypeOffset = 0x04;
constexpr std::size_t kModeInfoTargetClassOffset = 0x0c;
constexpr std::size_t kModeInfoActionIndexOffset = 0x14;
constexpr std::size_t kUpdateBuildButtonsVtableOffset = 0xe8;
constexpr std::size_t kPopupCurrentMenuOffset = 0x124;
constexpr std::size_t kShipDisplaySelectedObjectOffset = 0x1e8;
constexpr std::size_t kShipDisplayBuildQueueOffset = 0x120;
constexpr std::size_t kBuildWireframeObjectOffset = 0x28;
constexpr std::size_t kBuildWireframeTargetClassOffset = 0x3c;
constexpr std::size_t kRaceIconObjectOffset = 0x28;
constexpr std::size_t kResearchPodClassFlagOffset = 0x450;
constexpr std::size_t kResearchPodLevelOffset = 0x454;
constexpr std::size_t kResearchPodFamilyOffset = 0x458;
constexpr std::size_t kEvolverTailOffset = 0x2ac;
constexpr std::size_t kEvolverTailSize = 0x1c;
constexpr std::size_t kEvolverTailOpacityOffset = 0x04;
constexpr std::size_t kEvolverTailBuildStartOffset = 0x10;
constexpr std::size_t kConstructionMatrixPositionOffset = 0x24;
constexpr std::size_t kConstructionRigTailOffset = 0x2a4;
constexpr std::size_t kConstructionRigTailSize = 0x14;
constexpr std::size_t kConstructionRigInterfaceTailOffset = 0x00;
constexpr std::size_t kConstructionRigSoundHandleTailOffset = 0x08;
constexpr std::size_t kConstructionRigSoundTimerTailOffset = 0x0c;
constexpr std::size_t kConstructionRigObjectIdTailOffset = 0x10;
constexpr std::size_t kBuildPositionInterfaceSize = 0x3c;
constexpr std::size_t kBuildPositionInterfaceMatrixOffset = 0x04;
constexpr std::size_t kBuildPositionInterfaceMatrixSize = 0x30;
constexpr std::size_t kBuildCommandTargetClassOffset = 0x20;
constexpr std::size_t kGetActionVtableOffset = 0x90;
constexpr std::size_t kConstructionRigIsBusyVtableOffset = 0x138;
constexpr std::size_t kConstructionRigMatrixVtableOffset = 0x188;
constexpr std::size_t kClassHasGeometryVtableOffset = 0x28;

constexpr std::size_t kRuntimeBuildListCapacity = 57;
constexpr std::size_t kNativeResearchButtonCount = 14;
constexpr std::size_t kFoPopupButtonCount = 64;
constexpr std::uint32_t kNativeQueueCapacity = 10;
constexpr unsigned kRetainResearchMenuRefreshLimit = 60;
constexpr std::uint32_t kRootMenu = 0;
constexpr std::uint32_t kBuildMenu = 2;
constexpr std::uint32_t kResearchMenu = 3;
constexpr std::uint32_t kEvolveMenu = 4;
// PopupPalette treats either of these bits as permission to show its Build
// button. 0x80 is the Producer/yard capability; 0x40 is the placement-based
// constructor capability and must not be added to a ResearchStation.
constexpr std::uint32_t kYardMenuCapability = 0x80;
constexpr std::uint32_t kResearchMenuCapability = 0x2000;
constexpr std::uint32_t kEvolveMenuCapability = 0x80000;
// ModeInfo type 3 stores an index into Armada's sixteen-byte root-action
// metadata table. The third pointer is the sprite stem that the native name
// builder prefixes with "b_". A copied Build ModeInfo can therefore retain all
// native Build behavior while resolving the hybrid Construction sprite as
// b_construct during its own name-build call.
constexpr std::uintptr_t kButtonActionMetadataRva = 0x309f60;
constexpr std::size_t kButtonActionMetadataRecordSize = 0x10;
constexpr std::size_t kButtonActionSpriteStemOffset = 0x08;
constexpr std::uint32_t kMaximumButtonActionIndex = 15;
constexpr const char* kConstructButtonSpriteStem = "construct";
// BuildWireframe selects a single `_s` sprite when this object-kind bit is
// present on its owner. Research pods publish that queue sprite, while yard
// craft publish the ordinary `w1` through `w5` wireframe layers. The selected
// hybrid station owns both kinds, so the bit is applied only around rendering
// a non-yard queue entry and restored immediately afterwards.
constexpr std::uint32_t kSingleSpriteWireframeFlag = 0x1000;

const std::uint8_t kExpectedParameterDbGetProjectId[] =
    {0x55, 0x8b, 0xec, 0x81, 0xec, 0x40, 0x01, 0x00, 0x00};
const std::uint8_t kExpectedGameObjectClassFindProjectId[] =
    {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68};
const std::uint8_t kExpectedProducerGetAction[] =
    {0x55, 0x8b, 0xec, 0x56, 0x57};
const std::uint8_t kExpectedConstructionRigGetAction[] =
    {0x55, 0x8b, 0xec, 0x56, 0x57, 0x8b, 0xf9};
const std::uint8_t kExpectedConstructionRigStart[] =
    {0x55, 0x8b, 0xec, 0x83, 0xec, 0x30, 0x56};
const std::uint8_t kExpectedConstructionRigStartHardpointCall[] =
    {0xe8, 0x73, 0x5c, 0xfc, 0xff};
const std::uint8_t kExpectedConstructionRigCancel[] =
    {0x56, 0x8b, 0xf1, 0x57, 0xb9, 0x48, 0x6e, 0x73, 0x00};
const std::uint8_t kExpectedConstructionRigFinish[] =
    {0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x57};
const std::uint8_t kExpectedConstructionRigRemoveObject[] =
    {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x87, 0xb4, 0x02, 0x00, 0x00};
const std::uint8_t kExpectedConstructionRigConstructionMatrix[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x56};
const std::uint8_t kExpectedBuildPositionInterfaceConstructor[] =
    {0x55, 0x8b, 0xec, 0x8b, 0xc1, 0x56};
const std::uint8_t kExpectedBuildPositionInterfaceDestructor[] =
    {0x56, 0x8b, 0x71, 0x34, 0x85, 0xf6};
const std::uint8_t kExpectedPlaceholderRenderInternal[] =
    {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68};
const std::uint8_t kExpectedProducerIsBusy[] =
    {0x8b, 0x81, 0x54, 0x02, 0x00, 0x00, 0x85, 0xc0};
const std::uint8_t kExpectedBuildCommandCleanupIsBusyQuery[] =
    {0x8b, 0x73, 0x30, 0x8b, 0xce, 0x8b, 0x06,
     0xff, 0x90, 0x38, 0x01, 0x00, 0x00, 0x84, 0xc0};
const std::uint8_t kExpectedBuildCommandAdmitIsBusyQuery[] =
    {0x8b, 0x06, 0x8b, 0xce, 0xff, 0x90, 0x38,
     0x01, 0x00, 0x00, 0x8b, 0x7d, 0x08, 0x84, 0xc0};
const std::uint8_t kExpectedBuildCommandCleanupPopQuery[] =
    {0x8b, 0xce, 0xe8};
const std::uint8_t kExpectedBuildCommandReplacePopQuery[] =
    {0x8b, 0xce, 0xe8};
const std::uint8_t kExpectedBuildCommandPositionInterfaceLoad[] =
    {0x8b, 0x8e, 0xa4, 0x02, 0x00, 0x00};
const std::uint8_t kExpectedProducerConstructionMatrix[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x81, 0x58, 0x02, 0x00, 0x00};
const std::uint8_t kExpectedProducerStartConstructionEffect[] =
    {0x55, 0x8b, 0xec, 0x83, 0xec, 0x30, 0x56};
const std::uint8_t kExpectedProducerCancelConstructionEffect[] =
    {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0x68, 0x02, 0x00, 0x00};
const std::uint8_t kExpectedProducerUpdateConstructionEffect[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x81, 0x68, 0x02, 0x00, 0x00};
const std::uint8_t kExpectedProducerStopConstructionEffect[] =
    {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0x68, 0x02, 0x00, 0x00};
const std::uint8_t kExpectedResearchStationStart[] =
    {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x56};
const std::uint8_t kExpectedResearchStationCancel[] =
    {0x55, 0x8b, 0xec, 0x51, 0x56, 0x57};
const std::uint8_t kExpectedResearchStationCanBuild[] =
    {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0x54, 0x02, 0x00, 0x00};
const std::uint8_t kExpectedResearchStationItemConflict[] =
    {0x55, 0x8b, 0xec, 0x53, 0x56, 0x57};
const std::uint8_t kExpectedResearchStationConstructionMatrix[] =
    {0x55, 0x8b, 0xec, 0x51, 0x56, 0x57};
const std::uint8_t kExpectedEvolverSwapObjects[] =
    {0x55, 0x8b, 0xec, 0x53, 0x56, 0x57};
const std::uint8_t kExpectedEvolverConstructionMatrix[] =
    {0x55, 0x8b, 0xec, 0x56, 0x57};
const std::uint8_t kExpectedEvolverStartConstructionEffect[] =
    {0x55, 0x8b, 0xec, 0x6a, 0xff};
const std::uint8_t kExpectedEvolverDoRemoveConstructionEffect[] =
    {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0xac, 0x02, 0x00, 0x00};
const std::uint8_t kExpectedEvolverCancelConstructionEffect[] =
    {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0xac, 0x02, 0x00, 0x00};
const std::uint8_t kExpectedEvolverStopConstructionEffect[] =
    {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0xac, 0x02, 0x00, 0x00};
const std::uint8_t kExpectedEvolverUpdateConstructionEffect[] =
    {0x55, 0x8b, 0xec, 0x81, 0xec, 0xac, 0x00, 0x00, 0x00};
const std::uint8_t kExpectedEvolverRenderInternal[] =
    {0x55, 0x8b, 0xec, 0x53, 0x56};
const std::uint8_t kExpectedFoCraftRenderInternalCallback[] =
    {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf4, 0x53};
// Complete first three instructions: mov eax,ecx; push esi;
// mov edx,[eax+0x88]. The nine-byte boundary is deliberate so the gateway
// cannot begin in the middle of the final memory-load instruction.
const std::uint8_t kExpectedControlButtonPress[] =
    {0x8b, 0xc1, 0x56, 0x8b, 0x90, 0x88, 0x00, 0x00, 0x00};
const std::uint8_t kExpectedModeInfoBuildButtonName[] =
    {0x55, 0x8b, 0xec, 0x83, 0xec, 0x48, 0x53};
// Complete the first two instructions: push ebp; mov ebp,esp; push -1.
const std::uint8_t kExpectedRaceIconRender[] =
    {0x55, 0x8b, 0xec, 0x6a, 0xff};
// Fleet Ops has already redirected Armada's ordinary single-object dispatcher
// calls by the time native feature modules initialize. Hybrid ResearchStations
// remain on this natural display path; the wrappers add the queue post-pass
// without entering Fleet Ops' incompatible single-builder callback.
const std::uint8_t kExpectedShipDisplaySingleObjectDisplayCall[] =
    {0xe8, 0x1a, 0xbc, 0x4f, 0x5a};
const std::uint8_t kExpectedShipDisplaySingleObjectSimulateCall[] =
    {0xe8, 0x2b, 0xc1, 0x4f, 0x5a};
const std::uint8_t kExpectedFoProducerStart[] =
    {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xec, 0x53};
const std::uint8_t kExpectedFoProducerPopChecked[] =
    {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xe0};
const std::uint8_t kExpectedFoResearchStationFinish[] =
    {0x55, 0x8b, 0xec, 0x51, 0x89, 0x4d, 0xfc};
const std::uint8_t kExpectedFoPopupUpdateButtons[] =
    {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xe4, 0x53};
const std::uint8_t kExpectedFoPopupBuildButtonBindCall[] =
    {0xe8, 0x50, 0xb6, 0xff, 0xff};
const std::uint8_t kExpectedFoPopupEvolveButtonBindCall[] =
    {0xe8, 0xa8, 0xb5, 0xff, 0xff};
const std::uint8_t kExpectedFoPopupAiButtonBindCall[] =
    {0xe8, 0xa6, 0xb4, 0xff, 0xff};
const std::uint8_t kExpectedFoDrawProducerWireframeCall[] =
    {0xe8, 0xfd, 0xf9, 0xff, 0xff};

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
extern "C" void* a2fo_fo_database_find_element(
    void* function, void* database, const char* name);
extern "C" void a2fo_fo_sprite_set_colour(
    void* function, void* sprite, const void* colour);
extern "C" void a2fo_fo_sprite_draw_scaled_2d(
    void* function, void* sprite, const void* position,
    float display_width, float display_height);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
CRITICAL_SECTION g_registry_lock;
bool g_registry_lock_ready = false;
A2FO_InlineHook g_research_start_hook{};
A2FO_InlineHook g_research_cancel_hook{};
A2FO_InlineHook g_research_can_build_hook{};
A2FO_InlineHook g_research_item_conflict_hook{};
A2FO_InlineHook g_research_matrix_hook{};
A2FO_InlineHook g_research_finish_hook{};
A2FO_InlineHook g_producer_get_action_hook{};
A2FO_InlineHook g_build_position_interface_load_hook{};
A2FO_InlineHook g_producer_start_effect_hook{};
A2FO_InlineHook g_producer_cancel_effect_hook{};
A2FO_InlineHook g_producer_update_effect_hook{};
A2FO_InlineHook g_producer_stop_effect_hook{};
A2FO_InlineHook g_craft_render_internal_hook{};
A2FO_InlineHook g_control_button_press_hook{};
A2FO_InlineHook g_mode_info_build_button_name_hook{};
A2FO_InlineHook g_race_icon_render_hook{};
A2FO_InlineHook g_producer_is_busy_hook{};
A2FO_InlineHook g_fo_producer_pop_checked_hook{};
A2FO_InlineHook g_popup_update_buttons_hook{};
std::unordered_map<void*, std::unique_ptr<HybridBuildLists>> g_class_lists;

struct RuntimeClassLists {
    void** legacy_research = nullptr;
    std::array<void*, kRuntimeBuildListCapacity> construct{};
    std::array<void*, kRuntimeBuildListCapacity> yard{};
    std::array<void*, kRuntimeBuildListCapacity> research{};
    std::array<void*, kRuntimeBuildListCapacity> evolve{};
    bool has_construct = false;
    bool has_yard = false;
    bool has_explicit_research = false;
    bool has_evolve = false;
};

std::unordered_map<void*, std::unique_ptr<RuntimeClassLists>>
    g_runtime_class_lists;

struct HybridCocoonState {
    std::array<std::uint8_t, kEvolverTailSize> tail{};

    HybridCocoonState() noexcept {
        const float opaque = 1.0f;
        std::memcpy(tail.data() + kEvolverTailOpacityOffset,
                    &opaque, sizeof(opaque));
    }
};

std::unordered_map<void*, HybridCocoonState> g_hybrid_cocoons;
volatile LONG g_hybrid_cocoon_state_count = 0;

struct HybridConstructionState {
    std::array<std::uint8_t, kConstructionRigTailSize> tail{};
    void* cursor_interface = nullptr;
    void* pending_interface = nullptr;
    void* pending_target_class = nullptr;
    std::uint32_t pending_queue_id = 0;

    struct Placement {
        std::uint32_t queue_id = 0;
        void* target_class = nullptr;
        void* build_interface = nullptr;
    };
    std::array<Placement, kNativeQueueCapacity> placements{};
};

std::unordered_map<void*, HybridConstructionState> g_hybrid_construction;
std::unordered_map<void**, void*> g_hybrid_get_action_originals;
volatile LONG g_hybrid_construction_state_count = 0;
// The game UI is single-threaded. This is non-null only across a direct
// Producer/ResearchStation UpdateBuildButtons call made by the popup adapter.
// Both hybrid category refreshes must see queue capacity rather than the
// ResearchStation's normal one-active-job busy result.
void* g_queue_enabled_button_station = nullptr;
bool g_logged_shared_queue_ui = false;
bool g_logged_shared_queue_command = false;
bool g_logged_shared_queue_construct_action = false;
bool g_logged_preserved_command_queue = false;
bool g_logged_queued_research_conflict = false;
bool g_logged_queued_research_button_disabled = false;
bool g_logged_research_menu_retention_armed = false;
bool g_logged_retained_research_menu = false;
bool g_logged_hybrid_yard_start = false;
bool g_logged_hybrid_research_start = false;
bool g_logged_hybrid_evolve_start = false;
bool g_logged_hybrid_evolve_finish = false;
bool g_logged_hybrid_cocoon_start = false;
bool g_logged_hybrid_cocoon_api_missing = false;
bool g_logged_hybrid_queue_slot_binding = false;
bool g_logged_hybrid_queue_wireframes = false;
bool g_logged_hybrid_research_queue_sprite = false;
bool g_logged_hybrid_full_queue_icon_suppressed = false;
bool g_logged_hybrid_research_hover_wireframe = false;
bool g_logged_hybridbuild_identity = false;
bool g_logged_missing_classlabel_identity_api = false;
bool g_logged_hybrid_construct_action = false;
bool g_logged_hybrid_construct_sidecar = false;
bool g_logged_hybrid_construct_placement_bound = false;
bool g_logged_hybrid_construct_placement_selected = false;
bool g_logged_hybrid_construct_queued_ghosts = false;
bool g_hybrid_construct_ghost_renderer_ready = false;
bool g_logged_hybrid_construct_start = false;
bool g_logged_hybrid_construct_finish = false;
bool g_logged_hybrid_construct_hardpoint_bypass = false;
bool g_logged_hybrid_get_action_vtable = false;
bool g_logged_hybrid_construct_root_press = false;
bool g_logged_hybrid_construct_placement_press = false;
bool g_logged_hybrid_construct_premature_order = false;
bool g_logged_hybrid_root_button_rebase = false;
bool g_logged_hybrid_construct_root_icon = false;
// These flags span only one synchronous PopupPalette refresh on the game's UI
// thread. They let the patched Build and Evolve bindings select independent
// physical controls without changing ordinary native behavior globally.
bool g_split_hybrid_root_buttons = false;
void* g_hybrid_yard_root_button = nullptr;
void* g_hybrid_construct_root_button = nullptr;
void* g_hybrid_evolve_root_button = nullptr;
// Unlike the three pointers above, these survive the scoped popup refresh.
// Fleet Ops may update its pointer table again before the deferred press is
// dispatched, so click classification must use the exact objects that were
// actually bound during the last hybrid root refresh.
void* g_last_hybrid_yard_root_button = nullptr;
void* g_last_hybrid_construct_root_button = nullptr;
// Fleet Ops compacts valid palette controls after binding them by copying the
// bound modes into the first visible ControlButtons. Remember the Build mode
// across that compaction so the two final controls can be recovered in their
// displayed order: Yard first, Construction second.
void* g_hybrid_root_build_mode_info = nullptr;
alignas(void*) std::array<std::uint8_t, kModeInfoSize>
    g_hybrid_construct_mode_info{};

enum class HybridBuildPalette : std::uint8_t {
    yard,
    construct,
};

HybridBuildPalette g_hybrid_build_palette = HybridBuildPalette::yard;
bool g_hybrid_build_palette_press_armed = false;
void* g_last_single_hybrid_selection = nullptr;
void* g_retain_research_menu_station = nullptr;
unsigned g_retain_research_menu_refreshes = 0;
bool g_retain_research_menu_saw_root = false;
unsigned g_control_button_press_depth = 0;
// Non-null only while PopupPalette::CheckCanExecute handles the constructItem
// button which is supposed to arm placement. A confirmed map click reaches
// the synchronized order path after this scope has ended.
void* g_hybrid_construct_placement_press_station = nullptr;
void* g_post_press_research_menu_station = nullptr;
void* g_object_control_button_press_target = nullptr;
void* g_last_popup = nullptr;
void* g_last_popup_craft_array = nullptr;
std::uintptr_t g_last_popup_argument2 = 0;
std::uintptr_t g_last_popup_argument3 = 0;
thread_local unsigned g_craft_render_internal_depth = 0;

using FindClassByProjectIdFunction = void* (__cdecl*)(
    const std::uint32_t* project_id);
using ControlButtonStateModeInfoFunction =
    std::uintptr_t (__attribute__((regparm(3))) *)(
        void* button, void* mode_info, std::uintptr_t enabled);
using DelphiRegister2Function =
    std::uintptr_t (__attribute__((regparm(2))) *)(void*, void*);
using DelphiRegister2Stack2Function =
    void (__attribute__((regparm(2), stdcall)) *)(
        void*, void*, std::uintptr_t, void*);

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) {
    return reinterpret_cast<T*>(
        reinterpret_cast<std::uint8_t*>(module) + rva);
}

void log_message(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

std::uint8_t* bytes(void* value) noexcept {
    return static_cast<std::uint8_t*>(value);
}

bool readable_range(const void* pointer, std::size_t size) noexcept {
    return pointer && size != 0 && !IsBadReadPtr(pointer, size);
}

bool writable_range(void* pointer, std::size_t size) noexcept {
    return pointer && size != 0 && !IsBadWritePtr(pointer, size);
}

bool executable_address(const void* pointer) noexcept {
    if (!pointer) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
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

bool class_project_id(void* object_class,
                      std::uint32_t& project_id) noexcept {
    project_id = 0;
    if (!object_class || !readable_range(
            bytes(object_class) + kClassProjectIdOffset, sizeof(void*))) {
        return false;
    }
    const auto* id = *reinterpret_cast<const std::uint32_t* const*>(
        bytes(object_class) + kClassProjectIdOffset);
    if (!readable_range(id, sizeof(*id))) return false;
    project_id = *id;
    return project_id != 0;
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) {
    return module && std::memcmp(at(module, rva), expected, Size) == 0;
}

template <std::size_t Size>
bool require_signature(const char* name, HMODULE module,
                       std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) {
    if (signature_matches(module, rva, expected)) return true;
    log_message(std::string("Hybrid production signature mismatch: ") +
                name);
    return false;
}

bool read_parameter_string(void* parameter_db, const std::string& key,
                           std::string& value) {
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

bool is_hybridbuild_parameter_db(void* parameter_db) noexcept {
    if (!parameter_db || !g_api ||
        !A2FO_MODULE_API_HAS(g_api, get_original_classlabel) ||
        (g_api->capabilities & A2FO_CAP_ORIGINAL_CLASSLABEL) == 0 ||
        !g_api->get_original_classlabel) {
        if (!g_logged_missing_classlabel_identity_api) {
            g_logged_missing_classlabel_identity_api = true;
            log_message("HybridBuild identity API unavailable; explicit "
                        "hybrid menus remain disabled on this core");
        }
        return false;
    }

    std::array<char, 64> classlabel{};
    if (!g_api->get_original_classlabel(
            parameter_db, classlabel.data(),
            static_cast<std::uint32_t>(classlabel.size()))) {
        return false;
    }
    const bool hybridbuild = _stricmp(
        classlabel.data(), "hybridbuild") == 0;
    if (hybridbuild && !g_logged_hybridbuild_identity) {
        g_logged_hybridbuild_identity = true;
        log_message("HybridBuild source identity preserved through the "
                    "research classlabel alias");
    }
    return hybridbuild;
}

bool has_explicit_list_marker(void* parameter_db) {
    // ODF build lists conventionally begin at zero. This four-lookup gate
    // avoids another 228 ParameterDB searches on every ordinary Producer
    // class while retaining sparse entries after the first item.
    constexpr ProductionMethod methods[] = {
        ProductionMethod::construct,
        ProductionMethod::yard,
        ProductionMethod::research,
        ProductionMethod::evolve,
    };
    for (const ProductionMethod method : methods) {
        std::string ignored;
        if (read_parameter_string(parameter_db,
                                  production_command(method, 0), ignored)) {
            return true;
        }
    }
    return false;
}

void load_explicit_lists(void* producer_class, void* parameter_db) {
    if (!producer_class || !parameter_db || !has_explicit_list_marker(
            parameter_db)) {
        return;
    }

    constexpr ProductionMethod methods[] = {
        ProductionMethod::construct,
        ProductionMethod::yard,
        ProductionMethod::research,
        ProductionMethod::evolve,
    };
    auto lists = std::make_unique<HybridBuildLists>();
    std::array<std::size_t, 4> counts{};
    for (std::size_t method_index = 0; method_index < counts.size();
         ++method_index) {
        const ProductionMethod method = methods[method_index];
        for (std::size_t index = 0; index < kHybridBuildListCapacity;
             ++index) {
            const std::string key = production_command(method, index);
            std::string declared_target;
            if (!read_parameter_string(parameter_db, key, declared_target)) {
                continue;
            }
            if (declared_target.empty()) {
                log_message(key + " ignored: target ODF is empty");
                continue;
            }
            std::uint32_t project_id = 0;
            if (!read_parameter_project_id(parameter_db, key, project_id) ||
                project_id == 0) {
                log_message(key + " ignored: " + declared_target +
                            " could not be assigned a project ID");
                continue;
            }
            const AddListEntryResult added = lists->add(
                method, index, project_id);
            if (added == AddListEntryResult::added) {
                ++counts[method_index];
            } else if (added == AddListEntryResult::ambiguous_project) {
                log_message(key + " ignored: " + declared_target +
                            " already belongs to another explicit method");
            } else {
                log_message(key + " ignored: explicit list entry is invalid");
            }
        }
    }
    if (lists->empty()) {
        log_message("Hybrid production commands were present but no valid "
                    "targets were registered");
        return;
    }

    const std::size_t total = lists->entries().size();
    EnterCriticalSection(&g_registry_lock);
    try {
        g_class_lists[producer_class] = std::move(lists);
        LeaveCriticalSection(&g_registry_lock);
    } catch (...) {
        LeaveCriticalSection(&g_registry_lock);
        throw;
    }

    char message[256];
    std::snprintf(
        message, sizeof(message),
        "Hybrid production ODF lists registered: %lu total "
        "(construct %lu, yard %lu, research %lu, evolve %lu); runtime "
        "HybridBuild ResearchStation four-method adapter available",
        static_cast<unsigned long>(total),
        static_cast<unsigned long>(counts[0]),
        static_cast<unsigned long>(counts[1]),
        static_cast<unsigned long>(counts[2]),
        static_cast<unsigned long>(counts[3]));
    log_message(message);
}

bool active_explicit_method(void* producer,
                            ProductionMethod& method) noexcept {
    if (!producer || !readable_range(
            bytes(producer) + kQueueHeadOffset, sizeof(void*))) {
        return false;
    }
    void* producer_class = *reinterpret_cast<void**>(
        bytes(producer) + kObjectClassOffset);
    if (!producer_class) return false;

    void* target_class = *reinterpret_cast<void**>(
        bytes(producer) + kCurrentBuildClassOffset);
    if (!target_class) {
        void* queue_head = *reinterpret_cast<void**>(
            bytes(producer) + kQueueHeadOffset);
        if (readable_range(queue_head, sizeof(void*))) {
            target_class = *reinterpret_cast<void**>(queue_head);
        }
    }
    std::uint32_t target_project_id = 0;
    return class_project_id(target_class, target_project_id) &&
        resolve_explicit_production_method(
            producer_class, target_project_id, method);
}

bool active_yard_job(void* producer) noexcept {
    ProductionMethod method = ProductionMethod::legacy;
    return active_explicit_method(producer, method) &&
        method == ProductionMethod::yard;
}

bool active_construct_job(void* producer) noexcept {
    ProductionMethod method = ProductionMethod::legacy;
    return active_explicit_method(producer, method) &&
        method == ProductionMethod::construct;
}

bool active_evolve_job(void* producer) noexcept {
    ProductionMethod method = ProductionMethod::legacy;
    return active_explicit_method(producer, method) &&
        method == ProductionMethod::evolve;
}

class HybridCocoonTailSwap {
public:
    explicit HybridCocoonTailSwap(void* station) noexcept
        : station_(station) {
        if (!station_ || !g_registry_lock_ready || !writable_range(
                bytes(station_) + kEvolverTailOffset, kEvolverTailSize)) {
            return;
        }
        EnterCriticalSection(&g_registry_lock);
        locked_ = true;
        const auto found = g_hybrid_cocoons.find(station_);
        if (found == g_hybrid_cocoons.end()) return;
        state_ = &found->second;
        std::memcpy(saved_station_tail_.data(),
                    bytes(station_) + kEvolverTailOffset,
                    saved_station_tail_.size());
        std::memcpy(bytes(station_) + kEvolverTailOffset,
                    state_->tail.data(), state_->tail.size());
        active_ = true;
    }

    ~HybridCocoonTailSwap() {
        if (active_) {
            std::memcpy(state_->tail.data(),
                        bytes(station_) + kEvolverTailOffset,
                        state_->tail.size());
            std::memcpy(bytes(station_) + kEvolverTailOffset,
                        saved_station_tail_.data(),
                        saved_station_tail_.size());
        }
        if (locked_) LeaveCriticalSection(&g_registry_lock);
    }

    bool active() const noexcept { return active_; }

    HybridCocoonTailSwap(const HybridCocoonTailSwap&) = delete;
    HybridCocoonTailSwap& operator=(const HybridCocoonTailSwap&) = delete;

private:
    void* station_ = nullptr;
    HybridCocoonState* state_ = nullptr;
    std::array<std::uint8_t, kEvolverTailSize> saved_station_tail_{};
    bool locked_ = false;
    bool active_ = false;
};

bool hybrid_cocoon_has_instance(void* station) noexcept {
    if (!station || !g_registry_lock_ready ||
        InterlockedCompareExchange(
            &g_hybrid_cocoon_state_count, 0, 0) == 0) {
        return false;
    }
    bool present = false;
    EnterCriticalSection(&g_registry_lock);
    const auto found = g_hybrid_cocoons.find(station);
    if (found != g_hybrid_cocoons.end()) {
        std::uint32_t instance = 0;
        std::memcpy(&instance, found->second.tail.data(), sizeof(instance));
        present = instance != 0;
    }
    LeaveCriticalSection(&g_registry_lock);
    return present;
}

void erase_hybrid_cocoon_state(void* station) noexcept {
    if (!station || !g_registry_lock_ready) return;
    EnterCriticalSection(&g_registry_lock);
    if (g_hybrid_cocoons.erase(station) != 0) {
        InterlockedDecrement(&g_hybrid_cocoon_state_count);
    }
    LeaveCriticalSection(&g_registry_lock);
}

void remove_hybrid_cocoon_effect(void* station, bool erase) noexcept {
    if (!station || !g_armada) return;
    {
        HybridCocoonTailSwap swapped(station);
        if (swapped.active()) {
            a2fo_call_thiscall_0(
                at(g_armada, kEvolverDoRemoveConstructionEffectRva),
                station);
        }
    }
    if (erase) erase_hybrid_cocoon_state(station);
}

bool initialize_hybrid_cocoon_state(void* station) noexcept {
    if (!station || !g_armada || !g_registry_lock_ready) return false;
    remove_hybrid_cocoon_effect(station, true);

    HybridCocoonState state;
    std::array<std::uint8_t, 0x30> matrix{};
    a2fo_call_thiscall_1(
        at(g_armada, kEvolverConstructionMatrixRva), station,
        reinterpret_cast<std::uintptr_t>(matrix.data()));
    std::memcpy(state.tail.data() + kEvolverTailBuildStartOffset,
                matrix.data() + kConstructionMatrixPositionOffset,
                3 * sizeof(float));
    try {
        EnterCriticalSection(&g_registry_lock);
        const auto inserted = g_hybrid_cocoons.insert_or_assign(
            station, state);
        if (inserted.second) {
            InterlockedIncrement(&g_hybrid_cocoon_state_count);
        }
        LeaveCriticalSection(&g_registry_lock);
        return true;
    } catch (...) {
        LeaveCriticalSection(&g_registry_lock);
        log_message("Hybrid cocoon sidecar allocation failed; evolution "
                    "continues without a cocoon");
        return false;
    }
}

bool target_has_explicit_method(void* producer, void* target_class,
                                ProductionMethod wanted) noexcept;

using GameAllocateFunction = void* (__cdecl*)(std::size_t);
using GameFreeFunction = void (__cdecl*)(void*);

void destroy_build_position_interface(void* build_interface) noexcept {
    if (!build_interface || !g_armada) return;
    a2fo_call_thiscall_0(
        at(g_armada, kBuildPositionInterfaceDestructorRva),
        build_interface);
    reinterpret_cast<GameFreeFunction>(
        at(g_armada, kGameOperatorDeleteRva))(build_interface);
}

void* allocate_build_position_interface(
    void* station, void* clone_matrix_from = nullptr) noexcept {
    if (!station || !g_armada) return nullptr;
    void* build_interface = reinterpret_cast<GameAllocateFunction>(
        at(g_armada, kGameOperatorNewRva))(kBuildPositionInterfaceSize);
    if (!build_interface) return nullptr;
    void* const allocation = build_interface;
    build_interface = reinterpret_cast<void*>(a2fo_call_thiscall_1(
        at(g_armada, kBuildPositionInterfaceConstructorRva),
        build_interface, reinterpret_cast<std::uintptr_t>(station)));
    if (!build_interface) {
        reinterpret_cast<GameFreeFunction>(
            at(g_armada, kGameOperatorDeleteRva))(allocation);
        return nullptr;
    }
    if (clone_matrix_from && readable_range(
            bytes(clone_matrix_from) +
                kBuildPositionInterfaceMatrixOffset,
            kBuildPositionInterfaceMatrixSize) &&
        writable_range(
            bytes(build_interface) +
                kBuildPositionInterfaceMatrixOffset,
            kBuildPositionInterfaceMatrixSize)) {
        // Rotation can be adjusted while the placement cursor is active.
        // Seed the command-owned interface from that cursor transform before
        // native SetRealBuildPositionAndCookie writes its final position.
        std::memcpy(
            bytes(build_interface) + kBuildPositionInterfaceMatrixOffset,
            bytes(clone_matrix_from) + kBuildPositionInterfaceMatrixOffset,
            kBuildPositionInterfaceMatrixSize);
    }
    return build_interface;
}

bool initialize_hybrid_construction_state(void* station) noexcept {
    if (!station || !g_armada || !g_registry_lock_ready) return false;

    EnterCriticalSection(&g_registry_lock);
    const bool existing = g_hybrid_construction.find(station) !=
        g_hybrid_construction.end();
    LeaveCriticalSection(&g_registry_lock);
    if (existing) return true;

    void* build_interface = allocate_build_position_interface(station);
    if (!build_interface) return false;

    HybridConstructionState state;
    state.cursor_interface = build_interface;
    std::memcpy(
        state.tail.data() + kConstructionRigInterfaceTailOffset,
        &build_interface, sizeof(build_interface));
    const std::int32_t no_sound = -1;
    std::memcpy(
        state.tail.data() + kConstructionRigSoundHandleTailOffset,
        &no_sound, sizeof(no_sound));
    const std::uint32_t no_timer = 0;
    std::memcpy(
        state.tail.data() + kConstructionRigSoundTimerTailOffset,
        &no_timer, sizeof(no_timer));
    std::uint32_t null_object_id = 0;
    const auto* null_id = at<const std::uint32_t>(
        g_armada, kNullObjectIdGlobalRva);
    if (readable_range(null_id, sizeof(*null_id))) {
        null_object_id = *null_id;
    }
    std::memcpy(
        state.tail.data() + kConstructionRigObjectIdTailOffset,
        &null_object_id, sizeof(null_object_id));

    bool inserted = false;
    try {
        EnterCriticalSection(&g_registry_lock);
        inserted = g_hybrid_construction.emplace(
            station, std::move(state)).second;
        if (inserted) {
            InterlockedIncrement(&g_hybrid_construction_state_count);
        }
        LeaveCriticalSection(&g_registry_lock);
    } catch (...) {
        LeaveCriticalSection(&g_registry_lock);
        destroy_build_position_interface(build_interface);
        log_message("Hybrid Construction sidecar allocation failed");
        return false;
    }
    if (!inserted) {
        destroy_build_position_interface(build_interface);
    } else if (!g_logged_hybrid_construct_sidecar) {
        g_logged_hybrid_construct_sidecar = true;
        log_message("Hybrid Construction allocated a protected native "
                    "placement sidecar");
    }
    return true;
}

std::uint32_t active_construction_queue_id(void* station) noexcept {
    if (!station) return 0;
    if (readable_range(
            bytes(station) + kCurrentQueueIdOffset,
            sizeof(std::uint32_t))) {
        const std::uint32_t current =
            *reinterpret_cast<const std::uint32_t*>(
                bytes(station) + kCurrentQueueIdOffset);
        if (current) return current;
    }
    if (!readable_range(
            bytes(station) + kQueueHeadOffset, sizeof(void*))) {
        return 0;
    }
    void* queue_head = *reinterpret_cast<void**>(
        bytes(station) + kQueueHeadOffset);
    if (!queue_head) return 0;
    return readable_range(
            bytes(queue_head) + kQueueItemIdOffset,
            sizeof(std::uint32_t))
        ? *reinterpret_cast<const std::uint32_t*>(
              bytes(queue_head) + kQueueItemIdOffset)
        : 0;
}

void* placement_interface_for_queue_id_locked(
    HybridConstructionState& state, std::uint32_t queue_id) noexcept {
    if (!queue_id) return nullptr;
    for (const HybridConstructionState::Placement& placement :
         state.placements) {
        if (placement.queue_id == queue_id) {
            return placement.build_interface;
        }
    }
    return nullptr;
}

class HybridConstructionTailSwap {
public:
    explicit HybridConstructionTailSwap(
        void* station, bool select_active_placement = true) noexcept
        : station_(station) {
        if (!station_ || !g_armada || !g_registry_lock_ready ||
            !writable_range(station_, sizeof(void*)) || !writable_range(
                bytes(station_) + kConstructionRigTailOffset,
                kConstructionRigTailSize)) {
            return;
        }
        void** const construction_vtable = at<void*>(
            g_armada, kConstructionRigVtableRva);
        if (!readable_range(
                construction_vtable,
                kConstructionRigMatrixVtableOffset + sizeof(void*)) ||
            !executable_address(construction_vtable[2]) ||
            !executable_address(construction_vtable[
                kConstructionRigIsBusyVtableOffset / sizeof(void*)]) ||
            !executable_address(construction_vtable[
                kConstructionRigMatrixVtableOffset / sizeof(void*)])) {
            return;
        }
        EnterCriticalSection(&g_registry_lock);
        locked_ = true;
        const auto found = g_hybrid_construction.find(station_);
        if (found == g_hybrid_construction.end()) return;
        state_ = &found->second;
        void* selected_interface = state_->cursor_interface;
        if (select_active_placement) {
            void* queued_interface = placement_interface_for_queue_id_locked(
                *state_, active_construction_queue_id(station_));
            if (queued_interface) {
                selected_interface = queued_interface;
                if (!g_logged_hybrid_construct_placement_selected) {
                    g_logged_hybrid_construct_placement_selected = true;
                    log_message("Hybrid Construction selected the active "
                                "queue item's saved transform");
                }
            }
        }
        std::memcpy(
            state_->tail.data() + kConstructionRigInterfaceTailOffset,
            &selected_interface, sizeof(selected_interface));
        saved_station_vtable_ = *reinterpret_cast<void***>(station_);
        std::memcpy(saved_station_tail_.data(),
                    bytes(station_) + kConstructionRigTailOffset,
                    saved_station_tail_.size());
        std::memcpy(bytes(station_) + kConstructionRigTailOffset,
                    state_->tail.data(), state_->tail.size());
        *reinterpret_cast<void***>(station_) = construction_vtable;
        active_ = true;
    }

    ~HybridConstructionTailSwap() {
        if (active_) {
            std::memcpy(state_->tail.data(),
                        bytes(station_) + kConstructionRigTailOffset,
                        state_->tail.size());
            // The active queue item's interface is selected per call. Keep
            // the persistent sidecar rooted on the cursor interface so a
            // completed placement can be destroyed without leaving a stale
            // pointer in the saved ConstructionRig tail.
            std::memcpy(
                state_->tail.data() + kConstructionRigInterfaceTailOffset,
                &state_->cursor_interface,
                sizeof(state_->cursor_interface));
            std::memcpy(bytes(station_) + kConstructionRigTailOffset,
                        saved_station_tail_.data(),
                        saved_station_tail_.size());
            *reinterpret_cast<void***>(station_) = saved_station_vtable_;
        }
        if (locked_) LeaveCriticalSection(&g_registry_lock);
    }

    bool active() const noexcept { return active_; }

    HybridConstructionTailSwap(const HybridConstructionTailSwap&) = delete;
    HybridConstructionTailSwap& operator=(
        const HybridConstructionTailSwap&) = delete;

private:
    void* station_ = nullptr;
    HybridConstructionState* state_ = nullptr;
    void** saved_station_vtable_ = nullptr;
    std::array<std::uint8_t, kConstructionRigTailSize>
        saved_station_tail_{};
    bool locked_ = false;
    bool active_ = false;
};

void* hybrid_construction_interface(void* station) noexcept {
    if (!station || !g_registry_lock_ready ||
        InterlockedCompareExchange(
            &g_hybrid_construction_state_count, 0, 0) == 0) {
        return nullptr;
    }
    void* build_interface = nullptr;
    EnterCriticalSection(&g_registry_lock);
    const auto found = g_hybrid_construction.find(station);
    if (found != g_hybrid_construction.end()) {
        build_interface = found->second.cursor_interface;
    }
    LeaveCriticalSection(&g_registry_lock);
    return build_interface;
}

std::uintptr_t __attribute__((fastcall))
construction_rig_build_hardpoints_hook(
    void* manager, void*, void* station) noexcept {
    if (hybrid_construction_interface(station)) {
        if (!g_logged_hybrid_construct_hardpoint_bypass) {
            g_logged_hybrid_construct_hardpoint_bypass = true;
            log_message("Hybrid Construction bypassed the native "
                        "ConstructionRigClass-only build-hardpoint loop");
        }
        return station && readable_range(
                bytes(station) + kProducerConstructionEffectOffset,
                sizeof(std::uintptr_t))
            ? *reinterpret_cast<const std::uintptr_t*>(
                  bytes(station) + kProducerConstructionEffectOffset)
            : static_cast<std::uintptr_t>(-1);
    }
    return a2fo_call_thiscall_1(
        at(g_armada, kConstructionRigBuildHardpointsRva), manager,
        reinterpret_cast<std::uintptr_t>(station));
}

void remove_hybrid_construction_state(void* station) noexcept {
    if (!station || !g_armada || !g_registry_lock_ready) return;
    {
        HybridConstructionTailSwap swapped(station);
        if (swapped.active()) {
            a2fo_call_thiscall_0(
                at(g_armada, kConstructionRigRemoveObjectRva), station);
        }
    }

    std::array<void*, kNativeQueueCapacity + 2> interfaces{};
    std::size_t interface_count = 0;
    auto remember_interface = [&interfaces, &interface_count](
                                  void* build_interface) noexcept {
        if (!build_interface) return;
        for (std::size_t index = 0; index < interface_count; ++index) {
            if (interfaces[index] == build_interface) return;
        }
        if (interface_count < interfaces.size()) {
            interfaces[interface_count++] = build_interface;
        }
    };
    EnterCriticalSection(&g_registry_lock);
    const auto found = g_hybrid_construction.find(station);
    if (found != g_hybrid_construction.end()) {
        remember_interface(found->second.cursor_interface);
        remember_interface(found->second.pending_interface);
        for (const HybridConstructionState::Placement& placement :
             found->second.placements) {
            remember_interface(placement.build_interface);
        }
        g_hybrid_construction.erase(found);
        InterlockedDecrement(&g_hybrid_construction_state_count);
    }
    LeaveCriticalSection(&g_registry_lock);
    for (std::size_t index = 0; index < interface_count; ++index) {
        destroy_build_position_interface(interfaces[index]);
    }
}

void* hybrid_position_interface_for_command(
    void* producer, void* command) noexcept {
    void* target_class = command && readable_range(
            bytes(command) + kBuildCommandTargetClassOffset,
            sizeof(void*))
        ? *reinterpret_cast<void**>(
              bytes(command) + kBuildCommandTargetClassOffset)
        : nullptr;
    if (target_has_explicit_method(
            producer, target_class, ProductionMethod::construct)) {
        if (!initialize_hybrid_construction_state(producer)) {
            return nullptr;
        }
        void* cursor_interface = hybrid_construction_interface(producer);
        void* command_interface = allocate_build_position_interface(
            producer, cursor_interface);
        if (!command_interface) return nullptr;

        void* replaced_pending = nullptr;
        bool stored = false;
        EnterCriticalSection(&g_registry_lock);
        const auto found = g_hybrid_construction.find(producer);
        if (found != g_hybrid_construction.end()) {
            replaced_pending = found->second.pending_interface;
            found->second.pending_interface = command_interface;
            found->second.pending_target_class = target_class;
            found->second.pending_queue_id = readable_range(
                    bytes(producer) + kNextQueueIdOffset,
                    sizeof(std::uint32_t))
                ? *reinterpret_cast<const std::uint32_t*>(
                      bytes(producer) + kNextQueueIdOffset)
                : 0;
            stored = true;
        }
        LeaveCriticalSection(&g_registry_lock);
        if (replaced_pending && replaced_pending != command_interface) {
            destroy_build_position_interface(replaced_pending);
        }
        if (!stored) {
            destroy_build_position_interface(command_interface);
            return nullptr;
        }
        return command_interface;
    }
    return producer && readable_range(
            bytes(producer) + kConstructionRigTailOffset, sizeof(void*))
        ? *reinterpret_cast<void**>(
              bytes(producer) + kConstructionRigTailOffset)
        : nullptr;
}

bool target_has_explicit_method(void* producer, void* target_class,
                                ProductionMethod wanted) noexcept {
    if (!producer || !target_class || !readable_range(
            bytes(producer) + kObjectClassOffset, sizeof(void*))) {
        return false;
    }
    void* producer_class = *reinterpret_cast<void**>(
        bytes(producer) + kObjectClassOffset);
    std::uint32_t target_project_id = 0;
    ProductionMethod method = ProductionMethod::legacy;
    return class_project_id(target_class, target_project_id) &&
        resolve_explicit_production_method(
            producer_class, target_project_id, method) &&
        method == wanted;
}

std::uint32_t queued_id_for_target(
    void* producer, void* target_class,
    std::uint32_t preferred_queue_id) noexcept {
    if (!producer || !target_class || !readable_range(
            bytes(producer) + kQueueHeadOffset, sizeof(void*))) {
        return 0;
    }
    void* queue_item = *reinterpret_cast<void**>(
        bytes(producer) + kQueueHeadOffset);
    std::uint32_t fallback_id = 0;
    for (std::uint32_t visited = 0;
         queue_item && visited < kNativeQueueCapacity; ++visited) {
        if (!readable_range(
                queue_item, kQueueItemIdOffset + sizeof(std::uint32_t))) {
            return 0;
        }
        void* queued_class = *reinterpret_cast<void**>(queue_item);
        const std::uint32_t queued_id =
            *reinterpret_cast<const std::uint32_t*>(
                bytes(queue_item) + kQueueItemIdOffset);
        if (queued_class == target_class) {
            if (preferred_queue_id && queued_id == preferred_queue_id) {
                return queued_id;
            }
            fallback_id = queued_id;
        }
        queue_item = *reinterpret_cast<void**>(
            bytes(queue_item) + kQueueItemNextOffset);
    }
    return preferred_queue_id ? 0 : fallback_id;
}

void bind_pending_construct_placement(
    void* producer, void* target_class, bool admitted) noexcept {
    if (!target_has_explicit_method(
            producer, target_class, ProductionMethod::construct) ||
        !g_registry_lock_ready) {
        return;
    }

    std::uint32_t expected_queue_id = 0;
    EnterCriticalSection(&g_registry_lock);
    const auto pending = g_hybrid_construction.find(producer);
    if (pending != g_hybrid_construction.end() &&
        pending->second.pending_target_class == target_class) {
        expected_queue_id = pending->second.pending_queue_id;
    }
    LeaveCriticalSection(&g_registry_lock);
    const std::uint32_t queue_id = admitted
        ? queued_id_for_target(
              producer, target_class, expected_queue_id)
        : 0;
    void* destroy_interface = nullptr;
    bool bound = false;
    EnterCriticalSection(&g_registry_lock);
    const auto found = g_hybrid_construction.find(producer);
    if (found != g_hybrid_construction.end() &&
        found->second.pending_target_class == target_class) {
        void* pending = found->second.pending_interface;
        found->second.pending_interface = nullptr;
        found->second.pending_target_class = nullptr;
        found->second.pending_queue_id = 0;
        if (pending && queue_id) {
            if (found->second.cursor_interface && readable_range(
                    bytes(pending) +
                        kBuildPositionInterfaceMatrixOffset,
                    kBuildPositionInterfaceMatrixSize) &&
                writable_range(
                    bytes(found->second.cursor_interface) +
                        kBuildPositionInterfaceMatrixOffset,
                    kBuildPositionInterfaceMatrixSize)) {
                // Preserve the cursor's most recently confirmed orientation
                // for the next placement without making queued jobs share the
                // same mutable interface.
                std::memcpy(
                    bytes(found->second.cursor_interface) +
                        kBuildPositionInterfaceMatrixOffset,
                    bytes(pending) +
                        kBuildPositionInterfaceMatrixOffset,
                    kBuildPositionInterfaceMatrixSize);
            }
            HybridConstructionState::Placement* destination = nullptr;
            for (HybridConstructionState::Placement& placement :
                 found->second.placements) {
                if (placement.queue_id == queue_id) {
                    destination = &placement;
                    break;
                }
                if (!destination && !placement.build_interface) {
                    destination = &placement;
                }
            }
            if (destination) {
                destroy_interface = destination->build_interface;
                destination->queue_id = queue_id;
                destination->target_class = target_class;
                destination->build_interface = pending;
                bound = true;
            } else {
                destroy_interface = pending;
            }
        } else {
            destroy_interface = pending;
        }
    }
    LeaveCriticalSection(&g_registry_lock);
    destroy_build_position_interface(destroy_interface);
    if (bound && !g_logged_hybrid_construct_placement_bound) {
        g_logged_hybrid_construct_placement_bound = true;
        log_message("Hybrid Construction bound a distinct native position "
                    "and rotation interface to each construct queue ID");
    }
}

void release_construct_placement(
    void* producer, std::uint32_t queue_id) noexcept {
    if (!producer || !queue_id || !g_registry_lock_ready) return;
    void* build_interface = nullptr;
    EnterCriticalSection(&g_registry_lock);
    const auto found = g_hybrid_construction.find(producer);
    if (found != g_hybrid_construction.end()) {
        for (HybridConstructionState::Placement& placement :
             found->second.placements) {
            if (placement.queue_id == queue_id) {
                build_interface = placement.build_interface;
                placement = {};
                break;
            }
        }
    }
    LeaveCriticalSection(&g_registry_lock);
    destroy_build_position_interface(build_interface);
}

void clear_construct_placements(void* producer) noexcept {
    if (!producer || !g_registry_lock_ready) return;
    std::array<void*, kNativeQueueCapacity + 1> interfaces{};
    std::size_t interface_count = 0;
    EnterCriticalSection(&g_registry_lock);
    const auto found = g_hybrid_construction.find(producer);
    if (found != g_hybrid_construction.end()) {
        if (found->second.pending_interface) {
            interfaces[interface_count++] =
                found->second.pending_interface;
            found->second.pending_interface = nullptr;
            found->second.pending_target_class = nullptr;
            found->second.pending_queue_id = 0;
        }
        for (HybridConstructionState::Placement& placement :
             found->second.placements) {
            if (placement.build_interface &&
                interface_count < interfaces.size()) {
                interfaces[interface_count++] = placement.build_interface;
            }
            placement = {};
        }
    }
    LeaveCriticalSection(&g_registry_lock);
    for (std::size_t index = 0; index < interface_count; ++index) {
        destroy_build_position_interface(interfaces[index]);
    }
}

bool has_evolution_barrier(void* producer) noexcept {
    if (!producer || !readable_range(
            bytes(producer) + kCurrentBuildClassOffset, sizeof(void*)) ||
        !readable_range(bytes(producer) + kQueueHeadOffset, sizeof(void*))) {
        return false;
    }

    void* active_class = *reinterpret_cast<void**>(
        bytes(producer) + kCurrentBuildClassOffset);
    if (target_has_explicit_method(
            producer, active_class, ProductionMethod::evolve)) {
        return true;
    }

    void* queue_item = *reinterpret_cast<void**>(
        bytes(producer) + kQueueHeadOffset);
    for (std::uint32_t visited = 0;
         queue_item && visited < kNativeQueueCapacity; ++visited) {
        if (!readable_range(
                queue_item, kQueueItemNextOffset + sizeof(void*))) {
            break;
        }
        void* queued_class = *reinterpret_cast<void**>(queue_item);
        if (target_has_explicit_method(
                producer, queued_class, ProductionMethod::evolve)) {
            return true;
        }
        queue_item = *reinterpret_cast<void**>(
            bytes(queue_item) + kQueueItemNextOffset);
    }
    return false;
}

RuntimeClassLists* runtime_lists_for_class(void* producer_class) noexcept {
    if (!producer_class || !g_registry_lock_ready) return nullptr;
    RuntimeClassLists* result = nullptr;
    EnterCriticalSection(&g_registry_lock);
    const auto found = g_runtime_class_lists.find(producer_class);
    if (found != g_runtime_class_lists.end()) result = found->second.get();
    LeaveCriticalSection(&g_registry_lock);
    return result;
}

bool raw_list_contains(void** list, void* target_class) noexcept {
    if (!list || !target_class || !readable_range(
            list, kRuntimeBuildListCapacity * sizeof(void*))) {
        return false;
    }
    for (std::size_t index = 0; index < kRuntimeBuildListCapacity; ++index) {
        if (list[index] == target_class) return true;
    }
    return false;
}

bool hybrid_research_target(void* producer, void* target_class) noexcept {
    if (!producer || !target_class || !readable_range(
            bytes(producer) + kObjectClassOffset, sizeof(void*))) {
        return false;
    }
    void* producer_class = *reinterpret_cast<void**>(
        bytes(producer) + kObjectClassOffset);
    RuntimeClassLists* lists = runtime_lists_for_class(producer_class);
    if (!lists || !lists->has_yard) return false;

    std::uint32_t target_project_id = 0;
    ProductionMethod method = ProductionMethod::legacy;
    if (class_project_id(target_class, target_project_id) &&
        resolve_explicit_production_method(
            producer_class, target_project_id, method)) {
        return method == ProductionMethod::research;
    }

    if (std::find(lists->yard.begin(), lists->yard.end(), target_class) !=
        lists->yard.end()) {
        return false;
    }
    if (std::find(lists->research.begin(), lists->research.end(),
                  target_class) != lists->research.end() ||
        raw_list_contains(lists->legacy_research, target_class)) {
        return true;
    }

    // Fleet Ops may substitute a tier-specific research table on the live
    // station. Include that current table without ever reclassifying an item
    // from the explicit yard array as a research pod.
    void*** active_list_slot = reinterpret_cast<void***>(
        bytes(producer) + kCurrentBuildClassOffset - sizeof(void*));
    if (!readable_range(active_list_slot, sizeof(void*))) return false;
    void** active_list = *active_list_slot;
    return active_list != lists->yard.data() &&
        raw_list_contains(active_list, target_class);
}

bool research_classes_conflict(void* existing_class,
                               void* target_class) noexcept {
    if (!existing_class || !target_class) return false;
    if (existing_class == target_class) return true;
    const std::size_t class_bytes =
        kResearchPodFamilyOffset + sizeof(std::int32_t);
    if (!readable_range(existing_class, class_bytes) ||
        !readable_range(target_class, class_bytes)) {
        return false;
    }
    const std::uint8_t existing_is_pod = *reinterpret_cast<const std::uint8_t*>(
        bytes(existing_class) + kResearchPodClassFlagOffset);
    const std::uint8_t target_is_pod = *reinterpret_cast<const std::uint8_t*>(
        bytes(target_class) + kResearchPodClassFlagOffset);
    if (!existing_is_pod || !target_is_pod) return false;

    const std::int32_t existing_family =
        *reinterpret_cast<const std::int32_t*>(
            bytes(existing_class) + kResearchPodFamilyOffset);
    const std::int32_t target_family =
        *reinterpret_cast<const std::int32_t*>(
            bytes(target_class) + kResearchPodFamilyOffset);
    if (existing_family != target_family) return false;

    const std::int32_t existing_level =
        *reinterpret_cast<const std::int32_t*>(
            bytes(existing_class) + kResearchPodLevelOffset);
    const std::int32_t target_level =
        *reinterpret_cast<const std::int32_t*>(
            bytes(target_class) + kResearchPodLevelOffset);
    // Match ResearchStation's native attached-pod rule: exact classes always
    // conflict, and a lower tier cannot be ordered behind a higher tier from
    // the same research-pod family.
    return target_level < existing_level;
}

bool hybrid_station_has_queue_capacity(void* producer) noexcept {
    if (!producer || !readable_range(
            bytes(producer) + kObjectClassOffset, sizeof(void*)) ||
        !readable_range(bytes(producer) + kQueueCountOffset,
                        sizeof(std::uint32_t))) {
        return false;
    }
    void* producer_class = *reinterpret_cast<void**>(
        bytes(producer) + kObjectClassOffset);
    RuntimeClassLists* lists = runtime_lists_for_class(producer_class);
    const std::uint32_t count =
        *reinterpret_cast<const std::uint32_t*>(
            bytes(producer) + kQueueCountOffset);
    return lists && lists->has_yard &&
        !has_evolution_barrier(producer) &&
        count < kNativeQueueCapacity;
}

bool active_hybrid_queue(void* producer) noexcept {
    if (!producer || !readable_range(
            bytes(producer) + kObjectClassOffset, sizeof(void*)) ||
        !readable_range(bytes(producer) + kCurrentBuildClassOffset,
                        sizeof(void*)) ||
        !readable_range(bytes(producer) + kQueueCountOffset,
                        sizeof(std::uint32_t))) {
        return false;
    }
    void* producer_class = *reinterpret_cast<void**>(
        bytes(producer) + kObjectClassOffset);
    RuntimeClassLists* lists = runtime_lists_for_class(producer_class);
    const std::uint32_t count =
        *reinterpret_cast<const std::uint32_t*>(
            bytes(producer) + kQueueCountOffset);
    void* active_class = *reinterpret_cast<void**>(
        bytes(producer) + kCurrentBuildClassOffset);
    // A ResearchStation only opts into the full queue display when it also
    // publishes a yard list. Ordinary research-only stations retain their
    // native single-job display.
    return lists && lists->has_yard && (active_class || count != 0);
}

void* single_hybrid_station(void* craft_array) noexcept {
    if (!readable_range(craft_array, 12)) return nullptr;
    void** selected = *reinterpret_cast<void***>(craft_array);
    const std::int32_t count = *reinterpret_cast<const std::int32_t*>(
        bytes(craft_array) + 8);
    if (count != 1 || !readable_range(selected, sizeof(void*)) ||
        !readable_range(selected[0], kObjectClassOffset + sizeof(void*))) {
        return nullptr;
    }
    void* station = selected[0];
    void* station_class = *reinterpret_cast<void**>(
        bytes(station) + kObjectClassOffset);
    RuntimeClassLists* lists = runtime_lists_for_class(station_class);
    if (!lists || !lists->has_yard ||
        (!lists->has_explicit_research && !lists->legacy_research)) {
        return nullptr;
    }
    return station;
}

bool selection_contains(void* craft_array, void* target) noexcept {
    if (!target || !readable_range(craft_array, 12)) return false;
    void** selected = *reinterpret_cast<void***>(craft_array);
    const std::int32_t count = *reinterpret_cast<const std::int32_t*>(
        bytes(craft_array) + 8);
    if (count <= 0 || count > 256 ||
        !readable_range(selected,
                        static_cast<std::size_t>(count) * sizeof(void*))) {
        return false;
    }
    for (std::int32_t index = 0; index < count; ++index) {
        if (selected[index] == target) return true;
    }
    return false;
}

void update_research_buttons_with_queue(void* station) noexcept {
    if (!station || !readable_range(station, sizeof(void*))) return;
    void** vtable = *reinterpret_cast<void***>(station);
    if (!readable_range(vtable + kUpdateBuildButtonsVtableOffset /
                                  sizeof(void*), sizeof(void*))) {
        return;
    }
    void* update = vtable[kUpdateBuildButtonsVtableOffset / sizeof(void*)];
    if (!update) return;
    g_queue_enabled_button_station = station;
    a2fo_call_thiscall_0(update, station);
    g_queue_enabled_button_station = nullptr;
}

void disable_queued_research_buttons(void* station) noexcept {
    if (!station || !readable_range(
            bytes(station) + kCurrentBuildClassOffset - sizeof(void*),
            sizeof(void*)) ||
        !readable_range(bytes(station) + kProducerButtonListOffset,
                        sizeof(void*))) {
        return;
    }

    void** build_items = *reinterpret_cast<void***>(
        bytes(station) + kCurrentBuildClassOffset - sizeof(void*));
    void* button_list = *reinterpret_cast<void**>(
        bytes(station) + kProducerButtonListOffset);
    if (!readable_range(
            build_items, kNativeResearchButtonCount * sizeof(void*)) ||
        !writable_range(bytes(button_list) + kButtonListEnabledMaskOffset,
                        sizeof(std::uint32_t))) {
        return;
    }

    auto* enabled_mask = reinterpret_cast<std::uint32_t*>(
        bytes(button_list) + kButtonListEnabledMaskOffset);
    bool disabled_conflict = false;
    for (std::size_t index = 0;
         index < kNativeResearchButtonCount; ++index) {
        void* target_class = build_items[index];
        if (!target_class ||
            !hybrid_production_has_queued_research_conflict(
                station, target_class)) {
            continue;
        }
        *enabled_mask &= ~(std::uint32_t{1} << (index + 1));
        // Fleet Ops copies ButtonList state into a separate array of popup
        // ControlButtons. Match the control by its already-bound ModeInfo
        // target and change only its state. Rebinding by an assumed visual
        // index can copy this pod's ModeInfo over the neighbouring button.
        void** popup_buttons = at<void*>(
            g_fleet_ops, kFoPopupButtonPointerArrayRva);
        for (std::size_t control_index = 0;
             control_index < kFoPopupButtonCount; ++control_index) {
            void** popup_button = popup_buttons + control_index;
            if (!readable_range(popup_button, sizeof(void*)) ||
                !*popup_button || !readable_range(
                    bytes(*popup_button) + kControlButtonModeInfoOffset,
                    sizeof(void*))) {
                continue;
            }
            void* mode_info = *reinterpret_cast<void**>(
                bytes(*popup_button) + kControlButtonModeInfoOffset);
            if (!readable_range(
                    bytes(mode_info) + kModeInfoTargetClassOffset,
                    sizeof(void*)) ||
                *reinterpret_cast<void**>(
                    bytes(mode_info) + kModeInfoTargetClassOffset) !=
                    target_class ||
                !writable_range(
                    bytes(*popup_button) + kControlButtonStateOffset,
                    sizeof(std::uint32_t))) {
                continue;
            }
            *reinterpret_cast<std::uint32_t*>(
                bytes(*popup_button) + kControlButtonStateOffset) = 0;
        }
        disabled_conflict = true;
    }
    if (disabled_conflict && !g_logged_queued_research_button_disabled) {
        g_logged_queued_research_button_disabled = true;
        log_message("Hybrid ResearchStation disabled research buttons for "
                    "active or queued pod conflicts");
    }
}

void prepare_hybrid_station_menu(void* station,
                                 std::uint32_t menu) noexcept {
    if (!station || !writable_range(
            bytes(station) + kCurrentBuildClassOffset - sizeof(void*),
            sizeof(void*))) {
        return;
    }
    void* station_class = *reinterpret_cast<void**>(
        bytes(station) + kObjectClassOffset);
    RuntimeClassLists* lists = runtime_lists_for_class(station_class);
    if (!lists) return;

    void** selected_items = nullptr;
    if (menu == kBuildMenu &&
        (lists->has_yard || lists->has_construct)) {
        const bool show_construct = lists->has_construct &&
            (g_hybrid_build_palette == HybridBuildPalette::construct ||
             !lists->has_yard);
        selected_items = show_construct ? lists->construct.data()
                                        : lists->yard.data();
        *reinterpret_cast<void***>(
            bytes(station) + kCurrentBuildClassOffset - sizeof(void*)) =
            selected_items;
        // The ResearchStation override would reinterpret attached upgrade pods
        // over these slots. The Producer base refresh is the correct yard path.
        g_queue_enabled_button_station = station;
        a2fo_call_thiscall_0(
            at(g_armada, kProducerUpdateBuildButtonsRva), station);
        g_queue_enabled_button_station = nullptr;
        return;
    }
    if (menu == kEvolveMenu && lists->has_evolve) {
        selected_items = lists->evolve.data();
        *reinterpret_cast<void***>(
            bytes(station) + kCurrentBuildClassOffset - sizeof(void*)) =
            selected_items;
        // Evolution uses Producer timing, resources, and FIFO state. The
        // Evolver-only cocoon fields deliberately never touch this host.
        g_queue_enabled_button_station = station;
        a2fo_call_thiscall_0(
            at(g_armada, kProducerUpdateBuildButtonsRva), station);
        g_queue_enabled_button_station = nullptr;
        return;
    }
    if (menu != kResearchMenu) return;

    selected_items = lists->has_explicit_research
        ? lists->research.data() : lists->legacy_research;
    if (!selected_items) return;
    *reinterpret_cast<void***>(
        bytes(station) + kCurrentBuildClassOffset - sizeof(void*)) =
        selected_items;
    update_research_buttons_with_queue(station);
}

void prepare_selected_hybrid_menus(void* craft_array,
                                   std::uint32_t menu) noexcept {
    if ((menu != kBuildMenu && menu != kResearchMenu &&
         menu != kEvolveMenu) ||
        !readable_range(craft_array, 12)) {
        return;
    }
    void** selected = *reinterpret_cast<void***>(craft_array);
    const std::int32_t raw_count = *reinterpret_cast<const std::int32_t*>(
        bytes(craft_array) + 8);
    if (raw_count <= 0 || raw_count > 256 ||
        !readable_range(selected,
                        static_cast<std::size_t>(raw_count) * sizeof(void*))) {
        return;
    }
    for (std::int32_t index = 0; index < raw_count; ++index) {
        prepare_hybrid_station_menu(selected[index], menu);
    }
}

std::uintptr_t __attribute__((fastcall)) producer_is_busy_hook(
    void* producer, void*) noexcept {
    const void* return_address = __builtin_return_address(0);
    const bool build_order_query = g_armada &&
        (return_address == at(
             g_armada, kBuildCommandCleanupIsBusyReturnRva) ||
         return_address == at(
             g_armada, kBuildCommandAdmitIsBusyReturnRva));
    const bool menu_query = producer &&
        producer == g_queue_enabled_button_station;
    // ConstructionRig::GetAction normally refuses to create a placement
    // action while its Producer is busy. A hybrid station can legitimately
    // place another constructItem behind unrelated work in its shared FIFO,
    // so expose that action while this station still has queue capacity. The
    // sidecar check keeps native construction ships and yards unchanged.
    const bool construct_action_query = g_armada &&
        return_address == at(
            g_armada, kConstructionRigGetActionBusyReturnRva) &&
        hybrid_construction_interface(producer);
    if ((menu_query || build_order_query || construct_action_query) &&
        hybrid_station_has_queue_capacity(producer)) {
        if (menu_query && !g_logged_shared_queue_ui) {
            g_logged_shared_queue_ui = true;
            log_message("Hybrid ResearchStation Build/Research buttons "
                        "use the shared ten-slot Producer queue");
        }
        if (build_order_query && !g_logged_shared_queue_command) {
            g_logged_shared_queue_command = true;
            log_message("Hybrid ResearchStation build orders admit pending "
                        "items until the shared Producer FIFO is full");
        }
        if (construct_action_query &&
            !g_logged_shared_queue_construct_action) {
            g_logged_shared_queue_construct_action = true;
            log_message("Hybrid Construction placement remains available "
                        "while the shared queue has capacity");
        }
        return 0;
    }
    return a2fo_call_thiscall_0(g_producer_is_busy_hook.gateway, producer);
}

void __attribute__((fastcall)) fo_producer_pop_checked_hook(
    void* producer, void*) noexcept {
    const void* return_address = __builtin_return_address(0);
    const bool command_replacement_pop = g_armada &&
        (return_address == at(
             g_armada, kBuildCommandCleanupPopReturnRva) ||
         return_address == at(
             g_armada, kBuildCommandReplacePopReturnRva));
    if (command_replacement_pop &&
        hybrid_station_has_queue_capacity(producer)) {
        if (!g_logged_preserved_command_queue) {
            g_logged_preserved_command_queue = true;
            log_message("Hybrid ResearchStation preserved existing FIFO "
                        "items while admitting another build command");
        }
        return;
    }
    a2fo_call_thiscall_0(g_fo_producer_pop_checked_hook.gateway, producer);
}

std::uintptr_t dispatch_hybrid_get_action(
    void* fallback, void* producer, void* result, void* mode_info,
    void* where) noexcept {
    void* target_class = mode_info && readable_range(
            bytes(mode_info) + kModeInfoTargetClassOffset, sizeof(void*))
        ? *reinterpret_cast<void**>(
              bytes(mode_info) + kModeInfoTargetClassOffset)
        : nullptr;
    const bool placement_mode = mode_info && readable_range(
            bytes(mode_info) + kModeInfoTypeOffset,
            sizeof(std::uint32_t)) &&
        *reinterpret_cast<const std::uint32_t*>(
            bytes(mode_info) + kModeInfoTypeOffset) == 1;
    if (placement_mode && target_has_explicit_method(
            producer, target_class, ProductionMethod::construct)) {
        if (initialize_hybrid_construction_state(producer)) {
            HybridConstructionTailSwap swapped(producer, false);
            if (swapped.active()) {
                if (!g_logged_hybrid_construct_action) {
                    g_logged_hybrid_construct_action = true;
                    log_message("Hybrid Construction item entered the "
                                "native station-placement action through "
                                "the scoped ConstructionRig identity");
                }
                return a2fo_call_thiscall_3(
                    at(g_armada, kConstructionRigGetActionRva), producer,
                    reinterpret_cast<std::uintptr_t>(result),
                    reinterpret_cast<std::uintptr_t>(mode_info),
                    reinterpret_cast<std::uintptr_t>(where));
            }
        }
    }
    return fallback
        ? a2fo_call_thiscall_3(
              fallback, producer,
              reinterpret_cast<std::uintptr_t>(result),
              reinterpret_cast<std::uintptr_t>(mode_info),
              reinterpret_cast<std::uintptr_t>(where))
        : 0;
}

std::uintptr_t __attribute__((fastcall)) hybrid_get_action_vtable_hook(
    void* producer, void*, void* result, void* mode_info,
    void* where) noexcept {
    void* original = nullptr;
    if (producer && readable_range(producer, sizeof(void*)) &&
        g_registry_lock_ready) {
        void** vtable = *reinterpret_cast<void***>(producer);
        EnterCriticalSection(&g_registry_lock);
        const auto found = g_hybrid_get_action_originals.find(vtable);
        if (found != g_hybrid_get_action_originals.end()) {
            original = found->second;
        }
        LeaveCriticalSection(&g_registry_lock);
    }
    if (!original) original = g_producer_get_action_hook.gateway;
    return dispatch_hybrid_get_action(
        original, producer, result, mode_info, where);
}

void ensure_hybrid_get_action_route(void* station) noexcept {
    if (!station || !g_registry_lock_ready || !readable_range(
            station, sizeof(void*))) {
        return;
    }
    void** vtable = *reinterpret_cast<void***>(station);
    void** slot = vtable
        ? reinterpret_cast<void**>(
              bytes(vtable) + kGetActionVtableOffset)
        : nullptr;
    if (!readable_range(slot, sizeof(void*))) return;
    void* current = *slot;
    if (!current || current == reinterpret_cast<void*>(
            &hybrid_get_action_vtable_hook)) {
        return;
    }

    try {
        EnterCriticalSection(&g_registry_lock);
        const auto existing = g_hybrid_get_action_originals.find(vtable);
        if (existing != g_hybrid_get_action_originals.end()) {
            LeaveCriticalSection(&g_registry_lock);
            return;
        }
        g_hybrid_get_action_originals.emplace(vtable, current);
        LeaveCriticalSection(&g_registry_lock);
    } catch (...) {
        LeaveCriticalSection(&g_registry_lock);
        return;
    }

    DWORD previous_protection = 0;
    if (!VirtualProtect(
            slot, sizeof(void*), PAGE_EXECUTE_READWRITE,
            &previous_protection)) {
        EnterCriticalSection(&g_registry_lock);
        g_hybrid_get_action_originals.erase(vtable);
        LeaveCriticalSection(&g_registry_lock);
        return;
    }
    *slot = reinterpret_cast<void*>(&hybrid_get_action_vtable_hook);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), previous_protection, &ignored);
    if (!g_logged_hybrid_get_action_vtable) {
        g_logged_hybrid_get_action_vtable = true;
        log_message("Hybrid Construction routed the selected station's "
                    "GetAction vtable through native placement");
    }
}

std::uintptr_t __attribute__((regparm(3))) build_button_bind_hook(
    void* button, void* mode_info, std::uintptr_t enabled) noexcept {
    auto state_mode_info =
        reinterpret_cast<ControlButtonStateModeInfoFunction>(at(
            g_fleet_ops, kFoControlButtonStateModeInfoRva));
    if (!g_split_hybrid_root_buttons) {
        return state_mode_info(button, mode_info, enabled);
    }

    g_hybrid_root_build_mode_info = mode_info;
    void* yard_button = g_hybrid_yard_root_button
        ? g_hybrid_yard_root_button : button;
    g_last_hybrid_yard_root_button = yard_button;
    const std::uintptr_t result =
        state_mode_info(yard_button, mode_info, enabled);
    if (g_hybrid_construct_root_button) {
        g_last_hybrid_construct_root_button =
            g_hybrid_construct_root_button;
        void* construct_mode_info = mode_info;
        if (mode_info && readable_range(mode_info, kModeInfoSize)) {
            std::memcpy(g_hybrid_construct_mode_info.data(), mode_info,
                        kModeInfoSize);
            construct_mode_info = g_hybrid_construct_mode_info.data();
        }
        state_mode_info(
            g_hybrid_construct_root_button, construct_mode_info, enabled);
    } else {
        g_last_hybrid_construct_root_button = nullptr;
    }
    return result;
}

std::uintptr_t __attribute__((fastcall))
mode_info_build_button_name_hook(
    void* mode_info, void*, void* output_name) noexcept {
    auto call_original = [&]() noexcept {
        return a2fo_call_thiscall_1(
            g_mode_info_build_button_name_hook.gateway, mode_info,
            reinterpret_cast<std::uintptr_t>(output_name));
    };
    if (mode_info != g_hybrid_construct_mode_info.data() || !g_armada ||
        !readable_range(bytes(mode_info) + kModeInfoActionIndexOffset,
                        sizeof(std::uint32_t))) {
        return call_original();
    }

    const std::uint32_t action_index =
        *reinterpret_cast<const std::uint32_t*>(
            bytes(mode_info) + kModeInfoActionIndexOffset);
    if (action_index > kMaximumButtonActionIndex) {
        return call_original();
    }
    const std::uintptr_t stem_rva = kButtonActionMetadataRva +
        static_cast<std::uintptr_t>(action_index) *
            kButtonActionMetadataRecordSize +
        kButtonActionSpriteStemOffset;
    const char** stem = at<const char*>(g_armada, stem_rva);
    if (!readable_range(stem, sizeof(*stem)) ||
        !writable_range(stem, sizeof(*stem))) {
        return call_original();
    }

    // This native UI path is synchronous and single-threaded. Keep Armada's
    // shared Build record changed only across this Construction clone's own
    // filename call, then restore it before any other control is simulated.
    const char* previous_stem = *stem;
    *stem = kConstructButtonSpriteStem;
    const std::uintptr_t result = call_original();
    *stem = previous_stem;
    if (!g_logged_hybrid_construct_root_icon) {
        g_logged_hybrid_construct_root_icon = true;
        log_message("Hybrid Construction root resolved the distinct "
                    "b_construct sprite");
    }
    return result;
}

void rebase_hybrid_build_root_buttons_after_compaction(
    const RuntimeClassLists* lists) noexcept {
    if (!lists || !g_fleet_ops || !g_hybrid_root_build_mode_info) {
        return;
    }

    void** button_pointers = at<void*>(
        g_fleet_ops, kFoPopupButtonPointerArrayRva);
    if (!readable_range(
            button_pointers, kFoPopupButtonCount * sizeof(void*))) {
        return;
    }

    void* yard_mode_button = nullptr;
    void* construct_mode_button = nullptr;
    void* first_legacy_build_button = nullptr;
    void* second_legacy_build_button = nullptr;
    for (std::size_t index = 0; index < kFoPopupButtonCount; ++index) {
        void* button = button_pointers[index];
        if (!button || !readable_range(
                bytes(button) + kControlButtonModeInfoOffset,
                2 * sizeof(void*))) {
            continue;
        }
        void* primary_mode = *reinterpret_cast<void**>(
            bytes(button) + kControlButtonModeInfoOffset);
        void* alternate_mode = *reinterpret_cast<void**>(
            bytes(button) + kControlButtonModeInfoOffset + sizeof(void*));
        const bool yard_mode =
            primary_mode == g_hybrid_root_build_mode_info ||
            alternate_mode == g_hybrid_root_build_mode_info;
        const bool construct_mode =
            primary_mode == g_hybrid_construct_mode_info.data() ||
            alternate_mode == g_hybrid_construct_mode_info.data();
        if (yard_mode && !yard_mode_button) {
            yard_mode_button = button;
        }
        if (construct_mode && !construct_mode_button) {
            construct_mode_button = button;
        }
        // Preserve the earlier shared-ModeInfo recovery as a compatibility
        // fallback if another hook copies or replaces our cloned pointer.
        if (yard_mode) {
            if (!first_legacy_build_button) {
                first_legacy_build_button = button;
            } else if (button != first_legacy_build_button &&
                       !second_legacy_build_button) {
                second_legacy_build_button = button;
            }
        }
    }

    if (lists->has_yard) {
        g_last_hybrid_yard_root_button = yard_mode_button
            ? yard_mode_button : first_legacy_build_button;
    }
    if (lists->has_construct) {
        g_last_hybrid_construct_root_button = construct_mode_button
            ? construct_mode_button
            : lists->has_yard ? second_legacy_build_button
                              : first_legacy_build_button;
    }
    if (lists->has_yard && lists->has_construct &&
        g_last_hybrid_yard_root_button &&
        g_last_hybrid_construct_root_button &&
        !g_logged_hybrid_root_button_rebase) {
        g_logged_hybrid_root_button_rebase = true;
        log_message("Hybrid Yard/Construction root identities followed "
                    "Fleet Ops palette compaction");
    }
}

std::uintptr_t __attribute__((regparm(3))) evolve_button_bind_hook(
    void* button, void* mode_info, std::uintptr_t enabled) noexcept {
    if (g_split_hybrid_root_buttons && g_hybrid_evolve_root_button) {
        button = g_hybrid_evolve_root_button;
    }
    auto state_mode_info =
        reinterpret_cast<ControlButtonStateModeInfoFunction>(at(
            g_fleet_ops, kFoControlButtonStateModeInfoRva));
    return state_mode_info(button, mode_info, enabled);
}

std::uintptr_t __attribute__((regparm(3))) ai_button_bind_hook(
    void* button, void* mode_info, std::uintptr_t enabled) noexcept {
    if (g_split_hybrid_root_buttons) {
        void** hybrid_ai = at<void*>(
            g_fleet_ops, kFoPopupHybridAiButtonPointerRva);
        if (readable_range(hybrid_ai, sizeof(void*)) && *hybrid_ai) {
            button = *hybrid_ai;
        }
    }
    auto state_mode_info =
        reinterpret_cast<ControlButtonStateModeInfoFunction>(at(
            g_fleet_ops, kFoControlButtonStateModeInfoRva));
    return state_mode_info(button, mode_info, enabled);
}

void complete_research_menu_retention() noexcept {
    g_retain_research_menu_station = nullptr;
    g_retain_research_menu_refreshes = 0;
    g_retain_research_menu_saw_root = false;
    if (!g_logged_retained_research_menu) {
        g_logged_retained_research_menu = true;
        log_message("Hybrid ResearchStation retained the Research palette "
                    "after a pod order");
    }
}

std::uintptr_t __attribute__((fastcall)) popup_update_buttons_hook(
    void* popup, void*, void* craft_array, std::uintptr_t argument2,
    std::uintptr_t argument3) noexcept {
    g_last_popup = popup;
    g_last_popup_craft_array = craft_array;
    g_last_popup_argument2 = argument2;
    g_last_popup_argument3 = argument3;
    std::uint32_t menu = kRootMenu;
    bool have_menu = popup && readable_range(
        bytes(popup) + kPopupCurrentMenuOffset, sizeof(menu));
    if (have_menu) {
        menu = *reinterpret_cast<const std::uint32_t*>(
            bytes(popup) + kPopupCurrentMenuOffset);
    }
    if (menu == kRootMenu && g_control_button_press_depth == 0 &&
        !g_hybrid_build_palette_press_armed) {
        g_hybrid_build_palette = HybridBuildPalette::yard;
    }

    void* hybrid_station = single_hybrid_station(craft_array);
    if (hybrid_station) {
        ensure_hybrid_get_action_route(hybrid_station);
    }
    void* retained_station = g_retain_research_menu_station;
    const bool retain_selection =
        g_retain_research_menu_refreshes != 0 &&
        (hybrid_station ||
         selection_contains(craft_array, retained_station) ||
         g_last_single_hybrid_selection == retained_station);
    if (!hybrid_station) {
        if (!retain_selection) {
            g_last_single_hybrid_selection = nullptr;
            g_last_hybrid_yard_root_button = nullptr;
            g_last_hybrid_construct_root_button = nullptr;
            g_hybrid_build_palette_press_armed = false;
        }
    } else if (hybrid_station != g_last_single_hybrid_selection) {
        g_last_single_hybrid_selection = hybrid_station;
        g_hybrid_build_palette = HybridBuildPalette::yard;
        g_hybrid_build_palette_press_armed = false;
        if (popup && writable_range(
                bytes(popup) + kPopupCurrentMenuOffset, sizeof(menu))) {
            menu = kRootMenu;
            *reinterpret_cast<std::uint32_t*>(
                bytes(popup) + kPopupCurrentMenuOffset) = menu;
            have_menu = true;
        }
    }

    if (retain_selection && have_menu && menu == kRootMenu && popup &&
        writable_range(bytes(popup) + kPopupCurrentMenuOffset,
                       sizeof(menu))) {
        g_retain_research_menu_saw_root = true;
        menu = kResearchMenu;
        *reinterpret_cast<std::uint32_t*>(
            bytes(popup) + kPopupCurrentMenuOffset) = menu;
    }

    if (have_menu) {
        prepare_selected_hybrid_menus(craft_array, menu);
        if (menu == kBuildMenu) {
            g_hybrid_build_palette_press_armed = false;
        }
    }
    const bool previous_split = g_split_hybrid_root_buttons;
    void* previous_yard_root_button = g_hybrid_yard_root_button;
    void* previous_construct_root_button =
        g_hybrid_construct_root_button;
    void* previous_evolve_root_button = g_hybrid_evolve_root_button;
    void* previous_build_mode_info = g_hybrid_root_build_mode_info;
    g_split_hybrid_root_buttons = hybrid_station &&
        (menu == kRootMenu || menu == 8);
    g_hybrid_yard_root_button = nullptr;
    g_hybrid_construct_root_button = nullptr;
    g_hybrid_evolve_root_button = nullptr;
    g_hybrid_root_build_mode_info = nullptr;
    RuntimeClassLists* hybrid_lists = nullptr;
    if (g_split_hybrid_root_buttons && readable_range(
            bytes(hybrid_station) + kObjectClassOffset, sizeof(void*))) {
        void* station_class = *reinterpret_cast<void**>(
            bytes(hybrid_station) + kObjectClassOffset);
        hybrid_lists = runtime_lists_for_class(station_class);
        void** yard_button_pointer = at<void*>(
            g_fleet_ops, kFoPopupSpareRootButtonPointerRva);
        if (hybrid_lists && hybrid_lists->has_yard &&
            readable_range(yard_button_pointer, sizeof(void*))) {
            g_hybrid_yard_root_button = *yard_button_pointer;
        }
        void** construct_button_pointer = at<void*>(
            g_fleet_ops, kFoPopupConstructionButtonPointerRva);
        if (hybrid_lists && hybrid_lists->has_construct &&
            readable_range(construct_button_pointer, sizeof(void*))) {
            g_hybrid_construct_root_button = *construct_button_pointer;
        }
        if (readable_range(
                bytes(station_class) + kClassMenuCapabilitiesOffset,
                sizeof(std::uint32_t))) {
            const std::uint32_t capabilities =
                *reinterpret_cast<const std::uint32_t*>(
                    bytes(station_class) + kClassMenuCapabilitiesOffset);
            const std::uintptr_t button_rva =
                (capabilities & 0x2u) == 0
                    ? kFoPopupPreferredEvolveButtonPointerRva
                    : kFoPopupFallbackEvolveButtonPointerRva;
            void** button_pointer = at<void*>(g_fleet_ops, button_rva);
            if (readable_range(button_pointer, sizeof(void*))) {
                g_hybrid_evolve_root_button = *button_pointer;
            }
        }
    }
    // Fleet Ops performs another capability/button refresh inside the popup
    // gateway. Keep the queue-capacity IsBusy result active across that whole
    // refresh; limiting it to prepare_hybrid_station_menu allowed the gateway
    // to disable every choice again as soon as the first job became active.
    void* previous_queue_station = g_queue_enabled_button_station;
    if (hybrid_station && (menu == kBuildMenu || menu == kResearchMenu ||
                           menu == kEvolveMenu)) {
        g_queue_enabled_button_station = hybrid_station;
    }
    const std::uintptr_t result = a2fo_call_thiscall_3(
        g_popup_update_buttons_hook.gateway, popup,
        reinterpret_cast<std::uintptr_t>(craft_array), argument2, argument3);
    if (g_split_hybrid_root_buttons) {
        rebase_hybrid_build_root_buttons_after_compaction(hybrid_lists);
    }
    std::uint32_t gateway_menu = menu;
    const bool have_gateway_menu = popup && readable_range(
        bytes(popup) + kPopupCurrentMenuOffset, sizeof(gateway_menu));
    if (have_gateway_menu) {
        gateway_menu = *reinterpret_cast<const std::uint32_t*>(
            bytes(popup) + kPopupCurrentMenuOffset);
    }

    if (g_retain_research_menu_refreshes != 0) {
        if (retain_selection && have_gateway_menu &&
            gateway_menu == kRootMenu && popup && writable_range(
                bytes(popup) + kPopupCurrentMenuOffset,
                sizeof(gateway_menu))) {
            // A pod order can temporarily make Fleet Ops treat the selection
            // as plural. Its menu-2/3 branch then writes root mode after the
            // hook's pre-pass. Put Research back for the next refresh and keep
            // the marker until that Research refresh completes.
            g_retain_research_menu_saw_root = true;
            *reinterpret_cast<std::uint32_t*>(
                bytes(popup) + kPopupCurrentMenuOffset) = kResearchMenu;
            if (hybrid_station) {
                // The popup update is event-driven, so merely restoring its
                // menu field can leave the already-bound root controls on
                // screen indefinitely. One direct, non-hooked refresh applies
                // menu 3 immediately after Fleet Ops' late root reset.
                prepare_hybrid_station_menu(
                    hybrid_station, kResearchMenu);
                a2fo_call_thiscall_3(
                    g_popup_update_buttons_hook.gateway, popup,
                    reinterpret_cast<std::uintptr_t>(craft_array),
                    argument2, argument3);
                gateway_menu = *reinterpret_cast<const std::uint32_t*>(
                    bytes(popup) + kPopupCurrentMenuOffset);
                if (gateway_menu == kResearchMenu) {
                    complete_research_menu_retention();
                } else {
                    *reinterpret_cast<std::uint32_t*>(
                        bytes(popup) + kPopupCurrentMenuOffset) =
                        kResearchMenu;
                }
            }
        } else if (retain_selection && g_retain_research_menu_saw_root &&
                   have_gateway_menu && gateway_menu == kResearchMenu) {
            complete_research_menu_retention();
        }
        if (g_retain_research_menu_refreshes != 0 &&
            --g_retain_research_menu_refreshes == 0) {
            g_retain_research_menu_station = nullptr;
            g_retain_research_menu_saw_root = false;
        }
    }
    // Fleet Ops has now completed its final button refresh. Applying the
    // queued-pod uniqueness mask here avoids detouring Armada's virtual
    // ResearchStation::UpdateBuildButtons entry, which Fleet Ops also invokes
    // through its own patched UI path.
    void* research_station = hybrid_station ? hybrid_station
        : retain_selection ? retained_station : nullptr;
    if (research_station && have_gateway_menu &&
        gateway_menu == kResearchMenu) {
        disable_queued_research_buttons(research_station);
    }
    g_queue_enabled_button_station = previous_queue_station;
    g_split_hybrid_root_buttons = previous_split;
    g_hybrid_yard_root_button = previous_yard_root_button;
    g_hybrid_construct_root_button = previous_construct_root_button;
    g_hybrid_evolve_root_button = previous_evolve_root_button;
    g_hybrid_root_build_mode_info = previous_build_mode_info;
    return result;
}

void restore_research_palette_after_button_press() noexcept {
    void* station = g_post_press_research_menu_station;
    if (!station || !g_last_popup || !g_last_popup_craft_array ||
        !g_popup_update_buttons_hook.gateway ||
        !writable_range(bytes(g_last_popup) + kPopupCurrentMenuOffset,
                        sizeof(std::uint32_t))) {
        return;
    }

    if (!readable_range(
            bytes(station) + kObjectClassOffset, sizeof(void*)) ||
        !runtime_lists_for_class(*reinterpret_cast<void**>(
            bytes(station) + kObjectClassOffset))) {
        return;
    }

    *reinterpret_cast<std::uint32_t*>(
        bytes(g_last_popup) + kPopupCurrentMenuOffset) = kResearchMenu;
    prepare_hybrid_station_menu(station, kResearchMenu);

    void* previous_queue_station = g_queue_enabled_button_station;
    const bool previous_split = g_split_hybrid_root_buttons;
    g_queue_enabled_button_station = station;
    g_split_hybrid_root_buttons = false;
    a2fo_call_thiscall_3(
        g_popup_update_buttons_hook.gateway, g_last_popup,
        reinterpret_cast<std::uintptr_t>(g_last_popup_craft_array),
        g_last_popup_argument2, g_last_popup_argument3);
    g_queue_enabled_button_station = previous_queue_station;
    g_split_hybrid_root_buttons = previous_split;

    auto* current_menu = reinterpret_cast<std::uint32_t*>(
        bytes(g_last_popup) + kPopupCurrentMenuOffset);
    if (*current_menu == kResearchMenu) {
        disable_queued_research_buttons(station);
        complete_research_menu_retention();
    } else {
        // Preserve the requested state even if Fleet Ops rejects this immediate
        // refresh; the existing bounded popup post-pass will retry it.
        *current_menu = kResearchMenu;
        g_retain_research_menu_saw_root = true;
    }
}

void select_hybrid_build_palette(void* button) noexcept {
    if (!button || !g_fleet_ops || !g_last_single_hybrid_selection ||
        !readable_range(
            bytes(g_last_single_hybrid_selection) + kObjectClassOffset,
            sizeof(void*))) {
        return;
    }
    void* station_class = *reinterpret_cast<void**>(
        bytes(g_last_single_hybrid_selection) + kObjectClassOffset);
    RuntimeClassLists* lists = runtime_lists_for_class(station_class);
    if (!lists) return;

    void** construct_pointer = at<void*>(
        g_fleet_ops, kFoPopupConstructionButtonPointerRva);
    const bool current_construct_pointer =
        readable_range(construct_pointer, sizeof(void*)) &&
        button == *construct_pointer;
    if (lists->has_construct &&
        (button == g_last_hybrid_construct_root_button ||
         current_construct_pointer)) {
        g_hybrid_build_palette = HybridBuildPalette::construct;
        g_hybrid_build_palette_press_armed = true;
        if (!g_logged_hybrid_construct_root_press) {
            g_logged_hybrid_construct_root_press = true;
            log_message("Hybrid Construction root button selected its "
                        "separate constructItem palette");
        }
        return;
    }
    void** yard_pointer = at<void*>(
        g_fleet_ops, kFoPopupSpareRootButtonPointerRva);
    const bool current_yard_pointer =
        readable_range(yard_pointer, sizeof(void*)) &&
        button == *yard_pointer;
    if (lists->has_yard &&
        (button == g_last_hybrid_yard_root_button ||
         current_yard_pointer)) {
        g_hybrid_build_palette = HybridBuildPalette::yard;
        g_hybrid_build_palette_press_armed = true;
    }
}

void* pressed_mode_info(void* button) noexcept {
    if (!button || !readable_range(
            bytes(button) + kControlButtonModeInfoOffset,
            sizeof(void*))) {
        return nullptr;
    }
    return *reinterpret_cast<void**>(
        bytes(button) + kControlButtonModeInfoOffset);
}

// Native PopupPalette::CheckCanExecute distinguishes a constructor from a
// yard by object flag 0x40. For ModeInfo type 1, constructors enter
// mSetActiveMode and wait for a world click; other Producers immediately emit
// QueueClassCommand(0x19). A hybrid ResearchStation must not keep that flag
// permanently because it changes unrelated menus and simulation behavior, so
// expose it only across this one synchronous constructItem press.
class HybridConstructionPlacementPress {
public:
    explicit HybridConstructionPlacementPress(void* button) noexcept {
        station_ = g_last_single_hybrid_selection;
        void* mode_info = pressed_mode_info(button);
        if (!station_ || !mode_info || !readable_range(
                bytes(mode_info) + kModeInfoTargetClassOffset,
                sizeof(void*)) || !readable_range(
                bytes(mode_info) + kModeInfoTypeOffset,
                sizeof(std::uint32_t))) {
            return;
        }
        const std::uint32_t type =
            *reinterpret_cast<const std::uint32_t*>(
                bytes(mode_info) + kModeInfoTypeOffset);
        void* target_class = *reinterpret_cast<void**>(
            bytes(mode_info) + kModeInfoTargetClassOffset);
        if (type != 1 || !target_has_explicit_method(
                station_, target_class, ProductionMethod::construct) ||
            !initialize_hybrid_construction_state(station_) ||
            !writable_range(
                bytes(station_) + kObjectFlagsOffset,
                sizeof(std::uint32_t))) {
            return;
        }

        ensure_hybrid_get_action_route(station_);
        auto* flags = reinterpret_cast<std::uint32_t*>(
            bytes(station_) + kObjectFlagsOffset);
        saved_flags_ = *flags;
        *flags = saved_flags_ | 0x40u;
        previous_press_station_ =
            g_hybrid_construct_placement_press_station;
        g_hybrid_construct_placement_press_station = station_;
        active_ = true;
        if (!g_logged_hybrid_construct_placement_press) {
            g_logged_hybrid_construct_placement_press = true;
            log_message("Hybrid Construction constructItem press entered "
                        "native cursor-placement mode before queue admission");
        }
    }

    ~HybridConstructionPlacementPress() {
        if (!active_) return;
        *reinterpret_cast<std::uint32_t*>(
            bytes(station_) + kObjectFlagsOffset) = saved_flags_;
        g_hybrid_construct_placement_press_station =
            previous_press_station_;
    }

    bool active() const noexcept { return active_; }

    HybridConstructionPlacementPress(
        const HybridConstructionPlacementPress&) = delete;
    HybridConstructionPlacementPress& operator=(
        const HybridConstructionPlacementPress&) = delete;

private:
    void* station_ = nullptr;
    void* previous_press_station_ = nullptr;
    std::uint32_t saved_flags_ = 0;
    bool active_ = false;
};

void __attribute__((fastcall)) object_control_button_press_hook(
    void* button, void*) noexcept {
    const bool outermost = g_control_button_press_depth++ == 0;
    select_hybrid_build_palette(button);
    HybridConstructionPlacementPress placement(button);
    // Fleet Ops ObjectControlButton bypasses PopupPalette and emits its
    // callback directly. A construct item needs the native base-button route
    // so CheckCanExecute can install the placement ModeInfo.
    a2fo_call_thiscall_0(
        placement.active() ? g_control_button_press_hook.gateway
                           : g_object_control_button_press_target,
        button);
    --g_control_button_press_depth;
    if (outermost) {
        restore_research_palette_after_button_press();
        g_post_press_research_menu_station = nullptr;
    }
}

void __attribute__((fastcall)) control_button_press_hook(
    void* button, void*) noexcept {
    const bool outermost = g_control_button_press_depth++ == 0;
    select_hybrid_build_palette(button);
    HybridConstructionPlacementPress placement(button);
    a2fo_call_thiscall_0(g_control_button_press_hook.gateway, button);
    --g_control_button_press_depth;
    if (outermost) {
        restore_research_palette_after_button_press();
        g_post_press_research_menu_station = nullptr;
    }
}

std::uintptr_t __attribute__((fastcall)) producer_get_action_hook(
    void* producer, void*, void* result, void* mode_info,
    void* where) noexcept {
    return dispatch_hybrid_get_action(
        g_producer_get_action_hook.gateway, producer, result,
        mode_info, where);
}

std::uintptr_t __attribute__((fastcall)) producer_start_effect_hook(
    void* station, void*) noexcept {
    if (!active_evolve_job(station)) {
        return a2fo_call_thiscall_0(
            g_producer_start_effect_hook.gateway, station);
    }
    if (!initialize_hybrid_cocoon_state(station)) {
        // Construction effects are cosmetic. Do not reject a synchronized
        // evolution order if the optional sidecar cannot be allocated.
        return 1;
    }

    std::uintptr_t result = 0;
    {
        HybridCocoonTailSwap swapped(station);
        if (swapped.active()) {
            result = a2fo_call_thiscall_0(
                at(g_armada, kEvolverStartConstructionEffectRva), station);
        }
    }
    if ((result & 0xffu) == 0 ||
        !hybrid_cocoon_has_instance(station)) {
        remove_hybrid_cocoon_effect(station, true);
        return 1;
    }
    if (!g_logged_hybrid_cocoon_start) {
        g_logged_hybrid_cocoon_start = true;
        log_message("Hybrid evolution cocoon started through a protected "
                    "Evolver-tail sidecar");
    }
    return result;
}

std::uintptr_t __attribute__((fastcall)) producer_cancel_effect_hook(
    void* station, void*) noexcept {
    if (!active_evolve_job(station) &&
        !hybrid_cocoon_has_instance(station)) {
        return a2fo_call_thiscall_0(
            g_producer_cancel_effect_hook.gateway, station);
    }
    {
        HybridCocoonTailSwap swapped(station);
        if (swapped.active()) {
            a2fo_call_thiscall_0(
                at(g_armada, kEvolverCancelConstructionEffectRva), station);
        }
    }
    erase_hybrid_cocoon_state(station);
    return 0;
}

std::uintptr_t __attribute__((fastcall)) producer_update_effect_hook(
    void* station, void*, std::uintptr_t progress) noexcept {
    if (!active_evolve_job(station) &&
        !hybrid_cocoon_has_instance(station)) {
        return a2fo_call_thiscall_1(
            g_producer_update_effect_hook.gateway, station, progress);
    }
    HybridCocoonTailSwap swapped(station);
    return swapped.active()
        ? a2fo_call_thiscall_1(
              at(g_armada, kEvolverUpdateConstructionEffectRva), station,
              progress)
        : 0;
}

std::uintptr_t __attribute__((fastcall)) producer_stop_effect_hook(
    void* station, void*) noexcept {
    if (!active_evolve_job(station) &&
        !hybrid_cocoon_has_instance(station)) {
        return a2fo_call_thiscall_0(
            g_producer_stop_effect_hook.gateway, station);
    }
    {
        HybridCocoonTailSwap swapped(station);
        if (swapped.active()) {
            a2fo_call_thiscall_0(
                at(g_armada, kEvolverStopConstructionEffectRva), station);
        }
    }
    erase_hybrid_cocoon_state(station);
    return 0;
}

struct QueuedConstructionGhost {
    void* target_class = nullptr;
    std::uint32_t queue_id = 0;
    std::array<float, kBuildPositionInterfaceMatrixSize / sizeof(float)>
        matrix{};
};

std::size_t collect_queued_construction_ghosts(
    void* station,
    std::array<QueuedConstructionGhost, kNativeQueueCapacity>& ghosts)
    noexcept {
    ghosts.fill({});
    if (!station || !g_registry_lock_ready ||
        InterlockedCompareExchange(
            &g_hybrid_construction_state_count, 0, 0) == 0) {
        return 0;
    }
    void* active_class = readable_range(
            bytes(station) + kCurrentBuildClassOffset, sizeof(void*))
        ? *reinterpret_cast<void**>(
              bytes(station) + kCurrentBuildClassOffset)
        : nullptr;
    const std::uint32_t active_id = target_has_explicit_method(
            station, active_class, ProductionMethod::construct)
        ? active_construction_queue_id(station)
        : 0;
    std::size_t count = 0;
    EnterCriticalSection(&g_registry_lock);
    const auto found = g_hybrid_construction.find(station);
    if (found != g_hybrid_construction.end()) {
        for (const HybridConstructionState::Placement& placement :
             found->second.placements) {
            if (!placement.target_class || !placement.build_interface ||
                placement.queue_id == active_id ||
                count >= ghosts.size() ||
                !readable_range(
                    bytes(placement.build_interface) +
                        kBuildPositionInterfaceMatrixOffset,
                    kBuildPositionInterfaceMatrixSize)) {
                continue;
            }
            QueuedConstructionGhost& ghost = ghosts[count++];
            ghost.target_class = placement.target_class;
            ghost.queue_id = placement.queue_id;
            std::memcpy(
                ghost.matrix.data(),
                bytes(placement.build_interface) +
                    kBuildPositionInterfaceMatrixOffset,
                kBuildPositionInterfaceMatrixSize);
        }
    }
    LeaveCriticalSection(&g_registry_lock);
    return count;
}

void render_queued_construction_ghosts(void* station) noexcept {
    if (!g_hybrid_construct_ghost_renderer_ready) return;
    std::array<QueuedConstructionGhost, kNativeQueueCapacity> ghosts{};
    const std::size_t count = collect_queued_construction_ghosts(
        station, ghosts);
    if (!count) return;

    struct GhostColour {
        float red;
        float green;
        float blue;
    };
    // PlaceholderFeature provides the native translucent construction ghost;
    // this warm yellow distinguishes future FIFO placements from the active
    // object's ordinary team-coloured construction placeholder.
    const GhostColour queued_colour{1.0f, 0.8f, 0.1f};
    using PlaceholderRenderFunction = void (__cdecl*)(
        const void*, const void*, const void*);
    auto render = reinterpret_cast<PlaceholderRenderFunction>(
        at(g_armada, kPlaceholderRenderInternalRva));
    std::size_t rendered = 0;
    for (std::size_t index = 0; index < count; ++index) {
        void* target_class = ghosts[index].target_class;
        if (!readable_range(target_class, sizeof(void*))) continue;
        void** vtable = *reinterpret_cast<void***>(target_class);
        if (!vtable) continue;
        void* has_geometry = readable_range(
                vtable + kClassHasGeometryVtableOffset / sizeof(void*),
                sizeof(void*))
            ? vtable[kClassHasGeometryVtableOffset / sizeof(void*)]
            : nullptr;
        if (!executable_address(has_geometry) ||
            a2fo_call_thiscall_0(has_geometry, target_class) == 0) {
            continue;
        }
        render(target_class, ghosts[index].matrix.data(), &queued_colour);
        ++rendered;
    }
    if (rendered && !g_logged_hybrid_construct_queued_ghosts) {
        g_logged_hybrid_construct_queued_ghosts = true;
        log_message("Hybrid Construction rendered queued station ghosts "
                    "in yellow while their hybrid builder is selected");
    }
}

std::uintptr_t __attribute__((fastcall)) craft_render_internal_hook(
    void* craft, void*, void* render_context) noexcept {
    const bool render_cocoon = hybrid_cocoon_has_instance(craft);
    // Popup refreshes maintain this stable object pointer until selection
    // changes. Do not retain or inspect their craft-array argument here: Fleet
    // Ops owns that transient container and may recycle it before rendering.
    const bool render_construct_ghosts = craft &&
        craft == g_last_single_hybrid_selection;
    if (g_craft_render_internal_depth != 0 ||
        (!render_cocoon && !render_construct_ghosts)) {
        return a2fo_call_thiscall_1(
            g_craft_render_internal_hook.gateway, craft,
            reinterpret_cast<std::uintptr_t>(render_context));
    }

    ++g_craft_render_internal_depth;
    std::uintptr_t result = 0;
    if (render_cocoon) {
        HybridCocoonTailSwap swapped(craft);
        result = swapped.active()
            ? a2fo_call_thiscall_1(
                  at(g_armada, kEvolverRenderInternalRva), craft,
                  reinterpret_cast<std::uintptr_t>(render_context))
            : a2fo_call_thiscall_1(
                  g_craft_render_internal_hook.gateway, craft,
                  reinterpret_cast<std::uintptr_t>(render_context));
    } else {
        result = a2fo_call_thiscall_1(
            g_craft_render_internal_hook.gateway, craft,
            reinterpret_cast<std::uintptr_t>(render_context));
    }
    if (render_construct_ghosts) {
        render_queued_construction_ghosts(craft);
    }
    --g_craft_render_internal_depth;
    return result;
}

std::uintptr_t __attribute__((fastcall)) research_start_hook(
    void* station, void*) noexcept {
    ProductionMethod method = ProductionMethod::legacy;
    const bool explicit_method = active_explicit_method(station, method);
    const bool construct = explicit_method &&
        method == ProductionMethod::construct;
    const bool yard = explicit_method && method == ProductionMethod::yard;
    const bool evolve = explicit_method && method == ProductionMethod::evolve;
    const std::uint32_t before = station && readable_range(
        bytes(station) + kQueueCountOffset, sizeof(std::uint32_t))
        ? *reinterpret_cast<const std::uint32_t*>(
              bytes(station) + kQueueCountOffset)
        : 0;
    std::uintptr_t result = 0;
    if (construct) {
        if (initialize_hybrid_construction_state(station)) {
            HybridConstructionTailSwap swapped(station);
            if (swapped.active()) {
                result = a2fo_call_thiscall_0(
                    at(g_armada, kConstructionRigStartRva), station);
            }
        }
    } else {
        result = (yard || evolve)
            ? a2fo_call_thiscall_0(
                  at(g_fleet_ops, kFoProducerStartRva), station)
            : a2fo_call_thiscall_0(
                  g_research_start_hook.gateway, station);
    }
    bool& logged_start = construct ? g_logged_hybrid_construct_start
        : evolve ? g_logged_hybrid_evolve_start
        : yard ? g_logged_hybrid_yard_start
               : g_logged_hybrid_research_start;
    if (!logged_start && station && readable_range(
            bytes(station) + kObjectClassOffset, sizeof(void*))) {
        void* station_class = *reinterpret_cast<void**>(
            bytes(station) + kObjectClassOffset);
        if (runtime_lists_for_class(station_class)) {
            logged_start = true;
            const std::uint32_t after = readable_range(
                bytes(station) + kQueueCountOffset, sizeof(std::uint32_t))
                ? *reinterpret_cast<const std::uint32_t*>(
                      bytes(station) + kQueueCountOffset)
                : 0;
            void* active_class = readable_range(
                bytes(station) + kCurrentBuildClassOffset, sizeof(void*))
                ? *reinterpret_cast<void**>(
                      bytes(station) + kCurrentBuildClassOffset)
                : nullptr;
            const char* route = construct
                ? "scoped ConstructionRig identity plus protected placement "
                  "sidecar"
                : yard ? "yard Producer"
                : evolve ? "evolve Producer plus safe swap"
                : explicit_method ? "explicit non-yard ResearchStation"
                                  : "unresolved ResearchStation";
            char message[224];
            std::snprintf(
                message, sizeof(message),
                "First hybrid %s StartBuild route: %s, count %lu -> %lu, "
                "active %s, result %lu",
                construct ? "construct"
                    : evolve ? "evolve" : yard ? "yard" : "research",
                route,
                static_cast<unsigned long>(before),
                static_cast<unsigned long>(after),
                active_class ? "set" : "clear",
                static_cast<unsigned long>(result));
            log_message(message);
        }
    }
    return result;
}

std::uintptr_t __attribute__((fastcall)) research_cancel_hook(
    void* station, void*) noexcept {
    if (active_construct_job(station)) {
        const std::uint32_t queue_id =
            active_construction_queue_id(station);
        std::uintptr_t cancelled = 0;
        {
            HybridConstructionTailSwap swapped(station);
            cancelled = swapped.active()
                ? a2fo_call_thiscall_0(
                      at(g_armada, kConstructionRigCancelRva), station)
                : a2fo_call_thiscall_0(
                      at(g_fleet_ops, kFoProducerCancelRva), station);
        }
        if (cancelled) release_construct_placement(station, queue_id);
        return cancelled;
    }
    if (active_yard_job(station) || active_evolve_job(station)) {
        return a2fo_call_thiscall_0(
            at(g_fleet_ops, kFoProducerCancelRva), station);
    }
    return a2fo_call_thiscall_0(g_research_cancel_hook.gateway, station);
}

std::uintptr_t __attribute__((fastcall)) research_can_build_hook(
    void* station, void*) noexcept {
    if (active_construct_job(station) || active_yard_job(station) ||
        active_evolve_job(station)) {
        return 1;
    }
    return a2fo_call_thiscall_0(
        g_research_can_build_hook.gateway, station);
}

std::uintptr_t __attribute__((fastcall)) research_item_conflict_hook(
    void* station, void*, void* target_class) noexcept {
    if (hybrid_production_has_queued_research_conflict(
            station, target_class)) {
        if (!g_logged_queued_research_conflict) {
            g_logged_queued_research_conflict = true;
            log_message("Hybrid ResearchStation rejected a queued research "
                        "pod conflict");
        }
        return 0;
    }
    return a2fo_call_thiscall_1(
        g_research_item_conflict_hook.gateway, station,
        reinterpret_cast<std::uintptr_t>(target_class));
}

std::uintptr_t __attribute__((fastcall)) research_matrix_hook(
    void* station, void*, void* result) noexcept {
    if (active_construct_job(station)) {
        if (!initialize_hybrid_construction_state(station)) return 0;
        HybridConstructionTailSwap swapped(station);
        return swapped.active()
            ? a2fo_call_thiscall_1(
                  at(g_armada, kConstructionRigConstructionMatrixRva),
                  station, reinterpret_cast<std::uintptr_t>(result))
            : 0;
    }
    if (active_evolve_job(station)) {
        // Evolver::GetConstructionMatrix is only Entity::GetTransform plus a
        // value copy; it contains no Evolver-tail access and preserves the
        // replacement object's exact position and orientation.
        return a2fo_call_thiscall_1(
            at(g_armada, kEvolverConstructionMatrixRva), station,
            reinterpret_cast<std::uintptr_t>(result));
    }
    if (active_yard_job(station)) {
        return a2fo_call_thiscall_1(
            at(g_armada, kProducerConstructionMatrixRva), station,
            reinterpret_cast<std::uintptr_t>(result));
    }
    return a2fo_call_thiscall_1(
        g_research_matrix_hook.gateway, station,
        reinterpret_cast<std::uintptr_t>(result));
}

std::uintptr_t __attribute__((fastcall)) research_finish_hook(
    void* station, void*) noexcept {
    if (active_construct_job(station)) {
        const std::uint32_t queue_id =
            active_construction_queue_id(station);
        std::uintptr_t completed = 0;
        {
            HybridConstructionTailSwap swapped(station);
            if (!swapped.active()) return 0;
            completed = a2fo_call_thiscall_0(
                at(g_armada, kConstructionRigFinishRva), station);
        }
        if (completed) release_construct_placement(station, queue_id);
        if (completed && !g_logged_hybrid_construct_finish) {
            g_logged_hybrid_construct_finish = true;
            log_message("Hybrid Construction completed through the native "
                        "ConstructionRig station handoff");
        }
        return completed;
    }
    if (active_evolve_job(station)) {
        void* replacement = reinterpret_cast<void*>(a2fo_call_thiscall_0(
            at(g_fleet_ops, kFoProducerFinishRva), station));
        if (!replacement) return 0;

        // Evolver::mSwapObjects uses only common Craft/Producer fields: name,
        // selection, overview relationship, race, and owner. Construction
        // effects and their overlapping +0x2ac tail remain bypassed.
        a2fo_call_thiscall_1(
            at(g_armada, kEvolverSwapObjectsRva), station,
            reinterpret_cast<std::uintptr_t>(replacement));

        // Producer normally stops the effect immediately before FinishBuild.
        // Keep an explicit cleanup here as well so exceptional/zero-duration
        // paths cannot carry a cocoon sidecar into station destruction.
        cleanup_hybrid_cocoon(station);

        void** debriefing_link = at<void*>(
            g_armada, kDebriefingDataGlobalRva);
        if (readable_range(debriefing_link, sizeof(void*)) &&
            *debriefing_link) {
            a2fo_call_thiscall_2(
                at(g_armada, kDebriefingDestroyShipRva), *debriefing_link,
                reinterpret_cast<std::uintptr_t>(station), 1);
        }

        // Match Evolver::FinishBuild's final virtual removal call. This is
        // intentionally last: the hybrid station is invalid afterwards.
        if (readable_range(station, sizeof(void*))) {
            void** vtable = *reinterpret_cast<void***>(station);
            if (readable_range(vtable + 1, sizeof(void*)) && vtable[1]) {
                a2fo_call_thiscall_0(vtable[1], station);
            }
        }
        if (!g_logged_hybrid_evolve_finish) {
            g_logged_hybrid_evolve_finish = true;
            log_message("Hybrid evolution completed through Producer finish "
                        "and the safe Evolver replacement handoff");
        }
        return reinterpret_cast<std::uintptr_t>(replacement);
    }
    if (active_yard_job(station)) {
        return a2fo_call_thiscall_0(
            at(g_fleet_ops, kFoProducerFinishRva), station);
    }
    return a2fo_call_thiscall_0(g_research_finish_hook.gateway, station);
}

std::uint32_t collect_hybrid_build_queue(
    void* producer,
    std::array<void*, kNativeQueueCapacity>& queue) noexcept {
    queue.fill(nullptr);
    if (!active_hybrid_queue(producer)) return 0;

    void* active_class = *reinterpret_cast<void**>(
        bytes(producer) + kCurrentBuildClassOffset);
    const std::uint32_t active_id = readable_range(
        bytes(producer) + kCurrentQueueIdOffset, sizeof(std::uint32_t))
        ? *reinterpret_cast<const std::uint32_t*>(
              bytes(producer) + kCurrentQueueIdOffset)
        : 0;
    std::uint32_t populated = 0;
    if (active_class) queue[populated++] = active_class;

    void* queue_item = *reinterpret_cast<void**>(
        bytes(producer) + kQueueHeadOffset);
    while (queue_item && populated < kNativeQueueCapacity) {
        if (!readable_range(
                queue_item, kQueueItemIdOffset + sizeof(std::uint32_t))) {
            break;
        }
        void* queued_class = *reinterpret_cast<void**>(queue_item);
        const std::uint32_t queued_id =
            *reinterpret_cast<const std::uint32_t*>(
                bytes(queue_item) + kQueueItemIdOffset);
        if (!active_class || queued_id != active_id) {
            queue[populated++] = queued_class;
        }
        queue_item = *reinterpret_cast<void**>(
            bytes(queue_item) + kQueueItemNextOffset);
    }
    return populated;
}

void __attribute__((fastcall)) race_icon_render_hook(
    void* race_icon, void*) noexcept {
    void* selected = race_icon && readable_range(
        bytes(race_icon) + kRaceIconObjectOffset, sizeof(void*))
        ? *reinterpret_cast<void**>(
              bytes(race_icon) + kRaceIconObjectOffset)
        : nullptr;

    std::array<void*, kNativeQueueCapacity> queue{};
    if (selected && collect_hybrid_build_queue(selected, queue) ==
            kNativeQueueCapacity) {
        // Native ShipDisplay suppresses RaceIcon when a builder's tenth queue
        // slot is occupied. A hybrid ResearchStation remains on the ordinary
        // single-object path, so apply that exact policy at the icon itself.
        if (!g_logged_hybrid_full_queue_icon_suppressed) {
            g_logged_hybrid_full_queue_icon_suppressed = true;
            log_message("Hybrid ten-item queue suppressed RaceIcon behind "
                        "the final BuildWireframe slot");
        }
        return;
    }
    a2fo_call_thiscall_0(g_race_icon_render_hook.gateway, race_icon);
}

struct HoverRectangle {
    std::int32_t left;
    std::int32_t top;
    std::int32_t right;
    std::int32_t bottom;
};

struct SpriteVector {
    float x;
    float y;
    float z;
};

bool research_single_sprite_name(
    void* target_class, std::array<char, 260>& name) noexcept {
    name.fill('\0');
    if (!target_class || !readable_range(
            bytes(target_class) + kClassBaseNameOffset, sizeof(void*))) {
        return false;
    }
    const char* basename = *reinterpret_cast<const char* const*>(
        bytes(target_class) + kClassBaseNameOffset);
    if (!basename) return false;
    for (std::size_t index = 0; index + 3 < name.size(); ++index) {
        if (!readable_range(basename + index, sizeof(char))) return false;
        name[index] = basename[index];
        if (name[index] == '\0') {
            name[index] = '_';
            name[index + 1] = 's';
            name[index + 2] = '\0';
            return index != 0;
        }
    }
    return false;
}

bool draw_research_hover_wireframe(
    void* target_class, std::uintptr_t scale_bits,
    void* rectangle) noexcept {
    if (!rectangle || !readable_range(
            rectangle, sizeof(HoverRectangle))) {
        return false;
    }

    std::array<char, 260> sprite_name{};
    if (!research_single_sprite_name(target_class, sprite_name)) return false;

    void* database = nullptr;
    void** database_link = at<void*>(
        g_fleet_ops, kFoSpriteDatabaseGlobalRva);
    if (!readable_range(database_link, sizeof(void*))) return false;
    database = *database_link;
    for (unsigned dereference = 0; dereference < 2; ++dereference) {
        if (!readable_range(database, sizeof(void*))) return false;
        database = *reinterpret_cast<void**>(database);
    }
    if (!database) return false;

    void* sprite = a2fo_fo_database_find_element(
        at(g_fleet_ops, kFoDatabaseFindElementRva), database,
        sprite_name.data());
    if (!sprite) return false;

    float scale = 0.0f;
    const std::uint32_t packed_scale =
        static_cast<std::uint32_t>(scale_bits);
    std::memcpy(&scale, &packed_scale, sizeof(scale));
    const SpriteVector colour{scale, scale, scale};
    const auto* bounds = static_cast<const HoverRectangle*>(rectangle);
    const SpriteVector position{
        static_cast<float>(bounds->left - 35),
        static_cast<float>(bounds->bottom + 5), 0.0f};

    void** dimension_link = at<void*>(
        g_fleet_ops, kFoScreenDimensionGlobalRva);
    if (!readable_range(dimension_link, sizeof(void*)) ||
        !readable_range(*dimension_link, sizeof(std::int32_t))) {
        return false;
    }
    const float display_dimension = static_cast<float>(
        *static_cast<const std::int32_t*>(*dimension_link));
    a2fo_fo_sprite_set_colour(
        at(g_fleet_ops, kFoSpriteSetColourRva), sprite, &colour);
    a2fo_fo_sprite_draw_scaled_2d(
        at(g_fleet_ops, kFoSpriteDrawScaled2DRva), sprite, &position,
        display_dimension, display_dimension);
    if (!g_logged_hybrid_research_hover_wireframe) {
        g_logged_hybrid_research_hover_wireframe = true;
        char message[352];
        std::snprintf(
            message, sizeof(message),
            "Hybrid research hover preview drew single sprite '%s'",
            sprite_name.data());
        log_message(message);
    }
    return true;
}

void __attribute__((regparm(2), stdcall))
selection_display_draw_producer_wireframe_hook(
    void* selection_display, void* selected, std::uintptr_t scale,
    void* rectangle) noexcept {
    void** queue_head = selected && writable_range(
        bytes(selected) + kQueueHeadOffset, sizeof(void*))
        ? reinterpret_cast<void**>(bytes(selected) + kQueueHeadOffset)
        : nullptr;
    void* active_class = selected && readable_range(
        bytes(selected) + kCurrentBuildClassOffset, sizeof(void*))
        ? *reinterpret_cast<void**>(
              bytes(selected) + kCurrentBuildClassOffset)
        : nullptr;
    void* saved_head = nullptr;
    const bool research_preview = queue_head && active_class &&
        hybrid_research_target(selected, active_class);
    if (research_preview) {
        saved_head = *queue_head;
        // Do not let Fleet Ops draw the next queued yard craft while research
        // is active. Its normal preview searches only w1..w5 layers; the pod's
        // native queue/hover asset is the single basename_s sprite drawn below.
        *queue_head = nullptr;
    }

    auto original = reinterpret_cast<DelphiRegister2Stack2Function>(at(
        g_fleet_ops, kFoSelectionDisplayDrawProducerWireframeRva));
    original(selection_display, selected, scale, rectangle);
    if (research_preview) {
        *queue_head = saved_head;
        draw_research_hover_wireframe(active_class, scale, rectangle);
    }
}

void update_hybrid_build_queue_wireframes(void* ship_display,
                                          bool render) noexcept {
    if (!ship_display || !readable_range(
            bytes(ship_display) + kShipDisplaySelectedObjectOffset,
            sizeof(void*)) || !readable_range(
            bytes(ship_display) + kShipDisplayBuildQueueOffset,
            kNativeQueueCapacity * sizeof(void*))) {
        return;
    }
    void* selected = *reinterpret_cast<void**>(
        bytes(ship_display) + kShipDisplaySelectedObjectOffset);
    if (!active_hybrid_queue(selected)) return;

    // ResearchStation can move the active research pod out of the linked FIFO
    // while retaining it in Producer::currentBuildClass. Project that active
    // item first, then append the inherited FIFO used by queue/cancel/save/load.
    // Generic Producer builds normally remain at the FIFO head, so compare the
    // queue IDs and suppress only that exact active-node duplicate.
    std::array<void*, kNativeQueueCapacity> queue{};
    void* active_class = *reinterpret_cast<void**>(
        bytes(selected) + kCurrentBuildClassOffset);
    const std::uint32_t populated =
        collect_hybrid_build_queue(selected, queue);

    for (std::uint32_t index = 0; index < kNativeQueueCapacity; ++index) {
        void* wireframe = *reinterpret_cast<void**>(
            bytes(ship_display) + kShipDisplayBuildQueueOffset +
            index * sizeof(void*));
        if (!wireframe || !writable_range(
                bytes(wireframe) + kBuildWireframeObjectOffset,
                kBuildWireframeTargetClassOffset -
                    kBuildWireframeObjectOffset +
                    sizeof(void*)) || !readable_range(wireframe,
                                                       sizeof(void*))) {
            continue;
        }
        *reinterpret_cast<void**>(
            bytes(wireframe) + kBuildWireframeObjectOffset) = selected;
        *reinterpret_cast<void**>(
            bytes(wireframe) + kBuildWireframeTargetClassOffset) =
            queue[index];

        void** vtable = *reinterpret_cast<void***>(wireframe);
        const std::size_t method_offset = render ? 0x10 : 0x0c;
        if (!vtable || !readable_range(bytes(vtable) + method_offset,
                                       sizeof(void*))) {
            continue;
        }
        void* method = *reinterpret_cast<void**>(bytes(vtable) + method_offset);
        if (queue[index] && !g_logged_hybrid_queue_slot_binding) {
            g_logged_hybrid_queue_slot_binding = true;
            char message[192];
            std::snprintf(
                message, sizeof(message),
                "First Fleet Ops hybrid BuildWireframe binding: class %p, "
                "slot %p, %s method %p",
                queue[index],
                wireframe, render ? "render" : "simulate", method);
            log_message(message);
        }
        if (method) {
            std::uint32_t saved_flags = 0;
            bool force_single_sprite = false;
            if (render && queue[index] && readable_range(
                    bytes(selected) + kObjectFlagsOffset,
                    sizeof(saved_flags)) && writable_range(
                    bytes(selected) + kObjectFlagsOffset,
                    sizeof(saved_flags))) {
                void* producer_class = *reinterpret_cast<void**>(
                    bytes(selected) + kObjectClassOffset);
                std::uint32_t target_project_id = 0;
                ProductionMethod target_method = ProductionMethod::legacy;
                const bool explicit_target = class_project_id(
                    queue[index], target_project_id) &&
                    resolve_explicit_production_method(
                        producer_class, target_project_id, target_method);
                force_single_sprite = !explicit_target ||
                    target_method == ProductionMethod::research;
                if (force_single_sprite) {
                    auto* flags = reinterpret_cast<std::uint32_t*>(
                        bytes(selected) + kObjectFlagsOffset);
                    saved_flags = *flags;
                    *flags = saved_flags | kSingleSpriteWireframeFlag;
                    if (!g_logged_hybrid_research_queue_sprite) {
                        g_logged_hybrid_research_queue_sprite = true;
                        log_message("Hybrid research queue entries use their "
                                    "native _s wireframe sprites");
                    }
                }
            }
            a2fo_call_thiscall_0(method, wireframe);
            if (force_single_sprite) {
                *reinterpret_cast<std::uint32_t*>(
                    bytes(selected) + kObjectFlagsOffset) = saved_flags;
            }
        }
    }

    if (!g_logged_hybrid_queue_wireframes) {
        g_logged_hybrid_queue_wireframes = true;
        const std::uint32_t count = *reinterpret_cast<const std::uint32_t*>(
            bytes(selected) + kQueueCountOffset);
        char message[192];
        std::snprintf(
            message, sizeof(message),
            "Hybrid production bound ShipDisplay+0x120 BuildWireframes to "
            "active job plus shared Producer FIFO: count %lu, active %s, "
            "populated %lu",
            static_cast<unsigned long>(count),
            active_class ? "set" : "clear",
            static_cast<unsigned long>(populated));
        log_message(message);
    }
}

}  // namespace

void cleanup_hybrid_cocoon(void* station) noexcept {
    remove_hybrid_cocoon_effect(station, true);
}

void cleanup_hybrid_construction(void* station) noexcept {
    remove_hybrid_construction_state(station);
}

bool hybrid_production_is_evolve_target(
    void* producer, void* target_class) noexcept {
    return target_has_explicit_method(
        producer, target_class, ProductionMethod::evolve);
}

bool hybrid_production_has_evolution_barrier(void* producer) noexcept {
    return has_evolution_barrier(producer);
}

bool hybrid_production_should_defer_construct_order(
    void* producer, void* target_class) noexcept {
    const bool defer = producer && target_class &&
        producer == g_hybrid_construct_placement_press_station &&
        target_has_explicit_method(
            producer, target_class, ProductionMethod::construct);
    if (defer && !g_logged_hybrid_construct_premature_order) {
        g_logged_hybrid_construct_premature_order = true;
        log_message("Hybrid Construction blocked a premature button-order; "
                    "the confirmed map placement will issue the build order");
    }
    return defer;
}

void finalize_hybrid_construct_order(
    void* producer, void* target_class, bool admitted) noexcept {
    bind_pending_construct_placement(producer, target_class, admitted);
}

void discard_hybrid_construct_placement(
    void* producer, std::uint32_t queue_id) noexcept {
    release_construct_placement(producer, queue_id);
}

void clear_hybrid_construct_placements(void* producer) noexcept {
    clear_construct_placements(producer);
}

bool hybrid_production_has_queued_research_conflict(
    void* producer, void* target_class) noexcept {
    if (!hybrid_research_target(producer, target_class) ||
        !readable_range(bytes(producer) + kCurrentBuildClassOffset,
                        sizeof(void*)) ||
        !readable_range(bytes(producer) + kQueueHeadOffset,
                        sizeof(void*))) {
        return false;
    }

    auto conflicts_with_target = [producer, target_class](
                                     void* existing_class) noexcept {
        if (!existing_class) return false;
        if (existing_class == target_class) return true;
        return hybrid_research_target(producer, existing_class) &&
            research_classes_conflict(existing_class, target_class);
    };

    void* active_class = *reinterpret_cast<void**>(
        bytes(producer) + kCurrentBuildClassOffset);
    if (conflicts_with_target(active_class)) return true;

    void* queue_item = *reinterpret_cast<void**>(
        bytes(producer) + kQueueHeadOffset);
    for (std::uint32_t visited = 0;
         queue_item && visited < kNativeQueueCapacity; ++visited) {
        if (!readable_range(
                queue_item, kQueueItemNextOffset + sizeof(void*))) {
            break;
        }
        void* queued_class = *reinterpret_cast<void**>(queue_item);
        if (conflicts_with_target(queued_class)) return true;
        queue_item = *reinterpret_cast<void**>(
            bytes(queue_item) + kQueueItemNextOffset);
    }
    return false;
}

void retain_hybrid_research_menu_after_order(
    void* producer, void* target_class) noexcept {
    if (!hybrid_research_target(producer, target_class)) return;
    if (g_control_button_press_depth != 0) {
        // The popup may refresh dozens of times inside one click. Keep a
        // separate press-scoped marker which those refreshes cannot consume;
        // the outer ControlButton callback clears it after its final post-pass.
        g_post_press_research_menu_station = producer;
    }
    g_retain_research_menu_station = producer;
    g_retain_research_menu_refreshes = kRetainResearchMenuRefreshLimit;
    g_retain_research_menu_saw_root = false;
    if (!g_logged_research_menu_retention_armed) {
        g_logged_research_menu_retention_armed = true;
        log_message("Hybrid ResearchStation armed Research palette retention "
                    "for a pod order");
    }
}

// The assembly dispatchers call Fleet Ops first, preserve its result register
// state, and then invoke this ordinary cdecl post-pass.
extern "C" void __cdecl a2fo_hybrid_queue_post_dispatch(
    void* ship_display, std::uintptr_t render) noexcept {
    update_hybrid_build_queue_wireframes(ship_display, render != 0);
}

extern "C" {
void* a2fo_fo_single_object_display_target = nullptr;
void* a2fo_fo_single_object_simulate_target = nullptr;
void a2fo_ship_display_single_object_display_dispatch();
void a2fo_ship_display_single_object_simulate_dispatch();
}

bool initialize_hybrid_production_registry(const A2FO_ModuleApi* api,
                                           HMODULE armada,
                                           HMODULE fleet_ops) noexcept {
    if (!api || !api->install_inline_hook || !api->patch_call || !armada ||
        !fleet_ops) {
        return false;
    }
    g_api = api;
    g_armada = armada;
    g_fleet_ops = fleet_ops;
    // Queued ghosts are presentation-only. Validate their native renderer
    // independently so a preview incompatibility can never disable the
    // hybridbuild classlabel or its production menus.
    g_hybrid_construct_ghost_renderer_ready = executable_address(
            at(armada, kPlaceholderRenderInternalRva)) &&
        signature_matches(armada, kPlaceholderRenderInternalRva,
                          kExpectedPlaceholderRenderInternal);
    if (!g_hybrid_construct_ghost_renderer_ready) {
        log_message("Hybrid Construction queued ghost renderer unavailable; "
                    "queued ghost previews disabled");
    }
    a2fo_fo_single_object_display_target =
        at(fleet_ops, kFoShipDisplaySingleObjectDisplayRva);
    a2fo_fo_single_object_simulate_target =
        at(fleet_ops, kFoShipDisplaySingleObjectSimulateRva);

    // ParameterDB::GetString is already detoured by the core before native
    // modules load. Calling that public entry is intentional: it preserves the
    // core's semantic ODF policies. Do not compare it with the on-disk prologue
    // here or every real game run disables the hybrid parser while isolated
    // smoke fixtures misleadingly pass.
    bool signatures_match = true;
    signatures_match &= require_signature(
        "ParameterDB::GetProjectId", armada, kParameterDbGetProjectIdRva,
        kExpectedParameterDbGetProjectId);
    signatures_match &= require_signature(
        "GameObjectClass::Find(cPrjID)", armada,
        kGameObjectClassFindProjectIdRva,
        kExpectedGameObjectClassFindProjectId);
    signatures_match &= require_signature(
        "Producer::GetAction", armada, kProducerGetActionRva,
        kExpectedProducerGetAction);
    signatures_match &= require_signature(
        "ConstructionRig::GetAction", armada,
        kConstructionRigGetActionRva,
        kExpectedConstructionRigGetAction);
    signatures_match &= require_signature(
        "ConstructionRig::StartBuild", armada,
        kConstructionRigStartRva, kExpectedConstructionRigStart);
    signatures_match &= require_signature(
        "ConstructionRig::CancelBuild", armada,
        kConstructionRigCancelRva, kExpectedConstructionRigCancel);
    signatures_match &= require_signature(
        "ConstructionRig::FinishBuild", armada,
        kConstructionRigFinishRva, kExpectedConstructionRigFinish);
    signatures_match &= require_signature(
        "ConstructionRig::RemoveConstructionObject", armada,
        kConstructionRigRemoveObjectRva,
        kExpectedConstructionRigRemoveObject);
    signatures_match &= require_signature(
        "ConstructionRig::GetConstructionMatrix", armada,
        kConstructionRigConstructionMatrixRva,
        kExpectedConstructionRigConstructionMatrix);
    signatures_match &= require_signature(
        "BuildPositionInterface constructor", armada,
        kBuildPositionInterfaceConstructorRva,
        kExpectedBuildPositionInterfaceConstructor);
    signatures_match &= require_signature(
        "BuildPositionInterface destructor", armada,
        kBuildPositionInterfaceDestructorRva,
        kExpectedBuildPositionInterfaceDestructor);
    // Fleet Ops may already detour Armada's public allocation entry points by
    // the time native feature modules load. Calling those public entries is
    // intentional because the ConstructionRig-compatible sidecar must use
    // the game's active heap policy; their original on-disk prologues are not
    // an initialization requirement.
    signatures_match &= require_signature(
        "Producer::IsBusy", armada, kProducerIsBusyRva,
        kExpectedProducerIsBusy);
    signatures_match &= require_signature(
        "CraftProcess build-command cleanup IsBusy query", armada,
        kBuildCommandCleanupIsBusyQueryRva,
        kExpectedBuildCommandCleanupIsBusyQuery);
    signatures_match &= require_signature(
        "CraftProcess build-command admission IsBusy query", armada,
        kBuildCommandAdmitIsBusyQueryRva,
        kExpectedBuildCommandAdmitIsBusyQuery);
    signatures_match &= require_signature(
        "CraftProcess build-command cleanup queue pop", armada,
        kBuildCommandCleanupPopQueryRva,
        kExpectedBuildCommandCleanupPopQuery);
    signatures_match &= require_signature(
        "CraftProcess build-command replacement queue pop", armada,
        kBuildCommandReplacePopQueryRva,
        kExpectedBuildCommandReplacePopQuery);
    signatures_match &= require_signature(
        "CraftProcess ConstructionRig placement-interface load", armada,
        kBuildCommandPositionInterfaceLoadRva,
        kExpectedBuildCommandPositionInterfaceLoad);
    signatures_match &= require_signature(
        "Producer::GetConstructionMatrix", armada,
        kProducerConstructionMatrixRva,
        kExpectedProducerConstructionMatrix);
    signatures_match &= require_signature(
        "Producer::mStartConstructionEffect", armada,
        kProducerStartConstructionEffectRva,
        kExpectedProducerStartConstructionEffect);
    signatures_match &= require_signature(
        "Producer::mCancelConstructionEffect", armada,
        kProducerCancelConstructionEffectRva,
        kExpectedProducerCancelConstructionEffect);
    signatures_match &= require_signature(
        "Producer::mUpdateConstructionEffect", armada,
        kProducerUpdateConstructionEffectRva,
        kExpectedProducerUpdateConstructionEffect);
    signatures_match &= require_signature(
        "Producer::mStopConstructionEffect", armada,
        kProducerStopConstructionEffectRva,
        kExpectedProducerStopConstructionEffect);
    signatures_match &= require_signature(
        "ResearchStation::StartBuild", armada,
        kResearchStationStartRva, kExpectedResearchStationStart);
    signatures_match &= require_signature(
        "ResearchStation::CancelBuild", armada,
        kResearchStationCancelRva, kExpectedResearchStationCancel);
    signatures_match &= require_signature(
        "ResearchStation::CanBuild", armada,
        kResearchStationCanBuildRva, kExpectedResearchStationCanBuild);
    signatures_match &= require_signature(
        "ResearchStation queued-item conflict check", armada,
        kResearchStationItemConflictRva,
        kExpectedResearchStationItemConflict);
    signatures_match &= require_signature(
        "ResearchStation::GetConstructionMatrix", armada,
        kResearchStationConstructionMatrixRva,
        kExpectedResearchStationConstructionMatrix);
    signatures_match &= require_signature(
        "Evolver::mSwapObjects", armada,
        kEvolverSwapObjectsRva, kExpectedEvolverSwapObjects);
    signatures_match &= require_signature(
        "Evolver::GetConstructionMatrix", armada,
        kEvolverConstructionMatrixRva,
        kExpectedEvolverConstructionMatrix);
    signatures_match &= require_signature(
        "Evolver::mStartConstructionEffect", armada,
        kEvolverStartConstructionEffectRva,
        kExpectedEvolverStartConstructionEffect);
    signatures_match &= require_signature(
        "Evolver::mDoRemoveConstructionEffect", armada,
        kEvolverDoRemoveConstructionEffectRva,
        kExpectedEvolverDoRemoveConstructionEffect);
    signatures_match &= require_signature(
        "Evolver::mCancelConstructionEffect", armada,
        kEvolverCancelConstructionEffectRva,
        kExpectedEvolverCancelConstructionEffect);
    signatures_match &= require_signature(
        "Evolver::mStopConstructionEffect", armada,
        kEvolverStopConstructionEffectRva,
        kExpectedEvolverStopConstructionEffect);
    signatures_match &= require_signature(
        "Evolver::mUpdateConstructionEffect", armada,
        kEvolverUpdateConstructionEffectRva,
        kExpectedEvolverUpdateConstructionEffect);
    signatures_match &= require_signature(
        "Evolver::RenderInternal", armada,
        kEvolverRenderInternalRva,
        kExpectedEvolverRenderInternal);
    signatures_match &= require_signature(
        "FleetOps Craft::RenderInternal callback", fleet_ops,
        kFoCraftRenderInternalCallbackRva,
        kExpectedFoCraftRenderInternalCallback);
    // Fleet Ops detours DebriefingData::DestroyShip before native modules
    // load. Calling that public entry is intentional so its replacement keeps
    // Fleet Ops statistics/bookkeeping; unlike functions we hook ourselves,
    // its on-disk prologue must not be required here.
    signatures_match &= require_signature(
        "ControlButton::mButtonPressFunction", armada,
        kControlButtonPressRva, kExpectedControlButtonPress);
    signatures_match &= require_signature(
        "ModeInfo Build-button sprite-name builder", armada,
        kModeInfoBuildButtonNameRva,
        kExpectedModeInfoBuildButtonName);
    signatures_match &= require_signature(
        "RaceIcon::Render", armada,
        kRaceIconRenderRva, kExpectedRaceIconRender);
    signatures_match &= require_signature(
        "Fleet Ops-patched ShipDisplay single-object display call", armada,
        kShipDisplaySingleObjectDisplayCallRva,
        kExpectedShipDisplaySingleObjectDisplayCall);
    signatures_match &= require_signature(
        "Fleet Ops-patched ShipDisplay single-object simulate call", armada,
        kShipDisplaySingleObjectSimulateCallRva,
        kExpectedShipDisplaySingleObjectSimulateCall);
    signatures_match &= require_signature(
        "FleetOps Producer start", fleet_ops,
        kFoProducerStartRva, kExpectedFoProducerStart);
    // A2FOFeaturePack loads first and may already have wrapped these two public
    // callbacks for continuous-queue bookkeeping. Hybrid jobs intentionally
    // call the active entries so both modules' policies compose; their original
    // prologues are therefore not HybridBuild initialization requirements.
    signatures_match &= require_signature(
        "FleetOps checked Producer queue pop", fleet_ops,
        kFoProducerPopCheckedRva, kExpectedFoProducerPopChecked);
    signatures_match &= require_signature(
        "FleetOps ResearchStation finish", fleet_ops,
        kFoResearchStationFinishRva,
        kExpectedFoResearchStationFinish);
    signatures_match &= require_signature(
        "FleetOps popup button update", fleet_ops,
        kFoPopupUpdateButtonsRva, kExpectedFoPopupUpdateButtons);
    signatures_match &= require_signature(
        "FleetOps Build palette bind call", fleet_ops,
        kFoPopupBuildButtonBindCallRva,
        kExpectedFoPopupBuildButtonBindCall);
    signatures_match &= require_signature(
        "FleetOps Evolve palette bind call", fleet_ops,
        kFoPopupEvolveButtonBindCallRva,
        kExpectedFoPopupEvolveButtonBindCall);
    signatures_match &= require_signature(
        "FleetOps AI palette bind call", fleet_ops,
        kFoPopupAiButtonBindCallRva,
        kExpectedFoPopupAiButtonBindCall);
    signatures_match &= require_signature(
        "FleetOps producer hover-wireframe call", fleet_ops,
        kFoSelectionDisplayDrawProducerWireframeCallRva,
        kExpectedFoDrawProducerWireframeCall);
    void** object_button_press_slot = at<void*>(
        fleet_ops, kFoObjectControlButtonPressVtableSlotRva);
    if (!readable_range(object_button_press_slot, sizeof(void*)) ||
        !writable_range(object_button_press_slot, sizeof(void*)) ||
        *object_button_press_slot !=
            at(fleet_ops, kFoObjectControlButtonPressRva)) {
        signatures_match = false;
        log_message("Hybrid production signature mismatch: FleetOps "
                    "ObjectControlButton press vtable slot");
    }
    if (!signatures_match) {
        log_message("Hybrid production parser/runtime signatures mismatch; "
                    "disabled");
        return false;
    }

    InitializeCriticalSection(&g_registry_lock);
    g_registry_lock_ready = true;
    try {
        g_class_lists.reserve(128);
        g_runtime_class_lists.reserve(128);
        g_hybrid_cocoons.reserve(32);
        g_hybrid_construction.reserve(32);
        g_hybrid_get_action_originals.reserve(8);
    } catch (...) {
        log_message("Hybrid production registry allocation failed; disabled");
        return false;
    }

    a2fo_hybrid_build_position_load_continue = at(
        armada, kBuildCommandPositionInterfaceContinueRva);
    const bool installed = api->install_inline_hook(
            at(armada, kProducerGetActionRva),
            reinterpret_cast<void*>(&producer_get_action_hook),
            sizeof(kExpectedProducerGetAction),
            kExpectedProducerGetAction,
            &g_producer_get_action_hook) &&
        api->install_inline_hook(
            at(armada, kBuildCommandPositionInterfaceLoadRva),
            reinterpret_cast<void*>(
                &a2fo_hybrid_build_position_load_dispatch),
            sizeof(kExpectedBuildCommandPositionInterfaceLoad),
            kExpectedBuildCommandPositionInterfaceLoad,
            &g_build_position_interface_load_hook) &&
        api->install_inline_hook(
            at(armada, kProducerStartConstructionEffectRva),
            reinterpret_cast<void*>(&producer_start_effect_hook),
            sizeof(kExpectedProducerStartConstructionEffect),
            kExpectedProducerStartConstructionEffect,
            &g_producer_start_effect_hook) &&
        api->install_inline_hook(
            at(armada, kProducerCancelConstructionEffectRva),
            reinterpret_cast<void*>(&producer_cancel_effect_hook),
            sizeof(kExpectedProducerCancelConstructionEffect),
            kExpectedProducerCancelConstructionEffect,
            &g_producer_cancel_effect_hook) &&
        api->install_inline_hook(
            at(armada, kProducerUpdateConstructionEffectRva),
            reinterpret_cast<void*>(&producer_update_effect_hook),
            sizeof(kExpectedProducerUpdateConstructionEffect),
            kExpectedProducerUpdateConstructionEffect,
            &g_producer_update_effect_hook) &&
        api->install_inline_hook(
            at(armada, kProducerStopConstructionEffectRva),
            reinterpret_cast<void*>(&producer_stop_effect_hook),
            sizeof(kExpectedProducerStopConstructionEffect),
            kExpectedProducerStopConstructionEffect,
            &g_producer_stop_effect_hook) &&
        api->install_inline_hook(
            at(fleet_ops, kFoCraftRenderInternalCallbackRva),
            reinterpret_cast<void*>(&craft_render_internal_hook),
            sizeof(kExpectedFoCraftRenderInternalCallback),
            kExpectedFoCraftRenderInternalCallback,
            &g_craft_render_internal_hook) &&
        api->install_inline_hook(
            at(armada, kResearchStationStartRva),
            reinterpret_cast<void*>(&research_start_hook),
            sizeof(kExpectedResearchStationStart),
            kExpectedResearchStationStart, &g_research_start_hook) &&
        api->install_inline_hook(
            at(armada, kResearchStationCancelRva),
            reinterpret_cast<void*>(&research_cancel_hook),
            sizeof(kExpectedResearchStationCancel),
            kExpectedResearchStationCancel, &g_research_cancel_hook) &&
        api->install_inline_hook(
            at(armada, kResearchStationCanBuildRva),
            reinterpret_cast<void*>(&research_can_build_hook),
            sizeof(kExpectedResearchStationCanBuild),
            kExpectedResearchStationCanBuild,
            &g_research_can_build_hook) &&
        api->install_inline_hook(
            at(armada, kResearchStationItemConflictRva),
            reinterpret_cast<void*>(&research_item_conflict_hook),
            sizeof(kExpectedResearchStationItemConflict),
            kExpectedResearchStationItemConflict,
            &g_research_item_conflict_hook) &&
        api->install_inline_hook(
            at(armada, kResearchStationConstructionMatrixRva),
            reinterpret_cast<void*>(&research_matrix_hook),
            sizeof(kExpectedResearchStationConstructionMatrix),
            kExpectedResearchStationConstructionMatrix,
            &g_research_matrix_hook) &&
        api->install_inline_hook(
            at(fleet_ops, kFoResearchStationFinishRva),
            reinterpret_cast<void*>(&research_finish_hook),
            sizeof(kExpectedFoResearchStationFinish),
            kExpectedFoResearchStationFinish, &g_research_finish_hook) &&
        api->install_inline_hook(
            at(armada, kControlButtonPressRva),
            reinterpret_cast<void*>(&control_button_press_hook),
            sizeof(kExpectedControlButtonPress),
            kExpectedControlButtonPress, &g_control_button_press_hook) &&
        api->install_inline_hook(
            at(armada, kModeInfoBuildButtonNameRva),
            reinterpret_cast<void*>(
                &mode_info_build_button_name_hook),
            sizeof(kExpectedModeInfoBuildButtonName),
            kExpectedModeInfoBuildButtonName,
            &g_mode_info_build_button_name_hook) &&
        api->install_inline_hook(
            at(armada, kRaceIconRenderRva),
            reinterpret_cast<void*>(&race_icon_render_hook),
            sizeof(kExpectedRaceIconRender),
            kExpectedRaceIconRender, &g_race_icon_render_hook) &&
        api->install_inline_hook(
            at(armada, kProducerIsBusyRva),
            reinterpret_cast<void*>(&producer_is_busy_hook),
            sizeof(kExpectedProducerIsBusy), kExpectedProducerIsBusy,
            &g_producer_is_busy_hook) &&
        api->install_inline_hook(
            at(fleet_ops, kFoProducerPopCheckedRva),
            reinterpret_cast<void*>(&fo_producer_pop_checked_hook),
            sizeof(kExpectedFoProducerPopChecked),
            kExpectedFoProducerPopChecked,
            &g_fo_producer_pop_checked_hook) &&
        api->install_inline_hook(
            at(fleet_ops, kFoPopupUpdateButtonsRva),
            reinterpret_cast<void*>(&popup_update_buttons_hook),
            sizeof(kExpectedFoPopupUpdateButtons),
            kExpectedFoPopupUpdateButtons, &g_popup_update_buttons_hook) &&
        api->patch_call(
            at(armada, kConstructionRigStartHardpointCallRva),
            reinterpret_cast<void*>(
                &construction_rig_build_hardpoints_hook),
            kExpectedConstructionRigStartHardpointCall,
            sizeof(kExpectedConstructionRigStartHardpointCall)) &&
        api->patch_call(
            at(fleet_ops, kFoPopupBuildButtonBindCallRva),
            reinterpret_cast<void*>(&build_button_bind_hook),
            kExpectedFoPopupBuildButtonBindCall,
            sizeof(kExpectedFoPopupBuildButtonBindCall)) &&
        api->patch_call(
            at(fleet_ops, kFoPopupEvolveButtonBindCallRva),
            reinterpret_cast<void*>(&evolve_button_bind_hook),
            kExpectedFoPopupEvolveButtonBindCall,
            sizeof(kExpectedFoPopupEvolveButtonBindCall)) &&
        api->patch_call(
            at(fleet_ops, kFoPopupAiButtonBindCallRva),
            reinterpret_cast<void*>(&ai_button_bind_hook),
            kExpectedFoPopupAiButtonBindCall,
            sizeof(kExpectedFoPopupAiButtonBindCall)) &&
        api->patch_call(
            at(fleet_ops,
               kFoSelectionDisplayDrawProducerWireframeCallRva),
            reinterpret_cast<void*>(
                &selection_display_draw_producer_wireframe_hook),
            kExpectedFoDrawProducerWireframeCall,
            sizeof(kExpectedFoDrawProducerWireframeCall)) &&
        api->patch_call(
            at(armada, kShipDisplaySingleObjectDisplayCallRva),
            reinterpret_cast<void*>(
                &a2fo_ship_display_single_object_display_dispatch),
            kExpectedShipDisplaySingleObjectDisplayCall,
            sizeof(kExpectedShipDisplaySingleObjectDisplayCall)) &&
        api->patch_call(
            at(armada, kShipDisplaySingleObjectSimulateCallRva),
            reinterpret_cast<void*>(
                &a2fo_ship_display_single_object_simulate_dispatch),
            kExpectedShipDisplaySingleObjectSimulateCall,
            sizeof(kExpectedShipDisplaySingleObjectSimulateCall));
    if (!installed) {
        log_message("Hybrid production parser/runtime hook installation "
                    "failed");
        return false;
    }
    g_object_control_button_press_target = *object_button_press_slot;
    *object_button_press_slot =
        reinterpret_cast<void*>(&object_control_button_press_hook);
    log_message("Hybrid production isolated Yard/Construction/Research/"
                "Evolve buttons, grouped production controls, native station "
                "placement through a scoped ConstructionRig identity and "
                "protected sidecar with a class-safe hardpoint bypass, "
                "protected evolution cocoons, safe replacement handoff, "
                "ten-slot queue polish, and research hover wireframes "
                "enabled");
    return true;
}

bool register_research_station_hybrid_lists(
    void* station_class, void* parameter_db) noexcept {
    if (!station_class || !parameter_db || !g_registry_lock_ready) {
        return false;
    }
    if (!is_hybridbuild_parameter_db(parameter_db)) return false;
    if (g_api &&
        A2FO_MODULE_API_HAS(g_api, associate_evolver_cocoon_class) &&
        (g_api->capabilities & A2FO_CAP_COCOON_CLASS_ASSOCIATION) != 0 &&
        g_api->associate_evolver_cocoon_class) {
        if (!g_api->associate_evolver_cocoon_class(
                station_class, parameter_db)) {
            log_message("HybridBuild cocoon policy association failed; "
                        "Fleet Ops default selection remains available");
        }
    } else if (!g_logged_hybrid_cocoon_api_missing) {
        g_logged_hybrid_cocoon_api_missing = true;
        log_message("HybridBuild cocoon association API unavailable; "
                    "Fleet Ops default selection remains available");
    }
    try {
        load_explicit_lists(station_class, parameter_db);
    } catch (...) {
        log_message("Hybrid ResearchStation ODF-list parsing failed; class "
                    "kept on native research behavior");
        return false;
    }

    bool registered = false;
    EnterCriticalSection(&g_registry_lock);
    registered = g_class_lists.find(station_class) != g_class_lists.end();
    LeaveCriticalSection(&g_registry_lock);
    return registered;
}

std::size_t publish_research_station_hybrid_items(
    void* station_class) noexcept {
    if (!station_class || !g_registry_lock_ready || !g_armada ||
        !readable_range(bytes(station_class) + kProducerBuildItemsOffset,
                        sizeof(void*))) {
        return 0;
    }

    std::vector<ProductionListEntry> entries;
    EnterCriticalSection(&g_registry_lock);
    const auto found = g_class_lists.find(station_class);
    if (found != g_class_lists.end()) {
        try {
            entries = found->second->entries();
        } catch (...) {
            entries.clear();
        }
    }
    LeaveCriticalSection(&g_registry_lock);
    if (entries.empty()) return 0;

    void** legacy_research = *reinterpret_cast<void***>(
        bytes(station_class) + kProducerBuildItemsOffset);
    if (!readable_range(legacy_research,
                        kRuntimeBuildListCapacity * sizeof(void*))) {
        log_message("Hybrid ResearchStation command-panel storage is "
                    "unavailable; explicit items were not published");
        return 0;
    }

    auto runtime = std::make_unique<RuntimeClassLists>();
    runtime->legacy_research = legacy_research;
    const bool declared_construct = std::any_of(
        entries.begin(), entries.end(), [](const ProductionListEntry& entry) {
            return entry.method == ProductionMethod::construct;
        });
    const bool declared_yard = std::any_of(
        entries.begin(), entries.end(), [](const ProductionListEntry& entry) {
            return entry.method == ProductionMethod::yard;
        });
    const bool declared_research = std::any_of(
        entries.begin(), entries.end(), [](const ProductionListEntry& entry) {
            return entry.method == ProductionMethod::research;
        });
    const bool declared_evolve = std::any_of(
        entries.begin(), entries.end(), [](const ProductionListEntry& entry) {
            return entry.method == ProductionMethod::evolve;
        });
    auto find_class = reinterpret_cast<FindClassByProjectIdFunction>(
        at(g_armada, kGameObjectClassFindProjectIdRva));
    std::size_t construct_published = 0;
    std::size_t yard_published = 0;
    std::size_t research_published = 0;
    std::size_t evolve_published = 0;
    for (const ProductionListEntry& entry : entries) {
        if (entry.method != ProductionMethod::construct &&
            entry.method != ProductionMethod::yard &&
            entry.method != ProductionMethod::research &&
            entry.method != ProductionMethod::evolve) {
            continue;
        }
        void* item_class = find_class(&entry.project_id);
        if (!item_class) {
            log_message("Hybrid production target project " +
                        std::to_string(entry.project_id) +
                        " could not be loaded for the command panel");
            continue;
        }
        if (entry.index >= kRuntimeBuildListCapacity) continue;
        if (entry.method == ProductionMethod::construct) {
            runtime->construct[entry.index] = item_class;
            ++construct_published;
        } else if (entry.method == ProductionMethod::yard) {
            runtime->yard[entry.index] = item_class;
            ++yard_published;
        } else if (entry.method == ProductionMethod::research) {
            runtime->research[entry.index] = item_class;
            ++research_published;
        } else {
            runtime->evolve[entry.index] = item_class;
            ++evolve_published;
        }
    }
    runtime->has_construct = declared_construct && construct_published != 0;
    runtime->has_yard = declared_yard && yard_published != 0;
    runtime->has_explicit_research =
        declared_research && research_published != 0;
    // This execution adapter is deliberately scoped to the already-supported
    // hybrid ResearchStation host, whose inherited Producer FIFO is active.
    runtime->has_evolve = runtime->has_yard && declared_evolve &&
        evolve_published != 0;
    const std::size_t active_evolve_published = runtime->has_evolve
        ? evolve_published : 0;
    if (!runtime->has_construct && !runtime->has_yard &&
        !runtime->has_explicit_research &&
        !runtime->has_evolve) {
        return 0;
    }

    if (writable_range(bytes(station_class) + kClassMenuCapabilitiesOffset,
                       sizeof(std::uint32_t))) {
        auto* capabilities = reinterpret_cast<std::uint32_t*>(
            bytes(station_class) + kClassMenuCapabilitiesOffset);
        if (runtime->has_yard) *capabilities |= kYardMenuCapability;
        if (runtime->has_explicit_research) {
            *capabilities |= kResearchMenuCapability;
        }
        if (runtime->has_evolve) *capabilities |= kEvolveMenuCapability;
    } else {
        log_message("Hybrid ResearchStation menu capability storage is "
                    "unavailable");
        return 0;
    }

    EnterCriticalSection(&g_registry_lock);
    g_runtime_class_lists[station_class] = std::move(runtime);
    LeaveCriticalSection(&g_registry_lock);

    log_message("Hybrid ResearchStation published separate lists: yard " +
                std::to_string(yard_published) + ", construct " +
                std::to_string(construct_published) + ", research " +
                std::to_string(research_published) +
                (declared_research ? " (explicit)" : " (legacy)") +
                ", evolve " + std::to_string(active_evolve_published));
    return construct_published + yard_published + research_published +
        active_evolve_published;
}

bool resolve_explicit_production_method(
    void* producer_class, std::uint32_t target_project_id,
    ProductionMethod& method) noexcept {
    if (!producer_class || target_project_id == 0 ||
        !g_registry_lock_ready) {
        return false;
    }
    bool resolved = false;
    EnterCriticalSection(&g_registry_lock);
    const auto found = g_class_lists.find(producer_class);
    if (found != g_class_lists.end()) {
        const ProductionListEntry* entry =
            found->second->explicit_entry_for(target_project_id);
        if (entry) {
            method = entry->method;
            resolved = true;
        }
    }
    LeaveCriticalSection(&g_registry_lock);
    return resolved;
}

}  // namespace a2fo

extern "C" void* __cdecl a2fo_hybrid_build_position_for_command(
    void* producer, void* command) noexcept {
    return a2fo::hybrid_position_interface_for_command(producer, command);
}
