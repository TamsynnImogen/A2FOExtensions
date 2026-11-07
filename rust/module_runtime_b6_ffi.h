#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foRsRuntimeB6Op)(void*,uint32_t,uint32_t);
uint8_t a2fo_rs_odf_enabled(uint64_t,uint64_t*,void*,A2foRsRuntimeB6Op);
uint8_t a2fo_rs_odf_init(uint8_t,void*,A2foRsRuntimeB6Op);
void a2fo_rs_odf_race(uint8_t,void*,A2foRsRuntimeB6Op);
uint8_t a2fo_rs_odf_swap(uint8_t,void*,A2foRsRuntimeB6Op);
void a2fo_rs_odf_craft(uint8_t,uint8_t,uint32_t,void*,A2foRsRuntimeB6Op);
void a2fo_rs_odf_shutdown(void*,A2foRsRuntimeB6Op);
#ifdef __cplusplus
}
#endif
