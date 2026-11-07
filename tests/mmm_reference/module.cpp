/*
 * A2FOMissions
 *
 * Integrates Megadroid's Mission Mod (MMM) runtime into the A2FOExtensions
 * native module set.  The legacy missions/mmm_loader.dll is now only a tiny
 * compatibility shim; Armada's ScriptInit/ScriptExtendInterface callbacks are
 * forwarded here.
 *
 * MMM remains vendored under third_party/mmm.  We deliberately do not compile
 * mmm_loader/Main.cpp: its process-global Worker constructed too early when
 * used as an A2FO module.  Instead this file owns the Worker lazily and keeps
 * the original MMM callback sequence.
 */

#include "../../sdk/include/a2fo_module_api.h"

#include "mmm_loader/Worker.h"
#include "mmm_armada/ScriptInterface.h"
#include "mmm_armada/Types.h"
#include "mmm_armada/Game_Internal.h"
#include "mmm_armada/Mission_Internal.h"

#include <windows.h>

#include <cstdint>
#include <memory>

#include "fault_diagnostics.hpp"

namespace mmm {
namespace a2fo_missions {

namespace {

const A2FO_ModuleApi* g_api = nullptr;
#if defined(A2FO_BUILTIN_STARTUP)
// Core CRT teardown can run after Armada has invalidated the script interface.
// Retain the owner itself so Worker is only destroyed on a mission reset.
std::unique_ptr<Worker>* g_worker_owner = nullptr;
std::unique_ptr<Worker>& worker_owner() {
    if (!g_worker_owner) g_worker_owner = new std::unique_ptr<Worker>();
    return *g_worker_owner;
}
std::unique_ptr<Worker>* existing_worker_owner() noexcept { return g_worker_owner; }
#else
std::unique_ptr<Worker> g_worker;
std::unique_ptr<Worker>& worker_owner() noexcept { return g_worker; }
std::unique_ptr<Worker>* existing_worker_owner() noexcept { return &g_worker; }
#endif
bool g_first_update = true;
bool g_is_new_game = false;
volatile LONG g_mission_unloaded = 0;

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

void reset_for_next_mission(ScriptInterface* next_interface) {
    // mmm_loader.dll marks the previous mission as unloaded from its very small
    // DLL detach path.  Defer real cleanup until Armada enters the next mission
    // so no complex MMM teardown executes while the Windows loader lock is held.
    if (InterlockedExchange(&g_mission_unloaded, 0) == 0) {
        return;
    }

    mission_reset_pending_finish();

    // Worker::~Worker() uses getScriptInterface().  Point it at the newly valid
    // interface before tearing down the old runtime, then rebuild the MMM worker.
    setScriptInterface(next_interface);
    auto* worker = existing_worker_owner();
    if (worker && *worker) {
        worker->reset();
    }
    g_first_update = true;
    g_is_new_game = false;
}

unsigned char __stdcall InputHandler() {
    // Process queued mission completion outside Lua/Timer callbacks. If this
    // fires, return immediately and let Armada perform the native transition.
    if (mission_process_pending_finish()) {
        log_line("MMM deferred mission result handed to native mission system");
        return 1;
    }

    if (g_first_update) {
        log_line("first MMM input update entered");
        auto& worker = worker_owner();
        if (!worker) {
            log_line("constructing MMM Worker on first input update");
            worker = std::make_unique<Worker>();
            log_line("MMM Worker construction completed");
        }

        if (g_is_new_game) {
            log_line("starting MMM new-game Lua runtime");
            worker->Startup();
            log_line("MMM new-game Lua runtime started");
        } else {
            log_line("resuming MMM Lua runtime");
            worker->Resume();
            log_line("MMM Lua runtime resumed");
        }
        g_first_update = false;
    }

    // MMM historically returns 1; returning 0 breaks Armada's input monitor.
    return 1;
}

int loaderFunction() {
    if (getScriptInterface()) {
        getScriptInterface()->MonitorInputControls(InputHandler);
    }
    return 0;
}

void loaderStub() {}

void SetupReload(ScriptInterface* param) {
    constexpr std::size_t Address_WorldCharacter = 0x007349c8;
    constexpr std::size_t Function_GetIIIE = 0x00403250;
    constexpr std::size_t Function_CreateRule = 0x00405fd0;
    constexpr std::size_t Function_AddDependentRule = 0x004055e0;
    constexpr std::size_t Function_AddRuleToAffectedList = 0x00405a00;

    using fun1 = int (*)();
    using fun2 = void (*)();

    character* world = reinterpret_cast<character*>(Address_WorldCharacter);
    IIIE& iiie = (world->*memory_function<IIIE& (character::*)()>(Function_GetIIIE))();

    rule* reloader =
        (iiie.*memory_function<rule* (IIIE::*)(fun1, fun2, const char*)>(Function_CreateRule))(
            loaderFunction, loaderStub, "reloader");

    hot_int* hotint = param->ScriptCreate_hot_int("loadHot");
    (hotint->*memory_function<void (hot_int::*)(rule*)>(Function_AddDependentRule))(reloader);
    (iiie.*memory_function<void (IIIE::*)(rule*)>(Function_AddRuleToAffectedList))(reloader);
}

}  // namespace

int script_extend_interface(ScriptInterface* param) {
    log_line("ScriptExtendInterface entered");
    if (!param) {
        log_line("ScriptExtendInterface called with a null ScriptInterface");
        return 51;
    }

    reset_for_next_mission(param);
    log_line("previous MMM mission state reset check completed");

    if (edit_mode()) {
        log_line("MMM handoff skipped in edit mode");
        return 51;
    }
    log_line("edit-mode compatibility probe completed");

    // v13 runtime bring-up: SetupReload reaches directly into Armada's IIIE/rule
    // internals. Keep the code available, but do not execute it until those
    // five legacy addresses have been signature-validated for the supported
    // Jan_B image. New-game ScriptInit does not require this reload helper.
    log_line("legacy MMM reload-rule bridge deferred pending Jan_B validation");

    setScriptInterface(param);
    log_line("MMM ScriptInterface stored; Worker remains deferred to first input update");

    g_first_update = true;
    g_is_new_game = false;
    log_line("MMM ScriptInterface attached");
    return 51;
}

int script_init(void*) {
    log_line("ScriptInit entered");
    mission_reset_pending_finish();
    if (edit_mode()) {
        log_line("ScriptInit skipped in edit mode");
        return 51;
    }

    ScriptInterface* script_interface = getScriptInterface();
    if (!script_interface) {
        log_line("ScriptInit called before ScriptExtendInterface");
        return 0;
    }

    log_line("registering MMM input callback");
    script_interface->MonitorInputControls(InputHandler);
    log_line("MMM input callback registered");
    g_is_new_game = true;
    return 0;
}

void mark_mission_unloaded() noexcept {
    // Must remain loader-lock safe: the shim calls this from DLL_PROCESS_DETACH.
    InterlockedExchange(&g_mission_unloaded, 1);
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
    // A normal mission unload is cleaned at the next ScriptExtendInterface.
    // During process/module shutdown avoid forcing MMM's heavyweight destructor
    // through an unknown game teardown state; Windows will reclaim the process.
    g_api = nullptr;
    auto* worker = existing_worker_owner();
    if (worker && *worker) {
        (void)worker->release();
    }
}

}  // namespace a2fo_missions
}  // namespace mmm

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
