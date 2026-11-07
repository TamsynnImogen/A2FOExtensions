/* Private synchronous runtime bridge. Borrowed byte spans never outlive a call. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsRuntimeBytes { const uint8_t* data; size_t size; } A2foRsRuntimeBytes;
typedef struct A2foRsRuntimeField {
    A2foRsRuntimeBytes name;
    A2foRsRuntimeBytes value;
    uint8_t value_valid;
} A2foRsRuntimeField;
typedef uint8_t (*A2foRsRuntimeFieldGet)(void*, uint32_t, A2foRsRuntimeField*);
typedef uint8_t (*A2foRsRuntimeRegister)(void*);
typedef void (*A2foRsRuntimeLog)(void*, uint32_t);
/* Status: 0 = no replacement; 1 = replacement; 2 = invalid chance. */
uint32_t a2fo_rs_runtime_wreckage_handler(uint8_t enabled, uint8_t valid_event,
    uint8_t valid_replacement, uint8_t valid_fields, uint32_t field_count,
    uint32_t seed, size_t output_capacity, A2foRsRuntimeFieldGet get_field,
    void* context, A2foRsRuntimeBytes* replacement);
uint8_t a2fo_rs_runtime_wreckage_init(uint8_t valid_api, uint8_t dispatch_available,
    A2foRsRuntimeRegister register_handler, A2foRsRuntimeLog log, void* context);
/* Info operations: root(0), read base(1), read selected(2), expand shared(3),
 * expand selected(4), default config root(5), register(6). Root index UINT32_MAX
 * denotes root_directory; all other indices denote extension_root(index).
 * Native callbacks return zero only for failed root selection or registration. */
typedef uint32_t (*A2foRsRuntimeInfoStep)(void*, uint32_t, uint32_t);
typedef struct A2foRsRuntimeInfoState {
    uint8_t has_speed;
    int32_t speed;
    A2foRsRuntimeBytes configured;
    A2foRsRuntimeBytes shared;
    A2foRsRuntimeBytes default_root;
} A2foRsRuntimeInfoState;
typedef uint8_t (*A2foRsRuntimeResolve)(void*, A2foRsRuntimeBytes,
    A2foRsRuntimeBytes*);
uint8_t a2fo_rs_runtime_info_init(uint8_t valid_api, uint32_t root_count,
    A2foRsRuntimeInfoStep step, A2foRsRuntimeLog log, void* context);
uint8_t a2fo_rs_runtime_info_provide(uint8_t enabled, uint8_t ready,
    uint8_t has_speed_output, uint8_t speed_output, A2foRsRuntimeInfoState state,
    A2foRsRuntimeBytes normal, uint8_t normal_present, uint8_t output_present,
    uint32_t output_capacity, A2foRsRuntimeResolve resolve, void* context,
    uint32_t* has_speed, int32_t* speed, uint8_t* output);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Normal weapon native reads/calls are sequenced by Rust. Values are native
 * pointer-width scalars; operation codes are private to this bridge. */
typedef uintptr_t (*A2foRsRuntimeNormalRead)(void*, uint32_t, uintptr_t, uintptr_t);
typedef void (*A2foRsRuntimeNormalLog)(void*, uint32_t, uint32_t, int32_t);
uint8_t a2fo_rs_runtime_normal_allows(uint8_t enabled, uint8_t ready,
    uintptr_t weapon, A2foRsRuntimeNormalRead read,
    A2foRsRuntimeNormalLog log, void* context);
uint8_t a2fo_rs_runtime_normal_event(uint8_t valid, uint32_t kind,
    uintptr_t weapon, uint8_t enabled, uint8_t ready,
    A2foRsRuntimeNormalRead read, A2foRsRuntimeNormalLog log, void* context);
typedef uint8_t (*A2foRsRuntimeNormalInitStep)(void*, uint32_t);
uint8_t a2fo_rs_runtime_normal_init(uint8_t valid_api,
    A2foRsRuntimeNormalInitStep step, A2foRsRuntimeLog log, void* context);

#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Team-change hook workflow. Pending is copied through POD; the C++ adapter
 * owns both persistent pending storage and each synchronous pass snapshot. */
typedef struct A2foRsTeamPending {
    uint32_t handle;
    uintptr_t craft;
    int32_t team;
    uint64_t activation;
} A2foRsTeamPending;
typedef uint64_t (*A2foRsTeamNative)(void*, uint32_t, uintptr_t, uintptr_t,
    uintptr_t, A2foRsTeamPending*);
typedef uint8_t (*A2foRsTeamInitStep)(void*, uint32_t);
uint64_t a2fo_rs_runtime_team_activation(uint8_t ready,
    A2foRsTeamNative native, void* context);
void a2fo_rs_runtime_team_class_loaded(uint8_t ready, uint8_t valid_event,
    uintptr_t weapon_class, uint32_t field_count, A2foRsRuntimeFieldGet field,
    A2foRsTeamNative native, void* context);
void a2fo_rs_runtime_team_craft_event(uint8_t valid_event, uint32_t kind,
    uintptr_t craft, A2foRsTeamNative native, void* context);
void a2fo_rs_runtime_team_ownership(uintptr_t craft, int32_t before,
    uint64_t activation, uint8_t ready, A2foRsTeamNative native, void* context);
uint8_t a2fo_rs_runtime_team_simulate(uint8_t ready, float elapsed,
    A2foRsTeamNative native, void* context);
uint8_t a2fo_rs_runtime_team_init(uint8_t valid_api,
    A2foRsTeamInitStep step, A2foRsRuntimeLog log, void* context,
    uint8_t* ready);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
int32_t a2fo_rs_runtime_team_before(uint64_t activation, uintptr_t craft,
    A2foRsTeamNative native, void* context);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
void a2fo_rs_runtime_team_shutdown(A2foRsTeamNative native, void* context);
#ifdef __cplusplus
}
#endif
