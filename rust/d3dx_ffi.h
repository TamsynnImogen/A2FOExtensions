#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsD3dxData {uintptr_t words[16];uint32_t values[16];const uint8_t* text;size_t text_size;} A2foRsD3dxData;
typedef struct A2foRsD3dxOps {void* context;uint8_t (*readable)(void*,uintptr_t,size_t);void (*read)(void*,uintptr_t,void*,size_t);uint8_t (*equals)(void*,uintptr_t,const uint8_t*,size_t,uint8_t);uintptr_t (*action)(void*,uint32_t,uint32_t,A2foRsD3dxData*,const uint8_t*,size_t);} A2foRsD3dxOps;
typedef struct A2foRsD3dxLayout {uint32_t dos_size,nt_size,entry_size,lfanew,signature,machine,magic,image_size,import_rva,import_size,entry_name,entry_original,entry_first;} A2foRsD3dxLayout;
uint8_t a2fo_rs_d3dx_install(const A2foRsD3dxOps*,uintptr_t,const A2foRsD3dxLayout*);
int32_t a2fo_rs_d3dx_info(const A2foRsD3dxOps*,uint32_t);
int32_t a2fo_rs_d3dx_create(const A2foRsD3dxOps*,uint32_t,A2foRsD3dxData*);
#ifdef __cplusplus
}
#endif
