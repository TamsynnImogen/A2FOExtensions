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

constexpr std::uint32_t kQueueCapacity = 10;
constexpr std::uint32_t kBuildCommand = 0x19;
// Reserved within A2FO's typed-class order channel. The receive hook consumes
// these markers before Armada installs them as live object commands.
constexpr std::uint32_t kQueueFillMarkerCommand = 0xa1;
constexpr std::uint32_t kContinuousMarkerCommand = 0xa2;
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

extern "C" std::uintptr_t a2fo_call_thiscall_0(
    void* function, void* self);
extern "C" std::uintptr_t a2fo_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument);
extern "C" std::uintptr_t a2fo_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
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

struct ContinuousState {
    bool active = false;
    void* target_class = nullptr;
    std::uint32_t target_project_id = 0;
    unsigned paused_retry_ticks = 0;
};

std::unordered_map<std::uint32_t, ContinuousState> g_continuous;

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) {
    return reinterpret_cast<T*>(
        reinterpret_cast<std::uint8_t*>(module) + rva);
}

std::uint8_t* bytes(void* value) {
    return static_cast<std::uint8_t*>(value);
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

std::uint32_t fill_queue_checked(void* producer, void* target_class) {
    if (!producer || !target_class) return 0;
    if (hybrid_production_has_evolution_barrier(producer)) return 0;
    if (hybrid_production_has_queued_research_conflict(
            producer, target_class)) {
        return 0;
    }
    auto* producer_bytes = bytes(producer);
    const std::uint32_t initial = *reinterpret_cast<std::uint32_t*>(
        producer_bytes + kQueueCountOffset);
    std::uint32_t count = initial;
    const bool evolve = hybrid_production_is_evolve_target(
        producer, target_class);
    const std::uint32_t attempt_limit = evolve ? 1 : kQueueCapacity;
    for (std::uint32_t attempt = 0;
         attempt < attempt_limit && count < kQueueCapacity; ++attempt) {
        if (!dispatch_producer_event(
                A2FO_PRODUCER_EVENT_ADMIT, producer, target_class)) {
            break;
        }
        a2fo_call_thiscall_1(
            at(g_fleet_ops, kFoProducerPushCheckedRva), producer,
            reinterpret_cast<std::uintptr_t>(target_class));
        const std::uint32_t after = *reinterpret_cast<std::uint32_t*>(
            producer_bytes + kQueueCountOffset);
        if (after <= count) break;
        count = after;
    }
    return count > initial ? count - initial : 0;
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
    if (!fill && !continuous) {
        a2fo_call_thiscall_2(
            g_game_object_dequeue_class_command_hook.gateway, game_object,
            command, reinterpret_cast<std::uintptr_t>(target_class));
        return;
    }

    if (!g_repeat_ready || !game_object || !target_class) {
        log_message("Queue-enhancement marker rejected");
        return;
    }

    const std::uint32_t added = fill_queue_checked(game_object, target_class);
    char fill_message[96];
    std::snprintf(fill_message, sizeof(fill_message),
                  continuous
                      ? "Ctrl+Alt synchronized queue fill received: %lu added"
                      : "Ctrl synchronized queue fill received: %lu added",
                  static_cast<unsigned long>(added));
    log_message(fill_message);

    if (!continuous || hybrid_production_is_evolve_target(
            game_object, target_class)) {
        stop_continuous(game_object);
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
        log_message("Continuous production enabled by synchronized marker");
    } catch (...) {
        LeaveCriticalSection(&g_queue_lock);
        log_message("Continuous production marker could not be recorded");
    }
}

void try_refill(void* producer) {
    if (!g_repeat_ready || !producer) return;
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

    auto* producer_bytes = bytes(producer);
    const std::uint32_t before = *reinterpret_cast<std::uint32_t*>(
        producer_bytes + kQueueCountOffset);
    if (before >= kQueueCapacity) return;
    if (!dispatch_producer_event(
            A2FO_PRODUCER_EVENT_ADMIT, producer, target_class)) {
        return;
    }
    a2fo_call_thiscall_1(
        at(g_fleet_ops, kFoProducerPushCheckedRva), producer,
        reinterpret_cast<std::uintptr_t>(target_class));
    const std::uint32_t after = *reinterpret_cast<std::uint32_t*>(
        producer_bytes + kQueueCountOffset);
    if (after > before) {
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
    if (command != kBuildCommand || !target_class ||
        !modifier_key_down(kCommandControlPointerRva, VK_CONTROL) ||
        !g_repeat_ready) {
        a2fo_call_thiscall_2(
            g_game_object_queue_class_command_hook.gateway, game_object,
            command, reinterpret_cast<std::uintptr_t>(target_class));
        return;
    }

    if (!g_logged_build_order_path) {
        g_logged_build_order_path = true;
        log_message("First Producer build order reached");
    }

    const bool continuous =
        modifier_key_down(kCommandAltPointerRva, VK_MENU);
    a2fo_call_thiscall_2(
        g_game_object_queue_class_command_hook.gateway, game_object,
        continuous ? kContinuousMarkerCommand : kQueueFillMarkerCommand,
        reinterpret_cast<std::uintptr_t>(target_class));
    log_message(continuous
                    ? "Ctrl+Alt synchronized queue-fill marker queued"
                    : "Ctrl synchronized queue-fill marker queued");
}

void __attribute__((fastcall)) producer_command_push_hook(
    void* producer, void*, void* target_class) {
    if (producer && target_class &&
        !dispatch_producer_event(
            A2FO_PRODUCER_EVENT_ADMIT, producer, target_class)) {
        return;
    }
    if (producer && target_class &&
        hybrid_production_should_defer_construct_order(
            producer, target_class)) {
        return;
    }
    if (!g_repeat_ready || !producer || !target_class) {
        const std::uint32_t before = producer
            ? *reinterpret_cast<const std::uint32_t*>(
                  bytes(producer) + kQueueCountOffset)
            : 0;
        a2fo_call_thiscall_1(g_fo_command_push_hook.gateway, producer,
                            reinterpret_cast<std::uintptr_t>(target_class));
        if (producer && target_class) {
            const std::uint32_t after =
                *reinterpret_cast<const std::uint32_t*>(
                    bytes(producer) + kQueueCountOffset);
            finalize_hybrid_construct_order(
                producer, target_class, after > before);
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
    const std::uint32_t handle = object_handle(producer);
    try {
        EnterCriticalSection(&g_queue_lock);
        ContinuousState& state = g_continuous[handle];
        if (!g_logged_synchronized_build_path) {
            g_logged_synchronized_build_path = true;
            log_message("First synchronized Producer build command reached");
        }
        if (state.active && state.target_class == target_class &&
            !evolve_target) {
            state.active = false;
            state.target_class = nullptr;
            state.target_project_id = 0;
            suppress = true;
        } else {
            if (state.active) {
                state.active = false;
                state.target_class = nullptr;
                state.target_project_id = 0;
            }
        }
        LeaveCriticalSection(&g_queue_lock);
    } catch (...) {
        LeaveCriticalSection(&g_queue_lock);
        suppress = research_conflict || evolution_barrier;
    }

    const std::uint32_t before = *reinterpret_cast<const std::uint32_t*>(
        bytes(producer) + kQueueCountOffset);
    if (!suppress) {
        a2fo_call_thiscall_1(g_fo_command_push_hook.gateway, producer,
                            reinterpret_cast<std::uintptr_t>(target_class));
    }
    const std::uint32_t after =
        *reinterpret_cast<const std::uint32_t*>(
            bytes(producer) + kQueueCountOffset);
    finalize_hybrid_construct_order(
        producer, target_class, !suppress && after > before);
    if (g_synchronized_push_log_count < kSynchronizedPushLogLimit) {
        ++g_synchronized_push_log_count;
        char message[160];
        std::snprintf(
            message, sizeof(message),
            "Synchronized Producer queue result: target %lu, "
            "count %lu -> %lu (%s)",
            static_cast<unsigned long>(project_id(target_class)),
            static_cast<unsigned long>(before),
            static_cast<unsigned long>(after),
            research_conflict ? "research conflict rejected"
                              : evolution_barrier
                                  ? "evolution barrier rejected"
                              : suppress ? "repeat marker suppressed"
                                         : "forwarded");
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
    return a2fo_call_thiscall_0(g_fo_cancel_hook.gateway, producer);
}

void __attribute__((fastcall)) producer_act_delete_hook(
    void* producer, void*, std::uint32_t queue_id) {
    stop_continuous(producer);
    const std::uint32_t current_id = producer
        ? *reinterpret_cast<std::uint32_t*>(
              bytes(producer) + kCurrentQueueIdOffset)
        : 0;
    void* removed_class = queued_target_class(producer, queue_id);
    if (!removed_class && queue_id == current_id) {
        removed_class = current_build_target_class(producer);
    }
    if (queue_id != current_id &&
        charges_resources_when_queued(producer)) {
        if (void* target_class = removed_class) {
            dispatch_producer_event(
                A2FO_PRODUCER_EVENT_DELETED, producer, target_class);
        }
    }
    // queue_id is the authoritative refit sidecar key. Notify even when the
    // native item/class lookup is transiently empty during active deletion.
    if (producer && queue_id != 0) {
        notify_refit_job_removed(
            producer, queue_id, removed_class,
            A2FO_REFIT_QUEUE_DELETED);
    }
    a2fo_call_thiscall_1(g_fo_act_delete_hook.gateway, producer, queue_id);
    discard_hybrid_construct_placement(producer, queue_id);
}

void __attribute__((fastcall)) producer_clear_hook(void* producer, void*) {
    stop_continuous(producer);
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
    log_message("Ctrl-click queue fill enabled (ten native slots)");

    g_repeat_ready = install_repeat_hooks();
    if (g_repeat_ready) {
        log_message("Ctrl+Alt continuous production enabled with "
                    "synchronized orders and save markers");
    }
    return true;
}

extern "C" __declspec(dllexport)
std::uint32_t __cdecl A2FO_ProducerPushRefit(
    void* producer, void* target_class) {
    return push_refit_checked(producer, target_class);
}

}  // namespace a2fo
