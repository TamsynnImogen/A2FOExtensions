#include "animation_phase.hpp"
#include <cmath>
#include <limits>

#if !defined(__i386__) && !defined(__x86_64__)
#error Native animation phase parity requires an x87 target
#endif

namespace a2fo::native11 {
namespace {
void fail(std::string& error,const char* message) noexcept {try{error=message;}catch(...){error.clear();}}
// The original uses its private x87 rounding helper around intermediate float
// stores. SSE state and the caller's other control bits must remain untouched.
struct RoundDown {
    unsigned short saved=0;
    RoundDown() noexcept {
        asm volatile("fnstcw %0":"=m"(saved));
        const unsigned short down=static_cast<unsigned short>((saved&~0x0c00u)|0x0400u);
        asm volatile("fldcw %0"::"m"(down):"memory");
    }
    ~RoundDown(){restore();}
    void restore() const noexcept {asm volatile("fldcw %0"::"m"(saved):"memory");}
};
float stored(long double value) noexcept {
    float result=0;
    asm volatile("fldt %1; fstps %0":"=m"(result):"m"(value):"st","memory");
    return result;
}
}
bool evaluate_animation_phase(const AnimationPhaseInput& input,AnimationPhase& output,
                              std::string& error) noexcept {
    if(!std::isfinite(input.elapsed)||!std::isfinite(input.offset)||!std::isfinite(input.duration)||
       input.elapsed<0||input.offset<0||input.duration<=0||!input.key_count||input.key_count>65535||
       (!input.timestamps.empty()&&input.timestamps.size()!=input.key_count)){
        fail(error,"unsupported animation timing or key count");return false;
    }
    if(!input.timestamps.empty()){
        float previous=-1;
        for(float time:input.timestamps){
            if(!std::isfinite(time)||time<0||time>=input.duration||time<=previous){
                fail(error,"unsupported animation key timestamps");return false;
            }
            previous=time;
        }
        if(input.timestamps.front()!=0){fail(error,"animation first key must start at zero");return false;}
    }
    RoundDown rounding;
    const long double time=static_cast<long double>(input.elapsed)+input.offset;
    AnimationPhase candidate;
    if(!(input.flags&1u)&&time>=input.duration){
        candidate.key=(input.flags&4u)?0:input.key_count-1;
        candidate.fraction=-1;candidate.finished=true;
        output=candidate;error.clear();return true;
    }
    const long double cycles=time/input.duration;
    if(!std::isfinite(cycles)||cycles>std::numeric_limits<std::int32_t>::max()){
        fail(error,"animation cycle count exceeds supported native range");return false;
    }
    const auto cycle=static_cast<std::int32_t>(cycles);
    float local=stored(time-static_cast<long double>(cycle)*input.duration);
    if(input.flags&4u)local=stored(static_cast<long double>(input.duration)-local);
    float start=0,span=0;
    if(!input.timestamps.empty()){
        // Native binary search keeps the lower key, including exact ties.
        std::uint32_t lo=0,hi=input.key_count;
        while(hi-lo>1){const auto mid=lo+(hi-lo)/2;
            if(input.timestamps[mid]>local)hi=mid;else lo=mid;
        }
        candidate.key=lo;start=input.timestamps[lo];
        span=stored(static_cast<long double>(lo+1==input.key_count?input.duration:input.timestamps[lo+1])-start);
    }else{
        // Preserve the engine's +0.05f selection bias. It is NOT the interval
        // denominator, so the final fraction can legitimately exceed one.
        const long double key=(static_cast<long double>(local)/(static_cast<long double>(input.duration)+0x1.99999ap-5f))*input.key_count;
        if(!std::isfinite(key)||key<0||key>=input.key_count){
            fail(error,"animation uniform key selection unsupported");return false;
        }
        candidate.key=static_cast<std::uint32_t>(key);
        span=stored(static_cast<long double>(input.duration)/input.key_count);
        start=stored(static_cast<long double>(candidate.key)*span);
    }
    if(!std::isfinite(local)||!std::isfinite(start)||!std::isfinite(span)||span<=0){
        fail(error,"animation phase interval is nonfinite or empty");return false;
    }
    rounding.restore();
    candidate.fraction=stored((static_cast<long double>(local)-start)/span);
    if(!std::isfinite(candidate.fraction)){fail(error,"animation phase result is nonfinite");return false;}
    output=candidate;error.clear();return true;
}
}
