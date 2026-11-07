#include "../renderer/native11/scene_capture.hpp"
#include <windows.h>
#include <array>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
extern "C" {
float native11_fixture_fp=3.25f;
alignas(16) unsigned char native11_fixture_xmm[16]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
alignas(16) unsigned char native11_fixture_fx_expected[512]{};
alignas(16) unsigned char native11_fixture_fx_actual[512]{};
std::uint32_t native11_fixture_gpr[6]{};
std::uint32_t native11_fixture_flags_expected=0,native11_fixture_flags_actual=0;
std::uint32_t native11_fixture_camera_actual=0,native11_fixture_original_calls=0;
std::uint32_t native11_fixture_stack_before=0,native11_fixture_stack_after=0;
std::uint32_t native11_fixture_call_tap(void*,void*,void*);
void native11_fixture_instance_render();
}
using namespace a2fo::native11;
namespace {
void need(bool condition,const char* message){if(!condition){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
bool read_local(void*,std::uint32_t address,void* output,std::size_t length){
    SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),output,length,&copied)&&copied==length;
}
Collector collector(nullptr,read_local);
unsigned observations=0;
std::uint32_t id(void* p){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));}
}
extern "C" void a2fo_native11_observe_entity(void* entity,void* camera) noexcept {
    ++observations;collector.observe_post_frustum(id(entity),id(camera));
    // Deliberately dirty x87 and SSE state. The real observer performs float
    // validation and must be invisible to the original renderer's machine state.
    __asm__ volatile("fninit\n\tfld1\n\tpxor %%xmm0,%%xmm0":::"st","xmm0");
}
int main(){
    alignas(16) std::array<unsigned char,0x100> instance{};
    alignas(16) std::array<unsigned char,0x20> entity{};
    alignas(16) std::array<unsigned char,0x1c0> camera{};
    void* vtable[6]{};vtable[5]=reinterpret_cast<void*>(&native11_fixture_instance_render);
    auto* vt=vtable;std::memcpy(instance.data(),&vt,4);
    auto* ip=instance.data();std::memcpy(entity.data()+4,&ip,4);
    const float world[12]={1,0,0,0,1,0,0,0,1,10,20,30};
    std::memcpy(instance.data()+0x44,world,sizeof(world));
    const float screen[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float viewport[4]={0,0,640,480};
    std::memcpy(camera.data()+0x160,screen,sizeof(screen));
    std::memcpy(camera.data()+0x1b0,viewport,sizeof(viewport));
    need(collector.begin(id(camera.data()),true),"begin fixture view");
    const auto result=native11_fixture_call_tap(instance.data(),entity.data(),camera.data());
    need(result==0xaabbccdd,"original return value");
    need(native11_fixture_original_calls==1&&observations==1,"one observer and one original call");
    need(native11_fixture_stack_before==native11_fixture_stack_after,"original ret4 and stack balance");
    need(native11_fixture_gpr[0]==0x11223344&&native11_fixture_gpr[1]==0x55667788&&
         native11_fixture_gpr[2]==id(instance.data())&&native11_fixture_gpr[3]==id(vtable)&&
         native11_fixture_gpr[4]==id(entity.data())&&native11_fixture_gpr[5]==id(camera.data()),"original register contract");
    need(native11_fixture_camera_actual==id(camera.data()),"original camera stack argument");
    need((native11_fixture_flags_expected&0xcd5)==(native11_fixture_flags_actual&0xcd5),"original arithmetic/direction flags");
    need(std::memcmp(native11_fixture_fx_expected,native11_fixture_fx_actual,5)==0,"x87 control status and stack tag");
    need(std::memcmp(native11_fixture_fx_expected+24,native11_fixture_fx_actual+24,4)==0,"SSE control");
    for(unsigned slot=0;slot<7;++slot)
        need(std::memcmp(native11_fixture_fx_expected+32+slot*16,native11_fixture_fx_actual+32+slot*16,10)==0,"seven occupied x87 registers restored");
    need(std::memcmp(native11_fixture_fx_expected+160,native11_fixture_fx_actual+160,128)==0,"all SSE registers");
    auto frame=std::make_unique<Frame>();
    need(collector.end(id(camera.data()),*frame)&&frame->object_count==1,"bridge publishes owned high-level observation");
    need(frame->objects[0].world[3]==10&&frame->objects[0].world[7]==20&&frame->objects[0].world[11]==30,"bridge placement");
    std::puts("PASS native11 post-frustum bridge: original once, ret4, GPR/flags/x87/SSE, owned scene observation");
}
