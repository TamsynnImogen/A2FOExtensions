#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foRsRuntimeB5Op)(void*,uint32_t,uint32_t);
uint8_t a2fo_rs_edit_init(uint8_t,void*,A2foRsRuntimeB5Op);
void a2fo_rs_edit_update(void*,A2foRsRuntimeB5Op);
void a2fo_rs_edit_transition(uint8_t,int32_t,int32_t,void*,A2foRsRuntimeB5Op);
void a2fo_rs_edit_enter(void*,A2foRsRuntimeB5Op);
void a2fo_rs_edit_leave(void*,A2foRsRuntimeB5Op);
void a2fo_rs_edit_shutdown(void*,A2foRsRuntimeB5Op);
uint8_t a2fo_rs_instant_init(uint8_t,uint8_t,void*,A2foRsRuntimeB5Op);
void a2fo_rs_instant_load(void*,A2foRsRuntimeB5Op);
uintptr_t a2fo_rs_instant_reader(void*,A2foRsRuntimeB5Op);
uint8_t a2fo_rs_instant_blob(uint32_t,uint32_t,void*,A2foRsRuntimeB5Op);
intptr_t a2fo_rs_instant_dialog(void*,A2foRsRuntimeB5Op);
#ifdef __cplusplus
}
#endif
