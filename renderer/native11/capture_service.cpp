#include "capture_service.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <vector>

namespace a2fo::native11 {
static std::int32_t submit_impl(const A2FO_N11_API&,A2FO_N11_Session,const Frame&,
    std::uint64_t,const Light*,std::uint32_t,const float[3],std::uint32_t) noexcept;
std::int32_t submit_captured(const A2FO_N11_API4& api,A2FO_N11_Session session,
    const Frame& captured,std::uint64_t epoch,const Light* source,std::uint32_t count,
    const float ambient[3]) noexcept {
    if(captured.complete_casters||api.base.base.base.struct_size!=sizeof(api)||api.base.base.base.version!=A2FO_N11_API_VERSION_PART_STATES||
       !(api.base.base.base.capabilities&A2FO_N11_CAP_PART_STATES)||!api.configure_part_states||
       !api.base.configure_texture_bindings||!api.base.base.configure_materials||!api.base.base.configure_shadows)return E_INVALIDARG;
    auto prefix=api.base.base.base;prefix.struct_size=sizeof(prefix);prefix.version=A2FO_N11_API_VERSION;
    return submit_impl(prefix,session,captured,epoch,source,count,ambient,
        captured.requires_part_states?A2FO_N11_FRAME_REQUIRE_PART_STATES:0);
}
std::int32_t submit_captured(const A2FO_N11_API3& api,A2FO_N11_Session session,
    const Frame& captured,std::uint64_t epoch,const Light* source,std::uint32_t count,
    const float ambient[3]) noexcept {
    if(api.base.base.struct_size!=sizeof(api)||api.base.base.version!=A2FO_N11_API_VERSION_TEXTURE_BINDINGS||
       !(api.base.base.capabilities&A2FO_N11_CAP_TEXTURE_BINDINGS)||!api.configure_texture_bindings||
       !api.base.configure_materials)return E_INVALIDARG;
    auto prefix=api.base;prefix.base.struct_size=sizeof(prefix);prefix.base.version=A2FO_N11_API_VERSION_SHADOWS;
    return submit_captured(prefix,session,captured,epoch,source,count,ambient);
}
std::int32_t submit_captured(const A2FO_N11_API2& api,A2FO_N11_Session session,
    const Frame& captured,std::uint64_t epoch,const Light* source,std::uint32_t count,
    const float ambient[3]) noexcept {
    if((api.base.struct_size!=sizeof(api)&&api.base.struct_size!=A2FO_N11_API2_SHADOWS_SIZE)||api.base.version!=A2FO_N11_API_VERSION_SHADOWS||
       !(api.base.capabilities&A2FO_N11_CAP_OFFSCREEN_SHADOWS)||!api.configure_shadows)return E_INVALIDARG;
    // Version 2's prefix retains the exact version-1 call signatures. The
    // session owns its configured shadow settings independently of the table.
    auto prefix=api.base;prefix.struct_size=sizeof(prefix);prefix.version=A2FO_N11_API_VERSION;
    return submit_captured(prefix,session,captured,epoch,source,count,ambient);
}
std::int32_t submit_captured(const A2FO_N11_API& api,A2FO_N11_Session session,
    const Frame& captured,std::uint64_t epoch,const Light* source,std::uint32_t count,
    const float ambient[3]) noexcept {
    if(captured.requires_part_states)return E_INVALIDARG;
    return submit_impl(api,session,captured,epoch,source,count,ambient,0);
}
static std::int32_t submit_impl(const A2FO_N11_API& api,A2FO_N11_Session session,
    const Frame& captured,std::uint64_t epoch,const Light* source,std::uint32_t count,
    const float ambient[3],std::uint32_t flags) noexcept {
    if(captured.complete_casters||api.struct_size!=sizeof(api)||api.version!=A2FO_N11_API_VERSION||api.reserved||
       !(api.capabilities&A2FO_N11_CAP_OFFSCREEN_STATIC)||!api.submit||!session||
       !captured.camera||!captured.generation||!epoch||captured.object_count>capacity||
       count>8||(count&&!source)||!ambient)return E_INVALIDARG;
    try {
        std::vector<A2FO_N11_Object> objects(captured.object_count);
        for(std::size_t i=0;i<captured.object_count;++i){const auto& src=captured.objects[i];auto& dst=objects[i];
            if(src.shadow_only)return E_INVALIDARG; // only API5 submit_world admits this role
            const auto* end=static_cast<const char*>(std::memchr(src.model_name,0,sizeof(src.model_name)));
            if(src.model_identity!=ModelIdentity::plain_database_name||!end||end==src.model_name||
               (src.placement_identity!=PlacementIdentity::authored_placement&&
                src.placement_identity!=PlacementIdentity::craft_runtime_scale))return E_INVALIDARG;
            std::copy(src.world,src.world+16,dst.local_to_world);
            dst.model_identifier=reinterpret_cast<const std::uint8_t*>(src.model_name);
            dst.model_identifier_length=static_cast<std::uint32_t>(end-src.model_name);
        }
        std::array<A2FO_N11_Light,8> lights{};
        for(std::uint32_t i=0;i<count;++i){const auto& src=source[i];auto& dst=lights[i];
            dst.type=static_cast<std::uint32_t>(src.type);
            std::copy(src.position,src.position+3,dst.position);std::copy(src.direction,src.direction+3,dst.direction);
            std::copy(src.colour,src.colour+3,dst.colour);dst.intensity=src.intensity;
            dst.falloff_start=src.falloff_start;dst.falloff_range=src.falloff_range;
            dst.cone_inner_degrees=src.cone_inner_degrees;dst.cone_outer_degrees=src.cone_outer_degrees;
        }
        A2FO_N11_Frame input{};input.struct_size=sizeof(input);input.flags=flags;input.generation=captured.generation;input.source_epoch=epoch;
        std::copy(captured.view.world_to_clip,captured.view.world_to_clip+16,input.world_to_clip);
        std::copy(captured.view.viewport,captured.view.viewport+4,input.viewport);
        std::copy(ambient,ambient+3,input.ambient);
        input.object_count=static_cast<std::uint32_t>(objects.size());input.objects=objects.data();
        input.light_count=count;input.lights=lights.data();
        return api.submit(session,&input);
    }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
}
}
