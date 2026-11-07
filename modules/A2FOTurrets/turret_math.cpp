#include "turret_math.hpp"
#include "../../rust/motion_ffi.h"
#include <algorithm>
namespace a2fo::turrets {
namespace {
A2foRsMatrix34 rs(const Matrix34 &m) noexcept {
    A2foRsMatrix34 r{};
    std::copy(m.values, m.values + 12, r.values);
    return r;
}
Matrix34 cpp(const A2foRsMatrix34 &m) noexcept {
    Matrix34 r{};
    std::copy(m.values, m.values + 12, r.values);
    return r;
}
A2foRsAimAngles rs(AimAngles a) noexcept { return {a.yaw_degrees, a.pitch_degrees}; }
AimAngles cpp(A2foRsAimAngles a) noexcept { return {a.yaw, a.pitch}; }
A2foRsAimLimits rs(const AimLimits &l) noexcept {
    return {l.yaw_min_degrees,   l.yaw_max_degrees,  l.pitch_min_degrees,
            l.pitch_max_degrees, l.yaw_rate_degrees, l.pitch_rate_degrees};
}
} // namespace
float clamp_value(float v, float min, float max) noexcept {
    return a2fo_rs_turret_clamp(v, min, max);
}
float normalize_degrees(float v) noexcept { return a2fo_rs_turret_normalize(v); }
AimAngles calculate_aim_angles(const Matrix34 &m, const float target[3],
                               const AimLimits &limits, AimAngles fallback) noexcept {
    if (!target)
        return fallback;
    const auto r = rs(m);
    return cpp(a2fo_rs_turret_calculate(&r, target, rs(limits), rs(fallback)));
}
AimAngles advance_aim_angles(AimAngles current, AimAngles desired,
                             const AimLimits &limits, float elapsed) noexcept {
    return cpp(a2fo_rs_turret_advance(rs(current), rs(desired), rs(limits), elapsed));
}
Matrix34 compose_turret_transform(const Matrix34 &m, AimAngles angles) noexcept {
    const auto r = rs(m);
    return cpp(a2fo_rs_turret_compose(&r, rs(angles)));
}
} // namespace a2fo::turrets
