#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t (*A2foA1OfficerUiStep)(
    void* context, uint32_t operation, uintptr_t argument);

typedef struct A2foA1OfficerUiOwnerOut {
    uintptr_t button;
    size_t index;
    size_t duplicates;
} A2foA1OfficerUiOwnerOut;

typedef struct A2foA1OfficerUiFreeOut {
    uintptr_t button;
    size_t index;
} A2foA1OfficerUiFreeOut;

// Native owns checked array access. Op 0 reads current slot(index); op 1
// classifies captured candidate; op 2 checks race of that same candidate.
uintptr_t a2fo_rs_stage9_a1_officer_ui_first_target(
    void* context, size_t capacity, A2foA1OfficerUiStep step);
uint8_t a2fo_rs_stage9_a1_officer_ui_enabled(
    uint32_t completed, uint32_t queued, int32_t maximum);

// Owner: op 0 reads current slot(index), op 1 matches captured candidate's
// ModeInfo identity, op 2 clears a duplicate candidate. Output is private POD.
void a2fo_rs_stage9_a1_officer_ui_owner(
    void* context, size_t capacity, size_t usable,
    A2foA1OfficerUiStep step, A2foA1OfficerUiOwnerOut* output);

// First free: op 3 checks current popup binding(index), then op 0 rereads
// that slot if free. A null candidate still selects the free index.
void a2fo_rs_stage9_a1_officer_ui_first_free(
    void* context, size_t usable, A2foA1OfficerUiStep step,
    A2foA1OfficerUiFreeOut* output);

uint8_t a2fo_rs_stage9_a1_officer_ui_menu_action(
    uint32_t menu, uint32_t build_menu, uint32_t root_menu);
size_t a2fo_rs_stage9_a1_officer_ui_usable_count(
    int32_t rectangle_count, size_t capacity);
// Native preserves original short-circuit field reads before this pure call.
uint8_t a2fo_rs_stage9_a1_officer_ui_displace(
    uint8_t different_slot, uint8_t displaced_present,
    uint8_t bound, uint8_t native_speed);

#ifdef __cplusplus
}
#endif
