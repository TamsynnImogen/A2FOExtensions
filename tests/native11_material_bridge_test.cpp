#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
std::uint32_t native11_fixture_call_tap(void* instance, void* camera);
void native11_fixture_original();
std::uintptr_t a2fo_native11_material_draw_original =
    reinterpret_cast<std::uintptr_t>(&native11_fixture_original);

alignas(16) unsigned char native11_fixture_fx_expected[512]{};
alignas(16) unsigned char native11_fixture_fx_actual[512]{};
alignas(16) unsigned char native11_fixture_fx_return_expected[512]{};
alignas(16) unsigned char native11_fixture_fx_return_actual[512]{};
alignas(16) unsigned char native11_fixture_xmm[128]{};
std::uint32_t native11_fixture_gpr[6]{};
std::uint32_t native11_fixture_gpr_return[6]{};
std::uint32_t native11_fixture_flags_expected=0,native11_fixture_flags_actual=0;
std::uint32_t native11_fixture_flags_return_expected=0,native11_fixture_flags_return_actual=0;
std::uint32_t native11_fixture_camera_actual=0,native11_fixture_original_calls=0;
std::uint32_t native11_fixture_stack_before=0,native11_fixture_stack_after=0;
std::uint32_t native11_fixture_observer_instance=0,native11_fixture_observer_camera=0;
std::uint32_t native11_fixture_observer_x87_status=0,native11_fixture_observer_mxcsr=0;
std::uint32_t native11_fixture_end_calls=0;
}

namespace {
void need(bool condition,const char* message){
    if(!condition){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}
}
unsigned observations=0;
}

extern "C" void a2fo_native11_observe_materials(void* instance,void* camera) noexcept {
    // Read the bridge's clean callback state before executing any observer work.
    __asm__ volatile("fnstsw %0\n\tstmxcsr %1"
        : "=m"(native11_fixture_observer_x87_status),
          "=m"(native11_fixture_observer_mxcsr));
    native11_fixture_observer_instance=static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(instance));
    native11_fixture_observer_camera=static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(camera));
    ++observations;
    // Exercise the bridge's caller-state restoration: volatile GPRs, flags,
    // x87 and SSE are intentionally changed after the observer has recorded input.
    __asm__ volatile("movl $0xdeadbeef, %%eax\n\t"
                     "movl $0xdeadbeef, %%ecx\n\t"
                     "movl $0xdeadbeef, %%edx\n\t"
                     "stc\n\tfninit\n\tfld1\n\tpxor %%xmm0, %%xmm0"
        ::: "eax","ecx","edx","cc","st","xmm0");
}

extern "C" void a2fo_native11_material_render_end() noexcept {
    ++native11_fixture_end_calls;
    __asm__ volatile("movl $0xdeadbeef, %%eax\n\t"
                     "movl $0xdeadbeef, %%ecx\n\t"
                     "movl $0xdeadbeef, %%edx\n\t"
                     "clc\n\tfninit\n\tfld1\n\tpxor %%xmm0, %%xmm0"
        ::: "eax","ecx","edx","cc","st","xmm0");
}

int main(){
    alignas(16) std::uint32_t instance=0x13572468;
    alignas(16) std::uint32_t camera=0x24681357;
    for(unsigned reg=0;reg<8;++reg)
        for(unsigned byte=0;byte<16;++byte)
            native11_fixture_xmm[reg*16+byte]=static_cast<unsigned char>(reg*16+byte+1);

    const auto result=native11_fixture_call_tap(&instance,&camera);
    need(result==0xaabbccdd,"original return value survives tail dispatch");
    need(native11_fixture_observer_instance==reinterpret_cast<std::uintptr_t>(&instance)&&
         native11_fixture_observer_camera==reinterpret_cast<std::uintptr_t>(&camera),
         "observer receives exact instance and camera");
    need(native11_fixture_observer_x87_status==0,"observer begins with empty initialized x87");
    need(native11_fixture_observer_mxcsr==0x1f80,"observer begins with default MXCSR");
    need(native11_fixture_original_calls==1&&observations==1&&native11_fixture_end_calls==1,
         "one observer, original, and end call");
    need(native11_fixture_stack_before==native11_fixture_stack_after,"original ret4 and stack balance");
    need(native11_fixture_gpr[0]==0x11223344&&native11_fixture_gpr[1]==0x55667788&&
         native11_fixture_gpr[2]==reinterpret_cast<std::uintptr_t>(&instance)&&
         native11_fixture_gpr[3]==0x12345678&&native11_fixture_gpr[4]==0x89abcdef&&
         native11_fixture_gpr[5]==0x76543210,"original entry GPR state restored");
    need(native11_fixture_camera_actual==reinterpret_cast<std::uintptr_t>(&camera),
         "original receives camera stack argument");
    need(native11_fixture_flags_expected==native11_fixture_flags_actual,
         "original entry flags restored");
    need(!std::memcmp(native11_fixture_fx_expected,native11_fixture_fx_actual,5),
         "x87 environment restored");
    need(!std::memcmp(native11_fixture_fx_expected+24,native11_fixture_fx_actual+24,4),
         "MXCSR restored");
    for(unsigned slot=0;slot<8;++slot)
        need(!std::memcmp(native11_fixture_fx_expected+32+slot*16,
                          native11_fixture_fx_actual+32+slot*16,10),
             "all eight occupied x87 registers restored");
    need(!std::memcmp(native11_fixture_fx_expected+160,native11_fixture_fx_actual+160,128),
         "all eight XMM registers restored");
    const std::uint32_t post_gpr[6]={0xaabbccdd,0x13579bdf,0x2468ace0,
                                      0xfedcba98,0x55aa55aa,0xaa55aa55};
    need(!std::memcmp(native11_fixture_gpr_return,post_gpr,sizeof(post_gpr)),
         "original return GPR state survives end callback");
    need(native11_fixture_flags_return_expected==native11_fixture_flags_return_actual,
         "original return flags survive end callback");
    need(!std::memcmp(native11_fixture_fx_return_expected,native11_fixture_fx_return_actual,5),
         "original return x87 environment survives end callback");
    need(!std::memcmp(native11_fixture_fx_return_expected+24,native11_fixture_fx_return_actual+24,4),
         "original return MXCSR survives end callback");
    for(unsigned slot=0;slot<8;++slot)
        need(!std::memcmp(native11_fixture_fx_return_expected+32+slot*16,
                          native11_fixture_fx_return_actual+32+slot*16,10),
             "original return x87 registers survive end callback");
    need(!std::memcmp(native11_fixture_fx_return_expected+160,
                      native11_fixture_fx_return_actual+160,128),
         "original return XMM registers survive end callback");
    std::puts("PASS native11 material bridge: entry and post-original state preserved through both callbacks");
}
