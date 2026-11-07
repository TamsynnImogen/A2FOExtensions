#pragma once

#include <windows.h>
#include <dxgiformat.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::renderer::textures {

enum class AlphaMode : UINT {
    unknown = 0, straight = 1, premultiplied = 2, opaque = 3, custom = 4
};

struct CompressedMip {
    UINT width = 0;
    UINT height = 0;
    UINT row_pitch = 0;
    UINT slice_pitch = 0;
    std::size_t offset = 0;
};

struct CompressedImage {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0;
    UINT height = 0;
    AlphaMode alpha_mode = AlphaMode::unknown;
    std::vector<CompressedMip> mips;
    std::vector<std::uint8_t> bytes;
};

// Tightly packed 4x4 BC block rows. Outputs are unchanged on failure.
bool compressed_mip_layout(DXGI_FORMAT format, UINT width, UINT height,
                           UINT& row_pitch, UINT& slice_pitch) noexcept;

// Checks an image, including its exact mip payload extent, before upload.
HRESULT validate_compressed_image(const CompressedImage& image,
                                  std::string& error) noexcept;

// Copies only the encoded mip payload. Trailing file bytes are ignored.
// On failure, output is unchanged. No block data is decompressed.
HRESULT parse_compressed_dds(const void* data, std::size_t size,
                             CompressedImage& output, std::string& error) noexcept;

} // namespace a2fo::renderer::textures
