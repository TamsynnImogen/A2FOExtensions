/*
 * Host-testable geometry contract for A2FOFireArcs.
 */

#pragma once

#include <cstddef>

namespace a2fo::fire_arcs {

enum class ArcMode {
    // Independent yaw and pitch widths form a rectangular angular window.
    box,

    // One angular diameter forms a circular cap around the centre direction.
    cone,
};

// Armada's Matrix34 stores world-space right, up, and forward basis vectors,
// followed by translation. Each vector occupies three consecutive floats.
struct Matrix34 {
    float values[12]{};
};

struct Vector3 {
    float values[3]{};
};

enum class ArcLineStyle {
    boundary,
    centre,
};

struct ArcLine {
    Vector3 start{};
    Vector3 end{};
    ArcLineStyle style = ArcLineStyle::boundary;
};

struct ArcConfig {
    ArcMode mode = ArcMode::box;

    // Centre direction. Yaw wraps; pitch is clamped to -90..+90 degrees.
    float yaw_degrees = 0.0f;
    float pitch_degrees = 0.0f;

    // Total widths, not per-side limits. A 90-degree width extends 45 degrees
    // to either side of the centre. Only box mode consumes these values.
    float yaw_angle_degrees = 360.0f;
    float pitch_angle_degrees = 180.0f;

    // Total circular diameter consumed only by cone mode.
    float cone_angle_degrees = 360.0f;
};

// Wraps an angle into -180..+180. Used for yaw and yaw differences.
float normalize_degrees(float value) noexcept;

// Applies the public wrapping/clamping contract and replaces non-finite values
// with safe defaults. Runtime parsing calls this before retaining a policy;
// allows_target repeats it so direct test callers are equally safe.
void normalize_config(ArcConfig* config) noexcept;

// Converts the target into owner-local right/up/forward coordinates and tests
// it against the configured box or cone. Boundaries are inclusive.
bool allows_target(
    const ArcConfig& config,
    const Matrix34& owner_transform,
    const float target_position[3]) noexcept;

// Builds a bounded wireframe representation of the same owner-local volume
// used by allows_target. The origin may be a weapon hardpoint rather than the
// owner's translation; directions still follow the owner's live axes so the
// picture exactly matches runtime firing authorization.
std::size_t build_visualization_lines(
    const ArcConfig& config,
    const Matrix34& owner_transform,
    const float origin[3],
    float radius,
    ArcLine* output,
    std::size_t capacity) noexcept;

}  // namespace a2fo::fire_arcs
