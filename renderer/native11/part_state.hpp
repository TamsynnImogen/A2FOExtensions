#pragma once
#include <cmath>
#include <cstdint>
#include <string>
namespace a2fo::native11 {
struct PartState {
    bool visible=true;
    float local_to_model[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
};
inline bool valid_part_state(const PartState& state) noexcept {
    const auto* m=state.local_to_model;
    for(unsigned i=0;i<16;++i)if(!std::isfinite(m[i]))return false;
    if(m[12]!=0||m[13]!=0||m[14]!=0||m[15]!=1)return false;
    if(!state.visible)return true;
    const double determinant=double(m[0])*(double(m[5])*m[10]-double(m[6])*m[9])-
        double(m[1])*(double(m[4])*m[10]-double(m[6])*m[8])+
        double(m[2])*(double(m[4])*m[9]-double(m[5])*m[8]);
    return std::isfinite(determinant)&&std::fabs(determinant)>=1e-20;
}
using ResolvePartState=bool(*)(void*,const std::string& model,std::uint32_t part,
    const float authored_local_to_model[16],PartState&,std::string& error);
}
