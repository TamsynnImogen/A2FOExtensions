#include "../renderer/shadows/pass.hpp"
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
struct Snapshot {
    DWORD values[sizeof(states)/sizeof(states[0])]{};
    DWORD sampler_values[sizeof(sampler_states)/sizeof(sampler_states[0])]{};
    D3DVIEWPORT9 viewport{};
    Ref<IDirect3DSurface9> target, depth;
};
void snapshot(IDirect3DDevice9* d, Snapshot& s) {
    for (unsigned i=0; i<sizeof(states)/sizeof(states[0]); ++i)
        hr(d->GetRenderState(states[i], &s.values[i]), "GetRenderState");
    for (unsigned i=0; i<sizeof(sampler_states)/sizeof(sampler_states[0]); ++i)
        hr(d->GetSamplerState(0, sampler_states[i], &s.sampler_values[i]), "GetSamplerState");
    hr(d->GetViewport(&s.viewport), "GetViewport");
    hr(d->GetRenderTarget(0, s.target.out()), "GetRenderTarget");
    hr(d->GetDepthStencilSurface(s.depth.out()), "GetDepthStencilSurface");
}
void same_state(IDirect3DDevice9* d, const Snapshot& before) {
    Snapshot after{};
    snapshot(d, after);
    for (unsigned i=0; i<sizeof(states)/sizeof(states[0]); ++i)
        require(before.values[i] == after.values[i], "render state changed");
    for (unsigned i=0; i<sizeof(sampler_states)/sizeof(sampler_states[0]); ++i)
        require(before.sampler_values[i] == after.sampler_values[i], "sampler state changed");
    require(std::memcmp(&before.viewport, &after.viewport, sizeof(D3DVIEWPORT9)) == 0,
            "viewport changed");
    require(before.target.p == after.target.p && before.depth.p == after.depth.p,
            "target or depth changed");
}
void positions(float radius, float z, float (&out)[18]) {
    const float p[18] = {-radius,-radius,z, -radius,radius,z, radius,radius,z,
                         -radius,-radius,z, radius,radius,z, radius,-radius,z};
    std::memcpy(out, p, sizeof(p));
}
void draw_quad(IDirect3DDevice9* d, float radius, float z) {
    const Vertex verts[6] = {
        {-radius,-radius,z,0xffffffff}, {-radius,radius,z,0xffffffff},
        {radius,radius,z,0xffffffff}, {-radius,-radius,z,0xffffffff},
        {radius,radius,z,0xffffffff}, {radius,-radius,z,0xffffffff}
    };
    hr(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, verts, sizeof(Vertex)), "baseline quad");
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
void capture_quad(a2fo::shadows::Pass& pass, IDirect3DDevice9* device,
                  float radius, float z) {
    a2fo::shadows::Capture capture{};
    capture.clip[0] = capture.clip[5] = capture.clip[10] = capture.clip[15] = 1.0f;
    capture.clip[2] = .75f; // Column-vector equivalent of native VIEW _31 shear.
    capture.bounds_center[2] = z;
    capture.bounds_extent[0] = capture.bounds_extent[1] = radius;
    capture.bounds_extent[2] = .001f;
    capture.light_direction[2] = 1.0f;
    a2fo::shadows::Draw draw{};
    draw.primitive_count = 2;
    float triangles[18]{};
    positions(radius, z, triangles);
    require(pass.capture(device, capture, draw, triangles, 6), pass.diagnostic());
    for (float& value : triangles) value = 999.0f; // Pass owns the captured vertices.
}
struct Result { int shadow, lit, caster; DWORD shadow_pixel, lit_pixel, caster_pixel; };
Result run_case(IDirect3DDevice9* device, IDirect3DSurface9* target,
                bool remote, bool fixed_bias, const char* name) {
    hr(device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                     D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0), "clear");
    hr(device->BeginScene(), "BeginScene");
    // Camera shear separates the caster from its +z shadow on the receiver.
    draw_quad(device, .9f, .7f);
    draw_quad(device, .12f, .3f);
    a2fo::shadows::Pass pass;
    a2fo::shadows::Settings settings{};
    settings.enabled = true;
    settings.size = 512;
    settings.strength = .6f;
    settings.bias = .0015f;
    settings.bias_world = fixed_bias ? .01f : -1.0f;
    pass.configure(settings);
    pass.begin_view();
    capture_quad(pass, device, .9f, .7f);  // receiver
    capture_quad(pass, device, .12f, .3f); // caster
    if (remote) capture_quad(pass, device, .1f, 1000.0f); // off-camera depth extender
    Snapshot before{};
    snapshot(device, before);
    require(pass.finish_view(), pass.diagnostic());
    require(std::strcmp(pass.diagnostic(), "shadow depth and overlay drawn") == 0,
            "success diagnostic");
    same_state(device, before);
    hr(device->EndScene(), "EndScene");
    Ref<IDirect3DSurface9> pixels;
    hr(device->CreateOffscreenPlainSurface(64, 64, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                           pixels.out(), nullptr), "readback surface");
    hr(device->GetRenderTargetData(target, pixels.p), "GetRenderTargetData");
    Result result{};
    result.shadow_pixel = pixel(pixels.p, 49, 32);
    result.lit_pixel = pixel(pixels.p, 54, 32);
    result.caster_pixel = pixel(pixels.p, 39, 32);
    result.shadow = (result.shadow_pixel >> 16) & 0xff;
    result.lit = (result.lit_pixel >> 16) & 0xff;
    result.caster = (result.caster_pixel >> 16) & 0xff;
    std::printf("%s shadow=%d lit=%d caster=%d pixels=%08lx/%08lx/%08lx\n", name,
                result.shadow, result.lit, result.caster,
                static_cast<unsigned long>(result.shadow_pixel),
                static_cast<unsigned long>(result.lit_pixel),
                static_cast<unsigned long>(result.caster_pixel));
    require((result.shadow_pixel >> 24) == 255 && (result.lit_pixel >> 24) == 255 &&
            (result.caster_pixel >> 24) == 255,
            "target alpha changed");
    require(result.lit > 245, "unshadowed receiver changed");
    require(result.caster > 245, "caster self-shadowed or disappeared");
    pass.release();
    return result;
}
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::puts("shadow bias stability GPU fixture: start");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    // Run only on a private display; this window is never shown or activated.
    HWND window = CreateWindowExA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, "STATIC",
                                  "shadow fixture", WS_POPUP, 0, 0, 64, 64,
                                  nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    require(window != nullptr, "private fixture window");
    const HMODULE library = LoadLibraryA("d3d9.dll");
    require(library != nullptr, "D3D9 provider available");
    using Create9 = IDirect3D9* (WINAPI*)(UINT);
    const FARPROC proc = GetProcAddress(library, "Direct3DCreate9");
    Create9 create = nullptr;
    static_assert(sizeof(create) == sizeof(proc));
    std::memcpy(&create, &proc, sizeof(create));
    require(create != nullptr, "Direct3DCreate9 export");
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
    Ref<IDirect3DSurface9> target, depth;
    hr(device->CreateRenderTarget(64, 64, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0,
                                  TRUE, target.out(), nullptr), "A8R8G8B8 target");
    hr(device->CreateDepthStencilSurface(512, 512, D3DFMT_D24S8, D3DMULTISAMPLE_NONE,
                                          0, TRUE, depth.out(), nullptr), "D24S8 depth");
    hr(device->SetRenderTarget(0, target.p), "SetRenderTarget");
    hr(device->SetDepthStencilSurface(depth.p), "SetDepthStencilSurface");
    D3DVIEWPORT9 viewport{0, 0, 64, 64, 0.0f, 1.0f};
    hr(device->SetViewport(&viewport), "SetViewport");
    D3DMATRIX identity{};
    identity._11 = identity._22 = identity._33 = identity._44 = 1.0f;
    hr(device->SetTransform(D3DTS_WORLD, &identity), "world identity");
    D3DMATRIX view = identity;
    view._31 = .75f; // Native row-vector x' = x + .75*z.
    hr(device->SetTransform(D3DTS_VIEW, &view), "view shear");
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

    const Result near_fixed = run_case(device.p, target.p, false, true, "fixed near");
    const Result far_fixed = run_case(device.p, target.p, true, true, "fixed remote");
    const Result far_legacy = run_case(device.p, target.p, true, false, "legacy remote");
    require(near_fixed.shadow < near_fixed.lit * .8f, "fixed near cast shadow");
    require(far_fixed.shadow < far_fixed.lit * .8f, "fixed remote cast shadow");
    require(far_legacy.shadow > 245, "legacy normalized bias loses remote shadow");
    DestroyWindow(window);
    std::puts("shadow bias stability GPU fixture: PASS");
    return 0;
}
