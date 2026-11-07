#include "native11_capture.hpp"
#include "native11_assets.hpp"
#include "native11_sprites.hpp"
#include "native11_materials.hpp"
#include "native11_world_lights.hpp"
#include "native11_world_pipeline.hpp"
#include "native11_capture_scale_profile.hpp"
#include "native11_craft_view_profile.hpp"
#include "native11_light_classes.hpp"
#include "native11_animation_clock.hpp"
#include "native11_textures.hpp"
#include "native11_borg_material_source.hpp"
#include "native11_preview_export.hpp"
#include "native11_craft_visibility_base_source.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include "../renderer/native11/scene_capture.hpp"
#include "../rust/native_hooks_ffi.h"
#include <wincrypt.h>
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdint>
#if defined(A2FO_NATIVE11_CAPTURE_TESTING)
extern "C" void a2fo_native11_capture_before_publish() noexcept;
#endif

namespace {
using a2fo::native11::Collector;
using a2fo::native11::Frame;
constexpr std::uint32_t tap_rva=0x000cfdbf;
constexpr std::array<unsigned char,5> tap_bytes{{0x8b,0x11,0xff,0x52,0x14}};
// Exact reviewed Roots executable; no wildcard/prologue scan or ODF model guess.
constexpr unsigned char target_sha[32]={
    0x43, 0x00, 0xab, 0x71, 0x73, 0x1e, 0x32, 0x3f, 0x57, 0x67, 0xd8, 0xee, 0x05, 0x45, 0x7e, 0x64,
    0x44, 0x40, 0x1b, 0x82, 0x32, 0x53, 0xae, 0xbe, 0xe8, 0x6c, 0x5a, 0xc2, 0x3c, 0x2b, 0x00, 0xde};
template<std::size_t N>
bool loaded_bytes_match(HMODULE image,std::uint32_t rva,const std::array<unsigned char,N>& expected) noexcept {
    std::array<unsigned char,N> actual{};SIZE_T count=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<unsigned char*>(image)+rva,
        actual.data(),actual.size(),&count)&&count==actual.size()&&
        std::memcmp(actual.data(),expected.data(),actual.size())==0;
}
bool loaded_scale_profile_matches(HMODULE image) noexcept {
    namespace p=a2fo::native11_capture::profile;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image));
    const auto delta=base-p::preferred_base;
    for(const auto& region:p::regions){
        std::array<std::uint8_t,512> bytes{};SIZE_T copied=0;
        if(region.size>bytes.size()||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(base+region.rva),
            bytes.data(),region.size,&copied)||copied!=region.size)return false;
        for(std::size_t i=0;i<region.relocation_count;++i){
            const auto offset=region.relocations[i];if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;
            std::memcpy(bytes.data()+offset,&value,4);
        }
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    // Selected instance/DB implementations must still own the verified slots.
    const std::uint32_t slots[][2]={{0x2b3e38,0xcb6b0},{0x2b3e40,0xcb0e0},{0x2bc56c,0x2209b0}};
    for(const auto& pair:slots){std::uint32_t value=0;SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(base+pair[0]),&value,4,&copied)||
           copied!=4||value!=base+pair[1])return false;
    }
    return true;
}
bool loaded_view_profile_matches(HMODULE image) noexcept {
    namespace p=a2fo::native11_animation_clock::view_profile;
    if(!image)return false;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image));
    const auto delta=base-p::preferred_base;
    for(const auto& region:p::regions){
        // Frame inputs are copied and validated by capture_craft_view; their
        // on-disk defaults are not a code-ownership signature.
        if(region.rva==p::mutable_near_fade_rva)continue;
        std::array<std::uint8_t,512> bytes{};
        SIZE_T copied=0;
        if(region.size>bytes.size()||region.rva>UINT32_MAX-base||
           !ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(base+region.rva),
               bytes.data(),region.size,&copied)||copied!=region.size)return false;
        for(std::size_t i=0;i<region.relocation_count;++i){
            const auto offset=region.relocations[i];if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;
            std::memcpy(bytes.data()+offset,&value,4);
        }
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    for(const auto& slot:p::slots){std::uint32_t value=0;SIZE_T copied=0;
        if(slot[0]>UINT32_MAX-base||slot[1]>UINT32_MAX-base||
           !ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(base+slot[0]),&value,4,&copied)||
           copied!=4||value!=base+slot[1])return false;
    }
    return true;
}
bool exact_image(HMODULE image,const unsigned char* expected) noexcept {
    char path[32768]{};
    const DWORD length=GetModuleFileNameA(image,path,sizeof(path));
    if(!length||length>=sizeof(path))return false;
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool valid=false;
    if(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&
       CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)) {
        std::array<BYTE,16384> bytes{};DWORD count=0;bool read_ok=true;
        for(;;){
            if(!ReadFile(file,bytes.data(),bytes.size(),&count,nullptr)){read_ok=false;break;}
            if(!count)break;
            if(!CryptHashData(hash,bytes.data(),count,0)){read_ok=false;break;}
        }
        BYTE digest[32]{};DWORD size=sizeof(digest);
        valid=read_ok&&CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)&&size==32&&
            std::memcmp(digest,expected,32)==0;
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);return valid;
}
bool read_local(void*,std::uint32_t address,void* output,std::size_t size) {
    SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),output,size,&copied)&&copied==size;
}
Collector collector(nullptr,read_local);
Frame published{};
SRWLOCK bound_lock=SRWLOCK_INIT;
std::shared_ptr<const a2fo::native11::BoundCapture> bound_capture;
std::shared_ptr<const a2fo::native11::BoundMaterialCapture> material_capture;
std::shared_ptr<const a2fo::native11_world_lights::Snapshot> light_capture;
std::shared_ptr<const a2fo::native11_capture::WorldModelSnapshot> world_models;
std::shared_ptr<const a2fo::native11_capture::WorldViewSnapshot> world_views;
std::shared_ptr<const a2fo::native11_capture::WorldPreparationSnapshot> world_preparations;
std::shared_ptr<const a2fo::native11_capture::WorldVisibilitySnapshot> world_visibility;
std::shared_ptr<const a2fo::native11_capture::WorldBorgSnapshot> world_borg_materials;
HMODULE capture_image=nullptr;
// Admission retains successful file identity and pins Fleet before current()
// may be used. The separate lock also serializes first admission with getters.
SRWLOCK preparation_admission_lock=SRWLOCK_INIT;
HMODULE preparation_admitted_image=nullptr,preparation_admitted_fleet=nullptr;
HMODULE visibility_admitted_image=nullptr;
std::uint32_t active_context_engine=0,active_render_context=0,active_context_quality=0;
#if defined(A2FO_NATIVE11_CAPTURE_TESTING)
a2fo::native11::PinWorldTemplate test_template_pin=nullptr;
void* test_template_context=nullptr;
a2fo::native11::PinWorldMaterialBaseline test_material_pin=nullptr;
void* test_material_context=nullptr;
a2fo::native11::ResolveTextureAsset test_borg_texture=nullptr;
void* test_borg_texture_context=nullptr;
#endif
volatile LONG enabled=0,owner=0;
std::uint64_t lifecycle_generation=0,active_generation=0;
std::uint32_t active_pipeline_epoch=0;
bool arm_capture() noexcept {
    a2fo::native11_world_pipeline::invalidate_sources();
    AcquireSRWLockExclusive(&bound_lock);
    const bool available=lifecycle_generation!=UINT64_MAX;
    if(available)++lifecycle_generation;
    bound_capture.reset();material_capture.reset();light_capture.reset();world_models.reset();world_views.reset();world_preparations.reset();world_visibility.reset();world_borg_materials.reset();InterlockedExchange(&enabled,available?1:0);
    ReleaseSRWLockExclusive(&bound_lock);return available;
}
a2fo::native11_capture::Log logger=nullptr;
LONG frames=0;
bool installed=false;
void report(const char* text) noexcept {if(logger)logger(text);}
std::uint64_t model_serial(void*,std::uint32_t database,const char* name) noexcept {
    a2fo::native11::ModelAssetInfo info{};
    return a2fo::native11_assets::model_info(database,info)&&!std::strcmp(name,info.name)?info.serial:0;
}
std::shared_ptr<const a2fo::native11::ModelAsset> pin_world_model(void*,std::uint32_t db,const char* name){
    return a2fo::native11_assets::pin_current_model(db,name);
}
bool current_world_models(const a2fo::native11_capture::WorldModelSnapshot& value) noexcept {
    return capture_image&&loaded_scale_profile_matches(capture_image)&&
        value.world&&value.source_ticket&&value.model_generation&&value.capture_lifecycle&&
        value.source_ticket==a2fo::native11_world_pipeline::source_epoch()&&
        value.model_generation==a2fo::native11_assets::generation()&&
        a2fo::native11_world_lights::current(*value.world);
}
bool bind_world_models(const Frame& frame,
    const std::shared_ptr<const a2fo::native11_world_lights::Snapshot>& lights,
    std::shared_ptr<const a2fo::native11_capture::WorldModelSnapshot>& output,std::string& error) noexcept {
    try {
        const auto epoch=a2fo::native11_assets::generation();
        if(!capture_image||!active_pipeline_epoch||!epoch||!lights||lights->camera!=frame.camera||
           lights->frame_generation!=frame.generation||!a2fo::native11_world_lights::current(*lights)||
           active_pipeline_epoch!=a2fo::native11_world_pipeline::source_epoch()||!loaded_scale_profile_matches(capture_image)){
            error="world model association source/profile unavailable";return false;
        }
        auto pending=std::make_shared<a2fo::native11_capture::WorldModelSnapshot>();
        pending->world=lights;pending->source_ticket=active_pipeline_epoch;
        pending->capture_lifecycle=active_generation;pending->model_generation=epoch;
        const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(capture_image));
        if(!a2fo::native11::capture_world_model_associations({base+0x361084,base+0x2b3e2c,base+0x2bc56c},
           lights->captured.members,nullptr,read_local,nullptr,pin_world_model,pending->associations,error))return false;
        if(!current_world_models(*pending)){
            error="world model association source changed during capture";return false;
        }
        output=std::move(pending);error.clear();return true;
    }catch(...){try{error="world model association allocation failure";}catch(...){}return false;}
}
bool current_world_views(const a2fo::native11_capture::WorldViewSnapshot& value) noexcept {
    return value.models&&current_world_models(*value.models)&&loaded_view_profile_matches(capture_image)&&
        value.models->world&&value.views.camera==value.models->world->camera;
}
bool bind_world_views(const std::shared_ptr<const a2fo::native11_capture::WorldModelSnapshot>& models,
    std::shared_ptr<const a2fo::native11_capture::WorldViewSnapshot>& output,std::string& error) noexcept {
    try {
        if(!models||!current_world_models(*models)||!loaded_view_profile_matches(capture_image)){
            error="world Craft view source/profile unavailable";return false;
        }
        auto pending=std::make_shared<a2fo::native11_capture::WorldViewSnapshot>();pending->models=models;
        const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(capture_image));
        const a2fo::native11::CraftViewReadProfile profile{base+0x3ad508,base+0x36b5ac,
            base+0x36b610,base+0x3637b5,base+0x2fcac0,base+0x2b3e2c,base+0x2bc56c,base+0x30444c};
        std::uint16_t control=0;__asm__ __volatile__("fnstcw %0":"=m"(control));
        if(!a2fo::native11::capture_world_craft_views(models->associations,
            {base+0x361084,base+0x2b3e2c,base+0x2bc56c},profile,models->world->camera,
            a2fo::native11::encode_native_x87(control),nullptr,read_local,nullptr,pin_world_model,
            pending->views,error))return false;
        if(!current_world_views(*pending)){error="world Craft view source revoked during capture";return false;}
        output=std::move(pending);error.clear();return true;
    }catch(...){try{error="world Craft view allocation failure";}catch(...){}return false;}
}

bool preparation_source(a2fo::native11_craft_preparation::source::Proof& proof) noexcept {
    namespace source=a2fo::native11_craft_preparation::source;
    const auto image=capture_image;
    const auto fleet=GetModuleHandleA("FleetOpsHook.dll");
    if(!image)return false;
    AcquireSRWLockExclusive(&preparation_admission_lock);
    bool admitted=preparation_admitted_image==image&&preparation_admitted_fleet==fleet;
    if(!admitted&&source::qualify(image,fleet)){
        preparation_admitted_image=image;preparation_admitted_fleet=fleet;admitted=true;
    }
    const bool valid=admitted&&source::current(image,fleet,proof);
    ReleaseSRWLockExclusive(&preparation_admission_lock);return valid;
}
std::shared_ptr<const a2fo::native11::LightClassRecord> pin_preparation_class(void*,std::uint64_t id){
    return a2fo::native11_light_classes::pin(id);
}
bool current_world_preparations(const a2fo::native11_capture::WorldPreparationSnapshot& value) noexcept {
    a2fo::native11_craft_preparation::source::Proof proof;
    return value.models&&current_world_models(*value.models)&&preparation_source(proof)&&
        a2fo::native11_craft_preparation::source::same(proof,value.source);
}
bool bind_world_preparations(const std::shared_ptr<const a2fo::native11_capture::WorldModelSnapshot>& models,
    std::shared_ptr<const a2fo::native11_capture::WorldPreparationSnapshot>& output,std::string& error) noexcept {
    try {
        a2fo::native11_craft_preparation::source::Proof proof;
        if(!models||!current_world_models(*models)||!preparation_source(proof)){
            error="world Craft preparation source/profile unavailable";return false;
        }
        auto pending=std::make_shared<a2fo::native11_capture::WorldPreparationSnapshot>();
        pending->models=models;pending->source=proof;
        const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(capture_image));
        const a2fo::native11::CraftPreparationReadProfile profile{base+0x2b3e2c,base+0x2bc56c,
            base+0xcb390,base+0xaf100,base+0x340bac,proof.fleet_base_wrapper};
        std::uint16_t control=0;__asm__ __volatile__("fnstcw %0":"=m"(control));
        if(!a2fo::native11::capture_world_craft_preparations(models->associations,
            {base+0x361084,base+0x2b3e2c,base+0x2bc56c},profile,
            a2fo::native11::encode_native_x87(control),nullptr,read_local,nullptr,pin_world_model,
            nullptr,pin_preparation_class,pending->preparations,error))return false;
        if(!current_world_preparations(*pending)){
            error="world Craft preparation sources revoked during capture";return false;
        }
        output=std::move(pending);error.clear();return true;
    }catch(...){try{error="world Craft preparation allocation failure";}catch(...){}return false;}
}


bool base_visibility_source() noexcept {
    namespace source=a2fo::native11_craft_visibility_base::source;
    const auto image=capture_image;if(!image)return false;
    AcquireSRWLockExclusive(&preparation_admission_lock);
    bool admitted=visibility_admitted_image==image;
    // Retain file admission even while optional code ownership is unavailable;
    // an unknown detour must not trigger a whole-executable hash every frame.
    if(!admitted&&(preparation_admitted_image==image||a2fo::native11_capture::exact_engine_image(image))){
        visibility_admitted_image=image;admitted=true;
    }
    const bool valid=admitted&&source::current(image);
    ReleaseSRWLockExclusive(&preparation_admission_lock);return valid;
}
bool main_context(std::uint32_t& engine,std::uint32_t& context,std::uint32_t& quality) noexcept {
    if(!capture_image)return false;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(capture_image));
    std::uint32_t again=0;
    return base<=UINT32_MAX-0x3ad50b&&read_local(nullptr,base+0x3ad508,&engine,4)&&engine&&
        engine<=UINT32_MAX-0x107&&read_local(nullptr,engine+0x104,&context,4)&&context&&
        context<=UINT32_MAX-0x43&&read_local(nullptr,context+0x40,&quality,4)&&
        read_local(nullptr,base+0x3ad508,&again,4)&&again==engine;
}
std::shared_ptr<const a2fo::native11::ModelNodeTemplate> pin_world_template(void*,
    const std::shared_ptr<const a2fo::native11::ModelAsset>& model){
#if defined(A2FO_NATIVE11_CAPTURE_TESTING)
    if(test_template_pin)return test_template_pin(test_template_context,model);
#endif
    std::shared_ptr<const a2fo::native11::ModelNodeTemplate> value;std::string error;
    return model&&a2fo::native11_animation_clock::model_template(model->info.database,model->info.serial,value,error)?value:nullptr;
}
bool context_matches(const a2fo::native11::WorldPreparedVisibility& value) noexcept {
    std::uint32_t engine=0,context=0,quality=0;
    return main_context(engine,context,quality)&&engine==value.engine&&context==value.render_context&&quality==value.quality;
}
bool current_world_visibility(const a2fo::native11_capture::WorldVisibilitySnapshot& value) noexcept {
    try {
        if(value.owner_thread!=GetCurrentThreadId()||!value.preparations||
           !current_world_preparations(*value.preparations)||!base_visibility_source()||!context_matches(value.visibility))return false;
        for(const auto& row:value.visibility.members){
            if(row.status!=a2fo::native11::WorldPreparedVisibilityStatus::captured)continue;
            const auto& held=row.capture.baseline;if(!held)return false;
            const auto current=pin_world_template(nullptr,held->model);
            if(!current||current->model!=held->model||current->authored!=held->authored||current->data!=held->data)return false;
        }
        return current_world_preparations(*value.preparations)&&base_visibility_source()&&context_matches(value.visibility);
    }catch(...){return false;}
}
bool bind_world_visibility(const std::shared_ptr<const a2fo::native11_capture::WorldPreparationSnapshot>& preparations,
    std::shared_ptr<const a2fo::native11_capture::WorldVisibilitySnapshot>& output,std::string& error) noexcept {
    try {
        if(!preparations||!current_world_preparations(*preparations)||!base_visibility_source()){
            error="world base visibility source/profile unavailable";return false;
        }
        std::uint32_t engine=0,context=0,quality=0;
        if(!main_context(engine,context,quality)||!active_render_context||engine!=active_context_engine||
           context!=active_render_context||quality!=active_context_quality){
            error="owned main-view context unavailable or changed";return false;
        }
        auto pending=std::make_shared<a2fo::native11_capture::WorldVisibilitySnapshot>();
        pending->preparations=preparations;pending->owner_thread=GetCurrentThreadId();
        const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(capture_image));
        if(!a2fo::native11::capture_world_prepared_visibility(preparations->models->associations,preparations->preparations,
            {base+0x361084,base+0x2b3e2c,base+0x2bc56c},
            {base+0x2b3e2c,base+0x2bc56c,base+0x3ad508,context},
            {base+0x2b3e2c,base+0x2bc56c,base+0xcb390,base+0xaf100,base+0x340bac,preparations->source.fleet_base_wrapper},
            nullptr,read_local,nullptr,pin_world_model,nullptr,pin_preparation_class,nullptr,pin_world_template,
            pending->visibility,error))return false;
        if(!current_world_visibility(*pending)){error="world base visibility sources revoked during capture";return false;}
        output=std::move(pending);error.clear();return true;
    }catch(...){try{error="world base visibility allocation failure";}catch(...){}return false;}
}

std::shared_ptr<const a2fo::native11::ModelMaterialBaseline> pin_world_material(void*,
    const std::shared_ptr<const a2fo::native11::ModelAsset>& model){
#if defined(A2FO_NATIVE11_CAPTURE_TESTING)
    if(test_material_pin)return test_material_pin(test_material_context,model);
#endif
    std::shared_ptr<const a2fo::native11::ModelMaterialBaseline> value;std::string error;
    return model&&a2fo::native11_animation_clock::model_material_baseline(
        model->info.database,model->info.serial,value,error)?value:nullptr;
}
std::shared_ptr<const a2fo::native11::TextureAsset> pin_borg_texture(void*,std::uint32_t texture){
#if defined(A2FO_NATIVE11_CAPTURE_TESTING)
    if(test_borg_texture)return test_borg_texture(test_borg_texture_context,texture);
#endif
    return a2fo::native11_textures::current(texture);
}
bool borg_source() noexcept {
    // Capture's original admission already pinned the exact executable identity.
    // These signatures qualify the base Craft selector branch and named
    // operations, not class readiness or complete extension overlay ordering.
    // No unknown redirect is normalized.
    return capture_image&&a2fo::native11_borg_material::source::matches(capture_image);
}
bool current_world_borg(const a2fo::native11_capture::WorldBorgSnapshot& value) noexcept {
    try {
        if(value.owner_thread!=GetCurrentThreadId()||!value.preparations||
           !current_world_preparations(*value.preparations)||!borg_source())return false;
        const auto& models=value.preparations->models->associations.members;
        const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(capture_image));
        if(value.materials.members.size()!=models.size()||
           value.preparations->preparations.members.size()!=models.size())return false;
        for(std::size_t k=0;k<models.size();++k){const auto& row=value.materials.members[k];
            if(models[k].status!=a2fo::native11::WorldModelStatus::bound_current_database||
               value.preparations->preparations.members[k].status!=a2fo::native11::WorldPreparationStatus::captured)continue;
            if(row.route_valid){
                a2fo::native11::CraftBorgMaterialRoute route;std::string reason;
                if(!row.preparation||!a2fo::native11::capture_craft_borg_material_route(*row.preparation,
                    {base+0x2b3e2c,base+0xcb0e0},nullptr,read_local,nullptr,pin_preparation_class,route,reason)||
                   !a2fo::native11::same_craft_borg_material_route(row.route,route))return false;
            }
            const auto current=pin_world_material(nullptr,models[k].model);
            if(!a2fo::native11::same_material_baseline_owners(current,row.baseline))return false;
            if(current){
                if(current->model!=models[k].model||!current->data||!current->nodes||!current->authored)return false;
                for(const auto& mesh:current->data->meshes){
                    for(const auto& held:mesh.slots)
                        if(held&&pin_borg_texture(nullptr,held->texture)!=held)return false;
                    if(mesh.borg_alternate&&pin_borg_texture(nullptr,mesh.borg_alternate->texture)!=mesh.borg_alternate)return false;
                }
            }
            if(row.status==a2fo::native11::WorldBorgMaterialStatus::correlated&&
               (!row.preparation||!row.table.object_class||
                pin_preparation_class(nullptr,row.member.object_class)!=row.table.object_class))return false;
            if(row.status==a2fo::native11::WorldBorgMaterialStatus::correlated){
                a2fo::native11::CapturedCraftBorgTable table;std::string reason;
                const auto pin=[](void*,std::uint32_t db,std::uint64_t serial){
                    return a2fo::native11_assets::pin_model(db,serial);
                };
                if(!a2fo::native11::capture_craft_borg_table(row.table.object_class,current,
                    {base+0x2bc56c,base+0x1968f0},nullptr,read_local,nullptr,pin,
                    nullptr,pin_preparation_class,nullptr,pin_borg_texture,table,reason)||
                   table.class_vtable!=row.table.class_vtable||
                   table.table_pointer!=row.table.table_pointer||table.rows.size()!=row.table.rows.size())return false;
                for(std::size_t j=0;j<table.rows.size();++j){const auto& a=table.rows[j];const auto& b=row.table.rows[j];
                    if(a.node_index!=b.node_index||a.original_texture!=b.original_texture||
                       a.borg_texture!=b.borg_texture||a.slot_count!=b.slot_count)return false;
                }
                a2fo::native11::CraftBorgMaterialRoute route;
                if(!a2fo::native11::capture_craft_borg_material_route(*row.preparation,
                    {base+0x2b3e2c,base+0xcb0e0},nullptr,read_local,nullptr,pin_preparation_class,route,reason)||
                   !a2fo::native11::same_craft_borg_material_route(row.route,route))return false;
            }
        }
        return current_world_preparations(*value.preparations)&&borg_source();
    }catch(...){return false;}
}
bool bind_world_borg(const std::shared_ptr<const a2fo::native11_capture::WorldPreparationSnapshot>& preparations,
    std::shared_ptr<const a2fo::native11_capture::WorldBorgSnapshot>& output,std::string& error) noexcept {
    try {
        if(!preparations||!current_world_preparations(*preparations)||!borg_source()){
            error="world base/Borg named operations or logical preparation unavailable";return false;
        }
        auto pending=std::make_shared<a2fo::native11_capture::WorldBorgSnapshot>();
        pending->preparations=preparations;pending->owner_thread=GetCurrentThreadId();
        const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(capture_image));
        if(!a2fo::native11::capture_world_borg_materials(preparations->models->associations,preparations->preparations,
            {base+0x361084,base+0x2b3e2c,base+0x2bc56c},
            {base+0x2b3e2c,base+0x2bc56c,base+0xcb390,base+0xaf100,base+0x340bac,preparations->source.fleet_base_wrapper},
            {base+0x2bc56c,base+0x1968f0},{base+0x2b3e2c,base+0xcb0e0},nullptr,read_local,nullptr,pin_world_model,
            nullptr,pin_preparation_class,nullptr,pin_world_material,nullptr,pin_borg_texture,pending->materials,error))return false;
        if(!current_world_borg(*pending)){error="world base/Borg sources revoked during capture";return false;}
        output=std::move(pending);error.clear();return true;
    }catch(...){try{error="world base/Borg correlation allocation failure";}catch(...){}return false;}
}

}
namespace a2fo::native11_capture {
bool exact_engine_image(HMODULE image) noexcept {return exact_image(image,target_sha);}
bool exact_image_file(HMODULE image,const unsigned char expected[32]) noexcept {return expected&&exact_image(image,expected);}
bool initialize(HMODULE image,bool requested,Log log) noexcept {
    logger=log;
    if(!requested){disable();return false;}
    if(!IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)) {
        report("Native11 scene capture unavailable: x87/SSE save support missing");return false;
    }
    if(installed){if(image!=capture_image){disable();return false;}
        if(!native11_world_pipeline::set_material_validator(native11_materials::current))return false;
        return arm_capture();}
    BYTE actual[5]{};SIZE_T copied=0;
    auto* site=reinterpret_cast<BYTE*>(image)+tap_rva;
    const std::array<unsigned char,15> frustum_context{{0xff,0x50,0x0c,0x84,0xc0,0x74,0x0d,0x8b,0x4e,0x04,0xc6,0x46,0x25,0x01,0x57}};
    const std::array<unsigned char,16> visibility_context{{0x8a,0x46,0x27,0x84,0xc0,0x75,0x1d,0x8b,0x06,0xff,0x50,0x08,0x84,0xc0,0x74,0x14}};
    if(!exact_image(image,target_sha)||!loaded_scale_profile_matches(image)||!loaded_bytes_match(image,0x000cfdb0,frustum_context)||
       !loaded_bytes_match(image,0x000d54e6,visibility_context)||!ReadProcessMemory(GetCurrentProcess(),site,actual,sizeof(actual),&copied)||
       copied!=sizeof(actual)||std::memcmp(actual,tap_bytes.data(),sizeof(actual))||
       !a2fo_rs_windows_patch_call(site,reinterpret_cast<void*>(&a2fo_native11_entity_render_tap),tap_bytes.data(),tap_bytes.size())) {
        report("Native11 scene capture unavailable: exact engine identity or post-frustum hook ownership mismatch");
        return false;
    }
    if(!collector.configure_plain_database_vtable(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image))+0x002bc56c))return false;
    if(!collector.configure_craft_instance_vtable(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image))+0x002b3e2c))return false;
    if(!collector.configure_model_serial_reader(nullptr,model_serial))return false;
    capture_image=image;installed=true;
    if(!native11_world_pipeline::set_material_validator(native11_materials::current))return false;
    if(!arm_capture())return false;
    report("Native11 scene capture armed: high-level Entity::Render post-frustum telemetry; no graphics takeover");
    native11_craft_preparation::source::Proof preparation;
    if(preparation_source(preparation))
        report(preparation.fleet_base_wrapper?
            "Native11 Craft preparation source qualified: checked Fleet saved-original base setup; logical flags/hull only":
            "Native11 Craft preparation source qualified: original base setup; logical flags/hull only");
    else report("Native11 Craft preparation source pending: exact setup/getter ownership unavailable; no geometry authority");
    report(base_visibility_source()?
        "Native11 base visibility source qualified: original node-flag snippets only; material/FOW/draw excluded":
        "Native11 base visibility source pending: node-flag ownership unavailable");
    report(borg_source()?
        "Native11 base/Borg source qualified: eight named operations only; class/caller/overlays excluded":
        "Native11 base/Borg source pending: named operation ownership unavailable");
    return true;
}
bool begin(void* camera,unsigned main) noexcept {
    if(!(main&0xffu)||!InterlockedCompareExchange(&enabled,0,0)||
       InterlockedCompareExchange(&owner,static_cast<LONG>(GetCurrentThreadId()),0)!=0)return false;
    if(!collector.begin(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(camera)),true)){
        InterlockedExchange(&owner,0);return false;
    }
    AcquireSRWLockShared(&bound_lock);
    const bool current=InterlockedCompareExchange(&enabled,0,0)!=0;
    active_generation=lifecycle_generation;
    active_pipeline_epoch=native11_world_pipeline::source_epoch();
    ReleaseSRWLockShared(&bound_lock);
    if(!current){collector.abort();InterlockedExchange(&owner,0);return false;}
    active_context_engine=active_render_context=active_context_quality=0;
    if(!main_context(active_context_engine,active_render_context,active_context_quality))
        active_context_engine=active_render_context=active_context_quality=0;
    native11_materials::begin(camera);
    return true;
}
void end(void* camera,bool owned) noexcept {
    if(!owned||InterlockedCompareExchange(&owner,0,0)!=static_cast<LONG>(GetCurrentThreadId()))return;
    const bool valid=collector.end(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(camera)),published);
    collector.abort();
    std::shared_ptr<const a2fo::native11::BoundCapture> bound;
    std::string binding_error;
    bool bound_valid=valid&&a2fo::native11_assets::bind_frame(published,bound,binding_error);
    std::shared_ptr<const a2fo::native11::BoundMaterialCapture> materials;
    std::string material_error;
    bool materials_valid=valid&&native11_materials::bind_frame(published,materials,material_error);
    std::shared_ptr<const native11_world_lights::Snapshot> lights;
    std::string light_error;
    bool lights_valid=false;
    if(valid&&native11_world_lights::requested()){
        float ambient[3]{};native11_world_pipeline::ambient(ambient);
        lights_valid=native11_world_lights::sample(published.camera,published.generation,ambient,lights,light_error);
    }
    std::shared_ptr<const WorldModelSnapshot> associations;std::string association_error;
    bool associations_valid=false;
    if(!valid)association_error="scene frame unavailable";
    else if(!lights_valid)association_error="checked world roster unavailable";
    else associations_valid=bind_world_models(published,lights,associations,association_error);
    std::shared_ptr<const WorldViewSnapshot> views;std::string view_error;
    bool views_valid=false;
    if(associations_valid)views_valid=bind_world_views(associations,views,view_error);
    else view_error="checked model associations unavailable";
    std::shared_ptr<const WorldPreparationSnapshot> preparations;std::string preparation_error;
    bool preparations_valid=false;
    if(associations_valid)preparations_valid=bind_world_preparations(associations,preparations,preparation_error);
    else preparation_error="checked model associations unavailable";
    std::shared_ptr<const WorldVisibilitySnapshot> visibility;std::string visibility_error;
    bool visibility_valid=false;
    if(preparations_valid)visibility_valid=bind_world_visibility(preparations,visibility,visibility_error);
    else visibility_error="checked logical preparation unavailable";
    std::shared_ptr<const WorldBorgSnapshot> borg;std::string borg_error;
    bool borg_valid=false;
    if(preparations_valid)borg_valid=bind_world_borg(preparations,borg,borg_error);
    else borg_error="checked logical preparation unavailable";
    if(!valid)native11_materials::abort();
#if defined(A2FO_NATIVE11_CAPTURE_TESTING)
    a2fo_native11_capture_before_publish();
#endif
    if(associations&&!current_world_models(*associations)){
        associations.reset();associations_valid=false;
        association_error="world model association sources changed before publication";
    }
    if(views&&(!associations||!current_world_views(*views))){
        views.reset();views_valid=false;view_error="world Craft view sources changed before publication";
    }
    if(preparations&&(!associations||!current_world_preparations(*preparations))){
        preparations.reset();preparations_valid=false;
        preparation_error="world Craft preparation sources changed before publication";
    }
    if(visibility&&(!preparations||!current_world_visibility(*visibility))){
        visibility.reset();visibility_valid=false;visibility_error="world base visibility sources changed before publication";
    }
    if(borg&&(!preparations||!current_world_borg(*borg))){
        borg.reset();borg_valid=false;borg_error="world base/Borg sources changed before publication";
    }
    AcquireSRWLockExclusive(&bound_lock);
    if(!InterlockedCompareExchange(&enabled,0,0)||active_generation!=lifecycle_generation){bound.reset();materials.reset();lights.reset();associations.reset();views.reset();preparations.reset();visibility.reset();borg.reset();borg_valid=false;visibility_valid=false;bound_valid=false;materials_valid=false;lights_valid=false;associations_valid=false;views_valid=false;preparations_valid=false;}
    bound_capture=std::move(bound);material_capture=materials;light_capture=lights;world_models=associations;world_views=views;world_preparations=preparations;world_visibility=visibility;world_borg_materials=borg;ReleaseSRWLockExclusive(&bound_lock);
    if(native11_world_pipeline::requested())native11_world_pipeline::on_frame(materials,lights,active_pipeline_epoch);
    if(native11_preview_export::requested()){
        struct Context {std::shared_ptr<const native11::BoundMaterialCapture> packet;std::uint32_t ticket;};
        const auto state=native11_world_pipeline::material_status();
        Context context{materials,state.source_ticket};
        native11::PreviewExportStatus output;
        output.frame=materials?materials->frame().generation:0;
        output.source_epoch=state.source_epoch;output.source_ticket=state.source_ticket;
        output.objects=materials?static_cast<std::uint32_t>(materials->frame().object_count):0;
        const bool ready=state.state==native11_world_pipeline::MaterialState::previewed&&
            materials&&state.frame==materials->frame().generation;
        output.state=ready?"frame":
            (state.state==native11_world_pipeline::MaterialState::unavailable?"unavailable":"waiting");
        output.reason=ready?"Observed ships rendered by DX11":
            (!material_error.empty()?material_error:state.reason);
        if(!ready&&state.state==native11_world_pipeline::MaterialState::prepared)
            output.reason="DX11 resources prepared; waiting for captured ship poses";
        const auto current=[](void* raw) noexcept {
            const auto& c=*static_cast<const Context*>(raw);
            return c.ticket&&c.ticket==native11_world_pipeline::source_epoch()&&
                native11_materials::current(c.packet);
        };
        const auto read=[](void*,std::vector<std::uint8_t>& bytes,std::uint32_t& width,
            std::uint32_t& height,std::string& error){
            return native11_world_pipeline::readback_material_preview(bytes,width,height,error);
        };
        native11_preview_export::sample(std::move(output),read,current,&context,GetTickCount64());
    }
    if(valid)native11_sprites::begin_ui(published,active_pipeline_epoch);
    else native11_sprites::abort_ui();
    const LONG frame=++frames;
    if(frame<=8||frame%300==0){
        char message[384]{};
        if(valid)std::snprintf(message,sizeof(message),
            "Native11 scene capture: frame=%ld objects=%u camera=%08lx viewport=%.0f,%.0f,%.0f,%.0f; owned CPU camera/placement; baked node scale/effects remain incomplete",
            frame,static_cast<unsigned>(published.object_count),static_cast<unsigned long>(published.camera),
            published.view.viewport[0],published.view.viewport[1],published.view.viewport[2],published.view.viewport[3]);
        else std::snprintf(message,sizeof(message),"Native11 scene capture: frame=%ld rejected (unreadable/invalid/overflow); no native submission",frame);
        report(message);
        if(valid){std::snprintf(message,sizeof(message),"Native11 scene asset binding: frame=%ld bound=%u; version-pinned CPU assets only; textures and effective geometry remain incomplete",frame,bound_valid?1u:0u);report(message);}
        if(valid){std::snprintf(message,sizeof(message),"Native11 scene material binding: frame=%ld selected=%u reason=%.160s; per-instance diffuse only; node state and live acceptance remain incomplete",frame,materials_valid?1u:0u,material_error.c_str());report(message);}
        if(native11_world_lights::requested()){
            std::snprintf(message,sizeof(message),"Native11 world lights: frame=%ld owned=%u sources=%u reason=%.180s; configured ambient; no presentation takeover",frame,lights_valid?1u:0u,lights?static_cast<unsigned>(lights->lighting.lights.size()):0u,light_error.c_str());report(message);
            std::size_t count=0,bound_count=0;
            if(associations){count=associations->associations.members.size();
                for(const auto& member:associations->associations.members)
                    if(member.status==native11::WorldModelStatus::bound_current_database)++bound_count;}
            std::snprintf(message,sizeof(message),"Native11 world model associations: frame=%ld owned=%u members=%u bound=%u reason=%.160s; current DB/placement only, not render selection or geometry",frame,associations_valid?1u:0u,static_cast<unsigned>(count),static_cast<unsigned>(bound_count),association_error.c_str());report(message);
        }
        if(native11_world_lights::requested()){
            std::size_t sampled=0;if(views)for(const auto& row:views->views.members)
                if(row.status==native11::WorldViewStatus::captured)++sampled;
            std::snprintf(message,sizeof(message),"Native11 world Craft views: frame=%ld owned=%u sampled=%u reason=%.180s; fresh distance/fade only, not FOW/full visibility",frame,views_valid?1u:0u,static_cast<unsigned>(sampled),view_error.c_str());report(message);
        }
        if(native11_world_pipeline::requested()){
            std::size_t sampled=0;if(preparations)for(const auto& row:preparations->preparations.members)
                if(row.status==native11::WorldPreparationStatus::captured)++sampled;
            std::snprintf(message,sizeof(message),"Native11 world Craft preparation: frame=%ld owned=%u sampled=%u reason=%.170s; logical systems/Borg/hull only; no material/geometry authority",frame,preparations_valid?1u:0u,static_cast<unsigned>(sampled),preparation_error.c_str());report(message);
            std::size_t visible_rows=0;if(visibility)for(const auto& row:visibility->visibility.members)
                if(row.status==native11::WorldPreparedVisibilityStatus::captured)++visible_rows;
            std::snprintf(message,sizeof(message),"Native11 world base visibility: frame=%ld owned=%u sampled=%u reason=%.160s; prepared node flags only; no FOW/material/geometry authority",frame,visibility_valid?1u:0u,static_cast<unsigned>(visible_rows),visibility_error.c_str());report(message);
            std::size_t correlated=0;if(borg)for(const auto& row:borg->materials.members)
                if(row.status==native11::WorldBorgMaterialStatus::correlated)++correlated;
            std::snprintf(message,sizeof(message),"Native11 world base/Borg operations: frame=%ld owned=%u correlated=%u reason=%.150s; candidate writes only; class/caller/overlays and prior slots unresolved",frame,borg_valid?1u:0u,static_cast<unsigned>(correlated),borg_error.c_str());report(message);
            const auto status=native11_world_pipeline::status();
            const auto material_status=native11_world_pipeline::material_status();
            std::snprintf(message,sizeof(message),"Native11 live material submission: frame=%ld state=%u submitted=%llu reason=%.200s; no world/presentation authority",frame,static_cast<unsigned>(material_status.state),static_cast<unsigned long long>(material_status.submitted_frames),material_status.reason.c_str());report(message);
            std::snprintf(message,sizeof(message),"Native11 offscreen world: frame=%ld state=%u rendered=%llu reason=%.220s",frame,static_cast<unsigned>(status.state),static_cast<unsigned long long>(status.rendered_frames),status.reason.c_str());report(message);
        }
        if(valid&&published.object_count) {
            unsigned named=0,scaled=0;
            for(std::size_t i=0;i<published.object_count;++i)
                {if(published.objects[i].model_identity==a2fo::native11::ModelIdentity::plain_database_name)++named;
                 if(published.objects[i].placement_identity==a2fo::native11::PlacementIdentity::craft_runtime_scale)++scaled;}
            std::snprintf(message,sizeof(message),"Native11 scene models: plain database names=%u/%u runtime scale resolved=%u; first=%.240s",
                named,static_cast<unsigned>(published.object_count),scaled,published.objects[0].model_name);
            report(message);
        }
    }
    InterlockedExchange(&owner,0);
}
std::shared_ptr<const native11::BoundCapture> latest_bound() noexcept {
    AcquireSRWLockShared(&bound_lock);
    auto output=InterlockedCompareExchange(&enabled,0,0)?bound_capture:nullptr;
    ReleaseSRWLockShared(&bound_lock);return output;
}
std::shared_ptr<const native11::BoundMaterialCapture> latest_materials() noexcept {
    AcquireSRWLockShared(&bound_lock);
    auto output=InterlockedCompareExchange(&enabled,0,0)?material_capture:nullptr;
    ReleaseSRWLockShared(&bound_lock);return output;
}
std::shared_ptr<const native11_world_lights::Snapshot> latest_lights() noexcept {
    AcquireSRWLockShared(&bound_lock);auto output=InterlockedCompareExchange(&enabled,0,0)?light_capture:nullptr;
    ReleaseSRWLockShared(&bound_lock);return output;
}
std::shared_ptr<const WorldModelSnapshot> latest_world_models() noexcept {
    AcquireSRWLockShared(&bound_lock);
    auto value=InterlockedCompareExchange(&enabled,0,0)?world_models:nullptr;
    const auto lifecycle=lifecycle_generation;
    ReleaseSRWLockShared(&bound_lock);
    if(!value||value->capture_lifecycle!=lifecycle||!current_world_models(*value))return nullptr;
    AcquireSRWLockShared(&bound_lock);
    const bool valid=InterlockedCompareExchange(&enabled,0,0)&&lifecycle==lifecycle_generation&&value==world_models;
    ReleaseSRWLockShared(&bound_lock);return valid?value:nullptr;
}
std::shared_ptr<const WorldViewSnapshot> latest_world_views() noexcept {
    AcquireSRWLockShared(&bound_lock);
    auto value=InterlockedCompareExchange(&enabled,0,0)?world_views:nullptr;
    const auto lifecycle=lifecycle_generation;
    ReleaseSRWLockShared(&bound_lock);
    if(!value||!value->models||value->models->capture_lifecycle!=lifecycle||!current_world_views(*value))return nullptr;
    AcquireSRWLockShared(&bound_lock);
    const bool valid=InterlockedCompareExchange(&enabled,0,0)&&lifecycle==lifecycle_generation&&
        value==world_views&&value->models==world_models;
    ReleaseSRWLockShared(&bound_lock);return valid?value:nullptr;
}
std::shared_ptr<const WorldVisibilitySnapshot> latest_world_visibility() noexcept {
    AcquireSRWLockShared(&bound_lock);
    auto value=InterlockedCompareExchange(&enabled,0,0)?world_visibility:nullptr;
    const auto lifecycle=lifecycle_generation;
    ReleaseSRWLockShared(&bound_lock);
    if(!value||!value->preparations||!value->preparations->models||
       value->preparations->models->capture_lifecycle!=lifecycle||!current_world_visibility(*value))return nullptr;
    AcquireSRWLockShared(&bound_lock);
    const bool valid=InterlockedCompareExchange(&enabled,0,0)&&lifecycle==lifecycle_generation&&
        value==world_visibility&&value->preparations==world_preparations&&value->preparations->models==world_models;
    ReleaseSRWLockShared(&bound_lock);return valid?value:nullptr;
}
std::shared_ptr<const WorldBorgSnapshot> latest_world_borg_materials() noexcept {
    AcquireSRWLockShared(&bound_lock);
    auto value=InterlockedCompareExchange(&enabled,0,0)?world_borg_materials:nullptr;
    const auto lifecycle=lifecycle_generation;
    ReleaseSRWLockShared(&bound_lock);
    if(!value||!value->preparations||!value->preparations->models||
       value->preparations->models->capture_lifecycle!=lifecycle||!current_world_borg(*value))return nullptr;
    AcquireSRWLockShared(&bound_lock);
    const bool valid=InterlockedCompareExchange(&enabled,0,0)&&lifecycle==lifecycle_generation&&
        value==world_borg_materials&&value->preparations==world_preparations&&value->preparations->models==world_models;
    ReleaseSRWLockShared(&bound_lock);return valid?value:nullptr;
}
#if defined(A2FO_NATIVE11_CAPTURE_TESTING)
void set_world_template_source_for_testing(native11::PinWorldTemplate pin,void* context) noexcept {
    test_template_pin=pin;test_template_context=context;
}
void set_world_material_sources_for_testing(native11::PinWorldMaterialBaseline pin,void* context,
    native11::ResolveTextureAsset texture,void* texture_context) noexcept {
    test_material_pin=pin;test_material_context=context;
    test_borg_texture=texture;test_borg_texture_context=texture_context;
}
#endif
bool material_target(void* instance,void* camera,native11::Object& output) noexcept {
    return InterlockedCompareExchange(&enabled,0,0)&&
        InterlockedCompareExchange(&owner,0,0)==static_cast<LONG>(GetCurrentThreadId())&&
        collector.material_target(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(instance)),
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(camera)),output);
}
bool camera_view(void* camera,native11::EngineCameraView& output) noexcept {
    return InterlockedCompareExchange(&enabled,0,0)&&
        InterlockedCompareExchange(&owner,0,0)==static_cast<LONG>(GetCurrentThreadId())&&
        collector.camera_view(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(camera)),output);
}
std::shared_ptr<const WorldPreparationSnapshot> latest_world_preparations() noexcept {
    AcquireSRWLockShared(&bound_lock);
    auto value=InterlockedCompareExchange(&enabled,0,0)?world_preparations:nullptr;
    const auto lifecycle=lifecycle_generation;
    ReleaseSRWLockShared(&bound_lock);
    if(!value||!value->models||value->models->capture_lifecycle!=lifecycle||!current_world_preparations(*value))return nullptr;
    AcquireSRWLockShared(&bound_lock);
    const bool valid=InterlockedCompareExchange(&enabled,0,0)&&lifecycle==lifecycle_generation&&
        value==world_preparations&&value->models==world_models;
    ReleaseSRWLockShared(&bound_lock);return valid?value:nullptr;
}
void disable() noexcept {
    native11_sprites::abort_ui();
    native11_world_pipeline::invalidate_sources();
    InterlockedExchange(&enabled,0);
    AcquireSRWLockExclusive(&bound_lock);
    InterlockedExchange(&enabled,0);
    if(lifecycle_generation!=UINT64_MAX)++lifecycle_generation;
    bound_capture.reset();material_capture.reset();light_capture.reset();world_models.reset();world_views.reset();world_preparations.reset();world_visibility.reset();world_borg_materials.reset();ReleaseSRWLockExclusive(&bound_lock);
    native11_materials::abort();
}
}
extern "C" void a2fo_native11_observe_entity(void* entity,void* camera) noexcept {
    if(!InterlockedCompareExchange(&enabled,0,0)||
       InterlockedCompareExchange(&owner,0,0)!=static_cast<LONG>(GetCurrentThreadId()))return;
    collector.observe_post_frustum(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(entity)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(camera)));
}
