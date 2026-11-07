// Includes the complete production core; uncalled native hooks are link-GC'd.
#include CORE_SOURCE
#include <new>
#include <cstdlib>
#include <stdexcept>
static int allocation_budget=-1;
__attribute__((noinline)) void* operator new(std::size_t n) {
    if(allocation_budget==0){allocation_budget=-1;throw std::bad_alloc();}
    if(allocation_budget>0)--allocation_budget;
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
__attribute__((noinline)) void operator delete(void* p)noexcept{std::free(p);}
__attribute__((noinline)) void operator delete(void* p,std::size_t)noexcept{std::free(p);}
__attribute__((noinline)) void* operator new[](std::size_t n){return ::operator new(n);}
__attribute__((noinline)) void operator delete[](void* p)noexcept{::operator delete(p);}
__attribute__((noinline)) void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static DWORD WINAPI registration_lock_probe(void* arg){EnterCriticalSection(&g_state_lock);LeaveCriticalSection(&g_state_lock);SetEvent(static_cast<HANDLE>(arg));return 0;}
static bool register_test_fields(unsigned kind,const char* owner,const char* const* fields,unsigned count,bool handler){
    const std::uintptr_t address=handler?0x1000u+kind:0;
    switch(kind){
    case 0:return api_register_object_destroyed_handler(owner,fields,count,reinterpret_cast<A2FO_ObjectDestroyedHandler>(address),nullptr);
    case 1:return api_register_game_object_class_loaded_handler(owner,fields,count,reinterpret_cast<A2FO_GameObjectClassLoadedHandler>(address),nullptr);
    case 2:return api_register_race_loaded_handler(owner,fields,count,reinterpret_cast<A2FO_RaceLoadedHandler>(address),nullptr);
    default:return api_register_weapon_class_loaded_handler(owner,fields,count,reinterpret_cast<A2FO_WeaponClassLoadedHandler>(address),nullptr);
    }
}
static bool register_test_simple(unsigned kind,const char* owner,bool handler){
    const std::uintptr_t address=handler?0x2000u+kind:0;
    switch(kind){
    case 0:return api_register_producer_event_handler(owner,reinterpret_cast<A2FO_ProducerEventHandler>(address),reinterpret_cast<void*>(0x55));
    case 1:return api_register_weapon_trigger_handler(owner,reinterpret_cast<A2FO_WeaponTriggerHandler>(address),reinterpret_cast<void*>(0x55));
    default:return api_register_craft_event_handler(owner,reinterpret_cast<A2FO_CraftEventHandler>(address),reinterpret_cast<void*>(0x55));
    }
}
struct ProducerProbe {unsigned id;bool accept;int mutate_kind=-1;unsigned action=0;};
static std::string producer_trace;
static bool A2FO_CALL producer_probe_handler(const A2FO_ProducerEvent* event,void* data){
    auto& probe=*static_cast<ProducerProbe*>(data);producer_trace.push_back(static_cast<char>('A'+probe.id));
    if(probe.mutate_kind>=0)const_cast<A2FO_ProducerEvent*>(event)->kind=static_cast<unsigned>(probe.mutate_kind);
    if(probe.action==1)g_producer_event_handlers.clear();
    if(probe.action==2)api_register_producer_event_handler("reentrant",&producer_probe_handler,data);
    if(probe.action==3)throw std::runtime_error("native handler failure");
    if(probe.action==4){auto* mutable_event=const_cast<A2FO_ProducerEvent*>(event);mutable_event->producer=nullptr;mutable_event->target_class=nullptr;g_state_lock_ready=false;}
    return probe.accept;
}
struct DestroyProbe {unsigned id;bool claim=true;unsigned action=0;const char* odf="replacement.odf";std::uint32_t size=sizeof(A2FO_ObjectReplacement);std::uint32_t flags=A2FO_REPLACEMENT_INHERIT_POSITION|A2FO_REPLACEMENT_INHERIT_ROTATION;std::uint32_t owner=A2FO_REPLACEMENT_OWNER_ORIGINAL;};
static bool A2FO_CALL destroy_probe_handler(const A2FO_ObjectDestroyedEvent* event,A2FO_ObjectReplacement* replacement,void* data){
    auto& p=*static_cast<DestroyProbe*>(data);
    std::printf("destroy-handler:%u:%u:%u:%u:%u\n",p.id,event->source_handle,event->odf_field_count,replacement->struct_size,replacement->flags);
    if(p.action==1)throw std::runtime_error("native destroyed handler failure");
    if(p.action==2||p.action==3){g_object_destroyed_handlers.clear();if(p.action==3){NativeObjectDestroyedRegistration fresh;fresh.owner="reentrant";fresh.handler=&destroy_probe_handler;fresh.user_data=data;g_object_destroyed_handlers.push_back(fresh);}throw std::runtime_error("after native registry mutation");}
    if(p.action==4)const_cast<A2FO_ObjectDestroyedEvent*>(event)->source_handle=55;
    replacement->struct_size=p.size;replacement->odf=p.odf;replacement->flags=p.flags;replacement->owner=p.owner;
    return p.claim;
}
struct SpawnProbe {
    bool find=true,construct=true;unsigned mission_mode=3,throw_phase=0,rebase_phase=0;
    HMODULE alternate=nullptr;unsigned finds=0,constructs=0,missions=0,adds=0;
    std::array<std::uint32_t,12> bits{};int team=0;
} spawn_probe;
static void* spawn_vtable[7]{};
static struct {void** vtable;} spawn_mission;
static void* A2FO_CALL spawn_find(const char* name){
    if(std::strcmp(name,"spawn.odf")!=0)std::abort();
    ++spawn_probe.finds;
    if(spawn_probe.throw_phase==1)throw std::runtime_error("find failure");
    if(spawn_probe.rebase_phase==1)g_armada=spawn_probe.alternate;
    return spawn_probe.find?reinterpret_cast<void*>(0x1111):nullptr;
}
static void* A2FO_CALL spawn_get_mission(){
    ++spawn_probe.missions;
    if(spawn_probe.throw_phase==3)throw std::runtime_error("mission failure");
    if(spawn_probe.mission_mode==0)return nullptr;
    spawn_mission.vtable=spawn_probe.mission_mode==1?nullptr:spawn_vtable;
    spawn_vtable[6]=spawn_probe.mission_mode==2?nullptr:reinterpret_cast<void*>(0x3333);
    return &spawn_mission;
}
extern "C" void* a2fo_call_game_object_construct(void* function,void* object_class,const void* transform,int team,int parent,char* label){
    if(function!=at(g_armada,kGameObjectClassConstructRva)||object_class!=reinterpret_cast<void*>(0x1111)||parent!=0||label)std::abort();
    ++spawn_probe.constructs;spawn_probe.team=team;std::memcpy(spawn_probe.bits.data(),transform,sizeof(spawn_probe.bits));
    if(spawn_probe.throw_phase==2)throw std::runtime_error("construct failure");
    if(spawn_probe.rebase_phase==2)g_armada=spawn_probe.alternate;
    return spawn_probe.construct?reinterpret_cast<void*>(0x2222):nullptr;
}
extern "C" void a2fo_call_game_object_argument(void* function,void* self,void* argument){
    if(function!=reinterpret_cast<void*>(0x3333)||self!=&spawn_mission||argument!=reinterpret_cast<void*>(0x2222))std::abort();
    ++spawn_probe.adds;if(spawn_probe.throw_phase==4)throw std::runtime_error("add failure");
}
static void install_spawn_jump(std::uint8_t* base,std::uintptr_t rva,std::uintptr_t target){
    auto* site=base+rva;site[0]=0xe9;
    const auto displacement=static_cast<std::uint32_t>(target-reinterpret_cast<std::uintptr_t>(site+5));
    std::memcpy(site+1,&displacement,sizeof(displacement));FlushInstructionCache(GetCurrentProcess(),site,5);
}
static unsigned cocoon_mode=0,cocoon_loads=0;
static std::uint8_t* cocoon_object=nullptr;
extern "C" void* a2fo_load_sod(void* function,void* database,const char* name){
    if(function!=at(g_armada,kLoadSodRva)||database!=reinterpret_cast<void*>(0x9090))std::abort();
    const bool custom=_stricmp(name,"CuStOm.SoD")==0;
    if(!custom&&std::strcmp(name,kAlternativeCocoonName)!=0&&std::strcmp(name,kDefaultCocoonName)!=0)std::abort();
    ++cocoon_loads;std::printf("load-cocoon:%u:%u:%u\n",custom,std::strcmp(name,kAlternativeCocoonName)==0,std::strcmp(name,kDefaultCocoonName)==0);
    if((custom&&cocoon_mode==11)||(!custom&&cocoon_mode==12))throw std::runtime_error("SOD load failure");
    if(custom&&cocoon_mode==13){const std::uint32_t value=1;std::memcpy(cocoon_object+0x1e8,&value,sizeof(value));}
    if(custom)return (cocoon_mode==7||cocoon_mode==10||cocoon_mode==12||cocoon_mode==13||cocoon_mode==15)?nullptr:reinterpret_cast<void*>(0x4000);
    return cocoon_mode==10?nullptr:reinterpret_cast<void*>(0x5000);
}
static void hex(const std::string& value){for(unsigned char c:value)std::printf("%02x",c);}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    const std::vector<std::string> cases={"","a","_",".","..","a_b-c","a.b","a/b","a\\b","a/../b","a//b","a/","/a","a.","a ","a/b.c","a:b","a?b","a*b","a|b","a<b","a>b","a\"b","a\tb",std::string(63,'x'),std::string(64,'x'),std::string(127,'x'),std::string(128,'x'),std::string(255,'x'),std::string(256,'x')};
    unsigned row=0;
    for(const auto& value:cases){
        std::string out="sentinel";
        const bool ok=normalize_relative_directory(value.c_str(),out);
        std::printf("row:%u:%u%u%u%u:%u:",row++,valid_policy_identifier(value.c_str()),valid_odf_field_name(value.c_str()),valid_odf_default_value(value.c_str()),valid_replacement_odf_name(value.data(),value.size()),ok);hex(out);std::puts("");
        std::string alias=value;const bool alias_ok=normalize_relative_directory(alias.c_str(),alias);
        std::printf("alias:%u:",alias_ok);hex(alias);std::puts("");
    }
    for(unsigned ch=1;ch<=255;++ch){char text[2]{static_cast<char>(ch),0};
        std::printf("byte:%u:%u%u%u%u\n",ch,valid_policy_identifier(text),valid_odf_field_name(text),valid_odf_default_value(text),valid_replacement_odf_name(text,1));}
    std::string nullout="sentinel";const bool nullok=normalize_relative_directory(nullptr,nullout);
    std::printf("null:%u%u%u%u:%u:%zu\n",valid_policy_identifier(nullptr),valid_odf_field_name(nullptr),valid_odf_default_value(nullptr),valid_replacement_odf_name(nullptr,1),nullok,nullout.size());
    SYSTEM_INFO system{};GetSystemInfo(&system);auto page=system.dwPageSize;
    auto* memory=static_cast<char*>(VirtualAlloc(nullptr,page*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!memory) return 2;
    DWORD old=0;
    if(!VirtualProtect(memory+page,page,PAGE_NOACCESS,&old)) return 3;
    char* edge=memory+page-1;*edge='!';
    std::printf("guard:%u%u%u\n",valid_policy_identifier(edge),valid_odf_field_name(edge),valid_replacement_odf_name(edge,2));
    *edge=1;std::printf("guard-default:%u\n",valid_odf_default_value(edge));
    *edge='?';std::string bad="sentinel";std::printf("guard-path:%u\n",normalize_relative_directory(edge,bad));VirtualFree(memory,0,MEM_RELEASE);
    const std::string longpath=std::string(40,'a')+"/"+std::string(40,'b');
    for(int budget=0;budget<12;++budget){std::string out="sentinel";bool threw=false,ok=false;allocation_budget=budget;
        try{ok=normalize_relative_directory(longpath.c_str(),out);}catch(const std::bad_alloc&){threw=true;}
        allocation_budget=-1;std::printf("alloc:%d:%u:%u:",budget,threw,ok);hex(out);std::puts("");}

    InitializeCriticalSection(&g_state_lock);
    const auto reset_registration=[](){
        g_classlabel_aliases.clear();g_classlabel_odf_defaults.clear();
        g_race_odf_defaults.clear();g_race_odf_fields.clear();g_odf_overlay_directories.clear();
        g_evolver_cocoon_command.clear();g_evolver_cocoon_owner.clear();
        g_registration_transaction=RegistrationTransaction{};
        g_game_object_class_odf_fields.clear();g_weapon_class_odf_fields.clear();
        g_object_destroyed_handlers.clear();g_game_object_class_loaded_handlers.clear();g_race_loaded_handlers.clear();g_weapon_class_loaded_handlers.clear();
        g_producer_event_handlers.clear();g_weapon_trigger_handlers.clear();g_craft_event_handlers.clear();
        g_policy_registration_open=true;g_registration_transaction.active=true;
    };
    const auto dump_registration=[](const char* label,bool result,bool threw){
        std::printf("registration:%s:%u:%u:%zu:",label,result,threw,g_classlabel_aliases.size());
        if(!g_classlabel_aliases.empty()){
            const auto& item=*g_classlabel_aliases.begin();hex(item.first);std::putchar(':');
            hex(item.second.target);std::putchar(':');hex(item.second.owner);
        }
        std::putchar(':');hex(g_evolver_cocoon_command);std::putchar(':');hex(g_evolver_cocoon_owner);std::puts("");
    };
    for(unsigned mode=0;mode<8;++mode){
        reset_registration();
        if(mode==1)g_policy_registration_open=false;
        if(mode==2)g_registration_transaction.active=false;
        if(mode==3)g_classlabel_aliases.emplace("source",ClasslabelAliasPolicy{"oldtarget","original owner"});
        const char* source=mode==4?nullptr:mode==5?"invalid!":"SOURCE";
        const char* target=mode==6?nullptr:mode==7?"invalid!":"TARGET";
        bool result=register_classlabel_alias_policy(mode?"owner":nullptr,source,target);
        dump_registration(("alias-"+std::to_string(mode)).c_str(),result,false);
    }
    for(unsigned mode=0;mode<7;++mode){
        reset_registration();
        if(mode==1)g_policy_registration_open=false;
        if(mode==2)g_registration_transaction.active=false;
        if(mode==3){g_evolver_cocoon_command="existing";g_evolver_cocoon_owner="original owner";}
        const char* input=mode==4?nullptr:mode==5?"invalid!":mode==6?"":"COCOON";
        bool result=register_evolver_cocoon_policy(mode?"owner":"",input);
        dump_registration(("cocoon-"+std::to_string(mode)).c_str(),result,false);
    }
    const std::string owner(80,'o'),source(50,'s'),target(50,'t');
    for(unsigned kind=0;kind<2;++kind)for(int budget=0;budget<24;++budget){
        reset_registration();bool result=false,threw=false;allocation_budget=budget;
        try{result=kind?register_evolver_cocoon_policy(owner.c_str(),source.c_str()):register_classlabel_alias_policy(owner.c_str(),source.c_str(),target.c_str());}
        catch(const std::bad_alloc&){threw=true;}
        allocation_budget=-1;
        dump_registration((std::string(kind?"cocoon-alloc-":"alias-alloc-")+std::to_string(budget)).c_str(),result,threw);
        // An exception must release the native registration guard before return.
        HANDLE event=CreateEventA(nullptr,TRUE,FALSE,nullptr);
        HANDLE thread=CreateThread(nullptr,0,&registration_lock_probe,event,0,nullptr);
        if(!event||!thread||WaitForSingleObject(event,5000)!=WAIT_OBJECT_0)return 5;
        WaitForSingleObject(thread,5000);CloseHandle(thread);CloseHandle(event);
    }

    const auto dump_defaults=[](const char* label,bool result,bool threw){
        std::printf("defaults:%s:%u:%u:%zu:%zu:%zu:",label,result,threw,g_classlabel_odf_defaults.size(),g_race_odf_defaults.size(),g_race_odf_fields.size());
        std::vector<std::string> rows;
        for(const auto& entry:g_classlabel_odf_defaults){for(const auto& item:entry.second.defaults)rows.push_back(entry.first+"|"+entry.second.owner+"|"+item.first+"|"+item.second);}
        for(const auto& entry:g_race_odf_defaults)rows.push_back(entry.first+"|"+entry.second.owner+"|"+entry.second.value);
        std::sort(rows.begin(),rows.end());for(const auto& row:rows){hex(row);std::putchar(':');}
        std::puts("");
    };
    for(unsigned kind=0;kind<2;++kind)for(unsigned mode=0;mode<12;++mode){
        reset_registration();
        A2FO_ClasslabelOdfDefault cls[]={{"Rate","1.5"},{"Health","20"}};
        A2FO_RaceOdfDefault race[]={{"Rate","1.5"},{"Health","20"}};
        if(mode==1)g_policy_registration_open=false;
        if(mode==2)g_registration_transaction.active=false;
        if(mode==3){g_classlabel_odf_defaults.emplace("ship",ClasslabelOdfDefaultsPolicy{{{"old","value"}},"old owner"});g_race_odf_defaults.emplace("rate",RaceOdfDefaultPolicy{"old value","old owner"});g_race_odf_fields.insert("rate");}
        if(mode==4){cls[1].command="RATE";race[1].command="RATE";}
        if(mode==5){cls[1].command="bad!";race[1].command="bad!";}
        if(mode==6){cls[1].value="\1";race[1].value="\1";}
        if(mode==7){cls[0].command=nullptr;race[0].command=nullptr;}
        if(mode==8){cls[0].value=nullptr;race[0].value=nullptr;}
        const unsigned count=mode==9?0:mode==10?65:2;
        bool result=kind?register_race_odf_defaults_policy("owner",mode==11?nullptr:race,count):register_classlabel_odf_defaults_policy("owner","SHIP",mode==11?nullptr:cls,count);
        dump_defaults((std::string(kind?"race-":"class-")+std::to_string(mode)).c_str(),result,false);
    }
    const std::string field(70,'f'),field2(70,'g'),default_value(90,'v');
    A2FO_ClasslabelOdfDefault cls[]={{field.c_str(),default_value.c_str()},{field2.c_str(),default_value.c_str()}};
    A2FO_RaceOdfDefault race[]={{field.c_str(),default_value.c_str()},{field2.c_str(),default_value.c_str()}};
    for(unsigned kind=0;kind<2;++kind)for(int budget=0;budget<55;++budget){
        reset_registration();bool result=false,threw=false;allocation_budget=budget;
        try{result=kind?register_race_odf_defaults_policy(owner.c_str(),race,2):register_classlabel_odf_defaults_policy(owner.c_str(),source.c_str(),cls,2);}
        catch(const std::bad_alloc&){threw=true;}
        allocation_budget=-1;
        dump_defaults((std::string(kind?"race-alloc-":"class-alloc-")+std::to_string(budget)).c_str(),result,threw);
    }

    for(unsigned mode=0;mode<5;++mode){
        reset_registration();g_registration_transaction=RegistrationTransaction{};
        g_registration_transaction.active=true;
        register_classlabel_alias_policy("before","before","base");
        register_evolver_cocoon_policy("before","before");
        g_game_object_class_odf_fields.insert("oldfield");
        begin_module_registration("module-path");
        g_classlabel_aliases.clear();g_evolver_cocoon_command="after";g_evolver_cocoon_owner="after owner";
        g_game_object_class_odf_fields.clear();g_game_object_class_odf_fields.insert("newfield");
        if(mode==3)g_registration_transaction.active=false;
        finish_module_registration(mode==2?"other-path":"module-path",mode==1);
        dump_registration(("transaction-"+std::to_string(mode)).c_str(),false,false);
        std::printf("transaction-state:%u:%u:%zu:",mode,g_registration_transaction.active,g_game_object_class_odf_fields.size());hex(g_registration_transaction.path);std::putchar(':');
        for(const auto& name:g_game_object_class_odf_fields){hex(name);std::putchar(':');}std::puts("");
        if(mode==4)finish_module_registration("module-path",false);
    }
    for(int budget=0;budget<70;++budget){
        reset_registration();register_classlabel_alias_policy(owner.c_str(),source.c_str(),target.c_str());
        register_evolver_cocoon_policy(owner.c_str(),source.c_str());
        g_game_object_class_odf_fields.insert(field);bool threw=false;allocation_budget=budget;
        try{begin_module_registration(owner);}catch(const std::bad_alloc&){threw=true;}
        allocation_budget=-1;
        std::printf("begin-alloc:%d:%u:%u:",budget,threw,g_registration_transaction.active);hex(g_registration_transaction.path);std::puts("");
        finish_module_registration(owner,false);
        dump_registration(("after-begin-"+std::to_string(budget)).c_str(),false,false);
    }

    const auto dump_overlays=[](unsigned mode,bool result,bool threw){
        std::printf("overlay:%u:%u:%u:%zu:",mode,result,threw,g_odf_overlay_directories.size());
        for(const auto& item:g_odf_overlay_directories){hex(item.directory);std::putchar(':');hex(item.owner);std::printf(":%u:",item.precedence);}std::puts("");
    };
    for(unsigned mode=0;mode<12;++mode){
        reset_registration();
        if(mode==1)g_policy_registration_open=false;
        if(mode==2)g_registration_transaction.active=false;
        if(mode==3)g_odf_overlay_directories.push_back({"PATH\\Dir","original owner",0});
        if(mode==4)for(unsigned n=0;n<64;++n)g_odf_overlay_directories.push_back({"entry"+std::to_string(n),"owner",0});
        const char* input=mode==5?nullptr:mode==6?"../unsafe":mode==7?"":mode==8?"a//b":"path/dir";
        const unsigned precedence=mode==9?2:mode==10?UINT32_MAX:mode==11?0:1;
        dump_overlays(mode,register_odf_overlay_directory_policy("owner",input,precedence),false);
    }
    for(int budget=0;budget<45;++budget){
        reset_registration();bool result=false,threw=false;allocation_budget=budget;
        try{result=register_odf_overlay_directory_policy(owner.c_str(),source.c_str(),1);}catch(const std::bad_alloc&){threw=true;}
        allocation_budget=-1;dump_overlays(100+budget,result,threw);
    }

    const auto dump_field_handlers=[](unsigned kind,unsigned mode,bool result,bool threw){
        std::printf("fields:%u:%u:%u:%u:",kind,mode,result,threw);
        const auto dump=[](const auto& registrations){
            std::printf("%zu:",registrations.size());for(const auto& item:registrations){hex(item.owner);std::putchar(':');for(const auto& field:item.required_odf_fields){hex(field);std::putchar(',');}}
        };
        dump(g_object_destroyed_handlers);dump(g_game_object_class_loaded_handlers);dump(g_race_loaded_handlers);dump(g_weapon_class_loaded_handlers);
        for(const auto& field:g_game_object_class_odf_fields){hex(field);std::putchar(',');}std::putchar(':');
        for(const auto& field:g_race_odf_fields){hex(field);std::putchar(',');}std::putchar(':');
        for(const auto& field:g_weapon_class_odf_fields){hex(field);std::putchar(',');}std::puts("");
    };
    for(unsigned kind=0;kind<4;++kind)for(unsigned mode=0;mode<11;++mode){
        reset_registration();const char* fields[]={"Rate","HEALTH","RATE"};
        if(mode==5)fields[1]="bad!";
        if(mode==6)fields[1]=nullptr;
        if(mode==7)g_policy_registration_open=false;
        if(mode==8)g_registration_transaction.active=false;
        if(mode==9)for(unsigned n=0;n<64;++n)register_test_fields(kind,"seed",nullptr,0,true);
        const char* const* input=mode==1||mode==4?reinterpret_cast<const char* const*>(1):mode==2||mode==3?nullptr:fields;
        const unsigned count=mode==1?65:mode==3||mode==10?0:3;
        const bool result=register_test_fields(kind,"owner",input,count,mode!=4);
        dump_field_handlers(kind,mode,result,false);
    }
    const char* longfields[]={field.c_str(),field2.c_str(),field.c_str()};
    for(unsigned kind=0;kind<4;++kind)for(int budget=0;budget<60;++budget){
        reset_registration();bool result=false,threw=false;allocation_budget=budget;
        try{result=register_test_fields(kind,owner.c_str(),longfields,3,true);}catch(const std::bad_alloc&){threw=true;}
        allocation_budget=-1;dump_field_handlers(kind,100+budget,result,threw);
    }
    const std::vector<std::string> names={""," ","\t\r\n","Ship"," .SOD ","a/b.SOD","a\\b.ODF",".odf","a.odf"," a/b.odf ","a\\",std::string("a\0b",3),std::string(80,'X')};
    for(const auto& input:names){
        std::string lower=lower_ascii(input),slashes=input;replace_slashes(slashes,'!');
        std::printf("text:");hex(input);std::putchar(':');hex(lower);std::putchar(':');hex(slashes);std::putchar(':');hex(normalize_cocoon_name(input.c_str()));std::putchar(':');hex(normalize_odf_basename(input));std::puts("");
        for(const auto& right:names){std::printf("join:");hex(join_path(input,right));std::puts("");}
    }
    std::string allbytes;for(unsigned i=0;i<256;++i)allbytes.push_back(static_cast<char>(i));
    std::printf("lower-all:");hex(lower_ascii(allbytes));std::puts("");
    for(int budget=0;budget<16;++budget){bool threw=false;std::string output;allocation_budget=budget;
        try{output=normalize_cocoon_name((source+" /"+target).c_str());}catch(const std::bad_alloc&){threw=true;}
        allocation_budget=-1;std::printf("name-alloc:%d:%u:",budget,threw);hex(output);std::puts("");}


    const auto dump_simple=[](unsigned kind,unsigned mode,bool result,bool threw){
        std::printf("simple-handler:%u:%u:%u:%u:",kind,mode,result,threw);
        const auto dump=[](const auto& entries){std::printf("%zu:",entries.size());for(const auto& item:entries){hex(item.owner);std::printf(":%08x:%08x:",static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(item.handler)),static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(item.user_data)));}};
        dump(g_producer_event_handlers);dump(g_weapon_trigger_handlers);dump(g_craft_event_handlers);std::puts("");
    };
    for(unsigned kind=0;kind<3;++kind)for(unsigned mode=0;mode<8;++mode){
        reset_registration();g_state_lock_ready=true;
        if(mode==1)g_state_lock_ready=false;
        if(mode==2)g_policy_registration_open=false;
        if(mode==3)g_registration_transaction.active=false;
        if(mode==4)for(unsigned n=0;n<64;++n)register_test_simple(kind,"seed",true);
        const bool result=register_test_simple(kind,mode==6?nullptr:mode==7?"":"owner",mode!=5);
        dump_simple(kind,mode,result,false);
    }
    for(unsigned kind=0;kind<3;++kind)for(int budget=0;budget<30;++budget){
        reset_registration();g_state_lock_ready=true;bool result=false,threw=false;allocation_budget=budget;
        try{result=register_test_simple(kind,owner.c_str(),true);}catch(const std::bad_alloc&){threw=true;}
        allocation_budget=-1;dump_simple(kind,100+budget,result,threw);
    }
    const auto dispatch_probe=[](unsigned mode,A2FO_ProducerEvent* event){
        producer_trace.clear();bool result=false,threw=false;
        try{result=api_dispatch_producer_event(event);}catch(const std::runtime_error&){threw=true;}
        allocation_budget=-1;std::printf("producer:%u:%u:%u:%u:%zu:",mode,result,threw,event?event->kind:99,g_producer_event_handlers.size());hex(producer_trace);std::puts("");
    };
    for(unsigned kind=0;kind<9;++kind)for(unsigned mode=0;mode<7;++mode){
        g_state_lock_ready=true;g_producer_event_handlers.clear();producer_trace.reserve(32);
        ProducerProbe probes[]={{0,false,-1,0},{1,true,-1,0}};
        g_producer_event_handlers.push_back({"first",&producer_probe_handler,&probes[0]});
        g_producer_event_handlers.push_back({"null",nullptr,nullptr});
        g_producer_event_handlers.push_back({"second",&producer_probe_handler,&probes[1]});
        A2FO_ProducerEvent event{sizeof(A2FO_ProducerEvent),kind,reinterpret_cast<void*>(1),reinterpret_cast<void*>(2)};
        if(mode==1)event.struct_size=0;
        if(mode==2)event.producer=nullptr;
        if(mode==3)event.target_class=nullptr;
        if(mode==4)g_state_lock_ready=false;
        if(mode==5)probes[0].accept=true;
        dispatch_probe(kind*10+mode,mode==6?nullptr:&event);
    }
    for(unsigned mode=0;mode<7;++mode){
        g_state_lock_ready=true;g_producer_event_handlers.clear();
        ProducerProbe probes[]={{0,false,-1,0},{1,true,-1,0}};
        A2FO_ProducerEvent event{sizeof(A2FO_ProducerEvent),A2FO_PRODUCER_EVENT_FINISHED,reinterpret_cast<void*>(1),reinterpret_cast<void*>(2)};
        if(mode==0){event.kind=A2FO_PRODUCER_EVENT_ADMIT;probes[0].mutate_kind=A2FO_PRODUCER_EVENT_FINISHED;}
        if(mode==1)probes[0].mutate_kind=A2FO_PRODUCER_EVENT_ADMIT;
        if(mode>=2)probes[0].action=mode-1;
        g_producer_event_handlers.push_back({"first",&producer_probe_handler,&probes[0]});
        g_producer_event_handlers.push_back({"second",&producer_probe_handler,&probes[1]});
        dispatch_probe(100+mode,&event);
    }
    for(unsigned kind=0;kind<8;++kind)for(int budget=0;budget<16;++budget){
        g_state_lock_ready=true;g_producer_event_handlers.clear();producer_trace.clear();producer_trace.reserve(32);
        ProducerProbe probe{0,true,-1,0};
        g_producer_event_handlers.push_back({owner,&producer_probe_handler,&probe});
        A2FO_ProducerEvent event{sizeof(A2FO_ProducerEvent),kind,reinterpret_cast<void*>(1),reinterpret_cast<void*>(2)};
        allocation_budget=budget;dispatch_probe(200+kind*20+budget,&event);
    }
    {
        SYSTEM_INFO sys{};GetSystemInfo(&sys);auto* data=static_cast<char*>(VirtualAlloc(nullptr,sys.dwPageSize*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if(!data)return 6;
        DWORD previous=0;if(!VirtualProtect(data+sys.dwPageSize,sys.dwPageSize,PAGE_NOACCESS,&previous))return 7;
        auto* event=reinterpret_cast<A2FO_ProducerEvent*>(data+sys.dwPageSize-sizeof(std::uint32_t));event->struct_size=0;
        // Printing only the result: the remaining event fields are inaccessible.
        std::printf("producer-guard:%u\n",api_dispatch_producer_event(event));VirtualFree(data,0,MEM_RELEASE);
    }

    for(const std::string& odf:std::vector<std::string>{"","a.odf",std::string("a\0b",3),std::string(255,static_cast<char>(0xff))})for(std::uint32_t handle:{0u,1u,UINT32_MAX})for(int team:{0,-1,INT32_MIN,INT32_MAX}){
        Matrix34Snapshot matrix;
        for(unsigned n=0;n<matrix.values.size();++n){const std::uint32_t bits=n%3==0?0x80000000u:n%3==1?0x7fc00001u:0xffffffffu;std::memcpy(&matrix.values[n],&bits,sizeof(bits));}
        std::printf("object-hash:%08x\n",hash_destroyed_object(odf,handle,team,matrix));
    }

    const auto setup_destroyed=[](DestroyProbe* probes){
        g_object_destroyed_handlers.clear();
        for(unsigned n=0;n<2;++n){NativeObjectDestroyedRegistration item;item.owner="owner"+std::to_string(n);item.handler=&destroy_probe_handler;item.user_data=&probes[n];g_object_destroyed_handlers.push_back(item);}
    };
    const auto run_destroyed=[](unsigned mode,const DestroyedObjectSnapshot& source){
        ObjectReplacement selected;selected.odf="sentinel";selected.inherit_position=false;selected.inherit_rotation=false;selected.owner=ReplacementOwner::neutral;
        bool result=false,threw=false;try{result=resolve_native_object_destroyed(source,selected);}catch(const std::bad_alloc&){threw=true;}
        allocation_budget=-1;std::printf("destroy-result:%u:%u:%u:",mode,result,threw);hex(selected.odf);std::printf(":%u:%u:%u:%zu:",selected.inherit_position,selected.inherit_rotation,selected.owner==ReplacementOwner::original,g_object_destroyed_handlers.size());
        for(const auto& registration:g_object_destroyed_handlers){std::putchar(registration.enabled?'1':'0');}
        std::puts("");
    };
    DestroyedObjectSnapshot destroy_source;destroy_source.valid=true;destroy_source.source_odf="source.odf";destroy_source.source_team=3;destroy_source.source_handle=7;destroy_source.random_seed=11;destroy_source.odf={{"basename","source.odf"},{"field","value"}};
    std::array<char,256> unterminated{};unterminated.fill('x');const std::string maxname(255,'x');
    for(unsigned mode=0;mode<16;++mode){
        DestroyProbe probes[]={{0},{1}};setup_destroyed(probes);
        if(mode==0)g_object_destroyed_handlers.clear();
        if(mode==1)g_object_destroyed_handlers.set_enabled(0,false);
        if(mode==2)g_object_destroyed_handlers.set_handler(0,nullptr);
        if(mode==3)probes[0].claim=false;
        if(mode==4)probes[0].action=1;
        if(mode==5)probes[0].action=2;
        if(mode==6)probes[0].action=3;
        if(mode==7){probes[0].action=4;probes[0].claim=false;}
        if(mode==8)probes[0].odf=nullptr;
        if(mode==9)probes[0].odf="";
        if(mode==10)probes[0].odf="bad/name";
        if(mode==11)probes[0].size=0;
        if(mode==12)probes[0].flags=0x80000000u;
        if(mode==13)probes[0].owner=UINT32_MAX;
        if(mode==14)probes[0].odf=unterminated.data();
        if(mode==15)probes[0].odf=maxname.c_str();
        run_destroyed(mode,destroy_source);
    }
    for(unsigned flags=0;flags<4;++flags)for(unsigned owner_mode=0;owner_mode<=A2FO_REPLACEMENT_OWNER_ORIGINAL;++owner_mode){
        DestroyProbe probes[]={{0},{1}};setup_destroyed(probes);probes[0].flags=flags;probes[0].owner=owner_mode;run_destroyed(100+flags*10+owner_mode,destroy_source);
    }
    const std::string replacement_name(100,'r');
    for(int budget=0;budget<35;++budget){
        DestroyProbe probes[]={{0},{1}};setup_destroyed(probes);probes[0].odf=replacement_name.c_str();
        for(auto& registration:g_object_destroyed_handlers)registration.owner=owner;
        allocation_budget=budget;run_destroyed(200+budget,destroy_source);
    }

    {
        const auto saved_armada=g_armada;
        auto* first=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x100000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
        auto* second=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x100000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
        if(!first||!second)return 8;
        for(auto* base:{first,second}){install_spawn_jump(base,kGameObjectClassFindRva,reinterpret_cast<std::uintptr_t>(&spawn_find));install_spawn_jump(base,kAiMissionGetCurrentRva,reinterpret_cast<std::uintptr_t>(&spawn_get_mission));}
        for(unsigned mode=0;mode<12;++mode)for(unsigned flags=0;flags<4;++flags)for(bool original:{false,true}){
            spawn_probe=SpawnProbe{};spawn_probe.alternate=reinterpret_cast<HMODULE>(second);g_armada=reinterpret_cast<HMODULE>(first);
            if(mode==1)spawn_probe.find=false;
            if(mode==2)spawn_probe.construct=false;
            if(mode>=3&&mode<=5)spawn_probe.mission_mode=mode-3;
            if(mode>=6&&mode<=9)spawn_probe.throw_phase=mode-5;
            if(mode>=10)spawn_probe.rebase_phase=mode-9;
            ObjectReplacement selected;selected.odf="spawn.odf";selected.inherit_rotation=(flags&1)!=0;selected.inherit_position=(flags&2)!=0;selected.owner=original?ReplacementOwner::original:ReplacementOwner::neutral;
            auto input=destroy_source;input.source_team=-3;
            for(unsigned n=0;n<input.transform.values.size();++n)input.transform.values[n]=static_cast<float>(n+1);
            bool threw=false;try{spawn_object_replacement(input,selected);}catch(const std::runtime_error&){threw=true;}
            std::printf("spawn:%u:%u:%u:%u:%u%u%u%u:%d:",mode,flags,original,threw,spawn_probe.finds,spawn_probe.constructs,spawn_probe.missions,spawn_probe.adds,spawn_probe.team);
            for(auto bits:spawn_probe.bits){std::printf("%08x,",bits);}
            std::puts("");
        }
        g_armada=saved_armada;VirtualFree(first,0,MEM_RELEASE);VirtualFree(second,0,MEM_RELEASE);
    }

    {
        const auto saved_armada=g_armada;
        auto* image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x3b0000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if(!image)return 9;
        g_armada=reinterpret_cast<HMODULE>(image);
        alignas(void*) std::array<std::uint8_t,0x200> object{};cocoon_object=object.data();
        const auto class_object=reinterpret_cast<void*>(0x88);std::memcpy(object.data()+0x40,&class_object,sizeof(class_object));
        for(cocoon_mode=0;cocoon_mode<16;++cocoon_mode){
            g_class_cocoons.clear();g_loaded_cocoons.clear();g_logged_cocoon_classes.clear();g_state_lock_ready=true;cocoon_loads=0;
            *at<void*>(g_armada,kDefaultCocoonRva)=nullptr;*at<void*>(g_armada,kAlternativeCocoonRva)=nullptr;*at<void*>(g_armada,kSodDatabaseRva)=reinterpret_cast<void*>(0x9090);
            const std::uint32_t normal=cocoon_mode==3?1:0;std::memcpy(object.data()+0x1e8,&normal,sizeof(normal));
            if(cocoon_mode>=4&&cocoon_mode!=9&&cocoon_mode!=14)g_class_cocoons[class_object]="CuStOm.SoD";
            if(cocoon_mode==4)g_loaded_cocoons["custom.sod"]=reinterpret_cast<void*>(0x3000);
            if(cocoon_mode==5)g_loaded_cocoons["custom.sod"]=nullptr;
            if(cocoon_mode==8)*at<void*>(g_armada,kSodDatabaseRva)=nullptr;
            if(cocoon_mode==9)*at<void*>(g_armada,kAlternativeCocoonRva)=reinterpret_cast<void*>(0x6000);
            if(cocoon_mode==1)g_state_lock_ready=false;
            void* result=nullptr;bool threw=false;
            try{result=a2fo_select_cocoon(cocoon_mode==0?nullptr:cocoon_mode==1?reinterpret_cast<void*>(1):object.data());if(cocoon_mode==15)result=a2fo_select_cocoon(object.data());}catch(const std::runtime_error&){threw=true;}
            std::printf("cocoon:%u:%u:%08x:%u:%zu:%zu:%08x:%08x\n",cocoon_mode,threw,static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(result)),cocoon_loads,g_loaded_cocoons.size(),g_logged_cocoon_classes.size(),static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(*at<void*>(g_armada,kDefaultCocoonRva))),static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(*at<void*>(g_armada,kAlternativeCocoonRva))));
        }
        g_class_cocoons.clear();g_loaded_cocoons.clear();g_logged_cocoon_classes.clear();g_armada=saved_armada;VirtualFree(image,0,MEM_RELEASE);
    }
    g_object_destroyed_handlers.clear();
    g_producer_event_handlers.clear();g_state_lock_ready=false;
    reset_registration();DeleteCriticalSection(&g_state_lock);
    std::puts("core validators: PASS");
}
