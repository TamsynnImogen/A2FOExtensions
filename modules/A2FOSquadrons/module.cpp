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
#include "squadron_config.hpp"
#include "squadron_state.hpp"
#include "api.hpp"
#include "../A2FOFeaturePack/refit_queue_bridge_api.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
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
    1 + 1 + (a2fo::squadrons::kMaxMemberRows * 2) + 4;

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
    // Raw GameObjectClass costs occupy six int32 fields at 0x80..0x94.
    // Verify the actual getters, not just unrelated class constructor bytes.
    const std::uintptr_t getters[]{0xce170, 0xce190, 0xce180, 0xce1b0, 0xce1a0, 0xce1c0};
    for (std::size_t i = 0; i < 6; ++i) {
        const std::uint8_t expected[]{0x8b, 0x81,
            static_cast<std::uint8_t>(0x80 + i * 4), 0, 0, 0, 0xc3};
        if (!signature_matches(getters[i], expected)) return false;
    }
    const std::uint8_t build_time[]{0x8b, 0x47, 0x68};
    if (!signature_matches(0xce29f, build_time)) return false;
    return signature_matches(kAiMissionGetCurrentRva,
                             kExpectedAiMissionGetCurrent) &&
        signature_matches(kGameObjectClassFindRva,
                          kExpectedGameObjectClassFind) &&
        signature_matches(kGameObjectClassConstructRva,
                          kExpectedGameObjectClassConstruct) &&
        signature_matches(kGameObjectClassGetOdfNameRva,
                          kExpectedGameObjectClassGetOdfName) &&
        signature_matches(kStarbaseFinishBuildRva,
                          kExpectedStarbaseFinishBuild) &&
        signature_matches(kEntityGetRva, kExpectedEntityGet) &&
        signature_matches(kProducerPopBuildQueueItemRva,
                          kExpectedProducerPopBuildQueueItem);
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
    for (char& character : value) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

std::string unquote_trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

OdfFields copy_fields(const A2FO_GameObjectClassLoadedEvent* event) {
    OdfFields fields;
    if (!event || !event->odf_fields) return fields;
    fields.reserve(event->odf_field_count);
    for (std::uint32_t index = 0; index < event->odf_field_count; ++index) {
        const auto& field = event->odf_fields[index];
        if (!field.name.data || (!field.value.data && field.value.size != 0)) {
            continue;
        }
        fields.emplace_back(
            std::string(field.name.data, field.name.size),
            std::string(field.value.data ? field.value.data : "",
                        field.value.size));
    }
    return fields;
}

std::string field_value(const OdfFields& fields, const char* name) {
    if (!name) return {};
    for (const auto& field : fields) {
        if (_stricmp(field.first.c_str(), name) == 0) {
            return unquote_trim(field.second);
        }
    }
    return {};
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

bool definitely_unsupported_label(const std::string& label) {
    static const std::unordered_set<std::string> labels{
        "background_obj", "sensor", "turret", "shipyard", "research",
        "researchstation", "starbase", "station", "planet", "asteroid",
        "nebula", "weapon", "ordnance", "projectile", "squadron",
    };
    return labels.count(label) != 0;
}

MemberClass resolve_member_class(const std::string& odf) {
    const std::string key = a2fo::squadrons::odf_key(odf);
    if (key.empty()) return MemberClass::missing;
    void* klass = nullptr;
    const auto cached = g_classes_by_odf.find(key);
    if (cached != g_classes_by_odf.end()) klass = cached->second;
    if (!klass) klass = find_class(key);
    if (!klass) return MemberClass::missing;
    if (g_definitions.count(klass)) return MemberClass::squadron;

    const auto label_it = g_classlabels_by_odf.find(key);
    const std::string label = label_it == g_classlabels_by_odf.end()
        ? std::string{} : label_it->second;
    if (label == "squadron") return MemberClass::squadron;
    if (definitely_unsupported_label(label)) return MemberClass::unsupported;

    // Phase one deliberately errs toward allowing an existing GameObjectClass
    // because Fleet Ops has numerous mod-defined Craft-derived labels.  The
    // actual completed object is still checked for a valid handle/team/ODF.
    // A later hardening pass can replace this with a native CraftClass type
    // predicate once that boundary is exposed by the shared SDK.
    if (!label.empty() && g_logged_loose_member_labels.insert(label).second) {
        log_text("Prototype member resolver accepted classLabel '" + label +
                 "' as Craft-capable; native CraftClass predicate is pending");
    }
    return MemberClass::mobile_craft;
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

bool spawn_squadron(void* producer, const Definition& definition) {
    if (!producer || !g_runtime_ready) return false;
    const std::int32_t team = object_team(producer);
    const std::uint32_t producer_handle = object_handle(producer);
    Matrix34 base{};
    void* add_method = output_queue_add_method(producer);
    if (team < 0 || producer_handle == 0 || !add_method ||
        !build_transform(producer, &base)) {
        log_line("Squadron completion rejected: yard identity/output queue/build transform unavailable");
        return false;
    }
    const std::string validation = a2fo::squadrons::validate_definition(
        definition, &resolve_member_class);
    if (!validation.empty()) {
        log_text("Squadron completion rejected for '" + definition.odf +
                 "': " + validation);
        return false;
    }
    PendingLaunch job;
    job.yard_handle = producer_handle;
    job.team = team;
    job.odfs.reserve(definition.size());
    for (const auto& row : definition.members) {
        for (std::uint32_t member_index = 0;
             member_index < row.count; ++member_index) {
            job.odfs.push_back(row.odf);
        }
    }
    if (job.odfs.empty()) return false;
    if (job.odfs.size() > 1)
        g_pending_launches.reserve(g_pending_launches.size() + 1);
    void* first = construct_member(
        job.odfs[0], base, team, producer_handle, 0);
    if (!first) {
        log_line("Squadron first output could not be constructed");
        return false;
    }
    UncommittedMember rollback{first};
    const auto first_handle = object_handle(first);
    const std::string first_odf = class_odf_name(object_class(first));
    if (!first_handle || object_team(first) != team ||
        first_odf != job.odfs[0]) {
        log_line("Squadron first output failed its publication identity check");
        return false;
    }
    a2fo::squadrons::Result<SquadId> created;
    {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        created = g_registry.create_launching(
            definition, static_cast<a2fo::squadrons::TeamId>(team),
            Craft{first_handle, static_cast<a2fo::squadrons::TeamId>(team),
                  first_odf}, &resolve_member_class);
    }
    if (!created) {
        log_text("Squadron '" + definition.odf +
                 "' initial membership failed: " + created.error);
        return false;
    }
    rollback.created_squad = *created.value;
    void* queue = read_at<void*>(
        producer, kShipyardBuildOutputQueueOffset, nullptr);
    a2fo_squadrons_call_thiscall_1(
        add_method, queue, reinterpret_cast<std::uintptr_t>(first));
    if (read_at<std::uint32_t>(first, kCraftQueueStageOffset, 0) == 0 ||
        read_at<std::uint32_t>(first, kCraftQueueOwnerOffset, 0) !=
            producer_handle) {
        log_line("Squadron first output did not enter the yard queue");
        return false;
    }
    job.squad = *created.value;
    job.current_handle = first_handle;
    job.next_slot = 1;
    if (job.next_slot < job.odfs.size()) {
        g_pending_launches.push_back(std::move(job));
    } else {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        g_registry.finish_launching(job.squad);
    }
    rollback.craft = nullptr;
    char message[240]{};
    std::snprintf(message, sizeof(message),
        "Squadron '%s' started sequential yard launch as squad %llu (%lu members)",
        definition.odf.c_str(),
        static_cast<unsigned long long>(*created.value),
        static_cast<unsigned long>(definition.size()));
    log_line(message);
    return true;
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

bool launch_next_member(PendingLaunch* job) {
    if (!job || job->next_slot >= job->odfs.size()) return false;
    void* yard = find_entity(job->yard_handle);
    if (!yard || object_expired(yard) || object_team(yard) != job->team)
        return false;
    void* add_method = output_queue_add_method(yard);
    Matrix34 transform{};
    if (!add_method || !build_transform(yard, &transform)) return false;
    const auto slot = job->next_slot;
    void* member = construct_member(job->odfs[slot], transform,
        job->team, job->yard_handle, slot);
    if (!member) return false;
    UncommittedMember rollback{member};
    const auto handle = object_handle(member);
    const auto odf = class_odf_name(object_class(member));
    if (!handle || object_team(member) != job->team ||
        odf != job->odfs[slot]) {
        return false;
    }
    bool joined = false;
    {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        joined = g_registry.add_launching(job->squad, slot,
            Craft{handle,
                static_cast<a2fo::squadrons::TeamId>(job->team), odf});
    }
    if (!joined) {
        return false;
    }
    void* queue = read_at<void*>(
        yard, kShipyardBuildOutputQueueOffset, nullptr);
    a2fo_squadrons_call_thiscall_1(
        add_method, queue, reinterpret_cast<std::uintptr_t>(member));
    if (read_at<std::uint32_t>(member, kCraftQueueStageOffset, 0) == 0 ||
        read_at<std::uint32_t>(member, kCraftQueueOwnerOffset, 0) !=
            job->yard_handle) {
        return false;
    }
    job->current_handle = handle;
    ++job->next_slot;
    rollback.craft = nullptr;
    char message[240]{};
    std::snprintf(message, sizeof(message),
        "Squad %llu output %lu/%lu '%s' entered yard queue after prior exit",
        static_cast<unsigned long long>(job->squad),
        static_cast<unsigned long>(job->next_slot),
        static_cast<unsigned long>(job->odfs.size()), odf.c_str());
    log_line(message);
    return true;
}

void update_pending_launches(void* craft, std::uint32_t kind) {
    const auto handle = object_handle(craft);
    if (!handle) return;
    for (std::size_t index = 0; index < g_pending_launches.size();) {
        auto& pending = g_pending_launches[index];
        if (kind == A2FO_CRAFT_EVENT_CLEANUP) {
            if (handle == pending.yard_handle) {
                PendingLaunch ended = std::move(pending);
                g_pending_launches.erase(g_pending_launches.begin() + index);
                finish_pending_launch(ended, "stopped because the yard was removed");
                continue;
            }
            if (handle == pending.current_handle) pending.current_handle = 0;
            ++index;
            continue;
        }
        if (kind != A2FO_CRAFT_EVENT_SIMULATE_POST ||
            (handle != pending.current_handle &&
             !(pending.current_handle == 0 &&
               handle == pending.yard_handle)) ||
            (pending.current_handle &&
             read_at<std::uint32_t>(craft, kCraftQueueStageOffset, 1) != 0)) {
            ++index;
            continue;
        }
        PendingLaunch advancing = std::move(pending);
        g_pending_launches.erase(g_pending_launches.begin() + index);
        if (advancing.next_slot == advancing.odfs.size()) {
            finish_pending_launch(advancing, "completed");
        } else {
            bool launched = false;
            try {
                launched = launch_next_member(&advancing);
            } catch (...) {
                log_line("Squad member construction raised an exception; ending this launch job");
            }
            if (launched) g_pending_launches.push_back(std::move(advancing));
            else finish_pending_launch(advancing, "stopped after an output failure");
        }
    }
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
    if (!g_ui_get_count_original || !g_ui_get_handles_original) return 0;
    const auto count = static_cast<std::uint32_t>(
        a2fo_squadrons_call_thiscall_0(g_ui_get_count_original, selection));
    const auto* handles = reinterpret_cast<const std::uint32_t*>(
        a2fo_squadrons_call_thiscall_0(
            g_ui_get_handles_original, selection));
    if (count > kNativeSelectionLimit ||
        (count && !readable_range(handles,
            count * sizeof(std::uint32_t)))) return 0;

    std::array<SquadId, kNativeSelectionLimit> seen{};
    std::uint32_t seen_count = 0;
    std::uint32_t logical_count = 0;
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto handle = handles[index];
        if (!handle) continue;
        const auto* squad = g_registry.containing(handle);
        if (squad) {
            bool duplicate = false;
            for (std::uint32_t prior = 0; prior < seen_count; ++prior) {
                if (seen[prior] == squad->id) { duplicate = true; break; }
            }
            if (duplicate) continue;
            seen[seen_count++] = squad->id;
        }
        g_logical_ui_handles[logical_count++] = handle;
    }
    return logical_count;
}

std::uintptr_t __attribute__((fastcall)) ui_get_count_hook(
    void* selection, void*) noexcept {
    const auto physical = a2fo_squadrons_call_thiscall_0(
        g_ui_get_count_original, selection);
    if (!g_runtime_ready || !ui_return_address(
            __builtin_return_address(0), kShipDisplayCountReturns))
        return physical;
    const auto logical = logical_ui_selection(selection);
    return physical && !logical ? physical : logical;
}

std::uintptr_t __attribute__((fastcall)) ui_get_handles_hook(
    void* selection, void*) noexcept {
    const auto physical = a2fo_squadrons_call_thiscall_0(
        g_ui_get_handles_original, selection);
    if (!g_runtime_ready || !ui_return_address(
            __builtin_return_address(0), kShipDisplayHandlesReturns))
        return physical;
    const auto logical = logical_ui_selection(selection);
    return logical ? reinterpret_cast<std::uintptr_t>(
        g_logical_ui_handles.data()) : physical;
}

void ensure_logical_ui_hooks() noexcept {
    std::lock_guard<std::mutex> lock(g_ui_hook_mutex);
    if (g_ui_selection_vtable) return;
    const auto failure = [](const char* reason) {
        if (g_ui_probe_failure != reason) {
            g_ui_probe_failure = reason;
            char message[220]{};
            std::snprintf(message, sizeof(message),
                "Squad selection panel projection unavailable: %s", reason);
            log_line(message);
        }
    };
    if (!signature_matches(0x000f2ef5, kExpectedUiCountCall) ||
        !signature_matches(0x000f2ca0, kExpectedUiCountCall) ||
        !signature_matches(0x000f2d27, kExpectedUiCountCall) ||
        !signature_matches(0x000f2f1a, kExpectedUiHandlesCall) ||
        !signature_matches(0x000f2cb2, kExpectedUiHandlesCall) ||
        !signature_matches(0x000f2d39, kExpectedUiHandlesCall)) {
        failure("ShipDisplay call-site signature mismatch");
        return;
    }
    void* selection = read_at<void*>(at(kUiSelectionPointerRva), 0, nullptr);
    if (!selection) { failure("Armada selection singleton unavailable"); return; }
    void* vtable = read_at<void*>(selection, 0, nullptr);
    if (!vtable) { failure("selection vtable unavailable"); return; }
    auto* count_slot = reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(vtable) +
        kSelectionGetCountVtableOffset);
    auto* handles_slot = reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(vtable) +
        kSelectionGetHandlesVtableOffset);
    if (!readable_range(count_slot, 2 * sizeof(void*))) {
        failure("selection getters unreadable"); return;
    }
    void* count_method = *count_slot;
    void* handles_method = *handles_slot;
    if (!executable_address(count_method) ||
        !executable_address(handles_method)) {
        failure("selection getters are not executable"); return;
    }
    const auto native_count = static_cast<std::uint32_t>(
        a2fo_squadrons_call_thiscall_0(count_method, selection));
    const auto native_handles = a2fo_squadrons_call_thiscall_0(
        handles_method, selection);
    const auto selection_address = reinterpret_cast<std::uintptr_t>(selection);
    // Fleet Ops can relocate the backing array. The virtual getters are the
    // contract; requiring Armada's original inline-array address rejects
    // otherwise valid Fleet Ops selections.
    if (native_count > kNativeSelectionLimit || !native_handles ||
        (native_count && !readable_range(
            reinterpret_cast<void*>(native_handles),
            native_count * sizeof(std::uint32_t)))) {
        char message[220]{};
        std::snprintf(message, sizeof(message),
            "Squad UI getter mismatch: vtable=%p nativeCount=%lu rawCount=%lu handlesOffset=%ld",
            vtable, static_cast<unsigned long>(native_count),
            static_cast<unsigned long>(read_at<std::uint32_t>(
                selection, kNativeSelectionCountOffset, 0)),
            static_cast<long>(native_handles - selection_address));
        log_line(message);
        failure("selection getter layout mismatch");
        return;
    }
    DWORD old_protection = 0;
    if (!VirtualProtect(count_slot, 2 * sizeof(void*), PAGE_READWRITE,
                        &old_protection)) {
        failure("selection vtable write protection unavailable"); return;
    }
    g_ui_get_count_original = count_method;
    g_ui_get_handles_original = handles_method;
    InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(count_slot),
        reinterpret_cast<void*>(&ui_get_count_hook));
    InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(handles_slot),
        reinterpret_cast<void*>(&ui_get_handles_hook));
    DWORD ignored = 0;
    VirtualProtect(count_slot, 2 * sizeof(void*), old_protection, &ignored);
    g_ui_selection_vtable = vtable;
    log_line("Squadron selection panel projection installed: one tile per squad");
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
    LiveSquad result;
    const auto handle = object_handle(craft);
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    const auto* squad = g_registry.containing(handle);
    if (!squad || object_team(craft) != static_cast<std::int32_t>(squad->team))
        return result;
    result.team = static_cast<std::int32_t>(squad->team);
    result.maximum = squad->slots.size();
    for (const auto& slot : squad->slots) {
        if (slot.member && result.count < result.handles.size())
            result.handles[result.count++] = static_cast<std::uint32_t>(slot.member);
    }
    return result;
}

void* __attribute__((fastcall)) squad_bar_values_hook(
    void* bar, void*, void* output) noexcept {
    SquadBarOverride* binding = nullptr;
    for (std::size_t i = 0; i < g_active_bar_count; ++i) {
        if (g_active_bars[i].bar == bar) { binding = &g_active_bars[i]; break; }
    }
    if (!binding) return output;
    void* original = read_at<void*>(binding->original_vtable, 0x24, nullptr);
    void* craft = read_at<void*>(bar, 0x428, nullptr);
    const auto squad = live_squad(craft);
    if (!squad.count || !writable_range(output, sizeof(float) * 2)) {
        return reinterpret_cast<void*>(a2fo_squadrons_call_thiscall_1(
            original, bar, reinterpret_cast<std::uintptr_t>(output)));
    }
    std::array<float, 2> total{};
    for (std::size_t i = 0; i < squad.count; ++i) {
        void* member = find_entity(squad.handles[i]);
        if (!member || object_expired(member) ||
            object_team(member) != squad.team) continue;
        std::memcpy(static_cast<std::uint8_t*>(bar) + 0x428,
                    &member, sizeof(member));
        std::array<float, 2> values{};
        a2fo_squadrons_call_thiscall_1(original, bar,
            reinterpret_cast<std::uintptr_t>(values.data()));
        if (std::isfinite(values[0]) && std::isfinite(values[1]) &&
            values[1] > 0.0f) {
            total[0] += std::clamp(values[0], 0.0f, values[1]);
            total[1] += values[1];
        }
    }
    std::memcpy(static_cast<std::uint8_t*>(bar) + 0x428,
                &craft, sizeof(craft));
    std::memcpy(output, total.data(), sizeof(total));
    return output;
}

void* __attribute__((fastcall)) squad_bar_colour_hook(
    void* bar, void*, void* output) noexcept {
    SquadBarOverride* binding = nullptr;
    for (std::size_t i = 0; i < g_active_bar_count; ++i) {
        if (g_active_bars[i].bar == bar) { binding = &g_active_bars[i]; break; }
    }
    if (!binding) return output;
    void* original = read_at<void*>(binding->original_vtable, 0x20, nullptr);
    void* craft = read_at<void*>(bar, 0x428, nullptr);
    const auto squad = live_squad(craft);
    if (!squad.count || !writable_range(output, sizeof(float) * 3))
        return reinterpret_cast<void*>(a2fo_squadrons_call_thiscall_1(
            original, bar, reinterpret_cast<std::uintptr_t>(output)));
    // Armada's health bar uses hull for its length and shield fraction for
    // its colour. Preserve the native colour provider (including disabled
    // shields), but weight the members by shield capacity rather than letting
    // the selected representative determine the whole squad's shield tint.
    std::array<float, 3> total{};
    float weight_sum = 0.0f;
    for (std::size_t i = 0; i < squad.count; ++i) {
        void* member = find_entity(squad.handles[i]);
        if (!member || object_expired(member) || object_team(member) != squad.team)
            continue;
        float weight = read_at<float>(member, 0x1cc, 0.0f);
        if (!std::isfinite(weight) || weight <= 0.0f) weight = 1.0f;
        std::memcpy(static_cast<std::uint8_t*>(bar) + 0x428, &member, sizeof(member));
        std::array<float, 3> colour{};
        a2fo_squadrons_call_thiscall_1(original, bar,
            reinterpret_cast<std::uintptr_t>(colour.data()));
        if (std::all_of(colour.begin(), colour.end(),
                [](float value) { return std::isfinite(value); })) {
            for (std::size_t channel = 0; channel < total.size(); ++channel)
                total[channel] += colour[channel] * weight;
            weight_sum += weight;
        }
    }
    std::memcpy(static_cast<std::uint8_t*>(bar) + 0x428, &craft, sizeof(craft));
    if (weight_sum > 0.0f)
        for (auto& channel : total) channel /= weight_sum;
    std::memcpy(output, total.data(), sizeof(total));
    return output;
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
    const auto squad = live_squad(craft);
    if (!squad.count || !icon) return;
    auto rectangle = read_at<Rectangle>(icon, 8, {});
    if (rectangle.right <= rectangle.left || rectangle.bottom <= rectangle.top)
        return;
    // Anchor the badge to the icon's lower right using its live layout, so
    // alternate resolutions and the A1 compatibility layout follow it.
    rectangle.left = std::max(rectangle.left, rectangle.right - 42);
    rectangle.top = std::max(rectangle.top, rectangle.bottom - 18);
    void* text = panel_child(panel, 0x94, panel);
    if (!text || !readable_range(text, 0x88)) return;
    void* display_slot = read_at<void*>(text, 0x28, nullptr);
    void* display_override = read_at<void*>(display_slot, 0, nullptr);
    const std::array<float, 3> colour{{1.0f, 1.0f, 1.0f}};
    char badge[24]{};
    std::snprintf(badge, sizeof(badge), "%lu/%lu",
        static_cast<unsigned long>(squad.count),
        static_cast<unsigned long>(squad.maximum));
    a2fo_squadrons_call_thiscall_7(at(kDrawTextRva), panel,
        reinterpret_cast<std::uintptr_t>(badge),
        reinterpret_cast<std::uintptr_t>(&rectangle), 9,
        reinterpret_cast<std::uintptr_t>(colour.data()),
        reinterpret_cast<std::uintptr_t>(display_override), 0,
        reinterpret_cast<std::uintptr_t>(static_cast<std::uint8_t*>(text) + 0x7c));
}

void __attribute__((fastcall)) squad_panel_render_hook(
    void* panel, void*) noexcept {
    ensure_logical_ui_hooks();
    // Nested renders retain their own scope; never leave a stack vtable on a
    // UI object after returning to the engine.
    if (g_active_bars || !g_runtime_ready) {
        a2fo_squadrons_call_thiscall_0(g_ship_display_render_original, panel);
        return;
    }
    std::array<SquadBarOverride, 18> bars{};
    std::size_t count = 0;
    const auto bind = [&](void* bar) {
        if (!bar || count == bars.size() || !writable_range(bar, 0x43c)) return;
        for (std::size_t i = 0; i < count; ++i) if (bars[i].bar == bar) return;
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
    };
    bind(panel_child(panel, 0xa0, panel));
    bind(panel_child(panel, 0x118, panel));
    for (std::size_t i = 0; i < 16; ++i) {
        void* tile = multi_tile(panel, i);
        void* shield = panel_child(tile, 0x3c, panel);
        if (!shield) shield = panel_child(tile, 0x2c, panel);
        bind(shield);
    }
    g_active_bars = bars.data();
    g_active_bar_count = count;
    a2fo_squadrons_call_thiscall_0(g_ship_display_render_original, panel);
    for (std::size_t i = 0; i < count; ++i)
        std::memcpy(bars[i].bar, &bars[i].original_vtable, sizeof(void*));
    g_active_bars = nullptr;
    g_active_bar_count = 0;

    void* selected = read_at<void*>(panel, 0x1e8, nullptr);
    if (selected) {
        draw_squad_badge(panel, selected, panel_child(panel, 0xac, panel));
    } else {
        void* selection = read_at<void*>(at(kUiSelectionPointerRva), 0, nullptr);
        const auto logical = g_ui_selection_vtable ? logical_ui_selection(selection) : 0;
        for (std::size_t i = 0; i < std::min<std::size_t>(logical, 16); ++i) {
            void* tile = multi_tile(panel, i);
            void* craft = read_at<void*>(tile, 0x28, nullptr);
            draw_squad_badge(panel, craft, panel_child(tile, 0x30, panel));
        }
    }
}

bool install_squad_panel_hook() noexcept {
    auto* slot = static_cast<std::uint8_t*>(at(kShipDisplayVtableRva)) +
        kShipDisplayRenderSlot;
    void* original = read_at<void*>(slot, 0, nullptr);
    constexpr std::uint8_t text_signature[] = {0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};
    if (!executable_address(original) ||
        !signature_matches(kDrawTextRva, text_signature)) return false;
    DWORD protection = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) return false;
    g_ship_display_render_original = original;
    InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(slot),
        reinterpret_cast<void*>(&squad_panel_render_hook));
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), protection, &ignored);
    log_line("Squad panel renderer installed: member badge and summed native health bars");
    return true;
}

using SelectionAddFn = void (__stdcall*)(
    void*, void*, std::int32_t, std::int32_t, std::int32_t, std::int32_t);

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

    // Fleet Ops normalizes toggle mode 2 against Craft's native selected bit.
    // An explicit clear happens first in native code, making a subsequent
    // toggle additive. Otherwise remember the bit before native dispatch.
    const std::int32_t operation = mode == 2
        ? (!clear_selection && read_at<std::uint8_t>(craft, kNativeSelectedFlagOffset, 0)
            ? 1 : 0) : mode;
    if (operation != 0 && operation != 1) {
        original(selection, craft, mode, clear_selection, play_sound, check_compatibility);
        return;
    }

    const std::uint32_t handle = object_handle(craft);
    std::vector<std::uint32_t> peers;
    {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        const auto* squad = g_registry.containing(handle);
        if (squad && squad->team ==
                static_cast<a2fo::squadrons::TeamId>(object_team(craft))) {
            peers.reserve(squad->live_count());
            for (const auto& slot : squad->slots) {
                if (slot.member && slot.member != handle)
                    peers.push_back(static_cast<std::uint32_t>(slot.member));
            }
        }
    }
    ensure_logical_ui_hooks();
    if (peers.empty()) {
        original(selection, craft, mode, clear_selection, play_sound, check_compatibility);
        return;
    }
    // The native selection array is fixed at 30 handles in this build. Work
    // out the complete expansion before changing it, so one gesture cannot
    // leave a partially selected squad or overflow the command payload.
    std::vector<void*> peer_craft;
    peer_craft.reserve(peers.size());
    const auto team = object_team(craft);
    for (const std::uint32_t peer_handle : peers) {
        void* peer = find_entity(peer_handle);
        if (!peer || object_expired(peer) || object_handle(peer) != peer_handle ||
            object_team(peer) != team) {
            log_line("Squad selection skipped: member identity became stale");
            return;
        }
        peer_craft.push_back(peer);
    }
    if (operation == 0) {
        const std::uint32_t stored_count = read_at<std::uint32_t>(
            selection, kNativeSelectionCountOffset,
            kNativeSelectionLimit + 1);
        if (stored_count > kNativeSelectionLimit ||
            !readable_range(static_cast<std::uint8_t*>(selection) +
                kNativeSelectionHandlesOffset,
                kNativeSelectionLimit * sizeof(std::uint32_t))) {
            return;
        }
        const std::uint32_t count = clear_selection ? 0 : stored_count;
        std::uint32_t needed = 0;
        const auto contains = [&](std::uint32_t candidate) {
            for (std::uint32_t index = 0; index < count; ++index) {
                if (read_at<std::uint32_t>(selection,
                        kNativeSelectionHandlesOffset +
                            index * sizeof(std::uint32_t), 0) == candidate)
                    return true;
            }
            return false;
        };
        if (!contains(handle)) ++needed;
        for (const auto peer : peers) if (!contains(peer)) ++needed;
        if (needed > kNativeSelectionLimit - count) {
            log_line("Squad selection rejected: full group exceeds native 30-ship selection capacity");
            return;
        }
    }

    original(selection, craft, mode, clear_selection, play_sound, check_compatibility);
    for (void* peer : peer_craft) {
        // Call the native gateway directly to avoid recursive group expansion.
        // Fleet Ops tests argument FOUR ([ebp+0x14]) and clears through
        // selection vtable+0xa0 before adding a member. Only the original
        // click may clear. Argument six is the mixed-selection compatibility
        // gate, which is also disabled for validated squadmates; suppress
        // repeated selection audio on the peer calls.
        original(selection, peer, operation, 0, 0, 0);
    }
    ensure_logical_ui_hooks();
    const auto selected_count = read_at<std::uint32_t>(
        selection, kNativeSelectionCountOffset, 0);
    char message[240]{};
    std::snprintf(message, sizeof(message),
        "Squad selection %s: handle %lu with %lu squadmates; native count=%lu; clear=%ld compatibility=%ld",
        operation == 0 ? "added" : "removed",
        static_cast<unsigned long>(handle),
        static_cast<unsigned long>(peer_craft.size()),
        static_cast<unsigned long>(selected_count),
        static_cast<long>(clear_selection),
        static_cast<long>(check_compatibility));
    log_line(message);
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
    if (team < 0 || team > 63 || project_id == 0 || project_id > 8192) {
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
    if (*active > 0) --*active;
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
    void* target_class = read_at<void*>(
        starbase, kProducerCurrentBuildClassOffset, nullptr);
    const auto found = g_runtime_ready ? g_definitions.find(target_class)
                                       : g_definitions.end();
    if (found == g_definitions.end()) {
        const auto queue_id = read_at<std::uint32_t>(starbase, kProducerCurrentQueueIdOffset);
        const auto result = g_starbase_finish_original
            ? a2fo_squadrons_call_thiscall_0(
                  g_starbase_finish_original, starbase)
            : 0;
        try { finish_reinforcement(starbase, queue_id, reinterpret_cast<void*>(result)); }
        catch (...) { log_line("Squad replacement completion callback failed"); }
        return result;
    }

    // Even a failed member construction must retire the paid build job. It
    // cannot fall through to native completion, which would queue a dummy.
    bool spawned = false;
    try {
        spawned = spawn_squadron(starbase, found->second);
    } catch (...) {
        log_line("Squadron outer completion caught a member-spawn exception");
    }
    if (!decrement_active_technology(starbase, target_class)) {
        log_line("Squadron completion could not update active technology count");
    }
    a2fo_squadrons_call_thiscall_0(
        at(kProducerPopBuildQueueItemRva), starbase);
    *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(starbase) +
        kProducerCurrentBuildClassOffset) = nullptr;
    *reinterpret_cast<std::uint32_t*>(
        static_cast<std::uint8_t*>(starbase) +
        kProducerCurrentQueueIdOffset) = 0;
    *reinterpret_cast<std::uint32_t*>(
        static_cast<std::uint8_t*>(starbase) +
        kProducerLastBuiltHandleOffset) = 0;
    void* vtable = read_at<void*>(starbase, 0, nullptr);
    void* stop_effect = read_at<void*>(
        vtable, kProducerStopConstructionEffectVtableOffset, nullptr);
    if (stop_effect) {
        a2fo_squadrons_call_thiscall_0(stop_effect, starbase);
    }
    try {
        dispatch_finished(starbase, target_class);
    } catch (...) {
        log_line("Squadron finished-event dispatch raised an exception");
    }
    log_line(spawned
        ? "Squadron outer completion consumed after spawning members"
        : "Squadron outer completion consumed after failed spawn");
    return 0;
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
    if (!patch_finish_vtable(kStarbaseVtableRva,
                             &g_starbase_finish_slot)) return false;
    if (!patch_finish_vtable(kShipyardVtableRva,
                             &g_shipyard_finish_slot)) {
        restore_finish_vtable(g_starbase_finish_slot);
        g_starbase_finish_slot = nullptr;
        return false;
    }
    g_starbase_finish_original = at(kStarbaseFinishBuildRva);
    return true;
}

void A2FO_CALL class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    if (!g_runtime_ready || !event ||
        event->struct_size < sizeof(A2FO_GameObjectClassLoadedEvent) ||
        !event->object_class || !event->source_odf.data) {
        return;
    }
    try {
        const std::string source(
            event->source_odf.data, event->source_odf.size);
        const std::string key = a2fo::squadrons::odf_key(source);
        if (key.empty()) return;
        const OdfFields fields = copy_fields(event);
        const std::string label = lower_ascii(field_value(fields, "classLabel"));
        g_yard_build_items.erase(event->object_class);
        g_classes_by_odf[key] = event->object_class;
        if (!label.empty()) g_classlabels_by_odf[key] = label;
        capture_member_economics(event->object_class, fields);
        if (label != "squadron") return;

        auto parsed = a2fo::squadrons::parse_definition(source, fields);
        if (!parsed) {
            g_definitions.erase(event->object_class);
            log_text("Rejected squadron ODF '" + key + "': " + parsed.error);
            return;
        }
        Definition definition = std::move(*parsed.definition);
        const std::uint32_t member_count = definition.size();
        g_definitions[event->object_class] = std::move(definition);
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "Registered squadron ODF '%s' with %lu configured member%s",
            key.c_str(), static_cast<unsigned long>(member_count),
            member_count == 1 ? "" : "s");
        log_line(message);
        resolve_squad_economics(event->object_class);
    } catch (...) {
        log_line("Squadron class-loaded callback ignored an unexpected exception");
    }
}

bool A2FO_CALL producer_event_handler(
    const A2FO_ProducerEvent* event, void*) {
    if (!g_runtime_ready || !event ||
        event->struct_size < sizeof(A2FO_ProducerEvent) ||
        !event->producer || !event->target_class) {
        return true;
    }
    const auto found = g_definitions.find(event->target_class);
    if (found == g_definitions.end()) return true;
    try {
        const Definition& definition = found->second;
        switch (event->kind) {
            case A2FO_PRODUCER_EVENT_ADMIT: {
                if (!resolve_squad_economics(event->target_class)) return false;
                if (definition.size() > kNativeSelectionLimit) {
                    log_line("Rejected squadron build: composition exceeds native 30-ship selection capacity");
                    return false;
                }
                if (!supported_producer(event->producer)) {
                    void* vtable = read_at<void*>(
                        event->producer, 0, nullptr);
                    void* finish = read_at<void*>(
                        vtable, kStarbaseFinishBuildVtableOffset, nullptr);
                    char message[240]{};
                    std::snprintf(message, sizeof(message),
                        "Rejected squadron build: producer has no safe outer "
                        "completion hook (vtable=%p finish=%p)",
                        vtable, finish);
                    log_line(message);
                    return false;
                }
                const std::string problem = a2fo::squadrons::validate_definition(
                    definition, &resolve_member_class);
                if (!problem.empty()) {
                    log_text("Rejected build admission for squadron '" +
                             definition.odf + "': " + problem);
                    return false;
                }
                if (!g_logged_cap_warning) {
                    g_logged_cap_warning = true;
                    log_line("Phase-one warning: squadron queue admission does not yet reserve aggregate physical unit-cap slots");
                }
                return true;
            }
            case A2FO_PRODUCER_EVENT_STARTING_EFFECT:
                // The squadron descriptor is an abstract build item.  Never
                // ask the renderer to construct a cosmetic instance for its
                // aliased placeholder CraftClass.
                return false;
            case A2FO_PRODUCER_EVENT_FINISHING: {
                // Starbase::FinishBuild consumes supported squadron jobs
                // before this inner Producer callback can run. Any arrival
                // here is an unsupported producer and must not return null
                // into its outer native completion path.
                log_line("Unexpected inner squadron completion; outer boundary was bypassed");
                return true;
            }
            case A2FO_PRODUCER_EVENT_FINISHED:
                return true;
            default:
                return true;
        }
    } catch (...) {
        log_line("Squadron Producer callback caught an unexpected exception; squadron operation failed closed");
        // The outer Starbase hook owns completion. Reject admission/effects,
        // but never make an unexpected inner FINISHING return null to its
        // native caller.
        return event->kind != A2FO_PRODUCER_EVENT_ADMIT &&
            event->kind != A2FO_PRODUCER_EVENT_STARTING_EFFECT;
    }
}

void A2FO_CALL craft_event_handler(
    const A2FO_CraftEvent* event, void*) {
    if (!g_runtime_ready || !event ||
        event->struct_size < sizeof(A2FO_CraftEvent) || !event->craft) {
        return;
    }
    const std::uint32_t handle = object_handle(event->craft);
    if (handle == 0) return;
    const auto object_id = static_cast<a2fo::squadrons::ObjectId>(handle);
    try {
        update_pending_launches(event->craft, event->kind);
    } catch (...) {
        log_line("Sequential squad launch caught an unexpected native event error");
    }

    if (event->kind == A2FO_CRAFT_EVENT_CLEANUP) {
        SquadId squad_id = 0;
        a2fo::squadrons::Removal removal;
        {
            std::lock_guard<std::mutex> lock(g_registry_mutex);
            const auto* squad = g_registry.containing(object_id);
            squad_id = squad ? squad->id : 0;
            removal = g_registry.remove(object_id);
        }
        if (removal.removed) {
            char message[256]{};
            std::snprintf(
                message, sizeof(message),
                "Squad %llu member handle %lu removed%s",
                static_cast<unsigned long long>(squad_id),
                static_cast<unsigned long>(handle),
                removal.squad_retired ? "; squad retired" : "");
            log_line(message);
        }
        try { update_reinforcement(event->craft, event->kind, 0); }
        catch (...) { log_line("Squad reinforcement cleanup failed"); }
        return;
    }

    if (event->kind != A2FO_CRAFT_EVENT_SIMULATE_POST) return;
    try { update_reinforcement(event->craft, event->kind, event->elapsed_seconds); }
    catch (...) { log_line("Squad reinforcement update failed"); }
    const std::int32_t team = object_team(event->craft);
    if (team < 0) return;
    SquadId squad_id = 0;
    a2fo::squadrons::Removal removal;
    {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        const auto* squad = g_registry.containing(object_id);
        if (!squad ||
            static_cast<a2fo::squadrons::TeamId>(team) == squad->team) return;
        squad_id = squad->id;
        removal = g_registry.change_owner(
            object_id, static_cast<a2fo::squadrons::TeamId>(team));
    }
    if (removal.removed) {
        char message[300]{};
        std::snprintf(
            message, sizeof(message),
            "Captured squad %llu member handle %lu detached to team %ld%s",
            static_cast<unsigned long long>(squad_id),
            static_cast<unsigned long>(handle), static_cast<long>(team),
            removal.squad_retired ? "; original squad retired" : "");
        log_line(message);
    }
}

std::array<std::string, kRequiredFieldCount> required_field_storage() {
    std::array<std::string, kRequiredFieldCount> fields{};
    std::size_t index = 0;
    fields[index++] = "classLabel";
    fields[index++] = "squadReinforceAtYard";
    for (std::uint32_t row = 0; row < a2fo::squadrons::kMaxMemberRows; ++row) {
        fields[index++] = "squadMember" + std::to_string(row);
        fields[index++] = "squadMemberCount" + std::to_string(row);
    }
    for (const auto* cost : kAdditionalCostFields) fields[index++] = cost;
    return fields;
}

void A2FO_CALL yard_class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    if (!g_runtime_ready || !event || event->struct_size < sizeof(*event) ||
        !event->object_class) return;
    try {
        const auto fields = copy_fields(event);
        for (unsigned index = 0; index < 100; ++index) {
            const auto name = "buildItem" + std::to_string(index);
            const auto item = a2fo::squadrons::odf_key(field_value(fields, name.c_str()));
            if (!item.empty()) g_yard_build_items[event->object_class].insert(item);
        }
    } catch (...) { log_line("Squad repair yard metadata could not be read"); }
}

bool register_yard_fields(const A2FO_ModuleApi* api) {
    // The core permits at most 64 requested ODF fields per registration.
    // Main class registration runs first and clears stale class metadata;
    // these two following callbacks merge the inherited build-list fields.
    std::array<std::string, 100> names;
    std::array<const char*, 100> fields{};
    for (unsigned i = 0; i < fields.size(); ++i) {
        names[i] = "buildItem" + std::to_string(i);
        fields[i] = names[i].c_str();
    }
    return api->register_game_object_class_loaded_handler(kModuleName,
        fields.data(), 64, &yard_class_loaded_handler, nullptr) &&
        api->register_game_object_class_loaded_handler(kModuleName,
        fields.data() + 64, 36, &yard_class_loaded_handler, nullptr);
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->register_classlabel_alias ||
        !A2FO_MODULE_API_HAS(api, register_classlabel_odf_defaults) ||
        !api->register_classlabel_odf_defaults ||
        !A2FO_MODULE_API_HAS(api, register_game_object_class_loaded_handler) ||
        !api->register_game_object_class_loaded_handler ||
        !A2FO_MODULE_API_HAS(api, register_producer_event_handler) ||
        !api->register_producer_event_handler ||
        !A2FO_MODULE_API_HAS(api, register_craft_event_handler) ||
        !api->register_craft_event_handler ||
        (api->capabilities & A2FO_CAP_CLASSLABEL_ODF_DEFAULTS) == 0 ||
        (api->capabilities & A2FO_CAP_GAME_OBJECT_CLASS_LOADED) == 0 ||
        (api->capabilities & A2FO_CAP_PRODUCER_EVENTS) == 0 ||
        (api->capabilities & A2FO_CAP_CRAFT_EVENTS) == 0) {
        return false;
    }

    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !g_fleet_ops || !preflight_signatures() ||
        !install_starbase_finish_hook()) {
        log_line("Native squadron construction or outer completion boundary unavailable; disabled");
        return false;
    }

    // Mark runtime active before class loading begins: registering the class
    // callback can immediately encounter classes as the shared dispatcher
    // comes online after all modules finish startup.
    g_runtime_ready = true;
    log_line(install_repair_hooks()
        ? "Native squad reinforcement enabled: accepted repair and paid Producer jobs"
        : "Native squad reinforcement hooks unavailable; health repair remains native");
    if (!install_squad_panel_hook())
        log_line("Squad panel renderer unavailable: badge/combined health hooks not installed");

    void* selection_add = reinterpret_cast<std::uint8_t*>(g_fleet_ops) +
        kFoSelectionAddRva;
    if (!api->install_inline_hook ||
        !api->install_inline_hook(
            selection_add, reinterpret_cast<void*>(&selection_add_hook),
            sizeof(kExpectedFoSelectionAdd), kExpectedFoSelectionAdd,
            &g_selection_add_hook)) {
        log_line("Native group-selection admission unavailable; members still build and launch");
    }

    const auto field_storage = required_field_storage();
    std::array<const char*, kRequiredFieldCount> required_fields{};
    for (std::size_t index = 0; index < field_storage.size(); ++index) {
        required_fields[index] = field_storage[index].c_str();
    }

    const bool alias_registered = api->register_classlabel_alias(
        kModuleName, "squadron", "craft");
    const bool defaults_registered = api->register_classlabel_odf_defaults(
        kModuleName, "squadron", kSquadronDefaults.data(),
        static_cast<std::uint32_t>(kSquadronDefaults.size()));
    const bool class_registered = api->register_game_object_class_loaded_handler(
        kModuleName, required_fields.data(),
        static_cast<std::uint32_t>(required_fields.size()),
        &class_loaded_handler, nullptr);
    const bool yards_registered = register_yard_fields(api);
    const bool producer_registered = api->register_producer_event_handler(
        kModuleName, &producer_event_handler, nullptr);
    bool craft_registered = false;
    if (A2FO_MODULE_API_HAS(api, register_craft_event_handler_masked) &&
        api->register_craft_event_handler_masked) {
        craft_registered = api->register_craft_event_handler_masked(
            kModuleName,
            A2FO_CRAFT_EVENT_MASK_SIMULATE_POST |
                A2FO_CRAFT_EVENT_MASK_CLEANUP,
            &craft_event_handler, nullptr);
    } else {
        // Older cores expose the original unmasked craft dispatcher.  It is
        // perfectly adequate here because craft_event_handler ignores event
        // kinds other than SIMULATE_POST and CLEANUP itself.
        craft_registered = api->register_craft_event_handler(
            kModuleName, &craft_event_handler, nullptr);
        if (craft_registered) {
            log_line("Masked Craft events unavailable; using legacy shared Craft dispatcher");
        }
    }

    if (!alias_registered || !defaults_registered || !class_registered || !yards_registered ||
        !producer_registered || !craft_registered) {
        g_runtime_ready = false;
        log_line("Squadron semantic/native dispatcher registration failed");
        // Some callbacks may already be retained by the core.  Keep the DLL
        // resident and leave every callback as a fail-open no-op.
        return true;
    }

    log_line(g_selection_add_hook.gateway
        ? "Squadron runtime initialized: yard output launch, group selection, UI projection on first squad selection, and registry lifecycle active"
        : "Squadron runtime initialized: yard output launch and registry lifecycle active; group selection unavailable");
    return true;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FOSquadrons_GetAdditionalCosts(
    void* klass, std::int32_t* costs, std::uint32_t count) {
    if (!g_runtime_ready || !costs || count != 4) return false;
    const auto found = g_squad_economics.find(klass);
    if (found == g_squad_economics.end()) return false;
    std::copy_n(found->second.costs.data() + 6, 4, costs);
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Shared core callbacks are process-lifetime registrations.  The module
    // intentionally remains passive rather than trying to unregister them.
}
