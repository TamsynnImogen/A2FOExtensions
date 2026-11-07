#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uintptr_t (*A2foRsInspectorStep)(void*,uint32_t,uint32_t);
uint8_t a2fo_rs_inspector_draft(void*,uint32_t*,uint8_t,A2foRsInspectorStep);
void a2fo_rs_inspector_build(void*,A2foRsInspectorStep);
void a2fo_rs_inspector_okay(void*,A2foRsInspectorStep);
void a2fo_rs_inspector_destroy(void*,A2foRsInspectorStep);
uint8_t a2fo_rs_inspector_init(void*,A2foRsInspectorStep);
#ifdef __cplusplus
}
#endif
