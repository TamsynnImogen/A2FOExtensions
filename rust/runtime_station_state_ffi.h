#pragma once
// Private C++ view of the Rust-owned Station Rotation state. Layout is checked
// against Rust by a focused native fixture before this feature is integrated.
#include "../sdk/include/a2fo_module_api.h"
#include "../modules/A2FOStationRotation/rotation.hpp"
#include <cstddef>
#include <cstdint>
#include <type_traits>

struct A2foRsStationState {
    const A2FO_ModuleApi* api;
    std::uint8_t* armada;
    std::uint8_t* fleet;
    bool ready;
    a2fo::station_rotation::PlacementControl control;
    A2FO_InlineHook keyboard, broadcast, queue, set_command, preview;
    void* sending_class;
    unsigned sending_turns;
    std::uint64_t sending_activation;
    std::uint64_t previous_activation;
    A2FO_InlineHook action, render_footprints, footprint, can_place, placement_clear, build;
    const void* build_process;
    const void* placement_class;
    unsigned placement_turns;
    unsigned ui_footprint_depth;
};

// State layout followed by each nested POD layout, including all field offsets.
struct A2foRsStationLayout { std::size_t values[36]; };
extern "C" A2foRsStationState* a2fo_rs_station_state();
extern "C" A2foRsStationLayout a2fo_rs_station_state_layout();

static_assert(std::is_standard_layout<A2foRsStationState>::value, "station slot layout");
static_assert(std::is_standard_layout<a2fo::station_rotation::PlacementControl>::value,
              "placement layout");
static_assert(sizeof(bool) == 1, "Rust ready byte must match C++ bool");

inline bool a2fo_station_state_layout_matches() noexcept {
    const auto layout = a2fo_rs_station_state_layout();
    const std::size_t native[] = {
        sizeof(A2foRsStationState), alignof(A2foRsStationState),
        offsetof(A2foRsStationState, api), offsetof(A2foRsStationState, armada),
        offsetof(A2foRsStationState, fleet), offsetof(A2foRsStationState, ready),
        offsetof(A2foRsStationState, control), offsetof(A2foRsStationState, keyboard),
        offsetof(A2foRsStationState, broadcast), offsetof(A2foRsStationState, queue),
        offsetof(A2foRsStationState, set_command), offsetof(A2foRsStationState, preview),
        offsetof(A2foRsStationState, sending_class), offsetof(A2foRsStationState, sending_turns),
        offsetof(A2foRsStationState, sending_activation), offsetof(A2foRsStationState, previous_activation),
        offsetof(A2foRsStationState, action), offsetof(A2foRsStationState, render_footprints),
        offsetof(A2foRsStationState, footprint), offsetof(A2foRsStationState, can_place),
        offsetof(A2foRsStationState, placement_clear), offsetof(A2foRsStationState, build),
        offsetof(A2foRsStationState, build_process), offsetof(A2foRsStationState, placement_class),
        offsetof(A2foRsStationState, placement_turns), offsetof(A2foRsStationState, ui_footprint_depth),
        sizeof(a2fo::station_rotation::PlacementControl),
        alignof(a2fo::station_rotation::PlacementControl),
        offsetof(a2fo::station_rotation::PlacementControl, target),
        offsetof(a2fo::station_rotation::PlacementControl, turns),
        offsetof(a2fo::station_rotation::PlacementControl, r_down),
        sizeof(A2FO_InlineHook), alignof(A2FO_InlineHook),
        offsetof(A2FO_InlineHook, target), offsetof(A2FO_InlineHook, gateway),
        offsetof(A2FO_InlineHook, length)
    };
    static_assert(sizeof(native) / sizeof(native[0]) == 36, "station layout field count");
    for (std::size_t i = 0; i < 36; ++i) if (native[i] != layout.values[i]) return false;
    return true;
}
