#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t (*A2foA1StateBoolRead)(void* context);
typedef uintptr_t (*A2foA1StatePointerRead)(void* context);

typedef struct A2foA1RegistrationDecision {
    uint8_t constructor_like;
    uint8_t admit;
} A2foA1RegistrationDecision;

/* Call after the native maximum/gain/class-label/build-item reads. `classify`
 * performs likely_constructionrig_class only if both earlier class clues fail. */
void a2fo_rs_stage9_a1_state_registration(
    uint8_t has_classlabel, uint8_t has_officer_items,
    uint8_t has_maximum, uint8_t has_gain,
    void* context, A2foA1StateBoolRead classify,
    A2foA1RegistrationDecision* output);

/* The race callback reads race[0] only if both earlier policy clues fail.
 * Call separately at the original storage and logging expressions. */
uint8_t a2fo_rs_stage9_a1_state_has_policy(
    uint8_t has_maximum, uint8_t has_gain,
    void* context, A2foA1StateBoolRead has_race);
uint8_t a2fo_rs_stage9_a1_state_completed_admit(
    uint8_t has_maximum, uint8_t has_gain);

/* Call inside the existing OfficerStateLockGuard immediately where the
 * credited-team comparison occurs; native map/state reads stay in C++. */
uint8_t a2fo_rs_stage9_a1_state_credit_transition(
    uintptr_t credited_team, uintptr_t new_team);

typedef struct A2foA1CursorSlotDecision {
    uint8_t capture_native;
    uint8_t replace_cache;
} A2foA1CursorSlotDecision;

/* `read_native` reads this slot's stored native cursor only when cached is
 * nonnull and differs from fallback. Native C++ then performs any capture,
 * rereads native cursor, checks active, and writes cache in original order. */
void a2fo_rs_stage9_a1_state_cursor_slot(
    uintptr_t cached, uintptr_t fallback, void* context,
    A2foA1StatePointerRead read_native,
    A2foA1CursorSlotDecision* output);
uint8_t a2fo_rs_stage9_a1_state_cursor_active(
    uint8_t active_replaced, uintptr_t native_cursor, uintptr_t active_cursor);
uint8_t a2fo_rs_stage9_a1_state_cursor_changed(
    uint8_t active_replaced, uint32_t cache_replacements);

#ifdef __cplusplus
}
#endif
