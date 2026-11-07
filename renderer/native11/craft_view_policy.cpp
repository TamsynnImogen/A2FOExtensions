#include "craft_view_policy.hpp"
#include "native_arithmetic.hpp"
#include <cmath>
#include <limits>

#if !defined(__i386__) && !defined(__x86_64__)
#error Native Craft view parity requires an x87 target
#endif

namespace a2fo::native11 {
namespace {
bool position(float value) noexcept {
    return std::isfinite(value)&&std::fabs(value)<=1000000.0f;
}
float stored_difference(float a,float b) noexcept {
    float value=0;
    asm volatile("flds %1; fsubs %2; fstps %0"
        :"=m"(value):"m"(a),"m"(b):"st","memory");
    return value;
}
float weighted(const CraftViewInput& input) noexcept {
    const float dx=stored_difference(input.position[0],input.camera[0]);
    const float dy=stored_difference(input.position[1],input.camera[1]);
    const float dz=stored_difference(input.position[2],input.camera[2]);
    constexpr float quarter=0.25f;
    float value=0;
    asm volatile(
        "flds %1; fmuls %1; flds %2; fmuls %2; faddp; "
        "flds %3; fmuls %4; fmul %%st(0),%%st(0); faddp; fsqrt; fstps %0"
        :"=m"(value):"m"(dx),"m"(dz),"m"(dy),"m"(quarter):"st","st(1)","memory");
    return value;
}
float euclidean(const CraftViewInput& input) noexcept {
    float value=0;
    asm volatile(
        "flds %1; fsubs %2; fmul %%st(0),%%st(0); "
        "flds %3; fsubs %4; fmul %%st(0),%%st(0); faddp; "
        "flds %5; fsubs %6; fmul %%st(0),%%st(0); faddp; fsqrt; fstps %0"
        :"=m"(value)
        :"m"(input.camera[2]),"m"(input.position[2]),
         "m"(input.camera[1]),"m"(input.position[1]),
         "m"(input.camera[0]),"m"(input.position[0])
        :"st","st(1)","memory");
    return value;
}
long double threshold(const CraftViewInput& input) noexcept {
    float base=input.max_distance;
    if(!input.detailed_range){
        constexpr float factor=0.7f;
        asm volatile("flds %1; fmuls %2; fstps %0"
            :"=m"(base):"m"(input.max_distance),"m"(factor):"st","memory");
    }
    if(input.radius>100.0f){
        constexpr double percent=0.01;
        long double result=0;
        asm volatile("flds %1; fmuls %2; fmull %3; fstpt %0"
            :"=m"(result):"m"(input.radius),"m"(base),"m"(percent):"st","memory");
        return result;
    }
    return static_cast<long double>(base);
}
}
bool native_craft_view(const CraftViewInput& input,std::uint32_t native_x87_control,
    CraftViewResult& output) noexcept {
    if(!native_x87_control||!valid_native_x87_encoding(native_x87_control)||
       !position(input.camera[0])||!position(input.camera[1])||!position(input.camera[2])||
       !position(input.position[0])||!position(input.position[1])||!position(input.position[2])||
       !std::isfinite(input.radius)||input.radius<0||input.radius>1000000.0f||
       !std::isfinite(input.max_distance)||input.max_distance<=0||
       input.max_distance>1000000000.0f||
       !std::isfinite(input.near_fade_min)||!std::isfinite(input.near_fade_max)||
       !std::isfinite(input.near_fade_amount)||input.near_fade_min<0||
       input.near_fade_max<=input.near_fade_min||input.near_fade_amount<0||input.near_fade_amount>1)return false;
    NativeArithmeticScope arithmetic(native_x87_control);
    if(!arithmetic.valid()||!arithmetic.known())return false;
    CraftViewResult candidate;
    candidate.weighted_distance=weighted(input);
    candidate.distance=euclidean(input);
    const long double limit=threshold(input);
    if(!std::isfinite(candidate.weighted_distance)||!std::isfinite(candidate.distance)||
       !std::isfinite(limit))return false;
    candidate.culled=(input.instance_cull_flag&&
        (input.mode==0||!input.detailed_range)&&!input.reveal_all)||
        limit<candidate.weighted_distance;
    if(input.fade_enabled){
        // Preserve the native extended start and the association of each
        // following operation. Volatile long double is x87 80-bit on targets.
        static_assert(std::numeric_limits<long double>::digits==64);
        volatile long double start=static_cast<long double>(0.9f)*limit;
        if(!(start>candidate.weighted_distance)){
            volatile long double numerator=static_cast<long double>(limit)-candidate.weighted_distance;
            volatile long double denominator=static_cast<long double>(limit)-start;
            candidate.alpha=static_cast<float>(numerator/denominator);
            candidate.fade_written=true;
        }else if(input.mode!=2&&!input.skip_near_fade&&input.radius>0){
            volatile long double ratio=candidate.distance==0?static_cast<long double>(input.near_fade_max):
                static_cast<long double>(input.radius)/candidate.distance;
            if(ratio>input.near_fade_min){
                if(ratio>input.near_fade_max)ratio=input.near_fade_max;
                volatile long double delta=ratio-input.near_fade_min;
                volatile long double product=delta*input.near_fade_amount;
                volatile long double divisor=static_cast<long double>(input.near_fade_max)-input.near_fade_min;
                candidate.alpha=static_cast<float>(1.0-product/divisor);
                candidate.fade_written=true;
            }
        }
    }
    if(!std::isfinite(candidate.alpha))return false;
    output=candidate;return true;
}
} // namespace a2fo::native11
