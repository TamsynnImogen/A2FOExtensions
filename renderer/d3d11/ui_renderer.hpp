#pragma once
#include <windows.h>
#include <cstddef>
#include <memory>
#include <string>

// Keep the compatibility boundary usable from a D3D8 translation unit. MinGW's
// legacy and modern SDK headers cannot coexist; no COM pointer is reinterpreted.
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11ShaderResourceView;

namespace a2fo::renderer {
namespace compat {struct TextureUpload;}
namespace textures {struct CompressedImage;}
struct Viewport {
    UINT x=0,y=0,width=0,height=0;
    float min_z=0,max_z=1;
};
// Numeric state values are the original D3D8 enums. This first draw slice is
// explicit: one colour/alpha texture stage, transformed FVF vertices, no fog,
// programmable shaders, lighting, texgen, or implicit approximation of them.
struct UiDrawState {
    DWORD color_op=4,color_arg1=2,color_arg2=0; // MODULATE, TEXTURE, DIFFUSE
    DWORD alpha_op=4,alpha_arg1=2,alpha_arg2=0;
    bool alpha_test=false,alpha_blend=false,depth_enable=false,depth_write=false;
    bool specular_enable=false;
    DWORD alpha_ref=0,alpha_func=8;
    DWORD src_blend=5,dst_blend=6,blend_op=1,color_write_mask=15;
    DWORD depth_func=4,cull=1;
    DWORD min_filter=1,mag_filter=1,mip_filter=0,address_u=3,address_v=3;
};
class UiRenderer {
public:
    using Log=void (*)(const std::string&);
    UiRenderer();
    ~UiRenderer();
    UiRenderer(const UiRenderer&)=delete;
    UiRenderer& operator=(const UiRenderer&)=delete;
    // The caller serializes all use, including shutdown/destruction, on the
    // initialization thread. This object owns retained COM references and its
    // own pipeline state; it is not an overlay that restores an external
    // D3D11 context. The future compatibility device must own that context.
    HRESULT initialize(ID3D11Device*,ID3D11DeviceContext*,const std::string& shader_cache,Log=nullptr);
    void shutdown() noexcept;
    HRESULT set_target(ID3D11RenderTargetView*,ID3D11DepthStencilView*,UINT width,UINT height);
    HRESULT set_viewport(const Viewport&);
    HRESULT set_texture(ID3D11ShaderResourceView*);
    HRESULT upload_texture(const compat::TextureUpload&,ID3D11ShaderResourceView**);
    HRESULT upload_compressed_texture(const textures::CompressedImage&,
                    ID3D11ShaderResourceView**,std::string& error);
    // Full Clear semantics are a later slice. This call implements D3D8 TARGET
    // clears clipped to the current viewport, including each screen-space rect.
    struct Rect {LONG left,top,right,bottom;};
    HRESULT clear_color(DWORD argb,UINT count=0,const Rect* rects=nullptr);
    HRESULT draw_up(UINT primitive_type,UINT primitive_count,DWORD fvf,
                    const void* vertices,std::size_t vertex_bytes,UINT stride,
                    const UiDrawState&);
    HRESULT draw_indexed_up(UINT primitive_type,UINT primitive_count,DWORD fvf,
                    const void* vertices,std::size_t vertex_bytes,UINT vertex_count,UINT stride,
                    const void* indices,std::size_t index_bytes,UINT index_format,
                    const UiDrawState&);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
