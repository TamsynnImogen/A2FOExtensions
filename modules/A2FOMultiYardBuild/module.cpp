// Multi-yard build palette and local command routing, adapted from Refit QOL-6.
// Copyright/provenance: third_party/armada2-refit/SOURCE.md and LICENSE.txt.
// Addresses below are cross-checked against installed ArmadaL/FleetOpsHook;
// the original files are never patched. Only the enabled module patches memory.
#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/module_runtime_a4_ffi.h"
#include "../../sdk/include/a2fo_module_provider.hpp"
#include "policy.hpp"
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <mutex>
#include <string>
#include <unordered_set>

static_assert(sizeof(void*) == 4, "Native i386 module only");
extern "C" {
void multi_yard_enable_stub();
void multi_yard_menu_stub();
void multi_yard_route_stub();
void multi_yard_cancel_stub();
void multi_yard_qsize_stub();
void multi_yard_select_first_stub();
void multi_yard_select_compat_stub();
std::uintptr_t multi_yard_call_thiscall_0(void*, void*);
std::uintptr_t multi_yard_call_thiscall_2(void*, void*, std::uintptr_t, std::uintptr_t);
std::uintptr_t multi_yard_enable_continue = 0;
std::uintptr_t multi_yard_menu_open = 0;
std::uintptr_t multi_yard_menu_reject = 0;
std::uintptr_t multi_yard_route_continue = 0;
std::uintptr_t multi_yard_route_loop = 0;
std::uintptr_t multi_yard_cancel_continue = 0;
std::uintptr_t multi_yard_cancel_empty = 0;
std::uintptr_t multi_yard_native_qsize = 0;
std::uintptr_t multi_yard_select_one = 0;
std::uintptr_t multi_yard_select_many = 0;
std::uintptr_t multi_yard_select_check = 0;
std::uintptr_t multi_yard_select_accept = 0;
}

namespace {
constexpr char kName[] = "A2FOMultiYardBuild";
constexpr unsigned kMaxSelection = 30;
const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr, g_fleet_ops = nullptr;
bool g_active = false;
std::mutex g_classes_mutex, g_route_mutex;
std::unordered_set<void*> g_yard_classes;
std::uint32_t g_last_built = 0;

void log_line(const char* s) noexcept { if (g_api && g_api->log) g_api->log(kName,s); }
void* at(HMODULE image, std::uintptr_t rva) noexcept {
    return image ? reinterpret_cast<std::uint8_t*>(image) + rva : nullptr;
}
bool readable(const void* p, std::size_t n) noexcept {
    MEMORY_BASIC_INFORMATION m{};
    if (!p || !n || !VirtualQuery(p,&m,sizeof(m)) || m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_NOACCESS|PAGE_GUARD))) return false;
    const auto start = reinterpret_cast<std::uintptr_t>(p);
    const auto end = reinterpret_cast<std::uintptr_t>(m.BaseAddress)+m.RegionSize;
    return start <= end && n <= end-start;
}
template<class T> T read(const void* p, std::size_t off, T fallback = {}) noexcept {
    if (!p) return fallback;
    const auto* address = static_cast<const std::uint8_t*>(p)+off;
    if (!readable(address,sizeof(T))) return fallback;
    T value; std::memcpy(&value,address,sizeof(value)); return value;
}
void* entity(std::uint32_t id) noexcept {
    using Fn = void* (__cdecl*)(std::uint32_t);
    return id && g_armada ? reinterpret_cast<Fn>(at(g_armada,0xcfff0))(id) : nullptr;
}
int user_team() noexcept {
    using Fn = int (__cdecl*)();
    return g_armada ? reinterpret_cast<Fn>(at(g_armada,0xd0060))() : -1;
}
std::uint32_t extended_queue_size(void* object) noexcept {
    using Extended = std::uint32_t (__cdecl*)(void*);
    HMODULE pack = a2fo::startup_module_provider(
        "A2FOFeaturePack.dll", {"A2FO_ProducerLogicalQueueSize"});
    auto query = pack ? reinterpret_cast<Extended>(reinterpret_cast<void*>(
        GetProcAddress(pack,"A2FO_ProducerLogicalQueueSize"))) : nullptr;
    return query ? query(object) : UINT32_MAX;
}
struct YardContext {
    const A2FO_ModuleApi* api=nullptr;
    void* ci=nullptr;
    std::exception_ptr error;
    char config_value[16]{};
};
void yard_log(void*,std::uint32_t code) noexcept {
    switch(code) {
    case 0: log_line("Invalid MultiYardBuild info.ini value; passive");break;
    case 1: log_line("MultiYardBuild is not enabled in info.ini; passive");break;
    case 2: log_line("Multi-yard signatures mismatch; disabled without patches");break;
    case 3: log_line("Multi-yard patch incomplete; installed stubs retain native behavior");break;
    case 4: log_line("Multi-yard build active: same-class owned yards; Build palette retained; shortest queue, rotating ties; native synchronized orders");break;
    }
}
std::uint32_t yard_config(void* opaque,std::uint32_t index,A2foRsA4Bytes* out) noexcept {
    auto& context=*static_cast<YardContext*>(opaque);
    if(context.error)return 2;
    try {
        const char* root=context.api->extension_root(index);
        if (!root || !*root) return 2;
        const auto path=std::string(root)+"\\info.ini";
        std::memset(context.config_value,0,sizeof(context.config_value));
        const auto n=GetPrivateProfileStringA("mod","MultiYardBuild","",
            context.config_value,sizeof(context.config_value),path.c_str());
        if (!n) return 0;
        *out={reinterpret_cast<const std::uint8_t*>(context.config_value),n};
        return 1;
    } catch (...) {context.error=std::current_exception();return 2;}
}
std::uint32_t yard_read(void* opaque,std::uintptr_t raw,std::uint32_t operation) noexcept {
    void* p=reinterpret_cast<void*>(raw);
    if(operation==9 && !p) p=static_cast<YardContext*>(opaque)->ci;
    if ((operation&0xffff0000)==0x10000) return readable(p,operation&0xffff);
    if ((operation&0xffff0000)==0x20000)
        return read<std::uint32_t>(p,static_cast<std::size_t>(operation&0xffff)*sizeof(void*));
    if ((operation&0xffff0000)==0x30000)
        return read<std::uint32_t>(p,0x3d0+static_cast<std::size_t>(operation&0xffff)*sizeof(std::uint32_t));
    switch(operation) {
    case 1:return read<int>(p,2*sizeof(int));
    case 2:return read<std::uint32_t>(p,0);
    case 3:return reinterpret_cast<std::uintptr_t>(read<void*>(p,0x40));
    case 4:return read<int>(p,0xec,-1);
    case 5:return read<std::uint8_t>(p,0x27,1);
    case 6:return read<std::uint8_t>(p,0x14);
    case 7:return read<std::uint32_t>(p,0x28);
    case 8:return read<std::uint32_t>(p,0x124,UINT32_MAX);
    case 9:return read<std::uint32_t>(p,0x1a0);
    case 10:return read<std::uint32_t>(p,0xb8,kMaxSelection+1);
    default:return 0;
    }
}
std::uint8_t yard_field(void* opaque,std::uint32_t operation,A2foRsA4Bytes* out) noexcept {
    const auto* event=static_cast<const A2FO_GameObjectClassLoadedEvent*>(
        static_cast<YardContext*>(opaque)->ci);
    const auto& field=event->odf_fields[operation/2];
    const auto& value=operation&1?field.value:field.name;
    if (!field.name.data || !field.value.data) return 0;
    *out={reinterpret_cast<const std::uint8_t*>(value.data),value.size};
    return 1;
}
std::uint8_t yard_compare(void* opaque,A2foRsA4Bytes left,A2foRsA4Bytes right) noexcept {
    auto& context=*static_cast<YardContext*>(opaque);
    if(context.error)return 0;
    try {
        const std::string a(reinterpret_cast<const char*>(left.data),left.size);
        const std::string b(reinterpret_cast<const char*>(right.data),right.size);
        return _stricmp(a.c_str(),b.c_str())==0;
    } catch (...) {context.error=std::current_exception();return 0;}
}
std::uintptr_t yard_native(void*,std::uint32_t,std::uintptr_t,std::uint32_t) noexcept;
void A2FO_CALL class_loaded(const A2FO_GameObjectClassLoadedEvent* event,void*) {
    const bool valid=event && event->struct_size>=sizeof(*event) && event->object_class &&
        (!event->odf_field_count || event->odf_fields);
    YardContext context{nullptr,const_cast<A2FO_GameObjectClassLoadedEvent*>(event),{}};
    a2fo_rs_runtime_yard_class(valid,reinterpret_cast<std::uintptr_t>(valid?event->object_class:nullptr),
        valid?event->odf_field_count:0,yard_field,yard_compare,yard_native,&context);
    if(context.error) log_line("Multi-yard class metadata could not be retained");
}

struct Site { bool fleet; std::uintptr_t rva; const std::uint8_t* bytes; std::size_t size; void (*stub)(); bool call; };
// Fleet Ops replaces the native palette updater. Its real build-button gate
// uses the CraftArray at [ebp+8], before HybridBuild's later bind call at 1e6d97.
constexpr std::uint8_t enable_bytes[]{0x8b,0x45,0x08,0x83,0x78,0x08,0x01,0x75,0x07};
// The Build/Research/Special palette has a separate single-selection gate.
// Multi-selection otherwise reaches 1e7133 and immediately resets to Root.
constexpr std::uint8_t menu_bytes[]{0x8b,0x45,0x08,0x83,0x78,0x08,0x01,0x74,0x10};
constexpr std::uint8_t route_bytes[]{0x8b,0x45,0xe8,0x33,0xf6,0x85,0xc0,0x7e,0x75};
constexpr std::uint8_t cancel_bytes[]{0x8b,0x45,0xe0,0x85,0xc0,0xc7,0x45,0xec,0,0,0,0,0x0f,0x8e,0x1c,0x01,0,0};
constexpr std::uint8_t qsize_bytes[]{0xe8,0xe8,0xbb,0xfb,0xff};
constexpr std::uint8_t first_bytes[]{0x8b,0x45,0x08,0x83,0xb8,0xb8,0,0,0,0x01,0x0f,0x85,0xa8,0,0,0};
constexpr std::uint8_t compat_bytes[]{0x80,0x7d,0x1c,0,0x0f,0x84,0xe5,0,0,0};
const Site sites[]{
    {true,0x1e6d75,enable_bytes,sizeof(enable_bytes),multi_yard_enable_stub,false},
    {true,0x1e6fe5,menu_bytes,sizeof(menu_bytes),multi_yard_menu_stub,false},
    {false,0xfc486,route_bytes,sizeof(route_bytes),multi_yard_route_stub,false},
    {false,0xfc0a6,cancel_bytes,sizeof(cancel_bytes),multi_yard_cancel_stub,false},
    {false,0xfbf83,qsize_bytes,sizeof(qsize_bytes),multi_yard_qsize_stub,true},
    {true,0x1dadc9,first_bytes,sizeof(first_bytes),multi_yard_select_first_stub,false},
    {true,0x1daf9a,compat_bytes,sizeof(compat_bytes),multi_yard_select_compat_stub,false},
};
bool signatures_match() noexcept {
    for(const auto& s:sites) {
        const void* p=at(s.fleet?g_fleet_ops:g_armada,s.rva);
        if (!readable(p,s.size) || std::memcmp(p,s.bytes,s.size)) return false;
    }
    constexpr std::uint8_t get[]{0x55,0x8b,0xec,0x8b,0x4d,0x08};
    constexpr std::uint8_t qs[]{0x8b,0x89,0x70,0x02,0,0,0x33,0xc0,0x85,0xc9,0x74,0x08,0x8b,0x49,0x08,0x40,0x85,0xc9,0x75,0xf8,0xc3};
    // GetUserTeam is a native load plus tail call. Compare its relocated global
    // operand to the actual image, retaining exact opcode/displacement checks.
    const std::uint8_t team_head[]{0x8b,0x0d};
    const std::uint8_t team_tail[]{0xe9,0x45,0x78,0x08,0};
    return readable(at(g_armada,0xcfff0),sizeof(get)) &&
        !std::memcmp(at(g_armada,0xcfff0),get,sizeof(get)) &&
        readable(at(g_armada,0xb7b70),sizeof(qs)) &&
        !std::memcmp(at(g_armada,0xb7b70),qs,sizeof(qs)) &&
        readable(at(g_armada,0xd0060),11) &&
        !std::memcmp(at(g_armada,0xd0060),team_head,sizeof(team_head)) &&
        read<void*>(at(g_armada,0xd0060),2)==at(g_armada,0x36b8d4) &&
        !std::memcmp(at(g_armada,0xd0066),team_tail,sizeof(team_tail));
}
std::uintptr_t yard_native(void* opaque,std::uint32_t operation,
                           std::uintptr_t raw,std::uint32_t argument) noexcept {
    auto& context=*static_cast<YardContext*>(opaque);
    if(context.error)return 0;
    try {
    switch(operation) {
    case 0:return static_cast<std::uintptr_t>(user_team());
    case 1:return reinterpret_cast<std::uintptr_t>(entity(argument));
    case 2: {
        std::lock_guard<std::mutex> lock(g_classes_mutex);
        return g_yard_classes.count(reinterpret_cast<void*>(raw));
    }
    case 3:return extended_queue_size(reinterpret_cast<void*>(raw));
    case 4:return multi_yard_call_thiscall_2(at(g_armada,0xd4280),
        reinterpret_cast<void*>(raw),0x19,argument);
    case 5:g_api=context.api;g_armada=static_cast<HMODULE>(g_api->armada_module());
        g_fleet_ops=static_cast<HMODULE>(g_api->fleetops_module());return 1;
    case 6:return signatures_match();
    case 7: {
        const char* fields[]{"classLabel"};
        return g_api->register_game_object_class_loaded_handler(kName,fields,1,class_loaded,nullptr);
    }
    case 8:
        multi_yard_enable_continue=reinterpret_cast<std::uintptr_t>(at(g_fleet_ops,0x1e6d87));
        multi_yard_menu_open=reinterpret_cast<std::uintptr_t>(at(g_fleet_ops,0x1e6ffe));
        multi_yard_menu_reject=reinterpret_cast<std::uintptr_t>(at(g_fleet_ops,0x1e6fee));
        multi_yard_route_continue=reinterpret_cast<std::uintptr_t>(at(g_armada,0xfc504));
        multi_yard_route_loop=reinterpret_cast<std::uintptr_t>(at(g_armada,0xfc48f));
        multi_yard_cancel_continue=reinterpret_cast<std::uintptr_t>(at(g_armada,0xfc0b8));
        multi_yard_cancel_empty=reinterpret_cast<std::uintptr_t>(at(g_armada,0xfc1d4));
        multi_yard_native_qsize=reinterpret_cast<std::uintptr_t>(at(g_armada,0xb7b70));
        multi_yard_select_one=reinterpret_cast<std::uintptr_t>(at(g_fleet_ops,0x1dadd9));
        multi_yard_select_many=reinterpret_cast<std::uintptr_t>(at(g_fleet_ops,0x1dae81));
        multi_yard_select_check=reinterpret_cast<std::uintptr_t>(at(g_fleet_ops,0x1dafa4));
        multi_yard_select_accept=reinterpret_cast<std::uintptr_t>(at(g_fleet_ops,0x1db089));
        return 1;
    case 9: {
        const auto& site=sites[argument];
        auto patch=site.call?g_api->patch_call:g_api->patch_jump;
        return patch(at(site.fleet?g_fleet_ops:g_armada,site.rva),
            reinterpret_cast<void*>(site.stub),site.bytes,site.size);
    }
    case 10:if(argument)g_active=true;return g_active;
    case 11:g_active=false;return 0;
    case 12:
        return g_api->extension_root_count && g_api->extension_root
            ? g_api->extension_root_count():0;
    case 13: {
        std::lock_guard<std::mutex> lock(g_classes_mutex);
        auto* klass=reinterpret_cast<void*>(raw);
        g_yard_classes.erase(klass);
        if(argument)g_yard_classes.insert(klass);
        return 1;
    }
    case 14:g_route_mutex.lock();return g_last_built;
    case 15:g_last_built=argument;return 1;
    case 16: {
        auto* arr=reinterpret_cast<int*>(raw);
        auto** objects=reinterpret_cast<void**>(arr[0]);
        objects[0]=objects[argument];arr[2]=1;return 1;
    }
    case 17:g_route_mutex.unlock();return 1;
    case 18:return multi_yard_call_thiscall_0(
        reinterpret_cast<void*>(multi_yard_native_qsize),reinterpret_cast<void*>(raw));
    default:return 0;
    }
    } catch (...) { context.error=std::current_exception(); return 0; }
}
} // namespace

extern "C" int __cdecl multi_yard_build_enable(int* arr) noexcept {
    YardContext context{};
    return a2fo_rs_runtime_yard_gate(0,reinterpret_cast<std::uintptr_t>(arr),0,0,
        yard_read,yard_native,&context);
}
extern "C" int __cdecl multi_yard_menu_permit(int* arr,void* popup) noexcept {
    YardContext context{};
    return a2fo_rs_runtime_yard_gate(1,reinterpret_cast<std::uintptr_t>(arr),0,
        reinterpret_cast<std::uintptr_t>(popup),yard_read,yard_native,&context);
}
extern "C" int __cdecl multi_yard_build_route(void** objects,int n,void* klass) noexcept {
    YardContext context{};
    return a2fo_rs_runtime_yard_route(reinterpret_cast<std::uintptr_t>(objects),n,
        reinterpret_cast<std::uintptr_t>(klass),yard_read,yard_native,&context);
}
extern "C" int __cdecl multi_yard_longest_queue(int* arr) noexcept {
    YardContext context{};
    return a2fo_rs_runtime_yard_queue(reinterpret_cast<std::uintptr_t>(arr),0,0,
        yard_read,yard_native,&context);
}
extern "C" void __cdecl multi_yard_cancel_filter(void* ci,int* arr) noexcept {
    YardContext context{nullptr,ci,{}};
    a2fo_rs_runtime_yard_queue(reinterpret_cast<std::uintptr_t>(arr),0,1,
        yard_read,yard_native,&context);
}
extern "C" int __cdecl multi_yard_selection_permit(void* selection,void* object) noexcept {
    YardContext context{};
    return a2fo_rs_runtime_yard_selection(g_active,reinterpret_cast<std::uintptr_t>(selection),
        reinterpret_cast<std::uintptr_t>(object),yard_read,yard_native,&context);
}
extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    const bool valid=api && api->api_version==A2FO_MODULE_API_VERSION &&
        A2FO_MODULE_API_HAS(api,register_game_object_class_loaded_handler) &&
        api->log && api->armada_module && api->fleetops_module &&
        api->patch_jump && api->patch_call && api->register_game_object_class_loaded_handler;
    YardContext context{api,nullptr,{}};
    const bool result=a2fo_rs_runtime_yard_init(valid,yard_native,yard_config,yard_log,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}
extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    YardContext context{};
    a2fo_rs_runtime_yard_shutdown(yard_native,&context);
}
