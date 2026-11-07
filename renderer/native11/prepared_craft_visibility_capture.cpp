#include "prepared_craft_visibility_capture.hpp"
#include "craft_visibility_inputs.hpp"
#include <cstring>
#include <utility>
namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* why) noexcept {try{error=why;}catch(...){}return false;}

}
bool capture_prepared_craft_visibility(const WorldModelAssociation& association,
    const CapturedCraftPreparation& preparation,std::shared_ptr<const ModelNodeTemplate> baseline,
    CraftVisibilityReadProfile visibility_profile,CraftPreparationReadProfile preparation_profile,
    void* context,Read read,void* class_context,PinLightClass pin,
    CapturedCraftVisibility& output,std::string& error) noexcept {
    try {
        if(!baseline||!association.model||baseline->model!=association.model||
           visibility_profile.instance_vtable!=preparation_profile.instance_vtable||
           visibility_profile.database_vtable!=preparation_profile.database_vtable)
            return fail(error,"prepared visibility model or dispatch profile mismatch");
        CapturedCraftPreparation before,after;
        if(!capture_craft_preparation(association,preparation_profile,preparation.native_x87_control,
            context,read,class_context,pin,before,error))return false;
        if(!same_craft_preparation(before,preparation))return fail(error,"prepared visibility logical snapshot is stale or inconsistent");
        detail::CraftVisibilitySubject subject;
        subject.entity=association.member.entity;subject.instance=association.member.instance;
        subject.database=association.database;subject.instance_vtable=association.instance_vtable;
        subject.database_vtable=association.database_vtable;subject.model_serial=association.model->info.serial;
        subject.model_identity=ModelIdentity::plain_database_name;
        static_assert(sizeof(subject.model_name)==sizeof(association.model_name),"model name identity capacity");
        std::memcpy(subject.model_name,association.model_name,sizeof(subject.model_name));
        CapturedCraftVisibility pending;
        if(!detail::capture_craft_visibility_inputs(subject,association.member,before.object_class,
            std::move(baseline),visibility_profile,&before.state,context,read,class_context,pin,pending,error)||
           !capture_craft_preparation(association,preparation_profile,preparation.native_x87_control,
            context,read,class_context,pin,after,error))return false;
        if(!same_craft_preparation(before,after))return fail(error,"prepared visibility logical source changed during role capture");
        pending.preparation=std::make_shared<const CapturedCraftPreparation>(before);
        output=std::move(pending);error.clear();return true;
    }catch(...){return fail(error,"prepared visibility allocation or callback failure");}
}
}
