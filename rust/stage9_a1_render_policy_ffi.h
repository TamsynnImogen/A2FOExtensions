#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef size_t (*A2foA1RenderStep)(void*,uint32_t,size_t);
uint8_t a2fo_rs_stage9_a1_render_energy(uint8_t,size_t,void*,A2foA1RenderStep,size_t*);
size_t a2fo_rs_stage9_a1_render_collect(size_t,void*,A2foA1RenderStep);
uint8_t a2fo_rs_stage9_a1_render_multi_admit(size_t);
void a2fo_rs_stage9_a1_render_visit(size_t,void*,A2foA1RenderStep);
#ifdef __cplusplus
}
#endif
