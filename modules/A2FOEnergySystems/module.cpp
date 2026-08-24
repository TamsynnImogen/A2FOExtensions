/*
 * ODF-driven Photon and Quantum Torpedo ammunition stores.
 *
 * Weapon ODF costs are enforced once for each successfully launched projectile.
 * Craft stores recharge either continuously (mode 1) or only near an allied
 * shipyard/RepairShip/explicit provider (mode 2).
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "energy_systems.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>

extern "C" {
std::uintptr_t __cdecl a2fo_energy_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_energy_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
std::uintptr_t __cdecl a2fo_energy_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
std::uintptr_t __cdecl a2fo_energy_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
std::uintptr_t __cdecl a2fo_energy_call_thiscall_7(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4, std::uintptr_t argument5,
    std::uintptr_t argument6, std::uintptr_t argument7);
}

namespace {

using a2fo::energy_systems::RechargeMode;
using a2fo::energy_systems::StorePolicy;
using a2fo::energy_systems::Stores;

constexpr char kModuleName[] = "A2FOEnergySystems";
constexpr float kDefaultResupplyRange = 200.0f;
constexpr std::uint32_t kSaveMagic = 0x45463241u;  // "A2FE"
constexpr std::uint32_t kSaveVersion = 1;
constexpr char kSaveLabel[] = "a2fo_torpedoStores";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs and stable object fields.
constexpr std::uintptr_t kWeaponSelectTargetOrdnanceRva = 0x0026fd40;
constexpr std::uintptr_t kWeaponSelectPositionOrdnanceRva = 0x0026fde0;
constexpr std::uintptr_t kWeaponCommitShotRva = 0x00270dd0;
constexpr std::uintptr_t kWeaponLaunchPositionOrdnanceRva = 0x002679f0;
constexpr std::uintptr_t kWeaponGetOwnerRva = 0x00271050;
// FleetOpsHook.dll 5.0.0.Bob CannonImp overrides both the pooled-ordnance
// selector and the guided launch method used by its repeat-fire loop.
constexpr std::uintptr_t kCannonImpLaunchTargetOrdnanceRva = 0x001392cc;
constexpr std::uintptr_t kCannonImpSelectTargetOrdnanceRva = 0x0013a550;
constexpr std::uintptr_t kCraftLoadRva = 0x000c2340;
constexpr std::uintptr_t kCraftSaveRva = 0x000c2980;
constexpr std::uintptr_t kFileOutBytesRva = 0x0012c680;
constexpr std::uintptr_t kFileInBytesRva = 0x0012d7a0;
constexpr std::uintptr_t kParameterDbGetRectangleRva = 0x001358f0;
constexpr std::uintptr_t kParameterDbGetColorRva = 0x00135ba0;
constexpr std::uintptr_t kDisplayInterfaceDrawTextInRectangleRva = 0x0011b160;
constexpr std::uintptr_t kGuiParameterDbPointerRva = 0x0036502c;
constexpr std::size_t kWeaponClassOffset = 0x04;
constexpr std::size_t kObjectExpiredOffset = 0x27;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kObjectPositionOffset = 0xac;
constexpr std::size_t kObjectTeamOffset = 0xec;
constexpr std::size_t kInfoDisplayCaptainTextOffset = 0xbc;
constexpr std::size_t kTextComponentLiveRectangleOffset = 0x58;

// The selected-panel callback is deliberately fail-closed.  The first live
// Roots test reached the render pass but faulted before the callback completed
// its initial ParameterDB lookup.  Keep the ammunition simulation available
// while the native GUI boundary is investigated separately.
constexpr bool kSelectedInfoUiEnabled = false;

int whole_ammunition_amount(float value) noexcept {
    if (!std::isfinite(value) || value <= 0.0f) return 0;
    const float maximum = static_cast<float>(
        std::numeric_limits<int>::max());
    return static_cast<int>(std::floor(std::min(value, maximum) + 0.0001f));
}

constexpr std::uint8_t kExpectedWeaponGetOwner[] = {
    0x8b, 0x49, 0x18, 0x51, 0xe8};
constexpr std::uint8_t kExpectedWeaponSelectTargetOrdnance[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x8b, 0x45, 0x08, 0x57};
constexpr std::uint8_t kExpectedWeaponSelectPositionOrdnance[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x14, 0x53, 0x56, 0x8b, 0xf1, 0x57};
constexpr std::uint8_t kExpectedWeaponCommitShot[] = {
    0x55, 0x8b, 0xec, 0x51, 0x56, 0x8b, 0xf1};
constexpr std::uint8_t kExpectedWeaponLaunchPositionOrdnance[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x48};
constexpr std::uint8_t kExpectedCannonImpLaunchTargetOrdnance[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xa0};
constexpr std::uint8_t kExpectedCannonImpSelectTargetOrdnance[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xec};
constexpr std::uint8_t kExpectedCraftLoad[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0xbc, 0x00, 0x00, 0x00};
constexpr std::uint8_t kExpectedCraftSave[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x84, 0x00, 0x00, 0x00};
constexpr std::uint8_t kExpectedParameterDbGetRectangle[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x04, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedParameterDbGetColor[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};
constexpr std::uint8_t kExpectedDisplayInterfaceDrawTextInRectangle[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};

constexpr std::array<const char*, 9> kCraftFields{{
    "maxPhotonTorpedoes",
    "photonTorpedoRate",
    "photonTorpedoRechargeMode",
    "maxQuantumTorpedoes",
    "quantumTorpedoRate",
    "quantumTorpedoRechargeMode",
    "torpedoResupply",
    "torpedoResupplyRange",
    "classLabel",
}};
constexpr std::array<const char*, 2> kWeaponFields{{
    "photonTorpedoCost",
    "quantumTorpedoCost",
}};

enum class Ammunition : std::uint8_t {
    photon,
    quantum,
};

struct CraftPolicy {
    StorePolicy photon{};
    StorePolicy quantum{};
    bool provider = false;
    float provider_range = kDefaultResupplyRange;
};

struct WeaponPolicy {
    Ammunition ammunition = Ammunition::photon;
    float cost = 0.0f;
};

struct SavedStores {
    std::uint32_t magic = kSaveMagic;
    std::uint32_t version = kSaveVersion;
    float photon = 0.0f;
    float quantum = 0.0f;
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

struct UiConfiguration {
    void* parameter_db = nullptr;
    bool loaded = false;
    bool anchor_found = false;
    bool photon_rectangle_found = false;
    bool quantum_rectangle_found = false;
    RawRectangle anchor{};
    RawRectangle photon_rectangle{};
    RawRectangle quantum_rectangle{};
    bool shared_colour_found = false;
    bool photon_colour_found = false;
    bool quantum_colour_found = false;
    Colour shared_colour{};
    Colour photon_colour{};
    Colour quantum_colour{};
};

using FileOutBytes = bool (__cdecl*)(
    void* writer, void* data, std::uint32_t size, const char* label);
using FileInBytes = bool (__cdecl*)(
    void* reader, void* data, std::uint32_t size);
using SelectedInfoObserver = void (A2FO_CALL*)(
    void* info_display, void* selected_craft, void* user_data);
using RegisterSelectedInfoObserver = bool (A2FO_CALL*)(
    const char* owner, SelectedInfoObserver handler, void* user_data);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
bool g_runtime_ready = false;
bool g_persistence_ready = false;
bool g_ui_ready = false;
A2FO_InlineHook g_craft_load_hook{};
A2FO_InlineHook g_craft_save_hook{};
A2FO_InlineHook g_weapon_select_target_ordnance_hook{};
A2FO_InlineHook g_weapon_select_position_ordnance_hook{};
A2FO_InlineHook g_weapon_commit_shot_hook{};
A2FO_InlineHook g_weapon_launch_position_ordnance_hook{};
A2FO_InlineHook g_cannon_imp_launch_target_ordnance_hook{};
A2FO_InlineHook g_cannon_imp_select_target_ordnance_hook{};
std::unordered_map<void*, CraftPolicy> g_craft_policies;
std::unordered_map<void*, WeaponPolicy> g_weapon_policies;
std::unordered_map<void*, Stores> g_craft_stores;
std::unordered_set<void*> g_resupply_providers;
bool g_store_policies_present = false;
bool g_resupply_consumers_present = false;
UiConfiguration g_ui_configuration{};
void* g_pending_ammunition_weapon = nullptr;
void* g_pending_cannon_imp_weapon = nullptr;

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

void* at(std::uintptr_t rva) noexcept { return at(g_armada, rva); }

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

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    const void* address = at(module, rva);
    return readable_range(address, Size) &&
        std::memcmp(address, expected, Size) == 0;
}

template <std::size_t Size>
bool signature_matches(std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    return signature_matches(g_armada, rva, expected);
}

template <typename T>
T read_live(const void* object, std::size_t offset,
            T fallback = T{}) noexcept {
    if (!object) return fallback;
    T value{};
    std::memcpy(&value,
                static_cast<const std::uint8_t*>(object) + offset,
                sizeof(value));
    return value;
}

template <typename T>
T read_ui_live(const void* object, std::size_t offset,
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

void* gui_parameter_db() noexcept {
    return read_ui_live<void*>(at(kGuiParameterDbPointerRva), 0, nullptr);
}

bool read_ui_rectangle(void* parameter_db, const char* key,
                       RawRectangle* output) noexcept {
    if (!parameter_db || !key || !output) return false;
    const RawRectangle fallback{};
    const std::uintptr_t found = a2fo_energy_call_thiscall_3(
        at(kParameterDbGetRectangleRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key),
        reinterpret_cast<std::uintptr_t>(output),
        reinterpret_cast<std::uintptr_t>(&fallback));
    return (found & 0xffu) != 0;
}

bool read_ui_colour(void* parameter_db, const char* key,
                    Colour* output) noexcept {
    if (!parameter_db || !key || !output) return false;
    const Colour fallback{};
    const std::uintptr_t found = a2fo_energy_call_thiscall_3(
        at(kParameterDbGetColorRva), parameter_db,
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
        loaded.anchor_found = read_ui_rectangle(
            parameter_db, "infoSingleCaptainTextArea", &loaded.anchor);
        if (!loaded.anchor_found) {
            loaded.anchor_found = read_ui_rectangle(
                parameter_db, "captainName", &loaded.anchor);
        }
        loaded.photon_rectangle_found = read_ui_rectangle(
            parameter_db, "infoSinglePhotonTorpedoesTextArea",
            &loaded.photon_rectangle);
        loaded.quantum_rectangle_found = read_ui_rectangle(
            parameter_db, "infoSingleQuantumTorpedoesTextArea",
            &loaded.quantum_rectangle);
        loaded.shared_colour_found = read_ui_colour(
            parameter_db, "infoTextColor", &loaded.shared_colour);
        loaded.photon_colour_found = read_ui_colour(
            parameter_db, "photonTorpedoColor", &loaded.photon_colour);
        loaded.quantum_colour_found = read_ui_colour(
            parameter_db, "quantumTorpedoColor", &loaded.quantum_colour);
    }
    g_ui_configuration = loaded;

    char message[256]{};
    std::snprintf(
        message, sizeof(message),
        "Torpedo UI: Photon rectangle=%s, Quantum rectangle=%s",
        loaded.photon_rectangle_found ? "configured" : "automatic",
        loaded.quantum_rectangle_found ? "configured" : "automatic");
    log_line(message);
}

bool usable_native_rectangle(const NativeRectangle& rectangle) noexcept {
    return rectangle.right > rectangle.left &&
        rectangle.bottom > rectangle.top;
}

Colour text_component_colour(const void* text_component) noexcept {
    Colour colour = read_ui_live<Colour>(text_component, 0x70, Colour{});
    if (!std::isfinite(colour.red) || !std::isfinite(colour.green) ||
        !std::isfinite(colour.blue)) {
        return Colour{};
    }
    return colour;
}

NativeRectangle translated_rectangle(
    const NativeRectangle& live_anchor, const RawRectangle& configured_anchor,
    const RawRectangle& configured_target) noexcept {
    NativeRectangle result = live_anchor;
    result.left += configured_target.x - configured_anchor.x;
    result.top += configured_target.y - configured_anchor.y;
    result.right += configured_target.x - configured_anchor.x +
        configured_target.width - configured_anchor.width;
    result.bottom += configured_target.y - configured_anchor.y +
        configured_target.height - configured_anchor.height;
    return result;
}

bool draw_ammunition_text(const char* value,
                          const NativeRectangle& render_rectangle,
                          const Colour& colour,
                          void* text_component) noexcept {
    if (!value || !*value || !usable_native_rectangle(render_rectangle) ||
        !text_component) {
        return false;
    }
    void* display_interface = read_ui_live<void*>(
        text_component, 0x04, nullptr);
    if (!display_interface) return false;
    void* display_override = nullptr;
    void* display_slot = read_ui_live<void*>(
        text_component, 0x28, nullptr);
    if (display_slot) {
        display_override = read_ui_live<void*>(display_slot, 0, nullptr);
    }
    const std::int32_t text_flags = read_ui_live<std::int32_t>(
        text_component, 0x68, 9);
    const std::uint8_t constrain_to_rectangle = read_ui_live<std::uint8_t>(
        text_component, 0x6c, 0);
    void* font_state = static_cast<std::uint8_t*>(text_component) + 0x7c;
    if (!readable_range(font_state, 12)) return false;

    a2fo_energy_call_thiscall_7(
        at(kDisplayInterfaceDrawTextInRectangleRva), display_interface,
        reinterpret_cast<std::uintptr_t>(value),
        reinterpret_cast<std::uintptr_t>(&render_rectangle),
        static_cast<std::uintptr_t>(text_flags),
        reinterpret_cast<std::uintptr_t>(&colour),
        reinterpret_cast<std::uintptr_t>(display_override),
        static_cast<std::uintptr_t>(constrain_to_rectangle),
        reinterpret_cast<std::uintptr_t>(font_state));
    return true;
}

bool field_value(const A2FO_OdfFieldView* fields, std::uint32_t count,
                 const char* name, std::string* value) {
    if (!fields || !name || !value) return false;
    const std::size_t name_size = std::strlen(name);
    for (std::uint32_t index = 0; index < count; ++index) {
        const A2FO_OdfFieldView& field = fields[index];
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

bool parse_float(const A2FO_OdfFieldView* fields, std::uint32_t count,
                 const char* name, float* value) noexcept {
    if (!value) return false;
    try {
        std::string text;
        if (!field_value(fields, count, name, &text)) return false;
        char* end = nullptr;
        const float parsed = std::strtof(text.c_str(), &end);
        if (end == text.c_str() || !std::isfinite(parsed)) return false;
        while (*end == ' ' || *end == '\t' || *end == '\r' ||
               *end == '\n') ++end;
        if (*end == 'f' || *end == 'F') ++end;
        while (*end == ' ' || *end == '\t' || *end == '\r' ||
               *end == '\n') ++end;
        if (*end != '\0' || parsed < 0.0f) return false;
        *value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_mode(const A2FO_OdfFieldView* fields, std::uint32_t count,
                const char* name, RechargeMode* mode) noexcept {
    float value = 0.0f;
    if (!mode || !parse_float(fields, count, name, &value)) return false;
    const int parsed = static_cast<int>(value);
    if (value != static_cast<float>(parsed) || parsed < 0 || parsed > 2) {
        return false;
    }
    *mode = static_cast<RechargeMode>(parsed);
    return true;
}

bool parse_bool(const A2FO_OdfFieldView* fields, std::uint32_t count,
                const char* name, bool* value) noexcept {
    float numeric = 0.0f;
    if (parse_float(fields, count, name, &numeric)) {
        *value = numeric != 0.0f;
        return true;
    }
    try {
        std::string text;
        if (!field_value(fields, count, name, &text)) return false;
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char character) {
                           return static_cast<char>(std::tolower(character));
                       });
        if (text == "true" || text == "yes" || text == "on") {
            *value = true;
            return true;
        }
        if (text == "false" || text == "no" || text == "off") {
            *value = false;
            return true;
        }
    } catch (...) {
    }
    return false;
}

bool automatic_provider(const A2FO_OdfFieldView* fields,
                        std::uint32_t count) noexcept {
    try {
        std::string class_label;
        if (!field_value(fields, count, "classLabel", &class_label)) {
            return false;
        }
        class_label.erase(std::remove(class_label.begin(), class_label.end(),
                                      '"'), class_label.end());
        std::transform(class_label.begin(), class_label.end(),
                       class_label.begin(), [](unsigned char character) {
                           return static_cast<char>(std::tolower(character));
                       });
        return class_label == "shipyard" || class_label == "repairship";
    } catch (...) {
        return false;
    }
}

void A2FO_CALL craft_class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    if (!g_runtime_ready || !event ||
        event->struct_size < sizeof(*event) || !event->object_class) {
        return;
    }
    CraftPolicy policy{};
    parse_float(event->odf_fields, event->odf_field_count,
                "maxPhotonTorpedoes", &policy.photon.maximum);
    parse_float(event->odf_fields, event->odf_field_count,
                "photonTorpedoRate", &policy.photon.recharge_per_second);
    parse_mode(event->odf_fields, event->odf_field_count,
               "photonTorpedoRechargeMode", &policy.photon.mode);
    parse_float(event->odf_fields, event->odf_field_count,
                "maxQuantumTorpedoes", &policy.quantum.maximum);
    parse_float(event->odf_fields, event->odf_field_count,
                "quantumTorpedoRate", &policy.quantum.recharge_per_second);
    parse_mode(event->odf_fields, event->odf_field_count,
               "quantumTorpedoRechargeMode", &policy.quantum.mode);
    policy.photon = a2fo::energy_systems::normalize_policy(policy.photon);
    policy.quantum = a2fo::energy_systems::normalize_policy(policy.quantum);
    policy.provider = automatic_provider(
        event->odf_fields, event->odf_field_count);
    parse_bool(event->odf_fields, event->odf_field_count,
               "torpedoResupply", &policy.provider);
    parse_float(event->odf_fields, event->odf_field_count,
                "torpedoResupplyRange", &policy.provider_range);
    if (policy.photon.maximum <= 0.0f && policy.quantum.maximum <= 0.0f &&
        !policy.provider) {
        return;
    }
    const bool has_store = a2fo::energy_systems::store_enabled(policy.photon) ||
        a2fo::energy_systems::store_enabled(policy.quantum);
    if (has_store) {
        g_store_policies_present = true;
        if (a2fo::energy_systems::requires_resupply(policy.photon) ||
            a2fo::energy_systems::requires_resupply(policy.quantum)) {
            g_resupply_consumers_present = true;
        }
    }
    try {
        g_craft_policies[event->object_class] = policy;
    } catch (...) {
        log_line("Could not retain a Craft torpedo-store policy");
    }
}

void A2FO_CALL weapon_class_loaded_handler(
    const A2FO_WeaponClassLoadedEvent* event, void*) {
    if (!g_runtime_ready || !event ||
        event->struct_size < sizeof(*event) || !event->weapon_class) {
        return;
    }
    float photon = 0.0f;
    float quantum = 0.0f;
    parse_float(event->odf_fields, event->odf_field_count,
                "photonTorpedoCost", &photon);
    parse_float(event->odf_fields, event->odf_field_count,
                "quantumTorpedoCost", &quantum);
    if (photon <= 0.0f && quantum <= 0.0f) return;
    if (photon > 0.0f && quantum > 0.0f) {
        log_line("Ignored weapon with both Photon and Quantum torpedo costs");
        return;
    }
    try {
        g_weapon_policies[event->weapon_class] = WeaponPolicy{
            photon > 0.0f ? Ammunition::photon : Ammunition::quantum,
            photon > 0.0f ? photon : quantum};
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "Registered %s torpedo cost %.3f on WeaponClass %p",
            photon > 0.0f ? "Photon" : "Quantum",
            photon > 0.0f ? photon : quantum,
            event->weapon_class);
        log_line(message);
    } catch (...) {
        log_line("Could not retain a weapon torpedo-cost policy");
    }
}

const CraftPolicy* policy_for_craft(const void* craft) noexcept {
    const void* object_class = read_live<const void*>(
        craft, kObjectClassOffset, nullptr);
    const auto found = g_craft_policies.find(
        const_cast<void*>(object_class));
    return found == g_craft_policies.end() ? nullptr : &found->second;
}

Stores* stores_for_craft(void* craft, bool create) noexcept {
    if (!craft) return nullptr;
    const auto present = g_craft_stores.find(craft);
    if (present != g_craft_stores.end()) return &present->second;
    if (!create) return nullptr;
    const CraftPolicy* policy = policy_for_craft(craft);
    if (!policy || (policy->photon.maximum <= 0.0f &&
                    policy->quantum.maximum <= 0.0f)) {
        return nullptr;
    }
    try {
        const auto inserted = g_craft_stores.emplace(
            craft, Stores{policy->photon.maximum, policy->quantum.maximum});
        return inserted.second ? &inserted.first->second : nullptr;
    } catch (...) {
        return nullptr;
    }
}

bool in_resupply_range(void* craft) noexcept {
    if (!craft) return false;
    const std::int32_t team = read_live<std::int32_t>(
        craft, kObjectTeamOffset, -1);
    const std::array<float, 3> position = read_live<std::array<float, 3>>(
        craft, kObjectPositionOffset, {});
    for (void* provider : g_resupply_providers) {
        if (!provider ||
            read_live<std::uint8_t>(provider, kObjectExpiredOffset, 1) != 0 ||
            read_live<std::int32_t>(provider, kObjectTeamOffset, -2) != team) {
            continue;
        }
        const CraftPolicy* provider_policy = policy_for_craft(provider);
        if (!provider_policy || !provider_policy->provider) continue;
        const std::array<float, 3> provider_position =
            read_live<std::array<float, 3>>(
                provider, kObjectPositionOffset, {});
        const float x = position[0] - provider_position[0];
        const float y = position[1] - provider_position[1];
        const float z = position[2] - provider_position[2];
        const float range = provider_policy->provider_range;
        if (x * x + y * y + z * z <= range * range) return true;
    }
    return false;
}

void A2FO_CALL craft_event_handler(
    const A2FO_CraftEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) ||
        !event->craft || !g_runtime_ready) {
        return;
    }
    void* craft = event->craft;
    if (event->kind == A2FO_CRAFT_EVENT_CLEANUP) {
        g_resupply_providers.erase(craft);
        g_craft_stores.erase(craft);
        return;
    }
    // Automatic shipyard/repair providers are inert when the active mod has
    // no configured torpedo stores. This avoids a class read and hash lookup
    // for every craft on both sides of every simulation tick in stock mods.
    if ((event->kind == A2FO_CRAFT_EVENT_SIMULATE_PRE ||
         event->kind == A2FO_CRAFT_EVENT_SIMULATE_POST) &&
        !g_store_policies_present) {
        return;
    }
    const CraftPolicy* policy = policy_for_craft(craft);
    if (!policy) return;
    if (event->kind == A2FO_CRAFT_EVENT_SIMULATE_PRE) {
        if (policy->provider && g_resupply_consumers_present) {
            try {
                g_resupply_providers.insert(craft);
            } catch (...) {
            }
        }
        if (!a2fo::energy_systems::store_enabled(policy->photon) &&
            !a2fo::energy_systems::store_enabled(policy->quantum)) {
            return;
        }
        stores_for_craft(craft, true);
        return;
    }
    if (event->kind == A2FO_CRAFT_EVENT_POST_LOAD) {
        if (policy->provider && g_resupply_consumers_present) {
            try {
                g_resupply_providers.insert(craft);
            } catch (...) {
            }
        }
        if (!a2fo::energy_systems::store_enabled(policy->photon) &&
            !a2fo::energy_systems::store_enabled(policy->quantum)) {
            return;
        }
        stores_for_craft(craft, true);
        return;
    }
    if (event->kind != A2FO_CRAFT_EVENT_SIMULATE_POST) return;
    if (!a2fo::energy_systems::store_enabled(policy->photon) &&
        !a2fo::energy_systems::store_enabled(policy->quantum)) {
        return;
    }
    Stores* stores = stores_for_craft(craft, true);
    if (!stores) return;
    const bool requires_provider =
        policy->photon.mode == RechargeMode::resupply_only ||
        policy->quantum.mode == RechargeMode::resupply_only;
    const bool supplied = !requires_provider || in_resupply_range(craft);
    stores->photon = a2fo::energy_systems::recharge(
        stores->photon, policy->photon, event->elapsed_seconds, supplied);
    stores->quantum = a2fo::energy_systems::recharge(
        stores->quantum, policy->quantum, event->elapsed_seconds, supplied);
}

const WeaponPolicy* policy_for_weapon(const void* weapon) noexcept {
    void* weapon_class = read_live<void*>(
        weapon, kWeaponClassOffset, nullptr);
    const auto found = g_weapon_policies.find(weapon_class);
    return found == g_weapon_policies.end() ? nullptr : &found->second;
}

float* ammunition_amount_for_weapon(
    void* weapon, const WeaponPolicy& policy) noexcept {
    void* owner = reinterpret_cast<void*>(a2fo_energy_call_thiscall_0(
        at(kWeaponGetOwnerRva), weapon));
    Stores* stores = stores_for_craft(owner, true);
    if (!stores) return nullptr;
    return policy.ammunition == Ammunition::photon
        ? &stores->photon : &stores->quantum;
}

bool ammunition_allows_selection(void* weapon) noexcept {
    const WeaponPolicy* policy = policy_for_weapon(weapon);
    if (!policy) return true;
    float* amount = ammunition_amount_for_weapon(weapon, *policy);
    return amount && a2fo::energy_systems::can_consume(
        *amount, policy->cost);
}

void commit_ammunition_shot(void* weapon) noexcept {
    const WeaponPolicy* policy = policy_for_weapon(weapon);
    if (!policy) return;
    float* amount = ammunition_amount_for_weapon(weapon, *policy);
    if (!amount) return;
    *amount = a2fo::energy_systems::consume(*amount, policy->cost);
}

std::uintptr_t __attribute__((fastcall))
cannon_imp_select_target_ordnance_hook(
    void* weapon, void*, std::uintptr_t target) noexcept {
    // CannonImp performs exploratory selections while assembling a volley, so
    // selection only gates availability and records which Weapon may use the
    // generic position-launch fallback. The debit belongs to the launch hook.
    g_pending_cannon_imp_weapon = nullptr;
    if (g_runtime_ready && weapon && !ammunition_allows_selection(weapon)) {
        return 0u;
    }
    const std::uintptr_t ordnance = a2fo_energy_call_thiscall_1(
        g_cannon_imp_select_target_ordnance_hook.gateway, weapon, target);
    if (ordnance != 0u && policy_for_weapon(weapon)) {
        g_pending_cannon_imp_weapon = weapon;
    }
    return ordnance;
}

void __attribute__((fastcall)) cannon_imp_launch_target_ordnance_hook(
    void* weapon, void*, std::uintptr_t ordnance) noexcept {
    const bool configured = g_runtime_ready && weapon &&
        policy_for_weapon(weapon);
    if (configured && !ammunition_allows_selection(weapon)) {
        if (g_pending_cannon_imp_weapon == weapon) {
            g_pending_cannon_imp_weapon = nullptr;
        }
        return;
    }
    a2fo_energy_call_thiscall_1(
        g_cannon_imp_launch_target_ordnance_hook.gateway, weapon, ordnance);
    if (!configured) return;
    if (g_pending_cannon_imp_weapon == weapon) {
        g_pending_cannon_imp_weapon = nullptr;
    }
    commit_ammunition_shot(weapon);
}

void __attribute__((fastcall)) weapon_launch_position_ordnance_hook(
    void* weapon, void*, std::uintptr_t ordnance) noexcept {
    // Armada's position launch is shared with ordinary weapons. Debit here
    // only when CannonImp's own selector immediately identified this Weapon;
    // ordinary weapons remain owned by the selector/common-commit hooks.
    const bool cannon_imp_shot = g_runtime_ready && weapon &&
        g_pending_cannon_imp_weapon == weapon && policy_for_weapon(weapon);
    if (cannon_imp_shot && !ammunition_allows_selection(weapon)) {
        g_pending_cannon_imp_weapon = nullptr;
        return;
    }
    a2fo_energy_call_thiscall_1(
        g_weapon_launch_position_ordnance_hook.gateway, weapon, ordnance);
    if (!cannon_imp_shot) return;
    g_pending_cannon_imp_weapon = nullptr;
    commit_ammunition_shot(weapon);
}

std::uintptr_t __attribute__((fastcall)) weapon_select_target_ordnance_hook(
    void* weapon, void*, std::uintptr_t target) noexcept {
    // A successful selector return is consumed by the firing path immediately
    // before Weapon::CommitShot. Clearing here also prevents an abandoned
    // selection from leaking into a later shot.
    g_pending_ammunition_weapon = nullptr;
    if (!g_runtime_ready || !weapon || ammunition_allows_selection(weapon)) {
        const std::uintptr_t ordnance = a2fo_energy_call_thiscall_1(
            g_weapon_select_target_ordnance_hook.gateway, weapon, target);
        if (ordnance != 0u && policy_for_weapon(weapon)) {
            g_pending_ammunition_weapon = weapon;
        }
        return ordnance;
    }
    return 0u;
}

std::uintptr_t __attribute__((fastcall)) weapon_select_position_ordnance_hook(
    void* weapon, void*, std::uintptr_t x, std::uintptr_t y,
    std::uintptr_t z) noexcept {
    g_pending_ammunition_weapon = nullptr;
    if (!g_runtime_ready || !weapon || ammunition_allows_selection(weapon)) {
        const std::uintptr_t ordnance = a2fo_energy_call_thiscall_3(
            g_weapon_select_position_ordnance_hook.gateway, weapon,
            x, y, z);
        if (ordnance != 0u && policy_for_weapon(weapon)) {
            g_pending_ammunition_weapon = weapon;
        }
        return ordnance;
    }
    return 0u;
}

void __attribute__((fastcall)) weapon_commit_shot_hook(
    void* weapon, void*) noexcept {
    if (g_runtime_ready && weapon && policy_for_weapon(weapon)) {
        if (g_pending_ammunition_weapon == weapon) {
            g_pending_ammunition_weapon = nullptr;
        }
        // Fleet Operations weapon classes such as CannonImp can select pooled
        // ordnance through an override rather than Armada's two base
        // selectors. This common post-fire method is still reached only after
        // the launch virtual succeeds, so it is the authoritative debit point.
        commit_ammunition_shot(weapon);
    }
    a2fo_energy_call_thiscall_0(
        g_weapon_commit_shot_hook.gateway, weapon);
}

std::uintptr_t __attribute__((fastcall)) craft_save_hook(
    void* craft, void*, void* writer) noexcept {
    const std::uintptr_t native_saved = a2fo_energy_call_thiscall_1(
        g_craft_save_hook.gateway, craft,
        reinterpret_cast<std::uintptr_t>(writer));
    if ((native_saved & 0xffu) == 0 || !g_persistence_ready) {
        return native_saved;
    }
    Stores* stores = stores_for_craft(craft, true);
    if (!stores) return native_saved;
    SavedStores saved{};
    saved.photon = stores->photon;
    saved.quantum = stores->quantum;
    const auto out = reinterpret_cast<FileOutBytes>(at(kFileOutBytesRva));
    return out && out(writer, &saved, sizeof(saved), kSaveLabel) ? 1u : 0u;
}

std::uintptr_t __attribute__((fastcall)) craft_load_hook(
    void* craft, void*, void* reader) noexcept {
    const std::uintptr_t native_loaded = a2fo_energy_call_thiscall_1(
        g_craft_load_hook.gateway, craft,
        reinterpret_cast<std::uintptr_t>(reader));
    if ((native_loaded & 0xffu) == 0 || !g_persistence_ready) {
        return native_loaded;
    }
    Stores* stores = stores_for_craft(craft, true);
    if (!stores) return native_loaded;
    SavedStores saved{};
    const auto in = reinterpret_cast<FileInBytes>(at(kFileInBytesRva));
    if (!in || !in(reader, &saved, sizeof(saved)) ||
        saved.magic != kSaveMagic || saved.version != kSaveVersion) {
        log_line("Torpedo-store save data is missing or invalid");
        return 0u;
    }
    const CraftPolicy* policy = policy_for_craft(craft);
    if (!policy) return native_loaded;
    stores->photon = a2fo::energy_systems::clamp_amount(
        saved.photon, policy->photon);
    stores->quantum = a2fo::energy_systems::clamp_amount(
        saved.quantum, policy->quantum);
    return native_loaded;
}

template <std::size_t Size>
bool install_hook(HMODULE module, std::uintptr_t rva, void* replacement,
                  const std::uint8_t (&expected)[Size],
                  A2FO_InlineHook* hook) noexcept {
    return g_api->install_inline_hook(
        at(module, rva), replacement, Size, expected, hook);
}

template <std::size_t Size>
bool install_hook(std::uintptr_t rva, void* replacement,
                  const std::uint8_t (&expected)[Size],
                  A2FO_InlineHook* hook) noexcept {
    return install_hook(g_armada, rva, replacement, expected, hook);
}

bool install_persistence_hooks() noexcept {
    if (!signature_matches(kCraftLoadRva, kExpectedCraftLoad) ||
        !signature_matches(kCraftSaveRva, kExpectedCraftSave) ||
        !readable_range(at(kFileOutBytesRva), 5) ||
        !readable_range(at(kFileInBytesRva), 5)) {
        return false;
    }
    return install_hook(kCraftLoadRva,
                        reinterpret_cast<void*>(&craft_load_hook),
                        kExpectedCraftLoad, &g_craft_load_hook) &&
        install_hook(kCraftSaveRva,
                     reinterpret_cast<void*>(&craft_save_hook),
                     kExpectedCraftSave, &g_craft_save_hook);
}

bool install_ammunition_shot_hooks() noexcept {
    if (!signature_matches(
            kWeaponSelectTargetOrdnanceRva,
            kExpectedWeaponSelectTargetOrdnance) ||
        !signature_matches(
            kWeaponSelectPositionOrdnanceRva,
            kExpectedWeaponSelectPositionOrdnance) ||
        !signature_matches(
            kWeaponCommitShotRva, kExpectedWeaponCommitShot) ||
        !signature_matches(
            kWeaponLaunchPositionOrdnanceRva,
            kExpectedWeaponLaunchPositionOrdnance) ||
        !signature_matches(
            g_fleet_ops, kCannonImpLaunchTargetOrdnanceRva,
            kExpectedCannonImpLaunchTargetOrdnance) ||
        !signature_matches(
            g_fleet_ops, kCannonImpSelectTargetOrdnanceRva,
            kExpectedCannonImpSelectTargetOrdnance)) {
        return false;
    }
    return install_hook(
               kWeaponSelectTargetOrdnanceRva,
               reinterpret_cast<void*>(&weapon_select_target_ordnance_hook),
               kExpectedWeaponSelectTargetOrdnance,
               &g_weapon_select_target_ordnance_hook) &&
        install_hook(
            kWeaponSelectPositionOrdnanceRva,
            reinterpret_cast<void*>(&weapon_select_position_ordnance_hook),
            kExpectedWeaponSelectPositionOrdnance,
            &g_weapon_select_position_ordnance_hook) &&
        install_hook(
            kWeaponCommitShotRva,
            reinterpret_cast<void*>(&weapon_commit_shot_hook),
            kExpectedWeaponCommitShot,
            &g_weapon_commit_shot_hook) &&
        install_hook(
            kWeaponLaunchPositionOrdnanceRva,
            reinterpret_cast<void*>(&weapon_launch_position_ordnance_hook),
            kExpectedWeaponLaunchPositionOrdnance,
            &g_weapon_launch_position_ordnance_hook) &&
        install_hook(
            g_fleet_ops, kCannonImpSelectTargetOrdnanceRva,
            reinterpret_cast<void*>(&cannon_imp_select_target_ordnance_hook),
            kExpectedCannonImpSelectTargetOrdnance,
            &g_cannon_imp_select_target_ordnance_hook) &&
        install_hook(
            g_fleet_ops, kCannonImpLaunchTargetOrdnanceRva,
            reinterpret_cast<void*>(&cannon_imp_launch_target_ordnance_hook),
            kExpectedCannonImpLaunchTargetOrdnance,
            &g_cannon_imp_launch_target_ordnance_hook);
}

float current_amount(void* craft, Ammunition ammunition) noexcept {
    Stores* stores = stores_for_craft(craft, true);
    if (!stores) return 0.0f;
    return ammunition == Ammunition::photon
        ? stores->photon : stores->quantum;
}

float maximum_amount(void* craft, Ammunition ammunition) noexcept {
    const CraftPolicy* policy = policy_for_craft(craft);
    if (!policy) return 0.0f;
    return ammunition == Ammunition::photon
        ? policy->photon.maximum : policy->quantum.maximum;
}

float remaining_reload_seconds(
    void* craft, Ammunition ammunition) noexcept {
    Stores* stores = stores_for_craft(craft, true);
    const CraftPolicy* policy = policy_for_craft(craft);
    if (!stores || !policy) return -1.0f;
    const StorePolicy& store_policy = ammunition == Ammunition::photon
        ? policy->photon : policy->quantum;
    const float amount = ammunition == Ammunition::photon
        ? stores->photon : stores->quantum;
    const bool supplied = store_policy.mode != RechargeMode::resupply_only ||
        in_resupply_range(craft);
    return a2fo::energy_systems::reload_seconds(
        amount, store_policy, supplied);
}

void set_amount(void* craft, Ammunition ammunition, float amount) noexcept {
    Stores* stores = stores_for_craft(craft, true);
    const CraftPolicy* policy = policy_for_craft(craft);
    if (!stores || !policy) return;
    if (ammunition == Ammunition::photon) {
        stores->photon = a2fo::energy_systems::clamp_amount(
            amount, policy->photon);
    } else {
        stores->quantum = a2fo::energy_systems::clamp_amount(
            amount, policy->quantum);
    }
}

const char* recharge_status(RechargeMode mode, bool supplied) noexcept {
    switch (mode) {
        case RechargeMode::automatic:
            return "AUTO";
        case RechargeMode::resupply_only:
            return supplied ? "SUPPLY" : "NO SUPPLY";
        case RechargeMode::none:
        default:
            return "NO RECHARGE";
    }
}

void A2FO_CALL selected_info_render_handler(
    void* info_display, void* selected_craft, void*) noexcept {
    if (!g_runtime_ready || !g_ui_ready || !info_display ||
        !selected_craft) {
        return;
    }
    void* craft = selected_craft;
    const CraftPolicy* policy = policy_for_craft(craft);
    if (!policy || (policy->photon.maximum <= 0.0f &&
                    policy->quantum.maximum <= 0.0f)) {
        return;
    }
    Stores* stores = stores_for_craft(craft, true);
    if (!stores) return;

    void* parameter_db = gui_parameter_db();
    if (!g_ui_configuration.loaded ||
        g_ui_configuration.parameter_db != parameter_db) {
        refresh_ui_configuration(parameter_db);
    }
    void* text_component = read_ui_live<void*>(
        info_display, kInfoDisplayCaptainTextOffset, nullptr);
    if (!text_component) return;
    const NativeRectangle anchor = read_ui_live<NativeRectangle>(
        text_component, kTextComponentLiveRectangleOffset,
        NativeRectangle{});
    if (!usable_native_rectangle(anchor)) return;

    NativeRectangle photon_rectangle = anchor;
    NativeRectangle quantum_rectangle = anchor;
    photon_rectangle.top += 56;
    photon_rectangle.bottom += 56;
    quantum_rectangle.top += 84;
    quantum_rectangle.bottom += 84;
    if (g_ui_configuration.anchor_found &&
        g_ui_configuration.photon_rectangle_found) {
        photon_rectangle = translated_rectangle(
            anchor, g_ui_configuration.anchor,
            g_ui_configuration.photon_rectangle);
    }
    if (g_ui_configuration.anchor_found &&
        g_ui_configuration.quantum_rectangle_found) {
        quantum_rectangle = translated_rectangle(
            anchor, g_ui_configuration.anchor,
            g_ui_configuration.quantum_rectangle);
    }

    const Colour native_colour = text_component_colour(text_component);
    const Colour shared_colour = g_ui_configuration.shared_colour_found
        ? g_ui_configuration.shared_colour : native_colour;
    const Colour photon_colour = g_ui_configuration.photon_colour_found
        ? g_ui_configuration.photon_colour : shared_colour;
    const Colour quantum_colour = g_ui_configuration.quantum_colour_found
        ? g_ui_configuration.quantum_colour : shared_colour;
    const bool supplied = in_resupply_range(craft);

    char photon_text[128]{};
    char quantum_text[128]{};
    if (policy->photon.maximum > 0.0f) {
        std::snprintf(
            photon_text, sizeof(photon_text),
            "Photon Torpedoes: %d/%d  [%s]",
            whole_ammunition_amount(stores->photon),
            whole_ammunition_amount(policy->photon.maximum),
            recharge_status(policy->photon.mode, supplied));
        draw_ammunition_text(
            photon_text, photon_rectangle, photon_colour, text_component);
    }
    if (policy->quantum.maximum > 0.0f) {
        std::snprintf(
            quantum_text, sizeof(quantum_text),
            "Quantum Torpedoes: %d/%d  [%s]",
            whole_ammunition_amount(stores->quantum),
            whole_ammunition_amount(policy->quantum.maximum),
            recharge_status(policy->quantum.mode, supplied));
        draw_ammunition_text(
            quantum_text, quantum_rectangle, quantum_colour, text_component);
    }
}

bool register_selected_info_observer() noexcept {
    HMODULE identity = GetModuleHandleA("A2FOCraftIdentity.dll");
    FARPROC exported = identity
        ? GetProcAddress(
              identity,
              "A2FOCraftIdentity_RegisterSelectedInfoObserver")
        : nullptr;
    RegisterSelectedInfoObserver registration = nullptr;
    static_assert(sizeof(registration) == sizeof(exported),
                  "unexpected function-pointer size");
    std::memcpy(&registration, &exported, sizeof(registration));
    return registration && registration(
        kModuleName, &selected_info_render_handler, nullptr);
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->install_inline_hook ||
        !A2FO_MODULE_API_HAS(api, register_game_object_class_loaded_handler) ||
        !api->register_game_object_class_loaded_handler ||
        !A2FO_MODULE_API_HAS(api, register_weapon_class_loaded_handler) ||
        !api->register_weapon_class_loaded_handler ||
        !A2FO_MODULE_API_HAS(api, register_craft_event_handler) ||
        !api->register_craft_event_handler ||
        (api->capabilities & A2FO_CAP_GAME_OBJECT_CLASS_LOADED) == 0 ||
        (api->capabilities & A2FO_CAP_WEAPON_CLASS_LOADED) == 0 ||
        (api->capabilities & A2FO_CAP_CRAFT_EVENTS) == 0) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !g_fleet_ops ||
        !signature_matches(kWeaponGetOwnerRva, kExpectedWeaponGetOwner) ||
        !signature_matches(kWeaponSelectTargetOrdnanceRva,
                           kExpectedWeaponSelectTargetOrdnance) ||
        !signature_matches(kWeaponSelectPositionOrdnanceRva,
                           kExpectedWeaponSelectPositionOrdnance) ||
        !signature_matches(kWeaponCommitShotRva,
                           kExpectedWeaponCommitShot) ||
        !signature_matches(kWeaponLaunchPositionOrdnanceRva,
                           kExpectedWeaponLaunchPositionOrdnance) ||
        !signature_matches(g_fleet_ops,
                           kCannonImpLaunchTargetOrdnanceRva,
                           kExpectedCannonImpLaunchTargetOrdnance) ||
        !signature_matches(g_fleet_ops,
                           kCannonImpSelectTargetOrdnanceRva,
                           kExpectedCannonImpSelectTargetOrdnance)) {
        return false;
    }

    // Set readiness before class callbacks begin. The core installs and starts
    // the shared dispatchers only after every module has initialized.
    g_runtime_ready = true;
    const bool registered =
        api->register_game_object_class_loaded_handler(
            kModuleName, kCraftFields.data(),
            static_cast<std::uint32_t>(kCraftFields.size()),
            &craft_class_loaded_handler, nullptr) &&
        api->register_weapon_class_loaded_handler(
            kModuleName, kWeaponFields.data(),
            static_cast<std::uint32_t>(kWeaponFields.size()),
            &weapon_class_loaded_handler, nullptr) &&
        api->register_craft_event_handler(
            kModuleName, &craft_event_handler, nullptr);
    if (!registered) {
        g_runtime_ready = false;
        return false;
    }

    // Install this mandatory hook only after every fallible core registration.
    // No later failure may unload the DLL while this detour remains active.
    if (!install_ammunition_shot_hooks()) {
        g_runtime_ready = false;
        return false;
    }

    g_persistence_ready = install_persistence_hooks();
    const bool native_ui_ready =
        signature_matches(kParameterDbGetRectangleRva,
                          kExpectedParameterDbGetRectangle) &&
        signature_matches(kParameterDbGetColorRva,
                          kExpectedParameterDbGetColor) &&
        signature_matches(kDisplayInterfaceDrawTextInRectangleRva,
                          kExpectedDisplayInterfaceDrawTextInRectangle);
    g_ui_ready = kSelectedInfoUiEnabled && native_ui_ready &&
        register_selected_info_observer();
    log_line(g_persistence_ready
        ? "Per-shot Photon and Quantum torpedo stores initialized with save/load support"
        : "Per-shot Photon and Quantum torpedo stores initialized; save/load hooks unavailable");
    log_line(g_ui_ready
        ? "Selected-craft Photon and Quantum torpedo UI initialized"
        : kSelectedInfoUiEnabled
            ? "Selected-craft torpedo UI unavailable; CraftIdentity observer or native draw signatures did not match"
            : "Direct torpedo UI renderer disabled; CraftIdentity may display exported ammunition values");
    return true;
}

extern "C" __declspec(dllexport)
float A2FO_CALL A2FOEnergySystems_GetPhotonTorpedoes(void* craft) {
    return current_amount(craft, Ammunition::photon);
}

extern "C" __declspec(dllexport)
float A2FO_CALL A2FOEnergySystems_GetQuantumTorpedoes(void* craft) {
    return current_amount(craft, Ammunition::quantum);
}

extern "C" __declspec(dllexport)
float A2FO_CALL A2FOEnergySystems_GetMaximumPhotonTorpedoes(void* craft) {
    return maximum_amount(craft, Ammunition::photon);
}

extern "C" __declspec(dllexport)
float A2FO_CALL A2FOEnergySystems_GetMaximumQuantumTorpedoes(void* craft) {
    return maximum_amount(craft, Ammunition::quantum);
}

extern "C" __declspec(dllexport)
float A2FO_CALL A2FOEnergySystems_GetPhotonTorpedoReloadSeconds(void* craft) {
    return remaining_reload_seconds(craft, Ammunition::photon);
}

extern "C" __declspec(dllexport)
float A2FO_CALL A2FOEnergySystems_GetQuantumTorpedoReloadSeconds(void* craft) {
    return remaining_reload_seconds(craft, Ammunition::quantum);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOEnergySystems_SetPhotonTorpedoes(
    void* craft, float amount) {
    set_amount(craft, Ammunition::photon, amount);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOEnergySystems_SetQuantumTorpedoes(
    void* craft, float amount) {
    set_amount(craft, Ammunition::quantum, amount);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOEnergySystems_AddPhotonTorpedoes(
    void* craft, float amount) {
    set_amount(craft, Ammunition::photon,
               current_amount(craft, Ammunition::photon) + amount);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOEnergySystems_AddQuantumTorpedoes(
    void* craft, float amount) {
    set_amount(craft, Ammunition::quantum,
               current_amount(craft, Ammunition::quantum) + amount);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    g_runtime_ready = false;
}
