#include "../renderer/shadows/pass.hpp"
#include <windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace {
template<class T> struct Ref {
    T* p = nullptr;
    ~Ref() { if (p) p->Release(); }
    T** out() { return &p; }
    T* operator->() const { return p; }
};
void require(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
void hr(HRESULT result, const char* what) {
    if (FAILED(result)) { std::fprintf(stderr, "FAIL: %s (0x%08lx)\n", what,
                                       static_cast<unsigned long>(result)); std::exit(1); }
}
struct Vertex { float x, y, z; DWORD color; };
constexpr DWORD fvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;
constexpr D3DRENDERSTATETYPE states[] = {
    D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ZFUNC, D3DRS_ALPHABLENDENABLE,
    D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_CULLMODE, D3DRS_SRGBWRITEENABLE,
    D3DRS_STENCILENABLE, D3DRS_FOGENABLE, D3DRS_SCISSORTESTENABLE,
    D3DRS_COLORWRITEENABLE, D3DRS_LIGHTING, D3DRS_DEPTHBIAS, D3DRS_SLOPESCALEDEPTHBIAS,
    D3DRS_FILLMODE, D3DRS_CLIPPLANEENABLE, D3DRS_BLENDOP,
    D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_SRCBLENDALPHA, D3DRS_DESTBLENDALPHA,
    D3DRS_BLENDOPALPHA
};
constexpr D3DSAMPLERSTATETYPE sampler_states[] = {
    D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER,
    D3DSAMP_MIPFILTER, D3DSAMP_SRGBTEXTURE
};
DWORD float_bits(float value) {
    DWORD bits{};
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}
struct Snapshot {
    DWORD values[sizeof(states)/sizeof(states[0])]{};
    DWORD sampler_values[sizeof(sampler_states)/sizeof(sampler_states[0])]{};
    float clip_plane[4]{};
    DWORD active_fvf{};
    D3DVIEWPORT9 viewport{};
    Ref<IDirect3DSurface9> target, depth;
    Ref<IDirect3DVertexDeclaration9> declaration;
    Ref<IDirect3DVertexShader9> vertex_shader;
    Ref<IDirect3DPixelShader9> pixel_shader;
    Ref<IDirect3DVertexBuffer9> stream;
    Ref<IDirect3DIndexBuffer9> indices;
    UINT offset{}, stride{};
};
void snapshot(IDirect3DDevice9* d, Snapshot& s) {
    for (unsigned i=0; i<sizeof(states)/sizeof(states[0]); ++i)
        hr(d->GetRenderState(states[i], &s.values[i]), "GetRenderState");
    for (unsigned i=0; i<sizeof(sampler_states)/sizeof(sampler_states[0]); ++i)
        hr(d->GetSamplerState(0, sampler_states[i], &s.sampler_values[i]), "GetSamplerState");
    hr(d->GetClipPlane(0, s.clip_plane), "GetClipPlane");
    hr(d->GetFVF(&s.active_fvf), "GetFVF");
    hr(d->GetViewport(&s.viewport), "GetViewport");
    hr(d->GetRenderTarget(0, s.target.out()), "GetRenderTarget");
    hr(d->GetDepthStencilSurface(s.depth.out()), "GetDepthStencilSurface");
    hr(d->GetVertexDeclaration(s.declaration.out()), "GetVertexDeclaration");
    hr(d->GetVertexShader(s.vertex_shader.out()), "GetVertexShader");
    hr(d->GetPixelShader(s.pixel_shader.out()), "GetPixelShader");
    hr(d->GetStreamSource(0, s.stream.out(), &s.offset, &s.stride), "GetStreamSource");
    hr(d->GetIndices(s.indices.out()), "GetIndices");
}
void same_state(IDirect3DDevice9* d, const Snapshot& before) {
    Snapshot after{};
    snapshot(d, after);
    for (unsigned i=0; i<sizeof(states)/sizeof(states[0]); ++i)
        require(before.values[i] == after.values[i], "render state changed");
    for (unsigned i=0; i<sizeof(sampler_states)/sizeof(sampler_states[0]); ++i)
        require(before.sampler_values[i] == after.sampler_values[i], "sampler state changed");
    require(std::memcmp(before.clip_plane, after.clip_plane, sizeof(before.clip_plane)) == 0,
            "clip plane changed");
    require(before.active_fvf == after.active_fvf, "FVF changed");
    require(std::memcmp(&before.viewport, &after.viewport, sizeof(D3DVIEWPORT9)) == 0,
            "viewport changed");
    require(before.target.p == after.target.p && before.depth.p == after.depth.p,
            "target or depth changed");
    require(before.declaration.p == after.declaration.p &&
            before.vertex_shader.p == after.vertex_shader.p &&
            before.pixel_shader.p == after.pixel_shader.p, "shader/declaration changed");
    require(before.stream.p == after.stream.p && before.offset == after.offset &&
            before.stride == after.stride && before.indices.p == after.indices.p,
            "stream or index buffer changed");
}
void draw_quad(IDirect3DDevice9* d, float radius, float z) {
    const Vertex verts[6] = {
        {-radius,-radius,z,0xffffffff}, {-radius,radius,z,0xffffffff},
        {radius,radius,z,0xffffffff}, {-radius,-radius,z,0xffffffff},
        {radius,radius,z,0xffffffff}, {radius,-radius,z,0xffffffff}
    };
    hr(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, verts, sizeof(Vertex)), "baseline quad");
}
void draw_uncaptured_occluder(IDirect3DDevice9* d) {
    constexpr float x = .35f, half_width = .025f, half_height = .2f, z = .1f;
    const Vertex verts[6] = {
        {x-half_width,-half_height,z,0xff00ff00}, {x-half_width,half_height,z,0xff00ff00},
        {x+half_width,half_height,z,0xff00ff00}, {x-half_width,-half_height,z,0xff00ff00},
        {x+half_width,half_height,z,0xff00ff00}, {x+half_width,-half_height,z,0xff00ff00}
    };
    hr(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, verts, sizeof(Vertex)),
       "uncaptured green occluder");
}
void positions(float radius, float z, float (&out)[18]) {
    const float p[18] = {-radius,-radius,z, -radius,radius,z, radius,radius,z,
                         -radius,-radius,z, radius,radius,z, radius,-radius,z};
    std::memcpy(out, p, sizeof(p));
}
DWORD pixel(IDirect3DSurface9* surface, int x, int y) {
    D3DLOCKED_RECT lock{};
    hr(surface->LockRect(&lock, nullptr, D3DLOCK_READONLY), "readback LockRect");
    const auto* row = static_cast<const unsigned char*>(lock.pBits) + y * lock.Pitch;
    DWORD value{};
    std::memcpy(&value, row + x * 4, sizeof(value));
    hr(surface->UnlockRect(), "readback UnlockRect");
    return value;
}
}

int main() {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    std::puts("shadow MSAA depth preservation fixture: start");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    // This fixture must be run only on a private display; it never shows or activates its window.
    HWND window = CreateWindowExA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, "STATIC",
                                  "shadow fixture", WS_POPUP, 0, 0, 64, 64,
                                  nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    require(window != nullptr, "private fixture window");
    const HMODULE library = LoadLibraryA("d3d9.dll");
    if (!library) {
        std::fprintf(stderr,"D3D9 loader error: %lu\n",GetLastError());
        require(false,"D3D9 provider unavailable");
    }
    using Create9 = IDirect3D9* (WINAPI*)(UINT);
    const FARPROC proc = GetProcAddress(library,"Direct3DCreate9");
    Create9 create = nullptr;
    static_assert(sizeof(create)==sizeof(proc));
    std::memcpy(&create,&proc,sizeof(create));
    require(create != nullptr,"Direct3DCreate9 export");
    Ref<IDirect3D9> factory;
    factory.p = create(D3D_SDK_VERSION);
    require(factory.p != nullptr, "Direct3DCreate9");
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window;
    pp.BackBufferWidth = pp.BackBufferHeight = 64;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    Ref<IDirect3DDevice9> device;
    HRESULT created = factory->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                     D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, device.out());
    if (FAILED(created))
        created = factory->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, window,
                     D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, device.out());
    hr(created, "HAL/REF CreateDevice on private display");
    Ref<IDirect3DSurface9> target, depth, resolved;
    hr(device->CreateRenderTarget(64, 64, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_2_SAMPLES, 0,
                                  FALSE, target.out(), nullptr), "2x MSAA A8R8G8B8 target");
    hr(device->CreateDepthStencilSurface(512, 512, D3DFMT_D24S8, D3DMULTISAMPLE_2_SAMPLES,
                                          0, TRUE, depth.out(), nullptr), "2x MSAA D24S8 depth");
    hr(device->CreateRenderTarget(64, 64, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0,
                                  FALSE, resolved.out(), nullptr), "private resolve target");
    hr(device->SetRenderTarget(0, target.p), "SetRenderTarget");
    hr(device->SetDepthStencilSurface(depth.p), "SetDepthStencilSurface");
    D3DVIEWPORT9 viewport{0, 0, 64, 64, 0.0f, 1.0f};
    hr(device->SetViewport(&viewport), "SetViewport");
    D3DMATRIX identity{};
    identity._11 = identity._22 = identity._33 = identity._44 = 1.0f;
    hr(device->SetTransform(D3DTS_WORLD, &identity), "world identity");
    hr(device->SetTransform(D3DTS_VIEW, &identity), "view identity");
    hr(device->SetTransform(D3DTS_PROJECTION, &identity), "projection identity");
    hr(device->SetFVF(fvf), "baseline FVF");
    hr(device->SetRenderState(D3DRS_LIGHTING, FALSE), "lighting off");
    hr(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE), "cull none");
    hr(device->SetRenderState(D3DRS_ZENABLE, TRUE), "Z enable");
    hr(device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE), "Z write");
    hr(device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL), "Z less equal");
    hr(device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE), "blend off");
    hr(device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE), "sRGB off");
    hr(device->SetRenderState(D3DRS_STENCILENABLE, FALSE), "stencil off");
    hr(device->SetRenderState(D3DRS_FOGENABLE, FALSE), "fog off");
    hr(device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE), "scissor off");
    hr(device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xf), "RGBA write");
    hr(device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                     D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0), "clear");
    hr(device->BeginScene(), "BeginScene");
    draw_quad(device.p, .9f, .7f);
    draw_quad(device.p, .2f, .3f);
    draw_uncaptured_occluder(device.p);
    hr(device->EndScene(), "baseline EndScene");
    Ref<IDirect3DSurface9> pixels;
    hr(device->CreateOffscreenPlainSurface(64, 64, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                           pixels.out(), nullptr), "readback surface");
    hr(device->StretchRect(target.p, nullptr, resolved.p, nullptr, D3DTEXF_NONE),
       "baseline MSAA resolve");
    hr(device->GetRenderTargetData(resolved.p, pixels.p), "baseline GetRenderTargetData");
    const DWORD green_before = pixel(pixels.p, 43, 32);
    require(((green_before >> 8) & 0xff) == 255 &&
            ((green_before >> 16) & 0xff) == 0 && (green_before & 0xff) == 0,
            "uncaptured occluder must be green before shadow pass");
    hr(device->BeginScene(), "shadow BeginScene");

    a2fo::shadows::Pass pass;
    a2fo::shadows::Settings settings{};
    settings.enabled = true; settings.size = 512; settings.strength = .6f; settings.bias = .001f;
    pass.configure(settings);
    pass.begin_view();
    a2fo::shadows::Draw draw{};
    draw.primitive_count = 2;
    for (unsigned group=0; group<2; ++group) {
        const float radius = group ? .2f : .9f;
        const float z = group ? .3f : .7f;
        a2fo::shadows::Capture capture{};
        capture.clip[0] = capture.clip[5] = capture.clip[10] = capture.clip[15] = 1.0f;
        capture.bounds_center[2] = z;
        capture.bounds_extent[0] = capture.bounds_extent[1] = radius;
        capture.bounds_extent[2] = .001f;
        capture.light_direction[0] = .5f; capture.light_direction[2] = 1.0f;
        float triangles[18]{};
        positions(radius, z, triangles);
        require(pass.capture(device.p, capture, draw, triangles, 6), pass.diagnostic());
        for (float& value : triangles) value = 999.0f; // Owned snapshot survives caller mutation.
    }
    Ref<IDirect3DVertexBuffer9> sentinel_vb;
    Ref<IDirect3DIndexBuffer9> sentinel_ib;
    hr(device->CreateVertexBuffer(64, 0, 0, D3DPOOL_MANAGED, sentinel_vb.out(), nullptr),
       "sentinel VB");
    hr(device->CreateIndexBuffer(12, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED,
                                  sentinel_ib.out(), nullptr), "sentinel IB");
    hr(device->SetStreamSource(0, sentinel_vb.p, 4, sizeof(Vertex)), "bind sentinel VB");
    hr(device->SetIndices(sentinel_ib.p), "bind sentinel IB");
    // Inherited state is deliberately unsuitable for the shadow passes. The pass must
    // configure its own drawing and restore every value after its two GPU draws.
    hr(device->SetRenderState(D3DRS_DEPTHBIAS, float_bits(.25f)), "hostile depth bias");
    hr(device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, float_bits(2.0f)), "hostile slope bias");
    hr(device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME), "hostile fill mode");
    const float clip_plane[4]{1.0f, 0.0f, 0.0f, .5f};
    hr(device->SetClipPlane(0, clip_plane), "hostile clip plane");
    hr(device->SetRenderState(D3DRS_CLIPPLANEENABLE, 1), "hostile clip enable");
    hr(device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT), "hostile blend op");
    hr(device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE), "hostile alpha blend");
    hr(device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ZERO), "hostile source alpha");
    hr(device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ZERO), "hostile dest alpha");
    hr(device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_REVSUBTRACT), "hostile alpha op");
    hr(device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP), "hostile sampler U");
    hr(device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR), "hostile sampler V");
    hr(device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR), "hostile sampler min");
    hr(device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR), "hostile sampler mag");
    hr(device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR), "hostile sampler mip");
    hr(device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, TRUE), "hostile sampler sRGB");
    Snapshot before{};
    snapshot(device.p, before);
    require(pass.finish_view(), pass.diagnostic());
    require(std::strcmp(pass.diagnostic(), "shadow depth and overlay drawn") == 0,
            "success diagnostic");
    same_state(device.p, before);
    hr(device->EndScene(), "EndScene");
    hr(device->StretchRect(target.p, nullptr, resolved.p, nullptr, D3DTEXF_NONE),
       "final MSAA resolve");
    hr(device->GetRenderTargetData(resolved.p, pixels.p), "GetRenderTargetData");
    // Light travels +x as z increases: caster at z=.3 shades receiver at z=.7 near x=.25.
    const DWORD shadow_pixel = pixel(pixels.p, 40, 32);
    const DWORD lit_pixel = pixel(pixels.p, 54, 32);    // world x approximately .7
    const DWORD caster_pixel = pixel(pixels.p, 32, 32);
    const DWORD green_after = pixel(pixels.p, 43, 32);
    const int shadow = (shadow_pixel >> 16) & 0xff;
    const int lit = (lit_pixel >> 16) & 0xff;
    const int caster = (caster_pixel >> 16) & 0xff;
    std::printf("shadow=%d lit=%d caster=%d green=%u/%u/%u alpha=%u/%u/%u\n", shadow, lit, caster,
                static_cast<unsigned>((green_after >> 16) & 0xff),
                static_cast<unsigned>((green_after >> 8) & 0xff),
                static_cast<unsigned>(green_after & 0xff),
                static_cast<unsigned>(shadow_pixel >> 24),
                static_cast<unsigned>(lit_pixel >> 24),
                static_cast<unsigned>(caster_pixel >> 24));
    require((lit_pixel & 0x00ffffff) == 0x00ffffff &&
            (caster_pixel & 0x00ffffff) == 0x00ffffff &&
            shadow >= 90 && shadow <= 115,
            "receiver shadow near 102 with white lit and caster pixels");
    require(((green_after >> 8) & 0xff) == 255 &&
            ((green_after >> 16) & 0xff) == 0 && (green_after & 0xff) == 0,
            "uncaptured green occluder must remain unshadowed");
    require((shadow_pixel >> 24) == 255 && (lit_pixel >> 24) == 255 &&
            (caster_pixel >> 24) == 255, "target alpha changed");
    pass.release();
    DestroyWindow(window);
    std::puts("shadow MSAA depth preservation fixture: PASS");
    return 0;
}
