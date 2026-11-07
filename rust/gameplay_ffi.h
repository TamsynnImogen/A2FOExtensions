#ifndef A2FO_RUST_GAMEPLAY_FFI_H
#define A2FO_RUST_GAMEPLAY_FFI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsRect { float x, y, width, height; } A2foRsRect;
typedef struct A2foRsFill { A2foRsRect source, destination; } A2foRsFill;
typedef struct A2foRsOptionalNumber { uint8_t present, valid; float value; } A2foRsOptionalNumber;
typedef struct A2foRsCycleState { size_t next_delay_index; float idle_reset_remaining; uint64_t fire_count; } A2foRsCycleState;
typedef struct A2foRsShieldConfig { int32_t display_mode; int32_t positions[4]; uint8_t configured; } A2foRsShieldConfig;
typedef struct A2foRsShieldReport { uint32_t valid, invalid; uint8_t display_found, position_found[4]; } A2foRsShieldReport;
typedef struct A2foRsBytes { const uint8_t* data; size_t length; } A2foRsBytes;
typedef struct A2foRsField { A2foRsBytes name, value; } A2foRsField;
typedef struct A2foRsMember { uint32_t row, count; A2foRsBytes odf; } A2foRsMember;
typedef struct A2foRsEconomics { int32_t costs[10]; float seconds; } A2foRsEconomics;
typedef void (*A2foRsTextCallback)(void* context, const uint8_t* text, size_t length);
typedef void (*A2foRsMemberCallback)(void* context, uint32_t row, const uint8_t* odf, size_t length, uint32_t count);
typedef uint32_t (*A2foRsClassCallback)(void* context, const uint8_t* odf, size_t length);
typedef uint8_t (*A2foRsEconomicsCallback)(void* context, const uint8_t* odf, size_t length, A2foRsEconomics* output);
void a2fo_rs_gameplay_odf_key(A2foRsBytes input, void* context, A2foRsTextCallback output);
uint32_t a2fo_rs_gameplay_definition_size(const A2foRsMember* members, size_t count);
uint32_t a2fo_rs_gameplay_definition_size_add(uint32_t total, uint32_t count, uint8_t* overflow);
void a2fo_rs_gameplay_parse_squadron(A2foRsBytes odf, const A2foRsField* fields, size_t count,
    void* context, A2foRsTextCallback set_odf, A2foRsMemberCallback add_member,
    uint8_t* reinforce_at_yard, A2foRsTextCallback set_error);
void a2fo_rs_gameplay_validate_squadron(A2foRsBytes odf, const A2foRsMember* members, size_t count,
    void* context, A2foRsClassCallback resolver, A2foRsTextCallback set_error);
void a2fo_rs_gameplay_economics_squadron(A2foRsBytes odf, const A2foRsMember* members, size_t count,
    void* context, A2foRsEconomicsCallback resolver, A2foRsEconomics* output,
    A2foRsTextCallback set_error);
void a2fo_rs_gameplay_parse_shield_config(const uint8_t* source, size_t length,
    A2foRsShieldConfig* config, A2foRsShieldReport* report);
uint8_t a2fo_rs_gameplay_valid_shield_mapping(const A2foRsShieldConfig* config);
uint8_t a2fo_rs_gameplay_remap_shield(const A2foRsShieldConfig* config,
    const A2foRsRect* input, size_t* indices);
void a2fo_rs_gameplay_scale_shield(A2foRsRect* positions, const uint8_t* authored,
    float graphic_width, float graphic_height);
uint8_t a2fo_rs_gameplay_shield_value(uint32_t mode, float current, float maximum,
    double* first, double* second);
uint32_t a2fo_rs_gameplay_build_cycle(const A2foRsOptionalNumber* numbered, size_t numbered_count,
    float legacy_delay, A2foRsOptionalNumber save_point, A2foRsOptionalNumber reset_time,
    float* delays, size_t* delay_count, size_t* save_index, float* reset, uint8_t* numbered_used);
void a2fo_rs_gameplay_normalize_cycle(const float* delays, size_t count, size_t save_index,
    float reset, A2foRsCycleState* state);
float a2fo_rs_gameplay_consume_cycle(const float* delays, size_t count, size_t save_index,
    float reset, A2foRsCycleState* state);
void a2fo_rs_gameplay_update_cycle(const float* delays, size_t count, float reset,
    A2foRsCycleState* state, uint8_t timer_advanced, float timer_after, float elapsed);
uint8_t a2fo_rs_gameplay_identity_index(int32_t index, size_t count, size_t* output);
uint8_t a2fo_rs_gameplay_parse_nonnegative(const uint8_t* text, size_t length, int32_t* output);
uint8_t a2fo_rs_gameplay_can_afford(const int64_t* amounts, const int32_t* costs);
void a2fo_rs_gameplay_debit(int64_t* amounts, const int32_t* costs);
void a2fo_rs_gameplay_credit(int64_t* amounts, const int32_t* costs);
int64_t a2fo_rs_gameplay_saturating_add(int64_t value, int64_t delta);
uint32_t a2fo_rs_gameplay_node_flags(uint32_t current, uint8_t selected);
uint8_t a2fo_rs_gameplay_subsystem_condition(uint8_t operational, uint8_t forced_disabled, int32_t maximum, double current, float disable_time);
size_t a2fo_rs_gameplay_mesh_choice(uint32_t handle, size_t index, uint32_t sequence, size_t count);
float a2fo_rs_gameplay_rebuild_scale(int32_t maximum, double current);
float a2fo_rs_gameplay_repair_sample(uint32_t handle, size_t index, uint32_t sequence, size_t component);
uint8_t a2fo_rs_gameplay_damage_policy_active(size_t mesh_count, float threshold, size_t scorch_count, size_t hardpoint_count);
uint8_t a2fo_rs_gameplay_icon_state(uint8_t operational, uint8_t forced_disabled, int32_t maximum, double current, float disable_time, uint8_t* output);
void a2fo_rs_gameplay_tint_colour(const float* native_colour, const float* configured_colour, float* output);
uint8_t a2fo_rs_gameplay_weapon_presentation(uint8_t kind, uint8_t technology, uint8_t hide);
uint8_t a2fo_rs_gameplay_weapon_colour_source(uint8_t weapon, uint8_t system);
uint8_t a2fo_rs_gameplay_passive_weapon_class(const uint8_t* text, size_t length);
uint8_t a2fo_rs_gameplay_node_name(const uint8_t* input, size_t length, uint8_t* output, size_t* output_length);
uint8_t a2fo_rs_gameplay_faction_suffix(const uint8_t* input, size_t length, uint8_t* output, size_t* output_length);
size_t a2fo_rs_gameplay_texture_name(const uint8_t* input, size_t input_length,
    const uint8_t* suffix, size_t suffix_length, uint8_t* output);
size_t a2fo_rs_gameplay_texture_path(const uint8_t* input, size_t input_length,
    const uint8_t* extension, size_t extension_length, uint8_t* output);
void a2fo_rs_gameplay_centered_fill(const A2foRsRect* source, const A2foRsRect* destination, float ratio, uint8_t horizontal, A2foRsFill* output);
int32_t a2fo_rs_gameplay_segment_at(const A2foRsRect* rectangles, float x, float y);
#ifdef __cplusplus
}
#endif
#endif
