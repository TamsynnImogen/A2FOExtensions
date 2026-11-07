#include <cstdint>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
void node_fixture_run(unsigned branch, void* instance, void* database);
void a2fo_native11_node_branch_a();
void a2fo_native11_node_branch_b();
extern std::uint32_t node_fixture_native_frame[3];
alignas(16) unsigned char node_fixture_fx_expected[512]{};
alignas(16) unsigned char node_fixture_fx_actual[512]{};
alignas(16) unsigned char node_fixture_xmm[128]{};
std::uint32_t node_fixture_gpr_actual[6]{};
std::uint32_t node_fixture_flags_expected=0,node_fixture_flags_actual=0;
std::uint32_t node_fixture_stack_before=0,node_fixture_stack_after=0;
std::uint32_t node_fixture_ebp_actual=0;
std::uint32_t node_fixture_observer_x87_status=0,node_fixture_observer_mxcsr=0;
std::uintptr_t node_fixture_expected_instance[2]{};
std::uintptr_t node_fixture_expected_database=0;
std::uint32_t node_fixture_observed[2][4]{};
std::uint32_t node_fixture_observations[2]{};
}

namespace {
void need(bool value,const char* message){
    if(!value){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}
}
void check_fp(const char* label){
    need(!std::memcmp(node_fixture_fx_expected,node_fixture_fx_actual,5),label);
    need(!std::memcmp(node_fixture_fx_expected+24,node_fixture_fx_actual+24,4),label);
    for(unsigned slot=0;slot<8;++slot)
        need(!std::memcmp(node_fixture_fx_expected+32+slot*16,
                          node_fixture_fx_actual+32+slot*16,10),label);
    need(!std::memcmp(node_fixture_fx_expected+160,node_fixture_fx_actual+160,128),label);
}
}

extern "C" void a2fo_native11_observe_nodes(void* instance,void* camera,
                                               void* database,unsigned native_x87_control) noexcept {
    __asm__ volatile("fnstsw %0\n\tstmxcsr %1"
        : "=m"(node_fixture_observer_x87_status),
          "=m"(node_fixture_observer_mxcsr));
    unsigned slot=instance==reinterpret_cast<void*>(node_fixture_expected_instance[0])?0u:1u;
    node_fixture_observed[slot][0]=static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(instance));
    node_fixture_observed[slot][1]=static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(camera));
    node_fixture_observed[slot][2]=static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(database));
    node_fixture_observed[slot][3]=native_x87_control;
    ++node_fixture_observations[slot];
    __asm__ volatile("movl $0xdeadbeef, %%eax\n\t"
                     "movl $0xdeadbeef, %%ecx\n\t"
                     "movl $0xdeadbeef, %%edx\n\t"
                     "stc\n\tfninit\n\t"
                     "fld1\n\t fld1\n\t fld1\n\t fld1\n\t"
                     "fld1\n\t fld1\n\t fld1\n\t fld1\n\t"
                     "pxor %%xmm0,%%xmm0\n\tpxor %%xmm1,%%xmm1\n\t"
                     "pxor %%xmm2,%%xmm2\n\tpxor %%xmm3,%%xmm3\n\t"
                     "pxor %%xmm4,%%xmm4\n\tpxor %%xmm5,%%xmm5\n\t"
                     "pxor %%xmm6,%%xmm6\n\tpxor %%xmm7,%%xmm7"
        ::: "eax","ecx","edx","cc","st","xmm0","xmm1","xmm2","xmm3",
            "xmm4","xmm5","xmm6","xmm7");
}

int main(){
    alignas(16) unsigned char instance_a[0x80]{},instance_b[0x80]{};
    std::uint32_t database[]{0xdecafbad};
    node_fixture_expected_instance[0]=reinterpret_cast<std::uintptr_t>(instance_a);
    node_fixture_expected_instance[1]=reinterpret_cast<std::uintptr_t>(instance_b);
    node_fixture_expected_database=reinterpret_cast<std::uintptr_t>(database);
    for(unsigned reg=0;reg<8;++reg)
        for(unsigned byte=0;byte<16;++byte)
            node_fixture_xmm[reg*16+byte]=static_cast<unsigned char>(reg*16+byte+1);

    unsigned short saved_cw=0;asm volatile("fnstcw %0":"=m"(saved_cw));
    for(unsigned pc:{0u,2u,3u})for(unsigned rounding=0;rounding<4;++rounding){
    const unsigned short cw=static_cast<unsigned short>((saved_cw&~0x0f00u)|(pc<<8)|(rounding<<10));
    asm volatile("fldcw %0"::"m"(cw):"memory");
    node_fixture_observations[0]=node_fixture_observations[1]=0;
    node_fixture_run(0,instance_a,database);
    need(node_fixture_observations[0]==1&&node_fixture_observations[1]==0,
         "branch A invokes observer exactly once");
    need(node_fixture_observed[0][0]==node_fixture_expected_instance[0]&&
         node_fixture_observed[0][1]==0x24681357u&&
         node_fixture_observed[0][2]==node_fixture_expected_database,
         "branch A observer receives exact instance, EBP+8 camera, and database");
    need(node_fixture_observer_x87_status==0&&node_fixture_observer_mxcsr==0x1f80,
         "observer starts with initialized default x87 and SSE state");
    need(node_fixture_observed[0][3]==(0x10000u|cw),"branch A transports original native CW from FX save");
    const std::uint32_t expected_a[]{0x11223344,0x55667788,
        static_cast<std::uint32_t>(node_fixture_expected_database),0xdecafbad,
        static_cast<std::uint32_t>(node_fixture_expected_instance[0]+0x44),0x76543210};
    need(!std::memcmp(node_fixture_gpr_actual,expected_a,sizeof(expected_a)),
         "branch A replays ADD and MOV EDX while preserving other GPRs");
    need(node_fixture_ebp_actual==reinterpret_cast<std::uintptr_t>(node_fixture_native_frame),
         "branch A preserves native EBP frame");
    need(node_fixture_stack_before==node_fixture_stack_after,
         "branch A RET leaves caller stack balanced");
    need((node_fixture_flags_actual&0x8d5)==(node_fixture_flags_expected&0x8d5),
         "branch A flags equal displaced ADD result");
    check_fp("branch A restores all eight occupied x87 registers and XMM0-XMM7");

    asm volatile("fldcw %0"::"m"(cw):"memory");
    node_fixture_run(1,instance_b,database);
    need(node_fixture_observations[1]==1&&node_fixture_observations[0]==1,
         "branch B invokes observer once without repeating branch A");
    need(node_fixture_observed[1][0]==node_fixture_expected_instance[1]&&
         node_fixture_observed[1][1]==0x24681357u&&
         node_fixture_observed[1][2]==node_fixture_expected_database,
         "branch B observer receives exact instance, camera, and database");
    need(node_fixture_observed[1][3]==(0x10000u|cw),"branch B transports original native CW from FX save");
    const std::uint32_t expected_b[]{0xdecafbad,0x55667788,
        static_cast<std::uint32_t>(node_fixture_expected_database),0x12345678,
        static_cast<std::uint32_t>(node_fixture_expected_instance[1]+0x44),0x76543210};
    need(!std::memcmp(node_fixture_gpr_actual,expected_b,sizeof(expected_b)),
         "branch B replays ADD and MOV EAX while preserving other GPRs");
    need(node_fixture_ebp_actual==reinterpret_cast<std::uintptr_t>(node_fixture_native_frame),
         "branch B preserves native EBP frame");
    need(node_fixture_stack_before==node_fixture_stack_after,
         "branch B RET leaves caller stack balanced");
    need((node_fixture_flags_actual&0x8d5)==(node_fixture_flags_expected&0x8d5),
         "branch B flags equal displaced ADD result");
    check_fp("branch B restores all eight occupied x87 registers and XMM0-XMM7");
    }
    asm volatile("fldcw %0"::"m"(saved_cw):"memory");
    std::puts("PASS native11 node bridge: 12 FP modes, original CW, both taps, exact observer args, displaced instructions, GPR/flags/full-x87/SSE restore");
}
