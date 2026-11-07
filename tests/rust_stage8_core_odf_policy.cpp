#include "../rust/stage8_core_odf_ffi.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>
struct Trace {std::vector<std::uint32_t> ids;std::uint32_t fail=UINT32_MAX;
    std::uint32_t error=UINT32_MAX;std::uint64_t commands=2,fields=1,handlers=2;};
static std::uint64_t op(void* opaque,std::uint32_t id){
    auto& t=*static_cast<Trace*>(opaque);t.ids.push_back(id);
    if(id==t.error)return UINT64_MAX;
    if(id==0&&t.fail!=UINT32_MAX)return 0;
    if(id==t.fail)return 0;
    if(id==23)return t.commands;
    if(id==30)return t.fields;
    if(id==32)return t.handlers;
    if(id==0x10001||id==0x20000||id==0x30000||id==0x30001)return 0;
    return 1;
}
static bool check(bool okay,const char* label){
    if(!okay)std::fprintf(stderr,"core ODF policy failure: %s\n",label);
    return okay;
}
int main(){
    Trace string{};
    if(!check(a2fo_rs_stage8_core_odf_snapshot(&string,op)==1&&
        string.ids==std::vector<std::uint32_t>{0,1,2,3,4},
        "string getter precedes terminator, scan, assignment"))return 1;
    Trace invalid{};invalid.fail=3;
    if(!check(a2fo_rs_stage8_core_odf_snapshot(&invalid,op)==0&&invalid.ids.back()==3,
        "invalid bounded output stops before write"))return 1;
    Trace numeric{};numeric.fail=0;
    if(!check(a2fo_rs_stage8_core_odf_snapshot(&numeric,op)==1&&
        numeric.ids==std::vector<std::uint32_t>{0,1,2,3,5,6,7,8,9,10},
        "float format precedes integer fallback"))return 1;
    Trace integer{};integer.fail=6;
    if(!check(a2fo_rs_stage8_core_odf_snapshot(&integer,op)==1&&
        integer.ids==std::vector<std::uint32_t>{0,1,2,3,5,6,11,12,13},
        "failed float getter reaches integer"))return 1;
    Trace exception{};exception.error=4;
    if(!check(a2fo_rs_stage8_core_odf_snapshot(&exception,op)==0&&exception.ids.back()==4,
        "native assignment exception stops Rust workflow"))return 1;
    Trace empty{};empty.fail=22;
    a2fo_rs_stage8_core_odf_race(&empty,op);
    if(!check(empty.ids==std::vector<std::uint32_t>{21,22},
        "three collections copied before empty-handler gate"))return 1;
    Trace dispatched{};dispatched.fail=24;
    a2fo_rs_stage8_core_odf_race(&dispatched,op);
    if(!check(dispatched.ids.back()==34&&
        std::count(dispatched.ids.begin(),dispatched.ids.end(),24u)==2&&
        std::count(dispatched.ids.begin(),dispatched.ids.end(),27u)==2,
        "requested defaults and handler loops are ordered"))return 1;
    Trace stopped{};stopped.error=21;
    a2fo_rs_stage8_core_odf_race(&stopped,op);
    if(!check(stopped.ids==std::vector<std::uint32_t>{21},
        "lock-copy exception stops before handlers"))return 1;
    std::puts("core ODF policy: 8 branches PASS");
}
