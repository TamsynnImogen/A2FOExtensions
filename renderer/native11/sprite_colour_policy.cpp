#include "sprite_colour_policy.hpp"
#include "native_arithmetic.hpp"
#include <cmath>
#include <cstring>

#if !defined(__i386__) && !defined(__x86_64__)
#error Native sprite colour parity requires an x87 target
#endif

namespace a2fo::native11 {
namespace {
std::uint32_t native_byte(float value) noexcept {
    constexpr float scale=255.0f;
    // Little-endian bytes 00 00 00 00 00 00 38 43, the native faddl operand.
    constexpr std::uint64_t magic=0x4338000000000000ull;
    std::uint64_t stored=0;
    asm volatile("flds %1; fmuls %2; faddl %3; fstpl %0"
        :"=m"(stored):"m"(value),"m"(scale),"m"(magic):"st","memory");
    std::uint32_t low=0;
    std::memcpy(&low,&stored,sizeof(low));
    return low;
}
bool component(float value) noexcept {
    return std::isfinite(value)&&value>=0.0f&&value<=1.0f;
}
}
bool native_sprite_colour(const float rgb[3],float alpha,
    std::uint32_t native_x87_control,std::uint32_t& output) noexcept {
    if(!rgb||!component(alpha)||!component(rgb[0])||!component(rgb[1])||
       !component(rgb[2])||!native_x87_control||
       !valid_native_x87_encoding(native_x87_control))return false;
    NativeArithmeticScope arithmetic(native_x87_control);
    if(!arithmetic.valid()||!arithmetic.known())return false;
    const auto red=native_byte(rgb[0]);
    const auto green=native_byte(rgb[1]);
    const auto blue=native_byte(rgb[2]);
    const auto opacity=native_byte(alpha);
    std::uint32_t packed=((red<<8)+green)<<8;
    packed+=blue;
    packed+=(opacity<<24);
    output=packed;
    return true;
}
} // namespace a2fo::native11
