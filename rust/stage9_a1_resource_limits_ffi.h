#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct A2foA1ResourceLimitPlan {
    int32_t values[3];
    uint8_t legacy_mask;
    uint8_t modern_mask;
    uint8_t apply_mask;
    uint8_t reserved;
} A2foA1ResourceLimitPlan;

/* 1 = completed, 0 = native callback failed; native owns exception storage. */
typedef uint8_t (*A2foA1ResourceRootStep)(void* context, uint32_t index);
uint8_t a2fo_rs_stage9_a1_resource_scan_roots(
    uint32_t count, void* context, A2foA1ResourceRootStep step);

/* Begin constructs native statement from [begin,end) and exposes its bytes.
 * Identifier constructs the native identifier only after '=' has been found.
 * Parse returns 2 for a valid literal, 1 for an invalid literal, 0 for a
 * captured native exception. End destroys identifier, then statement.
 * Rust reads returned bytes only before its next callback invocation. */
typedef uint8_t (*A2foA1ResourceBeginStatement)(
    void* context, size_t begin, size_t end,
    const uint8_t** bytes, size_t* length);
typedef uint8_t (*A2foA1ResourceIdentifier)(
    void* context, size_t equals, const uint8_t** bytes, size_t* length);
typedef uint8_t (*A2foA1ResourceParseLiteral)(
    void* context, size_t equals, int32_t* value);
typedef uint8_t (*A2foA1ResourceEndStatement)(void* context);

uint8_t a2fo_rs_stage9_a1_resource_parse(
    const uint8_t* source, size_t length, void* context,
    A2foA1ResourceBeginStatement begin,
    A2foA1ResourceIdentifier identifier,
    A2foA1ResourceParseLiteral parse,
    A2foA1ResourceEndStatement end,
    A2foA1ResourceLimitPlan* output);

/* Step keeps native layout validation, float conversion, memcpy, and logging
 * on the C++ side. A failure leaves earlier native writes as before. */
typedef uint8_t (*A2foA1ResourceApplyStep)(
    void* context, uint32_t index, int32_t value);
uint8_t a2fo_rs_stage9_a1_resource_apply(
    const A2foA1ResourceLimitPlan* plan, void* context,
    A2foA1ResourceApplyStep step, uint32_t* applied);

#ifdef __cplusplus
}
#endif
