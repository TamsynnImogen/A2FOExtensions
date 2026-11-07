#include "live_material.hpp"
#include <cmath>
namespace a2fo::native11 {
bool valid_live_material(const LiveMaterial& m) noexcept {
    if(m.texture_identifier.size()>1024||m.flags&~15u||
       (m.flags&alpha_blend&&m.flags&additive)||
       static_cast<std::uint32_t>(m.sampler)>3||static_cast<std::uint32_t>(m.depth_mode)>2||
       !std::isfinite(m.alpha_cutoff)||m.alpha_cutoff<0||m.alpha_cutoff>1)return false;
    for(unsigned char c:m.texture_identifier)if(c<32||c>126)return false;
    for(float c:m.colour)if(!std::isfinite(c)||c<0||c>1)return false;
    for(float c:m.emissive)if(!std::isfinite(c)||c<0)return false;
    for(float c:m.uv_transform)if(!std::isfinite(c))return false;
    return true;
}
}
