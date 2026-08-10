#include "turret_math.hpp"

#include <cmath>

namespace a2fo::turrets {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kRadiansToDegrees = 180.0f / kPi;
constexpr float kDegreesToRadians = kPi / 180.0f;

float dot_axis(const float direction[3], const Matrix34& matrix,
               std::size_t axis) noexcept {
    const std::size_t offset = axis * 3;
    return direction[0] * matrix.values[offset] +
        direction[1] * matrix.values[offset + 1] +
        direction[2] * matrix.values[offset + 2];
}

void combine_axis(float destination[3], const Matrix34& matrix,
                  float right, float up, float forward) noexcept {
    for (std::size_t component = 0; component < 3; ++component) {
        destination[component] =
            right * matrix.values[component] +
            up * matrix.values[3 + component] +
            forward * matrix.values[6 + component];
    }
}

float advance_axis(float current, float desired, float rate,
                   float elapsed_seconds, bool wraps) noexcept {
    float difference = desired - current;
    if (wraps) difference = normalize_degrees(difference);
    const float maximum_step = std::fmax(0.0f, rate) * elapsed_seconds;
    if (difference > maximum_step) difference = maximum_step;
    if (difference < -maximum_step) difference = -maximum_step;
    const float result = current + difference;
    return wraps ? normalize_degrees(result) : result;
}

}  // namespace

float clamp_value(float value, float minimum, float maximum) noexcept {
    if (minimum > maximum) {
        const float swap = minimum;
        minimum = maximum;
        maximum = swap;
    }
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

float normalize_degrees(float value) noexcept {
    while (value > 180.0f) value -= 360.0f;
    while (value < -180.0f) value += 360.0f;
    return value;
}

AimAngles calculate_aim_angles(
    const Matrix34& mount_transform,
    const float target_position[3],
    const AimLimits& limits,
    AimAngles fallback) noexcept {
    if (!target_position) return fallback;

    const float direction[3]{
        target_position[0] - mount_transform.values[9],
        target_position[1] - mount_transform.values[10],
        target_position[2] - mount_transform.values[11],
    };
    const float local_x = dot_axis(direction, mount_transform, 0);
    const float local_y = dot_axis(direction, mount_transform, 1);
    const float local_z = dot_axis(direction, mount_transform, 2);
    const float horizontal = std::sqrt(
        local_x * local_x + local_z * local_z);
    if (horizontal < 0.0001f && std::fabs(local_y) < 0.0001f) {
        return fallback;
    }

    AimAngles desired{};
    desired.yaw_degrees = std::atan2(local_x, local_z) *
        kRadiansToDegrees;
    desired.pitch_degrees = std::atan2(local_y, horizontal) *
        kRadiansToDegrees;
    desired.yaw_degrees = clamp_value(
        desired.yaw_degrees, limits.yaw_min_degrees,
        limits.yaw_max_degrees);
    desired.pitch_degrees = clamp_value(
        desired.pitch_degrees, limits.pitch_min_degrees,
        limits.pitch_max_degrees);
    return desired;
}

AimAngles advance_aim_angles(
    AimAngles current,
    AimAngles desired,
    const AimLimits& limits,
    float elapsed_seconds) noexcept {
    elapsed_seconds = clamp_value(elapsed_seconds, 0.0f, 1.0f);
    const bool yaw_wraps =
        limits.yaw_max_degrees - limits.yaw_min_degrees >= 359.0f;
    current.yaw_degrees = advance_axis(
        current.yaw_degrees, desired.yaw_degrees,
        limits.yaw_rate_degrees, elapsed_seconds, yaw_wraps);
    current.pitch_degrees = advance_axis(
        current.pitch_degrees, desired.pitch_degrees,
        limits.pitch_rate_degrees, elapsed_seconds, false);
    current.yaw_degrees = clamp_value(
        current.yaw_degrees, limits.yaw_min_degrees,
        limits.yaw_max_degrees);
    current.pitch_degrees = clamp_value(
        current.pitch_degrees, limits.pitch_min_degrees,
        limits.pitch_max_degrees);
    return current;
}

Matrix34 compose_turret_transform(
    const Matrix34& mount_transform,
    AimAngles angles) noexcept {
    const float yaw = angles.yaw_degrees * kDegreesToRadians;
    const float pitch = angles.pitch_degrees * kDegreesToRadians;
    const float sine_yaw = std::sin(yaw);
    const float cosine_yaw = std::cos(yaw);
    const float sine_pitch = std::sin(pitch);
    const float cosine_pitch = std::cos(pitch);

    Matrix34 result{};
    combine_axis(result.values, mount_transform,
                 cosine_yaw, 0.0f, -sine_yaw);
    combine_axis(result.values + 3, mount_transform,
                 -sine_yaw * sine_pitch, cosine_pitch,
                 -cosine_yaw * sine_pitch);
    combine_axis(result.values + 6, mount_transform,
                 sine_yaw * cosine_pitch, sine_pitch,
                 cosine_yaw * cosine_pitch);
    result.values[9] = mount_transform.values[9];
    result.values[10] = mount_transform.values[10];
    result.values[11] = mount_transform.values[11];
    return result;
}

}  // namespace a2fo::turrets
