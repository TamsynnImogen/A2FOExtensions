#pragma once
#include "scene.hpp"
#include <string>
namespace a2fo::native11 {
struct MaterialSeed {
    std::uint32_t part_index=0,node_index=UINT32_MAX,group_index=UINT32_MAX;
    std::string authored_texture;
    float colour[4]{1,1,1,1},emissive[3]{};
    std::uint32_t flags=0;
};
struct LiveMaterial {
    std::string texture_identifier;
    float colour[4]{1,1,1,1},emissive[3]{};
    std::uint32_t flags=0;
    Sampling sampler=Sampling::linear_clamp;
    DepthMode depth_mode=DepthMode::automatic;
    float alpha_cutoff=.5f,uv_transform[6]{1,0,0,0,1,0};
};
bool valid_live_material(const LiveMaterial&) noexcept;
using ResolveLiveMaterial=bool(*)(void*,const std::string&,const MaterialSeed&,
    LiveMaterial&,std::string&);
}
