#include "native11_sprites.hpp"
#include "native11_sprite_profile.hpp"
#include "native11_capture.hpp"
#include "native11_textures.hpp"
#include "native11_world_pipeline.hpp"
#include "../rust/native_hooks_ffi.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <utility>
extern "C" {std::uintptr_t a2fo_native11_sprite_end_scene_original=0;}
namespace a2fo::native11_sprites {
namespace {
static_assert(sizeof(void*)==4,"Pinned sprite observer is i686");
#if defined(A2FO_NATIVE11_SPRITE_TESTING)
constexpr std::size_t draw_limit=8,texture_limit=4;
constexpr std::uint64_t texture_limit_bytes=256;
#else
constexpr std::size_t draw_limit=4096,texture_limit=256;
constexpr std::uint64_t texture_limit_bytes=128ull*1024*1024;
#endif
constexpr std::array<std::uint8_t,6> sprite_call{{0xff,0x92,0xc4,0,0,0}};
constexpr std::array<std::uint8_t,6> end_scene_call{{0xff,0x91,0x8c,0,0,0}};
struct Call {std::uint32_t rva;const std::array<std::uint8_t,6>* bytes;void(*tap)();bool installed=false;};
std::array<Call,2> calls{{{0x23b486,&sprite_call,a2fo_native11_sprite_dispatch_tap},
    {0x224719,&end_scene_call,a2fo_native11_sprite_end_scene_tap}}};
HMODULE image=nullptr,fleet=nullptr;bool fleet_route=false;
bool fleet_dimensions=false;
HMODULE dimension_fleet=nullptr;
RefreshOwnerCheck refresh_owner=nullptr;
constexpr std::uint32_t dimension_nops_rva=0x221636;
constexpr std::size_t dimension_nops_size=44;
constexpr std::uint32_t bloom_entry_rva=0x224710;
constexpr std::size_t bloom_entry_size=7;
volatile LONG enabled=0;std::atomic<Log> logger{nullptr};
SRWLOCK lock=SRWLOCK_INIT;
struct Held {Held(){AcquireSRWLockExclusive(&lock);}~Held(){ReleaseSRWLockExclusive(&lock);}};
DWORD owner=0;std::uint64_t last_generation=0,events=0;
std::shared_ptr<Trace> pending;
std::shared_ptr<const Trace> published;
std::map<std::uint64_t,const native11::TextureAsset*> textures;
std::uint64_t texture_bytes=0;
std::string failure;bool invalid=true;
thread_local bool observing=false;
struct Callback {Callback(){observing=true;}~Callback(){observing=false;}};
void fail(const char* why) noexcept {invalid=true;try{failure=why;}catch(...){failure.clear();}published.reset();}
void report(const char* text) noexcept {try{if(auto call=logger.load())call(text);}catch(...){}}
bool read(std::uint32_t address,void* out,std::size_t size) noexcept {
    if(!address||!size||size-1>UINT32_MAX-address)return false;
    SIZE_T copied=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),out,size,&copied)&&copied==size;
}
bool word(std::uint32_t address,std::uint32_t offset,std::uint32_t& out) noexcept {
    return address&&offset<=UINT32_MAX-3&&address<=UINT32_MAX-offset-3&&read(address+offset,&out,4);
}
bool reader(void*,std::uint32_t address,void* out,std::size_t size){return read(address,out,size);}
std::shared_ptr<const native11::TextureAsset> resolve(void*,std::uint32_t address){return native11_textures::current(address);}
bool branch(HMODULE module,const Call& call,void(*target)()) noexcept {
    if(!module||!target)return false;
    const auto base=reinterpret_cast<std::uintptr_t>(module);
    std::array<std::uint8_t,6> bytes{};std::int32_t relative=0;
    if(!read(static_cast<std::uint32_t>(base+call.rva),bytes.data(),bytes.size())||bytes[0]!=0xe8||bytes[5]!=0x90)return false;
    std::memcpy(&relative,bytes.data()+1,4);
    return static_cast<std::uint32_t>(base+call.rva+5+relative)==reinterpret_cast<std::uintptr_t>(target);
}
bool fleet_matches(HMODULE module) noexcept {
    if(!module||!profile::fleet_callback_available)return false;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    const auto& region=profile::fleet_callback;std::array<std::uint8_t,512> bytes{};
    if(region.rva>UINT32_MAX-base||region.size>bytes.size()||!read(base+region.rva,bytes.data(),region.size))return false;
    const auto delta=base-profile::fleet_preferred_base;
    for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
        if(offset+4u>region.size)return false;
        std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;std::memcpy(bytes.data()+offset,&value,4);}
    return !std::memcmp(bytes.data(),region.bytes,region.size);
}
using Target=void(*)();
Target fleet_target(HMODULE module) noexcept {
    return reinterpret_cast<void(*)()>(reinterpret_cast<std::uintptr_t>(module)+profile::fleet_callback.rva);
}
bool matches(HMODULE module) noexcept {
    if(!module)return false;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module)),delta=base-profile::preferred_base;
    if(fleet_route&&(!fleet_matches(fleet)||
       a2fo_native11_sprite_end_scene_original!=reinterpret_cast<std::uintptr_t>(fleet_target(fleet))))return false;
    for(const auto& region:profile::regions){std::array<std::uint8_t,512> bytes{};
        if(region.rva>UINT32_MAX-base||region.size>bytes.size()||!read(base+region.rva,bytes.data(),region.size))return false;
        for(const auto& call:calls)if(call.rva>=region.rva&&call.rva+6<=region.rva+region.size){
            if(call.installed){if(module!=image||!branch(module,call,call.tap))return false;
                std::memcpy(bytes.data()+call.rva-region.rva,call.bytes->data(),6);
            }else if(call.rva==0x224719&&fleet_route){
                if(!branch(module,call,fleet_target(fleet)))return false;
                std::memcpy(bytes.data()+call.rva-region.rva,call.bytes->data(),6);
            }
        }
        if(bloom_entry_rva>=region.rva&&bloom_entry_rva+bloom_entry_size<=region.rva+region.size){
            const auto offset=bloom_entry_rva-region.rva;
            if(std::memcmp(bytes.data()+offset,region.bytes+offset,bloom_entry_size)){
                if(!refresh_owner||!refresh_owner(module))return false;
                std::memcpy(bytes.data()+offset,region.bytes+offset,bloom_entry_size);
            }
        }
        const bool nop_region=fleet_dimensions&&dimension_nops_rva>=region.rva&&
            dimension_nops_rva+dimension_nops_size<=region.rva+region.size;
        if(nop_region){
            for(std::size_t i=0;i<dimension_nops_size;++i)
                if(bytes[dimension_nops_rva-region.rva+i]!=0x90)return false;
        }
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;std::memcpy(bytes.data()+offset,&value,4);}
        // Restore the file comparison bytes after relocation normalization:
        // Fleet's known NOP range contains two original HIGHLOW operands.
        if(nop_region)std::memcpy(bytes.data()+dimension_nops_rva-region.rva,
            region.bytes+dimension_nops_rva-region.rva,dimension_nops_size);
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    for(const auto& slot:profile::slots){std::uint32_t value=0;
        if(slot.rva>UINT32_MAX-base||slot.target_rva>UINT32_MAX-base||
           !read(base+slot.rva,&value,4)||value!=base+slot.target_rva)return false;}
    return true;
}
void clear_pending() noexcept {pending.reset();textures.clear();texture_bytes=0;}
bool current_device(std::uint32_t camera,std::uint32_t& device) noexcept {
    if(!image)return false;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image));
    std::uint32_t engine=0,view=0,index=0;
    return word(base,0x3ad508,engine)&&word(engine,0xfc,view)&&view==camera&&
        word(engine,0xc0,index)&&index<16&&word(engine,0xcc+4*index,device)&&device;
}
bool target_size(std::uint32_t device,float (&size)[2]) noexcept {
    // Native DeviceDirectX8::Open forwards this unchanged to 0x6215f0,
    // which stores the mode/window pixel dimensions at +0x84/+0x88.
    // +0x10/+0x14 are not those dimensions (zero in the live menu).
    return device<=UINT32_MAX-0x8b&&read(device+0x84,size,sizeof(size))&&
        std::isfinite(size[0])&&std::isfinite(size[1])&&size[0]>=1&&size[1]>=1&&size[0]<=16384&&size[1]<=16384;
}
}
void disable() noexcept {
    InterlockedExchange(&enabled,0);Held held;clear_pending();published.reset();failure.clear();invalid=true;owner=0;last_generation=0;
}
bool healthy() noexcept {
    return InterlockedCompareExchange(&enabled,0,0)&&image&&matches(image);
}
bool initialize(HMODULE module,HMODULE fleet_ops,bool requested,Log log,RefreshOwnerCheck owner_check) noexcept {
    disable();logger.store(log);refresh_owner=owner_check;
    if(!requested)return false;
    if(!module||(image&&image!=module)||!IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)||
       !native11_capture::exact_engine_image(module))return false;
    if(!calls[0].installed&&!calls[1].installed){
        fleet_dimensions=false;dimension_fleet=nullptr;
        std::array<std::uint8_t,dimension_nops_size> code{};
        if(read(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module))+dimension_nops_rva,code.data(),code.size())){
            bool all_nops=true;for(auto byte:code)all_nops=all_nops&&byte==0x90;
            if(all_nops){
                if(!fleet_ops||!profile::fleet_callback_available||
                   !native11_capture::exact_image_file(fleet_ops,profile::fleet_sha256.data()))return false;
                HMODULE retained=nullptr;
                if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                    reinterpret_cast<const char*>(fleet_ops),&retained)||retained!=fleet_ops)return false;
                fleet_dimensions=true;dimension_fleet=fleet_ops;
            }
        }
    }else if(fleet_dimensions&&fleet_ops!=dimension_fleet)return false;
    if(!calls[1].installed){
        const auto target=fleet_ops?fleet_target(fleet_ops):nullptr;
        if(fleet_ops&&branch(module,calls[1],target)){
            if(!native11_capture::exact_image_file(fleet_ops,profile::fleet_sha256.data())||!fleet_matches(fleet_ops))return false;
            fleet=fleet_ops;fleet_route=true;calls[1].tap=a2fo_native11_sprite_fleet_end_scene_tap;
            a2fo_native11_sprite_end_scene_original=reinterpret_cast<std::uintptr_t>(target);
        }else{fleet=nullptr;fleet_route=false;calls[1].tap=a2fo_native11_sprite_end_scene_tap;}
    }else if(fleet_route&&fleet_ops!=fleet)return false;
    if(!matches(module)){report("Native11 sprite trace refused: checked CPU dispatch/profile ownership unavailable");return false;}
    image=module;
    for(auto& call:calls)if(!call.installed){
        std::array<std::uint8_t,6> expected=*call.bytes;
        if(call.rva==0x224719&&fleet_route){
            expected[0]=0xe8;expected[5]=0x90;
            const auto relative=static_cast<std::uint32_t>(a2fo_native11_sprite_end_scene_original-
                (reinterpret_cast<std::uintptr_t>(module)+call.rva+5));std::memcpy(expected.data()+1,&relative,4);
        }
        const bool patched=a2fo_rs_windows_patch_call(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(module)+call.rva),
            reinterpret_cast<void*>(call.tap),expected.data(),expected.size());
        call.installed=branch(module,call,call.tap);
        if(!patched||!call.installed)return false;
    }
    if(!matches(module))return false;
    InterlockedExchange(&enabled,1);
    report("Native11 sprite CPU tracing armed: checked ClipAndRender2D route only; native drawing/presentation unchanged");return true;
}
void begin_ui(const native11::Frame& frame,std::uint32_t ticket) noexcept {
    if(!InterlockedCompareExchange(&enabled,0,0))return;
    try {
        Held held;published.reset();clear_pending();failure.clear();invalid=false;
        if(observing||!healthy()||!ticket||ticket!=native11_world_pipeline::source_epoch()||
           !frame.camera||!frame.generation||frame.generation<=last_generation||
           (owner&&owner!=GetCurrentThreadId())){fail("sprite frame identity, owner or source unavailable");return;}
        for(float value:frame.view.viewport)if(!std::isfinite(value)||std::fabs(value)>16384){fail("invalid sprite world viewport");return;}
        if(frame.view.viewport[2]<=0||frame.view.viewport[3]<=0){fail("empty sprite world viewport");return;}
        std::uint32_t device=0;float size[2]{};
        if(!current_device(frame.camera,device)||!target_size(device,size)){fail("sprite native target identity unavailable");return;}
        auto next=std::make_shared<Trace>();next->camera=frame.camera;next->device=device;
        next->generation=frame.generation;next->source_ticket=ticket;
        std::memcpy(next->world_viewport,frame.view.viewport,sizeof(next->world_viewport));
        std::memcpy(next->native_target_size,size,sizeof(size));
        owner=GetCurrentThreadId();last_generation=frame.generation;pending=std::move(next);
    }catch(...){Held held;clear_pending();fail("sprite frame allocation failed");}
}
void abort_ui() noexcept {Held held;clear_pending();fail("main world capture aborted");}
std::shared_ptr<const Trace> latest(std::string& error) noexcept {
    try {Held held;
        if(!healthy()){
            clear_pending();fail("sprite dispatch/profile ownership revoked");
            error=failure;return nullptr;
        }
        if(!published){error=failure.empty()?"checked sprite trace unavailable":failure;return nullptr;}
        if(published->source_ticket!=native11_world_pipeline::source_epoch()){
            published.reset();error="sprite frame source ticket expired";return nullptr;
        }
        for(const auto& sprite:published->sprites)if(sprite->texture&&native11_textures::current(sprite->texture)!=sprite->texture_owner){
            published.reset();error="sprite texture version retired or replaced";return nullptr;}
        error.clear();return published;
    }catch(...){try{error="sprite trace snapshot failure";}catch(...){error.clear();}return nullptr;}
}
void observe_dispatch(void* device,const std::uint32_t* arguments,
    std::uint32_t control,void* dispatch_table) noexcept {
    if(!InterlockedCompareExchange(&enabled,0,0))return;
    if(observing){Held held;if(pending)fail("reentrant sprite observation");return;}
    Callback callback;std::shared_ptr<Trace> destination;
    try {
        {Held held;
            if(!pending||invalid)return;
            if(owner!=GetCurrentThreadId()||!healthy()||pending->source_ticket!=native11_world_pipeline::source_epoch()){
                fail("sprite source identity revoked");return;}
            if(pending->sprites.size()>=draw_limit){fail("sprite trace draw budget exceeded");return;}
            destination=pending;
        }
        const auto address=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(device));
        std::array<std::uint32_t,7> args{};
        bool valid=address==destination->device&&read(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(arguments)),args.data(),sizeof(args));
        native11::SpriteDispatch request;request.device=address;
        request.dispatch_table=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(dispatch_table));
        request.sprite=args[0];request.position=args[1];request.size=args[2];
        std::memcpy(request.uv,args.data()+3,sizeof(request.uv));request.native_x87_control=control;
        const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image));
        std::shared_ptr<const native11::SpriteObservation> observation;std::string error;
        if(valid)valid=native11::capture_sprite_observation(request,{base+0x2bc6ac,base+0x2bcb34,base+0x225590},
            nullptr,reader,nullptr,resolve,observation,error);
        float size[2]{};
        if(valid)valid=target_size(address,size)&&!std::memcmp(size,destination->native_target_size,sizeof(size));
        Held held;
        if(pending!=destination)return;
        if(!valid||!healthy()||destination->source_ticket!=native11_world_pipeline::source_epoch()){
            fail(error.empty()?"sprite source/target changed during observation":error.c_str());return;}
        if(observation->texture_owner){const auto& asset=observation->texture_owner;
            const auto found=textures.find(asset->serial);
            if(!asset->serial||(found!=textures.end()&&found->second!=asset.get())){fail("sprite texture version identity conflict");return;}
            if(found==textures.end()){
                if(textures.size()>=texture_limit||asset->bytes->size()>texture_limit_bytes-texture_bytes){fail("sprite trace texture budget exceeded");return;}
                textures.emplace(asset->serial,asset.get());texture_bytes+=asset->bytes->size();
            }
        }
        destination->sprites.push_back(std::move(observation));
    }catch(...){Held held;if(pending==destination)fail("sprite observation allocation failed");}
}
void end_scene(void* wrapper,void* native_device,void* dispatch_table) noexcept {
    if(!InterlockedCompareExchange(&enabled,0,0))return;
    if(observing){Held held;if(pending)fail("reentrant sprite frame boundary");return;}
    Callback callback;char message[384]{};
    try {
        {Held held;
            if(!pending){
                if(published&&published->device==reinterpret_cast<std::uintptr_t>(wrapper))published.reset();
                return;
            }
            const auto device=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(wrapper));
            if(device!=pending->device)return; // A different native device cannot seal this view.
            std::uint32_t actual=0,table=0;float size[2]{};
            if(owner!=GetCurrentThreadId()||!healthy()||pending->source_ticket!=native11_world_pipeline::source_epoch()||
               !word(device,0x90,actual)||actual!=reinterpret_cast<std::uintptr_t>(native_device)||
               !word(actual,0,table)||table!=reinterpret_cast<std::uintptr_t>(dispatch_table)||
               !target_size(device,size)||std::memcmp(size,pending->native_target_size,sizeof(size)))
                fail("sprite end boundary, native target or source changed");
            for(const auto& sprite:pending->sprites)if(sprite->texture&&native11_textures::current(sprite->texture)!=sprite->texture_owner){
                fail("sprite texture retired before frame seal");break;}
            const auto generation=pending->generation;const auto count=pending->sprites.size();
            const bool good=!invalid;
            if(good)published=std::move(pending);
            if(events!=UINT64_MAX)++events;
            if(events<=8||events%300==0)std::snprintf(message,sizeof(message),
                "Native11 sprite trace: frame=%llu sprites=%u state=%s reason=%.140s; ClipAndRender2D source route only, full HUD/text/minimap coverage unproven",
                static_cast<unsigned long long>(generation),static_cast<unsigned>(count),good?"captured":"unavailable",failure.c_str());
            clear_pending();
        }
        if(message[0])report(message);
    }catch(...){Held held;clear_pending();fail("sprite frame sealing failed");}
}
} // namespace a2fo::native11_sprites
extern "C" void a2fo_native11_observe_sprite(void* device,const std::uint32_t* arguments,
    std::uint32_t control,void* table) noexcept {a2fo::native11_sprites::observe_dispatch(device,arguments,control,table);}
extern "C" void a2fo_native11_end_sprite_ui(void* wrapper,void* device,void* table) noexcept {
    a2fo::native11_sprites::end_scene(wrapper,device,table);
}
