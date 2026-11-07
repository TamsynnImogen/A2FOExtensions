// Headless resource fixture: native D3D8 factory only; no D3D device/window.
#include "../renderer/d3d8_compat/game/private.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
using namespace a2fo::renderer::game;
namespace a2fo::renderer::game { extern std::vector<std::string> native_events; extern bool native_fail_bc; extern bool native_match_bc; extern bool native_read_success; bool factory_texture_create_fail=false; }

static void row(const char *tag, HRESULT hr, const Device *owner,
                unsigned a = 0, unsigned b = 0) {
  std::printf("%s|%08x|refs=%ld|resources=%u|%u|%u", tag,
              unsigned(hr), long(owner->refs), unsigned(owner->resources.size()), a, b);
  for (const auto &event:native_events) std::printf("|%s",event.c_str());
  std::printf("\n");native_events.clear();
}
int main() {
  D3DPRESENT_PARAMETERS present{};
  auto *owner = new Device(nullptr, 0, D3DDEVTYPE_HAL, nullptr, 0, present);
  {
    auto *buffer = new VertexBuffer8(owner, 8, 0, 0, D3DPOOL_MANAGED);
    BYTE *bits = nullptr;
    row("buffer-null", buffer->Lock(0, 2, nullptr, 0), owner);
    row("buffer-over", buffer->Lock(7, 2, &bits, 0), owner);
    row("buffer-lock", buffer->Lock(3, 2, &bits, 0), owner, unsigned(bits != nullptr));
    row("buffer-double", buffer->Lock(0, 1, &bits, 0), owner);
    row("buffer-unlock", buffer->Unlock(), owner);
    row("buffer-double-unlock", buffer->Unlock(), owner);
    buffer->Release();
    row("buffer-release", D3D_OK, owner);
  }
  {
    auto *texture = new Texture8(owner, 8, 8, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED);
    row("mip-count", D3D_OK, owner, texture->GetLevelCount(), texture->mips[0].pitch);
    D3DLOCKED_RECT lock{};
    RECT region{1,1,3,3};
    auto hr = texture->LockRect(0, &lock, &region, 0);
    row("texture-lock", hr, owner, unsigned(lock.Pitch));
    row("texture-double", texture->LockRect(0, &lock, nullptr, 0), owner);
    row("texture-upload-fail", texture->UnlockRect(0), owner);
    row("texture-double-unlock", texture->UnlockRect(0), owner);
    texture->gpu = std::make_shared<Texture>();
    row("texture-update-fail", texture->upload(0), owner);
    IDirect3DSurface8 *surface = reinterpret_cast<IDirect3DSurface8 *>(1);
    hr = texture->GetSurfaceLevel(0, &surface);
    row("surface-get", hr, owner, unsigned(surface != nullptr));
    hr = texture->GetSurfaceLevel(0, &surface);
    row("surface-repeat", hr, owner, unsigned(surface != nullptr));
    if (surface) { surface->Release(); surface->Release(); }
    hr = texture->GetSurfaceLevel(99, &surface);
    row("surface-invalid", hr, owner, unsigned(surface != nullptr));
    texture->Release();
    row("texture-release", D3D_OK, owner);
  }
  {
    auto *texture = new Texture8(owner, 4, 4, 1, D3DUSAGE_RENDERTARGET,
                                 D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT);
    row("target-init-fail", texture->initialize(), owner);
    texture->gpu = std::make_shared<Texture>();
    IDirect3DSurface8 *surface = reinterpret_cast<IDirect3DSurface8 *>(1);
    auto hr=texture->GetSurfaceLevel(0, &surface);
    row("target-surface-fail", hr, owner, unsigned(surface != nullptr));
    texture->Release();
    row("target-release", D3D_OK, owner);
  }
  {
    D3DSURFACE_DESC desc{D3DFMT_A8R8G8B8, D3DRTYPE_SURFACE, 0,
                         D3DPOOL_SYSTEMMEM, 64, D3DMULTISAMPLE_NONE, 4, 4};
    auto *surface = new Surface8(owner, desc, false);
    D3DLOCKED_RECT lock{};
    RECT region{1,0,3,2};
    row("surface-lock", surface->LockRect(&lock, &region, 0), owner,
        unsigned(lock.Pitch));
    row("surface-unlock", surface->UnlockRect(), owner);
    surface->target = std::make_shared<Target>();
    row("read-target-fail", surface->LockRect(&lock, nullptr,D3DLOCK_READONLY),owner);
    native_read_success=true;
    auto read_hr=surface->LockRect(&lock,nullptr,D3DLOCK_READONLY);
    const auto *pixels=static_cast<const BYTE *>(lock.pBits);
    row("read-target-swizzle",read_hr,owner,unsigned(pixels[0]),unsigned(pixels[2]));
    row("read-target-unlock",surface->UnlockRect(),owner);
    native_read_success=false;
    surface->Release();
    row("surface-release", D3D_OK, owner);
  }
  {
    owner->config.bc_assets=false;
    auto *texture=new Texture8(owner,4,4,1,0,D3DFMT_DXT4,D3DPOOL_MANAGED);
    row("catalog-miss-fallback",texture->upload(0),owner);
    native_fail_bc=true;
    row("catalog-error",texture->upload(0),owner);
    native_fail_bc=false;
    native_match_bc=true;
    row("catalog-init-fail",texture->upload(0),owner);
    native_match_bc=false;
    texture->compressed=std::make_unique<a2fo::renderer::CompressedTextureHandle>();
    texture->expected_hashes={14695981039346656037ull};
    row("immutable-mismatch",texture->upload(0),owner);
    texture->expected_hashes[0]=14695981039346656037ull;
    for(BYTE b:texture->mips[0].bytes) texture->expected_hashes[0]=(texture->expected_hashes[0]^b)*1099511628211ull;
    row("immutable-valid",texture->upload(0),owner);
    row("immutable-out-of-range",texture->upload(99),owner);
    texture->Release();
    row("catalog-release",D3D_OK,owner);
  }
  const auto refs = owner->Release();
  std::printf("owner-release|%u\n", unsigned(refs));
}
