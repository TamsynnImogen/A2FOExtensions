#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct A2foA1OfficerFinishIn {
    uint32_t completed;
    int32_t upgrade_gain;
    int32_t credited_gain;
} A2foA1OfficerFinishIn;

typedef struct A2foA1OfficerFinishOut {
    uint8_t admitted;
    uint32_t completed;
    int32_t upgrade_gain;
    int32_t credited_gain;
    int32_t current_delta;
    uint8_t clear_previous;
} A2foA1OfficerFinishOut;

// Pure arithmetic only. Native code owns all memory reads/writes, identity
// checks, critical sections, and calls. Inputs obey existing state invariants:
// gain is clamped to 0..1,000,000; stored upgrade/credited gains are >= 0.
int32_t a2fo_rs_stage9_a1_officer_adjust(int32_t current, int32_t delta);
int32_t a2fo_rs_stage9_a1_officer_maximum(int32_t value);
int32_t a2fo_rs_stage9_a1_officer_gain(int32_t value);
int32_t a2fo_rs_stage9_a1_officer_desired(int32_t base, int32_t upgrade);
uint8_t a2fo_rs_stage9_a1_officer_admit(uint32_t completed,
                                         uint32_t queued, int32_t maximum);
uint8_t a2fo_rs_stage9_a1_officer_active_decrement(int32_t value,
                                                    int32_t* output);
// A blocked finish leaves output untouched. The lead calls this while holding
// the existing officer-state critical section and performs mutations natively.
void a2fo_rs_stage9_a1_officer_finish(
    const A2foA1OfficerFinishIn* input, int32_t base, int32_t maximum,
    int32_t gain, uint8_t same_team, uint8_t current_team_present,
    A2foA1OfficerFinishOut* output);

#ifdef __cplusplus
}
#endif
