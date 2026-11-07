#include "../renderer/native11/sprite_colour_policy.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){
    if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}
}
std::uint16_t control(){std::uint16_t value=0;asm volatile("fnstcw %0":"=m"(value));return value;}
void control(std::uint16_t value){asm volatile("fldcw %0"::"m"(value):"memory");}
void rejected(const float* rgb,float alpha,std::uint32_t encoded,const char* why){
    std::uint32_t output=0x12345678u;
    const auto before=control();
    need(!native_sprite_colour(rgb,alpha,encoded,output)&&output==0x12345678u&&
         control()==before,why);
}
}
int main(){
    constexpr auto default_cw=encode_native_x87(0x037f);
    const std::array<float,3> black{{0,0,0}},white{{1,1,1}};
    const std::array<float,3> half{{0.5f,0.5f,0.5f}};
    std::uint32_t colour=0;
    need(native_sprite_colour(black.data(),0,default_cw,colour)&&colour==0x00000000u,
         "default x87 black and zero alpha");
    need(native_sprite_colour(black.data(),1,default_cw,colour)&&colour==0xff000000u,
         "default x87 opaque black");
    need(native_sprite_colour(white.data(),1,default_cw,colour)&&colour==0xffffffffu,
         "default x87 opaque white");
    need(native_sprite_colour(half.data(),0.5f,default_cw,colour)&&colour==0x80808080u,
         "default x87 ties round half channels and alpha to 128");
    const std::array<float,3> red{{1,0,0}};
    need(native_sprite_colour(red.data(),0.5f,default_cw,colour)&&colour==0x80ff0000u,
         "alpha is added after wrapping RGB channel shifts");

    rejected(nullptr,1,default_cw,"null RGB rejected atomically");
    rejected(black.data(),0,0,"unknown arithmetic rejected atomically");
    rejected(black.data(),0,0x037fu,"unmarked arithmetic rejected atomically");
    rejected(black.data(),0,encode_native_x87(0x0100),
             "reserved precision rejected atomically");
    rejected(black.data(),0,0x20000u,"upper encoding bits rejected atomically");
    rejected(black.data(),-0.01f,default_cw,"negative alpha rejected atomically");
    rejected(black.data(),1.01f,default_cw,"alpha above one rejected atomically");
    rejected(black.data(),std::numeric_limits<float>::quiet_NaN(),default_cw,
             "nonfinite alpha rejected atomically");
    for(unsigned channel=0;channel<3;++channel){
        auto bad=half;bad[channel]=-0.01f;
        rejected(bad.data(),0.5f,default_cw,"negative RGB channel rejected atomically");
        bad=half;bad[channel]=1.01f;
        rejected(bad.data(),0.5f,default_cw,"RGB channel above one rejected atomically");
        bad=half;bad[channel]=std::numeric_limits<float>::infinity();
        rejected(bad.data(),0.5f,default_cw,"nonfinite RGB channel rejected atomically");
    }

    const auto caller=control();
    for(unsigned pc:{0u,2u,3u})for(unsigned rc=0;rc<4;++rc){
        const auto selected=encode_native_x87(static_cast<std::uint16_t>(
            (caller&~0x0f00u)|(pc<<8)|(rc<<10)|0x003fu));
        const auto before=control();
        std::uint32_t first=0,again=0;
        need(native_sprite_colour(half.data(),0.5f,selected,first)&&control()==before&&
             native_sprite_colour(half.data(),0.5f,selected,again)&&
             control()==before&&first==again,
             "each precision and rounding mode restores caller CW and is deterministic");
    }
    control(caller);
    std::puts("PASS native11 sprite colour policy");
}
