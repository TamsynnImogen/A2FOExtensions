#include "../renderer/native11/craft_preparation.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

using namespace a2fo::native11;
namespace {
void need(bool okay,const char* message){
    if(!okay){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}
}
std::uint16_t control(){std::uint16_t value=0;asm volatile("fnstcw %0":"=m"(value));return value;}
void control(std::uint16_t value){asm volatile("fldcw %0"::"m"(value):"memory");}
PreparedCraftState marker(){
    PreparedCraftState value;value.systems_hidden={{true,false,true,false,true}};
    value.borg_variant=true;value.hull_fraction=123.f;return value;
}
void rejected(const CraftPreparationInput& input,std::uint32_t cw,const char* message){
    auto output=marker();const auto before=output;std::string error;
    need(!prepare_craft_state(input,cw,output,error)&&!error.empty()&&
         output.systems_hidden==before.systems_hidden&&
         output.borg_variant==before.borg_variant&&
         output.hull_fraction==before.hull_fraction,message);
}
PreparedCraftState accepted(const CraftPreparationInput& input,std::uint32_t cw){
    PreparedCraftState output;std::string error="stale";
    need(prepare_craft_state(input,cw,output,error)&&error.empty(),
         "valid Craft preparation accepted and stale error cleared");
    return output;
}
}
int main(){
    const auto source_cw=encode_native_x87(0x037f);
    CraftPreparationInput input;
    input.system_state={{0,1,2,255,0}};input.race_matches_borg_cache=true;
    input.hull_divisor=2.f;
    auto result=accepted(input,source_cw);
    need(result.systems_hidden==std::array<bool,5>{{false,true,true,true,false}}&&
         result.borg_variant&&result.hull_fraction==.5f,
         "all native nonzero system bytes normalize to true and Borg flag is copied");
    input.suppress_system_hiding=255;
    result=accepted(input,source_cw);
    need(result.systems_hidden==std::array<bool,5>{{false,false,false,false,false}}&&
         result.borg_variant,"nonzero suppression clears every hidden-system flag");
    input.suppress_system_hiding=0;input.race_matches_borg_cache=false;
    result=accepted(input,source_cw);
    need(!result.borg_variant&&result.systems_hidden[3],
         "race mismatch clears Borg variant independently of system state");

    input.use_hull_ratio=true;input.hull_maximum=100.f;
    for(const auto& item:std::array<std::array<float,2>,4>{{
            {{0.f,0.f}},{{50.f,.25f}},{{100.f,.5f}},{{150.f,.5f}}}}){
        input.hull_current=item[0];
        result=accepted(input,source_cw);
        need(result.hull_fraction==item[1],
             "hull ratio zero, half, one and above-one clamp before divisor");
    }
    input.use_hull_ratio=false;input.hull_current=std::numeric_limits<float>::quiet_NaN();
    input.hull_maximum=-1.f;input.hull_divisor=4.f;
    result=accepted(input,source_cw);
    need(result.hull_fraction==.25f,
         "disabled hull ratio ignores unused health values but applies valid divisor");

    rejected(input,0,"unknown x87 source rejected atomically");
    rejected(input,encode_native_x87(0x017f),"reserved x87 precision rejected atomically");
    rejected(input,0x20000u,"malformed encoded x87 source rejected atomically");
    input.use_hull_ratio=true;input.hull_maximum=100.f;input.hull_current=-1.f;
    rejected(input,source_cw,"negative current hull rejected atomically");
    input.hull_current=1.f;input.hull_maximum=0.f;
    rejected(input,source_cw,"zero maximum hull rejected atomically");
    input.hull_maximum=std::numeric_limits<float>::infinity();
    rejected(input,source_cw,"nonfinite maximum hull rejected atomically");
    input.hull_maximum=100.f;input.hull_current=std::numeric_limits<float>::quiet_NaN();
    rejected(input,source_cw,"nonfinite current hull rejected atomically");
    input.hull_current=1.f;input.hull_divisor=0.f;
    rejected(input,source_cw,"zero divisor rejected atomically");
    input.hull_divisor=-1.f;rejected(input,source_cw,"negative divisor rejected atomically");
    input.hull_divisor=std::numeric_limits<float>::infinity();
    rejected(input,source_cw,"nonfinite divisor rejected atomically");
    input.hull_divisor=std::numeric_limits<float>::quiet_NaN();
    rejected(input,source_cw,"NaN divisor rejected atomically");
    input.use_hull_ratio=false;input.hull_divisor=std::numeric_limits<float>::denorm_min();
    rejected(input,source_cw,"unrepresentable final hull division preserves output");

    const auto saved=control();
    CraftPreparationInput simple;simple.hull_divisor=2.f;
    for(unsigned pc:{0u,2u,3u})for(unsigned rounding=0;rounding<4;++rounding){
        const auto caller=static_cast<std::uint16_t>((saved&~0x0f00u)|
            (pc<<8)|(rounding<<10)|0x003fu);
        control(caller);
        result=accepted(simple,source_cw);
        need(result.hull_fraction==.5f&&control()==caller,
             "successful preparation restores caller CW across PC and RC");
        simple.hull_divisor=std::numeric_limits<float>::denorm_min();
        rejected(simple,source_cw,"failing preparation preserves caller CW case");
        need(control()==caller,"failed preparation restores caller CW across PC and RC");
        simple.hull_divisor=2.f;
    }
    control(saved);
    std::puts("PASS native11 Craft preparation CPU policy");
}
