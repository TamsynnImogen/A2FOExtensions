// Native GPU geometry/light occlusion; no game, HWND or legacy device.
#include "../renderer/native11/renderer.cpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace a2fo::native11;
extern "C" unsigned native11_sdk_device_slots();
extern "C" unsigned native11_sdk_depth_view_slot();
namespace a2fo::native11 {
struct RendererFixtureAccess {static ID3D11Device* device(Renderer& r){return r.p_->device;}};
}
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void ok(HRESULT hr,const char* why){if(FAILED(hr)){std::fprintf(stderr,"FAIL %s hr=%08lx\n",why,static_cast<unsigned long>(hr));std::exit(1);}}
void log(const char* message){std::fprintf(stderr,"shader: %s\n",message);}
std::uint32_t quad(Renderer& r,float x,float y,float z,float nz){
    const Vertex vertices[4]={{{-x,-y,z},{0,0,nz},{0,0}},{{x,-y,z},{0,0,nz},{1,0}},
        {{x,y,z},{0,0,nz},{1,1}},{{-x,y,z},{0,0,nz},{0,1}}};
    const std::uint32_t indices[6]{0,1,2,0,2,3};std::uint32_t handle=0;
    ok(r.upload_mesh({vertices,4,indices,6},handle),"quad upload");return handle;
}
std::vector<std::uint8_t> image(Renderer& r,unsigned expected_w=32,unsigned expected_h=32){
    std::vector<std::uint8_t> b;std::uint32_t w=0,h=0;ok(r.readback(b,w,h),"shadow readback");
    need(w==expected_w&&h==expected_h,"shadow dimensions");return b;
}
int channel(const std::vector<std::uint8_t>& b,unsigned x,unsigned y,unsigned ch=2,unsigned w=32){return b[(y*w+x)*4+ch];}
void dark(const std::vector<std::uint8_t>& b,unsigned x,unsigned y,const char* why){
    const int p=channel(b,x,y);if(p<20||p>35){std::fprintf(stderr,"FAIL %s pixel=%d expected ambient only\n",why,p);std::exit(1);}
}
void lit(const std::vector<std::uint8_t>& b,unsigned x,unsigned y,const char* why){
    const int p=channel(b,x,y);if(p<100){std::fprintf(stderr,"FAIL %s pixel=%d expected direct light\n",why,p);std::exit(1);}
}
using CreateDepthView=HRESULT (STDMETHODCALLTYPE*)(ID3D11Device*,ID3D11Resource*,const D3D11_DEPTH_STENCIL_VIEW_DESC*,ID3D11DepthStencilView**);
CreateDepthView original_depth_view=nullptr;
unsigned allowed_depth_views=0,depth_view_calls=0;
HRESULT STDMETHODCALLTYPE fail_depth_view(ID3D11Device* device,ID3D11Resource* resource,const D3D11_DEPTH_STENCIL_VIEW_DESC* desc,ID3D11DepthStencilView** output){
    ++depth_view_calls;
    if(allowed_depth_views){--allowed_depth_views;return original_depth_view(device,resource,desc,output);}
    if(output)*output=nullptr;
    return E_OUTOFMEMORY;
}
struct FailDepthView {
    void** slot;void* original;
    static void write(void** at,void* value){DWORD protection=0;need(VirtualProtect(at,4,PAGE_READWRITE,&protection)!=0,"private slot writable");
        InterlockedExchangePointer(reinterpret_cast<void* volatile*>(at),value);DWORD ignored=0;
        need(VirtualProtect(at,4,protection,&ignored)!=0,"private slot protection restored");}
    explicit FailDepthView(Renderer& r,unsigned allow=0){need(native11_sdk_device_slots()==43&&native11_sdk_depth_view_slot()==10,"SDK device layout");
        slot=*reinterpret_cast<void***>(RendererFixtureAccess::device(r))+native11_sdk_depth_view_slot();original=*slot;
        std::memcpy(&original_depth_view,&original,sizeof(original_depth_view));allowed_depth_views=allow;depth_view_calls=0;
        const auto function=&fail_depth_view;void* value=nullptr;std::memcpy(&value,&function,sizeof(value));write(slot,value);}
    ~FailDepthView(){write(slot,original);original_depth_view=nullptr;}
};
void run(){
    Renderer r;ok(r.initialize_offscreen(32,32,log),"native shadow device");
    {auto* device=RendererFixtureAccess::device(r);UINT raw=0,depth=0;
     const auto a=device->CheckFormatSupport(DXGI_FORMAT_R32_FLOAT,&raw),b=device->CheckFormatSupport(DXGI_FORMAT_D32_FLOAT,&depth);
     std::printf("Depth format support: R32_FLOAT hr=%08lx mask=%08x D32_FLOAT hr=%08lx mask=%08x\n",static_cast<unsigned long>(a),raw,static_cast<unsigned long>(b),depth);}
    const auto receiver=quad(r,.95f,.95f,.8f,-1),blocker=quad(r,.15f,.25f,.3f,-1);
    Draw draws[2]{};draws[0].mesh=receiver;draws[1].mesh=blocker;
    Light lights[2]{};lights[0].direction[0]=1;lights[0].direction[1]=0;lights[0].direction[2]=1;
    FrameView f{};f.draws=draws;f.draw_count=2;f.lights=lights;f.light_count=1;
    f.ambient[0]=f.ambient[1]=f.ambient[2]=.1f;
    f.shadows.resolution=256;
    ok(r.render(f,false),"unshadowed reference");lit(image(r),24,16,"reference receiver illuminated");
    f.shadows.light_mask=1;ok(r.render(f,false),"directional shadow");auto b=image(r);
    dark(b,24,16,"directional blocker casts along light ray");lit(b,8,16,"directional other side stays illuminated");
    lights[0].direction[0]=-1;ok(r.render(f,false),"changed directional source");b=image(r);
    dark(b,8,16,"source movement moves directional shadow");lit(b,24,16,"old directional footprint clears");
    lights[0].direction[0]=1;draws[1].local_to_world[3]=-.4f;ok(r.render(f,false),"moved caster");b=image(r);
    dark(b,18,16,"caster transform moves shadow");lit(b,24,16,"old caster footprint clears");draws[1].local_to_world[3]=0;

    lights[0].type=LightType::spot;lights[0].position[0]=-.5f;lights[0].position[2]=0;
    lights[0].falloff_start=5;lights[0].falloff_range=5;lights[0].cone_inner_degrees=70;lights[0].cone_outer_degrees=100;
    ok(r.render(f,false),"cone shadow");b=image(r);dark(b,24,16,"spotlight has physical blocker");lit(b,12,16,"unblocked spotlight receiver");
    lights[0].type=LightType::point;ok(r.render(f,false),"point shadow");b=image(r);
    dark(b,24,16,"point +X face shadow");lit(b,8,16,"point unblocked receiver");
    lights[0].position[0]=.5f;ok(r.render(f,false),"point source movement");b=image(r);
    dark(b,8,16,"point -X face shadow");lit(b,24,16,"point old footprint clears");
    lights[0].position[0]=0;ok(r.render(f,false),"point forward face");b=image(r);
    dark(b,20,16,"point +Z face casts beyond visible blocker");
    const auto forward_blocker=draws[1].mesh;draws[1].mesh=quad(r,.25f,.15f,.3f,-1);
    lights[0].position[1]=-.5f;ok(r.render(f,false),"point +Y face");b=image(r);dark(b,16,8,"point +Y face shadow");
    lights[0].position[1]=.5f;ok(r.render(f,false),"point -Y face");b=image(r);dark(b,16,24,"point -Y face shadow");
    lights[0].position[1]=0;lights[0].position[2]=1.5f;draws[0].mesh=quad(r,.95f,.95f,.8f,1);
    draws[1].mesh=forward_blocker;draws[1].local_to_world[11]=.9f;
    ok(r.render(f,false),"point rear face");b=image(r);dark(b,16,16,"point -Z face casts from object behind camera-visible receiver");
    draws[0].emissive[0]=.2f;ok(r.render(f,false),"emissive shadow receiver");b=image(r);
    need(channel(b,16,16)>70&&channel(b,16,16)<85,"shadow preserves emissive contribution");draws[0].emissive[0]=0;

    draws[0].mesh=receiver;draws[1].local_to_world[11]=0;
    lights[0]=Light{};lights[0].direction[0]=1;lights[0].direction[1]=0;lights[0].direction[2]=1;
    lights[1]=lights[0];lights[0].colour[1]=lights[0].colour[2]=0;lights[1].colour[0]=lights[1].colour[2]=0;f.light_count=2;
    ok(r.render(f,false),"per-source red shadow");b=image(r);
    need(channel(b,24,16)<35&&channel(b,24,16,1)>100,"shadow occludes only selected red light");
    f.shadows.light_mask=2;ok(r.render(f,false),"per-source green shadow");b=image(r);
    need(channel(b,24,16)>100&&channel(b,24,16,1)<35,"shadow mask selects green light independently");
    f.light_count=1;lights[0].colour[1]=lights[0].colour[2]=1;f.shadows.light_mask=1;
    const std::uint8_t transparent[4]{255,255,255,0};std::uint32_t texture=0;
    ok(r.upload_texture({transparent,1,1,4,4},texture),"cutout texture");draws[1].texture=texture;draws[1].flags=two_sided|cutout;
    ok(r.render(f,false),"cutout shadow geometry");lit(image(r),24,16,"transparent cutout cannot cast a solid shadow");
    draws[1].texture=0;draws[1].flags=two_sided;
    ok(r.render(f,false),"restore opaque caster");const auto prior=image(r);dark(prior,24,16,"restored opaque caster shadow");
    {FailDepthView fail(r);f.shadows.resolution=512;need(r.render(f,false)==E_OUTOFMEMORY,"depth array view creation failure");
        need(image(r)==prior,"shadow allocation failure preserves prior colour target");}
    f.shadows.resolution=256;ok(r.render(f,false),"prior atlas remains usable after allocation failure");dark(image(r),24,16,"prior atlas recovery");
    f.shadows.resolution=512;ok(r.render(f,false),"shadow atlas allocation retry");dark(image(r),24,16,"new atlas recovery");
    const auto prior_point_change=image(r);const auto directional=lights[0];
    lights[0].type=LightType::point;lights[0].position[0]=-.5f;
    {FailDepthView fail(r,2);need(r.render(f,false)==E_OUTOFMEMORY&&depth_view_calls==3,"point target fails after two actual depth slices");
        need(image(r)==prior_point_change,"partial point allocation preserves previous frame");}
    ok(r.render(f,false),"partial point atlas allocation retry");dark(image(r),24,16,"point target recovery after partial allocation");
    lights[0]=directional;ok(r.render(f,false),"restore directional source after partial point allocation");
    f.shadows.light_mask=8;need(r.render(f,false)==E_INVALIDARG,"unavailable light mask refused");f.shadows.light_mask=1;
    ok(r.resize(48,32),"shadow receiver resize");ok(r.render(f,false),"shadow after resize");b=image(r,48,32);
    need(channel(b,36,16,2,48)<35,"shadow projection survives receiver target resize");
    f.shadows.light_mask=0;ok(r.render(f,false),"disable physical shadow maps");b=image(r,48,32);
    need(channel(b,36,16,2,48)>100,"disabling shadow maps restores direct lighting");
    need(!GetModuleHandleA("d3d8.dll")&&!GetModuleHandleA("d3d9.dll")&&!GetModuleHandleA("D3DX81ab.dll"),"legacy graphics absent");
    std::puts("PASS native11 per-source directional/spot/point-six-face shadows; light/caster motion, ambient/emission, cutout, mask isolation, allocation rollback, resize and disable");
}
}
int main(){run();}
