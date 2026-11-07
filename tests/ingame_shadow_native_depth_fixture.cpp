#include "../renderer/shadows/pass.hpp"
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
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
using Assemble = HRESULT (WINAPI*)(const void*, SIZE_T, const char*,
    const D3D_SHADER_MACRO*, ID3DInclude*, UINT, ID3DBlob**, ID3DBlob**);
constexpr D3DVERTEXELEMENT9 position_declaration[] = {
    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    D3DDECL_END()
};
constexpr char native_vs[] =
    "vs_1_1\n"
    "dcl_position v0\n"
    "dp4 oPos.x, v0, c2\n"
    "dp4 oPos.y, v0, c3\n"
    "dp4 oPos.z, v0, c4\n"
    "dp4 oPos.w, v0, c5\n"
    "mov oD0, c6\n";
constexpr char white_ps[] =
    "float4 main() : COLOR0 { return float4(1.0, 1.0, 1.0, 1.0); }";
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
    float native_constants[20]{};
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
    hr(d->GetVertexShaderConstantF(2, s.native_constants, 5), "GetVertexShaderConstantF");
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
    require(std::memcmp(before.native_constants, after.native_constants,
                        sizeof(before.native_constants)) == 0, "native VS constants changed");
}
void positions(float radius, float z, float (&out)[18]);
void draw_quad(IDirect3DDevice9* d, float radius, float z) {
    float verts[18]{};
    positions(radius, z, verts);
    hr(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, verts, 3*sizeof(float)), "native VS baseline quad");
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
struct Case {
    const char* name;
    bool perspective;
    int shadow_x, lit_x;
    float world_x, world_y, world_z;
};
void run_case(IDirect3DDevice9* device, IDirect3DSurface9* target,
              IDirect3DSurface9* depth, IDirect3DVertexShader9* native_shader,
              IDirect3DVertexDeclaration9* native_decl,
              IDirect3DPixelShader9* baseline_shader, const Case& test) {
    // Reset hostile state restored by the preceding case before its native
    // baseline is drawn; otherwise the test itself leaves biased wireframes.
    hr(device->SetRenderState(D3DRS_ZENABLE, TRUE), "baseline Z enable");
    hr(device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE), "baseline Z write");
    hr(device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL), "baseline Z compare");
    hr(device->SetRenderState(D3DRS_DEPTHBIAS, 0), "baseline depth bias");
    hr(device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0), "baseline slope bias");
    hr(device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID), "baseline fill");
    hr(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE), "baseline culling");
    hr(device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0), "baseline clip planes");
    hr(device->SetRenderState(D3DRS_STENCILENABLE, FALSE), "baseline stencil");
    hr(device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE), "baseline blend");
    hr(device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xf), "baseline color write");
    D3DVIEWPORT9 viewport{0, 0, 64, 64, 0.0f, 1.0f};
    hr(device->SetViewport(&viewport), "SetViewport");
    D3DMATRIX world{}, view{}, projection{};
    world._11 = world._22 = world._33 = world._44 = 1.0f;
    view._11 = view._22 = view._33 = view._44 = 1.0f;
    projection._11 = projection._22 = projection._33 = projection._44 = 1.0f;
    if (test.perspective) {
        // Native D3D row-vector matrices. W*V cancels camera translation.
        world._41 = test.world_x; world._42 = test.world_y; world._43 = test.world_z;
        view._41 = -test.world_x; view._42 = -test.world_y; view._43 = -test.world_z;
        constexpr float near_z = 1.0f, far_z = 100000.0f;
        projection._33 = far_z / (far_z - near_z);
        projection._34 = 1.0f;
        projection._43 = -near_z * far_z / (far_z - near_z);
        projection._44 = 0.0f;
    }
    hr(device->SetTransform(D3DTS_WORLD, &world), "world");
    hr(device->SetTransform(D3DTS_VIEW, &view), "view");
    hr(device->SetTransform(D3DTS_PROJECTION, &projection), "projection");
    // c2..c5 are the rows of transpose(W*V*P), so each dp4 gives one clip component.
    float clip[16]{};
    clip[0] = clip[5] = 1.0f;
    clip[10] = test.perspective ? projection._33 : 1.0f;
    clip[11] = test.perspective ? projection._43 : 0.0f;
    clip[14] = test.perspective ? 1.0f : 0.0f;
    clip[15] = test.perspective ? 0.0f : 1.0f;
    const float white[4]{1.0f, 1.0f, 1.0f, 1.0f};
    hr(device->SetVertexShaderConstantF(2, clip, 4), "native VS clip constants");
    hr(device->SetVertexShaderConstantF(6, white, 1), "native VS white constant");
    hr(device->SetVertexDeclaration(native_decl), "native declaration");
    hr(device->SetVertexShader(native_shader), "native VS");
    hr(device->SetPixelShader(baseline_shader), "constant-white baseline pixel shader");
    hr(device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                     D3DCOLOR_XRGB(0, 0, 64), 1.0f, 0), "clear blue");
    hr(device->BeginScene(), "BeginScene");
    const float receiver_radius = test.perspective ? 630.0f : .9f;
    const float caster_radius = test.perspective ? 60.0f : .2f;
    const float receiver_z = test.perspective ? 700.0f : .7f;
    const float caster_z = test.perspective ? 300.0f : .3f;
    draw_quad(device, receiver_radius, receiver_z);
    draw_quad(device, caster_radius, caster_z);
    hr(device->EndScene(), "native baseline EndScene");
    Ref<IDirect3DSurface9> baseline_pixels;
    hr(device->CreateOffscreenPlainSurface(64, 64, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                           baseline_pixels.out(), nullptr), "baseline readback surface");
    hr(device->GetRenderTargetData(target, baseline_pixels.p), "baseline GetRenderTargetData");
    const DWORD baseline_caster = pixel(baseline_pixels.p, 32, 32);
    const DWORD baseline_receiver = pixel(baseline_pixels.p, test.lit_x, 32);
    std::printf("%s native baseline caster=%08lx receiver=%08lx\n", test.name,
                static_cast<unsigned long>(baseline_caster),
                static_cast<unsigned long>(baseline_receiver));
    require(((baseline_caster >> 16) & 0xff) > 245 &&
            ((baseline_receiver >> 16) & 0xff) > 245, "native VS baseline drew white geometry");
    hr(device->BeginScene(), "shadow BeginScene");

    a2fo::shadows::Pass pass;
    a2fo::shadows::Settings settings{};
    settings.enabled = true; settings.size = 512; settings.strength = .6f; settings.bias = .001f;
    pass.configure(settings);
    pass.begin_view();
    a2fo::shadows::Draw draw{};
    draw.primitive_count = 2;
    for (unsigned group=0; group<2; ++group) {
        const float radius = group ? caster_radius : receiver_radius;
        const float z = group ? caster_z : receiver_z;
        a2fo::shadows::Capture capture{};
        std::memcpy(capture.clip, clip, sizeof(clip));
        capture.bounds_center[2] = z;
        capture.bounds_extent[0] = capture.bounds_extent[1] = radius;
        capture.bounds_extent[2] = .001f;
        capture.light_direction[0] = test.perspective ? 1.0f : .5f;
        capture.light_direction[2] = 1.0f;
        float triangles[18]{};
        positions(radius, z, triangles);
        require(pass.capture(device, capture, draw, triangles, 6), pass.diagnostic());
        for (float& value : triangles) value = 999.0f;
    }
    Ref<IDirect3DVertexBuffer9> sentinel_vb;
    Ref<IDirect3DIndexBuffer9> sentinel_ib;
    hr(device->CreateVertexBuffer(64, 0, 0, D3DPOOL_MANAGED, sentinel_vb.out(), nullptr),
       "sentinel VB");
    hr(device->CreateIndexBuffer(12, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED,
                                  sentinel_ib.out(), nullptr), "sentinel IB");
    hr(device->SetStreamSource(0, sentinel_vb.p, 4, 3*sizeof(float)), "bind sentinel VB");
    hr(device->SetIndices(sentinel_ib.p), "bind sentinel IB");
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
    snapshot(device, before);
    require(before.vertex_shader.p == native_shader, "native VS bound at pass entry");
    require(pass.finish_view(), pass.diagnostic());
    require(std::strcmp(pass.diagnostic(), "shadow depth and overlay drawn") == 0,
            "success diagnostic");
    same_state(device, before);
    hr(device->EndScene(), "EndScene");
    Ref<IDirect3DSurface9> pixels;
    hr(device->CreateOffscreenPlainSurface(64, 64, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                           pixels.out(), nullptr), "readback surface");
    hr(device->GetRenderTargetData(target, pixels.p), "GetRenderTargetData");
    const DWORD shadow_pixel = pixel(pixels.p, test.shadow_x, 32);
    const DWORD lit_pixel = pixel(pixels.p, test.lit_x, 32);
    const DWORD caster_pixel = pixel(pixels.p, 32, 32);
    const int shadow = (shadow_pixel >> 16) & 0xff;
    const int lit = (lit_pixel >> 16) & 0xff;
    const int caster = (caster_pixel >> 16) & 0xff;
#ifdef A2FO_SHADOW_TEST_PROBE
    const float caster_point[3]{0,0,caster_z};float probe[4]{};
    require(pass.probe(1,caster_point,probe),"shadow probe");
    std::printf("%s caster map uv=(%g,%g) expected=%g sampled=%g; %s\n",test.name,
        probe[0],probe[1],probe[2],probe[3],pass.capture_diagnostic());
    std::printf("%s pixels shadow/lit/caster=%08lx/%08lx/%08lx\n",test.name,
        static_cast<unsigned long>(shadow_pixel),static_cast<unsigned long>(lit_pixel),
        static_cast<unsigned long>(caster_pixel));
#endif
    std::printf("%s shadow=%d lit=%d caster=%d alpha=%u/%u/%u\n", test.name,
                shadow, lit, caster, static_cast<unsigned>(shadow_pixel >> 24),
                static_cast<unsigned>(lit_pixel >> 24), static_cast<unsigned>(caster_pixel >> 24));
#ifndef A2FO_SHADOW_TEST_PROBE
    require(lit > 245 && caster > 245 && shadow < lit * .8f,
            "native depth cast shadow with lit receiver and caster");
#endif
    require((shadow_pixel >> 24) == 255 && (lit_pixel >> 24) == 255 &&
            (caster_pixel >> 24) == 255, "target alpha changed");
    pass.release();
    hr(device->SetRenderState(D3DRS_DEPTHBIAS, 0), "reset depth bias");
    hr(device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0), "reset slope bias");
    hr(device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID), "reset fill mode");
    hr(device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0), "reset clip plane");
    hr(device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD), "reset blend op");
    hr(device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE), "reset alpha blend");
    hr(device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE), "reset sampler sRGB");
    (void)depth;
}
}

int main() {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    std::puts("shadow native depth fixture: start");
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
                     D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, device.out());
    if (FAILED(created)) {
        std::puts("hardware vertex processing unavailable; trying software HAL");
        created = factory->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                     D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, device.out());
    }
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
    const HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
    require(compiler != nullptr, "d3dcompiler_47.dll for native VS assembly");
    const FARPROC assembler_proc = GetProcAddress(compiler, "D3DAssemble");
    Assemble assemble = nullptr;
    static_assert(sizeof(assemble) == sizeof(assembler_proc));
    std::memcpy(&assemble, &assembler_proc, sizeof(assemble));
    require(assemble != nullptr, "D3DAssemble export");
    Ref<ID3DBlob> code, errors;
    const HRESULT assembled = assemble(native_vs, std::strlen(native_vs), nullptr,
                                       nullptr, nullptr, 0, code.out(), errors.out());
    if (FAILED(assembled) && errors.p)
        std::fprintf(stderr, "VS assembly: %.*s\n", static_cast<int>(errors->GetBufferSize()),
                     static_cast<const char*>(errors->GetBufferPointer()));
    hr(assembled, "assemble VS 1.1 dp4 shader");
    const FARPROC compile_proc = GetProcAddress(compiler, "D3DCompile");
    pD3DCompile compile = nullptr;
    static_assert(sizeof(compile) == sizeof(compile_proc));
    std::memcpy(&compile, &compile_proc, sizeof(compile));
    require(compile != nullptr, "D3DCompile export");
    Ref<ID3DBlob> pixel_code, pixel_errors;
    const HRESULT compiled = compile(white_ps, std::strlen(white_ps), nullptr,
                                     nullptr, nullptr, "main", "ps_3_0", 0, 0,
                                     pixel_code.out(), pixel_errors.out());
    if (FAILED(compiled) && pixel_errors.p)
        std::fprintf(stderr, "PS compile: %.*s\n",
                     static_cast<int>(pixel_errors->GetBufferSize()),
                     static_cast<const char*>(pixel_errors->GetBufferPointer()));
    hr(compiled, "compile constant-white pixel shader");
    Ref<IDirect3DVertexShader9> native_shader;
    hr(device->CreateVertexShader(static_cast<const DWORD*>(code->GetBufferPointer()),
                                  native_shader.out()), "CreateVertexShader VS 1.1");
    Ref<IDirect3DVertexDeclaration9> native_decl;
    hr(device->CreateVertexDeclaration(position_declaration, native_decl.out()),
       "CreateVertexDeclaration position");
    Ref<IDirect3DPixelShader9> baseline_shader;
    hr(device->CreatePixelShader(static_cast<const DWORD*>(pixel_code->GetBufferPointer()),
                                 baseline_shader.out()), "CreatePixelShader constant white");
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
    run_case(device.p, target.p, depth.p, native_shader.p, native_decl.p, baseline_shader.p,
             {"orthographic origin control", false, 42, 54, 0, 0, 0});
    run_case(device.p, target.p, depth.p, native_shader.p, native_decl.p, baseline_shader.p,
             {"translated perspective native VS", true, 50, 9, 1200, -340, 800});
    run_case(device.p, target.p, depth.p, native_shader.p, native_decl.p, baseline_shader.p,
             {"large-world perspective native VS", true, 50, 9, 10000, -5000, 20000});
    DestroyWindow(window);
    std::puts("shadow native depth fixture: PASS");
    return 0;
}
