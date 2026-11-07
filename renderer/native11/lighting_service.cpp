#include "lighting_service.hpp"
#include <windows.h>
#include <cmath>
#include <cstring>
namespace a2fo::native11 {
std::int32_t submit_lit_capture(const A2FO_N11_API4& api,A2FO_N11_Session session,
    const Frame& captured,std::uint64_t epoch,const LightingFrame& lighting) noexcept {
    try {
        const auto& base=api.base.base.base;
        constexpr auto required=A2FO_N11_CAP_OFFSCREEN_STATIC|A2FO_N11_CAP_OFFSCREEN_SHADOWS|
            A2FO_N11_CAP_TEXTURE_BINDINGS|A2FO_N11_CAP_PART_STATES;
        if(captured.complete_casters||base.struct_size!=sizeof(api)||base.version!=A2FO_N11_API_VERSION_PART_STATES||base.reserved||
           (base.capabilities&required)!=required||!api.base.base.configure_shadows||
           !api.configure_part_states||!api.base.configure_texture_bindings||!api.base.base.configure_materials||
           !base.submit||!session||!epoch||!captured.camera||!captured.generation||captured.object_count>capacity||
           lighting.lights.size()>8||lighting.source_ids.size()!=lighting.lights.size()||
           (lighting.shadows.light_mask&~((1u<<lighting.lights.size())-1u)))return E_INVALIDARG;
        for(float v:captured.view.world_to_clip)if(!std::isfinite(v))return E_INVALIDARG;
        for(float v:captured.view.viewport)if(!std::isfinite(v)||v<0)return E_INVALIDARG;
        for(std::size_t i=0;i<captured.object_count;++i){const auto& object=captured.objects[i];
            if(object.shadow_only)return E_INVALIDARG;
            if(object.model_identity!=ModelIdentity::plain_database_name||!object.model_name[0]||
               !std::memchr(object.model_name,0,sizeof(object.model_name))||
               (object.placement_identity!=PlacementIdentity::authored_placement&&
                object.placement_identity!=PlacementIdentity::craft_runtime_scale))return E_INVALIDARG;
            for(const unsigned char c:std::string(object.model_name))if(c<32||c>126)return E_INVALIDARG;
            for(float v:object.world)if(!std::isfinite(v))return E_INVALIDARG;
            if(object.world[12]!=0||object.world[13]!=0||object.world[14]!=0||object.world[15]!=1)return E_INVALIDARG;
        }
        std::vector<PlacedLight> sources; sources.reserve(lighting.lights.size());
        for(std::size_t i=0;i<lighting.lights.size();++i){
            PlacedLight source;source.source_id=lighting.source_ids[i];source.config.enabled=true;
            source.config.light=lighting.lights[i];source.config.cast_shadows=(lighting.shadows.light_mask&(1u<<i))!=0;
            for(unsigned c=0;c<3;++c)source.local_to_world[c*4+3]=source.config.light.position[c];
            sources.push_back(source);
        }
        LightingFrame checked;std::string error;
        if(!assemble_world_lighting(sources,true,lighting.ambient,checked,error)||
           checked.source_ids!=lighting.source_ids||checked.shadows.light_mask!=lighting.shadows.light_mask)return E_INVALIDARG;
        const A2FO_N11_Shadows shadows{sizeof(shadows),lighting.shadows.light_mask,
            lighting.shadows.resolution,lighting.shadows.depth_bias};
        const auto configured=api.base.base.configure_shadows(session,&shadows);
        if(configured<0)return configured;
        return submit_captured(api,session,captured,epoch,checked.lights.data(),
            static_cast<std::uint32_t>(checked.lights.size()),checked.ambient);
    }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
}
}
