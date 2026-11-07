#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct A2foA1QuarterStats {
    uint32_t visible;
    uint32_t matches;
    uint32_t changed;
    uint32_t highest;
    size_t remaining;
    uint8_t truncated;
} A2foA1QuarterStats;

// The native caller supplies a readable NUL-terminated name. The parser
// reads only the prefix and decimal suffix required by the original parser.
uint8_t a2fo_rs_stage9_a1_quarters_parse(const char* name, uint32_t* output);

// Callback operations: 0 -> pointer to native 96-byte NUL name scratch;
// 1 -> flags u32 or 0x100000000 if the flags cell is not writable;
// 2 -> write flags from arg and return 0; 3 -> first child pointer;
// 4 -> next sibling pointer of node. Name storage remains valid until the
// next callback. Context and stats do not alias; callbacks cannot access the
// walker's internal stats and do not retain supplied pointers.
typedef uint64_t (*A2foA1QuarterStep)(void* context, uint32_t operation,
                                      uintptr_t node, uintptr_t argument);

// Native C++ supplies its existing depth, budget, and hidden-flag constant.
// Stats are copied to an owned Rust local before callbacks and written back
// after all callbacks return; no native memory layout is passed through.
void a2fo_rs_stage9_a1_quarters_walk(
    uintptr_t node, size_t depth, size_t max_depth, uint32_t hidden_flag,
    void* context, A2foA1QuarterStep step, A2foA1QuarterStats* stats);

#ifdef __cplusplus
}
#endif
