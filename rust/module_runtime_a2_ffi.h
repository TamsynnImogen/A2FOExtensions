#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Private synchronous LogMailer lifecycle bridge. Native owns Win32 handles. */
typedef uint8_t (*A2foRsMailerStep)(void*,uint32_t);
typedef void (*A2foRsMailerLog)(void*,uint32_t);
uint8_t a2fo_rs_runtime_mailer_init(uint8_t valid_api,uint8_t already_started,
    A2foRsMailerStep step,A2foRsMailerLog log,void* context);
void a2fo_rs_runtime_mailer_shutdown(A2foRsMailerStep step,void* context);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsHybridBytes {const uint8_t* data;size_t size;} A2foRsHybridBytes;
typedef void (*A2foRsHybridChildEmit)(void*,A2foRsHybridBytes,uint8_t,uint8_t);
typedef void (*A2foRsHybridTextEmit)(void*,A2foRsHybridBytes);
typedef uint32_t (*A2foRsHybridRootCount)(void*);
typedef uint8_t (*A2foRsHybridRoot)(void*,uint32_t,A2foRsHybridBytes*);
typedef void (*A2foRsHybridList)(void*,A2foRsHybridBytes,A2foRsHybridChildEmit,void*);
typedef void (*A2foRsHybridStore)(void*,A2foRsHybridBytes,A2foRsHybridBytes);
typedef uint8_t (*A2foRsHybridFind)(void*,A2foRsHybridBytes,A2foRsHybridBytes*);
typedef uint8_t (*A2foRsHybridRead)(void*,A2foRsHybridBytes,A2foRsHybridTextEmit,void*);
typedef uint8_t (*A2foRsHybridParse)(void*,A2foRsHybridBytes);
typedef uint8_t (*A2foRsHybridRegister)(void*,uintptr_t,A2foRsHybridBytes);
typedef uint32_t (*A2foRsHybridStep)(void*,uint32_t);
typedef void (*A2foRsHybridLog)(void*,uint32_t,A2foRsHybridBytes,uint32_t);
void a2fo_rs_runtime_hybrid_index(A2foRsHybridRootCount count,
    A2foRsHybridRoot root,A2foRsHybridList list,A2foRsHybridStore store,void* context);
void a2fo_rs_runtime_hybrid_handler(uint8_t valid_event,uintptr_t object_class,
    A2foRsHybridBytes source,A2foRsHybridFind find,A2foRsHybridRead read,
    A2foRsHybridParse parse,A2foRsHybridRegister register_submenus,
    A2foRsHybridLog log,void* context);
uint8_t a2fo_rs_runtime_hybrid_init(uint8_t valid_api,A2foRsHybridStep step,
    A2foRsHybridRootCount count,A2foRsHybridRoot root,A2foRsHybridList list,
    A2foRsHybridStore store,A2foRsHybridLog log,void* context);
#ifdef __cplusplus
}
#endif
