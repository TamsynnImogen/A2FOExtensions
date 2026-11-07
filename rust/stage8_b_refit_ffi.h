#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foStage8RefitOp)(void*, uint32_t);
uint8_t a2fo_rs_stage8_b_refit_init(void*, A2foStage8RefitOp);
void a2fo_rs_stage8_b_refit_attach(void*, A2foStage8RefitOp);
void a2fo_rs_stage8_b_refit_restore(uint8_t, void*, A2foStage8RefitOp);
uint8_t a2fo_rs_stage8_b_refit_present_gate(void*, A2foStage8RefitOp);
void a2fo_rs_stage8_b_refit_shutdown(void*, A2foStage8RefitOp);
uint8_t a2fo_rs_stage8_b_refit_queue(void*, A2foStage8RefitOp);
uint8_t a2fo_rs_stage8_b_refit_timer(void*, A2foStage8RefitOp);
void a2fo_rs_stage8_b_refit_action(void*, A2foStage8RefitOp);
void a2fo_rs_stage8_b_refit_paint(void*, A2foStage8RefitOp);
void a2fo_rs_stage8_b_refit_mouse(uint32_t, void*, A2foStage8RefitOp);
uint8_t a2fo_rs_stage8_b_refit_key(uint32_t, uint32_t, void*, A2foStage8RefitOp);
uint8_t a2fo_rs_stage8_b_refit_present(void*, A2foStage8RefitOp);
void a2fo_rs_stage8_b_refit_dispatch(uint32_t, uint32_t, uint32_t,
                                     void*, A2foStage8RefitOp);
uint8_t a2fo_rs_stage8_b_refit_guarded_call(void*, A2foStage8RefitOp);
uint8_t a2fo_rs_stage8_b_refit_status(void*, A2foStage8RefitOp);
void a2fo_rs_stage8_b_refit_end_departure(void*, A2foStage8RefitOp);
#ifdef __cplusplus
}
#endif
