#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <string>

namespace a2fo::renderer {

// A single-window diagnostic device. Calls must stay on the owning window thread.
class ProbeDevice {
public:
    using Log = void (*)(const std::string&);

    ProbeDevice() = default;
    ProbeDevice(const ProbeDevice&) = delete;
    ProbeDevice& operator=(const ProbeDevice&) = delete;
    ~ProbeDevice();

    HRESULT initialize(HWND window, UINT width, UINT height, bool debug_layer, Log log);
    HRESULT present_test_colour();
    // Exercises the actual transformed-FVF draw backend on this swap chain.
    // Diagnostic geometry only; this does not translate a whole game frame.
    HRESULT present_test_triangle(const std::string& shader_cache);
    void shutdown() noexcept;
    bool ready() const noexcept;

private:
    void report(const char* operation, HRESULT result) const;

    Log log_ = nullptr;
    HMODULE d3d11_ = nullptr;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    IDXGISwapChain* swap_chain_ = nullptr;
    ID3D11RenderTargetView* render_target_ = nullptr;
    ID3D11Texture2D* depth_texture_ = nullptr;
    ID3D11DepthStencilView* depth_view_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;
};

} // namespace a2fo::renderer
