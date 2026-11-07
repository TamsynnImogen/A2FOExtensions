#include "craft_preparation_capture.hpp"
#include "native_arithmetic.hpp"
#include <array>
#include <cstring>
namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* why) noexcept {try{error=why;}catch(...){}return false;}
struct Reader {
    void* context;Read read;
    bool at(std::uint32_t base,std::uint32_t offset,void* out,std::size_t size)const {
        return base&&size&&offset<=UINT32_MAX-base&&size-1<=UINT32_MAX-base-offset&&
            read(context,base+offset,out,size);
    }
    template<class T>bool get(std::uint32_t base,std::uint32_t offset,T& out)const{return at(base,offset,&out,sizeof(out));}
};
struct Pass {CapturedCraftPreparation value;std::uint32_t name_pointer=0;};
bool pass(const WorldModelAssociation& model,CraftPreparationReadProfile profile,std::uint32_t control,
    Reader reader,void* context,PinLightClass pin,Pass& pending,std::string& error){
    auto& out=pending.value;out.member=model.member;out.model=model.model;out.database=model.database;
    out.native_x87_control=control;out.object_class=pin(context,model.member.object_class);
    if(!out.object_class||out.object_class->class_id!=model.member.object_class||
       out.object_class->version!=model.member.class_version)return fail(error,"Craft preparation class owner unavailable");
    std::uint32_t instance=0,klass=0,vtable=0,target=0,db=0;
    if(!reader.get(out.member.entity,4,instance)||instance!=out.member.instance||
       !reader.get(out.member.entity,0x40,klass)||klass!=out.member.object_class||
       !reader.get(instance,0,vtable)||vtable!=profile.instance_vtable||
       !reader.get(vtable,0x38,target)||target!=profile.setup_function||
       !reader.get(instance,0x98,klass)||klass!=out.member.object_class||
       !reader.get(instance,0x80,db)||db!=model.database||
       !reader.get(db,0,vtable)||vtable!=profile.database_vtable||
       !reader.get(db,0x38,pending.name_pointer)||!pending.name_pointer||
       !reader.get(out.member.entity,0,out.entity_vtable)||!out.entity_vtable||
       !reader.get(out.entity_vtable,0x108,target)||target!=profile.hull_divisor_function)
        return fail(error,"Craft preparation object, class, DB or dispatch mismatch");
    for(std::uint32_t k=0;k<sizeof(model.model_name);++k){char ch=0;
        if(!reader.get(pending.name_pointer,k,ch)||ch!=model.model_name[k])return fail(error,"Craft preparation DB name mismatch");
        if(!ch)break;
    }
    if(!reader.get(profile.borg_cache_pointer,0,out.borg_cache)||!out.borg_cache||
       !reader.get(out.member.entity,0xfc,out.race)||!out.race||
       !reader.get(out.race,0x288,out.input.hull_divisor)||
       !reader.get(out.member.entity,0x1e0,out.systems)||!out.systems||
       !reader.get(out.member.entity,0x1bf,out.input.suppress_system_hiding)||
       !reader.get(out.member.object_class,0x84,out.health_mode))
        return fail(error,"Craft preparation logical source unavailable");
    constexpr std::array<std::uint32_t,5> offsets{{0,0x30,0x60,0x90,0xc0}};
    for(std::size_t k=0;k<offsets.size();++k)
        if(!reader.get(out.systems,offsets[k],out.input.system_state[k]))return fail(error,"Craft preparation system state unavailable");
    out.input.race_matches_borg_cache=out.race==out.borg_cache;
    out.input.use_hull_ratio=out.health_mode!=0;
    if(out.input.use_hull_ratio&&(!reader.get(out.member.entity,0x1dc,out.input.hull_current)||
       !reader.get(out.member.entity,0x1c4,out.input.hull_maximum)))return fail(error,"Craft preparation hull source unavailable");
    if(profile.fleet_cache){
        constexpr std::array<std::uint32_t,4> places{{0xa2,0xa3,0x91,0x92}};
        std::array<std::uint8_t,4> bytes{};
        for(std::size_t k=0;k<places.size();++k)
            if(!reader.get(instance,places[k],bytes[k]))return fail(error,"Craft Fleet cache pointer unavailable");
        std::memcpy(&out.fleet_cache_pointer,bytes.data(),4);
        const auto cache=out.fleet_cache_pointer;
        if(!cache||cache>UINT32_MAX-7||instance>UINT32_MAX-0xa7||
           (std::uint64_t(cache)<std::uint64_t(instance)+0xa8&&std::uint64_t(instance)<std::uint64_t(cache)+8)||
           !reader.get(cache,0,out.fleet_cached_index)||!reader.get(cache,4,out.fleet_cached_texture))
            return fail(error,"Craft Fleet cache overlaps instance or is unavailable");
    }
    if(pin(context,model.member.object_class)!=out.object_class)return fail(error,"Craft preparation class owner changed");
    return prepare_craft_state(out.input,control,out.state,error);
}
bool same(const Pass& a,const Pass& b) noexcept {
    const auto& x=a.value;const auto& y=b.value;const auto& p=x.input;const auto& q=y.input;
    return a.name_pointer==b.name_pointer&&x.object_class==y.object_class&&x.entity_vtable==y.entity_vtable&&
        x.race==y.race&&x.systems==y.systems&&x.borg_cache==y.borg_cache&&x.health_mode==y.health_mode&&
        x.fleet_cache_pointer==y.fleet_cache_pointer&&x.fleet_cached_index==y.fleet_cached_index&&
        x.fleet_cached_texture==y.fleet_cached_texture&&p.system_state==q.system_state&&
        p.suppress_system_hiding==q.suppress_system_hiding&&
        !std::memcmp(&p.hull_current,&q.hull_current,sizeof(float))&&
        !std::memcmp(&p.hull_maximum,&q.hull_maximum,sizeof(float))&&
        !std::memcmp(&p.hull_divisor,&q.hull_divisor,sizeof(float));
}
}
bool same_craft_preparation(const CapturedCraftPreparation& a,const CapturedCraftPreparation& b) noexcept {
    const auto& p=a.input;const auto& q=b.input;
    return a.member.entity==b.member.entity&&a.member.instance==b.member.instance&&
        a.member.object_class==b.member.object_class&&a.member.class_version==b.member.class_version&&
        a.model==b.model&&a.object_class==b.object_class&&a.database==b.database&&
        a.entity_vtable==b.entity_vtable&&a.race==b.race&&a.systems==b.systems&&
        a.borg_cache==b.borg_cache&&a.health_mode==b.health_mode&&
        a.native_x87_control==b.native_x87_control&&a.fleet_cache_pointer==b.fleet_cache_pointer&&
        a.fleet_cached_texture==b.fleet_cached_texture&&a.fleet_cached_index==b.fleet_cached_index&&
        p.system_state==q.system_state&&p.suppress_system_hiding==q.suppress_system_hiding&&
        p.race_matches_borg_cache==q.race_matches_borg_cache&&p.use_hull_ratio==q.use_hull_ratio&&
        !std::memcmp(&p.hull_current,&q.hull_current,sizeof(float))&&
        !std::memcmp(&p.hull_maximum,&q.hull_maximum,sizeof(float))&&
        !std::memcmp(&p.hull_divisor,&q.hull_divisor,sizeof(float))&&
        a.state.systems_hidden==b.state.systems_hidden&&a.state.borg_variant==b.state.borg_variant&&
        !std::memcmp(&a.state.hull_fraction,&b.state.hull_fraction,sizeof(float));
}
bool capture_craft_preparation(const WorldModelAssociation& model,CraftPreparationReadProfile profile,
    std::uint32_t control,void* context,Read read,void* class_context,PinLightClass pin,
    CapturedCraftPreparation& output,std::string& error) noexcept {
    try {
        const auto* end=static_cast<const char*>(std::memchr(model.model_name,0,sizeof(model.model_name)));
        if(!read||!pin||!control||!valid_native_x87_encoding(control)||
           model.status!=WorldModelStatus::bound_current_database||!model.member.entity||!model.member.instance||
           model.member.entity>UINT32_MAX-0x1e3||model.member.instance>UINT32_MAX-0xa7||
           !model.member.object_class||model.member.object_class>UINT32_MAX-0x87||!model.database||
           model.database>UINT32_MAX-0x3b||!model.member.class_version||!model.model||!model.model->info.serial||
           model.model->info.database!=model.database||!model.model->bytes.size()||
           model.model->info.byte_count!=model.model->bytes.size()||!end||end==model.model_name||
           std::strncmp(model.model_name,model.model->info.name,sizeof(model.model_name))||
           !profile.instance_vtable||!profile.database_vtable||profile.instance_vtable==profile.database_vtable||!profile.setup_function||
           !profile.hull_divisor_function||!profile.borg_cache_pointer||
           model.instance_vtable!=profile.instance_vtable||model.database_vtable!=profile.database_vtable)
            return fail(error,"invalid Craft preparation target/profile");
        Pass first,second;
        if(!pass(model,profile,control,{context,read},class_context,pin,first,error)||
           !pass(model,profile,control,{context,read},class_context,pin,second,error))return false;
        if(!same(first,second))return fail(error,"Craft preparation source changed during capture");
        output=std::move(first.value);error.clear();return true;
    }catch(...){return fail(error,"Craft preparation read or allocation failure");}
}
}
