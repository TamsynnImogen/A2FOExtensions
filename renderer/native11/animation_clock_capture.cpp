#include "animation_clock_capture.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::uint32_t max_channels=256;
constexpr std::uint32_t max_timestamp_bytes=1024*1024;
struct ObservedChannel {
    std::uint32_t node=0,next=0,prev=0,owner=0,backlink=0;
    std::uint32_t vtable=0,play=0,begin=0,end=0,capacity=0;
    AnimationChannelClock clock;
};
struct Pass {
    std::uint32_t engine=0,instance_vtable=0,database_vtable=0;
    std::uint32_t sentinel=0,count=0,head=0,tail=0;
    std::uint8_t enabled=0;
    float raw_elapsed=0;
    AnimationClockSnapshot snapshot;
    std::vector<ObservedChannel> observed;
};
bool at(void* context,Read read,std::uint32_t base,std::uint32_t offset,
        void* out,std::size_t size) {
    if(!read||!base||!size||offset>UINT32_MAX-base||
       size-1>UINT32_MAX-base-offset)return false;
    return read(context,base+offset,out,size);
}
template<typename T> bool field(void* context,Read read,std::uint32_t base,
                                 std::uint32_t offset,T& out) {
    return at(context,read,base,offset,&out,sizeof(out));
}
bool same_float(float a,float b) {return std::memcmp(&a,&b,sizeof(a))==0;}
bool profile_valid(const AnimationClockProfile& p) {
    return p.engine_pointer&&p.elapsed_pointer&&p.instance_vtable&&p.database_vtable&&
           p.float_channel_vtable&&p.matrix_channel_vtable&&p.float_play&&p.matrix_play&&
           p.float_channel_vtable!=p.matrix_channel_vtable;
}
bool pass(const Object& object,const AnimationClockProfile& profile,void* context,
          Read read,Pass& result,const char*& failure) {
    if(!profile_valid(profile)||!read||!object.instance||!object.database||
       object.instance_vtable!=profile.instance_vtable||
       object.database_vtable!=profile.database_vtable) {
        failure="missing or mismatched animation clock profile/object";return false;
    }
    auto& snapshot=result.snapshot;
    if(!field(context,read,profile.engine_pointer,0,result.engine)||!result.engine||
       !field(context,read,result.engine,0x100,snapshot.instance)||
       snapshot.instance!=object.instance||
       !field(context,read,profile.elapsed_pointer,0,result.raw_elapsed)||
       !std::isfinite(result.raw_elapsed)||result.raw_elapsed<0||
       !field(context,read,snapshot.instance,0,result.instance_vtable)||
       result.instance_vtable!=profile.instance_vtable||
       !field(context,read,snapshot.instance,0x80,snapshot.database)||
       snapshot.database!=object.database||
       !field(context,read,snapshot.instance,0xc,snapshot.flags)||
       !field(context,read,snapshot.instance,0x76,result.enabled)||result.enabled>1||
       !field(context,read,snapshot.database,0,result.database_vtable)||
       result.database_vtable!=profile.database_vtable||
       !field(context,read,snapshot.database,0x6c,result.sentinel)||!result.sentinel||
       !field(context,read,snapshot.database,0x70,result.count)||result.count>max_channels||
       !field(context,read,result.sentinel,0,result.head)||!result.head||
       !field(context,read,result.sentinel,4,result.tail)||!result.tail) {
        failure="unreadable or inconsistent animation clock owner/list";return false;
    }
    snapshot.animating=result.enabled!=0;
    snapshot.elapsed=snapshot.animating?result.raw_elapsed:0.0f;
    std::set<std::uint32_t> nodes,payloads;
    std::uint32_t current=result.head,previous=result.sentinel;
    std::uint32_t aggregate=0;
    while(current!=result.sentinel) {
        if(!current||result.observed.size()==max_channels||!nodes.insert(current).second) {
            failure="animation channel list cycle or capacity exceeded";return false;
        }
        ObservedChannel item{};
        item.node=current;
        auto& channel=item.clock;
        if(!field(context,read,current,0,item.next)||!item.next||
           !field(context,read,current,4,item.prev)||item.prev!=previous||
           !field(context,read,current,8,channel.identity)||!channel.identity||
           !payloads.insert(channel.identity).second||
           !field(context,read,channel.identity,0,item.vtable)||
           (item.vtable!=profile.float_channel_vtable&&item.vtable!=profile.matrix_channel_vtable)||
           !field(context,read,item.vtable,0x10,item.play)||
           item.play!=(item.vtable==profile.float_channel_vtable?profile.float_play:profile.matrix_play)||
           !field(context,read,channel.identity,4,item.owner)||item.owner!=snapshot.database||
           !field(context,read,channel.identity,0xc,item.backlink)||item.backlink!=current||
           !field(context,read,channel.identity,0x30,channel.native_key)||
           !field(context,read,channel.identity,0x34,channel.timing.key_count)||
           channel.timing.key_count>65535||
           !field(context,read,channel.identity,0x2c,channel.timing.duration)||
           !field(context,read,channel.identity,0x38,channel.timing.offset)||
           !field(context,read,channel.identity,0x20,item.begin)||
           !field(context,read,channel.identity,0x24,item.end)||
           !field(context,read,channel.identity,0x28,item.capacity)) {
            failure="invalid animation channel identity or timing";return false;
        }
        if(item.begin>item.end||item.end>item.capacity||
           (item.begin|item.end|item.capacity)&3u||
           (item.end-item.begin)%sizeof(float)||
           (!item.begin&&item.capacity)||
           (item.end!=item.begin&&!item.begin)||
           ((item.end-item.begin)/sizeof(float)!=0&&
            (item.end-item.begin)/sizeof(float)!=channel.timing.key_count)||
           item.end-item.begin>max_timestamp_bytes-aggregate) {
            failure="invalid animation timestamp vector";return false;
        }
        const auto bytes=item.end-item.begin;
        aggregate+=bytes;
        if(bytes) {
            channel.timing.timestamps.resize(bytes/sizeof(float));
            if(!at(context,read,item.begin,0,channel.timing.timestamps.data(),bytes)) {
                failure="unreadable animation timestamps";return false;
            }
        }
        channel.timing.flags=snapshot.flags;
        channel.timing.elapsed=snapshot.elapsed;
        std::string phase_error;
        if(!evaluate_animation_phase(channel.timing,channel.evaluated,phase_error)||
           channel.evaluated.key!=channel.native_key) {
            failure="animation phase or native key mismatch";return false;
        }
        result.observed.push_back(std::move(item));
        previous=current;current=result.observed.back().next;
    }
    if(result.observed.size()!=result.count||previous!=result.tail||
       (result.count==0&&(result.head!=result.sentinel||result.tail!=result.sentinel))||
       (result.count!=0&&(result.head==result.sentinel||result.tail==result.sentinel))) {
        failure="animation channel list count or tail mismatch";return false;
    }
    return true;
}
bool same_channel(const ObservedChannel& a,const ObservedChannel& b) {
    const auto& x=a.clock;const auto& y=b.clock;
    if(a.node!=b.node||a.next!=b.next||a.prev!=b.prev||a.owner!=b.owner||
       a.backlink!=b.backlink||a.vtable!=b.vtable||a.play!=b.play||
       a.begin!=b.begin||a.end!=b.end||a.capacity!=b.capacity||
       x.identity!=y.identity||x.native_key!=y.native_key||
       x.timing.key_count!=y.timing.key_count||x.timing.flags!=y.timing.flags||
       !same_float(x.timing.elapsed,y.timing.elapsed)||
       !same_float(x.timing.offset,y.timing.offset)||
       !same_float(x.timing.duration,y.timing.duration)||
       x.timing.timestamps.size()!=y.timing.timestamps.size()||
       x.evaluated.key!=y.evaluated.key||
       !same_float(x.evaluated.fraction,y.evaluated.fraction)||
       x.evaluated.finished!=y.evaluated.finished)return false;
    return x.timing.timestamps.empty()||
        std::memcmp(x.timing.timestamps.data(),y.timing.timestamps.data(),
                    x.timing.timestamps.size()*sizeof(float))==0;
}
bool same(const Pass& a,const Pass& b) {
    if(a.engine!=b.engine||a.instance_vtable!=b.instance_vtable||
       a.database_vtable!=b.database_vtable||a.sentinel!=b.sentinel||
       a.count!=b.count||a.head!=b.head||a.tail!=b.tail||
       a.enabled!=b.enabled||!same_float(a.raw_elapsed,b.raw_elapsed)||
       a.snapshot.instance!=b.snapshot.instance||a.snapshot.database!=b.snapshot.database||
       a.snapshot.flags!=b.snapshot.flags||a.observed.size()!=b.observed.size())return false;
    for(std::size_t i=0;i<a.observed.size();++i)
        if(!same_channel(a.observed[i],b.observed[i]))return false;
    return true;
}
void fail(std::string& error,const char* message) noexcept {
    try{error=message;}catch(...){error.clear();}
}
}
bool capture_animation_clocks(const Object& object,AnimationClockProfile profile,
                              void* context,Read read,AnimationClockSnapshot& output,
                              std::string& error) noexcept {
    try {
        const char* failure=nullptr;
        Pass first{},second{};
        if(!pass(object,profile,context,read,first,failure)||
           !pass(object,profile,context,read,second,failure)) {
            fail(error,failure);return false;
        }
        if(!same(first,second)) {
            fail(error,"animation clock observation changed during capture");return false;
        }
        first.snapshot.channels.reserve(first.observed.size());
        for(auto& observed:first.observed)
            first.snapshot.channels.push_back(std::move(observed.clock));
        output=std::move(first.snapshot);
        error.clear();return true;
    }catch(...) {
        fail(error,"animation clock allocation or callback failure");return false;
    }
}
} // namespace a2fo::native11
