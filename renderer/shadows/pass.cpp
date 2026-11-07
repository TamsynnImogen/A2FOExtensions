#include "pass.hpp"
#include "math.hpp"
#include "shader.hpp"
#include <windows.h>
#include <initguid.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

namespace a2fo::shadows {
namespace {
template<class T> struct Ref {
    T* p = nullptr;
    Ref() = default;
    ~Ref() { clear(); }
    Ref(const Ref&) = delete;
    Ref& operator=(const Ref&) = delete;
    Ref(Ref&& other) noexcept : p(other.p) { other.p = nullptr; }
    Ref& operator=(Ref&& other) noexcept {
        if (this != &other) { clear(); p = other.p; other.p = nullptr; }
        return *this;
    }
    T** out() { clear(); return &p; }
    void clear() { if (p) p->Release(); p = nullptr; }
    T* operator->() const { return p; }
};
constexpr D3DVERTEXELEMENT9 declaration[] = {
    {0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
    D3DDECL_END()
};
using Compile = pD3DCompile;
bool same(float a, float b) { return std::isfinite(a) && std::isfinite(b) &&
    std::abs(a-b) <= 1e-4f * std::max({1.0f,std::abs(a),std::abs(b)}); }
bool same_matrix(const D3DMATRIX& a,const D3DMATRIX& b) {
    return std::memcmp(&a,&b,sizeof(a))==0;
}
Mat4 matrix(const float* p) { Mat4 m{}; std::memcpy(m.v.data(),p,sizeof(float)*16); return m; }
bool valid_settings(Settings s) { return s.size>=512 && s.size<=4096 && !(s.size&(s.size-1)) &&
    s.max_draws>0 && s.max_draws<=65536 && std::isfinite(s.strength) && s.strength>=0 &&
    s.strength<=1 && std::isfinite(s.bias) && s.bias>=0 && s.bias<=0.1f &&
    std::isfinite(s.bias_world) && (s.bias_world==-1.0f ||
        (s.bias_world>=0 && s.bias_world<=100.0f)); }
bool valid_bounds(const Capture& c,Bounds& b) {
    for(unsigned i=0;i<3;++i) if(!std::isfinite(c.bounds_center[i]) ||
        !std::isfinite(c.bounds_extent[i]) || c.bounds_extent[i]<=0 ||
        !std::isfinite(c.light_direction[i])) return false;
    b.low={c.bounds_center[0]-c.bounds_extent[0],c.bounds_center[1]-c.bounds_extent[1],
           c.bounds_center[2]-c.bounds_extent[2]};
    b.high={c.bounds_center[0]+c.bounds_extent[0],c.bounds_center[1]+c.bounds_extent[1],
            c.bounds_center[2]+c.bounds_extent[2]};
    b.valid=finite(b.low)&&finite(b.high);
    return b.valid;
}
constexpr unsigned max_view_vertices=786432;
bool set_render(IDirect3DDevice9* d,D3DRENDERSTATETYPE key,DWORD value) {
    return SUCCEEDED(d->SetRenderState(key,value));
}
bool setup_common(IDirect3DDevice9* d) {
    return set_render(d,D3DRS_ZENABLE,TRUE) && set_render(d,D3DRS_ZFUNC,D3DCMP_LESSEQUAL) &&
      set_render(d,D3DRS_DEPTHBIAS,0) && set_render(d,D3DRS_SLOPESCALEDEPTHBIAS,0) &&
      set_render(d,D3DRS_FILLMODE,D3DFILL_SOLID) &&
      set_render(d,D3DRS_CULLMODE,D3DCULL_NONE) && set_render(d,D3DRS_STENCILENABLE,FALSE) &&
      set_render(d,D3DRS_ALPHATESTENABLE,FALSE) && set_render(d,D3DRS_FOGENABLE,FALSE) &&
      set_render(d,D3DRS_SCISSORTESTENABLE,FALSE) && set_render(d,D3DRS_CLIPPING,TRUE) &&
      set_render(d,D3DRS_CLIPPLANEENABLE,0) && set_render(d,D3DRS_LIGHTING,FALSE) &&
      set_render(d,D3DRS_MULTISAMPLEMASK,0xffffffffu) &&
      set_render(d,D3DRS_MULTISAMPLEANTIALIAS,FALSE) &&
      set_render(d,D3DRS_BLENDOP,D3DBLENDOP_ADD) &&
      set_render(d,D3DRS_SEPARATEALPHABLENDENABLE,FALSE) &&
      set_render(d,D3DRS_SRGBWRITEENABLE,FALSE) && set_render(d,D3DRS_COLORWRITEENABLE,0xf);
}
bool set_matrix(IDirect3DDevice9* d,UINT index,Mat4 m) {
    return SUCCEEDED(d->SetVertexShaderConstantF(index,m.v.data(),4));
}
bool compile_shader(Compile compile,const char* source,const char* profile,Ref<ID3DBlob>& blob) {
    Ref<ID3DBlob> errors;
    return SUCCEEDED(compile(source,std::strlen(source),nullptr,nullptr,nullptr,"main",profile,
                             0,0,blob.out(),errors.out())) && blob.p;
}
} // namespace

struct Pass::Impl {
    struct Record {
        std::vector<float> positions;
        Draw draw{};
        Mat4 local_to_world{};
        Mat4 native_clip{};
    };
    Settings settings{};
    char message[256]="not attempted";
    char capture_message[384]="no captures";
    bool blocked=false, aborted=false;
    Ref<IDirect3DDevice9> device;
    Ref<IDirect3DSurface9> target,depth;
    D3DVIEWPORT9 viewport{};
    D3DMATRIX view{},projection{};
    Vec3 light{};
    Bounds union_bounds{};
    std::vector<Record> records;
    unsigned view_vertices=0;
    Ref<IDirect3DTexture9> map;
    Ref<IDirect3DDevice9> resource_device;
    Ref<IDirect3DSurface9> map_surface,map_render_surface;
    Ref<IDirect3DVertexDeclaration9> decl;
    Ref<IDirect3DVertexShader9> vs;
    Ref<IDirect3DPixelShader9> depth_ps,overlay_ps;
    unsigned resource_size=0;
    D3DMULTISAMPLE_TYPE resource_samples=D3DMULTISAMPLE_NONE;
    DWORD resource_quality=0;
    D3DFORMAT resource_depth_format=D3DFMT_UNKNOWN;
#ifdef A2FO_SHADOW_TEST_PROBE
    Mat4 debug_uv[2]{};
    unsigned debug_count=0;
#endif
    void note(const char* text) { std::snprintf(message,sizeof(message),"%s",text); }
    void fail(const char* text) { note(text); blocked=true; }
    void clear_view() { records.clear(); view_vertices=0; target.clear(); depth.clear();
        device.clear(); union_bounds={}; }
    bool prepare() {
        D3DSURFACE_DESC native_depth{};
        if(!depth.p || FAILED(depth->GetDesc(&native_depth))) {
            fail("native depth description unavailable");return false;
        }
        // Never unbind the game's depth surface: discard-enabled surfaces lose
        // their CONTENTS on a depth-surface switch, even if the pointer is restored.
        // A MIN-blended colour target finds the nearest light depth without Z.
        unsigned size=settings.size;
        while(size>native_depth.Width || size>native_depth.Height) size/=2;
        if(size<512) {fail("native depth surface smaller than minimum shadow map");return false;}
        if(map.p && resource_size==size && resource_device.p==device.p &&
            resource_samples==native_depth.MultiSampleType &&
            resource_quality==native_depth.MultiSampleQuality &&
            resource_depth_format==native_depth.Format) return true;
        map.clear();map_surface.clear();map_render_surface.clear();decl.clear();vs.clear();
        depth_ps.clear();overlay_ps.clear();resource_device.clear();resource_size=0;
        D3DCAPS9 caps{};
        if(FAILED(device->GetDeviceCaps(&caps)) || caps.VertexShaderVersion<D3DVS_VERSION(3,0) ||
           caps.PixelShaderVersion<D3DPS_VERSION(3,0) || caps.NumSimultaneousRTs>4 ||
           !(caps.PrimitiveMiscCaps&D3DPMISCCAPS_BLENDOP)) {
            fail("SM3 or render-target capabilities unavailable");return false;
        }
        HMODULE lib=LoadLibraryA("d3dcompiler_47.dll");
        if(!lib) lib=LoadLibraryA("d3dcompiler_43.dll");
        if(!lib) { fail("D3DCompile unavailable");return false; }
        FARPROC proc=GetProcAddress(lib,"D3DCompile");
        Compile compile=nullptr;
        static_assert(sizeof(proc)==sizeof(compile));
        std::memcpy(&compile,&proc,sizeof(proc));
        Ref<ID3DBlob> vb,db,ob;
        const bool compiled=compile && compile_shader(compile,shader::vertex,"vs_3_0",vb) &&
           compile_shader(compile,shader::depth_pixel,"ps_3_0",db) &&
           compile_shader(compile,shader::overlay_pixel,"ps_3_0",ob);
        if(!compiled) {
            vb.clear();db.clear();ob.clear();FreeLibrary(lib);
            fail("shadow shader compilation failed");return false;
        }
        const bool created=SUCCEEDED(device->CreateVertexDeclaration(declaration,decl.out())) &&
           SUCCEEDED(device->CreateVertexShader(static_cast<const DWORD*>(vb->GetBufferPointer()),vs.out())) &&
           SUCCEEDED(device->CreatePixelShader(static_cast<const DWORD*>(db->GetBufferPointer()),depth_ps.out())) &&
           SUCCEEDED(device->CreatePixelShader(static_cast<const DWORD*>(ob->GetBufferPointer()),overlay_ps.out()));
        vb.clear();db.clear();ob.clear();FreeLibrary(lib);
        if(!created) {
            fail("shadow shader creation failed");return false;
        }
        if(FAILED(device->CreateTexture(size,size,1,D3DUSAGE_RENDERTARGET,
              D3DFMT_R32F,D3DPOOL_DEFAULT,map.out(),nullptr)) ||
           FAILED(map->GetSurfaceLevel(0,map_surface.out()))) {
            fail("R32F shadow target unavailable");return false;
        }
        Ref<IDirect3D9> factory;
        D3DDEVICE_CREATION_PARAMETERS cp{};
        D3DDISPLAYMODE mode{};
        if(FAILED(device->GetDirect3D(factory.out())) ||
           FAILED(device->GetCreationParameters(&cp)) ||
           FAILED(factory->GetAdapterDisplayMode(cp.AdapterOrdinal,&mode))) {
            fail("shadow target compatibility query failed");return false;
        }
        if(FAILED(factory->CheckDeviceFormat(cp.AdapterOrdinal,cp.DeviceType,mode.Format,
                D3DUSAGE_RENDERTARGET|D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                D3DRTYPE_TEXTURE,D3DFMT_R32F)) ||
           FAILED(factory->CheckDepthStencilMatch(cp.AdapterOrdinal,cp.DeviceType,
                mode.Format,D3DFMT_R32F,native_depth.Format))) {
            fail("R32F minimum blend or native-depth pairing unavailable");return false;
        }
        if(native_depth.MultiSampleType==D3DMULTISAMPLE_NONE) {
            map_render_surface.p=map_surface.p;map_render_surface.p->AddRef();
        } else if(FAILED(device->CreateRenderTarget(size,size,D3DFMT_R32F,
                native_depth.MultiSampleType,native_depth.MultiSampleQuality,FALSE,
                map_render_surface.out(),nullptr))) {
            fail("native-MSAA-compatible R32F shadow target unavailable");return false;
        }
        resource_device.p=device.p;
        resource_device.p->AddRef();
        resource_size=size;resource_samples=native_depth.MultiSampleType;
        resource_quality=native_depth.MultiSampleQuality;resource_depth_format=native_depth.Format;
        return true;
    }
};

Pass::Pass():impl_(new(std::nothrow) Impl){}
Pass::~Pass(){release();delete impl_;}
void Pass::configure(Settings s) noexcept {
    if(!impl_)return;
    if(!valid_settings(s)) { impl_->settings.enabled=false;impl_->note("invalid shadow settings");return; }
    if(s.size!=impl_->settings.size) { impl_->map.clear();impl_->map_surface.clear();
        impl_->map_render_surface.clear();impl_->resource_size=0; }
    impl_->settings=s;
}
void Pass::begin_view() noexcept {
    if(!impl_)return;
    impl_->clear_view();impl_->aborted=false;
    std::snprintf(impl_->capture_message,sizeof(impl_->capture_message),"no captures");
}
bool Pass::capture(void* device8,const Capture& c,const Draw& draw,
                   const float* triangle_positions,unsigned vertex_count) noexcept {
    auto* s=impl_;
    if(!s || !s->settings.enabled || s->blocked || s->aborted || !device8)return false;
    if(s->records.size()>=s->settings.max_draws) {
        s->aborted=true;s->note("shadow draw cap exceeded");s->clear_view();return false;
    }
    if(!triangle_positions || draw.primitive_count==0 ||
       draw.primitive_count>max_view_vertices/3 ||
       vertex_count!=draw.primitive_count*3) {
        s->note("invalid shadow triangle snapshot");return false;
    }
    if(vertex_count>max_view_vertices-s->view_vertices) {
        s->aborted=true;s->note("shadow vertex cap exceeded");s->clear_view();return false;
    }
    for(unsigned i=0;i<vertex_count*3;++i) if(!std::isfinite(triangle_positions[i])) {
        s->note("nonfinite shadow triangle snapshot");return false;
    }
    Bounds local{};
    if(!valid_bounds(c,local)) {s->note("invalid shadow bounds");return false;}
    Ref<IDirect3DDevice9> device;
    if(FAILED(static_cast<IUnknown*>(device8)->QueryInterface(IID_IDirect3DDevice9,
         reinterpret_cast<void**>(device.out()))) || !device.p) {
        s->note("D3D9 device unavailable");return false;
    }
    Impl::Record record{};
    D3DMATRIX view{},projection{};D3DVIEWPORT9 viewport{};
    Ref<IDirect3DSurface9> target,depth;
    if(FAILED(device->GetTransform(D3DTS_VIEW,&view)) ||
       FAILED(device->GetTransform(D3DTS_PROJECTION,&projection)) ||
       FAILED(device->GetViewport(&viewport)) ||
       FAILED(device->GetRenderTarget(0,target.out())) || !target.p ||
       FAILED(device->GetDepthStencilSurface(depth.out())) || !depth.p) {
        s->note("shadow capture state unavailable");return false;
    }
    if(!s->records.empty()) {
        if(s->device.p!=device.p || s->target.p!=target.p || s->depth.p!=depth.p ||
           std::memcmp(&s->viewport,&viewport,sizeof(viewport)) ||
           !same_matrix(s->view,view) || !same_matrix(s->projection,projection) ||
           !same(s->light.x,c.light_direction[0]) ||
           !same(s->light.y,c.light_direction[1]) ||
           !same(s->light.z,c.light_direction[2])) {
            s->aborted=true;s->note("shadow view state changed");s->clear_view();return false;
        }
    }
    record.native_clip=matrix(c.clip);
    Bounds world{};
    if(!infer_affine_transform(
            transpose(matrix(reinterpret_cast<const float*>(&projection))),
            transpose(matrix(reinterpret_cast<const float*>(&view))),
            record.native_clip,record.local_to_world) ||
       !transformed_bounds(local,record.local_to_world,world)) {
        s->note("shadow local transform invalid");return false;
    }
    if(s->records.empty()) {
        s->device=std::move(device);s->target=std::move(target);s->depth=std::move(depth);
        s->view=view;s->projection=projection;s->viewport=viewport;
        s->light={c.light_direction[0],c.light_direction[1],c.light_direction[2]};
    }
    include(s->union_bounds,world.low);include(s->union_bounds,world.high);
    std::snprintf(s->capture_message,sizeof(s->capture_message),
        "origin=(%.3f,%.3f,%.3f) basis=(%.4f,%.4f,%.4f;%.4f,%.4f,%.4f;%.4f,%.4f,%.4f) bounds=(%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f) key=(%.4f,%.4f,%.4f)",
        record.local_to_world.v[3],record.local_to_world.v[7],record.local_to_world.v[11],
        record.local_to_world.v[0],record.local_to_world.v[1],record.local_to_world.v[2],
        record.local_to_world.v[4],record.local_to_world.v[5],record.local_to_world.v[6],
        record.local_to_world.v[8],record.local_to_world.v[9],record.local_to_world.v[10],
        world.low.x,world.low.y,world.low.z,world.high.x,world.high.y,world.high.z,
        s->light.x,s->light.y,s->light.z);
    record.draw=draw;
    try {
        record.positions.assign(triangle_positions,triangle_positions+vertex_count*3);
        s->records.push_back(std::move(record));
    }
    catch(...) {s->aborted=true;s->clear_view();s->note("shadow capture allocation failed");return false;}
    s->view_vertices+=vertex_count;
    s->note("shadow draw captured");return true;
}

bool Pass::finish_view() noexcept {
    auto* s=impl_;
    if(!s || s->blocked || s->aborted || s->records.empty() || !s->settings.enabled) {
        if(s) s->clear_view();
        return false;
    }
    Mat4 light_clip{},light_uv{};
    if(!s->prepare()) {s->clear_view();return false;}
    if(!fit_shadow(s->union_bounds,s->light,s->resource_size,light_clip,light_uv)) {
        s->note("shadow light fitting failed");s->clear_view();return false;
    }
    const float width=2.0f/std::sqrt(light_clip.v[0]*light_clip.v[0]+
        light_clip.v[1]*light_clip.v[1]+light_clip.v[2]*light_clip.v[2]);
    const float range=1.0f/std::sqrt(light_clip.v[8]*light_clip.v[8]+
        light_clip.v[9]*light_clip.v[9]+light_clip.v[10]*light_clip.v[10]);
    const float comparison_bias=s->settings.bias_world>=0 ?
        s->settings.bias_world/range : s->settings.bias;
    std::snprintf(s->capture_message,sizeof(s->capture_message),
        "draws=%u map=%u samples=%u width=%.3f depth-range=%.3f bias-world=%.4f bias-mode=%s bounds=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f) key=(%.4f,%.4f,%.4f)",
        static_cast<unsigned>(s->records.size()),s->resource_size,
        static_cast<unsigned>(s->resource_samples),width,range,comparison_bias*range,
        s->settings.bias_world>=0 ? "world" : "normalized",
        s->union_bounds.low.x,s->union_bounds.low.y,s->union_bounds.low.z,
        s->union_bounds.high.x,s->union_bounds.high.y,s->union_bounds.high.z,
        s->light.x,s->light.y,s->light.z);
#ifdef A2FO_SHADOW_TEST_PROBE
    s->debug_count=static_cast<unsigned>(std::min<std::size_t>(2,s->records.size()));
    for(unsigned i=0;i<s->debug_count;++i)
        s->debug_uv[i]=multiply(light_uv,s->records[i].local_to_world);
#endif
    auto* d=s->device.p;
    Ref<IDirect3DStateBlock9> state;
    D3DCAPS9 caps{};
    Ref<IDirect3DSurface9> targets[4],depth;
    D3DVIEWPORT9 viewport{};
    if(FAILED(d->GetDeviceCaps(&caps)) || caps.NumSimultaneousRTs<1 ||
       caps.NumSimultaneousRTs>4 || caps.MaxStreams<1 || caps.MaxStreams>16 ||
       FAILED(d->CreateStateBlock(D3DSBT_ALL,state.out())) || !state.p ||
       FAILED(state->Capture()) || FAILED(d->GetViewport(&viewport)) ||
       FAILED(d->GetDepthStencilSurface(depth.out()))) {
        s->fail("shadow state capture failed");s->clear_view();return false;
    }
    bool captured=true;
    for(UINT i=0;i<caps.NumSimultaneousRTs;++i)
        if(FAILED(d->GetRenderTarget(i,targets[i].out())) && i==0) captured=false;
    if(!captured || !targets[0].p) {
        s->fail("shadow render target capture failed");s->clear_view();return false;
    }
    D3DMATRIX current_view{},current_projection{};
    if(targets[0].p!=s->target.p || depth.p!=s->depth.p ||
       std::memcmp(&viewport,&s->viewport,sizeof(viewport)) ||
       FAILED(d->GetTransform(D3DTS_VIEW,&current_view)) ||
       FAILED(d->GetTransform(D3DTS_PROJECTION,&current_projection)) ||
       !same_matrix(current_view,s->view) || !same_matrix(current_projection,s->projection)) {
        s->note("shadow view ended with changed target or camera");s->clear_view();return false;
    }
    bool ok=true;
    // D3DSBT_ALL captures viewport and stream frequency, but not render targets.
    for(UINT i=1;i<caps.NumSimultaneousRTs;++i) ok &= SUCCEEDED(d->SetRenderTarget(i,nullptr));
    ok &= SUCCEEDED(d->SetRenderTarget(0,s->map_render_surface.p));
    D3DVIEWPORT9 shadow_view{0,0,s->resource_size,s->resource_size,0.0f,1.0f};
    ok &= SUCCEEDED(d->SetViewport(&shadow_view));
    if(ok)ok &= SUCCEEDED(d->Clear(0,nullptr,D3DCLEAR_TARGET,
                                  D3DCOLOR_XRGB(255,255,255),1.0f,0));
    if(ok)ok &= setup_common(d) && set_render(d,D3DRS_ZENABLE,FALSE) &&
        set_render(d,D3DRS_ZWRITEENABLE,FALSE) &&
        set_render(d,D3DRS_ALPHABLENDENABLE,TRUE) &&
        set_render(d,D3DRS_SRCBLEND,D3DBLEND_ONE) &&
        set_render(d,D3DRS_DESTBLEND,D3DBLEND_ONE) &&
        set_render(d,D3DRS_BLENDOP,D3DBLENDOP_MIN) &&
        SUCCEEDED(d->SetTexture(0,nullptr)) &&
        SUCCEEDED(d->SetVertexDeclaration(s->decl.p)) &&
        SUCCEEDED(d->SetVertexShader(s->vs.p)) &&
        SUCCEEDED(d->SetPixelShader(s->depth_ps.p));
    // User-memory draws use stream zero only; clear inherited instancing dividers.
    if(ok) for(UINT i=0;i<caps.MaxStreams;++i)
        if(FAILED(d->SetStreamSourceFreq(i,1))) {ok=false;break;}
    if(ok) for(const auto& record:s->records) {
        Mat4 shadow=multiply(light_clip,record.local_to_world);
        ok=set_matrix(d,0,shadow) && set_matrix(d,4,shadow) &&
           SUCCEEDED(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,record.draw.primitive_count,
                record.positions.data(),3*sizeof(float)));
        if(!ok)break;
    }
    if(ok) {
        ok=SUCCEEDED(d->SetRenderTarget(0,targets[0].p));
        if(ok && s->resource_samples!=D3DMULTISAMPLE_NONE)
            ok=SUCCEEDED(d->StretchRect(s->map_render_surface.p,nullptr,
                s->map_surface.p,nullptr,D3DTEXF_NONE));
        ok=ok &&
           SUCCEEDED(d->SetViewport(&viewport)) && setup_common(d) &&
           set_render(d,D3DRS_ZWRITEENABLE,FALSE) &&
           set_render(d,D3DRS_ALPHABLENDENABLE,TRUE) &&
           set_render(d,D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_RED|
                      D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE) &&
           set_render(d,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA) &&
           set_render(d,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA) &&
           SUCCEEDED(d->SetPixelShader(s->overlay_ps.p)) &&
           SUCCEEDED(d->SetTexture(0,s->map.p)) &&
           SUCCEEDED(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT)) &&
           SUCCEEDED(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT)) &&
           SUCCEEDED(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE)) &&
           SUCCEEDED(d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE)) &&
           SUCCEEDED(d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP)) &&
           SUCCEEDED(d->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP));
        const float options[4]{s->settings.strength,comparison_bias,
                               1.0f/float(s->resource_size),0};
        if(ok)ok=SUCCEEDED(d->SetPixelShaderConstantF(0,options,1));
    }
    if(ok) for(const auto& record:s->records) {
        Mat4 uv=multiply(light_uv,record.local_to_world);
        ok=set_matrix(d,0,record.native_clip) && set_matrix(d,4,uv) &&
           SUCCEEDED(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,record.draw.primitive_count,
                record.positions.data(),3*sizeof(float)));
        if(!ok)break;
    }
    bool restored=true;
    for(UINT i=1;i<caps.NumSimultaneousRTs;++i)
        restored &= SUCCEEDED(d->SetRenderTarget(i,nullptr));
    restored &= SUCCEEDED(d->SetRenderTarget(0,targets[0].p));
    for(UINT i=1;i<caps.NumSimultaneousRTs;++i)
        restored &= SUCCEEDED(d->SetRenderTarget(i,targets[i].p));
    restored &= SUCCEEDED(d->SetViewport(&viewport));
    restored &= SUCCEEDED(state->Apply());
    if(!restored) s->fail("shadow state restoration failed");
    else if(!ok) s->fail("shadow GPU pass failed");
    else s->note("shadow depth and overlay drawn");
    s->clear_view();
    return ok && restored;
}
void Pass::release() noexcept {
    if(!impl_)return;
    impl_->clear_view();
    impl_->map.clear();impl_->map_surface.clear();impl_->map_render_surface.clear();
    impl_->resource_device.clear();
    impl_->decl.clear();impl_->vs.clear();impl_->depth_ps.clear();impl_->overlay_ps.clear();
    impl_->resource_size=0;impl_->blocked=false;impl_->aborted=false;
}
const char* Pass::diagnostic() const noexcept {return impl_?impl_->message:"shadow allocation failed";}
const char* Pass::capture_diagnostic() const noexcept {
    return impl_?impl_->capture_message:"shadow allocation failed";
}
#ifdef A2FO_SHADOW_TEST_PROBE
bool Pass::probe(unsigned record,const float point[3],float out[4]) noexcept {
    auto* s=impl_;if(!s || record>=s->debug_count || !s->map_surface.p)return false;
    Vec3 p{};if(!transform(s->debug_uv[record],{point[0],point[1],point[2]},p))return false;
    out[0]=p.x;out[1]=p.y;out[2]=p.z;
    Ref<IDirect3DSurface9> copy;
    if(FAILED(s->resource_device->CreateOffscreenPlainSurface(s->resource_size,s->resource_size,
        D3DFMT_R32F,D3DPOOL_SYSTEMMEM,copy.out(),nullptr)) ||
        FAILED(s->resource_device->GetRenderTargetData(s->map_surface.p,copy.p)))return false;
    D3DLOCKED_RECT lock{};if(FAILED(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY)))return false;
    const unsigned x=static_cast<unsigned>(std::clamp(p.x*float(s->resource_size),0.f,float(s->resource_size-1)));
    const unsigned y=static_cast<unsigned>(std::clamp(p.y*float(s->resource_size),0.f,float(s->resource_size-1)));
    std::memcpy(out+3,static_cast<const unsigned char*>(lock.pBits)+y*lock.Pitch+x*4,4);
    copy->UnlockRect();return true;
}
#endif
} // namespace a2fo::shadows
