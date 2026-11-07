#include "../renderer/native11/prepared_craft_visibility_capture.hpp"
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
constexpr std::uint32_t entity=0x1000,instance=0x2000,database=0x3000;
constexpr std::uint32_t klass=0xa000,engine_pointer=0xb000,engine=0xb100,context=0xb200;
constexpr std::uint32_t ivtable=0xc000,dvtable=0xc100,setup=0xc200,divisor_getter=0xc300;
constexpr std::uint32_t entity_vtable=0xc400,race=0xf000,systems=0xf400,borg=0xf900;
constexpr std::array<std::uint32_t,5> class_offsets{{0x40c,0x418,0x410,0x41c,0x414}};
constexpr CraftVisibilityReadProfile visibility_profile{ivtable,dvtable,engine_pointer,context};
constexpr CraftPreparationReadProfile preparation_profile{ivtable,dvtable,setup,divisor_getter,borg,false};
const auto control=encode_native_x87(0x037f);
void need(bool okay,const char* why){if(!okay){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Memory {
    std::map<std::uint32_t,std::uint8_t> bytes;
    std::map<std::uint32_t,unsigned> reads;
    std::function<void(Memory&)> on_read;
    std::uint32_t trigger_address=0;unsigned trigger_at=0,trigger_count=0;
    unsigned cached_reads=0;
    template<class T>void put(std::uint32_t address,const T& value){
        const auto* source=reinterpret_cast<const std::uint8_t*>(&value);
        for(std::size_t k=0;k<sizeof(value);++k)bytes[address+static_cast<std::uint32_t>(k)]=source[k];
    }
    void name(std::uint32_t address,const char* value){
        for(std::size_t k=0;k<=std::strlen(value);++k)put(address+static_cast<std::uint32_t>(k),value[k]);
    }
    static bool read(void* context,std::uint32_t address,void* output,std::size_t size){
        auto& m=*static_cast<Memory*>(context);++m.reads[address];
        if(address==m.trigger_address&&++m.trigger_count==m.trigger_at&&m.on_read)m.on_read(m);
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
};
__attribute__((noinline)) std::shared_ptr<const LightClassRecord> class_owner(){
    auto result=std::make_shared<LightClassRecord>();result->class_id=klass;result->version=5;return result;
}
struct Fixture {
    Memory memory;
    std::shared_ptr<const LightClassRecord> owner=class_owner();
    std::shared_ptr<ModelAsset> model=std::make_shared<ModelAsset>();
    std::shared_ptr<AuthoredPoseData> authored=std::make_shared<AuthoredPoseData>();
    std::shared_ptr<ModelNodeTemplateData> data=std::make_shared<ModelNodeTemplateData>();
    std::shared_ptr<ModelNodeTemplate> baseline=std::make_shared<ModelNodeTemplate>();
    WorldModelAssociation association;
    unsigned pins=0;
    bool replace_class=false;
    static std::shared_ptr<const LightClassRecord> pin(void* raw,std::uint64_t id){
        auto& f=*static_cast<Fixture*>(raw);++f.pins;
        if(f.replace_class)f.owner=class_owner();
        return id==klass?f.owner:nullptr;
    }
    Fixture(){
        model->info.database=database;model->info.serial=7;model->bytes={1};
        model->info.byte_count=1;std::strcpy(model->info.name,"craft.sod");
        baseline->model=model;baseline->authored=authored;baseline->data=data;
        association.member={entity,instance,klass,5};
        association.status=WorldModelStatus::bound_current_database;
        association.instance_vtable=ivtable;association.database=database;
        association.database_vtable=dvtable;association.model=model;
        std::strcpy(association.model_name,"craft.sod");
        auto& m=memory;
        m.put(entity,entity_vtable);m.put(entity+4,instance);m.put(entity+0x40,klass);
        m.put(instance,ivtable);m.put(instance+0x80,database);m.put(instance+0x98,klass);
        m.put(ivtable+0x38,setup);m.put(entity_vtable+0x108,divisor_getter);
        m.put(database,dvtable);m.put(database+0x38,std::uint32_t{0xe000});
        m.name(0xe000,"craft.sod");m.put(database+0x3c,std::uint32_t{0x4000});
        m.put(engine_pointer,engine);m.put(engine+0x104,context);m.put(context+0x40,std::uint32_t{1});
        m.put(instance+0x90,std::uint8_t{0});m.put(klass+0x1dc,database);
        m.put(klass+0x84,std::uint32_t{1});m.put(borg,race);m.put(entity+0xfc,race);
        m.put(race+0x288,2.f);m.put(entity+0x1e0,systems);
        m.put(entity+0x1bf,std::uint8_t{0});m.put(entity+0x1dc,50.f);m.put(entity+0x1c4,100.f);
        const std::uint32_t system_offsets[]{0,0x30,0x60,0x90,0xc0};
        const std::uint8_t system_values[]{0,1,2,255,0};
        for(unsigned k=0;k<5;++k)m.put(systems+system_offsets[k],system_values[k]);
        for(const auto offset:{0x9cu,0x9du,0x9eu,0x9fu,0xa0u,0xa1u,0xa4u})
            m.put(instance+offset,std::uint8_t{0xff});
        data->hardpoints=1;data->damage=2;data->lights=5;
        const std::array<const char*,12> names{{"root","hardpoints","damage","d0","d1","lights",
            "crew","life","weapons","engines","sensors","assimilation"}};
        data->nodes.resize(names.size());authored->nodes.resize(names.size());
        for(std::size_t k=0;k<names.size();++k){
            auto& node=data->nodes[k];auto& pose=authored->nodes[k];
            node.address=0x4000+static_cast<std::uint32_t>(k)*0x100;
            node.vtable=0x7000+static_cast<std::uint32_t>(k)*0x20;
            node.type_function=0x8000+static_cast<std::uint32_t>(k)*0x10;
            node.name_pointer=0x9000+static_cast<std::uint32_t>(k)*0x40;
            node.name=names[k];pose.name=node.name;
            node.parent=k==0?UINT32_MAX:(k==3||k==4?2:0);pose.parent=node.parent;
            node.flags=k==3||k==4?0x81u:0x80u;
            m.put(node.address,node.vtable);m.put(node.vtable+4,node.type_function);
            m.put(node.address+4,database);m.put(node.address+8,node.name_pointer);
            m.name(node.name_pointer,names[k]);
        }
        auto link=[&](unsigned parent,unsigned child,unsigned sibling){
            auto& node=data->nodes[child];node.parent=parent;
            node.parent_address=data->nodes[parent].address;
            node.next_sibling=sibling==UINT32_MAX?UINT32_MAX:sibling;
            node.sibling_address=sibling==UINT32_MAX?0:data->nodes[sibling].address;
            authored->nodes[child].parent=parent;
        };
        data->nodes[0].first_child=1;data->nodes[0].child_address=data->nodes[1].address;
        link(0,1,2);link(0,2,5);link(0,5,6);
        for(unsigned k=6;k<11;++k)link(0,k,k+1);
        link(0,11,UINT32_MAX);
        data->nodes[2].first_child=3;data->nodes[2].child_address=data->nodes[3].address;
        link(2,3,4);link(2,4,UINT32_MAX);
        data->root_address=data->nodes[0].address;
        m.put(database+0x2c,data->nodes[1].address);
        m.put(database+0x30,data->nodes[2].address);
        m.put(database+0x34,data->nodes[5].address);
        for(const auto& node:data->nodes){
            m.put(node.address+0x18,node.parent_address);
            m.put(node.address+0x1c,node.child_address);
            m.put(node.address+0x20,node.sibling_address);
        }
        m.put(klass+0x408,data->nodes[2].address);
        m.put(klass+0x420,data->nodes[11].address);
        for(unsigned k=0;k<5;++k)m.put(klass+class_offsets[k],data->nodes[6+k].address);
    }
    bool prepare(CapturedCraftPreparation& output,std::string& error){
        return capture_craft_preparation(association,preparation_profile,control,
                                         &memory,Memory::read,this,pin,output,error);
    }
    bool capture(const CapturedCraftPreparation& held,CapturedCraftVisibility& output,std::string& error,
                 CraftVisibilityReadProfile vp=visibility_profile,
                 CraftPreparationReadProfile pp=preparation_profile){
        return capture_prepared_craft_visibility(association,held,baseline,vp,pp,
                                                 &memory,Memory::read,this,pin,output,error);
    }
};
void rejected(Fixture& f,const CapturedCraftPreparation& held,const char* why,
              CraftVisibilityReadProfile vp=visibility_profile,
              CraftPreparationReadProfile pp=preparation_profile){
    CapturedCraftVisibility output;output.entity=0xdead;output.instance=0xbeef;
    output.quality=77;output.input.root=9;output.baseline=std::make_shared<ModelNodeTemplate>();
    output.preparation=std::make_shared<CapturedCraftPreparation>(held);
    const auto saved=output.baseline;const auto saved_preparation=output.preparation;std::string error;
    need(!f.capture(held,output,error,vp,pp)&&!error.empty()&&
         output.entity==0xdead&&output.instance==0xbeef&&output.quality==77&&
         output.input.root==9&&output.baseline==saved&&output.preparation==saved_preparation,why);
}
}
int main(){
    Fixture f;CapturedCraftPreparation held;std::string error;
    need(f.prepare(held,error)&&error.empty(),"checked logical preparation baseline");
    f.memory.reads.clear();f.memory.cached_reads=0;
    CapturedCraftVisibility output;
    need(f.capture(held,output,error)&&error.empty()&&output.preparation&&
         output.preparation->model==held.model&&output.preparation->object_class==held.object_class&&
         output.baseline==f.baseline&&output.input.nodes.size()==12&&
         output.input.system_hidden==std::array<bool,5>{{false,true,true,true,false}}&&
         output.input.hull_fraction==.25f&&output.input.primary_database&&
         f.memory.cached_reads==0,
         "fresh preparation supplies twelve-node visibility without cached output flags");
    CraftVisibilityResult evaluated;
    need(evaluate_craft_visibility(output.input,evaluated,error)&&error.empty()&&
         evaluated.flags[6]==0x80&&evaluated.flags[7]==0x82&&
         evaluated.flags[11]==0x80,
         "prepared system and Borg inputs change evaluated node flags");
    f.memory.put(systems+0x30,std::uint8_t{0});
    rejected(f,held,"stale held logical preparation rejected atomically");
    CapturedCraftPreparation fresh;
    need(f.prepare(fresh,error)&&f.capture(fresh,output,error)&&
         !output.input.system_hidden[1]&&held.state.systems_hidden[1]&&
         f.memory.cached_reads==0,"new held preparation reflects fresh logical state");
    auto forged=fresh;forged.state.hull_fraction=.9f;
    rejected(f,forged,"forged prepared policy rejected");
    f.association.model=std::make_shared<ModelAsset>(*f.model);
    rejected(f,fresh,"model owner mismatch rejected");
    f.association.model=f.model;
    f.owner=class_owner();rejected(f,fresh,"class owner mismatch rejected");
    f.owner=fresh.object_class;
    f.association.status=WorldModelStatus::unsupported_instance;
    rejected(f,fresh,"unsupported association rejected");
    f.association.status=WorldModelStatus::bound_current_database;
    ++f.association.member.instance;
    rejected(f,fresh,"world member identity mismatch rejected");
    --f.association.member.instance;
    f.baseline=std::make_shared<ModelNodeTemplate>(*f.baseline);
    f.baseline->model=std::make_shared<ModelAsset>(*f.model);
    rejected(f,fresh,"baseline model mismatch rejected");
    f.baseline->model=f.model;
    f.memory.put(f.data->nodes[0].address+0x20,std::uint32_t{1});
    rejected(f,fresh,"topology drift rejected");
    f.memory.put(f.data->nodes[0].address+0x20,f.data->nodes[0].sibling_address);
    f.memory.put(context+0x40,std::uint32_t{0});
    CapturedCraftVisibility disabled_context;
    need(f.capture(fresh,disabled_context,error)&&!disabled_context.input.context_enabled&&
         disabled_context.preparation&&disabled_context.preparation->object_class==fresh.object_class,
         "stable disabled context remains a captured policy input");
    f.memory.put(context+0x40,std::uint32_t{1});
    f.memory.put(klass+0x40c,std::uint32_t{1});
    rejected(f,fresh,"class role drift rejected");
    f.memory.put(klass+0x40c,f.data->nodes[6].address);
    f.memory.trigger_address=f.data->nodes[0].address+0x18;f.memory.trigger_at=1;
    f.memory.on_read=[](Memory& m){m.put(systems+0x60,std::uint8_t{0});};
    rejected(f,fresh,"logical preparation drift during topology scan rejected");
    f.memory.put(systems+0x60,std::uint8_t{2});
    // Change quality after its first read and before the second pass reads it.
    f.memory.trigger_count=0;f.memory.trigger_at=1;
    f.memory.on_read=[](Memory& m){m.put(context+0x40,std::uint32_t{0});};
    rejected(f,fresh,"render context drift between topology passes rejected");
    f.memory.put(context+0x40,std::uint32_t{1});
    f.memory.trigger_address=klass+0x40c;f.memory.trigger_count=0;f.memory.trigger_at=2;
    f.memory.on_read=[](Memory& m){m.put(klass+0x40c,std::uint32_t{0});};
    rejected(f,fresh,"class role drift between topology passes rejected");
    f.memory.put(klass+0x40c,f.data->nodes[6].address);
    f.memory.on_read=nullptr;f.memory.trigger_address=0;
    f.memory.put(borg,std::uint32_t{0});
    rejected(f,fresh,"unavailable fresh preparation source rejected");
    f.memory.put(borg,race);
    auto saved_baseline=f.baseline;f.baseline.reset();
    rejected(f,fresh,"missing completed-load baseline rejected");f.baseline=saved_baseline;
    auto bad=preparation_profile;bad.setup_function=0;
    rejected(f,fresh,"invalid preparation profile rejected",visibility_profile,bad);
    Fixture alternate;CapturedCraftPreparation alternate_held;
    need(alternate.prepare(alternate_held,error),"alternate logical preparation baseline");
    alternate.memory.put(klass+0x1dc,std::uint32_t{database+4});
    CapturedCraftVisibility secondary;
    need(alternate.capture(alternate_held,secondary,error)&&!secondary.input.primary_database&&
         secondary.input.hull_fraction==1.f&&alternate.memory.cached_reads==0,
         "secondary database branch skips primary roles while retaining fresh logical preparation");
    need(f.memory.cached_reads==0,"prepared path never reads cached instance output flags");
    std::puts("PASS native11 prepared Craft visibility capture");
}
