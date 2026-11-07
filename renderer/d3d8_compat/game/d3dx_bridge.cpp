// PE layout, WINAPI and COM adapters; bridge policy is Rust.
#include "factory.hpp"
#include "bc_catalog.hpp"
#include "../../../rust/d3dx_ffi.h"
#include <cstring>
#include <cstddef>
#include <exception>
namespace a2fo::renderer::game { namespace {
using Info=HRESULT(WINAPI*)(LPCSTR,void*);
using Create=HRESULT(WINAPI*)(IDirect3DDevice8*,LPCSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,DWORD,DWORD,D3DCOLOR,void*,PALETTEENTRY*,IDirect3DTexture8**);
struct Hook{void** info_slot{};void** create_slot{};Info info{};Create create{};Log log{};LONG info_reported{},create_reported{};bool complete{};};Hook hooks[8];
struct Context{Log log{};LPCSTR path{};void* info{};IDirect3DDevice8* device{};UINT width{},height{},mips{};DWORD usage{};D3DFORMAT format{};D3DPOOL pool{};DWORD filter{},mip_filter{};D3DCOLOR key{};PALETTEENTRY* palette{};IDirect3DTexture8** texture{};std::string transport,error;std::exception_ptr failure;};
void* callback(unsigned,bool);
uint8_t readable(void*,uintptr_t p,size_t n){MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return 0;const auto start=reinterpret_cast<uintptr_t>(m.BaseAddress);return p>=start&&p-start<=m.RegionSize&&n<=m.RegionSize-(p-start);}
void read(void*,uintptr_t p,void* out,size_t n){std::memcpy(out,reinterpret_cast<const void*>(p),n);}
uint8_t equals(void*,uintptr_t p,const uint8_t* text,size_t n,uint8_t insensitive){return insensitive?!_strnicmp(reinterpret_cast<const char*>(p),reinterpret_cast<const char*>(text),n):!std::memcmp(reinterpret_cast<const void*>(p),text,n);}
uintptr_t action(void* p,uint32_t op,uint32_t index,A2foRsD3dxData* d,const uint8_t* bytes,size_t n){auto& c=*static_cast<Context*>(p);if(op==17)return c.failure?1:0;if(c.failure)return 0;try{auto& h=hooks[index];switch(op){
case 0:d->words[0]=reinterpret_cast<uintptr_t>(h.info_slot);d->words[1]=reinterpret_cast<uintptr_t>(h.create_slot);d->values[0]=h.complete;d->values[1]=h.log!=nullptr;return 1;
case 1:h.info_slot=reinterpret_cast<void**>(d->words[0]);h.create_slot=reinterpret_cast<void**>(d->words[1]);std::memcpy(&h.info,h.info_slot,sizeof(h.info));std::memcpy(&h.create,h.create_slot,sizeof(h.create));h.log=c.log;return 1;
case 2:return reinterpret_cast<uintptr_t>(callback(index,d->values[0]!=0));
case 3:{auto* slot=reinterpret_cast<void**>(d->words[0]);DWORD old{};if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old))return 0;InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot),reinterpret_cast<void*>(d->words[1]));DWORD ignored{};return VirtualProtect(slot,sizeof(void*),old,&ignored)!=FALSE;}
case 4:h.complete=true;return 1;
case 5:if(h.log)h.log(std::string(reinterpret_cast<const char*>(bytes),n));return 1;
case 6:return static_cast<uintptr_t>(InterlockedIncrement(d->values[0]?&h.info_reported:&h.create_reported));
case 7:c.transport=bc_transport_path(c.path);d->text=reinterpret_cast<const uint8_t*>(c.transport.data());d->text_size=c.transport.size();return 1;
case 8:return static_cast<uint32_t>(h.info(d->values[0]?c.transport.c_str():c.path,c.info));
case 9:return is_bc_device(c.device);
case 10:{textures::CompressedImage image;bool normal=false;std::string asset;const bool ok=load_bc_asset(c.path,image,normal,asset,c.error);d->values[0]=image.width;d->values[1]=image.height;d->values[2]=static_cast<uint32_t>(image.mips.size());d->text=reinterpret_cast<const uint8_t*>(c.error.data());d->text_size=c.error.size();return ok;}
case 11:if(c.texture)*c.texture=nullptr;return 1;
case 12:return static_cast<uint32_t>(h.create(c.device,d->values[0]?c.transport.c_str():c.path,c.width,c.height,d->values[1],c.usage,c.format,c.pool,c.filter,c.mip_filter,c.key,c.info,c.palette,c.texture));
case 13:return c.texture!=nullptr;
case 14:return is_bc_texture(c.device,*c.texture);
case 15:if(c.texture&&*c.texture){(*c.texture)->Release();*c.texture=nullptr;}return 1;
case 16:{void* original{};std::memcpy(&original,&h.info,sizeof(original));return reinterpret_cast<uintptr_t>(original);}
case 17:return c.failure?1:0;
}}catch(...){c.failure=std::current_exception();}return 0;}
A2foRsD3dxOps bridge(Context& c){return {&c,readable,read,equals,action};}
template<unsigned N> HRESULT WINAPI image_info(LPCSTR path,void* info){try{Context c;c.path=path;c.info=info;const auto a=bridge(c);const auto hr=a2fo_rs_d3dx_info(&a,N);return c.failure?E_OUTOFMEMORY:hr;}catch(...){return E_OUTOFMEMORY;}}
template<unsigned N> HRESULT WINAPI create_texture(IDirect3DDevice8* device,LPCSTR path,UINT width,UINT height,UINT mips,DWORD usage,D3DFORMAT format,D3DPOOL pool,DWORD filter,DWORD mip_filter,D3DCOLOR key,void* info,PALETTEENTRY* palette,IDirect3DTexture8** texture){try{Context c;c.path=path;c.device=device;c.width=width;c.height=height;c.mips=mips;c.usage=usage;c.format=format;c.pool=pool;c.filter=filter;c.mip_filter=mip_filter;c.key=key;c.info=info;c.palette=palette;c.texture=texture;const auto a=bridge(c);A2foRsD3dxData d{};d.values[0]=width;d.values[1]=height;d.values[2]=mips;d.values[3]=usage;d.values[4]=static_cast<uint32_t>(format);d.values[5]=key;const auto hr=a2fo_rs_d3dx_create(&a,N,&d);return c.failure?E_OUTOFMEMORY:hr;}catch(...){return E_OUTOFMEMORY;}}
void* callback(unsigned index,bool info){const Info infos[]={image_info<0>,image_info<1>,image_info<2>,image_info<3>,image_info<4>,image_info<5>,image_info<6>,image_info<7>};const Create creates[]={create_texture<0>,create_texture<1>,create_texture<2>,create_texture<3>,create_texture<4>,create_texture<5>,create_texture<6>,create_texture<7>};void* p{};static_assert(sizeof(p)==sizeof(Info)&&sizeof(p)==sizeof(Create));if(info)std::memcpy(&p,&infos[index],sizeof(p));else std::memcpy(&p,&creates[index],sizeof(p));return p;}
}
bool install_d3dx_file_bridge(HMODULE module,Log log) noexcept {try{Context c;c.log=log;const auto a=bridge(c);const A2foRsD3dxLayout layout={sizeof(IMAGE_DOS_HEADER),sizeof(IMAGE_NT_HEADERS32),sizeof(IMAGE_IMPORT_DESCRIPTOR),offsetof(IMAGE_DOS_HEADER,e_lfanew),offsetof(IMAGE_NT_HEADERS32,Signature),offsetof(IMAGE_NT_HEADERS32,FileHeader.Machine),offsetof(IMAGE_NT_HEADERS32,OptionalHeader.Magic),offsetof(IMAGE_NT_HEADERS32,OptionalHeader.SizeOfImage),offsetof(IMAGE_NT_HEADERS32,OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress),offsetof(IMAGE_NT_HEADERS32,OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size),offsetof(IMAGE_IMPORT_DESCRIPTOR,Name),offsetof(IMAGE_IMPORT_DESCRIPTOR,OriginalFirstThunk),offsetof(IMAGE_IMPORT_DESCRIPTOR,FirstThunk)};return a2fo_rs_d3dx_install(&a,reinterpret_cast<uintptr_t>(module),&layout)!=0&&!c.failure;}catch(...){return false;}}
}
