#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

extern "C" {
void sprite_fixture_run(void* device,void* table);
void end_sprite_fixture_run(void* wrapper,void* table);
void fleet_end_sprite_fixture_run(void* wrapper,void* table);
void sprite_fixture_original();
void end_sprite_fixture_original();
void fleet_end_sprite_fixture_original();
std::uintptr_t a2fo_native11_sprite_end_scene_original=
    reinterpret_cast<std::uintptr_t>(&fleet_end_sprite_fixture_original);
std::uint16_t sprite_fixture_cw=0x037f;
std::uint32_t sprite_fixture_mxcsr=0x3f80,sprite_fixture_result_mxcsr=0x5f80;
std::uint32_t sprite_fixture_args[7]{},end_sprite_fixture_comthis=0;
std::uint32_t sprite_fixture_observer_calls=0,end_sprite_fixture_observer_calls=0;
std::uint32_t sprite_fixture_original_calls=0,sprite_fixture_original_saw_observer=0;
std::uint32_t sprite_fixture_observed_device=0,sprite_fixture_observed_table=0;
std::uint32_t sprite_fixture_observed_args[7]{},sprite_fixture_observed_cw=0;
std::uint32_t end_sprite_fixture_observed_wrapper=0,end_sprite_fixture_observed_comthis=0;
std::uint32_t end_sprite_fixture_observed_table=0;
std::uint32_t sprite_fixture_stack_before=0,sprite_fixture_stack_after=0;
std::uint32_t sprite_fixture_original_stack=0,sprite_fixture_original_args[7]{};
std::uint32_t sprite_fixture_expected_flags=0,sprite_fixture_original_flags=0;
std::uint32_t sprite_fixture_callback_flags=0,sprite_fixture_original_gpr[7]{};
std::uint32_t sprite_fixture_result_gpr[7]{};
alignas(16) unsigned char sprite_fixture_expected_fx[512]{};
alignas(16) unsigned char sprite_fixture_original_fx[512]{};
alignas(16) unsigned char sprite_fixture_callback_fx[512]{};
alignas(16) unsigned char sprite_fixture_result_fx[512]{};
alignas(16) unsigned char sprite_fixture_xmm_seed[128]{};
alignas(16) unsigned char sprite_fixture_xmm_result[16]{};
float sprite_fixture_fp_seed[8]={1,2,3,4,5,6,7,8};
float sprite_fixture_fp_sprite=11.25f,sprite_fixture_fp_end=22.5f;
float sprite_fixture_fp_fleet=33.75f;
std::uint32_t sprite_fixture_clean_mxcsr=0x1f80;
}

namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
std::uint32_t bits(float value){std::uint32_t result=0;std::memcpy(&result,&value,4);return result;}
std::uint16_t control(){std::uint16_t value=0;asm volatile("fnstcw %0":"=m"(value));return value;}
void compare_entry_fp(){
    need(!std::memcmp(sprite_fixture_expected_fx,sprite_fixture_original_fx,5)&&
         !std::memcmp(sprite_fixture_expected_fx+24,sprite_fixture_original_fx+24,4),
         "native original sees entry x87 status/control/tags and MXCSR");
    for(unsigned slot=0;slot<8;++slot)
        need(!std::memcmp(sprite_fixture_expected_fx+32+slot*16,
                          sprite_fixture_original_fx+32+slot*16,10),
             "native original sees all eight occupied x87 slots");
    need(!std::memcmp(sprite_fixture_expected_fx+160,sprite_fixture_original_fx+160,128),
         "native original sees XMM0 through XMM7");
}
void compare_result_fp(bool end_scene,bool fleet=false){
    const auto value=fleet?sprite_fixture_fp_fleet:
        (end_scene?sprite_fixture_fp_end:sprite_fixture_fp_sprite);
    alignas(16) unsigned char expected[512]{};
    asm volatile("fninit; flds %1; fxsave %0; fninit"
        :"=m"(expected):"m"(value):"st","memory");
    need(!std::memcmp(sprite_fixture_result_fx,expected,5),
         "fake original x87 control/status/tag reaches caller");
    const unsigned top=(expected[3]>>3)&7u;
    for(unsigned slot=0;slot<8;++slot)
        if(expected[4]&(1u<<((top+slot)&7u)))
            need(!std::memcmp(sprite_fixture_result_fx+32+slot*16,
                              expected+32+slot*16,10),
                 "fake original complete 80-bit x87 result reaches caller");
    need(!std::memcmp(sprite_fixture_result_fx+24,&sprite_fixture_result_mxcsr,4)&&
         !std::memcmp(sprite_fixture_result_fx+160,sprite_fixture_xmm_result,16),
         "fake original SSE result reaches caller");
}
void compare_common(bool end_scene,bool fleet=false){
    need(sprite_fixture_original_calls==1&&sprite_fixture_original_saw_observer==1&&
         sprite_fixture_observer_calls==(end_scene?0u:1u)&&
         end_sprite_fixture_observer_calls==(end_scene?1u:0u),
         "exactly one correct callback precedes exactly one native original");
    need(sprite_fixture_stack_after==sprite_fixture_stack_before&&
         sprite_fixture_original_stack==sprite_fixture_stack_before-(end_scene?8u:32u),
         "native ret4 or ret28 leaves caller stack balanced");
    need((sprite_fixture_original_flags&0xcd5)==(sprite_fixture_expected_flags&0xcd5),
         "native original sees exact arithmetic and direction flags");
    compare_entry_fp();
    need(!(sprite_fixture_callback_flags&0x400)&&sprite_fixture_callback_fx[4]==0&&
         !std::memcmp(sprite_fixture_callback_fx+24,&sprite_fixture_clean_mxcsr,4),
         "callback starts with clean DF, empty x87 and default MXCSR");
    compare_result_fp(end_scene,fleet);
}
void clobber_callback_state(){
    asm volatile(
        "fninit; fld1; fld1; fld1; fld1; fld1; fld1; fld1; fld1; "
        "pxor %%xmm0,%%xmm0; pxor %%xmm1,%%xmm1; pxor %%xmm2,%%xmm2; "
        "pxor %%xmm3,%%xmm3; pxor %%xmm4,%%xmm4; pxor %%xmm5,%%xmm5; "
        "pxor %%xmm6,%%xmm6; pxor %%xmm7,%%xmm7; "
        "ldmxcsr _sprite_fixture_mxcsr; std"
        :::"cc","memory","st","xmm0","xmm1","xmm2","xmm3",
           "xmm4","xmm5","xmm6","xmm7");
}
}

extern "C" void a2fo_native11_observe_sprite(void* device,const std::uint32_t* args,
    std::uint32_t encoded_original_cw,void* table) noexcept {
    asm volatile("pushfl; popl %0; fxsave %1"
        :"=m"(sprite_fixture_callback_flags),"=m"(sprite_fixture_callback_fx));
    ++sprite_fixture_observer_calls;
    sprite_fixture_observed_device=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(device));
    sprite_fixture_observed_table=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(table));
    sprite_fixture_observed_cw=encoded_original_cw;
    if(args)std::memcpy(sprite_fixture_observed_args,args,sizeof(sprite_fixture_observed_args));
    clobber_callback_state();
}
extern "C" void a2fo_native11_end_sprite_ui(void* wrapper,void* com_this,void* table) noexcept {
    asm volatile("pushfl; popl %0; fxsave %1"
        :"=m"(sprite_fixture_callback_flags),"=m"(sprite_fixture_callback_fx));
    ++end_sprite_fixture_observer_calls;
    end_sprite_fixture_observed_wrapper=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(wrapper));
    end_sprite_fixture_observed_comthis=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(com_this));
    end_sprite_fixture_observed_table=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(table));
    clobber_callback_state();
}

int main(){
    alignas(4) unsigned char sprite_table[0xc8]{},end_table[0x90]{};
    unsigned char device[16]{},wrapper[16]{},com_this[16]{},sprite[16]{},pos[16]{},size[16]{};
    const auto sprite_original=reinterpret_cast<std::uintptr_t>(&sprite_fixture_original);
    const auto end_original=reinterpret_cast<std::uintptr_t>(&end_sprite_fixture_original);
    std::memcpy(sprite_table+0xc4,&sprite_original,4);
    std::memcpy(end_table+0x8c,&end_original,4);
    sprite_fixture_args[0]=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(sprite));
    sprite_fixture_args[1]=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pos));
    sprite_fixture_args[2]=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(size));
    sprite_fixture_args[3]=bits(0.125f);sprite_fixture_args[4]=bits(0.25f);
    sprite_fixture_args[5]=bits(0.5f);sprite_fixture_args[6]=bits(0.75f);
    end_sprite_fixture_comthis=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(com_this));
    for(unsigned i=0;i<128;++i)sprite_fixture_xmm_seed[i]=static_cast<unsigned char>(i+1);
    for(unsigned i=0;i<16;++i)sprite_fixture_xmm_result[i]=static_cast<unsigned char>(0xe0+i);
    const auto caller=control();
    for(unsigned pc:{0u,2u,3u})for(unsigned rc=0;rc<4;++rc){
        sprite_fixture_cw=static_cast<std::uint16_t>((caller&~0x0f00u)|
            (pc<<8)|(rc<<10)|0x003fu);
        sprite_fixture_observer_calls=end_sprite_fixture_observer_calls=
            sprite_fixture_original_calls=0;
        sprite_fixture_run(device,sprite_table);
        compare_common(false);
        const std::uint32_t sprite_gpr[]{0x11111111,0x22222222,
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(device)),
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(sprite_table)),
            0x55555555,0x66666666,0x24681357};
        need(!std::memcmp(sprite_fixture_original_gpr,sprite_gpr,sizeof(sprite_gpr))&&
             !std::memcmp(sprite_fixture_original_args,sprite_fixture_args,sizeof(sprite_fixture_args))&&
             !std::memcmp(sprite_fixture_observed_args,sprite_fixture_args,sizeof(sprite_fixture_args))&&
             sprite_fixture_observed_device==sprite_gpr[2]&&
             sprite_fixture_observed_table==sprite_gpr[3]&&
             sprite_fixture_observed_cw==(0x10000u|sprite_fixture_cw),
             "Sprite preserves device/table/GPRs, seven args and original x87 CW");
        const std::uint32_t sprite_result[]{0xa0a0a0a0u,0x22222222u,0xc0c0c0c0u,
            sprite_gpr[3],0x55555555u,0x66666666u,0x24681357u};
        need(!std::memcmp(sprite_fixture_result_gpr,sprite_result,sizeof(sprite_result)),
             "Sprite fake original GPR effects reach caller");

        sprite_fixture_observer_calls=end_sprite_fixture_observer_calls=
            sprite_fixture_original_calls=0;
        end_sprite_fixture_run(wrapper,end_table);
        compare_common(true);
        const std::uint32_t end_gpr[]{0x11111111,0x22222222,
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(end_table)),
            0x44444444,static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(wrapper)),
            0x66666666,0x24681357};
        need(!std::memcmp(sprite_fixture_original_gpr,end_gpr,sizeof(end_gpr))&&
             sprite_fixture_original_args[0]==end_sprite_fixture_comthis&&
             end_sprite_fixture_observed_wrapper==end_gpr[4]&&
             end_sprite_fixture_observed_comthis==end_sprite_fixture_comthis&&
             end_sprite_fixture_observed_table==end_gpr[2],
             "EndScene preserves wrapper/table/COM this and entry GPRs");
        const std::uint32_t end_result[]{0xb0b0b0b0u,0x22222222u,0xd0d0d0d0u,
            0x44444444u,end_gpr[4],0x66666666u,0x24681357u};
        need(!std::memcmp(sprite_fixture_result_gpr,end_result,sizeof(end_result)),
             "EndScene fake original GPR effects reach caller");

        sprite_fixture_observer_calls=end_sprite_fixture_observer_calls=
            sprite_fixture_original_calls=0;
        fleet_end_sprite_fixture_run(wrapper,end_table);
        compare_common(true,true);
        need(!std::memcmp(sprite_fixture_original_gpr,end_gpr,sizeof(end_gpr))&&
             sprite_fixture_original_args[0]==end_sprite_fixture_comthis&&
             end_sprite_fixture_observed_wrapper==end_gpr[4]&&
             end_sprite_fixture_observed_comthis==end_sprite_fixture_comthis&&
             end_sprite_fixture_observed_table==end_gpr[2],
             "Fleet EndScene preserves wrapper/table/COM this and entry GPRs");
        const std::uint32_t fleet_result[]{0xe0e0e0e0u,0x22222222u,0xf0f0f0f0u,
            0x44444444u,end_gpr[4],0x66666666u,0x24681357u};
        need(!std::memcmp(sprite_fixture_result_gpr,fleet_result,sizeof(fleet_result)),
             "Fleet EndScene tail-jumps global original instead of COM slot");
    }
    asm volatile("cld; fninit; fldcw %0"::"m"(caller):"memory");
    std::puts("PASS native11 sprite bridge: 12 PC/RC modes, three taps, complete entry state and native returns");
}
