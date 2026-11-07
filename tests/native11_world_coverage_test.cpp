#include "../renderer/native11/world_coverage.hpp"
#include "native11_model_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool good,const char* name) {
    if (!good) { std::fprintf(stderr,"FAIL %s\n",name);std::exit(1); }
}
struct Assets {
    std::shared_ptr<const ModelAsset> model;
    Assets() {
        auto value=std::make_shared<ModelAsset>();value->info.database=7;value->info.serial=11;
        std::strcpy(value->info.name,"world.sod");value->bytes=native11_fixture::model();
        value->info.byte_count=value->bytes.size();model=value;
    }
    static std::shared_ptr<const ModelAsset> resolve(void* raw,std::uint32_t db,std::uint64_t serial) {
        const auto& assets=*static_cast<Assets*>(raw);
        return assets.model&&assets.model->info.database==db&&assets.model->info.serial==serial?
            assets.model:nullptr;
    }
};
struct Spec {std::uint32_t entity,instance;bool shadow_only=false;float x=0;};
std::shared_ptr<const BoundMaterialCapture> capture(Assets& assets,
    const std::vector<Spec>& specs,std::uint64_t generation=1,bool states=true,
    std::uint32_t camera=1) {
    auto frame=std::make_unique<Frame>();frame->camera=camera;frame->generation=generation;
    frame->object_count=specs.size();
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observations;
    for (std::size_t i=0;i<specs.size();++i) {
        const auto& spec=specs[i];auto& object=frame->objects[i];
        object.entity=spec.entity;object.instance=spec.instance;object.database=7;
        object.model_serial=11;object.model_identity=ModelIdentity::plain_database_name;
        object.placement_identity=PlacementIdentity::authored_placement;
        object.shadow_only=spec.shadow_only;std::strcpy(object.model_name,"world.sod");
        for (int j=0;j<16;++j)object.world[j]=(j%5)==0?1.f:0.f;
        object.world[3]=spec.x;
        auto observed=std::make_shared<MaterialObservation>();observed->instance=spec.instance;
        observed->database=7;observed->model_serial=11;observed->model=assets.model;
        MaterialBinding part{};part.node_state_valid=states;observed->parts.push_back(part);
        observations.emplace(spec.instance,observed);
    }
    std::shared_ptr<const BoundMaterialCapture> result;std::string error;
    need(build_bound_material_capture(*frame,20,Assets::resolve,&assets,observations,result,error),
         "material fixture builds");return result;
}
std::shared_ptr<const LightClassRecord> klass() {
    auto value=std::make_shared<LightClassRecord>();value->class_id=1001;value->version=5;
    return value;
}
WorldLightCapture world(const std::vector<WorldMember>& members) {
    WorldLightCapture result{};auto owner=klass();
    for (auto member:members) {
        result.members.push_back(member);result.class_owners.push_back(owner);
        PlacedLight source{};source.source_id=member.entity;result.sources.push_back(source);
    }
    return result;
}
WorldMember member(std::uint32_t entity,std::uint32_t instance) {
    return {entity,instance,1001,5};
}
void reject(const BoundMaterialCapture& visible,const WorldLightCapture& world,
    const std::vector<std::shared_ptr<const BoundMaterialCapture>>& candidates,const char* name) {
    CoverageReport report{};report.world_objects=77;report.observed_objects=55;
    report.game_objects_complete=true;report.missing.push_back(member(9,9));
    std::string error="old";
    need(!audit_coverage(visible,world,candidates,report,error),name);
    need(report.world_objects==77&&report.observed_objects==55&&
         report.game_objects_complete&&report.missing.size()==1&&
         report.missing[0].entity==9,"failure transactional");
    need(!error.empty()&&error!="old","failure diagnostic");
}
}
int main() {
    Assets assets;
    auto visible=capture(assets,{{201,101}});
    auto all=capture(assets,{{201,101},{202,102,true}});
    auto checked=world({member(201,101),member(202,102)});
    CoverageReport report{};std::string error;
    need(audit_coverage(*visible,checked,{all},report,error)&&error.empty()&&
         report.world_objects==2&&report.observed_objects==2&&
         report.missing.empty()&&report.game_objects_complete&&report.covered.size()==2,
         "visible and offscreen candidate cover checked GameObjects");
    need(report.covered[0].member.entity==201&&report.covered[0].visible&&
         !report.covered[0].shadow_only&&report.covered[1].member.entity==202&&
         !report.covered[1].visible&&report.covered[1].shadow_only,
         "visible preferred and shadow-only role reported");
    auto candidate_role=capture(assets,{{201,101,true},{202,102,true}});
    need(audit_coverage(*visible,checked,{candidate_role},report,error)&&
         report.game_objects_complete&&report.covered[0].visible&&
         !report.covered[0].shadow_only&&report.covered[1].shadow_only,
         "candidate role cannot override identical visible receiver");
    need(audit_coverage(*visible,checked,{},report,error)&&
         report.missing.size()==1&&report.missing[0].entity==202&&
         !report.game_objects_complete,"no candidate does not imply offscreen absence");
    auto partial=world({member(201,101),member(202,102),member(203,0)});
    need(audit_coverage(*visible,partial,{all},report,error)&&
         report.world_objects==3&&report.observed_objects==2&&
         report.missing.size()==1&&report.missing[0].entity==203&&
         !report.game_objects_complete,"zero instance remains missing, no absence inference");
    all.reset();visible.reset();partial.sources.clear();partial.members.clear();
    partial.class_owners.clear();assets.model.reset();
    need(report.missing[0].instance==0&&report.covered[0].member.entity==201,
         "report owns values after input removal");
    Assets fresh;visible=capture(fresh,{{201,101}});
    all=capture(fresh,{{201,101},{202,102,true}});
    checked=world({member(201,101),member(202,102)});
    auto extra=capture(fresh,{{201,101},{202,102,true},{204,104,true}});
    reject(*visible,checked,{extra},"extra geometry outside checked list");
    auto stale=capture(fresh,{{201,101},{202,102,true}},2);
    reject(*visible,checked,{stale},"stale candidate generation");
    auto wrong_camera=capture(fresh,{{201,101},{202,102,true}},1,true,2);
    reject(*visible,checked,{wrong_camera},"wrong candidate camera");
    reject(*visible,checked,{all,all},"duplicate offscreen candidate ambiguous");
    auto conflict=capture(fresh,{{201,101},{202,102,true,.5f}});
    reject(*visible,checked,{all,conflict},"conflicting candidate pose");
    auto primary_conflict=capture(fresh,{{201,101,false,.5f},{202,102,true}});
    reject(*visible,checked,{primary_conflict},"candidate conflicts with visible primary");
    auto duplicate=capture(fresh,{{201,101},{201,103,true}});
    reject(*visible,checked,{duplicate},"duplicate entity inside candidate");
    auto unobserved=capture(fresh,{{201,101},{202,102,true}},1,false);
    reject(*visible,checked,{unobserved},"nonempty candidate lacks part states");
    auto bad_world=checked;bad_world.members[1].entity=201;
    reject(*visible,bad_world,{all},"duplicate world entity");
    bad_world=checked;bad_world.members[1].instance=101;
    reject(*visible,bad_world,{all},"duplicate world instance");
    bad_world=checked;bad_world.sources[1].source_id=999;
    reject(*visible,bad_world,{all},"source ID mismatch");
    bad_world=checked;bad_world.members[1].class_version=6;
    reject(*visible,bad_world,{all},"class owner version mismatch");
    bad_world=checked;bad_world.class_owners.pop_back();
    reject(*visible,bad_world,{all},"parallel owner mismatch");
    std::vector<std::shared_ptr<const BoundMaterialCapture>> many(17,all);
    reject(*visible,checked,many,"candidate count cap");
    bad_world=checked;bad_world.members.resize(4097);bad_world.sources.resize(4097);
    bad_world.class_owners.resize(4097);
    reject(*visible,bad_world,{all},"world member cap");
    std::vector<Spec> large_specs;large_specs.reserve(2048);
    for (std::uint32_t i=0;i<2048;++i)
        large_specs.push_back({3000+i,4000+i,true});
    auto large=capture(fresh,large_specs);
    reject(*visible,checked,{large},"2048 combined geometry cap");
    std::puts("native11 world coverage: PASS");
}
