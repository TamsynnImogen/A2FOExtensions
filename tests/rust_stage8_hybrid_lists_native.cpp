#define main a2fo_hybrid_runtime_unused_main
#include "rust_stage8_hybrid_runtime_native.cpp"
#undef main
#include <map>
struct Entry{std::string text;std::uint32_t project=0;std::uintptr_t found=1;};
struct Db{std::map<std::string,Entry> entries;};
static int throw_read=-1;
extern "C" std::uintptr_t a2fo_call_thiscall_4(void*,void* db,std::uintptr_t key,std::uintptr_t output,std::uintptr_t size,std::uintptr_t){
    const auto* name=reinterpret_cast<const char*>(key);std::printf("get:%s\n",name);
    if(throw_read==0){throw_read=-1;throw std::runtime_error("read");}if(throw_read>0)--throw_read;
    const auto& rows=static_cast<Db*>(db)->entries;const auto it=rows.find(name);
    const auto* text=it==rows.end()?"":it->second.text.c_str();
    std::snprintf(reinterpret_cast<char*>(output),size,"%s",text);
    return it==rows.end()?0:it->second.found;
}
extern "C" std::uintptr_t a2fo_call_thiscall_3(void*,void* db,std::uintptr_t key,std::uintptr_t output,std::uintptr_t){
    const auto* name=reinterpret_cast<const char*>(key);std::printf("project:%s\n",name);
    const auto& rows=static_cast<Db*>(db)->entries;const auto it=rows.find(name);
    if(it==rows.end())return 0;
    *reinterpret_cast<std::uint32_t*>(output)=it->second.project;return it->second.found;
}
static const char* identity="";
static bool identity_found=false;
static bool A2FO_CALL original_label(void* db,char* output,std::uint32_t size){
    std::printf("identity:%u:%u\n",db!=nullptr,size);std::snprintf(output,size,"%s",identity);return identity_found;
}
static void dump(){
    const auto it=a2fo::g_class_lists.find(reinterpret_cast<void*>(1));
    if(it==a2fo::g_class_lists.end()){std::puts("lists:none");return;}
    std::printf("lists:%u\n",static_cast<unsigned>(it->second->entries().size()));
    for(const auto& row:it->second->entries())std::printf("entry:%u:%u:%u\n",static_cast<unsigned>(row.method),static_cast<unsigned>(row.index),row.project_id);
}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    InitializeCriticalSection(&a2fo::g_registry_lock);a2fo::g_registry_lock_ready=true;
    A2FO_ModuleApi api{};api.log=fixture_log;api.get_original_classlabel=original_label;
    for(unsigned gate=0;gate<6;++gate)for(const char* name:{"","HybridBuild","research"}){
        api.struct_size=gate==1?0:sizeof(api);api.capabilities=gate==2?0u:static_cast<std::uint32_t>(A2FO_CAP_ORIGINAL_CLASSLABEL);
        api.get_original_classlabel=gate==3?nullptr:original_label;a2fo::g_api=gate==0?nullptr:&api;
        identity=name;identity_found=gate!=4;a2fo::g_logged_missing_classlabel_identity_api=false;a2fo::g_logged_hybridbuild_identity=false;
        const bool result=a2fo::is_hybridbuild_parameter_db(reinterpret_cast<void*>(1));
        std::printf("identity-result:%u:%s:%u\n",gate,name,result);
        a2fo::is_hybridbuild_parameter_db(reinterpret_cast<void*>(1));
        a2fo::is_hybridbuild_parameter_db(nullptr);
    }
    a2fo::g_api=&api;a2fo::g_armada=reinterpret_cast<HMODULE>(0x1000000);
    for(unsigned mode=0;mode<5;++mode){
        Db db;
        if(mode==1)db.entries={{"constructItem1",{"no marker",8,1}}};
        if(mode==2)db.entries={{"constructItem0",{"empty list",0,1}},{"yardItem0",{"",7,1}}};
        if(mode>=3)db.entries={{"constructItem0",{"first",1,1}},{"constructItem2",{"",4,1}},
            {"constructItem3",{"bad id",0,1}},{"yardItem0",{"ambiguous",1,1}},
            {"researchItem1",{"research",2,1}},{"evolveItem56",{"terminal",3,1}},
            {"yardItem4",{"low byte false",9,0x100}}};
        a2fo::g_class_lists.clear();std::printf("list-case:%u\n",mode);
        a2fo::load_explicit_lists(nullptr,&db);a2fo::load_explicit_lists(reinterpret_cast<void*>(1),nullptr);
        std::printf("marker:%u\n",a2fo::has_explicit_list_marker(&db));
        a2fo::load_explicit_lists(reinterpret_cast<void*>(1),&db);dump();
        if(mode==4){
            db.entries["constructItem0"]={"replacement",99,1};a2fo::load_explicit_lists(reinterpret_cast<void*>(1),&db);dump();
            throw_read=2;try{a2fo::load_explicit_lists(reinterpret_cast<void*>(1),&db);}
            catch(const std::exception& error){std::printf("caught:%s\n",error.what());}dump();
        }
    }
    a2fo::g_class_lists.clear();DeleteCriticalSection(&a2fo::g_registry_lock);
    std::puts("hybrid lists fixture: PASS");
}
