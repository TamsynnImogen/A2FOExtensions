/*
 * Pure policy and byte definitions for Fleet Operations' AMD native-DOT3
 * compatibility path. Kept independent of Win32 so the exact declaration
 * and selection rules can be host-unit-tested.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace a2fo::amd_dot3 {

enum class Policy : int {
    disabled = 0,
    automatic = 1,
    forced = 2,
};

inline constexpr std::uint32_t kAmdPciVendorId = 0x1002;
inline constexpr std::uint32_t kUncreatedShaderHandle = 0xffffffffu;

// Armada's stock stream is position(float3), normal(float3), UV(float2), and
// tangent basis(float3 x3). D3D8 register numbers become D3D9 semantics in
// d3d8to9, so v1-v5 turn into BLENDWEIGHT/BLENDINDICES/NORMAL/PSIZE/COLOR.
inline constexpr std::array<std::uint32_t, 8> kStockDeclaration{{
    0x20000000u,  // D3DVSD_STREAM(0)
    0x40020000u,  // v0  FLOAT3: position
    0x40020001u,  // v1  FLOAT3: normal
    0x40010002u,  // v2  FLOAT2: UV
    0x40020003u,  // v3  FLOAT3: tangent S
    0x40020004u,  // v4  FLOAT3: tangent T
    0x40020005u,  // v5  FLOAT3: tangent SxT
    0xffffffffu,  // D3DVSD_END()
}};

// Preserve the byte layout and stride, but route the non-position fields to
// v7-v11. d3d8to9 maps those registers to neutral TEXCOORD0-TEXCOORD4
// semantics, avoiding special blend-index, point-size and colour inputs.
inline constexpr std::array<std::uint32_t, 8> kRemappedDeclaration{{
    0x20000000u,
    0x40020000u,  // v0  FLOAT3: position
    0x40020007u,  // v7  FLOAT3: normal
    0x40010008u,  // v8  FLOAT2: UV
    0x40020009u,  // v9  FLOAT3: tangent S
    0x4002000au,  // v10 FLOAT3: tangent T
    0x4002000bu,  // v11 FLOAT3: tangent SxT
    0xffffffffu,
}};

// FleetOpsHook replaces both native shader-assembly calls with a callback
// whose source path is this fixed-size string. The replacement is padded to
// exactly the same size so checked in-place patching cannot touch adjacent
// code.
inline constexpr char kStockShaderPath[] =
    "shaders\\dot3_directional.nvv";
inline constexpr char kRemappedShaderPath[sizeof(kStockShaderPath)] =
    "shaders\\dot3_amd.nvv";

// Fleet Operations' /d3d9 route constructs this D3DVERTEXELEMENT9 array
// itself instead of translating Armada's D3D8 declaration. Keep a header-only
// representation so the exact byte layout can be checked on non-Windows test
// hosts without including mutually incompatible D3D8 and D3D9 headers.
struct VertexElement9 {
    std::uint16_t stream;
    std::uint16_t offset;
    std::uint8_t type;
    std::uint8_t method;
    std::uint8_t usage;
    std::uint8_t usage_index;
};

inline constexpr std::uint8_t kDeclTypeFloat2 = 1;
inline constexpr std::uint8_t kDeclTypeFloat3 = 2;
inline constexpr std::uint8_t kDeclTypeUnused = 17;
inline constexpr std::uint8_t kDeclUsagePosition = 0;
inline constexpr std::uint8_t kDeclUsageBlendWeight = 1;
inline constexpr std::uint8_t kDeclUsageBlendIndices = 2;
inline constexpr std::uint8_t kDeclUsageNormal = 3;
inline constexpr std::uint8_t kDeclUsagePointSize = 4;
inline constexpr std::uint8_t kDeclUsageTexcoord = 5;
inline constexpr std::uint8_t kDeclUsageColour = 10;

inline constexpr std::array<VertexElement9, 7>
    kDirectD3d9StockDeclaration{{
        {0, 0, kDeclTypeFloat3, 0, kDeclUsagePosition, 0},
        {0, 12, kDeclTypeFloat3, 0, kDeclUsageBlendWeight, 0},
        {0, 24, kDeclTypeFloat2, 0, kDeclUsageBlendIndices, 0},
        {0, 32, kDeclTypeFloat3, 0, kDeclUsageNormal, 0},
        {0, 44, kDeclTypeFloat3, 0, kDeclUsagePointSize, 0},
        {0, 56, kDeclTypeFloat3, 0, kDeclUsageColour, 0},
        {0xffffu, 0, kDeclTypeUnused, 0, 0, 0},
    }};

inline constexpr std::array<VertexElement9, 7>
    kDirectD3d9RemappedDeclaration{{
        {0, 0, kDeclTypeFloat3, 0, kDeclUsagePosition, 0},
        {0, 12, kDeclTypeFloat3, 0, kDeclUsageTexcoord, 0},
        {0, 24, kDeclTypeFloat2, 0, kDeclUsageTexcoord, 1},
        {0, 32, kDeclTypeFloat3, 0, kDeclUsageTexcoord, 2},
        {0, 44, kDeclTypeFloat3, 0, kDeclUsageTexcoord, 3},
        {0, 56, kDeclTypeFloat3, 0, kDeclUsageTexcoord, 4},
        {0xffffu, 0, kDeclTypeUnused, 0, 0, 0},
    }};

inline constexpr char kDirectD3d9RemappedShaderPath[] =
    "Shaders\\dot3_amd9.nvv";

constexpr Policy policy_from_ini(int value) noexcept {
    return value == static_cast<int>(Policy::disabled)
        ? Policy::disabled
        : value == static_cast<int>(Policy::forced)
            ? Policy::forced
            : Policy::automatic;
}

constexpr bool should_apply(Policy policy, bool vendor_known,
                            std::uint32_t pci_vendor_id) noexcept {
    if (policy == Policy::disabled) return false;
    if (policy == Policy::forced) return true;
    return vendor_known && pci_vendor_id == kAmdPciVendorId;
}

static_assert(sizeof(kStockShaderPath) == sizeof(kRemappedShaderPath));
static_assert(kStockDeclaration.size() == kRemappedDeclaration.size());
static_assert(sizeof(VertexElement9) == 8);
static_assert(kDirectD3d9StockDeclaration.size() ==
              kDirectD3d9RemappedDeclaration.size());

}  // namespace a2fo::amd_dot3
