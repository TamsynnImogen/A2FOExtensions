#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foRsRuntimeB3Op)(void*,uint32_t,uint32_t);
uint8_t a2fo_rs_hardpoints_live(uint8_t ready,uint64_t activation,uint64_t* previous,
    void* context,A2foRsRuntimeB3Op op);
uintptr_t a2fo_rs_hardpoints_query(uint8_t inside,void* context,A2foRsRuntimeB3Op op);
void a2fo_rs_hardpoints_evaluate(void* context,A2foRsRuntimeB3Op op);
uint8_t a2fo_rs_hardpoints_init(uint8_t valid,void* context,A2foRsRuntimeB3Op op);
uint8_t a2fo_rs_animations_live(uint64_t activation,uint64_t* previous,void* context,A2foRsRuntimeB3Op op);
uint8_t a2fo_rs_animations_install(void* context,A2foRsRuntimeB3Op op);
uint8_t a2fo_rs_animations_init(uint8_t valid,void* context,A2foRsRuntimeB3Op op);
void a2fo_rs_animations_emit(const uint8_t* event,size_t length,void* context,A2foRsRuntimeB3Op op);
void a2fo_rs_animations_evaluate_hook(void* context,A2foRsRuntimeB3Op op);
void a2fo_rs_animations_craft_event(uint32_t kind,uint8_t valid,uint8_t active,
    void* context,A2foRsRuntimeB3Op op);
void a2fo_rs_animations_producer_event(uint32_t kind,uint8_t valid,uint8_t active,
    void* context,A2foRsRuntimeB3Op op);
void a2fo_rs_animations_read(uint8_t ready,void* context,A2foRsRuntimeB3Op op);
uintptr_t a2fo_rs_animations_clone(uint8_t ready,void* context,A2foRsRuntimeB3Op op);
void a2fo_rs_animations_destroy(void* context,A2foRsRuntimeB3Op op);
void a2fo_rs_animations_animate(void* context,A2foRsRuntimeB3Op op);
void a2fo_rs_animations_observer(void* context,A2foRsRuntimeB3Op op);
#ifdef __cplusplus
}
#endif
