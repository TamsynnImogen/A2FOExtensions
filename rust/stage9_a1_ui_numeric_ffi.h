#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int32_t (*A2foA1UiRoundBits)(uint64_t);
void a2fo_rs_stage9_a1_ui_scale(const void*,const uint64_t*,const uint64_t*,int32_t*,A2foA1UiRoundBits);
int32_t a2fo_rs_stage9_a1_ui_scale_scalar(int32_t,const uint64_t*,A2foA1UiRoundBits);
#ifdef __cplusplus
}
#endif
