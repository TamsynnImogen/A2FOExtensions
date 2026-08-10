/*
 * Host-testable geometry contract for A2FOTurrets.
 */

#pragma once

#include <cstddef>

namespace a2fo::turrets {

// Armada's Matrix34 stores three world-space basis vectors followed by its
// translation: right, up, forward, position. Each vector occupies three
// consecutive floats.
struct Matrix34 {
    float values[12]{};
};

struct AimAngles {
    // Owner-local angles: positive yaw is starboard and positive pitch is up.
    float yaw_degrees = 0.0f;
    float pitch_degrees = 0.0f;
};

struct AimLimits {
    // Mechanical travel limits and maximum slew rates in degrees/second.
    float yaw_min_degrees = -180.0f;
    float yaw_max_degrees = 180.0f;
    float pitch_min_degrees = -10.0f;
    float pitch_max_degrees = 85.0f;
    float yaw_rate_degrees = 90.0f;
    float pitch_rate_degrees = 60.0f;
};

float clamp_value(float value, float minimum, float maximum) noexcept;
float normalize_degrees(float value) noexcept;

// Finds the desired owner-local angles, then clamps them to mechanical travel.
AimAngles calculate_aim_angles(
    const Matrix34& mount_transform,
    const float target_position[3],
    const AimLimits& limits,
    AimAngles fallback) noexcept;

// Advances current aim without exceeding the configured per-second rates.
AimAngles advance_aim_angles(
    AimAngles current,
    AimAngles desired,
    const AimLimits& limits,
    float elapsed_seconds) noexcept;

// Rotates the mount basis by the supplied aim while preserving translation.
Matrix34 compose_turret_transform(
    const Matrix34& mount_transform,
    AimAngles angles) noexcept;

}  // namespace a2fo::turrets
