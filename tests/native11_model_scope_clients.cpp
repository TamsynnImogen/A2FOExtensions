// API9 opt-in host adapter mock. No renderer DLL or model-completeness claim.
#include "../renderer/native11/world_session.hpp"
#include "../renderer/native11/gameplay_session.hpp"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
int creates=0,destroys=0,legacy=0,all_parts=0,presents=0;
bool reentry_refused=false;
WorldRenderSession* world_active=nullptr;
GameplayRenderSession* gameplay_active=nullptr;
std::int32_t A2FO_N11_CALL create(const A2FO_N11_Host*,A2FO_N11_Session* out){
    ++creates;*out=17;return S_OK;
}
std::int32_t A2FO_N11_CALL destroy(A2FO_N11_Session){++destroys;return S_OK;}
std::int32_t A2FO_N11_CALL legacy_submit(A2FO_N11_Session,const A2FO_N11_Frame*){
    ++legacy;return S_OK;
}
std::int32_t A2FO_N11_CALL all_submit(A2FO_N11_Session,const A2FO_N11_Frame* frame){
    ++all_parts;
    need((frame->flags&(A2FO_N11_FRAME_COMPLETE_CASTERS|A2FO_N11_FRAME_REQUIRE_PART_STATES))==
         (A2FO_N11_FRAME_COMPLETE_CASTERS|A2FO_N11_FRAME_REQUIRE_PART_STATES),
         "all-parts receives complete posed world frame");
    std::string error;
    if(world_active)reentry_refused=!world_active->close(error)&&!error.empty();
    if(gameplay_active)reentry_refused=!gameplay_active->close(error)&&!error.empty();
    return S_OK;
}
std::int32_t A2FO_N11_CALL present(A2FO_N11_Session,const A2FO_N11_Present*){
    ++presents;return S_OK;
}
std::int32_t A2FO_N11_CALL configure(A2FO_N11_Session,const A2FO_N11_PresentationTarget*){
    return S_OK;
}
bool current(void*,const FrameIdentity&) noexcept {return true;}
std::shared_ptr<const ModelAsset> resolve(void* opaque,std::uint32_t db,std::uint64_t serial){
    const auto model=*static_cast<std::shared_ptr<const ModelAsset>*>(opaque);
    return model&&model->info.database==db&&model->info.serial==serial?model:nullptr;
}
std::shared_ptr<const OwnedWorldFrame> packet(){
    auto model=std::make_shared<ModelAsset>();model->info.database=7;model->info.serial=11;
    std::strcpy(model->info.name,"scope.sod");model->bytes={42};model->info.byte_count=1;
    auto frame=std::make_unique<Frame>();frame->camera=1;frame->generation=1;
    frame->object_count=1;frame->complete_casters=true;
    for(unsigned i=0;i<4;++i){frame->view.world_to_clip[i*5]=1;frame->objects[0].world[i*5]=1;}
    frame->view.viewport[2]=frame->view.viewport[3]=32;
    auto& object=frame->objects[0];object.instance=101;object.database=7;
    object.model_serial=11;object.model_identity=ModelIdentity::plain_database_name;
    object.placement_identity=PlacementIdentity::authored_placement;
    std::strcpy(object.model_name,"scope.sod");
    auto observed=std::make_shared<MaterialObservation>();observed->instance=101;
    observed->database=7;observed->model_serial=11;observed->model=model;
    MaterialBinding part;part.node_state_valid=true;observed->parts.push_back(part);
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observations{{101,observed}};
    std::shared_ptr<const ModelAsset> owned=model;
    std::shared_ptr<const BoundMaterialCapture> bound;std::string error;
    need(build_bound_material_capture(*frame,10,resolve,&owned,observations,bound,error),
         "mock posed world capture");
    auto result=std::make_shared<OwnedWorldFrame>();result->materials=bound;
    result->light_camera=1;result->light_generation=1;result->complete_casters=true;
    return result;
}
A2FO_N11_API9 api9(){
    A2FO_N11_API9 api{};auto& base=api.base.base.base.base.base.base.base.base;
    base.struct_size=sizeof(api);base.version=A2FO_N11_API_VERSION_ALL_MODEL_PARTS;
    base.capabilities=1023;base.create=create;base.destroy=destroy;
    base.submit=legacy_submit;
    base.resize=+[](A2FO_N11_Session,std::uint32_t,std::uint32_t)->std::int32_t{return S_OK;};
    base.readback=+[](A2FO_N11_Session,A2FO_N11_Readback*)->std::int32_t{return S_OK;};
    base.usage=+[](A2FO_N11_Session,A2FO_N11_Usage*)->std::int32_t{return S_OK;};
    api.base.base.base.base.base.base.base.configure_shadows=
        +[](A2FO_N11_Session,const A2FO_N11_Shadows*)->std::int32_t{return S_OK;};
    api.base.base.base.base.base.base.base.configure_materials=
        +[](A2FO_N11_Session,const A2FO_N11_Materials*)->std::int32_t{return S_OK;};
    api.base.base.base.base.base.base.configure_texture_bindings=
        +[](A2FO_N11_Session,const A2FO_N11_TextureBindings*)->std::int32_t{return S_OK;};
    api.base.base.base.base.base.configure_part_states=
        +[](A2FO_N11_Session,const A2FO_N11_PartStates*)->std::int32_t{return S_OK;};
    api.base.base.base.base.submit_world=legacy_submit;
    api.base.base.base.configure_mesh_sharing=
        +[](A2FO_N11_Session,const A2FO_N11_MeshSharing*)->std::int32_t{return S_OK;};
    api.base.base.submit_primitives=
        +[](A2FO_N11_Session,const A2FO_N11_PrimitiveFrame*)->std::int32_t{return S_OK;};
    api.base.configure_presentation=configure;api.base.present=present;
    api.submit_world_all_parts=all_submit;return api;
}
}
int main(){
    auto api=api9();std::string error;
    auto invalid=api;invalid.base.base.base.base.base.base.base.base.struct_size=76;
    WorldRenderSession w;GameplayRenderSession g;
    need(!w.initialize_all_parts(invalid,32,32,error)&&
         !g.initialize_all_parts(invalid,32,32,reinterpret_cast<HWND>(1),error)&&creates==0,
         "API9 size rejected before create");
    invalid=api;invalid.base.base.base.base.base.base.base.base.version=8;
    need(!w.initialize_all_parts(invalid,32,32,error)&&
         !g.initialize_all_parts(invalid,32,32,reinterpret_cast<HWND>(1),error)&&creates==0,
         "API9 version rejected");
    invalid=api;invalid.base.base.base.base.base.base.base.base.capabilities&=~A2FO_N11_CAP_ALL_MODEL_PARTS;
    need(!w.initialize_all_parts(invalid,32,32,error)&&
         !g.initialize_all_parts(invalid,32,32,reinterpret_cast<HWND>(1),error)&&creates==0,
         "API9 capability rejected");
    invalid=api;invalid.base.base.base.base.base.base.base.base.reserved=1;
    need(!w.initialize_all_parts(invalid,32,32,error)&&
         !g.initialize_all_parts(invalid,32,32,reinterpret_cast<HWND>(1),error)&&creates==0,
         "API9 reserved header rejected");
    invalid=api;invalid.submit_world_all_parts=nullptr;
    need(!w.initialize_all_parts(invalid,32,32,error)&&
         !g.initialize_all_parts(invalid,32,32,reinterpret_cast<HWND>(1),error)&&creates==0,
         "API9 missing all-parts entry rejected");
    auto world=packet();
    need(w.initialize_all_parts(api,32,32,error),"API9 world initializer");
    world_active=&w;need(w.render(world,error)&&all_parts==1&&legacy==0&&reentry_refused,
        "world routes through all-parts entry with reentry guard");
    world_active=nullptr;need(w.close(error),"world close");
    need(g.initialize_all_parts(api,32,32,reinterpret_cast<HWND>(1),error),
         "API9 gameplay initializer");
    auto frame=std::make_shared<GameplayFrame>();frame->identity={1,1,10,5};
    frame->width=frame->height=32;frame->world=world;frame->world_domains=4;
    frame->absent_domains=all_frame_domains&~4u;frame->complete_geometry=true;
    gameplay_active=&g;
    need(g.render(frame,current,nullptr,error)==PresentationResult::presented&&
         all_parts==2&&legacy==0&&presents==1&&reentry_refused,
         "gameplay routes through all-parts entry then presents");
    gameplay_active=nullptr;need(g.close(error)&&destroys==2,"gameplay close");
    // The old initializers still select the API5/API8 prefix entry.
    auto api5=api.base.base.base.base;
    api5.base.base.base.base.struct_size=sizeof(api5);
    api5.base.base.base.base.version=A2FO_N11_API_VERSION_WORLD;
    WorldRenderSession old_world;
    need(old_world.initialize(api5,32,32,error)&&old_world.render(world,error)&&legacy==1,
         "legacy world initializer keeps prefix dispatch");
    need(old_world.close(error)&&destroys==3,"legacy world close");
    std::puts("PASS native11 API9 host adapters: opt-in all-parts dispatch and legacy prefix");
}
