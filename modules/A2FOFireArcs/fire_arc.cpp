/*
 * Pure owner-local fire-volume mathematics. This translation unit contains no
 * engine calls so every angle convention and boundary can be unit tested.
 */

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
        // Build a unit vector for the configured centre, then compare its dot
        // product with the target direction against cos(half-angle). This is
        // a true circular spherical cap, not independent yaw/pitch limits.
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

    // Box mode converts the owner-local direction back into yaw and pitch so
    // the two total widths can be tested independently. Yaw is undefined at
    // the vertical poles. Trigonometric rounding can leave a tiny horizontal
    // component there, so deliberately use the configured centre yaw instead
    // of allowing that noise to select an arbitrary (often opposite) yaw.
    const float horizontal = std::sqrt(
        local_right * local_right + local_forward * local_forward);
    const float direction_length = std::sqrt(length_squared);
    const float target_yaw = horizontal <=
            direction_length * kDirectionEpsilon
        ? config.yaw_degrees
        : std::atan2(local_right, local_forward) * kRadiansToDegrees;
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
    // Pitch itself occupies only -90..90 degrees. A 180-degree width centred
    // at zero therefore covers that complete domain naturally, while the same
    // width centred at +90 or -90 covers only the upper or lower hemisphere.
    // Treating every 180-degree width as unconditional discarded its centre.
    const bool pitch_allowed =
        pitch_difference <= config.pitch_angle_degrees * 0.5f +
            kBoundaryEpsilonDegrees;
    return yaw_allowed && pitch_allowed;
}

}  // namespace a2fo::fire_arcs
