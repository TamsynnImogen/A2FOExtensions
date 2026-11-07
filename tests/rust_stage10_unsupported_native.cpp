#include "rust_stage10_unsupported_private.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

using a2fo::renderer::game::Device;
using a2fo::renderer::game::invalid_call;
namespace {
constexpr std::array<const char*, 23> names{{
    "SetCursorProperties", "SetCursorPosition", "ShowCursor",
    "CreateAdditionalSwapChain", "GetRasterStatus", "SetGammaRamp",
    "GetGammaRamp", "CreateVolumeTexture", "CreateCubeTexture", "GetInfo",
    "SetPaletteEntries", "GetPaletteEntries", "SetCurrentTexturePalette",
    "GetCurrentTexturePalette", "ProcessVertices", "CreatePixelShader",
    "DeletePixelShader", "SetPixelShaderConstant", "GetPixelShaderConstant",
    "GetPixelShaderFunction", "DrawRectPatch", "DrawTriPatch", "DeletePatch"}};
constexpr HRESULT void_result = static_cast<HRESULT>(0x10203040u);
constexpr HRESULT custom_result = static_cast<HRESULT>(0x81234567u);

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

HRESULT invoke(Device& d, unsigned operation, void* p) {
  switch (operation) {
    case 0: return d.SetCursorProperties(3, 5, static_cast<IDirect3DSurface8*>(p));
    case 1: d.SetCursorPosition(7, 11, 13); return void_result;
    case 2: return d.ShowCursor(TRUE);
    case 3: return d.CreateAdditionalSwapChain(static_cast<D3DPRESENT_PARAMETERS*>(p), static_cast<IDirect3DSwapChain8**>(p));
    case 4: return d.GetRasterStatus(static_cast<D3DRASTER_STATUS*>(p));
    case 5: d.SetGammaRamp(17, static_cast<const D3DGAMMARAMP*>(p)); return void_result;
    case 6: d.GetGammaRamp(static_cast<D3DGAMMARAMP*>(p)); return void_result;
    case 7: return d.CreateVolumeTexture(1, 2, 3, 4, 5, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, static_cast<IDirect3DVolumeTexture8**>(p));
    case 8: return d.CreateCubeTexture(1, 2, 3, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, static_cast<IDirect3DCubeTexture8**>(p));
    case 9: return d.GetInfo(19, p, 23);
    case 10: return d.SetPaletteEntries(29, static_cast<const PALETTEENTRY*>(p));
    case 11: return d.GetPaletteEntries(31, static_cast<PALETTEENTRY*>(p));
    case 12: return d.SetCurrentTexturePalette(37);
    case 13: return d.GetCurrentTexturePalette(static_cast<UINT*>(p));
    case 14: return d.ProcessVertices(1, 2, 3, static_cast<IDirect3DVertexBuffer8*>(p), 4);
    case 15: return d.CreatePixelShader(static_cast<const DWORD*>(p), static_cast<DWORD*>(p));
    case 16: return d.DeletePixelShader(41);
    case 17: return d.SetPixelShaderConstant(43, p, 2);
    case 18: return d.GetPixelShaderConstant(47, p, 2);
    case 19: return d.GetPixelShaderFunction(53, p, static_cast<DWORD*>(p));
    case 20: return d.DrawRectPatch(59, static_cast<const float*>(p), static_cast<const D3DRECTPATCH_INFO*>(p));
    case 21: return d.DrawTriPatch(61, static_cast<const float*>(p), static_cast<const D3DTRIPATCH_INFO*>(p));
    case 22: return d.DeletePatch(67);
  }
  throw std::runtime_error("invalid operation");
}

void case_one(unsigned operation, Device::Mode mode) {
  Device d;
  d.mode = mode;
  alignas(std::max_align_t) std::array<unsigned char, 16384> bytes{};
  bytes.fill(0xa5);
  HRESULT result = 0;
  bool thrown = false;
  try {
    result = invoke(d, operation, bytes.data());
  } catch (const std::runtime_error& e) {
    require(std::strcmp(e.what(), "synthetic fail exception") == 0, "wrong exception");
    thrown = true;
  }
  require(thrown == (mode == Device::Mode::throwing), "exception parity");
  require(d.diagnostics.size() == (mode == Device::Mode::reentrant ? 2u : 1u), "callback count");
  require(d.diagnostics.front() == std::string(names.at(operation)) + " unsupported in experimental backend", "diagnostic text");
  if (mode == Device::Mode::reentrant)
    require(d.diagnostics.back() == "DeletePatch unsupported in experimental backend", "nested diagnostic");
  require(d.mutations == d.diagnostics.size(), "callback mutation count");
  for (const auto byte : bytes) require(byte == 0xa5, "outparameter changed");
  if (!thrown) {
    const HRESULT expected = operation == 1 || operation == 5 || operation == 6
        ? void_result : operation == 2 ? 0
        : mode == Device::Mode::custom ? custom_result : invalid_call;
    require(result == expected, "return value");
  }
  std::printf("%02u|%u|%08lx|%u|%u|%s\n", operation,
              static_cast<unsigned>(mode), static_cast<unsigned long>(result),
              static_cast<unsigned>(thrown), d.mutations,
              d.diagnostics.front().c_str());
}
} // namespace

int main() {
  std::puts("ready:unsupported");
  std::fflush(stdout);
  for (unsigned operation = 0; operation < names.size(); ++operation)
    for (const auto mode : {Device::Mode::normal, Device::Mode::custom,
                            Device::Mode::throwing, Device::Mode::reentrant})
      case_one(operation, mode);
  std::puts("completed:unsupported");
  return 0;
}
