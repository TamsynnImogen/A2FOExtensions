/*
 * Stable C-compatible ABI shared by A2FOExtensions and optional native DLLs.
 *
 * The API is append-only. Modules must check both the major version and the
 * member boundary before using fields introduced by later revisions.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#  define A2FO_CALL __cdecl
#  if defined(A2FO_BUILDING_CORE)
#    define A2FO_EXPORT __declspec(dllexport)
#  else
#    define A2FO_EXPORT __declspec(dllimport)
#  endif
#else
#  define A2FO_CALL
#  define A2FO_EXPORT
#endif

#define A2FO_MODULE_API_VERSION 4u
#define A2FO_MODULE_API_REVISION 11u

// The major version remains 4 so DLLs compiled against the original v4 ABI
// continue to load. Revisions append fields to A2FO_ModuleApi; never insert or
// reorder fields in the existing prefix.
enum A2FO_ModuleCapability : std::uint64_t {
    A2FO_CAP_NONE = 0,
    A2FO_CAP_OBJECT_DESTROYED_DISPATCH = 1ull << 0,
    A2FO_CAP_UPGRADE_POD_POLICY = 1ull << 1,
    A2FO_CAP_ORIGINAL_CLASSLABEL = 1ull << 2,
    A2FO_CAP_COCOON_CLASS_ASSOCIATION = 1ull << 3,
    A2FO_CAP_INFO_INI_DEFAULTS = 1ull << 4,
    A2FO_CAP_ODF_OVERLAY_DIRECTORIES = 1ull << 5,
    A2FO_CAP_PRODUCER_EVENTS = 1ull << 6,
    A2FO_CAP_CLASSLABEL_ODF_DEFAULTS = 1ull << 7,
};

enum A2FO_ProducerEventKind : std::uint32_t {
    // Sent before a class is admitted to a Producer queue. Returning false
    // rejects this one admission without changing any existing queue items.
    A2FO_PRODUCER_EVENT_ADMIT = 0,

    // Sent after the native Producer completion callback has consumed the
    // head item. target_class is captured before the native callback runs.
    A2FO_PRODUCER_EVENT_FINISHED = 1,

    // Sent before the native Producer destructor runs. target_class is null.
    A2FO_PRODUCER_EVENT_DESTROYING = 2,

    // Sent immediately before the native Producer completion callback.
    // Returning false claims an in-place completion and suppresses that
    // callback; FeaturePack still sends FINISHED afterward. This is intended
    // for legacy non-object build classes whose Build() cannot return the
    // GameObject required by A2/FO's ordinary completion path.
    A2FO_PRODUCER_EVENT_FINISHING = 3,

    // Sent immediately before Producer::mStartConstructionEffect creates a
    // cosmetic construction instance for target_class. Returning false
    // suppresses only that effect; the synchronized build remains admitted
    // and continues normally. This is required by legacy non-object build
    // classes which cannot safely back a Craft construction renderer.
    A2FO_PRODUCER_EVENT_STARTING_EFFECT = 4,
};

enum A2FO_OdfOverlayPrecedence : std::uint32_t {
    A2FO_ODF_OVERLAY_NORMAL = 0,
    A2FO_ODF_OVERLAY_OVERRIDE = 1,
};

enum A2FO_ObjectReplacementFlags : std::uint32_t {
    A2FO_REPLACEMENT_INHERIT_POSITION = 1u << 0,
    A2FO_REPLACEMENT_INHERIT_ROTATION = 1u << 1,
};

enum A2FO_ObjectReplacementOwner : std::uint32_t {
    A2FO_REPLACEMENT_OWNER_NEUTRAL = 0,
    A2FO_REPLACEMENT_OWNER_ORIGINAL = 1,
};

struct A2FO_StringView {
    const char* data;
    std::uint32_t size;
};

struct A2FO_OdfFieldView {
    A2FO_StringView name;
    A2FO_StringView value;
};

// Startup-only declarative fallback for one aliased classlabel. The core
// copies both strings during registration. A fallback is visible only when
// ParameterDB's ordinary lookup (including ODF includes) reports the command
// missing, so an object or inherited parent value always wins.
struct A2FO_ClasslabelOdfDefault {
    const char* command;
    const char* value;
};

struct A2FO_ObjectDestroyedEvent {
    std::uint32_t struct_size;
    A2FO_StringView source_odf;
    const A2FO_OdfFieldView* odf_fields;
    std::uint32_t odf_field_count;
    float transform[12];
    std::int32_t source_team;
    std::uint32_t source_handle;
    std::uint32_t random_seed;
};

struct A2FO_ObjectReplacement {
    std::uint32_t struct_size;
    const char* odf;
    std::uint32_t flags;
    std::uint32_t owner;
};

// Return true to claim the event and request the replacement written to
// `replacement`. All event pointers expire when the callback returns. The
// core validates and copies replacement.odf before invoking another module.
using A2FO_ObjectDestroyedHandler = bool (A2FO_CALL*)(
    const A2FO_ObjectDestroyedEvent* event,
    A2FO_ObjectReplacement* replacement,
    void* user_data);

// Called from the core's checked FOFS_ItemGet dispatcher. Return true when the
// module selected the result written to `result`; return false to let Fleet
// Operations perform its native hash-table lookup unchanged.
using A2FO_FofsItemLookupHandler = bool (A2FO_CALL*)(
    void* file_system,
    void* delphi_name,
    std::uint32_t flags,
    void** result,
    void* user_data);

// Supplies optional Fleet Ops info.ini defaults without placing mod policy in
// the core hook DLL. `normal_settings_directory` is the native Fleet Ops path
// when a resolved override is requested and may be null for speed-only calls.
// Write an empty string to keep the native directory. The callback owns no
// returned storage; the core copies every value before returning.
using A2FO_InfoIniDefaultsHandler = bool (A2FO_CALL*)(
    const char* normal_settings_directory,
    char* resolved_settings_directory,
    std::uint32_t resolved_settings_directory_size,
    std::uint32_t* has_default_game_speed,
    std::int32_t* default_game_speed,
    void* user_data);

struct A2FO_ProducerEvent {
    std::uint32_t struct_size;
    std::uint32_t kind;
    void* producer;
    void* target_class;
};

// ADMIT, FINISHING, and STARTING_EFFECT continue only when every registered
// handler returns true. FINISHED and DESTROYING are notification-only. Event
// pointers expire when the callback returns.
using A2FO_ProducerEventHandler = bool (A2FO_CALL*)(
    const A2FO_ProducerEvent* event,
    void* user_data);

struct A2FO_InlineHook {
    void* target;
    void* gateway;
    std::size_t length;
};

struct A2FO_ModuleApi {
    std::uint32_t struct_size;
    std::uint32_t api_version;

    void (A2FO_CALL* log)(const char* module_name, const char* message);
    void* (A2FO_CALL* armada_module)();
    void* (A2FO_CALL* fleetops_module)();
    const char* (A2FO_CALL* root_directory)();

    bool (A2FO_CALL* install_inline_hook)(
        void* target,
        void* replacement,
        std::size_t length,
        const std::uint8_t* expected,
        A2FO_InlineHook* hook);

    bool (A2FO_CALL* patch_jump)(
        void* target,
        void* replacement,
        const std::uint8_t* expected,
        std::size_t length);

    bool (A2FO_CALL* patch_call)(
        void* target,
        void* replacement,
        const std::uint8_t* expected,
        std::size_t length);

    bool (A2FO_CALL* register_fofs_item_lookup_handler)(
        const char* module_name,
        A2FO_FofsItemLookupHandler handler,
        void* user_data);

    // Extension roots are ordered from lowest to highest precedence: Data,
    // parent mods, then the selected mod. Returned strings remain valid for
    // the process lifetime.
    std::uint32_t (A2FO_CALL* extension_root_count)();
    const char* (A2FO_CALL* extension_root)(std::uint32_t index);

    // Semantic policy registration is accepted only during deferred startup.
    // Duplicate ownership is rejected; a mod should override the original
    // module/script by using the same basename in a higher-precedence root.
    bool (A2FO_CALL* register_classlabel_alias)(
        const char* module_name,
        const char* source,
        const char* target);
    bool (A2FO_CALL* register_evolver_cocoon_command)(
        const char* module_name,
        const char* command);

    // Revision 1 additions. Use A2FO_MODULE_API_HAS before reading these
    // members so one module binary can run against compatible v4 cores.
    std::uint32_t api_revision;
    std::uint64_t capabilities;

    // Required ODF fields are copied during startup. Handlers run in
    // deterministic module-load/registration order; the first valid claim
    // wins. `basename` is always included and need not be requested.
    bool (A2FO_CALL* register_object_destroyed_handler)(
        const char* module_name,
        const char* const* required_odf_fields,
        std::uint32_t required_odf_field_count,
        A2FO_ObjectDestroyedHandler handler,
        void* user_data);

    // Revision 2 addition. Returns the Lua-configured maximum upgrade-pod
    // tier. The value is dynamic because native modules load before scripts.
    std::uint32_t (A2FO_CALL* upgrade_pod_maximum_tier)();

    // Revision 3 addition. Classlabel aliases replace ParameterDB's public
    // value before Armada selects a native class. This query preserves the
    // normalized source label requested by the ODF so an aliased feature can
    // retain its own identity while deliberately using a native base class.
    bool (A2FO_CALL* get_original_classlabel)(
        void* parameter_db,
        char* output,
        std::uint32_t output_size);

    // Revision 4 addition. Associates an aliased/native class object with the
    // same registered cocoon policy used by EvolverClass::BuildClass. This
    // lets a deliberately aliased host retain per-ODF cocoon selection while
    // continuing to use its safe native base class.
    bool (A2FO_CALL* associate_evolver_cocoon_class)(
        void* class_object,
        void* parameter_db);

    // Revision 5 addition. The core retains the timing-critical native hooks;
    // one module owns parsing and resolving the SettingsDirectory and
    // DefaultGameSpeed values supplied by Fleet Ops info.ini files.
    bool (A2FO_CALL* register_info_ini_defaults_handler)(
        const char* module_name,
        A2FO_InfoIniDefaultsHandler handler,
        void* user_data);

    // Revision 6 additions. Modules may declare extra relative directories
    // containing ODFs during startup. Higher precedence wins only within the
    // same Fleet Operations mod root; normal child/parent mod priority remains
    // authoritative. Query functions remain available after registration
    // closes so the filesystem owner can build its index lazily.
    bool (A2FO_CALL* register_odf_overlay_directory)(
        const char* module_name,
        const char* relative_directory,
        std::uint32_t precedence);
    std::uint32_t (A2FO_CALL* odf_overlay_directory_count)();
    bool (A2FO_CALL* get_odf_overlay_directory)(
        std::uint32_t index,
        char* relative_directory,
        std::uint32_t relative_directory_size,
        std::uint32_t* precedence);

    // Revision 7 additions. Policy modules register during deferred startup;
    // the module which owns Fleet Operations' Producer hooks dispatches the
    // events at runtime. Keeping one hook owner prevents independent modules
    // from patching the same executable addresses.
    bool (A2FO_CALL* register_producer_event_handler)(
        const char* module_name,
        A2FO_ProducerEventHandler handler,
        void* user_data);
    bool (A2FO_CALL* dispatch_producer_event)(
        const A2FO_ProducerEvent* event);

    // Revision 10 addition. The core owns all shared ParameterDB getter hooks;
    // compatibility modules supply only copied classlabel-specific policy.
    bool (A2FO_CALL* register_classlabel_odf_defaults)(
        const char* module_name,
        const char* classlabel,
        const A2FO_ClasslabelOdfDefault* defaults,
        std::uint32_t default_count);

    // Revision 11 addition. Replaces an exact byte range only when it still
    // matches the supported executable signature. This is intended for data
    // pointers and fixed-size constants which cannot use a CALL/JMP patch.
    bool (A2FO_CALL* patch_bytes)(
        void* target,
        const std::uint8_t* replacement,
        const std::uint8_t* expected,
        std::size_t length);
};

#define A2FO_MODULE_API_V4_BASE_SIZE \
    (offsetof(A2FO_ModuleApi, register_evolver_cocoon_command) + \
     sizeof(((A2FO_ModuleApi*)0)->register_evolver_cocoon_command))

#define A2FO_MODULE_API_HAS(api, member) \
    ((api) != nullptr && \
     (api)->struct_size >= offsetof(A2FO_ModuleApi, member) + \
         sizeof((api)->member))

using A2FO_ModuleInitFn = bool (A2FO_CALL*)(const A2FO_ModuleApi* api);
using A2FO_ModuleShutdownFn = void (A2FO_CALL*)();

extern "C" bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api);
extern "C" void A2FO_CALL A2FO_ModuleShutdown();
