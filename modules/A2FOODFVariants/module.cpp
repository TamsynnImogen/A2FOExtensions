/* Ownership-aware ODF variants for Armada II / Fleet Operations Roots. */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_faction_suffix.hpp"

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
            HMODULE renderer = GetModuleHandleA(kNebulaRendererModuleName);
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

bool swap_craft_odf(void* craft, void* current_race) noexcept {
    if (!g_runtime_alive || !craft || !g_armada) return false;
    try {
        const std::string source_odf = craft_odf(craft);
        if (source_odf.empty()) return false;

        const std::string base_odf = infer_base_odf(craft, source_odf);
        if (base_odf.empty()) return false;
        g_base_odfs[craft] = base_odf;

        std::string suffix;
        const auto suffix_entry = g_race_suffixes.find(current_race);
        if (suffix_entry != g_race_suffixes.end()) suffix = suffix_entry->second;

        std::string target_odf = suffix.empty()
            ? base_odf
            : base_odf + suffix;
        if (equal_ascii_case_insensitive(target_odf, source_odf)) {
            return false;
        }
        void* target_class = find_object_class(target_odf);

        if (!target_class && !suffix.empty()) {
            if (g_missing_variants_logged.insert(target_odf).second) {
                log_line("No loaded ODF variant '" + target_odf +
                         "'; falling back to '" + base_odf + "'");
            }
            target_odf = base_odf;
            if (!equal_ascii_case_insensitive(target_odf, source_odf)) {
                target_class = find_object_class(target_odf);
            }
        }

        if (equal_ascii_case_insensitive(target_odf, source_odf)) {
            return false;
        }
        if (!target_class) {
            log_line("Could not resolve fallback base ODF '" + base_odf + "'");
            return false;
        }

        const void* transform = reinterpret_cast<const void*>(
            a2fo_odf_variants_call_thiscall_0(
                at(g_armada, kGameObjectGetTransformRva), craft));
        if (!transform) {
            log_line("Source transform unavailable for '" + source_odf + "'");
            return false;
        }
        std::array<float, 12> transform_copy{};
        std::memcpy(transform_copy.data(), transform,
                    transform_copy.size() * sizeof(float));
        const std::int32_t team = *reinterpret_cast<const std::int32_t*>(
            static_cast<const std::uint8_t*>(craft) + kObjectTeamOffset);
        const CraftStateSnapshot state = snapshot_craft_state(craft);

        void* replacement = a2fo_odf_variants_construct(
            at(g_armada, kGameObjectClassConstructRva), target_class,
            transform_copy.data(), team, 0, nullptr);
        if (!replacement) {
            log_line("Could not construct ODF variant '" + target_odf + "'");
            return false;
        }

        // Construction is the first reliable point at which a late-loaded
        // variant CraftClass may expose its SOD/material state. Refresh now,
        // then retry once on the replacement's first simulation tick to cover
        // Fleet Ops' remaining lazy-geometry paths.
        refresh_nebula_renderer_class(target_class);
        g_pending_renderer_refresh_crafts.insert(replacement);

        const auto get_current_mission = reinterpret_cast<GetCurrentMission>(
            at(g_armada, kAiMissionGetCurrentRva));
        void* mission = get_current_mission ? get_current_mission() : nullptr;
        void** mission_vtable = mission ? *reinterpret_cast<void***>(mission)
                                        : nullptr;
        if (!mission_vtable || !mission_vtable[6]) {
            log_line("Active mission/AddObject unavailable for '" +
                     target_odf + "'");
            a2fo_odf_variants_call_thiscall_0(
                at(g_armada, kCraftDoExpireRva), replacement);
            return false;
        }
        a2fo_odf_variants_call_thiscall_1(
            mission_vtable[6], mission,
            reinterpret_cast<std::uintptr_t>(replacement));

        // Evolver's native handoff preserves common ownership, selection and
        // overview relationships without mutating a live CraftClass pointer.
        a2fo_odf_variants_call_thiscall_1(
            at(g_armada, kEvolverSwapObjectsRva), craft,
            reinterpret_cast<std::uintptr_t>(replacement));
        restore_craft_state(replacement, state);

        g_base_odfs[replacement] = base_odf;
        g_last_races[replacement] = current_race;
        g_base_odfs.erase(craft);
        g_last_races.erase(craft);

        // We are called from SIMULATE_POST. Expire the original and let
        // Armada perform its normal deferred cleanup after this callback.
        a2fo_odf_variants_call_thiscall_0(
            at(g_armada, kCraftDoExpireRva), craft);

        log_line("ODF ownership variant: " + source_odf + " -> " + target_odf);
        return true;
    } catch (...) {
        log_line("ODF ownership swap skipped after an unexpected C++ exception");
        return false;
    }
}

void A2FO_CALL race_loaded_handler(
    const A2FO_RaceLoadedEvent* event, void*) {
    if (!g_runtime_alive || !event || event->struct_size < sizeof(*event) ||
        !event->race) {
        return;
    }
    try {
        std::string raw_name;
        std::string raw_suffix;
        const bool name_found = race_event_field(
            event, kFactionNameCommand, &raw_name);
        const bool suffix_found = race_event_field(
            event, a2fo::faction_suffix::kCommand, &raw_suffix);

        std::string suffix;
        if (suffix_found) {
            if (!a2fo::faction_suffix::normalize(raw_suffix, &suffix)) {
                char message[192]{};
                std::snprintf(
                    message, sizeof(message),
                    "Rejected invalid %s on Race %p",
                    a2fo::faction_suffix::kCommand, event->race);
                log_line(message);
                g_race_suffixes.erase(event->race);
                return;
            }
        } else if (name_found &&
                   normalized_race_name(raw_name) == "borg") {
            suffix = kBorgDefaultSuffix;
        }

        if (suffix.empty()) {
            g_race_suffixes.erase(event->race);
            return;
        }
        g_race_suffixes[event->race] = suffix;
        char message[224]{};
        std::snprintf(message, sizeof(message),
                      "Registered ODF ownership suffix '%s' on Race %p%s",
                      suffix.c_str(), event->race,
                      (!suffix_found && suffix == kBorgDefaultSuffix)
                          ? " (Borg default)" : "");
        log_line(message);
    } catch (...) {
        log_line("Could not retain Race ODF ownership suffix");
    }
}

void cleanup_craft(void* craft) noexcept {
    if (!craft) return;
    g_last_races.erase(craft);
    g_base_odfs.erase(craft);
    g_pending_renderer_refresh_crafts.erase(craft);
}

void A2FO_CALL craft_event_handler(
    const A2FO_CraftEvent* event, void*) {
    if (!g_runtime_alive || !event || event->struct_size < sizeof(*event) ||
        !event->craft) {
        return;
    }
    try {
        switch (event->kind) {
        case A2FO_CRAFT_EVENT_SIMULATE_POST: {
            const auto pending_refresh =
                g_pending_renderer_refresh_crafts.find(event->craft);
            if (pending_refresh != g_pending_renderer_refresh_crafts.end()) {
                void* object_class = *reinterpret_cast<void**>(
                    static_cast<std::uint8_t*>(event->craft) +
                    kObjectClassOffset);
                refresh_nebula_renderer_class(object_class);
                g_pending_renderer_refresh_crafts.erase(pending_refresh);
                log_line("Retried mapped-lighting refresh on replacement craft first simulation tick");
            }

            void* current_race = craft_race(event->craft);
            const auto found = g_last_races.find(event->craft);
            if (found == g_last_races.end()) {
                g_last_races.emplace(event->craft, current_race);
                const std::string current_odf = craft_odf(event->craft);
                if (current_odf.empty()) return;

                // A craft placed in the map editor may have its Race changed
                // before it ever receives a simulation tick. In that case our
                // first observation already sees the new owner, so waiting for
                // a later Race transition would miss the initial ODF swap.
                // Seed the canonical ODF and immediately reconcile the craft
                // against its current owner's suffix.
                std::string base_odf = current_odf;
                bool already_current_variant = false;
                const auto suffix_entry = g_race_suffixes.find(current_race);
                if (suffix_entry != g_race_suffixes.end() &&
                    !suffix_entry->second.empty()) {
                    const std::string& suffix = suffix_entry->second;
                    already_current_variant =
                        ends_with_case_insensitive(current_odf, suffix);
                    if (already_current_variant &&
                        suffix.size() < current_odf.size()) {
                        const std::string candidate = current_odf.substr(
                            0, current_odf.size() - suffix.size());
                        if (!candidate.empty() && find_object_class(candidate)) {
                            base_odf = candidate;
                        }
                    }
                }
                g_base_odfs[event->craft] = base_odf;

                if (suffix_entry != g_race_suffixes.end() &&
                    !suffix_entry->second.empty() &&
                    !already_current_variant) {
                    swap_craft_odf(event->craft, current_race);
                }
                return;
            }
            if (found->second == current_race) return;
            found->second = current_race;
            swap_craft_odf(event->craft, current_race);
            return;
        }
        case A2FO_CRAFT_EVENT_CLEANUP:
            cleanup_craft(event->craft);
            return;
        case A2FO_CRAFT_EVENT_POST_LOAD: {
            g_last_races[event->craft] = craft_race(event->craft);
            const std::string current_odf = craft_odf(event->craft);
            if (!current_odf.empty()) {
                g_base_odfs[event->craft] =
                    infer_base_odf(event->craft, current_odf);
            }
            return;
        }
        default:
            return;
        }
    } catch (...) {
        log_line("Craft ownership transition check failed");
    }
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

    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    if (!g_armada || !native_signatures_supported()) return false;

    const char* race_fields[] = {
        kFactionNameCommand, a2fo::faction_suffix::kCommand};
    if (!api->register_race_loaded_handler(
            kModuleName, race_fields,
            static_cast<std::uint32_t>(std::size(race_fields)),
            &race_loaded_handler, nullptr)) {
        return false;
    }

    bool craft_registered = false;
    if (A2FO_MODULE_API_HAS(api, register_craft_event_handler_masked) &&
        api->register_craft_event_handler_masked) {
        craft_registered = api->register_craft_event_handler_masked(
            kModuleName,
            A2FO_CRAFT_EVENT_MASK_SIMULATE_POST |
                A2FO_CRAFT_EVENT_MASK_CLEANUP |
                A2FO_CRAFT_EVENT_MASK_POST_LOAD,
            &craft_event_handler, nullptr);
    } else {
        craft_registered = api->register_craft_event_handler(
            kModuleName, &craft_event_handler, nullptr);
    }
    if (!craft_registered) return false;

    g_runtime_alive = true;
    log_line("Ownership-aware ODF variants initialized; Borg defaults to _b");
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    g_runtime_alive = false;
    g_race_suffixes.clear();
    g_last_races.clear();
    g_base_odfs.clear();
    g_missing_variants_logged.clear();
    g_pending_renderer_refresh_crafts.clear();
    g_nebula_class_observer = nullptr;
    g_armada = nullptr;
    g_api = nullptr;
}
