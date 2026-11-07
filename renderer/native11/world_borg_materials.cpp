#include "world_borg_materials.hpp"
#include "native_arithmetic.hpp"
#include <map>
#include <set>
#include <utility>
namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* why) noexcept {try{error=why;}catch(...){}return false;}
bool same(const WorldMember& a,const WorldMember& b) noexcept {
    return a.entity==b.entity&&a.instance==b.instance&&
        a.object_class==b.object_class&&a.class_version==b.class_version;
}
bool usable(const std::shared_ptr<const ModelMaterialBaseline>& value,
    const WorldModelAssociation& model) noexcept {
    return value&&value->model==model.model&&value->authored&&value->nodes&&value->data;
}
struct ModelAdapter {void* context;PinWorldModel pin;const char* name;};
std::shared_ptr<const ModelAsset> pin_by_serial(void* raw,std::uint32_t database,
    std::uint64_t serial){
    const auto& adapter=*static_cast<const ModelAdapter*>(raw);
    auto model=adapter.pin(adapter.context,database,adapter.name);
    return model&&model->info.database==database&&model->info.serial==serial?model:nullptr;
}
bool same_rows(const std::vector<CraftBorgMaterialRow>& a,
    const std::vector<CraftBorgMaterialRow>& b) noexcept {
    if(a.size()!=b.size())return false;
    for(std::size_t k=0;k<a.size();++k)
        if(a[k].node_index!=b[k].node_index||a[k].original_texture!=b[k].original_texture||
           a[k].borg_texture!=b[k].borg_texture||a[k].slot_count!=b[k].slot_count)return false;
    return true;
}
bool same_writes(const std::vector<CraftMaterialSlotWrite>& a,
    const std::vector<CraftMaterialSlotWrite>& b) noexcept {
    if(a.size()!=b.size())return false;
    for(std::size_t k=0;k<a.size();++k)
        if(a[k].node_index!=b[k].node_index||a[k].slot!=b[k].slot||
           a[k].texture!=b[k].texture)return false;
    return true;
}
bool same(const WorldBorgMaterialMember& a,const WorldBorgMaterialMember& b) noexcept {
    if(!same(a.member,b.member)||a.status!=b.status||a.reason!=b.reason||
       !same_material_baseline_owners(a.baseline,b.baseline))return false;
    if(a.route_valid!=b.route_valid||!same_craft_borg_material_route(a.route,b.route)||
       bool(a.preparation)!=bool(b.preparation)||
       (a.preparation&&!same_craft_preparation(*a.preparation,*b.preparation)))return false;
    return a.table.object_class==b.table.object_class&&
        same_material_baseline_owners(a.table.baseline,b.table.baseline)&&
        a.table.class_vtable==b.table.class_vtable&&
        a.table.table_pointer==b.table.table_pointer&&
        same_rows(a.table.rows,b.table.rows)&&same_writes(a.writes,b.writes);
}
struct SharedClassTable {
    std::shared_ptr<const LightClassRecord> owner;
    std::uint32_t vtable=0,pointer=0;
    std::vector<CraftBorgMaterialRow> rows;
};
bool sweep(const WorldModelAssociations& models,const WorldCraftPreparations& preparations,
    CraftPreparationReadProfile prep_profile,MaterialBaselineReadProfile material_profile,
    CraftBorgMaterialRouteProfile route_profile,
    void* read_context,Read read,void* model_context,PinWorldModel pin_model,
    void* class_context,PinLightClass pin_class,void* baseline_context,
    PinWorldMaterialBaseline pin_baseline,void* texture_context,ResolveTextureAsset resolve,
    WorldBorgMaterials& out,std::string& error){
    out.native_x87_control=preparations.native_x87_control;
    out.members.reserve(models.members.size());
    std::map<std::uint32_t,std::pair<bool,std::shared_ptr<const ModelMaterialBaseline>>> shared_db;
    std::map<std::uint32_t,SharedClassTable> shared_class;
    for(std::size_t k=0;k<models.members.size();++k){
        const auto& model=models.members[k];const auto& held=preparations.members[k];
        WorldBorgMaterialMember row;row.member=model.member;
        if(model.status!=WorldModelStatus::bound_current_database)
            row.reason="current model association unavailable";
        else if(held.status!=WorldPreparationStatus::captured){
            row.status=WorldBorgMaterialStatus::preparation_unavailable;row.reason=held.reason;
        }else{
            auto baseline=pin_baseline(baseline_context,model.model);
            const auto before=baseline;
            if(before&&!usable(before,model))
                return fail(error,"foreign or incomplete world material baseline");
            const auto prior=shared_db.emplace(model.database,std::make_pair(bool(before),before));
            if(!prior.second&&(prior.first->second.first!=bool(before)||
                !same_material_baseline_owners(prior.first->second.second,before)))
                return fail(error,"shared database material baseline changed");
            row.baseline=before;
            if(!before){
                row.status=WorldBorgMaterialStatus::baseline_unavailable;
                row.reason="current model material baseline unavailable";
            }else{
                CapturedCraftPreparation fresh_before,fresh_after;
                if(!capture_craft_preparation(model,prep_profile,preparations.native_x87_control,
                    read_context,read,class_context,pin_class,fresh_before,error)||
                   !same_craft_preparation(fresh_before,held.preparation))
                    return fail(error,"held Craft preparation changed before material table");
                row.preparation=std::make_shared<const CapturedCraftPreparation>(fresh_before);
                ModelAdapter adapter{model_context,pin_model,model.model_name};
                row.route_valid=capture_craft_borg_material_route(fresh_before,route_profile,
                    read_context,read,class_context,pin_class,row.route,row.reason);
                if(!row.route_valid)row.status=WorldBorgMaterialStatus::route_unavailable;
                else if(!row.route.selector_reached){
                    row.status=WorldBorgMaterialStatus::selector_bypassed;
                    row.reason="base Craft render path bypasses Borg selector for current database";
                }else row.status=capture_craft_borg_table(fresh_before.object_class,before,
                    material_profile,read_context,read,&adapter,pin_by_serial,
                    class_context,pin_class,texture_context,resolve,row.table,row.reason)?
                    WorldBorgMaterialStatus::correlated:WorldBorgMaterialStatus::table_unavailable;
                if(row.status==WorldBorgMaterialStatus::correlated){
                    if(row.table.class_vtable!=row.route.class_vtable)
                        return fail(error,"material class dispatch changed around table");
                    if(!craft_borg_material_writes(row.table.rows,
                        fresh_before.state.borg_variant,row.writes,row.reason))
                        row.status=WorldBorgMaterialStatus::table_unavailable;
                    else {
                        const auto inserted=shared_class.emplace(model.member.object_class,
                            SharedClassTable{row.table.object_class,row.table.class_vtable,
                                row.table.table_pointer,row.table.rows});
                        if(!inserted.second){const auto& old=inserted.first->second;
                            if(old.owner!=row.table.object_class||old.vtable!=row.table.class_vtable||
                               old.pointer!=row.table.table_pointer||!same_rows(old.rows,row.table.rows))
                                return fail(error,"shared class material table changed");
                        }
                    }
                }
                if(!capture_craft_preparation(model,prep_profile,preparations.native_x87_control,
                    read_context,read,class_context,pin_class,fresh_after,error)||
                   !same_craft_preparation(fresh_before,fresh_after)||
                   !same_craft_preparation(fresh_after,held.preparation))
                    return fail(error,"Craft preparation changed around material table");
                if(row.route_valid){
                    CraftBorgMaterialRoute after;
                    if(!capture_craft_borg_material_route(fresh_after,route_profile,read_context,
                        read,class_context,pin_class,after,error)||
                       !same_craft_borg_material_route(row.route,after))
                        return fail(error,"Craft material route changed around table");
                }
            }
            baseline=pin_baseline(baseline_context,model.model);
            if(!same_material_baseline_owners(before,baseline))
                return fail(error,"material baseline owner changed around member");
        }
        out.members.push_back(std::move(row));
    }
    return true;
}
}
bool capture_world_borg_materials(const WorldModelAssociations& models,
    const WorldCraftPreparations& preparations,WorldModelReadProfile model_profile,
    CraftPreparationReadProfile prep_profile,MaterialBaselineReadProfile material_profile,
    CraftBorgMaterialRouteProfile route_profile,
    void* read_context,Read read,void* model_context,PinWorldModel pin_model,
    void* class_context,PinLightClass pin_class,void* baseline_context,
    PinWorldMaterialBaseline pin_baseline,void* texture_context,ResolveTextureAsset resolve,
    WorldBorgMaterials& output,std::string& error) noexcept {
    try {
        if(!read||!pin_model||!pin_class||!pin_baseline||!resolve||
           !valid_native_x87_encoding(preparations.native_x87_control)||
           !model_profile.object_list_pointer||!model_profile.craft_instance_vtable||
           !model_profile.plain_database_vtable||
           model_profile.craft_instance_vtable==model_profile.plain_database_vtable||
           prep_profile.instance_vtable!=model_profile.craft_instance_vtable||
           prep_profile.database_vtable!=model_profile.plain_database_vtable||
           !prep_profile.setup_function||!prep_profile.hull_divisor_function||
           !prep_profile.borg_cache_pointer||!material_profile.database_vtable||
           !material_profile.mesh_type_function||
           route_profile.instance_vtable!=model_profile.craft_instance_vtable||
           !route_profile.render_function||
           material_profile.database_vtable!=model_profile.plain_database_vtable||
           models.members.size()>4096||models.members.size()!=preparations.members.size())
            return fail(error,"invalid world Borg material source/profile");
        std::vector<WorldMember> expected;expected.reserve(models.members.size());
        std::set<std::uint32_t> entities,instances;
        for(std::size_t k=0;k<models.members.size();++k){
            const auto& model=models.members[k];const auto& held=preparations.members[k];
            const auto& member=model.member;
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
                return fail(error,"world Borg material roster or held preparation mismatch");
            expected.push_back(member);
        }
        WorldBorgMaterials first,second;
        if(!sweep(models,preparations,prep_profile,material_profile,route_profile,read_context,read,
            model_context,pin_model,class_context,pin_class,baseline_context,pin_baseline,
            texture_context,resolve,first,error)||
           !sweep(models,preparations,prep_profile,material_profile,route_profile,read_context,read,
            model_context,pin_model,class_context,pin_class,baseline_context,pin_baseline,
            texture_context,resolve,second,error))return false;
        for(std::size_t k=0;k<first.members.size();++k)
            if(!same(first.members[k],second.members[k]))
                return fail(error,"world Borg material changed across sweeps");
        WorldModelAssociations fresh_models;
        if(!capture_world_model_associations(model_profile,expected,read_context,read,
            model_context,pin_model,fresh_models,error)||
           !same_world_model_associations(models,fresh_models))
            return fail(error,"world Borg material model/list rewalk changed");
        WorldCraftPreparations fresh_preparations;
        if(!capture_world_craft_preparations(models,model_profile,prep_profile,
            preparations.native_x87_control,read_context,read,model_context,pin_model,
            class_context,pin_class,fresh_preparations,error)||
           !same_world_craft_preparations(preparations,fresh_preparations))
            return fail(error,"world Borg material preparation rewalk changed");
        for(std::size_t k=0;k<models.members.size();++k){
            const auto& model=models.members[k];const auto& row=first.members[k];
            if(model.status!=WorldModelStatus::bound_current_database||
               preparations.members[k].status!=WorldPreparationStatus::captured)continue;
            if(!same_material_baseline_owners(row.baseline,
                pin_baseline(baseline_context,model.model)))
                return fail(error,"world material baseline changed after rewalk");
            if(row.route_valid){
                CraftBorgMaterialRoute route;
                if(!row.preparation||!capture_craft_borg_material_route(*row.preparation,
                    route_profile,read_context,read,class_context,pin_class,route,error)||
                   !same_craft_borg_material_route(row.route,route))
                    return fail(error,"world material route changed after rewalk");
            }
            if(row.status==WorldBorgMaterialStatus::correlated){
                ModelAdapter adapter{model_context,pin_model,model.model_name};
                CapturedCraftBorgTable current;std::string reason;
                if(!capture_craft_borg_table(row.table.object_class,row.baseline,
                    material_profile,read_context,read,&adapter,pin_by_serial,class_context,
                    pin_class,texture_context,resolve,current,reason)||
                   current.class_vtable!=row.table.class_vtable||
                   current.table_pointer!=row.table.table_pointer||
                   !same_rows(current.rows,row.table.rows))
                    return fail(error,"world material table or owners changed after rewalk");
            }
        }
        output=std::move(first);error.clear();return true;
    }catch(...){return fail(error,"world Borg material allocation or callback failure");}
}
} // namespace a2fo::native11
