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
    float yaw_degrees = 0.0f;
    float pitch_degrees = 0.0f;
};

struct AimLimits {
    float yaw_min_degrees = -180.0f;
    float yaw_max_degrees = 180.0f;
    float pitch_min_degrees = -10.0f;
    float pitch_max_degrees = 85.0f;
    float yaw_rate_degrees = 90.0f;
    float pitch_rate_degrees = 60.0f;
};

float clamp_value(float value, float minimum, float maximum) noexcept;
float normalize_degrees(float value) noexcept;

AimAngles calculate_aim_angles(
    const Matrix34& mount_transform,
    const float target_position[3],
    const AimLimits& limits,
    AimAngles fallback) noexcept;

AimAngles advance_aim_angles(
    AimAngles current,
    AimAngles desired,
    const AimLimits& limits,
    float elapsed_seconds) noexcept;

Matrix34 compose_turret_transform(
    const Matrix34& mount_transform,
    AimAngles angles) noexcept;

}  // namespace a2fo::turrets
