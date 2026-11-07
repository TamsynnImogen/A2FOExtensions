// Actual frozen/current team-change module compiled into a native mock.
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
static std::uint64_t builtin_epoch = 0x100000001ULL;
namespace a2fo {
std::uint64_t builtin_module_activation(BuiltinFeature) noexcept { return builtin_epoch; }
}
#endif
namespace {
std::vector<std::string> trace;
std::uint8_t* image = nullptr;
void* craft_ptr = nullptr;
void* target_ptr = nullptr;
unsigned triggers = 0, simulations = 0, installations = 0;
bool reject_registration = false;
unsigned reject_hook_at = 0;
A2FO_WeaponClassLoadedHandler class_callback = nullptr;
A2FO_CraftEventHandler craft_callback = nullptr;
template<class T> void put(void* base,std::size_t offset,T value) {
    std::memcpy(static_cast<std::uint8_t*>(base)+offset,&value,sizeof(value));
}
void bytes(std::size_t offset,std::initializer_list<std::uint8_t> values) {
    std::memcpy(image+offset,values.begin(),values.size());
}
void jump(std::size_t offset,void* target) {
    bytes(offset,{0xb8,0,0,0,0,0xff,0xe0});
    put(image+offset,1,reinterpret_cast<std::uintptr_t>(target));
}
void A2FO_CALL logger(const char*,const char* message) {
    trace.emplace_back(std::string("log:")+message);
}
void* A2FO_CALL armada_module() { return image; }
bool A2FO_CALL register_class(const char*,const char* const*,std::uint32_t count,
                              A2FO_WeaponClassLoadedHandler cb,void*) {
    trace.emplace_back("register-class:"+std::to_string(count));
    class_callback=cb; return !reject_registration;
}
bool A2FO_CALL register_craft(const char*,std::uint32_t mask,
                              A2FO_CraftEventHandler cb,void*) {
    trace.emplace_back("register-craft:"+std::to_string(mask));
    craft_callback=cb; return !reject_registration;
}
void __cdecl native_simulate(float) { ++simulations; }
bool A2FO_CALL install(void* target,void*,std::size_t count,const std::uint8_t* expected,
                       A2FO_InlineHook* hook) {
    ++installations;
    trace.emplace_back("install:"+std::to_string(reinterpret_cast<std::uintptr_t>(target)-
        reinterpret_cast<std::uintptr_t>(image))+":"+std::to_string(count)+":"+
        std::to_string(std::memcmp(target,expected,count)==0));
    if (installations==reject_hook_at) return false;
    hook->gateway = target==image+0x26eb00 ? reinterpret_cast<void*>(&native_simulate) :
        target==image+0xd0ea0 ? reinterpret_cast<void*>(1) : reinterpret_cast<void*>(2);
    return true;
}
void seed() {
    image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x400000,
        MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    bytes(0xd0ea0,{0x55,0x8b,0xec,0x56,0x8b,0xf1});
    bytes(0xd0ed0,{0x55,0x8b,0xec,0x56,0x8b,0xf1});
    bytes(0x26eb00,{0x55,0x8b,0xec,0xa1,0,0,0,0});
    put(image+0x26eb00,4,reinterpret_cast<std::uintptr_t>(image+0x3b5564));
    bytes(0x13800,{0x55,0x8b,0xec,0x8b,0x45,0x08});
    bytes(0x271290,{0x55,0x8b,0xec,0x8b,0x45,0x08});
    bytes(0x271300,{0x8b,0x49,0x38,0x51,0xe8});
}
void setup_native();
void class_event(void* klass,const char* value,bool valid=true) {
    A2FO_OdfFieldView fields[]{{{"activateOnTeamChange",20},{value,
        static_cast<std::uint32_t>(std::strlen(value))}}};
    A2FO_WeaponClassLoadedEvent event{};
    event.struct_size=valid?sizeof(event):0;
    event.weapon_class=klass; event.odf_fields=fields; event.odf_field_count=1;
    class_callback(&event,nullptr);
    trace.emplace_back(std::string("class:")+value+":"+std::to_string(g_classes.count(klass)));
}
void craft_event_kind(std::uint32_t kind,void* craft,bool valid=true) {
    A2FO_CraftEvent event{};
    event.struct_size=valid?sizeof(event):0;event.kind=kind;event.craft=craft;
    craft_callback(&event,nullptr);
    trace.emplace_back("craft-event:"+std::to_string(kind)+":"+std::to_string(g_pending.size()));
}
void short_event_cases() {
    SYSTEM_INFO system{};GetSystemInfo(&system);
    const std::size_t page=system.dwPageSize;
    auto* guarded=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,2*page,
        MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!guarded)throw std::runtime_error("short event allocation failed");
    DWORD previous=0;
    if(!VirtualProtect(guarded+page,page,PAGE_NOACCESS,&previous))
        throw std::runtime_error("short event protection failed");
    const std::uint32_t size=0;
    std::memcpy(guarded+page-sizeof(size),&size,sizeof(size));
    const auto* short_event=guarded+page-sizeof(size);
    class_callback(reinterpret_cast<const A2FO_WeaponClassLoadedEvent*>(short_event),nullptr);
    trace.emplace_back("short-class:"+std::to_string(g_classes.size()));
    craft_callback(reinterpret_cast<const A2FO_CraftEvent*>(short_event),nullptr);
    trace.emplace_back("short-craft:"+std::to_string(g_pending.size()));
    VirtualFree(guarded,0,MEM_RELEASE);
}
void pass(float elapsed) {
    simulate_all_hook(elapsed);
    trace.emplace_back("pass:"+std::to_string(triggers)+":"+
                       std::to_string(simulations)+":"+std::to_string(g_pending.size()));
}
void run() {
    seed();
    A2FO_ModuleApi api{};
    api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
    api.capabilities=A2FO_CAP_WEAPON_CLASS_LOADED|A2FO_CAP_CRAFT_EVENTS;
    api.log=&logger;api.armada_module=&armada_module;
    api.install_inline_hook=&install;api.register_weapon_class_loaded_handler=&register_class;
    api.register_craft_event_handler_masked=&register_craft;
    trace.emplace_back(std::string("init:")+(A2FO_ModuleInit(&api)?"1":"0"));
    setup_native();
    std::array<std::uint8_t,0x130> craft{},weapon{};
    std::array<std::uint8_t,0x1e0> klass{};
    std::array<std::uint8_t,0x20> carrier{};
    void* slots[]{weapon.data()};
    craft_ptr=craft.data();target_ptr=craft.data();
    put(craft.data(),0x28,std::uint32_t(11));put(craft.data(),0xec,std::int32_t(1));
    put(craft.data(),0x128,carrier.data());
    put(carrier.data(),0x0c,slots);put(carrier.data(),0x10,slots+1);
    put(weapon.data(),0x18,std::uint32_t(11));put(weapon.data(),0x04,klass.data());
    put(klass.data(),0x1de,std::uint8_t(1));
    class_event(klass.data(),"TrUe");
    short_event_cases();
    swap_team_hook(craft.data(),nullptr,2);
    trace.emplace_back("queued:"+std::to_string(g_pending.size()));
    pass(0.0f);pass(0.1f);
    swap_race_hook(craft.data(),nullptr,0,3);pass(0.1f);
    put(weapon.data(),0x2d,std::uint8_t(1));
    swap_team_hook(craft.data(),nullptr,4);pass(0.1f);
    put(weapon.data(),0x2d,std::uint8_t(0));
    class_event(klass.data(),"0");
    swap_team_hook(craft.data(),nullptr,5);pass(0.1f);
    class_event(klass.data(),"1");
    swap_team_hook(craft.data(),nullptr,6);
    craft_event_kind(A2FO_CRAFT_EVENT_CLEANUP,craft.data());pass(0.1f);
    swap_team_hook(craft.data(),nullptr,7);
    craft_event_kind(A2FO_CRAFT_EVENT_POST_LOAD,craft.data());pass(0.1f);
#ifdef TEST_BUILTIN
    swap_team_hook(craft.data(),nullptr,8);
    builtin_epoch=0;
    pass(0.1f);
    builtin_epoch=0x100000002ULL;
    swap_team_hook(craft.data(),nullptr,9);pass(0.1f);
#endif
    A2FO_ModuleShutdown();
    trace.emplace_back("shutdown:"+std::to_string(g_classes.size())+":"+
                       std::to_string(g_pending.size()));
    class_callback(reinterpret_cast<const A2FO_WeaponClassLoadedEvent*>(1),nullptr);
    trace.emplace_back("disabled-poison-class:"+std::to_string(g_classes.size()));
    reject_registration=true;
    trace.emplace_back(std::string("registration-fail:")+(A2FO_ModuleInit(&api)?"1":"0"));
    reject_registration=false;
    installations=0; reject_hook_at=2;
    bytes(0x13800,{0x55,0x8b,0xec,0x8b,0x45,0x08});
    bytes(0x271300,{0x8b,0x49,0x38,0x51,0xe8});
    trace.emplace_back(std::string("partial-hook:")+(A2FO_ModuleInit(&api)?"1":"0")+":"+
                       std::to_string(g_ready)+":"+std::to_string(installations));
    pass(0.1f); A2FO_ModuleShutdown();
    reject_hook_at=0; image[0xd0ea0]^=1;
    trace.emplace_back(std::string("signature-fail:")+(A2FO_ModuleInit(&api)?"1":"0"));
    api.capabilities=0;
    trace.emplace_back(std::string("cap-fail:")+(A2FO_ModuleInit(&api)?"1":"0"));
    trace.emplace_back(std::string("null-api:")+(A2FO_ModuleInit(nullptr)?"1":"0"));
    VirtualFree(image,0,MEM_RELEASE);
}
void* __cdecl native_get_craft(std::uint32_t handle) {
    return craft_ptr && read<std::uint32_t>(craft_ptr,0x28)==handle ? craft_ptr : nullptr;
}
void* __fastcall native_get_target(void*,void*) { return target_ptr; }
void setup_native() {
    jump(0x13800,reinterpret_cast<void*>(&native_get_craft));
    jump(0x271300,reinterpret_cast<void*>(&native_get_target));
}
}
extern "C" std::uintptr_t __cdecl a2fo_team_change_thiscall_1(
    void* function,void* self,std::uintptr_t arg) {
    if (function==reinterpret_cast<void*>(1)) {
        put(self,0xec,static_cast<std::int32_t>(arg));return 101;
    }
    ++triggers;
    trace.emplace_back("trigger:"+std::to_string(arg==reinterpret_cast<std::uintptr_t>(target_ptr)));
    return 202;
}
extern "C" std::uintptr_t __cdecl a2fo_team_change_thiscall_2(
    void*,void* self,std::uintptr_t,std::uintptr_t team) {
    put(self,0xec,static_cast<std::int32_t>(team));return 303;
}
int main(){run();for(const auto& line:trace)std::printf("%s\n",line.c_str());}
