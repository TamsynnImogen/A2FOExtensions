#include "shadow_target.hpp"

namespace a2fo::native11 {
namespace {
template<class T> void drop(T*& pointer) noexcept {
    if (pointer) { pointer->Release(); pointer = nullptr; }
}
}

ShadowTarget::~ShadowTarget() noexcept {
    for (auto& depth_view : depth_views_) drop(depth_view);
    drop(view_);
    drop(texture_);
}

HRESULT ShadowTarget::initialize(ID3D11Device* device, std::uint32_t resolution,
                                 std::uint32_t slices) noexcept {
    if (texture_) return E_UNEXPECTED;
    if (!device || resolution < 128 || resolution > 2048 ||
        (resolution & (resolution - 1)) != 0 || slices == 0 || slices > depth_views_.size() ||
        std::uint64_t(resolution) * resolution * slices * 4 > 64ull * 1024 * 1024)
        return E_INVALIDARG;

    UINT support = 0;
    HRESULT hr = device->CheckFormatSupport(DXGI_FORMAT_R32_FLOAT, &support);
    if (FAILED(hr)) return hr;
    if ((support & (D3D11_FORMAT_SUPPORT_TEXTURE2D |
                    D3D11_FORMAT_SUPPORT_SHADER_LOAD)) !=
        (D3D11_FORMAT_SUPPORT_TEXTURE2D |
         D3D11_FORMAT_SUPPORT_SHADER_LOAD)) return DXGI_ERROR_UNSUPPORTED;
    hr = device->CheckFormatSupport(DXGI_FORMAT_D32_FLOAT, &support);
    if (FAILED(hr)) return hr;
    if ((support & (D3D11_FORMAT_SUPPORT_TEXTURE2D |
                    D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)) !=
        (D3D11_FORMAT_SUPPORT_TEXTURE2D |
         D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)) return DXGI_ERROR_UNSUPPORTED;

    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* view = nullptr;
    std::array<ID3D11DepthStencilView*, 48> depth_views{};
    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.Width = resolution;
    texture_desc.Height = resolution;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = slices;
    texture_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    hr = device->CreateTexture2D(&texture_desc, nullptr, &texture);
    if (SUCCEEDED(hr) && !texture) hr = E_FAIL;

    if (SUCCEEDED(hr)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{};
        view_desc.Format = DXGI_FORMAT_R32_FLOAT;
        view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        view_desc.Texture2DArray.MostDetailedMip = 0;
        view_desc.Texture2DArray.MipLevels = 1;
        view_desc.Texture2DArray.FirstArraySlice = 0;
        view_desc.Texture2DArray.ArraySize = slices;
        hr = device->CreateShaderResourceView(texture, &view_desc, &view);
        if (SUCCEEDED(hr) && !view) hr = E_FAIL;
    }
    if (SUCCEEDED(hr)) {
        D3D11_DEPTH_STENCIL_VIEW_DESC depth_desc{};
        depth_desc.Format = DXGI_FORMAT_D32_FLOAT;
        depth_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        depth_desc.Texture2DArray.MipSlice = 0;
        depth_desc.Texture2DArray.ArraySize = 1;
        for (std::uint32_t slice = 0; slice < slices; ++slice) {
            depth_desc.Texture2DArray.FirstArraySlice = slice;
            hr = device->CreateDepthStencilView(texture, &depth_desc, &depth_views[slice]);
            if (SUCCEEDED(hr) && !depth_views[slice]) hr = E_FAIL;
            if (FAILED(hr)) break;
        }
    }
    if (FAILED(hr)) {
        for (auto& depth_view : depth_views) drop(depth_view);
        drop(view);
        drop(texture);
        return hr;
    }
    texture_ = texture;
    view_ = view;
    depth_views_ = depth_views;
    resolution_ = resolution;
    slices_ = slices;
    return S_OK;
}

} // namespace a2fo::native11
