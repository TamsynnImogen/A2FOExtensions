#include "world_preparation_capture.hpp"
#include "native_arithmetic.hpp"
#include <cstring>
#include <map>
#include <set>
#include <utility>
namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* why) noexcept {try{error=why;}catch(...){}return false;}
std::uint32_t bits(float value)noexcept{std::uint32_t result=0;std::memcpy(&result,&value,4);return result;}
bool sweep(const WorldModelAssociations& models,CraftPreparationReadProfile profile,std::uint32_t control,
    void* context,Read read,void* class_context,PinLightClass pin,WorldCraftPreparations& out,std::string& error){
    out.native_x87_control=control;out.members.reserve(models.members.size());
    std::map<std::uint32_t,std::pair<std::shared_ptr<const LightClassRecord>,std::uint32_t>> classes;
    std::map<std::uint32_t,std::uint32_t> races;
    std::uint32_t borg=0;
    for(const auto& model:models.members){WorldCraftPreparation row;row.member=model.member;
        if(model.status!=WorldModelStatus::bound_current_database)row.reason="current model association unavailable";
        else row.status=capture_craft_preparation(model,profile,control,context,read,class_context,pin,row.preparation,row.reason)?
            WorldPreparationStatus::captured:WorldPreparationStatus::preparation_unavailable;
        if(row.status==WorldPreparationStatus::captured){const auto& data=row.preparation;
            if(borg&&borg!=data.borg_cache)return fail(error,"shared Borg reference changed across world preparation");
            borg=data.borg_cache;
            const auto klass=classes.emplace(row.member.object_class,std::make_pair(data.object_class,data.health_mode));
            if(!klass.second&&(klass.first->second.first!=data.object_class||klass.first->second.second!=data.health_mode))
                return fail(error,"shared class changed across world preparation");
            const auto race=races.emplace(data.race,bits(data.input.hull_divisor));
            if(!race.second&&race.first->second!=bits(data.input.hull_divisor))
                return fail(error,"shared race divisor changed across world preparation");
        }
        out.members.push_back(std::move(row));
    }return true;
}
}
bool same_world_craft_preparations(const WorldCraftPreparations& a,const WorldCraftPreparations& b) noexcept {
    if(a.native_x87_control!=b.native_x87_control||a.members.size()!=b.members.size())return false;
    for(std::size_t k=0;k<a.members.size();++k){const auto& x=a.members[k];const auto& y=b.members[k];
        if(x.member.entity!=y.member.entity||x.member.instance!=y.member.instance||
           x.member.object_class!=y.member.object_class||x.member.class_version!=y.member.class_version||
           x.status!=y.status||x.reason!=y.reason||
           (x.status==WorldPreparationStatus::captured&&!same_craft_preparation(x.preparation,y.preparation)))return false;
    }return true;
}
bool capture_world_craft_preparations(const WorldModelAssociations& models,WorldModelReadProfile model_profile,
    CraftPreparationReadProfile profile,std::uint32_t control,void* context,Read read,
    void* model_context,PinWorldModel pin_model,void* class_context,PinLightClass pin_class,
    WorldCraftPreparations& output,std::string& error) noexcept {
    try {
        if(!read||!pin_model||!pin_class||!control||!valid_native_x87_encoding(control)||models.members.size()>4096||
           !model_profile.object_list_pointer||!model_profile.craft_instance_vtable||!model_profile.plain_database_vtable||
           model_profile.craft_instance_vtable==model_profile.plain_database_vtable||
           profile.instance_vtable!=model_profile.craft_instance_vtable||profile.database_vtable!=model_profile.plain_database_vtable||
           !profile.setup_function||!profile.hull_divisor_function||!profile.borg_cache_pointer)
            return fail(error,"invalid world preparation source/profile");
        std::vector<WorldMember> expected;expected.reserve(models.members.size());std::set<std::uint32_t> entities,instances;
        for(const auto& model:models.members){const auto& member=model.member;
            if(!member.entity||!member.object_class||!member.class_version||!entities.insert(member.entity).second||
               (member.instance&&!instances.insert(member.instance).second))return fail(error,"invalid world preparation roster");
            expected.push_back(member);
        }
        WorldCraftPreparations first,second;
        if(!sweep(models,profile,control,context,read,class_context,pin_class,first,error)||
           !sweep(models,profile,control,context,read,class_context,pin_class,second,error))return false;
        for(std::size_t k=0;k<first.members.size();++k){const auto& a=first.members[k];const auto& b=second.members[k];
            if(a.status!=b.status||a.reason!=b.reason||(a.status==WorldPreparationStatus::captured&&!same_craft_preparation(a.preparation,b.preparation)))
                return fail(error,"world preparation changed across sweeps");
        }
        WorldModelAssociations fresh;
        if(!capture_world_model_associations(model_profile,expected,context,read,model_context,pin_model,fresh,error))return false;
        if(!same_world_model_associations(models,fresh))return fail(error,"world preparation model/list association changed");
        output=std::move(first);error.clear();return true;
    }catch(...){return fail(error,"world preparation allocation or callback failure");}
}
}
