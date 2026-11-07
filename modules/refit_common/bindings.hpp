#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Stock engine anchors researched by Cedric Haase's Armada II Refit, pinned
// in third_party/armada2-refit/SOURCE.md; independently checked against our
// clean and patched inputs. These are RVAs, not absolute runtime pointers.
namespace a2fo::refit::bindings {
inline constexpr std::uintptr_t preferred_base = 0x400000;
inline constexpr std::uint32_t pan_context_rva = 0x091766;
inline constexpr std::size_t pan_patch_offset = 17;
inline constexpr std::uint32_t fastscroll_rva = 0x30fbb4;
inline constexpr std::array<std::uint8_t, 25> pan_context{{
    0x68,0xcc,0xd7,0x6f,0x00,0xe8,0x00,0x6c,0x1c,0x00,
    0x8b,0x45,0xf0,0x85,0xc0,0x75,0x08,
    0xd9,0x1d,0xb4,0xfb,0x70,0x00,0xeb,0x02}};
inline constexpr std::array<std::uint32_t, 2> pan_consumers{{0x1268d7,0x1268f5}};
inline constexpr std::array<std::uint8_t, 6> pan_consumer{{0xd8,0x0d,0xb4,0xfb,0x70,0x00}};
inline constexpr std::uint32_t view_context_rva = 0x091bef;
inline constexpr std::uint32_t far_plane_rva = 0x2fcab0;
inline constexpr std::uint32_t cull_rva = 0x2fcac0;
inline constexpr std::uint32_t restore_cull_rva = 0x2fcac4;
inline constexpr std::array<std::uint8_t, 21> view_context{{
    0x8b,0x0d,0xc0,0xca,0x6f,0x00,0x89,0x15,0xfc,0x3c,0x77,0x00,
    0x8d,0x55,0xf0,0x89,0x0d,0xc4,0xca,0x6f,0x00}};
} // namespace a2fo::refit::bindings
