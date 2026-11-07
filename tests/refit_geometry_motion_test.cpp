#include "../renderer/refit/geometry_parity.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace a2fo::refit;

namespace {
constexpr std::array<float, 12> world = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
constexpr std::array<float, 16> identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
constexpr std::array<float, 4> viewport = {0, 0, 100, 100};
constexpr ProjectedTriangle2 native = {{{10, 20, .5f, 0, 0, 1},
                                         {10, 10, .5f, 1, 0, 1},
                                         {20, 20, .5f, 0, 1, 1}}};

bool accepted(const std::vector<AuthoredTriangle2>& authored,
              const std::vector<ProjectedTriangle2>& observed, const char* label) {
    std::uint32_t visible = 999;
    std::string error = "uninitialized";
    const bool result = check_geometry_parity2(authored, world, identity, identity,
                                                viewport, GeometryCull2::None,
                                                observed, visible, error);
    if (!result || visible != authored.size() || !error.empty()) {
        std::cerr << label << ": expected accepted triangles; result=" << result
                  << " visible=" << visible << " error=" << error << '\n';
        return false;
    }
    return true;
}

bool rejected(const std::vector<AuthoredTriangle2>& authored,
              const std::vector<ProjectedTriangle2>& observed,
              const char* expected_error, const char* label) {
    std::uint32_t visible = 999;
    std::string error = "uninitialized";
    const bool result = check_geometry_parity2(authored, world, identity, identity,
                                                viewport, GeometryCull2::None,
                                                observed, visible, error);
    if (result || visible != 999 || error != expected_error) {
        std::cerr << label << ": expected rejection '" << expected_error
                  << "'; result=" << result << " visible=" << visible
                  << " error=" << error << '\n';
        return false;
    }
    return true;
}

AuthoredTriangle2 shift_authored(AuthoredTriangle2 triangle, float pixel_x) {
    for (auto& vertex : triangle) vertex.x += pixel_x / 50.f;
    return triangle;
}

ProjectedTriangle2 shift_native(ProjectedTriangle2 triangle, float pixel_x) {
    for (auto& vertex : triangle) vertex.x += pixel_x;
    return triangle;
}
}

int main() {
    // A vertical baseline has the same canonical corner in both streams.
    const AuthoredTriangle2 baseline = {{{-.8f, .6f, .5f, 0, 0},
                                         {-.8f, .8f, .5f, 1, 0},
                                         {-.6f, .6f, .5f, 0, 1}}};
    if (!accepted({baseline}, {native}, "vertical baseline")) return 1;

    // Both X coordinates project within 1/256 pixel of the native X=10.
    // A now sorts before B in the authored projection, while B sorts before A
    // after native X quantization. The corner correspondence remains A/B/C.
    const AuthoredTriangle2 moving = {{{-.799998f, .6f, .5f, 0, 0},
                                       {-.799996f, .8f, .5f, 1, 0},
                                       {-.6f, .6f, .5f, 0, 1}}};
    if (!accepted({moving}, {native}, "subpixel motion frame")) return 1;

    // Frame-to-frame positions may cross the exact X ordering boundary while
    // every native vertex remains quantized to the same X pixel.
    for (float offset : {-3e-6f, -2e-6f, -1e-6f, 0.f, 1e-6f, 2e-6f, 3e-6f}) {
        auto frame = baseline;
        frame[0].x += offset;
        frame[1].x += offset + 2e-6f;
        for (unsigned authored_rotation = 0; authored_rotation < 3; ++authored_rotation) {
            auto rotated_frame = frame;
            std::rotate(rotated_frame.begin(), rotated_frame.begin() + authored_rotation,
                        rotated_frame.end());
            for (unsigned native_rotation = 0; native_rotation < 3; ++native_rotation) {
                auto rotated_native = native;
                std::rotate(rotated_native.begin(), rotated_native.begin() + native_rotation,
                            rotated_native.end());
                if (!accepted({rotated_frame}, {rotated_native}, "motion/rotation frame")) return 1;
            }
        }
    }

    // E0 can match N0 or N1; E1 can match only N0. Both input orders and native
    // orders must find the one-to-one E0->N1, E1->N0 assignment.
    const auto e1 = shift_authored(baseline, -float(geometry_xy_tolerance2));
    const auto n0 = shift_native(native, -float(geometry_xy_tolerance2 / 2));
    const auto n1 = shift_native(native, float(geometry_xy_tolerance2 / 2));
    for (unsigned authored_order = 0; authored_order < 2; ++authored_order)
        for (unsigned native_order = 0; native_order < 2; ++native_order) {
            const std::vector<AuthoredTriangle2> authored = authored_order
                ? std::vector<AuthoredTriangle2>{e1, baseline}
                : std::vector<AuthoredTriangle2>{baseline, e1};
            const std::vector<ProjectedTriangle2> observed = native_order
                ? std::vector<ProjectedTriangle2>{n1, n0}
                : std::vector<ProjectedTriangle2>{n0, n1};
            if (!accepted(authored, observed, "augmenting one-to-one assignment")) return 1;
        }

    // The three ratios straddle a common positive scale by less than 1e-6.
    // A larger spread has no such scale and must remain refused.
    auto rhw = native;
    rhw[0].rhw = 1.f + .9e-6f;
    rhw[1].rhw = 1.f;
    rhw[2].rhw = 1.f - .9e-6f;
    if (!accepted({baseline}, {rhw}, "common positive RHW scale")) return 1;
    rhw[0].rhw = 1.f + 3e-6f;
    rhw[2].rhw = 1.f - 3e-6f;
    if (!rejected({baseline}, {rhw}, "projected geometry mismatch",
                  "nonuniform RHW scale")) return 1;

    // Dense duplicates are bounded instead of creating unbounded matching
    // work; the caller's visible count must remain untouched on refusal.
    if (!rejected(std::vector<AuthoredTriangle2>(2048, baseline),
                  std::vector<ProjectedTriangle2>(2048, native),
                  "geometry matching work limit", "ambiguous 2048-triangle limit")) return 1;
    std::cout << "motion, permutations, matching, RHW, and work limit verified\n";
}
