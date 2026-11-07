#include "swarm_motion.hpp"
#include "../../rust/motion_ffi.h"
#include <algorithm>
namespace a2fo::swarm {
namespace {
float random_range_callback(void *context, float minimum, float maximum) noexcept {
    return static_cast<Random *>(context)->range(minimum, maximum);
}
float random_unit_callback(void *context) noexcept {
    return static_cast<Random *>(context)->unit();
}
A2foRsVec3 rs(Vec3 v) noexcept { return {v.x, v.y, v.z}; }
Vec3 cpp(A2foRsVec3 v) noexcept { return {v.x, v.y, v.z}; }
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
} // namespace
Random::Random(std::uint32_t seed) noexcept
    : state_(a2fo_rs_motion_random_seed(seed)) {}
std::uint32_t Random::next_u32() noexcept { return a2fo_rs_motion_next_u32(&state_); }
float Random::unit() noexcept { return a2fo_rs_motion_unit(&state_); }
float Random::range(float minimum, float maximum) noexcept {
    return a2fo_rs_motion_range(&state_, minimum, maximum);
}
float length_squared(Vec3 v) noexcept { return a2fo_rs_motion_length_squared(rs(v)); }
float length(Vec3 v) noexcept { return a2fo_rs_motion_length(rs(v)); }
Vec3 normalized(Vec3 v, Vec3 fallback) noexcept {
    return cpp(a2fo_rs_motion_normalized(rs(v), rs(fallback)));
}
Vec3 add(Vec3 a, Vec3 b) noexcept { return cpp(a2fo_rs_motion_add(rs(a), rs(b))); }
Vec3 subtract(Vec3 a, Vec3 b) noexcept {
    return cpp(a2fo_rs_motion_subtract(rs(a), rs(b)));
}
Vec3 multiply(Vec3 v, float s) noexcept {
    return cpp(a2fo_rs_motion_multiply(rs(v), s));
}
Vec3 random_shell_point(Random &random, float minimum, float maximum) noexcept {
    return cpp(a2fo_rs_motion_shell(&random, random_range_callback,
                                    random_unit_callback, minimum, maximum));
}
Vec3 project_outside_sphere(Vec3 p, Vec3 c, float r, Vec3 f) noexcept {
    return cpp(a2fo_rs_motion_project(rs(p), rs(c), r, rs(f)));
}
Vec3 transform_point(const Matrix34 &m, Vec3 v) noexcept {
    const auto n = rs(m);
    return cpp(a2fo_rs_motion_transform_point(&n, rs(v)));
}
Vec3 transform_direction(const Matrix34 &m, Vec3 v) noexcept {
    const auto n = rs(m);
    return cpp(a2fo_rs_motion_transform_direction(&n, rs(v)));
}
Vec3 inverse_transform_point(const Matrix34 &m, Vec3 v) noexcept {
    const auto n = rs(m);
    return cpp(a2fo_rs_motion_inverse_transform_point(&n, rs(v)));
}
bool advance_agent(Vec3 *p, Vec3 *d, Vec3 t, float s, float tr, float e,
                   float a) noexcept {
    if (!p || !d)
        return true;
    auto rp = rs(*p), rd = rs(*d);
    const bool result = a2fo_rs_motion_advance(&rp, &rd, rs(t), s, tr, e, a) != 0;
    *p = cpp(rp);
    *d = cpp(rd);
    return result;
}
bool constrain_to_radius(Vec3 *p, float r) noexcept {
    if (!p)
        return false;
    auto rp = rs(*p);
    const bool result = a2fo_rs_motion_constrain_radius(&rp, r) != 0;
    *p = cpp(rp);
    return result;
}
bool constrain_motion_outside_sphere(Vec3 prev, Vec3 *p, Vec3 *d, Vec3 c,
                                     float r) noexcept {
    if (!p || !d)
        return false;
    auto rp = rs(*p), rd = rs(*d);
    const bool result =
        a2fo_rs_motion_constrain_sphere(rs(prev), &rp, &rd, rs(c), r) != 0;
    *p = cpp(rp);
    *d = cpp(rd);
    return result;
}
bool separate_pair(Vec3 *lp, Vec3 *ld, Vec3 *rp, Vec3 *rd, float minimum,
                   Vec3 f) noexcept {
    if (!lp || !ld || !rp || !rd)
        return false;
    auto a = rs(*lp), b = rs(*ld), c = rs(*rp), d = rs(*rd);
    const bool result = a2fo_rs_motion_separate(&a, &b, &c, &d, minimum, rs(f)) != 0;
    *lp = cpp(a);
    *ld = cpp(b);
    *rp = cpp(c);
    *rd = cpp(d);
    return result;
}
Matrix34 compose_facing_transform(Vec3 p, Vec3 f, Vec3 u) noexcept {
    return cpp(a2fo_rs_motion_facing(rs(p), rs(f), rs(u)));
}
} // namespace a2fo::swarm
