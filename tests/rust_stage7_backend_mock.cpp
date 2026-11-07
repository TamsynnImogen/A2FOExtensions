// Full backend.cpp with normally constructed headless COM ABI mocks.
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
static WINBOOL WINAPI mock_is_window(HWND){return TRUE;}
static DWORD WINAPI mock_window_thread(HWND,LPDWORD process){*process=GetCurrentProcessId();return GetCurrentThreadId();}
static bool mock_load_ok=false,mock_symbol_ok=false;
static int mock_create_calls=0;
static HRESULT WINAPI mock_create_device(IDXGIAdapter*,D3D_DRIVER_TYPE,HMODULE,UINT,
    const D3D_FEATURE_LEVEL*,UINT,UINT,const DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**,
    ID3D11Device**,D3D_FEATURE_LEVEL*,ID3D11DeviceContext**){
    ++mock_create_calls;
    return mock_create_calls==1?DXGI_ERROR_SDK_COMPONENT_MISSING:E_FAIL;
}
static HMODULE WINAPI mock_load_library(const char*){if(!mock_load_ok){SetLastError(ERROR_MOD_NOT_FOUND);return nullptr;}return reinterpret_cast<HMODULE>(1);}
static FARPROC WINAPI mock_get_proc(HMODULE,const char*){if(!mock_symbol_ok){SetLastError(ERROR_PROC_NOT_FOUND);return nullptr;}return reinterpret_cast<FARPROC>(&mock_create_device);}
static BOOL WINAPI mock_free_library(HMODULE){return TRUE;}
#define LoadLibraryA mock_load_library
#define GetProcAddress mock_get_proc
#define FreeLibrary mock_free_library
#define IsWindow mock_is_window
#define GetWindowThreadProcessId mock_window_thread
#include A2FO_BACKEND_SOURCE
#undef IsWindow
#undef GetWindowThreadProcessId
#undef LoadLibraryA
#undef GetProcAddress
#undef FreeLibrary
#include <cstdio>
#include <vector>
using namespace a2fo::renderer::game;
namespace a2fo::renderer {
struct DrawPipeline::Impl {};
DrawPipeline::DrawPipeline() = default;
DrawPipeline::~DrawPipeline() = default;
HRESULT DrawPipeline::initialize(ID3D11Device*,ID3D11DeviceContext*,const std::string&,Log){return E_FAIL;}
HRESULT DrawPipeline::draw(const State&,const Draw&,const std::array<Sampling,8>&){return E_FAIL;}
HRESULT compile_shader(const ShaderCompileRequest&,const std::string&,ShaderCompileResult&){return E_FAIL;}
}
namespace a2fo::renderer::compat {
bool prepare_ok=false;
HRESULT prepare_texture_upload(UINT,UINT width,UINT height,const void*,std::size_t,UINT,TextureUpload& out) noexcept {
    if(!prepare_ok)return E_FAIL;out.width=width;out.height=height;out.format=DXGI_FORMAT_R8G8B8A8_UNORM;return S_OK;
}
}
namespace a2fo::renderer::game {
struct MockCom {
    void** methods;
    ULONG refs=1;
    const char* name;
    MockCom(void** v,const char* n):methods(v),name(n){}
};
static std::vector<const char*> calls;
static unsigned logs=0;
static void log_message(const std::string&){++logs;}
static MockCom *device_object=nullptr;
static HRESULT fullscreen_result=E_FAIL,resize_result=E_FAIL,present_result=S_OK;
static ULONG STDMETHODCALLTYPE add_ref(void *p){return ++static_cast<MockCom*>(p)->refs;}
static ULONG STDMETHODCALLTYPE release_ref(void *p){return --static_cast<MockCom*>(p)->refs;}
static void STDMETHODCALLTYPE get_device(void*,ID3D11Device** out){
    calls.push_back("get-device");*out=reinterpret_cast<ID3D11Device*>(device_object);add_ref(device_object);
}
static void STDMETHODCALLTYPE om(void*,UINT,ID3D11RenderTargetView*const*,ID3D11DepthStencilView*){
    calls.push_back("om");
}
static void STDMETHODCALLTYPE ps(void*,UINT,UINT,ID3D11ShaderResourceView*const*){
    calls.push_back("ps");
}
static void STDMETHODCALLTYPE copy(void*,ID3D11Resource*,UINT,UINT,UINT,UINT,
                                  ID3D11Resource*,UINT,const D3D11_BOX* box){
    calls.push_back("copy");
    std::printf("copy-box|%u,%u,%u,%u\n",box->left,box->top,box->right,box->bottom);
}
static void STDMETHODCALLTYPE clear_state(void*){calls.push_back("clear-state");}
static void STDMETHODCALLTYPE viewport(void*,UINT,const D3D11_VIEWPORT*){calls.push_back("viewport");}
static void STDMETHODCALLTYPE clear_color(void*,ID3D11RenderTargetView*,const FLOAT[4]){calls.push_back("clear-color");}
static void STDMETHODCALLTYPE clear_depth(void*,ID3D11DepthStencilView*,UINT,FLOAT,UINT8){calls.push_back("clear-depth");}
static HRESULT STDMETHODCALLTYPE fullscreen(void*,BOOL,IDXGIOutput*){calls.push_back("fullscreen");return fullscreen_result;}
static HRESULT STDMETHODCALLTYPE resize_buffers(void*,UINT,UINT,UINT,DXGI_FORMAT,UINT){calls.push_back("resize-buffers");return resize_result;}
static HRESULT STDMETHODCALLTYPE present(void*,UINT,UINT){calls.push_back("present");return present_result;}
static HRESULT STDMETHODCALLTYPE removed_reason(void*){calls.push_back("removed-reason");return E_FAIL;}
static void STDMETHODCALLTYPE get_desc(void*,D3D11_TEXTURE2D_DESC* desc){
    calls.push_back("get-desc");*desc={};desc->Width=8;desc->Height=8;
    desc->Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc->SampleDesc.Count=1;
}
static HRESULT STDMETHODCALLTYPE create_view(void*,ID3D11Resource*,const D3D11_RENDER_TARGET_VIEW_DESC*,
    ID3D11RenderTargetView**){calls.push_back("create-view");return E_FAIL;}
static void STDMETHODCALLTYPE update(void*,ID3D11Resource*,UINT,const D3D11_BOX*,const void*,UINT,UINT){calls.push_back("update");}
static HRESULT STDMETHODCALLTYPE create_texture2d(void*,const D3D11_TEXTURE2D_DESC*,
    const D3D11_SUBRESOURCE_DATA*,ID3D11Texture2D**){calls.push_back("create-texture2d");return E_FAIL;}
static HRESULT STDMETHODCALLTYPE get_buffer(void*,UINT,REFIID,void**){calls.push_back("get-buffer");return E_FAIL;}
struct BackendFixture {
    static void ready(Backend& b,MockCom& d,MockCom& c,MockCom& s){
        device_object=&d;
        b.impl_->device=reinterpret_cast<ID3D11Device*>(&d);
        b.impl_->context=reinterpret_cast<ID3D11DeviceContext*>(&c);
        b.impl_->swap=reinterpret_cast<IDXGISwapChain*>(&s);
        b.impl_->thread=GetCurrentThreadId();
        b.impl_->width=8;b.impl_->height=8;
    }
    static std::shared_ptr<Target> target(MockCom& image,MockCom& color,UINT width,UINT height){
        auto t=std::make_shared<Target>();
        t->impl_->image=reinterpret_cast<ID3D11Texture2D*>(&image);
        t->impl_->color=reinterpret_cast<ID3D11RenderTargetView*>(&color);
        t->impl_->width=width;t->impl_->height=height;
        t->impl_->format=DXGI_FORMAT_R8G8B8A8_UNORM;
        return t;
    }
    static void bound(Backend& b,const std::shared_ptr<Target>& target){b.impl_->bound_color=target;}
    static bool lost(const Backend& b){return b.impl_->device_lost;}
    static void window(Backend& b){b.impl_->hwnd=reinterpret_cast<HWND>(0x1234);}
    static void log(Backend& b){b.impl_->log=&log_message;}
    static void pipeline(Backend& b){b.impl_->pipeline=std::make_unique<DrawPipeline>();}
    static std::shared_ptr<Texture> texture(MockCom& image,MockCom& view){
        auto t=std::make_shared<Texture>();
        t->impl_->image=reinterpret_cast<ID3D11Texture2D*>(&image);
        t->impl_->view=reinterpret_cast<ID3D11ShaderResourceView*>(&view);
        t->impl_->format=DXGI_FORMAT_R8G8B8A8_UNORM;
        t->impl_->levels=1;t->impl_->targets.resize(1);t->impl_->legacy=22;
        return t;
    }
};
}
int main(){
    void *device_table[64]{};void *context_table[120]{};void *swap_table[32]{};
    void *image_table[16]{};void *color_table[16]{};
    for(auto *table:{device_table,context_table,swap_table,image_table,color_table}){
        table[1]=reinterpret_cast<void*>(&add_ref);
        table[2]=reinterpret_cast<void*>(&release_ref);
    }
    context_table[33]=reinterpret_cast<void*>(&om);
    context_table[8]=reinterpret_cast<void*>(&ps);
    context_table[46]=reinterpret_cast<void*>(&copy);
    context_table[110]=reinterpret_cast<void*>(&clear_state);
    image_table[3]=reinterpret_cast<void*>(&get_device);
    image_table[10]=reinterpret_cast<void*>(&get_desc);
    device_table[9]=reinterpret_cast<void*>(&create_view);
    context_table[48]=reinterpret_cast<void*>(&update);
    context_table[44]=reinterpret_cast<void*>(&viewport);
    context_table[50]=reinterpret_cast<void*>(&clear_color);
    context_table[53]=reinterpret_cast<void*>(&clear_depth);
    swap_table[10]=reinterpret_cast<void*>(&fullscreen);
    swap_table[13]=reinterpret_cast<void*>(&resize_buffers);
    swap_table[9]=reinterpret_cast<void*>(&get_buffer);
    swap_table[8]=reinterpret_cast<void*>(&present);
    device_table[39]=reinterpret_cast<void*>(&removed_reason);
    device_table[5]=reinterpret_cast<void*>(&create_texture2d);
    MockCom device(device_table,"device"),context(context_table,"context"),swap(swap_table,"swap"),
            source_image(image_table,"source-image"),dest_image(image_table,"dest-image"),
            source_view(color_table,"source-view"),dest_view(color_table,"dest-view");
    Backend backend;BackendFixture::ready(backend,device,context,swap);
    auto source=BackendFixture::target(source_image,source_view,8,8);
    auto dest=BackendFixture::target(dest_image,dest_view,8,8);
    BackendFixture::bound(backend,dest);
    BackendFixture::window(backend);
    auto show=[](const char *name,HRESULT hr){std::printf("%s|%08x\n",name,unsigned(hr));};
    show("initialize-invalid",backend.initialize(reinterpret_cast<HWND>(0x1234),0,8,true,false,false,"",nullptr,75));
    show("initialize-depth",backend.initialize(reinterpret_cast<HWND>(0x1234),8,8,true,false,false,"",nullptr,999));
    show("initialize-loader",backend.initialize(reinterpret_cast<HWND>(0x1234),8,8,true,false,false,"",nullptr,75));
    mock_load_ok=true;
    show("initialize-symbol",backend.initialize(reinterpret_cast<HWND>(0x1234),8,8,true,false,false,"",nullptr,75));
    mock_symbol_ok=true;
    show("initialize-debug-fallback",backend.initialize(reinterpret_cast<HWND>(0x1234),8,8,true,false,true,"",nullptr,75));
    std::printf("initialize-create-calls|%d\n",mock_create_calls);
    show("cooperative",backend.cooperative());
    RECT bad{};
    show("present-invalid",backend.present(&bad,nullptr,nullptr,nullptr));
    show("present-success",backend.present(nullptr,nullptr,nullptr,nullptr));
    present_result=E_FAIL;
    show("present-fail",backend.present(nullptr,nullptr,nullptr,nullptr));
    show("bind-valid",backend.bind_target(dest,nullptr));
    show("bind-invalid",backend.bind_target({},nullptr));
    std::vector<std::uint8_t> pixels;UINT read_width=0,read_height=0;
    show("read-staging-fail",backend.read_target(*source,pixels,read_width,read_height));
    show("copy-whole",backend.copy_target(*source,nullptr,0,*dest,nullptr));
    RECT r{1,2,3,4};POINT p{3,4};
    show("copy-part",backend.copy_target(*source,&r,1,*dest,&p));
    r={-1,0,3,4};show("copy-invalid",backend.copy_target(*source,&r,1,*dest,&p));
    r={0,0,2,2};show("copy-overlap",backend.copy_target(*source,&r,1,*source,nullptr));
    State state{};state.width=8;state.height=8;
    show("clear-zero",backend.clear(state,0,nullptr,0,0,1,0));
    show("clear-whole",backend.clear(state,0,nullptr,1,0xff224466,1,0));
    show("clear-invalid",backend.clear(state,1,nullptr,1,0,1,0));
    state.width=0;show("clear-empty",backend.clear(state,0,nullptr,1,0,1,0));
    state.width=8;RECT half{0,0,4,4};
    show("clear-partial-compile-fail",backend.clear(state,1,&half,1,0,1,0));
    std::vector<MipUpload> empty;
    std::shared_ptr<Texture> texture;
    show("texture-empty",backend.create_texture(22,empty,texture));
    std::vector<MipUpload> mips(2);mips[0].width=4;mips[0].height=4;
    mips[1].width=3;mips[1].height=3;
    show("texture-prepare-fail",backend.create_texture(22,mips,texture));
    a2fo::renderer::compat::prepare_ok=true;
    show("texture-mip-fail",backend.create_texture(22,mips,texture));
    mips[1].width=2;mips[1].height=2;
    show("texture-create-fail",backend.create_texture(22,mips,texture));
    std::shared_ptr<Target> target;
    auto native_texture=BackendFixture::texture(source_image,source_view);
    ID3D11ShaderResourceView *view=nullptr;
    show("texture-view",backend.texture_view(*native_texture,&view));
    if(view)view->Release();
    show("texture-target-invalid",backend.texture_target(*native_texture,target,2));
    show("texture-target-create-fail",backend.texture_target(*native_texture,target,0));
    MipUpload update_mip{};update_mip.width=8;update_mip.height=8;
    show("texture-update-invalid",backend.update_texture(*native_texture,2,update_mip));
    a2fo::renderer::compat::prepare_ok=false;
    show("texture-update-prepare-fail",backend.update_texture(*native_texture,0,update_mip));
    show("target-size",backend.create_target(0,8,22,false,target));
    show("target-create-fail",backend.create_target(8,8,22,false,target));
    show("draw-empty",backend.draw(state,Draw{},{}));
    BackendFixture::pipeline(backend);
    show("draw-failed",backend.draw(state,Draw{},{}));
    show("draw-repeat",backend.draw(state,Draw{},{}));
    state.width=0;show("draw-viewport-invalid",backend.draw(state,Draw{},{}));
    show("resize-invalid",backend.resize(0,8,true,75));
    show("resize-depth",backend.resize(8,8,true,999));
    show("resize-fullscreen-fail",backend.resize(8,8,true,75));
    fullscreen_result=S_OK;
    show("resize-buffers-fail",backend.resize(8,8,true,75));
    resize_result=S_OK;
    show("resize-back-fail",backend.resize(8,8,true,75));
    present_result=DXGI_ERROR_DEVICE_REMOVED;
    show("present-lost",backend.present(nullptr,nullptr,nullptr,nullptr));
    std::printf("device-lost|%u\n",unsigned(BackendFixture::lost(backend)));
    BackendFixture::log(backend);
    std::printf("memory-first|%u\n",backend.available_memory());
    std::printf("memory-repeat|%u\n",backend.available_memory());
    backend.log_memory();
    std::printf("memory-logs|%u\n",logs);
    std::printf("calls");for(const char* call:calls)std::printf("|%s",call);std::printf("\n");
}
