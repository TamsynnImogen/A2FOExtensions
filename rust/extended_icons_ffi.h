#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uintptr_t (*A2foRsIconStep)(void*,uint32_t,uint32_t);
void a2fo_rs_icon_create(void*,A2foRsIconStep);
void a2fo_rs_icon_destroy(void*,A2foRsIconStep);
void a2fo_rs_icon_virtuals(void*,A2foRsIconStep);
void a2fo_rs_icon_hook(uint32_t,void*,A2foRsIconStep);
uint8_t a2fo_rs_icon_install(void*,A2foRsIconStep);
void a2fo_rs_icon_flash(float,uint32_t,uint8_t,float*,uint8_t*);
#ifdef __cplusplus
}
#endif
