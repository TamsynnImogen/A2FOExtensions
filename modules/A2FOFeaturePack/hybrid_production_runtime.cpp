#include "hybrid_production_runtime.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace a2fo {
namespace {

constexpr const char* kModuleName = "A2FOFeaturePack";

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
constexpr std::uintptr_t kProducerConstructionMatrixRva = 0x0b9170;
constexpr std::uintptr_t kProducerUpdateBuildButtonsRva = 0x0b8c10;
constexpr std::uintptr_t kResearchStationStartRva = 0x0ba0e0;
constexpr std::uintptr_t kResearchStationCancelRva = 0x0ba1b0;
constexpr std::uintptr_t kResearchStationCanBuildRva = 0x0ba280;
constexpr std::uintptr_t kResearchStationItemConflictRva = 0x0ba4a0;
constexpr std::uintptr_t kResearchStationConstructionMatrixRva = 0x0babd0;
constexpr std::uintptr_t kControlButtonPressRva = 0x0e69e0;
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
constexpr std::uintptr_t kFoSelectionDisplayDrawProducerWireframeRva =
    0x1e7f74;
constexpr std::uintptr_t kFoSelectionDisplayDrawProducerWireframeCallRva =
    0x1e8572;
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
// capabilities are normally mutually exclusive. Entry 6 is constructed and
// laid out by the same code but is deliberately unused by root mode.
constexpr std::uintptr_t kFoPopupSpareRootButtonPointerRva = 0x247f0c;
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
constexpr std::size_t kProducerBuildItemsOffset = 0x450;
constexpr std::size_t kProducerButtonListOffset = 0x130;
constexpr std::size_t kButtonListEnabledMaskOffset = 0x08;
constexpr std::size_t kControlButtonStateOffset = 0x34;
constexpr std::size_t kControlButtonModeInfoOffset = 0x84;
constexpr std::size_t kModeInfoTargetClassOffset = 0x0c;
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

constexpr std::size_t kRuntimeBuildListCapacity = 57;
constexpr std::size_t kNativeResearchButtonCount = 14;
constexpr std::size_t kFoPopupButtonCount = 64;
constexpr std::uint32_t kNativeQueueCapacity = 10;
constexpr unsigned kRetainResearchMenuRefreshLimit = 60;
constexpr std::uint32_t kRootMenu = 0;
constexpr std::uint32_t kBuildMenu = 2;
constexpr std::uint32_t kResearchMenu = 3;
// PopupPalette treats either of these bits as permission to show its Build
// button. 0x80 is the Producer/yard capability; 0x40 is the placement-based
// constructor capability and must not be added to a ResearchStation.
constexpr std::uint32_t kYardMenuCapability = 0x80;
constexpr std::uint32_t kResearchMenuCapability = 0x2000;
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
const std::uint8_t kExpectedProducerConstructionMatrix[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x81, 0x58, 0x02, 0x00, 0x00};
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
// Complete first three instructions: mov eax,ecx; push esi;
// mov edx,[eax+0x88]. The nine-byte boundary is deliberate so the gateway
// cannot begin in the middle of the final memory-load instruction.
const std::uint8_t kExpectedControlButtonPress[] =
    {0x8b, 0xc1, 0x56, 0x8b, 0x90, 0x88, 0x00, 0x00, 0x00};
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
const std::uint8_t kExpectedFoProducerCancel[] =
    {0x55, 0x8b, 0xec, 0x51, 0x89, 0x4d, 0xfc};
const std::uint8_t kExpectedFoProducerFinish[] =
    {0x55, 0x8b, 0xec, 0x51, 0x53};
const std::uint8_t kExpectedFoProducerPopChecked[] =
    {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xe0};
const std::uint8_t kExpectedFoResearchStationFinish[] =
    {0x55, 0x8b, 0xec, 0x51, 0x89, 0x4d, 0xfc};
const std::uint8_t kExpectedFoPopupUpdateButtons[] =
    {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xe4, 0x53};
const std::uint8_t kExpectedFoPopupBuildButtonBindCall[] =
    {0xe8, 0x50, 0xb6, 0xff, 0xff};
const std::uint8_t kExpectedFoDrawProducerWireframeCall[] =
    {0xe8, 0xfd, 0xf9, 0xff, 0xff};

extern "C" std::uintptr_t a2fo_call_thiscall_0(
    void* function, void* self);
extern "C" std::uintptr_t a2fo_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument);
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
A2FO_InlineHook g_control_button_press_hook{};
A2FO_InlineHook g_race_icon_render_hook{};
A2FO_InlineHook g_producer_is_busy_hook{};
A2FO_InlineHook g_fo_producer_pop_checked_hook{};
A2FO_InlineHook g_popup_update_buttons_hook{};
std::unordered_map<void*, std::unique_ptr<HybridBuildLists>> g_class_lists;

struct RuntimeClassLists {
    void** legacy_research = nullptr;
    std::array<void*, kRuntimeBuildListCapacity> yard{};
    std::array<void*, kRuntimeBuildListCapacity> research{};
    bool has_yard = false;
    bool has_explicit_research = false;
};

std::unordered_map<void*, std::unique_ptr<RuntimeClassLists>>
    g_runtime_class_lists;
// The game UI is single-threaded. This is non-null only across a direct
// Producer/ResearchStation UpdateBuildButtons call made by the popup adapter.
// Both hybrid category refreshes must see queue capacity rather than the
// ResearchStation's normal one-active-job busy result.
void* g_queue_enabled_button_station = nullptr;
bool g_logged_shared_queue_ui = false;
bool g_logged_shared_queue_command = false;
bool g_logged_preserved_command_queue = false;
bool g_logged_queued_research_conflict = false;
bool g_logged_queued_research_button_disabled = false;
bool g_logged_research_menu_retention_armed = false;
bool g_logged_retained_research_menu = false;
bool g_logged_hybrid_yard_start = false;
bool g_logged_hybrid_research_start = false;
bool g_logged_hybrid_queue_slot_binding = false;
bool g_logged_hybrid_queue_wireframes = false;
bool g_logged_hybrid_research_queue_sprite = false;
bool g_logged_hybrid_full_queue_icon_suppressed = false;
bool g_logged_hybrid_research_hover_wireframe = false;
// These flags span only one synchronous PopupPalette refresh on the game's UI
// thread. They let the patched Build binding select a second physical control
// without changing native Research/Evolve/Trade behavior globally.
bool g_split_hybrid_root_buttons = false;
void* g_last_single_hybrid_selection = nullptr;
void* g_retain_research_menu_station = nullptr;
unsigned g_retain_research_menu_refreshes = 0;
bool g_retain_research_menu_saw_root = false;
unsigned g_control_button_press_depth = 0;
void* g_post_press_research_menu_station = nullptr;
void* g_object_control_button_press_target = nullptr;
void* g_last_popup = nullptr;
void* g_last_popup_craft_array = nullptr;
std::uintptr_t g_last_popup_argument2 = 0;
std::uintptr_t g_last_popup_argument3 = 0;

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
        "ResearchStation yard/research slice available",
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
    return lists && lists->has_yard && count < kNativeQueueCapacity;
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
    if (menu == kBuildMenu && lists->has_yard) {
        selected_items = lists->yard.data();
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
    if ((menu != kBuildMenu && menu != kResearchMenu) ||
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
    if ((menu_query || build_order_query) &&
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

std::uintptr_t __attribute__((regparm(3))) build_button_bind_hook(
    void* button, void* mode_info, std::uintptr_t enabled) noexcept {
    if (g_split_hybrid_root_buttons && g_fleet_ops) {
        void** spare_pointer = at<void*>(
            g_fleet_ops, kFoPopupSpareRootButtonPointerRva);
        if (readable_range(spare_pointer, sizeof(void*)) && *spare_pointer) {
            button = *spare_pointer;
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

    void* hybrid_station = single_hybrid_station(craft_array);
    void* retained_station = g_retain_research_menu_station;
    const bool retain_selection =
        g_retain_research_menu_refreshes != 0 &&
        (hybrid_station ||
         selection_contains(craft_array, retained_station) ||
         g_last_single_hybrid_selection == retained_station);
    if (!hybrid_station) {
        if (!retain_selection) g_last_single_hybrid_selection = nullptr;
    } else if (hybrid_station != g_last_single_hybrid_selection) {
        g_last_single_hybrid_selection = hybrid_station;
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
    }
    const bool previous_split = g_split_hybrid_root_buttons;
    g_split_hybrid_root_buttons = hybrid_station &&
        (menu == kRootMenu || menu == 8);
    // Fleet Ops performs another capability/button refresh inside the popup
    // gateway. Keep the queue-capacity IsBusy result active across that whole
    // refresh; limiting it to prepare_hybrid_station_menu allowed the gateway
    // to disable every choice again as soon as the first job became active.
    void* previous_queue_station = g_queue_enabled_button_station;
    if (hybrid_station && (menu == kBuildMenu || menu == kResearchMenu)) {
        g_queue_enabled_button_station = hybrid_station;
    }
    const std::uintptr_t result = a2fo_call_thiscall_3(
        g_popup_update_buttons_hook.gateway, popup,
        reinterpret_cast<std::uintptr_t>(craft_array), argument2, argument3);
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

void __attribute__((fastcall)) object_control_button_press_hook(
    void* button, void*) noexcept {
    const bool outermost = g_control_button_press_depth++ == 0;
    a2fo_call_thiscall_0(g_object_control_button_press_target, button);
    --g_control_button_press_depth;
    if (outermost) {
        restore_research_palette_after_button_press();
        g_post_press_research_menu_station = nullptr;
    }
}

void __attribute__((fastcall)) control_button_press_hook(
    void* button, void*) noexcept {
    const bool outermost = g_control_button_press_depth++ == 0;
    a2fo_call_thiscall_0(g_control_button_press_hook.gateway, button);
    --g_control_button_press_depth;
    if (outermost) {
        restore_research_palette_after_button_press();
        g_post_press_research_menu_station = nullptr;
    }
}

std::uintptr_t __attribute__((fastcall)) research_start_hook(
    void* station, void*) noexcept {
    ProductionMethod method = ProductionMethod::legacy;
    const bool explicit_method = active_explicit_method(station, method);
    const bool yard = explicit_method && method == ProductionMethod::yard;
    const std::uint32_t before = station && readable_range(
        bytes(station) + kQueueCountOffset, sizeof(std::uint32_t))
        ? *reinterpret_cast<const std::uint32_t*>(
              bytes(station) + kQueueCountOffset)
        : 0;
    const std::uintptr_t result = yard
        ? a2fo_call_thiscall_0(at(g_fleet_ops, kFoProducerStartRva), station)
        : a2fo_call_thiscall_0(g_research_start_hook.gateway, station);
    bool& logged_start = yard ? g_logged_hybrid_yard_start
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
            const char* route = yard ? "yard Producer"
                : explicit_method ? "explicit non-yard ResearchStation"
                                  : "unresolved ResearchStation";
            char message[224];
            std::snprintf(
                message, sizeof(message),
                "First hybrid %s StartBuild route: %s, count %lu -> %lu, "
                "active %s, result %lu",
                yard ? "yard" : "research", route,
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
    if (active_yard_job(station)) {
        return a2fo_call_thiscall_0(
            at(g_fleet_ops, kFoProducerCancelRva), station);
    }
    return a2fo_call_thiscall_0(g_research_cancel_hook.gateway, station);
}

std::uintptr_t __attribute__((fastcall)) research_can_build_hook(
    void* station, void*) noexcept {
    if (active_yard_job(station)) return 1;
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
                    target_method != ProductionMethod::yard;
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
        "Producer::GetConstructionMatrix", armada,
        kProducerConstructionMatrixRva,
        kExpectedProducerConstructionMatrix);
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
        "ControlButton::mButtonPressFunction", armada,
        kControlButtonPressRva, kExpectedControlButtonPress);
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
    signatures_match &= require_signature(
        "FleetOps Producer cancel", fleet_ops,
        kFoProducerCancelRva, kExpectedFoProducerCancel);
    signatures_match &= require_signature(
        "FleetOps Producer finish", fleet_ops,
        kFoProducerFinishRva, kExpectedFoProducerFinish);
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
    } catch (...) {
        log_message("Hybrid production registry allocation failed; disabled");
        return false;
    }

    const bool installed = api->install_inline_hook(
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
            at(fleet_ops, kFoPopupBuildButtonBindCallRva),
            reinterpret_cast<void*>(&build_button_bind_hook),
            kExpectedFoPopupBuildButtonBindCall,
            sizeof(kExpectedFoPopupBuildButtonBindCall)) &&
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
    log_message("Hybrid production isolated Build/Research buttons, separate "
                "menus, ten-slot queue polish, and research hover wireframes "
                "enabled");
    return true;
}

bool register_research_station_hybrid_lists(
    void* station_class, void* parameter_db) noexcept {
    if (!station_class || !parameter_db || !g_registry_lock_ready) {
        return false;
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
    const bool declared_yard = std::any_of(
        entries.begin(), entries.end(), [](const ProductionListEntry& entry) {
            return entry.method == ProductionMethod::yard;
        });
    const bool declared_research = std::any_of(
        entries.begin(), entries.end(), [](const ProductionListEntry& entry) {
            return entry.method == ProductionMethod::research;
        });
    auto find_class = reinterpret_cast<FindClassByProjectIdFunction>(
        at(g_armada, kGameObjectClassFindProjectIdRva));
    std::size_t yard_published = 0;
    std::size_t research_published = 0;
    for (const ProductionListEntry& entry : entries) {
        if (entry.method != ProductionMethod::yard &&
            entry.method != ProductionMethod::research) {
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
        if (entry.method == ProductionMethod::yard) {
            runtime->yard[entry.index] = item_class;
            ++yard_published;
        } else {
            runtime->research[entry.index] = item_class;
            ++research_published;
        }
    }
    runtime->has_yard = declared_yard && yard_published != 0;
    runtime->has_explicit_research =
        declared_research && research_published != 0;
    if (!runtime->has_yard && !runtime->has_explicit_research) return 0;

    if (writable_range(bytes(station_class) + kClassMenuCapabilitiesOffset,
                       sizeof(std::uint32_t))) {
        auto* capabilities = reinterpret_cast<std::uint32_t*>(
            bytes(station_class) + kClassMenuCapabilitiesOffset);
        if (runtime->has_yard) *capabilities |= kYardMenuCapability;
        if (runtime->has_explicit_research) {
            *capabilities |= kResearchMenuCapability;
        }
    } else {
        log_message("Hybrid ResearchStation menu capability storage is "
                    "unavailable");
        return 0;
    }

    EnterCriticalSection(&g_registry_lock);
    g_runtime_class_lists[station_class] = std::move(runtime);
    LeaveCriticalSection(&g_registry_lock);

    log_message("Hybrid ResearchStation published separate lists: yard " +
                std::to_string(yard_published) + ", research " +
                std::to_string(research_published) +
                (declared_research ? " (explicit)" : " (legacy)"));
    return yard_published + research_published;
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
