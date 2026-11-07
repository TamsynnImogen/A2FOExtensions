#include "../renderer/native11/owned_part_states.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
std::array<float,12> local(){return {1,0,0,0,1,0,0,0,1,0,0,0};}
std::array<float,16> matrix(float x){return {1,0,0,x,0,1,0,0,0,0,1,0,0,0,0,1};}
struct Fixture {
    Object object;
    ModelPoseSnapshot snapshot;
    ModelMaterialSource source;
    OwnedAnimationPose pose;
    std::vector<std::uint8_t> visible{0,1};
    MaterialObservation selected;
    std::shared_ptr<const MaterialObservation> output;
    std::string error;
    Fixture(){
        auto model=std::make_shared<ModelAsset>();
        model->info.database=7;model->info.serial=11;model->bytes={1};model->info.byte_count=1;
        std::strcpy(model->info.name,"ship.sod");
        auto metadata=std::make_shared<AuthoredPoseData>();
        AuthoredPoseNode mesh;mesh.name="mesh";mesh.parent=1;mesh.kind=1;
        mesh.group_count=2;mesh.local=local();
        AuthoredPoseNode root;root.name="root";root.local=local();
        metadata->nodes={mesh,root};snapshot={model,metadata};source.model=model;
        source.parts={{"mesh","hull.dds",0,0},{"mesh","hull.dds",0,1}};
        object.instance=101;object.database=7;object.model_serial=11;
        object.model_identity=ModelIdentity::plain_database_name;
        std::strcpy(object.model_name,"ship.sod");
        pose.instance=101;pose.database=7;pose.model_serial=11;
        pose.local={local(),local()};pose.scales={1,1};
        pose.local_to_model={matrix(3),matrix(0)};
        auto texture=std::make_shared<TextureAsset>();
        texture->serial=41;texture->texture=301;texture->logical_name="hull.dds";
        texture->bytes=std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{2});
        selected.instance=101;selected.database=7;selected.model_serial=11;
        selected.model=model;
        MaterialBinding stale;stale.authored_texture="hull.dds";stale.texture=texture;
        stale.node_state_valid=true;stale.node_state.visible=true;
        std::memcpy(stale.node_state.local_to_model,matrix(99).data(),sizeof(stale.node_state.local_to_model));
        selected.parts={stale,stale};
    }
    bool bind(){return bind_owned_part_states(object,snapshot,source,pose,visible,selected,output,error);}
    void reject(){auto old=output;assert(!bind());assert(output==old&&!error.empty());}
};
}
int main(){
    Fixture f;
    assert(f.bind()&&f.error.empty()&&f.output->parts.size()==2);
    assert(f.output->model==f.snapshot.model&&f.output->parts[0].texture==f.selected.parts[0].texture);
    for(const auto& part:f.output->parts)
        assert(part.node_state_valid&&!part.node_state.visible&&
               part.node_state.local_to_model[3]==3);
    assert(f.selected.parts[0].node_state.local_to_model[3]==99);
    auto first=f.output;
    auto second=f;second.object.instance=202;second.pose.instance=202;
    second.selected.instance=202;second.visible[0]=1;
    second.pose.local_to_model[0]=matrix(8);
    assert(second.bind()&&second.output->instance==202&&
           second.output->parts[0].node_state.visible&&
           second.output->parts[0].node_state.local_to_model[3]==8&&
           first->parts[0].node_state.local_to_model[3]==3);
    // Copied observation pins both owners even after source and selected release.
    auto retained=second.output;
    second.source={};second.selected={};second.snapshot={};
    assert(retained->model&&retained->parts[0].texture&&retained->parts[0].texture->bytes->at(0)==2);

    f.source.model=std::make_shared<ModelAsset>(*f.snapshot.model);f.reject();
    f.source.model=f.snapshot.model;
    f.selected.model=std::make_shared<ModelAsset>(*f.snapshot.model);f.reject();
    f.selected.model=f.snapshot.model;
    auto original_name=f.object.model_name[0];
    f.object.model_name[0]=0;f.reject();f.object.model_name[0]=original_name;
    std::memset(f.object.model_name,'x',sizeof(f.object.model_name));f.reject();
    std::strcpy(f.object.model_name,"ship.sod");
    std::strcpy(f.object.model_name,"foreign.sod");f.reject();
    std::strcpy(f.object.model_name,"ship.sod");
    f.object.model_identity=ModelIdentity::unsupported_database;f.reject();
    f.object.model_identity=ModelIdentity::plain_database_name;
    auto altered_model=std::make_shared<ModelAsset>(*f.snapshot.model);
    std::memset(altered_model->info.name,'x',sizeof(altered_model->info.name));
    f.snapshot.model=altered_model;f.source.model=altered_model;f.selected.model=altered_model;
    f.reject();
    altered_model->info.name[0]=0;f.reject();
    std::strcpy(altered_model->info.name,"foreign.sod");f.reject();
    f.snapshot.model=first->model;f.source.model=first->model;f.selected.model=first->model;
    auto singular=f;
    singular.pose.local_to_model[1][0]=0;singular.visible[1]=1;
    assert(singular.bind()&&singular.output->parts[0].node_state.visible==false);
    singular.pose.local_to_model[0][0]=0;
    assert(singular.bind()&&singular.output->parts[0].node_state.visible==false);
    singular.visible[0]=1;singular.reject();
    f.visible[0]=2;f.reject();f.visible[0]=0;
    f.visible.pop_back();f.reject();f.visible.push_back(1);
    f.pose.local.pop_back();f.reject();f.pose.local.push_back(local());
    f.pose.scales.pop_back();f.reject();f.pose.scales.push_back(1);
    f.pose.local_to_model.pop_back();f.reject();f.pose.local_to_model.push_back(matrix(0));
    f.pose.local_to_model[0][3]=NAN;f.reject();f.pose.local_to_model[0]=matrix(3);
    f.pose.local_to_model[0][15]=2;f.reject();f.pose.local_to_model[0]=matrix(3);
    f.source.parts[1].group_index=0;f.reject();f.source.parts[1].group_index=1;
    std::swap(f.source.parts[0],f.source.parts[1]);f.reject();
    std::swap(f.source.parts[0],f.source.parts[1]);
    f.source.parts[1].node_index=1;f.reject();f.source.parts[1].node_index=0;
    f.source.parts[1].node_name="root";f.reject();f.source.parts[1].node_name="mesh";
    f.source.parts[1].group_index=2;f.reject();f.source.parts[1].group_index=1;
    f.selected.parts[1].authored_texture="wrong.dds";f.reject();
    f.selected.parts[1].authored_texture="hull.dds";
    f.selected.parts[1].texture.reset();f.reject();
    f.selected.parts[1].texture=f.selected.parts[0].texture;
    f.selected.parts[1].texture=std::make_shared<TextureAsset>(*f.selected.parts[0].texture);
    f.reject();f.selected.parts[1].texture=f.selected.parts[0].texture;
    f.pose.instance=999;f.reject();f.pose.instance=101;
    f.object.model_serial=12;f.reject();f.object.model_serial=11;
    f.selected.parts.pop_back();f.reject();
}
