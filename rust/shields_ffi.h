#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Private, synchronous, allocation-free boundary. Arrays hold four floats;
// transforms hold twelve floats. No C++ enum or class layout crosses this ABI.
uint32_t a2fo_rs_shields_index(uint32_t facing);
uint8_t a2fo_rs_shields_active(uint32_t mode, uint32_t facing);
void a2fo_rs_shields_normalize(uint32_t mode, float* maximum);
uint8_t a2fo_rs_shields_valid(uint32_t mode, const float* maximum);
float a2fo_rs_shields_total(const float* values);
float a2fo_rs_shields_aggregate(uint32_t mode, const float* maximum,
                               const float* current, uint32_t facing);
uint8_t a2fo_rs_shields_clear(float remaining, int32_t effect);
uint8_t a2fo_rs_shields_suppress(int32_t type, uint8_t resolved, float remaining);
uint8_t a2fo_rs_shields_select(uint32_t mode, const float* transform,
                              const float* attacker, uint32_t* facing);
uint8_t a2fo_rs_shields_select_local(uint32_t mode, const float* transform,
                                    uint32_t* facing);
void a2fo_rs_shields_initialize(uint32_t mode, const float* maximum,
                               float* current, float native_total);
void a2fo_rs_shields_reconcile(uint32_t mode, const float* maximum,
                              float* current, float native_total);
#ifdef __cplusplus
}
#endif
