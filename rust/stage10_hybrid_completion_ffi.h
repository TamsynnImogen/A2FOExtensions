#ifndef A2FO_STAGE10_HYBRID_COMPLETION_FFI_H
#define A2FO_STAGE10_HYBRID_COMPLETION_FFI_H

#include <cstddef>
#include <cstdint>

extern "C" {
using A2foHybridStage10Probe = std::uint32_t (*)(void*, std::size_t,
                                                std::uint32_t);
struct A2foHybridStage10Result {
    std::size_t index;
    std::uint32_t status;
};
// kind 0: first match; kind 1: visit all; kind 2: first exact,
// otherwise first empty. In kinds 0-2 a callback returns 0 (continue),
// 1 (match), or 2 (abort). Kind 4 uses 1 for preferred
// and 5 for first fallback; kind 5 uses phase 0 inclusion and phase 1+output
// slot for native emission; kind 6 stops at the first callback return 1.
// No native pointer is encoded
// in the return value. An absent match is represented by index == count.
A2foHybridStage10Result a2fo_rs_stage10_hybrid_visit(
    std::uint32_t kind, std::size_t count, void* context,
    A2foHybridStage10Probe probe);
// Live loop condition is re-read before every binding. Kind 0 stops at the
// first match; kind 1 visits all current bindings.
A2foHybridStage10Result a2fo_rs_stage10_hybrid_live_bindings(
    std::uint32_t kind, void* context, A2foHybridStage10Probe probe);
using A2foHybridStage10QueueProbe = std::uint32_t (*)(
    void*, std::uint32_t, std::uint32_t*);
struct A2foHybridStage10QueueResult {
    std::uint32_t id;
    std::uint32_t status;
};
// phase 0 returns 0 nonmatch, 1 match, 2 end, 3 invalid and writes a raw ID
// only for a match; phase 1 advances the native cursor after processing.
A2foHybridStage10QueueResult a2fo_rs_stage10_hybrid_queue(
    std::size_t limit, std::uint32_t preferred_id, void* context,
    A2foHybridStage10QueueProbe probe);
// Native callback copies backing slot `from` from adjacent slot `to`.
void a2fo_rs_stage10_hybrid_rotate(
    std::size_t from, std::size_t destination, void* context,
    void (*copy)(void*, std::size_t, std::size_t));
struct A2foHybridStage10Roots {
    std::size_t yard_index;
    std::size_t construct_index;
};
A2foHybridStage10Roots a2fo_rs_stage10_hybrid_roots(
    std::size_t count, bool has_yard, void* context,
    A2foHybridStage10Probe probe);
struct A2foHybridStage10RefitPositions {
    std::size_t refit_index;
    std::size_t back_index;
};
A2foHybridStage10RefitPositions a2fo_rs_stage10_hybrid_refit_positions(
    std::size_t count, void* context, A2foHybridStage10Probe probe);
using A2foHybridStage10SubmenuStep = std::uint32_t (*)(
    void*, std::uint32_t, std::size_t, std::size_t, std::size_t);
struct A2foHybridStage10SubmenuResult {
    std::size_t count;
    std::uint32_t status;
};
A2foHybridStage10SubmenuResult a2fo_rs_stage10_hybrid_submenus(
    std::uint32_t kind, std::size_t root_count, std::size_t child_count,
    void* context, A2foHybridStage10SubmenuStep step);
using A2foHybridStage10PublishStep = std::size_t (*)(
    void*, std::uint32_t, std::size_t, std::uint32_t);
struct A2foHybridStage10PublishResult {
    std::uint32_t declared[4];
    std::size_t published[4];
};
A2foHybridStage10PublishResult a2fo_rs_stage10_hybrid_publish(
    std::size_t entry_count, std::size_t capacity, void* context,
    A2foHybridStage10PublishStep step);
using A2foHybridStage10UniqueProbe = std::uint32_t (*)(
    void*, std::uint32_t, std::size_t, std::size_t);
A2foHybridStage10Result a2fo_rs_stage10_hybrid_unique(
    std::size_t candidate_count, std::size_t capacity, void* context,
    A2foHybridStage10UniqueProbe probe);
}

#endif
