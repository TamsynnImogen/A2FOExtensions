#pragma once
// Private typed view; production checks every field before binding aliases.
#include <windows.h>
#include "../sdk/include/a2fo_module_api.h"
#include <cstddef>
#include <type_traits>

struct A2foRsNormalState {
    const A2FO_ModuleApi* api;
    HMODULE armada;
    HMODULE fleet_ops;
    bool runtime_ready;
    volatile LONG logged_first_default_weapon;
    volatile LONG logged_first_listed_allowed;
    volatile LONG logged_first_listed_blocked;
};
struct A2foRsNormalLayout { std::size_t values[9]; };
extern "C" A2foRsNormalState* a2fo_rs_normal_state();
extern "C" A2foRsNormalLayout a2fo_rs_normal_state_layout();
static_assert(std::is_standard_layout<A2foRsNormalState>::value, "normal state layout");
static_assert(sizeof(bool) == 1 && sizeof(LONG) == 4, "normal state field widths");
inline bool a2fo_normal_state_layout_matches() noexcept {
    const auto layout = a2fo_rs_normal_state_layout();
    const std::size_t expected[] = {
        sizeof(A2foRsNormalState), alignof(A2foRsNormalState),
        offsetof(A2foRsNormalState, api), offsetof(A2foRsNormalState, armada),
        offsetof(A2foRsNormalState, fleet_ops), offsetof(A2foRsNormalState, runtime_ready),
        offsetof(A2foRsNormalState, logged_first_default_weapon),
        offsetof(A2foRsNormalState, logged_first_listed_allowed),
        offsetof(A2foRsNormalState, logged_first_listed_blocked),
    };
    static_assert(sizeof(expected) / sizeof(expected[0]) == 9, "normal layout descriptor");
    for (std::size_t i = 0; i < 9; ++i)
        if (expected[i] != layout.values[i]) return false;
    return true;
}
