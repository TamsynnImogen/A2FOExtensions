/*
 * Early DX8 renderer hook ownership for the optional Nebula module.
 *
 * The hook sites must be claimed during A2FOExtensions process attach because
 * Armada creates its shared DOT3 shader before deferred modules are loaded.
 * Heavy D3DX/file work remains lazy and occurs at the first DOT3 compilation,
 * outside the Windows loader lock.
 */

#pragma once

#include <windows.h>

#include <cstdint>
#include <string>

namespace a2fo {

using NebulaRendererLog = void (*)(const std::string& message);

// Installs checked pass-through hooks only. The feature activates lazily when
// A2FONebulaRenderer.dll and its shaders are present at first DOT3 use.
bool install_nebula_renderer_early(HMODULE armada, HMODULE fleet_ops,
                                   const std::string& root_directory,
                                   NebulaRendererLog log);

// -1 disabled/failed, 0 unavailable, 1 armed/waiting, 2 active.
int nebula_renderer_status() noexcept;

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

// Cooperative render-boundary callbacks supplied to A2FOHybridBuild. They
// maintain a thread-local stack because Fleet Operations may nest craft draws.
void nebula_begin_craft_render(void* craft) noexcept;
void nebula_end_craft_render(void* craft) noexcept;

}  // namespace a2fo
