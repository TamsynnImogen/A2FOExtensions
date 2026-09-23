#pragma once

#include "rotation.hpp"
#include <cmath>

namespace a2fo::station_rotation {
struct Rectangle { float min_x, min_z, max_x, max_z; };
struct Margins { float positive_x, negative_x, positive_z, negative_z; };

inline Rectangle rotate_rectangle(Rectangle r, unsigned turns) noexcept {
    switch (turns & 3) {
    case 1: return {r.min_z, -r.max_x, r.max_z, -r.min_x};
    case 2: return {-r.max_x, -r.max_z, -r.min_x, -r.min_z};
    case 3: return {-r.max_z, r.min_x, -r.min_z, r.max_x};
    default: return r;
    }
}

inline Rectangle padded_footprint(const float* bounds, Margins margin) noexcept {
    return {bounds[0] - margin.negative_x, bounds[2] - margin.negative_z,
            bounds[3] + margin.positive_x, bounds[5] + margin.positive_z};
}

// Add/RemoveFromPathPlanners both add the class margins themselves. Pass a
// private box which yields the rotated padded rectangle after that addition;
// never mutate a shared SOD or class (other instances can face differently).
inline void planner_bounds(float* bounds, Margins margin, unsigned turns) noexcept {
    const auto r = rotate_rectangle(padded_footprint(bounds, margin), turns);
    bounds[0] = r.min_x + margin.negative_x;
    bounds[2] = r.min_z + margin.negative_z;
    bounds[3] = r.max_x - margin.positive_x;
    bounds[5] = r.max_z - margin.positive_z;
}

inline unsigned cardinal_turns(const float* matrix) noexcept {
    float expected[12]{};
    for (unsigned turns = 1; turns < 4; ++turns) {
        set_yaw(expected, turns);
        bool equal = true;
        for (unsigned i = 0; i < 9; ++i)
            equal = equal && std::isfinite(matrix[i]) && std::fabs(matrix[i] - expected[i]) < 0.0001f;
        if (equal) return turns;
    }
    return 0;
}
} // namespace a2fo::station_rotation
