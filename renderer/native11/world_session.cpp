#include "world_session.hpp"
#include <windows.h>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace a2fo::native11 {
namespace {
void fail(std::string& error,const char* message) noexcept {
    try { error=message; } catch (...) { error.clear(); }
}
void failed_call(std::string& error,const char* operation,std::int32_t result,
    const std::string& detail) noexcept {
    char message[96]{};
    std::snprintf(message,sizeof(message),"native11 %s failed (HRESULT %08lx)",operation,
        static_cast<unsigned long>(static_cast<std::uint32_t>(result)));
    fail(error,message);
    if(!detail.empty())try {error += ": ";error += detail;}catch(...){}
}
struct Busy {bool& flag;explicit Busy(bool& value):flag(value){flag=true;}~Busy(){flag=false;}};
bool valid_api(const A2FO_N11_API5& api) noexcept {
    const auto& base=api.base.base.base.base;
    constexpr std::uint32_t required=A2FO_N11_CAP_OFFSCREEN_STATIC|
        A2FO_N11_CAP_OFFSCREEN_SHADOWS|A2FO_N11_CAP_MATERIAL_RELIGHT|
        A2FO_N11_CAP_TEXTURE_BINDINGS|A2FO_N11_CAP_PART_STATES|
        A2FO_N11_CAP_WORLD_CASTERS;
    return base.struct_size==sizeof(api)&&base.version==A2FO_N11_API_VERSION_WORLD&&
        !base.reserved&&(base.capabilities&required)==required&&
        base.create&&base.destroy&&base.submit&&base.resize&&base.readback&&base.usage&&
        api.base.base.base.configure_shadows&&api.base.base.base.configure_materials&&
        api.base.base.configure_texture_bindings&&api.base.configure_part_states&&api.submit_world;
}
}

bool WorldRenderSession::available() const noexcept {
    return session_&&owner_thread_==GetCurrentThreadId()&&!busy_;
}

bool WorldRenderSession::initialize(const A2FO_N11_API5& api,std::uint32_t width,
    std::uint32_t height,std::string& error) noexcept {
    if(session_||busy_){fail(error,"native11 world session already active");return false;}
    if(!valid_api(api)||!width||!height){fail(error,"invalid native11 API5 session request");return false;}
    Busy guard(busy_);
    api_=api;owner_thread_=GetCurrentThreadId();
    last_log_.clear();
    A2FO_N11_Host host{sizeof(host),0,width,height,this,read_asset,log};
    auto result=api_.base.base.base.base.create(&host,&session_);
    if(result<0||!session_){failed_call(error,"create",result,last_log_);session_=0;owner_thread_=0;return false;}
    A2FO_N11_TextureBindings textures{sizeof(textures),0,this,resolve_texture};
    result=api_.base.base.configure_texture_bindings(session_,&textures);
    if(result>=0){A2FO_N11_PartStates states{sizeof(states),0,this,resolve_part_state};
        result=api_.base.configure_part_states(session_,&states);}
    if(result<0){
        const auto destroyed=api_.base.base.base.base.destroy(session_);
        if(destroyed>=0){session_=0;owner_thread_=0;}
        failed_call(error,"configure callbacks",result,last_log_);
        return false;
    }
    error.clear();return true;
}

bool WorldRenderSession::initialize_shared(const A2FO_N11_API6& api,std::uint32_t width,
    std::uint32_t height,std::string& error) noexcept {
    const auto& header=api.base.base.base.base.base;
    if(header.struct_size!=sizeof(api)||header.version!=A2FO_N11_API_VERSION_GEOMETRY||
       !(header.capabilities&A2FO_N11_CAP_MESH_SHARING)||!api.configure_mesh_sharing){
        fail(error,"invalid native11 API6 shared session request");return false;
    }
    auto prefix=api.base;prefix.base.base.base.base.struct_size=sizeof(prefix);
    prefix.base.base.base.base.version=A2FO_N11_API_VERSION_WORLD;
    if(!initialize(prefix,width,height,error))return false;
    // initialize has returned and released its admission. Keep callback reentry
    // refused while the additional policy is configured on this owner thread.
    std::int32_t result=0;
    {Busy guard(busy_);A2FO_N11_MeshSharing policy{sizeof(policy),1,{0,0}};
        result=api.configure_mesh_sharing(session_,&policy);}
    if(result<0){
        failed_call(error,"configure shared geometry",result,last_log_);
        std::string ignored;close(ignored);return false;
    }
    error.clear();return true;
}

bool WorldRenderSession::initialize_all_parts(const A2FO_N11_API9& api,
    std::uint32_t width,std::uint32_t height,std::string& error) noexcept {
    const auto& header=api.base.base.base.base.base.base.base.base;
    if(header.struct_size!=sizeof(api)||
       header.version!=A2FO_N11_API_VERSION_ALL_MODEL_PARTS||header.reserved||
       !(header.capabilities&A2FO_N11_CAP_ALL_MODEL_PARTS)||
       !api.submit_world_all_parts){
        fail(error,"invalid native11 API9 all-parts session request");return false;
    }
    auto prefix=api.base.base.base.base;
    prefix.base.base.base.base.struct_size=sizeof(prefix);
    prefix.base.base.base.base.version=A2FO_N11_API_VERSION_WORLD;
    prefix.submit_world=api.submit_world_all_parts;
    return initialize(prefix,width,height,error);
}

bool WorldRenderSession::initialize_native_geometry(const A2FO_N11_API10& api,
    std::uint32_t width,std::uint32_t height,std::string& error) noexcept {
    const auto& header=api.base.base.base.base.base.base.base.base.base;
    if(header.struct_size!=sizeof(api)||
       header.version!=A2FO_N11_API_VERSION_NATIVE_GEOMETRY||header.reserved||
       (header.capabilities&(A2FO_N11_CAP_NATIVE_GEOMETRY|A2FO_N11_CAP_ALL_MODEL_PARTS))!=
           (A2FO_N11_CAP_NATIVE_GEOMETRY|A2FO_N11_CAP_ALL_MODEL_PARTS)||
       !api.configure_geometry_policies||!api.submit_world_native_geometry){
        fail(error,"invalid native11 API10 geometry policy session request");return false;
    }
    auto prefix=api.base.base.base.base.base;
    prefix.base.base.base.base.struct_size=sizeof(prefix);
    prefix.base.base.base.base.version=A2FO_N11_API_VERSION_WORLD;
    prefix.submit_world=api.submit_world_native_geometry;
    if(!initialize(prefix,width,height,error))return false;
    std::int32_t result=0;
    {Busy guard(busy_);A2FO_N11_GeometryPolicies policies{sizeof(policies),0,this,
        resolve_geometry_policy};
        result=api.configure_geometry_policies(session_,&policies);}
    if(result<0){
        failed_call(error,"configure native geometry policies",result,last_log_);
        std::string ignored;close(ignored);return false;
    }
    error.clear();return true;
}

bool WorldRenderSession::initialize_live_materials(const A2FO_N11_API11& api,
    std::uint32_t width,std::uint32_t height,std::string& error,bool share_geometry) noexcept {
    const auto& header=api.base.base.base.base.base.base.base.base.base.base;
    if(header.struct_size!=sizeof(api)||header.version!=A2FO_N11_API_VERSION_LIVE_MATERIALS||
       header.reserved||(header.capabilities&A2FO_N11_CAP_LIVE_MATERIALS)==0||
       !api.configure_live_materials||!api.prepare_live_materials||
       !api.submit_live_material_preview){
        fail(error,"invalid native11 API11 live material session request");return false;
    }
    if(share_geometry&&(!(header.capabilities&A2FO_N11_CAP_MESH_SHARING)||
       !api.base.base.base.base.base.configure_mesh_sharing)){
        fail(error,"native11 API11 mesh sharing unavailable");return false;
    }
    auto prefix=api.base;
    prefix.base.base.base.base.base.base.base.base.base.struct_size=sizeof(prefix);
    prefix.base.base.base.base.base.base.base.base.base.version=A2FO_N11_API_VERSION_NATIVE_GEOMETRY;
    if(!initialize_native_geometry(prefix,width,height,error))return false;
    std::int32_t result=0;
    {Busy guard(busy_);A2FO_N11_LiveMaterials materials{sizeof(materials),0,this,resolve_live_material};
        result=api.configure_live_materials(session_,&materials);}
    if(result<0){
        failed_call(error,"configure live materials",result,last_log_);
        std::string ignored;close(ignored);return false;
    }
    if(share_geometry){
        Busy guard(busy_);A2FO_N11_MeshSharing sharing{sizeof(sharing),1,{0,0}};
        result=api.base.base.base.base.base.configure_mesh_sharing(session_,&sharing);
    }
    if(result!=0){
        failed_call(error,"configure live geometry sharing",result,last_log_);
        std::string ignored;close(ignored);return false;
    }
    live_api_=api;live_enabled_=true;error.clear();return true;
}

bool WorldRenderSession::submit_live_materials(std::shared_ptr<const BoundMaterialCapture> material,
    bool preview,const float ambient[3],bool (*checkpoint)(void*) noexcept,void* context,
    std::string& error) noexcept {
    if(!available()||!live_enabled_){fail(error,"native11 live material session unavailable or reentrant");return false;}
    if(!material||!checkpoint||!ambient||!material->source_epoch()||
       !material->frame().camera||!material->frame().generation||
       material->frame().generation<=attempted_material_generation_||
       (preview&&!material->frame().requires_part_states)||
       material->frame().object_count>capacity){
        fail(error,"invalid or stale live material frame");return false;
    }
    if(current_material_&&current_material_->source_epoch()==material->source_epoch()&&
       !current_material_->same_resources(*material)){
        fail(error,"live material resources changed inside source epoch");return false;
    }
    if(current_&&current_->materials->source_epoch()==material->source_epoch()&&
       !current_->materials->same_resources(*material)){
        fail(error,"world and live material resources differ inside source epoch");return false;
    }
    Busy guard(busy_);
    try {
        const auto& captured=material->frame();
        std::vector<A2FO_N11_Object> objects(captured.object_count);
        for(std::size_t i=0;i<captured.object_count;++i){
            const auto& source=captured.objects[i];auto& target=objects[i];
            const auto* end=static_cast<const char*>(std::memchr(source.model_name,0,sizeof(source.model_name)));
            if(source.model_identity!=ModelIdentity::plain_database_name||!end||end==source.model_name||
               (source.placement_identity!=PlacementIdentity::authored_placement&&
                source.placement_identity!=PlacementIdentity::craft_runtime_scale)){
                fail(error,"invalid live material model identity");return false;
            }
            for(const char* p=source.model_name;p!=end;++p)
                if(static_cast<unsigned char>(*p)<32||static_cast<unsigned char>(*p)>126){
                    fail(error,"invalid live material model identifier");return false;
                }
            for(float value:source.world)if(!std::isfinite(value)){
                fail(error,"invalid live material world transform");return false;
            }
            if(source.world[12]!=0||source.world[13]!=0||source.world[14]!=0||source.world[15]!=1){
                fail(error,"invalid live material world transform");return false;
            }
            std::copy(source.world,source.world+16,target.local_to_world);
            target.model_identifier=reinterpret_cast<const std::uint8_t*>(source.model_name);
            target.model_identifier_length=static_cast<std::uint32_t>(end-source.model_name);
        }
        for(float value:captured.view.world_to_clip)if(!std::isfinite(value)){
            fail(error,"invalid live material camera");return false;
        }
        for(float value:captured.view.viewport)if(!std::isfinite(value)||value<0){
            fail(error,"invalid live material viewport");return false;
        }
        for(unsigned i=0;i<3;++i)if(!std::isfinite(ambient[i])||ambient[i]<0){
            fail(error,"invalid live material ambient");return false;
        }
        A2FO_N11_Frame frame{};frame.struct_size=sizeof(frame);
        frame.flags=captured.requires_part_states?A2FO_N11_FRAME_REQUIRE_PART_STATES:0;
        frame.generation=captured.generation;frame.source_epoch=material->source_epoch();
        std::copy(captured.view.world_to_clip,captured.view.world_to_clip+16,frame.world_to_clip);
        std::copy(captured.view.viewport,captured.view.viewport+4,frame.viewport);
        std::copy(ambient,ambient+3,frame.ambient);
        frame.object_count=static_cast<std::uint32_t>(objects.size());frame.objects=objects.data();
        if(!checkpoint(context)){fail(error,"live material source changed before preparation");return false;}
        pending_material_=std::move(material);last_log_.clear();
        attempted_material_generation_=frame.generation;
        const auto prepared=live_api_.prepare_live_materials(session_,&frame);
        if(!checkpoint(context)){
            pending_material_.reset();fail(error,"live material source changed during preparation");return false;
        }
        if(prepared!=S_OK){
            pending_material_.reset();failed_call(error,"live material preparation",prepared,last_log_);return false;
        }
        if(preview){
            if(!checkpoint(context)){
                pending_material_.reset();fail(error,"live material source changed before preview");return false;
            }
            const auto result=live_api_.submit_live_material_preview(session_,&frame);
            if(!checkpoint(context)){
                pending_material_.reset();fail(error,"live material source changed during preview");return false;
            }
            if(result!=S_OK){
                pending_material_.reset();failed_call(error,"live material preview",result,last_log_);return false;
            }
        }
        current_material_=std::move(pending_material_);
        accepted_material_generation_=frame.generation;error.clear();return true;
    }catch(...){pending_material_.reset();fail(error,"live material session allocation or callback failed");return false;}
}

bool WorldRenderSession::render(std::shared_ptr<const OwnedWorldFrame> packet,
    std::string& error) noexcept {
    if(!available()){fail(error,"native11 world session unavailable or reentrant");return false;}
    if(!packet||!packet->materials||!packet->complete_casters||
       !packet->materials->frame().requires_part_states||
       !packet->light_camera||!packet->light_generation||
       packet->light_camera!=packet->materials->frame().camera||
       packet->light_generation!=packet->materials->frame().generation||
       packet->light_generation<=accepted_generation_){
        fail(error,"invalid or stale complete world frame");return false;
    }
    if(current_&&current_->materials->source_epoch()==packet->materials->source_epoch()&&
       !current_->materials->same_resources(*packet->materials)){
        fail(error,"world resources changed inside source epoch");return false;
    }
    Busy guard(busy_);
    pending_=std::move(packet);
    last_log_.clear();
    const auto result=submit_world_capture(api_,session_,pending_->materials->frame(),
        pending_->materials->source_epoch(),pending_->lighting,pending_->complete_casters);
    if(result<0){pending_.reset();failed_call(error,"world submit",result,last_log_);return false;}
    current_=std::move(pending_);
    accepted_generation_=current_->light_generation;
    error.clear();return true;
}

bool WorldRenderSession::resize(std::uint32_t width,std::uint32_t height,
    std::string& error) noexcept {
    if(!available()){fail(error,"native11 world session unavailable or reentrant");return false;}
    if(!width||!height){fail(error,"invalid native11 target size");return false;}
    Busy guard(busy_);
    last_log_.clear();
    const auto result=api_.base.base.base.base.resize(session_,width,height);
    if(result<0){failed_call(error,"resize",result,last_log_);return false;}
    error.clear();return true;
}

bool WorldRenderSession::close(std::string& error) noexcept {
    if(busy_){fail(error,"native11 world session is active");return false;}
    if(!session_){error.clear();return true;}
    if(!available()){fail(error,"native11 world session unavailable or reentrant");return false;}
    Busy guard(busy_);
    last_log_.clear();
    const auto result=api_.base.base.base.base.destroy(session_);
    if(result<0){failed_call(error,"destroy",result,last_log_);return false;}
    session_=0;owner_thread_=0;pending_.reset();current_.reset();accepted_generation_=0;
    pending_material_.reset();current_material_.reset();accepted_material_generation_=0;
    attempted_material_generation_=0;live_enabled_=false;live_api_={};
    error.clear();return true;
}

bool WorldRenderSession::readback(std::vector<std::uint8_t>& bgra,std::uint32_t& width,
    std::uint32_t& height,std::string& error) noexcept {
    if(!available()){fail(error,"native11 world session unavailable or reentrant");return false;}
    Busy guard(busy_);
    last_log_.clear();
    try {
        A2FO_N11_Readback result{};result.struct_size=sizeof(result);
        auto hr=api_.base.base.base.base.readback(session_,&result);
        if(hr<0||!result.width||!result.height||!result.required_bytes||
           result.required_bytes>std::numeric_limits<std::size_t>::max()){
            failed_call(error,"readback metadata",hr,last_log_);return false;
        }
        std::vector<std::uint8_t> candidate(static_cast<std::size_t>(result.required_bytes));
        result.bgra=candidate.data();result.capacity=candidate.size();
        hr=api_.base.base.base.base.readback(session_,&result);
        if(hr<0){failed_call(error,"readback pixels",hr,last_log_);return false;}
        bgra=std::move(candidate);width=result.width;height=result.height;error.clear();return true;
    }catch(...){fail(error,"native11 readback allocation failed");return false;}
}

bool WorldRenderSession::usage(A2FO_N11_Usage& output,std::string& error) noexcept {
    if(!available()){fail(error,"native11 world session unavailable or reentrant");return false;}
    Busy guard(busy_);
    last_log_.clear();
    A2FO_N11_Usage candidate{};candidate.struct_size=sizeof(candidate);
    const auto result=api_.base.base.base.base.usage(session_,&candidate);
    if(result<0){failed_call(error,"usage",result,last_log_);return false;}
    output=candidate;error.clear();return true;
}

std::shared_ptr<const OwnedWorldFrame> WorldRenderSession::current() const noexcept {
    return available()?current_:nullptr;
}
std::uint64_t WorldRenderSession::accepted_generation() const noexcept {
    return available()?accepted_generation_:0;
}
std::uint64_t WorldRenderSession::accepted_material_generation() const noexcept {
    return available()?accepted_material_generation_:0;
}
std::shared_ptr<const BoundMaterialCapture> WorldRenderSession::callback_materials() const noexcept {
    if(pending_material_)return pending_material_;
    if(pending_&&pending_->materials)return pending_->materials;
    return nullptr;
}

std::int32_t A2FO_N11_CALL WorldRenderSession::read_asset(void* context,std::uint32_t kind,
    const std::uint8_t* id,std::uint32_t size,std::uint32_t limit,
    A2FO_N11_BytesSink sink,void* sink_context) noexcept {
    if(!context)return 0;
    auto& self=*static_cast<WorldRenderSession*>(context);
    auto owner=self.callback_materials();
    return self.busy_&&self.owner_thread_==GetCurrentThreadId()&&owner?
        BoundMaterialCapture::read_asset(const_cast<BoundMaterialCapture*>(owner.get()),
            kind,id,size,limit,sink,sink_context):0;
}
std::int32_t A2FO_N11_CALL WorldRenderSession::resolve_texture(void* context,
    const std::uint8_t* model,std::uint32_t model_size,std::uint32_t part,
    const std::uint8_t* authored,std::uint32_t authored_size,std::uint32_t limit,
    A2FO_N11_BytesSink sink,void* sink_context) noexcept {
    if(!context)return 0;
    auto& self=*static_cast<WorldRenderSession*>(context);
    auto owner=self.callback_materials();
    return self.busy_&&self.owner_thread_==GetCurrentThreadId()&&owner?
        BoundMaterialCapture::resolve_texture(const_cast<BoundMaterialCapture*>(owner.get()),
            model,model_size,part,authored,authored_size,limit,sink,sink_context):0;
}
std::int32_t A2FO_N11_CALL WorldRenderSession::resolve_part_state(void* context,
    const std::uint8_t* model,std::uint32_t size,std::uint32_t part,
    const float authored[16],A2FO_N11_PartStateSink sink,void* sink_context) noexcept {
    if(!context)return 0;
    auto& self=*static_cast<WorldRenderSession*>(context);
    auto owner=self.callback_materials();
    if(!self.busy_||self.owner_thread_!=GetCurrentThreadId()||!owner)return 0;
    if(!owner->frame().requires_part_states)return 0;
    return BoundMaterialCapture::resolve_part_state(
        const_cast<BoundMaterialCapture*>(owner.get()),model,size,part,
        authored,sink,sink_context);
}
std::int32_t A2FO_N11_CALL WorldRenderSession::resolve_geometry_policy(void* context,
    const std::uint8_t* model,std::uint32_t length,A2FO_N11_GeometryPolicySink sink,
    void* sink_context) noexcept {
    if(!context)return 0;
    auto& self=*static_cast<WorldRenderSession*>(context);
    auto owner=self.callback_materials();
    return self.busy_&&self.owner_thread_==GetCurrentThreadId()&&owner?
        BoundMaterialCapture::resolve_geometry_policy(
            const_cast<BoundMaterialCapture*>(owner.get()),model,length,
            sink,sink_context):0;
}
std::int32_t A2FO_N11_CALL WorldRenderSession::resolve_live_material(void* context,
    const std::uint8_t* model,std::uint32_t length,const A2FO_N11_MaterialSeed* seed,
    A2FO_N11_LiveMaterialSink sink,void* sink_context) noexcept {
    if(!context)return 0;
    auto& self=*static_cast<WorldRenderSession*>(context);
    auto owner=self.callback_materials();
    return self.busy_&&self.owner_thread_==GetCurrentThreadId()&&owner?
        BoundMaterialCapture::resolve_live_material(
            const_cast<BoundMaterialCapture*>(owner.get()),model,length,seed,sink,sink_context):0;
}
void A2FO_N11_CALL WorldRenderSession::log(void* context,const char* message,
    std::uint32_t size) noexcept {
    if(!context||!message||!size)return;
    auto& self=*static_cast<WorldRenderSession*>(context);
    if(!self.busy_||self.owner_thread_!=GetCurrentThreadId())return;
    try {self.last_log_.assign(message,message+(size<256?size:256));}catch(...) {self.last_log_.clear();}
}
} // namespace a2fo::native11
