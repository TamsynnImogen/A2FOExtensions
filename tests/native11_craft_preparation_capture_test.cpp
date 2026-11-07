#include "../renderer/native11/craft_preparation_capture.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
constexpr std::uint32_t entity=0x1000,instance=0x2000,database=0x3000;
constexpr std::uint32_t klass=0x4000,instance_vtable=0x5000,entity_vtable=0x5100;
constexpr std::uint32_t race=0x6000,systems=0x7000,borg_global=0x8000;
constexpr std::uint32_t name_address=0x9000,fleet_cache=0xa000;
constexpr std::uint32_t db_vtable=0xb000,setup_function=0xc000,divisor_function=0xc100;
const CraftPreparationReadProfile profile{instance_vtable,db_vtable,
    setup_function,divisor_function,borg_global,false};
const auto control=encode_native_x87(0x037f);
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Memory {
    std::map<std::uint32_t,std::uint8_t> bytes;
    std::vector<std::uint32_t> reads;
    std::function<void(Memory&)> on_second;
    std::shared_ptr<const LightClassRecord> owner;
    unsigned entity_instance_reads=0,pins=0,replace_pin=0;
    bool throw_read=false,throw_pin=false;
    template<class T>void put(std::uint32_t address,const T& value){
        const auto* source=reinterpret_cast<const std::uint8_t*>(&value);
        for(std::size_t k=0;k<sizeof(value);++k)bytes[address+static_cast<std::uint32_t>(k)]=source[k];
    }
    void name(std::uint32_t address,const char* value){
        for(std::size_t k=0;k<=std::strlen(value);++k)put(address+static_cast<std::uint32_t>(k),value[k]);
    }
};
bool read(void* context,std::uint32_t address,void* output,std::size_t size){
    auto& m=*static_cast<Memory*>(context);m.reads.push_back(address);
    if(m.throw_read)throw std::runtime_error("synthetic read callback");
    if(address==entity+4&&++m.entity_instance_reads==2&&m.on_second)m.on_second(m);
    auto* target=static_cast<std::uint8_t*>(output);
    for(std::size_t k=0;k<size;++k){
        if(k>UINT32_MAX-address)return false;
        const auto found=m.bytes.find(address+static_cast<std::uint32_t>(k));
        if(found==m.bytes.end())return false;
        target[k]=found->second;
    }return true;
}
// Keep the typed class allocation out of the optimized callback. GCC 16's
// shared_ptr inlining otherwise confuses its ModelAsset and class deleters.
__attribute__((noinline)) std::shared_ptr<const LightClassRecord> class_owner(){
    auto result=std::make_shared<LightClassRecord>();
    result->class_id=klass;result->version=7;
    return result;
}
std::shared_ptr<const LightClassRecord> pin(void* context,std::uint64_t id){
    auto& m=*static_cast<Memory*>(context);++m.pins;
    if(m.throw_pin)throw std::runtime_error("synthetic class pin callback");
    if(m.replace_pin&&m.pins==m.replace_pin)m.owner=class_owner();
    return id==klass?m.owner:nullptr;
}
std::shared_ptr<const ModelAsset> model(){
    auto result=std::make_shared<ModelAsset>();
    result->info.database=database;result->info.serial=11;
    result->info.byte_count=3;std::strcpy(result->info.name,"fixture.sod");
    result->bytes={1,2,3};return result;
}
struct Fixture {
    Memory memory;
    WorldModelAssociation association;
    CraftPreparationReadProfile source=profile;
    Fixture(){
        association.member={entity,instance,klass,7};
        association.status=WorldModelStatus::bound_current_database;
        association.instance_vtable=instance_vtable;
        association.database=database;association.database_vtable=db_vtable;
        std::strcpy(association.model_name,"fixture.sod");association.model=model();
        memory.owner=class_owner();
        auto& m=memory;
        m.put(entity,entity_vtable);m.put(entity+4,instance);m.put(entity+0x40,klass);
        m.put(instance,instance_vtable);m.put(instance+0x98,klass);
        m.put(instance+0x80,database);m.put(instance_vtable+0x38,setup_function);
        m.put(database,db_vtable);m.put(database+0x38,name_address);
        m.name(name_address,"fixture.sod");m.put(entity_vtable+0x108,divisor_function);
        m.put(borg_global,race);m.put(entity+0xfc,race);
        m.put(race+0x288,2.f);m.put(entity+0x1e0,systems);
        m.put(entity+0x1bf,std::uint8_t{0});m.put(klass+0x84,std::uint32_t{1});
        const std::uint32_t offsets[]{0,0x30,0x60,0x90,0xc0};
        const std::uint8_t states[]{0,1,2,255,0};
        for(unsigned i=0;i<5;++i)m.put(systems+offsets[i],states[i]);
        m.put(entity+0x1dc,50.f);m.put(entity+0x1c4,100.f);
        // Render-cache bytes are deliberately present but never source fields.
        for(const auto offset:{0x9cu,0x9du,0x9eu,0xa0u,0x9fu,0xa1u,0xa4u})
            m.put(instance+offset,std::uint8_t{0xff});
    }
    bool capture(CapturedCraftPreparation& output,std::string& error,std::uint32_t cw=control){
        return capture_craft_preparation(association,source,cw,&memory,read,&memory,pin,output,error);
    }
};
void no_cached_reads(const Memory& m,bool fleet){
    for(auto address:m.reads){
        for(const auto offset:{0x9cu,0x9du,0x9eu,0xa0u,0x9fu,0xa1u,0xa4u})
            need(address!=instance+offset,"native render-cache output flag never read");
        if(!fleet)for(const auto offset:{0xa2u,0xa3u,0x91u,0x92u})
            need(address!=instance+offset,"Fleet pointer bytes excluded without Fleet profile");
    }
}
void rejected(Fixture& f,const char* why,std::uint32_t cw=control){
    CapturedCraftPreparation output;
    output.member={0xdead,0,0xbeef,99};output.model=model();
    output.object_class=f.memory.owner;output.database=0xface;
    output.state.systems_hidden={{true,false,true,false,true}};
    output.state.borg_variant=true;output.state.hull_fraction=123.f;
    const auto before=output;std::string error;
    need(!f.capture(output,error,cw)&&!error.empty()&&
         output.member.entity==before.member.entity&&output.model==before.model&&
         output.object_class==before.object_class&&output.database==before.database&&
         output.state.systems_hidden==before.state.systems_hidden&&
         output.state.borg_variant==before.state.borg_variant&&
         output.state.hull_fraction==before.state.hull_fraction,why);
}
}
int main(){
    Fixture f;CapturedCraftPreparation output;std::string error;
    need(f.capture(output,error)&&error.empty()&&output.model==f.association.model&&
         output.object_class==f.memory.owner&&output.member.entity==entity&&
         output.entity_vtable==entity_vtable&&output.race==race&&output.systems==systems&&
         output.borg_cache==race&&output.health_mode==1&&output.native_x87_control==control&&
         output.input.system_state==std::array<std::uint8_t,5>{{0,1,2,255,0}}&&
         output.state.systems_hidden==std::array<bool,5>{{false,true,true,true,false}}&&
         output.state.borg_variant&&output.state.hull_fraction==.25f,
         "fresh logical Craft setup captures owner, class, systems, Borg and hull policy");
    no_cached_reads(f.memory,false);
    f.memory.put(entity+0x1bf,std::uint8_t{255});
    need(f.capture(output,error)&&
         output.state.systems_hidden==std::array<bool,5>{{false,false,false,false,false}},
         "nonzero logical suppress byte hides no systems");
    f.memory.put(borg_global,std::uint32_t{0x6100});
    need(f.capture(output,error)&&!output.state.borg_variant,
         "Borg cache identity mismatch clears variant");
    f=Fixture{};f.memory.put(klass+0x84,std::uint32_t{0});
    f.memory.bytes.erase(entity+0x1dc);f.memory.bytes.erase(entity+0x1c4);
    need(f.capture(output,error)&&!output.input.use_hull_ratio&&output.state.hull_fraction==.5f,
         "disabled hull mode omits current and maximum reads");

    f=Fixture{};f.source.fleet_cache=true;
    const std::uint8_t pointer_bytes[]{0x00,0xa0,0x00,0x00};
    const std::uint32_t pointer_offsets[]{0xa2,0xa3,0x91,0x92};
    for(unsigned i=0;i<4;++i)f.memory.put(instance+pointer_offsets[i],pointer_bytes[i]);
    f.memory.put(fleet_cache,std::int32_t{-7});
    f.memory.put(fleet_cache+4,std::uint32_t{0xdeadbeef});
    need(f.capture(output,error)&&output.fleet_cache_pointer==fleet_cache&&
         output.fleet_cached_index==-7&&output.fleet_cached_texture==0xdeadbeef&&
         output.model==f.association.model,
         "Fleet split pointer bytes copy cache correlation without selected-texture claim");
    no_cached_reads(f.memory,true);
    f=Fixture{};f.source.fleet_cache=true;
    const std::uint8_t overlap_bytes[]{0xa4,0x20,0x00,0x00};
    for(unsigned i=0;i<4;++i)f.memory.put(instance+pointer_offsets[i],overlap_bytes[i]);
    rejected(f,"Fleet cache overlapping instance storage rejected");
    f=Fixture{};f.source.fleet_cache=true;
    for(unsigned i=0;i<4;++i)f.memory.put(instance+pointer_offsets[i],pointer_bytes[i]);
    rejected(f,"unreadable Fleet cache target rejected");

    f=Fixture{};
    f.association.database=database+4;rejected(f,"different supplied current DB rejected atomically");
    need(f.memory.reads.empty(),"invalid bound model identity rejects before native reads");
    f=Fixture{};f.association.model_name[0]='x';rejected(f,"copied database name mismatch rejected");
    f=Fixture{};f.memory.owner.reset();rejected(f,"missing class owner rejected atomically");
    f=Fixture{};f.memory.replace_pin=3;rejected(f,"same-version class owner replacement rejected");
    f=Fixture{};auto wrong_version=std::make_shared<LightClassRecord>();
    wrong_version->class_id=klass;wrong_version->version=8;f.memory.owner=wrong_version;
    rejected(f,"class version mismatch rejected");
    f=Fixture{};f.memory.put(instance_vtable+0x38,std::uint32_t{0});
    rejected(f,"unknown setup dispatch rejected");
    f=Fixture{};f.memory.put(entity_vtable+0x108,std::uint32_t{0});
    rejected(f,"unknown hull divisor getter rejected");
    f=Fixture{};f.memory.put(borg_global,std::uint32_t{0});
    rejected(f,"zero Borg cache owner rejected");
    f=Fixture{};f.memory.put(systems+0x30,std::uint8_t{3});
    f.memory.on_second=[](Memory& m){m.put(systems+0x30,std::uint8_t{4});};
    rejected(f,"system byte drift across passes rejected");
    f=Fixture{};f.memory.on_second=[](Memory& m){m.put(entity+0xfc,std::uint32_t{0x6100});};
    rejected(f,"logical race drift across passes rejected");
    f=Fixture{};f.memory.on_second=[](Memory& m){m.put(klass+0x84,std::uint32_t{0});};
    rejected(f,"health mode drift across passes rejected");
    f=Fixture{};f.memory.on_second=[](Memory& m){m.put(entity+0x1dc,75.f);};
    rejected(f,"hull current drift across passes rejected");
    f=Fixture{};f.memory.put(entity+0x1dc,std::numeric_limits<float>::quiet_NaN());
    rejected(f,"nonfinite hull input rejected");
    f=Fixture{};f.memory.put(klass+0x84,std::uint32_t{255});
    need(f.capture(output,error)&&output.health_mode==255&&output.input.use_hull_ratio&&
         output.state.hull_fraction==.25f,
         "raw nonzero class health mode enables hull ratio");
    f=Fixture{};f.memory.put(race+0x288,0.f);rejected(f,"zero native divisor rejected");
    f=Fixture{};f.memory.throw_read=true;rejected(f,"throwing read callback preserves output");
    f=Fixture{};f.memory.throw_pin=true;rejected(f,"throwing class callback preserves output");
    f=Fixture{};f.memory.reads.clear();rejected(f,"unknown x87 control rejected before read",0);
    need(f.memory.reads.empty(),"unknown CW rejected before reads");
    f=Fixture{};f.memory.reads.clear();rejected(f,"reserved x87 precision rejected before read",
                                               encode_native_x87(0x017f));
    need(f.memory.reads.empty(),"reserved CW rejected before reads");
    f=Fixture{};f.memory.reads.clear();rejected(f,"malformed x87 encoding rejected before read",0x20000u);
    need(f.memory.reads.empty(),"malformed CW rejected before reads");
    std::puts("PASS native11 Craft preparation capture");
}
