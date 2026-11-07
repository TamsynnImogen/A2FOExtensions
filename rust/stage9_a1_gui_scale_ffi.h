#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void a2fo_rs_stage9_a1_gui_scale_control(const int32_t*,const int32_t*,void*,void*);
uint8_t a2fo_rs_stage9_a1_gui_scale_cinematic(uint8_t,uint8_t,const int32_t*,const int32_t*,void*,void*);
uint8_t a2fo_rs_stage9_a1_gui_scale_use_raw(uint8_t,const void*,const void*);
uint8_t a2fo_rs_stage9_a1_gui_scale_speed_valid(const void*);
const char* a2fo_rs_stage9_a1_gui_default_energy_sprite(uint8_t,uint8_t);
#ifdef __cplusplus
}
#endif
