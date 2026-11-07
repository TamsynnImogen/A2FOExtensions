#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Native owns std::string storage and all CRT calls. The callback returns
// SIZE_MAX after capturing an exception; the adapter rethrows after return.
typedef uintptr_t (*A2foA1AipNameStep)(
    void* context, uint32_t operation, uintptr_t index, uintptr_t value);

// `suffix_size` is sizeof(kInstantActionBuildListSuffix) - 1 from native C++.
uint8_t a2fo_rs_stage9_a1_aip_name(
    void* context, size_t suffix_size, A2foA1AipNameStep step);

#ifdef __cplusplus
}
#endif
