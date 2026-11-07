#ifndef A2FO_STAGE9_A1_LAYOUT_FFI_H
#define A2FO_STAGE9_A1_LAYOUT_FFI_H

#include <cstddef>
#include <cstdint>

extern "C" {
bool a2fo_rs_stage9_a1_layout_crew(
    const std::int32_t* crew_rectangle,
    const std::int32_t* crew_dot_rectangle,
    const std::int32_t* hidden_rectangle,
    std::uint8_t custom_identity_text,
    std::int32_t* component_output,
    std::int32_t* text_output,
    std::uint8_t* has_crew_dot_output);
bool a2fo_rs_stage9_a1_layout_select_rectangle(
    const std::int32_t* visible_rectangle,
    const std::int32_t* hidden_rectangle,
    std::uint8_t use_hidden,
    std::int32_t* output);
bool a2fo_rs_stage9_a1_layout_screen_rectangle(
    const std::int32_t* panel_rectangle,
    const std::int32_t* local_rectangle,
    std::int32_t* output);
bool a2fo_rs_stage9_a1_layout_local_rectangle(
    const std::int32_t* screen_rectangle,
    const std::int32_t* panel_rectangle,
    std::int32_t* output);
bool a2fo_rs_stage9_a1_layout_queue_rectangle(
    const std::int32_t* screen_rectangles,
    std::size_t rectangle_count,
    std::size_t index,
    const std::int32_t* ship_display_rectangle,
    const std::int32_t* hidden_rectangle,
    std::int32_t* output);
}

#endif
