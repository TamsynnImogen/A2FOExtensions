#include "turret_math.hpp"

#include <cmath>
#include <cstdio>

namespace {

using a2fo::turrets::AimAngles;
using a2fo::turrets::AimLimits;
using a2fo::turrets::Matrix34;

constexpr float kTolerance = 0.001f;

bool near(float actual, float expected) {
    return std::fabs(actual - expected) <= kTolerance;
}

Matrix34 identity_at(float x = 0.0f, float y = 0.0f,
                     float z = 0.0f) {
    Matrix34 matrix{};
    matrix.values[0] = 1.0f;
    matrix.values[4] = 1.0f;
    matrix.values[8] = 1.0f;
    matrix.values[9] = x;
    matrix.values[10] = y;
    matrix.values[11] = z;
    return matrix;
}

bool expect_angles(const char* name, AimAngles actual,
                   float yaw, float pitch) {
    if (near(actual.yaw_degrees, yaw) &&
        near(actual.pitch_degrees, pitch)) {
        return true;
    }
    std::fprintf(stderr,
                 "%s: got yaw %.3f pitch %.3f, expected %.3f %.3f\n",
                 name, actual.yaw_degrees, actual.pitch_degrees,
                 yaw, pitch);
    return false;
}

}  // namespace

int main() {
    const Matrix34 mount = identity_at(10.0f, 20.0f, 30.0f);
    AimLimits limits{};

    const float forward[] = {10.0f, 20.0f, 130.0f};
    if (!expect_angles(
            "forward",
            a2fo::turrets::calculate_aim_angles(
                mount, forward, limits, {12.0f, 7.0f}),
            0.0f, 0.0f)) {
        return 1;
    }

    const float up_and_forward[] = {10.0f, 120.0f, 130.0f};
    if (!expect_angles(
            "up-and-forward",
            a2fo::turrets::calculate_aim_angles(
                mount, up_and_forward, limits, {}),
            0.0f, 45.0f)) {
        return 2;
    }

    limits.yaw_min_degrees = -45.0f;
    limits.yaw_max_degrees = 45.0f;
    const float right[] = {110.0f, 20.0f, 30.0f};
    if (!expect_angles(
            "yaw-clamp",
            a2fo::turrets::calculate_aim_angles(
                mount, right, limits, {}),
            45.0f, 0.0f)) {
        return 3;
    }

    limits.yaw_min_degrees = -180.0f;
    limits.yaw_max_degrees = 180.0f;
    limits.yaw_rate_degrees = 30.0f;
    limits.pitch_rate_degrees = 20.0f;
    if (!expect_angles(
            "slew-rate",
            a2fo::turrets::advance_aim_angles(
                {}, {90.0f, 45.0f}, limits, 0.5f),
            15.0f, 10.0f)) {
        return 4;
    }

    limits.yaw_rate_degrees = 10.0f;
    if (!expect_angles(
            "full-yaw-wrap",
            a2fo::turrets::advance_aim_angles(
                {170.0f, 0.0f}, {-170.0f, 0.0f}, limits, 1.0f),
            180.0f, 0.0f)) {
        return 5;
    }

    const Matrix34 rotated = a2fo::turrets::compose_turret_transform(
        mount, {90.0f, 0.0f});
    if (!near(rotated.values[0], 0.0f) ||
        !near(rotated.values[2], -1.0f) ||
        !near(rotated.values[4], 1.0f) ||
        !near(rotated.values[6], 1.0f) ||
        !near(rotated.values[8], 0.0f) ||
        !near(rotated.values[9], 10.0f) ||
        !near(rotated.values[10], 20.0f) ||
        !near(rotated.values[11], 30.0f)) {
        std::fprintf(stderr, "composed turret transform was incorrect\n");
        return 6;
    }

    std::puts("turret math tests passed");
    return 0;
}
