#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct A2foA1UiArea {
    int32_t x, y, width, height;
} A2foA1UiArea;

typedef struct A2foA1UiRect {
    int32_t left, top, right, bottom;
} A2foA1UiRect;

uint8_t a2fo_rs_stage9_a1_ui_restore(uint8_t pending, uint8_t producer_matches,
                                      uint32_t menu);
void a2fo_rs_stage9_a1_ui_decide(uint8_t speed, uint8_t control,
                                 uint8_t width, uint8_t height,
                                 uint8_t* legacy, uint8_t* apply,
                                 int32_t* reference_width,
                                 int32_t* reference_height);
void a2fo_rs_stage9_a1_ui_rect(const A2foA1UiArea* area, A2foA1UiRect* out);
uint8_t a2fo_rs_stage9_a1_ui_usable(const A2foA1UiArea* area);
void a2fo_rs_stage9_a1_ui_union(const A2foA1UiArea* first,
                                const A2foA1UiArea* second,
                                const A2foA1UiArea* third, A2foA1UiArea* out);
void a2fo_rs_stage9_a1_ui_child_union(const A2foA1UiArea* panel,
                                      const A2foA1UiArea* child,
                                      const A2foA1UiArea* combined,
                                      A2foA1UiArea* out);
void a2fo_rs_stage9_a1_ui_child_parent(const A2foA1UiArea* parent,
                                       const A2foA1UiArea* child,
                                       A2foA1UiArea* out);
void a2fo_rs_stage9_a1_ui_race_icon(const A2foA1UiArea* component,
                                    const A2foA1UiArea* display,
                                    A2foA1UiRect* component_out,
                                    A2foA1UiRect* display_out);
void a2fo_rs_stage9_a1_ui_speed_slot(const A2foA1UiArea* button,
                                     const A2foA1UiArea* separator,
                                     int32_t gap, uint32_t slot,
                                     A2foA1UiArea* out);
int32_t a2fo_rs_stage9_a1_ui_index(const uint8_t* key, size_t key_len,
                                   const uint8_t* prefix, size_t prefix_len,
                                   int32_t maximum);
const char* a2fo_rs_stage9_a1_ui_rect_alias(const uint8_t* key, size_t key_len);
const char* a2fo_rs_stage9_a1_ui_string_alias(const uint8_t* key, size_t key_len);

#ifdef __cplusplus
}
#endif
