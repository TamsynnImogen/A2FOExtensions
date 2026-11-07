#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsRefitBytes {const uint8_t* data;size_t size;} A2foRsRefitBytes;
typedef struct A2foRsRefitConfig {
    A2foRsRefitBytes enabled,value;
    uint32_t enabled_length,value_length;
} A2foRsRefitConfig;
typedef uint8_t (*A2foRsRefitRead)(void*,A2foRsRefitConfig*);
typedef uint8_t (*A2foRsRefitNumber)(void*,A2foRsRefitBytes,float*);
typedef uint8_t (*A2foRsRefitStep)(void*,uint32_t,uint32_t);
typedef void (*A2foRsRefitLog)(void*,uint32_t);
uint8_t a2fo_rs_runtime_refit_init(uint8_t valid_api,uint8_t already_initialized,
    uint8_t camera,uint8_t builtin,A2foRsRefitRead read,A2foRsRefitNumber number,
    A2foRsRefitStep step,A2foRsRefitLog log,void* context);
uint32_t a2fo_rs_runtime_refit_value_bits(uint8_t camera,uint8_t installed,
    uint8_t enabled,uint32_t configured_bits);
uint8_t a2fo_rs_runtime_refit_armed(uint8_t camera,uint8_t installed,uint32_t value_bits);
typedef void (*A2foRsRefitShutdownStep)(void*,uint32_t);
void a2fo_rs_runtime_refit_shutdown(A2foRsRefitShutdownStep step,void* context);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsShellScope {
    uintptr_t device,selected;
    int32_t width,height,depth;
    uint8_t borderless;
} A2foRsShellScope;
typedef struct A2foRsShellCandidate {
    uint8_t profile_present,editor,config_readable,mode_readable;
    A2foRsShellScope scope;
} A2foRsShellCandidate;
typedef struct A2foRsShellRect {int32_t left,top,right,bottom;} A2foRsShellRect;
typedef intptr_t (*A2foRsShellActivateStep)(void*,uint32_t,
    const A2foRsShellScope*,intptr_t);
typedef uintptr_t (*A2foRsShellResizeStep)(void*,uint32_t,intptr_t,intptr_t,
    A2foRsShellRect*);
typedef uint8_t (*A2foRsShellInitStep)(void*,uint32_t);
typedef void (*A2foRsShellLog)(void*,uint32_t);
A2foRsShellScope a2fo_rs_runtime_shell_select(uint8_t ready,int32_t display,
    int32_t mode,A2foRsShellCandidate candidate);
uint8_t a2fo_rs_runtime_shell_lookup_route(uint8_t ready,uintptr_t selected,
    uintptr_t scope_device,uintptr_t device);
int32_t a2fo_rs_runtime_shell_activate(uint8_t ready,int32_t display,
    int32_t mode,uint8_t force,A2foRsShellCandidate candidate,
    A2foRsShellScope applied,A2foRsShellActivateStep step,void* context);
void a2fo_rs_runtime_shell_resize(uint8_t ready,A2foRsShellScope scope,
    uint8_t window_present,int32_t width,int32_t height,uint8_t fullscreen,
    uint32_t adapter,A2foRsShellResizeStep step,void* context);
uint8_t a2fo_rs_runtime_shell_init(uint8_t valid_api,
    A2foRsShellInitStep step,A2foRsShellLog log,void* context);
void a2fo_rs_runtime_shell_shutdown(A2foRsShellInitStep step,void* context);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
uint8_t a2fo_rs_runtime_shell_scope_eligible(uint8_t ready,int32_t display,
    int32_t mode,uint8_t profile_present,uint8_t editor);
#ifdef __cplusplus
}
#endif
