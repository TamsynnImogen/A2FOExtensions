#pragma once

#include <cstddef>
#include <cstdint>

extern "C" {
struct A2foStage8InfestationBytes {
    const std::uint8_t* data;
    std::size_t size;
};
struct A2foStage8InfestationPolicy {
    std::uint8_t enabled;
    float delay_seconds;
    float damage;
    A2foStage8InfestationBytes larva_odf;
    std::uint32_t larva_count;
    std::uint32_t explosion_project_id;
    std::uint8_t bypass_shields;
    float spawn_radius;
};
using A2foStage8InfestationLower = std::uint8_t (*)(void*, std::uint8_t);
using A2foStage8InfestationField = std::uint8_t (*)(
    void*, std::uint32_t, A2foStage8InfestationBytes*,
    A2foStage8InfestationBytes*);
using A2foStage8InfestationFloat = std::uint8_t (*)(
    void*, A2foStage8InfestationBytes, float*);
using A2foStage8InfestationCount = std::uint8_t (*)(
    void*, A2foStage8InfestationBytes, std::uint32_t*);
using A2foStage8InfestationProject = std::uint8_t (*)(void*, std::uint32_t*);
using A2foStage8InfestationStore = std::uint8_t (*)(
    void*, const A2foStage8InfestationPolicy*);
using A2foStage8InfestationStoreMother = std::uint8_t (*)(void*, std::uint8_t);
using A2foStage8InfestationLog = void (*)(void*);
using A2foStage8InfestationStep = std::uintptr_t (*)(
    void*, std::uint32_t, std::uintptr_t);

void a2fo_rs_stage8_infestation_weapon_policy(
    std::uintptr_t weapon_class, std::uint32_t field_count,
    A2foStage8InfestationLower lower, A2foStage8InfestationField field,
    A2foStage8InfestationFloat parse_float,
    A2foStage8InfestationCount parse_count,
    A2foStage8InfestationProject project,
    A2foStage8InfestationStore store, A2foStage8InfestationLog log,
    void* context);

void a2fo_rs_stage8_infestation_mother_metadata(
    std::uintptr_t object_class, std::uint32_t field_count,
    A2foStage8InfestationLower lower, A2foStage8InfestationField field,
    A2foStage8InfestationStoreMother store, void* context);

void a2fo_rs_stage8_infestation_class_event(
    std::uintptr_t event, std::uint32_t required_size,
    std::uint8_t object_class,
    A2foStage8InfestationStep step, void* context);

std::uint8_t a2fo_rs_stage8_infestation_activation(
    std::uint64_t current, std::uint64_t previous,
    A2foStage8InfestationStep step, void* context);

std::uint8_t a2fo_rs_stage8_infestation_init(
    A2foStage8InfestationStep step, void* context);

void a2fo_rs_stage8_infestation_shutdown(
    A2foStage8InfestationStep step, void* context);

std::uintptr_t a2fo_rs_stage8_infestation_nearest_mother(
    A2foStage8InfestationStep step, void* context);

std::uint8_t a2fo_rs_stage8_infestation_repair_admitted(
    std::uintptr_t craft, A2foStage8InfestationStep step, void* context);

std::uint32_t a2fo_rs_stage8_infestation_attacker_handle(
    std::uint32_t infector_handle, std::int32_t infector_team,
    A2foStage8InfestationStep step, void* context);

void a2fo_rs_stage8_infestation_update_route(
    std::uintptr_t larva, float elapsed_seconds,
    A2foStage8InfestationStep step, void* context);

void a2fo_rs_stage8_infestation_spawn_larvae(
    std::uint32_t count, std::int32_t infector_team,
    std::uint8_t has_larva_odf,
    A2foStage8InfestationStep step, void* context);

void a2fo_rs_stage8_infestation_hatch(
    std::uintptr_t craft, A2foStage8InfestationStep step, void* context);

void a2fo_rs_stage8_infestation_craft_event(
    std::uintptr_t event, std::uint32_t required_size,
    std::uint32_t cleanup_kind, std::uint32_t postload_kind,
    std::uint32_t simulate_kind,
    A2foStage8InfestationStep step, void* context);

std::uint8_t a2fo_rs_stage8_infestation_target_valid(
    std::uintptr_t weapon, std::uintptr_t target,
    A2foStage8InfestationStep step, void* context);

std::uint8_t a2fo_rs_stage8_infestation_trigger(
    std::uintptr_t event, std::uint32_t required_size,
    std::uint32_t precheck_kind, std::uint32_t committed_kind,
    A2foStage8InfestationStep step, void* context);

std::uint8_t a2fo_rs_stage8_infestation_signatures(
    A2foStage8InfestationStep step, void* context);

void a2fo_rs_stage8_infestation_add_to_mission(
    std::uintptr_t object, A2foStage8InfestationStep step, void* context);

std::uintptr_t a2fo_rs_stage8_infestation_construct(
    std::uintptr_t object_class, std::int32_t team,
    A2foStage8InfestationStep step, void* context);

void a2fo_rs_stage8_infestation_move(
    std::uintptr_t craft, A2foStage8InfestationStep step, void* context);

void a2fo_rs_stage8_infestation_explosion(
    std::uint32_t project_id, A2foStage8InfestationStep step, void* context);

void a2fo_rs_stage8_infestation_damage(
    std::uintptr_t craft, float amount,
    A2foStage8InfestationStep step, void* context);

std::uint8_t a2fo_rs_stage8_infestation_project_id(
    std::uintptr_t parameter_db, std::uintptr_t command,
    std::uintptr_t output, A2foStage8InfestationStep step, void* context);
}
