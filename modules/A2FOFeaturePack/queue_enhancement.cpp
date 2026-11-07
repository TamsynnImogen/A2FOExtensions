/*
 * File: modules/A2FOFeaturePack/queue_enhancement.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Producer queue filling, continuous production, and save markers.
 */

#include "queue_enhancement.hpp"

#include "hybrid_bridge_client.hpp"
#include "refit_queue_bridge_api.hpp"
#include "refit_queue_bridge_client.hpp"
#include "../../rust/stage8_c_queue_ffi.h"
#include "../A2FOCollectiveXP/queue_resolver.hpp"
#include "../../sdk/include/a2fo_module_provider.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
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

void* resolve_build_queue_class(void* producer, void* base_class) noexcept {
    using collective_xp::ResolveBuildClassFn;
    static std::atomic<ResolveBuildClassFn> cached{nullptr};
    ResolveBuildClassFn resolver = cached.load(std::memory_order_acquire);
    if (!resolver) {
        // Module initialization may follow FeaturePack. Retry missing lookups,
        // but retain a successful export lookup for subsequent admissions.
        HMODULE module = a2fo::module_provider(
            "A2FOCollectiveXP.dll",
            {"A2FOCollectiveXP_ResolveBuildClass",
             "A2FOCollectiveXP_IsEnabledForObject"});
        if (module) {
            resolver = reinterpret_cast<ResolveBuildClassFn>(
                reinterpret_cast<void*>(GetProcAddress(
                    module, "A2FOCollectiveXP_ResolveBuildClass")));
            if (resolver) cached.store(resolver, std::memory_order_release);
        }
    }
    return collective_xp::resolve_queue_class(producer, base_class, resolver);
}

bool collective_queue_may_change_class(void* producer) noexcept {
    // A read-only hint only. Never load rank classes or resolve a command on
    // the local sender: the synchronized receiver owns admission and pricing.
    using Enabled = bool (__cdecl*)(void*);
    HMODULE module = a2fo::module_provider(
        "A2FOCollectiveXP.dll",
        {"A2FOCollectiveXP_ResolveBuildClass",
         "A2FOCollectiveXP_IsEnabledForObject"});
    auto enabled = module ? reinterpret_cast<Enabled>(reinterpret_cast<void*>(
        GetProcAddress(module, "A2FOCollectiveXP_IsEnabledForObject"))) : nullptr;
    return enabled && enabled(producer);
}

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
    struct Context {void* producer;void* item{};} context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:c.item=*reinterpret_cast<void**>(
            bytes(c.producer)+kQueueHeadOffset);
            return reinterpret_cast<std::uintptr_t>(c.item);
        case 1:return *reinterpret_cast<std::uint32_t*>(
            bytes(c.item)+kQueueItemIdOffset);
        case 2:return reinterpret_cast<std::uintptr_t>(
            *reinterpret_cast<void**>(c.item));
        case 3:c.item=*reinterpret_cast<void**>(
            bytes(c.item)+kQueueItemNextOffset);
            return reinterpret_cast<std::uintptr_t>(c.item);
        default:return 0;
        }
    };
    return reinterpret_cast<void*>(a2fo_rs_c_queue_find_target(&context,
        reinterpret_cast<std::uintptr_t>(producer),queue_id,step));
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
    struct Context {
        void* producer;
        std::uint32_t handle{};
        decltype(g_pending_queue)::iterator found{};
        PendingQueueSnapshot snapshot{};
        std::uint8_t failed{};
    } context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t value) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:c.handle=object_handle(c.producer);return c.handle;
        case 1:EnterCriticalSection(&g_queue_lock);
            c.found=g_pending_queue.find(c.handle);
            return c.found==g_pending_queue.end()?0:c.found->second.size();
        case 2:c.snapshot.count=static_cast<std::uint32_t>(value);return 1;
        case 3:c.snapshot.entries[value]=c.found->second[value];return 1;
        case 4:LeaveCriticalSection(&g_queue_lock);return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    a2fo_rs_c_queue_snapshot_pending(&context,
        reinterpret_cast<std::uintptr_t>(producer),g_queue_lock_ready,
        step,&context.failed);
    if(context.failed)std::terminate();
    return context.snapshot;
}

bool append_pending_queue(void* producer, void* target_class) noexcept {
    struct Context {
        void* producer;
        void* target;
        std::uint32_t handle{};
        std::vector<void*>* pending{};
    } context{producer,target_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:c.handle=object_handle(c.producer);return c.handle;
        case 1:try{
            EnterCriticalSection(&g_queue_lock);
            c.pending=&g_pending_queue[c.handle];
            return c.pending->size();
            }catch(...){LeaveCriticalSection(&g_queue_lock);
                return UINTPTR_MAX;}
        case 2:try{c.pending->push_back(c.target);
            LeaveCriticalSection(&g_queue_lock);return 1;
            }catch(...){LeaveCriticalSection(&g_queue_lock);return 0;}
        case 3:LeaveCriticalSection(&g_queue_lock);return 1;
        default:return 0;
        }
    };
    return a2fo_rs_c_queue_append_pending(&context,
        reinterpret_cast<std::uintptr_t>(producer),
        reinterpret_cast<std::uintptr_t>(target_class),g_queue_lock_ready,
        step);
}

bool erase_pending_queue_index(void* producer, std::uint32_t index) noexcept {
    struct Context {
        void* producer;
        std::uint32_t handle{};
        decltype(g_pending_queue)::iterator found{};
        std::uint8_t failed{};
    } context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t value) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:c.handle=object_handle(c.producer);return c.handle;
        case 1:EnterCriticalSection(&g_queue_lock);
            c.found=g_pending_queue.find(c.handle);
            return c.found==g_pending_queue.end()?0:c.found->second.size();
        case 2:c.found->second.erase(c.found->second.begin()+value);
            return 1;
        case 3:return c.found->second.empty();
        case 4:g_pending_queue.erase(c.found);return 1;
        case 5:LeaveCriticalSection(&g_queue_lock);return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    const bool removed=a2fo_rs_c_queue_erase_pending(&context,
        reinterpret_cast<std::uintptr_t>(producer),index,
        g_queue_lock_ready,step,&context.failed);
    if(context.failed)std::terminate();
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

    struct Context {
        void* producer;
        void* item{};
        PendingQueueSnapshot pending{};
        void* repeat_target{};
    } context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t arg) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:return reinterpret_cast<std::uintptr_t>(
            current_build_target_class(c.producer));
        case 1:return current_queue_id(c.producer);
        case 2:c.item=*reinterpret_cast<void**>(
            bytes(c.producer)+kQueueHeadOffset);
            return reinterpret_cast<std::uintptr_t>(c.item);
        case 3:return readable_range(c.item,
            kQueueItemIdOffset+sizeof(std::uint32_t));
        case 4:return reinterpret_cast<std::uintptr_t>(
            *reinterpret_cast<void**>(c.item));
        case 5:return *reinterpret_cast<const std::uint32_t*>(
            bytes(c.item)+kQueueItemIdOffset);
        case 6:c.item=*reinterpret_cast<void**>(
            bytes(c.item)+kQueueItemNextOffset);
            return reinterpret_cast<std::uintptr_t>(c.item);
        case 7:c.pending=pending_queue_snapshot(c.producer);
            return c.pending.count;
        case 8:return reinterpret_cast<std::uintptr_t>(c.pending.entries[arg]);
        case 9:return continuous_state(c.producer,&c.repeat_target);
        case 10:return reinterpret_cast<std::uintptr_t>(c.repeat_target);
        default:return 0;
        }
    };
    A2foStage8QueueView view{};
    if(!a2fo_rs_c_queue_collect(&context,step,kQueueCapacity,
            kVisibleQueueSlots,kUnitsPerVisibleSlot,&view)){
        return result;
    }
    result.count=view.count;
    for(std::uint32_t index=0;index<view.count;++index){
        const auto& source=view.groups[index];
        auto& group=result.groups[index];
        group.target_class=reinterpret_cast<void*>(source.target);
        group.count=source.count;
        group.physical_count=source.physical_count;
        group.pending_count=source.pending_count;
        group.first_queue_id=source.first_queue_id;
        group.last_queue_id=source.last_queue_id;
        group.first_pending_index=source.first_pending_index;
        group.last_pending_index=source.last_pending_index;
        group.infinite=source.infinite;
    }
    return result;
}

struct QueueViewReadContext { const GroupedQueueView& view; };
std::uintptr_t queue_view_step(void* opaque,std::uint32_t op,
                               std::size_t index) noexcept {
    const auto& view=static_cast<QueueViewReadContext*>(opaque)->view;
    switch(op){
    case 0:return view.count;
    case 1:return view.groups[index].count;
    case 2:return view.groups[view.count-1].infinite;
    case 3:return reinterpret_cast<std::uintptr_t>(
        view.groups[view.count-1].target_class);
    case 4:return view.groups[view.count-1].count;
    default:return 0;
    }
}

std::uint32_t logical_queue_units(const GroupedQueueView& view) noexcept {
    QueueViewReadContext context{view};
    return a2fo_rs_c_queue_units(&context,queue_view_step);
}

bool logical_queue_has_room_for(
    const GroupedQueueView& view, void* target_class) noexcept {
    QueueViewReadContext context{view};
    return a2fo_rs_c_queue_room(&context,
        reinterpret_cast<std::uintptr_t>(target_class),queue_view_step);
}

bool logical_queue_can_enable_continuous(
    const GroupedQueueView& view, void* target_class) noexcept {
    QueueViewReadContext context{view};
    return a2fo_rs_c_queue_continuous_room(&context,
        reinterpret_cast<std::uintptr_t>(target_class),queue_view_step);
}

bool grouped_queue_icon_context(
    void* icon, void** ship_display, void** producer,
    std::uint32_t* visual_slot) noexcept {
    struct Context {
        void* icon;
        void** ship_display;
        void** producer;
        std::uint32_t* visual_slot;
        void* parent{};
        void* selected{};
    } context{icon,ship_display,producer,visual_slot};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t index) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:return readable_range(
            bytes(c.icon)+kDisplayComponentParentOffset,sizeof(void*));
        case 1:c.parent=read_ui_value<void*>(
            c.icon,kDisplayComponentParentOffset,nullptr);
            return reinterpret_cast<std::uintptr_t>(c.parent);
        case 2:return readable_range(
            bytes(c.parent)+kShipDisplayBuildQueueOffset,
            kQueueCapacity*sizeof(void*));
        case 3:return readable_range(
            bytes(c.parent)+kShipDisplaySelectedObjectOffset,sizeof(void*));
        case 4:return reinterpret_cast<std::uintptr_t>(
            read_ui_value<void*>(c.parent,
                kShipDisplayBuildQueueOffset+index*sizeof(void*),nullptr));
        case 5:c.selected=read_ui_value<void*>(c.parent,
            kShipDisplaySelectedObjectOffset,nullptr);
            return reinterpret_cast<std::uintptr_t>(c.selected);
        case 6:*c.ship_display=c.parent;
            *c.producer=c.selected;
            *c.visual_slot=static_cast<std::uint32_t>(index);
            return 1;
        default:return 0;
        }
    };
    return a2fo_rs_c_queue_icon_context(&context,
        reinterpret_cast<std::uintptr_t>(icon),
        reinterpret_cast<std::uintptr_t>(ship_display),
        reinterpret_cast<std::uintptr_t>(producer),
        reinterpret_cast<std::uintptr_t>(visual_slot),step);
}

void* queue_count_text_component(void* ship_display) noexcept {
    struct Context {void* display;void* component{};}
        context{ship_display};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t index) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0: {
            constexpr std::size_t offsets[]={
                kShipDisplayBuildNameOffset,kShipDisplayBuildClassOffset,
                kShipDisplayNormalNameOffset};
            c.component=read_ui_value<void*>(
                c.display,offsets[index],nullptr);
            return reinterpret_cast<std::uintptr_t>(c.component);
        }
        case 1:return readable_range(
            bytes(c.component)+kTextComponentFontStateOffset,12);
        default:return 0;
        }
    };
    return reinterpret_cast<void*>(a2fo_rs_c_queue_text_component(
        &context,reinterpret_cast<std::uintptr_t>(ship_display),step));
}

bool draw_group_count(void* icon, void* ship_display,
                      std::uint32_t count, bool infinite) noexcept {
    struct Context {
        void* icon;
        void* ship_display;
        std::uint32_t count;
        bool infinite;
        A2foStage8QueueRect rect{};
        void* text_component{};
        void* display_interface{};
        void* display_slot{};
        void* display_override{};
        std::int32_t text_flags{};
        std::uint8_t constrain{};
        QueueUiColour colour{};
        void* font_state{};
    } context{icon,ship_display,count,infinite};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:return g_armada&&c.icon&&c.ship_display&&
            signature_matches(g_armada,
                kDisplayInterfaceDrawTextInRectangleRva,
                kExpectedDisplayInterfaceDrawTextInRectangle);
        case 1:c.text_component=queue_count_text_component(c.ship_display);
            return reinterpret_cast<std::uintptr_t>(c.text_component);
        case 2: {
            const QueueUiRectangle rectangle=read_ui_value<QueueUiRectangle>(
                c.icon,kDisplayComponentRectangleOffset,QueueUiRectangle{});
            c.rect={rectangle.left,rectangle.top,rectangle.right,
                rectangle.bottom};return 1;
        }
        case 3:c.display_interface=read_ui_value<void*>(
            c.text_component,kDisplayComponentParentOffset,nullptr);
            return reinterpret_cast<std::uintptr_t>(c.display_interface);
        case 4:c.display_slot=read_ui_value<void*>(c.text_component,
            kTextComponentDisplayOverrideSlotOffset,nullptr);
            return reinterpret_cast<std::uintptr_t>(c.display_slot);
        case 5:c.display_override=read_ui_value<void*>(
            c.display_slot,0,nullptr);return 1;
        case 6:c.text_flags=read_ui_value<std::int32_t>(c.text_component,
            kTextComponentFlagsOffset,9);return 1;
        case 7:c.constrain=read_ui_value<std::uint8_t>(c.text_component,
            kTextComponentConstrainOffset,0);return 1;
        case 8:c.colour=read_ui_value<QueueUiColour>(c.text_component,
            kTextComponentColourOffset,QueueUiColour{1.0f,1.0f,1.0f});
            return 1;
        case 9:c.font_state=bytes(c.text_component)+kTextComponentFontStateOffset;
            return readable_range(c.font_state,12);
        case 10: {
            QueueUiRectangle rectangle{c.rect.left,c.rect.top,c.rect.right,
                c.rect.bottom};
            char label[16]{};
            if(c.infinite){
                std::snprintf(label,sizeof(label),"xINF");
            }else{
                std::snprintf(label,sizeof(label),"x%lu",
                    static_cast<unsigned long>(c.count));
            }
            a2fo_call_thiscall_7(
                at(g_armada,kDisplayInterfaceDrawTextInRectangleRva),
                c.display_interface,
                reinterpret_cast<std::uintptr_t>(label),
                reinterpret_cast<std::uintptr_t>(&rectangle),
                static_cast<std::uintptr_t>(c.text_flags),
                reinterpret_cast<std::uintptr_t>(&c.colour),
                reinterpret_cast<std::uintptr_t>(c.display_override),
                static_cast<std::uintptr_t>(c.constrain),
                reinterpret_cast<std::uintptr_t>(c.font_state));
            return 1;
        }
        default:return 0;
        }
    };
    return a2fo_rs_c_queue_draw_count(&context,count,infinite,
        &context.rect,step);
}

void __attribute__((fastcall)) grouped_build_queue_icon_render_hook(
    void* icon, void*) noexcept {
    struct Context {
        void* icon;
        void* ship_display{};
        void* producer{};
        std::uint32_t slot{};
        GroupedQueueView view{};
        void** target_slot{};
        void* saved_target{};
        std::exception_ptr error{};
    } context{icon};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return g_build_queue_icon_render_original!=nullptr;
        case 1:return g_grouped_queue_ui_ready;
        case 2:a2fo_call_thiscall_0(g_build_queue_icon_render_original,c.icon);
            return 1;
        case 3:return grouped_queue_icon_context(c.icon,&c.ship_display,
            &c.producer,&c.slot);
        case 4:return writable_range(
            bytes(c.icon)+kBuildQueueIconTargetClassOffset,sizeof(void*));
        case 5:c.view=collect_grouped_queue(c.producer);return 1;
        case 6:c.target_slot=reinterpret_cast<void**>(
            bytes(c.icon)+kBuildQueueIconTargetClassOffset);
            c.saved_target=*c.target_slot;
            *c.target_slot=c.slot<c.view.count
                ?c.view.groups[c.slot].target_class:nullptr;return 1;
        case 7:if(writable_range(
            bytes(c.icon)+kBuildQueueIconOwnerOffset,sizeof(void*))){
                *reinterpret_cast<void**>(
                    bytes(c.icon)+kBuildQueueIconOwnerOffset)=c.producer;
            }return 1;
        case 8:return c.slot<c.view.count;
        case 9:return c.view.groups[c.slot].count;
        case 10:return c.view.groups[c.slot].infinite;
        case 11:draw_group_count(c.icon,c.ship_display,
            c.view.groups[c.slot].count,c.view.groups[c.slot].infinite);
            return 1;
        case 12:*c.target_slot=c.saved_target;return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    a2fo_rs_c_queue_icon_render(&context,step);
    if(context.error)std::terminate();
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
    struct Context {
        void* producer;
        const GroupedQueueView& view;
        void** update_slot{};
        std::uint32_t* queue_count{};
        std::uint32_t saved_count{};
        std::exception_ptr error{};
    } context{producer,view};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return !g_extended_queue_button_refresh;
        case 1:return native_queue_count(c.producer);
        case 2:return logical_queue_units(c.view);
        case 3:return c.view.count;
        case 4:return writable_range(bytes(c.producer)+kQueueCountOffset,
            sizeof(std::uint32_t));
        case 5:return readable_range(c.producer,sizeof(void*));
        case 6: {
            void** vtable=*reinterpret_cast<void***>(c.producer);
            c.update_slot=vtable?reinterpret_cast<void**>(
                bytes(vtable)+kUpdateBuildButtonsVtableOffset):nullptr;
            return 1;
        }
        case 7:return readable_range(c.update_slot,sizeof(void*))&&
            *c.update_slot;
        case 8:c.queue_count=reinterpret_cast<std::uint32_t*>(
            bytes(c.producer)+kQueueCountOffset);
            c.saved_count=*c.queue_count;return c.saved_count;
        case 9:g_extended_queue_button_refresh=true;
            *c.queue_count=kQueueCapacity-1;return 1;
        case 10:a2fo_call_thiscall_0(*c.update_slot,c.producer);return 1;
        case 11:*c.queue_count=c.saved_count;return 1;
        case 12:g_extended_queue_button_refresh=false;return 1;
        case 13:if(!g_logged_extended_queue_buttons){
            g_logged_extended_queue_buttons=true;
            log_message("Extended queue kept Producer build buttons enabled past 10 "
                "native jobs");
            }return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    a2fo_rs_c_queue_refresh_buttons(&context,
        reinterpret_cast<std::uintptr_t>(producer),step);
    if(context.error)std::terminate();
}

void __attribute__((fastcall)) grouped_build_queue_icon_simulate_hook(
    void* icon, void*) noexcept {
    struct Context {
        void* icon;
        void* ship_display{};
        void* producer{};
        std::uint32_t slot{};
        GroupedQueueView view{};
        void** target_slot{};
        void* saved_target{};
        GroupedQueueUiContext saved_context{};
        std::exception_ptr error{};
    } context{icon};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return g_build_queue_icon_simulate_original!=nullptr;
        case 1:return g_grouped_queue_ui_ready;
        case 2:a2fo_call_thiscall_0(g_build_queue_icon_simulate_original,c.icon);
            return 1;
        case 3:return grouped_queue_icon_context(c.icon,&c.ship_display,
            &c.producer,&c.slot);
        case 4:return writable_range(
            bytes(c.icon)+kBuildQueueIconTargetClassOffset,sizeof(void*));
        case 5:c.view=collect_grouped_queue(c.producer);return 1;
        case 6:return c.slot;
        case 7:refresh_build_buttons_for_extended_queue(c.producer,c.view);
            return 1;
        case 8:c.target_slot=reinterpret_cast<void**>(
            bytes(c.icon)+kBuildQueueIconTargetClassOffset);
            c.saved_target=*c.target_slot;
            *c.target_slot=c.slot<c.view.count
                ?c.view.groups[c.slot].target_class:nullptr;return 1;
        case 9:c.saved_context=g_grouped_queue_ui_context;
            g_grouped_queue_ui_context.active=c.slot<c.view.count;
            g_grouped_queue_ui_context.producer=c.producer;
            g_grouped_queue_ui_context.visual_slot=c.slot;return 1;
        case 10:g_grouped_queue_ui_context=c.saved_context;return 1;
        case 11:*c.target_slot=c.saved_target;return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    a2fo_rs_c_queue_icon_simulate(&context,step);
    if(context.error)std::terminate();
}

bool install_grouped_queue_ui() noexcept {
    struct Context {
        void** vtable{};
        void** simulate_slot{};
        void** render_slot{};
        void* simulate{};
        void* render{};
        DWORD old_protection{};
        std::exception_ptr error{};
    } context;
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return g_armada&&g_repeat_ready&&signature_matches(
            g_armada,kDisplayInterfaceDrawTextInRectangleRva,
            kExpectedDisplayInterfaceDrawTextInRectangle);
        case 1:c.vtable=at<void*>(g_armada,kBuildQueueIconVtableRva);
            return c.vtable&&readable_range(c.vtable,
                (kBuildQueueIconRenderVtableIndex+1)*sizeof(void*));
        case 2:c.simulate_slot=c.vtable+kBuildQueueIconSimulateVtableIndex;
            c.render_slot=c.vtable+kBuildQueueIconRenderVtableIndex;
            c.simulate=*c.simulate_slot;c.render=*c.render_slot;
            return c.simulate&&c.render&&readable_range(c.simulate,1)&&
                readable_range(c.render,1);
        case 3:return VirtualProtect(c.simulate_slot,2*sizeof(void*),
            PAGE_EXECUTE_READWRITE,&c.old_protection)!=0;
        case 4:g_build_queue_icon_simulate_original=c.simulate;
            g_build_queue_icon_render_original=c.render;
            *c.simulate_slot=reinterpret_cast<void*>(
                &grouped_build_queue_icon_simulate_hook);
            *c.render_slot=reinterpret_cast<void*>(
                &grouped_build_queue_icon_render_hook);return 1;
        case 5: {
            DWORD ignored=0;
            VirtualProtect(c.simulate_slot,2*sizeof(void*),
                c.old_protection,&ignored);return 1;
        }
        case 6:FlushInstructionCache(GetCurrentProcess(),c.simulate_slot,
            2*sizeof(void*));return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const bool result=a2fo_rs_c_queue_install_ui(&context,step);
    if(context.error)std::terminate();
    return result;
}

std::uint32_t push_refit_checked(
    void* producer, void* target_class) noexcept {
    struct Context {
        void* producer;
        void* target_class;
        void* item{};
        std::uint8_t failed{};
    } context{producer,target_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return *reinterpret_cast<const std::uint32_t*>(
            bytes(c.producer)+kQueueCountOffset);
        case 1:return hybrid_production_has_evolution_barrier(c.producer);
        case 2:return dispatch_producer_event(A2FO_PRODUCER_EVENT_ADMIT,
            c.producer,c.target_class);
        case 3:c.item=*reinterpret_cast<void**>(
            bytes(c.producer)+kQueueHeadOffset);
            return reinterpret_cast<std::uintptr_t>(c.item);
        case 4:return *reinterpret_cast<const std::uint32_t*>(
            bytes(c.item)+kQueueItemIdOffset);
        case 5:c.item=*reinterpret_cast<void**>(
            bytes(c.item)+kQueueItemNextOffset);
            return reinterpret_cast<std::uintptr_t>(c.item);
        case 6:a2fo_call_thiscall_1(at(g_fleet_ops,kFoProducerPushCheckedRva),
            c.producer,reinterpret_cast<std::uintptr_t>(c.target_class));
            return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    const auto result=a2fo_rs_c_queue_push_refit(&context,
        reinterpret_cast<std::uintptr_t>(producer),
        reinterpret_cast<std::uintptr_t>(target_class),g_repeat_ready,
        step,&context.failed);
    if(context.failed)std::terminate();
    return result;
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
    void* producer, void* target_class, bool dispatch_admit,
    bool target_resolved = false) noexcept {
    struct Context {void* producer;void* target_class;}
        context{producer,target_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:c.target_class=resolve_build_queue_class(
            c.producer,c.target_class);
            return reinterpret_cast<std::uintptr_t>(c.target_class);
        case 1:return c.target_class!=nullptr;
        case 2:return native_queue_count(c.producer);
        case 3:return hybrid_production_has_evolution_barrier(c.producer);
        case 4:return dispatch_producer_event(
            A2FO_PRODUCER_EVENT_ADMIT,c.producer,c.target_class);
        case 5:return native_queue_count(c.producer);
        case 6:a2fo_call_thiscall_1(
            at(g_fleet_ops,kFoProducerPushCheckedRva),c.producer,
            reinterpret_cast<std::uintptr_t>(c.target_class));return 1;
        case 7:return native_queue_count(c.producer);
        default:return 0;
        }
    };
    return a2fo_rs_c_queue_push_once(&context,
        reinterpret_cast<std::uintptr_t>(producer),
        reinterpret_cast<std::uintptr_t>(target_class),dispatch_admit,
        target_resolved,step);
}

bool queue_logical_one_checked(void* producer, void* target_class,
                               bool target_resolved = false) noexcept {
    struct Context {
        void* producer;void* target_class;
        GroupedQueueView view{};
        PendingQueueSnapshot pending{};
    } context{producer,target_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:c.target_class=resolve_build_queue_class(
            c.producer,c.target_class);
            return reinterpret_cast<std::uintptr_t>(c.target_class);
        case 1:return c.target_class!=nullptr;
        case 2:return hybrid_production_has_evolution_barrier(c.producer);
        case 3:return hybrid_production_has_queued_research_conflict(
            c.producer,c.target_class);
        case 4:c.view=collect_grouped_queue(c.producer);return 1;
        case 5:return logical_queue_has_room_for(c.view,c.target_class);
        case 6:c.pending=pending_queue_snapshot(c.producer);
            return c.pending.count;
        case 7:return native_queue_count(c.producer);
        case 8:return push_native_checked_once(c.producer,c.target_class,
            true,true);
        case 9:return dispatch_producer_event(A2FO_PRODUCER_EVENT_ADMIT,
            c.producer,c.target_class);
        case 10:return append_pending_queue(c.producer,c.target_class);
        default:return 0;
        }
    };
    return a2fo_rs_c_queue_logical_one(&context,
        reinterpret_cast<std::uintptr_t>(producer),
        reinterpret_cast<std::uintptr_t>(target_class),target_resolved,step);
}

void materialize_pending_queue(void* producer) noexcept {
    struct Context {
        void* producer;
        PendingQueueSnapshot pending{};
        void* target_class{};
    } context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:return refit_has_waiting_job(c.producer);
        case 1:return hybrid_production_has_evolution_barrier(c.producer);
        case 2:return native_queue_count(c.producer);
        case 3:c.pending=pending_queue_snapshot(c.producer);
            return c.pending.count;
        case 4:c.target_class=c.pending.entries[0];
            return reinterpret_cast<std::uintptr_t>(c.target_class);
        case 5:return hybrid_production_has_queued_research_conflict(
            c.producer,c.target_class);
        case 6:return push_native_checked_once(c.producer,c.target_class,
            false);
        case 7:return erase_pending_queue_index(c.producer,0);
        default:return 0;
        }
    };
    a2fo_rs_c_queue_materialize(&context,
        reinterpret_cast<std::uintptr_t>(producer),step);
}

std::uint32_t fill_queue_checked(void* producer, void* target_class) {
    struct Context {
        void* producer;
        void* target_class;
        GroupedQueueView view{};
    } context{producer,target_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:c.target_class=resolve_build_queue_class(
            c.producer,c.target_class);
            return reinterpret_cast<std::uintptr_t>(c.target_class);
        case 1:return hybrid_production_has_evolution_barrier(c.producer);
        case 2:return hybrid_production_has_queued_research_conflict(
            c.producer,c.target_class);
        case 3:c.view=collect_grouped_queue(c.producer);return 1;
        case 4:return c.view.count;
        case 5:return c.view.groups[c.view.count-1].infinite;
        case 6:return reinterpret_cast<std::uintptr_t>(
            c.view.groups[c.view.count-1].target_class);
        case 7:return c.view.groups[c.view.count-1].count;
        case 8:return hybrid_production_is_evolve_target(
            c.producer,c.target_class);
        case 9:return queue_logical_one_checked(
            c.producer,c.target_class,true);
        default:return 0;
        }
    };
    return a2fo_rs_c_queue_fill(&context,
        reinterpret_cast<std::uintptr_t>(producer),
        reinterpret_cast<std::uintptr_t>(target_class),step);
}

void __attribute__((fastcall)) game_object_dequeue_class_command_hook(
    void* game_object, void*, std::uint32_t command,
    void* target_class) {
    struct Context {
        void* object;
        std::uint32_t command;
        void* target;
        void* repeat_target{};
        void* effective{};
        GroupedQueueView view{};
        std::uint8_t failed{};
        std::exception_ptr error{};
    } context{game_object,command,target_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t value) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return consume_refit_synchronized_command(
            c.object,c.target);
        case 1:log_message("Synchronized refit request rejected");return 1;
        case 2:return cancel_refit_synchronized_command(c.object);
        case 3:log_message("Synchronized refit cancellation rejected");
            return 1;
        case 4:return a2fo_call_thiscall_2(
            g_game_object_dequeue_class_command_hook.gateway,c.object,
            c.command,reinterpret_cast<std::uintptr_t>(c.target));
        case 5:log_message("Queue-enhancement marker rejected");return 1;
        case 6:return continuous_state(c.object,&c.repeat_target);
        case 7:return reinterpret_cast<std::uintptr_t>(c.repeat_target);
        case 8:stop_continuous(c.object);return 1;
        case 9:log_message(
            "Continuous production disabled by repeated build order");
            return 1;
        case 10:return queue_logical_one_checked(c.object,c.target);
        case 11:log_message("Extended synchronized build order accepted");
            return 1;
        case 12:log_message("Extended synchronized build order rejected");
            return 1;
        case 13:return hybrid_production_is_evolve_target(c.object,c.target);
        case 14:return fill_queue_checked(c.object,c.target);
        case 15:{char message[96];
            std::snprintf(message,sizeof(message),
                "Ctrl synchronized logical slot fill received: %lu added",
                static_cast<unsigned long>(value));
            log_message(message);return 1;}
        case 16:c.effective=resolve_build_queue_class(c.object,c.target);
            return reinterpret_cast<std::uintptr_t>(c.effective);
        case 17:log_message(
            "Continuous production marker rejected: class resolution");
            return 1;
        case 18:c.view=collect_grouped_queue(c.object);return c.view.count;
        case 19:return reinterpret_cast<std::uintptr_t>(
            c.view.groups[c.view.count-1].target_class);
        case 20:return queue_logical_one_checked(c.object,c.effective,true);
        case 21:log_message(
            "Continuous production marker rejected: no logical slot");
            return 1;
        case 22:try {
            const auto handle=object_handle(c.object);
            EnterCriticalSection(&g_queue_lock);
            auto& state=g_continuous[handle];
            state.active=true;
            state.target_class=c.target;
            state.target_project_id=project_id(c.target);
            state.paused_retry_ticks=0;
            LeaveCriticalSection(&g_queue_lock);
            log_message("Continuous production enabled as one logical xINF slot");
            return 1;
            }catch(...){
                LeaveCriticalSection(&g_queue_lock);
                log_message("Continuous production marker could not be recorded");
                return 0;
            }
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    a2fo_rs_c_queue_dequeue_command(&context,
        reinterpret_cast<std::uintptr_t>(game_object),command,
        reinterpret_cast<std::uintptr_t>(target_class),g_repeat_ready,
        step,&context.failed);
    if(context.error)std::rethrow_exception(context.error);
}

void try_refill(void* producer) {
    struct Context {
        void* producer;
        std::uint32_t handle{};
        void* target_class{};
        PendingQueueSnapshot pending{};
    } context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:materialize_pending_queue(c.producer);return 1;
        case 1: {
            c.handle=object_handle(c.producer);
            EnterCriticalSection(&g_queue_lock);
            const auto found=g_continuous.find(c.handle);
            if(found!=g_continuous.end()&&found->second.active){
                c.target_class=found->second.target_class;
            }
            LeaveCriticalSection(&g_queue_lock);
            return reinterpret_cast<std::uintptr_t>(c.target_class);
        }
        case 2:return refit_has_waiting_job(c.producer);
        case 3:return hybrid_production_has_evolution_barrier(c.producer);
        case 4:stop_continuous(c.producer);return 1;
        case 5:return hybrid_production_has_queued_research_conflict(
            c.producer,c.target_class);
        case 6:c.pending=pending_queue_snapshot(c.producer);
            return c.pending.count;
        case 7:return native_queue_count(c.producer);
        case 8:return push_native_checked_once(c.producer,c.target_class,true);
        case 9: {
            EnterCriticalSection(&g_queue_lock);
            const auto found=g_continuous.find(c.handle);
            if(found!=g_continuous.end()){
                found->second.paused_retry_ticks=0;
            }
            LeaveCriticalSection(&g_queue_lock);
            return 1;
        }
        default:return 0;
        }
    };
    a2fo_rs_c_queue_refill(&context,
        reinterpret_cast<std::uintptr_t>(producer),g_repeat_ready,step);
}

void __attribute__((fastcall)) game_object_queue_class_command_hook(
    void* game_object, void*, std::uint32_t command,
    void* target_class) {
    struct Context {
        void* object;
        std::uint32_t command;
        void* target;
        PendingQueueSnapshot pending{};
        GroupedQueueView view{};
        void* repeat_target{};
        std::uint8_t failed{};
        std::exception_ptr error{};
    } context{game_object,command,target_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t value) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return hybrid_production_should_defer_construct_order(
            c.object,c.target);
        case 1:retain_hybrid_research_menu_after_order(c.object,c.target);
            return 1;
        case 2:return a2fo_call_thiscall_2(
            g_game_object_queue_class_command_hook.gateway,c.object,
            value,reinterpret_cast<std::uintptr_t>(c.target));
        case 3:if(!g_logged_build_order_path){
            g_logged_build_order_path=true;
            log_message("First Producer build order reached");}
            return 1;
        case 4:return modifier_key_down(kCommandControlPointerRva,VK_CONTROL);
        case 5:return modifier_key_down(kCommandAltPointerRva,VK_MENU);
        case 6:return a2fo_call_thiscall_2(
            g_game_object_queue_class_command_hook.gateway,c.object,
            value,reinterpret_cast<std::uintptr_t>(c.target));
        case 7:log_message(value
            ?"Ctrl+Alt synchronized continuous marker queued"
            :"Ctrl synchronized logical-slot fill marker queued");
            return 1;
        case 8:c.pending=pending_queue_snapshot(c.object);
            return c.pending.count;
        case 9:return native_queue_count(c.object);
        case 10:c.view=collect_grouped_queue(c.object);return 1;
        case 11:return continuous_state(c.object,&c.repeat_target);
        case 12:return reinterpret_cast<std::uintptr_t>(c.repeat_target);
        case 13:return logical_queue_has_room_for(c.view,c.target);
        case 14:return collective_queue_may_change_class(c.object);
        case 15:log_message("Logical build queue full (10 slots x 10 units)");
            return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    a2fo_rs_c_queue_queue_command(&context,command,
        reinterpret_cast<std::uintptr_t>(target_class),g_repeat_ready,
        step,&context.failed);
    if(context.error)std::rethrow_exception(context.error);
}

void __attribute__((fastcall)) producer_command_push_hook(
    void* producer, void*, void* target_class) {
    struct Context {
        void* producer;
        void* base;
        void* target;
        std::uint32_t handle{};
        decltype(g_continuous)::iterator found{};
        GroupedQueueView view{};
        PendingQueueSnapshot pending{};
        std::uint32_t before{};
        std::uint32_t after{};
        std::uint8_t failed{};
        std::exception_ptr error{};
    } context{producer,target_class,target_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t value) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        if(op>=11&&op<=14){
            try {switch(op){
            case 11:
                EnterCriticalSection(&g_queue_lock);
                c.found=g_continuous.find(c.handle);
                if(!g_logged_synchronized_build_path){
                    g_logged_synchronized_build_path=true;
                    log_message("First synchronized Producer build command reached");
                }
                return 1;
            case 12:return c.found!=g_continuous.end()&&
                c.found->second.active;
            case 13:return reinterpret_cast<std::uintptr_t>(
                c.found->second.target_class);
            case 14:if(value!=0)g_continuous.erase(c.found);
                LeaveCriticalSection(&g_queue_lock);return 1;
            default:break;
            }}catch(...){LeaveCriticalSection(&g_queue_lock);
                return UINTPTR_MAX;}
        }
        try {switch(op){
        case 0:return hybrid_production_should_defer_construct_order(
            c.producer,c.target);
        case 1:c.target=resolve_build_queue_class(c.producer,c.base);
            return reinterpret_cast<std::uintptr_t>(c.target);
        case 2:return dispatch_producer_event(A2FO_PRODUCER_EVENT_ADMIT,
            c.producer,c.target);
        case 3:return native_queue_count(c.producer);
        case 4:a2fo_call_thiscall_1(g_fo_command_push_hook.gateway,c.producer,
            reinterpret_cast<std::uintptr_t>(c.target));return 1;
        case 5:finalize_hybrid_construct_order(c.producer,c.target,
            value!=0);return 1;
        case 6:retain_hybrid_research_menu_after_order(c.producer,c.target);
            return 1;
        case 7:return hybrid_production_has_queued_research_conflict(
            c.producer,c.target);
        case 8:return hybrid_production_has_evolution_barrier(c.producer);
        case 9:return hybrid_production_is_evolve_target(c.producer,c.target);
        case 10:c.handle=object_handle(c.producer);return c.handle;
        case 15:c.before=native_queue_count(c.producer);return c.before;
        case 16:c.view=collect_grouped_queue(c.producer);return 1;
        case 17:return logical_queue_has_room_for(c.view,c.target);
        case 18:return dispatch_producer_event(A2FO_PRODUCER_EVENT_ADMIT,
            c.producer,c.target);
        case 19:c.pending=pending_queue_snapshot(c.producer);
            return c.pending.count;
        case 20:a2fo_call_thiscall_1(g_fo_command_push_hook.gateway,
            c.producer,reinterpret_cast<std::uintptr_t>(c.target));return 1;
        case 21:return native_queue_count(c.producer);
        case 22:return append_pending_queue(c.producer,c.target);
        case 23:c.after=native_queue_count(c.producer);return c.after;
        case 24:finalize_hybrid_construct_order(c.producer,c.target,
            value!=0);return 1;
        case 25:if(g_synchronized_push_log_count<kSynchronizedPushLogLimit){
            ++g_synchronized_push_log_count;return 1;}return 0;
        case 26:{
            static const char* const outcome[]={
                "research conflict rejected",
                "evolution barrier rejected",
                "continuous mode toggled off",
                "logical 10x10 queue full",
                "accepted into extended queue",
                "forwarded to native queue",
                "rejected","no native change"};
            char message[192];
            std::snprintf(message,sizeof(message),
                "Synchronized Producer queue result: target %lu, "
                "native %lu -> %lu (%s)",
                static_cast<unsigned long>(project_id(c.target)),
                static_cast<unsigned long>(c.before),
                static_cast<unsigned long>(c.after),outcome[value]);
            log_message(message);return 1;}
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    a2fo_rs_c_queue_producer_push(&context,
        reinterpret_cast<std::uintptr_t>(producer),
        reinterpret_cast<std::uintptr_t>(target_class),g_repeat_ready,
        step,&context.failed);
    if(context.error)std::rethrow_exception(context.error);
}

std::uintptr_t __attribute__((fastcall)) producer_finish_hook(
    void* producer, void*) {
    struct Context {
        void* producer;
        void* target_class{};
        std::uint32_t queue_id{};
        std::uint8_t failed{};
        std::exception_ptr error{};
    } context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t value) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:c.target_class=current_build_target_class(c.producer);
            return reinterpret_cast<std::uintptr_t>(c.target_class);
        case 1:c.target_class=queue_head_target_class(c.producer);
            return reinterpret_cast<std::uintptr_t>(c.target_class);
        case 2:c.queue_id=current_queue_id(c.producer);return c.queue_id;
        case 3:return dispatch_producer_event(A2FO_PRODUCER_EVENT_FINISHING,
            c.producer,c.target_class);
        case 4:return a2fo_call_thiscall_0(g_fo_finish_hook.gateway,
            c.producer);
        case 5:dispatch_producer_event(A2FO_PRODUCER_EVENT_FINISHED,
            c.producer,c.target_class);return 1;
        case 6:notify_refit_job_finished(c.producer,c.queue_id,
            c.target_class,reinterpret_cast<void*>(value));return 1;
        case 7:try_refill(c.producer);return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    const auto result=a2fo_rs_c_queue_finish(&context,
        reinterpret_cast<std::uintptr_t>(producer),step,&context.failed);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

std::uintptr_t __attribute__((fastcall)) producer_cancel_hook(
    void* producer, void*) {
    struct Context {
        void* producer;
        void* target_class{};
        std::uint32_t queue_id{};
        std::uint8_t failed{};
        std::exception_ptr error{};
    } context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:stop_continuous(c.producer);return 1;
        case 1:c.queue_id=current_queue_id(c.producer);return c.queue_id;
        case 2:c.target_class=current_build_target_class(c.producer);
            return reinterpret_cast<std::uintptr_t>(c.target_class);
        case 3:dispatch_producer_event(A2FO_PRODUCER_EVENT_CANCELLED,
            c.producer,c.target_class);return 1;
        case 4:notify_refit_job_removed(c.producer,c.queue_id,c.target_class,
            A2FO_REFIT_QUEUE_CANCELLED);return 1;
        case 5:return a2fo_call_thiscall_0(g_fo_cancel_hook.gateway,
            c.producer);
        case 6:materialize_pending_queue(c.producer);return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    const auto result=a2fo_rs_c_queue_cancel(&context,
        reinterpret_cast<std::uintptr_t>(producer),step,&context.failed);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

void delete_native_queue_item(
    void* producer, std::uint32_t queue_id) noexcept {
    struct Context {
        void* producer;
        std::uint32_t queue_id;
        void* removed_class{};
        std::uint8_t failed{};
    } context{producer,queue_id};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return *reinterpret_cast<std::uint32_t*>(
            bytes(c.producer)+kCurrentQueueIdOffset);
        case 1:c.removed_class=queued_target_class(c.producer,c.queue_id);
            return reinterpret_cast<std::uintptr_t>(c.removed_class);
        case 2:c.removed_class=current_build_target_class(c.producer);
            return reinterpret_cast<std::uintptr_t>(c.removed_class);
        case 3:return charges_resources_when_queued(c.producer);
        case 4:dispatch_producer_event(A2FO_PRODUCER_EVENT_DELETED,
            c.producer,c.removed_class);return 1;
        case 5:notify_refit_job_removed(c.producer,c.queue_id,
            c.removed_class,A2FO_REFIT_QUEUE_DELETED);return 1;
        case 6:a2fo_call_thiscall_1(g_fo_act_delete_hook.gateway,
            c.producer,c.queue_id);return 1;
        case 7:discard_hybrid_construct_placement(c.producer,c.queue_id);
            return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    a2fo_rs_c_queue_delete_native(&context,
        reinterpret_cast<std::uintptr_t>(producer),queue_id,step,
        &context.failed);
    if(context.failed)std::terminate();
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

    struct Context {void* producer;void* item{};} context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:return reinterpret_cast<std::uintptr_t>(
            current_build_target_class(c.producer));
        case 1:return current_queue_id(c.producer);
        case 2:c.item=*reinterpret_cast<void**>(
            bytes(c.producer)+kQueueHeadOffset);
            return reinterpret_cast<std::uintptr_t>(c.item);
        case 3:return readable_range(c.item,
            kQueueItemIdOffset+sizeof(std::uint32_t));
        case 4:return reinterpret_cast<std::uintptr_t>(
            *reinterpret_cast<void**>(c.item));
        case 5:return *reinterpret_cast<const std::uint32_t*>(
            bytes(c.item)+kQueueItemIdOffset);
        case 6:c.item=*reinterpret_cast<void**>(
            bytes(c.item)+kQueueItemNextOffset);
            return reinterpret_cast<std::uintptr_t>(c.item);
        default:return 0;
        }
    };
    return a2fo_rs_c_queue_visual_slot(&context,step,queue_id,
        kQueueCapacity,visual_slot);
}

void trim_group_to_single(
    void* producer, std::uint32_t visual_slot) noexcept {
    struct Context {
        void* producer;
        GroupedQueueView view{};
        GroupedQueueEntry group{};
    } context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t arg) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:c.view=collect_grouped_queue(c.producer);return c.view.count;
        case 1:c.group=c.view.groups[arg];return c.group.count;
        case 2:return c.group.pending_count;
        case 3:return c.group.last_pending_index;
        case 4:return erase_pending_queue_index(c.producer,
            static_cast<std::uint32_t>(arg));
        case 5:return current_queue_id(c.producer);
        case 6:return c.group.last_queue_id;
        case 7:delete_native_queue_item(c.producer,
            static_cast<std::uint32_t>(arg));return 1;
        default:return 0;
        }
    };
    a2fo_rs_c_queue_trim_group(&context,
        reinterpret_cast<std::uintptr_t>(producer),visual_slot,step);
}

void __attribute__((fastcall)) producer_act_delete_hook(
    void* producer, void*, std::uint32_t queue_id) {
    struct Context {
        void* producer;
        std::uint32_t queue_id;
        std::uint32_t requested_slot=0xffffffffu;
        GroupedQueueView view{};
        GroupedQueueEntry group{};
    } context{producer,queue_id};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t arg) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:return g_grouped_queue_ui_ready;
        case 1:return g_grouped_queue_ui_context.active&&
            g_grouped_queue_ui_context.producer==c.producer;
        case 2:c.requested_slot=g_grouped_queue_ui_context.visual_slot;
            return 1;
        case 3:return visual_slot_from_native_queue_id(
            c.producer,c.queue_id,&c.requested_slot);
        case 4:if(!g_logged_grouped_delete_slot_fallback){
            g_logged_grouped_delete_slot_fallback=true;
            log_message("Grouped queue cancellation recovered the clicked "
                "visual slot from the native icon index");
            }return 1;
        case 5:c.view=collect_grouped_queue(c.producer);return c.view.count;
        case 6:return c.requested_slot;
        case 7:c.group=c.view.groups[c.requested_slot];
            return c.group.infinite;
        case 8:trim_group_to_single(c.producer,c.requested_slot);
            stop_continuous(c.producer);
            log_message("Continuous queue slot removed: xINF reverted "
                        "to one normal build");return 1;
        case 9:return c.group.pending_count;
        case 10:return c.group.last_pending_index;
        case 11:erase_pending_queue_index(c.producer,
            c.group.last_pending_index);
            if(!g_logged_grouped_delete_remap){
                g_logged_grouped_delete_remap=true;
                log_message("Grouped queue cancellation removed the last "
                    "extended item in the visible run");
            }return 1;
        case 12:return c.group.last_queue_id;
        case 13:return c.queue_id;
        case 14:c.queue_id=static_cast<std::uint32_t>(arg);
            if(!g_logged_grouped_delete_remap){
                g_logged_grouped_delete_remap=true;
                log_message("Grouped queue cancellation remapped to the last "
                    "native item in the visible run");
            }return 1;
        case 15:delete_native_queue_item(c.producer,c.queue_id);return 1;
        case 16:materialize_pending_queue(c.producer);return 1;
        default:return 0;
        }
    };
    a2fo_rs_c_queue_act_delete(&context,step);
}

void __attribute__((fastcall)) producer_clear_hook(void* producer, void*) {
    struct Context {
        void* producer;
        bool charge_at_queue{};
        std::uint32_t active_id{};
        void* item{};
        void* target_class{};
        std::uint32_t queue_id{};
        std::exception_ptr error{};
    } context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:stop_continuous(c.producer);return 1;
        case 1:clear_pending_queue(c.producer);return 1;
        case 2:c.charge_at_queue=charges_resources_when_queued(c.producer);
            return c.charge_at_queue;
        case 3:c.active_id=current_queue_id(c.producer);return c.active_id;
        case 4:c.item=c.producer?*reinterpret_cast<void**>(
            bytes(c.producer)+kQueueHeadOffset):nullptr;
            return reinterpret_cast<std::uintptr_t>(c.item);
        case 5:c.target_class=*reinterpret_cast<void**>(c.item);
            return reinterpret_cast<std::uintptr_t>(c.target_class);
        case 6:c.queue_id=*reinterpret_cast<const std::uint32_t*>(
            bytes(c.item)+kQueueItemIdOffset);return c.queue_id;
        case 7:dispatch_producer_event(A2FO_PRODUCER_EVENT_CLEARED,
            c.producer,c.target_class);return 1;
        case 8:notify_refit_job_removed(c.producer,c.queue_id,
            c.target_class,A2FO_REFIT_QUEUE_CLEARED);return 1;
        case 9:c.item=*reinterpret_cast<void**>(
            bytes(c.item)+kQueueItemNextOffset);
            return reinterpret_cast<std::uintptr_t>(c.item);
        case 10:c.target_class=current_build_target_class(c.producer);
            return reinterpret_cast<std::uintptr_t>(c.target_class);
        case 11:dispatch_producer_event(A2FO_PRODUCER_EVENT_CLEARED,
            c.producer,c.target_class);return 1;
        case 12:notify_refit_job_removed(c.producer,c.active_id,
            c.target_class,A2FO_REFIT_QUEUE_CLEARED);return 1;
        case 13:notify_refit_job_removed(c.producer,c.active_id,
            current_build_target_class(c.producer),
            A2FO_REFIT_QUEUE_CLEARED);return 1;
        case 14:a2fo_call_thiscall_0(g_fo_clear_hook.gateway,c.producer);
            return 1;
        case 15:clear_hybrid_construct_placements(c.producer);return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    a2fo_rs_c_queue_clear(&context,
        reinterpret_cast<std::uintptr_t>(producer),step);
    if(context.error)std::rethrow_exception(context.error);
}

void __attribute__((fastcall)) producer_simulate_hook(
    void* producer, void*, std::uint32_t delta_bits) {
    struct Context {
        void* producer;
        std::uint32_t delta_bits;
        std::uint32_t handle{};
        ContinuousState* state{};
        std::uint8_t failed{};
        std::exception_ptr error{};
    } context{producer,delta_bits};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:a2fo_call_thiscall_1(g_producer_simulate_hook.gateway,
            c.producer,c.delta_bits);return 1;
        case 1:return native_queue_count(c.producer);
        case 2:return pending_queue_snapshot(c.producer).count;
        case 3:materialize_pending_queue(c.producer);return 1;
        case 4:c.handle=object_handle(c.producer);
            EnterCriticalSection(&g_queue_lock);
            {const auto found=g_continuous.find(c.handle);
            c.state=found==g_continuous.end()?nullptr:&found->second;}
            return c.state&&c.state->active;
        case 5:return *reinterpret_cast<std::uint32_t*>(
            bytes(c.producer)+kQueueCountOffset);
        case 6:return ++c.state->paused_retry_ticks;
        case 7:c.state->paused_retry_ticks=0;return 1;
        case 8:LeaveCriticalSection(&g_queue_lock);return 1;
        case 9:try_refill(c.producer);return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    a2fo_rs_c_queue_simulate(&context,
        reinterpret_cast<std::uintptr_t>(producer),g_repeat_ready,step,
        &context.failed);
    if(context.error)std::rethrow_exception(context.error);
}

std::uintptr_t __attribute__((fastcall)) producer_dtor_hook(
    void* producer, void*) {
    struct Context {
        void* producer;
        std::uint8_t failed{};
        std::exception_ptr error{};
    } context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:dispatch_producer_event(A2FO_PRODUCER_EVENT_DESTROYING,
            c.producer,nullptr);return 1;
        case 1:notify_refit_job_removed(c.producer,0,nullptr,
            A2FO_REFIT_QUEUE_PRODUCER_DESTROYED);return 1;
        case 2:stop_continuous(c.producer);return 1;
        case 3:clear_pending_queue(c.producer);return 1;
        case 4:return a2fo_call_thiscall_0(g_producer_dtor_hook.gateway,
            c.producer);
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    const auto result=a2fo_rs_c_queue_dtor(&context,
        reinterpret_cast<std::uintptr_t>(producer),step,&context.failed);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

void reconstruct_queue_ids(void* producer) {
    struct Context {void* producer;void* item{};} context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t value) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:c.item=*reinterpret_cast<void**>(
            bytes(c.producer)+kQueueHeadOffset);
            return reinterpret_cast<std::uintptr_t>(c.item);
        case 1:return *reinterpret_cast<std::uint32_t*>(
            bytes(c.item)+kQueueItemIdOffset);
        case 2:return *reinterpret_cast<std::uint32_t*>(
            bytes(c.item)+kQueueItemIdOffset);
        case 3:c.item=*reinterpret_cast<void**>(
            bytes(c.item)+kQueueItemNextOffset);
            return reinterpret_cast<std::uintptr_t>(c.item);
        case 4:*reinterpret_cast<std::uint32_t*>(
            bytes(c.producer)+kCurrentQueueIdOffset)=
            static_cast<std::uint32_t>(value);return 1;
        case 5:*reinterpret_cast<std::uint32_t*>(
            bytes(c.producer)+kNextQueueIdOffset)=
            static_cast<std::uint32_t>(value);return 1;
        default:return 0;
        }
    };
    a2fo_rs_c_queue_reconstruct_ids(&context,step);
}

std::uintptr_t __attribute__((fastcall)) producer_save_hook(
    void* producer, void*, void* writer) {
    struct Context {
        void* producer;
        void* writer;
        bool active{};
        std::uint32_t target_project_id{};
        std::uint32_t* current_id{};
        std::uint32_t* next_id{};
        std::uint32_t saved_current_id{};
        std::uint32_t saved_next_id{};
        std::uint8_t failed{};
        std::exception_ptr error{};
    } context{producer,writer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return a2fo_call_thiscall_1(
            g_producer_save_hook.gateway,c.producer,
            reinterpret_cast<std::uintptr_t>(c.writer));
        case 1: {
            const std::uint32_t handle=object_handle(c.producer);
            EnterCriticalSection(&g_queue_lock);
            const auto found=g_continuous.find(handle);
            if(found!=g_continuous.end()&&found->second.active){
                c.active=true;
                c.target_project_id=found->second.target_project_id;
            }
            LeaveCriticalSection(&g_queue_lock);
            return c.active;
        }
        case 2:c.current_id=reinterpret_cast<std::uint32_t*>(
            bytes(c.producer)+kCurrentQueueIdOffset);
            c.saved_current_id=*c.current_id;return c.saved_current_id;
        case 3:c.next_id=reinterpret_cast<std::uint32_t*>(
            bytes(c.producer)+kNextQueueIdOffset);
            c.saved_next_id=*c.next_id;return c.saved_next_id;
        case 4:*c.current_id=kRepeatSaveMarker;
            *c.next_id=c.target_project_id;return 1;
        case 5:return a2fo_call_thiscall_1(
            g_producer_save_hook.gateway,c.producer,
            reinterpret_cast<std::uintptr_t>(c.writer));
        case 6:*c.current_id=c.saved_current_id;return 1;
        case 7:*c.next_id=c.saved_next_id;return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;
            return 0;}
    };
    const auto result=a2fo_rs_c_queue_save(&context,
        reinterpret_cast<std::uintptr_t>(producer),g_repeat_ready,step,
        &context.failed);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

std::uintptr_t __attribute__((fastcall)) producer_load_hook(
    void* producer, void*, void* reader) {
    struct Context {
        void* producer;
        void* reader;
        std::uint32_t target_project_id{};
        void* target_class{};
        std::uint8_t failed{};
        std::exception_ptr error{};
    } context{producer,reader};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return a2fo_call_thiscall_1(
            g_producer_load_hook.gateway,c.producer,
            reinterpret_cast<std::uintptr_t>(c.reader));
        case 1:clear_pending_queue(c.producer);return 1;
        case 2:return *reinterpret_cast<std::uint32_t*>(
            bytes(c.producer)+kCurrentQueueIdOffset);
        case 3:c.target_project_id=*reinterpret_cast<std::uint32_t*>(
            bytes(c.producer)+kNextQueueIdOffset);
            return c.target_project_id;
        case 4:reconstruct_queue_ids(c.producer);return 1;
        case 5: {
            using FindByProjectIdFunction=void* (A2FO_CALL*)(std::uint32_t);
            const auto find_by_project_id=
                reinterpret_cast<FindByProjectIdFunction>(
                    at(g_armada,kGameObjectClassFindByProjectIdRva));
            c.target_class=find_by_project_id(c.target_project_id);
            return reinterpret_cast<std::uintptr_t>(c.target_class);
        }
        case 6:stop_continuous(c.producer);return 1;
        case 7: {
            try {
                const std::uint32_t handle=object_handle(c.producer);
                EnterCriticalSection(&g_queue_lock);
                ContinuousState& state=g_continuous[handle];
                state.active=true;
                state.target_class=c.target_class;
                state.target_project_id=c.target_project_id;
                state.paused_retry_ticks=0;
                LeaveCriticalSection(&g_queue_lock);
            }catch(...){LeaveCriticalSection(&g_queue_lock);}
            return 1;
        }
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;
            return 0;}
    };
    const auto result=a2fo_rs_c_queue_load(&context,
        reinterpret_cast<std::uintptr_t>(producer),g_repeat_ready,step,
        &context.failed);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) {
    return module &&
        std::memcmp(at(module, rva), expected, Size) == 0;
}

template <std::size_t Size>
bool install_hook(HMODULE module, std::uintptr_t rva, void* replacement,
                  const std::uint8_t (&expected)[Size],
                  A2FO_InlineHook& hook) {
    return g_api->install_inline_hook(
        at(module, rva), replacement, Size, expected, &hook);
}

bool install_repeat_hooks() {
    struct SignatureSpec {
        bool fleet;
        std::uintptr_t rva;
        const std::uint8_t* expected;
        std::size_t length;
        const char* name;
    };
    const std::array<SignatureSpec,12> signatures{{
        {false,kGameObjectDequeueClassCommandRva,kExpectedGameObjectDequeueClassCommand,sizeof(kExpectedGameObjectDequeueClassCommand),"Armada synchronized typed-class order receive"},
        {false,kProducerDtorRva,kExpectedProducerDtor,sizeof(kExpectedProducerDtor),"Armada Producer destructor"},
        {false,kProducerSimulateRva,kExpectedProducerSimulate,sizeof(kExpectedProducerSimulate),"Armada Producer simulate"},
        {false,kProducerLoadRva,kExpectedProducerLoad,sizeof(kExpectedProducerLoad),"Armada Producer load"},
        {false,kProducerSaveRva,kExpectedProducerSave,sizeof(kExpectedProducerSave),"Armada Producer save"},
        {false,kGameObjectClassFindByProjectIdRva,kExpectedFindByProjectId,sizeof(kExpectedFindByProjectId),"Armada class lookup by project ID"},
        {true,kFoProducerFinishRva,kExpectedFoFinish,sizeof(kExpectedFoFinish),"Fleet Ops Producer finish callback"},
        {true,kFoProducerCancelRva,kExpectedFoCancel,sizeof(kExpectedFoCancel),"Fleet Ops Producer cancel callback"},
        {true,kFoProducerPushCheckedRva,kExpectedFoPushChecked,sizeof(kExpectedFoPushChecked),"Fleet Ops checked queue push"},
        {true,kFoProducerCommandPushRva,kExpectedFoCommandPush,sizeof(kExpectedFoCommandPush),"Fleet Ops build-command push"},
        {true,kFoProducerActDeleteRva,kExpectedFoActDelete,sizeof(kExpectedFoActDelete),"Fleet Ops queue-item delete callback"},
        {true,kFoProducerClearRva,kExpectedFoClear,sizeof(kExpectedFoClear),"Fleet Ops queue clear callback"},
    }};
    struct HookSpec {
        bool fleet;
        std::uintptr_t rva;
        void* replacement;
        const std::uint8_t* expected;
        std::size_t length;
        A2FO_InlineHook* hook;
    };
    const std::array<HookSpec,10> hooks{{
        {false,kGameObjectDequeueClassCommandRva,reinterpret_cast<void*>(&game_object_dequeue_class_command_hook),kExpectedGameObjectDequeueClassCommand,sizeof(kExpectedGameObjectDequeueClassCommand),&g_game_object_dequeue_class_command_hook},
        {false,kProducerDtorRva,reinterpret_cast<void*>(&producer_dtor_hook),kExpectedProducerDtor,sizeof(kExpectedProducerDtor),&g_producer_dtor_hook},
        {false,kProducerSimulateRva,reinterpret_cast<void*>(&producer_simulate_hook),kExpectedProducerSimulate,sizeof(kExpectedProducerSimulate),&g_producer_simulate_hook},
        {false,kProducerLoadRva,reinterpret_cast<void*>(&producer_load_hook),kExpectedProducerLoad,sizeof(kExpectedProducerLoad),&g_producer_load_hook},
        {false,kProducerSaveRva,reinterpret_cast<void*>(&producer_save_hook),kExpectedProducerSave,sizeof(kExpectedProducerSave),&g_producer_save_hook},
        {true,kFoProducerFinishRva,reinterpret_cast<void*>(&producer_finish_hook),kExpectedFoFinish,sizeof(kExpectedFoFinish),&g_fo_finish_hook},
        {true,kFoProducerCancelRva,reinterpret_cast<void*>(&producer_cancel_hook),kExpectedFoCancel,sizeof(kExpectedFoCancel),&g_fo_cancel_hook},
        {true,kFoProducerCommandPushRva,reinterpret_cast<void*>(&producer_command_push_hook),kExpectedFoCommandPush,sizeof(kExpectedFoCommandPush),&g_fo_command_push_hook},
        {true,kFoProducerActDeleteRva,reinterpret_cast<void*>(&producer_act_delete_hook),kExpectedFoActDelete,sizeof(kExpectedFoActDelete),&g_fo_act_delete_hook},
        {true,kFoProducerClearRva,reinterpret_cast<void*>(&producer_clear_hook),kExpectedFoClear,sizeof(kExpectedFoClear),&g_fo_clear_hook},
    }};
    struct Context {
        const std::array<SignatureSpec,12>& signatures;
        const std::array<HookSpec,10>& hooks;
        std::exception_ptr error{};
    } context{signatures,hooks};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t index) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0: {
            const auto& spec=c.signatures[index];
            HMODULE module=spec.fleet?g_fleet_ops:g_armada;
            const bool matches=module&&std::memcmp(
                at(module,spec.rva),spec.expected,spec.length)==0;
            if(!matches){
                log_message("Continuous production signature mismatch at:");
                log_message(spec.name);
            }
            return matches;
        }
        case 1:log_message("Continuous production signatures mismatch; disabled");return 1;
        case 2: {
            const auto& spec=c.hooks[index];
            HMODULE module=spec.fleet?g_fleet_ops:g_armada;
            return g_api->install_inline_hook(at(module,spec.rva),
                spec.replacement,spec.length,spec.expected,spec.hook);
        }
        case 3:log_message("Continuous production hook installation incomplete; feature disabled safely");return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const bool result=a2fo_rs_c_queue_install_hooks(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

}  // namespace

// Startup capability query for core-provided Producer event consumers. A
// loaded FeaturePack alone does not prove its checked queue hooks succeeded.
extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ProducerEventsReady() noexcept {
    return g_repeat_ready;
}

bool initialize_queue_enhancements(const A2FO_ModuleApi* api,
                                   HMODULE armada,
                                   HMODULE fleet_ops) noexcept {
    g_api=api;
    g_armada=armada;
    g_fleet_ops=fleet_ops;
    struct Context {std::exception_ptr error{};} context;
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return g_api&&g_api->install_inline_hook&&g_armada&&g_fleet_ops;
        case 1:InitializeCriticalSection(&g_queue_lock);
            g_queue_lock_ready=true;return 1;
        case 2:try {g_continuous.reserve(256);
                    g_pending_queue.reserve(256);return 1;}
            catch(...){return 0;}
        case 3:log_message("Queue state allocation failed; enhancements disabled");return 1;
        case 4:return signature_matches(g_armada,
            kGameObjectQueueClassCommandRva,
            kExpectedGameObjectQueueClassCommand);
        case 5:return install_hook(g_armada,
            kGameObjectQueueClassCommandRva,
            reinterpret_cast<void*>(&game_object_queue_class_command_hook),
            kExpectedGameObjectQueueClassCommand,
            g_game_object_queue_class_command_hook);
        case 6:log_message("Ctrl-fill queue hook signature mismatch; disabled");return 1;
        case 7:log_message("Ctrl-click queue fill enabled (10 units per logical slot)");return 1;
        case 8:g_repeat_ready=install_repeat_hooks();return g_repeat_ready;
        case 9:log_message("Ctrl+Alt continuous production enabled as xINF "
            "with synchronized orders and save markers");return 1;
        case 10:g_grouped_queue_ui_ready=install_grouped_queue_ui();
            return g_grouped_queue_ui_ready;
        case 11:log_message(
            "Grouped build queue enabled: 10 visible slots x 10 units "
            "(100 logical jobs), with xN/xINF counters");return 1;
        case 12:log_message(
            "Grouped build queue presentation unavailable; native "
            "ten-slot display retained");return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const bool result=a2fo_rs_c_queue_initialize(&context,step);
    if(context.error)std::terminate();
    return result;
}

extern "C" __declspec(dllexport)
std::uint32_t __cdecl A2FO_ProducerPushRefit(
    void* producer, void* target_class) {
    return push_refit_checked(producer, target_class);
}

extern "C" __declspec(dllexport)
bool __cdecl A2FO_ProducerCancelQueuedJob(void* producer,
                                          std::uint32_t queue_id) {
    struct Context {void* producer;std::uint32_t queue_id;}
        context{producer,queue_id};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:return g_fo_act_delete_hook.gateway!=nullptr;
        case 1:return queued_target_class(c.producer,c.queue_id)!=nullptr;
        case 2:delete_native_queue_item(c.producer,c.queue_id);return 1;
        case 3:materialize_pending_queue(c.producer);return 1;
        default:return 0;
        }
    };
    return a2fo_rs_c_queue_export_cancel(&context,
        reinterpret_cast<std::uintptr_t>(producer),queue_id,g_repeat_ready,
        step);
}

bool producer_logical_queue_has_room(void* producer) noexcept {
    struct Context {void* producer;GroupedQueueView view{};}
        context{producer};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:c.view=collect_grouped_queue(c.producer);return 1;
        case 1:return logical_queue_units(c.view);
        case 2:return c.view.count;
        case 3:return c.view.groups[c.view.count-1].infinite;
        case 4:return c.view.groups[c.view.count-1].count;
        default:return 0;
        }
    };
    return a2fo_rs_c_queue_export_room(&context,
        reinterpret_cast<std::uintptr_t>(producer),g_repeat_ready,step);
}

extern "C" __declspec(dllexport)
bool __cdecl A2FO_ProducerLogicalQueueHasRoom(void* producer) {
    return producer_logical_queue_has_room(producer);
}

extern "C" __declspec(dllexport)
std::uint32_t __cdecl A2FO_ProducerLogicalQueueSize(void* producer) {
    struct Context {void* producer;} context{producer};
    const auto step=+[](void* opaque,std::uint32_t,
        std::size_t) noexcept ->std::uintptr_t {
        const auto& c=*static_cast<Context*>(opaque);
        return logical_queue_units(collect_grouped_queue(c.producer));
    };
    return a2fo_rs_c_queue_export_size(&context,
        reinterpret_cast<std::uintptr_t>(producer),g_repeat_ready,step);
}

extern "C" __declspec(dllexport)
bool __cdecl A2FO_ProducerCanQueueExtendedBuild(
    void* producer, void* target_class) {
    struct Context {void* producer;void* target_class;GroupedQueueView view{};}
        context{producer,target_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:c.view=collect_grouped_queue(c.producer);return 1;
        case 1:return logical_queue_has_room_for(c.view,c.target_class);
        default:return 0;
        }
    };
    return a2fo_rs_c_queue_export_can(&context,
        reinterpret_cast<std::uintptr_t>(producer),
        reinterpret_cast<std::uintptr_t>(target_class),g_repeat_ready,step);
}

extern "C" __declspec(dllexport)
bool __cdecl A2FO_ProducerQueueExtendedBuild(
    void* producer, void* target_class) {
    struct Context {void* producer;void* target_class;GroupedQueueView view{};
        std::exception_ptr error{};}
        context{producer,target_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t arg) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return native_queue_count(c.producer);
        case 1:c.view=collect_grouped_queue(c.producer);return 1;
        case 2:return modifier_key_down(kCommandControlPointerRva,VK_CONTROL);
        case 3:return modifier_key_down(kCommandAltPointerRva,VK_MENU);
        case 4:return logical_queue_can_enable_continuous(
            c.view,c.target_class);
        case 5:return logical_queue_has_room_for(c.view,c.target_class);
        case 6:a2fo_call_thiscall_2(
            g_game_object_queue_class_command_hook.gateway,c.producer,arg,
            reinterpret_cast<std::uintptr_t>(c.target_class));return 1;
        case 7:if(!g_logged_direct_extended_button_order){
            g_logged_direct_extended_button_order=true;
            const char* marker_name=arg==kContinuousMarkerCommand
                ?"continuous":arg==kQueueFillMarkerCommand
                ?"fill-to-10":"single";
            char message[160];
            std::snprintf(message,sizeof(message),
                "Direct overflow-button order preserved %s modifier "
                "semantics and emitted synchronized queue marker",
                marker_name);
            log_message(message);
            }return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const bool result=a2fo_rs_c_queue_export_overflow(&context,
        reinterpret_cast<std::uintptr_t>(producer),
        reinterpret_cast<std::uintptr_t>(target_class),g_repeat_ready,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

}  // namespace a2fo
