#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int32_t (*A2foA1DisplayRound)(uint64_t);
uint8_t a2fo_rs_stage9_a1_display_energy(uint32_t,uint32_t,uint32_t*);
uint8_t a2fo_rs_stage9_a1_display_colour_finite(const uint32_t*);
uint8_t a2fo_rs_stage9_a1_display_number(uint8_t,uint32_t,A2foA1DisplayRound,int32_t*);
int32_t a2fo_rs_stage9_a1_display_nonnegative(int32_t);
uint8_t a2fo_rs_stage9_a1_display_has_maximum(int32_t);
#ifdef __cplusplus
}
#endif
