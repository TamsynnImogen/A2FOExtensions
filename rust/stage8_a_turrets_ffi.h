#ifndef A2FO_STAGE8_A_TURRETS_FFI_H
#define A2FO_STAGE8_A_TURRETS_FFI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t (*A2FO_Stage8TurretStep)(void*, uint32_t, uintptr_t);
typedef struct A2FO_Stage8TurretAim {
    float yaw_min;
    float yaw_max;
    float pitch_min;
    float pitch_max;
    float yaw_rate;
    float pitch_rate;
    float rest_yaw;
    float rest_pitch;
} A2FO_Stage8TurretAim;
typedef struct A2FO_Stage8TurretTargetHandles {
    uint32_t target;
    uint32_t ordered;
} A2FO_Stage8TurretTargetHandles;
uint8_t a2fo_rs_stage8_turrets_init(A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_normalize(A2FO_Stage8TurretAim*);
void a2fo_rs_stage8_turrets_trim(
    const uint8_t*, uintptr_t, A2FO_Stage8TurretStep, void*,
    uintptr_t*, uintptr_t*);
uint8_t a2fo_rs_stage8_turrets_odf_suffix(
    const uint8_t*, uintptr_t, A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_register_class(A2FO_Stage8TurretStep, void*);
uint8_t a2fo_rs_stage8_turrets_weapon_trigger(A2FO_Stage8TurretStep, void*,
    A2FO_Stage8TurretTargetHandles*, uint32_t, uint32_t);
void a2fo_rs_stage8_turrets_set_target(A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_track_target(A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_inherited_target(A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_weapon_owner(A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_ensure_parent(A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_find_mount(A2FO_Stage8TurretStep, void*);
uint8_t a2fo_rs_stage8_turrets_resolve_hardpoint(A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_craft_event(
    A2FO_Stage8TurretStep, void*, uint32_t, uint32_t, uint32_t, uint32_t);
void a2fo_rs_stage8_turrets_cleanup_parent(A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_unlink_child(A2FO_Stage8TurretStep, void*);
uint8_t a2fo_rs_stage8_turrets_parse_label(
    A2FO_Stage8TurretStep, void*, uint32_t);
void a2fo_rs_stage8_turrets_class_construct(A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_spawn_pending(
    A2FO_Stage8TurretStep, void*, uint32_t, uint32_t);
void a2fo_rs_stage8_turrets_attach_loaded(
    A2FO_Stage8TurretStep, void*, uint32_t);
void a2fo_rs_stage8_turrets_sync_ownership(A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_sync_combat_policy(A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_current_attack_order(
    A2FO_Stage8TurretStep, void*);
uint8_t a2fo_rs_stage8_turrets_child_has_attack_order(
    A2FO_Stage8TurretStep, void*, uint32_t, uint32_t);
void a2fo_rs_stage8_turrets_mirror_attack_order(
    A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_update_inherited_order(
    A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_prepare_linked(
    A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_update_linked(
    A2FO_Stage8TurretStep, void*);
uint8_t a2fo_rs_stage8_turrets_mount_world_transform(
    A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_add_to_mission(
    A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_expire_craft(
    A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_construct_turret(
    A2FO_Stage8TurretStep, void*);
uint8_t a2fo_rs_stage8_turrets_read_parameter_bool(
    A2FO_Stage8TurretStep, void*);
uint8_t a2fo_rs_stage8_turrets_is_semantic_turret(
    A2FO_Stage8TurretStep, void*);
uint8_t a2fo_rs_stage8_turrets_parse_float(
    A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_resolve_shield_observers(
    A2FO_Stage8TurretStep, void*);
uint8_t a2fo_rs_stage8_turrets_read_parameter_string(
    A2FO_Stage8TurretStep, void*);
uint8_t a2fo_rs_stage8_turrets_read_parameter_float(
    A2FO_Stage8TurretStep, void*);
void a2fo_rs_stage8_turrets_normalize_odf(A2FO_Stage8TurretStep, void*);

#ifdef __cplusplus
}
#endif

#endif
