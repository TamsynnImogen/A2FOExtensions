#include "private.hpp"
#include "../../../rust/render_draw_ffi.h"
#include <cmath>
#include <cstdio>
namespace a2fo::renderer::game {
namespace {
A2foRsMemoryRegion draw_region(void *, std::uintptr_t);
struct ShaderWordsContext {std::vector<DWORD> *words;};
static_assert(static_cast<UINT>(shaders::TokenParseStatus::complete)==0);
static_assert(static_cast<UINT>(shaders::TokenParseStatus::truncated)==1);
std::uint8_t append_shader_word(void *raw,std::uint32_t value) {
  try { static_cast<ShaderWordsContext *>(raw)->words->push_back(value); return 1; }
  catch (...) { return 0; }
}
std::uint32_t parse_shader_words(void *raw,std::uint8_t declaration) {
  const auto &words=*static_cast<ShaderWordsContext *>(raw)->words;
  const auto r=declaration
      ? shaders::parse_d3d8_vertex_declaration(
          reinterpret_cast<const std::uint32_t *>(words.data()),words.size())
      : shaders::parse_d3d8_shader_bytecode(
          reinterpret_cast<const std::uint32_t *>(words.data()),words.size());
  return static_cast<std::uint32_t>(r.status);
}
std::uint8_t shader_words(const DWORD *input,bool declaration,
                          std::vector<DWORD> &words) {
  ShaderWordsContext context{&words};
  const A2foRsShaderWordsOps ops{&context,draw_region,append_shader_word,
      parse_shader_words};
  return a2fo_rs_shader_words(reinterpret_cast<const std::uint32_t *>(input),
      declaration,MEM_COMMIT,PAGE_NOACCESS,PAGE_GUARD,ops);
}
// Log only DWORDs already admitted to the bounded parser. A failed parse
// must not trigger a second scan through adjacent, unrelated readable memory.
std::string diagnostic_words(const std::vector<DWORD>& input) {
  std::string result;
  for (std::size_t i=0;i<input.size();++i) {
    char word[16];std::snprintf(word,sizeof(word),"%s0x%08lx",i?",":"",static_cast<unsigned long>(input[i]));
    result+=word;
  }
  return result;
}
std::uint8_t append_shader_element(void *raw,A2foRsDrawElement e) {
  try {
    static_cast<Shader *>(raw)->elements.push_back({e.stream,e.offset,e.reg,e.type});
    return 1;
  } catch (...) { return 0; }
}
HRESULT shader_blob(const std::vector<DWORD> &blob,void *out,DWORD *size) {
  const auto plan=a2fo_rs_shader_blob(
      reinterpret_cast<const std::uint8_t *>(blob.data()),blob.size(),
      static_cast<std::uint8_t *>(out),size!=nullptr,size?*size:0,
      invalid_call,D3D_OK,D3DERR_MOREDATA);
  if (size) *size=plan.needed;
  return plan.hr;
}
struct DrawContext {
  static_assert(sizeof(Vertex)==64*sizeof(float));
  Device *device;
  Draw packet;
  State state;
  compat::FvfLayout layout;
  const Shader *shader;
  std::array<Sampling, 8> views{};
  explicit DrawContext(Device *d) : device(d), state(d->current.gpu),
      shader(d->current.vertex_object.get()) {}
  ~DrawContext() {
    for (auto &s : views)
      if (s.view) reinterpret_cast<IUnknown *>(s.view)->Release();
  }
};
A2foRsMemoryRegion draw_region(void *, std::uintptr_t at) {
  MEMORY_BASIC_INFORMATION m{};
  if (!VirtualQuery(reinterpret_cast<void *>(at), &m, sizeof(m)))
    return {0, 0, 0, 0, 0};
  return {reinterpret_cast<std::uintptr_t>(m.BaseAddress), m.RegionSize,
      static_cast<std::uint32_t>(m.State), static_cast<std::uint32_t>(m.Protect), 1};
}
std::int32_t draw_count(void *, std::uint32_t type, std::uint32_t count,
                        std::uint32_t *out) {
  UINT n = 0;
  auto hr = compat::primitive_vertex_count(type, count, n);
  *out = n;
  return hr;
}
std::uint8_t draw_indices(void *raw, std::uint32_t n) {
  try { static_cast<DrawContext *>(raw)->packet.indices.resize(n); return 1; }
  catch (...) { return 0; }
}
std::uint8_t draw_vertices(void *raw, std::uint32_t n) {
  try { static_cast<DrawContext *>(raw)->packet.vertices.resize(n); return 1; }
  catch (...) { return 0; }
}
void draw_index(void *raw, std::uint32_t at, std::uint32_t value) {
  static_cast<DrawContext *>(raw)->packet.indices[at] = value;
}
float *draw_vertex(void *raw, std::uint32_t at) {
  return &static_cast<DrawContext *>(raw)->packet.vertices[at].v[0][0];
}
std::uint8_t draw_has_shader(void *raw) {
  return static_cast<DrawContext *>(raw)->shader != nullptr;
}
std::uint8_t draw_shader_hlsl_empty(void *raw) {
  return static_cast<DrawContext *>(raw)->shader->hlsl.empty();
}
std::uint32_t draw_shader_elements(void *raw) {
  return static_cast<std::uint32_t>(static_cast<DrawContext *>(raw)->shader->elements.size());
}
A2foRsDrawElement draw_shader_element(void *raw, std::uint32_t at) {
  const auto &e = static_cast<DrawContext *>(raw)->shader->elements[at];
  return {e.stream, e.offset, e.reg, e.type};
}
std::int32_t draw_layout(void *raw, A2foRsDrawLayout *out) {
  auto &c = *static_cast<DrawContext *>(raw);
  auto hr = compat::decode_fvf(c.device->current.vertex_shader, c.layout);
  if (FAILED(hr)) return hr;
  const auto &l = c.layout;
  out->stride=l.stride;out->position_components=l.position_components;
  out->texture_count=l.texture_count;out->normal_offset=l.normal_offset;
  out->diffuse_offset=l.diffuse_offset;out->specular_offset=l.specular_offset;
  out->transformed=l.transformed;out->normal=l.normal;out->diffuse=l.diffuse;
  out->specular=l.specular;
  for (UINT i=0;i<8;++i) {
    out->texture_offsets[i]=l.texture_offsets[i];
    out->texture_components[i]=l.texture_components[i];
  }
  return hr;
}
std::uint8_t draw_state(void *raw, std::uint8_t mode, std::uint8_t transformed,
                std::uint8_t normal, std::uint8_t diffuse,
                std::uint8_t specular, const std::uint32_t *components) {
  try {
  auto &c = *static_cast<DrawContext *>(raw);
  auto &s = c.state;
  if (mode==1) {
    s.vertex_hlsl = c.shader->hlsl;
    s.transformed = s.has_normal = s.has_diffuse = s.has_specular = false;
    s.tex_components.fill(0);
  } else if (mode==0) {
    s.vertex_hlsl.clear();
    s.transformed=transformed;s.has_normal=normal;
    s.has_diffuse=diffuse;s.has_specular=specular;
    for (UINT i=0;i<8;++i) s.tex_components[i]=components[i];
  } else if (mode==2) {
    if (normal) s.has_normal=true;
    if (diffuse) s.has_diffuse=true;
    if (specular) s.has_specular=true;
  } else if (mode==3) {
    for (UINT i=0;i<8;++i) s.tex_components[i]=components[i];
  }
  return 1;
  } catch (...) { return 0; }
}
A2foRsDrawSource draw_stream(void *raw, std::uint32_t stream) {
  auto &d=*static_cast<DrawContext *>(raw)->device;
  if (stream>=d.current.streams.size()) return {nullptr,0,0,0,0};
  auto *r=d.current.streams[stream].resource;
  if (!r) return {nullptr,0,0,0,0};
  auto *buffer=static_cast<VertexBuffer8 *>(r->api);
  return {buffer->bytes.data(),buffer->bytes.size(),d.current.strides[stream],1,
      static_cast<std::uint8_t>(buffer->locked)};
}
A2foRsDrawStage draw_stage(void *raw, std::uint32_t stage) {
  auto &d=*static_cast<DrawContext *>(raw)->device;
  auto *r=d.current.textures[stage].resource;
  if (!r) return {0,0,0,0,0};
  auto *t=static_cast<Texture8 *>(r->api);
  return {1,static_cast<std::uint8_t>(t->compressed!=nullptr),
      static_cast<std::uint8_t>(t->compressed&&t->normal_xy),
      static_cast<std::uint8_t>(!t->compressed&&!t->gpu),t->lod};
}
std::int32_t draw_compressed_view(void *raw, std::uint32_t stage) {
  auto &c=*static_cast<DrawContext *>(raw);
  auto *t=static_cast<Texture8 *>(c.device->current.textures[stage].resource->api);
  try {return t->compressed->get_view(&c.views[stage].view);}
  catch (...) {return std::numeric_limits<std::int32_t>::min();}
}
std::int32_t draw_upload(void *raw, std::uint32_t stage) {
  auto &d=*static_cast<DrawContext *>(raw)->device;
  auto *t=static_cast<Texture8 *>(d.current.textures[stage].resource->api);
  try {return t->upload(0);}
  catch (...) {return std::numeric_limits<std::int32_t>::min();}
}
std::int32_t draw_texture_view(void *raw, std::uint32_t stage) {
  auto &c=*static_cast<DrawContext *>(raw);
  auto *t=static_cast<Texture8 *>(c.device->current.textures[stage].resource->api);
  try {return c.device->backend.texture_view(*t->gpu,&c.views[stage].view);}
  catch (...) {return std::numeric_limits<std::int32_t>::min();}
}
void draw_stage_finish(void *raw, std::uint32_t stage, std::uint8_t normal,
                       std::uint32_t lod) {
  auto &view=static_cast<DrawContext *>(raw)->views[stage];
  view.normal_xy=normal;view.minimum_lod=lod;
}
std::uint8_t draw_stage_view_present(void *raw,std::uint32_t stage) {
  return static_cast<DrawContext *>(raw)->views[stage].view!=nullptr;
}
std::int32_t draw_backend(void *raw) {
  auto &c=*static_cast<DrawContext *>(raw);
  try { return c.device->backend.draw(c.state,c.packet,c.views); }
  catch (...) { return std::numeric_limits<std::int32_t>::min(); }
}
void draw_success(void *raw, std::uint8_t compressed,
                  std::uint8_t user_vertices, std::uint8_t user_indices) {
  auto &d=*static_cast<DrawContext *>(raw)->device;
  ++d.draws;
  if (compressed) ++d.compressed_draws;
  if (user_vertices) {
    d.current.streams[0]=Binding{};
    d.current.strides[0]=0;
    if (user_indices) {
      d.current.indices=Binding{};
      d.current.base=0;
    }
  }
}
std::int32_t draw_fail(void *raw, std::uint32_t code, std::int32_t hr) {
  static constexpr const char *reasons[]{"",
      "draw outside scene or during state recording",
      "programmable pixel shader draw unsupported",
      "draw primitive count above bound",
      "DrawIndexedPrimitive index outside declared vertex range",
      "draw unsupported FVF",
      "vertex declaration stream/range invalid",
      "declaration-only fixed-function input unsupported",
      "FVF stream range/stride invalid",
      "SetTexture modern SRV binding",
      "translated D3D11 draw rejected; inspect pipeline reason"};
  try { return static_cast<DrawContext *>(raw)->device->fail(reasons[code],hr); }
  catch (...) { return E_OUTOFMEMORY; }
}
void mark_shader_constant(void *raw,std::uint32_t index) {
  auto *mask=static_cast<Device *>(raw)->mask();
  if (mask) mask->constants.set(index);
}
bool shader_diagnostic(Device *device) {
  std::uint32_t next=device->shader_diagnostics;
  const bool emit=a2fo_rs_shader_diagnostic(device->config.log!=nullptr,
      device->shader_diagnostics,&next)!=0;
  device->shader_diagnostics=next;
  return emit;
}
A2foRsDrawOps draw_ops(DrawContext *c) {
  return {c,draw_count,draw_region,draw_indices,draw_vertices,draw_index,draw_vertex,
      draw_has_shader,draw_shader_hlsl_empty,draw_shader_elements,draw_shader_element,
      draw_layout,draw_state,draw_stream,draw_stage,draw_compressed_view,draw_upload,
      draw_texture_view,draw_stage_finish,draw_stage_view_present,
      draw_backend,draw_success,draw_fail};
}
} // namespace
HRESULT Device::sample_views(std::array<Sampling, 8> &views) {
  try {
    DrawContext c(this);
    const HRESULT hr=a2fo_rs_draw_samples(draw_ops(&c),D3D_OK,E_OUTOFMEMORY);
    for (UINT i=0;i<8;++i) {
      views[i]=c.views[i];
      c.views[i].view=nullptr;
    }
    return hr;
  } catch (...) { return E_OUTOFMEMORY; }
}
HRESULT Device::draw(UINT type, UINT count, UINT first,
                     const void *user_vertices, UINT user_stride,
                     const void *user_indices, UINT index_format,
                     UINT min_index, UINT vertex_count, UINT base) {
  try {
    DrawContext c(this);
    c.packet.primitive=type;
    const A2foRsDrawInput input{type,count,first,user_stride,index_format,
        min_index,vertex_count,base,user_vertices,user_indices,
        static_cast<std::uint8_t>(in_scene),static_cast<std::uint8_t>(recording!=nullptr),
        static_cast<std::uint8_t>(current.pixel_shader!=0),
        D3DFMT_INDEX16,D3DFMT_INDEX32,sizeof(Vertex),MEM_COMMIT,PAGE_NOACCESS,PAGE_GUARD,
        invalid_call,E_OUTOFMEMORY,D3D_OK};
    return a2fo_rs_draw_run(input,draw_ops(&c));
  } catch (...) { return E_OUTOFMEMORY; }
}
HRESULT Device::DrawPrimitive(D3DPRIMITIVETYPE type, UINT first, UINT count) {
  return draw(type, count, first, nullptr, 0, nullptr, 0, 0, 0, 0);
}
HRESULT Device::DrawPrimitiveUP(D3DPRIMITIVETYPE type, UINT count,
                                const void *vertices, UINT stride) {
  return draw(type, count, 0, vertices, stride, nullptr, 0, 0, 0, 0);
}
HRESULT Device::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE type, UINT minimum,
                                       UINT vertices, UINT count,
                                       const void *indices, D3DFORMAT format,
                                       const void *data, UINT stride) {
  const HRESULT hr=a2fo_rs_shader_up_valid(indices!=nullptr,data!=nullptr,
      invalid_call,D3D_OK);
  if (FAILED(hr)) return hr;
  return draw(type,count,0,data,stride,indices,format,minimum,vertices,0);
}
HRESULT Device::DrawIndexedPrimitive(D3DPRIMITIVETYPE type, UINT minimum,
                                     UINT vertices, UINT start, UINT count) {
  auto *r=current.indices.resource;
  if (!r) return a2fo_rs_shader_up_valid(0,1,invalid_call,D3D_OK);
  auto *buffer=static_cast<IndexBuffer8 *>(r->api);
  const UINT size=a2fo_rs_shader_index_size(buffer->desc.Format,D3DFMT_INDEX16);
  UINT n=0;
  const HRESULT count_hr=compat::primitive_vertex_count(type,count,n);
  const HRESULT hr=a2fo_rs_shader_indexed_valid(1,buffer->locked,count_hr,
      start,n,size,buffer->bytes.size(),invalid_call,D3D_OK);
  if (FAILED(hr)) return hr;
  return draw(type,count,0,nullptr,0,
      buffer->bytes.data()+std::size_t(start)*size,buffer->desc.Format,
      minimum,vertices,current.base);
}
HRESULT Device::CreateVertexShader(const DWORD *declaration,
                                   const DWORD *function, DWORD *out, DWORD) {
  const HRESULT admission=a2fo_rs_shader_up_valid(out!=nullptr,
      declaration!=nullptr,invalid_call,D3D_OK);
  if (FAILED(admission)) return admission;
  *out = 0;
  try {
    auto shader = std::make_shared<Shader>();
    const auto declaration_status=shader_words(declaration,true,shader->declaration);
    if (declaration_status==2) return E_OUTOFMEMORY;
    if (declaration_status!=1)
      return fail("CreateVertexShader unsupported/incomplete declaration");
    std::uint32_t registers=0;
    const A2foRsDeclarationOps ops{shader.get(),append_shader_element};
    const auto declaration_plan=a2fo_rs_shader_declaration(
        reinterpret_cast<const std::uint32_t *>(shader->declaration.data()),
        shader->declaration.size(),ops,&registers);
    if (declaration_plan==1) return fail("tessellated vertex stream unsupported");
    if (declaration_plan==2)
      return fail("declaration constant/tessellation/extension token unsupported");
    if (declaration_plan==3) return invalid_call;
    if (declaration_plan==4) return E_OUTOFMEMORY;
    if (function) {
      const auto code_status=shader_words(function,false,shader->code);
      if (code_status==2) return E_OUTOFMEMORY;
      if (code_status!=1) {
        if(shader_diagnostic(this))
          config.log("[Shader] Rejected vertex bytecode prefix_words="+std::to_string(shader->code.size())+" declaration=["+diagnostic_words(shader->declaration)+"] function_prefix=["+diagnostic_words(shader->code)+"]");
        return fail("CreateVertexShader unsupported/incomplete bytecode");
      }
      shaders::Vs11Translation translated;
      std::string error;
      if (!shaders::translate_vs11(
              reinterpret_cast<const std::uint32_t *>(shader->code.data()),
              shader->code.size(), translated, error)) {
        if(shader_diagnostic(this))
          config.log("[Shader] Rejected complete vertex program declaration=["+diagnostic_words(shader->declaration)+"] function=["+diagnostic_words(shader->code)+"] reason="+error);
        return fail(error.c_str());
      }
      if (!a2fo_rs_shader_mask_valid(translated.input_mask,registers))
        return fail("vertex program reads undeclared inputs");
      shader->hlsl = std::move(translated.hlsl);
    }
    std::uint32_t next=0;
    DWORD token=a2fo_rs_shader_next_handle(next_shader,&next);
    next_shader=next;
    shaders.emplace(token, std::move(shader));
    *out = token;
    return D3D_OK;
  } catch (...) {
    return E_OUTOFMEMORY;
  }
}
// Native state storage and FVF decoder remain C++ primitives; Rust selects admission.
HRESULT Device::SetVertexShader(DWORD handle) {
  auto it=shaders.find(handle);
  HRESULT fvf_hr=D3D_OK;
  if (it==shaders.end()) {
    compat::FvfLayout layout;
    fvf_hr=compat::decode_fvf(handle,layout);
  }
  const HRESULT hr=a2fo_rs_shader_handle_valid(it!=shaders.end(),fvf_hr,
      invalid_call,D3D_OK);
  if (FAILED(hr)) return fail("SetVertexShader unknown handle/FVF");
  if (it==shaders.end()) setting().vertex_object.reset();
  else setting().vertex_object=it->second;
  setting().vertex_shader=handle;
  if (mask()) mask()->vertex_shader=true;
  return hr;
}
HRESULT Device::GetVertexShader(DWORD *out) {
  const HRESULT hr=a2fo_rs_shader_up_valid(out!=nullptr,1,invalid_call,D3D_OK);
  if (FAILED(hr)) return hr;
  *out=current.vertex_shader;return hr;
}
HRESULT Device::DeleteVertexShader(DWORD handle) {
  return a2fo_rs_shader_erase_result(shaders.erase(handle)!=0,invalid_call,D3D_OK);
}
HRESULT Device::SetVertexShaderConstant(DWORD first,const void *data,DWORD n) {
  const HRESULT hr=a2fo_rs_shader_constant_valid(first,data!=nullptr,n,
      invalid_call,D3D_OK);
  if (FAILED(hr)||!n) return hr;
  return a2fo_rs_shader_constant(first,static_cast<const std::uint8_t *>(data),
      n,reinterpret_cast<std::uint8_t *>(setting().gpu.constants[first]),
      this,mark_shader_constant,invalid_call,D3D_OK);
}
HRESULT Device::GetVertexShaderConstant(DWORD first,void *data,DWORD n) {
  const HRESULT hr=a2fo_rs_shader_constant_valid(first,data!=nullptr,n,
      invalid_call,D3D_OK);
  if (FAILED(hr)||!n) return hr;
  return a2fo_rs_shader_constant(first,
      reinterpret_cast<const std::uint8_t *>(current.gpu.constants[first]),n,
      static_cast<std::uint8_t *>(data),nullptr,nullptr,invalid_call,D3D_OK);
}
HRESULT Device::GetVertexShaderDeclaration(DWORD handle, void *out, DWORD *n) {
  auto it = shaders.find(handle);
  return it == shaders.end() ? invalid_call
                             : shader_blob(it->second->declaration, out, n);
}
HRESULT Device::GetVertexShaderFunction(DWORD handle, void *out, DWORD *n) {
  auto it = shaders.find(handle);
  return it == shaders.end() ? invalid_call
                             : shader_blob(it->second->code, out, n);
}
HRESULT Device::SetPixelShader(DWORD handle) {
  const HRESULT hr=a2fo_rs_shader_set_pixel(handle,invalid_call,D3D_OK);
  if (FAILED(hr)) return fail("SetPixelShader nonzero shader unsupported");
  setting().pixel_shader=0;
  if (mask()) mask()->pixel_shader=true;
  return hr;
}
HRESULT Device::GetPixelShader(DWORD *out) {
  const HRESULT hr=a2fo_rs_shader_up_valid(out!=nullptr,1,invalid_call,D3D_OK);
  if (FAILED(hr)) return hr;
  *out = current.pixel_shader;
  return hr;
}
} // namespace a2fo::renderer::game
