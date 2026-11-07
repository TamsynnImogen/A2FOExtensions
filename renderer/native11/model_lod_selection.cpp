#include "model_lod_selection.hpp"
#include <algorithm>
#include <cmath>

#if !defined(__i386__) && !defined(__x86_64__)
#error Native LOD parity requires an x87 target
#endif

namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* message) noexcept {
    try {error=message;}catch(...){error.clear();}return false;
}
}
bool select_model_lod(const ModelLodSelectionInput& input,std::uint32_t& index,
    std::string& error) noexcept {
    if(input.level_count>4096)return fail(error,"native LOD level limit exceeded");
    if(!input.level_count){index=UINT32_MAX;error.clear();return true;}
    if(input.force_highest){index=input.level_count-1;error.clear();return true;}
    auto chosen=input.requested;
    if(chosen==-1){
        if(!std::isfinite(input.radius)||input.radius<0)
            return fail(error,"invalid native LOD radius");
        for(float value:input.translation)if(!std::isfinite(value))
            return fail(error,"nonfinite native LOD translation");
        for(float value:input.centre)if(!std::isfinite(value))
            return fail(error,"nonfinite native LOD centre");
        for(float value:input.thresholds)if(!std::isfinite(value))
            return fail(error,"nonfinite native LOD threshold");
        float distance=0;unsigned short status=0;
        // VA 630a98..630b20: retain all three sums and products on the x87
        // stack. The FIRST comparison uses the unrounded distance despite
        // the nonpopping binary32 store; subsequent comparisons reload it.
        asm volatile(
            "flds %2; fadds %3; flds %4; fadds %5; flds %6; fadds %7; "
            "fld %%st(0); fmul %%st(1),%%st; fld %%st(2); fmul %%st(3),%%st; "
            "faddp; fld %%st(3); fmul %%st(4),%%st; faddp; fsqrt; "
            "fstp %%st(3); fstp %%st(0); fstp %%st(0); fsubs %8; "
            "fsts %0; fcomps %9; fnstsw %%ax"
            :"=m"(distance),"=a"(status)
            :"m"(input.translation[0]),"m"(input.centre[0]),
             "m"(input.translation[1]),"m"(input.centre[1]),
             "m"(input.translation[2]),"m"(input.centre[2]),
             "m"(input.radius),"m"(input.thresholds[3])
            :"st","st(1)","st(2)","st(3)","st(4)","memory");
        chosen=4;
        if(!(status&0x100u)){
            chosen=3;
            while(chosen>0&&!(distance<input.thresholds[chosen-1]))--chosen;
        }
        chosen=std::min(chosen,input.quality_cap);
    }
    const auto candidate=chosen<=0?0u:std::min(static_cast<std::uint32_t>(chosen),input.level_count-1);
    index=candidate;error.clear();return true;
}
} // namespace a2fo::native11
