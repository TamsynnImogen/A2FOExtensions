// Actual complete core source with inert ParameterDB bridges. No game entrypoint.
#include CORE_SOURCE
#include <new>
#include <cstdlib>
#include <stdexcept>
#include <limits>

static int allocation_budget=-1;
__attribute__((noinline)) void* operator new(std::size_t n){
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

struct MockEntry {std::string text{};float number{};int integer{};
    bool has_text{},has_float{},has_int{},fill_without_nul{};};
struct MockDb {std::map<std::string,MockEntry> entries{};bool mutate{};};
static std::string calls;
static unsigned handler_calls{};
static unsigned handler_two_calls{};
static bool mutate_event{};
static void hex(const std::string& value){for(unsigned char byte:value)std::printf("%02x",byte);}
static std::uint64_t digest(const std::string& value){std::uint64_t hash=14695981039346656037ull;
    for(unsigned char byte:value){hash^=byte;hash*=1099511628211ull;}return hash;}

extern "C" bool a2fo_parameter_db_get_string(void* function,void* parameter_db,
    const char* key,char* output,std::uint32_t output_size,const char*){
    calls.push_back('S');
    if(function!=reinterpret_cast<void*>(0x1111)||!parameter_db||!key)return false;
    auto& db=*static_cast<MockDb*>(parameter_db);
    if(db.mutate){db.mutate=false;g_race_odf_fields.clear();g_race_loaded_handlers.clear();
        g_race_odf_defaults.clear();}
    const auto it=db.entries.find(key);
    if(it==db.entries.end()||!it->second.has_text)return false;
    if(it->second.fill_without_nul){std::memset(output,'X',output_size);return true;}
    const auto count=std::min<std::size_t>(it->second.text.size(),output_size-1);
    std::memcpy(output,it->second.text.data(),count);output[count]='\0';return true;
}
extern "C" bool a2fo_parameter_db_get_scalar(void* function,void* parameter_db,
    const char* key,void* output,std::uint32_t default_value_bits){
    calls.push_back(function==reinterpret_cast<void*>(0x2222)?'F':'I');
    if(!parameter_db||!key||default_value_bits!=0)return false;
    auto& db=*static_cast<MockDb*>(parameter_db);
    const auto it=db.entries.find(key);if(it==db.entries.end())return false;
    if(function==reinterpret_cast<void*>(0x2222)&&it->second.has_float){
        *static_cast<float*>(output)=it->second.number;return true;}
    if(function==reinterpret_cast<void*>(0x3333)&&it->second.has_int){
        *static_cast<int*>(output)=it->second.integer;return true;}
    return false;
}
static void A2FO_CALL first_handler(const A2FO_RaceLoadedEvent* event,void*){
    ++handler_calls;std::printf("handler-one:%u:%u:",event->struct_size,event->odf_field_count);
    for(std::uint32_t i=0;i<event->odf_field_count;++i){
        hex(std::string(event->odf_fields[i].name.data,event->odf_fields[i].name.size));
        std::putchar('=');hex(std::string(event->odf_fields[i].value.data,event->odf_fields[i].value.size));
        std::putchar(',');
    }
    std::puts("");
    if(mutate_event)const_cast<A2FO_RaceLoadedEvent*>(event)->race=reinterpret_cast<void*>(0x99);
    throw std::runtime_error("handler throw");
}
static void A2FO_CALL second_handler(const A2FO_RaceLoadedEvent* event,void*){
    ++handler_two_calls;
    std::printf("handler-two:%u:%p\n",event->odf_field_count,event->race);
}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    g_log=INVALID_HANDLE_VALUE;
    g_parameter_db_get_string_hook.gateway=reinterpret_cast<void*>(0x1111);
    g_parameter_db_get_float_hook.gateway=reinterpret_cast<void*>(0x2222);
    g_parameter_db_get_int_hook.gateway=reinterpret_cast<void*>(0x3333);
    MockDb db;
    db.entries["text"]={"alpha",0,0,true,false,false,false};
    db.entries["fraction"]={"",0.25f,0,false,true,false,false};
    db.entries["integer"]={"",0,42,false,false,true,false};
    db.entries["both"]={"winner",0.5f,7,true,true,true,false};
    db.entries["guard"]={"",0,0,true,false,false,true};
    db.entries["nan"]={"",std::numeric_limits<float>::quiet_NaN(),0,false,true,false,false};
    for(const char* key:{"text","fraction","integer","both","missing","guard","nan"}){
        std::string value="initial";calls.clear();
        const bool found=read_odf_snapshot_value(&db,key,value);
        std::printf("snapshot:%s:%u:%zu:%016llx:",key,found,value.size(),
            static_cast<unsigned long long>(digest(value)));
        if(value.size()<64)hex(value);
        std::printf(":%s\n",calls.c_str());
    }
    std::string alias="text";calls.clear();const bool alias_found=read_odf_snapshot_value(&db,alias.c_str(),alias);
    std::printf("snapshot-alias:%u:",alias_found);hex(alias);std::printf(":%s\n",calls.c_str());
    std::string null_key="initial";calls.clear();
    const bool null_key_found=read_odf_snapshot_value(&db,nullptr,null_key);
    std::printf("snapshot-null-key:%u:",null_key_found);hex(null_key);std::printf(":%s\n",calls.c_str());
    g_parameter_db_get_float_hook.gateway=nullptr;calls.clear();std::string no_float="initial";
    const bool no_float_found=read_odf_snapshot_value(&db,"fraction",no_float);
    std::printf("no-float:%u:%s\n",no_float_found,calls.c_str());
    g_parameter_db_get_float_hook.gateway=reinterpret_cast<void*>(0x2222);
    const std::string large(90,'v');db.entries["large"]={large,0,0,true,false,false,false};
    bool allocation_threw=false;std::string failure_value="old";allocation_budget=0;
    try{(void)read_odf_snapshot_value(&db,"large",failure_value);}catch(const std::bad_alloc&){allocation_threw=true;}
    allocation_budget=-1;std::printf("snapshot-alloc:%u:",allocation_threw);hex(failure_value);std::puts("");

    InitializeCriticalSection(&g_state_lock);g_state_lock_ready=true;
    g_race_odf_fields={"fraction","missing","text"};
    g_race_odf_defaults.emplace("missing",RaceOdfDefaultPolicy{"fallback","owner"});
    g_race_loaded_handlers.push_back(NativeRaceLoadedRegistration{"first",{},&first_handler,nullptr});
    g_race_loaded_handlers.push_back(NativeRaceLoadedRegistration{"empty",{},nullptr,nullptr});
    g_race_loaded_handlers.push_back(NativeRaceLoadedRegistration{"second",{},&second_handler,nullptr});
    calls.clear();mutate_event=true;db.mutate=true;
    a2fo_dispatch_race_odf_loaded(reinterpret_cast<void*>(0x66),&db);
    std::printf("race-mutate:%u:%u:%s\n",handler_calls,handler_two_calls,calls.c_str());
    g_race_odf_fields={"text"};
    g_race_loaded_handlers.clear();
    g_race_loaded_handlers.push_back(NativeRaceLoadedRegistration{"second",{},&second_handler,nullptr});
    calls.clear();allocation_budget=0;
    a2fo_dispatch_race_odf_loaded(reinterpret_cast<void*>(0x66),&db);
    allocation_budget=-1;
    std::printf("race-alloc:%u:%s\n",handler_two_calls,calls.c_str());
    g_race_loaded_handlers.clear();calls.clear();
    a2fo_dispatch_race_odf_loaded(reinterpret_cast<void*>(0x66),&db);
    std::printf("race-empty:%s\n",calls.c_str());
    calls.clear();a2fo_dispatch_race_odf_loaded(nullptr,reinterpret_cast<void*>(1));
    a2fo_dispatch_race_odf_loaded(reinterpret_cast<void*>(1),nullptr);
    g_state_lock_ready=false;a2fo_dispatch_race_odf_loaded(reinterpret_cast<void*>(1),&db);
    std::printf("race-off:%s\n",calls.c_str());
    DeleteCriticalSection(&g_state_lock);
    std::puts("core ODF workflow: PASS");
}
