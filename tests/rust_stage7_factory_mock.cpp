// Synthetic PE32 and COM provider; no graphics device or window is created.
#include "../renderer/d3d8_compat/game/private.hpp"
#include <cstdio>
#include <cstring>
#include <string>
using namespace a2fo::renderer::game;
namespace a2fo::renderer::game { bool install_file_bridge(HMODULE,Log) noexcept;
extern HRESULT factory_initialize_result; extern bool factory_catalog_result;
extern bool factory_asset_error; extern bool factory_asset_match;
extern bool factory_texture_create_fail; extern HRESULT factory_upload_result; }
struct MockNative8 final : IDirect3D8 {
  ULONG refs=1;unsigned format_calls=0,type_calls=0;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void **out) override {*out=this;AddRef();return S_OK;}
  ULONG STDMETHODCALLTYPE AddRef() override {return ++refs;}
  ULONG STDMETHODCALLTYPE Release() override {return --refs;}
  HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void*) override {return E_FAIL;}
  UINT STDMETHODCALLTYPE GetAdapterCount() override {return 1;}
  HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT,DWORD,D3DADAPTER_IDENTIFIER8*) override {return E_FAIL;}
  UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT) override {return 0;}
  HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT,UINT,D3DDISPLAYMODE*) override {return E_FAIL;}
  HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT,D3DDISPLAYMODE*) override {return E_FAIL;}
  HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT,D3DDEVTYPE,D3DFORMAT,D3DFORMAT,WINBOOL) override {++type_calls;return E_FAIL;}
  HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT,D3DDEVTYPE,D3DFORMAT,DWORD,D3DRESOURCETYPE,D3DFORMAT) override {++format_calls;return E_FAIL;}
  HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT,D3DDEVTYPE,D3DFORMAT,WINBOOL,D3DMULTISAMPLE_TYPE) override {return E_FAIL;}
  HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(UINT,D3DDEVTYPE,D3DFORMAT,D3DFORMAT,D3DFORMAT) override {return E_FAIL;}
  HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT,D3DDEVTYPE,D3DCAPS8 *out) override {
    std::memset(out,0,sizeof(*out));out->MaxTextureWidth=32768;out->MaxTextureHeight=32768;
    out->TextureCaps=0xffffffff;out->DevCaps=0xffffffff;return S_OK;
  }
  HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT) override {return nullptr;}
  HRESULT STDMETHODCALLTYPE CreateDevice(UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice8**) override {return E_FAIL;}
};
static void report(const char *name,HRESULT hr,MockNative8 &native) {
  std::printf("%s|%08x|native=%u,%u|refs=%u\n",name,unsigned(hr),native.format_calls,native.type_calls,native.refs);
}
int main() {
  MockNative8 native;
  auto *factory=new Factory(&native,{});
  D3DCAPS8 caps{};
  auto hr=factory->GetDeviceCaps(0,D3DDEVTYPE_HAL,&caps);
  report("caps",hr,native);
  std::printf("caps-data|%u|%u|%u|%u|%08x|%08x\n",caps.MaxTextureWidth,caps.MaxTextureHeight,
              caps.MaxTextureBlendStages,caps.VertexShaderVersion,unsigned(caps.TextureCaps),unsigned(caps.DevCaps));
  for(auto format:{D3DFMT_A8R8G8B8,D3DFMT_DXT1,D3DFMT_P8,D3DFMT_D16,D3DFMT_UNKNOWN}) {
    hr=factory->CheckDeviceFormat(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,0,D3DRTYPE_TEXTURE,format);
    report("texture-format",hr,native);
    hr=factory->CheckDeviceFormat(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,D3DUSAGE_RENDERTARGET,D3DRTYPE_TEXTURE,format);
    report("target-format",hr,native);
    hr=factory->CheckDeviceFormat(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,D3DUSAGE_DEPTHSTENCIL,D3DRTYPE_SURFACE,format);
    report("depth-format",hr,native);
  }
  hr=factory->CheckDeviceFormat(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,0,D3DRTYPE_VERTEXBUFFER,D3DFMT_UNKNOWN);
  report("native-format",hr,native);
  for(auto back:{D3DFMT_UNKNOWN,D3DFMT_A8R8G8B8,D3DFMT_P8}) {
    hr=factory->CheckDeviceType(0,D3DDEVTYPE_HAL,D3DFMT_A8R8G8B8,back,TRUE);
    report("device-type",hr,native);
  }
  for(auto depth:{D3DFMT_D16,D3DFMT_D24S8,D3DFMT_UNKNOWN}) {
    hr=factory->CheckDepthStencilMatch(0,D3DDEVTYPE_HAL,D3DFMT_A8R8G8B8,D3DFMT_A8R8G8B8,depth);
    report("depth-match",hr,native);
  }
  D3DPRESENT_PARAMETERS pp{};
  pp.BackBufferWidth=8;pp.BackBufferHeight=8;
  pp.BackBufferFormat=D3DFMT_UNKNOWN;
  IDirect3DDevice8 *device=nullptr;
  hr=factory->CreateDevice(1,D3DDEVTYPE_HAL,nullptr,0,&pp,&device);
  std::printf("create-admission|%08x|%u\n",unsigned(hr),unsigned(device!=nullptr));
  hr=factory->CreateDevice(0,D3DDEVTYPE_HAL,nullptr,0,&pp,&device);
  std::printf("create-initialize-fail|%08x|%u\n",unsigned(hr),unsigned(device!=nullptr));
  factory_initialize_result=D3D_OK;
  hr=factory->CreateDevice(0,D3DDEVTYPE_HAL,nullptr,0,&pp,&device);
  std::printf("create-success|%08x|%u|%u\n",unsigned(hr),unsigned(device!=nullptr),unsigned(pp.BackBufferFormat));
  IDirect3DTexture8 *tex=nullptr;
  hr=load_texture_file(device,"mock",&tex);
  std::printf("load-miss|%08x|%u\n",unsigned(hr),unsigned(tex!=nullptr));
  factory_asset_error=true;
  hr=load_texture_file(device,"mock",&tex);
  std::printf("load-catalog-error|%08x|%u\n",unsigned(hr),unsigned(tex!=nullptr));
  factory_asset_error=false;
  factory_asset_match=true;
  factory_texture_create_fail=true;
  hr=load_texture_file(device,"mock",&tex);
  std::printf("load-create-fail|%08x|%u\n",unsigned(hr),unsigned(tex!=nullptr));
  factory_texture_create_fail=false;
  hr=load_texture_file(device,"mock",&tex);
  std::printf("load-upload-fail|%08x|%u\n",unsigned(hr),unsigned(tex!=nullptr));
  factory_upload_result=D3D_OK;
  hr=load_texture_file(device,"mock",&tex);
  std::printf("load-success|%08x|%u\n",unsigned(hr),unsigned(tex!=nullptr));
  if(tex)tex->Release();
  if(device)device->Release();
  Configuration bc{};bc.bc_assets=true;
  factory_catalog_result=false;
  auto *none=create_factory(&native,bc);
  std::printf("factory-catalog-fail|%u|%u\n",unsigned(none!=nullptr),native.refs);
  factory_catalog_result=true;
  auto *yes=create_factory(&native,bc);
  std::printf("factory-catalog-success|%u|%u\n",unsigned(yes!=nullptr),native.refs);
  if(yes)yes->Release();
  auto *image=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
  if(!image)return 31;
  auto *dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=128;
  auto *nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(image+128);nt->Signature=IMAGE_NT_SIGNATURE;
  nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
  nt->OptionalHeader.SizeOfImage=4096;nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]={512,40};
  auto *imports=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image+512);
  imports[0].Name=600;imports[0].OriginalFirstThunk=640;imports[0].FirstThunk=680;
  auto *name=reinterpret_cast<DWORD*>(image+640);*name=720;
  auto *slot=reinterpret_cast<void**>(image+680);*slot=reinterpret_cast<void*>(&std::fopen);
  std::memcpy(image+722,"fopen",6);
  auto *original=*slot;
  dos->e_magic=0;
  auto installed=install_file_bridge(reinterpret_cast<HMODULE>(image),nullptr);
  std::printf("bridge-bad-dos|%u\n",unsigned(installed));
  dos->e_magic=IMAGE_DOS_SIGNATURE;
  nt->Signature=0;
  installed=install_file_bridge(reinterpret_cast<HMODULE>(image),nullptr);
  std::printf("bridge-bad-nt|%u\n",unsigned(installed));
  nt->Signature=IMAGE_NT_SIGNATURE;
  *name=IMAGE_ORDINAL_FLAG32;
  installed=install_file_bridge(reinterpret_cast<HMODULE>(image),nullptr);
  std::printf("bridge-ordinal|%u\n",unsigned(installed));
  *name=720;
  std::memcpy(image+722,"other",6);
  installed=install_file_bridge(reinterpret_cast<HMODULE>(image),nullptr);
  std::printf("bridge-other|%u\n",unsigned(installed));
  std::memcpy(image+722,"fopen",6);
  *slot=nullptr;
  installed=install_file_bridge(reinterpret_cast<HMODULE>(image),nullptr);
  std::printf("bridge-null-slot|%u\n",unsigned(installed));
  *slot=original;
  installed=install_file_bridge(reinterpret_cast<HMODULE>(image),nullptr);
  std::printf("bridge-first|%u|%u\n",unsigned(installed),unsigned(*slot!=original));
  installed=install_file_bridge(reinterpret_cast<HMODULE>(image),nullptr);
  std::printf("bridge-repeat|%u|%u\n",unsigned(installed),unsigned(*slot!=original));
  *slot=original;
  installed=install_file_bridge(reinterpret_cast<HMODULE>(image),nullptr);
  std::printf("bridge-wrong-duplicate|%u\n",unsigned(installed));
  dos->e_magic=0;
  installed=install_file_bridge(reinterpret_cast<HMODULE>(image),nullptr);
  std::printf("bridge-bad-magic|%u\n",unsigned(installed));
  VirtualFree(image,0,MEM_RELEASE);
  factory->Release();
  std::printf("native-end|%u\n",native.refs);
}
