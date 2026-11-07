#include "world_prepared_visibility.hpp"
#include "native_arithmetic.hpp"
#include <cstring>
#include <map>
#include <set>
#include <utility>
namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* why) noexcept {try{error=why;}catch(...){}return false;}
std::uint32_t bits(float value) noexcept {std::uint32_t raw=0;std::memcpy(&raw,&value,4);return raw;}
bool same(const WorldMember& a,const WorldMember& b) noexcept {
    return a.entity==b.entity&&a.instance==b.instance&&a.object_class==b.object_class&&a.class_version==b.class_version;
}
struct TemplateTuple {
    std::shared_ptr<const ModelAsset> model;
    std::shared_ptr<const AuthoredPoseData> authored;
    std::shared_ptr<const ModelNodeTemplateData> data;
};
TemplateTuple tuple(const std::shared_ptr<const ModelNodeTemplate>& value){
    return value?TemplateTuple{value->model,value->authored,value->data}:TemplateTuple{};
}
bool same(const TemplateTuple& a,const TemplateTuple& b) noexcept {
    return a.model==b.model&&a.authored==b.authored&&a.data==b.data;
}
bool usable(const TemplateTuple& value,const WorldModelAssociation& model) noexcept {
    return value.model==model.model&&value.authored&&value.data;
}
bool same(const CraftVisibilityInput& a,const CraftVisibilityInput& b) noexcept {
    if(a.root!=b.root||a.primary_database!=b.primary_database||
       a.database_damage!=b.database_damage||a.damage_child!=b.damage_child||
       a.system_groups!=b.system_groups||a.system_hidden!=b.system_hidden||
       a.assimilation_group!=b.assimilation_group||a.assimilated!=b.assimilated||
       a.base_group!=b.base_group||a.instance_hidden!=b.instance_hidden||
       a.context_enabled!=b.context_enabled||bits(a.hull_fraction)!=bits(b.hull_fraction)||
       a.nodes.size()!=b.nodes.size())return false;
    for(std::size_t k=0;k<a.nodes.size();++k){const auto& x=a.nodes[k];const auto& y=b.nodes[k];
        if(x.parent!=y.parent||x.first_child!=y.first_child||x.next_sibling!=y.next_sibling||x.flags!=y.flags)return false;
    }return true;
}
bool same(const CraftVisibilityResult& a,const CraftVisibilityResult& b) noexcept {
    return a.flags==b.flags&&a.visible==b.visible&&a.low_bit_writes==b.low_bit_writes&&
        bits(a.damage_threshold)==bits(b.damage_threshold);
}
bool same(const WorldPreparedVisibilityMember& a,const WorldPreparedVisibilityMember& b) noexcept {
    if(!same(a.member,b.member)||a.status!=b.status||a.reason!=b.reason)return false;
    if(a.status!=WorldPreparedVisibilityStatus::captured)return true;
    const auto& x=a.capture;const auto& y=b.capture;
    return x.entity==y.entity&&x.instance==y.instance&&x.engine==y.engine&&
        x.render_context==y.render_context&&x.quality==y.quality&&
        x.object_class==y.object_class&&same(tuple(x.baseline),tuple(y.baseline))&&
        x.preparation&&y.preparation&&same_craft_preparation(*x.preparation,*y.preparation)&&
        same(x.input,y.input)&&same(a.policy,b.policy);
}
bool at(void* context,Read read,std::uint32_t base,std::uint32_t offset,void* out,std::size_t size){
    return base&&size&&offset<=UINT32_MAX-base&&size-1<=UINT32_MAX-base-offset&&
        read(context,base+offset,out,size);
}
struct Global {std::uint32_t engine=0,context=0,quality=0;};
bool global(CraftVisibilityReadProfile profile,void* context,Read read,Global& out){
    return at(context,read,profile.engine_pointer,0,&out.engine,4)&&out.engine&&
        at(context,read,out.engine,0x104,&out.context,4)&&
        out.context==profile.render_context&&
        at(context,read,out.context,0x40,&out.quality,4);
}
bool same(const Global& a,const Global& b) noexcept {
    return a.engine==b.engine&&a.context==b.context&&a.quality==b.quality;
}
bool sweep(const WorldModelAssociations& models,const WorldCraftPreparations& preparations,
    CraftVisibilityReadProfile visibility,CraftPreparationReadProfile preparation,
    void* context,Read read,void* class_context,PinLightClass pin_class,
    void* template_context,PinWorldTemplate pin_template,const Global& shared,
    WorldPreparedVisibility& out,std::vector<TemplateTuple>& tuple_pins,std::string& error){
    out.native_x87_control=preparations.native_x87_control;
    out.engine=shared.engine;out.render_context=shared.context;out.quality=shared.quality;
    out.members.reserve(models.members.size());
    tuple_pins.reserve(models.members.size());
    std::map<std::uint32_t,std::pair<bool,TemplateTuple>> by_database;
    for(std::size_t k=0;k<models.members.size();++k){
        const auto& model=models.members[k];const auto& held=preparations.members[k];
        WorldPreparedVisibilityMember row;row.member=model.member;
        TemplateTuple pinned_tuple;
        if(model.status!=WorldModelStatus::bound_current_database){
            row.reason="current model association unavailable";
        }else if(held.status!=WorldPreparationStatus::captured){
            row.status=WorldPreparedVisibilityStatus::preparation_unavailable;
            row.reason=held.reason;
        }else{
            auto baseline=pin_template(template_context,model.model);
            const auto before=tuple(baseline);
            pinned_tuple=before;
            const bool available=bool(baseline);
            if(available&&!usable(before,model))
                return fail(error,"model node template has foreign or incomplete ownership");
            const auto prior=by_database.emplace(model.database,std::make_pair(available,before));
            if(!prior.second&&(prior.first->second.first!=available||
                (available&&!same(prior.first->second.second,before))))
                return fail(error,"shared database template changed across members");
            if(!available){
                row.status=WorldPreparedVisibilityStatus::template_unavailable;
                row.reason="current model node template unavailable";
            }else{
                row.status=capture_prepared_craft_visibility(model,held.preparation,baseline,
                    visibility,preparation,context,read,class_context,pin_class,row.capture,row.reason)?
                    WorldPreparedVisibilityStatus::captured:WorldPreparedVisibilityStatus::visibility_unavailable;
                if(row.status==WorldPreparedVisibilityStatus::captured){
                    if(row.capture.engine!=shared.engine||row.capture.render_context!=shared.context||
                       row.capture.quality!=shared.quality)return fail(error,"shared Craft context changed across members");
                    NativeArithmeticScope arithmetic(preparations.native_x87_control);
                    if(!arithmetic.valid()||!arithmetic.known()||
                       !evaluate_craft_visibility(row.capture.input,row.policy,row.reason))
                        row.status=WorldPreparedVisibilityStatus::visibility_unavailable;
                }
            }
            const auto after=tuple(pin_template(template_context,model.model));
            if(!same(before,after))return fail(error,"model node template owner changed around member");
        }
        tuple_pins.push_back(std::move(pinned_tuple));
        out.members.push_back(std::move(row));
    }
    return true;
}
}
bool capture_world_prepared_visibility(const WorldModelAssociations& models,
    const WorldCraftPreparations& preparations,WorldModelReadProfile model_profile,
    CraftVisibilityReadProfile visibility,CraftPreparationReadProfile preparation,
    void* context,Read read,void* model_context,PinWorldModel pin_model,
    void* class_context,PinLightClass pin_class,void* template_context,PinWorldTemplate pin_template,
    WorldPreparedVisibility& output,std::string& error) noexcept {
    try {
        if(!read||!pin_model||!pin_class||!pin_template||!preparations.native_x87_control||
           !valid_native_x87_encoding(preparations.native_x87_control)||
           models.members.size()>4096||models.members.size()!=preparations.members.size()||
           !model_profile.object_list_pointer||!model_profile.craft_instance_vtable||
           !model_profile.plain_database_vtable||
           model_profile.craft_instance_vtable==model_profile.plain_database_vtable||
           visibility.instance_vtable!=model_profile.craft_instance_vtable||
           visibility.database_vtable!=model_profile.plain_database_vtable||
           !visibility.engine_pointer||!visibility.render_context||
           preparation.instance_vtable!=model_profile.craft_instance_vtable||
           preparation.database_vtable!=model_profile.plain_database_vtable||
           !preparation.setup_function||!preparation.hull_divisor_function||
           !preparation.borg_cache_pointer)return fail(error,"invalid world prepared visibility source/profile");
        std::vector<WorldMember> expected;expected.reserve(models.members.size());
        std::set<std::uint32_t> entities,instances;
        for(std::size_t k=0;k<models.members.size();++k){const auto& model=models.members[k];
            const auto& held=preparations.members[k];const auto& member=model.member;
            if(!member.entity||!member.object_class||!member.class_version||
               !entities.insert(member.entity).second||
               (member.instance&&!instances.insert(member.instance).second)||
               !same(member,held.member)||
               (model.status==WorldModelStatus::bound_current_database&&
                held.status==WorldPreparationStatus::association_unavailable)||
               (model.status!=WorldModelStatus::bound_current_database&&
                held.status!=WorldPreparationStatus::association_unavailable)||
               (held.status==WorldPreparationStatus::captured&&
                (!same(member,held.preparation.member)||held.preparation.model!=model.model||
                 held.preparation.native_x87_control!=preparations.native_x87_control)))
                return fail(error,"world model/preparation roster or ownership mismatch");
            expected.push_back(member);
        }
        Global first_global,middle_global,last_global;
        if(!global(visibility,context,read,first_global))return fail(error,"world Craft context unavailable");
        WorldPreparedVisibility first,second;
        std::vector<TemplateTuple> first_tuples,second_tuples;
        if(!sweep(models,preparations,visibility,preparation,context,read,class_context,pin_class,
            template_context,pin_template,first_global,first,first_tuples,error))return false;
        if(!global(visibility,context,read,middle_global)||!same(first_global,middle_global))
            return fail(error,"world Craft context changed after first sweep");
        if(!sweep(models,preparations,visibility,preparation,context,read,class_context,pin_class,
            template_context,pin_template,middle_global,second,second_tuples,error))return false;
        if(!global(visibility,context,read,last_global)||!same(first_global,last_global))
            return fail(error,"world Craft context changed after second sweep");
        for(std::size_t k=0;k<first.members.size();++k)
            if(!same(first.members[k],second.members[k])||!same(first_tuples[k],second_tuples[k]))
                return fail(error,"world prepared visibility changed across sweeps");
        WorldModelAssociations fresh_models;
        if(!capture_world_model_associations(model_profile,expected,context,read,model_context,pin_model,
            fresh_models,error)||!same_world_model_associations(models,fresh_models))
            return fail(error,"world prepared visibility model/list rewalk changed");
        WorldCraftPreparations fresh_preparations;
        if(!capture_world_craft_preparations(models,model_profile,preparation,
            preparations.native_x87_control,context,read,model_context,pin_model,class_context,pin_class,
            fresh_preparations,error)||!same_world_craft_preparations(preparations,fresh_preparations))
            return fail(error,"world prepared visibility logical rewalk changed");
        output=std::move(first);error.clear();return true;
    }catch(...){return fail(error,"world prepared visibility allocation or callback failure");}
}
}
