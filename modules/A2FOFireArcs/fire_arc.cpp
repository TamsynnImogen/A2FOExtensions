#include "fire_arc.hpp"
#include "../../rust/motion_ffi.h"
#include <algorithm>
namespace a2fo::fire_arcs {
namespace {
A2foRsMatrix34 rs(const Matrix34 &m) noexcept {
    A2foRsMatrix34 r{};
    std::copy(m.values, m.values + 12, r.values);
    return r;
}
A2foRsArcConfig rs(const ArcConfig &c) noexcept {
    return {static_cast<int>(c.mode), c.yaw_degrees,         c.pitch_degrees,
            c.yaw_angle_degrees,      c.pitch_angle_degrees, c.cone_angle_degrees};
}
void copy(ArcConfig &c, A2foRsArcConfig r) noexcept {
    c.mode = static_cast<ArcMode>(r.mode);
    c.yaw_degrees = r.yaw;
    c.pitch_degrees = r.pitch;
    c.yaw_angle_degrees = r.yaw_angle;
    c.pitch_angle_degrees = r.pitch_angle;
    c.cone_angle_degrees = r.cone_angle;
}
} // namespace
bool defer_arc_to_candidate_selection(bool cannon_imp, bool primary) noexcept {
    return a2fo_rs_arc_defer(cannon_imp, primary) != 0;
}
float normalize_degrees(float v) noexcept { return a2fo_rs_arc_normalize(v); }
void normalize_config(ArcConfig *config) noexcept {
    if (!config)
        return;
    auto r = rs(*config);
    a2fo_rs_arc_normalize_config(&r);
    copy(*config, r);
}
bool allows_target(const ArcConfig &config, const Matrix34 &matrix,
                   const float target[3]) noexcept {
    if (!target)
        return false;
    const auto r = rs(matrix);
    return a2fo_rs_arc_allows(rs(config), &r, target) != 0;
}
std::size_t build_visualization_lines(const ArcConfig &config, const Matrix34 &matrix,
                                      const float origin[3], float radius,
                                      ArcLine *output, std::size_t capacity) noexcept {
    if (!origin || !output || capacity == 0)
        return 0;
    // The source emits at most 97 lines; keep a bounded POD scratch buffer and
    // let Rust apply the caller's capacity before copying public records.
    A2foRsArcLine lines[97]{};
    const auto r = rs(matrix);
    const std::size_t count = a2fo_rs_arc_lines(rs(config), &r, origin, radius, lines,
                                                std::min(capacity, std::size_t{97}));
    for (std::size_t i = 0; i < count; ++i) {
        for (int j = 0; j < 3; ++j) {
            output[i].start.values[j] = lines[i].start[j];
            output[i].end.values[j] = lines[i].end[j];
        }
        output[i].style = static_cast<ArcLineStyle>(lines[i].style);
    }
    return count;
}
} // namespace a2fo::fire_arcs
