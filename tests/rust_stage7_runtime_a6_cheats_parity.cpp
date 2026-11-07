#ifdef TEST_BUILTIN
#define A2FO_ModuleInit A2FOBuiltinCheats_Init
#endif
#include MODULE_SOURCE
#include <array>
#include <cstdio>
#include <fstream>
#include <new>
#include <stdexcept>
#pragma GCC diagnostic ignored "-Wattributes"
namespace {
void check(bool value,const char* label){if(!value)throw std::runtime_error(label);}
std::uint8_t* armada_image=nullptr,*fleet_image=nullptr;
int hook_calls=0,hook_fail=0,original_calls=0,move_calls=0,shield_calls=0,elim_calls=0;
bool throw_root_count=false;
std::string actions;
std::array<std::uint8_t,0x700> selection{},craft101{},craft102{},craft103{},team{};
struct Registry {std::uint32_t count=0;CheatRegistration entries[8]{};} registry;
void put(void* address,void* value){std::memcpy(address,&value,sizeof(value));}
template<class T>void field(void* p,std::size_t offset,T value){
    std::memcpy(static_cast<std::uint8_t*>(p)+offset,&value,sizeof(value));
}
void jump(void* where,void* target){std::uint8_t bytes[]{0x68,0,0,0,0,0xc3};
    std::memcpy(where,bytes,sizeof(bytes));field(where,1,target);}
void A2FO_CALL mock_log(const char*,const char* message){
    const std::string line(message);
    const auto marker=line.find(", Dilithium ");
    if(line.rfind("showmethemoney invoked: Team=",0)==0&&marker!=std::string::npos)
        std::printf("log:showmethemoney invoked: Team=<team>%s\n",line.c_str()+marker);
    else std::printf("log:%s\n",message);
}
void* A2FO_CALL mock_armada(){return armada_image;}
void* A2FO_CALL mock_fleet(){return fleet_image;}
std::uint32_t A2FO_CALL root_count(){if(throw_root_count)throw std::bad_alloc();return 3;}
const char* A2FO_CALL root_at(std::uint32_t index){
    return index==0?"build\\rust-migration-stage7\\runtime-a-work\\cheats-root1":
        index==1?"build\\rust-migration-stage7\\runtime-a-work\\cheats-root2":nullptr;
}
void __cdecl mock_original(){++original_calls;}
bool A2FO_CALL mock_hook(void*,void*,std::size_t,const std::uint8_t*,A2FO_InlineHook* out){
    ++hook_calls;if(hook_calls==hook_fail)return false;
    out->gateway=reinterpret_cast<void*>(&mock_original);return true;
}
void __attribute__((regparm(3))) mock_register(
    const char* name,std::uint32_t multiplayer,CheatHandler handler){
    check(registry.count<8,"registry capacity");
    registry.entries[registry.count++]={name,static_cast<std::uint8_t>(multiplayer),{},handler};
    actions+='R';
}
void* __cdecl mock_entity(std::int32_t handle){
    if(handle==101)return craft101.data();
    if(handle==102)return craft102.data();
    return handle==103?craft103.data():nullptr;
}
void* __cdecl mock_cast(void* object,std::int32_t,void*,void*,std::int32_t){return object;}
__attribute__((naked)) void* mock_transform(){__asm__ __volatile__("lea 512(%ecx),%eax; ret");}
void __attribute__((thiscall)) mock_move(void* self,int command,const Vector3* destination,int value){
    check(command==4&&value==0,"move ABI");
    check(destination->x==220.0f&&destination->y==30.0f&&destination->z==40.0f,"move vector");
    actions+=self==craft102.data()?'B':self==craft103.data()?'C':'A';++move_calls;
}
void __attribute__((thiscall)) mock_shield(void* self,float seconds){
    check(seconds==10000.0f,"shield ABI");
    actions+=self==craft102.data()?'b':self==craft103.data()?'c':'a';++shield_calls;
    if(self==craft102.data())field(selection.data(),kSelectionHandlesOffset,std::int32_t{103});
}
void __attribute__((thiscall)) mock_elim(void*,std::int32_t player){
    check(player==2,"elimination team");++elim_calls;actions+='E';
}
void* __cdecl mock_player(){return reinterpret_cast<void*>(0x1234);}
void* __cdecl mock_team(void*){return team.data();}
void __attribute__((regparm(1),stdcall)) mock_dil(void* object,float amount){
    check(object==team.data(),"grant team");
    float old=0;std::memcpy(&old,team.data()+kTeamDilithiumOffset,sizeof(old));
    field(team.data(),kTeamDilithiumOffset,old+amount);actions+='D';
}
void __attribute__((regparm(1),stdcall)) mock_tri(void*,float){actions+='T';}
void __attribute__((regparm(1),stdcall)) mock_metal(void*,float){actions+='M';}
void __attribute__((regparm(1),stdcall)) mock_supplies(void*,float){actions+='S';}
void __attribute__((thiscall)) mock_crew_capacity(void*,float){actions+='C';}
void __attribute__((regparm(1),stdcall)) mock_crew(void*,float){actions+='W';}
void signatures(bool repurposed){
    auto copy=[](void* address,const auto& bytes){std::memcpy(address,bytes.data(),bytes.size());};
    copy(at(g_fleet_ops,kChatRegisterCheatRva),kExpectedChatRegisterCheat);
    copy(at(g_armada,kAddCrewCapacityRva),kExpectedAddCrewCapacity);
    copy(at(g_armada,kDisableShieldGeneratorRva),kExpectedDisableShieldGenerator);
    copy(at(g_armada,kEntityGetTransformRva),kExpectedEntityGetTransform);
    copy(at(g_armada,kQueueCommandVectorRva),kExpectedQueueCommandVector);
    copy(at(g_armada,kEliminateTeamRva),kExpectedEliminateTeam);
    for(auto rva:{kAddDilithiumRva,kAddTritaniumRva,kAddMetalRva,kAddSuppliesRva,kAddCrewRva})
        copy(at(g_fleet_ops,rva),kExpectedResourceMutator);
    copy(at(g_fleet_ops,kShowMeTheMoneyRva),
        repurposed?repurposed_money_signature():
        std::array<std::uint8_t,8>{});
    if(!repurposed)copy(at(g_fleet_ops,kShowMeTheMoneyRva),show_me_the_money_signature());
    copy(at(g_fleet_ops,kChatHookInitRva),chat_hook_init_signature());
}
void link(std::uintptr_t rva,void* pointer){auto* cell=new void*(pointer);put(at(g_fleet_ops,rva),cell);}
void fixture(){
    armada_image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x310000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    fleet_image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x260000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    check(armada_image&&fleet_image,"image allocation");
    g_armada=reinterpret_cast<HMODULE>(armada_image);
    g_fleet_ops=reinterpret_cast<HMODULE>(fleet_image);
    CreateDirectoryA(root_at(0),nullptr);CreateDirectoryA(root_at(1),nullptr);
    {std::ofstream file(std::string(root_at(0))+"\\RTS_CFG.h");
        file<<"// first layer\nSHOWMETHEMONEY_DILITHIUM = 12000f;\n"
              "SHOWMETHEMONEY_METAL = -1; /* rejected */\n"
              "SHOWMETHEMONEY_CREW = 25;\nSHOWMETHEMONEY_METAL = 3";
        file.put('\0');file<<"ignored;\n";}
    {std::ofstream file(std::string(root_at(1))+"\\RTS_CFG.h");
        file<<"SHOWMETHEMONEY_DILITHIUM = 3.5e4F;\n"
              "SHOWMETHEMONEY_TRITANIUM = 2;\n"
              "SHOWMETHEMONEY_SUPPLIES = 7;\n";}
    A2FO_ModuleApi supplied{};supplied.struct_size=sizeof(supplied);
    supplied.api_version=A2FO_MODULE_API_VERSION;supplied.log=mock_log;
    supplied.armada_module=mock_armada;supplied.fleetops_module=mock_fleet;
    supplied.install_inline_hook=mock_hook;supplied.extension_root_count=root_count;
    supplied.extension_root=root_at;
    check(!A2FO_ModuleInit(nullptr),"null init");
    throw_root_count=true;bool contained=false;
    try{A2FO_ModuleInit(&supplied);}catch(const std::bad_alloc&){contained=true;}
    throw_root_count=false;check(contained&&hook_calls==0,"root allocation exception boundary");
    signatures(true);hook_calls=0;hook_fail=0;
    check(A2FO_ModuleInit(&supplied)&&hook_calls==1,"repurposed init");
    check(g_grant_amounts.dilithium==35000.0f&&g_grant_amounts.metal==3.0f&&
          g_grant_amounts.crew==25.0f,"layered grants");
    signatures(false);hook_calls=0;hook_fail=2;
    check(A2FO_ModuleInit(&supplied)&&hook_calls==2,"partial money hook retained");
    signatures(true);hook_calls=0;hook_fail=1;
    check(!A2FO_ModuleInit(&supplied)&&hook_calls==1,"chat hook failure");
    signatures(true);hook_calls=0;hook_fail=0;
    check(A2FO_ModuleInit(&supplied),"final init");
    jump(at(g_fleet_ops,kChatRegisterCheatRva),reinterpret_cast<void*>(&mock_register));
    registry.count=0;actions.clear();
    chat_hook_init_hook();
    check(original_calls==1&&registry.count==5&&actions=="RRRRR",
        "missing registry attempts every command");
    registry.count=1;registry.entries[0]={kMoneyCheat,1,{},nullptr};
    put(at(g_fleet_ops,kCheatRegistryRva),registry.entries);
    chat_hook_init_hook();
    check(original_calls==2&&registry.count==5&&registry.entries[0].multiplayer_allowed==0,
        "registration existing and missing");
    check(registry.entries[0].handler==show_me_the_money_hook&&
          registry.entries[4].handler==eliminate_selected_team_hook,"registration handlers");
    actions.clear();
    field(selection.data(),kSelectionCountOffset,std::int32_t{2});
    field(selection.data(),kSelectionHandlesOffset,std::int32_t{101});
    field(selection.data(),kSelectionHandlesOffset+4,std::int32_t{102});
    link(kSelectionDispatchLinkRva,selection.data());
    link(kEntityGetDispatchLinkRva,reinterpret_cast<void*>(&mock_entity));
    link(kDynamicCastDispatchLinkRva,reinterpret_cast<void*>(&mock_cast));
    jump(at(g_armada,kEntityGetTransformRva),reinterpret_cast<void*>(&mock_transform));
    jump(at(g_armada,kQueueCommandVectorRva),reinterpret_cast<void*>(&mock_move));
    jump(at(g_armada,kDisableShieldGeneratorRva),reinterpret_cast<void*>(&mock_shield));
    jump(at(g_armada,kEliminateTeamRva),reinterpret_cast<void*>(&mock_elim));
    for(auto* craft:{craft101.data(),craft102.data(),craft103.data()}){
        float matrix[12]{};matrix[6]=1;matrix[9]=20;matrix[10]=30;matrix[11]=40;
        std::memcpy(craft+512,matrix,sizeof(matrix));}
    check(move_selected_hook()&&move_calls==2,"move selected");
    check(disable_selected_shields_hook()&&shield_calls==2,"shield selected with mutation");
    field(craft103.data(),kGameObjectTeamOffset,std::int32_t{-1});
    field(craft102.data(),kGameObjectTeamOffset,std::int32_t{2});
    field(selection.data(),kSelectionHandlesOffset+4,std::int32_t{102});
    link(kGameTypeDispatchLinkRva,reinterpret_cast<void*>(0x102030));
    check(eliminate_selected_team_hook()&&elim_calls==1,"eliminate first valid reverse");
    link(kCurrentPlayerDispatchLinkRva,reinterpret_cast<void*>(&mock_player));
    link(kTeamLookupDispatchLinkRva,reinterpret_cast<void*>(&mock_team));
    jump(at(g_fleet_ops,kAddDilithiumRva),reinterpret_cast<void*>(&mock_dil));
    jump(at(g_fleet_ops,kAddTritaniumRva),reinterpret_cast<void*>(&mock_tri));
    jump(at(g_fleet_ops,kAddMetalRva),reinterpret_cast<void*>(&mock_metal));
    jump(at(g_fleet_ops,kAddSuppliesRva),reinterpret_cast<void*>(&mock_supplies));
    jump(at(g_fleet_ops,kAddCrewRva),reinterpret_cast<void*>(&mock_crew));
    jump(at(g_armada,kAddCrewCapacityRva),reinterpret_cast<void*>(&mock_crew_capacity));
    field(team.data(),kTeamDilithiumOffset,100.0f);
    check(show_me_the_money_hook(),"money grant");
    check(actions=="BAbcEDTMSCW","action order");
    std::printf("counts:%u:%d:%d:%d:%s\n",registry.count,move_calls,shield_calls,elim_calls,actions.c_str());
    VirtualFree(armada_image,0,MEM_RELEASE);VirtualFree(fleet_image,0,MEM_RELEASE);
}
}
int main(){try{fixture();return 0;}catch(const std::exception& error){
    std::fprintf(stderr,"FAIL:%s\n",error.what());return 1;}}
