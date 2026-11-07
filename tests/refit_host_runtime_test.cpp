#include "../renderer/refit/host.h"
#include "native11_model_fixture.hpp"

#include <windows.h>
#include <d3d8.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr unsigned WIDTH = 64, HEIGHT = 64;
static_assert(sizeof(void*) == 4, "fixture requires the i686 host ABI");
static_assert(sizeof(A2FORefitConfig) == 16 && offsetof(A2FORefitConfig, display_module) == 4);
static_assert(sizeof(A2FORefitModel) == 20 && offsetof(A2FORefitModel, bytes) == 12);
static_assert(sizeof(A2FORefitTexture) == 32 && offsetof(A2FORefitTexture, bgra) == 24);
static_assert(sizeof(A2FORefitNode) == 52 && offsetof(A2FORefitNode, shown) == 48);
static_assert(sizeof(A2FORefitObject) == 116 && offsetof(A2FORefitObject, nodes) == 64);
static_assert(sizeof(A2FORefitLight) == 52 && offsetof(A2FORefitLight, range) == 48);
static_assert(sizeof(A2FORefitFrame) == 188 && offsetof(A2FORefitFrame, objects) == 172);
static_assert(sizeof(A2FORefitReadback) == 28 && offsetof(A2FORefitReadback, bgra) == 20);
static_assert(sizeof(A2FORefitStatus) == 28 && offsetof(A2FORefitStatus, device_epoch) == 24);
static_assert(sizeof(A2FORefitApi) == 44 && offsetof(A2FORefitApi, shutdown) == 40);
static_assert(sizeof(BITMAPFILEHEADER) == 14 && sizeof(BITMAPINFOHEADER) == 40);

void need(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void check(int32_t actual, int32_t expected, const char* message) {
    if (actual != expected) {
        std::fprintf(stderr, "FAIL: %s: got %ld, expected %ld\n", message,
                     static_cast<long>(actual), static_cast<long>(expected));
        std::exit(1);
    }
}
void d3d_ok(HRESULT result, const char* message) {
    if (FAILED(result)) {
        std::fprintf(stderr, "FAIL: %s HRESULT=0x%08lx\n", message,
                     static_cast<unsigned long>(result));
        std::exit(1);
    }
}
void identity34(float (&v)[12]) {
    std::fill(std::begin(v), std::end(v), 0.f);
    v[0] = v[4] = v[8] = 1.f;
}
void identity44(float (&v)[16]) {
    std::fill(std::begin(v), std::end(v), 0.f);
    v[0] = v[5] = v[10] = v[15] = 1.f;
}
struct Pixel { unsigned b, g, r; };
Pixel at(const std::vector<uint8_t>& pixels, unsigned x, unsigned y) {
    const size_t offset = (static_cast<size_t>(y) * WIDTH + x) * 4;
    return {pixels[offset], pixels[offset + 1], pixels[offset + 2]};
}
unsigned changed(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    unsigned count = 0;
    for (size_t i = 0; i < a.size(); i += 4)
        if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2]) ++count;
    return count;
}
unsigned blue_pixels(const std::vector<uint8_t>& pixels) {
    unsigned count = 0;
    for (size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i] > pixels[i + 1] + 30 && pixels[i] > pixels[i + 2] + 30) ++count;
    return count;
}
uint64_t blue_total(const std::vector<uint8_t>& pixels) {
    uint64_t total = 0;
    for (size_t i = 0; i < pixels.size(); i += 4) total += pixels[i];
    return total;
}
void readback(const A2FORefitApi& api, uint32_t epoch, uint32_t serial,
              std::vector<uint8_t>& pixels) {
    A2FORefitReadback request{sizeof(request), epoch, serial, WIDTH, HEIGHT,
                               pixels.data(), static_cast<uint32_t>(pixels.size())};
    check(api.readback(&request), A2FO_REFIT_OK, "readback last successful frame");
}
void save_bmp(const std::string& path, const std::vector<uint8_t>& bgra) {
    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER image{};
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(image);
    file.bfSize = file.bfOffBits + static_cast<DWORD>(bgra.size());
    image.biSize = sizeof(image);
    image.biWidth = WIDTH;
    image.biHeight = -static_cast<LONG>(HEIGHT); // API readback is top-down BGRA.
    image.biPlanes = 1;
    image.biBitCount = 32;
    image.biCompression = BI_RGB;
    image.biSizeImage = static_cast<DWORD>(bgra.size());
    FILE* output = std::fopen(path.c_str(), "wb");
    need(output != nullptr, "open Present mismatch BMP");
    const bool written = std::fwrite(&file, sizeof(file), 1, output) == 1 &&
                         std::fwrite(&image, sizeof(image), 1, output) == 1 &&
                         std::fwrite(bgra.data(), 1, bgra.size(), output) == bgra.size();
    const int close_code = std::fclose(output);
    need(written && close_code == 0, "write Present mismatch BMP");
}
void present_and_preserve(IDirect3DDevice8* device, const A2FORefitApi& api,
                          uint32_t epoch, uint32_t serial,
                          const std::vector<uint8_t>& before, const char* output_dir) {
    const HRESULT hr = device->Present(nullptr, nullptr, nullptr, nullptr);
    if (FAILED(hr)) std::fprintf(stderr, "Present HRESULT=0x%08lx\n", static_cast<unsigned long>(hr));
    need(SUCCEEDED(hr), "native facade Present");
    std::vector<uint8_t> after(before.size());
    readback(api, epoch, serial, after);
    if (after != before) {
        size_t first = before.size();
        for (size_t i = 0; i < before.size(); i += 4) {
            if (before[i] != after[i] || before[i + 1] != after[i + 1] ||
                before[i + 2] != after[i + 2]) { first = i; break; }
        }
        const unsigned count = changed(before, after);
        if (first < before.size()) {
            const size_t pixel = first / 4;
            std::fprintf(stderr,
                "Present mismatch: serial=%lu changed_pixels=%u first=(%lu,%lu) before_rgb=(%u,%u,%u) after_rgb=(%u,%u,%u)\n",
                static_cast<unsigned long>(serial), count,
                static_cast<unsigned long>(pixel % WIDTH), static_cast<unsigned long>(pixel / WIDTH),
                before[first + 2], before[first + 1], before[first],
                after[first + 2], after[first + 1], after[first]);
        } else {
            std::fprintf(stderr, "Present mismatch: serial=%lu alpha-only difference\n",
                         static_cast<unsigned long>(serial));
        }
        const std::string stem = std::string(output_dir) + "/present-serial-" + std::to_string(serial);
        save_bmp(stem + "-before.bmp", before);
        save_bmp(stem + "-after.bmp", after);
        std::fprintf(stderr, "Present mismatch BMPs: %s-{before,after}.bmp\n", stem.c_str());
        need(false, "native Present preserves last renderer readback");
    }
}
struct Window {
    HINSTANCE instance = GetModuleHandleA(nullptr);
    HWND hwnd = nullptr;
    explicit Window(bool create_now = true) {
        WNDCLASSEXA cls{};
        cls.cbSize = sizeof(cls); cls.hInstance = instance;
        cls.lpfnWndProc = DefWindowProcA; cls.lpszClassName = "A2FORefitHostPrivateFixture";
        need(RegisterClassExA(&cls) != 0, "register private window class");
        if (create_now) create();
    }
    void create() {
        need(!hwnd, "create private HWND only once");
        hwnd = CreateWindowExA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, "A2FORefitHostPrivateFixture",
                               "private hidden Refit fixture", WS_POPUP, 0, 0,
                               WIDTH, HEIGHT, nullptr, nullptr, instance, nullptr);
        need(hwnd && !IsWindowVisible(hwnd), "create hidden owner-thread HWND");
    }
    ~Window() {
        if (hwnd) DestroyWindow(hwnd);
        UnregisterClassA("A2FORefitHostPrivateFixture", instance);
    }
};
using Create8 = IDirect3D8* (WINAPI*)(UINT);
template<class Function> Function export_of(HMODULE module, const char* name) {
    FARPROC raw = GetProcAddress(module, name);
    Function typed = nullptr;
    static_assert(sizeof(typed) == sizeof(raw), "Windows function pointer size");
    std::memcpy(&typed, &raw, sizeof(typed));
    return typed;
}
struct WrongThreadProbe {
    const A2FORefitApi* api;
    A2FORefitDisplayPassthrough passthrough;
    A2FORefitStatus output;
    int32_t status_result = 0;
    int32_t passthrough_result = 0;
};
DWORD WINAPI probe_wrong_thread(LPVOID raw) {
    auto& p = *static_cast<WrongThreadProbe*>(raw);
    p.status_result = p.api->status(&p.output);
    p.passthrough_result = p.passthrough(1);
    return 0;
}
struct HudVertex { float x, y, z, rhw; DWORD diffuse; float u, v; };
static_assert(sizeof(HudVertex) == 28);
void fill_hud_texture(IDirect3DTexture8* texture, DWORD argb) {
    D3DLOCKED_RECT locked{};
    d3d_ok(texture->LockRect(0, &locked, nullptr, 0), "lock facade HUD texture");
    for (unsigned y = 0; y < 2; ++y)
        for (unsigned x = 0; x < 2; ++x)
            std::memcpy(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch + x * 4,
                        &argb, sizeof(argb));
    d3d_ok(texture->UnlockRect(0), "unlock facade HUD texture");
}
void hud_state(IDirect3DDevice8* device, IDirect3DTexture8* texture) {
    d3d_ok(device->SetTexture(0, texture), "bind facade HUD texture");
    d3d_ok(device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1), "HUD FVF");
    d3d_ok(device->SetPixelShader(0), "fixed-function HUD pixel shader");
    d3d_ok(device->SetRenderState(D3DRS_LIGHTING, FALSE), "HUD lighting off");
    d3d_ok(device->SetRenderState(D3DRS_ZENABLE, FALSE), "HUD depth off");
    d3d_ok(device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE), "HUD depth writes off");
    d3d_ok(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE), "HUD culling off");
    d3d_ok(device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE), "HUD blending on");
    d3d_ok(device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA), "HUD source blend");
    d3d_ok(device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA), "HUD destination blend");
    d3d_ok(device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE), "HUD colour op");
    d3d_ok(device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE), "HUD colour texture");
    d3d_ok(device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE), "HUD colour diffuse");
    d3d_ok(device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE), "HUD alpha op");
    d3d_ok(device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE), "HUD alpha texture");
    d3d_ok(device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE), "HUD alpha diffuse");
    d3d_ok(device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_POINT), "HUD min filter");
    d3d_ok(device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_POINT), "HUD mag filter");
    d3d_ok(device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE), "HUD stage one off");
    d3d_ok(device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE), "HUD alpha stage one off");
}
void draw_hud_quad(IDirect3DDevice8* device) {
    const HudVertex quad[4] = {
        {1.5f, 1.5f, .5f, 1.f, 0xffffffff, 0.f, 0.f},
        {17.5f, 1.5f, .5f, 1.f, 0xffffffff, 1.f, 0.f},
        {1.5f, 17.5f, .5f, 1.f, 0xffffffff, 0.f, 1.f},
        {17.5f, 17.5f, .5f, 1.f, 0xffffffff, 1.f, 1.f},
    };
    d3d_ok(device->BeginScene(), "begin facade HUD scene");
    d3d_ok(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(HudVertex)),
           "draw facade HUD quad");
    d3d_ok(device->EndScene(), "end facade HUD scene");
}

void run(const char* backend, const char* display_path, const char* renderer_path,
         const char* output_dir, uint32_t renderer_flags, bool deferred_window) {
    const uint32_t requested_backend = std::strcmp(backend, "vulkan") == 0 ? 1u : 2u;
    need(requested_backend == 1 || std::strcmp(backend, "d3d12") == 0, "known backend argument");
    need(GetModuleHandleA("d3d8.dll") == nullptr && GetModuleHandleA("d3d9.dll") == nullptr,
         "no system Direct3D 8/9 module before test");
    Window window(!deferred_window);
    HMODULE display = LoadLibraryA(display_path);
    need(display != nullptr, "load staged Display DLL");
    auto configure = export_of<A2FORefitDisplayConfigure>(display, "A2FORefitDisplay_Configure_v1");
    auto prepare = export_of<A2FORefitDisplayPrepare>(display, "A2FORefitDisplay_Prepare_v1");
    auto create8 = export_of<Create8>(display, "Direct3DCreate8");
    need(configure && create8, "Display host exports");
    if (deferred_window) {
        need(prepare != nullptr, "Display deferred-window host export");
        check(prepare(requested_backend, WIDTH, HEIGHT, 0), 0, "prepare Display before HWND exists");
        check(prepare(requested_backend, WIDTH, HEIGHT, 0), -2, "reject second Display prepare");
    } else {
        check(configure(requested_backend, window.hwnd, WIDTH, HEIGHT, 0), 0, "configure Display once");
        check(configure(requested_backend, window.hwnd, WIDTH, HEIGHT, 0), -2,
              "reject second Display configure");
    }
    IDirect3D8* d3d = create8(D3D_SDK_VERSION);
    need(d3d != nullptr, "create Refit D3D8 facade");
    if (deferred_window) {
        D3DDISPLAYMODE mode{};
        d3d_ok(d3d->GetAdapterDisplayMode(0, &mode), "prepared adapter mode before HWND");
        need(mode.Width == WIDTH && mode.Height == HEIGHT && mode.Format == D3DFMT_X8R8G8B8,
             "prepared adapter mode matches requested dimensions and format");
        window.create();
    }
    D3DPRESENT_PARAMETERS pp{};
    pp.BackBufferWidth = WIDTH; pp.BackBufferHeight = HEIGHT;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8; pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow = window.hwnd;
    pp.Windowed = TRUE; pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    IDirect3DDevice8* device = nullptr;
    if (deferred_window) {
        D3DPRESENT_PARAMETERS rejected = pp;
        rejected.hDeviceWindow = nullptr;
        IDirect3DDevice8* invalid_device = reinterpret_cast<IDirect3DDevice8*>(1);
        check(d3d->CreateDevice(0, D3DDEVTYPE_HAL, nullptr,
                                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &rejected, &invalid_device),
              D3DERR_INVALIDCALL, "reject null HWND before binding");
        need(invalid_device == nullptr, "null HWND clears device output");

        HWND foreign = GetDesktopWindow();
        DWORD foreign_process = 0;
        need(foreign && GetWindowThreadProcessId(foreign, &foreign_process) != 0 &&
             foreign_process != GetCurrentProcessId(), "desktop HWND belongs to another process");
        rejected.hDeviceWindow = foreign;
        invalid_device = reinterpret_cast<IDirect3DDevice8*>(1);
        check(d3d->CreateDevice(0, D3DDEVTYPE_HAL, foreign,
                                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &rejected, &invalid_device),
              D3DERR_INVALIDCALL, "reject foreign-process HWND before binding");
        need(invalid_device == nullptr, "foreign HWND clears device output");
    }
    HRESULT hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, window.hwnd,
                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    if (FAILED(hr)) std::fprintf(stderr, "CreateDevice HRESULT=0x%08lx\n", static_cast<unsigned long>(hr));
    need(SUCCEEDED(hr) && device, "create Refit facade device");
    if (deferred_window) {
        HWND other = CreateWindowExA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                                     "A2FORefitHostPrivateFixture", "second hidden Refit fixture",
                                     WS_POPUP, 0, 0, WIDTH, HEIGHT, nullptr, nullptr,
                                     window.instance, nullptr);
        need(other && !IsWindowVisible(other), "create second hidden owner-thread HWND");
        D3DPRESENT_PARAMETERS rejected = pp;
        rejected.hDeviceWindow = other;
        IDirect3DDevice8* invalid_device = reinterpret_cast<IDirect3DDevice8*>(1);
        check(d3d->CreateDevice(0, D3DDEVTYPE_HAL, other,
                                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &rejected, &invalid_device),
              D3DERR_INVALIDCALL, "reject retarget to second owner-thread HWND");
        need(invalid_device == nullptr, "retarget rejection clears device output");
        need(DestroyWindow(other) != 0, "destroy second hidden HWND");
    }

    HMODULE renderer = LoadLibraryA(renderer_path);
    need(renderer != nullptr, "load staged Renderer DLL");
    need(GetModuleHandleA("UI.asi") == nullptr &&
         GetProcAddress(renderer, "Render3D_Settings") == nullptr,
         "host scene does not load or expose Refit in-game UI");
    auto get_api = export_of<A2FORefitGetInterface>(renderer, "A2FORefitRenderer_GetInterface");
    need(get_api, "Renderer host export");
    need(get_api(0) == nullptr && get_api(2) == nullptr, "reject unsupported ABI versions");
    const A2FORefitApi* api = get_api(A2FO_REFIT_ABI);
    need(api && api->size >= sizeof(*api) && api->abi_version == A2FO_REFIT_ABI,
         "Renderer v1 table size and version");
    need((api->capabilities & (A2FO_REFIT_CAP_SOD | A2FO_REFIT_CAP_POSE |
                               A2FO_REFIT_CAP_TEXTURE | A2FO_REFIT_CAP_LIGHTS |
                               A2FO_REFIT_CAP_READBACK)) ==
         (A2FO_REFIT_CAP_SOD | A2FO_REFIT_CAP_POSE | A2FO_REFIT_CAP_TEXTURE |
          A2FO_REFIT_CAP_LIGHTS | A2FO_REFIT_CAP_READBACK), "required scene capabilities");

    A2FORefitConfig config{sizeof(config), display, renderer_flags, 1};
    A2FORefitConfig bad_config = config;
    bad_config.size = sizeof(config) - 4;
    check(api->initialize(&bad_config), A2FO_REFIT_INVALID,
          "reject wrong host config size before attachment");
    bad_config = config; bad_config.flags = 8;
    check(api->initialize(&bad_config), A2FO_REFIT_INVALID,
          "reject unknown renderer flag before attachment");
    check(api->initialize(&config), A2FO_REFIT_OK, "initialize host renderer");
    A2FORefitStatus status{}; status.size = sizeof(status);
    check(api->status(&status), A2FO_REFIT_OK, "initial status");
    need(status.backend == requested_backend && status.source_epoch == 1 &&
         status.last_serial == 0 && status.device_epoch > 0, "initial backend and epoch");
    auto passthrough = export_of<A2FORefitDisplayPassthrough>(
        display, "A2FORefitDisplay_Passthrough_v1");
    need(passthrough != nullptr, "Display optional host passthrough export");
    WrongThreadProbe wrong_thread{api, passthrough, {}};
    std::memset(&wrong_thread.output, 0xa5, sizeof(wrong_thread.output));
    wrong_thread.output.size = sizeof(wrong_thread.output);
    const A2FORefitStatus before_wrong_thread = wrong_thread.output;
    HANDLE worker = CreateThread(nullptr, 0, probe_wrong_thread, &wrong_thread, 0, nullptr);
    need(worker != nullptr, "create wrong-owner-thread probe");
    need(WaitForSingleObject(worker, 10000) == WAIT_OBJECT_0, "join wrong-owner-thread probe");
    CloseHandle(worker);
    check(wrong_thread.status_result, A2FO_REFIT_STATE,
          "wrong-thread renderer status rejected");
    check(wrong_thread.passthrough_result, A2FO_REFIT_STATE,
          "wrong-thread Display passthrough rejected");
    need(std::memcmp(&wrong_thread.output, &before_wrong_thread,
                     sizeof(before_wrong_thread)) == 0,
         "wrong-thread status leaves output untouched");

    std::array<uint8_t, 16> blue_bgra{};
    for (size_t i = 0; i < blue_bgra.size(); i += 4) {
        blue_bgra[i] = 255; blue_bgra[i + 3] = 255;
    }
    const char texture_name[] = "fixture.tga";
    A2FORefitTexture texture{sizeof(texture), texture_name,
                             static_cast<uint32_t>(std::strlen(texture_name)), 1, 2, 2,
                             blue_bgra.data(), static_cast<uint32_t>(blue_bgra.size())};
    check(api->upload_texture(&texture), A2FO_REFIT_OK, "upload blue texture");
    auto model_bytes = native11_fixture::model(texture_name, false);
    A2FORefitModel model{sizeof(model), 7, 1, model_bytes.data(),
                          static_cast<uint32_t>(model_bytes.size())};
    uint32_t node_count = 0;
    check(api->upload_model(&model, &node_count), A2FO_REFIT_OK, "upload synthetic SOD");
    need(node_count == 1, "synthetic SOD node count");
    std::fill(model_bytes.begin(), model_bytes.end(), 0);
    std::fill(blue_bgra.begin(), blue_bgra.end(), 0);

    A2FORefitNode node{}; identity34(node.world); node.shown = 1;
    A2FORefitObject object{}; object.size = sizeof(object);
    object.model_id = 7; object.model_revision = 1; object.flags = A2FO_REFIT_UNLIT;
    identity34(object.world); object.nodes = &node; object.node_count = 1;
    object.alpha = 1; object.team[0] = object.team[1] = object.team[2] = 1;
    object.ambient[0] = object.ambient[1] = object.ambient[2] = 1;
    A2FORefitFrame frame{}; frame.size = sizeof(frame); frame.source_epoch = 1;
    frame.serial = 1; frame.width = WIDTH; frame.height = HEIGHT;
    identity44(frame.view); identity44(frame.projection);
    frame.eye[2] = -2; frame.objects = &object; frame.object_count = 1;
    check(api->render(&frame), A2FO_REFIT_OK, "render copied blue model and texture");
    std::vector<uint8_t> first(WIDTH * HEIGHT * 4);
    readback(*api, 1, 1, first);
    need(blue_pixels(first) > 20, "blue textured SOD covers visible pixels");
    need(at(first, 0, 0).b < 30, "scene background differs from blue SOD");
    present_and_preserve(device, *api, 1, 1, first, output_dir);

    std::vector<uint8_t> untouched(first.size(), 0x5a);
    A2FORefitReadback wrong{sizeof(wrong), 1, 2, WIDTH, HEIGHT,
                            untouched.data(), static_cast<uint32_t>(untouched.size())};
    need(api->readback(&wrong) != A2FO_REFIT_OK, "reject unrendered serial readback");
    need(std::all_of(untouched.begin(), untouched.end(), [](uint8_t value){return value == 0x5a;}),
         "rejected readback leaves output untouched");
    wrong.serial = 1; wrong.size = sizeof(wrong) - 4;
    check(api->readback(&wrong), A2FO_REFIT_INVALID, "reject wrong readback struct size");
    need(std::all_of(untouched.begin(), untouched.end(), [](uint8_t value){return value == 0x5a;}),
         "bad-size readback leaves output untouched");
    check(api->render(&frame), A2FO_REFIT_STALE, "reject duplicate frame serial");
    std::vector<uint8_t> still(first.size()); readback(*api, 1, 1, still);
    need(still == first, "stale frame did not mutate last image");

    frame.serial = 2; object.model_revision = 9;
    check(api->render(&frame), A2FO_REFIT_STALE, "reject stale model revision");
    object.model_revision = 1;
    check(api->status(&status), A2FO_REFIT_OK, "status after rejected frame");
    need(status.last_serial == 1, "rejected frame did not advance serial");

    frame.serial = 2; object.world[9] = .4f; node.world[9] = .4f;
    check(api->render(&frame), A2FO_REFIT_OK, "render explicit translated pose");
    std::vector<uint8_t> moved(first.size()); readback(*api, 1, 2, moved);
    need(changed(first, moved) > 20, "explicit pose changes native pixels");
    present_and_preserve(device, *api, 1, 2, moved, output_dir);

    object.world[9] = 0; node.world[9] = 0; object.flags = 0;
    object.ambient[0] = object.ambient[1] = object.ambient[2] = 0;
    A2FORefitLight light{}; light.kind = A2FO_REFIT_DIRECTIONAL;
    // Source face winding gives +Z; Refit's NORMAL_SIGN=-1 makes the lit
    // front -Z. Directional stores travel direction, so +Z travels onto it.
    light.direction[2] = 1; light.colour[0] = light.colour[1] = light.colour[2] = 1;
    light.intensity = 1;
    frame.lights = &light; frame.light_count = 1; frame.serial = 3;
    check(api->render(&frame), A2FO_REFIT_OK, "render directional light");
    std::vector<uint8_t> directional(first.size()); readback(*api, 1, 3, directional);
    light.direction[2] = -1; frame.serial = 4;
    check(api->render(&frame), A2FO_REFIT_OK, "render opposite directional light");
    std::vector<uint8_t> opposite(first.size()); readback(*api, 1, 4, opposite);
    need(blue_total(directional) > blue_total(opposite) + 1000,
         "front-facing directional light exceeds backlight");
    light.kind = A2FO_REFIT_POINT; light.position[2] = -.8f;
    light.start = 0; light.range = 2.f; frame.serial = 5;
    check(api->render(&frame), A2FO_REFIT_OK, "render point light");
    std::vector<uint8_t> point(first.size()); readback(*api, 1, 5, point);
    need(blue_total(point) > blue_total(opposite) + 500,
         "front-side point light exceeds backlight");
    need(changed(directional, point) > 10, "light kind changes native pixels");
    present_and_preserve(device, *api, 1, 5, point, output_dir);
    std::printf("light blue totals: directional=%llu opposite=%llu point=%llu\n",
                static_cast<unsigned long long>(blue_total(directional)),
                static_cast<unsigned long long>(blue_total(opposite)),
                static_cast<unsigned long long>(blue_total(point)));

    // Existing FO-style D3D8 HUD draws belong to the host only while it asks
    // Display to pass them through. Refit's in-game UI remains absent.
    need(GetModuleHandleA("UI.asi") == nullptr, "Refit UI.asi remains unloaded");
    IDirect3DTexture8* hud_texture = nullptr;
    d3d_ok(device->CreateTexture(2, 2, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                 &hud_texture), "create facade HUD texture");
    need(hud_texture != nullptr, "facade HUD texture pointer");
    fill_hud_texture(hud_texture, 0x8000ff00u);
    hud_state(device, hud_texture);
    frame.serial = 6;
    check(api->render(&frame), A2FO_REFIT_OK, "render scene before FO HUD quad");
    std::vector<uint8_t> hud_base(first.size()); readback(*api, 1, 6, hud_base);
    need(at(hud_base, 5, 5).r < 20 && at(hud_base, 5, 5).g < 20 &&
         at(hud_base, 5, 5).b < 20, "HUD corner starts as scene background");
    draw_hud_quad(device);
    std::vector<uint8_t> default_off(first.size()); readback(*api, 1, 6, default_off);
    need(default_off == hud_base, "host passthrough defaults off");
    check(passthrough(2), A2FO_REFIT_INVALID, "reject invalid passthrough toggle");
    check(passthrough(1), A2FO_REFIT_OK, "enable explicit FO HUD passthrough");
    draw_hud_quad(device);
    std::vector<uint8_t> composite(first.size()); readback(*api, 1, 6, composite);
    const Pixel hud = at(composite, 5, 5);
    need(hud.g >= 95 && hud.g <= 160 && hud.r < 25 && hud.b < 25,
         "half-alpha green HUD quad blends over black scene corner");
    const Pixel model_before = at(hud_base, 32, 32), model_after = at(composite, 32, 32);
    need(model_before.b > 25 && model_after.b == model_before.b &&
         model_after.g == model_before.g && model_after.r == model_before.r,
         "HUD overlay preserves underlying model away from quad");
    const std::string hud_bmp = std::string(output_dir) + "/fo-hud-composite-" + backend + ".bmp";
    save_bmp(hud_bmp, composite);
    std::printf("HUD composite: corner_rgb=(%u,%u,%u) model_rgb=(%u,%u,%u) image=%s\n",
                hud.r, hud.g, hud.b, model_after.r, model_after.g, model_after.b,
                hud_bmp.c_str());
    present_and_preserve(device, *api, 1, 6, composite, output_dir);
    check(passthrough(0), A2FO_REFIT_OK, "disable explicit FO HUD passthrough");
    frame.serial = 7;
    check(api->render(&frame), A2FO_REFIT_OK, "fresh scene after HUD ownership release");
    std::vector<uint8_t> released(first.size()); readback(*api, 1, 7, released);
    fill_hud_texture(hud_texture, 0xffff0000u);
    draw_hud_quad(device);
    std::vector<uint8_t> red_dropped(first.size()); readback(*api, 1, 7, red_dropped);
    need(red_dropped == released, "disabled passthrough drops red opaque HUD draw");
    d3d_ok(device->SetTexture(0, nullptr), "unbind facade HUD texture");
    hud_texture->Release();

    // The SOD encoder's final seven bytes are cull, trailing word,
    // channel count and animation-ref count; cull=1 makes this one-sided.
    auto one_sided = native11_fixture::model(texture_name, false);
    need(one_sided.size() >= 7 && one_sided[one_sided.size() - 7] == 0,
         "synthetic SOD cull byte at documented tail");
    one_sided[one_sided.size() - 7] = 1;
    A2FORefitModel sided_model{sizeof(sided_model), 8, 1, one_sided.data(),
                               static_cast<uint32_t>(one_sided.size())};
    uint32_t sided_nodes = 0;
    check(api->upload_model(&sided_model, &sided_nodes), A2FO_REFIT_OK,
          "upload one-sided synthetic SOD");
    need(sided_nodes == 1, "one-sided SOD node count");
    object.model_id = 8; object.flags = A2FO_REFIT_UNLIT;
    object.ambient[0] = object.ambient[1] = object.ambient[2] = 1;
    frame.lights = nullptr; frame.light_count = 0; frame.serial = 8;
    check(api->render(&frame), A2FO_REFIT_OK, "one-sided front face render");
    std::vector<uint8_t> sided(first.size()); readback(*api, 1, 8, sided);
    need(blue_pixels(sided) > 20, "one-sided front face remains visible");
    node.world[0] = -1.f; frame.serial = 9;
    check(api->render(&frame), A2FO_REFIT_OK, "mirrored one-sided node render");
    std::vector<uint8_t> mirrored(first.size()); readback(*api, 1, 9, mirrored);
    need(blue_pixels(mirrored) > 20, "mirrored basis reverses cull and remains visible");
    node.shown = 0; frame.serial = 10;
    check(api->render(&frame), A2FO_REFIT_OK, "hidden one-sided node render");
    std::vector<uint8_t> hidden(first.size()); readback(*api, 1, 10, hidden);
    need(blue_pixels(hidden) == 0 && at(hidden, 32, 32).b < 20,
         "hidden node contributes no model pixels");

    check(api->reset_content(2), A2FO_REFIT_OK, "advance content epoch");
    check(api->status(&status), A2FO_REFIT_OK, "status after content reset");
    need(status.source_epoch == 2 && status.last_serial == 0 && status.models == 0 &&
         status.textures == 0, "reset clears datasets and frame");
    wrong.size = sizeof(wrong); wrong.source_epoch = 1; wrong.serial = 10;
    check(api->readback(&wrong), A2FO_REFIT_STALE, "reject old-epoch readback");
    need(std::all_of(untouched.begin(), untouched.end(), [](uint8_t value){return value == 0x5a;}),
         "old-epoch readback leaves output untouched");
    frame.serial = 11;
    check(api->render(&frame), A2FO_REFIT_STALE, "old epoch frame rejected");
    check(api->shutdown(), A2FO_REFIT_OK, "terminal renderer shutdown");
    need(GetModuleHandleA("d3d8.dll") == nullptr && GetModuleHandleA("d3d9.dll") == nullptr,
         "never loaded system Direct3D 8/9");
    device->Release(); d3d->Release();
    need(!IsWindowVisible(window.hwnd), "host left fixture HWND hidden");
    // Host ABI v1 pins both loaded helper modules for process lifetime.
    (void)renderer; (void)display;
    std::printf("PASS: %s real Refit host GPU scene, copies, pose, light, epochs and readback\n", backend);
}
}

int main(int argc, char** argv) {
    if (argc != 5 && argc != 6 && argc != 7) {
        std::fprintf(stderr, "usage: fixture BACKEND DISPLAY_DLL RENDERER_DLL OUTPUT_DIR [RENDERER_FLAGS [DEFERRED_WINDOW]]\n");
        return 2;
    }
    uint32_t flags = 0;
    if (argc >= 6) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(argv[5], &end, 0);
        if (!argv[5][0] || !end || *end != '\0' || parsed > 7) {
            std::fprintf(stderr, "RENDERER_FLAGS must be an integer from 0 to 7\n");
            return 2;
        }
        flags = static_cast<uint32_t>(parsed);
    }
    bool deferred_window = false;
    if (argc == 7) {
        if (std::strcmp(argv[6], "0") != 0 && std::strcmp(argv[6], "1") != 0) {
            std::fprintf(stderr, "DEFERRED_WINDOW must be 0 or 1\n");
            return 2;
        }
        deferred_window = argv[6][0] == '1';
    }
    run(argv[1], argv[2], argv[3], argv[4], flags, deferred_window);
    return 0;
}
