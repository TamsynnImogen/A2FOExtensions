/*
 * Presentation-only callback contract between HybridBuild's popup hook owner
 * and the optional RefitYards gameplay module.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#define A2FO_REFIT_UI_BRIDGE_VERSION 2u

struct A2FO_RefitUiItem {
    void* target_class;
    std::uint32_t enabled;
};

struct A2FO_RefitUiBridge {
    std::uint32_t struct_size;
    std::uint32_t version;

    // Null output is a count query. Returned items are copied immediately by
    // HybridBuild and need remain valid only for the duration of this call.
    std::size_t (*enumerate_items)(
        void* source, A2FO_RefitUiItem* output,
        std::size_t capacity) noexcept;

    // Queues the synchronized class-command marker. It must not mutate the
    // refit state directly on this local UI path.
    bool (*request_refit)(void* source, void* target_class) noexcept;

    // Queues a synchronized cancellation marker. HybridBuild invokes this
    // after the selected source receives the ordinary native Halt action.
    bool (*cancel_refit)(void* source) noexcept;
};

using A2FO_RegisterRefitUiBridgeFn = bool (__cdecl*)(
    const A2FO_RefitUiBridge* bridge);
