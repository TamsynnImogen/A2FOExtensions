// Operation order for Rust-owned animation workflows. The runner separately
// compares complete frozen/current Win32 mock transcripts.
#include "../rust/module_runtime_b3_ffi.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

struct Trace {std::vector<unsigned> ops;unsigned fail=99,exception=99,exception_arg=0;
    bool pair=false,emit_mode=false;uint64_t generation=1;};
extern "C" uint64_t primitive(void* opaque,uint32_t op,uint32_t arg) {
    auto& t=*static_cast<Trace*>(opaque);t.ops.push_back(op*100+arg);
    if(op==t.exception && arg==t.exception_arg)return UINT64_MAX;
    if(op==t.fail)return 0;
    if(t.emit_mode){if(op==1)return t.pair;if(op==3)return 0;return 1;}
    if(op==1 && arg==0)return 4; // helper count
    if(op==3 && arg==0)return 8; // hook count
    if(op==7 && arg==0)return 4; // class field chunks
    if(op==0 && arg==0)return t.generation;
    if(op==1 && arg!=0)return t.pair;
    if(op==3 && arg==0)return t.generation;
    return 1;
}
int main() {
    Trace t{};t.fail=2;
    assert(a2fo_rs_hardpoints_init(1,&t,primitive));
    assert((t.ops==std::vector<unsigned>{0,100,200,300,400,500,600}));
    t={};
    assert(a2fo_rs_hardpoints_query(1,&t,primitive)==8);
    assert((t.ops==std::vector<unsigned>{300}));
    t={};t.fail=4;
    assert(!a2fo_rs_animations_install(&t,primitive));
    assert((t.ops==std::vector<unsigned>{1600,100,200,201,202,203,300,400}));
    t={};t.exception=1;
    assert(a2fo_rs_animations_install(&t,primitive)==2);
    assert((t.ops==std::vector<unsigned>{1600,100}));
    t={};t.exception=7;
    assert(a2fo_rs_animations_init(1,&t,primitive));
    assert(t.ops.back()==1100);
    assert(std::count(t.ops.begin(),t.ops.end(),700)==1);
    t={};t.exception=8;
    assert(a2fo_rs_animations_init(1,&t,primitive));
    assert((t.ops.end()[-2]==800 && t.ops.back()==1100));
    t={};t.exception=8;t.exception_arg=1;
    assert(a2fo_rs_animations_init(1,&t,primitive));
    assert((t.ops.end()[-3]==800 && t.ops.end()[-2]==801 && t.ops.back()==1100));
    t={};t.fail=8;
    assert(a2fo_rs_animations_init(1,&t,primitive));
    assert((t.ops.end()[-5]==800 && t.ops.end()[-4]==801 &&
            t.ops.end()[-3]==802 && t.ops.end()[-2]==803 && t.ops.back()==1200));
    t={};t.exception=9;
    assert(a2fo_rs_animations_init(1,&t,primitive));
    assert((t.ops.end()[-2]==900 && t.ops.back()==1100));
    t={};t.emit_mode=true;t.pair=true;
    const char event[]="production_begin";
    a2fo_rs_animations_emit(reinterpret_cast<const uint8_t*>(event),sizeof(event)-1,&t,primitive);
    assert((t.ops==std::vector<unsigned>{3,101}));
    t={};t.emit_mode=true;
    const char cancel[]="production_cancel";
    a2fo_rs_animations_emit(reinterpret_cast<const uint8_t*>(cancel),sizeof(cancel)-1,&t,primitive);
    assert((t.ops==std::vector<unsigned>{2,101,300,200}));
    t={};t.emit_mode=true;
    const char repair[]="repair_begin";
    a2fo_rs_animations_emit(reinterpret_cast<const uint8_t*>(repair),sizeof(repair)-1,&t,primitive);
    assert((t.ops==std::vector<unsigned>{5,102,300,200}));
    t={};
    a2fo_rs_animations_craft_event(2,1,0,&t,primitive);
    assert((t.ops==std::vector<unsigned>{0}));
    t={};
    a2fo_rs_animations_craft_event(0,1,1,&t,primitive);
    assert((t.ops==std::vector<unsigned>{300,400}));
    t={};
    a2fo_rs_animations_craft_event(1,1,1,&t,primitive);
    assert((t.ops==std::vector<unsigned>{301,500}));
    t={};
    a2fo_rs_animations_producer_event(5,1,1,&t,primitive);
    assert((t.ops==std::vector<unsigned>{200}));
    t={};t.generation=2;
    a2fo_rs_animations_observer(&t,primitive);
    assert((t.ops==std::vector<unsigned>{0,100,200,300}));
    std::puts("stage7 runtime-b3 operation order: PASS");
}
