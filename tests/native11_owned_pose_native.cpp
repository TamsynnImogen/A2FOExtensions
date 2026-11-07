// Private Rust pose metadata -> owned per-instance transforms -> API5 world pixels.
// Synthetic scene completeness is asserted only for these two known objects.
#include "../renderer/native11/model_pose_snapshot.hpp"
#include "../renderer/native11/owned_animation_pose.hpp"
#include "../renderer/native11/owned_part_states.hpp"
#include "../renderer/native11/model_node_template.hpp"
#include "../renderer/native11/world_session.hpp"
#include "../rust/native11_model_ffi.h"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
extern "C" int native11_pose_c_layout(const A2FO_N11_PoseMetadata*);
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
unsigned parser_calls=0;
int inspect(const std::uint8_t* data,std::uint32_t size,A2FO_N11_PoseSink sink,void* context){
    ++parser_calls;return a2fo_rs_native11_inspect_sod_pose(data,size,sink,context);
}
int layout_sink(void*,const A2FO_N11_PoseMetadata* metadata){
    return native11_pose_c_layout(metadata);
}
void half(std::vector<std::uint8_t>& bytes,std::uint16_t value){
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value>>8));
}
void real(std::vector<std::uint8_t>& bytes,float value){
    std::uint32_t bits=0;std::memcpy(&bits,&value,4);
    for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(bits>>(8*i)));
}
void name(std::vector<std::uint8_t>& bytes,const char* text){
    half(bytes,static_cast<std::uint16_t>(std::strlen(text)));
    while(*text)bytes.push_back(static_cast<std::uint8_t>(*text++));
}
void matrix(std::vector<std::uint8_t>& bytes,float x){
    for(float value:{1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,x,0.f,0.f})real(bytes,value);
}
std::vector<std::uint8_t> animated_sod(){
    auto bytes=native11_fixture::model("",true);
    need(bytes.size()>4,"base SOD fixture has terminal tables");
    bytes.resize(bytes.size()-4); // leave mesh trailing count; replace channel/ref counts
    half(bytes,2);
    name(bytes,"mesh");half(bytes,2);real(bytes,1);bytes.push_back(0);bytes.push_back(0);
    matrix(bytes,0);matrix(bytes,.25f);
    name(bytes,"mesh");half(bytes,2);real(bytes,1);bytes.push_back(5);bytes.push_back(0);
    real(bytes,1);real(bytes,.5f);
    half(bytes,0);return bytes;
}
std::shared_ptr<const ModelAsset> asset(std::vector<std::uint8_t> bytes,
    std::uint32_t database,std::uint64_t serial,const char* name_value){
    auto value=std::make_shared<ModelAsset>();value->info.database=database;
    value->info.serial=serial;std::strncpy(value->info.name,name_value,sizeof(value->info.name)-1);
    value->info.byte_count=static_cast<std::uint32_t>(bytes.size());value->bytes=std::move(bytes);
    return value;
}
InstanceAnimationTime clock(std::uint32_t instance,float elapsed){
    InstanceAnimationTime value;value.engine=1;value.instance=instance;value.database=7;
    value.flags=1;value.animating=true;value.engine_time=1;value.start_time=1-elapsed;
    value.elapsed=elapsed;return value;
}
std::vector<PoseChannelPlayback> playback(){
    std::vector<PoseChannelPlayback> result(2);
    result[0].interpolation=1;result[1].interpolation=1;return result;
}
struct Source {std::shared_ptr<const ModelAsset> model;};
std::shared_ptr<const ModelAsset> resolve(void* opaque,std::uint32_t db,std::uint64_t serial){
    auto model=static_cast<Source*>(opaque)->model;
    return model&&model->info.database==db&&model->info.serial==serial?model:nullptr;
}
Object object(std::uint32_t instance,float x){
    Object value;value.instance=instance;value.entity=instance+100;
    value.database=7;value.model_serial=11;
    value.model_identity=ModelIdentity::plain_database_name;
    value.placement_identity=PlacementIdentity::authored_placement;
    std::strcpy(value.model_name,"animated.sod");
    value.world[0]=value.world[5]=value.world[10]=.5f;value.world[15]=1;
    value.world[3]=x;return value;
}
std::shared_ptr<const OwnedWorldFrame> packet(Source& source,const ModelPoseSnapshot& snapshot,
    const ModelMaterialSource& parts,const OwnedAnimationPose& left,const OwnedAnimationPose& right,std::uint64_t generation){
    auto frame=std::make_unique<Frame>();frame->camera=1;frame->generation=generation;
    frame->object_count=2;frame->complete_casters=true;
    for(unsigned i=0;i<4;++i)frame->view.world_to_clip[i*5]=1;
    frame->view.viewport[2]=frame->view.viewport[3]=32;
    frame->objects[0]=object(101,-.5f);frame->objects[1]=object(102,.5f);
    for(std::size_t i=0;i<frame->object_count;++i){
        frame->objects[i].database=source.model->info.database;
        frame->objects[i].model_serial=source.model->info.serial;
        std::strcpy(frame->objects[i].model_name,source.model->info.name);
    }
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observations;
    for(const auto* pose:{&left,&right}){
        MaterialObservation selected;selected.instance=pose->instance;
        selected.database=source.model->info.database;selected.model_serial=source.model->info.serial;selected.model=source.model;
        for(const auto& part:parts.parts){MaterialBinding binding;
            binding.authored_texture=part.authored_texture;selected.parts.push_back(binding);}
        std::vector<std::uint8_t> visible(snapshot.pose->nodes.size(),1);
        std::shared_ptr<const MaterialObservation> fresh;std::string bind_error;
        const auto& object_value=frame->objects[pose->instance==101?0:1];
        need(bind_owned_part_states(object_value,snapshot,parts,*pose,visible,selected,fresh,bind_error),
             "owned all-part state binding");
        observations.emplace(pose->instance,std::move(fresh));
    }
    std::shared_ptr<const BoundMaterialCapture> bound;std::string error;
    need(build_bound_material_capture(*frame,10,resolve,&source,observations,bound,error),
         "bound posed world capture");
    auto world=std::make_shared<OwnedWorldFrame>();world->materials=bound;
    world->light_camera=1;world->light_generation=generation;world->complete_casters=true;
    world->lighting.ambient[0]=world->lighting.ambient[1]=world->lighting.ambient[2]=1;
    return world;
}
int red(const std::vector<std::uint8_t>& pixels,unsigned x,unsigned y){
    return pixels[(y*32+x)*4+2];
}
bool no_legacy_modules(){
    for(const char* name:{"d3d8.dll","d3d9.dll","d3dx8.dll","d3dx9_43.dll"})
        if(GetModuleHandleA(name))return false;
    return true;
}
int optional_source(ModelPoseCache& cache,const char* path,std::uint64_t serial){
    std::ifstream file(path,std::ios::binary);
    if(!file){std::fprintf(stderr,"REJECT optional SOD %s: unreadable\n",path);return 1;}
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    if(bytes.empty()||bytes.size()>64u*1024u*1024u){
        std::fprintf(stderr,"REJECT optional SOD %s: size outside pose-cache bounds\n",path);return 1;
    }
    auto model=asset(std::move(bytes),static_cast<std::uint32_t>(serial),serial,path);
    ModelPoseSnapshot snapshot;std::string error;
    if(!cache.inspect(model,inspect,snapshot,error)){
        std::fprintf(stderr,"REJECT optional SOD %s: %s\n",path,error.c_str());return 1;
    }
    std::printf("OPTIONAL SOD %s: nodes=%zu channels=%zu raw_references=%u\n",
        path,snapshot.pose->nodes.size(),snapshot.pose->channels.size(),snapshot.pose->reference_count);
    for(const auto& channel:snapshot.pose->channels)
        std::printf("  channel node=%u type=%u width=%u keys=%zu\n",channel.node_index,
            channel.channel_type,channel.key_width,channel.keys.size()/channel.key_width);
    std::shared_ptr<const ModelMaterialSource> pristine,all;
    if(!inspect_model_materials(model,a2fo_rs_native11_inspect_sod,pristine,error)||
       !inspect_model_materials(model,a2fo_rs_native11_inspect_sod_all,all,error)){
        std::fprintf(stderr,"REJECT optional SOD geometry %s: %s\n",path,error.c_str());return 1;
    }
    need(all->parts.size()>=pristine->parts.size(),"all scope retains pristine parts");
    std::printf("  mesh groups pristine=%zu all=%zu\n",pristine->parts.size(),all->parts.size());
    for(const auto& part:all->parts){
        need(part.node_index<snapshot.pose->nodes.size(),"real all-part node index");
        const auto& node=snapshot.pose->nodes[part.node_index];
        need(node.kind==1&&node.name==part.node_name&&part.group_index<node.group_count,
             "real all-part metadata maps to complete pose hierarchy");
    }
    return 0;
}
}
int main(int argc,char** argv){
    need(argc>=2&&argc<=4,"usage: native11_owned_pose_native.exe RENDERER_DLL [VENTURE_SOD] [MEDITERRANEAN_SOD]");
    need(no_legacy_modules(),"legacy D3D8/9 modules absent before fixture");
    Source source{asset(animated_sod(),7,11,"animated.sod")};
    need(a2fo_rs_native11_inspect_sod_pose(source.model->bytes.data(),
         static_cast<std::uint32_t>(source.model->bytes.size()),layout_sink,nullptr)==1,
         "real Rust pose metadata passes independent C ABI layout check");
    ModelPoseCache cache;ModelPoseSnapshot snapshot;std::string error;
    need(cache.inspect(source.model,inspect,snapshot,error),"real Rust synthetic SOD pose inspection");
    need(snapshot.pose->nodes.size()==1&&snapshot.pose->channels.size()==2&&
         snapshot.pose->reference_count==0&&snapshot.pose->channels[0].node_index==0&&
         snapshot.pose->channels[0].key_width==12&&snapshot.pose->channels[0].keys.size()==24&&
         snapshot.pose->channels[1].node_index==0&&snapshot.pose->channels[1].channel_type==5&&
         snapshot.pose->channels[1].key_width==1&&snapshot.pose->channels[1].keys.size()==2&&
         snapshot.pose->channels[1].keys[1]==.5f,"authored animated channel metadata");
    ModelPoseSnapshot hot;need(cache.inspect(source.model,inspect,hot,error)&&
        hot.pose==snapshot.pose&&parser_calls==1,"hot pose cache avoids Rust reparse");
    std::shared_ptr<const ModelMaterialSource> parts;
    need(inspect_model_materials(source.model,a2fo_rs_native11_inspect_sod_all,parts,error)&&
         parts->parts.size()==1,"private all-parts source inspection");
    const auto controls=playback();OwnedAnimationPose left,right;
    need(evaluate_owned_animation_pose(snapshot,clock(101,0),controls,left,error)&&
         evaluate_owned_animation_pose(snapshot,clock(102,.75f),controls,right,error),
         "independent instance poses");
    need(left.instance==101&&right.instance==102&&left.model_serial==11&&
         left.local[0][9]==0&&left.scales[0]==1&&
         right.local[0][9]==.25f&&right.scales[0]==.5f&&
         right.local_to_model[0][3]==.25f&&right.local_to_model[0][0]==.5f,
         "native Matrix34 and scalar channels preserve distinct instances");
    HMODULE dll=LoadLibraryA(argv[1]);need(dll,"renderer DLL load");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol,"API export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API5 api{};need(get(5,sizeof(api),&api)==S_OK,"API5 query");
    WorldRenderSession session;need(session.initialize(api,32,32,error),"offscreen world session");
    auto first=packet(source,snapshot,*parts,left,right,1);
    need(session.render(first,error),"first two-pose world render");
    std::vector<std::uint8_t> pixels;std::uint32_t width=0,height=0;
    need(session.readback(pixels,width,height,error)&&width==32&&height==32,
         "first posed GPU readback");
    need(red(pixels,5,18)>20&&red(pixels,21,18)<10&&red(pixels,26,17)>20,
         "left wide and right narrow authored footprints");
    const auto first_pixels=pixels;
    const auto retained_left=left,retained_right=right;
    need(evaluate_owned_animation_pose(snapshot,clock(101,.75f),controls,left,error)&&
         evaluate_owned_animation_pose(snapshot,clock(102,0),controls,right,error),
         "independent second instance poses");
    need(retained_left.local[0][9]==0&&retained_right.local[0][9]==.25f&&
         left.local[0][9]==.25f&&right.local[0][9]==0,
         "published old poses remain immutable");
    auto second=packet(source,snapshot,*parts,left,right,2);
    need(session.render(second,error),"second two-pose world render");
    need(session.readback(pixels,width,height,error)&&width==32&&height==32,
         "second posed GPU readback");
    need(red(pixels,5,18)<10&&red(pixels,21,18)>20&&red(pixels,10,16)>20,
         "left narrow and right wide independent footprint change");
    need(parser_calls==1&&session.close(error),"hot cache and explicit offscreen close");
    A2FO_N11_API9 all_api{};need(get(9,sizeof(all_api),&all_api)==S_OK,"API9 query");
    need(session.initialize_all_parts(all_api,32,32,error),"API9 all-parts world session");
    need(session.render(first,error),"API9 independent first poses");
    std::vector<std::uint8_t> all_pixels;
    need(session.readback(all_pixels,width,height,error)&&all_pixels==first_pixels,
         "API9 first poses match legacy scope on single-mesh model");
    need(session.render(second,error),"API9 independent second poses");
    need(session.readback(all_pixels,width,height,error)&&all_pixels==pixels,
         "API9 fresh second poses preserve independent animation");
    need(session.close(error),"API9 explicit close");
    Source plain{asset(native11_fixture::model("",true),7,90,"seeded.sod")};
    ModelPoseSnapshot plain_pose;
    need(cache.inspect(plain.model,inspect,plain_pose,error),"real Rust unanimated seeded model");
    std::shared_ptr<const ModelMaterialSource> plain_parts;
    need(inspect_model_materials(plain.model,a2fo_rs_native11_inspect_sod_all,plain_parts,error),
         "seeded model complete material source");
    auto defaults=std::make_shared<ModelNodeTemplateData>();defaults->root_address=0x1000;
    ModelNodeTemplateNode native;native.address=0x1000;native.name="mesh";
    native.local=plain_pose.pose->nodes[0].local;native.local[9]=.25f;native.scale=.5f;
    defaults->nodes.push_back(native);
    ModelNodeTemplate seed{plain.model,plain_pose.pose,defaults};
    OwnedAnimationPose seeded_left,seeded_right;
    need(evaluate_owned_animation_pose_from_template(plain_pose,clock(101,0),{},seed,seeded_left,error)&&
         evaluate_owned_animation_pose_from_template(plain_pose,clock(102,0),{},seed,seeded_right,error),
         "captured native defaults seed both independent instances");
    need(session.initialize_all_parts(all_api,32,32,error),"seeded API9 world session");
    need(session.render(packet(plain,plain_pose,*plain_parts,seeded_left,seeded_right,1),error)&&
         session.readback(all_pixels,width,height,error),"native-default-seeded poses reach GPU");
    need(red(all_pixels,5,18)<10&&red(all_pixels,10,17)>20&&
         red(all_pixels,21,18)<10&&red(all_pixels,26,17)>20,
         "native matrix translation and scale produce expected narrow GPU footprints");
    need(session.close(error),"seeded explicit close");
    need(FreeLibrary(dll)!=0&&no_legacy_modules(),"DLL unload without legacy D3D8/9 modules");
    int rejected=0;
    for(int i=2;i<argc;++i)rejected+=optional_source(cache,argv[i],12u+static_cast<std::uint64_t>(i));
    if(rejected){std::fprintf(stderr,"SYNTHETIC PASS; %d optional SOD inspection(s) rejected\n",rejected);return 2;}
    std::puts("PASS native11 owned pose: private Rust metadata, two instance transforms, native-default seeds, owned part binding and API5/API9 GPU pixels");
}
