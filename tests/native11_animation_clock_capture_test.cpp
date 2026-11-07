#include "../renderer/native11/animation_clock_capture.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace a2fo::native11;
namespace {
void need(bool ok,const char* why) {if(!ok){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Memory {
    std::map<std::uint32_t,std::uint8_t> bytes;
    int engine_reads=0;
    bool change_elapsed=false,unreadable=false;
    std::map<std::uint32_t,std::uint32_t> synthetic_times;
    template<typename T> void put(std::uint32_t address,T value) {
        const auto* source=reinterpret_cast<const std::uint8_t*>(&value);
        for(std::size_t i=0;i<sizeof(value);++i)bytes[address+i]=source[i];
    }
    static bool read(void* context,std::uint32_t address,void* output,std::size_t length) {
        auto& m=*static_cast<Memory*>(context);
        auto timestamps=m.synthetic_times.find(address);
        if(timestamps!=m.synthetic_times.end()&&
           length==std::size_t(timestamps->second)*sizeof(float)) {
            auto* target=static_cast<float*>(output);
            for(std::uint32_t i=0;i<timestamps->second;++i)target[i]=static_cast<float>(i);
            return true;
        }
        if(address==0x1000) {
            ++m.engine_reads;
            if(m.engine_reads==2&&m.change_elapsed)m.put(0x1010,8.0f);
            if(m.engine_reads==2&&m.unreadable)return false;
        }
        auto* target=static_cast<std::uint8_t*>(output);
        for(std::size_t i=0;i<length;++i) {
            auto found=m.bytes.find(address+static_cast<std::uint32_t>(i));
            if(found==m.bytes.end())return false;
            target[i]=found->second;
        }
        return true;
    }
};
constexpr AnimationClockProfile p{0x1000,0x1010,0x7000,0x7100,0x7200,0x7300,0x7400,0x7500};
Object object() {Object o;o.instance=0x3000;o.database=0x4000;o.instance_vtable=p.instance_vtable;o.database_vtable=p.database_vtable;return o;}
Memory fixture(bool two=true) {
    Memory m;
    m.put(0x1000,std::uint32_t(0x2000));m.put(0x1010,0.25f);
    m.put(0x2100,std::uint32_t(0x3000));
    m.put(0x3000,p.instance_vtable);m.put(0x300c,std::uint32_t(1));
    m.put(0x3076,std::uint8_t(1));m.put(0x3080,std::uint32_t(0x4000));
    m.put(0x4000,p.database_vtable);m.put(0x406c,std::uint32_t(0x5000));
    m.put(0x4070,std::uint32_t(two?2:1));
    m.put(0x5000,std::uint32_t(0x5100));m.put(0x5004,std::uint32_t(two?0x5200:0x5100));
    m.put(0x5100,std::uint32_t(two?0x5200:0x5000));m.put(0x5104,std::uint32_t(0x5000));
    m.put(0x5108,std::uint32_t(0x6000));
    if(two) {m.put(0x5200,std::uint32_t(0x5000));m.put(0x5204,std::uint32_t(0x5100));m.put(0x5208,std::uint32_t(0x6100));}
    m.put(0x7210,p.float_play);m.put(0x7310,p.matrix_play);
    auto channel=[&](std::uint32_t addr,std::uint32_t node,std::uint32_t vt,std::uint32_t times) {
        m.put(addr,vt);m.put(addr+4,std::uint32_t(0x4000));m.put(addr+0xc,node);
        m.put(addr+0x20,times);m.put(addr+0x24,times?times+8:0u);
        m.put(addr+0x28,times?times+12:0u);
        m.put(addr+0x2c,1.0f);m.put(addr+0x30,std::uint32_t(0));
        m.put(addr+0x34,std::uint32_t(2));m.put(addr+0x38,0.0f);
        if(times){m.put(times,0.0f);m.put(times+4,0.5f);}
    };
    channel(0x6000,0x5100,p.float_channel_vtable,0x8000);
    if(two)channel(0x6100,0x5200,p.matrix_channel_vtable,0);
    return m;
}
void reject(Memory m,const char* name,AnimationClockProfile profile=p,Object obj=object()) {
    AnimationClockSnapshot out;out.instance=0xdeadbeef;out.channels.resize(1);
    std::string error;
    need(!capture_animation_clocks(obj,profile,&m,Memory::read,out,error),name);
    need(out.instance==0xdeadbeef&&out.channels.size()==1,"failure preserves output");
    need(!error.empty(),"failure reports reason");
}
void cases() {
    auto m=fixture();AnimationClockSnapshot out;std::string error="stale";
    need(capture_animation_clocks(object(),p,&m,Memory::read,out,error),"two channel capture");
    need(error.empty()&&out.channels.size()==2&&out.animating&&out.elapsed==0.25f,
         "owned two channel snapshot");
    need(out.channels[0].identity==0x6000&&out.channels[1].identity==0x6100&&
         out.channels[0].timing.timestamps.size()==2&&out.channels[1].timing.timestamps.empty()&&
         out.channels[0].evaluated.key==0&&out.channels[1].evaluated.key==0,
         "float and matrix channel phases");
    m=fixture(false);m.put(0x3076,std::uint8_t(0));m.put(0x1010,9.0f);
    need(capture_animation_clocks(object(),p,&m,Memory::read,out,error)&&
         !out.animating&&out.elapsed==0&&out.channels[0].timing.elapsed==0,
         "paused phase uses zero elapsed");
    auto invalid=p;invalid.float_play=0;reject(fixture(),"missing profile",invalid);
    m=fixture();m.put(0x6000,std::uint32_t(0x1234));reject(m,"channel vtable");
    m=fixture();m.put(0x7210,std::uint32_t(0x1234));reject(m,"play target");
    m=fixture();m.put(0x6004,std::uint32_t(0x1234));reject(m,"channel owner");
    m=fixture();m.put(0x600c,std::uint32_t(0x1234));reject(m,"channel backlink");
    m=fixture();m.put(0x3000,std::uint32_t(0x1234));reject(m,"instance vtable");
    m=fixture();m.put(0x4000,std::uint32_t(0x1234));reject(m,"database vtable");
    m=fixture();m.put(0x5204,std::uint32_t(0x5000));reject(m,"list reciprocal link");
    m=fixture();m.put(0x5004,std::uint32_t(0x5100));reject(m,"list tail");
    m=fixture();m.put(0x5208,std::uint32_t(0x6000));reject(m,"duplicate payload");
    m=fixture();m.put(0x5100,std::uint32_t(0x5100));reject(m,"list cycle");
    m=fixture();m.put(0x4070,std::uint32_t(1));reject(m,"list count");
    m=fixture();m.put(0x6024,std::uint32_t(0x8004));reject(m,"timestamp count");
    m=fixture();m.put(0x6028,std::uint32_t(0x8004));reject(m,"timestamp capacity");
    m=fixture();m.put(0x6020,std::uint32_t(0xfffffffc));
    m.put(0x6024,std::uint32_t(0xfffffffc));m.put(0x6028,std::uint32_t(0xfffffffc));
    // An empty vector is valid even at the address-space end.
    need(capture_animation_clocks(object(),p,&m,Memory::read,out,error),"empty timestamp end pointer");
    m=fixture();m.put(0x6024,std::uint32_t(0x8020));reject(m,"timestamp budget/count");
    m=fixture();m.put(0x8004,0.0f);reject(m,"timestamp ordering");
    m=fixture();m.put(0x6030,std::uint32_t(1));reject(m,"native key mismatch");
    m=fixture();m.change_elapsed=true;reject(m,"mutation between passes");
    m=fixture(false);m.put(0x3076,std::uint8_t(0));m.change_elapsed=true;
    reject(m,"paused raw elapsed mutation");
    m=fixture(false);m.put(0x3076,std::uint8_t(0));
    m.put(0x1010,std::numeric_limits<float>::quiet_NaN());
    reject(m,"paused NaN raw elapsed");
    m=fixture();m.unreadable=true;reject(m,"unreadable second pass");
    m=fixture();m.put(0x4070,std::uint32_t(257));reject(m,"channel budget");
    m=fixture();m.put(0x4070,std::uint32_t(0));
    m.put(0x5000,std::uint32_t(0x5000));m.put(0x5004,std::uint32_t(0x5000));
    need(capture_animation_clocks(object(),p,&m,Memory::read,out,error)&&out.channels.empty(),
         "empty self-linked list");
    m.put(0x1010,-1.0f);reject(m,"empty list negative raw elapsed");
}
void timestamp_budget_boundary() {
    auto m=fixture();
    m.put(0x4070,std::uint32_t(5));
    m.put(0x5004,std::uint32_t(0x5500));
    for(std::uint32_t i=0;i<5;++i) {
        const auto node=0x5100u+i*0x100u;
        const auto channel=0x6000u+i*0x100u;
        const auto begin=0x100000u+i*0x100000u;
        const std::uint32_t count=i==4?4u:65535u;
        m.put(node,i==4?0x5000u:node+0x100u);
        m.put(node+4,i==0?0x5000u:node-0x100u);
        m.put(node+8,channel);
        m.put(channel,i&1?p.matrix_channel_vtable:p.float_channel_vtable);
        m.put(channel+4,std::uint32_t(0x4000));m.put(channel+0xc,node);
        m.put(channel+0x20,begin);m.put(channel+0x24,begin+count*4);
        m.put(channel+0x28,begin+count*4);
        m.put(channel+0x2c,65536.0f);m.put(channel+0x30,std::uint32_t(0));
        m.put(channel+0x34,count);m.put(channel+0x38,0.0f);
        m.synthetic_times[begin]=count;
    }
    AnimationClockSnapshot output;std::string error;
    need(capture_animation_clocks(object(),p,&m,Memory::read,output,error)&&
         output.channels.size()==5,"exact 1 MiB timestamp budget accepted");
    m.synthetic_times[0x500000]=5;
    m.put(0x6424,std::uint32_t(0x500014));
    m.put(0x6428,std::uint32_t(0x500014));
    m.put(0x6434,std::uint32_t(5));
    reject(m,"1 MiB plus four bytes rejected");
}
}
int main() {cases();timestamp_budget_boundary();std::puts("PASS animation clock capture fake-memory cases");}
