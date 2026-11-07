#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static_assert(sizeof(void*)==4);

extern "C" {
std::uint32_t refit_mesh_fixture_call(std::uint32_t,void*,void*,void*);
void refit_mesh_fixture_fast_original();
void refit_mesh_fixture_legacy_original();
std::uintptr_t a2fo_refit_mesh_fast_original=
    reinterpret_cast<std::uintptr_t>(&refit_mesh_fixture_fast_original);
std::uintptr_t a2fo_refit_mesh_legacy_original=
    reinterpret_cast<std::uintptr_t>(&refit_mesh_fixture_legacy_original);
std::uintptr_t refit_mesh_fixture_target=0;
alignas(16) unsigned char refit_mesh_fixture_fx_expected[512]{};
alignas(16) unsigned char refit_mesh_fixture_fx_actual[512]{};
alignas(16) unsigned char refit_mesh_fixture_fx_observer[512]{};
alignas(16) unsigned char refit_mesh_fixture_fx_post_observer[512]{};
alignas(16) unsigned char refit_mesh_fixture_fx_return_expected[512]{};
alignas(16) unsigned char refit_mesh_fixture_fx_return_actual[512]{};
alignas(16) unsigned char refit_mesh_fixture_xmm[128]{};
std::uint32_t refit_mesh_fixture_gpr_entry[6]{},refit_mesh_fixture_gpr_return[6]{};
std::uint32_t refit_mesh_fixture_ebp_expected=0,refit_mesh_fixture_ebp_entry=0,
              refit_mesh_fixture_ebp_return=0;
std::uint32_t refit_mesh_fixture_flags_expected=0,refit_mesh_fixture_flags_actual=0;
std::uint32_t refit_mesh_fixture_flags_return_expected=0,
              refit_mesh_fixture_flags_return_actual=0;
std::uint32_t refit_mesh_fixture_stack_before=0,refit_mesh_fixture_stack_after=0;
std::uint32_t refit_mesh_fixture_pass_actual=0,refit_mesh_fixture_material_actual=0;
std::uint32_t refit_mesh_fixture_original_calls=0,refit_mesh_fixture_original_route=99;
std::uint32_t refit_mesh_fixture_observer_calls=0,refit_mesh_fixture_observer_route=99;
std::uint32_t refit_mesh_fixture_post_calls=0,refit_mesh_fixture_post_route=99;
std::uint32_t refit_mesh_fixture_events=0,refit_mesh_fixture_pre_event=0,
              refit_mesh_fixture_original_event=0,refit_mesh_fixture_post_event=0;
std::uint32_t refit_mesh_fixture_original_calls_at_post=0;
std::uintptr_t refit_mesh_fixture_observer_mesh=0,refit_mesh_fixture_observer_pass=0,
               refit_mesh_fixture_observer_material=0;
std::uint32_t refit_mesh_fixture_observations_at_original=0;
std::uint32_t refit_mesh_fixture_mxcsr_clobber=0x5f80;
}

extern "C" void refit_mesh_fixture_observer_body(void* mesh,void* pass,void* material,
                                                  std::uint32_t route) noexcept {
    refit_mesh_fixture_observer_mesh=reinterpret_cast<std::uintptr_t>(mesh);
    refit_mesh_fixture_observer_pass=reinterpret_cast<std::uintptr_t>(pass);
    refit_mesh_fixture_observer_material=reinterpret_cast<std::uintptr_t>(material);
    refit_mesh_fixture_observer_route=route;
    ++refit_mesh_fixture_observer_calls;
    refit_mesh_fixture_pre_event=++refit_mesh_fixture_events;
    __asm__ volatile("movl $0xdeadbeef, %%eax\n\t"
                     "movl $0xdeadbeef, %%ecx\n\t"
                     "movl $0xdeadbeef, %%edx\n\t"
                     "stc\n\tfninit\n\tfld1\n\t"
                     "ldmxcsr %0\n\t"
                     "pxor %%xmm0, %%xmm0\n\tpxor %%xmm1, %%xmm1\n\t"
                     "pxor %%xmm2, %%xmm2\n\tpxor %%xmm3, %%xmm3\n\t"
                     "pxor %%xmm4, %%xmm4\n\tpxor %%xmm5, %%xmm5\n\t"
                     "pxor %%xmm6, %%xmm6\n\tpxor %%xmm7, %%xmm7"
        :: "m"(refit_mesh_fixture_mxcsr_clobber)
        : "eax","ecx","edx","cc","st","xmm0","xmm1","xmm2","xmm3",
          "xmm4","xmm5","xmm6","xmm7");
}

extern "C" void refit_mesh_fixture_post_body(std::uint32_t route) noexcept {
    refit_mesh_fixture_post_route=route;
    ++refit_mesh_fixture_post_calls;
    refit_mesh_fixture_original_calls_at_post=refit_mesh_fixture_original_calls;
    refit_mesh_fixture_post_event=++refit_mesh_fixture_events;
    __asm__ volatile("movl $0xdeadbeef, %%eax\n\t"
                     "movl $0xdeadbeef, %%ecx\n\t"
                     "movl $0xdeadbeef, %%edx\n\t"
                     "stc\n\tfninit\n\tfld1\n\t"
                     "ldmxcsr %0\n\t"
                     "pxor %%xmm0, %%xmm0\n\tpxor %%xmm1, %%xmm1\n\t"
                     "pxor %%xmm2, %%xmm2\n\tpxor %%xmm3, %%xmm3\n\t"
                     "pxor %%xmm4, %%xmm4\n\tpxor %%xmm5, %%xmm5\n\t"
                     "pxor %%xmm6, %%xmm6\n\tpxor %%xmm7, %%xmm7"
        :: "m"(refit_mesh_fixture_mxcsr_clobber)
        : "eax","ecx","edx","cc","st","xmm0","xmm1","xmm2","xmm3",
          "xmm4","xmm5","xmm6","xmm7");
}

namespace {
void need(bool condition,const char* message){
    if(!condition){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
void same_fx(const unsigned char* expected,const unsigned char* actual,const char* label){
    need(!std::memcmp(expected,actual,5),label);
    need(!std::memcmp(expected+24,actual+24,4),label);
    for(unsigned slot=0;slot<8;++slot)
        need(!std::memcmp(expected+32+slot*16,actual+32+slot*16,10),label);
    need(!std::memcmp(expected+160,actual+160,128),label);
}
void check_route(std::uint32_t route){
    alignas(16) std::uint32_t mesh=0x13572468+route;
    alignas(16) std::uint32_t pass=0x24681357+route;
    alignas(16) std::uint32_t material=0xabcdef01+route;
    for(unsigned i=0;i<sizeof(refit_mesh_fixture_xmm);++i)
        refit_mesh_fixture_xmm[i]=static_cast<unsigned char>(i+1+route);
    refit_mesh_fixture_observer_calls=0;
    refit_mesh_fixture_original_calls=0;
    refit_mesh_fixture_post_calls=0;
    refit_mesh_fixture_events=0;
    const auto result=refit_mesh_fixture_call(route,&mesh,&pass,&material);
    need(result==0xaabbccdd,"native original return EAX survives tail dispatch");
    need(refit_mesh_fixture_stack_before==refit_mesh_fixture_stack_after,
         "native ret8 balances caller stack");
    need(refit_mesh_fixture_observer_calls==1&&refit_mesh_fixture_original_calls==1&&
         refit_mesh_fixture_post_calls==1&&refit_mesh_fixture_observer_route==route&&
         refit_mesh_fixture_original_route==route&&refit_mesh_fixture_post_route==route,
         "one route-specific PRE, original and POST call");
    need(refit_mesh_fixture_events==3&&refit_mesh_fixture_pre_event==1&&
         refit_mesh_fixture_original_event==2&&refit_mesh_fixture_post_event==3&&
         refit_mesh_fixture_original_calls_at_post==1,
         "PRE, original and POST execute in exact order");
    need(refit_mesh_fixture_observations_at_original==1,
         "observer completed before original entry");
    need(refit_mesh_fixture_observer_mesh==reinterpret_cast<std::uintptr_t>(&mesh)&&
         refit_mesh_fixture_observer_pass==reinterpret_cast<std::uintptr_t>(&pass)&&
         refit_mesh_fixture_observer_material==reinterpret_cast<std::uintptr_t>(&material),
         "observer receives exact thiscall arguments and route");
    need(refit_mesh_fixture_pass_actual==reinterpret_cast<std::uintptr_t>(&pass)&&
         refit_mesh_fixture_material_actual==reinterpret_cast<std::uintptr_t>(&material),
         "original receives exact pass and material stack arguments");
    need(refit_mesh_fixture_gpr_entry[0]==0x11223344&&
         refit_mesh_fixture_gpr_entry[1]==0x55667788&&
         refit_mesh_fixture_gpr_entry[2]==reinterpret_cast<std::uintptr_t>(&mesh)&&
         refit_mesh_fixture_gpr_entry[3]==0x12345678&&
         refit_mesh_fixture_gpr_entry[4]==0x89abcdef&&
         refit_mesh_fixture_gpr_entry[5]==0x76543210&&
         refit_mesh_fixture_ebp_entry==refit_mesh_fixture_ebp_expected,
         "original receives exact entry GPRs and EBP");
    need(refit_mesh_fixture_flags_actual==refit_mesh_fixture_flags_expected,
         "original entry EFLAGS restored after observer");
    const unsigned char clean_x87[]={0x7f,0x03,0,0,0};
    need(!std::memcmp(refit_mesh_fixture_fx_observer,clean_x87,sizeof(clean_x87))&&
         *reinterpret_cast<const std::uint32_t*>(refit_mesh_fixture_fx_observer+24)==0x1f80,
         "observer receives initialized empty x87 and default MXCSR");
    need(!std::memcmp(refit_mesh_fixture_fx_post_observer,clean_x87,sizeof(clean_x87))&&
         *reinterpret_cast<const std::uint32_t*>(refit_mesh_fixture_fx_post_observer+24)==0x1f80,
         "POST receives initialized empty x87 and default MXCSR");
    need(!std::memcmp(refit_mesh_fixture_fx_observer+160,
                      refit_mesh_fixture_fx_expected+160,128),
         "observer retains caller XMM values until its own work");
    same_fx(refit_mesh_fixture_fx_expected,refit_mesh_fixture_fx_actual,
            "original entry x87, MXCSR and all XMM restored");
    const std::uint32_t expected_return[6]={0xaabbccdd,0x13579bdf,0x2468ace0,
                                             0xfedcba98,0x55aa55aa,0xaa55aa55};
    need(!std::memcmp(refit_mesh_fixture_gpr_return,expected_return,sizeof(expected_return))&&
         refit_mesh_fixture_ebp_return==refit_mesh_fixture_ebp_expected,
         "original return GPRs and nonvolatile EBP reach caller");
    need(refit_mesh_fixture_flags_return_actual==refit_mesh_fixture_flags_return_expected,
         "original return EFLAGS reach caller");
    same_fx(refit_mesh_fixture_fx_return_expected,refit_mesh_fixture_fx_return_actual,
            "original return x87, MXCSR and XMM reach caller");
}
}
int main(){
    check_route(0);
    check_route(1);
    std::puts("PASS: fast and legacy mesh taps preserve PRE, original, POST ordering and exact return state");
}
