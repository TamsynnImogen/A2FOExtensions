#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsA4Bytes { const uint8_t* data; size_t size; } A2foRsA4Bytes;
typedef struct A2foRsA4Candidate { uint32_t handle, queued; } A2foRsA4Candidate;
typedef uint32_t (*A2foRsYardConfig)(void*,uint32_t,A2foRsA4Bytes*);
typedef uint32_t (*A2foRsYardRead)(void*,uintptr_t,uint32_t);
typedef uintptr_t (*A2foRsYardNative)(void*,uint32_t,uintptr_t,uint32_t);
typedef void (*A2foRsYardLog)(void*,uint32_t);
typedef uint8_t (*A2foRsYardField)(void*,uint32_t,A2foRsA4Bytes*);
typedef uint8_t (*A2foRsYardCompare)(void*,A2foRsA4Bytes,A2foRsA4Bytes);
uint8_t a2fo_rs_runtime_yard_config(uint32_t,A2foRsYardConfig,A2foRsYardLog,void*);
void a2fo_rs_runtime_yard_class(uint8_t,uintptr_t,uint32_t,A2foRsYardField,
    A2foRsYardCompare,A2foRsYardNative,void*);
uint8_t a2fo_rs_runtime_yard_one_kind(uint8_t,uintptr_t,int32_t,int32_t,
    A2foRsYardRead,A2foRsYardNative,void*);
int32_t a2fo_rs_runtime_yard_gate(uint32_t,uintptr_t,int32_t,uintptr_t,
    A2foRsYardRead,A2foRsYardNative,void*);
int32_t a2fo_rs_runtime_yard_route(uintptr_t,int32_t,uintptr_t,
    A2foRsYardRead,A2foRsYardNative,void*);
int32_t a2fo_rs_runtime_yard_queue(uintptr_t,int32_t,uint8_t,
    A2foRsYardRead,A2foRsYardNative,void*);
int32_t a2fo_rs_runtime_yard_selection(uint8_t,uintptr_t,uintptr_t,
    A2foRsYardRead,A2foRsYardNative,void*);
uint8_t a2fo_rs_runtime_yard_init(uint8_t,A2foRsYardNative,A2foRsYardConfig,
    A2foRsYardLog,void*);
void a2fo_rs_runtime_yard_shutdown(A2foRsYardNative,void*);

typedef uint8_t (*A2foRsMuzzleField)(void*,uint32_t,A2foRsA4Bytes*,A2foRsA4Bytes*);
typedef uint8_t (*A2foRsMuzzleNumber)(void*,A2foRsA4Bytes,float*,uint32_t*);
typedef uint8_t (*A2foRsMuzzleColour)(void*,A2foRsA4Bytes,float*,uint8_t*);
typedef uintptr_t (*A2foRsMuzzleStep)(void*,uint32_t,uintptr_t,uintptr_t);
typedef void (*A2foRsMuzzleStore)(void*,uint32_t,A2foRsA4Bytes,float,float,const float*);
void a2fo_rs_runtime_muzzle_class(uint8_t,uint32_t,A2foRsMuzzleField,A2foRsMuzzleNumber,
    A2foRsMuzzleColour,A2foRsMuzzleStore,void*);
void a2fo_rs_runtime_muzzle_emit(uint8_t,uint8_t,uintptr_t,uintptr_t,
    A2foRsMuzzleStep,void*);
uint8_t a2fo_rs_runtime_muzzle_init(uint8_t,A2foRsMuzzleStep,void*);
void a2fo_rs_runtime_muzzle_shutdown(A2foRsMuzzleStep,void*);
#ifdef __cplusplus
}
#endif
