#include "../renderer/native11/observed_world.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Assets {std::shared_ptr<const ModelAsset> model;};
std::shared_ptr<const ModelAsset> model(std::uint8_t byte){auto value=std::make_shared<ModelAsset>();
    value->info.database=7;value->info.serial=11;std::strcpy(value->info.name,"world.sod");
    value->bytes={byte};value->info.byte_count=1;return value;}
std::shared_ptr<const ModelAsset> resolve(void* raw,std::uint32_t database,std::uint64_t serial){
    auto asset=static_cast<Assets*>(raw)->model;
    return asset&&asset->info.database==database&&asset->info.serial==serial?asset:nullptr;
}
std::shared_ptr<const BoundMaterialCapture> capture(Assets& assets,std::uint32_t entity,
    std::uint32_t instance,std::uint64_t generation,bool shadow,float x){
    auto frame=std::make_unique<Frame>();frame->camera=1;frame->generation=generation;
    frame->object_count=1;for(unsigned i=0;i<4;++i){frame->objects[0].world[i*5]=1;
        frame->view.world_to_clip[i*5]=1;}
    frame->view.viewport[2]=frame->view.viewport[3]=32;
    auto& object=frame->objects[0];object.entity=entity;object.instance=instance;
    object.database=7;object.model_serial=11;object.model_identity=ModelIdentity::plain_database_name;
    object.placement_identity=PlacementIdentity::authored_placement;object.shadow_only=shadow;
    object.world[3]=x;std::strcpy(object.model_name,"world.sod");
    auto observed=std::make_shared<MaterialObservation>();observed->instance=instance;
    observed->database=7;observed->model_serial=11;observed->model=assets.model;
    MaterialBinding part{};part.node_state_valid=true;observed->parts.push_back(part);
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observations{{instance,observed}};
    std::shared_ptr<const BoundMaterialCapture> output;std::string error;
    need(build_bound_material_capture(*frame,5,resolve,&assets,observations,output,error),"input capture");
    return output;
}
std::shared_ptr<const BoundMaterialCapture> both_visible(Assets& assets,
    const BoundMaterialCapture& left,const BoundMaterialCapture& right){
    auto frame=std::make_unique<Frame>(left.frame());frame->object_count=2;
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observations;
    std::string error;
    for(const auto& item:{std::pair<const BoundMaterialCapture*,std::uint32_t>{&left,101},
                         std::pair<const BoundMaterialCapture*,std::uint32_t>{&right,102}}){
        Object raw{};std::shared_ptr<const MaterialObservation> observed;
        need(item.first->export_instance(item.second,raw,observed,error),"visible export");
        raw.shadow_only=false;frame->objects[observations.size()]=raw;
        observations.emplace(item.second,std::move(observed));
    }
    std::shared_ptr<const BoundMaterialCapture> output;
    need(build_bound_material_capture(*frame,5,resolve,&assets,observations,output,error),
        "both-visible capture");return output;
}
WorldLightCapture world(){WorldLightCapture value;
    auto owner=std::make_shared<LightClassRecord>();owner->class_id=1001;owner->version=7;
    for(const auto member:{WorldMember{201,101,1001,7},WorldMember{202,102,1001,7}}){
        value.members.push_back(member);value.class_owners.push_back(owner);
        PlacedLight light{};light.source_id=member.entity;value.sources.push_back(light);}
    return value;
}
DomainEvidence full(){return {true,true,true};}
const Object& by_entity(const BoundMaterialCapture& capture,std::uint32_t entity){
    for(std::size_t i=0;i<capture.frame().object_count;++i)
        if(capture.frame().objects[i].entity==entity)return capture.frame().objects[i];
    std::fprintf(stderr,"FAIL missing assembled entity\n");std::exit(1);
}
}
int main(){
    Assets assets{model(1)};auto members=world();
    auto visible=capture(assets,201,101,1,false,0);
    ObservedWorldAssembler assembler;std::string error;
    need(!assembler.update(visible,members,{},full(),10,error)&&
        assembler.report().missing.size()==1&&!assembler.current(),"offscreen member blocks admission");
    auto candidate=capture(assets,202,102,1,true,.5f);
    auto entirely_visible=both_visible(assets,*visible,*candidate);
    need(!assembler.update(entirely_visible,members,{},DomainEvidence{},10,error)&&
        assembler.report().game_objects_complete&&assembler.report().covered.size()==2&&
        assembler.report().covered[0].visible&&assembler.report().covered[1].visible&&
        !assembler.current(),"all visible GameObjects cannot prove other caster domains");
    need(!assembler.update(visible,members,{candidate},{},10,error)&&
        assembler.report().game_objects_complete&&!assembler.current(),
        "shadow candidate alone does not prove other caster domains");
    need(assembler.update(visible,members,{candidate},full(),10,error),"complete owned frame admitted");
    auto first=assembler.current();need(first&&first->source_epoch()==10&&
        first->frame().complete_casters&&first->frame().requires_part_states&&
        first->frame().object_count==2&&!by_entity(*first,201).shadow_only&&
        by_entity(*first,202).shadow_only,"visible and shadow-only roles");
    auto stale=capture(assets,202,102,2,true,.5f);
    need(!assembler.update(visible,members,{stale},full(),11,error)&&
        assembler.current()==first,"stale candidate retains current");
    auto conflicting=capture(assets,201,101,1,false,.5f);
    need(!assembler.update(visible,members,{conflicting,candidate},full(),11,error)&&
        assembler.current()==first,"conflicting duplicate candidate refused");
    auto moved_visible=capture(assets,201,101,2,false,.25f);
    auto moved_candidate=capture(assets,202,102,2,true,.75f);
    need(assembler.update(moved_visible,members,{moved_candidate},full(),11,error)&&
        assembler.current()->source_epoch()==10&&
        by_entity(*assembler.current(),202).world[3]==.75f,"pose-only frame keeps resource epoch");
    auto prior=assembler.current();
    assets.model=model(2);auto replacement_visible=capture(assets,201,101,3,false,.25f);
    auto replacement_candidate=capture(assets,202,102,3,true,.75f);
    need(assembler.update(replacement_visible,members,{replacement_candidate},full(),12,error)&&
        assembler.current()->source_epoch()==12,"same serial new owner advances resource epoch");
    auto latest=assembler.current();
    Assets conflicting_assets{model(3)};
    auto owner_conflict=capture(conflicting_assets,202,102,3,true,.75f);
    need(!assembler.update(replacement_visible,members,{owner_conflict},full(),13,error)&&
        assembler.current()==latest,"same database and serial with two owners refused");
    need(!assembler.update(replacement_visible,members,{},full(),13,error)&&
        assembler.current()==latest&&assembler.report().missing.size()==1,
        "missing coverage updates diagnostic but retains latest owner");
    need(!assembler.update(replacement_visible,members,{replacement_candidate},{},13,error)&&
        assembler.current()==latest&&assembler.report().game_objects_complete,
        "domain gap keeps last owner and complete GameObject report");
    std::weak_ptr<const BoundMaterialCapture> weak=latest;
    latest.reset();prior.reset();first.reset();
    visible.reset();candidate.reset();entirely_visible.reset();stale.reset();conflicting.reset();moved_visible.reset();
    moved_candidate.reset();replacement_visible.reset();replacement_candidate.reset();owner_conflict.reset();
    need(!weak.expired()&&assembler.clear()&&weak.expired()&&!assembler.current(),
        "clear releases latest frame and report");
    need(!assembler.update(capture(assets,201,101,4,false,0),members,{},full(),12,error),
        "clear cannot make incomplete frame admissible");
    auto recovered_visible=capture(assets,201,101,4,false,0);
    auto recovered_candidate=capture(assets,202,102,4,true,.5f);
    need(!assembler.update(recovered_visible,members,{recovered_candidate},full(),12,error),
        "clear retains admitted epoch guard");
    need(assembler.update(recovered_visible,members,{recovered_candidate},full(),13,error)&&
        assembler.current()->source_epoch()==13,"fresh epoch recovers after clear");
    std::puts("native11 observed world test passed");
}
