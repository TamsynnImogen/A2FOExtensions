#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foStage8CanvasOp)(void*, uint32_t);
uint8_t a2fo_rs_stage8_b_canvas_initialize(void*, A2foStage8CanvasOp);
void a2fo_rs_stage8_b_canvas_active_mode(void*, A2foStage8CanvasOp);
void a2fo_rs_stage8_b_canvas_shutdown(void*, A2foStage8CanvasOp);
uint8_t a2fo_rs_stage8_b_canvas_copy(void*, A2foStage8CanvasOp);
uint8_t a2fo_rs_stage8_b_canvas_presenter(void*, A2foStage8CanvasOp);
void a2fo_rs_stage8_b_canvas_register(void*, A2foStage8CanvasOp);
void a2fo_rs_stage8_b_canvas_scroll(void*, A2foStage8CanvasOp);
uint8_t a2fo_rs_stage8_b_canvas_acquire(void*, A2foStage8CanvasOp);
uint8_t a2fo_rs_stage8_b_canvas_finish(void*, A2foStage8CanvasOp);
uint8_t a2fo_rs_stage8_b_canvas_mouse_message(uint32_t);
void a2fo_rs_stage8_b_canvas_logical_parameter(void*, A2foStage8CanvasOp,
                                               uint32_t);
void a2fo_rs_stage8_b_canvas_dialog_dispatch(void*, A2foStage8CanvasOp);
void a2fo_rs_stage8_b_canvas_window_dispatch(void*, A2foStage8CanvasOp);
uint8_t a2fo_rs_stage8_b_canvas_adopt(void*, A2foStage8CanvasOp);
void a2fo_rs_stage8_b_canvas_adopt_child(void*, A2foStage8CanvasOp);
void a2fo_rs_stage8_b_canvas_fit_children(void*, A2foStage8CanvasOp);
uint8_t a2fo_rs_stage8_b_canvas_ensure_child(void*, A2foStage8CanvasOp);
void a2fo_rs_stage8_b_canvas_bridge_dialog(void*, A2foStage8CanvasOp);
void a2fo_rs_stage8_b_canvas_update_font(void*, A2foStage8CanvasOp);
void a2fo_rs_stage8_b_canvas_refresh_seed(void*, A2foStage8CanvasOp);
uint8_t a2fo_rs_stage8_b_canvas_present_seed(void*, A2foStage8CanvasOp);
void a2fo_rs_stage8_b_canvas_represent_seed(void*, A2foStage8CanvasOp);
uint8_t a2fo_rs_stage8_b_canvas_normalize_fit(void*, A2foStage8CanvasOp);
uint8_t a2fo_rs_stage8_b_canvas_nested_parent(void*, A2foStage8CanvasOp);
void a2fo_rs_stage8_b_canvas_quiet_secondary(void*, A2foStage8CanvasOp, const uint8_t*);
void a2fo_rs_stage8_b_canvas_move_window(void*, A2foStage8CanvasOp);
void a2fo_rs_stage8_b_canvas_position_window(void*, A2foStage8CanvasOp);
#ifdef __cplusplus
}
#endif
