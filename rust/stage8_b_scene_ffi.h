#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foStage8SceneOp)(void*, uint32_t, uint32_t);
uint8_t a2fo_rs_stage8_b_scene_start(void*, A2foStage8SceneOp);
void a2fo_rs_stage8_b_scene_configure(void*, A2foStage8SceneOp);
int32_t a2fo_rs_stage8_b_scene_choice_at(void*, A2foStage8SceneOp);
void a2fo_rs_stage8_b_scene_choose(int32_t, void*, A2foStage8SceneOp);
uint8_t a2fo_rs_stage8_b_scene_select(void*, A2foStage8SceneOp);
uint8_t a2fo_rs_stage8_b_scene_departure(void*, A2foStage8SceneOp);
void a2fo_rs_stage8_b_scene_loading_frame(void*, A2foStage8SceneOp);
void a2fo_rs_stage8_b_scene_frame(void*, A2foStage8SceneOp);
uint8_t a2fo_rs_stage8_b_scene_handover(void*, A2foStage8SceneOp);
uint8_t a2fo_rs_stage8_b_scene_worker_step(void*, A2foStage8SceneOp);
void a2fo_rs_stage8_b_scene_present_handover(void*, A2foStage8SceneOp);
void a2fo_rs_stage8_b_scene_destination(void*, A2foStage8SceneOp);
void a2fo_rs_stage8_b_scene_end_handover(void*, A2foStage8SceneOp);
uint8_t a2fo_rs_stage8_b_scene_draw_handover(void*, A2foStage8SceneOp);
void a2fo_rs_stage8_b_scene_frame_presented(void*, A2foStage8SceneOp);
void a2fo_rs_stage8_b_scene_canvas_completed(void*, A2foStage8SceneOp);
void a2fo_rs_stage8_b_scene_frame_drawn(void*, A2foStage8SceneOp);
void a2fo_rs_stage8_b_scene_shutdown(void*, A2foStage8SceneOp);
uint8_t a2fo_rs_stage8_b_scene_erase(void*, A2foStage8SceneOp);
uint8_t a2fo_rs_stage8_b_scene_window(void*, A2foStage8SceneOp);
uint8_t a2fo_rs_stage8_b_scene_backend_load(void*, A2foStage8SceneOp);
uint8_t a2fo_rs_stage8_b_scene_resize(void*, A2foStage8SceneOp);
uint8_t a2fo_rs_stage8_b_scene_backend_create(void*, A2foStage8SceneOp);
uint8_t a2fo_rs_stage8_b_scene_backend_render(void*, A2foStage8SceneOp);
typedef struct A2foStage8SceneRect {int32_t x,y,width,height;} A2foStage8SceneRect;
int32_t a2fo_rs_stage8_b_scene_page_count(int32_t, int32_t);
int32_t a2fo_rs_stage8_b_scene_choice_count(int32_t, int32_t, int32_t);
void a2fo_rs_stage8_b_scene_choice_rect(int32_t,int32_t,int32_t,int32_t,int32_t,
    int32_t,A2foStage8SceneRect*);
void a2fo_rs_stage8_b_scene_paint_choices(void*, A2foStage8SceneOp);
#ifdef __cplusplus
}
#endif
