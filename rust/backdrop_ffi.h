#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsBackdropData {uintptr_t words[12];int32_t ints[16];void* pixels;size_t count;} A2foRsBackdropData;
typedef struct A2foRsBackdropOps {void* context;uint8_t (*read)(void*,uintptr_t,void*,size_t);uint8_t (*callable)(void*,uintptr_t);uintptr_t (*action)(void*,uint32_t,A2foRsBackdropData*,const uint8_t*,size_t);} A2foRsBackdropOps;
uint8_t a2fo_rs_backdrop_init(const A2foRsBackdropOps*,uintptr_t,uintptr_t,uint8_t);
void a2fo_rs_backdrop_observe(const A2foRsBackdropOps*,uintptr_t);
uint8_t a2fo_rs_backdrop_active(const A2foRsBackdropOps*,uintptr_t);
uint8_t a2fo_rs_backdrop_frame(const A2foRsBackdropOps*,uintptr_t);
void a2fo_rs_backdrop_reset(const A2foRsBackdropOps*);
void a2fo_rs_backdrop_shutdown(const A2foRsBackdropOps*);
#ifdef __cplusplus
}
#endif
