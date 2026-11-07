#include "../renderer/native11/world_preparation_capture.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
constexpr std::uint32_t list=0x100,container=0x200,sentinel=0x300;
constexpr std::uint32_t n1=0x400,n2=0x420,n3=0x440,n4=0x460;
constexpr std::uint32_t e1=0x1000,e2=0x1200,e3=0x1400,e4=0x1600;
constexpr std::uint32_t i1=0x2000,i2=0x2200,i4=0x2400,d1=0x3000,d2=0x3200;
constexpr std::uint32_t craft_vtable=0x9000,db_vtable=0x9004,entity_vtable=0x9100;
constexpr std::uint32_t setup=0x9200,divisor_getter=0x9300;
constexpr std::uint32_t klass=0xa100,race=0xb000,systems1=0xc000,systems2=0xc200,borg=0xd000;
constexpr WorldModelReadProfile model_profile{list,craft_vtable,db_vtable};
constexpr CraftPreparationReadProfile preparation_profile{craft_vtable,db_vtable,setup,divisor_getter,borg,false};
const auto control=encode_native_x87(0x037f);
const std::vector<WorldMember> roster{{e1,i1,klass,1},{e2,i2,klass,1},
                                      {e3,0,0xa300,3},{e4,i4,0xa400,4}};
const std::array<float,12> placement{{1,0,0,0,1,0,0,0,1,.25f,0,0}};
void need(bool okay,const char* why){if(!okay){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Memory {
    std::map<std::uint32_t,std::uint8_t> bytes;
    std::map<std::uint32_t,std::shared_ptr<const ModelAsset>> models;
    std::shared_ptr<const LightClassRecord> owner;
    std::function<void(Memory&)> on_read;
    std::uint32_t trigger_address=0;
    unsigned trigger_at=0,trigger_count=0,reads=0,cached_reads=0,unsupported_database_reads=0;
    template<class T>void put(std::uint32_t address,const T& value){
        const auto* source=reinterpret_cast<const std::uint8_t*>(&value);
        for(std::size_t k=0;k<sizeof(value);++k)bytes[address+static_cast<std::uint32_t>(k)]=source[k];
    }
    void name(std::uint32_t address,const char* value){
        for(std::size_t k=0;k<=std::strlen(value);++k)put(address+static_cast<std::uint32_t>(k),value[k]);
    }
};
bool read(void* context,std::uint32_t address,void* output,std::size_t size){
    auto& m=*static_cast<Memory*>(context);++m.reads;
    if(address==m.trigger_address&&++m.trigger_count==m.trigger_at&&m.on_read)m.on_read(m);
    if(address==i4+0x80)++m.unsupported_database_reads;
    for(const auto instance:{i1,i2,i4})
        for(const auto offset:{0x9cu,0x9du,0x9eu,0x9fu,0xa0u,0xa1u,0xa4u})
            if(address==instance+offset)++m.cached_reads;
    auto* destination=static_cast<std::uint8_t*>(output);
    for(std::size_t k=0;k<size;++k){
        if(k>UINT32_MAX-address)return false;
        const auto found=m.bytes.find(address+static_cast<std::uint32_t>(k));
        if(found==m.bytes.end())return false;
        destination[k]=found->second;
    }return true;
}
std::shared_ptr<const ModelAsset> model(std::uint32_t database,const char* name,std::uint64_t serial){
    auto result=std::make_shared<ModelAsset>();result->info.database=database;
    result->info.serial=serial;result->bytes={1,2,3};result->info.byte_count=3;
    std::strcpy(result->info.name,name);return result;
}
std::shared_ptr<const ModelAsset> pin_model(void* context,std::uint32_t database,const char* name){
    const auto& models=static_cast<Memory*>(context)->models;
    const auto found=models.find(database);
    return found!=models.end()&&found->second&&
        std::strcmp(found->second->info.name,name)==0?found->second:nullptr;
}
std::shared_ptr<const LightClassRecord> pin_class(void* context,std::uint64_t id){
    auto& m=*static_cast<Memory*>(context);
    return id==klass?m.owner:nullptr;
}
// Keep typed class allocation separate from GCC 16's inlined shared_ptr
// callback disposal, which otherwise mistakes this owner for ModelAsset.
__attribute__((noinline)) std::shared_ptr<const LightClassRecord> class_owner(){
    auto result=std::make_shared<LightClassRecord>();
    result->class_id=klass;result->version=1;
    return result;
}
Memory seeded(){
    Memory m;
    m.put(list,container);m.put(container+4,sentinel);
    m.put(sentinel,n1);m.put(sentinel+4,n4);
    const std::uint32_t nodes[]{n1,n2,n3,n4};
    const std::uint32_t entities[]{e1,e2,e3,e4};
    for(unsigned k=0;k<4;++k){
        m.put(nodes[k],k==3?sentinel:nodes[k+1]);
        m.put(nodes[k]+4,k==0?sentinel:nodes[k-1]);
        m.put(nodes[k]+8,entities[k]);
        m.put(entities[k]+4,roster[k].instance);
        m.put(entities[k]+0x40,roster[k].object_class);
    }
    for(const auto instance:{i1,i2}){
        m.put(instance,craft_vtable);m.put(instance+0x44,placement);
        m.put(instance+0x74,std::uint8_t{0});m.put(instance+0x80,instance==i1?d1:d2);
        m.put(instance+0x98,klass);
        for(const auto offset:{0x9cu,0x9du,0x9eu,0x9fu,0xa0u,0xa1u,0xa4u})
            m.put(instance+offset,std::uint8_t{0xff});
    }
    m.put(i4,std::uint32_t{0xdead});
    m.put(d1,db_vtable);m.put(d1+0x38,std::uint32_t{0x3400});m.name(0x3400,"first.sod");
    m.put(d2,db_vtable);m.put(d2+0x38,std::uint32_t{0x3500});m.name(0x3500,"second.sod");
    m.models[d1]=model(d1,"first.sod",1);m.models[d2]=model(d2,"second.sod",2);
    m.owner=class_owner();
    m.put(craft_vtable+0x38,setup);m.put(entity_vtable+0x108,divisor_getter);
    m.put(klass+0x84,std::uint32_t{1});m.put(borg,race);m.put(race+0x288,2.f);
    for(const auto entity:{e1,e2}){
        m.put(entity,entity_vtable);m.put(entity+0xfc,race);
        m.put(entity+0x1e0,entity==e1?systems1:systems2);
        m.put(entity+0x1bf,std::uint8_t{0});
        m.put(entity+0x1dc,50.f);m.put(entity+0x1c4,100.f);
    }
    const std::uint32_t offsets[]{0,0x30,0x60,0x90,0xc0};
    const std::uint8_t states[]{0,1,2,255,0};
    for(const auto systems:{systems1,systems2})for(unsigned k=0;k<5;++k)
        m.put(systems+offsets[k],states[k]);
    return m;
}
WorldModelAssociations associations(Memory& m){
    WorldModelAssociations result;std::string error;
    need(capture_world_model_associations(model_profile,roster,&m,read,&m,pin_model,result,error)&&
         result.members.size()==4,"stable ordered model association baseline");
    return result;
}
bool capture(const WorldModelAssociations& baseline,Memory& m,WorldCraftPreparations& output,
             std::string& error,CraftPreparationReadProfile profile=preparation_profile,
             std::uint32_t cw=control){
    return capture_world_craft_preparations(baseline,model_profile,profile,cw,&m,read,&m,pin_model,
                                            &m,pin_class,output,error);
}
void fails(const WorldModelAssociations& baseline,Memory& m,const char* why,
           CraftPreparationReadProfile profile=preparation_profile,std::uint32_t cw=control){
    WorldCraftPreparations output;output.native_x87_control=0xbeef;
    WorldCraftPreparation marker;marker.reason="retained";output.members.push_back(marker);
    std::string error;
    need(!capture(baseline,m,output,error,profile,cw)&&!error.empty()&&
         output.native_x87_control==0xbeef&&output.members.size()==1&&
         output.members[0].reason=="retained",why);
}
}
int main(){
    Memory m=seeded();const auto baseline=associations(m);
    const auto reset=[&](){
        Memory fresh=seeded();
        fresh.models[d1]=baseline.members[0].model;
        fresh.models[d2]=baseline.members[1].model;
        return fresh;
    };
    need(baseline.members[0].status==WorldModelStatus::bound_current_database&&
         baseline.members[1].status==WorldModelStatus::bound_current_database&&
         baseline.members[2].status==WorldModelStatus::no_instance&&
         baseline.members[3].status==WorldModelStatus::unsupported_instance&&
         m.unsupported_database_reads==0,"two bound actors and two explicit association-unavailable rows");
    WorldCraftPreparations output;std::string error;
    need(capture(baseline,m,output,error)&&error.empty()&&output.native_x87_control==control&&
         output.members.size()==4&&output.members[0].status==WorldPreparationStatus::captured&&
         output.members[1].status==WorldPreparationStatus::captured&&
         output.members[2].status==WorldPreparationStatus::association_unavailable&&
         output.members[3].status==WorldPreparationStatus::association_unavailable&&
         output.members[0].preparation.model==baseline.members[0].model&&
         output.members[1].preparation.model==baseline.members[1].model&&
         output.members[0].preparation.object_class==m.owner&&
         output.members[1].preparation.object_class==m.owner&&
         output.members[0].preparation.state.hull_fraction==.25f&&
         output.members[1].preparation.state.hull_fraction==.25f&&
         m.cached_reads==0&&m.unsupported_database_reads==0,
         "full ordered roster captures logical setup without visibility or cached render flags");
    m.put(systems1+0x30,std::uint8_t{0});
    need(capture(baseline,m,output,error)&&!output.members[0].preparation.state.systems_hidden[1]&&
         output.members[1].preparation.state.systems_hidden[1]&&m.cached_reads==0,
         "fresh logical system state changes one actor independently of render cache");
    m=reset();
    auto fleet_profile=preparation_profile;fleet_profile.fleet_cache=true;
    const std::uint32_t caches[]{0xe000,0xe100};
    const std::uint32_t instances[]{i1,i2};
    const std::uint32_t offsets[]{0xa2,0xa3,0x91,0x92};
    for(unsigned k=0;k<2;++k){
        std::uint8_t pointer[4]{};std::memcpy(pointer,&caches[k],4);
        for(unsigned j=0;j<4;++j)m.put(instances[k]+offsets[j],pointer[j]);
        m.put(caches[k],std::int32_t{-7-static_cast<int>(k)});
        m.put(caches[k]+4,std::uint32_t{0x12340000u+k});
    }
    need(capture(baseline,m,output,error,fleet_profile)&&
         output.members[0].preparation.fleet_cache_pointer==caches[0]&&
         output.members[1].preparation.fleet_cache_pointer==caches[1]&&
         output.members[0].preparation.fleet_cached_index==-7&&
         output.members[1].preparation.fleet_cached_index==-8&&
         output.members[0].preparation.fleet_cached_texture==0x12340000u&&
         output.members[1].preparation.fleet_cached_texture==0x12340001u&&
         output.members[0].preparation.model==baseline.members[0].model&&
         m.cached_reads==0,
         "Fleet split pointer copies per-actor correlation without cached output reads");
    m.trigger_address=i1+0xa2;m.trigger_at=3;
    m.on_read=[](Memory& x){x.put(0xe000+4,std::uint32_t{0xdeadbeef});};
    fails(baseline,m,"Fleet cache correlation drift across sweeps revokes output",fleet_profile);
    m=reset();m.put(e2+0x1c4,0.f);
    need(capture(baseline,m,output,error)&&output.members[1].status==WorldPreparationStatus::preparation_unavailable&&
         !output.members[1].reason.empty()&&output.members[0].status==WorldPreparationStatus::captured,
         "bad hull is an explicit unavailable row");
    m=reset();m.trigger_address=e2+4;m.trigger_at=1;
    m.on_read=[](Memory& x){x.put(borg,std::uint32_t{race+4});};
    fails(baseline,m,"shared Borg source drift across members revokes output");
    m=reset();m.trigger_address=e2+4;m.trigger_at=1;
    m.on_read=[](Memory& x){x.owner=class_owner();};
    fails(baseline,m,"shared class owner drift across members revokes output");
    m=reset();m.trigger_address=e2+4;m.trigger_at=1;
    m.on_read=[](Memory& x){x.put(klass+0x84,std::uint32_t{0});};
    fails(baseline,m,"shared class health mode drift across members revokes output");
    m=reset();m.trigger_address=e2+4;m.trigger_at=1;
    m.on_read=[](Memory& x){x.put(race+0x288,3.f);};
    fails(baseline,m,"shared race divisor drift across members revokes output");
    m=reset();m.trigger_address=e1+0x1bf;m.trigger_at=3;
    m.on_read=[](Memory& x){x.put(systems1+0x30,std::uint8_t{0});};
    fails(baseline,m,"one member changes between complete sweeps");
    m=reset();m.trigger_address=e1+4;m.trigger_at=1;
    m.on_read=[](Memory& x){x.put(n2+8,e1);};
    fails(baseline,m,"roster drift during setup rejected by final list rewalk");
    m=reset();m.trigger_address=e1+4;m.trigger_at=1;
    m.on_read=[](Memory& x){auto changed=placement;changed[9]=1.f;x.put(i1+0x44,changed);};
    fails(baseline,m,"placement drift during setup rejected by final association rewalk");
    m=reset();m.trigger_address=e1+4;m.trigger_at=1;
    m.on_read=[](Memory& x){x.models[d1]=model(d1,"first.sod",3);};
    fails(baseline,m,"same-database model owner replacement during setup rejected");
    m=reset();m.models[d1]=model(d1,"first.sod",3);
    fails(baseline,m,"model owner replacement before final pin rejected");
    m=reset();m.reads=0;fails(baseline,m,"invalid control rejects before reads",preparation_profile,0);
    need(m.reads==0,"invalid control performs no reads");
    m=reset();auto invalid=preparation_profile;invalid.instance_vtable=0;
    m.reads=0;fails(baseline,m,"invalid profile rejects before reads",invalid);
    need(m.reads==0,"invalid profile performs no reads");
    m=reset();need(m.cached_reads==0&&m.unsupported_database_reads==0,
                   "no cached flag or unsafe unsupported-instance read");
    std::puts("PASS native11 world Craft preparation capture");
}
