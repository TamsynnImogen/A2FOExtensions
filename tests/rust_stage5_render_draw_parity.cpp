#ifdef A2FO_DRAW_FIXTURE
#include "../build/rust-migration-stage5/render-draw-work/mock_draw.hpp"
#include <cstdio>
using namespace a2fo::renderer::game;
namespace {
void row(const char *name, HRESULT hr, const Device &d) {
  std::printf("%s %08x draws=%llu compressed=%llu failures=%zu vertices=%zu indices=%zu\n",
      name, static_cast<unsigned>(hr), d.draws, d.compressed_draws,
      d.failures.size(), d.backend.last.vertices.size(), d.backend.last.indices.size());
  if (!d.failures.empty()) std::printf("failure %s\n", d.failures.back().c_str());
  if (!d.backend.last.vertices.empty()) {
    for (const auto &v : d.backend.last.vertices) {
      for (UINT r=0;r<16;++r) {
        for (float f : v.v[r]) {
          std::uint32_t bits=0;std::memcpy(&bits,&f,4);
          std::printf("%08x ",bits);
        }
      }
      std::putchar('\n');
    }
  }
  for (auto index : d.backend.last.indices) std::printf("index %u\n",index);
}
}
int main() {
  Device d;
  float xyz[3][3]={{1,2,3},{4,5,6},{7,8,9}};
  row("outside",d.DrawPrimitiveUP(4,1,xyz,12),d);
  d.in_scene=true;
  d.current.pixel_shader=1;
  row("pixel",d.DrawPrimitiveUP(4,1,xyz,12),d);
  d.current.pixel_shader=0;
  row("badtype",d.DrawPrimitiveUP(99,1,xyz,12),d);
  row("zero",d.DrawPrimitiveUP(4,0,xyz,12),d);
  row("up",d.DrawPrimitiveUP(4,1,xyz,12),d);
  row("up_unreadable",d.DrawPrimitiveUP(4,1,reinterpret_cast<void*>(0x10),12),d);
  std::uint16_t indices[3]={2,0,1};
  row("indexed_up",d.DrawIndexedPrimitiveUP(4,0,3,1,indices,D3DFMT_INDEX16,xyz,12),d);
  indices[0]=3;
  row("indexed_range",d.DrawIndexedPrimitiveUP(4,0,3,1,indices,D3DFMT_INDEX16,xyz,12),d);
  indices[0]=2;
  row("indexed_missing",d.DrawIndexedPrimitiveUP(4,0,3,1,nullptr,D3DFMT_INDEX16,xyz,12),d);
  row("indexed_format",d.DrawIndexedPrimitiveUP(4,0,3,1,indices,999,xyz,12),d);
  row("indexed_empty",d.DrawIndexedPrimitiveUP(4,0,0,1,indices,D3DFMT_INDEX16,xyz,12),d);
  row("indexed_overflow",d.DrawIndexedPrimitiveUP(4,UINT32_MAX,3,1,indices,D3DFMT_INDEX16,xyz,12),d);
  Resource stream_resource;VertexBuffer8 stream;
  stream.bytes.assign(reinterpret_cast<BYTE *>(xyz),reinterpret_cast<BYTE *>(xyz)+sizeof xyz);
  stream_resource.api=&stream;d.current.streams[0].resource=&stream_resource;d.current.strides[0]=12;
  row("buffer",d.DrawPrimitive(4,0,1),d);
  stream.locked=true;row("locked",d.DrawPrimitive(4,0,1),d);stream.locked=false;
  d.current.strides[0]=8;row("small_stride",d.DrawPrimitive(4,0,1),d);
  d.current.strides[0]=12;
  Resource index_resource;IndexBuffer8 index_buffer;
  index_buffer.bytes.assign(reinterpret_cast<BYTE *>(indices),reinterpret_cast<BYTE *>(indices)+sizeof indices);
  index_resource.api=&index_buffer;d.current.indices.resource=&index_resource;
  row("indexed_buffer",d.DrawIndexedPrimitive(4,0,3,0,1),d);
  row("indexed_short",d.DrawIndexedPrimitive(4,0,3,2,1),d);
  index_buffer.locked=true;row("indexed_locked",d.DrawIndexedPrimitive(4,0,3,0,1),d);
  index_buffer.locked=false;
  DWORD decl[]={0x40020000,0xffffffff};DWORD code[]={0xffffffff};DWORD handle=0;
  row("shader_create",d.CreateVertexShader(decl,code,&handle,0),d);
  std::printf("handle %08x\n",handle);
  row("shader_set",d.SetVertexShader(handle),d);
  row("shader_draw",d.DrawPrimitive(4,0,1),d);
  DWORD out=0;row("shader_get",d.GetVertexShader(&out),d);
  std::printf("shader_value %08x\n",out);
  DWORD n=0;row("shader_blob_size",d.GetVertexShaderDeclaration(handle,nullptr,&n),d);
  std::printf("shader_blob_n %u\n",n);
  DWORD blob[2]{};n=4;row("shader_blob_small",d.GetVertexShaderDeclaration(handle,blob,&n),d);
  n=sizeof blob;row("shader_blob",d.GetVertexShaderDeclaration(handle,blob,&n),d);
  std::printf("shader_blob %08x %08x %u\n",blob[0],blob[1],n);
  float constant[4]={1,2,3,4},copy[4]{};
  row("constant_set",d.SetVertexShaderConstant(0,constant,1),d);
  row("constant_get",d.GetVertexShaderConstant(0,copy,1),d);
  for (float f:copy){std::uint32_t bits;std::memcpy(&bits,&f,4);std::printf("constant %08x\n",bits);}
  row("constant_invalid",d.SetVertexShaderConstant(96,constant,1),d);
  row("pixel_invalid",d.SetPixelShader(1),d);
  row("pixel_zero",d.SetPixelShader(0),d);
  row("shader_delete",d.DeleteVertexShader(handle),d);
  row("shader_delete_missing",d.DeleteVertexShader(handle),d);
  row("shader_fvf",d.SetVertexShader(2),d);
  row("shader_unknown",d.SetVertexShader(999),d);
  row("bad_declaration",d.CreateVertexShader(nullptr,nullptr,&handle,0),d);
  row("unreadable_declaration",d.CreateVertexShader(reinterpret_cast<const DWORD*>(0x10),nullptr,&handle,0),d);
  DWORD tess[]={0x30000000,0xffffffff};
  row("shader_tess",d.CreateVertexShader(tess,nullptr,&handle,0),d);
  DWORD unsupported[]={0x60000000,0xffffffff};
  row("shader_unsupported",d.CreateVertexShader(unsupported,nullptr,&handle,0),d);
  DWORD duplicate[]={0x40020000,0x40020000,0xffffffff};
  row("shader_duplicate",d.CreateVertexShader(duplicate,nullptr,&handle,0),d);
  DWORD bad_stream[]={0x20000001,0x40020000,0xffffffff};
  row("shader_stream_create",d.CreateVertexShader(bad_stream,nullptr,&handle,0),d);
  row("shader_stream_set",d.SetVertexShader(handle),d);
  row("shader_stream_draw",d.DrawPrimitiveUP(4,1,xyz,12),d);
  DWORD color_decl[]={0x40020000,0x40040005,0xffffffff};
  row("color_shader_create",d.CreateVertexShader(color_decl,nullptr,&handle,0),d);
  row("color_shader_set",d.SetVertexShader(handle),d);
  struct Colored {float xyz[3];DWORD color;};
  Colored colored[3]={{{1,2,3},0x11223344},{{4,5,6},0x55667788},{{7,8,9},0x99aabbcc}};
  row("color_shader_draw",d.DrawPrimitiveUP(4,1,colored,sizeof(Colored)),d);
  row("shader_fvf_restore",d.SetVertexShader(2),d);
  d.backend.result=invalid_call;
  row("backend_fail",d.DrawPrimitiveUP(4,1,xyz,12),d);
  d.backend.result=D3D_OK;
  Texture8 texture;texture.compressed=std::make_unique<Compressed>();texture.normal_xy=true;
  Resource texture_resource;texture_resource.api=&texture;
  d.current.textures[0].resource=&texture_resource;
  row("compressed_draw",d.DrawPrimitiveUP(4,1,xyz,12),d);
  texture.normal_xy=false;
  row("compressed_no_xy",d.DrawPrimitiveUP(4,1,xyz,12),d);
  d.current.textures[0].resource=nullptr;
  std::uint32_t seed=0x12345678;
  for (unsigned sample=0;sample<32;++sample) {
    for (auto &vertex : xyz)
      for (float &component : vertex) {
        seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
        component=static_cast<float>(static_cast<std::int32_t>(seed%20001)-10000)/100.f;
      }
    row("seeded_up",d.DrawPrimitiveUP(4,1,xyz,12),d);
  }
}
#else
#include "../build/rust-migration-stage5/render-draw-work/mock_com.hpp"
#include <cstdio>
#include <utility>
using namespace a2fo::renderer;
namespace {
struct Owner final : IDirect3DDevice8 {
  ULONG refs = 1;
  HRESULT QueryInterface(REFIID, void **out) override { if (out) *out = nullptr; return E_NOINTERFACE; }
  ULONG AddRef() override { return ++refs; }
  ULONG Release() override { return --refs; }
};
void row(const char *label, HRESULT hr, ULONG a = 0, ULONG b = 0) {
  std::printf("%s %08x %u %u\n", label, static_cast<unsigned>(hr), a, b);
}
textures::CompressedImage image(DXGI_FORMAT format, bool data = true) {
  textures::CompressedImage x;
  x.format = format;
  x.mips.push_back({8, 8, 16, 64, 0});
  x.mips.push_back({4, 4, 16, 16, 64});
  if (data) {
    x.bytes.resize(80);
    for (std::size_t i=0;i<x.bytes.size();++i) x.bytes[i]=static_cast<BYTE>(i);
  }
  return x;
}
}
int main() {
  Owner owner;
  std::string error;
  IDirect3DTexture8 *texture = nullptr;
  for (unsigned i = 0; i < 4; ++i) {
    auto x = image(i == 3 ? DXGI_FORMAT_BC5_UNORM : DXGI_FORMAT_BC1_UNORM, i != 2);
    auto mode = i == 1 || i == 3 ? compat::TextureInterpretation::normal_xy : compat::TextureInterpretation::raw;
    auto hr = compat::create_compressed_texture8(i == 0 ? nullptr : &owner,
        reinterpret_cast<ID3D11Device*>(0x1), std::move(x), mode, &texture, error);
    row("create", hr, texture != nullptr, owner.refs);
    std::printf("error %s\n", error.c_str()); error.clear();
    if (texture) {
      auto count=texture->Release();texture=nullptr;
      row("create_release",count,owner.refs);
    }
  }
  auto x = image(DXGI_FORMAT_BC5_SNORM);
  auto create_hr = compat::create_compressed_texture8(&owner,
      reinterpret_cast<ID3D11Device*>(0x1), std::move(x), compat::TextureInterpretation::normal_xy,
      &texture, error);
  row("create_ok", create_hr, texture != nullptr, owner.refs);
  if (!texture) return 1;
  void *binding_pointer=nullptr;
  auto hr=texture->QueryInterface(IID_CompressedTextureBinding,&binding_pointer);
  row("binding_query",hr,binding_pointer!=nullptr);
  if (binding_pointer) {
    auto *binding=static_cast<compat::CompressedTextureBinding*>(binding_pointer);
    compat::CompressedBinding info{};
    ID3D11ShaderResourceView *view=nullptr;
    hr=binding->GetBinding(&view,&info);
    row("binding_view",hr,view!=nullptr,info.minimum_lod);
    row("binding_info",info.format,static_cast<ULONG>(info.interpretation));
    hr=binding->GetBinding(nullptr,&info);row("binding_null_view",hr);
    hr=binding->GetBinding(&view,nullptr);row("binding_null_info",hr);
    IDirect3DTexture8 *again=nullptr;
    hr=binding->GetTexture(&again);row("binding_texture",hr,again!=nullptr);
    if (again) {auto count=again->Release();row("binding_texture_release",count);}
    row("binding_retain",binding->RetainBinding());
    row("binding_drop",binding->ReleaseBinding());
    row("binding_release",binding->Release());
  }
  const GUID guids[] = {IID_IUnknown, IID_IDirect3DResource8,
      IID_IDirect3DBaseTexture8, IID_IDirect3DTexture8,
      IID_CompressedTextureBinding, IID_IDirect3DSurface8, IID_Unknown};
  for (const auto &guid : guids) {
    void *out = reinterpret_cast<void*>(0x1);
    auto hr = texture->QueryInterface(guid, &out);
    row("query", hr, out != nullptr);
    if (SUCCEEDED(hr)) {
      if (IsEqualGUID(guid, IID_CompressedTextureBinding))
        static_cast<compat::CompressedTextureBinding*>(out)->Release();
      else static_cast<IDirect3DTexture8*>(out)->Release();
    }
  }
  row("query_null", texture->QueryInterface(IID_IUnknown, nullptr));
  auto old = texture->SetPriority(19); row("priority", old, texture->GetPriority());
  old = texture->SetLOD(50); row("lod", old, texture->GetLOD());
  old = texture->SetLOD(0); row("lod2", old, texture->GetLOD());
  row("levels", texture->GetLevelCount());
  for (UINT level = 0; level < 3; ++level) {
    D3DSURFACE_DESC desc{};
    auto hr = texture->GetLevelDesc(level, &desc);
    row("desc", hr, desc.Width, desc.Height);
  }
  GUID private_guid{42, 0, 0, {}};
  DWORD bytes = 3; BYTE sample[] = {1, 2, 3}; BYTE output[4]{};
  row("set_private", texture->SetPrivateData(private_guid, sample, 3, 0));
  hr = texture->GetPrivateData(private_guid, output, &bytes);
  row("get_small", hr, bytes);
  bytes = sizeof output;
  hr = texture->GetPrivateData(private_guid, output, &bytes);
  row("get_private", hr, bytes, output[0]);
  row("erase_private", texture->FreePrivateData(private_guid));
  row("erase_missing", texture->FreePrivateData(private_guid));
  hr=texture->SetPrivateData(private_guid,&owner,sizeof(IUnknown*),D3DSPD_IUNKNOWN);
  row("set_private_object",hr,owner.refs);
  IUnknown *returned=nullptr;bytes=sizeof returned;
  hr=texture->GetPrivateData(private_guid,&returned,&bytes);
  row("get_private_object",hr,bytes,owner.refs);
  if (returned) {auto count=returned->Release();row("release_private_object",count,owner.refs);}
  hr=texture->FreePrivateData(private_guid);row("erase_private_object",hr,owner.refs);
  row("desc_null",texture->GetLevelDesc(0,nullptr));
  row("surface_null",texture->GetSurfaceLevel(0,nullptr));
  D3DLOCKED_RECT locked{};
  row("lock_badflags", texture->LockRect(0, &locked, nullptr, 0));
  hr = texture->LockRect(0, &locked, nullptr, D3DLOCK_READONLY);
  row("lock_ok", hr, locked.Pitch,locked.pBits?*static_cast<BYTE*>(locked.pBits):0);
  row("lock_twice", texture->LockRect(0, &locked, nullptr, D3DLOCK_READONLY));
  row("unlock", texture->UnlockRect(0));
  row("unlock_twice", texture->UnlockRect(0));
  RECT region{4, 4, 8, 8};
  hr = texture->LockRect(0, &locked, &region, D3DLOCK_READONLY);
  row("lock_region", hr, locked.Pitch,locked.pBits?*static_cast<BYTE*>(locked.pBits):0);
  row("unlock_region", texture->UnlockRect(0));
  RECT bad_region{1,0,8,8};
  row("lock_unaligned",texture->LockRect(0,&locked,&bad_region,D3DLOCK_READONLY));
  row("lock_badlevel",texture->LockRect(3,&locked,nullptr,D3DLOCK_READONLY));
  row("unlock_badlevel",texture->UnlockRect(3));
  IDirect3DSurface8 *surface = nullptr;
  hr = texture->GetSurfaceLevel(1, &surface);
  row("surface", hr, surface != nullptr);
  if (surface) {
    void *out = nullptr;
    hr = surface->QueryInterface(IID_IDirect3DSurface8, &out);
    row("surface_query", hr, out != nullptr);
    if (out) static_cast<IDirect3DSurface8*>(out)->Release();
    row("surface_query_wrong",surface->QueryInterface(IID_IDirect3DTexture8,&out));
    D3DSURFACE_DESC desc{};
    hr=surface->GetDesc(&desc);row("surface_desc",hr,desc.Width,desc.Height);
    hr=surface->LockRect(&locked,nullptr,D3DLOCK_READONLY);
    row("surface_lock",hr,locked.Pitch);
    row("surface_unlock",surface->UnlockRect());
    row("surface_release", surface->Release());
  }
  auto remaining = texture->Release(); row("texture_release", remaining, owner.refs);
}
#endif
