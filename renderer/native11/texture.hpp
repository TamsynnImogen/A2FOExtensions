#pragma once
#include <cstdint>

namespace a2fo::native11 {
// Asset/scene vocabulary, deliberately independent of DXGI/D3D enumerations.
// Numeric values match the synchronous CPU adapter's texture callback protocol.
enum class TextureFormat : std::uint32_t {
    bgra8=1, rgba8=2, bgra8_srgb=3, rgba8_srgb=4,
    bc1=5, bc1_srgb=6, bc2=7, bc2_srgb=8, bc3=9, bc3_srgb=10,
    bc4=11, bc4_snorm=12, bc5=13, bc5_snorm=14, bc7=15, bc7_srgb=16
};
enum class TextureAlpha : std::uint32_t {
    unknown=0, straight=1, premultiplied=2, opaque=3, custom=4
};
struct TextureMip {
    std::uint32_t width, height, row_pitch, slice_pitch;
    std::uint64_t offset, byte_count;
};
struct TextureView {
    TextureFormat format=TextureFormat::bgra8;
    TextureAlpha alpha=TextureAlpha::unknown;
    std::uint32_t width=0, height=0;
    const TextureMip* mips=nullptr;
    std::uint32_t mip_count=0;
    const std::uint8_t* bytes=nullptr;
    std::uint64_t byte_count=0;
};
struct TexturePlan {
    TextureFormat format;
    TextureAlpha alpha;
    std::uint32_t width, height, mip_count;
    std::uint64_t storage_bytes;
};
// Validates every level before upload; canonical tightly packed level payloads
// must be contiguous and exactly span byte_count. No pixel data is dereferenced.
// Source storage expires after upload returns. Output changes only on success.
bool plan_texture(const TextureView&, TexturePlan&) noexcept;
} // namespace a2fo::native11
