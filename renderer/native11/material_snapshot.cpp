#include "material_snapshot.hpp"
#include "native_arithmetic.hpp"
#include "model_geometry_policy.hpp"
#include <cstring>
#include <limits>
#include <set>
#include <utility>
#include <algorithm>
#include <cmath>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_single_bytes = 64ull * 1024 * 1024;
constexpr std::size_t max_total_bytes = 128ull * 1024 * 1024;
bool printable(const std::string& name, bool nonempty) noexcept {
    if ((nonempty && name.empty()) || name.size() > 1024) return false;
    for (unsigned char c : name) if (c < 0x20 || c > 0x7e) return false;
    return true;
}
void fail(std::string& error, const char* message) noexcept {
    try { error = message; } catch (...) { error.clear(); }
}
}

bool BoundMaterialCapture::same_resources(const BoundMaterialCapture& other) const noexcept {
    try {
        if(frame_.object_count!=other.frame_.object_count ||
           frame_.requires_part_states!=other.frame_.requires_part_states || textures_!=other.textures_)return false;
        std::map<std::uint32_t,std::size_t> indices;
        for(std::size_t i=0;i<other.frame_.object_count;++i)indices.emplace(other.frame_.objects[i].instance,i);
        for(std::size_t i=0;i<frame_.object_count;++i){
            const auto& object=frame_.objects[i];const auto found=indices.find(object.instance);
            if(found==indices.end())return false;
            const auto& next=other.frame_.objects[found->second];
            if(object.entity!=next.entity || object.database!=next.database || object.model_serial!=next.model_serial)return false;
            const auto& a=views_.at(object.model_name);const auto& z=other.views_.at(next.model_name);
            if(base_->model_asset(a.model_id)!=other.base_->model_asset(z.model_id)||a.parts.size()!=z.parts.size())return false;
            for(std::size_t j=0;j<a.parts.size();++j)
                if(a.parts[j].authored!=z.parts[j].authored || a.parts[j].texture_id!=z.parts[j].texture_id ||
                   a.parts[j].node_index!=z.parts[j].node_index || a.parts[j].group_index!=z.parts[j].group_index ||
                   a.parts[j].state_valid!=z.parts[j].state_valid)return false;
        }
        return true;
    }catch(...){return false;}
}

bool BoundMaterialCapture::same_instance_contents(std::uint32_t instance,const BoundMaterialCapture& other) const noexcept {
    try {
        const Object* a=nullptr;const Object* b=nullptr;
        for(std::size_t i=0;i<frame_.object_count;++i)if(frame_.objects[i].instance==instance)a=&frame_.objects[i];
        for(std::size_t i=0;i<other.frame_.object_count;++i)if(other.frame_.objects[i].instance==instance)b=&other.frame_.objects[i];
        if(!a||!b||a->entity!=b->entity||a->database!=b->database||a->model_serial!=b->model_serial||
           a->placement_identity!=b->placement_identity||std::memcmp(a->world,b->world,sizeof(a->world)))return false;
        const auto& x=views_.at(a->model_name);const auto& y=other.views_.at(b->model_name);
        if(base_->model_asset(x.model_id)!=other.base_->model_asset(y.model_id)||x.parts.size()!=y.parts.size()||
           x.native_x87_control!=y.native_x87_control)return false;
        for(std::size_t i=0;i<x.parts.size();++i){const auto& p=x.parts[i];const auto& q=y.parts[i];
            if(p.authored!=q.authored||p.texture_id!=q.texture_id||p.node_index!=q.node_index||
               p.group_index!=q.group_index||p.state_valid!=q.state_valid)return false;
            if(!p.texture_id.empty()&&textures_.at(p.texture_id)!=other.textures_.at(q.texture_id))return false;
            if(p.state_valid&&(p.state.visible!=q.state.visible||std::memcmp(p.state.local_to_model,q.state.local_to_model,sizeof(p.state.local_to_model))))return false;
        }
        return true;
    }catch(...){return false;}
}

bool BoundMaterialCapture::resources_current(ResolveSnapshotModel model,void* model_context,
    ResolveTextureAsset texture,void* texture_context) const noexcept {
    if(!model||!texture||!base_)return false;
    try {
        for(std::size_t i=0;i<frame_.object_count;++i){
            const auto& object=frame_.objects[i];
            const auto& view=views_.at(object.model_name);
            const auto owned=base_->model_asset(view.model_id);
            if(!owned||model(model_context,object.database,object.model_serial)!=owned)return false;
        }
        for(const auto& entry:textures_){
            if(!entry.second||texture(texture_context,entry.second->texture)!=entry.second)return false;
        }
        return true;
    }catch(...){return false;}
}

std::uint32_t BoundMaterialCapture::native_x87_control(std::uint32_t instance) const noexcept {
    try {
        for(std::size_t i=0;i<frame_.object_count;++i)if(frame_.objects[i].instance==instance)
            return views_.at(frame_.objects[i].model_name).native_x87_control;
    }catch(...){}
    return 0;
}

std::uint32_t BoundMaterialCapture::common_native_x87_control() const noexcept {
    if(!frame_.object_count)return 0;
    const auto first=native_x87_control(frame_.objects[0].instance);
    if(!first)return 0;
    for(std::size_t i=1;i<frame_.object_count;++i)
        if(native_x87_control(frame_.objects[i].instance)!=first)return 0;
    return first;
}

bool BoundMaterialCapture::export_instance(std::uint32_t instance,Object& output,
    std::shared_ptr<const MaterialObservation>& observation,std::string& error) const noexcept {
    try {
        const Object* object=nullptr;
        for(std::size_t i=0;i<frame_.object_count;++i)if(frame_.objects[i].instance==instance)object=&frame_.objects[i];
        if(!object){fail(error,"missing owned instance");return false;}
        const auto& view=views_.at(object->model_name);auto model=base_->model_asset(view.model_id);
        if(!model){fail(error,"missing owned instance model");return false;}
        auto pending=std::make_shared<MaterialObservation>();pending->instance=object->instance;pending->database=object->database;
        pending->model_serial=object->model_serial;pending->model=model;
        pending->native_x87_control=view.native_x87_control;
        for(const auto& part:view.parts){
            if(!part.state_valid){fail(error,"owned instance lacks observed node states");return false;}
            MaterialBinding binding{};binding.authored_texture=part.authored;binding.node_state_valid=true;binding.node_state=part.state;
            binding.node_index=part.node_index;binding.group_index=part.group_index;
            if(!part.texture_id.empty())binding.texture=textures_.at(part.texture_id);
            pending->parts.push_back(std::move(binding));
        }
        Object raw=*object;std::memcpy(raw.model_name,model->info.name,sizeof(raw.model_name));
        output=raw;observation=std::move(pending);error.clear();return true;
    }catch(...){fail(error,"owned instance export failed");return false;}
}

std::int32_t A2FO_N11_CALL BoundMaterialCapture::read_asset(void* context, std::uint32_t kind,
    const std::uint8_t* identifier, std::uint32_t length, std::uint32_t max_bytes,
    A2FO_N11_BytesSink sink, void* sink_context) noexcept {
    if (!context || !identifier || !length || !sink ||
        (kind != 1 && kind != 2) || length > (kind == 1 ? 259u : 1024u)) return 0;
    try {
        const auto& self = *static_cast<const BoundMaterialCapture*>(context);
        const std::string id(reinterpret_cast<const char*>(identifier), length);
        const std::vector<std::uint8_t>* bytes = nullptr;
        if (kind == 1) {
            const auto view = self.views_.find(id);
            if (view == self.views_.end()) return 0;
            const auto model = self.base_->model_asset(view->second.model_id);
            if (model) bytes = &model->bytes;
        } else {
            const auto texture = self.textures_.find(id);
            if (texture != self.textures_.end()) bytes = texture->second->bytes.get();
        }
        if (!bytes || bytes->empty() || bytes->size() > max_bytes ||
            bytes->size() > std::numeric_limits<std::uint32_t>::max()) return 0;
        return sink(sink_context, bytes->data(), static_cast<std::uint32_t>(bytes->size())) == 1 ? 1 : 0;
    } catch (...) { return 0; }
}

std::int32_t A2FO_N11_CALL BoundMaterialCapture::resolve_texture(void* context,
    const std::uint8_t* model_identifier, std::uint32_t model_identifier_length,
    std::uint32_t part_index, const std::uint8_t* authored_texture,
    std::uint32_t authored_length, std::uint32_t max_identifier_bytes,
    A2FO_N11_BytesSink sink, void* sink_context) noexcept {
    if (!context || !model_identifier || !model_identifier_length || model_identifier_length > 259 ||
        !authored_texture || !authored_length || authored_length > 1024 || !sink) return 0;
    try {
        const auto& self = *static_cast<const BoundMaterialCapture*>(context);
        const std::string view_id(reinterpret_cast<const char*>(model_identifier), model_identifier_length);
        const auto view = self.views_.find(view_id);
        if (view == self.views_.end() || part_index >= view->second.parts.size()) return 0;
        const auto& part = view->second.parts[part_index];
        if (part.texture_id.empty() || part.authored.size() != authored_length ||
            std::memcmp(part.authored.data(), authored_texture, authored_length) != 0 ||
            part.texture_id.size() > max_identifier_bytes) return 0;
        return sink(sink_context, reinterpret_cast<const std::uint8_t*>(part.texture_id.data()),
            static_cast<std::uint32_t>(part.texture_id.size())) == 1 ? 1 : 0;
    } catch (...) { return 0; }
}

std::int32_t A2FO_N11_CALL BoundMaterialCapture::resolve_part_state(void* context,
    const std::uint8_t* model,std::uint32_t length,std::uint32_t index,const float*,
    A2FO_N11_PartStateSink sink,void* sink_context) noexcept {
    if(!context||!model||!length||length>259||!sink)return 0;
    try {
        const auto& self=*static_cast<const BoundMaterialCapture*>(context);
        const auto found=self.views_.find(std::string(reinterpret_cast<const char*>(model),length));
        if(found==self.views_.end()||index>=found->second.parts.size())return 0;
        const auto& part=found->second.parts[index];
        if(!part.state_valid)return 0;
        A2FO_N11_PartState state{};state.struct_size=sizeof(state);state.visible=part.state.visible?1:0;
        std::copy(part.state.local_to_model,part.state.local_to_model+16,state.local_to_model);
        return sink(sink_context,&state)==1?1:0;
    }catch(...){return 0;}
}
std::int32_t A2FO_N11_CALL BoundMaterialCapture::resolve_geometry_policy(void* context,
    const std::uint8_t* model,std::uint32_t length,A2FO_N11_GeometryPolicySink sink,
    void* sink_context) noexcept {
    if(!context||!model||!length||length>259||!sink)return 0;
    try {
        const auto& self=*static_cast<const BoundMaterialCapture*>(context);
        const auto found=self.views_.find(std::string(reinterpret_cast<const char*>(model),length));
        if(found==self.views_.end()||!self.base_)return 0;
        const auto asset=self.base_->model_asset(found->second.model_id);
        if(!asset)return 0;
        const ModelGeometryPolicy source{asset->info.parser_scalar_bits,
            asset->info.parser_native_x87_control};
        if(!valid_model_geometry_policy(source))return 0;
        const A2FO_N11_GeometryPolicy result{sizeof(A2FO_N11_GeometryPolicy),0,
            source.parser_scalar_bits,source.native_x87_control};
        return sink(sink_context,&result)==1?1:0;
    }catch(...){return 0;}
}
std::int32_t A2FO_N11_CALL BoundMaterialCapture::resolve_live_material(void* context,
    const std::uint8_t* model,std::uint32_t length,const A2FO_N11_MaterialSeed* seed,
    A2FO_N11_LiveMaterialSink sink,void* sink_context) noexcept {
    if(!context||!model||!length||length>259||!seed||!sink||
       seed->struct_size!=sizeof(A2FO_N11_MaterialSeed)||
       seed->node_index==UINT32_MAX||seed->group_index==UINT32_MAX||
       seed->authored_texture_length>1024||
       (seed->authored_texture_length&&!seed->authored_texture)||
       (seed->flags&~15u)||((seed->flags&4u)&&(seed->flags&8u)))return 0;
    for(float c:seed->colour)if(!std::isfinite(c)||c<0||c>1)return 0;
    for(float c:seed->emissive)if(!std::isfinite(c)||c<0)return 0;
    try {
        const auto& self=*static_cast<const BoundMaterialCapture*>(context);
        const auto found=self.views_.find(std::string(reinterpret_cast<const char*>(model),length));
        if(found==self.views_.end()||seed->part_index>=found->second.parts.size())return 0;
        const auto& part=found->second.parts[seed->part_index];
        if(part.node_index!=seed->node_index||part.group_index!=seed->group_index||
           part.authored.size()!=seed->authored_texture_length||
           (seed->authored_texture_length&&std::memcmp(part.authored.data(),seed->authored_texture,
               seed->authored_texture_length)))return 0;
        A2FO_N11_LiveMaterial material{};
        material.struct_size=sizeof(material);material.flags=seed->flags;
        material.sampler=0;material.depth_mode=0;material.alpha_cutoff=.5f;
        std::copy(seed->colour,seed->colour+4,material.colour);
        std::copy(seed->emissive,seed->emissive+3,material.emissive);
        material.uv_transform[0]=1;material.uv_transform[4]=1;
        material.texture_identifier=reinterpret_cast<const std::uint8_t*>(part.texture_id.data());
        material.texture_identifier_length=static_cast<std::uint32_t>(part.texture_id.size());
        return sink(sink_context,&material)==1?1:0;
    }catch(...){return 0;}
}
bool build_bound_material_capture(const Frame& input, std::uint64_t source_epoch,
    ResolveSnapshotModel resolve, void* resolve_context,
    const std::map<std::uint32_t, std::shared_ptr<const MaterialObservation>>& observations,
    std::shared_ptr<const BoundMaterialCapture>& output, std::string& error) noexcept {
    try {
        if (!source_epoch || !resolve || input.object_count > capacity) {
            fail(error, "invalid material snapshot bounds or source"); return false;
        }
        std::shared_ptr<const BoundCapture> base;
        if (!build_bound_capture(input, source_epoch, resolve, resolve_context, {}, base, error)) return false;
        auto pending = std::shared_ptr<BoundMaterialCapture>(new BoundMaterialCapture);
        pending->base_ = std::move(base);
        pending->frame_ = pending->base_->frame();
        std::size_t total_bytes = 0, total_parts = 0;
        std::set<const ModelAsset*> models;
        for (const auto& entry : pending->base_->models()) {
            if (models.insert(entry.second.get()).second) {
                if (entry.second->bytes.size() > max_total_bytes - total_bytes) {
                    fail(error, "material snapshot byte budget exceeded"); return false;
                }
                total_bytes += entry.second->bytes.size();
            }
        }
        std::set<std::uint32_t> instances;
        std::map<std::uint64_t, std::shared_ptr<const TextureAsset>> texture_serials;
        for (std::size_t i = 0; i < input.object_count; ++i) {
            const auto& object = input.objects[i];
            if (!object.instance || !instances.insert(object.instance).second) {
                fail(error, "invalid or duplicate material instance"); return false;
            }
            const auto observation = observations.find(object.instance);
            const std::string model_id(pending->frame_.objects[i].model_name);
            const auto model = pending->base_->model_asset(model_id);
            if (observation == observations.end() || !observation->second ||
                observation->second->instance != object.instance ||
                observation->second->database != object.database ||
                observation->second->model_serial != object.model_serial ||
                observation->second->model != model ||
                !valid_native_x87_encoding(observation->second->native_x87_control)) {
                fail(error, "missing or stale material observation"); return false;
            }
            const auto& observed = *observation->second;
            if (observed.parts.empty() || observed.parts.size() > 8192 - total_parts) {
                fail(error, "material part budget exceeded"); return false;
            }
            total_parts += observed.parts.size();
            BoundMaterialCapture::View view; view.model_id = model_id;
            view.native_x87_control=observed.native_x87_control;
            view.parts.reserve(observed.parts.size());
            std::set<std::pair<std::uint32_t,std::uint32_t>> identities;
            for (const auto& binding : observed.parts) {
                const bool qualified=binding.node_index!=UINT32_MAX&&binding.group_index!=UINT32_MAX;
                if (!printable(binding.authored_texture, false) ||
                    (binding.node_state_valid?!valid_part_state(binding.node_state):bool(binding.node_flags&3u)) ||
                    ((binding.node_index==UINT32_MAX)!=(binding.group_index==UINT32_MAX)) ||
                    (qualified&&!identities.insert({binding.node_index,binding.group_index}).second) ||
                    (!qualified&&static_cast<bool>(binding.texture) != !binding.authored_texture.empty())) {
                    fail(error, "invalid material binding or hidden node"); return false;
                }
                BoundMaterialCapture::Part part; part.authored = binding.authored_texture;
                part.node_index=binding.node_index;part.group_index=binding.group_index;
                part.state_valid=binding.node_state_valid;part.state=binding.node_state;
                if(part.state_valid)pending->frame_.requires_part_states=true;
                if (binding.texture) {
                    const auto& texture = binding.texture;
                    if (!texture->serial || !texture->texture || texture->flags || texture->colour_key ||
                        !printable(texture->logical_name, true) || !texture->bytes ||
                        texture->bytes->empty() || texture->bytes->size() > max_single_bytes) {
                        fail(error, "invalid material texture asset"); return false;
                    }
                    const auto serial = texture_serials.emplace(texture->serial, texture);
                    if (!serial.second && serial.first->second != texture) {
                        fail(error, "texture serial owner collision"); return false;
                    }
                    part.texture_id = "n11:texture:" + std::to_string(texture->serial);
                    if (serial.second) {
                        if (texture_serials.size() > 256 || texture->bytes->size() > max_total_bytes - total_bytes) {
                            fail(error, "material texture budget exceeded"); return false;
                        }
                        total_bytes += texture->bytes->size();
                        pending->textures_.emplace(part.texture_id, texture);
                    }
                }
                view.parts.push_back(std::move(part));
            }
            const std::string view_id = "n11:view:" + std::to_string(source_epoch) + ":" + std::to_string(object.instance);
            if (view_id.size() >= sizeof(pending->frame_.objects[i].model_name)) {
                fail(error, "material view identifier too long"); return false;
            }
            pending->views_.emplace(view_id, std::move(view));
            auto& destination = pending->frame_.objects[i];
            std::memset(destination.model_name, 0, sizeof(destination.model_name));
            std::memcpy(destination.model_name, view_id.data(), view_id.size());
        }
        if(pending->frame_.requires_part_states)for(const auto& view:pending->views_)
            for(const auto& part:view.second.parts)if(!part.state_valid){fail(error,"incomplete node states");return false;}
        output = std::move(pending);
        error.clear(); return true;
    } catch (...) { fail(error, "material snapshot allocation or resolver failure"); return false; }
}
} // namespace a2fo::native11
