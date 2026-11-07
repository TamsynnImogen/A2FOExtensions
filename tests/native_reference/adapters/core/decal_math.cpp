#include "decal_math.hpp"
#include "../rust/text_geometry_ffi.h"
namespace a2fo::decal {
Vector3 rotate_xyz(Vector3 value, const Vector3 &degrees) noexcept {
    Vector3 result{};
    a2fo_rs_geometry_rotate_xyz(value.data(), degrees.data(), result.data());
    return result;
}
} // namespace a2fo::decal
