#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsLoaderText {const uint8_t* data;size_t size;} A2foRsLoaderText;
typedef struct A2foRsLoaderEntry {A2foRsLoaderText name,filename,path;uintptr_t handle,shutdown;uint32_t state,installed;} A2foRsLoaderEntry;
typedef struct A2foRsLoaderApi {void* context;size_t (*get)(void*,uint32_t,size_t,A2foRsLoaderEntry*);uintptr_t (*act)(void*,uint32_t,size_t,uintptr_t,uintptr_t,const uint8_t*,size_t);} A2foRsLoaderApi;
uint8_t a2fo_rs_loader_run(const A2foRsLoaderApi*);
void a2fo_rs_loader_unload(const A2foRsLoaderApi*);
uint8_t a2fo_rs_loader_producer(const A2foRsLoaderApi*);
#ifdef __cplusplus
}
#endif
