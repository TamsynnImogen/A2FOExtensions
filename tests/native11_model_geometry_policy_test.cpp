#include "../renderer/native11/model_geometry_policy.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
std::uint32_t bits(float value){std::uint32_t out=0;std::memcpy(&out,&value,4);return out;}
std::uint16_t control(){std::uint16_t out=0;asm volatile("fnstcw %0":"=m"(out));return out;}
void control(std::uint16_t value){asm volatile("fldcw %0"::"m"(value):"memory");}
ModelGeometryPolicy policy(float scale,std::uint32_t encoded){return {bits(scale),encoded};}
float oracle(float value,float scale,std::uint16_t word){
    const auto saved=control();
    const auto selected=static_cast<std::uint16_t>((saved&~0x0f00u)|(word&0x0f00u)|0x003fu);
    control(selected);
    float out=0;
    asm volatile("flds %1; fmuls %2; fstps %0":"=m"(out):"m"(value),"m"(scale):"st","memory");
    control(saved);return out;
}
void rejected(ModelGeometryPolicy selected,float* xyz,std::uint32_t count,
    const std::vector<float>& before,const char* why){
    std::string error="stale";const auto caller=control();
    need(!apply_model_geometry_policy(selected,xyz,count,error)&&!error.empty()&&control()==caller,why);
    if(xyz)need(std::memcmp(xyz,before.data(),before.size()*sizeof(float))==0,
                "rejection leaves every supplied coordinate bit unchanged");
}
void cases(){
    const auto original=control();
    const auto caller=static_cast<std::uint16_t>((original&~0x0f00u)|0x0b00u);
    control(caller);
    const std::array<float,9> seed{{1.2345679f,-8.765432f,0.0f,
        -0.0f,499999.5f,-12.25f,0.33333334f,-0.000001f,17.125f}};
    for(const auto pc:std::array<std::uint16_t,3>{{0,0x0200,0x0300}}){
        for(const auto rc:std::array<std::uint16_t,4>{{0,0x0400,0x0800,0x0c00}}){
            const auto word=static_cast<std::uint16_t>(pc|rc|0x0040u);
            const float scale=1.3f;auto xyz=seed;std::string error="stale";
            need(valid_model_geometry_policy(policy(scale,encode_native_x87(word))),
                 "captured precision and rounding accepted");
            need(apply_model_geometry_policy(policy(scale,encode_native_x87(word)),xyz.data(),3,error)
                 &&error.empty()&&control()==caller,"scale succeeds and restores caller x87 CW");
            for(std::size_t i=0;i<xyz.size();++i)
                need(bits(xyz[i])==bits(oracle(seed[i],scale,word)),
                     "every XYZ matches native x87 scalar multiply bits");
        }
    }
    auto unchanged=seed;std::string error="stale";
    need(apply_model_geometry_policy(policy(1.f,encode_native_x87(0x037f)),unchanged.data(),3,error)
         &&error.empty()&&std::memcmp(unchanged.data(),seed.data(),sizeof(seed))==0&&control()==caller,
         "unity preserves signed zero and all original bits");
    const std::vector<float> before(seed.begin(),seed.end());
    auto xyz=before;
    for(const auto encoded:{0u,1u,0x100u,encode_native_x87(0x0100),0x20000u,0xffffffffu}){
        need(!valid_model_geometry_policy(policy(2.f,encoded)),"unknown or invalid CW rejected by validator");
        rejected(policy(2.f,encoded),xyz.data(),3,before,"unknown or invalid CW rejected");
    }
    for(const float scale:{0.f,-1.f,std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity()}){
        need(!valid_model_geometry_policy(policy(scale,encode_native_x87(0x037f))),
             "nonpositive or nonfinite scalar rejected by validator");
        rejected(policy(scale,encode_native_x87(0x037f)),xyz.data(),3,before,
                 "nonpositive or nonfinite scalar rejected");
    }
    const auto valid=policy(2.f,encode_native_x87(0x037f));
    rejected(valid,xyz.data(),0,before,"zero points rejected");
    rejected(valid,xyz.data(),1048577,before,"point count above one Mi rejected before access");
    rejected(valid,nullptr,1,before,"null position array rejected");
    xyz=before;xyz.back()=std::numeric_limits<float>::quiet_NaN();
    auto snapshot=xyz;
    rejected(valid,xyz.data(),3,snapshot,"last coordinate NaN rejects atomically");
    xyz=before;xyz.back()=std::numeric_limits<float>::max();snapshot=xyz;
    rejected(valid,xyz.data(),3,snapshot,"last coordinate overflow rejects atomically");
    xyz=before;xyz.back()=500000.5f;snapshot=xyz;
    rejected(valid,xyz.data(),3,snapshot,"last coordinate over bound rejects atomically");
    std::vector<float> maximum(3u*1048576u,0.f);
    error="stale";
    need(apply_model_geometry_policy(valid,maximum.data(),1048576,error)&&error.empty()
         &&control()==caller,"one Mi points accepted and caller CW restored");
    control(original);
    need(control()==original,"test restores original caller control");
}
}
int main(){cases();std::puts("PASS native11 model geometry policy");}
