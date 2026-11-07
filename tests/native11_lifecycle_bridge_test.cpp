#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

extern "C" {
void lifecycle_fixture_run_model(void*);
void lifecycle_fixture_run_texture(void*);
void lifecycle_fixture_model_original();
void lifecycle_fixture_texture_original();
std::uintptr_t a2fo_native11_model_destroy_original=
    reinterpret_cast<std::uintptr_t>(&lifecycle_fixture_model_original);
std::uintptr_t a2fo_native11_texture_destroy_original=
    reinterpret_cast<std::uintptr_t>(&lifecycle_fixture_texture_original);
std::uint16_t lifecycle_input_cw=0x037f;
std::uint32_t lifecycle_input_mxcsr=0x3f80,lifecycle_output_mxcsr=0x5f80;
std::uint32_t lifecycle_observer_calls=0,lifecycle_original_calls=0;
std::uint32_t lifecycle_observed_kind=0,lifecycle_observed_this=0;
std::uint32_t lifecycle_original_saw_observer=0,lifecycle_original_stack=0;
std::uint32_t lifecycle_stack_before=0,lifecycle_stack_after=0;
std::uint32_t lifecycle_flags_expected=0,lifecycle_flags_original=0;
std::uint32_t lifecycle_callback_flags=0;
std::uint32_t lifecycle_original_gpr[7]{},lifecycle_result_gpr[7]{};
alignas(16) unsigned char lifecycle_fx_expected[512]{};
alignas(16) unsigned char lifecycle_fx_original[512]{};
alignas(16) unsigned char lifecycle_fx_result[512]{};
alignas(16) unsigned char lifecycle_fx_callback[512]{};
alignas(16) unsigned char lifecycle_xmm_seed[128]{};
alignas(16) unsigned char lifecycle_xmm_output[16]{};
float lifecycle_fp_seed[8]={1.0f,2.0f,3.0f,4.0f,5.0f,6.0f,7.0f,8.0f};
float lifecycle_fp_model=11.25f,lifecycle_fp_texture=22.5f;
std::uint32_t lifecycle_callback_clobber_mxcsr=0x1f80;
}

namespace {
void need(bool yes,const char* why){
    if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}
}
void check_entry_fp(){
    need(!std::memcmp(lifecycle_fx_expected,lifecycle_fx_original,5),
         "original sees entry x87 control/status/tags");
    need(!std::memcmp(lifecycle_fx_expected+24,lifecycle_fx_original+24,4),
         "original sees entry MXCSR");
    for(unsigned slot=0;slot<8;++slot)
        need(!std::memcmp(lifecycle_fx_expected+32+slot*16,
                          lifecycle_fx_original+32+slot*16,10),
             "original sees all eight occupied x87 slots");
    need(!std::memcmp(lifecycle_fx_expected+160,lifecycle_fx_original+160,128),
         "original sees XMM0 through XMM7");
}
void check_result_fp(bool texture){
    const auto value=texture?lifecycle_fp_texture:lifecycle_fp_model;
    alignas(16) unsigned char expected[512]{};
    // Build the expected x87 image independently of the tap and fake original.
    // FXSAVE stores an 80-bit register value, not the four input float bytes.
    asm volatile("fninit; flds %1; fxsave %0; fninit"
        : "=m"(expected):"m"(value):"st","memory");
    need(!std::memcmp(lifecycle_fx_result,expected,5),
         "fake original x87 control/status and single occupied tag reach caller");
    const unsigned top=(expected[3]>>3)&7u;
    for(unsigned slot=0;slot<8;++slot)
        if(expected[4]&(1u<<((top+slot)&7u)))
            need(!std::memcmp(lifecycle_fx_result+32+slot*16,expected+32+slot*16,10),
                 "fake original 80-bit x87 result reaches caller");
    need(!std::memcmp(lifecycle_fx_result+24,&lifecycle_output_mxcsr,4)&&
         !std::memcmp(lifecycle_fx_result+160,lifecycle_xmm_output,16),
         "fake original SSE result reaches caller");
}
void one_case(bool texture,void* object,std::uint16_t cw){
    lifecycle_input_cw=cw;
    lifecycle_observer_calls=lifecycle_original_calls=0;
    lifecycle_observed_kind=lifecycle_observed_this=0;
    lifecycle_original_saw_observer=0;
    if(texture)lifecycle_fixture_run_texture(object);
    else lifecycle_fixture_run_model(object);
    need(lifecycle_observer_calls==1&&lifecycle_original_calls==1&&
         lifecycle_original_saw_observer==1,
         "observer precedes exactly one fake original");
    need(lifecycle_observed_kind==(texture?2u:1u)&&
         lifecycle_observed_this==reinterpret_cast<std::uintptr_t>(object),
         "observer gets exact kind and native this");
    need(lifecycle_stack_before==lifecycle_stack_after&&
         lifecycle_original_stack==lifecycle_stack_before-4,
         "tail jump and original ret preserve caller stack");
    const std::uint32_t expected_gpr[]{0x11111111,0x22222222,
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(object)),
        0x44444444,0x55555555,0x66666666,0x24681357};
    need(!std::memcmp(lifecycle_original_gpr,expected_gpr,sizeof(expected_gpr)),
         "all entry GPRs including ECX and EBP reach fake original");
    need((lifecycle_flags_original&0xcd5)==(lifecycle_flags_expected&0xcd5),
         "entry arithmetic and direction flags reach fake original");
    check_entry_fp();
    need(!(lifecycle_callback_flags&0x400)&&lifecycle_fx_callback[4]==0&&
         !std::memcmp(lifecycle_fx_callback+24,&lifecycle_callback_clobber_mxcsr,4),
         "observer begins with clean DF, x87 and MXCSR");
    const std::uint32_t expected_result[]{texture?0xb0b0b0b0u:0xa0a0a0a0u,
        0x22222222u,texture?0xd0d0d0d0u:0xc0c0c0c0u,
        0x44444444u,0x55555555u,0x66666666u,0x24681357u};
    need(!std::memcmp(lifecycle_result_gpr,expected_result,sizeof(expected_result)),
         "distinct fake-original GPR effects reach caller");
    check_result_fp(texture);
}
}

extern "C" void a2fo_native11_observe_destruction(std::uint32_t kind,void* object) noexcept {
    asm volatile("pushfl; popl %0; fxsave %1"
        : "=m"(lifecycle_callback_flags),"=m"(lifecycle_fx_callback));
    ++lifecycle_observer_calls;
    lifecycle_observed_kind=kind;
    lifecycle_observed_this=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(object));
    asm volatile(
        "fninit; fld1; fld1; fld1; fld1; fld1; fld1; fld1; fld1; "
        "pxor %%xmm0,%%xmm0; pxor %%xmm1,%%xmm1; pxor %%xmm2,%%xmm2; "
        "pxor %%xmm3,%%xmm3; pxor %%xmm4,%%xmm4; pxor %%xmm5,%%xmm5; "
        "pxor %%xmm6,%%xmm6; pxor %%xmm7,%%xmm7; "
        "ldmxcsr _lifecycle_input_mxcsr; std"
        ::: "cc","memory","st","xmm0","xmm1","xmm2","xmm3",
            "xmm4","xmm5","xmm6","xmm7");
}

int main(){
    unsigned char model[16]{},texture[16]{};
    for(unsigned i=0;i<128;++i)lifecycle_xmm_seed[i]=static_cast<unsigned char>(i+1);
    for(unsigned i=0;i<16;++i)lifecycle_xmm_output[i]=static_cast<unsigned char>(0xe0+i);
    std::uint16_t caller_cw=0;asm volatile("fnstcw %0":"=m"(caller_cw));
    for(unsigned pc:{0u,2u,3u})for(unsigned rc=0;rc<4;++rc){
        const auto cw=static_cast<std::uint16_t>((caller_cw&~0x0f00u)|
            (pc<<8)|(rc<<10)|0x003fu);
        one_case(false,model,cw);
        one_case(true,texture,cw);
    }
    asm volatile("cld; fninit; fldcw %0"::"m"(caller_cw):"memory");
    std::puts("PASS native11 lifecycle bridge: 24 PC/RC/kind cases, exact entry and fake-original effects");
}
