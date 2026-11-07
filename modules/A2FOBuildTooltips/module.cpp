#include "../../sdk/include/a2fo_module_provider.hpp"
/*
 * Build-time text for build-button tooltips.
 *
 * ModeInfo already owns the object class selected by the button. Calling
 * GameObjectClass::Get_Build_Time for the local team gives the duration used
 * when Producer starts construction. Armada conditionally applies the global
 * game-setup multiplier by class metadata, so this module also reads that live
 * multiplier and reconciles a result which remains at raw ODF time. A targeted
 * final-delimiter call replacement inserts the adjusted time and additional
 * resources into Armada's own parenthesised cost row.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/module_runtime_a5_ffi.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "../A2FOResources/api.hpp"
#include "build_time_text.hpp"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {
std::uintptr_t __cdecl a2fo_build_tooltips_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_build_tooltips_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
std::uint32_t __cdecl a2fo_build_tooltips_call_thiscall_float_1_bits(
    void* function, void* self, std::uintptr_t argument1);
std::uint32_t __cdecl a2fo_build_tooltips_call_thiscall_float_0_bits(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_build_tooltips_button_text_bridge();
std::uintptr_t __cdecl a2fo_build_tooltips_button_verbose_bridge();
}

namespace {

constexpr char kModuleName[] = "A2FOBuildTooltips";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kModeInfoButtonTextRva = 0x000e6ca0;
constexpr std::uintptr_t kModeInfoButtonVerboseRva = 0x000e72b0;
constexpr std::uintptr_t kGameObjectClassGetBuildTimeRva = 0x000ce290;
constexpr std::uintptr_t kLocalTeamRva = 0x000d0060;
constexpr std::uintptr_t kTeamForTeamRva = 0x00096340;
constexpr std::uintptr_t kGetGameSetupRva = 0x00157940;
constexpr std::uintptr_t kGameSetupGetBuildTimeModifierRva = 0x00146360;
constexpr std::uintptr_t kCurrentSetupShellRva = 0x0036b8d4;
constexpr std::uintptr_t kLocalizationLookupRva = 0x00081c90;
constexpr std::uintptr_t kLocalizationManagerPointerRva = 0x003379fc;

constexpr std::size_t kModeInfoTypeOffset = 0x04;
constexpr std::size_t kModeInfoTargetClassOffset = 0x0c;
constexpr std::size_t kGameObjectClassRawBuildTimeOffset = 0x68;
constexpr std::int32_t kBuildItemType = 1;

constexpr std::array<std::uint8_t, 10> kExpectedButtonText{
    0x55, 0x8b, 0xec, 0x6a, 0xff,
    0x68, 0x5b, 0xed, 0x69, 0x00};
constexpr std::array<std::uint8_t, 10> kExpectedButtonVerbose{
    0x55, 0x8b, 0xec, 0x6a, 0xff,
    0x68, 0x80, 0xed, 0x69, 0x00};
constexpr std::array<std::uint8_t, 9> kExpectedGetBuildTime{
    0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x57, 0x8b, 0xf9};
constexpr std::array<std::uint8_t, 10> kExpectedLocalTeam{
    0x8b, 0x0d, 0xd4, 0xb8, 0x76,
    0x00, 0xe9, 0x45, 0x78, 0x08};
constexpr std::array<std::uint8_t, 15> kExpectedTeamForTeam{{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x8b, 0x04,
    0x85, 0xb0, 0x8d, 0x73, 0x00, 0x5d, 0xc3}};
constexpr std::array<std::uint8_t, 4> kExpectedGetGameSetup{
    0x8b, 0x41, 0x30, 0xc3};
constexpr std::array<std::uint8_t, 10> kExpectedGetBuildTimeModifier{
    0x8b, 0x41, 0x10, 0xd9, 0x80,
    0x2c, 0x03, 0x00, 0x00, 0xc3};

struct CheckedCallPatch {
    std::uintptr_t rva;
    std::array<std::uint8_t, 5> expected;
};

// Final GUI_CP_END_EXTRA lookups in ButtonText and ButtonVerbose. Replacing
// only these calls lets additional resources join the native parenthesised
// cost row without rebuilding either complete formatter.
constexpr std::array<CheckedCallPatch, 2> kEndExtraCallSites{{
    {0x000e711a, {{0xe8, 0x71, 0xab, 0xf9, 0xff}}},
    {0x000e772a, {{0xe8, 0x61, 0xa5, 0xf9, 0xff}}},
}};

using LocalTeam = std::int32_t (__cdecl*)();
using TeamForTeam = void* (__cdecl*)(std::int32_t);
const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
A2FO_InlineHook g_button_text_hook{};
A2FO_InlineHook g_button_verbose_hook{};
bool g_runtime_ready = false;
thread_local bool g_inside_tooltip = false;
bool g_logged_modifier_observation = false;
thread_local char g_end_extra_override[512]{};

void* at(std::uintptr_t rva) noexcept {
    return g_armada
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(g_armada) + rva)
        : nullptr;
}

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

bool readable_range(const void* pointer, std::size_t size) noexcept {
    if (!pointer || size == 0) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    if (begin + size < begin) return false;

    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(pointer, &information, sizeof(information)) !=
            sizeof(information) ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const auto region_begin = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress);
    const auto region_end = region_begin + information.RegionSize;
    return begin >= region_begin && begin + size <= region_end;
}

template <typename T>
T read_value(const void* base, std::size_t offset,
             T fallback = T{}) noexcept {
    if (!base) return fallback;
    const auto* address = static_cast<const std::uint8_t*>(base) + offset;
    if (!readable_range(address, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

bool signature_matches(std::uintptr_t rva, const std::uint8_t* expected,
                       std::size_t size) noexcept {
    const void* address = at(rva);
    return readable_range(address, size) &&
           std::memcmp(address, expected, size) == 0;
}

float float_from_bits(std::uint32_t bits) noexcept {
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

float current_global_build_time_modifier() noexcept {
    void* setup_shell = read_value<void*>(
        at(kCurrentSetupShellRva), 0, nullptr);
    if (!setup_shell || !readable_range(setup_shell, sizeof(void*))) {
        return 1.0f;
    }
    void* game_setup = reinterpret_cast<void*>(
        a2fo_build_tooltips_call_thiscall_0(
            at(kGetGameSetupRva), setup_shell));
    if (!game_setup || !readable_range(game_setup, sizeof(void*))) {
        return 1.0f;
    }
    return float_from_bits(
        a2fo_build_tooltips_call_thiscall_float_0_bits(
            at(kGameSetupGetBuildTimeModifierRva), game_setup));
}

const char* localize(const char* key, const char* fallback) noexcept {
    void* manager = read_value<void*>(
        at(kLocalizationManagerPointerRva), 0, nullptr);
    if (!manager || !key) return fallback;
    const auto translated = reinterpret_cast<const char*>(
        a2fo_build_tooltips_call_thiscall_1(
            at(kLocalizationLookupRva), manager,
            reinterpret_cast<std::uintptr_t>(key)));
    return translated && *translated ? translated : fallback;
}

struct TooltipContext {
    const A2FO_ModuleApi* supplied=nullptr;
    A2FO_InlineHook* hook=nullptr;
    void* mode_info=nullptr;
    void* string_stream=nullptr;
    void* object_class=nullptr;
    void* team=nullptr;
    A2FOResourcesGetCostFn get_cost=nullptr;
    A2FOResourcesGetPresentationTextFn get_presentation=nullptr;
    bool verbose=false;
};
std::uintptr_t tooltip_step(void*,std::uint32_t,std::uint32_t,A2foRsTooltipMeasure*) noexcept;

std::uintptr_t __attribute__((fastcall)) end_extra_lookup(
    void* manager, void*, const char* key) noexcept {
    if (a2fo_rs_runtime_tooltip_override(g_inside_tooltip,
        g_end_extra_override[0] != '\0',key)) {
        return reinterpret_cast<std::uintptr_t>(g_end_extra_override);
    }
    return a2fo_build_tooltips_call_thiscall_1(
        at(kLocalizationLookupRva), manager,
        reinterpret_cast<std::uintptr_t>(key));
}

bool preflight() noexcept {
    using a2fo::supported_armada::Identity;
    if (a2fo::supported_armada::identify(g_armada) == Identity::unsupported) {
        log_line("Unsupported ArmadaL executable; runtime disabled");
        return false;
    }
    if (!signature_matches(kModeInfoButtonTextRva,
                           kExpectedButtonText.data(),
                           kExpectedButtonText.size()) ||
        !signature_matches(kModeInfoButtonVerboseRva,
                           kExpectedButtonVerbose.data(),
                           kExpectedButtonVerbose.size()) ||
        !signature_matches(kGameObjectClassGetBuildTimeRva,
                           kExpectedGetBuildTime.data(),
                           kExpectedGetBuildTime.size()) ||
        !signature_matches(kLocalTeamRva, kExpectedLocalTeam.data(),
                           kExpectedLocalTeam.size()) ||
        !signature_matches(kTeamForTeamRva, kExpectedTeamForTeam.data(),
                           kExpectedTeamForTeam.size()) ||
        !signature_matches(kGetGameSetupRva, kExpectedGetGameSetup.data(),
                           kExpectedGetGameSetup.size()) ||
        !signature_matches(kGameSetupGetBuildTimeModifierRva,
                           kExpectedGetBuildTimeModifier.data(),
                           kExpectedGetBuildTimeModifier.size()) ||
        !readable_range(at(kCurrentSetupShellRva), sizeof(void*))) {
        log_line("Supported build-tooltip signatures were not found; "
                 "runtime disabled");
        return false;
    }
    for (const CheckedCallPatch& call_site : kEndExtraCallSites) {
        if (!signature_matches(call_site.rva, call_site.expected.data(),
                               call_site.expected.size())) {
            log_line("Supported build-tooltip end-bracket calls were not "
                     "found; runtime disabled");
            return false;
        }
    }
    return true;
}

std::uintptr_t tooltip_step(void* opaque,std::uint32_t operation,std::uint32_t argument,
                            A2foRsTooltipMeasure* value) noexcept {
    auto& context=*static_cast<TooltipContext*>(opaque);
    switch(operation) {
    case 0:g_end_extra_override[0]='\0';return 1;
    case 1:return read_value<std::int32_t>(context.mode_info,kModeInfoTypeOffset,-1);
    case 2:context.object_class=read_value<void*>(context.mode_info,kModeInfoTargetClassOffset,nullptr);
        return reinterpret_cast<std::uintptr_t>(context.object_class);
    case 3: {
        HMODULE resources=a2fo::startup_module_provider("A2FOResources.dll",
            {"A2FOResources_GetCost","A2FOResources_GetPresentationText"});
        FARPROC exported=resources?GetProcAddress(resources,"A2FOResources_GetCost"):nullptr;
        static_assert(sizeof(exported)==sizeof(context.get_cost));
        std::memcpy(&context.get_cost,&exported,sizeof(exported));
        exported=resources?GetProcAddress(resources,"A2FOResources_GetPresentationText"):nullptr;
        std::memcpy(&context.get_presentation,&exported,sizeof(exported));
        return context.get_cost!=nullptr;
    }
    case 4: {
        const auto team=reinterpret_cast<LocalTeam>(at(kLocalTeamRva));
        return static_cast<std::uintptr_t>(team?team():-1);
    }
    case 5: {
        const auto function=reinterpret_cast<TeamForTeam>(at(kTeamForTeamRva));
        const auto team=static_cast<std::int32_t>(argument);
        context.team=function&&team>=0&&team<16?function(team):nullptr;
        return reinterpret_cast<std::uintptr_t>(context.team);
    }
    case 6:return reinterpret_cast<std::uintptr_t>(localize("GUI_CP_AMOUNT_SEPARATE","/"));
    case 7:return context.get_cost?static_cast<std::uintptr_t>(context.get_cost(
        context.object_class,A2FO_RESOURCE_TRITANIUM+argument)):0;
    case 8:return reinterpret_cast<std::uintptr_t>(context.get_presentation
        ?context.get_presentation(context.team,A2FO_RESOURCE_TRITANIUM+argument,
            context.verbose?A2FO_RESOURCE_PRESENTATION_RES:A2FO_RESOURCE_PRESENTATION_ICON)
        :nullptr);
    case 9: {
        if(!context.object_class||!readable_range(context.object_class,sizeof(void*)))return 0;
        const auto bits=a2fo_build_tooltips_call_thiscall_float_1_bits(
            at(kGameObjectClassGetBuildTimeRva),context.object_class,argument);
        value->native_duration=float_from_bits(bits);
        value->raw=read_value<float>(context.object_class,kGameObjectClassRawBuildTimeOffset,
                                     value->native_duration);
        value->modifier=current_global_build_time_modifier();
        return 1;
    }
    case 10:return reinterpret_cast<std::uintptr_t>(localize("GUI_CP_END_EXTRA",")"));
    case 11:
        if(!value->bytes||value->size>=sizeof(g_end_extra_override))return 0;
        std::memcpy(g_end_extra_override,value->bytes,value->size);
        g_end_extra_override[value->size]='\0';return 1;
    case 12: {
        g_logged_modifier_observation=true;
        char message[192]{};
        std::snprintf(message,sizeof(message),
            "Live IA Ship Build Time: raw=%.3f, native=%.3f, "
            "modifier=%.3f, displayed=%.3f (%s)",
            static_cast<double>(value->raw),static_cast<double>(value->native_duration),
            static_cast<double>(value->modifier),static_cast<double>(value->displayed),
            value->displayed==value->native_duration?"native":"reconciled");
        log_line(message);return 1;
    }
    case 13:return g_logged_modifier_observation;
    case 19:return context.verbose;
    case 21:return a2fo_build_tooltips_call_thiscall_1(context.hook->gateway,
        context.mode_info,reinterpret_cast<std::uintptr_t>(context.string_stream));
    case 22:g_inside_tooltip=true;return 1;
    case 23:g_end_extra_override[0]='\0';return 1;
    case 24:g_inside_tooltip=false;return 1;
    case 30:g_api=context.supplied;g_armada=static_cast<HMODULE>(g_api->armada_module());
        return g_armada!=nullptr;
    case 31:return preflight();
    case 32:return g_api->install_inline_hook(at(kModeInfoButtonTextRva),
        reinterpret_cast<void*>(&a2fo_build_tooltips_button_text_bridge),
        kExpectedButtonText.size(),kExpectedButtonText.data(),&g_button_text_hook);
    case 33:return g_api->install_inline_hook(at(kModeInfoButtonVerboseRva),
        reinterpret_cast<void*>(&a2fo_build_tooltips_button_verbose_bridge),
        kExpectedButtonVerbose.size(),kExpectedButtonVerbose.data(),&g_button_verbose_hook);
    case 34: {
        const auto& site=kEndExtraCallSites[argument];
        return g_api->patch_call(at(site.rva),reinterpret_cast<void*>(&end_extra_lookup),
            site.expected.data(),site.expected.size());
    }
    case 36: {
        char message[160]{};
        std::snprintf(message,sizeof(message),
            "Build-tooltip integration partial: text=%s verbose=%s "
            "end-bracket=%lu/%lu; installed hooks remain pass-through",
            argument&1?"yes":"no",argument&2?"yes":"no",
            static_cast<unsigned long>(argument>>2),
            static_cast<unsigned long>(kEndExtraCallSites.size()));
        log_line(message);return 1;
    }
    case 37:g_runtime_ready=argument!=0;
        log_line(g_runtime_ready?"Build-time tooltip text initialized":
            "Build-time tooltip module loaded with runtime disabled");return 1;
    case 38:g_runtime_ready=false;return 1;
    default:return 0;
    }
}

}  // namespace

extern "C" std::uintptr_t __cdecl
a2fo_build_tooltips_button_text_hook_cpp(void* mode_info,
                                         void* string_stream) noexcept {
    TooltipContext context{nullptr,&g_button_text_hook,mode_info,string_stream,
        nullptr,nullptr,nullptr,nullptr,false};
#ifdef A2FO_BUILTIN_FEATURE
    const bool enabled=g_button_text_hook.gateway&&
        a2fo::builtin_module_enabled(A2FO_BUILTIN_FEATURE);
#else
    const bool enabled=true;
#endif
    return a2fo_rs_runtime_tooltip_call(g_button_text_hook.gateway!=nullptr,
        enabled,g_runtime_ready,g_inside_tooltip,tooltip_step,&context);
}

extern "C" std::uintptr_t __cdecl
a2fo_build_tooltips_button_verbose_hook_cpp(void* mode_info,
                                            void* string_stream) noexcept {
    TooltipContext context{nullptr,&g_button_verbose_hook,mode_info,string_stream,
        nullptr,nullptr,nullptr,nullptr,true};
#ifdef A2FO_BUILTIN_FEATURE
    const bool enabled=g_button_verbose_hook.gateway&&
        a2fo::builtin_module_enabled(A2FO_BUILTIN_FEATURE);
#else
    const bool enabled=true;
#endif
    return a2fo_rs_runtime_tooltip_call(g_button_verbose_hook.gateway!=nullptr,
        enabled,g_runtime_ready,g_inside_tooltip,tooltip_step,&context);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    const bool valid=api&&api->struct_size>=A2FO_MODULE_API_V4_BASE_SIZE&&
        api->api_version==A2FO_MODULE_API_VERSION&&api->log&&api->armada_module&&
        api->install_inline_hook&&api->patch_call;
    TooltipContext context{};context.supplied=api;
    return a2fo_rs_runtime_tooltip_init(valid,tooltip_step,&context)!=0;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    TooltipContext context{};
    a2fo_rs_runtime_tooltip_shutdown(tooltip_step,&context);
}
