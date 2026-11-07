/* MMM compatibility facade. Rust owns handoff state and the opaque Worker
 * lifetime; native operations below fence C++ exceptions before returning. */
#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/mmm_runtime_ffi.h"
#include "mmm_loader/Worker.h"
#include "mmm_armada/ScriptInterface.h"
#include "mmm_armada/Mission_Internal.h"
#include <windows.h>
#include <cstdint>
#include <exception>
#include "fault_diagnostics.hpp"

namespace mmm { namespace a2fo_missions {
namespace {
const A2FO_ModuleApi* g_api = nullptr;
void log_line(const char* text) noexcept {
    if (g_api && g_api->log) {
        g_api->log("A2FOMissions", text ? text : "");
    } else if (text) {
        OutputDebugStringA("A2FOMissions: ");
        OutputDebugStringA(text);
        OutputDebugStringA("\n");
    }
}

bool read_process_memory(std::uintptr_t address, void* destination, std::size_t size) noexcept {
    SIZE_T copied = 0;
    return address != 0 && destination && size != 0 &&
           ReadProcessMemory(GetCurrentProcess(),
                             reinterpret_cast<const void*>(address),
                             destination, size, &copied) != FALSE &&
           copied == size;
}

bool edit_mode() noexcept {
    // Original MMM address. Read it defensively: an obsolete pointer must not
    // turn a mission compatibility probe into an access violation.
    constexpr std::uintptr_t kEditModeRoot = 0x0076B5ACu;
    std::uintptr_t state = 0;
    if (!read_process_memory(kEditModeRoot, &state, sizeof(state)) || state == 0) {
        return false;
    }

    std::uint8_t enabled = 0;
    if (!read_process_memory(state + 0x84u, &enabled, sizeof(enabled))) {
        return false;
    }
    return enabled != 0;
}


unsigned char __stdcall InputHandler();
struct Context { std::exception_ptr error; };
const char* const messages[] = {
    "",
    "ScriptExtendInterface entered",
    "ScriptExtendInterface called with a null ScriptInterface",
    "previous MMM mission state reset check completed",
    "MMM handoff skipped in edit mode",
    "edit-mode compatibility probe completed",
    "legacy MMM reload-rule bridge deferred pending Jan_B validation",
    "MMM ScriptInterface stored; Worker remains deferred to first input update",
    "MMM ScriptInterface attached",
    "ScriptInit entered",
    "ScriptInit skipped in edit mode",
    "ScriptInit called before ScriptExtendInterface",
    "registering MMM input callback",
    "MMM input callback registered",
    "MMM deferred mission result handed to native mission system",
    "first MMM input update entered",
    "constructing MMM Worker on first input update",
    "MMM Worker construction completed",
    "starting MMM new-game Lua runtime",
    "MMM new-game Lua runtime started",
    "resuming MMM Lua runtime",
    "MMM Lua runtime resumed"
};
A2FO_MmmResult invoke(void* opaque, uint32_t operation, void* argument) noexcept {
    try {
        uintptr_t value = 0;
        switch (operation) {
        case A2FO_MMM_LOG:
            log_line(messages[reinterpret_cast<uintptr_t>(argument)]); break;
        case A2FO_MMM_EDIT_MODE: value = edit_mode(); break;
        case A2FO_MMM_RESET_FINISH: mission_reset_pending_finish(); break;
        case A2FO_MMM_PENDING_FINISH: value = mission_process_pending_finish(); break;
        case A2FO_MMM_SET_INTERFACE:
            setScriptInterface(static_cast<ScriptInterface*>(argument)); break;
        case A2FO_MMM_HAS_INTERFACE: value = getScriptInterface() != nullptr; break;
        case A2FO_MMM_MONITOR:
            getScriptInterface()->MonitorInputControls(InputHandler); break;
        case A2FO_MMM_CREATE_WORKER: value = reinterpret_cast<uintptr_t>(new Worker); break;
        case A2FO_MMM_DESTROY_WORKER: delete static_cast<Worker*>(argument); break;
        case A2FO_MMM_START_WORKER: static_cast<Worker*>(argument)->Startup(); break;
        case A2FO_MMM_RESUME_WORKER: static_cast<Worker*>(argument)->Resume(); break;
        }
        return {value, 0};
    } catch (...) {
        static_cast<Context*>(opaque)->error = std::current_exception();
        return {0, 1};
    }
}
int complete(Context& context, A2FO_MmmResult result) {
    if (result.failed) std::rethrow_exception(context.error);
    return static_cast<int>(result.value);
}
#if !defined(A2FO_BUILTIN_STARTUP)
// The former standalone global unique_ptr also destroyed a live Worker when
// FreeLibrary/process teardown bypassed A2FO_ModuleShutdown. Keep that fallback;
// built-in ownership deliberately has no CRT-time destructor.
struct StandaloneFinalizer {
    ~StandaloneFinalizer() noexcept {
        Context context;
        if (a2fo_rs_mmm_drop_worker(a2fo_rs_mmm_runtime(), invoke, &context).failed)
            std::terminate();
    }
} standalone_finalizer;
#endif
unsigned char __stdcall InputHandler() {
    Context context;
    const auto result = a2fo_rs_mmm_input(a2fo_rs_mmm_runtime(), invoke, &context);
    return static_cast<unsigned char>(complete(context, result));
}
} // namespace
int script_extend_interface(ScriptInterface* param) {
    Context context;
    const auto result = a2fo_rs_mmm_extend(a2fo_rs_mmm_runtime(), invoke, &context, param);
    return complete(context, result);
}
int script_init(void*) {
    Context context;
    const auto result = a2fo_rs_mmm_init(a2fo_rs_mmm_runtime(), invoke, &context);
    return complete(context, result);
}
void mark_mission_unloaded() noexcept {
    a2fo_rs_mmm_mark_unloaded(a2fo_rs_mmm_runtime());
}
#if defined(A2FO_BUILTIN_STARTUP)
bool builtin_active() noexcept { return g_api != nullptr; }
#endif
bool module_init(const A2FO_ModuleApi* api) noexcept {
    g_api = api;
    fault_diagnostics::start();
    log_line("initialized; MMM runtime is dormant until missions/mmm_loader.dll forwards a mission");
    return true;
}
void module_shutdown() noexcept {
    fault_diagnostics::stop();
    g_api = nullptr;
    // Preserve process-lifetime abandonment; next-mission cleanup uses extend.
    a2fo_rs_mmm_abandon_worker(a2fo_rs_mmm_runtime());
}
} } // namespace mmm::a2fo_missions

extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log) {
        return false;
    }

    return mmm::a2fo_missions::module_init(api);
}

extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    mmm::a2fo_missions::module_shutdown();
}

extern "C" __declspec(dllexport) int __cdecl
A2FOMissions_ScriptExtendInterface(mmm::ScriptInterface* param) {
#if defined(A2FO_BUILTIN_STARTUP)
    if (!mmm::a2fo_missions::builtin_active()) return 51;
#endif
    return mmm::a2fo_missions::script_extend_interface(param);
}

extern "C" __declspec(dllexport) int __cdecl A2FOMissions_ScriptInit(void* param) {
#if defined(A2FO_BUILTIN_STARTUP)
    if (!mmm::a2fo_missions::builtin_active()) return 0;
#endif
    return mmm::a2fo_missions::script_init(param);
}

extern "C" __declspec(dllexport) void __cdecl A2FOMissions_MarkMissionUnloaded() {
#if defined(A2FO_BUILTIN_STARTUP)
    if (!mmm::a2fo_missions::builtin_active()) return;
#endif
    mmm::a2fo_missions::mark_mission_unloaded();
}
