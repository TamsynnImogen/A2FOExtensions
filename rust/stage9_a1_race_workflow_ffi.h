#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foA1RaceWorkflowStep)(void*,uint32_t);
void a2fo_rs_stage9_a1_race_loaded(void*,A2foA1RaceWorkflowStep);
void a2fo_rs_stage9_a1_race_record(void*,A2foA1RaceWorkflowStep);
void a2fo_rs_stage9_a1_race_present(void*,A2foA1RaceWorkflowStep);
#ifdef __cplusplus
}
#endif
