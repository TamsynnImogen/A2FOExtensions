#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foRsRuntimeB4Op)(void*, uint32_t, uint32_t);
uint8_t a2fo_rs_point_init(uint8_t, void*, A2foRsRuntimeB4Op);
void a2fo_rs_point_register(uint8_t, void*, A2foRsRuntimeB4Op);
uint32_t a2fo_rs_point_select(void*, A2foRsRuntimeB4Op);
void a2fo_rs_point_update(void*, A2foRsRuntimeB4Op);
uintptr_t a2fo_rs_point_simulate(void*, A2foRsRuntimeB4Op);
uintptr_t a2fo_rs_point_reset(void*, A2foRsRuntimeB4Op);
uintptr_t a2fo_rs_point_save(void*, A2foRsRuntimeB4Op);
uintptr_t a2fo_rs_point_load(void*, A2foRsRuntimeB4Op);
uint8_t a2fo_rs_point_timer(uint8_t, float);
uint8_t a2fo_rs_damage_init(uint8_t, void*, A2foRsRuntimeB4Op);
void a2fo_rs_damage_register(uint8_t, void*, A2foRsRuntimeB4Op);
uintptr_t a2fo_rs_damage_hit(void*, A2foRsRuntimeB4Op);
uint8_t a2fo_rs_damage_refresh(uint8_t, void*, A2foRsRuntimeB4Op);
#ifdef __cplusplus
}
#endif
