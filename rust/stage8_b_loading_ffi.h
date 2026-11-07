#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foStage8LoadingOp)(void*, uint32_t, uint32_t);
uint8_t a2fo_rs_stage8_b_loading_init(void*, A2foStage8LoadingOp);
uint8_t a2fo_rs_stage8_b_loading_mission(void*, A2foStage8LoadingOp);
uint8_t a2fo_rs_stage8_b_loading_release(void*, A2foStage8LoadingOp);
uint8_t a2fo_rs_stage8_b_loading_animation(void*, A2foStage8LoadingOp);
uint8_t a2fo_rs_stage8_b_loading_bar(void*, A2foStage8LoadingOp);
uint8_t a2fo_rs_stage8_b_loading_splash_begin(void*, A2foStage8LoadingOp);
uint8_t a2fo_rs_stage8_b_loading_splash_end(void*, A2foStage8LoadingOp);
uint8_t a2fo_rs_stage8_b_loading_present(void*, A2foStage8LoadingOp, float);
uint8_t a2fo_rs_stage8_b_loading_extra_frame(void*, A2foStage8LoadingOp);
uint8_t a2fo_rs_stage8_b_loading_players(void*, A2foStage8LoadingOp);
uint8_t a2fo_rs_stage8_b_loading_player_bindings(void*, A2foStage8LoadingOp);
uint8_t a2fo_rs_stage8_b_loading_status(void*, A2foStage8LoadingOp);
#ifdef __cplusplus
}
#endif
