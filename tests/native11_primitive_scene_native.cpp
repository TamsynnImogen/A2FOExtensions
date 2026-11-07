// Private HWND-free GPU fixture for owned primitive packets. No engine entry.
#include "../renderer/native11/primitive_scene.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace a2fo::native11;
namespace {
void need(bool value,const char* label){if(!value){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
void ok(HRESULT hr,const char* label){if(FAILED(hr)){std::fprintf(stderr,"FAIL %s hr=%08lx\n",label,
    static_cast<unsigned long>(hr));std::exit(1);}}
struct Image {std::vector<std::uint8_t> bytes;std::uint32_t width=0,height=0;
    int channel(unsigned x,unsigned y,unsigned component)const{
        return bytes[(std::size_t(y)*width+x)*4+component];}
};
Image read(Renderer& renderer){Image image;ok(renderer.readback(image.bytes,image.width,image.height),"readback");
    need(image.width==32&&image.height==32&&image.bytes.size()==4096,"readback size");return image;}
ResourceUsage usage(Renderer& renderer){ResourceUsage value{};ok(renderer.resource_usage(value),"resource usage");return value;}
struct ForeignCall {PrimitiveScene* scene;const PrimitiveFrame* frame;HRESULT result=S_OK;std::string error;};
DWORD WINAPI foreign_render(void* context){
    auto& call=*static_cast<ForeignCall*>(context);
    call.result=call.scene->render(*call.frame,call.error);
    return 0;
}
PrimitiveFrame packet(){
    PrimitiveFrame value;value.generation=1;value.source_epoch=1;
    value.settings.ambient[0]=value.settings.ambient[1]=value.settings.ambient[2]=1;
    value.settings.clear_colour[0]=value.settings.clear_colour[1]=value.settings.clear_colour[2]=0;
    PrimitiveMesh mesh;mesh.id=11;
    mesh.vertices={{{-1,-1,.5f},{0,0,-1},{0,0}},{{3,-1,.5f},{0,0,-1},{0,0}},
                   {{-1,3,.5f},{0,0,-1},{0,0}}};
    mesh.indices={0,1,2};value.meshes.push_back(mesh);
    PrimitiveTexture texture;texture.id=21;texture.width=1;texture.height=1;
    texture.bgra={0,0,255,255};value.textures.push_back(texture);
    PrimitiveDraw draw;draw.mesh=11;draw.texture=21;value.draws.push_back(draw);
    return value;
}
}
int main(){
    Renderer renderer;ok(renderer.initialize_offscreen(32,32),"primitive offscreen renderer");
    PrimitiveScene scene(renderer);std::string error;
    auto source=packet();
    ok(scene.render(source,error),"first primitive packet");need(error.empty(),"first packet error clear");
    const auto first=read(renderer);need(first.channel(4,16,2)>245,"red primitive visible");
    const auto resident=usage(renderer);
    need(resident.meshes==1&&resident.textures==2,"first packet residency");

    source.generation=2;source.draws[0].surface.local_to_world[3]=.5f;
    ok(scene.render(source,error),"hot pose packet");
    const auto moved=read(renderer);
    need(moved.channel(4,16,2)<10&&moved.channel(16,16,2)>245,"hot pose changes pixels");
    const auto hot=usage(renderer);
    need(hot.epoch==resident.epoch&&hot.meshes==resident.meshes&&
         hot.textures==resident.textures&&hot.mesh_bytes==resident.mesh_bytes&&
         hot.texture_bytes==resident.texture_bytes,"hot pose preserves residency");

    source.textures[0].bgra={0,255,0,255};source.generation=3;
    need(scene.render(source,error)==E_INVALIDARG,"same epoch changed bytes refused");
    need(read(renderer).bytes==moved.bytes,"same epoch refusal preserves image");
    source.textures[0].bgra={0,0,255,255};
    {
        auto duplicate=source;
        duplicate.meshes.push_back(duplicate.meshes[0]);
        need(scene.render(duplicate,error)==E_INVALIDARG,"duplicate mesh ID refused");
        duplicate=source;
        duplicate.textures.push_back(duplicate.textures[0]);
        need(scene.render(duplicate,error)==E_INVALIDARG,"duplicate texture ID refused");
        auto oversized=source;
        oversized.draws.resize(8193,source.draws[0]);
        need(scene.render(oversized,error)==E_INVALIDARG,"oversized draw count refused");
        ForeignCall call{&scene,&source,S_OK,{}};
        HANDLE worker=CreateThread(nullptr,0,foreign_render,&call,0,nullptr);
        need(worker!=nullptr,"foreign render thread created");
        need(WaitForSingleObject(worker,INFINITE)==WAIT_OBJECT_0,"foreign render thread joined");
        CloseHandle(worker);
        need(call.result==E_UNEXPECTED && !call.error.empty(),"foreign-thread render refused");
        need(read(renderer).bytes==moved.bytes,"rejected packets preserve image");
    }
    source.draws[0].mesh=999;
    need(scene.render(source,error)==E_INVALIDARG,"missing mesh refused");
    need(read(renderer).bytes==moved.bytes&&usage(renderer).meshes==1,
         "missing reference retains image and cache");
    source.draws[0].mesh=11;
    source.settings.draw_count=1;
    need(scene.render(source,error)==E_INVALIDARG,"borrowed draw settings refused");
    source.settings.draw_count=0;
    source.textures[0].width=2;
    need(scene.render(source,error)==E_INVALIDARG,"bad BGRA extent refused");
    need(read(renderer).bytes==moved.bytes,"bad BGRA retains target");
    source.textures[0].width=1;
    source.textures[0].bgra={255,0,0,255};source.source_epoch=2;
    source.draws[0].surface.local_to_world[15]=2;
    const auto before_failed_candidate=usage(renderer);
    need(scene.render(source,error)==E_INVALIDARG,"candidate renderer validation failure");
    const auto after_failed_candidate=usage(renderer);
    need(after_failed_candidate.epoch==before_failed_candidate.epoch&&
         after_failed_candidate.meshes==before_failed_candidate.meshes&&
         after_failed_candidate.textures==before_failed_candidate.textures&&
         after_failed_candidate.mesh_bytes==before_failed_candidate.mesh_bytes&&
         after_failed_candidate.texture_bytes==before_failed_candidate.texture_bytes,
         "failed candidate uploads rolled back");
    need(read(renderer).bytes==moved.bytes,"failed candidate preserves old image");
    source.draws[0].surface.local_to_world[15]=1;
    ok(scene.render(source,error),"new source epoch replacement");
    const auto blue=read(renderer);
    need(blue.channel(16,16,0)>245&&blue.channel(16,16,2)<10,
         "new source bytes replace pixels");
    need(usage(renderer).meshes==1&&usage(renderer).textures==2,
         "replacement releases prior residency");

    source.textures[0].bgra={0,255,0,255};
    need(read(renderer).bytes==blue.bytes,"GPU texture owns uploaded source bytes");
    source.generation=4;
    need(scene.render(source,error)==E_INVALIDARG,"poisoned source same epoch refused");
    source.textures[0].bgra={255,0,0,255};
    renderer.shutdown();
    ok(renderer.initialize_offscreen(32,32),"device recreate");
    source.generation=5;
    ok(scene.render(source,error),"identical manifest recovers on new device epoch");
    need(read(renderer).channel(16,16,0)>245,"recreated packet visible");
    need(usage(renderer).meshes==1&&usage(renderer).textures==2,"recreated residency");

    scene.clear();
    need(usage(renderer).meshes==0&&usage(renderer).textures==1,"clear releases current device handles");
    source.generation=6;
    need(scene.render(source,error)==E_INVALIDARG,"clear retains source watermark");
    source.source_epoch=3;
    ok(scene.render(source,error),"fresh epoch after clear");
    need(read(renderer).channel(16,16,0)>245,"fresh packet after clear visible");
    scene.clear();renderer.shutdown();
    std::puts("PASS native11 primitive scene ownership, residency, rollback, recovery");
}
