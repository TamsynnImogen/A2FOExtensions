// Controlled driver-failure fixture. All COM slots come from the local SDK.
#include "../renderer/native11/renderer.cpp"
#include <cstdio>
#include <cstdlib>
extern "C" unsigned native11_sdk_swap_slots();
extern "C" unsigned native11_sdk_swap_present_slot();
extern "C" unsigned native11_sdk_swap_resize_slot();
namespace a2fo::native11 {
struct RendererFixtureAccess {
    static IDXGISwapChain* swap(Renderer& r){return r.p_->presentation_swap;}
    static ID3D11Device* device(Renderer& r){return r.p_->device;}
    static ID3D11DeviceContext* context(Renderer& r){return r.p_->context;}
};
}
using namespace a2fo::native11;
namespace {
void need(bool value,const char* text){if(!value){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
using PresentCall=HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
using ResizeCall=HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
struct Probe {Renderer* renderer;PresentCall original;ResizeCall original_resize=nullptr;HRESULT result=S_OK;bool real=false,copied=false,fail_resize=false;unsigned calls=0,resizes=0;};
Probe* active=nullptr;
HRESULT STDMETHODCALLTYPE probe_resize(IDXGISwapChain* swap,UINT count,UINT w,UINT h,DXGI_FORMAT format,UINT flags){
    auto& p=*active;++p.resizes;
    return p.fail_resize?E_FAIL:p.original_resize(swap,count,w,h,format,flags);
}
HRESULT STDMETHODCALLTYPE probe_present(IDXGISwapChain* swap,UINT sync,UINT flags){
    auto& p=*active;++p.calls;ID3D11Texture2D *back=nullptr,*staging=nullptr;
    auto hr=swap->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&back));
    if(FAILED(hr))return hr;
    D3D11_TEXTURE2D_DESC desc{};back->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;
    desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    hr=RendererFixtureAccess::device(*p.renderer)->CreateTexture2D(&desc,nullptr,&staging);
    if(SUCCEEDED(hr)){
        auto* context=RendererFixtureAccess::context(*p.renderer);context->CopyResource(staging,back);
        D3D11_MAPPED_SUBRESOURCE map{};hr=context->Map(staging,0,D3D11_MAP_READ,0,&map);
        if(SUCCEEDED(hr)){
            const auto* bytes=static_cast<const unsigned char*>(map.pData);
            p.copied=bytes&&map.RowPitch>=desc.Width*4&&bytes[0]==153&&bytes[1]==102&&bytes[2]==51&&bytes[3]==255;
            context->Unmap(staging,0);
        }
    }
    if(staging)staging->Release();
    back->Release();
    if(FAILED(hr))return hr;
    return p.real?p.original(swap,sync,flags):p.result;
}
struct SwapProbe {
    IDXGISwapChain* swap;void** original;std::vector<void*> replacement;Probe probe;
    explicit SwapProbe(Renderer& r):swap(RendererFixtureAccess::swap(r)),original(*reinterpret_cast<void***>(swap)),
        replacement(original,original+native11_sdk_swap_slots()),probe{&r,nullptr}{
        const auto slot=native11_sdk_swap_present_slot();std::memcpy(&probe.original,&original[slot],sizeof(probe.original));
        auto function=probe_present;std::memcpy(&replacement[slot],&function,sizeof(function));
        const auto resize_slot=native11_sdk_swap_resize_slot();
        std::memcpy(&probe.original_resize,&original[resize_slot],sizeof(probe.original_resize));
        auto resize_function=probe_resize;std::memcpy(&replacement[resize_slot],&resize_function,sizeof(resize_function));
        active=&probe;*reinterpret_cast<void***>(swap)=replacement.data();
    }
    ~SwapProbe(){*reinterpret_cast<void***>(swap)=original;active=nullptr;}
};
HWND hidden(unsigned w,unsigned h){auto v=CreateWindowExW(0,L"STATIC",L"Native11 GPU copy fixture",WS_POPUP,0,0,w,h,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);need(v&&!IsWindowVisible(v),"hidden fixture");return v;}
}
int main(){
    Renderer renderer;need(renderer.initialize_offscreen(64,64)==S_OK,"offscreen device");auto window=hidden(64,64);
    need(renderer.configure_presentation(window,64,64)==S_OK,"native swapchain");
    FrameView frame;frame.clear_colour[0]=.2f;frame.clear_colour[1]=.4f;frame.clear_colour[2]=.6f;frame.clear_colour[3]=1;
    need(renderer.present()<0,"no fresh image");
    {
        SwapProbe hook(renderer);need(renderer.render(frame,false)==S_OK,"native clear");
        hook.probe.real=true;const auto hr=renderer.present();need(hr==S_OK||hr==S_FALSE,"actual private Present");
        need(hook.probe.copied&&hook.probe.calls==1,"swapchain pixels received GPU-only copy before Present");
        need(renderer.present()<0&&hook.probe.calls==1,"one-shot image");
        need(renderer.render(frame,false)==S_OK&&renderer.present(5)==E_INVALIDARG,"renderer sync validation");
        need(renderer.present()<0&&hook.probe.calls==1,"invalid renderer present consumed image");
        hook.probe.real=false;hook.probe.result=DXGI_STATUS_OCCLUDED;
        need(renderer.render(frame,false)==S_OK&&renderer.present()==S_FALSE,"occlusion normalized");
        need(renderer.present()<0,"occluded image not reusable");
        hook.probe.result=DXGI_ERROR_DEVICE_REMOVED;
        need(renderer.render(frame,false)==S_OK&&renderer.present()==DXGI_ERROR_DEVICE_REMOVED,"driver failure returned");
        need(renderer.present()<0,"failed image not reusable");
        hook.probe.result=S_OK;need(renderer.render(frame,false)==S_OK&&renderer.present()==S_OK,"fresh retry after driver failure");
        need(renderer.render(frame,false)==S_OK&&renderer.resize(64,64)==S_OK&&renderer.present()<0,"even same-size resize revokes image");
        need(renderer.resize(80,48)==S_OK,"resize colour/depth targets first");
        need(renderer.configure_presentation(window,80,48)==E_INVALIDARG,"client-size mismatch rejected");
        // Only this process's hidden window on the private X server is resized;
        // NOACTIVATE prevents input focus or activation.
        need(SetWindowPos(window,nullptr,0,0,80,48,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE)!=0,"fixture-owned hidden client resize");
        hook.probe.fail_resize=true;need(renderer.configure_presentation(window,80,48)==E_FAIL,"swapchain resize failure returned");
        need(renderer.present()<0,"failed resize cannot present old image");hook.probe.fail_resize=false;
        need(renderer.configure_presentation(window,80,48)==S_OK&&hook.probe.resizes==2,"same-window resize retry");
        need(renderer.render(frame,false)==S_OK&&renderer.present()==S_OK&&hook.probe.copied,"resized swapchain gets fresh GPU pixels");
    }
    need(renderer.configure_presentation(nullptr,0,0)==S_OK,"detach");need(DestroyWindow(window)!=0,"destroy fixture");
    need(renderer.resize(80,48)==S_OK,"offscreen resize");window=hidden(80,48);
    need(renderer.configure_presentation(window,80,48)==S_OK,"reattach different dimensions");
    need(renderer.render(frame,false)==S_OK,"resized render");need(DestroyWindow(window)!=0&&renderer.present()<0,"window loss");
    need(renderer.configure_presentation(nullptr,0,0)==S_OK,"detach dead window");
    std::vector<unsigned char> bytes;std::uint32_t w=0,h=0;need(renderer.readback(bytes,w,h)==S_OK&&w==80&&h==48,"offscreen survives window loss");
    renderer.shutdown();need(renderer.initialize_offscreen(32,32)==S_OK,"device recreation");renderer.shutdown();
    std::puts("PASS native D3D11 presentation GPU copy, occlusion and recovery");
}
