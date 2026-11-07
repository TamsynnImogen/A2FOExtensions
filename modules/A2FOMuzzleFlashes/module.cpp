/*
 * Weapon-ODF muzzle flashes using Armada's attached sprite particles.
 * Observe completed OrdnanceClass::Build, not Weapon::Trigger or the shot
 * timer. This preserves the actual firing node, including burst/miss shots,
 * without competing with EnergySystems' ammunition hooks.
 */
#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/module_runtime_a4_ffi.h"
#include "../A2FOAnimations/api.hpp"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif

#include <windows.h>

#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <sstream>
#include <string>
#include <unordered_map>

extern "C" std::uintptr_t a2fo_muzzle_call_thiscall_0(void*, void*);
extern "C" std::uintptr_t a2fo_muzzle_call_thiscall_2(
    void*, void*, std::uintptr_t, std::uintptr_t);
extern "C" std::uintptr_t a2fo_muzzle_call_thiscall_3(
    void*, void*, std::uintptr_t, std::uintptr_t, std::uintptr_t);

namespace {

constexpr char kModuleName[] = "A2FOMuzzleFlashes";
constexpr std::uintptr_t kOrdnanceClassBuildRva = 0x18aba0;
constexpr std::uintptr_t kWeaponGetOwnerRva = 0x271050;
constexpr std::uintptr_t kNodeParticleAddRva = 0x733c0;
constexpr std::uintptr_t kDatabaseFindElementRva = 0x220750;
constexpr std::uintptr_t kStorm3dGlobalRva = 0x3ad508;
constexpr std::size_t kSpriteDatabaseOffset = 0x44;
constexpr std::size_t kWeaponClassOffset = 0x04;
constexpr std::size_t kObjectExpiredOffset = 0x27;

// Complete, relocation-free instructions for the only detour.
constexpr std::uint8_t kExpectedBuild[]{0x55, 0x8b, 0xec, 0x8b, 0x01};
constexpr std::uint8_t kExpectedGetOwner[]{0x8b, 0x49, 0x18, 0x51, 0xe8};
constexpr std::uint8_t kExpectedParticleAdd[]{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x34};
constexpr std::uint8_t kExpectedFindElement[]{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::array<const char*, 4> kWeaponFields{
    "muzzleFlashSprite", "muzzleFlashSize", "muzzleFlashDuration",
    "muzzleFlashColor"};

struct Policy {
    std::string sprite;
    float size = 8.0f;
    float duration = 0.12f;
    std::array<float, 3> colour{1.0f, 1.0f, 1.0f};
    bool missing_sprite_logged = false;
};

// ST3D_Colour is three floats, not an RGBA/D3DCOLOR value. AddParticle copies
// the colour and retains an engine sprite and owner handle, not this policy.
using NodeParticleAdd = void (__cdecl*)(
    const char*, const float*, void*, void*, float, float);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
A2FO_InlineHook g_build_hook{};
bool g_ready = false;
std::unordered_map<void*, Policy> g_policies;
A2FO_AnimationsWeaponFiredFn g_animation_fired=nullptr;
void notify_animation(void* weapon) noexcept {
    if(!g_animation_fired) {
        auto module=GetModuleHandleA("A2FOExtensions.dll");
        auto symbol=module?GetProcAddress(module,"A2FO_AnimationsWeaponFired"):nullptr;
        if(!symbol) {
            module=GetModuleHandleA("A2FOAnimations.dll");
            symbol=module?GetProcAddress(module,"A2FO_AnimationsWeaponFired"):nullptr;
        }
        std::memcpy(&g_animation_fired,&symbol,sizeof(symbol));
    }
    if(g_animation_fired) g_animation_fired(weapon);
}


void log_line(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message);
}

void* at(std::uintptr_t rva) noexcept {
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uintptr_t>(g_armada) + rva);
}

bool readable_range(const void* pointer, std::size_t size) noexcept {
    if (!pointer || !size) return false;
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(pointer, &region, sizeof(region)) ||
        region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0 ||
        (region.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                           PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                           PAGE_EXECUTE_WRITECOPY)) == 0) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto base = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    return address >= base && address - base <= region.RegionSize &&
        size <= region.RegionSize - (address - base);
}

template <typename T>
T read_at(const void* pointer, std::size_t offset, T fallback) noexcept {
    if (!pointer) return fallback;
    const auto* address = static_cast<const std::uint8_t*>(pointer) + offset;
    T result = fallback;
    if (readable_range(address, sizeof(result))) {
        std::memcpy(&result, address, sizeof(result));
    }
    return result;
}

template <std::size_t Size>
bool signature_matches(std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    return readable_range(at(rva), Size) &&
        std::memcmp(at(rva), expected, Size) == 0;
}

struct MuzzleClassContext { const A2FO_WeaponClassLoadedEvent* event; std::exception_ptr error; };
std::string text(A2foRsA4Bytes value) {
    return std::string(reinterpret_cast<const char*>(value.data),value.size);
}
std::uint8_t muzzle_field(void* opaque,std::uint32_t index,A2foRsA4Bytes* name,
                          A2foRsA4Bytes* value) noexcept {
    const auto& event=*static_cast<MuzzleClassContext*>(opaque)->event;
    if (!event.odf_fields || index>=event.odf_field_count) return 0;
    const auto& field=event.odf_fields[index];
    if (!field.name.data) return 0;
    *name={reinterpret_cast<const std::uint8_t*>(field.name.data),field.name.size};
    *value={reinterpret_cast<const std::uint8_t*>(field.value.data),field.value.size};
    return 1;
}
std::uint8_t muzzle_number(void* opaque,A2foRsA4Bytes value,float* result,
                           std::uint32_t* consumed) noexcept {
    try {
        const auto input=text(value);
        char* end=nullptr;
        *result=std::strtof(input.c_str(),&end);
        *consumed=static_cast<std::uint32_t>(end-input.c_str());
        return end!=input.c_str();
    } catch (...) { static_cast<MuzzleClassContext*>(opaque)->error=std::current_exception(); return 0; }
}
std::uint8_t muzzle_colour(void* opaque,A2foRsA4Bytes value,float* channels,
                           std::uint8_t* eof) noexcept {
    try {
        std::istringstream input(text(value));
        for(unsigned i=0;i<3;++i) if (!(input>>channels[i])) return 0;
        input>>std::ws; *eof=input.eof();
        return 1;
    } catch (...) { static_cast<MuzzleClassContext*>(opaque)->error=std::current_exception(); return 0; }
}
void muzzle_store(void* opaque,std::uint32_t operation,A2foRsA4Bytes sprite,
                  float size,float duration,const float* colour) noexcept {
    auto& context=*static_cast<MuzzleClassContext*>(opaque);
    if (context.error) return;
    try {
        void* klass=context.event->weapon_class;
        if (operation==0) { g_policies.erase(klass); return; }
        if (operation==1) {
            char message[192]{};
            std::snprintf(message,sizeof(message),
                "Ignored invalid muzzle-flash settings on WeaponClass %p",klass);
            log_line(message); return;
        }
        Policy policy;
        policy.sprite=text(sprite); policy.size=size; policy.duration=duration;
        std::memcpy(policy.colour.data(),colour,3*sizeof(float));
        char message[384]{};
        std::snprintf(message,sizeof(message),
            "WeaponClass %p: muzzle sprite '%s', size %.3f, duration %.3fs",
            klass,policy.sprite.c_str(),policy.size,policy.duration);
        g_policies.emplace(klass,std::move(policy));
        log_line(message);
    } catch (...) { context.error=std::current_exception(); }
}
void A2FO_CALL weapon_class_loaded_handler(
    const A2FO_WeaponClassLoadedEvent* event, void*) noexcept {
    const bool valid=g_ready && event && event->struct_size>=sizeof(*event) && event->weapon_class;
    MuzzleClassContext context{event,{}};
    a2fo_rs_runtime_muzzle_class(valid,valid?event->odf_field_count:0,
        muzzle_field,muzzle_number,muzzle_colour,muzzle_store,&context);
    if (context.error) log_line("Could not retain weapon muzzle-flash settings");
}

struct MuzzleEmitContext {void* node;};
std::uintptr_t muzzle_step(void* opaque,std::uint32_t operation,
                           std::uintptr_t first,std::uintptr_t second) noexcept {
    auto& context=*static_cast<MuzzleEmitContext*>(opaque);
    auto* policy=reinterpret_cast<Policy*>(second);
    switch(operation) {
    case 0: {
        if(g_policies.empty()) return 0;
        auto found=g_policies.find(read_at<void*>(reinterpret_cast<void*>(first),kWeaponClassOffset,nullptr));
        return found==g_policies.end()?0:reinterpret_cast<std::uintptr_t>(&found->second);
    }
    case 1: return a2fo_muzzle_call_thiscall_0(at(kWeaponGetOwnerRva),reinterpret_cast<void*>(first));
    case 2: return read_at<std::uint8_t>(reinterpret_cast<void*>(first),kObjectExpiredOffset,1);
    case 3: return readable_range(reinterpret_cast<void*>(first),sizeof(void*));
    case 4: {
        void* storm=read_at<void*>(at(kStorm3dGlobalRva),0,nullptr);
        return reinterpret_cast<std::uintptr_t>(read_at<void*>(storm,kSpriteDatabaseOffset,nullptr));
    }
    case 5: return a2fo_muzzle_call_thiscall_2(at(kDatabaseFindElementRva),
        reinterpret_cast<void*>(first),reinterpret_cast<std::uintptr_t>(policy->sprite.c_str()),0);
    case 6: {
        auto* missing=reinterpret_cast<Policy*>(first);
        if(!missing->missing_sprite_logged) {
            missing->missing_sprite_logged=true;
            char message[384]{};
            // The owning key is recovered without changing the stored policy layout.
            for(const auto& entry:g_policies) if(&entry.second==missing) {
                std::snprintf(message,sizeof(message),
                    "Skipped unknown muzzleFlashSprite '%s' on WeaponClass %p",
                    missing->sprite.c_str(),entry.first); break;
            }
            log_line(message);
        }
        return 0;
    }
    case 7: {
        auto* selected=reinterpret_cast<Policy*>(first);
        reinterpret_cast<NodeParticleAdd>(at(kNodeParticleAddRva))(
            selected->sprite.c_str(),selected->colour.data(),reinterpret_cast<void*>(second),
            context.node,selected->size,selected->duration); return 0;
    }
    default: return 0;
    }
}
void emit_flash(void* weapon,void* node) noexcept {
#ifdef A2FO_BUILTIN_FEATURE
    const bool enabled=a2fo::builtin_module_enabled(A2FO_BUILTIN_FEATURE);
#else
    const bool enabled=true;
#endif
    MuzzleEmitContext context{node};
    a2fo_rs_runtime_muzzle_emit(g_ready,enabled,reinterpret_cast<std::uintptr_t>(weapon),
        reinterpret_cast<std::uintptr_t>(node),muzzle_step,&context);
}

struct MuzzleInitContext {const A2FO_ModuleApi* api;};
std::uintptr_t __attribute__((fastcall)) ordnance_build_hook(
    void*,void*,std::uintptr_t,std::uintptr_t,std::uintptr_t) noexcept;
std::uintptr_t muzzle_init_step(void* opaque,std::uint32_t operation,
                              std::uintptr_t first,std::uintptr_t) noexcept {
    const auto* api=static_cast<MuzzleInitContext*>(opaque)->api;
    switch(operation) {
    case 8: g_api=api;g_armada=static_cast<HMODULE>(api->armada_module());return 1;
    case 9: return g_armada &&
        signature_matches(kOrdnanceClassBuildRva,kExpectedBuild) &&
        signature_matches(kWeaponGetOwnerRva,kExpectedGetOwner) &&
        signature_matches(kNodeParticleAddRva,kExpectedParticleAdd) &&
        signature_matches(kDatabaseFindElementRva,kExpectedFindElement);
    case 10: log_line("Muzzle flashes unavailable: native signatures do not match");return 0;
    case 11: return api->register_weapon_class_loaded_handler(kModuleName,
        kWeaponFields.data(),static_cast<std::uint32_t>(kWeaponFields.size()),
        &weapon_class_loaded_handler,nullptr);
    case 12: g_ready=api->install_inline_hook(at(kOrdnanceClassBuildRva),
        reinterpret_cast<void*>(&ordnance_build_hook),sizeof(kExpectedBuild),
        kExpectedBuild,&g_build_hook);return g_ready;
    case 13: log_line(first
        ? "Weapon-ODF muzzle flashes initialized at native projectile creation"
        : "Muzzle flashes disabled: projectile-creation hook unavailable");return 0;
    case 14: g_ready=false;g_policies.clear();return 0;
    default: return 0;
    }
}

std::uintptr_t __attribute__((fastcall)) ordnance_build_hook(
    void* ordnance_class, void*, std::uintptr_t node,
    std::uintptr_t transform, std::uintptr_t weapon) noexcept {
    const auto ordnance = a2fo_muzzle_call_thiscall_3(
        g_build_hook.gateway, ordnance_class, node, transform, weapon);
    if (ordnance) {
        notify_animation(reinterpret_cast<void*>(weapon));
        emit_flash(reinterpret_cast<void*>(weapon),
                   reinterpret_cast<void*>(node));
    }
    return ordnance;
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    const bool valid=api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
        api->api_version == A2FO_MODULE_API_VERSION && api->log &&
        api->armada_module && api->install_inline_hook &&
        A2FO_MODULE_API_HAS(api, register_weapon_class_loaded_handler) &&
        api->register_weapon_class_loaded_handler &&
        (api->capabilities & A2FO_CAP_WEAPON_CLASS_LOADED) != 0;
    MuzzleInitContext context{api};
    return a2fo_rs_runtime_muzzle_init(valid,muzzle_init_step,&context)!=0;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    MuzzleInitContext context{g_api};
    a2fo_rs_runtime_muzzle_shutdown(muzzle_init_step,&context);
    // Existing particles are engine-owned and expire through NodeParticleEffect.
}
