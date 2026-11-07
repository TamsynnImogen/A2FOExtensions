#include "refit_mesh_capture.hpp"
#include "refit_scene_profile.hpp"
#include "native11_capture.hpp"
#include "../rust/native_hooks_ffi.h"
#include <array>
#include <cstring>

extern "C" {
void a2fo_refit_mesh_fast_tap();
void a2fo_refit_mesh_legacy_tap();
std::uintptr_t a2fo_refit_mesh_fast_original=0;
std::uintptr_t a2fo_refit_mesh_legacy_original=0;
}
namespace {
constexpr std::array<std::uint32_t,2> sites{{0x2327fc,0x232812}};
constexpr std::array<std::uint32_t,2> originals{{0x231f10,0x231fd0}};
HMODULE image=nullptr;DWORD owner=0;
bool installed[2]{},active=false;
a2fo::refit_mesh_capture::Observe observe=nullptr;
a2fo::refit_mesh_capture::Scope notify_scope=nullptr;
bool read(std::uintptr_t address,void* out,std::size_t n) noexcept {
    SIZE_T copied=0;return address&&n<=UINTPTR_MAX-address&&ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<void*>(address),out,n,&copied)&&copied==n;
}
std::uintptr_t target(unsigned i) noexcept {
    return reinterpret_cast<std::uintptr_t>(i?&a2fo_refit_mesh_legacy_tap:&a2fo_refit_mesh_fast_tap);
}
std::array<std::uint8_t,5> native_call(unsigned i) noexcept {
    std::array<std::uint8_t,5> out{{0xe8,0,0,0,0}};
    const auto relative=static_cast<std::int32_t>(originals[i]-sites[i]-5);
    std::memcpy(out.data()+1,&relative,4);return out;
}
bool owned(HMODULE module,unsigned i) noexcept {
    const auto base=reinterpret_cast<std::uintptr_t>(module);
    std::array<std::uint8_t,5> actual{};
    if(!read(base+sites[i],actual.data(),actual.size()))return false;
    if(!installed[i])return actual==native_call(i);
    std::int32_t relative=0;std::memcpy(&relative,actual.data()+1,4);
    return actual[0]==0xe8&&base+sites[i]+5+relative==target(i);
}
bool context_matches(HMODULE module) noexcept {
    namespace p=a2fo::refit_scene::profile;
    const auto base=reinterpret_cast<std::uintptr_t>(module);
    std::array<std::uint8_t,p::mesh_render_dispatch_bytes.size()> bytes{};
    if(!owned(module,0)||!owned(module,1)||!read(base+0x2327cf,bytes.data(),bytes.size()))return false;
    for(unsigned i=0;i<2;++i)if(installed[i]){
        const auto call=native_call(i);std::memcpy(bytes.data()+sites[i]-0x2327cf,call.data(),call.size());
    }
    // This dispatch region has only relative calls and stack/register operands.
    static_assert(p::mesh_render_dispatch_relocations.empty());
    return bytes==p::mesh_render_dispatch_bytes;
}
bool regions_match(HMODULE module,const a2fo::refit_scene::profile::Region* regions,std::size_t count) noexcept {
    const auto base=reinterpret_cast<std::uintptr_t>(module);
    for(std::size_t j=0;j<count;++j){const auto& region=regions[j];
        std::array<std::uint8_t,512> bytes{};
        if(region.size>bytes.size()||!read(base+region.rva,bytes.data(),region.size))return false;
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);
            value-=static_cast<std::uint32_t>(base)-0x400000u;
            std::memcpy(bytes.data()+offset,&value,4);
        }
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    return true;
}
bool bodies_match(HMODULE module) noexcept {
    const auto& regions=a2fo::refit_scene::profile::mesh_body_regions;
    return regions_match(module,regions,sizeof(regions)/sizeof(regions[0]));
}
}
namespace a2fo::refit_mesh_capture {
bool owns_calls() noexcept {return active&&image&&context_matches(image);}
bool draw_source_current() noexcept {
    const auto& regions=a2fo::refit_scene::profile::mesh_draw_regions;
    return GetCurrentThreadId()==owner&&owns_calls()&&bodies_match(image)&&
        regions_match(image,regions,sizeof(regions)/sizeof(regions[0]));
}
bool initialize(HMODULE module,Observe callback,Log log,Scope scope) noexcept {
    if(owner||!module||!callback)return false;
    if(!IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)||
       !native11_capture::exact_engine_image(module)||!context_matches(module)||!bodies_match(module))return false;
    const auto base=reinterpret_cast<std::uintptr_t>(module);
    // Called at the existing externally serialized pre-workspace boundary.
    // Pin originals before any call is changed, so partial installs pass through.
    a2fo_refit_mesh_fast_original=base+originals[0];
    a2fo_refit_mesh_legacy_original=base+originals[1];
    owner=GetCurrentThreadId();image=module;observe=callback;notify_scope=scope;
    for(unsigned i=0;i<2;++i){const auto expected=native_call(i);
        if(!a2fo_rs_windows_patch_call(reinterpret_cast<void*>(base+sites[i]),
            reinterpret_cast<void*>(target(i)),expected.data(),expected.size())){
            if(log)log("[RefitMesh] checked pre-batch observer install refused; taps remain pass-through");
            return false;
        }
        installed[i]=true;
    }
    active=true;
    if(log)log("[RefitMesh] pre-batch per-mesh observers armed; all native submissions retained");
    return true;
}
}
extern "C" void __cdecl a2fo_refit_observe_mesh(void* mesh,std::uint32_t pass,
    void* material,std::uint32_t route) noexcept {
    if(GetCurrentThreadId()!=owner||route>1||!active||!observe)return;
    const bool qualified=a2fo::refit_mesh_capture::owns_calls();
    try{if(notify_scope)notify_scope(true,mesh,pass,material,route,qualified);
        if(qualified)observe(mesh,pass,material,route);}catch(...){}
}
extern "C" void __cdecl a2fo_refit_mesh_finished(std::uint32_t route) noexcept {
    if(GetCurrentThreadId()!=owner||route>1||!active||!notify_scope)return;
    // A begin with lost call ownership still creates an explicitly unsupported
    // scope. Always close it, so it cannot inherit its parent's draw identity.
    try{notify_scope(false,nullptr,0,nullptr,route,false);}catch(...){}
}
