/*
 * Pure owner-local fire-volume mathematics. This translation unit contains no
 * engine calls so every angle convention and boundary can be unit tested.
 */

#include "fire_arc.hpp"

#include <algorithm>
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

void direction_for(float yaw_degrees, float pitch_degrees,
                   float output[3]) noexcept {
    const float yaw = yaw_degrees * kDegreesToRadians;
    const float pitch = pitch_degrees * kDegreesToRadians;
    const float cosine_pitch = std::cos(pitch);
    output[0] = std::sin(yaw) * cosine_pitch;
    output[1] = std::sin(pitch);
    output[2] = std::cos(yaw) * cosine_pitch;
}

Vector3 world_point(const Matrix34& matrix, const float origin[3],
                    const float local_direction[3],
                    float radius) noexcept {
    Vector3 result{};
    for (int component = 0; component < 3; ++component) {
        result.values[component] = origin[component] + radius *
            (local_direction[0] * matrix.values[component] +
             local_direction[1] * matrix.values[3 + component] +
             local_direction[2] * matrix.values[6 + component]);
    }
    return result;
}

bool append_line(ArcLine* output, std::size_t capacity,
                 std::size_t* count, const Vector3& start,
                 const Vector3& end, ArcLineStyle style) noexcept {
    if (!output || !count || *count >= capacity) return false;
    output[*count] = ArcLine{start, end, style};
    ++*count;
    return true;
}

void append_direction_line(
    const Matrix34& matrix, const float origin[3], float radius,
    const float start_direction[3], const float end_direction[3],
    ArcLineStyle style, ArcLine* output, std::size_t capacity,
    std::size_t* count) noexcept {
    append_line(output, capacity, count,
                world_point(matrix, origin, start_direction, radius),
                world_point(matrix, origin, end_direction, radius), style);
}

void append_radial_line(
    const Matrix34& matrix, const float origin[3], float radius,
    const float direction[3], ArcLineStyle style, ArcLine* output,
    std::size_t capacity, std::size_t* count) noexcept {
    const Vector3 start{{origin[0], origin[1], origin[2]}};
    append_line(output, capacity, count, start,
                world_point(matrix, origin, direction, radius), style);
}

void append_sphere(
    const Matrix34& matrix, const float origin[3], float radius,
    ArcLine* output, std::size_t capacity, std::size_t* count) noexcept {
    constexpr int kSegments = 32;
    for (int plane = 0; plane < 3; ++plane) {
        float previous[3]{};
        for (int segment = 0; segment <= kSegments; ++segment) {
            const float angle = static_cast<float>(segment) *
                2.0f * kPi / static_cast<float>(kSegments);
            float current[3]{};
            if (plane == 0) {
                current[0] = std::sin(angle);
                current[2] = std::cos(angle);
            } else if (plane == 1) {
                current[1] = std::sin(angle);
                current[2] = std::cos(angle);
            } else {
                current[0] = std::sin(angle);
                current[1] = std::cos(angle);
            }
            if (segment != 0) {
                append_direction_line(
                    matrix, origin, radius, previous, current,
                    ArcLineStyle::boundary, output, capacity, count);
            }
            std::copy(current, current + 3, previous);
        }
    }
}

void cross_product(const float left[3], const float right[3],
                   float output[3]) noexcept {
    output[0] = left[1] * right[2] - left[2] * right[1];
    output[1] = left[2] * right[0] - left[0] * right[2];
    output[2] = left[0] * right[1] - left[1] * right[0];
}

void normalize_vector(float value[3]) noexcept {
    const float length = std::sqrt(
        value[0] * value[0] + value[1] * value[1] +
        value[2] * value[2]);
    if (length <= kDirectionEpsilon) return;
    value[0] /= length;
    value[1] /= length;
    value[2] /= length;
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

std::size_t build_visualization_lines(
    const ArcConfig& raw_config,
    const Matrix34& owner_transform,
    const float origin[3],
    float radius,
    ArcLine* output,
    std::size_t capacity) noexcept {
    if (!origin || !output || capacity == 0 || !std::isfinite(radius) ||
        radius <= 0.0f) {
        return 0;
    }

    ArcConfig config = raw_config;
    normalize_config(&config);
    std::size_t count = 0;
    float centre[3]{};
    direction_for(config.yaw_degrees, config.pitch_degrees, centre);

    if (config.mode == ArcMode::cone) {
        if (config.cone_angle_degrees >=
            360.0f - kBoundaryEpsilonDegrees) {
            append_sphere(owner_transform, origin, radius,
                          output, capacity, &count);
        } else if (config.cone_angle_degrees >
                   kBoundaryEpsilonDegrees) {
            const float half_angle = config.cone_angle_degrees * 0.5f *
                kDegreesToRadians;
            const float cosine_half = std::cos(half_angle);
            const float sine_half = std::sin(half_angle);
            const float reference[3]{
                std::fabs(centre[1]) > 0.95f ? 1.0f : 0.0f,
                std::fabs(centre[1]) > 0.95f ? 0.0f : 1.0f,
                0.0f,
            };
            float tangent_a[3]{};
            float tangent_b[3]{};
            cross_product(reference, centre, tangent_a);
            normalize_vector(tangent_a);
            cross_product(centre, tangent_a, tangent_b);
            normalize_vector(tangent_b);

            constexpr int kSegments = 32;
            float previous[3]{};
            for (int segment = 0; segment <= kSegments; ++segment) {
                const float around = static_cast<float>(segment) *
                    2.0f * kPi / static_cast<float>(kSegments);
                float current[3]{};
                for (int component = 0; component < 3; ++component) {
                    current[component] = centre[component] * cosine_half +
                        (tangent_a[component] * std::cos(around) +
                         tangent_b[component] * std::sin(around)) * sine_half;
                }
                if (segment != 0) {
                    append_direction_line(
                        owner_transform, origin, radius, previous, current,
                        ArcLineStyle::boundary, output, capacity, &count);
                }
                if (segment < kSegments && segment % 8 == 0) {
                    append_radial_line(
                        owner_transform, origin, radius, current,
                        ArcLineStyle::boundary, output, capacity, &count);
                }
                std::copy(current, current + 3, previous);
            }
        }
    } else {
        const float half_yaw = config.yaw_angle_degrees * 0.5f;
        const float half_pitch = config.pitch_angle_degrees * 0.5f;
        const float minimum_yaw = config.yaw_degrees - half_yaw;
        const float maximum_yaw = config.yaw_degrees + half_yaw;
        const float minimum_pitch = clamp_value(
            config.pitch_degrees - half_pitch, -90.0f, 90.0f);
        const float maximum_pitch = clamp_value(
            config.pitch_degrees + half_pitch, -90.0f, 90.0f);
        const bool full_yaw = config.yaw_angle_degrees >=
            360.0f - kBoundaryEpsilonDegrees;
        const bool full_pitch = minimum_pitch <=
                -90.0f + kBoundaryEpsilonDegrees &&
            maximum_pitch >= 90.0f - kBoundaryEpsilonDegrees;

        if (full_yaw && full_pitch) {
            append_sphere(owner_transform, origin, radius,
                          output, capacity, &count);
        } else {
            constexpr int kSegments = 24;
            // Constant-pitch spherical edges. With unrestricted yaw these are
            // complete latitude rings; otherwise they span only the box.
            for (int edge = 0; edge < 2; ++edge) {
                const float pitch = edge == 0
                    ? minimum_pitch : maximum_pitch;
                float previous[3]{};
                for (int segment = 0; segment <= kSegments; ++segment) {
                    const float fraction = static_cast<float>(segment) /
                        static_cast<float>(kSegments);
                    const float yaw = full_yaw
                        ? config.yaw_degrees + fraction * 360.0f
                        : minimum_yaw + fraction *
                              config.yaw_angle_degrees;
                    float current[3]{};
                    direction_for(yaw, pitch, current);
                    if (segment != 0) {
                        append_direction_line(
                            owner_transform, origin, radius,
                            previous, current, ArcLineStyle::boundary,
                            output, capacity, &count);
                    }
                    std::copy(current, current + 3, previous);
                }
            }

            // Restricted-yaw boxes also need the two constant-yaw sides and
            // four origin rays to make their angular volume legible.
            if (!full_yaw) {
                constexpr int kSideSegments = 12;
                for (int edge = 0; edge < 2; ++edge) {
                    const float yaw = edge == 0
                        ? minimum_yaw : maximum_yaw;
                    float previous[3]{};
                    for (int segment = 0;
                         segment <= kSideSegments; ++segment) {
                        const float fraction = static_cast<float>(segment) /
                            static_cast<float>(kSideSegments);
                        const float pitch = minimum_pitch + fraction *
                            (maximum_pitch - minimum_pitch);
                        float current[3]{};
                        direction_for(yaw, pitch, current);
                        if (segment != 0) {
                            append_direction_line(
                                owner_transform, origin, radius,
                                previous, current, ArcLineStyle::boundary,
                                output, capacity, &count);
                        }
                        std::copy(current, current + 3, previous);
                    }
                }
                for (float yaw : {minimum_yaw, maximum_yaw}) {
                    for (float pitch : {minimum_pitch, maximum_pitch}) {
                        float corner[3]{};
                        direction_for(yaw, pitch, corner);
                        append_radial_line(
                            owner_transform, origin, radius, corner,
                            ArcLineStyle::boundary, output, capacity, &count);
                    }
                }
            }
        }
    }

    append_radial_line(owner_transform, origin, radius, centre,
                       ArcLineStyle::centre, output, capacity, &count);
    return count;
}

}  // namespace a2fo::fire_arcs
