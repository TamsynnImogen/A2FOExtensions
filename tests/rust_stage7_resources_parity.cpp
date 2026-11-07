#include "../renderer/d3d8_compat/game/private.hpp"
#include <cstdio>
#include <cstdint>
#ifdef RESOURCE_RUST
#include "../rust/game_resources_ffi.h"
#endif
using namespace a2fo::renderer::game;
struct MockUnknown final : IUnknown {
  ULONG refs = 1;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **out) override { *out = this; ++refs; return S_OK; }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
  ULONG STDMETHODCALLTYPE Release() override { return --refs; }
};
int main() {
  constexpr D3DFORMAT formats[] = {D3DFMT_DXT1,D3DFMT_DXT2,D3DFMT_DXT3,D3DFMT_DXT4,D3DFMT_DXT5,D3DFMT_A8R8G8B8,D3DFMT_R8G8B8,D3DFMT_R5G6B5,D3DFMT_A8L8,D3DFMT_L8,D3DFMT_P8,D3DFMT_UNKNOWN};
  for (auto f : formats) for (UINT w : {0u,1u,3u,4u,5u,17u,1024u}) std::printf("pitch:%u:%u:%u:%u\n", unsigned(f),w,pixel_pitch(f,w),pixel_rows(f,w));
  for (bool locked : {false,true}) for (UINT off : {0u,3u,8u,9u}) for (UINT size_value : {0u,1u,6u}) for (DWORD flags : {DWORD(0),DWORD(D3DLOCK_READONLY),DWORD(D3DLOCK_READONLY|D3DLOCK_DISCARD),DWORD(0x40000000)}) {
    const bool original = !locked && off <= 8 && (!size_value || size_value <= 8-off) &&
      !(flags & ~DWORD(D3DLOCK_READONLY|D3DLOCK_DISCARD|D3DLOCK_NOOVERWRITE|D3DLOCK_NOSYSLOCK)) &&
      !((flags&D3DLOCK_READONLY) && (flags&(D3DLOCK_DISCARD|D3DLOCK_NOOVERWRITE)));
#ifdef RESOURCE_RUST
    const bool value = a2fo_resource_buffer_lock(true,locked,8,off,size_value,flags);
#else
    const bool value = original;
#endif
    if (value != original) return 41;
    std::printf("buffer:%u:%u:%u:%u:%u\n",unsigned(locked),off,size_value,unsigned(flags),unsigned(value));
  }
  for (auto f : {D3DFMT_DXT1,D3DFMT_A8R8G8B8}) for (LONG left : {0L,1L,4L}) for (LONG right : {4L,5L,8L}) for (DWORD flags : {DWORD(0),DWORD(D3DLOCK_READONLY|D3DLOCK_DISCARD)}) {
    const bool block = f==D3DFMT_DXT1;
    const bool original = right>left && right<=8 && !(flags&D3DLOCK_READONLY && flags&D3DLOCK_DISCARD) &&
      (!block || (left%4==0 && (right%4==0 || right==8)));
#ifdef RESOURCE_RUST
    std::size_t offset=0;
    const bool value=a2fo_resource_rect_lock(true,false,false,flags,f,8,8,pixel_pitch(f,8),left,0,right,8,&offset);
#else
    const bool value=original;
#endif
    if(value!=original)return 42;
    std::printf("rect:%u:%ld:%ld:%u:%u\n",unsigned(f),long(left),long(right),unsigned(flags),unsigned(value));
  }
  for (UINT w : {0u,1u,2u,3u,16u,256u}) for (UINT h : {0u,1u,2u,7u,129u}) {
    UINT x=w,y=h,levels=1;
    while(x>1||y>1){x=std::max(1u,x/2);y=std::max(1u,y/2);++levels;}
#ifdef RESOURCE_RUST
    const UINT value=a2fo_resource_levels(w,h);
#else
    const UINT value=levels;
#endif
    if(value!=levels)return 43;
    std::printf("mips:%u:%u:%u\n",w,h,value);
  }
  for (bool compressed : {false,true}) for (bool dxt4 : {false,true}) for (UINT level : {0u,1u}) for (bool gpu : {false,true}) {
    const UINT original=!compressed&&dxt4&&level==0?1:compressed?2:!gpu?3:4;
#ifdef RESOURCE_RUST
    const UINT value=a2fo_resource_upload_plan(compressed,dxt4,level,gpu);
#else
    const UINT value=original;
#endif
    if(value!=original)return 44;
    std::printf("upload:%u:%u:%u:%u:%u\n",unsigned(compressed),unsigned(dxt4),level,unsigned(gpu),value);
  }
  const BYTE rgba[]={1,2,3,4,5,6,7,8}; BYTE bgra[8]{};
#ifdef RESOURCE_RUST
  a2fo_resource_swizzle_bgra(bgra,rgba,sizeof(rgba));
#else
  for(std::size_t i=0;i<sizeof(rgba);i+=4){bgra[i]=rgba[i+2];bgra[i+1]=rgba[i+1];bgra[i+2]=rgba[i];bgra[i+3]=rgba[i+3];}
#endif
  for(auto value:bgra)std::printf("swizzle:%u\n",unsigned(value));
  for (std::size_t len : {std::size_t(0),std::size_t(1),std::size_t(8)}) {
    const BYTE input[]={0,1,2,3,4,5,6,7};
    std::uint64_t original=14695981039346656037ull;
    for(std::size_t i=0;i<len;++i)original=(original^input[i])*1099511628211ull;
#ifdef RESOURCE_RUST
    const auto value=a2fo_resource_hash(input,len);
#else
    const auto value=original;
#endif
    if(value!=original)return 45;
    std::printf("hash:%u:%llu\n",unsigned(len),static_cast<unsigned long long>(value));
  }
  for (bool target : {false,true}) for (bool locked : {false,true}) for (bool region : {false,true}) for (DWORD flags : {DWORD(0),DWORD(D3DLOCK_READONLY)}) {
    const bool original=!locked&&(!target||(flags&D3DLOCK_READONLY)&&!region);
#ifdef RESOURCE_RUST
    const bool value=a2fo_resource_surface_lock(true,locked,target,region,flags);
#else
    const bool value=original;
#endif
    if(value!=original)return 46;
    std::printf("surface:%u:%u:%u:%u:%u\n",unsigned(target),unsigned(locked),unsigned(region),unsigned(flags),unsigned(value));
  }
  for (UINT n : {0u,1u,7u,99u}) {
    const UINT original=std::min<UINT>(n,3);
#ifdef RESOURCE_RUST
    const UINT value=a2fo_resource_lod(n,4);
#else
    const UINT value=original;
#endif
    if(value!=original)return 47;
    std::printf("lod:%u:%u\n",n,value);
  }
  PrivateData data;
  GUID guid{};
  BYTE source[] = {3,7,11};
  std::printf("null:%08x\n",unsigned(data.set(guid,nullptr,3,0)));
  std::printf("flags:%08x\n",unsigned(data.set(guid,source,3,2)));
  std::printf("set:%08x\n",unsigned(data.set(guid,source,3,0)));
  DWORD size=0;
  auto size_hr=data.get(guid,nullptr,&size);
  std::printf("size:%08x:%u\n",unsigned(size_hr),size);
  BYTE dest[8]{};size=1;
  auto hr=data.get(guid,dest,&size);
  std::printf("short:%08x:%u\n",unsigned(hr),size);
  size=8;hr=data.get(guid,dest,&size);
  std::printf("get:%08x:%u:%u:%u:%u\n",unsigned(hr),size,dest[0],dest[1],dest[2]);
  MockUnknown object;
  auto object_hr=data.set(guid,&object,sizeof(IUnknown *),D3DSPD_IUNKNOWN);
  std::printf("object-set:%08x:%u\n",unsigned(object_hr),unsigned(object.refs));
  size=sizeof(IUnknown *);IUnknown *retrieved=nullptr;
  hr=data.get(guid,&retrieved,&size);
  std::printf("object-get:%08x:%u:%u:%u\n",unsigned(hr),size,unsigned(object.refs),unsigned(retrieved==&object));
  retrieved->Release();
  auto erase_hr=data.erase(guid);
  std::printf("erase:%08x:%u\n",unsigned(erase_hr),unsigned(object.refs));
  std::printf("missing:%08x\n",unsigned(data.erase(guid)));
}
