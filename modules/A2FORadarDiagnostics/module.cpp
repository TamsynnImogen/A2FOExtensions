#include "../../rust/native_hooks_ffi.h"
// Temporary, opt-in native radar observations. No rendering/filter changes.
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif
#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "../../rust/module_runtime_b_ffi.h"
#include <d3d8.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdint>
#include <atomic>
#include <array>

namespace radar_diagnostics {
constexpr std::uintptr_t craft_call = 0xea547;
constexpr std::uintptr_t map_object = 0xea150;
constexpr std::uintptr_t clear_call = 0x223b99;
constexpr unsigned char expected_clear[] = {0xff,0x90,0x90,0x00,0x00,0x00};
constexpr unsigned char expected_call[] = {0xe8,0x04,0xfc,0xff,0xff};
// Dummy EDX gives the native MSVC thiscall layout: ECX=self and all
// actual arguments on the stack, with callee cleanup.
using ObjectFn = void (__attribute__((fastcall))*)(void*,void*,void*,const float*,int,int);
const A2FO_ModuleApi* api = nullptr;
A2FO_ModuleApi saved_api{};
std::uint8_t* image = nullptr;
ObjectFn original_object = nullptr;
bool installed = false;
bool g_config_enabled = false;
std::atomic<bool> capturing{true};
std::uint64_t activation_operation(void*,std::uint32_t op) noexcept {
#ifdef A2FO_BUILTIN_FEATURE
    return op==0 ? a2fo::builtin_module_activation(A2FO_BUILTIN_FEATURE) : 0;
#else
    return op==1 ? 1 : 0;
#endif
}
std::uint64_t runtime_activation() noexcept {
#ifdef A2FO_BUILTIN_FEATURE
    return a2fo_rs_radar_activation(capturing.load(),installed,nullptr,activation_operation);
#else
    return a2fo_rs_radar_activation(capturing.load(),0,nullptr,activation_operation);
#endif
}
LONG samples = 0;
std::atomic<unsigned long long> objects[2]{}, dots{0};
SRWLOCK seen_lock = SRWLOCK_INIT;
thread_local bool craft_scope = false;
thread_local unsigned craft_dots = 0;
using Seen = A2foRsRadarSeen;
Seen seen[96]{};
std::uint32_t seen_count = 0;

bool readable(const void* pointer, std::size_t length) noexcept {
    auto p = reinterpret_cast<std::uintptr_t>(pointer);
    if (!p || p + length < p) return false;
    const auto end = p + length;
    while (p < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(p),&info,sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD|PAGE_NOACCESS))) return false;
        const DWORD access = info.Protect & 0xff;
        if (access != PAGE_READONLY && access != PAGE_READWRITE && access != PAGE_WRITECOPY &&
            access != PAGE_EXECUTE_READ && access != PAGE_EXECUTE_READWRITE && access != PAGE_EXECUTE_WRITECOPY) return false;
        const auto next = reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize;
        if (next <= p) return false;
        p = next;
    }
    return true;
}
template<class T> T read(const void* pointer) noexcept {
    T value{};
    if (readable(pointer,sizeof(value))) std::memcpy(&value,pointer,sizeof(value));
    return value;
}
template<class T> T field(const void* base, std::uintptr_t offset) noexcept {
    if (!base) return T{};
    return read<T>(reinterpret_cast<const std::uint8_t*>(base)+offset);
}
void log(const char* format, ...) noexcept {
    if (!api || !api->log) return;
    char buffer[1500];
    va_list args;
    va_start(args,format);
    std::vsnprintf(buffer,sizeof(buffer),format,args);
    va_end(args);
    try { api->log("A2FORadarDiagnostics",buffer); } catch (...) {}
}
void camera(float (&rect)[4]) noexcept {
    const void* value = field<void*>(image,0x365010);
    if (value && readable(static_cast<const std::uint8_t*>(value)+0x1b0,sizeof(rect)))
        std::memcpy(rect,static_cast<const std::uint8_t*>(value)+0x1b0,sizeof(rect));
}
struct ClearContext {
    IDirect3DDevice8* native; DWORD count; const D3DRECT* rects;
    DWORD flags; D3DCOLOR color; float z; DWORD stencil;
    D3DVIEWPORT8 viewport{}; HRESULT viewport_hr=E_PENDING,result=E_PENDING;
};
std::uint64_t clear_operation(void* opaque,std::uint32_t op) noexcept {
    auto& c=*static_cast<ClearContext*>(opaque);
    try {
    switch(op) {
    case 0: return InterlockedIncrement(&samples);
    case 1: c.viewport_hr=c.native->GetViewport(&c.viewport);return 1;
    case 2: c.result=c.native->Clear(c.count,c.rects,c.flags,c.color,c.z,c.stencil);return static_cast<std::uint32_t>(c.result);
    case 3: return runtime_activation();
    case 4: ++craft_dots; ++dots;return 1;
    case 5: {
        D3DRECT rect{}; if(c.count && readable(c.rects,sizeof(rect)))std::memcpy(&rect,c.rects,sizeof(rect));
        float cam[4]{}; camera(cam);
        log("dot Clear hr=0x%08lx count=%lu flags=0x%lx color=0x%08lx rect=(%ld,%ld,%ld,%ld) viewport hr=0x%08lx (%lu,%lu,%lu,%lu) camera=(%.3f,%.3f,%.3f,%.3f)",
            static_cast<unsigned long>(c.result),c.count,c.flags,static_cast<unsigned long>(c.color),
            rect.x1,rect.y1,rect.x2,rect.y2,static_cast<unsigned long>(c.viewport_hr),
            c.viewport.X,c.viewport.Y,c.viewport.Width,c.viewport.Height,cam[0],cam[1],cam[2],cam[3]);
        if(samples==1) {
            MEMORY_BASIC_INFORMATION info{}; char provider[MAX_PATH]{};
            void* function=(*reinterpret_cast<void***>(c.native))[36];
            if(VirtualQuery(function,&info,sizeof(info)))
                GetModuleFileNameA(static_cast<HMODULE>(info.AllocationBase),provider,sizeof(provider));
            log("Clear provider=%s function=%p dxwrapper_loaded=%u",provider,function,
                GetModuleHandleA("dxwrapper.dll") ? 1u : 0u);
        }
        return 1;
    }
    default:return 0;
    }
    } catch(...) {return 0;}
}
HRESULT WINAPI clear_hook(IDirect3DDevice8* native, DWORD count, const D3DRECT* rects,
                          DWORD flags, D3DCOLOR color, float z, DWORD stencil) {
    ClearContext context{native,count,rects,flags,color,z,stencil};
    return static_cast<HRESULT>(a2fo_rs_radar_clear(runtime_activation(),craft_scope,&context,clear_operation));
}
struct ObjectContext {
    void* self;void* object_class;const float* position;int team,pass;
    bool previous_scope=false;unsigned previous_dots=0,result_dots=0;
};
std::uint64_t object_operation(void* opaque,std::uint32_t op) noexcept {
    auto& c=*static_cast<ObjectContext*>(opaque);
    try {
    switch(op) {
    case 0:c.previous_scope=craft_scope;c.previous_dots=craft_dots;craft_scope=true;craft_dots=0;return 1;
    case 1:original_object(c.self,nullptr,c.object_class,c.position,c.team,c.pass);return 1;
    case 2:c.result_dots=craft_dots;craft_dots=c.previous_dots;craft_scope=c.previous_scope;return 1;
    case 3:return runtime_activation();
    case 4: {
    const auto object_class=c.object_class;
    const auto position=c.position;
    const auto pass=c.pass,team=c.team;
    const auto result_dots=c.result_dots;
    const auto which = pass ? 1u : 0u;
    ++objects[which];
    AcquireSRWLockExclusive(&seen_lock);
    const bool fresh = a2fo_rs_radar_mark_seen(seen,&seen_count,object_class,pass);
    ReleaseSRWLockExclusive(&seen_lock);
    if (fresh) {
        float p[3]{}; if(readable(position,sizeof(p)))std::memcpy(p,position,sizeof(p));
        log("craft class=%p pass=%d team=%d class18c=%lu icon=%p station=%u filters=(%lu,%lu) pos=(%.1f,%.1f,%.1f) map=(%.1f,%.1f,%.1f,%.1f) dots=%u",
            object_class,pass,team,static_cast<unsigned long>(field<DWORD>(object_class,0x18c)),
            field<void*>(object_class,0x190),static_cast<unsigned>(field<BYTE>(object_class,0x20d)),
            field<DWORD>(image,0x36386c),field<DWORD>(image,0x363870),p[0],p[1],p[2],
            static_cast<double>(field<float>(image,0x339590)),static_cast<double>(field<float>(image,0x33959c)),
            static_cast<double>(field<float>(image,0x339598)),static_cast<double>(field<float>(image,0x3395a4)),result_dots);
    }
    if ((objects[0].load()+objects[1].load())%30000==0)
        log("totals craft_pass0=%llu craft_pass1=%llu rectangle_dots=%llu filters=(%lu,%lu)",
            objects[0].load(),objects[1].load(),dots.load(),field<DWORD>(image,0x36386c),field<DWORD>(image,0x363870));
    return 1;
    }
    default:return 0;
    }
    } catch(...) {return 0;}
}
void __attribute__((fastcall)) object_hook(void* self, void*, void* object_class,
                                         const float* position, int team, int pass) {
    ObjectContext context{self,object_class,position,team,pass};
    a2fo_rs_radar_object(runtime_activation(),&context,object_operation);
}
bool write(void* destination, const void* source, std::size_t length) noexcept {
    A2foRsPatchReport result{};
    if (!a2fo_rs_windows_write_report(destination,source,length,&result)) return false;
    // Written code remains installed even when restoring protection fails.
    if (!result.restored) log("Page protection restore failed: %lu",
        static_cast<unsigned long>(result.restore_error));
    return true;
}

struct InitContext {const A2FO_ModuleApi* value;};
std::uint64_t init_operation(void* opaque,std::uint32_t op) noexcept {
    const auto* value=static_cast<InitContext*>(opaque)->value;
    try {
    switch(op) {
    case 0: {
        saved_api.log=value->log;saved_api.armada_module=value->armada_module;
        saved_api.patch_call=value->patch_call;saved_api.root_directory=value->root_directory;
        api=&saved_api;
        const char* root=api->root_directory();char path[32768];
        if(!root || std::snprintf(path,sizeof(path),"%s\\A2FORadarDiagnostics.ini",root)>=static_cast<int>(sizeof(path)))return 0;
        g_config_enabled=GetPrivateProfileIntA("Diagnostics","Enabled",0,path)!=0;
        return g_config_enabled?2:1;
    }
    case 1:
        image=static_cast<std::uint8_t*>(api->armada_module());
        return a2fo::supported_armada::identify(reinterpret_cast<HMODULE>(image))!=a2fo::supported_armada::Identity::unsupported;
    case 2: {HMODULE pinned=nullptr;return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<const char*>(&A2FO_ModuleInit),&pinned)!=0;}
    case 3: {HMODULE pinned=nullptr;return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<const char*>(api->log),&pinned)!=0;}
    case 4: {
    const unsigned char prologue[]={0x55,0x8b,0xec,0x83,0xec,0x44};
    original_object=reinterpret_cast<ObjectFn>(image+map_object);
    if(std::memcmp(image+craft_call,expected_call,sizeof(expected_call)) ||
       std::memcmp(image+map_object,prologue,sizeof(prologue)) ||
       std::memcmp(image+clear_call,expected_clear,sizeof(expected_clear)))return 0;
    return 1;
    }
    case 5:return api->patch_call(image+clear_call,reinterpret_cast<void*>(&clear_hook),expected_clear,6);
    case 6:return api->patch_call(image+craft_call,reinterpret_cast<void*>(&object_hook),expected_call,5);
    case 7:return write(image+clear_call,expected_clear,6);
    case 8:installed=true;capturing.store(false);log("Craft patch and Clear rollback failed; retaining pass-through callback, capture disabled");return 1;
    case 9:installed=true;return 1;
    case 10:log("Disabled; no hooks installed");return 1;
    case 11:log("Rejected unsupported image; no hooks installed");return 1;
    case 12:log("Changed radar sites or failed patch; diagnostic session rejected");return 1;
    case 13:if(capturing.load())log("Enabled: native craft marker call and rectangle Clear observed; rendering and filters unchanged");return 1;
    default:return 0;
    }
    } catch(...) {return 0;}
}
}
extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* value) {
    using namespace radar_diagnostics;
    if (!a2fo_rs_radar_api_valid(value!=nullptr,value?value->api_version:0,
        A2FO_MODULE_API_VERSION,value?value->struct_size:0,A2FO_MODULE_API_V4_BASE_SIZE,
        value&&value->log,value&&value->armada_module,value&&value->patch_call,
        value&&value->root_directory)) return false;
    InitContext context{value};
    return a2fo_rs_radar_init(1,installed,&context,init_operation)!=0;
}
extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    using namespace radar_diagnostics;
    // Hooks, callback storage and core logger are pinned until process exit.
    // Do not rewrite instructions under potentially active rendering threads
    // or clear pointers used by an in-flight callback. Future calls forward
    // without collecting samples. Full removal requires a process restart.
    const auto off=[](void*,std::uint32_t) noexcept ->std::uint64_t {
        capturing.store(false);return 1;
    };
    a2fo_rs_radar_shutdown(nullptr,off);
}
