#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Synchronous callback: op 0 = first child, op 1 = native name match,
   op 2 = next sibling. Return native pointer values as uintptr_t. */
typedef uintptr_t (*A2foA1NodeStep)(void* context, uint32_t operation,
                                    uintptr_t node);

/* Parent/requested guards remain in C++; callbacks own names and links. */
uintptr_t a2fo_rs_stage9_a1_node_find_direct_child(
    void* context, uintptr_t parent, uintptr_t requested,
    A2foA1NodeStep step);

/* Caller supplies a valid, disjoint budget span; callback must not access it. */
uintptr_t a2fo_rs_stage9_a1_node_find_named_bounded(
    void* context, uintptr_t node, uintptr_t requested, size_t depth,
    size_t* remaining, A2foA1NodeStep step);

uint32_t a2fo_rs_stage9_a1_node_hidden_flags(uint32_t flags,
                                             uint8_t hidden,
                                             uint32_t mask);

#ifdef __cplusplus
}
#endif
