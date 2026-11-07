// Native D3D8 sampler probe. Keep the window private, hidden, and nonactivating.
#include <d3d8.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
template<class T> struct Owned {
    T* value=nullptr;
    ~Owned(){if(value)value->Release();}
    T** out(){return &value;}
    T* operator->()const{return value;}
    ULONG finish(){if(!value)return 0;T* p=value;value=nullptr;return p->Release();}
};
struct Window {
    HWND value=nullptr;
    HINSTANCE instance=nullptr;
    const char* name=nullptr;
    ~Window(){if(value)DestroyWindow(value);if(name)UnregisterClassA(name,instance);}
};
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
void checked(HRESULT hr,const char* what){
    if(FAILED(hr)){
        char message[160];
        std::snprintf(message,sizeof(message),"%s HRESULT=0x%08lx",what,
                      static_cast<unsigned long>(hr));
        throw std::runtime_error(message);
    }
}
struct Vertex {float x,y,z,rhw;DWORD diffuse;float u,v;};
constexpr DWORD fvf=D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1;
constexpr std::array<DWORD,5> colors{{0xffff0000,0xff00ff00,0xff0000ff,
                                       0xffffffff,0xff000000}};
void fill_texture(IDirect3DTexture8* texture){
    for(UINT level=0;level<colors.size();++level){
        D3DLOCKED_RECT locked{};
        checked(texture->LockRect(level,&locked,nullptr,0),"texture LockRect");
        const UINT side=16u>>level;
        for(UINT y=0;y<side;++y)for(UINT x=0;x<side;++x)
            std::memcpy(static_cast<unsigned char*>(locked.pBits)+y*locked.Pitch+x*4,
                        &colors[level],4);
        checked(texture->UnlockRect(level),"texture UnlockRect");
    }
}
DWORD center_pixel(IDirect3DDevice8* device){
    Owned<IDirect3DSurface8> back;
    checked(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,back.out()),"GetBackBuffer");
    D3DLOCKED_RECT locked{};
    checked(back->LockRect(&locked,nullptr,D3DLOCK_READONLY),"backbuffer LockRect");
    DWORD pixel=0;
    std::memcpy(&pixel,static_cast<const unsigned char*>(locked.pBits)+32*locked.Pitch+32*4,4);
    checked(back->UnlockRect(),"backbuffer UnlockRect");
    require(back.finish()==0,"backbuffer Release zero");
    return pixel;
}
void run_case(IDirect3DDevice8* device,const char* name,DWORD minmag,DWORD anisotropy,DWORD mip){
    checked(device->SetTextureStageState(0,D3DTSS_MINFILTER,minmag),"Set MINFILTER");
    checked(device->SetTextureStageState(0,D3DTSS_MAGFILTER,minmag),"Set MAGFILTER");
    checked(device->SetTextureStageState(0,D3DTSS_MAXANISOTROPY,anisotropy),"Set MAXANISOTROPY");
    const HRESULT set=device->SetTextureStageState(0,D3DTSS_MIPFILTER,mip);
    DWORD observed=0xffffffff;
    const HRESULT get=device->GetTextureStageState(0,D3DTSS_MIPFILTER,&observed);
    if(mip!=D3DTEXF_ANISOTROPIC){
        checked(set,"Set base MIPFILTER");
        checked(get,"Get base MIPFILTER");
    }
    HRESULT draw=E_FAIL;
    HRESULT read=E_FAIL;
    DWORD pixel=0;
    if(SUCCEEDED(set)){
        checked(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff404040,1.0f,0),"Clear");
        const Vertex quad[4]={{-0.5f,-0.5f,0.5f,1.0f,0xffffffff,0,0},
                              {63.5f,-0.5f,0.5f,1.0f,0xffffffff,1,0},
                              {-0.5f,63.5f,0.5f,1.0f,0xffffffff,0,1},
                              {63.5f,63.5f,0.5f,1.0f,0xffffffff,1,1}};
        checked(device->BeginScene(),"BeginScene");
        draw=device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex));
        const HRESULT end=device->EndScene();
        checked(end,"EndScene");
        if(SUCCEEDED(draw)){
            // The readback API's HRESULT is represented by this successful call.
            pixel=center_pixel(device);
            read=S_OK;
        }
    }
    std::printf("case=%s minmag=%lu max_anisotropy=%lu mip=%lu set=0x%08lx get=0x%08lx observed=%lu draw=0x%08lx read=0x%08lx pixel=0x%08lx\n",
                name,static_cast<unsigned long>(minmag),static_cast<unsigned long>(anisotropy),
                static_cast<unsigned long>(mip),static_cast<unsigned long>(set),
                static_cast<unsigned long>(get),static_cast<unsigned long>(observed),
                static_cast<unsigned long>(draw),static_cast<unsigned long>(read),
                static_cast<unsigned long>(pixel));
    if(mip!=D3DTEXF_ANISOTROPIC || SUCCEEDED(set)){
        checked(draw,"DrawPrimitiveUP");
        checked(read,"center pixel readback");
        require(pixel!=0xff404040,"center pixel unchanged after draw");
    }
}
void run(){
    WNDCLASSEXA cls{};cls.cbSize=sizeof(cls);cls.hInstance=GetModuleHandleA(nullptr);
    cls.lpfnWndProc=DefWindowProcA;cls.lpszClassName="A2FOSamplerNativeFixture";
    require(RegisterClassExA(&cls)!=0,"register private window class");
    Window window;window.instance=cls.hInstance;window.name=cls.lpszClassName;
    window.value=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,window.name,
        "private sampler fixture",WS_POPUP,0,0,64,64,nullptr,nullptr,cls.hInstance,nullptr);
    require(window.value && !IsWindowVisible(window.value),"hidden current-thread HWND");
    {
        Owned<IDirect3D8> factory;factory.value=Direct3DCreate8(D3D_SDK_VERSION);
        require(factory.value!=nullptr,"Direct3DCreate8");
        D3DPRESENT_PARAMETERS params{};
        params.BackBufferWidth=params.BackBufferHeight=64;params.BackBufferCount=1;
        params.BackBufferFormat=D3DFMT_A8R8G8B8;params.SwapEffect=D3DSWAPEFFECT_DISCARD;
        params.hDeviceWindow=window.value;params.Windowed=TRUE;
        params.Flags=D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
        Owned<IDirect3DDevice8> device;
        checked(factory->CreateDevice(0,D3DDEVTYPE_HAL,window.value,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,device.out()),"CreateDevice");
        Owned<IDirect3DTexture8> texture;
        checked(device->CreateTexture(16,16,5,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,
                                      texture.out()),"CreateTexture five mips");
        fill_texture(texture.value);
        checked(device->SetTexture(0,texture.value),"SetTexture");
        checked(device->SetVertexShader(fvf),"SetVertexShader FVF");
        checked(device->SetPixelShader(0),"SetPixelShader fixed function");
        checked(device->SetRenderState(D3DRS_LIGHTING,FALSE),"disable lighting");
        checked(device->SetRenderState(D3DRS_ZENABLE,FALSE),"disable depth");
        checked(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"disable culling");
        checked(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"COLOROP");
        checked(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE),"COLORARG1");
        checked(device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1),"ALPHAOP");
        checked(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE),"ALPHAARG1");
        checked(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"disable stage 1");
        float bias=3.5f;DWORD bias_bits=0;std::memcpy(&bias_bits,&bias,sizeof(bias));
        checked(device->SetTextureStageState(0,D3DTSS_MIPMAPLODBIAS,bias_bits),"MIPMAPLODBIAS");
        for(DWORD mip=1;mip<=3;++mip)run_case(device.value,"linear_minmag_aniso1",D3DTEXF_LINEAR,1,mip);
        for(DWORD mip=1;mip<=3;++mip)run_case(device.value,"linear_minmag_aniso4",D3DTEXF_LINEAR,4,mip);
        for(DWORD mip=1;mip<=3;++mip)run_case(device.value,"aniso_minmag_aniso4",D3DTEXF_ANISOTROPIC,4,mip);
        checked(device->SetTexture(0,nullptr),"unbind texture");
        require(texture.finish()==0,"texture Release zero");
        require(device.finish()==0,"device Release zero");
        require(factory.finish()==0,"factory Release zero");
    }
    require(DestroyWindow(window.value)!=FALSE,"DestroyWindow");window.value=nullptr;
    require(UnregisterClassA(window.name,window.instance)!=FALSE,"UnregisterClass");window.name=nullptr;
    std::puts("PASS: native D3D8 sampler probe");
}
}
int main(){try{run();return 0;}
    catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
