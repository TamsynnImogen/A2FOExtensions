// Private, nonactivating D3D8/D3D11 comparison. No game or input devices.
#include "renderer_vs11_scene.hpp"
#include "../renderer/shaders/vs11.hpp"
#include "../renderer/shaders/bytecode_cache.hpp"
#include "fixtures/renderer_vs11_capture.inc"
#include <d3d11.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

namespace {
using vs11_fixture::Scene;
using vs11_fixture::Vertex;
template<class T> struct Owned {
    T* p = nullptr;
    ~Owned() { if (p) p->Release(); }
    T** out() { return &p; }
    T* operator->() { return p; }
};
void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void checked(HRESULT hr, const char* message) {
    if (FAILED(hr)) {
        std::fprintf(stderr, "FAIL: %s HRESULT=0x%08lx\n", message,
                     static_cast<unsigned long>(hr));
        std::exit(1);
    }
}
struct Module { HMODULE handle = nullptr; ~Module() { if (handle) FreeLibrary(handle); } };
struct Window {
    HWND handle = nullptr;
    HINSTANCE instance = nullptr;
    const char* class_name = nullptr;
    ~Window() {
        if (handle) DestroyWindow(handle);
        if (class_name) UnregisterClassA(class_name, instance);
    }
};
void put(std::array<float, 384>& c, unsigned reg, float x, float y, float z, float w) {
    c[reg * 4] = x; c[reg * 4 + 1] = y;
    c[reg * 4 + 2] = z; c[reg * 4 + 3] = w;
}
Scene make_scene(const char* name, const std::uint32_t* program,
                 std::size_t count, unsigned variant, bool offset) {
    Scene s; s.name = name; s.program.assign(program, program + count);
    if (offset) {
        s.viewport_x = 8; s.viewport_y = 6;
        s.viewport_width = 48; s.viewport_height = 50;
    }
    auto& c = s.constants;
    put(c, 0, 1, 1, 1, 1);
    // Row vectors: clip.w varies with vertex z to exercise the pixel-centre correction.
    put(c, 2, .72f, 0, 0, 0);
    put(c, 3, 0, .72f, 0, 0);
    put(c, 4, 0, 0, .5f, .25f);
    put(c, 5, 0, 0, .5f, 1);
    put(c, 6, variant ? -.45f : .25f, variant ? .3f : -.5f, .8f, 0);
    put(c, 8, .8f, .1f, .45f, 0);
    put(c, 9, .65f, .2f, .1f, 1);
    put(c, 10, -.2f, .8f, .5f, 0);
    put(c, 11, .1f, .45f, .8f, 1);
    put(c, 12, .1f, -.7f, .6f, 0);
    put(c, 13, .4f, .7f, .2f, 1);
    put(c, 24, 0, 1, 1, .0001f);
    put(c, 25, variant ? .62f : .35f, 0, 0, 0);
    put(c, 26, variant ? .15f : .06f, variant ? .07f : .1f, .12f, 1);
    const Vertex corners[4] = {
        {{-.8f,-.8f,.15f},{0,0,1},{0,1},{1,0,0},{0,1,0},{0,0,1}},
        {{ .8f,-.8f,.35f},{0,0,1},{1,1},{.8660254f,0,-.5f},{0,1,0},{.5f,0,.8660254f}},
        {{ .8f, .8f,.55f},{0,0,1},{1,0},{.8660254f,0,.5f},{0,1,0},{-.5f,0,.8660254f}},
        {{-.8f, .8f,.3f},{0,0,1},{0,0},{.7071068f,0,-.7071068f},{0,1,0},{.7071068f,0,.7071068f}}
    };
    s.vertices = {corners[0], corners[1], corners[2],
                  corners[0], corners[2], corners[3]};
    return s;
}
void dump(const std::string& path, const std::vector<std::uint8_t>& rgba) {
    FILE* file = std::fopen(path.c_str(), "wb");
    require(file != nullptr, "open PPM output");
    std::fprintf(file, "P6\n64 64\n255\n");
    for (std::size_t p = 0; p < rgba.size(); p += 4)
        require(std::fwrite(rgba.data() + p, 1, 3, file) == 3, "write PPM");
    require(std::fclose(file) == 0, "close PPM");
}
std::vector<std::uint32_t> read_program(const char* path) {
    FILE* file = std::fopen(path, "rb");
    require(file != nullptr, "open external VS program");
    require(std::fseek(file, 0, SEEK_END) == 0, "seek external VS program");
    const long bytes = std::ftell(file);
    require(bytes >= 8 && bytes <= 4096 * 4 && bytes % 4 == 0,
            "external VS program has a bounded whole-word length");
    require(std::fseek(file, 0, SEEK_SET) == 0, "rewind external VS program");
    std::vector<std::uint32_t> program(static_cast<std::size_t>(bytes) / 4);
    require(std::fread(program.data(), 4, program.size(), file) == program.size(),
            "read every external VS program word");
    require(std::fgetc(file) == EOF && !std::ferror(file),
            "external VS program ends at the declared length");
    require(std::fclose(file) == 0, "close external VS program");
    return program;
}
int run(int argc, char** argv) {
    require(argc == 2 || argc == 3, "private output directory and optional VS program arguments");
    const std::string cache=std::string(argv[1])+"/vs11-shader-cache";
    require(CreateDirectoryA(cache.c_str(),nullptr)||GetLastError()==ERROR_ALREADY_EXISTS,"private persistent shader cache");
    WNDCLASSEXA cls{}; cls.cbSize = sizeof(cls);
    cls.hInstance = GetModuleHandleA(nullptr); cls.lpfnWndProc = DefWindowProcA;
    cls.lpszClassName = "A2FOVs11PixelFixture";
    require(RegisterClassExA(&cls) != 0, "register private window");
    Window window;
    window.instance = cls.hInstance; window.class_name = cls.lpszClassName;
    window.handle = CreateWindowExA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        cls.lpszClassName, "private shader comparison", WS_POPUP,
        0, 0, 64, 64, nullptr, nullptr, cls.hInstance, nullptr);
    require(window.handle && !IsWindowVisible(window.handle), "hidden nonactivating window");
    Module library{LoadLibraryA("d3d11.dll")};
    require(library.handle != nullptr, "load D3D11 dynamically");
    using Create = HRESULT (WINAPI*)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE,
        UINT, const D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**,
        D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
    Create create = nullptr;
    const auto symbol = GetProcAddress(library.handle, "D3D11CreateDevice");
    std::memcpy(&create, &symbol, sizeof(create));
    require(create != nullptr, "resolve D3D11CreateDevice");
    Owned<ID3D11Device> device; Owned<ID3D11DeviceContext> context;
    const D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL obtained{};
    checked(create(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                   &requested, 1, D3D11_SDK_VERSION, device.out(),
                   &obtained, context.out()), "D3D11 hardware CreateDevice");
    require(obtained >= requested, "SM5 feature level");
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width = texture.Height = 64;
    texture.MipLevels = texture.ArraySize = 1;
    texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture.SampleDesc.Count = 1;
    texture.Usage = D3D11_USAGE_DEFAULT;
    texture.BindFlags = D3D11_BIND_RENDER_TARGET;
    Owned<ID3D11Texture2D> target, resolved, staging;
    Owned<ID3D11RenderTargetView> rtv;
    checked(device->CreateTexture2D(&texture, nullptr, target.out()), "render target");
    checked(device->CreateRenderTargetView(target.p, nullptr, rtv.out()), "RTV");
    checked(device->CreateTexture2D(&texture, nullptr, resolved.out()), "resolve target");
    texture.Usage = D3D11_USAGE_STAGING;
    texture.BindFlags = 0;
    texture.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    checked(device->CreateTexture2D(&texture, nullptr, staging.out()), "readback staging");
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = 96 * 4 * sizeof(float);
    buffer.Usage = D3D11_USAGE_DEFAULT;
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Owned<ID3D11Buffer> constants;
    checked(device->CreateBuffer(&buffer, nullptr, constants.out()), "shader constants b0");
    buffer.ByteWidth = 16;
    Owned<ID3D11Buffer> viewport_constants;
    checked(device->CreateBuffer(&buffer, nullptr, viewport_constants.out()), "viewport constants b1");
    buffer.ByteWidth = 6 * sizeof(Vertex);
    buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    Owned<ID3D11Buffer> vertices;
    checked(device->CreateBuffer(&buffer, nullptr, vertices.out()), "vertex buffer");
    const std::string pixel_source = a2fo::renderer::shaders::vs11_output_interface()+
        "float4 PSMain(VSOutput input) : SV_Target { return input.diffuse; }";
    a2fo::renderer::ShaderCompileResult pixel_code;
    checked(a2fo::renderer::compile_shader({pixel_source, "PSMain", "ps_5_0", 0},
            cache, pixel_code), "compile pixel shader");
    Owned<ID3D11PixelShader> pixel;
    checked(device->CreatePixelShader(pixel_code.bytes.data(), pixel_code.bytes.size(),
                                      nullptr, pixel.out()), "create pixel shader");
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.DepthClipEnable = TRUE;
    raster.MultisampleEnable = TRUE;
    Owned<ID3D11RasterizerState> raster_state;
    checked(device->CreateRasterizerState(&raster, raster_state.out()), "raster state");
    const D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"TEXCOORD",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",2,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",3,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",4,DXGI_FORMAT_R32G32B32_FLOAT,0,44,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",5,DXGI_FORMAT_R32G32B32_FLOAT,0,56,D3D11_INPUT_PER_VERTEX_DATA,0}
    };
    std::vector<Scene> scenes;
    for (unsigned variant = 0; variant != 2; ++variant) {
        scenes.push_back(make_scene(variant ? "program-1-variant" : "program-1",
            program_1, std::size(program_1), variant, variant != 0));
        scenes.push_back(make_scene(variant ? "program-2-variant" : "program-2",
            program_2, std::size(program_2), variant, variant != 0));
        scenes.push_back(make_scene(variant ? "program-4-variant" : "program-4",
            program_4, std::size(program_4), variant, variant != 0));
    }
    if (argc == 3) {
        const auto program = read_program(argv[2]);
        for (unsigned variant = 0; variant != 2; ++variant) {
            auto scene = make_scene(variant ? "external-program-variant" : "external-program",
                program.data(), program.size(), variant, variant != 0);
            // This captured program normalizes a three-component basis and then
            // applies oD0 = (normal + c0) * c1.
            put(scene.constants, 0, 1, 1, 1, 0);
            put(scene.constants, 1, .5f, .5f, .5f, 1);
            scenes.push_back(std::move(scene));
        }
    }
    const float clear[] = {16.f/255.f, 32.f/255.f, 48.f/255.f, 1};
    for (const auto& s : scenes) {
        std::vector<std::uint8_t> expected, actual(64 * 64 * 4);
        std::string error;
        UINT samples=0;
        HRESULT hr = vs11_fixture::render_reference(window.handle, s, expected, samples, error);
        if (FAILED(hr)) {
            std::fprintf(stderr, "FAIL: %s D3D8 %s HRESULT=0x%08lx\n",
                s.name, error.c_str(), static_cast<unsigned long>(hr));
            return 1;
        }
        std::printf("SAMPLES %s native=%u\n",s.name,samples);std::fflush(stdout);
        require(samples&&samples<=16,"bounded native multisample count");
        D3D11_TEXTURE2D_DESC target_desc{};target->GetDesc(&target_desc);
        if(target_desc.SampleDesc.Count!=samples) {
            UINT quality=0;
            checked(device->CheckMultisampleQualityLevels(target_desc.Format,samples,&quality),"native-compatible multisample support");
            require(quality>0,"native-compatible sample count supported");
            context->OMSetRenderTargets(0,nullptr,nullptr);
            rtv.p->Release();rtv.p=nullptr;target.p->Release();target.p=nullptr;
            target_desc.SampleDesc.Count=samples;target_desc.SampleDesc.Quality=0;
            checked(device->CreateTexture2D(&target_desc,nullptr,target.out()),"matching multisample target");
            checked(device->CreateRenderTargetView(target.p,nullptr,rtv.out()),"matching multisample RTV");
        }
        a2fo::renderer::shaders::Vs11Translation translated;
        require(a2fo::renderer::shaders::translate_vs11(s.program.data(),
            s.program.size(), translated, error), "translate captured VS1.1");
        a2fo::renderer::ShaderCompileResult vertex_code;
        hr = a2fo::renderer::compile_shader({translated.hlsl, "VSMain", "vs_5_0", 0},
            cache, vertex_code);
        if (FAILED(hr)) std::fprintf(stderr, "HLSL %s: %s\n", s.name,
                                     vertex_code.diagnostics.c_str());
        checked(hr, "compile translated VS1.1");
        std::printf("SHADER %s cache_hit=%u compile_us=%llu instructions=%zu\n",s.name,
            vertex_code.cache_hit ? 1u : 0u,vertex_code.compile_us,translated.instruction_count);
        if(std::string(s.name).find("variant")!=std::string::npos)
            require(vertex_code.cache_hit&&vertex_code.compile_us==0,"constant-only changes reuse compiled shader permutation");
        Owned<ID3D11VertexShader> vertex;
        Owned<ID3D11InputLayout> layout;
        checked(device->CreateVertexShader(vertex_code.bytes.data(), vertex_code.bytes.size(),
                                           nullptr, vertex.out()), "create vertex shader");
        checked(device->CreateInputLayout(elements, std::size(elements),
            vertex_code.bytes.data(), vertex_code.bytes.size(), layout.out()), "input layout");
        context->UpdateSubresource(constants.p, 0, nullptr, s.constants.data(), 0, 0);
        const float viewport_data[] = {1.f/s.viewport_width, 1.f/s.viewport_height, 0, 0};
        context->UpdateSubresource(viewport_constants.p, 0, nullptr, viewport_data, 0, 0);
        context->UpdateSubresource(vertices.p, 0, nullptr, s.vertices.data(), 0, 0);
        ID3D11Buffer* cb[] = {constants.p, viewport_constants.p};
        context->VSSetConstantBuffers(0, 2, cb);
        context->VSSetShader(vertex.p, nullptr, 0);
        context->PSSetShader(pixel.p, nullptr, 0);
        context->RSSetState(raster_state.p);
        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = static_cast<float>(s.viewport_x);
        viewport.TopLeftY = static_cast<float>(s.viewport_y);
        viewport.Width = static_cast<float>(s.viewport_width);
        viewport.Height = static_cast<float>(s.viewport_height);
        viewport.MaxDepth = 1;
        context->RSSetViewports(1, &viewport);
        context->OMSetRenderTargets(1, &rtv.p, nullptr);
        context->ClearRenderTargetView(rtv.p, clear);
        const UINT stride = sizeof(Vertex), offset = 0;
        context->IASetInputLayout(layout.p);
        context->IASetVertexBuffers(0, 1, &vertices.p, &stride, &offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->Draw(6, 0);
        if(samples>1) {
            context->ResolveSubresource(resolved.p,0,target.p,0,DXGI_FORMAT_R8G8B8A8_UNORM);
            context->CopyResource(staging.p,resolved.p);
        } else context->CopyResource(staging.p, target.p);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        checked(context->Map(staging.p, 0, D3D11_MAP_READ, 0, &mapped), "read D3D11 pixels");
        for (UINT y = 0; y < 64; ++y)
            std::memcpy(actual.data() + y*64*4,
                static_cast<const std::uint8_t*>(mapped.pData) + y*mapped.RowPitch, 64*4);
        context->Unmap(staging.p, 0);
        unsigned mismatches = 0, maximum = 0, coverage = 0;
        for (UINT p = 0; p < 64*64; ++p) {
            if (expected[p*4] != 16 || expected[p*4+1] != 32 || expected[p*4+2] != 48)
                ++coverage;
            for (UINT ch = 0; ch < 3; ++ch) {
                const unsigned delta = static_cast<unsigned>(std::abs(
                    int(expected[p*4+ch]) - int(actual[p*4+ch])));
                maximum = std::max(maximum, delta);
                if (delta > 1) {
                    if (mismatches < 5) std::fprintf(stderr,
                        "MISMATCH %s (%u,%u) channel=%u D3D8=%u D3D11=%u\n",
                        s.name, p%64, p/64, ch, expected[p*4+ch], actual[p*4+ch]);
                    ++mismatches;
                }
            }
        }
        const std::string stem = std::string(argv[1]) + "/" + s.name;
        dump(stem + "-d3d8.ppm", expected);
        dump(stem + "-d3d11.ppm", actual);
        std::printf("PIXELS %s covered=%u differences=%u max_channel_delta=%u\n",
                    s.name, coverage, mismatches, maximum);
        require(coverage > 100, "reference shader covers visible pixels");
        require(mismatches == 0, "D3D8/D3D11 RGB within one colour level");
    }
    context->ClearState();
    context->Flush();
    return 0;
}
}

int main(int argc, char** argv) {
    const int status = run(argc, argv);
    if (!status) std::puts("PASS: captured VS1.1 D3D8/D3D11 pixels");
    return status;
}
