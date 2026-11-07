// Paired installed D3D8 provider / translated game-device alpha-stage probe.
// The private window stays hidden; no game process or input is used.
#include "../renderer/d3d8_compat/game/factory.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
template<class T> struct Owned {
    T* value=nullptr;
    ~Owned(){if(value)value->Release();}
    T** out(){return &value;}
    T* operator->()const{return value;}
};
struct Window {
    HWND value=nullptr; HINSTANCE instance=nullptr; const char* name=nullptr;
    ~Window(){if(value)DestroyWindow(value);if(name)UnregisterClassA(name,instance);}
};
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
void checked(HRESULT hr,const char* what){
    if(FAILED(hr)){char msg[160];std::snprintf(msg,sizeof(msg),"%s hr=0x%08lx",what,
        static_cast<unsigned long>(hr));throw std::runtime_error(msg);}
}
struct Vertex {float x,y,z,rhw; DWORD diffuse; float u,v;};
constexpr DWORD kFvf=D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1;
constexpr DWORD kBackground=0xff204060;
constexpr std::array<DWORD,3> kOps{{D3DTOP_SELECTARG1,D3DTOP_MODULATE,D3DTOP_DOTPRODUCT3}};
struct Case {int stage; DWORD op; bool textured; bool blend; bool dest_color=false; bool resume_alpha=false; bool all_alpha_disabled=false;};
struct Result {HRESULT set=S_OK,get=S_OK,draw=E_FAIL; DWORD observed=0,pixel=0; bool read=false;};

DWORD center_pixel(IDirect3DDevice8* device){
    Owned<IDirect3DSurface8> back;
    checked(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,back.out()),"GetBackBuffer");
    D3DLOCKED_RECT lock{};
    checked(back->LockRect(&lock,nullptr,D3DLOCK_READONLY),"backbuffer LockRect");
    DWORD pixel=0;
    std::memcpy(&pixel,static_cast<const unsigned char*>(lock.pBits)+32*lock.Pitch+32*4,4);
    checked(back->UnlockRect(),"backbuffer UnlockRect");
    return pixel;
}
void set_stage(IDirect3DDevice8* d,DWORD stage,DWORD color,DWORD arg1,DWORD arg2,DWORD alpha,
               bool defer_alpha){
    checked(d->SetTextureStageState(stage,D3DTSS_COLOROP,color),"Set COLOROP");
    checked(d->SetTextureStageState(stage,D3DTSS_COLORARG1,arg1),"Set COLORARG1");
    checked(d->SetTextureStageState(stage,D3DTSS_COLORARG2,arg2),"Set COLORARG2");
    if(!defer_alpha)checked(d->SetTextureStageState(stage,D3DTSS_ALPHAOP,alpha),"Set ALPHAOP");
    checked(d->SetTextureStageState(stage,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE),"Set ALPHAARG1");
    checked(d->SetTextureStageState(stage,D3DTSS_ALPHAARG2,D3DTA_CURRENT),"Set ALPHAARG2");
    checked(d->SetTextureStageState(stage,D3DTSS_MINFILTER,D3DTEXF_POINT),"Set MINFILTER");
    checked(d->SetTextureStageState(stage,D3DTSS_MAGFILTER,D3DTEXF_POINT),"Set MAGFILTER");
}
Result probe(IDirect3DDevice8* d,IDirect3DTexture8* texture,const Case& c){
    // Explicitly reset every stage between cases so the only variable is this case.
    for(DWORD s=0;s<3;++s){
        const bool active=static_cast<int>(s)<=c.stage;
        const bool target=static_cast<int>(s)==c.stage;
        const DWORD arg1=c.textured?D3DTA_TEXTURE:D3DTA_DIFFUSE;
        set_stage(d,s,active?(target?c.op:DWORD(D3DTOP_SELECTARG1)):DWORD(D3DTOP_DISABLE),
                  target?arg1:D3DTA_DIFFUSE,D3DTA_CURRENT,
                  active?(target?(c.resume_alpha?D3DTOP_SELECTARG1:D3DTOP_DISABLE):
                          ((c.resume_alpha||c.all_alpha_disabled)?D3DTOP_DISABLE:D3DTOP_SELECTARG1)):
                          D3DTOP_DISABLE,target);
        checked(d->SetTextureStageState(s,D3DTSS_TEXCOORDINDEX,0),"Set TEXCOORDINDEX");
        checked(d->SetTexture(s,active&&c.textured?texture:nullptr),"SetTexture");
    }
    checked(d->SetTextureStageState(3,D3DTSS_COLOROP,D3DTOP_DISABLE),"disable stage 3");
    checked(d->SetRenderState(D3DRS_ALPHABLENDENABLE,c.blend?TRUE:FALSE),"ALPHABLENDENABLE");
    checked(d->SetRenderState(D3DRS_SRCBLEND,c.dest_color?D3DBLEND_DESTCOLOR:D3DBLEND_SRCALPHA),"SRCBLEND");
    checked(d->SetRenderState(D3DRS_DESTBLEND,c.dest_color?D3DBLEND_ONE:D3DBLEND_INVSRCALPHA),"DESTBLEND");
    Result r;
    const DWORD target_alpha=c.resume_alpha?D3DTOP_SELECTARG1:D3DTOP_DISABLE;
    r.set=d->SetTextureStageState(c.stage,D3DTSS_ALPHAOP,target_alpha);
    r.get=d->GetTextureStageState(c.stage,D3DTSS_ALPHAOP,&r.observed);
    checked(d->Clear(0,nullptr,D3DCLEAR_TARGET,kBackground,1.0f,0),"Clear");
    const Vertex quad[4]={{-0.5f,-0.5f,0.5f,1,0x6090b0e0,0,0},
                          {63.5f,-0.5f,0.5f,1,0x6090b0e0,1,0},
                          {-0.5f,63.5f,0.5f,1,0x6090b0e0,0,1},
                          {63.5f,63.5f,0.5f,1,0x6090b0e0,1,1}};
    checked(d->BeginScene(),"BeginScene");
    r.draw=d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex));
    checked(d->EndScene(),"EndScene");
    if(SUCCEEDED(r.draw)){r.pixel=center_pixel(d);r.read=true;}
    return r;
}
void print_result(const char* provider,const Case& c,const Result& r){
    std::printf("provider=%s stage=%d color_op=%lu textured=%d blend=%d dest_color=%d resume_alpha=%d all_alpha_disabled=%d alpha_set=0x%08lx get=0x%08lx observed=%lu draw=0x%08lx read=%d pixel=0x%08lx\n",
        provider,c.stage,static_cast<unsigned long>(c.op),c.textured,c.blend,c.dest_color,c.resume_alpha,c.all_alpha_disabled,
        static_cast<unsigned long>(r.set),static_cast<unsigned long>(r.get),
        static_cast<unsigned long>(r.observed),static_cast<unsigned long>(r.draw),
        r.read,static_cast<unsigned long>(r.pixel));
    std::fflush(stdout);
}
bool close_rgb(DWORD a,DWORD b){
    for(int shift: {0,8,16}){
        const int delta=static_cast<int>((a>>shift)&255)-static_cast<int>((b>>shift)&255);
        if(delta < -2 || delta > 2)return false;
    }
    return true;
}
void prepare(IDirect3DDevice8* d){
    checked(d->SetVertexShader(kFvf),"SetVertexShader FVF");
    checked(d->SetPixelShader(0),"SetPixelShader fixed function");
    checked(d->SetRenderState(D3DRS_LIGHTING,FALSE),"LIGHTING");
    checked(d->SetRenderState(D3DRS_ZENABLE,FALSE),"ZENABLE");
    checked(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"CULLMODE");
}
void run(const char* output){
    WNDCLASSEXA cls{};cls.cbSize=sizeof(cls);cls.hInstance=GetModuleHandleA(nullptr);
    cls.lpfnWndProc=DefWindowProcA;cls.lpszClassName="A2FOAlphaNativeFixture";
    require(RegisterClassExA(&cls)!=0,"register private window class");
    Window window;window.instance=cls.hInstance;window.name=cls.lpszClassName;
    window.value=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,window.name,
        "private alpha fixture",WS_POPUP,0,0,64,64,nullptr,nullptr,cls.hInstance,nullptr);
    require(window.value&&!IsWindowVisible(window.value),"hidden current-thread HWND");
    Owned<IDirect3D8> native;native.value=Direct3DCreate8(D3D_SDK_VERSION);
    require(native.value!=nullptr,"Direct3DCreate8 installed provider");
    a2fo::renderer::game::Configuration config;
    config.mod_root=output;config.shader_cache=std::string(output)+"/cache";config.vsync=false;
    config.bc_assets=false; // Alpha-state probe has no converted DDS catalog.
    require(CreateDirectoryA(config.shader_cache.c_str(),nullptr)||GetLastError()==ERROR_ALREADY_EXISTS,
            "private shader cache");
    Owned<IDirect3D8> facade;facade.value=a2fo::renderer::game::create_factory(native.value,config);
    require(facade.value!=nullptr,"translated create_factory");
    D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=pp.BackBufferHeight=64;
    pp.BackBufferCount=1;pp.BackBufferFormat=D3DFMT_A8R8G8B8;
    pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window.value;pp.Windowed=TRUE;
    pp.Flags=D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    D3DPRESENT_PARAMETERS facade_pp=pp;
    Owned<IDirect3DDevice8> devices[2];
    checked(native->CreateDevice(0,D3DDEVTYPE_HAL,window.value,D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &pp,devices[0].out()),"native CreateDevice");
    checked(facade->CreateDevice(0,D3DDEVTYPE_HAL,window.value,D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &facade_pp,devices[1].out()),"facade CreateDevice");
    Owned<IDirect3DTexture8> textures[2];
    for(int i=0;i<2;++i){
        prepare(devices[i].value);
        checked(devices[i]->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,
            textures[i].out()),"CreateTexture");
        D3DLOCKED_RECT lock{};
        checked(textures[i]->LockRect(0,&lock,nullptr,0),"texture LockRect");
        const DWORD texel=0x80c04020;
        std::memcpy(lock.pBits,&texel,sizeof(texel));
        checked(textures[i]->UnlockRect(0),"texture UnlockRect");
    }
    std::vector<Case> cases;
    for(int stage=0;stage<=2;++stage)for(DWORD op:kOps)
        for(bool textured:{false,true})for(bool blend:{false,true})
            cases.push_back({stage,op,textured,blend});
    cases.push_back({0,D3DTOP_SELECTARG1,false,true,true,false});
    cases.push_back({1,D3DTOP_SELECTARG1,false,true,true,false,true});
    cases.push_back({1,D3DTOP_SELECTARG1,true,true,false,true});
    std::vector<Result> goldens;
    std::size_t changed=0,native_invalid=0;
    for(const Case& c:cases){
        Result r=probe(devices[0].value,textures[0].value,c);
        print_result("native",c,r);
        const DWORD wanted=c.resume_alpha?D3DTOP_SELECTARG1:D3DTOP_DISABLE;
        if(FAILED(r.set)||FAILED(r.get)||r.observed!=wanted||FAILED(r.draw)||!r.read)
            ++native_invalid;
        if(r.read&&(r.pixel&0xffffff)!=(kBackground&0xffffff))++changed;
        goldens.push_back(r);
    }
    std::printf("native cases=%lu invalid=%lu covered=%lu\n",
        static_cast<unsigned long>(cases.size()),static_cast<unsigned long>(native_invalid),
        static_cast<unsigned long>(changed));
    require(native_invalid==0,"native provider did not accept all alpha cases; inspect goldens");
    require(changed>=6,"native reference did not produce varied covered pixels");
    std::size_t mismatch=0;
    for(std::size_t i=0;i<cases.size();++i){
        Result r=probe(devices[1].value,textures[1].value,cases[i]);
        print_result("facade",cases[i],r);
        const Result& g=goldens[i];
        if(FAILED(r.set)||FAILED(r.get)||r.observed!=g.observed||FAILED(r.draw)||
           !r.read||!close_rgb(r.pixel,g.pixel)){
            ++mismatch;
            std::printf("mismatch case=%lu native=0x%08lx facade=0x%08lx\n",
                static_cast<unsigned long>(i),static_cast<unsigned long>(g.pixel),
                static_cast<unsigned long>(r.pixel));
        }
    }
    std::printf("alpha cases=%lu native_covered=%lu mismatches=%lu\n",
        static_cast<unsigned long>(cases.size()),static_cast<unsigned long>(changed),
        static_cast<unsigned long>(mismatch));
    require(mismatch==0,"facade alpha results differ from installed native D3D8 provider");
    std::puts("PASS: paired native D3D8 alpha stage pixels");
}
}
int main(int argc,char** argv){try{
    require(argc==2,"usage: renderer_alpha_native.exe OUTPUT_DIR");run(argv[1]);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
