// API8 mock for the owned-world + primitive overlay transaction. No GUI input.
#include "../renderer/native11/gameplay_session.hpp"
#include <windows.h>
#include <cassert>
#include <cstring>
#include <map>
#include <memory>
#include <string>
using namespace a2fo::native11;
namespace {
GameplayRenderSession* active=nullptr;
A2FO_N11_Host host_copy{};
int creates=0,shadows=0,worlds=0,overlays=0,presents=0;
int destroys=0,detaches=0;
std::int32_t create_result=S_OK,destroy_result=S_OK,texture_result=S_OK;
std::int32_t part_result=S_OK,sharing_result=S_OK,attach_result=S_OK,detach_result=S_OK;
bool revoke_in_shadow=false,source_live=true,reentry_refused=false,callback_owned=false;
bool revoke_in_present=false;
std::int32_t present_result=S_OK;
std::int32_t world_result=S_OK,overlay_result=S_OK;
std::uint64_t last_wire_generation=0;
std::int32_t A2FO_N11_CALL create(const A2FO_N11_Host* h,A2FO_N11_Session* out){
    ++creates;host_copy=*h;*out=17;return create_result;
}
std::int32_t A2FO_N11_CALL destroy(A2FO_N11_Session){++destroys;return destroy_result;}
std::int32_t A2FO_N11_CALL resize(A2FO_N11_Session,std::uint32_t,std::uint32_t){return S_OK;}
std::int32_t A2FO_N11_CALL configure(A2FO_N11_Session,const A2FO_N11_PresentationTarget* target){
    assert(target&&target->struct_size==sizeof(*target));
    if(!target->window){++detaches;return detach_result;}
    return attach_result;
}
std::int32_t A2FO_N11_CALL shadow(A2FO_N11_Session,const A2FO_N11_Shadows*){
    ++shadows;std::string error;reentry_refused=!active->close(error);
    if(revoke_in_shadow)source_live=false;
    return S_OK;
}
std::int32_t A2FO_N11_CALL world(A2FO_N11_Session,const A2FO_N11_Frame* frame){
    ++worlds;assert(frame->flags&A2FO_N11_FRAME_COMPLETE_CASTERS);
    assert(frame->flags&A2FO_N11_FRAME_REQUIRE_PART_STATES);
    assert(frame->object_count==1);
    const auto& object=frame->objects[0];
    callback_owned=host_copy.read_asset(host_copy.context,1,object.model_identifier,
        object.model_identifier_length,1024,+[](void*,const std::uint8_t* bytes,std::uint32_t count)->std::int32_t{
            return bytes&&count==1&&bytes[0]==42?1:0;
        },nullptr)==1;
    return world_result;
}
std::int32_t A2FO_N11_CALL overlay(A2FO_N11_Session,const A2FO_N11_PrimitiveFrame* frame){
    ++overlays;assert(frame->generation>last_wire_generation);last_wire_generation=frame->generation;
    assert((frame->flags&(A2FO_N11_PRIMITIVE_LOAD_COLOUR|A2FO_N11_PRIMITIVE_LOAD_DEPTH))==3);
    return overlay_result;
}
std::int32_t A2FO_N11_CALL present(A2FO_N11_Session,const A2FO_N11_Present*){
    ++presents;if(revoke_in_present)source_live=false;return present_result;
}
std::int32_t A2FO_N11_CALL readback(A2FO_N11_Session,A2FO_N11_Readback* output){
    output->width=output->height=32;output->pitch=128;output->required_bytes=4096;
    if(!output->bgra)return S_FALSE;
    assert(output->capacity>=4096);std::memset(output->bgra,51,4096);return S_OK;
}
bool current(void*,const FrameIdentity&) noexcept {return source_live;}
std::shared_ptr<const ModelAsset> resolve(void* raw,std::uint32_t db,std::uint64_t serial){
    auto model=*static_cast<std::shared_ptr<const ModelAsset>*>(raw);
    return model&&db==7&&serial==11?model:nullptr;
}
std::shared_ptr<const BoundMaterialCapture> capture(std::uint64_t generation,
    std::shared_ptr<const ModelAsset>& model){
    if(!model){auto asset=std::make_shared<ModelAsset>();asset->info.database=7;asset->info.serial=11;
        std::strcpy(asset->info.name,"hud-world.sod");asset->bytes={42};asset->info.byte_count=1;model=asset;}
    auto frame=std::make_unique<Frame>();frame->camera=1;frame->generation=generation;
    frame->object_count=1;frame->complete_casters=true;
    for(unsigned i=0;i<4;++i){frame->view.world_to_clip[i*5]=1;frame->objects[0].world[i*5]=1;}
    frame->view.viewport[2]=frame->view.viewport[3]=32;
    auto& obj=frame->objects[0];obj.instance=101;obj.database=7;obj.model_serial=11;
    obj.model_identity=ModelIdentity::plain_database_name;
    obj.placement_identity=PlacementIdentity::authored_placement;
    std::strcpy(obj.model_name,"hud-world.sod");
    auto observation=std::make_shared<MaterialObservation>();observation->instance=101;
    observation->database=7;observation->model_serial=11;observation->model=model;
    MaterialBinding part{};part.node_state_valid=true;observation->parts.push_back(part);
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observations{{101,observation}};
    std::shared_ptr<const BoundMaterialCapture> output;std::string error;
    assert(build_bound_material_capture(*frame,10,resolve,&model,observations,output,error));
    return output;
}
std::shared_ptr<const GameplayFrame> packet(std::uint64_t generation,std::shared_ptr<const ModelAsset>& model){
    auto value=std::make_shared<GameplayFrame>();value->identity={1,generation,10,5};
    value->width=value->height=32;value->world_domains=4;value->absent_domains=123&~8u;
    value->complete_geometry=true;
    auto w=std::make_shared<OwnedWorldFrame>();w->materials=capture(generation,model);
    w->light_camera=1;w->light_generation=generation;w->complete_casters=true;
    value->world=w;
    auto p=std::make_shared<PrimitiveFrame>();p->generation=generation;p->source_epoch=20;
    p->settings.viewport[2]=p->settings.viewport[3]=32;
    p->settings.clear_colour_target=false;p->settings.clear_depth_target=false;
    PrimitiveMesh mesh;mesh.id=1;mesh.vertices.resize(3);mesh.indices={0,1,2};p->meshes.push_back(mesh);
    PrimitiveDraw draw;draw.mesh=1;p->draws.push_back(draw);
    value->overlays.push_back({FrameDomain::effects,p});return value;
}
A2FO_N11_API8 api(){
    A2FO_N11_API8 a{};auto& b=a.base.base.base.base.base.base.base;
    b.struct_size=sizeof(a);b.version=8;b.capabilities=511;b.create=create;b.destroy=destroy;
    b.resize=resize;b.submit=+[](A2FO_N11_Session,const A2FO_N11_Frame*)->std::int32_t{return S_OK;};
    b.readback=readback;
    b.usage=+[](A2FO_N11_Session,A2FO_N11_Usage*)->std::int32_t{return S_OK;};
    a.base.base.base.base.base.base.configure_shadows=shadow;
    a.base.base.base.base.base.base.configure_materials=+[](A2FO_N11_Session,const A2FO_N11_Materials*)->std::int32_t{return S_OK;};
    a.base.base.base.base.base.configure_texture_bindings=+[](A2FO_N11_Session,const A2FO_N11_TextureBindings*)->std::int32_t{return texture_result;};
    a.base.base.base.base.configure_part_states=+[](A2FO_N11_Session,const A2FO_N11_PartStates*)->std::int32_t{return part_result;};
    a.base.base.base.submit_world=world;
    a.base.base.configure_mesh_sharing=+[](A2FO_N11_Session,const A2FO_N11_MeshSharing*)->std::int32_t{return sharing_result;};
    a.base.submit_primitives=overlay;a.configure_presentation=configure;a.present=present;return a;
}
void initialization_failures(){
    std::string error;const auto table=api();
    for(auto stage:{0,1,2,3,4}){
        create_result=stage==0?S_FALSE:S_OK;
        texture_result=stage==1?S_FALSE:S_OK;
        part_result=stage==2?S_FALSE:S_OK;
        sharing_result=stage==3?S_FALSE:S_OK;
        attach_result=stage==4?S_FALSE:S_OK;
        const auto before=destroys,detached=detaches;
        GameplayRenderSession trial;
        assert(!trial.initialize(table,32,32,reinterpret_cast<HWND>(1),error));
        assert(!error.empty()&&destroys==before+1&&detaches==detached+(stage==4?1:0));
        assert(trial.close(error));
    }
    create_result=texture_result=part_result=sharing_result=attach_result=S_OK;
    texture_result=E_FAIL;destroy_result=E_FAIL;
    GameplayRenderSession retained;
    assert(!retained.initialize(table,32,32,reinterpret_cast<HWND>(1),error)&&
           error.find("cleanup destroy failed")!=std::string::npos);
    assert(!retained.initialize(table,32,32,reinterpret_cast<HWND>(1),error));
    destroy_result=S_OK;assert(retained.close(error));
    texture_result=S_OK;attach_result=E_FAIL;detach_result=E_FAIL;
    GameplayRenderSession attached;
    assert(!attached.initialize(table,32,32,reinterpret_cast<HWND>(1),error)&&
           error.find("cleanup detach failed")!=std::string::npos);
    detach_result=S_OK;assert(attached.close(error));
    attach_result=S_OK;creates=destroys=detaches=0;
}
}
int main(){
    initialization_failures();
    GameplayRenderSession session;active=&session;std::string error;auto table=api();
    assert(session.initialize(table,32,32,reinterpret_cast<HWND>(1),error));
    std::shared_ptr<const ModelAsset> model;
    auto first=packet(1,model);
    auto bad=std::make_shared<GameplayFrame>(*first);bad->absent_domains=0;
    assert(session.render(bad,current,nullptr,error)==PresentationResult::fallback&&shadows==0);
    revoke_in_shadow=true;
    assert(session.render(first,current,nullptr,error)==PresentationResult::fallback&&
           shadows==1&&worlds==0&&overlays==0&&presents==0&&reentry_refused);
    source_live=true;revoke_in_shadow=false;
    assert(session.render(first,current,nullptr,error)==PresentationResult::presented&&
           callback_owned&&worlds==1&&overlays==1&&presents==1&&session.accepted_generation()==1);
    std::vector<std::uint8_t> pixels;std::uint32_t width=0,height=0;
    assert(session.readback(pixels,width,height,error)&&width==32&&height==32&&
           pixels.size()==4096&&pixels[0]==51);
    source_live=false;assert(!session.current()&&session.accepted_generation()==0);
    assert(!session.readback(pixels,width,height,error));
    source_live=true;auto second=packet(2,model);
    world_result=S_FALSE;
    assert(session.render(second,current,nullptr,error)==PresentationResult::fallback&&
           worlds==2&&overlays==1&&presents==1);
    world_result=S_OK;overlay_result=S_FALSE;
    assert(session.render(second,current,nullptr,error)==PresentationResult::fallback&&
           worlds==3&&overlays==2&&presents==1);
    overlay_result=S_OK;
    present_result=S_FALSE;
    assert(session.render(second,current,nullptr,error)==PresentationResult::occluded&&
           !session.current()&&session.accepted_generation()==0);
    present_result=S_OK;
    assert(session.render(second,current,nullptr,error)==PresentationResult::presented&&
           worlds==5&&overlays==4&&presents==3);
    auto third=packet(3,model);revoke_in_present=true;
    assert(session.render(third,current,nullptr,error)==PresentationResult::fallback&&
           !session.current()&&presents==4);
    source_live=true;revoke_in_present=false;
    std::shared_ptr<const ModelAsset> changed_owner;
    auto changed=packet(3,changed_owner);
    assert(session.render(changed,current,nullptr,error)==PresentationResult::fallback&&
           worlds==6&&presents==4);
    assert(session.resize(0,0,nullptr,error)&&!session.current());
    assert(session.render(third,current,nullptr,error)==PresentationResult::fallback);
    assert(session.close(error)&&creates==1);
}
