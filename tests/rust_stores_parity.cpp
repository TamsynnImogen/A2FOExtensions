// Same harness is linked separately to the frozen C++ and current Rust APIs.
#include "energy_systems.hpp"
#include "wreckage_policy.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
std::uint32_t state = 0x35a2159bu;
std::uint32_t next() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
float value(std::uint32_t bits) { float result; std::memcpy(&result,&bits,4); return result; }
void number(float input) { std::uint32_t bits; std::memcpy(&bits,&input,4); std::printf(" %08x",bits); }
void bytes(std::string_view input) { std::printf(" %zu:",input.size()); for(unsigned char c:input)std::printf("%02x",c); }
void energy(unsigned count=12000) {
    using namespace a2fo::energy_systems;
    const std::array<std::uint32_t,22> values{{0,0x80000000u,0x3f800000u,0xbf800000u,
        0x3f800347u,0x3f800346u,0x3f800348u,0x7f800000u,0xff800000u,
        0x7fc00001u,0x7fa00003u,0x7f7fffffu,0x00800000u,1,0x80000001u,
        0x38d1b717u,0x38d1b718u,0x40000000u,0x41200000u,0x3fc00000u,0x21800000u,0x3f800003u}};
    for(unsigned i=0;i<count;++i) {
        const auto pick=[&](unsigned offset){return value(i<values.size()*8?values[(i+offset)%values.size()]:next());};
        StorePolicy p{pick(3),pick(7),static_cast<RechargeMode>(i%5)};
        float amount=pick(0),cost=pick(1),elapsed=pick(11);
        if(i==0){p={10.0f,value(0x3f800003u),RechargeMode::automatic};amount=value(0x21800000u);cost=1.0001f;elapsed=1.5f;}
        if(i==1){p={2.0f,1.0f,RechargeMode::automatic};amount=1.0f;cost=1.0001f;elapsed=0.25f;}
        const auto normalized=normalize_policy(p);
        std::printf("energy/%u",i);number(p.maximum);number(p.recharge_per_second);number(amount);number(cost);number(elapsed);
        number(normalized.maximum);number(normalized.recharge_per_second);std::printf(" %u %u %u %u",static_cast<unsigned>(normalized.mode),store_enabled(p),requires_resupply(p),can_consume(amount,cost));
        number(clamp_amount(amount,p));number(consume(amount,cost));
        for(bool range:{false,true}){number(recharge(amount,p,elapsed,range));number(reload_seconds(amount,p,range));}
        std::puts("");
    }
}
void wreckage(unsigned count=6000) {
    using namespace a2fo::wreckage;
    const std::vector<std::string> names={"", " \t\r\n", "wreckage", "  kdestroy.odf\r\n", "\vfighter\f", std::string(" f\0oo ",6),std::string(" \xff\xc0 ",4)};
    const std::vector<std::string> chances={"", "0", "100", " 100 ", "-0", "-1", "100.00001", "99.99999", "50", "NaN", "inf", "1e-45", "1e-999", "0x1.9p6", "0x1.8p5", "100x", "\v100", "100\v", std::string("100\0bad",7),std::string(63,'0'),std::string(64,'0'),"+25.75", "1,5"};
    for(auto seed : {0xdc47778bu,0x33984aa9u,0xd5f9efc4u}) {
        for(auto chance : {"100","50","99.99999"}) {
            const auto result=decide_replacement("wreck",true,chance,seed);
            std::printf("wreck-seam/%08x/%s %u\n",seed,chance,static_cast<unsigned>(result.status));
        }
    }
    for(unsigned i=0;i<count;++i) {
        const auto& name=names[i%names.size()];
        const auto& text=chances[(i/names.size())%chances.size()];
        const std::uint32_t seed=i<7?i:next();const bool present=i%3!=0;
        const auto result=decide_replacement(name,present,text,seed);
        std::printf("wreckage/%u %u %u %u",i,seed,present,static_cast<unsigned>(result.status));
        bytes(result.replacement_odf);
        if(result.status==DecisionStatus::replace)std::printf(" %zu",static_cast<std::size_t>(result.replacement_odf.data()-name.data()));
        std::puts("");
    }
}
}
int main(){
    energy();wreckage();
#if defined(_WIN32) && defined(__i386__)
    // D3D-era code can select a different x87 precision. Exercise the exact
    // same APIs at 24/53/64-bit precision and restore the caller's control word.
    unsigned short original=0;
    asm volatile("fnstcw %0" : "=m"(original));
    for(unsigned precision : {0u,0x200u,0x300u}) {
        const unsigned short selected=static_cast<unsigned short>((original & ~0x300u)|precision);
        asm volatile("fldcw %0" : : "m"(selected) : "memory");
        std::printf("precision/%u\n",precision);
        energy(1500);wreckage(1200);
    }
    asm volatile("fldcw %0" : : "m"(original) : "memory");
#endif
}
