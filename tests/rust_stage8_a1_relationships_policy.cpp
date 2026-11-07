#include "../rust/stage8_a1_relationships_ffi.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>
struct Trace {
    std::vector<std::array<std::uint64_t,6>> calls;
    std::uint32_t fail_id=UINT32_MAX;
    std::uint64_t fail_a=UINT64_MAX;
    std::array<std::uint32_t,8> side{1,2,0,0,0,0,0,0};
    std::array<std::uint32_t,8> team{4,4,0,0,0,0,0,0};
    std::array<bool,9> present{true,true,true,false,false,false,false,false,false};
};
static std::uint64_t op(void* opaque,std::uint32_t id,std::uint64_t a,
    std::uint64_t b,std::uint64_t c,std::uint64_t d,std::uint64_t e){
    auto& t=*static_cast<Trace*>(opaque);t.calls.push_back({id,a,b,c,d,e});
    if(id==t.fail_id&&(t.fail_a==UINT64_MAX||t.fail_a==a))return UINT64_MAX;
    switch(id){
    case 0:return a<2?1:0;
    case 1:return t.side[a];
    case 2:return t.team[a];
    case 5:return t.present[a];
    case 6:return static_cast<std::uint32_t>(-1);
    default:return 1;
    }
}
static bool has(const Trace& t,std::uint32_t id,std::uint64_t a,
    std::uint64_t b=0,std::uint64_t c=0){
    for(const auto& row:t.calls)if(row[0]==id&&row[1]==a&&row[2]==b&&row[3]==c)return true;
    return false;
}
int main(){
    Trace equal{};
    if(a2fo_rs_stage8_a1_relationship_restore(&equal,op)!=1)return 1;
    if(!has(equal,15,0)||!has(equal,14,0)||!has(equal,6,1,2,2)||!has(equal,6,0,2,1)||!has(equal,7,1,2,0xffffffffu)
        ||!has(equal,8,0)||!has(equal,9,2,1))return 2;
    Trace changed{};changed.team[1]=5;
    if(a2fo_rs_stage8_a1_relationship_restore(&changed,op)!=1||!has(changed,6,1,2,0))return 3;
    Trace invalid{};invalid.side[1]=9;
    if(a2fo_rs_stage8_a1_relationship_restore(&invalid,op)!=1||has(invalid,7,1,2))return 4;
    Trace none{};none.fail_id=0;none.fail_a=0;
    if(a2fo_rs_stage8_a1_relationship_restore(&none,op)!=0||has(none,8,0))return 5;
    Trace stop{};stop.fail_id=6;stop.fail_a=1;
    if(a2fo_rs_stage8_a1_relationship_restore(&stop,op)!=0||has(stop,8,0))return 6;
    Trace before{};a2fo_rs_stage8_a1_relationships_before_elimination(&before,op);
    if(!has(before,10,0)||!has(before,11,0)||!has(before,12,0)||!has(before,13,0))return 7;
    Trace gate{};gate.fail_id=11;
    a2fo_rs_stage8_a1_relationships_before_elimination(&gate,op);
    if(has(gate,12,0)||has(gate,13,0))return 8;
    std::puts("A1 relationship policy: 8 paths PASS");
}
