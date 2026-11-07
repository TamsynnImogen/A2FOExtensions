#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foStage8BinkOp)(void*, uint32_t);
uint8_t a2fo_rs_stage8_b_bink_initialize(void*, A2foStage8BinkOp);
void a2fo_rs_stage8_b_bink_shutdown(void*, A2foStage8BinkOp);
void a2fo_rs_stage8_b_bink_window(void*, A2foStage8BinkOp);
uint8_t a2fo_rs_stage8_b_bink_play(void*, A2foStage8BinkOp);
void a2fo_rs_stage8_b_bink_render(void*, A2foStage8BinkOp);
uint8_t a2fo_rs_stage8_b_bink_preserve_shell(void*, A2foStage8BinkOp);
uint8_t a2fo_rs_stage8_b_bink_seed_presentation(void*, A2foStage8BinkOp);
void a2fo_rs_stage8_b_bink_gdi(void*, A2foStage8BinkOp);
void a2fo_rs_stage8_b_bink_texture(void*, A2foStage8BinkOp);
void a2fo_rs_stage8_b_bink_prepare_viewport(void*, A2foStage8BinkOp);
uint8_t a2fo_rs_stage8_b_bink_apply_window(void*, A2foStage8BinkOp);
void a2fo_rs_stage8_b_bink_log_once(void*, A2foStage8BinkOp);
void a2fo_rs_stage8_b_bink_fit_dimensions(uint32_t, uint32_t, uint32_t, uint32_t,
                                          uint32_t*, uint32_t*);
#ifdef __cplusplus
}
#endif
