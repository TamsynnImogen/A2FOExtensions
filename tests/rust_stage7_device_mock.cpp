// Real Device methods with a C++ COM parent and headless backend stubs.
#include "../build/rust-migration-stage7/device-work/native8.hpp"
#include <cstdio>
#include <cstdint>
using namespace a2fo::renderer::game;
static void line(const char *name,HRESULT hr,Device *d) {
  std::printf("%s|%08x|resources=%u|refs=%ld|state=%u,%u\n",name,unsigned(hr),
      unsigned(d->resources.size()),long(d->refs),unsigned(d->in_scene),
      unsigned(d->current.gpu.render[D3DRS_ZENABLE]));
}
int main() {
  MockNative8 native;
  auto *factory=new Factory(&native,{});
  D3DPRESENT_PARAMETERS p{};p.BackBufferWidth=8;p.BackBufferHeight=8;
  p.BackBufferFormat=D3DFMT_A8R8G8B8;
  auto *d=new Device(factory,0,D3DDEVTYPE_HAL,nullptr,0,p);
  factory->Release();
  line("defaults",D3D_OK,d);
  std::uint64_t defaults_hash=14695981039346656037ull;
  auto add=[&](std::uint32_t value){for(unsigned i=0;i<4;++i){defaults_hash=(defaults_hash^((value>>(i*8))&255))*1099511628211ull;}};
  for(auto value:d->current.gpu.render)add(value);
  for(const auto &stage:d->current.gpu.stage)for(auto value:stage)add(value);
  for(float value:d->current.gpu.world.m){std::uint32_t bits;std::memcpy(&bits,&value,4);add(bits);}
  std::printf("defaults-hash|%016llx\n",static_cast<unsigned long long>(defaults_hash));
  for(unsigned i=0;i<256;++i)if(d->current.gpu.render[i])std::printf("render|%u|%lu\n",i,static_cast<unsigned long>(d->current.gpu.render[i]));
  for(unsigned i=0;i<8;++i)for(unsigned j=0;j<33;++j)if(d->current.gpu.stage[i][j])std::printf("stage|%u|%u|%lu\n",i,j,static_cast<unsigned long>(d->current.gpu.stage[i][j]));

  D3DVIEWPORT8 vp{0,0,8,8,0,1};
  line("viewport",d->SetViewport(&vp),d);
  vp.MinZ=-1;line("viewport-bad",d->SetViewport(&vp),d);
  IDirect3DTexture8 *tex=nullptr;
  line("texture-bad",d->CreateTexture(0,8,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&tex),d);
  line("texture",d->CreateTexture(8,8,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&tex),d);
  if(tex)tex->Release();
  IDirect3DVertexBuffer8 *vb=nullptr;
  line("buffer",d->CreateVertexBuffer(8,0,0,D3DPOOL_DEFAULT,&vb),d);
  D3DPRESENT_PARAMETERS next=p;
  line("reset-blocked",d->Reset(&next),d);
  if(vb)vb->Release();
  line("reset-resize-fail",d->Reset(&next),d);
  line("reset-invalid",d->Reset(nullptr),d);
  D3DMATRIX matrix{};matrix._11=1;matrix._22=1;matrix._33=1;matrix._44=1;
  line("matrix-set",d->SetTransform(D3DTS_WORLD,&matrix),d);
  line("matrix-multiply",d->MultiplyTransform(D3DTS_WORLD,&matrix),d);
  DWORD passes=0;line("validate",d->ValidateDevice(&passes),d);
  std::printf("passes|%lu\n",static_cast<unsigned long>(passes));
  d->Release();
  std::printf("end|native_refs=%u\n",native.refs);
}
