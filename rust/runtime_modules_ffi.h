#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsRuntimeModuleView {
    uintptr_t handle;
    uintptr_t shutdown;
    const uint8_t* path;
    size_t path_size;
} A2foRsRuntimeModuleView;
void* a2fo_rs_runtime_modules_new(void);
void* a2fo_rs_runtime_modules_clone(const void* modules);
void a2fo_rs_runtime_modules_drop(void* modules);
size_t a2fo_rs_runtime_modules_len(const void* modules);
uint8_t a2fo_rs_runtime_modules_push(void* modules, uintptr_t handle,
                                      uintptr_t shutdown, const uint8_t* path, size_t path_size);
void a2fo_rs_runtime_modules_clear(void* modules);
uint8_t a2fo_rs_runtime_modules_get(const void* modules, size_t index,
                                     A2foRsRuntimeModuleView* out);
#ifdef __cplusplus
}
#endif
