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
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "../A2FOResources/api.hpp"
#include "build_time_text.hpp"

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
bool g_inside_tooltip = false;
bool g_logged_modifier_observation = false;
char g_end_extra_override[512]{};

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

bool adjusted_build_time(void* object_class, std::int32_t team,
                         float* output) noexcept {
    if (!output || !object_class ||
        !readable_range(object_class, sizeof(void*))) {
        return false;
    }
    const std::uint32_t duration_bits =
        a2fo_build_tooltips_call_thiscall_float_1_bits(
            at(kGameObjectClassGetBuildTimeRva), object_class,
            static_cast<std::uintptr_t>(team));
    const float native_duration = float_from_bits(duration_bits);
    const float raw_duration = read_value<float>(
        object_class, kGameObjectClassRawBuildTimeOffset, native_duration);
    const float global_modifier = current_global_build_time_modifier();
    const float duration = a2fo::build_tooltips::reconcile_global_build_time(
        raw_duration, native_duration, global_modifier);
    if (!g_logged_modifier_observation && global_modifier != 1.0f) {
        g_logged_modifier_observation = true;
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "Live IA Ship Build Time: raw=%.3f, native=%.3f, "
            "modifier=%.3f, displayed=%.3f (%s)",
            static_cast<double>(raw_duration),
            static_cast<double>(native_duration),
            static_cast<double>(global_modifier),
            static_cast<double>(duration),
            duration == native_duration ? "native" : "reconciled");
        log_line(message);
    }
    *output = duration;
    return true;
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

bool append_buffer(char* output, std::size_t output_size,
                   const char* text) noexcept {
    if (!output || output_size == 0 || !text) return false;
    const std::size_t used = std::strlen(output);
    const std::size_t added = std::strlen(text);
    if (used >= output_size || added >= output_size - used) return false;
    std::memcpy(output + used, text, added + 1);
    return true;
}

bool prepare_cost_suffix(void* mode_info, bool verbose) noexcept {
    g_end_extra_override[0] = '\0';
    if (!mode_info ||
        read_value<std::int32_t>(mode_info, kModeInfoTypeOffset, -1) !=
            kBuildItemType) {
        return false;
    }
    void* object_class = read_value<void*>(
        mode_info, kModeInfoTargetClassOffset, nullptr);
    HMODULE resources = GetModuleHandleA("A2FOResources.dll");
    FARPROC exported = resources
        ? GetProcAddress(resources, "A2FOResources_GetCost") : nullptr;
    A2FOResourcesGetCostFn get_cost = nullptr;
    static_assert(sizeof(exported) == sizeof(get_cost),
                  "unexpected function-pointer size");
    std::memcpy(&get_cost, &exported, sizeof(get_cost));
    FARPROC presentation_export = resources
        ? GetProcAddress(resources, "A2FOResources_GetPresentationText")
        : nullptr;
    A2FOResourcesGetPresentationTextFn get_presentation = nullptr;
    static_assert(sizeof(presentation_export) == sizeof(get_presentation),
                  "unexpected function-pointer size");
    std::memcpy(&get_presentation, &presentation_export,
                sizeof(get_presentation));
    if (!object_class) return false;

    constexpr const char* fallback_names[] = {
        "Tritanium", "Supply", "Credits", "Collective connections"};
    const auto local_team = reinterpret_cast<LocalTeam>(at(kLocalTeamRva));
    const auto team_for_team = reinterpret_cast<TeamForTeam>(
        at(kTeamForTeamRva));
    const std::int32_t team_index = local_team ? local_team() : -1;
    void* team = team_for_team && team_index >= 0 && team_index < 16
        ? team_for_team(team_index) : nullptr;
    const char* separator = localize("GUI_CP_AMOUNT_SEPARATE", "/");
    if (get_cost) {
        for (std::uint32_t index = 0; index < 4; ++index) {
            const std::uint32_t resource = A2FO_RESOURCE_TRITANIUM + index;
            const std::int32_t cost = get_cost(object_class, resource);
            if (cost <= 0) continue;
            const std::uint32_t presentation = verbose
                ? A2FO_RESOURCE_PRESENTATION_RES
                : A2FO_RESOURCE_PRESENTATION_ICON;
            const char* name = get_presentation
                ? get_presentation(team, resource,
                                   presentation)
                : nullptr;
            if (!name || !*name) name = fallback_names[index];
            char token[128]{};
            if (!a2fo::build_tooltips::format_resource_cost_token(
                    separator, name, cost, !verbose, token,
                    sizeof(token)) ||
                !append_buffer(g_end_extra_override,
                               sizeof(g_end_extra_override), token)) {
                g_end_extra_override[0] = '\0';
                return false;
            }
        }
    }
    float duration = 0.0f;
    char build_time[64]{};
    if (!adjusted_build_time(object_class, team_index, &duration) ||
        !a2fo::build_tooltips::format_build_time_token(
            separator, duration, !verbose, build_time,
            sizeof(build_time)) ||
        !append_buffer(g_end_extra_override,
                       sizeof(g_end_extra_override), build_time) ||
        !append_buffer(
            g_end_extra_override, sizeof(g_end_extra_override),
            localize("GUI_CP_END_EXTRA", ")"))) {
        g_end_extra_override[0] = '\0';
        return false;
    }
    return true;
}

std::uintptr_t __attribute__((fastcall)) end_extra_lookup(
    void* manager, void*, const char* key) noexcept {
    if (g_inside_tooltip && g_end_extra_override[0] && key &&
        std::strcmp(key, "GUI_CP_END_EXTRA") == 0) {
        return reinterpret_cast<std::uintptr_t>(g_end_extra_override);
    }
    return a2fo_build_tooltips_call_thiscall_1(
        at(kLocalizationLookupRva), manager,
        reinterpret_cast<std::uintptr_t>(key));
}

bool call_original_and_append(A2FO_InlineHook& hook, void* mode_info,
                              void* string_stream) noexcept {
    if (!hook.gateway) return false;
    if (!g_runtime_ready || g_inside_tooltip) {
        return a2fo_build_tooltips_call_thiscall_1(
                   hook.gateway, mode_info,
                   reinterpret_cast<std::uintptr_t>(string_stream)) != 0;
    }

    g_inside_tooltip = true;
    prepare_cost_suffix(
        mode_info, &hook == &g_button_verbose_hook);
    const bool result = a2fo_build_tooltips_call_thiscall_1(
                            hook.gateway, mode_info,
                            reinterpret_cast<std::uintptr_t>(string_stream)) !=
        0;
    g_end_extra_override[0] = '\0';
    g_inside_tooltip = false;
    return result;
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

bool install_hooks() noexcept {
    if (!g_api || !g_api->install_inline_hook || !g_api->patch_call ||
        !preflight()) {
        return false;
    }
    const bool text_installed = g_api->install_inline_hook(
        at(kModeInfoButtonTextRva),
        reinterpret_cast<void*>(&a2fo_build_tooltips_button_text_bridge),
        kExpectedButtonText.size(), kExpectedButtonText.data(),
        &g_button_text_hook);
    const bool verbose_installed = g_api->install_inline_hook(
        at(kModeInfoButtonVerboseRva),
        reinterpret_cast<void*>(&a2fo_build_tooltips_button_verbose_bridge),
        kExpectedButtonVerbose.size(), kExpectedButtonVerbose.data(),
        &g_button_verbose_hook);
    std::size_t patched = 0;
    for (const CheckedCallPatch& call_site : kEndExtraCallSites) {
        if (g_api->patch_call(
                at(call_site.rva), reinterpret_cast<void*>(&end_extra_lookup),
                call_site.expected.data(), call_site.expected.size())) {
            ++patched;
        }
    }
    if (!text_installed || !verbose_installed ||
        patched != kEndExtraCallSites.size()) {
        char message[160]{};
        std::snprintf(
            message, sizeof(message),
            "Build-tooltip integration partial: text=%s verbose=%s "
            "end-bracket=%lu/%lu; installed hooks remain pass-through",
            text_installed ? "yes" : "no",
            verbose_installed ? "yes" : "no",
            static_cast<unsigned long>(patched),
            static_cast<unsigned long>(kEndExtraCallSites.size()));
        log_line(message);
    }
    return text_installed && verbose_installed &&
        patched == kEndExtraCallSites.size();
}

}  // namespace

extern "C" std::uintptr_t __cdecl
a2fo_build_tooltips_button_text_hook_cpp(void* mode_info,
                                         void* string_stream) noexcept {
    return call_original_and_append(
        g_button_text_hook, mode_info, string_stream);
}

extern "C" std::uintptr_t __cdecl
a2fo_build_tooltips_button_verbose_hook_cpp(void* mode_info,
                                            void* string_stream) noexcept {
    return call_original_and_append(
        g_button_verbose_hook, mode_info, string_stream);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->install_inline_hook ||
        !api->patch_call) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    if (!g_armada) return false;

    g_runtime_ready = install_hooks();
    log_line(g_runtime_ready
                 ? "Build-time tooltip text initialized"
                 : "Build-time tooltip module loaded with runtime disabled");
    // Hooks are process-lifetime patches. Keep the DLL resident after a
    // partial installation so an installed entry remains a safe pass-through.
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    g_runtime_ready = false;
}
