#include "world_lighting.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr float max_float=std::numeric_limits<float>::max();
bool finite3(const float v[3]) {
    return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);
}
bool valid_pose(const float m[16]) {
    for (int i=0;i<16;++i) if (!std::isfinite(m[i])) return false;
    if (m[12]!=0||m[13]!=0||m[14]!=0||m[15]!=1) return false;
    const double det=
        double(m[0])*(double(m[5])*m[10]-double(m[6])*m[9])-
        double(m[1])*(double(m[4])*m[10]-double(m[6])*m[8])+
        double(m[2])*(double(m[4])*m[9]-double(m[5])*m[8]);
    return std::isfinite(det)&&det!=0;
}
bool valid_light(const Light& l) {
    if (l.type!=LightType::directional&&l.type!=LightType::point&&l.type!=LightType::spot) return false;
    if (!finite3(l.position)||!finite3(l.direction)||!finite3(l.colour)||
        !std::isfinite(l.intensity)||!std::isfinite(l.falloff_start)||
        !std::isfinite(l.falloff_range)||!std::isfinite(l.cone_inner_degrees)||
        !std::isfinite(l.cone_outer_degrees)||l.intensity<0) return false;
    for (float c:l.colour) if (c<0||double(c)*l.intensity>max_float) return false;
    if (l.type!=LightType::directional &&
        (l.falloff_start<0||l.falloff_range<=0||
         double(l.falloff_start)+l.falloff_range>max_float)) return false;
    if (l.type==LightType::spot &&
        !(l.cone_inner_degrees>0&&l.cone_inner_degrees<=l.cone_outer_degrees&&
          l.cone_outer_degrees<180)) return false;
    if (l.type!=LightType::point &&
        !(std::hypot(double(l.direction[0]),double(l.direction[1]),double(l.direction[2]))>0))
        return false;
    return true;
}
bool world_direction(const float m[16],const float local[3],float out[3]) {
    double transformed[3]{};
    for (int row=0;row<3;++row)
        for (int col=0;col<3;++col)
            transformed[row]+=double(m[row*4+col])*local[col];
    const double length=std::hypot(transformed[0],transformed[1],transformed[2]);
    if (!(length>0)||!std::isfinite(length)) return false;
    for (int i=0;i<3;++i) {
        out[i]=float(transformed[i]/length);
        if (!std::isfinite(out[i])) return false;
    }
    return true;
}
bool assemble(const std::vector<PlacedLight>& sources,bool complete_world_sources,
    const float ambient[3],LightingFrame& output,const char*& failure) {
    if (!complete_world_sources) { failure="world source list is incomplete";return false; }
    if (sources.size()>4096) { failure="too many world light sources (max 4096)";return false; }
    if (!ambient||!finite3(ambient)||ambient[0]<0||ambient[1]<0||ambient[2]<0) {
        failure="invalid ambient light";return false;
    }
    std::vector<std::size_t> order(sources.size());
    std::iota(order.begin(),order.end(),0);
    std::sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b) {
        return sources[a].source_id<sources[b].source_id;
    });
    LightingFrame candidate{};
    for (int i=0;i<3;++i) candidate.ambient[i]=ambient[i];
    candidate.lights.reserve(8);
    candidate.source_ids.reserve(8);
    std::uint64_t previous=0;
    for (std::size_t index:order) {
        const auto& source=sources[index];
        if (!source.source_id||source.source_id==previous) {
            failure="zero or duplicate light source ID";return false;
        }
        previous=source.source_id;
        if (!source.config.enabled) continue;
        if (!valid_pose(source.local_to_world)) {
            failure="invalid enabled light pose";return false;
        }
        const Light& local=source.config.light;
        if (!valid_light(local)) { failure="invalid enabled light";return false; }
        if (local.intensity==0||
            (local.colour[0]==0&&local.colour[1]==0&&local.colour[2]==0)) continue;
        if (candidate.lights.size()==8) { failure="too many active lights (max 8)";return false; }
        Light world=local;
        for (int row=0;row<3;++row)
            world.position[row]=world.type==LightType::directional ? 0 : source.local_to_world[row*4+3];
        if (world.type!=LightType::point &&
            !world_direction(source.local_to_world,local.direction,world.direction)) {
            failure="invalid transformed light direction";return false;
        }
        const auto slot=static_cast<unsigned>(candidate.lights.size());
        if (source.config.cast_shadows) candidate.shadows.light_mask|=(1u<<slot);
        candidate.lights.push_back(world);
        candidate.source_ids.push_back(source.source_id);
    }
    output=std::move(candidate);
    return true;
}
} // namespace

bool assemble_world_lighting(const std::vector<PlacedLight>& sources,
    bool complete_world_sources,const float ambient[3],LightingFrame& output,
    std::string& error) noexcept {
    try {
        const char* failure=nullptr;
        if (!assemble(sources,complete_world_sources,ambient,output,failure)) {
            error=failure;
            return false;
        }
        error.clear();
        return true;
    } catch (...) {
        error.clear();
        return false;
    }
}
} // namespace a2fo::native11
