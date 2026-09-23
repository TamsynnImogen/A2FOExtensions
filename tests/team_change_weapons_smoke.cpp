// Execute checked native SwapTeam/SwapRaceAndTeam, Weapon::Trigger, and
// Weapon::SimulateAll code from a private EXE fixture without its entry point.
// No window, input, or running game is involved.
#include "../modules/A2FOTeamChangeWeapons/module.cpp"
#include "../core/hook.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>

namespace {
std::uint8_t* fixture;
unsigned installs, fail_install;
A2FO_WeaponClassLoadedHandler loaded;
A2FO_CraftEventHandler lifecycle;
std::array<std::uint8_t,0x300> craft{};
std::array<std::uint8_t,0x120> target_object{};
std::array<std::uint8_t,0x30> carrier{};
std::array<std::uint8_t,0x300> enabled_class{}, disabled_class{};
std::array<std::uint8_t,0x80> weapon1{}, weapon2{};
std::array<void*,2> weapons{weapon1.data(),weapon2.data()};
std::array<void*,64> craft_vtable{}, weapon_vtable{};
std::array<void*,3> head{}, node1{}, node2{};
bool entity_present = true, allow_trigger = true;
unsigned activations, trigger_requests, weapon_passes;
std::int32_t fired_team = -1;
std::uintptr_t expected_target = 0;
unsigned detonations = 0;
a2fo::InlineHook trigger_filter;

template<class T> void put(void* pointer,std::size_t offset,T value) {
    std::memcpy(static_cast<std::uint8_t*>(pointer)+offset,&value,sizeof(value));
}
void A2FO_CALL test_log(const char*,const char* message) { std::puts(message); }
void* A2FO_CALL armada() { return fixture; }
bool A2FO_CALL register_class(const char*,const char* const* fields,std::uint32_t count,
                              A2FO_WeaponClassLoadedHandler handler,void*) {
    assert(count==1 && std::strcmp(fields[0],kCommand)==0); loaded=handler; return true;
}
bool A2FO_CALL register_lifecycle(const char*,std::uint32_t mask,A2FO_CraftEventHandler handler,void*) {
    assert(mask==(A2FO_CRAFT_EVENT_MASK_CLEANUP | A2FO_CRAFT_EVENT_MASK_POST_LOAD));
    lifecycle=handler; return true;
}
bool A2FO_CALL install(void* target,void* replacement,std::size_t length,
                       const std::uint8_t* expected,A2FO_InlineHook* output) {
    assert(std::memcmp(target,expected,length)==0);
    if (++installs==fail_install) return false;
    a2fo::InlineHook hook;
    if (!a2fo::install_inline_hook(target,replacement,length,expected,hook)) return false;
    *output={hook.target,hook.gateway,hook.length}; return true;
}
void undo(A2FO_InlineHook& hook) {
    if (!hook.gateway) return;
    std::memcpy(hook.target,hook.gateway,hook.length);
    VirtualFree(hook.gateway,0,MEM_RELEASE); hook={};
}
void* __cdecl entity_get(std::uint32_t handle) {
    if (handle==456) return target_object.data();
    return entity_present && handle==read<std::uint32_t>(craft.data(),kHandle) ? craft.data() : nullptr;
}
void __fastcall clear_team(void* self,void*) { put(self,kTeam,std::int32_t(-1)); }
void __fastcall set_team(void* self,void*,std::int32_t team) { put(self,kTeam,team); }
void __fastcall command(void*,void*,std::uintptr_t,std::uintptr_t,std::uintptr_t,std::uintptr_t) {}
int __fastcall can_simulate(void*,void*) { return 0; }
void __fastcall simulate_weapon(void* weapon,void*,float elapsed) {
    assert(elapsed>=0); ++weapon_passes;
    if (read<std::uint8_t>(weapon,0x2c)) {
        ++activations; fired_team=read<std::int32_t>(craft.data(),kTeam);
    }
    put(weapon,0x2c,std::uint8_t(0));
}
void __fastcall filtered_trigger(void* weapon,void*,std::uintptr_t target) {
    assert(target==expected_target); ++trigger_requests;
    if (allow_trigger) a2fo_team_change_thiscall_1(trigger_filter.gateway,weapon,target);
}
void __cdecl message(std::uint32_t,void*,std::uint32_t) {}
void __fastcall detonate(void* self,void*,std::uint32_t) {
    assert(self==craft.data()); ++detonations;
}
void jump(std::uintptr_t rva,void* function) {
    const std::uint8_t* original=fixture+rva;
    std::array<std::uint8_t,5> bytes{}; std::memcpy(bytes.data(),original,5);
    assert(a2fo::patch_jump(fixture+rva,function,bytes.data(),5));
}
void map_fixture(const char* path) {
    std::ifstream input(path,std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
    assert(bytes.size()>sizeof(IMAGE_DOS_HEADER));
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(bytes.data()+dos->e_lfanew);
    fixture=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,nt->OptionalHeader.SizeOfImage,
        MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE)); assert(fixture);
    std::memcpy(fixture,bytes.data(),nt->OptionalHeader.SizeOfHeaders);
    const auto* sections=IMAGE_FIRST_SECTION(nt);
    for (unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        const auto& s=sections[i]; assert(s.PointerToRawData+s.SizeOfRawData<=bytes.size());
        std::memcpy(fixture+s.VirtualAddress,bytes.data()+s.PointerToRawData,s.SizeOfRawData);
    }
    const auto delta=reinterpret_cast<std::uintptr_t>(fixture)-nt->OptionalHeader.ImageBase;
    const auto directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    for (unsigned offset=0;offset<directory.Size;) {
        const auto* block=reinterpret_cast<const IMAGE_BASE_RELOCATION*>(fixture+directory.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block));
        const auto* entries=reinterpret_cast<const std::uint16_t*>(block+1);
        for (unsigned i=0;i<(block->SizeOfBlock-sizeof(*block))/2;++i) {
            if ((entries[i]>>12)==IMAGE_REL_BASED_HIGHLOW) {
                auto* address=fixture+block->VirtualAddress+(entries[i]&0xfff);
                std::uint32_t value; std::memcpy(&value,address,4); put(address,0,value+delta);
            } else assert((entries[i]>>12)==IMAGE_REL_BASED_ABSOLUTE);
        }
        offset+=block->SizeOfBlock;
    }
}
void configure(void* cls,const char* value) {
    A2FO_OdfFieldView field{{"activateOnTeamChange",20},{value,static_cast<std::uint32_t>(std::strlen(value))}};
    A2FO_WeaponClassLoadedEvent event{sizeof(event),cls,nullptr,nullptr,&field,1};
    loaded(&event,nullptr);
}
void swap(std::int32_t team) {
    a2fo_team_change_thiscall_1(fixture+kSwapTeam,craft.data(),team);
}
void step(float elapsed=0.1f) {
    reinterpret_cast<void (__cdecl*)(float)>(fixture+kSimulateAll)(elapsed);
}
void notify_lifecycle(std::uint32_t kind) {
    A2FO_CraftEvent event{sizeof(event),kind,craft.data(),0}; lifecycle(&event,nullptr);
}
} // namespace

int main(int argc,char** argv) {
    assert(argc==2); map_fixture(argv[1]);
    A2FO_ModuleApi api{}; api.struct_size=sizeof(api); api.api_version=A2FO_MODULE_API_VERSION;
    api.capabilities=A2FO_CAP_WEAPON_CLASS_LOADED|A2FO_CAP_CRAFT_EVENTS;
    api.log=test_log; api.armada_module=armada; api.install_inline_hook=install;
    api.register_weapon_class_loaded_handler=register_class;
    api.register_craft_event_handler_masked=register_lifecycle;
    assert(!A2FO_ModuleInit(nullptr));
    for (auto rva:{kSwapTeam,kSwapRaceAndTeam,kSimulateAll,kGetCraft,kTrigger,kGetTarget}) {
        fixture[rva]^=1; assert(!A2FO_ModuleInit(&api) && installs==0); fixture[rva]^=1;
    }
    // Failed later hooks leave earlier gateways callable and the module inert.
    for (unsigned failure=1;failure<=3;++failure) {
        fail_install=failure; installs=0;
        assert(A2FO_ModuleInit(&api) && !g_ready && installs==failure);
        undo(g_swap_race); undo(g_swap); undo(g_simulate);
    }
    fail_install=0; installs=0;
    assert(A2FO_ModuleInit(&api) && g_ready && installs==3);
    assert(enabled_value("1") && enabled_value(" \"1\" ") && enabled_value("TRUE"));
    for (auto invalid:{"","0","false","-1","2","1bad"}) assert(!enabled_value(invalid));
    configure(enabled_class.data(),"1"); configure(disabled_class.data(),"0");
    assert(g_classes.size()==1);

    // Native helpers keep their real code; only external engine dependencies
    // and per-weapon effects are stubbed. The mapped EXE is fully relocated.
    jump(0xcfff0,reinterpret_cast<void*>(&entity_get));
    jump(0xd1a40,reinterpret_cast<void*>(&command));
    jump(0xd4f20,reinterpret_cast<void*>(&can_simulate));
    assert(a2fo::install_inline_hook(fixture+kTrigger,reinterpret_cast<void*>(&filtered_trigger),
        sizeof(kTriggerBytes),kTriggerBytes,trigger_filter));
    craft_vtable[0xdc/4]=reinterpret_cast<void*>(&set_team);
    craft_vtable[0xe0/4]=reinterpret_cast<void*>(&clear_team);
    put(craft.data(),0,craft_vtable.data()); put(craft.data(),0x14,std::uint32_t(0xc));
    put(craft.data(),kHandle,std::uint32_t(123)); put(craft.data(),kTeam,std::int32_t(1));
    put(target_object.data(),kHandle,std::uint32_t(456)); put(target_object.data(),0x14,std::uint32_t(4));
    put(craft.data(),kCarrier,carrier.data());
    put(carrier.data(),kVectorBegin,weapons.data()); put(carrier.data(),kVectorEnd,weapons.data()+2);
    weapon_vtable[0x10/4]=reinterpret_cast<void*>(&simulate_weapon);
    for (void* weapon:weapons) { put(weapon,0,weapon_vtable.data()); put(weapon,kWeaponOwner,std::uint32_t(123)); }
    put(weapon1.data(),kWeaponClass,enabled_class.data());
    put(weapon2.data(),kWeaponClass,disabled_class.data());
    head[0]=node1.data(); node1[0]=node2.data(); node2[0]=head.data();
    node1[2]=weapon1.data(); node2[2]=weapon2.data(); put(fixture,kWeaponList,head.data());

    step(); assert(activations==0 && trigger_requests==0 && weapon_passes==2); // spawn
    swap(1); step(); assert(activations==0); // same team
    swap(2); assert(activations==0 && g_pending.size()==1); // never run effects in SetTeam
    step(); assert(activations==1 && fired_team==2 && trigger_requests==1);
    step(); assert(activations==1 && trigger_requests==1); // one request
    swap(0); step(); assert(activations==2 && fired_team==0); // derelict
    swap(3); step(); assert(activations==3 && fired_team==3); // recrew/capture
    a2fo_team_change_thiscall_2(fixture+kSwapRaceAndTeam,craft.data(),0x12345678,4);
    step(); assert(activations==4 && fired_team==4 && read<std::uint32_t>(craft.data(),0xfc)==0x12345678);
    a2fo_team_change_thiscall_2(fixture+kSwapRaceAndTeam,craft.data(),0x87654321,4);
    step(); assert(activations==4); // race-only change
    swap(5); swap(6); assert(g_pending.size()==1); step(); assert(activations==5 && fired_team==6);
    swap(7); step(0); assert(g_pending.size()==1 && activations==5); step(); assert(activations==6);
    swap(8); allow_trigger=false; step(); assert(activations==6); allow_trigger=true;
    step(); assert(activations==6); // rejected requests do not retry
    swap(9); notify_lifecycle(A2FO_CRAFT_EVENT_POST_LOAD); step(); assert(activations==6);
    swap(10); notify_lifecycle(A2FO_CRAFT_EVENT_CLEANUP); step(); assert(activations==6);
    swap(11); entity_present=false; step(); assert(activations==6); entity_present=true;
    swap(12); put(craft.data(),kHandle,std::uint32_t(124)); step(); assert(activations==6);
    put(craft.data(),kHandle,std::uint32_t(123)); // reused pointer/new handle must never trigger old work
    swap(13); put(craft.data(),0x113,std::uint8_t(1)); step(); assert(activations==6);
    put(craft.data(),0x113,std::uint8_t(0));
    swap(14); configure(enabled_class.data(),"0"); step(); assert(activations==6);
    configure(enabled_class.data(),"1"); configure(disabled_class.data(),"1");
    swap(15); step(); assert(activations==8); // native slot order, both configured weapons
    configure(enabled_class.data(),"garbage"); configure(disabled_class.data(),"0");
    swap(16); step(); assert(activations==8 && g_pending.empty());
    configure(enabled_class.data(),"1");
    put(craft.data(),0x14,std::uint32_t(4)); swap(17); step(); assert(activations==8);
    put(craft.data(),0x14,std::uint32_t(0xc)); // non-Craft objects are ignored
    put(carrier.data(),kVectorEnd,reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(weapons.data())+3));
    swap(16); step(); assert(activations==8); // invalid native vector is never walked
    put(carrier.data(),kVectorEnd,weapons.data()+2);
    swap(17); put(craft.data(),kTeam,std::int32_t(18)); step(); assert(activations==8); // stale owner
    swap(19); notify_lifecycle(A2FO_CRAFT_EVENT_POST_LOAD); step(); assert(activations==8);
    // Ordinary/targeted weapons keep their existing native target.
    put(enabled_class.data(),0x1de,std::uint8_t(1)); put(weapon1.data(),0x38,std::uint32_t(456));
    expected_target=reinterpret_cast<std::uintptr_t>(target_object.data());
    swap(20); step(); assert(activations==9 && read<std::uint32_t>(weapon1.data(),0x38)==456);
    put(enabled_class.data(),0x1de,std::uint8_t(0)); expected_target=0;
    swap(21); step(); assert(activations==10); // self weapons never inherit that target
    const auto requests_before_toggle=trigger_requests;
    put(weapon1.data(),0x2d,std::uint8_t(1)); swap(22); step();
    assert(activations==10 && trigger_requests==requests_before_toggle);
    put(weapon1.data(),0x2d,std::uint8_t(0));

    // Execute the REAL SelfDestruct::Simulate: ownership starts the authored
    // countdown, another capture cannot toggle it off, and expiry detonates.
    const std::uint8_t self_destruct_prefix[]{0x55,0x8b,0xec,0x53,0x56,0x8b,0xf1};
    assert(std::memcmp(fixture+0x26cf80,self_destruct_prefix,sizeof(self_destruct_prefix))==0);
    jump(0x5d8c0,reinterpret_cast<void*>(&message));
    craft_vtable[0xf8/4]=reinterpret_cast<void*>(&detonate);
    weapon_vtable[0x10/4]=fixture+0x26cf80;
    put(enabled_class.data(),0x250,1.0f); put(enabled_class.data(),0x254,2.0f);
    swap(23); step();
    assert(read<std::uint8_t>(weapon1.data(),0x2d)==1 && detonations==0);
    assert(std::fabs(read<float>(weapon1.data(),0x3c)-0.9f)<0.0001f);
    swap(24); step();
    assert(read<std::uint8_t>(weapon1.data(),0x2d)==1 && detonations==0);
    assert(std::fabs(read<float>(weapon1.data(),0x3c)-0.8f)<0.0001f);
    step(1.0f); assert(detonations==1 && read<std::uint8_t>(weapon1.data(),0x2d)==0);
    assert(read<float>(craft.data(),0x204)==2.0f); // native authored damage multiplier
    step(); assert(detonations==1);
    weapon_vtable[0x10/4]=reinterpret_cast<void*>(&simulate_weapon);
    swap(25); A2FO_ModuleShutdown(); step(); assert(activations==10);
    swap(26); step(); assert(activations==10 && read<std::int32_t>(craft.data(),kTeam)==26);
    std::puts("team-change weapon native integration passed: capture, race swap, derelict, filters, lifecycle, targeting, self-destruct countdown/detonation, relocation, partial failure");
}
