// Synthetic, complete two-object world. Native class/context policy is supplied
// explicitly; real Rust SOD metadata, owned LOD evaluation and API9 reach GPU.
#include "../renderer/native11/model_pose_snapshot.hpp"
#include "../renderer/native11/model_lod_template.hpp"
#include "../renderer/native11/model_lod_visibility.hpp"
#include "../renderer/native11/owned_animation_pose.hpp"
#include "../renderer/native11/owned_part_states.hpp"
#include "../renderer/native11/material_snapshot.hpp"
#include "../renderer/native11/world_session.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include "../rust/native11_model_ffi.h"
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool pass,const char* what){if(!pass){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);}}
std::vector<std::uint8_t> model(){
    std::vector<std::uint8_t> d;
    auto half=[&](std::uint16_t v){d.push_back(v&255);d.push_back(v>>8);};
    auto full=[&](std::uint32_t v){for(unsigned i=0;i<4;++i)d.push_back((v>>(8*i))&255);};
    auto real=[&](float v){std::uint32_t bits;std::memcpy(&bits,&v,4);full(bits);};
    auto str=[&](const char* s){half(static_cast<std::uint16_t>(std::strlen(s)));while(*s)d.push_back(*s++);};
    const char magic[]="Storm3D_SW";d.insert(d.end(),magic,magic+10);real(1.93f);
    half(1);str("mat");for(float v:{0.f,0.f,0.f,1.f,1.f,1.f,0.f,0.f,0.f,1.f})real(v);
    d.push_back(2);d.push_back(0);
    const char* names[]={"root","geometry","lod0","lod1","low","high","damage","overlay"};
    const char* parents[]={"","root","geometry","geometry","lod0","lod1","root","damage"};
    const char* textures[]={"blue.tga","red.tga","green.tga"};
    half(8);
    for(unsigned node=0;node<8;++node){
        const bool mesh=node==4||node==5||node==7;
        half(mesh?1:0);str(names[node]);str(parents[node]);
        for(float v:{1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f})real(v);
        if(!mesh)continue;
        str("opaque");full(4);full(1);str(textures[node==4?0:node==5?1:2]);full(0);str("");half(0);
        half(3);half(3);half(1);
        const float centre=node==4?-.6f:node==5?0.f:.6f;
        for(float v:{centre-.28f,-.75f,.2f,centre+.28f,-.75f,.2f,centre,.75f,.2f})real(v);
        for(unsigned i=0;i<6;++i)real(0.f);
        half(1);str("mat");for(unsigned i=0;i<3;++i){half(i);half(i);}
        d.push_back(1);half(0);
    }
    half(0);half(0);return d;
}
std::vector<std::uint8_t> tga(std::uint8_t b,std::uint8_t g,std::uint8_t r){
    std::vector<std::uint8_t> d(22);d[2]=2;d[12]=d[14]=1;d[16]=24;d[17]=0x20;
    d[18]=b;d[19]=g;d[20]=r;return d;
}
struct Source {
    std::shared_ptr<const ModelAsset> model;
    std::map<std::string,std::shared_ptr<const TextureAsset>> textures;
};
std::shared_ptr<const ModelAsset> resolve(void* raw,std::uint32_t db,std::uint64_t serial){
    const auto value=static_cast<Source*>(raw)->model;
    return value&&value->info.database==db&&value->info.serial==serial?value:nullptr;
}
std::shared_ptr<const TextureAsset> texture(std::uint64_t serial,const char* name,
    std::vector<std::uint8_t> bytes){
    auto result=std::make_shared<TextureAsset>();result->serial=serial;
    result->texture=static_cast<std::uint32_t>(serial);result->logical_name=name;
    result->bytes=std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));return result;
}
Source source(){
    Source value;auto asset=std::make_shared<ModelAsset>();asset->info.database=7;
    asset->info.serial=11;std::strcpy(asset->info.name,"lod.sod");asset->bytes=model();
    asset->info.byte_count=static_cast<std::uint32_t>(asset->bytes.size());value.model=asset;
    value.textures.emplace("blue.tga",texture(1,"blue.tga",tga(255,0,0)));
    value.textures.emplace("red.tga",texture(2,"red.tga",tga(0,0,255)));
    value.textures.emplace("green.tga",texture(3,"green.tga",tga(0,255,0)));
    return value;
}
std::shared_ptr<const ModelNodeTemplate> baseline(const ModelPoseSnapshot& snapshot){
    need(snapshot.pose&&snapshot.pose->nodes.size()==8&&snapshot.pose->channels.empty()&&
         snapshot.pose->reference_count==0,"real Rust complete pose metadata");
    const char* names[]={"root","geometry","lod0","lod1","low","high","damage","overlay"};
    const std::uint32_t parents[]={UINT32_MAX,0,1,1,2,3,0,6};
    for(unsigned i=0;i<8;++i)
        need(snapshot.pose->nodes[i].name==names[i]&&snapshot.pose->nodes[i].parent==parents[i],
             "synthetic SOD node order and parent contract");
    auto data=std::make_shared<ModelNodeTemplateData>();data->nodes.resize(8);
    for(unsigned i=0;i<8;++i){auto& node=data->nodes[i];node.address=0x1000+0x100*i;
        node.name=names[i];node.parent=parents[i];node.local=snapshot.pose->nodes[i].local;
        node.scale=1;node.flags=i==2||i==3||i==6?3:0;
        if(parents[i]!=UINT32_MAX)node.parent_address=0x1000+0x100*parents[i];}
    auto child=[&](unsigned parent,unsigned first){data->nodes[parent].first_child=first;
        data->nodes[parent].child_address=data->nodes[first].address;};
    auto sibling=[&](unsigned first,unsigned next){data->nodes[first].next_sibling=next;
        data->nodes[first].sibling_address=data->nodes[next].address;};
    child(0,1);sibling(1,6);child(1,2);sibling(2,3);
    child(2,4);child(3,5);child(6,7);
    data->root_address=data->nodes[0].address;
    auto lods=std::make_shared<ModelLodTemplateData>();
    // These list ordinals are explicit synthetic completed-load provenance.
    lods->controls.push_back({1,{0,0,0},0,{{2,1},{3,1}}});data->lods=lods;
    return std::make_shared<const ModelNodeTemplate>(ModelNodeTemplate{snapshot.model,snapshot.pose,data});
}
InstanceAnimationTime timing(std::uint32_t instance){
    InstanceAnimationTime value;value.engine=1;value.instance=instance;value.database=7;
    value.engine_time=1;value.start_time=1;value.elapsed=0;return value;
}
Object object(std::uint32_t instance,float x){
    Object value;value.entity=instance+100;value.instance=instance;
    value.database=7;value.model_serial=11;value.model_identity=ModelIdentity::plain_database_name;
    value.placement_identity=PlacementIdentity::authored_placement;
    std::strcpy(value.model_name,"lod.sod");
    value.world[0]=value.world[5]=value.world[10]=.35f;value.world[15]=1;
    value.world[3]=x;return value;
}
ModelLodContext context(std::uint32_t instance,float x,int requested){
    ModelLodContext value;value.engine=1;value.camera=1;value.render_context=1;
    value.instance=instance;value.database=7;
    value.native_x87_control=encode_native_x87(0x037f);
    value.placement={{.35f,0,0,0,.35f,0,0,0,.35f,x,0,0}};
    value.view={{1,0,0,0,1,0,0,0,1,0,0,0}};
    value.thresholds={{100,50,20,10}};value.requested=requested;return value;
}
std::shared_ptr<const OwnedWorldFrame> packet(Source& source,const ModelPoseSnapshot& snapshot,
    const ModelMaterialSource& materials,const ModelNodeTemplate& native,
    std::uint64_t generation,const std::array<int,2>& requested){
    auto frame=std::make_unique<Frame>();frame->camera=1;frame->generation=generation;
    frame->complete_casters=true;frame->object_count=2;
    for(unsigned i=0;i<4;++i)frame->view.world_to_clip[i*5]=1;
    frame->view.viewport[2]=frame->view.viewport[3]=32;
    frame->objects[0]=object(101,-.5f);frame->objects[1]=object(102,.5f);
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observations;
    for(unsigned i=0;i<2;++i){
        const auto& item=frame->objects[i];const float x=i? .5f:-.5f;
        OwnedAnimationPose pose;std::string error;
        need(evaluate_owned_animation_pose(snapshot,timing(item.instance),{},pose,error),
             "fresh owned Rust pose for each synthetic object");
        std::vector<std::uint32_t> class_flags;
        for(const auto& node:native.data->nodes)class_flags.push_back(node.flags);
        std::vector<std::uint8_t> class_write_masks(class_flags.size(),0);
        class_write_masks[6]=3; // explicit synthetic damage-branch hide, not a LOD level write
        ModelLodVisibility lod;
        need(evaluate_model_lod_visibility(native,pose,context(item.instance,x,requested[i]),
            class_flags,class_write_masks,lod,error),"fresh native-order LOD visibility");
        need(lod.selected_nodes.size()==1&&
             lod.selected_nodes[0]==static_cast<std::uint32_t>(requested[i]?3:2)&&
             lod.visible[4]==!requested[i]&&lod.visible[5]==bool(requested[i])&&
             !lod.visible[7],"one selected LOD, damage subtree suppressed");
        MaterialObservation selected;selected.instance=item.instance;selected.database=7;
        selected.model_serial=11;selected.model=source.model;
        selected.native_x87_control=encode_native_x87(0x037f);
        for(const auto& part:materials.parts){MaterialBinding binding;
            binding.authored_texture=part.authored_texture;
            if(!part.authored_texture.empty()){
                const auto found=source.textures.find(part.authored_texture);
                need(found!=source.textures.end(),"authored synthetic texture owner");
                binding.texture=found->second;
            }
            selected.parts.push_back(std::move(binding));
        }
        std::vector<std::uint8_t> visible;
        for(bool state:lod.visible)visible.push_back(state?1:0);
        std::shared_ptr<const MaterialObservation> bound;
        need(bind_owned_part_states(item,snapshot,materials,pose,visible,selected,bound,error),
             "owned per-part state binds Rust all-parts metadata");
        need(bound->parts.size()==3&&bound->parts[0].node_state.visible==!requested[i]&&
             bound->parts[1].node_state.visible==bool(requested[i])&&
             !bound->parts[2].node_state.visible,"all authored groups bound with LOD and damage state");
        observations.emplace(item.instance,std::move(bound));
    }
    std::shared_ptr<const BoundMaterialCapture> capture;std::string error;
    need(build_bound_material_capture(*frame,10,resolve,&source,observations,capture,error),
         "sealed complete synthetic material frame");
    auto world=std::make_shared<OwnedWorldFrame>();world->materials=capture;
    world->light_camera=1;world->light_generation=generation;world->complete_casters=true;
    world->lighting.ambient[0]=world->lighting.ambient[1]=world->lighting.ambient[2]=1;
    return world;
}
int channel(const std::vector<std::uint8_t>& pixels,unsigned x,unsigned component){
    return pixels[(16*32+x)*4+component];
}
void coloured(const std::vector<std::uint8_t>& pixels,unsigned x,unsigned component,const char* what){
    need(channel(pixels,x,component)>100&&
         channel(pixels,x,(component+1)%3)<35&&channel(pixels,x,(component+2)%3)<35,what);
}
bool no_legacy_modules(){
    for(const char* name:{"d3d8.dll","d3d9.dll","d3dx8.dll","d3dx9_43.dll"})
        if(GetModuleHandleA(name))return false;
    return true;
}
}
int main(int argc,char** argv){
    need(argc==2,"renderer DLL path required");
    need(no_legacy_modules(),"no D3D8/9 before offscreen fixture");
    Source input=source();ModelPoseCache cache;ModelPoseSnapshot snapshot;std::string error;
    need(cache.inspect(input.model,a2fo_rs_native11_inspect_sod_pose,snapshot,error),
         "real Rust complete SOD pose inspection");
    std::shared_ptr<const ModelMaterialSource> materials;
    need(inspect_model_materials(input.model,a2fo_rs_native11_inspect_sod_all,materials,error)&&
         materials->parts.size()==3&&materials->parts[0].node_index==4&&
         materials->parts[1].node_index==5&&materials->parts[2].node_index==7,
         "real Rust all-part groups in authored order");
    auto native=baseline(snapshot);
    HMODULE dll=LoadLibraryA(argv[1]);need(dll!=nullptr,"renderer DLL load");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol!=nullptr,"API export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API9 api{};need(get(9,sizeof(api),&api)==S_OK,"API9 query");
    WorldRenderSession session;need(session.initialize_all_parts(api,32,32,error),
         "API9 complete offscreen world session");
    auto first=packet(input,snapshot,*materials,*native,1,{{0,1}});
    need(session.render(first,error),"first two-object LOD world submit");
    std::vector<std::uint8_t> pixels;std::uint32_t width=0,height=0;
    need(session.readback(pixels,width,height,error)&&width==32&&height==32,
         "first real GPU readback");
    coloured(pixels,4,0,"left coarse blue footprint");
    coloured(pixels,24,2,"right highest red footprint");
    need(channel(pixels,8,2)<35&&channel(pixels,20,0)<35&&
         channel(pixels,11,1)<35&&channel(pixels,27,1)<35,
         "unselected levels and damage have no first-frame footprints");
    const auto first_pixels=pixels;
    auto second=packet(input,snapshot,*materials,*native,2,{{1,0}});
    need(first->materials->same_resources(*second->materials),
         "LOD state swap preserves model and texture resources");
    need(session.render(second,error),"second two-object LOD world submit");
    need(session.readback(pixels,width,height,error)&&width==32&&height==32&&
         pixels!=first_pixels,"second real GPU readback changes with fresh choices");
    coloured(pixels,8,2,"left highest red footprint");
    coloured(pixels,20,0,"right coarse blue footprint");
    need(channel(pixels,4,0)<35&&channel(pixels,24,2)<35&&
         channel(pixels,11,1)<35&&channel(pixels,27,1)<35,
         "unselected levels and damage have no second-frame footprints");
    need(session.close(error),"explicit offscreen session close");
    need(FreeLibrary(dll)!=0&&no_legacy_modules(),"DLL unload without D3D8/9");
    std::puts("PASS native11 synthetic all-parts LOD GPU chain");
}
