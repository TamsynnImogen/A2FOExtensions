#pragma once

namespace a2fo::fire_arcs {

enum class ArcMode {
    box,
    cone,
};

// Armada's Matrix34 stores world-space right, up, and forward basis vectors,
// followed by translation. Each vector occupies three consecutive floats.
struct Matrix34 {
    float values[12]{};
};

struct ArcConfig {
    ArcMode mode = ArcMode::box;
    float yaw_degrees = 0.0f;
    float pitch_degrees = 0.0f;
    float yaw_angle_degrees = 360.0f;
    float pitch_angle_degrees = 180.0f;
    float cone_angle_degrees = 360.0f;
};

float normalize_degrees(float value) noexcept;
void normalize_config(ArcConfig* config) noexcept;

bool allows_target(
    const ArcConfig& config,
    const Matrix34& owner_transform,
    const float target_position[3]) noexcept;

}  // namespace a2fo::fire_arcs
