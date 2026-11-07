// Real D3D8-provider fixture. Run only in an isolated headless environment.
// The private window is never shown or activated, and no input is sent.
#include <d3d8.h>

#include <cstdio>
#include <cstdlib>

namespace {

constexpr unsigned kSide = 64;
constexpr D3DCOLOR kBackground = 0x00112233;
constexpr D3DCOLOR kFirstDot = 0x00c02040;
constexpr D3DCOLOR kSecondDot = 0x0000d0a0;
constexpr D3DCOLOR kEdgeDot = 0x00e0a010;

void require(bool condition, const char* check) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", check);
        std::exit(1);
    }
}

struct Fixture {
    HINSTANCE instance = GetModuleHandleA(nullptr);
    const char* class_name = "A2FORadarClearFixture";
    bool registered = false;
    HWND window = nullptr;
    IDirect3D8* d3d = nullptr;
    IDirect3DDevice8* device = nullptr;
    IDirect3DSurface8* backbuffer = nullptr;

    ~Fixture() {
        if (backbuffer) backbuffer->Release();
        if (device) device->Release();
        if (d3d) d3d->Release();
        if (window) DestroyWindow(window);
        if (registered) UnregisterClassA(class_name, instance);
    }
};

int skip(const char* reason, HRESULT result = S_OK) {
    std::printf("SKIP: %s HRESULT=0x%08lx\n", reason,
                static_cast<unsigned long>(result));
    return 77;
}

bool in_rect(unsigned x, unsigned y, const D3DRECT& rect) {
    return x >= static_cast<unsigned>(rect.x1) &&
           x < static_cast<unsigned>(rect.x2) &&
           y >= static_cast<unsigned>(rect.y1) &&
           y < static_cast<unsigned>(rect.y2);
}

void verify_pixels(IDirect3DSurface8* backbuffer, const D3DRECT& first,
                   const D3DRECT& second, const D3DRECT& edge,
                   const D3DRECT& viewport) {
    D3DLOCKED_RECT locked{};
    require(SUCCEEDED(backbuffer->LockRect(&locked, nullptr, D3DLOCK_READONLY)) &&
                locked.pBits, "read back cleared backbuffer");
    unsigned mismatches = 0;
    for (unsigned y = 0; y < kSide; ++y) {
        const auto* row = reinterpret_cast<const DWORD*>(
            static_cast<const BYTE*>(locked.pBits) + locked.Pitch * y);
        for (unsigned x = 0; x < kSide; ++x) {
            D3DCOLOR expected = kBackground;
            if (in_rect(x, y, viewport)) {
                if (in_rect(x, y, first)) expected = kFirstDot;
                if (in_rect(x, y, second)) expected = kSecondDot;
                if (in_rect(x, y, edge)) expected = kEdgeDot;
            }
            if ((row[x] & 0x00ffffffu) != expected) {
                if (mismatches++ < 8) {
                    std::fprintf(stderr,
                        "FAIL: pixel (%u,%u) expected=%06lx actual=%06lx\n",
                        x, y, static_cast<unsigned long>(expected),
                        static_cast<unsigned long>(row[x] & 0x00ffffffu));
                }
            }
        }
    }
    require(SUCCEEDED(backbuffer->UnlockRect()), "unlock backbuffer");
    require(mismatches == 0, "minimap rectangles and viewport clipping");
}

} // namespace

int main() {
    Fixture fixture;
    WNDCLASSEXA cls{};
    cls.cbSize = sizeof(cls);
    cls.hInstance = fixture.instance;
    cls.lpfnWndProc = DefWindowProcA;
    cls.lpszClassName = fixture.class_name;
    require(RegisterClassExA(&cls) != 0, "register private window class");
    fixture.registered = true;
    fixture.window = CreateWindowExA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        fixture.class_name, "private D3D8 radar clear fixture", WS_POPUP,
        0, 0, kSide, kSide, nullptr, nullptr, fixture.instance, nullptr);
    require(fixture.window && !IsWindowVisible(fixture.window),
            "create hidden nonactivating window");

    fixture.d3d = Direct3DCreate8(D3D_SDK_VERSION);
    if (!fixture.d3d) return skip("D3D8 provider unavailable");

    D3DPRESENT_PARAMETERS presentation{};
    presentation.BackBufferWidth = kSide;
    presentation.BackBufferHeight = kSide;
    presentation.BackBufferFormat = D3DFMT_X8R8G8B8;
    presentation.BackBufferCount = 1;
    presentation.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentation.hDeviceWindow = fixture.window;
    presentation.Windowed = TRUE;
    presentation.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    const HRESULT created = fixture.d3d->CreateDevice(0, D3DDEVTYPE_HAL,
        fixture.window, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &presentation, &fixture.device);
    if (FAILED(created) || !fixture.device)
        return skip("lockable D3D8 HAL device unavailable", created);

    const HRESULT obtained = fixture.device->GetBackBuffer(
        0, D3DBACKBUFFER_TYPE_MONO, &fixture.backbuffer);
    if (FAILED(obtained) || !fixture.backbuffer)
        return skip("lockable backbuffer unavailable", obtained);
    D3DLOCKED_RECT available{};
    const HRESULT locked = fixture.backbuffer->LockRect(
        &available, nullptr, D3DLOCK_READONLY);
    if (FAILED(locked) || !available.pBits)
        return skip("backbuffer readback unavailable", locked);
    require(SUCCEEDED(fixture.backbuffer->UnlockRect()),
            "unlock backbuffer after capability check");

    const D3DVIEWPORT8 full{0, 0, kSide, kSide, 0.0f, 1.0f};
    const D3DVIEWPORT8 radar{16, 16, 32, 32, 0.0f, 1.0f};
    const D3DRECT radar_bounds{16, 16, 48, 48};
    const D3DRECT first{20, 22, 24, 26};
    const D3DRECT second{33, 30, 37, 34};
    const D3DRECT edge{44, 44, 52, 52};
    require(SUCCEEDED(fixture.device->SetViewport(&full)),
            "set full backbuffer viewport");
    require(SUCCEEDED(fixture.device->Clear(0, nullptr, D3DCLEAR_TARGET,
                                           kBackground, 0.0f, 0)),
            "initialize background without depth buffer");
    require(SUCCEEDED(fixture.device->SetViewport(&radar)),
            "set radar subviewport");

    // Armada's ST3D_DeviceDirectX8::DrawRectangle uses this Clear shape:
    // one screen-space D3DRECT, target flag only, 0x00RRGGBB, Z=0, stencil=0.
    require(SUCCEEDED(fixture.device->Clear(1, &first, D3DCLEAR_TARGET,
                                           kFirstDot, 0.0f, 0)),
            "clear first dot outside BeginScene");
    require(SUCCEEDED(fixture.device->BeginScene()), "begin scene");
    require(SUCCEEDED(fixture.device->Clear(1, &second, D3DCLEAR_TARGET,
                                           kSecondDot, 0.0f, 0)),
            "clear second dot inside BeginScene");
    require(SUCCEEDED(fixture.device->Clear(1, &edge, D3DCLEAR_TARGET,
                                           kEdgeDot, 0.0f, 0)),
            "clear rectangle crossing viewport edge");
    require(SUCCEEDED(fixture.device->EndScene()), "end scene");

    D3DVIEWPORT8 current{};
    require(SUCCEEDED(fixture.device->GetViewport(&current)) &&
                current.X == radar.X && current.Y == radar.Y &&
                current.Width == radar.Width && current.Height == radar.Height &&
                current.MinZ == radar.MinZ && current.MaxZ == radar.MaxZ,
            "Clear preserves radar viewport");
    verify_pixels(fixture.backbuffer, first, second, edge, radar_bounds);

    require(SUCCEEDED(fixture.device->SetViewport(&full)),
            "restore full backbuffer viewport");
    require(SUCCEEDED(fixture.device->GetViewport(&current)) &&
                current.X == full.X && current.Y == full.Y &&
                current.Width == full.Width && current.Height == full.Height &&
                current.MinZ == full.MinZ && current.MaxZ == full.MaxZ,
            "full viewport restored");
    verify_pixels(fixture.backbuffer, first, second, edge, radar_bounds);
    std::puts("PASS: minimap Clear rectangles");
    return 0;
}
