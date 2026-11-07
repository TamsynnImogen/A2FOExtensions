#pragma once
#include "compressed_texture.hpp"
namespace a2fo::renderer {
// SDK-neutral ownership bridge. Legacy and modern Direct3D headers stay in
// separate translation units; no COM pointer is cast between API versions.
class CompressedTextureHandle {
public:
    CompressedTextureHandle();
    ~CompressedTextureHandle();
    CompressedTextureHandle(const CompressedTextureHandle&)=delete;
    CompressedTextureHandle& operator=(const CompressedTextureHandle&)=delete;
    HRESULT initialize(ID3D11Device*,const textures::CompressedImage&,std::string&) noexcept;
    HRESULT get_view(ID3D11ShaderResourceView**) noexcept; // caller receives AddRef
private:
    void* impl_=nullptr; // Opaque Rust-owned resource state.
};
}
