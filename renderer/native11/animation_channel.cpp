#include "animation_channel.hpp"
#include <cmath>

#if !defined(__i386__) && !defined(__x86_64__)
#error Native animation channel parity requires an x87 target
#endif

namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* message) noexcept {
    try {error=message;}catch(...){error.clear();}return false;
}
float native_lerp(float current,float next,float fraction) noexcept {
    float result=0;
    // Float operands enter the x87 stack; difference and multiply remain
    // extended until the one final binary32 store. Caller CW is untouched.
    asm volatile("flds %1; fsubs %2; fmuls %3; fadds %2; fstps %0"
        :"=m"(result):"m"(next),"m"(current),"m"(fraction):"st","memory");
    return result;
}
bool finite_lerp_intermediates(float current,float next,float fraction) noexcept {
    const long double delta=static_cast<long double>(next)-current;
    const long double product=delta*fraction;
    return std::isfinite(delta)&&std::isfinite(product)&&
        std::isfinite(product+current);
}
}

bool evaluate_scalar_animation_channel(const ScalarAnimationChannel& input,
    ScalarAnimationSample& output,std::string& error) noexcept {
    try {
        if(input.interpolation>3||!input.timing.key_count||input.timing.key_count>65535||
           input.keys.size()!=std::size_t(input.timing.key_count)+(input.interpolation==3?1u:0u))
            return fail(error,"unsupported scalar interpolation or key shape");
        for(float key:input.keys)if(!std::isfinite(key))return fail(error,"nonfinite scalar key");
        ScalarAnimationSample candidate;
        if(!evaluate_animation_phase(input.timing,candidate.phase,error))return false;
        const auto mode=candidate.phase.finished?1u:input.interpolation;
        if(mode){
            candidate.writes=true;
            const auto key=candidate.phase.key;
            if(key>=input.timing.key_count)return fail(error,"scalar phase key out of range");
            if(mode==1)candidate.value=input.keys[key];
            else {
                const auto next=mode==2?(key+1==input.timing.key_count?0u:key+1):key+1;
                if(!finite_lerp_intermediates(input.keys[key],input.keys[next],candidate.phase.fraction))
                    return fail(error,"nonfinite scalar interpolation intermediate");
                candidate.value=native_lerp(input.keys[key],input.keys[next],candidate.phase.fraction);
            }
            if(!std::isfinite(candidate.value))return fail(error,"nonfinite scalar result");
        }
        output=candidate;error.clear();return true;
    }catch(...){return fail(error,"scalar channel allocation failed");}
}

bool evaluate_matrix_animation_channel(const MatrixAnimationChannel& input,
    MatrixAnimationSample& output,std::string& error) noexcept {
    try {
        if(input.interpolation>3||!input.timing.key_count||input.timing.key_count>65535||
           input.keys.size()!=input.timing.key_count)
            return fail(error,"unsupported matrix interpolation or key shape");
        for(const auto& key:input.keys)for(float value:key)
            if(!std::isfinite(value))return fail(error,"nonfinite Matrix34 key");
        MatrixAnimationSample candidate;
        if(!evaluate_animation_phase(input.timing,candidate.phase,error))return false;
        const auto mode=candidate.phase.finished?1u:input.interpolation;
        if(mode){
            if(candidate.phase.key>=input.timing.key_count)return fail(error,"matrix phase key out of range");
            candidate.writes=true;candidate.value=input.keys[candidate.phase.key];
        }
        output=candidate;error.clear();return true;
    }catch(...){return fail(error,"matrix channel allocation failed");}
}
} // namespace a2fo::native11
