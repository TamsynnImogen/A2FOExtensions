#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foStage8CoreOp)(void*, uint32_t);
typedef uint64_t (*A2foStage8CoreIndexedOp)(void*, uint32_t, uint64_t);
uint8_t a2fo_rs_stage8_b_module_menu_install(void*, A2foStage8CoreOp);
void a2fo_rs_stage8_b_module_menu_button(void*, A2foStage8CoreOp);
int32_t a2fo_rs_stage8_b_module_menu_button_left(int32_t, int32_t);
uint8_t a2fo_rs_stage8_b_module_menu_launch_admit(void*, A2foStage8CoreOp);
void a2fo_rs_stage8_b_module_menu_launch_commit(void*, A2foStage8CoreOp);
void a2fo_rs_stage8_b_module_menu_click(void*, A2foStage8CoreOp);
uint8_t a2fo_rs_stage8_b_module_menu_save(void*, A2foStage8CoreIndexedOp);
void a2fo_rs_stage8_b_module_menu_flag_row(int32_t, int32_t, int32_t, int32_t,
                                          int32_t*, int32_t*);
void a2fo_rs_stage8_b_module_menu_toggle_row(int32_t, int32_t, int32_t, int32_t,
                                            int32_t*, int32_t*);
void a2fo_rs_stage8_b_module_menu_flags(void*, A2foStage8CoreOp);
uint8_t a2fo_rs_stage8_b_module_menu_jet_flags(void*, A2foStage8CoreOp);
uint8_t a2fo_rs_stage8_b_module_menu_state_locked(uint32_t);
uint8_t a2fo_rs_stage8_b_module_menu_state_checked(uint32_t);
uint64_t a2fo_rs_stage8_b_module_menu_notification(void*, A2foStage8CoreIndexedOp);
typedef uint8_t (*A2foStage8CoreIsSpace)(uint8_t);
uint8_t a2fo_rs_stage8_b_module_menu_has_switch(const uint8_t*, uintptr_t,
    const uint8_t*, uintptr_t, A2foStage8CoreIsSpace);
void a2fo_rs_stage8_b_module_menu_append_switches(void*, A2foStage8CoreOp);
uint8_t a2fo_rs_stage8_b_module_menu_selected_mod(void*, A2foStage8CoreOp);
uint8_t a2fo_rs_stage8_b_module_menu_resolve_mod(void*, A2foStage8CoreOp);
void a2fo_rs_stage8_b_module_menu_dialog(void*, A2foStage8CoreIndexedOp);
uint32_t a2fo_rs_stage8_b_module_menu_import_for(void*, A2foStage8CoreIndexedOp);
uint8_t a2fo_rs_stage8_b_module_menu_import_all(void*, A2foStage8CoreOp);
void a2fo_rs_stage8_b_module_menu_launch_intercept(void*, A2foStage8CoreOp, uint32_t);
uint64_t a2fo_rs_stage8_b_module_menu_dialog_message(void*, A2foStage8CoreOp);
#ifdef __cplusplus
}
#endif
