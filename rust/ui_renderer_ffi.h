#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsUiReply { int64_t value; uint8_t failed; } A2foRsUiReply;
typedef void (*A2foRsUiStep)(void*,uint32_t,uint32_t,A2foRsUiReply*);
int32_t a2fo_rs_ui_run(uint32_t,void*,A2foRsUiStep);
uint8_t a2fo_rs_ui_state(const uint32_t*,uint32_t,uint8_t);
uint8_t a2fo_rs_ui_used_texture(uint32_t,uint32_t,uint32_t);
uint32_t a2fo_rs_ui_blend(uint32_t,uint8_t);
uint8_t a2fo_rs_ui_positions(const uint8_t*,uint32_t,uint32_t);
uint8_t a2fo_rs_ui_upload(uint32_t,uint32_t,uint32_t,uint32_t,size_t);
uint8_t a2fo_rs_ui_indices(const uint8_t*,size_t,uint32_t,uint32_t,uint32_t);
uint32_t a2fo_rs_ui_capacity(uint32_t);
uint32_t a2fo_rs_ui_filter(uint32_t,uint32_t,uint32_t);
uint32_t a2fo_rs_ui_topology(uint32_t);
uint8_t a2fo_rs_ui_viewport(const uint32_t*,float,float,uint32_t,uint32_t);
uint8_t a2fo_rs_ui_clip(const int32_t*,const int32_t*,float*);
#ifdef __cplusplus
}
#endif
