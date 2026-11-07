#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsRuntimeBytes {const uint8_t* data;size_t size;} A2foRsRuntimeBytes;
typedef struct A2foRsRuntimeEntry {A2foRsRuntimeBytes name;uint32_t state;} A2foRsRuntimeEntry;
typedef uint8_t (*A2foRsRuntimeInit)(void*,int32_t);
typedef void (*A2foRsRuntimeGate)(uint8_t);
typedef void (*A2foRsRuntimeEmit)(void*,const uint8_t*,size_t);
typedef size_t (*A2foRsRuntimeCount)(void*);
typedef A2foRsRuntimeEntry (*A2foRsRuntimeGet)(void*,size_t);
int32_t a2fo_rs_startup_index(int32_t);
uint8_t a2fo_rs_startup_initialize(int32_t,void*,A2foRsRuntimeInit);
uint8_t a2fo_rs_startup_selected(int32_t,void*,A2foRsRuntimeCount,A2foRsRuntimeGet);
uint64_t a2fo_rs_runtime_state(int32_t,uint32_t);
uint8_t a2fo_rs_runtime_initial(int32_t,uint8_t*);
uint8_t a2fo_rs_runtime_prepare(int32_t,uint8_t,void*,A2foRsRuntimeInit,A2foRsRuntimeInit);
void a2fo_rs_runtime_finish(uint32_t,A2foRsRuntimeGate,A2foRsRuntimeGate);
uint8_t a2fo_rs_runtime_roots_match(const A2foRsRuntimeBytes*,size_t,const A2foRsRuntimeBytes*,size_t);
uint8_t a2fo_rs_runtime_apply(uint8_t,uint8_t,const A2foRsRuntimeEntry*,size_t,A2foRsRuntimeEmit,void*,A2foRsRuntimeGate,A2foRsRuntimeGate);
void a2fo_rs_runtime_status(A2foRsRuntimeBytes,uint8_t,A2foRsRuntimeEmit,void*);
uint8_t a2fo_rs_runtime_startup_ready(A2foRsRuntimeBytes);
#ifdef __cplusplus
}
#endif
