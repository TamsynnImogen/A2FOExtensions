#include "compressed_texture.hpp"
#include "../../rust/d3d_native_ffi.h"

#include <d3d11.h>
#include <cstdio>
#include <exception>
#include <new>

namespace a2fo::renderer {
namespace {
HRESULT failure(std::string& error,const char* operation,HRESULT hr,
                const textures::CompressedImage& image) {
    char text[360];
    std::snprintf(text,sizeof(text),"[Texture] %s HRESULT=0x%08lx DXGI_format=%u size=%ux%u mips=%zu compressed_bytes=%zu",
        operation,static_cast<unsigned long>(hr),static_cast<UINT>(image.format),
        image.width,image.height,image.mips.size(),image.bytes.size());
    error=text;return hr;
}
struct UploadContext {
    const textures::CompressedImage& image;
    std::string& error;
    std::exception_ptr exception;
};
std::int32_t validate(void* raw) noexcept {
    auto& c=*static_cast<UploadContext*>(raw);
    return textures::validate_compressed_image(c.image,c.error);
}
std::uintptr_t read(void* raw,std::uint32_t field) noexcept {
    const auto& image=static_cast<UploadContext*>(raw)->image;
    return field==0?static_cast<std::uintptr_t>(image.format):
        reinterpret_cast<std::uintptr_t>(image.bytes.data());
}
const char* failure_operation(std::uint32_t operation) noexcept {
    switch(operation) {
    case A2FO_D3D_ERROR_MISSING_OUTPUT:return "missing output view";
    case A2FO_D3D_ERROR_MISSING_DEVICE:return "missing D3D11 device";
    case A2FO_D3D_ERROR_FEATURE:return "compressed format requires higher feature level";
    case A2FO_D3D_ERROR_CHECK_FORMAT:return "CheckFormatSupport";
    case A2FO_D3D_ERROR_SUPPORT:return "format lacks Texture2D/sample/mip support";
    case A2FO_D3D_ERROR_CREATE_TEXTURE:return "CreateTexture2D (compressed, immutable)";
    case A2FO_D3D_ERROR_CREATE_VIEW:return "CreateShaderResourceView (all compressed mips)";
    default:return "compressed texture upload failed";
    }
}
}
HRESULT create_compressed_texture(ID3D11Device* device,const textures::CompressedImage& image,
    ID3D11ShaderResourceView** output,CompressedTextureInfo& info,std::string& error) noexcept {
    if(output)*output=nullptr;
    try {
        error.clear();
        A2FoD3DMip mips[15]{};
        if(image.mips.size()<=15)for(std::size_t i=0;i<image.mips.size();++i) {
            const auto& mip=image.mips[i];
            mips[i]={mip.width,mip.height,mip.row_pitch,mip.slice_pitch,mip.offset};
        }
        const A2FoD3DImage view{static_cast<UINT>(image.format),image.width,image.height,
            static_cast<UINT>(image.alpha_mode),image.mips.size()<=15?mips:nullptr,
            image.mips.size(),image.bytes.data(),image.bytes.size()};
        const A2FoD3DConstants constants{
            E_POINTER,E_INVALIDARG,E_UNEXPECTED,DXGI_ERROR_UNSUPPORTED,
            DXGI_ERROR_SDK_COMPONENT_MISSING,
            DXGI_FORMAT_BC4_UNORM,DXGI_FORMAT_BC4_SNORM,
            DXGI_FORMAT_BC5_UNORM,DXGI_FORMAT_BC5_SNORM,
            DXGI_FORMAT_BC7_UNORM,DXGI_FORMAT_BC7_UNORM_SRGB,
            D3D_FEATURE_LEVEL_10_0,D3D_FEATURE_LEVEL_11_0,
            D3D11_FORMAT_SUPPORT_TEXTURE2D,D3D11_FORMAT_SUPPORT_SHADER_SAMPLE,
            D3D11_FORMAT_SUPPORT_MIP};
        UploadContext context{image,error,{}};
        const A2FoD3DNativeHost host{&context,validate,read,&context.exception};
        A2FoD3DResult result{};
        void* native_view=nullptr;
        const auto called=a2fo_rs_d3d_native_upload(&view,&constants,device,
            output!=nullptr,&host,a2fo_native_d3d_guard_api(),&native_view,&result);
        if(context.exception)std::rethrow_exception(context.exception);
        if(called==2)throw std::bad_alloc();
        if(called==3) {
            try{error="[Texture] Compressed texture upload failed";}catch(...){}
            return E_FAIL;
        }
        if(!called)return E_FAIL;
        if(FAILED(result.hr)) {
            if(result.operation!=A2FO_D3D_ERROR_NONE)
                return failure(error,failure_operation(result.operation),result.hr,image);
            return result.hr;
        }
        info.format=static_cast<DXGI_FORMAT>(result.plan.format);
        info.width=result.plan.width;info.height=result.plan.height;
        info.mip_levels=result.plan.mip_levels;
        info.alpha_mode=static_cast<textures::AlphaMode>(result.plan.alpha);
        info.block_payload_bytes=result.plan.block_bytes;
        info.rgba8_reference_bytes=result.plan.rgba8_bytes;
        *output=static_cast<ID3D11ShaderResourceView*>(native_view);
        return S_OK;
    } catch(const std::bad_alloc&) {
        try{error="[Texture] Compressed texture upload allocation failed";}catch(...){}
        return E_OUTOFMEMORY;
    } catch(...) {
        try{error="[Texture] Compressed texture upload failed";}catch(...){}
        return E_FAIL;
    }
}
}
