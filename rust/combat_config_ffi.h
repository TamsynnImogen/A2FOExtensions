#ifndef A2FO_RUST_COMBAT_CONFIG_FFI_H
#define A2FO_RUST_COMBAT_CONFIG_FFI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct A2foRsOrderTargetState {
    uint32_t ordered_target_handle;
    uint32_t visual_target_handle;
} A2foRsOrderTargetState;

/* Status values match FireArcSettingStatus: absent=0, valid=1, invalid=2. */
uint32_t a2fo_rs_combat_parse_firearc(const uint8_t* source, size_t length,
                                      uint8_t* enabled);
uint32_t a2fo_rs_combat_attack_target(uint32_t command_id,
                                      uint32_t target_form,
                                      uint32_t target_handle,
                                      uint8_t target_exists);
void a2fo_rs_combat_update_target(uint32_t previous_ordered_target_handle,
                                  uint32_t next_ordered_target_handle,
                                  uint32_t current_visual_target_handle,
                                  A2foRsOrderTargetState* output);
#ifdef __cplusplus
}
#endif
#endif
