#include "model_geometry_policy.hpp"
#include <cmath>
#include <cstring>
#include <vector>

namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* message) noexcept {
    try{error=message;}catch(...){error.clear();}return false;
}
float scalar(ModelGeometryPolicy policy) noexcept {
    float value=0;std::memcpy(&value,&policy.parser_scalar_bits,sizeof(value));return value;
}
}
bool apply_model_geometry_policy(ModelGeometryPolicy policy,float* xyz,std::uint32_t count,
    std::string& error) noexcept {
    try{
        if(!valid_model_geometry_policy(policy)||!xyz||!count||count>1048576)
            return fail(error,"invalid native model geometry policy or point count");
        NativeArithmeticScope arithmetic(policy.native_x87_control);
        if(!arithmetic.valid()||!arithmetic.known())return fail(error,"unknown loader arithmetic");
        const float scale=scalar(policy);const std::size_t values=std::size_t(count)*3;
        std::vector<float> pending(values);
        for(std::size_t i=0;i<values;++i){
            if(!std::isfinite(xyz[i]))return fail(error,"nonfinite original model position");
            // ReadNode skips Mesh::Scale for scalar==1. Preserve its bits.
            if(scale==1.f)pending[i]=xyz[i];
            else asm volatile("flds %1; fmuls %2; fstps %0"
                :"=m"(pending[i]):"m"(xyz[i]),"m"(scale):"st","memory");
            if(!std::isfinite(pending[i])||std::fabs(pending[i])>1000000.f)
                return fail(error,"scaled model position exceeds supported bounds");
        }
        std::memcpy(xyz,pending.data(),values*sizeof(float));error.clear();return true;
    }catch(...){return fail(error,"model geometry policy allocation failed");}
}
} // namespace a2fo::native11
