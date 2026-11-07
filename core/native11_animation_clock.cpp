#include "native11_animation_clock.hpp"
#include "native11_animation_clock_profile.hpp"
#include "native11_template_profile.hpp"
#include "native11_template_fleet.hpp"
#include "native11_craft_view_profile.hpp"
#include "native11_capture.hpp"
#include "native11_nodes.hpp"
#include "native11_assets.hpp"
#include "native11_textures.hpp"
#include "native11_material_profile.hpp"
#include "../rust/native11_model_ffi.h"
#include "../renderer/native11/material_observation.hpp"
#include "../renderer/native11/animation_clock_capture.hpp"
#include "../renderer/native11/model_node_template_cache.hpp"
#include "../renderer/native11/model_material_baseline_cache.hpp"
#include "../renderer/native11/model_lod_template.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <limits>
#include <algorithm>
#include <cmath>

namespace a2fo::native11_animation_clock {
namespace {
std::atomic<HMODULE> image{nullptr};
std::atomic<HMODULE> template_fleet{nullptr};
std::atomic<animations::NativePlaybackSource> playback_source{nullptr};
std::atomic<Log> logger{nullptr};
volatile LONG active=0;
volatile LONG templates_active=0;
DWORD owner=0;bool busy=false;
std::uint64_t events=0,failures=0;
native11::ModelPoseCache pose_cache;
native11::ModelNodeTemplateCache template_cache;
native11::ModelMaterialBaselineCache material_cache;
std::uint64_t template_events=0;
// Any observed node-owner loss revokes both admission domains, including
// loss discovered by a post-capture check. Playback mismatch affects runtime only.
bool node_owner() noexcept {
    const bool good=native11_nodes::owns_calls();
    if(!good){InterlockedExchange(&active,0);InterlockedExchange(&templates_active,0);}
    return good;
}
bool read(void*,std::uint32_t address,void* output,std::size_t size){SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),output,size,&copied)&&copied==size;
}
bool executable(std::uintptr_t address,std::size_t size,void** allocation=nullptr) noexcept {
    MEMORY_BASIC_INFORMATION page{};
    if(!address||!size||size-1>UINTPTR_MAX-address||
       VirtualQuery(reinterpret_cast<void*>(address),&page,sizeof(page))!=sizeof(page)||
       page.State!=MEM_COMMIT||(page.Protect!=PAGE_EXECUTE_READ&&
       page.Protect!=PAGE_EXECUTE_READWRITE&&page.Protect!=PAGE_EXECUTE_WRITECOPY))return false;
    const auto start=reinterpret_cast<std::uintptr_t>(page.BaseAddress);
    if(address<start||address-start>page.RegionSize||size>page.RegionSize-(address-start))return false;
    if(allocation){if(page.Type!=MEM_IMAGE)return false;*allocation=page.AllocationBase;}
    return true;
}
bool normalize_native_fallback(HMODULE module,std::uint8_t* bytes,std::uint64_t& epoch) noexcept {
    const auto source=playback_source.load();animations::NativePlaybackProof proof{},after{};
    if(!source||!source(module,proof)||proof.image!=module||!proof.epoch||proof.length!=6||
       reinterpret_cast<std::uintptr_t>(proof.target)!=reinterpret_cast<std::uintptr_t>(module)+0x2161c0)return false;
    void* source_owner=nullptr;void* replacement_owner=nullptr;
    if(!executable(reinterpret_cast<std::uintptr_t>(source),1,&source_owner)||
       !executable(reinterpret_cast<std::uintptr_t>(proof.replacement),1,&replacement_owner)||
       source_owner!=replacement_owner||!executable(reinterpret_cast<std::uintptr_t>(proof.gateway),11))return false;
    std::array<std::uint8_t,6> entry{};std::array<std::uint8_t,11> gateway{};
    constexpr std::array<std::uint8_t,6> original{{0x55,0x8b,0xec,0x53,0x8b,0xd9}};
    const auto site=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(proof.target));
    if(!read(nullptr,site,entry.data(),entry.size())||entry[0]!=0xe9||entry[5]!=0x90||
       std::memcmp(bytes,entry.data(),entry.size())||
       !read(nullptr,static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(proof.gateway)),gateway.data(),gateway.size())||
       std::memcmp(gateway.data(),original.data(),original.size())||gateway[6]!=0xe9)return false;
    std::uint32_t displacement=0;std::memcpy(&displacement,entry.data()+1,4);
    if(site+5+displacement!=reinterpret_cast<std::uintptr_t>(proof.replacement))return false;
    std::memcpy(&displacement,gateway.data()+7,4);
    if(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(proof.gateway)+11+displacement)!=site+6||
       !source(module,after)||after.epoch!=proof.epoch||after.image!=proof.image||after.target!=proof.target||
       after.replacement!=proof.replacement||after.gateway!=proof.gateway||after.length!=proof.length||
       playback_source.load()!=source)return false;
    std::memcpy(bytes,original.data(),original.size());epoch=proof.epoch;return true;
}
bool profile_matches(HMODULE module,bool load_only,std::uint64_t* stamp=nullptr) noexcept {
    if(!module)return false;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    const auto delta=base-profile::preferred_base;
    unsigned loaders=0;std::uint64_t epoch=0;
    for(const auto& region:profile::regions){
        if(load_only){
            if(region.rva!=0x23f850&&region.rva!=0x23f9ff)continue;
            ++loaders;
        }
        std::array<std::uint8_t,512> bytes{};
        if(region.size>bytes.size()||!read(nullptr,base+region.rva,bytes.data(),region.size))return false;
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;
            std::memcpy(bytes.data()+offset,&value,4);
        }
        if(!load_only&&region.rva==0x2161c0&&std::memcmp(bytes.data(),region.bytes,region.size)&&
           !normalize_native_fallback(module,bytes.data(),epoch))return false;
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    if(load_only)return loaders==2;
    for(const auto& slot:profile::slots){std::uint32_t value=0;
        if(!read(nullptr,base+slot[0],&value,4)||value!=base+slot[1])return false;
    }
    if(stamp)*stamp=epoch;
    return true;
}
bool matches(HMODULE module,std::uint64_t* stamp=nullptr) noexcept {return profile_matches(module,false,stamp);}
bool same_profile(HMODULE module,std::uint64_t stamp) noexcept {
    std::uint64_t current=0;return matches(module,&current)&&current==stamp;
}
bool templates_enabled() noexcept {return InterlockedCompareExchange(&templates_active,0,0)!=0;}
bool view_matches(HMODULE module) noexcept {
    if(!module)return false;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    const auto delta=base-view_profile::preferred_base;
    for(const auto& region:view_profile::regions){
        // Mutable near-fade values are sampled with each view, not matched
        // against the file defaults as if they were immutable instructions.
        if(region.rva==view_profile::mutable_near_fade_rva)continue;
        std::array<std::uint8_t,512> bytes{};
        if(region.size>bytes.size()||region.rva>UINT32_MAX-base||
           !read(nullptr,base+region.rva,bytes.data(),region.size))return false;
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;
            std::memcpy(bytes.data()+offset,&value,4);
        }
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    for(const auto& slot:view_profile::slots){std::uint32_t value=0;
        if(slot[0]>UINT32_MAX-base||slot[1]>UINT32_MAX-base||
           !read(nullptr,base+slot[0],&value,4)||value!=base+slot[1])return false;
    }
    return true;
}
bool template_matches(HMODULE module) noexcept {
    if(!module)return false;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    const auto delta=base-template_profile::preferred_base;
    for(const auto& region:template_profile::regions){
        std::array<std::uint8_t,512> bytes{};
        if(region.size>bytes.size()||region.rva>UINT32_MAX-base||
           !read(nullptr,base+region.rva,bytes.data(),region.size))return false;
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;
            std::memcpy(bytes.data()+offset,&value,4);
        }
        if(const auto fleet=template_fleet.load())
            if(!native11_template_fleet::normalize(module,fleet,region.rva,bytes.data(),region.size))return false;
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    return true;
}
bool template_guards(HMODULE module,std::uint64_t* stamp=nullptr) noexcept {
    return node_owner()&&native11_assets::owns_load_call(module)&&
        matches(module,stamp)&&template_matches(module);
}
bool load_template_guards(HMODULE module) noexcept {
    // Immutable completed-load metadata depends on construction, not a later
    // playback override. Runtime producers retain the full clock profile.
    return node_owner()&&native11_assets::owns_load_call(module)&&
        profile_matches(module,true)&&template_matches(module);
}
bool material_baseline_matches(HMODULE module) noexcept {
    if(!module)return false;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    // Reuse the pinned mesh type and slot getter only. Render callbacks and
    // class material policies are outside this immutable load-time producer.
    unsigned matched=0;
    for(const auto& region:native11_materials::profile::regions){
        if(region.rva!=0x231380&&region.rva!=0x1968f0)continue;
        ++matched;
        std::array<std::uint8_t,64> bytes{};void* allocation=nullptr;
        if(region.size>bytes.size()||region.relocation_count||region.rva>UINT32_MAX-base||
           !executable(base+region.rva,region.size,&allocation)||allocation!=module||
           !read(nullptr,base+region.rva,bytes.data(),region.size)||
           std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    std::uint32_t type=0;
    return matched==2&&base<=UINT32_MAX-0x2bca00&&
        read(nullptr,base+0x2bca00,&type,4)&&type==base+0x1968f0;
}
std::shared_ptr<const native11::TextureAsset> texture(void*,std::uint32_t pointer){
    return native11_textures::current(pointer);
}
bool material_owners_current(const native11::ModelMaterialBaseline& value) noexcept {
    if(!value.data)return false;
    for(const auto& mesh:value.data->meshes){
        for(const auto& held:mesh.slots)
            if(held&&native11_textures::current(held->texture)!=held)return false;
        if(mesh.borg_alternate&&native11_textures::current(mesh.borg_alternate->texture)!=mesh.borg_alternate)return false;
    }
    return true;
}
bool template_header(const native11::ModelNodeTemplate& value,HMODULE module) noexcept {
    if(!value.model||!value.data)return false;
    const auto db=value.model->info.database;
    if(!db||db>UINT32_MAX-0x40)return false;
    std::uint32_t vtable=0,root=0,name=0;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    if(!read(nullptr,db,&vtable,4)||vtable!=base+0x2bc56c||
       !read(nullptr,db+0x3c,&root,4)||root!=value.data->root_address||
       !read(nullptr,db+0x38,&name,4)||!name)return false;
    for(std::uint32_t i=0;i<sizeof(value.model->info.name);++i){char c=0;
        if(name>UINT32_MAX-i||!read(nullptr,name+i,&c,1)||c!=value.model->info.name[i])return false;
        if(!c)return i!=0;
    }
    return false;
}
bool model_template_unlocked(std::uint32_t database,std::uint64_t serial,
    std::shared_ptr<const native11::ModelNodeTemplate>& output,std::string& error) noexcept {
    try {
        error.clear();const auto module=image.load();
        auto model=native11_assets::pin_model(database,serial);
        std::shared_ptr<const native11::ModelNodeTemplate> candidate;
        if(!model||!load_template_guards(module)||!template_cache.find(model,candidate,error)||
           !template_header(*candidate,module)||!templates_enabled()||image.load()!=module||
           !load_template_guards(module)||native11_assets::pin_model(database,serial)!=model){
            if(error.empty())error="load template unavailable or source revoked";
            return false;
        }
        output=std::move(candidate);error.clear();return true;
    }catch(...){try{error="model template publication failure";}catch(...){}return false;}
}
void report(const char* text) noexcept {try{if(auto call=logger.load())call(text);}catch(...){}}
struct Busy {Busy(){busy=true;}~Busy(){busy=false;}};
bool sample_time_unlocked(const native11::Object& object,
    native11::InstanceAnimationTime& output,std::string& error) noexcept {
    const auto module=image.load();
    std::uint64_t stamp=0;
    const bool nodes_owned=node_owner();
    if(!nodes_owned||!matches(module,&stamp)){
        InterlockedExchange(&active,0);
        try{error="animation time profile or observer ownership lost";}catch(...){}
        return false;
    }
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    const native11::InstanceAnimationTimeProfile addresses{base+0x3ad508,base+0x2b3e2c,base+0x2bc56c};
    native11::InstanceAnimationTime candidate;
    if(!native11::capture_instance_animation_time(object,addresses,nullptr,read,candidate,error))return false;
    if(!InterlockedCompareExchange(&active,0,0)||image.load()!=module||
       !node_owner()||!same_profile(module,stamp)){
        try{error="animation time source revoked during capture";}catch(...){}
        return false;
    }
    output=candidate;error.clear();return true;
}
bool sample_view_unlocked(const native11::Object& object,std::uint32_t camera,
    std::uint32_t native_x87_control,native11::CapturedCraftView& output,
    std::string& error) noexcept {
    const auto module=image.load();
    std::uint64_t stamp=0;
    if(!node_owner()||!native11_assets::owns_load_call(module)||
       !matches(module,&stamp)||!view_matches(module)){
        try{error="Craft view profile or producer ownership unavailable";}catch(...){}return false;
    }
    const auto model=native11_assets::pin_model(object.database,object.model_serial);
    if(!model){try{error="active version-pinned view model unavailable";}catch(...){}return false;}
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    const native11::CraftViewReadProfile addresses{base+0x3ad508,base+0x36b5ac,
        base+0x36b610,base+0x3637b5,base+0x2fcac0,base+0x2b3e2c,base+0x2bc56c,base+0x30444c};
    native11::CapturedCraftView candidate;
    if(!native11::capture_craft_view(object,camera,native_x87_control,
        addresses,nullptr,read,candidate,error))return false;
    if(!enabled()||image.load()!=module||!node_owner()||
       !native11_assets::owns_load_call(module)||!same_profile(module,stamp)||!view_matches(module)||
       native11_assets::pin_model(object.database,object.model_serial)!=model){
        try{error="Craft view source revoked during capture";}catch(...){}return false;
    }
    output=candidate;error.clear();return true;
}
bool sample_channels_unlocked(const native11::Object& object,
    native11::RuntimePoseChannels& output,std::string& error) noexcept {
    const auto module=image.load();
    std::uint64_t stamp=0;
    const bool nodes_owned=node_owner();
    if(!nodes_owned||!matches(module,&stamp)){
        InterlockedExchange(&active,0);
        try{error="runtime channel profile or observer ownership lost";}catch(...){}
        return false;
    }
    auto model=native11_assets::pin_model(object.database,object.model_serial);
    if(!model){try{error="active version-pinned pose model unavailable";}catch(...){}return false;}
    native11::ModelPoseSnapshot authored;
    if(!pose_cache.inspect(model,a2fo_rs_native11_inspect_sod_pose,authored,error))return false;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    const native11::RuntimePoseReadProfile addresses{{base+0x3ad508,base+0x2b3e2c,base+0x2bc56c},
        base+0x2bc39c,base+0x2bc42c,base+0x2146a0,base+0x2161c0};
    native11::RuntimePoseChannels candidate;
    if(!native11::capture_runtime_pose_channels(object,authored,addresses,nullptr,read,candidate,error))return false;
    if(!InterlockedCompareExchange(&active,0,0)||image.load()!=module||
       !node_owner()||!same_profile(module,stamp)||
       native11_assets::pin_model(object.database,object.model_serial)!=model){
        try{error="runtime pose source revoked during capture";}catch(...){}return false;
    }
    output=std::move(candidate);error.clear();return true;
}
}
bool enabled() noexcept {return InterlockedCompareExchange(&active,0,0)!=0;}
void disable() noexcept {InterlockedExchange(&active,0);InterlockedExchange(&templates_active,0);}
bool initialize(HMODULE module,bool request,Log log,HMODULE fleet_ops,animations::NativePlaybackSource source) noexcept {
    // External startup serialization must precede any callback admission.
    disable();logger.store(log);playback_source.store(source);
    if(!request)return false;
    if(!module||!native11_capture::exact_engine_image(module)||!matches(module)||!node_owner()){
        report("Native11 animation clock unavailable: exact profile or checked node observer missing");return false;
    }
    template_fleet.store(native11_template_fleet::qualify(module,fleet_ops)?fleet_ops:nullptr);
    pose_cache.clear();template_cache.clear();material_cache.clear();template_events=0;
    image.store(module);owner=GetCurrentThreadId();events=failures=0;
    InterlockedExchange(&templates_active,1);
    InterlockedExchange(&active,1);
    if(template_fleet.load())report("Native11 load-template metadata retains checked Fleet database constructor chain");
    report("Native11 animation clock diagnostics armed; read-only render-time phase comparison, no offscreen authority");return true;
}
bool sample_time(const native11::Object& object,native11::InstanceAnimationTime& output,
    std::string& error) noexcept {
    if(!enabled()||owner!=GetCurrentThreadId()||busy){
        try{error="animation time producer disabled, foreign or reentrant";}catch(...){}
        return false;
    }
    Busy guard;return sample_time_unlocked(object,output,error);
}
bool sample_channels(const native11::Object& object,native11::RuntimePoseChannels& output,
    std::string& error) noexcept {
    if(!enabled()||owner!=GetCurrentThreadId()||busy){
        try{error="runtime pose producer disabled, foreign or reentrant";}catch(...){}return false;
    }
    Busy guard;return sample_channels_unlocked(object,output,error);
}
bool sample_lod_context(const native11::Object& object,std::uint32_t camera,
    std::uint32_t render_context,std::uint32_t native_x87_control,
    native11::ModelLodContext& output,std::string& error) noexcept {
    if(!enabled()||owner!=GetCurrentThreadId()||busy){
        try{error="native LOD producer disabled, foreign or reentrant";}catch(...){}return false;
    }
    Busy guard;
    try{
        const auto module=image.load();const auto model=native11_assets::pin_model(object.database,object.model_serial);
        std::uint64_t stamp=0;
        if(!model||!template_guards(module,&stamp)){error="native LOD model or code source unavailable";return false;}
        const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
        const native11::ModelLodContextProfile addresses{base+0x3ad508,base+0x2b3e2c,base+0x2bc56c,render_context};
        native11::ModelLodContext candidate;
        if(!native11::capture_model_lod_context(object,camera,native_x87_control,addresses,nullptr,read,candidate,error))return false;
        if(!enabled()||image.load()!=module||!template_guards(module)||!same_profile(module,stamp)||
           native11_assets::pin_model(object.database,object.model_serial)!=model){
            error="native LOD source revoked during capture";return false;
        }
        output=candidate;error.clear();return true;
    }catch(...){try{error="native LOD producer failed";}catch(...){}return false;}
}
bool sample_view(const native11::Object& object,std::uint32_t camera,
    std::uint32_t native_x87_control,native11::CapturedCraftView& output,
    std::string& error) noexcept {
    if(!enabled()||owner!=GetCurrentThreadId()||busy){
        try{error="Craft view producer disabled, foreign or reentrant";}catch(...){}return false;
    }
    Busy guard;return sample_view_unlocked(object,camera,native_x87_control,output,error);
}
bool observe_model_loaded(const std::shared_ptr<const native11::ModelAsset>& model) noexcept {
    if(!templates_enabled()||owner!=GetCurrentThreadId()||busy||!model)return false;
    Busy guard;
    try {
        const auto module=image.load();std::string error;
        bool good=load_template_guards(module);
        if(!good)error="load construction profile or node/load observer ownership mismatch";
        if(good&&native11_assets::pin_model(model->info.database,model->info.serial)!=model){
            good=false;error="completed-load model version unavailable";
        }
        native11::ModelPoseSnapshot authored;
        std::shared_ptr<const native11::ModelNodeTemplate> candidate;
        if(good)good=pose_cache.inspect(model,a2fo_rs_native11_inspect_sod_pose,authored,error);
        const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
        const native11::NodeTemplateReadProfile addresses{base+0x2bc56c,
            base+0x239e10,base+0x230be0,base+0x1968f0,
            base+0x19cf90,base+0x23cb10,base+0x212660};
        if(good)good=native11::capture_model_node_template(authored,addresses,nullptr,read,candidate,error);
        if(good){
            std::shared_ptr<const native11::ModelLodTemplateData> lods;
            good=native11::capture_model_lod_template(*candidate,addresses.lod_type,nullptr,read,lods,error);
            if(good){
                auto data=std::make_shared<native11::ModelNodeTemplateData>(*candidate->data);
                data->lods=std::move(lods);
                candidate=std::make_shared<const native11::ModelNodeTemplate>(
                    native11::ModelNodeTemplate{candidate->model,candidate->authored,std::move(data)});
            }
        }
        if(good){
            good=templates_enabled()&&image.load()==module&&load_template_guards(module)&&
                native11_assets::pin_model(model->info.database,model->info.serial)==model&&
                template_header(*candidate,module);
            if(!good)error="completed-load source or database header changed during capture";
        }
        if(good)good=template_cache.publish(candidate,error);
        if(good){
            std::string material_error;
            std::shared_ptr<const native11::ModelMaterialBaseline> materials;
            bool material_good=material_baseline_matches(module);
            if(!material_good)material_error="mesh type or slot source mismatch";
            if(material_good)material_good=native11::capture_model_material_baseline(
                *candidate,{base+0x2bc56c,base+0x1968f0},nullptr,read,nullptr,texture,materials,material_error);
            if(material_good){
                material_good=templates_enabled()&&image.load()==module&&
                    load_template_guards(module)&&material_baseline_matches(module)&&
                    native11_assets::pin_model(model->info.database,model->info.serial)==model&&
                    template_header(*candidate,module)&&material_owners_current(*materials);
                if(!material_good)material_error="load material source or owner revoked";
            }
            if(material_good)material_good=material_cache.publish(materials,material_error);
            if(template_events<8||template_events%128==0){char message[360]{};
                std::snprintf(message,sizeof(message),
                    "Native11 load materials: model=%llu state=%s meshes=%u cached=%u detail=%s; raw slots only, no effective selection",
                    static_cast<unsigned long long>(model->info.serial),material_good?"captured":"unavailable",
                    static_cast<unsigned>(materials?materials->data->meshes.size():0),
                    static_cast<unsigned>(material_cache.size()),material_good?"completed model-load boundary":material_error.c_str());
                report(message);
            }
        }
        if(good&&template_events==0){std::uint64_t stamp=0;
            if(matches(module,&stamp)&&stamp){char message[180]{};
                std::snprintf(message,sizeof(message),"Native11 animation source: checked builtin native fallback; no configured clip classes; epoch=%llu",static_cast<unsigned long long>(stamp));
                report(message);
            }
        }
        if(template_events!=UINT64_MAX)++template_events;
        if(template_events<=8||template_events%128==0){char message[320]{};
            std::snprintf(message,sizeof(message),
                "Native11 load template: model=%llu status=%s nodes=%u cached=%u detail=%s; no class/LOD authority",
                static_cast<unsigned long long>(model->info.serial),good?"captured":"unavailable",
                good?static_cast<unsigned>(candidate->data->nodes.size()):0u,
                static_cast<unsigned>(template_cache.size()),good?"completed model-load boundary":
                (error.empty()?"profile or model ownership mismatch":error.c_str()));report(message);
        }
        return good;
    }catch(...){return false;}
}
bool model_template(std::uint32_t database,std::uint64_t serial,
    std::shared_ptr<const native11::ModelNodeTemplate>& output,std::string& error) noexcept {
    if(!templates_enabled()||owner!=GetCurrentThreadId()||busy){
        try{error="model template producer disabled, foreign or reentrant";}catch(...){}return false;
    }
    Busy guard;
    return model_template_unlocked(database,serial,output,error);
}
bool model_material_baseline(std::uint32_t database,std::uint64_t serial,
    std::shared_ptr<const native11::ModelMaterialBaseline>& output,std::string& error) noexcept {
    if(!templates_enabled()||owner!=GetCurrentThreadId()||busy){
        try{error="material baseline producer disabled, foreign or reentrant";}catch(...){}return false;
    }
    Busy guard;
    try {
        error.clear();
        const auto module=image.load();
        std::shared_ptr<const native11::ModelNodeTemplate> nodes;
        std::shared_ptr<const native11::ModelMaterialBaseline> candidate;
        if(!material_baseline_matches(module)||!model_template_unlocked(database,serial,nodes,error)||
           !material_cache.find(nodes->model,candidate,error)||candidate->authored!=nodes->authored||
           candidate->nodes!=nodes->data||!material_owners_current(*candidate)||
           !templates_enabled()||image.load()!=module||!load_template_guards(module)||
           !material_baseline_matches(module)||!template_header(*nodes,module)||
           native11_assets::pin_model(database,serial)!=nodes->model||!material_owners_current(*candidate)){
            if(error.empty())error="load material baseline unavailable or source revoked";
            return false;
        }
        output=std::move(candidate);error.clear();return true;
    }catch(...){try{error="load material baseline publication failure";}catch(...){}return false;}
}
void observe(const native11::Object& object,std::uint32_t camera,
    const native11::ModelMaterialSource* source,
    const native11::MaterialObservation* effective,unsigned native_x87_control) noexcept {
    if(!enabled()||owner!=GetCurrentThreadId()||busy||!camera)return;
    Busy guard;
    native11::NativeArithmeticScope arithmetic(native_x87_control);
    if(!arithmetic.valid()){report("Native11 arithmetic unavailable: unsupported captured x87 control word");return;}
    const auto module=image.load();
    std::uint64_t stamp=0;
    const bool nodes_owned=node_owner();
    if(!nodes_owned||!matches(module,&stamp)){
        InterlockedExchange(&active,0);
        report("Native11 animation clock disabled: observer/profile ownership changed; load templates require separate construction checks");return;
    }
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    const native11::AnimationClockProfile addresses{base+0x3ad508,base+0x3a8e74,
        base+0x2b3e2c,base+0x2bc56c,base+0x2bc39c,base+0x2bc42c,base+0x2146a0,base+0x2161c0};
    native11::AnimationClockSnapshot snapshot;std::string error;
    bool good=native11::capture_animation_clocks(object,addresses,nullptr,read,snapshot,error);
    native11::InstanceAnimationTime independent;
    if(good){
        good=sample_time_unlocked(object,independent,error);
        if(good&&(independent.flags!=snapshot.flags||independent.animating!=snapshot.animating||
           std::memcmp(&independent.elapsed,&snapshot.elapsed,sizeof(float)))){
            good=false;
            try{error="independent instance elapsed time differs from native render clock";}catch(...){}
        }
    }
    if(good&&!same_profile(module,stamp)){good=false;error="animation playback source changed during diagnostic capture";}
    if(events!=std::numeric_limits<std::uint64_t>::max())++events;
    if(!good&&failures!=std::numeric_limits<std::uint64_t>::max())++failures;
    if(!enabled())return;
    if(events<=8||events%300==0||(!good&&failures<=8)){
        char text[384]{};
        std::snprintf(text,sizeof(text),"Native11 animation clock: camera=%08lx instance=%08lx channels=%u status=%s failures=%llu reason=%.140s; diagnostics only, offscreen geometry unavailable",
            static_cast<unsigned long>(camera),static_cast<unsigned long>(object.instance),
            static_cast<unsigned>(snapshot.channels.size()),good?"matched":"unavailable",
            static_cast<unsigned long long>(failures),good?"native keys and independent instance time agree":error.c_str());
        report(text);
        if(source&&effective){
            native11::RuntimePoseChannels channels;
            native11::OwnedAnimationPose pose;
            std::string pose_error;
            bool available=source->model==effective->model&&source->model&&
                source->model->info.serial==object.model_serial&&
                sample_channels_unlocked(object,channels,pose_error);
            std::shared_ptr<const native11::ModelNodeTemplate> baseline;std::string baseline_error;
            const bool seeded=available&&model_template_unlocked(object.database,object.model_serial,
                baseline,baseline_error);
            if(available){
                available=seeded?native11::evaluate_owned_animation_pose_from_template(channels.model,
                    channels.clock,channels.playback,*baseline,pose,pose_error):
                    native11::evaluate_owned_animation_pose(channels.model,channels.clock,
                        channels.playback,pose,pose_error);
            }
            bool equal=available&&source->parts.size()==effective->parts.size();
            float max_error=0;
            if(equal)for(std::size_t i=0;i<source->parts.size();++i){
                const auto node=source->parts[i].node_index;
                if(node>=pose.local_to_model.size()||!effective->parts[i].node_state_valid){equal=false;break;}
                for(unsigned c=0;c<16;++c){
                    const float actual=effective->parts[i].node_state.local_to_model[c];
                    const float delta=std::fabs(pose.local_to_model[node][c]-actual);
                    max_error=std::max(max_error,delta);
                    if(!std::isfinite(delta)||delta>1e-4f*(1+std::fabs(actual)))equal=false;
                }
            }
            if(!enabled())return;
            std::snprintf(text,sizeof(text),
                "Native11 owned transform: camera=%08lx instance=%08lx state=%s seed=%s parts=%u max_error=%.8g reason=%.140s; visibility, LOD and class policy remain separate",
                static_cast<unsigned long>(camera),static_cast<unsigned long>(object.instance),
                available?(equal?"matched":"differs"):"unavailable",seeded?"load_template":"authored",
                static_cast<unsigned>(source->parts.size()),double(max_error),pose_error.c_str());
            report(text);
            native11::CapturedCraftView fresh_view;std::string view_error;
            const bool view_available=sample_view_unlocked(object,camera,native_x87_control,fresh_view,view_error);
            float native_values[2]{},native_alpha=0;
            const bool reference=view_available&&object.instance<=UINT32_MAX-0x90&&
                read(nullptr,object.instance+0x88,native_values,sizeof(native_values))&&
                read(nullptr,object.instance+0x78,&native_alpha,sizeof(native_alpha));
            const bool same=reference&&
                !std::memcmp(&fresh_view.result.weighted_distance,&native_values[0],sizeof(float))&&
                !std::memcmp(&fresh_view.result.distance,&native_values[1],sizeof(float))&&
                !std::memcmp(&fresh_view.result.alpha,&native_alpha,sizeof(float));
            std::snprintf(text,sizeof(text),
                "Native11 fresh view: camera=%08lx instance=%08lx state=%s culled=%u alpha=%.8g reason=%.120s; copied logical inputs, native cached outputs comparison only",
                static_cast<unsigned long>(camera),static_cast<unsigned long>(object.instance),
                reference?(same?"matched":"differs"):"unavailable",
                view_available&&fresh_view.result.culled?1u:0u,
                view_available?double(fresh_view.result.alpha):0.,view_error.c_str());
            report(text);
        }
    }
}
}
