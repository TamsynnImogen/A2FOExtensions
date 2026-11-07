#pragma once
#define A2FO_DIRECT_HOOK_BINDINGS 1
#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef int32_t (*A2foRsProtectMemory)(void*,size_t,uint32_t,uint32_t*);
typedef void* (*A2foRsAllocateMemory)(size_t);
typedef void (*A2foRsFreeMemory)(void*);
typedef void (*A2foRsFlushMemory)(void*,size_t);
typedef struct A2foRsHookMemory {
 A2foRsProtectMemory protect;
 A2foRsAllocateMemory allocate;
 A2foRsFreeMemory free_memory;
 A2foRsFlushMemory flush;
} A2foRsHookMemory;
typedef struct A2foRsInlineHook {void* target;void* gateway;size_t length;} A2foRsInlineHook;
uint8_t a2fo_rs_hook_patch(void*,const void*,const uint8_t*,size_t,uint32_t,const A2foRsHookMemory*);
uint8_t a2fo_rs_hook_install(void*,void*,size_t,const uint8_t*,A2foRsInlineHook*,const A2foRsHookMemory*);
uint8_t a2fo_rs_windows_hook_patch(void*,const void*,const uint8_t*,size_t,uint32_t);
/* Same bool/cdecl signatures as the existing module API patch callbacks. */
bool a2fo_rs_windows_patch_jump(void*,void*,const uint8_t*,size_t);
bool a2fo_rs_windows_patch_call(void*,void*,const uint8_t*,size_t);
bool a2fo_rs_windows_patch_bytes(void*,const uint8_t*,const uint8_t*,size_t);
uint8_t a2fo_rs_windows_hook_install(void*,void*,size_t,const uint8_t*,A2foRsInlineHook*);
typedef struct A2foRsPatchReport {uint32_t written,restored,restore_error;} A2foRsPatchReport;
typedef struct A2foRsVideoInstall {void* gateway;uint32_t status;} A2foRsVideoInstall;
typedef uint32_t (*A2foRsLastError)(void);
typedef void (*A2foRsPublishGateway)(void*,void*);
uint8_t a2fo_rs_hook_write_report(void*,const void*,size_t,A2foRsPatchReport*,const A2foRsHookMemory*,A2foRsLastError);
uint8_t a2fo_rs_hook_patch_double(void*,const uint8_t*,const uint8_t*,size_t,const A2foRsHookMemory*);
uint8_t a2fo_rs_hook_video_install(void*,void*,size_t,const uint8_t*,A2foRsVideoInstall*,const A2foRsHookMemory*,void*,A2foRsPublishGateway);
uint8_t a2fo_rs_windows_write_report(void*,const void*,size_t,A2foRsPatchReport*);
uint8_t a2fo_rs_windows_jump_report(void*,const void*,A2foRsPatchReport*);
uint8_t a2fo_rs_windows_patch_double(void*,const uint8_t*,const uint8_t*,size_t);
uint8_t a2fo_rs_windows_jump_unchecked(void*,const void*,size_t);
uint8_t a2fo_rs_windows_video_install(void*,void*,size_t,const uint8_t*,A2foRsVideoInstall*,void*,A2foRsPublishGateway);
#ifdef __cplusplus
}
#endif
