#include "compressed_texture.hpp"
#include "texture_handle.hpp"
#include "probe_bridge.hpp"
#include "probe_device.hpp"
#include "ui_renderer.hpp"
#include <d3d11.h>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> trace;
std::string failing;
bool debug_missing=false;
UINT feature=D3D_FEATURE_LEVEL_11_0,support=D3D11_FORMAT_SUPPORT_TEXTURE2D|
    D3D11_FORMAT_SUPPORT_SHADER_SAMPLE|D3D11_FORMAT_SUPPORT_MIP;
int directory_count=0;
DWORD last_error=5;
int tick=0;
ID3D11Device device;
ID3D11DeviceContext context;
IDXGISwapChain swap;
ID3D11Texture2D texture,buffer;
ID3D11ShaderResourceView view;
ID3D11RenderTargetView rtv;
ID3D11DepthStencilView dsv;
IDXGIDevice dxgi;
IDXGIAdapter adapter;
IDXGIFactory factory;
void add(const std::string& line){trace.push_back(line);}
HRESULT status(const char* name){return failing==name?E_FAIL:S_OK;}
void flush(const std::string& name,HRESULT hr=0,const std::string& error={}){
 std::cout<<name<<"|"<<std::hex<<static_cast<std::uint32_t>(hr)<<std::dec<<"|"<<error;
 for(const auto& x:trace)std::cout<<"|"<<x;
 std::cout<<'\n';trace.clear();
}
void log_line(const std::string& line){add("LOG:"+line);}
void reset(const std::string& fail={}){trace.clear();failing=fail;debug_missing=false;
 feature=D3D_FEATURE_LEVEL_11_0;support=D3D11_FORMAT_SUPPORT_TEXTURE2D|
 D3D11_FORMAT_SUPPORT_SHADER_SAMPLE|D3D11_FORMAT_SUPPORT_MIP;directory_count=0;last_error=5;tick=0;}
} // namespace

namespace a2fo::renderer::textures {
HRESULT validate_compressed_image(const CompressedImage& image,std::string& error) noexcept {
 add("validate:"+std::to_string(image.mips.size()));
 if(failing=="validate") {error="synthetic validation failure";return E_INVALIDARG;}
 error.clear();return S_OK;
}
}
UINT ID3D11Texture2D::Release(){add("release:texture");return 0;}
UINT ID3D11ShaderResourceView::Release(){add("release:view");return 0;}
UINT ID3D11ShaderResourceView::AddRef(){add("addref:view");return 2;}
UINT ID3D11RenderTargetView::Release(){add("release:rtv");return 0;}
UINT ID3D11DepthStencilView::Release(){add("release:dsv");return 0;}
UINT ID3D11DeviceContext::Release(){add("release:context");return 0;}
void ID3D11DeviceContext::ClearState(){add("clear-state");}
void ID3D11DeviceContext::OMSetRenderTargets(UINT,ID3D11RenderTargetView* const*,ID3D11DepthStencilView*){add("bind-targets");}
void ID3D11DeviceContext::RSSetViewports(UINT,const D3D11_VIEWPORT* v){add("viewport:"+std::to_string(int(v->Width))+","+std::to_string(int(v->Height)));}
void ID3D11DeviceContext::ClearRenderTargetView(ID3D11RenderTargetView*,const float*){add("clear-colour");}
void ID3D11DeviceContext::ClearDepthStencilView(ID3D11DepthStencilView*,UINT,float,UINT){add("clear-depth");}
UINT ID3D11Device::Release(){add("release:device");return 0;}
D3D_FEATURE_LEVEL ID3D11Device::GetFeatureLevel(){add("feature-level");return feature;}
HRESULT ID3D11Device::CheckFormatSupport(DXGI_FORMAT f,UINT* out){add("support:"+std::to_string(f));*out=support;return status("support");}
HRESULT ID3D11Device::CreateTexture2D(const D3D11_TEXTURE2D_DESC* desc,const D3D11_SUBRESOURCE_DATA* data,ID3D11Texture2D** out){
 const bool compressed=data!=nullptr;add(std::string("create-texture:")+(compressed?"compressed:":"depth:")+std::to_string(desc->Width)+"x"+std::to_string(desc->Height)+":"+std::to_string(desc->MipLevels)+":"+std::to_string(desc->BindFlags));
 if(compressed) add("mip:"+std::to_string(data[0].SysMemPitch)+":"+std::to_string(data[0].SysMemSlicePitch));
 if(failing==(compressed?"texture":"depth"))return E_FAIL;
 *out=&texture;return S_OK;
}
HRESULT ID3D11Device::CreateShaderResourceView(ID3D11Texture2D*,const D3D11_SHADER_RESOURCE_VIEW_DESC* desc,ID3D11ShaderResourceView** out){add("create-view:"+std::to_string(desc->Texture2D.MipLevels));if(failing=="view")return E_FAIL;*out=&view;return S_OK;}
HRESULT ID3D11Device::QueryInterface(int,void** out){add("query-dxgi");if(failing=="query")return E_FAIL;*out=&dxgi;return S_OK;}
HRESULT ID3D11Device::CreateRenderTargetView(ID3D11Texture2D*,const void*,ID3D11RenderTargetView** out){add("create-rtv");if(failing=="rtv")return E_FAIL;*out=&rtv;return S_OK;}
HRESULT ID3D11Device::CreateDepthStencilView(ID3D11Texture2D*,const void*,ID3D11DepthStencilView** out){add("create-dsv");if(failing=="dsv")return E_FAIL;*out=&dsv;return S_OK;}
HRESULT ID3D11Device::GetDeviceRemovedReason(){add("removed-reason");return E_FAIL;}
HRESULT IDXGIDevice::GetAdapter(IDXGIAdapter** out){add("get-adapter");if(failing=="adapter")return E_FAIL;*out=&adapter;return S_OK;}
UINT IDXGIDevice::Release(){add("release:dxgi");return 0;}
HRESULT IDXGIAdapter::GetParent(int,void** out){add("get-factory");if(failing=="factory")return E_FAIL;*out=&factory;return S_OK;}
UINT IDXGIAdapter::Release(){add("release:adapter");return 0;}
HRESULT IDXGIFactory::MakeWindowAssociation(HWND,UINT flags){add("association:"+std::to_string(flags));return status("association");}
UINT IDXGIFactory::Release(){add("release:factory");return 0;}
HRESULT IDXGISwapChain::GetBuffer(UINT,int,void** out){add("get-buffer");if(failing=="buffer")return E_FAIL;*out=&buffer;return S_OK;}
HRESULT IDXGISwapChain::Present(UINT,UINT){add("present");if(failing=="removed")return DXGI_ERROR_DEVICE_REMOVED;if(failing=="reset")return DXGI_ERROR_DEVICE_RESET;return status("present");}
UINT IDXGISwapChain::Release(){add("release:swap");return 0;}
DWORD GetEnvironmentVariableA(const char*,char* out,DWORD){add("env");if(failing=="env-missing")return 0;std::memcpy(out,"C:\\Cache",9);return 8;}
DWORD GetTempPathA(DWORD,char* out){add("temp");std::memcpy(out,"C:\\Temp",8);return 7;}
BOOL CreateDirectoryA(const char* path,void*){add(std::string("mkdir:")+path);++directory_count;if(failing=="mkdir"&&directory_count==2){last_error=5;return 0;}if(failing=="mkdir-exists"){last_error=ERROR_ALREADY_EXISTS;return 0;}return 1;}
DWORD GetLastError(){add("last-error");return last_error;}
BOOL IsWindow(HWND window){add("is-window");return window==reinterpret_cast<HWND>(1);}
DWORD GetWindowThreadProcessId(HWND,DWORD* pid){add("window-owner");*pid=42;return 7;}
DWORD GetCurrentProcessId(){return 42;}
DWORD GetCurrentThreadId(){return 7;}
HMODULE LoadLibraryA(const char*){add("load-library");return failing=="load"?nullptr:reinterpret_cast<HMODULE>(1);}
HRESULT WINAPI fake_create(IDXGIAdapter*,D3D_DRIVER_TYPE,HMODULE,UINT flags,
 const D3D_FEATURE_LEVEL*,UINT,UINT,const DXGI_SWAP_CHAIN_DESC* desc,
 IDXGISwapChain** sw,ID3D11Device** dev,D3D_FEATURE_LEVEL* obtained,ID3D11DeviceContext** ctx){
 add("create-device:"+std::to_string(flags)+":"+std::to_string(desc->BufferDesc.Width)+"x"+std::to_string(desc->BufferDesc.Height));
 if(debug_missing&&flags)return DXGI_ERROR_SDK_COMPONENT_MISSING;
 if(failing=="create")return E_FAIL;
 *sw=&swap;*dev=&device;*ctx=&context;*obtained=feature;return S_OK;
}
FARPROC GetProcAddress(HMODULE,const char*){add("get-proc");return failing=="proc"?nullptr:reinterpret_cast<FARPROC>(&fake_create);}
BOOL FreeLibrary(HMODULE){add("free-library");return 1;}
ULONGLONG GetTickCount64(){add("tick");return static_cast<ULONGLONG>((tick++)*10);}
namespace a2fo::renderer {
struct UiRenderer::Impl {};
UiRenderer::UiRenderer(){add("ui-construct");}
UiRenderer::~UiRenderer(){add("ui-destruct");}
HRESULT UiRenderer::initialize(ID3D11Device*,ID3D11DeviceContext*,const std::string& cache,Log){add("ui-init:"+cache);return status("ui-init");}
void UiRenderer::shutdown() noexcept {add("ui-shutdown");}
HRESULT UiRenderer::set_target(ID3D11RenderTargetView*,ID3D11DepthStencilView*,UINT w,UINT h){add("ui-target:"+std::to_string(w)+"x"+std::to_string(h));return status("ui-target");}
HRESULT UiRenderer::clear_color(DWORD color,UINT,const Rect*){add("ui-clear:"+std::to_string(color));return status("ui-clear");}
HRESULT UiRenderer::draw_up(UINT p,UINT count,DWORD fvf,const void*,std::size_t bytes,UINT stride,const UiDrawState& state){add("ui-draw:"+std::to_string(p)+":"+std::to_string(count)+":"+std::to_string(fvf)+":"+std::to_string(bytes)+":"+std::to_string(stride)+":"+std::to_string(state.color_op));return status("ui-draw");}
}
int main(){
 using namespace a2fo::renderer;
#ifndef A2FO_PROBE_ONLY
 textures::CompressedImage image;image.format=DXGI_FORMAT_BC7_UNORM;image.width=4;image.height=4;image.mips.push_back({4,4,16,16,0});image.bytes.resize(16);
 for(const auto& test:std::vector<std::string>{"","validate","support","texture","view","feature","low-support"}){
  reset(test);if(test=="feature")feature=D3D_FEATURE_LEVEL_10_0;if(test=="low-support")support=0;
  ID3D11ShaderResourceView* out=nullptr;CompressedTextureInfo info;
  info.width=123;info.height=456;info.rgba8_reference_bytes=789;
  std::string error;
  HRESULT hr=create_compressed_texture(&device,image,&out,info,error);
  add("info:"+std::to_string(info.width)+"x"+std::to_string(info.height)+
      ":"+std::to_string(info.rgba8_reference_bytes)+":"+std::to_string(out!=nullptr));
  if(out)out->Release();flush("compressed-"+test,hr,error);
 }
 for(const auto& test:std::vector<std::string>{"null-output","null-device","no-mips","two-mips"}){
  reset();auto variant=image;
  if(test=="no-mips")variant.mips.clear();
  if(test=="two-mips")variant.mips.push_back({2,2,16,16,0});
  ID3D11ShaderResourceView* out=nullptr;CompressedTextureInfo info;
  info.width=123;std::string error;
  HRESULT hr=create_compressed_texture(test=="null-device"?nullptr:&device,variant,
      test=="null-output"?nullptr:&out,info,error);
  add("info:"+std::to_string(info.width)+":"+std::to_string(out!=nullptr));
  if(out)out->Release();flush("compressed-"+test,hr,error);
 }
 reset();CompressedTextureHandle handle;ID3D11ShaderResourceView* out=nullptr;std::string error;
 flush("handle-empty",handle.get_view(&out));
 HRESULT hr=handle.initialize(&device,image,error);flush("handle-init",hr,error);
 hr=handle.get_view(&out);if(out)out->Release();flush("handle-get",hr);
 reset("view");hr=handle.initialize(&device,image,error);flush("handle-failed-reinit",hr,error);
 hr=handle.get_view(&out);if(out)out->Release();flush("handle-still-valid",hr);
#else
 HRESULT hr=0;
#endif
 reset();{ProbeDevice probe;hr=probe.present_test_colour();flush("probe-unready-colour",hr);
 hr=probe.present_test_triangle("");flush("probe-unready-triangle",hr);
 hr=probe.initialize(nullptr,32,16,false,log_line);flush("probe-invalid-window",hr);
 hr=probe.initialize(reinterpret_cast<HWND>(1),0,16,false,log_line);flush("probe-zero-width",hr);
 hr=probe.initialize(reinterpret_cast<HWND>(1),16385,16,false,log_line);flush("probe-wide",hr);}
 for(const auto& test:std::vector<std::string>{"load","proc","create","query","adapter","factory","association","buffer","rtv","depth","dsv",""}){
  reset(test);ProbeDevice probe;hr=probe.initialize(reinterpret_cast<HWND>(1),32,16,false,log_line);
  flush("probe-init-"+test,hr);probe.shutdown();trace.clear();
 }
 reset();debug_missing=true;{ProbeDevice probe;hr=probe.initialize(reinterpret_cast<HWND>(1),32,16,true,log_line);flush("probe-debug-retry",hr);probe.shutdown();trace.clear();}
 for(const auto& test:std::vector<std::string>{"","removed","reset","ui-init","ui-target","ui-clear","ui-draw","present"}){
  reset();ProbeDevice probe;probe.initialize(reinterpret_cast<HWND>(1),32,16,false,log_line);trace.clear();failing=test;
  hr=probe.present_test_colour();flush("colour-"+test,hr);probe.shutdown();trace.clear();
 }
 for(const auto& test:std::vector<std::string>{"","removed","ui-init","ui-target","ui-clear","ui-draw","present"}){
  reset();ProbeDevice probe;probe.initialize(reinterpret_cast<HWND>(1),32,16,false,log_line);trace.clear();failing=test;
  hr=probe.present_test_triangle("cache");flush("triangle-"+test,hr);probe.shutdown();trace.clear();
 }
 for(const auto& test:std::vector<std::string>{"","env-missing","mkdir","mkdir-exists"}){
  reset(test);hr=run_d3d11_window_probe(reinterpret_cast<HWND>(1),32,16,false,log_line);
  flush("bridge-"+test,hr);
 }
}
