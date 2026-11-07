#include "../rust/stage8_b_refit_ffi.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <vector>

namespace {
struct Mock {
    std::array<std::uint64_t,64> values{};
    std::vector<std::uint32_t> calls;
};
std::uint64_t op(void* opaque,std::uint32_t id) {
    auto& mock=*static_cast<Mock*>(opaque);
    mock.calls.push_back(id);
    return mock.values[id];
}
void expect(const Mock& mock,std::initializer_list<std::uint32_t> calls,const char* label) {
    if(mock.calls!=std::vector<std::uint32_t>(calls)) {
        std::fprintf(stderr,"FAIL %s:",label);
        for(auto id:mock.calls)std::fprintf(stderr," %u",id);
        std::fprintf(stderr,"\n");std::exit(1);
    }
}
}
int main() {
    Mock m{};
    m.values.fill(1);
    m.values[1]=0;m.values[2]=0;m.values[7]=0;m.values[8]=0;m.values[11]=0;
    if(a2fo_rs_stage8_b_refit_init(&m,op)!=1)return 1;
    expect(m,{0,1,2,3,4,6,7,8,9,11,12,14},"both messages registered before failure gate");

    m={};m.values.fill(1);m.values[3]=0;
    if(a2fo_rs_stage8_b_refit_present_gate(&m,op)!=0)return 1;
    expect(m,{0,1,2,3},"present gate short circuit");

    m={};m.values.fill(0);
    a2fo_rs_stage8_b_refit_dispatch(0x0082,0xc100,0xc101,&m,op);
    expect(m,{0,1},"destroy before subclass forwarding");

    m={};m.values.fill(0);m.values[0]=1;m.values[1]=0;m.values[7]=1;
    if(a2fo_rs_stage8_b_refit_timer(&m,op)!=1)return 1;
    expect(m,{0,1,2,4,5,6,7,9,10},"timer fallback before native action");

    m={};m.values.fill(0);m.values[11]=1;m.values[12]=1;m.values[7]=1;
    m.values[13]=2;m.values[14]=1;m.values[16]=1;
    a2fo_rs_stage8_b_refit_action(&m,op);
    expect(m,{11,12,7,13,14,15,16},"IA action deferred after handover");

    m={};m.values.fill(0);m.values[2]=1;
    a2fo_rs_stage8_b_refit_mouse(1,&m,op);
    expect(m,{0,1,2,3,4},"scene press supersedes menu hit test");

    m={};m.values.fill(0);m.values[15]=1;
    if(a2fo_rs_stage8_b_refit_key(0x09,0,&m,op)!=1)return 1;
    expect(m,{15,16,4},"shift tab moves to prior enabled row");

    m={};m.values.fill(0);
    a2fo_rs_stage8_b_refit_shutdown(&m,op);
    expect(m,{0,1,2,3,4,5,6,8},"shutdown reads active window after subsystems");

    m={};m.values.fill(1);m.values[1]=0;m.values[7]=0;m.values[8]=0;
    m.values[15]=UINT64_MAX;
    a2fo_rs_stage8_b_refit_init(&m,op);
    expect(m,{0,1,2,3,4,6,7,8,9,11,12,13,15},"native install exception stops callbacks");

    m={};m.values.fill(0);m.values[0]=1;m.values[1]=1;
    a2fo_rs_stage8_b_refit_paint(&m,op);
    expect(m,{0,1,2},"handover paint ends native DC");
    std::puts("refit policy: 10 ordered branches PASS");
}
