// Isolated operation-order checks. Full frozen/current Win32 mocks run in the
// stage-seven b2 parity runner after this test.
#include "../rust/module_runtime_b2_ffi.h"
#include <cassert>
#include <cstdio>
#include <vector>

struct Trace {
    std::vector<unsigned> calls;
    unsigned fail=99;
    bool a1=false;
    bool native=false;
};
extern "C" uint64_t operation(void* opaque,uint32_t op) {
    auto& t=*static_cast<Trace*>(opaque);t.calls.push_back(op);
    if(op==t.fail)return 0;
    if(t.a1 && (op==3||op==10))return 0;
    if(!t.a1 && op==1)return t.native;
    if(op==0)return 0x1234;
    if(op==6)return 0x5678;
    if(op==8)return 0x9abc;
    return 1;
}
int main() {
    Trace a1{};a1.a1=true;
    assert(a2fo_rs_a1_dispatch(&a1,operation)==0x9abc);
    assert((a1.calls==std::vector<unsigned>{0,1,2,3,4,5,10,6,10,8}));
    a1={};a1.a1=true;a1.fail=6;
    assert(a2fo_rs_a1_dispatch(&a1,operation)==1);
    assert((a1.calls==std::vector<unsigned>{0,1,2,3,4,5,10,6,10,7,10,9}));
    Trace station{};station.native=true;
    assert(a2fo_rs_station_init(1,&station,operation));
    assert((station.calls==std::vector<unsigned>{0,1,11})); // native Roots route
    station={};station.fail=1;
    assert(a2fo_rs_station_init(1,&station,operation));
    assert((station.calls==std::vector<unsigned>{0,1,2,3,4,5,6,7,8,9,10,13}));
    station={};station.fail=5;
    assert(a2fo_rs_station_init(1,&station,operation));
    assert((station.calls==std::vector<unsigned>{0,1,2,3,4,5,15,14}));
    station={};
    assert(a2fo_rs_station_queue(0x19,1,1,1,1,2)==0xb2);
    assert(a2fo_rs_station_queue(0x19,1,0,1,1,2)==0x19);
    assert(a2fo_rs_station_broadcast(0x19,&station,operation));
    assert((station.calls==std::vector<unsigned>{0,4,1,2,3}));
    std::puts("stage7 runtime-b2 operation order: PASS");
}
