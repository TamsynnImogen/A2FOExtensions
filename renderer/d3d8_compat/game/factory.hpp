#pragma once
#include "../../d3d11/game/contracts.hpp"
#include <d3d8.h>
namespace a2fo::renderer::game {
struct Configuration {
  std::string mod_root, shader_cache, original_texture_root;
  bool debug = false, vsync = true, bc_assets = true;
  Log log = nullptr;
};
// Retains native factory on success only; never creates a native D3D8 device.
IDirect3D8 *create_factory(IDirect3D8 *, const Configuration &) noexcept;
} // namespace a2fo::renderer::game
namespace a2fo::renderer::game {
// Optional named fopen import bridge, installed before module file redirects.
// Does no DDS/GPU work until a test catalog is explicitly loaded.
bool install_file_bridge(HMODULE, Log) noexcept;
// Named D3DX8 file APIs open files independently of the game's CRT. Keep
// their native decoding/parameter semantics while supplying catalog transport.
bool install_d3dx_file_bridge(HMODULE, Log) noexcept;
bool is_bc_device(IDirect3DDevice8 *) noexcept;
bool is_bc_texture(IDirect3DDevice8 *, IDirect3DTexture8 *) noexcept;
// S_FALSE means legacy/non-catalog input; errors must not become native
// fallback.
HRESULT load_texture_file(IDirect3DDevice8 *, const char *,
                          IDirect3DTexture8 **) noexcept;
std::string legacy_source_path(IDirect3DDevice8 *, const char *) noexcept;
} // namespace a2fo::renderer::game
