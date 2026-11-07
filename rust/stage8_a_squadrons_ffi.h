#pragma once

#include <cstdint>

extern "C" std::uint8_t a2fo_rs_stage8_squadrons_init(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_squadrons_class(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_producer(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint32_t admit,
    std::uint32_t starting_effect, std::uint32_t finishing);
extern "C" void a2fo_rs_stage8_squadrons_craft(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint32_t post_load, std::uint32_t cleanup,
    std::uint32_t simulate_post);
extern "C" void a2fo_rs_stage8_squadrons_yard_class(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint32_t field_count);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_additional_costs(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_squadrons_pending(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint32_t kind, std::uint32_t cleanup,
    std::uint32_t simulate_post);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_spawn(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_launch_next(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uintptr_t a2fo_rs_stage8_squadrons_finish(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_install_finish(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_squadrons_capture_pod(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, const std::uint8_t* label_data, std::uintptr_t label_len);
extern "C" void a2fo_rs_stage8_squadrons_track_pod(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint32_t kind, std::uint32_t cleanup,
    std::uint32_t simulate_post, std::uint32_t post_load);
extern "C" void a2fo_rs_stage8_squadrons_attached_peers(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_observe_repair(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uintptr_t a2fo_rs_stage8_squadrons_needs_repair(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uintptr_t a2fo_rs_stage8_squadrons_activate_repair(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_squadrons_end_repair(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint8_t cancel_native);
extern "C" void a2fo_rs_stage8_squadrons_finish_repair(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_squadrons_update_repair(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint32_t kind, std::uint32_t cleanup,
    std::uint32_t simulate_post, std::uint32_t seconds_bits);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_native_job_exists(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint32_t job_id);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_install_repair(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_squadrons_select(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::int32_t mode, std::int32_t clear_selection,
    std::uint32_t selection_limit);
extern "C" std::uint32_t a2fo_rs_stage8_squadrons_logical_selection(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_squadrons_install_ui(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_squadrons_render_panel(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_repair_compatible(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_resolve_member(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_squadrons_copy_fields(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_squadrons_lower_label(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_squadrons_field_value(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_badge_bounds(
    std::int32_t* left, std::int32_t* top,
    std::int32_t right, std::int32_t bottom);
extern "C" void a2fo_rs_stage8_squadrons_live_squad(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uint32_t capacity);
extern "C" std::uintptr_t a2fo_rs_stage8_squadrons_bar(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, void* output);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_install_panel(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uintptr_t a2fo_rs_stage8_squadrons_ui_projection(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context, std::uintptr_t physical, std::uint8_t handles);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_preflight(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_squadrons_capture_economics(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_resolve_economics_member(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_resolve_economics(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_pod_layout(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" std::uint8_t a2fo_rs_stage8_squadrons_pod_root(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
extern "C" void a2fo_rs_stage8_squadrons_badge(
    std::uintptr_t (*step)(void*, std::uint32_t, std::uintptr_t),
    void* context);
