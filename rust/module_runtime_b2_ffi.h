#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foRsRuntimeB2Op)(void*,uint32_t);
uint8_t a2fo_rs_a1_init(uint8_t valid,void* context,A2foRsRuntimeB2Op op);
uintptr_t a2fo_rs_a1_dispatch(void* context,A2foRsRuntimeB2Op op);
void a2fo_rs_a1_report(void* context,A2foRsRuntimeB2Op op);
void a2fo_rs_a1_shutdown(void* context,A2foRsRuntimeB2Op op);
uint8_t a2fo_rs_station_init(uint8_t valid,void* context,A2foRsRuntimeB2Op op);
uint8_t a2fo_rs_station_enabled(uint64_t activation,uint64_t* previous,void* context,A2foRsRuntimeB2Op op);
uint32_t a2fo_rs_station_queue(uint32_t command,uint8_t enabled,uint8_t same_generation,
    uint8_t same_class,uint8_t has_class,uint32_t turns);
uint8_t a2fo_rs_station_broadcast(uint32_t action,void* context,A2foRsRuntimeB2Op op);
void a2fo_rs_station_shutdown(void* context,A2foRsRuntimeB2Op op);
int32_t a2fo_rs_station_keyboard(void* context,A2foRsRuntimeB2Op op);
void a2fo_rs_station_set_command(uint32_t turns,void* context,A2foRsRuntimeB2Op op);
void a2fo_rs_station_preview(void* context,A2foRsRuntimeB2Op op);
#ifdef __cplusplus
}
#endif
