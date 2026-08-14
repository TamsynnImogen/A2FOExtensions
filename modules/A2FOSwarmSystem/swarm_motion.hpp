/*
 * Host-testable movement and transform helpers for A2FOSwarmSystem.
 */

#pragma once

#include <cstdint>

namespace a2fo::swarm {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Armada stores right, up, forward, then position as four Vec3 values.
struct Matrix34 {
    float values[12]{};
};

class Random {
public:
    explicit Random(std::uint32_t seed) noexcept;

    std::uint32_t next_u32() noexcept;
    float unit() noexcept;
    float range(float minimum, float maximum) noexcept;

private:
    std::uint32_t state_;
};

float length_squared(Vec3 value) noexcept;
float length(Vec3 value) noexcept;
Vec3 normalized(Vec3 value, Vec3 fallback = {0.0f, 0.0f, 1.0f}) noexcept;
Vec3 add(Vec3 left, Vec3 right) noexcept;
Vec3 subtract(Vec3 left, Vec3 right) noexcept;
Vec3 multiply(Vec3 value, float scale) noexcept;

Vec3 random_shell_point(Random& random, float minimum_radius,
                        float maximum_radius) noexcept;

// Projects a point to the nearest safe position on or outside an exclusion
// sphere. The fallback supplies a stable outward direction for a point that
// exactly matches the sphere centre.
Vec3 project_outside_sphere(Vec3 point, Vec3 centre, float radius,
                            Vec3 fallback = {0.0f, 0.0f, 1.0f}) noexcept;

Vec3 transform_point(const Matrix34& transform, Vec3 local) noexcept;
Vec3 transform_direction(const Matrix34& transform, Vec3 local) noexcept;
Vec3 inverse_transform_point(const Matrix34& transform, Vec3 world) noexcept;

// Smoothly turns and advances one visual agent. Returns true when the target
// was reached this step; overshoot is clamped to the target.
bool advance_agent(Vec3* position, Vec3* direction, Vec3 target,
                   float speed, float turn_rate,
                   float elapsed_seconds, float arrival_radius) noexcept;

// Keeps curved roaming routes within their configured host-local boundary.
// Returns true when the position had to be projected back onto the sphere.
bool constrain_to_radius(Vec3* position, float maximum_radius) noexcept;

// Prevents a completed movement step from entering or tunnelling through an
// exclusion sphere. On contact, the direction is made tangential so following
// steps slide naturally around the boundary. Returns true on contact.
bool constrain_motion_outside_sphere(
    Vec3 previous_position, Vec3* position, Vec3* direction,
    Vec3 centre, float radius) noexcept;

// Resolves overlap between two visual members and biases their headings away
// from one another. minimum_distance is centre-to-centre; fallback supplies a
// deterministic axis when both members occupy exactly the same point.
bool separate_pair(Vec3* left_position, Vec3* left_direction,
                   Vec3* right_position, Vec3* right_direction,
                   float minimum_distance, Vec3 fallback) noexcept;

Matrix34 compose_facing_transform(Vec3 position, Vec3 forward,
                                  Vec3 preferred_up) noexcept;

}  // namespace a2fo::swarm
