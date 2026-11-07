#pragma once
#include <cstdint>

// API-neutral scene input. No legacy device, shader token, FVF or render-state
// identifier crosses this boundary. Matrices are row-major, column-vector.
// Single-sided meshes use clockwise screen-space fronts before mirrored instance
// transforms; normals point outward. Asset adapters normalize source conventions.
namespace a2fo::native11 {
struct Vertex { float position[3], normal[3], uv[2]; };
struct MeshView {
    const Vertex* vertices;
    std::uint32_t vertex_count;
    const std::uint32_t* indices;
    std::uint32_t index_count;
};
struct ImageView {
    const std::uint8_t* bgra;
    std::uint32_t width, height, pitch;
    std::uint64_t byte_count;
};
enum class LightType : std::uint32_t { directional=1, point=2, spot=3 };
struct Light {
    LightType type=LightType::directional;
    float position[3]{};
    float direction[3]{0,-1,0}; // ray travel, world space
    float colour[3]{1,1,1};
    float intensity=1;
    float falloff_start=0, falloff_range=100; // full-strength radius, then fade distance
    float cone_inner_degrees=30, cone_outer_degrees=60; // full angles
};
enum DrawFlags : std::uint32_t { two_sided=1, cutout=2, alpha_blend=4, additive=8 };
enum class Sampling : std::uint32_t {linear_clamp=0,linear_wrap=1,point_clamp=2,point_wrap=3};
enum class DepthMode : std::uint32_t {automatic=0,read_only=1,disabled=2};
enum class DrawOrdering : std::uint32_t {capture=0,opaque_then_alpha=1};
struct Draw {
    std::uint32_t mesh=0, texture=0; // renderer-owned handles; texture 0 is white
    float local_to_world[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    float colour[4]{1,1,1,1};
    float emissive[3]{}; // unlit RGB, modulated by the diffuse texture
    std::uint32_t flags=two_sided;
    float uv_transform[6]{1,0,0,0,1,0}; // affine row-major 2x3
    Sampling sampler=Sampling::linear_clamp;
    DepthMode depth_mode=DepthMode::automatic;
    float alpha_cutoff=.5f;
    bool casts_shadow=true,receives_shadow=true;
    bool fog_enabled=true;
    std::uint32_t layer=0; // explicit ascending layer under opt-in ordering
    bool shadow_only=false; // participates in light depth/bounds, never camera colour/depth
};
enum class ShadowFilter : std::uint32_t {bilinear=0,hard=1,pcf3=2};
struct ShadowSettings {
    std::uint32_t light_mask=0; // bit per light index; opt-in, no implicit sun
    std::uint32_t resolution=512; // power of two, 128..2048; atlas <=64 MiB
    float depth_bias=0.001f; // normalized light-depth comparison offset
    ShadowFilter filter=ShadowFilter::bilinear;
};
struct FogSettings {
    bool enabled=false;
    float colour[3]{};
    float start=0,end=1;
    float camera_position[3]{};
};
struct FrameView {
    float world_to_clip[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    float ambient[3]{0.04f,0.04f,0.04f};
    float clear_colour[4]{0.005f,0.008f,0.02f,1};
    const Light* lights=nullptr;
    std::uint32_t light_count=0;
    const Draw* draws=nullptr;
    std::uint32_t draw_count=0;
    float viewport[4]{}; // x,y,width,height; all zero selects the full target
    ShadowSettings shadows{};
    DrawOrdering ordering=DrawOrdering::capture;
    FogSettings fog{};
    bool clear_colour_target=true,clear_depth_target=true;
};
} // namespace a2fo::native11
