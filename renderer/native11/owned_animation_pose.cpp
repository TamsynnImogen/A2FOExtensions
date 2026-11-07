#include "owned_animation_pose.hpp"
#include "model_node_template.hpp"
#include <cfloat>
#include <cmath>
#include <set>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_nodes=4096,max_channels=256,max_key_floats=1048576,max_depth=256;
bool fail(std::string& error,const char* message) noexcept {
    try {error=message;}catch(...){error.clear();}return false;
}
bool printable(const std::string& name) noexcept {
    if(name.empty()||name.size()>259)return false;
    for(unsigned char c:name)if(c<32||c>126)return false;
    return true;
}
bool compose(const std::array<float,16>& parent,const std::array<float,12>& raw,
             float scale,std::array<float,16>& out) noexcept {
    float local[16]{};
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)local[r*4+c]=raw[c*3+r]*scale;
    for(unsigned r=0;r<3;++r)local[r*4+3]=raw[9+r];
    local[15]=1;
    for(float value:local)if(!std::isfinite(value))return false;
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){
        double sum=0;
        for(unsigned k=0;k<4;++k)sum+=double(parent[r*4+k])*local[k*4+c];
        if(!std::isfinite(sum)||sum>FLT_MAX||sum< -FLT_MAX)return false;
        out[r*4+c]=static_cast<float>(sum);
    }
    return true;
}
bool visit(std::size_t index,const AuthoredPoseData& pose,OwnedAnimationPose& result,
           std::vector<std::uint8_t>& state,std::size_t depth) noexcept {
    if(depth>max_depth||state[index]==1)return false;
    if(state[index]==2)return true;
    state[index]=1;
    const auto parent=pose.nodes[index].parent;
    std::array<float,16> basis{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    if(parent!=UINT32_MAX){
        if(parent>=pose.nodes.size()||!visit(parent,pose,result,state,depth+1))return false;
        basis=result.local_to_model[parent];
    }
    if(!compose(basis,result.local[index],result.scales[index],result.local_to_model[index]))return false;
    state[index]=2;return true;
}
}
static bool evaluate_impl(const ModelPoseSnapshot& snapshot,
    const InstanceAnimationTime& clock,const std::vector<PoseChannelPlayback>& playback,
    const ModelNodeTemplate* seed,OwnedAnimationPose& output,std::string& error) noexcept {
    try {
        if(!snapshot.model||!snapshot.pose||!snapshot.model->info.database||
           !snapshot.model->info.serial||!clock.instance||!clock.database||
           snapshot.model->info.database!=clock.database||
           !std::isfinite(clock.engine_time)||clock.engine_time<0||
           !std::isfinite(clock.start_time)||clock.start_time<0||
           !std::isfinite(clock.elapsed)||clock.elapsed<0||
           snapshot.pose->reference_count||snapshot.pose->nodes.size()>max_nodes||
           snapshot.pose->channels.size()>max_channels||
           playback.size()!=snapshot.pose->channels.size())
            return fail(error,"invalid owned pose identity, time, references or count");
        const auto& pose=*snapshot.pose;
        if(seed){
            if(seed->model!=snapshot.model||!seed->authored||!seed->data||
               seed->authored->nodes.size()!=pose.nodes.size()||
               seed->data->nodes.size()!=pose.nodes.size())
                return fail(error,"mismatched native pose template owner or count");
            for(std::size_t i=0;i<pose.nodes.size();++i){
                const auto& authored=seed->authored->nodes[i];
                const auto& runtime=pose.nodes[i];
                const auto& native=seed->data->nodes[i];
                if(authored.name!=runtime.name||authored.parent!=runtime.parent||
                   authored.kind!=runtime.kind||authored.group_count!=runtime.group_count||
                   native.name!=runtime.name||native.parent!=runtime.parent||
                   (native.first_child!=UINT32_MAX&&native.first_child>=pose.nodes.size())||
                   (native.next_sibling!=UINT32_MAX&&native.next_sibling>=pose.nodes.size())||
                   !std::isfinite(native.scale))
                    return fail(error,"mismatched native pose template metadata");
                for(float value:native.local)
                    if(!std::isfinite(value))return fail(error,"nonfinite native pose baseline");
            }
        }
        OwnedAnimationPose candidate;
        candidate.instance=clock.instance;candidate.database=clock.database;
        candidate.model_serial=snapshot.model->info.serial;
        candidate.local.reserve(pose.nodes.size());
        candidate.scales.assign(pose.nodes.size(),1.0f);
        candidate.local_to_model.resize(pose.nodes.size());
        for(std::size_t i=0;i<pose.nodes.size();++i){
            const auto& node=pose.nodes[i];
            if(!printable(node.name)||
               (node.kind!=0&&node.kind!=1&&node.kind!=2&&node.kind!=3&&node.kind!=12)||
               (node.parent!=UINT32_MAX&&node.parent>=pose.nodes.size()))
                return fail(error,"invalid authored pose node metadata");
            for(float v:node.local)if(!std::isfinite(v))return fail(error,"nonfinite authored node matrix");
            candidate.local.push_back(seed?seed->data->nodes[i].local:node.local);
            if(seed)candidate.scales[i]=seed->data->nodes[i].scale;
        }
        // Count the actual ancestry independently of traversal/cache order.
        // A parent-first table otherwise keeps recursive stack depth small
        // even when the hierarchy itself exceeds the supported depth.
        for(std::size_t i=0;i<pose.nodes.size();++i){
            auto parent=pose.nodes[i].parent;
            std::size_t depth=1;
            while(parent!=UINT32_MAX){
                if(parent>=pose.nodes.size()||parent==i||++depth>max_depth)
                    return fail(error,"authored pose hierarchy cycle or depth limit");
                parent=pose.nodes[parent].parent;
            }
        }
        std::set<std::uint64_t> targets;
        std::size_t key_floats=0;
        for(std::size_t i=0;i<pose.channels.size();++i){
            const auto& channel=pose.channels[i];const auto& play=playback[i];
            const bool scalar=channel.channel_type==5;
            const std::size_t width=scalar?1:12;
            if(channel.node_index>=pose.nodes.size()||channel.key_width!=width||
               channel.keys.empty()||channel.keys.size()%width||
               channel.keys.size()/width>65535||channel.keys.size()>max_key_floats-key_floats||
               !std::isfinite(channel.period)||channel.period<=0||play.interpolation>3||
               !std::isfinite(play.offset)||play.offset<0||
               (scalar ? play.has_guard!=(play.interpolation==3) : play.has_guard)||
               (play.has_guard&&!std::isfinite(play.scalar_guard))||
               (!play.timestamps.empty()&&play.timestamps.size()!=channel.keys.size()/width))
                return fail(error,"invalid authored channel or playback shape");
            key_floats+=channel.keys.size();
            for(float v:channel.keys)if(!std::isfinite(v))return fail(error,"nonfinite authored key");
            const auto target=(std::uint64_t(channel.node_index)<<1)|(scalar?1u:0u);
            if(!targets.insert(target).second)return fail(error,"duplicate animated node field");
            AnimationPhaseInput timing;
            timing.elapsed=clock.elapsed;timing.offset=play.offset;
            timing.duration=channel.period;timing.flags=clock.flags;
            timing.key_count=static_cast<std::uint32_t>(channel.keys.size()/width);
            timing.timestamps=play.timestamps;
            if(scalar){
                ScalarAnimationChannel evaluation;
                evaluation.timing=std::move(timing);evaluation.interpolation=play.interpolation;
                evaluation.keys=channel.keys;
                if(play.has_guard)evaluation.keys.push_back(play.scalar_guard);
                ScalarAnimationSample sample;
                if(!evaluate_scalar_animation_channel(evaluation,sample,error))return false;
                if(!sample.writes)return fail(error,"unwritten scalar channel target unknown");
                candidate.scales[channel.node_index]=sample.value;
            }else{
                MatrixAnimationChannel evaluation;
                evaluation.timing=std::move(timing);evaluation.interpolation=play.interpolation;
                evaluation.keys.reserve(channel.keys.size()/12);
                for(std::size_t k=0;k<channel.keys.size();k+=12){
                    std::array<float,12> matrix{};
                    for(unsigned e=0;e<12;++e)matrix[e]=channel.keys[k+e];
                    evaluation.keys.push_back(matrix);
                }
                MatrixAnimationSample sample;
                if(!evaluate_matrix_animation_channel(evaluation,sample,error))return false;
                if(!sample.writes)return fail(error,"unwritten matrix channel target unknown");
                candidate.local[channel.node_index]=sample.value;
            }
        }
        std::vector<std::uint8_t> state(pose.nodes.size());
        for(std::size_t i=0;i<pose.nodes.size();++i)
            if(!visit(i,pose,candidate,state,1))return fail(error,"pose hierarchy cycle, depth or composition failure");
        output=std::move(candidate);error.clear();return true;
    }catch(...){return fail(error,"owned pose evaluation allocation failed");}
}
bool evaluate_owned_animation_pose(const ModelPoseSnapshot& snapshot,
    const InstanceAnimationTime& clock,const std::vector<PoseChannelPlayback>& playback,
    OwnedAnimationPose& output,std::string& error) noexcept {
    return evaluate_impl(snapshot,clock,playback,nullptr,output,error);
}
bool evaluate_owned_animation_pose_from_template(const ModelPoseSnapshot& snapshot,
    const InstanceAnimationTime& clock,const std::vector<PoseChannelPlayback>& playback,
    const ModelNodeTemplate& seed,OwnedAnimationPose& output,std::string& error) noexcept {
    return evaluate_impl(snapshot,clock,playback,&seed,output,error);
}
} // namespace a2fo::native11
