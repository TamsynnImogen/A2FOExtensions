#include "bc_catalog.hpp"
#include "private.hpp"
#include "../../../rust/game_resources_ffi.h"
#include <new>
namespace a2fo::renderer::game {
namespace {
std::array<BYTE, 16> key(REFGUID g) {
  std::array<BYTE, 16> b;
  std::memcpy(b.data(), &g, 16);
  return b;
}
std::uint64_t hash_bytes(const BYTE *p, std::size_t n) {
  return a2fo_resource_hash(p, n);
}
template <class T> void register_resource(T *object, Resource &r) {
  r.api = object;
  r.destroy = [](Resource *p) { delete static_cast<T *>(p->api); };
  r.owner->resources.emplace(r.api, &r);
}
} // namespace
HRESULT PrivateData::set(REFGUID g, const void *p, DWORD n,
                         DWORD flags) noexcept {
  if (a2fo_resource_private_set(p != nullptr, n, flags, sizeof(IUnknown *)) != 3)
    return invalid_call;
  try {
    auto v = std::make_shared<Value>();
    if (a2fo_resource_is_object(flags)) {
      v->object = static_cast<IUnknown *>(const_cast<void *>(p));
      v->object->AddRef();
    } else
      v->bytes.assign(static_cast<const BYTE *>(p),
                      static_cast<const BYTE *>(p) + n);
    values[key(g)] = std::move(v);
    return D3D_OK;
  } catch (...) {
    return E_OUTOFMEMORY;
  }
}
HRESULT PrivateData::get(REFGUID g, void *p, DWORD *n) noexcept {
  if (!n)
    return invalid_call;
  auto it = values.find(key(g));
  if (a2fo_resource_private_get(true, it != values.end(), false, 0, 0) == 1)
    return D3DERR_NOTFOUND;
  auto v = it->second;
  DWORD capacity = *n;
  *n = v->object ? sizeof(IUnknown *) : v->bytes.size();
  if (!p)
    return D3D_OK;
  if (a2fo_resource_private_get(true, true, true, capacity, *n) == 2)
    return D3DERR_MOREDATA;
  if (v->object) {
    v->object->AddRef();
    std::memcpy(p, &v->object, sizeof(IUnknown *));
  } else if (*n)
    std::memcpy(p, v->bytes.data(), *n);
  return D3D_OK;
}
HRESULT PrivateData::erase(REFGUID g) noexcept {
  auto it = values.find(key(g));
  if (a2fo_resource_private_get(true, it != values.end(), false, 0, 0) == 1)
    return D3DERR_NOTFOUND;
  auto value = std::move(it->second);
  values.erase(it);
  return D3D_OK;
}
Resource::Resource(Device *p, Kind k) : owner(p), kind(k) { owner->AddRef(); }
Resource::~Resource() {
  if (owner) {
    owner->resources.erase(api);
    if (refs > 0)
      owner->Release();
  }
}
ULONG Resource::retain() {
  auto n = InterlockedIncrement(&refs);
  if (n == 1) {
    if (a2fo_resource_texture_owner(kind, kind == surface &&
          static_cast<Surface8 *>(api)->texture != nullptr))
      static_cast<Surface8 *>(api)->texture->AddRef();
    else
      owner->AddRef();
  }
  return n;
}
ULONG Resource::release() {
  hold();
  auto n = InterlockedDecrement(&refs);
  if (!n) {
    if (a2fo_resource_texture_owner(kind, kind == surface &&
          static_cast<Surface8 *>(api)->texture != nullptr))
      static_cast<Surface8 *>(api)->texture->Release();
    else if (owner)
      owner->Release();
  }
  drop();
  return n;
}
void Resource::hold() { InterlockedIncrement(&implicit); }
void Resource::drop() {
  if (!InterlockedDecrement(&implicit) &&
      !InterlockedCompareExchange(&refs, 0, 0))
    destroy(this);
}
HRESULT Resource::get_device(IDirect3DDevice8 **out) {
  if (!out || !owner)
    return invalid_call;
  owner->AddRef();
  *out = owner;
  return D3D_OK;
}
bool block_format(D3DFORMAT f) { return a2fo_resource_block_format(f); }
UINT pixel_pitch(D3DFORMAT f, UINT w) { return a2fo_resource_pitch(f, w); }
UINT pixel_rows(D3DFORMAT f, UINT h) { return a2fo_resource_rows(f, h); }
#define RESOURCE_COMMON(C)                                                     \
  ULONG C::AddRef() { return resource.retain(); }                              \
  ULONG C::Release() { return resource.release(); }                            \
  HRESULT C::GetDevice(IDirect3DDevice8 **p) {                                 \
    return resource.get_device(p);                                             \
  }                                                                            \
  HRESULT C::SetPrivateData(REFGUID g, const void *p, DWORD n, DWORD f) {      \
    return resource.private_data.set(g, p, n, f);                              \
  }                                                                            \
  HRESULT C::GetPrivateData(REFGUID g, void *p, DWORD *n) {                    \
    return resource.private_data.get(g, p, n);                                 \
  }                                                                            \
  HRESULT C::FreePrivateData(REFGUID g) {                                      \
    return resource.private_data.erase(g);                                     \
  }
#define RESOURCE_PRIORITY(C, T)                                                \
  DWORD C::SetPriority(DWORD n) {                                              \
    DWORD previous = resource.priority;                                        \
    resource.priority = n;                                                     \
    return previous;                                                           \
  }                                                                            \
  DWORD C::GetPriority() { return resource.priority; }                         \
  void C::PreLoad() {}                                                         \
  D3DRESOURCETYPE C::GetType() { return T; }
RESOURCE_COMMON(Texture8)
RESOURCE_COMMON(Surface8)
RESOURCE_COMMON(VertexBuffer8) RESOURCE_COMMON(IndexBuffer8)
    RESOURCE_PRIORITY(Texture8,
                      D3DRTYPE_TEXTURE) RESOURCE_PRIORITY(VertexBuffer8,
                                                          D3DRTYPE_VERTEXBUFFER)
        RESOURCE_PRIORITY(IndexBuffer8, D3DRTYPE_INDEXBUFFER)
#define QI(C, TEST)                                                            \
  HRESULT C::QueryInterface(REFIID iid, void **out) {                          \
    if (!out)                                                                  \
      return E_POINTER;                                                        \
    *out = nullptr;                                                            \
    if (!(IsEqualGUID(iid, IID_IUnknown) || (TEST)))                           \
      return E_NOINTERFACE;                                                    \
    *out = this;                                                               \
    AddRef();                                                                  \
    return S_OK;                                                               \
  }
            QI(Texture8, IsEqualGUID(iid, IID_IDirect3DResource8) ||
                             IsEqualGUID(iid, IID_IDirect3DBaseTexture8) ||
                             IsEqualGUID(iid, IID_IDirect3DTexture8))
                QI(VertexBuffer8,
                   IsEqualGUID(iid, IID_IDirect3DResource8) ||
                       IsEqualGUID(iid, IID_IDirect3DVertexBuffer8))
                    QI(IndexBuffer8,
                       IsEqualGUID(iid, IID_IDirect3DResource8) ||
                           IsEqualGUID(iid, IID_IDirect3DIndexBuffer8))
                        QI(Surface8, IsEqualGUID(iid, IID_IDirect3DSurface8))
                            VertexBuffer8::VertexBuffer8(Device *p, UINT n,
                                                         DWORD usage, DWORD fvf,
                                                         D3DPOOL pool)
    : resource(p, Resource::vertex), bytes(n) {
  desc = {D3DFMT_VERTEXDATA, D3DRTYPE_VERTEXBUFFER, usage, pool, n, fvf};
  register_resource(this, resource);
}
IndexBuffer8::IndexBuffer8(Device *p, UINT n, DWORD usage, D3DFORMAT format,
                           D3DPOOL pool)
    : resource(p, Resource::index), bytes(n) {
  desc = {format, D3DRTYPE_INDEXBUFFER, usage, pool, n};
  register_resource(this, resource);
}
#define BUFFER_METHODS(C, DESC)                                                \
  HRESULT C::GetDesc(DESC *p) {                                                \
    if (!p)                                                                    \
      return invalid_call;                                                     \
    *p = desc;                                                                 \
    return D3D_OK;                                                             \
  }                                                                            \
  HRESULT C::Lock(UINT offset, UINT size, BYTE **out, DWORD flags) {           \
    if (!a2fo_resource_buffer_lock(out != nullptr, locked, bytes.size(),     \
                                   offset, size, flags))                         \
      return invalid_call;                                                     \
    locked = true;                                                             \
    *out = bytes.data() + offset;                                              \
    return D3D_OK;                                                             \
  }                                                                            \
  HRESULT C::Unlock() {                                                        \
    if (!a2fo_resource_can_unlock(locked))                                     \
      return invalid_call;                                                     \
    locked = false;                                                            \
    return D3D_OK;                                                             \
  }
BUFFER_METHODS(VertexBuffer8, D3DVERTEXBUFFER_DESC)
BUFFER_METHODS(IndexBuffer8, D3DINDEXBUFFER_DESC)
Texture8::Texture8(Device *p, UINT w, UINT h, UINT levels, DWORD u, D3DFORMAT f,
                   D3DPOOL pool_value)
    : resource(p, Resource::texture), width(w), height(h), usage(u), format(f),
      pool(pool_value) {
  if (!levels) {
    levels = a2fo_resource_levels(w, h);
  }
  for (UINT level = 0; level < levels; ++level) {
    MipUpload mip;
    mip.width = w;
    mip.height = h;
    mip.pitch = pixel_pitch(f, w);
    mip.bytes.resize(std::size_t(mip.pitch) * pixel_rows(f, h));
    mips.push_back(std::move(mip));
    w = a2fo_resource_next_dimension(w);
    h = a2fo_resource_next_dimension(h);
  }
  locked.resize(levels);
  readonly.resize(levels);
  surfaces.resize(levels);
  register_resource(this, resource);
}
Texture8::~Texture8() {
  if (resource.owner && bc_size)
    resource.owner->bc_live_bytes -= bc_size;
  for (auto *surface : surfaces)
    if (surface)
      surface->resource.drop();
}
HRESULT Texture8::initialize() {
  if (!a2fo_resource_can_initialize(pixel_pitch(format, width)))
    return invalid_call;
  if (a2fo_resource_is_render_target(usage))
    return resource.owner->backend.create_texture(format, mips, gpu);
  return D3D_OK;
}
HRESULT Texture8::upload(UINT level) {
  Device *owner = resource.owner;
  if (!owner)
    return invalid_call;
  if (a2fo_resource_upload_plan(compressed != nullptr,
                                format == D3DFMT_DXT4, level,
                                gpu != nullptr) == 1) {
    textures::CompressedImage image;
    bool normal = false;
    std::string name, error;
    if (match_bc_texture(width, height, mips.size(), mips[0].bytes.data(),
                         mips[0].bytes.size(), image, normal, name, error)) {
      auto handle = std::make_unique<CompressedTextureHandle>();
      HRESULT hr = handle->initialize(owner->backend.device(), image, error);
      if (FAILED(hr))
        return owner->fail(error.c_str(), hr);
      for (const auto &mip : image.mips)
        expected_hashes.push_back(
            hash_bytes(image.bytes.data() + mip.offset, mip.slice_pitch));
      bc_size = 0;
      for (const auto &mip : image.mips)
        bc_size = a2fo_resource_accumulate_size(bc_size, mip.slice_pitch);
      gpu.reset();
      compressed = std::move(handle);
      normal_xy = normal;
      asset = name;
      ++owner->bc_uploads;
      owner->bc_live_bytes += bc_size;
      if (owner->config.log)
        owner->config.log("[D3D11] BC asset upload " + name +
                          " format=" + std::to_string(image.format) +
                          " mips=" + std::to_string(image.mips.size()) +
                          " encoded_bytes=" + std::to_string(bc_size) +
                          " normal_xy=" + std::to_string(normal));
      return D3D_OK;
    }
    const auto miss = a2fo_resource_bc_miss_plan(!error.empty(),
                                                   owner->config.bc_assets);
    if (miss == 1)
      return owner->fail(error.c_str());
    if (miss == 2)
      return owner->fail("DXT4 test transport does not match a verified BC asset");
  }
  const auto plan = a2fo_resource_upload_plan(compressed != nullptr, false,
                                               level, gpu != nullptr);
  if (plan == 2) {
    if (!a2fo_resource_upload_immutable_level(level, expected_hashes.size()))
      return owner->fail(
          "immutable BC transport was altered during native mip upload");
    const auto digest = hash_bytes(mips[level].bytes.data(), mips[level].bytes.size());
    if (!a2fo_resource_upload_immutable(level, expected_hashes.size(), digest,
                                         expected_hashes.data()))
      return owner->fail(
          "immutable BC transport was altered during native mip upload");
    return D3D_OK;
  }
  if (plan == 3)
    return owner->backend.create_texture(format, mips, gpu);
  return owner->backend.update_texture(*gpu, level, mips[level]);
}
DWORD Texture8::SetLOD(DWORD n) {
  DWORD old = lod;
  lod = a2fo_resource_lod(n, mips.size());
  return old;
}
DWORD Texture8::GetLOD() { return lod; }
DWORD Texture8::GetLevelCount() { return mips.size(); }
HRESULT Texture8::GetLevelDesc(UINT level, D3DSURFACE_DESC *out) {
  if (!a2fo_resource_has_level(out != nullptr, level, mips.size()))
    return invalid_call;
  const auto &mip = mips[level];
  *out = {format,
          D3DRTYPE_SURFACE,
          usage,
          pool,
          static_cast<UINT>(mip.bytes.size()),
          D3DMULTISAMPLE_NONE,
          mip.width,
          mip.height};
  return D3D_OK;
}
HRESULT Texture8::GetSurfaceLevel(UINT level, IDirect3DSurface8 **out) {
  if (!out)
    return invalid_call;
  *out = nullptr;
  if (!a2fo_resource_has_level(true, level, mips.size()))
    return invalid_call;
  try {
    if (!surfaces[level]) {
      D3DSURFACE_DESC desc;
      GetLevelDesc(level, &desc);
      auto *surface = new Surface8(resource.owner, desc, true);
      surface->texture = this;
      surface->level = level;
      if (a2fo_resource_is_render_target(usage)) {
        HRESULT hr = resource.owner->backend.texture_target(
            *gpu, surface->target, level);
        if (FAILED(hr)) {
          surface->texture = nullptr;
          surface->resource.drop();
          return hr;
        }
      }
      surfaces[level] = surface;
    }
    surfaces[level]->AddRef();
    *out = surfaces[level];
    return D3D_OK;
  } catch (...) {
    return E_OUTOFMEMORY;
  }
}
HRESULT Texture8::LockRect(UINT level, D3DLOCKED_RECT *out, const RECT *region,
                           DWORD flags) {
  if (!a2fo_resource_has_level(true, level, mips.size()))
    return invalid_call;
  auto &mip = mips[level];
  RECT r = {0, 0, LONG(mip.width), LONG(mip.height)};
  if (region)
    r = *region;
  std::size_t offset = 0;
  if (!a2fo_resource_rect_lock(out != nullptr, locked[level],
                               a2fo_resource_is_render_target(usage), flags,
                               format, mip.width, mip.height, mip.pitch,
                               r.left, r.top, r.right, r.bottom, &offset))
    return invalid_call;
  locked[level] = true;
  readonly[level] = a2fo_resource_is_readonly(flags);
  out->Pitch = mip.pitch;
  out->pBits = mip.bytes.data() + offset;
  return D3D_OK;
}
HRESULT Texture8::UnlockRect(UINT level) {
  if (!a2fo_resource_has_level(true, level, mips.size()) ||
      !a2fo_resource_can_unlock(locked[level]))
    return invalid_call;
  locked[level] = false;
  return a2fo_resource_should_upload(readonly[level]) ? upload(level) : D3D_OK;
}
HRESULT Texture8::AddDirtyRect(const RECT *) {
  return a2fo_resource_can_dirty(compressed != nullptr) ? D3D_OK : invalid_call;
}
Surface8::Surface8(Device *p, const D3DSURFACE_DESC &descriptor,
                   bool implicit_value)
    : resource(p, Resource::surface), desc(descriptor),
      implicit_surface(implicit_value) {
  register_resource(this, resource);
  if (implicit_value) {
    resource.hold();
    resource.release();
  }
}
Surface8::~Surface8() = default;
HRESULT Surface8::GetContainer(REFIID iid, void **out) {
  if (!out)
    return E_POINTER;
  *out = nullptr;
  return texture ? texture->QueryInterface(iid, out) : E_NOINTERFACE;
}
HRESULT Surface8::GetDesc(D3DSURFACE_DESC *out) {
  if (!out)
    return invalid_call;
  *out = desc;
  return D3D_OK;
}
HRESULT Surface8::LockRect(D3DLOCKED_RECT *out, const RECT *rect, DWORD flags) {
  if (a2fo_resource_target_plan(texture != nullptr, target != nullptr))
    return texture->LockRect(level, out, rect, flags);
  if (!a2fo_resource_surface_lock(out != nullptr, locked, target != nullptr,
                                  rect != nullptr, flags))
    return invalid_call;
  if (target) {
    std::vector<BYTE> rgba;
    UINT w = 0, h = 0;
    HRESULT hr = resource.owner->backend.read_target(*target, rgba, w, h);
    if (FAILED(hr))
      return hr;
    bytes.resize(rgba.size());
    a2fo_resource_swizzle_bgra(bytes.data(), rgba.data(), rgba.size());
  }
  UINT pitch = pixel_pitch(desc.Format, desc.Width);
  if (!a2fo_resource_can_initialize(pitch))
    return invalid_call;
  RECT r = {0, 0, LONG(desc.Width), LONG(desc.Height)};
  if (rect)
    r = *rect;
  std::size_t offset = 0;
  if (block_format(desc.Format) ||
      !a2fo_resource_rect_lock(true, false, false, 0, desc.Format,
                               desc.Width, desc.Height, pitch, r.left, r.top,
                               r.right, r.bottom, &offset))
    return invalid_call;
  if (bytes.empty())
    bytes.resize(std::size_t(pitch) * desc.Height);
  locked = true;
  readonly = a2fo_resource_is_readonly(flags);
  out->Pitch = pitch;
  out->pBits = bytes.data() + offset;
  return D3D_OK;
}
HRESULT Surface8::UnlockRect() {
  if (a2fo_resource_target_plan(texture != nullptr, target != nullptr))
    return texture->UnlockRect(level);
  if (!a2fo_resource_can_unlock(locked))
    return invalid_call;
  locked = false;
  return D3D_OK;
}
} // namespace a2fo::renderer::game
