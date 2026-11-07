/* Private migration ABI; not part of the public module SDK.
 * State storage must be zero-initialized, writable and 8-byte aligned, remain
 * alive for the call, and be accessed ONLY by these atomic functions after
 * publication. No C++ atomics are reinterpreted as Rust atomics.
 * Byte ranges may be empty/null; nonempty ranges must be valid for the call.
 * Returned names are immutable process-lifetime NUL-terminated ASCII strings.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
uint32_t a2fo_rs_builtin_count(void);
uint8_t a2fo_rs_builtin_requires_restart(int32_t feature);
const char* a2fo_rs_builtin_name(int32_t feature);
int32_t a2fo_rs_builtin_find(const uint8_t* name, size_t length);
uint8_t a2fo_rs_builtin_same_root(const uint8_t* left, size_t left_length,
                                const uint8_t* right, size_t right_length);
void a2fo_rs_builtin_prepare(uint64_t* state, uint8_t available, uint8_t requested);
uint8_t a2fo_rs_builtin_request(uint64_t* state, uint8_t enabled);
uint64_t a2fo_rs_builtin_flags(const uint64_t* state);
uint64_t a2fo_rs_builtin_activation(const uint64_t* state);
#ifdef __cplusplus
}
#endif
