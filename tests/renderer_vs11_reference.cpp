#include "renderer_vs11_scene.hpp"
#include <d3d8.h>
#include <cstring>

namespace vs11_fixture {
namespace {
template<class T> struct Owned {
    T* p = nullptr;
    ~Owned() { if (p) p->Release(); }
    T** out() { return &p; }
    T* operator->() { return p; }
};
}

HRESULT render_reference(HWND window, const Scene& s,
                         std::vector<std::uint8_t>& rgba, UINT& target_samples, std::string& error) {
    Owned<IDirect3D8> factory;
    factory.p = Direct3DCreate8(D3D_SDK_VERSION);
    if (!factory.p) { error = "Direct3DCreate8"; return E_FAIL; }
    Owned<IDirect3DDevice8> device;
    D3DPRESENT_PARAMETERS present{};
    present.BackBufferWidth = present.BackBufferHeight = 64;
    present.BackBufferCount = 1;
    present.BackBufferFormat = D3DFMT_X8R8G8B8;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = window;
    present.Windowed = TRUE;
    present.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    HRESULT hr = factory->CreateDevice(0, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present, device.out());
    if (FAILED(hr)) { error = "D3D8 CreateDevice"; return hr; }
#define REF(call) do { hr = (call); if (FAILED(hr)) { error = #call; return hr; } } while (false)
    Owned<IDirect3DSurface8> render_target;
    REF(device->GetRenderTarget(render_target.out()));
    D3DSURFACE_DESC target_desc{};
    REF(render_target->GetDesc(&target_desc));
    target_samples=static_cast<UINT>(target_desc.MultiSampleType);
    if(target_samples==1){error="non-counted native multisample mode unsupported by comparison";return E_NOTIMPL;}
    if(!target_samples)target_samples=1;
    const DWORD declaration[] = {
        D3DVSD_STREAM(0), D3DVSD_REG(0, D3DVSDT_FLOAT3),
        D3DVSD_REG(1, D3DVSDT_FLOAT3), D3DVSD_REG(2, D3DVSDT_FLOAT2),
        D3DVSD_REG(3, D3DVSDT_FLOAT3), D3DVSD_REG(4, D3DVSDT_FLOAT3),
        D3DVSD_REG(5, D3DVSDT_FLOAT3), D3DVSD_END()
    };
    std::vector<DWORD> code(s.program.begin(), s.program.end());
    DWORD shader = 0;
    REF(device->CreateVertexShader(declaration, code.data(), &shader, 0));
    struct ShaderOwner {
        IDirect3DDevice8* device; DWORD shader;
        ~ShaderOwner() { if (shader) device->DeleteVertexShader(shader); }
    } shader_owner{device.p, shader};
    REF(device->SetVertexShader(shader));
    REF(device->SetVertexShaderConstant(0, s.constants.data(), 96));
    REF(device->SetRenderState(D3DRS_LIGHTING, FALSE));
    REF(device->SetRenderState(D3DRS_FOGENABLE, FALSE));
    REF(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));
    REF(device->SetRenderState(D3DRS_ZENABLE, FALSE));
    REF(device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
    REF(device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE));
    REF(device->SetRenderState(D3DRS_SPECULARENABLE, FALSE));
    REF(device->SetTexture(0, nullptr));
    REF(device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1));
    REF(device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE));
    REF(device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1));
    REF(device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE));
    REF(device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE));
    D3DVIEWPORT8 full{0, 0, 64, 64, 0, 1};
    REF(device->SetViewport(&full));
    REF(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00102030, 1, 0));
    D3DVIEWPORT8 viewport{s.viewport_x, s.viewport_y, s.viewport_width,
                          s.viewport_height, 0, 1};
    REF(device->SetViewport(&viewport));
    REF(device->BeginScene());
    REF(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, s.vertices.data(), sizeof(Vertex)));
    REF(device->EndScene());
    Owned<IDirect3DSurface8> back;
    REF(device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, back.out()));
    D3DLOCKED_RECT locked{};
    REF(back->LockRect(&locked, nullptr, D3DLOCK_READONLY));
    rgba.resize(64 * 64 * 4);
    for (UINT y = 0; y < 64; ++y) for (UINT x = 0; x < 64; ++x) {
        DWORD pixel = 0;
        std::memcpy(&pixel, static_cast<const std::uint8_t*>(locked.pBits) +
                    y * locked.Pitch + x * 4, 4);
        auto* dst = rgba.data() + (y * 64 + x) * 4;
        dst[0] = static_cast<std::uint8_t>(pixel >> 16);
        dst[1] = static_cast<std::uint8_t>(pixel >> 8);
        dst[2] = static_cast<std::uint8_t>(pixel);
        dst[3] = 255;
    }
    REF(back->UnlockRect());
#undef REF
    return S_OK;
}
} // namespace vs11_fixture
