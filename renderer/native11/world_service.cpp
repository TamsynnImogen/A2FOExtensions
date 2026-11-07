#include "world_service.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <new>
namespace a2fo::native11 {
namespace {
std::int32_t submit_impl(const A2FO_N11_API5& api,A2FO_N11_Session session,
    const Frame& captured,std::uint64_t epoch,const LightingFrame& lighting,bool complete,
    bool (*checkpoint)(void*) noexcept,void* context,bool* submitted_flag) noexcept {
    try {
        const auto& base=api.base.base.base.base;
        constexpr auto required=A2FO_N11_CAP_OFFSCREEN_STATIC|A2FO_N11_CAP_OFFSCREEN_SHADOWS|
            A2FO_N11_CAP_TEXTURE_BINDINGS|A2FO_N11_CAP_PART_STATES|A2FO_N11_CAP_WORLD_CASTERS;
        if(!complete||base.struct_size!=sizeof(api)||base.version!=A2FO_N11_API_VERSION_WORLD||base.reserved||
           (base.capabilities&required)!=required||!api.submit_world||!api.base.base.base.configure_shadows||
           !api.base.configure_part_states||!api.base.base.configure_texture_bindings||!api.base.base.base.configure_materials||
           !session||!epoch||!captured.camera||!captured.generation||captured.object_count>capacity||
           lighting.lights.size()>8||lighting.source_ids.size()!=lighting.lights.size()||
           (lighting.shadows.light_mask&~((1u<<lighting.lights.size())-1u)))return E_INVALIDARG;
        std::vector<A2FO_N11_Object> objects(captured.object_count);
        for(std::size_t i=0;i<captured.object_count;++i){const auto& source=captured.objects[i];auto& target=objects[i];
            const auto* end=static_cast<const char*>(std::memchr(source.model_name,0,sizeof(source.model_name)));
            if(source.model_identity!=ModelIdentity::plain_database_name||!end||end==source.model_name||
               (source.placement_identity!=PlacementIdentity::authored_placement&&source.placement_identity!=PlacementIdentity::craft_runtime_scale))return E_INVALIDARG;
            for(const char* p=source.model_name;p!=end;++p)if(static_cast<unsigned char>(*p)<32||static_cast<unsigned char>(*p)>126)return E_INVALIDARG;
            for(float v:source.world)if(!std::isfinite(v))return E_INVALIDARG;
            if(source.world[12]!=0||source.world[13]!=0||source.world[14]!=0||source.world[15]!=1)return E_INVALIDARG;
            std::copy(source.world,source.world+16,target.local_to_world);
            target.model_identifier=reinterpret_cast<const std::uint8_t*>(source.model_name);
            target.model_identifier_length=static_cast<std::uint32_t>(end-source.model_name);
            target.reserved=source.shadow_only?A2FO_N11_OBJECT_SHADOW_ONLY:0;
        }
        for(float v:captured.view.world_to_clip)if(!std::isfinite(v))return E_INVALIDARG;
        for(float v:captured.view.viewport)if(!std::isfinite(v)||v<0)return E_INVALIDARG;
        std::vector<PlacedLight> sources;sources.reserve(lighting.lights.size());
        for(std::size_t i=0;i<lighting.lights.size();++i){PlacedLight source;source.source_id=lighting.source_ids[i];
            source.config.enabled=true;source.config.light=lighting.lights[i];source.config.cast_shadows=(lighting.shadows.light_mask&(1u<<i))!=0;
            for(unsigned c=0;c<3;++c)source.local_to_world[c*4+3]=source.config.light.position[c];
            sources.push_back(source);
        }
        LightingFrame checked;std::string error;
        if(!assemble_world_lighting(sources,true,lighting.ambient,checked,error)||checked.source_ids!=lighting.source_ids||
           checked.shadows.light_mask!=lighting.shadows.light_mask)return E_INVALIDARG;
        std::array<A2FO_N11_Light,8> lights{};
        for(std::size_t i=0;i<checked.lights.size();++i){const auto& source=checked.lights[i];auto& target=lights[i];
            target.type=static_cast<std::uint32_t>(source.type);
            std::copy(source.position,source.position+3,target.position);std::copy(source.direction,source.direction+3,target.direction);
            std::copy(source.colour,source.colour+3,target.colour);target.intensity=source.intensity;
            target.falloff_start=source.falloff_start;target.falloff_range=source.falloff_range;
            target.cone_inner_degrees=source.cone_inner_degrees;target.cone_outer_degrees=source.cone_outer_degrees;
        }
        A2FO_N11_Frame frame{};frame.struct_size=sizeof(frame);frame.flags=A2FO_N11_FRAME_COMPLETE_CASTERS|
            (captured.requires_part_states?A2FO_N11_FRAME_REQUIRE_PART_STATES:0);
        frame.generation=captured.generation;frame.source_epoch=epoch;
        std::copy(captured.view.world_to_clip,captured.view.world_to_clip+16,frame.world_to_clip);
        std::copy(captured.view.viewport,captured.view.viewport+4,frame.viewport);std::copy(checked.ambient,checked.ambient+3,frame.ambient);
        frame.object_count=static_cast<std::uint32_t>(objects.size());frame.objects=objects.data();
        frame.light_count=static_cast<std::uint32_t>(checked.lights.size());frame.lights=lights.data();
        const A2FO_N11_Shadows shadows{sizeof(shadows),lighting.shadows.light_mask,lighting.shadows.resolution,lighting.shadows.depth_bias};
        if(checkpoint&&!checkpoint(context))return E_ABORT;
        const auto configured=api.base.base.base.configure_shadows(session,&shadows);
        if(checkpoint&&!checkpoint(context))return E_ABORT;
        if(checkpoint ? configured!=S_OK : configured<0)return checkpoint&&configured>0?E_FAIL:configured;
        if(checkpoint&&!checkpoint(context))return E_ABORT;
        const auto submitted=api.submit_world(session,&frame);
        if(submitted==S_OK&&submitted_flag)*submitted_flag=true;
        if(checkpoint&&!checkpoint(context))return E_ABORT;
        return checkpoint&&submitted>0?E_FAIL:submitted;
    }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
}
}
std::int32_t submit_world_capture(const A2FO_N11_API5& api,A2FO_N11_Session session,
    const Frame& captured,std::uint64_t epoch,const LightingFrame& lighting,bool complete) noexcept {
    return submit_impl(api,session,captured,epoch,lighting,complete,nullptr,nullptr,nullptr);
}
std::int32_t submit_world_capture_checked(const A2FO_N11_API5& api,A2FO_N11_Session session,
    const Frame& captured,std::uint64_t epoch,const LightingFrame& lighting,bool complete,
    bool (*checkpoint)(void*) noexcept,void* context,bool* submitted) noexcept {
    if(!checkpoint)return E_INVALIDARG;
    if(submitted)*submitted=false;
    return submit_impl(api,session,captured,epoch,lighting,complete,checkpoint,context,submitted);
}
}
