#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
std::uint32_t refit_fixture_call_tap(void*,void*,void*);
void refit_fixture_original();
std::uintptr_t a2fo_refit_workspace_ctor_original=
    reinterpret_cast<std::uintptr_t>(&refit_fixture_original);
alignas(16) unsigned char refit_fixture_fx_expected[512]{};
alignas(16) unsigned char refit_fixture_fx_actual[512]{};
alignas(16) unsigned char refit_fixture_xmm[128]{};
std::uint32_t refit_fixture_gpr[6]{};
std::uint32_t refit_fixture_flags_expected=0,refit_fixture_flags_actual=0;
std::uint32_t refit_fixture_arg1=0,refit_fixture_arg2=0;
std::uint32_t refit_fixture_calls=0,refit_fixture_observations=0;
std::uint32_t refit_fixture_observations_at_original=0;
std::uint32_t refit_fixture_stack_before=0,refit_fixture_stack_after=0;
std::uint32_t refit_fixture_observer_x87_status=0,refit_fixture_observer_mxcsr=0;
std::uint32_t refit_fixture_mxcsr_clobber=0x3f80;
}

extern "C" void a2fo_refit_prepare_sources() noexcept {
    __asm__ volatile("fnstsw %0\n\tstmxcsr %1"
        : "=m"(refit_fixture_observer_x87_status),"=m"(refit_fixture_observer_mxcsr));
    ++refit_fixture_observations;
    __asm__ volatile("movl $0xdeadbeef, %%eax\n\t"
                     "movl $0xdeadbeef, %%ecx\n\t"
                     "movl $0xdeadbeef, %%edx\n\t"
                     "stc\n\tfninit\n\tfld1\n\t"
                     "ldmxcsr %0\n\t"
                     "pxor %%xmm0, %%xmm0\n\tpxor %%xmm1, %%xmm1\n\t"
                     "pxor %%xmm2, %%xmm2\n\tpxor %%xmm3, %%xmm3\n\t"
                     "pxor %%xmm4, %%xmm4\n\tpxor %%xmm5, %%xmm5\n\t"
                     "pxor %%xmm6, %%xmm6\n\tpxor %%xmm7, %%xmm7"
        :: "m"(refit_fixture_mxcsr_clobber)
        : "eax","ecx","edx","cc","st","xmm0","xmm1","xmm2","xmm3",
            "xmm4","xmm5","xmm6","xmm7");
}

static void need(bool condition,const char* message) {
    if(!condition){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}
}
int main() {
    alignas(16) std::uint32_t object=0x13572468,arg1=0x24681357,arg2=0xabcdef01;
    for(unsigned i=0;i<sizeof(refit_fixture_xmm);++i)
        refit_fixture_xmm[i]=static_cast<unsigned char>(i+1);
    const auto result=refit_fixture_call_tap(&object,&arg1,&arg2);
    need(result==0xaabbccdd,"original return EAX survives tail dispatch");
    need(refit_fixture_calls==1&&refit_fixture_observations==1,
         "one callback before one original call");
    need(refit_fixture_observations_at_original==1,"callback precedes original call");
    need(refit_fixture_stack_before==refit_fixture_stack_after,"original ret8 balances stack");
    need(refit_fixture_arg1==reinterpret_cast<std::uintptr_t>(&arg1)&&
         refit_fixture_arg2==reinterpret_cast<std::uintptr_t>(&arg2),
         "original receives both exact stack arguments");
    need(refit_fixture_gpr[0]==0x11223344&&refit_fixture_gpr[1]==0x55667788&&
         refit_fixture_gpr[2]==reinterpret_cast<std::uintptr_t>(&object)&&
         refit_fixture_gpr[3]==0x12345678&&refit_fixture_gpr[4]==0x89abcdef&&
         refit_fixture_gpr[5]==0x76543210,"original receives exact entry GPRs");
    need(refit_fixture_flags_expected==refit_fixture_flags_actual,
         "original receives exact entry flags");
    need(refit_fixture_observer_x87_status==0&&refit_fixture_observer_mxcsr==0x1f80,
         "callback receives clean x87 and MXCSR");
    need(!std::memcmp(refit_fixture_fx_expected,refit_fixture_fx_actual,5),
         "original receives exact x87 control, status and tag");
    need(!std::memcmp(refit_fixture_fx_expected+24,refit_fixture_fx_actual+24,4),
         "original receives exact MXCSR");
    for(unsigned slot=0;slot<8;++slot)
        need(!std::memcmp(refit_fixture_fx_expected+32+slot*16,
                          refit_fixture_fx_actual+32+slot*16,10),
             "original receives all eight occupied x87 registers");
    need(!std::memcmp(refit_fixture_fx_expected+160,refit_fixture_fx_actual+160,128),
         "original receives all eight XMM registers");
    std::puts("PASS refit workspace bridge: callback isolation, exact original entry, ret8 balance");
}
