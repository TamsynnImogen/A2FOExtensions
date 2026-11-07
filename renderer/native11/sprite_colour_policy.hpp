#pragma once
#include <cstdint>

namespace a2fo::native11 {
// Native sprite RGB/alpha byte conversion and wrapping ARGB addition under the
// captured original x87 precision/rounding mode. Failure leaves output intact.
bool native_sprite_colour(const float rgb[3],float alpha,
    std::uint32_t native_x87_control,std::uint32_t& output) noexcept;
} // namespace a2fo::native11
