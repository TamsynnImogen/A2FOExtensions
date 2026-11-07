#include "../renderer/native11/instance_animation_time.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
enum class Change {none,clock,base,flags,database,owner,enabled,engine_pointer};
struct Memory {
    std::array<std::uint8_t,0x10000> bytes{};
    std::array<std::uint8_t,0x10000> present{};
    Change change=Change::none;
    unsigned passes=0,forbidden_reads=0;
    std::uint32_t unreadable=0;
    template<class T> void put(std::uint32_t address,T value){
        need(address<=bytes.size()-sizeof(value),"fixture address");
        std::memcpy(bytes.data()+address,&value,sizeof(value));
        for(unsigned i=0;i<sizeof(value);++i)present[address+i]=1;
    }
    static bool read(void* raw,std::uint32_t address,void* output,std::size_t size){
        auto& m=*static_cast<Memory*>(raw);
        if(address==0x2100){++m.forbidden_reads;return false;}
        if(address==0x1000&&++m.passes==2){
            switch(m.change){
            case Change::clock:m.put(0x208c,8.0f);break;
            case Change::base:m.put(0x3018,1.0f);break;
            case Change::flags:m.put(0x300c,std::uint32_t(9));break;
            case Change::database:m.put(0x4000,std::uint32_t(0x7101));break;
            case Change::owner:m.put(0x3080,std::uint32_t(0x4100));break;
            case Change::enabled:m.put(0x3076,std::uint8_t(0));break;
            case Change::engine_pointer:m.put(0x1000,std::uint32_t(0x2200));break;
            case Change::none:break;
            }
        }
        if(address==m.unreadable||address>=m.bytes.size()||size>m.bytes.size()-address)return false;
        for(std::size_t i=0;i<size;++i)if(!m.present[address+i])return false;
        std::memcpy(output,m.bytes.data()+address,size);return true;
    }
};
constexpr InstanceAnimationTimeProfile profile{0x1000,0x7000,0x7100};
Object object(){Object o;o.instance=0x3000;o.database=0x4000;
    o.instance_vtable=profile.instance_vtable;o.database_vtable=profile.database_vtable;return o;}
Memory fixture(){
    Memory m;m.put(0x1000,std::uint32_t(0x2000));m.put(0x208c,5.0f);
    m.put(0x2200+0x8c,5.0f);
    // Engine's shared current-instance slot deliberately disagrees with Object.
    m.put(0x2000+0x100,std::uint32_t(0xdeadbeef));
    m.put(0x3000,profile.instance_vtable);m.put(0x300c,std::uint32_t(3));
    m.put(0x3018,2.0f);m.put(0x3076,std::uint8_t(1));
    m.put(0x3080,std::uint32_t(0x4000));m.put(0x4000,profile.database_vtable);
    m.put(0x4100,profile.database_vtable);return m;
}
void reject(Memory m,const char* why,InstanceAnimationTimeProfile p=profile,Object o=object()){
    InstanceAnimationTime out{1,2,3,4,true,5,6,7};std::string error;
    need(!capture_instance_animation_time(o,p,&m,Memory::read,out,error),why);
    need(out.engine==1&&out.instance==2&&out.database==3&&out.flags==4&&
         out.animating&&out.engine_time==5&&out.start_time==6&&out.elapsed==7&&
         !error.empty(),"failure leaves snapshot unchanged");
}
unsigned short control(){unsigned short cw=0;asm volatile("fnstcw %0":"=m"(cw));return cw;}
void set_control(unsigned short cw){asm volatile("fldcw %0"::"m"(cw):"memory");}
std::uint32_t bits(float value){std::uint32_t raw=0;std::memcpy(&raw,&value,sizeof(raw));return raw;}
float raw_difference(float a,float b){
    float result=0;
    asm volatile("flds %1; fsubs %2; fstps %0"
        :"=m"(result):"m"(a),"m"(b):"st","memory");
    return result;
}
void cases(){
    auto m=fixture();InstanceAnimationTime out;std::string error="stale";
    need(capture_instance_animation_time(object(),profile,&m,Memory::read,out,error),"owned instance timing");
    need(error.empty()&&m.passes==2&&m.forbidden_reads==0&&out.engine==0x2000&&
         out.instance==0x3000&&out.database==0x4000&&out.flags==3&&out.animating&&
         out.engine_time==5&&out.start_time==2&&out.elapsed==3,
         "two stable passes use observed instance and engine clock");
    m=fixture();m.put(0x3076,std::uint8_t(0));
    need(capture_instance_animation_time(object(),profile,&m,Memory::read,out,error)&&
         !out.animating&&out.elapsed==0,"disabled instance elapsed zero");
    m=fixture();m.put(0x3018,6.0f);
    need(capture_instance_animation_time(object(),profile,&m,Memory::read,out,error)&&
         out.animating&&out.start_time==6&&bits(out.elapsed)==bits(0.0f),
         "future base time clamps to positive zero");
    m=fixture();m.put(0x3018,5.0f);
    need(capture_instance_animation_time(object(),profile,&m,Memory::read,out,error)&&
         bits(out.elapsed)==bits(raw_difference(5.0f,5.0f)),
         "equal time preserves caller-rounded signed zero");
    for(auto change:{Change::clock,Change::base,Change::flags,Change::database,
                     Change::owner,Change::enabled,Change::engine_pointer}){
        m=fixture();m.change=change;reject(m,"changed raw input between full passes");
    }
    m=fixture();m.unreadable=0x208c;reject(m,"unreadable engine clock");
    m=fixture();m.put(0x208c,std::numeric_limits<float>::quiet_NaN());
    reject(m,"nonfinite engine clock");
    m=fixture();m.put(0x3018,std::numeric_limits<float>::infinity());
    reject(m,"nonfinite start time");
    m=fixture();m.put(0x208c,-1.0f);reject(m,"negative engine clock");
    m=fixture();m.put(0x3018,-1.0f);reject(m,"negative start time");
    m=fixture();m.put(0x3076,std::uint8_t(2));reject(m,"unsupported enabled byte");
    m=fixture();m.put(0x1000,std::uint32_t(0xffffff80));
    reject(m,"engine field address overflow");
    auto mismatched=object();mismatched.instance_vtable=1;
    reject(fixture(),"profile/object mismatch",profile,mismatched);
    auto missing=profile;missing.engine_pointer=0;
    reject(fixture(),"missing profile engine pointer",missing);
}
}
int main(){
    const auto original=control();
    for(unsigned mode=0;mode<4;++mode){
        const auto selected=static_cast<unsigned short>((original&~0x0c00u)|(mode<<10));
        set_control(selected);cases();need(control()==selected,"caller x87 control word preserved");
    }
    set_control(original);
    std::puts("PASS native11 instance animation time: owned two-pass timing, mutation and atomic rejection");
}
