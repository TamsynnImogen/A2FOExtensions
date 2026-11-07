// Additive D3D8 observation, deliberately independent of renderer effects.
// No reference-project implementation code is incorporated here.
#include "observer.hpp"
#include "../../rust/game_backend_ffi.h"
#include "game/factory.hpp"
#include "../../core/extension_roots.hpp"
#include "../d3d11/probe_bridge.hpp"
#include "../shaders/d3d8_tokens.hpp"
#include <d3d8.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <sstream>
#include <type_traits>
#include <vector>

namespace a2fo::renderer {
namespace {
enum class Kind { IDirect3D8, IDirect3DDevice8, IDirect3DTexture8,
    IDirect3DVolumeTexture8, IDirect3DCubeTexture8, IDirect3DVertexBuffer8,
    IDirect3DIndexBuffer8, IDirect3DSurface8, IDirect3DVolume8, IDirect3DSwapChain8 };
struct Table { void* const* methods = nullptr; size_t count = 0; };
static Table hooks_for(Kind);
struct Dispatch {
    void** original = nullptr;
    std::vector<void*> replacement;
    Kind kind{};
};
struct Record {
    void* object = nullptr;
    void** original = nullptr;
    std::shared_ptr<Dispatch> dispatch;
    Kind kind{};
    unsigned id = 0;
    D3DPRESENT_PARAMETERS presentation{};
    HWND focus = nullptr;
};
struct Count { unsigned long long calls = 0, failures = 0, microseconds = 0; };
struct Signature { unsigned long long count = 0; };
using Factory = IDirect3D8* (WINAPI*)(UINT);
struct Runtime {
    SRWLOCK lock = SRWLOCK_INIT;
    std::map<void*, std::shared_ptr<Record>> objects;
    std::map<std::pair<void**,Kind>, std::shared_ptr<Dispatch>> dispatches;
    std::map<void**, std::shared_ptr<Dispatch>> cloned_tables;
    std::map<std::string, Count> methods;
    std::map<std::string, Signature> signatures;
    RendererLog log = nullptr;
    Factory factory = nullptr;
    bool trace = false, probe = false, debug = false, game_test = false;
    bool game_ready = false;
    game::Configuration game_configuration;
    std::atomic<bool> probe_attempted{false};
    unsigned next_id = 0, events = 0, max_events = 8192, frames = 0;
    unsigned shader_captures = 0;
    bool shader_limit_reported = false;
    unsigned long long dropped_signatures = 0;
    unsigned long long dropped_objects = 0;
    std::string trace_path;
    FILE* file = nullptr;
    bool trace_open_attempted = false, trace_limit_reported = false;
    size_t trace_bytes = 0;
    LARGE_INTEGER frequency{};
};
// Process lifetime: cloned vtables must outlive all provider-owned COM refs.
// There is intentionally no unload path or DllMain COM cleanup.
Runtime* runtime = nullptr;
struct Lock {
    Lock() { AcquireSRWLockExclusive(&runtime->lock); }
    ~Lock() { ReleaseSRWLockExclusive(&runtime->lock); }
};

template<class T> void* address(T function) noexcept {
    void* result = nullptr;
    static_assert(sizeof(result) == sizeof(function));
    std::memcpy(&result, &function, sizeof(result));
    return result;
}
template<class T> T function_at(void* pointer) noexcept {
    T result = nullptr;
    static_assert(sizeof(result) == sizeof(pointer));
    std::memcpy(&result, &pointer, sizeof(result));
    return result;
}
bool readable(const void* pointer, size_t size) noexcept {
    if (!pointer || !size) return false;
    const auto start = reinterpret_cast<std::uintptr_t>(pointer);
    if (start + size < start) return false;
    auto cursor = start;
    while (cursor < start + size) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        const DWORD access = info.Protect & 0xff;
        if (access != PAGE_READONLY && access != PAGE_READWRITE && access != PAGE_WRITECOPY &&
            access != PAGE_EXECUTE_READ && access != PAGE_EXECUTE_READWRITE && access != PAGE_EXECUTE_WRITECOPY) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (end <= cursor) return false;
        cursor = end;
    }
    return true;
}
template<class T> bool copy_value(const T* source, T& dest) noexcept {
    if (!readable(source, sizeof(T))) return false;
    std::memcpy(&dest, source, sizeof(T));
    return true;
}
void message(const std::string& line) noexcept {
    try { if (runtime && runtime->log) runtime->log(line); } catch (...) {}
}
void message(const char* line) noexcept {
    try { if (runtime && runtime->log) runtime->log(std::string(line)); } catch (...) {}
}
bool emit_record_locked(const char* text, size_t length) noexcept {
    if (!runtime->file) return false;
    constexpr size_t max_bytes = 4 * 1024 * 1024;
    if (a2fo_observer_trace_file_limit(runtime->trace_bytes,length,max_bytes,128)) {
        if (!runtime->trace_limit_reported) {
            std::fputs("{\"event\":\"file_limit\",\"bytes_limit\":4194304,\"counters_continue_in_memory\":true}\n",runtime->file);
            runtime->trace_limit_reported = true;
        }
        return false;
    }
    std::fwrite(text, 1, length, runtime->file);
    runtime->trace_bytes += length;
    return true;
}
template<class... Args> void emit_locked(const char* format, Args... args) noexcept {
    char text[2048];
    const int length = std::snprintf(text, sizeof(text), format, args...);
    if (length <= 0 || static_cast<size_t>(length) >= sizeof(text)) return;
    emit_record_locked(text, static_cast<size_t>(length));
}
void open_trace_locked() noexcept {
    if (runtime->trace_open_attempted || runtime->trace_path.empty()) return;
    runtime->trace_open_attempted = true;
    runtime->file = std::fopen(runtime->trace_path.c_str(), "ab");
    if (runtime->file) {
        emit_locked("{\"event\":\"session\",\"schema\":2,\"pid\":%lu,\"backend\":\"%s\",\"trace\":%s,\"shader_capture_limit\":64,\"shader_word_limit\":4096}\n",
            GetCurrentProcessId(), runtime->probe ? "d3d11-probe" : "d3d8", runtime->trace ? "true" : "false");
        std::fflush(runtime->file);
    }
}
std::shared_ptr<Record> find_record(void* object) noexcept {
    Lock lock;
    const auto found = runtime->objects.find(object);
    return found == runtime->objects.end() ? nullptr : found->second;
}
void observe(void* object, Kind kind) noexcept {
    if (!object || !runtime) return;
    struct Context {
        void* object;
        Kind kind;
        Table hooks{};
        void** original=nullptr;
        std::map<void*,std::shared_ptr<Record>>::iterator prior;
        std::shared_ptr<Dispatch> dispatch;
        std::exception_ptr exception;
    } context{object,kind,{},nullptr,{},{},{}};
    try {
        Lock lock;
        const auto prepare=[](void* opaque)->bool {
            auto& c=*static_cast<Context*>(opaque);
            c.hooks=hooks_for(c.kind);
            if(!c.hooks.methods || !readable(c.object,sizeof(void*)))return false;
            MEMORY_BASIC_INFORMATION memory{};
            if(!VirtualQuery(c.object,&memory,sizeof(memory)))return false;
            const DWORD access=memory.Protect&0xff;
            if(access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
               access!=PAGE_EXECUTE_READWRITE && access!=PAGE_EXECUTE_WRITECOPY)return false;
            c.original=*static_cast<void***>(c.object);
            c.prior=runtime->objects.find(c.object);
            return true;
        };
        const auto prior=[](void* opaque)->UINT {
            auto& c=*static_cast<Context*>(opaque);
            if(c.prior==runtime->objects.end())return 0;
            return c.original==c.prior->second->dispatch->replacement.data()?2u:1u;
        };
        const auto erase=[](void* opaque){
            auto& c=*static_cast<Context*>(opaque);
            runtime->objects.erase(c.prior);
        };
        const auto count=[](void*)->UINT{return runtime->objects.size();};
        const auto dropped=[](void*){++runtime->dropped_objects;};
        const auto cloned=[](void* opaque)->UINT {
            auto& c=*static_cast<Context*>(opaque);
            const auto found=runtime->cloned_tables.find(c.original);
            if(found==runtime->cloned_tables.end())return 0;
            c.dispatch=found->second;
            if(c.dispatch->kind!=c.kind)return 2;
            c.original=c.dispatch->original;
            return 1;
        };
        const auto readable_table=[](void* opaque)->bool {
            auto& c=*static_cast<Context*>(opaque);
            return readable(c.original,c.hooks.count*sizeof(void*));
        };
        const auto cache_allowed=[](void* opaque)->bool {
            auto& c=*static_cast<Context*>(opaque);
            return runtime->dispatches.size()<256 ||
                   runtime->dispatches.count({c.original,c.kind})!=0;
        };
        const auto ensure=[](void* opaque)->bool {
            auto& c=*static_cast<Context*>(opaque);
            try {
                auto& cached=runtime->dispatches[{c.original,c.kind}];
                if(!cached){
                    auto created=std::make_shared<Dispatch>();
                    created->original=c.original;created->kind=c.kind;
                    created->replacement.assign(c.hooks.methods,c.hooks.methods+c.hooks.count);
                    runtime->cloned_tables.emplace(created->replacement.data(),created);
                    cached=std::move(created);
                }
                c.dispatch=cached;
                return true;
            } catch (...) {c.exception=std::current_exception();return false;}
        };
        const auto publish=[](void* opaque)->bool {
            auto& c=*static_cast<Context*>(opaque);
            try {
                auto record=std::make_shared<Record>();
                record->object=c.object;record->original=c.original;record->kind=c.kind;
                record->id=++runtime->next_id;record->dispatch=c.dispatch;
                runtime->objects.emplace(c.object,record);
                InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(c.object),
                                           c.dispatch->replacement.data());
                return true;
            } catch (...) {c.exception=std::current_exception();return false;}
        };
        a2fo_observer_observe_workflow(&context,prepare,prior,erase,count,dropped,
                                       cloned,readable_table,cache_allowed,ensure,publish);
        if(context.exception)std::rethrow_exception(context.exception);
    } catch (...) {message("[D3D8Compat] Observation allocation failed; native object retained");}
}
void forget(void* object, const std::shared_ptr<Record>& record) noexcept {
    Lock lock;
    const auto found = runtime->objects.find(object);
    if (found != runtime->objects.end() && found->second == record) runtime->objects.erase(found);
    // Public Release(0) can leave an implicit Direct3D surface alive. Its
    // dispatch table remains in the process-lifetime cache, so internal AddRef
    // can re-enter safely and recreate observation metadata. Never write into
    // an object here: its storage may also have actually been destroyed.
}
void observe_iid(void* object, REFIID iid) noexcept {
    static_assert(sizeof(GUID)==16);
    const GUID known[]={IID_IDirect3D8,IID_IDirect3DDevice8,IID_IDirect3DTexture8,
        IID_IDirect3DVolumeTexture8,IID_IDirect3DCubeTexture8,
        IID_IDirect3DVertexBuffer8,IID_IDirect3DIndexBuffer8,
        IID_IDirect3DSurface8,IID_IDirect3DVolume8,IID_IDirect3DSwapChain8};
    const auto kind=a2fo_observer_iid_kind(reinterpret_cast<const std::uint8_t*>(&iid),
                                           reinterpret_cast<const std::uint8_t*>(known));
    if(kind>=0)observe(object,static_cast<Kind>(kind));
    // IUnknown/base-resource aliases retain native pointer identity. They are
    // observed already when returned through typed creation/getter methods.
}
void observe_texture(IDirect3DBaseTexture8* texture) noexcept {
    if (!texture || find_record(texture)) return;
    const auto kind=a2fo_observer_texture_kind(texture->GetType());
    if(kind>=0)observe(texture,static_cast<Kind>(kind));
}
void remember_device(void* device, HWND focus, const D3DPRESENT_PARAMETERS* parameters) noexcept {
    D3DPRESENT_PARAMETERS copy{};
    if (!copy_value(parameters, copy)) return;
    const auto record = find_record(device);
    if (!record) return;
    Lock lock;
    record->presentation = copy;
    if (focus) record->focus = focus;
}

class Call {
public:
    Call(void* object, Kind kind, size_t slot, const char* api, const char* method) noexcept
        : record_(find_record(object)), slot_(slot), interface_(api), method_(method) {
        if (!record_) { observe(object,kind); record_=find_record(object); }
        if (record_) original_=record_->original;
        else if (readable(object,sizeof(void*))) {
            // Allocation failure must still forward through the stable table.
            Lock lock;
            const auto found=runtime->cloned_tables.find(*static_cast<void***>(object));
            if(found!=runtime->cloned_tables.end())original_=found->second->original;
        }
        if (runtime->trace) QueryPerformanceCounter(&started_);
    }
    ~Call() noexcept {
        if (!runtime->trace) return;
        try {
            LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
            const auto us = static_cast<unsigned long long>((now.QuadPart - started_.QuadPart) * 1000000 / runtime->frequency.QuadPart);
            const bool failed = hresult_ && static_cast<LONG>(result_) < 0;
            const std::string method = std::string(interface_) + "." + method_;
            const std::string key = method + "|" + signature_ + detail_;
            Lock lock;
            struct Diagnostic {
                Call* call;
                const std::string* method;
                const std::string* key;
                unsigned long long us;
                bool failed;
            } diagnostic{this,&method,&key,us,failed};
            const auto state=[](void* opaque)->UINT {
                auto& c=*static_cast<Diagnostic*>(opaque);
                if(runtime->signatures.find(*c.key)!=runtime->signatures.end())return 0;
                return runtime->signatures.size()<8192?1u:2u;
            };
            const auto step=[](void* opaque,UINT operation)->bool {
                auto& c=*static_cast<Diagnostic*>(opaque);
                try {
                    switch(operation){
                        case 0:{auto& value=runtime->methods[*c.method];
                            ++value.calls;value.failures+=c.failed;value.microseconds+=c.us;break;}
                        case 1:++runtime->signatures.find(*c.key)->second.count;break;
                        case 2:runtime->signatures.emplace(*c.key,Signature{1});break;
                        case 3:++runtime->dropped_signatures;break;
                        case 4:open_trace_locked();break;
                        case 5:return runtime->file!=nullptr;
                        case 6:
                            emit_locked("{\"event\":\"call\",\"interface\":\"%s\",\"method\":\"%s\",\"object\":%u,\"result\":\"0x%08lx\",\"us\":%llu,\"args\":\"%s\",\"detail\":\"%s\"}\n",
                                c.call->interface_,c.call->method_,
                                c.call->record_?c.call->record_->id:0,c.call->result_,
                                c.us,c.call->args_.c_str(),c.call->detail_.c_str());
                            ++runtime->events;std::fflush(runtime->file);break;
                    }
                    return true;
                } catch (...) {return false;}
            };
            a2fo_observer_diagnostic_workflow(&diagnostic,failed,runtime->events,
                                               runtime->max_events,step,state);
        } catch (...) {} // Diagnostics never change an application's HRESULT.
    }
    template<class T> T original() const noexcept {
        return original_ ? function_at<T>(original_[slot_]) : nullptr;
    }
    const std::shared_ptr<Record>& record() const noexcept { return record_; }
    void complete(unsigned long result) noexcept { result_ = result; }
    template<class T> void complete_result(T value, bool hresult) noexcept {
        hresult_ = hresult;
        if constexpr (std::is_pointer_v<T>) result_ = reinterpret_cast<unsigned long>(value);
        else result_ = static_cast<unsigned long>(value);
    }
    template<class T> void argument(const char* name, T value) noexcept {
        if (!runtime->trace) return;
        try {
            std::ostringstream text;
            text << name << '=';
            if constexpr (std::is_pointer_v<T>) text << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(value);
            else if constexpr (std::is_enum_v<T>) text << static_cast<unsigned long>(value);
            else text << value;
            args_ += text.str() + ';';
            if constexpr (std::is_pointer_v<T>) signature_ += std::string(name) + "=ptr;";
            else signature_ += text.str() + ';';
        } catch (...) {}
    }
    void argument(const char* name, const GUID& value) noexcept {
        argument(name, value.Data1);
    }
    void presentation(const D3DPRESENT_PARAMETERS* pointer) noexcept {
        if (!runtime->trace) return;
        D3DPRESENT_PARAMETERS p{};
        if (!copy_value(pointer, p)) return;
        char text[400];
        std::snprintf(text, sizeof(text), "pp width=%u height=%u format=%u count=%u multisample=%u swap=%u hwnd=0x%lx windowed=%d depth=%d depthFormat=%u flags=%lu refresh=%u interval=%u;",
            p.BackBufferWidth,p.BackBufferHeight,p.BackBufferFormat,p.BackBufferCount,p.MultiSampleType,p.SwapEffect,
            reinterpret_cast<unsigned long>(p.hDeviceWindow),p.Windowed,p.EnableAutoDepthStencil,p.AutoDepthStencilFormat,p.Flags,p.FullScreen_RefreshRateInHz,p.FullScreen_PresentationInterval);
        try { detail_ += text; } catch (...) {}
    }
    void viewport(const D3DVIEWPORT8* pointer) noexcept {
        if (!runtime->trace) return;
        D3DVIEWPORT8 v{}; if (!copy_value(pointer, v)) return;
        char text[160]; std::snprintf(text,sizeof(text),"viewport x=%lu y=%lu w=%lu h=%lu minZ=%.9g maxZ=%.9g;",v.X,v.Y,v.Width,v.Height,v.MinZ,v.MaxZ);
        try { detail_ += text; } catch (...) {}
    }
    void descriptor(const D3DSURFACE_DESC* pointer) noexcept {
        D3DSURFACE_DESC d{}; if (!runtime->trace || !copy_value(pointer,d)) return;
        char text[200]; std::snprintf(text,sizeof(text),"surface format=%u type=%u usage=%lu pool=%u size=%u multisample=%u w=%u h=%u;",d.Format,d.Type,d.Usage,d.Pool,d.Size,d.MultiSampleType,d.Width,d.Height);
        try { detail_ += text; } catch (...) {}
    }
    template<class T> void descriptor(const T* pointer) noexcept {
        T d{}; if (!runtime->trace || !copy_value(pointer,d)) return;
        char text[160]; std::snprintf(text,sizeof(text),"buffer format=%u type=%u usage=%lu pool=%u size=%u;",d.Format,d.Type,d.Usage,d.Pool,d.Size);
        try { detail_ += text; } catch (...) {}
    }
private:
    std::shared_ptr<Record> record_;
    void** original_ = nullptr;
    size_t slot_;
    const char* interface_; const char* method_;
    unsigned long result_ = 0;
    bool hresult_ = false;
    LARGE_INTEGER started_{};
    std::string args_, signature_, detail_;
};

#include "shader_diagnostics.inl"

void probe_after_present(void* device, HWND override_window) noexcept {
    struct Context {
        void* device;
        HWND override_window;
        std::shared_ptr<Record> record;
        D3DPRESENT_PARAMETERS presentation{};
        HWND focus=nullptr,target=nullptr;
        RECT client{};
    } context{device,override_window,{},{},nullptr,nullptr,{}};
    const auto find=[](void* opaque)->bool {
        auto& c=*static_cast<Context*>(opaque);
        c.record=find_record(c.device);
        if(!c.record)return false;
        {Lock lock;c.presentation=c.record->presentation;c.focus=c.record->focus;}
        return true;
    };
    const auto windowed=[](void* opaque)->bool {
        return static_cast<Context*>(opaque)->presentation.Windowed!=FALSE;
    };
    const auto claim=[](void*)->bool {return !runtime->probe_attempted.exchange(true);};
    const auto client=[](void* opaque)->bool {
        auto& c=*static_cast<Context*>(opaque);
        c.target=c.override_window?c.override_window:
            c.presentation.hDeviceWindow?c.presentation.hDeviceWindow:c.focus;
        return c.target && GetClientRect(c.target,&c.client) &&
               c.client.right>0 && c.client.bottom>0;
    };
    const auto skipped=[](void*) {
        message("[D3D11] Probe skipped: no nonzero native client area; D3D8 retained");
    };
    const auto run=[](void* opaque) {
        auto& c=*static_cast<Context*>(opaque);
        try {
            message("[D3D11] One-frame diagnostic in native window; this is not translated game rendering");
            const HRESULT hr=run_d3d11_window_probe(c.target,c.client.right,c.client.bottom,
                                                     runtime->debug,runtime->log);
            char text[200];std::snprintf(text,sizeof(text),
                "[D3D11] Probe complete HRESULT=0x%08lx; temporary device released, next frame uses D3D8",
                static_cast<unsigned long>(hr));
            message(text);
        } catch (...) {message("[D3D11] Probe exception; D3D8 retained");}
    };
    a2fo_observer_probe_workflow(&context,runtime->probe,
        runtime->probe_attempted.load(),find,windowed,claim,client,skipped,run);
}
void summary_locked() noexcept {
    try {
        open_trace_locked(); if (!runtime->file) return;
        for (const auto& item : runtime->methods)
            emit_locked("{\"event\":\"count\",\"method\":\"%s\",\"calls\":%llu,\"failures\":%llu,\"us\":%llu}\n",item.first.c_str(),item.second.calls,item.second.failures,item.second.microseconds);
        for (const auto& item : runtime->signatures)
            emit_locked("{\"event\":\"signature\",\"key\":\"%s\",\"calls\":%llu}\n",item.first.c_str(),item.second.count);
        emit_locked("{\"event\":\"summary\",\"frames\":%u,\"samples\":%u,\"signature_overflow\":%llu,\"object_overflow\":%llu,\"live_object_records\":%zu,\"dispatch_tables\":%zu,\"shader_capture_attempts\":%u,\"shader_capture_limit_reached\":%s}\n",runtime->frames,runtime->events,runtime->dropped_signatures,runtime->dropped_objects,runtime->objects.size(),runtime->dispatches.size(),runtime->shader_captures,runtime->shader_limit_reported ? "true" : "false");
        std::fflush(runtime->file);
    } catch (...) {}
}
void present_boundary(void*, HRESULT) noexcept {
    if (!runtime->trace) return;
    Lock lock;
    ++runtime->frames;
    if (a2fo_observer_present_summary(runtime->frames)) summary_locked();
}

#include "generated_thunks.inl"

IDirect3D8* WINAPI factory_hook(UINT sdk) {
    struct Context {
        UINT sdk;IDirect3D8 *native=nullptr;
        std::exception_ptr exception;
    } context{sdk,nullptr,{}};
    const auto step=[](void *opaque,UINT action,std::uintptr_t *result,bool *threw){
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(action){
                case 0:
                    message("[D3D8Compat] D3D11 child startup refused: required import bridge incomplete; select normal FO4 to recover");
                    break;
                case 1:
                    c.native=runtime->factory(c.sdk);
                    *result=reinterpret_cast<std::uintptr_t>(c.native);break;
                case 2:{
                    const std::string base=runtime->game_configuration.shader_cache;
                    const auto split=base.find_last_of("\\/");
                    if(split!=std::string::npos)CreateDirectoryA(base.substr(0,split).c_str(),nullptr);
                    CreateDirectoryA(base.c_str(),nullptr);
                    IDirect3D8* translated=game::create_factory(c.native,runtime->game_configuration);
                    if(c.native)c.native->Release();
                    message(translated ? "[D3D8Compat] Experimental D3D11 factory returned; legacy device creation bypassed" : "[D3D8Compat] Experimental factory failed; no incompatible native BC fallback");
                    *result=reinterpret_cast<std::uintptr_t>(translated);break;
                }
                case 3:{
                    observe(c.native,Kind::IDirect3D8);
                    char text[180];
                    std::snprintf(text,sizeof(text),
                        "[D3D8Compat] Direct3DCreate8 SDK=%u native=%p; observing existing provider",
                        c.sdk,static_cast<void*>(c.native));
                    message(text);break;
                }
            }
        } catch (...) {c.exception=std::current_exception();*threw=true;}
    };
    bool threw=false;
    const auto result=a2fo_observer_factory_workflow(&context,runtime->game_test,
        runtime->game_ready,step,&threw);
    if(threw)std::rethrow_exception(context.exception);
    return reinterpret_cast<IDirect3D8*>(result);
}

// Named PE32 import discovery; no map-derived addresses are installed.
// Image bounds are checked even though the core separately pins the executable.
bool patch_factory_import(HMODULE module) noexcept {
    if (!module) return false;
    static_assert(sizeof(void*)==4,"PE32 observer requires 32-bit pointers");
    const A2foObserverPeLayout layout{
        sizeof(IMAGE_DOS_HEADER),sizeof(IMAGE_NT_HEADERS32),
        sizeof(IMAGE_IMPORT_DESCRIPTOR),sizeof(IMAGE_THUNK_DATA32),
        offsetof(IMAGE_DOS_HEADER,e_magic),offsetof(IMAGE_DOS_HEADER,e_lfanew),
        offsetof(IMAGE_NT_HEADERS32,Signature),
        offsetof(IMAGE_NT_HEADERS32,OptionalHeader.Magic),
        offsetof(IMAGE_NT_HEADERS32,FileHeader.Machine),
        offsetof(IMAGE_NT_HEADERS32,OptionalHeader.SizeOfImage),
        offsetof(IMAGE_NT_HEADERS32,OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress),
        offsetof(IMAGE_NT_HEADERS32,OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size),
        offsetof(IMAGE_IMPORT_DESCRIPTOR,Name),
        offsetof(IMAGE_IMPORT_DESCRIPTOR,OriginalFirstThunk),
        offsetof(IMAGE_IMPORT_DESCRIPTOR,FirstThunk),
        IMAGE_DOS_SIGNATURE,IMAGE_NT_SIGNATURE,IMAGE_NT_OPTIONAL_HDR32_MAGIC,
        IMAGE_FILE_MACHINE_I386,IMAGE_ORDINAL_FLAG32};
    const auto checked_span=[](void*,std::uintptr_t address,std::size_t bytes)->bool{
        return readable(reinterpret_cast<const void*>(address),bytes);
    };
    const auto checked_read=[](void*,std::uintptr_t address,std::uint8_t *out,std::size_t bytes){
        std::memcpy(out,reinterpret_cast<const void*>(address),bytes);
    };
    A2foObserverPatchPlan plan{};
    const auto callback=reinterpret_cast<std::uintptr_t>(address(&factory_hook));
    const auto previous_factory=reinterpret_cast<std::uintptr_t>(address(runtime->factory));
    if(!a2fo_observer_patch_walk(reinterpret_cast<std::uintptr_t>(module),&layout,
        checked_span,checked_read,nullptr,callback,previous_factory,&plan))return false;
    if(plan.duplicate)return true;
    auto** slot=reinterpret_cast<void**>(plan.slot);
    void* original=reinterpret_cast<void*>(plan.original);
            // Pin the code owner before publishing callbacks. Dynamic unloading
            // cannot leave provider objects pointing into an unloaded extension.
            HMODULE owner=nullptr;
            if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<LPCSTR>(address(&factory_hook)),&owner)) return false;
            DWORD protection=0;
            if (!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection)) return false;
            runtime->factory=function_at<Factory>(original);
            void* previous=InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(slot),address(&factory_hook),original);
            DWORD ignored=0; const bool restored=VirtualProtect(slot,sizeof(void*),protection,&ignored)!=FALSE;
            if (!restored) message("[D3D8Compat] Could not restore factory IAT protection");
            return previous==original;
}
} // namespace

bool install_d3d8_observer(HMODULE executable, HMODULE fleet_ops,
                          const std::string& data_root, RendererLog log) {
    if (runtime) return true;
    std::string ini=data_root+"\\A2FORenderer.ini";
    std::string test_mod_root;
    const std::string active=a2fo::active_mod_from_command_line(GetCommandLineA());
    if(a2fo::safe_mod_directory_name(active)){
        const std::string root=data_root+"\\Mods\\"+active;
        const std::string candidate=root+"\\A2FORenderer.ini";
        char selected[40]{};
        GetPrivateProfileStringA("D3D8Compat","Backend","",selected,sizeof(selected),candidate.c_str());
        if(!_stricmp(selected,"d3d11-test")){ini=candidate;test_mod_root=root;}
    }
    char backend[40]{};
    GetPrivateProfileStringA("D3D8Compat","Backend","d3d8",backend,sizeof(backend),ini.c_str());
    const bool trace=GetPrivateProfileIntA("D3D8Compat","Trace",0,ini.c_str())!=0;
    const auto mode=a2fo_observer_backend_mode(
        reinterpret_cast<const std::uint8_t*>(backend),std::strlen(backend),
        !test_mod_root.empty());
    const bool probe=mode==2;
    const bool game_test=mode==3;
    if (!mode) {
        if(log) log("[D3D8Compat] Unsupported backend; D3D8 retained. Available: d3d8, d3d11-probe (one diagnostic frame)");
        return false;
    }
    if (!a2fo_observer_should_install(trace,probe,game_test)) return true; // Absolutely no hook or GPU work by default.
    try {
        runtime=new Runtime;
        runtime->log=log; runtime->trace=trace; runtime->probe=probe; runtime->game_test=game_test;
        if(game_test){
            runtime->game_configuration.mod_root=test_mod_root;
            char parent[256]{};
            GetPrivateProfileStringA("mod","ParentMod","",parent,sizeof(parent),(test_mod_root+"\\info.ini").c_str());
            if(a2fo::safe_mod_directory_name(parent))runtime->game_configuration.original_texture_root=data_root+"\\Mods\\"+parent+"\\Textures";
            runtime->game_configuration.shader_cache=data_root+"\\A2FOShaderCache\\d3d11-test";
            runtime->game_configuration.log=log;
            runtime->game_configuration.debug=GetPrivateProfileIntA("D3D8Compat","DebugLayer",0,ini.c_str())!=0;
            runtime->game_configuration.vsync=GetPrivateProfileIntA("D3D8Compat","VSync",1,ini.c_str())!=0;
            runtime->game_configuration.bc_assets=GetPrivateProfileIntA("D3D8Compat","BCTextures",1,ini.c_str())!=0;
        }
        runtime->debug=GetPrivateProfileIntA("D3D8Compat","DebugLayer",0,ini.c_str())!=0;
        runtime->max_events=a2fo_observer_max_events(GetPrivateProfileIntA(
            "D3D8Compat","MaxEvents",8192,ini.c_str()));
        QueryPerformanceFrequency(&runtime->frequency);
        if (!runtime->frequency.QuadPart) runtime->frequency.QuadPart=1;
        char suffix[96]; std::snprintf(suffix,sizeof(suffix),"\\A2FOD3D8-%lu-%llu.jsonl",GetCurrentProcessId(),GetTickCount64());
        runtime->trace_path=data_root+suffix;
        struct BridgeContext {HMODULE executable,fleet;RendererLog log;};
        BridgeContext bridge_context{executable,fleet_ops,log};
        const auto patch=[](void* opaque,UINT step)->bool {
            auto& c=*static_cast<BridgeContext*>(opaque);
            return patch_factory_import(step==0?c.executable:c.fleet);
        };
        const auto bridge=[](void* opaque,UINT step)->bool {
            auto& c=*static_cast<BridgeContext*>(opaque);
            switch(step){
                case 0:return game::install_file_bridge(c.executable,c.log);
                case 1:return game::install_file_bridge(c.fleet,c.log);
                default:return game::install_d3dx_file_bridge(c.fleet,c.log);
            }
        };
        const auto plan=a2fo_observer_activation_workflow(&bridge_context,game_test,
            runtime->game_configuration.bc_assets,fleet_ops!=nullptr,patch,bridge);
        const bool installed=plan.installed!=0;
        const bool fo_installed=plan.fo_installed!=0;
        if(game_test){
            const bool files=plan.files!=0;
            runtime->game_ready=plan.ready!=0;
            message(installed&&files?"[D3D8Compat] Child-only D3D11 game test route armed; GPU work deferred to CreateDevice":"[D3D8Compat] Test import bridge incomplete; D3D11 test cannot start");
            if(!runtime->game_ready)message(installed?
                "[D3D8Compat] Fail-closed child: Direct3DCreate8 will return null; normal FO4 backend is unchanged":
                "[D3D8Compat] No factory interception: BC file redirects were not installed; child route unavailable");
        }else message(installed ? "[D3D8Compat] Optional native-provider observer installed; legacy shader/resource ownership preserved" : "[D3D8Compat] Factory import not patched; D3D8 retained, no API coverage claimed");
        if(fo_installed) message("[D3D8Compat] FleetOpsHook D3D8 factory import observed");
        message("[D3D8Compat] D3D9 paths, GDI/video and offscreen Refit renderer are outside this trace");
        if(trace) message("[D3D8Compat] Bounded trace: "+runtime->trace_path);
        return plan.result!=0;
    } catch (...) {
        if(log) log("[D3D8Compat] Observer initialization failed; D3D8 retained");
        return false;
    }
}
bool d3d11_game_test_requested() noexcept {
    return runtime&&runtime->game_test;
}
void flush_d3d8_observer() noexcept {
    if (!runtime || !runtime->trace) return;
    Lock lock; summary_locked();
}
} // namespace a2fo::renderer
