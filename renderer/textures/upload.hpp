#pragma once

#include <windows.h>
#include <dxgiformat.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace a2fo::renderer::compat {

struct TextureUpload {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0;
    UINT height = 0;
    UINT row_pitch = 0;
    std::vector<std::uint8_t> bytes;
};

// Produces tightly packed D3D11 upload rows. On failure, output is unchanged.
// source_bytes must cover every copied row, including gaps from source_pitch.
HRESULT prepare_texture_upload(UINT format, UINT width, UINT height,
                               const void* source, std::size_t source_bytes,
                               UINT source_pitch, TextureUpload& output) noexcept;

} // namespace a2fo::renderer::compat
