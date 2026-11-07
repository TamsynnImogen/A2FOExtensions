// Real D3D8-provider fixture. Run inside a private headless compositor only.
// Its window is never shown or activated, and it sends no input.
// Include privately so the fixture can compare observed calls with the saved
// native functions, without exporting a bypass API in the installed core.
#include "../renderer/d3d8_compat/observer.cpp"

#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::vector<std::string> messages;

void log_line(const std::string& line) {
    messages.push_back(line);
    std::printf("%s\n", line.c_str());
    std::fflush(stdout);
}

void phase(const char* name) {
    std::printf("PHASE: %s\n", name);
    std::fflush(stdout);
}

void require(bool condition, const char* check) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", check);
        std::fflush(stderr);
        std::exit(1);
    }
}

template <typename T> void release(T*& object) {
    if (object) { object->Release(); object = nullptr; }
}

struct WindowState {
    LONG style;
    LONG exstyle;
    RECT rect;
    bool visible;
};

WindowState window_state(HWND window) {
    WindowState result{};
    result.style = GetWindowLongA(window, GWL_STYLE);
    result.exstyle = GetWindowLongA(window, GWL_EXSTYLE);
    require(GetWindowRect(window, &result.rect), "window rect");
    result.visible = IsWindowVisible(window) != FALSE;
    return result;
}

bool same_state(const WindowState& a, const WindowState& b) {
    return a.style == b.style && a.exstyle == b.exstyle &&
        a.rect.left == b.rect.left && a.rect.top == b.rect.top &&
        a.rect.right == b.rect.right && a.rect.bottom == b.rect.bottom &&
        a.visible == b.visible;
}
void print_state(const char* phase_name,const WindowState& s) {
    std::printf("WINDOW %s style=%08lx exstyle=%08lx rect=%ld,%ld,%ld,%ld visible=%d\n",
        phase_name,static_cast<unsigned long>(s.style),static_cast<unsigned long>(s.exstyle),s.rect.left,s.rect.top,s.rect.right,s.rect.bottom,s.visible);
    std::fflush(stdout);
}

unsigned probe_count() {
    unsigned count = 0;
    for (const auto& line : messages)
        if (line.find("[D3D11] Probe complete HRESULT=") != std::string::npos) ++count;
    return count;
}

HRESULT probe_result() {
    unsigned long value = 0;
    for (const auto& line : messages)
        if (std::sscanf(line.c_str(), "[D3D11] Probe complete HRESULT=0x%lx", &value) == 1)
            return static_cast<HRESULT>(value);
    return E_UNEXPECTED;
}

template <typename T>
void verify_resource_identity(T* resource, IDirect3DDevice8* device) {
    IDirect3DDevice8* owner = nullptr;
    require(SUCCEEDED(resource->GetDevice(&owner)) && owner == device,
            "resource GetDevice native identity");
    release(owner);
}

void exercise_resources(IDirect3DDevice8* device) {
    IDirect3DVertexBuffer8* vertices = nullptr;
    IDirect3DIndexBuffer8* indices = nullptr;
    IDirect3DTexture8* texture = nullptr;
    IDirect3DSurface8* surface = nullptr;
    require(SUCCEEDED(device->CreateVertexBuffer(64, 0, D3DFVF_XYZ,
                                                 D3DPOOL_MANAGED, &vertices)),
            "create vertex buffer");
    verify_resource_identity(vertices, device);
    BYTE* bytes = nullptr;
    require(SUCCEEDED(vertices->Lock(0, 0, &bytes, 0)) && bytes,
            "lock vertex buffer");
    std::memset(bytes, 0, 64);
    require(SUCCEEDED(vertices->Unlock()), "unlock vertex buffer");

    require(SUCCEEDED(device->CreateIndexBuffer(12, 0, D3DFMT_INDEX16,
                                                D3DPOOL_MANAGED, &indices)),
            "create index buffer");
    verify_resource_identity(indices, device);
    bytes = nullptr;
    require(SUCCEEDED(indices->Lock(0, 0, &bytes, 0)) && bytes,
            "lock index buffer");
    std::memset(bytes, 0, 12);
    require(SUCCEEDED(indices->Unlock()), "unlock index buffer");

    require(SUCCEEDED(device->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8,
                                            D3DPOOL_MANAGED, &texture)),
            "create texture");
    verify_resource_identity(texture, device);
    D3DLOCKED_RECT locked{};
    require(SUCCEEDED(texture->LockRect(0, &locked, nullptr, 0)) && locked.pBits,
            "lock texture");
    std::memset(locked.pBits, 0x5a, locked.Pitch * 4);
    require(SUCCEEDED(texture->UnlockRect(0)), "unlock texture");
    require(SUCCEEDED(texture->GetSurfaceLevel(0, &surface)) && surface,
            "get texture surface");
    verify_resource_identity(surface, device);
    locked = {};
    require(SUCCEEDED(surface->LockRect(&locked, nullptr, 0)) && locked.pBits,
            "lock texture surface");
    require(SUCCEEDED(surface->UnlockRect()), "unlock texture surface");

    release(surface);
    release(texture);
    release(indices);
    release(vertices);
}

void exercise_shaders(IDirect3DDevice8* device) {
    const DWORD declaration[]={D3DVSD_STREAM(0),D3DVSD_REG(0,D3DVSDT_FLOAT3),D3DVSD_END()};
    const DWORD vertex_code[]={0xfffe0101,D3DSIO_MOV,0xc00f0000,0x90e40000,D3DSIO_END};
    const DWORD pixel_code[]={0xffff0101,D3DSIO_MOV,0x800f0000,0x90e40000,D3DSIO_END};
    DWORD vertex=0,pixel=0;
    require(SUCCEEDED(device->CreateVertexShader(declaration,vertex_code,&vertex,0)),"native vertex shader creation with diagnostic capture");
    require(SUCCEEDED(device->CreatePixelShader(pixel_code,&pixel)),"native pixel shader creation with diagnostic capture");
    // Compare against the provider itself: wrappers can translate function
    // readback or refuse declaration readback. Preserve those native results.
    const auto readback=[&](auto getter,unsigned slot,DWORD handle,const char* name) {
        using Getter=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*,DWORD,void*,DWORD*);
        const auto record=a2fo::renderer::find_record(device);
        require(record!=nullptr,"shader device observation metadata");
        const auto native=a2fo::renderer::function_at<Getter>(record->original[slot]);
        DWORD expected_bytes=0,bytes=0;
        const HRESULT expected=native(device,handle,nullptr,&expected_bytes);
        const HRESULT observed=getter(nullptr,&bytes);
        require(observed==expected&&bytes==expected_bytes,"shader size query exactly forwards native result");
        std::printf("SHADER %s native size-query HRESULT=%08lx bytes=%lu\n",name,static_cast<unsigned long>(expected),bytes);
        if(FAILED(expected))return;
        require(bytes>=4&&bytes<=65536&&bytes%4==0,"bounded native shader readback size");
        std::vector<DWORD> expected_words(bytes/4,0xcccccccc),words=expected_words;
        const HRESULT expected_data=native(device,handle,expected_words.data(),&expected_bytes);
        const HRESULT observed_data=getter(words.data(),&bytes);
        require(expected_data==observed_data&&bytes==expected_bytes&&words==expected_words,"shader readback exactly forwards native bytes and result");
    };
    readback([&](void* data,DWORD* bytes){return device->GetVertexShaderFunction(vertex,data,bytes);},82,vertex,"VS");
    readback([&](void* data,DWORD* bytes){return device->GetVertexShaderDeclaration(vertex,data,bytes);},81,vertex,"declaration");
    readback([&](void* data,DWORD* bytes){return device->GetPixelShaderFunction(pixel,data,bytes);},93,pixel,"PS");
    require(SUCCEEDED(device->DeleteVertexShader(vertex)),"native vertex shader deletion");
    require(SUCCEEDED(device->DeletePixelShader(pixel)),"native pixel shader deletion");
}

void verify_backbuffer(IDirect3DDevice8* device, D3DCOLOR expected) {
    IDirect3DSurface8* backbuffer = nullptr;
    require(SUCCEEDED(device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO,
                                            &backbuffer)) && backbuffer,
            "get lockable backbuffer");
    verify_resource_identity(backbuffer, device);
    D3DLOCKED_RECT locked{};
    require(SUCCEEDED(backbuffer->LockRect(&locked, nullptr, D3DLOCK_READONLY)) &&
            locked.pBits, "read back cleared backbuffer");
    const auto* row = reinterpret_cast<const DWORD*>(
        static_cast<const BYTE*>(locked.pBits) + locked.Pitch * 16);
    require((row[16] & 0x00ffffffu) == (expected & 0x00ffffffu),
            "D3D8 backbuffer RGB preserved");
    require(SUCCEEDED(backbuffer->UnlockRect()), "unlock backbuffer");
    release(backbuffer);
}

} // namespace

int main(int argc, char** argv) {
    require(argc == 2, "isolated output root argument");
    const std::string root = argv[1];
    const std::string ini = root + "\\A2FORenderer.ini";
    require(WritePrivateProfileStringA("D3D8Compat", "Backend", "d3d11-probe", ini.c_str()),
            "write probe backend");
    require(WritePrivateProfileStringA("D3D8Compat", "Trace", "1", ini.c_str()),
            "write trace setting");
    require(WritePrivateProfileStringA("D3D8Compat", "DebugLayer", "0", ini.c_str()),
            "write debug setting");
    require(a2fo::renderer::install_d3d8_observer(GetModuleHandleA(nullptr), nullptr,
                                                  root, log_line),
            "patch real executable D3D8 factory import");

    WNDCLASSEXA cls{};
    cls.cbSize = sizeof(cls);
    cls.hInstance = GetModuleHandleA(nullptr);
    cls.lpfnWndProc = DefWindowProcA;
    cls.lpszClassName = "A2FOD3D8D3D11WindowFixture";
    require(RegisterClassExA(&cls) != 0, "register private window class");
    HWND window = CreateWindowExA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        cls.lpszClassName, "private D3D8 and D3D11 fixture", WS_POPUP,
        0, 0, 64, 64, nullptr, nullptr, cls.hInstance, nullptr);
    require(window && !IsWindowVisible(window), "hidden nonactivating window");
    const WindowState created = window_state(window);
    print_state("created",created);

    IDirect3D8* d3d = Direct3DCreate8(D3D_SDK_VERSION);
    require(d3d != nullptr, "real imported D3D8 factory");
    void* identity = nullptr;
    require(SUCCEEDED(d3d->QueryInterface(IID_IUnknown, &identity)) && identity == d3d,
            "factory native IUnknown identity");
    static_cast<IUnknown*>(identity)->Release();
    D3DCAPS8 caps{};
    require(SUCCEEDED(d3d->GetDeviceCaps(0, D3DDEVTYPE_HAL, &caps)),
            "real D3D8 HAL caps");

    D3DPRESENT_PARAMETERS presentation{};
    presentation.BackBufferWidth = 64;
    presentation.BackBufferHeight = 64;
    presentation.BackBufferFormat = D3DFMT_X8R8G8B8;
    presentation.BackBufferCount = 1;
    presentation.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentation.hDeviceWindow = window;
    presentation.Windowed = TRUE;
    presentation.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    IDirect3DDevice8* device = nullptr;
    require(SUCCEEDED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &presentation, &device)) && device,
        "real D3D8 HAL device");
    identity = nullptr;
    require(SUCCEEDED(device->QueryInterface(IID_IUnknown, &identity)) &&
            identity == device, "device native IUnknown identity");
    static_cast<IUnknown*>(identity)->Release();
    D3DCAPS8 device_caps{};
    require(SUCCEEDED(device->GetDeviceCaps(&device_caps)) &&
            device_caps.DeviceType == D3DDEVTYPE_HAL, "native device caps");
    const WindowState before = window_state(window);
    print_state("legacy-device-created",before);

    exercise_resources(device);
    exercise_shaders(device);
    phase("D3D8 resources complete");
    DWORD state = 0;
    require(SUCCEEDED(device->SetRenderState(D3DRS_LIGHTING, FALSE)),
            "set D3D8 render state");
    D3DVIEWPORT8 viewport{1, 2, 60, 59, 0.0f, 1.0f};
    require(SUCCEEDED(device->SetViewport(&viewport)), "set D3D8 viewport");
    const D3DCOLOR colour = D3DCOLOR_XRGB(0x28, 0x70, 0xb0);
    require(SUCCEEDED(device->Clear(0, nullptr, D3DCLEAR_TARGET, colour, 1.0f, 0)),
            "first D3D8 clear");
    verify_backbuffer(device, colour);
    phase("first D3D8 backbuffer verified");
    require(SUCCEEDED(device->Present(nullptr, nullptr, nullptr, nullptr)),
            "first D3D8 Present triggers probe");
    phase("first Present returned");
    require(probe_count() == 1, "exactly one D3D11 probe completion");
    const HRESULT diagnostic_result = probe_result();
    const bool unavailable = diagnostic_result == DXGI_ERROR_UNSUPPORTED ||
        diagnostic_result == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE ||
        diagnostic_result == HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    if (unavailable) {
        std::printf("SKIP: D3D11 probe unavailable HRESULT=0x%08lx\n",
                    static_cast<unsigned long>(diagnostic_result));
        release(device);
        release(d3d);
        DestroyWindow(window);
        UnregisterClassA(cls.lpszClassName, cls.hInstance);
        return 77;
    }
    require(SUCCEEDED(diagnostic_result), "D3D11 probe returned success");
    phase("probe result verified");
    print_state("after-probe",window_state(window));
    require(same_state(before, window_state(window)),
            "window style, rect and visibility preserved");
    require(SUCCEEDED(device->GetRenderState(D3DRS_LIGHTING, &state)) && !state,
            "D3D8 render state survived probe");
    D3DVIEWPORT8 after{};
    require(SUCCEEDED(device->GetViewport(&after)) &&
            std::memcmp(&after, &viewport, sizeof(after)) == 0,
            "D3D8 viewport survived probe");

    DWORD state_block = 0;
    require(SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &state_block)),
            "create state block");
    require(SUCCEEDED(device->ApplyStateBlock(state_block)), "apply state block");
    require(SUCCEEDED(device->DeleteStateBlock(state_block)), "delete state block");
    phase("D3D8 state block complete");
    require(SUCCEEDED(device->Clear(0, nullptr, D3DCLEAR_TARGET, colour, 1.0f, 0)),
            "next D3D8 clear");
    verify_backbuffer(device, colour);
    require(SUCCEEDED(device->Present(nullptr, nullptr, nullptr, nullptr)),
            "next D3D8 Present");
    require(probe_count() == 1, "probe remains one-shot");
    phase("second Present complete");

    require(SUCCEEDED(device->Reset(&presentation)), "D3D8 Reset after resource release");
    require(SUCCEEDED(device->Clear(0, nullptr, D3DCLEAR_TARGET, colour, 1.0f, 0)),
            "post-Reset D3D8 clear");
    require(SUCCEEDED(device->Present(nullptr, nullptr, nullptr, nullptr)),
            "post-Reset D3D8 Present");
    require(probe_count() == 1, "Reset does not repeat probe");
    phase("Reset and post-Reset Present complete");
    require(same_state(before, window_state(window)), "window preserved through Reset");

    release(device);
    release(d3d);
    a2fo::renderer::flush_d3d8_observer();
    require(DestroyWindow(window), "destroy private window after COM release");
    require(UnregisterClassA(cls.lpszClassName, cls.hInstance), "unregister class");
    std::puts("PASS: real D3D8 factory and resources, one D3D11 probe, state, Present and Reset");
    return 0;
}
