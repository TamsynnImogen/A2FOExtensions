/* Ownership-aware ODF variants for Armada II / Fleet Operations Roots. */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_module_provider.hpp"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif
#include "../../sdk/include/a2fo_faction_suffix.hpp"
#include "../../rust/module_runtime_b6_ffi.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

extern "C" {
std::uintptr_t __cdecl a2fo_odf_variants_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_odf_variants_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
void* __cdecl a2fo_odf_variants_construct(
    void* function, void* self, const void* transform,
    std::int32_t team, std::int32_t unknown, void* parent);
}

namespace {

constexpr char kModuleName[] = "A2FOODFVariants";
constexpr char kFactionNameCommand[] = "name";
constexpr char kBorgDefaultSuffix[] = "_b";
constexpr char kNebulaRendererModuleName[] = "A2FONebulaRenderer.dll";
constexpr char kNebulaClassObserverExport[] =
    "A2FONebulaRenderer_RegisterClass";

// ArmadaL.exe 1.1 / supported Fleet Operations Roots RVAs. These are the
// same native boundaries used by the previously tested core prototype.
constexpr std::uintptr_t kAiMissionGetCurrentRva = 0x00001370;
constexpr std::uintptr_t kGameObjectClassFindRva = 0x000cd370;
constexpr std::uintptr_t kGameObjectClassConstructRva = 0x000cd390;
constexpr std::uintptr_t kGameObjectClassGetOdfNameRva = 0x000ce370;
constexpr std::uintptr_t kGameObjectGetTransformRva = 0x000cfd50;
constexpr std::uintptr_t kCraftDoExpireRva = 0x000caae0;
constexpr std::uintptr_t kEvolverSwapObjectsRva = 0x000b0e10;

const std::array<std::uint8_t, 6> kExpectedAiMissionGetCurrent{{
    0xa1, 0xdc, 0x47, 0x73, 0x00, 0xc3}};
const std::array<std::uint8_t, 6> kExpectedGameObjectClassFind{{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08}};
const std::array<std::uint8_t, 9> kExpectedGameObjectClassConstruct{{
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x84, 0x00, 0x00, 0x00}};
const std::array<std::uint8_t, 11> kExpectedGameObjectClassGetOdfName{{
    0x8b, 0x89, 0xcc, 0x01, 0x00, 0x00,
    0xe9, 0x25, 0xb0, 0x18, 0x00}};
const std::array<std::uint8_t, 7> kExpectedGameObjectGetTransform{{
    0x8b, 0x41, 0x04, 0x83, 0xc0, 0x44, 0xc3}};
const std::array<std::uint8_t, 6> kExpectedEvolverSwapObjects{{
    0x55, 0x8b, 0xec, 0x53, 0x56, 0x57}};

constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kObjectTeamOffset = 0xec;
constexpr std::size_t kObjectRaceOffset = 0xfc;
constexpr std::size_t kObjectVelocityOffset = 0xdc;
constexpr std::size_t kCurrentHealthOffset = 0x15c;
constexpr std::size_t kMaximumHealthOffset = 0x160;
constexpr std::size_t kMaximumSpecialEnergyOffset = 0x168;
constexpr std::size_t kCurrentSpecialEnergyOffset = 0x16c;
constexpr std::size_t kMaximumCrewOffset = 0x1c4;
constexpr std::size_t kCurrentShieldsOffset = 0x1c8;
constexpr std::size_t kMaximumShieldsOffset = 0x1cc;
constexpr std::size_t kCurrentCrewOffset = 0x1dc;
constexpr std::size_t kSystemsOffset = 0x1e0;
constexpr std::size_t kSystemRecordSize = 0x30;
constexpr std::size_t kSystemMaximumOffset = 0x04;
constexpr std::size_t kSystemOperationalOffset = 0x00;
constexpr std::size_t kSystemForcedDisabledOffset = 0x01;
constexpr std::size_t kSystemCurrentOffset = 0x18;
constexpr std::size_t kSystemDisableTimeOffset = 0x28;
constexpr std::size_t kSystemCount = 5;

struct CraftStateSnapshot {
    float hull_fraction = 1.0f;
    bool hull_valid = false;
    float shield_fraction = 1.0f;
    bool shield_valid = false;
    float special_energy_fraction = 1.0f;
    bool special_energy_valid = false;
    float current_crew = 0.0f;
    bool crew_valid = false;
    std::array<float, 3> velocity{};
    bool velocity_valid = false;
    std::array<double, kSystemCount> system_fractions{};
    std::array<std::uint8_t, kSystemCount> system_operational{};
    std::array<std::uint8_t, kSystemCount> system_forced_disabled{};
    std::array<float, kSystemCount> system_disable_times{};
    std::array<bool, kSystemCount> system_valid{};
};

using NebulaClassObserver = void (A2FO_CALL*)(void* object_class,
                                              void* parameter_db);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
bool g_runtime_alive = false;
std::unordered_map<void*, std::string> g_race_suffixes;
std::unordered_map<void*, void*> g_last_races;
std::unordered_map<void*, std::string> g_base_odfs;
std::unordered_set<std::string> g_missing_variants_logged;
std::unordered_set<void*> g_pending_renderer_refresh_crafts;
NebulaClassObserver g_nebula_class_observer = nullptr;

std::uint64_t runtime_gate_op(void*, std::uint32_t operation,
                              std::uint32_t) noexcept {
    if (operation == 0) g_pending_renderer_refresh_crafts.clear();
    return 0;
}
bool runtime_enabled() noexcept {
#ifdef A2FO_BUILTIN_FEATURE
    static std::uint64_t activation = 0;
    return a2fo_rs_odf_enabled(
        a2fo::builtin_module_activation(A2FO_BUILTIN_FEATURE),
        &activation, nullptr, &runtime_gate_op) != 0;
#else
    return g_runtime_alive;
#endif
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

void log_line(const std::string& message) noexcept {
    log_line(message.c_str());
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::array<std::uint8_t, Size>& expected) noexcept {
    return module &&
        std::memcmp(at(module, rva), expected.data(), expected.size()) == 0;
}

char lower_ascii(char value) noexcept {
    return static_cast<char>(
        std::tolower(static_cast<unsigned char>(value)));
}

bool equal_ascii_case_insensitive(std::string_view left,
                                  std::string_view right) noexcept {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (lower_ascii(left[index]) != lower_ascii(right[index])) return false;
    }
    return true;
}

bool ends_with_case_insensitive(std::string_view value,
                                std::string_view ending) noexcept {
    if (ending.empty() || ending.size() > value.size()) return false;
    return equal_ascii_case_insensitive(
        value.substr(value.size() - ending.size()), ending);
}

std::string normalize_odf_basename(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n\"");
    if (first == std::string::npos) return {};
    const std::size_t last = value.find_last_not_of(" \t\r\n\"");
    value = value.substr(first, last - first + 1);
    std::replace(value.begin(), value.end(), '/', '\\');
    const std::size_t slash = value.find_last_of('\\');
    if (slash != std::string::npos) value.erase(0, slash + 1);
    std::transform(value.begin(), value.end(), value.begin(), lower_ascii);
    if (value.size() > 4 &&
        value.compare(value.size() - 4, 4, ".odf") == 0) {
        value.resize(value.size() - 4);
    }
    return value;
}

std::string normalized_race_name(std::string_view value) {
    std::size_t first = 0;
    while (first < value.size() &&
           (std::isspace(static_cast<unsigned char>(value[first])) ||
            value[first] == '"')) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first &&
           (std::isspace(static_cast<unsigned char>(value[last - 1])) ||
            value[last - 1] == '"')) {
        --last;
    }
    std::string result(value.substr(first, last - first));
    std::transform(result.begin(), result.end(), result.begin(), lower_ascii);
    return result;
}

bool race_event_field(const A2FO_RaceLoadedEvent* event,
                      const char* name, std::string* value) {
    if (!event || !name || !value ||
        (!event->odf_fields && event->odf_field_count != 0)) {
        return false;
    }
    const std::size_t name_size = std::strlen(name);
    for (std::uint32_t index = 0; index < event->odf_field_count; ++index) {
        const A2FO_OdfFieldView& field = event->odf_fields[index];
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

using FindObjectClass = void* (A2FO_CALL*)(const char*);
using GetCurrentMission = void* (A2FO_CALL*)();

FindObjectClass find_object_class_function() noexcept {
    return reinterpret_cast<FindObjectClass>(
        at(g_armada, kGameObjectClassFindRva));
}

void* find_object_class(std::string_view odf) noexcept {
    if (!g_armada || odf.empty()) return nullptr;
    try {
        const std::string name(odf);
        const FindObjectClass find = find_object_class_function();
        return find ? find(name.c_str()) : nullptr;
    } catch (...) {
        return nullptr;
    }
}

void refresh_nebula_renderer_class(void* object_class) noexcept {
    if (!object_class) return;
    try {
        if (!g_nebula_class_observer) {
            HMODULE renderer = a2fo::module_provider(kNebulaRendererModuleName, {"A2FONebulaRenderer_RegisterClass"});
            FARPROC exported = renderer
                ? GetProcAddress(renderer, kNebulaClassObserverExport)
                : nullptr;
            static_assert(sizeof(exported) == sizeof(g_nebula_class_observer),
                          "unexpected function-pointer size");
            std::memcpy(&g_nebula_class_observer, &exported,
                        sizeof(g_nebula_class_observer));
        }
        if (!g_nebula_class_observer) return;

        // Ownership variants may expose their SOD/material state only after an
        // actual craft instance exists. The caller therefore invokes this
        // after construction and retries once on the replacement's first tick.
        // A null ParameterDB asks the renderer for a SOD/ART-only refresh.
        g_nebula_class_observer(object_class, nullptr);
        log_line("Requested mapped-lighting refresh for ODF variant CraftClass");
    } catch (...) {
        log_line("Could not refresh mapped-lighting policies for an ODF variant CraftClass");
    }
}

void* craft_race(void* craft) noexcept {
    if (!craft) return nullptr;
    return *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(craft) + kObjectRaceOffset);
}

std::string craft_odf(void* craft) {
    if (!craft || !g_armada) return {};
    void* object_class = *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(craft) + kObjectClassOffset);
    if (!object_class) return {};
    const char* odf_name = reinterpret_cast<const char*>(
        a2fo_odf_variants_call_thiscall_0(
            at(g_armada, kGameObjectClassGetOdfNameRva), object_class));
    if (!odf_name) return {};
    const char* end = static_cast<const char*>(
        std::memchr(odf_name, '\0', 256));
    if (!end) return {};
    return normalize_odf_basename(std::string(
        odf_name, static_cast<std::size_t>(end - odf_name)));
}

std::string infer_base_odf(void* craft, const std::string& source_odf) {
    const auto retained = g_base_odfs.find(craft);
    if (retained != g_base_odfs.end() && !retained->second.empty()) {
        return retained->second;
    }

    // This is primarily for saves or objects first observed after another
    // module has already supplied a suffixed ODF. Prefer the longest known
    // suffix and only strip it when the corresponding base class exists.
    std::string best_suffix;
    for (const auto& entry : g_race_suffixes) {
        const std::string& suffix = entry.second;
        if (suffix.size() > best_suffix.size() &&
            suffix.size() < source_odf.size() &&
            ends_with_case_insensitive(source_odf, suffix)) {
            best_suffix = suffix;
        }
    }
    if (!best_suffix.empty()) {
        const std::string candidate =
            source_odf.substr(0, source_odf.size() - best_suffix.size());
        if (!candidate.empty() && find_object_class(candidate)) {
            return candidate;
        }
    }
    return source_odf;
}

CraftStateSnapshot snapshot_craft_state(void* craft) noexcept {
    CraftStateSnapshot state;
    if (!craft) return state;
    const auto* bytes = static_cast<const std::uint8_t*>(craft);

    const float current_hull = *reinterpret_cast<const float*>(
        bytes + kCurrentHealthOffset);
    const float maximum_hull = *reinterpret_cast<const float*>(
        bytes + kMaximumHealthOffset);
    if (std::isfinite(current_hull) && std::isfinite(maximum_hull) &&
        maximum_hull > 0.0001f) {
        state.hull_fraction = std::clamp(
            current_hull / maximum_hull, 0.0f, 1.0f);
        state.hull_valid = true;
    }

    const float current_shields = *reinterpret_cast<const float*>(
        bytes + kCurrentShieldsOffset);
    const float maximum_shields = *reinterpret_cast<const float*>(
        bytes + kMaximumShieldsOffset);
    if (std::isfinite(current_shields) && std::isfinite(maximum_shields) &&
        maximum_shields > 0.0001f) {
        state.shield_fraction = std::clamp(
            current_shields / maximum_shields, 0.0f, 1.0f);
        state.shield_valid = true;
    }

    const float current_special_energy = *reinterpret_cast<const float*>(
        bytes + kCurrentSpecialEnergyOffset);
    const float maximum_special_energy = *reinterpret_cast<const float*>(
        bytes + kMaximumSpecialEnergyOffset);
    if (std::isfinite(current_special_energy) &&
        std::isfinite(maximum_special_energy) &&
        maximum_special_energy > 0.0001f) {
        state.special_energy_fraction = std::clamp(
            current_special_energy / maximum_special_energy, 0.0f, 1.0f);
        state.special_energy_valid = true;
    }

    const float current_crew = *reinterpret_cast<const float*>(
        bytes + kCurrentCrewOffset);
    if (std::isfinite(current_crew) && current_crew >= 0.0f) {
        state.current_crew = current_crew;
        state.crew_valid = true;
    }

    const auto* velocity = reinterpret_cast<const float*>(
        bytes + kObjectVelocityOffset);
    if (std::isfinite(velocity[0]) && std::isfinite(velocity[1]) &&
        std::isfinite(velocity[2])) {
        std::copy_n(velocity, state.velocity.size(), state.velocity.begin());
        state.velocity_valid = true;
    }

    void* systems = *reinterpret_cast<void* const*>(bytes + kSystemsOffset);
    if (!systems) return state;
    const auto* system_bytes = static_cast<const std::uint8_t*>(systems);
    for (std::size_t index = 0; index < kSystemCount; ++index) {
        const auto* record = system_bytes + index * kSystemRecordSize;
        const std::int32_t maximum = *reinterpret_cast<const std::int32_t*>(
            record + kSystemMaximumOffset);
        const double current = *reinterpret_cast<const double*>(
            record + kSystemCurrentOffset);
        if (maximum <= 0 || !std::isfinite(current)) continue;
        state.system_fractions[index] = std::clamp(
            current / static_cast<double>(maximum), 0.0, 1.0);
        state.system_operational[index] =
            *reinterpret_cast<const std::uint8_t*>(
                record + kSystemOperationalOffset);
        state.system_forced_disabled[index] =
            *reinterpret_cast<const std::uint8_t*>(
                record + kSystemForcedDisabledOffset);
        const float disable_time = *reinterpret_cast<const float*>(
            record + kSystemDisableTimeOffset);
        state.system_disable_times[index] =
            std::isfinite(disable_time) ? std::max(0.0f, disable_time) : 0.0f;
        state.system_valid[index] = true;
    }
    return state;
}

void restore_craft_state(void* craft,
                         const CraftStateSnapshot& state) noexcept {
    if (!craft) return;
    auto* bytes = static_cast<std::uint8_t*>(craft);
    if (state.hull_valid) {
        const float maximum = *reinterpret_cast<const float*>(
            bytes + kMaximumHealthOffset);
        if (std::isfinite(maximum) && maximum > 0.0001f) {
            *reinterpret_cast<float*>(bytes + kCurrentHealthOffset) =
                maximum * state.hull_fraction;
        }
    }
    if (state.shield_valid) {
        const float maximum = *reinterpret_cast<const float*>(
            bytes + kMaximumShieldsOffset);
        if (std::isfinite(maximum) && maximum > 0.0001f) {
            *reinterpret_cast<float*>(bytes + kCurrentShieldsOffset) =
                maximum * state.shield_fraction;
        }
    }
    if (state.special_energy_valid) {
        const float maximum = *reinterpret_cast<const float*>(
            bytes + kMaximumSpecialEnergyOffset);
        if (std::isfinite(maximum) && maximum > 0.0001f) {
            *reinterpret_cast<float*>(bytes + kCurrentSpecialEnergyOffset) =
                maximum * state.special_energy_fraction;
        }
    }
    if (state.crew_valid) {
        const float maximum = *reinterpret_cast<const float*>(
            bytes + kMaximumCrewOffset);
        if (std::isfinite(maximum) && maximum >= 0.0f) {
            *reinterpret_cast<float*>(bytes + kCurrentCrewOffset) =
                std::clamp(state.current_crew, 0.0f, maximum);
        }
    }
    if (state.velocity_valid) {
        auto* velocity = reinterpret_cast<float*>(bytes + kObjectVelocityOffset);
        std::copy_n(state.velocity.begin(), state.velocity.size(), velocity);
    }

    void* systems = *reinterpret_cast<void**>(bytes + kSystemsOffset);
    if (!systems) return;
    auto* system_bytes = static_cast<std::uint8_t*>(systems);
    for (std::size_t index = 0; index < kSystemCount; ++index) {
        if (!state.system_valid[index]) continue;
        auto* record = system_bytes + index * kSystemRecordSize;
        const std::int32_t maximum = *reinterpret_cast<const std::int32_t*>(
            record + kSystemMaximumOffset);
        if (maximum <= 0) continue;
        *reinterpret_cast<double*>(record + kSystemCurrentOffset) =
            static_cast<double>(maximum) * state.system_fractions[index];
        *reinterpret_cast<std::uint8_t*>(record + kSystemOperationalOffset) =
            state.system_operational[index];
        *reinterpret_cast<std::uint8_t*>(
            record + kSystemForcedDisabledOffset) =
            state.system_forced_disabled[index];
        *reinterpret_cast<float*>(record + kSystemDisableTimeOffset) =
            state.system_disable_times[index];
    }
}

struct SwapContext {
    void* craft;
    void* current_race;
    std::string source_odf;
    std::string base_odf;
    std::string suffix;
    std::string target_odf;
    void* target_class = nullptr;
    void* replacement = nullptr;
    void* mission = nullptr;
    void** mission_vtable = nullptr;
    std::array<float, 12> transform_copy{};
    std::int32_t team = 0;
    CraftStateSnapshot state{};
};
std::uint64_t swap_op(void* opaque, std::uint32_t operation,
                      std::uint32_t) noexcept {
    auto& context = *static_cast<SwapContext*>(opaque);
    try {
        switch (operation) {
            case 0:
                context.source_odf = craft_odf(context.craft);
                return !context.source_odf.empty();
            case 1:
                context.base_odf = infer_base_odf(
                    context.craft, context.source_odf);
                if (context.base_odf.empty()) return 0;
                g_base_odfs[context.craft] = context.base_odf;
                return 1;
            case 2: {
                const auto suffix_entry = g_race_suffixes.find(
                    context.current_race);
                if (suffix_entry != g_race_suffixes.end())
                    context.suffix = suffix_entry->second;
                context.target_odf = context.suffix.empty()
                    ? context.base_odf : context.base_odf + context.suffix;
                return 1;
            }
            case 3: return equal_ascii_case_insensitive(
                context.target_odf, context.source_odf);
            case 4:
                context.target_class = find_object_class(context.target_odf);
                return context.target_class != nullptr;
            case 5: return !context.suffix.empty();
            case 6:
                if (g_missing_variants_logged.insert(
                        context.target_odf).second) {
                    log_line("No loaded ODF variant '" + context.target_odf +
                             "'; falling back to '" + context.base_odf + "'");
                }
                context.target_odf = context.base_odf;
                if (!equal_ascii_case_insensitive(
                        context.target_odf, context.source_odf)) {
                    context.target_class = find_object_class(context.target_odf);
                }
                return context.target_class != nullptr;
            case 7: return equal_ascii_case_insensitive(
                context.target_odf, context.source_odf);
            case 8:
                log_line("Could not resolve fallback base ODF '" +
                         context.base_odf + "'");
                return 0;
            case 9: {
                const void* transform = reinterpret_cast<const void*>(
                    a2fo_odf_variants_call_thiscall_0(
                        at(g_armada, kGameObjectGetTransformRva),
                        context.craft));
                if (!transform) {
                    log_line("Source transform unavailable for '" +
                             context.source_odf + "'");
                    return 0;
                }
                std::memcpy(context.transform_copy.data(), transform,
                            context.transform_copy.size() * sizeof(float));
                context.team = *reinterpret_cast<const std::int32_t*>(
                    static_cast<const std::uint8_t*>(context.craft) +
                    kObjectTeamOffset);
                context.state = snapshot_craft_state(context.craft);
                return 1;
            }
            case 10:
                context.replacement = a2fo_odf_variants_construct(
                    at(g_armada, kGameObjectClassConstructRva),
                    context.target_class, context.transform_copy.data(),
                    context.team, 0, nullptr);
                return context.replacement != nullptr;
            case 11:
                log_line("Could not construct ODF variant '" +
                         context.target_odf + "'");
                return 0;
            case 12:
                refresh_nebula_renderer_class(context.target_class);
                g_pending_renderer_refresh_crafts.insert(context.replacement);
                return 0;
            case 13: {
                const auto get_current_mission =
                    reinterpret_cast<GetCurrentMission>(
                        at(g_armada, kAiMissionGetCurrentRva));
                context.mission = get_current_mission
                    ? get_current_mission() : nullptr;
                context.mission_vtable = context.mission
                    ? *reinterpret_cast<void***>(context.mission) : nullptr;
                return context.mission_vtable &&
                    context.mission_vtable[6] != nullptr;
            }
            case 14:
                log_line("Active mission/AddObject unavailable for '" +
                         context.target_odf + "'");
                a2fo_odf_variants_call_thiscall_0(
                    at(g_armada, kCraftDoExpireRva), context.replacement);
                return 0;
            case 15:
                a2fo_odf_variants_call_thiscall_1(
                    context.mission_vtable[6], context.mission,
                    reinterpret_cast<std::uintptr_t>(context.replacement));
                return 0;
            case 16:
                a2fo_odf_variants_call_thiscall_1(
                    at(g_armada, kEvolverSwapObjectsRva), context.craft,
                    reinterpret_cast<std::uintptr_t>(context.replacement));
                return 0;
            case 17:
                restore_craft_state(context.replacement, context.state);
                g_base_odfs[context.replacement] = context.base_odf;
                g_last_races[context.replacement] = context.current_race;
                g_base_odfs.erase(context.craft);
                g_last_races.erase(context.craft);
                a2fo_odf_variants_call_thiscall_0(
                    at(g_armada, kCraftDoExpireRva), context.craft);
                log_line("ODF ownership variant: " + context.source_odf +
                         " -> " + context.target_odf);
                return 1;
        }
        return 0;
    } catch (...) {
        log_line("ODF ownership swap skipped after an unexpected C++ exception");
        return UINT64_MAX;
    }
}
bool swap_craft_odf(void* craft, void* current_race) noexcept {
    SwapContext context{};
    context.craft = craft;
    context.current_race = current_race;
    return a2fo_rs_odf_swap(g_runtime_alive && craft && g_armada,
                            &context, &swap_op) != 0;
}

struct RaceContext {
    const A2FO_RaceLoadedEvent* event;
    std::string raw_name;
    std::string raw_suffix;
    std::string suffix;
    bool name_found = false;
    bool suffix_found = false;
};
std::uint64_t race_op(void* opaque, std::uint32_t operation,
                      std::uint32_t) noexcept {
    auto& context = *static_cast<RaceContext*>(opaque);
    try {
        switch (operation) {
            case 0:
                context.name_found = race_event_field(
                    context.event, kFactionNameCommand, &context.raw_name);
                context.suffix_found = race_event_field(
                    context.event, a2fo::faction_suffix::kCommand,
                    &context.raw_suffix);
                return static_cast<std::uint64_t>(context.name_found) |
                    (static_cast<std::uint64_t>(context.suffix_found) << 1);
            case 1:
                return a2fo::faction_suffix::normalize(
                    context.raw_suffix, &context.suffix);
            case 2: {
                char message[192]{};
                std::snprintf(message, sizeof(message),
                    "Rejected invalid %s on Race %p",
                    a2fo::faction_suffix::kCommand, context.event->race);
                log_line(message);
                g_race_suffixes.erase(context.event->race);
                return 0;
            }
            case 3:
                return normalized_race_name(context.raw_name) == "borg";
            case 4:
                context.suffix = kBorgDefaultSuffix;
                return 0;
            case 5: return context.suffix.empty();
            case 6: g_race_suffixes.erase(context.event->race); return 0;
            case 7: {
                g_race_suffixes[context.event->race] = context.suffix;
                char message[224]{};
                std::snprintf(message, sizeof(message),
                    "Registered ODF ownership suffix '%s' on Race %p%s",
                    context.suffix.c_str(), context.event->race,
                    (!context.suffix_found &&
                     context.suffix == kBorgDefaultSuffix)
                        ? " (Borg default)" : "");
                log_line(message);
                return 0;
            }
        }
        return 0;
    } catch (...) {
        log_line("Could not retain Race ODF ownership suffix");
        return UINT64_MAX;
    }
}
void A2FO_CALL race_loaded_handler(
    const A2FO_RaceLoadedEvent* event, void*) {
    RaceContext context{};
    context.event = event;
    a2fo_rs_odf_race(g_runtime_alive && event &&
        event->struct_size >= sizeof(*event) && event->race,
        &context, &race_op);
}

void cleanup_craft(void* craft) noexcept {
    if (!craft) return;
    g_last_races.erase(craft);
    g_base_odfs.erase(craft);
    g_pending_renderer_refresh_crafts.erase(craft);
}

struct CraftEventContext {
    const A2FO_CraftEvent* event;
    void* current_race = nullptr;
    std::unordered_map<void*, void*>::iterator found;
    std::string current_odf;
    std::string base_odf;
    bool suffix_found = false;
    bool already_current_variant = false;
};
std::uint64_t craft_event_op(void* opaque, std::uint32_t operation,
                             std::uint32_t) noexcept {
    auto& context = *static_cast<CraftEventContext*>(opaque);
    void* craft = context.event->craft;
    try {
        switch (operation) {
            case 0: {
                const auto pending = g_pending_renderer_refresh_crafts.find(craft);
                if (pending != g_pending_renderer_refresh_crafts.end()) {
                    void* object_class = *reinterpret_cast<void**>(
                        static_cast<std::uint8_t*>(craft) + kObjectClassOffset);
                    refresh_nebula_renderer_class(object_class);
                    g_pending_renderer_refresh_crafts.erase(pending);
                    log_line("Retried mapped-lighting refresh on replacement craft first simulation tick");
                }
                return 0;
            }
            case 1:
                context.current_race = craft_race(craft);
                context.found = g_last_races.find(craft);
                return context.found != g_last_races.end();
            case 2:
                g_last_races.emplace(craft, context.current_race);
                return 0;
            case 3:
                context.current_odf = craft_odf(craft);
                return !context.current_odf.empty();
            case 4:
                context.base_odf = context.current_odf;
#ifdef A2FO_BUILTIN_FEATURE
                context.base_odf = infer_base_odf(craft, context.current_odf);
#endif
                return 0;
            case 5: {
                const auto suffix_entry = g_race_suffixes.find(context.current_race);
                context.suffix_found = suffix_entry != g_race_suffixes.end() &&
                    !suffix_entry->second.empty();
                if (context.suffix_found) {
                    const std::string& suffix = suffix_entry->second;
                    context.already_current_variant =
                        ends_with_case_insensitive(context.current_odf, suffix);
                    if (context.already_current_variant &&
                        suffix.size() < context.current_odf.size()) {
                        const std::string candidate = context.current_odf.substr(
                            0, context.current_odf.size() - suffix.size());
                        if (!candidate.empty() && find_object_class(candidate)) {
                            context.base_odf = candidate;
                        }
                    }
                }
                return 0;
            }
            case 6: g_base_odfs[craft] = context.base_odf; return 0;
            case 7: return context.suffix_found &&
                !context.already_current_variant;
            case 8:
                swap_craft_odf(craft, context.current_race);
                return 0;
            case 9: return context.found->second == context.current_race;
            case 10:
                context.found->second = context.current_race;
                swap_craft_odf(craft, context.current_race);
                return 0;
            case 11: cleanup_craft(craft); return 0;
            case 12:
                g_last_races[craft] = craft_race(craft);
                context.current_odf = craft_odf(craft);
                if (!context.current_odf.empty()) {
                    g_base_odfs[craft] = infer_base_odf(craft, context.current_odf);
                }
                return 0;
        }
        return 0;
    } catch (...) {
        log_line("Craft ownership transition check failed");
        return UINT64_MAX;
    }
}
void A2FO_CALL craft_event_handler(
    const A2FO_CraftEvent* event, void*) {
    const bool enabled = runtime_enabled();
    CraftEventContext context{};
    context.event = event;
    const bool valid = g_runtime_alive && event &&
        event->struct_size >= sizeof(*event) && event->craft;
    const std::uint32_t kind = !valid ? 3u :
        event->kind == A2FO_CRAFT_EVENT_SIMULATE_POST ? 0u :
        event->kind == A2FO_CRAFT_EVENT_CLEANUP ? 1u :
        event->kind == A2FO_CRAFT_EVENT_POST_LOAD ? 2u : 3u;
    a2fo_rs_odf_craft(valid, enabled, kind, &context, &craft_event_op);
}

bool native_signatures_supported() noexcept {
    if (!g_armada) return false;
    const bool supported =
        signature_matches(g_armada, kAiMissionGetCurrentRva,
                          kExpectedAiMissionGetCurrent) &&
        signature_matches(g_armada, kGameObjectClassFindRva,
                          kExpectedGameObjectClassFind) &&
        signature_matches(g_armada, kGameObjectClassConstructRva,
                          kExpectedGameObjectClassConstruct) &&
        signature_matches(g_armada, kGameObjectClassGetOdfNameRva,
                          kExpectedGameObjectClassGetOdfName) &&
        signature_matches(g_armada, kGameObjectGetTransformRva,
                          kExpectedGameObjectGetTransform) &&
        signature_matches(g_armada, kEvolverSwapObjectsRva,
                          kExpectedEvolverSwapObjects);
    if (!supported) {
        log_line("Required Armada ODF-swap signatures do not match; module disabled");
    }
    return supported;
}

std::uint64_t odf_init_op(void* opaque, std::uint32_t operation,
                          std::uint32_t) noexcept {
    const auto* api = static_cast<const A2FO_ModuleApi*>(opaque);
    try {
    switch (operation) {
        case 0:
            g_api = api;
            g_armada = static_cast<HMODULE>(api->armada_module());
            return g_armada != nullptr;
        case 1: return native_signatures_supported();
        case 2: {
            const char* race_fields[]{
                kFactionNameCommand, a2fo::faction_suffix::kCommand};
            return api->register_race_loaded_handler(
                kModuleName, race_fields,
                static_cast<std::uint32_t>(std::size(race_fields)),
                &race_loaded_handler, nullptr);
        }
        case 3:
            return A2FO_MODULE_API_HAS(api, register_craft_event_handler_masked) &&
                api->register_craft_event_handler_masked;
        case 4:
            return api->register_craft_event_handler_masked(
                kModuleName,
                A2FO_CRAFT_EVENT_MASK_SIMULATE_POST |
                    A2FO_CRAFT_EVENT_MASK_CLEANUP |
                    A2FO_CRAFT_EVENT_MASK_POST_LOAD,
                &craft_event_handler, nullptr);
        case 5:
            return api->register_craft_event_handler(
                kModuleName, &craft_event_handler, nullptr);
        case 6:
            g_runtime_alive = true;
            log_line("Ownership-aware ODF variants initialized; Borg defaults to _b");
            return 0;
    }
    return 0;
    } catch (...) {
        return 0;
    }
}
std::uint64_t odf_shutdown_op(void*, std::uint32_t operation,
                              std::uint32_t) noexcept {
    switch (operation) {
        case 0: g_runtime_alive = false; break;
        case 1: g_race_suffixes.clear(); break;
        case 2: g_last_races.clear(); g_base_odfs.clear(); break;
        case 3: g_missing_variants_logged.clear(); break;
        case 4: g_pending_renderer_refresh_crafts.clear(); break;
        case 5: g_nebula_class_observer = nullptr; break;
        case 6: g_armada = nullptr; g_api = nullptr; break;
    }
    return 0;
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module ||
        (api->capabilities & A2FO_CAP_RACE_LOADED) == 0 ||
        (api->capabilities & A2FO_CAP_CRAFT_EVENTS) == 0 ||
        !A2FO_MODULE_API_HAS(api, register_race_loaded_handler) ||
        !api->register_race_loaded_handler ||
        !A2FO_MODULE_API_HAS(api, register_craft_event_handler) ||
        !api->register_craft_event_handler) {
        return false;
    }

    return a2fo_rs_odf_init(true, const_cast<A2FO_ModuleApi*>(api),
                            &odf_init_op) != 0;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    a2fo_rs_odf_shutdown(nullptr, &odf_shutdown_op);
}
