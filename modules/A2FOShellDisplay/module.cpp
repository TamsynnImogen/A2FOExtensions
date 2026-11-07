// The primary shell keeps its non-exclusive D3D device, but uses the chosen
// primary display mode and either a framed or borderless native VCL window.
#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/module_runtime_a3_ffi.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "../../sdk/include/a2fo_supported_fleetops.hpp"
#include "../refit_common/fleetops_profile.hpp"
#include "policy.hpp"
#include "canvas.hpp"
#include "presentation.hpp"
#include "bink_video.hpp"
#include "native_intro.hpp"
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <exception>

extern "C" void __cdecl a2fo_shell_set_position(void*, void*, int);

namespace {
using namespace a2fo::shell_display;
constexpr char kName[] = "A2FOShellDisplay";
constexpr std::uintptr_t kActivate = 0x1d3520, kWindow = 0x1d0310;
constexpr std::uintptr_t kShellLookup = 0x22c4e0, kFindMode = 0x226bf0;
constexpr std::uint8_t kActivateBytes[] = {0x55,0x8b,0xec,0x51,0x8b,0x55,0x0c};
constexpr std::uint8_t kWindowBytes[] = {0x55,0x8b,0xec,0x83,0xc4,0xb4};
constexpr std::uint8_t kLookupBytes[] = {0xe8,0x0b,0xa7,0xff,0xff};
const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr, g_fo = nullptr;
A2FO_InlineHook g_activate{}, g_window{};
std::atomic<bool> g_ready{false};
struct Scope {
    void* device = nullptr;
    void* selected = nullptr;
    Mode mode{};
    bool borderless = false;
};
// Native callbacks are synchronous. A thread-local scope excludes other
// displays, gameplay, editor and unrelated concurrent mode lookups.
thread_local Scope g_scope;
thread_local Scope g_applied;
thread_local unsigned g_scope_diagnostics = 0;
thread_local std::exception_ptr* g_shell_error_slot = nullptr;
std::exception_ptr g_shell_init_error;
using Activate = int (__fastcall*)(void*, void*, int, int, bool);
using FindMode = void* (__fastcall*)(void*, void*, int, int, int);
using Resize = void (__cdecl*)(HWND*, int, int, bool, unsigned);
std::atomic<Activate> g_activate_next{nullptr};
std::atomic<Resize> g_resize_next{nullptr};
template<class T> T await_gateway(const std::atomic<T>& gateway) {
    // The core writes the jump before returning the gateway to its caller.
    // An early callback may arrive in that short publication interval.
    T next;
    while (!(next = gateway.load(std::memory_order_acquire))) Sleep(0);
    return next;
}

void* at(HMODULE image, std::uintptr_t rva) {
    return reinterpret_cast<std::uint8_t*>(image) + rva;
}
void log(const char* message) { if (g_api && g_api->log) g_api->log(kName, message); }
bool readable(const void* value, std::size_t size) {
    if (!value || !size) return false;
    auto begin = reinterpret_cast<std::uintptr_t>(value);
    const auto end = begin + size;
    if (end < begin) return false;
    while (begin < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(begin), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const DWORD protection = info.Protect & 0xff;
        if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
            protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
            protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY) return false;
        const auto next = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (next <= begin) return false;
        begin = next;
    }
    return true;
}
template<class T> T read(const void* value, std::size_t offset = 0) {
    T result{};
    if (value) {
        const auto* source = static_cast<const std::uint8_t*>(value) + offset;
        if (readable(source, sizeof(T))) std::memcpy(&result, source, sizeof(T));
    }
    return result;
}
template<std::size_t N> bool matches(void* address, const std::uint8_t (&bytes)[N]) {
    return readable(address, N) && std::memcmp(address, bytes, N) == 0;
}

bool known_fleetops() {
    // Whole-file CRC and length pin the original supplied DLLs; the approved
    // profile admits the same layout family. Live instruction checks below
    // also catch competing hooks. See README hashes.
    char path[32768]{};
    const DWORD length = GetModuleFileNameA(g_fo, path, sizeof(path));
    if (!length || length >= sizeof(path)) return false;
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &info) ||
        info.nFileSizeHigh || info.nFileSizeLow != 3043840) return false;
    std::uint32_t crc = 0;
    const bool known_crc =
        a2fo::supported_armada::file_range_crc32(g_fo, 0, info.nFileSizeLow, crc) &&
        (crc == 0x07bd1e4f || crc == 0x3ed28fbe || crc == 0x46a0dd96 ||
         (crc == 0x7cd6661e && a2fo::fleetops_profile::october_roots_file(path)));
    return known_crc || a2fo::supported_fleetops::approved_profile(g_fo);
}

A2foRsShellScope shell_scope(const Scope& source) noexcept {
    return {reinterpret_cast<std::uintptr_t>(source.device),
            reinterpret_cast<std::uintptr_t>(source.selected),
            source.mode.width,source.mode.height,source.mode.depth,
            static_cast<std::uint8_t>(source.borderless)};
}
Scope native_scope(const A2foRsShellScope& source) noexcept {
    Scope result{};
    result.device=reinterpret_cast<void*>(source.device);
    result.selected=reinterpret_cast<void*>(source.selected);
    result.mode={source.width,source.height,source.depth};
    result.borderless=source.borderless!=0;
    return result;
}
A2foRsShellCandidate shell_candidate(void* engine,int display,int mode) {
    A2foRsShellCandidate candidate{};
    void* profile=read<void*>(read<void*>(read<void*>(at(g_fo,0x212e08))));
    candidate.profile_present=profile && readable(static_cast<std::uint8_t*>(profile)+0x84,1);
    if(!candidate.profile_present)return candidate;
    candidate.editor=read<std::uint8_t>(profile,0x84)!=0;
    if(!a2fo_rs_runtime_shell_scope_eligible(g_ready,display,mode,
        candidate.profile_present,candidate.editor))return candidate;
    void* config=read<void*>(engine,0x104);
    candidate.config_readable=readable(config,0x28);
    if(!candidate.config_readable)return candidate;
    candidate.scope.selected=reinterpret_cast<std::uintptr_t>(read<void*>(config,0x24));
    candidate.scope.device=reinterpret_cast<std::uintptr_t>(read<void*>(config,0x1c));
    if(!candidate.scope.device || !readable(reinterpret_cast<void*>(candidate.scope.selected),sizeof(Mode)))
        return candidate;
    const Mode selected=read<Mode>(reinterpret_cast<void*>(candidate.scope.selected));
    candidate.scope.width=selected.width;
    candidate.scope.height=selected.height;
    candidate.scope.depth=selected.depth;
    candidate.scope.borderless=read<std::uint8_t>(config,1)!=0;
    candidate.mode_readable=1;
    return candidate;
}
void* __fastcall shell_lookup(void* device,void*,int width,int height,int depth) {
    if(a2fo_rs_runtime_shell_lookup_route(g_ready,
        reinterpret_cast<std::uintptr_t>(g_scope.selected),
        reinterpret_cast<std::uintptr_t>(g_scope.device),
        reinterpret_cast<std::uintptr_t>(device)))return g_scope.selected;
    return reinterpret_cast<FindMode>(at(g_armada,kFindMode))(
        device,nullptr,width,height,depth);
}
struct ActivateContext {
    void* engine;int display,mode;bool force;Scope previous;
};
std::intptr_t activate_step(void* opaque,std::uint32_t operation,
                            const A2foRsShellScope* selected,
                            std::intptr_t value) noexcept {
    auto& context=*static_cast<ActivateContext*>(opaque);
    if(g_shell_error_slot && *g_shell_error_slot)return 0;
    try {
        switch(operation){
        case 0:g_scope=native_scope(*selected);return 0;
        case 1:return g_scope_diagnostics++;
        case 2:{
            void* config=read<void*>(context.engine,0x104);
            const Mode mode=read<Mode>(read<void*>(config,0x24));
            char message[160]{};
            std::snprintf(message,sizeof(message),
                "Shell activation left native: editor or unavailable primary mode; selection=%dx%dx%d",
                mode.width,mode.height,mode.depth);
            log(message);return 0;
        }
        case 3:canvas::active_mode(g_scope.selected?&g_scope.mode:nullptr);return 0;
        case 4:presentation::publish(g_scope.mode,g_scope.borderless,nullptr,
            g_scope.mode.width,g_scope.mode.height);return 0;
        case 5:return await_gateway(g_activate_next)(context.engine,nullptr,
            context.display,context.mode,value!=0);
        case 6:g_applied=value==0?g_scope:Scope{};return 0;
        case 7:presentation::clear();return 0;
        case 8:canvas::active_mode(g_applied.selected?&g_applied.mode:nullptr);return 0;
        case 9:g_scope=context.previous;return 0;
        default:return 0;
        }
    } catch (...) {if(g_shell_error_slot)*g_shell_error_slot=std::current_exception();return 0;}
}
int __fastcall activate(void* engine,void*,int display,int mode,bool force) {
    ActivateContext context{engine,display,mode,force,g_scope};
    std::exception_ptr callback_error;
    auto* previous_error_slot=g_shell_error_slot;
    g_shell_error_slot=&callback_error;
    const auto candidate=g_ready?shell_candidate(engine,display,mode):A2foRsShellCandidate{};
    const int result=a2fo_rs_runtime_shell_activate(g_ready,display,mode,force,
        candidate,shell_scope(g_applied),&activate_step,&context);
    g_shell_error_slot=previous_error_slot;
    if(callback_error)std::rethrow_exception(callback_error);
    return result;
}

HMONITOR adapter_monitor(unsigned adapter) {
    const bool d3d9 = reinterpret_cast<bool (__cdecl*)()>(at(g_fo, 0x1df754))();
    // Same native D3D slot and vtable method used by FO's resize callback.
    void* d3d = read<void*>(read<void*>(read<void*>(at(g_fo, d3d9 ? 0x213108 : 0x2125cc))));
    void* table = read<void*>(d3d);
    void* method = read<void*>(table, d3d9 ? 0x3c : 0x38);
    return method ? reinterpret_cast<HMONITOR (__stdcall*)(void*, unsigned)>(method)(d3d, adapter) : nullptr;
}

struct ResizeContext {
    HWND* window;int width,height;bool fullscreen;unsigned adapter;
    bool gateway_fullscreen=false;
    void* form=nullptr;
    MONITORINFO monitor{};
    bool have_monitor=false;
    RECT outer{},client{};
};
std::uintptr_t resize_step(void* opaque,std::uint32_t operation,
                           std::intptr_t first,std::intptr_t second,
                           A2foRsShellRect* output) noexcept {
    auto& context=*static_cast<ResizeContext*>(opaque);
    if(g_shell_error_slot && *g_shell_error_slot)return 0;
    try {
        switch(operation){
        case 0:
            await_gateway(g_resize_next)(context.window,static_cast<int>(first),
                static_cast<int>(second),context.gateway_fullscreen,context.adapter);
            return 0;
        case 1:
            context.form=read<void*>(at(g_fo,0x24485c));
            return reinterpret_cast<std::uintptr_t>(context.form);
        case 2:{
            const HMONITOR handle=adapter_monitor(context.adapter);
            context.monitor.cbSize=sizeof(context.monitor);
            context.have_monitor=handle && GetMonitorInfoA(handle,&context.monitor);
            if(context.have_monitor && output)
                *output={context.monitor.rcMonitor.left,context.monitor.rcMonitor.top,
                         context.monitor.rcMonitor.right,context.monitor.rcMonitor.bottom};
            return context.have_monitor;
        }
        case 3:
            if(context.have_monitor && output)
                *output={context.monitor.rcWork.left,context.monitor.rcWork.top,
                         context.monitor.rcWork.right,context.monitor.rcWork.bottom};
            return context.have_monitor;
        case 4:
            a2fo_shell_set_position(at(g_fo,0xad390),context.form,static_cast<int>(first));
            a2fo_shell_set_position(at(g_fo,0xad35c),context.form,static_cast<int>(second));
            return 0;
        case 5:
            if(!GetWindowRect(*context.window,&context.outer))return 0;
            if(output)*output={context.outer.left,context.outer.top,
                context.outer.right,context.outer.bottom};
            return 1;
        case 6:
            a2fo_shell_set_position(at(g_fo,0xb2708),context.form,static_cast<int>(first));
            a2fo_shell_set_position(at(g_fo,0xb2744),context.form,static_cast<int>(second));
            return 0;
        case 7:
            GetClientRect(*context.window,&context.client);
            if(output)*output={context.client.left,context.client.top,
                context.client.right,context.client.bottom};
            return 1;
        case 8:
            presentation::publish(g_scope.mode,context.gateway_fullscreen,
                *context.window,static_cast<int>(first),static_cast<int>(second));
            return 0;
        case 9:{
            char message[192]{};
            std::snprintf(message,sizeof(message),
                "Shell window: %s, requested=%dx%d, device=%dx%d, client=%ldx%ld, adapter=%u",
                context.gateway_fullscreen?"borderless":"windowed",
                g_scope.mode.width,g_scope.mode.height,
                static_cast<int>(first),static_cast<int>(second),
                context.client.right-context.client.left,
                context.client.bottom-context.client.top,context.adapter);
            log(message);return 0;
        }
        case 10:context.gateway_fullscreen=first!=0;return 0;
        case 11:return context.window && *context.window;
        default:return 0;
        }
    } catch (...) {if(g_shell_error_slot)*g_shell_error_slot=std::current_exception();return 0;}
}
void __cdecl resize(HWND* window,int width,int height,bool fullscreen,unsigned adapter) {
    ResizeContext context{window,width,height,fullscreen,adapter};
    std::exception_ptr callback_error;
    auto* previous_error_slot=g_shell_error_slot;
    g_shell_error_slot=&callback_error;
    a2fo_rs_runtime_shell_resize(g_ready,shell_scope(g_scope),
        window != nullptr,width,height,fullscreen,adapter,&resize_step,&context);
    g_shell_error_slot=previous_error_slot;
    if(callback_error)std::rethrow_exception(callback_error);
}

std::uint8_t shell_init_step(void*,std::uint32_t operation) noexcept {
    if(g_shell_init_error)return 0;
    try {
        switch(operation){
        case 0:
            g_armada=static_cast<HMODULE>(g_api->armada_module());
            g_fo=static_cast<HMODULE>(g_api->fleetops_module());
            return g_armada && g_fo;
        case 1:return known_fleetops();
        case 2:return a2fo::supported_armada::identify(g_armada)!=
            a2fo::supported_armada::Identity::unsupported;
        case 3:return matches(at(g_fo,kActivate),kActivateBytes) &&
            matches(at(g_fo,kWindow),kWindowBytes) &&
            matches(at(g_armada,kShellLookup),kLookupBytes);
        case 4:{
            const auto* block=static_cast<const std::uint8_t*>(at(g_armada,0x22c4cc));
            const std::uint8_t prefix[]={0x39,0x55,0x0c,0x75,0x17,0x8b,0x4d,0xf8,0x6a};
            const unsigned depth=read<std::uint8_t>(block,9);
            const int height=read<int>(block,11),width=read<int>(block,16);
            return matches(const_cast<std::uint8_t*>(block),prefix) &&
                (depth==16||depth==32) && read<std::uint8_t>(block,10)==0x68 &&
                read<std::uint8_t>(block,15)==0x68 &&
                ((width==800&&height==600)||(width==1200&&height==900));
        }
        case 5:{
            const auto* call=static_cast<const std::uint8_t*>(at(g_armada,0x22c70b));
            const void* target=call+5+read<std::int32_t>(call,1);
            return read<std::uint8_t>(call)==0xe8 &&
                (target==at(g_fo,kActivate)||target==at(g_armada,0x22c3b0));
        }
        case 6:return g_api->install_inline_hook(at(g_fo,kActivate),
            reinterpret_cast<void*>(&activate),sizeof(kActivateBytes),kActivateBytes,&g_activate);
        case 7:g_activate_next.store(reinterpret_cast<Activate>(g_activate.gateway),
            std::memory_order_release);return 1;
        case 8:return g_api->install_inline_hook(at(g_fo,kWindow),
            reinterpret_cast<void*>(&resize),sizeof(kWindowBytes),kWindowBytes,&g_window);
        case 9:g_resize_next.store(reinterpret_cast<Resize>(g_window.gateway),
            std::memory_order_release);return 1;
        case 10:return g_resize_next.load(std::memory_order_acquire)!=nullptr;
        case 11:return g_api->patch_call(at(g_armada,kShellLookup),
            reinterpret_cast<void*>(&shell_lookup),kLookupBytes,sizeof(kLookupBytes));
        case 12:g_ready=true;return 1;
        case 13:return canvas::initialize(g_api,g_armada,g_fo);
        case 14:return a2fo::shell_display::initialize_bink_video_scaling(g_api,g_armada);
        case 15:return native_intro::initialize(g_api,g_armada,g_fo);
        case 16:native_intro::shutdown();return 1;
        case 17:a2fo::shell_display::shutdown_bink_video_scaling();return 1;
        case 18:presentation::clear();return 1;
        case 19:canvas::shutdown();return 1;
        case 20:g_ready=false;return 1;
        default:return 0;
        }
    } catch (...) {g_shell_init_error=std::current_exception();return 0;}
}
void shell_init_log(void*,std::uint32_t code) noexcept {
    if(g_shell_init_error)return;
    try {
        log(code==0?"Unsupported Armada/FleetOpsHook binary identity; no patches installed":
            code==1?"Shell hook signature mismatch; no patches installed":
            code==2?"Native shell resolution block mismatch; no patches installed":
            code==3?"Primary display CALL has another owner; no patches installed":
            code==4?"Hook installation incomplete; module retained with runtime disabled":
            code==5?"Full-window canvas unavailable: import/signature checks failed; working resolution/border hooks remain enabled":
            code==6?"Intro/movie scaling unavailable; shell resolution hooks remain enabled":
            code==7?"Native engine frame fitting unavailable; saved intro window resolution remains enabled":
                    "Initialized v16: inherited Admiral-log panes and secondary shell taskbar suppression");
    } catch (...) {g_shell_init_error=std::current_exception();}
}
} // namespace

extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    const bool valid=api && api->struct_size>=A2FO_MODULE_API_V4_BASE_SIZE &&
        api->api_version==A2FO_MODULE_API_VERSION && api->log &&
        api->armada_module && api->fleetops_module &&
        api->install_inline_hook && api->patch_call;
    if(!valid)return false;
    g_api=api;
    g_shell_init_error=nullptr;
    const bool result=a2fo_rs_runtime_shell_init(valid,&shell_init_step,
        &shell_init_log,nullptr)!=0;
    if(g_shell_init_error)std::rethrow_exception(g_shell_init_error);
    return result;
}
extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    g_shell_init_error=nullptr;
    a2fo_rs_runtime_shell_shutdown(&shell_init_step,nullptr);
    if(g_shell_init_error)std::rethrow_exception(g_shell_init_error);
}
