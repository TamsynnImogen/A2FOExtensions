#include MODULE_SOURCE
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
#ifdef TEST_CURRENT
void test_emit(void* opaque,A2foRsXpBytes value) noexcept {
    auto& context=*static_cast<XpContext*>(opaque);
    if(context.output)*context.output=xp_text(value);
}
bool prepare_chain(const std::string& first){
    XpContext context{};
    const bool result=a2fo_rs_runtime_xp_prepare_chain(xp_bytes(first),xp_chain_step,&context)!=0;
    if(context.error)std::rethrow_exception(context.error);
    return result;
}
std::string family(const std::string& name){
    std::string result;XpContext context{};context.output=&result;
    a2fo_rs_runtime_xp_family(xp_bytes(name),xp_chain_step,test_emit,&context);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}
bool configure(unsigned player){
    XpContext context{};
    const bool result=a2fo_rs_runtime_xp_configure(player,xp_chain_step,&context)!=0;
    if(context.error)std::rethrow_exception(context.error);
    return result;
}
void reconcile(unsigned player){
    XpContext context{};
    a2fo_rs_runtime_xp_reconcile(player,syncing,promoting,xp_chain_step,&context);
    if(context.error)std::rethrow_exception(context.error);
}
void observe(void* craft){
    XpContext context{};
    a2fo_rs_runtime_xp_observe(reinterpret_cast<std::uintptr_t>(craft),xp_chain_step,&context);
    if(context.error)std::rethrow_exception(context.error);
}
#endif
int logs=0,finds=0;
int fail_registration=-1;
std::string registrations;
void check(bool,const char*);
std::vector<std::uint8_t> saved;
__attribute__((naked)) unsigned fake_team(){__asm__ __volatile__("movl $1, %eax; ret $4");}
void __cdecl fake_clean(){}
bool __cdecl fake_out(void*,const void* bytes,unsigned count,const char* tag){
    check(std::strcmp(tag,"a2fo_collectiveXP")==0,"save tag");
    saved.assign(static_cast<const std::uint8_t*>(bytes),
        static_cast<const std::uint8_t*>(bytes)+count);return true;
}
bool __cdecl fake_in(void*,void* bytes,unsigned count){
    if(saved.size()!=count)return false;
    std::memcpy(bytes,saved.data(),count);return true;
}
void* A2FO_CALL image_module(){return armada;}
bool A2FO_CALL register_race(const char*,const char* const*,std::uint32_t,
                            A2FO_RaceLoadedHandler,void*){
    registrations+='R';return fail_registration!=0;
}
bool A2FO_CALL register_class(const char*,const char* const*,std::uint32_t,
                             A2FO_GameObjectClassLoadedHandler,void*){
    registrations+='C';return fail_registration!=1;
}
bool A2FO_CALL register_craft(const char*,std::uint32_t,A2FO_CraftEventHandler,void*){
    registrations+='E';return fail_registration!=2;
}
bool A2FO_CALL fake_hook(void*,void*,std::size_t,const std::uint8_t*,A2FO_InlineHook*){return false;}
bool A2FO_CALL fake_patch(void*,void*,const std::uint8_t*,std::size_t){return false;}
void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
void A2FO_CALL fake_log(const char*,const char* line){++logs;std::printf("log:%s\n",line);}
template<class T>void put(void* p,std::size_t offset,T value){
    std::memcpy(static_cast<char*>(p)+offset,&value,sizeof(value));
}
void jump(void* location,void* target){
    std::uint8_t bytes[]{0xb8,0,0,0,0,0xff,0xe0};
    std::memcpy(location,bytes,sizeof(bytes));put(location,1,reinterpret_cast<std::uintptr_t>(target));
}
A2FO_OdfFieldView field_view(const char* name,const char* value){
    return {{name,static_cast<std::uint32_t>(std::strlen(name))},
            {value,static_cast<std::uint32_t>(std::strlen(value))}};
}
void class_event(const char* source,void* klass,const A2FO_OdfFieldView* fields,std::uint32_t count){
    A2FO_GameObjectClassLoadedEvent event{};event.struct_size=sizeof(event);
    event.object_class=klass;event.source_odf={source,static_cast<std::uint32_t>(std::strlen(source))};
    event.odf_fields=fields;event.odf_field_count=count;class_loaded(&event,nullptr);
}
void* __cdecl fake_find(const char* name){
    ++finds;
    if(std::strcmp(name,"late")!=0)return nullptr;
    A2FO_OdfFieldView fields[]{field_view("XPlevel","1")};
    class_event("late.odf",reinterpret_cast<void*>(0x54321),fields,1);
    return reinterpret_cast<void*>(0x54321);
}
void run(){
    armada=static_cast<HMODULE>(VirtualAlloc(nullptr,0x400000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    check(armada!=nullptr,"image allocation");jump(at(armada,0xcd370),reinterpret_cast<void*>(&fake_find));
    jump(at(armada,0x12c680),reinterpret_cast<void*>(&fake_out));
    jump(at(armada,0x12d7a0),reinterpret_cast<void*>(&fake_in));
    save_hook.gateway=reinterpret_cast<void*>(&fake_team);
    load_hook.gateway=reinterpret_cast<void*>(&fake_team);
    clean_hook.gateway=reinterpret_cast<void*>(&fake_clean);
    A2FO_ModuleApi supplied{};supplied.log=fake_log;api=&supplied;
    A2FO_OdfFieldView first[]{field_view("XPlevel","1"),field_view("nextrankXP","10"),
        field_view("nextrankClass","ship2")};
    A2FO_OdfFieldView second[]{field_view("XPlevel","2")};
    class_event("  \"SHIP1.ODF\"  ",reinterpret_cast<void*>(0x10001),first,3);
    class_event("ship2.odf",reinterpret_cast<void*>(0x10002),second,1);
    check(definitions.count("ship1")&&definitions.count("ship2"),"normalized class storage");
    check(prepare_chain("ship1"),"rank chain");
    check(family("ship2")=="ship1","parent family");
    check(prepare_chain("late")&&finds==1&&definitions.count("late"),"reentrant native class find");
    A2FO_OdfFieldView cyclea[]{field_view("XPlevel","1"),field_view("nextrankXP","1"),
        field_view("nextrankClass","cycleb")};
    A2FO_OdfFieldView cycleb[]{field_view("XPlevel","2"),field_view("nextrankXP","1"),
        field_view("nextrankClass","cyclea")};
    class_event("cyclea",reinterpret_cast<void*>(0x10003),cyclea,3);
    class_event("cycleb",reinterpret_cast<void*>(0x10004),cycleb,3);
    check(!prepare_chain("cyclea"),"cycle rejection");
    std::array<std::uint8_t,0x700> race{},team{};
    void* raceptr=race.data();put(team.data(),0x244,raceptr);
    put(at(armada,0x338db0),0,team.data());
    A2FO_OdfFieldView race_fields[]{field_view("collectiveXP","1"),
        field_view("collectiveXPModifier","1.5")};
    A2FO_RaceLoadedEvent event{sizeof(event),race.data(),race_fields,2};
    ready=true;race_loaded(&event,nullptr);
    check(factions[race.data()].enabled&&race[0x634]==1,"faction enabled");
    check(configure(0),"player enabled");
    reconcile(0);
    check(save_team(team.data(),nullptr,nullptr)==1&&saved.size()==33,"team save");
    check(load_team(team.data(),nullptr,nullptr)==1,"team restore");
    auto original=saved;saved.clear();
    check(load_team(team.data(),nullptr,nullptr)==0,"invalid save rejected");
    saved=std::move(original);
    std::array<std::uint8_t,0x300> craft{};
    std::array<std::uint8_t,0x40> enhancement{};
    std::array<std::uint8_t,0x100> rank_class{};
    put(craft.data(),0x14,std::uint32_t{8});put(craft.data(),0x40,reinterpret_cast<void*>(0x10001));
    put(craft.data(),0xec,std::int32_t{0});put(craft.data(),0x1a4,enhancement.data());
    put(enhancement.data(),4,rank_class.data());put(rank_class.data(),0xac,std::int32_t{1});
    observe(craft.data());check(identities.size()==1&&units.size()==1,"unit observed");
    std::array<std::uint8_t,0x100> victim_rank{};
    std::array<std::uint8_t,0x20> victim_enhancement{};
    put(victim_rank.data(),0xb4,10.0f);put(victim_enhancement.data(),4,victim_rank.data());
    check(a2fo_collective_award(craft.data(),victim_enhancement.data())==1,"collective award");
    put(victim_rank.data(),0xb4,5.0f);
    check(a2fo_collective_award(craft.data(),victim_enhancement.data())==1,"rank threshold");
    unsigned level=0;double xp=0,threshold=0;
    check(A2FOCollectiveXP_GetProgress(0,&level,&xp,&threshold)&&level==2,"progress");
    check(A2FOCollectiveXP_IsEnabledForObject(craft.data()),"enabled object");
    check(a2fo_collective_level(craft.data())==1,"level boundary");
    check(A2FOCollectiveXP_ResolveBuildClass(craft.data(),reinterpret_cast<void*>(0x99999))==
        reinterpret_cast<void*>(0x99999),"ordinary job");
    check(A2FOCollectiveXP_ResolveBuildClass(craft.data(),reinterpret_cast<void*>(0x10001))==nullptr,
        "unlisted tech class rejected");
    std::printf("progress:%u:%.6g:%.6g:%zu\n",level,xp,threshold,saved.size());
    std::array<std::uint8_t,0x20> list{},head{},node{};
    put(list.data(),4,head.data());put(list.data(),8,std::uint32_t{1});
    put(head.data(),0,node.data());put(node.data(),0,head.data());put(node.data(),8,craft.data());
    put(at(armada,0x361084),0,list.data());
    recalculate();check(seeded&&identities.size()==1,"fleet validation");
    put(node.data(),0,static_cast<void*>(nullptr));
    recalculate();check(identities.size()==1,"damaged list preserves unit");
    A2FO_CraftEvent cleanup{};cleanup.struct_size=sizeof(cleanup);
    cleanup.kind=A2FO_CRAFT_EVENT_CLEANUP;cleanup.craft=craft.data();
    craft_event(&cleanup,nullptr);check(identities.empty()&&units.empty(),"cleanup");
    race_fields[1]=field_view("collectiveXPModifier","nan");race_loaded(&event,nullptr);
    check(factions[race.data()].enabled,"invalid config retains previous");
    clean_teams();check(identities.empty()&&units.empty()&&reports.empty()&&!seeded,"match reset");
    std::printf("counts:%d:%d:%zu:%zu\n",logs,finds,definitions.size(),parents.size());
    check(!A2FO_ModuleInit(nullptr),"null init");
    ready=false;
    A2FO_ModuleApi init_api{};init_api.struct_size=sizeof(init_api);
    init_api.api_version=A2FO_MODULE_API_VERSION;init_api.log=fake_log;
    init_api.armada_module=image_module;init_api.fleetops_module=image_module;
    init_api.register_race_loaded_handler=register_race;
    init_api.register_game_object_class_loaded_handler=register_class;
    init_api.register_craft_event_handler_masked=register_craft;
    init_api.install_inline_hook=fake_hook;init_api.patch_jump=fake_patch;
    for(int failure=0;failure<3;++failure){fail_registration=failure;registrations.clear();
        check(!A2FO_ModuleInit(&init_api),"registration failure");
        check(registrations==std::string("RCE",static_cast<std::size_t>(failure+1)),"registration order");}
    fail_registration=-1;registrations.clear();
    check(A2FO_ModuleInit(&init_api)&&!ready&&registrations=="RCE","unsupported signature pass through");
    A2FO_ModuleShutdown();check(!ready,"shutdown gate");
    std::printf("init:%s:%d\n",registrations.c_str(),ready);
    VirtualFree(armada,0,MEM_RELEASE);
}
}
int main(){try{run();return 0;}catch(const std::exception& error){std::fprintf(stderr,"FAIL:%s\n",error.what());return 1;}}
