#include "lights.hpp"
#include <cmath>
#include <cstdint>

namespace a2fo::refit {
namespace {
bool fail(std::string& error,const char* reason) noexcept {
    try {error=reason;} catch(...) {error.clear();}
    return false;
}
bool finite3(const float* values) noexcept {
    for(unsigned i=0;i<3;++i)
        if(!std::isfinite(values[i])||std::fabs(values[i])>1e12f)return false;
    return true;
}
bool valid(const A2FORefitLight& light) noexcept {
    if((light.kind!=A2FO_REFIT_DIRECTIONAL&&light.kind!=A2FO_REFIT_POINT)||
       !finite3(light.position)||!finite3(light.direction)||!finite3(light.colour)||
       !std::isfinite(light.intensity)||!std::isfinite(light.start)||
       !std::isfinite(light.range)||light.intensity<0||light.intensity>1e12f||
       light.start<0||light.start>1e12f||light.range<0||light.range>1e12f||
       (light.kind==A2FO_REFIT_POINT&&light.range<=0)||
       (light.kind==A2FO_REFIT_DIRECTIONAL&&
        std::hypot(light.direction[0],light.direction[1],light.direction[2])<1e-6f))
        return false;
    for(float c:light.colour)if(c<0||double(c)*light.intensity>1e12)return false;
    return true;
}
bool equal3(const float* a,const float* b) noexcept {
    return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2];
}
bool equal_light(const native11::Light& a,const native11::Light& b) noexcept {
    return a.type==b.type&&equal3(a.position,b.position)&&
           equal3(a.direction,b.direction)&&equal3(a.colour,b.colour)&&
           a.intensity==b.intensity&&a.falloff_start==b.falloff_start&&
           a.falloff_range==b.falloff_range&&
           a.cone_inner_degrees==b.cone_inner_degrees&&
           a.cone_outer_degrees==b.cone_outer_degrees;
}
}

bool append_world_lights2(const std::vector<A2FORefitLight>& registered,
                          const native11::LightingFrame& odf,
                          std::vector<A2FORefitLight>& out,
                          std::string& error) noexcept {
    try {
        if(odf.lights.size()!=odf.source_ids.size()||odf.lights.size()>8||
           registered.size()>32||odf.lights.size()>32-registered.size())
            return fail(error,"world light count or source ID count is invalid");
        if(!finite3(odf.ambient)||odf.ambient[0]<0||odf.ambient[1]<0||
           odf.ambient[2]<0)
            return fail(error,"invalid world ambient light");
        std::uint64_t previous=0;
        unsigned directional=0;
        std::vector<A2FORefitLight> next;
        next.reserve(registered.size()+odf.lights.size());
        for(const auto& light:registered){
            if(!valid(light))return fail(error,"invalid registered light");
            if(light.kind==A2FO_REFIT_DIRECTIONAL&&++directional>4)
                return fail(error,"directional light limit");
            next.push_back(light);
        }
        for(std::size_t i=0;i<odf.lights.size();++i){
            const auto id=odf.source_ids[i];
            if(!id||id<=previous)return fail(error,"invalid world source ID order");
            previous=id;
            const auto& source=odf.lights[i];
            if(!std::isfinite(source.cone_inner_degrees)||
               !std::isfinite(source.cone_outer_degrees))
                return fail(error,"invalid world light cone metadata");
            A2FORefitLight light{};
            if(source.type==native11::LightType::directional)
                light.kind=A2FO_REFIT_DIRECTIONAL;
            else if(source.type==native11::LightType::point)
                light.kind=A2FO_REFIT_POINT;
            else return fail(error,"unsupported world light type");
            for(unsigned component=0;component<3;++component){
                light.position[component]=source.position[component];
                light.direction[component]=source.direction[component];
                light.colour[component]=source.colour[component];
            }
            light.intensity=source.intensity;
            light.start=source.falloff_start;
            light.range=source.falloff_range;
            if(!valid(light))return fail(error,"invalid world light");
            if(light.kind==A2FO_REFIT_DIRECTIONAL&&++directional>4)
                return fail(error,"directional light limit");
            next.push_back(light);
        }
        out=std::move(next);
        error.clear();
        return true;
    } catch(...) {return fail(error,"world light allocation failed");}
}

bool append_world_lights3(const std::vector<A2FORefitLight>& registered,
                          const native11::LightingFrame& odf,
                          std::vector<A2FORefitLight>& out,
                          std::vector<A2FORefitSpot3>& spots,
                          std::string& error) noexcept {
    try {
        // Split spots out, convert everything else with the unchanged ABI2 rules.
        native11::LightingFrame rest=odf;rest.lights.clear();rest.source_ids.clear();
        std::vector<A2FORefitSpot3> next_spots;
        if(odf.lights.size()!=odf.source_ids.size())
            return fail(error,"world light count or source ID count is invalid");
        std::uint64_t previous=0;
        for(std::size_t i=0;i<odf.lights.size();++i){
            const auto id=odf.source_ids[i];
            if(!id||id<=previous)return fail(error,"invalid world source ID order");
            previous=id;
            const auto& source=odf.lights[i];
            if(source.type!=native11::LightType::spot){
                rest.lights.push_back(source);rest.source_ids.push_back(id);continue;
            }
            if(next_spots.size()>=A2FO_REFIT_MAX_SPOTS)return fail(error,"spot light limit");
            const float inner=source.cone_inner_degrees,outer=source.cone_outer_degrees;
            if(!std::isfinite(inner)||!std::isfinite(outer)||!(inner>0)||inner>outer||!(outer<180))
                return fail(error,"invalid world spot cone");
            A2FORefitSpot3 spot{};
            for(unsigned c=0;c<3;++c){
                spot.position[c]=source.position[c];spot.direction[c]=source.direction[c];
                spot.colour[c]=source.colour[c];
            }
            spot.intensity=source.intensity;spot.start=source.falloff_start;spot.range=source.falloff_range;
            constexpr float radians=3.14159265358979323846f/180.f;
            spot.inner=inner*radians;spot.outer=outer*radians;
            const float length2=spot.direction[0]*spot.direction[0]+spot.direction[1]*spot.direction[1]+
                spot.direction[2]*spot.direction[2];
            if(!finite3(spot.position)||!finite3(spot.direction)||!finite3(spot.colour)||
               spot.colour[0]<0||spot.colour[1]<0||spot.colour[2]<0||!std::isfinite(spot.intensity)||
               spot.intensity<0||!std::isfinite(spot.start)||spot.start<0||!std::isfinite(spot.range)||
               !(spot.range>0)||!(length2>1e-12f))
                return fail(error,"invalid world spot light");
            next_spots.push_back(spot);
        }
        std::vector<A2FORefitLight> next;
        if(!append_world_lights2(registered,rest,next,error))return false;
        out=std::move(next);spots=std::move(next_spots);
        error.clear();
        return true;
    } catch(...) {return fail(error,"world light allocation failed");}
}

bool equal_world_lighting2(const native11::LightingFrame& a,
                           const native11::LightingFrame& b) noexcept {
    if(a.source_ids!=b.source_ids||a.lights.size()!=b.lights.size()||
       !equal3(a.ambient,b.ambient)||
       a.shadows.light_mask!=b.shadows.light_mask||
       a.shadows.resolution!=b.shadows.resolution||
       a.shadows.depth_bias!=b.shadows.depth_bias||
       a.shadows.filter!=b.shadows.filter)return false;
    for(std::size_t i=0;i<a.lights.size();++i)
        if(!equal_light(a.lights[i],b.lights[i]))return false;
    return true;
}
} // namespace a2fo::refit
