#include "texture_handle.hpp"
#include "../../rust/d3d_native_ffi.h"

namespace a2fo::renderer {
CompressedTextureHandle::CompressedTextureHandle()=default;
CompressedTextureHandle::~CompressedTextureHandle() {
    a2fo_rs_d3d_native_handle_drop(impl_);
}
HRESULT CompressedTextureHandle::initialize(ID3D11Device* device,
    const textures::CompressedImage& image,std::string& error) noexcept {
    struct Context {
        ID3D11Device* device;
        const textures::CompressedImage& image;
        std::string& error;
    } context{device,image,error};
    auto create=[](void* raw,void** output) noexcept -> std::int32_t {
        auto& c=*static_cast<Context*>(raw);
        CompressedTextureInfo info;
        ID3D11ShaderResourceView* view=nullptr;
        const auto hr=create_compressed_texture(c.device,c.image,&view,info,c.error);
        *output=view;
        return hr;
    };
    return a2fo_rs_d3d_native_handle_initialize(&impl_,a2fo_native_d3d_guard_api(),
        &context,create);
}
HRESULT CompressedTextureHandle::get_view(ID3D11ShaderResourceView** output) noexcept {
    void* view=nullptr;
    if(output)*output=nullptr;
    const auto hr=a2fo_rs_d3d_native_handle_get(impl_,output?&view:nullptr);
    if(output)*output=static_cast<ID3D11ShaderResourceView*>(view);
    return hr;
}
}
