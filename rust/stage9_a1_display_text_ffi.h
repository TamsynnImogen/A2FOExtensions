#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Synchronous native callback; operations 0-8 mirror localized_ship_label.
// Native owns all pointer reads, lookup, IsBadStringPtrA, CRT comparisons,
// and fallback storage. No callback may unwind through this boundary.
typedef uintptr_t (*A2foA1DisplayTextStep)(void* context, uint32_t operation);

// Returns a captured resolved pointer, fallback pointer, or static empty C
// string pointer. Native performs the bounded snprintf copy immediately.
uintptr_t a2fo_rs_stage9_a1_display_text_label(
    void* context, A2foA1DisplayTextStep step);

#ifdef __cplusplus
}
#endif
