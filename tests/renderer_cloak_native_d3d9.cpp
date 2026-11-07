// D3D9 inspection lives apart from the D3D8 fixture because SDK headers conflict.
#include <windows.h>
#include <d3d9.h>
#include <cstdio>

namespace {
template<class T> void drop(T*& p) { if (p) { p->Release(); p = nullptr; } }
}
extern "C" bool cloak_d3d9_probe(void* device8, bool after) {
    static IDirect3DDevice9* device = nullptr;
    static IDirect3DVertexShader9* before_vs = nullptr;
    static IDirect3DPixelShader9* before_ps = nullptr;
    static IDirect3DVertexDeclaration9* before_decl = nullptr;
    static IDirect3DBaseTexture9* before_texture = nullptr;
    static IDirect3DVertexBuffer9* before_stream = nullptr;
    static IDirect3DIndexBuffer9* before_indices = nullptr;
    static DWORD before_blend = 0, before_zwrite = 0;
    static D3DVIEWPORT9 before_view{};
    static UINT before_offset = 0, before_stride = 0;
    auto fail = [](const char* part, HRESULT hr) {
        std::fprintf(stderr, "D3D9 %s hr=0x%08lx\n", part, static_cast<unsigned long>(hr));
        return false;
    };
    if (!after) {
        HRESULT hr = static_cast<IUnknown*>(device8)->QueryInterface(IID_IDirect3DDevice9,
                                                           reinterpret_cast<void**>(&device));
        if (FAILED(hr) || !device) return fail("QueryInterface", hr);
        D3DCAPS9 caps{};
        hr = device->GetDeviceCaps(&caps);
        if (FAILED(hr)) return fail("GetDeviceCaps", hr);
        if (caps.VertexShaderVersion < D3DVS_VERSION(3,0) ||
            caps.PixelShaderVersion < D3DPS_VERSION(3,0)) return fail("SM3 unavailable", E_NOTIMPL);
        if (FAILED(hr = device->GetVertexShader(&before_vs)) ||
            FAILED(hr = device->GetPixelShader(&before_ps)) ||
            FAILED(hr = device->GetVertexDeclaration(&before_decl)) ||
            FAILED(hr = device->GetTexture(1, &before_texture)) ||
            FAILED(hr = device->GetStreamSource(0, &before_stream, &before_offset, &before_stride)) ||
            FAILED(hr = device->GetIndices(&before_indices)) ||
            FAILED(hr = device->GetRenderState(D3DRS_ALPHABLENDENABLE, &before_blend)) ||
            FAILED(hr = device->GetRenderState(D3DRS_ZWRITEENABLE, &before_zwrite)) ||
            FAILED(hr = device->GetViewport(&before_view))) return fail("capture state", hr);
        std::puts("D3D9 QueryInterface and SM3 caps succeeded");
        return true;
    }
    if (!device) return false;
    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    IDirect3DVertexDeclaration9* decl = nullptr;
    IDirect3DBaseTexture9* texture = nullptr;
    IDirect3DVertexBuffer9* stream = nullptr;
    IDirect3DIndexBuffer9* indices = nullptr;
    DWORD blend = 0, zwrite = 0;
    UINT offset = 0, stride = 0;
    D3DVIEWPORT9 view{};
    HRESULT hr = device->GetVertexShader(&vs);
    if (SUCCEEDED(hr)) hr = device->GetPixelShader(&ps);
    if (SUCCEEDED(hr)) hr = device->GetVertexDeclaration(&decl);
    if (SUCCEEDED(hr)) hr = device->GetTexture(1, &texture);
    if (SUCCEEDED(hr)) hr = device->GetStreamSource(0, &stream, &offset, &stride);
    if (SUCCEEDED(hr)) hr = device->GetIndices(&indices);
    if (SUCCEEDED(hr)) hr = device->GetRenderState(D3DRS_ALPHABLENDENABLE, &blend);
    if (SUCCEEDED(hr)) hr = device->GetRenderState(D3DRS_ZWRITEENABLE, &zwrite);
    if (SUCCEEDED(hr)) hr = device->GetViewport(&view);
    const bool ok = SUCCEEDED(hr) && vs == before_vs && ps == before_ps &&
        decl == before_decl && texture == before_texture && stream == before_stream &&
        indices == before_indices && offset == before_offset && stride == before_stride &&
        blend == before_blend && zwrite == before_zwrite &&
        view.X == before_view.X && view.Y == before_view.Y &&
        view.Width == before_view.Width && view.Height == before_view.Height;
    drop(vs); drop(ps); drop(decl); drop(texture); drop(stream); drop(indices);
    drop(before_vs); drop(before_ps); drop(before_decl); drop(before_texture);
    drop(before_stream); drop(before_indices); drop(device);
    if (!ok) return fail("state restoration", hr);
    std::puts("D3D9 shader/texture/stream/index/render/viewport state restored");
    return true;
}
