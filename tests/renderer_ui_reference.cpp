#include "renderer_ui_scene.hpp"
#include <d3d8.h>
#include <cstring>

namespace renderer_fixture {
namespace {
template<class T> struct Owned {
    T* p=nullptr;
    ~Owned(){if(p)p->Release();}
    T** out(){return &p;}
    T* operator->(){return p;}
};
}
HRESULT render_reference(HWND window,const Scene& s,std::vector<std::uint8_t>& rgba,std::string& error) {
    Owned<IDirect3D8> factory;
    factory.p=Direct3DCreate8(D3D_SDK_VERSION);
    if(!factory.p)return E_FAIL;
    Owned<IDirect3DDevice8> device;
    D3DPRESENT_PARAMETERS p{};
    p.BackBufferWidth=p.BackBufferHeight=64;
    p.BackBufferCount=1;
    p.BackBufferFormat=D3DFMT_X8R8G8B8;
    p.SwapEffect=D3DSWAPEFFECT_DISCARD;
    p.hDeviceWindow=window;p.Windowed=TRUE;p.Flags=D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    p.EnableAutoDepthStencil=TRUE;p.AutoDepthStencilFormat=D3DFMT_D24S8;
    HRESULT hr=factory->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&p,device.out());
    if(FAILED(hr)){error="reference CreateDevice";return hr;}
    auto check=[&](HRESULT value,const char* operation){if(FAILED(value))error=operation;return value;};
#define REF(call) do{hr=check((call),#call);if(FAILED(hr))return hr;}while(false)
    D3DVIEWPORT8 full{0,0,64,64,0,1};
    REF(device->SetViewport(&full));
    REF(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,s.background,1,0));
    D3DVIEWPORT8 view{s.viewport.x,s.viewport.y,s.viewport.width,s.viewport.height,s.viewport.min_z,s.viewport.max_z};
    REF(device->SetViewport(&view));
    if(!s.clear_rectangles.empty()) {
        std::vector<D3DRECT> rectangles;
        for(const auto& r:s.clear_rectangles)rectangles.push_back({r.left,r.top,r.right,r.bottom});
        REF(device->Clear(static_cast<DWORD>(rectangles.size()),rectangles.data(),D3DCLEAR_TARGET,s.clear_color,0,0));
    }
    const auto& st=s.state;
    REF(device->SetRenderState(D3DRS_LIGHTING,FALSE));
    REF(device->SetRenderState(D3DRS_FOGENABLE,FALSE));
    REF(device->SetRenderState(D3DRS_CULLMODE,st.cull));
    REF(device->SetRenderState(D3DRS_ZENABLE,st.depth_enable));
    REF(device->SetRenderState(D3DRS_ZWRITEENABLE,st.depth_write));
    REF(device->SetRenderState(D3DRS_ZFUNC,st.depth_func));
    REF(device->SetRenderState(D3DRS_ALPHATESTENABLE,st.alpha_test));
    REF(device->SetRenderState(D3DRS_ALPHAFUNC,st.alpha_func));
    REF(device->SetRenderState(D3DRS_ALPHAREF,st.alpha_ref));
    REF(device->SetRenderState(D3DRS_ALPHABLENDENABLE,st.alpha_blend));
    REF(device->SetRenderState(D3DRS_SRCBLEND,st.src_blend));
    REF(device->SetRenderState(D3DRS_DESTBLEND,st.dst_blend));
    REF(device->SetRenderState(D3DRS_BLENDOP,st.blend_op));
    REF(device->SetRenderState(D3DRS_COLORWRITEENABLE,st.color_write_mask));
    REF(device->SetRenderState(D3DRS_SPECULARENABLE,st.specular_enable));
    REF(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
    REF(device->SetTextureStageState(0,D3DTSS_COLOROP,st.color_op));
    REF(device->SetTextureStageState(0,D3DTSS_COLORARG1,st.color_arg1));
    REF(device->SetTextureStageState(0,D3DTSS_COLORARG2,st.color_arg2));
    REF(device->SetTextureStageState(0,D3DTSS_ALPHAOP,st.alpha_op));
    REF(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,st.alpha_arg1));
    REF(device->SetTextureStageState(0,D3DTSS_ALPHAARG2,st.alpha_arg2));
    REF(device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0));
    REF(device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));
    REF(device->SetTextureStageState(0,D3DTSS_MINFILTER,st.min_filter));
    REF(device->SetTextureStageState(0,D3DTSS_MAGFILTER,st.mag_filter));
    REF(device->SetTextureStageState(0,D3DTSS_MIPFILTER,st.mip_filter));
    REF(device->SetTextureStageState(0,D3DTSS_ADDRESSU,st.address_u));
    REF(device->SetTextureStageState(0,D3DTSS_ADDRESSV,st.address_v));
    Owned<IDirect3DTexture8> texture;
    if(!s.texture.empty()) {
        REF(device->CreateTexture(s.texture_width,s.texture_height,1,0,static_cast<D3DFORMAT>(s.texture_format),D3DPOOL_MANAGED,texture.out()));
        D3DLOCKED_RECT locked{};REF(texture->LockRect(0,&locked,nullptr,0));
        const bool compressed=s.texture_format==D3DFMT_DXT1 || s.texture_format==D3DFMT_DXT3 || s.texture_format==D3DFMT_DXT5;
        const UINT rows=compressed ? (s.texture_height+3)/4 : s.texture_height;
        if(s.texture_pitch>static_cast<UINT>(locked.Pitch)){texture->UnlockRect(0);error="reference texture pitch";return E_INVALIDARG;}
        for(UINT y=0;y<rows;++y)
            std::memcpy(static_cast<std::uint8_t*>(locked.pBits)+y*locked.Pitch,s.texture.data()+y*s.texture_pitch,s.texture_pitch);
        REF(texture->UnlockRect(0));
    }
    REF(device->SetTexture(0,texture.p));
    REF(device->SetVertexShader(s.fvf));REF(device->SetPixelShader(0));
    if(s.primitive_count) {
        REF(device->BeginScene());
        if(s.indices.empty()) {
            REF(device->DrawPrimitiveUP(static_cast<D3DPRIMITIVETYPE>(s.primitive),s.primitive_count,s.vertices.data(),s.stride));
        } else {
            REF(device->DrawIndexedPrimitiveUP(static_cast<D3DPRIMITIVETYPE>(s.primitive),0,s.vertex_count,s.primitive_count,s.indices.data(),static_cast<D3DFORMAT>(s.index_format),s.vertices.data(),s.stride));
        }
        REF(device->EndScene());
    }
    Owned<IDirect3DSurface8> back;
    REF(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,back.out()));
    D3DLOCKED_RECT locked{};REF(back->LockRect(&locked,nullptr,D3DLOCK_READONLY));
    rgba.resize(64*64*4);
    for(UINT y=0;y<64;++y)for(UINT x=0;x<64;++x) {
        DWORD pixel=0;std::memcpy(&pixel,static_cast<const std::uint8_t*>(locked.pBits)+y*locked.Pitch+x*4,4);
        auto* destination=rgba.data()+(y*64+x)*4;
        destination[0]=static_cast<std::uint8_t>(pixel>>16);destination[1]=static_cast<std::uint8_t>(pixel>>8);
        destination[2]=static_cast<std::uint8_t>(pixel);destination[3]=255;
    }
    REF(back->UnlockRect());
#undef REF
    return S_OK;
}
}
