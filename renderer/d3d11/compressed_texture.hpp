#pragma once
#include "../textures/dds.hpp"
#include <cstdint>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace a2fo::renderer {
struct CompressedTextureInfo {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0, height = 0, mip_levels = 0;
    std::uint64_t block_payload_bytes = 0, rgba8_reference_bytes = 0;
    textures::AlphaMode alpha_mode = textures::AlphaMode::unknown;
};
// One immutable, shader-readable Texture2D containing exactly the supplied BC
// mip chain. No RGBA fallback, extra mip generation or retained CPU shadow.
// Caller owns the input and serializes device use if its device requires it.
// Output view is cleared on failure; info is updated only on success. Payload
// byte counts are format-derived, not driver allocation/residency measurements.
HRESULT create_compressed_texture(ID3D11Device*,const textures::CompressedImage&,
    ID3D11ShaderResourceView**,CompressedTextureInfo&,std::string& error) noexcept;
}
