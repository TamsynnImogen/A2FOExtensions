#include "rust_stage8_b_unsupported_private.hpp"
#include <cstdio>
#include <cstdint>

using a2fo::renderer::game::Device;
template<class T> T* poison() { return reinterpret_cast<T*>(std::uintptr_t{1}); }
int main() {
  Device d;
  const auto show = [&](int index, HRESULT result) {
    std::printf("%02d|%08lx|%s\n", index, static_cast<unsigned long>(result),
                d.diagnostics.back().c_str());
  };
  show(0, d.SetCursorProperties(3, 5, poison<IDirect3DSurface8>()));
  d.SetCursorPosition(7, 11, 13); show(1, 0);
  show(2, d.ShowCursor(TRUE));
  show(3, d.CreateAdditionalSwapChain(poison<D3DPRESENT_PARAMETERS>(), poison<IDirect3DSwapChain8*>()));
  show(4, d.GetRasterStatus(poison<D3DRASTER_STATUS>()));
  d.SetGammaRamp(17, poison<const D3DGAMMARAMP>()); show(5, 0);
  d.GetGammaRamp(poison<D3DGAMMARAMP>()); show(6, 0);
  show(7, d.CreateVolumeTexture(1, 2, 3, 4, 5, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, poison<IDirect3DVolumeTexture8*>()));
  show(8, d.CreateCubeTexture(1, 2, 3, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, poison<IDirect3DCubeTexture8*>()));
  show(9, d.GetInfo(19, poison<void>(), 23));
  show(10, d.SetPaletteEntries(29, poison<const PALETTEENTRY>()));
  show(11, d.GetPaletteEntries(31, poison<PALETTEENTRY>()));
  show(12, d.SetCurrentTexturePalette(37));
  show(13, d.GetCurrentTexturePalette(poison<UINT>()));
  show(14, d.ProcessVertices(1, 2, 3, poison<IDirect3DVertexBuffer8>(), 4));
  show(15, d.CreatePixelShader(poison<const DWORD>(), poison<DWORD>()));
  show(16, d.DeletePixelShader(41));
  show(17, d.SetPixelShaderConstant(43, poison<const void>(), 2));
  show(18, d.GetPixelShaderConstant(47, poison<void>(), 2));
  show(19, d.GetPixelShaderFunction(53, poison<void>(), poison<DWORD>()));
  show(20, d.DrawRectPatch(59, poison<const float>(), poison<const D3DRECTPATCH_INFO>()));
  show(21, d.DrawTriPatch(61, poison<const float>(), poison<const D3DTRIPATCH_INFO>()));
  show(22, d.DeletePatch(67));
  return d.diagnostics.size() == 23 ? 0 : 1;
}
