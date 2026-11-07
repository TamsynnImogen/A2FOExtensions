#pragma once

#include "entity_registry.hpp"

#include <cstdint>
#include <cstring>

namespace armadal::node_transform {
namespace detail {

inline float bits_to_float(std::uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline std::uint32_t float_to_bits(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

inline std::uint32_t scaled(const entity_registry::Memory& memory,
                            std::uint32_t object,
                            std::uint32_t source_offset,
                            float scale) {
    const float source = bits_to_float(memory.read(object + source_offset));
    return float_to_bits(scale * source);
}

} // namespace detail

// Recovered ArmadaL 0x00639460.  Unit scale returns the authored transform at
// +0x24.  Non-unit scale fills the cached transform at +0x58 while preserving
// the translation words at +0x48..+0x50.
inline std::uint32_t get(std::uint32_t object,
                         const entity_registry::Memory& memory) {
    constexpr std::uint32_t one_bits = 0x3f800000u;
    const std::uint32_t scale_bits = memory.read(object + 0x54u);
    if (scale_bits == one_bits) return object + 0x24u;

    const float scale = detail::bits_to_float(scale_bits);
    constexpr std::uint32_t source[9] = {
        0x24u, 0x28u, 0x2cu,
        0x30u, 0x34u, 0x38u,
        0x3cu, 0x40u, 0x44u,
    };
    constexpr std::uint32_t destination[9] = {
        0x58u, 0x5cu, 0x60u,
        0x64u, 0x68u, 0x6cu,
        0x70u, 0x74u, 0x78u,
    };

    for (unsigned i = 0; i < 9; ++i)
        memory.write(object + destination[i],
                     detail::scaled(memory, object, source[i], scale));

    memory.write(object + 0x7cu, memory.read(object + 0x48u));
    memory.write(object + 0x80u, memory.read(object + 0x4cu));
    memory.write(object + 0x84u, memory.read(object + 0x50u));
    return object + 0x58u;
}

} // namespace armadal::node_transform
