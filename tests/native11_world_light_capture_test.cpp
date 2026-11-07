#include "../renderer/native11/world_light_capture.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool ok,const char* name) {
    if (!ok) { std::fprintf(stderr,"FAIL %s\n",name);std::exit(1); }
}
struct Memory {
    std::vector<unsigned char> bytes=std::vector<unsigned char>(0xc0000);
    int global_reads=0;
    bool mutate_list=false;
    bool mutate_disabled_instance=false;
    template<class T> void put(std::uint32_t address,const T& value) {
        std::memcpy(bytes.data()+address,&value,sizeof(value));
    }
    void word(std::uint32_t address,std::uint32_t value) { put(address,value); }
    static bool read(void* ctx,std::uint32_t address,void* out,std::size_t size) {
        auto& m=*static_cast<Memory*>(ctx);
        if (address==0x100&&++m.global_reads==2){
            if(m.mutate_list)m.word(0x400+8,0x1200);
            if(m.mutate_disabled_instance)m.word(0x1100+4,0x9000);
        }
        if (address>m.bytes.size()||size>m.bytes.size()-address) return false;
        std::memcpy(out,m.bytes.data()+address,size);
        return true;
    }
};
struct Classes {
    std::map<std::uint64_t,std::shared_ptr<const LightClassRecord>> records;
    int pins=0;
    bool replace_on_third=false;
    static std::shared_ptr<const LightClassRecord> pin(void* ctx,std::uint64_t id) {
        auto& c=*static_cast<Classes*>(ctx);
        if (++c.pins==3&&c.replace_on_third) {
            auto replacement=std::make_shared<LightClassRecord>(*c.records.at(id));
            replacement->version++;
            c.records[id]=replacement;
        }
        const auto it=c.records.find(id);
        return it==c.records.end()?nullptr:it->second;
    }
};
std::shared_ptr<const LightClassRecord> owner(std::uint64_t id,bool enabled) {
    auto p=std::make_shared<LightClassRecord>();
    p->class_id=id;p->config.enabled=enabled;p->source_odf="owned.odf";p->version=1;
    p->config.light.type=LightType::spot;
    return p;
}
void setup(Memory& m) {
    m.word(0x100,0x200);m.word(0x204,0x300);
    m.word(0x300,0x400);m.word(0x304,0x500);
    m.word(0x400,0x500);m.word(0x404,0x300);m.word(0x408,0x1000);
    m.word(0x500,0x300);m.word(0x504,0x400);m.word(0x508,0x1100);
    // Decimal +40 is a decoy; the pinned class field is hexadecimal +0x40.
    m.word(0x1000+40,99);m.word(0x1000+0x40,20);m.word(0x1000+4,0x2000);
    m.word(0x1100+40,99);m.word(0x1100+0x40,21);
    const float matrix[12]{0,1,0,-1,0,0,0,0,1,7,8,9};
    std::memcpy(m.bytes.data()+0x2000+0x44,matrix,sizeof(matrix));
}
void setup_many(Memory& m,std::uint32_t count) {
    m.word(0x100,0x200);m.word(0x204,0x300);
    m.word(0x300,0x4000);
    m.word(0x304,0x4000+16*(count-1));
    for (std::uint32_t i=0;i<count;++i) {
        const std::uint32_t node=0x4000+16*i,object=0x20000+0x80*i;
        m.word(node,i+1==count?0x300:node+16);
        m.word(node+4,i?node-16:0x300);
        m.word(node+8,object);
        m.word(object+40,99);
        m.word(object+0x40,21);
    }
}
void reject(Memory& m,Classes& c,const char* name) {
    WorldLightCapture output{};
    output.sources.push_back(PlacedLight{});output.sources[0].source_id=77;
    output.class_owners.push_back(owner(77,true));
    std::string error="old";
    need(!capture_world_lights({0x100},&m,&Memory::read,&c,&Classes::pin,output,error),name);
    need(output.sources.size()==1&&output.sources[0].source_id==77&&
         output.class_owners.size()==1&&output.class_owners[0]->class_id==77,
         "failure output transactional");
    need(!error.empty()&&error!="old","failure diagnostic");
}
}
int main() {
    Memory m;setup(m);Classes c;
    c.records[20]=owner(20,true);c.records[21]=owner(21,false);
    WorldLightCapture capture{};std::string error;
    need(capture_world_lights({0x100},&m,&Memory::read,&c,&Classes::pin,capture,error)&&
         error.empty()&&capture.sources.size()==2&&capture.class_owners.size()==2,
         "complete world capture includes offscreen objects");
    need(capture.sources[0].source_id==0x1000&&capture.sources[1].source_id==0x1100&&
         capture.sources[0].config.enabled&&!capture.sources[1].config.enabled,
         "enabled and disabled known classes");
    const auto& matrix=capture.sources[0].local_to_world;
    need(matrix[0]==0&&matrix[1]==-1&&matrix[4]==1&&matrix[5]==0&&
         matrix[3]==7&&matrix[7]==8&&matrix[11]==9&&matrix[15]==1,
         "Matrix34 transposed to column-vector world");
    need(capture.sources[1].local_to_world[0]==1,"disabled class needs no instance");
    need(capture.members.size()==2&&capture.members[0].entity==0x1000&&
         capture.members[0].instance==0x2000&&capture.members[0].object_class==20&&
         capture.members[0].class_version==1&&capture.members[1].instance==0,
         "owned membership includes disabled zero-instance objects");
    const auto retained=capture.class_owners[0];
    m.word(0x1000+0x40,99);c.records.clear();m.bytes.clear();
    need(retained->class_id==20&&capture.sources[0].local_to_world[3]==7,
         "capture and class owners survive source removal");
    m=Memory{};setup(m);c=Classes{};c.records[20]=owner(20,true);
    reject(m,c,"unknown class refuses complete capture");
    c.records[21]=owner(21,false);
    m=Memory{};setup(m);m.mutate_disabled_instance=true;
    reject(m,c,"disabled member instance mutation refuses publication");
    m=Memory{};setup(m);
    m.word(0x100,0);reject(m,c,"missing global pointer");
    m=Memory{};setup(m);m.word(0x300,0x300);m.word(0x304,0x300);
    need(capture_world_lights({0x100},&m,&Memory::read,&c,&Classes::pin,capture,error)&&
         capture.sources.empty(),"valid empty sentinel");
    m=Memory{};setup(m);m.word(0x500,0x400);reject(m,c,"cycle refuses capture");
    m=Memory{};setup(m);m.word(0x508,0x1000);reject(m,c,"duplicate object refuses capture");
    m=Memory{};setup(m);m.word(0x504,0x300);reject(m,c,"previous link mismatch");
    m=Memory{};setup(m);m.word(0x304,0x400);reject(m,c,"sentinel tail mismatch");
    m=Memory{};setup(m);const float nan=std::numeric_limits<float>::quiet_NaN();
    m.put(0x2000+0x44,nan);reject(m,c,"NaN matrix");
    m=Memory{};setup(m);const float zero=0;m.put(0x2000+0x48,zero);
    reject(m,c,"singular matrix");
    m=Memory{};setup(m);m.mutate_list=true;reject(m,c,"list mutation between reads");
    m=Memory{};setup(m);c.pins=0;c.replace_on_third=true;
    reject(m,c,"class replacement between reads");
    c.replace_on_third=false;m=Memory{};setup_many(m,4096);
    need(capture_world_lights({0x100},&m,&Memory::read,&c,&Classes::pin,capture,error)&&
         capture.sources.size()==4096,"4096 objects accepted");
    m=Memory{};setup_many(m,4097);reject(m,c,"4097 objects refused");
    std::puts("native11 world light capture: PASS");
}
