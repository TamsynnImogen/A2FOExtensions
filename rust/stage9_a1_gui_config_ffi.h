#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t (*A2foA1GuiTooltipRead)(void* context, const char* key);
typedef uint8_t (*A2foA1GuiStep)(void* context, uint32_t operation,
                                 uint32_t index);
typedef uint8_t (*A2foA1GuiReadReady)(void* context);

// Calls every frame getter, including those after the first missing key.
uint8_t a2fo_rs_stage9_a1_gui_tooltip_frame(
    void* context, A2foA1GuiTooltipRead read);
// Captures only the contiguous controlButton1..12 prefix.
int32_t a2fo_rs_stage9_a1_gui_control_buttons(
    void* context, A2foA1GuiStep step);
// operations 0..2 are fallback getters, 3..5 are native area checks.
uint8_t a2fo_rs_stage9_a1_gui_cinematic_layout(
    uint8_t has_panel, uint8_t use_raw, void* context, A2foA1GuiStep step);
// operations 0..1 rectangle getters; 2..3 area checks; 4..7 sprite getters.
uint8_t a2fo_rs_stage9_a1_gui_cinematic_buttons(
    uint8_t layout_complete, uint8_t use_raw, uint8_t raw_buttons,
    void* context, A2foA1GuiStep step);
// Bits 0/1 request background/text tooltip fallbacks, respectively.
uint8_t a2fo_rs_stage9_a1_gui_tooltip_fallbacks(
    uint8_t has_background, uint8_t has_text);
uint8_t a2fo_rs_stage9_a1_gui_speed_background(
    uint8_t inputs_complete, int32_t piece_count,
    void* context, A2foA1GuiStep piece);
uint8_t a2fo_rs_stage9_a1_gui_background_count_valid(int32_t count);
uint8_t a2fo_rs_stage9_a1_gui_background_pieces(
    uint32_t count, void* context, A2foA1GuiStep piece);
uint8_t a2fo_rs_stage9_a1_gui_resource_strip(
    void* context, A2foA1GuiStep row);
// A false completeness result must not read the native runtime flag.
uint8_t a2fo_rs_stage9_a1_gui_ready(
    uint8_t complete, void* context, A2foA1GuiReadReady read_ready);

#ifdef __cplusplus
}
#endif
