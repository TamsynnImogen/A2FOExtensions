#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
extern "C" {
void asset_fixture_original();
void asset_fixture_run();
std::uintptr_t a2fo_native11_read_database_original=reinterpret_cast<std::uintptr_t>(&asset_fixture_original);
std::uint32_t asset_stack_before=0,asset_stack_after=0,asset_original_calls=0;
std::uint32_t asset_input[3]{},asset_gpr_actual[6]{};
std::uint32_t asset_flags_expected=0,asset_flags_actual=0;
unsigned short asset_input_cw=0x037f,asset_original_cw=0,asset_result_cw=0;
alignas(16) unsigned char asset_fx_expected[512]{},asset_fx_actual[512]{},asset_callback_fx[512]{};
alignas(16) unsigned char asset_xmm[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
}
namespace {
unsigned observations=0;
std::uint32_t observed[5]{},callback_flags=0;
void need(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
}
extern "C" void a2fo_native11_observe_database(void* database,void* stream,
    std::uint32_t scalar,const char* name,std::uint32_t native_x87_control) noexcept {
    __asm__ volatile("pushfl\n\tpopl %0\n\tfxsave %1":"=m"(callback_flags),"=m"(asset_callback_fx));
    ++observations;
    observed[0]=reinterpret_cast<std::uintptr_t>(database);
    observed[1]=reinterpret_cast<std::uintptr_t>(stream);
    observed[2]=scalar;observed[3]=reinterpret_cast<std::uintptr_t>(name);
    observed[4]=native_x87_control;
    __asm__ volatile("fld1\n\tpxor %%xmm0,%%xmm0\n\tpxor %%xmm7,%%xmm7":::"st","xmm0","xmm7");
}
int main(){
    unsigned short caller_cw=0;asm volatile("fnstcw %0":"=m"(caller_cw));
    for(unsigned pc:{0u,2u,3u})for(unsigned rc=0;rc<4;++rc)for(bool changed:{false,true}){
    asset_input_cw=static_cast<unsigned short>((caller_cw&~0xf00u)|(pc<<8)|(rc<<10)|0x3fu);
    asset_result_cw=changed?static_cast<unsigned short>(asset_input_cw^0x400u):0;
    asset_original_calls=observations=0;
    asset_fixture_run();
    need(asset_original_cw==asset_input_cw,"original loader receives unchanged entry control word");
    need(asset_original_calls==1&&observations==1,"native parser and observer called exactly once");
    need(asset_stack_before==asset_stack_after,"duplicate native ret8 and original ret8 balance");
    need(asset_input[0]==0x10203040&&asset_input[1]==0x3f800000&&asset_input[2]==0x13579bdf,"original ECX and argument order");
    const std::uint32_t expected_args[]{0x2468ace0,0x10203040,0x3f800000,0x13579bdf,
        changed?0u:0x10000u|asset_input_cw};
    need(!std::memcmp(observed,expected_args,sizeof(observed)),"post-parser result and original arguments observed");
    const std::uint32_t expected_gpr[]{0x2468ace0,0xaabbccdd,0xdeadbeef,0x11223344,0x55667788,0x99aabbcc};
    need(!std::memcmp(asset_gpr_actual,expected_gpr,sizeof(expected_gpr)),"native volatile result and callee-saved GPR preserved");
    need((asset_flags_actual&0xcd5)==(asset_flags_expected&0xcd5),"native arithmetic and direction flags preserved");
    need(!(callback_flags&0x400)&&asset_callback_fx[4]==0,"callback clean DF and empty x87 stack");
    const std::uint32_t clean_mxcsr=0x1f80;
    need(!std::memcmp(asset_callback_fx+24,&clean_mxcsr,4),"callback clean SSE environment");
    need(!std::memcmp(asset_fx_expected,asset_fx_actual,5),"native x87 control status and tags preserved");
    need(!std::memcmp(asset_fx_expected+24,asset_fx_actual+24,4),"native MXCSR preserved");
    for(unsigned i=0;i<8;++i)need(!std::memcmp(asset_fx_expected+32+i*16,asset_fx_actual+32+i*16,10),"eight occupied x87 slots preserved");
    need(!std::memcmp(asset_fx_expected+160,asset_fx_actual+160,128),"all native XMM registers preserved");
    }
    asm volatile("fldcw %0"::"m"(caller_cw):"memory");
    std::puts("PASS native11 opened-model bridge: native once, ret8, exact args/result, GPR/flags/full-x87/SSE; clean observer");
}
