#include "../renderer/native11/world_borg_materials.hpp"
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
using namespace a2fo::native11;
namespace {
constexpr std::uint32_t list=0x100,container=0x200,sentinel=0x300,n1=0x400,n2=0x420,n3=0x440;
constexpr std::uint32_t e1=0x1000,e2=0x1300,e3=0x1600,i1=0x2000,i2=0x2200;
constexpr std::uint32_t db=0x3000,root=0x4000,mesh=0x4200,texture=0x6000,alternate=0x6100;
constexpr std::uint32_t craft_vtable=0x9000,db_vtable=0x9004,entity_vtable=0x9100;
constexpr std::uint32_t setup=0x9200,divisor_getter=0x9300,mesh_type=0x9400;
constexpr std::uint32_t render_function=0x9700;
constexpr std::uint32_t klass=0xa000,race=0xb000,race2=0xb300,systems1=0xc000,systems2=0xc200;
constexpr std::uint32_t borg_cache=0xd100,table=0xd200;
constexpr WorldModelReadProfile model_profile{list,craft_vtable,db_vtable};
constexpr CraftPreparationReadProfile preparation_profile{craft_vtable,db_vtable,setup,divisor_getter,borg_cache,false};
constexpr MaterialBaselineReadProfile material_profile{db_vtable,mesh_type};
constexpr CraftBorgMaterialRouteProfile route_profile{craft_vtable,render_function};
const auto control=encode_native_x87(0x037f);
const std::vector<WorldMember> roster{{e1,i1,klass,1},{e2,i2,klass,1},{e3,0,0xa100,2}};
const std::array<float,12> placement{{1,0,0,0,1,0,0,0,1,.25f,0,0}};
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Memory {
    std::map<std::uint32_t,std::uint8_t> bytes;
    std::shared_ptr<ModelAsset> model=std::make_shared<ModelAsset>();
    std::shared_ptr<AuthoredPoseData> authored=std::make_shared<AuthoredPoseData>();
    std::shared_ptr<ModelNodeTemplateData> nodes=std::make_shared<ModelNodeTemplateData>();
    std::shared_ptr<LightClassRecord> klass_owner=std::make_shared<LightClassRecord>();
    std::shared_ptr<const ModelMaterialBaseline> baseline;
    std::function<void(Memory&)> on_read;
    std::uint32_t trigger=0;unsigned trigger_at=0,trigger_count=0,reads=0,cached_reads=0,slot_reads=0,table_reads=0;
    unsigned baseline_pins=0,change_baseline_at=0,texture_pins=0,change_texture_at=0;
    bool no_baseline=false,retire_texture=false,retire_class=false,retire_model=false,throw_read=false;
    bool alternate_wrapper=false;
    std::shared_ptr<const TextureAsset> original,altered;
    template<class T>void put(std::uint32_t at,const T& value){
        const auto* source=reinterpret_cast<const std::uint8_t*>(&value);
        for(std::size_t k=0;k<sizeof(value);++k)bytes[at+static_cast<std::uint32_t>(k)]=source[k];
    }
    void str(std::uint32_t at,const char* value){
        for(std::size_t k=0;k<=std::strlen(value);++k)put(at+static_cast<std::uint32_t>(k),value[k]);
    }
    static bool read(void* raw,std::uint32_t at,void* output,std::size_t count){
        auto& m=*static_cast<Memory*>(raw);++m.reads;
        if(m.throw_read)throw std::runtime_error("synthetic read failure");
        if(at==m.trigger&&++m.trigger_count==m.trigger_at&&m.on_read)m.on_read(m);
        for(const auto instance:{i1,i2})
            for(const auto offset:{0x9cu,0x9du,0x9eu,0x9fu,0xa0u,0xa1u,0xa4u})
                if(at==instance+offset)++m.cached_reads;
        if(at==mesh+0x114||at==mesh+0x11c||(at>=0x8000&&at<0x8010))++m.slot_reads;
        if(at==table||at==table+4)++m.table_reads;
        auto* dest=static_cast<std::uint8_t*>(output);
        for(std::size_t k=0;k<count;++k){if(k>UINT32_MAX-at)return false;
            const auto found=m.bytes.find(at+static_cast<std::uint32_t>(k));
            if(found==m.bytes.end())return false;
            dest[k]=found->second;
        }return true;
    }
    static std::shared_ptr<const ModelAsset> pin_model(void* raw,std::uint32_t database,const char* name){
        auto& m=*static_cast<Memory*>(raw);
        return !m.retire_model&&database==db&&std::strcmp(name,"ship.sod")==0?m.model:nullptr;
    }
    static std::shared_ptr<const LightClassRecord> pin_class(void* raw,std::uint64_t id){
        auto& m=*static_cast<Memory*>(raw);
        return !m.retire_class&&id==klass?m.klass_owner:nullptr;
    }
    static std::shared_ptr<const TextureAsset> pin_texture(void* raw,std::uint32_t id){
        auto& m=*static_cast<Memory*>(raw);++m.texture_pins;
        if(m.change_texture_at&&m.texture_pins==m.change_texture_at)
            m.altered=std::make_shared<TextureAsset>(*m.altered);
        if(m.retire_texture)return nullptr;
        return id==texture?m.original:id==alternate?m.altered:nullptr;
    }
    static std::shared_ptr<const ModelMaterialBaseline> pin_baseline(void* raw,
        const std::shared_ptr<const ModelAsset>& owner){
        auto& m=*static_cast<Memory*>(raw);++m.baseline_pins;
        if(m.change_baseline_at&&m.baseline_pins==m.change_baseline_at)
            m.no_baseline=true;
        if(m.no_baseline||owner!=m.model)return nullptr;
        if(!m.alternate_wrapper)return m.baseline;
        return std::make_shared<ModelMaterialBaseline>(*m.baseline);
    }
    static std::shared_ptr<const TextureAsset> texture_owner(std::uint32_t id,
        std::uint64_t serial,const char* name){
        auto result=std::make_shared<TextureAsset>();result->texture=id;result->serial=serial;
        result->logical_name=name;
        result->bytes=std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{1,2,3});
        return result;
    }
    Memory(){
        model->info.database=db;model->info.serial=7;model->bytes={1,2,3};
        model->info.byte_count=3;std::strcpy(model->info.name,"ship.sod");
        klass_owner->class_id=klass;klass_owner->version=1;
        put(list,container);put(container+4,sentinel);put(sentinel,n1);put(sentinel+4,n3);
        const std::uint32_t links[]{n1,n2,n3};const std::uint32_t entities[]{e1,e2,e3};
        for(unsigned k=0;k<3;++k){put(links[k],k==2?sentinel:links[k+1]);
            put(links[k]+4,k?links[k-1]:sentinel);put(links[k]+8,entities[k]);
            put(entities[k]+4,roster[k].instance);put(entities[k]+0x40,roster[k].object_class);
        }
        put(db,db_vtable);put(db+0x38,std::uint32_t{0x3500});str(0x3500,"ship.sod");
        put(db+0x3c,root);for(const auto offset:{0x2cu,0x30u,0x34u})put(db+offset,std::uint32_t{0});
        for(const auto instance:{i1,i2}){put(instance,craft_vtable);put(instance+0x44,placement);
            put(instance+0x74,std::uint8_t{0});put(instance+0x80,db);put(instance+0x98,klass);
            for(const auto offset:{0x9cu,0x9du,0x9eu,0x9fu,0xa0u,0xa1u,0xa4u})
                put(instance+offset,std::uint8_t{0xff});
        }
        put(craft_vtable+0x14,render_function);put(craft_vtable+0x38,setup);
        put(entity_vtable+0x108,divisor_getter);
        put(klass+0x84,std::uint32_t{1});put(klass,std::uint32_t{0xab00});
        put(klass+0x1d8,db);put(klass+0x1dc,db);
        put(klass+0x424,std::int32_t{1});put(klass+0x428,table);
        put(table,mesh);put(table+4,texture);
        put(borg_cache,race);put(race+0x288,2.f);put(race2+0x288,2.f);
        for(const auto entity:{e1,e2}){put(entity,entity_vtable);
            put(entity+0xfc,entity==e1?race:race2);
            put(entity+0x1e0,entity==e1?systems1:systems2);
            put(entity+0x1bf,std::uint8_t{0});put(entity+0x1dc,50.f);put(entity+0x1c4,100.f);
        }
        for(const auto systems:{systems1,systems2})
            for(const auto offset:{0u,0x30u,0x60u,0x90u,0xc0u})put(systems+offset,std::uint8_t{0});
        put(systems2,std::uint8_t{1});
        nodes->nodes.resize(2);authored->nodes.resize(2);
        const std::uint32_t addresses[]{root,mesh};const char* names[]{"root","mesh"};
        for(unsigned k=0;k<2;++k){auto& node=nodes->nodes[k];auto& pose=authored->nodes[k];
            node.address=addresses[k];node.vtable=0x9500+0x20*k;
            node.type_function=k?mesh_type:0x9600;node.name_pointer=0x5000+0x40*k;
            node.name=names[k];node.parent=k?0:UINT32_MAX;node.parent_address=k?root:0;
            pose.name=node.name;pose.parent=node.parent;pose.kind=k?1:0;
            put(node.address,node.vtable);put(node.vtable+4,node.type_function);
            put(node.address+4,db);put(node.address+8,node.name_pointer);str(node.name_pointer,names[k]);
        }
        nodes->nodes[0].first_child=1;nodes->nodes[0].child_address=mesh;nodes->root_address=root;
        for(const auto& node:nodes->nodes){put(node.address+0x18,node.parent_address);
            put(node.address+0x1c,node.child_address);put(node.address+0x20,node.sibling_address);}
        original=texture_owner(texture,11,"one.dds");altered=texture_owner(alternate,12,"borg.dds");
        put(texture+8,std::uint32_t{0x7000});str(0x7000,"one.dds");
        put(alternate+8,std::uint32_t{0x7100});str(0x7100,"borg.dds");
        put(texture+0x18,std::uint32_t{0});put(alternate+0x18,std::uint32_t{0});
        put(texture+0x2c,std::uint8_t{0});put(alternate+0x2c,std::uint8_t{0});
        put(mesh+0x114,std::uint32_t{0x8000});put(mesh+0x11c,std::uint32_t{4});
        put(mesh+0x13c,alternate);put(0x8000,texture);
        for(unsigned slot=1;slot<4;++slot)put(0x8000+4*slot,std::uint32_t{0});
        ModelNodeTemplate source{model,authored,nodes};std::string error;
        need(capture_model_material_baseline(source,material_profile,this,read,this,pin_texture,
            baseline,error),"completed-load material baseline fixture");
        slot_reads=reads=0;
    }
    WorldModelAssociations models(){WorldModelAssociations output;std::string error;
        need(capture_world_model_associations(model_profile,roster,this,read,this,pin_model,
            output,error),"world model fixture");return output;
    }
    WorldCraftPreparations preparations(const WorldModelAssociations& models){
        WorldCraftPreparations output;std::string error;
        need(capture_world_craft_preparations(models,model_profile,preparation_profile,control,
            this,read,this,pin_model,this,pin_class,output,error),"world preparation fixture");
        return output;
    }
    bool capture(const WorldModelAssociations& models,const WorldCraftPreparations& preps,
        WorldBorgMaterials& out,std::string& error,std::uint32_t cw=control){
        auto supplied=preps;supplied.native_x87_control=cw;
        return capture_world_borg_materials(models,supplied,model_profile,preparation_profile,
            material_profile,route_profile,this,read,this,pin_model,this,pin_class,this,pin_baseline,
            this,pin_texture,out,error);
    }
};
void rejected(Memory& m,const WorldModelAssociations& models,const WorldCraftPreparations& preps,
    const char* why,std::uint32_t cw=control){
    WorldBorgMaterials output;output.native_x87_control=0xdead;
    WorldBorgMaterialMember marker;marker.reason="retained";output.members.push_back(marker);
    std::string error;
    need(!m.capture(models,preps,output,error,cw)&&!error.empty()&&
        output.native_x87_control==0xdead&&output.members.size()==1&&
        output.members[0].reason=="retained",why);
}
}
int main(){
    Memory m;const auto models=m.models();const auto preps=m.preparations(models);
    WorldBorgMaterials out;std::string error;
    need(m.capture(models,preps,out,error)&&error.empty()&&out.members.size()==3&&
         out.members[0].status==WorldBorgMaterialStatus::correlated&&
         out.members[1].status==WorldBorgMaterialStatus::correlated&&
         out.members[2].status==WorldBorgMaterialStatus::association_unavailable&&
         out.members[0].writes.size()==1&&out.members[0].writes[0].slot==0&&
         out.members[0].writes[0].texture==alternate&&
         out.members[1].writes.size()==2&&out.members[1].writes[0].slot==3&&
         out.members[1].writes[1].slot==0&&
         out.members[1].writes[0].texture==texture&&out.members[1].writes[1].texture==texture&&
         out.members[0].baseline->model==models.members[0].model&&
         out.members[1].baseline->model==models.members[1].model&&
         out.members[0].route_valid&&out.members[0].route.selector_reached&&
         out.members[0].route.instance_vtable==craft_vtable&&
         out.members[0].route.primary_database==db&&
         out.members[1].route_valid&&out.members[1].route.selector_reached&&
         m.slot_reads==0&&m.cached_reads==0,
         "two bound actors share DB/table but fresh Borg variant yields distinct ordered writes");
    CraftBorgMaterialRoute route;route.class_vtable=0xdead;
    need(capture_craft_borg_material_route(preps.members[0].preparation,route_profile,
         &m,Memory::read,&m,Memory::pin_class,route,error)&&error.empty()&&
         route.instance_vtable==craft_vtable&&route.class_vtable==0xab00&&
         route.primary_database==db&&route.selector_reached,
         "direct Craft route qualifies pinned dispatch and primary database");
    m.put(klass+0x1dc,std::uint32_t{0x3500});m.put(table+4,alternate);
    m.table_reads=0;
    need(m.capture(models,preps,out,error)&&error.empty()&&
         out.members[0].status==WorldBorgMaterialStatus::selector_bypassed&&
         out.members[1].status==WorldBorgMaterialStatus::selector_bypassed&&
         out.members[0].route_valid&&!out.members[0].route.selector_reached&&
         out.members[1].route_valid&&!out.members[1].route.selector_reached&&
         out.members[0].writes.empty()&&out.members[1].writes.empty()&&
         m.table_reads==0,
         "nonprimary Craft database bypasses selector before poisoned table read");
    m.put(klass+0x1dc,db);m.put(craft_vtable+0x14,std::uint32_t{0x9750});
    m.table_reads=0;
    need(m.capture(models,preps,out,error)&&error.empty()&&
         out.members[0].status==WorldBorgMaterialStatus::route_unavailable&&
         out.members[1].status==WorldBorgMaterialStatus::route_unavailable&&
         !out.members[0].route_valid&&!out.members[1].route_valid&&
         out.members[0].writes.empty()&&out.members[1].writes.empty()&&
         m.table_reads==0,
         "unknown Craft render dispatch gives no invented material writes");
    CraftBorgMaterialRoute retained;retained.class_vtable=0xdead;
    need(!capture_craft_borg_material_route(preps.members[0].preparation,route_profile,
         &m,Memory::read,&m,Memory::pin_class,retained,error)&&!error.empty()&&
         retained.class_vtable==0xdead,
         "unknown Craft render dispatch preserves direct route output");
    m.put(craft_vtable+0x14,render_function);m.put(table+4,texture);
    m.trigger=klass+0x1dc;m.trigger_at=2;m.trigger_count=0;
    m.on_read=[](Memory& x){x.put(klass+0x1dc,std::uint32_t{0x3500});};
    need(!capture_craft_borg_material_route(preps.members[0].preparation,route_profile,
         &m,Memory::read,&m,Memory::pin_class,retained,error)&&!error.empty()&&
         retained.class_vtable==0xdead&&m.trigger_count==2,
         "primary database change between route passes preserves output");
    m.trigger=0;m.on_read={};m.put(klass+0x1dc,db);
    m.trigger=craft_vtable+0x14;m.trigger_at=2;m.trigger_count=0;
    m.on_read=[](Memory& x){x.put(craft_vtable+0x14,std::uint32_t{0x9750});};
    need(!capture_craft_borg_material_route(preps.members[0].preparation,route_profile,
         &m,Memory::read,&m,Memory::pin_class,retained,error)&&!error.empty()&&
         retained.class_vtable==0xdead&&m.trigger_count==2,
         "render dispatch change between route passes preserves output");
    m.trigger=0;m.on_read={};m.put(craft_vtable+0x14,render_function);
    m.trigger=klass+0x1dc;m.trigger_at=9;m.trigger_count=0;
    m.on_read=[](Memory& x){x.put(klass+0x1dc,std::uint32_t{0x3500});};
    rejected(m,models,preps,"primary database change across world sweeps rejected");
    need(m.trigger_count>=9,"cross-sweep primary database mutation exercised");
    m.trigger=0;m.on_read={};m.put(klass+0x1dc,db);
    m.trigger=craft_vtable+0x14;m.trigger_at=9;m.trigger_count=0;
    m.on_read=[](Memory& x){x.put(craft_vtable+0x14,std::uint32_t{0x9750});};
    rejected(m,models,preps,"render dispatch change across world sweeps rejected");
    need(m.trigger_count>=9,"cross-sweep dispatch mutation exercised");
    m.trigger=0;m.on_read={};m.put(craft_vtable+0x14,render_function);
    m.trigger=klass+0x1dc;m.trigger_at=17;m.trigger_count=0;
    m.on_read=[](Memory& x){x.put(klass+0x1dc,std::uint32_t{0x3500});};
    rejected(m,models,preps,"primary database change after world sweeps rejected");
    need(m.trigger_count>=17,"final route rewalk mutation exercised");
    m.trigger=0;m.on_read={};m.put(klass+0x1dc,db);
    m.trigger=craft_vtable+0x14;m.trigger_at=17;m.trigger_count=0;
    m.on_read=[](Memory& x){x.put(craft_vtable+0x14,std::uint32_t{0x9750});};
    rejected(m,models,preps,"render dispatch change after world sweeps rejected");
    need(m.trigger_count>=17,"final dispatch rewalk mutation exercised");
    m.trigger=0;m.on_read={};m.put(craft_vtable+0x14,render_function);
    m.alternate_wrapper=true;
    need(m.capture(models,preps,out,error)&&out.members[0].status==WorldBorgMaterialStatus::correlated,
         "fresh baseline wrappers with identical owner tuple accepted");
    m.alternate_wrapper=false;m.no_baseline=true;
    need(m.capture(models,preps,out,error)&&out.members[0].status==WorldBorgMaterialStatus::baseline_unavailable&&
         out.members[1].status==WorldBorgMaterialStatus::baseline_unavailable,
         "missing baseline explicit for each bound actor");
    m.no_baseline=false;m.put(table+4,alternate);
    need(m.capture(models,preps,out,error)&&out.members[0].status==WorldBorgMaterialStatus::table_unavailable&&
         out.members[1].status==WorldBorgMaterialStatus::table_unavailable,
         "bad native table remains explicit unavailability, never guessed material");
    m.put(table+4,texture);
    m.trigger=table;m.trigger_at=3;m.trigger_count=0;m.on_read=[](Memory& x){x.put(table+4,alternate);};
    rejected(m,models,preps,"table row changed between world sweeps rejected");
    m.trigger=0;m.on_read={};m.put(table+4,texture);
    m.baseline_pins=0;m.change_baseline_at=2;
    rejected(m,models,preps,"baseline availability changed around actor rejected");
    m.change_baseline_at=0;m.no_baseline=false;m.baseline_pins=0;
    m.retire_texture=true;
    need(m.capture(models,preps,out,error)&&
         out.members[0].status==WorldBorgMaterialStatus::table_unavailable&&
         out.members[1].status==WorldBorgMaterialStatus::table_unavailable,
         "texture retirement makes checked table explicitly unavailable");
    m.retire_texture=false;m.retire_class=true;rejected(m,models,preps,"class owner revocation rejects held preparation");
    m.retire_class=false;m.retire_model=true;rejected(m,models,preps,"model owner revocation rejected by final list rewalk");
    m.retire_model=false;m.put(i1+0x44,std::array<float,12>{{1,0,0,0,1,0,0,0,1,2,0,0}});
    rejected(m,models,preps,"placement change rejected by final model rewalk");
    m.put(i1+0x44,placement);m.put(systems1,std::uint8_t{1});
    rejected(m,models,preps,"held logical Borg variant change rejected");
    m.put(systems1,std::uint8_t{0});m.put(n2+8,e1);
    rejected(m,models,preps,"changed world roster rejected");
    m.put(n2+8,e2);m.throw_read=true;
    rejected(m,models,preps,"throwing read callback preserves output");
    m.throw_read=false;m.reads=0;
    rejected(m,models,preps,"unknown x87 control rejected",0);
    need(m.reads==0,"invalid control causes no reads");
    auto forged=preps;forged.members[0].preparation.state.borg_variant=false;
    rejected(m,models,forged,"forged held Borg state rejected");
    auto duplicate=models;duplicate.members[1].member.entity=duplicate.members[0].member.entity;
    m.reads=0;rejected(m,duplicate,preps,"duplicate entity rejected before reads");
    need(m.reads==0,"duplicate roster performs no reads");
    auto oversized=models;oversized.members.resize(4097);
    m.reads=0;rejected(m,oversized,preps,"oversized roster rejected before reads");
    need(m.reads==0,"oversized roster performs no reads");
    std::puts("PASS native11 world Borg material correlation");
}
