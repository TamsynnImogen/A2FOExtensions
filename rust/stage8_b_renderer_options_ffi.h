#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foStage8RendererOp)(void*, uint32_t, uint32_t);
int32_t a2fo_rs_stage8_b_renderer_scaled_coordinate(int32_t, int32_t, int32_t);
uint8_t a2fo_rs_stage8_b_renderer_valid_bump_bounds(int32_t, int32_t, int32_t);
uint8_t a2fo_rs_stage8_b_renderer_reconcile(void*, A2foStage8RendererOp);
uint8_t a2fo_rs_stage8_b_renderer_schedule(void*, A2foStage8RendererOp);
uint8_t a2fo_rs_stage8_b_renderer_select(void*, A2foStage8RendererOp, uint32_t);
uint8_t a2fo_rs_stage8_b_renderer_install(void*, A2foStage8RendererOp);
void a2fo_rs_stage8_b_renderer_observe_monitor(void*, A2foStage8RendererOp);
void a2fo_rs_stage8_b_renderer_monitor_timer(void*, A2foStage8RendererOp);
void a2fo_rs_stage8_b_renderer_window(void*, A2foStage8RendererOp);
void a2fo_rs_stage8_b_renderer_effect_controls(void*, A2foStage8RendererOp);
uint8_t a2fo_rs_stage8_b_renderer_subclass(void*, A2foStage8RendererOp);
void a2fo_rs_stage8_b_renderer_refresh_monitor(void*, A2foStage8RendererOp);
void a2fo_rs_stage8_b_renderer_save_monitor(void*, A2foStage8RendererOp, int32_t);
void a2fo_rs_stage8_b_renderer_monitor_controls(void*, A2foStage8RendererOp);
void a2fo_rs_stage8_b_renderer_controls(void*, A2foStage8RendererOp);
uint8_t a2fo_rs_stage8_b_renderer_read_effect(void*, A2foStage8RendererOp);
void a2fo_rs_stage8_b_renderer_save_effect(void*, A2foStage8RendererOp);
uint8_t a2fo_rs_stage8_b_renderer_preserve_payload(void*, A2foStage8RendererOp);
void a2fo_rs_stage8_b_renderer_position_effect(void*, A2foStage8RendererOp);
uint8_t a2fo_rs_stage8_b_renderer_create_control(void*, A2foStage8RendererOp);
#ifdef __cplusplus
}
#endif
