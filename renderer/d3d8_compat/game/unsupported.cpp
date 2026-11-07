#include "private.hpp"
#include "../../../rust/stage8_b_unsupported_ffi.h"
#include <exception>
namespace a2fo::renderer::game {
namespace {
struct UnsupportedContext { Device* device; std::exception_ptr error; };
std::int32_t fail_unsupported(void* opaque, const char* message) noexcept {
  auto& context = *static_cast<UnsupportedContext*>(opaque);
  try { return context.device->fail(message); }
  catch (...) { context.error = std::current_exception(); return invalid_call; }
}
HRESULT unsupported(Device* device, std::uint32_t operation) {
  UnsupportedContext context{device, {}};
  const auto result = a2fo_rs_stage8_b_unsupported(operation, &context, fail_unsupported);
  if (context.error) std::rethrow_exception(context.error);
  return result;
}
} // namespace
HRESULT Device::SetCursorProperties(UINT, UINT, IDirect3DSurface8 *) {
  return unsupported(this, 0);
}
void Device::SetCursorPosition(UINT, UINT, DWORD) {
  unsupported(this, 1);
}
WINBOOL Device::ShowCursor(WINBOOL) {
  return unsupported(this, 2);
}
HRESULT Device::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *,
                                          IDirect3DSwapChain8 **) {
  return unsupported(this, 3);
}
HRESULT Device::GetRasterStatus(D3DRASTER_STATUS *) {
  return unsupported(this, 4);
}
void Device::SetGammaRamp(DWORD, const D3DGAMMARAMP *) {
  unsupported(this, 5);
}
void Device::GetGammaRamp(D3DGAMMARAMP *) {
  unsupported(this, 6);
}
HRESULT Device::CreateVolumeTexture(UINT, UINT, UINT, UINT, DWORD, D3DFORMAT,
                                    D3DPOOL, IDirect3DVolumeTexture8 **) {
  return unsupported(this, 7);
}
HRESULT Device::CreateCubeTexture(UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
                                  IDirect3DCubeTexture8 **) {
  return unsupported(this, 8);
}
HRESULT Device::GetInfo(DWORD, void *, DWORD) {
  return unsupported(this, 9);
}
HRESULT Device::SetPaletteEntries(UINT, const PALETTEENTRY *) {
  return unsupported(this, 10);
}
HRESULT Device::GetPaletteEntries(UINT, PALETTEENTRY *) {
  return unsupported(this, 11);
}
HRESULT Device::SetCurrentTexturePalette(UINT) {
  return unsupported(this, 12);
}
HRESULT Device::GetCurrentTexturePalette(UINT *) {
  return unsupported(this, 13);
}
HRESULT Device::ProcessVertices(UINT, UINT, UINT, IDirect3DVertexBuffer8 *,
                                DWORD) {
  return unsupported(this, 14);
}
HRESULT Device::CreatePixelShader(const DWORD *, DWORD *) {
  return unsupported(this, 15);
}
HRESULT Device::DeletePixelShader(DWORD) {
  return unsupported(this, 16);
}
HRESULT Device::SetPixelShaderConstant(DWORD, const void *, DWORD) {
  return unsupported(this, 17);
}
HRESULT Device::GetPixelShaderConstant(DWORD, void *, DWORD) {
  return unsupported(this, 18);
}
HRESULT Device::GetPixelShaderFunction(DWORD, void *, DWORD *) {
  return unsupported(this, 19);
}
HRESULT Device::DrawRectPatch(UINT, const float *, const D3DRECTPATCH_INFO *) {
  return unsupported(this, 20);
}
HRESULT Device::DrawTriPatch(UINT, const float *, const D3DTRIPATCH_INFO *) {
  return unsupported(this, 21);
}
HRESULT Device::DeletePatch(UINT) {
  return unsupported(this, 22);
}
} // namespace a2fo::renderer::game
