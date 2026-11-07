#include "../renderer/refit/geometry_parity.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace a2fo::refit;

namespace {
constexpr std::array<float, 12> world = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
constexpr std::array<float, 16> identity = {1, 0, 0, 0, 0, 1, 0, 0,
                                             0, 0, 1, 0, 0, 0, 0, 1};
// Row-vector projection: clip=(x,y,0.5,1+z). Its determinant is nonzero.
constexpr std::array<float, 16> projection = {1, 0, 0, 0, 0, 1, 0, 0,
                                               0, 0, 0, 1, 0, 0, .5f, 1};
constexpr std::array<float, 4> viewport = {0, 0, 100, 100};

AuthoredTriangle2 triangle(float z) {
    return {{{0, 0, z, 0, 0}, {.2f, 0, z, 1, 0}, {.1f, .2f, z, 0, 1}}};
}

ProjectedTriangle2 exact_projection(const AuthoredTriangle2& authored) {
    ProjectedTriangle2 result{};
    for (unsigned corner = 0; corner < 3; ++corner) {
        const auto& v = authored[corner];
        const double w = 1.0 + double(v.z);
        result[corner] = {float((double(v.x) / w + 1.0) * 50.0),
                          float((1.0 - double(v.y) / w) * 50.0),
                          float(.5 / w), v.u, v.v, float(1.0 / w)};
    }
    return result;
}

bool near_without_rhw(const ProjectedTriangle2& a, const ProjectedTriangle2& b) {
    for (unsigned corner = 0; corner < 3; ++corner)
        if (std::fabs(double(a[corner].x) - b[corner].x) > geometry_xy_tolerance2 ||
            std::fabs(double(a[corner].y) - b[corner].y) > geometry_xy_tolerance2 ||
            std::fabs(double(a[corner].z) - b[corner].z) > geometry_z_tolerance2 ||
            std::fabs(double(a[corner].u) - b[corner].u) > geometry_uv_tolerance2 ||
            std::fabs(double(a[corner].v) - b[corner].v) > geometry_uv_tolerance2)
            return false;
    return true;
}

bool accepted(const std::vector<AuthoredTriangle2>& authored,
              const std::vector<ProjectedTriangle2>& observed, const std::string& label) {
    std::uint32_t visible = 999;
    std::string error = "uninitialized";
    const bool result = check_geometry_parity2(authored, world, identity, projection,
                                                viewport, GeometryCull2::None,
                                                observed, visible, error);
    if (!result || visible != 2 || !error.empty()) {
        std::cerr << label << ": expected acceptance; result=" << result
                  << " visible=" << visible << " error=" << error << '\n';
        return false;
    }
    return true;
}

bool rejected(const std::vector<AuthoredTriangle2>& authored,
              const std::vector<ProjectedTriangle2>& observed,
              const std::string& expected_error) {
    std::uint32_t visible = 999;
    std::string error = "uninitialized";
    const bool result = check_geometry_parity2(authored, world, identity, projection,
                                                viewport, GeometryCull2::None,
                                                observed, visible, error);
    if (result || visible != 999 || error != expected_error) {
        std::cerr << "impossible shared RHW scale: expected '" << expected_error
                  << "'; result=" << result << " visible=" << visible
                  << " error=" << error << '\n';
        return false;
    }
    return true;
}

template<typename Triangle>
Triangle cyclic(Triangle triangle, unsigned shift) {
    std::rotate(triangle.begin(), triangle.begin() + shift, triangle.end());
    return triangle;
}
}

int main() {
    constexpr float z1 = -1.5e-6f;
    const auto e0 = triangle(0);
    const auto e1 = triangle(z1);
    const auto n0 = exact_projection(e0);
    const auto n1 = exact_projection(e1);
    const std::vector<AuthoredTriangle2> authored = {e0, e1};

    std::cout << std::setprecision(10)
              << "z1=" << z1 << " rhw0=" << n0[0].rhw << " rhw1=" << n1[0].rhw
              << " depth0=" << n0[0].z << " depth1=" << n1[0].z << '\n';
    if (!near_without_rhw(n0, n1)) {
        std::cerr << "cross-pair XY/Z/UV differences exceed existing tolerances\n";
        return 1;
    }
    // Reversing the failing native input order makes the first available
    // candidate for each authored triangle its exact projection.
    if (!accepted(authored, {n0, n1}, "reversed native order control")) return 1;

    // Both candidates are XY/Z/UV-near for either authored triangle. The
    // first perfect matching crosses RHW ratios in opposite directions;
    // swapping the assignments gives a single exact common scale of 1.
    if (!accepted(authored, {n1, n0}, "crossed native order")) return 1;

    unsigned permutations = 0;
    for (float scale : {.25f, 1.f, 8.f}) {
        auto scaled_n0 = n0;
        auto scaled_n1 = n1;
        for (auto& vertex : scaled_n0) vertex.rhw *= scale;
        for (auto& vertex : scaled_n1) vertex.rhw *= scale;
        for (unsigned authored_order = 0; authored_order < 2; ++authored_order)
            for (unsigned native_order = 0; native_order < 2; ++native_order)
                for (unsigned ae0 = 0; ae0 < 3; ++ae0)
                    for (unsigned ae1 = 0; ae1 < 3; ++ae1)
                        for (unsigned nn0 = 0; nn0 < 3; ++nn0)
                            for (unsigned nn1 = 0; nn1 < 3; ++nn1) {
                                const auto a0 = cyclic(e0, ae0);
                                const auto a1 = cyclic(e1, ae1);
                                const auto p0 = cyclic(scaled_n0, nn0);
                                const auto p1 = cyclic(scaled_n1, nn1);
                                const std::vector<AuthoredTriangle2> source = authored_order
                                    ? std::vector<AuthoredTriangle2>{a1, a0}
                                    : std::vector<AuthoredTriangle2>{a0, a1};
                                const std::vector<ProjectedTriangle2> observed = native_order
                                    ? std::vector<ProjectedTriangle2>{p1, p0}
                                    : std::vector<ProjectedTriangle2>{p0, p1};
                                if (!accepted(source, observed, "permuted common RHW scale")) {
                                    std::cerr << "scale=" << scale << " authored_order="
                                              << authored_order << " native_order=" << native_order
                                              << " rotations=" << ae0 << ae1 << nn0 << nn1 << '\n';
                                    return 1;
                                }
                                ++permutations;
                            }
    }

    // Each native triangle has an individually plausible scale near 1, but
    // one-to-one matching must use both and their scale intervals are disjoint.
    auto high = exact_projection(e0);
    auto low = exact_projection(e0);
    for (auto& vertex : high) vertex.rhw *= 1.f + 1.5e-6f;
    for (auto& vertex : low) vertex.rhw *= 1.f - 1.5e-6f;
    if (!rejected({e0, e0}, {high, low}, "reciprocal W scale mismatch")) return 1;
    if (!rejected({e0, e0}, {low, high}, "reciprocal W scale mismatch")) return 1;
    std::cout << permutations << " order/rotation/scale permutations accepted; "
              << "impossible shared scales rejected\n";
}
