#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t (*A2foStage8ObjectEditorOp)(void*, uint32_t);
uint8_t a2fo_rs_stage8_b_object_editor_initialize(void*, A2foStage8ObjectEditorOp);
uint8_t a2fo_rs_stage8_b_object_editor_read_field(void*, A2foStage8ObjectEditorOp);
void a2fo_rs_stage8_b_object_editor_event(void*, A2foStage8ObjectEditorOp);
uint8_t a2fo_rs_stage8_b_object_editor_write_field(void*, A2foStage8ObjectEditorOp);
uint8_t a2fo_rs_stage8_b_object_editor_find_entry(void*, A2foStage8ObjectEditorOp);
uint8_t a2fo_rs_stage8_b_object_editor_prepare_entry(void*, A2foStage8ObjectEditorOp);
void a2fo_rs_stage8_b_object_editor_assign_entry(void*, A2foStage8ObjectEditorOp);
uint8_t a2fo_rs_stage8_b_object_editor_resolve_shields(void*, A2foStage8ObjectEditorOp);
uint8_t a2fo_rs_stage8_b_object_editor_make_save_state(void*, A2foStage8ObjectEditorOp);
uint8_t a2fo_rs_stage8_b_object_editor_read_values(void*, A2foStage8ObjectEditorOp);
uint8_t a2fo_rs_stage8_b_object_editor_apply_values(void*, A2foStage8ObjectEditorOp);
uint8_t a2fo_rs_stage8_b_object_editor_take_loaded(void*, A2foStage8ObjectEditorOp);
uint8_t a2fo_rs_stage8_b_object_editor_read_shield_controls(void*, A2foStage8ObjectEditorOp);
void a2fo_rs_stage8_b_object_editor_undo_controls(void*, A2foStage8ObjectEditorOp);
uint8_t a2fo_rs_stage8_b_object_editor_accept_controls(void*, A2foStage8ObjectEditorOp);
void a2fo_rs_stage8_b_object_editor_dialog_proc(void*, A2foStage8ObjectEditorOp);
void a2fo_rs_stage8_b_object_editor_dialog_destructor(void*, A2foStage8ObjectEditorOp);
uint8_t a2fo_rs_stage8_b_object_editor_add_controls(void*, A2foStage8ObjectEditorOp);
#ifdef __cplusplus
}
#endif
