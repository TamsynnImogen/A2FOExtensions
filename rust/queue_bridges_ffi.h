#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
// kind 0: Refit v2 (5 callbacks); kind 1: Hybrid v1 (12 callbacks).
// Callback addresses remain native ABI values. Rust owns immutable publication
// and dispatch; the synchronous native invoker expresses each exact signature.
typedef uintptr_t (*A2FO_RsBridgeInvoke)(uintptr_t function, uint32_t operation,
    void* source, void* target, void* result, uint32_t queue, uint32_t value);
uint8_t a2fo_rs_queue_bridge_valid_header(uint32_t kind, uint32_t version, uint32_t struct_size);
uint8_t a2fo_rs_queue_bridge_register(uint32_t kind, uint32_t version,
    uint32_t struct_size, const uintptr_t* functions);
uintptr_t a2fo_rs_queue_bridge_dispatch(uint32_t kind, uint32_t operation,
    A2FO_RsBridgeInvoke invoke, void* source, void* target, void* result,
    uint32_t queue, uint32_t value);
#ifdef __cplusplus
}
#endif
