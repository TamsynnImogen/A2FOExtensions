#pragma once
#include "native_arithmetic.hpp"
#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>
namespace a2fo::native11 {
struct ModelGeometryPolicy {
    std::uint32_t parser_scalar_bits=0,native_x87_control=0;
};
// Positive uniform loader scale only. Unknown arithmetic, zero/negative scale
// and nonfinite input fail closed. Matrices/translations are not rescaled.
inline bool valid_model_geometry_policy(ModelGeometryPolicy policy) noexcept {
    float value=0;std::memcpy(&value,&policy.parser_scalar_bits,sizeof(value));
    return std::isfinite(value)&&value>0&&policy.native_x87_control!=0&&
        valid_native_x87_encoding(policy.native_x87_control);
}
// Original XYZ coordinates become binary32 scaled XYZ before reconstruction of
// normals. Failure leaves the supplied position array unchanged; at most 1Mi
// points and 12MiB temporary storage. Caller keeps source policy immutable.
bool apply_model_geometry_policy(ModelGeometryPolicy,float* xyz,std::uint32_t count,
    std::string& error) noexcept;
using ResolveModelGeometryPolicy=bool(*)(void*,const std::string&,ModelGeometryPolicy&,std::string&);
} // namespace a2fo::native11
