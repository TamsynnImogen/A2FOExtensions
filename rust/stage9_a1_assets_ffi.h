#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct A2foA1AssetView {
    const uint8_t* bytes;
    size_t size;
    int32_t width;
    int32_t height;
    uint32_t indexed;
    uint32_t legacy;
} A2foA1AssetView;

typedef struct A2foA1CursorDecision {
    uint8_t standard_found;
    uint8_t standard_invalid;
    uint8_t select_found;
    uint8_t select_invalid;
} A2foA1CursorDecision;

typedef struct A2foA1TeamPlan {
    uint32_t parsed_files;
    uint32_t indexed_values;
    uint32_t legacy_aliases;
    uint32_t resolved_colors;
    uint8_t active;
} A2foA1TeamPlan;

// Callback returns 0 for absent/false, 1 for present/true, and 2 for a
// native exception retained by the caller. The view is consumed before the
// next callback; no Rust reference survives a callback that mutates storage.
typedef uint8_t (*A2foA1AssetStep)(void* context, uint32_t operation,
                                   uint32_t index, uint32_t argument,
                                   A2foA1AssetView* output);

void a2fo_rs_stage9_a1_cursor_roots(uint32_t roots, void* context,
                                    A2foA1AssetStep step);
void a2fo_rs_stage9_a1_cursor_lines(void* context, A2foA1AssetStep step,
                                    A2foA1CursorDecision* output);
void a2fo_rs_stage9_a1_team_find(void* context, A2foA1AssetStep step);
void a2fo_rs_stage9_a1_team_load(uint32_t roots, void* context,
                                 A2foA1AssetStep step, A2foA1TeamPlan* output);
uint32_t a2fo_rs_stage9_a1_team_apply(uint32_t slots, void* context,
                                       A2foA1AssetStep step);
uint8_t a2fo_rs_stage9_a1_team_log_first8(int32_t pass);

#ifdef __cplusplus
}
#endif
