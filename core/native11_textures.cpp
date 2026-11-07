#include "native11_textures.hpp"
#include "native11_fofs_stream.hpp"
#include "native11_texture_profile.hpp"
#include "native11_tga_profile.hpp"
#include "native11_capture.hpp"
#include "native11_lifecycle.hpp"
#include "../renderer/native11/open_read_owner.hpp"
#include "../rust/native_hooks_ffi.h"
#include <array>
#include <cstring>
#include <cstdio>
#include <map>
#include <utility>
#include <atomic>
#if defined(A2FO_NATIVE11_TEXTURE_TESTING)
extern "C" void a2fo_native11_texture_before_commit() noexcept;
#endif
extern "C" {std::uintptr_t a2fo_native11_texture_resolver_original=0;}
extern "C" {std::uintptr_t a2fo_native11_tga_open_original=0;}
namespace {
using namespace a2fo::native11;
static_assert(sizeof(void*)==4,"Pinned resolver is i686");
constexpr unsigned char identity[32]={0x3f,0xd1,0x8e,0xee,0xe4,0x20,0x1a,0x9a,0x12,0x97,0xac,0x26,0xb3,0xeb,0x88,0xf7,0x8c,0x5f,0x72,0xe8,0xc5,0x5a,0x62,0xc2,0x51,0x79,0xf1,0xd4,0x93,0x2e,0x4c,0x11};
SRWLOCK lock=SRWLOCK_INIT;
volatile LONG enabled=0,tga_enabled=0,events=0;
bool parameters_installed=false,pixels_installed=false;
bool tga_installed=false;
std::atomic<HMODULE> tga_image{nullptr};
std::atomic<HMODULE> tga_fleet{nullptr};
std::atomic<a2fo::native11_textures::OpenReadOwnerCheck> rgb_owner{nullptr};
ResolvedTextureStore store;
std::uint64_t next_ticket=0;
std::uint64_t tga_generation=0;
std::map<std::uint32_t,std::uint64_t> pending;
std::uint64_t inflight_bytes=0;
#if defined(A2FO_NATIVE11_TEXTURE_TESTING)
constexpr std::uint64_t inflight_limit=256; // same admission policy, small fixture payloads
#else
constexpr std::uint64_t inflight_limit=128ull*1024*1024;
#endif
std::atomic<a2fo::native11_textures::Log> logger{nullptr};
struct Exclusive{Exclusive(){AcquireSRWLockExclusive(&lock);}~Exclusive(){ReleaseSRWLockExclusive(&lock);}};
struct Shared{Shared(){AcquireSRWLockShared(&lock);}~Shared(){ReleaseSRWLockShared(&lock);}};
struct IoLease {
    IoLease()=default;
    IoLease(const IoLease&)=delete;
    IoLease& operator=(const IoLease&)=delete;
    std::uint64_t bytes=0;
    ~IoLease(){if(bytes){Exclusive held;inflight_bytes-=bytes;}}
};
void report(const char* message) noexcept {try{const auto log=logger.load();if(log)log(message);}catch(...){}}
bool read(std::uint32_t address,void* output,std::size_t size) noexcept {
    if(!address||!size||size-1>UINT32_MAX-address)return false;
    SIZE_T count=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),output,size,&count)&&count==size;
}
bool word(std::uint32_t base,std::uint32_t offset,std::uint32_t& value) noexcept {
    return base<=UINT32_MAX-offset&&read(base+offset,&value,4);
}
bool ansi(std::uint32_t address,std::size_t maximum,std::string& out) {
    std::int32_t length=0;
    if(address<4||!read(address-4,&length,4)||length<=0||std::size_t(length)>maximum)return false;
    std::string text(std::size_t(length)+1,0);
    if(!read(address,text.data(),text.size())||text.back()!=0||std::memchr(text.data(),0,length))return false;
    text.resize(length);out=std::move(text);return true;
}
bool logical_name(std::uint32_t object,std::string& out) {
    std::uint32_t address=0;if(!word(object,8,address))return false;
    std::string text;
    for(std::uint32_t i=0;i<=1024;++i){char c=0;
        if(address>UINT32_MAX-i||!read(address+i,&c,1))return false;
        if(!c){if(text.empty())return false;out=std::move(text);return true;}
        if(static_cast<unsigned char>(c)<0x20||static_cast<unsigned char>(c)>0x7e)return false;
        text.push_back(c);
    }return false;
}
bool profile_matches(HMODULE module) noexcept {
    namespace p=a2fo::native11_textures::profile;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module)),delta=base-p::preferred_base;
    for(const auto& region:p::regions){
        std::array<std::uint8_t,512> bytes{};
        if(region.size>bytes.size()||!read(base+region.rva,bytes.data(),region.size))return false;
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;std::memcpy(bytes.data()+offset,&value,4);
        }
        // Already-owned CALLs are checked against their own target separately.
        for(const auto site:std::array<std::pair<std::uint32_t,bool>,2>{{{0x1efec0,parameters_installed},{0x1f0152,pixels_installed}}})
            if(site.second&&site.first>=region.rva&&site.first+5<=region.rva+region.size)
                std::memcpy(bytes.data()+site.first-region.rva,region.bytes+site.first-region.rva,5);
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }return true;
}
bool owns_call(HMODULE module,std::uint32_t rva,void(*target)()) noexcept {
    const auto base=reinterpret_cast<std::uintptr_t>(module);std::array<std::uint8_t,5> bytes{};
    if(!read(static_cast<std::uint32_t>(base+rva),bytes.data(),5)||bytes[0]!=0xe8)return false;
    std::int32_t offset=0;std::memcpy(&offset,bytes.data()+1,4);
    return static_cast<std::uint32_t>(base+rva+5+offset)==static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(target));
}
bool approved_open_read(HMODULE image) noexcept {
    std::array<std::uint8_t,6> bytes{};
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image));
    if(read(base+0x240150,bytes.data(),bytes.size())&&bytes==open_read_entry_bytes)return true;
    const auto owner=rgb_owner.load();return owner&&owner(image);
}
bool tga_profile_matches(HMODULE image) noexcept {
    namespace p=a2fo::native11_textures::tga_profile;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image)),delta=base-p::preferred_base;
    for(const auto& region:p::regions){std::array<std::uint8_t,512> bytes{};
        if(region.size>bytes.size()||!read(base+region.rva,bytes.data(),region.size))return false;
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;std::memcpy(bytes.data()+offset,&value,4);
        }
        if(tga_installed&&0x2435d1>=region.rva&&0x2435d6<=region.rva+region.size)
            std::memcpy(bytes.data()+0x2435d1-region.rva,region.bytes+0x2435d1-region.rva,5);
        if(const auto fleet=tga_fleet.load())
            if(!a2fo::native11_fofs_stream::normalize(image,fleet,region.rva,bytes.data(),region.size))return false;
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    for(const auto& slot:p::slots){std::uint32_t value=0;if(!read(base+slot[0],&value,4)||value!=base+slot[1])return false;}
    return approved_open_read(image)&&(!tga_installed||owns_call(image,0x2435d1,a2fo_native11_tga_open_tap));
}
bool file_bytes(const std::string& path,std::vector<std::uint8_t>& out,IoLease& lease) {
    struct File{HANDLE value;~File(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}} file{CreateFileA(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if(file.value==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER length{};if(!GetFileSizeEx(file.value,&length)||length.QuadPart<4||length.QuadPart>64ll*1024*1024)return false;
    {Exclusive held;const auto wanted=static_cast<std::uint64_t>(length.QuadPart);
        if(!InterlockedCompareExchange(&enabled,0,0)||wanted>inflight_limit-inflight_bytes)return false;
        inflight_bytes+=wanted;lease.bytes=wanted;}
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length.QuadPart));DWORD count=0;
    if(!ReadFile(file.value,bytes.data(),static_cast<DWORD>(bytes.size()),&count,nullptr)||count!=bytes.size()||std::memcmp(bytes.data(),"DDS ",4))return false;
    out=std::move(bytes);return true;
}
}
namespace a2fo::native11_textures {
bool initialize(HMODULE module,bool requested,Log log) noexcept {
    logger=log;if(!requested){disable();return false;}
    disable();
    if(!module||!IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)||
       !native11_capture::exact_image_file(module,identity)||!profile_matches(module)||
       (parameters_installed&&!owns_call(module,0x1efec0,a2fo_native11_texture_parameters_tap))||
       (pixels_installed&&!owns_call(module,0x1f0152,a2fo_native11_texture_pixels_tap))){
        report("Native11 DDS capture refused: pinned FleetOps image or loaded resolver ownership mismatch");return false;
    }
    const auto base=reinterpret_cast<std::uintptr_t>(module);
    a2fo_native11_texture_resolver_original=base+0x109944;
    constexpr std::uint8_t parameters[]{0xe8,0x7f,0x9a,0xf1,0xff},pixels[]{0xe8,0xed,0x97,0xf1,0xff};
    if(!parameters_installed){
        if(!a2fo_rs_windows_patch_call(reinterpret_cast<void*>(base+0x1efec0),reinterpret_cast<void*>(&a2fo_native11_texture_parameters_tap),parameters,5))return false;
        parameters_installed=true;
    }
    if(!pixels_installed){
        if(!a2fo_rs_windows_patch_call(reinterpret_cast<void*>(base+0x1f0152),reinterpret_cast<void*>(&a2fo_native11_texture_pixels_tap),pixels,5))return false;
        pixels_installed=true;
    }
    {Exclusive held;pending.clear();store.clear();InterlockedExchange(&enabled,1);}
    report("Native11 DDS resolver capture armed: owned file snapshots; no decode, graphics takeover or TGA coverage");return true;
}
void disable() noexcept {Exclusive held;InterlockedExchange(&enabled,0);InterlockedExchange(&tga_enabled,0);if(tga_generation!=UINT64_MAX)++tga_generation;pending.clear();store.clear();}
bool retire_texture(std::uint32_t texture) noexcept {
    Exclusive held;
    const bool changed=bool(store.current(texture))||pending.find(texture)!=pending.end();
    if(changed){pending.erase(texture);store.invalidate(texture);}
    return changed;
}
std::shared_ptr<const native11::TextureAsset> current(std::uint32_t texture) noexcept {
    if(!native11_lifecycle::healthy(native11_lifecycle::texture)){
        static std::atomic<unsigned> refused{0};
        if(++refused==1)report("Native11 texture lookup refused: destruction ownership no longer available");
        return nullptr;
    }
    Shared held;return (InterlockedCompareExchange(&enabled,0,0)||InterlockedCompareExchange(&tga_enabled,0,0))?store.current(texture):nullptr;
}
bool collect(std::vector<native11::SnapshotTexture>& output,std::uint64_t& generation,std::string& error) noexcept {
    if(!native11_lifecycle::healthy(native11_lifecycle::texture)){try{error="texture lifetime observation unavailable";}catch(...){error.clear();}return false;}
    Shared held;
    if(!InterlockedCompareExchange(&enabled,0,0)&&!InterlockedCompareExchange(&tga_enabled,0,0)){output.clear();generation=store.generation();error.clear();return true;}
    if(!store.collect(output,error))return false;
    generation=store.generation();return true;
}
bool collect_required(const std::vector<std::string>& names,std::vector<native11::SnapshotTexture>& output,
    std::uint64_t& generation,std::string& error) noexcept {
    if(!native11_lifecycle::healthy(native11_lifecycle::texture)){try{error="texture lifetime observation unavailable";}catch(...){error.clear();}return false;}
    Shared held;
    if(!InterlockedCompareExchange(&enabled,0,0)&&!InterlockedCompareExchange(&tga_enabled,0,0)){
        if(!names.empty()){try{error="required texture capture unavailable";}catch(...){error.clear();}return false;}
        output.clear();generation=store.generation();error.clear();return true;
    }
    if(!store.collect_required(names,output,error))return false;
    generation=store.generation();return true;
}
bool initialize_tga(HMODULE image,bool requested,Log log,OpenReadOwnerCheck owner,HMODULE fleet_ops) noexcept {
    logger=log;
    {Exclusive held;InterlockedExchange(&tga_enabled,0);
        store.clear_source(TextureSourceKind::opened_stream);
        if(tga_generation==UINT64_MAX)return false;
        ++tga_generation;
    }
    if(!requested)return false;
    rgb_owner=owner;
    tga_fleet.store(native11_fofs_stream::qualify(image,fleet_ops)?fleet_ops:nullptr);
    if(!image||!IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)||
       !native11_capture::exact_engine_image(image)||!tga_profile_matches(image)){
        report("Native11 TGA capture refused: pinned loader or OpenRead ownership mismatch");return false;
    }
    tga_image=image;
    const auto base=reinterpret_cast<std::uintptr_t>(image);
    a2fo_native11_tga_open_original=base+0x240150;
    constexpr std::uint8_t expected[]{0xe8,0x7a,0xcb,0xff,0xff};
    if(!tga_installed){
        if(!a2fo_rs_windows_patch_call(reinterpret_cast<void*>(base+0x2435d1),reinterpret_cast<void*>(&a2fo_native11_tga_open_tap),expected,5))return false;
        tga_installed=true;
    }
    {Exclusive held;InterlockedExchange(&tga_enabled,1);}
    if(tga_fleet.load())report("Native11 TGA stream retains checked Fleet search-root fopen ownership");
    report("Native11 TGA stream capture armed: owned level-zero actual-read bytes; existing RGB OpenRead owner retained");return true;
}
}
extern "C" void a2fo_native11_observe_texture_path(void* output_slot,const char* requested,
    void* caller_frame,std::uint32_t route) noexcept {
    if(!a2fo::native11_lifecycle::healthy(a2fo::native11_lifecycle::texture))return;
    if(!InterlockedCompareExchange(&enabled,0,0)||(route!=1&&route!=2))return;
    const auto frame=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(caller_frame));
    std::uint32_t texture=0;
    if((route==1&&(frame<4||!read(frame-4,&texture,4)))||
       (route==2&&!word(frame,8,texture))||!texture)return;
    std::uint64_t ticket=0;bool published=false;unsigned gates=0;
    std::string diagnostic_name,diagnostic_input;
    try {
        {Exclusive held;
            if(!InterlockedCompareExchange(&enabled,0,0))return;
            store.invalidate(texture);pending.erase(texture);
            if(next_ticket==UINT64_MAX||pending.size()>=2048)return;
            ticket=++next_ticket;pending.emplace(texture,ticket);
        }
        std::string logical,path,input;std::uint32_t address=0,flags=0;
        IoLease lease;
        // Reverse destruction order frees an aborted payload before returning
        // its reservation to another reader.
        std::uint8_t colour_key=0,async=1;std::vector<std::uint8_t> bytes;
        const auto gate=[&](bool ok,unsigned bit){if(ok)gates|=bit;return ok;};
        const bool valid=gate(logical_name(texture,logical),1)&&
            gate(ansi(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(requested)),1040,input),2)&&
            gate(input=="Textures\\"+logical+".dds",4)&&
            gate(read(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(output_slot)),&address,4)&&ansi(address,32767,path),8)&&
            gate(word(texture,0x18,flags)&&texture<=UINT32_MAX-0x58&&read(texture+0x2c,&colour_key,1)&&read(texture+0x58,&async,1),16)&&
            gate(!async,32)&&gate(file_bytes(path,bytes,lease),64);
        diagnostic_name=logical;diagnostic_input=input;
#if defined(A2FO_NATIVE11_TEXTURE_TESTING)
        if(valid)a2fo_native11_texture_before_commit();
#endif
        {Exclusive held;const auto found=pending.find(texture);
            if(found!=pending.end()&&found->second==ticket){
                pending.erase(found);
                if(valid&&a2fo::native11_lifecycle::healthy(a2fo::native11_lifecycle::texture)&&InterlockedCompareExchange(&enabled,0,0))published=store.publish(texture,std::move(logical),std::move(path),flags,colour_key!=0,std::move(bytes));
            }
        }
    }catch(...){Exclusive held;const auto found=pending.find(texture);if(found!=pending.end()&&found->second==ticket)pending.erase(found);}
    const LONG event=InterlockedIncrement(&events);
    if(event<=8||event%128==0){char message[192]{};
        std::snprintf(message,sizeof(message),"Native11 DDS source: event=%ld texture=%08lx route=%u captured=%u; resolved-file version, live material association incomplete",event,static_cast<unsigned long>(texture),route,published?1u:0u);report(message);}
    if(!published){static std::atomic<unsigned> failures{0};if(++failures<=32){char message[384]{};
        std::uint64_t resident=0;std::size_t records=0;{Shared held;resident=store.bytes();records=store.size();}
        std::snprintf(message,sizeof(message),"Native11 DDS source refused: texture=%08lx route=%u gates=%02x resident=%llu records=%u name=%.80s input=%.112s",
            static_cast<unsigned long>(texture),route,gates,static_cast<unsigned long long>(resident),unsigned(records),diagnostic_name.c_str(),diagnostic_input.c_str());report(message);}}
}
extern "C" void a2fo_native11_observe_tga_open(std::uint32_t result,void* stream,
    const char* requested,std::uint32_t buffered,void* texture_pointer,std::uint32_t level) noexcept {
    if(!a2fo::native11_lifecycle::healthy(a2fo::native11_lifecycle::texture))return;
    if(level||!texture_pointer||!InterlockedCompareExchange(&tga_enabled,0,0))return;
    const auto texture=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(texture_pointer));
    const auto image=tga_image.load();
    std::uint64_t ticket=0,generation=0;bool published=false;
    try {
        {Exclusive held;
            if(!InterlockedCompareExchange(&tga_enabled,0,0))return;
            store.invalidate(texture);pending.erase(texture);
            if(next_ticket==UINT64_MAX||pending.size()>=2048)return;
            ticket=++next_ticket;pending.emplace(texture,ticket);
            generation=tga_generation;
        }
        IoLease lease;
        OpenedFile opened;std::string logical;std::uint32_t flags=0,type=0,begin=0,end=0;
        std::uint8_t colour_key=0;
        const auto stream_address=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(stream));
        bool valid=!result&&buffered==1&&tga_profile_matches(image)&&
            logical_name(texture,logical)&&word(texture,0x18,flags)&&texture<=UINT32_MAX-0x2c&&read(texture+0x2c,&colour_key,1)&&
            word(stream_address,0,type)&&type==static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image))+0x2bc874&&
            word(stream_address,0x14,begin)&&word(stream_address,0x18,end)&&begin&&end>begin&&end-begin<=64u*1024*1024;
        if(valid){Exclusive held;const auto wanted=std::uint64_t(end-begin);
            valid=InterlockedCompareExchange(&tga_enabled,0,0)&&wanted<=inflight_limit-inflight_bytes;
            if(valid){inflight_bytes+=wanted;lease.bytes=wanted;}
        }
        if(valid){
            const auto reader=+[](void*,std::uint32_t address,void* output,std::size_t size)->bool{return read(address,output,size);};
            valid=capture_opened_file(nullptr,reader,stream_address,
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(requested)),
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image))+0x2bc874,
                static_cast<std::uint32_t>(lease.bytes),opened)==OpenedFileStatus::ok;
        }
#if defined(A2FO_NATIVE11_TEXTURE_TESTING)
        if(valid)a2fo_native11_texture_before_commit();
#endif
        {Exclusive held;const auto found=pending.find(texture);
            if(found!=pending.end()&&found->second==ticket){pending.erase(found);
                if(valid&&a2fo::native11_lifecycle::healthy(a2fo::native11_lifecycle::texture)&&InterlockedCompareExchange(&tga_enabled,0,0)&&generation==tga_generation)
                    published=store.publish_stream(texture,std::move(logical),std::move(opened.name),flags,colour_key!=0,std::move(opened.bytes));
            }
        }
    }catch(...){Exclusive held;const auto found=pending.find(texture);if(found!=pending.end()&&found->second==ticket)pending.erase(found);}
    const LONG event=InterlockedIncrement(&events);
    if(event<=8||event%128==0){char message[160]{};
        std::snprintf(message,sizeof(message),"Native11 TGA source: event=%ld texture=%08lx captured=%u; actual-read level-zero bytes; no physical-path guess",event,static_cast<unsigned long>(texture),published?1u:0u);report(message);}
}
