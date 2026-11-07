#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foA1SodStats {size_t visited,mesh_nodes,changed,invalid;} A2foA1SodStats;
typedef uint64_t (*A2foA1SodStep)(void*,uint32_t,uintptr_t,size_t);
uint8_t a2fo_rs_stage9_a1_sod_walk(uintptr_t,void*,A2foA1SodStep,A2foA1SodStats*);
#ifdef __cplusplus
}
#endif
