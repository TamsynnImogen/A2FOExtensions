#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t (*A2foA1PhysicsRead)(void* context, uint32_t operation);
typedef uint8_t (*A2foA1PhysicsCompare)(void* context,
                                        const char* candidate);

// 0: neither, 1: primary, 2: fallback. Callbacks retain native getter order.
uint8_t a2fo_rs_stage9_a1_physics_legacy_db(void* context,
                                              A2foA1PhysicsRead read);
uint8_t a2fo_rs_stage9_a1_physics_declared(void* context,
                                             A2foA1PhysicsRead read);
uint8_t a2fo_rs_stage9_a1_physics_declared_both(void* context,
                                                  A2foA1PhysicsRead read);
int32_t a2fo_rs_stage9_a1_physics_default_key(void* context,
                                                A2foA1PhysicsCompare compare);
void a2fo_rs_stage9_a1_physics_default_bits(int32_t index, uint32_t profile,
                                             const void* legacy_db,
                                             const void* fallback_db,
                                             uint32_t* bits,
                                             uint8_t* combat_controller);
uint8_t a2fo_rs_stage9_a1_physics_turn_key(const char* key, void* context,
                                             A2foA1PhysicsCompare compare);
uint8_t a2fo_rs_stage9_a1_physics_forward_key(const char* key, void* context,
                                                A2foA1PhysicsCompare compare);
uint8_t a2fo_rs_stage9_a1_physics_model_admit(uintptr_t found,
                                                const char* key, void* context,
                                                A2foA1PhysicsCompare compare);
const char* a2fo_rs_stage9_a1_physics_model_name(uintptr_t borg_found,
                                                  int32_t borg_physics);
uint8_t a2fo_rs_stage9_a1_physics_log_case(int32_t incident);

#ifdef __cplusplus
}
#endif
