/*
 * Lightweight host-relative swarm movement mathematics.
 */

#include "swarm_motion.hpp"

#include <algorithm>
#include <cmath>

namespace a2fo::swarm {
namespace {

constexpr float kEpsilon = 0.000001f;

float dot(Vec3 left, Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3 cross(Vec3 left, Vec3 right) noexcept {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

float clamp(float value, float minimum, float maximum) noexcept {
    return std::max(minimum, std::min(value, maximum));
}

}  // namespace

Random::Random(std::uint32_t seed) noexcept
    : state_(seed == 0 ? 0x6d2b79f5u : seed) {}

std::uint32_t Random::next_u32() noexcept {
    std::uint32_t value = state_;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    state_ = value == 0 ? 0x6d2b79f5u : value;
    return state_;
}

float Random::unit() noexcept {
    // Use the high 24 bits, which are exactly representable by float.
    return static_cast<float>(next_u32() >> 8) * (1.0f / 16777216.0f);
}

float Random::range(float minimum, float maximum) noexcept {
    if (minimum > maximum) std::swap(minimum, maximum);
    return minimum + (maximum - minimum) * unit();
}

float length_squared(Vec3 value) noexcept {
    return dot(value, value);
}

float length(Vec3 value) noexcept {
    return std::sqrt(length_squared(value));
}

Vec3 normalized(Vec3 value, Vec3 fallback) noexcept {
    const float magnitude_squared = length_squared(value);
    if (!std::isfinite(magnitude_squared) || magnitude_squared <= kEpsilon) {
        const float fallback_squared = length_squared(fallback);
        if (!std::isfinite(fallback_squared) || fallback_squared <= kEpsilon) {
            return {0.0f, 0.0f, 1.0f};
        }
        return multiply(fallback, 1.0f / std::sqrt(fallback_squared));
    }
    return multiply(value, 1.0f / std::sqrt(magnitude_squared));
}

Vec3 add(Vec3 left, Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vec3 multiply(Vec3 value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vec3 random_shell_point(Random& random, float minimum_radius,
                        float maximum_radius) noexcept {
    minimum_radius = std::max(0.0f, minimum_radius);
    maximum_radius = std::max(minimum_radius, maximum_radius);

    // Marsaglia's sphere method avoids directional bias. Bound the retry loop
    // so even a pathological deterministic stream cannot stall the game.
    Vec3 direction{0.0f, 0.0f, 1.0f};
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        const float x = random.range(-1.0f, 1.0f);
        const float y = random.range(-1.0f, 1.0f);
        const float radius_squared = x * x + y * y;
        if (radius_squared <= kEpsilon || radius_squared >= 1.0f) continue;
        const float factor = 2.0f * std::sqrt(1.0f - radius_squared);
        direction = {x * factor, y * factor, 1.0f - 2.0f * radius_squared};
        break;
    }

    const float minimum_cubed = minimum_radius * minimum_radius * minimum_radius;
    const float maximum_cubed = maximum_radius * maximum_radius * maximum_radius;
    const float radius = std::cbrt(
        minimum_cubed + (maximum_cubed - minimum_cubed) * random.unit());
    return multiply(direction, radius);
}

Vec3 project_outside_sphere(Vec3 point, Vec3 centre, float radius,
                            Vec3 fallback) noexcept {
    radius = std::max(0.0f, radius);
    if (radius <= kEpsilon) return point;
    const Vec3 relative = subtract(point, centre);
    const float distance_squared = length_squared(relative);
    const float radius_squared = radius * radius;
    if (std::isfinite(distance_squared) &&
        distance_squared >= radius_squared) {
        return point;
    }
    return add(centre, multiply(normalized(relative, fallback), radius));
}

Vec3 transform_point(const Matrix34& transform, Vec3 local) noexcept {
    const Vec3 translated = transform_direction(transform, local);
    return {
        translated.x + transform.values[9],
        translated.y + transform.values[10],
        translated.z + transform.values[11],
    };
}

Vec3 transform_direction(const Matrix34& transform, Vec3 local) noexcept {
    return {
        local.x * transform.values[0] + local.y * transform.values[3] +
            local.z * transform.values[6],
        local.x * transform.values[1] + local.y * transform.values[4] +
            local.z * transform.values[7],
        local.x * transform.values[2] + local.y * transform.values[5] +
            local.z * transform.values[8],
    };
}

Vec3 inverse_transform_point(const Matrix34& transform, Vec3 world) noexcept {
    const Vec3 relative{
        world.x - transform.values[9],
        world.y - transform.values[10],
        world.z - transform.values[11],
    };
    const Vec3 right{transform.values[0], transform.values[1],
                     transform.values[2]};
    const Vec3 up{transform.values[3], transform.values[4],
                  transform.values[5]};
    const Vec3 forward{transform.values[6], transform.values[7],
                       transform.values[8]};
    const float right_squared = std::max(kEpsilon, length_squared(right));
    const float up_squared = std::max(kEpsilon, length_squared(up));
    const float forward_squared = std::max(kEpsilon, length_squared(forward));
    return {
        dot(relative, right) / right_squared,
        dot(relative, up) / up_squared,
        dot(relative, forward) / forward_squared,
    };
}

bool advance_agent(Vec3* position, Vec3* direction, Vec3 target,
                   float speed, float turn_rate,
                   float elapsed_seconds, float arrival_radius) noexcept {
    if (!position || !direction) return true;
    elapsed_seconds = clamp(elapsed_seconds, 0.0f, 0.25f);
    speed = std::max(0.0f, speed);
    turn_rate = std::max(0.0f, turn_rate);
    arrival_radius = std::max(0.001f, arrival_radius);

    const Vec3 to_target = subtract(target, *position);
    const float distance = length(to_target);
    if (!std::isfinite(distance) || distance <= arrival_radius) {
        *position = target;
        return true;
    }

    const Vec3 desired = multiply(to_target, 1.0f / distance);
    const float blend = clamp(turn_rate * elapsed_seconds, 0.0f, 1.0f);
    *direction = normalized(
        add(multiply(*direction, 1.0f - blend), multiply(desired, blend)),
        desired);
    const float step = speed * elapsed_seconds;
    if (step + arrival_radius >= distance) {
        *position = target;
        *direction = desired;
        return true;
    }
    *position = add(*position, multiply(*direction, step));
    return false;
}

bool constrain_to_radius(Vec3* position, float maximum_radius) noexcept {
    if (!position) return false;
    maximum_radius = std::max(0.0f, maximum_radius);
    const float radius_squared = length_squared(*position);
    const float maximum_squared = maximum_radius * maximum_radius;
    if (std::isfinite(radius_squared) && radius_squared <= maximum_squared) {
        return false;
    }
    if (!std::isfinite(radius_squared) || radius_squared <= kEpsilon ||
        maximum_radius <= kEpsilon) {
        *position = {};
        return true;
    }
    *position = multiply(*position,
                         maximum_radius / std::sqrt(radius_squared));
    return true;
}

bool constrain_motion_outside_sphere(
        Vec3 previous_position, Vec3* position, Vec3* direction,
        Vec3 centre, float radius) noexcept {
    if (!position || !direction) return false;
    radius = std::max(0.0f, radius);
    if (radius <= kEpsilon) return false;

    const Vec3 start = subtract(previous_position, centre);
    const Vec3 end = subtract(*position, centre);
    const Vec3 step = subtract(end, start);
    const float start_squared = length_squared(start);
    const float end_squared = length_squared(end);
    const float radius_squared = radius * radius;
    bool contact = !std::isfinite(end_squared) ||
                   end_squared < radius_squared;
    float contact_fraction = 1.0f;

    // Also catch a fast step whose endpoint has already emerged on the far
    // side. This is uncommon at normal swarm speeds, but keeps extreme ODF
    // speed values from tunnelling through a small host.
    const float step_squared = length_squared(step);
    if (std::isfinite(start_squared) && start_squared >= radius_squared &&
        std::isfinite(step_squared) && step_squared > kEpsilon) {
        const float b = dot(start, step);
        const float c = start_squared - radius_squared;
        const float discriminant = b * b - step_squared * c;
        if (b < 0.0f && discriminant >= 0.0f) {
            const float entry = (-b - std::sqrt(discriminant)) / step_squared;
            if (entry >= 0.0f && entry <= 1.0f) {
                contact = true;
                contact_fraction = entry;
            }
        }
    }

    if (!contact) return false;
    Vec3 contact_point = *position;
    if (contact_fraction < 1.0f) {
        const Vec3 entry_point = add(
            previous_position, multiply(step, contact_fraction));
        const Vec3 entry_radial = normalized(
            subtract(entry_point, centre), start);
        const Vec3 remaining_step = multiply(
            step, 1.0f - contact_fraction);
        const Vec3 tangential_step = subtract(
            remaining_step,
            multiply(entry_radial, dot(remaining_step, entry_radial)));
        // Preserve the tangential part of this frame's movement. Merely
        // clamping at the entry point would pin an agent whose target lies on
        // the far side of the host; carrying the tangent makes it slide.
        contact_point = add(entry_point, tangential_step);
    }
    contact_point = project_outside_sphere(
        contact_point, centre, radius, start);
    *position = contact_point;

    const Vec3 radial = normalized(subtract(contact_point, centre), start);
    const float inward = dot(*direction, radial);
    Vec3 tangent = *direction;
    if (inward < 0.0f) {
        tangent = subtract(tangent, multiply(radial, inward));
    }
    if (length_squared(tangent) <= kEpsilon) {
        // Choose the least-aligned cardinal axis for a stable, non-degenerate
        // tangent when the member was travelling exactly toward the centre.
        Vec3 axis = std::fabs(radial.x) < std::fabs(radial.y)
            ? Vec3{1.0f, 0.0f, 0.0f} : Vec3{0.0f, 1.0f, 0.0f};
        if (std::fabs(radial.z) < std::fabs(dot(radial, axis))) {
            axis = {0.0f, 0.0f, 1.0f};
        }
        tangent = cross(axis, radial);
    }
    *direction = normalized(tangent);
    return true;
}

bool separate_pair(Vec3* left_position, Vec3* left_direction,
                   Vec3* right_position, Vec3* right_direction,
                   float minimum_distance, Vec3 fallback) noexcept {
    if (!left_position || !left_direction ||
        !right_position || !right_direction) return false;
    minimum_distance = std::max(0.0f, minimum_distance);
    if (minimum_distance <= kEpsilon) return false;

    const Vec3 delta = subtract(*left_position, *right_position);
    const float distance_squared = length_squared(delta);
    const float minimum_squared = minimum_distance * minimum_distance;
    if (std::isfinite(distance_squared) &&
        distance_squared >= minimum_squared) return false;

    const float distance = std::isfinite(distance_squared) &&
                           distance_squared > kEpsilon
        ? std::sqrt(distance_squared) : 0.0f;
    const Vec3 normal = normalized(delta, fallback);
    const float correction = (minimum_distance - distance) * 0.5f;
    *left_position = add(*left_position, multiply(normal, correction));
    *right_position = subtract(*right_position, multiply(normal, correction));

    // A modest heading bias stops the normal movement update from immediately
    // undoing the positional correction, while retaining the authored target.
    constexpr float kSteeringBias = 0.35f;
    *left_direction = normalized(
        add(*left_direction, multiply(normal, kSteeringBias)),
        *left_direction);
    *right_direction = normalized(
        subtract(*right_direction, multiply(normal, kSteeringBias)),
        *right_direction);
    return true;
}

Matrix34 compose_facing_transform(Vec3 position, Vec3 forward,
                                  Vec3 preferred_up) noexcept {
    forward = normalized(forward);
    preferred_up = normalized(preferred_up, {0.0f, 1.0f, 0.0f});
    Vec3 right = cross(preferred_up, forward);
    if (length_squared(right) <= kEpsilon) {
        right = cross({1.0f, 0.0f, 0.0f}, forward);
    }
    right = normalized(right, {1.0f, 0.0f, 0.0f});
    const Vec3 up = normalized(cross(forward, right), preferred_up);

    Matrix34 result{};
    result.values[0] = right.x;
    result.values[1] = right.y;
    result.values[2] = right.z;
    result.values[3] = up.x;
    result.values[4] = up.y;
    result.values[5] = up.z;
    result.values[6] = forward.x;
    result.values[7] = forward.y;
    result.values[8] = forward.z;
    result.values[9] = position.x;
    result.values[10] = position.y;
    result.values[11] = position.z;
    return result;
}

}  // namespace a2fo::swarm
