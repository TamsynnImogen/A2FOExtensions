#include "../build/refit/material-colours-candidate/source/renderer/refit/host.h"
#include <windows.h>
#include <d3d8.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
constexpr unsigned WIDTH = 64, HEIGHT = 64;
static_assert(sizeof(void*) == 4, "i686 Display callback fixture only");
static_assert(sizeof(A2FORefitApi2) == 60 && sizeof(A2FORefitDraw2) == 84,
              "ABI2 and Draw2 wire sizes must remain unchanged");

void need(bool yes, const char* message) {
    if (!yes) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void check(int32_t actual, int32_t expected, const char* message) {
    if (actual != expected) {
        std::fprintf(stderr, "FAIL: %s: got %ld, expected %ld\n", message,
                     static_cast<long>(actual), static_cast<long>(expected));
        std::exit(1);
    }
}
void ok(HRESULT hr, const char* message) {
    if (FAILED(hr)) {
        std::fprintf(stderr, "FAIL: %s: HRESULT 0x%08lx\n", message,
                     static_cast<unsigned long>(hr));
        std::exit(1);
    }
}
template<class Function> Function export_of(HMODULE module, const char* name) {
    FARPROC raw = GetProcAddress(module, name);
    Function typed = nullptr;
    static_assert(sizeof(raw) == sizeof(typed), "Windows function pointer size");
    std::memcpy(&typed, &raw, sizeof(typed));
    return typed;
}
struct PrivateWindow {
    HINSTANCE instance = GetModuleHandleA(nullptr);
    HWND hwnd = nullptr;
    PrivateWindow() {
        WNDCLASSEXA cls{};
        cls.cbSize = sizeof(cls);
        cls.hInstance = instance;
        cls.lpfnWndProc = DefWindowProcA;
        cls.lpszClassName = "A2FORefitRhwCallbackPrivateFixture";
        need(RegisterClassExA(&cls) != 0, "register private window class");
        hwnd = CreateWindowExA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, cls.lpszClassName,
                               "hidden RHW callback fixture", WS_POPUP, 0, 0,
                               WIDTH, HEIGHT, nullptr, nullptr, instance, nullptr);
        need(hwnd != nullptr && !IsWindowVisible(hwnd), "create hidden private window");
    }
    ~PrivateWindow() {
        if (hwnd) DestroyWindow(hwnd);
        UnregisterClassA("A2FORefitRhwCallbackPrivateFixture", instance);
    }
};
using Create8 = IDirect3D8* (WINAPI*)(UINT);
struct Vertex144 { float x, y, z, rhw; DWORD diffuse; float u, v; };
struct Vertex1c4 { float x, y, z, rhw; DWORD diffuse, specular; float u, v; };
static_assert(sizeof(Vertex144) == 28 && offsetof(Vertex144, u) == 20 &&
              (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1) == 0x144);
static_assert(sizeof(Vertex1c4) == 32 && offsetof(Vertex1c4, u) == 24 &&
              (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1) == 0x1c4);

struct Calls {
    A2FORefitDisplayReplaceMesh2 old_v2 = nullptr;
    A2FORefitDisplayReplaceMeshRhw3 new_v3 = nullptr;
    A2FORefitDisplayReplaceOpaque2 opaque = nullptr;
    std::vector<std::uint32_t> formats;
};
int32_t mesh_callback(void* raw, const A2FORefitDraw2* draw) {
    auto& calls = *static_cast<Calls*>(raw);
    need(draw && draw->size == sizeof(*draw), "borrowed Draw2 record");
    need(draw->vertex_format == 0x144 || draw->vertex_format == 0x1c4,
         "only qualified RHW formats enter callback");
    calls.formats.push_back(draw->vertex_format);
    check(calls.old_v2(mesh_callback, raw), A2FO_REFIT_STATE,
          "old registration blocked during callback");
    check(calls.new_v3(mesh_callback, raw), A2FO_REFIT_STATE,
          "new registration blocked during callback");
    check(calls.opaque(nullptr, nullptr), A2FO_REFIT_STATE,
          "generic registration blocked during callback");
    return 1; // Test claim only; does not attest scene or gameplay correctness.
}
struct WrongThread {
    A2FORefitDisplayReplaceMeshRhw3 new_v3 = nullptr;
    int32_t result = 0;
};
DWORD WINAPI wrong_thread_register(LPVOID raw) {
    auto& probe = *static_cast<WrongThread*>(raw);
    probe.result = probe.new_v3(nullptr, nullptr);
    return 0;
}
void check_wrong_thread(A2FORefitDisplayReplaceMeshRhw3 new_v3) {
    WrongThread probe{new_v3, 0};
    HANDLE thread = CreateThread(nullptr, 0, wrong_thread_register, &probe, 0, nullptr);
    need(thread != nullptr, "create wrong-thread registration probe");
    need(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0,
         "wrong-thread registration probe completed");
    need(CloseHandle(thread) != 0, "close wrong-thread probe handle");
    check(probe.result, A2FO_REFIT_STATE, "wrong-thread registration refused");
}
void opaque_state(IDirect3DDevice8* device) {
    ok(device->SetTexture(0, nullptr), "no texture");
    ok(device->SetPixelShader(0), "fixed pixel stage");
    ok(device->SetRenderState(D3DRS_LIGHTING, FALSE), "lighting off");
    ok(device->SetRenderState(D3DRS_ZENABLE, TRUE), "depth test on");
    ok(device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE), "depth write on");
    ok(device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL), "depth comparison");
    ok(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE), "no culling");
    ok(device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE), "opaque blending");
    ok(device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE), "neutral source blend");
    ok(device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO), "neutral destination blend");
    ok(device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD), "neutral blend operation");
    ok(device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE), "alpha test off");
    ok(device->SetRenderState(D3DRS_ALPHAREF, 0), "alpha reference");
    ok(device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS), "alpha comparison");
    ok(device->SetRenderState(D3DRS_FOGENABLE, FALSE), "fog off");
    ok(device->SetRenderState(D3DRS_STENCILENABLE, FALSE), "stencil off");
    ok(device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0), "clip planes off");
    ok(device->SetRenderState(D3DRS_ZBIAS, 0), "depth bias off");
    ok(device->SetRenderState(D3DRS_COLORWRITEENABLE, 15), "all colour channels");
    ok(device->SetRenderState(D3DRS_VERTEXBLEND, 0), "vertex blend off");
    ok(device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1), "colour op");
    ok(device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE), "diffuse colour");
    ok(device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1), "alpha op");
    ok(device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE), "diffuse alpha");
    ok(device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE), "stage one off");
    ok(device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE), "stage one alpha off");
    D3DVIEWPORT8 viewport{0, 0, WIDTH, HEIGHT, 0.f, 1.f};
    ok(device->SetViewport(&viewport), "full-depth viewport");
}
void draw(IDirect3DDevice8* device, std::uint32_t format) {
    constexpr WORD indices[3] = {0, 1, 2};
    const Vertex144 plain[3] = {
        {16, 16, .2f, 1, 0xffff0000, 0, 0},
        {48, 16, .2f, 1, 0xffff0000, 1, 0},
        {32, 48, .2f, 1, 0xffff0000, .5f, 1},
    };
    const Vertex1c4 specular[3] = {
        {16, 16, .2f, 1, 0xffff0000, 0, 0, 0},
        {48, 16, .2f, 1, 0xffff0000, 0, 1, 0},
        {32, 48, .2f, 1, 0xffff0000, 0, .5f, 1},
    };
    const bool with_specular = format == 0x1c4;
    ok(device->SetVertexShader(format), "set exact RHW FVF");
    ok(device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                     0xff202020, 1.f, 0), "clear opaque draw target");
    ok(device->BeginScene(), "begin RHW draw");
    ok(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 3, 1, indices,
                                      D3DFMT_INDEX16, with_specular
                                          ? static_cast<const void*>(specular)
                                          : static_cast<const void*>(plain),
                                      with_specular ? sizeof(Vertex1c4) : sizeof(Vertex144)),
       "draw indexed RHW triangle");
    ok(device->EndScene(), "end RHW draw");
}
void count(const Calls& calls, std::size_t want, std::uint32_t last, const char* label) {
    if (calls.formats.size() != want || (last && (calls.formats.empty() || calls.formats.back() != last))) {
        std::fprintf(stderr, "FAIL: %s calls=%lu last=0x%lx\n", label,
                     static_cast<unsigned long>(calls.formats.size()),
                     static_cast<unsigned long>(calls.formats.empty() ? 0 : calls.formats.back()));
        std::exit(1);
    }
}
void run(const char* backend, const char* display_path) {
    const std::uint32_t backend_id = std::strcmp(backend, "vulkan") == 0 ? 1u : 2u;
    need(backend_id == 1 || std::strcmp(backend, "d3d12") == 0, "known backend argument");
    PrivateWindow window;
    HMODULE display = LoadLibraryA(display_path);
    need(display != nullptr, "load staged Display DLL");
    const auto configure = export_of<A2FORefitDisplayConfigure>(display, "A2FORefitDisplay_Configure_v1");
    const auto prepare = export_of<A2FORefitDisplayPrepare>(display, "A2FORefitDisplay_Prepare_v1");
    const auto create8 = export_of<Create8>(display, "Direct3DCreate8");
    const auto old_v2 = export_of<A2FORefitDisplayReplaceMesh2>(display, "A2FORefitDisplay_ReplaceMesh_v2");
    const auto new_v3 = export_of<A2FORefitDisplayReplaceMeshRhw3>(display, "A2FORefitDisplay_ReplaceMeshRhw_v3");
    const auto opaque = export_of<A2FORefitDisplayReplaceOpaque2>(display, "A2FORefitDisplay_ReplaceOpaque_v2");
    need(configure && prepare && create8 && old_v2 && new_v3 && opaque,
         "Display configure/prepare/factory and replacement exports");
    check(configure(backend_id, window.hwnd, WIDTH, HEIGHT, 0), A2FO_REFIT_OK,
          "configure Display on private owner HWND");
    IDirect3D8* d3d = create8(D3D_SDK_VERSION);
    need(d3d != nullptr, "create Display D3D8 facade");
    D3DPRESENT_PARAMETERS pp{};
    pp.BackBufferWidth = WIDTH;
    pp.BackBufferHeight = HEIGHT;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window.hwnd;
    pp.Windowed = TRUE;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    IDirect3DDevice8* device = nullptr;
    ok(d3d->CreateDevice(0, D3DDEVTYPE_HAL, window.hwnd,
                         D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device),
       "create private Display device with auto depth");
    need(device != nullptr, "private Display device pointer");
    opaque_state(device);
    Calls calls{old_v2, new_v3, opaque, {}};

    check(old_v2(mesh_callback, &calls), A2FO_REFIT_OK, "register strict ABI2 mesh callback");
    draw(device, 0x144);
    count(calls, 0, 0, "strict ABI2 retains 0x144 original draw");
    draw(device, 0x1c4);
    count(calls, 1, 0x1c4, "strict ABI2 admits only 0x1c4");
    check_wrong_thread(new_v3);
    draw(device, 0x144);
    count(calls, 1, 0x1c4, "wrong-thread attempt leaves strict mode");
    draw(device, 0x1c4);
    count(calls, 2, 0x1c4, "strict mode still owns 0x1c4 after wrong thread");

    check(new_v3(mesh_callback, &calls), A2FO_REFIT_OK, "register opt-in RHW v3 callback");
    draw(device, 0x144);
    count(calls, 3, 0x144, "RHW v3 admits 0x144");
    draw(device, 0x1c4);
    count(calls, 4, 0x1c4, "RHW v3 admits 0x1c4");
    check(old_v2(mesh_callback, &calls), A2FO_REFIT_OK, "switch back to strict ABI2");
    draw(device, 0x144);
    count(calls, 4, 0x1c4, "strict ABI2 removes 0x144 eligibility");
    draw(device, 0x1c4);
    count(calls, 5, 0x1c4, "strict ABI2 retains 0x1c4 eligibility");
    check(old_v2(nullptr, nullptr), A2FO_REFIT_OK, "unregister strict mesh callback");
    draw(device, 0x1c4);
    count(calls, 5, 0x1c4, "unregister clears mesh callback");
    check(new_v3(mesh_callback, &calls), A2FO_REFIT_OK, "re-register RHW v3 callback");
    draw(device, 0x144);
    count(calls, 6, 0x144, "RHW v3 re-enables 0x144 eligibility");
    check(opaque(nullptr, nullptr), A2FO_REFIT_OK, "generic clear removes mesh callback");
    draw(device, 0x144);
    draw(device, 0x1c4);
    count(calls, 6, 0x144, "generic clear removes all mesh eligibility");

    device->Release();
    d3d->Release();
    std::printf("PASS: %s Display RHW callback admission; no scene/gameplay acceptance\n", backend);
}
}
int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: refit_rhw_callback_runtime_test BACKEND DISPLAY_DLL\n");
        return 2;
    }
    run(argv[1], argv[2]);
    return 0;
}
