#ifndef A2FO_STAGE9_CRAFT_RUNTIME_FFI_H
#define A2FO_STAGE9_CRAFT_RUNTIME_FFI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t (*A2FO_Stage9CraftStep)(void*, uint32_t, uintptr_t);

void a2fo_rs_stage9_craft_copy_identity_rows(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_register_class(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_ensure_identity(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_hover_update(A2FO_Stage9CraftStep, void*, uintptr_t, int32_t);
void a2fo_rs_stage9_craft_colour_apply(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_weapon_colour(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_panel_styles(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_refresh_ui(A2FO_Stage9CraftStep, void*, uintptr_t, uintptr_t);
uint8_t a2fo_rs_stage9_craft_resolve_shield_sprites(A2FO_Stage9CraftStep, void*, uintptr_t, uintptr_t);
uint8_t a2fo_rs_stage9_craft_resolve_simple_sprites(A2FO_Stage9CraftStep, void*, uintptr_t);
uint8_t a2fo_rs_stage9_craft_format_ammunition_tip(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_register_selected_status(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_register_directional(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_register_ammunition(A2FO_Stage9CraftStep, void*, uintptr_t);
uint8_t a2fo_rs_stage9_craft_format_selected_status_tip(A2FO_Stage9CraftStep, void*);
uint8_t a2fo_rs_stage9_craft_all_sprites_live(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_render_panel(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_draw_status_bars(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_component_tooltip(A2FO_Stage9CraftStep, void*);
uint8_t a2fo_rs_stage9_craft_format_directional_tip(A2FO_Stage9CraftStep, void*);
uint8_t a2fo_rs_stage9_craft_append_directional_tip(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_draw_directional_tip(A2FO_Stage9CraftStep, void*);
uint8_t a2fo_rs_stage9_craft_configure_native_tip(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_clear_native_tip(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_present_native_tip(A2FO_Stage9CraftStep, void*);
uint8_t a2fo_rs_stage9_craft_install_hooks(A2FO_Stage9CraftStep, void*);
uint8_t a2fo_rs_stage9_craft_module_init(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_draw_shield_rows(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_load_art(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_native_tip_phase(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_update_shield_tip_rects(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_animate_shield_ratios(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_value_rectangles(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_draw_ammo_rows(A2FO_Stage9CraftStep, void*, uintptr_t);
uint8_t a2fo_rs_stage9_craft_draw_shield_graphic(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_join_path(A2FO_Stage9CraftStep, void*);
uint8_t a2fo_rs_stage9_craft_read_art_file(A2FO_Stage9CraftStep, void*);
uint8_t a2fo_rs_stage9_craft_apply_hover_rectangle(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_wireframe_update(A2FO_Stage9CraftStep, void*);
uintptr_t a2fo_rs_stage9_craft_tooltip_show(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_tooltip_render(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_install_tooltip_hook(A2FO_Stage9CraftStep, void*);
uint8_t a2fo_rs_stage9_craft_install_icon_hooks(A2FO_Stage9CraftStep, void*, uintptr_t, uintptr_t);
uint8_t a2fo_rs_stage9_craft_install_constructor(A2FO_Stage9CraftStep, void*);
uint8_t a2fo_rs_stage9_craft_resolve_energy(A2FO_Stage9CraftStep, void*, uintptr_t);
uint8_t a2fo_rs_stage9_craft_resolve_directional(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_observer(A2FO_Stage9CraftStep, void*, uint8_t);
uint8_t a2fo_rs_stage9_craft_experience_progress(A2FO_Stage9CraftStep, void*);
uint8_t a2fo_rs_stage9_craft_value_colour(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_value_text_colour(A2FO_Stage9CraftStep, void*);
void a2fo_rs_stage9_craft_ship_icon_render(A2FO_Stage9CraftStep, void*);
uint8_t a2fo_rs_stage9_craft_copy_native_string(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_release_native_vector(A2FO_Stage9CraftStep, void*, uintptr_t);
void a2fo_rs_stage9_craft_reset_hover(A2FO_Stage9CraftStep, void*, uintptr_t);
int32_t a2fo_rs_stage9_craft_find_hover_row(A2FO_Stage9CraftStep, void*, uintptr_t);

#ifdef __cplusplus
}
#endif

#endif
