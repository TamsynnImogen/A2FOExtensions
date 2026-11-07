#pragma once
#include <cstdint>

namespace a2fo::native11 {
// The marker distinguishes a captured native x87 control word from an unknown
// source. Only precision and rounding bits are applied by the scope.
constexpr std::uint32_t encode_native_x87(std::uint16_t word) noexcept {
    return 0x10000u|word;
}
constexpr bool valid_native_x87_encoding(std::uint32_t encoded) noexcept {
    return encoded==0||((encoded&~0x1ffffu)==0&&(encoded&0x10000u)!=0&&
                        (encoded&0x0300u)!=0x0100u);
}

class NativeArithmeticScope final {
public:
    explicit NativeArithmeticScope(std::uint32_t encoded) noexcept;
    ~NativeArithmeticScope() noexcept;
    NativeArithmeticScope(const NativeArithmeticScope&)=delete;
    NativeArithmeticScope& operator=(const NativeArithmeticScope&)=delete;
    NativeArithmeticScope(NativeArithmeticScope&&)=delete;
    NativeArithmeticScope& operator=(NativeArithmeticScope&&)=delete;
    bool valid() const noexcept {return valid_;}
    bool known() const noexcept {return known_;}
private:
    std::uint16_t saved_=0;
    bool valid_=false,known_=false;
};
} // namespace a2fo::native11
