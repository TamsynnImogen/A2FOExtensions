#pragma once
#include "opened_file.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace a2fo::native11 {
struct OpenReadOwner {
    std::uint32_t target=0;
    std::uint32_t gateway=0;
    std::uint32_t replacement=0;
    std::size_t length=0;
};

inline constexpr std::array<std::uint8_t,6> open_read_entry_bytes{
    0x55,0x8b,0xec,0x8b,0x45,0x08
};

bool owns_open_read_gateway(void* ctx,AssetMemoryRead read,
    const OpenReadOwner& owner) noexcept;
} // namespace a2fo::native11
