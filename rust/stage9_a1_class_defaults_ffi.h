#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Returns 0 for absent/no match, 1 for present/match, 2 for a captured
// native exception. The caller rethrows after Rust returns.
typedef uint8_t (*A2foA1ClassProbe)(void* context, uint32_t index);
// operation 0 reads native field presence; operation 1 appends its native
// command spelling after Rust has ORed the associated native bit constant.
typedef uint8_t (*A2foA1MissingStep)(void* context, uint32_t operation,
                                      uint32_t index);

typedef struct A2foA1ClassCapability {
    // Existing C++ command spellings and bit constants remain authoritative.
    const char* command;
    uint32_t bit;
} A2foA1ClassCapability;

typedef struct A2foA1ClassMissing {
    uint32_t mask;
    uint32_t admitted;
    uint8_t failed;
} A2foA1ClassMissing;

uint8_t a2fo_rs_stage9_a1_class_family(void* context,
                                        A2foA1ClassProbe probe);
uint8_t a2fo_rs_stage9_a1_resource_family(void* context,
                                           A2foA1ClassProbe probe);
uint32_t a2fo_rs_stage9_a1_class_groups(uint8_t scout, uint8_t station,
                                         uint8_t moon);
void a2fo_rs_stage9_a1_missing(
    const A2foA1ClassCapability* capabilities, uint32_t count, void* context,
    A2foA1MissingStep step, A2foA1ClassMissing* output);
uint8_t a2fo_rs_stage9_a1_report(int32_t incident, int32_t cap);

#ifdef __cplusplus
}
#endif
