/*
 * Optional ODF-driven three-dimensional weapon firing arcs.
 *
 * Weapon ODFs which use none of the new commands retain Armada/Fleet Ops'
 * native restrictFireArc/fireArc path. A configured weapon replaces the
 * stock directional gate while retaining Armada's native range, obstruction,
 * and target-validity checks, using the weapon owner's local
 * right/up/forward axes.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "fire_arc.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

extern "C" {
std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
}

namespace {

using a2fo::fire_arcs::ArcConfig;
using a2fo::fire_arcs::ArcLine;
using a2fo::fire_arcs::ArcLineStyle;
using a2fo::fire_arcs::ArcMode;
using a2fo::fire_arcs::Matrix34;

constexpr char kModuleName[] = "A2FOFireArcs";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kWeaponClassConstructorRva = 0x00264e30;
constexpr std::uintptr_t kWeaponCanFireAtRva = 0x0026f8c0;
constexpr std::uintptr_t kParameterDbGetFloatRva = 0x00134df0;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kEntityGetTransformRva = 0x000cfd50;
constexpr std::uintptr_t kEntityGetWorldTransformRva = 0x000cff90;
constexpr std::uintptr_t kWeaponGetOwnerRva = 0x00271050;
constexpr std::uintptr_t kWeaponGetTargetRva = 0x00271300;
constexpr std::uintptr_t kDisplayInterfaceDrawLineRva = 0x0011b130;
constexpr std::uintptr_t kStandardComponentIsMouseOverRva = 0x0010c140;

// Fleet Operations detours both entries before extension modules load. The
// new module chains only these exact supported handlers.
constexpr std::uintptr_t kFoWeaponClassConstructorHandlerRva = 0x0010ef74;
constexpr std::uintptr_t kFoWeaponCanFireAtHandlerRva = 0x001358ac;
constexpr std::uintptr_t kFoShipSystemIconRenderRva = 0x001ed458;

constexpr std::size_t kWeaponClassOnWeaponOffset = 0x04;
constexpr std::size_t kHardpointListOnWeaponOffset = 0x10;
constexpr std::size_t kRestrictFireArcOnWeaponClassOffset = 0x1b7;
constexpr std::size_t kRangeOnWeaponClassOffset = 0x1c0;
constexpr std::size_t kPositionOnGameObjectOffset = 0xac;
constexpr std::size_t kWeaponSystemOnCraftOffset = 0x128;
constexpr std::size_t kWeaponVectorBeginOffset = 0x0c;
constexpr std::size_t kWeaponVectorEndOffset = 0x10;
constexpr std::size_t kCraftOnShipSystemIconOffset = 0x28;
constexpr std::size_t kWeaponIndexOnShipSystemIconOffset = 0x30;
constexpr std::size_t kHardpointOnWeaponListNodeOffset = 0x08;

constexpr std::uint8_t kExpectedWeaponClassConstructor[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uint8_t kExpectedFoWeaponClassConstructorHandler[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xdc, 0x53};
constexpr std::uint8_t kExpectedFoWeaponCanFireAtHandler[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf4, 0x53};
constexpr std::uint8_t kExpectedWeaponCanFireAt[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x1c};
constexpr std::uint8_t kExpectedEntityGetWorldTransform[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0c, 0x8b, 0x49, 0x04};
constexpr std::uint8_t kExpectedDisplayInterfaceDrawLine[] = {
    0x55, 0x8b, 0xec, 0xa1, 0x08, 0xd5, 0x7a, 0x00};
constexpr std::uint8_t kExpectedStandardComponentIsMouseOver[] = {
    0x8a, 0x41, 0x18, 0x84, 0xc0, 0x74, 0x12};
constexpr std::uint8_t kExpectedWeaponGetTarget[] = {
    0x8b, 0x49, 0x38, 0x51, 0xe8};
constexpr std::uint8_t kExpectedFoShipSystemIconRender[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xbc, 0x53};

struct OptionalFloat {
    bool present = false;
    bool valid = false;
    float value = 0.0f;
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
bool g_runtime_ready = false;
bool g_chained_fo_weapon_class_constructor = false;
bool g_chained_fo_weapon_can_fire_at = false;
void* g_weapon_class_constructor_original = nullptr;
void* g_weapon_can_fire_at_original = nullptr;
void* g_ship_system_icon_render_original = nullptr;
A2FO_InlineHook g_weapon_class_constructor_hook{};
A2FO_InlineHook g_weapon_can_fire_at_hook{};
A2FO_InlineHook g_ship_system_icon_render_hook{};
std::unordered_map<void*, ArcConfig> g_class_arcs;

// These one-shot messages prove that both engine stages reached the module
// without turning the per-frame target loop into an unbounded log stream.
volatile LONG g_logged_first_custom_check = 0;
volatile LONG g_logged_first_direction_allowed_target = 0;
volatile LONG g_logged_first_direction_rejected_target = 0;
volatile LONG g_logged_first_trigger_allowed_target = 0;
volatile LONG g_logged_first_trigger_rejected_target = 0;
volatile LONG g_logged_first_hover_visualization = 0;

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(
              reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
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

bool writable_range(void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const DWORD protection = information.Protect & 0xffu;
    const bool writable = protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
    if (!writable) return false;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress);
    return start >= base && size <= information.RegionSize - (start - base);
}

template <typename T>
T read_at(const void* object, std::size_t offset,
          T fallback = T{}) noexcept {
    const auto* address = object
        ? static_cast<const std::uint8_t*>(object) + offset : nullptr;
    if (!readable_range(address, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

void trim_string(std::string* value) {
    if (!value) return;
    std::size_t begin = 0;
    while (begin < value->size() && std::isspace(
               static_cast<unsigned char>((*value)[begin]))) {
        ++begin;
    }
    std::size_t end = value->size();
    while (end > begin && std::isspace(
               static_cast<unsigned char>((*value)[end - 1]))) {
        --end;
    }
    *value = value->substr(begin, end - begin);
}

void lower_string(std::string* value) {
    if (!value) return;
    for (char& character : *value) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
}

bool query_parameter_string(void* parameter_db, const char* key,
                            std::string* output,
                            bool* present) noexcept {
    if (present) *present = false;
    if (output) output->clear();
    if (!parameter_db || !key || !*key || !output || !g_armada) {
        return false;
    }
    std::array<char, 260> value{};
    const std::uintptr_t found = a2fo_fire_arc_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(value.data()),
        static_cast<std::uintptr_t>(value.size()),
        reinterpret_cast<std::uintptr_t>(""));
    value.back() = '\0';
    if ((found & 0xffu) == 0) return true;
    if (present) *present = true;
    try {
        *output = value.data();
        trim_string(output);
        return true;
    } catch (...) {
        output->clear();
        return false;
    }
}

bool parse_float(const std::string& text, float* output) noexcept {
    if (!output || text.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || errno == ERANGE || !std::isfinite(parsed)) {
        return false;
    }
    if (*end == 'f' || *end == 'F') ++end;
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end != '\0') return false;
    *output = parsed;
    return true;
}

OptionalFloat read_optional_float(void* parameter_db,
                                  const char* key) noexcept {
    OptionalFloat result{};
    float native_value = 0.0f;
    const std::uintptr_t native_found =
        a2fo_fire_arc_call_thiscall_3(
            at(g_armada, kParameterDbGetFloatRva), parameter_db,
            reinterpret_cast<std::uintptr_t>(key),
            reinterpret_cast<std::uintptr_t>(&native_value), 0);
    if ((native_found & 0xffu) != 0) {
        result.present = true;
        result.valid = std::isfinite(native_value);
        result.value = native_value;
        return result;
    }

    // A quoted number or an extension parser which retained an unknown field
    // as text can still opt in through the string representation.
    std::string text;
    if (!query_parameter_string(
            parameter_db, key, &text, &result.present) ||
        !result.present) {
        return result;
    }
    result.valid = parse_float(text, &result.value);
    return result;
}

std::string weapon_description(void* parameter_db) noexcept {
    std::string description;
    bool present = false;
    if (query_parameter_string(
            parameter_db, "wpnName", &description, &present) &&
        present && !description.empty()) {
        return description;
    }
    if (query_parameter_string(
            parameter_db, "ordName", &description, &present) &&
        present && !description.empty()) {
        return description;
    }
    return "<unnamed weapon>";
}

void log_invalid_policy(const std::string& description,
                        const char* reason) noexcept {
    char message[420]{};
    std::snprintf(
        message, sizeof(message),
        "Ignoring custom fire arc on '%s': %s; native behavior retained",
        description.c_str(), reason ? reason : "invalid configuration");
    log_line(message);
}

bool build_arc_config(void* parameter_db, ArcConfig* output,
                      std::string* description) noexcept {
    if (!parameter_db || !output || !description) return false;

    const OptionalFloat centre = read_optional_float(
        parameter_db, "fireArcCenter");
    const OptionalFloat width = read_optional_float(
        parameter_db, "fireArcWidth");
    const OptionalFloat yaw = read_optional_float(
        parameter_db, "fireArcYaw");
    const OptionalFloat pitch = read_optional_float(
        parameter_db, "fireArcPitch");
    const OptionalFloat yaw_angle = read_optional_float(
        parameter_db, "fireArcYawAngle");
    const OptionalFloat pitch_angle = read_optional_float(
        parameter_db, "fireArcPitchAngle");
    const OptionalFloat cone_angle = read_optional_float(
        parameter_db, "fireArcAngle");
    std::string mode_text;
    bool mode_present = false;
    const bool mode_read = query_parameter_string(
        parameter_db, "fireArcMode", &mode_text, &mode_present);

    const bool any_command = centre.present || width.present ||
        yaw.present || pitch.present || yaw_angle.present ||
        pitch_angle.present || cone_angle.present || mode_present;
    if (!any_command) return false;

    *description = weapon_description(parameter_db);
    const bool numeric_values_valid =
        (!centre.present || centre.valid) &&
        (!width.present || width.valid) &&
        (!yaw.present || yaw.valid) &&
        (!pitch.present || pitch.valid) &&
        (!yaw_angle.present || yaw_angle.valid) &&
        (!pitch_angle.present || pitch_angle.valid) &&
        (!cone_angle.present || cone_angle.valid);
    if (!mode_read || !numeric_values_valid) {
        log_invalid_policy(*description,
                           "one or more angle values are malformed");
        return false;
    }

    ArcConfig config{};
    bool explicit_mode = false;
    if (mode_present) {
        trim_string(&mode_text);
        lower_string(&mode_text);
        explicit_mode = true;
        if (mode_text == "box") {
            config.mode = ArcMode::box;
        } else if (mode_text == "cone") {
            config.mode = ArcMode::cone;
        } else {
            log_invalid_policy(*description,
                               "fireArcMode must be 'box' or 'cone'");
            return false;
        }
    } else if (cone_angle.present) {
        config.mode = ArcMode::cone;
    }

    if (centre.present) config.yaw_degrees = centre.value;
    if (yaw.present) config.yaw_degrees = yaw.value;
    if (pitch.present) config.pitch_degrees = pitch.value;

    if (config.mode == ArcMode::cone) {
        if (!cone_angle.present) {
            log_invalid_policy(*description,
                               "cone mode requires fireArcAngle");
            return false;
        }
        config.cone_angle_degrees = cone_angle.value;
    } else {
        const bool has_box_coverage = width.present ||
            yaw_angle.present || pitch_angle.present;
        if (!has_box_coverage) {
            log_invalid_policy(
                *description,
                explicit_mode
                    ? "box mode requires fireArcWidth, fireArcYawAngle, or fireArcPitchAngle"
                    : "an arc centre requires a width/angle command");
            return false;
        }
        if (width.present) config.yaw_angle_degrees = width.value;
        if (yaw_angle.present) {
            config.yaw_angle_degrees = yaw_angle.value;
        }
        if (pitch_angle.present) {
            config.pitch_angle_degrees = pitch_angle.value;
        }
    }

    a2fo::fire_arcs::normalize_config(&config);
    *output = config;
    return true;
}

void register_class_arc(void* weapon_class,
                        void* parameter_db) noexcept {
    if (!g_runtime_ready || !weapon_class || !parameter_db) return;
    ArcConfig config{};
    std::string description;
    if (!build_arc_config(parameter_db, &config, &description)) return;

    try {
        g_class_arcs[weapon_class] = config;
    } catch (...) {
        log_line("Could not retain a weapon fire-arc policy");
        return;
    }

    char message[420]{};
    if (config.mode == ArcMode::cone) {
        std::snprintf(
            message, sizeof(message),
            "Registered cone fire arc on '%s': yaw %.1f, pitch %.1f, angle %.1f",
            description.c_str(), config.yaw_degrees,
            config.pitch_degrees, config.cone_angle_degrees);
    } else {
        std::snprintf(
            message, sizeof(message),
            "Registered box fire arc on '%s': yaw %.1f x %.1f, pitch %.1f x %.1f",
            description.c_str(), config.yaw_degrees,
            config.yaw_angle_degrees, config.pitch_degrees,
            config.pitch_angle_degrees);
    }
    log_line(message);
}

std::uintptr_t __attribute__((fastcall)) weapon_class_constructor_hook(
    void* self, void*, void* parent_class, void* parameter_db) noexcept {
    const std::uintptr_t result = a2fo_fire_arc_call_thiscall_2(
        g_weapon_class_constructor_original, self,
        reinterpret_cast<std::uintptr_t>(parent_class),
        reinterpret_cast<std::uintptr_t>(parameter_db));
    register_class_arc(self, parameter_db);
    return result;
}

bool call_native_can_fire_at(void* weapon, void* firing_context,
                             void* target, void* distance_output) noexcept {
    return (a2fo_fire_arc_call_thiscall_3(
                g_weapon_can_fire_at_original, weapon,
                reinterpret_cast<std::uintptr_t>(firing_context),
                reinterpret_cast<std::uintptr_t>(target),
                reinterpret_cast<std::uintptr_t>(distance_output)) &
            0xffu) != 0;
}

const ArcConfig* configured_arc(void* weapon,
                                void** weapon_class_output) noexcept {
    if (weapon_class_output) *weapon_class_output = nullptr;
    if (!g_runtime_ready || !weapon) return nullptr;
    void* weapon_class = read_at<void*>(
        weapon, kWeaponClassOnWeaponOffset, nullptr);
    if (weapon_class_output) *weapon_class_output = weapon_class;
    const auto policy = g_class_arcs.find(weapon_class);
    return policy == g_class_arcs.end() ? nullptr : &policy->second;
}

void draw_world_line(const ArcLine& line,
                     bool target_inside_arc) noexcept {
    if (!g_armada) return;
    // DisplayInterface::DrawLine accepts two world-space Vector3 values and
    // an RGB float triplet. The active camera performs projection/clipping.
    using DrawLineFn = void (__cdecl*)(const float*, const float*,
                                      const float*);
    const auto draw_line = reinterpret_cast<DrawLineFn>(
        at(g_armada, kDisplayInterfaceDrawLineRva));
    static const float boundary_color[3]{0.10f, 0.90f, 1.00f};
    static const float centre_color[3]{1.00f, 0.82f, 0.12f};
    static const float target_inside_color[3]{0.15f, 1.00f, 0.20f};
    const float* color = target_inside_arc
        ? target_inside_color
        : (line.style == ArcLineStyle::centre
              ? centre_color : boundary_color);
    draw_line(line.start.values, line.end.values, color);
}

float visualization_radius(void* weapon_class) noexcept {
    const float weapon_range = read_at<float>(
        weapon_class, kRangeOnWeaponClassOffset, 300.0f);
    if (!std::isfinite(weapon_range) || weapon_range <= 0.0f) {
        return 120.0f;
    }
    // The overlay explains angle, not range. A fraction of the real range
    // keeps its boundary on screen for long-range weapons while still scaling
    // sensibly between fighters, ships, and stations.
    return std::max(80.0f, std::min(500.0f, weapon_range * 0.35f));
}

void draw_arc_at_origin(const ArcConfig& policy,
                        const Matrix34& owner_transform,
                        const float origin[3], float radius,
                        bool target_inside_arc) noexcept {
    std::array<ArcLine, 128> lines{};
    const std::size_t line_count =
        a2fo::fire_arcs::build_visualization_lines(
            policy, owner_transform, origin, radius,
            lines.data(), lines.size());
    for (std::size_t index = 0; index < line_count; ++index) {
        draw_world_line(lines[index], target_inside_arc);
    }
}

void draw_hovered_weapon_arcs(void* weapon) noexcept {
    void* weapon_class = nullptr;
    const ArcConfig* policy = configured_arc(weapon, &weapon_class);
    if (!policy || !weapon_class) return;

    void* owner = reinterpret_cast<void*>(
        a2fo_fire_arc_call_thiscall_0(
            at(g_armada, kWeaponGetOwnerRva), weapon));
    const auto* live_owner_transform = owner
        ? reinterpret_cast<const Matrix34*>(
              a2fo_fire_arc_call_thiscall_0(
                  at(g_armada, kEntityGetTransformRva), owner))
        : nullptr;
    if (!readable_range(live_owner_transform, sizeof(Matrix34))) return;
    Matrix34 owner_transform{};
    std::memcpy(&owner_transform, live_owner_transform,
                sizeof(owner_transform));

    // Weapon::GetTarget resolves the native target handle and rejects stale
    // objects. Testing that live target against the same pure geometry used by
    // firing authorization keeps UI rendering read-only and non-reentrant.
    void* target = reinterpret_cast<void*>(
        a2fo_fire_arc_call_thiscall_0(
            at(g_armada, kWeaponGetTargetRva), weapon));
    const auto* target_position_address = target
        ? static_cast<const std::uint8_t*>(target) +
              kPositionOnGameObjectOffset
        : nullptr;
    bool target_inside_arc = false;
    if (readable_range(target_position_address, sizeof(float) * 3)) {
        float target_position[3]{};
        std::memcpy(target_position, target_position_address,
                    sizeof(target_position));
        target_inside_arc = a2fo::fire_arcs::allows_target(
            *policy, owner_transform, target_position);
    }

    const float radius = visualization_radius(weapon_class);
    void* sentinel = read_at<void*>(
        weapon, kHardpointListOnWeaponOffset, nullptr);
    void* node = read_at<void*>(sentinel, 0, nullptr);
    std::size_t hardpoint_count = 0;
    std::size_t nodes_visited = 0;
    while (sentinel && node && node != sentinel && nodes_visited < 64) {
        ++nodes_visited;
        void* hardpoint = read_at<void*>(
            node, kHardpointOnWeaponListNodeOffset, nullptr);
        Matrix34 hardpoint_transform{};
        if (hardpoint) {
            const std::uintptr_t result =
                a2fo_fire_arc_call_thiscall_2(
                    at(g_armada, kEntityGetWorldTransformRva), owner,
                    reinterpret_cast<std::uintptr_t>(&hardpoint_transform),
                    reinterpret_cast<std::uintptr_t>(hardpoint));
            if (result == reinterpret_cast<std::uintptr_t>(
                              &hardpoint_transform)) {
                draw_arc_at_origin(
                    *policy, owner_transform,
                    &hardpoint_transform.values[9], radius,
                    target_inside_arc);
                ++hardpoint_count;
            }
        }
        void* next = read_at<void*>(node, 0, nullptr);
        if (!next || next == node) break;
        node = next;
    }

    // A malformed or empty native hardpoint list must not make the hover
    // silently useless. Fall back to the weapon owner's centre in that case.
    if (hardpoint_count == 0) {
        draw_arc_at_origin(
            *policy, owner_transform, &owner_transform.values[9], radius,
            target_inside_arc);
    }
    if (InterlockedCompareExchange(
            &g_logged_first_hover_visualization, 1, 0) == 0) {
        char message[180]{};
        std::snprintf(
            message, sizeof(message),
            "Rendered first weapon-icon fire-arc hover (%u hardpoint%s)",
            static_cast<unsigned>(hardpoint_count),
            hardpoint_count == 1 ? "" : "s");
        log_line(message);
    }
}

void* weapon_for_ship_system_icon(void* icon) noexcept {
    if (!icon || !g_armada) return nullptr;
    const bool hovered = (a2fo_fire_arc_call_thiscall_0(
        at(g_armada, kStandardComponentIsMouseOverRva), icon) & 0xffu) != 0;
    if (!hovered) return nullptr;

    const std::int32_t weapon_index = read_at<std::int32_t>(
        icon, kWeaponIndexOnShipSystemIconOffset, -1);
    void* craft = read_at<void*>(
        icon, kCraftOnShipSystemIconOffset, nullptr);
    void* weapon_system = read_at<void*>(
        craft, kWeaponSystemOnCraftOffset, nullptr);
    void** begin = read_at<void**>(
        weapon_system, kWeaponVectorBeginOffset, nullptr);
    void** end = read_at<void**>(
        weapon_system, kWeaponVectorEndOffset, nullptr);
    const std::uintptr_t begin_address =
        reinterpret_cast<std::uintptr_t>(begin);
    const std::uintptr_t end_address =
        reinterpret_cast<std::uintptr_t>(end);
    const std::uintptr_t byte_count = end_address >= begin_address
        ? end_address - begin_address : 0;
    const std::size_t weapon_count = static_cast<std::size_t>(
        byte_count / sizeof(void*));
    if (weapon_index < 0 || !begin || !end ||
        end_address < begin_address || byte_count % sizeof(void*) != 0 ||
        weapon_count == 0 || weapon_count > 256 ||
        !readable_range(begin, weapon_count * sizeof(void*)) ||
        static_cast<std::size_t>(weapon_index) >= weapon_count) {
        return nullptr;
    }
    return read_at<void*>(begin, static_cast<std::size_t>(weapon_index) *
        sizeof(void*), nullptr);
}

void __attribute__((fastcall)) ship_system_icon_render_hook(
    void* icon, void*) noexcept {
    a2fo_fire_arc_call_thiscall_0(
        g_ship_system_icon_render_original, icon);
    if (!g_runtime_ready) return;
    void* weapon = weapon_for_ship_system_icon(icon);
    if (weapon) draw_hovered_weapon_arcs(weapon);
}

class ScopedNativeArcBypass {
public:
    explicit ScopedNativeArcBypass(void* weapon_class) noexcept {
        address_ = weapon_class
            ? static_cast<std::uint8_t*>(weapon_class) +
                  kRestrictFireArcOnWeaponClassOffset
            : nullptr;
        if (!writable_range(address_, sizeof(*address_))) {
            address_ = nullptr;
            return;
        }
        original_ = *address_;
        if (original_ == 0) {
            // Most custom weapons already leave the native restriction off.
            // Avoid writing shared class memory when no bypass is required.
            address_ = nullptr;
            return;
        }
        *address_ = 0;
    }

    ~ScopedNativeArcBypass() noexcept {
        if (address_) *address_ = original_;
    }

    ScopedNativeArcBypass(const ScopedNativeArcBypass&) = delete;
    ScopedNativeArcBypass& operator=(const ScopedNativeArcBypass&) = delete;

private:
    std::uint8_t* address_ = nullptr;
    std::uint8_t original_ = 0;
};

bool evaluate_arc_policy(void* weapon, const void* target,
                         const ArcConfig& policy) noexcept {
    // Geometry failure is deliberately fail-open. Native CanFireAt has already
    // validated the target; refusing every shot because one reverse-engineered
    // pointer is unavailable would be a much more damaging failure mode.
    if (!weapon) return true;
    void* owner = reinterpret_cast<void*>(
        a2fo_fire_arc_call_thiscall_0(
            at(g_armada, kWeaponGetOwnerRva), weapon));
    const auto* owner_transform = owner
        ? reinterpret_cast<const Matrix34*>(
              a2fo_fire_arc_call_thiscall_0(
                  at(g_armada, kEntityGetTransformRva), owner))
        : nullptr;
    const auto* target_position_address = target
        ? static_cast<const std::uint8_t*>(target) +
              kPositionOnGameObjectOffset
        : nullptr;
    if (!readable_range(owner_transform, sizeof(Matrix34)) ||
        !readable_range(target_position_address, sizeof(float) * 3)) {
        return true;
    }

    // Copy both values before doing any floating-point work. Engine-owned
    // pointers are not retained beyond this synchronous callback.
    Matrix34 owner_copy{};
    float target_position[3]{};
    std::memcpy(&owner_copy, owner_transform, sizeof(owner_copy));
    std::memcpy(target_position, target_position_address,
                sizeof(target_position));
    return a2fo::fire_arcs::allows_target(
        policy, owner_copy, target_position);
}

bool evaluate_custom_arc(void* weapon, const void* target,
                         bool* configured) noexcept {
    if (configured) *configured = false;
    if (!g_runtime_ready || !weapon) return true;

    const ArcConfig* policy = configured_arc(weapon, nullptr);
    if (!policy) return true;
    if (configured) *configured = true;
    return evaluate_arc_policy(weapon, target, *policy);
}

bool __attribute__((fastcall)) weapon_can_fire_at_hook(
    void* weapon, void*, void* firing_context, void* target,
    void* distance_output) noexcept {
    void* weapon_class = nullptr;
    const ArcConfig* policy = configured_arc(weapon, &weapon_class);
    if (!policy) {
        return call_native_can_fire_at(
            weapon, firing_context, target, distance_output);
    }

    // A custom arc owns only the directional decision. Temporarily disable
    // Armada's stock restrictFireArc gate while its CanFireAt path performs
    // the range, obstruction, and target-validity checks. There is no separate
    // native entry for those non-directional checks. Fleet Ops serializes this
    // simulation path; the scoped write is restored before control returns to
    // any other weapon. Weapons without a custom policy never enter this path.
    bool native_allowed = false;
    {
        ScopedNativeArcBypass bypass(weapon_class);
        native_allowed = call_native_can_fire_at(
            weapon, firing_context, target, distance_output);
    }
    if (!native_allowed) return false;

    // Use the policy found before the native call. WeaponClass policies are
    // immutable once startup class construction has completed.
    const bool direction_allowed = evaluate_arc_policy(
        weapon, target, *policy);
    if (InterlockedCompareExchange(
            &g_logged_first_custom_check, 1, 0) == 0) {
        log_line("First configured weapon target check reached");
    }
    volatile LONG* logged_result = direction_allowed
        ? &g_logged_first_direction_allowed_target
        : &g_logged_first_direction_rejected_target;
    if (InterlockedCompareExchange(logged_result, 1, 0) == 0) {
        log_line(direction_allowed
            ? "Custom 3D arc accepted its first target authorization"
            : "Custom 3D arc rejected its first target authorization");
    }
    return direction_allowed;
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    const void* address = at(module, rva);
    return readable_range(address, Size) &&
        std::memcmp(address, expected, Size) == 0;
}

void* existing_detour_destination(const void* site,
                                  std::size_t* patch_length) noexcept {
    if (patch_length) *patch_length = 0;
    if (!site || !readable_range(site, 5)) return nullptr;
    const auto* bytes = static_cast<const std::uint8_t*>(site);
    if (bytes[0] == 0xe9) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, bytes + 1, sizeof(displacement));
        if (patch_length) *patch_length = 5;
        return const_cast<std::uint8_t*>(bytes + 5 + displacement);
    }
    if (readable_range(site, 6) && bytes[0] == 0x68 &&
        bytes[5] == 0xc3) {
        std::uint32_t destination = 0;
        std::memcpy(&destination, bytes + 1, sizeof(destination));
        if (patch_length) *patch_length = 6;
        return reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(destination));
    }
    return nullptr;
}

bool supported_fleet_ops_weapon_class_detour(
    void** destination, std::size_t* patch_length) noexcept {
    void* resolved = existing_detour_destination(
        at(g_armada, kWeaponClassConstructorRva), patch_length);
    void* expected = at(g_fleet_ops,
                        kFoWeaponClassConstructorHandlerRva);
    const bool supported = resolved == expected &&
        signature_matches(
            g_fleet_ops, kFoWeaponClassConstructorHandlerRva,
            kExpectedFoWeaponClassConstructorHandler);
    if (destination) *destination = supported ? resolved : nullptr;
    return supported;
}

bool supported_fleet_ops_weapon_can_fire_at_detour(
    void** destination, std::size_t* patch_length) noexcept {
    void* resolved = existing_detour_destination(
        at(g_armada, kWeaponCanFireAtRva), patch_length);
    void* expected = at(g_fleet_ops,
                        kFoWeaponCanFireAtHandlerRva);
    const bool supported = resolved == expected &&
        signature_matches(
            g_fleet_ops, kFoWeaponCanFireAtHandlerRva,
            kExpectedFoWeaponCanFireAtHandler);
    if (destination) *destination = supported ? resolved : nullptr;
    return supported;
}

bool weapon_class_constructor_supported() noexcept {
    if (signature_matches(
            g_armada, kWeaponClassConstructorRva,
            kExpectedWeaponClassConstructor)) {
        return true;
    }
    void* destination = nullptr;
    std::size_t patch_length = 0;
    if (supported_fleet_ops_weapon_class_detour(
            &destination, &patch_length)) {
        return true;
    }
    char message[360]{};
    std::snprintf(
        message, sizeof(message),
        "WeaponClass constructor has neither the stock prologue nor Fleet "
        "Ops' supported detour (destination=%p, expected=%p)",
        existing_detour_destination(
            at(g_armada, kWeaponClassConstructorRva), nullptr),
        at(g_fleet_ops, kFoWeaponClassConstructorHandlerRva));
    log_line(message);
    return false;
}

bool weapon_can_fire_at_supported() noexcept {
    if (signature_matches(
            g_armada, kWeaponCanFireAtRva,
            kExpectedWeaponCanFireAt)) {
        return true;
    }
    void* destination = nullptr;
    std::size_t patch_length = 0;
    if (supported_fleet_ops_weapon_can_fire_at_detour(
            &destination, &patch_length)) {
        return true;
    }
    char message[360]{};
    std::snprintf(
        message, sizeof(message),
        "Weapon target authorization has neither the stock prologue nor "
        "Fleet Ops' supported detour (destination=%p, expected=%p)",
        existing_detour_destination(
            at(g_armada, kWeaponCanFireAtRva), nullptr),
        at(g_fleet_ops, kFoWeaponCanFireAtHandlerRva));
    log_line(message);
    return false;
}

bool preflight_signatures() noexcept {
    bool supported = weapon_class_constructor_supported();
    supported = weapon_can_fire_at_supported() && supported;
    if (!readable_range(at(g_armada, kParameterDbGetStringRva), 5)) {
        log_line("ParameterDB::GetString entry is unreadable");
        supported = false;
    }
    if (!readable_range(at(g_armada, kParameterDbGetFloatRva), 5)) {
        log_line("ParameterDB::GetFloat entry is unreadable");
        supported = false;
    }
    if (!readable_range(at(g_armada, kEntityGetTransformRva), 5)) {
        log_line("Entity::GetTransform entry is unreadable");
        supported = false;
    }
    if (!readable_range(at(g_armada, kWeaponGetOwnerRva), 5)) {
        log_line("Weapon::GetOwner entry is unreadable");
        supported = false;
    }
    if (!signature_matches(
            g_armada, kWeaponGetTargetRva,
            kExpectedWeaponGetTarget)) {
        log_line("Weapon::GetTarget signature is unsupported");
        supported = false;
    }
    if (!signature_matches(
            g_armada, kEntityGetWorldTransformRva,
            kExpectedEntityGetWorldTransform)) {
        log_line("Entity::GetWorldTransform signature is unsupported");
        supported = false;
    }
    if (!signature_matches(
            g_armada, kDisplayInterfaceDrawLineRva,
            kExpectedDisplayInterfaceDrawLine)) {
        log_line("DisplayInterface::DrawLine signature is unsupported");
        supported = false;
    }
    if (!signature_matches(
            g_armada, kStandardComponentIsMouseOverRva,
            kExpectedStandardComponentIsMouseOver)) {
        log_line("StandardComponent mouse-over signature is unsupported");
        supported = false;
    }
    if (!signature_matches(
            g_fleet_ops, kFoShipSystemIconRenderRva,
            kExpectedFoShipSystemIconRender)) {
        log_line("Fleet Operations ShipSystemIcon render signature is unsupported");
        supported = false;
    }
    return supported;
}

bool install_ship_system_icon_render_hook(
    const A2FO_ModuleApi* api) noexcept {
    if (!api->install_inline_hook(
            at(g_fleet_ops, kFoShipSystemIconRenderRva),
            reinterpret_cast<void*>(&ship_system_icon_render_hook),
            sizeof(kExpectedFoShipSystemIconRender),
            kExpectedFoShipSystemIconRender,
            &g_ship_system_icon_render_hook)) {
        return false;
    }
    g_ship_system_icon_render_original =
        g_ship_system_icon_render_hook.gateway;
    return g_ship_system_icon_render_original != nullptr;
}

bool install_weapon_class_constructor_hook(
    const A2FO_ModuleApi* api) noexcept {
    void* site = at(g_armada, kWeaponClassConstructorRva);
    if (signature_matches(
            g_armada, kWeaponClassConstructorRva,
            kExpectedWeaponClassConstructor)) {
        if (!api->install_inline_hook(
                site,
                reinterpret_cast<void*>(
                    &weapon_class_constructor_hook),
                sizeof(kExpectedWeaponClassConstructor),
                kExpectedWeaponClassConstructor,
                &g_weapon_class_constructor_hook)) {
            return false;
        }
        g_weapon_class_constructor_original =
            g_weapon_class_constructor_hook.gateway;
        g_chained_fo_weapon_class_constructor = false;
        return g_weapon_class_constructor_original != nullptr;
    }

    void* destination = nullptr;
    std::size_t patch_length = 0;
    if (!supported_fleet_ops_weapon_class_detour(
            &destination, &patch_length) || patch_length > 6) {
        return false;
    }
    std::uint8_t expected_detour[6]{};
    std::memcpy(expected_detour, site, patch_length);
    if (!api->patch_jump(
            site,
            reinterpret_cast<void*>(&weapon_class_constructor_hook),
            expected_detour, patch_length)) {
        return false;
    }
    g_weapon_class_constructor_original = destination;
    g_chained_fo_weapon_class_constructor = true;
    return true;
}

bool install_weapon_can_fire_at_hook(
    const A2FO_ModuleApi* api) noexcept {
    void* site = at(g_armada, kWeaponCanFireAtRva);
    if (signature_matches(
            g_armada, kWeaponCanFireAtRva,
            kExpectedWeaponCanFireAt)) {
        if (!api->install_inline_hook(
                site,
                reinterpret_cast<void*>(&weapon_can_fire_at_hook),
                sizeof(kExpectedWeaponCanFireAt),
                kExpectedWeaponCanFireAt,
                &g_weapon_can_fire_at_hook)) {
            return false;
        }
        g_weapon_can_fire_at_original =
            g_weapon_can_fire_at_hook.gateway;
        g_chained_fo_weapon_can_fire_at = false;
        return g_weapon_can_fire_at_original != nullptr;
    }

    void* destination = nullptr;
    std::size_t patch_length = 0;
    if (!supported_fleet_ops_weapon_can_fire_at_detour(
            &destination, &patch_length) || patch_length > 6) {
        return false;
    }
    std::uint8_t expected_detour[6]{};
    std::memcpy(expected_detour, site, patch_length);
    if (!api->patch_jump(
            site, reinterpret_cast<void*>(&weapon_can_fire_at_hook),
            expected_detour, patch_length)) {
        return false;
    }
    g_weapon_can_fire_at_original = destination;
    g_chained_fo_weapon_can_fire_at = true;
    return true;
}

bool install_runtime_hooks(const A2FO_ModuleApi* api) noexcept {
    if (!api || !api->install_inline_hook || !api->patch_jump ||
        !g_armada || !g_fleet_ops) {
        return false;
    }
    if (!preflight_signatures()) {
        log_line(
            "Supported ArmadaL signatures were not found; runtime disabled");
        return false;
    }

    // Install the UI hook first. If either simulation hook then fails,
    // runtime_ready remains false and this process-lifetime detour is a pure
    // pass-through rather than exposing a partial visual/runtime feature.
    bool installed = install_ship_system_icon_render_hook(api);
    installed = install_weapon_can_fire_at_hook(api) && installed;
    installed = install_weapon_class_constructor_hook(api) && installed;
    if (!installed) {
        log_line(
            "A fire-arc hook could not be installed; hooks fail closed");
    }
    return installed;
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->install_inline_hook || !api->patch_jump) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !g_fleet_ops) return false;

    g_runtime_ready = install_runtime_hooks(api);
    if (g_runtime_ready) {
        if (g_chained_fo_weapon_class_constructor &&
            g_chained_fo_weapon_can_fire_at) {
            log_line(
                "3D fire-arc runtime initialized and chained through Fleet "
                "Operations WeaponClass construction and target authorization; "
                "weapon-icon hover visualization active");
        } else if (g_chained_fo_weapon_class_constructor) {
            log_line("3D fire-arc runtime initialized and chained through Fleet Operations WeaponClass construction");
        } else if (g_chained_fo_weapon_can_fire_at) {
            log_line("3D fire-arc runtime initialized and chained through Fleet Operations target authorization");
        } else {
            log_line("3D fire-arc runtime initialized");
        }
    } else {
        log_line("Fire-arc module loaded with runtime disabled");
    }
    // Inline hooks are process-lifetime patches. A partially installed module
    // remains resident; with runtime_ready false each hook is pass-through.
    return true;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FOFireArcs_AllowWeaponTrigger(
    void* weapon, const void* target) {
    bool configured = false;
    const bool allowed = evaluate_custom_arc(
        weapon, target, &configured);
    if (!configured) return true;

    volatile LONG* logged_result = allowed
        ? &g_logged_first_trigger_allowed_target
        : &g_logged_first_trigger_rejected_target;
    if (InterlockedCompareExchange(logged_result, 1, 0) == 0) {
        log_line(allowed
            ? "Full 3D fire arc allowed its first weapon trigger"
            : "Full 3D fire arc suppressed its first weapon trigger");
    }
    return allowed;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Process-lifetime inline hooks are intentionally not removed.
}
