#include "instance_animation_time.hpp"
#include <cmath>
#include <cstring>
#include <limits>

#if !defined(__i386__) && !defined(__x86_64__)
#error Native instance animation time parity requires an x87 target
#endif

namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* message) noexcept {
    try {error=message;}catch(...){error.clear();}return false;
}
bool at(void* context,Read read,std::uint32_t base,std::uint32_t offset,
        void* output,std::size_t size) noexcept {
    return read&&base&&size&&offset<=UINT32_MAX-base&&
        size-1<=UINT32_MAX-base-offset&&read(context,base+offset,output,size);
}
template<class T> bool field(void* context,Read read,std::uint32_t base,
                             std::uint32_t offset,T& output) noexcept {
    return at(context,read,base,offset,&output,sizeof(output));
}
struct Pass {
    std::uint32_t engine_pointer=0,engine=0,engine_time_bits=0;
    std::uint32_t instance=0,instance_vtable=0,database=0,database_vtable=0;
    std::uint32_t flags=0,start_bits=0;
    std::uint8_t enabled=0;
};
float value(std::uint32_t bits) noexcept {
    float result=0;std::memcpy(&result,&bits,sizeof(result));return result;
}
bool read_pass(const Object& object,InstanceAnimationTimeProfile profile,
               void* context,Read read,Pass& output) noexcept {
    output.engine_pointer=profile.engine_pointer;output.instance=object.instance;
    if(!field(context,read,profile.engine_pointer,0,output.engine)||!output.engine||
       !field(context,read,output.engine,0x8c,output.engine_time_bits)||
       !field(context,read,output.instance,0,output.instance_vtable)||
       output.instance_vtable!=profile.instance_vtable||
       !field(context,read,output.instance,0x80,output.database)||
       output.database!=object.database||
       !field(context,read,output.database,0,output.database_vtable)||
       output.database_vtable!=profile.database_vtable||
       !field(context,read,output.instance,0xc,output.flags)||
       !field(context,read,output.instance,0x76,output.enabled)||output.enabled>1||
       !field(context,read,output.instance,0x18,output.start_bits))return false;
    const float engine_time=value(output.engine_time_bits),start_time=value(output.start_bits);
    return std::isfinite(engine_time)&&engine_time>=0&&
           std::isfinite(start_time)&&start_time>=0;
}
bool same(const Pass& a,const Pass& b) noexcept {
    return a.engine_pointer==b.engine_pointer&&a.engine==b.engine&&
        a.engine_time_bits==b.engine_time_bits&&a.instance==b.instance&&
        a.instance_vtable==b.instance_vtable&&a.database==b.database&&
        a.database_vtable==b.database_vtable&&a.flags==b.flags&&
        a.enabled==b.enabled&&a.start_bits==b.start_bits;
}
float native_elapsed(float engine_time,float start_time) noexcept {
    float result=0;
    asm volatile("flds %1; fsubs %2; fstps %0"
        :"=m"(result):"m"(engine_time),"m"(start_time):"st","memory");
    // Native compares the extended subtraction with zero before the store.
    // Equal operands can store -0 under round-down; preserve that bit pattern.
    return engine_time<start_time?0.0f:result;
}
}
bool capture_instance_animation_time(const Object& object,InstanceAnimationTimeProfile profile,
    void* context,Read read,InstanceAnimationTime& output,std::string& error) noexcept {
    try {
        if(!profile.engine_pointer||!profile.instance_vtable||!profile.database_vtable||
           !object.instance||!object.database||
           object.instance_vtable!=profile.instance_vtable||
           object.database_vtable!=profile.database_vtable||!read)
            return fail(error,"invalid animation time profile or object");
        Pass first,second;
        if(!read_pass(object,profile,context,read,first)||
           !read_pass(object,profile,context,read,second)||!same(first,second))
            return fail(error,"unreadable or changing animation time owner");
        InstanceAnimationTime candidate;
        candidate.engine=first.engine;candidate.instance=first.instance;
        candidate.database=first.database;candidate.flags=first.flags;
        candidate.animating=first.enabled!=0;
        candidate.engine_time=value(first.engine_time_bits);
        candidate.start_time=value(first.start_bits);
        candidate.elapsed=candidate.animating?native_elapsed(candidate.engine_time,candidate.start_time):0;
        if(!std::isfinite(candidate.elapsed))return fail(error,"nonfinite animation elapsed time");
        output=candidate;error.clear();return true;
    }catch(...){return fail(error,"animation time snapshot failed");}
}
} // namespace a2fo::native11
