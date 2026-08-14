#pragma once

#include <array>

namespace a2fo::decal {

using Vector3 = std::array<float, 3>;

// Matches glam/Bevy EulerRot::XYZ: angles are written X/Y/Z but applied to a
// vector from the right, so Z is applied first, followed by Y and then X.
Vector3 rotate_xyz(Vector3 value, const Vector3& degrees) noexcept;

}  // namespace a2fo::decal
