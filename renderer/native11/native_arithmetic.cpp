#include "native_arithmetic.hpp"

#if !defined(__i386__) && !defined(__x86_64__)
#error Native arithmetic scope requires an x87 target
#endif

namespace a2fo::native11 {
NativeArithmeticScope::NativeArithmeticScope(std::uint32_t encoded) noexcept {
    if(!valid_native_x87_encoding(encoded))return;
    if(encoded==0){valid_=true;return;}
    valid_=known_=true;
    asm volatile("fnstcw %0":"=m"(saved_));
    const auto selected=static_cast<std::uint16_t>(
        (saved_&static_cast<std::uint16_t>(~0x0f00u))|(encoded&0x0f00u)|0x003fu);
    asm volatile("fldcw %0"::"m"(selected):"memory");
}
NativeArithmeticScope::~NativeArithmeticScope() noexcept {
    if(known_)asm volatile("fldcw %0"::"m"(saved_):"memory");
}
} // namespace a2fo::native11
