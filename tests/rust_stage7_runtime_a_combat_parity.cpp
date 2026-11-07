// Actual frozen/current module source compiled into a headless native mock.
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include MODULE_SOURCE
#ifdef TEST_BUILTIN
static bool builtin_enabled = true;
namespace a2fo {
bool builtin_module_enabled(BuiltinFeature) noexcept { return builtin_enabled; }
}
#endif
namespace {
std::vector<std::string> trace;
std::uint8_t* armada = nullptr;
std::uint8_t* fleet = nullptr;
void* owner_result = nullptr;
bool native_allowed = true;
bool register_ok = true;
A2FO_WeaponTriggerHandler trigger_handler = nullptr;
void A2FO_CALL logger(const char*, const char* message) { trace.emplace_back(std::string("log:") + message); }
void* A2FO_CALL armada_module() { return armada; }
void* A2FO_CALL fleetops_module() { return fleet; }
bool A2FO_CALL reg_legacy(const char*, A2FO_WeaponTriggerHandler cb, void*) {
    trace.emplace_back("register:legacy"); trigger_handler = cb; return register_ok;
}
bool A2FO_CALL reg_masked(const char*, std::uint32_t mask,
                          A2FO_WeaponTriggerHandler cb, void*) {
    trace.emplace_back("register:masked:" + std::to_string(mask));
    trigger_handler = cb; return register_ok;
}
template<class T> void put(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base)+offset, &value, sizeof(value));
}
void event_case(const char* name, void* weapon, std::uint32_t kind, bool valid = true) {
    A2FO_WeaponTriggerEvent event{};
    event.struct_size = valid ? sizeof(event) : 0;
    event.kind = kind;
    event.weapon = weapon;
    trace.emplace_back(std::string(name)+":"+
        (trigger_handler(&event,nullptr) ? "1" : "0"));
}
void short_event_case() {
    SYSTEM_INFO system{}; GetSystemInfo(&system);
    const std::size_t page = system.dwPageSize;
    auto* guarded = static_cast<std::uint8_t*>(VirtualAlloc(nullptr,2*page,
        MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if (!guarded) throw std::runtime_error("short event allocation failed");
    DWORD previous=0;
    if (!VirtualProtect(guarded+page,page,PAGE_NOACCESS,&previous))
        throw std::runtime_error("short event protection failed");
    const std::uint32_t size=0;
    std::memcpy(guarded+page-sizeof(size),&size,sizeof(size));
    trace.emplace_back(std::string("short-event:")+
        (trigger_handler(reinterpret_cast<const A2FO_WeaponTriggerEvent*>(
            guarded+page-sizeof(size)),nullptr)?"1":"0"));
    VirtualFree(guarded,0,MEM_RELEASE);
}
void run() {
    armada = static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x300000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    fleet = static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x220000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    std::memcpy(armada+0x271050,"\x8b\x49\x18\x51\xe8",5);
    const std::uint8_t signature[]{0x53,0x51,0x89,0x14,0x24,0x33,0xd2,0x8b,
        0x40,0x0c,0x8b,0x0c,0x24,0x8b,0x5c,0x88};
    std::memcpy(fleet+0x120680,signature,sizeof(signature));
    A2FO_ModuleApi api{};
    api.struct_size=sizeof(api); api.api_version=A2FO_MODULE_API_VERSION;
    api.capabilities=A2FO_CAP_WEAPON_TRIGGER_EVENTS; api.log=&logger;
    api.armada_module=&armada_module; api.fleetops_module=&fleetops_module;
    api.register_weapon_trigger_handler=&reg_legacy;
    api.register_weapon_trigger_handler_masked=&reg_masked;
    trace.emplace_back(std::string("init:")+(A2FO_ModuleInit(&api)?"1":"0"));
    std::array<std::uint8_t,0x210> klass{};
    std::array<std::uint8_t,0x100> weapon{}, owner{};
    std::array<std::uint8_t,8> project{}, item{}, tree_items{};
    std::array<std::uint8_t,0x20> tree{};
    void* trees[64]{};
    owner_result=owner.data();
    put(weapon.data(),4,klass.data()); put(klass.data(),0x208,project.data());
    put(project.data(),0,std::uint32_t(1)); put(owner.data(),0xec,std::int32_t(0));
    put(fleet+0x212f08,0,trees); trees[0]=tree.data();
    put(tree.data(),0x0c,tree_items.data());
    put(tree_items.data(),0,item.data());
    event_case("committed",weapon.data(),A2FO_WEAPON_TRIGGER_COMMITTED);
    event_case("unknown",weapon.data(),99);
    event_case("invalid-event",weapon.data(),A2FO_WEAPON_TRIGGER_PRECHECK,false);
    short_event_case();
    event_case("listed-allowed",weapon.data(),A2FO_WEAPON_TRIGGER_PRECHECK);
    native_allowed=false; event_case("listed-blocked",weapon.data(),A2FO_WEAPON_TRIGGER_PRECHECK);
    put(tree_items.data(),0,static_cast<void*>(nullptr));
    event_case("unlisted",weapon.data(),A2FO_WEAPON_TRIGGER_PRECHECK);
    put(klass.data(),0x1b4,std::uint8_t(1));
    event_case("special",weapon.data(),A2FO_WEAPON_TRIGGER_PRECHECK);
    put(klass.data(),0x1b4,std::uint8_t(0));
    put(project.data(),0,std::uint32_t(0));
    event_case("project-zero",weapon.data(),A2FO_WEAPON_TRIGGER_PRECHECK);
    put(project.data(),0,std::uint32_t(0x80000000));
    event_case("project-high",weapon.data(),A2FO_WEAPON_TRIGGER_PRECHECK);
    put(project.data(),0,std::uint32_t(1));
    put(owner.data(),0xec,std::int32_t(-1));
    event_case("bad-team",weapon.data(),A2FO_WEAPON_TRIGGER_PRECHECK);
    put(owner.data(),0xec,std::int32_t(0));
    owner_result=nullptr; event_case("no-owner",weapon.data(),A2FO_WEAPON_TRIGGER_PRECHECK);
    owner_result=owner.data();
#ifdef TEST_BUILTIN
    builtin_enabled=false;
    event_case("disabled",weapon.data(),A2FO_WEAPON_TRIGGER_PRECHECK);
    trace.emplace_back(std::string("direct-disabled:")+
        (A2FONormalWeaponTech_AllowWeaponTrigger(weapon.data(),nullptr)?"1":"0"));
    builtin_enabled=true;
#endif
    A2FO_ModuleShutdown();
    event_case("shutdown",weapon.data(),A2FO_WEAPON_TRIGGER_PRECHECK);
    register_ok=false;
    trace.emplace_back(std::string("reg-fail:")+(A2FO_ModuleInit(&api)?"1":"0"));
    api.capabilities=0;
    trace.emplace_back(std::string("cap-fail:")+(A2FO_ModuleInit(&api)?"1":"0"));
    trace.emplace_back(std::string("null-api:")+(A2FO_ModuleInit(nullptr)?"1":"0"));
    VirtualFree(armada,0,MEM_RELEASE); VirtualFree(fleet,0,MEM_RELEASE);
}
}
extern "C" std::uintptr_t __cdecl a2fo_normal_weapon_tech_call_thiscall_0(void*,void*) {
    return reinterpret_cast<std::uintptr_t>(owner_result);
}
extern "C" std::uintptr_t __cdecl a2fo_normal_weapon_tech_call_delphi_2(void*,std::uintptr_t,std::uintptr_t) {
    return native_allowed ? 1 : 0;
}
int main(){ run(); for(const auto& line:trace) std::printf("%s\n",line.c_str()); }
