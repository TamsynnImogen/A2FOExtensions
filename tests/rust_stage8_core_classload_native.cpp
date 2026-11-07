#include <windows.h>
#include <cstdio>
static BOOL WINAPI fixture_write(HANDLE,const void* text,DWORD size,DWORD* written,OVERLAPPED*){
    std::fputs("log:",stdout);std::fwrite(text,1,size,stdout);if(written)*written=size;return TRUE;
}
#define WriteFile fixture_write
#include CORE_SOURCE
#undef WriteFile
#include <stdexcept>
struct Db{std::map<std::string,std::string> values;};
static Db db;
static unsigned mode=0;
static bool getter_found=true,unterminated=false,switch_context=false;
static GameObjectClassOdfLoadContext alternate;
extern "C" bool a2fo_parameter_db_get_string(void*,void* raw,const char* key,char* output,std::uint32_t size,const char*){
    std::printf("get:%s:%u:%u\n",key?key:"<null>",output!=nullptr,size);
    if(mode==8)throw std::runtime_error("getter");
    if(!key)return false;
    const auto& values=static_cast<Db*>(raw)->values;const auto it=values.find(key);
    if(switch_context&&!std::strcmp(key,"alpha"))g_game_object_class_odf_load_context=&alternate;
    if(output&&size){
        if(unterminated)std::memset(output,'X',size);
        else std::snprintf(output,size,"%s",it==values.end()?"":it->second.c_str());
    }
    return getter_found&&it!=values.end();
}
extern "C" bool a2fo_parameter_db_get_scalar(void*,void*,const char*,void*,std::uint32_t){return false;}
extern "C" void* a2fo_call_game_object_method(void* function,void*){
    std::printf("method:%08x\n",static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(function)));
    static char nonterminated[256];std::memset(nonterminated,'Z',sizeof(nonterminated));
    if(mode==2)return nullptr;
    if(mode==3)return nonterminated;
    if(mode==4)return const_cast<char*>("   ");
    return const_cast<char*>(".\\Ships\\Test.ODF");
}
namespace a2fo {void register_nebula_cloak_class_name(void* object_class,const char* name)noexcept{
    std::printf("cloak:%u:%s\n",static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(object_class)),name);
}}
static void A2FO_CALL first_handler(const A2FO_GameObjectClassLoadedEvent* event,void*){
    std::printf("first:%u:%.*s:%u\n",event->struct_size,static_cast<int>(event->source_odf.size),event->source_odf.data,event->odf_field_count);
    for(std::uint32_t i=0;i<event->odf_field_count;++i){const auto& f=event->odf_fields[i];
        std::printf("field:%.*s=%.*s\n",static_cast<int>(f.name.size),f.name.data,static_cast<int>(f.value.size),f.value.data);}
    if(mode==5)g_game_object_class_loaded_handlers.clear();
    if(mode==6)throw std::runtime_error("handler");
    const_cast<A2FO_GameObjectClassLoadedEvent*>(event)->object_class=reinterpret_cast<void*>(9);
}
static void A2FO_CALL last_handler(const A2FO_GameObjectClassLoadedEvent* event,void*){
    std::printf("last:%u\n",static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(event->object_class)));
}
static void* A2FO_CALL original_find(const std::uint32_t* project){
    std::printf("find:%u\n",*project);g_armada=reinterpret_cast<HMODULE>(0x2000000);
    if(mode==9)throw std::runtime_error("original");
    if(mode!=1){
        char output[64]{};const bool found=parameter_db_get_string_hook(&db,nullptr,"classlabel",output,sizeof(output),"");
        std::printf("original-label:%u:%s\n",found,output);
    }
    return mode==0?nullptr:reinterpret_cast<void*>(111);
}
static void dump(const char* name,const OdfSnapshot& fields){
    std::printf("snapshot:%s:%u\n",name,static_cast<unsigned>(fields.size()));
    for(const auto& row:fields)std::printf("snapshot-row:%s=%s\n",row.first.c_str(),row.second.c_str());
}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    InitializeCriticalSection(&g_state_lock);g_log=reinterpret_cast<HANDLE>(1);g_state_lock_ready=true;
    g_deferred_initialization_finished=1;g_parameter_db_get_string_hook.gateway=reinterpret_cast<void*>(0x111);
    g_game_object_class_find_project_id_hook.gateway=reinterpret_cast<void*>(&original_find);
    db.values={{"classlabel","HybridBuild"},{"alpha","one"},{"beta","two"}};
    g_classlabel_aliases["hybridbuild"]={"research","fixture"};
    g_game_object_class_odf_fields={"alpha","basename","beta","missing"};
    GameObjectClassOdfLoadContext previous;
    for(mode=0;mode<10;++mode){
        g_game_object_class_odf_load_context=&previous;g_armada=reinterpret_cast<HMODULE>(0x1000000);g_odf_snapshots.clear();
        g_game_object_class_loaded_handlers={{"first",{},first_handler,nullptr},{"null",{},nullptr,nullptr},{"last",{},last_handler,nullptr}};
        const std::uint32_t project=mode;std::printf("load-case:%u\n",mode);
        try{const auto result=game_object_class_find_project_id_hook(&project);
            std::printf("load-result:%u\n",static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(result)));}
        catch(const std::exception& error){std::printf("caught:%s\n",error.what());}
        std::printf("context-restored:%u\n",g_game_object_class_odf_load_context==&previous);
        for(const auto& row:g_odf_snapshots)dump(row.first.c_str(),row.second);
    }
    mode=10;g_game_object_class_odf_load_context=&previous;
    for(const char* key:{static_cast<const char*>(nullptr),"","alpha","classlabel"})
    for(std::uint32_t size:{0u,1u,4u,64u})for(unsigned found=0;found<2;++found){
        getter_found=found!=0;char output[64]{};std::printf("string-case:%s:%u:%u\n",key?key:"<null>",size,found);
        const bool result=parameter_db_get_string_hook(&db,nullptr,key,output,size,"");
        std::printf("string-result:%u:%s\n",result,output);
    }
    getter_found=true;unterminated=true;char output[64]{};
    std::printf("unterminated:%u\n",parameter_db_get_string_hook(&db,nullptr,"classlabel",output,sizeof(output),""));
    unterminated=false;
    std::printf("null-output:%u\n",parameter_db_get_string_hook(&db,nullptr,"classlabel",nullptr,0,""));
    previous.fields.clear();alternate.fields.clear();previous.parameter_db_observed=false;switch_context=true;
    g_game_object_class_odf_load_context=&previous;capture_game_object_class_odf_fields(&db);switch_context=false;
    dump("previous",previous.fields);dump("alternate",alternate.fields);
    g_game_object_class_odf_load_context=nullptr;capture_game_object_class_odf_fields(&db);capture_game_object_class_odf_fields(nullptr);
    DeleteCriticalSection(&g_state_lock);std::puts("core classload fixture: PASS");return 0;
}
