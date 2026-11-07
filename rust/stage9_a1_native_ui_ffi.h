#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef size_t (*A2foA1NativeUiStep)(void*,uint32_t,size_t,size_t);
size_t a2fo_rs_stage9_a1_native_ui_scan(uint8_t,size_t,size_t,void*,A2foA1NativeUiStep);
uint8_t a2fo_rs_stage9_a1_native_ui_action(void*,A2foA1NativeUiStep);
uint8_t a2fo_rs_stage9_a1_native_ui_transport(int32_t);
uint8_t a2fo_rs_stage9_a1_native_ui_special(int32_t,void*,A2foA1NativeUiStep);
uint8_t a2fo_rs_stage9_a1_native_ui_rig(uint8_t,uint8_t,void*,A2foA1NativeUiStep);
uint8_t a2fo_rs_stage9_a1_native_ui_lookup(uint8_t,uint32_t);
uint8_t a2fo_rs_stage9_a1_native_ui_target(uint8_t,uint8_t);
uint8_t a2fo_rs_stage9_a1_native_ui_scout_id(int32_t);
#ifdef __cplusplus
}
#endif
