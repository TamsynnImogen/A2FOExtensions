#include "decal_math.hpp"

#include <cmath>

namespace a2fo::decal {

Vector3 rotate_xyz(Vector3 value, const Vector3& degrees) noexcept {
    constexpr float radians = 0.01745329251994329577f;
    const float sx = std::sin(degrees[0] * radians);
    const float cx = std::cos(degrees[0] * radians);
    const float sy = std::sin(degrees[1] * radians);
    const float cy = std::cos(degrees[1] * radians);
    const float sz = std::sin(degrees[2] * radians);
    const float cz = std::cos(degrees[2] * radians);

    value = {value[0] * cz - value[1] * sz,
             value[0] * sz + value[1] * cz, value[2]};
    value = {value[0] * cy + value[2] * sy, value[1],
             -value[0] * sy + value[2] * cy};
    return {value[0], value[1] * cx - value[2] * sx,
            value[1] * sx + value[2] * cx};
}

}  // namespace a2fo::decal
