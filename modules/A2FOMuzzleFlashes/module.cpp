/*
 * Weapon-ODF muzzle flashes using Armada's attached sprite particles.
 * Observe completed OrdnanceClass::Build, not Weapon::Trigger or the shot
 * timer. This preserves the actual firing node, including burst/miss shots,
 * without competing with EnergySystems' ammunition hooks.
 */
#include "../../sdk/include/a2fo_module_api.h"
#include "../A2FOAnimations/api.hpp"

#include <windows.h>

#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
        auto module=GetModuleHandleA("A2FOAnimations.dll");
        auto symbol=module?GetProcAddress(module,"A2FO_AnimationsWeaponFired"):nullptr;
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

std::string clean_text(std::string text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    text = text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        text = text.substr(1, text.size() - 2);
    }
    return text;
}

bool field_text(const A2FO_WeaponClassLoadedEvent& event,
                const char* name, std::string& text) {
    if (!event.odf_fields) return false;
    const auto length = std::strlen(name);
    for (std::uint32_t i = 0; i < event.odf_field_count; ++i) {
        const auto& field = event.odf_fields[i];
        if (!field.name.data || field.name.size != length ||
            _strnicmp(field.name.data, name, length) != 0) continue;
        if (!field.value.data && field.value.size) return false;
        text = clean_text(std::string(
            field.value.data ? field.value.data : "", field.value.size));
        return true;
    }
    return false;
}

bool number_field(const A2FO_WeaponClassLoadedEvent& event,
                  const char* name, float maximum, float& value) {
    std::string text;
    if (!field_text(event, name, text)) return true;
    if (text.find('\0') != std::string::npos) return false;
    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || !std::isfinite(parsed) ||
        parsed <= 0.0f || parsed > maximum) return false;
    if (*end == 'f' || *end == 'F') ++end;
    while (std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end) return false;
    value = parsed;
    return true;
}

bool colour_field(const A2FO_WeaponClassLoadedEvent& event,
                  std::array<float, 3>& colour) {
    std::string text;
    if (!field_text(event, "muzzleFlashColor", text)) return true;
    std::istringstream input(text);
    for (float& channel : colour) {
        if (!(input >> channel) || !std::isfinite(channel) ||
            channel < 0.0f || channel > 1.0f) return false;
    }
    input >> std::ws;
    return input.eof();
}

void A2FO_CALL weapon_class_loaded_handler(
    const A2FO_WeaponClassLoadedEvent* event, void*) noexcept {
    if (!g_ready || !event || event->struct_size < sizeof(*event) ||
        !event->weapon_class) return;
    try {
        // The core supplies resolved includes. An explicitly empty sprite
        // disables an inherited flash; do not resurrect it from parent_class.
        g_policies.erase(event->weapon_class);
        Policy policy;
        if (!field_text(*event, "muzzleFlashSprite", policy.sprite) ||
            policy.sprite.empty()) return;
        if (policy.sprite.size() > 255 ||
            policy.sprite.find('\0') != std::string::npos ||
            !number_field(*event, "muzzleFlashSize", 10000.0f, policy.size) ||
            !number_field(*event, "muzzleFlashDuration", 10.0f, policy.duration) ||
            !colour_field(*event, policy.colour)) {
            char message[192]{};
            std::snprintf(message, sizeof(message),
                "Ignored invalid muzzle-flash settings on WeaponClass %p",
                event->weapon_class);
            log_line(message);
            return;
        }
        char message[384]{};
        std::snprintf(message, sizeof(message),
            "WeaponClass %p: muzzle sprite '%s', size %.3f, duration %.3fs",
            event->weapon_class, policy.sprite.c_str(), policy.size,
            policy.duration);
        g_policies.emplace(event->weapon_class, std::move(policy));
        log_line(message);
    } catch (...) {
        log_line("Could not retain weapon muzzle-flash settings");
    }
}

void emit_flash(void* weapon, void* node) noexcept {
    if (!g_ready || !weapon || !node || g_policies.empty()) return;
    const auto found = g_policies.find(
        read_at<void*>(weapon, kWeaponClassOffset, nullptr));
    if (found == g_policies.end()) return;
    Policy& policy = found->second;
    void* owner = reinterpret_cast<void*>(a2fo_muzzle_call_thiscall_0(
        at(kWeaponGetOwnerRva), weapon));
    if (!owner || read_at<std::uint8_t>(owner, kObjectExpiredOffset, 1) ||
        !readable_range(node, sizeof(void*))) return;

    void* storm = read_at<void*>(at(kStorm3dGlobalRva), 0, nullptr);
    void* database = read_at<void*>(storm, kSpriteDatabaseOffset, nullptr);
    if (!database) return;
    // Native AddParticle does not check FindElement's result before retaining
    // it. Validate first: an unknown sprite must not become a renderer crash.
    const auto sprite = a2fo_muzzle_call_thiscall_2(
        at(kDatabaseFindElementRva), database,
        reinterpret_cast<std::uintptr_t>(policy.sprite.c_str()), 0);
    if (!sprite) {
        if (!policy.missing_sprite_logged) {
            policy.missing_sprite_logged = true;
            char message[384]{};
            std::snprintf(message, sizeof(message),
                "Skipped unknown muzzleFlashSprite '%s' on WeaponClass %p",
                policy.sprite.c_str(), found->first);
            log_line(message);
        }
        return;
    }
    reinterpret_cast<NodeParticleAdd>(at(kNodeParticleAddRva))(
        policy.sprite.c_str(), policy.colour.data(), owner, node,
        policy.size, policy.duration);
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
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->install_inline_hook ||
        !A2FO_MODULE_API_HAS(api, register_weapon_class_loaded_handler) ||
        !api->register_weapon_class_loaded_handler ||
        (api->capabilities & A2FO_CAP_WEAPON_CLASS_LOADED) == 0) return false;
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    if (!g_armada ||
        !signature_matches(kOrdnanceClassBuildRva, kExpectedBuild) ||
        !signature_matches(kWeaponGetOwnerRva, kExpectedGetOwner) ||
        !signature_matches(kNodeParticleAddRva, kExpectedParticleAdd) ||
        !signature_matches(kDatabaseFindElementRva, kExpectedFindElement)) {
        log_line("Muzzle flashes unavailable: native signatures do not match");
        return false;
    }
    if (!api->register_weapon_class_loaded_handler(
            kModuleName, kWeaponFields.data(),
            static_cast<std::uint32_t>(kWeaponFields.size()),
            &weapon_class_loaded_handler, nullptr)) return false;

    g_ready = api->install_inline_hook(
        at(kOrdnanceClassBuildRva),
        reinterpret_cast<void*>(&ordnance_build_hook), sizeof(kExpectedBuild),
        kExpectedBuild, &g_build_hook);
    // Registration retained a callback into this DLL. Keep it loaded even
    // when hook installation fails; the callback remains a disabled no-op.
    log_line(g_ready
        ? "Weapon-ODF muzzle flashes initialized at native projectile creation"
        : "Muzzle flashes disabled: projectile-creation hook unavailable");
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    g_ready = false;
    g_policies.clear();
    // Existing particles are engine-owned and expire through NodeParticleEffect.
}
