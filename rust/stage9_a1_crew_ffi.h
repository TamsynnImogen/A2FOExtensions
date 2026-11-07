#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int32_t (*A2foA1CrewFallback)(void*);
uint32_t a2fo_rs_stage9_a1_crew_health(uint32_t,uint32_t);
void a2fo_rs_stage9_a1_crew_colour_words(uint32_t,uint32_t*);
void a2fo_rs_stage9_a1_crew_colour(uint32_t,uint32_t,uint8_t,void*,A2foA1CrewFallback,uint32_t*);
#ifdef __cplusplus
}
#endif
