#pragma once
#include <windows.h>
#include <d3d8.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace a2fo::renderer::game {
constexpr HRESULT invalid_call = static_cast<HRESULT>(0x8876086cu);
struct Device {
  enum class Mode { normal, custom, throwing, reentrant };
  Mode mode = Mode::normal;
  unsigned depth = 0;
  unsigned mutations = 0;
  std::vector<std::string> diagnostics;
  HRESULT fail(const char* text, HRESULT = invalid_call) {
    diagnostics.emplace_back(text);
    ++mutations;
    if (mode == Mode::reentrant && depth++ == 0) {
      DeletePatch(0xabcdefu);
    }
    if (mode == Mode::throwing) throw std::runtime_error("synthetic fail exception");
    return mode == Mode::custom ? static_cast<HRESULT>(0x81234567u) : invalid_call;
  }
  HRESULT SetCursorProperties(UINT, UINT, IDirect3DSurface8*);
  void SetCursorPosition(UINT, UINT, DWORD);
  WINBOOL ShowCursor(WINBOOL);
  HRESULT CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS*, IDirect3DSwapChain8**);
  HRESULT GetRasterStatus(D3DRASTER_STATUS*);
  void SetGammaRamp(DWORD, const D3DGAMMARAMP*);
  void GetGammaRamp(D3DGAMMARAMP*);
  HRESULT CreateVolumeTexture(UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture8**);
  HRESULT CreateCubeTexture(UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture8**);
  HRESULT GetInfo(DWORD, void*, DWORD);
  HRESULT SetPaletteEntries(UINT, const PALETTEENTRY*);
  HRESULT GetPaletteEntries(UINT, PALETTEENTRY*);
  HRESULT SetCurrentTexturePalette(UINT);
  HRESULT GetCurrentTexturePalette(UINT*);
  HRESULT ProcessVertices(UINT, UINT, UINT, IDirect3DVertexBuffer8*, DWORD);
  HRESULT CreatePixelShader(const DWORD*, DWORD*);
  HRESULT DeletePixelShader(DWORD);
  HRESULT SetPixelShaderConstant(DWORD, const void*, DWORD);
  HRESULT GetPixelShaderConstant(DWORD, void*, DWORD);
  HRESULT GetPixelShaderFunction(DWORD, void*, DWORD*);
  HRESULT DrawRectPatch(UINT, const float*, const D3DRECTPATCH_INFO*);
  HRESULT DrawTriPatch(UINT, const float*, const D3DTRIPATCH_INFO*);
  HRESULT DeletePatch(UINT);
};
}
