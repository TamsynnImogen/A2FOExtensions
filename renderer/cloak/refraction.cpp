#include "refraction.hpp"
#include "refraction_shader.hpp"
#include "../../rust/refraction_ffi.h"
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <new>

namespace a2fo::cloak {
namespace {
constexpr D3DVERTEXELEMENT9 declaration[] = {
    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
    {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
    {0, 32, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
    {0, 44, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},
    {0, 56, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3},
    D3DDECL_END()
};
}
struct NativeContext {
    A2FoRefractState state{};
    char message[512] = "not attempted";
    void fail(const char* phase, HRESULT hr) noexcept {
        std::snprintf(message, sizeof(message), "%s: HRESULT=0x%08lx", phase,
                      static_cast<unsigned long>(hr));
    }
};
namespace {
int native_call(void* raw, std::uint32_t operation, const A2FoRefractRequest* q,
                A2FoRefractReply* r) noexcept {
    auto& s = *static_cast<NativeContext*>(raw);
    auto* device = static_cast<IDirect3DDevice9*>(s.state.device);
    try {
        switch (operation) {
        case A2FO_REF_QI: {
            IDirect3DDevice9* found = nullptr;
            r->hr = static_cast<IUnknown*>(q->first)->QueryInterface(
                IID_IDirect3DDevice9, reinterpret_cast<void**>(&found));
            r->handle = found; break;
        }
        case A2FO_REF_CAPS: {
            D3DCAPS9 caps{}; r->hr = device->GetDeviceCaps(&caps);
            r->a = caps.VertexShaderVersion >= D3DVS_VERSION(3,0);
            r->b = caps.PixelShaderVersion >= D3DPS_VERSION(3,0); break;
        }
        case A2FO_REF_CREATE_DECL: {
            IDirect3DVertexDeclaration9* found = nullptr;
            r->hr = device->CreateVertexDeclaration(declaration, &found);
            r->handle = found; break;
        }
        case A2FO_REF_CREATE_VS: {
            IDirect3DVertexShader9* found = nullptr;
            auto* blob = static_cast<ID3DBlob*>(q->first);
            r->hr = device->CreateVertexShader(
                static_cast<const DWORD*>(blob->GetBufferPointer()), &found);
            r->handle = found; break;
        }
        case A2FO_REF_CREATE_PS: {
            IDirect3DPixelShader9* found = nullptr;
            auto* blob = static_cast<ID3DBlob*>(q->first);
            r->hr = device->CreatePixelShader(
                static_cast<const DWORD*>(blob->GetBufferPointer()), &found);
            r->handle = found; break;
        }
        case A2FO_REF_CREATE_TEX: {
            IDirect3DTexture9* found = nullptr;
            r->hr = device->CreateTexture(q->index, q->count, 1,
                D3DUSAGE_RENDERTARGET, static_cast<D3DFORMAT>(q->value),
                D3DPOOL_DEFAULT, &found, nullptr);
            r->handle = found; break;
        }
        case A2FO_REF_GET_STREAM: {
            IDirect3DVertexBuffer9* found = nullptr; UINT offset=0,stride=0;
            r->hr = device->GetStreamSource(0,&found,&offset,&stride);
            r->handle=found;r->a=offset;r->b=stride;break;
        }
        case A2FO_REF_GET_INDICES: {
            IDirect3DIndexBuffer9* found=nullptr;r->hr=device->GetIndices(&found);
            r->handle=found;break;
        }
        case A2FO_REF_GET_HULL: {
            IDirect3DBaseTexture9* found=nullptr;r->hr=device->GetTexture(0,&found);
            r->handle=found;break;
        }
        case A2FO_REF_GET_TARGET: {
            IDirect3DSurface9* found=nullptr;r->hr=device->GetRenderTarget(0,&found);
            r->handle=found;break;
        }
        case A2FO_REF_GET_VIEWPORT: {
            D3DVIEWPORT9 viewport{};r->hr=device->GetViewport(&viewport);
            r->a=viewport.X;r->b=viewport.Y;r->c=viewport.Width;r->d=viewport.Height;break;
        }
        case A2FO_REF_GET_DESC: {
            D3DSURFACE_DESC desc{};
            r->hr=static_cast<IDirect3DSurface9*>(q->first)->GetDesc(&desc);
            r->a=desc.Width;r->b=desc.Height;r->format=desc.Format;break;
        }
        case A2FO_REF_GET_SURFACE: {
            IDirect3DSurface9* found=nullptr;
            r->hr=static_cast<IDirect3DTexture9*>(q->first)->GetSurfaceLevel(0,&found);
            r->handle=found;break;
        }
        case A2FO_REF_STRETCH: {
            RECT src{q->left,q->top,q->right,q->bottom};
            RECT dst{0,0,static_cast<LONG>(q->index),static_cast<LONG>(q->count)};
            r->hr=device->StretchRect(static_cast<IDirect3DSurface9*>(q->first),&src,
                    static_cast<IDirect3DSurface9*>(q->second),&dst,D3DTEXF_NONE);
            break;
        }
        case A2FO_REF_CREATE_STATE: {
            IDirect3DStateBlock9* found=nullptr;
            r->hr=device->CreateStateBlock(D3DSBT_ALL,&found);r->handle=found;break;
        }
        case A2FO_REF_CAPTURE:
            r->hr=static_cast<IDirect3DStateBlock9*>(q->first)->Capture();break;
        case A2FO_REF_APPLY:
            r->hr=static_cast<IDirect3DStateBlock9*>(q->first)->Apply();break;
        case A2FO_REF_DRAW:
            r->hr=device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,q->left,q->value,
                   q->extra,q->right,q->count);break;
        case A2FO_REF_SET_DECL:
            r->hr=device->SetVertexDeclaration(static_cast<IDirect3DVertexDeclaration9*>(q->first));break;
        case A2FO_REF_SET_VS:
            r->hr=device->SetVertexShader(static_cast<IDirect3DVertexShader9*>(q->first));break;
        case A2FO_REF_SET_PS:
            r->hr=device->SetPixelShader(static_cast<IDirect3DPixelShader9*>(q->first));break;
        case A2FO_REF_SET_VSCONST:
            r->hr=device->SetVertexShaderConstantF(q->index,q->floats,q->count);break;
        case A2FO_REF_SET_PSCONST:
            r->hr=device->SetPixelShaderConstantF(q->index,q->floats,q->count);break;
        case A2FO_REF_SET_TEXTURE:
            r->hr=device->SetTexture(q->index,static_cast<IDirect3DBaseTexture9*>(q->first));break;
        case A2FO_REF_SET_SAMPLER: {
            const D3DSAMPLERSTATETYPE kind[] = {D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,
                                                D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV};
            const DWORD value[] = {D3DTEXF_LINEAR,D3DTEXF_LINEAR,
                                   D3DTADDRESS_CLAMP,D3DTADDRESS_CLAMP};
            r->hr=device->SetSamplerState(1,kind[q->index],value[q->index]);break;
        }
        case A2FO_REF_SET_RENDER:
            r->hr=device->SetRenderState(q->index==0?D3DRS_ALPHABLENDENABLE:
                                         D3DRS_ZWRITEENABLE,FALSE);break;
        case A2FO_REF_RELEASE:
            static_cast<IUnknown*>(q->first)->Release();break;
        case A2FO_REF_LOAD47:
            r->handle=LoadLibraryA("d3dcompiler_47.dll");break;
        case A2FO_REF_LOAD43:
            r->handle=LoadLibraryA("d3dcompiler_43.dll");break;
        case A2FO_REF_PROC: {
            FARPROC found=GetProcAddress(static_cast<HMODULE>(q->first),"D3DCompile");
            static_assert(sizeof(found)==sizeof(r->handle));
            std::memcpy(&r->handle,&found,sizeof(found));break;
        }
        case A2FO_REF_FREE: FreeLibrary(static_cast<HMODULE>(q->first));break;
        case A2FO_REF_COMPILE: {
            pD3DCompile fn=nullptr;std::memcpy(&fn,&q->first,sizeof(fn));
            const char* source=q->index==0?shader::vertex:shader::pixel;
            const char* profile=q->index==0?"vs_3_0":"ps_3_0";
            ID3DBlob *compiled=nullptr,*errors=nullptr;
            r->hr=fn(source,std::strlen(source),nullptr,nullptr,nullptr,"main",profile,
                     0,0,&compiled,&errors);
            r->handle=compiled;r->other=errors;break;
        }
        case A2FO_REF_MESSAGE_ERROR: {
            const char* phases[]={"","IDirect3DDevice9 QueryInterface","GetDeviceCaps",
              "Create SM3 shader/declaration","bound stream/index/texture/viewport",
              "Create scene copy","StretchRect scene copy/resolve","capture all D3D9 state",
              "restore all D3D9 state; call consumed",
              "replacement DrawIndexedPrimitive failed; call consumed",
              "replacement state setup","vs_3_0","ps_3_0"};
            s.fail(phases[q->index],static_cast<HRESULT>(q->value));
            if (q->index>=11 && q->first) {
                auto* blob=static_cast<ID3DBlob*>(q->first);
                if (blob->GetBufferPointer()) {
                    const char* detail=static_cast<const char*>(blob->GetBufferPointer());
                    const std::size_t used=std::strlen(s.message);
                    std::snprintf(s.message+used,sizeof(s.message)-used,": %.380s",detail);
                }
            }
            break;
        }
        case A2FO_REF_MESSAGE_TEXT: {
            const char* texts[]={"","invalid draw or parameters","SM3 unavailable",
                "D3DCompile unavailable (d3dcompiler_47/43)",
                "D3D9 QueryInterface, SM3 cloak draw and state restoration succeeded"};
            std::strcpy(s.message,texts[q->index]);break;
        }
        }
        return 1;
    } catch (...) {return 0;}
}
A2FoRefractParams parameters(const Parameters& p) {
    A2FoRefractParams out{};
    std::memcpy(out.clip,p.clip,sizeof(out.clip));
    std::memcpy(out.lights,p.lights,sizeof(out.lights));
    out.visibility=p.visibility;out.time=p.time;out.hull_floor=p.hull_floor;
    out.bend=p.bend;out.ripple=p.ripple;out.rim=p.rim;out.front=p.front;
    out.shimmer=p.shimmer;out.snapshot_key=p.snapshot_key;
    std::memcpy(out.bounds_center,p.bounds_center,sizeof(out.bounds_center));
    std::memcpy(out.bounds_extent,p.bounds_extent,sizeof(out.bounds_extent));
    return out;
}
}
struct Refraction::Impl : NativeContext {};
Refraction::Refraction():impl_(new(std::nothrow) Impl){}
Refraction::~Refraction(){release();delete impl_;}
void Refraction::release() noexcept {
    if (impl_) a2fo_rs_refraction_release(&impl_->state,static_cast<NativeContext*>(impl_),native_call);
}
const char* Refraction::diagnostic() const noexcept {
    return impl_?impl_->message:"allocation failed";
}
Outcome Refraction::draw(void* device8,const Parameters& p,const Draw& draw) noexcept {
    if (!impl_) return Outcome::fallback;
    const auto input=parameters(p);
    const A2FoRefractDraw command{draw.primitive_type,draw.base_vertex,draw.min_vertex,
                                  draw.num_vertices,draw.start_index,draw.primitive_count};
    return static_cast<Outcome>(a2fo_rs_refraction_draw(device8,&input,&command,
                               &impl_->state,static_cast<NativeContext*>(impl_),native_call));
}
} // namespace a2fo::cloak
