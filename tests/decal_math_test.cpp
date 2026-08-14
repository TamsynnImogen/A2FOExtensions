#include "../core/decal_math.hpp"

#include <cassert>
#include <cmath>

namespace {

bool near(float left, float right) {
    return std::fabs(left - right) < 0.0001f;
}

void expect(const a2fo::decal::Vector3& actual,
            const a2fo::decal::Vector3& wanted) {
    assert(near(actual[0], wanted[0]));
    assert(near(actual[1], wanted[1]));
    assert(near(actual[2], wanted[2]));
}

}  // namespace

int main() {
    const a2fo::decal::Vector3 galaxy_rotation{{0.0f, 10.0f, 90.0f}};
    constexpr float sine_ten = 0.1736481777f;
    constexpr float cosine_ten = 0.9848077530f;

    // Arc Lab's EulerRot::XYZ applies Z, then Y, then X.
    expect(a2fo::decal::rotate_xyz({{1.0f, 0.0f, 0.0f}}, galaxy_rotation),
           {{0.0f, 1.0f, 0.0f}});
    expect(a2fo::decal::rotate_xyz({{0.0f, 1.0f, 0.0f}}, galaxy_rotation),
           {{-cosine_ten, 0.0f, sine_ten}});
    expect(a2fo::decal::rotate_xyz({{0.0f, 0.0f, 1.0f}}, galaxy_rotation),
           {{sine_ten, 0.0f, cosine_ten}});
}
