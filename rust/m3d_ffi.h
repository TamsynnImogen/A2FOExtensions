#pragma once

#include <cstddef>
#include <cstdint>

// Private synchronous bridge. All byte spans are borrowed for the callback.
extern "C" {
struct A2FoM3DEvent {
    std::uint32_t kind;
    const std::uint8_t* first;
    std::size_t first_length;
    const std::uint8_t* second;
    std::size_t second_length;
    const std::uint8_t* third;
    std::size_t third_length;
    std::size_t values[18];
    float bounds_min[3];
    float bounds_max[3];
};
using A2FoM3DEmit = int (*)(void*, const A2FoM3DEvent*) noexcept;
using A2FoM3DFormatVersion = std::size_t (*)(void*, std::uint32_t,
                                              char*, std::size_t) noexcept;
int a2fo_rs_m3d_convert(const std::uint8_t*, std::size_t, void*,
                         A2FoM3DEmit, A2FoM3DFormatVersion) noexcept;
}

enum : std::uint32_t {
    A2FO_M3D_STATS = 1,
    A2FO_M3D_WARNING = 2,
    A2FO_M3D_HIERARCHY = 3,
    A2FO_M3D_TEXTURE = 4,
    A2FO_M3D_OUTPUT = 5,
    A2FO_M3D_ERROR = 6,
};
