#include "../rust/stage8_a1_maps_runtime_ffi.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>
struct Trace {std::vector<std::array<std::uint64_t,3>> calls;unsigned mode{};unsigned writes{};unsigned root{};};
static std::uint64_t find_op(void* opaque,std::uint32_t id,std::uint64_t a,std::uint64_t b){
    auto& t=*static_cast<Trace*>(opaque);t.calls.push_back({id,a,b});
    switch(id){case 0:return 5;case 1:return a==1?'/':'x';case 3:return 0;
    case 4:return 1;case 6:return 0;case 9:return 3;
    case 10:t.root=static_cast<unsigned>(a);return 1;case 11:return a==2&&t.root==1;
    default:return 1;}
}
static std::uint64_t mdf_op(void* opaque,std::uint32_t id,std::uint64_t a,std::uint64_t b){
    auto& t=*static_cast<Trace*>(opaque);t.calls.push_back({id,a,b});
    switch(id){case 0:return 1;case 2:return 3;case 3:return t.mode==0?0xffffffffu:5;
    case 5:return 1;default:return 1;}
}
static std::uint64_t apply_op(void* opaque,std::uint32_t id,std::uint64_t a,std::uint64_t b){
    auto& t=*static_cast<Trace*>(opaque);t.calls.push_back({id,a,b});
    switch(id){case 1:return 4;case 5:return t.mode==1&&a==3?0:1;
    case 9:return t.mode==2&&t.writes!=0?0:2;
    case 6:++t.writes;return 1;default:return 1;}
}
static std::uint64_t prefers_op(void* opaque,std::uint32_t id,std::uint64_t a,std::uint64_t b){
    auto& t=*static_cast<Trace*>(opaque);t.calls.push_back({id,a,b});
    switch(id){case 1:return 3;case 2:return a==1;case 4:return t.mode==0?0:1;
    case 5:return 1;default:return 1;}
}
static std::uint64_t resolve_op(void* opaque,std::uint32_t id,std::uint64_t a,std::uint64_t b){
    auto& t=*static_cast<Trace*>(opaque);t.calls.push_back({id,a,b});
    switch(id){case 2:return 0x1234;case 3:return t.mode==0?32:t.mode==1?33:0xffffffffu;default:return 1;}
}
static bool saw(const Trace&t,unsigned id,unsigned a){for(auto c:t.calls)if(c[0]==id&&c[1]==a)return true;return false;}
int main(){
    Trace find{};if(!a2fo_rs_stage8_a1_find_bzn(&find,find_op)||!saw(find,2,1)||!saw(find,10,2)||!saw(find,10,1))return 1;
    Trace mdf{};if(!a2fo_rs_stage8_a1_read_mdf(&mdf,mdf_op)||!saw(mdf,4,1))return 2;
    mdf.mode=1;mdf.calls.clear();if(!a2fo_rs_stage8_a1_read_mdf(&mdf,mdf_op)||!saw(mdf,4,0))return 3;
    Trace apply{};if(!a2fo_rs_stage8_a1_apply_mdf(&apply,apply_op)||apply.writes!=3||!saw(apply,8,0))return 4;
    apply={};apply.mode=1;if(a2fo_rs_stage8_a1_apply_mdf(&apply,apply_op)||apply.writes!=0)return 5;
    apply={};apply.mode=2;if(!a2fo_rs_stage8_a1_apply_mdf(&apply,apply_op)||apply.writes!=3)return 6;
    for(std::size_t i=0;i<apply.calls.size();++i)if(apply.calls[i][0]==10&&
        (i+1>=apply.calls.size()||apply.calls[i+1][0]!=9))return 7;
    Trace prefers{};if(!a2fo_rs_stage8_a1_prefers_legacy_aip(&prefers,prefers_op)||!saw(prefers,3,2)||!saw(prefers,4,0)||!saw(prefers,5,0))return 8;
    prefers.mode=1;if(a2fo_rs_stage8_a1_prefers_legacy_aip(&prefers,prefers_op))return 9;
    Trace resolve{};if(a2fo_rs_stage8_a1_resolve_aip(&resolve,resolve_op)!=0x1234||!saw(resolve,4,32))return 10;
    resolve.mode=1;resolve.calls.clear();if(a2fo_rs_stage8_a1_resolve_aip(&resolve,resolve_op)!=0x1234||!saw(resolve,5,0))return 11;
    resolve.mode=2;resolve.calls.clear();if(a2fo_rs_stage8_a1_resolve_aip(&resolve,resolve_op)!=0x1234||!saw(resolve,4,0xffffffffu))return 12;
    std::puts("A1 maps runtime policy: 12 paths PASS");
}
