#include "native11_world_lights.hpp"
#include "native11_world_light_profile.hpp"
#include "native11_light_classes.hpp"
#include "native11_capture.hpp"
#include <array>
#include <cstring>
namespace a2fo::native11_world_lights {
namespace {
SRWLOCK lock=SRWLOCK_INIT;
HMODULE image=nullptr;bool wanted=false,armed=false;
DWORD owner_thread=0;
std::uint64_t lifecycle=0;
struct Held {Held(){AcquireSRWLockExclusive(&lock);}~Held(){ReleaseSRWLockExclusive(&lock);}};
bool read(void*,std::uint32_t address,void* output,std::size_t size){SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),output,size,&copied)&&copied==size;
}
bool matches(HMODULE module) noexcept {
    if(!module)return false;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    const auto delta=base-profile::preferred_base;
    for(const auto& region:profile::regions){std::array<std::uint8_t,512> bytes{};
        if(region.size>bytes.size()||!read(nullptr,base+region.rva,bytes.data(),region.size))return false;
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;std::memcpy(bytes.data()+offset,&value,4);
        }
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    return true;
}
std::shared_ptr<const native11::LightClassRecord> pin(void*,std::uint64_t id){return native11_light_classes::pin(id);}
}
bool initialize(HMODULE module,bool request,Log log) noexcept {
    const bool valid=request&&native11_capture::exact_engine_image(module)&&matches(module)&&native11_light_classes::requested();
    bool admitted=false;
    {Held held;wanted=request;image=module;armed=valid&&lifecycle!=UINT64_MAX;admitted=armed;
        owner_thread=GetCurrentThreadId();if(lifecycle!=UINT64_MAX)++lifecycle;}
    try{if(log&&request)log(valid?"Native11 complete world-light reader armed; no graphics takeover":"Native11 world lights unavailable: image/profile or class producer mismatch");}catch(...){}
    return admitted;
}
bool requested() noexcept {Held held;return wanted;}
bool enabled() noexcept {Held held;return armed;}
void disable() noexcept {Held held;armed=false;if(lifecycle!=UINT64_MAX)++lifecycle;}
bool current(const Snapshot& snapshot) noexcept {
    if(!snapshot.camera||!snapshot.frame_generation||!snapshot.source_lifecycle)return false;
    HMODULE module=nullptr;std::uint64_t ticket=0;
    {Held held;if(!armed||snapshot.source_lifecycle!=lifecycle)return false;module=image;ticket=lifecycle;}
    if(!matches(module)||!native11_light_classes::healthy()||
       snapshot.class_generation!=native11_light_classes::generation())return false;
    Held held;return armed&&module==image&&ticket==lifecycle;
}
bool sample(std::uint32_t camera,std::uint64_t generation,const float ambient[3],
    std::shared_ptr<const Snapshot>& output,std::string& error) noexcept {
    try {
        HMODULE module=nullptr;std::uint64_t ticket=0;
        {Held held;if(!armed||!camera||!generation||GetCurrentThreadId()!=owner_thread){error="world-light reader unavailable or foreign thread";return false;}module=image;ticket=lifecycle;}
        if(!matches(module)||!native11_light_classes::healthy()){error="world-light profile/class store unavailable";return false;}
        const auto class_epoch=native11_light_classes::generation();
        auto pending=std::make_shared<Snapshot>();pending->camera=camera;pending->frame_generation=generation;pending->class_generation=class_epoch;pending->source_lifecycle=ticket;
        const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
        if(!native11::capture_world_lights({base+0x361084},nullptr,read,nullptr,pin,pending->captured,error)||
           !native11::assemble_world_lighting(pending->captured.sources,true,ambient,pending->lighting,error))return false;
        if(!native11_light_classes::healthy()||class_epoch!=native11_light_classes::generation()){
            error="light classes changed during sampling";return false;
        }
        {Held held;if(!armed||ticket!=lifecycle){error="world-light lifecycle changed during sampling";return false;}
            output=std::move(pending);}
        error.clear();return true;
    }catch(...){try{error="world-light sampling allocation failure";}catch(...){}return false;}
}
}
