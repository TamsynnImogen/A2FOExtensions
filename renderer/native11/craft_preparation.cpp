#include "craft_preparation.hpp"
#include "native_arithmetic.hpp"
#include <cmath>
namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* why) noexcept {try{error=why;}catch(...){}return false;}
}
bool prepare_craft_state(const CraftPreparationInput& in,std::uint32_t control,
    PreparedCraftState& output,std::string& error) noexcept {
    if(!control||!valid_native_x87_encoding(control)||!std::isfinite(in.hull_divisor)||in.hull_divisor<=0||
       (in.use_hull_ratio&&(!std::isfinite(in.hull_current)||in.hull_current<0||
        !std::isfinite(in.hull_maximum)||in.hull_maximum<=0)))
        return fail(error,"invalid Craft preparation inputs or arithmetic context");
    NativeArithmeticScope arithmetic(control);
    if(!arithmetic.valid()||!arithmetic.known())return fail(error,"unavailable Craft preparation arithmetic");
    PreparedCraftState pending;
    for(std::size_t i=0;i<in.system_state.size();++i)
        pending.systems_hidden[i]=in.system_state[i]!=0&&in.suppress_system_hiding==0;
    pending.borg_variant=in.race_matches_borg_cache;
    float numerator=1;
    if(in.use_hull_ratio){
        constexpr float one=1;
        std::uint16_t status=0;
        // Native keeps the quotient extended for the comparison but stores
        // binary32 before the later division by the virtual getter's result.
        asm volatile("flds %2; fdivs %3; fsts %0; fcomps %4; fnstsw %1"
            :"=m"(numerator),"=am"(status)
            :"m"(in.hull_current),"m"(in.hull_maximum),"m"(one):"st","memory");
        if(!(status&0x4100u))numerator=1;
    }
    asm volatile("flds %1; fdivrs %2; fstps %0"
        :"=m"(pending.hull_fraction):"m"(in.hull_divisor),"m"(numerator):"st","memory");
    if(!std::isfinite(pending.hull_fraction)||pending.hull_fraction<0)
        return fail(error,"unrepresentable Craft hull fraction");
    output=pending;error.clear();return true;
}
}
