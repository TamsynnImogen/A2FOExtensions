#include "../rust/runtime_station_state_ffi.h"
#include <cstdio>
#include <cstring>

int main() {
    if (!a2fo_station_state_layout_matches()) {
        std::fputs("station state layout mismatch\n", stderr);
        return 1;
    }
    auto* state = a2fo_rs_station_state();
    if (!state || state != a2fo_rs_station_state()) return 2;
    if (state->api || state->armada || state->fleet || state->ready ||
        state->control.target || state->control.turns || state->control.r_down ||
        state->sending_activation || state->previous_activation ||
        state->placement_class || state->ui_footprint_depth) return 3;

    state->ready = true;
    state->control.key(0x1234, true, true, false);
    state->keyboard.gateway = reinterpret_cast<void*>(0x5678);
    state->sending_activation = UINT64_C(0xFEDCBA9876543210);
    state->previous_activation = UINT64_C(0x123456789ABCDEF0);
    state->placement_class = reinterpret_cast<void*>(0x2468);
    state->ui_footprint_depth = 7;
    const auto* same = a2fo_rs_station_state();
    if (!same->ready || same->control.target != 0x1234 || same->control.turns != 1 ||
        !same->control.r_down || same->keyboard.gateway != reinterpret_cast<void*>(0x5678) ||
        same->sending_activation != UINT64_C(0xFEDCBA9876543210) ||
        same->previous_activation != UINT64_C(0x123456789ABCDEF0) ||
        same->placement_class != reinterpret_cast<void*>(0x2468) ||
        same->ui_footprint_depth != 7) return 4;
    std::puts("runtime station state: passed");
    return 0;
}
