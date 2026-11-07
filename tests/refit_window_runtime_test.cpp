#include "../renderer/refit/host.h"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <d3d8.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
constexpr unsigned W=64,H=64;
static_assert(sizeof(void*)==4 && sizeof(A2FORefitApi2)==60);
void need(bool ok,const char* why) {if(!ok){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void check(int32_t got,int32_t want,const char* why) {
    if(got!=want){std::fprintf(stderr,"FAIL %s got=%ld want=%ld\n",why,long(got),long(want));std::exit(1);}
}
void ok(HRESULT hr,const char* why) {
    if(FAILED(hr))std::fprintf(stderr,"HRESULT %s 0x%08lx\n",why,static_cast<unsigned long>(hr));
    need(SUCCEEDED(hr),why);
}
template<class F> F symbol(HMODULE module,const char* name) {
    FARPROC p=GetProcAddress(module,name);F out=nullptr;
    static_assert(sizeof(out)==sizeof(p));std::memcpy(&out,&p,sizeof(out));return out;
}
using Create8=IDirect3D8* (WINAPI*)(UINT);
struct VkDeviceHead {uint32_t size;uintptr_t get_proc,instance,physical,device,queue;};
struct VkImage {uint32_t size;uint64_t image,view;uint32_t format,width,height,samples,layout;};
struct DxDeviceHead {uint32_t size;uintptr_t device,queue;};
struct DxImage {uint32_t size;uintptr_t resource;uint32_t format,width,height,levels,samples,state;
                uintptr_t rtv,dsv,srv;};
static_assert(sizeof(VkImage)==48&&sizeof(DxImage)==44);
using VkDeviceFn=const VkDeviceHead* (*)();
using DxDeviceFn=const DxDeviceHead* (*)();
using VkTargetFn=bool (*)(uint32_t,VkImage*);
using DxTargetFn=bool (*)(uint32_t,DxImage*);
struct TargetIdentity {uintptr_t device=0,queue=0;uint64_t colour=0,depth=0;};
TargetIdentity targets(HMODULE display,uint32_t backend) {
    if(backend==1) {
        auto get=symbol<VkDeviceFn>(display,"display8v1_vk");
        auto target=symbol<VkTargetFn>(display,"display8v1_vk_target");
        need(get&&target,"Vulkan device and target exports");
        const auto* dev=get();VkImage colour{},depth{};
        colour.size=sizeof(colour);depth.size=sizeof(depth);
        const bool colour_ok=target(0,&colour),depth_ok=target(1,&depth);
        need(dev&&dev->size>=sizeof(VkDeviceHead)&&dev->device&&dev->queue&&
             colour_ok&&depth_ok&&colour.size==sizeof(colour)&&
             depth.size==sizeof(depth)&&colour.image&&depth.image,"Vulkan target identity");
        return {dev->device,dev->queue,colour.image,depth.image};
    }
    auto get=symbol<DxDeviceFn>(display,"display8v1_dx12");
    auto target=symbol<DxTargetFn>(display,"display8v1_dx12_target");
    need(get&&target,"DX12 device and target exports");
    const auto* dev=get();DxImage colour{},depth{};
    colour.size=sizeof(colour);depth.size=sizeof(depth);
    need(dev&&dev->size>=sizeof(DxDeviceHead)&&dev->device&&dev->queue&&
         target(0,&colour)&&target(1,&depth)&&colour.size==sizeof(colour)&&
         depth.size==sizeof(depth)&&colour.resource&&depth.resource,"DX12 target identity");
    return {dev->device,dev->queue,colour.resource,depth.resource};
}
struct Windows {
    HINSTANCE instance=GetModuleHandleA(nullptr);
    HWND a=nullptr,b=nullptr;
    Windows() {
        WNDCLASSEXA c{};c.cbSize=sizeof(c);c.hInstance=instance;c.lpfnWndProc=DefWindowProcA;
        c.lpszClassName="A2FORefitWindowPrivateFixture";
        need(RegisterClassExA(&c)!=0,"register private class");
        a=create();b=create();
    }
    HWND create() const {
        HWND w=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,
            "A2FORefitWindowPrivateFixture","hidden handoff fixture",WS_POPUP,
            0,0,W,H,nullptr,nullptr,instance,nullptr);
        need(w&&!IsWindowVisible(w),"create hidden nonactivating HWND");return w;
    }
    ~Windows(){if(b)DestroyWindow(b);if(a)DestroyWindow(a);
        UnregisterClassA("A2FORefitWindowPrivateFixture",instance);}
};
struct WorkerWindow {const Windows* windows;HANDLE ready,stop;HWND hwnd=nullptr;};
DWORD WINAPI make_worker_window(void* raw) {
    auto& w=*static_cast<WorkerWindow*>(raw);
    w.hwnd=w.windows->create();SetEvent(w.ready);WaitForSingleObject(w.stop,10000);
    DestroyWindow(w.hwnd);return 0;
}
HRESULT create_device(IDirect3D8* d3d,HWND hwnd,IDirect3DDevice8** out) {
    D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=W;pp.BackBufferHeight=H;
    pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferCount=1;
    pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=hwnd;pp.Windowed=TRUE;
    pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;
    return d3d->CreateDevice(0,D3DDEVTYPE_HAL,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,out);
}
struct Vertex {float x,y,z,rhw;float u,v;};
void draw(IDirect3DDevice8* d,IDirect3DTexture8* texture,float z,
          unsigned x0,unsigned x1,DWORD colour) {
    ok(d->SetTexture(0,texture),"bind retained texture");
    ok(d->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_TEX1),"set textured FVF");
    ok(d->SetPixelShader(0),"fixed function pixel shader");
    ok(d->SetRenderState(D3DRS_LIGHTING,FALSE),"unlit");
    ok(d->SetRenderState(D3DRS_ZENABLE,TRUE),"depth enabled");
    ok(d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE),"depth write enabled");
    ok(d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL),"depth less equal");
    ok(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"no cull");
    ok(d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE),"opaque");
    ok(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"select colour");
    ok(d->SetTextureStageState(0,D3DTSS_COLORARG1,texture?D3DTA_TEXTURE:D3DTA_TFACTOR),"colour source");
    ok(d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1),"select alpha");
    ok(d->SetTextureStageState(0,D3DTSS_ALPHAARG1,texture?D3DTA_TEXTURE:D3DTA_TFACTOR),"alpha source");
    ok(d->SetRenderState(D3DRS_TEXTUREFACTOR,colour),"fallback colour");
    const Vertex v[4]={{float(x0)-.5f,15.5f,z,1,0,0},{float(x1)-.5f,15.5f,z,1,1,0},
                       {float(x0)-.5f,47.5f,z,1,0,1},{float(x1)-.5f,47.5f,z,1,1,1}};
    ok(d->BeginScene(),"begin legacy scene");
    ok(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(v[0])),"legacy draw");
    ok(d->EndScene(),"end legacy scene");
}
void identity(float (&m)[16]) {m[0]=m[5]=m[10]=m[15]=1;}
void identity34(float (&m)[12]) {m[0]=m[4]=m[8]=1;}
void upload_scene(const A2FORefitApi2* api) {
    auto sod=native11_fixture::model("unused.tga",false);
    A2FORefitModel model{sizeof(model),7,1,sod.data(),static_cast<uint32_t>(sod.size())};
    uint32_t nodes=0;check(api->upload_model(&model,&nodes),0,"upload actual SOD");
    need(nodes==1,"uploaded SOD has one node");
    std::vector<uint8_t> tga(18+16,0);tga[2]=2;tga[12]=2;tga[14]=2;tga[16]=32;tga[17]=0x20;
    for(std::size_t i=18;i<tga.size();i+=4){tga[i]=0xe0;tga[i+1]=0x60;tga[i+2]=0x10;tga[i+3]=255;}
    constexpr char name[]="bridgeblue.tga";
    A2FORefitEncodedTexture2 texture{sizeof(texture),name,sizeof(name)-1,1,
                                      tga.data(),static_cast<uint32_t>(tga.size()),0};
    check(api->upload_encoded_texture(&texture),0,"upload actual encoded TGA");
    std::fill(sod.begin(),sod.end(),0);
    std::fill(tga.begin(),tga.end(),0);
}
void run(const char* mode,const char* backend,const char* display_path,const char* renderer_path) {
    const bool movable=std::strcmp(mode,"movable")==0;
    need(movable||std::strcmp(mode,"fixed")==0,"known mode");
    const uint32_t backend_id=std::strcmp(backend,"vulkan")==0?1:2;
    need(backend_id==1||std::strcmp(backend,"d3d12")==0,"known backend");
    Windows windows;
    HMODULE display=LoadLibraryA(display_path);need(display,"load staged Display");
    auto configure=symbol<A2FORefitDisplayConfigure>(display,"A2FORefitDisplay_Configure_v1");
    auto prepare=symbol<A2FORefitDisplayPrepare>(display,"A2FORefitDisplay_Prepare_v1");
    auto passthrough=symbol<A2FORefitDisplayPassthrough>(display,"A2FORefitDisplay_Passthrough_v1");
    auto create8=symbol<Create8>(display,"Direct3DCreate8");
    need(configure&&prepare&&passthrough&&create8,"Display exports");
    check(movable?prepare(backend_id,W,H,0):configure(backend_id,windows.a,W,H,0),0,
          "initial Display admission");
    if(!movable)check(configure(backend_id,windows.b,W,H,0),A2FO_REFIT_STATE,
                      "fixed Configure refuses second owned HWND");
    IDirect3D8* d3d=create8(D3D_SDK_VERSION);need(d3d,"create facade");
    if(movable) {
        IDirect3DDevice8* invalid=reinterpret_cast<IDirect3DDevice8*>(1);
        check(create_device(d3d,nullptr,&invalid),D3DERR_INVALIDCALL,"null HWND refused");
        need(!invalid,"null HWND output cleared");
        WorkerWindow worker{&windows,CreateEventA(nullptr,TRUE,FALSE,nullptr),
                            CreateEventA(nullptr,TRUE,FALSE,nullptr)};
        need(worker.ready&&worker.stop,"worker events");
        HANDLE thread=CreateThread(nullptr,0,make_worker_window,&worker,0,nullptr);
        need(thread&&WaitForSingleObject(worker.ready,10000)==WAIT_OBJECT_0,"worker HWND ready");
        DWORD process=0;need(GetWindowThreadProcessId(worker.hwnd,&process)!=GetCurrentThreadId()&&
                             process==GetCurrentProcessId(),"wrong thread same-process HWND");
        invalid=reinterpret_cast<IDirect3DDevice8*>(1);
        check(create_device(d3d,worker.hwnd,&invalid),D3DERR_INVALIDCALL,
              "wrong-thread owned HWND refused");
        need(!invalid,"wrong-thread output cleared");
        SetEvent(worker.stop);need(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0,"worker joined");
        CloseHandle(thread);CloseHandle(worker.ready);CloseHandle(worker.stop);
    }
    IDirect3DDevice8* device=nullptr;
    ok(create_device(d3d,windows.a,&device),"create first device A");need(device,"first device");
    const auto original_targets=targets(display,backend_id);
    if(!movable) {
        IDirect3DDevice8* rejected=reinterpret_cast<IDirect3DDevice8*>(1);
        check(create_device(d3d,windows.b,&rejected),D3DERR_INVALIDCALL,
              "fixed owner refuses B");need(!rejected,"fixed rejection clears output");
        need(!IsWindowVisible(windows.a)&&!IsWindowVisible(windows.b),"fixed windows stay hidden");
        device->Release();d3d->Release();return;
    }
    check(passthrough(1),0,"enable native fixed-function passthrough");
    IDirect3DTexture8* texture=nullptr;
    ok(device->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture),"create retained texture");
    D3DLOCKED_RECT lock{};ok(texture->LockRect(0,&lock,nullptr,0),"lock retained texture");
    const DWORD blue=0xff1060e0;
    for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x)
        std::memcpy(static_cast<std::uint8_t*>(lock.pBits)+y*lock.Pitch+x*4,&blue,4);
    ok(texture->UnlockRect(0),"unlock retained texture");
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1,0),
       "seed colour and depth before handoff");
    draw(device,texture,.2f,8,30,0xffffffff); // Pending work; no Present.
    IDirect3DDevice8 *at_b=nullptr,*back_a=nullptr;
    ok(create_device(d3d,windows.b,&at_b),"move A to B");need(at_b,"B facade");
    ok(create_device(d3d,windows.a,&back_a),"move B to A");need(back_a,"A facade again");
    const auto final_targets=targets(display,backend_id);
    need(original_targets.device==final_targets.device&&
         original_targets.queue==final_targets.queue&&
         original_targets.colour==final_targets.colour&&
         original_targets.depth==final_targets.depth,
         "physical GPU device and engine targets unchanged");
    draw(back_a,nullptr,.8f,8,30,0xffff2020); // Must fail the retained depth test.
    draw(back_a,nullptr,.8f,30,34,0xffff2020); // Unoccluded control must render red.
    draw(back_a,texture,.2f,34,56,0xffffffff); // Original texture remains usable.
    HMODULE renderer=LoadLibraryA(renderer_path);need(renderer,"load Renderer only after final move");
    auto get=symbol<A2FORefitGetInterface>(renderer,"A2FORefitRenderer_GetInterface");
    need(get,"Renderer API export");
    auto api=reinterpret_cast<const A2FORefitApi2*>(get(2));
    need(api&&api->size==sizeof(A2FORefitApi2),"ABI2 Renderer");
    A2FORefitConfig config{sizeof(config),display,0,1};
    check(api->initialize(&config),0,"attach Renderer after handoff");
    upload_scene(api);
    constexpr char texture_name[]="bridgeblue.tga";
    A2FORefitPart2 part{};part.size=sizeof(part);part.model_id=7;part.model_revision=1;
    part.node_index=0;part.group_index=0;part.flags=A2FO_REFIT_UNLIT;
    identity34(part.world);part.world[10]=-1.2f;part.alpha=1;
    part.ambient[0]=part.ambient[1]=part.ambient[2]=1;
    part.texture_name=texture_name;part.texture_name_length=sizeof(texture_name)-1;
    part.texture_revision=1;
    A2FORefitFrame2 frame{};frame.size=sizeof(frame);frame.source_epoch=1;frame.serial=1;
    frame.width=W;frame.height=H;frame.flags=A2FO_REFIT_FRAME_DIRECT;
    identity(frame.view);identity(frame.projection);frame.parts=&part;frame.part_count=1;
    check(api->prepare_parts(&frame),0,"prepare uploaded DIRECT part");
    check(api->draw_prepared(1,1),0,"draw uploaded DIRECT part");
    std::vector<std::uint8_t> pixels(W*H*4);
    A2FORefitReadback readback{sizeof(readback),1,1,W,H,pixels.data(),
                              static_cast<uint32_t>(pixels.size())};
    check(api->readback(&readback),0,"read back retained engine colour");
    auto blue_at=[&](unsigned x,unsigned y){const auto i=(y*W+x)*4;
        return pixels[i]>pixels[i+1]+45&&pixels[i]>pixels[i+2]+45;};
    auto red_at=[&](unsigned x,unsigned y){const auto i=(y*W+x)*4;
        return pixels[i+2]>pixels[i+1]+45&&pixels[i+2]>pixels[i]+45;};
    need(blue_at(18,30)&&red_at(32,30)&&blue_at(45,30)&&blue_at(32,52),
         "pre-handoff colour/depth, retained texture, and uploaded DIRECT part survive");
    A2FORefitStatus status{};status.size=sizeof(status);
    check(api->status(&status),0,"status after uploaded direct draw");
    need(status.models==1&&status.textures==1&&status.source_epoch==1&&status.last_serial==1,
         "actual content and frame accepted before facade reset");
    const auto old_epoch=status.device_epoch;
    D3DPRESENT_PARAMETERS reset{};reset.BackBufferWidth=W;reset.BackBufferHeight=H;
    reset.BackBufferFormat=D3DFMT_X8R8G8B8;reset.BackBufferCount=1;
    reset.SwapEffect=D3DSWAPEFFECT_DISCARD;reset.hDeviceWindow=windows.a;reset.Windowed=TRUE;
    reset.EnableAutoDepthStencil=TRUE;reset.AutoDepthStencilFormat=D3DFMT_D24S8;
    ok(back_a->Reset(&reset),"facade Reset after renderer attach");
    A2FORefitStatus untouched{};untouched.size=sizeof(untouched);untouched.backend=0xa5a5a5a5;
    check(api->status(&untouched),A2FO_REFIT_STALE,"status stale after facade Reset");
    need(untouched.backend==0xa5a5a5a5,"stale status output untouched");
    check(api->prepare_parts(&frame),A2FO_REFIT_STALE,"old frame rejected before content reset");
    std::fill(pixels.begin(),pixels.end(),0x5a);
    check(api->readback(&readback),A2FO_REFIT_STALE,"old readback rejected before content reset");
    need(pixels[0]==0x5a,"stale readback output untouched");
    check(api->reset_content(2),0,"reacquire renderer after facade Reset");
    check(api->status(&status),0,"status after content reset");
    need(status.source_epoch==2&&status.last_serial==0&&status.models==0&&status.textures==0&&
         status.device_epoch!=old_epoch,"reset clears content and binds new device epoch");
    std::fill(pixels.begin(),pixels.end(),0x5a);
    check(api->readback(&readback),A2FO_REFIT_STALE,"old readback rejected after reset");
    need(pixels[0]==0x5a,"rejected readback output untouched");
    check(api->prepare_parts(&frame),A2FO_REFIT_STALE,"old source epoch rejected after reset");
    upload_scene(api);frame.source_epoch=2;frame.serial=1;
    check(api->prepare_parts(&frame),0,"prepare reuploaded DIRECT part after Reset");
    check(api->draw_prepared(2,1),0,"draw reuploaded DIRECT part after Reset");
    readback.source_epoch=2;readback.serial=1;
    check(api->readback(&readback),0,"readback after facade Reset reacquire");
    need(blue_at(45,30),"retained native colour after Reset and direct draw");
    need(!IsWindowVisible(windows.a)&&!IsWindowVisible(windows.b),"windows stay hidden");
    check(api->shutdown(),0,"renderer shutdown");
    texture->Release();back_a->Release();at_b->Release();device->Release();d3d->Release();
}
}
int main(int argc,char** argv) {
    need(argc==5,"usage: fixture MODE BACKEND DISPLAY_DLL RENDERER_DLL");
    run(argv[1],argv[2],argv[3],argv[4]);
    std::printf("PASS refit window %s %s\n",argv[1],argv[2]);
}
