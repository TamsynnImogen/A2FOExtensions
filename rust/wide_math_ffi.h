#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 16 writable bytes, accessed only by Rust between reset and final result. */
void a2fo_rs_xp_wide_reset(uint8_t*);
uint8_t a2fo_rs_xp_wide_add(uint8_t*,double,double);
uint8_t a2fo_rs_xp_wide_result(const uint8_t*,double*);
#ifdef __cplusplus
}
#endif
