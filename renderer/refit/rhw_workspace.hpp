#pragma once
#include "geometry_parity.hpp"
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <utility>

namespace a2fo::refit {
struct RhwLayout2 {std::uint32_t format=0,stride=0,uv_offset=0;};
inline bool rhw_layout2(std::uint32_t format,std::uint32_t stride,std::uint32_t model,
    RhwLayout2& output,std::string& error) noexcept {
    if(!((format==0x144&&stride==28&&model<=1)||(format==0x1c4&&stride==32&&model==2))){
        try{error="unsupported native RHW material/layout pairing";}catch(...){error.clear();}
        return false;
    }
    output={format,stride,format==0x144?20u:24u};error.clear();return true;
}
inline bool decode_rhw_triangles2(const std::vector<std::uint8_t>& vertices,
    const std::vector<std::uint16_t>& indices,const RhwLayout2& layout,float offset,
    std::vector<ProjectedTriangle2>& output,std::string& error) noexcept {
    const auto fail=[&](const char* why) noexcept {try{error=why;}catch(...){error.clear();}return false;};
    try{
        if(!((layout.format==0x144&&layout.stride==28&&layout.uv_offset==20)||
             (layout.format==0x1c4&&layout.stride==32&&layout.uv_offset==24)))
            return fail("invalid RHW vertex layout");
        if(!std::isfinite(offset)||vertices.empty()||vertices.size()%layout.stride||
           vertices.size()/layout.stride>65535||indices.empty()||indices.size()%3||indices.size()>6144)
            return fail("invalid RHW buffer sizes or raster offset");
        const auto count=vertices.size()/layout.stride;
        std::vector<ProjectedTriangle2> next(indices.size()/3);
        for(std::size_t i=0;i<indices.size();++i){
            if(indices[i]>=count)return fail("RHW index outside vertex buffer");
            const auto* data=vertices.data()+std::size_t(indices[i])*layout.stride;
            float position[4]{},uv[2]{};std::memcpy(position,data,sizeof(position));
            std::memcpy(uv,data+layout.uv_offset,sizeof(uv));
            for(const float value:position)if(!std::isfinite(value))return fail("nonfinite RHW position");
            if(position[3]<=0||!std::isfinite(uv[0])||!std::isfinite(uv[1]))
                return fail("invalid RHW reciprocal W or UV");
            const float x=position[0]+offset,y=position[1]+offset;
            if(!std::isfinite(x)||!std::isfinite(y))return fail("RHW raster adjustment overflow");
            next[i/3][i%3]={x,y,position[2],uv[0],uv[1],position[3]};
        }
        output=std::move(next);error.clear();return true;
    }catch(...){return fail("RHW triangle decode allocation failed");}
}
} // namespace a2fo::refit
