#include "../renderer/native11/open_read_owner.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {
struct Memory {
    std::uint32_t target=0x1000,gateway=0x2000;
    std::array<std::uint8_t,6> target_bytes{};
    std::array<std::uint8_t,11> gateway_bytes{};
    unsigned calls=0;
    bool fail_target=false,fail_gateway=false;
};
void put_u32(std::uint8_t* out,std::uint32_t value) {
    for(unsigned i=0;i<4;++i) out[i]=static_cast<std::uint8_t>(value>>(8*i));
}
void put_jump(std::uint8_t* out,std::uint32_t instruction,std::uint32_t destination) {
    out[0]=0xe9;
    put_u32(out+1,destination-(instruction+5));
}
bool read_memory(void* opaque,std::uint32_t address,void* out,std::size_t length) {
    auto& m=*static_cast<Memory*>(opaque);++m.calls;
    if(address==m.target&&length==m.target_bytes.size()&&!m.fail_target){
        std::memcpy(out,m.target_bytes.data(),length);return true;
    }
    if(address==m.gateway&&length==m.gateway_bytes.size()&&!m.fail_gateway){
        std::memcpy(out,m.gateway_bytes.data(),length);return true;
    }
    return false;
}
void need(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
void setup(Memory& m,std::uint32_t replacement=0x3000) {
    m.target_bytes.fill(0);put_jump(m.target_bytes.data(),m.target,replacement);m.target_bytes[5]=0x90;
    std::memcpy(m.gateway_bytes.data(),a2fo::native11::open_read_entry_bytes.data(),6);
    put_jump(m.gateway_bytes.data()+6,m.gateway+6,m.target+6);
}
}

int main(){
    using namespace a2fo::native11;
    Memory m;setup(m);OpenReadOwner owner{m.target,m.gateway,0x3000,6};
    need(owns_open_read_gateway(&m,read_memory,owner),"exact owned gateway accepted");

    auto rejected=[&](const char* why){m.calls=0;need(!owns_open_read_gateway(&m,read_memory,owner),why);};
    owner.length=5;rejected("wrong length rejected");owner.length=6;
    m.target_bytes[0]=0x90;rejected("target opcode rejected");setup(m);
    m.target_bytes[5]=0xcc;rejected("target NOP rejected");setup(m);
    m.gateway_bytes[0]^=1;rejected("gateway prefix rejected");setup(m);
    put_jump(m.gateway_bytes.data()+6,m.gateway+6,m.target+7);rejected("gateway back target rejected");setup(m);
    owner.replacement=0x3001;rejected("foreign replacement rejected");owner.replacement=0x3000;
    m.fail_target=true;rejected("unreadable target rejected");m.fail_target=false;
    m.fail_gateway=true;rejected("unreadable gateway rejected");m.fail_gateway=false;
    owner.target=0;rejected("zero target rejected");owner.target=m.target;
    owner.gateway=0;rejected("zero gateway rejected");owner.gateway=m.gateway;
    owner.replacement=0;rejected("zero replacement rejected");owner.replacement=0x3000;
    owner.target=std::numeric_limits<std::uint32_t>::max()-5+1;
    m.calls=0;need(!owns_open_read_gateway(&m,read_memory,owner)&&m.calls==0,"target overflow rejected before read");owner.target=m.target;
    owner.gateway=std::numeric_limits<std::uint32_t>::max()-10+1;
    m.calls=0;need(!owns_open_read_gateway(&m,read_memory,owner)&&m.calls==0,"gateway overflow rejected before read");owner.gateway=m.gateway;

    // Signed-negative rel32 and 32-bit wraparound both use x86 modulo arithmetic.
    setup(m,0x800);owner.replacement=0x800;
    need(owns_open_read_gateway(&m,read_memory,owner),"negative target displacement accepted");
    m.target=0xfffffff0u;m.gateway=0x2000;setup(m,0x1000);
    owner={m.target,m.gateway,0x1000,6};
    need(owns_open_read_gateway(&m,read_memory,owner),"modulo-32-bit next-address normalization accepted");
    std::puts("native11_open_read_owner_test: PASS");
}
