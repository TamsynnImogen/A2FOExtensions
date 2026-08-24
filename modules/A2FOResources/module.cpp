/*
 * Four independent team resources layered beside Armada II's six native
 * resource slots. Fleet Operations' historic UI aliases are deliberately not
 * reused: tritanium, supply, credits, and collective connections each retain
 * their own balance and cost.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "additional_resources.hpp"
#include "api.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>

extern "C" {
std::uintptr_t __cdecl a2fo_resources_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_resources_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument);
std::uintptr_t __cdecl a2fo_resources_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
std::uintptr_t __cdecl a2fo_resources_call_thiscall_7(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4, std::uintptr_t argument5,
    std::uintptr_t argument6, std::uintptr_t argument7);
void a2fo_resources_deduct_hook();
void* g_a2fo_resources_deduct_gateway = nullptr;
}

namespace {

using a2fo::resources::Amounts;
using a2fo::resources::Costs;
using a2fo::resources::Resource;
using a2fo::resources::kResourceCount;

constexpr char kModuleName[] = "A2FOResources";
constexpr std::uint32_t kFirstAdditionalResource = A2FO_RESOURCE_TRITANIUM;
constexpr std::uint32_t kTotalResourceCount = A2FO_RESOURCE_COUNT;
constexpr std::size_t kNativeResourceCount = 6;

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kTeamConstructorRva = 0x00095670;
constexpr std::uintptr_t kTeamDestructorRva = 0x00095850;
constexpr std::uintptr_t kGetGameSetupRva = 0x00157940;
constexpr std::uintptr_t kGameSetupGetStartingResourcesIndexRva = 0x00146310;
constexpr std::uintptr_t kGameSetupInfiniteResourcesRva = 0x00146400;
constexpr std::uintptr_t kCurrentSetupShellRva = 0x0036b8d4;
constexpr std::uintptr_t kResourcePanelRenderRva = 0x000ffa40;
constexpr std::uintptr_t kLocalTeamRva = 0x000d0060;
constexpr std::uintptr_t kTeamForTeamRva = 0x00096340;
constexpr std::uintptr_t kParameterDbGetRectangleRva = 0x001358f0;
constexpr std::uintptr_t kDisplayInterfaceDrawTextRva = 0x0011b160;
constexpr std::uintptr_t kGuiParameterDbPointerRva = 0x0036502c;
constexpr std::uintptr_t kLocalizationLookupRva = 0x00081c90;
constexpr std::uintptr_t kLocalizationManagerPointerRva = 0x003379fc;
constexpr std::uintptr_t kInsertCStringIatRva = 0x003b7dec;
constexpr std::uintptr_t kResourceComponentTooltipRva = 0x0010a350;
constexpr std::uintptr_t kResourceComponentVerboseTooltipRva = 0x0010a500;
constexpr std::uintptr_t kCursorXRva = 0x00365018;
constexpr std::uintptr_t kCursorYRva = 0x0036501c;
constexpr std::uintptr_t kStandardComponentSetTooltipTextRva = 0x0010c040;
constexpr std::uintptr_t kStandardComponentSetVerboseTooltipTextRva =
    0x0010c080;

// FleetOpsHook.dll Delphi-register routine: EAX=Producer, EDX=target class.
constexpr std::uintptr_t kDeductResourcesForBuildRva = 0x001226ec;

constexpr std::size_t kProducerTeamOffset = 0x00f0;
// Armada resolves the selected race while completing Team setup and stores
// the same Race object dispatched by the core's Race-loaded callback here.
constexpr std::size_t kTeamRaceOffset = 0x0244;
constexpr std::size_t kResourcePanelFirstTextOffset = 0x002c;
// ResourcePanel stores six ResourceDisplay wrappers at +0x2c. Each wrapper
// owns the actual GUIText component at +0x2c; the live rectangle, colour,
// display interface and font state belong to that child, not the wrapper.
constexpr std::size_t kResourceDisplayTextOffset = 0x002c;
constexpr std::size_t kResourceComponentResourceOffset = 0x0028;
constexpr std::size_t kTextComponentLiveRectangleOffset = 0x0058;
constexpr std::size_t kResourcePanelBackgroundOffset = 0x0028;
constexpr std::size_t kOstreamSubobjectOffset = 0x0008;

constexpr std::array<const char*, kResourceCount> kCostCommands{{
    "tritaniumCost",
    "supplyCost",
    "creditsCost",
    "collectiveconnectionsCost",
}};

constexpr std::array<const char*, kResourceCount> kNormalCommands{{
    "normalTritanium",
    "normalSupply",
    "normalCredits",
    "normalCollectiveConnections",
}};

constexpr std::array<const char*, kResourceCount> kLotsCommands{{
    "lotsTritanium",
    "lotsSupply",
    "lotsCredits",
    "lotsCollectiveConnections",
}};

constexpr std::size_t kPresentationCount =
    A2FO_RESOURCE_PRESENTATION_COUNT;
constexpr std::size_t kPresentationFieldCount =
    kTotalResourceCount * kPresentationCount;

// Resource-major order: display label, short tooltip, verbose tooltip,
// compact icon glyph. Icon fields are optional; Fleet Operations' canonical
// high-byte glyphs remain the defaults.
constexpr std::array<const char*, kPresentationFieldCount>
    kPresentationCommands{{
        "crewRes", "crewTooltip", "crewVerboseTooltip", "crewIcon",
        "officerRes", "officerTooltip", "officerVerboseTooltip",
        "officerIcon",
        "dilithiumRes", "dilithiumTooltip", "dilithiumVerboseTooltip",
        "dilithiumIcon",
        "latinumRes", "latinumTooltip", "latinumVerboseTooltip",
        "latinumIcon",
        "metalRes", "metalTooltip", "metalVerboseTooltip", "metalIcon",
        "biomatterRes", "biomatterTooltip", "biomatterVerboseTooltip",
        "biomatterIcon",
        "tritaniumRes", "tritaniumTooltip", "tritaniumVerboseTooltip",
        "tritaniumIcon",
        "supplyRes", "supplyTooltip", "supplyVerboseTooltip",
        "supplyIcon",
        "creditsRes", "creditsTooltip", "creditsVerboseTooltip",
        "creditsIcon",
        "collectiveconnectionsRes", "collectiveconnectionsTooltip",
        "collectiveconnectionsVerboseTooltip", "collectiveconnectionsIcon",
    }};

constexpr std::array<const char*, kTotalResourceCount> kDefaultResourceNames{{
    "Crew", "Officers", "Dilithium", "Latinum", "Metal", "Biomatter",
    "Tritanium", "Supply", "Credits", "Collective connections"}};

// The test font reserves a contiguous high-byte glyph block for the ten
// independent resources. Byte 0x83 is the non-resource energy picture and
// 0x8a is build time, so neither appears in this table. Officers deliberately
// retain Armada's localized text until dedicated artwork is supplied.
constexpr std::array<const char*, kTotalResourceCount> kDefaultResourceIcons{{
    "\x88",  // crew
    "",      // officers (no dedicated icon yet)
    "\x84",  // dilithium
    "\x81",  // latinum
    "\x80",  // metal
    "\x82",  // biomatter
    "\x87",  // tritanium
    "\x86",  // supply
    "\x89",  // credits
    "\x85",  // collective connections
}};

// FontFinal4_16a advances for the four additional-resource glyphs, including
// the following space used by the previous combined "icon amount" string.
// Scale them from the live row height so custom UI resolutions retain the
// same placement.
constexpr std::array<float, kResourceCount> kPanelIconAdvanceRatios{{
    29.0f / 24.0f,  // tritanium: 23px glyph + 6px space
    31.0f / 24.0f,  // supply: 25px glyph + 6px space
    34.0f / 24.0f,  // credits: 28px glyph + 6px space
    27.0f / 24.0f,  // collective connections: 21px glyph + 6px space
}};

// Canonical keys requested by Armada's native cost formatters. Officers use
// Race::officerRes directly and therefore require no A2FO substitution.
constexpr std::array<const char*, kNativeResourceCount>
    kNativeResourceResKeys{{
        "GUI_CP_CREW_RES", nullptr, "GUI_CP_DILITHIUM_RES",
        "GUI_CP_LATINUM_RES", "GUI_CP_METAL_RES",
        "GUI_CP_BIOMATTER_RES"}};

constexpr std::array<std::uint8_t, 10> kExpectedTeamConstructor{{
    0x55, 0x8b, 0xec, 0x6a, 0xff,
    0x68, 0x1d, 0xca, 0x69, 0x00}};
constexpr std::array<std::uint8_t, 7> kExpectedTeamDestructor{{
    0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x57}};
constexpr std::array<std::uint8_t, 7> kExpectedDeductResources{{
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xc8, 0x53}};
constexpr std::array<std::uint8_t, 9> kExpectedResourcePanelRender{{
    0xa1, 0xcc, 0x43, 0x76, 0x00, 0x56, 0x8b, 0xf1, 0x57}};
constexpr std::array<std::uint8_t, 10> kExpectedLocalTeam{{
    0x8b, 0x0d, 0xd4, 0xb8, 0x76,
    0x00, 0xe9, 0x45, 0x78, 0x08}};
constexpr std::array<std::uint8_t, 15> kExpectedTeamForTeam{{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x8b, 0x04,
    0x85, 0xb0, 0x8d, 0x73, 0x00, 0x5d, 0xc3}};
constexpr std::array<std::uint8_t, 9> kExpectedParameterDbGetRectangle{{
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x04, 0x01, 0x00, 0x00}};
constexpr std::array<std::uint8_t, 6> kExpectedDisplayInterfaceDrawText{{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x10}};
constexpr std::array<std::uint8_t, 8> kExpectedSetTooltipText{{
    0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1, 0x8b, 0x46}};
constexpr std::array<std::uint8_t, 10> kExpectedResourceComponentTooltip{{
    0x55, 0x8b, 0xec, 0x81, 0xec,
    0x04, 0x04, 0x00, 0x00, 0x56}};

struct CheckedCallPatch {
    std::uintptr_t rva;
    std::array<std::uint8_t, 5> expected;
};

// The normal and verbose cost formatters each contain one localization call
// for the five native resources that lack Race-owned fields.
constexpr std::array<CheckedCallPatch, 10> kNativeResourceResCallSites{{
    {0x000e6e1d, {{0xe8, 0x6e, 0xae, 0xf9, 0xff}}},
    {0x000e6ea1, {{0xe8, 0xea, 0xad, 0xf9, 0xff}}},
    {0x000e6f25, {{0xe8, 0x66, 0xad, 0xf9, 0xff}}},
    {0x000e6fa9, {{0xe8, 0xe2, 0xac, 0xf9, 0xff}}},
    {0x000e7029, {{0xe8, 0x62, 0xac, 0xf9, 0xff}}},
    {0x000e7462, {{0xe8, 0x29, 0xa8, 0xf9, 0xff}}},
    {0x000e74de, {{0xe8, 0xad, 0xa7, 0xf9, 0xff}}},
    {0x000e755a, {{0xe8, 0x31, 0xa7, 0xf9, 0xff}}},
    {0x000e75d6, {{0xe8, 0xb5, 0xa6, 0xf9, 0xff}}},
    {0x000e764e, {{0xe8, 0x3d, 0xa6, 0xf9, 0xff}}},
}};

// The normal formatter resolves Race::officerRes separately from the other
// five native resources. Its verbose counterpart remains untouched so it can
// continue to print the Race-specific full officer name.
constexpr CheckedCallPatch kNativeOfficerIconCallSite{
    0x000e70d8, {{0xe8, 0xb3, 0xab, 0xf9, 0xff}}};

constexpr std::array<const char*, kResourceCount> kPanelRectangleKeys{{
    "resource_6", "resource_7", "resource_8", "resource_9"}};

struct RawRectangle {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

constexpr std::array<RawRectangle, kResourceCount> kDefaultPanelRectangles{{
    {50, 30, 120, 18},
    {250, 30, 120, 18},
    {450, 30, 120, 18},
    {650, 30, 120, 18},
}};

struct NativeRectangle {
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t right = 0;
    std::int32_t bottom = 0;
};

struct Colour {
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
};

struct PanelConfiguration {
    void* parameter_db = nullptr;
    bool loaded = false;
    std::array<bool, kNativeResourceCount> native_rectangle_found{};
    std::array<RawRectangle, kNativeResourceCount> native_rectangles{};
    std::array<bool, kResourceCount> rectangle_found{};
    std::array<RawRectangle, kResourceCount> rectangles{};
};

struct RaceStartingResources {
    Amounts normal{};
    Amounts lots{};
};

using ResourcePresentation =
    std::array<std::array<std::string, kPresentationCount>,
               kTotalResourceCount>;

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
CRITICAL_SECTION g_lock;
bool g_lock_ready = false;
bool g_runtime_alive = false;
bool g_production_integration_ready = false;
A2FO_InlineHook g_team_constructor_hook{};
A2FO_InlineHook g_team_destructor_hook{};
A2FO_InlineHook g_deduct_hook{};
A2FO_InlineHook g_resource_panel_render_hook{};
A2FO_InlineHook g_resource_component_tooltip_hook{};
A2FO_InlineHook g_resource_component_verbose_tooltip_hook{};
std::unordered_map<void*, Amounts> g_team_amounts;
std::unordered_map<void*, Costs> g_class_costs;
std::unordered_map<void*, RaceStartingResources> g_race_starting_resources;
std::unordered_map<void*, ResourcePresentation> g_race_presentations;
std::uint32_t g_presentation_generation = 0;
PanelConfiguration g_panel_configuration{};
void* g_panel_tooltip_component = nullptr;
void* g_panel_tooltip_team = nullptr;
std::int32_t g_panel_tooltip_resource = -1;

struct PresentationCache {
    void* team = nullptr;
    void* race = nullptr;
    std::uint32_t generation = 0;
    std::array<std::array<std::string, kPresentationCount>, kResourceCount>
        added{};
    std::array<std::array<std::string, kPresentationCount>,
               kNativeResourceCount>
        native{};
};

PresentationCache g_presentation_cache{};

struct PanelTextCache {
    void* team = nullptr;
    std::uint32_t generation = 0;
    Amounts amounts{};
    std::array<std::string, kResourceCount> lines{};
};

PanelTextCache g_panel_text_cache{};

class LockGuard {
public:
    LockGuard() { EnterCriticalSection(&g_lock); }
    ~LockGuard() { LeaveCriticalSection(&g_lock); }
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
};

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t* expected,
                       std::size_t length) noexcept {
    return module && expected &&
        std::memcmp(at(module, rva), expected, length) == 0;
}

bool readable_range(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress);
    return start >= base && size <= information.RegionSize - (start - base);
}

template <typename T>
T read_at(const void* base, std::size_t offset, T fallback = T{}) noexcept {
    if (!base) return fallback;
    const auto* address = static_cast<const std::uint8_t*>(base) + offset;
    if (!readable_range(address, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

// ResourcePanel::Render has just traversed these same objects before our
// chained hook runs. Re-querying every field through VirtualQuery dozens of
// times per frame is costly under Wine, so the render-only path uses direct
// copies after null checks. Untrusted/event/API pointers continue to use
// read_at above.
template <typename T>
T read_live(const void* base, std::size_t offset,
            T fallback = T{}) noexcept {
    if (!base) return fallback;
    T value{};
    std::memcpy(&value,
                static_cast<const std::uint8_t*>(base) + offset,
                sizeof(value));
    return value;
}

bool event_field(const A2FO_OdfFieldView* fields, std::uint32_t count,
                 const char* name, std::string* value) {
    if (!fields || !name || !value) return false;
    const std::size_t name_size = std::strlen(name);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto& field = fields[index];
        if (!field.name.data || field.name.size != name_size ||
            _strnicmp(field.name.data, name, name_size) != 0 ||
            (!field.value.data && field.value.size != 0)) {
            continue;
        }
        value->assign(field.value.data ? field.value.data : "",
                      field.value.size);
        return true;
    }
    return false;
}

template <typename Values>
void parse_fields(const A2FO_OdfFieldView* fields, std::uint32_t count,
                  const std::array<const char*, kResourceCount>& commands,
                  Values* values) {
    if (!values) return;
    for (std::size_t index = 0; index < kResourceCount; ++index) {
        std::string text;
        std::int32_t parsed = 0;
        if (event_field(fields, count, commands[index], &text) &&
            a2fo::resources::parse_nonnegative(text, &parsed)) {
            (*values)[index] = parsed;
        }
    }
}

void parse_presentation_fields(const A2FO_OdfFieldView* fields,
                               std::uint32_t count,
                               ResourcePresentation* presentation) {
    if (!presentation) return;
    for (std::size_t resource = 0; resource < kTotalResourceCount;
         ++resource) {
        for (std::size_t kind = 0; kind < kPresentationCount; ++kind) {
            std::string value;
            const std::size_t field =
                resource * kPresentationCount + kind;
            if (event_field(fields, count, kPresentationCommands[field],
                            &value)) {
                (*presentation)[resource][kind] = std::move(value);
            }
        }
    }
}

void* team_race(void* team) noexcept {
    return read_at<void*>(team, kTeamRaceOffset, nullptr);
}

void* local_team_object() noexcept {
    using LocalTeam = std::int32_t (__cdecl*)();
    using TeamForTeam = void* (__cdecl*)(std::int32_t);
    const auto local_team = reinterpret_cast<LocalTeam>(
        at(g_armada, kLocalTeamRva));
    const auto team_for_team = reinterpret_cast<TeamForTeam>(
        at(g_armada, kTeamForTeamRva));
    if (!local_team || !team_for_team) return nullptr;
    const std::int32_t index = local_team();
    // Armada's TeamForTeam helper indexes its global pointer table directly.
    // Localization also runs in shell states where no local Team exists, so
    // never pass an unset or otherwise implausible index into that helper.
    if (index < 0 || index >= 16) return nullptr;
    return team_for_team(index);
}

std::string presentation_key(void* team, std::uint32_t resource,
                             std::uint32_t kind) {
    if (!team || resource >= kTotalResourceCount ||
        kind >= kPresentationCount || !g_lock_ready) {
        return {};
    }
    void* race = team_race(team);
    if (!race) return {};
    LockGuard lock;
    const auto found = g_race_presentations.find(race);
    return found == g_race_presentations.end()
        ? std::string{} : found->second[resource][kind];
}

bool localized_presentation_override(void* team, std::uint32_t resource,
                                     std::uint32_t kind,
                                     std::string* output) noexcept {
    if (!output || resource >= kTotalResourceCount ||
        kind >= kPresentationCount) {
        return false;
    }
    std::string key;
    try {
        key = presentation_key(team, resource, kind);
    } catch (...) {
        return false;
    }
    if (key.empty()) return false;

    void* manager = read_at<void*>(
        at(g_armada, kLocalizationManagerPointerRva), 0, nullptr);
    void* lookup = at(g_armada, kLocalizationLookupRva);
    const char* translated = nullptr;
    if (manager && lookup) {
        translated = reinterpret_cast<const char*>(
            a2fo_resources_call_thiscall_1(
                lookup, manager,
                reinterpret_cast<std::uintptr_t>(key.c_str())));
    }
    try {
        // Match the practical behavior of the existing officer fields: a
        // value is a localization key when present in the string table and
        // otherwise remains useful as literal display text.
        output->assign(translated && *translated ? translated : key);
    } catch (...) {
        output->clear();
        return false;
    }
    return !output->empty();
}

const char* localize_presentation(void* team, std::uint32_t resource,
                                  std::uint32_t kind) noexcept {
    if (resource >= kTotalResourceCount || kind >= kPresentationCount) {
        return "";
    }
    const char* fallback = kind == A2FO_RESOURCE_PRESENTATION_ICON
        ? kDefaultResourceIcons[resource]
        : kDefaultResourceNames[resource];
    thread_local std::string text;
    return localized_presentation_override(team, resource, kind, &text)
        ? text.c_str() : fallback;
}

void refresh_presentation_cache(void* team, void* race) noexcept {
    if (g_presentation_cache.team == team &&
        g_presentation_cache.race == race &&
        g_presentation_cache.generation == g_presentation_generation) {
        return;
    }
    PresentationCache refreshed{};
    refreshed.team = team;
    refreshed.race = race;
    refreshed.generation = g_presentation_generation;
    for (std::size_t index = 0; index < kResourceCount; ++index) {
        const std::uint32_t resource = kFirstAdditionalResource +
            static_cast<std::uint32_t>(index);
        for (std::size_t kind = 0; kind < kPresentationCount; ++kind) {
            refreshed.added[index][kind] = localize_presentation(
                team, resource, static_cast<std::uint32_t>(kind));
        }
    }
    for (std::size_t resource = 0; resource < kNativeResourceCount;
         ++resource) {
        for (std::size_t kind = 0; kind < kPresentationCount; ++kind) {
            if (kind == A2FO_RESOURCE_PRESENTATION_ICON) {
                refreshed.native[resource][kind] = localize_presentation(
                    team, static_cast<std::uint32_t>(resource),
                    static_cast<std::uint32_t>(kind));
            } else {
                localized_presentation_override(
                    team, static_cast<std::uint32_t>(resource),
                    static_cast<std::uint32_t>(kind),
                    &refreshed.native[resource][kind]);
            }
        }
    }
    g_presentation_cache = std::move(refreshed);
}

const char* cached_added_presentation(std::size_t index,
                                      std::uint32_t kind) noexcept {
    if (index >= kResourceCount || kind >= kPresentationCount) return "";
    return g_presentation_cache.added[index][kind].c_str();
}

void refresh_panel_text_cache(void* team, const Amounts& amounts) noexcept {
    if (g_panel_text_cache.team == team &&
        g_panel_text_cache.generation == g_presentation_generation &&
        g_panel_text_cache.amounts == amounts) {
        return;
    }
    PanelTextCache refreshed{};
    refreshed.team = team;
    refreshed.generation = g_presentation_generation;
    refreshed.amounts = amounts;
    for (std::size_t index = 0; index < kResourceCount; ++index) {
        char text[48]{};
        std::snprintf(text, sizeof(text), "%lld",
                      static_cast<long long>(amounts[index]));
        refreshed.lines[index] = text;
    }
    g_panel_text_cache = std::move(refreshed);
}

using InsertCString = void* (__cdecl*)(void*, const char*);

InsertCString insert_c_string() noexcept {
    void* function = read_at<void*>(at(g_armada, kInsertCStringIatRva), 0,
                                    nullptr);
    return reinterpret_cast<InsertCString>(function);
}

bool append_native_resource_override(void* component, void* string_stream,
                                     std::uint32_t kind) noexcept {
    if (!g_runtime_alive || !component || !string_stream ||
        kind >= kPresentationCount) {
        return false;
    }
    if (!g_presentation_cache.team ||
        g_presentation_cache.generation != g_presentation_generation) {
        void* team = local_team_object();
        if (team) refresh_presentation_cache(team, team_race(team));
    }
    const std::uint32_t resource = read_live<std::uint32_t>(
        component, kResourceComponentResourceOffset, kTotalResourceCount);
    if (resource >= kNativeResourceCount) return false;
    const std::string& text = g_presentation_cache.native[resource][kind];
    if (text.empty()) return false;
    InsertCString insert = insert_c_string();
    if (!insert || !readable_range(
            static_cast<std::uint8_t*>(string_stream) +
                kOstreamSubobjectOffset,
            sizeof(void*))) {
        return false;
    }
    insert(static_cast<std::uint8_t*>(string_stream) +
               kOstreamSubobjectOffset,
           text.c_str());
    return true;
}

void __attribute__((fastcall)) resource_component_tooltip_hook(
    void* component, void*, void* string_stream) noexcept {
    if (!append_native_resource_override(
            component, string_stream,
            A2FO_RESOURCE_PRESENTATION_TOOLTIP)) {
        a2fo_resources_call_thiscall_1(
            g_resource_component_tooltip_hook.gateway, component,
            reinterpret_cast<std::uintptr_t>(string_stream));
    }
}

void __attribute__((fastcall)) resource_component_verbose_tooltip_hook(
    void* component, void*, void* string_stream) noexcept {
    if (!append_native_resource_override(
            component, string_stream,
            A2FO_RESOURCE_PRESENTATION_VERBOSE_TOOLTIP)) {
        a2fo_resources_call_thiscall_1(
            g_resource_component_verbose_tooltip_hook.gateway, component,
            reinterpret_cast<std::uintptr_t>(string_stream));
    }
}

std::uintptr_t native_resource_presentation_lookup(
    void* manager, const char* key, std::uint32_t presentation) noexcept {
    // Resolve the active Race once, then keep this palette-hot route to key
    // comparisons and one cached pointer return. The previous implementation
    // performed Team discovery, locking, and localization at every call site.
    if ((!g_presentation_cache.team ||
         g_presentation_cache.generation != g_presentation_generation) &&
        g_runtime_alive) {
        void* team = local_team_object();
        if (team) refresh_presentation_cache(team, team_race(team));
    }
    if (key && g_presentation_cache.team) {
        for (std::size_t resource = 0;
             resource < kNativeResourceResKeys.size(); ++resource) {
            const char* canonical = kNativeResourceResKeys[resource];
            if (!canonical || std::strcmp(key, canonical) != 0) continue;
            const std::string& replacement =
                g_presentation_cache.native[resource][presentation];
            if (!replacement.empty()) {
                return reinterpret_cast<std::uintptr_t>(
                    replacement.c_str());
            }
            break;
        }
    }
    return a2fo_resources_call_thiscall_1(
        at(g_armada, kLocalizationLookupRva), manager,
        reinterpret_cast<std::uintptr_t>(key));
}

std::uintptr_t __attribute__((fastcall)) native_resource_icon_lookup(
    void* manager, void*, const char* key) noexcept {
    return native_resource_presentation_lookup(
        manager, key, A2FO_RESOURCE_PRESENTATION_ICON);
}

std::uintptr_t __attribute__((fastcall)) native_resource_res_lookup(
    void* manager, void*, const char* key) noexcept {
    return native_resource_presentation_lookup(
        manager, key, A2FO_RESOURCE_PRESENTATION_RES);
}

std::uintptr_t __attribute__((fastcall)) native_officer_icon_lookup(
    void* manager, void*, const char* key) noexcept {
    if ((!g_presentation_cache.team ||
         g_presentation_cache.generation != g_presentation_generation) &&
        g_runtime_alive) {
        void* team = local_team_object();
        if (team) refresh_presentation_cache(team, team_race(team));
    }
    if (g_presentation_cache.team) {
        const std::string& icon =
            g_presentation_cache.native[1]
                                       [A2FO_RESOURCE_PRESENTATION_ICON];
        if (!icon.empty()) {
            return reinterpret_cast<std::uintptr_t>(icon.c_str());
        }
    }
    return a2fo_resources_call_thiscall_1(
        at(g_armada, kLocalizationLookupRva), manager,
        reinterpret_cast<std::uintptr_t>(key));
}

void* current_game_setup() noexcept {
    void* shell = read_at<void*>(at(g_armada, kCurrentSetupShellRva), 0);
    return shell ? reinterpret_cast<void*>(
        a2fo_resources_call_thiscall_0(
            at(g_armada, kGetGameSetupRva), shell)) : nullptr;
}

bool infinite_resources() noexcept {
    void* setup = current_game_setup();
    return setup &&
        (a2fo_resources_call_thiscall_0(
             at(g_armada, kGameSetupInfiniteResourcesRva), setup) & 0xffu) != 0;
}

bool starting_amounts(void* team, Amounts* output) {
    if (!team || !output) return false;
    void* setup = current_game_setup();
    if (!setup) return false;
    void* race = read_at<void*>(team, kTeamRaceOffset, nullptr);
    if (!race) return false;
    const std::uint32_t setting = static_cast<std::uint32_t>(
        a2fo_resources_call_thiscall_0(
            at(g_armada, kGameSetupGetStartingResourcesIndexRva), setup));
    Amounts amounts{};
    {
        LockGuard lock;
        const auto found = g_race_starting_resources.find(race);
        if (found == g_race_starting_resources.end()) return false;
        amounts = setting == 1 ? found->second.lots : found->second.normal;
    }
    *output = amounts;
    return true;
}

Amounts& ensure_team_locked(void* team, const Amounts& initial) {
    const auto inserted = g_team_amounts.emplace(team, initial);
    return inserted.first->second;
}

Amounts team_snapshot(void* team) {
    if (!team || !g_lock_ready) return {};
    {
        LockGuard lock;
        const auto found = g_team_amounts.find(team);
        if (found != g_team_amounts.end()) return found->second;
    }
    Amounts initial{};
    // A Team's Race pointer is assigned after its constructor returns. Do not
    // cache a zero balance during that short window; retry on the next access.
    if (!starting_amounts(team, &initial)) return {};
    LockGuard lock;
    return ensure_team_locked(team, initial);
}

Costs class_costs(void* object_class) {
    if (!object_class || !g_lock_ready) return {};
    LockGuard lock;
    const auto found = g_class_costs.find(object_class);
    return found == g_class_costs.end() ? Costs{} : found->second;
}

bool additional_resource_index(std::uint32_t resource,
                               std::size_t* index) noexcept {
    if (!index || resource < kFirstAdditionalResource ||
        resource >= kTotalResourceCount) {
        return false;
    }
    *index = static_cast<std::size_t>(resource - kFirstAdditionalResource);
    return true;
}

void A2FO_CALL class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    if (!g_runtime_alive || !event ||
        event->struct_size < sizeof(*event) || !event->object_class) {
        return;
    }
    Costs costs{};
    parse_fields(event->odf_fields, event->odf_field_count,
                 kCostCommands, &costs);
    {
        LockGuard lock;
        g_class_costs[event->object_class] = costs;
    }
    if (std::any_of(costs.begin(), costs.end(),
                    [](std::int32_t cost) { return cost != 0; })) {
        char message[192]{};
        std::snprintf(message, sizeof(message),
                      "Additional costs registered: [%ld, %ld, %ld, %ld]",
                      static_cast<long>(costs[0]),
                      static_cast<long>(costs[1]),
                      static_cast<long>(costs[2]),
                      static_cast<long>(costs[3]));
        log_line(message);
    }
}

void A2FO_CALL race_loaded_handler(
    const A2FO_RaceLoadedEvent* event, void*) {
    if (!g_runtime_alive || !event ||
        event->struct_size < sizeof(*event) || !event->race) {
        return;
    }
    RaceStartingResources resources;
    parse_fields(event->odf_fields, event->odf_field_count,
                 kNormalCommands, &resources.normal);
    parse_fields(event->odf_fields, event->odf_field_count,
                 kLotsCommands, &resources.lots);
    ResourcePresentation presentation{};
    parse_presentation_fields(event->odf_fields, event->odf_field_count,
                              &presentation);
    std::string race_name;
    event_field(event->odf_fields, event->odf_field_count, "name",
                &race_name);
    for (std::size_t resource = 0; resource < kTotalResourceCount;
         ++resource) {
        const auto& fields = presentation[resource];
        if (std::all_of(fields.begin(), fields.end(),
                        [](const std::string& value) {
                            return value.empty();
                        })) {
            continue;
        }
        char message[640]{};
        std::snprintf(
            message, sizeof(message),
            "Race presentation registered: race='%s' resource=%s "
            "Res='%s' Tooltip='%s' VerboseTooltip='%s' Icon='%s'",
            race_name.empty() ? "<unnamed>" : race_name.c_str(),
            kDefaultResourceNames[resource], fields[0].c_str(),
            fields[1].c_str(), fields[2].c_str(), fields[3].c_str());
        log_line(message);
    }
    {
        LockGuard lock;
        g_race_starting_resources[event->race] = resources;
        g_race_presentations[event->race] = std::move(presentation);
        ++g_presentation_generation;
    }
    g_presentation_cache = {};
    g_panel_text_cache = {};
    g_panel_tooltip_component = nullptr;
    g_panel_tooltip_team = nullptr;
    g_panel_tooltip_resource = -1;
    const bool configured = std::any_of(
        resources.normal.begin(), resources.normal.end(),
        [](std::int64_t amount) { return amount != 0; }) ||
        std::any_of(resources.lots.begin(), resources.lots.end(),
                    [](std::int64_t amount) { return amount != 0; });
    if (configured) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "Race starting resources registered: normal=[%lld, %lld, %lld, "
            "%lld], lots=[%lld, %lld, %lld, %lld]",
            static_cast<long long>(resources.normal[0]),
            static_cast<long long>(resources.normal[1]),
            static_cast<long long>(resources.normal[2]),
            static_cast<long long>(resources.normal[3]),
            static_cast<long long>(resources.lots[0]),
            static_cast<long long>(resources.lots[1]),
            static_cast<long long>(resources.lots[2]),
            static_cast<long long>(resources.lots[3]));
        log_line(message);
    }
}

void* gui_parameter_db() noexcept {
    return read_live<void*>(at(g_armada, kGuiParameterDbPointerRva), 0,
                            nullptr);
}

bool read_ui_rectangle(void* parameter_db, const char* key,
                       RawRectangle* output) noexcept {
    if (!parameter_db || !key || !output) return false;
    const RawRectangle fallback{};
    const std::uintptr_t found = a2fo_resources_call_thiscall_3(
        at(g_armada, kParameterDbGetRectangleRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(output),
        reinterpret_cast<std::uintptr_t>(&fallback));
    return (found & 0xffu) != 0;
}

void refresh_panel_configuration(void* parameter_db) noexcept {
    PanelConfiguration loaded{};
    loaded.parameter_db = parameter_db;
    loaded.loaded = true;
    for (std::size_t index = 0;
         parameter_db && index < kNativeResourceCount; ++index) {
        char native_key[24]{};
        std::snprintf(native_key, sizeof(native_key), "resource_%lu",
                      static_cast<unsigned long>(index));
        loaded.native_rectangle_found[index] = read_ui_rectangle(
            parameter_db, native_key, &loaded.native_rectangles[index]);
    }
    for (std::size_t index = 0; parameter_db && index < kResourceCount;
         ++index) {
        loaded.rectangle_found[index] = read_ui_rectangle(
            parameter_db, kPanelRectangleKeys[index],
            &loaded.rectangles[index]);
    }
    g_panel_configuration = loaded;

    const std::size_t configured = static_cast<std::size_t>(std::count(
        loaded.rectangle_found.begin(), loaded.rectangle_found.end(), true));
    char message[160]{};
    std::snprintf(
        message, sizeof(message),
        "Additional resource panel: %lu/4 resource_6..resource_9 "
        "rectangles configured%s",
        static_cast<unsigned long>(configured),
        configured == kResourceCount ? "" : "; using second-row defaults");
    log_line(message);
}

bool usable_rectangle(const NativeRectangle& rectangle) noexcept {
    return rectangle.right > rectangle.left &&
        rectangle.bottom > rectangle.top;
}

NativeRectangle resource_panel_rectangle(
    std::size_t index, std::size_t context_index,
    const NativeRectangle& context_rectangle) noexcept {
    if (index >= kResourceCount || !usable_rectangle(context_rectangle) ||
        context_index >= kNativeResourceCount ||
        !g_panel_configuration.native_rectangle_found[context_index]) {
        return {};
    }

    const RawRectangle& raw_context =
        g_panel_configuration.native_rectangles[context_index];
    const RawRectangle& raw_target =
        g_panel_configuration.rectangle_found[index]
            ? g_panel_configuration.rectangles[index]
            : kDefaultPanelRectangles[index];
    if (raw_context.width <= 0 || raw_context.height <= 0 ||
        raw_target.width <= 0 || raw_target.height <= 0) {
        return {};
    }
    const double scale_x = static_cast<double>(
        context_rectangle.right - context_rectangle.left) / raw_context.width;
    const double scale_y = static_cast<double>(
        context_rectangle.bottom - context_rectangle.top) / raw_context.height;
    NativeRectangle result{};
    result.left = context_rectangle.left +
        static_cast<std::int32_t>(std::lround(
            (raw_target.x - raw_context.x) * scale_x));
    result.top = context_rectangle.top +
        static_cast<std::int32_t>(std::lround(
            (raw_target.y - raw_context.y) * scale_y));
    result.right = result.left + static_cast<std::int32_t>(std::lround(
        raw_target.width * scale_x));
    result.bottom = result.top + static_cast<std::int32_t>(std::lround(
        raw_target.height * scale_y));
    return result;
}

struct PanelRenderContext {
    std::size_t index = kNativeResourceCount;
    void* resource_display = nullptr;
    void* text_component = nullptr;
    NativeRectangle rectangle{};
};

PanelRenderContext find_panel_render_context(void* panel) noexcept {
    PanelRenderContext result{};
    for (std::size_t candidate = 0; candidate < kNativeResourceCount;
         ++candidate) {
        if (!g_panel_configuration.native_rectangle_found[candidate]) continue;
        void* resource_display = read_live<void*>(
            panel, kResourcePanelFirstTextOffset + candidate * sizeof(void*),
            nullptr);
        void* text_component = read_live<void*>(
            resource_display, kResourceDisplayTextOffset, nullptr);
        const NativeRectangle rectangle = read_live<NativeRectangle>(
            text_component, kTextComponentLiveRectangleOffset,
            NativeRectangle{});
        if (!resource_display || !text_component ||
            !usable_rectangle(rectangle)) {
            continue;
        }
        result.index = candidate;
        result.resource_display = resource_display;
        result.text_component = text_component;
        result.rectangle = rectangle;
        return result;
    }
    return result;
}

bool draw_panel_text(const char* value, const NativeRectangle& rectangle,
                     void* text_component,
                     const Colour* colour_override = nullptr) noexcept {
    if (!value || !*value || !usable_rectangle(rectangle) ||
        !text_component) {
        return false;
    }
    void* display_interface = read_live<void*>(text_component, 0x04, nullptr);
    if (!display_interface) return false;
    void* display_override = nullptr;
    void* display_slot = read_live<void*>(text_component, 0x28, nullptr);
    if (display_slot) {
        display_override = read_live<void*>(display_slot, 0, nullptr);
    }
    const std::int32_t text_flags = read_live<std::int32_t>(
        text_component, 0x68, 9);
    const std::uint8_t constrain = read_live<std::uint8_t>(
        text_component, 0x6c, 0);
    Colour colour = colour_override
        ? *colour_override
        : read_live<Colour>(text_component, 0x70, Colour{});
    if (!std::isfinite(colour.red) || !std::isfinite(colour.green) ||
        !std::isfinite(colour.blue)) {
        colour = Colour{};
    }
    void* font_state = static_cast<std::uint8_t*>(text_component) + 0x7c;

    a2fo_resources_call_thiscall_7(
        at(g_armada, kDisplayInterfaceDrawTextRva), display_interface,
        reinterpret_cast<std::uintptr_t>(value),
        reinterpret_cast<std::uintptr_t>(&rectangle),
        static_cast<std::uintptr_t>(text_flags),
        reinterpret_cast<std::uintptr_t>(&colour),
        reinterpret_cast<std::uintptr_t>(display_override),
        static_cast<std::uintptr_t>(constrain),
        reinterpret_cast<std::uintptr_t>(font_state));
    return true;
}

NativeRectangle panel_amount_rectangle(
    std::size_t index, const NativeRectangle& rectangle) noexcept {
    if (index >= kPanelIconAdvanceRatios.size() ||
        !usable_rectangle(rectangle)) {
        return {};
    }
    NativeRectangle result = rectangle;
    const std::int32_t height = rectangle.bottom - rectangle.top;
    result.left += static_cast<std::int32_t>(std::lround(
        height * kPanelIconAdvanceRatios[index]));
    return result;
}

bool cursor_inside(const NativeRectangle& rectangle, std::int32_t x,
                   std::int32_t y) noexcept {
    if (!usable_rectangle(rectangle)) return false;
    return x >= rectangle.left && x <= rectangle.right &&
        y >= rectangle.top && y <= rectangle.bottom;
}

void set_component_text(void* component, std::uintptr_t setter_rva,
                        const char* text) noexcept {
    if (!component || !readable_range(component, sizeof(void*)) ||
        !readable_range(at(g_armada, setter_rva), 1)) {
        return;
    }
    a2fo_resources_call_thiscall_1(
        at(g_armada, setter_rva), component,
        reinterpret_cast<std::uintptr_t>(text));
}

void update_added_resource_tooltip(
    void* panel, void* team,
    const std::array<NativeRectangle, kResourceCount>& rectangles) noexcept {
    void* component = read_live<void*>(
        panel, kResourcePanelBackgroundOffset, nullptr);
    std::int32_t hovered = -1;
    const std::int32_t cursor_x = read_live<std::int32_t>(
        at(g_armada, kCursorXRva), 0,
        std::numeric_limits<std::int32_t>::min());
    const std::int32_t cursor_y = read_live<std::int32_t>(
        at(g_armada, kCursorYRva), 0,
        std::numeric_limits<std::int32_t>::min());
    for (std::size_t index = 0; index < rectangles.size(); ++index) {
        if (cursor_inside(rectangles[index], cursor_x, cursor_y)) {
            hovered = static_cast<std::int32_t>(index);
            break;
        }
    }
    if (component == g_panel_tooltip_component &&
        team == g_panel_tooltip_team &&
        hovered == g_panel_tooltip_resource) {
        return;
    }

    g_panel_tooltip_component = component;
    g_panel_tooltip_team = team;
    g_panel_tooltip_resource = hovered;
    if (hovered < 0) {
        set_component_text(component, kStandardComponentSetTooltipTextRva,
                           nullptr);
        set_component_text(
            component, kStandardComponentSetVerboseTooltipTextRva, nullptr);
        return;
    }

    set_component_text(
        component, kStandardComponentSetTooltipTextRva,
        cached_added_presentation(
            static_cast<std::size_t>(hovered),
            A2FO_RESOURCE_PRESENTATION_TOOLTIP));
    set_component_text(
        component, kStandardComponentSetVerboseTooltipTextRva,
        cached_added_presentation(
            static_cast<std::size_t>(hovered),
            A2FO_RESOURCE_PRESENTATION_VERBOSE_TOOLTIP));
}

void __attribute__((fastcall)) resource_panel_render_hook(
    void* panel, void*) noexcept {
    a2fo_resources_call_thiscall_0(
        g_resource_panel_render_hook.gateway, panel);
    if (!g_runtime_alive || !panel) return;

    void* team = local_team_object();
    if (!team) return;
    const Amounts amounts = team_snapshot(team);
    void* race = read_live<void*>(team, kTeamRaceOffset, nullptr);
    refresh_presentation_cache(team, race);
    refresh_panel_text_cache(team, amounts);

    void* parameter_db = gui_parameter_db();
    if (!g_panel_configuration.loaded ||
        g_panel_configuration.parameter_db != parameter_db) {
        refresh_panel_configuration(parameter_db);
    }
    const PanelRenderContext context = find_panel_render_context(panel);
    std::array<NativeRectangle, kResourceCount> rectangles{};
    for (std::size_t index = 0; index < kResourceCount; ++index) {
        const NativeRectangle rectangle = resource_panel_rectangle(
            index, context.index, context.rectangle);
        rectangles[index] = rectangle;
        const Colour neutral_icon_colour{};
        draw_panel_text(
            cached_added_presentation(
                index, A2FO_RESOURCE_PRESENTATION_ICON),
            rectangle, context.text_component, &neutral_icon_colour);
        draw_panel_text(
            g_panel_text_cache.lines[index].c_str(),
            panel_amount_rectangle(index, rectangle),
            context.text_component);
    }
    update_added_resource_tooltip(panel, team, rectangles);
}

bool A2FO_CALL producer_event_handler(
    const A2FO_ProducerEvent* event, void*) {
    if (!g_runtime_alive || !g_production_integration_ready || !event ||
        event->struct_size < sizeof(*event) || !event->producer) {
        return true;
    }
    if (event->kind != A2FO_PRODUCER_EVENT_CANCELLED &&
        event->kind != A2FO_PRODUCER_EVENT_DELETED &&
        event->kind != A2FO_PRODUCER_EVENT_CLEARED) {
        return true;
    }
    if (infinite_resources()) return true;
    void* team = read_at<void*>(event->producer, kProducerTeamOffset);
    if (!team || !event->target_class) return true;
    const Costs costs = class_costs(event->target_class);
    const Amounts initial = team_snapshot(team);
    LockGuard lock;
    Amounts& amounts = ensure_team_locked(team, initial);
    a2fo::resources::credit(amounts, costs);
    return true;
}

std::uintptr_t __attribute__((fastcall)) team_constructor_hook(
    void* team, void*, std::uintptr_t argument) noexcept {
    const std::uintptr_t result = a2fo_resources_call_thiscall_1(
        g_team_constructor_hook.gateway, team, argument);
    if (g_lock_ready && team) {
        LockGuard lock;
        g_team_amounts.erase(team);
    }
    if (g_presentation_cache.team == team) g_presentation_cache = {};
    if (g_panel_text_cache.team == team) g_panel_text_cache = {};
    return result;
}

std::uintptr_t __attribute__((fastcall)) team_destructor_hook(
    void* team, void*) noexcept {
    if (g_lock_ready && team) {
        LockGuard lock;
        g_team_amounts.erase(team);
    }
    if (g_presentation_cache.team == team) g_presentation_cache = {};
    if (g_panel_text_cache.team == team) g_panel_text_cache = {};
    return a2fo_resources_call_thiscall_0(
        g_team_destructor_hook.gateway, team);
}

bool install_runtime_hooks() {
    if (!signature_matches(g_armada, kTeamConstructorRva,
                           kExpectedTeamConstructor.data(),
                           kExpectedTeamConstructor.size()) ||
        !signature_matches(g_armada, kTeamDestructorRva,
                           kExpectedTeamDestructor.data(),
                           kExpectedTeamDestructor.size()) ||
        !signature_matches(g_fleet_ops, kDeductResourcesForBuildRva,
                           kExpectedDeductResources.data(),
                           kExpectedDeductResources.size())) {
        log_line("Supported resource hook signatures were not found");
        return false;
    }
    if (!g_api->install_inline_hook(
            at(g_armada, kTeamConstructorRva),
            reinterpret_cast<void*>(&team_constructor_hook),
            kExpectedTeamConstructor.size(), kExpectedTeamConstructor.data(),
            &g_team_constructor_hook) ||
        !g_api->install_inline_hook(
            at(g_armada, kTeamDestructorRva),
            reinterpret_cast<void*>(&team_destructor_hook),
            kExpectedTeamDestructor.size(), kExpectedTeamDestructor.data(),
            &g_team_destructor_hook) ||
        !g_api->install_inline_hook(
            at(g_fleet_ops, kDeductResourcesForBuildRva),
            reinterpret_cast<void*>(&a2fo_resources_deduct_hook),
            kExpectedDeductResources.size(), kExpectedDeductResources.data(),
            &g_deduct_hook)) {
        log_line("Additional resource hook installation was incomplete");
        return false;
    }
    g_a2fo_resources_deduct_gateway = g_deduct_hook.gateway;
    g_production_integration_ready = true;
    return true;
}

bool install_resource_panel_hook() {
    if (!signature_matches(g_armada, kResourcePanelRenderRva,
                           kExpectedResourcePanelRender.data(),
                           kExpectedResourcePanelRender.size()) ||
        !signature_matches(g_armada, kLocalTeamRva,
                           kExpectedLocalTeam.data(),
                           kExpectedLocalTeam.size()) ||
        !signature_matches(g_armada, kTeamForTeamRva,
                           kExpectedTeamForTeam.data(),
                           kExpectedTeamForTeam.size()) ||
        !signature_matches(g_armada, kParameterDbGetRectangleRva,
                           kExpectedParameterDbGetRectangle.data(),
                           kExpectedParameterDbGetRectangle.size()) ||
        !signature_matches(g_armada, kDisplayInterfaceDrawTextRva,
                           kExpectedDisplayInterfaceDrawText.data(),
                           kExpectedDisplayInterfaceDrawText.size()) ||
        !signature_matches(g_armada, kStandardComponentSetTooltipTextRva,
                           kExpectedSetTooltipText.data(),
                           kExpectedSetTooltipText.size()) ||
        !signature_matches(
            g_armada, kStandardComponentSetVerboseTooltipTextRva,
            kExpectedSetTooltipText.data(), kExpectedSetTooltipText.size())) {
        log_line("Resource-panel signatures were not found; balances remain active");
        return false;
    }
    if (!g_api->install_inline_hook(
            at(g_armada, kResourcePanelRenderRva),
            reinterpret_cast<void*>(&resource_panel_render_hook),
            kExpectedResourcePanelRender.size(),
            kExpectedResourcePanelRender.data(),
            &g_resource_panel_render_hook)) {
        log_line("Resource-panel render hook was not installed");
        return false;
    }
    return true;
}

bool install_native_presentation_hooks() {
    if (!g_api->patch_call ||
        !signature_matches(g_armada, kResourceComponentTooltipRva,
                           kExpectedResourceComponentTooltip.data(),
                           kExpectedResourceComponentTooltip.size()) ||
        !signature_matches(g_armada, kResourceComponentVerboseTooltipRva,
                           kExpectedResourceComponentTooltip.data(),
                           kExpectedResourceComponentTooltip.size()) ||
        !insert_c_string()) {
        log_line("Native resource presentation signatures were not found");
        return false;
    }
    for (const CheckedCallPatch& call_site :
         kNativeResourceResCallSites) {
        if (!signature_matches(g_armada, call_site.rva,
                               call_site.expected.data(),
                               call_site.expected.size())) {
            log_line("Native resource Res call signatures were not found");
            return false;
        }
    }
    if (!signature_matches(g_armada, kNativeOfficerIconCallSite.rva,
                           kNativeOfficerIconCallSite.expected.data(),
                           kNativeOfficerIconCallSite.expected.size())) {
        log_line("Native officer icon signature was not found");
        return false;
    }

    const bool tooltip_installed = g_api->install_inline_hook(
        at(g_armada, kResourceComponentTooltipRva),
        reinterpret_cast<void*>(&resource_component_tooltip_hook),
        kExpectedResourceComponentTooltip.size(),
        kExpectedResourceComponentTooltip.data(),
        &g_resource_component_tooltip_hook);
    const bool verbose_installed = g_api->install_inline_hook(
        at(g_armada, kResourceComponentVerboseTooltipRva),
        reinterpret_cast<void*>(&resource_component_verbose_tooltip_hook),
        kExpectedResourceComponentTooltip.size(),
        kExpectedResourceComponentTooltip.data(),
        &g_resource_component_verbose_tooltip_hook);

    std::size_t patched = 0;
    for (std::size_t index = 0;
         index < kNativeResourceResCallSites.size(); ++index) {
        const CheckedCallPatch& call_site =
            kNativeResourceResCallSites[index];
        void* replacement = index < kNativeResourceResCallSites.size() / 2
            ? reinterpret_cast<void*>(&native_resource_icon_lookup)
            : reinterpret_cast<void*>(&native_resource_res_lookup);
        if (g_api->patch_call(
                at(g_armada, call_site.rva),
                replacement,
                call_site.expected.data(), call_site.expected.size())) {
            ++patched;
        }
    }
    const bool officer_icon_patched = g_api->patch_call(
        at(g_armada, kNativeOfficerIconCallSite.rva),
        reinterpret_cast<void*>(&native_officer_icon_lookup),
        kNativeOfficerIconCallSite.expected.data(),
        kNativeOfficerIconCallSite.expected.size());

    if (!tooltip_installed || !verbose_installed ||
        patched != kNativeResourceResCallSites.size() ||
        !officer_icon_patched) {
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "Native presentation integration partial: short=%s verbose=%s "
            "Res=%lu/%lu officerIcon=%s",
            tooltip_installed ? "yes" : "no",
            verbose_installed ? "yes" : "no",
            static_cast<unsigned long>(patched),
            static_cast<unsigned long>(kNativeResourceResCallSites.size()),
            officer_icon_patched ? "yes" : "no");
        log_line(message);
        return false;
    }
    log_line("Race-specific native resource presentation enabled (cached)");
    return true;
}

}  // namespace

extern "C" bool __cdecl a2fo_resources_can_pay(
    void* producer, void* object_class) noexcept {
    if (!g_runtime_alive || !producer || !object_class) return false;
    if (infinite_resources()) return true;
    const Costs costs = class_costs(object_class);
    if (std::all_of(costs.begin(), costs.end(),
                    [](std::int32_t cost) { return cost == 0; })) {
        return true;
    }
    void* team = read_at<void*>(producer, kProducerTeamOffset);
    if (!team) return false;
    const Amounts amounts = team_snapshot(team);
    return a2fo::resources::can_afford(amounts, costs);
}

extern "C" void __cdecl a2fo_resources_commit_payment(
    void* producer, void* object_class) noexcept {
    if (!g_runtime_alive || !producer || !object_class ||
        infinite_resources()) {
        return;
    }
    void* team = read_at<void*>(producer, kProducerTeamOffset);
    if (!team) return;
    const Costs costs = class_costs(object_class);
    const Amounts initial = team_snapshot(team);
    LockGuard lock;
    Amounts& amounts = ensure_team_locked(team, initial);
    if (a2fo::resources::can_afford(amounts, costs)) {
        a2fo::resources::debit(amounts, costs);
    }
}

extern "C" __declspec(dllexport)
std::int64_t A2FO_CALL A2FOResources_Get(
    void* team, std::uint32_t resource) {
    std::size_t index = 0;
    if (!g_runtime_alive || !additional_resource_index(resource, &index)) {
        return 0;
    }
    return team_snapshot(team)[index];
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FOResources_Set(
    void* team, std::uint32_t resource, std::int64_t amount) {
    std::size_t index = 0;
    if (!g_runtime_alive || !team || amount < 0 ||
        !additional_resource_index(resource, &index)) {
        return false;
    }
    const Amounts initial = team_snapshot(team);
    LockGuard lock;
    ensure_team_locked(team, initial)[index] = amount;
    return true;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FOResources_Add(
    void* team, std::uint32_t resource, std::int64_t amount) {
    std::size_t index = 0;
    if (!g_runtime_alive || !team ||
        !additional_resource_index(resource, &index)) {
        return false;
    }
    const Amounts initial = team_snapshot(team);
    LockGuard lock;
    Amounts& values = ensure_team_locked(team, initial);
    values[index] = std::max<std::int64_t>(
        0, a2fo::resources::saturating_add(values[index], amount));
    return true;
}

extern "C" __declspec(dllexport)
std::int32_t A2FO_CALL A2FOResources_GetCost(
    void* object_class, std::uint32_t resource) {
    std::size_t index = 0;
    if (!g_runtime_alive ||
        !additional_resource_index(resource, &index)) {
        return 0;
    }
    return class_costs(object_class)[index];
}

extern "C" __declspec(dllexport)
const char* A2FO_CALL A2FOResources_GetPresentationText(
    void* team, std::uint32_t resource, std::uint32_t presentation) {
    if (!g_runtime_alive || resource >= kTotalResourceCount ||
        presentation >= kPresentationCount) {
        return "";
    }
    return localize_presentation(team, resource, presentation);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->install_inline_hook ||
        !A2FO_MODULE_API_HAS(api, register_game_object_class_loaded_handler) ||
        !A2FO_MODULE_API_HAS(api, register_race_loaded_handler) ||
        !api->register_game_object_class_loaded_handler ||
        !api->register_race_loaded_handler ||
        !api->register_producer_event_handler ||
        (api->capabilities & A2FO_CAP_GAME_OBJECT_CLASS_LOADED) == 0 ||
        (api->capabilities & A2FO_CAP_RACE_LOADED) == 0 ||
        (api->capabilities & A2FO_CAP_PRODUCER_EVENTS) == 0) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !g_fleet_ops ||
        !GetModuleHandleA("A2FOFeaturePack.dll")) {
        log_line("A2FOFeaturePack.dll is required for shared Producer refunds");
        return false;
    }

    InitializeCriticalSection(&g_lock);
    g_lock_ready = true;
    g_runtime_alive = true;

    std::array<const char*, kResourceCount> cost_fields = kCostCommands;
    std::array<const char*,
               kResourceCount * 2 + kPresentationFieldCount + 1>
        race_fields{};
    for (std::size_t index = 0; index < kResourceCount; ++index) {
        race_fields[index] = kNormalCommands[index];
        race_fields[index + kResourceCount] = kLotsCommands[index];
    }
    for (std::size_t index = 0; index < kPresentationFieldCount; ++index) {
        race_fields[kResourceCount * 2 + index] =
            kPresentationCommands[index];
    }
    race_fields.back() = "name";
    if (!api->register_game_object_class_loaded_handler(
            kModuleName, cost_fields.data(),
            static_cast<std::uint32_t>(cost_fields.size()),
            &class_loaded_handler, nullptr) ||
        !api->register_race_loaded_handler(
            kModuleName, race_fields.data(),
            static_cast<std::uint32_t>(race_fields.size()),
            &race_loaded_handler, nullptr) ||
        !api->register_producer_event_handler(
            kModuleName, &producer_event_handler, nullptr)) {
        g_runtime_alive = false;
        DeleteCriticalSection(&g_lock);
        g_lock_ready = false;
        return false;
    }

    if (!install_runtime_hooks()) {
        // A partially installed process-lifetime hook must keep this module
        // resident. Its wrappers remain safe pass-throughs while runtime state
        // stays available for whichever routes were installed.
        log_line("Four-resource module retained after partial initialization");
        return true;
    }
    install_native_presentation_hooks();
    install_resource_panel_hook();
    log_line("Four independent resources enabled (10 total team resources)");
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Runtime hooks are process-lifetime patches; the module loader does not
    // unload initialized modules during normal play.
    g_runtime_alive = false;
    g_production_integration_ready = false;
}
