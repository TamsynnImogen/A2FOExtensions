/*
 * ODF-driven Photon Torpedo, Quantum Torpedo, and Shuttle Craft stores.
 *
 * Weapon ODF costs are enforced once for each successfully launched projectile.
 * Craft stores recharge either continuously (mode 1) or only near a same-team
 * shipyard/RepairShip/explicit provider (mode 2).
 *
 * classLabel = "resupplyweapon" hosts a native cannon which fires only at
 * Craft whose mode-2 stores its owner is currently replenishing.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_module_provider.hpp"
#include "energy_systems.hpp"
#include "resupply_policy.hpp"
#include "../../rust/stage8_a_energy_ffi.h"
#include "../../rust/stage9_energy_numeric_ffi.h"
#include "../../rust/stage10_energy_runtime_ffi.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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
constexpr std::uint32_t kLegacySaveMagic = 0x45463241u;  // "A2FE"
constexpr std::uint32_t kLegacySaveVersion = 1;
constexpr std::uint32_t kSaveMagic = 0x33463241u;  // "A2F3"
constexpr char kSaveLabel[] = "a2fo_torpedoStores";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs and stable object fields.
constexpr std::uintptr_t kWeaponSelectTargetOrdnanceRva = 0x0026fd40;
constexpr std::uintptr_t kWeaponSelectPositionOrdnanceRva = 0x0026fde0;
constexpr std::uintptr_t kWeaponCommitShotRva = 0x00270dd0;
constexpr std::uintptr_t kWeaponLaunchPositionOrdnanceRva = 0x002679f0;
constexpr std::uintptr_t kWeaponGetOwnerRva = 0x00271050;
// Public Weapon::Trigger(GameObject const*) entry. The core detours it after
// module initialization, so calls retain every shared trigger precheck.
constexpr std::uintptr_t kWeaponTriggerRva = 0x00271290;
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
constexpr std::size_t kWeaponOwnerHandleOffset = 0x18;
constexpr std::size_t kWeaponActiveOffset = 0x2c;
constexpr std::size_t kObjectHandleOffset = 0x28;
constexpr std::size_t kCraftCarrierOffset = 0x128;
constexpr std::size_t kCarrierWeaponsBeginOffset = 0x0c;
// Adjacent native vector pointers (0x10 on i686); keep synthetic host
// fixtures from overlapping these fields when pointers are eight bytes.
constexpr std::size_t kCarrierWeaponsEndOffset =
    kCarrierWeaponsBeginOffset + sizeof(void*);
constexpr std::size_t kMaximumCarrierWeapons = 256;
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

// resupplyweapon is a semantic label on Armada's native CannonClass, which
// fires at the Weapon's own triggered target. Fleet Ops' CannonImp is not
// used because it aims at the owner's primary target instead.
constexpr char kResupplyWeaponClasslabel[] = "resupplyweapon";
constexpr char kResupplyWeaponNativeClasslabel[] = "cannon";
// A receiver remains a valid resupply-weapon target for this long after its
// latest replenished tick, bridging Craft and Weapon simulation order.
constexpr float kResupplyActivitySeconds = 0.5f;

int whole_ammunition_amount(float value) noexcept {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return a2fo_rs_stage10_energy_whole(bits);
}

constexpr std::uint8_t kExpectedWeaponGetOwner[] = {
    0x8b, 0x49, 0x18, 0x51, 0xe8};
constexpr std::uint8_t kExpectedWeaponTrigger[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
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

constexpr std::array<const char*, 15> kCraftFields{{
    "maxPhotonTorpedoes",
    "photonTorpedoRate",
    "photonTorpedoRechargeMode",
    "maxQuantumTorpedoes",
    "quantumTorpedoRate",
    "quantumTorpedoRechargeMode",
    "maxShuttleCraft",
    "shuttleCraftRate",
    "shuttleCraftRechargeMode",
    "torpedoResupply",
    "torpedoResupplyRange",
    "canResupplySelf",
    "resupplyTargetType",
    "resupplyValidTargets",
    "classLabel",
}};
constexpr std::array<const char*, 5> kWeaponFields{{
    "photonTorpedoCost",
    "quantumTorpedoCost",
    "shuttleCraftCost",
    "classLabel",
    "resupplyValidTargets",
}};

enum class Ammunition : std::uint8_t {
    photon,
    quantum,
    shuttle_craft,
};

struct CraftPolicy {
    StorePolicy photon{};
    StorePolicy quantum{};
    StorePolicy shuttle_craft{};
    bool provider = false;
    float provider_range = kDefaultResupplyRange;
    // Preserve existing providers' self-resupply unless explicitly disabled.
    bool can_resupply_self = true;
    std::string resupply_target_type;
    std::string source_odf;
    a2fo::energy_systems::ResupplyTargetFilter resupply_targets;
};

struct WeaponPolicy {
    Ammunition ammunition = Ammunition::photon;
    float cost = 0.0f;
};

struct ResupplyWeaponPolicy {
    // Receivers this weapon reacts to; omitted means every receiver its
    // owner is resupplying.
    a2fo::energy_systems::ResupplyTargetFilter targets;
};

struct ResupplyReceiver {
    void* craft = nullptr;
    float remaining_seconds = 0.0f;
};

struct ResupplyWeaponTarget {
    void* provider = nullptr;
    void* receiver = nullptr;
};

struct LegacySavedStores {
    std::uint32_t magic;
    std::uint32_t version;
    float photon;
    float quantum;
};

struct SavedStores {
    std::uint32_t magic = kSaveMagic;
    float photon = 0.0f;
    float quantum = 0.0f;
    float shuttle_craft = 0.0f;
};

static_assert(sizeof(LegacySavedStores) == sizeof(SavedStores),
              "ammunition save block must remain backward-compatible");

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

static_assert(sizeof(RawRectangle) == 4 * sizeof(std::int32_t) &&
              offsetof(RawRectangle, y) == 4 &&
              offsetof(RawRectangle, width) == 8 &&
              offsetof(RawRectangle, height) == 12,
              "Rust rectangle adapter requires contiguous native fields");
static_assert(sizeof(NativeRectangle) == 4 * sizeof(std::int32_t) &&
              offsetof(NativeRectangle, top) == 4 &&
              offsetof(NativeRectangle, right) == 8 &&
              offsetof(NativeRectangle, bottom) == 12,
              "Rust native rectangle adapter requires contiguous fields");
static_assert(sizeof(Colour) == 3 * sizeof(float) &&
              offsetof(Colour, green) == 4 && offsetof(Colour, blue) == 8,
              "Rust colour adapter requires contiguous native fields");

struct UiConfiguration {
    void* parameter_db = nullptr;
    bool loaded = false;
    bool anchor_found = false;
    bool photon_rectangle_found = false;
    bool quantum_rectangle_found = false;
    bool shuttle_craft_rectangle_found = false;
    RawRectangle anchor{};
    RawRectangle photon_rectangle{};
    RawRectangle quantum_rectangle{};
    RawRectangle shuttle_craft_rectangle{};
    bool shared_colour_found = false;
    bool photon_colour_found = false;
    bool quantum_colour_found = false;
    bool shuttle_craft_colour_found = false;
    Colour shared_colour{};
    Colour photon_colour{};
    Colour quantum_colour{};
    Colour shuttle_craft_colour{};
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
// Keyed by WeaponClass / provider Craft / live Weapon respectively.
std::unordered_map<void*, ResupplyWeaponPolicy> g_resupply_weapon_policies;
std::unordered_map<void*, std::vector<ResupplyReceiver>> g_resupply_activity;
std::unordered_map<void*, ResupplyWeaponTarget> g_resupply_weapon_targets;
bool g_issuing_resupply_trigger = false;
// Installed by A2FO_ModuleInit only after the native signature matched; the
// host regression substitutes a recorder.
using TriggerWeapon = void (*)(void* weapon, void* target);
TriggerWeapon g_trigger_weapon = nullptr;

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

struct EnergyUiRefreshContext {
    void* parameter_db;
    UiConfiguration loaded{};
};

std::uintptr_t energy_ui_refresh_step(
    void* opaque, std::uint32_t op, std::uintptr_t) noexcept {
    auto& context = *static_cast<EnergyUiRefreshContext*>(opaque);
    auto& loaded = context.loaded;
    switch (op) {
    case 0:
        loaded.parameter_db = context.parameter_db;
        loaded.loaded = true;
        return context.parameter_db != nullptr;
    case 1: return loaded.anchor_found = read_ui_rectangle(
        context.parameter_db, "infoSingleCaptainTextArea", &loaded.anchor);
    case 2: return loaded.anchor_found = read_ui_rectangle(
        context.parameter_db, "captainName", &loaded.anchor);
    case 3: return loaded.photon_rectangle_found = read_ui_rectangle(
        context.parameter_db, "infoSinglePhotonTorpedoesTextArea",
        &loaded.photon_rectangle);
    case 4: return loaded.quantum_rectangle_found = read_ui_rectangle(
        context.parameter_db, "infoSingleQuantumTorpedoesTextArea",
        &loaded.quantum_rectangle);
    case 5: return loaded.shuttle_craft_rectangle_found = read_ui_rectangle(
        context.parameter_db, "infoSingleShuttleCraftTextArea",
        &loaded.shuttle_craft_rectangle);
    case 6: return loaded.shared_colour_found = read_ui_colour(
        context.parameter_db, "infoTextColor", &loaded.shared_colour);
    case 7: return loaded.photon_colour_found = read_ui_colour(
        context.parameter_db, "photonTorpedoColor", &loaded.photon_colour);
    case 8: return loaded.quantum_colour_found = read_ui_colour(
        context.parameter_db, "quantumTorpedoColor", &loaded.quantum_colour);
    case 9: return loaded.shuttle_craft_colour_found = read_ui_colour(
        context.parameter_db, "shuttleCraftColor",
        &loaded.shuttle_craft_colour);
    case 10: {
        g_ui_configuration = loaded;
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "Ammunition UI: Photon rectangle=%s, Quantum rectangle=%s, Shuttle Craft rectangle=%s",
            loaded.photon_rectangle_found ? "configured" : "automatic",
            loaded.quantum_rectangle_found ? "configured" : "automatic",
            loaded.shuttle_craft_rectangle_found
                ? "configured" : "automatic");
        log_line(message);
        return 1;
    }
    default: return 0;
    }
}

void refresh_ui_configuration(void* parameter_db) noexcept {
    EnergyUiRefreshContext context{parameter_db, {}};
    a2fo_rs_stage8_energy_ui_refresh(&energy_ui_refresh_step, &context);
}

bool usable_native_rectangle(const NativeRectangle& rectangle) noexcept {
    return a2fo_rs_stage10_energy_rectangle_usable(&rectangle.left) != 0;
}

Colour text_component_colour(const void* text_component) noexcept {
    Colour colour = read_ui_live<Colour>(text_component, 0x70, Colour{});
    if (!a2fo_rs_stage10_energy_colour_valid(&colour.red)) {
        return Colour{};
    }
    return colour;
}

NativeRectangle translated_rectangle(
    const NativeRectangle& live_anchor, const RawRectangle& configured_anchor,
    const RawRectangle& configured_target) noexcept {
    NativeRectangle result = live_anchor;
    a2fo_rs_stage10_energy_translate(
        &result.left, &configured_anchor.x, &configured_target.x);
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

struct EnergyFieldContext {
    const A2FO_OdfFieldView* fields;
    std::uint32_t count;
    const char* name;
    std::string* value;
    std::size_t name_size = 0;
    std::exception_ptr exception{};
};

std::uintptr_t energy_field_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t index) noexcept {
    auto& context = *static_cast<EnergyFieldContext*>(opaque);
    try {
        switch (op) {
        case 0:
            if (!context.fields || !context.name || !context.value) return 0;
            context.name_size = std::strlen(context.name);
            return 1;
        case 1: return context.count;
        case 2: {
            const A2FO_OdfFieldView& field = context.fields[index];
            return field.name.data &&
                field.name.size == context.name_size &&
                _strnicmp(field.name.data, context.name,
                           context.name_size) == 0 &&
                (field.value.data || field.value.size == 0);
        }
        case 3: {
            const A2FO_OdfFieldView& field = context.fields[index];
            context.value->assign(
                field.value.data ? field.value.data : "",
                field.value.size);
            return 1;
        }
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

bool field_value(const A2FO_OdfFieldView* fields, std::uint32_t count,
                 const char* name, std::string* value) {
    EnergyFieldContext context{fields, count, name, value};
    const bool found = a2fo_rs_stage8_energy_field(
        &energy_field_step, &context) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return found;
}

struct EnergyFloatContext {
    const A2FO_OdfFieldView* fields;
    std::uint32_t count;
    const char* name;
    float* value;
    std::string text;
    float parsed = 0.0f;
};

std::uintptr_t energy_float_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t index) noexcept {
    auto& context = *static_cast<EnergyFloatContext*>(opaque);
    try {
        switch (op) {
        case 0:
            return field_value(context.fields, context.count,
                               context.name, &context.text);
        case 1: {
            char* end = nullptr;
            context.parsed = std::strtof(context.text.c_str(), &end);
            return static_cast<std::uintptr_t>(
                end - context.text.c_str());
        }
        case 2: {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &context.parsed, sizeof(bits));
            return bits;
        }
        case 3:
            return static_cast<unsigned char>(
                context.text.c_str()[index]);
        case 4: *context.value = context.parsed; return 1;
        default: return 0;
        }
    } catch (...) {
        return UINTPTR_MAX;
    }
}

bool parse_float(const A2FO_OdfFieldView* fields, std::uint32_t count,
                 const char* name, float* value) noexcept {
    if (!value) return false;
    EnergyFloatContext context{fields, count, name, value, {}, 0.0f};
    return a2fo_rs_stage8_energy_parse_float(
        &energy_float_step, &context) != 0;
}

struct EnergyParseContext {
    const A2FO_OdfFieldView* fields;
    std::uint32_t count;
    const char* name;
    float numeric = 0.0f;
    std::string text;
};

std::uintptr_t energy_parse_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t index) noexcept {
    auto& context = *static_cast<EnergyParseContext*>(opaque);
    try {
        switch (op) {
        case 0:
            return parse_float(context.fields, context.count,
                               context.name, &context.numeric);
        case 1: {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &context.numeric, sizeof(bits));
            return bits;
        }
        case 2:
            return field_value(context.fields, context.count,
                               context.name, &context.text);
        case 3: return context.text.size();
        case 4:
            return static_cast<unsigned char>(std::tolower(
                static_cast<unsigned char>(context.text[index])));
        default: return 0;
        }
    } catch (...) {
        return UINTPTR_MAX;
    }
}

bool parse_mode(const A2FO_OdfFieldView* fields, std::uint32_t count,
                const char* name, RechargeMode* mode) noexcept {
    if (!mode) return false;
    EnergyParseContext context{fields, count, name, 0.0f, {}};
    const std::int32_t selected = a2fo_rs_stage8_energy_parse(
        &energy_parse_step, &context, 0);
    if (selected < 0) return false;
    *mode = static_cast<RechargeMode>(selected);
    return true;
}

bool parse_bool(const A2FO_OdfFieldView* fields, std::uint32_t count,
                const char* name, bool* value) noexcept {
    EnergyParseContext context{fields, count, name, 0.0f, {}};
    const std::int32_t selected = a2fo_rs_stage8_energy_parse(
        &energy_parse_step, &context, 1);
    if (selected < 0) return false;
    *value = selected != 0;
    return true;
}

bool automatic_provider(const A2FO_OdfFieldView* fields,
                        std::uint32_t count) noexcept {
    EnergyParseContext context{fields, count, "classLabel", 0.0f, {}};
    return a2fo_rs_stage8_energy_parse(
        &energy_parse_step, &context, 2) != 0;
}

struct EnergyCraftClassContext {
    const A2FO_GameObjectClassLoadedEvent* event;
    CraftPolicy policy{};
    std::string text{};
    std::exception_ptr exception{};
};

std::uintptr_t energy_craft_class_step(void* opaque, std::uint32_t op,
                                       std::uintptr_t index) noexcept {
    auto& context = *static_cast<EnergyCraftClassContext*>(opaque);
    const auto* event = context.event;
    static constexpr const char* kNumericFields[9] = {
        "maxPhotonTorpedoes", "photonTorpedoRate", "photonTorpedoRechargeMode",
        "maxQuantumTorpedoes", "quantumTorpedoRate", "quantumTorpedoRechargeMode",
        "maxShuttleCraft", "shuttleCraftRate", "shuttleCraftRechargeMode"};
    try {
        switch (op) {
        case 0:
            return g_runtime_ready && event &&
                event->struct_size >= sizeof(*event) && event->object_class;
        case 1: {
            StorePolicy* store = index < 3 ? &context.policy.photon
                : index < 6 ? &context.policy.quantum
                : &context.policy.shuttle_craft;
            const std::uint32_t field = index % 3;
            if (field == 0) {
                parse_float(event->odf_fields, event->odf_field_count,
                            kNumericFields[index], &store->maximum);
            } else if (field == 1) {
                parse_float(event->odf_fields, event->odf_field_count,
                            kNumericFields[index], &store->recharge_per_second);
            } else {
                parse_mode(event->odf_fields, event->odf_field_count,
                           kNumericFields[index], &store->mode);
            }
            return 1;
        }
        case 2: {
            StorePolicy* store = index == 0 ? &context.policy.photon
                : index == 1 ? &context.policy.quantum
                : &context.policy.shuttle_craft;
            *store = a2fo::energy_systems::normalize_policy(*store);
            return 1;
        }
        case 3:
            context.policy.provider = automatic_provider(
                event->odf_fields, event->odf_field_count);
            return 1;
        case 4:
            parse_bool(event->odf_fields, event->odf_field_count,
                       "torpedoResupply", &context.policy.provider);
            return 1;
        case 5:
            parse_float(event->odf_fields, event->odf_field_count,
                        "torpedoResupplyRange", &context.policy.provider_range);
            return 1;
        case 6:
            return field_value(event->odf_fields, event->odf_field_count,
                               "canResupplySelf", &context.text);
        case 7:
            return parse_bool(event->odf_fields, event->odf_field_count,
                              "canResupplySelf",
                              &context.policy.can_resupply_self);
        case 8:
            context.policy.can_resupply_self = false;
            log_line("Invalid canResupplySelf; self-resupply disabled");
            return 1;
        case 9:
            if (event->source_odf.data && event->source_odf.size) {
                context.policy.source_odf =
                    a2fo::energy_systems::normalize_resupply_odf(
                        std::string_view(event->source_odf.data,
                                         event->source_odf.size));
            }
            return 1;
        case 10:
            if (field_value(event->odf_fields, event->odf_field_count,
                            "resupplyTargetType", &context.text)) {
                context.policy.resupply_target_type =
                    a2fo::energy_systems::normalize_resupply_type(context.text);
            }
            return 1;
        case 11:
            if (field_value(event->odf_fields, event->odf_field_count,
                            "resupplyValidTargets", &context.text)) {
                context.policy.resupply_targets =
                    a2fo::energy_systems::parse_resupply_targets(context.text);
                if (!context.policy.resupply_targets.valid) {
                    log_line("Invalid resupplyValidTargets; provider will not resupply other Craft");
                }
            }
            return 1;
        case 12:
            context.policy.can_resupply_self = false;
            context.policy.resupply_targets = {};
            context.policy.resupply_targets.valid = false;
            context.policy.resupply_targets.default_allow = false;
            log_line("Could not retain resupply target policy; provider will not resupply Craft");
            return 1;
        case 13:
            switch (index) {
            case 0: return context.policy.photon.maximum > 0.0f;
            case 1: return context.policy.quantum.maximum > 0.0f;
            case 2: return context.policy.shuttle_craft.maximum > 0.0f;
            case 3: return context.policy.provider;
            default: return 0;
            }
        case 14: {
            const StorePolicy& store = index == 0 ? context.policy.photon
                : index == 1 ? context.policy.quantum
                : context.policy.shuttle_craft;
            return a2fo::energy_systems::store_enabled(store);
        }
        case 15: g_store_policies_present = true; return 1;
        case 16: {
            const StorePolicy& store = index == 0 ? context.policy.photon
                : index == 1 ? context.policy.quantum
                : context.policy.shuttle_craft;
            return a2fo::energy_systems::requires_resupply(store);
        }
        case 17: g_resupply_consumers_present = true; return 1;
        case 18:
            try {
                g_craft_policies[event->object_class] =
                    std::move(context.policy);
            } catch (...) {
                log_line("Could not retain a Craft ammunition-store policy");
            }
            return 1;
        default: return 0;
        }
    } catch (...) {
        if (op < 6 || op > 11) context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

void A2FO_CALL craft_class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event, void*) {
    EnergyCraftClassContext context{event, {}, {}, {}};
    a2fo_rs_stage8_energy_craft_class(
        &energy_craft_class_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
}

struct EnergyWeaponClassContext {
    const A2FO_WeaponClassLoadedEvent* event;
    std::string label{};
    std::string text{};
    ResupplyWeaponPolicy resupply{};
    float costs[3]{};
};

std::uintptr_t energy_weapon_class_step(void* opaque, std::uint32_t op,
                                        std::uintptr_t index) noexcept {
    auto& context = *static_cast<EnergyWeaponClassContext*>(opaque);
    const auto* event = context.event;
    static constexpr const char* kCostFields[3] = {
        "photonTorpedoCost", "quantumTorpedoCost", "shuttleCraftCost"};
    static constexpr const char* kNames[3] = {
        "Photon Torpedo", "Quantum Torpedo", "Shuttle Craft"};
    try {
        switch (op) {
        case 0:
            return g_runtime_ready && event &&
                event->struct_size >= sizeof(*event) && event->weapon_class;
        case 1:
            g_resupply_weapon_policies.erase(event->weapon_class);
            return 1;
        case 2: {
            std::array<char, 64> original{};
            if (g_api &&
                A2FO_MODULE_API_HAS(g_api, get_original_classlabel) &&
                (g_api->capabilities & A2FO_CAP_ORIGINAL_CLASSLABEL) != 0 &&
                g_api->get_original_classlabel && event->parameter_db &&
                g_api->get_original_classlabel(
                    event->parameter_db, original.data(),
                    static_cast<std::uint32_t>(original.size()))) {
                context.label = original.data();
                return 1;
            }
            return 0;
        }
        case 3:
            return field_value(event->odf_fields, event->odf_field_count,
                               "classLabel", &context.label);
        case 4:
            context.label.erase(a2fo_rs_stage10_energy_unquote(
                context.label.data(), context.label.size()));
            return _strnicmp(context.label.c_str(),
                             kResupplyWeaponClasslabel,
                             sizeof(kResupplyWeaponClasslabel)) == 0;
        case 5:
            if (field_value(event->odf_fields, event->odf_field_count,
                            "resupplyValidTargets", &context.text)) {
                context.resupply.targets =
                    a2fo::energy_systems::parse_resupply_targets(context.text);
                if (!context.resupply.targets.valid) {
                    log_line("Invalid resupplyweapon resupplyValidTargets; weapon will not fire");
                }
            }
            return 1;
        case 6: {
            g_resupply_weapon_policies[event->weapon_class] =
                std::move(context.resupply);
            char message[128]{};
            std::snprintf(message, sizeof(message),
                          "Registered resupplyweapon WeaponClass %p",
                          event->weapon_class);
            log_line(message);
            return 1;
        }
        case 7:
            try {
                ResupplyWeaponPolicy denied{};
                denied.targets.valid = false;
                denied.targets.default_allow = false;
                g_resupply_weapon_policies[event->weapon_class] = denied;
            } catch (...) {
            }
            log_line("Could not retain resupplyweapon target policy; weapon will not fire");
            return 1;
        case 8:
            parse_float(event->odf_fields, event->odf_field_count,
                        kCostFields[index], &context.costs[index]);
            return 1;
        case 9: return context.costs[index] > 0.0f;
        case 10:
            log_line("Ignored weapon with more than one ammunition cost");
            return 1;
        case 11:
            try {
                g_weapon_policies[event->weapon_class] = WeaponPolicy{
                    static_cast<Ammunition>(index), context.costs[index]};
                char message[192]{};
                std::snprintf(message, sizeof(message),
                    "Registered %s cost %.3f on WeaponClass %p",
                    kNames[index], context.costs[index], event->weapon_class);
                log_line(message);
            } catch (...) {
                log_line("Could not retain a weapon ammunition-cost policy");
            }
            return 1;
        default: return 0;
        }
    } catch (...) {
        // The original class-label lookup and resupply retention contain
        // these failures; cost parsing and map insertion do too.
        return UINTPTR_MAX;
    }
}

void A2FO_CALL weapon_class_loaded_handler(
    const A2FO_WeaponClassLoadedEvent* event, void*) {
    EnergyWeaponClassContext context{event, {}, {}, {}, {}};
    a2fo_rs_stage8_energy_weapon_class(
        &energy_weapon_class_step, &context);
}

const CraftPolicy* policy_for_craft(const void* craft) noexcept {
    const void* object_class = read_live<const void*>(
        craft, kObjectClassOffset, nullptr);
    const auto found = g_craft_policies.find(
        const_cast<void*>(object_class));
    return found == g_craft_policies.end() ? nullptr : &found->second;
}

struct EnergyStoresContext {
    void* craft;
    Stores* result = nullptr;
    const CraftPolicy* policy = nullptr;
};

std::uintptr_t energy_stores_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t index) noexcept {
    auto& context = *static_cast<EnergyStoresContext*>(opaque);
    switch (op) {
    case 0: return context.craft != nullptr;
    case 1: {
        const auto present = g_craft_stores.find(context.craft);
        if (present == g_craft_stores.end()) return 0;
        context.result = &present->second;
        return 1;
    }
    case 2:
        context.policy = policy_for_craft(context.craft);
        return context.policy != nullptr;
    case 3:
        return (index == 0 ? context.policy->photon.maximum
            : index == 1 ? context.policy->quantum.maximum
            : context.policy->shuttle_craft.maximum) > 0.0f;
    case 4:
        try {
            const auto inserted = g_craft_stores.emplace(
                context.craft,
                Stores{context.policy->photon.maximum,
                       context.policy->quantum.maximum,
                       context.policy->shuttle_craft.maximum});
            context.result = inserted.second
                ? &inserted.first->second : nullptr;
            return context.result != nullptr;
        } catch (...) {
            return 0;
        }
    default: return 0;
    }
}

Stores* stores_for_craft(void* craft, bool create) noexcept {
    EnergyStoresContext context{craft};
    a2fo_rs_stage8_energy_stores(
        &energy_stores_step, &context, static_cast<std::uint8_t>(create));
    return context.result;
}

struct EnergyReceiverActiveContext {
    void* provider;
    const void* receiver;
    const std::vector<ResupplyReceiver>* entries = nullptr;
};

std::uintptr_t energy_receiver_active_step(
    void* opaque, std::uint32_t op, std::uintptr_t index) noexcept {
    auto& context = *static_cast<EnergyReceiverActiveContext*>(opaque);
    switch (op) {
    case 0: {
        const auto found = g_resupply_activity.find(context.provider);
        if (found == g_resupply_activity.end()) return 0;
        context.entries = &found->second;
        return 1;
    }
    case 1: return context.entries->size();
    case 2: return (*context.entries)[index].craft == context.receiver;
    default: return 0;
    }
}

bool resupply_receiver_active(void* provider, const void* receiver) noexcept {
    EnergyReceiverActiveContext context{provider, receiver};
    return a2fo_rs_stage8_energy_receiver_active(
        &energy_receiver_active_step, &context) != 0;
}

struct EnergyResupplyTriggerContext {
    void* provider;
    void* receiver;
    const CraftPolicy& receiver_policy;
    std::uint32_t handle = 0;
    void** begin = nullptr;
    void** end = nullptr;
    void* weapon = nullptr;
    const ResupplyWeaponPolicy* weapon_policy = nullptr;
    std::exception_ptr exception{};
};

std::uintptr_t energy_trigger_resupply_step(
    void* opaque, std::uint32_t op, std::uintptr_t index) noexcept {
    auto& context = *static_cast<EnergyResupplyTriggerContext*>(opaque);
    try {
        switch (op) {
        case 0: return g_trigger_weapon &&
            context.provider != context.receiver;
        case 1: {
            context.handle = read_live<std::uint32_t>(
                context.provider, kObjectHandleOffset, 0);
            void* carrier = read_live<void*>(
                context.provider, kCraftCarrierOffset, nullptr);
            if (!carrier) return 0;
            context.begin = read_live<void**>(
                carrier, kCarrierWeaponsBeginOffset, nullptr);
            context.end = read_live<void**>(
                carrier, kCarrierWeaponsEndOffset, nullptr);
            return 1;
        }
        case 2:
            if (!context.begin || context.end < context.begin ||
                static_cast<std::size_t>(context.end - context.begin) >
                    kMaximumCarrierWeapons) return 0;
            return static_cast<std::size_t>(context.end - context.begin) + 1;
        case 3: {
            context.weapon = context.begin[index];
            if (!context.weapon ||
                read_live<std::uint32_t>(
                    context.weapon, kWeaponOwnerHandleOffset, 0) !=
                    context.handle) return 0;
            const auto policy = g_resupply_weapon_policies.find(
                read_live<void*>(context.weapon,
                                 kWeaponClassOffset, nullptr));
            if (policy == g_resupply_weapon_policies.end()) return 0;
            context.weapon_policy = &policy->second;
            return 1;
        }
        case 4:
            return a2fo::energy_systems::resupply_target_allowed(
                context.weapon_policy->targets,
                context.receiver_policy.resupply_target_type,
                context.receiver_policy.source_odf, false, false);
        case 5:
            return read_live<std::uint8_t>(
                context.weapon, kWeaponActiveOffset, 0) != 0;
        case 6: {
            const auto current = g_resupply_weapon_targets.find(
                context.weapon);
            return current != g_resupply_weapon_targets.end() &&
                current->second.provider == context.provider &&
                resupply_receiver_active(
                    context.provider, current->second.receiver);
        }
        case 7:
            g_resupply_weapon_targets[context.weapon] =
                ResupplyWeaponTarget{context.provider, context.receiver};
            return 1;
        case 8: g_issuing_resupply_trigger = true; return 1;
        case 9:
            g_trigger_weapon(context.weapon, context.receiver);
            return 1;
        case 10: g_issuing_resupply_trigger = false; return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

void trigger_resupply_weapons(void* provider, void* receiver,
                              const CraftPolicy& receiver_policy) {
    EnergyResupplyTriggerContext context{provider, receiver, receiver_policy};
    a2fo_rs_stage8_energy_trigger_resupply(
        &energy_trigger_resupply_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
}

struct EnergyCreditContext {
    void* provider;
    void* receiver;
    const CraftPolicy& receiver_policy;
    std::vector<ResupplyReceiver>* receivers = nullptr;
    std::exception_ptr exception{};
};

std::uintptr_t energy_credit_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t index) noexcept {
    auto& context = *static_cast<EnergyCreditContext*>(opaque);
    try {
        switch (op) {
        case 0: return !g_resupply_weapon_policies.empty();
        case 1:
            context.receivers = &g_resupply_activity[context.provider];
            return 1;
        case 2: return context.receivers->size();
        case 3: return (*context.receivers)[index].craft == context.receiver;
        case 4:
            (*context.receivers)[index].remaining_seconds =
                kResupplyActivitySeconds;
            return 1;
        case 5:
            context.receivers->push_back(
                {context.receiver, kResupplyActivitySeconds});
            return 1;
        case 6:
            trigger_resupply_weapons(
                context.provider, context.receiver,
                context.receiver_policy);
            return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

void credit_resupply_provider(void* provider, void* receiver,
                              const CraftPolicy& receiver_policy) {
    EnergyCreditContext context{provider, receiver, receiver_policy};
    a2fo_rs_stage8_energy_credit_provider(&energy_credit_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
}

struct EnergyAgeContext {
    void* provider;
    float elapsed;
    decltype(g_resupply_activity)::iterator activity{};
    std::vector<ResupplyReceiver>* receivers = nullptr;
    std::size_t source_index = 0;
    std::exception_ptr exception{};
};

std::uintptr_t energy_age_step(void* opaque, std::uint32_t op,
                               std::uintptr_t index) noexcept {
    auto& context = *static_cast<EnergyAgeContext*>(opaque);
    try {
        switch (op) {
        case 0:
            context.activity = g_resupply_activity.find(context.provider);
            if (context.activity == g_resupply_activity.end()) return 0;
            context.receivers = &context.activity->second;
            return 1;
        case 1: context.elapsed = 0.0f; return 1;
        case 2: return context.receivers->size();
        case 3:
            a2fo_rs_stage10_energy_age(
                &(*context.receivers)[index].remaining_seconds, &context.elapsed);
            return 1;
        case 4: {
            std::uint32_t bits;
            std::memcpy(&bits, &(*context.receivers)[index].remaining_seconds,
                        sizeof(bits));
            return a2fo_rs_stage10_energy_active(bits);
        }
        case 5: context.source_index = index; return 1;
        case 6:
            (*context.receivers)[index] =
                (*context.receivers)[context.source_index];
            return 1;
        case 7:
            context.receivers->erase(
                context.receivers->begin() + index,
                context.receivers->end());
            return 1;
        case 8:
            g_resupply_activity.erase(context.activity);
            return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

void age_resupply_activity(void* provider, float elapsed_seconds) noexcept {
    EnergyAgeContext context{provider, elapsed_seconds};
    a2fo_rs_stage8_energy_age_provider(
        &energy_age_step, &context, elapsed_seconds);
    if (context.exception) std::rethrow_exception(context.exception);
}

struct EnergyForgetContext {
    void* craft;
    decltype(g_resupply_activity)::iterator activity{};
    decltype(g_resupply_weapon_targets)::iterator target{};
    std::vector<ResupplyReceiver>* receivers = nullptr;
    std::size_t source_index = 0;
    std::exception_ptr exception{};
};

std::uintptr_t energy_forget_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t index) noexcept {
    auto& context = *static_cast<EnergyForgetContext*>(opaque);
    try {
        switch (op) {
        case 0: g_resupply_activity.erase(context.craft); return 1;
        case 1:
            context.activity = g_resupply_activity.begin(); return 1;
        case 2:
            if (context.activity == g_resupply_activity.end()) return 0;
            context.receivers = &context.activity->second;
            return 1;
        case 3: return context.receivers->size();
        case 4:
            return (*context.receivers)[index].craft != context.craft;
        case 5: context.source_index = index; return 1;
        case 6:
            (*context.receivers)[index] =
                (*context.receivers)[context.source_index];
            return 1;
        case 7:
            context.receivers->erase(
                context.receivers->begin() + index,
                context.receivers->end());
            return 1;
        case 8: ++context.activity; return 1;
        case 9:
            context.target = g_resupply_weapon_targets.begin();
            return 1;
        case 10: return context.target != g_resupply_weapon_targets.end();
        case 11:
            return context.target->second.provider == context.craft ||
                context.target->second.receiver == context.craft;
        case 12:
            context.target = g_resupply_weapon_targets.erase(context.target);
            return 1;
        case 13: ++context.target; return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

void forget_resupply_craft(void* craft) noexcept {
    EnergyForgetContext context{craft};
    a2fo_rs_stage8_energy_forget_craft(&energy_forget_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
}

// With `credit`, every eligible provider in range is credited with this
// receiver, so the result does not depend on provider-set iteration order.
struct EnergyRangeContext {
    void* craft;
    const CraftPolicy* receiver_policy = nullptr;
    const CraftPolicy* provider_policy = nullptr;
    std::int32_t team = -1;
    std::array<float, 3> position{};
    std::unordered_set<void*>::iterator current{};
    std::unordered_set<void*>::iterator end{};
    void* provider = nullptr;
    std::exception_ptr exception{};
};

std::uintptr_t energy_range_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t) noexcept {
    auto& context = *static_cast<EnergyRangeContext*>(opaque);
    try {
        switch (op) {
        case 0: return context.craft != nullptr;
        case 1:
            context.receiver_policy = policy_for_craft(context.craft);
            return context.receiver_policy != nullptr;
        case 2:
            context.team = read_live<std::int32_t>(
                context.craft, kObjectTeamOffset, -1);
            context.position = read_live<std::array<float, 3>>(
                context.craft, kObjectPositionOffset, {});
            context.current = g_resupply_providers.begin();
            context.end = g_resupply_providers.end();
            return 1;
        case 3: return context.current != context.end;
        case 4:
            context.provider = *context.current;
            return context.provider &&
                read_live<std::uint8_t>(
                    context.provider, kObjectExpiredOffset, 1) == 0 &&
                read_live<std::int32_t>(
                    context.provider, kObjectTeamOffset, -2) == context.team;
        case 5:
            context.provider_policy = policy_for_craft(context.provider);
            return context.provider_policy && context.provider_policy->provider;
        case 6:
            return a2fo::energy_systems::resupply_target_allowed(
                context.provider_policy->resupply_targets,
                context.receiver_policy->resupply_target_type,
                context.receiver_policy->source_odf,
                context.provider == context.craft,
                context.provider_policy->can_resupply_self);
        case 7: {
            const std::array<float, 3> provider_position =
                read_live<std::array<float, 3>>(
                    context.provider, kObjectPositionOffset, {});
            const float range = context.provider_policy->provider_range;
            return a2fo_rs_stage9_energy_range_admit(
                context.position[0], context.position[1], context.position[2],
                provider_position[0], provider_position[1], provider_position[2],
                range);
        }
        case 8:
            try {
                credit_resupply_provider(context.provider, context.craft,
                                         *context.receiver_policy);
            } catch (...) {
                g_issuing_resupply_trigger = false;
                log_line("Could not record resupplyweapon activity");
            }
            return 1;
        case 9: ++context.current; return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

bool in_resupply_range(void* craft, bool credit = false) noexcept {
    EnergyRangeContext context{craft};
    const bool result = a2fo_rs_stage8_energy_in_range(
        &energy_range_step, &context, static_cast<std::uint8_t>(credit)) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return result;
}

struct EnergyCraftEventContext {
    const A2FO_CraftEvent* event;
    void* craft = nullptr;
    const CraftPolicy* policy = nullptr;
    Stores* stores = nullptr;
    Stores before{};
    std::exception_ptr exception{};
};

std::uintptr_t energy_craft_event_step(void* opaque, std::uint32_t op,
                                       std::uintptr_t value) noexcept {
    auto& context = *static_cast<EnergyCraftEventContext*>(opaque);
    const auto* event = context.event;
    try {
        switch (op) {
        case 0:
            if (!event || event->struct_size < sizeof(*event) ||
                !event->craft || !g_runtime_ready) return 0;
            context.craft = event->craft;
            return 1;
        case 1: return event->kind;
        case 2: g_resupply_providers.erase(context.craft); return 1;
        case 3: g_craft_stores.erase(context.craft); return 1;
        case 4: forget_resupply_craft(context.craft); return 1;
        case 5: return g_store_policies_present;
        case 6:
            context.policy = policy_for_craft(context.craft);
            return context.policy != nullptr;
        case 7:
            return context.policy->provider && g_resupply_consumers_present;
        case 8:
            try { g_resupply_providers.insert(context.craft); }
            catch (...) {}
            return 1;
        case 9:
            age_resupply_activity(context.craft, event->elapsed_seconds);
            return 1;
        case 10: {
            const StorePolicy& store = value == 0 ? context.policy->photon
                : value == 1 ? context.policy->quantum
                : context.policy->shuttle_craft;
            return a2fo::energy_systems::store_enabled(store);
        }
        case 11:
            context.stores = stores_for_craft(context.craft, true);
            return context.stores != nullptr;
        case 12:
        case 17: {
            const StorePolicy& store = value == 0 ? context.policy->photon
                : value == 1 ? context.policy->quantum
                : context.policy->shuttle_craft;
            return static_cast<std::uintptr_t>(store.mode);
        }
        case 13: return in_resupply_range(context.craft);
        case 14: context.before = *context.stores; return 1;
        case 15: {
            const std::uintptr_t store_index = value & 3;
            const bool supplied = (value & 4) != 0;
            float* amount = store_index == 0 ? &context.stores->photon
                : store_index == 1 ? &context.stores->quantum
                : &context.stores->shuttle_craft;
            const StorePolicy& store = store_index == 0
                ? context.policy->photon : store_index == 1
                    ? context.policy->quantum : context.policy->shuttle_craft;
            *amount = a2fo::energy_systems::recharge(
                *amount, store, event->elapsed_seconds, supplied);
            return 1;
        }
        case 16: return !g_resupply_weapon_policies.empty();
        case 18: in_resupply_range(context.craft, true); return 1;
        case 19:
        case 20: {
            const Stores& stores = op == 19 ? context.before : *context.stores;
            const float amount = value == 0 ? stores.photon
                : value == 1 ? stores.quantum : stores.shuttle_craft;
            std::uint32_t bits = 0;
            std::memcpy(&bits, &amount, sizeof(bits));
            return bits;
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
    EnergyCraftEventContext context{event};
    a2fo_rs_stage8_energy_craft_event(
        &energy_craft_event_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
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
    switch (policy.ammunition) {
        case Ammunition::photon:
            return &stores->photon;
        case Ammunition::quantum:
            return &stores->quantum;
        case Ammunition::shuttle_craft:
            return &stores->shuttle_craft;
    }
    return nullptr;
}

bool is_resupply_weapon(const void* weapon) noexcept {
    return !g_resupply_weapon_policies.empty() &&
        g_resupply_weapon_policies.count(
            read_live<void*>(weapon, kWeaponClassOffset, nullptr)) != 0;
}

std::uintptr_t energy_trigger_precheck_step(
    void* opaque, std::uint32_t op, std::uintptr_t) noexcept {
    const auto* event = static_cast<const A2FO_WeaponTriggerEvent*>(opaque);
    switch (op) {
    case 0:
        return g_runtime_ready && event &&
            event->struct_size >= sizeof(*event) &&
            event->kind == A2FO_WEAPON_TRIGGER_PRECHECK;
    case 1: return g_issuing_resupply_trigger;
    case 2: return is_resupply_weapon(event->weapon);
    default: return 0;
    }
}

bool A2FO_CALL resupply_weapon_trigger_handler(
    const A2FO_WeaponTriggerEvent* event, void*) {
    return a2fo_rs_stage8_energy_trigger_precheck(
        &energy_trigger_precheck_step,
        const_cast<A2FO_WeaponTriggerEvent*>(event)) != 0;
}

void native_trigger_weapon(void* weapon, void* target) {
    a2fo_energy_call_thiscall_1(at(kWeaponTriggerRva), weapon,
                                reinterpret_cast<std::uintptr_t>(target));
}

struct EnergyShotContext {
    void* weapon;
    std::uintptr_t first;
    std::uintptr_t second;
    std::uintptr_t third;
    std::uint32_t kind;
    const WeaponPolicy* policy = nullptr;
    float* amount = nullptr;
    void* provider = nullptr;
};

std::uintptr_t energy_shot_step(void* opaque, std::uint32_t op,
                                std::uintptr_t) noexcept {
    auto& context = *static_cast<EnergyShotContext*>(opaque);
    switch (op) {
    case 0:
        if (context.kind == 0) g_pending_cannon_imp_weapon = nullptr;
        else g_pending_ammunition_weapon = nullptr;
        return 1;
    case 1: return g_runtime_ready && context.weapon;
    case 2:
    case 11:
        context.policy = policy_for_weapon(context.weapon);
        return context.policy != nullptr;
    case 3:
        context.amount = ammunition_amount_for_weapon(
            context.weapon, *context.policy);
        return context.amount && a2fo::energy_systems::can_consume(
            *context.amount, context.policy->cost);
    case 4: return is_resupply_weapon(context.weapon);
    case 5: {
        const auto current = g_resupply_weapon_targets.find(context.weapon);
        if (current == g_resupply_weapon_targets.end() ||
            current->second.receiver !=
                reinterpret_cast<const void*>(context.first)) {
            return 0;
        }
        context.provider = current->second.provider;
        return 1;
    }
    case 6:
        return resupply_receiver_active(
            context.provider, reinterpret_cast<const void*>(context.first));
    case 7:
        if (context.kind == 4) {
            return a2fo_energy_call_thiscall_3(
                g_weapon_select_position_ordnance_hook.gateway,
                context.weapon, context.first, context.second, context.third);
        }
        return a2fo_energy_call_thiscall_1(
            context.kind == 0
                ? g_cannon_imp_select_target_ordnance_hook.gateway
                : g_weapon_select_target_ordnance_hook.gateway,
            context.weapon, context.first);
    case 8:
        if (context.kind == 0) g_pending_cannon_imp_weapon = context.weapon;
        else g_pending_ammunition_weapon = context.weapon;
        return 1;
    case 9:
        return context.kind == 1 || context.kind == 2
            ? g_pending_cannon_imp_weapon == context.weapon
            : g_pending_ammunition_weapon == context.weapon;
    case 10:
        if (context.kind == 1 || context.kind == 2)
            g_pending_cannon_imp_weapon = nullptr;
        else g_pending_ammunition_weapon = nullptr;
        return 1;
    case 12:
        context.amount = ammunition_amount_for_weapon(
            context.weapon, *context.policy);
        return context.amount != nullptr;
    case 13:
        *context.amount = a2fo::energy_systems::consume(
            *context.amount, context.policy->cost);
        return 1;
    case 14:
        if (context.kind == 5) {
            a2fo_energy_call_thiscall_0(
                g_weapon_commit_shot_hook.gateway, context.weapon);
            return 1;
        }
        a2fo_energy_call_thiscall_1(
            context.kind == 1
                ? g_cannon_imp_launch_target_ordnance_hook.gateway
                : g_weapon_launch_position_ordnance_hook.gateway,
            context.weapon, context.first);
        return 1;
    default: return 0;
    }
}

[[maybe_unused]] bool resupply_weapon_allows_shot(
    void* weapon, const void* target) noexcept {
    EnergyShotContext context{
        weapon, reinterpret_cast<std::uintptr_t>(target), 0, 0, 3};
    return a2fo_rs_stage8_energy_shot_allowed(
        &energy_shot_step, &context, target != nullptr) != 0;
}

std::uintptr_t __attribute__((fastcall))
cannon_imp_select_target_ordnance_hook(
    void* weapon, void*, std::uintptr_t target) noexcept {
    EnergyShotContext context{weapon, target, 0, 0, 0};
    return a2fo_rs_stage8_energy_shot(
        &energy_shot_step, &context, 0, target != 0);
}

void __attribute__((fastcall)) cannon_imp_launch_target_ordnance_hook(
    void* weapon, void*, std::uintptr_t ordnance) noexcept {
    EnergyShotContext context{weapon, ordnance, 0, 0, 1};
    a2fo_rs_stage8_energy_shot(&energy_shot_step, &context, 1, 0);
}

void __attribute__((fastcall)) weapon_launch_position_ordnance_hook(
    void* weapon, void*, std::uintptr_t ordnance) noexcept {
    EnergyShotContext context{weapon, ordnance, 0, 0, 2};
    a2fo_rs_stage8_energy_shot(&energy_shot_step, &context, 2, 0);
}

std::uintptr_t __attribute__((fastcall)) weapon_select_target_ordnance_hook(
    void* weapon, void*, std::uintptr_t target) noexcept {
    EnergyShotContext context{weapon, target, 0, 0, 3};
    return a2fo_rs_stage8_energy_shot(
        &energy_shot_step, &context, 3, target != 0);
}

std::uintptr_t __attribute__((fastcall)) weapon_select_position_ordnance_hook(
    void* weapon, void*, std::uintptr_t x, std::uintptr_t y,
    std::uintptr_t z) noexcept {
    EnergyShotContext context{weapon, x, y, z, 4};
    return a2fo_rs_stage8_energy_shot(&energy_shot_step, &context, 4, 0);
}

void __attribute__((fastcall)) weapon_commit_shot_hook(
    void* weapon, void*) noexcept {
    EnergyShotContext context{weapon, 0, 0, 0, 5};
    a2fo_rs_stage8_energy_shot(&energy_shot_step, &context, 5, 0);
}

struct EnergyPersistenceContext {
    void* craft;
    void* stream;
    bool load;
    Stores* stores = nullptr;
    const CraftPolicy* policy = nullptr;
    SavedStores saved{};
    LegacySavedStores legacy{};
};

std::uintptr_t energy_persistence_step(void* opaque, std::uint32_t op,
                                       std::uintptr_t index) noexcept {
    auto& context = *static_cast<EnergyPersistenceContext*>(opaque);
    switch (op) {
    case 0:
        return a2fo_energy_call_thiscall_1(
            context.load ? g_craft_load_hook.gateway : g_craft_save_hook.gateway,
            context.craft,
            reinterpret_cast<std::uintptr_t>(context.stream));
    case 1: return g_persistence_ready;
    case 2:
        context.stores = stores_for_craft(context.craft, true);
        return context.stores != nullptr;
    case 3:
        context.saved.photon = context.stores->photon;
        context.saved.quantum = context.stores->quantum;
        context.saved.shuttle_craft = context.stores->shuttle_craft;
        return 1;
    case 4: {
        const auto out = reinterpret_cast<FileOutBytes>(at(kFileOutBytesRva));
        return out && out(context.stream, &context.saved,
                          sizeof(context.saved), kSaveLabel);
    }
    case 5: {
        const auto in = reinterpret_cast<FileInBytes>(at(kFileInBytesRva));
        return in && in(context.stream, &context.saved,
                        sizeof(context.saved));
    }
    case 6:
        log_line("Ammunition-store save data is missing or invalid");
        return 1;
    case 7:
        context.policy = policy_for_craft(context.craft);
        return context.policy != nullptr;
    case 8: return context.saved.magic == kSaveMagic;
    case 9: {
        float* amount = index == 0 ? &context.stores->photon
            : index == 1 ? &context.stores->quantum
            : &context.stores->shuttle_craft;
        const float saved = index == 0 ? context.saved.photon
            : index == 1 ? context.saved.quantum
            : context.saved.shuttle_craft;
        const StorePolicy& store = index == 0 ? context.policy->photon
            : index == 1 ? context.policy->quantum
            : context.policy->shuttle_craft;
        *amount = a2fo::energy_systems::clamp_amount(saved, store);
        return 1;
    }
    case 10:
        std::memcpy(&context.legacy, &context.saved,
                    sizeof(context.legacy));
        return context.legacy.magic == kLegacySaveMagic &&
            context.legacy.version == kLegacySaveVersion;
    case 11: {
        float* amount = index == 0 ? &context.stores->photon
            : &context.stores->quantum;
        const float saved = index == 0 ? context.legacy.photon
            : context.legacy.quantum;
        const StorePolicy& store = index == 0 ? context.policy->photon
            : context.policy->quantum;
        *amount = a2fo::energy_systems::clamp_amount(saved, store);
        return 1;
    }
    default: return 0;
    }
}

std::uintptr_t __attribute__((fastcall)) craft_save_hook(
    void* craft, void*, void* writer) noexcept {
    EnergyPersistenceContext context{craft, writer, false};
    return a2fo_rs_stage8_energy_persistence(
        &energy_persistence_step, &context, 0);
}

std::uintptr_t __attribute__((fastcall)) craft_load_hook(
    void* craft, void*, void* reader) noexcept {
    EnergyPersistenceContext context{craft, reader, true};
    return a2fo_rs_stage8_energy_persistence(
        &energy_persistence_step, &context, 1);
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

struct EnergyAmountContext {
    void* craft;
    Ammunition ammunition;
    float value = 0.0f;
    Stores* stores = nullptr;
    const CraftPolicy* policy = nullptr;
};

const StorePolicy* amount_policy(
    const EnergyAmountContext& context) noexcept {
    if (!context.policy) return nullptr;
    switch (context.ammunition) {
        case Ammunition::photon: return &context.policy->photon;
        case Ammunition::quantum: return &context.policy->quantum;
        case Ammunition::shuttle_craft:
            return &context.policy->shuttle_craft;
    }
    return nullptr;
}

float* amount_store(const EnergyAmountContext& context) noexcept {
    if (!context.stores) return nullptr;
    switch (context.ammunition) {
        case Ammunition::photon: return &context.stores->photon;
        case Ammunition::quantum: return &context.stores->quantum;
        case Ammunition::shuttle_craft:
            return &context.stores->shuttle_craft;
    }
    return nullptr;
}

std::uintptr_t energy_amount_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t bits) noexcept {
    auto& context = *static_cast<EnergyAmountContext*>(opaque);
    switch (op) {
    case 0:
#ifdef A2FO_BUILTIN_STARTUP
        return g_runtime_ready;
#else
        return 1;
#endif
    case 1:
        context.stores = stores_for_craft(context.craft, true);
        return context.stores != nullptr;
    case 2:
        context.policy = policy_for_craft(context.craft);
        return context.policy != nullptr;
    case 3: {
        const float amount = *amount_store(context);
        std::uint32_t raw = 0;
        std::memcpy(&raw, &amount, sizeof(raw));
        return raw;
    }
    case 4: {
        const float maximum = amount_policy(context)->maximum;
        std::uint32_t raw = 0;
        std::memcpy(&raw, &maximum, sizeof(raw));
        return raw;
    }
    case 5: return static_cast<std::uintptr_t>(
        amount_policy(context)->mode);
    case 6: return in_resupply_range(context.craft);
    case 7: {
        const float remaining = a2fo::energy_systems::reload_seconds(
            *amount_store(context), *amount_policy(context), bits != 0);
        std::uint32_t raw = 0;
        std::memcpy(&raw, &remaining, sizeof(raw));
        return raw;
    }
    case 8:
        *amount_store(context) = a2fo::energy_systems::clamp_amount(
            context.value, *amount_policy(context));
        return 1;
    case 9: {
        a2fo_rs_stage10_energy_add(
            &context.value, static_cast<std::uint32_t>(bits));
        return 1;
    }
    default: return 0;
    }
}

float amount_result(void* craft, Ammunition ammunition,
                    std::uint32_t kind, float value = 0.0f) noexcept {
    EnergyAmountContext context{craft, ammunition, value};
    const std::uint32_t bits = a2fo_rs_stage8_energy_amount(
        &energy_amount_step, &context, kind);
    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

float current_amount(void* craft, Ammunition ammunition) noexcept {
    return amount_result(craft, ammunition, 0);
}

float maximum_amount(void* craft, Ammunition ammunition) noexcept {
    return amount_result(craft, ammunition, 1);
}

float remaining_reload_seconds(
    void* craft, Ammunition ammunition) noexcept {
    return amount_result(craft, ammunition, 2);
}

void set_amount(void* craft, Ammunition ammunition, float amount) noexcept {
    amount_result(craft, ammunition, 3, amount);
}

void add_amount(void* craft, Ammunition ammunition, float amount) noexcept {
    amount_result(craft, ammunition, 4, amount);
}

struct EnergyUiRenderContext {
    void* info_display;
    void* craft;
    const CraftPolicy* policy = nullptr;
    Stores* stores = nullptr;
    void* text_component = nullptr;
    NativeRectangle anchor{};
    std::array<NativeRectangle, 3> rectangles{};
    Colour native_colour{};
    std::array<Colour, 3> colours{};
    char text[3][128]{};
};

const StorePolicy& ui_store_policy(
    const CraftPolicy& policy, std::uintptr_t index) noexcept {
    return index == 0 ? policy.photon
        : index == 1 ? policy.quantum : policy.shuttle_craft;
}

float ui_store_amount(const Stores& stores,
                      std::uintptr_t index) noexcept {
    return index == 0 ? stores.photon
        : index == 1 ? stores.quantum : stores.shuttle_craft;
}

const RawRectangle& ui_configured_rectangle(
    const UiConfiguration& config, std::uintptr_t index) noexcept {
    return index == 0 ? config.photon_rectangle
        : index == 1 ? config.quantum_rectangle
        : config.shuttle_craft_rectangle;
}

const Colour& ui_configured_colour(
    const UiConfiguration& config, std::uintptr_t index) noexcept {
    return index == 0 ? config.photon_colour
        : index == 1 ? config.quantum_colour
        : config.shuttle_craft_colour;
}

std::uintptr_t energy_ui_render_step(
    void* opaque, std::uint32_t op, std::uintptr_t value) noexcept {
    auto& context = *static_cast<EnergyUiRenderContext*>(opaque);
    switch (op) {
    case 0:
        return g_runtime_ready && g_ui_ready && context.info_display &&
            context.craft;
    case 1:
        context.policy = policy_for_craft(context.craft);
        return context.policy != nullptr;
    case 2:
        return ui_store_policy(*context.policy, value).maximum > 0.0f;
    case 3:
        context.stores = stores_for_craft(context.craft, true);
        return context.stores != nullptr;
    case 4: {
        void* parameter_db = gui_parameter_db();
        if (!g_ui_configuration.loaded ||
            g_ui_configuration.parameter_db != parameter_db) {
            refresh_ui_configuration(parameter_db);
        }
        return 1;
    }
    case 5:
        context.text_component = read_ui_live<void*>(
            context.info_display, kInfoDisplayCaptainTextOffset, nullptr);
        return context.text_component != nullptr;
    case 6:
        context.anchor = read_ui_live<NativeRectangle>(
            context.text_component, kTextComponentLiveRectangleOffset,
            NativeRectangle{});
        return usable_native_rectangle(context.anchor);
    case 7:
        a2fo_rs_stage10_energy_default_rectangles(
            &context.rectangles[0].left, &context.anchor.left);
        return 1;
    case 8:
        return g_ui_configuration.anchor_found &&
            (value == 0 ? g_ui_configuration.photon_rectangle_found
                : value == 1 ? g_ui_configuration.quantum_rectangle_found
                : g_ui_configuration.shuttle_craft_rectangle_found);
    case 9:
        context.rectangles[value] = translated_rectangle(
            context.anchor, g_ui_configuration.anchor,
            ui_configured_rectangle(g_ui_configuration, value));
        return 1;
    case 10:
        context.native_colour = text_component_colour(context.text_component);
        return 1;
    case 11: return g_ui_configuration.shared_colour_found;
    case 12:
        return value == 0 ? g_ui_configuration.photon_colour_found
            : value == 1 ? g_ui_configuration.quantum_colour_found
            : g_ui_configuration.shuttle_craft_colour_found;
    case 13: {
        const std::uintptr_t index = value & 3;
        const std::uintptr_t source = value >> 2;
        context.colours[index] = source == 2
            ? ui_configured_colour(g_ui_configuration, index)
            : source == 1 ? g_ui_configuration.shared_colour
            : context.native_colour;
        return 1;
    }
    case 14: return in_resupply_range(context.craft);
    case 15:
        return static_cast<std::uintptr_t>(
            ui_store_policy(*context.policy, value).mode);
    case 16: {
        const std::uintptr_t index = value & 3;
        const std::uintptr_t status = value >> 2;
        static constexpr const char* kNames[3] = {
            "Photon Torpedoes", "Quantum Torpedoes", "Shuttle Craft"};
        static constexpr const char* kStatuses[4] = {
            "NO RECHARGE", "AUTO", "SUPPLY", "NO SUPPLY"};
        std::snprintf(
            context.text[index], sizeof(context.text[index]),
            "%s: %d/%d  [%s]", kNames[index],
            whole_ammunition_amount(ui_store_amount(*context.stores, index)),
            whole_ammunition_amount(
                ui_store_policy(*context.policy, index).maximum),
            kStatuses[status]);
        return 1;
    }
    case 17:
        draw_ammunition_text(
            context.text[value], context.rectangles[value],
            context.colours[value], context.text_component);
        return 1;
    default: return 0;
    }
}

void A2FO_CALL selected_info_render_handler(
    void* info_display, void* selected_craft, void*) noexcept {
    EnergyUiRenderContext context{info_display, selected_craft};
    a2fo_rs_stage8_energy_ui_render(
        &energy_ui_render_step, &context);
}

HMODULE selected_info_module() noexcept {
    return a2fo::startup_module_provider(
        "A2FOCraftIdentity.dll",
        {"A2FOCraftIdentity_TakeLoadedDirectionalShields"});
}

bool register_selected_info_observer(
    RegisterSelectedInfoObserver registration) noexcept {
    return registration && registration(
        kModuleName, &selected_info_render_handler, nullptr);
}

struct EnergyInitContext {
    const A2FO_ModuleApi* api;
    std::exception_ptr exception;
    HMODULE identity = nullptr;
    RegisterSelectedInfoObserver observer = nullptr;
};

std::uintptr_t energy_init_step(void* opaque, std::uint32_t op,
                                std::uintptr_t value) noexcept {
    auto& context = *static_cast<EnergyInitContext*>(opaque);
    const A2FO_ModuleApi* api = context.api;
    try {
        switch (op) {
        case 0:
            return api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
                api->api_version == A2FO_MODULE_API_VERSION && api->log &&
                api->armada_module && api->fleetops_module &&
                api->install_inline_hook &&
                A2FO_MODULE_API_HAS(api, register_game_object_class_loaded_handler) &&
                api->register_game_object_class_loaded_handler &&
                A2FO_MODULE_API_HAS(api, register_weapon_class_loaded_handler) &&
                api->register_weapon_class_loaded_handler &&
                A2FO_MODULE_API_HAS(api, register_craft_event_handler) &&
                api->register_craft_event_handler &&
                (api->capabilities & A2FO_CAP_GAME_OBJECT_CLASS_LOADED) != 0 &&
                (api->capabilities & A2FO_CAP_WEAPON_CLASS_LOADED) != 0 &&
                (api->capabilities & A2FO_CAP_CRAFT_EVENTS) != 0;
        case 1:
            g_api = api;
            g_armada = static_cast<HMODULE>(api->armada_module());
            return 1;
        case 2:
            g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
            return 1;
        case 3: return g_armada && g_fleet_ops;
        case 4:
            switch (value) {
            case 0: return signature_matches(
                kWeaponGetOwnerRva, kExpectedWeaponGetOwner);
            case 1: return signature_matches(
                kWeaponSelectTargetOrdnanceRva,
                kExpectedWeaponSelectTargetOrdnance);
            case 2: return signature_matches(
                kWeaponSelectPositionOrdnanceRva,
                kExpectedWeaponSelectPositionOrdnance);
            case 3: return signature_matches(
                kWeaponCommitShotRva, kExpectedWeaponCommitShot);
            case 4: return signature_matches(
                kWeaponLaunchPositionOrdnanceRva,
                kExpectedWeaponLaunchPositionOrdnance);
            case 5: return signature_matches(
                g_fleet_ops, kCannonImpLaunchTargetOrdnanceRva,
                kExpectedCannonImpLaunchTargetOrdnance);
            case 6: return signature_matches(
                g_fleet_ops, kCannonImpSelectTargetOrdnanceRva,
                kExpectedCannonImpSelectTargetOrdnance);
            default: return 0;
            }
        case 5: g_runtime_ready = true; return 1;
        case 6:
            return api->register_game_object_class_loaded_handler(
                kModuleName, kCraftFields.data(),
                static_cast<std::uint32_t>(kCraftFields.size()),
                &craft_class_loaded_handler, nullptr);
        case 7:
            return api->register_weapon_class_loaded_handler(
                kModuleName, kWeaponFields.data(),
                static_cast<std::uint32_t>(kWeaponFields.size()),
                &weapon_class_loaded_handler, nullptr);
        case 8:
            return api->register_craft_event_handler(
                kModuleName, &craft_event_handler, nullptr);
        case 9: g_runtime_ready = false; return 1;
        case 10:
            return (api->capabilities & A2FO_CAP_WEAPON_TRIGGER_EVENTS) != 0 &&
                (A2FO_MODULE_API_HAS(api, register_weapon_trigger_handler_masked) &&
                         api->register_weapon_trigger_handler_masked
                    ? api->register_weapon_trigger_handler_masked(
                          kModuleName,
                          A2FO_WEAPON_TRIGGER_EVENT_MASK_PRECHECK,
                          &resupply_weapon_trigger_handler, nullptr)
                    : A2FO_MODULE_API_HAS(api, register_weapon_trigger_handler) &&
                          api->register_weapon_trigger_handler &&
                          api->register_weapon_trigger_handler(
                              kModuleName, &resupply_weapon_trigger_handler,
                              nullptr));
        case 11:
            return api->register_classlabel_alias &&
                api->register_classlabel_alias(
                    kModuleName, kResupplyWeaponClasslabel,
                    kResupplyWeaponNativeClasslabel);
        case 12:
            return signature_matches(kWeaponTriggerRva,
                                     kExpectedWeaponTrigger);
        case 13:
            if ((value & 1) != 0) {
                g_trigger_weapon = &native_trigger_weapon;
                log_line("resupplyweapon classlabel initialized on native cannon");
            } else {
                log_line((value & 2) != 0
                    ? "resupplyweapon unavailable: trigger event or Weapon::Trigger signature missing; weapons will not fire"
                    : "resupplyweapon classlabel alias unavailable");
            }
            return 1;
        case 16:
            switch (value) {
            case 0: return signature_matches(
                kParameterDbGetRectangleRva, kExpectedParameterDbGetRectangle);
            case 1: return signature_matches(
                kParameterDbGetColorRva, kExpectedParameterDbGetColor);
            case 2: return signature_matches(
                kDisplayInterfaceDrawTextInRectangleRva,
                kExpectedDisplayInterfaceDrawTextInRectangle);
            default: return 0;
            }
        case 18: g_ui_ready = value != 0; return 1;
        case 19:
            log_line(g_persistence_ready
                ? "Per-shot Photon Torpedo, Quantum Torpedo, and Shuttle Craft stores initialized with save/load support"
                : "Per-shot Photon Torpedo, Quantum Torpedo, and Shuttle Craft stores initialized; save/load hooks unavailable");
            return 1;
        case 20:
            log_line(g_ui_ready
                ? "Selected-craft three-store ammunition UI initialized"
                : kSelectedInfoUiEnabled
                    ? "Selected-craft ammunition UI unavailable; CraftIdentity observer or native draw signatures did not match"
                    : "Direct ammunition UI renderer disabled; CraftIdentity may display exported ammunition values");
            return 1;
        case 21: return kSelectedInfoUiEnabled;
        case 22:
            if (value >= 256) {
                switch (value - 256) {
                case 0: return signature_matches(
                    kCraftLoadRva, kExpectedCraftLoad);
                case 1: return signature_matches(
                    kCraftSaveRva, kExpectedCraftSave);
                case 2: return readable_range(at(kFileOutBytesRva), 5);
                case 3: return readable_range(at(kFileInBytesRva), 5);
                default: return 0;
                }
            }
            switch (value) {
            case 0: return signature_matches(
                kWeaponSelectTargetOrdnanceRva,
                kExpectedWeaponSelectTargetOrdnance);
            case 1: return signature_matches(
                kWeaponSelectPositionOrdnanceRva,
                kExpectedWeaponSelectPositionOrdnance);
            case 2: return signature_matches(
                kWeaponCommitShotRva, kExpectedWeaponCommitShot);
            case 3: return signature_matches(
                kWeaponLaunchPositionOrdnanceRva,
                kExpectedWeaponLaunchPositionOrdnance);
            case 4: return signature_matches(
                g_fleet_ops, kCannonImpLaunchTargetOrdnanceRva,
                kExpectedCannonImpLaunchTargetOrdnance);
            case 5: return signature_matches(
                g_fleet_ops, kCannonImpSelectTargetOrdnanceRva,
                kExpectedCannonImpSelectTargetOrdnance);
            default: return 0;
            }
        case 23:
            if (value >= 256) {
                switch (value - 256) {
                case 0: return install_hook(
                    kCraftLoadRva, reinterpret_cast<void*>(&craft_load_hook),
                    kExpectedCraftLoad, &g_craft_load_hook);
                case 1: return install_hook(
                    kCraftSaveRva, reinterpret_cast<void*>(&craft_save_hook),
                    kExpectedCraftSave, &g_craft_save_hook);
                default: return 0;
                }
            }
            switch (value) {
            case 0: return install_hook(
                kWeaponSelectTargetOrdnanceRva,
                reinterpret_cast<void*>(&weapon_select_target_ordnance_hook),
                kExpectedWeaponSelectTargetOrdnance,
                &g_weapon_select_target_ordnance_hook);
            case 1: return install_hook(
                kWeaponSelectPositionOrdnanceRva,
                reinterpret_cast<void*>(&weapon_select_position_ordnance_hook),
                kExpectedWeaponSelectPositionOrdnance,
                &g_weapon_select_position_ordnance_hook);
            case 2: return install_hook(
                kWeaponCommitShotRva,
                reinterpret_cast<void*>(&weapon_commit_shot_hook),
                kExpectedWeaponCommitShot, &g_weapon_commit_shot_hook);
            case 3: return install_hook(
                kWeaponLaunchPositionOrdnanceRva,
                reinterpret_cast<void*>(&weapon_launch_position_ordnance_hook),
                kExpectedWeaponLaunchPositionOrdnance,
                &g_weapon_launch_position_ordnance_hook);
            case 4: return install_hook(
                g_fleet_ops, kCannonImpSelectTargetOrdnanceRva,
                reinterpret_cast<void*>(&cannon_imp_select_target_ordnance_hook),
                kExpectedCannonImpSelectTargetOrdnance,
                &g_cannon_imp_select_target_ordnance_hook);
            case 5: return install_hook(
                g_fleet_ops, kCannonImpLaunchTargetOrdnanceRva,
                reinterpret_cast<void*>(&cannon_imp_launch_target_ordnance_hook),
                kExpectedCannonImpLaunchTargetOrdnance,
                &g_cannon_imp_launch_target_ordnance_hook);
            default: return 0;
            }
        case 24: g_persistence_ready = value != 0; return 1;
        case 25:
            context.identity = selected_info_module();
            return context.identity != nullptr;
        case 26: {
            FARPROC exported = GetProcAddress(
                context.identity,
                "A2FOCraftIdentity_RegisterSelectedInfoObserver");
            static_assert(sizeof(context.observer) == sizeof(exported),
                          "unexpected function-pointer size");
            std::memcpy(&context.observer, &exported,
                        sizeof(context.observer));
            return context.observer != nullptr;
        }
        case 27:
            return register_selected_info_observer(context.observer);
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
    EnergyInitContext context{api, {}};
    const bool result = a2fo_rs_stage8_energy_init(
        &energy_init_step, &context) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return result;
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
float A2FO_CALL A2FOEnergySystems_GetShuttleCraft(void* craft) {
    return current_amount(craft, Ammunition::shuttle_craft);
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
float A2FO_CALL A2FOEnergySystems_GetMaximumShuttleCraft(void* craft) {
    return maximum_amount(craft, Ammunition::shuttle_craft);
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
float A2FO_CALL A2FOEnergySystems_GetShuttleCraftReloadSeconds(void* craft) {
    return remaining_reload_seconds(craft, Ammunition::shuttle_craft);
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
void A2FO_CALL A2FOEnergySystems_SetShuttleCraft(
    void* craft, float amount) {
    set_amount(craft, Ammunition::shuttle_craft, amount);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOEnergySystems_AddPhotonTorpedoes(
    void* craft, float amount) {
    add_amount(craft, Ammunition::photon, amount);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOEnergySystems_AddQuantumTorpedoes(
    void* craft, float amount) {
    add_amount(craft, Ammunition::quantum, amount);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOEnergySystems_AddShuttleCraft(
    void* craft, float amount) {
    add_amount(craft, Ammunition::shuttle_craft, amount);
}

// Selected-panel icon state for CraftIdentity: -1 when the Weapon is not a
// resupplyweapon, 1 while it has a valid receiver in its owner's range,
// otherwise 0.
extern "C" __declspec(dllexport)
std::int32_t A2FO_CALL A2FOEnergySystems_GetResupplyWeaponState(void* weapon) {
    struct Context {
        void* weapon;
        void* provider = nullptr;
        const void* receiver = nullptr;
    } context{weapon};
    const auto step = [](void* opaque, std::uint32_t op,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: return g_runtime_ready && state.weapon;
        case 1: return is_resupply_weapon(state.weapon);
        case 2: {
            const auto current = g_resupply_weapon_targets.find(state.weapon);
            if (current == g_resupply_weapon_targets.end()) return 0;
            state.provider = current->second.provider;
            state.receiver = current->second.receiver;
            return 1;
        }
        case 3:
            return resupply_receiver_active(state.provider, state.receiver);
        default: return 0;
        }
    };
    return a2fo_rs_stage8_energy_resupply_state(step, &context);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    const auto step = [](void*, std::uint32_t op,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        if (op == 0) g_runtime_ready = false;
        else if (op == 1) g_trigger_weapon = nullptr;
        return 1;
    };
    a2fo_rs_stage8_energy_shutdown(step, nullptr);
}
