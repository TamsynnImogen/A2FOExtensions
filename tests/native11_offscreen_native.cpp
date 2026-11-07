// Compile the backend once here to reach the private device for a scoped
// failure injection. No diagnostic device API is added to production.
#include "../renderer/native11/renderer.cpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace a2fo::native11;
extern "C" unsigned native11_sdk_device_slots();
extern "C" unsigned native11_sdk_depth_view_slot();
extern "C" unsigned native11_sdk_context_map_slot();
namespace a2fo::native11 {
struct RendererFixtureAccess {static ID3D11Device* device(Renderer& r){return r.p_->device;}
    static ID3D11DeviceContext* context(Renderer& r){return r.p_->context;}
    static bool has_staging(Renderer& r){return r.p_->readback_staging!=nullptr;}
    static std::uint64_t staging_creations(Renderer& r){return r.p_->readback_staging_creations;}};
}
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void ok(HRESULT hr,const char* why){if(FAILED(hr)){std::fprintf(stderr,"FAIL %s 0x%08lx\n",why,static_cast<unsigned long>(hr));std::exit(1);}}
void colour(const std::vector<std::uint8_t>& bytes,unsigned width,unsigned x,unsigned y,
            int blue,int green,int red,const char* why){
    const auto* p=bytes.data()+(static_cast<std::size_t>(y)*width+x)*4;
    if(std::abs(int(p[0])-blue)>3||std::abs(int(p[1])-green)>3||std::abs(int(p[2])-red)>3){
        std::fprintf(stderr,"FAIL %s actual BGRA=%u,%u,%u,%u\n",why,p[0],p[1],p[2],p[3]);std::exit(1);
    }
}
void capture(Renderer& renderer,std::vector<std::uint8_t>& bytes,unsigned expected_w,unsigned expected_h){
    std::uint32_t width=0,height=0;ok(renderer.readback(bytes,width,height),"readback");
    need(width==expected_w&&height==expected_h&&
         bytes.size()==static_cast<std::size_t>(width)*height*4,"readback dimensions");
}
unsigned depth_view_failures=0;
HRESULT STDMETHODCALLTYPE fail_depth_view(ID3D11Device*,ID3D11Resource*,
    const D3D11_DEPTH_STENCIL_VIEW_DESC*,ID3D11DepthStencilView** out){
    ++depth_view_failures;if(out)*out=nullptr;return E_OUTOFMEMORY;
}
struct FailDepthView {
    void** slot;void* original;
    static void write(void** target,void* value){
        DWORD previous=0;need(VirtualProtect(target,sizeof(void*),PAGE_READWRITE,&previous)!=0,"device slot writable");
        InterlockedExchangePointer(reinterpret_cast<void* volatile*>(target),value);
        DWORD ignored=0;need(VirtualProtect(target,sizeof(void*),previous,&ignored)!=0,"device slot protection restored");
    }
    explicit FailDepthView(Renderer& renderer){
        need(native11_sdk_device_slots()==43&&native11_sdk_depth_view_slot()==10,"SDK-derived device layout");
        slot=*reinterpret_cast<void***>(RendererFixtureAccess::device(renderer))+native11_sdk_depth_view_slot();
        original=*slot;
        const auto failure=&fail_depth_view;void* replacement=nullptr;
        static_assert(sizeof(failure)==sizeof(replacement));std::memcpy(&replacement,&failure,sizeof(failure));
        write(slot,replacement);
    }
    ~FailDepthView(){write(slot,original);}
};
using MapFunction=HRESULT (STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,D3D11_MAP,UINT,D3D11_MAPPED_SUBRESOURCE*);
MapFunction original_map=nullptr;bool bad_pitch=false;
HRESULT STDMETHODCALLTYPE injected_map(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource,
    D3D11_MAP type,UINT flags,D3D11_MAPPED_SUBRESOURCE* out){
    if(!bad_pitch){if(out)*out={};return E_OUTOFMEMORY;}
    const auto hr=original_map(context,resource,subresource,type,flags,out);
    if(SUCCEEDED(hr)&&out)out->RowPitch=0;
    return hr;
}
struct MapFault {
    void** slot;void* original;
    MapFault(Renderer& renderer,bool pitch){
        bad_pitch=pitch;slot=*reinterpret_cast<void***>(RendererFixtureAccess::context(renderer))+native11_sdk_context_map_slot();
        original=*slot;static_assert(sizeof(original_map)==sizeof(original));std::memcpy(&original_map,&original,sizeof(original));
        const auto target=&injected_map;void* replacement=nullptr;static_assert(sizeof(target)==sizeof(replacement));
        std::memcpy(&replacement,&target,sizeof(target));FailDepthView::write(slot,replacement);
    }
    ~MapFault(){FailDepthView::write(slot,original);original_map=nullptr;}
};
}
int main(){
    Renderer renderer;
    need(renderer.initialize_offscreen(0,32)==E_INVALIDARG,"zero width rejected");
    need(renderer.initialize_offscreen(32,0)==E_INVALIDARG,"zero height rejected");
    need(renderer.initialize_offscreen(16384,16384)==E_INVALIDARG,"budget rejected");
    ok(renderer.initialize_offscreen(32,32),"offscreen initialization");
    ResourceUsage usage{};ok(renderer.resource_usage(usage),"initial usage");
    need(usage.epoch==1&&usage.textures==1&&usage.meshes==0,"first epoch and implicit image");

    Vertex triangle[3]={{{-1,-1,.2f},{0,0,1},{0,0}},{{3,-1,.2f},{0,0,1},{0,0}},{{-1,3,.2f},{0,0,1},{0,0}}};
    const std::uint32_t indices[3]={0,1,2};std::uint32_t mesh=0;
    ok(renderer.upload_mesh({triangle,3,indices,3},mesh),"triangle upload");
    const std::uint8_t red_bgra[4]={0,0,255,255};std::uint32_t image=0;
    ok(renderer.upload_texture({red_bgra,1,1,4,4},image),"red texture upload");
    Draw draw{};draw.mesh=mesh;draw.texture=image;
    FrameView frame{};frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;
    frame.draws=&draw;frame.draw_count=1;
    ok(renderer.render(frame,false),"offscreen triangle render");
    std::vector<std::uint8_t> bytes;capture(renderer,bytes,32,32);
    colour(bytes,32,16,16,0,0,255,"red triangle centre");
    const auto first_staging=RendererFixtureAccess::staging_creations(renderer);
    need(first_staging==1,"first readback staging allocation");
    capture(renderer,bytes,32,32);
    need(RendererFixtureAccess::staging_creations(renderer)==first_staging,"repeated readback reuses staging");
    for(bool malformed:{false,true}){
        std::vector<std::uint8_t> unchanged{17,23};std::uint32_t fw=7,fh=9;
        {MapFault fault(renderer,malformed);
            need(renderer.readback(unchanged,fw,fh)==(malformed?E_FAIL:E_OUTOFMEMORY)&&
                 unchanged==std::vector<std::uint8_t>({17,23})&&fw==7&&fh==9&&
                 !RendererFixtureAccess::has_staging(renderer),"failed mapping drops staging and preserves output");}
        capture(renderer,bytes,32,32);colour(bytes,32,16,16,0,0,255,"staging map failure recovers");
    }
    const auto steady_staging=RendererFixtureAccess::staging_creations(renderer);
    need(steady_staging==first_staging+2,"two failed staging maps recreated once each");
    const auto prior=bytes;
    frame.draw_count=0;frame.clear_colour[0]=0;frame.clear_colour[1]=1;frame.clear_colour[2]=0;
    need(renderer.render(frame,true)==E_INVALIDARG,"present rejected");
    capture(renderer,bytes,32,32);need(bytes==prior,"present rejection preserves prior frame");
    ok(renderer.render(frame,false),"render after present rejection");
    capture(renderer,bytes,32,32);colour(bytes,32,16,16,0,255,0,"new green clear");
    frame.draw_count=1;

    ok(renderer.resize(48,24),"offscreen resize");
    ok(renderer.resource_usage(usage),"usage after resize");
    need(usage.epoch==1&&usage.meshes==1&&usage.textures==2,"resize retains resources and epoch");
    ok(renderer.render(frame,false),"render after resize");
    capture(renderer,bytes,48,24);colour(bytes,48,24,12,0,0,255,"resized red triangle");
    need(RendererFixtureAccess::staging_creations(renderer)==steady_staging+1,"resize recreates staging once");
    need(renderer.resize(16384,16384)==E_INVALIDARG,"invalid resize rejected");
    capture(renderer,bytes,48,24);colour(bytes,48,24,12,0,0,255,"invalid resize preserves target");
    {
        FailDepthView fail(renderer);
        need(renderer.resize(64,40)==E_OUTOFMEMORY&&depth_view_failures==1,
             "actual offscreen depth target creation failure");
        need(renderer.render(frame,false)==E_UNEXPECTED,"missing targets reject render");
        std::vector<std::uint8_t> refused;std::uint32_t refused_w=7,refused_h=9;
        need(renderer.readback(refused,refused_w,refused_h)==E_UNEXPECTED&&
             refused.empty()&&refused_w==7&&refused_h==9,"missing targets reject readback");
        ok(renderer.resource_usage(usage),"residency available after failed target creation");
        need(usage.epoch==1&&usage.meshes==1&&usage.textures==2,"failure retains device resources");
        ok(renderer.release_mesh(mesh),"release mesh without targets");
        ok(renderer.release_texture(image),"release texture without targets");
        ok(renderer.resource_usage(usage),"residency after release without targets");
        need(usage.epoch==1&&usage.meshes==0&&usage.textures==1&&
             usage.mesh_bytes==0&&usage.texture_bytes==4,"failed targets do not strand residency");
    }
    ok(renderer.resize(64,40),"retry failed offscreen target size");
    ok(renderer.resource_usage(usage),"usage after target recovery");
    need(usage.epoch==1,"target recovery preserves epoch");
    ok(renderer.upload_mesh({triangle,3,indices,3},mesh),"mesh upload after target recovery");
    ok(renderer.upload_texture({red_bgra,1,1,4,4},image),"texture upload after target recovery");
    draw.mesh=mesh;draw.texture=image;
    ok(renderer.render(frame,false),"render after target recovery");
    capture(renderer,bytes,64,40);colour(bytes,64,32,20,0,0,255,"recovered red triangle");
    need(RendererFixtureAccess::staging_creations(renderer)==steady_staging+2,"failed target retry recreates staging once");
    renderer.shutdown();
    ok(renderer.initialize_offscreen(16,16),"offscreen reinitialize");
    ok(renderer.resource_usage(usage),"new epoch usage");
    need(usage.epoch==2&&usage.meshes==0&&usage.textures==1,"reinit epoch and resource reset");
    std::uint32_t new_mesh=0;ok(renderer.upload_mesh({triangle,3,indices,3},new_mesh),"new mesh upload");
    need(new_mesh>mesh,"mesh IDs remain monotonic");
    need(renderer.render(frame,false)==E_INVALIDARG,"stale mesh rejected");
    draw.mesh=new_mesh;
    need(renderer.render(frame,false)==E_INVALIDARG,"stale image rejected");
    draw.texture=0;ok(renderer.render(frame,false),"new handles render");
    capture(renderer,bytes,16,16);colour(bytes,16,8,8,255,255,255,"new epoch geometry");
    need(RendererFixtureAccess::staging_creations(renderer)==steady_staging+3,"device recreation drops old staging");
    renderer.shutdown();
    std::puts("PASS native11 offscreen device colour readback present rejection resize resources epoch stale IDs");
    return 0;
}
