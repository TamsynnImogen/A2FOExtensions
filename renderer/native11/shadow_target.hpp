#pragma once
#include <array>
#include <cstdint>
#include <d3d11.h>

namespace a2fo::native11 {

// Owns one typeless depth texture array and its read/write views.
// Initialize and destroy on the renderer's owning device thread.
class ShadowTarget {
public:
    ShadowTarget() noexcept = default;
    ShadowTarget(const ShadowTarget&) = delete;
    ShadowTarget& operator=(const ShadowTarget&) = delete;
    ~ShadowTarget() noexcept;

    HRESULT initialize(ID3D11Device* device, std::uint32_t resolution,
                       std::uint32_t slices) noexcept;
    ID3D11ShaderResourceView* view() const noexcept { return view_; }
    ID3D11DepthStencilView* depth_view(std::uint32_t slice) const noexcept {
        return slice < slices_ ? depth_views_[slice] : nullptr;
    }
    std::uint32_t resolution() const noexcept { return resolution_; }
    std::uint32_t slices() const noexcept { return slices_; }

private:
    ID3D11Texture2D* texture_ = nullptr;
    ID3D11ShaderResourceView* view_ = nullptr;
    std::array<ID3D11DepthStencilView*, 48> depth_views_{};
    std::uint32_t resolution_ = 0;
    std::uint32_t slices_ = 0;
};

} // namespace a2fo::native11
