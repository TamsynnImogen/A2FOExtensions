#pragma once

#include <cstdint>
#include <cstddef>

struct A2foFireArcClassField {
    std::uint8_t present;
    std::uint8_t valid;
    float value;
    const std::uint8_t* text;
    std::size_t len;
};

struct A2foFireArcClassConfig {
    std::int32_t mode;
    float yaw;
    float pitch;
    float yaw_angle;
    float pitch_angle;
    float cone_angle;
};

extern "C" std::uint8_t a2fo_rs_stage8_fire_arcs_init(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);
extern "C" void a2fo_rs_stage8_fire_arcs_class(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context,
    A2foFireArcClassField* field, A2foFireArcClassConfig* config);
extern "C" void a2fo_rs_stage8_fire_arcs_optional_float(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context,
    const A2foFireArcClassField* field);
extern "C" std::uint8_t a2fo_rs_stage8_fire_arcs_trigger(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context,
    std::uint8_t from_event);
extern "C" std::uint8_t a2fo_rs_stage8_fire_arcs_can_fire_at(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);
extern "C" std::size_t a2fo_rs_stage8_fire_arcs_render_weapon(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);
extern "C" void a2fo_rs_stage8_fire_arcs_render_lines(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context,
    std::uint8_t target_inside);
extern "C" void a2fo_rs_stage8_fire_arcs_render_all(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);
extern "C" void a2fo_rs_stage8_fire_arcs_render_hovered(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);
extern "C" void a2fo_rs_stage8_fire_arcs_render_icon(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context,
    std::uint8_t system_icon);
extern "C" void a2fo_rs_stage8_fire_arcs_refresh_colours(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);
extern "C" std::uint8_t a2fo_rs_stage8_fire_arcs_select_weapon(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t), void* context);
