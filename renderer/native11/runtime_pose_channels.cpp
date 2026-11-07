#include "runtime_pose_channels.hpp"
#include <cmath>
#include <cstring>
#include <set>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::uint32_t max_channels=256;
constexpr std::size_t max_floats=1048576;
bool fail(std::string& error,const char* message) noexcept {
    try{error=message;}catch(...){error.clear();}return false;
}
bool at(void* context,Read read,std::uint32_t base,std::uint32_t offset,
        void* output,std::size_t size) noexcept {
    return read&&base&&size&&offset<=UINT32_MAX-base&&
        size-1<=UINT32_MAX-base-offset&&read(context,base+offset,output,size);
}
template<class T> bool field(void* context,Read read,std::uint32_t base,
                             std::uint32_t offset,T& output) noexcept {
    return at(context,read,base,offset,&output,sizeof(output));
}
struct VectorHeader {std::uint32_t begin=0,end=0,cap=0;};
bool vector_header(void* context,Read read,std::uint32_t base,std::uint32_t offset,
                   VectorHeader& out) noexcept {
    if(!field(context,read,base,offset,out.begin)||
       !field(context,read,base,offset+4,out.end)||
       !field(context,read,base,offset+8,out.cap))return false;
    return out.begin<=out.end&&out.end<=out.cap&&
           ((out.begin|out.end|out.cap)&3u)==0&&
           (out.begin!=0||(out.end==0&&out.cap==0));
}
bool name(void* context,Read read,std::uint32_t pointer,std::string& result) {
    if(!pointer)return false;
    for(std::uint32_t i=0;i<=259;++i){
        std::uint8_t c=0;
        if(!field(context,read,pointer,i,c))return false;
        if(c==0)return !result.empty();
        if(c<0x20||c>0x7e||i==259)return false;
        result.push_back(static_cast<char>(c));
    }
    return false;
}
bool equal_name(const std::string& a,const std::string& b) noexcept {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i){
        const auto x=static_cast<unsigned char>(a[i]),y=static_cast<unsigned char>(b[i]);
        const auto lower=[](unsigned char c){return c>='A'&&c<='Z'?c+32:c;};
        if(lower(x)!=lower(y))return false;
    }
    return true;
}
bool same_float(float a,float b) noexcept {return std::memcmp(&a,&b,sizeof(a))==0;}
bool same_vector(const std::vector<float>& a,const std::vector<float>& b) noexcept {
    return a.size()==b.size()&&
        (a.empty()||std::memcmp(a.data(),b.data(),a.size()*sizeof(float))==0);
}
bool same_clock(const InstanceAnimationTime& a,const InstanceAnimationTime& b) noexcept {
    return a.engine==b.engine&&a.instance==b.instance&&a.database==b.database&&
        a.flags==b.flags&&a.animating==b.animating&&
        same_float(a.engine_time,b.engine_time)&&same_float(a.start_time,b.start_time)&&
        same_float(a.elapsed,b.elapsed);
}
struct Channel {
    std::uint32_t list_node=0,next=0,prev=0,address=0,vtable=0,play=0;
    std::uint32_t owner=0,name_ptr=0,backlink=0;
    std::uint8_t runtime_type=0;
    VectorHeader timestamps,keys;
    std::uint32_t target=0,target_owner=0,target_name_ptr=0,target_offset=0;
    std::uint32_t interpolation=0,key_count=0,key_vector_flag=0;
    float duration=0,offset=0;
    std::string channel_name,target_name;
    std::vector<float> times,values;
    std::size_t authored_index=0;
};
struct Pass {
    std::uint32_t sentinel=0,count=0,head=0,tail=0;
    std::vector<Channel> channels;
};
bool same_header(const VectorHeader& a,const VectorHeader& b) noexcept {
    return a.begin==b.begin&&a.end==b.end&&a.cap==b.cap;
}
bool same_channel(const Channel& a,const Channel& b) noexcept {
    return a.list_node==b.list_node&&a.next==b.next&&a.prev==b.prev&&
        a.address==b.address&&a.vtable==b.vtable&&a.play==b.play&&
        a.owner==b.owner&&a.name_ptr==b.name_ptr&&a.backlink==b.backlink&&
        a.runtime_type==b.runtime_type&&same_header(a.timestamps,b.timestamps)&&
        same_header(a.keys,b.keys)&&a.target==b.target&&a.target_owner==b.target_owner&&
        a.target_name_ptr==b.target_name_ptr&&a.target_offset==b.target_offset&&
        a.interpolation==b.interpolation&&a.key_count==b.key_count&&
        a.key_vector_flag==b.key_vector_flag&&
        same_float(a.duration,b.duration)&&same_float(a.offset,b.offset)&&
        a.channel_name==b.channel_name&&a.target_name==b.target_name&&
        same_vector(a.times,b.times)&&same_vector(a.values,b.values)&&
        a.authored_index==b.authored_index;
}
bool same_pass(const Pass& a,const Pass& b) noexcept {
    if(a.sentinel!=b.sentinel||a.count!=b.count||a.head!=b.head||
       a.tail!=b.tail||a.channels.size()!=b.channels.size())return false;
    for(std::size_t i=0;i<a.channels.size();++i)
        if(!same_channel(a.channels[i],b.channels[i]))return false;
    return true;
}
bool read_pass(const Object& object,const ModelPoseSnapshot& authored,
    const RuntimePoseReadProfile& profile,void* context,Read read,Pass& pass) {
    if(!field(context,read,object.database,0x6c,pass.sentinel)||!pass.sentinel||
       !field(context,read,object.database,0x70,pass.count)||
       pass.count>max_channels||pass.count!=authored.pose->channels.size()||
       !field(context,read,pass.sentinel,0,pass.head)||!pass.head||
       !field(context,read,pass.sentinel,4,pass.tail)||!pass.tail)return false;
    std::set<std::uint32_t> list_nodes,channel_addresses;
    std::set<std::uint64_t> targets;
    std::vector<bool> matched(authored.pose->channels.size());
    std::uint32_t current=pass.head,previous=pass.sentinel;
    std::size_t aggregate=0;
    while(current!=pass.sentinel){
        if(!current||pass.channels.size()>=max_channels||
           !list_nodes.insert(current).second)return false;
        Channel item;item.list_node=current;
        if(!field(context,read,current,0,item.next)||!item.next||
           !field(context,read,current,4,item.prev)||item.prev!=previous||
           !field(context,read,current,8,item.address)||!item.address||
           !channel_addresses.insert(item.address).second||
           !field(context,read,item.address,0,item.vtable)||
           !field(context,read,item.vtable,0x10,item.play)||
           !field(context,read,item.address,4,item.owner)||item.owner!=object.database||
           !field(context,read,item.address,8,item.name_ptr)||
           !name(context,read,item.name_ptr,item.channel_name)||
           !field(context,read,item.address,0xc,item.backlink)||item.backlink!=current||
           !field(context,read,item.address,0x18,item.runtime_type)||
           !field(context,read,item.address,0x2c,item.duration)||
           !field(context,read,item.address,0x34,item.key_count)||
           !field(context,read,item.address,0x38,item.offset)||
           !field(context,read,item.address,0x3c,item.target)||!item.target||
           !field(context,read,item.address,0x40,item.target_offset)||
           !field(context,read,item.address,0x44,item.interpolation)||
           !field(context,read,item.address,0x48,item.key_vector_flag)||
           !vector_header(context,read,item.address,0x20,item.timestamps)||
           !vector_header(context,read,item.address,0x4c,item.keys)||
           !field(context,read,item.target,4,item.target_owner)||
           !field(context,read,item.target,8,item.target_name_ptr)||
           !name(context,read,item.target_name_ptr,item.target_name))return false;
        const bool scalar=item.runtime_type==5;
        if((item.runtime_type!=0&&!scalar)||
           item.vtable!=(scalar?profile.scalar_vtable:profile.matrix_vtable)||
           item.play!=(scalar?profile.scalar_play:profile.matrix_play)||
           item.target_owner!=object.database||
           !equal_name(item.channel_name,item.target_name)||
           item.target_offset!=(scalar?0x54u:0x24u)||
           !std::isfinite(item.duration)||item.duration<=0||
           !std::isfinite(item.offset)||item.offset<0||
           item.interpolation>3||item.key_count==0||item.key_count>65535)return false;
        std::size_t node_index=authored.pose->nodes.size();
        for(std::size_t n=0;n<authored.pose->nodes.size();++n)
            if(equal_name(item.channel_name,authored.pose->nodes[n].name)){
                if(node_index!=authored.pose->nodes.size())return false;
                node_index=n;
            }
        if(node_index==authored.pose->nodes.size())return false;
        const auto target_key=(std::uint64_t(node_index)<<1)|(scalar?1u:0u);
        if(!targets.insert(target_key).second)return false;
        std::size_t channel_index=authored.pose->channels.size();
        for(std::size_t n=0;n<authored.pose->channels.size();++n){
            const auto& candidate=authored.pose->channels[n];
            if(candidate.node_index==node_index&&
               candidate.key_width==(scalar?1u:12u)&&
               (candidate.channel_type==5)==scalar){
                if(channel_index!=authored.pose->channels.size())return false;
                channel_index=n;
            }
        }
        if(channel_index==authored.pose->channels.size()||matched[channel_index])return false;
        matched[channel_index]=true;item.authored_index=channel_index;
        const std::size_t timestamp_count=(item.timestamps.end-item.timestamps.begin)/4;
        const std::size_t key_float_count=(item.keys.end-item.keys.begin)/4;
        const std::size_t expected=std::size_t(item.key_count)*(scalar?1u:12u)+
            (item.interpolation==3?(scalar?1u:12u):0u);
        if((timestamp_count!=0&&timestamp_count!=item.key_count)||
           key_float_count!=expected||
           timestamp_count>max_floats-aggregate||
           key_float_count>max_floats-aggregate-timestamp_count)return false;
        aggregate+=timestamp_count+key_float_count;
        item.times.resize(timestamp_count);
        item.values.resize(key_float_count);
        if((timestamp_count&&!at(context,read,item.timestamps.begin,0,item.times.data(),
                                  timestamp_count*4))||
           (key_float_count&&!at(context,read,item.keys.begin,0,item.values.data(),
                                  key_float_count*4)))return false;
        for(std::size_t n=0;n<item.times.size();++n)
            if(!std::isfinite(item.times[n])||item.times[n]<0||
               (n==0?item.times[n]!=0:item.times[n]<=item.times[n-1])||
               item.times[n]>=item.duration)return false;
        for(float value:item.values)if(!std::isfinite(value))return false;
        pass.channels.push_back(std::move(item));
        previous=current;current=pass.channels.back().next;
    }
    if(pass.channels.size()!=pass.count||previous!=pass.tail||
       (pass.count==0&&(pass.head!=pass.sentinel||pass.tail!=pass.sentinel))||
       (pass.count!=0&&(pass.head==pass.sentinel||pass.tail==pass.sentinel)))return false;
    for(bool found:matched)if(!found)return false;
    return true;
}
} // namespace

bool capture_runtime_pose_channels(const Object& object,const ModelPoseSnapshot& authored,
    RuntimePoseReadProfile profile,void* context,Read read,RuntimePoseChannels& output,
    std::string& error) noexcept {
    try {
        if(!read||!authored.model||!authored.pose||!object.instance||!object.database||
           !object.model_serial||authored.pose->nodes.size()>4096||
           authored.pose->channels.size()>256||authored.model->info.database!=object.database||
           authored.model->info.serial!=object.model_serial||
           authored.pose->reference_count||
           !profile.scalar_vtable||!profile.matrix_vtable||
           profile.scalar_vtable==profile.matrix_vtable||
           !profile.scalar_play||!profile.matrix_play)
            return fail(error,"invalid runtime pose identity, references or profile");
        InstanceAnimationTime before,after;
        if(!capture_instance_animation_time(object,profile.clock,context,read,before,error))return false;
        Pass first,second;
        if(!read_pass(object,authored,profile,context,read,first)||
           !read_pass(object,authored,profile,context,read,second))
            return fail(error,"invalid or unreadable runtime pose channels");
        if(!capture_instance_animation_time(object,profile.clock,context,read,after,error))return false;
        if(!same_clock(before,after)||!same_pass(first,second))
            return fail(error,"runtime pose changed during capture");
        auto owned=std::make_shared<AuthoredPoseData>(*authored.pose);
        RuntimePoseChannels candidate;
        candidate.model={authored.model,owned};
        candidate.clock=before;
        candidate.playback.resize(owned->channels.size());
        for(const auto& observed:first.channels){
            auto& channel=owned->channels[observed.authored_index];
            auto& playback=candidate.playback[observed.authored_index];
            channel.period=observed.duration;
            channel.keys=observed.values;
            playback.offset=observed.offset;
            playback.interpolation=observed.interpolation;
            playback.timestamps=observed.times;
            playback.has_guard=observed.runtime_type==5&&observed.interpolation==3;
            if(playback.has_guard){
                playback.scalar_guard=channel.keys.back();
                channel.keys.pop_back();
            }else if(observed.runtime_type==0&&observed.interpolation==3){
                // Native Matrix34 SetInterpolation reserves an extra key in
                // mode 3, although EvaluateAndPlay always copies a step key.
                // Its bytes participated in both freshness passes above.
                channel.keys.resize(std::size_t(observed.key_count)*12);
            }
        }
        output=std::move(candidate);error.clear();return true;
    }catch(...){return fail(error,"runtime pose capture allocation or reader failure");}
}
} // namespace a2fo::native11
