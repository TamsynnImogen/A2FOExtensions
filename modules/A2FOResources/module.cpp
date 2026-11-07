/*
 * Four independent team resources layered beside Armada II's six native
 * resource slots. Fleet Operations' historic UI aliases are deliberately not
 * reused: tritanium, supply, credits, and collective connections each retain
 * their own balance and cost.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_module_provider.hpp"
#include "additional_resources.hpp"
#include "api.hpp"
#include "../A2FOSquadrons/api.hpp"
#include "../../rust/stage8_c_resources_ffi.h"
#include "../../rust/stage10_resource_presentation_ffi.h"
#include "../../rust/stage10_resources_runtime_ffi.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <optional>
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
constexpr char kA1CompatModuleName[] = "A1Compat.dll";
constexpr char kA1LegacyResourcePanelExport[] =
    "a2fo_a1_render_legacy_resource_panel";
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
A2FOSquadronsGetAdditionalCostsFn g_squad_costs = nullptr;
std::unordered_map<void*, RaceStartingResources> g_race_starting_resources;
std::unordered_map<void*, ResourcePresentation> g_race_presentations;
std::uint32_t g_presentation_generation = 0;
PanelConfiguration g_panel_configuration{};
void* g_panel_tooltip_component = nullptr;
void* g_panel_tooltip_team = nullptr;
std::int32_t g_panel_tooltip_resource = -1;
using A1LegacyResourcePanelRender = bool (__cdecl*)(void* panel);
A1LegacyResourcePanelRender g_a1_legacy_resource_panel_render = nullptr;
bool g_a1_legacy_resource_panel_bridge_reported = false;

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
    const auto read=[](void* opaque,std::uint32_t index,std::uint32_t kind,
                       A2foStage8ResourceView* out)->bool {
        const auto* source=static_cast<const A2FO_OdfFieldView*>(opaque);
        const auto& view=kind==0?source[index].name:source[index].value;
        *out={reinterpret_cast<const std::uint8_t*>(view.data),view.size};
        return true;
    };
    const auto compare=[](void*,const std::uint8_t* left,
                          const std::uint8_t* right,std::size_t size)->bool {
        return _strnicmp(reinterpret_cast<const char*>(left),
                         reinterpret_cast<const char*>(right),size)==0;
    };
    A2foStage8ResourceView found{};
    if (!a2fo_rs_c_resources_find_field(
            const_cast<A2FO_OdfFieldView*>(fields),count,
            reinterpret_cast<const std::uint8_t*>(name),name_size,
            read,compare,&found)) return false;
    value->assign(found.data?reinterpret_cast<const char*>(found.data):"",
                  found.size);
    return true;
}

template <typename Values>
void parse_fields(const A2FO_OdfFieldView* fields, std::uint32_t count,
                  const std::array<const char*, kResourceCount>& commands,
                  Values* values) {
    if (!values) return;
    static_assert(kResourceCount==4);
    struct Context {const A2FO_OdfFieldView* fields;std::uint32_t count;
        const std::array<const char*, kResourceCount>* commands;Values* values;
        std::exception_ptr error;};
    Context context{fields,count,&commands,values,{}};
    const auto parse=[](void* opaque,std::uint32_t index) noexcept -> std::int32_t {
        auto& c=*static_cast<Context*>(opaque);
        try{std::string text;std::int32_t parsed=0;
            if(event_field(c.fields,c.count,(*c.commands)[index],&text)&&
               a2fo::resources::parse_nonnegative(text,&parsed))
                (*c.values)[index]=parsed;
            return 0;
        }catch(...){c.error=std::current_exception();return -1;}
    };
    a2fo_rs_c_resources_parse4(&context,parse);
    if(context.error)std::rethrow_exception(context.error);
}

void parse_presentation_fields(const A2FO_OdfFieldView* fields,
                               std::uint32_t count,
                               ResourcePresentation* presentation) {
    if (!presentation) return;
    static_assert(kTotalResourceCount==10 && kPresentationCount==4);
    struct Context {const A2FO_OdfFieldView* fields;std::uint32_t count;
        ResourcePresentation* presentation;std::exception_ptr error;};
    Context context{fields,count,presentation,{}};
    const auto parse=[](void* opaque,std::uint32_t field) noexcept -> bool {
        auto& c=*static_cast<Context*>(opaque);
        try{std::string value;
            if(event_field(c.fields,c.count,kPresentationCommands[field],&value))
                (*c.presentation)[field/kPresentationCount][field%kPresentationCount]=
                    std::move(value);
            return true;
        }catch(...){c.error=std::current_exception();return false;}
    };
    a2fo_rs_c_resources_presentation40(&context,parse);
    if(context.error)std::rethrow_exception(context.error);
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
    struct Context { LocalTeam local; TeamForTeam team; } context{
        local_team, team_for_team};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        const auto& c = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return c.local && c.team;
        case 1: return static_cast<std::uint32_t>(c.local());
        case 2: return reinterpret_cast<std::uintptr_t>(
            c.team(static_cast<std::int32_t>(argument)));
        default: return 0;
        }
    };
    // Rust rejects shell-state indices before the native table lookup.
    return reinterpret_cast<void*>(a2fo_rs_stage10_resource_local_team(
        &context, step));
}

std::string presentation_key(void* team,std::uint32_t resource,std::uint32_t kind) {
    struct State {void* team;void* race=nullptr;};
    State state{team};
    const auto step=[](void* opaque,std::uint32_t op,std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<State*>(opaque);
        if(op==0)return c.team && g_lock_ready;
        c.race=team_race(c.team);return c.race!=nullptr;
    };
    if(!a2fo_rs_stage10_resources_key(&state,step,resource,kind))return {};
    LockGuard lock;
    const auto found=g_race_presentations.find(state.race);
    return found==g_race_presentations.end() ? std::string{} : found->second[resource][kind];
}

bool localized_presentation_override(void* team, std::uint32_t resource,
                                     std::uint32_t kind,
                                     std::string* output) noexcept {
    static_assert(kTotalResourceCount == 10 && kPresentationCount == 4);
    struct Context {
        void* team;
        std::uint32_t resource, kind;
        std::string* output;
        std::optional<std::string> key;
        void* manager = nullptr;
        void* lookup = nullptr;
        const char* translated = nullptr;
    } context{team, resource, kind, output, {}};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return c.output != nullptr;
        case 1:
            try {
                c.key.emplace();
                *c.key = presentation_key(c.team, c.resource, c.kind);
                return 1;
            } catch (...) { return 0; }
        case 2: return c.key->empty();
        case 3:
            c.manager = read_at<void*>(
                at(g_armada, kLocalizationManagerPointerRva), 0, nullptr);
            return reinterpret_cast<std::uintptr_t>(c.manager);
        case 4:
            c.lookup = at(g_armada, kLocalizationLookupRva);
            return reinterpret_cast<std::uintptr_t>(c.lookup);
        case 5:
            c.translated = reinterpret_cast<const char*>(
                a2fo_resources_call_thiscall_1(c.lookup, c.manager,
                    reinterpret_cast<std::uintptr_t>(c.key->c_str())));
            return 0;
        case 6: return c.translated != nullptr;
        case 7: return *c.translated != '\0';
        case 8:
            try {
                // Retain the native conditional-string temporary and assign
                // overload, including their allocation/failure behavior.
                c.output->assign(argument ? c.translated : *c.key);
                return 1;
            } catch (...) { c.output->clear(); return 0; }
        case 9: return !c.output->empty();
        default: return 0;
        }
    };
    return a2fo_rs_stage10_resource_override(resource, kind, &context, step) != 0;
}

const char* localize_presentation(void* team, std::uint32_t resource,
                                  std::uint32_t kind) noexcept {
    static_assert(A2FO_RESOURCE_PRESENTATION_ICON == 3);
    struct Context {
        void* team;
        std::uint32_t resource, kind;
        std::string* text = nullptr;
    } context{team, resource, kind};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return reinterpret_cast<std::uintptr_t>("");
        case 1: return reinterpret_cast<std::uintptr_t>(argument
            ? kDefaultResourceIcons[c.resource]
            : kDefaultResourceNames[c.resource]);
        case 2: {
            thread_local std::string text;
            c.text = &text;
            return 0;
        }
        case 3: return localized_presentation_override(
            c.team, c.resource, c.kind, c.text);
        case 4: return reinterpret_cast<std::uintptr_t>(c.text->c_str());
        default: return 0;
        }
    };
    return reinterpret_cast<const char*>(a2fo_rs_stage10_resource_localize(
        resource, kind, &context, step));
}

void refresh_presentation_cache(void* team,void* race) noexcept {
    struct State {void* team;void* race;std::optional<PresentationCache> refreshed;};
    State state{team,race,{}};
    const auto step=[](void* opaque,std::uint32_t op,std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<State*>(opaque);
        switch(op){
        case 0:return a2fo_rs_c_resources_cache_fresh(
            reinterpret_cast<std::uintptr_t>(c.team),
            reinterpret_cast<std::uintptr_t>(g_presentation_cache.team),
            reinterpret_cast<std::uintptr_t>(c.race),
            reinterpret_cast<std::uintptr_t>(g_presentation_cache.race),
            g_presentation_generation,g_presentation_cache.generation);
        case 1:
            c.refreshed.emplace();c.refreshed->team=c.team;c.refreshed->race=c.race;
            c.refreshed->generation=g_presentation_generation;return 1;
        case 2:{
    static_assert(kResourceCount==4 && kPresentationCount==4 &&
                  kNativeResourceCount==6 &&
                  A2FO_RESOURCE_PRESENTATION_ICON==3);
    struct Context {void* team;PresentationCache* refreshed;};
    Context context{c.team,&*c.refreshed};
    const auto fill=[](void* opaque,std::uint32_t phase,
                       std::uint32_t resource,std::uint32_t kind) noexcept {
        auto& c=*static_cast<Context*>(opaque);
        if(phase==0){
            c.refreshed->added[resource][kind]=localize_presentation(
                c.team,kFirstAdditionalResource+resource,kind);
        }else{
            if (phase==2) {
                c.refreshed->native[resource][kind]=localize_presentation(
                    c.team,resource,kind);
            } else {
                localized_presentation_override(
                    c.team,resource,kind,&c.refreshed->native[resource][kind]);
            }
        }
    };
    a2fo_rs_c_resources_fill_presentation(&context,fill);
            return 1;
        }
        case 3:g_presentation_cache=std::move(*c.refreshed);return 1;
        default:return 0;
        }
    };
    a2fo_rs_stage10_resources_cache(&state,step,0);
}

const char* cached_added_presentation(std::size_t index,
                                      std::uint32_t kind) noexcept {
    if (!a2fo_rs_stage10_resources_added_valid(index,kind)) return "";
    return g_presentation_cache.added[index][kind].c_str();
}

void refresh_panel_text_cache(void* team,const Amounts& amounts) noexcept {
    struct State {void* team;const Amounts* amounts;std::optional<PanelTextCache> refreshed;};
    State state{team,&amounts,{}};
    const auto step=[](void* opaque,std::uint32_t op,std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<State*>(opaque);
        switch(op){
        case 0:return a2fo_rs_c_resources_cache_fresh(
            reinterpret_cast<std::uintptr_t>(c.team),
            reinterpret_cast<std::uintptr_t>(g_panel_text_cache.team),0,0,
            g_presentation_generation,g_panel_text_cache.generation);
        case 1:return a2fo_rs_c_resources_equal_amounts(
            g_panel_text_cache.amounts.data(),c.amounts->data());
        case 2:
            c.refreshed.emplace();c.refreshed->team=c.team;
            c.refreshed->generation=g_presentation_generation;
            c.refreshed->amounts=*c.amounts;return 1;
        case 3:{
    struct Context {const Amounts* amounts;PanelTextCache* refreshed;};
    Context context{c.amounts,&*c.refreshed};
    const auto fill=[](void* opaque,std::uint32_t index) noexcept {
        auto& c=*static_cast<Context*>(opaque);
        char text[48]{};
        std::snprintf(text,sizeof(text),"%lld",
                      static_cast<long long>((*c.amounts)[index]));
        c.refreshed->lines[index]=text;
    };
    a2fo_rs_c_resources_draw_panel(&context,fill);
            return 1;
        }
        case 4:g_panel_text_cache=std::move(*c.refreshed);return 1;
        default:return 0;
        }
    };
    a2fo_rs_stage10_resources_cache(&state,step,1);
}

using InsertCString = void* (__cdecl*)(void*, const char*);

InsertCString insert_c_string() noexcept {
    void* function = read_at<void*>(at(g_armada, kInsertCStringIatRva), 0,
                                    nullptr);
    return reinterpret_cast<InsertCString>(function);
}

bool append_native_resource_override(void* component,void* string_stream,std::uint32_t kind) noexcept {
    struct State {void* component;void* stream;std::uint32_t kind;
        void* team=nullptr;const std::string* text=nullptr;InsertCString insert=nullptr;};
    State state{component,string_stream,kind};
    const auto step=[](void* opaque,std::uint32_t op,std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& c=*static_cast<State*>(opaque);
        switch(op){
        case 0:return g_runtime_alive && c.component && c.stream && c.kind<kPresentationCount;
        case 1:return a2fo_rs_c_resources_cache_refresh(
            reinterpret_cast<std::uintptr_t>(g_presentation_cache.team),
            g_presentation_cache.generation,g_presentation_generation,true);
        case 2:c.team=local_team_object();return c.team!=nullptr;
        case 3:refresh_presentation_cache(c.team,team_race(c.team));return 1;
        case 4:return read_live<std::uint32_t>(c.component,kResourceComponentResourceOffset,kTotalResourceCount);
        case 5:c.text=&g_presentation_cache.native[argument][c.kind];return 1;
        case 6:return c.text->empty();
        case 7:
            c.insert=insert_c_string();
            return c.insert && readable_range(static_cast<std::uint8_t*>(c.stream)+kOstreamSubobjectOffset,sizeof(void*));
        case 8:c.insert(static_cast<std::uint8_t*>(c.stream)+kOstreamSubobjectOffset,c.text->c_str());return 1;
        default:return 0;
        }
    };
    return a2fo_rs_stage10_resources_lookup(&state,step,2)!=0;
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

std::uintptr_t native_resource_presentation_lookup(void* manager,const char* key,std::uint32_t presentation) noexcept {
    struct State {void* manager;const char* key;std::uint32_t presentation;
        void* team=nullptr;const std::string* replacement=nullptr;std::uintptr_t result=0;};
    State state{manager,key,presentation};
    const auto step=[](void* opaque,std::uint32_t op,std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& c=*static_cast<State*>(opaque);
        switch(op){
        case 0:return a2fo_rs_c_resources_cache_refresh(
            reinterpret_cast<std::uintptr_t>(g_presentation_cache.team),
            g_presentation_cache.generation,g_presentation_generation,g_runtime_alive);
        case 1:c.team=local_team_object();return c.team!=nullptr;
        case 2:refresh_presentation_cache(c.team,team_race(c.team));return 1;
        case 3:return c.key && g_presentation_cache.team;
        case 4:{
            static_assert(kNativeResourceResKeys.size()==6);
            const auto check=[](void* opaque,std::uint32_t resource) noexcept -> bool {
                const char* canonical=kNativeResourceResKeys[resource];
                return canonical && std::strcmp(static_cast<State*>(opaque)->key,canonical)==0;
            };
            return static_cast<std::uintptr_t>(a2fo_rs_c_resources_find_native_key(&c,check)+1);
        }
        case 5:c.replacement=&g_presentation_cache.native[argument][c.presentation];return !c.replacement->empty();
        case 6:c.result=reinterpret_cast<std::uintptr_t>(c.replacement->c_str());return 1;
        case 7:c.result=a2fo_resources_call_thiscall_1(at(g_armada,kLocalizationLookupRva),c.manager,
            reinterpret_cast<std::uintptr_t>(c.key));return 1;
        default:return 0;
        }
    };
    a2fo_rs_stage10_resources_lookup(&state,step,0);
    return state.result;
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

std::uintptr_t __attribute__((fastcall)) native_officer_icon_lookup(void* manager,void*,const char* key) noexcept {
    struct State {void* manager;const char* key;void* team=nullptr;
        const std::string* icon=nullptr;std::uintptr_t result=0;};
    State state{manager,key};
    const auto step=[](void* opaque,std::uint32_t op,std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<State*>(opaque);
        switch(op){
        case 0:return a2fo_rs_c_resources_cache_refresh(
            reinterpret_cast<std::uintptr_t>(g_presentation_cache.team),
            g_presentation_cache.generation,g_presentation_generation,g_runtime_alive);
        case 1:c.team=local_team_object();return c.team!=nullptr;
        case 2:refresh_presentation_cache(c.team,team_race(c.team));return 1;
        case 3:return g_presentation_cache.team!=nullptr;
        case 5:c.icon=&g_presentation_cache.native[1][A2FO_RESOURCE_PRESENTATION_ICON];return !c.icon->empty();
        case 6:c.result=reinterpret_cast<std::uintptr_t>(c.icon->c_str());return 1;
        case 7:c.result=a2fo_resources_call_thiscall_1(at(g_armada,kLocalizationLookupRva),c.manager,
            reinterpret_cast<std::uintptr_t>(c.key));return 1;
        default:return 0;
        }
    };
    a2fo_rs_stage10_resources_lookup(&state,step,1);
    return state.result;
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
    struct Context {void* team;void* setup=nullptr;void* race=nullptr;
                    std::exception_ptr error{};};
    Context context{team};
    const auto step=[](void* opaque,std::uint32_t operation,
                       A2foStage8ResourceStart* values) noexcept -> bool {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(operation){
            case 0:
                c.setup=current_game_setup();
                return c.setup!=nullptr;
            case 1:
                c.race=read_at<void*>(c.team,kTeamRaceOffset,nullptr);
                return c.race!=nullptr;
            default:{
                values->setting=static_cast<std::uint32_t>(
                    a2fo_resources_call_thiscall_0(
                        at(g_armada,kGameSetupGetStartingResourcesIndexRva),
                        c.setup));
                LockGuard lock;
                const auto found=g_race_starting_resources.find(c.race);
                if(found==g_race_starting_resources.end())return false;
                std::copy(found->second.normal.begin(),found->second.normal.end(),
                          values->normal);
                std::copy(found->second.lots.begin(),found->second.lots.end(),
                          values->lots);
                return true;
            }
            }
        }catch(...){c.error=std::current_exception();return false;}
    };
    const bool found=a2fo_rs_c_resources_starting_amounts(
        &context,step,output->data());
    if(context.error)std::rethrow_exception(context.error);
    return found;
}

Amounts& ensure_team_locked(void* team, const Amounts& initial) {
    const auto inserted = g_team_amounts.emplace(team, initial);
    return inserted.first->second;
}

Amounts team_snapshot(void* team) {
    if (!team || !g_lock_ready) return {};
    struct Context {void* team;Amounts output{};Amounts initial{};
                    std::exception_ptr error{};};
    Context context{team};
    const auto step=[](void* opaque,std::uint32_t operation) noexcept -> std::int32_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(operation){
            case 0:{
                LockGuard lock;
                const auto found=g_team_amounts.find(c.team);
                if(found==g_team_amounts.end())return 0;
                c.output=found->second;
                return 1;
            }
            case 1:
                // Race may arrive after Team construction; do not cache zero.
                return starting_amounts(c.team,&c.initial)?1:0;
            default:{
                LockGuard lock;
                c.output=ensure_team_locked(c.team,c.initial);
                return 1;
            }
            }
        }catch(...){c.error=std::current_exception();return -1;}
    };
    a2fo_rs_c_resources_snapshot(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return context.output;
}

Costs class_costs(void* object_class) {
    if (!object_class || !g_lock_ready) return {};
    struct Context {void* object_class;Costs result{};
                    std::exception_ptr error{};};
    Context context{object_class};
    const auto step=[](void* opaque,std::uint32_t operation) noexcept -> std::int32_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(operation){
            case 0:{
                // Squadrons may initialize later; resolve before taking lock.
                const auto module=a2fo::startup_module_provider(
                    "A2FOSquadrons.dll",{"A2FOSquadrons_GetAdditionalCosts"});
                const auto exported=module
                    ?GetProcAddress(module,"A2FOSquadrons_GetAdditionalCosts")
                    :nullptr;
                static_assert(sizeof(exported)==sizeof(g_squad_costs));
                std::memcpy(&g_squad_costs,&exported,sizeof(g_squad_costs));
                return 0;
            }
            case 1:{
                Costs aggregate{};
                if(g_squad_costs&&g_squad_costs(c.object_class,
                    aggregate.data(),aggregate.size())){
                    c.result=aggregate;
                    return 1;
                }
                return 0;
            }
            default:{
                LockGuard lock;
                const auto found=g_class_costs.find(c.object_class);
                c.result=found==g_class_costs.end()?Costs{}:found->second;
                return 1;
            }
            }
        }catch(...){c.error=std::current_exception();return -1;}
    };
    a2fo_rs_c_resources_class_costs(&context,!g_squad_costs,step);
    if(context.error)std::rethrow_exception(context.error);
    return context.result;
}

// Retained as a native bridge for the full-source regression fixture.
[[maybe_unused]] bool additional_resource_index(std::uint32_t resource,
                               std::size_t* index) noexcept {
    if (!index) return false;
    static_assert(kFirstAdditionalResource == 6 && kTotalResourceCount == 10);
    const std::int32_t mapped = a2fo_rs_c_resources_index(resource);
    if (mapped < 0) return false;
    *index = static_cast<std::size_t>(mapped);
    return true;
}

void A2FO_CALL class_loaded_handler(const A2FO_GameObjectClassLoadedEvent* event,void*) {
    struct State {const A2FO_GameObjectClassLoadedEvent* event;Costs costs{};std::exception_ptr error{};};
    State state{event};
    const auto step=[](void* opaque,std::uint32_t op,std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<State*>(opaque);
        try{
            switch(op){
            case 0:return g_runtime_alive && c.event && c.event->struct_size>=sizeof(*c.event) && c.event->object_class;
            case 1:parse_fields(c.event->odf_fields,c.event->odf_field_count,kCostCommands,&c.costs);return 1;
            case 2:{LockGuard lock;g_class_costs[c.event->object_class]=c.costs;return 1;}
            case 3:return a2fo_rs_c_resources_nonzero_costs(c.costs.data());
            case 4:{
        char message[192]{};
        std::snprintf(message, sizeof(message),
                      "Additional costs registered: [%ld, %ld, %ld, %ld]",
                      static_cast<long>(c.costs[0]),
                      static_cast<long>(c.costs[1]),
                      static_cast<long>(c.costs[2]),
                      static_cast<long>(c.costs[3]));
        log_line(message);                return 1;
            }
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    a2fo_rs_stage10_resources_event(&state,step,0);
    if(state.error)std::rethrow_exception(state.error);
}

void A2FO_CALL race_loaded_handler(const A2FO_RaceLoadedEvent* event,void*) {
    struct State {const A2FO_RaceLoadedEvent* event;RaceStartingResources resources{};
        std::optional<ResourcePresentation> presentation;std::optional<std::string> race_name;
        std::exception_ptr error{};};
    State state{event,{}, {}, {}, {}};
    const auto step=[](void* opaque,std::uint32_t op,std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<State*>(opaque);
        try{
            switch(op){
            case 0:return g_runtime_alive && c.event && c.event->struct_size>=sizeof(*c.event) && c.event->race;
            case 1:parse_fields(c.event->odf_fields,c.event->odf_field_count,kNormalCommands,&c.resources.normal);return 1;
            case 2:parse_fields(c.event->odf_fields,c.event->odf_field_count,kLotsCommands,&c.resources.lots);return 1;
            case 3:c.presentation.emplace();return 1;
            case 4:parse_presentation_fields(c.event->odf_fields,c.event->odf_field_count,&*c.presentation);return 1;
            case 5:c.race_name.emplace();event_field(c.event->odf_fields,c.event->odf_field_count,"name",&*c.race_name);return 1;
            case 6:{
    static_assert(kTotalResourceCount==10 && kPresentationCount==4);
    struct LogContext {const ResourcePresentation* presentation;
                       const std::string* race_name;
                       std::exception_ptr error{};};
    LogContext context{&*c.presentation,&*c.race_name};
    const auto empty=[](void* opaque,std::uint32_t resource,
                        std::uint32_t kind) noexcept -> std::int32_t {
        const auto& c=*static_cast<LogContext*>(opaque);
        return (*c.presentation)[resource][kind].empty()?0:1;
    };
    const auto log=[](void* opaque,std::uint32_t resource) noexcept -> bool {
        auto& c=*static_cast<LogContext*>(opaque);
        try{
            const auto& fields=(*c.presentation)[resource];
            char message[640]{};
            std::snprintf(message,sizeof(message),
                "Race presentation registered: race='%s' resource=%s "
                "Res='%s' Tooltip='%s' VerboseTooltip='%s' Icon='%s'",
                c.race_name->empty()?"<unnamed>":c.race_name->c_str(),
                kDefaultResourceNames[resource],fields[0].c_str(),
                fields[1].c_str(),fields[2].c_str(),fields[3].c_str());
            log_line(message);
            return true;
        }catch(...){c.error=std::current_exception();return false;}
    };
    a2fo_rs_c_resources_log_presentations(&context,empty,log);
    if(context.error)std::rethrow_exception(context.error);
                return 1;
            }
            case 7:{
                LockGuard lock;
                g_race_starting_resources[c.event->race]=c.resources;
                g_race_presentations[c.event->race]=std::move(*c.presentation);
                ++g_presentation_generation;return 1;
            }
            case 8:g_presentation_cache={};return 1;
            case 9:g_panel_text_cache={};g_panel_tooltip_component=nullptr;
                g_panel_tooltip_team=nullptr;g_panel_tooltip_resource=-1;return 1;
            case 10:return a2fo_rs_c_resources_nonzero4(c.resources.normal.data());
            case 11:return a2fo_rs_c_resources_nonzero4(c.resources.lots.data());
            case 12:{
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "Race starting resources registered: normal=[%lld, %lld, %lld, "
            "%lld], lots=[%lld, %lld, %lld, %lld]",
            static_cast<long long>(c.resources.normal[0]),
            static_cast<long long>(c.resources.normal[1]),
            static_cast<long long>(c.resources.normal[2]),
            static_cast<long long>(c.resources.normal[3]),
            static_cast<long long>(c.resources.lots[0]),
            static_cast<long long>(c.resources.lots[1]),
            static_cast<long long>(c.resources.lots[2]),
            static_cast<long long>(c.resources.lots[3]));
        log_line(message);                return 1;
            }
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    a2fo_rs_stage10_resources_event(&state,step,1);
    if(state.error)std::rethrow_exception(state.error);
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
    loaded.parameter_db=parameter_db;loaded.loaded=true;
    struct State {void* parameter_db;PanelConfiguration* loaded;std::uint32_t configured=0;};
    State state{parameter_db,&loaded};
    const auto step=[](void* opaque,std::uint32_t op,std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<State*>(opaque);
        switch(op){
        case 0:{
    struct Context {void* parameter_db;PanelConfiguration* loaded;};
    Context context{c.parameter_db,c.loaded};
    const auto read=[](void* opaque,std::uint32_t kind,
                       std::uint32_t index) noexcept -> bool {
        auto& c=*static_cast<Context*>(opaque);
        if(kind==0){
            char native_key[24]{};
            std::snprintf(native_key,sizeof(native_key),"resource_%lu",
                          static_cast<unsigned long>(index));
            c.loaded->native_rectangle_found[index]=read_ui_rectangle(
                c.parameter_db,native_key,&c.loaded->native_rectangles[index]);
            return c.loaded->native_rectangle_found[index];
        }
        c.loaded->rectangle_found[index]=read_ui_rectangle(
            c.parameter_db,kPanelRectangleKeys[index],&c.loaded->rectangles[index]);
        return c.loaded->rectangle_found[index];
    };
            c.configured=a2fo_rs_c_resources_load_rectangles(
                &context,c.parameter_db!=nullptr,read,c.loaded->rectangle_found.data());
            return 1;
        }
        case 1:g_panel_configuration=*c.loaded;return 1;
        case 2:{
    char message[160]{};
    std::snprintf(
        message, sizeof(message),
        "Additional resource panel: %lu/4 resource_6..resource_9 "
        "rectangles configured%s",
        static_cast<unsigned long>(c.configured),
        c.configured == kResourceCount ? "" : "; using second-row defaults");
    log_line(message);
            return 1;
        }
        default:return 0;
        }
    };
    a2fo_rs_stage10_resources_cache(&state,step,2);
}

bool usable_rectangle(const NativeRectangle& rectangle) noexcept {
    return a2fo_rs_c_resources_usable_rectangle(
        {rectangle.left,rectangle.top,rectangle.right,rectangle.bottom});
}

// This arithmetic remains the production GNU++17/x87 plus CRT lround kernel.
// Rust selects the rectangles and admits dimensions before this native step.
NativeRectangle project_resource_rectangle_native_x87(
    const NativeRectangle& context_rectangle,
    const RawRectangle& raw_context,const RawRectangle& raw_target) noexcept {
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

NativeRectangle resource_panel_rectangle(
    std::size_t index, std::size_t context_index,
    const NativeRectangle& context_rectangle) noexcept {
    const auto read=[](void*,std::uint32_t index,std::uint32_t context_index,
                       A2foStage8ResourceRawRectangle* raw_context,
                       A2foStage8ResourceRawRectangle* raw_target) noexcept {
        const auto& native=g_panel_configuration.native_rectangles[context_index];
        const auto& target=g_panel_configuration.rectangle_found[index]
            ? g_panel_configuration.rectangles[index]
            : kDefaultPanelRectangles[index];
        *raw_context={native.x,native.y,native.width,native.height};
        *raw_target={target.x,target.y,target.width,target.height};
    };
    A2foStage8ResourceRawRectangle raw_context{},raw_target{};
    if(!a2fo_rs_c_resources_panel_plan(nullptr,
        static_cast<std::uint32_t>(index),
        static_cast<std::uint32_t>(context_index),
        {context_rectangle.left,context_rectangle.top,
         context_rectangle.right,context_rectangle.bottom},
        g_panel_configuration.native_rectangle_found.data(),read,
        &raw_context,&raw_target)) return {};
    return project_resource_rectangle_native_x87(context_rectangle,
        {raw_context.x,raw_context.y,raw_context.width,raw_context.height},
        {raw_target.x,raw_target.y,raw_target.width,raw_target.height});
}

struct PanelRenderContext {
    std::size_t index = kNativeResourceCount;
    void* resource_display = nullptr;
    void* text_component = nullptr;
    NativeRectangle rectangle{};
};

PanelRenderContext find_panel_render_context(void* panel) noexcept {
    PanelRenderContext result{};
    const auto read=[](void* opaque,std::uint32_t candidate,
                       A2foStage8ResourceContext* out) noexcept -> bool {
        void* panel=opaque;
        void* resource_display = read_live<void*>(
            panel, kResourcePanelFirstTextOffset + candidate * sizeof(void*),
            nullptr);
        void* text_component = read_live<void*>(
            resource_display, kResourceDisplayTextOffset, nullptr);
        const NativeRectangle rectangle = read_live<NativeRectangle>(
            text_component, kTextComponentLiveRectangleOffset,
            NativeRectangle{});
        *out={reinterpret_cast<std::uintptr_t>(resource_display),
              reinterpret_cast<std::uintptr_t>(text_component),
              {rectangle.left,rectangle.top,rectangle.right,rectangle.bottom}};
        return true;
    };
    std::uint32_t index=0;
    A2foStage8ResourceContext selected{};
    if(a2fo_rs_c_resources_find_context(panel,
        g_panel_configuration.native_rectangle_found.data(),read,
        &index,&selected)){
        result.index=index;
        result.resource_display=reinterpret_cast<void*>(selected.resource_display);
        result.text_component=reinterpret_cast<void*>(selected.text_component);
        result.rectangle={selected.rectangle.left,selected.rectangle.top,
                          selected.rectangle.right,selected.rectangle.bottom};
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
    if (!a2fo_rs_c_resources_finite_colour(
        colour.red,colour.green,colour.blue)) {
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
    static_assert(kPanelIconAdvanceRatios.size()==4);
    if (!a2fo_rs_c_resources_amount_plan(
        static_cast<std::uint32_t>(index),
        {rectangle.left,rectangle.top,rectangle.right,rectangle.bottom})) {
        return {};
    }
    NativeRectangle result = rectangle;
    const std::int32_t height = rectangle.bottom - rectangle.top;
    result.left += static_cast<std::int32_t>(std::lround(
        height * kPanelIconAdvanceRatios[index]));
    return result;
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
    void* panel,void* team,
    const std::array<NativeRectangle,kResourceCount>& rectangles) noexcept {

    void* component = read_live<void*>(
        panel, kResourcePanelBackgroundOffset, nullptr);
    std::int32_t hovered = -1;
    const std::int32_t cursor_x = read_live<std::int32_t>(
        at(g_armada, kCursorXRva), 0,
        std::numeric_limits<std::int32_t>::min());
    const std::int32_t cursor_y = read_live<std::int32_t>(
        at(g_armada, kCursorYRva), 0,
        std::numeric_limits<std::int32_t>::min());
    std::array<A2foStage8ResourceRectangle,kResourceCount> positions{};
    for(std::size_t index=0;index<kResourceCount;++index){
        const auto& rectangle=rectangles[index];
        positions[index]={rectangle.left,rectangle.top,rectangle.right,
                          rectangle.bottom};
    }
    hovered=a2fo_rs_c_resources_hover(positions.data(),cursor_x,cursor_y);
    struct State {void* component;void* team;std::int32_t hovered;};
    State state{component,team,hovered};
    const auto step=[](void* opaque,std::uint32_t op,std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<State*>(opaque);
        switch(op){
        case 0:return a2fo_rs_c_resources_tooltip_fresh(
            reinterpret_cast<std::uintptr_t>(c.component),
            reinterpret_cast<std::uintptr_t>(g_panel_tooltip_component),
            reinterpret_cast<std::uintptr_t>(c.team),
            reinterpret_cast<std::uintptr_t>(g_panel_tooltip_team),c.hovered,
            g_panel_tooltip_resource);
        case 1:g_panel_tooltip_component=c.component;g_panel_tooltip_team=c.team;
            g_panel_tooltip_resource=c.hovered;return 1;
        case 2:return c.hovered<0;
        case 3:set_component_text(c.component,kStandardComponentSetTooltipTextRva,nullptr);return 1;
        case 4:set_component_text(c.component,kStandardComponentSetVerboseTooltipTextRva,nullptr);return 1;
        case 5:set_component_text(c.component,kStandardComponentSetTooltipTextRva,
            cached_added_presentation(static_cast<std::size_t>(c.hovered),
                                      A2FO_RESOURCE_PRESENTATION_TOOLTIP));return 1;
        case 6:set_component_text(c.component,kStandardComponentSetVerboseTooltipTextRva,
            cached_added_presentation(static_cast<std::size_t>(c.hovered),
                                      A2FO_RESOURCE_PRESENTATION_VERBOSE_TOOLTIP));return 1;
        default:return 0;
        }
    };
    a2fo_rs_stage10_resources_tooltip(&state,step);
}

void resolve_a1_legacy_resource_panel_bridge() noexcept {
    if (g_a1_legacy_resource_panel_render) return;
    HMODULE a1_compat = a2fo::startup_module_provider(
        kA1CompatModuleName, {kA1LegacyResourcePanelExport});
    FARPROC exported = a1_compat
        ? GetProcAddress(a1_compat, kA1LegacyResourcePanelExport)
        : nullptr;
    static_assert(
        sizeof(exported) == sizeof(g_a1_legacy_resource_panel_render),
        "unexpected function-pointer size");
    std::memcpy(
        &g_a1_legacy_resource_panel_render, &exported,
        sizeof(g_a1_legacy_resource_panel_render));
    if (g_a1_legacy_resource_panel_render &&
        !g_a1_legacy_resource_panel_bridge_reported) {
        g_a1_legacy_resource_panel_bridge_reported = true;
        log_line("A1Compat legacy resource-strip renderer linked");
    }
}

void __attribute__((fastcall)) resource_panel_render_hook(void* panel,void*) noexcept {
    struct State {void* panel;bool legacy=false;void* team=nullptr;Amounts amounts{};
        void* race=nullptr;void* parameter_db=nullptr;PanelRenderContext render{};
        std::array<NativeRectangle,kResourceCount> rectangles{};};
    State state{panel};
    const auto step=[](void* opaque,std::uint32_t op,std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<State*>(opaque);
        switch(op){
        case 0:resolve_a1_legacy_resource_panel_bridge();return 1;
        case 1:c.legacy=g_a1_legacy_resource_panel_render &&
            g_a1_legacy_resource_panel_render(c.panel);return 1;
        case 2:a2fo_resources_call_thiscall_0(g_resource_panel_render_hook.gateway,c.panel);return 1;
        case 3:return g_runtime_alive && c.panel && !c.legacy;
        case 4:c.team=local_team_object();return c.team!=nullptr;
        case 5:c.amounts=team_snapshot(c.team);return 1;
        case 6:c.race=read_live<void*>(c.team,kTeamRaceOffset,nullptr);return 1;
        case 7:refresh_presentation_cache(c.team,c.race);return 1;
        case 8:refresh_panel_text_cache(c.team,c.amounts);return 1;
        case 9:c.parameter_db=gui_parameter_db();return 1;
        case 10:return a2fo_rs_c_resources_panel_config_stale(
            g_panel_configuration.loaded,
            reinterpret_cast<std::uintptr_t>(g_panel_configuration.parameter_db),
            reinterpret_cast<std::uintptr_t>(c.parameter_db));
        case 11:refresh_panel_configuration(c.parameter_db);return 1;
        case 12:c.render=find_panel_render_context(c.panel);return 1;
        case 13:{
    struct DrawContext {const PanelRenderContext* render;
                        std::array<NativeRectangle,kResourceCount>* rectangles;};
    DrawContext draw_context{&c.render,&c.rectangles};
    const auto draw=[](void* opaque,std::uint32_t index) noexcept {
        auto& c=*static_cast<DrawContext*>(opaque);
        const NativeRectangle rectangle=resource_panel_rectangle(
            index,c.render->index,c.render->rectangle);
        (*c.rectangles)[index]=rectangle;
        const Colour neutral_icon_colour{};
        draw_panel_text(cached_added_presentation(
            index,A2FO_RESOURCE_PRESENTATION_ICON),rectangle,
            c.render->text_component,&neutral_icon_colour);
        draw_panel_text(g_panel_text_cache.lines[index].c_str(),
            panel_amount_rectangle(index,rectangle),c.render->text_component);
    };
    a2fo_rs_c_resources_draw_panel(&draw_context,draw);
            return 1;
        }
        case 14:update_added_resource_tooltip(c.panel,c.team,c.rectangles);return 1;
        default:return 0;
        }
    };
    a2fo_rs_stage10_resources_panel(&state,step);
}

struct ResourceBalanceContext {Amounts* amounts;const Costs* costs;};
std::uintptr_t resource_balance_step(void* opaque,std::uint32_t op,
                                    std::uintptr_t) noexcept {
    auto& c=*static_cast<ResourceBalanceContext*>(opaque);
    switch(op){
    case 0:return a2fo::resources::can_afford(*c.amounts,*c.costs);
    case 1:a2fo::resources::debit(*c.amounts,*c.costs);return 1;
    case 2:a2fo::resources::credit(*c.amounts,*c.costs);return 1;
    default:return 0;
    }
}
struct ResourceTransactionContext {
    void* producer;void* object_class;const A2FO_ProducerEvent* event;
    std::uint32_t mode;void* team=nullptr;Costs costs{};Amounts initial{};
    std::exception_ptr error{};
};
// These primitives keep the original noexcept payment exception boundary.
std::uintptr_t resource_payment_step(void* opaque,std::uint32_t op,
                                    std::uintptr_t argument) noexcept {
    auto& c=*static_cast<ResourceTransactionContext*>(opaque);
    switch(op){
    case 0:
        if(c.mode==2) return g_runtime_alive && g_production_integration_ready &&
            c.event && c.event->struct_size>=sizeof(*c.event) && c.event->producer;
        return g_runtime_alive && c.producer && c.object_class;
    case 1:return infinite_resources();
    case 2:c.costs=class_costs(c.mode==2 ? c.event->target_class : c.object_class);return 1;
    case 3:return a2fo_rs_c_resources_zero_costs(c.costs.data());
    case 4:c.team=read_at<void*>(c.mode==2 ? c.event->producer : c.producer,
                                kProducerTeamOffset);return c.team!=nullptr;
    case 5:c.initial=team_snapshot(c.team);return 1;
    case 6:return a2fo::resources::can_afford(c.initial,c.costs);
    case 7:{
        LockGuard lock;
        Amounts& amounts=ensure_team_locked(c.team,c.initial);
        ResourceBalanceContext balance{&amounts,&c.costs};
        return a2fo_rs_stage10_resources_locked(&balance,resource_balance_step,
                                                c.mode==2 ? 1 : 0);
    }
    case 8:*reinterpret_cast<std::uint32_t*>(argument)=static_cast<std::uint32_t>(c.event->kind);return 0;
    case 9:return c.event->target_class!=nullptr;
    default:return 0;
    }
}
// Refund dispatch is allowed to propagate: unwind the native lock first,
// retain the exception while Rust stops, then rethrow from the native handler.
std::uintptr_t resource_refund_operation(void* opaque,std::uint32_t op,std::uintptr_t argument) {
    auto& c=*static_cast<ResourceTransactionContext*>(opaque);
    switch(op){
    case 0:
        if(c.mode==2) return g_runtime_alive && g_production_integration_ready &&
            c.event && c.event->struct_size>=sizeof(*c.event) && c.event->producer;
        return g_runtime_alive && c.producer && c.object_class;
    case 1:return infinite_resources();
    case 2:c.costs=class_costs(c.mode==2 ? c.event->target_class : c.object_class);return 1;
    case 3:return a2fo_rs_c_resources_zero_costs(c.costs.data());
    case 4:c.team=read_at<void*>(c.mode==2 ? c.event->producer : c.producer,
                                kProducerTeamOffset);return c.team!=nullptr;
    case 5:c.initial=team_snapshot(c.team);return 1;
    case 6:return a2fo::resources::can_afford(c.initial,c.costs);
    case 7:{
        LockGuard lock;
        Amounts& amounts=ensure_team_locked(c.team,c.initial);
        ResourceBalanceContext balance{&amounts,&c.costs};
        return a2fo_rs_stage10_resources_locked(&balance,resource_balance_step,
                                                c.mode==2 ? 1 : 0);
    }
    case 8:*reinterpret_cast<std::uint32_t*>(argument)=static_cast<std::uint32_t>(c.event->kind);return 0;
    case 9:return c.event->target_class!=nullptr;
    default:return 0;
    }
}
std::uintptr_t resource_refund_step(void* opaque,std::uint32_t op,
                                   std::uintptr_t argument) noexcept {
    try{return resource_refund_operation(opaque,op,argument);}
    catch(...){static_cast<ResourceTransactionContext*>(opaque)->error=
        std::current_exception();return UINTPTR_MAX;}
}
bool A2FO_CALL producer_event_handler(
    const A2FO_ProducerEvent* event,void*) {
    static_assert(A2FO_PRODUCER_EVENT_CANCELLED==5 &&
                  A2FO_PRODUCER_EVENT_DELETED==6 && A2FO_PRODUCER_EVENT_CLEARED==7);
    ResourceTransactionContext context{nullptr,nullptr,event,2};
    const bool result=a2fo_rs_stage10_resources_transaction(
        &context,resource_refund_step,2)!=0;
    if(context.error)std::rethrow_exception(context.error);
    return result;
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
    struct Context {std::exception_ptr error{};};
    Context context{};
    const auto step=[](void* opaque,std::uint32_t phase,
                       std::uint32_t index) noexcept -> std::int32_t {
        auto& context=*static_cast<Context*>(opaque);
        try{
        if(phase==0){
            switch(index){
            case 0:return signature_matches(g_armada,kTeamConstructorRva,
                kExpectedTeamConstructor.data(),kExpectedTeamConstructor.size());
            case 1:return signature_matches(g_armada,kTeamDestructorRva,
                kExpectedTeamDestructor.data(),kExpectedTeamDestructor.size());
            default:return signature_matches(g_fleet_ops,kDeductResourcesForBuildRva,
                kExpectedDeductResources.data(),kExpectedDeductResources.size());
            }
        }
        switch(index){
        case 0:return g_api->install_inline_hook(
            at(g_armada,kTeamConstructorRva),
            reinterpret_cast<void*>(&team_constructor_hook),
            kExpectedTeamConstructor.size(),kExpectedTeamConstructor.data(),
            &g_team_constructor_hook);
        case 1:return g_api->install_inline_hook(
            at(g_armada,kTeamDestructorRva),
            reinterpret_cast<void*>(&team_destructor_hook),
            kExpectedTeamDestructor.size(),kExpectedTeamDestructor.data(),
            &g_team_destructor_hook);
        default:return g_api->install_inline_hook(
            at(g_fleet_ops,kDeductResourcesForBuildRva),
            reinterpret_cast<void*>(&a2fo_resources_deduct_hook),
            kExpectedDeductResources.size(),kExpectedDeductResources.data(),
            &g_deduct_hook);
        }
        }catch(...){context.error=std::current_exception();return -1;}
    };
    const std::uint32_t result=a2fo_rs_c_resources_install_runtime(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    if(result==0){
        log_line("Supported resource hook signatures were not found");
        return false;
    }
    if(result==1){
        log_line("Additional resource hook installation was incomplete");
        return false;
    }
    g_a2fo_resources_deduct_gateway = g_deduct_hook.gateway;
    g_production_integration_ready = true;
    return true;
}

bool install_resource_panel_hook() {
    struct Context {std::exception_ptr error{};};
    Context context{};
    const auto step=[](void* opaque,std::uint32_t phase,
                       std::uint32_t index) noexcept -> std::int32_t {
        auto& context=*static_cast<Context*>(opaque);
        try{
        if(phase==1)return g_api->install_inline_hook(
            at(g_armada,kResourcePanelRenderRva),
            reinterpret_cast<void*>(&resource_panel_render_hook),
            kExpectedResourcePanelRender.size(),
            kExpectedResourcePanelRender.data(),&g_resource_panel_render_hook);
        switch(index){
        case 0:return signature_matches(g_armada,kResourcePanelRenderRva,
            kExpectedResourcePanelRender.data(),kExpectedResourcePanelRender.size());
        case 1:return signature_matches(g_armada,kLocalTeamRva,
            kExpectedLocalTeam.data(),kExpectedLocalTeam.size());
        case 2:return signature_matches(g_armada,kTeamForTeamRva,
            kExpectedTeamForTeam.data(),kExpectedTeamForTeam.size());
        case 3:return signature_matches(g_armada,kParameterDbGetRectangleRva,
            kExpectedParameterDbGetRectangle.data(),kExpectedParameterDbGetRectangle.size());
        case 4:return signature_matches(g_armada,kDisplayInterfaceDrawTextRva,
            kExpectedDisplayInterfaceDrawText.data(),kExpectedDisplayInterfaceDrawText.size());
        case 5:return signature_matches(g_armada,kStandardComponentSetTooltipTextRva,
            kExpectedSetTooltipText.data(),kExpectedSetTooltipText.size());
        default:return signature_matches(g_armada,kStandardComponentSetVerboseTooltipTextRva,
            kExpectedSetTooltipText.data(),kExpectedSetTooltipText.size());
        }
        }catch(...){context.error=std::current_exception();return -1;}
    };
    const std::uint32_t result=a2fo_rs_c_resources_install_panel(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    if(result==0){
        log_line("Resource-panel signatures were not found; balances remain active");
        return false;
    }
    if(result==1){
        log_line("Resource-panel render hook was not installed");
        return false;
    }
    return true;
}

bool install_native_presentation_hooks() {
    static_assert(kNativeResourceResCallSites.size()==10);
    struct Context {std::exception_ptr error{};};
    Context context{};
    const auto step=[](void* opaque,std::uint32_t phase,
                       std::uint32_t index) noexcept -> std::int32_t {
        auto& context=*static_cast<Context*>(opaque);
        try{
        switch(phase){
        case 0:
            switch(index){
            case 0:return g_api->patch_call!=nullptr;
            case 1:return signature_matches(g_armada,kResourceComponentTooltipRva,
                kExpectedResourceComponentTooltip.data(),
                kExpectedResourceComponentTooltip.size());
            case 2:return signature_matches(g_armada,kResourceComponentVerboseTooltipRva,
                kExpectedResourceComponentTooltip.data(),
                kExpectedResourceComponentTooltip.size());
            default:return insert_c_string()!=nullptr;
            }
        case 1:{
            const auto& call_site=kNativeResourceResCallSites[index];
            return signature_matches(g_armada,call_site.rva,
                call_site.expected.data(),call_site.expected.size());
        }
        case 2:return signature_matches(g_armada,kNativeOfficerIconCallSite.rva,
            kNativeOfficerIconCallSite.expected.data(),
            kNativeOfficerIconCallSite.expected.size());
        case 3:
            if(index==0)return g_api->install_inline_hook(
                at(g_armada,kResourceComponentTooltipRva),
                reinterpret_cast<void*>(&resource_component_tooltip_hook),
                kExpectedResourceComponentTooltip.size(),
                kExpectedResourceComponentTooltip.data(),
                &g_resource_component_tooltip_hook);
            return g_api->install_inline_hook(
                at(g_armada,kResourceComponentVerboseTooltipRva),
                reinterpret_cast<void*>(&resource_component_verbose_tooltip_hook),
                kExpectedResourceComponentTooltip.size(),
                kExpectedResourceComponentTooltip.data(),
                &g_resource_component_verbose_tooltip_hook);
        case 4:{
            const auto& call_site=kNativeResourceResCallSites[index/2];
            void* replacement=index%2==0
                ?reinterpret_cast<void*>(&native_resource_icon_lookup)
                :reinterpret_cast<void*>(&native_resource_res_lookup);
            return g_api->patch_call(at(g_armada,call_site.rva),replacement,
                call_site.expected.data(),call_site.expected.size());
        }
        default:return g_api->patch_call(
            at(g_armada,kNativeOfficerIconCallSite.rva),
            reinterpret_cast<void*>(&native_officer_icon_lookup),
            kNativeOfficerIconCallSite.expected.data(),
            kNativeOfficerIconCallSite.expected.size());
        }
        }catch(...){context.error=std::current_exception();return -1;}
    };
    A2foStage8ResourcePresentationInstall installed{};
    const std::uint32_t outcome=a2fo_rs_c_resources_install_presentation(
        &context,step,&installed);
    if(context.error)std::rethrow_exception(context.error);
    if(outcome==0){
        log_line("Native resource presentation signatures were not found");
        return false;
    }
    if(outcome==1){
        log_line("Native resource Res call signatures were not found");
        return false;
    }
    if(outcome==2){
        log_line("Native officer icon signature was not found");
        return false;
    }
    if(outcome==3){
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "Native presentation integration partial: short=%s verbose=%s "
            "Res=%lu/%lu officerIcon=%s",
            installed.tooltip ? "yes" : "no",
            installed.verbose ? "yes" : "no",
            static_cast<unsigned long>(installed.patched),
            static_cast<unsigned long>(kNativeResourceResCallSites.size()),
            installed.officer ? "yes" : "no");
        log_line(message);
        return false;
    }
    log_line("Race-specific native resource presentation enabled (cached)");
    return true;
}

struct ResourcePublicContext {
    void* object;std::uint32_t resource;std::uint32_t kind;
    std::int64_t amount=0;std::uint32_t presentation=0;std::size_t index=0;
    Amounts initial{};std::int64_t value=0;const char* text="";
    std::exception_ptr error{};
};
std::uintptr_t resource_public_step(void* opaque,std::uint32_t op,
                                   std::uintptr_t argument) noexcept {
    auto& c=*static_cast<ResourcePublicContext*>(opaque);
    try{
        switch(op){
        case 0:return g_runtime_alive;
        case 1:return c.resource;
        case 2:return a2fo_rs_c_resources_set_amount(c.amount);
        case 3:return c.object!=nullptr;
        case 4:return c.presentation;
        case 5:c.index=argument;return 1;
        case 6:c.value=team_snapshot(c.object)[c.index];return 1;
        case 7:c.initial=team_snapshot(c.object);return 1;
        case 8:{
            LockGuard lock;
            Amounts& values=ensure_team_locked(c.object,c.initial);
            if(c.kind==1) values[c.index]=c.amount;
            else values[c.index]=a2fo_rs_c_resources_clamp_amount(
                a2fo::resources::saturating_add(values[c.index],c.amount));
            return 1;
        }
        case 9:c.value=class_costs(c.object)[c.index];return 1;
        case 10:c.text=localize_presentation(c.object,c.resource,c.presentation);return 1;
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
}

}  // namespace

extern "C" bool __cdecl a2fo_resources_can_pay(
    void* producer,void* object_class) noexcept {
    ResourceTransactionContext context{producer,object_class,nullptr,0};
    return a2fo_rs_stage10_resources_transaction(
        &context,resource_payment_step,0)!=0;
}

extern "C" void __cdecl a2fo_resources_commit_payment(
    void* producer,void* object_class) noexcept {
    ResourceTransactionContext context{producer,object_class,nullptr,1};
    a2fo_rs_stage10_resources_transaction(&context,resource_payment_step,1);
}

extern "C" __declspec(dllexport)
std::int64_t A2FO_CALL A2FOResources_Get(
    void* team,std::uint32_t resource) {
    ResourcePublicContext c{team,resource,0};
    a2fo_rs_stage10_resources_public(&c,resource_public_step,0);
    if(c.error)std::rethrow_exception(c.error);
    return c.value;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FOResources_Set(
    void* team,std::uint32_t resource,std::int64_t amount) {
    ResourcePublicContext c{team,resource,1,amount};
    const bool result=a2fo_rs_stage10_resources_public(&c,resource_public_step,1)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FOResources_Add(
    void* team,std::uint32_t resource,std::int64_t amount) {
    ResourcePublicContext c{team,resource,2,amount};
    const bool result=a2fo_rs_stage10_resources_public(&c,resource_public_step,2)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}

extern "C" __declspec(dllexport)
std::int32_t A2FO_CALL A2FOResources_GetCost(
    void* object_class,std::uint32_t resource) {
    ResourcePublicContext c{object_class,resource,3};
    a2fo_rs_stage10_resources_public(&c,resource_public_step,3);
    if(c.error)std::rethrow_exception(c.error);
    return static_cast<std::int32_t>(c.value);
}

extern "C" __declspec(dllexport)
const char* A2FO_CALL A2FOResources_GetPresentationText(
    void* team,std::uint32_t resource,std::uint32_t presentation) {
    ResourcePublicContext c{team,resource,4,0,presentation};
    a2fo_rs_stage10_resources_public(&c,resource_public_step,4);
    if(c.error)std::rethrow_exception(c.error);
    return c.text;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    using Ready=bool (A2FO_CALL*)() noexcept;
    using CostFields=std::array<const char*,kResourceCount>;
    using RaceFields=std::array<const char*,kResourceCount*2+kPresentationFieldCount+1>;
    struct State {const A2FO_ModuleApi* api;Ready ready=nullptr;
        CostFields cost_fields=kCostCommands;RaceFields race_fields{};std::exception_ptr error{};};
    State state{api};
    const auto step=[](void* opaque,std::uint32_t op,std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<State*>(opaque);const auto* api=c.api;
        try{
            switch(op){
            case 0:return !(!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
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
        (api->capabilities & A2FO_CAP_PRODUCER_EVENTS) == 0);
            case 1:{
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    g_a1_legacy_resource_panel_render = nullptr;
    g_a1_legacy_resource_panel_bridge_reported = false;
                return 1;
            }
            case 2:{
    HMODULE feature_pack = a2fo::startup_module_provider(
        "A2FOFeaturePack.dll", {"A2FO_ProducerEventsReady"});
    FARPROC readiness_export = feature_pack
        ? GetProcAddress(feature_pack, "A2FO_ProducerEventsReady") : nullptr;
    static_assert(sizeof(c.ready) == sizeof(readiness_export));
    std::memcpy(&c.ready, &readiness_export,
                sizeof(c.ready));
                return 1;
            }
            case 3:return g_armada && g_fleet_ops && c.ready && c.ready();
            case 4:log_line("A2FOFeaturePack Producer dispatch is required for shared refunds");return 1;
            case 5:InitializeCriticalSection(&g_lock);g_lock_ready=true;g_runtime_alive=true;return 1;
            case 6:{
                auto& race_fields=c.race_fields;
    static_assert(kResourceCount==4 && kPresentationFieldCount==40);
    const auto fill=[](void* opaque,std::uint32_t phase,
                       std::uint32_t index) noexcept {
        auto& fields=*static_cast<RaceFields*>(opaque);
        switch(phase){
        case 0:
            fields[index]=kNormalCommands[index];
            fields[index+kResourceCount]=kLotsCommands[index];
            break;
        case 1:
            fields[kResourceCount*2+index]=kPresentationCommands[index];
            break;
        default:fields.back()="name";break;
        }
    };
    a2fo_rs_c_resources_registration_fields(&race_fields,fill);
                return 1;
            }
            case 7:{
    struct Context {const A2FO_ModuleApi* api;
        CostFields* cost_fields;RaceFields* race_fields;
        std::exception_ptr error{};};
    Context context{c.api,&c.cost_fields,&c.race_fields};
    const auto step=[](void* opaque,std::uint32_t operation) noexcept -> std::int32_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(operation){
            case 0:return c.api->register_game_object_class_loaded_handler(
                kModuleName,c.cost_fields->data(),
                static_cast<std::uint32_t>(c.cost_fields->size()),
                &class_loaded_handler,nullptr);
            case 1:return c.api->register_race_loaded_handler(
                kModuleName,c.race_fields->data(),
                static_cast<std::uint32_t>(c.race_fields->size()),
                &race_loaded_handler,nullptr);
            case 2:return c.api->register_producer_event_handler(
                kModuleName,&producer_event_handler,nullptr);
            case 3:return install_runtime_hooks();
            case 4:install_native_presentation_hooks();return 1;
            default:install_resource_panel_hook();return 1;
            }
        }catch(...){c.error=std::current_exception();return -1;}
    };
    const std::uint32_t outcome=a2fo_rs_c_resources_module_init(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return outcome;
            }
            case 8:g_runtime_alive=false;DeleteCriticalSection(&g_lock);g_lock_ready=false;return 1;
            case 9:log_line("Four-resource module retained after partial initialization");return 1;
            case 10:log_line("Four independent resources enabled (10 total team resources)");return 1;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const bool result=a2fo_rs_stage10_resources_init(&state,step)!=0;
    if(state.error)std::rethrow_exception(state.error);
    return result;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Runtime hooks are process-lifetime patches; the module loader does not
    // unload initialized modules during normal play.
    g_runtime_alive = false;
    g_production_integration_ready = false;
    g_a1_legacy_resource_panel_render = nullptr;
    g_a1_legacy_resource_panel_bridge_reported = false;
}
