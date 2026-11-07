#pragma once
#include "scene.hpp"
#include "texture.hpp"
#include <windows.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace a2fo::native11 {
using Log = void (*)(const char*);
struct ResourceUsage {
    std::uint64_t epoch=0, mesh_bytes=0, texture_bytes=0; // logical upload sizes, not measured VRAM
    std::uint32_t meshes=0, textures=0; // textures includes the implicit white image
};
class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    HRESULT initialize(HWND,std::uint32_t width,std::uint32_t height,Log=nullptr);
    HRESULT initialize_offscreen(std::uint32_t width,std::uint32_t height,Log=nullptr);
    HRESULT upload_mesh(MeshView,std::uint32_t& handle);
    HRESULT upload_texture(ImageView,std::uint32_t& handle);
    HRESULT upload_texture(TextureView,std::uint32_t& handle);
    HRESULT release_mesh(std::uint32_t handle);
    HRESULT release_texture(std::uint32_t handle); // white image 0 cannot be released
    HRESULT resource_usage(ResourceUsage&) const;
    HRESULT render_status() const; // target/device/thread availability; does not draw or clear
    HRESULT render(FrameView,bool present=true);
    HRESULT readback(std::vector<std::uint8_t>& bgra,std::uint32_t& width,std::uint32_t& height);
    HRESULT resize(std::uint32_t width,std::uint32_t height);
    // Optional native presentation for an offscreen renderer. The host owns
    // the existing HWND; these methods never show/focus/resize that window.
    HRESULT configure_presentation(HWND,std::uint32_t width,std::uint32_t height);
    HRESULT present(std::uint32_t sync_interval=0);
    void shutdown();
private:
#ifdef A2FO_NATIVE11_FIXTURE
    friend struct RendererFixtureAccess;
#endif
    struct Impl;
    std::unique_ptr<Impl> p_;
};
} // namespace a2fo::native11
