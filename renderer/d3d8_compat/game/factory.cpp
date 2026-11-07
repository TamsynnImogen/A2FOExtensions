#include "bc_catalog.hpp"
#include "private.hpp"
#include "../../../rust/game_device_ffi.h"
#include <cstdio>
#include <new>
namespace a2fo::renderer::game {
namespace {
using Fopen = FILE *(__cdecl *)(const char *, const char *);
struct FileHook { Fopen original = nullptr; void** slot = nullptr; };
FileHook file_hooks[8];
std::set<IDirect3DDevice8 *> devices;
template<unsigned Index>
FILE *__cdecl texture_fopen(const char *path, const char *mode) {
  const auto original = file_hooks[Index].original;
  try {
    if (original && mode && a2fo_game_read_mode(
            reinterpret_cast<const BYTE *>(mode), std::strlen(mode))) {
      std::string transport = bc_transport_path(path);
      if (!transport.empty()) return original(transport.c_str(), mode);
    }
  } catch (...) { return nullptr; }
  return original ? original(path, mode) : nullptr;
}
void* callback(unsigned index) {
  const Fopen functions[] = {&texture_fopen<0>, &texture_fopen<1>,
    &texture_fopen<2>, &texture_fopen<3>, &texture_fopen<4>,
    &texture_fopen<5>, &texture_fopen<6>, &texture_fopen<7>};
  void* result = nullptr;
  static_assert(sizeof(result) == sizeof(Fopen));
  std::memcpy(&result, &functions[index], sizeof(result));
  return result;
}
bool span(void *p, std::size_t size) {
  MEMORY_BASIC_INFORMATION m{};
  if (!VirtualQuery(p, &m, sizeof(m)) || m.State != MEM_COMMIT ||
      (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
    return false;
  auto at = reinterpret_cast<std::uintptr_t>(p),
       begin = reinterpret_cast<std::uintptr_t>(m.BaseAddress);
  return at >= begin && size <= m.RegionSize - (at - begin);
}
bool supported_texture(D3DFORMAT format) {
  return a2fo_game_supported_texture(format, pixel_pitch(format, 4));
}
} // namespace
bool install_file_bridge(HMODULE module, Log log) noexcept {
  try {
    if (!module)
      return false;
    static_assert(sizeof(void *) == 4, "PE32 import bridge requires 32-bit pointers");
    const A2foPeLayout layout{
        sizeof(IMAGE_DOS_HEADER), sizeof(IMAGE_NT_HEADERS32),
        sizeof(IMAGE_IMPORT_DESCRIPTOR),
        offsetof(IMAGE_DOS_HEADER, e_magic), offsetof(IMAGE_DOS_HEADER, e_lfanew),
        offsetof(IMAGE_NT_HEADERS32, Signature),
        offsetof(IMAGE_NT_HEADERS32, FileHeader.Machine),
        offsetof(IMAGE_NT_HEADERS32, OptionalHeader.Magic),
        offsetof(IMAGE_NT_HEADERS32, OptionalHeader.SizeOfImage),
        offsetof(IMAGE_NT_HEADERS32,
                 OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress),
        offsetof(IMAGE_NT_HEADERS32,
                 OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size),
        offsetof(IMAGE_IMPORT_DESCRIPTOR, Name),
        offsetof(IMAGE_IMPORT_DESCRIPTOR, OriginalFirstThunk),
        offsetof(IMAGE_IMPORT_DESCRIPTOR, FirstThunk),
        IMAGE_DOS_SIGNATURE, IMAGE_NT_SIGNATURE, IMAGE_FILE_MACHINE_I386,
        IMAGE_NT_OPTIONAL_HDR32_MAGIC, IMAGE_ORDINAL_FLAG32};
    const auto checked_span = [](void *, std::uintptr_t address,
                                 std::size_t count) -> bool {
      return span(reinterpret_cast<void *>(address), count);
    };
    const auto checked_read = [](void *, std::uintptr_t address, BYTE *out,
                                 std::size_t count) {
      std::memcpy(out, reinterpret_cast<const void *>(address), count);
    };
    std::uintptr_t slots[8]{}, callbacks[8]{};
    for (unsigned i = 0; i < 8; ++i) {
      slots[i] = reinterpret_cast<std::uintptr_t>(file_hooks[i].slot);
      callbacks[i] = reinterpret_cast<std::uintptr_t>(callback(i));
    }
    A2foPePatchPlan plan{};
    if (!a2fo_game_file_bridge_walk(reinterpret_cast<std::uintptr_t>(module),
                                    &layout, checked_span, checked_read,
                                    nullptr, slots, callbacks, &plan))
      return false;
    if (plan.duplicate)
      return true;
    auto **slot = reinterpret_cast<void **>(plan.slot);
    void *previous = reinterpret_cast<void *>(plan.previous);
    DWORD protect = 0;
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &protect))
      return false;
    std::memcpy(&file_hooks[plan.index].original, &previous, 4);
    file_hooks[plan.index].slot = slot;
    InterlockedExchangePointer(reinterpret_cast<void *volatile *>(slot),
                               callback(plan.index));
    DWORD ignored = 0;
    VirtualProtect(slot, 4, protect, &ignored);
    if (log)
      log("[D3D8Compat] Test-only named fopen bridge installed; "
          "canonical DDS untouched");
    return true;
  } catch (...) {
  }
  return false;
}
Factory::Factory(IDirect3D8 *p, const Configuration &c) : native(p), config(c) {
  native->AddRef();
}
Factory::~Factory() { native->Release(); }
void Factory::caps(D3DCAPS8 &c) {
  A2foGameCaps plan{
      c.MaxTextureWidth, c.MaxTextureHeight, c.MaxTextureBlendStages,
      c.MaxSimultaneousTextures, c.MaxActiveLights, c.MaxStreams,
      c.MaxStreamStride, c.VertexShaderVersion, c.MaxVertexShaderConst,
      c.PixelShaderVersion, c.MaxVertexBlendMatrices,
      c.MaxVertexBlendMatrixIndex, c.TextureCaps, c.DevCaps};
  a2fo_game_caps(&plan);
  c.MaxTextureWidth = plan.max_texture_width;
  c.MaxTextureHeight = plan.max_texture_height;
  c.MaxTextureBlendStages = plan.max_blend_stages;
  c.MaxSimultaneousTextures = plan.max_textures;
  c.MaxActiveLights = plan.max_lights;
  c.MaxStreams = plan.max_streams;
  c.MaxStreamStride = plan.max_stride;
  c.VertexShaderVersion = plan.vertex_shader_version;
  c.MaxVertexShaderConst = plan.max_vertex_consts;
  c.PixelShaderVersion = plan.pixel_shader_version;
  c.MaxVertexBlendMatrices = plan.max_blend_matrices;
  c.MaxVertexBlendMatrixIndex = plan.max_blend_matrix_index;
  c.TextureCaps = plan.texture_caps;
  c.DevCaps = plan.device_caps;
}
HRESULT Factory::QueryInterface(REFIID iid, void **out) {
  if (!out)
    return E_POINTER;
  *out = nullptr;
  if (!IsEqualGUID(iid, IID_IUnknown) && !IsEqualGUID(iid, IID_IDirect3D8))
    return E_NOINTERFACE;
  *out = this;
  AddRef();
  return S_OK;
}
ULONG Factory::AddRef() { return InterlockedIncrement(&refs); }
ULONG Factory::Release() {
  LONG n = InterlockedDecrement(&refs);
  if (!n)
    delete this;
  return n;
}
HRESULT Factory::RegisterSoftwareDevice(void *) { return invalid_call; }
UINT Factory::GetAdapterCount() { return native->GetAdapterCount(); }
HRESULT Factory::GetAdapterIdentifier(UINT adapter, DWORD flags,
                                      D3DADAPTER_IDENTIFIER8 *out) {
  return native->GetAdapterIdentifier(adapter, flags, out);
}
UINT Factory::GetAdapterModeCount(UINT adapter) {
  return native->GetAdapterModeCount(adapter);
}
HRESULT Factory::EnumAdapterModes(UINT adapter, UINT mode,
                                  D3DDISPLAYMODE *out) {
  return native->EnumAdapterModes(adapter, mode, out);
}
HRESULT Factory::GetAdapterDisplayMode(UINT adapter, D3DDISPLAYMODE *out) {
  return native->GetAdapterDisplayMode(adapter, out);
}
HRESULT Factory::CheckDeviceType(UINT adapter, D3DDEVTYPE type,
                                 D3DFORMAT display, D3DFORMAT back,
                                 WINBOOL windowed) {
  if (!a2fo_game_backbuffer(back))
    return D3DERR_NOTAVAILABLE;
  return native->CheckDeviceType(adapter, type, display, back, windowed);
}
HRESULT Factory::CheckDeviceFormat(UINT adapter, D3DDEVTYPE type,
                                   D3DFORMAT display, DWORD usage,
                                   D3DRESOURCETYPE resource, D3DFORMAT format) {
  const auto decision = a2fo_game_factory_format(
      usage, resource, format, supported_texture(format));
  if (decision == 1) return D3D_OK;
  if (decision == 2) return D3DERR_NOTAVAILABLE;
  return native->CheckDeviceFormat(adapter, type, display, usage, resource,
                                   format);
}
HRESULT Factory::CheckDeviceMultiSampleType(UINT, D3DDEVTYPE, D3DFORMAT,
                                            WINBOOL, D3DMULTISAMPLE_TYPE type) {
  return a2fo_game_surface_admission(type, false) ? D3D_OK
                                                   : D3DERR_NOTAVAILABLE;
}
HRESULT Factory::CheckDepthStencilMatch(UINT, D3DDEVTYPE, D3DFORMAT,
                                        D3DFORMAT target, D3DFORMAT depth) {
  return a2fo_game_depth_match(target, depth) ? D3D_OK : D3DERR_NOTAVAILABLE;
}
HRESULT Factory::GetDeviceCaps(UINT adapter, D3DDEVTYPE type, D3DCAPS8 *out) {
  HRESULT hr = native->GetDeviceCaps(adapter, type, out);
  if (SUCCEEDED(hr))
    caps(*out);
  return hr;
}
HMONITOR Factory::GetAdapterMonitor(UINT adapter) {
  return native->GetAdapterMonitor(adapter);
}
HRESULT Factory::CreateDevice(UINT adapter, D3DDEVTYPE type, HWND window,
                              DWORD flags, D3DPRESENT_PARAMETERS *p,
                              IDirect3DDevice8 **out) {
  if (!out) return invalid_call;
  *out = nullptr;
  try {
    struct Context {
      Factory *factory;
      UINT adapter;
      D3DDEVTYPE type;
      HWND window;
      DWORD flags;
      D3DPRESENT_PARAMETERS *input;
      IDirect3DDevice8 **out;
      D3DPRESENT_PARAMETERS parameters{};
      A2foGamePresentationPlan *plan = nullptr;
      std::unique_ptr<Device> device;
    } context{this, adapter, type, window, flags, p, out, {}, nullptr, {}};
    if (p) context.parameters = *p;
    A2foGamePresentationPlan plan{
        reinterpret_cast<std::uintptr_t>(window),
        reinterpret_cast<std::uintptr_t>(context.parameters.hDeviceWindow),
        context.parameters.BackBufferWidth, context.parameters.BackBufferHeight,
        UINT(context.parameters.BackBufferFormat), context.parameters.BackBufferCount,
        UINT(context.parameters.MultiSampleType)};
    context.plan = &plan;
    const auto client = [](void *opaque, UINT *w, UINT *h) -> bool {
      auto &c = *static_cast<Context *>(opaque);
      RECT rect{};
      if (!GetClientRect(reinterpret_cast<HWND>(
                             c.parameters.hDeviceWindow ? c.parameters.hDeviceWindow : c.window),
                         &rect)) return false;
      *w = rect.right; *h = rect.bottom;
      return true;
    };
    const auto initialize = [](void *opaque) -> std::int32_t {
      auto &c = *static_cast<Context *>(opaque);
      try {
        c.parameters.hDeviceWindow = reinterpret_cast<HWND>(c.plan->device_window);
        c.parameters.BackBufferWidth = c.plan->width;
        c.parameters.BackBufferHeight = c.plan->height;
        c.parameters.BackBufferFormat = D3DFORMAT(c.plan->format);
        c.device = std::make_unique<Device>(c.factory, c.adapter, c.type,
                                             c.window, c.flags, c.parameters);
        return c.device->initialize();
      } catch (...) { return E_OUTOFMEMORY; }
    };
    const auto publish = [](void *opaque) -> std::int32_t {
      auto &c = *static_cast<Context *>(opaque);
      try {
        devices.insert(c.device.get());
        *c.input = c.parameters;
        *c.out = c.device.release();
        return D3D_OK;
      } catch (...) { return E_OUTOFMEMORY; }
    };
    const auto result = a2fo_game_create_device_workflow(
        &context, &plan, p != nullptr, adapter, type, client,
        initialize, publish, invalid_call, D3DERR_NOTAVAILABLE);
    return result;
  } catch (...) { return E_OUTOFMEMORY; }
}
IDirect3D8 *create_factory(IDirect3D8 *native,
                           const Configuration &config) noexcept {
  struct Context {
    IDirect3D8 *native;
    const Configuration *config;
    std::string error;
    IDirect3D8 *result = nullptr;
  } context{native, &config, {}, nullptr};
  const auto catalog = [](void *opaque) -> UINT {
    auto &c = *static_cast<Context *>(opaque);
    try { return load_bc_catalog(c.config->mod_root, c.config->log, c.error) ? 0u : 1u; }
    catch (...) { return 2u; }
  };
  const auto log_failure = [](void *opaque) {
    auto &c = *static_cast<Context *>(opaque);
    try { if (c.config->log) c.config->log("[D3D8Compat] Cannot enable DX11 test: " + c.error); }
    catch (...) {}
  };
  const auto create = [](void *opaque) -> UINT {
    auto &c = *static_cast<Context *>(opaque);
    try { c.result = new Factory(c.native, *c.config); return 0u; }
    catch (...) { return 1u; }
  };
  if (!a2fo_game_create_factory_workflow(&context, native != nullptr,
                                         config.bc_assets, catalog,
                                         log_failure, create)) return nullptr;
  return context.result;
}
HRESULT load_texture_file(IDirect3DDevice8 *device, const char *path,
                          IDirect3DTexture8 **out) noexcept {
  if (!out) return invalid_call;
  *out = nullptr;
  if (!devices.count(device)) return S_FALSE;
  try {
  struct Context {
    Device *device;
    const char *path;
    IDirect3DTexture8 **out;
    textures::CompressedImage image;
    bool normal = false;
    std::string asset, error;
    IDirect3DTexture8 *api = nullptr;
  } context{static_cast<Device *>(device), path, out, {}, false, {}, {}, nullptr};
  const auto load = [](void *opaque) -> UINT {
    auto &c = *static_cast<Context *>(opaque);
    try {
      if (load_bc_asset(c.path, c.image, c.normal, c.asset, c.error)) return 2u;
      return c.error.empty() ? 0u : 1u;
    } catch (...) { return 3u; }
  };
  const auto catalog_failure = [](void *opaque) -> std::int32_t {
    auto &c = *static_cast<Context *>(opaque);
    try { return c.device->fail(c.error.c_str()); }
    catch (...) { return E_OUTOFMEMORY; }
  };
  const auto create = [](void *opaque) -> std::int32_t {
    auto &c = *static_cast<Context *>(opaque);
    try { return c.device->CreateTexture(c.image.width, c.image.height,
                c.image.mips.size(), 0, D3DFMT_DXT4, D3DPOOL_MANAGED, &c.api); }
    catch (...) { return E_OUTOFMEMORY; }
  };
  const auto copy = [](void *opaque) -> UINT {
    auto &c = *static_cast<Context *>(opaque);
    try {
      auto *t = static_cast<Texture8 *>(c.api);
      for (UINT i = 0; i < c.image.mips.size(); ++i) {
        const auto &m = c.image.mips[i];
        t->mips[i].bytes.assign(c.image.bytes.begin() + m.offset,
                                c.image.bytes.begin() + m.offset + m.slice_pitch);
      }
      return 0u;
    } catch (...) { return 1u; }
  };
  const auto upload = [](void *opaque, std::int32_t *result, bool *threw) {
    auto &c = *static_cast<Context *>(opaque);
    try { *result = static_cast<Texture8 *>(c.api)->upload(0); }
    catch (...) { *threw = true; }
  };
  const auto release = [](void *opaque) {
    static_cast<Context *>(opaque)->api->Release();
  };
  const auto publish = [](void *opaque) {
    auto &c = *static_cast<Context *>(opaque);
    *c.out = c.api;
  };
    return a2fo_game_load_texture_workflow(&context, load, create, copy,
               upload, release, publish, catalog_failure, S_FALSE, E_OUTOFMEMORY);
  } catch (...) { return E_OUTOFMEMORY; }
}
std::string legacy_source_path(IDirect3DDevice8 *device,
                               const char *requested) noexcept {
  try {
    if (!devices.count(device))
      return {};
    auto *d = static_cast<Device *>(device);
    if (d->config.original_texture_root.empty())
      return {};
    const auto transport = bc_transport_path(requested);
    if (transport.empty())
      return {};
    const auto at = a2fo_game_basename_index(
        reinterpret_cast<const BYTE *>(transport.data()), transport.size());
    const auto path =
        d->config.original_texture_root + "\\" + transport.substr(at);
    DWORD attr = GetFileAttributesA(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
      return {};
    return path;
  } catch (...) {
    return {};
  }
}
// Remove before releasing resources: a recycled pointer must not identify a
// later native provider device as our facade.
bool is_bc_device(IDirect3DDevice8 *device) noexcept {
  return devices.count(device) && static_cast<Device *>(device)->config.bc_assets;
}
bool is_bc_texture(IDirect3DDevice8 *device, IDirect3DTexture8 *texture) noexcept {
  if (!is_bc_device(device)) return false;
  auto *r = static_cast<Device *>(device)->find(texture, Resource::texture);
  return r && static_cast<Texture8 *>(r->api)->compressed != nullptr;
}
void forget_device(Device *d) { devices.erase(d); }
} // namespace a2fo::renderer::game
