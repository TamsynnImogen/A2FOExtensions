#include "fire_arc.hpp"

#include <cmath>
#include <cstdio>

namespace {

using a2fo::fire_arcs::ArcConfig;
using a2fo::fire_arcs::ArcLine;
using a2fo::fire_arcs::ArcLineStyle;
using a2fo::fire_arcs::ArcMode;
using a2fo::fire_arcs::Matrix34;

constexpr float kPi = 3.14159265358979323846f;

Matrix34 identity_at(float x = 0.0f, float y = 0.0f,
                     float z = 0.0f) {
    Matrix34 matrix{};
    matrix.values[0] = 1.0f;
    matrix.values[4] = 1.0f;
    matrix.values[8] = 1.0f;
    matrix.values[9] = x;
    matrix.values[10] = y;
    matrix.values[11] = z;
    return matrix;
}

void direction_for(float yaw_degrees, float pitch_degrees,
                   float output[3]) {
    const float yaw = yaw_degrees * kPi / 180.0f;
    const float pitch = pitch_degrees * kPi / 180.0f;
    const float cosine_pitch = std::cos(pitch);
    output[0] = std::sin(yaw) * cosine_pitch * 100.0f;
    output[1] = std::sin(pitch) * 100.0f;
    output[2] = std::cos(yaw) * cosine_pitch * 100.0f;
}

bool expect_allowed(const char* name, const ArcConfig& config,
                    const Matrix34& transform, float yaw, float pitch,
                    bool expected) {
    float position[3]{};
    direction_for(yaw, pitch, position);
    position[0] += transform.values[9];
    position[1] += transform.values[10];
    position[2] += transform.values[11];
    const bool actual = a2fo::fire_arcs::allows_target(
        config, transform, position);
    if (actual == expected) return true;
    std::fprintf(stderr, "%s: yaw %.2f pitch %.2f was %s\n",
                 name, yaw, pitch, actual ? "allowed" : "rejected");
    return false;
}

}  // namespace

int main() {
    const Matrix34 identity = identity_at(10.0f, 20.0f, 30.0f);
    ArcConfig box{};
    box.yaw_angle_degrees = 90.0f;
    box.pitch_angle_degrees = 60.0f;
    if (!expect_allowed("box forward", box, identity, 0.0f, 0.0f, true) ||
        !expect_allowed("box yaw boundary", box, identity, 45.0f, 0.0f, true) ||
        !expect_allowed("box yaw outside", box, identity, 46.0f, 0.0f, false) ||
        !expect_allowed("box pitch boundary", box, identity, 0.0f, 30.0f, true) ||
        !expect_allowed("box pitch outside", box, identity, 0.0f, 31.0f, false)) {
        return 1;
    }

    ArcConfig starboard = box;
    starboard.yaw_degrees = 90.0f;
    starboard.yaw_angle_degrees = 20.0f;
    starboard.pitch_angle_degrees = 20.0f;
    if (!expect_allowed("starboard", starboard, identity, 90.0f, 0.0f, true) ||
        !expect_allowed("starboard rejects forward", starboard, identity,
                        0.0f, 0.0f, false)) {
        return 2;
    }

    ArcConfig rear = starboard;
    rear.yaw_degrees = 180.0f;
    ArcConfig port = starboard;
    port.yaw_degrees = 270.0f;
    if (!expect_allowed("rear", rear, identity, 180.0f, 0.0f, true) ||
        !expect_allowed("port 270", port, identity, -90.0f, 0.0f, true)) {
        return 3;
    }

    ArcConfig upward = starboard;
    upward.yaw_degrees = 0.0f;
    upward.pitch_degrees = 90.0f;
    upward.yaw_angle_degrees = 360.0f;
    if (!expect_allowed("up", upward, identity, 0.0f, 90.0f, true) ||
        !expect_allowed("up rejects level", upward, identity,
                        0.0f, 0.0f, false)) {
        return 4;
    }

    ArcConfig upper_hemisphere{};
    upper_hemisphere.yaw_angle_degrees = 270.0f;
    upper_hemisphere.pitch_degrees = 90.0f;
    upper_hemisphere.pitch_angle_degrees = 180.0f;
    if (!expect_allowed("upper hemisphere", upper_hemisphere, identity,
                        0.0f, 45.0f, true) ||
        !expect_allowed("upper hemisphere zenith", upper_hemisphere,
                        identity, 0.0f, 90.0f, true) ||
        !expect_allowed("upper hemisphere horizon", upper_hemisphere,
                        identity, 0.0f, 0.0f, true) ||
        !expect_allowed("upper hemisphere rejects below",
                        upper_hemisphere, identity, 0.0f, -1.0f, false) ||
        !expect_allowed("upper hemisphere retains yaw limit",
                        upper_hemisphere, identity, 180.0f, 45.0f, false)) {
        return 5;
    }

    ArcConfig lower_hemisphere = upper_hemisphere;
    lower_hemisphere.pitch_degrees = -90.0f;
    if (!expect_allowed("lower hemisphere", lower_hemisphere, identity,
                        0.0f, -45.0f, true) ||
        !expect_allowed("lower hemisphere nadir", lower_hemisphere, identity,
                        0.0f, -90.0f, true) ||
        !expect_allowed("lower hemisphere rejects above",
                        lower_hemisphere, identity, 0.0f, 1.0f, false)) {
        return 6;
    }

    ArcConfig cone{};
    cone.mode = ArcMode::cone;
    cone.cone_angle_degrees = 90.0f;
    if (!expect_allowed("cone inside", cone, identity, 44.0f, 0.0f, true) ||
        !expect_allowed("cone outside", cone, identity, 46.0f, 0.0f, false) ||
        !expect_allowed("cone rejects box corner", cone, identity,
                        40.0f, 40.0f, false)) {
        return 7;
    }

    Matrix34 yawed = identity;
    yawed.values[0] = 0.0f;
    yawed.values[1] = 0.0f;
    yawed.values[2] = -1.0f;
    yawed.values[3] = 0.0f;
    yawed.values[4] = 1.0f;
    yawed.values[5] = 0.0f;
    yawed.values[6] = 1.0f;
    yawed.values[7] = 0.0f;
    yawed.values[8] = 0.0f;
    const float world_starboard[3]{110.0f, 20.0f, 30.0f};
    const float world_forward[3]{10.0f, 20.0f, 130.0f};
    if (!a2fo::fire_arcs::allows_target(box, yawed, world_starboard) ||
        a2fo::fire_arcs::allows_target(box, yawed, world_forward)) {
        std::fprintf(stderr, "owner-local rotation was not respected\n");
        return 8;
    }

    const float same_position[3]{10.0f, 20.0f, 30.0f};
    if (a2fo::fire_arcs::allows_target(box, identity, same_position)) {
        std::fprintf(stderr, "zero-length direction was allowed\n");
        return 9;
    }

    // Yaw wraps around a compass, while pitch deliberately clamps at the two
    // poles. These are easy conventions for ODF authors to confuse and are
    // therefore kept as explicit regression checks.
    ArcConfig normalized{};
    normalized.yaw_degrees = 270.0f;
    normalized.pitch_degrees = 270.0f;
    a2fo::fire_arcs::normalize_config(&normalized);
    if (normalized.yaw_degrees != -90.0f ||
        normalized.pitch_degrees != 90.0f) {
        std::fprintf(stderr, "positive angle normalization failed\n");
        return 10;
    }
    normalized.pitch_degrees = -270.0f;
    a2fo::fire_arcs::normalize_config(&normalized);
    if (normalized.pitch_degrees != -90.0f) {
        std::fprintf(stderr, "negative pitch clamping failed\n");
        return 11;
    }

    ArcLine lines[128]{};
    const float origin[3]{10.0f, 20.0f, 30.0f};
    const std::size_t box_line_count =
        a2fo::fire_arcs::build_visualization_lines(
            box, identity, origin, 100.0f,
            lines, sizeof(lines) / sizeof(lines[0]));
    if (box_line_count < 50 || box_line_count > 128 ||
        lines[box_line_count - 1].style != ArcLineStyle::centre) {
        std::fprintf(stderr, "box visualization was incomplete\n");
        return 12;
    }
    for (std::size_t index = 0; index < box_line_count; ++index) {
        if (!a2fo::fire_arcs::allows_target(
                box, identity, lines[index].end.values)) {
            std::fprintf(stderr,
                         "box visualization left its policy at line %zu\n",
                         index);
            return 13;
        }
    }

    const std::size_t cone_line_count =
        a2fo::fire_arcs::build_visualization_lines(
            cone, identity, origin, 100.0f,
            lines, sizeof(lines) / sizeof(lines[0]));
    if (cone_line_count < 32 || cone_line_count > 128 ||
        lines[cone_line_count - 1].style != ArcLineStyle::centre) {
        std::fprintf(stderr, "cone visualization was incomplete\n");
        return 14;
    }
    for (std::size_t index = 0; index < cone_line_count; ++index) {
        if (!a2fo::fire_arcs::allows_target(
                cone, identity, lines[index].end.values)) {
            std::fprintf(stderr,
                         "cone visualization left its policy at line %zu\n",
                         index);
            return 15;
        }
    }

    ArcConfig unrestricted{};
    const std::size_t sphere_line_count =
        a2fo::fire_arcs::build_visualization_lines(
            unrestricted, yawed, origin, 50.0f,
            lines, sizeof(lines) / sizeof(lines[0]));
    if (sphere_line_count < 90 || sphere_line_count > 128) {
        std::fprintf(stderr, "unrestricted arc did not render as a sphere\n");
        return 16;
    }

    std::puts("fire arc tests passed");
    return 0;
}
