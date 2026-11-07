#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsHandlerFieldView { const uint8_t* bytes; size_t size; } A2foRsHandlerFieldView;
typedef struct A2foRsHandlerView {
    void* lease;
    const uint8_t* owner;
    size_t owner_size;
    size_t field_count;
    uintptr_t handler;
    uintptr_t user_data;
    uint8_t enabled;
} A2foRsHandlerView;
void* a2fo_rs_runtime_handlers_new(void);
void* a2fo_rs_runtime_handlers_clone(const void* store);
void a2fo_rs_runtime_handlers_drop(void* store);
size_t a2fo_rs_runtime_handlers_len(const void* store);
uint8_t a2fo_rs_runtime_handlers_push(void* store, const uint8_t* owner,
    size_t owner_size, const A2foRsHandlerFieldView* fields, size_t field_count,
    uintptr_t handler, uintptr_t user_data, uint8_t enabled);
uint8_t a2fo_rs_runtime_handlers_get(const void* store, size_t index, A2foRsHandlerView* out);
uint8_t a2fo_rs_runtime_handlers_get_field(const void* lease,
    size_t field_index, A2foRsHandlerFieldView* out);
uint8_t a2fo_rs_runtime_handlers_retain(void* lease);
void a2fo_rs_runtime_handlers_release(void* lease);
uint8_t a2fo_rs_runtime_handlers_set_enabled(void* store, size_t index, uint8_t enabled);
uint8_t a2fo_rs_runtime_handlers_set_handler(void* store, size_t index, uintptr_t handler);
void a2fo_rs_runtime_handlers_clear(void* store);
#ifdef __cplusplus
}
#endif
