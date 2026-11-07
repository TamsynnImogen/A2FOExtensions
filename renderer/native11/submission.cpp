#include "submission.hpp"
#include <wincrypt.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

namespace a2fo::native11 {
namespace {
using Digest=std::array<unsigned char,32>;
bool geometry_signature(const ContentPart& part,Digest& output) noexcept {
    static_assert(sizeof(Vertex)==8*sizeof(float),"canonical packed vertex");
    if(part.vertices.empty()||part.indices.empty()||part.vertices.size()>MAXDWORD/sizeof(Vertex)||
       part.indices.size()>MAXDWORD/sizeof(std::uint32_t))return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool success=false;
    if(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
        const std::uint32_t counts[]{static_cast<std::uint32_t>(part.vertices.size()),static_cast<std::uint32_t>(part.indices.size())};
        success=CryptHashData(hash,reinterpret_cast<const BYTE*>(counts),sizeof(counts),0)&&
            CryptHashData(hash,reinterpret_cast<const BYTE*>(part.vertices.data()),static_cast<DWORD>(part.vertices.size()*sizeof(Vertex)),0)&&
            CryptHashData(hash,reinterpret_cast<const BYTE*>(part.indices.data()),static_cast<DWORD>(part.indices.size()*sizeof(std::uint32_t)),0);
        DWORD size=output.size();if(success)success=CryptGetHashParam(hash,HP_HASHVAL,output.data(),&size,0)&&size==output.size();
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    return success;
}
bool signature(const ContentTexture& texture,Digest& output) noexcept {
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool success=false;
    if(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&
       CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)) {
        const std::uint32_t values[]{static_cast<std::uint32_t>(texture.format),static_cast<std::uint32_t>(texture.alpha),
            texture.width,texture.height,static_cast<std::uint32_t>(texture.mips.size())};
        success=CryptHashData(hash,reinterpret_cast<const BYTE*>(values),sizeof(values),0)!=0;
        for(const auto& mip:texture.mips){
            const std::uint32_t layout[]{mip.width,mip.height,mip.row_pitch,mip.slice_pitch};
            const std::uint64_t extent[]{mip.offset,mip.byte_count};
            success=success&&CryptHashData(hash,reinterpret_cast<const BYTE*>(layout),sizeof(layout),0)&&
                CryptHashData(hash,reinterpret_cast<const BYTE*>(extent),sizeof(extent),0);
        }
        if(texture.bytes.size()>MAXDWORD)success=false;
        if(success)success=CryptHashData(hash,texture.bytes.data(),static_cast<DWORD>(texture.bytes.size()),0)!=0;
        DWORD size=output.size();
        if(success)success=CryptGetHashParam(hash,HP_HASHVAL,output.data(),&size,0)&&size==output.size();
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    return success;
}
bool compose(const float* object,const float* part,float* output) noexcept {
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col){
        double value=0;for(unsigned k=0;k<4;++k)value+=double(object[row*4+k])*part[k*4+col];
        output[row*4+col]=static_cast<float>(value);
        if(!std::isfinite(output[row*4+col]))return false;
    }
    return true;
}
void message(std::string& output,const char* text) noexcept {try{output=text;}catch(...){output.clear();}}
bool stable_upload_failure(HRESULT hr) noexcept {
    return hr==E_INVALIDARG||hr==E_NOTIMPL||hr==DXGI_ERROR_UNSUPPORTED||hr==DXGI_ERROR_INVALID_CALL;
}
}
struct SceneSubmission::Impl {
    struct Texture {std::uint32_t handle;Digest signature;std::size_t bytes=0;};
    struct Part {Draw draw;float local_to_model[16];MaterialSeed seed;};
    struct ResolvedPart {LiveMaterial material;std::uint32_t texture=0;};
    using LiveFrame=std::map<std::string,std::vector<ResolvedPart>>;
    struct Failure {HRESULT hr;std::string error;};
    Renderer& renderer;AssetReader reader;const StaticMaterialPolicy policy;
    std::map<std::string,Texture> textures;
    std::map<std::string,std::vector<Part>> models;
    std::map<std::string,Failure> failures;
    std::map<Digest,std::uint32_t> geometry;
    std::size_t cached_parts=0;
    std::uint64_t source_epoch=0,device_epoch=0;bool bound=false,rendering=false;
    explicit Impl(Renderer& value,AssetReader input,StaticMaterialPolicy material_policy):
        renderer(value),reader(input),policy(material_policy){}
    void clear() noexcept {
        ResourceUsage usage{};
        if(SUCCEEDED(renderer.resource_usage(usage))&&usage.epoch==device_epoch){
            if(policy.share_geometry){for(const auto& entry:geometry)renderer.release_mesh(entry.second);}
            else for(const auto& model:models)for(const auto& part:model.second)renderer.release_mesh(part.draw.mesh);
            for(const auto& texture:textures)renderer.release_texture(texture.second.handle);
        }
        models.clear();textures.clear();failures.clear();geometry.clear();cached_parts=0;bound=false;
    }
    HRESULT bind(std::uint64_t epoch){
        ResourceUsage usage{};const HRESULT hr=renderer.resource_usage(usage);if(FAILED(hr))return hr;
        if(!bound||source_epoch!=epoch||device_epoch!=usage.epoch){
            clear();source_epoch=epoch;device_epoch=usage.epoch;bound=true;
        }
        return S_OK;
    }
    HRESULT load(const std::string& name,std::string& error){
        if(policy.native_geometry&&(!policy.all_model_parts||!reader.resolve_geometry_policy)){
            error="native geometry source policy unavailable";return E_INVALIDARG;
        }
        if(models.find(name)!=models.end())return S_OK;
        const auto failed=failures.find(name);if(failed!=failures.end()){error=failed->second.error;return failed->second.hr;}
        if(models.size()+failures.size()>=2048){error="native model cache capacity reached";return E_OUTOFMEMORY;}
        ModelContent content;
        if(!load_model_content(reader,name,content,error,policy.all_model_parts)){
            if(!reader.resolve_live_material)failures.emplace(name,Failure{E_FAIL,error});
            return E_FAIL;
        }
        if((policy.share_geometry||reader.resolve_live_material)&&content.parts.size()>8192-cached_parts){
            error="geometry cache part budget exceeded";return E_OUTOFMEMORY;
        }
        struct Pending {
            Renderer& renderer;std::vector<std::uint32_t> meshes,images;bool committed=false;
            ~Pending(){if(!committed){for(auto id:meshes)renderer.release_mesh(id);for(auto id:images)renderer.release_texture(id);}}
        } pending{renderer,{},{},false};
        // Reserve before creating a GPU object so an allocation failure cannot
        // lose an uploaded handle before it joins the rollback owner.
        pending.meshes.reserve(content.parts.size());pending.images.reserve(content.textures.size());
        std::vector<std::uint32_t> handles;handles.reserve(content.textures.size());
        std::map<std::string,Texture> additions;
        std::map<Digest,std::uint32_t> mesh_additions;
        std::size_t dynamic_bytes=0;
        if(reader.resolve_live_material)for(const auto& entry:textures){
            if(entry.second.bytes>128u*1024u*1024u-dynamic_bytes){
                error="live texture byte budget exceeded";return E_OUTOFMEMORY;
            }
            dynamic_bytes+=entry.second.bytes;
        }
        for(const auto& texture:content.textures){
            Digest digest{};
            if(!signature(texture,digest)){error="texture content signature failed";return E_FAIL;}
            const auto existing=textures.find(texture.identifier);
            if(existing!=textures.end()){
                if(existing->second.signature!=digest){error="asset content changed inside source epoch";return E_INVALIDARG;}
                handles.push_back(existing->second.handle);continue;
            }
            if(reader.resolve_live_material){
                if(textures.size()+additions.size()>=256||
                   texture.bytes.size()>128u*1024u*1024u-dynamic_bytes){
                    error="live texture budget exceeded";return E_OUTOFMEMORY;
                }
                dynamic_bytes+=texture.bytes.size();
            }
            std::uint32_t handle=0;
            TextureView view{texture.format,texture.alpha,texture.width,texture.height,texture.mips.data(),
                static_cast<std::uint32_t>(texture.mips.size()),texture.bytes.data(),texture.bytes.size()};
            const HRESULT hr=renderer.upload_texture(view,handle);
            if(FAILED(hr)){error="native texture upload failed";
                if(!reader.resolve_live_material&&stable_upload_failure(hr))failures.emplace(name,Failure{hr,error});
                return hr;
            }
            pending.images.push_back(handle);handles.push_back(handle);
            additions.emplace(texture.identifier,Texture{handle,digest,texture.bytes.size()});
        }
        std::vector<Part> parts;parts.reserve(content.parts.size());
        for(const auto& input:content.parts){
            Part part{};Digest key{};
            if(policy.share_geometry){
                if(!geometry_signature(input,key)){error="mesh content signature failed";return E_FAIL;}
                const auto existing=geometry.find(key),added=mesh_additions.find(key);
                if(existing!=geometry.end())part.draw.mesh=existing->second;
                else if(added!=mesh_additions.end())part.draw.mesh=added->second;
            }
            if(!part.draw.mesh){
                const HRESULT hr=renderer.upload_mesh({input.vertices.data(),static_cast<std::uint32_t>(input.vertices.size()),
                    input.indices.data(),static_cast<std::uint32_t>(input.indices.size())},part.draw.mesh);
                if(FAILED(hr)){error="native mesh upload failed";
                    if(!reader.resolve_live_material&&stable_upload_failure(hr))failures.emplace(name,Failure{hr,error});
                    return hr;
                }
                pending.meshes.push_back(part.draw.mesh);
                if(policy.share_geometry)mesh_additions.emplace(key,part.draw.mesh);
            }
            part.draw.texture=input.texture_index==no_texture?0:handles.at(input.texture_index);
            part.seed=input.seed;
            std::copy(input.local_to_model,input.local_to_model+16,part.local_to_model);
            std::copy(input.colour,input.colour+4,part.draw.colour);
            std::copy(input.emissive,input.emissive+3,part.draw.emissive);part.draw.flags=input.flags;
            if(reader.resolve_live_material){
                part.draw.sampler=input.live_material.sampler;
                part.draw.depth_mode=input.live_material.depth_mode;
                part.draw.alpha_cutoff=input.live_material.alpha_cutoff;
                std::copy(input.live_material.uv_transform,input.live_material.uv_transform+6,
                    part.draw.uv_transform);
            }
            // Art-directed legacy relighting, not recovered engine behavior.
            // Keep glow and blended parts on their original material path.
            if(policy.relight_fullbright && !(input.flags&(alpha_blend|additive)) &&
               input.colour[0]==0.f && input.colour[1]==0.f && input.colour[2]==0.f &&
               (input.emissive[0]>0.f || input.emissive[1]>0.f || input.emissive[2]>0.f)) {
                for(unsigned channel=0;channel<3;++channel) {
                    part.draw.colour[channel]=input.emissive[channel]*(1.f-policy.retained_emission);
                    part.draw.emissive[channel]=input.emissive[channel]*policy.retained_emission;
                }
            }
            parts.push_back(part);
        }
        // map::merge transfers preallocated nodes without allocating. Models
        // become visible only after every GPU resource has succeeded.
        const auto count=parts.size();models.emplace(name,std::move(parts));textures.merge(additions);
        geometry.merge(mesh_additions);cached_parts+=count;pending.committed=true;
        return S_OK;
    }
    HRESULT resolve_frame_materials(const Frame& captured,LiveFrame& result,std::string& error){
        if(!reader.resolve_live_material)return S_OK;
        struct Pending {
            Renderer& renderer;std::vector<std::uint32_t> handles;bool committed=false;
            ~Pending(){if(!committed)for(auto id:handles)renderer.release_texture(id);}
        } pending{renderer,{ },false};
        pending.handles.reserve(256);
        std::map<std::string,Texture> additions;
        std::size_t total_bytes=0;
        for(const auto& entry:textures){
            if(entry.second.bytes>128u*1024u*1024u-total_bytes){error="live texture budget exceeded";return E_OUTOFMEMORY;}
            total_bytes+=entry.second.bytes;
        }
        for(std::size_t i=0;i<captured.object_count;++i){
            const std::string name=captured.objects[i].model_name;
            if(result.find(name)!=result.end())continue;
            const auto& parts=models.at(name);
            auto& resolved=result[name];resolved.reserve(parts.size());
            for(const auto& part:parts){
                LiveMaterial material;error.clear();
                if(!reader.resolve_live_material(reader.context,name,part.seed,material,error)||
                   !valid_live_material(material)){
                    if(error.empty())error="live material resolution failed";
                    return E_INVALIDARG;
                }
                std::uint32_t handle=0;
                if(!material.texture_identifier.empty()){
                    const auto old=textures.find(material.texture_identifier);
                    if(old!=textures.end())handle=old->second.handle;
                    else {
                        const auto added=additions.find(material.texture_identifier);
                        if(added!=additions.end())handle=added->second.handle;
                        else {
                            if(textures.size()+additions.size()>=256){error="live texture count exceeded";return E_OUTOFMEMORY;}
                            ContentTexture texture;
                            if(!load_content_texture(reader,material.texture_identifier,texture,error))return E_FAIL;
                            if(texture.bytes.size()>128u*1024u*1024u-total_bytes){
                                error="live texture byte budget exceeded";return E_OUTOFMEMORY;
                            }
                            Digest digest{};
                            if(!signature(texture,digest)){error="live texture signature failed";return E_FAIL;}
                            TextureView view{texture.format,texture.alpha,texture.width,texture.height,
                                texture.mips.data(),static_cast<std::uint32_t>(texture.mips.size()),
                                texture.bytes.data(),texture.bytes.size()};
                            const auto hr=renderer.upload_texture(view,handle);
                            if(FAILED(hr)){error="live texture upload failed";return hr;}
                            pending.handles.push_back(handle);
                            additions.emplace(material.texture_identifier,
                                Texture{handle,digest,texture.bytes.size()});
                            total_bytes+=texture.bytes.size();
                        }
                    }
                }
                resolved.push_back({std::move(material),handle});
            }
        }
        textures.merge(additions);pending.committed=true;return S_OK;
    }
};
SceneSubmission::SceneSubmission(Renderer& renderer,AssetReader reader,StaticMaterialPolicy policy):
    p_(std::make_unique<Impl>(renderer,reader,policy)){}
SceneSubmission::~SceneSubmission(){p_->clear();}
void SceneSubmission::clear() noexcept {if(!p_->rendering)p_->clear();}
HRESULT SceneSubmission::prepare_live_materials(const Frame& captured,
    std::uint64_t source_epoch,std::string& error) noexcept {
    if(p_->rendering){message(error,"reentrant live material preparation refused");return E_UNEXPECTED;}
    struct Owner {bool& flag;explicit Owner(bool& value):flag(value){flag=true;}~Owner(){flag=false;}} owner(p_->rendering);
    try {
        if(!p_->reader.resolve_live_material||!source_epoch||!captured.generation||
           captured.object_count>capacity||!captured.camera){
            message(error,"invalid live material preparation frame");return E_INVALIDARG;
        }
        for(std::size_t i=0;i<captured.object_count;++i){const auto& object=captured.objects[i];
            if(object.model_identity!=ModelIdentity::plain_database_name||!object.model_name[0]||
               !std::memchr(object.model_name,0,sizeof(object.model_name))){
                message(error,"unresolved live material model");return E_INVALIDARG;
            }
        }
        const auto available=p_->renderer.render_status();if(FAILED(available))return available;
        const auto bound=p_->bind(source_epoch);if(FAILED(bound))return bound;
        for(std::size_t i=0;i<captured.object_count;++i){
            const auto loaded=p_->load(captured.objects[i].model_name,error);
            if(FAILED(loaded))return loaded;
        }
        Impl::LiveFrame resolved;
        const auto hr=p_->resolve_frame_materials(captured,resolved,error);
        if(SUCCEEDED(hr))error.clear();
        return hr;
    }catch(const std::bad_alloc&){message(error,"live material preparation allocation failed");return E_OUTOFMEMORY;}
     catch(...){message(error,"live material preparation failed");return E_FAIL;}
}
HRESULT SceneSubmission::render_preview(const Frame& captured,std::uint64_t source_epoch,
    const Light* lights,std::uint32_t light_count,const float ambient[3],std::string& error,
    const ShadowSettings& shadows) noexcept {
    if(p_->rendering){message(error,"reentrant scene submission refused");return E_UNEXPECTED;}
    struct Owner {bool& flag;explicit Owner(bool& value):flag(value){flag=true;}~Owner(){flag=false;}} owner(p_->rendering);
    try{
        if(!std::isfinite(p_->policy.retained_emission) || p_->policy.retained_emission<0.f ||
           p_->policy.retained_emission>1.f) {
            message(error,"invalid static material policy");return E_INVALIDARG;
        }
        if(!captured.camera||!captured.generation||captured.object_count>capacity||!ambient||light_count>8||(light_count&&!lights)){
            message(error,"invalid owned scene snapshot");return E_INVALIDARG;
        }
        if(captured.requires_part_states&&!p_->reader.resolve_part_state){
            message(error,"capture requires a part-state resolver");return E_INVALIDARG;
        }
        // Reject unresolved model identities before reading assets or clearing the
        // target; an incomplete capture must never become a partial native view.
        for(std::size_t i=0;i<captured.object_count;++i){const auto& object=captured.objects[i];
            if(object.shadow_only&&!captured.complete_casters){message(error,"shadow-only geometry requires complete world casters");return E_INVALIDARG;}
            if(object.model_identity!=ModelIdentity::plain_database_name||!object.model_name[0]||
               !std::memchr(object.model_name,0,sizeof(object.model_name))||
               (object.placement_identity!=PlacementIdentity::authored_placement&&
                object.placement_identity!=PlacementIdentity::craft_runtime_scale)){
                message(error,"scene contains unresolved model identity");return E_INVALIDARG;
            }
        }
        const HRESULT available=p_->renderer.render_status();
        if(FAILED(available)){message(error,"native renderer targets/device unavailable");return available;}
        const HRESULT bound=p_->bind(source_epoch);if(FAILED(bound)){message(error,"native renderer unavailable");return bound;}
        for(std::size_t i=0;i<captured.object_count;++i){
            const auto loaded=p_->load(captured.objects[i].model_name,error);
            if(FAILED(loaded))return loaded;
        }
        Impl::LiveFrame resolved;
        const auto materials=p_->resolve_frame_materials(captured,resolved,error);
        if(FAILED(materials))return materials;
        std::vector<Draw> draws;
        std::size_t emitted_parts=0;
        for(std::size_t i=0;i<captured.object_count;++i){const auto& object=captured.objects[i];
            const auto& parts=p_->models.at(object.model_name);
            if(parts.size()>8192-emitted_parts){message(error,"scene draw budget exceeded");return E_INVALIDARG;}
            emitted_parts+=parts.size();
            for(std::size_t part_index=0;part_index<parts.size();++part_index){const auto& part=parts[part_index];
                PartState state{};
                std::copy(part.local_to_model,part.local_to_model+16,state.local_to_model);
                if(p_->reader.resolve_part_state){
                    if(!p_->reader.resolve_part_state(p_->reader.context,object.model_name,
                       static_cast<std::uint32_t>(part_index),part.local_to_model,state,error))return E_INVALIDARG;
                    if(!valid_part_state(state)){message(error,"invalid resolved part state");return E_INVALIDARG;}
                }
                if(!state.visible)continue;
                Draw draw=part.draw;
                if(p_->reader.resolve_live_material){
                    const auto& selected=resolved.at(object.model_name).at(part_index);
                    draw.texture=selected.texture;
                    std::copy(selected.material.colour,selected.material.colour+4,draw.colour);
                    std::copy(selected.material.emissive,selected.material.emissive+3,draw.emissive);
                    draw.flags=selected.material.flags;
                    draw.sampler=selected.material.sampler;
                    draw.depth_mode=selected.material.depth_mode;
                    draw.alpha_cutoff=selected.material.alpha_cutoff;
                    std::copy(selected.material.uv_transform,selected.material.uv_transform+6,
                        draw.uv_transform);
                }
                draw.shadow_only=object.shadow_only;
                if(!compose(object.world,state.local_to_model,draw.local_to_world)){
                    message(error,"invalid composed model transform");return E_INVALIDARG;
                }
                draws.push_back(draw);
            }
        }
        FrameView frame{};
        frame.shadows=shadows;
        std::copy(captured.view.world_to_clip,captured.view.world_to_clip+16,frame.world_to_clip);
        std::copy(captured.view.viewport,captured.view.viewport+4,frame.viewport);
        std::copy(ambient,ambient+3,frame.ambient);frame.lights=lights;frame.light_count=light_count;
        frame.draws=draws.data();frame.draw_count=static_cast<std::uint32_t>(draws.size());
        const HRESULT rendered=p_->renderer.render(frame,false);
        if(FAILED(rendered)){message(error,"native scene render rejected");return rendered;}
        error.clear();return S_OK;
    }catch(const std::bad_alloc&){message(error,"native scene allocation failed");return E_OUTOFMEMORY;}
     catch(...){message(error,"native scene submission failed");return E_FAIL;}
}
} // namespace a2fo::native11
