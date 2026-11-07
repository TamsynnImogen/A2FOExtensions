#include "../rust/text_geometry_ffi.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
using Vector = std::array<float, 3>;

bool near(const Vector& actual, const Vector& expected) {
    for (std::size_t i = 0; i < actual.size(); ++i) {
        if (std::fabs(actual[i] - expected[i]) > 0.00001f) return false;
    }
    return true;
}

bool check(const char* label, const Vector& input, const Vector& degrees,
           const Vector& expected) {
    const Vector original_input = input;
    const Vector original_degrees = degrees;
    Vector output{{-7.0f, -8.0f, -9.0f}};
    a2fo_rs_geometry_rotate_xyz(input.data(), degrees.data(), output.data());
    if (!near(output, expected) ||
        std::memcmp(input.data(), original_input.data(), sizeof(input)) != 0 ||
        std::memcmp(degrees.data(), original_degrees.data(), sizeof(degrees)) != 0) {
        std::fprintf(stderr, "decal binding failure: %s\n", label);
        return false;
    }
    return true;
}
} // namespace

int main() {
    const Vector zero{{0.0f, 0.0f, 0.0f}};
    if (!check("zero rotation", {{1.25f, -2.5f, 3.75f}}, zero,
               {{1.25f, -2.5f, 3.75f}})) return 1;
    if (!check("x ninety", {{0.0f, 1.0f, 0.0f}}, {{90.0f, 0.0f, 0.0f}},
               {{0.0f, 0.0f, 1.0f}})) return 1;
    if (!check("y ninety", {{0.0f, 0.0f, 1.0f}}, {{0.0f, 90.0f, 0.0f}},
               {{1.0f, 0.0f, 0.0f}})) return 1;
    if (!check("z ninety", {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 90.0f}},
               {{0.0f, 1.0f, 0.0f}})) return 1;
    std::puts("decal binding: PASS");
}
