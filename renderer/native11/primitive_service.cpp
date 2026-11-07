#include "primitive_service.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace a2fo::native11 {
namespace {
static_assert(sizeof(Vertex)==sizeof(A2FO_N11_Vertex),"wire vertex layout");
static_assert(offsetof(Vertex,position)==offsetof(A2FO_N11_Vertex,position),"wire position");
static_assert(offsetof(Vertex,normal)==offsetof(A2FO_N11_Vertex,normal),"wire normal");
static_assert(offsetof(Vertex,uv)==offsetof(A2FO_N11_Vertex,uv),"wire UV");
static_assert(std::is_standard_layout_v<Vertex> && std::is_trivially_copyable_v<Vertex>,"source vertex POD");
static_assert(std::is_standard_layout_v<A2FO_N11_Vertex> &&
              std::is_trivially_copyable_v<A2FO_N11_Vertex>,"wire vertex POD");
constexpr std::uint64_t max_payload=128ull*1024*1024;
void message(std::string& error,const char* value) noexcept {
    try { error=value; } catch (...) { error.clear(); }
}
bool add_bytes(std::uint64_t& total,std::uint64_t count,std::uint64_t item_size) noexcept {
    if(count>max_payload/item_size || count*item_size>max_payload-total)return false;
    total+=count*item_size;return true;
}
}

EncodedPrimitiveFrame::EncodedPrimitiveFrame(EncodedPrimitiveFrame&& other) noexcept {
    *this=std::move(other);
}
EncodedPrimitiveFrame& EncodedPrimitiveFrame::operator=(EncodedPrimitiveFrame&& other) noexcept {
    if(this!=&other){
        packet=std::move(other.packet);
        meshes=std::move(other.meshes);
        textures=std::move(other.textures);
        draws=std::move(other.draws);
        lights=std::move(other.lights);
        frame=other.frame;
        rebind();
        other.frame={};other.rebind();
    }
    return *this;
}
void EncodedPrimitiveFrame::rebind() noexcept {
    frame.meshes=meshes.empty()?nullptr:meshes.data();
    frame.textures=textures.empty()?nullptr:textures.data();
    frame.draws=draws.empty()?nullptr:draws.data();
    frame.lights=lights.empty()?nullptr:lights.data();
}

bool encode_primitive_frame(std::shared_ptr<const PrimitiveFrame> source,
                            EncodedPrimitiveFrame& output,std::string& error) noexcept {
    try {
        if(!source || !source->generation || !source->source_epoch ||
           source->meshes.size()>2048 || source->textures.size()>256 ||
           source->draws.size()>8192 || source->lights.size()>8 ||
           source->settings.draws || source->settings.draw_count ||
           source->settings.lights || source->settings.light_count) {
            message(error,"invalid primitive packet identity or representation");return false;
        }
        std::uint64_t payload=0,vertices=0,indices=0;
        for(const auto& mesh:source->meshes){
            if(mesh.vertices.size()>std::numeric_limits<std::uint32_t>::max() ||
               mesh.indices.size()>std::numeric_limits<std::uint32_t>::max() ||
               mesh.vertices.size()>1000000-vertices || mesh.indices.size()>3000000-indices ||
               !add_bytes(payload,mesh.vertices.size(),sizeof(Vertex)) ||
               !add_bytes(payload,mesh.indices.size(),sizeof(std::uint32_t))){
                message(error,"primitive geometry payload exceeds wire budget");return false;
            }
            vertices+=mesh.vertices.size();indices+=mesh.indices.size();
        }
        for(const auto& texture:source->textures){
            if(texture.bgra.size()>std::numeric_limits<std::uint32_t>::max() ||
               !add_bytes(payload,texture.bgra.size(),1)){
                message(error,"primitive texture payload exceeds wire budget");return false;
            }
        }
        for(const auto& draw:source->draws){
            if(draw.surface.mesh || draw.surface.texture){
                message(error,"primitive draw carries a foreign GPU handle");return false;
            }
        }
        EncodedPrimitiveFrame pending;
        pending.packet=std::move(source);
        const auto& p=*pending.packet;
        pending.meshes.reserve(p.meshes.size());pending.textures.reserve(p.textures.size());
        pending.draws.reserve(p.draws.size());pending.lights.reserve(p.lights.size());
        for(const auto& mesh:p.meshes){
            pending.meshes.push_back({mesh.id,static_cast<std::uint32_t>(mesh.vertices.size()),
                static_cast<std::uint32_t>(mesh.indices.size()),
                // API7 copies bytes synchronously with memcpy; it never dereferences
                // this as typed wire vertices. The static layout checks bind the view.
                reinterpret_cast<const A2FO_N11_Vertex*>(mesh.vertices.data()),mesh.indices.data()});
        }
        for(const auto& texture:p.textures){
            pending.textures.push_back({texture.id,texture.width,texture.height,
                texture.bgra.data(),static_cast<std::uint32_t>(texture.bgra.size())});
        }
        for(const auto& input:p.draws){
            const auto& surface=input.surface;A2FO_N11_PrimitiveDraw draw{};
            draw.mesh=input.mesh;draw.texture=input.texture;
            std::copy(surface.local_to_world,surface.local_to_world+16,draw.local_to_world);
            std::copy(surface.colour,surface.colour+4,draw.colour);
            std::copy(surface.emissive,surface.emissive+3,draw.emissive);
            draw.flags=surface.flags;draw.layer=surface.layer;
            draw.depth_mode=static_cast<std::uint32_t>(surface.depth_mode);
            draw.sampling=static_cast<std::uint32_t>(surface.sampler);
            if(surface.shadow_only)draw.options|=A2FO_N11_SURFACE_SHADOW_ONLY;
            if(!surface.casts_shadow)draw.options|=A2FO_N11_SURFACE_NO_CAST;
            if(!surface.receives_shadow)draw.options|=A2FO_N11_SURFACE_NO_RECEIVE;
            if(!surface.fog_enabled)draw.options|=A2FO_N11_SURFACE_NO_FOG;
            draw.alpha_cutoff=surface.alpha_cutoff;
            std::copy(surface.uv_transform,surface.uv_transform+6,draw.uv_transform);
            pending.draws.push_back(draw);
        }
        for(const auto& source_light:p.lights){
            A2FO_N11_Light light{};light.type=static_cast<std::uint32_t>(source_light.type);
            std::copy(source_light.position,source_light.position+3,light.position);
            std::copy(source_light.direction,source_light.direction+3,light.direction);
            std::copy(source_light.colour,source_light.colour+3,light.colour);
            light.intensity=source_light.intensity;light.falloff_start=source_light.falloff_start;
            light.falloff_range=source_light.falloff_range;
            light.cone_inner_degrees=source_light.cone_inner_degrees;
            light.cone_outer_degrees=source_light.cone_outer_degrees;
            pending.lights.push_back(light);
        }
        auto& wire=pending.frame;const auto& settings=p.settings;
        wire.struct_size=sizeof(wire);
        if(!settings.clear_colour_target)wire.flags|=A2FO_N11_PRIMITIVE_LOAD_COLOUR;
        if(!settings.clear_depth_target)wire.flags|=A2FO_N11_PRIMITIVE_LOAD_DEPTH;
        wire.generation=p.generation;wire.source_epoch=p.source_epoch;
        std::copy(settings.world_to_clip,settings.world_to_clip+16,wire.world_to_clip);
        std::copy(settings.viewport,settings.viewport+4,wire.viewport);
        std::copy(settings.ambient,settings.ambient+3,wire.ambient);
        std::copy(settings.clear_colour,settings.clear_colour+4,wire.clear_colour);
        wire.ordering=static_cast<std::uint32_t>(settings.ordering);
        wire.shadow_mask=settings.shadows.light_mask;
        wire.shadow_resolution=settings.shadows.resolution;
        wire.shadow_bias=settings.shadows.depth_bias;
        wire.shadow_filter=static_cast<std::uint32_t>(settings.shadows.filter);
        wire.fog_enabled=settings.fog.enabled?1u:0u;
        std::copy(settings.fog.colour,settings.fog.colour+3,wire.fog_colour);
        wire.fog_start=settings.fog.start;wire.fog_end=settings.fog.end;
        std::copy(settings.fog.camera_position,settings.fog.camera_position+3,wire.camera_position);
        wire.mesh_count=static_cast<std::uint32_t>(pending.meshes.size());
        wire.texture_count=static_cast<std::uint32_t>(pending.textures.size());
        wire.draw_count=static_cast<std::uint32_t>(pending.draws.size());
        wire.light_count=static_cast<std::uint32_t>(pending.lights.size());
        pending.rebind();
        output=std::move(pending);
        error.clear();return true;
    } catch(const std::bad_alloc&) {
        message(error,"primitive wire allocation failed");return false;
    } catch(...) {
        message(error,"primitive wire encoding failed");return false;
    }
}

} // namespace a2fo::native11
