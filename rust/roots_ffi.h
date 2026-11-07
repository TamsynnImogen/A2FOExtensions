#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsRootBytes {const uint8_t* data;size_t size;} A2foRsRootBytes;
typedef void (*A2foRsRootEmit)(void*,uint32_t,const uint8_t*,size_t);
typedef uint8_t (*A2foRsRootDirectory)(void*,const uint8_t*,size_t);
typedef void (*A2foRsRootRead)(void*,const uint8_t*,size_t,A2foRsRootEmit,void*);
void a2fo_rs_roots_active(A2foRsRootBytes,A2foRsRootEmit,void*);
void a2fo_rs_roots_parent(A2foRsRootBytes,A2foRsRootEmit,void*);
void a2fo_rs_roots_defaults(A2foRsRootBytes,int32_t*,A2foRsRootEmit,void*);
void a2fo_rs_roots_settings(A2foRsRootBytes,A2foRsRootBytes,A2foRsRootBytes,A2foRsRootBytes,A2foRsRootEmit,void*);
uint8_t a2fo_rs_roots_safe(A2foRsRootBytes);
void a2fo_rs_roots_discover(A2foRsRootBytes,A2foRsRootBytes,A2foRsRootDirectory,A2foRsRootRead,void*,A2foRsRootEmit,void*);
#ifdef __cplusplus
}
#endif
