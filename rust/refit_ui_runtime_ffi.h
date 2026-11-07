#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsUiResult {uint64_t value;uint8_t failed;} A2foRsUiResult;
typedef void (*A2foRsUiStep)(void*,uint32_t,uint32_t,A2foRsUiResult*);
uint8_t a2fo_rs_skin_preflight(uint32_t,void*,A2foRsUiStep);
uint8_t a2fo_rs_skin_init(uint32_t,void*,A2foRsUiStep);
uint8_t a2fo_rs_skin_load(void*,A2foRsUiStep);
uint8_t a2fo_rs_skin_compose(void*,A2foRsUiStep);
uint8_t a2fo_rs_skin_paint(uint32_t,void*,A2foRsUiStep);
uint8_t a2fo_rs_skin_dimensions(int32_t,int32_t);
uint8_t a2fo_rs_skin_background_name(const uint8_t*,size_t);
int32_t a2fo_rs_skin_legacy_coordinate(uint32_t,int32_t);
uint8_t a2fo_rs_skin_shell_class(const uint8_t*,size_t);
#ifdef __cplusplus
}
#endif
