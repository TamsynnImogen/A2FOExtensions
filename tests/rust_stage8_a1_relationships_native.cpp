// Whole actual source. Unsafe engine gateway calls are satisfied by synthetic
// setup/side storage; unrelated assembly gateways trap in the runner.
#include A1_SOURCE
#include <stdexcept>
static std::uint8_t* fixture_module{};
static std::uint8_t fixture_sides[9][0x200]{};
static std::uint32_t fixture_slot_side[8]{};
static std::uint32_t fixture_slot_team[8]{};
static bool fixture_slot_active[8]{};
static bool fixture_mutate_team{};
static bool fixture_throw_set{};
static unsigned fixture_set_count{};
static unsigned fixture_lookup_count{};
static int fixture_setup{};
static int fixture_transport{};
static void A2FO_CALL fixture_log(const char*,const char* message){std::printf("log:%s\n",message);}
static A2FO_ModuleApi fixture_api(){A2FO_ModuleApi api{};api.log=&fixture_log;return api;}
static void* fixture_side_lookup(std::int32_t side){
    std::printf("lookup:%d\n",side);
    ++fixture_lookup_count;
    if(fixture_mutate_team&&side==0){fixture_slot_team[1]=7;std::puts("mutate:slot1-team=7");}
    return side>=0&&side<3?fixture_sides[side]:nullptr;
}
static void fixture_stub(std::uintptr_t rva,void* target){
    auto* out=fixture_module+rva;out[0]=0xb8;
    auto value=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(target));
    std::memcpy(out+1,&value,sizeof(value));out[5]=0xff;out[6]=0xe0;
}
extern "C" std::uintptr_t a2fo_a1_call_thiscall_0(void* function,void* self){
    if(function==at(g_armada,kTransportGetGameSetupRva)){
        std::printf("get-setup:%u\n",self==&fixture_transport);return reinterpret_cast<std::uintptr_t>(&fixture_setup);
    }
    std::abort();
}
extern "C" std::uintptr_t a2fo_a1_call_thiscall_1(void* function,void* self,std::uintptr_t slot){
    if(self!=&fixture_setup||slot>=8)std::abort();
    if(function==at(g_armada,kGameSetupIsActiveSlotRva)){
        std::printf("active:%u:%u\n",static_cast<unsigned>(slot),fixture_slot_active[slot]);
        return fixture_slot_active[slot];
    }
    if(function==at(g_armada,kGameSetupGetSideForSlotRva)){
        std::printf("side:%u:%u\n",static_cast<unsigned>(slot),fixture_slot_side[slot]);return fixture_slot_side[slot];
    }
    if(function==at(g_armada,kGameSetupGetAllianceTeamForSlotRva)){
        std::printf("team:%u:%u\n",static_cast<unsigned>(slot),fixture_slot_team[slot]);return fixture_slot_team[slot];
    }
    std::abort();
}
extern "C" std::uintptr_t a2fo_a1_call_thiscall_2(void* function,void* self,
    std::uintptr_t other,std::uintptr_t relationship){
    if(function!=at(g_armada,kSideSetRelationshipRva)||other>=9)std::abort();
    unsigned side=9;for(unsigned i=0;i<9;++i)if(self==fixture_sides[i])side=i;
    if(side>=9)std::abort();
    std::printf("set:%u:%u:%u\n",side,static_cast<unsigned>(other),static_cast<unsigned>(relationship));
    if(fixture_throw_set&&fixture_set_count++==3)throw std::runtime_error("fixture set failure");
    auto value=static_cast<std::int32_t>(relationship);
    std::memcpy(fixture_sides[side]+0x170+other*sizeof(value),&value,sizeof(value));
    return 0;
}
static void fixture_reset(){
    fixture_slot_side[0]=1;fixture_slot_side[1]=2;
    fixture_slot_team[0]=4;fixture_slot_team[1]=4;
    fixture_slot_active[0]=true;fixture_slot_active[1]=true;
    for(auto& side:fixture_sides)for(unsigned other=0;other<9;++other){
        std::int32_t value=-1;std::memcpy(side+0x170+other*sizeof(value),&value,sizeof(value));
    }
    fixture_mutate_team=false;fixture_throw_set=false;fixture_set_count=0;fixture_lookup_count=0;
    InterlockedExchange(&g_a1_relationship_restore_count,0);
    InterlockedExchange(&g_a1_selected_map_active,1);
    *at<void*>(g_armada,kTransportCurrentRva)=&fixture_transport;
}
static void fixture_case(const char* name,unsigned mode){
    std::printf("case:%s\n",name);fixture_reset();
    if(mode==1)fixture_slot_team[1]=5;
    if(mode==2)fixture_slot_side[1]=9;
    if(mode==3)fixture_slot_active[0]=fixture_slot_active[1]=false;
    if(mode==4)fixture_mutate_team=true;
    if(mode==5)fixture_throw_set=true;
    if(mode==6)InterlockedExchange(&g_a1_relationship_restore_count,1);
    if(mode==7)*at<void*>(g_armada,kTransportCurrentRva)=nullptr;
    if(mode==8)InterlockedExchange(&g_a1_selected_map_active,0);
    if(mode==9)g_armada=nullptr;
    if(mode<=6){
        bool result=restore_a1_instant_action_relationships(&fixture_setup);
        std::printf("result:%u:%ld:%u\n",result,
            InterlockedCompareExchange(&g_a1_relationship_restore_count,0,0),fixture_lookup_count);
    }else{
        restore_a1_relationships_before_elimination();
        std::printf("before:%ld:%u\n",InterlockedCompareExchange(&g_a1_relationship_restore_count,0,0),fixture_lookup_count);
    }
    if(mode==9)g_armada=fixture_module;
    if(mode!=3&&mode!=6&&mode!=8&&mode!=9){
        std::int32_t side12=0;std::memcpy(&side12,fixture_sides[1]+0x170+2*sizeof(side12),sizeof(side12));
        std::printf("side12:%d\n",side12);
    }
}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    fixture_module=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x400000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    if(!fixture_module)std::abort();
    g_armada=fixture_module;
    fixture_stub(kGameSetupIsActiveSlotRva,reinterpret_cast<void*>(&fixture_side_lookup));
    fixture_stub(kGameSetupGetSideForSlotRva,reinterpret_cast<void*>(&fixture_side_lookup));
    fixture_stub(kGameSetupGetAllianceTeamForSlotRva,reinterpret_cast<void*>(&fixture_side_lookup));
    fixture_stub(kSideLookupRva,reinterpret_cast<void*>(&fixture_side_lookup));
    fixture_stub(kSideSetRelationshipRva,reinterpret_cast<void*>(&fixture_side_lookup));
    fixture_stub(kTransportGetGameSetupRva,reinterpret_cast<void*>(&fixture_side_lookup));
    auto api=fixture_api();g_api=&api;
    fixture_case("allied",0);fixture_case("enemy",1);fixture_case("invalid-side",2);
    fixture_case("no-active",3);fixture_case("mutation",4);fixture_case("throw-set",5);
    fixture_case("already-restored",6);fixture_case("missing-transport",7);
    fixture_case("inactive-map",8);fixture_case("missing-armada",9);
    fixture_case("before-active",10);
    std::puts("A1 relationships native fixture: PASS");
    VirtualFree(fixture_module,0,MEM_RELEASE);
}
