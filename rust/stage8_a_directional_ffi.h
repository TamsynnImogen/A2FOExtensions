#pragma once

#include <cstdint>
#include <cstddef>

struct A2foStage8DirectionalBytes {
    const std::uint8_t* data;
    std::size_t size;
};

struct A2foStage8DirectionalClassResult {
    std::uint32_t mode;
    float maximum[4];
    float native_maximum;
    float explicit_total;
    float total;
    float current_value;
    float share;
    std::uint32_t automatic_count;
    std::uint32_t offending_index;
    std::uint8_t native_maximum_declared;
};

extern "C" std::uint8_t a2fo_rs_stage8_directional_init(
    std::uintptr_t (*step)(void*, std::uint32_t), void* context);

extern "C" void a2fo_rs_stage8_directional_craft_event(
    std::uintptr_t event, std::uint32_t required_size,
    std::uint32_t cleanup_kind, std::uint32_t postload_kind,
    std::uint32_t simulate_pre_kind, std::uint32_t simulate_post_kind,
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);

extern "C" std::uint32_t a2fo_rs_stage8_directional_class(
    std::uintptr_t event, std::uint32_t required_size,
    std::uintptr_t (*admit)(void*, std::uint32_t),
    std::uint8_t (*field)(void*, std::uint32_t,
                          A2foStage8DirectionalBytes*, A2foStage8DirectionalBytes*),
    std::uint8_t (*equal)(void*, A2foStage8DirectionalBytes,
                          A2foStage8DirectionalBytes),
    std::uint8_t (*lower)(void*, std::uint8_t),
    std::uint8_t (*parse)(void*, std::uint32_t, float*),
    std::uint8_t (*numeric)(void*, std::uint32_t,
                            A2foStage8DirectionalClassResult*),
    std::uint8_t (*action)(void*, std::uint32_t,
                           const A2foStage8DirectionalClassResult*),
    void* context, A2foStage8DirectionalClassResult* output);

extern "C" std::int32_t a2fo_rs_stage8_directional_create_hit(
    std::int32_t shield_type,
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);

extern "C" void a2fo_rs_stage8_directional_update_effect(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);

extern "C" void a2fo_rs_stage8_directional_stop_effect(
    std::uintptr_t (*step)(void*, std::uint32_t), void* context);

extern "C" std::uint8_t a2fo_rs_stage8_directional_begin_damage(
    std::uintptr_t craft, std::uintptr_t scope, std::uint32_t required_size,
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);
extern "C" void a2fo_rs_stage8_directional_end_damage(
    std::uintptr_t scope, std::uint32_t required_size,
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);

extern "C" std::uint8_t a2fo_rs_stage8_directional_set_state(
    std::uintptr_t craft, std::uintptr_t values, std::uint32_t required_size,
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);

extern "C" std::uint8_t a2fo_rs_stage8_directional_connect(
    std::uintptr_t (*step)(void*, std::uint32_t), void* context);
extern "C" std::uint32_t a2fo_rs_stage8_directional_query(
    std::uintptr_t craft, std::uint32_t facing, std::uint32_t kind,
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);
extern "C" std::uint8_t a2fo_rs_stage8_directional_get_state(
    std::uintptr_t craft, std::uintptr_t values, std::uint32_t required_size,
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);

extern "C" std::uintptr_t a2fo_rs_stage8_directional_state_for_craft(
    std::uintptr_t craft, std::uint8_t create,
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);

extern "C" std::uint8_t a2fo_rs_stage8_directional_restore(
    std::uintptr_t craft,
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);

extern "C" void a2fo_rs_stage8_directional_forget_effects(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);
extern "C" std::uintptr_t a2fo_rs_stage8_directional_clear_tracked(
    float remaining,
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);

extern "C" void a2fo_rs_stage8_directional_push_scope(
    std::uintptr_t scope, std::uintptr_t craft,
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);
extern "C" void a2fo_rs_stage8_directional_remove_scope(
    std::uintptr_t scope,
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);
extern "C" std::uint8_t a2fo_rs_stage8_directional_active_facing(
    std::uintptr_t craft, std::uintptr_t facing,
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);
