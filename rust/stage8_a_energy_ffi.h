#pragma once

#include <cstdint>

extern "C" std::uint8_t a2fo_rs_stage8_energy_init(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_energy_craft_class(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_energy_weapon_class(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_energy_craft_event(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_energy_in_range(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint8_t credit);
extern "C" void a2fo_rs_stage8_energy_trigger_resupply(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_energy_credit_provider(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_energy_age_provider(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, float elapsed);
extern "C" void a2fo_rs_stage8_energy_forget_craft(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_energy_receiver_active(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uintptr_t a2fo_rs_stage8_energy_persistence(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint8_t load);
extern "C" std::uintptr_t a2fo_rs_stage8_energy_shot(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint32_t kind, std::uint8_t has_target);
extern "C" std::uint8_t a2fo_rs_stage8_energy_shot_allowed(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint8_t has_target);
extern "C" std::uint32_t a2fo_rs_stage8_energy_amount(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint32_t kind);
extern "C" std::int32_t a2fo_rs_stage8_energy_parse(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint32_t kind);
extern "C" void a2fo_rs_stage8_energy_ui_refresh(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_energy_ui_render(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_energy_trigger_precheck(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::int32_t a2fo_rs_stage8_energy_resupply_state(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_energy_shutdown(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_energy_field(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_energy_stores(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint8_t create);
extern "C" std::uint8_t a2fo_rs_stage8_energy_parse_float(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
