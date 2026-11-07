#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsCheatBytes {const uint8_t* data;size_t size;} A2foRsCheatBytes;
typedef struct A2foRsCheatValue {
    A2foRsCheatBytes first,second;
    uintptr_t word,other;
    float number,second_number;
    float vector[12];
} A2foRsCheatValue;
typedef uintptr_t (*A2foRsCheatStep)(void*,uint32_t,A2foRsCheatValue*);
typedef struct A2foRsCheatGrants {float values[5];} A2foRsCheatGrants;
void a2fo_rs_runtime_cheats_load(A2foRsCheatStep,void*,A2foRsCheatGrants*);
uint8_t a2fo_rs_runtime_cheats_command(uint32_t,A2foRsCheatStep,void*);
uint8_t a2fo_rs_runtime_cheats_register(A2foRsCheatStep,void*);
void a2fo_rs_runtime_cheats_chat_init(A2foRsCheatStep,void*);
uint8_t a2fo_rs_runtime_cheats_init(uint8_t,A2foRsCheatStep,void*);
#ifdef __cplusplus
}
#endif
