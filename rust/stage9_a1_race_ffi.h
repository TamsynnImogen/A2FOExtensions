#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct A2foA1RaceNext {
    uint8_t found;
    uint8_t nonempty;
    uint8_t neutral;
} A2foA1RaceNext;

typedef struct A2foA1RaceCountPlan {
    uint8_t append;
    uint8_t occupied;
} A2foA1RaceCountPlan;

// op 0 probes a declared row; op 1 probes the next undeclared row.
typedef uint8_t (*A2foA1RaceProbe)(void* context, uint32_t operation,
                                   uint32_t index, A2foA1RaceNext* next);
typedef uint8_t (*A2foA1RaceField)(void* context, uint32_t index);
typedef uint8_t (*A2foA1RaceCompare)(void* context);
// 0/1 read normal/lots; 2/3 write normal/lots. Native owns row addresses.
typedef uint8_t (*A2foA1RaceResource)(void* context, uint32_t operation,
                                      uint32_t command, int32_t* value);

void a2fo_rs_stage9_a1_race_count_plan(int32_t declared, void* context,
                                       A2foA1RaceProbe probe,
                                       A2foA1RaceCountPlan* output);
uint8_t a2fo_rs_stage9_a1_race_synthesize(void* context,
                                            A2foA1RaceProbe probe);
uint8_t a2fo_rs_stage9_a1_race_entry_admit(int32_t synthetic_index,
                                             const char* key,
                                             const char* output,
                                             uintptr_t found,
                                             uintptr_t output_size,
                                             size_t required,
                                             void* context,
                                             A2foA1RaceCompare compare);
uint8_t a2fo_rs_stage9_a1_race_field(uint32_t count, void* context,
                                      A2foA1RaceField field);
uint8_t a2fo_rs_stage9_a1_race_nonnegative(const uint8_t* text,
                                             size_t size_with_nul,
                                             size_t consumed,
                                             int64_t parsed,
                                             uint8_t range_error);
uint8_t a2fo_rs_stage9_a1_race_resources(void* context,
                                           A2foA1RaceResource resource);
uint8_t a2fo_rs_stage9_a1_race_log_case(int32_t incident);

#ifdef __cplusplus
}
#endif
