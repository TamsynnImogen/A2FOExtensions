#include "gameplay_session.hpp"
#include "primitive_service.hpp"
#include "world_service.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <set>

namespace a2fo::native11 {
namespace {
void fail(std::string& error,const char* text) noexcept {try{error=text;}catch(...){error.clear();}}
void failed(std::string& error,const char* operation,std::int32_t hr) noexcept {
    char text[112]{};std::snprintf(text,sizeof(text),"gameplay %s failed (HRESULT %08lx)",operation,
        static_cast<unsigned long>(static_cast<std::uint32_t>(hr)));fail(error,text);
}
void cleanup_failed(std::string& error,const char* operation,std::int32_t hr) noexcept {
    char text[112]{};std::snprintf(text,sizeof(text),"; cleanup %s failed (HRESULT %08lx)",operation,
        static_cast<unsigned long>(static_cast<std::uint32_t>(hr)));
    try {error+=text;}catch(...) {}
}
struct Busy {bool& flag;explicit Busy(bool& value):flag(value){flag=true;}~Busy(){flag=false;}};
const A2FO_N11_API& base(const A2FO_N11_API8& api) noexcept {return api.base.base.base.base.base.base.base;}
bool valid(const A2FO_N11_API8& api) noexcept {
    const auto& b=base(api);
    return sizeof(api)==76&&b.struct_size==sizeof(api)&&b.version==8&&!b.reserved&&
        (b.capabilities&511u)==511u&&b.create&&b.destroy&&b.submit&&b.resize&&b.readback&&b.usage&&
        api.base.base.base.base.base.base.configure_shadows&&
        api.base.base.base.base.base.base.configure_materials&&
        api.base.base.base.base.base.configure_texture_bindings&&
        api.base.base.base.base.configure_part_states&&api.base.base.base.submit_world&&
        api.base.base.configure_mesh_sharing&&api.base.submit_primitives&&
        api.configure_presentation&&api.present;
}
bool dimensions(std::uint32_t w,std::uint32_t h) noexcept {
    return w&&h&&w<=4096&&h<=4096&&std::uint64_t(w)*h*8<=256ull*1024*1024;
}
A2FO_N11_PresentationTarget target(std::uint32_t w,std::uint32_t h,HWND window) noexcept {
    A2FO_N11_PresentationTarget t{};t.struct_size=sizeof(t);t.window=window;t.width=w;t.height=h;return t;
}
bool same_manifest(const PrimitiveFrame& a,const PrimitiveFrame& b) noexcept {
    if(a.meshes.size()!=b.meshes.size()||a.textures.size()!=b.textures.size())return false;
    for(std::size_t i=0;i<a.meshes.size();++i){
        const auto& x=a.meshes[i],&y=b.meshes[i];
        if(x.id!=y.id||x.vertices.size()!=y.vertices.size()||x.indices.size()!=y.indices.size()||
           (!x.vertices.empty()&&std::memcmp(x.vertices.data(),y.vertices.data(),x.vertices.size()*sizeof(Vertex)))||
           (!x.indices.empty()&&std::memcmp(x.indices.data(),y.indices.data(),x.indices.size()*sizeof(std::uint32_t))))return false;
    }
    for(std::size_t i=0;i<a.textures.size();++i){
        const auto& x=a.textures[i],&y=b.textures[i];
        if(x.id!=y.id||x.width!=y.width||x.height!=y.height||x.bgra!=y.bgra)return false;
    }
    return true;
}
bool valid_overlay(const PrimitiveFrame& p) {
    if(p.meshes.size()>2048||p.textures.size()>256||p.draws.size()>8192||p.lights.size()>8||
       p.settings.draws||p.settings.draw_count||p.settings.lights||p.settings.light_count)return false;
    std::set<std::uint64_t> meshes,textures;
    for(const auto& m:p.meshes){
        if(!m.id||!meshes.insert(m.id).second||m.vertices.size()<3||m.indices.size()<3||
           m.indices.size()%3)return false;
        for(auto index:m.indices)if(index>=m.vertices.size())return false;
    }
    for(const auto& t:p.textures)if(!t.id||!textures.insert(t.id).second||!t.width||!t.height||
        t.width>16384||t.height>16384||std::uint64_t(t.width)*t.height*4!=t.bgra.size())return false;
    for(const auto& d:p.draws)if(!meshes.count(d.mesh)||(d.texture&&!textures.count(d.texture))||
        d.surface.mesh||d.surface.texture)return false;
    return true;
}
bool admit(const GameplayFrame& packet,const OwnedWorldFrame* previous_world,
           const PrimitiveFrame* previous_overlay,std::string& error) {
    const auto& id=packet.identity;
    constexpr std::uint32_t world_mask=7,overlay_mask=120;
    if(!id.view||id.view>UINT32_MAX||!id.generation||!id.source_epoch||!id.source_ticket||
       !packet.complete_geometry||!dimensions(packet.width,packet.height)||!packet.world||
       !packet.world->materials||!packet.world->complete_casters||
       (packet.world_domains&~world_mask)||!(packet.world_domains&4)||
       (packet.absent_domains&~all_frame_domains)||
       (packet.absent_domains&packet.world_domains)||packet.overlays.size()>4){
        fail(error,"invalid gameplay identity, world or domain declaration");return false;
    }
    const auto& world=*packet.world;const auto& frame=world.materials->frame();
    if(!frame.complete_casters||!frame.requires_part_states||frame.camera!=id.view||
       frame.generation!=id.generation||world.light_camera!=id.view||
       world.light_generation!=id.generation||world.materials->source_epoch()!=id.source_epoch||
       frame.view.viewport[0]!=0||frame.view.viewport[1]!=0||
       frame.view.viewport[2]!=packet.width||frame.view.viewport[3]!=packet.height||
       (previous_world&&id.source_epoch<previous_world->materials->source_epoch())||
       (previous_world&&previous_world->materials->source_epoch()==id.source_epoch&&
        !previous_world->materials->same_resources(*world.materials))){
        fail(error,"world identity, caster, viewport or resource epoch mismatch");return false;
    }
    std::uint32_t covered=packet.world_domains;
    const PrimitiveFrame* first=nullptr;
    std::uint64_t total_bytes=0,total_draws=0;
    for(const auto& pass:packet.overlays){
        const auto bit=static_cast<std::uint32_t>(pass.domain);
        if(!bit||(bit&(bit-1))||!(bit&overlay_mask)||(covered&bit)||
           (packet.absent_domains&bit)||!pass.frame){
            fail(error,"invalid or duplicate overlay domain");return false;
        }
        covered|=bit;
        const auto& p=*pass.frame;
        if(p.generation!=id.generation||!p.source_epoch||
           p.settings.viewport[0]!=0||p.settings.viewport[1]!=0||
           p.settings.viewport[2]!=packet.width||p.settings.viewport[3]!=packet.height||
           p.settings.clear_colour_target||p.settings.clear_depth_target||!valid_overlay(p)||
           (first&&(p.source_epoch!=first->source_epoch||!same_manifest(*first,p)))||
           (previous_overlay&&p.source_epoch<previous_overlay->source_epoch)||
           (previous_overlay&&p.source_epoch==previous_overlay->source_epoch&&
             !same_manifest(*previous_overlay,p))){
            fail(error,"invalid overlay source, load, viewport or resource manifest");return false;
        }
        first=&p;
        total_draws+=p.draws.size();
        if(total_draws>8192){fail(error,"overlay draw budget exceeded");return false;}
        for(const auto& m:p.meshes)total_bytes+=m.vertices.size()*sizeof(Vertex)+m.indices.size()*4;
        for(const auto& t:p.textures)total_bytes+=t.bgra.size();
        if(total_bytes>128ull*1024*1024){fail(error,"overlay payload budget exceeded");return false;}
    }
    if((covered|packet.absent_domains)!=all_frame_domains){
        fail(error,"gameplay domain inventory incomplete");return false;
    }
    error.clear();return true;
}
}

bool GameplayRenderSession::available() const noexcept {return session_&&thread_==GetCurrentThreadId()&&!busy_;}
bool GameplayRenderSession::checkpoint(void* context) noexcept {
    auto& self=*static_cast<GameplayRenderSession*>(context);
    return self.source_current_&&self.source_current_(self.source_context_,self.checking_);
}
bool GameplayRenderSession::initialize(const A2FO_N11_API8& api,std::uint32_t w,std::uint32_t h,
    HWND window,std::string& error) noexcept {
    if(session_||busy_||!valid(api)||!dimensions(w,h)||!window){fail(error,"invalid or active gameplay session");return false;}
    Busy guard(busy_);api_=api;thread_=GetCurrentThreadId();
    auto cleanup=[&](bool attach_attempted) noexcept {
        if(!session_){api_={};thread_=0;width_=height_=0;return;}
        if(attach_attempted){
            auto detach=target(0,0,nullptr);
            const auto detached=api_.configure_presentation(session_,&detach);
            if(detached!=S_OK){cleanup_failed(error,"detach",detached);return;}
        }
        const auto destroyed=base(api_).destroy(session_);
        if(destroyed!=S_OK){cleanup_failed(error,"destroy",destroyed);return;}
        session_=0;thread_=0;width_=height_=0;api_={};
    };
    A2FO_N11_Host host{sizeof(host),0,w,h,this,read_asset,nullptr};
    const auto created=base(api_).create(&host,&session_);
    if(created!=S_OK||!session_){failed(error,"create",created);cleanup(false);return false;}
    width_=w;height_=h;
    const A2FO_N11_TextureBindings textures{sizeof(textures),0,this,resolve_texture};
    auto result=api_.base.base.base.base.base.configure_texture_bindings(session_,&textures);
    if(result!=S_OK){failed(error,"texture bindings",result);cleanup(false);return false;}
    const A2FO_N11_PartStates states{sizeof(states),0,this,resolve_part_state};
    result=api_.base.base.base.base.configure_part_states(session_,&states);
    if(result!=S_OK){failed(error,"part states",result);cleanup(false);return false;}
    const A2FO_N11_MeshSharing sharing{sizeof(sharing),1,{0,0}};
    result=api_.base.base.configure_mesh_sharing(session_,&sharing);
    if(result!=S_OK){failed(error,"mesh sharing",result);cleanup(false);return false;}
    auto t=target(w,h,window);result=api_.configure_presentation(session_,&t);
    if(result!=S_OK){failed(error,"attach",result);cleanup(true);return false;}
    attached_=true;error.clear();return true;
}
bool GameplayRenderSession::initialize_all_parts(const A2FO_N11_API9& api,
    std::uint32_t width,std::uint32_t height,HWND window,std::string& error) noexcept {
    const auto& header=api.base.base.base.base.base.base.base.base;
    if(header.struct_size!=sizeof(api)||
       header.version!=A2FO_N11_API_VERSION_ALL_MODEL_PARTS||header.reserved||
       !(header.capabilities&A2FO_N11_CAP_ALL_MODEL_PARTS)||
       !api.submit_world_all_parts){
        fail(error,"invalid gameplay API9 all-parts session request");return false;
    }
    auto prefix=api.base;
    auto& base_header=prefix.base.base.base.base.base.base.base;
    base_header.struct_size=sizeof(prefix);
    base_header.version=A2FO_N11_API_VERSION_PRESENTATION;
    prefix.base.base.base.submit_world=api.submit_world_all_parts;
    return initialize(prefix,width,height,window,error);
}
PresentationResult GameplayRenderSession::render(std::shared_ptr<const GameplayFrame> packet,
    SourceCurrent source_current,void* context,std::string& error) noexcept {
    if(available()){current_.reset();source_current_=nullptr;source_context_=nullptr;}
    if(!available()||!attached_||!packet||!source_current||packet->width!=width_||
       packet->height!=height_||packet->identity.generation<=accepted_){
        fail(error,"gameplay unavailable, stale or wrong target");return PresentationResult::fallback;
    }
    Busy guard(busy_);
    try {
        if(!admit(*packet,submitted_world_.get(),submitted_overlay_.get(),error))return PresentationResult::fallback;
        std::vector<EncodedPrimitiveFrame> wires;wires.reserve(packet->overlays.size());
        for(const auto& pass:packet->overlays){
            EncodedPrimitiveFrame wire;
            if(!encode_primitive_frame(pass.frame,wire,error))return PresentationResult::fallback;
            wires.push_back(std::move(wire));
        }
        pending_world_=packet->world;source_current_=source_current;source_context_=context;checking_=packet->identity;
        struct Pending {GameplayRenderSession& self;~Pending(){self.pending_world_.reset();}} pending{*this};
        if(!checkpoint(this)){fail(error,"gameplay source revoked");return PresentationResult::fallback;}
        auto world_api=api_.base.base.base;
        auto& h=world_api.base.base.base.base;h.struct_size=sizeof(world_api);h.version=5;
        bool world_submitted=false;
        const auto world_result=submit_world_capture_checked(world_api,session_,
            pending_world_->materials->frame(),pending_world_->materials->source_epoch(),
            pending_world_->lighting,pending_world_->complete_casters,checkpoint,this,&world_submitted);
        if(world_submitted)submitted_world_=pending_world_;
        if(world_result!=S_OK){failed(error,"world submit",world_result);return PresentationResult::fallback;}
        for(auto& wire:wires){
            if(!checkpoint(this)){fail(error,"gameplay source revoked");return PresentationResult::fallback;}
            if(serial_==std::numeric_limits<std::uint64_t>::max()){
                fail(error,"gameplay primitive serial exhausted");return PresentationResult::fallback;
            }
            wire.frame.generation=++serial_;
            const auto result=api_.base.submit_primitives(session_,&wire.frame);
            if(result==S_OK)submitted_overlay_=wire.packet;
            if(!checkpoint(this)){fail(error,"gameplay source revoked");return PresentationResult::fallback;}
            if(result!=S_OK){failed(error,"overlay submit",result);return PresentationResult::fallback;}
        }
        if(!checkpoint(this)){fail(error,"gameplay source revoked");return PresentationResult::fallback;}
        A2FO_N11_Present request{};request.struct_size=sizeof(request);
        const auto result=api_.present(session_,&request);
        if(!checkpoint(this)){fail(error,"gameplay source revoked");return PresentationResult::fallback;}
        if(result==S_FALSE){error.clear();return PresentationResult::occluded;}
        if(result!=S_OK){failed(error,"present",result);return PresentationResult::fallback;}
        accepted_=packet->identity.generation;
        current_=std::move(packet);error.clear();return PresentationResult::presented;
    }catch(...){fail(error,"gameplay packet allocation failed");return PresentationResult::fallback;}
}
bool GameplayRenderSession::resize(std::uint32_t w,std::uint32_t h,HWND window,std::string& error) noexcept {
    if(!available()){fail(error,"gameplay resize unavailable or reentrant");return false;}
    Busy guard(busy_);current_.reset();source_current_=nullptr;source_context_=nullptr;attached_=false;
    const bool detach=!w&&!h&&!window;
    if(!detach&&(!dimensions(w,h)||!window)){fail(error,"invalid gameplay target");return false;}
    if(detach){auto t=target(0,0,nullptr);const auto hr=api_.configure_presentation(session_,&t);
        if(hr!=S_OK){failed(error,"detach",hr);return false;}width_=height_=0;error.clear();return true;}
    const auto resized=base(api_).resize(session_,w,h);
    if(resized!=S_OK){failed(error,"resize",resized);return false;}
    width_=w;height_=h;auto t=target(w,h,window);
    const auto attached=api_.configure_presentation(session_,&t);
    if(attached!=S_OK){failed(error,"attach",attached);return false;}
    attached_=true;error.clear();return true;
}
bool GameplayRenderSession::readback(std::vector<std::uint8_t>& bgra,std::uint32_t& w,
    std::uint32_t& h,std::string& error) noexcept {
    if(!available()||!attached_||!current()){
        fail(error,"gameplay readback requires a current presented frame");return false;
    }
    Busy guard(busy_);checking_=current_->identity;
    try {
        if(!checkpoint(this)){current_.reset();fail(error,"gameplay source revoked");return false;}
        A2FO_N11_Readback view{};view.struct_size=sizeof(view);
        const auto metadata=base(api_).readback(session_,&view);
        if(!checkpoint(this)){current_.reset();fail(error,"gameplay source revoked");return false;}
        if(metadata!=S_FALSE||view.struct_size!=sizeof(view)||view.bgra||view.capacity||
           view.width!=width_||view.height!=height_||
           view.pitch!=width_*4||view.required_bytes!=std::uint64_t(width_)*height_*4){
            failed(error,"readback metadata",metadata);return false;
        }
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(view.required_bytes));
        view.bgra=pixels.data();view.capacity=pixels.size();
        const auto expected= view;
        if(!checkpoint(this)){current_.reset();fail(error,"gameplay source revoked");return false;}
        const auto result=base(api_).readback(session_,&view);
        if(!checkpoint(this)){current_.reset();fail(error,"gameplay source revoked");return false;}
        if(result!=S_OK||view.struct_size!=expected.struct_size||
           view.width!=expected.width||view.height!=expected.height||
           view.pitch!=expected.pitch||view.required_bytes!=expected.required_bytes||
           view.bgra!=expected.bgra||view.capacity!=expected.capacity){
            failed(error,"readback pixels",result);return false;
        }
        bgra.swap(pixels);w=width_;h=height_;error.clear();return true;
    }catch(...){fail(error,"gameplay readback allocation failed");return false;}
}
bool GameplayRenderSession::close(std::string& error) noexcept {
    if((thread_&&thread_!=GetCurrentThreadId())||busy_){fail(error,"gameplay close unavailable or reentrant");return false;}
    if(!session_){error.clear();return true;}
    Busy guard(busy_);current_.reset();source_current_=nullptr;source_context_=nullptr;pending_world_.reset();attached_=false;
    auto t=target(0,0,nullptr);const auto detached=api_.configure_presentation(session_,&t);
    if(detached!=S_OK){failed(error,"detach",detached);return false;}
    const auto destroyed=base(api_).destroy(session_);
    if(destroyed!=S_OK){failed(error,"destroy",destroyed);return false;}
    session_=0;thread_=0;width_=height_=0;serial_=accepted_=0;
    submitted_world_.reset();submitted_overlay_.reset();api_={};error.clear();return true;
}
std::shared_ptr<const GameplayFrame> GameplayRenderSession::current() const noexcept {
    if(!available()||!current_||!source_current_)return nullptr;
    Busy guard(busy_);
    if(!source_current_(source_context_,current_->identity)){
        current_.reset();source_current_=nullptr;source_context_=nullptr;return nullptr;
    }
    return current_;
}
std::uint64_t GameplayRenderSession::accepted_generation() const noexcept {return current()?accepted_:0;}
std::int32_t A2FO_N11_CALL GameplayRenderSession::read_asset(void* context,std::uint32_t kind,
    const std::uint8_t* id,std::uint32_t size,std::uint32_t limit,A2FO_N11_BytesSink sink,
    void* sink_context) noexcept {
    auto* self=static_cast<GameplayRenderSession*>(context);
    auto owner=self&&self->busy_&&self->thread_==GetCurrentThreadId()?self->pending_world_:nullptr;
    if(!owner||!owner->materials||!checkpoint(self))return 0;
    const auto result=BoundMaterialCapture::read_asset(
        const_cast<BoundMaterialCapture*>(owner->materials.get()),kind,id,size,limit,sink,sink_context);
    return checkpoint(self)?result:0;
}
std::int32_t A2FO_N11_CALL GameplayRenderSession::resolve_texture(void* context,
    const std::uint8_t* model,std::uint32_t model_size,std::uint32_t part,
    const std::uint8_t* authored,std::uint32_t authored_size,std::uint32_t limit,
    A2FO_N11_BytesSink sink,void* sink_context) noexcept {
    auto* self=static_cast<GameplayRenderSession*>(context);
    auto owner=self&&self->busy_&&self->thread_==GetCurrentThreadId()?self->pending_world_:nullptr;
    if(!owner||!owner->materials||!checkpoint(self))return 0;
    const auto result=BoundMaterialCapture::resolve_texture(
        const_cast<BoundMaterialCapture*>(owner->materials.get()),model,model_size,part,
        authored,authored_size,limit,sink,sink_context);
    return checkpoint(self)?result:0;
}
std::int32_t A2FO_N11_CALL GameplayRenderSession::resolve_part_state(void* context,
    const std::uint8_t* model,std::uint32_t size,std::uint32_t part,const float authored[16],
    A2FO_N11_PartStateSink sink,void* sink_context) noexcept {
    auto* self=static_cast<GameplayRenderSession*>(context);
    auto owner=self&&self->busy_&&self->thread_==GetCurrentThreadId()?self->pending_world_:nullptr;
    if(!owner||!owner->materials||!owner->materials->frame().requires_part_states||
       !checkpoint(self))return 0;
    const auto result=BoundMaterialCapture::resolve_part_state(
        const_cast<BoundMaterialCapture*>(owner->materials.get()),model,size,part,authored,sink,sink_context);
    return checkpoint(self)?result:0;
}
} // namespace a2fo::native11
