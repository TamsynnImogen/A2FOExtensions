/*
 * Early DX8 renderer hook ownership for the optional Nebula module.
 *
 * The hook sites must be claimed during A2FOExtensions process attach because
 * Armada creates its shared DOT3 shader before deferred modules are loaded.
 * Heavy D3DX/file work remains lazy and occurs at the first DOT3 compilation,
 * outside the Windows loader lock. Fleet Operations' stock DOT3 vertex shader
 * and its source path remain untouched.
 */

#pragma once

#include <windows.h>

#include <cstdint>
#include <string>

namespace a2fo {

using NebulaRendererLog = void (*)(const std::string& message);

// Installs checked pass-through hooks only. The feature activates lazily when
// A2FONebulaRenderer.dll and its pixel shaders are present at first DOT3 use.
bool install_nebula_renderer_early(HMODULE armada, HMODULE fleet_ops,
                                   const std::string& root_directory,
                                   NebulaRendererLog log);

// Releases the retained live Direct3D device and every GPU object owned by
// the renderer. Process termination lets Windows reclaim these objects; this
// entry point is for an orderly explicit core unload.
void shutdown_nebula_renderer() noexcept;

// -1 disabled/failed, 0 unavailable, 1 armed/waiting, 2 active.
int nebula_renderer_status() noexcept;

// Scales emissive RGB in the combined DOT3 bump/emissive pixel shader.
// The fixed-function and non-bump emissive paths retain their native strength.
bool set_nebula_emissive_bump_multiplier(float multiplier) noexcept;

// Adds ambient light to the DOT3 result before diffuse modulation.
bool set_nebula_bump_light_bias(float bias) noexcept;

// Restores unlit diffuse RGB under the combined shader's emissive pixels.
bool set_nebula_emissive_diffuse_restore(float amount) noexcept;

// Called by the optional controller after a CraftClass has consumed its ODF.
// All six paths are copied immediately in warp/impulse/shields/life-support/
// sensors/weapons order. Null entries disable that subsystem map.
bool register_nebula_emissive_class(
    void* object_class, const char* const* texture_paths,
    std::uint32_t texture_path_count) noexcept;

// Indexed variant. diffuse_names contains one textureX value per material;
// texture_paths is a material-major table with texture_paths_per_material
// entries in warp/impulse/shields/life-support/sensors/weapons order.
bool register_nebula_emissive_materials(
    void* object_class, const char* const* diffuse_names,
    const char* const* texture_paths, std::uint32_t material_count,
    std::uint32_t texture_paths_per_material) noexcept;

// Registers one loose specular-map path per indexed diffuse material.
bool register_nebula_specular_materials(
    void* object_class, const char* const* diffuse_names,
    const char* const* texture_paths, std::uint32_t material_count) noexcept;

// Cooperative render-boundary callbacks supplied to A2FOHybridBuild. They
// maintain a thread-local stack because Fleet Operations may nest craft draws.
void nebula_begin_craft_render(void* craft) noexcept;
void nebula_end_craft_render(void* craft) noexcept;

// ABI-stable descriptor copied by the core. `node` is the CraftClass SOD node
// resolved by the optional ODF controller. system_index uses the native
// sensors/engines/weapons/life-support/shields order; 5 is hull.
struct DecalDescriptor {
    std::uint32_t struct_size;
    std::uint32_t system_index;
    std::uint32_t threshold_index;
    void* node;
    const char* texture_path;
    float offset[3];
    float rotation_degrees[3];
    float size[2];
};

bool register_damage_decal_class(
    void* object_class, float damage_threshold,
    const DecalDescriptor* descriptors, std::uint32_t descriptor_count)
    noexcept;

// A permanent hull-logo placement. When texture_path_count is zero the core
// uses Fleet Operations' already-loaded logoFileNames texture at the craft's
// selected possibleCraftNames index. Otherwise texture_paths is a row-aligned
// table (normally generated from logoFileNames plus an ODF suffix).
struct LogoDecalDescriptor {
    std::uint32_t struct_size;
    void* node;
    const char* const* texture_paths;
    std::uint32_t texture_path_count;
    std::uint32_t use_colour_key;
    std::uint32_t colour_key;
    std::uint32_t flip_u;
    float offset[3];
    float rotation_degrees[3];
    float size[2];
};

bool register_logo_decal_class(
    void* object_class, const LogoDecalDescriptor* descriptors,
    std::uint32_t descriptor_count) noexcept;

}  // namespace a2fo
