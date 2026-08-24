/*
 * RefitYards callback registration and fail-open dispatch wrappers. The
 * FeaturePack remains the sole owner of Producer and synchronized-command
 * hooks; the optional refit module owns policy and per-Craft state.
 */

#include "refit_queue_bridge_client.hpp"

#include "refit_queue_bridge_api.hpp"

#include <windows.h>

#include <cstring>

namespace {

A2FO_RefitQueueBridge g_bridge{};
volatile LONG g_bridge_ready = 0;

bool valid_bridge(const A2FO_RefitQueueBridge* bridge) noexcept {
    return bridge && bridge->version == A2FO_REFIT_QUEUE_BRIDGE_VERSION &&
        bridge->struct_size >= sizeof(A2FO_RefitQueueBridge) &&
        bridge->consume_synchronized_command &&
        bridge->cancel_synchronized_command && bridge->has_waiting_job &&
        bridge->job_finished && bridge->job_removed;
}

const A2FO_RefitQueueBridge* bridge() noexcept {
    return InterlockedCompareExchange(&g_bridge_ready, 0, 0) != 0
        ? &g_bridge : nullptr;
}

}  // namespace

extern "C" __declspec(dllexport)
bool __cdecl A2FO_RegisterRefitQueueBridge(
    const A2FO_RefitQueueBridge* value) {
    if (!valid_bridge(value) ||
        InterlockedCompareExchange(&g_bridge_ready, 0, 0) != 0) {
        return false;
    }
    std::memcpy(&g_bridge, value, sizeof(g_bridge));
    InterlockedExchange(&g_bridge_ready, 1);
    return true;
}

namespace a2fo {

bool consume_refit_synchronized_command(
    void* source, void* target_class) noexcept {
    const auto* callbacks = bridge();
    return callbacks && callbacks->consume_synchronized_command(
        source, target_class);
}

bool cancel_refit_synchronized_command(void* source) noexcept {
    const auto* callbacks = bridge();
    return callbacks && callbacks->cancel_synchronized_command(source);
}

bool refit_has_waiting_job(void* producer) noexcept {
    const auto* callbacks = bridge();
    return callbacks && callbacks->has_waiting_job(producer);
}

void notify_refit_job_finished(
    void* producer, std::uint32_t queue_id,
    void* target_class, void* result) noexcept {
    const auto* callbacks = bridge();
    if (callbacks) {
        callbacks->job_finished(
            producer, queue_id, target_class, result);
    }
}

void notify_refit_job_removed(
    void* producer, std::uint32_t queue_id, void* target_class,
    std::uint32_t removal_kind) noexcept {
    const auto* callbacks = bridge();
    if (callbacks) {
        callbacks->job_removed(
            producer, queue_id, target_class, removal_kind);
    }
}

}  // namespace a2fo
