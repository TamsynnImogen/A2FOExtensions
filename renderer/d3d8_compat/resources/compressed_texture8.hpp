#pragma once
#include <d3d8.h>
#include "../../textures/dds.hpp"
struct ID3D11Device;
struct ID3D11ShaderResourceView;
namespace a2fo::renderer::compat {
enum class TextureInterpretation : UINT {raw=0,normal_xy=1};
struct CompressedBinding {
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
    TextureInterpretation interpretation=TextureInterpretation::raw;
    DWORD minimum_lod=0;
};
inline constexpr GUID IID_CompressedTextureBinding =
    {0x17b1474e,0xcf0b,0x4421,{0x9c,0x4f,0xc8,0x33,0x20,0x74,0x08,0x11}};
// Private backend interface; never pass its modern view to a native D3D8 device.
struct CompressedTextureBinding : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetBinding(ID3D11ShaderResourceView**,
                                                   CompressedBinding*)=0;
    // Device state retains this private interface implicitly, avoiding a
    // device<->public-texture reference cycle. Pair exactly on the owner thread;
    // keep the backend module loaded for the lifetime of all resources/views.
    virtual ULONG STDMETHODCALLTYPE RetainBinding()=0;
    virtual ULONG STDMETHODCALLTYPE ReleaseBinding()=0;
    virtual HRESULT STDMETHODCALLTYPE GetTexture(IDirect3DTexture8**)=0;
};
// External immutable DDS assets only, not an IDirect3DDevice8::CreateTexture
// replacement. Caller owns a compatibility device; it must arrange device/state
// lifetimes and consume minimum_lod when building a modern sampler. All methods
// except nonfinal COM reference counting are serialized on the owner's thread.
// Final Release, implicit binding changes and destruction run on that thread.
// Keeps one encoded CPU shadow for read locks, never an expanded RGBA shadow.
// The image is consumed only after GPU creation succeeds. No startup hook yet.
HRESULT create_compressed_texture8(IDirect3DDevice8* compatibility_device,
    ID3D11Device*,textures::CompressedImage&&,TextureInterpretation,
    IDirect3DTexture8**,std::string& error) noexcept;
}
