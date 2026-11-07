/*
 * File: core/dllmain.cpp
 * Module: A2FOHookExtensions (main-hook)
 * Purpose: Core API setup, binary validation, root ordering, shared hook
 * ownership, and optional module loading.
 */

#include "extension_roots.hpp"
#include "initialize_boundary.hpp"
#include "../rust/runtime_globals_ffi.h"
#include "runtime_handlers.hpp"
#include "../rust/native_hooks_ffi.h"
#include "module_loader.hpp"
#include "module_menu.hpp"
#include "builtin_modules.hpp"
#include "nebula_renderer.hpp"
#include "native11_light_classes.hpp"
#include "class_load_context.hpp"
#include "../renderer/native11/light_config.hpp"
#include "renderer_options.hpp"
#include "../rust/stage8_core_ffi.h"
#include "../rust/stage8_core_events_ffi.h"
#include "../rust/stage8_core_lifecycle_ffi.h"
#include "../rust/stage8_core_settings_ffi.h"
#include "../rust/stage8_core_cocoon_ffi.h"
#include "../rust/stage8_core_classdefaults_ffi.h"
#include "../rust/stage9_core_parameters_ffi.h"
#include "../rust/stage9_core_shell_ffi.h"
#include "../rust/stage9_core_config_ffi.h"
#include "../rust/stage8_core_lookup_ffi.h"
#include "../rust/stage8_core_classload_ffi.h"
#include "../rust/stage8_core_odf_ffi.h"
#include "../rust/stage8_core_dispatch_ffi.h"
#include "../renderer/d3d8_compat/observer.hpp"
#include "../renderer/refit/startup.hpp"
#include "../sdk/include/a2fo_module_api.h"
#include "../sdk/include/a2fo_supported_armada.hpp"
#include "../sdk/include/a2fo_supported_fleetops.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using OdfSnapshot = std::map<std::string, std::string>;

enum class ReplacementOwner {
    neutral,
    original,
};

struct ObjectReplacement {
    std::string odf;
    bool inherit_position = true;
    bool inherit_rotation = true;
    ReplacementOwner owner = ReplacementOwner::neutral;
};

// This file bridges two different binaries and ABIs:
//   * ArmadaL.exe owns the gameplay classes and SOD database (MSVC thiscall).
//   * FleetOpsHook.dll owns the replacement virtual filesystem (Delphi register
//     convention: EAX/EDX/ECX before any stack arguments).
// Every address below is an RVA from the supported module's load base, never a
// process-global absolute address. docs/addresses.md records their provenance.
constexpr std::uint32_t kFleetOpsTimestamp = 0x51f6475c;
constexpr std::uint32_t kFleetOpsImageSize = 0x00322000;

constexpr std::uintptr_t kEvolverClassBuildClassRva = 0x0a85e0;
constexpr std::uintptr_t kEvolverClassDtorRva = 0x0a85d0;
constexpr std::uintptr_t kCocoonSelectorRva = 0x0b0534;
constexpr std::uintptr_t kCocoonUpdateSelectorRva = 0x400d10;
constexpr std::uintptr_t kCocoonUpdateResumeRva = 0x0b0c1b;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x135350;
constexpr std::uintptr_t kParameterDbGetIntRva = 0x134bf0;
constexpr std::uintptr_t kParameterDbGetIntAlternativeRva = 0x134cf0;
constexpr std::uintptr_t kParameterDbGetFloatRva = 0x134df0;
constexpr std::uintptr_t kParameterDbGetBoolRva = 0x134f50;
constexpr std::uintptr_t kParameterDbGetOwnedStringRva = 0x1354a0;
constexpr std::uintptr_t kParameterDbGetStringVectorRva = 0x135e80;
constexpr std::uintptr_t kAiMissionGetCurrentRva = 0x00001370;
constexpr std::uintptr_t kCraftExplodeRva = 0x0c6ab0;
constexpr std::uintptr_t kGameObjectClassFindProjectIdRva = 0x0cd1f0;
constexpr std::uintptr_t kGameObjectClassFindRva = 0x0cd370;
constexpr std::uintptr_t kGameObjectClassConstructRva = 0x0cd390;
constexpr std::uintptr_t kGameObjectClassGetOdfNameRva = 0x0ce370;
constexpr std::uintptr_t kGameObjectGetTransformRva = 0x0cfd50;
constexpr std::uintptr_t kCraftCleanupRva = 0x0c1fd0;
constexpr std::uintptr_t kCraftPostLoadRva = 0x0c2870;
constexpr std::uintptr_t kCraftSimulateRva = 0x0c6530;
constexpr std::uintptr_t kWeaponClassConstructorRva = 0x264e30;
constexpr std::uintptr_t kWeaponTriggerObjectRva = 0x271290;
constexpr std::uintptr_t kDefaultUserProfileGameSpeedRva = 0x13c8a0;
constexpr std::uintptr_t kLoadSodRva = 0x22cf10;
constexpr std::uintptr_t kSodDatabaseRva = 0x3ad508;
constexpr std::uintptr_t kDefaultCocoonRva = 0x33fccc;
constexpr std::uintptr_t kAlternativeCocoonRva = 0x33fd3c;
constexpr char kDefaultCocoonName[] = "8472_cocoon.sod";
constexpr char kAlternativeCocoonName[] = "8472_cocoon2.sod";

// FleetOpsHook.dll RVAs.
constexpr std::uintptr_t kFofsItemGetHashLookupCallRva = 0x105fec;
constexpr std::uintptr_t kFofsItemLocateHashLookupCallRva = 0x1061e2;
constexpr std::uintptr_t kFofsItemExistsHashLookupCallRva = 0x106263;
constexpr std::uintptr_t kFofsProjectIdHashLookupCallRva = 0x1063ee;
constexpr std::uintptr_t kGetFileFromHashTableRva = 0x109c14;
constexpr std::uintptr_t kGetModUserDirectoryRva = 0x10ab98;
constexpr std::uintptr_t kFoSettingsGetInstanceRva = 0x13e744;
constexpr std::uintptr_t kFoSettingsSaveRva = 0x13e824;
constexpr std::uintptr_t kGameConfigurationNewRva = 0x13e93c;
constexpr std::uintptr_t kGameConfigurationLoadProfileRva = 0x13ea8c;
constexpr std::uintptr_t kDelphiLStrAsgRva = 0x00570c;
constexpr std::uintptr_t kRaceParameterDbDestructorCallRva = 0x10b98b;
constexpr std::uintptr_t kParameterDbDestructorRva = 0x1e2c28;
constexpr std::uintptr_t kFoCraftSimulateHandlerRva = 0x1dcebc;
constexpr std::uintptr_t kFoWeaponClassConstructorHandlerRva = 0x10ef74;

constexpr std::size_t kMaximumPathLength = 32767;
// Temporarily leave Fleet Ops/Armada in control of shell and menu sizing.
// The monitor remains available for later repair without affecting gameplay.
constexpr bool kShellDisplayMonitorEnabled = false;

// A timestamp alone is not enough protection for an injected hook. Each patch
// also verifies the exact instructions it is about to replace, and fails
// closed when another Fleet Ops/Armada build is detected.
const std::uint8_t kExpectedBuildClass[] = {0x55, 0x8b, 0xec, 0x6a, 0xff};
const std::uint8_t kExpectedDtor[] = {0xc7, 0x01, 0x44, 0x1b, 0x6b, 0x00};
const std::uint8_t kExpectedCocoonJump[] = {0xe9, 0xa7, 0x07, 0x35, 0x00};
const std::uint8_t kExpectedCocoonUpdateSelector[] = {
    0x8b, 0x83, 0xe8, 0x01, 0x00, 0x00};
const std::uint8_t kExpectedParameterDbGetString[] = {
    0x55,
    0x8b, 0xec,
    0x81, 0xec, 0x00, 0x01, 0x00, 0x00
};
const std::uint8_t kExpectedParameterDbGetScalar[] = {
    0x55,
    0x8b, 0xec,
    0x81, 0xec, 0x00, 0x01, 0x00, 0x00
};
const std::uint8_t kExpectedAiMissionGetCurrent[] = {
    0xa1, 0xdc, 0x47, 0x73, 0x00, 0xc3};
const std::uint8_t kExpectedCraftExplode[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x20};
const std::uint8_t kExpectedGameObjectClassFindProjectId[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
const std::uint8_t kExpectedGameObjectClassFind[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
const std::uint8_t kExpectedGameObjectClassConstruct[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x84, 0x00, 0x00, 0x00};
const std::uint8_t kExpectedGameObjectClassGetOdfName[] = {
    0x8b, 0x89, 0xcc, 0x01, 0x00, 0x00,
    0xe9, 0x25, 0xb0, 0x18, 0x00};
const std::uint8_t kExpectedGameObjectGetTransform[] = {
    0x8b, 0x41, 0x04, 0x83, 0xc0, 0x44, 0xc3};
const std::uint8_t kExpectedCraftCleanup[] = {
    0x56, 0x8b, 0xf1, 0x8b, 0x8e, 0x34, 0x02, 0x00, 0x00};
const std::uint8_t kExpectedCraftPostLoad[] = {
    0x53, 0x56, 0x57, 0x8b, 0xf1};
const std::uint8_t kExpectedCraftSimulate[] = {
    0x55, 0x8b, 0xec, 0x53, 0x8b, 0x5d, 0x08};
const std::uint8_t kExpectedWeaponClassConstructor[] = {
    0x55, 0x8b, 0xec, 0x6a, 0xff};
const std::uint8_t kExpectedWeaponTriggerObject[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
const std::uint8_t kExpectedFoCraftSimulateHandler[] = {
    0x55, 0x8b, 0xec, 0x51, 0x53, 0x89, 0x4d};
const std::uint8_t kExpectedFoWeaponClassConstructorHandler[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xdc, 0x53};
const std::uint8_t kExpectedDefaultUserProfileGameSpeed[] = {
    0xb8, 0x05, 0x00, 0x00, 0x00, 0xc3};
const std::uint8_t kExpectedFofsItemGetHashLookupCall[] = {
    0xe8, 0x23, 0x3c, 0x00, 0x00};
const std::uint8_t kExpectedFofsItemLocateHashLookupCall[] = {
    0xe8, 0x2d, 0x3a, 0x00, 0x00};
const std::uint8_t kExpectedFofsItemExistsHashLookupCall[] = {
    0xe8, 0xac, 0x39, 0x00, 0x00};
const std::uint8_t kExpectedFofsProjectIdHashLookupCall[] = {
    0xe8, 0x21, 0x38, 0x00, 0x00};
const std::uint8_t kExpectedGetModUserDirectory[] = {
    0x53, 0x56, 0x57, 0x8b, 0xf2};
const std::uint8_t kExpectedFoSettingsGetInstance[] = {
    0x55, 0x8b, 0xec, 0x6a, 0x00};
const std::uint8_t kExpectedGameConfigurationNew[] = {
    0x55, 0x8b, 0xec, 0x51, 0x53};
const std::uint8_t kExpectedGameConfigurationLoadProfile[] = {
    0x55, 0x8b, 0xec, 0x51, 0x53};
const std::uint8_t kExpectedRaceParameterDbDestructorCall[] = {
    0xe8, 0x98, 0x72, 0x0d, 0x00};
const std::uint8_t kExpectedParameterDbDestructor[] = {
    0x55, 0x8b, 0xec, 0x51, 0x89, 0x45, 0xfc};

// Implemented in delphi_bridge.S. Normal C++ calls cannot express all of the
// Delphi register ABI and Armada's 32-bit thiscall ABI reliably, so these
// wrappers translate ordinary C stack arguments into the required registers.
extern "C" void* a2fo_call_get_file_from_hash(void* function,
                                                void* file_system,
                                                void* delphi_name,
                                                std::uint32_t flags);
extern "C" void* a2fo_call_evolver_build_class(void* function, void* self,
                                                void* parameter_db);
extern "C" void* a2fo_call_evolver_dtor(void* function, void* self);
extern "C" bool a2fo_parameter_db_get_string(void* function, void* parameter_db,
                                               const char* key, char* output,
                                               std::uint32_t output_size,
                                               const char* default_value);
extern "C" bool a2fo_parameter_db_get_scalar(
    void* function, void* parameter_db, const char* key, void* output,
    std::uint32_t default_value_bits);
extern "C" void* a2fo_load_sod(void* function, void* database,
                                const char* name);
extern "C" void a2fo_call_craft_explode(void* function, void* self);
extern "C" void* a2fo_call_game_object_method(void* function, void* self);
extern "C" void* a2fo_call_game_object_construct(
    void* function, void* object_class, const void* transform,
    int team, int parent, char* label);
extern "C" void a2fo_call_game_object_argument(
    void* function, void* self, void* argument);
extern "C" std::uintptr_t a2fo_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
extern "C" std::uintptr_t a2fo_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
extern "C" void* a2fo_call_delphi_one_register(
    void* function, void* eax_argument);
extern "C" void* a2fo_call_delphi_two_registers(
    void* function, void* eax_argument, void* edx_argument);
extern "C" void a2fo_cocoon_selector_hook();
extern "C" void a2fo_cocoon_update_selector_hook();
extern "C" void a2fo_race_parameter_db_dispatch_bridge();
extern "C" void* g_a2fo_race_parameter_db_destructor;

HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
extern "C" {
void* g_a2fo_race_parameter_db_destructor = nullptr;
}
A2foRuntimeGlobals* const g_runtime_globals = a2fo_rs_runtime_globals();
HANDLE& g_log = g_runtime_globals->log;
CRITICAL_SECTION g_state_lock;
bool g_state_lock_ready = false;
bool g_evolver_hooks_ready = false;
bool g_fofs_item_get_lookup_hook_ready = false;
bool g_mod_user_directory_hook_ready = false;
bool g_fo_settings_default_hook_ready = false;
bool g_default_game_speed_hook_ready = false;
bool g_runtime_game_speed_hook_ready = false;
bool g_user_profile_game_speed_hook_ready = false;
bool g_object_destroyed_hook_ready = false;
bool g_game_object_class_loaded_hook_ready = false;
bool g_race_loaded_hook_ready = false;
bool g_weapon_class_loaded_hook_ready = false;
bool g_weapon_trigger_hook_ready = false;
bool g_craft_event_hooks_ready = false;
bool g_module_menu_hook_ready = false;
bool g_renderer_options_hook_ready = false;
A2foRsInlineHook g_build_class_hook{};
A2foRsInlineHook g_dtor_hook{};
A2foRsInlineHook g_parameter_db_get_string_hook{};
A2foRsInlineHook g_parameter_db_get_int_hook{};
A2foRsInlineHook g_parameter_db_get_int_alternative_hook{};
A2foRsInlineHook g_parameter_db_get_float_hook{};
A2foRsInlineHook g_parameter_db_get_bool_hook{};
A2foRsInlineHook g_parameter_db_get_owned_string_hook{};
A2foRsInlineHook g_parameter_db_get_string_vector_hook{};
A2foRsInlineHook g_craft_explode_hook{};
A2foRsInlineHook g_game_object_class_find_project_id_hook{};
A2foRsInlineHook g_weapon_class_constructor_hook{};
A2foRsInlineHook g_weapon_trigger_object_hook{};
A2foRsInlineHook g_craft_cleanup_hook{};
A2foRsInlineHook g_craft_post_load_hook{};
A2foRsInlineHook g_craft_simulate_hook{};
void* g_weapon_class_constructor_original = nullptr;
void* g_craft_simulate_original = nullptr;
A2foRsInlineHook g_mod_user_directory_hook{};
A2foRsInlineHook g_fo_settings_get_instance_hook{};
A2foRsInlineHook g_game_configuration_new_hook{};
A2foRsInlineHook g_game_configuration_load_profile_hook{};
bool g_classlabel_alias_hook_ready = false;
std::unordered_map<void*, std::string> g_class_cocoons;
std::unordered_map<std::string, void*> g_loaded_cocoons;
std::set<void*> g_logged_cocoon_classes;
std::string g_root_directory;
std::vector<std::string> g_extension_roots;
a2fo::LoadedModules g_loaded_modules;
A2FO_ModuleApi g_module_api{};
volatile LONG& g_initialize_state = g_runtime_globals->initialize_state;
volatile LONG& g_deferred_initialization_finished = g_runtime_globals->deferred_finished;
volatile DWORD& g_initialize_thread_id = g_runtime_globals->initialize_thread_id;
volatile LONG g_mod_defaults_state = 0;
volatile LONG g_fo_settings_default_applied = 0;
volatile LONG g_fo_settings_first_run = 0;
volatile LONG g_default_game_speed_logged = 0;
volatile LONG g_runtime_game_speed_logged = 0;
volatile LONG g_user_profile_game_speed_logged = 0;
volatile PVOID g_fo_settings_instance = nullptr;
bool g_has_default_game_speed = false;
int g_default_game_speed = 0;
const char* g_delphi_settings_directory = nullptr;
A2FO_FofsItemLookupHandler g_fofs_item_lookup_handler = nullptr;
void* g_fofs_item_lookup_user_data = nullptr;
std::string g_fofs_item_lookup_owner;

// Module policy registration is startup-only. Once registration closes, publish
// the final FOFS dispatcher into immutable process-lifetime storage so the
// extremely hot FOFS_ItemGet path no longer needs g_state_lock for every file
// lookup. A release/acquire flag provides the publication barrier; the fields
// are never modified again while ready is non-zero.
struct FofsItemLookupDispatch {
    A2FO_FofsItemLookupHandler handler = nullptr;
    void* user_data = nullptr;
};
FofsItemLookupDispatch g_fofs_item_lookup_dispatch;
std::atomic<bool> g_fofs_item_lookup_dispatch_ready{false};
A2FO_InfoIniDefaultsHandler g_info_ini_defaults_handler = nullptr;
void* g_info_ini_defaults_user_data = nullptr;
std::string g_info_ini_defaults_owner;
struct ClasslabelAliasPolicy {
    std::string target;
    std::string owner;
};

struct ClasslabelOdfDefaultsPolicy {
    std::unordered_map<std::string, std::string> defaults;
    std::string owner;
};

struct RaceOdfDefaultPolicy {
    std::string value;
    std::string owner;
};

struct OdfOverlayDirectoryPolicy {
    std::string directory;
    std::string owner;
    std::uint32_t precedence = A2FO_ODF_OVERLAY_NORMAL;
};

struct NativeObjectDestroyedRegistration {
    std::string owner;
    std::vector<std::string> required_odf_fields;
    A2FO_ObjectDestroyedHandler handler = nullptr;
    void* user_data = nullptr;
    bool enabled = true;
};

struct NativeProducerEventRegistration {
    std::string owner;
    A2FO_ProducerEventHandler handler = nullptr;
    void* user_data = nullptr;
};

struct NativeGameObjectClassLoadedRegistration {
    std::string owner;
    std::vector<std::string> required_odf_fields;
    A2FO_GameObjectClassLoadedHandler handler = nullptr;
    void* user_data = nullptr;
};

struct NativeRaceLoadedRegistration {
    std::string owner;
    std::vector<std::string> required_odf_fields;
    A2FO_RaceLoadedHandler handler = nullptr;
    void* user_data = nullptr;
};

struct NativeWeaponClassLoadedRegistration {
    std::string owner;
    std::vector<std::string> required_odf_fields;
    A2FO_WeaponClassLoadedHandler handler = nullptr;
    void* user_data = nullptr;
};

struct NativeWeaponTriggerRegistration {
    std::string owner;
    A2FO_WeaponTriggerHandler handler = nullptr;
    void* user_data = nullptr;
};

struct NativeCraftEventRegistration {
    std::string owner;
    A2FO_CraftEventHandler handler = nullptr;
    void* user_data = nullptr;
};

std::unordered_map<std::string, ClasslabelAliasPolicy> g_classlabel_aliases;
std::unordered_map<std::string, ClasslabelOdfDefaultsPolicy>
    g_classlabel_odf_defaults;
std::unordered_map<std::string, RaceOdfDefaultPolicy> g_race_odf_defaults;
std::vector<OdfOverlayDirectoryPolicy> g_odf_overlay_directories;
std::unordered_map<void*, std::string> g_original_classlabels;
std::unordered_map<std::string, OdfSnapshot> g_odf_snapshots;

struct GameObjectClassOdfLoadContext {
    OdfSnapshot fields;
    bool parameter_db_observed = false;
};

thread_local GameObjectClassOdfLoadContext*
    g_game_object_class_odf_load_context = nullptr;

std::string g_evolver_cocoon_command;
std::string g_evolver_cocoon_owner;
a2fo::RustHandlerRegistry<NativeObjectDestroyedRegistration>
    g_object_destroyed_handlers;
a2fo::RustHandlerRegistry<NativeProducerEventRegistration> g_producer_event_handlers;
a2fo::RustHandlerRegistry<NativeGameObjectClassLoadedRegistration>
    g_game_object_class_loaded_handlers;
a2fo::RustHandlerRegistry<NativeRaceLoadedRegistration> g_race_loaded_handlers;
a2fo::RustHandlerRegistry<NativeWeaponClassLoadedRegistration>
    g_weapon_class_loaded_handlers;
a2fo::RustHandlerRegistry<NativeWeaponTriggerRegistration> g_weapon_trigger_handlers;
a2fo::RustHandlerRegistry<NativeCraftEventRegistration> g_craft_event_handlers;
std::set<std::string> g_game_object_class_odf_fields{"basename"};
std::set<std::string> g_race_odf_fields;
std::set<std::string> g_weapon_class_odf_fields;
bool g_policy_registration_open = true;

struct RegistrationTransaction {
    bool active = false;
    std::string path;
    A2FO_FofsItemLookupHandler fofs_handler = nullptr;
    void* fofs_user_data = nullptr;
    std::string fofs_owner;
    A2FO_InfoIniDefaultsHandler info_ini_defaults_handler = nullptr;
    void* info_ini_defaults_user_data = nullptr;
    std::string info_ini_defaults_owner;
    std::unordered_map<std::string, ClasslabelAliasPolicy> classlabel_aliases;
    std::unordered_map<std::string, ClasslabelOdfDefaultsPolicy>
        classlabel_odf_defaults;
    std::unordered_map<std::string, RaceOdfDefaultPolicy> race_odf_defaults;
    std::vector<OdfOverlayDirectoryPolicy> odf_overlay_directories;
    std::string cocoon_command;
    std::string cocoon_owner;
    a2fo::RustHandlerRegistry<NativeObjectDestroyedRegistration>
        object_destroyed_handlers;
    a2fo::RustHandlerRegistry<NativeProducerEventRegistration> producer_event_handlers;
    a2fo::RustHandlerRegistry<NativeGameObjectClassLoadedRegistration>
        game_object_class_loaded_handlers;
    a2fo::RustHandlerRegistry<NativeRaceLoadedRegistration> race_loaded_handlers;
    a2fo::RustHandlerRegistry<NativeWeaponClassLoadedRegistration>
        weapon_class_loaded_handlers;
    a2fo::RustHandlerRegistry<NativeWeaponTriggerRegistration> weapon_trigger_handlers;
    a2fo::RustHandlerRegistry<NativeCraftEventRegistration> craft_event_handlers;
    std::set<std::string> game_object_class_odf_fields;
    std::set<std::string> race_odf_fields;
    std::set<std::string> weapon_class_odf_fields;
};

RegistrationTransaction g_registration_transaction;

class StateLockGuard {
public:
    StateLockGuard() { EnterCriticalSection(&g_state_lock); }
    ~StateLockGuard() { LeaveCriticalSection(&g_state_lock); }

    StateLockGuard(const StateLockGuard&) = delete;
    StateLockGuard& operator=(const StateLockGuard&) = delete;
};

std::uint8_t core_policy_lower(std::uint8_t value,void*) noexcept {return static_cast<std::uint8_t>(std::tolower(value));}
std::string lower_ascii(std::string value) {
    a2fo_rs_stage8_core_text(0,reinterpret_cast<std::uint8_t*>(value.data()),value.size(),0,&core_policy_lower,nullptr);
    return value;
}
void replace_slashes(std::string& value,char slash='\\') {
    a2fo_rs_stage8_core_text(1,reinterpret_cast<std::uint8_t*>(value.data()),value.size(),static_cast<std::uint8_t>(slash),&core_policy_lower,nullptr);
}
std::string join_path(const std::string& left,const std::string& right) {
    switch(a2fo_rs_stage8_core_join(reinterpret_cast<const std::uint8_t*>(left.data()),left.size(),right.size())){
    case 0:return right;
    case 1:return left;
    case 2:return left+right;
    default:return left+"\\"+right;
    }
}

std::string module_directory(HMODULE module) {
    std::vector<char> buffer(kMaximumPathLength);
    const DWORD length = GetModuleFileNameA(module, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    std::string path(buffer.data(), length);
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? std::string{} : path.substr(0, slash);
}

void open_log() {
    const std::string directory = module_directory(nullptr);
    const std::string path = join_path(directory, "A2FOExtensions.log");
    g_log = CreateFileA(path.c_str(), GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

void log_line(const std::string& text) {
    if (g_log == INVALID_HANDLE_VALUE) {
        return;
    }
    const std::string line = text + "\r\n";
    DWORD written = 0;
    WriteFile(g_log, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    // WriteFile makes new diagnostics visible to readers immediately. Forcing
    // every line through FlushFileBuffers turns large recursive ODF indexes
    // into hundreds or thousands of synchronous disk commits. Windows flushes
    // the handle when the process closes; avoid imposing that durability cost
    // on the game's startup thread.
}

bool validate_module(HMODULE module, std::uint32_t timestamp,
                     std::uint32_t image_size, const char* label) {
    if (!module) {
        log_line(std::string(label) + " is not loaded");
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        log_line(std::string(label) + " has no DOS header");
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const std::uint8_t*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        !((nt->FileHeader.TimeDateStamp == timestamp &&
           nt->OptionalHeader.SizeOfImage == image_size) ||
          a2fo::supported_fleetops::approved_profile(module))) {
        char message[256]{};
        std::snprintf(message, sizeof(message),
                      "%s version mismatch (timestamp=%08lx, image=%08lx)",
                      label,
                      static_cast<unsigned long>(nt->FileHeader.TimeDateStamp),
                      static_cast<unsigned long>(nt->OptionalHeader.SizeOfImage));
        log_line(message);
        return false;
    }
    return true;
}

bool validate_armada_module(HMODULE module) {
    constexpr const char* label = "ArmadaL.exe";
    if (!module) {
        log_line(std::string(label) + " is not loaded");
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (!a2fo::supported_armada::readable_header(module, 0, sizeof(*dos)) ||
        dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        !a2fo::supported_armada::readable_header(
            module, dos->e_lfanew, sizeof(IMAGE_NT_HEADERS32))) {
        log_line(std::string(label) + " has no DOS header");
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const std::uint8_t*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        log_line(std::string(label) + " has an unsupported PE header");
        return false;
    }

    const auto identity = a2fo::supported_armada::identify(module);
    if (identity == a2fo::supported_armada::Identity::canonical) {
        return true;
    }
    if (identity == a2fo::supported_armada::Identity::normalized) {
        log_line(
            "ArmadaL.exe normalized PE header accepted "
            "(supported on-disk read-only fingerprint matched)");
        return true;
    }
    if (identity == a2fo::supported_armada::Identity::janb_20260905) {
        log_line(
            "ArmadaL.exe Jan_B September 2026 build accepted "
            "(supported on-disk code/read-only fingerprints matched)");
        return true;
    }
    if (identity == a2fo::supported_armada::Identity::janb_20260930) {
        log_line(
            "ArmadaL.exe Jan_B September 30 2026 build accepted "
            "(supported on-disk code/read-only/initialized-data fingerprints matched)");
        return true;
    }
    if (identity == a2fo::supported_armada::Identity::roots_profile) {
        log_line(
            "ArmadaL.exe approved Roots compatibility profile accepted "
            "(on-disk headers and all seven complete sections matched; native station rotation)");
        return true;
    }

    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "%s version mismatch (timestamp=%08lx, image=%08lx); "
                  "no built-in or approved A2FOArmadaProfiles.txt fingerprint matched",
                  label,
                  static_cast<unsigned long>(nt->FileHeader.TimeDateStamp),
                  static_cast<unsigned long>(nt->OptionalHeader.SizeOfImage));
    log_line(message);
    return false;
}

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) {
    return reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(module) + rva);
}

template<class Context, class Step>
std::uint64_t config_policy(std::uint32_t kind, Context& context, Step step) {
    struct Call { Context& context; Step& step; std::exception_ptr error; std::uint8_t failed = 0; };
    Call call{context, step, {}, 0};
    const auto bridge = +[](void* opaque, std::uint32_t op, std::uint64_t arg) -> std::uint64_t {
        auto& c = *static_cast<Call*>(opaque);
        try { return c.step(c.context, op, arg); }
        catch (...) { c.error = std::current_exception(); c.failed = 1; return 0; }
    };
    const auto result = a2fo_rs_stage9_config_flow(kind, bridge, &call, &call.failed);
    if (call.error) std::rethrow_exception(call.error);
    return result;
}

void wait_for_deferred_initialization() {
    int context = 0;
    config_policy(0, context, [](int&, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: return static_cast<std::uint32_t>(InterlockedCompareExchange(&g_deferred_initialization_finished, 0, 0));
        case 1: return GetCurrentThreadId();
        case 2: return g_initialize_thread_id;
        case 3: Sleep(1); return 0;
        default: return 0;
        }
    });
}

bool query_info_ini_defaults(const char* normal_directory,std::string* resolved_directory,bool& has_default_game_speed,int& default_game_speed) {
    try {
        struct Context {const char* normal;std::string* resolved;bool& has_result;int& speed_result;
            A2FO_InfoIniDefaultsHandler handler=nullptr;void* user_data=nullptr;std::vector<char> output{};
            std::uint32_t has_speed=0;std::int32_t speed=0;std::exception_ptr error{};
        } context{normal_directory,resolved_directory,has_default_game_speed,default_game_speed};
        const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept ->std::uint64_t {
            auto& c=*static_cast<Context*>(opaque);
            try{switch(op){
            case 0:wait_for_deferred_initialization();return 0;
            case 1:{StateLockGuard lock;c.handler=g_info_ini_defaults_handler;c.user_data=g_info_ini_defaults_user_data;return 0;}
            case 2:return c.handler!=nullptr;
            case 3:return c.resolved!=nullptr;
            case 4:c.output.resize(kMaximumPathLength+1);return 0;
            case 5:return c.handler(c.normal,c.output.empty()?nullptr:c.output.data(),static_cast<std::uint32_t>(c.output.size()),&c.has_speed,&c.speed,c.user_data);
            case 6:return !c.output.empty();
            case 7:c.output.back()='\0';c.resolved->assign(c.output.data());return 0;
            case 8:return c.has_speed;
            case 9:return static_cast<std::uint32_t>(c.speed);
            case 10:c.has_result=a!=0;return 0;
            case 11:c.speed_result=a?c.speed:0;return 0;
            default:return 0;
            }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
        };
        const auto result=a2fo_rs_stage8_core_query_defaults(step,&context);
        if(context.error)std::rethrow_exception(context.error);
        return result==1;
    }catch(...){log_line("Fleet Ops info.ini defaults provider failed");return false;}
}

bool A2FO_CALL api_get_settings_directory(char* output, std::uint32_t output_size) {
    struct Context { char* output; std::uint32_t size; const char* directory = nullptr; } c{output, output_size};
    return config_policy(1, c, [](Context& x, std::uint32_t op, std::uint64_t arg) -> std::uint64_t {
        switch (op) {
        case 0: return x.output != nullptr;
        case 1: return x.size;
        case 2: x.output[0] = '\0'; return 0;
        case 3: return a2fo::builtin_module_enabled(a2fo::BuiltinFeature::info_ini);
        case 4: { StateLockGuard lock; x.directory = g_delphi_settings_directory; return 0; }
        case 5: return x.directory != nullptr;
        case 6: return std::strlen(x.directory);
        case 7: std::memcpy(x.output, x.directory, static_cast<std::size_t>(arg)); return 0;
        default: return 0;
        }
    }) != 0;
}

void ensure_fleet_ops_info_defaults() {
    struct Context {bool has_speed=false;int speed=0;std::exception_ptr error{};} context;
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:wait_for_deferred_initialization();return 0;
        case 1:return a2fo::builtin_module_enabled(a2fo::BuiltinFeature::info_ini);
        case 2:return static_cast<std::uint32_t>(InterlockedCompareExchange(&g_mod_defaults_state,1,0));
        case 3:return query_info_ini_defaults(nullptr,nullptr,c.has_speed,c.speed);
        case 4:g_has_default_game_speed=c.has_speed;g_default_game_speed=c.speed;return 0;
        case 5:InterlockedExchange(&g_mod_defaults_state,a?2:0);return 0;
        case 6:return static_cast<std::uint32_t>(InterlockedCompareExchange(&g_mod_defaults_state,0,0));
        case 7:Sleep(0);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_core_ensure_defaults(step,&context);
    if(context.error)std::rethrow_exception(context.error);
}

const char* make_static_delphi_string(const std::string& value) {
    struct Context { const std::string& value; std::uint8_t* allocation = nullptr; } c{value};
    return reinterpret_cast<const char*>(static_cast<std::uintptr_t>(config_policy(2, c, [](Context& x, std::uint32_t op, std::uint64_t arg) -> std::uint64_t {
        switch (op) {
        case 0: return x.value.empty();
        case 1: return x.value.size();
        case 2: return kMaximumPathLength;
        case 3: x.allocation = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, static_cast<std::size_t>(arg), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)); return x.allocation != nullptr;
        case 4: {
            // Immortal Delphi string, deliberately allocated on the native heap.
            const std::int32_t reference_count = -1;
            const auto length = static_cast<std::int32_t>(x.value.size());
            std::memcpy(x.allocation, &reference_count, sizeof(reference_count));
            std::memcpy(x.allocation + 4, &length, sizeof(length)); return 0;
        }
        case 5: std::memcpy(x.allocation + 8, x.value.c_str(), x.value.size() + 1); return 0;
        case 6: return reinterpret_cast<std::uintptr_t>(x.allocation + 8);
        default: return 0;
        }
    })));
}

const char* resolved_settings_directory(const char* normal_directory) {
    struct Context {const char* normal;std::optional<std::string> resolved{};bool has_speed=false;int speed=0;
        const char* replacement=nullptr;std::exception_ptr error{};
    } context{normal_directory};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:ensure_fleet_ops_info_defaults();return 0;
        case 1:return a2fo::builtin_module_enabled(a2fo::BuiltinFeature::info_ini);
        case 2:{StateLockGuard lock;c.replacement=g_delphi_settings_directory;return c.replacement!=nullptr;}
        case 3:c.resolved.emplace();return query_info_ini_defaults(c.normal,&*c.resolved,c.has_speed,c.speed);
        case 4:return c.resolved->empty();
        case 5:c.replacement=make_static_delphi_string(*c.resolved);return 0;
        case 6:{StateLockGuard lock;if(!g_delphi_settings_directory)g_delphi_settings_directory=c.replacement;c.replacement=g_delphi_settings_directory;return 0;}
        case 7:return c.replacement!=nullptr;
        case 8:log_line("Fleet Ops settings directory: "+*c.resolved);return 0;
        case 9:log_line("Fleet Ops SettingsDirectory allocation failed");return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_core_resolve_directory(step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return context.replacement;
}

void __attribute__((regparm(2))) mod_user_directory_hook(void* modification_support, void* result_string) {
    a2fo_call_delphi_two_registers(g_mod_user_directory_hook.gateway, modification_support, result_string);
    try {
        struct Context { void* result; const char* replacement = nullptr; } c{result_string};
        config_policy(3, c, [](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
            switch (op) {
            case 0: return x.result != nullptr;
            case 1: x.replacement = resolved_settings_directory(*static_cast<const char* const*>(x.result)); return 0;
            case 2: return x.replacement != nullptr;
            case 3: a2fo_call_delphi_two_registers(at(g_fleet_ops, kDelphiLStrAsgRva), x.result, const_cast<char*>(x.replacement)); return 0;
            default: return 0;
            }
        });
    } catch (...) { log_line("Fleet Ops SettingsDirectory override was skipped"); }
}

void* __attribute__((regparm(1))) fo_settings_get_instance_hook(void* settings_class) {
    void* settings = a2fo_call_delphi_one_register(g_fo_settings_get_instance_hook.gateway, settings_class);
    config_policy(4, settings, [](void*& instance, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        if (op == 0) return instance != nullptr;
        InterlockedExchangePointer(&g_fo_settings_instance, instance); return 0;
    });
    try {
        config_policy(5, settings, [](void*& instance, std::uint32_t op, std::uint64_t arg) -> std::uint64_t {
            switch (op) {
            case 0: ensure_fleet_ops_info_defaults(); return 0;
            case 1: return a2fo::builtin_module_enabled(a2fo::BuiltinFeature::info_ini);
            case 2: return g_has_default_game_speed;
            case 3: return instance != nullptr;
            case 4: return static_cast<std::uint8_t*>(instance)[0x70] != 0;
            case 5: InterlockedExchange(&g_fo_settings_first_run, static_cast<LONG>(arg)); return 0;
            case 6: return static_cast<std::uint32_t>(InterlockedCompareExchange(&g_fo_settings_default_applied, 1, 0));
            case 7: *reinterpret_cast<int*>(static_cast<std::uint8_t*>(instance) + 0x08) = g_default_game_speed; return 0;
            case 8: a2fo_call_delphi_one_register(at(g_fleet_ops, kFoSettingsSaveRva), instance); return 0;
            case 9: log_line("Fleet Ops first-run Settings.xml game speed saved: " + std::to_string(g_default_game_speed)); return 0;
            default: return 0;
            }
        });
    } catch (...) { log_line("Fleet Ops first-run Settings.xml default was skipped"); }
    return settings;
}

struct ShellDisplaySettings {
    int display = 0;
    int width = 0;
    int height = 0;
    bool windowed = false;
};

struct ShellDisplayRuntime {
    HWND window = nullptr;
    DWORD windowed_style = 0;
    DWORD windowed_extended_style = 0;
    bool window_styles_captured = false;
    bool settings_logged = false;
    bool mode_applied = false;
    bool apply_failure_logged = false;
    int window_tree_logs = 0;
    ShellDisplaySettings settings;
};

struct ArmadaWindowSearch {
    DWORD process_id = 0;
    HWND window = nullptr;
    std::uint64_t score = 0;
};

struct ArmadaMonitorSearch {
    int target_index = 0;
    int current_index = 0;
    HMONITOR monitor = nullptr;
};

// Synchronous policy bridge: native callbacks retain every C++ exception until
// Rust has returned. Full-width scores/coordinates use a separate failure byte.
template<class Context, class Step>
std::uint64_t shell_policy(std::uint32_t kind, Context& context, Step step) {
    struct Call { Context& context; Step& step; std::exception_ptr error; std::uint8_t failed = 0; };
    Call call{context, step, {}, 0};
    const auto bridge = +[](void* opaque, std::uint32_t op, std::uint64_t arg) -> std::uint64_t {
        auto& c = *static_cast<Call*>(opaque);
        try { return c.step(c.context, op, arg); }
        catch (...) { c.error = std::current_exception(); c.failed = 1; return 0; }
    };
    const auto result = a2fo_rs_stage9_shell_flow(kind, bridge, &call, &call.failed);
    if (call.error) std::rethrow_exception(call.error);
    return result;
}

bool is_armada_main_window(HWND window, DWORD process_id) {
    struct Context { HWND window; DWORD expected; DWORD actual = 0; char name[64]{}; } c{window, process_id};
    return shell_policy(0, c, [](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: return x.window != nullptr;
        case 1: return IsWindowVisible(x.window);
        case 2: GetWindowThreadProcessId(x.window, &x.actual); return x.actual;
        case 3: return x.expected;
        case 4: return static_cast<std::uint64_t>(static_cast<std::int64_t>(GetClassNameA(x.window, x.name, sizeof(x.name))));
        case 5: return static_cast<std::uint64_t>(static_cast<std::int64_t>(_stricmp(x.name, "TMainWindowForm")));
        default: return 0;
        }
    }) != 0;
}

bool same_shell_display_settings(const ShellDisplaySettings& left,
                                 const ShellDisplaySettings& right) {
    const std::int32_t a[]{left.display, left.width, left.height, left.windowed};
    const std::int32_t b[]{right.display, right.width, right.height, right.windowed};
    return a2fo_rs_stage9_shell_rect_equal(a, b) != 0;
}

bool read_shell_display_settings(ShellDisplaySettings& settings) {
    // Interlocked snapshot and fixed foreign offsets are the native layout boundary.
    void* instance = InterlockedCompareExchangePointer(&g_fo_settings_instance, nullptr, nullptr);
    if (!instance) return false;
    const auto* bytes = static_cast<const std::uint8_t*>(instance);
    settings.display = *reinterpret_cast<const int*>(bytes + 0x30);
    settings.width = *reinterpret_cast<const int*>(bytes + 0x34);
    settings.height = *reinterpret_cast<const int*>(bytes + 0x38);
    settings.windowed = bytes[0x72] != 0;
    return a2fo_rs_stage9_shell_settings_valid(settings.display, settings.width, settings.height) != 0;
}

BOOL CALLBACK find_armada_window(HWND window, LPARAM parameter) {
    struct Context { HWND window; ArmadaWindowSearch* search; RECT rect{}; } c{window, reinterpret_cast<ArmadaWindowSearch*>(parameter)};
    return static_cast<BOOL>(shell_policy(1, c, [](Context& x, std::uint32_t op, std::uint64_t arg) -> std::uint64_t {
        switch (op) {
        case 0: return x.search != nullptr;
        case 1: return is_armada_main_window(x.window, x.search->process_id);
        case 2: return GetWindowRect(x.window, &x.rect);
        case 3: return static_cast<std::uint32_t>(x.rect.left);
        case 4: return static_cast<std::uint32_t>(x.rect.right);
        case 5: return static_cast<std::uint32_t>(x.rect.top);
        case 6: return static_cast<std::uint32_t>(x.rect.bottom);
        case 7: return reinterpret_cast<std::uintptr_t>(GetWindow(x.window, GW_OWNER));
        case 8: return x.window == GetForegroundWindow();
        case 9: return x.search->window != nullptr;
        case 10: return x.search->score;
        case 11: x.search->window = x.window; x.search->score = arg; return 0;
        default: return 0;
        }
    }));
}

HWND get_armada_window() {
    struct Context { DWORD process_id = 0; HWND foreground = nullptr; ArmadaWindowSearch search; } c;
    return reinterpret_cast<HWND>(static_cast<std::uintptr_t>(shell_policy(2, c, [](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: x.process_id = GetCurrentProcessId(); return 0;
        case 1: x.foreground = GetForegroundWindow(); return 0;
        case 2: return is_armada_main_window(x.foreground, x.process_id);
        case 3: return reinterpret_cast<std::uintptr_t>(x.foreground);
        case 4: x.search.process_id = x.process_id; EnumWindows(find_armada_window, reinterpret_cast<LPARAM>(&x.search)); return 0;
        case 5: return reinterpret_cast<std::uintptr_t>(x.search.window);
        default: return 0;
        }
    })));
}

BOOL CALLBACK find_armada_monitor(HMONITOR monitor, HDC, LPRECT, LPARAM parameter) {
    struct Context { HMONITOR monitor; ArmadaMonitorSearch* search; } c{monitor, reinterpret_cast<ArmadaMonitorSearch*>(parameter)};
    return static_cast<BOOL>(shell_policy(3, c, [](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: return x.search != nullptr;
        case 1: return static_cast<std::uint32_t>(x.search->current_index++);
        case 2: return static_cast<std::uint32_t>(x.search->target_index);
        case 3: x.search->monitor = x.monitor; return 0;
        default: return 0;
        }
    }));
}

HMONITOR get_configured_monitor(HWND window, int display) {
    struct Context { HWND window; ArmadaMonitorSearch search; } c{window, {display, 0, nullptr}};
    return reinterpret_cast<HMONITOR>(static_cast<std::uintptr_t>(shell_policy(4, c, [](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: EnumDisplayMonitors(nullptr, nullptr, find_armada_monitor, reinterpret_cast<LPARAM>(&x.search)); return 0;
        case 1: return reinterpret_cast<std::uintptr_t>(x.search.monitor);
        case 2: return reinterpret_cast<std::uintptr_t>(MonitorFromWindow(x.window, MONITOR_DEFAULTTONEAREST));
        default: return 0;
        }
    })));
}

void log_armada_window(const char* kind, HWND window) {
    char title[256]{};
    char class_name[128]{};
    GetWindowTextA(window, title, sizeof(title));
    GetClassNameA(window, class_name, sizeof(class_name));
    RECT rect{};
    RECT client{};
    GetWindowRect(window, &rect);
    GetClientRect(window, &client);
    log_line(std::string("Shell display: ") + kind + " window hwnd=" +
             std::to_string(reinterpret_cast<std::uintptr_t>(window)) +
             ", title=\"" + title + "\", class=\"" + class_name +
             "\", outer=" +
             std::to_string(rect.right - rect.left) + "x" +
             std::to_string(rect.bottom - rect.top) + "@" +
             std::to_string(rect.left) + "," + std::to_string(rect.top) +
             ", client=" +
             std::to_string(client.right - client.left) + "x" +
             std::to_string(client.bottom - client.top) +
             ", parent=" +
             std::to_string(reinterpret_cast<std::uintptr_t>(
                 GetParent(window))) +
             ", owner=" +
             std::to_string(reinterpret_cast<std::uintptr_t>(
                 GetWindow(window, GW_OWNER))) +
             ", style=" +
             std::to_string(static_cast<DWORD>(
                 GetWindowLongA(window, GWL_STYLE))));
}

BOOL CALLBACK log_armada_child_window(HWND window, LPARAM) {
    return static_cast<BOOL>(shell_policy(13, window, [](HWND& w, std::uint32_t, std::uint64_t) -> std::uint64_t {
        log_armada_window("child", w); return 0;
    }));
}

BOOL CALLBACK log_armada_top_window(HWND window, LPARAM parameter) {
    struct Context { HWND window; DWORD expected; DWORD actual = 0; } c{window, static_cast<DWORD>(parameter)};
    return static_cast<BOOL>(shell_policy(11, c, [](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: GetWindowThreadProcessId(x.window, &x.actual); return x.actual;
        case 1: return x.expected;
        case 2: log_armada_window("top-level", x.window); return 0;
        case 3: EnumChildWindows(x.window, log_armada_child_window, 0); return 0;
        default: return 0;
        }
    }));
}

void log_armada_window_tree() {
    int context = 0;
    shell_policy(12, context, [](int&, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        if (op == 0) log_line("Shell display: current Armada window tree follows");
        else EnumWindows(log_armada_top_window, static_cast<LPARAM>(GetCurrentProcessId()));
        return 0;
    });
}

bool set_window_style(HWND window, int index, DWORD style) {
    struct Context { HWND window; int index; DWORD style; } c{window, index, style};
    return shell_policy(10, c, [](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: SetLastError(ERROR_SUCCESS); return 0;
        case 1: return static_cast<std::uint32_t>(SetWindowLongA(x.window, x.index, static_cast<LONG>(x.style)));
        case 2: return GetLastError();
        default: return 0;
        }
    }) != 0;
}

bool same_rect(const RECT& left, const RECT& right) {
    const std::int32_t a[]{left.left, left.top, left.right, left.bottom};
    const std::int32_t b[]{right.left, right.top, right.right, right.bottom};
    return a2fo_rs_stage9_shell_rect_equal(a, b) != 0;
}

void capture_window_styles(ShellDisplayRuntime& runtime, HWND window) {
    struct Context { ShellDisplayRuntime& runtime; HWND window; } c{runtime, window};
    shell_policy(5, c, [](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: x.runtime.window = x.window; break;
        case 1: x.runtime.windowed_style = static_cast<DWORD>(GetWindowLongA(x.window, GWL_STYLE)); break;
        case 2: x.runtime.windowed_extended_style = static_cast<DWORD>(GetWindowLongA(x.window, GWL_EXSTYLE)); break;
        case 3: x.runtime.window_styles_captured = true; x.runtime.mode_applied = false; x.runtime.apply_failure_logged = false; break;
        case 4: log_line("Shell display: Armada window found through Win32"); break;
        }
        return 0;
    });
}

std::uint64_t shell_window_styles(bool windowed, DWORD style, DWORD ex) {
    const std::uint32_t masks[]{WS_OVERLAPPEDWINDOW, WS_CHILD, WS_POPUP, WS_VISIBLE, WS_CAPTION, 0, 0,
        WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE};
    return a2fo_rs_stage9_shell_styles(windowed, style, ex, masks);
}

// The context owns native RECTs and cached style/size locals. Policy samples
// callback-mutable runtime/monitor state at the same points as the original.
bool apply_shell_mode(ShellDisplayRuntime& runtime, HWND window,
                      const MONITORINFO& monitor, const ShellDisplaySettings* settings) {
    struct Context {
        ShellDisplayRuntime& runtime; HWND window; const MONITORINFO& monitor;
        const ShellDisplaySettings* settings; bool windowed;
        DWORD style = 0, ex = 0; int client_width = 0, client_height = 0;
        RECT outer{}, current{}, target{}; int width = 0, height = 0;
    } c{runtime, window, monitor, settings, settings != nullptr};
    return shell_policy(c.windowed ? 7 : 6, c, [](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: {
            if (x.windowed) {
                const auto client = a2fo_rs_stage9_shell_client(x.settings->width, x.settings->height);
                x.client_width = static_cast<int>(static_cast<std::uint32_t>(client));
                x.client_height = static_cast<int>(static_cast<std::uint32_t>(client >> 32));
            }
            const auto packed = shell_window_styles(x.windowed, x.runtime.windowed_style, x.runtime.windowed_extended_style);
            x.style = static_cast<DWORD>(packed); x.ex = static_cast<DWORD>(packed >> 32);
            return 0;
        }
        case 1: return static_cast<DWORD>(GetWindowLongA(x.window, GWL_STYLE));
        case 2: return x.style;
        case 3: return set_window_style(x.window, GWL_STYLE, x.style);
        case 4: return static_cast<DWORD>(GetWindowLongA(x.window, GWL_EXSTYLE));
        case 5: return x.ex;
        case 6: return set_window_style(x.window, GWL_EXSTYLE, x.ex);
        case 7: x.outer = RECT{0, 0, x.client_width, x.client_height}; return AdjustWindowRectEx(&x.outer, x.style, FALSE, x.ex);
        case 8: {
            const std::int32_t outer[]{x.outer.left, x.outer.top, x.outer.right, x.outer.bottom};
            const std::int32_t work[]{x.monitor.rcWork.left, x.monitor.rcWork.top, x.monitor.rcWork.right, x.monitor.rcWork.bottom};
            std::int32_t target[6]{}; a2fo_rs_stage9_shell_center(outer, work, target);
            x.target = RECT{target[0], target[1], target[2], target[3]}; x.width = target[4]; x.height = target[5]; return 0;
        }
        case 9: return GetWindowRect(x.window, &x.current);
        case 10: return same_rect(x.current, x.windowed ? x.target : x.monitor.rcMonitor);
        case 11:
            if (x.windowed) return SetWindowPos(x.window, nullptr, x.target.left, x.target.top, x.width, x.height,
                SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            return SetWindowPos(x.window, HWND_TOP, x.monitor.rcMonitor.left, x.monitor.rcMonitor.top,
                x.monitor.rcMonitor.right - x.monitor.rcMonitor.left, x.monitor.rcMonitor.bottom - x.monitor.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        case 12: return x.runtime.mode_applied;
        case 13:
            if (x.windowed) log_line("Shell display: configured windowed size applied: " + std::to_string(x.client_width) + "x" + std::to_string(x.client_height));
            else log_line("Shell display: borderless fullscreen applied: " + std::to_string(x.monitor.rcMonitor.right - x.monitor.rcMonitor.left) + "x" + std::to_string(x.monitor.rcMonitor.bottom - x.monitor.rcMonitor.top));
            return 0;
        default: return 0;
        }
    }) != 0;
}

bool apply_shell_fullscreen(ShellDisplayRuntime& runtime, HWND window, const MONITORINFO& monitor) {
    return apply_shell_mode(runtime, window, monitor, nullptr);
}
bool apply_shell_windowed_size(ShellDisplayRuntime& runtime, HWND window, const MONITORINFO& monitor, const ShellDisplaySettings& settings) {
    return apply_shell_mode(runtime, window, monitor, &settings);
}

void poll_shell_display(ShellDisplayRuntime& runtime) {
    struct Context { ShellDisplayRuntime& runtime; ShellDisplaySettings settings; HWND window = nullptr; HMONITOR handle = nullptr; MONITORINFO monitor{}; } c{runtime, {}};
    shell_policy(8, c, [](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: return read_shell_display_settings(x.settings);
        case 1: return x.runtime.settings_logged;
        case 2: return same_shell_display_settings(x.runtime.settings, x.settings);
        case 3: x.runtime.settings = x.settings; x.runtime.settings_logged = true; x.runtime.mode_applied = false; x.runtime.apply_failure_logged = false; return 0;
        case 4: log_line("Shell display: graphics settings detected: display=" + std::to_string(x.settings.display) + ", resolution=" + std::to_string(x.settings.width) + "x" + std::to_string(x.settings.height) + ", mode=" + (x.settings.windowed ? "windowed" : "fullscreen")); return 0;
        case 5: x.window = get_armada_window(); return reinterpret_cast<std::uintptr_t>(x.window);
        case 6: return IsWindow(x.window);
        case 7: return x.runtime.window == x.window;
        case 8: return x.runtime.window_styles_captured;
        case 9: capture_window_styles(x.runtime, x.window); return 0;
        case 10: return static_cast<std::uint32_t>(x.runtime.window_tree_logs++);
        case 11: log_armada_window_tree(); return 0;
        case 12: x.handle = get_configured_monitor(x.window, x.settings.display); x.monitor.cbSize = sizeof(x.monitor); return reinterpret_cast<std::uintptr_t>(x.handle);
        case 13: return GetMonitorInfoA(x.handle, &x.monitor);
        case 14: return x.settings.windowed;
        case 15: return apply_shell_windowed_size(x.runtime, x.window, x.monitor, x.settings);
        case 16: return apply_shell_fullscreen(x.runtime, x.window, x.monitor);
        case 17: x.runtime.mode_applied = true; x.runtime.apply_failure_logged = false; return 0;
        case 18: return x.runtime.apply_failure_logged;
        case 19: x.runtime.apply_failure_logged = true; return 0;
        case 20: log_line("Shell display: window mode application failed (error " + std::to_string(GetLastError()) + ")"); return 0;
        default: return 0;
        }
    });
}

void run_shell_display_monitor() {
    struct Context { ShellDisplayRuntime runtime; std::exception_ptr policy_error; } c;
    shell_policy(9, c, [](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
        switch (op) {
        case 0: log_line("Shell display: graphics-settings monitor started"); return 0;
        case 1: try { poll_shell_display(x.runtime); return 1; } catch (...) { x.policy_error = std::current_exception(); return 0; }
        case 2: {
            auto error = std::move(x.policy_error);
            try { std::rethrow_exception(error); }
            catch (...) { log_line("Shell display: update skipped after an unexpected C++ exception"); }
            return 0;
        }
        case 3: Sleep(100); return 0;
        default: return 0;
        }
    });
}

void __attribute__((fastcall)) game_configuration_new_hook(void* game_configuration, void*) {
    a2fo_call_game_object_method(g_game_configuration_new_hook.gateway, game_configuration);
    try {
        config_policy(6, game_configuration, [](void*& instance, std::uint32_t op, std::uint64_t) -> std::uint64_t {
            switch (op) {
            case 0: ensure_fleet_ops_info_defaults(); return 0;
            case 1: return a2fo::builtin_module_enabled(a2fo::BuiltinFeature::info_ini);
            case 2: return g_has_default_game_speed;
            case 3: return instance != nullptr;
            case 4: *reinterpret_cast<int*>(static_cast<std::uint8_t*>(instance) + 4) = g_default_game_speed; return 0;
            case 5: return static_cast<std::uint32_t>(InterlockedCompareExchange(&g_default_game_speed_logged, 1, 0));
            case 6: log_line("Fleet Ops first-run game speed default applied: " + std::to_string(g_default_game_speed)); return 0;
            default: return 0;
            }
        });
    } catch (...) { log_line("Fleet Ops DefaultGameSpeed override was skipped"); }
}

bool __attribute__((fastcall)) game_configuration_load_profile_hook(void* game_configuration, void*) {
    const auto original_result = reinterpret_cast<std::uintptr_t>(a2fo_call_game_object_method(g_game_configuration_load_profile_hook.gateway, game_configuration));
    try {
        struct Context { void* instance; int previous = 0; } c{game_configuration};
        config_policy(7, c, [](Context& x, std::uint32_t op, std::uint64_t) -> std::uint64_t {
            switch (op) {
            case 0: ensure_fleet_ops_info_defaults(); return 0;
            case 1: return a2fo::builtin_module_enabled(a2fo::BuiltinFeature::info_ini);
            case 2: return g_has_default_game_speed;
            case 3: return x.instance != nullptr;
            case 4: return static_cast<std::uint32_t>(InterlockedCompareExchange(&g_fo_settings_first_run, 0, 0));
            case 5: { auto* speed = reinterpret_cast<int*>(static_cast<std::uint8_t*>(x.instance) + 4); x.previous = *speed; *speed = g_default_game_speed; return 0; }
            case 6: return static_cast<std::uint32_t>(InterlockedCompareExchange(&g_runtime_game_speed_logged, 1, 0));
            case 7: log_line("Fleet Ops first-run runtime game speed applied: " + std::to_string(g_default_game_speed) + " (was " + std::to_string(x.previous) + ")"); return 0;
            default: return 0;
            }
        });
    } catch (...) { log_line("Fleet Ops runtime DefaultGameSpeed override was skipped"); }
    return (original_result & 0xffu) != 0;
}

int default_user_profile_game_speed_hook() {
    try {
        int context = 0;
        return static_cast<int>(static_cast<std::uint32_t>(config_policy(8, context, [](int&, std::uint32_t op, std::uint64_t) -> std::uint64_t {
            switch (op) {
            case 0: ensure_fleet_ops_info_defaults(); return 0;
            case 1: return a2fo::builtin_module_enabled(a2fo::BuiltinFeature::info_ini);
            case 2: return g_has_default_game_speed;
            case 3: return static_cast<std::uint32_t>(InterlockedCompareExchange(&g_user_profile_game_speed_logged, 1, 0));
            case 4: log_line("Armada default user-profile game speed supplied: " + std::to_string(g_default_game_speed)); return 0;
            case 5: return static_cast<std::uint32_t>(g_default_game_speed);
            default: return 0;
            }
        })));
    } catch (...) { log_line("Armada default user-profile game-speed override was skipped"); }
    return 5;
}

std::uint8_t core_policy_alnum(std::uint8_t value,void*) noexcept {return std::isalnum(value)!=0;}
bool valid_policy_identifier(const char* value) {return a2fo_rs_stage8_core_validate(0,reinterpret_cast<const std::uint8_t*>(value),0,&core_policy_alnum,nullptr)!=0;}

bool valid_odf_field_name(const char* value) {return a2fo_rs_stage8_core_validate(1,reinterpret_cast<const std::uint8_t*>(value),0,&core_policy_alnum,nullptr)!=0;}

bool valid_odf_default_value(const char* value) {return a2fo_rs_stage8_core_validate(2,reinterpret_cast<const std::uint8_t*>(value),0,&core_policy_alnum,nullptr)!=0;}

bool normalize_relative_directory(const char* value,std::string& output) {
    struct Context {const char* value;std::string& output;std::string component{};std::exception_ptr error{};} context{value,output};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:c.output.clear();return 0;
        case 1:return c.value&&*c.value;
        case 2:return static_cast<unsigned char>(c.value[a]);
        case 3:return c.output.size();
        case 4:return c.component.size();
        case 5:return static_cast<unsigned char>(c.component[a]);
        case 7:c.output.push_back('\\');return 0;
        case 8:c.output+=c.component;return 0;
        case 9:c.component.clear();return 0;
        case 10:c.component.push_back(static_cast<char>(a));return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_core_relative(step,&context)!=0;
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

bool valid_replacement_odf_name(const char* value,std::size_t length) {return a2fo_rs_stage8_core_validate(3,reinterpret_cast<const std::uint8_t*>(value),length,&core_policy_alnum,nullptr)!=0;}

std::string policy_owner(const char* value) {return reinterpret_cast<const char*>(a2fo_rs_stage8_core_owner(reinterpret_cast<const std::uint8_t*>(value)));}

bool register_classlabel_alias_policy(const char* owner_value,const char* source_value,const char* target_value) {
    struct Context {
        const char* source;const char* target;std::string owner;
        std::string key{},existing_owner{};ClasslabelAliasPolicy policy{};
        decltype(g_classlabel_aliases)::iterator existing{};
        std::exception_ptr error{};std::optional<StateLockGuard> guard{};
    } context{source_value,target_value,policy_owner(owner_value)};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return valid_policy_identifier(c.source);
        case 1:return valid_policy_identifier(c.target);
        case 2:log_line("Classlabel alias policy rejected for "+c.owner+"; source and target must be 1-63 character identifiers");return 0;
        case 3:c.key=lower_ascii(c.source);c.policy={lower_ascii(c.target),c.owner};return 0;
        case 4:c.guard.emplace();return 0;
        case 5:return g_policy_registration_open;
        case 6:return g_registration_transaction.active;
        case 7:c.existing=g_classlabel_aliases.find(c.key);return c.existing!=g_classlabel_aliases.end();
        case 8:g_classlabel_aliases.emplace(c.key,std::move(c.policy));return 0;
        case 9:c.guard.reset();return 0;
        case 10:log_line("Classlabel alias policy registered by "+c.owner+": "+c.key+" -> "+lower_ascii(c.target));return 0;
        case 11:log_line("Classlabel alias policy rejected for "+c.owner+"; startup registration is closed");return 0;
        case 12:log_line("Classlabel alias policy rejected for "+c.owner+"; "+c.key+" is already owned by "+c.existing_owner);return 0;
        case 13:c.existing_owner=c.existing->second.owner;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_registration(0,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result==1;
}

bool register_classlabel_odf_defaults_policy(
    const char* owner_value,const char* classlabel_value,
    const A2FO_ClasslabelOdfDefault* defaults,std::uint32_t default_count) {
    const std::string owner=policy_owner(owner_value);
    struct Context {
        const std::string& owner;const char* label;const A2FO_ClasslabelOdfDefault* defaults;
        ClasslabelOdfDefaultsPolicy policy{};std::optional<std::string> command{};std::string key{},existing_owner{};
        decltype(g_classlabel_odf_defaults)::iterator existing{};
        std::exception_ptr error{};std::optional<StateLockGuard> guard{};
    } context{owner,classlabel_value,defaults};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return valid_policy_identifier(c.label);
        case 1:return c.defaults!=nullptr;
        case 2:log_line("Classlabel ODF defaults rejected for "+c.owner+"; invalid classlabel or default list");return 0;
        case 3:c.policy.owner=c.owner;return 0;
        case 4:return valid_odf_field_name(c.defaults[a].command);
        case 5:return valid_odf_default_value(c.defaults[a].value);
        case 6:log_line("Classlabel ODF defaults rejected for "+c.owner+"; commands and values must be bounded text");return 0;
        case 7:c.command.emplace(lower_ascii(c.defaults[a].command));return c.policy.defaults.emplace(*c.command,c.defaults[a].value).second;
        case 8:log_line("Classlabel ODF defaults rejected for "+c.owner+"; duplicate command "+*c.command);return 0;
        case 9:c.command.reset();c.error=nullptr;log_line("Classlabel ODF defaults rejected for "+c.owner+"; could not copy registration data");return 0;
        case 10:c.key=lower_ascii(c.label);return 0;
        case 11:c.guard.emplace();return 0;
        case 12:return g_policy_registration_open;
        case 13:return g_registration_transaction.active;
        case 14:c.existing=g_classlabel_odf_defaults.find(c.key);return c.existing!=g_classlabel_odf_defaults.end();
        case 15:g_classlabel_odf_defaults.emplace(c.key,std::move(c.policy));return 0;
        case 16:c.existing_owner=c.existing->second.owner;return 0;
        case 17:c.guard.reset();return 0;
        case 18:log_line("Classlabel ODF defaults registered by "+c.owner+": "+c.key+" ("+std::to_string(a)+" commands)");return 0;
        case 19:log_line("Classlabel ODF defaults rejected for "+c.owner+"; startup registration is closed");return 0;
        case 20:log_line("Classlabel ODF defaults rejected for "+c.owner+"; "+c.key+" is already owned by "+c.existing_owner);return 0;
        case 32:c.command.reset();return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_defaults(0,default_count,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result==1;
}

bool register_race_odf_defaults_policy(const char* owner_value,
    const A2FO_RaceOdfDefault* defaults,std::uint32_t default_count) {
    const std::string owner=policy_owner(owner_value);
    struct Context {
        const std::string& owner;const A2FO_RaceOdfDefault* defaults;
        std::unordered_map<std::string,std::string> pending{};
        std::optional<std::string> command{};std::string conflict{},existing_owner{};
        decltype(pending)::const_iterator item{};
        std::optional<decltype(g_race_odf_defaults)> updated_defaults{};std::optional<decltype(g_race_odf_fields)> updated_fields{};
        std::exception_ptr error{};std::optional<StateLockGuard> guard{};
    } context{owner,defaults};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 1:return c.defaults!=nullptr;
        case 2:log_line("Race ODF defaults rejected for "+c.owner+"; invalid default list");return 0;
        case 4:return valid_odf_field_name(c.defaults[a].command);
        case 5:return valid_odf_default_value(c.defaults[a].value);
        case 6:log_line("Race ODF defaults rejected for "+c.owner+"; commands and values must be bounded text");return 0;
        case 7:c.command.emplace(lower_ascii(c.defaults[a].command));return c.pending.emplace(*c.command,c.defaults[a].value).second;
        case 8:log_line("Race ODF defaults rejected for "+c.owner+"; duplicate command "+*c.command);return 0;
        case 9:c.command.reset();c.error=nullptr;log_line("Race ODF defaults rejected for "+c.owner+"; could not copy registration data");return 0;
        case 11:c.guard.emplace();return 0;
        case 12:return g_policy_registration_open;
        case 13:return g_registration_transaction.active;
        case 17:c.guard.reset();return 0;
        case 18:log_line("Race ODF defaults registered by "+c.owner+" ("+std::to_string(a)+" commands)");return 0;
        case 19:log_line("Race ODF defaults rejected for "+c.owner+"; startup registration is closed");return 0;
        case 20:log_line("Race ODF defaults rejected for "+c.owner+"; "+c.conflict+" is already owned by "+c.existing_owner);return 0;
        case 21:c.item=c.pending.begin();return 0;
        case 22:return c.item!=c.pending.end();
        case 23:{const auto existing=g_race_odf_defaults.find(c.item->first);if(existing==g_race_odf_defaults.end())return 0;
            c.conflict=c.item->first;c.existing_owner=existing->second.owner;return 1;}
        case 24:++c.item;return 0;
        case 25:c.updated_defaults.emplace(g_race_odf_defaults);return 0;
        case 26:c.updated_fields.emplace(g_race_odf_fields);return 0;
        case 27:c.updated_defaults->emplace(c.item->first,RaceOdfDefaultPolicy{c.item->second,c.owner});return 0;
        case 28:c.updated_fields->insert(c.item->first);return 0;
        case 29:g_race_odf_defaults.swap(*c.updated_defaults);g_race_odf_fields.swap(*c.updated_fields);return 0;
        case 30:c.updated_fields.reset();c.updated_defaults.reset();c.guard.reset();return 0;
        case 31:c.error=nullptr;log_line("Race ODF defaults rejected for "+c.owner+"; could not commit registration data");return 0;
        case 32:c.command.reset();return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_defaults(1,default_count,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result==1;
}

bool register_evolver_cocoon_policy(const char* owner_value,const char* command_value) {
    struct Context {const char* input;std::string owner;std::string command{},existing_owner{};
        std::exception_ptr error{};std::optional<StateLockGuard> guard{};
    } context{command_value,policy_owner(owner_value)};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return valid_policy_identifier(c.input);
        case 2:log_line("Evolver cocoon policy rejected for "+c.owner+"; command must be a 1-63 character identifier");return 0;
        case 3:c.command=lower_ascii(c.input);return 0;
        case 4:c.guard.emplace();return 0;
        case 5:return g_policy_registration_open;
        case 6:return g_registration_transaction.active;
        case 7:return g_evolver_cocoon_command.empty();
        case 8:g_evolver_cocoon_command=std::move(c.command);g_evolver_cocoon_owner=c.owner;return 0;
        case 9:c.guard.reset();return 0;
        case 10:log_line("Evolver cocoon policy registered by "+c.owner+": "+lower_ascii(c.input));return 0;
        case 11:log_line("Evolver cocoon policy rejected for "+c.owner+"; startup registration is closed");return 0;
        case 12:log_line("Evolver cocoon policy rejected for "+c.owner+"; already owned by "+c.existing_owner);return 0;
        case 13:c.existing_owner=g_evolver_cocoon_owner;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_registration(1,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result==1;
}

bool register_odf_overlay_directory_policy(const char* owner_value,const char* directory_value,std::uint32_t precedence) {
    const std::string owner=policy_owner(owner_value);
    struct Context {const std::string& owner;const char* input;std::uint32_t precedence;
        std::string directory{},existing_owner{};std::optional<std::string> key{};
        decltype(g_odf_overlay_directories)::const_iterator item{};
        std::exception_ptr error{};std::optional<StateLockGuard> guard{};
    } context{owner,directory_value,precedence};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return normalize_relative_directory(c.input,c.directory);
        case 1:log_line("ODF overlay directory rejected for "+c.owner+"; expected a safe relative path and known precedence");return 0;
        case 2:c.guard.emplace();return 0;
        case 3:return g_policy_registration_open;
        case 4:return g_registration_transaction.active;
        case 5:c.key.emplace(lower_ascii(c.directory));c.item=g_odf_overlay_directories.begin();return 0;
        case 6:return c.item!=g_odf_overlay_directories.end();
        case 7:return lower_ascii(c.item->directory)==*c.key;
        case 8:++c.item;return 0;
        case 9:return g_odf_overlay_directories.size();
        case 10:g_odf_overlay_directories.push_back(OdfOverlayDirectoryPolicy{c.directory,c.owner,c.precedence});return 0;
        case 11:c.existing_owner=c.item->owner;return 0;
        case 12:c.key.reset();c.guard.reset();return 0;
        case 13:log_line("ODF overlay directory registered by "+c.owner+": "+c.directory+" (precedence="+std::to_string(c.precedence)+")");return 0;
        case 14:log_line("ODF overlay directory rejected for "+c.owner+"; startup registration is closed");return 0;
        case 15:return !c.existing_owner.empty();
        case 16:log_line("ODF overlay directory rejected for "+c.owner+"; "+c.directory+" is already owned by "+c.existing_owner);return 0;
        case 17:log_line("ODF overlay directory rejected for "+c.owner+"; the directory limit was reached");return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_overlay(precedence,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result==1;
}

void begin_module_registration(const std::string& path) {
    struct Context {const std::string& path;std::optional<RegistrationTransaction> snapshot{};
        std::exception_ptr error{};std::optional<StateLockGuard> guard{};
    } context{path};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:c.guard.emplace();return 0;
        case 1:c.snapshot.emplace();return 0;
        case 2:g_registration_transaction=std::move(*c.snapshot);return 0;
        case 3:c.snapshot.reset();c.guard.reset();return 0;
        case 4:c.error=nullptr;g_registration_transaction=RegistrationTransaction{};log_line("Module registration transaction could not start: "+c.path);return 0;
        case 10:c.snapshot->active = true;return 0;
        case 11:c.snapshot->path = c.path;return 0;
        case 12:c.snapshot->fofs_handler = g_fofs_item_lookup_handler;return 0;
        case 13:c.snapshot->fofs_user_data = g_fofs_item_lookup_user_data;return 0;
        case 14:c.snapshot->fofs_owner = g_fofs_item_lookup_owner;return 0;
        case 15:c.snapshot->info_ini_defaults_handler = g_info_ini_defaults_handler;return 0;
        case 16:c.snapshot->info_ini_defaults_user_data = g_info_ini_defaults_user_data;return 0;
        case 17:c.snapshot->info_ini_defaults_owner = g_info_ini_defaults_owner;return 0;
        case 18:c.snapshot->classlabel_aliases = g_classlabel_aliases;return 0;
        case 19:c.snapshot->classlabel_odf_defaults = g_classlabel_odf_defaults;return 0;
        case 20:c.snapshot->race_odf_defaults = g_race_odf_defaults;return 0;
        case 21:c.snapshot->odf_overlay_directories = g_odf_overlay_directories;return 0;
        case 22:c.snapshot->cocoon_command = g_evolver_cocoon_command;return 0;
        case 23:c.snapshot->cocoon_owner = g_evolver_cocoon_owner;return 0;
        case 24:c.snapshot->object_destroyed_handlers = g_object_destroyed_handlers;return 0;
        case 25:c.snapshot->producer_event_handlers = g_producer_event_handlers;return 0;
        case 26:c.snapshot->game_object_class_loaded_handlers = g_game_object_class_loaded_handlers;return 0;
        case 27:c.snapshot->race_loaded_handlers = g_race_loaded_handlers;return 0;
        case 28:c.snapshot->weapon_class_loaded_handlers = g_weapon_class_loaded_handlers;return 0;
        case 29:c.snapshot->weapon_trigger_handlers = g_weapon_trigger_handlers;return 0;
        case 30:c.snapshot->craft_event_handlers = g_craft_event_handlers;return 0;
        case 31:c.snapshot->game_object_class_odf_fields = g_game_object_class_odf_fields;return 0;
        case 32:c.snapshot->race_odf_fields = g_race_odf_fields;return 0;
        case 33:c.snapshot->weapon_class_odf_fields = g_weapon_class_odf_fields;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_core_transaction(0,0,step,&context);
    if(context.error)std::rethrow_exception(context.error);
}
void finish_module_registration(const std::string& path,bool initialized) {
    struct Context {const std::string& path;std::exception_ptr error{};std::optional<StateLockGuard> guard{};} context{path};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:c.guard.emplace();return 0;
        case 1:return g_registration_transaction.active;
        case 2:return g_registration_transaction.path==c.path;
        case 3:g_registration_transaction=RegistrationTransaction{};return 0;
        case 4:c.guard.reset();return 0;
        case 5:log_line("Module loader: rolled back registrations from "+c.path);return 0;
        case 10:g_fofs_item_lookup_handler = g_registration_transaction.fofs_handler;return 0;
        case 11:g_fofs_item_lookup_user_data = g_registration_transaction.fofs_user_data;return 0;
        case 12:g_fofs_item_lookup_owner = std::move(g_registration_transaction.fofs_owner);return 0;
        case 13:g_info_ini_defaults_handler = g_registration_transaction.info_ini_defaults_handler;return 0;
        case 14:g_info_ini_defaults_user_data = g_registration_transaction.info_ini_defaults_user_data;return 0;
        case 15:g_info_ini_defaults_owner = std::move( g_registration_transaction.info_ini_defaults_owner);return 0;
        case 16:g_classlabel_aliases = std::move(g_registration_transaction.classlabel_aliases);return 0;
        case 17:g_classlabel_odf_defaults = std::move( g_registration_transaction.classlabel_odf_defaults);return 0;
        case 18:g_race_odf_defaults = std::move( g_registration_transaction.race_odf_defaults);return 0;
        case 19:g_odf_overlay_directories = std::move( g_registration_transaction.odf_overlay_directories);return 0;
        case 20:g_evolver_cocoon_command = std::move(g_registration_transaction.cocoon_command);return 0;
        case 21:g_evolver_cocoon_owner = std::move(g_registration_transaction.cocoon_owner);return 0;
        case 22:g_object_destroyed_handlers = std::move( g_registration_transaction.object_destroyed_handlers);return 0;
        case 23:g_producer_event_handlers = std::move( g_registration_transaction.producer_event_handlers);return 0;
        case 24:g_game_object_class_loaded_handlers = std::move( g_registration_transaction.game_object_class_loaded_handlers);return 0;
        case 25:g_race_loaded_handlers = std::move( g_registration_transaction.race_loaded_handlers);return 0;
        case 26:g_weapon_class_loaded_handlers = std::move( g_registration_transaction.weapon_class_loaded_handlers);return 0;
        case 27:g_weapon_trigger_handlers = std::move( g_registration_transaction.weapon_trigger_handlers);return 0;
        case 28:g_craft_event_handlers = std::move( g_registration_transaction.craft_event_handlers);return 0;
        case 29:g_game_object_class_odf_fields = std::move( g_registration_transaction.game_object_class_odf_fields);return 0;
        case 30:g_race_odf_fields = std::move( g_registration_transaction.race_odf_fields);return 0;
        case 31:g_weapon_class_odf_fields = std::move( g_registration_transaction.weapon_class_odf_fields);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_core_transaction(1,initialized,step,&context);
    if(context.error)std::rethrow_exception(context.error);
}

void* __attribute__((regparm(3))) fofs_item_get_hash_lookup_hook(void* file_system,void* delphi_name,std::uintptr_t flags) {
    struct Context {void* file_system;void* delphi_name;std::uintptr_t flags;A2FO_FofsItemLookupHandler handler=nullptr;void* user_data=nullptr;void* external_result=nullptr;std::exception_ptr error{};} context{file_system,delphi_name,flags};
    const auto step=+[](void* opaque,std::uint32_t op) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return g_fofs_item_lookup_dispatch_ready.load(std::memory_order_acquire);
        case 1:wait_for_deferred_initialization();return 0;
        case 2:return g_fofs_item_lookup_dispatch_ready.load(std::memory_order_acquire);
        case 3:c.handler=g_fofs_item_lookup_dispatch.handler;c.user_data=g_fofs_item_lookup_dispatch.user_data;return 0;
        case 4:EnterCriticalSection(&g_state_lock);c.handler=g_fofs_item_lookup_handler;c.user_data=g_fofs_item_lookup_user_data;LeaveCriticalSection(&g_state_lock);return 0;
        case 5:return c.handler!=nullptr;
        case 6:return c.handler(c.file_system,c.delphi_name,static_cast<std::uint32_t>(c.flags&0xffu),&c.external_result,c.user_data);
        case 7:return reinterpret_cast<std::uintptr_t>(c.external_result);
        case 8:return reinterpret_cast<std::uintptr_t>(a2fo_call_get_file_from_hash(at(g_fleet_ops,kGetFileFromHashTableRva),c.file_system,c.delphi_name,static_cast<std::uint32_t>(c.flags&0xffu)));
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_lookup(0,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(result));
}

std::string normalize_cocoon_name(const char* value) {
    if(!value)return {};
    std::string name(value);std::size_t first=0,last=0;
    a2fo_rs_stage8_core_trim(reinterpret_cast<const std::uint8_t*>(name.data()),name.size(),&first,&last);
    if(first==name.size())return {};
    name=name.substr(first,last-first);
    replace_slashes(name);
    const std::string lower=lower_ascii(name);
    if(!a2fo_rs_stage8_core_suffix(reinterpret_cast<const std::uint8_t*>(lower.data()),lower.size(),0))name+=".sod";
    return name;
}

void* original_parameter_db_get_string() {
    if (g_parameter_db_get_string_hook.gateway) {
        return g_parameter_db_get_string_hook.gateway;
    }

    return at(g_armada, kParameterDbGetStringRva);
}

bool classlabel_odf_default(void* parameter_db,const char* key,std::string& value,std::string& classlabel) {
    struct Context {void* db;const char* key;std::string& value;std::string& classlabel;std::optional<StateLockGuard> lock{};decltype(g_original_classlabels)::iterator source{};decltype(g_classlabel_odf_defaults)::iterator policy{};decltype(ClasslabelOdfDefaultsPolicy::defaults)::iterator item{};std::exception_ptr error{};} context{parameter_db,key,value,classlabel};
    const auto step=+[](void* opaque,std::uint32_t op) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return c.db!=nullptr;
        case 1:return c.key!=nullptr;
        case 2:return *c.key!=0;
        case 3:c.lock.emplace();return 0;
        case 4:c.source=g_original_classlabels.find(c.db);return c.source!=g_original_classlabels.end();
        case 5:c.policy=g_classlabel_odf_defaults.find(c.source->second);return c.policy!=g_classlabel_odf_defaults.end();
        case 6:c.item=c.policy->second.defaults.find(lower_ascii(c.key));return c.item!=c.policy->second.defaults.end();
        case 7:c.classlabel=c.source->second;return 0;
        case 8:c.value=c.item->second;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_classdefault(0,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result!=0;
}

void log_classlabel_odf_default(const std::string& classlabel,
                                const char* key,
                                const std::string& value) {
    log_line("Classlabel ODF default applied: " + classlabel + "." +
             (key ? key : "<null>") + " = " + value);
}

bool apply_integer_classlabel_default(void* parameter_db,const char* key,int* output) {
    struct Context {void* db;const char* key;int* output;std::string value{};std::string classlabel{};std::exception_ptr error{};} context{parameter_db,key,output};
    const auto step=+[](void* opaque,std::uint32_t op) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return c.output!=nullptr;
        case 2:return classlabel_odf_default(c.db,c.key,c.value,c.classlabel);
        case 11:log_classlabel_odf_default(c.classlabel,c.key,c.value);return 0;
        case 4:*c.output=std::atoi(c.value.c_str());return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_classdefault(1,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result!=0;
}

bool apply_float_classlabel_default(void* parameter_db,const char* key,float* output) {
    struct Context {void* db;const char* key;float* output;std::string value{};std::string classlabel{};std::exception_ptr error{};} context{parameter_db,key,output};
    const auto step=+[](void* opaque,std::uint32_t op) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return c.output!=nullptr;
        case 2:return classlabel_odf_default(c.db,c.key,c.value,c.classlabel);
        case 11:log_classlabel_odf_default(c.classlabel,c.key,c.value);return 0;
        case 4:*c.output=std::strtof(c.value.c_str(),nullptr);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_classdefault(2,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result!=0;
}

bool apply_bool_classlabel_default(void* parameter_db,const char* key,std::uint8_t* output) {
    struct Context {void* db;const char* key;std::uint8_t* output;std::string value{};std::string classlabel{};std::string normalized{};std::exception_ptr error{};} context{parameter_db,key,output};
    const auto step=+[](void* opaque,std::uint32_t op) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return c.output!=nullptr;
        case 2:return classlabel_odf_default(c.db,c.key,c.value,c.classlabel);
        case 11:log_classlabel_odf_default(c.classlabel,c.key,c.value);return 0;
        case 4:c.normalized=lower_ascii(c.value);return 0;
        case 5:return c.normalized=="true";
        case 6:return c.normalized=="yes";
        case 7:return c.normalized=="on";
        case 8:return std::atoi(c.normalized.c_str())!=0;
        case 9:*c.output=1;return 0;
        case 10:*c.output=0;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_classdefault(3,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result!=0;
}

bool apply_string_classlabel_default(void* parameter_db,const char* key,char* output,std::uint32_t output_size) {
    struct Context {void* db;const char* key;char* output;std::uint32_t size;std::string value{};std::string classlabel{};std::exception_ptr error{};} context{parameter_db,key,output,output_size};
    const auto step=+[](void* opaque,std::uint32_t op) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return c.output!=nullptr;
        case 2:return classlabel_odf_default(c.db,c.key,c.value,c.classlabel);
        case 11:log_classlabel_odf_default(c.classlabel,c.key,c.value);return 0;
        case 1:return c.size!=0;
        case 3:return c.value.size()+1<=c.size;
        case 4:std::memcpy(c.output,c.value.c_str(),c.value.size()+1);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_classdefault(4,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result!=0;
}

// Native calling conventions, pointer-sized defaults and output ownership stay
// here; Rust owns which probe/policy/fallback steps run and in what order.
bool parameter_db_scalar_fallback(std::uint32_t kind, void* self,
                                  const char* key, void* output,
                                  std::uint32_t default_bits) {
    struct Context {
        std::uint32_t kind;
        void* self;
        const char* key;
        void* output;
        std::uint32_t bits;
        std::exception_ptr policy_error{};
        std::exception_ptr fatal{};
    } context{kind, self, key, output, default_bits};
    const auto step = +[](void* opaque, std::uint32_t operation) noexcept
        -> std::uint64_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (operation) {
            case 0: {
                void* gateway = nullptr;
                switch (c.kind) {
                case 0: gateway = g_parameter_db_get_int_hook.gateway; break;
                case 1: gateway = g_parameter_db_get_int_alternative_hook.gateway; break;
                case 2: gateway = g_parameter_db_get_float_hook.gateway; break;
                default: gateway = g_parameter_db_get_bool_hook.gateway; break;
                }
                return a2fo_parameter_db_get_scalar(
                    gateway, c.self, c.key, c.output, c.bits);
            }
            case 1:
                switch (c.kind) {
                case 0:
                case 1:
                    return apply_integer_classlabel_default(
                        c.self, c.key, static_cast<int*>(c.output));
                case 2:
                    return apply_float_classlabel_default(
                        c.self, c.key, static_cast<float*>(c.output));
                default:
                    return apply_bool_classlabel_default(
                        c.self, c.key, static_cast<std::uint8_t*>(c.output));
                }
            case 5: {
                // The original exception remains alive through its catch log,
                // then dies before the hook returns to its caller.
                auto original = std::move(c.policy_error);
                try {
                    std::rethrow_exception(original);
                } catch (...) {
                    constexpr const char* messages[] = {
                        "ParameterDB integer default skipped after an unexpected C++ exception",
                        "ParameterDB alternative integer default skipped after an unexpected C++ exception",
                        "ParameterDB float default skipped after an unexpected C++ exception",
                        "ParameterDB boolean default skipped after an unexpected C++ exception",
                    };
                    log_line(messages[c.kind]);
                }
                return 0;
            }
            default: return 0;
            }
        } catch (...) {
            if (operation == 1) c.policy_error = std::current_exception();
            else c.fatal = std::current_exception();
            return UINT64_MAX;
        }
    };
    const bool result = a2fo_rs_stage9_parameter_get(0, step, &context) != 0;
    if (context.fatal) std::rethrow_exception(context.fatal);
    return result;
}

bool __attribute__((fastcall)) parameter_db_get_int_hook(
    void* self, void*, const char* key, int* output, int default_value) {
    return parameter_db_scalar_fallback(
        0, self, key, output, static_cast<std::uint32_t>(default_value));
}

bool __attribute__((fastcall)) parameter_db_get_int_alternative_hook(
    void* self, void*, const char* key, int* output, int default_value) {
    return parameter_db_scalar_fallback(
        1, self, key, output, static_cast<std::uint32_t>(default_value));
}

bool __attribute__((fastcall)) parameter_db_get_float_hook(
    void* self, void*, const char* key, float* output, float default_value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(default_value),
                  "ParameterDB float default must occupy four bytes");
    std::memcpy(&bits, &default_value, sizeof(bits));
    return parameter_db_scalar_fallback(2, self, key, output, bits);
}

bool __attribute__((fastcall)) parameter_db_get_bool_hook(
    void* self, void*, const char* key, std::uint8_t* output,
    std::uint32_t default_value) {
    return parameter_db_scalar_fallback(3, self, key, output, default_value);
}

std::uint32_t parameter_db_pointer_bits(const void* value) {
    static_assert(sizeof(void*) == sizeof(std::uint32_t),
                  "Armada ParameterDB pointers must occupy four bytes");
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value));
}

// MSVC 6's std::vector<char*> has an allocator word and three native pointers.
struct ParameterDbStringVectorAbi {
    std::uint32_t allocator = 0;
    char** begin = nullptr;
    char** end = nullptr;
    char** capacity = nullptr;
};
static_assert(sizeof(ParameterDbStringVectorAbi) == 16,
              "Armada string-vector ABI must occupy sixteen bytes");

bool parameter_db_policy_fallback(bool vector, void* self, const char* key,
                                  void* output, const void* default_value) {
    struct NativeVectorDefault {
        char* item;
        ParameterDbStringVectorAbi abi;
        explicit NativeVectorDefault(const char* value)
            : item(const_cast<char*>(value)) {
            abi.begin = &item;
            abi.end = &item + 1;
            abi.capacity = abi.end;
        }
    };
    struct Policy {
        std::string value;
        std::string classlabel;
        std::optional<NativeVectorDefault> vector;
    };
    struct Context {
        bool vector;
        void* self;
        const char* key;
        void* output;
        const void* fallback;
        std::optional<Policy> policy{};
        std::exception_ptr policy_error{};
        std::exception_ptr fatal{};
    } context{vector, self, key, output, default_value};
    const auto step = +[](void* opaque, std::uint32_t operation) noexcept
        -> std::uint64_t {
        auto& c = *static_cast<Context*>(opaque);
        const auto gateway = [&c]() {
            return c.vector ? g_parameter_db_get_string_vector_hook.gateway
                            : g_parameter_db_get_owned_string_hook.gateway;
        };
        try {
            switch (operation) {
            case 0:
                return a2fo_parameter_db_get_scalar(
                    gateway(), c.self, c.key, c.output, 0);
            case 1:
                c.policy.emplace();
                return classlabel_odf_default(
                    c.self, c.key, c.policy->value, c.policy->classlabel);
            case 2: {
                const void* native_default = c.policy->value.c_str();
                if (c.vector) {
                    c.policy->vector.emplace(c.policy->value.c_str());
                    native_default = &c.policy->vector->abi;
                }
                // Keep native policy storage live through the subsequent log;
                // engine callbacks can inspect the borrowed vector until then.
                a2fo_parameter_db_get_scalar(
                    gateway(), c.self, c.key, c.output,
                    parameter_db_pointer_bits(native_default));
                return 0; // Original code intentionally ignored this bool.
            }
            case 3:
                log_classlabel_odf_default(
                    c.policy->classlabel, c.key, c.policy->value);
                return 0;
            case 4:
                c.policy.reset();
                return 0;
            case 5: {
                auto original = std::move(c.policy_error);
                try {
                    std::rethrow_exception(original);
                } catch (...) {
                    log_line(c.vector
                        ? "ParameterDB string-vector default skipped after an unexpected C++ exception"
                        : "ParameterDB owned-string default skipped after an unexpected C++ exception");
                }
                return 0;
            }
            case 6:
                return a2fo_parameter_db_get_scalar(
                    gateway(), c.self, c.key, c.output,
                    parameter_db_pointer_bits(c.fallback));
            default: return 0;
            }
        } catch (...) {
            if (operation >= 1 && operation <= 3)
                c.policy_error = std::current_exception();
            else c.fatal = std::current_exception();
            return UINT64_MAX;
        }
    };
    const bool result = a2fo_rs_stage9_parameter_get(1, step, &context) != 0;
    if (context.fatal) std::rethrow_exception(context.fatal);
    return result;
}

bool __attribute__((fastcall)) parameter_db_get_owned_string_hook(
    void* self, void*, const char* key, char** output,
    const char* default_value) {
    return parameter_db_policy_fallback(false, self, key, output, default_value);
}

bool __attribute__((fastcall)) parameter_db_get_string_vector_hook(
    void* self, void*, const char* key, void* output,
    const void* default_value) {
    return parameter_db_policy_fallback(true, self, key, output, default_value);
}

std::string normalize_odf_basename(std::string value) {
    std::size_t first=0,last=0;
    a2fo_rs_stage8_core_trim(reinterpret_cast<const std::uint8_t*>(value.data()),value.size(),&first,&last);
    if(first==value.size())return {};
    value=value.substr(first,last-first);
    replace_slashes(value);
    const auto begin=a2fo_rs_stage8_core_basename(reinterpret_cast<const std::uint8_t*>(value.data()),value.size());
    if(begin)value.erase(0,begin);
    value=lower_ascii(std::move(value));
    if(a2fo_rs_stage8_core_suffix(reinterpret_cast<const std::uint8_t*>(value.data()),value.size(),1))value.resize(value.size()-4);
    return value;
}

bool read_odf_snapshot_value(void* parameter_db, const char* key,
                             std::string& value) {
    struct Context {
        void* parameter_db;const char* key;std::string& value;
        std::array<char,4096> output{};const char* end{};
        float float_value{};std::array<char,48> numeric{};int length{};
        int integer_value{};std::exception_ptr error{};
    } context{parameter_db,key,value};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id){
        case 0:return a2fo_parameter_db_get_string(
            original_parameter_db_get_string(),c.parameter_db,c.key,
            c.output.data(),static_cast<std::uint32_t>(c.output.size()),"");
        case 1:c.output.back()='\0';return 0;
        case 2:c.end=static_cast<const char*>(
            std::memchr(c.output.data(),'\0',c.output.size()));return 0;
        case 3:return c.end!=nullptr;
        case 4:c.value.assign(c.output.data(),
            static_cast<std::size_t>(c.end-c.output.data()));return 0;
        case 5:return g_parameter_db_get_float_hook.gateway!=nullptr;
        case 6:return a2fo_parameter_db_get_scalar(
            g_parameter_db_get_float_hook.gateway,c.parameter_db,c.key,
            &c.float_value,0);
        case 7:c.length=std::snprintf(c.numeric.data(),c.numeric.size(),
            "%.9g",static_cast<double>(c.float_value));return 0;
        case 8:return c.length>0;
        case 9:return static_cast<std::size_t>(c.length)<c.numeric.size();
        case 10:c.value.assign(c.numeric.data(),static_cast<std::size_t>(c.length));return 0;
        case 11:return g_parameter_db_get_int_hook.gateway!=nullptr;
        case 12:return a2fo_parameter_db_get_scalar(
            g_parameter_db_get_int_hook.gateway,c.parameter_db,c.key,
            &c.integer_value,0);
        case 13:c.value=std::to_string(c.integer_value);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool found=a2fo_rs_stage8_core_odf_snapshot(&context,op)!=0;
    if(context.error)std::rethrow_exception(context.error);
    return found;
}

void capture_game_object_class_odf_fields(void* parameter_db) {
    struct Context {void* db;std::vector<std::string> fields{};const std::string* command=nullptr;std::optional<std::string> value{};std::exception_ptr error{};} context{parameter_db};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);(void)a;
        try{switch(op){
        case 0:return c.db!=nullptr;
        case 1:return g_game_object_class_odf_load_context!=nullptr;
        case 2:g_game_object_class_odf_load_context->parameter_db_observed=true;return 0;
        case 3:{StateLockGuard lock;c.fields.assign(g_game_object_class_odf_fields.begin(),g_game_object_class_odf_fields.end());return 0;}
        case 4:return c.fields.size();
        case 5:c.command=&c.fields[a];return 0;
        case 6:return *c.command=="basename";
        case 7:c.value.emplace();return 0;
        case 8:return read_odf_snapshot_value(c.db,c.command->c_str(),*c.value);
        case 9:g_game_object_class_odf_load_context->fields[*c.command]=std::move(*c.value);return 0;
        case 10:c.value.reset();return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_core_classload(0,step,&context);
    if(context.error)std::rethrow_exception(context.error);
}

bool __attribute__((fastcall)) parameter_db_get_string_hook(void* self,void*,const char* key,char* output,std::uint32_t output_size,const char* default_value) {
    const bool found=a2fo_parameter_db_get_string(original_parameter_db_get_string(),self,key,output,output_size,default_value);
    if(!a2fo_rs_stage8_core_classload_admit(reinterpret_cast<const std::uint8_t*>(key),reinterpret_cast<std::uint8_t*>(output),output_size))return found;
    wait_for_deferred_initialization();
    try {
    struct Context {void* db;const char* key;char* output;std::uint32_t size;bool found;const char* end=nullptr;std::optional<std::string> source{};std::optional<std::string> replacement{};std::optional<StateLockGuard> lock{};decltype(g_classlabel_aliases)::iterator alias{};std::exception_ptr error{};} context{self,key,output,output_size,found};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);(void)a;
        try{switch(op){
        case 0:return _stricmp(c.key,"classlabel")==0;
        case 1:return c.found;
        case 2:return apply_string_classlabel_default(c.db,c.key,c.output,c.size);
        case 3:capture_game_object_class_odf_fields(c.db);return 0;
        case 4:c.end=static_cast<const char*>(std::memchr(c.output,'\0',c.size));return 0;
        case 5:return c.end!=nullptr;
        case 6:c.source.emplace(lower_ascii(std::string(c.output,static_cast<std::size_t>(c.end-c.output))));return 0;
        case 7:c.replacement.emplace();return 0;
        case 8:c.lock.emplace();return 0;
        case 9:g_original_classlabels[c.db]=*c.source;return 0;
        case 10:c.alias=g_classlabel_aliases.find(*c.source);return c.alias!=g_classlabel_aliases.end();
        case 11:*c.replacement=c.alias->second.target;return 0;
        case 12:c.lock.reset();return 0;
        case 13:return !c.replacement->empty();
        case 14:return c.replacement->size()+1<=c.size;
        case 15:std::memcpy(c.output,c.replacement->c_str(),c.replacement->size()+1);return 0;
        case 16:log_line("Classlabel alias applied: "+*c.source+" -> "+*c.replacement);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_classload(1,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result!=0;
    }catch(...){log_line("ParameterDB string policy skipped after an unexpected C++ exception");}
    return found;
}

void* A2FO_CALL game_object_class_find_project_id_hook(const std::uint32_t* project_id) {
    struct Context {const std::uint32_t* project_id;a2fo::native11_light_classes::ClassLoadScope class_scope{};GameObjectClassOdfLoadContext load{};GameObjectClassOdfLoadContext* previous=nullptr;void* object_class=nullptr;const char* odf_name=nullptr;const char* odf_end=nullptr;std::optional<std::string> basename{};std::optional<std::string> key{};std::optional<std::string> field_names{};std::optional<OdfSnapshot> snapshot{};a2fo::RustHandlerRegistry<NativeGameObjectClassLoadedRegistration> handlers{};std::optional<std::vector<A2FO_OdfFieldView>> fields{};std::optional<A2FO_GameObjectClassLoadedEvent> event{};OdfSnapshot::const_iterator field{};a2fo::RustHandlerRegistry<NativeGameObjectClassLoadedRegistration>::const_iterator registration{};std::exception_ptr error{};} context{project_id};
    a2fo::RestoreClassLoadContext<GameObjectClassOdfLoadContext> restore_context(g_game_object_class_odf_load_context);
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);(void)a;
        try{switch(op){
        case 0:c.previous=g_game_object_class_odf_load_context;g_game_object_class_odf_load_context=&c.load;return 0;
        case 1:{using Original=void*(A2FO_CALL*)(const std::uint32_t*);c.object_class=reinterpret_cast<Original>(g_game_object_class_find_project_id_hook.gateway)(c.project_id);return 0;}
        case 2:g_game_object_class_odf_load_context=c.previous;return 0;
        case 3:return c.object_class!=nullptr;
        case 4:return c.load.parameter_db_observed;
        case 5:c.odf_name=static_cast<const char*>(a2fo_call_game_object_method(at(g_armada,kGameObjectClassGetOdfNameRva),c.object_class));return 0;
        case 6:return c.odf_name!=nullptr;
        case 7:c.odf_end=static_cast<const char*>(std::memchr(c.odf_name,'\0',256));return 0;
        case 8:return c.odf_end!=nullptr;
        case 9:c.basename.emplace(c.odf_name,static_cast<std::size_t>(c.odf_end-c.odf_name));c.key.emplace(normalize_odf_basename(*c.basename));return 0;
        case 10:return !c.key->empty();
        case 11:a2fo::register_nebula_cloak_class_name(c.object_class,c.key->c_str());return 0;
        case 12:c.field_names.emplace();c.field=c.load.fields.begin();return 0;
        case 13:return c.field!=c.load.fields.end();
        case 14:return c.field_names->empty();
        case 15:*c.field_names+=", ";return 0;
        case 16:*c.field_names+=c.field->first;++c.field;return 0;
        case 17:c.load.fields["basename"]=*c.basename;return 0;
        case 18:c.snapshot.emplace(c.load.fields);return 0;
        case 19:{{StateLockGuard lock;g_odf_snapshots[*c.key]=std::move(c.load.fields);}
            c.class_scope.observe_class(c.object_class,*c.basename,*c.snapshot);return 0;}
        case 20:{StateLockGuard lock;c.handlers=g_game_object_class_loaded_handlers;return 0;}
        case 21:return !c.handlers.empty();
        case 22:c.fields.emplace();c.fields->reserve(c.snapshot->size());c.field=c.snapshot->begin();return 0;
        case 23:return c.field!=c.snapshot->end();
        case 24:{const auto& field=*c.field++;c.fields->push_back(A2FO_OdfFieldView{A2FO_StringView{field.first.data(),static_cast<std::uint32_t>(field.first.size())},A2FO_StringView{field.second.data(),static_cast<std::uint32_t>(field.second.size())}});return 0;}
        case 25:c.event.emplace();c.event->struct_size=sizeof(*c.event);c.event->object_class=c.object_class;c.event->source_odf=A2FO_StringView{c.basename->data(),static_cast<std::uint32_t>(c.basename->size())};c.event->odf_fields=c.fields->data();c.event->odf_field_count=static_cast<std::uint32_t>(c.fields->size());return 0;
        case 26:c.registration=c.handlers.begin();return 0;
        case 27:return c.registration!=c.handlers.end();
        case 28:return c.registration->handler!=nullptr;
        case 29:try{c.registration->handler(&*c.event,c.registration->user_data);}catch(...){c.class_scope.fail();log_line("GameObjectClass-loaded handler failed for "+c.registration->owner);}return 0;
        case 30:++c.registration;return 0;
        case 31:c.event.reset();c.fields.reset();return 0;
        case 32:log_line("GameObjectClass ODF fields cached: "+*c.basename+" ("+*c.field_names+")");return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_core_classload(2,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    context.class_scope.complete();
    return context.object_class;
}

extern "C" void __cdecl a2fo_dispatch_race_odf_loaded(
    void* race, void* parameter_db) noexcept {
    if (!race || !parameter_db || !g_state_lock_ready) return;
    try {
        struct Context {
            void* race;void* parameter_db;
            std::vector<std::string> requested_fields{};
            a2fo::RustHandlerRegistry<NativeRaceLoadedRegistration> handlers{};
            std::unordered_map<std::string,RaceOdfDefaultPolicy> defaults{};
            std::optional<OdfSnapshot> snapshot{};
            std::optional<std::vector<A2FO_OdfFieldView>> fields{};
            std::optional<A2FO_RaceLoadedEvent> event{};
            std::optional<std::string> value{};
            const std::string* command{};
            decltype(defaults)::const_iterator fallback{};
            OdfSnapshot::const_iterator field{};
            std::optional<a2fo::RustHandlerRegistry<NativeRaceLoadedRegistration>::Borrowed> registration{};
            std::exception_ptr error{};
        } context{race,parameter_db};
        const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
            auto& c=*static_cast<Context*>(opaque);
            try {
                if(id>=0x30000){c.registration=c.handlers.borrow(id-0x30000);return 0;}
                if(id>=0x20000){const auto& item=*c.field++;
                    c.fields->push_back(A2FO_OdfFieldView{
                        A2FO_StringView{item.first.data(),static_cast<std::uint32_t>(item.first.size())},
                        A2FO_StringView{item.second.data(),static_cast<std::uint32_t>(item.second.size())}});
                    return 0;}
                if(id>=0x10000){c.command=&c.requested_fields.at(id-0x10000);
                    c.value.emplace();return 0;}
                switch(id){
                case 21:{StateLockGuard lock;
                    c.requested_fields.assign(g_race_odf_fields.begin(),g_race_odf_fields.end());
                    c.handlers=g_race_loaded_handlers;
                    c.defaults=g_race_odf_defaults;return 0;}
                case 22:return !c.handlers.empty();
                case 23:c.snapshot.emplace();return c.requested_fields.size();
                case 24:return read_odf_snapshot_value(
                    c.parameter_db,c.command->c_str(),*c.value);
                case 25:(*c.snapshot)[*c.command]=std::move(*c.value);return 0;
                case 26:c.fallback=c.defaults.find(*c.command);
                    return c.fallback!=c.defaults.end();
                case 27:(*c.snapshot)[*c.command]=c.fallback->second.value;return 0;
                case 28:c.value.reset();return 0;
                case 29:c.fields.emplace();c.fields->reserve(c.snapshot->size());return 0;
                case 30:c.field=c.snapshot->begin();return c.snapshot->size();
                case 31:c.event.emplace();c.event->struct_size=sizeof(*c.event);c.event->race=c.race;
                    c.event->odf_fields=c.fields->data();
                    c.event->odf_field_count=static_cast<std::uint32_t>(c.fields->size());return 0;
                case 32:return c.handlers.size();
                case 33:return c.registration->handler!=nullptr;
                case 34:try{c.registration->handler(&*c.event,c.registration->user_data);}
                    catch(...){log_line("Race-loaded handler failed for "+c.registration->owner);}
                    return 0;
                default:return 0;
                }
            }catch(...){c.error=std::current_exception();return UINT64_MAX;}
        };
        a2fo_rs_stage8_core_odf_race(&context,op);
        if(context.error)std::rethrow_exception(context.error);
    } catch (...) {
        log_line("Race-loaded dispatch skipped after an unexpected C++ "
                 "exception");
    }
}

void dispatch_weapon_class_loaded(
    void* weapon_class, void* parent_class, void* parameter_db) noexcept {
    try {
    struct Context {
        void* weapon_class;
        void* parent_class;
        void* parameter_db;
        OdfSnapshot snapshot;
        std::set<std::string>::const_iterator command;
        std::set<std::string>::const_iterator command_end;
        std::optional<std::string> value;
        std::vector<A2FO_OdfFieldView> fields;
        OdfSnapshot::const_iterator field;
        OdfSnapshot::const_iterator field_end;
        A2FO_WeaponClassLoadedEvent event{};
        a2fo::RustHandlerRegistry<NativeWeaponClassLoadedRegistration>::const_iterator registration;
        a2fo::RustHandlerRegistry<NativeWeaponClassLoadedRegistration>::const_iterator registration_end;
        std::exception_ptr error;
    } context{};
    context.weapon_class=weapon_class;
    context.parent_class=parent_class;
    context.parameter_db=parameter_db;
    auto op=[](void* opaque,std::uint32_t id)->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 0:return c.weapon_class!=nullptr;
            case 1:return c.parameter_db!=nullptr;
            case 2:return !g_weapon_class_loaded_handlers.empty();
            case 3:c.command=g_weapon_class_odf_fields.begin();c.command_end=g_weapon_class_odf_fields.end();return 0;
            case 4:return c.command!=c.command_end;
            case 5:c.value.emplace();return 0;
            case 6:return read_odf_snapshot_value(c.parameter_db,c.command->c_str(),*c.value);
            case 7:c.snapshot[*c.command]=std::move(*c.value);return 0;
            case 8:c.value.reset();++c.command;return 0;
            case 9:c.fields.reserve(c.snapshot.size());return 0;
            case 10:c.field=c.snapshot.begin();c.field_end=c.snapshot.end();return 0;
            case 11:return c.field!=c.field_end;
            case 12:c.fields.push_back(A2FO_OdfFieldView{
                A2FO_StringView{c.field->first.data(),static_cast<std::uint32_t>(c.field->first.size())},
                A2FO_StringView{c.field->second.data(),static_cast<std::uint32_t>(c.field->second.size())}});++c.field;return 0;
            case 13:
                c.event.struct_size=sizeof(c.event);c.event.weapon_class=c.weapon_class;
                c.event.parent_class=c.parent_class;c.event.parameter_db=c.parameter_db;
                c.event.odf_fields=c.fields.empty()?nullptr:c.fields.data();
                c.event.odf_field_count=static_cast<std::uint32_t>(c.fields.size());return 0;
            case 14:c.registration=g_weapon_class_loaded_handlers.begin();
                c.registration_end=g_weapon_class_loaded_handlers.end();return 0;
            case 15:return c.registration!=c.registration_end;
            case 16:return c.registration->handler!=nullptr;
            case 17:
                try{c.registration->handler(&c.event,c.registration->user_data);}
                catch(...){log_line("WeaponClass-loaded handler failed for "+c.registration->owner);}
                return 0;
            case 18:++c.registration;return 0;
            default:return 0;
            }
        } catch(...) {c.error=std::current_exception();return UINT64_MAX;}
    };
        a2fo_rs_stage8_core_dispatch_weapon_class(&context,op);
        if(context.error)std::rethrow_exception(context.error);
    } catch (...) {
        log_line("WeaponClass-loaded dispatch skipped after an unexpected "
                 "C++ exception");
    }
}

std::uintptr_t __attribute__((fastcall)) weapon_class_constructor_hook(
    void* self, void*, void* parent_class, void* parameter_db) noexcept {
    const std::uintptr_t result = a2fo_call_thiscall_2(
        g_weapon_class_constructor_original, self,
        reinterpret_cast<std::uintptr_t>(parent_class),
        reinterpret_cast<std::uintptr_t>(parameter_db));
    void* completed = result ? reinterpret_cast<void*>(result) : self;
    dispatch_weapon_class_loaded(completed, parent_class, parameter_db);
    return result;
}

bool dispatch_weapon_trigger(
    std::uint32_t kind, void* weapon, const void* target) noexcept {
    struct Context {
        A2FO_WeaponTriggerEvent event{};
        std::uint32_t original_kind = 0;
        a2fo::RustHandlerRegistry<NativeWeaponTriggerRegistration>::const_iterator registration;
        a2fo::RustHandlerRegistry<NativeWeaponTriggerRegistration>::const_iterator registration_end;
        std::exception_ptr error;
    } context;
    auto op=[](void* opaque,std::uint32_t id)->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 0:c.event.struct_size=sizeof(c.event);return 0;
            case 1:c.registration=g_weapon_trigger_handlers.begin();
                c.registration_end=g_weapon_trigger_handlers.end();return 0;
            case 2:return c.registration!=c.registration_end;
            case 3:return c.registration->handler!=nullptr;
            case 4:
                try{return c.registration->handler(&c.event,c.registration->user_data);}
                catch(...){log_line("Weapon-trigger handler failed for "+c.registration->owner);
                    return c.original_kind==A2FO_WEAPON_TRIGGER_PRECHECK?0:1;}
            case 5:++c.registration;return 0;
            default:return 0;
            }
        } catch(...) {c.error=std::current_exception();return UINT64_MAX;}
    };
    context.original_kind=kind;context.event.kind=kind;
    context.event.weapon=weapon;context.event.target=target;
    const bool result=a2fo_rs_stage8_core_dispatch_weapon_trigger(
        &context,op,kind,A2FO_WEAPON_TRIGGER_PRECHECK)!=0;
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

void __attribute__((fastcall)) weapon_trigger_object_hook(
    void* weapon, void*, const void* target) noexcept {
    if (!dispatch_weapon_trigger(
            A2FO_WEAPON_TRIGGER_PRECHECK, weapon, target)) {
        return;
    }
    a2fo_call_thiscall_1(
        g_weapon_trigger_object_hook.gateway, weapon,
        reinterpret_cast<std::uintptr_t>(target));
    dispatch_weapon_trigger(
        A2FO_WEAPON_TRIGGER_COMMITTED, weapon, target);
}

void dispatch_craft_event(std::uint32_t kind, void* craft,
                          float elapsed_seconds) noexcept {
    struct Context {
        A2FO_CraftEvent event{};
        a2fo::RustHandlerRegistry<NativeCraftEventRegistration>::const_iterator registration;
        a2fo::RustHandlerRegistry<NativeCraftEventRegistration>::const_iterator registration_end;
        std::exception_ptr error;
    } context;
    auto op=[](void* opaque,std::uint32_t id)->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(id) {
            case 0:c.event.struct_size=sizeof(c.event);return 0;
            case 1:c.registration=g_craft_event_handlers.begin();
                c.registration_end=g_craft_event_handlers.end();return 0;
            case 2:return c.registration!=c.registration_end;
            case 3:return c.registration->handler!=nullptr;
            case 4:
                try{c.registration->handler(&c.event,c.registration->user_data);}
                catch(...){log_line("Craft-event handler failed for "+c.registration->owner);}
                return 0;
            case 5:++c.registration;return 0;
            default:return 0;
            }
        } catch(...) {c.error=std::current_exception();return UINT64_MAX;}
    };
    context.event.kind=kind;context.event.craft=craft;
    context.event.elapsed_seconds=elapsed_seconds;
    a2fo_rs_stage8_core_dispatch_craft(&context,op);
    if(context.error)std::rethrow_exception(context.error);
}

void __attribute__((fastcall)) craft_simulate_event_hook(
    void* craft, void*, float elapsed_seconds) noexcept {
    dispatch_craft_event(
        A2FO_CRAFT_EVENT_SIMULATE_PRE, craft, elapsed_seconds);
    std::uint32_t elapsed_bits = 0;
    static_assert(sizeof(elapsed_bits) == sizeof(elapsed_seconds),
                  "Armada float arguments must occupy four bytes");
    std::memcpy(&elapsed_bits, &elapsed_seconds, sizeof(elapsed_bits));
    a2fo_call_thiscall_1(
        g_craft_simulate_original, craft, elapsed_bits);
    dispatch_craft_event(
        A2FO_CRAFT_EVENT_SIMULATE_POST, craft, elapsed_seconds);
}

void __attribute__((fastcall)) craft_cleanup_event_hook(
    void* craft, void*) noexcept {
    dispatch_craft_event(A2FO_CRAFT_EVENT_CLEANUP, craft, 0.0f);
    a2fo_call_game_object_method(g_craft_cleanup_hook.gateway, craft);
}

std::uintptr_t __attribute__((fastcall)) craft_post_load_event_hook(
    void* craft, void*) noexcept {
    const std::uintptr_t loaded = reinterpret_cast<std::uintptr_t>(
        a2fo_call_game_object_method(g_craft_post_load_hook.gateway, craft));
    if ((loaded & 0xffu) != 0) {
        dispatch_craft_event(A2FO_CRAFT_EVENT_POST_LOAD, craft, 0.0f);
    }
    return loaded;
}

struct Matrix34Snapshot {
    std::array<float, 12> values{};
};

struct DestroyedObjectSnapshot {
    bool valid = false;
    std::string source_odf;
    Matrix34Snapshot transform;
    int source_team = 0;
    std::uint32_t source_handle = 0;
    OdfSnapshot odf;
    std::uint32_t random_seed = 0;
};

std::uint32_t hash_destroyed_object(const std::string& odf,std::uint32_t handle,int team,const Matrix34Snapshot& transform) {
    static_assert(sizeof(team)==sizeof(std::int32_t));
    return a2fo_rs_stage8_core_object_hash(reinterpret_cast<const std::uint8_t*>(odf.data()),odf.size(),handle,team,reinterpret_cast<const std::uint8_t*>(transform.values.data()),transform.values.size()*sizeof(float));
}

DestroyedObjectSnapshot snapshot_destroyed_object(void* self) {
    DestroyedObjectSnapshot snapshot;
    if (!self) {
        log_line("Object-destroyed snapshot unavailable: null Craft pointer");
        return snapshot;
    }

    auto* bytes = static_cast<std::uint8_t*>(self);
    if (bytes[0x113] != 0) {
        log_line("Object-destroyed snapshot unavailable: Craft explosion "
                 "state was already " + std::to_string(bytes[0x113]));
        return snapshot;
    }
    void* object_class = *reinterpret_cast<void**>(bytes + 0x40);
    if (!object_class) {
        log_line("Object-destroyed snapshot unavailable: Craft class was null");
        return snapshot;
    }

    const char* odf_name = static_cast<const char*>(
        a2fo_call_game_object_method(
            at(g_armada, kGameObjectClassGetOdfNameRva), object_class));
    if (!odf_name) {
        log_line("Object-destroyed snapshot unavailable: class ODF name was "
                 "null");
        return snapshot;
    }
    const char* odf_end = static_cast<const char*>(
        std::memchr(odf_name, '\0', 256));
    if (!odf_end) {
        log_line("Object-destroyed snapshot unavailable: class ODF name was "
                 "not terminated");
        return snapshot;
    }
    snapshot.source_odf.assign(
        odf_name, static_cast<std::size_t>(odf_end - odf_name));
    const std::string normalized_odf =
        normalize_odf_basename(snapshot.source_odf);
    if (normalized_odf.empty()) {
        log_line("Object-destroyed snapshot unavailable: class ODF name was "
                 "empty");
        return snapshot;
    }

    const void* transform = a2fo_call_game_object_method(
        at(g_armada, kGameObjectGetTransformRva), self);
    if (!transform) {
        log_line("Object-destroyed snapshot unavailable: " +
                 snapshot.source_odf + " had no transform");
        return snapshot;
    }
    std::memcpy(snapshot.transform.values.data(), transform,
                snapshot.transform.values.size() * sizeof(float));
    snapshot.source_team = *reinterpret_cast<const int*>(bytes + 0xec);
    snapshot.source_handle =
        *reinterpret_cast<const std::uint32_t*>(bytes + 0x28);

    snapshot.odf["basename"] = snapshot.source_odf;
    {
        StateLockGuard lock;
        const auto cached = g_odf_snapshots.find(normalized_odf);
        if (cached != g_odf_snapshots.end()) {
            snapshot.odf = cached->second;
        }
    }
    snapshot.random_seed = hash_destroyed_object(
        normalized_odf, snapshot.source_handle, snapshot.source_team,
        snapshot.transform);
    snapshot.valid = true;
    log_line("Object-destroyed snapshot ready: " + snapshot.source_odf +
             " (" + std::to_string(snapshot.odf.size()) +
             " ODF field" +
             (snapshot.odf.size() == 1 ? "" : "s") + ")");
    return snapshot;
}

bool resolve_native_object_destroyed(const DestroyedObjectSnapshot& source,ObjectReplacement& selected_replacement) {
    struct Context {
        const DestroyedObjectSnapshot& source;ObjectReplacement& selected;
        a2fo::RustHandlerRegistry<NativeObjectDestroyedRegistration> handlers{};
        std::vector<A2FO_OdfFieldView> fields{};
        A2FO_ObjectDestroyedEvent event{};A2FO_ObjectReplacement replacement{};
        std::optional<a2fo::RustHandlerRegistry<NativeObjectDestroyedRegistration>::Borrowed> current{};
        const char* odf_end=nullptr;std::size_t odf_length=0;std::exception_ptr error{};
    } context{source,selected_replacement};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:{StateLockGuard lock;c.handlers=g_object_destroyed_handlers;return c.handlers.size();}
        case 1:
            // Native SDK marshalling retains native strings, iteration order,
            // float bit copies and the lifetime of every borrowed field view.
            c.fields.reserve(c.source.odf.size());
            for(const auto& field:c.source.odf)c.fields.push_back({
                {field.first.data(),static_cast<std::uint32_t>(field.first.size())},
                {field.second.data(),static_cast<std::uint32_t>(field.second.size())}});
            c.event.struct_size=sizeof(c.event);
            c.event.source_odf={c.source.source_odf.data(),static_cast<std::uint32_t>(c.source.source_odf.size())};
            c.event.odf_fields=c.fields.empty()?nullptr:c.fields.data();
            c.event.odf_field_count=static_cast<std::uint32_t>(c.fields.size());
            std::copy(c.source.transform.values.begin(),c.source.transform.values.end(),c.event.transform);
            c.event.source_team=c.source.source_team;c.event.source_handle=c.source.source_handle;c.event.random_seed=c.source.random_seed;
            return 0;
        case 2:return c.handlers.size();
        case 3:c.current=c.handlers.borrow(a);return c.current->enabled;
        case 4:return c.current->handler!=nullptr;
        case 5:c.replacement={};c.replacement.struct_size=sizeof(c.replacement);
            c.replacement.flags=A2FO_REPLACEMENT_INHERIT_POSITION|A2FO_REPLACEMENT_INHERIT_ROTATION;
            c.replacement.owner=A2FO_REPLACEMENT_OWNER_NEUTRAL;return 0;
        case 6:
            // Preserve the original native catch region and active exception
            // while logging and updating the current global registration.
            try{return c.current->handler(&c.event,&c.replacement,c.current->user_data)?1:0;}
            catch(...){
                log_line("Native object-destroyed handler threw and was disabled: "+c.current->owner);
                StateLockGuard lock;
                if(a<g_object_destroyed_handlers.size())g_object_destroyed_handlers.set_enabled(a,false);
                return 2;
            }
        case 7:c.odf_end=c.replacement.odf?static_cast<const char*>(std::memchr(c.replacement.odf,'\0',256)):nullptr;
            c.odf_length=c.odf_end?static_cast<std::size_t>(c.odf_end-c.replacement.odf):0;return 0;
        case 8:return c.replacement.struct_size;
        case 9:return c.odf_end!=nullptr;
        case 10:return valid_replacement_odf_name(c.replacement.odf,c.odf_length);
        case 11:return c.replacement.flags;
        case 12:return c.replacement.owner;
        case 13:log_line("Native object-destroyed handler returned an invalid replacement: "+c.current->owner);return 0;
        case 14:c.selected.odf.assign(c.replacement.odf,c.odf_length);return 0;
        case 16:c.selected.inherit_position=a!=0;return 0;
        case 17:c.selected.inherit_rotation=a!=0;return 0;
        case 18:c.selected.owner=a?ReplacementOwner::original:ReplacementOwner::neutral;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const A2foRsReplacementRules rules{sizeof(A2FO_ObjectReplacement),A2FO_REPLACEMENT_INHERIT_POSITION|A2FO_REPLACEMENT_INHERIT_ROTATION,A2FO_REPLACEMENT_INHERIT_POSITION,A2FO_REPLACEMENT_INHERIT_ROTATION,A2FO_REPLACEMENT_OWNER_ORIGINAL,A2FO_REPLACEMENT_OWNER_ORIGINAL};
    const auto result=a2fo_rs_stage8_core_resolve_destroyed(&rules,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result==1;
}

void spawn_object_replacement(const DestroyedObjectSnapshot& source,const ObjectReplacement& replacement) {
    struct Context {const DestroyedObjectSnapshot& source;const ObjectReplacement& replacement;
        void* object_class=nullptr;Matrix34Snapshot transform{};void* object=nullptr;void* mission=nullptr;void** vtable=nullptr;
        std::exception_ptr error{};
    } context{source,replacement};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:{using Fn=void*(A2FO_CALL*)(const char*);c.object_class=reinterpret_cast<Fn>(at(g_armada,kGameObjectClassFindRva))(c.replacement.odf.c_str());return c.object_class!=nullptr;}
        case 1:log_line("Object replacement ODF was not loaded: "+c.replacement.odf);return 0;
        case 2:c.transform.values[0]=1.0f;c.transform.values[4]=1.0f;c.transform.values[8]=1.0f;return 0;
        case 3:return c.replacement.inherit_rotation;
        case 4:std::copy_n(c.source.transform.values.begin(),9,c.transform.values.begin());return 0;
        case 5:return c.replacement.inherit_position;
        case 6:std::copy_n(c.source.transform.values.begin()+9,3,c.transform.values.begin()+9);return 0;
        case 7:return c.replacement.owner==ReplacementOwner::original;
        case 8:{const int team=a?c.source.source_team:0;c.object=a2fo_call_game_object_construct(at(g_armada,kGameObjectClassConstructRva),c.object_class,&c.transform,team,0,nullptr);return c.object!=nullptr;}
        case 9:log_line("Object replacement could not construct: "+c.replacement.odf);return 0;
        case 10:{using Fn=void*(A2FO_CALL*)();c.mission=reinterpret_cast<Fn>(at(g_armada,kAiMissionGetCurrentRva))();return c.mission!=nullptr;}
        case 11:log_line("Object replacement was constructed without an active mission: "+c.replacement.odf);return 0;
        case 12:c.vtable=*reinterpret_cast<void***>(c.mission);return c.vtable!=nullptr;
        case 13:return c.vtable[6]!=nullptr;
        case 14:log_line("Object replacement could not reach AiMission::AddObject: "+c.replacement.odf);return 0;
        case 15:a2fo_call_game_object_argument(c.vtable[6],c.mission,c.object);return 0;
        case 16:log_line("Object replacement: "+c.source.source_odf+" -> "+c.replacement.odf);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_core_spawn_replacement(step,&context);
    if(context.error)std::rethrow_exception(context.error);
}

void __attribute__((fastcall)) craft_explode_hook(void* self, void*) {
    wait_for_deferred_initialization();
    DestroyedObjectSnapshot source;
    try {
        source = snapshot_destroyed_object(self);
    } catch (...) {
        log_line("Object-destroyed snapshot skipped after an unexpected "
                 "C++ exception");
    }

    a2fo_call_craft_explode(g_craft_explode_hook.gateway, self);
    if (!source.valid) return;

    try {
        ObjectReplacement replacement;
        const bool replace = resolve_native_object_destroyed(
            source, replacement);
        if (replace) {
            log_line("Object-destroyed event claimed: " +
                     source.source_odf + " -> " + replacement.odf);
            spawn_object_replacement(source, replacement);
        } else if (source.odf.size() > 1) {
            log_line("Object-destroyed event was not claimed for configured "
                     "ODF: " + source.source_odf);
        }
    } catch (...) {
        log_line("Object-destroyed callback skipped after an unexpected "
                 "C++ exception");
    }
}

void* existing_entry_detour(void* site, std::size_t* patch_length) noexcept {
    if (patch_length) *patch_length = 0;
    if (!site) return nullptr;
    const auto* bytes = static_cast<const std::uint8_t*>(site);
    if (bytes[0] == 0xe9) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, bytes + 1, sizeof(displacement));
        if (patch_length) *patch_length = 5;
        return const_cast<std::uint8_t*>(bytes + 5 + displacement);
    }
    if (bytes[0] == 0x68 && bytes[5] == 0xc3) {
        std::uint32_t destination = 0;
        std::memcpy(&destination, bytes + 1, sizeof(destination));
        if (patch_length) *patch_length = 6;
        return reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(destination));
    }
    return nullptr;
}

bool install_weapon_class_loaded_hook() {
    void* site = at(g_armada, kWeaponClassConstructorRva);
    if (std::memcmp(site, kExpectedWeaponClassConstructor,
                    sizeof(kExpectedWeaponClassConstructor)) == 0) {
        if (!a2fo_rs_windows_hook_install(
                site, reinterpret_cast<void*>(&weapon_class_constructor_hook),
                sizeof(kExpectedWeaponClassConstructor),
                kExpectedWeaponClassConstructor, &(g_weapon_class_constructor_hook))) {
            log_line("Could not install WeaponClass-loaded dispatcher");
            return false;
        }
        g_weapon_class_constructor_original =
            g_weapon_class_constructor_hook.gateway;
    } else {
        std::size_t patch_length = 0;
        void* destination = existing_entry_detour(site, &patch_length);
        void* expected = at(
            g_fleet_ops, kFoWeaponClassConstructorHandlerRva);
        if (destination != expected || patch_length > 6 ||
            std::memcmp(expected, kExpectedFoWeaponClassConstructorHandler,
                        sizeof(kExpectedFoWeaponClassConstructorHandler)) != 0) {
            log_line("WeaponClass-loaded hook signature mismatch; weapon ODF "
                     "dispatch disabled");
            return false;
        }
        std::uint8_t expected_detour[6]{};
        std::memcpy(expected_detour, site, patch_length);
        if (!a2fo_rs_windows_patch_jump(
                site, reinterpret_cast<void*>(&weapon_class_constructor_hook),
                expected_detour, patch_length)) {
            log_line("Could not chain WeaponClass-loaded dispatcher");
            return false;
        }
        g_weapon_class_constructor_original = destination;
    }
    log_line("WeaponClass-loaded ODF dispatcher enabled");
    return g_weapon_class_constructor_original != nullptr;
}

bool install_weapon_trigger_hook() {
    if (!g_armada ||
        std::memcmp(at(g_armada, kWeaponTriggerObjectRva),
                    kExpectedWeaponTriggerObject,
                    sizeof(kExpectedWeaponTriggerObject)) != 0) {
        log_line("Weapon-trigger hook signature mismatch; shared trigger "
                 "dispatch disabled");
        return false;
    }
    if (!a2fo_rs_windows_hook_install(
            at(g_armada, kWeaponTriggerObjectRva),
            reinterpret_cast<void*>(&weapon_trigger_object_hook),
            sizeof(kExpectedWeaponTriggerObject),
            kExpectedWeaponTriggerObject, &(g_weapon_trigger_object_hook))) {
        log_line("Could not install shared weapon-trigger dispatcher");
        return false;
    }
    log_line("Shared weapon-trigger dispatcher enabled");
    return true;
}

bool install_craft_event_hooks() {
    void* simulate_site = at(g_armada, kCraftSimulateRva);
    const bool stock_simulate =
        std::memcmp(simulate_site, kExpectedCraftSimulate,
                    sizeof(kExpectedCraftSimulate)) == 0;
    std::size_t simulate_patch_length = 0;
    void* simulate_destination = stock_simulate
        ? nullptr
        : existing_entry_detour(simulate_site, &simulate_patch_length);
    void* expected_fo_simulate = at(
        g_fleet_ops, kFoCraftSimulateHandlerRva);
    const bool fleet_ops_simulate = !stock_simulate &&
        simulate_destination == expected_fo_simulate &&
        simulate_patch_length <= 6 &&
        std::memcmp(expected_fo_simulate, kExpectedFoCraftSimulateHandler,
                    sizeof(kExpectedFoCraftSimulateHandler)) == 0;
    if ((!stock_simulate && !fleet_ops_simulate) ||
        std::memcmp(at(g_armada, kCraftCleanupRva), kExpectedCraftCleanup,
                    sizeof(kExpectedCraftCleanup)) != 0 ||
        std::memcmp(at(g_armada, kCraftPostLoadRva), kExpectedCraftPostLoad,
                    sizeof(kExpectedCraftPostLoad)) != 0) {
        log_line("Craft-event hook signature mismatch; shared Craft dispatch "
                 "disabled");
        return false;
    }

    if (stock_simulate) {
        if (!a2fo_rs_windows_hook_install(
                simulate_site,
                reinterpret_cast<void*>(&craft_simulate_event_hook),
                sizeof(kExpectedCraftSimulate), kExpectedCraftSimulate, &(g_craft_simulate_hook))) {
            return false;
        }
        g_craft_simulate_original = g_craft_simulate_hook.gateway;
    } else {
        std::uint8_t expected_detour[6]{};
        std::memcpy(expected_detour, simulate_site, simulate_patch_length);
        if (!a2fo_rs_windows_patch_jump(
                simulate_site,
                reinterpret_cast<void*>(&craft_simulate_event_hook),
                expected_detour, simulate_patch_length)) {
            return false;
        }
        g_craft_simulate_original = simulate_destination;
    }
    const bool installed = a2fo_rs_windows_hook_install(
            at(g_armada, kCraftCleanupRva),
            reinterpret_cast<void*>(&craft_cleanup_event_hook),
            sizeof(kExpectedCraftCleanup), kExpectedCraftCleanup, &(g_craft_cleanup_hook)) &&
        a2fo_rs_windows_hook_install(
            at(g_armada, kCraftPostLoadRva),
            reinterpret_cast<void*>(&craft_post_load_event_hook),
            sizeof(kExpectedCraftPostLoad), kExpectedCraftPostLoad, &(g_craft_post_load_hook));
    if (!installed) {
        log_line("Could not install every shared Craft-event hook");
        return false;
    }
    log_line("Shared Craft simulation, cleanup, and post-load dispatcher enabled");
    return g_craft_simulate_original != nullptr;
}

bool install_game_object_class_loaded_hook() {
    if (!g_armada ||
        std::memcmp(at(g_armada, kGameObjectClassFindProjectIdRva),
                    kExpectedGameObjectClassFindProjectId,
                    sizeof(kExpectedGameObjectClassFindProjectId)) != 0 ||
        std::memcmp(at(g_armada, kGameObjectClassGetOdfNameRva),
                    kExpectedGameObjectClassGetOdfName,
                    sizeof(kExpectedGameObjectClassGetOdfName)) != 0) {
        log_line("GameObjectClass-loaded hook signature mismatch; class ODF "
                 "dispatch disabled");
        return false;
    }
    if (!a2fo_rs_windows_hook_install(
            at(g_armada, kGameObjectClassFindProjectIdRva),
            reinterpret_cast<void*>(&game_object_class_find_project_id_hook),
            sizeof(kExpectedGameObjectClassFindProjectId),
            kExpectedGameObjectClassFindProjectId, &(g_game_object_class_find_project_id_hook))) {
        log_line("Could not install GameObjectClass ODF field capture hook");
        return false;
    }
    log_line("GameObjectClass-loaded ODF dispatcher enabled");
    return true;
}

bool install_object_destroyed_hook() {
    if (!g_armada ||
        std::memcmp(at(g_armada, kAiMissionGetCurrentRva),
                    kExpectedAiMissionGetCurrent,
                    sizeof(kExpectedAiMissionGetCurrent)) != 0 ||
        std::memcmp(at(g_armada, kCraftExplodeRva),
                    kExpectedCraftExplode,
                    sizeof(kExpectedCraftExplode)) != 0 ||
        std::memcmp(at(g_armada, kGameObjectClassFindRva),
                    kExpectedGameObjectClassFind,
                    sizeof(kExpectedGameObjectClassFind)) != 0 ||
        std::memcmp(at(g_armada, kGameObjectClassConstructRva),
                    kExpectedGameObjectClassConstruct,
                    sizeof(kExpectedGameObjectClassConstruct)) != 0 ||
        std::memcmp(at(g_armada, kGameObjectClassGetOdfNameRva),
                    kExpectedGameObjectClassGetOdfName,
                    sizeof(kExpectedGameObjectClassGetOdfName)) != 0 ||
        std::memcmp(at(g_armada, kGameObjectGetTransformRva),
                    kExpectedGameObjectGetTransform,
                    sizeof(kExpectedGameObjectGetTransform)) != 0) {
        log_line("Object-destroyed hook signature mismatch; object "
                 "replacement dispatch disabled");
        return false;
    }
    if (!a2fo_rs_windows_hook_install(
            at(g_armada, kCraftExplodeRva),
            reinterpret_cast<void*>(&craft_explode_hook),
            sizeof(kExpectedCraftExplode), kExpectedCraftExplode, &(g_craft_explode_hook))) {
        log_line("Could not install object-destroyed hook");
        return false;
    }
    log_line("Object-destroyed dispatcher enabled");
    return true;
}

bool install_race_loaded_hook() {
    if (!g_fleet_ops ||
        std::memcmp(at(g_fleet_ops, kRaceParameterDbDestructorCallRva),
                    kExpectedRaceParameterDbDestructorCall,
                    sizeof(kExpectedRaceParameterDbDestructorCall)) != 0 ||
        std::memcmp(at(g_fleet_ops, kParameterDbDestructorRva),
                    kExpectedParameterDbDestructor,
                    sizeof(kExpectedParameterDbDestructor)) != 0) {
        log_line("Race-loaded hook signature mismatch; Race ODF dispatch "
                 "disabled");
        return false;
    }
    g_a2fo_race_parameter_db_destructor =
        at(g_fleet_ops, kParameterDbDestructorRva);
    if (!a2fo_rs_windows_patch_call(
            at(g_fleet_ops, kRaceParameterDbDestructorCallRva),
            reinterpret_cast<void*>(&a2fo_race_parameter_db_dispatch_bridge),
            kExpectedRaceParameterDbDestructorCall,
            sizeof(kExpectedRaceParameterDbDestructorCall))) {
        g_a2fo_race_parameter_db_destructor = nullptr;
        log_line("Could not install Race-loaded ODF dispatcher");
        return false;
    }
    log_line("Race-loaded ODF dispatcher enabled");
    return true;
}

bool associate_evolver_cocoon_class(void* class_object,
                                    void* parameter_db,
                                    const char* class_kind);

void* __attribute__((fastcall)) evolver_class_build_class_hook(
    void* self, void*, void* parameter_db) {
    // Let Armada construct the complete class first, then associate the final
    // class object returned in EAX with its optional cocoon setting.
    void* result = a2fo_call_evolver_build_class(g_build_class_hook.gateway,
                                                  self, parameter_db);
    wait_for_deferred_initialization();
    associate_evolver_cocoon_class(result, parameter_db, "EvolverClass");
    return result;
}

bool associate_evolver_cocoon_class(void* class_object,
                                    void* parameter_db,
                                    const char* class_kind) {
    if (!class_object || !parameter_db || !g_state_lock_ready) return false;

    char basename[MAX_PATH]{};
    std::string value;
    std::string command;
    try {
        StateLockGuard lock;
        command = g_evolver_cocoon_command;
    } catch (...) {
        log_line("Evolver cocoon policy lookup failed; using Fleet Ops default");
    }

    if (parameter_db && !command.empty()) {
        char native_value[MAX_PATH]{};
        a2fo_parameter_db_get_string(original_parameter_db_get_string(),
                                     parameter_db, command.c_str(), native_value,
                                     sizeof(native_value), "");
        value = native_value;
    }
    if (parameter_db) {
        a2fo_parameter_db_get_string(original_parameter_db_get_string(),
                                     parameter_db, "basename", basename,
                                     sizeof(basename), "<unnamed>");
    }
    const std::string cocoon = normalize_cocoon_name(value.c_str());
    try {
        StateLockGuard lock;
        if (cocoon.empty()) {
            g_class_cocoons.erase(class_object);
        } else {
            g_class_cocoons[class_object] = cocoon;
        }
    } catch (...) {
        log_line("Cocoon class association could not retain its policy; "
                 "using Fleet Ops default");
        return false;
    }
    log_line(std::string(class_kind ? class_kind : "EvolverClass") + " " +
             std::string(basename) + " cocoon: " +
             (cocoon.empty() ? "<Fleet Ops default>" : cocoon));
    return true;
}

void* __attribute__((fastcall)) evolver_class_dtor_hook(void* self, void*) {
    // Class pointers can be reused after destruction; erase all pointer-keyed
    // state so a later class cannot inherit an old cocoon choice.
    EnterCriticalSection(&g_state_lock);
    g_class_cocoons.erase(self);
    g_logged_cocoon_classes.erase(self);
    LeaveCriticalSection(&g_state_lock);
    return a2fo_call_evolver_dtor(g_dtor_hook.gateway, self);
}

bool install_classlabel_alias_hook() {
    if (!g_armada) {
        log_line("ArmadaL.exe is unavailable; ParameterDB policies "
                 "disabled");
        return false;
    }

    if (std::memcmp(at(g_armada, kParameterDbGetIntRva),
                    kExpectedParameterDbGetScalar,
                    sizeof(kExpectedParameterDbGetScalar)) != 0 ||
        std::memcmp(at(g_armada, kParameterDbGetIntAlternativeRva),
                    kExpectedParameterDbGetScalar,
                    sizeof(kExpectedParameterDbGetScalar)) != 0 ||
        std::memcmp(at(g_armada, kParameterDbGetFloatRva),
                    kExpectedParameterDbGetScalar,
                    sizeof(kExpectedParameterDbGetScalar)) != 0 ||
        std::memcmp(at(g_armada, kParameterDbGetBoolRva),
                    kExpectedParameterDbGetScalar,
                    sizeof(kExpectedParameterDbGetScalar)) != 0 ||
        std::memcmp(at(g_armada, kParameterDbGetOwnedStringRva),
                    kExpectedParameterDbGetScalar,
                    sizeof(kExpectedParameterDbGetScalar)) != 0 ||
        std::memcmp(at(g_armada, kParameterDbGetStringVectorRva),
                    kExpectedParameterDbGetScalar,
                    sizeof(kExpectedParameterDbGetScalar)) != 0 ||
        std::memcmp(at(g_armada, kParameterDbGetStringRva),
                    kExpectedParameterDbGetString,
                    sizeof(kExpectedParameterDbGetString)) != 0) {

        log_line(
            "ParameterDB getter signature mismatch; policies disabled");
        return false;
    }

    // Install the pass-through typed hooks first. No module can register a
    // default policy until the complete suite is ready, so a partial install
    // remains behaviorally native if a later detour unexpectedly fails.
    if (!a2fo_rs_windows_hook_install(
            at(g_armada, kParameterDbGetIntRva),
            reinterpret_cast<void*>(&parameter_db_get_int_hook),
            sizeof(kExpectedParameterDbGetScalar),
            kExpectedParameterDbGetScalar, &(g_parameter_db_get_int_hook)) ||
        !a2fo_rs_windows_hook_install(
            at(g_armada, kParameterDbGetIntAlternativeRva),
            reinterpret_cast<void*>(&parameter_db_get_int_alternative_hook),
            sizeof(kExpectedParameterDbGetScalar),
            kExpectedParameterDbGetScalar, &(g_parameter_db_get_int_alternative_hook)) ||
        !a2fo_rs_windows_hook_install(
            at(g_armada, kParameterDbGetFloatRva),
            reinterpret_cast<void*>(&parameter_db_get_float_hook),
            sizeof(kExpectedParameterDbGetScalar),
            kExpectedParameterDbGetScalar, &(g_parameter_db_get_float_hook)) ||
        !a2fo_rs_windows_hook_install(
            at(g_armada, kParameterDbGetBoolRva),
            reinterpret_cast<void*>(&parameter_db_get_bool_hook),
            sizeof(kExpectedParameterDbGetScalar),
            kExpectedParameterDbGetScalar, &(g_parameter_db_get_bool_hook)) ||
        !a2fo_rs_windows_hook_install(
            at(g_armada, kParameterDbGetOwnedStringRva),
            reinterpret_cast<void*>(&parameter_db_get_owned_string_hook),
            sizeof(kExpectedParameterDbGetScalar),
            kExpectedParameterDbGetScalar, &(g_parameter_db_get_owned_string_hook)) ||
        !a2fo_rs_windows_hook_install(
            at(g_armada, kParameterDbGetStringVectorRva),
            reinterpret_cast<void*>(&parameter_db_get_string_vector_hook),
            sizeof(kExpectedParameterDbGetScalar),
            kExpectedParameterDbGetScalar, &(g_parameter_db_get_string_vector_hook))) {
        log_line("Could not install ParameterDB typed-default hooks");
        return false;
    }

    if (!a2fo_rs_windows_hook_install(
            at(g_armada, kParameterDbGetStringRva),
            reinterpret_cast<void*>(&parameter_db_get_string_hook),
            sizeof(kExpectedParameterDbGetString),
            kExpectedParameterDbGetString, &(g_parameter_db_get_string_hook))) {

        log_line("Could not install ParameterDB GetString hook");
        return false;
    }

    log_line("ParameterDB classlabel and typed-default hooks enabled");
    return true;
}

bool install_evolver_hooks() {
    if (std::memcmp(at(g_armada, kEvolverClassBuildClassRva),
                    kExpectedBuildClass, sizeof(kExpectedBuildClass)) != 0 ||
        std::memcmp(at(g_armada, kEvolverClassDtorRva), kExpectedDtor,
                    sizeof(kExpectedDtor)) != 0 ||
        std::memcmp(at(g_armada, kCocoonSelectorRva), kExpectedCocoonJump,
                    sizeof(kExpectedCocoonJump)) != 0 ||
        std::memcmp(at(g_armada, kCocoonUpdateSelectorRva),
                    kExpectedCocoonUpdateSelector,
                    sizeof(kExpectedCocoonUpdateSelector)) != 0) {
        log_line("Evolver hook signature mismatch; no evolver hooks installed");
        return false;
    }
    if (!a2fo_rs_windows_hook_install(
            at(g_armada, kEvolverClassBuildClassRva),
            reinterpret_cast<void*>(&evolver_class_build_class_hook),
            sizeof(kExpectedBuildClass), kExpectedBuildClass, &(g_build_class_hook))) {
        log_line("EvolverClass BuildClass hook signature mismatch");
        return false;
    }
    if (!a2fo_rs_windows_hook_install(at(g_armada, kEvolverClassDtorRva),
                                   reinterpret_cast<void*>(&evolver_class_dtor_hook),
                                   sizeof(kExpectedDtor), kExpectedDtor, &(g_dtor_hook))) {
        log_line("EvolverClass destructor hook signature mismatch");
        return false;
    }
    if (!a2fo_rs_windows_patch_jump(at(g_armada, kCocoonSelectorRva),
                          reinterpret_cast<void*>(&a2fo_cocoon_selector_hook),
                          kExpectedCocoonJump, sizeof(kExpectedCocoonJump))) {
        log_line("Cocoon selector hook signature mismatch");
        return false;
    }
    if (!a2fo_rs_windows_patch_jump(
            at(g_armada, kCocoonUpdateSelectorRva),
            reinterpret_cast<void*>(&a2fo_cocoon_update_selector_hook),
            kExpectedCocoonUpdateSelector,
            sizeof(kExpectedCocoonUpdateSelector))) {
        log_line("Cocoon update selector hook signature mismatch");
        return false;
    }
    log_line("Evolver policy hooks enabled");
    return true;
}

bool install_fofs_item_get_lookup_hook() {
    if (!g_fleet_ops ||
        std::memcmp(at(g_fleet_ops, kFofsItemGetHashLookupCallRva),
                    kExpectedFofsItemGetHashLookupCall,
                    sizeof(kExpectedFofsItemGetHashLookupCall)) != 0 ||
        std::memcmp(at(g_fleet_ops, kFofsItemLocateHashLookupCallRva),
                    kExpectedFofsItemLocateHashLookupCall,
                    sizeof(kExpectedFofsItemLocateHashLookupCall)) != 0 ||
        std::memcmp(at(g_fleet_ops, kFofsItemExistsHashLookupCallRva),
                    kExpectedFofsItemExistsHashLookupCall,
                    sizeof(kExpectedFofsItemExistsHashLookupCall)) != 0 ||
        std::memcmp(at(g_fleet_ops, kFofsProjectIdHashLookupCallRva),
                    kExpectedFofsProjectIdHashLookupCall,
                    sizeof(kExpectedFofsProjectIdHashLookupCall)) != 0) {
        log_line("Fleet Operations ODF lookup call signature mismatch");
        return false;
    }
    if (!a2fo_rs_windows_patch_call(
            at(g_fleet_ops, kFofsItemGetHashLookupCallRva),
            reinterpret_cast<void*>(&fofs_item_get_hash_lookup_hook),
            kExpectedFofsItemGetHashLookupCall,
            sizeof(kExpectedFofsItemGetHashLookupCall)) ||
        !a2fo_rs_windows_patch_call(
            at(g_fleet_ops, kFofsItemLocateHashLookupCallRva),
            reinterpret_cast<void*>(&fofs_item_get_hash_lookup_hook),
            kExpectedFofsItemLocateHashLookupCall,
            sizeof(kExpectedFofsItemLocateHashLookupCall)) ||
        !a2fo_rs_windows_patch_call(
            at(g_fleet_ops, kFofsItemExistsHashLookupCallRva),
            reinterpret_cast<void*>(&fofs_item_get_hash_lookup_hook),
            kExpectedFofsItemExistsHashLookupCall,
            sizeof(kExpectedFofsItemExistsHashLookupCall)) ||
        !a2fo_rs_windows_patch_call(
            at(g_fleet_ops, kFofsProjectIdHashLookupCallRva),
            reinterpret_cast<void*>(&fofs_item_get_hash_lookup_hook),
            kExpectedFofsProjectIdHashLookupCall,
            sizeof(kExpectedFofsProjectIdHashLookupCall))) {
        log_line("Could not install Fleet Operations ODF lookup hooks");
        return false;
    }
    log_line("FOFS item lookup dispatcher enabled for get, locate, exists, "
             "and project IDs");
    return true;
}

bool install_mod_user_directory_hook() {
    if (!g_fleet_ops ||
        std::memcmp(at(g_fleet_ops, kGetModUserDirectoryRva),
                    kExpectedGetModUserDirectory,
                    sizeof(kExpectedGetModUserDirectory)) != 0) {
        log_line("Fleet Operations mod user-directory signature mismatch; "
                 "SettingsDirectory disabled");
        return false;
    }
    if (!a2fo_rs_windows_hook_install(
            at(g_fleet_ops, kGetModUserDirectoryRva),
            reinterpret_cast<void*>(&mod_user_directory_hook),
            sizeof(kExpectedGetModUserDirectory),
            kExpectedGetModUserDirectory, &(g_mod_user_directory_hook))) {
        log_line("Could not install Fleet Operations SettingsDirectory hook");
        return false;
    }
    log_line("Fleet Operations info.ini SettingsDirectory hook enabled");
    return true;
}

bool install_fo_settings_default_hook() {
    if (!g_fleet_ops ||
        std::memcmp(at(g_fleet_ops, kFoSettingsGetInstanceRva),
                    kExpectedFoSettingsGetInstance,
                    sizeof(kExpectedFoSettingsGetInstance)) != 0) {
        log_line("Fleet Operations settings-singleton signature mismatch; "
                 "Settings.xml DefaultGameSpeed disabled");
        return false;
    }
    if (!a2fo_rs_windows_hook_install(
            at(g_fleet_ops, kFoSettingsGetInstanceRva),
            reinterpret_cast<void*>(&fo_settings_get_instance_hook),
            sizeof(kExpectedFoSettingsGetInstance),
            kExpectedFoSettingsGetInstance, &(g_fo_settings_get_instance_hook))) {
        log_line("Could not install Fleet Operations Settings.xml default "
                 "hook");
        return false;
    }
    log_line("Fleet Operations first-run Settings.xml default hook enabled");
    return true;
}

bool install_default_game_speed_hook() {
    if (!g_fleet_ops ||
        std::memcmp(at(g_fleet_ops, kGameConfigurationNewRva),
                    kExpectedGameConfigurationNew,
                    sizeof(kExpectedGameConfigurationNew)) != 0) {
        log_line("Fleet Operations game-configuration signature mismatch; "
                 "DefaultGameSpeed disabled");
        return false;
    }
    if (!a2fo_rs_windows_hook_install(
            at(g_fleet_ops, kGameConfigurationNewRva),
            reinterpret_cast<void*>(&game_configuration_new_hook),
            sizeof(kExpectedGameConfigurationNew),
            kExpectedGameConfigurationNew, &(g_game_configuration_new_hook))) {
        log_line("Could not install Fleet Operations DefaultGameSpeed hook");
        return false;
    }
    log_line("Fleet Operations info.ini DefaultGameSpeed hook enabled");
    return true;
}

bool install_runtime_game_speed_hook() {
    if (!g_fleet_ops ||
        std::memcmp(at(g_fleet_ops, kGameConfigurationLoadProfileRva),
                    kExpectedGameConfigurationLoadProfile,
                    sizeof(kExpectedGameConfigurationLoadProfile)) != 0) {
        log_line("Fleet Operations profile-load signature mismatch; runtime "
                 "DefaultGameSpeed disabled");
        return false;
    }
    if (!a2fo_rs_windows_hook_install(
            at(g_fleet_ops, kGameConfigurationLoadProfileRva),
            reinterpret_cast<void*>(&game_configuration_load_profile_hook),
            sizeof(kExpectedGameConfigurationLoadProfile),
            kExpectedGameConfigurationLoadProfile, &(g_game_configuration_load_profile_hook))) {
        log_line("Could not install Fleet Operations runtime "
                 "DefaultGameSpeed hook");
        return false;
    }
    log_line("Fleet Operations runtime DefaultGameSpeed hook enabled");
    return true;
}

bool install_user_profile_game_speed_hook() {
    if (!g_armada ||
        std::memcmp(at(g_armada, kDefaultUserProfileGameSpeedRva),
                    kExpectedDefaultUserProfileGameSpeed,
                    sizeof(kExpectedDefaultUserProfileGameSpeed)) != 0) {
        log_line("Armada default user-profile game-speed signature mismatch; "
                 "DefaultGameSpeed supplier disabled");
        return false;
    }
    if (!a2fo_rs_windows_patch_jump(
            at(g_armada, kDefaultUserProfileGameSpeedRva),
            reinterpret_cast<void*>(&default_user_profile_game_speed_hook),
            kExpectedDefaultUserProfileGameSpeed,
            sizeof(kExpectedDefaultUserProfileGameSpeed))) {
        log_line("Could not install Armada default user-profile game-speed "
                 "hook");
        return false;
    }
    log_line("Armada default user-profile game-speed hook enabled");
    return true;
}


void A2FO_CALL module_log(const char* module_name, const char* message) {
    const std::string name = module_name && *module_name ? module_name : "module";
    const std::string text = message ? message : "";
    log_line("[" + name + "] " + text);
}

void* A2FO_CALL api_armada_module() {
    return g_armada;
}

void* A2FO_CALL api_fleetops_module() {
    return g_fleet_ops;
}

const char* A2FO_CALL api_root_directory() {
    return g_root_directory.c_str();
}

bool A2FO_CALL api_install_inline_hook(
    void* target, void* replacement, std::size_t length,
    const std::uint8_t* expected, A2FO_InlineHook* output) {
    if (!output) {
        return false;
    }
    A2foRsInlineHook hook{};
    if (!a2fo_rs_windows_hook_install(target, replacement, length, expected, &(hook))) {
        return false;
    }
    output->target = hook.target;
    output->gateway = hook.gateway;
    output->length = hook.length;
    return true;
}

bool A2FO_CALL api_patch_jump(void* target, void* replacement,
                              const std::uint8_t* expected,
                              std::size_t length) {
    return a2fo_rs_windows_patch_jump(target, replacement, expected, length);
}

bool A2FO_CALL api_patch_call(void* target, void* replacement,
                              const std::uint8_t* expected,
                              std::size_t length) {
    return a2fo_rs_windows_patch_call(target, replacement, expected, length);
}

bool A2FO_CALL api_patch_bytes(void* target,
                               const std::uint8_t* replacement,
                               const std::uint8_t* expected,
                               std::size_t length) {
    return a2fo_rs_windows_patch_bytes(target, replacement, expected, length);
}

bool A2FO_CALL api_register_fofs_item_lookup_handler(const char* module_name,A2FO_FofsItemLookupHandler handler,void* user_data) {
    struct Context {const char* name;A2FO_FofsItemLookupHandler handler;void* user_data;std::optional<std::string> owner{};std::optional<std::string> existing_owner{};std::exception_ptr error{};} context{module_name,handler,user_data};
    const auto step=+[](void* opaque,std::uint32_t op) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return c.handler!=nullptr;
        case 1:return g_state_lock_ready;
        case 2:c.owner.emplace(c.name);return 0;
        case 14:return c.name!=nullptr;
        case 15:c.owner.emplace("unnamed module");return 0;
        case 16:return *c.name!=0;
        case 3:EnterCriticalSection(&g_state_lock);return 0;
        case 4:return g_policy_registration_open;
        case 5:return g_registration_transaction.active;
        case 6:return g_fofs_item_lookup_handler==nullptr;
        case 7:g_fofs_item_lookup_handler=c.handler;return 0;
        case 8:g_fofs_item_lookup_user_data=c.user_data;return 0;
        case 9:g_fofs_item_lookup_owner=*c.owner;return 0;
        case 10:c.existing_owner.emplace(g_fofs_item_lookup_owner);return 0;
        case 11:LeaveCriticalSection(&g_state_lock);return 0;
        case 12:log_line("FOFS item lookup handler registered by "+*c.owner);return 0;
        case 13:log_line("FOFS item lookup handler rejected for "+*c.owner+"; already owned by "+*c.existing_owner);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_lookup(1,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result!=0;
}

bool A2FO_CALL api_register_info_ini_defaults_handler(const char* module_name,A2FO_InfoIniDefaultsHandler handler,void* user_data) {
    struct Context {const char* name;A2FO_InfoIniDefaultsHandler handler;void* user_data;std::optional<std::string> owner{};std::optional<std::string> existing_owner{};std::optional<StateLockGuard> lock{};std::exception_ptr error{};} context{module_name,handler,user_data};
    const auto step=+[](void* opaque,std::uint32_t op) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return c.handler!=nullptr;
        case 1:return g_state_lock_ready;
        case 2:c.owner.emplace(policy_owner(c.name));return 0;
        case 3:c.lock.emplace();return 0;
        case 4:return g_policy_registration_open;
        case 5:return g_registration_transaction.active;
        case 6:return g_info_ini_defaults_handler==nullptr;
        case 7:g_info_ini_defaults_handler=c.handler;return 0;
        case 8:g_info_ini_defaults_user_data=c.user_data;return 0;
        case 9:g_info_ini_defaults_owner=*c.owner;return 0;
        case 10:c.existing_owner.emplace(g_info_ini_defaults_owner);return 0;
        case 11:c.lock.reset();return 0;
        case 12:log_line("info.ini defaults handler registered by "+*c.owner);return 0;
        case 13:log_line("info.ini defaults handler rejected for "+*c.owner+"; already owned by "+*c.existing_owner);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_lookup(2,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result!=0;
}

bool A2FO_CALL api_register_classlabel_alias(
    const char* module_name, const char* source, const char* target) {
    try {
        return register_classlabel_alias_policy(module_name, source, target);
    } catch (...) {
        log_line("Classlabel alias policy registration failed after an "
                 "unexpected C++ exception");
        return false;
    }
}

bool A2FO_CALL api_register_classlabel_odf_defaults(
    const char* module_name, const char* classlabel,
    const A2FO_ClasslabelOdfDefault* defaults,
    std::uint32_t default_count) {
    if (!g_classlabel_alias_hook_ready) {
        log_line("Classlabel ODF defaults rejected; typed ParameterDB hooks "
                 "are unavailable");
        return false;
    }
    try {
        return register_classlabel_odf_defaults_policy(
            module_name, classlabel, defaults, default_count);
    } catch (...) {
        log_line("Classlabel ODF defaults registration failed after an "
                 "unexpected C++ exception");
        return false;
    }
}

bool A2FO_CALL api_register_race_odf_defaults(
    const char* module_name, const A2FO_RaceOdfDefault* defaults,
    std::uint32_t default_count) {
    try {
        return register_race_odf_defaults_policy(
            module_name, defaults, default_count);
    } catch (...) {
        log_line("Race ODF defaults registration failed after an unexpected "
                 "C++ exception");
        return false;
    }
}

bool A2FO_CALL api_register_evolver_cocoon_command(
    const char* module_name, const char* command) {
    try {
        return register_evolver_cocoon_policy(module_name, command);
    } catch (...) {
        log_line("Evolver cocoon policy registration failed after an "
                 "unexpected C++ exception");
        return false;
    }
}

bool A2FO_CALL api_register_odf_overlay_directory(
    const char* module_name, const char* relative_directory,
    std::uint32_t precedence) {
    try {
        return register_odf_overlay_directory_policy(
            module_name, relative_directory, precedence);
    } catch (...) {
        log_line("ODF overlay directory registration failed after an "
                 "unexpected C++ exception");
        return false;
    }
}

std::uint32_t A2FO_CALL api_odf_overlay_directory_count() {
    if (!g_state_lock_ready) return 0;
    StateLockGuard lock;
    return static_cast<std::uint32_t>(g_odf_overlay_directories.size());
}

bool A2FO_CALL api_get_odf_overlay_directory(
    std::uint32_t index, char* relative_directory,
    std::uint32_t relative_directory_size, std::uint32_t* precedence) {
    if (!relative_directory || relative_directory_size == 0 || !precedence ||
        !g_state_lock_ready) {
        return false;
    }
    relative_directory[0] = '\0';
    StateLockGuard lock;
    if (index >= g_odf_overlay_directories.size()) return false;
    const OdfOverlayDirectoryPolicy& policy =
        g_odf_overlay_directories[index];
    if (policy.directory.size() + 1 > relative_directory_size) return false;
    std::memcpy(relative_directory, policy.directory.c_str(),
                policy.directory.size() + 1);
    *precedence = policy.precedence;
    return true;
}

template<class Registration,class Handler>
bool core_register_field_handler(std::uint32_t kind,const std::string& owner,
    const char* const* fields,std::uint32_t count,Handler handler,void* user_data,
    a2fo::RustHandlerRegistry<Registration>& handlers,std::set<std::string>& captured_fields) {
    struct Context {
        std::uint32_t kind;const std::string& owner;const char* const* fields;Handler handler;void* user_data;
        a2fo::RustHandlerRegistry<Registration>& handlers;std::set<std::string>& captured_fields;
        std::optional<Registration> registration{};std::optional<std::set<std::string>> unique{};
        const char* field=nullptr;std::exception_ptr error{};std::optional<StateLockGuard> guard{};
    } context{kind,owner,fields,handler,user_data,handlers,captured_fields};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        constexpr const char* rejected[]={"Object-destroyed handler rejected for ","GameObjectClass-loaded handler rejected for ","Race-loaded handler rejected for ","WeaponClass-loaded handler rejected for "};
        constexpr const char* label[]={"Object-destroyed handler ","GameObjectClass-loaded handler ","Race-loaded handler ","WeaponClass-loaded handler "};
        try{switch(op){
        case 0:return c.handler!=nullptr;
        case 1:return c.fields!=nullptr;
        case 2:log_line(rejected[c.kind]+c.owner+"; invalid callback or field list");return 0;
        case 3:c.registration.emplace();c.registration->owner=c.owner;c.registration->handler=c.handler;c.registration->user_data=c.user_data;return 0;
        case 4:c.unique.emplace();return 0;
        case 5:c.field=c.fields[a];return valid_odf_field_name(c.field);
        case 6:log_line(rejected[c.kind]+c.owner+(c.kind==0?"; ODF fields must be 1-127 character identifiers":"; invalid ODF field name"));return 0;
        case 7:c.unique->insert(lower_ascii(c.field));return 0;
        case 8:c.registration->required_odf_fields.assign(c.unique->begin(),c.unique->end());c.unique.reset();return 0;
        case 9:c.unique.reset();c.error=nullptr;if(c.kind==0)log_line("Object-destroyed handler rejected for "+c.owner+"; could not copy registration data");return 0;
        case 10:c.guard.emplace();return 0;
        case 11:return g_policy_registration_open;
        case 12:return g_registration_transaction.active;
        case 13:return c.handlers.size();
        case 14:return c.registration->required_odf_fields.size();
        case 15:c.captured_fields.insert(c.registration->required_odf_fields[a]);return 0;
        case 16:c.handlers.push_back(std::move(*c.registration));return 0;
        case 17:c.guard.reset();return 0;
        case 18:
            if(c.kind==0){if(a)log_line("Object-destroyed handler registered by "+c.owner);
                else log_line("Object-destroyed handler rejected for "+c.owner+"; registration is closed or the handler limit was reached");}
            else log_line(std::string(label[c.kind])+(a?"registered by ":"rejected for ")+c.owner);
            return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_field_handler(count,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result==1;
}

bool A2FO_CALL api_register_object_destroyed_handler(const char* module_name,const char* const* required_odf_fields,
    std::uint32_t required_odf_field_count,A2FO_ObjectDestroyedHandler handler,void* user_data) {
    const std::string owner=policy_owner(module_name);
    return core_register_field_handler(0,owner,required_odf_fields,required_odf_field_count,handler,user_data,g_object_destroyed_handlers,g_game_object_class_odf_fields);
}

template<class Registration,class Handler>
bool core_register_event_handler(std::uint32_t kind,const std::string& owner,Handler handler,void* user_data,a2fo::RustHandlerRegistry<Registration>& handlers) {
    struct Context {std::uint32_t kind;const std::string& owner;Handler handler;void* user_data;a2fo::RustHandlerRegistry<Registration>& handlers;
        std::exception_ptr error{};std::optional<StateLockGuard> guard{};
    } context{kind,owner,handler,user_data,handlers};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return c.handler!=nullptr;
        case 1:return g_state_lock_ready;
        case 2:c.guard.emplace();return 0;
        case 3:return g_policy_registration_open;
        case 4:return g_registration_transaction.active;
        case 5:return c.handlers.size();
        case 6:c.handlers.push_back(Registration{c.owner,c.handler,c.user_data});return 0;
        case 7:c.guard.reset();return 0;
        case 8:
            if(c.kind==0){if(a)log_line("Producer event handler registered by "+c.owner);
                else log_line("Producer event handler rejected for "+c.owner+"; registration is closed or the handler limit was reached");}
            else log_line(std::string(c.kind==1?"Weapon-trigger handler ":"Craft-event handler ")+(a?"registered by ":"rejected for ")+c.owner);
            return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_register_handler(step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result==1;
}

bool A2FO_CALL api_register_producer_event_handler(const char* module_name,A2FO_ProducerEventHandler handler,void* user_data) {
    const std::string owner=policy_owner(module_name);
    return core_register_event_handler(0,owner,handler,user_data,g_producer_event_handlers);
}

bool A2FO_CALL api_register_game_object_class_loaded_handler(const char* module_name,const char* const* required_odf_fields,
    std::uint32_t required_odf_field_count,A2FO_GameObjectClassLoadedHandler handler,void* user_data) {
    const std::string owner=policy_owner(module_name);
    return core_register_field_handler(1,owner,required_odf_fields,required_odf_field_count,handler,user_data,g_game_object_class_loaded_handlers,g_game_object_class_odf_fields);
}

bool A2FO_CALL api_register_race_loaded_handler(const char* module_name,const char* const* required_odf_fields,
    std::uint32_t required_odf_field_count,A2FO_RaceLoadedHandler handler,void* user_data) {
    const std::string owner=policy_owner(module_name);
    return core_register_field_handler(2,owner,required_odf_fields,required_odf_field_count,handler,user_data,g_race_loaded_handlers,g_race_odf_fields);
}

bool A2FO_CALL api_register_weapon_class_loaded_handler(const char* module_name,const char* const* required_odf_fields,
    std::uint32_t required_odf_field_count,A2FO_WeaponClassLoadedHandler handler,void* user_data) {
    const std::string owner=policy_owner(module_name);
    return core_register_field_handler(3,owner,required_odf_fields,required_odf_field_count,handler,user_data,g_weapon_class_loaded_handlers,g_weapon_class_odf_fields);
}

bool A2FO_CALL api_register_weapon_trigger_handler(const char* module_name,A2FO_WeaponTriggerHandler handler,void* user_data) {
    const std::string owner=policy_owner(module_name);
    return core_register_event_handler(1,owner,handler,user_data,g_weapon_trigger_handlers);
}

bool A2FO_CALL api_register_craft_event_handler(const char* module_name,A2FO_CraftEventHandler handler,void* user_data) {
    const std::string owner=policy_owner(module_name);
    return core_register_event_handler(2,owner,handler,user_data,g_craft_event_handlers);
}

bool A2FO_CALL api_dispatch_producer_event(const A2FO_ProducerEvent* event) {
    struct Context {const A2FO_ProducerEvent* event;
        a2fo::RustHandlerRegistry<NativeProducerEventRegistration> handlers{};
        std::optional<a2fo::RustHandlerRegistry<NativeProducerEventRegistration>::Borrowed> current{};
        std::exception_ptr error{};std::optional<StateLockGuard> guard{};
    } context{event};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return c.event!=nullptr;
        case 1:return c.event->struct_size;
        case 2:return c.event->producer!=nullptr;
        case 3:return c.event->kind;
        case 4:return c.event->target_class!=nullptr;
        case 5:return g_state_lock_ready;
        case 6:c.guard.emplace();return 0;
        case 7:c.handlers=g_producer_event_handlers;return 0;
        case 8:c.guard.reset();return 0;
        case 9:c.guard.reset();c.error=nullptr;log_line("Producer event dispatch could not copy registrations");return 0;
        case 10:return c.handlers.size();
        case 11:c.current=c.handlers.borrow(a);return c.current->handler!=nullptr;
        case 12:return c.current->handler(c.event,c.current->user_data);
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const A2foRsProducerEventRules rules{sizeof(A2FO_ProducerEvent),A2FO_PRODUCER_EVENT_CLEARED,A2FO_PRODUCER_EVENT_DESTROYING,A2FO_PRODUCER_EVENT_ADMIT,A2FO_PRODUCER_EVENT_FINISHING,A2FO_PRODUCER_EVENT_STARTING_EFFECT};
    const auto result=a2fo_rs_stage8_core_dispatch_producer(&rules,step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result==1;
}

std::uint32_t A2FO_CALL api_extension_root_count() {
    return static_cast<std::uint32_t>(g_extension_roots.size());
}

const char* A2FO_CALL api_extension_root(std::uint32_t index) {
    if (index >= g_extension_roots.size()) return nullptr;
    return g_extension_roots[index].c_str();
}

std::uint32_t A2FO_CALL api_upgrade_pod_maximum_tier() {
    // Retained for binary compatibility with older third-party modules.
    // Current FeaturePack builds read upgradePodMaximumTier from RTS_CFG.h.
    return 6;
}

bool A2FO_CALL api_get_original_classlabel(
    void* parameter_db, char* output, std::uint32_t output_size) {
    if (!parameter_db || !output || output_size == 0 ||
        !g_state_lock_ready) {
        return false;
    }
    output[0] = '\0';
    try {
        StateLockGuard lock;
        const auto found = g_original_classlabels.find(parameter_db);
        if (found == g_original_classlabels.end() ||
            found->second.size() + 1 > output_size) {
            return false;
        }
        std::memcpy(output, found->second.c_str(), found->second.size() + 1);
        return true;
    } catch (...) {
        output[0] = '\0';
        return false;
    }
}

bool A2FO_CALL api_associate_evolver_cocoon_class(
    void* class_object, void* parameter_db) {
    if (!g_evolver_hooks_ready) return false;
    try {
        return associate_evolver_cocoon_class(
            class_object, parameter_db, "Hybrid class");
    } catch (...) {
        log_line("Hybrid cocoon class association failed; using Fleet Ops "
                 "default");
        return false;
    }
}

A2FO_ModuleApi make_module_api() {
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = &module_log;
    api.armada_module = &api_armada_module;
    api.fleetops_module = &api_fleetops_module;
    api.root_directory = &api_root_directory;
    api.install_inline_hook = &api_install_inline_hook;
    api.patch_jump = &api_patch_jump;
    api.patch_call = &api_patch_call;
    api.register_fofs_item_lookup_handler =
        &api_register_fofs_item_lookup_handler;
    api.extension_root_count = &api_extension_root_count;
    api.extension_root = &api_extension_root;
    api.register_classlabel_alias = &api_register_classlabel_alias;
    api.register_evolver_cocoon_command =
        &api_register_evolver_cocoon_command;
    api.api_revision = A2FO_MODULE_API_REVISION;
    api.capabilities = A2FO_CAP_OBJECT_DESTROYED_DISPATCH |
        A2FO_CAP_UPGRADE_POD_POLICY |
        A2FO_CAP_ORIGINAL_CLASSLABEL;
    if (g_evolver_hooks_ready) {
        api.capabilities |= A2FO_CAP_COCOON_CLASS_ASSOCIATION;
    }
    api.capabilities |= A2FO_CAP_INFO_INI_DEFAULTS;
    api.capabilities |= A2FO_CAP_ODF_OVERLAY_DIRECTORIES;
    api.capabilities |= A2FO_CAP_PRODUCER_EVENTS;
    api.capabilities |= A2FO_CAP_GAME_OBJECT_CLASS_LOADED;
    api.capabilities |= A2FO_CAP_RACE_LOADED;
    api.capabilities |= A2FO_CAP_WEAPON_CLASS_LOADED;
    api.capabilities |= A2FO_CAP_WEAPON_TRIGGER_EVENTS;
    api.capabilities |= A2FO_CAP_CRAFT_EVENTS;
    api.capabilities |= A2FO_CAP_RACE_ODF_DEFAULTS;
    if (g_classlabel_alias_hook_ready) {
        api.capabilities |= A2FO_CAP_CLASSLABEL_ODF_DEFAULTS;
    }
    api.register_object_destroyed_handler =
        &api_register_object_destroyed_handler;
    api.upgrade_pod_maximum_tier = &api_upgrade_pod_maximum_tier;
    api.get_original_classlabel = &api_get_original_classlabel;
    api.associate_evolver_cocoon_class =
        &api_associate_evolver_cocoon_class;
    api.register_info_ini_defaults_handler =
        &api_register_info_ini_defaults_handler;
    api.register_odf_overlay_directory =
        &api_register_odf_overlay_directory;
    api.odf_overlay_directory_count =
        &api_odf_overlay_directory_count;
    api.get_odf_overlay_directory =
        &api_get_odf_overlay_directory;
    api.register_producer_event_handler =
        &api_register_producer_event_handler;
    api.dispatch_producer_event = &api_dispatch_producer_event;
    api.register_classlabel_odf_defaults =
        &api_register_classlabel_odf_defaults;
    api.patch_bytes = &api_patch_bytes;
    api.get_settings_directory = &api_get_settings_directory;
    api.register_game_object_class_loaded_handler =
        &api_register_game_object_class_loaded_handler;
    api.register_race_loaded_handler = &api_register_race_loaded_handler;
    api.register_weapon_class_loaded_handler =
        &api_register_weapon_class_loaded_handler;
    api.register_weapon_trigger_handler =
        &api_register_weapon_trigger_handler;
    api.register_craft_event_handler = &api_register_craft_event_handler;
    api.register_race_odf_defaults = &api_register_race_odf_defaults;
    return api;
}

struct CoreEarlyStartupContext {std::exception_ptr error{};};
std::uint64_t core_early_startup_step(void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept {
    auto& c=*static_cast<CoreEarlyStartupContext*>(opaque);
    try {switch(op){
    case 0:g_armada=GetModuleHandleA(nullptr);return 0;
    case 1:return g_fleet_ops!=nullptr;
    case 2:g_fleet_ops=GetModuleHandleA("FleetOpsHook.dll");return 0;
    case 3:Sleep(10);return 0;
    case 4:return validate_armada_module(g_armada);
    case 5:return validate_module(g_fleet_ops,kFleetOpsTimestamp,kFleetOpsImageSize,"FleetOpsHook.dll");
    case 6:log_line("No hooks installed");return 0;
    default:return 0;
    }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
struct CoreStartupContext {
    a2fo::ExtensionRootDiscovery roots{};
    a2fo::ModuleRegistrationObserver observer{};
    bool destroyed_requested=false;
    std::size_t alias_count=0,classlabel_default_count=0,race_default_count=0;
    std::string cocoon_command{};
    std::exception_ptr error{};
};
struct CoreStartupHook {bool* ready;bool(*install)();const char* ready_message;};
std::uint64_t core_startup_step(void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept {
    auto& c=*static_cast<CoreStartupContext*>(opaque);
    const CoreStartupHook early[]={
        {&g_classlabel_alias_hook_ready,&install_classlabel_alias_hook,"ParameterDB string policy hook enabled before class loading"},
        {&g_evolver_hooks_ready,&install_evolver_hooks,"Evolver policy hooks enabled before class loading"},
        {&g_user_profile_game_speed_hook_ready,&install_user_profile_game_speed_hook,"Armada default user-profile game-speed hook enabled before profile loading"},
        {&g_fofs_item_get_lookup_hook_ready,&install_fofs_item_get_lookup_hook,"FOFS item lookup dispatcher enabled before ODF loading"},
        {&g_mod_user_directory_hook_ready,&install_mod_user_directory_hook,"Fleet Operations SettingsDirectory hook enabled before settings loading"},
        {&g_fo_settings_default_hook_ready,&install_fo_settings_default_hook,"Fleet Operations Settings.xml default hook enabled before settings loading"},
        {&g_default_game_speed_hook_ready,&install_default_game_speed_hook,"Fleet Operations DefaultGameSpeed hook enabled before profile loading"},
        {&g_runtime_game_speed_hook_ready,&install_runtime_game_speed_hook,"Fleet Operations runtime DefaultGameSpeed hook enabled before profile loading"},
    };
    const CoreStartupHook optional[]={
        {&g_race_loaded_hook_ready,&install_race_loaded_hook,nullptr},
        {&g_weapon_class_loaded_hook_ready,&install_weapon_class_loaded_hook,nullptr},
        {&g_weapon_trigger_hook_ready,&install_weapon_trigger_hook,nullptr},
        {&g_craft_event_hooks_ready,&install_craft_event_hooks,nullptr},
    };
    try {switch(op){
    case 0:return *early[a].ready;
    case 1:log_line(early[a].ready_message);return 0;
    case 2:*early[a].ready=early[a].install();return 0;
    case 10:return g_root_directory.empty();
    case 11:g_root_directory=module_directory(nullptr);return 0;
    case 12:c.roots=a2fo::discover_extension_roots(g_root_directory,GetCommandLineA()?GetCommandLineA():"");return 0;
    case 13:g_extension_roots=c.roots.roots;
        a2fo::native11_light_classes::configure_from_roots(g_extension_roots,+[](const char* text){log_line(text);});
        if(a2fo::native11_light_classes::requested())for(const auto* field:a2fo::native11::requested_light_fields())g_game_object_class_odf_fields.insert(field);return 0;
    case 14:return c.roots.diagnostics.size();
    case 15:log_line(c.roots.diagnostics[a]);return 0;
    case 16:return !c.roots.active_mod.empty();
    case 17:log_line("Extension roots: active mod: "+c.roots.active_mod);return 0;
    case 18:log_line("Extension roots: "+std::to_string(g_extension_roots.size())+" search roots");return 0;
    case 19:return g_extension_roots.size();
    case 20:log_line("  extension root: "+g_extension_roots[a]);return 0;
    case 21:return g_module_menu_hook_ready;
    case 22:return g_fleet_ops!=nullptr;
    case 23:g_module_menu_hook_ready=a2fo::install_module_menu(g_fleet_ops,g_extension_roots.front(),&log_line);return 0;
    case 24:return g_renderer_options_hook_ready;
    case 25:g_renderer_options_hook_ready=a2fo::install_renderer_options(g_fleet_ops,g_root_directory,&log_line);return 0;
    case 26:g_module_api=make_module_api();return 0;
    case 27:c.observer.begin=&begin_module_registration;c.observer.finish=&finish_module_registration;
        a2fo::load_native_modules_from_roots(g_extension_roots,g_module_api,g_loaded_modules,&log_line,c.observer);return 0;
    case 28:c.destroyed_requested=!g_object_destroyed_handlers.empty();return c.destroyed_requested;
    case 29:return !g_game_object_class_loaded_handlers.empty()||a2fo::native11_light_classes::requested();
    case 30:g_game_object_class_loaded_hook_ready=install_game_object_class_loaded_hook();
        a2fo::native11_light_classes::dispatch_available(g_game_object_class_loaded_hook_ready);return 0;
    case 31:return g_game_object_class_loaded_hook_ready;
    case 32:g_object_destroyed_hook_ready=install_object_destroyed_hook();return 0;
    case 33:switch(a){case 0:return !g_race_loaded_handlers.empty();case 1:return !g_weapon_class_loaded_handlers.empty();case 2:return !g_weapon_trigger_handlers.empty();case 3:return !g_craft_event_handlers.empty();default:return 0;}
    case 34:*optional[a].ready=optional[a].install();return 0;
    case 35:
        // Keep native SDK argument evaluation and process-lifetime API storage.
        a2fo::finish_builtin_module_preparation(
            g_object_destroyed_hook_ready&&g_game_object_class_loaded_hook_ready,
            g_weapon_trigger_hook_ready,g_weapon_class_loaded_hook_ready,
            g_mod_user_directory_hook_ready&&g_fo_settings_default_hook_ready&&
                g_default_game_speed_hook_ready&&g_runtime_game_speed_hook_ready&&g_user_profile_game_speed_hook_ready,
            g_craft_event_hooks_ready,g_game_object_class_loaded_hook_ready,g_race_loaded_hook_ready,
            a2fo::producer_dispatch_available(g_loaded_modules));return 0;
    case 36:return static_cast<std::size_t>(a2fo::BuiltinFeature::count);
    case 37:{const char* name=a2fo::canonical_builtin_name(static_cast<a2fo::BuiltinFeature>(a));
        log_line(std::string("Built-in module state: ")+name+" - "+a2fo::builtin_module_status(name,g_extension_roots));return 0;}
    case 38:{
        // One native critical-section publication, with the original release store.
        StateLockGuard lock;g_policy_registration_open=false;
        c.alias_count=g_classlabel_aliases.size();c.classlabel_default_count=g_classlabel_odf_defaults.size();
        c.race_default_count=g_race_odf_defaults.size();c.cocoon_command=g_evolver_cocoon_command;
        g_fofs_item_lookup_dispatch.handler=g_fofs_item_lookup_handler;
        g_fofs_item_lookup_dispatch.user_data=g_fofs_item_lookup_user_data;
        g_fofs_item_lookup_dispatch_ready.store(true,std::memory_order_release);return 0;
    }
    case 39:
        log_line("Policy registration closed: "+std::to_string(c.alias_count)+" classlabel alias"+
            (c.alias_count==1?"":"es")+", "+std::to_string(c.classlabel_default_count)+
            " classlabel ODF default set"+(c.classlabel_default_count==1?"":"s")+
            ", "+std::to_string(c.race_default_count)+" Race ODF default"+(c.race_default_count==1?"":"s")+
            ", Evolver cocoon command: "+(c.cocoon_command.empty()?"<none>":c.cocoon_command));return 0;
    case 40:log_line("Native callbacks ready: "+std::to_string(g_object_destroyed_handlers.size())+" object-destroyed");return 0;
    default:return 0;
    }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
DWORD WINAPI initialize(void*) {
    CoreEarlyStartupContext early;
    const auto ready=a2fo_rs_stage8_core_startup(0,&core_early_startup_step,&early);
    if(early.error)std::rethrow_exception(early.error);
    if(ready!=0)return 1;
    try {
        CoreStartupContext context;
        const auto result=a2fo_rs_stage8_core_startup(1,&core_startup_step,&context);
        if(context.error)std::rethrow_exception(context.error);
        if(result!=0)return 1;
    }catch(...){
        log_line("Initialization aborted by an unexpected C++ exception");return 1;
    }
    log_line("A2FOExtensions initialization complete");return 0;
}

} //namespace

extern "C" {
void* a2fo_cocoon_resume = nullptr;
void* a2fo_cocoon_update_resume = nullptr;
}

extern "C" void* a2fo_select_cocoon(void* evolver) {
    struct Context {void* evolver;const std::uint8_t* bytes=nullptr;void* class_object=nullptr;std::string name{};
        decltype(g_class_cocoons)::iterator selected{};void* cached=nullptr;void* database=nullptr;void* geometry=nullptr;
        bool alternative=false;void** slot=nullptr;const char* fallback_name=nullptr;void* result=nullptr;std::exception_ptr error{};
    } context{evolver};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return c.evolver!=nullptr;
        case 1:return g_state_lock_ready;
        case 2:c.bytes=static_cast<std::uint8_t*>(c.evolver);c.class_object=*reinterpret_cast<void* const*>(c.bytes+0x40);return 0;
        case 3:EnterCriticalSection(&g_state_lock);return 0;
        case 4:c.selected=g_class_cocoons.find(c.class_object);return c.selected!=g_class_cocoons.end();
        case 5:c.name=c.selected->second;return 0;
        case 6:return c.name.empty();
        case 7:{const auto found=g_loaded_cocoons.find(lower_ascii(c.name));if(found==g_loaded_cocoons.end())return 0;c.cached=found->second;return 1;}
        case 8:return c.cached!=nullptr;
        case 9:return g_logged_cocoon_classes.insert(c.class_object).second;
        case 10:LeaveCriticalSection(&g_state_lock);return 0;
        case 11:log_line("Cocoon selector for EvolverClass: "+(c.name.empty()?std::string("<Fleet Ops default>"):c.name));return 0;
        case 12:c.result=c.cached;return 0;
        case 13:c.name.clear();return 0;
        case 14:c.database=*at<void*>(g_armada,kSodDatabaseRva);return c.database!=nullptr;
        case 15:c.geometry=a2fo_load_sod(at(g_armada,kLoadSodRva),c.database,c.name.c_str());return 0;
        case 16:EnterCriticalSection(&g_state_lock);return 0;
        case 17:g_loaded_cocoons[lower_ascii(c.name)]=c.geometry;return 0;
        case 18:LeaveCriticalSection(&g_state_lock);return 0;
        case 19:return c.geometry!=nullptr;
        case 20:c.result=c.geometry;return 0;
        case 21:log_line("Could not load cocoon SOD; using Fleet Ops default: "+c.name);return 0;
        case 22:return *reinterpret_cast<const std::uint32_t*>(c.bytes+0x1e8)==0;
        case 23:c.alternative=a!=0;c.slot=at<void*>(g_armada,c.alternative?kAlternativeCocoonRva:kDefaultCocoonRva);return *c.slot!=nullptr;
        case 24:c.result=*c.slot;return 0;
        case 25:c.fallback_name=c.alternative?kAlternativeCocoonName:kDefaultCocoonName;return 0;
        case 26:c.database=*at<void*>(g_armada,kSodDatabaseRva);return c.database!=nullptr;
        case 27:c.geometry=a2fo_load_sod(at(g_armada,kLoadSodRva),c.database,c.fallback_name);return 0;
        case 29:*c.slot=c.geometry;return 0;
        case 30:log_line("Initialized Fleet Ops default cocoon on demand: "+std::string(c.fallback_name));return 0;
        case 31:log_line("Could not initialize Fleet Ops default cocoon: "+std::string(c.fallback_name));return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_core_select_cocoon(step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return context.result;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_Initialize() {
    // This native exception boundary is called outside the Rust entry's loader lock.
    struct Context {std::exception_ptr error{};} context;
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t a,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return static_cast<std::uint32_t>(InterlockedCompareExchange(&g_initialize_state,1,0));
        case 1:return static_cast<std::uint32_t>(InterlockedCompareExchange(&g_initialize_state,0,0));
        case 2:Sleep(1);return 0;
        case 3:return g_log==INVALID_HANDLE_VALUE;
        case 4:open_log();return 0;
        case 5:log_line("A2FOExtensions deferred initialization started");return 0;
        case 6:return g_root_directory.empty();
        case 7:g_root_directory=module_directory(nullptr);return 0;
        case 8:g_armada=GetModuleHandleA(nullptr);return 0;
        case 9:return !g_fleet_ops;
        case 10:g_fleet_ops=GetModuleHandleA("FleetOpsHook.dll");return 0;
        case 11:return !g_armada;
        case 12:log_line("ArmadaL.exe was unavailable during A2FO_Initialize");return 0;
        case 13:InterlockedExchange(&g_initialize_state,0);return 0;
        case 14:a2fo_cocoon_resume=at(g_armada,kCocoonSelectorRva+5);a2fo_cocoon_update_resume=at(g_armada,kCocoonUpdateResumeRva);return 0;
        case 15:return !g_state_lock_ready;
        case 16:InitializeCriticalSection(&g_state_lock);g_state_lock_ready=true;return 0;
        case 17:return initialize(nullptr)==0;
        case 18:InterlockedExchange(&g_initialize_state,a?2:0);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const auto result=a2fo_rs_stage8_core_initialize(step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result==1;
}
extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_InitializeNoUnwind() noexcept {
    return a2fo::initialize_without_unwind(
        [] { return A2FO_Initialize(); },
        []() noexcept { InterlockedExchange(&g_initialize_state, 0); });
}

DWORD WINAPI initialize_worker(void*) {
    struct Context {std::exception_ptr error{};} context;
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t,std::uintptr_t) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:g_initialize_thread_id=GetCurrentThreadId();return 0;
        case 1:return A2FO_Initialize();
        case 2:g_initialize_thread_id=0;return 0;
        case 3:InterlockedExchange(&g_deferred_initialization_finished,1);return 0;
        case 4:return kShellDisplayMonitorEnabled;
        case 5:run_shell_display_monitor();return 0;
        case 6:log_line("Shell display: graphics-settings monitor disabled");return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_core_worker(step,&context);
    if(context.error)std::rethrow_exception(context.error);
    return 0;
}

// Native primitives for the Rust-owned DLL entry. These operations preserve
// the established attachment order and cannot unwind across the Rust ABI.
extern "C" std::uint32_t a2fo_native_process_attach_step(
    std::uint32_t operation, HINSTANCE instance) noexcept {
    try {
        switch (operation) {
        case 0: DisableThreadLibraryCalls(instance); return 0;
        case 1: open_log(); return 0;
        case 2: log_line("A2FOExtensions initialization started"); return 0;
        case 3: g_root_directory=module_directory(nullptr); return 0;
        case 4: g_armada=GetModuleHandleA(nullptr); return 0;
        case 5: g_fleet_ops=GetModuleHandleA("FleetOpsHook.dll"); return 0;
        case 6: return g_armada!=nullptr;
        case 7: a2fo_cocoon_resume=at(g_armada,kCocoonSelectorRva+5); return 0;
        case 8: a2fo_cocoon_update_resume=at(g_armada,kCocoonUpdateResumeRva); return 0;
        case 9: InitializeCriticalSection(&g_state_lock); g_state_lock_ready=true; return 0;
        case 10: return validate_armada_module(g_armada);
        case 11:
            if (!a2fo::refit::install_startup(g_armada,g_root_directory,&log_line,&a2fo::initialize_refit_scene_sources))
                a2fo::renderer::install_d3d8_observer(g_armada,g_fleet_ops,g_root_directory,&log_line);
            return 0;
        case 12: g_evolver_hooks_ready=install_evolver_hooks(); return 0;
        case 13: g_user_profile_game_speed_hook_ready=install_user_profile_game_speed_hook(); return 0;
        case 14: return validate_module(g_fleet_ops,kFleetOpsTimestamp,kFleetOpsImageSize,"FleetOpsHook.dll");
        case 15: a2fo::install_nebula_renderer_early(g_armada,g_fleet_ops,g_root_directory,&log_line); return 0;
        case 16: g_fofs_item_get_lookup_hook_ready=install_fofs_item_get_lookup_hook(); return 0;
        case 17: g_mod_user_directory_hook_ready=install_mod_user_directory_hook(); return 0;
        case 18: g_fo_settings_default_hook_ready=install_fo_settings_default_hook(); return 0;
        case 19: g_default_game_speed_hook_ready=install_default_game_speed_hook(); return 0;
        case 20: g_runtime_game_speed_hook_ready=install_runtime_game_speed_hook(); return 0;
        case 21: log_line("Could not create deferred initialization worker"); return 0;
        default: std::terminate();
        }
    } catch (...) {
        // The old loader entry had no native exception recovery. Preserve a
        // fatal boundary failure without unwinding through Rust or loader code.
        std::terminate();
    }
}
extern "C" DWORD a2fo_native_initialize_worker() noexcept {
    try { return initialize_worker(nullptr); }
    catch (...) { std::terminate(); }
}

#if defined(A2FO_RUST_BINK_RUNTIME)
namespace { volatile LONG g_bink_early_attach_state = 0; }
// The paired proxy calls this while its original nested-load sequence still
// holds loader lock, after FleetOpsHook has attached. These are exactly the
// early hook primitives previously run by our DllMain, not full initialization.
extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_BinkEarlyAttachNoUnwind() noexcept {
    return a2fo::initialize_without_unwind([] {
        if (InterlockedCompareExchange(&g_bink_early_attach_state, 0, 0) == 2) return true;
        if (InterlockedCompareExchange(&g_bink_early_attach_state, 1, 0) != 0) return false;
        g_fleet_ops = GetModuleHandleA("FleetOpsHook.dll");
        if (!validate_armada_module(g_armada) ||
            !validate_module(g_fleet_ops,kFleetOpsTimestamp,kFleetOpsImageSize,"FleetOpsHook.dll")) {
            InterlockedExchange(&g_bink_early_attach_state,0);
            log_line("Rust Bink early attachment refused unsupported engine/provider");
            return false;
        }
        for (std::uint32_t operation = 11; operation <= 13; ++operation)
            a2fo_native_process_attach_step(operation,nullptr);
        for (std::uint32_t operation = 15; operation <= 20; ++operation)
            a2fo_native_process_attach_step(operation,nullptr);
        InterlockedExchange(&g_bink_early_attach_state,2);
        log_line("Rust Bink early attachment completed after FleetOpsHook attachment");
        return true;
    }, []() noexcept { InterlockedExchange(&g_bink_early_attach_state,0); });
}
extern "C" std::uint32_t a2fo_native_bink_early_ready() noexcept {
    if (InterlockedCompareExchange(&g_bink_early_attach_state,0,0) == 2) return 1;
    log_line("Rust Bink initialization requires the paired startup proxy early-attach bridge");
    return 0;
}
#endif
