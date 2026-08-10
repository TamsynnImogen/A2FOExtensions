#include "fire_arc.hpp"

#include <cmath>

namespace a2fo::fire_arcs {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegreesToRadians = kPi / 180.0f;
constexpr float kRadiansToDegrees = 180.0f / kPi;
constexpr float kDirectionEpsilon = 0.000001f;
constexpr float kBoundaryEpsilonDegrees = 0.0001f;
constexpr float kBoundaryEpsilonDot = 0.000001f;

float clamp_value(float value, float minimum, float maximum) noexcept {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

float dot_axis(const float direction[3], const Matrix34& matrix,
               int axis) noexcept {
    const int offset = axis * 3;
    return direction[0] * matrix.values[offset] +
        direction[1] * matrix.values[offset + 1] +
        direction[2] * matrix.values[offset + 2];
}

}  // namespace

float normalize_degrees(float value) noexcept {
    if (!std::isfinite(value)) return 0.0f;
    value = std::fmod(value, 360.0f);
    if (value > 180.0f) value -= 360.0f;
    if (value < -180.0f) value += 360.0f;
    return value;
}

void normalize_config(ArcConfig* config) noexcept {
    if (!config) return;
    config->yaw_degrees = normalize_degrees(config->yaw_degrees);
    config->pitch_degrees = clamp_value(
        std::isfinite(config->pitch_degrees)
            ? config->pitch_degrees : 0.0f,
        -90.0f, 90.0f);
    config->yaw_angle_degrees = clamp_value(
        std::isfinite(config->yaw_angle_degrees)
            ? config->yaw_angle_degrees : 360.0f,
        0.0f, 360.0f);
    config->pitch_angle_degrees = clamp_value(
        std::isfinite(config->pitch_angle_degrees)
            ? config->pitch_angle_degrees : 180.0f,
        0.0f, 180.0f);
    config->cone_angle_degrees = clamp_value(
        std::isfinite(config->cone_angle_degrees)
            ? config->cone_angle_degrees : 360.0f,
        0.0f, 360.0f);
}

bool allows_target(
    const ArcConfig& raw_config,
    const Matrix34& owner_transform,
    const float target_position[3]) noexcept {
    if (!target_position) return false;

    ArcConfig config = raw_config;
    normalize_config(&config);
    const float direction[3]{
        target_position[0] - owner_transform.values[9],
        target_position[1] - owner_transform.values[10],
        target_position[2] - owner_transform.values[11],
    };
    const float local_right = dot_axis(direction, owner_transform, 0);
    const float local_up = dot_axis(direction, owner_transform, 1);
    const float local_forward = dot_axis(direction, owner_transform, 2);
    const float length_squared = local_right * local_right +
        local_up * local_up + local_forward * local_forward;
    if (!std::isfinite(length_squared) ||
        length_squared <= kDirectionEpsilon) {
        return false;
    }

    if (config.mode == ArcMode::cone) {
        if (config.cone_angle_degrees >=
            360.0f - kBoundaryEpsilonDegrees) {
            return true;
        }
        const float inverse_length = 1.0f / std::sqrt(length_squared);
        const float yaw = config.yaw_degrees * kDegreesToRadians;
        const float pitch = config.pitch_degrees * kDegreesToRadians;
        const float cosine_pitch = std::cos(pitch);
        const float centre_right = std::sin(yaw) * cosine_pitch;
        const float centre_up = std::sin(pitch);
        const float centre_forward = std::cos(yaw) * cosine_pitch;
        const float target_dot_centre = inverse_length *
            (local_right * centre_right + local_up * centre_up +
             local_forward * centre_forward);
        const float minimum_dot = std::cos(
            config.cone_angle_degrees * 0.5f * kDegreesToRadians);
        return target_dot_centre + kBoundaryEpsilonDot >= minimum_dot;
    }

    const float horizontal = std::sqrt(
        local_right * local_right + local_forward * local_forward);
    const float target_yaw = std::atan2(
        local_right, local_forward) * kRadiansToDegrees;
    const float target_pitch = std::atan2(
        local_up, horizontal) * kRadiansToDegrees;
    const float yaw_difference = std::fabs(normalize_degrees(
        target_yaw - config.yaw_degrees));
    const float pitch_difference = std::fabs(
        target_pitch - config.pitch_degrees);
    const bool yaw_allowed = config.yaw_angle_degrees >=
            360.0f - kBoundaryEpsilonDegrees ||
        yaw_difference <= config.yaw_angle_degrees * 0.5f +
            kBoundaryEpsilonDegrees;
    const bool pitch_allowed = config.pitch_angle_degrees >=
            180.0f - kBoundaryEpsilonDegrees ||
        pitch_difference <= config.pitch_angle_degrees * 0.5f +
            kBoundaryEpsilonDegrees;
    return yaw_allowed && pitch_allowed;
}

}  // namespace a2fo::fire_arcs
