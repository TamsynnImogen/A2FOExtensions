#include "private.hpp"
#include "../../../rust/render_cache_state_ffi.h"
#include <exception>
#include <optional>

namespace a2fo::renderer::game {
namespace {
struct StateContext {
  Device &device;
  explicit StateContext(Device &d) : device(d) {}
  DWORD token = 0;
  void *first = nullptr, *second = nullptr, *output = nullptr;
  const RECT *rects = nullptr;
  const POINT *points = nullptr;
  Data *dst = nullptr;
  const Data *src = nullptr;
  const Mask *selected_mask = nullptr;
  Texture8 *from_texture = nullptr, *to_texture = nullptr;
  Surface8 *from_surface = nullptr, *to_surface = nullptr;
  std::optional<Snapshot> created;
  std::exception_ptr error;
};
void encode_mask(A2FoStateMask &out, const Mask &m) {
  for (UINT i = 0; i < 256; ++i) out.render[i] = m.render[i];
  for (UINT s = 0; s < 8; ++s) {
    for (UINT t = 0; t < 33; ++t) out.stage[s][t] = m.stage[s][t];
    out.textures[s] = m.textures[s];
    out.lights[s] = m.lights[s];
  }
  for (UINT i = 0; i < 11; ++i) out.transforms[i] = m.transforms[i];
  for (UINT i = 0; i < 16; ++i) out.streams[i] = m.streams[i];
  for (UINT i = 0; i < 96; ++i) out.constants[i] = m.constants[i];
  out.viewport = m.viewport; out.material = m.material; out.indices = m.indices;
  out.vertex_shader = m.vertex_shader; out.pixel_shader = m.pixel_shader;
  out.clip = m.clip;
}
void decode_mask(Mask &m, const A2FoStateMask &in) {
  for (UINT i = 0; i < 256; ++i) m.render[i] = in.render[i] != 0;
  for (UINT s = 0; s < 8; ++s) {
    for (UINT t = 0; t < 33; ++t) m.stage[s][t] = in.stage[s][t] != 0;
    m.textures[s] = in.textures[s] != 0;
    m.lights[s] = in.lights[s] != 0;
  }
  for (UINT i = 0; i < 11; ++i) m.transforms[i] = in.transforms[i] != 0;
  for (UINT i = 0; i < 16; ++i) m.streams[i] = in.streams[i] != 0;
  for (UINT i = 0; i < 96; ++i) m.constants[i] = in.constants[i] != 0;
  m.viewport = in.viewport != 0; m.material = in.material != 0;
  m.indices = in.indices != 0; m.vertex_shader = in.vertex_shader != 0;
  m.pixel_shader = in.pixel_shader != 0; m.clip = in.clip != 0;
}
int state_call(void *raw, std::uint32_t op, std::uint32_t a,
               std::uint32_t b, A2FoStateReply *r) noexcept {
  auto &c = *static_cast<StateContext *>(raw);
  auto &d = c.device;
  try {
    switch (op) {
    case A2FO_STATE_QUERY:
      r->value = bool(d.recording);
      r->width = d.next_block;
      if (a == A2FO_STATE_END) {
        if (d.recording) encode_mask(r->mask, d.recording->mask);
      } else if (a == A2FO_STATE_APPLY || a == A2FO_STATE_CAPTURE) {
        r->other = c.selected_mask != nullptr;
        if (c.selected_mask) encode_mask(r->mask, *c.selected_mask);
      } else if (a == A2FO_STATE_DELETE) {
        r->other = d.blocks.find(c.token) != d.blocks.end();
      }
      break;
    case A2FO_STATE_MAKE_RECORDING:
      d.recording = std::make_unique<Snapshot>();
      d.recording->data = d.current;
      r->hr = D3D_OK;
      break;
    case A2FO_STATE_CLEAR_BINDING:
      if (a == 1) d.recording->data.textures[b] = Binding{};
      else if (a == 2) d.recording->data.streams[b] = Binding{};
      else if (a == 3) d.recording->data.indices = Binding{};
      else d.recording->data.vertex_object.reset();
      break;
    case A2FO_STATE_SET_NEXT: d.next_block = a; break;
    case A2FO_STATE_EMPLACE_RECORDING:
      d.blocks.emplace(a, std::move(*d.recording));
      d.recording.reset();
      *static_cast<DWORD *>(c.output) = a;
      break;
    case A2FO_STATE_MAKE_CREATED:
      c.created.emplace();
      c.created->data = d.current;
      break;
    case A2FO_STATE_SET_ALL_MASK: {
      decode_mask(c.created->mask, r->mask);
      break;
    }
    case A2FO_STATE_EMPLACE_CREATED:
      d.blocks.emplace(a, std::move(*c.created));
      *static_cast<DWORD *>(c.output) = a;
      break;
    case A2FO_STATE_COPY_FIELD: {
      auto &dst = *c.dst;
      const auto &src = *c.src;
      switch (a) {
      case A2FO_STATE_FIELD_RENDER: dst.gpu.render[b] = src.gpu.render[b]; break;
      case A2FO_STATE_FIELD_STAGE: dst.gpu.stage[b / 33][b % 33] = src.gpu.stage[b / 33][b % 33]; break;
      case A2FO_STATE_FIELD_TEXTURE: dst.textures[b] = src.textures[b]; break;
      case A2FO_STATE_FIELD_LIGHT: dst.gpu.lights[b] = src.gpu.lights[b]; break;
      case A2FO_STATE_FIELD_TRANSFORM:
        if (b == 0) dst.gpu.world = src.gpu.world;
        else if (b == 1) dst.gpu.view = src.gpu.view;
        else if (b == 2) dst.gpu.projection = src.gpu.projection;
        else dst.gpu.texture[b - 3] = src.gpu.texture[b - 3];
        break;
      case A2FO_STATE_FIELD_MATERIAL: dst.gpu.material = src.gpu.material; break;
      case A2FO_STATE_FIELD_VIEWPORT:
        dst.gpu.x = src.gpu.x; dst.gpu.y = src.gpu.y;
        dst.gpu.width = src.gpu.width; dst.gpu.height = src.gpu.height;
        dst.gpu.min_z = src.gpu.min_z; dst.gpu.max_z = src.gpu.max_z;
        break;
      case A2FO_STATE_FIELD_STREAM: dst.streams[b] = src.streams[b]; dst.strides[b] = src.strides[b]; break;
      case A2FO_STATE_FIELD_INDICES: dst.indices = src.indices; dst.base = src.base; break;
      case A2FO_STATE_FIELD_VERTEX_SHADER:
        dst.vertex_shader = src.vertex_shader; dst.vertex_object = src.vertex_object; break;
      case A2FO_STATE_FIELD_PIXEL_SHADER: dst.pixel_shader = src.pixel_shader; break;
      case A2FO_STATE_FIELD_CONSTANT: std::memcpy(dst.gpu.constants[b], src.gpu.constants[b], 16); break;
      case A2FO_STATE_FIELD_CLIP:
        dst.clip = src.clip; std::memcpy(dst.planes, src.planes, sizeof(dst.planes)); break;
      }
      break;
    }
    case A2FO_STATE_ERASE: d.blocks.erase(a); r->hr = D3D_OK; break;
    case A2FO_STATE_SET_CLIP:
      if (a == A2FO_STATE_SET_CLIP_STATUS)
        d.setting().clip = *static_cast<const D3DCLIPSTATUS8 *>(c.first);
      else std::memcpy(d.setting().planes[b], c.first, 16);
      if (d.mask()) d.mask()->clip = true;
      r->hr = D3D_OK;
      break;
    case A2FO_STATE_GET_CLIP:
      if (a == A2FO_STATE_GET_CLIP_STATUS)
        *static_cast<D3DCLIPSTATUS8 *>(c.first) = d.current.clip;
      else std::memcpy(c.first, d.current.planes[b], 16);
      r->hr = D3D_OK;
      break;
    case A2FO_STATE_TEXTURE_INFO: {
      auto *from = d.find(c.first, Resource::texture);
      auto *to = d.find(c.second, Resource::texture);
      if (!from || !to) break;
      c.from_texture = static_cast<Texture8 *>(from->api);
      c.to_texture = static_cast<Texture8 *>(to->api);
      r->value = 1;
      r->other = c.from_texture->format == c.to_texture->format &&
          c.from_texture->width == c.to_texture->width &&
          c.from_texture->height == c.to_texture->height &&
          c.from_texture->mips.size() == c.to_texture->mips.size();
      r->levels = static_cast<UINT>(c.from_texture->mips.size());
      break;
    }
    case A2FO_STATE_TEXTURE_LOCKED:
      r->value = c.from_texture->locked[a] || c.to_texture->locked[a]; break;
    case A2FO_STATE_TEXTURE_COPY:
      c.to_texture->mips[a].bytes = c.from_texture->mips[a].bytes; break;
    case A2FO_STATE_TEXTURE_UPLOAD: r->hr = c.to_texture->upload(a); break;
    case A2FO_STATE_SURFACE_INFO: {
      auto *from = d.find(c.first, Resource::surface);
      auto *to = d.find(c.second, Resource::surface);
      if (!from || !to) break;
      c.from_surface = static_cast<Surface8 *>(from->api);
      c.to_surface = static_cast<Surface8 *>(to->api);
      r->value = 1;
      r->format = static_cast<UINT>(c.from_surface->desc.Format);
      r->dst_pitch = c.from_surface->desc.Format == c.to_surface->desc.Format;
      r->other = bool(c.from_surface->target) | (bool(c.to_surface->target) << 1);
      r->width = c.from_surface->desc.Width; r->height = c.from_surface->desc.Height;
      r->dst_width = c.to_surface->desc.Width; r->dst_height = c.to_surface->desc.Height;
      r->levels = pixel_pitch(c.from_surface->desc.Format, 1);
      break;
    }
    case A2FO_STATE_COPY_TARGET:
      r->hr = d.backend.copy_target(*c.from_surface->target, c.rects, a,
                                    *c.to_surface->target, c.points);
      break;
    case A2FO_STATE_LOCK_SOURCE: {
      D3DLOCKED_RECT lock{};
      r->hr = c.from_surface->LockRect(&lock, nullptr, D3DLOCK_READONLY);
      r->src_bits = lock.pBits; r->src_pitch = lock.Pitch;
      break;
    }
    case A2FO_STATE_LOCK_DEST: {
      D3DLOCKED_RECT lock{};
      r->hr = c.to_surface->LockRect(&lock, nullptr, 0);
      r->dst_bits = lock.pBits; r->dst_pitch = lock.Pitch;
      break;
    }
    case A2FO_STATE_UNLOCK_SOURCE: r->hr = c.from_surface->UnlockRect(); break;
    case A2FO_STATE_UNLOCK_DEST: r->hr = c.to_surface->UnlockRect(); break;
    case A2FO_STATE_RECT:
      if (c.rects) {
        r->left = c.rects[a].left; r->top = c.rects[a].top;
        r->right = c.rects[a].right; r->bottom = c.rects[a].bottom;
      }
      if (c.points) { r->x = c.points[a].x; r->y = c.points[a].y; }
      break;
    case A2FO_STATE_FAIL:
      switch (a) {
      case 1: r->hr = d.fail("CreateStateBlock typed PIXEL/VERTEX snapshots not implemented"); break;
      case 2: r->hr = d.fail("UpdateTexture mismatched levels/format"); break;
      case 3: r->hr = d.fail("CopyRects different formats"); break;
      case 4: r->hr = d.fail("CopyRects GPU target destination not implemented"); break;
      case 5: r->hr = d.fail("CopyRects source lock", static_cast<HRESULT>(b)); break;
      case 6: r->hr = d.fail("CopyRects destination lock", static_cast<HRESULT>(b)); break;
      case 7: r->hr = d.fail("GetFrontBuffer not implemented; backbuffer is not a front-buffer capture"); break;
      }
      break;
    }
    return 1;
  } catch (...) {
    c.error = std::current_exception();
    return 0;
  }
}
HRESULT run(StateContext &c, UINT method, UINT a = 0, UINT b = 0) {
  HRESULT hr = a2fo_rs_state_run(method, a, b, &c, state_call);
  if (c.error) {
    if (method == A2FO_STATE_BEGIN || method == A2FO_STATE_END ||
        method == A2FO_STATE_CREATE) return E_OUTOFMEMORY;
    std::rethrow_exception(c.error);
  }
  return hr;
}
} // namespace
void Device::apply(Data &dst, const Data &src, const Mask &m) {
  StateContext c{*this}; c.dst = &dst; c.src = &src; c.selected_mask = &m;
  (void)run(c, A2FO_STATE_APPLY);
}
HRESULT Device::BeginStateBlock() { StateContext c{*this}; return run(c, A2FO_STATE_BEGIN); }
HRESULT Device::EndStateBlock(DWORD *out) {
  StateContext c{*this}; c.output = out;
  return run(c, A2FO_STATE_END, out != nullptr);
}
HRESULT Device::ApplyStateBlock(DWORD token) {
  StateContext c{*this}; c.token = token;
  auto it = blocks.find(token);
  if (it != blocks.end()) { c.dst = &current; c.src = &it->second.data; c.selected_mask = &it->second.mask; }
  return run(c, A2FO_STATE_APPLY);
}
HRESULT Device::CaptureStateBlock(DWORD token) {
  StateContext c{*this}; c.token = token;
  auto it = blocks.find(token);
  if (it != blocks.end()) { c.dst = &it->second.data; c.src = &current; c.selected_mask = &it->second.mask; }
  return run(c, A2FO_STATE_CAPTURE);
}
HRESULT Device::DeleteStateBlock(DWORD token) {
  StateContext c{*this}; c.token = token; return run(c, A2FO_STATE_DELETE, token);
}
HRESULT Device::CreateStateBlock(D3DSTATEBLOCKTYPE type, DWORD *out) {
  StateContext c{*this}; c.output = out;
  return run(c, A2FO_STATE_CREATE, type, out != nullptr);
}
HRESULT Device::SetClipStatus(const D3DCLIPSTATUS8 *status) {
  StateContext c{*this}; c.first = const_cast<D3DCLIPSTATUS8 *>(status);
  return run(c, A2FO_STATE_SET_CLIP_STATUS, status != nullptr);
}
HRESULT Device::GetClipStatus(D3DCLIPSTATUS8 *status) {
  StateContext c{*this}; c.first = status;
  return run(c, A2FO_STATE_GET_CLIP_STATUS, status != nullptr);
}
HRESULT Device::SetClipPlane(DWORD i, const float *p) {
  StateContext c{*this}; c.first = const_cast<float *>(p);
  return run(c, A2FO_STATE_SET_CLIP_PLANE, i, p != nullptr);
}
HRESULT Device::GetClipPlane(DWORD i, float *p) {
  StateContext c{*this}; c.first = p;
  return run(c, A2FO_STATE_GET_CLIP_PLANE, i, p != nullptr);
}
HRESULT Device::UpdateTexture(IDirect3DBaseTexture8 *source,
                              IDirect3DBaseTexture8 *destination) {
  StateContext c{*this}; c.first = source; c.second = destination;
  return run(c, A2FO_STATE_UPDATE_TEXTURE);
}
HRESULT Device::CopyRects(IDirect3DSurface8 *source, const RECT *rects,
                          UINT count, IDirect3DSurface8 *destination,
                          const POINT *points) {
  StateContext c{*this}; c.first = source; c.second = destination;
  c.rects = rects; c.points = points;
  return run(c, A2FO_STATE_COPY_RECTS, count,
             (rects ? 1u : 0u) | (points ? 2u : 0u));
}
HRESULT Device::GetFrontBuffer(IDirect3DSurface8 *) {
  StateContext c{*this}; return run(c, A2FO_STATE_GET_FRONT_BUFFER);
}
} // namespace a2fo::renderer::game
