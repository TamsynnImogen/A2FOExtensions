#include "private.hpp"
#include "../../../rust/game_device_ffi.h"
#include <cmath>
#include <exception>
#include <iterator>
#include <cstdio>
#include <new>
namespace a2fo::renderer::game {
void forget_device(Device *);
void identity(Matrix &m) { a2fo_game_identity(m.m); }
void multiply(Matrix &out, const Matrix &a, const Matrix &b) {
  a2fo_game_multiply(out.m, a.m, b.m);
}
Device::Device(Factory *f, UINT adapter, D3DDEVTYPE type, HWND window,
               DWORD flags, const D3DPRESENT_PARAMETERS &p)
    : factory(f), config(f->config), thread(GetCurrentThreadId()),
      presentation(p) {
  factory->AddRef();
  creation = {adapter, type, window, flags};
  factory->native->GetDeviceCaps(adapter, type, &capabilities);
  factory->caps(capabilities);
  defaults();
}
Device::~Device() {
  forget_device(this);
  destroying = true;
  for (auto &r : resources)
    r.second->owner = nullptr;
  resources.clear();
  recording.reset();
  blocks.clear();
  current = Data{};
  render_target = Binding{};
  depth_target = Binding{};
  if (back)
    back->resource.drop();
  if (depth)
    depth->resource.drop();
  factory->Release();
}
HRESULT Device::fail(const char *name, HRESULT hr) {
  if (config.log && failures.size() < 128 && failures.insert(name).second) {
    char code[40];
    std::snprintf(code, sizeof(code), " HRESULT=0x%08lx",
                  static_cast<unsigned long>(hr));
    config.log(std::string("[D3D8Compat] ") + name + code);
  }
  return hr;
}
Resource *Device::find(void *pointer, Resource::Kind kind) {
  auto it = resources.find(pointer);
  return it != resources.end() && it->second->kind == kind ? it->second
                                                           : nullptr;
}
void Device::defaults() {
  current = Data{};
  auto &s = current.gpu;
  identity(s.world);
  identity(s.view);
  identity(s.projection);
  for (auto &t : s.texture)
    identity(t);
  s.width = presentation.BackBufferWidth;
  s.height = presentation.BackBufferHeight;
  std::array<std::uint32_t, 256> render{};
  std::array<std::uint32_t, 8 * 33> stage{};
  a2fo_game_defaults(render.data(), stage.data(),
                     presentation.EnableAutoDepthStencil != FALSE);
  for (std::size_t i = 0; i < render.size(); ++i) s.render[i] = render[i];
  for (std::size_t i = 0; i < stage.size(); ++i)
    s.stage[i / 33][i % 33] = stage[i];
}
HRESULT Device::initialize() {
  HRESULT hr = backend.initialize(
      presentation.hDeviceWindow ? presentation.hDeviceWindow
                                 : creation.hFocusWindow,
      presentation.BackBufferWidth, presentation.BackBufferHeight,
      presentation.Windowed != FALSE, config.vsync, config.debug,
      config.shader_cache, config.log,
      presentation.EnableAutoDepthStencil ? UINT(presentation.AutoDepthStencilFormat) : 0u);
  if (FAILED(hr))
    return fail("D3D11 game device creation", hr);
  hr = rebuild_surfaces();
  if (SUCCEEDED(hr) && config.log)
    config.log("[D3D11] Game device created: no native D3D8 device; translated "
               "draws/presentation active");
  return hr;
}
HRESULT Device::rebuild_surfaces() {
  try {
    D3DSURFACE_DESC d = {presentation.BackBufferFormat,
                         D3DRTYPE_SURFACE,
                         D3DUSAGE_RENDERTARGET,
                         D3DPOOL_DEFAULT,
                         presentation.BackBufferWidth *
                             presentation.BackBufferHeight * 4,
                         D3DMULTISAMPLE_NONE,
                         presentation.BackBufferWidth,
                         presentation.BackBufferHeight};
    back = new Surface8(this, d, true);
    back->target = backend.backbuffer();
    render_target = Binding(&back->resource);
    if (presentation.EnableAutoDepthStencil) {
      d.Format = presentation.AutoDepthStencilFormat;
      d.Usage = D3DUSAGE_DEPTHSTENCIL;
      d.Size = d.Width * d.Height *
          a2fo_game_depth_bytes(d.Format);
      depth = new Surface8(this, d, true);
      depth->target = backend.depthbuffer();
      depth_target = Binding(&depth->resource);
    }
    return backend.bind_target(back->target, depth ? depth->target : nullptr);
  } catch (...) {
    return E_OUTOFMEMORY;
  }
}
HRESULT Device::QueryInterface(REFIID iid, void **out) {
  if (!out)
    return E_POINTER;
  *out = nullptr;
  if (!IsEqualGUID(iid, IID_IUnknown) &&
      !IsEqualGUID(iid, IID_IDirect3DDevice8))
    return E_NOINTERFACE;
  *out = this;
  AddRef();
  return S_OK;
}
ULONG Device::AddRef() { return InterlockedIncrement(&refs); }
ULONG Device::Release() {
  LONG n = InterlockedDecrement(&refs);
  if (!n)
    delete this;
  return n;
}
HRESULT Device::TestCooperativeLevel() { return backend.cooperative(); }
UINT Device::GetAvailableTextureMem() { return backend.available_memory(); }
HRESULT Device::ResourceManagerDiscardBytes(DWORD) { return D3D_OK; }
HRESULT Device::GetDirect3D(IDirect3D8 **out) {
  if (!out)
    return invalid_call;
  *out = factory;
  factory->AddRef();
  return D3D_OK;
}
HRESULT Device::GetDeviceCaps(D3DCAPS8 *out) {
  if (!out)
    return invalid_call;
  *out = capabilities;
  return D3D_OK;
}
HRESULT Device::GetDisplayMode(D3DDISPLAYMODE *out) {
  return factory->native->GetAdapterDisplayMode(creation.AdapterOrdinal, out);
}
HRESULT Device::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *out) {
  if (!out)
    return invalid_call;
  *out = creation;
  return D3D_OK;
}
HRESULT Device::Reset(D3DPRESENT_PARAMETERS *p) {
  struct Context {
    Device *device;
    D3DPRESENT_PARAMETERS *input;
    D3DPRESENT_PARAMETERS next{};
    A2foGamePresentationPlan *plan = nullptr;
    std::exception_ptr exception;
  } context{this, p, {}, nullptr, {}};
  if (p) context.next = *p;
  A2foGamePresentationPlan plan{
      0, reinterpret_cast<std::uintptr_t>(context.next.hDeviceWindow),
      context.next.BackBufferWidth, context.next.BackBufferHeight,
      UINT(context.next.BackBufferFormat), context.next.BackBufferCount,
      UINT(context.next.MultiSampleType)};
  context.plan = &plan;
  const auto fail_callback = [](void *opaque, UINT reason, std::int32_t code) -> std::int32_t {
    auto &c = *static_cast<Context *>(opaque);
    const char *names[] = {"Reset invalid parameters/state",
      "Reset changing device window or backbuffer count unsupported",
      "Reset unsupported backbuffer format",
      "Reset while default surface is externally referenced",
      "Reset while default buffer is externally referenced",
      "Reset while default texture is externally referenced", "Reset/ResizeBuffers"};
    try { return c.device->fail(names[reason], code); }
    catch (...) { c.exception = std::current_exception(); return E_OUTOFMEMORY; }
  };
  const auto client = [](void *opaque, UINT *w, UINT *h) -> bool {
    auto &c = *static_cast<Context *>(opaque);
    RECT rect{};
    if (!GetClientRect(reinterpret_cast<HWND>(c.plan->device_window), &rect)) return false;
    *w = rect.right; *h = rect.bottom; return true;
  };
  const auto count = [](void *opaque) -> UINT {
    return static_cast<Context *>(opaque)->device->resources.size();
  };
  const auto info = [](void *opaque, UINT index, UINT *kind, UINT *pool,
                       std::int32_t *refs) {
    auto &items = static_cast<Context *>(opaque)->device->resources;
    auto it = items.begin(); std::advance(it, index);
    auto *r = it->second;
    *kind = r->kind; *refs = r->refs;
    if (r->kind == Resource::surface) *pool = static_cast<Surface8 *>(r->api)->desc.Pool;
    else if (r->kind == Resource::vertex) *pool = static_cast<VertexBuffer8 *>(r->api)->desc.Pool;
    else if (r->kind == Resource::index) *pool = static_cast<IndexBuffer8 *>(r->api)->desc.Pool;
    else *pool = static_cast<Texture8 *>(r->api)->pool;
  };
  const auto clear = [](void *opaque) -> std::int32_t {
    auto &c = *static_cast<Context *>(opaque);
    auto *d = c.device;
    try {
      d->render_target = Binding{}; d->depth_target = Binding{};
      if (d->back) { d->back->resource.drop(); d->back = nullptr; }
      if (d->depth) { d->depth->resource.drop(); d->depth = nullptr; }
      return D3D_OK;
    } catch (...) { c.exception = std::current_exception(); return E_OUTOFMEMORY; }
  };
  const auto resize = [](void *opaque, std::int32_t *result, bool *threw) {
    auto &c = *static_cast<Context *>(opaque);
    auto *d = c.device;
    try { *result = d->backend.resize(c.plan->width, c.plan->height,
             c.next.Windowed != FALSE, c.next.EnableAutoDepthStencil ?
             UINT(c.next.AutoDepthStencilFormat) : 0u); }
    catch (...) { c.exception = std::current_exception(); *result = E_OUTOFMEMORY; *threw = true; }
  };
  const auto commit = [](void *opaque) -> std::int32_t {
    auto &c = *static_cast<Context *>(opaque);
    try {
      c.next.hDeviceWindow = reinterpret_cast<HWND>(c.plan->device_window);
      c.next.BackBufferFormat = D3DFORMAT(c.plan->format);
      c.next.BackBufferWidth = c.plan->width; c.next.BackBufferHeight = c.plan->height;
      c.device->presentation = c.next; *c.input = c.next; c.device->defaults();
      return D3D_OK;
    } catch (...) { c.exception = std::current_exception(); return E_OUTOFMEMORY; }
  };
  const auto rebuild = [](void *opaque) -> std::int32_t {
    auto &c = *static_cast<Context *>(opaque);
    try { return c.device->rebuild_surfaces(); }
    catch (...) { c.exception = std::current_exception(); return E_OUTOFMEMORY; }
  };
  auto result = a2fo_game_reset_workflow(
      &context, &plan, p != nullptr, recording != nullptr, in_scene,
      reinterpret_cast<std::uintptr_t>(presentation.hDeviceWindow),
      UINT(presentation.BackBufferFormat), client, count, info, clear,
      resize, commit, rebuild, fail_callback, invalid_call);
  if (context.exception) std::rethrow_exception(context.exception);
  return result;
}
HRESULT Device::Present(const RECT *src, const RECT *dst, HWND window,
                        const RGNDATA *dirty) {
  HRESULT hr = backend.present(src, dst, window, dirty);
  if (FAILED(hr))
    return fail("Present", hr);
  ++frames;
  if (a2fo_game_present_log(frames)) {
    if (config.log)
      config.log("[D3D11] frame=" + std::to_string(frames) +
                 " translated_draws=" + std::to_string(draws) +
                 " compressed_draws=" + std::to_string(compressed_draws) +
                 " bc_uploads=" + std::to_string(bc_uploads) +
                 " bc_live_encoded_bytes=" + std::to_string(bc_live_bytes) +
                 " unsupported_categories=" + std::to_string(failures.size()));
    backend.log_memory();
  }
  return hr;
}
HRESULT Device::GetBackBuffer(UINT index, D3DBACKBUFFER_TYPE type,
                              IDirect3DSurface8 **out) {
  if (!out)
    return invalid_call;
  *out = nullptr;
  if (index || type != D3DBACKBUFFER_TYPE_MONO || !back)
    return invalid_call;
  back->AddRef();
  *out = back;
  return D3D_OK;
}
HRESULT Device::CreateTexture(UINT w, UINT h, UINT levels, DWORD usage,
                              D3DFORMAT format, D3DPOOL pool,
                              IDirect3DTexture8 **out) {
  if (!out)
    return invalid_call;
  *out = nullptr;
  if (!a2fo_game_texture_admission(w, h, levels, usage, format, pool,
                                   pixel_pitch(format, w) != 0))
    return fail("CreateTexture unsupported format/dimensions");
  try {
    auto t =
        std::make_unique<Texture8>(this, w, h, levels, usage, format, pool);
    HRESULT hr = t->initialize();
    if (FAILED(hr))
      return fail("CreateTexture modern allocation", hr);
    *out = t.release();
    return D3D_OK;
  } catch (...) {
    return E_OUTOFMEMORY;
  }
}
HRESULT Device::CreateVertexBuffer(UINT n, DWORD usage, DWORD fvf, D3DPOOL pool,
                                   IDirect3DVertexBuffer8 **out) {
  if (!out)
    return invalid_call;
  *out = nullptr;
  if (!a2fo_game_buffer_admission(n, pool))
    return invalid_call;
  try {
    *out = new VertexBuffer8(this, n, usage, fvf, pool);
    return D3D_OK;
  } catch (...) {
    return E_OUTOFMEMORY;
  }
}
HRESULT Device::CreateIndexBuffer(UINT n, DWORD usage, D3DFORMAT format,
                                  D3DPOOL pool, IDirect3DIndexBuffer8 **out) {
  if (!out)
    return invalid_call;
  *out = nullptr;
  if (!a2fo_game_index_admission(n, format, pool))
    return invalid_call;
  try {
    *out = new IndexBuffer8(this, n, usage, format, pool);
    return D3D_OK;
  } catch (...) {
    return E_OUTOFMEMORY;
  }
}
HRESULT Device::CreateRenderTarget(UINT w, UINT h, D3DFORMAT f,
                                   D3DMULTISAMPLE_TYPE ms, WINBOOL lockable,
                                   IDirect3DSurface8 **out) {
  if (!out)
    return invalid_call;
  *out = nullptr;
  if (!a2fo_game_surface_admission(ms, lockable != FALSE))
    return fail("CreateRenderTarget multisample/lockable unsupported");
  try {
    std::shared_ptr<Target> target;
    HRESULT hr = backend.create_target(w, h, f, false, target);
    if (FAILED(hr))
      return fail("CreateRenderTarget", hr);
    D3DSURFACE_DESC d = {f,
                         D3DRTYPE_SURFACE,
                         D3DUSAGE_RENDERTARGET,
                         D3DPOOL_DEFAULT,
                         w * h * 4,
                         D3DMULTISAMPLE_NONE,
                         w,
                         h};
    auto *surface = new Surface8(this, d);
    surface->target = std::move(target);
    *out = surface;
    return D3D_OK;
  } catch (...) {
    return E_OUTOFMEMORY;
  }
}
HRESULT Device::CreateDepthStencilSurface(UINT w, UINT h, D3DFORMAT f,
                                          D3DMULTISAMPLE_TYPE ms,
                                          IDirect3DSurface8 **out) {
  if (!out)
    return invalid_call;
  *out = nullptr;
  if (!a2fo_game_surface_admission(ms, false))
    return invalid_call;
  try {
    std::shared_ptr<Target> target;
    HRESULT hr = backend.create_target(w, h, f, true, target);
    if (FAILED(hr))
      return fail("CreateDepthStencilSurface", hr);
    D3DSURFACE_DESC d = {f,
                         D3DRTYPE_SURFACE,
                         D3DUSAGE_DEPTHSTENCIL,
                         D3DPOOL_DEFAULT,
                         w * h * 4,
                         D3DMULTISAMPLE_NONE,
                         w,
                         h};
    auto *s = new Surface8(this, d);
    s->target = std::move(target);
    *out = s;
    return D3D_OK;
  } catch (...) {
    return E_OUTOFMEMORY;
  }
}
HRESULT Device::CreateImageSurface(UINT w, UINT h, D3DFORMAT f,
                                   IDirect3DSurface8 **out) {
  if (!out)
    return invalid_call;
  *out = nullptr;
  UINT pitch = pixel_pitch(f, w);
  if (!a2fo_game_image_admission(w, h, pitch))
    return invalid_call;
  try {
    D3DSURFACE_DESC d = {f,
                         D3DRTYPE_SURFACE,
                         0,
                         D3DPOOL_SYSTEMMEM,
                         pitch * pixel_rows(f, h),
                         D3DMULTISAMPLE_NONE,
                         w,
                         h};
    auto *s = new Surface8(this, d);
    s->bytes.resize(d.Size);
    *out = s;
    return D3D_OK;
  } catch (...) {
    return E_OUTOFMEMORY;
  }
}
HRESULT Device::SetRenderTarget(IDirect3DSurface8 *target,
                                IDirect3DSurface8 *z) {
  auto *r = find(target, Resource::surface);
  auto *d = z ? find(z, Resource::surface) : nullptr;
  if (!r || (z && !d))
    return invalid_call;
  auto *color = static_cast<Surface8 *>(r->api);
  auto *depth_value = d ? static_cast<Surface8 *>(d->api) : nullptr;
  if (!color->target || (d && !depth_value->target))
    return invalid_call;
  HRESULT hr = backend.bind_target(color->target,
                                   depth_value ? depth_value->target : nullptr);
  if (FAILED(hr))
    return fail("SetRenderTarget", hr);
  render_target = Binding(r);
  depth_target = Binding(d);
  current.gpu.x = current.gpu.y = 0;
  current.gpu.width = color->desc.Width;
  current.gpu.height = color->desc.Height;
  current.gpu.min_z = 0;
  current.gpu.max_z = 1;
  return D3D_OK;
}
HRESULT Device::GetRenderTarget(IDirect3DSurface8 **out) {
  if (!out || !render_target.resource)
    return invalid_call;
  auto *s = static_cast<Surface8 *>(render_target.resource->api);
  s->AddRef();
  *out = s;
  return D3D_OK;
}
HRESULT Device::GetDepthStencilSurface(IDirect3DSurface8 **out) {
  if (!out)
    return invalid_call;
  *out = nullptr;
  if (!depth_target.resource)
    return D3DERR_NOTFOUND;
  auto *s = static_cast<Surface8 *>(depth_target.resource->api);
  s->AddRef();
  *out = s;
  return D3D_OK;
}
HRESULT Device::BeginScene() {
  if (!a2fo_game_scene_transition(in_scene, true))
    return invalid_call;
  in_scene = true;
  return D3D_OK;
}
HRESULT Device::EndScene() {
  if (!a2fo_game_scene_transition(in_scene, false))
    return invalid_call;
  in_scene = false;
  return D3D_OK;
}
HRESULT Device::Clear(DWORD n, const D3DRECT *r, DWORD f, D3DCOLOR c, float z,
                      DWORD stencil) {
  static_assert(sizeof(RECT) == sizeof(D3DRECT));
  HRESULT hr = backend.clear(current.gpu, n, reinterpret_cast<const RECT *>(r),
                             f, c, z, stencil);
  return FAILED(hr) ? fail("Clear", hr) : hr;
}
int Device::matrix_slot(DWORD state) {
  return a2fo_game_matrix_slot(state);
}
Matrix *Device::matrix(Data &data, DWORD state) {
  int slot = matrix_slot(state);
  if (slot == 0)
    return &data.gpu.world;
  if (slot == 1)
    return &data.gpu.view;
  if (slot == 2)
    return &data.gpu.projection;
  if (slot >= 3)
    return &data.gpu.texture[slot - 3];
  return nullptr;
}
HRESULT Device::SetTransform(D3DTRANSFORMSTATETYPE state,
                             const D3DMATRIX *input) {
  Matrix *output = matrix(setting(), state);
  if (!input || !output)
    return fail("SetTransform unsupported index");
  std::memcpy(output->m, input, 64);
  if (mask())
    mask()->transforms.set(matrix_slot(state));
  return D3D_OK;
}
HRESULT Device::GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *out) {
  Matrix *value = matrix(current, state);
  if (!out || !value)
    return invalid_call;
  std::memcpy(out, value, 64);
  return D3D_OK;
}
HRESULT Device::MultiplyTransform(D3DTRANSFORMSTATETYPE state,
                                  const D3DMATRIX *input) {
  if (!input)
    return invalid_call;
  Matrix *output = matrix(setting(), state);
  if (!output)
    return invalid_call;
  Matrix copy;
  std::memcpy(copy.m, input, 64);
  multiply(*output, copy, *output);
  if (mask())
    mask()->transforms.set(matrix_slot(state));
  return D3D_OK;
}
HRESULT Device::SetViewport(const D3DVIEWPORT8 *viewport) {
  if (!viewport || !a2fo_game_viewport(viewport->Width, viewport->Height,
                                        viewport->MinZ, viewport->MaxZ))
    return invalid_call;
  auto &s = setting().gpu;
  s.x = viewport->X;
  s.y = viewport->Y;
  s.width = viewport->Width;
  s.height = viewport->Height;
  s.min_z = viewport->MinZ;
  s.max_z = viewport->MaxZ;
  if (mask())
    mask()->viewport = true;
  return D3D_OK;
}
HRESULT Device::GetViewport(D3DVIEWPORT8 *out) {
  if (!out)
    return invalid_call;
  const auto &s = current.gpu;
  *out = {s.x, s.y, s.width, s.height, s.min_z, s.max_z};
  return D3D_OK;
}
HRESULT Device::SetMaterial(const D3DMATERIAL8 *m) {
  if (!m)
    return invalid_call;
  static_assert(sizeof(Material) == sizeof(D3DMATERIAL8));
  std::memcpy(static_cast<void *>(&setting().gpu.material), m, sizeof(*m));
  if (mask())
    mask()->material = true;
  return D3D_OK;
}
HRESULT Device::GetMaterial(D3DMATERIAL8 *m) {
  if (!m)
    return invalid_call;
  std::memcpy(m, &current.gpu.material, sizeof(*m));
  return D3D_OK;
}
HRESULT Device::SetLight(DWORD i, const D3DLIGHT8 *light) {
  if (!light || !a2fo_game_state_index(0, i, 0))
    return fail("SetLight index above supported eight lights");
  auto &l = setting().gpu.lights[i];
  bool enabled = l.enabled;
  static_assert(offsetof(Light, enabled) == sizeof(D3DLIGHT8));
  std::memcpy(static_cast<void *>(&l), light, sizeof(*light));
  l.enabled = enabled;
  if (mask())
    mask()->lights.set(i);
  return D3D_OK;
}
HRESULT Device::GetLight(DWORD i, D3DLIGHT8 *light) {
  if (!light || !a2fo_game_state_index(0, i, 0))
    return invalid_call;
  std::memcpy(light, &current.gpu.lights[i], sizeof(*light));
  return D3D_OK;
}
HRESULT Device::LightEnable(DWORD i, WINBOOL enabled) {
  if (!a2fo_game_state_index(0, i, 0))
    return invalid_call;
  setting().gpu.lights[i].enabled = enabled != FALSE;
  if (mask())
    mask()->lights.set(i);
  return D3D_OK;
}
HRESULT Device::GetLightEnable(DWORD i, WINBOOL *out) {
  if (!a2fo_game_state_index(0, i, 0) || !out)
    return invalid_call;
  *out = current.gpu.lights[i].enabled ? 128 : 0;
  return D3D_OK;
}
HRESULT Device::SetRenderState(D3DRENDERSTATETYPE state, DWORD value) {
  if (!a2fo_game_state_index(1, state, 0))
    return invalid_call;
  setting().gpu.render[state] = value;
  if (mask())
    mask()->render.set(state);
  return D3D_OK;
}
HRESULT Device::GetRenderState(D3DRENDERSTATETYPE state, DWORD *out) {
  if (!out || !a2fo_game_state_index(1, state, 0))
    return invalid_call;
  *out = current.gpu.render[state];
  return D3D_OK;
}
HRESULT Device::SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type,
                                     DWORD value) {
  if (!a2fo_game_state_index(2, stage, type))
    return invalid_call;
  setting().gpu.stage[stage][type] = value;
  if (mask())
    mask()->stage[stage].set(type);
  return D3D_OK;
}
HRESULT Device::GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type,
                                     DWORD *out) {
  if (!out || !a2fo_game_state_index(2, stage, type))
    return invalid_call;
  *out = current.gpu.stage[stage][type];
  return D3D_OK;
}
HRESULT Device::SetTexture(DWORD stage, IDirect3DBaseTexture8 *texture) {
  if (!a2fo_game_state_index(0, stage, 0))
    return invalid_call;
  auto *r = texture ? find(texture, Resource::texture) : nullptr;
  if (texture && !r)
    return fail("SetTexture foreign device/resource");
  setting().textures[stage] = Binding(r);
  if (mask())
    mask()->textures.set(stage);
  return D3D_OK;
}
HRESULT Device::GetTexture(DWORD stage, IDirect3DBaseTexture8 **out) {
  if (!a2fo_game_state_index(0, stage, 0) || !out)
    return invalid_call;
  *out = nullptr;
  if (auto *r = current.textures[stage].resource) {
    auto *t = static_cast<Texture8 *>(r->api);
    t->AddRef();
    *out = t;
  }
  return D3D_OK;
}
HRESULT Device::SetStreamSource(UINT i, IDirect3DVertexBuffer8 *buffer,
                                UINT stride) {
  if (!a2fo_game_state_index(3, i, stride))
    return invalid_call;
  auto *r = buffer ? find(buffer, Resource::vertex) : nullptr;
  if (buffer && !r)
    return invalid_call;
  setting().streams[i] = Binding(r);
  setting().strides[i] = stride;
  if (mask())
    mask()->streams.set(i);
  return D3D_OK;
}
HRESULT Device::GetStreamSource(UINT i, IDirect3DVertexBuffer8 **out,
                                UINT *stride) {
  if (!a2fo_game_state_index(3, i, 0) || !out || !stride)
    return invalid_call;
  *out = nullptr;
  *stride = current.strides[i];
  if (auto *r = current.streams[i].resource) {
    auto *b = static_cast<VertexBuffer8 *>(r->api);
    b->AddRef();
    *out = b;
  }
  return D3D_OK;
}
HRESULT Device::SetIndices(IDirect3DIndexBuffer8 *buffer, UINT base) {
  auto *r = buffer ? find(buffer, Resource::index) : nullptr;
  if (buffer && !r)
    return invalid_call;
  setting().indices = Binding(r);
  setting().base = base;
  if (mask())
    mask()->indices = true;
  return D3D_OK;
}
HRESULT Device::GetIndices(IDirect3DIndexBuffer8 **out, UINT *base) {
  if (!out || !base)
    return invalid_call;
  *out = nullptr;
  *base = current.base;
  if (auto *r = current.indices.resource) {
    auto *b = static_cast<IndexBuffer8 *>(r->api);
    b->AddRef();
    *out = b;
  }
  return D3D_OK;
}
HRESULT Device::ValidateDevice(DWORD *passes) {
  if (!passes)
    return invalid_call;
  *passes = 1;
  return D3D_OK;
}
} // namespace a2fo::renderer::game
