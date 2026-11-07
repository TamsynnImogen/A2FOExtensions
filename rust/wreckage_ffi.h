/* Private byte ABI. On replace, offset/length describe the caller's original
 * wreckage span; no string allocation or ownership crosses this boundary. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t a2fo_rs_wreckage_decide(const uint8_t* wreckage, size_t length,
    uint8_t chance_present, const uint8_t* chance, size_t chance_length,
    uint32_t seed, size_t* replacement_offset, size_t* replacement_length);
#ifdef __cplusplus
}
#endif
