#include "native11_nodes.hpp"
#include "native11_node_profile.hpp"
#include "native11_capture.hpp"
#include "native11_materials.hpp"
#include "../rust/native_hooks_ffi.h"
#include <array>
#include <atomic>
#include <cstring>
namespace {
constexpr std::array<std::uint32_t,2> sites{{0x22e89d,0x22e958}};
constexpr std::array<std::array<std::uint8_t,5>,2> expected{{{{0x83,0xc6,0x44,0x8b,0x11}},{{0x83,0xc6,0x44,0x8b,0x01}}}};
bool installed[2]{}; // startup admission only, not a concurrent hot-reload API
std::atomic<HMODULE> image_owner{nullptr};
std::atomic<a2fo::native11_nodes::Log> logger{nullptr};
volatile LONG active=0;
std::atomic<bool> wanted{false};
std::uintptr_t target(unsigned i) noexcept {return reinterpret_cast<std::uintptr_t>(i?&a2fo_native11_node_branch_b:&a2fo_native11_node_branch_a);}
bool read(std::uintptr_t address,void* output,std::size_t size) noexcept {
    SIZE_T copied=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),output,size,&copied)&&copied==size;
}
bool owned(HMODULE image,unsigned i) noexcept {
    const auto base=reinterpret_cast<std::uintptr_t>(image);std::array<std::uint8_t,5> actual{};
    if(!read(base+sites[i],actual.data(),actual.size()))return false;
    if(!installed[i])return actual==expected[i];
    std::int32_t delta=0;std::memcpy(&delta,actual.data()+1,4);
    return actual[0]==0xe8&&base+sites[i]+5+delta==target(i);
}
bool loaded(HMODULE image) noexcept {
    namespace p=a2fo::native11_nodes::profile;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image));
    const auto delta=base-p::preferred_base;
    if(!owned(image,0)||!owned(image,1))return false;
    for(const auto& region:p::regions){std::array<std::uint8_t,512> bytes{};
        if(region.size>bytes.size()||!read(base+region.rva,bytes.data(),region.size))return false;
        for(unsigned i=0;i<2;++i)if(installed[i]&&region.rva<=sites[i]&&sites[i]+5<=region.rva+region.size)
            std::memcpy(bytes.data()+sites[i]-region.rva,expected[i].data(),5);
        for(std::size_t i=0;i<region.relocation_count;++i){const auto at=region.relocations[i];
            if(at+4u>region.size)return false;
            std::uint32_t word=0;std::memcpy(&word,bytes.data()+at,4);word-=delta;std::memcpy(bytes.data()+at,&word,4);
        }
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    for(const auto& slot:p::slots){std::uint32_t word=0;
        if(!read(base+slot[0],&word,4)||word!=base+slot[1])return false;
    }
    return true;
}
void report(const char* text) noexcept {try{if(auto f=logger.load())f(text);}catch(...){}}
}
namespace a2fo::native11_nodes {
bool enabled() noexcept {return InterlockedCompareExchange(&active,0,0)!=0;}
bool requested() noexcept {return wanted.load();}
void disable() noexcept {InterlockedExchange(&active,0);}
bool owns_calls() noexcept {
    const auto image=image_owner.load();return enabled()&&image&&installed[0]&&installed[1]&&owned(image,0)&&owned(image,1);
}
bool initialize(HMODULE image,bool requested,Log log) noexcept {
    logger.store(log);wanted.store(requested);disable();
    if(!requested)return false;
    if(!image||!IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)||
       !native11_capture::exact_engine_image(image)||!loaded(image)){
        report("Native11 node capture unavailable: exact post-animation profile or ownership mismatch");return false;
    }
    for(unsigned i=0;i<2;++i)if(!installed[i]){
        if(!a2fo_rs_windows_patch_call(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(image)+sites[i]),
            reinterpret_cast<void*>(target(i)),expected[i].data(),expected[i].size())){
            report("Native11 node capture unavailable: checked branch refused; installed pass-through remains inactive");return false;
        }
        installed[i]=true;
    }
    image_owner.store(image);InterlockedExchange(&active,1);
    report("Native11 node capture armed: post-animation hierarchy state; native draws and animation evaluation unchanged");return true;
}
}
extern "C" void a2fo_native11_observe_nodes(void* instance,void* camera,void* database,
    unsigned native_x87_control) noexcept {
    if(a2fo::native11_nodes::enabled())a2fo::native11_materials::observe_nodes(instance,camera,database,native_x87_control);
}
