#include "../renderer/native11/world_view_capture.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <array>
#include <cmath>
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
constexpr std::uint32_t camera=0x7000,craft_vtable=0x9000,db_vtable=0x9004;
constexpr WorldModelReadProfile model_profile{list,craft_vtable,db_vtable};
constexpr CraftViewReadProfile view_profile{0x4000,0x4004,0x4008,0x400c,0x4010,
                                           craft_vtable,db_vtable,0x4014};
const auto control=encode_native_x87(0x037f);
const std::vector<WorldMember> roster{{e1,i1,0xa100,1},{e2,i2,0xa200,2},
                                      {e3,0,0xa300,3},{e4,i4,0xa400,4}};
const std::array<float,12> placement{{1,0,0,0,1,0,0,0,1,.25f,0,0}};
void need(bool good,const char* why){if(!good){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
std::uint32_t bits(float value){std::uint32_t raw=0;std::memcpy(&raw,&value,4);return raw;}
struct Memory {
    std::map<std::uint32_t,std::uint8_t> bytes;
    std::map<std::uint32_t,std::shared_ptr<const ModelAsset>> models;
    std::function<void(Memory&)> on_engine;
    unsigned reads=0,engine_reads=0,unsupported_database_reads=0,stale_reads=0,engine_trigger=0;
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
    if(address==view_profile.engine_pointer&&++m.engine_reads==m.engine_trigger&&m.on_engine)
        m.on_engine(m);
    if(address==i4+0x80)++m.unsupported_database_reads;
    if(address==0x5088||address==0x508c)++m.stale_reads;
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
std::shared_ptr<const ModelAsset> pin(void* context,std::uint32_t database,const char* name){
    const auto& models=static_cast<Memory*>(context)->models;
    const auto found=models.find(database);
    return found!=models.end()&&found->second&&
        std::strcmp(found->second->info.name,name)==0?found->second:nullptr;
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
        m.put(instance+0x74,std::uint8_t(0));
        m.put(instance+0x40,bits(1.f));m.put(instance+0x84,std::uint8_t(0));
    }
    m.put(i1+0x80,d1);m.put(i2+0x80,d2);m.put(i4,std::uint32_t{0xdead});
    m.put(d1,db_vtable);m.put(d1+0x38,std::uint32_t{0x3400});m.name(0x3400,"first.sod");
    m.put(d2,db_vtable);m.put(d2+0x38,std::uint32_t{0x3500});m.name(0x3500,"second.sod");
    m.models[d1]=model(d1,"first.sod",1);m.models[d2]=model(d2,"second.sod",2);
    m.put(view_profile.engine_pointer,std::uint32_t{0x5000});
    m.put(0x50fc,camera);m.put(0x50e8,std::uint32_t{0});
    m.put(view_profile.game_pointer,std::uint32_t{0x6000});
    m.put(0x6080,std::uint32_t{0x6100});m.put(0x6140,std::uint32_t{1});
    m.put(0x6084,std::uint8_t{0});m.put(view_profile.mode_pointer,std::uint32_t{1});
    m.put(view_profile.skip_near_pointer,std::uint8_t{0});
    m.put(view_profile.max_distance_pointer,bits(10.f));
    m.put(view_profile.near_fade_pointer,bits(.5f));
    m.put(view_profile.near_fade_pointer+4,bits(2.f));
    m.put(view_profile.near_fade_pointer+8,bits(.875f));
    for(unsigned k=0;k<3;++k){
        m.put(camera+0xe4+4*k,bits(0.f));
        m.put(e1+0xac+4*k,bits(k==0?3.f:k==1?4.f:0.f));
        m.put(e2+0xac+4*k,bits(k==0?6.f:k==1?8.f:0.f));
    }
    m.put(0x5088,bits(999.f));m.put(0x508c,bits(999.f));
    return m;
}
WorldModelAssociations associations(Memory& m){
    WorldModelAssociations result;std::string error;
    need(capture_world_model_associations(model_profile,roster,&m,read,&m,pin,result,error)&&
         result.members.size()==4,"stable ordered model association baseline");
    return result;
}
bool capture(const WorldModelAssociations& models,Memory& m,WorldCraftViews& output,
             std::string& error,CraftViewReadProfile vp=view_profile,std::uint32_t cw=control){
    return capture_world_craft_views(models,model_profile,vp,camera,cw,
                                     &m,read,&m,pin,output,error);
}
void fails(const WorldModelAssociations& models,Memory& m,const char* why,
           CraftViewReadProfile vp=view_profile,std::uint32_t cw=control){
    WorldCraftViews output;output.camera=0xdead;output.native_x87_control=0xbeef;
    WorldCraftView marker;marker.reason="retained";output.members.push_back(marker);
    std::string error;
    need(!capture(models,m,output,error,vp,cw)&&!error.empty()&&output.camera==0xdead&&
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
         m.unsupported_database_reads==0,"two current models and explicit unavailable associations");
    WorldCraftViews views;std::string error;
    need(capture(baseline,m,views,error)&&error.empty()&&views.camera==camera&&
         views.members.size()==4&&views.members[0].status==WorldViewStatus::captured&&
         views.members[1].status==WorldViewStatus::captured&&
         views.members[2].status==WorldViewStatus::association_unavailable&&
         views.members[3].status==WorldViewStatus::association_unavailable&&
         views.members[0].view.input.position[0]==3.f&&
         views.members[1].view.input.position[0]==6.f&&
         std::fabs(views.members[0].view.result.distance-5.f)<.0001f&&
         m.unsupported_database_reads==0&&m.stale_reads==0,
         "full roster retains two fresh views without visible-object input or unsafe instance dereference");
    m.put(camera+0xe4,bits(1.f));
    need(capture(baseline,m,views,error)&&views.members[0].view.input.camera[0]==1.f&&
         views.members[0].view.result.distance!=5.f&&m.stale_reads==0,
         "fresh camera changes policy result independent of cached engine 88/8c");
    m.put(e2+0xac,bits(5.f));
    need(capture(baseline,m,views,error)&&views.members[1].view.input.position[0]==5.f&&
         m.stale_reads==0,"fresh logical position changes copied view input");
    m=reset();m.put(i2+0x40,bits(1000001.f));
    need(capture(baseline,m,views,error)&&views.members[1].status==WorldViewStatus::view_unavailable&&
         !views.members[1].reason.empty(),"unreadable view remains an explicit row");
    m=reset();m.engine_trigger=1;
    m.on_engine=[](Memory& x){x.put(i1+0x44,std::array<float,12>{{2,0,0,0,1,0,0,0,1,0,0,0}});};
    fails(baseline,m,"placement drift during view scan revokes full output");
    m=reset();m.engine_trigger=1;m.on_engine=[](Memory& x){x.put(n2+8,e1);};
    fails(baseline,m,"list drift during view scan revokes full output");
    m=reset();m.engine_trigger=1;m.on_engine=[](Memory& x){x.put(i1+0x80,d2);};
    fails(baseline,m,"database drift during view scan revokes full output");
    m=reset();m.engine_trigger=3;m.on_engine=[](Memory& x){x.put(0x6140,std::uint32_t{0});};
    fails(baseline,m,"shared settings drift across roster members rejected");
    m=reset();m.engine_trigger=3;
    m.on_engine=[](Memory& x){x.put(view_profile.near_fade_pointer,bits(.9f));};
    fails(baseline,m,"shared near-fade parameters drift across roster members rejected");
    m=reset();m.engine_trigger=5;m.on_engine=[](Memory& x){x.put(camera+0xe4,bits(1.f));};
    fails(baseline,m,"shared camera drift across sweeps rejected");
    m=reset();m.engine_trigger=5;m.on_engine=[](Memory& x){x.put(view_profile.mode_pointer,std::uint32_t{2});};
    fails(baseline,m,"shared mode drift across sweeps rejected");
    m=reset();m.models[d1]=model(d1,"first.sod",3);
    fails(baseline,m,"same-database replacement owner rejected by fresh rewalk");
    m=reset();auto invalid=baseline;invalid.members[0].model.reset();
    m.reads=0;fails(invalid,m,"invalid bound owner rejected before reads");
    need(m.reads==0,"invalid bound owner performs no memory read");
    m=reset();m.reads=0;fails(baseline,m,"invalid control rejected before reads",view_profile,0);
    need(m.reads==0,"invalid x87 control performs no memory read");
    m=reset();auto bad_profile=view_profile;bad_profile.instance_vtable=0;
    m.reads=0;fails(baseline,m,"invalid profile rejected before reads",bad_profile);
    need(m.reads==0,"invalid view profile performs no memory read");
    std::puts("PASS native11 world Craft view capture");
}
