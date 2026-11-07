#include "../renderer/refit/host.h"
#include <windows.h>
#include <d3d8.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static_assert(sizeof(void*)==4);
static_assert(sizeof(A2FORefitDraw2)==84);
static_assert(sizeof(A2FORefitCaller2)==12);
static_assert(sizeof(A2FORefitRaster2)==8);
static_assert(D3DFMT_INDEX16==101);

extern "C" {
std::uint32_t refit_probe_draw(IDirect3DDevice8*,std::uint32_t);
std::uint32_t refit_probe_before=0,refit_probe_after=0,refit_probe_result=0;
std::uint32_t refit_probe_ebx=0,refit_probe_esi=0,refit_probe_edi=0,refit_probe_ebp=0;
std::uint32_t refit_probe_expected_caller=0;
}

namespace {
constexpr std::uint32_t W=64,H=64;
void need(bool yes,const char* what){
    if(!yes){std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);}
}
void ok(HRESULT hr,const char* what){
    if(FAILED(hr))std::fprintf(stderr,"HRESULT %s: 0x%08lx\n",what,static_cast<unsigned long>(hr));
    need(SUCCEEDED(hr),what);
}
template<class F> F export_of(HMODULE module,const char* name){
    FARPROC raw=GetProcAddress(module,name);F function=nullptr;
    static_assert(sizeof(function)==sizeof(raw));
    std::memcpy(&function,&raw,sizeof(function));return function;
}
struct Window {
    HINSTANCE instance=GetModuleHandleA(nullptr);HWND hwnd=nullptr;
    Window(){
        WNDCLASSEXA c{};c.cbSize=sizeof(c);c.hInstance=instance;
        c.lpfnWndProc=DefWindowProcA;c.lpszClassName="A2FORefitDrawAbiPrivateFixture";
        need(RegisterClassExA(&c)!=0,"register private window class");
        hwnd=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,c.lpszClassName,
            "hidden draw ABI fixture",WS_POPUP,0,0,W,H,nullptr,nullptr,instance,nullptr);
        need(hwnd&&!IsWindowVisible(hwnd),"create hidden private HWND");
    }
    ~Window(){if(hwnd)DestroyWindow(hwnd);UnregisterClassA("A2FORefitDrawAbiPrivateFixture",instance);}
};
struct Case {std::uint32_t slot,count;std::array<std::uint32_t,8> args;};
struct WrongOwner {A2FORefitDisplayLastCaller2 query;A2FORefitDisplayRaster2 raster;
    std::int32_t status=0,raster_status=0;bool unchanged=false,raster_unchanged=false;};
DWORD WINAPI wrong_owner(LPVOID raw){
    auto& probe=*static_cast<WrongOwner*>(raw);
    A2FORefitCaller2 caller{sizeof(A2FORefitCaller2),0x11223344,0x55667788};
    const auto prior=caller;
    probe.status=probe.query(&caller);
    probe.unchanged=std::memcmp(&caller,&prior,sizeof(caller))==0;
    A2FORefitRaster2 raster{sizeof(raster),123.f};const auto prior_raster=raster;
    probe.raster_status=probe.raster(&raster);
    probe.raster_unchanged=std::memcmp(&raster,&prior_raster,sizeof(raster))==0;
    return 0;
}
constexpr Case cases[]={
    {70,3,{4,0,0}},
    {71,5,{4,0,0,0,0}},
    {72,4,{4,0,0,36}},
    {73,8,{4,0,0,0,0,101,0,36}},
};
void run(const char* backend,const char* display_path){
    const std::uint32_t backend_id=std::strcmp(backend,"vulkan")==0?1:2;
    need(backend_id==1||std::strcmp(backend,"d3d12")==0,"backend name");
    Window window;
    HMODULE display=LoadLibraryA(display_path);need(display,"load Display");
    auto configure=export_of<A2FORefitDisplayConfigure>(display,"A2FORefitDisplay_Configure_v1");
    auto last_draw=export_of<A2FORefitDisplayLastDraw2>(display,"A2FORefitDisplay_LastDraw_v2");
    auto last_caller=export_of<A2FORefitDisplayLastCaller2>(display,"A2FORefitDisplay_LastCaller_v2");
    auto raster=export_of<A2FORefitDisplayRaster2>(display,"A2FORefitDisplay_Raster_v2");
    using Create8=IDirect3D8* (WINAPI*)(UINT);
    auto create8=export_of<Create8>(display,"Direct3DCreate8");
    need(configure&&last_draw&&last_caller&&raster&&create8,"required Display exports");
    need(configure(backend_id,window.hwnd,W,H,0)==0,"configure private Display");
    A2FORefitRaster2 raster_value{sizeof(A2FORefitRaster2),123.f};
    need(raster(&raster_value)==0&&raster_value.size==sizeof(raster_value)&&
         raster_value.rhw_to_scene_offset==0.4921875f,"default native raster offset");
    A2FORefitRaster2 invalid_raster{0,123.f};const auto raster_prior=invalid_raster;
    need(raster(&invalid_raster)==A2FO_REFIT_INVALID&&
         std::memcmp(&invalid_raster,&raster_prior,sizeof(invalid_raster))==0,
         "wrong raster size rejects transactionally");
    need(raster(nullptr)==A2FO_REFIT_INVALID,"null raster output rejects");
    alignas(A2FORefitRaster2) unsigned char storage[sizeof(A2FORefitRaster2)+1]{};
    const A2FORefitRaster2 unaligned_prior{sizeof(A2FORefitRaster2),123.f};
    std::memcpy(storage+1,&unaligned_prior,sizeof(unaligned_prior));
    need(raster(reinterpret_cast<A2FORefitRaster2*>(storage+1))==A2FO_REFIT_INVALID&&
         std::memcmp(storage+1,&unaligned_prior,sizeof(unaligned_prior))==0,
         "misaligned raster output rejects transactionally");
    IDirect3D8* d3d=create8(D3D_SDK_VERSION);need(d3d,"create D3D8 facade");
    D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=W;pp.BackBufferHeight=H;
    pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferCount=1;
    pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window.hwnd;
    pp.Windowed=TRUE;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;
    IDirect3DDevice8* device=nullptr;
    ok(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window.hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                         &pp,&device),"create facade device");
    need(device,"facade device pointer");
    ok(device->SetVertexShader(D3DFVF_XYZ|D3DFVF_DIFFUSE),"set fixed XYZ FVF");
    ok(device->SetPixelShader(0),"set fixed pixel stage");
    ok(device->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE),"set alpha test");
    ok(device->SetRenderState(D3DRS_ALPHAREF,77),"set alpha reference");
    ok(device->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER),"set alpha comparison");
    ok(device->BeginScene(),"begin zero-primitive ABI probes");
    std::uint32_t previous_sequence=0;
    for(const auto& test:cases){
        need(refit_probe_draw(device,test.slot)==0,"zero-primitive draw HRESULT");
        need(refit_probe_result==0&&refit_probe_before==refit_probe_after,
             "stdcall draw balances caller stack");
        need(refit_probe_ebx==0x11223344&&refit_probe_esi==0x55667788&&
             refit_probe_edi==0x12345678&&refit_probe_ebp==0x89abcdef,
             "stdcall draw preserves nonvolatile registers");
        A2FORefitDraw2 draw{};draw.size=sizeof(draw);
        need(last_draw(&draw)==0,"read last owned draw");
        need(draw.size==sizeof(draw)&&draw.slot==test.slot&&
             draw.caller==refit_probe_expected_caller&&draw.caller!=0,
             "captured exact vtable slot and assembly return label");
        need(draw.arg_count==test.count&&draw.args[0]==test.args[0],
             "captured draw argument count and primitive type");
        for(std::uint32_t i=0;i<test.count;++i)
            need(draw.args[i]==test.args[i],"captured every original argument in order");
        for(std::uint32_t i=test.count;i<8;++i)
            need(draw.args[i]==0,"unused diagnostic argument slot is zero");
        need(draw.vertex_format==(D3DFVF_XYZ|D3DFVF_DIFFUSE)&&
             draw.viewport[0]==0&&draw.viewport[1]==0&&
             draw.viewport[2]==W&&draw.viewport[3]==H,
             "captured FVF and private viewport");
        need(draw.alpha_test==1&&draw.alpha_ref==77&&draw.alpha_func==D3DCMP_GREATER,
             "captured alpha-test state");
        need(draw.sequence>previous_sequence&&
             (previous_sequence==0||draw.sequence==previous_sequence+1),
             "one diagnostic sequence per draw");
        A2FORefitCaller2 caller{};caller.size=sizeof(caller);
        need(last_caller(&caller)==0&&caller.size==sizeof(caller)&&
             caller.sequence==draw.sequence&&caller.caller_esi==0x55667788,
             "caller telemetry matches draw sequence and assembly ESI canary");
        A2FORefitCaller2 invalid{0,0x12345678,0xabcdef01};
        const auto unchanged=invalid;
        need(last_caller(&invalid)==A2FO_REFIT_INVALID&&
             std::memcmp(&invalid,&unchanged,sizeof(invalid))==0,
             "wrong Caller2 size rejects without changing output");
        previous_sequence=draw.sequence;
    }
    WrongOwner foreign{last_caller,raster};
    HANDLE thread=CreateThread(nullptr,0,wrong_owner,&foreign,0,nullptr);
    need(thread&&WaitForSingleObject(thread,10000)==WAIT_OBJECT_0,"join wrong owner query");
    CloseHandle(thread);
    need(foreign.status==A2FO_REFIT_STATE&&foreign.unchanged,
         "wrong owner Caller2 query rejects transactionally");
    need(foreign.raster_status==A2FO_REFIT_STATE&&foreign.raster_unchanged,
         "wrong owner Raster2 query rejects transactionally");
    ok(device->EndScene(),"end zero-primitive ABI probes");
    device->Release();d3d->Release();
    need(!IsWindowVisible(window.hwnd),"private window remained hidden");
    std::printf("PASS: %s four D3D8 draw entries, stdcall stack/registers, caller and argument capture\n",backend);
}
}
int main(int argc,char** argv){
    if(argc!=3){std::fprintf(stderr,"usage: fixture BACKEND DISPLAY_DLL\n");return 2;}
    run(argv[1],argv[2]);return 0;
}
