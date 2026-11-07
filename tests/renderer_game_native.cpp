// Private D3D8 API fixture for the translated D3D11 game device. No visible
// window, native D3D8 device, game launch, or synthesized input.
#include "../renderer/d3d8_compat/game/factory.hpp"
#include "../renderer/d3d8_compat/game/bc_catalog.hpp"
#include "../renderer/textures/dds.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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
    void reset(){if(value){value->Release();value=nullptr;}}
    ULONG finish(){if(!value)return 0;T* p=value;value=nullptr;return p->Release();}
};
struct Module {
    HMODULE value=nullptr;
    ~Module(){if(value)FreeLibrary(value);}
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
        char message[200];std::snprintf(message,sizeof(message),"%s HRESULT=0x%08lx",
            what,static_cast<unsigned long>(hr));throw std::runtime_error(message);
    }
}
std::vector<std::string> messages;
void log_line(const std::string& line){messages.push_back(line);std::printf("%s\n",line.c_str());}
std::vector<std::uint8_t> read_file(const std::string& path){
    FILE* f=std::fopen(path.c_str(),"rb");require(f!=nullptr,"open fixture file");
    if(std::fseek(f,0,SEEK_END)!=0){std::fclose(f);throw std::runtime_error("seek fixture end");}
    const long length=std::ftell(f);
    if(length<=0 || length>64L*1024*1024 || std::fseek(f,0,SEEK_SET)!=0){
        std::fclose(f);throw std::runtime_error("fixture length/seek");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    const bool ok=std::fread(bytes.data(),1,bytes.size(),f)==bytes.size();
    const bool closed=std::fclose(f)==0;
    require(ok && closed,"read fixture file");return bytes;
}
std::vector<std::uint8_t> read_win32_file(const std::string& path){
    HANDLE file=CreateFileA(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,nullptr);
    require(file!=INVALID_HANDLE_VALUE,"CreateFile canonical DDS without fopen bridge");
    LARGE_INTEGER length{};
    if(!GetFileSizeEx(file,&length) || length.QuadPart<=0 ||
       length.QuadPart>64LL*1024*1024){CloseHandle(file);throw std::runtime_error("Win32 DDS size");}
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length.QuadPart));
    DWORD done=0;
    const bool ok=ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&done,nullptr)!=FALSE &&
                  done==bytes.size();
    const bool closed=CloseHandle(file)!=FALSE;
    require(ok && closed,"ReadFile canonical DDS");return bytes;
}
std::vector<std::uint8_t> back_pixels(IDirect3DDevice8* device,UINT width,UINT height){
    Owned<IDirect3DSurface8> surface;
    checked(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,surface.out()),"GetBackBuffer");
    D3DLOCKED_RECT lock{};
    checked(surface->LockRect(&lock,nullptr,D3DLOCK_READONLY),"backbuffer LockRect READONLY");
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width)*height*4);
    for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x){
        const auto* bgra=static_cast<const std::uint8_t*>(lock.pBits)+y*lock.Pitch+x*4;
        auto* dst=rgba.data()+(static_cast<std::size_t>(y)*width+x)*4;
        dst[0]=bgra[2];dst[1]=bgra[1];dst[2]=bgra[0];dst[3]=bgra[3];
    }
    checked(surface->UnlockRect(),"backbuffer UnlockRect");
    require(surface.finish()==0,"backbuffer public Release zero");
    return rgba;
}
std::vector<std::uint8_t> surface_pixels(IDirect3DSurface8* surface,UINT width,UINT height){
    D3DLOCKED_RECT lock{};
    checked(surface->LockRect(&lock,nullptr,D3DLOCK_READONLY),"surface LockRect READONLY");
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width)*height*4);
    for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x){
        const auto* bgra=static_cast<const std::uint8_t*>(lock.pBits)+y*lock.Pitch+x*4;
        auto* dst=rgba.data()+(static_cast<std::size_t>(y)*width+x)*4;
        dst[0]=bgra[2];dst[1]=bgra[1];dst[2]=bgra[0];dst[3]=bgra[3];
    }
    checked(surface->UnlockRect(),"surface UnlockRect");return rgba;
}
void pixel_is(const std::vector<std::uint8_t>& image,UINT width,UINT x,UINT y,
              std::array<unsigned,3> wanted,const char* what){
    const auto* p=image.data()+(static_cast<std::size_t>(y)*width+x)*4;
    for(UINT c=0;c<3;++c)if(std::abs(int(p[c])-int(wanted[c]))>2){
        char detail[180];std::snprintf(detail,sizeof(detail),"%s at %u,%u channel %u got %u wanted %u",
            what,x,y,c,p[c],wanted[c]);throw std::runtime_error(detail);
    }
}
struct Vertex {float x,y,z,rhw;DWORD color;};
struct TexVertex {float x,y,z,rhw;DWORD color;float u,v;};
constexpr DWORD vertex_fvf=D3DFVF_XYZRHW|D3DFVF_DIFFUSE;
void basic_state(IDirect3DDevice8* d){
    checked(d->SetRenderState(D3DRS_LIGHTING,FALSE),"disable lighting");
    checked(d->SetRenderState(D3DRS_ZENABLE,FALSE),"disable depth");
    checked(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"disable cull");
    checked(d->SetRenderState(D3DRS_FOGENABLE,FALSE),"disable fog");
    checked(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"colour op");
    checked(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE),"colour diffuse");
    checked(d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1),"alpha op");
    checked(d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE),"alpha diffuse");
    checked(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"disable stage1");
    checked(d->SetVertexShader(vertex_fvf),"vertex FVF");
    checked(d->SetPixelShader(0),"fixed pixel shader");
}
void clear(IDirect3DDevice8* d){
    checked(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                     0xff000000,1,0),"clear backbuffer/depth/stencil");
}
void exercise_geometry(IDirect3DDevice8* d){
    basic_state(d);clear(d);
    const Vertex tri[]={{8,8,.5f,1,0xffff0000},{56,8,.5f,1,0xffff0000},
                        {8,56,.5f,1,0xffff0000}};
    checked(d->BeginScene(),"BeginScene UP");
    checked(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,tri,sizeof(Vertex)),"DrawPrimitiveUP red");
    checked(d->EndScene(),"EndScene UP");
    pixel_is(back_pixels(d,64,64),64,18,18,{255,0,0},"red UP triangle");

    checked(d->SetRenderState(D3DRS_CLIPPING,FALSE),"disable clipping for RHW triangle");
    clear(d);checked(d->BeginScene(),"BeginScene clipping disabled");
    require(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,tri,sizeof(Vertex))==S_OK,
            "RHW triangle with clipping disabled draws S_OK");
    checked(d->EndScene(),"EndScene clipping disabled");
    pixel_is(back_pixels(d,64,64),64,18,18,{255,0,0},"red RHW triangle with clipping disabled");
    checked(d->SetRenderState(D3DRS_CLIPPING,TRUE),"restore clipping");

    checked(d->SetRenderState(D3DRS_POINTSPRITEENABLE,TRUE),"enable point sprites");
    checked(d->SetRenderState(D3DRS_POINTSCALEENABLE,TRUE),"enable point scaling");
    clear(d);checked(d->BeginScene(),"BeginScene triangle with point states");
    require(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,tri,sizeof(Vertex))==S_OK,
            "triangle list with point states draws S_OK");
    checked(d->EndScene(),"EndScene triangle with point states");
    pixel_is(back_pixels(d,64,64),64,18,18,{255,0,0},"red triangle with point states");
    checked(d->SetRenderState(D3DRS_POINTSPRITEENABLE,FALSE),"restore point sprites");
    checked(d->SetRenderState(D3DRS_POINTSCALEENABLE,FALSE),"restore point scaling");

    checked(d->SetRenderState(D3DRS_LIGHTING,TRUE),"enable lighting for RHW triangle");
    checked(d->SetRenderState(D3DRS_SPECULARENABLE,TRUE),"enable specular for RHW triangle");
    checked(d->SetRenderState(D3DRS_LOCALVIEWER,FALSE),"disable local viewer for RHW triangle");
    clear(d);checked(d->BeginScene(),"BeginScene RHW with lighting states");
    require(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,tri,sizeof(Vertex))==S_OK,
            "RHW triangle with lighting states draws S_OK");
    checked(d->EndScene(),"EndScene RHW with lighting states");
    pixel_is(back_pixels(d,64,64),64,18,18,{255,0,0},"red RHW triangle with lighting states");
    checked(d->SetRenderState(D3DRS_LIGHTING,FALSE),"restore lighting");
    checked(d->SetRenderState(D3DRS_SPECULARENABLE,FALSE),"restore specular");
    checked(d->SetRenderState(D3DRS_LOCALVIEWER,TRUE),"restore local viewer");

    Owned<IDirect3DVertexBuffer8> vb;Owned<IDirect3DIndexBuffer8> ib;
    checked(d->CreateVertexBuffer(4*sizeof(Vertex),0,vertex_fvf,D3DPOOL_MANAGED,vb.out()),"CreateVertexBuffer");
    checked(d->CreateIndexBuffer(3*sizeof(WORD),0,D3DFMT_INDEX16,D3DPOOL_MANAGED,ib.out()),"CreateIndexBuffer");
    const Vertex green[]={{8,8,.5f,1,0xff00ff00},{56,8,.5f,1,0xff00ff00},
                          {8,56,.5f,1,0xff00ff00}};
    BYTE* locked=nullptr;checked(vb->Lock(0,0,&locked,0),"VB Lock ordinary");
    require(locked!=nullptr,"VB lock pointer");std::memcpy(locked,green,sizeof(green));
    checked(vb->Unlock(),"VB Unlock");
    const WORD indices[]={0,1,2};locked=nullptr;
    checked(ib->Lock(0,0,&locked,0),"IB Lock ordinary");
    require(locked!=nullptr,"IB lock pointer");std::memcpy(locked,indices,sizeof(indices));
    checked(ib->Unlock(),"IB Unlock");
    checked(d->SetStreamSource(0,vb.value,sizeof(Vertex)),"SetStreamSource");
    checked(d->SetIndices(ib.value,0),"SetIndices");
    clear(d);checked(d->BeginScene(),"BeginScene indexed");
    checked(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,3,0,1),"DrawIndexedPrimitive green");
    checked(d->EndScene(),"EndScene indexed");
    pixel_is(back_pixels(d,64,64),64,18,18,{0,255,0},"green indexed triangle");
    const Vertex blue[]={{8,8,.5f,1,0xff0000ff},{56,8,.5f,1,0xff0000ff},
                         {56,56,.5f,1,0xff0000ff},{8,56,.5f,1,0xff0000ff}};
    checked(vb->Lock(0,0,&locked,0),"VB Lock fan");
    require(locked!=nullptr,"VB fan lock pointer");std::memcpy(locked,blue,sizeof(blue));
    checked(vb->Unlock(),"VB Unlock fan");
    clear(d);checked(d->BeginScene(),"BeginScene fan");
    checked(d->DrawPrimitive(D3DPT_TRIANGLEFAN,0,2),"DrawPrimitive VB blue fan");
    checked(d->EndScene(),"EndScene fan");
    pixel_is(back_pixels(d,64,64),64,32,32,{0,0,255},"blue triangle fan");
    checked(d->SetStreamSource(0,nullptr,0),"unbind stream");
    checked(d->SetIndices(nullptr,0),"unbind indices");
    require(vb.finish()==0,"vertex buffer public Release zero");
    require(ib.finish()==0,"index buffer public Release zero");

    struct WorldVertex{float x,y,z;DWORD color;};
    const WorldVertex world[]={{-.8f,-.8f,.5f,0xffff0000},{.8f,-.8f,.5f,0xffff0000},
                               {-.8f,.8f,.5f,0xffff0000}};
    D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
    checked(d->SetTransform(D3DTS_WORLD,&identity),"world identity");
    checked(d->SetTransform(D3DTS_VIEW,&identity),"view identity");
    checked(d->SetTransform(D3DTS_PROJECTION,&identity),"projection identity");
    checked(d->SetVertexShader(D3DFVF_XYZ|D3DFVF_DIFFUSE),"XYZ FVF");
    clear(d);checked(d->BeginScene(),"BeginScene XYZ");
    checked(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,world,sizeof(WorldVertex)),"XYZ identity red");
    checked(d->EndScene(),"EndScene XYZ");
    pixel_is(back_pixels(d,64,64),64,18,40,{255,0,0},"XYZ identity triangle");
    checked(d->SetVertexShader(vertex_fvf),"restore RHW FVF");
}
void exercise_sampler(IDirect3DDevice8* d){
    basic_state(d);
    Owned<IDirect3DTexture8> texture;
    checked(d->CreateTexture(16,16,5,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,
                             texture.out()),"sampler CreateTexture five mips");
    const std::array<DWORD,5> colors{{0xffff0000,0xff00ff00,0xff0000ff,
                                      0xffffffff,0xff000000}};
    for(UINT level=0;level<colors.size();++level){
        D3DLOCKED_RECT locked{};
        checked(texture->LockRect(level,&locked,nullptr,0),"sampler texture LockRect");
        const UINT side=16u>>level;
        for(UINT y=0;y<side;++y)for(UINT x=0;x<side;++x)
            std::memcpy(static_cast<std::uint8_t*>(locked.pBits)+y*locked.Pitch+x*4,
                        &colors[level],4);
        checked(texture->UnlockRect(level),"sampler texture UnlockRect");
    }
    checked(d->SetTexture(0,texture.value),"sampler bind texture");
    checked(d->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1),
            "sampler textured FVF");
    checked(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE),"sampler texture colour");
    checked(d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE),"sampler texture alpha");
    checked(d->SetTextureStageState(0,D3DTSS_MINFILTER,D3DTEXF_LINEAR),"sampler min linear");
    checked(d->SetTextureStageState(0,D3DTSS_MAGFILTER,D3DTEXF_LINEAR),"sampler mag linear");
    checked(d->SetTextureStageState(0,D3DTSS_MAXANISOTROPY,1),"sampler max anisotropy one");
    const float bias=3.5f;DWORD bias_bits=0;std::memcpy(&bias_bits,&bias,sizeof(bias));
    checked(d->SetTextureStageState(0,D3DTSS_MIPMAPLODBIAS,bias_bits),"sampler LOD bias 3.5");
    const TexVertex quad[]={{-.5f,-.5f,.5f,1,0xffffffff,0,0},
                            {63.5f,-.5f,.5f,1,0xffffffff,1,0},
                            {-.5f,63.5f,.5f,1,0xffffffff,0,1},
                            {63.5f,63.5f,.5f,1,0xffffffff,1,1}};
    for(DWORD mip=1;mip<=3;++mip){
        checked(d->SetTextureStageState(0,D3DTSS_MIPFILTER,mip),"sampler MIPFILTER");
        DWORD observed=0;
        checked(d->GetTextureStageState(0,D3DTSS_MIPFILTER,&observed),"sampler Get MIPFILTER");
        require(observed==mip,"sampler MIPFILTER round trip");
        clear(d);
        checked(d->BeginScene(),"sampler BeginScene");
        checked(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(TexVertex)),
                "sampler textured quad");
        checked(d->EndScene(),"sampler EndScene");
        const auto image=back_pixels(d,64,64);
        if(mip==1)pixel_is(image,64,32,32,{0,0,255},"sampler mip point blue");
        else pixel_is(image,64,32,32,{0,127,127},"sampler mip linear green blue");
    }
    checked(d->SetTextureStageState(0,D3DTSS_MIPMAPLODBIAS,0),"sampler restore LOD bias");
    checked(d->SetTextureStageState(0,D3DTSS_MAXANISOTROPY,1),"sampler restore max anisotropy");
    checked(d->SetTextureStageState(0,D3DTSS_MIPFILTER,D3DTEXF_NONE),"sampler restore mip none");
    checked(d->SetTexture(0,nullptr),"sampler unbind texture");
    require(texture.finish()==0,"sampler texture public Release zero");
    basic_state(d);
}
void exercise_vs11(IDirect3DDevice8* d,UINT minor=1){
    basic_state(d);
    const DWORD declaration[]={D3DVSD_STREAM(0),D3DVSD_REG(0,D3DVSDT_FLOAT4),
                               D3DVSD_REG(1,D3DVSDT_D3DCOLOR),D3DVSD_END()};
    constexpr DWORD destination=0x80000000u|D3DSP_WRITEMASK_ALL;
    constexpr DWORD source=0x80000000u|D3DSPR_INPUT|D3DVS_NOSWIZZLE;
    const DWORD code[]={D3DVS_VERSION(1,minor),
        D3DSIO_MOV,destination|D3DSPR_RASTOUT,source|0,
        D3DSIO_MOV,destination|D3DSPR_ATTROUT,source|1,D3DSIO_END};
    DWORD shader=0;checked(d->CreateVertexShader(declaration,code,&shader,0),"CreateVertexShader VS1.1");
    require(shader!=0,"VS1.1 shader handle");
    struct ShaderOwner{IDirect3DDevice8* device;DWORD handle;
        ~ShaderOwner(){if(handle)device->DeleteVertexShader(handle);}} owner{d,shader};
    DWORD length=0;
    checked(d->GetVertexShaderDeclaration(shader,nullptr,&length),"VS declaration size");
    require(length==sizeof(declaration),"VS declaration exact size");
    std::array<DWORD,sizeof(declaration)/sizeof(DWORD)> got_declaration{};
    checked(d->GetVertexShaderDeclaration(shader,got_declaration.data(),&length),"VS declaration bytes");
    require(std::memcmp(got_declaration.data(),declaration,sizeof(declaration))==0,"VS declaration exact bytes");
    length=0;checked(d->GetVertexShaderFunction(shader,nullptr,&length),"VS function size");
    require(length==sizeof(code),"VS function exact size");
    std::array<DWORD,sizeof(code)/sizeof(DWORD)> got_code{};
    checked(d->GetVertexShaderFunction(shader,got_code.data(),&length),"VS function bytes");
    require(std::memcmp(got_code.data(),code,sizeof(code))==0,"VS function exact bytes");
    struct ShaderVertex{float position[4];DWORD colour;};
    const ShaderVertex triangle[]={{{-.8f,-.8f,.5f,1},0xffff0000},
                                   {{.8f,-.8f,.5f,1},0xffff0000},
                                   {{-.8f,.8f,.5f,1},0xffff0000}};
    checked(d->SetVertexShader(shader),"bind VS1.1");
    clear(d);checked(d->BeginScene(),"VS BeginScene");
    checked(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(ShaderVertex)),"VS1.1 red triangle");
    checked(d->EndScene(),"VS EndScene");
    pixel_is(back_pixels(d,64,64),64,18,40,{255,0,0},"VS1.1 red triangle");
    checked(d->SetVertexShader(vertex_fvf),"restore FVF after VS1.1");
    checked(d->DeleteVertexShader(shader),"DeleteVertexShader VS1.x");owner.handle=0;
    std::printf("SHADER_VERSION minor=%u red pixel and original bytecode roundtrip passed\n",minor);
}
void draw_full_quad(IDirect3DDevice8* d,DWORD color,float z){
    const Vertex quad[]={{-.5f,-.5f,z,1,color},{63.5f,-.5f,z,1,color},
                         {63.5f,63.5f,z,1,color},{-.5f,63.5f,z,1,color}};
    checked(d->SetVertexShader(vertex_fvf),"clear test FVF");
    checked(d->BeginScene(),"clear test BeginScene");
    checked(d->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,quad,sizeof(Vertex)),"clear test full quad");
    checked(d->EndScene(),"clear test EndScene");
}
void exercise_partial_clear(IDirect3DDevice8* d){
    basic_state(d);
    const D3DVIEWPORT8 full{0,0,64,64,0,1};
    checked(d->SetViewport(&full),"partial clear full viewport");
    checked(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                     0xff000000,.2f,0),"partial clear depth baseline");
    const D3DRECT region{8,8,32,32};
    checked(d->Clear(1,&region,D3DCLEAR_ZBUFFER,0,.9f,0),"partial depth clear");
    checked(d->SetRenderState(D3DRS_ZENABLE,TRUE),"depth test on");
    checked(d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE),"depth writes off");
    checked(d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESS),"depth less");
    draw_full_quad(d,0xff0000ff,.5f);
    const auto depth=back_pixels(d,64,64);
    pixel_is(depth,64,16,16,{0,0,255},"depth cleared inside rectangle");
    pixel_is(depth,64,48,16,{0,0,0},"depth preserved outside rectangle");
    checked(d->SetRenderState(D3DRS_ZENABLE,FALSE),"depth off for stencil");
    checked(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_STENCIL,0xff000000,1,0),
            "partial clear stencil baseline");
    checked(d->Clear(1,&region,D3DCLEAR_STENCIL,0,1,7),"partial stencil clear");
    checked(d->SetRenderState(D3DRS_STENCILENABLE,TRUE),"stencil test on");
    checked(d->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_EQUAL),"stencil equal");
    checked(d->SetRenderState(D3DRS_STENCILREF,7),"stencil ref seven");
    checked(d->SetRenderState(D3DRS_STENCILMASK,0xff),"stencil mask");
    checked(d->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_KEEP),"stencil pass keep");
    checked(d->SetRenderState(D3DRS_STENCILFAIL,D3DSTENCILOP_KEEP),"stencil fail keep");
    draw_full_quad(d,0xffff0000,.5f);
    const auto stencil=back_pixels(d,64,64);
    pixel_is(stencil,64,16,16,{255,0,0},"stencil cleared inside rectangle");
    pixel_is(stencil,64,48,16,{0,0,0},"stencil preserved outside rectangle");
    checked(d->SetRenderState(D3DRS_STENCILENABLE,FALSE),"stencil test off");
    checked(d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE),"restore depth writes");
}
void exercise_copy_target(IDirect3DDevice8* d){
    basic_state(d);clear(d);
    const Vertex red[]={{8,8,.5f,1,0xffff0000},{56,8,.5f,1,0xffff0000},
                        {8,56,.5f,1,0xffff0000}};
    checked(d->BeginScene(),"copy source BeginScene");
    checked(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,red,sizeof(Vertex)),"copy source triangle");
    checked(d->EndScene(),"copy source EndScene");
    Owned<IDirect3DSurface8> source,destination;
    checked(d->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,source.out()),"copy source backbuffer");
    checked(d->CreateRenderTarget(64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,FALSE,
                                  destination.out()),"copy destination target");
    const RECT region{8,8,32,32};const POINT point{36,8};
    checked(d->CopyRects(source.value,&region,1,destination.value,&point),"GPU CopyRects colour target");
    const auto pixels=surface_pixels(destination.value,64,64);
    pixel_is(pixels,64,46,18,{255,0,0},"GPU copied rectangle pixel");
    require(destination.finish()==0,"copy destination public Release zero");
    require(source.finish()==0,"copy source public Release zero");
}
void exercise_rt_mips(IDirect3DDevice8* d){
    Owned<IDirect3DTexture8> texture;
    checked(d->CreateTexture(8,8,4,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,
                             D3DPOOL_DEFAULT,texture.out()),"CreateTexture four RT mips");
    Owned<IDirect3DSurface8> level0,level1,back,depth,image,gpu_copy;
    checked(texture->GetSurfaceLevel(0,level0.out()),"RT GetSurfaceLevel zero");
    checked(texture->GetSurfaceLevel(1,level1.out()),"RT GetSurfaceLevel one");
    checked(d->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,back.out()),"RT retain backbuffer");
    checked(d->GetDepthStencilSurface(depth.out()),"RT retain depthbuffer");
    checked(d->SetRenderTarget(level0.value,nullptr),"bind RT mip zero");
    checked(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xffff0000,1,0),"clear RT mip zero red");
    checked(d->SetRenderTarget(level1.value,nullptr),"bind RT mip one");
    checked(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff00ff00,1,0),"clear RT mip one green");
    pixel_is(surface_pixels(level0.value,8,8),8,1,1,{255,0,0},"RT mip zero retained red");
    pixel_is(surface_pixels(level1.value,4,4),4,1,1,{0,255,0},"RT mip one green");
    checked(d->CreateImageSurface(4,4,D3DFMT_A8R8G8B8,image.out()),"RT copy destination image");
    checked(d->CopyRects(level1.value,nullptr,0,image.value,nullptr),"RT mip one CopyRects image");
    pixel_is(surface_pixels(image.value,4,4),4,1,1,{0,255,0},"RT mip one copy green");
    checked(d->CreateRenderTarget(4,4,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,FALSE,
                                  gpu_copy.out()),"RT mip GPU copy destination");
    checked(d->CopyRects(level1.value,nullptr,0,gpu_copy.value,nullptr),"RT mip GPU CopyRects");
    pixel_is(surface_pixels(gpu_copy.value,4,4),4,1,1,{0,255,0},"RT mip GPU copy green");
    checked(d->SetRenderTarget(back.value,depth.value),"restore backbuffer/depth");
    require(gpu_copy.finish()==0,"RT GPU copy destination public Release zero");
    require(image.finish()==0,"RT copy image public Release zero");
    require(level1.finish()==0,"RT mip one surface public Release zero");
    require(level0.finish()==0,"RT mip zero surface public Release zero");
    require(texture.finish()==0,"RT texture public Release zero");
    require(depth.finish()==0,"RT retained depth public Release zero");
    require(back.finish()==0,"RT retained back public Release zero");
}
void exercise_state_block(IDirect3DDevice8* d){
    checked(d->SetRenderState(D3DRS_ZENABLE,FALSE),"state initial disabled");
    checked(d->BeginStateBlock(),"BeginStateBlock");
    checked(d->SetRenderState(D3DRS_ZENABLE,TRUE),"record Z enable");
    DWORD token=0;checked(d->EndStateBlock(&token),"EndStateBlock");
    require(token!=0,"state block token");
    DWORD value=~0u;checked(d->GetRenderState(D3DRS_ZENABLE,&value),"GetRenderState after record");
    require(value==FALSE,"recording did not execute");
    checked(d->ApplyStateBlock(token),"ApplyStateBlock");
    checked(d->GetRenderState(D3DRS_ZENABLE,&value),"GetRenderState after apply");
    require(value==TRUE,"state block applied");
    checked(d->SetRenderState(D3DRS_ZENABLE,FALSE),"state for capture");
    checked(d->CaptureStateBlock(token),"CaptureStateBlock");
    checked(d->SetRenderState(D3DRS_ZENABLE,TRUE),"state before captured apply");
    checked(d->ApplyStateBlock(token),"Apply captured state");
    checked(d->GetRenderState(D3DRS_ZENABLE,&value),"Get captured render state");
    require(value==FALSE,"captured state applied");
    checked(d->DeleteStateBlock(token),"DeleteStateBlock");
}
void exercise_managed_texture(IDirect3DDevice8* d,Owned<IDirect3DTexture8>& texture){
    checked(d->CreateTexture(32,32,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,texture.out()),"CreateTexture managed");
    D3DLOCKED_RECT lock{};checked(texture->LockRect(0,&lock,nullptr,0),"managed LockRect");
    for(UINT y=0;y<32;++y)for(UINT x=0;x<32;++x){
        const DWORD green=0xff00ff00;
        std::memcpy(static_cast<BYTE*>(lock.pBits)+y*lock.Pitch+x*4,&green,4);
    }
    checked(texture->UnlockRect(0),"managed UnlockRect/upload");
    checked(d->SetTexture(0,texture.value),"SetTexture managed");
    Owned<IDirect3DBaseTexture8> returned;
    checked(d->GetTexture(0,returned.out()),"GetTexture managed");
    require(returned.value==texture.value,"GetTexture canonical identity");
    returned.reset();
}
void exercise_reset(IDirect3DDevice8* d,IDirect3DTexture8* managed,D3DPRESENT_PARAMETERS& params){
    checked(d->SetTexture(0,nullptr),"unbind managed before reset");
    Owned<IDirect3DSurface8> image;
    checked(d->CreateImageSurface(4,4,D3DFMT_A8R8G8B8,image.out()),"CreateImageSurface before Reset");
    params.BackBufferWidth=params.BackBufferHeight=32;
    checked(d->Reset(&params),"Reset 64 to 32");
    D3DSURFACE_DESC image_desc{};
    checked(image->GetDesc(&image_desc),"image surface survives Reset");
    require(image_desc.Pool==D3DPOOL_SYSTEMMEM && image_desc.Width==4,"image surface after Reset");
    require(image.finish()==0,"image surface public Release zero");
    D3DSURFACE_DESC description{};checked(managed->GetLevelDesc(0,&description),"managed survives Reset");
    require(description.Width==32 && description.Height==32,"managed dimensions after Reset");
    checked(d->SetTexture(0,managed),"bind managed after Reset");
    Owned<IDirect3DBaseTexture8> returned;
    checked(d->GetTexture(0,returned.out()),"GetTexture after Reset");
    require(returned.value==managed,"managed identity after Reset");
    returned.reset();checked(d->SetTexture(0,nullptr),"unbind managed after Reset");
    checked(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0),"Clear after Reset");
    require(back_pixels(d,32,32).size()==32*32*4,"readback after Reset");
    params.AutoDepthStencilFormat=D3DFMT_D16;
    checked(d->Reset(&params),"Reset 32x32 to D16 auto depth");
    {
        Owned<IDirect3DSurface8> depth;
        checked(d->GetDepthStencilSurface(depth.out()),"GetDepthStencilSurface D16");
        D3DSURFACE_DESC desc{};
        checked(depth->GetDesc(&desc),"GetDesc D16 auto depth");
        require(desc.Format==D3DFMT_D16 && desc.Width==32 && desc.Height==32 &&
                desc.Size==2048,"D16 auto depth dimensions and size");
        checked(d->Clear(0,nullptr,D3DCLEAR_ZBUFFER,0,0.5f,0),"Clear D16 auto depth");
        require(depth.finish()==0,"D16 auto depth public Release zero");
    }
    params.EnableAutoDepthStencil=FALSE;
    checked(d->Reset(&params),"Reset 32x32 without auto depth");
    checked(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0),"Clear without auto depth");
    Owned<IDirect3DSurface8> absent_depth;
    require(FAILED(d->GetDepthStencilSurface(absent_depth.out())) && !absent_depth.value,
            "GetDepthStencilSurface fails without auto depth");
    checked(managed->GetLevelDesc(0,&description),"managed survives depth resets");
    require(description.Width==32 && description.Height==32,"managed dimensions after depth resets");
}
std::vector<std::uint8_t> bc_reference(const std::vector<std::uint8_t>& raw,bool normal){
    require(raw.size()==8*8*4,"8x8 RGBA reference length");
    if(!normal)return raw;
    auto expected=raw;
    for(std::size_t i=0;i<expected.size();i+=4){
        const float x=raw[i]/255.f*2-1,y=raw[i+1]/255.f*2-1;
        const float z=std::sqrt(std::max(0.f,1-x*x-y*y));
        const float scale=1.f/std::sqrt(x*x+y*y+z*z);
        auto encode=[](float v){return static_cast<std::uint8_t>(std::lround(std::clamp(v*.5f+.5f,0.f,1.f)*255));};
        expected[i]=encode(x*scale);expected[i+1]=encode(y*scale);
        expected[i+2]=encode(z*scale);expected[i+3]=255;
    }
    return expected;
}
void compare_bc(const std::vector<std::uint8_t>& got,const std::vector<std::uint8_t>& expected,
                UINT stride,const char* asset){
    unsigned maximum=0,bad=0;
    for(UINT y=0;y<8;++y)for(UINT x=0;x<8;++x)for(UINT c=0;c<4;++c){
        const UINT actual=got[(y*stride+x)*4+c],want=expected[(y*8+x)*4+c];
        const unsigned delta=actual>want?actual-want:want-actual;
        maximum=std::max(maximum,delta);
        if(delta>2){
            if(bad<4)std::fprintf(stderr,"MISMATCH %s %u,%u channel=%u got=%u expected=%u\n",
                asset,x,y,c,actual,want);
            ++bad;
        }
    }
    std::printf("BC_PIXELS asset=%s maximum_delta=%u bad=%u\n",asset,maximum,bad);
    require(bad==0,"all 64 BC sampled pixels within two UNORM levels");
}
bool new_bc_upload(std::size_t first,bool normal){
    const std::string format=normal?" format=83 ":" format=98 ";
    const std::string mode=normal?" normal_xy=1":" normal_xy=0";
    for(std::size_t i=first;i<messages.size();++i)
        if(messages[i].find("BC asset upload ")!=std::string::npos &&
           messages[i].find(format)!=std::string::npos &&
           messages[i].find(mode)!=std::string::npos)return true;
    return false;
}
struct BcAsset {
    std::string transport_path;
    a2fo::renderer::textures::CompressedImage image;
};
std::uint32_t read32(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
BcAsset load_bc_asset(const std::string& modroot,const char* asset,bool normal){
    BcAsset loaded;
    loaded.transport_path=a2fo::renderer::game::bc_transport_path(asset);
    require(!loaded.transport_path.empty(),"catalog transport path");
    const auto canonical=read_file(modroot+"\\Textures\\"+asset);
    std::string error;
    checked(a2fo::renderer::textures::parse_compressed_dds(canonical.data(),canonical.size(),loaded.image,error),
            "parse canonical BC DDS");
    const auto& image=loaded.image;
    require(image.format==(normal?DXGI_FORMAT_BC5_UNORM:DXGI_FORMAT_BC7_UNORM),
            "expected canonical BC5 normal or BC7 colour");
    const auto source=read_file(loaded.transport_path);
    require(source.size()==128+image.bytes.size() &&
            std::memcmp(source.data(),"DDS ",4)==0 && read32(source.data()+4)==124 &&
            read32(source.data()+12)==image.height && read32(source.data()+16)==image.width &&
            read32(source.data()+28)==image.mips.size() &&
            std::memcmp(source.data()+84,"DXT4",4)==0 &&
            std::memcmp(source.data()+128,image.bytes.data(),image.bytes.size())==0,
            "DXT4 transport header and all encoded mips match canonical BC image");
    return loaded;
}
void draw_bc_texture(IDirect3DDevice8* d,IDirect3DTexture8* texture,
                     const BcAsset& loaded,const char* asset,bool normal,const char* reference){
    (void)loaded;
    checked(d->SetTexture(0,texture),"bind BC texture");
    checked(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"BC colour op");
    checked(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE),"BC colour texture");
    checked(d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1),"BC alpha op");
    checked(d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE),"BC alpha texture");
    checked(d->SetTextureStageState(0,D3DTSS_MAGFILTER,D3DTEXF_POINT),"BC mag point");
    checked(d->SetTextureStageState(0,D3DTSS_MINFILTER,D3DTEXF_POINT),"BC min point");
    checked(d->SetTextureStageState(0,D3DTSS_MIPFILTER,D3DTEXF_NONE),"BC mip none");
    checked(d->SetTextureStageState(0,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP),"BC address U clamp");
    checked(d->SetTextureStageState(0,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP),"BC address V clamp");
    checked(d->SetVertexShader(vertex_fvf|D3DFVF_TEX1),"BC textured FVF");
    // References sample the entire base mip onto 8x8, not its first 8 texels.
    const float u=1.f,v=1.f;
    const TexVertex quad[]={{-.5f,-.5f,.5f,1,0xffffffff,0,0},
                            {7.5f,-.5f,.5f,1,0xffffffff,u,0},
                            {7.5f,7.5f,.5f,1,0xffffffff,u,v},
                            {-.5f,7.5f,.5f,1,0xffffffff,0,v}};
    const D3DVIEWPORT8 small{0,0,8,8,0,1};
    checked(d->SetViewport(&small),"BC 8x8 viewport");
    checked(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0),"BC clear viewport");
    checked(d->BeginScene(),"BC BeginScene");
    checked(d->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,quad,sizeof(TexVertex)),"BC textured fan");
    checked(d->EndScene(),"BC EndScene");
    const auto readback=back_pixels(d,64,64);
    if(reference)compare_bc(readback,bc_reference(read_file(reference),normal),64,asset);
    checked(d->SetTexture(0,nullptr),"unbind BC texture");
    const D3DVIEWPORT8 full{0,0,64,64,0,1};
    checked(d->SetViewport(&full),"restore viewport");
    checked(d->SetVertexShader(vertex_fvf),"restore FVF after BC");
}
void exercise_bc(IDirect3DDevice8* d,const std::string& modroot,const char* asset,
                 bool normal,const char* reference){
    const BcAsset loaded=load_bc_asset(modroot,asset,normal);
    const auto& image=loaded.image;
    const std::size_t before=messages.size();
    Owned<IDirect3DTexture8> texture;
    checked(d->CreateTexture(image.width,image.height,static_cast<UINT>(image.mips.size()),0,
            D3DFMT_DXT4,D3DPOOL_MANAGED,texture.out()),"CreateTexture DXT4 transport");
    for(UINT level=0;level<image.mips.size();++level){
        const auto& mip=image.mips[level];D3DLOCKED_RECT lock{};
        checked(texture->LockRect(level,&lock,nullptr,0),"DXT4 mip LockRect");
        require(lock.Pitch>=static_cast<INT>(mip.row_pitch),"DXT4 mip pitch");
        for(UINT row=0;row<(mip.height+3)/4;++row)
            std::memcpy(static_cast<BYTE*>(lock.pBits)+row*lock.Pitch,
                image.bytes.data()+mip.offset+row*mip.row_pitch,mip.row_pitch);
        checked(texture->UnlockRect(level),"DXT4 mip UnlockRect/upload");
    }
    require(new_bc_upload(before,normal),"catalog BC payload uploaded with matching format/normal mode");
    draw_bc_texture(d,texture.value,loaded,asset,normal,reference);
    require(texture.finish()==0,"BC texture public Release zero");
}
void exercise_bc_d3dx(IDirect3DDevice8* d,const std::string& modroot,const char* dll,
                      const char* asset,bool normal,const char* reference){
    const BcAsset loaded=load_bc_asset(modroot,asset,normal);
    Module d3dx{LoadLibraryA(dll)};require(d3dx.value!=nullptr,"load installed D3DX81ab.dll path");
    const FARPROC symbol=GetProcAddress(d3dx.value,"D3DXCreateTextureFromFileExA");
    require(symbol!=nullptr,"resolve D3DXCreateTextureFromFileExA");
    using CreateTextureFromFileEx=HRESULT (WINAPI*)(
        IDirect3DDevice8*,LPCSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,
        DWORD,DWORD,D3DCOLOR,void*,PALETTEENTRY*,IDirect3DTexture8**);
    CreateTextureFromFileEx load=nullptr;
    static_assert(sizeof(load)==sizeof(symbol));std::memcpy(&load,&symbol,sizeof(symbol));
    constexpr DWORD d3dx_default=0xffffffffu; // Core's pinned D3DX8 default value.
    const std::size_t before=messages.size();
    Owned<IDirect3DTexture8> texture;
    checked(load(d,loaded.transport_path.c_str(),loaded.image.width,loaded.image.height,
        static_cast<UINT>(loaded.image.mips.size()),0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,
        d3dx_default,d3dx_default,0,nullptr,nullptr,texture.out()),
        "D3DXCreateTextureFromFileExA transport DDS");
    require(texture.value!=nullptr,"D3DX returned D3D8 texture");
    require(texture->GetLevelCount()==loaded.image.mips.size(),"D3DX texture mip count");
    for(UINT level=0;level<loaded.image.mips.size();++level){
        D3DSURFACE_DESC desc{};
        checked(texture->GetLevelDesc(level,&desc),"D3DX texture mip desc");
        require(desc.Format==D3DFMT_DXT4 &&
                desc.Width==loaded.image.mips[level].width &&
                desc.Height==loaded.image.mips[level].height,
                "D3DX produced logical DXT4 transport mip");
    }
    require(new_bc_upload(before,normal),"D3DX BC payload uploaded with matching format/normal mode");
    draw_bc_texture(d,texture.value,loaded,asset,normal,reference);
    require(texture.finish()==0,"D3DX texture public Release zero");
}
using D3dxImageInfo=HRESULT (WINAPI*)(LPCSTR,void*);
using D3dxTextureFileEx=HRESULT (WINAPI*)(
    IDirect3DDevice8*,LPCSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,
    DWORD,DWORD,D3DCOLOR,void*,PALETTEENTRY*,IDirect3DTexture8**);
struct D3dxImports {
    HMODULE image;
    IMAGE_THUNK_DATA32* d3dx8;
    IMAGE_THUNK_DATA32* d3dx9;
};
D3dxImports synthetic_d3dx_imports(FARPROC info,FARPROC texture){
    static_assert(sizeof(void*)==4,"synthetic PE32 IAT requires a 32-bit test process");
    // Hook state retains IAT slot addresses. Deliberately keep this fixture
    // committed until process exit, as in d3d8_game_activation_test.cpp.
    auto* data=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    require(data!=nullptr,"allocate synthetic D3DX PE32 image");
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(data);
    dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=128;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(data+128);
    nt->Signature=IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;
    nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage=4096;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]={512,60};
    auto* imports=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(data+512);
    imports[0].Name=608;imports[0].OriginalFirstThunk=768;imports[0].FirstThunk=800;
    imports[1].Name=624;imports[1].OriginalFirstThunk=832;imports[1].FirstThunk=848;
    std::memcpy(data+608,"D3DX81ab.dll",sizeof("D3DX81ab.dll"));
    std::memcpy(data+624,"D3DX9_99.dll",sizeof("D3DX9_99.dll"));
    std::memcpy(data+898,"D3DXGetImageInfoFromFileA",sizeof("D3DXGetImageInfoFromFileA"));
    std::memcpy(data+962,"D3DXCreateTextureFromFileExA",sizeof("D3DXCreateTextureFromFileExA"));
    auto* names8=reinterpret_cast<IMAGE_THUNK_DATA32*>(data+768);
    names8[0].u1.AddressOfData=896;names8[1].u1.AddressOfData=960;
    auto* slots8=reinterpret_cast<IMAGE_THUNK_DATA32*>(data+800);
    slots8[0].u1.Function=static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(info));
    slots8[1].u1.Function=static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(texture));
    auto* names9=reinterpret_cast<IMAGE_THUNK_DATA32*>(data+832);
    names9[0].u1.AddressOfData=896;
    auto* slots9=reinterpret_cast<IMAGE_THUNK_DATA32*>(data+848);
    slots9[0].u1.Function=slots8[0].u1.Function;
    return {reinterpret_cast<HMODULE>(data),slots8,slots9};
}
template<class Function> Function d3dx_import(DWORD entry){
    Function function=nullptr;
    static_assert(sizeof(function)==sizeof(entry));
    std::memcpy(&function,&entry,sizeof(entry));
    return function;
}
void exercise_d3dx_file_bridge(IDirect3DDevice8* d,const std::string& modroot,
                               const char* dll,const char* normal_reference,
                               const char* colour_reference){
    static Module d3dx{LoadLibraryA(dll)};
    require(d3dx.value!=nullptr,"load installed D3DX81ab for import bridge");
    const FARPROC info_symbol=GetProcAddress(d3dx.value,"D3DXGetImageInfoFromFileA");
    const FARPROC texture_symbol=GetProcAddress(d3dx.value,"D3DXCreateTextureFromFileExA");
    require(info_symbol && texture_symbol,"resolve installed D3DX8 file exports");
    const D3dxImports imports=synthetic_d3dx_imports(info_symbol,texture_symbol);
    const DWORD original_info=imports.d3dx8[0].u1.Function;
    const DWORD original_texture=imports.d3dx8[1].u1.Function;
    const DWORD d3dx9_sentinel=imports.d3dx9[0].u1.Function;
    require(a2fo::renderer::game::install_d3dx_file_bridge(imports.image,log_line),
            "install named D3DX81ab PE32 import bridge");
    require(imports.d3dx8[0].u1.Function!=original_info &&
            imports.d3dx8[1].u1.Function!=original_texture &&
            imports.d3dx9[0].u1.Function==d3dx9_sentinel,
            "only named D3DX81ab imports patched");
    const DWORD bridged_info=imports.d3dx8[0].u1.Function;
    const DWORD bridged_texture=imports.d3dx8[1].u1.Function;
    require(a2fo::renderer::game::install_d3dx_file_bridge(imports.image,log_line) &&
            imports.d3dx8[0].u1.Function==bridged_info &&
            imports.d3dx8[1].u1.Function==bridged_texture &&
            imports.d3dx9[0].u1.Function==d3dx9_sentinel,
            "D3DX8 import bridge installation is idempotent");
    require(!a2fo::renderer::game::install_d3dx_file_bridge(nullptr,log_line),
            "D3DX8 import bridge refuses null module");
    auto* malformed=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    require(malformed!=nullptr,"allocate malformed D3DX PE32 image");
    require(!a2fo::renderer::game::install_d3dx_file_bridge(
                reinterpret_cast<HMODULE>(malformed),log_line),
            "D3DX8 import bridge refuses malformed module");
    VirtualFree(malformed,0,MEM_RELEASE);

    const auto native_info=d3dx_import<D3dxImageInfo>(original_info);
    const auto native_texture=d3dx_import<D3dxTextureFileEx>(original_texture);
    const auto patched_info=d3dx_import<D3dxImageInfo>(bridged_info);
    const auto patched_texture=d3dx_import<D3dxTextureFileEx>(bridged_texture);
    constexpr DWORD d3dx_default=0xffffffffu;
    struct Case {const char* asset;bool normal;const char* reference;};
    const Case cases[]={{"federation_venture_bump.dds",true,normal_reference},
                        {"all_interface_buttons03.dds",false,colour_reference}};
    for(const Case& item:cases){
        const BcAsset loaded=load_bc_asset(modroot,item.asset,item.normal);
        const std::string canonical=modroot+"\\Textures\\"+item.asset;
        // D3DX8 headers are absent from this toolchain. Treat IMAGE_INFO as
        // opaque output and compare the real DLL's transport result bytewise.
        alignas(DWORD) std::array<std::uint8_t,64> expected_info{},bridged_info_bytes{};
        checked(native_info(loaded.transport_path.c_str(),expected_info.data()),
                "native D3DX8 transport image info");
        checked(patched_info(canonical.c_str(),bridged_info_bytes.data()),
                "bridged canonical D3DX8 image info");
        require(expected_info==bridged_info_bytes,
                "canonical D3DX8 image info equals native DXT4 transport info");

        const std::size_t before=messages.size();
        Owned<IDirect3DTexture8> texture;
        checked(patched_texture(d,canonical.c_str(),d3dx_default,d3dx_default,
            d3dx_default,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,d3dx_default,
            d3dx_default,0,nullptr,nullptr,texture.out()),
            "bridged canonical D3DXCreateTextureFromFileExA defaults");
        require(texture.value && texture->GetLevelCount()==loaded.image.mips.size(),
                "bridged D3DX8 texture has canonical mip count");
        for(UINT level=0;level<loaded.image.mips.size();++level){
            D3DSURFACE_DESC desc{};
            checked(texture->GetLevelDesc(level,&desc),"bridged D3DX8 mip desc");
            require(desc.Format==D3DFMT_DXT4 &&
                    desc.Width==loaded.image.mips[level].width &&
                    desc.Height==loaded.image.mips[level].height,
                    "bridged D3DX8 texture has DXT4 format and canonical dimensions");
        }
        require(new_bc_upload(before,item.normal),
                "bridged D3DX8 uploads native BC payload");
        draw_bc_texture(d,texture.value,loaded,item.asset,item.normal,item.reference);
        require(texture.finish()==0,"bridged D3DX8 texture public Release zero");
    }
    {
        const BcAsset loaded=load_bc_asset(modroot,"all_interface_buttons03.dds",false);
        const std::string canonical=modroot+"\\Textures\\all_interface_buttons03.dds";
        Owned<IDirect3DTexture8> native_keyed,bridged_keyed;
        constexpr D3DCOLOR key=0xff000000u;
        const HRESULT native_result=native_texture(d,loaded.transport_path.c_str(),
            d3dx_default,d3dx_default,static_cast<UINT>(loaded.image.mips.size()),
            0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,
            d3dx_default,d3dx_default,key,nullptr,nullptr,native_keyed.out());
        const HRESULT bridged_result=patched_texture(d,canonical.c_str(),
            d3dx_default,d3dx_default,d3dx_default,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,
            d3dx_default,d3dx_default,key,nullptr,nullptr,bridged_keyed.out());
        const bool native_compressed=SUCCEEDED(native_result) &&
            a2fo::renderer::game::is_bc_texture(d,native_keyed.value);
        if(native_compressed){
            checked(bridged_result,"keyed compressed native result remains compressed");
            require(a2fo::renderer::game::is_bc_texture(d,bridged_keyed.value),
                    "keyed bridge retains verified BC ownership");
        }else{
            require(FAILED(bridged_result) && !bridged_keyed.value,
                    "keyed fake-format RGBA fallback is refused");
        }
        std::printf("D3DX_COLORKEY native_hresult=0x%08lx native_compressed=%u bridged_hresult=0x%08lx\n",
            static_cast<unsigned long>(native_result),native_compressed?1u:0u,
            static_cast<unsigned long>(bridged_result));
        require(native_keyed.finish()==0 && bridged_keyed.finish()==0,
                "colour-key D3DX8 textures public Release zero");
    }
    {
        const BcAsset loaded=load_bc_asset(modroot,"all_interface_buttons03.dds",false);
        const std::string canonical=modroot+"\\Textures\\all_interface_buttons03.dds";
        const UINT resized_width=std::max<UINT>(1,loaded.image.width/2);
        require(resized_width!=loaded.image.width,"explicit resize fixture changes source width");
        Owned<IDirect3DTexture8> bridged_resized;
        const HRESULT bridged_result=patched_texture(d,canonical.c_str(),
            resized_width,d3dx_default,d3dx_default,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,
            d3dx_default,d3dx_default,0,nullptr,nullptr,bridged_resized.out());
        require(bridged_result==D3DERR_NOTAVAILABLE && !bridged_resized.value,
                "unsupported BC resize fails before fake-format decoding");
    }
    alignas(DWORD) std::array<std::uint8_t,64> unlisted_native{},unlisted_bridge{};
    const std::string unlisted=modroot+"\\Textures\\not_in_bc_catalog.dds";
    const HRESULT unlisted_result=native_info(unlisted.c_str(),unlisted_native.data());
    require(unlisted_result==patched_info(unlisted.c_str(),unlisted_bridge.data()),
            "unlisted D3DX8 file preserves native HRESULT");
    if(SUCCEEDED(unlisted_result))
        require(unlisted_native==unlisted_bridge,
                "unlisted D3DX8 file preserves native image info");
    std::puts("D3DX_BRIDGE named imports, native image info, BC5/BC7 draws, and pass-through passed");
}
void exercise_file_bridge(IDirect3DDevice8* d,const std::string& modroot,
                          const char* normal_reference,const char* colour_reference){
    struct Case {const char* asset;bool normal;const char* reference;BcAsset loaded;};
    Case cases[]={{"federation_venture_bump.dds",true,normal_reference,
                   load_bc_asset(modroot,"federation_venture_bump.dds",true)},
                  {"all_interface_buttons03.dds",false,colour_reference,
                   load_bc_asset(modroot,"all_interface_buttons03.dds",false)}};
    require(a2fo::renderer::game::install_file_bridge(GetModuleHandleA(nullptr),log_line),
            "install named fopen bridge on private EXE");
    for(const Case& item:cases){
        const std::string canonical=modroot+"\\Textures\\"+item.asset;
        const auto bridged=read_file(canonical);
        require(bridged.size()==128+item.loaded.image.bytes.size() &&
                std::memcmp(bridged.data(),"DDS ",4)==0 &&
                std::memcmp(bridged.data()+84,"DXT4",4)==0 &&
                std::memcmp(bridged.data()+128,item.loaded.image.bytes.data(),
                            item.loaded.image.bytes.size())==0,
                "named fopen exposes DXT4 transport with original encoded payload");
        const auto original=read_win32_file(canonical);
        a2fo::renderer::textures::CompressedImage image;std::string error;
        checked(a2fo::renderer::textures::parse_compressed_dds(original.data(),original.size(),image,error),
                "Win32 canonical DDS parse after fopen bridge");
        require(image.format==item.loaded.image.format &&
                image.bytes==item.loaded.image.bytes &&
                std::memcmp(original.data()+84,item.normal?"ATI2":"DX10",4)==0,
                "Win32 canonical DDS remains unchanged after bridge");
        const std::size_t before=messages.size();
        Owned<IDirect3DTexture8> texture;
        checked(a2fo::renderer::game::load_texture_file(d,canonical.c_str(),texture.out()),
                "direct canonical load_texture_file after IAT bridge");
        require(texture.value!=nullptr && new_bc_upload(before,item.normal),
                "direct canonical loader avoids recursive fopen transport");
        draw_bc_texture(d,texture.value,item.loaded,item.asset,item.normal,item.reference);
        require(texture.finish()==0,"bridge direct-loader texture public Release zero");
    }
}
void run(int argc,char** argv){
    require(argc>=3 && argc<=6,
            "usage: renderer_game_native.exe OUTPUT_DIR MOD_ROOT [NORMAL_REF [COLOUR_REF [D3DX81AB_DLL]]]");
    const std::string cache=std::string(argv[1])+"/cache";
    require(CreateDirectoryA(cache.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS,
            "create private shader cache directory");
    Module native_library{LoadLibraryA("d3d8.dll")};require(native_library.value!=nullptr,"load native d3d8.dll");
    using Create=IDirect3D8* (WINAPI*)(UINT);
    const FARPROC symbol=GetProcAddress(native_library.value,"Direct3DCreate8");
    require(symbol!=nullptr,"resolve native Direct3DCreate8");
    Create native_create=nullptr;static_assert(sizeof(native_create)==sizeof(symbol));
    std::memcpy(&native_create,&symbol,sizeof(symbol));
    WNDCLASSEXA cls{};cls.cbSize=sizeof(cls);cls.hInstance=GetModuleHandleA(nullptr);
    cls.lpfnWndProc=DefWindowProcA;cls.lpszClassName="A2FOGameNativeFixture";
    require(RegisterClassExA(&cls)!=0,"register private window class");
    Window window;window.instance=cls.hInstance;window.name=cls.lpszClassName;
    window.value=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,window.name,
        "private game renderer test",WS_POPUP,0,0,64,64,nullptr,nullptr,cls.hInstance,nullptr);
    require(window.value && !IsWindowVisible(window.value),"hidden current-thread HWND");
    {
        Owned<IDirect3D8> native;native.value=native_create(D3D_SDK_VERSION);
        require(native.value!=nullptr,"native D3D8 factory for adapter query");
        require(native->GetAdapterCount()>0,"native adapter count");
        D3DADAPTER_IDENTIFIER8 adapter{};
        checked(native->GetAdapterIdentifier(0,0,&adapter),"native adapter identifier");
        a2fo::renderer::game::Configuration config;
        config.mod_root=argv[2];config.shader_cache=cache;config.log=log_line;
        config.vsync=false;
        Owned<IDirect3D8> factory;
        factory.value=a2fo::renderer::game::create_factory(native.value,config);
        require(factory.value!=nullptr,"create translated D3D8 factory");
        native.reset(); // The translated factory retains its native adapter-query parent.
        D3DPRESENT_PARAMETERS params{};
        params.BackBufferWidth=params.BackBufferHeight=64;params.BackBufferCount=1;
        params.BackBufferFormat=D3DFMT_A8R8G8B8;params.SwapEffect=D3DSWAPEFFECT_DISCARD;
        params.hDeviceWindow=window.value;params.Windowed=TRUE;
        params.EnableAutoDepthStencil=TRUE;params.AutoDepthStencilFormat=D3DFMT_D24S8;
        params.Flags=D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
        Owned<IDirect3DDevice8> device;
        checked(factory->CreateDevice(0,D3DDEVTYPE_HAL,window.value,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,device.out()),"translated CreateDevice 64x64");
        checked(device->TestCooperativeLevel(),"TestCooperativeLevel");
        exercise_geometry(device.value);
        exercise_sampler(device.value);
        exercise_vs11(device.value);
        exercise_vs11(device.value,0);
        exercise_partial_clear(device.value);
        exercise_copy_target(device.value);
        exercise_rt_mips(device.value);
        exercise_state_block(device.value);
        Owned<IDirect3DTexture8> managed;
        exercise_managed_texture(device.value,managed);
        exercise_bc(device.value,config.mod_root,"federation_venture_bump.dds",true,argc>=4?argv[3]:nullptr);
        exercise_bc(device.value,config.mod_root,"all_interface_buttons03.dds",false,argc>=5?argv[4]:nullptr);
        if(argc==6){
            exercise_bc_d3dx(device.value,config.mod_root,argv[5],
                             "federation_venture_bump.dds",true,argv[3]);
            exercise_bc_d3dx(device.value,config.mod_root,argv[5],
                             "all_interface_buttons03.dds",false,argv[4]);
            exercise_d3dx_file_bridge(device.value,config.mod_root,argv[5],argv[3],argv[4]);
        }
        exercise_file_bridge(device.value,config.mod_root,
                             argc>=4?argv[3]:nullptr,argc>=5?argv[4]:nullptr);
        checked(device->Present(nullptr,nullptr,nullptr,nullptr),"translated Present");
        exercise_reset(device.value,managed.value,params);
        require(managed.finish()==0,"managed texture public Release zero");
        require(device.finish()==0,"translated device public Release zero");
        require(factory.finish()==0,"translated factory public Release zero");
    }
    require(DestroyWindow(window.value)!=FALSE,"destroy private window");window.value=nullptr;
    require(UnregisterClassA(window.name,window.instance)!=FALSE,"unregister private window class");window.name=nullptr;
    require(FreeLibrary(native_library.value)!=FALSE,"unload native d3d8.dll");native_library.value=nullptr;
    std::puts("PASS: translated D3D8 game native fixture");
}
}
int main(int argc,char** argv){try{run(argc,argv);return 0;}
    catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
