#ifndef A2FO_STAGE9_TEXTURE_VISUALS_FFI_H
#define A2FO_STAGE9_TEXTURE_VISUALS_FFI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t (*A2FO_Stage9TextureStep)(void*, uint32_t, uintptr_t);
void a2fo_rs_stage9_texture_register(A2FO_Stage9TextureStep, void*);
void a2fo_rs_stage9_texture_update(A2FO_Stage9TextureStep, void*);
void a2fo_rs_stage9_texture_render_plan(A2FO_Stage9TextureStep, void*);
void a2fo_rs_stage9_texture_walk(A2FO_Stage9TextureStep, void*);
void a2fo_rs_stage9_texture_subtree(A2FO_Stage9TextureStep, void*);
void a2fo_rs_stage9_texture_iterate(A2FO_Stage9TextureStep, void*);
void a2fo_rs_stage9_texture_resolve(A2FO_Stage9TextureStep, void*);
void a2fo_rs_stage9_texture_init(A2FO_Stage9TextureStep, void*);
void a2fo_rs_stage9_texture_shutdown(A2FO_Stage9TextureStep, void*);
void a2fo_rs_stage9_texture_prepare_render(A2FO_Stage9TextureStep, void*);
void a2fo_rs_stage9_texture_update_admission(A2FO_Stage9TextureStep, void*);
uintptr_t a2fo_rs_stage9_texture_find_variant(A2FO_Stage9TextureStep, void*);
void a2fo_rs_stage9_texture_repeat(A2FO_Stage9TextureStep, void*, uintptr_t);
uintptr_t a2fo_rs_stage9_texture_sum(A2FO_Stage9TextureStep, void*, uintptr_t);
uintptr_t a2fo_rs_stage9_texture_select_mesh(A2FO_Stage9TextureStep, void*);
uintptr_t a2fo_rs_stage9_texture_selected_mesh(A2FO_Stage9TextureStep, void*);
void a2fo_rs_stage9_texture_erase_race_name(A2FO_Stage9TextureStep, void*);
void a2fo_rs_stage9_texture_set_race_name(A2FO_Stage9TextureStep, void*);
int32_t a2fo_rs_stage9_texture_borg_result(A2FO_Stage9TextureStep, void*, int32_t);

#ifdef __cplusplus
}
#endif
#endif
