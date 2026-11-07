// Hidden, nonactivating native D3D8 provider fixture; no input or game process.
#include "../renderer/cloak/refraction.hpp"
#include <windows.h>
#include <d3d8.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

extern "C" bool cloak_d3d9_probe(void*, bool);
namespace {
template<class T> struct Owned { T* p = nullptr; ~Owned(){if(p)p->Release();}
    T** out(){return &p;} T* operator->(){return p;} };
[[noreturn]] void fail(const char* message, HRESULT hr = E_FAIL) {
    std::fprintf(stderr,"FAIL: %s HRESULT=0x%08lx\n",message,static_cast<unsigned long>(hr));
    std::exit(1);
}
void check(HRESULT hr,const char* text){if(FAILED(hr))fail(text,hr);}
struct Vertex {float pos[3], normal[3], uv[2], tx[3], ty[3], tz[3];};
static_assert(sizeof(Vertex)==68);
class UnsupportedDevice final : public IUnknown {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** out) override {
        if(out)*out=nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
};
void dump_ppm(IDirect3DDevice8* device,const std::string& path){
    Owned<IDirect3DSurface8> back;
    check(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,back.out()),"PPM GetBackBuffer");
    D3DLOCKED_RECT lock{};
    check(back->LockRect(&lock,nullptr,D3DLOCK_READONLY),"PPM LockRect");
    FILE* file=std::fopen(path.c_str(),"wb");
    if(!file)fail("open PPM output");
    if(std::fprintf(file,"P6\n96 96\n255\n")<0)fail("write PPM header");
    for(unsigned y=0;y<96;++y)for(unsigned x=0;x<96;++x){
        DWORD value=0;
        std::memcpy(&value,static_cast<const unsigned char*>(lock.pBits)+y*lock.Pitch+x*4,4);
        const unsigned char rgb[3]{static_cast<unsigned char>(value>>16),
            static_cast<unsigned char>(value>>8),static_cast<unsigned char>(value)};
        if(std::fwrite(rgb,1,3,file)!=3)fail("write PPM pixel");
    }
    if(std::fclose(file)!=0)fail("close PPM output");
    check(back->UnlockRect(),"PPM UnlockRect");
}
bool close_rgb(DWORD a,DWORD b){
    for(unsigned shift: {0u,8u,16u}){
        const int delta=int((a>>shift)&255)-int((b>>shift)&255);
        if(delta< -1 || delta>1)return false;
    }
    return true;
}
DWORD pixel(IDirect3DDevice8* device,unsigned x,unsigned y){
    Owned<IDirect3DSurface8> back;
    check(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,back.out()),"GetBackBuffer");
    D3DLOCKED_RECT lock{};
    check(back->LockRect(&lock,nullptr,D3DLOCK_READONLY),"LockRect");
    DWORD value=0;
    std::memcpy(&value,static_cast<const unsigned char*>(lock.pBits)+y*lock.Pitch+x*4,4);
    check(back->UnlockRect(),"UnlockRect");
    return value;
}
}
int main(int argc,char** argv){
    using Create=IDirect3D8*(WINAPI*)(UINT);
    HMODULE provider=nullptr;
    if(argc==3 && std::strcmp(argv[1],"--d3d8-provider-root")==0){
        char path[MAX_PATH]{};
        std::snprintf(path,sizeof(path),"%s\\d3d8.dll",argv[2]);
        provider=LoadLibraryA(path);
        if(!provider)fail("LoadLibrary provider",HRESULT_FROM_WIN32(GetLastError()));
    } else provider=LoadLibraryA("d3d8.dll");
    if(!provider)fail("LoadLibrary d3d8",HRESULT_FROM_WIN32(GetLastError()));
    const auto proc=GetProcAddress(provider,"Direct3DCreate8");
    Create create=nullptr;static_assert(sizeof(create)==sizeof(proc));
    std::memcpy(&create,&proc,sizeof(create));
    if(!create)fail("Direct3DCreate8 export");
    HINSTANCE instance=GetModuleHandleA(nullptr);
    WNDCLASSA wc{}; wc.lpfnWndProc=DefWindowProcA; wc.hInstance=instance;
    wc.lpszClassName="A2FOPrivateCloakFixture";
    if(!RegisterClassA(&wc))fail("RegisterClass");
    HWND window=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,wc.lpszClassName,
                  "",WS_POPUP,0,0,96,96,nullptr,nullptr,instance,nullptr);
    if(!window)fail("CreateWindowEx");
    Owned<IDirect3D8> factory; factory.p=create(D3D_SDK_VERSION);
    if(!factory.p)fail("Direct3DCreate8");
    D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=pp.BackBufferHeight=96;
    pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferCount=1;
    pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.Windowed=TRUE;pp.hDeviceWindow=window;
    pp.Flags=D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    Owned<IDirect3DDevice8> device;
    check(factory->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                &pp,device.out()),"D3D8 HAL hardware CreateDevice");
    Owned<IDirect3DVertexBuffer8> vb;
    Owned<IDirect3DIndexBuffer8> ib;
    Owned<IDirect3DTexture8> tex;
    const std::array<Vertex,4> quad{{
      {{-.7f,-.7f,.5f},{1,0,0},{0,1},{1,0,0},{0,1,0},{0,0,1}},
      {{ .7f,-.7f,.5f},{1,0,0},{1,1},{1,0,0},{0,1,0},{0,0,1}},
      {{ .7f, .7f,.5f},{1,0,0},{1,0},{1,0,0},{0,1,0},{0,0,1}},
      {{-.7f, .7f,.5f},{1,0,0},{0,0},{1,0,0},{0,1,0},{0,0,1}} }};
    const WORD order[6]{0,1,2,0,2,3};
    check(device->CreateVertexBuffer(sizeof(quad),0,0,D3DPOOL_MANAGED,vb.out()),"CreateVertexBuffer");
    check(device->CreateIndexBuffer(sizeof(order),0,D3DFMT_INDEX16,D3DPOOL_MANAGED,ib.out()),"CreateIndexBuffer");
    BYTE* data=nullptr;
    check(vb->Lock(0,sizeof(quad),&data,0),"Lock vertex buffer");
    std::memcpy(data,quad.data(),sizeof(quad));check(vb->Unlock(),"Unlock vertex buffer");
    check(ib->Lock(0,sizeof(order),&data,0),"Lock index buffer");
    std::memcpy(data,order,sizeof(order));check(ib->Unlock(),"Unlock index buffer");
    check(device->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,tex.out()),"Create hull texture");
    D3DLOCKED_RECT lock{};check(tex->LockRect(0,&lock,nullptr,0),"Lock hull texture");
    for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x)
        *reinterpret_cast<DWORD*>(static_cast<BYTE*>(lock.pBits)+y*lock.Pitch+x*4)=0xff909090;
    check(tex->UnlockRect(0),"Unlock hull texture");
    check(device->SetStreamSource(0,vb.p,sizeof(Vertex)),"SetStreamSource");
    check(device->SetIndices(ib.p,0),"SetIndices");
    check(device->SetTexture(0,tex.p),"SetTexture 0");
    check(device->SetRenderState(D3DRS_ZENABLE,FALSE),"ZENABLE");
    check(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"CULLMODE");
    check(device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE),"ALPHABLENDENABLE");
    D3DVIEWPORT8 vp{16,16,64,64,0,1};check(device->SetViewport(&vp),"SetViewport");
    check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff102040,1,0),"Clear");
    for(int i=0;i<64;++i){
        const D3DRECT stripe{16+i,16,17+i,80};
        const DWORD red=static_cast<DWORD>(24+i*3);
        const DWORD blue=static_cast<DWORD>((i/4)%2 ? 176 : 32);
        check(device->Clear(1,&stripe,D3DCLEAR_TARGET,0xff000000|(red<<16)|(0x40<<8)|blue,1,0),
              "Clear patterned column");
    }
    a2fo::cloak::Parameters params{};
    params.clip[0]=params.clip[5]=params.clip[10]=params.clip[15]=1;
    params.lights[18][0]=params.lights[18][1]=params.lights[18][2]=1;
    params.visibility=0.0f;params.hull_floor=0.0f;
    params.rim=0.0f;params.front=0.0f;
    params.bend=0.7f;params.shimmer=0.2f;
    params.snapshot_key=1;
    a2fo::cloak::Draw draw{};draw.num_vertices=4;draw.primitive_count=2;
    a2fo::cloak::Refraction effect;
    UnsupportedDevice unsupported;
    if(effect.draw(&unsupported,params,draw)!=a2fo::cloak::Outcome::fallback ||
       std::strstr(effect.diagnostic(),"QueryInterface")==nullptr)
        fail("unsupported IUnknown fallback");
    auto invalid=params;
    invalid.shimmer=std::numeric_limits<float>::quiet_NaN();
    if(effect.draw(device.p,invalid,draw)!=a2fo::cloak::Outcome::fallback)
        fail("nonfinite parameter fallback");
    const std::string output=(argc>=2)?std::string(argv[1]):std::string(".");
    dump_ppm(device.p,output+"/cloak-baseline.ppm");
    const std::array<unsigned,9> samples{{32,36,40,44,48,52,56,60,64}};
    std::array<DWORD,9> before{};
    for(unsigned i=0;i<samples.size();++i)before[i]=pixel(device.p,samples[i],48);
    if(!cloak_d3d9_probe(device.p,false))fail("D3D8 to D3D9 bridge/SM3");
    check(device->BeginScene(),"BeginScene");
    const auto drawn=effect.draw(device.p,params,draw);
    check(device->EndScene(),"EndScene");
    if(drawn!=a2fo::cloak::Outcome::drawn)fail(effect.diagnostic());
    if(!cloak_d3d9_probe(device.p,true))fail("D3D9 restoration");
    DWORD blend=0;check(device->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend),"D3D8 blend");
    if(blend!=TRUE)fail("D3D8 state restoration");
    dump_ppm(device.p,output+"/cloak-refracted.ppm");
    std::array<DWORD,9> after{};
    unsigned changed=0;
    for(unsigned i=0;i<samples.size();++i){
        after[i]=pixel(device.p,samples[i],48);
        changed+=before[i]!=after[i];
    }
    std::printf("Cloak sample changes=%u/9; centre before=%08lx after=%08lx\n",
                changed,static_cast<unsigned long>(before[4]),static_cast<unsigned long>(after[4]));
    if(changed==0)fail("no visible replacement effect");
    check(device->SetStreamSource(0,vb.p,64),"Set invalid stride");
    if(effect.draw(device.p,params,draw)!=a2fo::cloak::Outcome::fallback ||
       std::strstr(effect.diagnostic(),"bound stream")==nullptr)
        fail("invalid stride fallback");
    check(device->SetStreamSource(0,vb.p,sizeof(Vertex)),"Restore valid stride");
    check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0),"Clear before no-bend control");
    params.bend=0.0f;params.shimmer=0.0f;
    check(device->BeginScene(),"BeginScene no-bend control");
    const auto control=effect.draw(device.p,params,draw);
    check(device->EndScene(),"EndScene no-bend control");
    if(control!=a2fo::cloak::Outcome::drawn)fail(effect.diagnostic());
    dump_ppm(device.p,output+"/cloak-control.ppm");
    unsigned distortion_changes=0;
    for(unsigned i=0;i<samples.size();++i){
        const DWORD control_pixel=pixel(device.p,samples[i],48);
        if(!close_rgb(control_pixel,before[i]))
            fail("zero-distortion glass differs from original scene RGB");
        distortion_changes+=control_pixel!=after[i];
    }
    if(distortion_changes==0)fail("bend/shimmer did not change scene sample");
    params.bend=0.7f;params.shimmer=0.2f;
    check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0),"Clear after first group");
    check(device->BeginScene(),"BeginScene same key");
    const auto same=effect.draw(device.p,params,draw);
    check(device->EndScene(),"EndScene same key");
    if(same!=a2fo::cloak::Outcome::drawn)fail(effect.diagnostic());
    for(unsigned i=0;i<samples.size();++i)
        if(pixel(device.p,samples[i],48)!=after[i])
            fail("same-key group did not reuse pre-hull scene");
    params.snapshot_key=2;
    check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0),"Clear before new key");
    check(device->BeginScene(),"BeginScene new key");
    const auto fresh=effect.draw(device.p,params,draw);
    check(device->EndScene(),"EndScene new key");
    if(fresh!=a2fo::cloak::Outcome::drawn)fail(effect.diagnostic());
    unsigned refreshed=0;
    for(unsigned i=0;i<samples.size();++i)
        refreshed+=pixel(device.p,samples[i],48)!=after[i];
    if(refreshed==0)fail("new key did not refresh scene copy");
    params.visibility=0.5f;params.hull_floor=0.1f;
    params.rim=0.35f;params.front=1.0f;
    params.snapshot_key=3;
    check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0),"Clear before transition");
    check(device->BeginScene(),"BeginScene transition");
    const auto transition=effect.draw(device.p,params,draw);
    check(device->EndScene(),"EndScene transition");
    if(transition!=a2fo::cloak::Outcome::drawn)fail(effect.diagnostic());
    dump_ppm(device.p,output+"/cloak-transition.ppm");
    if(pixel(device.p,48,48)==0xff000000)fail("visibility transition has no hull contribution");
    effect.release();
    check(device->Reset(&pp),"D3D8 Reset after release");
    check(device->SetStreamSource(0,vb.p,sizeof(Vertex)),"post-reset stream");
    check(device->SetIndices(ib.p,0),"post-reset indices");
    check(device->SetTexture(0,tex.p),"post-reset texture");
    check(device->SetViewport(&vp),"post-reset viewport");
    check(device->SetRenderState(D3DRS_ZENABLE,FALSE),"post-reset z");
    check(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"post-reset cull");
    check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff102040,1,0),"post-reset clear");
    params.snapshot_key=4;
    check(device->BeginScene(),"post-reset BeginScene");
    const auto after_reset=effect.draw(device.p,params,draw);
    check(device->EndScene(),"post-reset EndScene");
    if(after_reset!=a2fo::cloak::Outcome::drawn)fail(effect.diagnostic());
    effect.release();
    device->SetTexture(0,nullptr);
    device->SetStreamSource(0,nullptr,0);
    device->SetIndices(nullptr,0);
    tex.p->Release();tex.p=nullptr;
    vb.p->Release();vb.p=nullptr;
    ib.p->Release();ib.p=nullptr;
    device.p->Release();device.p=nullptr;
    factory.p->Release();factory.p=nullptr;
    DestroyWindow(window);UnregisterClassA(wc.lpszClassName,instance);
    FreeLibrary(provider);
    std::puts("PASS: Refit cloak refraction: bridge, SM3 draw, distortion, fallback, state, snapshot, reset redraw");
    std::fflush(stdout);
    return 0;
}
