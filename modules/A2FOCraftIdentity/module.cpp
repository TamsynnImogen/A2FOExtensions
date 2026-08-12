/*
 * ODF-driven captain names and craft registries for Fleet Operations.
 *
 * Craft classes may declare possibleCaptainNames and possibleCraftRegistry.
 * Both companion lists use the exact entry Fleet Operations selected from
 * possibleCraftNames, including after save/load or a native rename. The
 * existing selected-object captain rectangle is activated and a matching
 * registry rectangle is added beside it.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "identity_selection.hpp"

#include <windows.h>

#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" {
std::uintptr_t __cdecl a2fo_identity_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_identity_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
std::uintptr_t __cdecl a2fo_identity_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
std::uintptr_t __cdecl a2fo_identity_call_thiscall_7(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4, std::uintptr_t argument5,
    std::uintptr_t argument6, std::uintptr_t argument7);
}

namespace {

using ShieldClassObserver = void (A2FO_CALL *)(
    void* object_class, void* parameter_db);
using NebulaClassObserver = void (A2FO_CALL *)(
    void* object_class, void* parameter_db);

constexpr char kModuleName[] = "A2FOCraftIdentity";
constexpr char kCraftNameCommand[] = "possibleCraftNames";
constexpr char kCaptainCommand[] = "possibleCaptainNames";
constexpr char kRegistryCommand[] = "possibleCraftRegistry";
constexpr char kAlwaysShowShieldsModuleName[] =
    "A2FOAlwaysShowShields.dll";
constexpr char kShieldClassObserverExport[] =
    "A2FOAlwaysShowShields_RegisterClass";
constexpr char kNebulaRendererModuleName[] = "A2FONebulaRenderer.dll";
constexpr char kNebulaClassObserverExport[] =
    "A2FONebulaRenderer_RegisterClass";
constexpr std::size_t kMaximumIdentityEntries = 4096;
constexpr std::size_t kMaximumIdentityLength = 512;

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kCraftClassConstructorRva = 0x000bf090;
constexpr std::uintptr_t kSelectedInfoRenderRva = 0x000f3770;
constexpr std::uintptr_t kGameObjectClassGetOdfNameRva = 0x000ce370;
constexpr std::uintptr_t kParameterDbGetRectangleRva = 0x001358f0;
constexpr std::uintptr_t kParameterDbGetColorRva = 0x00135ba0;
constexpr std::uintptr_t kParameterDbGetStringVectorRva = 0x00135e80;
constexpr std::uintptr_t kDisplayInterfaceDrawTextInRectangleRva = 0x0011b160;
constexpr std::uintptr_t kEngineOperatorDeleteRva = 0x002527d0;
constexpr std::uintptr_t kGuiParameterDbPointerRva = 0x0036502c;

// Fleet Operations detours CraftClass(ParameterDB) before extension modules
// load. Chain through the one supported handler just as the turret module
// chains Fleet Operations' GameObjectClass and Craft::Simulate handlers.
constexpr std::uintptr_t kFoCraftClassConstructorHandlerRva = 0x0010d6e4;

constexpr std::size_t kObjectHandleOffset = 0x28;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kCraftNameIndexOffset = 0x218;
constexpr std::size_t kInfoDisplaySelectedCraftOffset = 0x1e8;
constexpr std::size_t kInfoDisplayCaptainTextOffset = 0xbc;
constexpr std::size_t kTextComponentLiveRectangleOffset = 0x58;

constexpr std::uint8_t kExpectedCraftClassConstructor[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uint8_t kExpectedFoCraftClassConstructorHandler[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf8, 0x53};
constexpr std::uint8_t kExpectedSelectedInfoRender[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x08};
constexpr std::uint8_t kExpectedParameterDbGetRectangle[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x04, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedParameterDbGetColor[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedDisplayInterfaceDrawTextInRectangle[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};
constexpr std::uint8_t kExpectedGameObjectClassGetOdfName[] = {
    0x8b, 0x89, 0xcc, 0x01, 0x00, 0x00};

struct NativeStringVector {
    std::uint32_t allocator = 0;
    char** begin = nullptr;
    char** end = nullptr;
    char** capacity = nullptr;
};
static_assert(sizeof(NativeStringVector) == 16,
              "Armada string-vector ABI must occupy sixteen bytes");

struct ClassIdentityPolicy {
    std::string odf_name;
    std::vector<std::string> craft_names;
    std::vector<std::string> captain_names;
    std::vector<std::string> craft_registries;
};

struct CraftIdentity {
    void* object_class = nullptr;
    std::int32_t craft_name_index = -1;
    std::string captain_name;
    std::string craft_registry;
};

struct RawRectangle {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

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
static_assert(sizeof(Colour) == 12,
              "ST3D_Colour must contain three floats");

struct UiConfiguration {
    void* parameter_db = nullptr;
    bool loaded = false;
    bool captain_rectangle_found = false;
    bool registry_rectangle_found = false;
    RawRectangle captain_rectangle{};
    RawRectangle registry_rectangle{};
    bool ship_name_colour_found = false;
    bool captain_colour_found = false;
    bool registry_colour_found = false;
    Colour ship_name_colour{};
    Colour captain_colour{};
    Colour registry_colour{};
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
ShieldClassObserver g_shield_class_observer = nullptr;
NebulaClassObserver g_nebula_class_observer = nullptr;
bool g_runtime_ready = false;
bool g_chained_fo_craft_class_constructor = false;
void* g_craft_class_constructor_original = nullptr;
A2FO_InlineHook g_craft_class_constructor_hook{};
A2FO_InlineHook g_selected_info_render_hook{};
std::unordered_map<void*, ClassIdentityPolicy> g_class_policies;
std::unordered_map<std::uint32_t, CraftIdentity> g_craft_identities;
UiConfiguration g_ui_configuration{};
LONG g_assignment_report_count = 0;
LONG g_draw_report_count = 0;

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

void resolve_shield_class_observer() noexcept {
    HMODULE shields = GetModuleHandleA(kAlwaysShowShieldsModuleName);
    FARPROC exported = shields
        ? GetProcAddress(shields, kShieldClassObserverExport)
        : nullptr;
    static_assert(sizeof(exported) == sizeof(g_shield_class_observer),
                  "unexpected function-pointer size");
    std::memcpy(&g_shield_class_observer, &exported,
                sizeof(g_shield_class_observer));
    if (g_shield_class_observer) {
        log_line(
            "Shield class registration linked through "
            "A2FOAlwaysShowShields");
    }
}

void resolve_nebula_class_observer() noexcept {
    HMODULE renderer = GetModuleHandleA(kNebulaRendererModuleName);
    FARPROC exported = renderer
        ? GetProcAddress(renderer, kNebulaClassObserverExport)
        : nullptr;
    static_assert(sizeof(exported) == sizeof(g_nebula_class_observer),
                  "unexpected function-pointer size");
    std::memcpy(&g_nebula_class_observer, &exported,
                sizeof(g_nebula_class_observer));
    if (g_nebula_class_observer) {
        log_line(
            "Emissive-map class registration linked through "
            "A2FONebulaRenderer");
    }
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(module) + rva)
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

template <typename T>
T read_at(const void* object, std::size_t offset, T fallback = T{}) noexcept {
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

bool copy_native_string(const char* source, std::string* output) {
    if (!source || !output) return false;
    std::size_t length = 0;
    while (length < kMaximumIdentityLength) {
        if (!readable_range(source + length, 1)) return false;
        if (source[length] == '\0') {
            output->assign(source, length);
            trim_string(output);
            return true;
        }
        ++length;
    }
    return false;
}

bool valid_vector_bounds(const NativeStringVector& values,
                         std::size_t* count) noexcept {
    if (count) *count = 0;
    if (!values.begin && !values.end) return true;
    if (!values.begin || !values.end) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(values.begin);
    const auto end = reinterpret_cast<std::uintptr_t>(values.end);
    if (end < begin || (end - begin) % sizeof(char*) != 0) return false;
    const std::size_t entries = (end - begin) / sizeof(char*);
    if (entries > kMaximumIdentityEntries) return false;
    if (entries != 0 && !readable_range(
            values.begin, entries * sizeof(char*))) {
        return false;
    }
    if (count) *count = entries;
    return true;
}

void release_native_vector(NativeStringVector* values) noexcept {
    if (!values || !g_armada) return;
    std::size_t count = 0;
    if (!valid_vector_bounds(*values, &count)) return;
    using DeleteFn = void (__cdecl*)(void*);
    const auto engine_delete = reinterpret_cast<DeleteFn>(
        at(g_armada, kEngineOperatorDeleteRva));
    if (!engine_delete) return;
    for (std::size_t index = 0; index < count; ++index) {
        if (values->begin[index]) engine_delete(values->begin[index]);
    }
    if (values->begin) engine_delete(values->begin);
    *values = NativeStringVector{};
}

bool read_identity_list(void* parameter_db, const char* command,
                        std::vector<std::string>* output) noexcept {
    if (!parameter_db || !command || !output || !g_armada) return false;
    NativeStringVector native_values{};
    const std::uintptr_t found = a2fo_identity_call_thiscall_3(
        at(g_armada, kParameterDbGetStringVectorRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(command),
        reinterpret_cast<std::uintptr_t>(&native_values), 0);
    bool copied = false;
    try {
        std::size_t count = 0;
        if (valid_vector_bounds(native_values, &count)) {
            output->resize(count);
            for (std::size_t index = 0; index < count; ++index) {
                copy_native_string(native_values.begin[index],
                                   &(*output)[index]);
            }
            copied = !output->empty();
        }
    } catch (...) {
        output->clear();
        log_line(std::string("Could not retain ") + command + " values");
    }
    release_native_vector(&native_values);
    return (found & 0xffu) != 0 && copied;
}

std::string class_odf_name(void* object_class) {
    if (!object_class || !g_armada) return {};
    const char* name = reinterpret_cast<const char*>(
        a2fo_identity_call_thiscall_0(
            at(g_armada, kGameObjectClassGetOdfNameRva), object_class));
    std::string copied;
    if (copy_native_string(name, &copied)) return copied;
    return {};
}

void register_class_policy(void* object_class, void* parameter_db) noexcept {
    if (!g_runtime_ready || !object_class || !parameter_db) return;
    ClassIdentityPolicy policy{};
    read_identity_list(parameter_db, kCraftNameCommand,
                       &policy.craft_names);
    read_identity_list(parameter_db, kCaptainCommand,
                       &policy.captain_names);
    read_identity_list(parameter_db, kRegistryCommand,
                       &policy.craft_registries);
    if (policy.captain_names.empty() && policy.craft_registries.empty()) {
        return;
    }
    try {
        policy.odf_name = class_odf_name(object_class);
        const std::size_t craft_name_count = policy.craft_names.size();
        const std::size_t captain_count = policy.captain_names.size();
        const std::size_t registry_count = policy.craft_registries.size();
        const std::string odf = policy.odf_name;
        g_class_policies[object_class] = std::move(policy);
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "Registered identity rows on '%s': %lu craft name%s, "
            "%lu captain%s, %lu registr%s",
            odf.empty() ? "<unknown>" : odf.c_str(),
            static_cast<unsigned long>(craft_name_count),
            craft_name_count == 1 ? "" : "s",
            static_cast<unsigned long>(captain_count),
            captain_count == 1 ? "" : "s",
            static_cast<unsigned long>(registry_count),
            registry_count == 1 ? "y" : "ies");
        log_line(message);
        if (craft_name_count == 0) {
            log_line(std::string("Identity rows on '") +
                     (odf.empty() ? "<unknown>" : odf) +
                     "' cannot be selected without possibleCraftNames");
        } else if ((!g_class_policies[object_class].captain_names.empty() &&
                    captain_count != craft_name_count) ||
                   (!g_class_policies[object_class].craft_registries.empty() &&
                    registry_count != craft_name_count)) {
            log_line(std::string("Identity row-count mismatch on '") +
                     (odf.empty() ? "<unknown>" : odf) +
                     "'; out-of-range companion rows will remain blank");
        }
    } catch (...) {
        log_line("Could not retain a craft identity class policy");
    }
}

std::uintptr_t __attribute__((fastcall)) craft_class_constructor_hook(
    void* self, void*, void* parent_class, void* parameter_db) noexcept {
    const std::uintptr_t result = a2fo_identity_call_thiscall_2(
        g_craft_class_constructor_original, self,
        reinterpret_cast<std::uintptr_t>(parent_class),
        reinterpret_cast<std::uintptr_t>(parameter_db));
    register_class_policy(self, parameter_db);
    // CraftClass is the common completed-ODF boundary for both ships and
    // stations. Forward it to the optional shield module because the more
    // general GameObjectClass constructor is not entered by every derived
    // Fleet Operations class path.
    if (g_shield_class_observer) {
        g_shield_class_observer(self, parameter_db);
    }
    // A2FONebulaRenderer loads after this module alphabetically. Resolve its
    // optional observer lazily at the first completed CraftClass rather than
    // requiring a fragile module order or a second constructor hook.
    if (!g_nebula_class_observer) resolve_nebula_class_observer();
    if (g_nebula_class_observer) {
        g_nebula_class_observer(self, parameter_db);
    }
    return result;
}

const CraftIdentity* ensure_craft_identity(void* craft) noexcept {
    if (!craft) return nullptr;
    const std::uint32_t handle = read_at<std::uint32_t>(
        craft, kObjectHandleOffset, 0);
    void* object_class = read_at<void*>(craft, kObjectClassOffset, nullptr);
    const std::int32_t craft_name_index = read_at<std::int32_t>(
        craft, kCraftNameIndexOffset, -1);
    if (handle == 0 || !object_class || craft_name_index < 0) return nullptr;

    const auto policy = g_class_policies.find(object_class);
    if (policy == g_class_policies.end()) {
        g_craft_identities.erase(handle);
        return nullptr;
    }
    const auto present = g_craft_identities.find(handle);
    if (present != g_craft_identities.end() &&
        present->second.object_class == object_class &&
        present->second.craft_name_index == craft_name_index) {
        return &present->second;
    }

    try {
        CraftIdentity identity{};
        identity.object_class = object_class;
        identity.craft_name_index = craft_name_index;
        const ClassIdentityPolicy& class_policy = policy->second;
        std::size_t aligned_index = 0;
        if (a2fo::craft_identity::aligned_identity_index(
                craft_name_index, class_policy.captain_names.size(),
                &aligned_index)) {
            identity.captain_name =
                class_policy.captain_names[aligned_index];
        }
        if (a2fo::craft_identity::aligned_identity_index(
                craft_name_index, class_policy.craft_registries.size(),
                &aligned_index)) {
            identity.craft_registry =
                class_policy.craft_registries[aligned_index];
        }
        auto inserted = g_craft_identities.insert_or_assign(
            handle, std::move(identity));
        const LONG report = InterlockedIncrement(&g_assignment_report_count);
        if (report <= 32) {
            char message[512]{};
            std::snprintf(
                message, sizeof(message),
                "Aligned craft %lu from '%s' to native name row %ld "
                "('%s'): captain='%s', registry='%s'",
                static_cast<unsigned long>(handle),
                class_policy.odf_name.empty()
                    ? "<unknown>" : class_policy.odf_name.c_str(),
                static_cast<long>(craft_name_index),
                static_cast<std::size_t>(craft_name_index) <
                        class_policy.craft_names.size()
                    ? class_policy.craft_names[
                          static_cast<std::size_t>(craft_name_index)].c_str()
                    : "<out-of-range>",
                inserted.first->second.captain_name.empty()
                    ? "<none>" : inserted.first->second.captain_name.c_str(),
                inserted.first->second.craft_registry.empty()
                    ? "<none>" : inserted.first->second.craft_registry.c_str());
            log_line(message);
        } else if (report == 33) {
            log_line("Further per-craft identity assignment reports suppressed");
        }
        return &inserted.first->second;
    } catch (...) {
        g_craft_identities.erase(handle);
        log_line("Could not allocate per-craft identity state");
        return nullptr;
    }
}

void* gui_parameter_db() noexcept {
    return read_at<void*>(at(g_armada, kGuiParameterDbPointerRva), 0,
                          nullptr);
}

bool read_ui_rectangle(void* parameter_db, const char* key,
                       RawRectangle* output) noexcept {
    if (!parameter_db || !key || !output) return false;
    const RawRectangle fallback{};
    const std::uintptr_t found = a2fo_identity_call_thiscall_3(
        at(g_armada, kParameterDbGetRectangleRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(output),
        reinterpret_cast<std::uintptr_t>(&fallback));
    return (found & 0xffu) != 0;
}

bool read_ui_colour(void* parameter_db, const char* key,
                    Colour* output) noexcept {
    if (!parameter_db || !key || !output) return false;
    const Colour fallback{};
    const std::uintptr_t found = a2fo_identity_call_thiscall_3(
        at(g_armada, kParameterDbGetColorRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(output),
        reinterpret_cast<std::uintptr_t>(&fallback));
    return (found & 0xffu) != 0;
}

void refresh_ui_configuration(void* parameter_db) noexcept {
    UiConfiguration loaded{};
    loaded.parameter_db = parameter_db;
    loaded.loaded = true;
    if (parameter_db) {
        loaded.captain_rectangle_found = read_ui_rectangle(
            parameter_db, "infoSingleCaptainTextArea",
            &loaded.captain_rectangle);
        if (!loaded.captain_rectangle_found) {
            loaded.captain_rectangle_found = read_ui_rectangle(
                parameter_db, "captainName", &loaded.captain_rectangle);
        }
        loaded.registry_rectangle_found = read_ui_rectangle(
            parameter_db, "infoSingleRegistryTextArea",
            &loaded.registry_rectangle);
        if (!loaded.registry_rectangle_found) {
            loaded.registry_rectangle_found = read_ui_rectangle(
                parameter_db, "shipRegistry", &loaded.registry_rectangle);
        }
        loaded.ship_name_colour_found = read_ui_colour(
            parameter_db, "infoTextColor", &loaded.ship_name_colour);
        if (!loaded.ship_name_colour_found) {
            loaded.ship_name_colour_found = read_ui_colour(
                parameter_db, "shipNameColor", &loaded.ship_name_colour);
        }
        loaded.captain_colour_found = read_ui_colour(
            parameter_db, "captainNameColor", &loaded.captain_colour);
        loaded.registry_colour_found = read_ui_colour(
            parameter_db, "shipRegistryColor", &loaded.registry_colour);
    }
    g_ui_configuration = loaded;

    char message[256]{};
    std::snprintf(
        message, sizeof(message),
        "Selected identity UI: infoSingleCaptainTextArea=%s, "
        "infoSingleRegistryTextArea=%s",
        loaded.captain_rectangle_found ? "configured" : "absent",
        loaded.registry_rectangle_found ? "configured" : "absent");
    log_line(message);
}

bool usable_native_rectangle(const NativeRectangle& rectangle) noexcept {
    return rectangle.right > rectangle.left &&
        rectangle.bottom > rectangle.top;
}

Colour text_component_colour(const void* text_component) noexcept {
    Colour colour = read_at<Colour>(text_component, 0x70, Colour{});
    if (!std::isfinite(colour.red) || !std::isfinite(colour.green) ||
        !std::isfinite(colour.blue)) {
        return Colour{};
    }
    return colour;
}

bool draw_identity_text(const std::string& value,
                        const NativeRectangle& render_rectangle,
                        const Colour& colour,
                        void* text_component) noexcept {
    if (value.empty() || !usable_native_rectangle(render_rectangle) ||
        !text_component) {
        return false;
    }

    // Reuse the live native captain component's display/font state. Calling
    // the same rectangle-aware path as GUIText installs the requested
    // rectangle and preserves Fleet Operations' font, scaling and clipping
    // behaviour.
    void* display_interface = read_at<void*>(
        text_component, 0x04, nullptr);
    if (!display_interface) return false;
    void* display_override = nullptr;
    void* display_slot = read_at<void*>(text_component, 0x28, nullptr);
    if (display_slot) {
        display_override = read_at<void*>(display_slot, 0, nullptr);
    }
    const std::int32_t text_flags = read_at<std::int32_t>(
        text_component, 0x68, 9);
    const std::uint8_t constrain_to_rectangle = read_at<std::uint8_t>(
        text_component, 0x6c, 0);
    void* font_state = static_cast<std::uint8_t*>(text_component) + 0x7c;
    if (!readable_range(font_state, 12)) return false;

    a2fo_identity_call_thiscall_7(
        at(g_armada, kDisplayInterfaceDrawTextInRectangleRva),
        display_interface,
        reinterpret_cast<std::uintptr_t>(value.c_str()),
        reinterpret_cast<std::uintptr_t>(&render_rectangle),
        static_cast<std::uintptr_t>(text_flags),
        reinterpret_cast<std::uintptr_t>(&colour),
        reinterpret_cast<std::uintptr_t>(display_override),
        static_cast<std::uintptr_t>(constrain_to_rectangle),
        reinterpret_cast<std::uintptr_t>(font_state));
    return true;
}

void __attribute__((fastcall)) selected_info_render_hook(
    void* info_display, void*) noexcept {
    a2fo_identity_call_thiscall_0(
        g_selected_info_render_hook.gateway, info_display);
    if (!g_runtime_ready || !info_display) return;

    // The selected-info render pass resolves the one selected object into
    // +0x1e8 before drawing its middle/tall panel. Multiple selection and no
    // selection both leave this null, so identities never leak back into
    // SDInfoBar mouseover. Drawing here also retains the selected panel's live
    // display/scissor state; the later generic info-display pass has already
    // closed that context.
    void* craft = read_at<void*>(
        info_display, kInfoDisplaySelectedCraftOffset, nullptr);
    if (!craft) return;

    void* parameter_db = gui_parameter_db();
    if (!g_ui_configuration.loaded ||
        g_ui_configuration.parameter_db != parameter_db) {
        refresh_ui_configuration(parameter_db);
    }
    const CraftIdentity* identity = ensure_craft_identity(craft);
    if (!identity) return;

    void* captain_text = read_at<void*>(
        info_display, kInfoDisplayCaptainTextOffset, nullptr);
    if (!captain_text) return;
    const NativeRectangle captain_native_rectangle =
        read_at<NativeRectangle>(captain_text,
                                 kTextComponentLiveRectangleOffset,
                                 NativeRectangle{});
    if (!usable_native_rectangle(captain_native_rectangle)) return;

    const Colour native_colour = text_component_colour(captain_text);
    const Colour shared_colour = g_ui_configuration.ship_name_colour_found
        ? g_ui_configuration.ship_name_colour : native_colour;
    const Colour captain_colour = g_ui_configuration.captain_colour_found
        ? g_ui_configuration.captain_colour : shared_colour;
    const Colour registry_colour = g_ui_configuration.registry_colour_found
        ? g_ui_configuration.registry_colour : shared_colour;

    NativeRectangle registry_native_rectangle = captain_native_rectangle;
    if (g_ui_configuration.captain_rectangle_found &&
        g_ui_configuration.registry_rectangle_found) {
        const RawRectangle& captain =
            g_ui_configuration.captain_rectangle;
        const RawRectangle& registry =
            g_ui_configuration.registry_rectangle;
        registry_native_rectangle.left += registry.x - captain.x;
        registry_native_rectangle.top += registry.y - captain.y;
        registry_native_rectangle.right +=
            registry.x - captain.x + registry.width - captain.width;
        registry_native_rectangle.bottom +=
            registry.y - captain.y + registry.height - captain.height;
    }
    bool captain_drawn = false;
    bool registry_drawn = false;

    if (g_ui_configuration.captain_rectangle_found) {
        captain_drawn = draw_identity_text(
            identity->captain_name,
            captain_native_rectangle, captain_colour, captain_text);
    }
    if (g_ui_configuration.captain_rectangle_found &&
        g_ui_configuration.registry_rectangle_found) {
        registry_drawn = draw_identity_text(
            identity->craft_registry,
            registry_native_rectangle, registry_colour, captain_text);
    }
    if ((captain_drawn || registry_drawn) &&
        InterlockedCompareExchange(&g_draw_report_count, 1, 0) == 0) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "First selected identity draw submitted for native name row %ld: "
            "captain=%s at %ld,%ld; registry=%s at %ld,%ld",
            static_cast<long>(identity->craft_name_index),
            captain_drawn ? "yes" : "no",
            static_cast<long>(captain_native_rectangle.left),
            static_cast<long>(captain_native_rectangle.top),
            registry_drawn ? "yes" : "no",
            static_cast<long>(registry_native_rectangle.left),
            static_cast<long>(registry_native_rectangle.top));
        log_line(message);
    }
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    const void* address = at(module, rva);
    return readable_range(address, Size) &&
        std::memcmp(address, expected, Size) == 0;
}

template <std::size_t Size>
bool checked_armada_signature(
    const char* name, std::uintptr_t rva,
    const std::uint8_t (&expected)[Size]) noexcept {
    if (signature_matches(g_armada, rva, expected)) return true;
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "%s signature mismatch at Armada RVA 0x%08lX",
                  name, static_cast<unsigned long>(rva));
    log_line(message);
    return false;
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
    if (readable_range(site, 6) && bytes[0] == 0x68 && bytes[5] == 0xc3) {
        std::uint32_t destination = 0;
        std::memcpy(&destination, bytes + 1, sizeof(destination));
        if (patch_length) *patch_length = 6;
        return reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(destination));
    }
    return nullptr;
}

bool supported_fleet_ops_craft_class_detour(
    void** destination, std::size_t* patch_length) noexcept {
    void* resolved = existing_detour_destination(
        at(g_armada, kCraftClassConstructorRva), patch_length);
    void* expected = at(g_fleet_ops,
                        kFoCraftClassConstructorHandlerRva);
    const bool supported = resolved == expected &&
        signature_matches(
            g_fleet_ops, kFoCraftClassConstructorHandlerRva,
            kExpectedFoCraftClassConstructorHandler);
    if (destination) *destination = supported ? resolved : nullptr;
    return supported;
}

bool craft_class_constructor_supported() noexcept {
    if (signature_matches(
            g_armada, kCraftClassConstructorRva,
            kExpectedCraftClassConstructor)) {
        return true;
    }
    void* destination = nullptr;
    std::size_t patch_length = 0;
    if (supported_fleet_ops_craft_class_detour(
            &destination, &patch_length)) {
        return true;
    }
    char message[320]{};
    std::snprintf(
        message, sizeof(message),
        "CraftClass constructor has neither the stock prologue nor "
        "Fleet Ops' supported detour (destination=%p, expected=%p)",
        existing_detour_destination(
            at(g_armada, kCraftClassConstructorRva), nullptr),
        at(g_fleet_ops, kFoCraftClassConstructorHandlerRva));
    log_line(message);
    return false;
}

bool preflight_signatures() noexcept {
    bool supported = craft_class_constructor_supported();
    supported = checked_armada_signature(
        "InfoDisplay::RenderSelected", kSelectedInfoRenderRva,
        kExpectedSelectedInfoRender) && supported;
    supported = checked_armada_signature(
        "ParameterDB::Get(DBRectangle)", kParameterDbGetRectangleRva,
        kExpectedParameterDbGetRectangle) && supported;
    supported = checked_armada_signature(
        "ParameterDB::Get(DBColor)", kParameterDbGetColorRva,
        kExpectedParameterDbGetColor) && supported;
    supported = checked_armada_signature(
        "DisplayInterface::DrawText(rectangle)",
        kDisplayInterfaceDrawTextInRectangleRva,
        kExpectedDisplayInterfaceDrawTextInRectangle) && supported;
    supported = checked_armada_signature(
        "GameObjectClass::GetOdfName", kGameObjectClassGetOdfNameRva,
        kExpectedGameObjectClassGetOdfName) && supported;
    supported = readable_range(
        at(g_armada, kParameterDbGetStringVectorRva), 5) && supported;
    supported = readable_range(
        at(g_armada, kEngineOperatorDeleteRva), 5) && supported;
    return supported;
}

bool install_craft_class_constructor_hook(
    const A2FO_ModuleApi* api) noexcept {
    void* site = at(g_armada, kCraftClassConstructorRva);
    if (signature_matches(
            g_armada, kCraftClassConstructorRva,
            kExpectedCraftClassConstructor)) {
        if (!api->install_inline_hook(
                site,
                reinterpret_cast<void*>(&craft_class_constructor_hook),
                sizeof(kExpectedCraftClassConstructor),
                kExpectedCraftClassConstructor,
                &g_craft_class_constructor_hook)) {
            return false;
        }
        g_craft_class_constructor_original =
            g_craft_class_constructor_hook.gateway;
        g_chained_fo_craft_class_constructor = false;
        return g_craft_class_constructor_original != nullptr;
    }

    void* destination = nullptr;
    std::size_t patch_length = 0;
    if (!supported_fleet_ops_craft_class_detour(
            &destination, &patch_length) || patch_length > 6) {
        return false;
    }
    std::uint8_t expected_detour[6]{};
    std::memcpy(expected_detour, site, patch_length);
    if (!api->patch_jump(
            site, reinterpret_cast<void*>(&craft_class_constructor_hook),
            expected_detour, patch_length)) {
        return false;
    }
    g_craft_class_constructor_original = destination;
    g_chained_fo_craft_class_constructor = true;
    return true;
}

bool install_runtime_hooks(const A2FO_ModuleApi* api) noexcept {
    if (!api || !api->install_inline_hook || !api->patch_jump ||
        !g_armada || !g_fleet_ops) {
        return false;
    }
    if (!preflight_signatures()) {
        log_line("Supported ArmadaL signatures were not found; runtime disabled");
        return false;
    }
    bool installed = install_craft_class_constructor_hook(api);
    installed = api->install_inline_hook(
        at(g_armada, kSelectedInfoRenderRva),
        reinterpret_cast<void*>(&selected_info_render_hook),
        sizeof(kExpectedSelectedInfoRender), kExpectedSelectedInfoRender,
        &g_selected_info_render_hook) && installed;
    if (!installed) {
        log_line("A craft identity hook could not be installed; hooks fail closed");
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

    resolve_shield_class_observer();
    g_runtime_ready = install_runtime_hooks(api);
    if (g_runtime_ready) {
        log_line(g_chained_fo_craft_class_constructor
            ? "Selected-panel craft identity runtime initialized and chained "
              "through Fleet Operations CraftClass construction"
            : "Selected-panel craft identity runtime initialized");
    } else {
        log_line("Craft identity module loaded with runtime disabled");
    }
    // Inline hooks are process-lifetime patches. Keep a partially installed
    // module resident and make each hook a pass-through when setup failed.
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Process-lifetime hooks are owned by the core and intentionally remain.
}
