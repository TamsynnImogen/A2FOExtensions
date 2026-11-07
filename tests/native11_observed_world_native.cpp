// Controlled complete-domain fixture: owned observations -> API5 GPU world frame.
#include "../renderer/native11/observed_world.hpp"
#include "../renderer/native11/world_session.hpp"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool value,const char* label){if(!value){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
std::vector<std::uint8_t> tga(std::uint8_t blue,std::uint8_t green,std::uint8_t red){
    std::vector<std::uint8_t> bytes(22);bytes[2]=2;bytes[12]=1;bytes[14]=1;
    bytes[16]=24;bytes[17]=0x20;bytes[18]=blue;bytes[19]=green;bytes[20]=red;return bytes;
}
struct Assets {
    std::shared_ptr<const ModelAsset> model;
    std::shared_ptr<const TextureAsset> white,blue;
    Assets(){auto m=std::make_shared<ModelAsset>();m->info.database=7;m->info.serial=11;
        std::strcpy(m->info.name,"world.sod");m->bytes=native11_fixture::model("white.tga");
        m->info.byte_count=static_cast<std::uint32_t>(m->bytes.size());model=m;
        auto w=std::make_shared<TextureAsset>();w->serial=31;w->texture=41;
        w->logical_name="white.tga";w->bytes=std::make_shared<const std::vector<std::uint8_t>>(tga(255,255,255));white=w;
        auto b=std::make_shared<TextureAsset>();b->serial=32;b->texture=42;
        b->logical_name="blue.tga";b->bytes=std::make_shared<const std::vector<std::uint8_t>>(tga(255,0,0));blue=b;}
    static std::shared_ptr<const ModelAsset> resolve(void* raw,std::uint32_t db,std::uint64_t serial){
        auto model=static_cast<Assets*>(raw)->model;
        return model&&model->info.database==db&&model->info.serial==serial?model:nullptr;}
};
std::shared_ptr<const BoundMaterialCapture> capture(Assets& assets,std::uint64_t generation,
    bool blocker,bool hidden=false){
    auto frame=std::make_unique<Frame>();frame->camera=1;frame->generation=generation;
    frame->object_count=1;for(int i=0;i<4;++i){frame->view.world_to_clip[i*5]=1;
        frame->objects[0].world[i*5]=1;}
    frame->view.viewport[2]=frame->view.viewport[3]=32;
    auto& object=frame->objects[0];object.entity=blocker?202:201;
    object.instance=blocker?102:101;object.database=7;object.model_serial=11;
    object.model_identity=ModelIdentity::plain_database_name;
    object.placement_identity=PlacementIdentity::authored_placement;
    object.shadow_only=false; // assembler assigns candidate-only role from coverage.
    if(blocker){for(int i:{0,5,10})object.world[i]=.2f;object.world[11]=.26f;}
    else object.world[11]=.6f;
    std::strcpy(object.model_name,"world.sod");
    auto observation=std::make_shared<MaterialObservation>();observation->instance=object.instance;
    observation->database=7;observation->model_serial=11;observation->model=assets.model;
    MaterialBinding part{};part.authored_texture="white.tga";
    part.texture=blocker?assets.blue:assets.white;part.node_state_valid=true;
    part.node_state.visible=!hidden;observation->parts.push_back(part);
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observations{{object.instance,observation}};
    std::shared_ptr<const BoundMaterialCapture> output;std::string error;
    need(build_bound_material_capture(*frame,20,Assets::resolve,&assets,observations,output,error),
        "sealed observed material fixture");return output;
}
WorldLightCapture world(){WorldLightCapture value;
    auto owner=std::make_shared<LightClassRecord>();owner->class_id=1001;owner->version=7;
    for(const WorldMember member:{WorldMember{201,101,1001,7},WorldMember{202,102,1001,7}}){
        value.members.push_back(member);value.class_owners.push_back(owner);
        PlacedLight source{};source.source_id=member.entity;value.sources.push_back(source);}
    return value;
}
LightingFrame lighting(){LightingFrame result;PlacedLight red{};red.source_id=1;
    red.config.enabled=true;red.config.cast_shadows=true;
    red.config.light.type=LightType::directional;red.config.light.direction[0]=.5f;
    red.config.light.direction[1]=0;red.config.light.direction[2]=1;
    red.config.light.colour[0]=1;red.config.light.colour[1]=red.config.light.colour[2]=0;
    red.config.light.intensity=.6f;
    const float ambient[3]{.1f,.1f,.1f};std::string error;
    need(assemble_world_lighting({red},true,ambient,result,error)&&result.shadows.light_mask==1,
        "explicit red world lighting");return result;
}
std::shared_ptr<const OwnedWorldFrame> packet(const std::shared_ptr<const BoundMaterialCapture>& capture,
    const LightingFrame& lights){auto value=std::make_shared<OwnedWorldFrame>();
    value->materials=capture;value->lighting=lights;value->light_camera=capture->frame().camera;
    value->light_generation=capture->frame().generation;value->complete_casters=true;return value;}
int red(const std::vector<std::uint8_t>& bytes,unsigned x,unsigned y){return bytes[(y*32+x)*4+2];}
bool same_usage(const A2FO_N11_Usage& a,const A2FO_N11_Usage& b){return
    a.device_epoch==b.device_epoch&&a.mesh_bytes==b.mesh_bytes&&
    a.texture_bytes==b.texture_bytes&&a.meshes==b.meshes&&a.textures==b.textures;}
void run(const char* path){
    HMODULE dll=LoadLibraryA(path);need(dll,"renderer DLL load");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol,"API export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API5 api{};need(get(5,sizeof(api),&api)>=0,"API5 query");
    Assets assets;auto membership=world();ObservedWorldAssembler assembler;std::string error;
    auto visible=capture(assets,1,false);
    const DomainEvidence absent{true,true,true}; // Certified absent in this controlled fixture only.
    need(!assembler.update(visible,membership,{},absent,1,error)&&
        !assembler.current()&&assembler.report().missing.size()==1,
        "missing offscreen GameObject blocks owner publication");
    auto candidate=capture(assets,1,true);
    need(!assembler.update(visible,membership,{candidate},{},1,error)&&
        assembler.report().game_objects_complete&&!assembler.current(),
        "checked membership does not certify other domains");
    need(assembler.update(visible,membership,{candidate},absent,1,error),
        "controlled absent domains admit owned world");
    auto first=assembler.current();need(first&&first->frame().object_count==2&&
        first->frame().complete_casters&&first->frame().requires_part_states&&
        !first->frame().objects[0].shadow_only&&first->frame().objects[1].shadow_only,
        "coverage assigns visible receiver and shadow-only candidate");
    WorldRenderSession session;need(session.initialize(api,32,32,error),"owned API5 session");
    auto lights=lighting();auto first_packet=packet(first,lights);
    need(session.render(first_packet,error),"complete observed world GPU frame");
    std::vector<std::uint8_t> image;std::uint32_t width=0,height=0;
    need(session.readback(image,width,height,error)&&width==32&&height==32,
        "GPU readback");
    need(red(image,16,16)>100&&red(image,20,16)>15&&red(image,20,16)<55&&
        red(image,11,16)>red(image,20,16)+60,
        "receiver lit with offscreen candidate shadow footprint");
    A2FO_N11_Usage resident{};need(session.usage(resident,error),"initial residency");
    auto second_visible=capture(assets,2,false);
    auto second_candidate=capture(assets,2,true,true);
    need(assembler.update(second_visible,membership,{second_candidate},absent,2,error)&&
        assembler.current()->source_epoch()==first->source_epoch(),
        "hidden part changes pose without new resource epoch");
    auto second=assembler.current();auto second_packet=packet(second,lights);
    need(session.render(second_packet,error),"hidden blocker world frame");
    std::vector<std::uint8_t> unshadowed;
    need(session.readback(unshadowed,width,height,error)&&
        red(unshadowed,20,16)>red(image,20,16)+60,
        "hidden blocker no longer casts shadow");
    A2FO_N11_Usage hot{};need(session.usage(hot,error)&&same_usage(resident,hot),
        "same resource epoch retains GPU residency");
    auto third_visible=capture(assets,3,false);
    need(!assembler.update(third_visible,membership,{},absent,3,error)&&
        assembler.current()==second&&assembler.report().missing.size()==1&&
        session.current()==second_packet,"new missing member keeps admitted owner");
    std::vector<std::uint8_t> unchanged;
    need(session.readback(unchanged,width,height,error)&&unchanged==unshadowed,
        "blocked coverage causes no GPU submission");
    std::weak_ptr<const BoundMaterialCapture> second_weak=second;
    need(session.close(error),"owner-thread close");
    need(assembler.clear(),"assembler clear");
    need(first->frame().objects[1].shadow_only&&first->frame().object_count==2,
        "held immutable earlier frame survives clear");
    first_packet.reset();second_packet.reset();first.reset();second.reset();
    visible.reset();candidate.reset();second_visible.reset();second_candidate.reset();third_visible.reset();
    need(second_weak.expired(),"all latest frame owners released after close and clear");
    need(FreeLibrary(dll)!=0,"renderer DLL unload after close");
}
}
int main(int argc,char** argv){need(argc==2,"usage: native11_observed_world_native.exe RENDERER_DLL");
    run(argv[1]);std::puts("PASS native11 observed world API5 GPU fixture");}
