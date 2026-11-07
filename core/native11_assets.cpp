#include "native11_assets.hpp"
#include "native11_animation_clock.hpp"
#include "native11_asset_profile.hpp"
#include "native11_capture.hpp"
#include "native11_textures.hpp"
#include "native11_lifecycle.hpp"
#include "native11_fofs_stream.hpp"
#include "../rust/native_hooks_ffi.h"
#include "../rust/native11_model_ffi.h"
#include "../renderer/native11/dependency_plan.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <utility>

extern "C" {std::uintptr_t a2fo_native11_read_database_original=0;}
namespace {
static_assert(sizeof(void*)==4,"Pinned engine observer is i686");
using namespace a2fo::native11;
volatile LONG enabled=0,events=0;
bool installed=false;
HMODULE installed_image=nullptr; // startup serialized; hook remains installed
HMODULE file_open_fleet=nullptr;
std::uint32_t stream_vtable=0,database_vtable=0;
SRWLOCK lock=SRWLOCK_INIT;
ModelAssetStore store;
SRWLOCK dependency_lock=SRWLOCK_INIT;
ModelDependencyCache dependency_cache;
std::uint64_t source_epoch=1;
bool epoch_exhausted=false;
std::uint64_t last_texture_generation=0;
void advance_epoch() noexcept {
    if(source_epoch==UINT64_MAX)epoch_exhausted=true;
    else ++source_epoch;
}
a2fo::native11_assets::Log logger=nullptr;
constexpr std::uint32_t call_rva=0x22ce4d;
constexpr std::array<std::uint8_t,5> call_bytes{{0xe8,0x0e,0x2c,0x01,0x00}};
void report(const char* message) noexcept {try{if(logger)logger(message);}catch(...){}}
bool read_local(void*,std::uint32_t address,void* output,std::size_t size){
    SIZE_T copied=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),output,size,&copied)&&copied==size;
}
bool word_at(std::uint32_t base,std::uint32_t offset,std::uint32_t& output) noexcept {
    return base&&base<=UINT32_MAX-offset-3&&read_local(nullptr,base+offset,&output,4);
}
bool model_call_owned(HMODULE image) noexcept {
    if(!image||image!=installed_image||!installed)return false;
    const auto base=reinterpret_cast<std::uintptr_t>(image);
    std::array<std::uint8_t,5> bytes{};
    if(a2fo_native11_read_database_original!=base+0x23fa60||
       !read_local(nullptr,static_cast<std::uint32_t>(base+call_rva),bytes.data(),bytes.size())||bytes[0]!=0xe8)return false;
    std::int32_t displacement=0;std::memcpy(&displacement,bytes.data()+1,4);
    return static_cast<std::uint32_t>(base+call_rva+5+displacement)==
        reinterpret_cast<std::uintptr_t>(&a2fo_native11_database_read_tap);
}
bool source_owned(HMODULE image) noexcept {
    return model_call_owned(image)&&(!file_open_fleet||a2fo::native11_fofs_stream::owned(image,file_open_fleet));
}
bool loaded_profile(HMODULE image) noexcept {
    namespace p=a2fo::native11_assets::profile;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image)),delta=base-p::preferred_base;
    for(const auto& region:p::regions){
        std::array<std::uint8_t,512> bytes{};
        if(region.size>bytes.size()||!read_local(nullptr,base+region.rva,bytes.data(),region.size))return false;
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;std::memcpy(bytes.data()+offset,&value,4);
        }
        if(file_open_fleet&&!a2fo::native11_fofs_stream::normalize(image,file_open_fleet,region.rva,bytes.data(),region.size))return false;
        if(installed&&call_rva>=region.rva&&call_rva+call_bytes.size()<=region.rva+region.size){
            if(!model_call_owned(image))return false;
            std::memcpy(bytes.data()+call_rva-region.rva,call_bytes.data(),call_bytes.size());
        }
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    for(const auto& slot:p::slots){std::uint32_t value=0;
        if(!read_local(nullptr,base+slot[0],&value,4)||value!=base+slot[1])return false;
    }
    return true;
}
bool same_database_name(std::uint32_t db,const std::string& name) noexcept {
    std::uint32_t type=0,text=0;
    if(name.size()>=260||!word_at(db,0,type)||type!=database_vtable||!word_at(db,0x38,text)||!text)return false;
    for(std::size_t i=0;i<=name.size();++i){char c=0;
        if(text>UINT32_MAX-i||!read_local(nullptr,text+static_cast<std::uint32_t>(i),&c,1)||
           c!=(i==name.size()?0:name[i]))return false;
    }
    return true;
}
struct Exclusive {Exclusive(){AcquireSRWLockExclusive(&lock);}~Exclusive(){ReleaseSRWLockExclusive(&lock);}};
struct Shared {Shared(){AcquireSRWLockShared(&lock);}~Shared(){ReleaseSRWLockShared(&lock);}};
}
namespace a2fo::native11_assets {
bool initialize(HMODULE image,bool requested,Log log,HMODULE fleet_ops) noexcept {
    logger=log;
    if(!requested){disable();return false;}
    if(installed){
        if(image!=installed_image||(file_open_fleet&&fleet_ops!=file_open_fleet)||
           !native11_capture::exact_engine_image(image)||!loaded_profile(image)){
            disable();report("Native11 model reactivation refused: load/stream ownership changed");return false;
        }
        InterlockedExchange(&enabled,1);return true;
    }
    file_open_fleet=native11_fofs_stream::qualify(image,fleet_ops)?fleet_ops:nullptr;
    if(!image||!IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)||
       !native11_capture::exact_engine_image(image)||!loaded_profile(image)){
        report("Native11 model bytes unavailable: exact engine, stream profile or model-call ownership mismatch");return false;
    }
    const auto base=reinterpret_cast<std::uintptr_t>(image);
    stream_vtable=static_cast<std::uint32_t>(base+0x2bc874);
    database_vtable=static_cast<std::uint32_t>(base+0x2bc56c);
    // Seed the continuation before installing the startup-only checked CALL.
    a2fo_native11_read_database_original=base+0x23fa60;
    if(!a2fo_rs_windows_patch_call(reinterpret_cast<void*>(base+call_rva),
        reinterpret_cast<void*>(&a2fo_native11_database_read_tap),call_bytes.data(),call_bytes.size())){
        report("Native11 model bytes unavailable: checked model-load CALL patch refused");return false;
    }
    installed_image=image;installed=true;InterlockedExchange(&enabled,1);
    if(file_open_fleet)report("Native11 model stream retains checked Fleet search-root fopen ownership");
    report("Native11 opened-model observer armed: copied actual-read bytes; no parser replay or graphics takeover");return true;
}
void disable() noexcept {InterlockedExchange(&enabled,0);Exclusive held;store.clear();advance_epoch();}
bool owns_load_call(HMODULE image) noexcept {
    return InterlockedCompareExchange(&enabled,0,0)&&source_owned(image);
}
bool retire_model(std::uint32_t db) noexcept {
    Exclusive held;ModelAssetInfo info{};
    if(!store.info(db,info))return false;
    store.invalidate(db);advance_epoch();return true;
}
bool model_info(std::uint32_t db,ModelAssetInfo& output) noexcept {
    if(!native11_lifecycle::healthy(native11_lifecycle::model))return false;
    if(!InterlockedCompareExchange(&enabled,0,0))return false;
    Shared held;return InterlockedCompareExchange(&enabled,0,0)&&store.info(db,output);
}
std::shared_ptr<const ModelAsset> pin_model(std::uint32_t db,std::uint64_t serial) noexcept {
    if(!native11_lifecycle::healthy(native11_lifecycle::model))return nullptr;
    Shared held;
    return InterlockedCompareExchange(&enabled,0,0)?store.find(db,serial):nullptr;
}
std::shared_ptr<const ModelAsset> pin_current_model(std::uint32_t db,const char* name) noexcept {
    if(!name||!native11_lifecycle::healthy(native11_lifecycle::model)||!source_owned(installed_image))return nullptr;
    Shared held;ModelAssetInfo info{};
    return InterlockedCompareExchange(&enabled,0,0)&&store.info(db,info)&&
        !std::strncmp(info.name,name,sizeof(info.name))?store.find(db,info.serial):nullptr;
}
std::uint64_t generation() noexcept {
    if(!native11_lifecycle::healthy(native11_lifecycle::model)||!source_owned(installed_image))return 0;
    Shared held;return InterlockedCompareExchange(&enabled,0,0)&&!epoch_exhausted?source_epoch:0;
}
bool bind_frame(const Frame& frame,std::shared_ptr<const BoundCapture>& output,std::string& error) noexcept {
    try {
        if(!native11_lifecycle::healthy(native11_lifecycle::model)){error="model lifetime observation unavailable";return false;}
        if(frame.object_count>capacity){error="dependency frame capacity exceeded";return false;}
        using Models=std::map<std::pair<std::uint32_t,std::uint64_t>,std::shared_ptr<const ModelAsset>>;
        Models pinned;
        {Shared held;if(!InterlockedCompareExchange(&enabled,0,0)||epoch_exhausted){error="model capture disabled or epoch exhausted";return false;}
            for(std::size_t i=0;i<frame.object_count;++i){const auto& object=frame.objects[i];
                pinned.emplace(std::make_pair(object.database,object.model_serial),store.find(object.database,object.model_serial));
            }
        }
        const auto resolve=+[](void* context,std::uint32_t db,std::uint64_t serial)->std::shared_ptr<const ModelAsset>{
            const auto& models=*static_cast<const Models*>(context);const auto found=models.find({db,serial});return found==models.end()?nullptr:found->second;
        };
        std::shared_ptr<const DependencyPlan> plan;
        // CPU parsing never holds either producer's store lock. Cache entries
        // hold weak model owners, so unload/replacement is not delayed by it.
        {struct Held{Held(){AcquireSRWLockExclusive(&dependency_lock);}~Held(){ReleaseSRWLockExclusive(&dependency_lock);}} held;
            if(!build_dependency_plan(frame,resolve,&pinned,dependency_cache,a2fo_rs_native11_parse_sod,plan,error))return false;
        }
        std::vector<SnapshotTexture> textures;std::uint64_t texture_generation=0;
        if(!a2fo::native11_textures::collect_required(plan->textures(),textures,texture_generation,error))return false;
        Exclusive held;
        if(!InterlockedCompareExchange(&enabled,0,0)||epoch_exhausted){error="model capture disabled or epoch exhausted";return false;}
        for(const auto& item:plan->models())if(store.find(item.first.first,item.first.second)!=item.second){
            error="model version changed during dependency planning";return false;
        }
        if(texture_generation!=last_texture_generation){advance_epoch();last_texture_generation=texture_generation;}
        if(epoch_exhausted){error="asset source epoch exhausted";return false;}
        return build_bound_capture(plan->frame(),source_epoch,DependencyPlan::resolve,
            const_cast<DependencyPlan*>(plan.get()),textures,output,error);
    }catch(...){try{error="dependency binding allocation or parser failure";}catch(...){error.clear();}return false;}
}
bool read_model(std::uint32_t db,std::uint64_t serial,A2FO_N11_BytesSink sink,void* context) noexcept {
    if(!native11_lifecycle::healthy(native11_lifecycle::model))return false;
    if(!sink||!InterlockedCompareExchange(&enabled,0,0))return false;
    std::shared_ptr<const ModelAsset> asset;
    {Shared held;if(InterlockedCompareExchange(&enabled,0,0))asset=store.find(db,serial);}
    if(!asset)return false;
    try{return sink(context,asset->bytes.data(),asset->info.byte_count)==1;}catch(...){return false;}
}
}
extern "C" void a2fo_native11_observe_database(void* database,void* stream,
    std::uint32_t scalar_bits,const char* name,std::uint32_t native_x87_control) noexcept {
    if(!a2fo::native11_lifecycle::healthy(a2fo::native11_lifecycle::model))return;
    if(!database||!InterlockedCompareExchange(&enabled,0,0))return;
    const auto db=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(database));
    bool published=false;std::size_t records=0;std::uint64_t resident=0;
    std::shared_ptr<const ModelAsset> template_model;
    OpenedFileStatus status=OpenedFileStatus::invalid_arguments;
    {
        // Capture/publish are serialized. Readers take a shared immutable owner
        // then release this lock before calling a sink; no foreign callback runs
        // while the store lock is held. The capture itself uses only RPM/CRT.
        Exclusive held;if(!InterlockedCompareExchange(&enabled,0,0))return;
        store.invalidate(db); // A reused address must never retain old bytes.
        advance_epoch();
        if(file_open_fleet&&!a2fo::native11_fofs_stream::owned(installed_image,file_open_fleet))return;
        OpenedFile opened;
        status=capture_consumed_file(nullptr,read_local,
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(stream)),
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(name)),stream_vtable,64u*1024*1024,opened);
        if(status==OpenedFileStatus::ok&&same_database_name(db,opened.name)&&
           (!file_open_fleet||a2fo::native11_fofs_stream::owned(installed_image,file_open_fleet)))
            published=store.publish(db,scalar_bits,std::move(opened),native_x87_control);
        if(published){ModelAssetInfo info{};
            if(store.info(db,info))template_model=store.find(db,info.serial);}
        records=store.size();resident=store.bytes();
    }
    // The existing bridge invokes us after ReadDatabase returns and before its
    // caller resumes. Capture a baseline here, never on a later instance draw.
    // No callback or additional store read runs while the asset lock is held.
    if(template_model)a2fo::native11_animation_clock::observe_model_loaded(template_model);
    const auto count=InterlockedIncrement(&events);
    if(count<=8||count%128==0){char message[256]{};
        std::snprintf(message,sizeof(message),"Native11 model bytes: event=%ld db=%08lx captured=%u stream_status=%u resident_models=%u payload_bytes=%llu; scalar metadata only",
            count,static_cast<unsigned long>(db),published?1u:0u,static_cast<unsigned>(status),static_cast<unsigned>(records),static_cast<unsigned long long>(resident));report(message);
    }
}
