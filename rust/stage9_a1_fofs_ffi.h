#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint8_t (*A2foA1FofsProbe)(void*,uint32_t,size_t);
size_t a2fo_rs_stage9_a1_fofs_prefix(void*,A2foA1FofsProbe);
#ifdef __cplusplus
}
#endif
