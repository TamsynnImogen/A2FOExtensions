#include "../renderer/native11/owned_animation_pose.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
bool near(float a,float b){return std::fabs(a-b)<0.00001f;}
std::array<float,12> matrix(float basis,float x){
    return {basis,0,0,0,basis,0,0,0,basis,x,0,0};
}
struct Fixture {
    std::shared_ptr<ModelAsset> model=std::make_shared<ModelAsset>();
    std::shared_ptr<AuthoredPoseData> pose=std::make_shared<AuthoredPoseData>();
    ModelPoseSnapshot snapshot;
    InstanceAnimationTime clock{};
    std::vector<PoseChannelPlayback> playback;
    Fixture(){
        model->info.database=7;model->info.serial=11;
        snapshot={model,pose};
        clock.engine=3;clock.instance=101;clock.database=7;clock.flags=1;
        clock.animating=true;clock.engine_time=1;clock.start_time=.25f;clock.elapsed=.75f;
        AuthoredPoseNode child;child.name="child";child.parent=1;child.kind=1;child.local=matrix(1,3);
        AuthoredPoseNode parent;parent.name="parent";parent.parent=UINT32_MAX;
        parent.local=matrix(1,0);pose->nodes={child,parent};
        AuthoredPoseChannel scalar;scalar.node_index=0;scalar.channel_type=5;
        scalar.period=1;scalar.key_width=1;scalar.keys={1,2};
        AuthoredPoseChannel transform;transform.node_index=1;transform.channel_type=1;
        transform.period=1;transform.key_width=12;
        const auto first=matrix(1,0),second=matrix(2,10);
        transform.keys.insert(transform.keys.end(),first.begin(),first.end());
        transform.keys.insert(transform.keys.end(),second.begin(),second.end());
        pose->channels={scalar,transform};
        PoseChannelPlayback scale;scale.interpolation=2;scale.timestamps={0,.5f};
        PoseChannelPlayback mat;mat.interpolation=3;mat.timestamps={0,.5f};
        playback={scale,mat};
    }
};
void reject(Fixture& f,const char* why){
    OwnedAnimationPose out;out.instance=999;out.database=888;out.model_serial=777;
    out.local.push_back(matrix(4,5));out.scales.push_back(6);
    std::string error;
    need(!evaluate_owned_animation_pose(f.snapshot,f.clock,f.playback,out,error),why);
    need(out.instance==999&&out.database==888&&out.model_serial==777&&
         out.local.size()==1&&out.local[0][9]==5&&out.scales.size()==1&&out.scales[0]==6&&
         !error.empty(),"failed pose leaves output unchanged");
}
}
int main(){
    Fixture f;OwnedAnimationPose out;std::string error;
    need(evaluate_owned_animation_pose(f.snapshot,f.clock,f.playback,out,error),
         "child-before-parent animated pose");
    need(error.empty()&&out.instance==101&&out.database==7&&out.model_serial==11&&
         out.local.size()==2&&out.scales.size()==2&&out.local_to_model.size()==2,
         "owned pose identity and file-index output");
    need(near(out.scales[0],1.5f)&&near(out.scales[1],1)&&
         near(out.local[0][9],3)&&near(out.local[1][9],10),
         "animated matrix and scalar preserve native locals");
    need(near(out.local_to_model[1][0],2)&&near(out.local_to_model[1][3],10)&&
         near(out.local_to_model[0][0],3)&&near(out.local_to_model[0][3],16),
         "parent inheritance with basis-only scale and unscaled local translation");
    const auto first=out;
    auto other_clock=f.clock;other_clock.instance=202;other_clock.elapsed=.25f;
    need(evaluate_owned_animation_pose(f.snapshot,other_clock,f.playback,out,error)&&
         out.instance==202&&near(out.local_to_model[1][0],1)&&
         near(out.local_to_model[0][3],3)&&near(first.local_to_model[0][3],16),
         "two instances evaluate same authored asset independently");
    f.playback.pop_back();reject(f,"missing playback");
    f=Fixture{};f.playback[0].interpolation=0;reject(f,"unwritten scalar target refused");
    f=Fixture{};f.playback[1].interpolation=0;reject(f,"unwritten matrix target refused");
    f=Fixture{};f.pose->channels.push_back(f.pose->channels[0]);
    f.playback.push_back(f.playback[0]);reject(f,"duplicate scale target refused");
    f=Fixture{};f.pose->channels[0].node_index=1;
    need(evaluate_owned_animation_pose(f.snapshot,f.clock,f.playback,out,error)&&
         near(out.local_to_model[1][0],3)&&near(out.local_to_model[1][3],10)&&
         near(out.local_to_model[0][3],19),
         "matrix and scale channels may write distinct fields on one node");
    f=Fixture{};f.pose->reference_count=1;reject(f,"unsupported reference table refused");
    f=Fixture{};f.pose->nodes[1].local[0]=std::numeric_limits<float>::quiet_NaN();
    reject(f,"nonfinite authored matrix refused");
    f=Fixture{};f.pose->nodes[1].parent=0;reject(f,"hierarchy cycle refused");
    f=Fixture{};f.pose->channels.clear();f.playback.clear();f.pose->nodes.clear();
    for(unsigned i=0;i<257;++i){AuthoredPoseNode node;node.name="n"+std::to_string(i);
        node.parent=i==256?UINT32_MAX:i+1;node.local=matrix(1,0);
        f.pose->nodes.push_back(node);}
    reject(f,"hierarchy depth above 256 refused");
    for(unsigned i=0;i<257;++i)f.pose->nodes[i].parent=i?i-1:UINT32_MAX;
    reject(f,"parent-first hierarchy depth above 256 refused");
    f.pose->nodes.pop_back();
    need(evaluate_owned_animation_pose(f.snapshot,f.clock,f.playback,out,error),
         "parent-first hierarchy exactly 256 accepted");
    f=Fixture{};f.pose->channels[0].key_width=12;reject(f,"wrong scalar key width refused");
    f=Fixture{};f.playback[0].interpolation=3;reject(f,"missing scalar guard refused");
    f=Fixture{};f.playback[0].interpolation=3;f.playback[0].has_guard=true;
    f.playback[0].scalar_guard=4;f.clock.elapsed=.75f;
    need(evaluate_owned_animation_pose(f.snapshot,f.clock,f.playback,out,error)&&
         near(out.scales[0],3),"scalar mode 3 uses explicit guard");
    f=Fixture{};f.playback[1].has_guard=true;reject(f,"matrix guard refused");
    f=Fixture{};f.clock.database=8;reject(f,"model/database owner mismatch");
    f=Fixture{};f.clock.elapsed=std::numeric_limits<float>::infinity();
    reject(f,"nonfinite elapsed refused");
    f=Fixture{};f.pose->channels[1].keys[12]=std::numeric_limits<float>::max();
    f.pose->nodes[0].local[0]=2;reject(f,"composed basis overflow refused");
    f=Fixture{};f.pose->nodes.clear();f.pose->channels.clear();f.playback.clear();
    need(evaluate_owned_animation_pose(f.snapshot,f.clock,f.playback,out,error)&&
         out.local.empty()&&out.local_to_model.empty(),"empty authored pose accepted");
    std::puts("PASS native11 owned animation pose: per-instance transforms, hierarchy and atomic rejection");
}
