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
#define A2FO_MODULE_API_REVISION 2u

// The major version remains 4 so DLLs compiled against the original v4 ABI
// continue to load. Revisions append fields to A2FO_ModuleApi; never insert or
// reorder fields in the existing prefix.
enum A2FO_ModuleCapability : std::uint64_t {
    A2FO_CAP_NONE = 0,
    A2FO_CAP_OBJECT_DESTROYED_DISPATCH = 1ull << 0,
    A2FO_CAP_UPGRADE_POD_POLICY = 1ull << 1,
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
