#include "../renderer/refit/host.h"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <d3d8.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace {
constexpr unsigned W = 64, H = 64;
static_assert(sizeof(void*) == 4);
static_assert(sizeof(A2FORefitApi) == 44 && sizeof(A2FORefitFrame) == 188);
static_assert(sizeof(A2FORefitApi2) == 60 && sizeof(A2FORefitFrame2) == 192);
static_assert(sizeof(A2FORefitPart2) == 116 && sizeof(A2FORefitModel2) == 28);
static_assert(sizeof(A2FORefitEncodedTexture2) == 28);
static_assert(sizeof(A2FORefitRaster2) == 8);

void need(bool yes, const char* what) {
    if (!yes) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
void check(int32_t actual, int32_t expected, const char* what) {
    if (actual != expected) {
        std::fprintf(stderr, "FAIL: %s got=%ld expected=%ld\n", what,
                     static_cast<long>(actual), static_cast<long>(expected));
        std::exit(1);
    }
}
void ok(HRESULT hr, const char* what) {
    if (FAILED(hr)) std::fprintf(stderr, "HRESULT %s: 0x%08lx\n", what, static_cast<unsigned long>(hr));
    need(SUCCEEDED(hr), what);
}
template<class F> F export_of(HMODULE m, const char* name) {
    FARPROC raw = GetProcAddress(m, name); F f = nullptr;
    static_assert(sizeof(f) == sizeof(raw)); std::memcpy(&f, &raw, sizeof(f)); return f;
}
void identity34(float (&m)[12]) { std::fill(std::begin(m), std::end(m), 0.f); m[0]=m[4]=m[8]=1.f; }
void identity44(float (&m)[16]) { std::fill(std::begin(m), std::end(m), 0.f); m[0]=m[5]=m[10]=m[15]=1.f; }
struct RGB { unsigned r,g,b; };
RGB pixel(const std::vector<uint8_t>& p, unsigned x, unsigned y) {
    size_t i=(y*W+x)*4; return {p[i+2],p[i+1],p[i]};
}
std::vector<uint8_t> read(const A2FORefitApi2* api, uint32_t serial, uint32_t epoch=1) {
    std::vector<uint8_t> p(W*H*4);
    A2FORefitReadback d{sizeof(d),epoch,serial,W,H,p.data(),static_cast<uint32_t>(p.size())};
    check(api->readback(&d),0,"readback"); return p;
}
unsigned colour_count(const std::vector<uint8_t>& p, bool left, bool blue) {
    unsigned n=0;
    for(unsigned y=0;y<H;++y) for(unsigned x=left?0:W/2;x<(left?W/2:W);++x) {
        RGB c=pixel(p,x,y);
        if(blue ? (c.b>c.g+45 && c.b>c.r+45) : (c.g>c.b+45 && c.g>c.r+45)) ++n;
    }
    return n;
}
void word(std::vector<uint8_t>& p,size_t at,uint32_t v) { for(unsigned i=0;i<4;++i)p[at+i]=static_cast<uint8_t>(v>>(i*8)); }
std::vector<uint8_t> tga_blue() {
    std::vector<uint8_t> p(18+16,0); p[2]=2; p[12]=2; p[14]=2; p[16]=32; p[17]=0x20;
    for(size_t i=18;i<p.size();i+=4){p[i]=255;p[i+3]=255;} return p;
}
std::vector<uint8_t> dds_green(bool three_colour=false) {
    std::vector<uint8_t> p(136,0); std::memcpy(p.data(),"DDS ",4);
    word(p,4,124);word(p,8,0x1007);word(p,12,4);word(p,16,4);word(p,76,32);
    word(p,80,4);std::memcpy(p.data()+84,"DXT1",4);
    if(three_colour) {p[128]=0xe0;p[129]=0x07;p[130]=0x00;p[131]=0xf8;}
    else {p[128]=0x00;p[129]=0xf8;p[130]=0xe0;p[131]=0x07;
          word(p,132,0x55555555);}
    return p;
}
// Block-compressed DDS builders: each block is one solid colour (index 0 everywhere).
std::vector<uint8_t> dds_blocks(const char* fourcc,uint32_t w,uint32_t h,uint32_t levels,uint32_t dxgi,
                                const std::vector<uint8_t>& data) {
    const bool dx10=std::memcmp(fourcc,"DX10",4)==0;
    std::vector<uint8_t> p(dx10?148:128,0); std::memcpy(p.data(),"DDS ",4);
    word(p,4,124);word(p,8,0x1007u|(levels>1?0x20000u:0u));word(p,12,h);word(p,16,w);word(p,28,levels);
    word(p,76,32);word(p,80,4);std::memcpy(p.data()+84,fourcc,4);
    if(dx10){word(p,128,dxgi);word(p,132,3);word(p,136,0);word(p,140,1);word(p,144,0);}
    p.insert(p.end(),data.begin(),data.end()); return p;
}
std::vector<uint8_t> bc1(uint16_t c565) { return {static_cast<uint8_t>(c565),static_cast<uint8_t>(c565>>8),0,0,0,0,0,0}; }
std::vector<uint8_t> bc4(uint8_t v) { return {v,v,0,0,0,0,0,0}; }
std::vector<uint8_t> bc7_green() { // mode 6, both endpoints (1,255,1,255), all indices 0
    std::vector<uint8_t> b(16,0); unsigned at=0;
    auto put=[&](unsigned v,unsigned n){for(unsigned i=0;i<n;++i,++at) if(v>>i&1u) b[at/8]|=static_cast<uint8_t>(1u<<(at%8));};
    put(1u<<6,7);put(0,7);put(0,7);put(127,7);put(127,7);put(0,7);put(0,7);put(127,7);put(127,7);put(1,1);put(1,1);
    return b;
}
std::vector<uint8_t> repeat(const std::vector<uint8_t>& block,size_t n) {
    std::vector<uint8_t> out; for(size_t i=0;i<n;++i) out.insert(out.end(),block.begin(),block.end()); return out;
}
std::vector<uint8_t> join(std::vector<uint8_t> a,const std::vector<uint8_t>& b) { a.insert(a.end(),b.begin(),b.end()); return a; }
template<class F> unsigned count_where(const std::vector<uint8_t>& p,F f) {
    unsigned n=0; for(unsigned y=0;y<H;++y) for(unsigned x=0;x<W;++x) if(f(pixel(p,x,y))) ++n; return n;
}
struct Callback { unsigned calls=0; uint32_t count=0; bool accept=true, poison=false; };
int32_t transform(void* raw,float* xyz,uint32_t count) {
    auto& c=*static_cast<Callback*>(raw); ++c.calls;c.count=count;
    if(!c.accept)return 0;
    for(uint32_t i=0;i<count*3;++i)xyz[i]*=.5f;
    if(c.poison && count)xyz[0]=std::numeric_limits<float>::infinity();
    return 1;
}
struct Window {
    HINSTANCE instance=GetModuleHandleA(nullptr); HWND hwnd=nullptr;
    Window() {
        WNDCLASSEXA c{}; c.cbSize=sizeof(c);c.hInstance=instance;c.lpfnWndProc=DefWindowProcA;
        c.lpszClassName="A2FORefitPartsPrivateFixture";
        need(RegisterClassExA(&c)!=0,"register private class");
        hwnd=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,c.lpszClassName,"hidden parts fixture",
                             WS_POPUP,0,0,W,H,nullptr,nullptr,instance,nullptr);
        need(hwnd && !IsWindowVisible(hwnd),"hidden private HWND");
    }
    ~Window(){if(hwnd)DestroyWindow(hwnd);UnregisterClassA("A2FORefitPartsPrivateFixture",instance);}
};
struct Probe { const A2FORefitApi2* api; A2FORefitFrame2* frame;
    A2FORefitDisplayReplaceOpaque2 replace; A2FORefitDisplayRaster2 raster;
    int32_t status=0,prepare=0,replace_result=0,raster_result=0;
    bool raster_unchanged=false; };
DWORD WINAPI wrong_thread(LPVOID raw) {
    auto& p=*static_cast<Probe*>(raw);A2FORefitStatus s{};s.size=sizeof(s);
    p.status=p.api->status(&s);p.prepare=p.api->prepare_parts(p.frame);
    p.replace_result=p.replace(nullptr,nullptr);
    A2FORefitRaster2 out{sizeof(out),123.f};const auto prior=out;
    p.raster_result=p.raster(&out);
    p.raster_unchanged=std::memcmp(&out,&prior,sizeof(out))==0;
    return 0;
}
struct FFVertex { float x,y,z,rhw; DWORD diffuse; };
void quad(IDirect3DDevice8* d,float z,DWORD argb,unsigned x0,unsigned y0,unsigned x1,unsigned y1,bool depth) {
    ok(d->SetTexture(0,nullptr),"FF no texture");
    ok(d->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_DIFFUSE),"FF FVF");
    ok(d->SetPixelShader(0),"FF pixel shader");
    ok(d->SetRenderState(D3DRS_LIGHTING,FALSE),"FF unlit");
    ok(d->SetRenderState(D3DRS_ZENABLE,depth?TRUE:FALSE),"FF depth test");
    ok(d->SetRenderState(D3DRS_ZWRITEENABLE,depth?TRUE:FALSE),"FF depth write");
    ok(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"FF no cull");
    ok(d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE),"FF opaque");
    ok(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"FF colour op");
    ok(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE),"FF diffuse");
    ok(d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1),"FF alpha op");
    ok(d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE),"FF alpha diffuse");
    const FFVertex v[4]={{float(x0)-.5f,float(y0)-.5f,z,1,argb},{float(x1)-.5f,float(y0)-.5f,z,1,argb},
                         {float(x0)-.5f,float(y1)-.5f,z,1,argb},{float(x1)-.5f,float(y1)-.5f,z,1,argb}};
    ok(d->BeginScene(),"FF begin");ok(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(v[0])),"FF draw");
    ok(d->EndScene(),"FF end");
}
void native_triangle(IDirect3DDevice8* d,DWORD argb) {
    ok(d->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_DIFFUSE),"native RHW FVF");
    const FFVertex v[3]={{16.f,16.f,.2f,1,argb},{48.f,16.f,.2f,1,argb},{32.f,48.f,.2f,1,argb}};
    ok(d->BeginScene(),"native triangle begin");
    ok(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,v,sizeof(v[0])),"native triangle draw");
    ok(d->EndScene(),"native triangle end");
}
struct XYZVertex { float x,y,z; DWORD diffuse; };
void native_triangle_xyz(IDirect3DDevice8* d,DWORD argb) {
    ok(d->SetVertexShader(D3DFVF_XYZ|D3DFVF_DIFFUSE),"native XYZ FVF");
    const XYZVertex v[3]={{-.5f,-.5f,.2f,argb},{.5f,-.5f,.2f,argb},{0.f,.5f,.2f,argb}};
    ok(d->BeginScene(),"native XYZ triangle begin");
    ok(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,v,sizeof(v[0])),"native XYZ triangle draw");
    ok(d->EndScene(),"native XYZ triangle end");
}
struct MeshVertex { float x,y,z,rhw; DWORD diffuse,specular; float u,v; };
static_assert(sizeof(MeshVertex)==32 &&
              (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX1)==0x1c4);
struct Replacement {
    const A2FORefitApi2* api;
    A2FORefitDisplayReplaceOpaque2 replace;
    A2FORefitFrame2 frame;
    A2FORefitPart2 part;
    unsigned calls=0;
    bool claim=true;
    int32_t reentry=0,prepared=0,drawn=0;
    uint32_t viewport[4]{};
};
int32_t replace_callback(void* raw,const uint32_t viewport[4]) {
    auto& c=*static_cast<Replacement*>(raw);++c.calls;
    std::copy(viewport,viewport+4,c.viewport);
    c.reentry=c.replace(replace_callback,raw);
    if(!c.claim)return 0;
    c.frame.serial=10+c.calls;
    c.prepared=c.api->prepare_parts(&c.frame);
    if(c.prepared!=0)return 0;
    c.drawn=c.api->draw_prepared(c.frame.source_epoch,c.frame.serial);
    return c.drawn==0?1:0;
}
struct MeshReplacement {
    const A2FORefitApi2* api;
    A2FORefitDisplayReplaceMesh2 replace_mesh;
    A2FORefitDisplayReplaceOpaque2 replace_opaque;
    A2FORefitFrame2 frame;
    A2FORefitPart2 part;
    A2FORefitDraw2 draw{};
    std::uintptr_t vertices=0;
    unsigned calls=0;
    int32_t mesh_reentry=0,opaque_reentry=0,prepared=0,drawn=0;
};
int32_t replace_mesh_callback(void* raw,const A2FORefitDraw2* draw){
    auto& c=*static_cast<MeshReplacement*>(raw);++c.calls;
    need(draw&&draw->size==sizeof(*draw),"borrowed mesh draw descriptor");
    c.draw=*draw;
    c.mesh_reentry=c.replace_mesh(replace_mesh_callback,raw);
    c.opaque_reentry=c.replace_opaque(replace_callback,raw);
    c.frame.serial=20+c.calls;
    c.prepared=c.api->prepare_parts(&c.frame);
    if(c.prepared!=0)return 0;
    c.drawn=c.api->draw_prepared(c.frame.source_epoch,c.frame.serial);
    return c.drawn==0?1:0;
}
void run(const char* backend,const char* display_path,const char* renderer_path) {
    const uint32_t backend_id=std::strcmp(backend,"vulkan")==0?1:2;
    need(backend_id==1 || std::strcmp(backend,"d3d12")==0,"backend name");
    Window window; HMODULE display=LoadLibraryA(display_path);need(display,"load Display");
    auto configure=export_of<A2FORefitDisplayConfigure>(display,"A2FORefitDisplay_Configure_v1");
    auto passthrough=export_of<A2FORefitDisplayPassthrough>(display,"A2FORefitDisplay_Passthrough_v1");
    auto replace=export_of<A2FORefitDisplayReplaceOpaque2>(display,"A2FORefitDisplay_ReplaceOpaque_v2");
    auto replace_mesh=export_of<A2FORefitDisplayReplaceMesh2>(display,"A2FORefitDisplay_ReplaceMesh_v2");
    auto raster=export_of<A2FORefitDisplayRaster2>(display,"A2FORefitDisplay_Raster_v2");
    using Create8=IDirect3D8* (WINAPI*)(UINT);
    auto create8=export_of<Create8>(display,"Direct3DCreate8");
    need(configure && passthrough && replace && replace_mesh && raster && create8,"Display exports");
    check(configure(backend_id,window.hwnd,W,H,0),0,"configure Display");
    A2FORefitRaster2 raster_value{sizeof(A2FORefitRaster2),123.f};
    check(raster(&raster_value),0,"read default raster offset");
    need(raster_value.size==sizeof(raster_value) &&
         raster_value.rhw_to_scene_offset==0.4921875f,"default native pixel centres");
    A2FORefitRaster2 wrong_size{0,123.f};const auto wrong_size_prior=wrong_size;
    check(raster(&wrong_size),A2FO_REFIT_INVALID,"wrong raster size");
    need(std::memcmp(&wrong_size,&wrong_size_prior,sizeof(wrong_size))==0,"wrong raster size untouched");
    check(raster(nullptr),A2FO_REFIT_INVALID,"null raster output");
    alignas(A2FORefitRaster2) unsigned char storage[sizeof(A2FORefitRaster2)+1]{};
    const A2FORefitRaster2 prior_misaligned{sizeof(A2FORefitRaster2),123.f};
    std::memcpy(storage+1,&prior_misaligned,sizeof(prior_misaligned));
    check(raster(reinterpret_cast<A2FORefitRaster2*>(storage+1)),A2FO_REFIT_INVALID,
          "misaligned raster output");
    need(std::memcmp(storage+1,&prior_misaligned,sizeof(prior_misaligned))==0,
         "misaligned raster output untouched");
    IDirect3D8* d3d=create8(D3D_SDK_VERSION);need(d3d,"create facade");
    D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=W;pp.BackBufferHeight=H;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
    pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window.hwnd;
    pp.Windowed=TRUE;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;
    IDirect3DDevice8* device=nullptr;
    ok(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window.hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device),"create facade device");
    need(device,"device pointer");
    HMODULE renderer=LoadLibraryA(renderer_path);need(renderer,"load renderer");
    auto get=export_of<A2FORefitGetInterface>(renderer,"A2FORefitRenderer_GetInterface");need(get,"GetInterface");
    const A2FORefitApi* v1=get(1);need(v1 && v1->size==44 && v1->abi_version==1 && v1->capabilities==63,"v1 unchanged");
    const A2FORefitApi2* api=reinterpret_cast<const A2FORefitApi2*>(get(2));
    need(api && api->size==60 && api->abi_version==2 && api->capabilities==511,"v2 table");
    A2FORefitConfig config{sizeof(config),display,0,1};check(api->initialize(&config),0,"initialize ABI2");
    A2FORefitStatus status{};status.size=sizeof(status);check(v1->status(&status),0,"v1 status through shared session");
    need(status.backend==backend_id && status.source_epoch==1 && status.last_serial==0,"initial status");
    check(passthrough(1),0,"enable native FF passthrough");

    auto blue=tga_blue(),green=dds_green(),three=dds_green(true);
    const char blue_name[]="blue.tga", green_name[]="green.dds", three_name[]="three.dds";
    A2FORefitEncodedTexture2 b{sizeof(b),blue_name,8,1,blue.data(),static_cast<uint32_t>(blue.size()),0};
    A2FORefitEncodedTexture2 g{sizeof(g),green_name,9,2,green.data(),static_cast<uint32_t>(green.size()),0};
    A2FORefitEncodedTexture2 t{sizeof(t),three_name,9,1,three.data(),static_cast<uint32_t>(three.size()),0};
    check(api->upload_encoded_texture(&b),0,"upload encoded TGA");
    check(api->upload_encoded_texture(&g),0,"upload encoded DXT1 DDS");
    check(api->upload_encoded_texture(&t),0,"upload three-colour DXT1 DDS");
    auto sod=native11_fixture::model("unused.tga",false);
    Callback callback{};A2FORefitModel2 model{{sizeof(model),7,1,sod.data(),static_cast<uint32_t>(sod.size())},&callback,transform};
    uint32_t nodes=0;check(api->upload_model_transformed(&model,&nodes),0,"upload transformed SOD");
    need(nodes==1 && callback.calls==1 && callback.count==3,"transform once per mesh with XYZ triple count");
    A2FORefitModel2 invalid=model;invalid.base.id=8;invalid.transform_positions=nullptr;
    uint32_t untouched=0xa5a5a5a5;check(api->upload_model_transformed(&invalid,&untouched),A2FO_REFIT_INVALID,"null callback");
    need(untouched==0xa5a5a5a5,"null callback output untouched");
    callback.accept=false;invalid.transform_positions=transform;
    check(api->upload_model_transformed(&invalid,&untouched),A2FO_REFIT_CONTENT,"failed callback");
    need(untouched==0xa5a5a5a5 && callback.calls==2,"callback failure does not publish output");
    callback.accept=true;callback.poison=true;
    check(api->upload_model_transformed(&invalid,&untouched),A2FO_REFIT_CONTENT,"nonfinite callback output");
    need(untouched==0xa5a5a5a5 && callback.calls==3,"nonfinite callback does not publish output");
    callback.poison=false;check(api->status(&status),0,"status after callback rejection");need(status.models==1,"failed model not published");
    std::fill(sod.begin(),sod.end(),0);

    A2FORefitPart2 parts[2]{};
    for(auto& p:parts) {p.size=sizeof(p);p.model_id=7;p.model_revision=1;p.node_index=0;p.group_index=0;
        p.flags=A2FO_REFIT_UNLIT;p.alpha=1;identity34(p.world);p.ambient[0]=p.ambient[1]=p.ambient[2]=1;}
    parts[0].world[9]=-.45f;parts[0].texture_name=blue_name;parts[0].texture_name_length=8;parts[0].texture_revision=1;
    parts[1].world[9]=.45f;parts[1].texture_name=green_name;parts[1].texture_name_length=9;parts[1].texture_revision=2;
    A2FORefitFrame2 frame{};frame.size=sizeof(frame);frame.source_epoch=1;frame.serial=1;
    frame.width=W;frame.height=H;frame.flags=A2FO_REFIT_FRAME_DIRECT;
    identity44(frame.view);identity44(frame.projection);frame.eye[2]=-2;frame.parts=parts;frame.part_count=2;
    Probe probe{api,&frame,replace,raster};HANDLE thread=CreateThread(nullptr,0,wrong_thread,&probe,0,nullptr);
    need(thread,"create wrong thread");need(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0,"join wrong thread");CloseHandle(thread);
    check(probe.status,A2FO_REFIT_STATE,"wrong thread status");check(probe.prepare,A2FO_REFIT_STATE,"wrong thread prepare");
    check(probe.replace_result,A2FO_REFIT_STATE,"wrong thread replacement registration");
    check(probe.raster_result,A2FO_REFIT_STATE,"wrong thread raster query");
    need(probe.raster_unchanged,"wrong thread raster query untouched");

    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff282020,1.f,0),"seed colour and depth");
    check(api->prepare_parts(&frame),0,"prepare first direct frame");
    check(api->draw_prepared(1,1),0,"draw two explicit textures");
    auto first=read(api,1);
    need(colour_count(first,true,true)>15 && colour_count(first,false,false)>15,"per-instance blue and green textures");
    RGB corner=pixel(first,60,2);need(corner.r>25 && corner.b>20,"direct preserves seeded colour");
    check(api->draw_prepared(1,1),A2FO_REFIT_STALE,"prepared token consumed once");

    // Existing FF colour and depth precede the Refit direct pass.
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff282020,1.f,0),"clear for retained FF regions");
    quad(device,.5f,0xff00ff00,1,1,18,18,false); // HUD-style green corner.
    quad(device,.05f,0xffff0000,25,25,40,40,true); // Foreground depth occluder.
    auto seeded=read(api,1);
    RGB hud=pixel(seeded,5,5), foreground=pixel(seeded,32,32);
    need(hud.g>150 && foreground.r>150,"FF seeding reached native target");
    frame.serial=2;parts[0].world[9]=0;frame.part_count=1;
    check(api->prepare_parts(&frame),0,"prepare over retained FF scene");
    need(read(api,1)==seeded,"prepare issues no draw or colour change");
    check(api->draw_prepared(1,2),0,"direct draw over retained FF scene");
    auto composited=read(api,2);
    RGB hud_after=pixel(composited,5,5),front_after=pixel(composited,32,32);
    need(hud_after.g==hud.g && hud_after.r==hud.r && hud_after.b==hud.b,"HUD region preserved");
    need(front_after.r==foreground.r && front_after.g==foreground.g && front_after.b==foreground.b,
         "existing foreground depth occludes direct triangle");

    auto reject=[&](A2FORefitFrame2 bad,int32_t expected,const char* why) {
        check(api->prepare_parts(&bad),expected,why);
        check(api->draw_prepared(1,bad.serial),A2FO_REFIT_STALE,"failed prepare leaves no packet");
        need(read(api,2)==composited,"validation rejection preserves prior readable frame");
    };
    frame.serial=3;A2FORefitFrame2 bad=frame;bad.source_epoch=2;reject(bad,A2FO_REFIT_STALE,"stale epoch");
    A2FORefitPart2 saved=parts[0];parts[0].model_revision=9;reject(frame,A2FO_REFIT_STALE,"stale model revision");
    parts[0]=saved;parts[0].group_index=1;reject(frame,A2FO_REFIT_CONTENT,"bad group");
    parts[0]=saved;parts[0].texture_revision=3;reject(frame,A2FO_REFIT_STALE,"stale texture revision");
    parts[0]=saved;parts[0].texture_name="missing.tga";parts[0].texture_name_length=11;reject(frame,A2FO_REFIT_CONTENT,"missing texture");
    parts[0]=saved;parts[0].alpha=.5f;reject(frame,A2FO_REFIT_INVALID,"nonopaque instance");
    parts[0]=saved;
    auto additive=native11_fixture::model("",false);
    auto start=std::search(additive.begin(),additive.end(),"default","default"+7);
    need(start!=additive.end() && start>=additive.begin()+2,"find authored blend name");
    const auto offset=start-additive.begin();
    *(start-2)=8;additive.erase(start,start+7);additive.insert(additive.begin()+offset,"additive","additive"+8);
    A2FORefitModel add{sizeof(add),9,1,additive.data(),static_cast<uint32_t>(additive.size())};
    uint32_t add_nodes=0;check(api->upload_model(&add,&add_nodes),0,"upload authored additive SOD");
    // Upload invalidates readback by v1 contract; establish a fresh successful frame.
    frame.serial=3;check(api->prepare_parts(&frame),0,"prepare after model upload");check(api->draw_prepared(1,3),0,"draw after model upload");
    auto after_upload=read(api,3);frame.serial=4;parts[0].model_id=9;
    check(api->prepare_parts(&frame),A2FO_REFIT_CONTENT,"authored additive rejected");
    need(read(api,3)==after_upload,"unsupported blend preserves last readable frame");
    parts[0]=saved;

    // Ownership: all caller spans can be reused immediately after prepare.
    char owned_name[]="three.dds";parts[0].texture_name=owned_name;parts[0].texture_name_length=9;
    parts[0].texture_revision=1;parts[0].world[9]=.45f;frame.serial=4;
    A2FORefitLight light{};light.kind=A2FO_REFIT_DIRECTIONAL;light.direction[2]=1;
    light.colour[0]=light.colour[1]=light.colour[2]=1;light.intensity=1;
    frame.lights=&light;frame.light_count=1;
    check(api->prepare_parts(&frame),0,"prepare owned packet");
    std::memset(owned_name,'X',sizeof(owned_name)-1);parts[0].group_index=99;parts[0].world[9]=1000;
    light.direction[2]=0;frame.view[0]=0;
    check(api->draw_prepared(1,4),0,"draw deep-copied packet");
    auto owned=read(api,4);need(colour_count(owned,false,false)>15,"copied green override and transform visible");

    // The SOD material selects cutout; encoded texture flags remain zero. A
    // discarded texel must preserve both colour and depth in the native target.
    auto cutout_sod=native11_fixture::model("",false);
    auto material=std::search(cutout_sod.begin(),cutout_sod.end(),"default","default"+7);
    need(material!=cutout_sod.end() && material>=cutout_sod.begin()+2,"find cutout material name");
    const auto material_offset=material-cutout_sod.begin();
    *(material-2)=14;cutout_sod.erase(material,material+7);
    cutout_sod.insert(cutout_sod.begin()+material_offset,
                      "alphathreshold","alphathreshold"+14);
    A2FORefitModel cutout_model{sizeof(cutout_model),10,1,cutout_sod.data(),
                               static_cast<uint32_t>(cutout_sod.size())};
    uint32_t cutout_nodes=0;
    check(api->upload_model(&cutout_model,&cutout_nodes),0,"upload authored cutout SOD");
    need(cutout_nodes==1,"cutout model has one node");
    auto tga_low=tga_blue(),tga_high=tga_blue();
    tga_low[17]=tga_high[17]=0x28; // Top origin with eight authored alpha bits.
    for(size_t i=21;i<tga_low.size();i+=4){tga_low[i]=64;tga_high[i]=192;}
    auto dds_low=dds_green(true),dds_high=dds_green(true);
    word(dds_low,132,0xffffffffu); // DXT1 three-colour index 3: transparent.
    const char* cutout_names[]={"cutlow.tga","cuthi.tga","cutlow.dds","cuthi.dds"};
    const std::vector<uint8_t>* cutout_bytes[]={&tga_low,&tga_high,&dds_low,&dds_high};
    for(unsigned i=0;i<4;++i){
        A2FORefitEncodedTexture2 texture{sizeof(texture),cutout_names[i],
            static_cast<uint32_t>(std::strlen(cutout_names[i])),1,cutout_bytes[i]->data(),
            static_cast<uint32_t>(cutout_bytes[i]->size()),0};
        check(api->upload_encoded_texture(&texture),0,"upload encoded cutout texture");
    }
    A2FORefitPart2 cutout_part=saved;cutout_part.model_id=10;
    cutout_part.world[9]=0;
    A2FORefitFrame2 cutout_frame=frame;cutout_frame.parts=&cutout_part;
    cutout_frame.part_count=1;cutout_frame.lights=nullptr;cutout_frame.light_count=0;
    identity44(cutout_frame.view);
    for(unsigned i=0;i<4;++i){
        cutout_part.texture_name=cutout_names[i];
        cutout_part.texture_name_length=static_cast<uint32_t>(std::strlen(cutout_names[i]));
        cutout_part.texture_revision=1;cutout_frame.serial=5+i;
        ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),
           "clear cutout target");
        quad(device,.5f,0xffa02020,0,0,W,H,true);
        check(api->prepare_parts(&cutout_frame),0,"prepare cutout part");
        check(api->draw_prepared(1,cutout_frame.serial),0,"draw cutout part");
        const auto drawn=pixel(read(api,cutout_frame.serial),32,32);
        if((i&1)==0)need(drawn.r>100 && drawn.r>drawn.g+60 && drawn.g<60 && drawn.b<60,
                         "below-threshold TGA or DDS leaves colour unchanged");
        else if(i==1)need(drawn.b>drawn.r+45 && drawn.b>drawn.g+45,
                          "above-threshold TGA draws blue");
        else need(drawn.g>drawn.r+45 && drawn.g>drawn.b+45,
                  "opaque DXT1 index draws green");
        quad(device,.3f,0xffffff00,30,30,35,35,true);
        const auto depth_probe=pixel(read(api,cutout_frame.serial),32,32);
        if((i&1)==0)need(depth_probe.r>150 && depth_probe.g>150 && depth_probe.b<50,
                         "discarded TGA or DDS texel leaves depth writable");
        else need(depth_probe.r==drawn.r && depth_probe.g==drawn.g && depth_probe.b==drawn.b,
                  "accepted TGA or DDS texel writes depth");
    }

    // Null texture/revision zero samples owned white. The seeded colour makes
    // a missing texture binding visible, and a later native draw probes depth.
    auto white_sod=native11_fixture::model("",false);
    A2FORefitModel white_model{sizeof(white_model),11,1,white_sod.data(),
                              static_cast<uint32_t>(white_sod.size())};
    uint32_t white_nodes=0;
    check(api->upload_model(&white_model,&white_nodes),0,"upload untextured opaque SOD");
    need(white_nodes==1,"untextured model has one node");
    A2FORefitPart2 white_part=saved;white_part.model_id=11;
    identity34(white_part.world);white_part.texture_name=nullptr;
    white_part.texture_name_length=0;white_part.texture_revision=1;
    A2FORefitFrame2 white_frame=cutout_frame;white_frame.parts=&white_part;
    white_frame.serial=9;
    check(api->prepare_parts(&white_frame),A2FO_REFIT_INVALID,
          "untextured part rejects nonzero texture revision");
    white_part.texture_revision=0;
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),
       "clear untextured white target");
    quad(device,.7f,0xff801020,0,0,W,H,true);
    check(api->prepare_parts(&white_frame),0,"prepare untextured white part");
    check(api->draw_prepared(1,9),0,"draw untextured white part");
    const auto white_pixel=pixel(read(api,9),32,32);
    need(white_pixel.r>220&&white_pixel.g>220&&white_pixel.b>220,
         "untextured UNLIT material samples opaque white");
    quad(device,.3f,0xffffff00,30,30,35,35,true);
    const auto white_depth=pixel(read(api,9),32,32);
    need(white_depth.r==white_pixel.r&&white_depth.g==white_pixel.g&&
         white_depth.b==white_pixel.b,"untextured white part writes depth");
    white_part.flags=0;white_part.ambient[0]=white_part.ambient[1]=white_part.ambient[2]=0;
    white_frame.serial=10;
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff801020,1.f,0),
       "clear unlit control target");
    check(api->prepare_parts(&white_frame),0,"prepare zero-light control");
    check(api->draw_prepared(1,10),0,"draw zero-light control");
    const auto unlit_control=pixel(read(api,10),32,32);
    need(unlit_control.r<30&&unlit_control.g<30&&unlit_control.b<30,
         "zero-light material is black without UNLIT flag");

    // Display asks Refit before acquiring a backend draw context. A successful
    // callback replaces one eligible native triangle; refusal keeps it native.
    Replacement replacement{};replacement.api=api;replacement.replace=replace;
    replacement.part=saved;identity34(replacement.part.world);
    replacement.frame=frame;replacement.frame.parts=&replacement.part;replacement.frame.part_count=1;
    replacement.frame.lights=nullptr;replacement.frame.light_count=0;
    identity44(replacement.frame.view);
    check(replace(replace_callback,&replacement),0,"register opaque replacement scope");
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear replacement target");
    quad(device,.2f,0xff202020,0,0,1,1,true); // Establish eligible fixed-function state.
    native_triangle_xyz(device,0xffff0000);
    need(replacement.calls==1 && replacement.prepared==0 && replacement.drawn==0
         && replacement.reentry==A2FO_REFIT_STATE,"eligible callback draws once and blocks re-registration");
    need(replacement.viewport[0]==0 && replacement.viewport[1]==0 &&
         replacement.viewport[2]==W && replacement.viewport[3]==H,"callback receives current viewport");
    auto replaced=read(api,11);RGB middle=pixel(replaced,32,32);
    need(middle.b>middle.r+45,"successful replacement suppresses native red triangle");

    replacement.claim=false;
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear refusal target");
    native_triangle_xyz(device,0xffff0000);
    need(replacement.calls==2 && replacement.reentry==A2FO_REFIT_STATE,"refusal callback invoked once");
    auto refused=read(api,11);middle=pixel(refused,32,32);
    need(middle.r>middle.b+80 && middle.r>middle.g+80,"refusal preserves original red native draw");

    // Enabled ONE/ZERO/ADD is equivalent to an opaque draw and remains eligible.
    replacement.claim=true;
    ok(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE),"neutral source blend");
    ok(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ZERO),"neutral destination blend");
    ok(device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD),"neutral blend operation");
    ok(device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE),"enable neutral blending");
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear neutral blend target");
    native_triangle_xyz(device,0xffff0000);
    need(replacement.calls==3 && replacement.prepared==0 && replacement.drawn==0,
         "neutral enabled blend invokes replacement");
    middle=pixel(read(api,13),32,32);
    need(middle.b>middle.r+45,"neutral enabled blend replaces native triangle");

    // True blending and non-XYZ vertex formats remain native in this scope.
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear alpha target");
    ok(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA),"alpha source blend");
    ok(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA),"alpha destination blend");
    native_triangle_xyz(device,0xffff0000);
    need(replacement.calls==3,"alpha blend draw does not enter callback");
    middle=pixel(read(api,13),32,32);need(middle.r>middle.b+80,"alpha draw remains native");
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xffffffff,1.f,0),"clear destination-colour target");
    ok(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_DESTCOLOR),"destination-colour source blend");
    ok(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ZERO),"destination-colour destination blend");
    native_triangle_xyz(device,0xffff0000);
    need(replacement.calls==3,"destination-colour blend draw does not enter callback");
    middle=pixel(read(api,13),32,32);
    need(middle.r>middle.g+80 && middle.r>middle.b+80,"destination-colour draw remains native");
    ok(device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE),"restore opaque state");
    ok(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE),"restore source blend");
    ok(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ZERO),"restore destination blend");
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear RHW target");
    native_triangle(device,0xffff0000);
    need(replacement.calls==3,"RHW FVF does not enter callback");
    middle=pixel(read(api,13),32,32);need(middle.r>middle.b+80,"RHW draw remains native");
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear vertex blend target");
    ok(device->SetRenderState(D3DRS_VERTEXBLEND,1),"enable vertex blending");
    native_triangle_xyz(device,0xffff0000);
    need(replacement.calls==3,"vertex blend state does not enter callback");
    middle=pixel(read(api,13),32,32);need(middle.r>middle.b+80,"vertex blend draw remains native");
    ok(device->SetRenderState(D3DRS_VERTEXBLEND,0),"restore vertex blend state");
    const DWORD pixel_code[]={0xffff0101,D3DSIO_MOV,0x800f0000,0x90e40000,D3DSIO_END};
    DWORD pixel_shader=0;
    if(SUCCEEDED(device->CreatePixelShader(pixel_code,&pixel_shader))){
        ok(device->SetPixelShader(pixel_shader),"bind programmable pixel shader");
        ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),
           "clear programmable shader target");
        native_triangle_xyz(device,0xffff0000);
        need(replacement.calls==3,"programmable pixel shader does not enter callback");
        middle=pixel(read(api,13),32,32);
        need(middle.r>middle.b+80,"programmable pixel shader draw remains native");
        ok(device->SetPixelShader(0),"restore fixed-function pixel shader");
        ok(device->DeletePixelShader(pixel_shader),"delete fixture pixel shader");
    }else std::fprintf(stderr,"SKIP: programmable pixel shader unavailable in facade\n");
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear no-depth target");
    ok(device->SetRenderState(D3DRS_ZENABLE,FALSE),"disable depth");
    native_triangle_xyz(device,0xffff0000);
    need(replacement.calls==3,"depth-disabled draw does not enter callback");
    middle=pixel(read(api,13),32,32);need(middle.r>middle.b+80,"depth-disabled draw remains native");
    ok(device->SetRenderState(D3DRS_ZENABLE,TRUE),"restore depth test");

    IDirect3DSurface8 *back=nullptr,*depth=nullptr,*offscreen=nullptr;
    ok(device->GetRenderTarget(&back),"capture back buffer");
    ok(device->GetDepthStencilSurface(&depth),"capture depth buffer");
    ok(device->CreateRenderTarget(W,H,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,FALSE,&offscreen),"offscreen target");
    ok(device->SetRenderTarget(offscreen,depth),"bind non-main target");
    native_triangle_xyz(device,0xffff0000);
    need(replacement.calls==3,"non-main target draw does not enter callback");
    ok(device->SetRenderTarget(back,depth),"restore main target");
    offscreen->Release();depth->Release();back->Release();
    check(replace(nullptr,nullptr),0,"unregister replacement at scope return");

    // The explicit mesh scope admits only the observed 0x1c4 legacy layout.
    MeshReplacement mesh{};mesh.api=api;mesh.replace_mesh=replace_mesh;
    mesh.replace_opaque=replace;mesh.part=saved;identity34(mesh.part.world);
    mesh.frame=frame;mesh.frame.parts=&mesh.part;mesh.frame.part_count=1;
    mesh.frame.lights=nullptr;mesh.frame.light_count=0;identity44(mesh.frame.view);
    const MeshVertex mesh_vertices[3]={
        {16.f,16.f,.2f,1.f,0xffff0000,0,0.f,0.f},
        {48.f,16.f,.2f,1.f,0xffff0000,0,1.f,0.f},
        {32.f,48.f,.2f,1.f,0xffff0000,0,.5f,1.f}};
    mesh.vertices=reinterpret_cast<std::uintptr_t>(mesh_vertices);
    const auto draw_mesh=[&] {
        ok(device->SetVertexShader(0x1c4),"mesh native FVF");
        ok(device->BeginScene(),"mesh native begin");
        ok(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,mesh_vertices,sizeof(mesh_vertices[0])),
           "mesh native triangle");
        ok(device->EndScene(),"mesh native end");
    };
    quad(device,.2f,0xff202020,0,0,1,1,true);
    D3DVIEWPORT8 mesh_viewport{0,0,W,H,0.f,1.f};
    ok(device->SetViewport(&mesh_viewport),"mesh full-depth viewport");
    ok(device->SetPixelShader(0),"mesh fixed pixel stage");
    ok(device->SetRenderState(D3DRS_ZENABLE,TRUE),"mesh depth test");
    ok(device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE),"mesh depth write");
    ok(device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL),"mesh depth comparison");
    ok(device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE),"mesh opaque blend");
    ok(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE),"mesh neutral source blend");
    ok(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ZERO),"mesh neutral destination blend");
    ok(device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD),"mesh neutral blend operation");
    ok(device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE),"mesh alpha test off");
    ok(device->SetRenderState(D3DRS_ALPHAREF,0),"mesh alpha reference");
    ok(device->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_ALWAYS),"mesh alpha comparison");
    ok(device->SetRenderState(D3DRS_FOGENABLE,FALSE),"mesh fog off");
    ok(device->SetRenderState(D3DRS_STENCILENABLE,FALSE),"mesh stencil off");
    ok(device->SetRenderState(D3DRS_CLIPPLANEENABLE,0),"mesh clip planes off");
    ok(device->SetRenderState(D3DRS_ZBIAS,0),"mesh depth bias off");
    ok(device->SetRenderState(D3DRS_COLORWRITEENABLE,0xf),"mesh colour writes");
    ok(device->SetRenderState(D3DRS_VERTEXBLEND,0),"mesh vertex blend off");
    check(replace_mesh(replace_mesh_callback,&mesh),0,"register explicit mesh scope");
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),
       "clear explicit mesh replacement target");
    draw_mesh();
    need(mesh.calls==1&&mesh.prepared==0&&mesh.drawn==0&&
         mesh.mesh_reentry==A2FO_REFIT_STATE&&mesh.opaque_reentry==A2FO_REFIT_STATE,
         "explicit mesh callback draws once and blocks both registrations during callback");
    need(mesh.draw.size==sizeof(A2FORefitDraw2)&&mesh.draw.slot==72&&mesh.draw.caller!=0&&
         mesh.draw.vertex_format==0x1c4&&mesh.draw.viewport[0]==0&&mesh.draw.viewport[1]==0&&
         mesh.draw.viewport[2]==W&&mesh.draw.viewport[3]==H&&mesh.draw.alpha_test==0&&
         mesh.draw.arg_count==4&&mesh.draw.args[0]==D3DPT_TRIANGLELIST&&
         mesh.draw.args[1]==1&&mesh.draw.args[2]==mesh.vertices&&
         mesh.draw.args[3]==sizeof(MeshVertex)&&mesh.draw.sequence!=0,
         "mesh callback receives copied exact draw descriptor and arguments");
    middle=pixel(read(api,21),32,32);
    need(middle.b>middle.r+45,"explicit mesh claim suppresses native red triangle");
    const auto native_mesh=[&](const char* reason){
        need(mesh.calls==1,reason);
        const RGB colour=pixel(read(api,21),32,32);
        need(colour.r>colour.b+80&&colour.r>colour.g+80,reason);
    };
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),
       "clear different RHW layout target");
    native_triangle(device,0xffff0000);
    native_mesh("other RHW layout stays native");
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),
       "clear blended mesh target");
    ok(device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE),"enable actual mesh blending");
    ok(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA),"mesh alpha source blend");
    ok(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA),"mesh alpha destination blend");
    draw_mesh();native_mesh("actual blended mesh stays native");
    ok(device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE),"restore mesh opaque blend");
    ok(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE),"restore mesh source blend");
    ok(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ZERO),"restore mesh destination blend");
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),
       "clear alpha-tested mesh target");
    ok(device->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE),"enable native mesh alpha test");
    draw_mesh();native_mesh("alpha-tested mesh stays native");
    ok(device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE),"restore mesh alpha test");
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),
       "clear wrong depth comparison target");
    ok(device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESS),"set wrong mesh depth comparison");
    draw_mesh();native_mesh("wrong depth comparison stays native");
    ok(device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL),"restore mesh depth comparison");
    check(replace(replace_callback,&replacement),0,"generic registration clears mesh scope");
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),
       "clear cleared mesh scope target");
    draw_mesh();native_mesh("generic scope cleared explicit mesh registration");
    check(replace_mesh(replace_mesh_callback,&mesh),0,"mesh registration clears generic scope");
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),
       "clear cleared generic scope target");
    native_triangle_xyz(device,0xffff0000);
    need(mesh.calls==1&&replacement.calls==3,
         "mesh registration cleared generic and excludes XYZ draw");
    middle=pixel(read(api,21),32,32);
    need(middle.r>middle.b+80,"XYZ draw remains native under mesh scope");
    check(replace_mesh(nullptr,nullptr),0,"unregister explicit mesh scope");

    // The fixture's authored cross product is +Z; scene::NORMAL_SIGN=-1
    // makes its lit normal -Z. sod.hlsl uses dot(n,-directional_ray) and
    // dot(n,point_position-world_position), so +Z rays and -Z points face it.
    white_part.flags=0;white_part.ambient[0]=white_part.ambient[1]=white_part.ambient[2]=0;
    white_part.self_illum=0;white_frame.parts=&white_part;white_frame.part_count=1;
    white_frame.source_epoch=1;white_frame.lights=nullptr;white_frame.light_count=0;
    A2FORefitLight direct{};direct.kind=A2FO_REFIT_DIRECTIONAL;
    direct.direction[2]=1;direct.colour[0]=.5f;direct.colour[1]=.25f;
    direct.intensity=1;
    auto lit_pixel=[&](const A2FORefitLight& light,uint32_t serial){
        white_frame.serial=serial;white_frame.lights=&light;white_frame.light_count=1;
        ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,
                         0xff202020,1.f,0),"clear direct lighting control");
        check(api->prepare_parts(&white_frame),0,"prepare direct lighting control");
        check(api->draw_prepared(1,serial),0,"draw direct lighting control");
        return pixel(read(api,serial),32,32);
    };
    const auto directional_lit=lit_pixel(direct,22);
    need(directional_lit.r>80&&directional_lit.r>directional_lit.g+30&&
         directional_lit.g>20&&directional_lit.b<30,
         "front-facing directional light carries unsaturated RGB and intensity");
    direct.direction[2]=-1;
    const auto directional_back=lit_pixel(direct,23);
    need(directional_back.r<30&&directional_back.g<30&&directional_back.b<30,
         "opposite directional ray leaves lit white material dark");
    direct.direction[2]=1;direct.intensity=0;
    const auto zero_energy=lit_pixel(direct,24);
    need(zero_energy.r<30&&zero_energy.g<30&&zero_energy.b<30,
         "zero directional intensity leaves material dark");
    direct.intensity=.25f;
    const auto low_energy=lit_pixel(direct,25);
    direct.intensity=1;
    const auto high_energy=lit_pixel(direct,26);
    need(low_energy.r>10&&high_energy.r>low_energy.r+55&&high_energy.r<200,
         "directional intensity changes measurable unsaturated colour");

    A2FORefitLight point{};point.kind=A2FO_REFIT_POINT;
    point.position[2]=-1;point.colour[1]=.5f;point.intensity=1;
    point.start=2;point.range=1;
    const auto point_lit=lit_pixel(point,27);
    need(point_lit.g>80&&point_lit.g>point_lit.r+50&&point_lit.r<30&&
         point_lit.b<30,"front-facing point light reaches full falloff colour");
    point.position[2]=-10;
    const auto point_far=lit_pixel(point,28);
    need(point_far.r<30&&point_far.g<30&&point_far.b<30,
         "point light beyond start plus range leaves material dark");
    white_frame.lights=nullptr;white_frame.light_count=0;

    // ABI3 spotlights: the same white plane (lit normal -Z, centre pixel at the
    // origin). A spot at (0,0,-1) with start 2 has full distance falloff there; for
    // direction (sin t,0,cos t) sod.hlsl's cone term compares cos t with the
    // half-angle cosines of the FULL inner/outer angles.
    const A2FORefitApi3* api3=reinterpret_cast<const A2FORefitApi3*>(get(3));
    need(api3 && api3->size==sizeof(A2FORefitApi3) && api3->abi_version==3 &&
         (api3->capabilities&A2FO_REFIT_CAP_SPOTS) && (api3->capabilities&511)==511 &&
         api3->prepare_parts==api->prepare_parts && api3->draw_prepared==api->draw_prepared,
         "ABI3 table extends ABI2 unchanged");
    need(get(4)==nullptr,"unknown interface version rejected");
    const float deg=3.14159265f/180.f;
    auto spot_at=[&](float tilt_deg,float inner_deg,float outer_deg){
        A2FORefitSpot3 s{};s.position[2]=-1;s.direction[0]=std::sin(tilt_deg*deg);
        s.direction[2]=std::cos(tilt_deg*deg);s.colour[2]=.5f;s.intensity=1;
        s.start=2;s.range=1;s.inner=inner_deg*deg;s.outer=outer_deg*deg;return s;};
    A2FORefitFrame3 spot_frame{};
    auto spot_pixel=[&](const A2FORefitSpot3* spots,uint32_t count,uint32_t serial){
        spot_frame.base=white_frame;spot_frame.base.size=sizeof(A2FORefitFrame3);
        spot_frame.base.serial=serial;spot_frame.base.lights=nullptr;spot_frame.base.light_count=0;
        spot_frame.spots=spots;spot_frame.spot_count=count;
        ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear spot control");
        check(api3->prepare_parts3(&spot_frame),0,"prepare spot control");
        check(api3->draw_prepared(1,serial),0,"draw spot control");
        return pixel(read(api,serial),32,32);};
    const auto on_axis=spot_at(0,40,60);
    const auto spot_lit=spot_pixel(&on_axis,1,40);
    need(spot_lit.b>80&&spot_lit.b>spot_lit.r+50&&spot_lit.r<30&&spot_lit.g<30,
         "spot on axis lights the plane with its colour");
    const auto away=spot_at(180,40,60);
    const auto spot_away=spot_pixel(&away,1,41);
    need(spot_away.r<30&&spot_away.g<30&&spot_away.b<30,"spot pointing away leaves plane dark");
    const auto outside=spot_at(45,40,60);   // half-angles 20/30: 45 is outside
    const auto spot_out=spot_pixel(&outside,1,42);
    need(spot_out.r<30&&spot_out.g<30&&spot_out.b<30,"surface outside the outer cone stays dark");
    const auto edge=spot_at(25,40,60);      // between inner 20 and outer 30
    const auto spot_edge=spot_pixel(&edge,1,43);
    need(spot_edge.b>spot_away.b+10&&spot_edge.b+20<spot_lit.b,
         "between inner and outer cone the spot fades partially");
    const auto inner_edge=spot_at(15,40,60);// inside the inner cone: full colour
    const auto spot_inner=spot_pixel(&inner_edge,1,44);
    need(spot_inner.b+12>=spot_lit.b,"inside the inner cone the spot is at full colour");
    const auto hard_in=spot_at(18,40,40),hard_out=spot_at(22,40,40);
    const auto spot_hard_in=spot_pixel(&hard_in,1,45),spot_hard_out=spot_pixel(&hard_out,1,46);
    need(spot_hard_in.b>80&&spot_hard_out.b<30,"equal inner/outer angles give a hard edge");
    auto dark=on_axis;dark.intensity=0;
    const auto spot_zero=spot_pixel(&dark,1,47);
    need(spot_zero.b<30,"zero spot intensity contributes nothing");
    auto distant=on_axis;distant.position[2]=-10;
    const auto spot_far=spot_pixel(&distant,1,48);
    need(spot_far.b<30,"spot beyond start plus range leaves plane dark");
    const A2FORefitSpot3 two[2]={on_axis,away};
    const auto spot_two=spot_pixel(two,2,49);
    need(spot_two.b+12>=spot_lit.b&&spot_two.b>80,"an unrelated second spot does not dim the first");
    auto reject_spot=[&](A2FORefitFrame3 bad,const char* why,int32_t expected=A2FO_REFIT_INVALID){
        check(api3->prepare_parts3(&bad),expected,why);
        check(api3->draw_prepared(1,bad.base.serial),A2FO_REFIT_STALE,"failed spot prepare leaves no packet");};
    A2FORefitFrame3 good=spot_frame;good.base.serial=50;good.spots=&on_axis;good.spot_count=1;
    { auto s=on_axis;s.inner=50*deg;s.outer=40*deg;auto f=good;f.spots=&s;reject_spot(f,"inner wider than outer"); }
    { auto s=on_axis;s.outer=3.2f;auto f=good;f.spots=&s;reject_spot(f,"outer cone of pi or more"); }
    { auto s=on_axis;s.inner=0;auto f=good;f.spots=&s;reject_spot(f,"zero inner cone"); }
    { auto s=on_axis;s.direction[0]=s.direction[1]=s.direction[2]=0;auto f=good;f.spots=&s;reject_spot(f,"zero spot direction"); }
    { auto s=on_axis;s.range=0;auto f=good;f.spots=&s;reject_spot(f,"zero spot range"); }
    { auto s=on_axis;s.colour[0]=-1;auto f=good;f.spots=&s;reject_spot(f,"negative spot colour"); }
    { auto s=on_axis;s.position[0]=std::nanf("");auto f=good;f.spots=&s;reject_spot(f,"nonfinite spot position"); }
    { A2FORefitSpot3 five[5]={on_axis,on_axis,on_axis,on_axis,on_axis};auto f=good;f.spots=five;f.spot_count=5;reject_spot(f,"more than four spots",A2FO_REFIT_LIMIT); }
    { auto f=good;f.base.size=sizeof(A2FORefitFrame2);reject_spot(f,"ABI2-sized frame through prepare_parts3"); }
    { auto f=good;f.spots=nullptr;f.spot_count=1;reject_spot(f,"null spot span with count"); }
    // ABI3 per-part Blinn-Phong: white light travelling +Z, eye on the lit side at
    // (0,0,-5): at the centre the half vector equals the normal, so a blue specular
    // colour adds blue on top of the grey diffuse. Specular adds after texturing.
    A2FORefitLight white{};white.kind=A2FO_REFIT_DIRECTIONAL;white.direction[2]=1;
    white.colour[0]=white.colour[1]=white.colour[2]=.5f;white.intensity=1;
    auto phong_pixel=[&](const A2FORefitPartMaterial3* material,uint32_t count,const float eye[3],uint32_t serial){
        spot_frame.base=white_frame;spot_frame.base.size=sizeof(A2FORefitFrame3);spot_frame.base.serial=serial;
        spot_frame.base.lights=&white;spot_frame.base.light_count=1;
        spot_frame.base.eye[0]=eye[0];spot_frame.base.eye[1]=eye[1];spot_frame.base.eye[2]=eye[2];
        spot_frame.spots=nullptr;spot_frame.spot_count=0;spot_frame.materials=material;spot_frame.material_count=count;
        ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear phong control");
        check(api3->prepare_parts3(&spot_frame),0,"prepare phong control");
        check(api3->draw_prepared(1,serial),0,"draw phong control");
        return pixel(read(api,serial),32,32);};
    const float front_eye[3]={0,0,-5},grazing_eye[3]={5,0,-.2f};
    A2FORefitPartMaterial3 shine{{0,0,1},16};
    const auto no_material=phong_pixel(nullptr,0,front_eye,60);
    need(no_material.r>90&&std::abs(int(no_material.r)-int(no_material.b))<25,"no part material: grey diffuse only");
    const auto highlight=phong_pixel(&shine,1,front_eye,61);
    need(highlight.b>highlight.r+70&&highlight.r+25>=no_material.r,"shine specular highlight adds on top of diffuse");
    auto flat=shine;flat.power=0;
    const auto power_zero=phong_pixel(&flat,1,front_eye,62);
    need(std::abs(int(power_zero.b)-int(no_material.b))<25,"power 0 means no highlight");
    auto black=shine;black.specular[2]=0;
    const auto spec_zero=phong_pixel(&black,1,front_eye,63);
    need(std::abs(int(spec_zero.b)-int(no_material.b))<25,"zero specular colour means no highlight");
    auto sharp=shine;sharp.power=64;
    const auto grazing=phong_pixel(&sharp,1,grazing_eye,64);
    need(grazing.b+40<highlight.b,"off-axis eye with a sharp power gives a much weaker highlight");
    { A2FORefitFrame3 f=spot_frame;f.base.serial=65;f.materials=&shine;f.material_count=2;reject_spot(f,"material count differs from part count"); }
    { auto m=shine;m.specular[0]=-1;A2FORefitFrame3 f=spot_frame;f.base.serial=65;f.materials=&m;f.material_count=1;reject_spot(f,"negative specular"); }
    { auto m=shine;m.power=2000;A2FORefitFrame3 f=spot_frame;f.base.serial=65;f.materials=&m;f.material_count=1;reject_spot(f,"power above 1024"); }
    { auto m=shine;m.power=std::nanf("");A2FORefitFrame3 f=spot_frame;f.base.serial=65;f.materials=&m;f.material_count=1;reject_spot(f,"nonfinite power"); }
    { A2FORefitFrame3 f=spot_frame;f.base.serial=65;f.materials=nullptr;f.material_count=1;reject_spot(f,"null material span with count"); }

    // Specular maps (A2FO_REFIT_CAP_SPECULAR_MAPS): an uploaded texture's RGB masks the
    // highlight at the diffuse UVs; no map (or a null record) leaves it unmasked.
    need((api3->capabilities&A2FO_REFIT_CAP_SPECULAR_MAPS)!=0,"ABI3 advertises specular maps");
    {
        struct Map { const char* name; std::vector<uint8_t> dds; };
        Map maps[]{{"specwhite.dds",dds_blocks("DXT1",4,4,1,0,bc1(0xffff))},
                   {"specblack.dds",dds_blocks("DXT1",4,4,1,0,bc1(0x0000))},
                   {"specred.dds",dds_blocks("DXT1",4,4,1,0,bc1(0xf800))},
                   {"specblue.dds",dds_blocks("DXT1",4,4,1,0,bc1(0x001f))}};
        for(auto& m:maps){
            A2FORefitEncodedTexture2 e{sizeof(e),m.name,static_cast<uint32_t>(std::strlen(m.name)),1,
                                       m.dds.data(),static_cast<uint32_t>(m.dds.size()),0};
            check(api->upload_encoded_texture(&e),0,m.name);
        }
        // An upload invalidates readback; re-establish the unmasked reference first.
        const auto reference=phong_pixel(&shine,1,front_eye,66);
        need(reference.b>reference.r+70,"unmasked highlight after map uploads");
        auto map_frame=[&](const A2FORefitPartMap3* map,uint32_t count,uint32_t serial){
            A2FORefitFrame3Maps f{};f.frame=spot_frame;f.frame.base.size=sizeof(A2FORefitFrame3Maps);
            f.frame.base.serial=serial;f.frame.materials=&shine;f.frame.material_count=1;f.maps=map;f.map_count=count;
            return f;};
        auto map_pixel=[&](const char* name,uint32_t serial){
            A2FORefitPartMap3 m{name,name?static_cast<uint32_t>(std::strlen(name)):0u,name?1u:0u};
            auto f=map_frame(&m,1,serial);
            ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear specular map control");
            check(api3->prepare_parts3(reinterpret_cast<const A2FORefitFrame3*>(&f)),0,"prepare specular map control");
            check(api3->draw_prepared(1,serial),0,"draw specular map control");
            return pixel(read(api,serial),32,32);};
        const auto white_map=map_pixel("specwhite.dds",67);
        need(std::abs(int(white_map.b)-int(reference.b))<25,"white map leaves the highlight");
        const auto black_map=map_pixel("specblack.dds",68);
        need(std::abs(int(black_map.b)-int(no_material.b))<25&&black_map.b+50<reference.b,"black map removes the highlight");
        const auto red_map=map_pixel("specred.dds",69);
        need(red_map.b+50<reference.b,"a red map masks a blue highlight");
        const auto blue_map=map_pixel("specblue.dds",70);
        need(std::abs(int(blue_map.b)-int(reference.b))<25,"a blue map passes a blue highlight");
        const auto none=map_pixel(nullptr,71);
        need(std::abs(int(none.b)-int(reference.b))<25,"a null map record is unmasked");
        auto reject_map=[&](A2FORefitFrame3Maps bad,const char* why,int32_t expected){
            check(api3->prepare_parts3(reinterpret_cast<const A2FORefitFrame3*>(&bad)),expected,why);
            check(api3->draw_prepared(1,bad.frame.base.serial),A2FO_REFIT_STALE,"failed map prepare leaves no packet");};
        A2FORefitPartMap3 m{"specwhite.dds",13,1};
        { auto f=map_frame(&m,2,72);reject_map(f,"map count differs from part count",A2FO_REFIT_INVALID); }
        { auto f=map_frame(nullptr,1,72);reject_map(f,"null map span with count",A2FO_REFIT_INVALID); }
        { auto b=m;b.specular_revision=0;auto f=map_frame(&b,1,72);reject_map(f,"named map with revision 0",A2FO_REFIT_INVALID); }
        { auto b=m;b.specular_name_length=0;auto f=map_frame(&b,1,72);reject_map(f,"non-null name with zero length",A2FO_REFIT_INVALID); }
        { A2FORefitPartMap3 b{nullptr,0,1};auto f=map_frame(&b,1,72);reject_map(f,"no name with a revision",A2FO_REFIT_INVALID); }
        { auto b=m;b.specular_revision=2;auto f=map_frame(&b,1,72);reject_map(f,"stale map revision",A2FO_REFIT_STALE); }
        { A2FORefitPartMap3 b{"nomap.dds",9,1};auto f=map_frame(&b,1,72);reject_map(f,"missing map texture",A2FO_REFIT_CONTENT); }
        { auto f=map_frame(&m,1,72);f.frame.base.size=sizeof(A2FORefitFrame3Maps)+4;reject_map(f,"unknown frame3 size",A2FO_REFIT_INVALID); }
    }
    white_frame.eye[0]=white_frame.eye[1]=white_frame.eye[2]=0;

    // ABI2 keeps working unchanged after ABI3 use, and ABI1/2-size frames never carry spots.
    white_frame.serial=80;white_frame.lights=nullptr;white_frame.light_count=0;
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear post-spot ABI2 control");
    check(api->prepare_parts(&white_frame),0,"ABI2 prepare after ABI3 spots");
    check(api->draw_prepared(1,80),0,"ABI2 draw after ABI3 spots");
    const auto after_spots=pixel(read(api,80),32,32);
    need(after_spots.r<30&&after_spots.g<30&&after_spots.b<30,"ABI2 frame after spots has no leftover spot light");

    // BC1-BC7: block-compressed DDS goes to the GPU as is; each texture samples its solid block colour.
    {
        const auto green_px=[](RGB c){return c.g>c.r+45&&c.g>c.b+45;};
        const auto red_px=[](RGB c){return c.r>c.g+45&&c.r>c.b+45;};
        const auto grey_px=[](RGB c){return c.r>200&&c.g>200&&c.b>200;};
        const std::vector<uint8_t> alpha_opaque{255,255,0,0,0,0,0,0};
        struct Bc { const char* name; std::vector<uint8_t> dds; int kind; }; // 0 green, 1 red, 2 white
        std::vector<Bc> bcs;
        bcs.push_back({"bc1.dds",dds_blocks("DXT1",8,8,1,0,repeat(bc1(0x07e0),4)),0});
        bcs.push_back({"bc1red.dds",dds_blocks("DXT1",8,8,1,0,repeat(bc1(0xf800),4)),1});
        bcs.push_back({"bc2.dds",dds_blocks("DXT3",4,4,1,0,join(std::vector<uint8_t>(8,0xff),bc1(0x07e0))),0});
        bcs.push_back({"bc3.dds",dds_blocks("DXT5",4,4,1,0,join(alpha_opaque,bc1(0x07e0))),0});
        bcs.push_back({"bc4.dds",dds_blocks("ATI1",4,4,1,0,bc4(255)),2});
        bcs.push_back({"bc5.dds",dds_blocks("ATI2",4,4,1,0,join(bc4(0),bc4(255))),0});
        bcs.push_back({"bc5dx10.dds",dds_blocks("DX10",4,4,1,83,join(bc4(0),bc4(255))),0});
        bcs.push_back({"bc7.dds",dds_blocks("DX10",8,8,1,98,repeat(bc7_green(),4)),0});
        bcs.push_back({"bc7srgb.dds",dds_blocks("DX10",4,4,1,99,bc7_green()),0});
        // 8x8 with a full chain (8, 4, 2, 1): the sub-block levels are one block each; level 0 is sampled.
        bcs.push_back({"bc1mips.dds",dds_blocks("DXT1",8,8,4,0,join(repeat(bc1(0x07e0),4),repeat(bc1(0xf800),3))),0});
        bcs.push_back({"bc7mips.dds",dds_blocks("DX10",8,8,2,98,join(repeat(bc7_green(),4),bc7_green())),0});
        // Not a multiple of 4: stays on the CPU decode path and still samples correctly.
        bcs.push_back({"bc1small.dds",dds_blocks("DXT1",2,2,1,0,bc1(0x07e0)),0});
        A2FORefitStatus before{};before.size=sizeof(before);check(api->status(&before),0,"status before BC uploads");
        for(auto& b:bcs){
            A2FORefitEncodedTexture2 e{sizeof(e),b.name,static_cast<uint32_t>(std::strlen(b.name)),1,
                                       b.dds.data(),static_cast<uint32_t>(b.dds.size()),0};
            check(api->upload_encoded_texture(&e),0,b.name);
        }
        A2FORefitStatus after{};after.size=sizeof(after);check(api->status(&after),0,"status after BC uploads");
        need(after.textures==before.textures+bcs.size(),"every BC texture published");
        A2FORefitPart2 bc_part=replacement.part;bc_part.flags=A2FO_REFIT_UNLIT;
        bc_part.ambient[0]=bc_part.ambient[1]=bc_part.ambient[2]=1;bc_part.model_id=7;bc_part.model_revision=1;
        A2FORefitFrame2 bc_frame=replacement.frame;bc_frame.source_epoch=1;bc_frame.parts=&bc_part;bc_frame.part_count=1;
        bc_frame.lights=nullptr;bc_frame.light_count=0;
        uint32_t serial=81;
        for(auto& b:bcs){
            bc_part.texture_name=b.name;bc_part.texture_name_length=static_cast<uint32_t>(std::strlen(b.name));
            bc_part.texture_revision=1;bc_frame.serial=serial;
            ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear BC control");
            check(api->prepare_parts(&bc_frame),0,b.name);
            check(api->draw_prepared(1,serial),0,b.name);
            const auto img=read(api,serial);
            const unsigned g=count_where(img,green_px),r=count_where(img,red_px),w=count_where(img,grey_px);
            const bool pass=b.kind==0 ? g>15&&r==0&&w==0 : b.kind==1 ? r>15&&g==0 : w>15&&g==0&&r==0;
            if(!pass) std::fprintf(stderr,"%s: green=%u red=%u white=%u\n",b.name,g,r,w);
            need(pass,b.name);
            ++serial;
        }
        // Malformed compressed inputs are refused and publish nothing.
        auto truncated=dds_blocks("DX10",8,8,1,98,repeat(bc7_green(),3));
        auto cube=dds_blocks("DX10",4,4,1,98,bc7_green());word(cube,136,0x4);
        auto volume=dds_blocks("DX10",4,4,1,98,bc7_green());word(volume,132,4);
        auto array=dds_blocks("DX10",4,4,1,98,bc7_green());word(array,140,2);
        auto unknown=dds_blocks("DX10",4,4,1,95,bc7_green()); // BC6H unsupported
        auto mips_long=dds_blocks("DX10",4,4,4,98,repeat(bc7_green(),4));
        const std::pair<const char*,std::vector<uint8_t>*> bad_bc[]{{"truncated BC7",&truncated},{"cube BC7",&cube},
            {"volume BC7",&volume},{"array BC7",&array},{"BC6H",&unknown},{"too many BC7 levels",&mips_long}};
        for(auto& [why,bytes]:bad_bc){
            A2FORefitEncodedTexture2 e{sizeof(e),"badbc.dds",9,1,bytes->data(),static_cast<uint32_t>(bytes->size()),0};
            check(api->upload_encoded_texture(&e),A2FO_REFIT_CONTENT,why);
        }
        A2FORefitStatus refused{};refused.size=sizeof(refused);check(api->status(&refused),0,"status after BC refusals");
        need(refused.textures==after.textures,"refused BC uploads publish nothing");
        // A newer revision replaces a compressed texture in place; an older one is stale.
        auto red7=dds_blocks("DXT1",8,8,1,0,repeat(bc1(0xf800),4));
        A2FORefitEncodedTexture2 e{sizeof(e),"bc7.dds",7,2,red7.data(),static_cast<uint32_t>(red7.size()),0};
        check(api->upload_encoded_texture(&e),0,"replace BC7 with BC1 revision 2");
        e.revision=1;check(api->upload_encoded_texture(&e),A2FO_REFIT_STALE,"older BC revision stale");
        bc_part.texture_name="bc7.dds";bc_part.texture_name_length=7;bc_part.texture_revision=2;bc_frame.serial=serial;
        ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear BC replace control");
        check(api->prepare_parts(&bc_frame),0,"prepare replaced BC texture");
        check(api->draw_prepared(1,serial),0,"draw replaced BC texture");
        const auto replaced=read(api,serial);
        need(count_where(replaced,red_px)>15&&count_where(replaced,green_px)==0,"replaced BC texture samples new revision");
    }

    // A content reset must invalidate host pipeline cache and rebuild resources.
    check(api->reset_content(2),0,"reset content for second generation");
    auto fresh_blue=tga_blue(),fresh_sod=native11_fixture::model("unused.tga",false);
    A2FORefitEncodedTexture2 fresh_texture{sizeof(fresh_texture),blue_name,8,1,
        fresh_blue.data(),static_cast<uint32_t>(fresh_blue.size()),0};
    check(api->upload_encoded_texture(&fresh_texture),0,"reupload encoded texture after reset");
    Callback fresh_callback{};
    A2FORefitModel2 fresh_model{{sizeof(fresh_model),7,1,fresh_sod.data(),
        static_cast<uint32_t>(fresh_sod.size())},&fresh_callback,transform};
    nodes=0;check(api->upload_model_transformed(&fresh_model,&nodes),0,"reupload transformed model after reset");
    need(nodes==1 && fresh_callback.calls==1,"second generation model callback");
    replacement.frame.source_epoch=2;replacement.frame.serial=1;
    replacement.part.model_id=7;replacement.part.model_revision=1;
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff202020,1.f,0),"clear second generation target");
    check(api->prepare_parts(&replacement.frame),0,"prepare after cache reset");
    check(api->draw_prepared(2,1),0,"draw after cache reset");
    need(colour_count(read(api,1,2),true,true)>15,"second generation direct draw visible");
    auto fresh_white=native11_fixture::model("",false);
    A2FORefitModel fresh_white_model{sizeof(fresh_white_model),11,1,fresh_white.data(),
                                    static_cast<uint32_t>(fresh_white.size())};
    white_nodes=0;check(api->upload_model(&fresh_white_model,&white_nodes),0,
                        "reupload untextured model after content reset");
    need(white_nodes==1,"second generation untextured model has one node");
    white_part.flags=A2FO_REFIT_UNLIT;
    white_part.ambient[0]=white_part.ambient[1]=white_part.ambient[2]=1;
    white_frame.source_epoch=2;white_frame.serial=2;
    ok(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff801020,1.f,0),
       "clear second generation white target");
    check(api->prepare_parts(&white_frame),0,"prepare white after content reset");
    check(api->draw_prepared(2,2),0,"draw white after content reset");
    const auto reset_white=pixel(read(api,2,2),32,32);
    need(reset_white.r>220&&reset_white.g>220&&reset_white.b>220,
         "white fallback rebuilt after content reset");
    check(api->shutdown(),0,"shutdown");
    device->Release();d3d->Release();
    need(!IsWindowVisible(window.hwnd),"fixture HWND remained hidden");
    std::printf("PASS: %s ABI2 direct parts, encoded textures, transform, copies, depth, FF colour, rejection, ABI3 spotlights, Blinn-Phong, specular maps and BC1-BC7\n",backend);
}
}
int main(int argc,char** argv) {
    if(argc!=5){std::fprintf(stderr,"usage: fixture BACKEND DISPLAY_DLL RENDERER_DLL OUTPUT_DIR\n");return 2;}
    (void)argv[4];run(argv[1],argv[2],argv[3]);return 0;
}
