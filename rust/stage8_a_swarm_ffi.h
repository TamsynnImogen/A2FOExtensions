#ifndef A2FO_STAGE8_A_SWARM_FFI_H
#define A2FO_STAGE8_A_SWARM_FFI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t (*A2FO_Stage8SwarmStep)(void*, uint32_t, uintptr_t);

typedef struct A2FO_Stage8SwarmScalars {
    uint32_t count;
    float scale;
    float minimum_radius;
    float maximum_radius;
    float minimum_speed;
    float maximum_speed;
    float turn_rate;
    float interaction_chance;
    float interaction_time;
    float interaction_radius;
    float host_clearance;
    float member_separation;
    uint32_t hardpoint_capacity;
    uint32_t interaction_capacity;
} A2FO_Stage8SwarmScalars;

typedef struct A2FO_Stage8SwarmEnumeration {
    uintptr_t head;
    uintptr_t node;
    uintptr_t next;
    uint32_t count;
} A2FO_Stage8SwarmEnumeration;

typedef struct A2FO_Stage8SwarmObserved {
    uint32_t handle;
    uintptr_t object_class;
} A2FO_Stage8SwarmObserved;

typedef struct A2FO_Stage8SwarmSimulation {
    uint32_t epoch;
    uint32_t object_count;
    uint32_t last_object_count;
    uint32_t countdown;
    uint32_t enumerated_count;
    uint32_t last_seen_epoch;
    uint32_t group_count;
    uint32_t group_definition;
    uint32_t definition_count;
    uint32_t agent_count;
} A2FO_Stage8SwarmSimulation;

typedef struct A2FO_Stage8SwarmCreation {
    uint32_t definition_count;
    uint32_t member_count;
    size_t launch_count;
    uint32_t random;
    int32_t failure_report;
    uint32_t handle;
    uint32_t definition_index;
} A2FO_Stage8SwarmCreation;

uint8_t a2fo_rs_stage8_swarm_init(A2FO_Stage8SwarmStep, void*);
void a2fo_rs_stage8_swarm_shutdown(A2FO_Stage8SwarmStep, void*);
void a2fo_rs_stage8_swarm_normalize(A2FO_Stage8SwarmScalars*);
void a2fo_rs_stage8_swarm_parse(A2FO_Stage8SwarmStep, void*);
uint8_t a2fo_rs_stage8_swarm_activation(A2FO_Stage8SwarmStep, void*, uint8_t);
uint32_t a2fo_rs_stage8_swarm_seed(uint32_t, uint32_t, uint32_t);
void a2fo_rs_stage8_swarm_observe(
    A2FO_Stage8SwarmStep, void*, A2FO_Stage8SwarmObserved*);
uint8_t a2fo_rs_stage8_swarm_enumerate(
    A2FO_Stage8SwarmStep, void*, A2FO_Stage8SwarmEnumeration*);
void a2fo_rs_stage8_swarm_simulate(
    A2FO_Stage8SwarmStep, void*, A2FO_Stage8SwarmSimulation*, float);
uint8_t a2fo_rs_stage8_swarm_choose_hardpoint(
    A2FO_Stage8SwarmStep, void*, size_t, uint32_t, uint8_t, uint8_t);
void a2fo_rs_stage8_swarm_choose_target(A2FO_Stage8SwarmStep, void*);
void a2fo_rs_stage8_swarm_choose_roaming(A2FO_Stage8SwarmStep, void*);
void a2fo_rs_stage8_swarm_update(A2FO_Stage8SwarmStep, void*);
uint8_t a2fo_rs_stage8_swarm_refresh_target(A2FO_Stage8SwarmStep, void*);
void a2fo_rs_stage8_swarm_separate(A2FO_Stage8SwarmStep, void*);
void a2fo_rs_stage8_swarm_create_host(
    A2FO_Stage8SwarmStep, void*, A2FO_Stage8SwarmCreation*);
void a2fo_rs_stage8_swarm_render(A2FO_Stage8SwarmStep, void*);
void a2fo_rs_stage8_swarm_hook(A2FO_Stage8SwarmStep, void*);
uint8_t a2fo_rs_stage8_swarm_instance_init(A2FO_Stage8SwarmStep, void*);
void a2fo_rs_stage8_swarm_trim(
    const uint8_t*, size_t, A2FO_Stage8SwarmStep, void*,
    size_t*, size_t*);
int32_t a2fo_rs_stage8_swarm_count_default(uint32_t);
uint32_t a2fo_rs_stage8_swarm_count_value(int32_t);

#ifdef __cplusplus
}
#endif

#endif
