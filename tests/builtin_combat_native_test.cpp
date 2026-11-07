// Synthetic i386/Wine fixture for the two combat built-ins. No game process/UI.
#include "../core/builtin_modules.hpp"
#include "../core/module_loader.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" std::uintptr_t __cdecl test_swap(void*, std::uintptr_t, std::uintptr_t);
extern "C" std::uintptr_t __cdecl test_trigger(void*, std::uintptr_t);
extern "C" std::uintptr_t __cdecl test_damage(void*, void*, void*, void*, std::uintptr_t);
extern "C" std::uintptr_t __cdecl test_bool(void*, const char*, std::uint8_t*, std::uint32_t);
extern "C" std::uintptr_t __cdecl test_lookup(void*, const char*, void*, void*);
extern "C" std::uintptr_t __cdecl test_find(void*, void*, float*);

// Match the repository's thiscall_bridge.S contracts: ECX holds this and the
// native callee removes its own stack arguments.
extern "C" __attribute__((naked)) void swap1_bridge() {
    __asm__ volatile("pushl 4(%esp)\npushl $0\npushl %ecx\n"
                     "call _test_swap\naddl $12,%esp\nret $4");
}
extern "C" __attribute__((naked)) void swap2_bridge() {
    __asm__ volatile("pushl 8(%esp)\npushl 8(%esp)\npushl %ecx\n"
                     "call _test_swap\naddl $12,%esp\nret $8");
}
extern "C" __attribute__((naked)) void trigger_bridge() {
    __asm__ volatile("pushl 4(%esp)\npushl %ecx\ncall _test_trigger\naddl $8,%esp\nret $4");
}
extern "C" __attribute__((naked)) void damage_bridge() {
    __asm__ volatile("pushl 16(%esp)\npushl 16(%esp)\npushl 16(%esp)\n"
                     "pushl 16(%esp)\npushl %ecx\ncall _test_damage\n"
                     "addl $20,%esp\nret $16");
}
extern "C" __attribute__((naked)) void bool_bridge() {
    __asm__ volatile("pushl 12(%esp)\npushl 12(%esp)\npushl 12(%esp)\n"
                     "pushl %ecx\ncall _test_bool\naddl $16,%esp\nret $12");
}
extern "C" __attribute__((naked)) void lookup_bridge() {
    __asm__ volatile("pushl 12(%esp)\npushl 12(%esp)\npushl 12(%esp)\n"
                     "pushl %ecx\ncall _test_lookup\naddl $16,%esp\nret $12");
}
extern "C" __attribute__((naked)) void find_bridge() {
    __asm__ volatile("pushl 8(%esp)\npushl 8(%esp)\npushl %ecx\n"
                     "call _test_find\naddl $12,%esp\nret $8");
}

namespace {
using namespace a2fo;
using Swap1 = std::uintptr_t (__attribute__((fastcall)) *)(void*,void*,std::uintptr_t);
using Swap2 = std::uintptr_t (__attribute__((fastcall)) *)(void*,void*,std::uintptr_t,std::uintptr_t);
using Simulate = void (__cdecl *)(float);
using Damage = std::uintptr_t (__attribute__((fastcall)) *)(void*,void*,void*,void*,void*,std::uintptr_t);
std::uint8_t* image = nullptr;
std::string root;
bool root_owned = false, reject_registration = false, reject_hook = false;
unsigned begins = 0, ends = 0, registrations = 0, hooks = 0, patches = 0;
unsigned swaps = 0, simulations = 0, triggers = 0, damages = 0;
bool args_ok = true;
void* expected_craft = nullptr;
void* expected_weapon = nullptr;
void* expected_target = nullptr;
void* expected_database = nullptr;
void* original_damage = nullptr;
void* observed_damage_pointer = nullptr;
std::array<std::uint8_t,0x20> observed_damage{};
Swap1 swap1 = nullptr;
Swap2 swap2 = nullptr;
Simulate simulate = nullptr;
Damage damage = nullptr;
A2FO_WeaponClassLoadedHandler team_class = nullptr, damage_class = nullptr;
A2FO_CraftEventHandler craft_event = nullptr;
HMODULE directional_fixture = nullptr;
using DirectionalCalls = unsigned (A2FO_CALL *)(unsigned);
DirectionalCalls directional_calls = nullptr;

void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template<class T> void put(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base)+offset,&value,sizeof(value));
}
template<class T> T get(const void* base, std::size_t offset) {
    T value{}; std::memcpy(&value,static_cast<const std::uint8_t*>(base)+offset,sizeof(value)); return value;
}
void bytes(std::size_t offset, std::initializer_list<std::uint8_t> values) {
    std::memcpy(image+offset,values.begin(),values.size());
}
void jump(std::size_t offset, void* target) {
    bytes(offset,{0xb8,0,0,0,0,0xff,0xe0});
    put(image+offset,1,reinterpret_cast<std::uintptr_t>(target));
}
void A2FO_CALL log_message(const char*,const char*) {}
void log_line(const std::string&) {}
void* A2FO_CALL armada_module() { return image; }
void begin(const std::string&) { ++begins; }
void finish(const std::string&, bool) { ++ends; }
bool A2FO_CALL register_class(const char* owner,const char* const* fields,
                              std::uint32_t count,A2FO_WeaponClassLoadedHandler handler,void*) {
    check(handler && fields,"combat class registration");
    if (std::strcmp(owner,"A2FOTeamChangeWeapons")==0) {
        check(count==1 && std::strcmp(fields[0],"activateOnTeamChange")==0,"team field");
        team_class=handler;
    } else {
        check(std::strcmp(owner,"A2FOWeaponDamageControls")==0 && count==4 &&
              std::strcmp(fields[0],"canDamageShields")==0 &&
              std::strcmp(fields[3],"hullDamageModifier")==0,"damage fields");
        damage_class=handler;
    }
    ++registrations;
    return !reject_registration;
}
bool A2FO_CALL register_craft(const char* owner,std::uint32_t mask,
                             A2FO_CraftEventHandler handler,void*) {
    check(std::strcmp(owner,"A2FOTeamChangeWeapons")==0 && handler &&
          mask==(A2FO_CRAFT_EVENT_MASK_CLEANUP|A2FO_CRAFT_EVENT_MASK_POST_LOAD),"craft registration");
    craft_event=handler; ++registrations; return !reject_registration;
}
void __cdecl native_simulate(float) { ++simulations; }
void* __cdecl native_get_craft(std::uint32_t handle) {
    return expected_craft && get<std::uint32_t>(expected_craft,0x28)==handle ? expected_craft : nullptr;
}
void* __attribute__((fastcall)) native_target(void* weapon,void*) {
    args_ok &= weapon==expected_weapon; return expected_target;
}
void __attribute__((fastcall)) native_lookup_ctor(void* self,void*) {
    std::memset(self,0,0x18);
}
void __attribute__((fastcall)) native_lookup_dtor(void*,void*) {}
bool A2FO_CALL patch(void* target,void*,const std::uint8_t* expected,std::size_t count) {
    check(target==image+0xc5f08 && count==10 &&
          std::memcmp(target,expected,count)==0,"damage hull patch preflight");
    ++patches; return true;
}
bool A2FO_CALL install(void* target,void* replacement,std::size_t count,
                       const std::uint8_t* expected,A2FO_InlineHook* hook) {
    check(std::memcmp(target,expected,count)==0,"combat hook signature");
    ++hooks;
    if (reject_hook && (hooks==2 || target==image+0xc5bb0)) return false;
    if (target==image+0x26eb00) {
        simulate=reinterpret_cast<Simulate>(replacement);
        *hook={target,reinterpret_cast<void*>(&native_simulate),static_cast<std::uint32_t>(count)};
    } else if (target==image+0xd0ea0) {
        swap1=reinterpret_cast<Swap1>(replacement);
        *hook={target,reinterpret_cast<void*>(&swap1_bridge),static_cast<std::uint32_t>(count)};
    } else if (target==image+0xd0ed0) {
        swap2=reinterpret_cast<Swap2>(replacement);
        *hook={target,reinterpret_cast<void*>(&swap2_bridge),static_cast<std::uint32_t>(count)};
    } else {
        check(target==image+0xc5bb0,"damage hook target");
        damage=reinterpret_cast<Damage>(replacement);
        *hook={target,reinterpret_cast<void*>(&damage_bridge),static_cast<std::uint32_t>(count)};
    }
    return true;
}
void seed(bool bad_signature) {
    image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x400000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    check(image,"synthetic Armada image");
    bytes(0xd0ea0,{0x55,0x8b,0xec,0x56,0x8b,0xf1});
    bytes(0xd0ed0,{0x55,0x8b,0xec,0x56,0x8b,0xf1});
    bytes(0x26eb00,{0x55,0x8b,0xec,0xa1,0,0,0,0});
    put(image+0x26eb00,4,reinterpret_cast<std::uintptr_t>(image+0x3b5564));
    bytes(0x13800,{0x55,0x8b,0xec,0x8b,0x45,0x08});
    bytes(0x271290,{0x55,0x8b,0xec,0x8b,0x45,0x08});
    bytes(0x271300,{0x8b,0x49,0x38,0x51,0xe8});
    bytes(0xc5bb0,{0x55,0x8b,0xec,0x83,0xec,0x10});
    bytes(0xc5f08,{0x8b,0x43,0x04,0xc7,0x45,0xfc,0,0,0,0});
    bytes(0x134f50,{0x55,0x8b,0xec,0x81,0xec,0,1,0,0});
    bytes(0x135630,{0x55,0x8b,0xec,0x81,0xec,4,1,0,0});
    bytes(0x25cfb0,{0x8b,0xc1,0x33,0xc9,0xc7,0});
    bytes(0x25cfd0,{0xc7,0x01,0x84,0xce,0x6b,0});
    bytes(0x25d170,{0x55,0x8b,0xec,0x51,0x53,0x56,0x8b,0x71,0x08});
    if (bad_signature) { image[0xd0ea0]^=1; image[0xc5bb0]^=1; }
}
void stub_native_calls() {
    jump(0x13800,reinterpret_cast<void*>(&native_get_craft));
    jump(0x271290,reinterpret_cast<void*>(&trigger_bridge));
    jump(0x271300,reinterpret_cast<void*>(&native_target));
    jump(0x134f50,reinterpret_cast<void*>(&bool_bridge));
    jump(0x135630,reinterpret_cast<void*>(&lookup_bridge));
    jump(0x25cfb0,reinterpret_cast<void*>(&native_lookup_ctor));
    jump(0x25cfd0,reinterpret_cast<void*>(&native_lookup_dtor));
    jump(0x25d170,reinterpret_cast<void*>(&find_bridge));
}
void write_file(const std::string& path,const char* contents) {
    HANDLE file=CreateFileA(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(file!=INVALID_HANDLE_VALUE,"fixture file");
    DWORD written=0;
    check(WriteFile(file,contents,static_cast<DWORD>(std::strlen(contents)),&written,nullptr)!=0 &&
          written==std::strlen(contents),"fixture write");
    CloseHandle(file);
}
ModulePolicy policy(bool team_enabled, bool damage_enabled) {
    ModuleRules rules; rules.has_section=true;
    rules.rejected.push_back("A2FOM3D");
    if(team_enabled) rules.active.push_back("A2FOTeamChangeWeapons");
    if(damage_enabled) rules.active.push_back("A2FOWeaponDamageControls");
    return evaluate_module_rules({rules},discover_installed_modules(root));
}
ModulePolicy policy(bool enabled) { return policy(enabled,enabled); }
void toggle(bool team_enabled, bool damage_enabled) {
    std::string error;
    const bool applied=apply_builtin_module_policy({root},policy(team_enabled,damage_enabled),error);
    check(applied,error.c_str());
    check(builtin_module_enabled(BuiltinFeature::team_change_weapons)==team_enabled &&
          builtin_module_enabled(BuiltinFeature::weapon_damage_controls)==damage_enabled,"independent combat live gates");
}
void toggle(bool enabled) { toggle(enabled,enabled); }
std::string fixture_info(bool legacy, bool rejected) {
    const std::string mod_section = "[mod]\nName=Fixture\n";
    if (legacy) return mod_section;
    std::string contents = "[modules]\n";
    unsigned reject_number = 0;
    for (unsigned index = 0; index < static_cast<unsigned>(BuiltinFeature::count); ++index) {
        const auto feature = static_cast<BuiltinFeature>(index);
        if (!rejected && (feature == BuiltinFeature::team_change_weapons ||
                          feature == BuiltinFeature::weapon_damage_controls)) continue;
        contents += "reject" + std::to_string(++reject_number) + "=" +
                    canonical_builtin_name(feature) + "\n";
    }
    return contents;
}
void cleanup() {
    // Damage-policy NativeLookup destructors still call synthetic image thunks
    // during process teardown, matching the production process-lifetime image.
    if(!root_owned) return;
    DeleteFileA((root+"\\info.ini").c_str());
    DeleteFileA((root+"\\modules\\A2FOTeamChangeWeapons.dll").c_str());
    DeleteFileA((root+"\\modules\\A2FOWeaponDamageControls.dll").c_str());
    RemoveDirectoryA((root+"\\modules").c_str()); RemoveDirectoryA(root.c_str());
}
void fire_event(std::uint32_t kind,void* craft) {
    A2FO_CraftEvent event{};event.struct_size=sizeof(event);event.kind=kind;event.craft=craft;
    craft_event(&event,nullptr);
}
void run(const char* mode) {
    const bool legacy=std::strcmp(mode,"legacy")==0;
    const bool rejected=std::strcmp(mode,"rejected")==0;
    const bool signature_fail=std::strcmp(mode,"signature-fail")==0;
    const bool dispatch_fail=std::strcmp(mode,"dispatch-fail")==0;
    const bool craft_dispatch_fail=std::strcmp(mode,"craft-dispatch-fail")==0;
    reject_registration=std::strcmp(mode,"registration-fail")==0;
    reject_hook=std::strcmp(mode,"hook-fail")==0;
    check(std::strcmp(mode,"success")==0 || legacy || rejected || signature_fail ||
          dispatch_fail || craft_dispatch_fail || reject_registration || reject_hook,"known mode");
    char temp[MAX_PATH]{}; check(GetTempPathA(sizeof(temp),temp)!=0,"temp root");
    root=std::string(temp)+"a2fo-builtin-combat-"+std::to_string(GetCurrentProcessId());
    check(CreateDirectoryA(root.c_str(),nullptr)!=0,"private root");root_owned=true;
    check(discover_installed_modules(root).size()==static_cast<std::size_t>(BuiltinFeature::count),"synthetic builtin inventory");
    CreateDirectoryA((root+"\\modules").c_str(),nullptr);
    write_file(root+"\\modules\\A2FOTeamChangeWeapons.dll","invalid leftover");
    write_file(root+"\\modules\\A2FOWeaponDamageControls.dll","invalid leftover");
    std::vector<std::string> diagnostics;
    check(discover_installed_modules(root,&diagnostics).size()==static_cast<std::size_t>(BuiltinFeature::count) && diagnostics.size()==2,
          "stale combat DLLs ignored");
    const std::string info = fixture_info(legacy, rejected);
    write_file(root+"\\info.ini",info.c_str());
    seed(signature_fail);
    A2FO_ModuleApi api{};api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
    api.api_revision=A2FO_MODULE_API_REVISION;
    api.capabilities=A2FO_CAP_WEAPON_CLASS_LOADED|A2FO_CAP_CRAFT_EVENTS;
    api.log=&log_message;api.armada_module=&armada_module;
    api.install_inline_hook=&install;api.patch_jump=&patch;
    api.register_weapon_class_loaded_handler=&register_class;
    api.register_craft_event_handler_masked=&register_craft;
    LoadedModules loaded;
    check(load_native_modules_from_roots({root},api,loaded,&log_line,{&begin,&finish}),"combat loader");
    check(std::all_of(loaded.begin(),loaded.end(),
                      [](const LoadedModule& module) { return module.handle==nullptr; }),
          "stale DLLs never loaded");
    finish_builtin_module_preparation(true,true,!dispatch_fail,true,!craft_dispatch_fail);
    if(rejected) {
        check(begins==0 && ends==0 && registrations==0 && hooks==0 && patches==0,
              "rejected combat features never prepared");
        return;
    }
    check(begins==(legacy?static_cast<unsigned>(BuiltinFeature::count):2u) && ends==begins,"eligible preparation count");
    const bool damage_available=!signature_fail && !dispatch_fail &&
                                !reject_registration && !reject_hook;
    const bool team_available=damage_available && !craft_dispatch_fail;
    check(builtin_module_available(BuiltinFeature::team_change_weapons)==team_available &&
          builtin_module_available(BuiltinFeature::weapon_damage_controls)==damage_available,
          "combat availability follows native prerequisites");
    check(builtin_module_enabled(BuiltinFeature::team_change_weapons)==legacy &&
          builtin_module_enabled(BuiltinFeature::weapon_damage_controls)==legacy,
          "managed-off and legacy-on startup");
    if(!team_available || !damage_available) {
        std::string error;
        check(!apply_builtin_module_policy({root},policy(true),error) && !error.empty(),
              "unavailable activation reports error");
        return;
    }
    check(team_class && damage_class && craft_event && swap1 && swap2 && simulate && damage &&
          hooks==4 && patches==1 && registrations==3,"production callbacks and detours captured");
    if(directional_calls) check(directional_calls(0)>=1,"directional bridge connected");
    stub_native_calls(); // Only after both modules have completed signature preflight.
    std::array<std::uint8_t,0x240> craft{}, klass{}, weapon{}, carrier{}, ordnance{}, object_class{};
    std::array<void*,1> slots{weapon.data()};
    int project=17;
    expected_craft=craft.data();expected_weapon=weapon.data();expected_target=&project;
    expected_database=&project;
    put(craft.data(),0x28,std::uint32_t{42}); put(craft.data(),0xec,std::int32_t{1});
    put(craft.data(),0x128,carrier.data()); put(craft.data(),0x40,object_class.data());
    put(craft.data(),0x1c8,10.0f); put(object_class.data(),0x1cc,static_cast<void*>(&project));
    put(carrier.data(),0x0c,slots.data());put(carrier.data(),0x10,slots.data()+1);
    put(weapon.data(),0x04,klass.data());put(weapon.data(),0x18,std::uint32_t{42});
    put(klass.data(),0x1de,std::uint8_t{1});
    put(ordnance.data(),0x38,weapon.data());
    put(ordnance.data()+0x3c,0,12.0f);put(ordnance.data()+0x3c,0x14,std::uint32_t{0});
    original_damage=ordnance.data()+0x3c;
    const A2FO_OdfFieldView field[]{{{"activateOnTeamChange",20},{"1",1}}};
    A2FO_WeaponClassLoadedEvent event{};event.struct_size=sizeof(event);event.weapon_class=klass.data();
    event.parameter_db=expected_database;event.odf_fields=field;event.odf_field_count=1;
    if(!legacy) toggle(false);
    team_class(&event,nullptr);damage_class(&event,nullptr); // resolved metadata arrives while off
    if(!legacy) toggle(true);
    auto hit=[&](bool modified) {
        const unsigned before=damages;
        const unsigned directional_begin=directional_calls ? directional_calls(1) : 0;
        const unsigned directional_end=directional_calls ? directional_calls(2) : 0;
        check(damage(craft.data(),nullptr,original_damage,&project,&project,99)==0x76543210,
              "native Craft::Damage return retained");
        check(damages==before+1 && args_ok,"native Craft::Damage called once with arguments");
        if(directional_calls) check(directional_calls(1)==directional_begin+1 &&
                                    directional_calls(2)==directional_end+1,
                                    "directional begin/end remain active across damage toggles");
        check((observed_damage_pointer!=original_damage)==modified,
              "enabled policy uses a private DamageInfo");
        check((get<std::uint32_t>(observed_damage.data(),0x14)==0x80u)==modified,
              "damage policy follows live gate");
        check(get<float>(ordnance.data()+0x3c,0)==12.0f &&
              get<std::uint32_t>(ordnance.data()+0x3c,0x14)==0,"shared DamageInfo unchanged");
    };
    hit(true);
    check(swap1(craft.data(),nullptr,2)==0x12345678,"native team-swap return");
    check(triggers==0,"team activation waits for simulation");
    toggle(true); // Re-saving the same policy must retain the queued activation.
    simulate(0);check(triggers==0,"zero elapsed does not activate");
    simulate(0.1f);check(triggers==1,"team swap activates on positive simulation");
    check(swap2(craft.data(),nullptr,7,3)==0x12345678,"native race/team-swap return");
    simulate(0.1f);check(triggers==2,"race/team swap activates");
    check(swap1(craft.data(),nullptr,4)==0x12345678,"queue before disable");
    toggle(false);hit(false);
    check(swap1(craft.data(),nullptr,5)==0x12345678,"disabled ownership still native");
    toggle(true);hit(true);simulate(0.1f);
    check(triggers==2,"off/on cancels queued activation and disabled event");
    check(swap1(craft.data(),nullptr,6)==0x12345678,"queue cleanup candidate");
    fire_event(A2FO_CRAFT_EVENT_CLEANUP,craft.data());simulate(0.1f);
    check(triggers==2,"cleanup clears pending activation");
    check(swap1(craft.data(),nullptr,7)==0x12345678,"queue postload candidate");
    fire_event(A2FO_CRAFT_EVENT_POST_LOAD,craft.data());simulate(0.1f);
    check(triggers==2,"postload clears pending activation");
    toggle(false,true);hit(true);
    check(swap1(craft.data(),nullptr,8)==0x12345678,"native swap with only damage controls enabled");
    simulate(0.1f);check(triggers==2,"damage controls do not enable team-change activation");
    toggle(true,false);hit(false);
    check(swap1(craft.data(),nullptr,9)==0x12345678,"native swap with only team-change enabled");
    simulate(0.1f);check(triggers==3,"team-change activation works with damage controls disabled");
    check(swaps==8 && simulations==8 && damages==5 && args_ok,
          "native gateways retained throughout toggles");
}
} // namespace

extern "C" std::uintptr_t __cdecl test_swap(void* craft,std::uintptr_t race,std::uintptr_t team) {
    (void)race;
    args_ok &= craft==expected_craft;
    put(craft,0xec,static_cast<std::int32_t>(team));++swaps;return 0x12345678;
}
extern "C" std::uintptr_t __cdecl test_trigger(void* weapon,std::uintptr_t target) {
    args_ok &= weapon==expected_weapon && reinterpret_cast<void*>(target)==expected_target;
    ++triggers;return 1;
}
extern "C" std::uintptr_t __cdecl test_damage(void* craft,void* info,void* a2,void* a3,std::uintptr_t a4) {
    args_ok &= craft==expected_craft && a2==expected_database &&
               a3==expected_database && a4==99;
    observed_damage_pointer=info;
    std::memcpy(observed_damage.data(),info,observed_damage.size());
    ++damages;return 0x76543210;
}
extern "C" std::uintptr_t __cdecl test_bool(void* database,const char* key,std::uint8_t* value,std::uint32_t) {
    args_ok &= database==expected_database;
    if(std::strcmp(key,"canDamageHull")==0) { *value=0;return 1; }
    return 0;
}
extern "C" std::uintptr_t __cdecl test_lookup(void* database,const char*,void*,void*) {
    args_ok &= database==expected_database;return 0;
}
extern "C" std::uintptr_t __cdecl test_find(void*,void*,float*) { return 0; }

int main(int argc,char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    try {
        if(argc>2) {
            directional_fixture=LoadLibraryA(argv[2]);
            check(directional_fixture,"load synthetic directional bridge fixture");
            FARPROC address=GetProcAddress(
                directional_fixture,"A2FOBuiltinFixture_DirectionalCalls");
            static_assert(sizeof(address)==sizeof(directional_calls),"i386 export pointer");
            std::memcpy(&directional_calls,&address,sizeof(address));
            check(directional_calls,"directional bridge fixture counters");
        }
        run(argc>1?argv[1]:"success");cleanup();std::puts("builtin combat native: PASS");return 0;
    }
    catch(const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());cleanup();return 1;}
}
