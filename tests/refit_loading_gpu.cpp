// Run only through the private headless runner. No game or input automation.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../modules/A2FORefitMenus/loading_gpu.hpp"
#include "../modules/A2FORefitMenus/loading_presentation.hpp"
#include "../modules/A2FORefitMenus/mission_backdrop.cpp"
#include "../modules/A2FORefitMenus/loading_native.cpp"
namespace a2fo::refit_menu::setup_native {bool begin_loading_handover(){return false;}}
namespace a2fo::refit_menu::menu_scene {
void end_handover(){}
bool launch_handover_active(){return false;}
bool layered_requested{};
unsigned scene_acks{};
std::shared_ptr<const Artwork> layered_frame;
void end_departure() {}
bool begin_departure(const char*) { return false; }
std::shared_ptr<const Artwork> loading_frame(int,int,bool) { return layered_requested?layered_frame:nullptr; }
void loading_presented(const std::shared_ptr<const Artwork>& image) { if(image==layered_frame)++scene_acks; }
std::shared_ptr<const Artwork> retained_frame() { return layered_requested?layered_frame:nullptr; }
bool requested() { return layered_requested; }
bool departing() { return layered_requested; }
std::uint64_t menu_epoch() { return 0; }
}
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
using namespace a2fo::refit_menu;
namespace {
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
IDirect3DDevice8* overlay_device{};
unsigned flush_calls{},overlay_calls{};
bool overlay_drawn{};
void* expected_release_self{};
ULONG expected_release_refs{};
unsigned release_calls{};
bool release_before_original{};
extern "C" void __fastcall fixture_flush(void*,void*) { ++flush_calls; }
extern "C" const char* __fastcall fixture_map_name(void*,void*) { return "FixtureMap"; }
extern "C" const char* __fastcall fixture_map_title(void*,void*) { return "Fixture Title"; }
extern "C" void __stdcall fixture_original_bar(float,void*,float) {
    ++overlay_calls;
    if(!overlay_device)return;
    struct Vertex { float x,y,z,rhw; DWORD color; };
    const Vertex bar[]{{40,560,0,1,0xffe03020},{400,560,0,1,0xffe03020},
                       {40,580,0,1,0xffe03020},{400,580,0,1,0xffe03020}};
    overlay_drawn=SUCCEEDED(overlay_device->SetRenderState(D3DRS_ZENABLE,FALSE))&&
        SUCCEEDED(overlay_device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE))&&
        SUCCEEDED(overlay_device->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_DIFFUSE))&&
        SUCCEEDED(overlay_device->SetPixelShader(0))&&
        SUCCEEDED(overlay_device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1))&&
        SUCCEEDED(overlay_device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE))&&
        SUCCEEDED(overlay_device->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_CURRENT))&&
        SUCCEEDED(overlay_device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE))&&
        SUCCEEDED(overlay_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,bar,sizeof(Vertex)));
}
extern "C" void __fastcall fixture_original_release(void* self,void*) {
    ++release_calls;
    const ULONG references=overlay_device->AddRef();overlay_device->Release();
    release_before_original=self==expected_release_self&&
        references==expected_release_refs&&
        !loading_native::native_frame_seen&&!loading_native::loading_wrapper&&
        loading_native::loading_session;
}
void jump_to(BYTE* from,const void* to){
    from[0]=0xe9;
    const auto delta=static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(to)-
        reinterpret_cast<std::uintptr_t>(from+5));
    std::memcpy(from+1,&delta,sizeof(delta));
}
template<class T> void put(BYTE* base,std::size_t offset,const T& value){
    std::memcpy(base+offset,&value,sizeof(value));
}
DWORD pixel(IDirect3DDevice8* device,int x,int y){
    IDirect3DSurface8* buffer{};
    require(SUCCEEDED(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&buffer))&&buffer,"extra frame backbuffer");
    D3DLOCKED_RECT lock{};
    require(SUCCEEDED(buffer->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"extra frame readback");
    const DWORD value=reinterpret_cast<DWORD*>(static_cast<BYTE*>(lock.pBits)+y*lock.Pitch)[x]&0xffffffu;
    buffer->UnlockRect();buffer->Release();return value;
}
void check_layered(IDirect3DDevice8* device,ULONG reference_baseline){
    loading_native::LoadingTextureCache world_cache,ui_cache;
    Artwork world;world.width=320;world.height=240;
    world.pixels.assign(320*240,0x00ffffffu); // Opaque scene upload ignores source alpha.
    Artwork ui;ui.width=800;ui.height=600;ui.pixels.assign(800*600,0u);
    for(int y=100;y<180;++y)for(int x=600;x<700;++x)
        ui.pixels[static_cast<std::size_t>(y)*800+x]=0xffd02010u;
    IDirect3DTexture8* sentinel{};
    require(SUCCEEDED(device->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&sentinel)),
            "layered state sentinel texture");
    require(SUCCEEDED(device->SetTexture(0,sentinel)),"layered prior texture");
    D3DVIEWPORT8 before{7,11,780,570,0.f,1.f};
    require(SUCCEEDED(device->SetViewport(&before))&&
            SUCCEEDED(device->SetRenderState(D3DRS_ZENABLE,TRUE))&&
            SUCCEEDED(device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE))&&
            SUCCEEDED(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1))&&
            SUCCEEDED(device->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_DIFFUSE))&&
            SUCCEEDED(device->SetPixelShader(0)),"layered prior state");
    const auto draw=[&](loading_native::LoadingTextureCache* wc,
                        loading_native::LoadingTextureCache* uc,std::uint64_t wr,std::uint64_t ur){
        require(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff010203,1.f,0)),"layered clear");
        require(SUCCEEDED(device->BeginScene()),"layered scene begin");
        require(loading_native::draw_loading_layers(device,world,ui,800,600,wc,uc,wr,ur),
                "layered world and ARGB UI draw");
        D3DVIEWPORT8 after{};DWORD depth{},blend{},colour_op{},shader{},pixel_shader{};
        IDirect3DBaseTexture8* bound{};
        require(SUCCEEDED(device->GetViewport(&after))&&!std::memcmp(&after,&before,sizeof(before))&&
                SUCCEEDED(device->GetRenderState(D3DRS_ZENABLE,&depth))&&depth==TRUE&&
                SUCCEEDED(device->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend))&&blend==FALSE&&
                SUCCEEDED(device->GetTextureStageState(0,D3DTSS_COLOROP,&colour_op))&&colour_op==D3DTOP_SELECTARG1&&
                SUCCEEDED(device->GetVertexShader(&shader))&&shader==(D3DFVF_XYZRHW|D3DFVF_DIFFUSE)&&
                SUCCEEDED(device->GetPixelShader(&pixel_shader))&&pixel_shader==0&&
                SUCCEEDED(device->GetTexture(0,&bound))&&bound==sentinel,
                "layered draw restores viewport, states, shaders and texture");
        if(bound)bound->Release();
        require(SUCCEEDED(device->EndScene()),"layered scene end");
    };
    draw(&world_cache,&ui_cache,1,1);
    require(world_cache.stats().creations==1&&world_cache.stats().uploads==1&&
            ui_cache.stats().creations==1&&ui_cache.stats().uploads==1,
            "layered first uploads");
    const DWORD top_left=pixel(device,8,8),top_right=pixel(device,790,8);
    const DWORD foot_left=pixel(device,8,590),foot_right=pixel(device,790,590);
    require(((top_left>>16)&255)>95&&((top_left>>16)&255)<140&&
            ((top_right>>16)&255)>220&&((top_right>>16)&255)<=255&&
            ((foot_left>>16)&255)>75&&((foot_left>>16)&255)<115&&
            ((foot_right>>16)&255)>75&&((foot_right>>16)&255)<115,
            "independently scaled white world has expected corner shading through transparent UI");
    require(pixel(device,650,140)==0xd02010u,"opaque UI appears over world");
    draw(&world_cache,&ui_cache,1,1);
    require(world_cache.stats().uploads==1&&ui_cache.stats().uploads==1,
            "unchanged revisions reuse both textures");
    ui.pixels[140*800+650]=0xff20b040u;
    draw(&world_cache,&ui_cache,1,2);
    require(world_cache.stats().uploads==1&&ui_cache.stats().uploads==2&&
            pixel(device,650,140)==0x20b040u,"UI revision uploads independently");
    require(SUCCEEDED(device->BeginScene()),"alpha semantics scene begin");
    require(loading_native::draw_loading_image(device,ui,ui_cache,2),"opaque API reuses UI cache");
    require(SUCCEEDED(device->EndScene()),"alpha semantics scene end");
    require(ui_cache.stats().uploads==3,"alpha semantics changes cached upload");
    draw(&world_cache,&ui_cache,1,2);
    require(ui_cache.stats().uploads==4&&pixel(device,8,8)==top_left,
            "ARGB semantics restores transparent UI at equal revision");
    draw(nullptr,nullptr,0,0);
    require(pixel(device,650,140)==0x20b040u&&pixel(device,8,8)==top_left,
            "transient null-cache path renders both layers");
    require(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff315779,1.f,0))&&
            SUCCEEDED(device->BeginScene()),"aliased-cache target prepared");
    require(!loading_native::draw_loading_layers(device,world,ui,800,600,
                                                 &world_cache,&world_cache,1,2),
            "aliased layer caches rejected before drawing");
    require(SUCCEEDED(device->EndScene())&&pixel(device,650,140)==0x315779u,
            "rejected layer caches preserve native target pixels");
    require(SUCCEEDED(device->SetTexture(0,nullptr)),"release prior texture binding");
    sentinel->Release();world_cache.reset();ui_cache.reset();
    const ULONG references=device->AddRef();device->Release();
    require(references==reference_baseline,"layered caches and transient textures release device");
    std::fprintf(stderr,"loading fixture: layered scaling, alpha, shading, cache semantics, state and teardown passed\n");
}
void check_extra_frame(IDirect3DDevice8* device,ULONG reference_baseline){
    namespace b=loading_bindings;
    struct Synthetic {
        BYTE* engine=static_cast<BYTE*>(VirtualAlloc(nullptr,0x400000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        BYTE* fo=static_cast<BYTE*>(VirtualAlloc(nullptr,0x250000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        ~Synthetic(){
            menu_scene::layered_requested=false;menu_scene::layered_frame.reset();
            loading_native::enabled.store(false);
            loading_native::animation_armed.store(false);
            loading_native::cache_armed.store(false);
            loading_native::reset_textures();loading_native::painted.reset();
            loading_native::engine=nullptr;loading_native::fo=nullptr;
            loading_native::next_release.store(nullptr);
            overlay_device=nullptr;
            if(engine)VirtualFree(engine,0,MEM_RELEASE);
            if(fo)VirtualFree(fo,0,MEM_RELEASE);
        }
    } synthetic;
    require(synthetic.engine&&synthetic.fo,"private engine and FO images");
    BYTE storm[0x100]{},wrapper[0x100]{},vtable[0x40]{};
    BYTE transport[0x40]{},setup[0x20]{},data[0xe1]{};
    auto* engine=synthetic.engine;auto* fo=synthetic.fo;
    jump_to(engine+b::device_flush,reinterpret_cast<const void*>(&fixture_flush));
    jump_to(engine+b::map_name,reinterpret_cast<const void*>(&fixture_map_name));
    jump_to(engine+b::map_title,reinterpret_cast<const void*>(&fixture_map_title));
    void* storm_pointer=storm;void* wrapper_pointer=wrapper;void* vtable_pointer=vtable;
    void* transport_pointer=transport;void* setup_pointer=setup;void* data_pointer=data;
    void* flush_pointer=engine+b::device_flush;
    put(engine,0x3ad508,storm_pointer);put(storm,0xc0,0u);put(storm,0xcc,wrapper_pointer);
    put(wrapper,0,vtable_pointer);put(vtable,0x38,flush_pointer);put(wrapper,0x90,device);
    put(engine,b::transport,transport_pointer);put(transport,0x30,setup_pointer);put(setup,0x10,data_pointer);
    put(engine,b::status_visible,static_cast<BYTE>(1));
    put(engine,b::fraction_remaining,.5f);
    float cfg[13]{};cfg[0]=.03f;cfg[1]=.5f;cfg[2]=.05f;cfg[3]=.93f;
    std::memcpy(fo+b::bar_height,cfg,sizeof(cfg));
    loading_native::engine=reinterpret_cast<HMODULE>(engine);
    loading_native::fo=reinterpret_cast<HMODULE>(fo);
    loading_native::players_ready=false;
    loading_native::roots.clear();loading_native::content_loaded=false;
    loading_native::painted.reset();loading_native::paint_revision=0;
    loading_native::loading_wrapper=wrapper;loading_native::loading_first=0.f;
    loading_native::loading_session=true;loading_native::native_frame_seen=true;
    loading_native::inside=false;loading_native::owner.store(GetCurrentThreadId());
    loading_native::next.store(reinterpret_cast<void*>(&fixture_original_bar));
    loading_native::next_release.store(reinterpret_cast<void*>(&fixture_original_release));
    loading_native::next_extra=0;loading_native::extra_frames=0;
    loading_native::step_text="Fixture stage";
    auto scene=std::make_shared<Artwork>();scene->width=320;scene->height=240;
    scene->pixels.resize(320*240);
    for(int y=0;y<240;++y)for(int x=0;x<320;++x)
        scene->pixels[static_cast<std::size_t>(y)*320+x]=
            (static_cast<DWORD>(x*255/319)<<16)|(static_cast<DWORD>(y*255/239)<<8)|0x55u;
    menu_scene::layered_frame=scene;menu_scene::layered_requested=true;
    menu_scene::scene_acks=0;
    loading_native::cache_armed.store(true);
    loading_native::animation_armed.store(true);loading_native::enabled.store(true);
    overlay_device=device;flush_calls=overlay_calls=release_calls=0;
    overlay_drawn=release_before_original=false;
    expected_release_self=wrapper;expected_release_refs=reference_baseline;
    D3DVIEWPORT8 native_view{2,3,796,594,0.f,1.f};
    require(SUCCEEDED(device->SetViewport(&native_view))&&
            SUCCEEDED(device->SetRenderState(D3DRS_ZENABLE,TRUE))&&
            SUCCEEDED(device->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_TEMP)),
            "native viewport and state prepared");
    require(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff132435,1.f,0)),
            "extra frame target prepared");
    loading_native::extra_frame();
    require(loading_native::extra_frames==1&&overlay_calls==1&&overlay_drawn&&flush_calls==2,
            "extra frame presents once with original native overlay once");
    require(menu_scene::scene_acks==1&&!loading_native::pending_loading_scene,
            "completed extra present acknowledges its scene exactly once");
    require(loading_native::texture_cache.stats().creations>0,
            "extra frame created a production cached texture");
    require(loading_native::scene_texture_cache.stats().creations>0&&
            loading_native::painted_layered&&loading_native::painted&&
            loading_native::painted->width==800&&loading_native::painted->height==600,
            "production extra frame used both cached layers and transparent painter");
    const auto& ui_pixels=loading_native::painted->pixels;
    require(std::any_of(ui_pixels.begin(),ui_pixels.end(),[](DWORD p){return (p>>24)==0;})&&
            std::any_of(ui_pixels.begin(),ui_pixels.end(),[](DWORD p){return (p>>24)==255;}),
            "production ARGB UI has transparent and opaque pixels");
    D3DVIEWPORT8 restored{};DWORD depth{},result_arg{};
    require(SUCCEEDED(device->GetViewport(&restored))&&
            !std::memcmp(&restored,&native_view,sizeof(restored))&&
            SUCCEEDED(device->GetRenderState(D3DRS_ZENABLE,&depth))&&depth==TRUE&&
            SUCCEEDED(device->GetTextureStageState(0,D3DTSS_RESULTARG,&result_arg))&&
            result_arg==D3DTA_TEMP,"extra frame restores native viewport and state");
    loading_native::extra_frame();
    require(loading_native::extra_frames==1&&overlay_calls==1&&flush_calls==2,
            "immediate repeat respects extra frame cadence");
    loading_native::next_extra=0;
    require(SUCCEEDED(device->BeginScene()),"native scene opened before extra attempt");
    loading_native::extra_frame();
    require(loading_native::extra_frames==1&&overlay_calls==1&&flush_calls==2,
            "already open native scene rejects nested draw and original callback");
    require(SUCCEEDED(device->EndScene()),"native scene remains open for native owner");
    loading_native::next_extra=0;
    const float invalid=std::numeric_limits<float>::quiet_NaN();
    put(fo,b::bar_height,invalid);
    require(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff2468ac,1.f,0))&&
            pixel(device,700,500)==0x2468ac,"native fallback pixel prepared");
    loading_native::extra_frame();
    require(loading_native::extra_frames==1&&overlay_calls==1&&flush_calls==2&&
            pixel(device,700,500)==0x2468ac,
            "invalid FO bar keeps native backbuffer and skips extra present");
    require(menu_scene::scene_acks==1&&!loading_native::pending_loading_scene,
            "refused/open/cadence/fallback draws do not advance scene clock");
    loading_native::a2fo_refit_release_device(wrapper,nullptr);
    require(release_calls==1&&release_before_original&&
            !loading_native::native_frame_seen&&!loading_native::loading_wrapper&&
            loading_native::loading_session,
            "native release observer clears cache and wrapper before forwarding once");
    const ULONG after=device->AddRef();device->Release();
    require(after==reference_baseline,"native release observer restores device reference baseline");
    menu_scene::layered_requested=false;menu_scene::layered_frame.reset();
    std::fprintf(stderr,"loading fixture: production layered extra frame, native overlay, cadence, scene guard, fallback and release cleanup passed\n");
}
struct Dib {
    HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap{};HGDIOBJ old{};void* bits{};
    ~Dib(){if(old)SelectObject(dc,old);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
    void create(int w,int h){
        BITMAPINFO i{};i.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);i.bmiHeader.biWidth=w;i.bmiHeader.biHeight=-h;
        i.bmiHeader.biPlanes=1;i.bmiHeader.biBitCount=32;
        bitmap=CreateDIBSection(dc,&i,DIB_RGB_COLORS,&bits,nullptr,0);require(bitmap&&bits,"offscreen DIB");
        old=SelectObject(dc,bitmap);require(old&&old!=HGDI_ERROR,"select DIB");
    }
};
}
int main(int argc,char** argv){
    if(argc!=2){std::fprintf(stderr,"usage: fixture PRIVATE_HOST_ROOT\n");return 2;}
    HWND window{};IDirect3D8* d3d{};IDirect3DDevice8* device{};IDirect3DDevice8* second_device{};
    loading_native::LoadingTextureCache cache;
    try{
        std::fprintf(stderr,"loading fixture: entering staged proxy\n");
        require(SetDllDirectoryA(argv[1]),"private DLL root");
        // Explicit dynamic load ensures the staged native d3d8to9 wrapper is used.
        const std::string path=std::string(argv[1])+"\\d3d8.dll";
        HMODULE library=LoadLibraryA(path.c_str());require(library!=nullptr,"staged D3D8 wrapper");
        std::fprintf(stderr,"loading fixture: proxy loaded\n");
        using Create=IDirect3D8*(__stdcall*)(UINT);
        const auto symbol=GetProcAddress(library,"Direct3DCreate8");Create create{};
        static_assert(sizeof(create)==sizeof(symbol));std::memcpy(&create,&symbol,sizeof(create));
        require(create!=nullptr,"D3D8 export");
        d3d=create(D3D_SDK_VERSION);require(d3d!=nullptr,"D3D8 creation");
        std::fprintf(stderr,"loading fixture: D3D8 created\n");
        WNDCLASSEXA cls{};cls.cbSize=sizeof(cls);cls.hInstance=GetModuleHandleA(nullptr);
        cls.lpfnWndProc=DefWindowProcA;cls.lpszClassName="A2FOHeadlessLoadingFixture";
        require(RegisterClassExA(&cls)!=0,"class registration");
        window=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,cls.lpszClassName,"headless loading fixture",WS_POPUP,
            0,0,800,600,nullptr,nullptr,cls.hInstance,nullptr);
        require(window&&!IsWindowVisible(window),"hidden nonactivating fixture window");
        D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=800;pp.BackBufferHeight=600;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
        pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.Windowed=TRUE;
        pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;pp.Flags=D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
        require(SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device)),"headless device");
        std::fprintf(stderr,"loading fixture: device created\n");
        const auto device_references=[&]{const ULONG n=device->AddRef();device->Release();return n;};
        const ULONG device_reference_baseline=device_references();
        for(const auto size:{std::pair<int,int>{800,600},{1920,1080}}){
        const int width=size.first,height=size.second;
        if(width!=800){
            cache.reset();
            require(device_references()==device_reference_baseline,"reset dropped cached device reference");
            pp.BackBufferWidth=width;pp.BackBufferHeight=height;
            require(SUCCEEDED(device->Reset(&pp)),"Reset without retained loading GPU resources");
        }
        LoadingModel model;model.title="A New Frontier";model.mode="INSTANT ACTION";model.status="Preparing the game";
        model.progress=.5f;model.briefing={"The Refit layout keeps the original Fleet Operations loading overlays."};
        model.protected_regions={{40,560,720,20},{600,400,180,120}};
        Dib dib;dib.create(width,height);require(paint_loading(dib.dc,width,height,model),"GDI loading presentation");require(GdiFlush(),"GDI completed");
        Artwork image;image.width=width;image.height=height;
        image.pixels.assign(static_cast<std::uint32_t*>(dib.bits),static_cast<std::uint32_t*>(dib.bits)+width*height);
        const auto initial=cache.stats();
        for(int repeat=0;repeat<3;++repeat){
            require(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff030405,1,0)),"clear");
            require(SUCCEEDED(device->BeginScene()),"engine-owned scene begin");
            D3DVIEWPORT8 before{2,3,static_cast<DWORD>(width-4),static_cast<DWORD>(height-6),0,1};require(SUCCEEDED(device->SetViewport(&before)),"prior viewport");
            require(SUCCEEDED(device->SetRenderState(D3DRS_ZENABLE,TRUE)),"prior depth");
            require(SUCCEEDED(device->SetTexture(0,nullptr)),"prior texture");
            require(SUCCEEDED(device->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_TEMP)),"prior texture result");
            const DWORD start=GetTickCount();
            if(repeat==2)image.pixels[590*width+790]=0xff2266aa;
            require(loading_native::draw_loading_image(device,image,cache,repeat==2?2:1),"real D3D8 cached loading upload/draw");
            const auto counts=cache.stats();
            require(counts.creations==initial.creations+1,"repeat reused managed texture");
            require(counts.uploads==initial.uploads+(repeat==2?2:1),"revision controls upload");
            std::fprintf(stderr,"loading fixture: %dx%d image upload/draw API elapsed=%lu ms\n",width,height,static_cast<unsigned long>(GetTickCount()-start));
            D3DVIEWPORT8 after{};DWORD depth{};IDirect3DBaseTexture8* texture{};
            require(SUCCEEDED(device->GetViewport(&after))&&!std::memcmp(&before,&after,sizeof(before)),"viewport restored");
            require(SUCCEEDED(device->GetRenderState(D3DRS_ZENABLE,&depth))&&depth==TRUE,"depth restored");
            require(SUCCEEDED(device->GetTexture(0,&texture))&&!texture,"texture binding restored");
            DWORD argument{};require(SUCCEEDED(device->GetTextureStageState(0,D3DTSS_RESULTARG,&argument))&&argument==D3DTA_TEMP,"texture stage restored");
            // Simulate a later native FO rectangle draw: it must remain on top.
            D3DVIEWPORT8 full{0,0,static_cast<DWORD>(width),static_cast<DWORD>(height),0,1};device->SetViewport(&full);
            device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
            device->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);device->SetPixelShader(0);
            device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
            device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
            device->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_CURRENT);
            device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
            struct V{float x,y,z,rhw;DWORD c;};
            const V bar[]{{40,560,0,1,0xffe03020},{400,560,0,1,0xffe03020},{40,580,0,1,0xffe03020},{400,580,0,1,0xffe03020}};
            require(SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,bar,sizeof(V))),"later native overlay");
            require(SUCCEEDED(device->EndScene()),"engine-owned scene end");
            IDirect3DSurface8* buffer{};require(SUCCEEDED(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&buffer)),"backbuffer");
            D3DLOCKED_RECT lock{};require(SUCCEEDED(buffer->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"readback");
            const auto sample=[&](int x,int y){return reinterpret_cast<DWORD*>(static_cast<BYTE*>(lock.pBits)+y*lock.Pitch)[x]&0xffffffu;};
            require(sample(100,570)==0xe03020,"native overlay remains visible");
            require(sample(790,590)==(image.pixels[590*width+790]&0xffffffu),"loading image is not blank/native background");
            buffer->UnlockRect();buffer->Release();
        }
        require(SUCCEEDED(device->BeginScene()),"legacy transient scene begin");
        require(loading_native::draw_loading_image(device,image),"legacy transient upload/draw");
        require(SUCCEEDED(device->EndScene()),"legacy transient scene end");
        require(cache.stats().creations==initial.creations+1,
                "legacy draw did not opt in to cache");
        if(width==800){
            Artwork smaller=image;smaller.width=400;smaller.height=300;smaller.pixels.resize(400*300);
            require(SUCCEEDED(device->BeginScene()),"resized cache scene begin");
            require(loading_native::draw_loading_image(device,smaller,cache,3),"resized cached image draw");
            require(SUCCEEDED(device->EndScene()),"resized cache scene end");
            const auto counts=cache.stats();
            require(counts.creations==initial.creations+2&&counts.uploads==initial.uploads+3,
                    "resize replaced texture and uploaded image");
        }
        // Exercise the exact production capture against the real D3D8 proxy.
        // The native engine is replaced with synthetic read-only field layouts;
        // no actual game executable, visible window or input is involved.
        {
            std::vector<BYTE> image(0x3ad510),storm(0x100),wrapper(0x100),vtable(0x40);
            mission_backdrop::engine=reinterpret_cast<HMODULE>(image.data());
            const auto pointer=[](std::vector<BYTE>& target,std::size_t offset,const void* p){
                std::memcpy(target.data()+offset,&p,sizeof(p));
            };
            pointer(image,0x3ad508,storm.data());pointer(storm,0xcc,wrapper.data());
            pointer(wrapper,0,vtable.data());pointer(vtable,0x38,image.data()+loading_bindings::device_flush);
            pointer(wrapper,0x90,device);
            Artwork captured;
            require(SUCCEEDED(device->Present(nullptr,nullptr,nullptr,nullptr)),"private device present before capture");
            require(mission_backdrop::capture(captured),"production D3D8 front-buffer capture");
            // Window stays 800x600 even after the test changes backbuffer size:
            // capture must crop to the actual client, not include the desktop.
            require(captured.width==800&&captured.height==600&&captured.pixels.size()==480000,
                    "mission backdrop is cropped to known game client");
            void* invalid=reinterpret_cast<void*>(1);pointer(vtable,0x38,invalid);
            require(!mission_backdrop::capture(captured),"unknown device wrapper refused before indirect calls");
            mission_backdrop::engine=nullptr;
            std::fprintf(stderr,"loading fixture: production gameplay capture/crop and wrapper refusal passed\n");
        }
        }
        cache.reset();
        require(device_references()==device_reference_baseline,"pre-extra reset dropped cached device reference");
        pp.BackBufferWidth=800;pp.BackBufferHeight=600;
        require(SUCCEEDED(device->Reset(&pp)),"private device reset for extra frame");
        check_layered(device,device_reference_baseline);
        check_extra_frame(device,device_reference_baseline);
        pp.BackBufferWidth=800;pp.BackBufferHeight=600;
        require(SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&second_device)),"second private device");
        const auto second_device_references=[&]{const ULONG n=second_device->AddRef();second_device->Release();return n;};
        const ULONG second_reference_baseline=second_device_references();
        Artwork switched;switched.width=800;switched.height=600;switched.pixels.assign(800*600,0xff3a5c7e);
        const auto before_switch=cache.stats();
        require(SUCCEEDED(second_device->BeginScene()),"second device scene begin");
        require(loading_native::draw_loading_image(second_device,switched,cache,2),"device change draws with replacement texture");
        require(SUCCEEDED(second_device->EndScene()),"second device scene end");
        const auto after_switch=cache.stats();
        require(after_switch.creations==before_switch.creations+1&&after_switch.uploads==before_switch.uploads+1,
                "device change replaced texture and uploaded despite equal revision");
        cache.reset();
        require(device_references()==device_reference_baseline,"final reset dropped cached device reference");
        require(second_device_references()==second_reference_baseline,"reset dropped second device reference");
        second_device->Release();second_device=nullptr;
        device->Release();device=nullptr;d3d->Release();d3d=nullptr;DestroyWindow(window);window=nullptr;
        std::puts("PASS: loading D3D8 image, native-overlay order, state restoration and resource teardown");
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());}
    cache.reset();
    if(second_device)second_device->Release();
    if(device)device->Release();
    if(d3d)d3d->Release();
    if(window)DestroyWindow(window);
    return 1;
}
