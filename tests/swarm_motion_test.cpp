#include "swarm_motion.hpp"

#include <cmath>
#include <cstdio>

namespace {

constexpr float kTolerance = 0.001f;

bool near(float actual, float expected, float tolerance = kTolerance) {
    return std::fabs(actual - expected) <= tolerance;
}

}  // namespace

int main() {
    using namespace a2fo::swarm;

    Random first(0x12345678u);
    Random second(0x12345678u);
    for (unsigned index = 0; index < 32; ++index) {
        if (first.next_u32() != second.next_u32()) {
            std::fprintf(stderr, "swarm RNG is not deterministic\n");
            return 1;
        }
    }

    Random shell_random(7u);
    for (unsigned index = 0; index < 1000; ++index) {
        const Vec3 point = random_shell_point(shell_random, 10.0f, 50.0f);
        const float radius = length(point);
        if (radius < 9.999f || radius > 50.001f) {
            std::fprintf(stderr, "random point escaped its configured shell\n");
            return 2;
        }
    }

    Matrix34 transform{};
    // 90-degree yaw with non-unit axes also exercises inverse scale handling.
    transform.values[0] = 0.0f;
    transform.values[1] = 0.0f;
    transform.values[2] = -2.0f;
    transform.values[3] = 0.0f;
    transform.values[4] = 3.0f;
    transform.values[5] = 0.0f;
    transform.values[6] = 4.0f;
    transform.values[7] = 0.0f;
    transform.values[8] = 0.0f;
    transform.values[9] = 10.0f;
    transform.values[10] = 20.0f;
    transform.values[11] = 30.0f;
    const Vec3 local{2.0f, 3.0f, 4.0f};
    const Vec3 world = transform_point(transform, local);
    const Vec3 recovered = inverse_transform_point(transform, world);
    if (!near(recovered.x, local.x) || !near(recovered.y, local.y) ||
        !near(recovered.z, local.z)) {
        std::fprintf(stderr, "host-relative transform round trip failed\n");
        return 3;
    }

    Vec3 position{};
    Vec3 direction{1.0f, 0.0f, 0.0f};
    const Vec3 target{0.0f, 0.0f, 10.0f};
    if (advance_agent(&position, &direction, target,
                      5.0f, 1.0f, 0.1f, 0.1f)) {
        std::fprintf(stderr, "agent arrived prematurely\n");
        return 4;
    }
    if (!(position.x > 0.0f && position.z > 0.0f && direction.z > 0.0f)) {
        std::fprintf(stderr, "agent did not turn smoothly toward its target\n");
        return 5;
    }
    for (unsigned index = 0; index < 200; ++index) {
        if (advance_agent(&position, &direction, target,
                          5.0f, 4.0f, 0.1f, 0.1f)) break;
    }
    if (!near(position.x, target.x) || !near(position.y, target.y) ||
        !near(position.z, target.z)) {
        std::fprintf(stderr, "agent did not clamp to its destination\n");
        return 6;
    }

    Vec3 escaped{30.0f, 40.0f, 0.0f};
    if (!constrain_to_radius(&escaped, 10.0f) ||
        !near(length(escaped), 10.0f)) {
        std::fprintf(stderr, "roaming radius constraint failed\n");
        return 7;
    }
    if (constrain_to_radius(&escaped, 10.0f)) {
        std::fprintf(stderr, "in-bounds roaming position was changed\n");
        return 8;
    }

    const Vec3 sphere_centre{3.0f, 4.0f, 5.0f};
    const Vec3 projected = project_outside_sphere(
        sphere_centre, sphere_centre, 10.0f, {1.0f, 0.0f, 0.0f});
    if (!near(projected.x, 13.0f) || !near(projected.y, 4.0f) ||
        !near(projected.z, 5.0f)) {
        std::fprintf(stderr, "host exclusion projection failed\n");
        return 9;
    }

    Vec3 blocked_position{8.0f, 0.0f, 0.0f};
    Vec3 blocked_direction{-1.0f, 0.0f, 0.0f};
    if (!constrain_motion_outside_sphere(
            {12.0f, 0.0f, 0.0f}, &blocked_position,
            &blocked_direction, {}, 10.0f) ||
        !near(length(blocked_position), 10.0f) ||
        blocked_direction.x < -kTolerance) {
        std::fprintf(stderr, "host exclusion contact did not slide\n");
        return 10;
    }

    Vec3 tunnel_position{-12.0f, 0.0f, 0.0f};
    Vec3 tunnel_direction{-1.0f, 0.0f, 0.0f};
    if (!constrain_motion_outside_sphere(
            {12.0f, 0.0f, 0.0f}, &tunnel_position,
            &tunnel_direction, {}, 10.0f) ||
        !near(tunnel_position.x, 10.0f)) {
        std::fprintf(stderr, "fast swarm step tunnelled through host\n");
        return 11;
    }

    Vec3 sliding_position{9.0f, 2.0f, 0.0f};
    Vec3 sliding_direction{-0.5f, 1.0f, 0.0f};
    if (!constrain_motion_outside_sphere(
            {10.0f, 0.0f, 0.0f}, &sliding_position,
            &sliding_direction, {}, 10.0f) ||
        sliding_position.y <= 0.0f || length(sliding_position) < 9.999f) {
        std::fprintf(stderr, "swarm contact did not advance tangentially\n");
        return 12;
    }

    Vec3 orbit_position{12.0f, 0.0f, 0.0f};
    Vec3 orbit_direction{-1.0f, 0.0f, 0.0f};
    const Vec3 far_side_target{-12.0f, 0.0f, 0.0f};
    bool reached_far_side = false;
    for (unsigned index = 0; index < 1000; ++index) {
        const Vec3 previous = orbit_position;
        reached_far_side = advance_agent(
            &orbit_position, &orbit_direction, far_side_target,
            5.0f, 4.0f, 0.1f, 0.1f);
        if (constrain_motion_outside_sphere(
                previous, &orbit_position, &orbit_direction,
                {}, 10.0f)) {
            reached_far_side = false;
        }
        if (length(orbit_position) < 9.999f) {
            std::fprintf(stderr, "orbiting swarm entered host volume\n");
            return 13;
        }
        if (reached_far_side) break;
    }
    if (!reached_far_side) {
        std::fprintf(stderr, "swarm could not route around host volume\n");
        return 14;
    }

    Vec3 left_position{};
    Vec3 right_position{};
    Vec3 left_direction{0.0f, 0.0f, 1.0f};
    Vec3 right_direction{0.0f, 0.0f, 1.0f};
    if (!separate_pair(
            &left_position, &left_direction,
            &right_position, &right_direction,
            12.0f, {1.0f, 0.0f, 0.0f}) ||
        !near(length(subtract(left_position, right_position)), 12.0f) ||
        left_direction.x <= 0.0f || right_direction.x >= 0.0f) {
        std::fprintf(stderr, "overlapping swarm members did not separate\n");
        return 15;
    }
    if (separate_pair(
            &left_position, &left_direction,
            &right_position, &right_direction,
            12.0f, {1.0f, 0.0f, 0.0f})) {
        std::fprintf(stderr, "separated swarm members were changed again\n");
        return 16;
    }

    const Matrix34 facing = compose_facing_transform(
        {1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f});
    if (!near(facing.values[0], 1.0f) ||
        !near(facing.values[4], 1.0f) ||
        !near(facing.values[8], 1.0f) ||
        !near(facing.values[9], 1.0f) ||
        !near(facing.values[10], 2.0f) ||
        !near(facing.values[11], 3.0f)) {
        std::fprintf(stderr, "facing transform basis was incorrect\n");
        return 17;
    }

    std::puts("swarm movement and transform tests passed");
    return 0;
}
