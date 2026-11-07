#include "native11_lifecycle.hpp"
#include "native11_lifecycle_profile.hpp"
#include "native11_capture.hpp"
#include "native11_assets.hpp"
#include "native11_textures.hpp"
#include "native11_world_pipeline.hpp"
#include "../rust/native_hooks_ffi.h"
#include <array>
#include <cstring>

extern "C" {
std::uintptr_t a2fo_native11_model_destroy_original=0;
std::uintptr_t a2fo_native11_texture_destroy_original=0;
}
namespace a2fo::native11_lifecycle {
namespace {
static_assert(sizeof(void*)==4,"Native lifecycle observer is i686");
constexpr std::array<std::uint8_t,5> prolog{{0x55,0x8b,0xec,0x6a,0xff}};
struct Entry {std::uint32_t rva;void(*tap)();std::uintptr_t* original;void* trampoline=nullptr;bool installed=false;};
std::array<Entry,2> entries{{
    {0x220690,a2fo_native11_model_destroy_tap,&a2fo_native11_model_destroy_original},
    {0x2427d0,a2fo_native11_texture_destroy_tap,&a2fo_native11_texture_destroy_original}}};
HMODULE image=nullptr;volatile LONG wanted=0,active=0;Log logger=nullptr;
HMODULE fleet=nullptr;
std::uintptr_t fleet_original=0;
bool fleet_model=false,fleet_slot_installed=false;
static_assert(lifecycle_profile::regions.size()==2,"Two pinned destructor entries");
void report(const char* text) noexcept {try{if(logger)logger(text);}catch(...){}}
bool read(const void* address,void* out,std::size_t size) noexcept {
    SIZE_T copied=0;return address&&ReadProcessMemory(GetCurrentProcess(),address,out,size,&copied)&&copied==size;
}
bool executable(std::uintptr_t address,std::size_t size) noexcept {
    if(!address||!size||size-1>UINTPTR_MAX-address)return false;
    const auto end=address+size;
    while(address<end){
        MEMORY_BASIC_INFORMATION page{};
        if(VirtualQuery(reinterpret_cast<void*>(address),&page,sizeof(page))!=sizeof(page)||page.State!=MEM_COMMIT||
           (page.Protect!=PAGE_EXECUTE&&page.Protect!=PAGE_EXECUTE_READ&&
            page.Protect!=PAGE_EXECUTE_READWRITE&&page.Protect!=PAGE_EXECUTE_WRITECOPY))return false;
        const auto next=reinterpret_cast<std::uintptr_t>(page.BaseAddress)+page.RegionSize;
        if(next<=address)return false;
        address=next;
    }
    return true;
}
bool fleet_profile(HMODULE module) noexcept {
    if(!module||!fleet||!lifecycle_profile::fleet_available||!fleet_original)return false;
    const auto base=reinterpret_cast<std::uintptr_t>(module);
    const auto fb=reinterpret_cast<std::uintptr_t>(fleet);
    const auto& p=lifecycle_profile::fleet_wrapper;
    std::array<std::uint8_t,512> code{};
    if(p.size>code.size()||!read(reinterpret_cast<void*>(fb+p.rva),code.data(),p.size))return false;
    const auto delta=static_cast<std::uint32_t>(fb)-lifecycle_profile::fleet_preferred_base;
    for(std::size_t i=0;i<p.relocation_count;++i){
        const auto offset=p.relocations[i];if(offset+4u>p.size)return false;
        std::uint32_t value=0;std::memcpy(&value,code.data()+offset,4);value-=delta;
        std::memcpy(code.data()+offset,&value,4);
    }
    if(std::memcmp(code.data(),p.bytes,p.size))return false;
    std::array<std::uint8_t,6> entry{};
    if(!read(reinterpret_cast<void*>(base+entries[0].rva),entry.data(),entry.size())||
       entry[0]!=0x68||entry[5]!=0xc3)return false;
    std::uint32_t target=0;std::memcpy(&target,entry.data()+1,4);
    if(target!=fb+lifecycle_profile::fleet_wrapper_rva)return false;
    // Fleet's saved entry copies ten complete native prolog bytes, then uses
    // push/ret to continue at native+10. Observe its terminal indirect call,
    // rather than the reference-counting wrapper's public release entry.
    std::array<std::uint8_t,16> stub{};
    if(!executable(fleet_original,stub.size())||!read(reinterpret_cast<void*>(fleet_original),stub.data(),stub.size()))return false;
    std::array<std::uint8_t,10> expected{};
    std::memcpy(expected.data(),lifecycle_profile::regions[0].bytes,expected.size());
    std::uint32_t handler=0;std::memcpy(&handler,expected.data()+6,4);
    handler+=static_cast<std::uint32_t>(base)-lifecycle_profile::preferred_base;
    std::memcpy(expected.data()+6,&handler,4);
    if(std::memcmp(stub.data(),expected.data(),expected.size())||stub[10]!=0x68||stub[15]!=0xc3)return false;
    std::memcpy(&target,stub.data()+11,4);
    if(target!=base+entries[0].rva+10)return false;
    std::uintptr_t slot=0;
    if(!read(reinterpret_cast<void*>(fb+lifecycle_profile::fleet_original_slot_rva),&slot,4))return false;
    return slot==(fleet_slot_installed?reinterpret_cast<std::uintptr_t>(entries[0].tap):fleet_original)&&
        (!fleet_slot_installed||a2fo_native11_model_destroy_original==fleet_original);
}
bool owned(const Entry& entry) noexcept {
    if(!image||!entry.installed||!entry.trampoline||!*entry.original||
       *entry.original!=reinterpret_cast<std::uintptr_t>(entry.trampoline))return false;
    const auto site=reinterpret_cast<std::uintptr_t>(image)+entry.rva;
    std::array<std::uint8_t,5> jump{};std::int32_t relative=0;
    if(!read(reinterpret_cast<void*>(site),jump.data(),jump.size())||jump[0]!=0xe9)return false;
    std::memcpy(&relative,jump.data()+1,4);
    if(static_cast<std::uint32_t>(site+5+relative)!=reinterpret_cast<std::uintptr_t>(entry.tap))return false;
    std::array<std::uint8_t,10> code{};
    if(!read(entry.trampoline,code.data(),code.size())||
       std::memcmp(code.data(),prolog.data(),prolog.size())||code[5]!=0xe9)return false;
    std::memcpy(&relative,code.data()+6,4);
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(entry.trampoline)+10+relative)==site+5;
}
bool matches(HMODULE module) noexcept {
    if(!module)return false;
    const auto base=reinterpret_cast<std::uintptr_t>(module);
    const auto delta=static_cast<std::uint32_t>(base)-lifecycle_profile::preferred_base;
    for(std::size_t n=0;n<lifecycle_profile::regions.size();++n){
        const auto& region=lifecycle_profile::regions[n];std::array<std::uint8_t,512> bytes{};
        if(region.rva!=entries[n].rva||region.size<5||region.size>bytes.size()||
           !read(reinterpret_cast<void*>(base+region.rva),bytes.data(),region.size))return false;
        if(n==0&&fleet_model){
            if(!fleet_profile(module))return false;
            std::memcpy(bytes.data(),region.bytes,6);
        }else if(entries[n].installed){if(module!=image||!owned(entries[n]))return false;
            std::memcpy(bytes.data(),prolog.data(),prolog.size());}
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;
            std::memcpy(&value,bytes.data()+offset,4);value-=delta;
            std::memcpy(bytes.data()+offset,&value,4);}
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    return true;
}
bool install_fleet_model() noexcept {
    if(fleet_slot_installed)return fleet_profile(image);
    auto* slot=reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(fleet)+
        lifecycle_profile::fleet_original_slot_rva);
    MEMORY_BASIC_INFORMATION page{};
    if(reinterpret_cast<std::uintptr_t>(slot)%4||
       VirtualQuery(slot,&page,sizeof(page))!=sizeof(page)||page.State!=MEM_COMMIT||
       page.Type!=MEM_IMAGE||page.AllocationBase!=fleet||
       (page.Protect!=PAGE_READWRITE&&page.Protect!=PAGE_WRITECOPY))return false;
    // This opt-in observer is installed before engine construction, never into
    // an already running session. The aligned slot swap is atomic; no Fleet
    // or native executable bytes are rewritten for this route.
    std::uint32_t engine=0;
    if(!read(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(image)+0x3ad508),&engine,4)||engine)return false;
    HMODULE retained=nullptr;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<const char*>(fleet),&retained)||retained!=fleet)return false;
    a2fo_native11_model_destroy_original=fleet_original;
    auto* previous=InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(entries[0].tap),
        reinterpret_cast<void*>(fleet_original));
    if(previous!=reinterpret_cast<void*>(fleet_original))return false;
    fleet_slot_installed=true;
    return fleet_profile(image);
}
bool install(Entry& entry) noexcept {
    if(entry.installed)return owned(entry);
    const auto site=reinterpret_cast<std::uintptr_t>(image)+entry.rva;
    auto* code=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,10,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!code)return false;
    std::memcpy(code,prolog.data(),prolog.size());code[5]=0xe9;
    const auto relative=static_cast<std::uint32_t>(site+5-
        (reinterpret_cast<std::uintptr_t>(code)+10));
    std::memcpy(code+6,&relative,4);DWORD old=0;
    if(!VirtualProtect(code,10,PAGE_EXECUTE_READ,&old)||!FlushInstructionCache(GetCurrentProcess(),code,10)){
        VirtualFree(code,0,MEM_RELEASE);return false;}
    entry.trampoline=code;*entry.original=reinterpret_cast<std::uintptr_t>(code);
    const bool patched=a2fo_rs_windows_patch_jump(reinterpret_cast<void*>(site),
        reinterpret_cast<void*>(entry.tap),prolog.data(),prolog.size());
    // Retain a valid continuation even if the writer reports a late failure
    // after installing its branch. Never free executable code still reachable.
    entry.installed=true;
    if(!owned(entry)){
        // A failed ownership read is not proof that the writer left the site
        // untouched. Release only after a successful read proves the original
        // entry remains; otherwise retain the process-lifetime continuation.
        std::array<std::uint8_t,5> current{};
        if(read(reinterpret_cast<void*>(site),current.data(),current.size())&&current==prolog){
            entry.installed=false;entry.trampoline=nullptr;*entry.original=0;
            VirtualFree(code,0,MEM_RELEASE);
        }
        return false;
    }
    return patched;
}
}
void disable() noexcept {InterlockedExchange(&active,0);}
bool healthy(unsigned domains) noexcept {
    if(domains&~3u)return false;
    const auto required=domains&static_cast<unsigned>(InterlockedCompareExchange(&wanted,0,0));
    if(!required)return true;
    if((static_cast<unsigned>(InterlockedCompareExchange(&active,0,0))&required)!=required)return false;
    if(fleet_model&&(required&model)&&!matches(image))return false;
    for(unsigned i=0;i<2;++i)if((required&(1u<<i))&&!(i==0&&fleet_model)&&!owned(entries[i]))return false;
    return true;
}
bool initialize(HMODULE module,bool models,bool textures,Log log,HMODULE fleet_ops) noexcept {
    disable();logger=log;
    const unsigned request=(models?model:0u)|(textures?texture:0u);
    InterlockedExchange(&wanted,request);
    if(!request)return false;
    if(!entries[0].installed&&!fleet_slot_installed){
        fleet_model=false;fleet=nullptr;fleet_original=0;
        std::array<std::uint8_t,6> entry{};
        if(module&&read(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(module)+entries[0].rva),entry.data(),entry.size())&&
           entry[0]==0x68&&entry[5]==0xc3){
            if(!fleet_ops||!lifecycle_profile::fleet_available||
               !native11_capture::exact_image_file(fleet_ops,lifecycle_profile::fleet_sha256.data()))return false;
            fleet=fleet_ops;fleet_model=true;
            if(!read(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(fleet)+
                lifecycle_profile::fleet_original_slot_rva),&fleet_original,4))return false;
        }
    }else if(fleet_model&&fleet_ops!=fleet)return false;
    if((image&&image!=module)||!module||!IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)||
       !native11_capture::exact_engine_image(module)||!matches(module)){
        report("Native11 lifecycle refused: exact destructor profile or ownership unavailable");return false;}
    image=module;
    if((request&model)&&fleet_model&&!install_fleet_model())return false;
    for(unsigned i=0;i<2;++i)if((request&(1u<<i))&&!(i==0&&fleet_model)&&!install(entries[i]))return false;
    if(!matches(module))return false;
    InterlockedExchange(&active,request);
    if(fleet_model&&(request&model))report("Native11 model lifetime observer owns checked Fleet terminal cleanup slot; reference counting retained");
    report("Native11 model/texture destruction observers armed; native destruction unchanged");
    return true;
}
bool observing(unsigned kind) noexcept {
    return (kind==model||kind==texture)&&
        (static_cast<unsigned>(InterlockedCompareExchange(&active,0,0))&kind)&&healthy(kind);
}
} // namespace a2fo::native11_lifecycle

extern "C" void a2fo_native11_observe_destruction(unsigned kind,void* object) noexcept {
    if(!object||!a2fo::native11_lifecycle::observing(kind))return;
    const auto pointer=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(object));
    bool changed=false;
    if(kind==a2fo::native11_lifecycle::model)changed=a2fo::native11_assets::retire_model(pointer);
    else if(kind==a2fo::native11_lifecycle::texture)changed=a2fo::native11_textures::retire_texture(pointer);
    if(changed)a2fo::native11_world_pipeline::invalidate_sources();
}
