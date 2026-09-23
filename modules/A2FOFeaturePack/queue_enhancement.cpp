/*
 * File: modules/A2FOFeaturePack/queue_enhancement.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Producer queue filling, continuous production, and save markers.
 */

#include "queue_enhancement.hpp"

#include "hybrid_bridge_client.hpp"
#include "refit_queue_bridge_api.hpp"
#include "refit_queue_bridge_client.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace a2fo {
namespace {

constexpr const char* kModuleName = "A2FOFeaturePack";

// ArmadaL.exe RVAs from the supported Armada 1.1 symbol map/PDB.
constexpr std::uintptr_t kGameObjectQueueClassCommandRva = 0x0d4280;
constexpr std::uintptr_t kGameObjectDequeueClassCommandRva = 0x0d45f0;
constexpr std::uintptr_t kProducerDtorRva = 0x0b77d0;
constexpr std::uintptr_t kProducerSimulateRva = 0x0b7840;
constexpr std::uintptr_t kProducerLoadRva = 0x0b88d0;
constexpr std::uintptr_t kProducerSaveRva = 0x0b8aa0;
constexpr std::uintptr_t kGameObjectClassFindByProjectIdRva = 0x0cd150;
constexpr std::uintptr_t kCommandControlPointerRva = 0x36133c;
constexpr std::uintptr_t kCommandAltPointerRva = 0x361344;

// FleetOpsHook.dll RVAs. Fleet Ops already owns the Producer callbacks, so
// continuous production chains those callbacks instead of competing for the
// original Armada entry points.
constexpr std::uintptr_t kFoProducerFinishRva = 0x12255c;
constexpr std::uintptr_t kFoProducerCancelRva = 0x122514;
constexpr std::uintptr_t kFoProducerPushCheckedRva = 0x1229b8;
constexpr std::uintptr_t kFoProducerCommandPushRva = 0x122a10;
constexpr std::uintptr_t kFoProducerActDeleteRva = 0x122c8c;
constexpr std::uintptr_t kFoProducerClearRva = 0x122ef4;

constexpr std::uint32_t kQueueCapacity = 10;  // native Fleet Ops FIFO
constexpr std::uint32_t kVisibleQueueSlots = 10;
constexpr std::uint32_t kUnitsPerVisibleSlot = 10;
constexpr std::uint32_t kMaxLogicalQueue =
    kVisibleQueueSlots * kUnitsPerVisibleSlot;
constexpr std::uint32_t kBuildCommand = 0x19;
// Reserved within A2FO's typed-class order channel. The receive hook consumes
// these markers before Armada installs them as live object commands.
constexpr std::uint32_t kQueueFillMarkerCommand = 0xa1;
constexpr std::uint32_t kContinuousMarkerCommand = 0xa2;
// 0xa3 and 0xa4 are reserved by the RefitYards synchronized command bridge.
// Keep the extended build marker distinct: the receive hook checks refit
// commands before queue-enhancement markers, so sharing 0xa3 makes every
// overflow build look like (and be rejected as) a refit request.
constexpr std::uint32_t kExtendedQueueMarkerCommand = 0xa5;
static_assert(kExtendedQueueMarkerCommand != A2FO_REFIT_CLASS_COMMAND,
              "extended queue marker collides with refit marker");
static_assert(kExtendedQueueMarkerCommand != A2FO_REFIT_CANCEL_CLASS_COMMAND,
              "extended queue marker collides with refit-cancel marker");
constexpr std::uint32_t kRepeatSaveMarker = 0xa2f0c0deu;
constexpr unsigned kPausedRetryTicks = 30;
constexpr unsigned kSynchronizedPushLogLimit = 16;

constexpr std::size_t kObjectHandleOffset = 0x28;
constexpr std::size_t kClassProjectIdOffset = 0x1cc;
constexpr std::size_t kQueueHeadOffset = 0x270;
constexpr std::size_t kQueueCountOffset = 0x274;
constexpr std::size_t kCurrentQueueIdOffset = 0x2a0;
constexpr std::size_t kNextQueueIdOffset = 0x2a8;
constexpr std::size_t kCurrentBuildClassOffset = 0x254;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kProducerConfigOffset = 0x450;
constexpr std::size_t kChargeAtQueueOffset = 0xe4;
constexpr std::size_t kQueueItemNextOffset = 0x08;
constexpr std::size_t kQueueItemIdOffset = 0x0c;

// ShipDisplay / BuildQueueIcon presentation state. The real Producer FIFO stays
// untouched; these offsets only project consecutive identical native entries
// into fewer visible BuildQueueIcon children.
constexpr std::uintptr_t kBuildQueueIconVtableRva = 0x002b4994;
constexpr std::uintptr_t kDisplayInterfaceDrawTextInRectangleRva = 0x0011b160;
constexpr std::size_t kBuildQueueIconSimulateVtableIndex = 3;
constexpr std::size_t kBuildQueueIconRenderVtableIndex = 4;
constexpr std::size_t kDisplayComponentParentOffset = 0x04;
constexpr std::size_t kDisplayComponentRectangleOffset = 0x08;
constexpr std::size_t kBuildQueueIconOwnerOffset = 0x28;
constexpr std::size_t kBuildQueueIconTargetClassOffset = 0x3c;
constexpr std::size_t kShipDisplayBuildClassOffset = 0x100;
constexpr std::size_t kShipDisplayBuildNameOffset = 0x104;
constexpr std::size_t kShipDisplayNormalNameOffset = 0x94;
constexpr std::size_t kShipDisplayBuildQueueOffset = 0x120;
constexpr std::size_t kShipDisplaySelectedObjectOffset = 0x1e8;
constexpr std::size_t kTextComponentDisplayOverrideSlotOffset = 0x28;
constexpr std::size_t kTextComponentFlagsOffset = 0x68;
constexpr std::size_t kTextComponentConstrainOffset = 0x6c;
constexpr std::size_t kTextComponentColourOffset = 0x70;
constexpr std::size_t kTextComponentFontStateOffset = 0x7c;
constexpr std::size_t kUpdateBuildButtonsVtableOffset = 0xe8;

const std::uint8_t kExpectedGameObjectQueueClassCommand[] =
    {0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1};
const std::uint8_t kExpectedGameObjectDequeueClassCommand[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0c};
const std::uint8_t kExpectedProducerDtor[] =
    {0x55, 0x8b, 0xec, 0x6a, 0xff};
const std::uint8_t kExpectedProducerSimulate[] =
    {0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1};
const std::uint8_t kExpectedProducerLoad[] =
    {0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};
const std::uint8_t kExpectedProducerSave[] =
    {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08};
const std::uint8_t kExpectedFindByProjectId[] =
    {0x55, 0x8b, 0xec, 0xa1, 0xf8, 0x0b, 0x74, 0x00};
const std::uint8_t kExpectedFoFinish[] =
    {0x55, 0x8b, 0xec, 0x51, 0x53};
const std::uint8_t kExpectedFoCancel[] =
    {0x55, 0x8b, 0xec, 0x51, 0x89, 0x4d, 0xfc};
const std::uint8_t kExpectedFoCommandPush[] =
    {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xcc};
const std::uint8_t kExpectedFoActDelete[] =
    {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xc8};
const std::uint8_t kExpectedFoClear[] =
    {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xcc};
const std::uint8_t kExpectedFoPushChecked[] =
    {0x55, 0x8b, 0xec, 0x51, 0x53};
const std::uint8_t kExpectedDisplayInterfaceDrawTextInRectangle[] =
    {0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};

extern "C" std::uintptr_t a2fo_call_thiscall_0(
    void* function, void* self);
extern "C" std::uintptr_t a2fo_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument);
extern "C" std::uintptr_t a2fo_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
extern "C" std::uintptr_t a2fo_call_thiscall_7(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4, std::uintptr_t argument5,
    std::uintptr_t argument6, std::uintptr_t argument7);
const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
CRITICAL_SECTION g_queue_lock;
bool g_queue_lock_ready = false;
bool g_repeat_ready = false;
bool g_logged_build_order_path = false;
bool g_logged_synchronized_build_path = false;
unsigned g_synchronized_push_log_count = 0;

A2FO_InlineHook g_game_object_queue_class_command_hook{};
A2FO_InlineHook g_game_object_dequeue_class_command_hook{};
A2FO_InlineHook g_producer_dtor_hook{};
A2FO_InlineHook g_producer_simulate_hook{};
A2FO_InlineHook g_producer_load_hook{};
A2FO_InlineHook g_producer_save_hook{};
A2FO_InlineHook g_fo_finish_hook{};
A2FO_InlineHook g_fo_cancel_hook{};
A2FO_InlineHook g_fo_command_push_hook{};
A2FO_InlineHook g_fo_act_delete_hook{};
A2FO_InlineHook g_fo_clear_hook{};

void* g_build_queue_icon_simulate_original = nullptr;
void* g_build_queue_icon_render_original = nullptr;
bool g_grouped_queue_ui_ready = false;
bool g_logged_grouped_queue_ui = false;
bool g_logged_grouped_delete_remap = false;
bool g_logged_grouped_delete_slot_fallback = false;
bool g_logged_extended_queue_buttons = false;
bool g_logged_direct_extended_button_order = false;
thread_local bool g_extended_queue_button_refresh = false;

struct QueueUiRectangle {
    std::int32_t left;
    std::int32_t top;
    std::int32_t right;
    std::int32_t bottom;
};

struct QueueUiColour {
    float x;
    float y;
    float z;
};

struct PhysicalQueueEntry {
    void* target_class = nullptr;
    std::uint32_t queue_id = 0;
};

struct GroupedQueueEntry {
    void* target_class = nullptr;
    std::uint32_t count = 0;
    std::uint32_t physical_count = 0;
    std::uint32_t pending_count = 0;
    std::uint32_t first_queue_id = 0;
    std::uint32_t last_queue_id = 0;
    std::uint32_t first_pending_index = 0xffffffffu;
    std::uint32_t last_pending_index = 0xffffffffu;
    bool infinite = false;
};

struct GroupedQueueView {
    std::array<GroupedQueueEntry, kQueueCapacity> groups{};
    std::uint32_t count = 0;
};

struct GroupedQueueUiContext {
    bool active = false;
    void* producer = nullptr;
    std::uint32_t visual_slot = 0;
};

thread_local GroupedQueueUiContext g_grouped_queue_ui_context{};

struct ContinuousState {
    bool active = false;
    void* target_class = nullptr;
    std::uint32_t target_project_id = 0;
    unsigned paused_retry_ticks = 0;
};

std::unordered_map<std::uint32_t, ContinuousState> g_continuous;
std::unordered_map<std::uint32_t, std::vector<void*>> g_pending_queue;

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]);

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) {
    return reinterpret_cast<T*>(
        reinterpret_cast<std::uint8_t*>(module) + rva);
}

std::uint8_t* bytes(void* value) {
    return static_cast<std::uint8_t*>(value);
}


bool readable_range(const void* pointer, std::size_t size) noexcept {
    if (!pointer || size == 0) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    if (begin + size < begin) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(pointer, &information, sizeof(information)) !=
            sizeof(information) ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const auto region_begin = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress);
    const auto region_end = region_begin + information.RegionSize;
    return begin >= region_begin && begin + size <= region_end;
}

bool writable_range(void* pointer, std::size_t size) noexcept {
    if (!readable_range(pointer, size)) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(pointer, &information, sizeof(information)) !=
            sizeof(information)) {
        return false;
    }
    const DWORD protection = information.Protect & 0xffu;
    return protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

template <typename T>
T read_ui_value(const void* base, std::size_t offset,
                T fallback = T{}) noexcept {
    if (!base) return fallback;
    const auto* address = static_cast<const std::uint8_t*>(base) + offset;
    if (!readable_range(address, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

void log_message(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message);
}

bool command_key_down(std::uintptr_t pointer_rva) {
    if (!g_armada) return false;
    const int* command_state = *at<const int*>(g_armada, pointer_rva);
    return command_state && *command_state != 0;
}

bool modifier_key_down(std::uintptr_t pointer_rva, int virtual_key) {
    return command_key_down(pointer_rva) ||
        (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
}

std::uint32_t object_handle(void* producer) {
    return producer
        ? *reinterpret_cast<const std::uint32_t*>(
              bytes(producer) + kObjectHandleOffset)
        : 0;
}

std::uint32_t project_id(void* object_class) {
    if (!object_class) return 0;
    const auto* id = *reinterpret_cast<const std::uint32_t* const*>(
        bytes(object_class) + kClassProjectIdOffset);
    return id ? *id : 0;
}

bool dispatch_producer_event(std::uint32_t kind, void* producer,
                             void* target_class) {
    if (!g_api ||
        !A2FO_MODULE_API_HAS(g_api, dispatch_producer_event) ||
        (g_api->capabilities & A2FO_CAP_PRODUCER_EVENTS) == 0 ||
        !g_api->dispatch_producer_event) {
        return true;
    }
    A2FO_ProducerEvent event{};
    event.struct_size = sizeof(event);
    event.kind = kind;
    event.producer = producer;
    event.target_class = target_class;
    return g_api->dispatch_producer_event(&event);
}

void* queue_head_target_class(void* producer) {
    if (!producer) return nullptr;
    void* item = *reinterpret_cast<void**>(
        bytes(producer) + kQueueHeadOffset);
    return item ? *reinterpret_cast<void**>(item) : nullptr;
}

void* current_build_target_class(void* producer) {
    return producer ? *reinterpret_cast<void**>(
        bytes(producer) + kCurrentBuildClassOffset) : nullptr;
}

void* queued_target_class(void* producer, std::uint32_t queue_id) {
    if (!producer) return nullptr;
    void* item = *reinterpret_cast<void**>(
        bytes(producer) + kQueueHeadOffset);
    while (item) {
        if (*reinterpret_cast<std::uint32_t*>(
                bytes(item) + kQueueItemIdOffset) == queue_id) {
            return *reinterpret_cast<void**>(item);
        }
        item = *reinterpret_cast<void**>(
            bytes(item) + kQueueItemNextOffset);
    }
    return nullptr;
}

std::uint32_t current_queue_id(void* producer) {
    return producer ? *reinterpret_cast<const std::uint32_t*>(
        bytes(producer) + kCurrentQueueIdOffset) : 0;
}


struct PendingQueueSnapshot {
    std::array<void*, kMaxLogicalQueue> entries{};
    std::uint32_t count = 0;
};

PendingQueueSnapshot pending_queue_snapshot(void* producer) noexcept {
    PendingQueueSnapshot snapshot{};
    if (!producer || !g_queue_lock_ready) return snapshot;
    const std::uint32_t handle = object_handle(producer);
    if (!handle) return snapshot;
    EnterCriticalSection(&g_queue_lock);
    const auto found = g_pending_queue.find(handle);
    if (found != g_pending_queue.end()) {
        snapshot.count = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(found->second.size()),
            kMaxLogicalQueue);
        for (std::uint32_t index = 0; index < snapshot.count; ++index) {
            snapshot.entries[index] = found->second[index];
        }
    }
    LeaveCriticalSection(&g_queue_lock);
    return snapshot;
}

bool append_pending_queue(void* producer, void* target_class) noexcept {
    if (!producer || !target_class || !g_queue_lock_ready) return false;
    const std::uint32_t handle = object_handle(producer);
    if (!handle) return false;
    bool appended = false;
    try {
        EnterCriticalSection(&g_queue_lock);
        auto& pending = g_pending_queue[handle];
        if (pending.size() < kMaxLogicalQueue) {
            pending.push_back(target_class);
            appended = true;
        }
        LeaveCriticalSection(&g_queue_lock);
    } catch (...) {
        LeaveCriticalSection(&g_queue_lock);
    }
    return appended;
}

bool erase_pending_queue_index(void* producer, std::uint32_t index) noexcept {
    if (!producer || !g_queue_lock_ready) return false;
    const std::uint32_t handle = object_handle(producer);
    if (!handle) return false;
    bool removed = false;
    EnterCriticalSection(&g_queue_lock);
    const auto found = g_pending_queue.find(handle);
    if (found != g_pending_queue.end() && index < found->second.size()) {
        found->second.erase(found->second.begin() + index);
        if (found->second.empty()) g_pending_queue.erase(found);
        removed = true;
    }
    LeaveCriticalSection(&g_queue_lock);
    return removed;
}

void clear_pending_queue(void* producer) noexcept {
    if (!producer || !g_queue_lock_ready) return;
    const std::uint32_t handle = object_handle(producer);
    if (!handle) return;
    EnterCriticalSection(&g_queue_lock);
    g_pending_queue.erase(handle);
    LeaveCriticalSection(&g_queue_lock);
}

bool continuous_state(void* producer, void** target_class) noexcept {
    if (target_class) *target_class = nullptr;
    if (!producer || !g_queue_lock_ready) return false;
    const std::uint32_t handle = object_handle(producer);
    if (!handle) return false;
    bool active = false;
    EnterCriticalSection(&g_queue_lock);
    const auto found = g_continuous.find(handle);
    if (found != g_continuous.end() && found->second.active) {
        active = true;
        if (target_class) *target_class = found->second.target_class;
    }
    LeaveCriticalSection(&g_queue_lock);
    return active;
}

GroupedQueueView collect_grouped_queue(void* producer) noexcept {
    GroupedQueueView result{};
    if (!producer || !readable_range(
            bytes(producer) + kQueueHeadOffset, sizeof(void*)) ||
        !readable_range(
            bytes(producer) + kCurrentBuildClassOffset, sizeof(void*)) ||
        !readable_range(
            bytes(producer) + kCurrentQueueIdOffset,
            sizeof(std::uint32_t))) {
        return result;
    }

    std::array<PhysicalQueueEntry, kQueueCapacity> physical{};
    std::uint32_t physical_count = 0;
    const void* active_class = current_build_target_class(producer);
    const std::uint32_t active_id = current_queue_id(producer);
    bool active_in_fifo = false;

    void* item = *reinterpret_cast<void**>(
        bytes(producer) + kQueueHeadOffset);
    while (item && physical_count < kQueueCapacity) {
        if (!readable_range(item, kQueueItemIdOffset + sizeof(std::uint32_t))) {
            break;
        }
        void* target_class = *reinterpret_cast<void**>(item);
        const std::uint32_t id = *reinterpret_cast<const std::uint32_t*>(
            bytes(item) + kQueueItemIdOffset);
        if (target_class) {
            physical[physical_count++] = PhysicalQueueEntry{target_class, id};
            if (active_class && active_id != 0 && id == active_id) {
                active_in_fifo = true;
            }
        }
        item = *reinterpret_cast<void**>(bytes(item) + kQueueItemNextOffset);
    }

    // Generic Producer queues retain the active job at the FIFO head. Hybrid
    // research can detach that active node while preserving currentBuildClass;
    // prepend only in the latter case so the logical presentation remains in
    // actual completion order.
    if (active_class && !active_in_fifo &&
        (active_id != 0 || physical_count == 0)) {
        const std::uint32_t limit = std::min<std::uint32_t>(
            physical_count, kQueueCapacity - 1);
        for (std::uint32_t index = limit; index > 0; --index) {
            physical[index] = physical[index - 1];
        }
        physical[0] = PhysicalQueueEntry{
            const_cast<void*>(active_class), active_id};
        physical_count = std::min<std::uint32_t>(
            physical_count + 1, kQueueCapacity);
    }

    auto append_grouped = [&](void* target_class, std::uint32_t queue_id,
                              bool pending, std::uint32_t pending_index) {
        if (!target_class) return;
        GroupedQueueEntry* group = nullptr;
        if (result.count != 0) {
            GroupedQueueEntry& previous = result.groups[result.count - 1];
            if (previous.target_class == target_class &&
                previous.count < kUnitsPerVisibleSlot && !previous.infinite) {
                group = &previous;
            }
        }
        if (!group) {
            if (result.count >= kVisibleQueueSlots) return;
            group = &result.groups[result.count++];
            group->target_class = target_class;
        }
        ++group->count;
        if (pending) {
            ++group->pending_count;
            if (group->first_pending_index == 0xffffffffu) {
                group->first_pending_index = pending_index;
            }
            group->last_pending_index = pending_index;
        } else {
            ++group->physical_count;
            if (queue_id != 0) {
                if (group->first_queue_id == 0) group->first_queue_id = queue_id;
                group->last_queue_id = queue_id;
            }
        }
    };

    for (std::uint32_t index = 0; index < physical_count; ++index) {
        append_grouped(
            physical[index].target_class, physical[index].queue_id,
            false, 0xffffffffu);
    }

    const PendingQueueSnapshot pending = pending_queue_snapshot(producer);
    for (std::uint32_t index = 0; index < pending.count; ++index) {
        append_grouped(pending.entries[index], 0, true, index);
    }

    void* repeat_target = nullptr;
    if (continuous_state(producer, &repeat_target) && repeat_target) {
        if (result.count != 0 &&
            result.groups[result.count - 1].target_class == repeat_target) {
            result.groups[result.count - 1].infinite = true;
        } else if (result.count < kVisibleQueueSlots) {
            GroupedQueueEntry& group = result.groups[result.count++];
            group.target_class = repeat_target;
            group.count = 1;
            group.infinite = true;
        }
    }
    return result;
}

std::uint32_t logical_queue_units(const GroupedQueueView& view) noexcept {
    std::uint32_t result = 0;
    for (std::uint32_t index = 0; index < view.count; ++index) {
        result += view.groups[index].count;
    }
    return result;
}

bool logical_queue_has_room_for(
    const GroupedQueueView& view, void* target_class) noexcept {
    if (!target_class || logical_queue_units(view) >= kMaxLogicalQueue) {
        return false;
    }
    if (view.count == 0) return true;
    const GroupedQueueEntry& tail = view.groups[view.count - 1];
    if (tail.infinite) return false;
    if (tail.target_class == target_class &&
        tail.count < kUnitsPerVisibleSlot) {
        return true;
    }
    return view.count < kVisibleQueueSlots;
}

// Continuous mode does not always need another finite queue unit. If the
// requested class already occupies the logical tail, Ctrl+Alt may promote that
// existing slot to xINF even when the slot is already x10. Otherwise it needs
// the same room as one ordinary queued unit to create its backing job.
bool logical_queue_can_enable_continuous(
    const GroupedQueueView& view, void* target_class) noexcept {
    if (!target_class) return false;
    if (view.count != 0) {
        const GroupedQueueEntry& tail = view.groups[view.count - 1];
        if (tail.target_class == target_class) return true;
    }
    return logical_queue_has_room_for(view, target_class);
}

bool grouped_queue_icon_context(
    void* icon, void** ship_display, void** producer,
    std::uint32_t* visual_slot) noexcept {
    if (!icon || !ship_display || !producer || !visual_slot ||
        !readable_range(
            bytes(icon) + kDisplayComponentParentOffset, sizeof(void*))) {
        return false;
    }
    void* parent = read_ui_value<void*>(
        icon, kDisplayComponentParentOffset, nullptr);
    if (!parent || !readable_range(
            bytes(parent) + kShipDisplayBuildQueueOffset,
            kQueueCapacity * sizeof(void*)) ||
        !readable_range(
            bytes(parent) + kShipDisplaySelectedObjectOffset,
            sizeof(void*))) {
        return false;
    }
    std::uint32_t slot = kQueueCapacity;
    for (std::uint32_t index = 0; index < kQueueCapacity; ++index) {
        if (read_ui_value<void*>(
                parent, kShipDisplayBuildQueueOffset + index * sizeof(void*),
                nullptr) == icon) {
            slot = index;
            break;
        }
    }
    if (slot >= kQueueCapacity) return false;
    void* selected = read_ui_value<void*>(
        parent, kShipDisplaySelectedObjectOffset, nullptr);
    if (!selected) return false;
    *ship_display = parent;
    *producer = selected;
    *visual_slot = slot;
    return true;
}

void* queue_count_text_component(void* ship_display) noexcept {
    if (!ship_display) return nullptr;
    for (const std::size_t offset : {
             kShipDisplayBuildNameOffset,
             kShipDisplayBuildClassOffset,
             kShipDisplayNormalNameOffset}) {
        void* component = read_ui_value<void*>(ship_display, offset, nullptr);
        if (component && readable_range(
                bytes(component) + kTextComponentFontStateOffset, 12)) {
            return component;
        }
    }
    return nullptr;
}

bool draw_group_count(void* icon, void* ship_display,
                      std::uint32_t count, bool infinite) noexcept {
    if ((!infinite && count <= 1) || !g_armada || !icon || !ship_display ||
        !signature_matches(
            g_armada, kDisplayInterfaceDrawTextInRectangleRva,
            kExpectedDisplayInterfaceDrawTextInRectangle)) {
        return false;
    }
    void* text_component = queue_count_text_component(ship_display);
    if (!text_component) return false;
    QueueUiRectangle rectangle = read_ui_value<QueueUiRectangle>(
        icon, kDisplayComponentRectangleOffset, QueueUiRectangle{});
    if (rectangle.right <= rectangle.left ||
        rectangle.bottom <= rectangle.top) {
        return false;
    }
    const std::int32_t width = rectangle.right - rectangle.left;
    const std::int32_t height = rectangle.bottom - rectangle.top;
    rectangle.left = rectangle.left + std::max<std::int32_t>(0, width / 2);
    rectangle.bottom = rectangle.top + std::max<std::int32_t>(12, height / 2);

    void* display_interface = read_ui_value<void*>(
        text_component, kDisplayComponentParentOffset, nullptr);
    if (!display_interface) return false;
    void* display_override = nullptr;
    void* display_slot = read_ui_value<void*>(
        text_component, kTextComponentDisplayOverrideSlotOffset, nullptr);
    if (display_slot) {
        display_override = read_ui_value<void*>(display_slot, 0, nullptr);
    }
    const std::int32_t text_flags = read_ui_value<std::int32_t>(
        text_component, kTextComponentFlagsOffset, 9);
    const std::uint8_t constrain = read_ui_value<std::uint8_t>(
        text_component, kTextComponentConstrainOffset, 0);
    QueueUiColour colour = read_ui_value<QueueUiColour>(
        text_component, kTextComponentColourOffset,
        QueueUiColour{1.0f, 1.0f, 1.0f});
    void* font_state = bytes(text_component) + kTextComponentFontStateOffset;
    if (!readable_range(font_state, 12)) return false;

    char label[16]{};
    if (infinite) {
        // Keep continuous-build presentation ASCII-only so mods do not need
        // a custom bitmap-font glyph. This also survives every stock/localized
        // Fleet Ops font atlas unchanged.
        std::snprintf(label, sizeof(label), "xINF");
    } else {
        std::snprintf(label, sizeof(label), "x%lu",
                      static_cast<unsigned long>(count));
    }
    a2fo_call_thiscall_7(
        at(g_armada, kDisplayInterfaceDrawTextInRectangleRva),
        display_interface,
        reinterpret_cast<std::uintptr_t>(label),
        reinterpret_cast<std::uintptr_t>(&rectangle),
        static_cast<std::uintptr_t>(text_flags),
        reinterpret_cast<std::uintptr_t>(&colour),
        reinterpret_cast<std::uintptr_t>(display_override),
        static_cast<std::uintptr_t>(constrain),
        reinterpret_cast<std::uintptr_t>(font_state));
    return true;
}

void __attribute__((fastcall)) grouped_build_queue_icon_render_hook(
    void* icon, void*) noexcept {
    if (!g_build_queue_icon_render_original) return;
    if (!g_grouped_queue_ui_ready) {
        a2fo_call_thiscall_0(g_build_queue_icon_render_original, icon);
        return;
    }
    void* ship_display = nullptr;
    void* producer = nullptr;
    std::uint32_t slot = 0;
    if (!grouped_queue_icon_context(
            icon, &ship_display, &producer, &slot) ||
        !writable_range(
            bytes(icon) + kBuildQueueIconTargetClassOffset,
            sizeof(void*))) {
        a2fo_call_thiscall_0(g_build_queue_icon_render_original, icon);
        return;
    }
    const GroupedQueueView view = collect_grouped_queue(producer);
    void** target_slot = reinterpret_cast<void**>(
        bytes(icon) + kBuildQueueIconTargetClassOffset);
    void* saved_target = *target_slot;
    *target_slot = slot < view.count ? view.groups[slot].target_class : nullptr;
    if (writable_range(
            bytes(icon) + kBuildQueueIconOwnerOffset, sizeof(void*))) {
        *reinterpret_cast<void**>(
            bytes(icon) + kBuildQueueIconOwnerOffset) = producer;
    }
    a2fo_call_thiscall_0(g_build_queue_icon_render_original, icon);
    if (slot < view.count &&
        (view.groups[slot].count > 1 || view.groups[slot].infinite)) {
        draw_group_count(icon, ship_display, view.groups[slot].count,
                         view.groups[slot].infinite);
    }
    *target_slot = saved_target;
}

std::uint32_t native_queue_count(void* producer) noexcept;

// Fleet Ops disables Producer build buttons when the physical FIFO reaches
// ten jobs, before GameObject::QueueClassCommand can route an 11th click into
// A2FO's sidecar. Re-run the Producer's normal UpdateBuildButtons policy with
// only the queue-count field temporarily presented as nine. This preserves
// technology/resource/button checks while removing only the native 10-job UI
// gate. The actual queue, linked list, build order, and simulation count are
// never changed.
void refresh_build_buttons_for_extended_queue(
    void* producer, const GroupedQueueView& view) noexcept {
    if (!producer || g_extended_queue_button_refresh ||
        native_queue_count(producer) < kQueueCapacity ||
        logical_queue_units(view) >= kMaxLogicalQueue ||
        // Once all ten logical slots exist, HybridBuild's popup post-pass
        // applies the target-specific tail mask. Do not run this broad native
        // refresh afterward or it would re-enable classes that would require
        // an eleventh visible slot.
        view.count >= kVisibleQueueSlots ||
        !writable_range(bytes(producer) + kQueueCountOffset,
                        sizeof(std::uint32_t)) ||
        !readable_range(producer, sizeof(void*))) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(producer);
    void** update_slot = vtable
        ? reinterpret_cast<void**>(bytes(vtable) +
                                   kUpdateBuildButtonsVtableOffset)
        : nullptr;
    if (!readable_range(update_slot, sizeof(void*)) || !*update_slot) return;

    auto* queue_count = reinterpret_cast<std::uint32_t*>(
        bytes(producer) + kQueueCountOffset);
    const std::uint32_t saved_count = *queue_count;
    if (saved_count < kQueueCapacity) return;

    g_extended_queue_button_refresh = true;
    *queue_count = kQueueCapacity - 1;
    a2fo_call_thiscall_0(*update_slot, producer);
    *queue_count = saved_count;
    g_extended_queue_button_refresh = false;

    if (!g_logged_extended_queue_buttons) {
        g_logged_extended_queue_buttons = true;
        log_message(
            "Extended queue kept Producer build buttons enabled past 10 "
            "native jobs");
    }
}

void __attribute__((fastcall)) grouped_build_queue_icon_simulate_hook(
    void* icon, void*) noexcept {
    if (!g_build_queue_icon_simulate_original) return;
    if (!g_grouped_queue_ui_ready) {
        a2fo_call_thiscall_0(g_build_queue_icon_simulate_original, icon);
        return;
    }
    void* ship_display = nullptr;
    void* producer = nullptr;
    std::uint32_t slot = 0;
    if (!grouped_queue_icon_context(
            icon, &ship_display, &producer, &slot) ||
        !writable_range(
            bytes(icon) + kBuildQueueIconTargetClassOffset,
            sizeof(void*))) {
        a2fo_call_thiscall_0(g_build_queue_icon_simulate_original, icon);
        return;
    }
    const GroupedQueueView view = collect_grouped_queue(producer);
    if (slot == 0) {
        refresh_build_buttons_for_extended_queue(producer, view);
    }
    void** target_slot = reinterpret_cast<void**>(
        bytes(icon) + kBuildQueueIconTargetClassOffset);
    void* saved_target = *target_slot;
    *target_slot = slot < view.count ? view.groups[slot].target_class : nullptr;

    const GroupedQueueUiContext saved_context = g_grouped_queue_ui_context;
    g_grouped_queue_ui_context.active = slot < view.count;
    g_grouped_queue_ui_context.producer = producer;
    g_grouped_queue_ui_context.visual_slot = slot;
    a2fo_call_thiscall_0(g_build_queue_icon_simulate_original, icon);
    g_grouped_queue_ui_context = saved_context;
    *target_slot = saved_target;
}

bool install_grouped_queue_ui() noexcept {
    if (!g_armada || !g_repeat_ready ||
        !signature_matches(
            g_armada, kDisplayInterfaceDrawTextInRectangleRva,
            kExpectedDisplayInterfaceDrawTextInRectangle)) {
        return false;
    }
    void** vtable = at<void*>(g_armada, kBuildQueueIconVtableRva);
    if (!vtable || !readable_range(
            vtable, (kBuildQueueIconRenderVtableIndex + 1) * sizeof(void*))) {
        return false;
    }
    void** simulate_slot = vtable + kBuildQueueIconSimulateVtableIndex;
    void** render_slot = vtable + kBuildQueueIconRenderVtableIndex;
    void* simulate = *simulate_slot;
    void* render = *render_slot;
    if (!simulate || !render || !readable_range(simulate, 1) ||
        !readable_range(render, 1)) {
        return false;
    }

    DWORD old_protection = 0;
    if (!VirtualProtect(
            simulate_slot, 2 * sizeof(void*), PAGE_EXECUTE_READWRITE,
            &old_protection)) {
        return false;
    }
    g_build_queue_icon_simulate_original = simulate;
    g_build_queue_icon_render_original = render;
    *simulate_slot = reinterpret_cast<void*>(
        &grouped_build_queue_icon_simulate_hook);
    *render_slot = reinterpret_cast<void*>(
        &grouped_build_queue_icon_render_hook);
    DWORD ignored = 0;
    VirtualProtect(simulate_slot, 2 * sizeof(void*), old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), simulate_slot, 2 * sizeof(void*));
    return true;
}

std::uint32_t pushed_queue_id(
    void* producer,
    const std::array<std::uint32_t, kQueueCapacity>& previous_ids,
    std::uint32_t previous_count) {
    if (!producer) return 0;
    void* item = *reinterpret_cast<void**>(
        bytes(producer) + kQueueHeadOffset);
    for (std::uint32_t visited = 0;
         item && visited < kQueueCapacity; ++visited) {
        const std::uint32_t id = *reinterpret_cast<const std::uint32_t*>(
            bytes(item) + kQueueItemIdOffset);
        bool existed = false;
        for (std::uint32_t index = 0; index < previous_count; ++index) {
            if (previous_ids[index] == id) {
                existed = true;
                break;
            }
        }
        if (!existed) return id;
        item = *reinterpret_cast<void**>(
            bytes(item) + kQueueItemNextOffset);
    }
    return 0;
}

std::uint32_t push_refit_checked(
    void* producer, void* target_class) noexcept {
    if (!g_repeat_ready || !producer || !target_class ||
        *reinterpret_cast<const std::uint32_t*>(
            bytes(producer) + kQueueCountOffset) >= kQueueCapacity ||
        hybrid_production_has_evolution_barrier(producer) ||
        !dispatch_producer_event(
            A2FO_PRODUCER_EVENT_ADMIT, producer, target_class)) {
        return 0;
    }

    std::array<std::uint32_t, kQueueCapacity> previous_ids{};
    std::uint32_t previous_count = 0;
    void* item = *reinterpret_cast<void**>(
        bytes(producer) + kQueueHeadOffset);
    while (item && previous_count < kQueueCapacity) {
        previous_ids[previous_count++] =
            *reinterpret_cast<const std::uint32_t*>(
                bytes(item) + kQueueItemIdOffset);
        item = *reinterpret_cast<void**>(
            bytes(item) + kQueueItemNextOffset);
    }

    const std::uint32_t before = *reinterpret_cast<const std::uint32_t*>(
        bytes(producer) + kQueueCountOffset);
    a2fo_call_thiscall_1(
        at(g_fleet_ops, kFoProducerPushCheckedRva), producer,
        reinterpret_cast<std::uintptr_t>(target_class));
    const std::uint32_t after = *reinterpret_cast<const std::uint32_t*>(
        bytes(producer) + kQueueCountOffset);
    return after > before
        ? pushed_queue_id(producer, previous_ids, previous_count) : 0;
}

bool charges_resources_when_queued(void* producer) {
    if (!producer) return false;
    void* object_class = *reinterpret_cast<void**>(
        bytes(producer) + kObjectClassOffset);
    if (!object_class) return false;
    void* config = *reinterpret_cast<void**>(
        bytes(object_class) + kProducerConfigOffset);
    return config && *reinterpret_cast<const std::uint8_t*>(
        bytes(config) + kChargeAtQueueOffset) != 0;
}

void stop_continuous(void* producer) {
    const std::uint32_t handle = object_handle(producer);
    if (!handle || !g_queue_lock_ready) return;
    EnterCriticalSection(&g_queue_lock);
    g_continuous.erase(handle);
    LeaveCriticalSection(&g_queue_lock);
}

std::uint32_t native_queue_count(void* producer) noexcept {
    return producer ? *reinterpret_cast<const std::uint32_t*>(
        bytes(producer) + kQueueCountOffset) : 0;
}

bool push_native_checked_once(
    void* producer, void* target_class, bool dispatch_admit) noexcept {
    if (!producer || !target_class ||
        native_queue_count(producer) >= kQueueCapacity ||
        hybrid_production_has_evolution_barrier(producer)) {
        return false;
    }
    if (dispatch_admit && !dispatch_producer_event(
            A2FO_PRODUCER_EVENT_ADMIT, producer, target_class)) {
        return false;
    }
    const std::uint32_t before = native_queue_count(producer);
    a2fo_call_thiscall_1(
        at(g_fleet_ops, kFoProducerPushCheckedRva), producer,
        reinterpret_cast<std::uintptr_t>(target_class));
    return native_queue_count(producer) > before;
}

bool queue_logical_one_checked(void* producer, void* target_class) noexcept {
    if (!producer || !target_class ||
        hybrid_production_has_evolution_barrier(producer) ||
        hybrid_production_has_queued_research_conflict(
            producer, target_class)) {
        return false;
    }
    const GroupedQueueView view = collect_grouped_queue(producer);
    if (!logical_queue_has_room_for(view, target_class)) return false;

    const PendingQueueSnapshot pending = pending_queue_snapshot(producer);
    if (pending.count == 0 && native_queue_count(producer) < kQueueCapacity) {
        return push_native_checked_once(producer, target_class, true);
    }
    if (!dispatch_producer_event(
            A2FO_PRODUCER_EVENT_ADMIT, producer, target_class)) {
        return false;
    }
    return append_pending_queue(producer, target_class);
}

void materialize_pending_queue(void* producer) noexcept {
    if (!producer || refit_has_waiting_job(producer) ||
        hybrid_production_has_evolution_barrier(producer)) {
        return;
    }
    for (std::uint32_t attempt = 0;
         attempt < kQueueCapacity && native_queue_count(producer) < kQueueCapacity;
         ++attempt) {
        const PendingQueueSnapshot pending = pending_queue_snapshot(producer);
        if (pending.count == 0 || !pending.entries[0]) break;
        void* target_class = pending.entries[0];
        if (hybrid_production_has_queued_research_conflict(
                producer, target_class)) {
            break;
        }
        // The logical order already passed A2FO_PRODUCER_EVENT_ADMIT when it
        // was accepted. Fleet Ops performs its normal resource/tech checks as
        // the deferred order is moved into the native ten-job FIFO.
        if (!push_native_checked_once(producer, target_class, false)) break;
        if (!erase_pending_queue_index(producer, 0)) break;
    }
}

std::uint32_t fill_queue_checked(void* producer, void* target_class) {
    if (!producer || !target_class) return 0;
    if (hybrid_production_has_evolution_barrier(producer)) return 0;
    if (hybrid_production_has_queued_research_conflict(
            producer, target_class)) {
        return 0;
    }

    GroupedQueueView view = collect_grouped_queue(producer);
    std::uint32_t room_in_slot = kUnitsPerVisibleSlot;
    if (view.count != 0) {
        const GroupedQueueEntry& tail = view.groups[view.count - 1];
        if (tail.infinite) return 0;
        if (tail.target_class == target_class) {
            if (tail.count < kUnitsPerVisibleSlot) {
                room_in_slot = kUnitsPerVisibleSlot - tail.count;
            } else if (view.count >= kVisibleQueueSlots) {
                return 0;
            } else {
                // A full x10 batch starts a new visible slot on the next
                // Ctrl-fill, even when it is the same ship class.
                room_in_slot = kUnitsPerVisibleSlot;
            }
        } else if (view.count >= kVisibleQueueSlots) {
            return 0;
        }
    }

    const bool evolve = hybrid_production_is_evolve_target(
        producer, target_class);
    const std::uint32_t attempt_limit = evolve ? 1 : room_in_slot;
    std::uint32_t added = 0;
    for (std::uint32_t attempt = 0; attempt < attempt_limit; ++attempt) {
        if (!queue_logical_one_checked(producer, target_class)) break;
        ++added;
    }
    return added;
}

void __attribute__((fastcall)) game_object_dequeue_class_command_hook(
    void* game_object, void*, std::uint32_t command,
    void* target_class) {
    if (command == A2FO_REFIT_CLASS_COMMAND) {
        if (!consume_refit_synchronized_command(
                game_object, target_class)) {
            log_message("Synchronized refit request rejected");
        }
        return;
    }
    if (command == A2FO_REFIT_CANCEL_CLASS_COMMAND) {
        if (!cancel_refit_synchronized_command(game_object)) {
            log_message("Synchronized refit cancellation rejected");
        }
        return;
    }
    const bool fill = command == kQueueFillMarkerCommand;
    const bool continuous = command == kContinuousMarkerCommand;
    const bool extended_single = command == kExtendedQueueMarkerCommand;
    if (!fill && !continuous && !extended_single) {
        a2fo_call_thiscall_2(
            g_game_object_dequeue_class_command_hook.gateway, game_object,
            command, reinterpret_cast<std::uintptr_t>(target_class));
        return;
    }

    if (!g_repeat_ready || !game_object || !target_class) {
        log_message("Queue-enhancement marker rejected");
        return;
    }

    if (extended_single) {
        void* repeat_target = nullptr;
        if (continuous_state(game_object, &repeat_target)) {
            stop_continuous(game_object);
            if (repeat_target == target_class) {
                log_message(
                    "Continuous production disabled by repeated build order");
                return;
            }
        }
        const bool added = queue_logical_one_checked(
            game_object, target_class);
        log_message(added
            ? "Extended synchronized build order accepted"
            : "Extended synchronized build order rejected");
        return;
    }

    if (!continuous || hybrid_production_is_evolve_target(
            game_object, target_class)) {
        const std::uint32_t added =
            fill_queue_checked(game_object, target_class);
        char fill_message[96];
        std::snprintf(fill_message, sizeof(fill_message),
                      "Ctrl synchronized logical slot fill received: %lu added",
                      static_cast<unsigned long>(added));
        log_message(fill_message);
        stop_continuous(game_object);
        return;
    }

    // Continuous production is represented by one logical xINF slot,
    // not by pre-filling ten physical jobs. If the requested class is already
    // at the logical tail, promote that slot to continuous; otherwise append
    // one backing job first.
    stop_continuous(game_object);
    GroupedQueueView view = collect_grouped_queue(game_object);
    bool has_backing_job = view.count != 0 &&
        view.groups[view.count - 1].target_class == target_class;
    if (!has_backing_job) {
        has_backing_job = queue_logical_one_checked(
            game_object, target_class);
    }
    if (!has_backing_job) {
        log_message("Continuous production marker rejected: no logical slot");
        return;
    }

    try {
        const std::uint32_t handle = object_handle(game_object);
        EnterCriticalSection(&g_queue_lock);
        ContinuousState& state = g_continuous[handle];
        state.active = true;
        state.target_class = target_class;
        state.target_project_id = project_id(target_class);
        state.paused_retry_ticks = 0;
        LeaveCriticalSection(&g_queue_lock);
        log_message(
            "Continuous production enabled as one logical xINF slot");
    } catch (...) {
        LeaveCriticalSection(&g_queue_lock);
        log_message("Continuous production marker could not be recorded");
    }
}

void try_refill(void* producer) {
    if (!g_repeat_ready || !producer) return;

    // First keep the native ten-job FIFO fed from the extended sidecar. This
    // preserves the exact logical order while allowing up to 100 queued units.
    materialize_pending_queue(producer);

    const std::uint32_t handle = object_handle(producer);
    void* target_class = nullptr;
    {
        EnterCriticalSection(&g_queue_lock);
        const auto found = g_continuous.find(handle);
        if (found != g_continuous.end() && found->second.active) {
            target_class = found->second.target_class;
        }
        LeaveCriticalSection(&g_queue_lock);
    }
    if (!target_class || refit_has_waiting_job(producer)) return;

    if (hybrid_production_has_evolution_barrier(producer)) {
        stop_continuous(producer);
        return;
    }
    if (hybrid_production_has_queued_research_conflict(
            producer, target_class)) {
        return;
    }

    // xINF keeps only one real backing job. Do not pre-fill the native
    // FIFO: when the queue becomes empty the next copy is admitted.
    const PendingQueueSnapshot pending = pending_queue_snapshot(producer);
    if (native_queue_count(producer) != 0 || pending.count != 0) return;

    if (push_native_checked_once(producer, target_class, true)) {
        EnterCriticalSection(&g_queue_lock);
        const auto found = g_continuous.find(handle);
        if (found != g_continuous.end()) {
            found->second.paused_retry_ticks = 0;
        }
        LeaveCriticalSection(&g_queue_lock);
    }
}

void __attribute__((fastcall)) game_object_queue_class_command_hook(
    void* game_object, void*, std::uint32_t command,
    void* target_class) {
    if (command == kBuildCommand && target_class &&
        hybrid_production_should_defer_construct_order(
            game_object, target_class)) {
        return;
    }
    if (command == kBuildCommand && target_class) {
        retain_hybrid_research_menu_after_order(
            game_object, target_class);
    }
    if (command != kBuildCommand || !target_class || !g_repeat_ready) {
        a2fo_call_thiscall_2(
            g_game_object_queue_class_command_hook.gateway, game_object,
            command, reinterpret_cast<std::uintptr_t>(target_class));
        return;
    }

    if (!g_logged_build_order_path) {
        g_logged_build_order_path = true;
        log_message("First Producer build order reached");
    }

    if (modifier_key_down(kCommandControlPointerRva, VK_CONTROL)) {
        const bool continuous =
            modifier_key_down(kCommandAltPointerRva, VK_MENU);
        a2fo_call_thiscall_2(
            g_game_object_queue_class_command_hook.gateway, game_object,
            continuous ? kContinuousMarkerCommand : kQueueFillMarkerCommand,
            reinterpret_cast<std::uintptr_t>(target_class));
        log_message(continuous
                        ? "Ctrl+Alt synchronized continuous marker queued"
                        : "Ctrl synchronized logical-slot fill marker queued");
        return;
    }

    const PendingQueueSnapshot pending = pending_queue_snapshot(game_object);
    if (pending.count != 0 ||
        native_queue_count(game_object) >= kQueueCapacity) {
        const GroupedQueueView view = collect_grouped_queue(game_object);
        void* repeat_target = nullptr;
        const bool repeat_active = continuous_state(
            game_object, &repeat_target);
        const bool repeat_toggle = repeat_active &&
            repeat_target == target_class;
        if (!repeat_toggle &&
            !logical_queue_has_room_for(view, target_class)) {
            log_message("Logical build queue full (10 slots x 10 units)");
            return;
        }
        // The native QueueClassCommand path stops admitting build orders at
        // ten physical jobs. Route overflow through A2FO's synchronized typed
        // marker so every peer appends the same sidecar entry deterministically.
        a2fo_call_thiscall_2(
            g_game_object_queue_class_command_hook.gateway, game_object,
            kExtendedQueueMarkerCommand,
            reinterpret_cast<std::uintptr_t>(target_class));
        return;
    }

    a2fo_call_thiscall_2(
        g_game_object_queue_class_command_hook.gateway, game_object,
        command, reinterpret_cast<std::uintptr_t>(target_class));
}

void __attribute__((fastcall)) producer_command_push_hook(
    void* producer, void*, void* target_class) {
    if (producer && target_class &&
        hybrid_production_should_defer_construct_order(
            producer, target_class)) {
        return;
    }
    if (!g_repeat_ready || !producer || !target_class) {
        if (producer && target_class && !dispatch_producer_event(
                A2FO_PRODUCER_EVENT_ADMIT, producer, target_class)) {
            return;
        }
        const std::uint32_t before = producer
            ? native_queue_count(producer) : 0;
        a2fo_call_thiscall_1(g_fo_command_push_hook.gateway, producer,
                            reinterpret_cast<std::uintptr_t>(target_class));
        if (producer && target_class) {
            finalize_hybrid_construct_order(
                producer, target_class,
                native_queue_count(producer) > before);
        }
        return;
    }

    // This synchronized receiver is the common path for ordinary, hotkey,
    // Ctrl-fill, local, and remote orders. Arm the local palette adapter here
    // as well as at GameObject::QueueClassCommand so every accepted pod click
    // can survive Fleet Ops resetting menu 3 to root during its next refresh.
    retain_hybrid_research_menu_after_order(producer, target_class);
    const bool research_conflict =
        hybrid_production_has_queued_research_conflict(
            producer, target_class);
    const bool evolution_barrier =
        hybrid_production_has_evolution_barrier(producer);
    const bool evolve_target = hybrid_production_is_evolve_target(
        producer, target_class);
    bool suppress = research_conflict || evolution_barrier;
    bool repeat_toggle_off = false;
    const std::uint32_t handle = object_handle(producer);
    try {
        EnterCriticalSection(&g_queue_lock);
        const auto found = g_continuous.find(handle);
        if (!g_logged_synchronized_build_path) {
            g_logged_synchronized_build_path = true;
            log_message("First synchronized Producer build command reached");
        }
        if (found != g_continuous.end() && found->second.active) {
            if (found->second.target_class == target_class && !evolve_target) {
                repeat_toggle_off = true;
                suppress = true;
            }
            g_continuous.erase(found);
        }
        LeaveCriticalSection(&g_queue_lock);
    } catch (...) {
        LeaveCriticalSection(&g_queue_lock);
        suppress = research_conflict || evolution_barrier;
    }

    const std::uint32_t before = native_queue_count(producer);
    bool queued_native = false;
    bool queued_extended = false;
    bool logical_full = false;
    if (!suppress) {
        const GroupedQueueView view = collect_grouped_queue(producer);
        if (!logical_queue_has_room_for(view, target_class)) {
            logical_full = true;
            suppress = true;
        } else if (!dispatch_producer_event(
                A2FO_PRODUCER_EVENT_ADMIT, producer, target_class)) {
            suppress = true;
        } else {
            const PendingQueueSnapshot pending =
                pending_queue_snapshot(producer);
            if (pending.count == 0 && before < kQueueCapacity) {
                a2fo_call_thiscall_1(
                    g_fo_command_push_hook.gateway, producer,
                    reinterpret_cast<std::uintptr_t>(target_class));
                queued_native = native_queue_count(producer) > before;
            } else {
                queued_extended = append_pending_queue(
                    producer, target_class);
                suppress = !queued_extended;
            }
        }
    }

    const std::uint32_t after = native_queue_count(producer);
    finalize_hybrid_construct_order(
        producer, target_class, queued_native);
    if (g_synchronized_push_log_count < kSynchronizedPushLogLimit) {
        ++g_synchronized_push_log_count;
        char message[192];
        const char* outcome =
            research_conflict ? "research conflict rejected" :
            evolution_barrier ? "evolution barrier rejected" :
            repeat_toggle_off ? "continuous mode toggled off" :
            logical_full ? "logical 10x10 queue full" :
            queued_extended ? "accepted into extended queue" :
            queued_native ? "forwarded to native queue" :
            suppress ? "rejected" : "no native change";
        std::snprintf(
            message, sizeof(message),
            "Synchronized Producer queue result: target %lu, "
            "native %lu -> %lu (%s)",
            static_cast<unsigned long>(project_id(target_class)),
            static_cast<unsigned long>(before),
            static_cast<unsigned long>(after), outcome);
        log_message(message);
    }
}

std::uintptr_t __attribute__((fastcall)) producer_finish_hook(
    void* producer, void*) {
    void* target_class = current_build_target_class(producer);
    if (!target_class) target_class = queue_head_target_class(producer);
    const std::uint32_t queue_id = current_queue_id(producer);
    const bool use_native_completion = !producer || !target_class ||
        dispatch_producer_event(
            A2FO_PRODUCER_EVENT_FINISHING, producer, target_class);
    const std::uintptr_t result = use_native_completion
        ? a2fo_call_thiscall_0(g_fo_finish_hook.gateway, producer)
        : 0;
    if (producer && target_class) {
        dispatch_producer_event(
            A2FO_PRODUCER_EVENT_FINISHED, producer, target_class);
        notify_refit_job_finished(
            producer, queue_id, target_class,
            reinterpret_cast<void*>(result));
    }
    try_refill(producer);
    return result;
}

std::uintptr_t __attribute__((fastcall)) producer_cancel_hook(
    void* producer, void*) {
    stop_continuous(producer);
    const std::uint32_t queue_id = current_queue_id(producer);
    void* target_class = current_build_target_class(producer);
    if (target_class) {
        dispatch_producer_event(
            A2FO_PRODUCER_EVENT_CANCELLED, producer, target_class);
    }
    if (producer && queue_id != 0) {
        notify_refit_job_removed(
            producer, queue_id, target_class,
            A2FO_REFIT_QUEUE_CANCELLED);
    }
    const std::uintptr_t result =
        a2fo_call_thiscall_0(g_fo_cancel_hook.gateway, producer);
    materialize_pending_queue(producer);
    return result;
}

void delete_native_queue_item(
    void* producer, std::uint32_t queue_id) noexcept {
    if (!producer || queue_id == 0) return;
    const std::uint32_t current_id = *reinterpret_cast<std::uint32_t*>(
        bytes(producer) + kCurrentQueueIdOffset);
    void* removed_class = queued_target_class(producer, queue_id);
    if (!removed_class && queue_id == current_id) {
        removed_class = current_build_target_class(producer);
    }
    if (queue_id != current_id && charges_resources_when_queued(producer)) {
        if (removed_class) {
            dispatch_producer_event(
                A2FO_PRODUCER_EVENT_DELETED, producer, removed_class);
        }
    }
    notify_refit_job_removed(
        producer, queue_id, removed_class, A2FO_REFIT_QUEUE_DELETED);
    a2fo_call_thiscall_1(
        g_fo_act_delete_hook.gateway, producer, queue_id);
    discard_hybrid_construct_placement(producer, queue_id);
}

bool visual_slot_from_native_queue_id(
    void* producer, std::uint32_t queue_id,
    std::uint32_t* visual_slot) noexcept {
    if (!producer || queue_id == 0 || !visual_slot ||
        !readable_range(bytes(producer) + kQueueHeadOffset, sizeof(void*)) ||
        !readable_range(bytes(producer) + kCurrentBuildClassOffset,
                        sizeof(void*)) ||
        !readable_range(bytes(producer) + kCurrentQueueIdOffset,
                        sizeof(std::uint32_t))) {
        return false;
    }

    std::array<PhysicalQueueEntry, kQueueCapacity> physical{};
    std::uint32_t physical_count = 0;
    const void* active_class = current_build_target_class(producer);
    const std::uint32_t active_id = current_queue_id(producer);
    bool active_in_fifo = false;

    void* item = *reinterpret_cast<void**>(
        bytes(producer) + kQueueHeadOffset);
    while (item && physical_count < kQueueCapacity) {
        if (!readable_range(item,
                kQueueItemIdOffset + sizeof(std::uint32_t))) {
            break;
        }
        void* target_class = *reinterpret_cast<void**>(item);
        const std::uint32_t id = *reinterpret_cast<const std::uint32_t*>(
            bytes(item) + kQueueItemIdOffset);
        if (target_class) {
            physical[physical_count++] = PhysicalQueueEntry{target_class, id};
            if (active_class && active_id != 0 && id == active_id) {
                active_in_fifo = true;
            }
        }
        item = *reinterpret_cast<void**>(
            bytes(item) + kQueueItemNextOffset);
    }

    if (active_class && !active_in_fifo &&
        (active_id != 0 || physical_count == 0)) {
        const std::uint32_t limit = std::min<std::uint32_t>(
            physical_count, kQueueCapacity - 1);
        for (std::uint32_t index = limit; index > 0; --index) {
            physical[index] = physical[index - 1];
        }
        physical[0] = PhysicalQueueEntry{
            const_cast<void*>(active_class), active_id};
        physical_count = std::min<std::uint32_t>(
            physical_count + 1, kQueueCapacity);
    }

    for (std::uint32_t index = 0; index < physical_count; ++index) {
        if (physical[index].queue_id == queue_id) {
            *visual_slot = index;
            return true;
        }
    }
    return false;
}

void trim_group_to_single(
    void* producer, std::uint32_t visual_slot) noexcept {
    if (!producer) return;
    for (std::uint32_t guard = 0; guard < kUnitsPerVisibleSlot; ++guard) {
        const GroupedQueueView view = collect_grouped_queue(producer);
        if (visual_slot >= view.count) return;
        const GroupedQueueEntry group = view.groups[visual_slot];
        if (group.count <= 1) return;
        if (group.pending_count != 0 &&
            group.last_pending_index != 0xffffffffu) {
            if (!erase_pending_queue_index(
                    producer, group.last_pending_index)) {
                return;
            }
            continue;
        }
        const std::uint32_t current_id = current_queue_id(producer);
        if (group.last_queue_id != 0 &&
            group.last_queue_id != current_id) {
            delete_native_queue_item(producer, group.last_queue_id);
            continue;
        }
        return;
    }
}

void __attribute__((fastcall)) producer_act_delete_hook(
    void* producer, void*, std::uint32_t queue_id) {
    std::uint32_t requested_slot = 0xffffffffu;
    bool have_requested_slot = false;
    if (g_grouped_queue_ui_ready &&
        g_grouped_queue_ui_context.active &&
        g_grouped_queue_ui_context.producer == producer) {
        requested_slot = g_grouped_queue_ui_context.visual_slot;
        have_requested_slot = true;
    } else if (g_grouped_queue_ui_ready) {
        // Some Fleet Ops queue-icon paths defer the delete callback until
        // after BuildQueueIcon::Simulate returns. In that case the temporary
        // visual-slot context is gone, but the stock queue_id still belongs
        // to the native icon index that was clicked. Recover that index and
        // treat it as the grouped visual slot before remapping the deletion.
        have_requested_slot = visual_slot_from_native_queue_id(
            producer, queue_id, &requested_slot);
        if (have_requested_slot &&
            !g_logged_grouped_delete_slot_fallback) {
            g_logged_grouped_delete_slot_fallback = true;
            log_message("Grouped queue cancellation recovered the clicked "
                        "visual slot from the native icon index");
        }
    }

    if (have_requested_slot) {
        const GroupedQueueView view = collect_grouped_queue(producer);
        const std::uint32_t slot = requested_slot;
        if (slot < view.count) {
            const GroupedQueueEntry group = view.groups[slot];
            if (group.infinite) {
                // Selecting xINF for removal disables repeat but keeps
                // exactly one ordinary build of that class in the queue.
                trim_group_to_single(producer, slot);
                stop_continuous(producer);
                log_message(
                    "Continuous queue slot removed: xINF reverted "
                    "to one normal build");
                return;
            }
            if (group.pending_count != 0 &&
                group.last_pending_index != 0xffffffffu) {
                erase_pending_queue_index(
                    producer, group.last_pending_index);
                if (!g_logged_grouped_delete_remap) {
                    g_logged_grouped_delete_remap = true;
                    log_message(
                        "Grouped queue cancellation removed the last "
                        "extended item in the visible run");
                }
                return;
            }
            if (group.last_queue_id != 0 &&
                group.last_queue_id != queue_id) {
                queue_id = group.last_queue_id;
                if (!g_logged_grouped_delete_remap) {
                    g_logged_grouped_delete_remap = true;
                    log_message(
                        "Grouped queue cancellation remapped to the last "
                        "native item in the visible run");
                }
            }
        }
    }
    // Do not stop continuous production just because an earlier finite slot
    // was cancelled. The xINF slot is an independent logical tail and is only
    // disabled by explicitly selecting that infinite slot above.
    delete_native_queue_item(producer, queue_id);
    materialize_pending_queue(producer);
}

void __attribute__((fastcall)) producer_clear_hook(void* producer, void*) {
    stop_continuous(producer);
    clear_pending_queue(producer);
    const bool charge_at_queue = charges_resources_when_queued(producer);
    const std::uint32_t active_id = current_queue_id(producer);
    bool active_notified = false;
    void* item = producer ? *reinterpret_cast<void**>(
        bytes(producer) + kQueueHeadOffset) : nullptr;
    while (item) {
        void* target_class = *reinterpret_cast<void**>(item);
        const std::uint32_t queue_id =
            *reinterpret_cast<const std::uint32_t*>(
                bytes(item) + kQueueItemIdOffset);
        if (target_class) {
            if (charge_at_queue) {
                dispatch_producer_event(
                    A2FO_PRODUCER_EVENT_CLEARED,
                    producer, target_class);
            }
            notify_refit_job_removed(
                producer, queue_id, target_class,
                A2FO_REFIT_QUEUE_CLEARED);
            if (queue_id == active_id) active_notified = true;
        }
        item = *reinterpret_cast<void**>(
            bytes(item) + kQueueItemNextOffset);
    }
    if (!charge_at_queue) {
        // Fleet Operations' non-queue-charge branch refunds only the active
        // class directly; it does not route through its Cancel callback.
        if (void* target_class = current_build_target_class(producer)) {
            dispatch_producer_event(
                A2FO_PRODUCER_EVENT_CLEARED, producer, target_class);
            if (!active_notified) {
                notify_refit_job_removed(
                    producer, active_id, target_class,
                    A2FO_REFIT_QUEUE_CLEARED);
                active_notified = true;
            }
        }
    }
    if (producer && active_id != 0 && !active_notified) {
        // The active target pointer can be cleared before Clear reaches this
        // hook. Preserve the queue-ID removal notification so optional refit
        // state (including its collision restore) cannot be orphaned.
        notify_refit_job_removed(
            producer, active_id,
            current_build_target_class(producer),
            A2FO_REFIT_QUEUE_CLEARED);
    }
    a2fo_call_thiscall_0(g_fo_clear_hook.gateway, producer);
    clear_hybrid_construct_placements(producer);
}

void __attribute__((fastcall)) producer_simulate_hook(
    void* producer, void*, std::uint32_t delta_bits) {
    a2fo_call_thiscall_1(g_producer_simulate_hook.gateway, producer,
                        delta_bits);
    if (!g_repeat_ready || !producer) return;

    if (native_queue_count(producer) < kQueueCapacity) {
        const PendingQueueSnapshot pending = pending_queue_snapshot(producer);
        if (pending.count != 0) materialize_pending_queue(producer);
    }

    bool retry = false;
    const std::uint32_t handle = object_handle(producer);
    EnterCriticalSection(&g_queue_lock);
    const auto found = g_continuous.find(handle);
    if (found != g_continuous.end()) {
        ContinuousState& state = found->second;
        if (state.active) {
            const std::uint32_t count = *reinterpret_cast<std::uint32_t*>(
                bytes(producer) + kQueueCountOffset);
            if (count < kQueueCapacity &&
                ++state.paused_retry_ticks >= kPausedRetryTicks) {
                state.paused_retry_ticks = 0;
                retry = true;
            }
        }
    }
    LeaveCriticalSection(&g_queue_lock);
    if (retry) try_refill(producer);
}

std::uintptr_t __attribute__((fastcall)) producer_dtor_hook(
    void* producer, void*) {
    if (producer) {
        dispatch_producer_event(
            A2FO_PRODUCER_EVENT_DESTROYING, producer, nullptr);
        notify_refit_job_removed(
            producer, 0, nullptr,
            A2FO_REFIT_QUEUE_PRODUCER_DESTROYED);
    }
    stop_continuous(producer);
    clear_pending_queue(producer);
    return a2fo_call_thiscall_0(g_producer_dtor_hook.gateway, producer);
}

void reconstruct_queue_ids(void* producer) {
    auto* producer_bytes = bytes(producer);
    void* item = *reinterpret_cast<void**>(
        producer_bytes + kQueueHeadOffset);
    std::uint32_t current_id = 0;
    std::uint32_t maximum_id = 0;
    if (item) {
        current_id = *reinterpret_cast<std::uint32_t*>(
            bytes(item) + kQueueItemIdOffset);
    }
    unsigned visited = 0;
    while (item && visited++ < kQueueCapacity) {
        maximum_id = std::max(
            maximum_id,
            *reinterpret_cast<std::uint32_t*>(
                bytes(item) + kQueueItemIdOffset));
        item = *reinterpret_cast<void**>(bytes(item) + kQueueItemNextOffset);
    }
    *reinterpret_cast<std::uint32_t*>(
        producer_bytes + kCurrentQueueIdOffset) = current_id;
    *reinterpret_cast<std::uint32_t*>(
        producer_bytes + kNextQueueIdOffset) =
        maximum_id == 0xffffffffu ? 1u : std::max(1u, maximum_id + 1u);
}

std::uintptr_t __attribute__((fastcall)) producer_save_hook(
    void* producer, void*, void* writer) {
    if (!g_repeat_ready || !producer) {
        return a2fo_call_thiscall_1(
            g_producer_save_hook.gateway, producer,
            reinterpret_cast<std::uintptr_t>(writer));
    }

    bool active = false;
    std::uint32_t target_project_id = 0;
    const std::uint32_t handle = object_handle(producer);
    EnterCriticalSection(&g_queue_lock);
    const auto found = g_continuous.find(handle);
    if (found != g_continuous.end() && found->second.active) {
        active = true;
        target_project_id = found->second.target_project_id;
    }
    LeaveCriticalSection(&g_queue_lock);

    auto* producer_bytes = bytes(producer);
    auto* current_id = reinterpret_cast<std::uint32_t*>(
        producer_bytes + kCurrentQueueIdOffset);
    auto* next_id = reinterpret_cast<std::uint32_t*>(
        producer_bytes + kNextQueueIdOffset);
    const std::uint32_t saved_current_id = *current_id;
    const std::uint32_t saved_next_id = *next_id;
    if (active) {
        *current_id = kRepeatSaveMarker;
        *next_id = target_project_id;
    }
    const std::uintptr_t result = a2fo_call_thiscall_1(
        g_producer_save_hook.gateway, producer,
        reinterpret_cast<std::uintptr_t>(writer));
    *current_id = saved_current_id;
    *next_id = saved_next_id;
    return result;
}

std::uintptr_t __attribute__((fastcall)) producer_load_hook(
    void* producer, void*, void* reader) {
    const std::uintptr_t result = a2fo_call_thiscall_1(
        g_producer_load_hook.gateway, producer,
        reinterpret_cast<std::uintptr_t>(reader));
    if (producer) clear_pending_queue(producer);
    if (!g_repeat_ready || !producer || result == 0) return result;

    auto* producer_bytes = bytes(producer);
    const std::uint32_t marker = *reinterpret_cast<std::uint32_t*>(
        producer_bytes + kCurrentQueueIdOffset);
    if (marker != kRepeatSaveMarker) return result;
    const std::uint32_t target_project_id =
        *reinterpret_cast<std::uint32_t*>(
            producer_bytes + kNextQueueIdOffset);
    reconstruct_queue_ids(producer);

    using FindByProjectIdFunction = void* (A2FO_CALL*)(std::uint32_t);
    const auto find_by_project_id =
        reinterpret_cast<FindByProjectIdFunction>(
            at(g_armada, kGameObjectClassFindByProjectIdRva));
    void* target_class = find_by_project_id(target_project_id);
    if (!target_class) {
        stop_continuous(producer);
        return result;
    }

    try {
        const std::uint32_t handle = object_handle(producer);
        EnterCriticalSection(&g_queue_lock);
        ContinuousState& state = g_continuous[handle];
        state.active = true;
        state.target_class = target_class;
        state.target_project_id = target_project_id;
        state.paused_retry_ticks = 0;
        LeaveCriticalSection(&g_queue_lock);
    } catch (...) {
        LeaveCriticalSection(&g_queue_lock);
    }
    return result;
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) {
    return module &&
        std::memcmp(at(module, rva), expected, Size) == 0;
}

template <std::size_t Size>
bool require_signature(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size],
                       const char* name) {
    if (signature_matches(module, rva, expected)) return true;
    log_message("Continuous production signature mismatch at:");
    log_message(name);
    return false;
}

template <std::size_t Size>
bool install_hook(HMODULE module, std::uintptr_t rva, void* replacement,
                  const std::uint8_t (&expected)[Size],
                  A2FO_InlineHook& hook) {
    return g_api->install_inline_hook(
        at(module, rva), replacement, Size, expected, &hook);
}

bool install_repeat_hooks() {
    bool signatures_ok = true;
    signatures_ok &= require_signature(
        g_armada, kGameObjectDequeueClassCommandRva,
        kExpectedGameObjectDequeueClassCommand,
        "Armada synchronized typed-class order receive");
    signatures_ok &= require_signature(
        g_armada, kProducerDtorRva, kExpectedProducerDtor,
        "Armada Producer destructor");
    signatures_ok &= require_signature(
        g_armada, kProducerSimulateRva, kExpectedProducerSimulate,
        "Armada Producer simulate");
    signatures_ok &= require_signature(
        g_armada, kProducerLoadRva, kExpectedProducerLoad,
        "Armada Producer load");
    signatures_ok &= require_signature(
        g_armada, kProducerSaveRva, kExpectedProducerSave,
        "Armada Producer save");
    signatures_ok &= require_signature(
        g_armada, kGameObjectClassFindByProjectIdRva,
        kExpectedFindByProjectId, "Armada class lookup by project ID");
    signatures_ok &= require_signature(
        g_fleet_ops, kFoProducerFinishRva, kExpectedFoFinish,
        "Fleet Ops Producer finish callback");
    signatures_ok &= require_signature(
        g_fleet_ops, kFoProducerCancelRva, kExpectedFoCancel,
        "Fleet Ops Producer cancel callback");
    signatures_ok &= require_signature(
        g_fleet_ops, kFoProducerPushCheckedRva, kExpectedFoPushChecked,
        "Fleet Ops checked queue push");
    signatures_ok &= require_signature(
        g_fleet_ops, kFoProducerCommandPushRva, kExpectedFoCommandPush,
        "Fleet Ops build-command push");
    signatures_ok &= require_signature(
        g_fleet_ops, kFoProducerActDeleteRva, kExpectedFoActDelete,
        "Fleet Ops queue-item delete callback");
    signatures_ok &= require_signature(
        g_fleet_ops, kFoProducerClearRva, kExpectedFoClear,
        "Fleet Ops queue clear callback");
    if (!signatures_ok) {
        log_message("Continuous production signatures mismatch; disabled");
        return false;
    }

    const bool installed =
        install_hook(g_armada, kGameObjectDequeueClassCommandRva,
                     reinterpret_cast<void*>(
                         &game_object_dequeue_class_command_hook),
                     kExpectedGameObjectDequeueClassCommand,
                     g_game_object_dequeue_class_command_hook) &&
        install_hook(g_armada, kProducerDtorRva,
                     reinterpret_cast<void*>(&producer_dtor_hook),
                     kExpectedProducerDtor, g_producer_dtor_hook) &&
        install_hook(g_armada, kProducerSimulateRva,
                     reinterpret_cast<void*>(&producer_simulate_hook),
                     kExpectedProducerSimulate, g_producer_simulate_hook) &&
        install_hook(g_armada, kProducerLoadRva,
                     reinterpret_cast<void*>(&producer_load_hook),
                     kExpectedProducerLoad, g_producer_load_hook) &&
        install_hook(g_armada, kProducerSaveRva,
                     reinterpret_cast<void*>(&producer_save_hook),
                     kExpectedProducerSave, g_producer_save_hook) &&
        install_hook(g_fleet_ops, kFoProducerFinishRva,
                     reinterpret_cast<void*>(&producer_finish_hook),
                     kExpectedFoFinish, g_fo_finish_hook) &&
        install_hook(g_fleet_ops, kFoProducerCancelRva,
                     reinterpret_cast<void*>(&producer_cancel_hook),
                     kExpectedFoCancel, g_fo_cancel_hook) &&
        install_hook(g_fleet_ops, kFoProducerCommandPushRva,
                     reinterpret_cast<void*>(&producer_command_push_hook),
                     kExpectedFoCommandPush, g_fo_command_push_hook) &&
        install_hook(g_fleet_ops, kFoProducerActDeleteRva,
                     reinterpret_cast<void*>(&producer_act_delete_hook),
                     kExpectedFoActDelete, g_fo_act_delete_hook) &&
        install_hook(g_fleet_ops, kFoProducerClearRva,
                     reinterpret_cast<void*>(&producer_clear_hook),
                     kExpectedFoClear, g_fo_clear_hook);
    if (!installed) {
        log_message("Continuous production hook installation incomplete; "
                    "feature disabled safely");
        return false;
    }
    return true;
}

}  // namespace

bool initialize_queue_enhancements(const A2FO_ModuleApi* api,
                                   HMODULE armada,
                                   HMODULE fleet_ops) noexcept {
    g_api = api;
    g_armada = armada;
    g_fleet_ops = fleet_ops;
    if (!g_api || !g_api->install_inline_hook || !g_armada || !g_fleet_ops) {
        return false;
    }

    InitializeCriticalSection(&g_queue_lock);
    g_queue_lock_ready = true;
    try {
        g_continuous.reserve(256);
        g_pending_queue.reserve(256);
    } catch (...) {
        log_message("Queue state allocation failed; enhancements disabled");
        return false;
    }

    if (!signature_matches(g_armada, kGameObjectQueueClassCommandRva,
                           kExpectedGameObjectQueueClassCommand) ||
        !install_hook(g_armada, kGameObjectQueueClassCommandRva,
                      reinterpret_cast<void*>(
                          &game_object_queue_class_command_hook),
                      kExpectedGameObjectQueueClassCommand,
                      g_game_object_queue_class_command_hook)) {
        log_message("Ctrl-fill queue hook signature mismatch; disabled");
        return false;
    }
    log_message("Ctrl-click queue fill enabled (10 units per logical slot)");

    g_repeat_ready = install_repeat_hooks();
    if (g_repeat_ready) {
        log_message("Ctrl+Alt continuous production enabled as xINF "
                    "with synchronized orders and save markers");
        g_grouped_queue_ui_ready = install_grouped_queue_ui();
        if (g_grouped_queue_ui_ready) {
            log_message(
                "Grouped build queue enabled: 10 visible slots x 10 units "
                "(100 logical jobs), with xN/xINF counters");
        } else {
            log_message(
                "Grouped build queue presentation unavailable; native "
                "ten-slot display retained");
        }
    }
    return true;
}

extern "C" __declspec(dllexport)
std::uint32_t __cdecl A2FO_ProducerPushRefit(
    void* producer, void* target_class) {
    return push_refit_checked(producer, target_class);
}

extern "C" __declspec(dllexport)
bool __cdecl A2FO_ProducerCancelQueuedJob(void* producer, std::uint32_t queue_id) {
    if (!g_repeat_ready || !producer || !queue_id ||
        !g_fo_act_delete_hook.gateway || !queued_target_class(producer, queue_id))
        return false;
    // Calling producer_act_delete_hook here would interpret the ID as a GUI
    // slot and could cancel a different ship in a grouped queue.
    delete_native_queue_item(producer, queue_id);
    materialize_pending_queue(producer);
    return true;
}

// HybridBuild owns the checked Producer::IsBusy / replacement-pop gates used
// by CraftProcess before a build order reaches QueueClassCommand. Expose only
// a read-only admission hint so HybridBuild can let an 11th+ synchronized
// order reach FeaturePack's sidecar without ever increasing Armada's native
// ten-node FIFO. The target-specific decision still happens later in
// logical_queue_has_room_for().
bool producer_logical_queue_has_room(void* producer) noexcept {
    if (!g_repeat_ready || !producer) return false;
    const GroupedQueueView view = collect_grouped_queue(producer);
    if (logical_queue_units(view) >= kMaxLogicalQueue) return false;
    if (view.count < kVisibleQueueSlots) return true;
    if (view.count == 0) return true;
    const GroupedQueueEntry& tail = view.groups[view.count - 1];
    return !tail.infinite && tail.count < kUnitsPerVisibleSlot;
}

// Keep the export for compatibility with intermediate V3/V4 HybridBuild
// binaries. V5 passes this same function pointer directly through the existing
// HybridBridge and no longer depends on GetProcAddress succeeding.
extern "C" __declspec(dllexport)
bool __cdecl A2FO_ProducerLogicalQueueHasRoom(void* producer) {
    return producer_logical_queue_has_room(producer);
}

// Target-specific admission query for the Build palette. Once all ten visual
// slots exist, only the class already occupying slot ten may grow that slot,
// and only until its count reaches ten. HybridBuild uses this read-only query
// to gray out build choices that cannot be represented without creating an
// eleventh visible slot.
extern "C" __declspec(dllexport)
bool __cdecl A2FO_ProducerCanQueueExtendedBuild(
    void* producer, void* target_class) {
    if (!g_repeat_ready || !producer || !target_class) return false;
    const GroupedQueueView view = collect_grouped_queue(producer);
    return logical_queue_has_room_for(view, target_class);
}

// UI-side overflow admission. Fleet Ops can reject a normal build-button press
// before QueueClassCommand reaches FeaturePack once the physical FIFO is 10/10.
// HybridBuild calls this export only for that full-native-queue case. Reuse the
// same synchronized typed-class marker as FeaturePack's local hook so multiplayer
// order remains deterministic and the actual native FIFO never exceeds ten.
extern "C" __declspec(dllexport)
bool __cdecl A2FO_ProducerQueueExtendedBuild(
    void* producer, void* target_class) {
    if (!g_repeat_ready || !producer || !target_class ||
        native_queue_count(producer) < kQueueCapacity) {
        return false;
    }

    const GroupedQueueView view = collect_grouped_queue(producer);
    const bool control = modifier_key_down(
        kCommandControlPointerRva, VK_CONTROL);
    const bool continuous = control && modifier_key_down(
        kCommandAltPointerRva, VK_MENU);

    std::uint32_t marker = kExtendedQueueMarkerCommand;
    const char* marker_name = "single";
    if (continuous) {
        if (!logical_queue_can_enable_continuous(view, target_class)) {
            return false;
        }
        marker = kContinuousMarkerCommand;
        marker_name = "continuous";
    } else if (control) {
        if (!logical_queue_has_room_for(view, target_class)) return false;
        marker = kQueueFillMarkerCommand;
        marker_name = "fill-to-10";
    } else if (!logical_queue_has_room_for(view, target_class)) {
        return false;
    }

    a2fo_call_thiscall_2(
        g_game_object_queue_class_command_hook.gateway, producer, marker,
        reinterpret_cast<std::uintptr_t>(target_class));
    if (!g_logged_direct_extended_button_order) {
        g_logged_direct_extended_button_order = true;
        char message[160];
        std::snprintf(message, sizeof(message),
                      "Direct overflow-button order preserved %s modifier "
                      "semantics and emitted synchronized queue marker",
                      marker_name);
        log_message(message);
    }
    return true;
}

}  // namespace a2fo
