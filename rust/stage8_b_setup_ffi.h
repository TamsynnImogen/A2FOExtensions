#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foStage8SetupOp)(void*, uint32_t);
void a2fo_rs_stage8_b_setup_start(void*, A2foStage8SetupOp);
typedef uint8_t (*A2foStage8SetupAppend)(void*, uint8_t);
uint8_t a2fo_rs_stage8_b_setup_asset(const uint8_t*, uintptr_t, void*, A2foStage8SetupAppend);
void a2fo_rs_stage8_b_setup_observe(void*, A2foStage8SetupOp);
uint8_t a2fo_rs_stage8_b_setup_handover(void*, A2foStage8SetupOp);
void a2fo_rs_stage8_b_setup_shutdown(void*, A2foStage8SetupOp);
uint8_t a2fo_rs_stage8_b_setup_present(void*, A2foStage8SetupOp);
uint8_t a2fo_rs_stage8_b_setup_window_active(void*, A2foStage8SetupOp);
typedef uint64_t (*A2foStage8SetupIndexedOp)(void*, uint32_t, uintptr_t);
void a2fo_rs_stage8_b_setup_restore_logs(void*, A2foStage8SetupIndexedOp);
uint8_t a2fo_rs_stage8_b_setup_hide_logs(void*, A2foStage8SetupIndexedOp);
uint8_t a2fo_rs_stage8_b_setup_snapshot(void*, A2foStage8SetupOp);
uint8_t a2fo_rs_stage8_b_setup_seed(void*, A2foStage8SetupOp);
uint8_t a2fo_rs_stage8_b_setup_erase(void*, A2foStage8SetupOp);
void a2fo_rs_stage8_b_setup_admission(void*, A2foStage8SetupOp, uint32_t);
void a2fo_rs_stage8_b_setup_widget(void*, A2foStage8SetupOp);
uint8_t a2fo_rs_stage8_b_setup_control(void*, A2foStage8SetupOp);
#ifdef __cplusplus
}
#endif
