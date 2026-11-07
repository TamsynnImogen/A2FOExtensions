#include "../renderer/native11/world_prepared_visibility.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
constexpr std::uint32_t list=0x100,container=0x200,sentinel=0x300;
constexpr std::uint32_t n1=0x400,n2=0x420,n3=0x440,n4=0x460;
constexpr std::uint32_t e1=0x1000,e2=0x1200,e3=0x1400,e4=0x1600;
constexpr std::uint32_t i1=0x2000,i2=0x2200,i4=0x2400,db=0x3000;
constexpr std::uint32_t klass=0xa000,engine_pointer=0xb000,engine=0xb100,context=0xb200;
constexpr std::uint32_t ivtable=0xc000,dvtable=0xc100,setup=0xc200,divisor_getter=0xc300;
constexpr std::uint32_t entity_vtable=0xc400,race=0xf000,systems1=0xf400,systems2=0xf600,borg=0xf900;
constexpr WorldModelReadProfile model_profile{list,ivtable,dvtable};
constexpr CraftVisibilityReadProfile visibility_profile{ivtable,dvtable,engine_pointer,context};
constexpr CraftPreparationReadProfile preparation_profile{ivtable,dvtable,setup,divisor_getter,borg,false};
const auto control=encode_native_x87(0x037f);
const std::vector<WorldMember> roster{{e1,i1,klass,5},{e2,i2,klass,5},
                                      {e3,0,0xa300,3},{e4,i4,0xa400,4}};
const std::array<float,12> placement{{1,0,0,0,1,0,0,0,1,0,0,0}};
constexpr std::array<std::uint32_t,5> class_offsets{{0x40c,0x418,0x410,0x41c,0x414}};
void need(bool okay,const char* why){if(!okay){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Memory {
    std::map<std::uint32_t,std::uint8_t> bytes;
    std::map<std::uint32_t,std::shared_ptr<const ModelAsset>> models;
    std::map<std::uint32_t,unsigned> reads;
    std::function<void(Memory&)> on_read;
    std::uint32_t trigger_address=0;unsigned trigger_at=0,trigger_count=0,cached_reads=0;
    bool throw_read=false;
    template<class T>void put(std::uint32_t address,const T& value){
        const auto* source=reinterpret_cast<const std::uint8_t*>(&value);
        for(std::size_t k=0;k<sizeof(value);++k)bytes[address+static_cast<std::uint32_t>(k)]=source[k];
    }
    void name(std::uint32_t address,const char* value){
        for(std::size_t k=0;k<=std::strlen(value);++k)put(address+static_cast<std::uint32_t>(k),value[k]);
    }
    static bool read(void* context,std::uint32_t address,void* output,std::size_t size){
        auto& m=*static_cast<Memory*>(context);++m.reads[address];
        if(m.throw_read)throw std::runtime_error("synthetic reader");
        if(address==m.trigger_address&&++m.trigger_count==m.trigger_at&&m.on_read)m.on_read(m);
        for(const auto instance:{i1,i2,i4})
            for(const auto offset:{0x9cu,0x9du,0x9eu,0x9fu,0xa0u,0xa1u,0xa4u})
                if(address==instance+offset)++m.cached_reads;
        auto* target=static_cast<std::uint8_t*>(output);
        for(std::size_t k=0;k<size;++k){
            if(k>UINT32_MAX-address)return false;
            const auto found=m.bytes.find(address+static_cast<std::uint32_t>(k));
            if(found==m.bytes.end())return false;
            target[k]=found->second;
        }return true;
    }
};
__attribute__((noinline)) std::shared_ptr<const LightClassRecord> class_owner(){
    auto result=std::make_shared<LightClassRecord>();result->class_id=klass;result->version=5;return result;
}
std::shared_ptr<const ModelAsset> model(std::uint64_t serial){
    auto result=std::make_shared<ModelAsset>();result->info.database=db;result->info.serial=serial;
    result->bytes={1,2,3};result->info.byte_count=3;std::strcpy(result->info.name,"craft.sod");return result;
}
struct Fixture {
    Memory memory;
    std::shared_ptr<const ModelAsset> model_owner=model(7);
    std::shared_ptr<const LightClassRecord> owner=class_owner();
    std::shared_ptr<AuthoredPoseData> authored=std::make_shared<AuthoredPoseData>();
    std::shared_ptr<ModelNodeTemplateData> data=std::make_shared<ModelNodeTemplateData>();
    std::shared_ptr<const ModelNodeTemplateData> selected_data;
    WorldModelAssociations models;
    WorldCraftPreparations preparations;
    unsigned template_calls=0,replace_template_at=0;
    bool missing_template=false,foreign_template=false,throw_template=false;
    static std::shared_ptr<const ModelAsset> pin_model(void* raw,std::uint32_t database,const char* name){
        const auto& map=static_cast<Fixture*>(raw)->memory.models;
        const auto found=map.find(database);
        return found!=map.end()&&found->second&&std::strcmp(found->second->info.name,name)==0?
            found->second:nullptr;
    }
    static std::shared_ptr<const LightClassRecord> pin_class(void* raw,std::uint64_t id){
        const auto& f=*static_cast<Fixture*>(raw);return id==klass?f.owner:nullptr;
    }
    static std::shared_ptr<const ModelNodeTemplate> pin_template(void* raw,const std::shared_ptr<const ModelAsset>& model){
        auto& f=*static_cast<Fixture*>(raw);++f.template_calls;
        if(f.throw_template)throw std::runtime_error("synthetic template pin");
        if(f.missing_template||model!=f.model_owner)return nullptr;
        if(f.replace_template_at&&f.template_calls==f.replace_template_at)
            f.selected_data=std::make_shared<ModelNodeTemplateData>(*f.data);
        auto wrapper=std::make_shared<ModelNodeTemplate>();
        wrapper->model=f.model_owner;
        if(f.foreign_template)wrapper->model=std::make_shared<ModelAsset>(*f.model_owner);
        wrapper->authored=f.authored;wrapper->data=f.selected_data;
        return wrapper;
    }
    Fixture(){
        selected_data=data;auto& m=memory;m.models[db]=model_owner;
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
            m.put(instance,ivtable);m.put(instance+0x80,db);m.put(instance+0x98,klass);
            auto matrix=placement;if(instance==i2)matrix[9]=.25f;
            m.put(instance+0x44,matrix);m.put(instance+0x74,std::uint8_t{0});
            m.put(instance+0x90,std::uint8_t{0});
            for(const auto offset:{0x9cu,0x9du,0x9eu,0x9fu,0xa0u,0xa1u,0xa4u})
                m.put(instance+offset,std::uint8_t{0xff});
        }
        m.put(i4,std::uint32_t{0xdead});
        m.put(db,dvtable);m.put(db+0x38,std::uint32_t{0xe000});m.name(0xe000,"craft.sod");
        m.put(db+0x3c,std::uint32_t{0x4000});
        m.put(db+0x2c,std::uint32_t{0x4100});m.put(db+0x30,std::uint32_t{0x4200});
        m.put(db+0x34,std::uint32_t{0x4500});
        m.put(ivtable+0x38,setup);m.put(entity_vtable+0x108,divisor_getter);
        m.put(engine_pointer,engine);m.put(engine+0x104,context);m.put(context+0x40,std::uint32_t{1});
        m.put(klass+0x84,std::uint32_t{1});m.put(klass+0x1dc,db);
        m.put(borg,race);m.put(race+0x288,2.f);
        for(const auto entity:{e1,e2}){
            m.put(entity,entity_vtable);m.put(entity+0xfc,race);
            m.put(entity+0x1e0,entity==e1?systems1:systems2);
            m.put(entity+0x1bf,std::uint8_t{0});
            m.put(entity+0x1dc,entity==e1?50.f:100.f);
            m.put(entity+0x1c4,100.f);
        }
        const std::uint32_t system_offsets[]{0,0x30,0x60,0x90,0xc0};
        const std::uint8_t first[]{0,1,2,255,0},second[]{1,0,0,0,0};
        for(unsigned k=0;k<5;++k){m.put(systems1+system_offsets[k],first[k]);
            m.put(systems2+system_offsets[k],second[k]);}
        data->hardpoints=1;data->damage=2;data->lights=5;
        const std::array<const char*,12> names{{"root","hardpoints","damage","d0","d1","lights",
            "crew","life","weapons","engines","sensors","assimilation"}};
        data->nodes.resize(names.size());authored->nodes.resize(names.size());
        for(std::size_t k=0;k<names.size();++k){auto& node=data->nodes[k];auto& pose=authored->nodes[k];
            node.address=0x4000+static_cast<std::uint32_t>(k)*0x100;
            node.vtable=0x7000+static_cast<std::uint32_t>(k)*0x20;
            node.type_function=0x8000+static_cast<std::uint32_t>(k)*0x10;
            node.name_pointer=0x9000+static_cast<std::uint32_t>(k)*0x40;
            node.name=names[k];pose.name=node.name;
            node.parent=k==0?UINT32_MAX:(k==3||k==4?2:0);pose.parent=node.parent;
            node.flags=k==3||k==4?0x81u:0x80u;
            m.put(node.address,node.vtable);m.put(node.vtable+4,node.type_function);
            m.put(node.address+4,db);m.put(node.address+8,node.name_pointer);m.name(node.name_pointer,names[k]);
        }
        auto link=[&](unsigned parent,unsigned child,unsigned sibling){auto& node=data->nodes[child];
            node.parent=parent;node.parent_address=data->nodes[parent].address;
            node.next_sibling=sibling==UINT32_MAX?UINT32_MAX:sibling;
            node.sibling_address=sibling==UINT32_MAX?0:data->nodes[sibling].address;
            authored->nodes[child].parent=parent;};
        data->nodes[0].first_child=1;data->nodes[0].child_address=data->nodes[1].address;
        link(0,1,2);link(0,2,5);link(0,5,6);
        for(unsigned k=6;k<11;++k)link(0,k,k+1);
        link(0,11,UINT32_MAX);
        data->nodes[2].first_child=3;data->nodes[2].child_address=data->nodes[3].address;
        link(2,3,4);link(2,4,UINT32_MAX);
        data->root_address=data->nodes[0].address;
        for(const auto& node:data->nodes){m.put(node.address+0x18,node.parent_address);
            m.put(node.address+0x1c,node.child_address);m.put(node.address+0x20,node.sibling_address);}
        m.put(klass+0x408,data->nodes[2].address);
        m.put(klass+0x420,data->nodes[11].address);
        for(unsigned k=0;k<5;++k)m.put(klass+class_offsets[k],data->nodes[6+k].address);
    }
    void seed(){std::string error;
        need(capture_world_model_associations(model_profile,roster,&memory,Memory::read,this,pin_model,
            models,error),"stable world model baseline");
        need(capture_world_craft_preparations(models,model_profile,preparation_profile,control,
            &memory,Memory::read,this,pin_model,this,pin_class,preparations,error),
            "stable world preparation baseline");
        memory.reads.clear();memory.cached_reads=0;
    }
    bool capture(WorldPreparedVisibility& output,std::string& error,
                 CraftVisibilityReadProfile vp=visibility_profile,
                 CraftPreparationReadProfile pp=preparation_profile){
        return capture_world_prepared_visibility(models,preparations,model_profile,vp,pp,
            &memory,Memory::read,this,pin_model,this,pin_class,this,pin_template,output,error);
    }
};
void rejected(Fixture& f,const char* why,CraftVisibilityReadProfile vp=visibility_profile,
              CraftPreparationReadProfile pp=preparation_profile){
    WorldPreparedVisibility output;output.native_x87_control=0xbeef;output.engine=0xdead;
    WorldPreparedVisibilityMember marker;marker.reason="retained";output.members.push_back(marker);
    std::string error;
    need(!f.capture(output,error,vp,pp)&&!error.empty()&&output.native_x87_control==0xbeef&&
         output.engine==0xdead&&output.members.size()==1&&output.members[0].reason=="retained",why);
}
}
int main(){
    Fixture f;f.seed();WorldPreparedVisibility out;std::string error;
    need(f.models.members.size()==4&&f.preparations.members.size()==4&&
         f.preparations.members[0].status==WorldPreparationStatus::captured&&
         f.preparations.members[1].status==WorldPreparationStatus::captured&&
         f.preparations.members[2].status==WorldPreparationStatus::association_unavailable&&
         f.preparations.members[3].status==WorldPreparationStatus::association_unavailable,
         "two prepared actors and two explicit association-unavailable rows");
    need(f.capture(out,error)&&error.empty()&&out.native_x87_control==control&&
         out.engine==engine&&out.render_context==context&&out.quality==1&&
         out.members.size()==4&&out.members[0].status==WorldPreparedVisibilityStatus::captured&&
         out.members[1].status==WorldPreparedVisibilityStatus::captured&&
         out.members[2].status==WorldPreparedVisibilityStatus::association_unavailable&&
         out.members[3].status==WorldPreparedVisibilityStatus::association_unavailable&&
         out.members[0].capture.preparation&&out.members[1].capture.preparation&&
         out.members[0].capture.baseline!=out.members[1].capture.baseline&&
         out.members[0].capture.baseline->data==out.members[1].capture.baseline->data&&
         out.members[0].capture.input.hull_fraction==.25f&&
         out.members[1].capture.input.hull_fraction==.5f&&
         out.members[0].capture.input.system_hidden[1]&&
         !out.members[1].capture.input.system_hidden[1]&&
         out.members[0].policy.flags[7]==0x82&&out.members[1].policy.flags[7]==0x80&&
         f.memory.cached_reads==0,
         "two actors share immutable template tuple but evaluate independent prepared flags");
    const auto first_wrapper=out.members[0].capture.baseline;
    need(f.capture(out,error)&&out.members[0].capture.baseline!=first_wrapper&&
         out.members[0].capture.baseline->data==first_wrapper->data,
         "fresh wrapper identity with same model/authored/data tuple remains valid");
    std::uint16_t before=0,after=0;__asm__ __volatile__("fnstcw %0":"=m"(before));
    need(f.capture(out,error),"policy evaluation under known source CW");
    __asm__ __volatile__("fnstcw %0":"=m"(after));
    need(before==after,"batch policy restores caller x87 control word");
    need(f.memory.cached_reads==0,"batch never reads stored 9c..a4 outputs");
    Fixture quality_zero;quality_zero.seed();quality_zero.memory.put(context+0x40,std::uint32_t{0});
    need(quality_zero.capture(out,error)&&out.quality==0&&
         out.members[0].status==WorldPreparedVisibilityStatus::captured&&
         !out.members[0].capture.input.context_enabled&&out.members[0].policy.flags[5]==0x83,
         "stable zero quality is captured as disabled context");

    Fixture missing;missing.seed();missing.missing_template=true;
    need(missing.capture(out,error)&&out.members[0].status==WorldPreparedVisibilityStatus::template_unavailable&&
         out.members[1].status==WorldPreparedVisibilityStatus::template_unavailable,
         "missing template is explicit per-member unavailability");
    Fixture foreign;foreign.seed();foreign.foreign_template=true;
    rejected(foreign,"foreign template model owner rejected atomically");
    Fixture bad_prep;bad_prep.memory.put(e2+0x1c4,0.f);bad_prep.seed();
    need(bad_prep.capture(out,error)&&out.members[0].status==WorldPreparedVisibilityStatus::captured&&
         out.members[1].status==WorldPreparedVisibilityStatus::preparation_unavailable,
         "invalid hull gives explicit preparation-unavailable member");
    Fixture stale;stale.seed();stale.memory.put(systems1+0x30,std::uint8_t{0});
    rejected(stale,"stale held logical state rejects whole batch");
    Fixture forged;forged.seed();forged.preparations.members[0].preparation.state.hull_fraction=.9f;
    rejected(forged,"forged held prepared state rejects whole batch");
    Fixture identity;identity.seed();identity.preparations.members[0].member.entity=e2;
    identity.memory.reads.clear();rejected(identity,"misordered held roster rejected");
    need(identity.memory.reads.empty(),"invalid held roster rejects before reads");
    Fixture duplicate;duplicate.seed();duplicate.models.members[1].member.entity=e1;
    duplicate.memory.reads.clear();rejected(duplicate,"duplicate world entity rejected");
    need(duplicate.memory.reads.empty(),"duplicate identity rejects before reads");
    Fixture tuple_change;tuple_change.seed();tuple_change.replace_template_at=3;
    rejected(tuple_change,"shared database template tuple drift rejected");
    Fixture around;around.seed();around.replace_template_at=2;
    rejected(around,"template tuple change around member rejected");
    Fixture owner;owner.seed();owner.owner=class_owner();
    rejected(owner,"class owner replacement rejected");
    Fixture model_change;model_change.seed();model_change.memory.models[db]=model(8);
    rejected(model_change,"model owner replacement rejected by final rewalk");
    Fixture context_change;context_change.seed();context_change.memory.trigger_address=engine_pointer;
    context_change.memory.trigger_at=2;
    context_change.memory.on_read=[](Memory& m){m.put(context+0x40,std::uint32_t{0});};
    rejected(context_change,"shared context quality drift rejected");
    Fixture hidden_change;hidden_change.seed();hidden_change.memory.trigger_address=i1+0x90;
    hidden_change.memory.trigger_at=3;
    hidden_change.memory.on_read=[](Memory& m){m.put(i1+0x90,std::uint8_t{1});};
    rejected(hidden_change,"hidden input drift across sweeps rejected");
    Fixture role_change;role_change.seed();role_change.memory.trigger_address=klass+0x40c;
    role_change.memory.trigger_at=5;
    role_change.memory.on_read=[](Memory& m){m.put(klass+0x40c,std::uint32_t{0x4700});};
    rejected(role_change,"class role drift across sweeps rejected");
    Fixture topology_change;topology_change.seed();topology_change.memory.trigger_address=0x4000+0x18;
    topology_change.memory.trigger_at=5;
    topology_change.memory.on_read=[](Memory& m){m.put(0x4100+0x20,std::uint32_t{0});};
    rejected(topology_change,"node topology drift during scan rejected");
    Fixture roster_change;roster_change.seed();roster_change.memory.trigger_address=engine_pointer;
    roster_change.memory.trigger_at=11;
    roster_change.memory.on_read=[](Memory& m){m.put(n2+8,e1);};
    rejected(roster_change,"final world list rewalk rejects roster drift");
    Fixture placement_change;placement_change.seed();placement_change.memory.trigger_address=engine_pointer;
    placement_change.memory.trigger_at=11;
    placement_change.memory.on_read=[](Memory& m){auto changed=placement;changed[9]=1.f;m.put(i1+0x44,changed);};
    rejected(placement_change,"final world model rewalk rejects placement drift");
    Fixture prep_change;prep_change.seed();prep_change.memory.trigger_address=engine_pointer;
    prep_change.memory.trigger_at=11;
    prep_change.memory.on_read=[](Memory& m){m.put(systems1+0x30,std::uint8_t{0});};
    rejected(prep_change,"final logical preparation rewalk rejects changed systems");
    Fixture throwing;throwing.seed();throwing.throw_template=true;
    rejected(throwing,"throwing template callback preserves output");
    Fixture read_throw;read_throw.seed();read_throw.memory.throw_read=true;
    rejected(read_throw,"throwing read callback preserves output");
    Fixture invalid;invalid.seed();invalid.preparations.native_x87_control=0;
    invalid.memory.reads.clear();rejected(invalid,"unknown source CW rejected");
    need(invalid.memory.reads.empty(),"unknown CW performs no reads");
    Fixture bad_profile;bad_profile.seed();auto vp=visibility_profile;vp.engine_pointer=0;
    bad_profile.memory.reads.clear();rejected(bad_profile,"missing engine pointer rejected",vp);
    need(bad_profile.memory.reads.empty(),"invalid profile performs no reads");
    need(f.memory.cached_reads==0,"successful and repeated batches never read cached outputs");
    std::puts("PASS native11 world prepared Craft visibility");
}
