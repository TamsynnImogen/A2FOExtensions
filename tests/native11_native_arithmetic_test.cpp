#include "../renderer/native11/native_arithmetic.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
std::uint16_t control(){std::uint16_t value=0;asm volatile("fnstcw %0":"=m"(value));return value;}
void control(std::uint16_t value){asm volatile("fldcw %0"::"m"(value):"memory");}
std::uint32_t mxcsr(){std::uint32_t value=0;asm volatile("stmxcsr %0":"=m"(value));return value;}
void cases(){
    const auto original=control();const auto original_mxcsr=mxcsr();
    const auto selected_caller=static_cast<std::uint16_t>((original&~0x0f00u)|0x0b00u);
    control(selected_caller);
    {
        NativeArithmeticScope unknown(0);
        need(unknown.valid()&&!unknown.known()&&control()==selected_caller,
             "zero is valid unknown and preserves caller CW");
    }
    need(control()==selected_caller,"unknown scope destructor preserves CW");
    for(const auto encoded:{1u,0x100u,0x10000u|0x100u,0x20000u,0x30000u,0xffffffffu}){
        NativeArithmeticScope invalid(encoded);
        need(!invalid.valid()&&!invalid.known()&&control()==selected_caller,
             "invalid marker, reserved precision or upper bits rejected without FP change");
    }
    for(const auto pc:std::array<std::uint16_t,3>{{0,0x0200,0x0300}}){
        for(const auto rc:std::array<std::uint16_t,4>{{0,0x0400,0x0800,0x0c00}}){
            const auto word=static_cast<std::uint16_t>(pc|rc|0x0040u);
            const auto before=control();const auto sse=mxcsr();
            {
                NativeArithmeticScope scope(encode_native_x87(word));
                const auto expected=static_cast<std::uint16_t>(
                    (before&static_cast<std::uint16_t>(~0x0f00u))|(word&0x0f00u)|0x003fu);
                need(scope.valid()&&scope.known()&&control()==expected&&mxcsr()==sse,
                     "each native precision/round combination changes only x87 CW");
            }
            need(control()==before&&mxcsr()==sse,"scope restores exact caller CW and MXCSR");
        }
    }
    {
        NativeArithmeticScope outer(encode_native_x87(0x0200));
        const auto outer_cw=control();
        {
            NativeArithmeticScope inner(encode_native_x87(0x0c00));
            need(inner.valid()&&control()!=outer_cw,"nested inner scope applies its control");
        }
        need(control()==outer_cw,"nested inner scope restores outer control");
    }
    need(control()==selected_caller,"outer scope restores caller control");
    double popped=0;
    asm volatile("fld1":::"st");
    {
        NativeArithmeticScope scope(encode_native_x87(0x0300));
        need(scope.valid(),"stack preservation scope accepted");
    }
    asm volatile("fstpl %0":"=m"(popped)::"st","memory");
    need(popped==1.0&&control()==selected_caller&&mxcsr()==original_mxcsr,
         "x87 stack value and SSE control preserved");
    // Verify exception masking even when a caller leaves one mask bit clear.
    const auto unmasked=static_cast<std::uint16_t>(selected_caller&~1u);
    control(unmasked);
    {
        NativeArithmeticScope scope(encode_native_x87(0));
        need(scope.valid()&&scope.known()&&(control()&0x003fu)==0x003fu,
             "known scope masks all six x87 exceptions");
    }
    need(control()==unmasked,"exception mask restored byte-exact");
    control(original);
    need(control()==original&&mxcsr()==original_mxcsr,"test restores original FP control");
}
}
int main(){cases();std::puts("PASS native11 native arithmetic scope");}
