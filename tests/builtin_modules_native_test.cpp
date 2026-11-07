// Headless production-module fixture. Only synthetic buffers/stubs execute;
// no Armada/FleetOps binary, window, input, or live game process is used.
#include "../core/builtin_modules.hpp"
#include "../core/module_loader.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" std::uintptr_t __cdecl a2fo_build_tooltips_button_text_hook_cpp(void*, void*) noexcept;
extern "C" std::uintptr_t __cdecl a2fo_build_tooltips_button_verbose_hook_cpp(void*, void*) noexcept;
extern "C" bool A2FO_CALL A2FONormalWeaponTech_AllowWeaponTrigger(void*, const void*);

namespace {
using namespace a2fo;
unsigned inline_calls=0, patch_calls=0, destroyed_regs=0, weapon_regs=0, begins=0, ends=0;
A2FO_ObjectDestroyedHandler destroyed=nullptr;
A2FO_WeaponTriggerHandler trigger=nullptr;
std::uint8_t *armada=nullptr, *fleet=nullptr;
std::string root;
bool partial=false, mismatch=false, fixture_owned=false;
std::string rendered;
using Lookup=std::uintptr_t (__attribute__((fastcall)) *)(void*,void*,const char*);
Lookup lookup=nullptr;
float build_seconds=20.0f;
void check(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
template<class T> void put(void* base,std::size_t offset,T value) {
    std::memcpy(static_cast<std::uint8_t*>(base)+offset,&value,sizeof(value));
}
void bytes(std::uint8_t* base,std::size_t offset,std::initializer_list<std::uint8_t> value) {
    std::memcpy(base+offset,value.begin(),value.size());
}
void jump(void* location,void* target) {
    const std::uint8_t code[]{0xb8,0,0,0,0,0xff,0xe0};
    std::memcpy(location,code,sizeof(code));
    put(location,1,reinterpret_cast<std::uintptr_t>(target));
}
void A2FO_CALL log_message(const char*,const char*) {}
void log_line(const std::string&) {}
void* A2FO_CALL get_armada(){return armada;}
void* A2FO_CALL get_fleet(){return fleet;}
bool A2FO_CALL reg_destroyed(const char*,const char* const* fields,std::uint32_t count,
                             A2FO_ObjectDestroyedHandler callback,void*) {
    check(count==2 && std::strcmp(fields[0],"wreckage")==0 &&
          std::strcmp(fields[1],"wreckageChance")==0,"wreckage fields captured while initially off");
    ++destroyed_regs; destroyed=callback; return true;
}
bool A2FO_CALL reg_weapon(const char*,std::uint32_t mask,A2FO_WeaponTriggerHandler callback,void*) {
    check(mask==A2FO_WEAPON_TRIGGER_EVENT_MASK_PRECHECK,"weapon precheck mask");
    ++weapon_regs;trigger=callback;return true;
}
bool A2FO_CALL reg_weapon_legacy(const char*,A2FO_WeaponTriggerHandler callback,void*) {
    ++weapon_regs;trigger=callback;return true;
}
std::uintptr_t __attribute__((fastcall)) native_lookup(void*,void*,const char* key) {
    return reinterpret_cast<std::uintptr_t>(std::strcmp(key,"GUI_CP_END_EXTRA")==0 ? ")" : "/");
}
std::uintptr_t __attribute__((fastcall)) native_formatter(void*,void*,void*) {
    rendered=reinterpret_cast<const char*>(lookup(reinterpret_cast<void*>(1),nullptr,"GUI_CP_END_EXTRA"));
    return 1;
}
float __attribute__((fastcall)) native_duration(void*,void*,std::uintptr_t) {return build_seconds;}
int __cdecl native_team(){return -1;}
bool A2FO_CALL inline_hook(void* target,void*,std::size_t n,const std::uint8_t* expected,A2FO_InlineHook* hook) {
    ++inline_calls; check(std::memcmp(target,expected,n)==0,"inline signature");
    if(partial && inline_calls==2) return false;
    *hook={target,reinterpret_cast<void*>(&native_formatter),n};return true;
}
bool A2FO_CALL patch(void* target,void* replacement,const std::uint8_t* expected,std::size_t n) {
    ++patch_calls;check(std::memcmp(target,expected,n)==0,"call signature");
    std::memcpy(&lookup,&replacement,sizeof(lookup));return true;
}
void begin(const std::string&){++begins;}
void finish(const std::string&,bool){++ends;}
void seed() {
    armada=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x410000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    fleet=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x330000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    check(armada && fleet,"allocate synthetic images");
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(armada);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(armada+0x80);
    nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.TimeDateStamp=0x3c4c76bd;nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage=0x403999;
    bytes(armada,0xe6ca0,{0x55,0x8b,0xec,0x6a,0xff,0x68,0x5b,0xed,0x69,0});
    bytes(armada,0xe72b0,{0x55,0x8b,0xec,0x6a,0xff,0x68,0x80,0xed,0x69,0});
    bytes(armada,0xce290,{0x55,0x8b,0xec,0x51,0x53,0x56,0x57,0x8b,0xf9});
    bytes(armada,0xd0060,{0x8b,0x0d,0xd4,0xb8,0x76,0,0xe9,0x45,0x78,0x08});
    bytes(armada,0x96340,{0x55,0x8b,0xec,0x8b,0x45,0x08,0x8b,0x04,0x85,0xb0,0x8d,0x73,0,0x5d,0xc3});
    bytes(armada,0x157940,{0x8b,0x41,0x30,0xc3});
    bytes(armada,0x146360,{0x8b,0x41,0x10,0xd9,0x80,0x2c,0x03,0,0,0xc3});
    bytes(armada,0xe711a,{0xe8,0x71,0xab,0xf9,0xff});
    bytes(armada,0xe772a,{0xe8,0x61,0xa5,0xf9,0xff});
    bytes(armada,0x271050,{0x8b,0x49,0x18,0x51,0xe8});
    bytes(fleet,0x120680,{0x53,0x51,0x89,0x14,0x24,0x33,0xd2,0x8b,0x40,0x0c,0x8b,0x0c,0x24,0x8b,0x5c,0x88});
    if(mismatch) armada[0xe6ca0]^=1;
    jump(armada+0x81c90,reinterpret_cast<void*>(&native_lookup));
}
void policy_file(const char* content) {
    HANDLE file=CreateFileA((root+"\\info.ini").c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(file!=INVALID_HANDLE_VALUE,"create fixture info.ini");DWORD done=0;
    check(WriteFile(file,content,std::strlen(content),&done,nullptr)!=0,"write policy");CloseHandle(file);
}
ModulePolicy selected_policy(std::initializer_list<const char*> names) {
    ModuleRules rules;rules.has_section=true;for(auto name:names)rules.active.push_back(name);
    // This fixture supplies only the original three modules' native stubs.
    // M3D compatibility is auto-enabled by real policy unless explicitly rejected.
    rules.rejected.push_back("A2FOM3D");
    return evaluate_module_rules({rules},discover_installed_modules(root));
}
void enable_all(bool enabled) {
    std::string error;
    auto policy=enabled?selected_policy({"A2FOWreckage","A2FONormalWeaponTech","A2FOBuildTooltips"}):selected_policy({});
    const bool applied=apply_builtin_module_policy({root},policy,error);
    check(applied,error.c_str());
}
void cleanup() {
    if(!fixture_owned) return;
    DeleteFileA((root+"\\info.ini").c_str());
    DeleteFileA((root+"\\modules\\A2FOWreckage.dll").c_str());
    RemoveDirectoryA((root+"\\modules").c_str());RemoveDirectoryA(root.c_str());
}
void run(const char* mode) {
    partial=std::strcmp(mode,"partial")==0;mismatch=std::strcmp(mode,"mismatch")==0;
    const bool dispatch_fail=std::strcmp(mode,"dispatch-fail")==0;
    const bool rejected=std::strcmp(mode,"rejected")==0;
    char temp[MAX_PATH]{};check(GetTempPathA(sizeof(temp),temp)!=0,"temporary root");
    root=std::string(temp)+"a2fo-builtin-"+std::to_string(GetCurrentProcessId());
    check(CreateDirectoryA(root.c_str(),nullptr)!=0,"private fixture directory");
    fixture_owned=true;
    check(discover_installed_modules(root).size()==static_cast<std::size_t>(BuiltinFeature::count),"built-ins present without standalone files");
    CreateDirectoryA((root+"\\modules").c_str(),nullptr);
    // An invalid leftover DLL must never be passed to LoadLibrary.
    HANDLE stale=CreateFileA((root+"\\modules\\A2FOWreckage.dll").c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(stale!=INVALID_HANDLE_VALUE,"stale duplicate fixture");CloseHandle(stale);
    const bool legacy=std::strcmp(mode,"legacy")==0;
    policy_file(legacy?"[mod]\nName=Fixture\n":rejected?"[modules]\nreject1=A2FOWreckage\nreject2=A2FONormalWeaponTech\nreject3=A2FOBuildTooltips\n":"[modules]\n");
    seed();
    A2FO_ModuleApi api{};api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
    api.capabilities=A2FO_CAP_OBJECT_DESTROYED_DISPATCH|A2FO_CAP_WEAPON_TRIGGER_EVENTS;
    api.log=&log_message;api.armada_module=&get_armada;api.fleetops_module=&get_fleet;
    api.install_inline_hook=&inline_hook;api.patch_call=&patch;
    api.register_object_destroyed_handler=&reg_destroyed;
    api.register_weapon_trigger_handler=&reg_weapon_legacy;
    api.register_weapon_trigger_handler_masked=&reg_weapon;
    LoadedModules loaded;
    ModuleRegistrationObserver observer{&begin,&finish};
    check(load_native_modules_from_roots({root},api,loaded,&log_line,observer),"load built-ins without standalone DLLs");
    check(std::all_of(loaded.begin(), loaded.end(), [](const LoadedModule& item) { return item.handle == nullptr; }),"no standalone duplicate loaded");
    const auto inventory=discover_installed_modules(root);
    check(inventory.size()==static_cast<std::size_t>(BuiltinFeature::count),"all built-ins discovered once");
    ModuleRules parent,child;parent.has_section=true;child.has_section=true;
    parent.required={"A2FOWreckage"};parent.rejected={"A2FONormalWeaponTech"};
    parent.active={"A2FOBuildTooltips"};child.active={"A2FONormalWeaponTech"};
    const auto inherited=evaluate_module_rules({parent,child},inventory);
    auto inherited_state = [&](const char* name) {
        for (const auto& entry : inherited.entries) if (entry.name == name) return entry.state;
        throw std::runtime_error("missing inherited policy entry");
    };
    check(inherited.valid && inherited_state("A2FOBuildTooltips")==ModulePolicyState::inactive &&
          inherited_state("A2FONormalWeaponTech")==ModulePolicyState::rejected &&
          inherited_state("A2FOWreckage")==ModulePolicyState::required,"inherited required/reject and child active precedence");
    if(rejected) {
        check(inline_calls==0 && patch_calls==0 && destroyed_regs==0 && weapon_regs==0,"reject suppresses preparation");
        finish_builtin_module_preparation(true,true,false,false,false);
        check(!builtin_module_available(BuiltinFeature::wreckage),"rejected not available");return;
    }
    check(destroyed_regs==1 && weapon_regs==1 && begins==static_cast<unsigned>(BuiltinFeature::count) && ends==begins,"one preparation transaction per built-in");
    finish_builtin_module_preparation(!dispatch_fail,!dispatch_fail,false,false,false);
    if(partial || mismatch || dispatch_fail) {
        check(!builtin_module_available(partial||mismatch?BuiltinFeature::build_tooltips:BuiltinFeature::wreckage),"unavailable status");
        std::string error;check(!apply_builtin_module_policy({root},selected_policy({"A2FOWreckage","A2FONormalWeaponTech","A2FOBuildTooltips"}),error) && !error.empty(),"unavailable activation explained");
        check(!builtin_module_enabled(BuiltinFeature::wreckage) && !builtin_module_enabled(BuiltinFeature::normal_weapon_tech),"failed request publishes no partial live state");
        if(partial) check(a2fo_build_tooltips_button_text_hook_cpp(nullptr,nullptr)==1 && rendered==")","partial hook remains native pass-through");
        return;
    }
    check(builtin_module_enabled(BuiltinFeature::wreckage)==legacy &&
          builtin_module_enabled(BuiltinFeature::normal_weapon_tech)==legacy &&
          builtin_module_enabled(BuiltinFeature::build_tooltips)==legacy,"managed-off versus legacy load-all defaults");
    check(inline_calls==2 && patch_calls==2,"hooks installed exactly once");
    jump(armada+0x81c90,reinterpret_cast<void*>(&native_lookup));
    jump(armada+0xce290,reinterpret_cast<void*>(&native_duration));
    jump(armada+0xd0060,reinterpret_cast<void*>(&native_team));
    put(armada+0x3379fc,0,reinterpret_cast<void*>(1));
    std::array<std::uint8_t,0x300> klass{},owner{},weapon{},tree{},mode_info{};
    std::uint32_t project_id=1;std::array<void*,1> items{{reinterpret_cast<void*>(1)}};
    std::array<void*,64> trees{};trees[0]=tree.data();
    put(klass.data(),0x68,build_seconds);put(klass.data(),0x208,&project_id);
    put(weapon.data(),4,klass.data());put(owner.data(),0xec,std::int32_t{0});
    put(tree.data(),0x0c,items.data());put(fleet+0x212f08,0,trees.data());
    bytes(armada,0x271050,{0xb8,0,0,0,0,0xc3});put(armada+0x271050,1,owner.data());
    bytes(fleet,0x120680,{0x31,0xc0,0xc3}); // synthetic Delphi evaluator: deny
    put(mode_info.data(),4,std::int32_t{1});put(mode_info.data(),0xc,klass.data());
    const A2FO_OdfFieldView fields[]{{{"wreckage",8},{"husk",4}},{{"wreckageChance",14},{"100",3}}};
    A2FO_ObjectDestroyedEvent event{};event.struct_size=sizeof(event);event.odf_fields=fields;event.odf_field_count=2;
    A2FO_WeaponTriggerEvent fire{sizeof(fire),A2FO_WEAPON_TRIGGER_PRECHECK,weapon.data(),nullptr};
    for(bool enabled:{false,true,false,true}) {
        enable_all(enabled);
        if(!enabled) {
            check(!destroyed(reinterpret_cast<const A2FO_ObjectDestroyedEvent*>(1),nullptr,nullptr),"disabled wreckage reads no event state");
            check(A2FONormalWeaponTech_AllowWeaponTrigger(reinterpret_cast<void*>(1),nullptr),"disabled weapon filter reads no engine object");
        }
        A2FO_ObjectReplacement replacement{sizeof(replacement),nullptr,0,0};
        check(destroyed(&event,&replacement,nullptr)==enabled,"wreckage on/off/on behaviour");
        if(enabled)check(replacement.odf && std::strcmp(replacement.odf,"husk")==0,"wreckage payload");
        check(trigger(&fire,nullptr)==!enabled,"weapon tree deny only when enabled");
        check(A2FONormalWeaponTech_AllowWeaponTrigger(weapon.data(),nullptr)==!enabled,"legacy filter follows live gate");
        check(a2fo_build_tooltips_button_text_hook_cpp(mode_info.data(),nullptr)==1,"text formatter result");
        check((rendered!=")")==enabled,"tooltip suffix on/off/on behaviour");
        check(a2fo_build_tooltips_button_verbose_hook_cpp(mode_info.data(),nullptr)==1,"verbose formatter result");
        check((rendered.find("20")!=std::string::npos)==enabled,"verbose suffix on/off/on");
    }
    std::string error;
    check(apply_builtin_module_policy({root+"\\other-mod"},selected_policy({}),error),"other mod saved independently");
    check(builtin_module_enabled(BuiltinFeature::wreckage),"other mod cannot change live feature");
    ModulePolicy invalid;invalid.valid=false;
    check(!apply_builtin_module_policy({root},invalid,error) && builtin_module_enabled(BuiltinFeature::wreckage),"invalid policy preserves live state");
    check(destroyed_regs==1 && weapon_regs==1 && inline_calls==2 && patch_calls==2,"toggle never re-registers/repatches");
}
}
int main(int argc,char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {run(argc>1?argv[1]:"success");cleanup();std::puts("builtin production modules: PASS");return 0;}
    catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());cleanup();return 1;}
}
