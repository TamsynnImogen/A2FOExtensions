#include "../renderer/native11/owned_animation_pose.hpp"
#include "../renderer/native11/model_node_template.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
std::array<float,12> matrix(float basis,float x){
    return {basis,0,0,0,basis,0,0,0,basis,x,0,0};
}
bool near(float a,float b){return std::fabs(a-b)<0.00001f;}
struct Fixture {
    ModelPoseSnapshot runtime;
    ModelNodeTemplate native;
    InstanceAnimationTime clock;
    std::vector<PoseChannelPlayback> playback;
    OwnedAnimationPose output;
    std::string error;
    Fixture(){
        auto asset=std::make_shared<ModelAsset>();asset->info.database=7;asset->info.serial=11;
        auto pose=std::make_shared<AuthoredPoseData>();
        AuthoredPoseNode child;child.name="child";child.parent=1;child.kind=1;
        child.group_count=1;child.local=matrix(1,3);
        AuthoredPoseNode root;root.name="root";root.local=matrix(1,0);
        pose->nodes={child,root};runtime={asset,pose};
        auto data=std::make_shared<ModelNodeTemplateData>();
        ModelNodeTemplateNode live_child;live_child.name="child";live_child.parent=1;
        live_child.local=matrix(1,4);live_child.scale=2;
        ModelNodeTemplateNode live_root;live_root.name="root";
        live_root.local=matrix(1,10);live_root.scale=3;
        data->nodes={live_child,live_root};
        native={asset,pose,data};
        clock.engine=2;clock.instance=101;clock.database=7;clock.flags=1;
        clock.animating=true;clock.engine_time=1;clock.start_time=.25f;clock.elapsed=.75f;
    }
    bool evaluate(){return evaluate_owned_animation_pose_from_template(
        runtime,clock,playback,native,output,error);}
    void reject(){
        const auto old=output;
        assert(!evaluate());assert(!error.empty());
        assert(output.instance==old.instance&&output.database==old.database&&
               output.local==old.local&&output.scales==old.scales&&
               output.local_to_model==old.local_to_model);
    }
    void matrix_channel(float x){
        auto copy=std::make_shared<AuthoredPoseData>(*runtime.pose);
        AuthoredPoseChannel channel;channel.node_index=0;channel.channel_type=1;
        channel.period=1;channel.key_width=12;
        const auto value=matrix(1,x);
        channel.keys.assign(value.begin(),value.end());
        copy->channels.push_back(channel);runtime.pose=copy;
        PoseChannelPlayback play;play.interpolation=2;playback.push_back(play);
    }
    void scalar_channel(float value){
        auto copy=std::make_shared<AuthoredPoseData>(*runtime.pose);
        AuthoredPoseChannel channel;channel.node_index=0;channel.channel_type=5;
        channel.period=1;channel.key_width=1;channel.keys={value};
        copy->channels.push_back(channel);runtime.pose=copy;
        PoseChannelPlayback play;play.interpolation=2;playback.push_back(play);
    }
    void edit_data(void(*edit)(ModelNodeTemplateData&)){
        auto copy=std::make_shared<ModelNodeTemplateData>(*native.data);
        edit(*copy);native.data=copy;
    }
};
}
int main(){
    Fixture f;
    assert(f.evaluate()&&f.error.empty()&&f.output.local.size()==2);
    assert(near(f.output.local[0][9],4)&&near(f.output.local[1][9],10)&&
           near(f.output.scales[0],2)&&near(f.output.scales[1],3)&&
           near(f.output.local_to_model[0][0],6)&&
           near(f.output.local_to_model[0][3],22));
    OwnedAnimationPose legacy;std::string err;
    assert(evaluate_owned_animation_pose(f.runtime,f.clock,f.playback,legacy,err)&&
           near(legacy.local_to_model[0][3],3));
    f=Fixture{};f.matrix_channel(7);
    assert(f.evaluate()&&near(f.output.local[0][9],7)&&
           near(f.output.scales[0],2)&&near(f.output.local_to_model[0][3],31));
    f=Fixture{};f.scalar_channel(4);
    assert(f.evaluate()&&near(f.output.local[0][9],4)&&
           near(f.output.scales[0],4)&&near(f.output.local_to_model[0][0],12)&&
           near(f.output.local_to_model[0][3],22));
    f=Fixture{};f.matrix_channel(7);f.scalar_channel(4);
    assert(f.evaluate()&&near(f.output.local[0][9],7)&&
           near(f.output.scales[0],4)&&near(f.output.local_to_model[0][0],12)&&
           near(f.output.local_to_model[0][3],31));
    f=Fixture{};f.matrix_channel(7);
    auto copy=std::make_shared<AuthoredPoseData>(*f.runtime.pose);
    const auto second=matrix(1,8);
    copy->channels[0].keys.insert(copy->channels[0].keys.end(),second.begin(),second.end());
    f.runtime.pose=copy;f.playback[0].interpolation=0;
    f.output.instance=999;f.reject(); // native baseline cannot stand in for no-write
    f=Fixture{};f.scalar_channel(4);
    copy=std::make_shared<AuthoredPoseData>(*f.runtime.pose);
    copy->channels[0].keys.push_back(5);f.runtime.pose=copy;
    f.playback[0].interpolation=0;f.output.instance=999;f.reject();
    f=Fixture{};
    f.edit_data([](ModelNodeTemplateData& d){d.nodes[0].local=matrix(0,0);d.nodes[0].scale=0;});
    assert(f.evaluate()&&near(f.output.local_to_model[0][0],0));
    f=Fixture{};assert(f.evaluate());auto different=std::make_shared<ModelAsset>(*f.runtime.model);
    f.native.model=different;f.reject();
    f=Fixture{};assert(f.evaluate());
    auto authored=std::make_shared<AuthoredPoseData>(*f.native.authored);
    authored->nodes[0].name="other";f.native.authored=authored;f.reject();
    f=Fixture{};assert(f.evaluate());
    authored=std::make_shared<AuthoredPoseData>(*f.native.authored);
    authored->nodes[0].parent=UINT32_MAX;f.native.authored=authored;f.reject();
    f=Fixture{};assert(f.evaluate());
    authored=std::make_shared<AuthoredPoseData>(*f.native.authored);
    authored->nodes[0].kind=2;f.native.authored=authored;f.reject();
    f=Fixture{};assert(f.evaluate());
    authored=std::make_shared<AuthoredPoseData>(*f.native.authored);
    authored->nodes[0].group_count=2;f.native.authored=authored;f.reject();
    f=Fixture{};assert(f.evaluate());
    f.edit_data([](ModelNodeTemplateData& d){d.nodes.pop_back();});f.reject();
    f=Fixture{};assert(f.evaluate());
    f.edit_data([](ModelNodeTemplateData& d){d.nodes[0].name="other";});f.reject();
    f=Fixture{};assert(f.evaluate());
    f.edit_data([](ModelNodeTemplateData& d){d.nodes[0].parent=0;});f.reject();
    f=Fixture{};assert(f.evaluate());
    f.edit_data([](ModelNodeTemplateData& d){d.nodes[0].first_child=99;});f.reject();
    f=Fixture{};assert(f.evaluate());
    f.edit_data([](ModelNodeTemplateData& d){d.nodes[0].local[0]=NAN;});f.reject();
    f=Fixture{};assert(f.evaluate());
    f.edit_data([](ModelNodeTemplateData& d){d.nodes[0].scale=NAN;});f.reject();
}
