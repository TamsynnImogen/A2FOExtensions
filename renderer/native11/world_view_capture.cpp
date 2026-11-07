#include "world_view_capture.hpp"
#include "native_arithmetic.hpp"
#include <cstring>
#include <set>
#include <utility>
namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* why) noexcept {try{error=why;}catch(...){}return false;}
bool same_shared(const CapturedCraftView& a,const CapturedCraftView& b) noexcept {
    const auto& x=a.input;const auto& y=b.input;
    return a.engine==b.engine&&a.game==b.game&&a.settings==b.settings&&a.camera==b.camera&&
        a.native_x87_control==b.native_x87_control&&
        !std::memcmp(x.camera,y.camera,sizeof(x.camera))&&
        !std::memcmp(&x.max_distance,&y.max_distance,sizeof(float))&&x.mode==y.mode&&
        x.detailed_range==y.detailed_range&&x.reveal_all==y.reveal_all&&
        x.skip_near_fade==y.skip_near_fade&&x.fade_enabled==y.fade_enabled&&
        !std::memcmp(&x.near_fade_min,&y.near_fade_min,sizeof(float))&&
        !std::memcmp(&x.near_fade_max,&y.near_fade_max,sizeof(float))&&
        !std::memcmp(&x.near_fade_amount,&y.near_fade_amount,sizeof(float));
}
bool same_view(const CapturedCraftView& a,const CapturedCraftView& b) noexcept {
    return same_shared(a,b)&&a.entity==b.entity&&a.instance==b.instance&&a.database==b.database&&
        !std::memcmp(a.input.position,b.input.position,sizeof(a.input.position))&&
        !std::memcmp(&a.input.radius,&b.input.radius,sizeof(float))&&
        a.input.instance_cull_flag==b.input.instance_cull_flag;
}
bool sweep(const WorldModelAssociations& models,CraftViewReadProfile profile,
    std::uint32_t camera,std::uint32_t control,void* context,Read read,WorldCraftViews& out,std::string& error){
    out.camera=camera;out.native_x87_control=control;out.members.reserve(models.members.size());
    const CapturedCraftView* shared=nullptr;
    for(const auto& model:models.members){
        WorldCraftView row;row.member=model.member;
        if(model.status!=WorldModelStatus::bound_current_database)row.reason="current model association unavailable";
        else {
            const CraftViewSubject subject{model.member.entity,model.member.instance,model.database,
                model.instance_vtable,model.database_vtable,model.model->info.serial};
            row.status=capture_craft_view(subject,camera,control,profile,context,read,row.view,row.reason)?
                WorldViewStatus::captured:WorldViewStatus::view_unavailable;
        }
        out.members.push_back(std::move(row));
        const auto& last=out.members.back();
        if(last.status==WorldViewStatus::captured){
            if(shared&&!same_shared(*shared,last.view))return fail(error,"shared Craft view inputs changed across roster");
            if(!shared)shared=&last.view;
        }
    }return true;
}
}
bool capture_world_craft_views(const WorldModelAssociations& models,WorldModelReadProfile model_profile,
    CraftViewReadProfile view_profile,std::uint32_t camera,std::uint32_t control,
    void* context,Read read,void* model_context,PinWorldModel pin,WorldCraftViews& output,std::string& error) noexcept {
    try {
        if(!camera||!control||!valid_native_x87_encoding(control)||!read||!pin||models.members.size()>4096||
           !model_profile.object_list_pointer||!model_profile.craft_instance_vtable||!model_profile.plain_database_vtable||
           model_profile.craft_instance_vtable==model_profile.plain_database_vtable||
           view_profile.instance_vtable!=model_profile.craft_instance_vtable||
           view_profile.database_vtable!=model_profile.plain_database_vtable||
           !view_profile.engine_pointer||!view_profile.game_pointer||!view_profile.mode_pointer||
           !view_profile.skip_near_pointer||!view_profile.max_distance_pointer||!view_profile.near_fade_pointer)
            return fail(error,"invalid world Craft view source/profile");
        std::vector<WorldMember> expected;expected.reserve(models.members.size());
        std::set<std::uint32_t> entities,instances;
        for(const auto& model:models.members){const auto& member=model.member;
            if(!member.entity||!member.object_class||!member.class_version||!entities.insert(member.entity).second||
               (member.instance&&!instances.insert(member.instance).second))return fail(error,"invalid world Craft view roster");
            expected.push_back(member);
            if(model.status==WorldModelStatus::bound_current_database&&
               (!member.instance||!model.model||!model.model->info.serial||model.model->bytes.empty()||
                model.model->info.byte_count!=model.model->bytes.size()||model.database!=model.model->info.database||
                std::strncmp(model.model_name,model.model->info.name,sizeof(model.model_name))||
                model.instance_vtable!=view_profile.instance_vtable||model.database_vtable!=view_profile.database_vtable))
                return fail(error,"invalid bound world Craft view model");
        }
        WorldCraftViews first,second;
        if(!sweep(models,view_profile,camera,control,context,read,first,error)||
           !sweep(models,view_profile,camera,control,context,read,second,error))return false;
        for(std::size_t i=0;i<first.members.size();++i){const auto& a=first.members[i];const auto& b=second.members[i];
            if(a.status!=b.status||a.reason!=b.reason||
               (a.status==WorldViewStatus::captured&&!same_view(a.view,b.view)))
                return fail(error,"world Craft view fields changed across sweeps");
        }
        WorldModelAssociations fresh;
        if(!capture_world_model_associations(model_profile,expected,context,read,model_context,pin,fresh,error))return false;
        if(!same_world_model_associations(models,fresh))return fail(error,"world Craft view model/list association changed");
        output=std::move(first);error.clear();return true;
    }catch(...){return fail(error,"world Craft view allocation or callback failure");}
}
}
