/*
 * Native phase-one adapter for A2FOSquadrons.
 *
 * A semantic classLabel = "squadron" is hosted by Craft so Fleet Operations
 * can keep its ordinary build-list, technology, cost, queue and build-time
 * machinery. A checked Starbase::FinishBuild vtable hook consumes the abstract
 * job before Starbase can forward a null object to OutputQueueManager. This
 * module creates the configured real member Craft, publishes them to the
 * current mission, and commits their handles to the squadron Registry.
 *
 * Native selection admission expands member gestures to the live squad. The
 * ShipDisplay read-only selection getters project one tile per squad while
 * commands retain every physical member. Accepted repair visits replenish
 * missing slots through paid native production. Native save persistence and
 * aggregate initial physical-cap reservation remain.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_module_provider.hpp"
#include "squadron_config.hpp"
#include "squadron_state.hpp"
#include "api.hpp"
#include "../A2FOFeaturePack/refit_queue_bridge_api.hpp"
#include "../../rust/stage8_a_squadrons_ffi.h"
#include "../../rust/stage10_squadrons_ffi.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

extern "C" {
std::uintptr_t __cdecl a2fo_squadrons_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_squadrons_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
std::uintptr_t __cdecl a2fo_squadrons_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1, std::uintptr_t argument2);
std::uintptr_t __cdecl a2fo_squadrons_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
std::uintptr_t __cdecl a2fo_squadrons_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
std::uintptr_t __cdecl a2fo_squadrons_call_thiscall_7(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4, std::uintptr_t argument5,
    std::uintptr_t argument6, std::uintptr_t argument7);
}

namespace {

using a2fo::squadrons::Craft;
using a2fo::squadrons::Definition;
using a2fo::squadrons::MemberClass;
using a2fo::squadrons::OdfFields;
using a2fo::squadrons::Registry;
using a2fo::squadrons::SquadId;

constexpr char kModuleName[] = "A2FOSquadrons";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs already used by the other
// A2FO runtime modules. The Starbase completion vtable is claimed before its
// native post-processing can receive a null abstract squadron descriptor.
constexpr std::uintptr_t kAiMissionGetCurrentRva = 0x00001370;
constexpr std::uintptr_t kCraftDoExpireRva = 0x000caae0;
constexpr std::uintptr_t kGameObjectClassFindRva = 0x000cd370;
constexpr std::uintptr_t kGameObjectClassConstructRva = 0x000cd390;
constexpr std::uintptr_t kGameObjectClassGetOdfNameRva = 0x000ce370;
constexpr std::uintptr_t kProducerPopBuildQueueItemRva = 0x000b79b0;
constexpr std::uintptr_t kEntityGetRva = 0x000cfff0;
constexpr std::uintptr_t kStarbaseFinishBuildRva = 0x000bbd90;
constexpr std::uintptr_t kStarbaseVtableRva = 0x002b3834;
constexpr std::uintptr_t kShipyardVtableRva = 0x002b3584;
constexpr std::uintptr_t kOutputQueueManagerVtableRva = 0x002b3714;
constexpr std::size_t kStarbaseFinishBuildVtableOffset = 0x184;
constexpr std::size_t kProducerBuildTransformVtableOffset = 0x188;
constexpr std::size_t kShipyardBuildOutputQueueOffset = 0x2b4;
constexpr std::size_t kCraftQueueStageOffset = 0x194;
constexpr std::size_t kCraftQueueOwnerOffset = 0x198;
constexpr std::size_t kProducerCurrentBuildClassOffset = 0x254;
constexpr std::size_t kProducerLastBuiltHandleOffset = 0x26c;
constexpr std::size_t kProducerCurrentQueueIdOffset = 0x2a0;
constexpr std::size_t kProducerStopConstructionEffectVtableOffset = 0x178;
constexpr std::size_t kObjectClassProjectIdOffset = 0x1cc;
constexpr std::uintptr_t kTeamTechnologyTreesPointerRva = 0x00212f08;
constexpr std::size_t kTechnologyTreeItemsOffset = 0x0c;
constexpr std::size_t kTechnologyItemActiveBuildsOffset = 0x18;

constexpr std::size_t kObjectExpiredOffset = 0x27;
constexpr std::size_t kObjectHandleOffset = 0x28;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kObjectTeamOffset = 0xec;
constexpr std::size_t kNativeSelectionCountOffset = 0xb8;
constexpr std::size_t kNativeSelectionHandlesOffset = 0x3d0;
constexpr std::size_t kArmadaSelectionHandlesOffset = 0xbc;
constexpr std::size_t kNativeSelectedFlagOffset = 0x10e;
constexpr std::uint32_t kNativeSelectionLimit = 30;
constexpr std::uintptr_t kFoSelectionAddRva = 0x1dad3c;
constexpr std::uintptr_t kUiSelectionPointerRva = 0x00368e3c;
constexpr std::size_t kSelectionGetCountVtableOffset = 0x94;
constexpr std::size_t kSelectionGetHandlesVtableOffset = 0x98;
constexpr std::uintptr_t kShipDisplayVtableRva = 0x002b4bcc;
constexpr std::size_t kShipDisplayRenderSlot = 0x58;
constexpr std::uintptr_t kDrawTextRva = 0x0011b160;
constexpr std::array<std::uintptr_t, 3> kShipDisplayCountReturns{{
    0x000f2efb, 0x000f2ca6, 0x000f2d2d}};
constexpr std::array<std::uintptr_t, 3> kShipDisplayHandlesReturns{{
    0x000f2f20, 0x000f2cb8, 0x000f2d3f}};

constexpr std::uint8_t kExpectedAiMissionGetCurrent[] = {
    0xa1, 0xdc, 0x47, 0x73, 0x00, 0xc3};
constexpr std::uint8_t kExpectedGameObjectClassFind[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedGameObjectClassConstruct[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x84, 0x00, 0x00, 0x00};
constexpr std::uint8_t kExpectedGameObjectClassGetOdfName[] = {
    0x8b, 0x89, 0xcc, 0x01, 0x00, 0x00,
    0xe9, 0x25, 0xb0, 0x18, 0x00};
constexpr std::uint8_t kExpectedStarbaseFinishBuild[] = {
    0x53, 0x56, 0x57, 0x8b, 0xf1};
constexpr std::uint8_t kExpectedProducerPopBuildQueueItem[] = {
    0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb};
constexpr std::uint8_t kExpectedEntityGet[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x4d, 0x08};
constexpr std::uint8_t kExpectedFoSelectionAdd[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf0};
constexpr std::uint8_t kExpectedUiCountCall[] = {
    0xff, 0x90, 0x94, 0x00, 0x00, 0x00};
constexpr std::uint8_t kExpectedUiHandlesCall[] = {
    0xff, 0x92, 0x98, 0x00, 0x00, 0x00};

// Craft defaults needed by the squadron descriptor itself.  No squadron
// descriptor is allowed to become a live Craft, but Fleet Ops still constructs
// a CraftClass for build-list metadata after the semantic alias is applied.
// These mirror the safe common Craft defaults used by A1Compat's wingman host.
constexpr std::array<A2FO_ClasslabelOdfDefault, 13> kSquadronDefaults{{
    {"enginesHitPercent", "5.0f"},
    {"lifeSupportHitPercent", "8.5f"},
    {"weaponsHitPercent", "5.0f"},
    {"shieldGeneratorHitPercent", "8.0f"},
    {"sensorsHitPercent", "8.0f"},
    {"crewHitPercent", "8.5f"},
    {"hullHitPercent", "57.0f"},
    {"ship", "1"},
    {"has_hitpoints", "1"},
    {"has_crew", "1"},
    {"transporter", "1"},
    {"SHOW_MOVEMENT_AUTONOMY", "1"},
    {"can_explore", "1"},
}};

constexpr std::size_t kRequiredFieldCount =
    1 + 1 + 1 + (a2fo::squadrons::kMaxMemberRows * 2) + 4;

struct Matrix34 {
    // Armada stores right, up, forward, position as four consecutive vec3s.
    float values[12]{};
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
bool g_runtime_ready = false;
void** g_starbase_finish_slot = nullptr;
void** g_shipyard_finish_slot = nullptr;
void* g_starbase_finish_original = nullptr;
A2FO_InlineHook g_selection_add_hook{};
void* g_ui_get_count_original = nullptr;
void* g_ui_get_handles_original = nullptr;
void* g_ui_selection_vtable = nullptr;
void* g_ship_display_render_original = nullptr;
std::mutex g_ui_hook_mutex;
const char* g_ui_probe_failure = nullptr;
thread_local std::array<std::uint32_t, kNativeSelectionLimit>
    g_logical_ui_handles{};
Registry g_registry;
std::mutex g_registry_mutex;
struct PendingLaunch {
    SquadId squad = 0;
    std::uint32_t yard_handle = 0;
    std::uint32_t current_handle = 0;
    std::int32_t team = -1;
    std::size_t next_slot = 0;
    std::vector<std::string> odfs;
};
std::vector<PendingLaunch> g_pending_launches;
std::unordered_map<void*, Definition> g_definitions;
std::unordered_map<void*, std::unordered_set<std::string>> g_yard_build_items;
std::unordered_map<std::string, void*> g_classes_by_odf;
std::unordered_map<std::string, std::string> g_classlabels_by_odf;
std::unordered_set<std::string> g_logged_loose_member_labels;
bool g_logged_cap_warning = false;

void* at(std::uintptr_t rva) noexcept {
    return g_armada
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(g_armada) + rva)
        : nullptr;
}

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

void log_text(const std::string& message) noexcept {
    log_line(message.c_str());
}

bool readable_range(const void* pointer, std::size_t size) noexcept {
    if (!pointer || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(pointer, &information, sizeof(information)) !=
            sizeof(information) ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(pointer);
    const auto region_end =
        reinterpret_cast<std::uintptr_t>(information.BaseAddress) +
        information.RegionSize;
    return start <= region_end && size <= region_end - start;
}

bool writable_range(void* pointer, std::size_t size) noexcept {
    if (!readable_range(pointer, size)) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(pointer, &information, sizeof(information)) !=
        sizeof(information)) return false;
    const DWORD protection = information.Protect & 0xff;
    return protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

bool executable_address(const void* pointer) noexcept {
    if (!readable_range(pointer, 1)) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(pointer, &information, sizeof(information)) !=
            sizeof(information) || information.State != MEM_COMMIT) return false;
    const DWORD protection = information.Protect & 0xff;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

template <typename T>
T read_at(const void* object, std::size_t offset,
          T fallback = T{}) noexcept {
    if (!object || !readable_range(
            static_cast<const std::uint8_t*>(object) + offset,
            sizeof(T))) {
        return fallback;
    }
    T value{};
    std::memcpy(&value,
                static_cast<const std::uint8_t*>(object) + offset,
                sizeof(value));
    return value;
}

template <std::size_t Size>
bool signature_matches(std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    return g_armada && readable_range(at(rva), Size) &&
        std::memcmp(at(rva), expected, Size) == 0;
}

bool preflight_signatures() noexcept {
    const auto step = +[](void*, std::uint32_t op,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        switch (op) {
        case 0: {
            const std::uintptr_t getters[]{
                0xce170, 0xce190, 0xce180, 0xce1b0, 0xce1a0, 0xce1c0};
            const std::uint8_t expected[]{0x8b, 0x81,
                static_cast<std::uint8_t>(0x80 + index * 4), 0, 0, 0, 0xc3};
            return signature_matches(getters[index], expected);
        }
        case 1: {
            const std::uint8_t build_time[]{0x8b, 0x47, 0x68};
            return signature_matches(0xce29f, build_time);
        }
        case 2: return signature_matches(kAiMissionGetCurrentRva,
                                         kExpectedAiMissionGetCurrent);
        case 3: return signature_matches(kGameObjectClassFindRva,
                                         kExpectedGameObjectClassFind);
        case 4: return signature_matches(kGameObjectClassConstructRva,
                                         kExpectedGameObjectClassConstruct);
        case 5: return signature_matches(kGameObjectClassGetOdfNameRva,
                                         kExpectedGameObjectClassGetOdfName);
        case 6: return signature_matches(kStarbaseFinishBuildRva,
                                         kExpectedStarbaseFinishBuild);
        case 7: return signature_matches(kEntityGetRva,
                                         kExpectedEntityGet);
        case 8: return signature_matches(kProducerPopBuildQueueItemRva,
                                         kExpectedProducerPopBuildQueueItem);
        default: return 0;
        }
    };
    return a2fo_rs_stage8_squadrons_preflight(step, nullptr) != 0;
}

std::uint32_t object_handle(const void* object) noexcept {
    return read_at<std::uint32_t>(object, kObjectHandleOffset, 0);
}

void* object_class(const void* object) noexcept {
    return read_at<void*>(object, kObjectClassOffset, nullptr);
}

std::int32_t object_team(const void* object) noexcept {
    return read_at<std::int32_t>(object, kObjectTeamOffset, -1);
}

bool object_expired(const void* object) noexcept {
    return read_at<std::uint8_t>(object, kObjectExpiredOffset, 1) != 0;
}

void* find_entity(std::uint32_t handle) noexcept {
    if (!g_armada || handle == 0) return nullptr;
    using EntityGetFn = void* (__cdecl*)(std::uint32_t);
    const auto get = reinterpret_cast<EntityGetFn>(at(kEntityGetRva));
    return get ? get(handle) : nullptr;
}

std::string lower_ascii(std::string value) {
    struct Context { std::string& text; } context{value};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (op == 0) return c.text.size();
        if (op == 1) {
            char& character = c.text[index];
            character = static_cast<char>(
                std::tolower(static_cast<unsigned char>(character)));
            return 1;
        }
        return 0;
    };
    a2fo_rs_stage8_squadrons_lower_label(step, &context);
    return value;
}

OdfFields copy_fields(const A2FO_GameObjectClassLoadedEvent* event) {
    struct Context {
        const A2FO_GameObjectClassLoadedEvent* event;
        OdfFields fields;
        std::exception_ptr exception{};
        explicit Context(const A2FO_GameObjectClassLoadedEvent* value)
            : event(value) {}
    } context{event};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t value) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (op) {
            case 0: return c.event && c.event->odf_fields;
            case 1: return c.event->odf_field_count;
            case 2: c.fields.reserve(value); return 1;
            case 3: {
                const auto& field = c.event->odf_fields[value];
                return field.name.data &&
                    (field.value.data || field.value.size == 0);
            }
            case 4: {
                const auto& field = c.event->odf_fields[value];
                c.fields.emplace_back(
                    std::string(field.name.data, field.name.size),
                    std::string(field.value.data ? field.value.data : "",
                                field.value.size));
                return 1;
            }
            default: return 0;
            }
        } catch (...) {
            c.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_squadrons_copy_fields(step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
    return std::move(context.fields);
}

std::string field_value(const OdfFields& fields, const char* name) {
    struct BytesView { const std::uint8_t* data; std::size_t len; };
    struct Context {
        const OdfFields& fields;
        const char* name;
        std::string result;
        BytesView view{};
        std::size_t start = 0;
        std::exception_ptr exception{};
        Context(const OdfFields& source, const char* wanted)
            : fields(source), name(wanted) {}
    } context{fields, name};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t value) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (op) {
            case 0: return c.name != nullptr;
            case 1: return c.fields.size();
            case 2: return _stricmp(c.fields[value].first.c_str(), c.name) == 0;
            case 3:
                c.result = c.fields[value].second;
                c.view = {reinterpret_cast<const std::uint8_t*>(c.result.data()),
                          c.result.size()};
                return reinterpret_cast<std::uintptr_t>(&c.view);
            case 4: c.start = value; return 1;
            case 5: c.result = c.result.substr(c.start, value); return 1;
            case 6:
                c.result = c.result.substr(1, c.result.size() - 2);
                return 1;
            case 7: c.result.clear(); return 1;
            default: return 0;
            }
        } catch (...) {
            c.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_squadrons_field_value(step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
    return std::move(context.result);
}

void* find_class(const std::string& odf) noexcept {
    if (!g_armada || odf.empty()) return nullptr;
    using FindClassFn = void* (__cdecl*)(const char*);
    const auto find = reinterpret_cast<FindClassFn>(at(kGameObjectClassFindRva));
    return find ? find(odf.c_str()) : nullptr;
}

std::string class_odf_name(void* object_class_pointer) {
    if (!object_class_pointer || !g_armada) return {};
    const char* name = reinterpret_cast<const char*>(
        a2fo_squadrons_call_thiscall_0(
            at(kGameObjectClassGetOdfNameRva), object_class_pointer));
    if (!name || !readable_range(name, 1)) return {};
    const char* end = static_cast<const char*>(std::memchr(name, '\0', 256));
    if (!end) return {};
    return a2fo::squadrons::odf_key(
        std::string(name, static_cast<std::size_t>(end - name)));
}

void add_to_current_mission(void* object) noexcept {
    if (!object || !g_armada) return;
    using GetCurrentFn = void* (__cdecl*)();
    const auto get_current = reinterpret_cast<GetCurrentFn>(
        at(kAiMissionGetCurrentRva));
    void* mission = get_current ? get_current() : nullptr;
    void* vtable = read_at<void*>(mission, 0, nullptr);
    void* add_object = read_at<void*>(vtable, 0x18, nullptr);
    if (mission && add_object) {
        a2fo_squadrons_call_thiscall_1(
            add_object, mission, reinterpret_cast<std::uintptr_t>(object));
    }
}

void expire_craft(void* craft) noexcept {
    if (!craft || object_expired(craft) || !g_armada) return;
    a2fo_squadrons_call_thiscall_0(at(kCraftDoExpireRva), craft);
}

MemberClass resolve_member_class(const std::string& odf) {
    struct BytesView { const std::uint8_t* data; std::size_t len; };
    struct Context {
        const std::string& odf;
        std::string key;
        void* klass = nullptr;
        std::string label;
        BytesView view{};
        std::exception_ptr exception{};
        explicit Context(const std::string& name) : odf(name) {}
    } context{odf};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (op) {
            case 0:
                c.key = a2fo::squadrons::odf_key(c.odf);
                return !c.key.empty();
            case 1: {
                const auto cached = g_classes_by_odf.find(c.key);
                if (cached != g_classes_by_odf.end()) c.klass = cached->second;
                return c.klass != nullptr;
            }
            case 2: c.klass = find_class(c.key); return c.klass != nullptr;
            case 3: return g_definitions.count(c.klass) != 0;
            case 4: {
                const auto label_it = g_classlabels_by_odf.find(c.key);
                c.label = label_it == g_classlabels_by_odf.end()
                    ? std::string{} : label_it->second;
                c.view = {reinterpret_cast<const std::uint8_t*>(c.label.data()),
                          c.label.size()};
                return reinterpret_cast<std::uintptr_t>(&c.view);
            }
            case 5:
                if (g_logged_loose_member_labels.insert(c.label).second) {
                    log_text("Prototype member resolver accepted classLabel '" +
                             c.label +
                             "' as Craft-capable; native CraftClass predicate is pending");
                }
                return 1;
            default: return 0;
            }
        } catch (...) {
            c.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    const auto result = a2fo_rs_stage8_squadrons_resolve_member(step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
    switch (result) {
    case 1: return MemberClass::mobile_craft;
    case 2: return MemberClass::unsupported;
    case 3: return MemberClass::squadron;
    default: return MemberClass::missing;
    }
}

#include "native_economics.inl"

void* output_queue_add_method(void* producer) noexcept {
    void* queue = read_at<void*>(
        producer, kShipyardBuildOutputQueueOffset, nullptr);
    void* vtable = read_at<void*>(queue, 0, nullptr);
    if (vtable != at(kOutputQueueManagerVtableRva)) return nullptr;
    void* add = read_at<void*>(vtable, 4 * sizeof(void*), nullptr);
    return executable_address(add) ? add : nullptr;
}

bool build_transform(void* producer, Matrix34* result) noexcept {
    if (!result) return false;
    void* vtable = read_at<void*>(producer, 0, nullptr);
    void* method = read_at<void*>(
        vtable, kProducerBuildTransformVtableOffset, nullptr);
    if (!executable_address(method)) return false;
    // Producer::FinishBuild calls this same virtual method immediately before
    // constructing the output Craft. The yard owns the exact launch position.
    a2fo_squadrons_call_thiscall_1(
        method, producer, reinterpret_cast<std::uintptr_t>(result));
    return true;
}

void* construct_member(const std::string& odf, const Matrix34& transform,
                       std::int32_t team, std::uint32_t producer_handle,
                       std::size_t ordinal) noexcept {
    void* member_class = find_class(odf);
    if (!member_class) return nullptr;
    char label[64]{};
    std::snprintf(label, sizeof(label), "A2FOSQ:%08lX:%lu",
                  static_cast<unsigned long>(producer_handle),
                  static_cast<unsigned long>(ordinal));
    void* member = reinterpret_cast<void*>(
        a2fo_squadrons_call_thiscall_4(
            at(kGameObjectClassConstructRva), member_class,
            reinterpret_cast<std::uintptr_t>(&transform),
            static_cast<std::uintptr_t>(team), 0,
            reinterpret_cast<std::uintptr_t>(label)));
    if (member) add_to_current_mission(member);
    return member;
}

struct UncommittedMember {
    void* craft;
    SquadId created_squad = 0;
    ~UncommittedMember() {
        if (!craft) return;
        const auto handle = object_handle(craft);
        expire_craft(craft);
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        g_registry.remove(handle);
        if (created_squad) g_registry.finish_launching(created_squad);
    }
};

struct SquadronSpawnContext {
    void* producer;
    const Definition* definition;
    std::int32_t team = -1;
    std::uint32_t producer_handle = 0;
    Matrix34 base{};
    void* add_method = nullptr;
    std::string validation;
    PendingLaunch job;
    void* first = nullptr;
    UncommittedMember rollback{nullptr};
    std::uint32_t first_handle = 0;
    std::string first_odf;
    a2fo::squadrons::Result<SquadId> created;
    std::exception_ptr exception{};

    SquadronSpawnContext(void* value, const Definition* source)
        : producer(value), definition(source) {}
};

std::uintptr_t squadron_spawn_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t row_index) noexcept {
    auto& context = *static_cast<SquadronSpawnContext*>(opaque);
    try {
        switch (op) {
        case 0: return context.producer && g_runtime_ready;
        case 1:
            context.team = object_team(context.producer);
            return static_cast<std::uint32_t>(context.team);
        case 2:
            context.producer_handle = object_handle(context.producer);
            return context.producer_handle;
        case 3:
            context.add_method = output_queue_add_method(context.producer);
            return reinterpret_cast<std::uintptr_t>(context.add_method);
        case 4: return build_transform(context.producer, &context.base);
        case 5:
            log_line("Squadron completion rejected: yard identity/output queue/build transform unavailable");
            return 1;
        case 6:
            context.validation = a2fo::squadrons::validate_definition(
                *context.definition, &resolve_member_class);
            return !context.validation.empty();
        case 7:
            log_text("Squadron completion rejected for '" +
                     context.definition->odf + "': " + context.validation);
            return 1;
        case 8:
            context.job.yard_handle = context.producer_handle;
            context.job.team = context.team;
            context.job.odfs.reserve(context.definition->size());
            return 1;
        case 9: return context.definition->members.size();
        case 10: return context.definition->members[row_index].count;
        case 11:
            context.job.odfs.push_back(
                context.definition->members[row_index].odf);
            return 1;
        case 12: return context.job.odfs.size();
        case 13:
            g_pending_launches.reserve(g_pending_launches.size() + 1);
            return 1;
        case 14:
            context.first = construct_member(
                context.job.odfs[0], context.base, context.team,
                context.producer_handle, 0);
            context.rollback.craft = context.first;
            return reinterpret_cast<std::uintptr_t>(context.first);
        case 15:
            log_line("Squadron first output could not be constructed");
            return 1;
        case 16:
            context.first_handle = object_handle(context.first);
            context.first_odf = class_odf_name(object_class(context.first));
            return context.first_handle &&
                object_team(context.first) == context.team &&
                context.first_odf == context.job.odfs[0];
        case 17:
            log_line("Squadron first output failed its publication identity check");
            return 1;
        case 18: {
            std::lock_guard<std::mutex> lock(g_registry_mutex);
            context.created = g_registry.create_launching(
                *context.definition,
                static_cast<a2fo::squadrons::TeamId>(context.team),
                Craft{context.first_handle,
                      static_cast<a2fo::squadrons::TeamId>(context.team),
                      context.first_odf}, &resolve_member_class);
            if (context.created)
                context.rollback.created_squad = *context.created.value;
            return static_cast<bool>(context.created);
        }
        case 19:
            log_text("Squadron '" + context.definition->odf +
                     "' initial membership failed: " + context.created.error);
            return 1;
        case 20: {
            void* queue = read_at<void*>(
                context.producer, kShipyardBuildOutputQueueOffset, nullptr);
            a2fo_squadrons_call_thiscall_1(
                context.add_method, queue,
                reinterpret_cast<std::uintptr_t>(context.first));
            return 1;
        }
        case 21:
            return read_at<std::uint32_t>(
                       context.first, kCraftQueueStageOffset, 0) != 0 &&
                read_at<std::uint32_t>(
                    context.first, kCraftQueueOwnerOffset, 0) ==
                    context.producer_handle;
        case 22:
            log_line("Squadron first output did not enter the yard queue");
            return 1;
        case 23:
            context.job.squad = *context.created.value;
            context.job.current_handle = context.first_handle;
            context.job.next_slot = 1;
            return 1;
        case 24: return context.job.next_slot < context.job.odfs.size();
        case 25: g_pending_launches.push_back(std::move(context.job)); return 1;
        case 26: {
            std::lock_guard<std::mutex> lock(g_registry_mutex);
            g_registry.finish_launching(context.job.squad);
            return 1;
        }
        case 27: context.rollback.craft = nullptr; return 1;
        case 28: {
            char message[240]{};
            std::snprintf(message, sizeof(message),
                "Squadron '%s' started sequential yard launch as squad %llu (%lu members)",
                context.definition->odf.c_str(),
                static_cast<unsigned long long>(*context.created.value),
                static_cast<unsigned long>(context.definition->size()));
            log_line(message);
            return 1;
        }
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

bool spawn_squadron(void* producer, const Definition& definition) {
    SquadronSpawnContext context{producer, &definition};
    const bool result = a2fo_rs_stage8_squadrons_spawn(
        &squadron_spawn_step, &context) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return result;
}

void finish_pending_launch(const PendingLaunch& job,
                           const char* reason) noexcept {
    {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        g_registry.finish_launching(job.squad);
    }
    char message[240]{};
    std::snprintf(message, sizeof(message),
        "Squad %llu sequential yard launch %s (%lu/%lu outputs admitted)",
        static_cast<unsigned long long>(job.squad), reason,
        static_cast<unsigned long>(job.next_slot),
        static_cast<unsigned long>(job.odfs.size()));
    log_line(message);
}

struct SquadronLaunchNextContext {
    PendingLaunch* job;
    void* yard = nullptr;
    void* add_method = nullptr;
    Matrix34 transform{};
    std::size_t slot = 0;
    void* member = nullptr;
    UncommittedMember rollback{nullptr};
    std::uint32_t handle = 0;
    std::string odf;
    std::exception_ptr exception{};

    explicit SquadronLaunchNextContext(PendingLaunch* value) : job(value) {}
};

std::uintptr_t squadron_launch_next_step(void* opaque, std::uint32_t op,
                                        std::uintptr_t) noexcept {
    auto& context = *static_cast<SquadronLaunchNextContext*>(opaque);
    try {
        switch (op) {
        case 0: return context.job &&
            context.job->next_slot < context.job->odfs.size();
        case 1:
            context.yard = find_entity(context.job->yard_handle);
            return reinterpret_cast<std::uintptr_t>(context.yard);
        case 2: return object_expired(context.yard);
        case 3: return static_cast<std::uint32_t>(object_team(context.yard));
        case 4: return static_cast<std::uint32_t>(context.job->team);
        case 5:
            context.add_method = output_queue_add_method(context.yard);
            return reinterpret_cast<std::uintptr_t>(context.add_method);
        case 6: return build_transform(context.yard, &context.transform);
        case 7: context.slot = context.job->next_slot; return 1;
        case 8:
            context.member = construct_member(
                context.job->odfs[context.slot], context.transform,
                context.job->team, context.job->yard_handle, context.slot);
            context.rollback.craft = context.member;
            return reinterpret_cast<std::uintptr_t>(context.member);
        case 9:
            context.handle = object_handle(context.member);
            context.odf = class_odf_name(object_class(context.member));
            return context.handle &&
                object_team(context.member) == context.job->team &&
                context.odf == context.job->odfs[context.slot];
        case 10: {
            std::lock_guard<std::mutex> lock(g_registry_mutex);
            return g_registry.add_launching(
                context.job->squad, context.slot,
                Craft{context.handle,
                      static_cast<a2fo::squadrons::TeamId>(context.job->team),
                      context.odf});
        }
        case 11: {
            void* queue = read_at<void*>(
                context.yard, kShipyardBuildOutputQueueOffset, nullptr);
            a2fo_squadrons_call_thiscall_1(
                context.add_method, queue,
                reinterpret_cast<std::uintptr_t>(context.member));
            return 1;
        }
        case 12:
            return read_at<std::uint32_t>(
                       context.member, kCraftQueueStageOffset, 0) != 0 &&
                read_at<std::uint32_t>(
                    context.member, kCraftQueueOwnerOffset, 0) ==
                    context.job->yard_handle;
        case 13:
            context.job->current_handle = context.handle;
            ++context.job->next_slot;
            context.rollback.craft = nullptr;
            return 1;
        case 14: {
            char message[240]{};
            std::snprintf(message, sizeof(message),
                "Squad %llu output %lu/%lu '%s' entered yard queue after prior exit",
                static_cast<unsigned long long>(context.job->squad),
                static_cast<unsigned long>(context.job->next_slot),
                static_cast<unsigned long>(context.job->odfs.size()),
                context.odf.c_str());
            log_line(message);
            return 1;
        }
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

bool launch_next_member(PendingLaunch* job) {
    SquadronLaunchNextContext context{job};
    const bool result = a2fo_rs_stage8_squadrons_launch_next(
        &squadron_launch_next_step, &context) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return result;
}

struct SquadronPendingContext {
    void* craft;
    std::uint32_t handle = 0;
    std::optional<PendingLaunch> advancing;
    std::exception_ptr exception{};

    explicit SquadronPendingContext(void* value) : craft(value) {}
};

std::uintptr_t squadron_pending_step(void* opaque, std::uint32_t op,
                                    std::uintptr_t index) noexcept {
    auto& context = *static_cast<SquadronPendingContext*>(opaque);
    try {
        switch (op) {
        case 0: context.handle = object_handle(context.craft); return context.handle;
        case 1: return g_pending_launches.size();
        case 2: return context.handle == g_pending_launches[index].yard_handle;
        case 3:
            context.advancing.emplace(std::move(g_pending_launches[index]));
            g_pending_launches.erase(g_pending_launches.begin() + index);
            return 1;
        case 4:
            finish_pending_launch(
                *context.advancing, "stopped because the yard was removed");
            return 1;
        case 5: return context.handle == g_pending_launches[index].current_handle;
        case 6: g_pending_launches[index].current_handle = 0; return 1;
        case 7: return context.handle == g_pending_launches[index].current_handle;
        case 8:
            return g_pending_launches[index].current_handle == 0 &&
                context.handle == g_pending_launches[index].yard_handle;
        case 9: return g_pending_launches[index].current_handle != 0;
        case 10:
            return read_at<std::uint32_t>(
                context.craft, kCraftQueueStageOffset, 1) == 0;
        case 11:
            return context.advancing->next_slot == context.advancing->odfs.size();
        case 12: finish_pending_launch(*context.advancing, "completed"); return 1;
        case 13:
            try { return launch_next_member(&*context.advancing); }
            catch (...) {
                log_line("Squad member construction raised an exception; ending this launch job");
                return 0;
            }
        case 14:
            g_pending_launches.push_back(std::move(*context.advancing));
            return 1;
        case 15:
            finish_pending_launch(*context.advancing,
                                  "stopped after an output failure");
            return 1;
        case 16: context.advancing.reset(); return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

void update_pending_launches(void* craft, std::uint32_t kind) {
    SquadronPendingContext context{craft};
    a2fo_rs_stage8_squadrons_pending(
        &squadron_pending_step, &context, kind,
        A2FO_CRAFT_EVENT_CLEANUP, A2FO_CRAFT_EVENT_SIMULATE_POST);
    if (context.exception) std::rethrow_exception(context.exception);
}

// Producer::FinishBuild is too late for abstract outputs: Starbase::FinishBuild
// forwards its null result into OutputQueueManager, which dereferences +0x44.
// Consume the job at the same outer boundary used by A1Compat for in-place
// officer upgrades. The ordinary Starbase path always chains unchanged.
std::uintptr_t __attribute__((fastcall)) starbase_finish_build_hook(
    void* starbase, void*) noexcept;

bool supported_producer(void* producer) noexcept {
    void* vtable = read_at<void*>(producer, 0, nullptr);
    return g_starbase_finish_slot && g_shipyard_finish_slot && vtable &&
        vtable == at(kShipyardVtableRva) && output_queue_add_method(producer) &&
        read_at<void*>(vtable, kStarbaseFinishBuildVtableOffset, nullptr) ==
            reinterpret_cast<void*>(&starbase_finish_build_hook);
}

template <std::size_t N>
bool ui_return_address(void* address,
                       const std::array<std::uintptr_t, N>& sites) noexcept {
    if (!g_armada) return false;
    const auto actual = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(g_armada);
    for (const auto site : sites) if (actual == base + site) return true;
    return false;
}

std::uint32_t logical_ui_selection(void* selection) noexcept {
    struct Context {
        void* selection;
        std::uint32_t count = 0;
        const std::uint32_t* handles = nullptr;
        std::uint32_t handle = 0;
        SquadId squad_id = 0;
        std::array<SquadId, kNativeSelectionLimit> seen{};
        std::unique_lock<std::mutex> lock{};
        explicit Context(void* selected) : selection(selected) {}
    } context{selection};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t value) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: return g_ui_get_count_original && g_ui_get_handles_original;
        case 1:
            c.count = static_cast<std::uint32_t>(
                a2fo_squadrons_call_thiscall_0(g_ui_get_count_original, c.selection));
            return 1;
        case 2:
            c.handles = reinterpret_cast<const std::uint32_t*>(
                a2fo_squadrons_call_thiscall_0(
                    g_ui_get_handles_original, c.selection));
            return 1;
        case 3:
            return c.count <= kNativeSelectionLimit &&
                (!c.count || readable_range(c.handles,
                    c.count * sizeof(std::uint32_t)));
        case 4:
            c.lock = std::unique_lock<std::mutex>(g_registry_mutex);
            return 1;
        case 5:
            c.handle = c.handles[value];
            return c.handle != 0;
        case 6: {
            const auto* squad = g_registry.containing(c.handle);
            if (!squad) return 0;
            c.squad_id = squad->id;
            return 1;
        }
        case 7: return c.seen[value] == c.squad_id;
        case 8: c.seen[value] = c.squad_id; return 1;
        case 9: g_logical_ui_handles[value] = c.handle; return 1;
        case 10: return c.count;
        default: return 0;
        }
    };
    return a2fo_rs_stage8_squadrons_logical_selection(step, &context);
}

std::uintptr_t ui_projection(void* selection, void* return_address,
                             std::uintptr_t physical, bool handles) noexcept {
    struct Context {
        void* selection;
        void* return_address;
        bool handles;
    } context{selection, return_address, handles};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: return g_runtime_ready;
        case 1: return c.handles
            ? ui_return_address(c.return_address, kShipDisplayHandlesReturns)
            : ui_return_address(c.return_address, kShipDisplayCountReturns);
        case 2: return logical_ui_selection(c.selection);
        case 3: return reinterpret_cast<std::uintptr_t>(
            g_logical_ui_handles.data());
        default: return 0;
        }
    };
    return a2fo_rs_stage8_squadrons_ui_projection(
        step, &context, physical, handles);
}

std::uintptr_t __attribute__((fastcall)) ui_get_count_hook(
    void* selection, void*) noexcept {
    const auto physical = a2fo_squadrons_call_thiscall_0(
        g_ui_get_count_original, selection);
    return ui_projection(selection, __builtin_return_address(0),
                         physical, false);
}

std::uintptr_t __attribute__((fastcall)) ui_get_handles_hook(
    void* selection, void*) noexcept {
    const auto physical = a2fo_squadrons_call_thiscall_0(
        g_ui_get_handles_original, selection);
    return ui_projection(selection, __builtin_return_address(0),
                         physical, true);
}

void ensure_logical_ui_hooks() noexcept {
    std::lock_guard<std::mutex> lock(g_ui_hook_mutex);
    struct Context {
        void* selection = nullptr;
        void* vtable = nullptr;
        void** count_slot = nullptr;
        void** handles_slot = nullptr;
        void* count_method = nullptr;
        void* handles_method = nullptr;
        std::uint32_t native_count = 0;
        std::uintptr_t native_handles = 0;
        DWORD old_protection = 0;
    } context{};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t value) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: return g_ui_selection_vtable != nullptr;
        case 1: {
            constexpr std::uintptr_t sites[]{
                0x000f2ef5, 0x000f2ca0, 0x000f2d27,
                0x000f2f1a, 0x000f2cb2, 0x000f2d39};
            return value < 6 && signature_matches(sites[value],
                value < 3 ? kExpectedUiCountCall : kExpectedUiHandlesCall);
        }
        case 2:
            c.selection = read_at<void*>(at(kUiSelectionPointerRva), 0, nullptr);
            return reinterpret_cast<std::uintptr_t>(c.selection);
        case 3:
            c.vtable = read_at<void*>(c.selection, 0, nullptr);
            return reinterpret_cast<std::uintptr_t>(c.vtable);
        case 4:
            c.count_slot = reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(c.vtable) +
                kSelectionGetCountVtableOffset);
            c.handles_slot = reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(c.vtable) +
                kSelectionGetHandlesVtableOffset);
            return readable_range(c.count_slot, 2 * sizeof(void*));
        case 5:
            c.count_method = *c.count_slot;
            c.handles_method = *c.handles_slot;
            return executable_address(c.count_method) &&
                executable_address(c.handles_method);
        case 6:
            c.native_count = static_cast<std::uint32_t>(
                a2fo_squadrons_call_thiscall_0(c.count_method, c.selection));
            return 1;
        case 7:
            c.native_handles = a2fo_squadrons_call_thiscall_0(
                c.handles_method, c.selection);
            return 1;
        case 8:
            return c.native_count <= kNativeSelectionLimit &&
                c.native_handles &&
                (!c.native_count || readable_range(
                    reinterpret_cast<void*>(c.native_handles),
                    c.native_count * sizeof(std::uint32_t)));
        case 9: {
            const auto selection_address = reinterpret_cast<std::uintptr_t>(
                c.selection);
            char message[220]{};
            std::snprintf(message, sizeof(message),
                "Squad UI getter mismatch: vtable=%p nativeCount=%lu rawCount=%lu handlesOffset=%ld",
                c.vtable, static_cast<unsigned long>(c.native_count),
                static_cast<unsigned long>(read_at<std::uint32_t>(
                    c.selection, kNativeSelectionCountOffset, 0)),
                static_cast<long>(c.native_handles - selection_address));
            log_line(message);
            return 1;
        }
        case 10: return VirtualProtect(c.count_slot, 2 * sizeof(void*),
                                        PAGE_READWRITE, &c.old_protection) != 0;
        case 11:
            g_ui_get_count_original = c.count_method;
            g_ui_get_handles_original = c.handles_method;
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(c.count_slot),
                reinterpret_cast<void*>(&ui_get_count_hook));
            return 1;
        case 12:
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(c.handles_slot),
                reinterpret_cast<void*>(&ui_get_handles_hook));
            return 1;
        case 13: {
            DWORD ignored = 0;
            VirtualProtect(c.count_slot, 2 * sizeof(void*),
                           c.old_protection, &ignored);
            return 1;
        }
        case 14:
            g_ui_selection_vtable = c.vtable;
            log_line("Squadron selection panel projection installed: one tile per squad");
            return 1;
        case 20: {
            constexpr const char* reasons[]{
                "ShipDisplay call-site signature mismatch",
                "Armada selection singleton unavailable",
                "selection vtable unavailable",
                "selection getters unreadable",
                "selection getters are not executable",
                "selection getter layout mismatch",
                "selection vtable write protection unavailable"};
            const char* reason = reasons[value];
            if (g_ui_probe_failure != reason) {
                g_ui_probe_failure = reason;
                char message[220]{};
                std::snprintf(message, sizeof(message),
                    "Squad selection panel projection unavailable: %s", reason);
                log_line(message);
            }
            return 1;
        }
        default: return 0;
        }
    };
    a2fo_rs_stage8_squadrons_install_ui(step, &context);
}

struct Rectangle {
    std::int32_t left, top, right, bottom;
};

// The render scope owns these temporary UI vtables. Native bar providers
// still decide which resource they display (hull or shields); each provider
// is evaluated for every survivor and its current/max pair is summed. Only
// the bar's presentation target changes, never a Craft's health fields.
struct SquadBarOverride {
    void* bar = nullptr;
    void* original_vtable = nullptr;
    std::array<void*, 11> vtable{};
};
thread_local SquadBarOverride* g_active_bars = nullptr;
thread_local std::size_t g_active_bar_count = 0;

struct LiveSquad {
    std::array<std::uint32_t, a2fo::squadrons::kMaxMembers> handles{};
    std::size_t count = 0;
    std::size_t maximum = 0;
    std::int32_t team = -1;
};

LiveSquad live_squad(void* craft) noexcept {
    struct Context {
        void* craft;
        LiveSquad result{};
        std::uint32_t handle = 0;
        const a2fo::squadrons::Squadron* squad = nullptr;
        std::size_t slot = 0;
        std::unique_lock<std::mutex> lock{};
        explicit Context(void* value) : craft(value) {}
    } context{craft};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t value) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: c.handle = object_handle(c.craft); return 1;
        case 1:
            c.lock = std::unique_lock<std::mutex>(g_registry_mutex);
            c.squad = g_registry.containing(c.handle);
            return c.squad != nullptr;
        case 2: return static_cast<std::uint32_t>(object_team(c.craft));
        case 3: return static_cast<std::uint32_t>(c.squad->team);
        case 4:
            c.result.team = static_cast<std::int32_t>(c.squad->team);
            c.result.maximum = c.squad->slots.size();
            return 1;
        case 5: return c.squad->slots.size();
        case 6:
            c.slot = value;
            return c.squad->slots[c.slot].member != 0;
        case 7:
            c.result.handles[value] = static_cast<std::uint32_t>(
                c.squad->slots[c.slot].member);
            return 1;
        case 8: c.result.count = value; return 1;
        default: return 0;
        }
    };
    a2fo_rs_stage8_squadrons_live_squad(
        step, &context, static_cast<std::uint32_t>(context.result.handles.size()));
    return context.result;
}

void* __attribute__((fastcall)) squad_bar_values_hook(
    void* bar, void*, void* output) noexcept {
    struct Context {
        void* bar;
        void* output;
        SquadBarOverride* binding = nullptr;
        void* original = nullptr;
        void* craft = nullptr;
        LiveSquad squad{};
        Context(void* source, void* result) : bar(source), output(result) {}
    } context{bar, output};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t value) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: return g_active_bar_count;
        case 1:
            if (g_active_bars[value].bar != c.bar) return 0;
            c.binding = &g_active_bars[value];
            return 1;
        case 2:
            c.original = read_at<void*>(c.binding->original_vtable, 0x24, nullptr);
            c.craft = read_at<void*>(c.bar, 0x428, nullptr);
            c.squad = live_squad(c.craft);
            return 1;
        case 3:
            return !c.squad.count ||
                !writable_range(c.output, sizeof(float) * 2);
        case 4:
            return a2fo_squadrons_call_thiscall_1(
                c.original, c.bar, reinterpret_cast<std::uintptr_t>(c.output));
        case 5: {
            struct Accumulation {Context* outer;void* member=nullptr;
                std::array<float,2> total{},values{};};
            Accumulation state{&c};
            const auto numeric_step=+[](void* opaque,std::uint32_t code,std::uintptr_t index) noexcept -> std::uintptr_t {
                auto& n=*static_cast<Accumulation*>(opaque);auto& c=*n.outer;
                switch(code){
                case 0:return c.squad.count;
                case 1:
                    n.member=find_entity(c.squad.handles[index]);
                    return n.member && !object_expired(n.member) && object_team(n.member)==c.squad.team;
                case 2:
                    std::memcpy(static_cast<std::uint8_t*>(c.bar)+0x428,&n.member,sizeof(n.member));
                    n.values={};return 1;
                case 3:a2fo_squadrons_call_thiscall_1(c.original,c.bar,
                    reinterpret_cast<std::uintptr_t>(n.values.data()));return 1;
                case 4:a2fo_rs_stage10_squadrons_values(n.total.data(),n.values.data());return 1;
                case 6:std::memcpy(static_cast<std::uint8_t*>(c.bar)+0x428,&c.craft,sizeof(c.craft));return 1;
                case 8:a2fo_rs_stage10_squadrons_values_copy(static_cast<float*>(c.output),n.total.data());return 1;
                default:return 0;
                }
            };
            a2fo_rs_stage10_squadrons_bar(&state,numeric_step,0);
            return reinterpret_cast<std::uintptr_t>(c.output);
        }
        default: return 0;
        }
    };
    return reinterpret_cast<void*>(a2fo_rs_stage8_squadrons_bar(
        step, &context, output));
}

void* __attribute__((fastcall)) squad_bar_colour_hook(
    void* bar, void*, void* output) noexcept {
    struct Context {
        void* bar;
        void* output;
        SquadBarOverride* binding = nullptr;
        void* original = nullptr;
        void* craft = nullptr;
        LiveSquad squad{};
        Context(void* source, void* result) : bar(source), output(result) {}
    } context{bar, output};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t value) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: return g_active_bar_count;
        case 1:
            if (g_active_bars[value].bar != c.bar) return 0;
            c.binding = &g_active_bars[value];
            return 1;
        case 2:
            c.original = read_at<void*>(c.binding->original_vtable, 0x20, nullptr);
            c.craft = read_at<void*>(c.bar, 0x428, nullptr);
            c.squad = live_squad(c.craft);
            return 1;
        case 3:
            return !c.squad.count ||
                !writable_range(c.output, sizeof(float) * 3);
        case 4:
            return a2fo_squadrons_call_thiscall_1(
                c.original, c.bar, reinterpret_cast<std::uintptr_t>(c.output));
        case 5: {
            struct Accumulation {Context* outer;void* member=nullptr;
                std::array<float,3> total{},colour{};float weight_sum=0.0f;
                std::uint32_t weight=0;};
            Accumulation state{&c};
            const auto numeric_step=+[](void* opaque,std::uint32_t code,std::uintptr_t index) noexcept -> std::uintptr_t {
                auto& n=*static_cast<Accumulation*>(opaque);auto& c=*n.outer;
                switch(code){
                case 0:return c.squad.count;
                case 1:
                    n.member=find_entity(c.squad.handles[index]);
                    return n.member && !object_expired(n.member) && object_team(n.member)==c.squad.team;
                case 5:n.weight=a2fo_rs_stage10_squadrons_weight(read_at<std::uint32_t>(n.member,0x1cc,0));return 1;
                case 2:
                    std::memcpy(static_cast<std::uint8_t*>(c.bar)+0x428,&n.member,sizeof(n.member));
                    n.colour={};return 1;
                case 3:a2fo_squadrons_call_thiscall_1(c.original,c.bar,
                    reinterpret_cast<std::uintptr_t>(n.colour.data()));return 1;
                case 4:a2fo_rs_stage10_squadrons_colour(n.total.data(),&n.weight_sum,n.colour.data(),n.weight);return 1;
                case 6:std::memcpy(static_cast<std::uint8_t*>(c.bar)+0x428,&c.craft,sizeof(c.craft));return 1;
                case 7:a2fo_rs_stage10_squadrons_divide(n.total.data(),&n.weight_sum);return 1;
                case 8:std::memcpy(c.output,n.total.data(),sizeof(n.total));return 1;
                default:return 0;
                }
            };
            a2fo_rs_stage10_squadrons_bar(&state,numeric_step,1);
            return reinterpret_cast<std::uintptr_t>(c.output);
        }
        default: return 0;
        }
    };
    return reinterpret_cast<void*>(a2fo_rs_stage8_squadrons_bar(
        step, &context, output));
}

void* panel_child(void* holder, std::size_t offset, void* panel) noexcept {
    void* child = read_at<void*>(holder, offset, nullptr);
    return read_at<void*>(child, 4, nullptr) == panel ? child : nullptr;
}

void* multi_tile(void* panel, std::size_t index) noexcept {
    // Fleet Ops replaces the original inline tile array with a pointer.
    void* array = read_at<void*>(panel, 0x148, nullptr);
    if (read_at<void*>(array, 4, nullptr) == panel)
        return panel_child(panel, 0x148 + index * sizeof(void*), panel);
    return panel_child(array, index * sizeof(void*), panel);
}

void draw_squad_badge(void* panel, void* craft, void* icon) noexcept {
    struct Context {
        void* panel;
        void* craft;
        void* icon;
        LiveSquad squad{};
        Rectangle rectangle{};
        void* text = nullptr;
        Context(void* source, void* member, void* badge_icon)
            : panel(source), craft(member), icon(badge_icon) {}
    } context{panel, craft, icon};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: c.squad = live_squad(c.craft); return c.squad.count != 0;
        case 1: return c.icon != nullptr;
        case 2:
            c.rectangle = read_at<Rectangle>(c.icon, 8, {});
            return 1;
        case 3:
            // Anchor to the icon's live lower-right layout.
            return a2fo_rs_stage8_squadrons_badge_bounds(
                &c.rectangle.left, &c.rectangle.top,
                c.rectangle.right, c.rectangle.bottom);
        case 4:
            c.text = panel_child(c.panel, 0x94, c.panel);
            return c.text && readable_range(c.text, 0x88);
        case 5: {
            void* display_slot = read_at<void*>(c.text, 0x28, nullptr);
            void* display_override = read_at<void*>(display_slot, 0, nullptr);
            const std::array<float, 3> colour{{1.0f, 1.0f, 1.0f}};
            char badge[24]{};
            std::snprintf(badge, sizeof(badge), "%lu/%lu",
                static_cast<unsigned long>(c.squad.count),
                static_cast<unsigned long>(c.squad.maximum));
            a2fo_squadrons_call_thiscall_7(at(kDrawTextRva), c.panel,
                reinterpret_cast<std::uintptr_t>(badge),
                reinterpret_cast<std::uintptr_t>(&c.rectangle), 9,
                reinterpret_cast<std::uintptr_t>(colour.data()),
                reinterpret_cast<std::uintptr_t>(display_override), 0,
                reinterpret_cast<std::uintptr_t>(
                    static_cast<std::uint8_t*>(c.text) + 0x7c));
            return 1;
        }
        default: return 0;
        }
    };
    a2fo_rs_stage8_squadrons_badge(step, &context);
}

void __attribute__((fastcall)) squad_panel_render_hook(
    void* panel, void*) noexcept {
    struct Context {
        void* panel;
        std::array<SquadBarOverride, 18> bars{};
        std::size_t count = 0;
        void* selected = nullptr;

        explicit Context(void* source) : panel(source) {}
        void bind(void* bar) noexcept {
            if (!bar || count == bars.size() || !writable_range(bar, 0x43c)) return;
            struct Scan {const SquadBarOverride* bars;std::size_t count;void* bar;};
            Scan scan{bars.data(),count,bar};
            const auto duplicate=+[](void* opaque,std::uint32_t op,std::uintptr_t index) noexcept -> std::uintptr_t {
                const auto& c=*static_cast<Scan*>(opaque);
                return op==0 ? c.count : static_cast<std::uintptr_t>(c.bars[index].bar==c.bar);
            };
            if(a2fo_rs_stage10_squadrons_scan(&scan,duplicate))return;
            void* vtable = read_at<void*>(bar, 0, nullptr);
            if (!readable_range(vtable, sizeof(bars[0].vtable)) ||
                !executable_address(read_at<void*>(vtable, 0x20, nullptr)) ||
                !executable_address(read_at<void*>(vtable, 0x24, nullptr))) return;
            auto& entry = bars[count++];
            entry.bar = bar;
            entry.original_vtable = vtable;
            std::memcpy(entry.vtable.data(), vtable, sizeof(entry.vtable));
            entry.vtable[8] = reinterpret_cast<void*>(&squad_bar_colour_hook);
            entry.vtable[9] = reinterpret_cast<void*>(&squad_bar_values_hook);
            void* replacement = entry.vtable.data();
            std::memcpy(bar, &replacement, sizeof(replacement));
        }
    } context{panel};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t value) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: ensure_logical_ui_hooks(); return 1;
        case 1: return g_active_bars || !g_runtime_ready;
        case 2:
            a2fo_squadrons_call_thiscall_0(g_ship_display_render_original, c.panel);
            return 1;
        case 3: c.bind(panel_child(c.panel, 0xa0, c.panel)); return 1;
        case 4: c.bind(panel_child(c.panel, 0x118, c.panel)); return 1;
        case 5: {
            void* tile = multi_tile(c.panel, value);
            void* shield = panel_child(tile, 0x3c, c.panel);
            if (!shield) shield = panel_child(tile, 0x2c, c.panel);
            c.bind(shield);
            return 1;
        }
        case 6:
            g_active_bars = c.bars.data();
            g_active_bar_count = c.count;
            return 1;
        case 7: return c.count;
        case 8:
            std::memcpy(c.bars[value].bar, &c.bars[value].original_vtable,
                        sizeof(void*));
            return 1;
        case 9:
            g_active_bars = nullptr;
            g_active_bar_count = 0;
            return 1;
        case 10:
            c.selected = read_at<void*>(c.panel, 0x1e8, nullptr);
            return c.selected != nullptr;
        case 11:
            draw_squad_badge(c.panel, c.selected,
                             panel_child(c.panel, 0xac, c.panel));
            return 1;
        case 12: {
            void* selection = read_at<void*>(at(kUiSelectionPointerRva),
                                             0, nullptr);
            return g_ui_selection_vtable ? logical_ui_selection(selection) : 0;
        }
        case 13: {
            void* tile = multi_tile(c.panel, value);
            void* craft = read_at<void*>(tile, 0x28, nullptr);
            draw_squad_badge(c.panel, craft,
                             panel_child(tile, 0x30, c.panel));
            return 1;
        }
        default: return 0;
        }
    };
    a2fo_rs_stage8_squadrons_render_panel(step, &context);
}

bool install_squad_panel_hook() noexcept {
    constexpr std::uint8_t text_signature[] = {0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};
    struct Context {
        const std::uint8_t* signature;
        std::uint8_t* slot = nullptr;
        void* original = nullptr;
        DWORD protection = 0;
    } context{text_signature, nullptr, nullptr, 0};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0:
            c.slot = static_cast<std::uint8_t*>(at(kShipDisplayVtableRva)) +
                kShipDisplayRenderSlot;
            return 1;
        case 1: c.original = read_at<void*>(c.slot, 0, nullptr); return 1;
        case 2: return executable_address(c.original);
        case 3: return g_armada &&
            readable_range(at(kDrawTextRva), 6) &&
            std::memcmp(at(kDrawTextRva), c.signature, 6) == 0;
        case 4: return VirtualProtect(c.slot, sizeof(void*),
                                      PAGE_READWRITE, &c.protection) != 0;
        case 5:
            g_ship_display_render_original = c.original;
            InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(c.slot),
                reinterpret_cast<void*>(&squad_panel_render_hook));
            return 1;
        case 6: {
            DWORD ignored = 0;
            VirtualProtect(c.slot, sizeof(void*), c.protection, &ignored);
            return 1;
        }
        case 7:
            log_line("Squad panel renderer installed: member badge and summed native health bars");
            return 1;
        default: return 0;
        }
    };
    return a2fo_rs_stage8_squadrons_install_panel(step, &context) != 0;
}

using SelectionAddFn = void (__stdcall*)(
    void*, void*, std::int32_t, std::int32_t, std::int32_t, std::int32_t);

#include "native_pod_selection.inl"

void __stdcall selection_add_hook(
    void* selection, void* craft, std::int32_t mode,
    std::int32_t clear_selection, std::int32_t play_sound, std::int32_t check_compatibility) noexcept {
    auto original = reinterpret_cast<SelectionAddFn>(
        g_selection_add_hook.gateway);
    if (!original) return;
    if (!g_runtime_ready || !selection || !craft) {
        original(selection, craft, mode, clear_selection, play_sound, check_compatibility);
        return;
    }
    try {
        struct Context {
            void* selection;
            void* craft;
            SelectionAddFn original;
            std::int32_t mode;
            std::int32_t clear_selection;
            std::int32_t play_sound;
            std::int32_t check_compatibility;
            std::int32_t operation = 0;
            std::int32_t team = -1;
            std::uint32_t handle = 0;
            std::uint32_t count = 0;
            bool pod_group = false;
            std::vector<std::uint32_t> peers;
            std::vector<void*> peer_craft;
            std::exception_ptr exception{};
            Context(void* selection_in, void* craft_in, SelectionAddFn original_in,
                    std::int32_t mode_in, std::int32_t clear_in,
                    std::int32_t sound_in, std::int32_t compatibility_in)
                : selection(selection_in), craft(craft_in), original(original_in),
                  mode(mode_in), clear_selection(clear_in), play_sound(sound_in),
                  check_compatibility(compatibility_in) {}
        } context{selection, craft, original, mode, clear_selection,
                  play_sound, check_compatibility};
        const auto step = +[](void* opaque, std::uint32_t op,
                              std::uintptr_t value) noexcept -> std::uintptr_t {
            auto& c = *static_cast<Context*>(opaque);
            try {
                switch (op) {
                case 0: return read_at<std::uint8_t>(
                    c.craft, kNativeSelectedFlagOffset, 0) != 0;
                case 1:
                    c.original(c.selection, c.craft, c.mode, c.clear_selection,
                               c.play_sound, c.check_compatibility);
                    return 1;
                case 2: c.handle = object_handle(c.craft); return 1;
                case 3: {
                    std::lock_guard<std::mutex> lock(g_registry_mutex);
                    const auto* squad = g_registry.containing(c.handle);
                    if (squad && squad->team ==
                            static_cast<a2fo::squadrons::TeamId>(object_team(c.craft))) {
                        c.peers.reserve(squad->live_count());
                        struct Snapshot {const a2fo::squadrons::Squadron* squad;
                            std::vector<std::uint32_t>* peers;std::uint32_t clicked;
                            std::exception_ptr error{};};
                        Snapshot snapshot{squad,&c.peers,c.handle};
                        const auto copy=+[](void* opaque,std::uint32_t op,std::uintptr_t index) noexcept -> std::uintptr_t {
                            auto& s=*static_cast<Snapshot*>(opaque);
                            try{
                                if(op==0)return s.squad->slots.size();
                                const auto member=s.squad->slots[index].member;
                                if(op==1)return member && member!=s.clicked;
                                s.peers->push_back(static_cast<std::uint32_t>(member));return 1;
                            }catch(...){s.error=std::current_exception();return UINTPTR_MAX;}
                        };
                        a2fo_rs_stage10_squadrons_snapshot(&snapshot,copy);
                        if(snapshot.error)std::rethrow_exception(snapshot.error);
                    }
                    return 1;
                }
                case 4: return c.peers.size();
                case 5:
                    c.peers = attached_yard_peers(c.craft, &find_entity);
                    c.pod_group = !c.peers.empty();
                    return 1;
                case 6: ensure_logical_ui_hooks(); return 1;
                case 7:
                    c.peer_craft.reserve(c.peers.size());
                    c.team = object_team(c.craft);
                    c.operation = static_cast<std::int32_t>(value);
                    return 1;
                case 8: {
                    const auto peer_handle = c.peers[value];
                    void* peer = find_entity(peer_handle);
                    if (!peer || object_expired(peer) ||
                        object_handle(peer) != peer_handle ||
                        object_team(peer) != c.team) {
                        log_line("Squad selection skipped: member identity became stale");
                        return 0;
                    }
                    c.peer_craft.push_back(peer);
                    return 1;
                }
                case 9: return read_at<std::uint32_t>(
                    c.selection, kNativeSelectionCountOffset,
                    kNativeSelectionLimit + 1);
                case 10: return readable_range(
                    static_cast<std::uint8_t*>(c.selection) +
                        kNativeSelectionHandlesOffset,
                    kNativeSelectionLimit * sizeof(std::uint32_t));
                case 11:
                    c.count = static_cast<std::uint32_t>(value);
                    [[fallthrough]];
                case 12: {
                    const auto candidate = op == 11 ? c.handle : c.peers[value];
                    struct Scan {void* selection;std::uint32_t count;std::uint32_t candidate;};
                    Scan scan{c.selection,c.count,static_cast<std::uint32_t>(candidate)};
                    const auto selected=+[](void* opaque,std::uint32_t op,std::uintptr_t index) noexcept -> std::uintptr_t {
                        const auto& s=*static_cast<Scan*>(opaque);
                        if(op==0)return s.count;
                        return read_at<std::uint32_t>(s.selection,kNativeSelectionHandlesOffset+
                            index*sizeof(std::uint32_t),0)==s.candidate;
                    };
                    return a2fo_rs_stage10_squadrons_scan(&scan,selected);
                }
                case 13:
                    log_line("Squad selection rejected: full group exceeds native 30-ship selection capacity");
                    return 1;
                case 14: return c.pod_group;
                case 15:
                    // Gateway argument four clears selection; only the first
                    // native click may clear. Validated peers suppress audio
                    // and mixed-selection compatibility checks.
                    c.original(c.selection, c.peer_craft[value >> 1],
                               static_cast<std::int32_t>(value & 1), 0, 0, 0);
                    return 1;
                case 16: ensure_logical_ui_hooks(); return 1;
                case 17: {
                    const auto selected_count = read_at<std::uint32_t>(
                        c.selection, kNativeSelectionCountOffset, 0);
                    char message[240]{};
                    std::snprintf(message, sizeof(message),
                        "%s selection %s: handle %lu with %lu peers; native count=%lu; clear=%ld compatibility=%ld",
                        c.pod_group ? "Attached yard" : "Squad",
                        c.operation == 0 ? "added" : "removed",
                        static_cast<unsigned long>(c.handle),
                        static_cast<unsigned long>(c.peer_craft.size()),
                        static_cast<unsigned long>(selected_count),
                        static_cast<long>(c.clear_selection),
                        static_cast<long>(c.check_compatibility));
                    log_line(message);
                    return 1;
                }
                default: return 0;
                }
            } catch (...) {
                c.exception = std::current_exception();
                return UINTPTR_MAX;
            }
        };
        a2fo_rs_stage8_squadrons_select(
            step, &context, mode, clear_selection, kNativeSelectionLimit);
        if (context.exception) std::rethrow_exception(context.exception);
    } catch (...) {
        log_line("Squad selection expansion failed before native dispatch");
        original(selection, craft, mode, clear_selection, play_sound, check_compatibility);
    }
}

bool decrement_active_technology(void* producer, void* target_class) noexcept {
    if (!g_fleet_ops) return false;
    const std::int32_t team = object_team(producer);
    void* project = read_at<void*>(target_class, kObjectClassProjectIdOffset,
                                   nullptr);
    const std::uint32_t project_id = read_at<std::uint32_t>(project, 0, 0);
    if (!a2fo_rs_stage10_squadrons_project(team,project_id)) {
        return false;
    }
    const auto* fleet_ops_bytes =
        reinterpret_cast<const std::uint8_t*>(g_fleet_ops);
    void* trees = read_at<void*>(
        fleet_ops_bytes + kTeamTechnologyTreesPointerRva, 0, nullptr);
    void* tree = read_at<void*>(trees, static_cast<std::size_t>(team) *
                               sizeof(void*), nullptr);
    void* items = read_at<void*>(tree, kTechnologyTreeItemsOffset, nullptr);
    void* item = read_at<void*>(items,
        static_cast<std::size_t>(project_id - 1) * sizeof(void*), nullptr);
    auto* active = item ? reinterpret_cast<std::int32_t*>(
        static_cast<std::uint8_t*>(item) + kTechnologyItemActiveBuildsOffset)
        : nullptr;
    if (!writable_range(active, sizeof(*active))) return false;
    if (a2fo_rs_stage10_squadrons_active(*active)) --*active;
    return true;
}

void dispatch_finished(void* producer, void* target_class) noexcept {
    if (!g_api || !A2FO_MODULE_API_HAS(g_api, dispatch_producer_event) ||
        !g_api->dispatch_producer_event) return;
    A2FO_ProducerEvent event{};
    event.struct_size = sizeof(event);
    event.kind = A2FO_PRODUCER_EVENT_FINISHED;
    event.producer = producer;
    event.target_class = target_class;
    g_api->dispatch_producer_event(&event);
}

#include "native_repair.inl"

std::uintptr_t __attribute__((fastcall)) starbase_finish_build_hook(
    void* starbase, void*) noexcept {
    struct Context {
        void* starbase;
        void* target_class = nullptr;
        std::unordered_map<void*, Definition>::iterator found{};
        std::uint32_t queue_id = 0;
    } context{starbase};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t value) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0:
            c.target_class = read_at<void*>(
                c.starbase, kProducerCurrentBuildClassOffset, nullptr);
            return 1;
        case 1: return g_runtime_ready;
        case 2:
            c.found = g_definitions.find(c.target_class);
            return c.found != g_definitions.end();
        case 3:
            c.queue_id = read_at<std::uint32_t>(
                c.starbase, kProducerCurrentQueueIdOffset);
            return 1;
        case 4:
            return g_starbase_finish_original
                ? a2fo_squadrons_call_thiscall_0(
                      g_starbase_finish_original, c.starbase)
                : 0;
        case 5:
            try {
                finish_reinforcement(
                    c.starbase, c.queue_id, reinterpret_cast<void*>(value));
            } catch (...) {
                log_line("Squad replacement completion callback failed");
            }
            return 1;
        case 6:
            try { return spawn_squadron(c.starbase, c.found->second); }
            catch (...) {
                log_line("Squadron outer completion caught a member-spawn exception");
                return 0;
            }
        case 7: return decrement_active_technology(
            c.starbase, c.target_class);
        case 8:
            log_line("Squadron completion could not update active technology count");
            return 1;
        case 9:
            a2fo_squadrons_call_thiscall_0(
                at(kProducerPopBuildQueueItemRva), c.starbase);
            return 1;
        case 10:
            *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(c.starbase) +
                kProducerCurrentBuildClassOffset) = nullptr;
            return 1;
        case 11:
            *reinterpret_cast<std::uint32_t*>(
                static_cast<std::uint8_t*>(c.starbase) +
                kProducerCurrentQueueIdOffset) = 0;
            return 1;
        case 12:
            *reinterpret_cast<std::uint32_t*>(
                static_cast<std::uint8_t*>(c.starbase) +
                kProducerLastBuiltHandleOffset) = 0;
            return 1;
        case 13: {
            void* vtable = read_at<void*>(c.starbase, 0, nullptr);
            void* stop_effect = read_at<void*>(
                vtable, kProducerStopConstructionEffectVtableOffset, nullptr);
            if (stop_effect)
                a2fo_squadrons_call_thiscall_0(stop_effect, c.starbase);
            return 1;
        }
        case 14:
            try { dispatch_finished(c.starbase, c.target_class); }
            catch (...) {
                log_line("Squadron finished-event dispatch raised an exception");
            }
            return 1;
        case 15:
            log_line(value
                ? "Squadron outer completion consumed after spawning members"
                : "Squadron outer completion consumed after failed spawn");
            return 1;
        default: return 0;
        }
    };
    return a2fo_rs_stage8_squadrons_finish(step, &context);
}

bool patch_finish_vtable(std::uintptr_t vtable_rva,
                         void*** installed_slot) noexcept {
    auto* vtable = reinterpret_cast<std::uint8_t*>(
        at(vtable_rva));
    auto* slot = reinterpret_cast<void**>(
        vtable + kStarbaseFinishBuildVtableOffset);
    if (!readable_range(slot, sizeof(*slot)) ||
        *slot != at(kStarbaseFinishBuildRva)) {
        log_line("Starbase FinishBuild vtable mismatch; squadron builds disabled");
        return false;
    }
    DWORD old_protect = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old_protect)) {
        return false;
    }
    void* original = InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(slot),
        reinterpret_cast<void*>(&starbase_finish_build_hook));
    if (original != at(kStarbaseFinishBuildRva)) {
        InterlockedExchangePointer(
            reinterpret_cast<PVOID volatile*>(slot), original);
    }
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(*slot), old_protect, &ignored);
    if (original != at(kStarbaseFinishBuildRva)) {
        log_line("Starbase FinishBuild vtable changed during installation");
        return false;
    }
    *installed_slot = slot;
    return true;
}

void restore_finish_vtable(void** slot) noexcept {
    if (!slot || !g_armada) return;
    DWORD old_protect = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old_protect)) {
        return;
    }
    InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(slot),
        at(kStarbaseFinishBuildRva));
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(*slot), old_protect, &ignored);
}

bool install_starbase_finish_hook() noexcept {
    const auto step = +[](void*, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        switch (op) {
        case 0: return patch_finish_vtable(
            kStarbaseVtableRva, &g_starbase_finish_slot);
        case 1: return patch_finish_vtable(
            kShipyardVtableRva, &g_shipyard_finish_slot);
        case 2: restore_finish_vtable(g_starbase_finish_slot); return 1;
        case 3: g_starbase_finish_slot = nullptr; return 1;
        case 4:
            g_starbase_finish_original = at(kStarbaseFinishBuildRva);
            return 1;
        default: return 0;
        }
    };
    return a2fo_rs_stage8_squadrons_install_finish(step, nullptr) != 0;
}

struct SquadronClassContext {
    const A2FO_GameObjectClassLoadedEvent* event;
    std::string source;
    std::string key;
    OdfFields fields;
    std::string label;
    a2fo::squadrons::DefinitionResult parsed;
    std::uint32_t member_count = 0;
    std::exception_ptr exception{};

    explicit SquadronClassContext(const A2FO_GameObjectClassLoadedEvent* value)
        : event(value) {}
};

std::uintptr_t squadron_class_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t) noexcept {
    auto& context = *static_cast<SquadronClassContext*>(opaque);
    const auto* event = context.event;
    try {
        switch (op) {
        case 0:
            return g_runtime_ready && event &&
                event->struct_size >= sizeof(A2FO_GameObjectClassLoadedEvent) &&
                event->object_class && event->source_odf.data;
        case 1:
            context.source.assign(event->source_odf.data, event->source_odf.size);
            context.key = a2fo::squadrons::odf_key(context.source);
            return 1;
        case 2: return !context.key.empty();
        case 3:
            context.fields = copy_fields(event);
            context.label = lower_ascii(field_value(context.fields, "classLabel"));
            return 1;
        case 4:
            capture_pod_group_class(event->object_class, context.label,
                                    context.fields);
            return 1;
        case 5: g_yard_build_items.erase(event->object_class); return 1;
        case 6: g_classes_by_odf[context.key] = event->object_class; return 1;
        case 7: return !context.label.empty();
        case 8:
            g_classlabels_by_odf[context.key] = context.label;
            return 1;
        case 9: capture_member_economics(event->object_class, context.fields); return 1;
        case 10: return context.label == "squadron";
        case 11:
            context.parsed = a2fo::squadrons::parse_definition(
                context.source, context.fields);
            return static_cast<bool>(context.parsed);
        case 12: g_definitions.erase(event->object_class); return 1;
        case 13:
            log_text("Rejected squadron ODF '" + context.key +
                     "': " + context.parsed.error);
            return 1;
        case 14: {
            Definition definition = std::move(*context.parsed.definition);
            context.member_count = definition.size();
            g_definitions[event->object_class] = std::move(definition);
            return 1;
        }
        case 15: {
            char message[320]{};
            std::snprintf(message, sizeof(message),
                "Registered squadron ODF '%s' with %lu configured member%s",
                context.key.c_str(),
                static_cast<unsigned long>(context.member_count),
                context.member_count == 1 ? "" : "s");
            log_line(message);
            return 1;
        }
        case 16: resolve_squad_economics(event->object_class); return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

void A2FO_CALL class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    SquadronClassContext context{event};
    a2fo_rs_stage8_squadrons_class(&squadron_class_step, &context);
    if (context.exception)
        log_line("Squadron class-loaded callback ignored an unexpected exception");
}

struct SquadronProducerContext {
    const A2FO_ProducerEvent* event;
    std::unordered_map<void*, Definition>::iterator found{};
    std::string problem;
    std::exception_ptr exception{};

    explicit SquadronProducerContext(const A2FO_ProducerEvent* value)
        : event(value) {}
};

std::uintptr_t squadron_producer_step(void* opaque, std::uint32_t op,
                                     std::uintptr_t) noexcept {
    auto& context = *static_cast<SquadronProducerContext*>(opaque);
    const auto* event = context.event;
    try {
        switch (op) {
        case 0:
            return g_runtime_ready && event &&
                event->struct_size >= sizeof(A2FO_ProducerEvent) &&
                event->producer && event->target_class;
        case 1:
            context.found = g_definitions.find(event->target_class);
            return context.found != g_definitions.end();
        case 2: return resolve_squad_economics(event->target_class);
        case 3: return context.found->second.size() > kNativeSelectionLimit;
        case 4:
            log_line("Rejected squadron build: composition exceeds native 30-ship selection capacity");
            return 1;
        case 5: return supported_producer(event->producer);
        case 6: {
            void* vtable = read_at<void*>(event->producer, 0, nullptr);
            void* finish = read_at<void*>(
                vtable, kStarbaseFinishBuildVtableOffset, nullptr);
            char message[240]{};
            std::snprintf(message, sizeof(message),
                "Rejected squadron build: producer has no safe outer "
                "completion hook (vtable=%p finish=%p)", vtable, finish);
            log_line(message);
            return 1;
        }
        case 7:
            context.problem = a2fo::squadrons::validate_definition(
                context.found->second, &resolve_member_class);
            return !context.problem.empty();
        case 8:
            log_text("Rejected build admission for squadron '" +
                     context.found->second.odf + "': " + context.problem);
            return 1;
        case 9: return !g_logged_cap_warning;
        case 10:
            g_logged_cap_warning = true;
            log_line("Phase-one warning: squadron queue admission does not yet reserve aggregate physical unit-cap slots");
            return 1;
        case 11:
            log_line("Unexpected inner squadron completion; outer boundary was bypassed");
            return 1;
        case 12: return event->kind;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

bool A2FO_CALL producer_event_handler(
    const A2FO_ProducerEvent* event, void*) {
    SquadronProducerContext context{event};
    const auto result = a2fo_rs_stage8_squadrons_producer(
        &squadron_producer_step, &context,
        A2FO_PRODUCER_EVENT_ADMIT,
        A2FO_PRODUCER_EVENT_STARTING_EFFECT,
        A2FO_PRODUCER_EVENT_FINISHING);
    if (context.exception || result == 2) {
        log_line("Squadron Producer callback caught an unexpected exception; squadron operation failed closed");
        return event->kind != A2FO_PRODUCER_EVENT_ADMIT &&
            event->kind != A2FO_PRODUCER_EVENT_STARTING_EFFECT;
    }
    return result != 0;
}

struct SquadronCraftContext {
    const A2FO_CraftEvent* event;
    std::uint32_t handle = 0;
    std::int32_t team = -1;
    SquadId squad_id = 0;
    a2fo::squadrons::Removal removal{};
    std::exception_ptr exception{};

    explicit SquadronCraftContext(const A2FO_CraftEvent* value) : event(value) {}
};

std::uintptr_t squadron_craft_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t) noexcept {
    auto& context = *static_cast<SquadronCraftContext*>(opaque);
    const auto* event = context.event;
    try {
        switch (op) {
        case 0:
            return g_runtime_ready && event &&
                event->struct_size >= sizeof(A2FO_CraftEvent) && event->craft;
        case 1:
            context.handle = object_handle(event->craft);
            return context.handle;
        case 2:
            try { track_pod_group_root(event->craft, event->kind); }
            catch (...) { log_line("Attached yard root tracking failed"); }
            return 1;
        case 3: return event->kind;
        case 4:
            try { update_pending_launches(event->craft, event->kind); }
            catch (...) {
                log_line("Sequential squad launch caught an unexpected native event error");
            }
            return 1;
        case 5: {
            std::lock_guard<std::mutex> lock(g_registry_mutex);
            const auto* squad = g_registry.containing(context.handle);
            context.squad_id = squad ? squad->id : 0;
            context.removal = g_registry.remove(context.handle);
            return 1;
        }
        case 6: return context.removal.removed;
        case 7: {
            char message[256]{};
            std::snprintf(message, sizeof(message),
                "Squad %llu member handle %lu removed%s",
                static_cast<unsigned long long>(context.squad_id),
                static_cast<unsigned long>(context.handle),
                context.removal.squad_retired ? "; squad retired" : "");
            log_line(message);
            return 1;
        }
        case 8:
            try { update_reinforcement(event->craft, event->kind, 0); }
            catch (...) { log_line("Squad reinforcement cleanup failed"); }
            return 1;
        case 9:
            try {
                update_reinforcement(
                    event->craft, event->kind, event->elapsed_seconds);
            } catch (...) { log_line("Squad reinforcement update failed"); }
            return 1;
        case 10:
            context.team = object_team(event->craft);
            return static_cast<std::uint32_t>(context.team);
        case 11: {
            std::lock_guard<std::mutex> lock(g_registry_mutex);
            const auto* squad = g_registry.containing(context.handle);
            if (!squad ||
                static_cast<a2fo::squadrons::TeamId>(context.team) == squad->team)
                return 0;
            context.squad_id = squad->id;
            context.removal = g_registry.change_owner(
                context.handle,
                static_cast<a2fo::squadrons::TeamId>(context.team));
            return 1;
        }
        case 12: {
            char message[300]{};
            std::snprintf(message, sizeof(message),
                "Captured squad %llu member handle %lu detached to team %ld%s",
                static_cast<unsigned long long>(context.squad_id),
                static_cast<unsigned long>(context.handle),
                static_cast<long>(context.team),
                context.removal.squad_retired ? "; original squad retired" : "");
            log_line(message);
            return 1;
        }
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

void A2FO_CALL craft_event_handler(
    const A2FO_CraftEvent* event, void*) {
    SquadronCraftContext context{event};
    a2fo_rs_stage8_squadrons_craft(
        &squadron_craft_step, &context,
        A2FO_CRAFT_EVENT_POST_LOAD, A2FO_CRAFT_EVENT_CLEANUP,
        A2FO_CRAFT_EVENT_SIMULATE_POST);
    if (context.exception) std::rethrow_exception(context.exception);
}

struct SquadronYardClassContext {
    const A2FO_GameObjectClassLoadedEvent* event;
    OdfFields fields;
    std::string item;
    std::exception_ptr exception{};

    explicit SquadronYardClassContext(const A2FO_GameObjectClassLoadedEvent* value)
        : event(value) {}
};

std::uintptr_t squadron_yard_class_step(void* opaque, std::uint32_t op,
                                       std::uintptr_t index) noexcept {
    auto& context = *static_cast<SquadronYardClassContext*>(opaque);
    const auto* event = context.event;
    try {
        switch (op) {
        case 0:
            return g_runtime_ready && event &&
                event->struct_size >= sizeof(*event) && event->object_class;
        case 1: context.fields = copy_fields(event); return 1;
        case 2: {
            const auto name = "buildItem" + std::to_string(index);
            context.item = a2fo::squadrons::odf_key(
                field_value(context.fields, name.c_str()));
            return 1;
        }
        case 3: return !context.item.empty();
        case 4:
            g_yard_build_items[event->object_class].insert(context.item);
            return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

void A2FO_CALL yard_class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    SquadronYardClassContext context{event};
    a2fo_rs_stage8_squadrons_yard_class(
        &squadron_yard_class_step, &context, 100);
    if (context.exception)
        log_line("Squad repair yard metadata could not be read");
}

struct SquadronInitContext {
    const A2FO_ModuleApi* api;
    std::optional<std::array<std::string, kRequiredFieldCount>> field_storage;
    std::size_t field_count = 0;
    std::array<const char*, kRequiredFieldCount> required_fields{};
    std::optional<std::array<std::string, 100>> yard_names;
    std::array<const char*, 100> yard_fields{};
    std::exception_ptr exception{};

    explicit SquadronInitContext(const A2FO_ModuleApi* value) : api(value) {}
};

std::uintptr_t squadron_init_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t argument) noexcept {
    auto& context = *static_cast<SquadronInitContext*>(opaque);
    const auto* api = context.api;
    try {
        switch (op) {
        case 0:
            return api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
                api->api_version == A2FO_MODULE_API_VERSION && api->log &&
                api->armada_module && api->fleetops_module &&
                api->register_classlabel_alias &&
                A2FO_MODULE_API_HAS(api, register_classlabel_odf_defaults) &&
                api->register_classlabel_odf_defaults &&
                A2FO_MODULE_API_HAS(api, register_game_object_class_loaded_handler) &&
                api->register_game_object_class_loaded_handler &&
                A2FO_MODULE_API_HAS(api, register_producer_event_handler) &&
                api->register_producer_event_handler &&
                A2FO_MODULE_API_HAS(api, register_craft_event_handler) &&
                api->register_craft_event_handler &&
                (api->capabilities & A2FO_CAP_CLASSLABEL_ODF_DEFAULTS) != 0 &&
                (api->capabilities & A2FO_CAP_GAME_OBJECT_CLASS_LOADED) != 0 &&
                (api->capabilities & A2FO_CAP_PRODUCER_EVENTS) != 0 &&
                (api->capabilities & A2FO_CAP_CRAFT_EVENTS) != 0;
        case 1:
            g_api = api;
            g_armada = static_cast<HMODULE>(api->armada_module());
            return 1;
        case 2:
            g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
            return 1;
        case 3: return g_armada != nullptr;
        case 4: return g_fleet_ops != nullptr;
        case 5: return preflight_signatures();
        case 6: return install_starbase_finish_hook();
        case 7:
            log_line("Native squadron construction or outer completion boundary unavailable; disabled");
            return 1;
        case 8: g_runtime_ready = true; return 1;
        case 9:
            g_pod_group_layout_ready = pod_group_layout_matches();
            return g_pod_group_layout_ready;
        case 10:
            log_line(argument
                ? "Attached yard group selection available for selectPodsTogether research stations"
                : "Attached yard group selection disabled: native pod layout signature mismatch");
            return 1;
        case 11: return install_repair_hooks();
        case 12:
            log_line(argument
                ? "Native squad reinforcement enabled: accepted repair and paid Producer jobs"
                : "Native squad reinforcement hooks unavailable; health repair remains native");
            return 1;
        case 13: return install_squad_panel_hook();
        case 14:
            log_line("Squad panel renderer unavailable: badge/combined health hooks not installed");
            return 1;
        case 15: {
            void* selection_add = reinterpret_cast<std::uint8_t*>(g_fleet_ops) +
                kFoSelectionAddRva;
            return api->install_inline_hook && api->install_inline_hook(
                selection_add, reinterpret_cast<void*>(&selection_add_hook),
                sizeof(kExpectedFoSelectionAdd), kExpectedFoSelectionAdd,
                &g_selection_add_hook);
        }
        case 16:
            log_line("Native group-selection admission unavailable; members still build and launch");
            return 1;
        case 17:
            context.field_storage.emplace();
            context.field_count = 0;
            return a2fo::squadrons::kMaxMemberRows;
        case 18:
            return api->register_classlabel_alias(
                kModuleName, "squadron", "craft");
        case 19:
            return api->register_classlabel_odf_defaults(
                kModuleName, "squadron", kSquadronDefaults.data(),
                static_cast<std::uint32_t>(kSquadronDefaults.size()));
        case 20:
            return api->register_game_object_class_loaded_handler(
                kModuleName, context.required_fields.data(),
                static_cast<std::uint32_t>(context.required_fields.size()),
                &class_loaded_handler, nullptr);
        case 22:
            return api->register_producer_event_handler(
                kModuleName, &producer_event_handler, nullptr);
        case 23:
            return A2FO_MODULE_API_HAS(api, register_craft_event_handler_masked) &&
                api->register_craft_event_handler_masked;
        case 24:
            return api->register_craft_event_handler_masked(
                kModuleName,
                A2FO_CRAFT_EVENT_MASK_SIMULATE_POST |
                    A2FO_CRAFT_EVENT_MASK_POST_LOAD |
                    A2FO_CRAFT_EVENT_MASK_CLEANUP,
                &craft_event_handler, nullptr);
        case 25:
            return api->register_craft_event_handler(
                kModuleName, &craft_event_handler, nullptr);
        case 26:
            log_line("Masked Craft events unavailable; using legacy shared Craft dispatcher");
            return 1;
        case 27: g_runtime_ready = false; return 1;
        case 28:
            log_line("Squadron semantic/native dispatcher registration failed");
            return 1;
        case 29:
            log_line(g_selection_add_hook.gateway
                ? "Squadron runtime initialized: yard output launch, group selection, UI projection on first squad selection, and registry lifecycle active"
                : "Squadron runtime initialized: yard output launch and registry lifecycle active; group selection unavailable");
            return 1;
        case 30: {
            static const char* const names[]{
                "classLabel", "squadReinforceAtYard", "selectPodsTogether"};
            (*context.field_storage)[context.field_count++] = names[argument];
            return 1;
        }
        case 31: {
            const auto row = static_cast<std::uint32_t>(argument / 2);
            (*context.field_storage)[context.field_count++] =
                (argument % 2 == 0 ? "squadMember" : "squadMemberCount") +
                std::to_string(row);
            return 1;
        }
        case 32:
            (*context.field_storage)[context.field_count++] =
                kAdditionalCostFields[argument];
            return 1;
        case 33:
            for (std::size_t index = 0; index < context.field_storage->size(); ++index)
                context.required_fields[index] = (*context.field_storage)[index].c_str();
            return 1;
        case 34: context.yard_names.emplace(); return 1;
        case 35:
            (*context.yard_names)[argument] =
                "buildItem" + std::to_string(argument);
            context.yard_fields[argument] =
                (*context.yard_names)[argument].c_str();
            return 1;
        case 36:
            return api->register_game_object_class_loaded_handler(
                kModuleName, context.yard_fields.data(), 64,
                &yard_class_loaded_handler, nullptr);
        case 37:
            return api->register_game_object_class_loaded_handler(
                kModuleName, context.yard_fields.data() + 64, 36,
                &yard_class_loaded_handler, nullptr);
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

struct SquadronCostsContext {
    void* klass;
    std::int32_t* costs;
    std::uint32_t count;
    const std::int32_t* native_costs = nullptr;
    std::exception_ptr exception{};

    SquadronCostsContext(void* value, std::int32_t* output,
                         std::uint32_t length)
        : klass(value), costs(output), count(length) {}
};

std::uintptr_t squadron_costs_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t) noexcept {
    auto& context = *static_cast<SquadronCostsContext*>(opaque);
    try {
        switch (op) {
        case 0: return g_runtime_ready;
        case 1: return context.costs && context.count == 4;
        case 2: {
            const auto found = g_squad_economics.find(context.klass);
            if (found == g_squad_economics.end()) return 0;
            context.native_costs = found->second.costs.data();
            return 1;
        }
        case 3:
            std::copy_n(context.native_costs + 6, 4, context.costs);
            return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    SquadronInitContext context{api};
    const bool result = a2fo_rs_stage8_squadrons_init(
        &squadron_init_step, &context) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return result;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FOSquadrons_GetAdditionalCosts(
    void* klass, std::int32_t* costs, std::uint32_t count) {
    SquadronCostsContext context{klass, costs, count};
    const bool result = a2fo_rs_stage8_squadrons_additional_costs(
        &squadron_costs_step, &context) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return result;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Shared core callbacks are process-lifetime registrations.  The module
    // intentionally remains passive rather than trying to unregister them.
}
