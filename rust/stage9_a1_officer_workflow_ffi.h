#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    A2FO_A1_OFFICER_EVENT_ADMIT = 0,
    A2FO_A1_OFFICER_EVENT_FINISHED = 1,
    A2FO_A1_OFFICER_EVENT_DESTROYING = 2,
    A2FO_A1_OFFICER_EVENT_FINISHING = 3,
    A2FO_A1_OFFICER_EVENT_STARTING_EFFECT = 4,
};

typedef uintptr_t (*A2foA1OfficerWorkflowStep)(
    void* context, uint32_t operation, uintptr_t item);

typedef struct A2foA1OfficerSanitizedSave {
    uint32_t completed;
    int32_t gain;
} A2foA1OfficerSanitizedSave;

// Queue: op 0 head; up to ten iterations of op 1 classify current item then
// op 2 read that item's NEXT. Native checks producer non-null and owns fields.
uint32_t a2fo_rs_stage9_a1_officer_workflow_queue(
    void* context, A2foA1OfficerWorkflowStep step);

// Event: native validates event/readiness/producer. ADMIT invokes op 0 classify
// target, op 1 completion flag only when classified, then op 2 admit with
// CURRENT producer/target. STARTING_EFFECT invokes op 3 suppress and negates.
// DESTROYING invokes op 4 clear credit. Other kinds return true without calls.
uint8_t a2fo_rs_stage9_a1_officer_workflow_event(
    uint32_t kind, void* context, A2foA1OfficerWorkflowStep step);

uint8_t a2fo_rs_stage9_a1_officer_workflow_save_valid(
    uint8_t loaded, uint32_t magic, uint32_t version,
    uint32_t expected_magic, uint32_t expected_version);

// Caller supplies private writable POD. maximum is the existing native
// clamped policy value; the u32 cast preserves the original load comparison.
void a2fo_rs_stage9_a1_officer_workflow_sanitize(
    uint32_t completed, int32_t gain, int32_t maximum,
    A2foA1OfficerSanitizedSave* output);

uint8_t a2fo_rs_stage9_a1_officer_workflow_retained_policy(
    uint8_t has_maximum, uint8_t has_gain, uint8_t race_nonempty);

#ifdef __cplusplus
}
#endif
