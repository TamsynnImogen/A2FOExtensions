#include "../rust/stage8_a1_shutdown_ffi.h"
#include <cstdint>
#include <cstdio>
#include <vector>
struct Trace {std::vector<std::uint32_t> ids;std::uint32_t fail=UINT32_MAX;};
static std::uint64_t op(void* opaque,std::uint32_t id){
    auto& trace=*static_cast<Trace*>(opaque);trace.ids.push_back(id);
    return id==trace.fail?UINT64_MAX:0;
}
int main(){
    Trace complete{};a2fo_rs_stage8_a1_shutdown(&complete,op);
    if(complete.ids!=std::vector<std::uint32_t>{0,1,2,3,4,5,6,7,8,9,10,11,12,13})return 1;
    for(std::uint32_t fail:{0u,1u,4u,8u,9u,10u,13u}){
        Trace stopped{};stopped.fail=fail;
        a2fo_rs_stage8_a1_shutdown(&stopped,op);
        if(stopped.ids.size()!=fail+1||stopped.ids.back()!=fail)return 2;
    }
    std::puts("A1 shutdown policy: 8 ordered paths PASS");
}
