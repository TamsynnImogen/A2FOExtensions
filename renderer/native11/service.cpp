#include "../../sdk/include/a2fo_native11_geometry_policy_api.h"
#include "../../sdk/include/a2fo_native11_live_material_api.h"
#include "primitive_scene.hpp"
#include <type_traits>
#include "submission.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <new>

namespace n=a2fo::native11;
namespace {
static_assert(sizeof(void*)==4,"The engine service is an i686 ABI");
static_assert(sizeof(A2FO_N11_Host)==28);
static_assert(sizeof(A2FO_N11_Object)==76);
static_assert(sizeof(A2FO_N11_Light)==60);
static_assert(sizeof(A2FO_N11_Frame)==136);
static_assert(sizeof(A2FO_N11_Readback)==40);
static_assert(sizeof(A2FO_N11_Usage)==40);
static_assert(sizeof(A2FO_N11_API)==40);
static_assert(sizeof(A2FO_N11_Shadows)==16);
static_assert(sizeof(A2FO_N11_API2)==48);
static_assert(A2FO_N11_API2_SHADOWS_SIZE==44);
static_assert(sizeof(A2FO_N11_Materials)==16);
static_assert(sizeof(A2FO_N11_TextureBindings)==16);
static_assert(sizeof(A2FO_N11_API3)==52);
static_assert(offsetof(A2FO_N11_API3,configure_texture_bindings)==48);
static_assert(sizeof(A2FO_N11_PartState)==76);
static_assert(sizeof(A2FO_N11_PartStates)==16);
static_assert(sizeof(A2FO_N11_API4)==56);
static_assert(sizeof(A2FO_N11_API5)==60);
static_assert(sizeof(A2FO_N11_API6)==64);
static_assert(sizeof(A2FO_N11_API7)==68&&offsetof(A2FO_N11_API7,submit_primitives)==64);
static_assert(sizeof(A2FO_N11_API8)==76&&offsetof(A2FO_N11_API8,configure_presentation)==68&&offsetof(A2FO_N11_API8,present)==72);
static_assert(sizeof(A2FO_N11_API9)==80&&offsetof(A2FO_N11_API9,submit_world_all_parts)==76);
static_assert(sizeof(A2FO_N11_API10)==88&&offsetof(A2FO_N11_API10,configure_geometry_policies)==80&&
    offsetof(A2FO_N11_API10,submit_world_native_geometry)==84);
static_assert(sizeof(A2FO_N11_GeometryPolicy)==16&&sizeof(A2FO_N11_GeometryPolicies)==16);
static_assert(sizeof(A2FO_N11_API11)==100&&sizeof(A2FO_N11_MaterialSeed)==56&&
    sizeof(A2FO_N11_LiveMaterial)==80&&sizeof(A2FO_N11_LiveMaterials)==16);
static_assert(sizeof(A2FO_N11_PresentationTarget)==24&&sizeof(A2FO_N11_Present)==16);
static_assert(sizeof(A2FO_N11_PrimitiveFrame)==224&&sizeof(A2FO_N11_PrimitiveDraw)==160);
static_assert(sizeof(A2FO_N11_PrimitiveMesh)==24&&sizeof(A2FO_N11_PrimitiveTexture)==24);
static_assert(sizeof(A2FO_N11_Vertex)==sizeof(n::Vertex)&&sizeof(n::Vertex)==32);
static_assert(offsetof(A2FO_N11_Vertex,normal)==offsetof(n::Vertex,normal)&&offsetof(A2FO_N11_Vertex,uv)==offsetof(n::Vertex,uv));
static_assert(std::is_trivially_copyable<n::Vertex>::value);
static_assert(offsetof(A2FO_N11_API6,configure_mesh_sharing)==60);
static_assert(sizeof(A2FO_N11_MeshSharing)==16);
static_assert(offsetof(A2FO_N11_API5,submit_world)==56);
static_assert(offsetof(A2FO_N11_API4,configure_part_states)==52);
LONG owner_thread=0;
bool busy=false; // accessed only after thread admission
std::uint64_t last_session=0;
struct Session {
    A2FO_N11_Host host{};
    n::Renderer renderer;
    std::unique_ptr<n::SceneSubmission> scene; // destroyed before renderer
    std::unique_ptr<n::PrimitiveScene> primitives;
    n::ShadowSettings shadows{};
    n::StaticMaterialPolicy materials{};
    A2FO_N11_TextureBindings bindings{sizeof(A2FO_N11_TextureBindings),0,nullptr,nullptr};
    A2FO_N11_PartStates part_states{sizeof(A2FO_N11_PartStates),0,nullptr,nullptr};
    A2FO_N11_GeometryPolicies geometry_policies{sizeof(A2FO_N11_GeometryPolicies),0,nullptr,nullptr};
    A2FO_N11_LiveMaterials live_materials{sizeof(A2FO_N11_LiveMaterials),0,nullptr,nullptr};
    std::uint64_t material_epoch=0,material_generation=0;
    std::uint32_t width=0,height=0;
    bool presentation_eligible=false;
    struct Sink {
        std::vector<std::uint8_t>* output;
        std::uint32_t limit;
        unsigned calls=0;
        bool accepted=false;
    };
    static int32_t A2FO_N11_CALL sink(void* context,const uint8_t* bytes,uint32_t count) noexcept {
        auto& s=*static_cast<Sink*>(context);
        ++s.calls;
        if(s.calls!=1||!bytes||!count||count>s.limit){s.accepted=false;return 0;}
        try{s.output->assign(bytes,bytes+count);s.accepted=true;return 1;}
        catch(...){return 0;}
    }
    static bool read(void* context,n::AssetKind kind,const std::string& id,
                     std::vector<std::uint8_t>& bytes,std::string& error){
        auto& self=*static_cast<Session*>(context);
        const auto limit=kind==n::AssetKind::model?64u*1024*1024:256u*1024*1024;
        std::vector<std::uint8_t> copy;Sink s{&copy,limit,0,false};
        const auto result=self.host.read_asset(self.host.context,kind==n::AssetKind::model?1:2,
            reinterpret_cast<const uint8_t*>(id.data()),static_cast<uint32_t>(id.size()),limit,sink,&s);
        if(result!=1||s.calls!=1||!s.accepted){error="host asset read/single-sink contract failed";return false;}
        bytes=std::move(copy);return true;
    }
    static bool resolve_texture(void* context,const std::string& model,std::uint32_t part,
        const std::string& authored,std::string& output,std::string& error){
        auto& self=*static_cast<Session*>(context);
        if(!self.bindings.resolve){error="texture binding callback unavailable";return false;}
        std::vector<std::uint8_t> copy;Sink s{&copy,1024,0,false};
        const auto result=self.bindings.resolve(self.bindings.context,
            reinterpret_cast<const uint8_t*>(model.data()),static_cast<uint32_t>(model.size()),part,
            reinterpret_cast<const uint8_t*>(authored.data()),static_cast<uint32_t>(authored.size()),
            1024,sink,&s);
        if(result!=1||s.calls!=1||!s.accepted){error="host texture binding/single-sink contract failed";return false;}
        for(auto c:copy)if(c<32||c>126){error="host texture binding identifier invalid";return false;}
        output.assign(copy.begin(),copy.end());return true;
    }
    struct StateSink {
        n::PartState* output;
        unsigned calls=0;
        bool accepted=false;
    };
    static int32_t A2FO_N11_CALL state_sink(void* context,const A2FO_N11_PartState* state) noexcept {
        auto& sink=*static_cast<StateSink*>(context);
        ++sink.calls;
        if(sink.calls!=1||!state||state->struct_size!=sizeof(*state)||
           state->reserved||state->visible>1){sink.accepted=false;return 0;}
        sink.output->visible=state->visible!=0;
        std::copy(state->local_to_model,state->local_to_model+16,sink.output->local_to_model);
        sink.accepted=true;return 1;
    }
    static bool resolve_part_state(void* context,const std::string& model,std::uint32_t part,
        const float authored[16],n::PartState& output,std::string& error){
        auto& self=*static_cast<Session*>(context);
        if(!self.part_states.resolve){error="part state callback unavailable";return false;}
        float private_authored[16];
        std::copy(authored,authored+16,private_authored);
        StateSink sink{&output,0,false};
        const auto result=self.part_states.resolve(self.part_states.context,
            reinterpret_cast<const uint8_t*>(model.data()),static_cast<uint32_t>(model.size()),
            part,private_authored,state_sink,&sink);
        if(result!=1||sink.calls!=1||!sink.accepted){error="host part state/single-sink contract failed";return false;}
        return true;
    }
    struct GeometrySink {
        n::ModelGeometryPolicy value{};
        unsigned calls=0;bool accepted=false;
    };
    static int32_t A2FO_N11_CALL geometry_sink(void* context,const A2FO_N11_GeometryPolicy* policy) noexcept {
        auto& state=*static_cast<GeometrySink*>(context);++state.calls;
        state.accepted=false;
        if(state.calls!=1||!policy||policy->struct_size!=sizeof(*policy)||policy->reserved)return 0;
        state.value={policy->parser_scalar_bits,policy->native_x87_control};
        state.accepted=n::valid_model_geometry_policy(state.value);return state.accepted?1:0;
    }
    static bool resolve_geometry_policy(void* context,const std::string& model,
        n::ModelGeometryPolicy& output,std::string& error){
        auto& self=*static_cast<Session*>(context);GeometrySink state;
        if(!self.geometry_policies.resolve){error="native geometry policy callback unavailable";return false;}
        const auto result=self.geometry_policies.resolve(self.geometry_policies.context,
            reinterpret_cast<const std::uint8_t*>(model.data()),static_cast<std::uint32_t>(model.size()),
            geometry_sink,&state);
        if(result!=1||state.calls!=1||!state.accepted){error="native geometry policy/single-sink contract failed";return false;}
        output=state.value;error.clear();return true;
    }
    struct LiveSink {n::LiveMaterial value;unsigned calls=0;bool accepted=false;};
    static int32_t A2FO_N11_CALL live_sink(void* context,const A2FO_N11_LiveMaterial* material) noexcept {
        auto& state=*static_cast<LiveSink*>(context);++state.calls;state.accepted=false;
        if(state.calls!=1||!material||material->struct_size!=sizeof(*material)||
           material->texture_identifier_length>1024||
           (material->texture_identifier_length&&!material->texture_identifier))return 0;
        try {
            n::LiveMaterial value;value.flags=material->flags;
            value.sampler=static_cast<n::Sampling>(material->sampler);
            value.depth_mode=static_cast<n::DepthMode>(material->depth_mode);
            std::copy(material->colour,material->colour+4,value.colour);
            std::copy(material->emissive,material->emissive+3,value.emissive);
            std::copy(material->uv_transform,material->uv_transform+6,value.uv_transform);
            value.alpha_cutoff=material->alpha_cutoff;
            if(material->texture_identifier_length)value.texture_identifier.assign(
                reinterpret_cast<const char*>(material->texture_identifier),material->texture_identifier_length);
            if(!n::valid_live_material(value))return 0;
            state.value=std::move(value);state.accepted=true;return 1;
        }catch(...){return 0;}
    }
    static bool resolve_live_material(void* context,const std::string& model,
        const n::MaterialSeed& seed,n::LiveMaterial& output,std::string& error){
        auto& self=*static_cast<Session*>(context);
        if(!self.live_materials.resolve){error="live material callback unavailable";return false;}
        A2FO_N11_MaterialSeed input{};input.struct_size=sizeof(input);input.part_index=seed.part_index;
        input.node_index=seed.node_index;input.group_index=seed.group_index;input.flags=seed.flags;
        input.authored_texture=reinterpret_cast<const std::uint8_t*>(seed.authored_texture.data());
        input.authored_texture_length=static_cast<std::uint32_t>(seed.authored_texture.size());
        std::copy(seed.colour,seed.colour+4,input.colour);std::copy(seed.emissive,seed.emissive+3,input.emissive);
        LiveSink state;
        const auto result=self.live_materials.resolve(self.live_materials.context,
            reinterpret_cast<const std::uint8_t*>(model.data()),static_cast<std::uint32_t>(model.size()),
            &input,live_sink,&state);
        if(result!=1||state.calls!=1||!state.accepted){error="live material/single-sink contract failed";return false;}
        output=std::move(state.value);error.clear();return true;
    }
    n::AssetReader reader(bool texture_scoped,bool state_scoped=false,bool geometry_scoped=false,
        int live_scoped=-1) noexcept {
        return {this,Session::read,texture_scoped?Session::resolve_texture:nullptr,
            state_scoped?Session::resolve_part_state:nullptr,
            geometry_scoped?Session::resolve_geometry_policy:nullptr,
            (live_scoped<0?live_materials.resolve!=nullptr:live_scoped!=0)?Session::resolve_live_material:nullptr};
    }
    void report(const std::string& error)const{
        if(host.log&&!error.empty())host.log(host.context,error.data(),static_cast<uint32_t>(error.size()));
    }
};
// No GPU object has a static destructor under the loader lock. The registry is
// created on first successful admission and deleted after the last destroy.
// Hosts must destroy all sessions before FreeLibrary, as required by the ABI.
using Sessions=std::map<std::uint64_t,std::unique_ptr<Session>>;
Sessions* sessions=nullptr;
struct Admission {
    bool entered=false;
    bool release_owner=false;
    explicit Admission(bool create=false) noexcept {
        const LONG current=static_cast<LONG>(GetCurrentThreadId());
        const LONG owner=InterlockedCompareExchange(&owner_thread,create?current:0,0);
        if((owner!=current&&!(create&&owner==0))||busy)return;
        busy=true;entered=true;release_owner=create&&owner==0;
    }
    ~Admission(){if(entered){busy=false;if(release_owner)InterlockedExchange(&owner_thread,0);}}
};
Session* find(A2FO_N11_Session id){
    if(!sessions)return nullptr;
    const auto it=sessions->find(id);return it==sessions->end()?nullptr:it->second.get();
}
template<class F> int32_t protect(F&& f) noexcept {
    try{return f();}catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
}
int32_t A2FO_N11_CALL create(const A2FO_N11_Host* host,A2FO_N11_Session* output) noexcept {
    if(!host||!output||host->struct_size!=sizeof(*host)||host->flags||!host->read_asset)return E_INVALIDARG;
    Admission admission(true);if(!admission.entered)return E_UNEXPECTED;
    return protect([&]()->HRESULT{
        if(last_session==UINT64_MAX||(sessions&&sessions->size()>=4))return E_OUTOFMEMORY;
        auto value=std::make_unique<Session>();value->host=*host;
        HRESULT hr=value->renderer.initialize_offscreen(host->width,host->height);
        if(FAILED(hr))return hr;
        value->width=host->width;value->height=host->height;
        value->scene=std::make_unique<n::SceneSubmission>(value->renderer,value->reader(false));
        auto fresh= sessions?std::unique_ptr<Sessions>{}:std::make_unique<Sessions>();
        auto& registry=sessions?*sessions:*fresh;
        const auto id=last_session+1;
        registry.emplace(id,std::move(value));
        if(fresh)sessions=fresh.release();
        last_session=id;*output=id;admission.release_owner=false;return S_OK;
    });
}
int32_t A2FO_N11_CALL destroy(A2FO_N11_Session id) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    if(!find(id))return E_INVALIDARG;
    sessions->erase(id);if(sessions->empty()){delete sessions;sessions=nullptr;}return S_OK;
}
int32_t submit_impl(A2FO_N11_Session id,const A2FO_N11_Frame* input,bool world,bool all_parts=false,
    bool native_geometry=false,bool live_preview=false) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    self->presentation_eligible=false;
    if(!input||input->struct_size!=sizeof(*input)||(input->flags&~(A2FO_N11_FRAME_REQUIRE_PART_STATES|(world?A2FO_N11_FRAME_COMPLETE_CASTERS:0)))||
       (world&&!(input->flags&A2FO_N11_FRAME_COMPLETE_CASTERS))||
       (all_parts&&((!world&&!live_preview)||!(input->flags&A2FO_N11_FRAME_REQUIRE_PART_STATES)||!self->part_states.resolve))||
       (live_preview&&!self->live_materials.resolve)||
       (native_geometry&&(!all_parts||!self->geometry_policies.resolve))||
       ((input->flags&A2FO_N11_FRAME_REQUIRE_PART_STATES)&&!self->part_states.resolve)||!input->generation||!input->source_epoch||
       input->object_count>n::capacity||input->light_count>8||
       (input->object_count&&!input->objects)||(input->light_count&&!input->lights))return E_INVALIDARG;
    return protect([&]()->HRESULT{
        auto frame=std::make_unique<n::Frame>();frame->camera=1;frame->generation=input->generation;
        frame->requires_part_states=(input->flags&A2FO_N11_FRAME_REQUIRE_PART_STATES)!=0;
        frame->complete_casters=world;
        frame->object_count=input->object_count;
        std::copy(input->world_to_clip,input->world_to_clip+16,frame->view.world_to_clip);
        std::copy(input->viewport,input->viewport+4,frame->view.viewport);
        for(std::size_t i=0;i<frame->object_count;++i){
            const auto& src=input->objects[i];auto& dst=frame->objects[i];
            if((world?(src.reserved&~A2FO_N11_OBJECT_SHADOW_ONLY):src.reserved)||!src.model_identifier||!src.model_identifier_length||src.model_identifier_length>=sizeof(dst.model_name))return E_INVALIDARG;
            for(std::uint32_t j=0;j<src.model_identifier_length;++j)
                if(src.model_identifier[j]<32||src.model_identifier[j]>126)return E_INVALIDARG;
            std::copy(src.local_to_world,src.local_to_world+16,dst.world);
            std::memcpy(dst.model_name,src.model_identifier,src.model_identifier_length);
            dst.model_identity=n::ModelIdentity::plain_database_name;
            dst.shadow_only=world&&(src.reserved&A2FO_N11_OBJECT_SHADOW_ONLY);
        }
        std::array<n::Light,8> lights{};
        std::array<float,3> ambient{};
        std::copy(input->ambient,input->ambient+3,ambient.begin());
        for(std::uint32_t i=0;i<input->light_count;++i){const auto& src=input->lights[i];auto& dst=lights[i];
            dst.type=static_cast<n::LightType>(src.type);
            std::copy(src.position,src.position+3,dst.position);std::copy(src.direction,src.direction+3,dst.direction);
            std::copy(src.colour,src.colour+3,dst.colour);dst.intensity=src.intensity;
            dst.falloff_start=src.falloff_start;dst.falloff_range=src.falloff_range;
            dst.cone_inner_degrees=src.cone_inner_degrees;dst.cone_outer_degrees=src.cone_outer_degrees;
        }
        if(self->materials.all_model_parts!=all_parts||self->materials.native_geometry!=native_geometry){
            auto policy=self->materials;policy.all_model_parts=all_parts;policy.native_geometry=native_geometry;
            auto scene=std::make_unique<n::SceneSubmission>(self->renderer,self->reader(self->bindings.resolve!=nullptr,
                self->part_states.resolve!=nullptr,native_geometry),policy);
            self->scene=std::move(scene);self->materials=policy;
        }
        std::string error;const auto hr=self->scene->render_preview(*frame,input->source_epoch,lights.data(),input->light_count,ambient.data(),error,live_preview?n::ShadowSettings{}:self->shadows);
        if(FAILED(hr))self->report(error);
        self->presentation_eligible=hr==S_OK&&!live_preview;
        return hr;
    });
}
int32_t A2FO_N11_CALL submit(A2FO_N11_Session id,const A2FO_N11_Frame* input) noexcept {return submit_impl(id,input,false);}
int32_t A2FO_N11_CALL submit_world(A2FO_N11_Session id,const A2FO_N11_Frame* input) noexcept {return submit_impl(id,input,true);}
int32_t A2FO_N11_CALL submit_world_all_parts(A2FO_N11_Session id,const A2FO_N11_Frame* input) noexcept {return submit_impl(id,input,true,true);}
int32_t A2FO_N11_CALL submit_world_native_geometry(A2FO_N11_Session id,const A2FO_N11_Frame* input) noexcept {return submit_impl(id,input,true,true,true);}
int32_t A2FO_N11_CALL submit_live_material_preview(A2FO_N11_Session id,const A2FO_N11_Frame* input) noexcept {
    return submit_impl(id,input,false,true,true,true);
}
int32_t A2FO_N11_CALL prepare_live_materials(A2FO_N11_Session id,const A2FO_N11_Frame* input) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    self->presentation_eligible=false;
    if(!input||input->struct_size!=sizeof(*input)||input->flags&~A2FO_N11_FRAME_REQUIRE_PART_STATES||
       !input->generation||!input->source_epoch||input->object_count>n::capacity||
       (input->object_count&&!input->objects)||input->light_count||!self->live_materials.resolve||
       !self->geometry_policies.resolve||
       (self->material_epoch==input->source_epoch&&input->generation<=self->material_generation))return E_INVALIDARG;
    return protect([&]()->HRESULT{
        auto frame=std::make_unique<n::Frame>();frame->camera=1;frame->generation=input->generation;
        frame->object_count=input->object_count;
        for(std::size_t i=0;i<frame->object_count;++i){const auto& src=input->objects[i];auto& dst=frame->objects[i];
            if(src.reserved||!src.model_identifier||!src.model_identifier_length||src.model_identifier_length>=sizeof(dst.model_name))return E_INVALIDARG;
            for(std::uint32_t k=0;k<src.model_identifier_length;++k)
                if(src.model_identifier[k]<32||src.model_identifier[k]>126)return E_INVALIDARG;
            std::memcpy(dst.model_name,src.model_identifier,src.model_identifier_length);
            dst.model_identity=n::ModelIdentity::plain_database_name;
            std::copy(src.local_to_world,src.local_to_world+16,dst.world);
        }
        if(!self->materials.all_model_parts||!self->materials.native_geometry){
            auto policy=self->materials;policy.all_model_parts=policy.native_geometry=true;
            self->scene=std::make_unique<n::SceneSubmission>(self->renderer,
                self->reader(false,self->part_states.resolve!=nullptr,true),policy);
            self->materials=policy;
        }
        std::string error;const auto hr=self->scene->prepare_live_materials(*frame,input->source_epoch,error);
        if(hr!=S_OK){self->report(error);return hr;}
        self->material_epoch=input->source_epoch;self->material_generation=input->generation;return S_OK;
    });
}
int32_t A2FO_N11_CALL submit_primitives(A2FO_N11_Session id,const A2FO_N11_PrimitiveFrame* input) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    self->presentation_eligible=false;
    if(!input||input->struct_size!=sizeof(*input)||(input->flags&~3u)||input->reserved||
       !input->generation||!input->source_epoch||input->fog_enabled>1||
       input->mesh_count>2048||input->texture_count>256||input->draw_count>8192||input->light_count>8||
       (input->mesh_count&&!input->meshes)||(input->texture_count&&!input->textures)||
       (input->draw_count&&!input->draws)||(input->light_count&&!input->lights))return E_INVALIDARG;
    return protect([&]()->HRESULT{
        constexpr std::uint64_t limit=128ull*1024*1024;
        std::uint64_t vertices=0,indices=0,bytes=0;
        for(std::uint32_t i=0;i<input->mesh_count;++i){const auto& m=input->meshes[i];
            if(!m.identity||m.vertex_count<3||m.vertex_count>1000000-vertices||
               m.index_count<3||m.index_count>3000000-indices||m.index_count%3||!m.vertices||!m.indices)return E_INVALIDARG;
            vertices+=m.vertex_count;indices+=m.index_count;
            const auto size=std::uint64_t(m.vertex_count)*sizeof(n::Vertex)+std::uint64_t(m.index_count)*4;
            if(size>limit-bytes)return E_INVALIDARG;
            bytes+=size;
        }
        for(std::uint32_t i=0;i<input->texture_count;++i){const auto& t=input->textures[i];
            if(!t.identity||!t.width||!t.height||t.width>16384||t.height>16384||!t.bgra||
               std::uint64_t(t.width)*t.height*4!=t.byte_count||t.byte_count>limit-bytes)return E_INVALIDARG;
            bytes+=t.byte_count;
        }
        for(std::uint32_t i=0;i<input->draw_count;++i)
            if(input->draws[i].reserved||(input->draws[i].options&~15u))return E_INVALIDARG;
        n::PrimitiveFrame frame;frame.generation=input->generation;frame.source_epoch=input->source_epoch;
        auto& settings=frame.settings;
        std::copy(input->world_to_clip,input->world_to_clip+16,settings.world_to_clip);
        std::copy(input->viewport,input->viewport+4,settings.viewport);
        std::copy(input->ambient,input->ambient+3,settings.ambient);
        std::copy(input->clear_colour,input->clear_colour+4,settings.clear_colour);
        settings.ordering=static_cast<n::DrawOrdering>(input->ordering);
        settings.clear_colour_target=!(input->flags&A2FO_N11_PRIMITIVE_LOAD_COLOUR);
        settings.clear_depth_target=!(input->flags&A2FO_N11_PRIMITIVE_LOAD_DEPTH);
        settings.shadows.light_mask=input->shadow_mask;settings.shadows.resolution=input->shadow_resolution;
        settings.shadows.depth_bias=input->shadow_bias;settings.shadows.filter=static_cast<n::ShadowFilter>(input->shadow_filter);
        settings.fog.enabled=input->fog_enabled!=0;settings.fog.start=input->fog_start;settings.fog.end=input->fog_end;
        std::copy(input->fog_colour,input->fog_colour+3,settings.fog.colour);
        std::copy(input->camera_position,input->camera_position+3,settings.fog.camera_position);
        frame.meshes.reserve(input->mesh_count);frame.textures.reserve(input->texture_count);
        frame.draws.reserve(input->draw_count);frame.lights.reserve(input->light_count);
        for(std::uint32_t i=0;i<input->mesh_count;++i){const auto& src=input->meshes[i];n::PrimitiveMesh mesh;
            mesh.id=src.identity;mesh.vertices.resize(src.vertex_count);mesh.indices.assign(src.indices,src.indices+src.index_count);
            std::memcpy(mesh.vertices.data(),src.vertices,std::size_t(src.vertex_count)*sizeof(n::Vertex));frame.meshes.push_back(std::move(mesh));
        }
        for(std::uint32_t i=0;i<input->texture_count;++i){const auto& src=input->textures[i];n::PrimitiveTexture texture;
            texture.id=src.identity;texture.width=src.width;texture.height=src.height;
            texture.bgra.assign(src.bgra,src.bgra+src.byte_count);frame.textures.push_back(std::move(texture));
        }
        for(std::uint32_t i=0;i<input->draw_count;++i){const auto& src=input->draws[i];
            n::PrimitiveDraw draw;draw.mesh=src.mesh;draw.texture=src.texture;auto& surface=draw.surface;
            std::copy(src.local_to_world,src.local_to_world+16,surface.local_to_world);
            std::copy(src.colour,src.colour+4,surface.colour);std::copy(src.emissive,src.emissive+3,surface.emissive);
            surface.flags=src.flags;surface.layer=src.layer;surface.depth_mode=static_cast<n::DepthMode>(src.depth_mode);
            surface.sampler=static_cast<n::Sampling>(src.sampling);surface.alpha_cutoff=src.alpha_cutoff;
            std::copy(src.uv_transform,src.uv_transform+6,surface.uv_transform);
            surface.shadow_only=(src.options&A2FO_N11_SURFACE_SHADOW_ONLY)!=0;
            surface.casts_shadow=!(src.options&A2FO_N11_SURFACE_NO_CAST);
            surface.receives_shadow=!(src.options&A2FO_N11_SURFACE_NO_RECEIVE);
            surface.fog_enabled=!(src.options&A2FO_N11_SURFACE_NO_FOG);
            frame.draws.push_back(draw);
        }
        for(std::uint32_t i=0;i<input->light_count;++i){const auto& src=input->lights[i];n::Light light;
            light.type=static_cast<n::LightType>(src.type);
            std::copy(src.position,src.position+3,light.position);std::copy(src.direction,src.direction+3,light.direction);
            std::copy(src.colour,src.colour+3,light.colour);light.intensity=src.intensity;
            light.falloff_start=src.falloff_start;light.falloff_range=src.falloff_range;
            light.cone_inner_degrees=src.cone_inner_degrees;light.cone_outer_degrees=src.cone_outer_degrees;frame.lights.push_back(light);
        }
        if(!self->primitives)self->primitives=std::make_unique<n::PrimitiveScene>(self->renderer);
        std::string error;const auto hr=self->primitives->render(frame,error);if(FAILED(hr))self->report(error);
        self->presentation_eligible=hr==S_OK;return hr;
    });
}
int32_t A2FO_N11_CALL resize(A2FO_N11_Session id,uint32_t width,uint32_t height) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    self->presentation_eligible=false;
    return protect([&]()->HRESULT{const auto hr=self->renderer.resize(width,height);
        if(SUCCEEDED(hr)){self->width=width;self->height=height;}return hr;});
}
int32_t A2FO_N11_CALL readback(A2FO_N11_Session id,A2FO_N11_Readback* output) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    if(!output||output->struct_size!=sizeof(*output)||(!output->bgra&&output->capacity))return E_INVALIDARG;
    output->width=self->width;output->height=self->height;output->pitch=self->width*4;
    output->required_bytes=std::uint64_t(output->pitch)*self->height;
    if(!output->bgra)return S_FALSE;
    if(output->capacity<output->required_bytes)return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    return protect([&]()->HRESULT{
        std::vector<std::uint8_t> bytes;std::uint32_t width=0,height=0;
        const auto hr=self->renderer.readback(bytes,width,height);if(FAILED(hr))return hr;
        if(width!=self->width||height!=self->height||bytes.size()!=output->required_bytes)return E_UNEXPECTED;
        std::memcpy(output->bgra,bytes.data(),bytes.size());return S_OK;
    });
}
int32_t A2FO_N11_CALL usage(A2FO_N11_Session id,A2FO_N11_Usage* output) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    if(!output||output->struct_size!=sizeof(*output)||output->reserved)return E_INVALIDARG;
    n::ResourceUsage v{};const auto hr=self->renderer.resource_usage(v);if(FAILED(hr))return hr;
    const A2FO_N11_Usage result{sizeof(*output),0,v.epoch,v.mesh_bytes,v.texture_bytes,v.meshes,v.textures};
    *output=result;return S_OK;
}
int32_t A2FO_N11_CALL configure_presentation(A2FO_N11_Session id,const A2FO_N11_PresentationTarget* input) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    self->presentation_eligible=false;
    if(!input||input->struct_size!=sizeof(*input)||input->flags||input->reserved)return E_INVALIDARG;
    return protect([&]()->HRESULT{return self->renderer.configure_presentation(static_cast<HWND>(input->window),input->width,input->height);});
}
int32_t A2FO_N11_CALL present(A2FO_N11_Session id,const A2FO_N11_Present* input) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    const bool eligible=self->presentation_eligible;self->presentation_eligible=false;
    if(!input||input->struct_size!=sizeof(*input)||input->sync_interval>4||input->flags||input->reserved)return E_INVALIDARG;
    if(!eligible)return E_UNEXPECTED;
    return protect([&]()->HRESULT{return self->renderer.present(input->sync_interval);});
}
int32_t A2FO_N11_CALL configure_shadows(A2FO_N11_Session id,const A2FO_N11_Shadows* input) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    self->presentation_eligible=false;
    // The documented inclusive endpoint is the stored binary32 value.
    if(!input||input->struct_size!=sizeof(*input)||input->light_mask>255||
       input->resolution<128||input->resolution>2048||(input->resolution&(input->resolution-1))||
       !std::isfinite(input->depth_bias)||input->depth_bias<0||input->depth_bias>0x1.99999ap-4f)return E_INVALIDARG;
    self->shadows={input->light_mask,input->resolution,input->depth_bias};return S_OK;
}
int32_t A2FO_N11_CALL configure_materials(A2FO_N11_Session id,const A2FO_N11_Materials* input) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    self->presentation_eligible=false;
    if(!input||input->struct_size!=sizeof(*input)||input->relight_fullbright>1||input->reserved||
       !std::isfinite(input->retained_emission)||input->retained_emission<0||input->retained_emission>1)return E_INVALIDARG;
    auto policy=self->materials;
    policy.relight_fullbright=input->relight_fullbright!=0;policy.retained_emission=input->retained_emission;
    if(policy.relight_fullbright==self->materials.relight_fullbright&&
       policy.retained_emission==self->materials.retained_emission)return S_OK;
    return protect([&]()->HRESULT{
        auto scene=std::make_unique<n::SceneSubmission>(self->renderer,self->reader(self->bindings.resolve!=nullptr,
            self->part_states.resolve!=nullptr,self->materials.native_geometry),policy);
        self->scene=std::move(scene);self->materials=policy;return S_OK;
    });
}
int32_t A2FO_N11_CALL configure_mesh_sharing(A2FO_N11_Session id,const A2FO_N11_MeshSharing* input) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    self->presentation_eligible=false;
    if(!input||input->struct_size!=sizeof(*input)||input->enabled>1||
       input->reserved[0]||input->reserved[1])return E_INVALIDARG;
    if(self->materials.share_geometry==(input->enabled!=0))return S_OK;
    return protect([&]()->HRESULT{
        auto policy=self->materials;policy.share_geometry=input->enabled!=0;
        auto scene=std::make_unique<n::SceneSubmission>(self->renderer,self->reader(self->bindings.resolve!=nullptr,
            self->part_states.resolve!=nullptr,self->materials.native_geometry),policy);
        self->scene=std::move(scene);self->materials=policy;return S_OK;
    });
}
int32_t A2FO_N11_CALL configure_texture_bindings(A2FO_N11_Session id,const A2FO_N11_TextureBindings* input) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    self->presentation_eligible=false;
    if(!input||input->struct_size!=sizeof(*input)||input->reserved||(!input->resolve&&input->context))return E_INVALIDARG;
    if(input->resolve==self->bindings.resolve&&input->context==self->bindings.context)return S_OK;
    return protect([&]()->HRESULT{
        auto scene=std::make_unique<n::SceneSubmission>(self->renderer,self->reader(input->resolve!=nullptr,
            self->part_states.resolve!=nullptr,self->materials.native_geometry),self->materials);
        self->scene=std::move(scene);self->bindings=*input;return S_OK;
    });
}
int32_t A2FO_N11_CALL configure_part_states(A2FO_N11_Session id,const A2FO_N11_PartStates* input) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    self->presentation_eligible=false;
    if(!input||input->struct_size!=sizeof(*input)||input->reserved||(!input->resolve&&input->context))return E_INVALIDARG;
    if(input->resolve==self->part_states.resolve&&input->context==self->part_states.context)return S_OK;
    return protect([&]()->HRESULT{
        auto scene=std::make_unique<n::SceneSubmission>(self->renderer,
            self->reader(self->bindings.resolve!=nullptr,input->resolve!=nullptr,self->materials.native_geometry),self->materials);
        self->scene=std::move(scene);self->part_states=*input;return S_OK;
    });
}
int32_t A2FO_N11_CALL configure_geometry_policies(A2FO_N11_Session id,const A2FO_N11_GeometryPolicies* input) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    self->presentation_eligible=false;
    if(!input||input->struct_size!=sizeof(*input)||input->reserved||(!input->resolve&&input->context))return E_INVALIDARG;
    if(input->resolve==self->geometry_policies.resolve&&input->context==self->geometry_policies.context)return S_OK;
    return protect([&]()->HRESULT{
        // Reader holds only this Session; install the new callback after the
        // replacement scene is allocated successfully, before any future read.
        auto scene=std::make_unique<n::SceneSubmission>(self->renderer,
            self->reader(self->bindings.resolve!=nullptr,self->part_states.resolve!=nullptr,
                self->materials.native_geometry),self->materials);
        self->scene=std::move(scene);self->geometry_policies=*input;return S_OK;
    });
}
int32_t A2FO_N11_CALL configure_live_materials(A2FO_N11_Session id,const A2FO_N11_LiveMaterials* input) noexcept {
    Admission admission;if(!admission.entered)return E_UNEXPECTED;
    auto* self=find(id);if(!self)return E_INVALIDARG;
    self->presentation_eligible=false;
    if(!input||input->struct_size!=sizeof(*input)||input->reserved||(!input->resolve&&input->context))return E_INVALIDARG;
    if(input->resolve==self->live_materials.resolve&&input->context==self->live_materials.context)return S_OK;
    return protect([&]()->HRESULT{
        auto scene=std::make_unique<n::SceneSubmission>(self->renderer,
            self->reader(self->bindings.resolve!=nullptr,self->part_states.resolve!=nullptr,
                self->materials.native_geometry,input->resolve!=nullptr),self->materials);
        self->scene=std::move(scene);self->live_materials=*input;
        self->material_epoch=self->material_generation=0;return S_OK;
    });
}
}
extern "C" int32_t A2FO_N11_CALL A2FO_Native11_GetInterface(uint32_t version,uint32_t size,void* output) noexcept {
    if(version==A2FO_N11_API_VERSION){
        if(!output||size!=sizeof(A2FO_N11_API))return E_INVALIDARG;
        const A2FO_N11_API api{sizeof(api),version,A2FO_N11_CAP_OFFSCREEN_STATIC,0,
            create,destroy,submit,resize,readback,usage};std::memcpy(output,&api,sizeof(api));return S_OK;
    }
    if(version==A2FO_N11_API_VERSION_SHADOWS){
        if(!output||(size!=sizeof(A2FO_N11_API2)&&size!=A2FO_N11_API2_SHADOWS_SIZE))return E_INVALIDARG;
        const auto capabilities=A2FO_N11_CAP_OFFSCREEN_STATIC|A2FO_N11_CAP_OFFSCREEN_SHADOWS|
            (size==sizeof(A2FO_N11_API2)?A2FO_N11_CAP_MATERIAL_RELIGHT:0);
        const A2FO_N11_API2 api{{size,version,capabilities,0,
            create,destroy,submit,resize,readback,usage},configure_shadows,configure_materials};
        std::memcpy(output,&api,size);return S_OK;
    }
    if(version==A2FO_N11_API_VERSION_TEXTURE_BINDINGS){
        if(!output||size!=sizeof(A2FO_N11_API3))return E_INVALIDARG;
        const A2FO_N11_API3 api{{{size,version,A2FO_N11_CAP_OFFSCREEN_STATIC|
            A2FO_N11_CAP_OFFSCREEN_SHADOWS|A2FO_N11_CAP_MATERIAL_RELIGHT|A2FO_N11_CAP_TEXTURE_BINDINGS,0,
            create,destroy,submit,resize,readback,usage},configure_shadows,configure_materials},configure_texture_bindings};
        std::memcpy(output,&api,sizeof(api));return S_OK;
    }
    if(version==A2FO_N11_API_VERSION_PART_STATES){
        if(!output||size!=sizeof(A2FO_N11_API4))return E_INVALIDARG;
        const A2FO_N11_API4 api{{{{size,version,A2FO_N11_CAP_OFFSCREEN_STATIC|
            A2FO_N11_CAP_OFFSCREEN_SHADOWS|A2FO_N11_CAP_MATERIAL_RELIGHT|
            A2FO_N11_CAP_TEXTURE_BINDINGS|A2FO_N11_CAP_PART_STATES,0,
            create,destroy,submit,resize,readback,usage},configure_shadows,configure_materials},
            configure_texture_bindings},configure_part_states};
        std::memcpy(output,&api,sizeof(api));return S_OK;
    }
    if(version==A2FO_N11_API_VERSION_WORLD){
        if(!output||size!=sizeof(A2FO_N11_API5))return E_INVALIDARG;
        const A2FO_N11_API5 api{{{{{size,version,A2FO_N11_CAP_OFFSCREEN_STATIC|
            A2FO_N11_CAP_OFFSCREEN_SHADOWS|A2FO_N11_CAP_MATERIAL_RELIGHT|
            A2FO_N11_CAP_TEXTURE_BINDINGS|A2FO_N11_CAP_PART_STATES|A2FO_N11_CAP_WORLD_CASTERS,0,
            create,destroy,submit,resize,readback,usage},configure_shadows,configure_materials},
            configure_texture_bindings},configure_part_states},submit_world};
        std::memcpy(output,&api,sizeof(api));return S_OK;
    }
    if(version==A2FO_N11_API_VERSION_GEOMETRY){
        if(!output||size!=sizeof(A2FO_N11_API6))return E_INVALIDARG;
        const A2FO_N11_API6 api{{{{{{size,version,A2FO_N11_CAP_OFFSCREEN_STATIC|
            A2FO_N11_CAP_OFFSCREEN_SHADOWS|A2FO_N11_CAP_MATERIAL_RELIGHT|
            A2FO_N11_CAP_TEXTURE_BINDINGS|A2FO_N11_CAP_PART_STATES|A2FO_N11_CAP_WORLD_CASTERS|
            A2FO_N11_CAP_MESH_SHARING,0,
            create,destroy,submit,resize,readback,usage},configure_shadows,configure_materials},
            configure_texture_bindings},configure_part_states},submit_world},configure_mesh_sharing};
        std::memcpy(output,&api,sizeof(api));return S_OK;
    }
    if(version==A2FO_N11_API_VERSION_PRIMITIVES){
        if(!output||size!=sizeof(A2FO_N11_API7))return E_INVALIDARG;
        A2FO_N11_API7 api{};
        if(A2FO_Native11_GetInterface(A2FO_N11_API_VERSION_GEOMETRY,sizeof(api.base),&api.base)<0)return E_NOINTERFACE;
        auto& header=api.base.base.base.base.base.base;
        header.struct_size=size;header.version=version;header.capabilities|=A2FO_N11_CAP_PRIMITIVES;
        api.submit_primitives=submit_primitives;std::memcpy(output,&api,sizeof(api));return S_OK;
    }
    if(version==A2FO_N11_API_VERSION_PRESENTATION){
        if(!output||size!=sizeof(A2FO_N11_API8))return E_INVALIDARG;
        A2FO_N11_API8 api{};
        if(A2FO_Native11_GetInterface(A2FO_N11_API_VERSION_PRIMITIVES,sizeof(api.base),&api.base)<0)return E_NOINTERFACE;
        auto& header=api.base.base.base.base.base.base.base;
        header.struct_size=size;header.version=version;header.capabilities|=A2FO_N11_CAP_PRESENTATION;
        api.configure_presentation=configure_presentation;api.present=present;
        std::memcpy(output,&api,sizeof(api));return S_OK;
    }
    if(version==A2FO_N11_API_VERSION_ALL_MODEL_PARTS){
        if(!output||size!=sizeof(A2FO_N11_API9))return E_INVALIDARG;
        A2FO_N11_API9 api{};
        if(A2FO_Native11_GetInterface(A2FO_N11_API_VERSION_PRESENTATION,sizeof(api.base),&api.base)<0)return E_NOINTERFACE;
        auto& header=api.base.base.base.base.base.base.base.base;
        header.struct_size=size;header.version=version;header.capabilities|=A2FO_N11_CAP_ALL_MODEL_PARTS;
        api.submit_world_all_parts=submit_world_all_parts;
        std::memcpy(output,&api,sizeof(api));return S_OK;
    }
    if(version==A2FO_N11_API_VERSION_NATIVE_GEOMETRY){
        if(!output||size!=sizeof(A2FO_N11_API10))return E_INVALIDARG;
        A2FO_N11_API10 api{};
        if(A2FO_Native11_GetInterface(A2FO_N11_API_VERSION_ALL_MODEL_PARTS,sizeof(api.base),&api.base)<0)return E_NOINTERFACE;
        auto& header=api.base.base.base.base.base.base.base.base.base;
        header.struct_size=size;header.version=version;header.capabilities|=A2FO_N11_CAP_NATIVE_GEOMETRY;
        api.configure_geometry_policies=configure_geometry_policies;
        api.submit_world_native_geometry=submit_world_native_geometry;
        std::memcpy(output,&api,sizeof(api));return S_OK;
    }
    if(version==A2FO_N11_API_VERSION_LIVE_MATERIALS){
        if(!output||size!=sizeof(A2FO_N11_API11))return E_INVALIDARG;
        A2FO_N11_API11 api{};
        if(A2FO_Native11_GetInterface(A2FO_N11_API_VERSION_NATIVE_GEOMETRY,sizeof(api.base),&api.base)<0)return E_NOINTERFACE;
        auto& header=api.base.base.base.base.base.base.base.base.base.base;
        header.struct_size=size;header.version=version;header.capabilities|=A2FO_N11_CAP_LIVE_MATERIALS;
        api.configure_live_materials=configure_live_materials;api.prepare_live_materials=prepare_live_materials;
        api.submit_live_material_preview=submit_live_material_preview;
        std::memcpy(output,&api,sizeof(api));return S_OK;
    }
    return E_NOINTERFACE;
}
