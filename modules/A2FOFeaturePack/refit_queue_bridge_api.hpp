/*
 * File: modules/A2FOFeaturePack/refit_queue_bridge_api.hpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Narrow callback contract between the Producer hook owner and the
 *          optional RefitYards module.
 */

#pragma once

#include <cstdint>

#define A2FO_REFIT_QUEUE_BRIDGE_VERSION 2u

// Reserved inside A2FO's synchronized typed-class command channel. The
// FeaturePack receiver consumes this marker before Armada can install it as a
// live AI command on the source Craft.
#define A2FO_REFIT_CLASS_COMMAND 0xa3u
#define A2FO_REFIT_CANCEL_CLASS_COMMAND 0xa4u

enum A2FO_RefitQueueRemovalKind : std::uint32_t {
    A2FO_REFIT_QUEUE_CANCELLED = 0,
    A2FO_REFIT_QUEUE_DELETED = 1,
    A2FO_REFIT_QUEUE_CLEARED = 2,
    A2FO_REFIT_QUEUE_PRODUCER_DESTROYED = 3,
};

struct A2FO_RefitQueueBridge {
    std::uint32_t struct_size;
    std::uint32_t version;

    // Runs on the synchronized receive path. Return true only when the source
    // Craft and requested destination form a valid refit request.
    bool (*consume_synchronized_command)(
        void* source, void* target_class) noexcept;

    // Runs on the synchronized receive path for a Halt-issued cancellation.
    // Cancellation is idempotent so a late or repeated Halt remains harmless.
    bool (*cancel_synchronized_command)(void* source) noexcept;

    // Prevent continuous-production refill from stealing a newly opened slot
    // while an arrived refit is waiting to enter this Producer FIFO.
    bool (*has_waiting_job)(void* producer) noexcept;

    // Native Producer completion has already created and published result.
    // queue_id identifies the exact refit sidecar even when classes repeat.
    void (*job_finished)(void* producer, std::uint32_t queue_id,
                         void* target_class, void* result) noexcept;

    // Sent before or during native queue removal while producer is still
    // readable. queue_id is zero only for whole-Producer destruction.
    void (*job_removed)(void* producer, std::uint32_t queue_id,
                        void* target_class,
                        std::uint32_t removal_kind) noexcept;
};

using A2FO_RegisterRefitQueueBridgeFn = bool (__cdecl*)(
    const A2FO_RefitQueueBridge* bridge);
using A2FO_ProducerPushRefitFn = std::uint32_t (__cdecl*)(
    void* producer, void* target_class);
