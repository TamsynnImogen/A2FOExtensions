#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
namespace a2fo::renderer::game {
using Log = void (*)(const std::string&);
constexpr HRESULT invalid_call=static_cast<HRESULT>(0x8876086c);
struct Matrix {float m[16]{};};
struct Material {float diffuse[4]{1,1,1,1},ambient[4]{1,1,1,1},specular[4]{},emissive[4]{};float power=0;};
struct Light {DWORD type=3;float diffuse[4]{1,1,1,1},specular[4]{},ambient[4]{};float position[3]{},direction[3]{0,0,1};float range=0,falloff=0,attenuation0=0,attenuation1=0,attenuation2=0,theta=0,phi=0;bool enabled=false;};
struct State {
    std::array<DWORD,256> render{};
    std::array<std::array<DWORD,33>,8> stage{};
    Matrix world,view,projection;std::array<Matrix,8> texture;
    Material material;std::array<Light,8> lights;
    UINT x=0,y=0,width=0,height=0;float min_z=0,max_z=1;
    bool transformed=false,has_normal=false,has_diffuse=false,has_specular=false;
    std::array<UINT,8> tex_components{};
    float constants[96][4]{};
    std::string vertex_hlsl; // empty: fixed function; otherwise translate_vs11 output
};
// FF register mapping v0=XYZ/RHW, v1=normal, v2=diffuse RGBA, v3=specular,
// v4..v11=texture coordinate sets. Programmable declarations map actual v0..v15.
struct Vertex {float v[16][4]{};};
struct Draw {UINT primitive=4;std::vector<Vertex> vertices;std::vector<std::uint32_t> indices;};
struct Sampling {ID3D11ShaderResourceView* view=nullptr;UINT minimum_lod=0;bool normal_xy=false;};
class DrawPipeline {
public:
    DrawPipeline();~DrawPipeline();
    HRESULT initialize(ID3D11Device*,ID3D11DeviceContext*,const std::string& cache,Log);
    HRESULT draw(const State&,const Draw&,const std::array<Sampling,8>&);
private:struct Impl;std::unique_ptr<Impl> impl_;
#if defined(A2FO_HEADLESS_NATIVE_TEST)
    friend struct DrawFixture;
#endif
};
class Target {
public:
    Target();~Target();
    Target(const Target&)=delete;Target& operator=(const Target&)=delete;
private:struct Impl;std::unique_ptr<Impl> impl_;friend class Backend;
#if defined(A2FO_HEADLESS_NATIVE_TEST)
    friend struct BackendFixture;
#endif
};
class Texture {
public:
    Texture();~Texture();
    Texture(const Texture&)=delete;Texture& operator=(const Texture&)=delete;
private:struct Impl;std::unique_ptr<Impl> impl_;friend class Backend;
#if defined(A2FO_HEADLESS_NATIVE_TEST)
    friend struct BackendFixture;
#endif
};
struct MipUpload {UINT width=0,height=0,pitch=0;std::vector<std::uint8_t> bytes;};
class Backend {
public:
    Backend();~Backend();
    HRESULT initialize(HWND,UINT width,UINT height,bool windowed,bool vsync,bool debug,const std::string& cache,Log,UINT depth_format=75);
    HRESULT resize(UINT,UINT,bool windowed,UINT depth_format=75);
    HRESULT present(const RECT*,const RECT*,HWND,const RGNDATA*);
    HRESULT cooperative()const;
    UINT available_memory()const;
    void log_memory()const;
    ID3D11Device* device()const;
    std::shared_ptr<Target> backbuffer()const;
    std::shared_ptr<Target> depthbuffer()const;
    HRESULT create_target(UINT,UINT,UINT legacy_format,bool depth,std::shared_ptr<Target>&);
    HRESULT bind_target(const std::shared_ptr<Target>&,const std::shared_ptr<Target>&);
    HRESULT copy_target(Target&,const RECT*,UINT,Target&,const POINT*);
    // Submit a copy of a depth target into an independent sampled texture.
    // S_OK means the GPU copy was queued, not completed. The caller owns the
    // snapshot; texture_view() returns an AddRef'd SRV to its caller.
    HRESULT snapshot_depth(const std::shared_ptr<Target>&,std::shared_ptr<Texture>&);
    HRESULT clear(const State&,UINT count,const RECT*,DWORD flags,DWORD argb,float z,DWORD stencil);
    HRESULT create_texture(UINT legacy_format,const std::vector<MipUpload>&,std::shared_ptr<Texture>&);
    HRESULT update_texture(Texture&,UINT level,const MipUpload&);
    HRESULT texture_view(Texture&,ID3D11ShaderResourceView**);
    HRESULT texture_target(Texture&,std::shared_ptr<Target>&,UINT level=0);
    HRESULT draw(const State&,const Draw&,const std::array<Sampling,8>&);
    HRESULT read_target(Target&,std::vector<std::uint8_t>& rgba,UINT& width,UINT& height);
private:struct Impl;std::unique_ptr<Impl> impl_;
#if defined(A2FO_HEADLESS_NATIVE_TEST)
    friend struct BackendFixture;
#endif
};
}
