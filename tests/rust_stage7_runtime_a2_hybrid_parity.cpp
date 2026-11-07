#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "../modules/A2FOFeaturePack/hybrid_bridge_api.hpp"
#ifdef BUILD_PROVIDER
static bool bridge_reject=false, producer_ready=true;
extern "C" __declspec(dllexport) bool __cdecl A2FO_RegisterHybridBridge(
    const A2FO_HybridBridge* bridge){return !bridge_reject && bridge &&
        bridge->version==A2FO_HYBRID_BRIDGE_VERSION && bridge->cleanup_construction;}
extern "C" __declspec(dllexport) bool __cdecl A2FO_ProducerEventsReady(){return producer_ready;}
extern "C" __declspec(dllexport) void __cdecl A2FO_TestBridgeReject(bool value){bridge_reject=value;}
extern "C" __declspec(dllexport) void __cdecl A2FO_TestProducerReady(bool value){producer_ready=value;}
#else
#include MODULE_SOURCE
namespace {
std::vector<std::string> trace;
std::string roots[2];
std::uint32_t root_total=2;
bool alias_ok=true,cocoon_ok=true,runtime_ok=true,parser_ok=true,submenus_ok=true;
A2FO_GameObjectClassLoadedHandler class_callback=nullptr;
void A2FO_CALL logger(const char*,const char* text){trace.emplace_back(std::string("log:")+text);}
void* A2FO_CALL armada_module(){return reinterpret_cast<void*>(1);}
void* A2FO_CALL fleetops_module(){return reinterpret_cast<void*>(2);}
std::uint32_t A2FO_CALL root_count(){return root_total;}
const char* A2FO_CALL root_at(std::uint32_t index){return index<root_total?roots[index].c_str():nullptr;}
bool A2FO_CALL alias(const char*,const char*,const char*){
    trace.emplace_back("alias");return alias_ok;
}
bool A2FO_CALL cocoon(const char*,const char*){
    trace.emplace_back("cocoon");return cocoon_ok;
}
bool A2FO_CALL register_class(const char*,const char* const*,std::uint32_t,
                              A2FO_GameObjectClassLoadedHandler cb,void*){
    trace.emplace_back("register-class");class_callback=cb;return parser_ok;
}
void write_file(const std::string& path,const char* value){std::ofstream out(path,std::ios::binary);out<<value;}
void loaded(const char* label,const char* source,bool valid=true,bool object=true){
    A2FO_GameObjectClassLoadedEvent event{};
    event.struct_size=valid?sizeof(event):0;
    event.object_class=object?reinterpret_cast<void*>(0x1111):nullptr;
    event.source_odf={source,source?static_cast<std::uint32_t>(std::strlen(source)):0};
    const auto before=trace.size();
    class_callback(&event,nullptr);
    trace.emplace_back(std::string("loaded:")+label+":"+
        std::to_string(trace.size()-before));
}
void short_loaded() {
    SYSTEM_INFO system{};GetSystemInfo(&system);
    const std::size_t page=system.dwPageSize;
    auto* guarded=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,2*page,
        MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!guarded)throw std::runtime_error("short event allocation failed");
    DWORD previous=0;
    if(!VirtualProtect(guarded+page,page,PAGE_NOACCESS,&previous))
        throw std::runtime_error("short event protection failed");
    const std::uint32_t size=0;
    std::memcpy(guarded+page-sizeof(size),&size,sizeof(size));
    const auto before=trace.size();
    class_callback(reinterpret_cast<const A2FO_GameObjectClassLoadedEvent*>(
        guarded+page-sizeof(size)),nullptr);
    trace.emplace_back("loaded:short-guarded:"+std::to_string(trace.size()-before));
    VirtualFree(guarded,0,MEM_RELEASE);
}
void invoke(const char* label,const A2FO_ModuleApi* api){
    trace.emplace_back(std::string(label)+":"+(A2FO_ModuleInit(api)?"1":"0")+":"+
        std::to_string(g_odf_paths.size()));
}
void run(){
    char temp[MAX_PATH]{};GetTempPathA(MAX_PATH,temp);
    const std::string base=std::string(temp)+"a2fo-hybrid-base-"+
        std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64());
    const std::string active=base+"-active";
    CreateDirectoryA(base.c_str(),nullptr);CreateDirectoryA(active.c_str(),nullptr);
    CreateDirectoryA((base+"\\odf").c_str(),nullptr);
    CreateDirectoryA((active+"\\odf").c_str(),nullptr);
    CreateDirectoryA((active+"\\odf\\nested").c_str(),nullptr);
    write_file(base+"\\odf\\hbuild.odf","buildItem0Refit0 = base\n");
    write_file(active+"\\odf\\hbuild.odf","buildItem0Refit0 = active\n");
    write_file(active+"\\odf\\nested\\other.odf","buildItem0Refit0 = nested\n");
    write_file(active+"\\odf\\broken.odf","buildItem57Refit0 = invalid\n");
    roots[0]=base;roots[1]=active;
    A2FO_ModuleApi api{};api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
    api.log=&logger;api.armada_module=&armada_module;api.fleetops_module=&fleetops_module;
    api.extension_root_count=&root_count;api.extension_root=&root_at;
    api.register_classlabel_alias=&alias;api.register_evolver_cocoon_command=&cocoon;
    api.capabilities=A2FO_CAP_GAME_OBJECT_CLASS_LOADED;
    api.register_game_object_class_loaded_handler=&register_class;
    invoke("null",nullptr);
    alias_ok=false;invoke("alias-fail",&api);alias_ok=true;
    invoke("no-provider",&api);
    char exe[MAX_PATH]{};GetModuleFileNameA(nullptr,exe,MAX_PATH);
    std::string folder(exe);folder.erase(folder.find_last_of("\\/"));
    HMODULE provider=LoadLibraryA((folder+"\\A2FOFeaturePack.dll").c_str());
    if(!provider){std::fprintf(stderr,"provider load failed\n");return;}
    auto set_reject=reinterpret_cast<void(__cdecl*)(bool)>(GetProcAddress(provider,"A2FO_TestBridgeReject"));
    auto set_ready=reinterpret_cast<void(__cdecl*)(bool)>(GetProcAddress(provider,"A2FO_TestProducerReady"));
    set_ready(false);invoke("producer-not-ready",&api);set_ready(true);
    set_reject(true);invoke("bridge-reject",&api);set_reject(false);
    invoke("success",&api);
    trace.emplace_back("index:"+std::to_string(g_odf_paths.size())+":"+
        std::to_string(g_odf_paths.at("hbuild.odf").find("active")!=std::string::npos));
    loaded("overridden","RACE/HBUILD");
    loaded("nested","nested\\other.odf");
    loaded("broken","broken.odf");
    loaded("missing","missing.odf");
    loaded("invalid-size","hbuild.odf",false);
    short_loaded();
    loaded("null-class","hbuild.odf",true,false);
    submenus_ok=false;loaded("rejected","hbuild.odf");submenus_ok=true;
    parser_ok=false;invoke("parser-reject",&api);parser_ok=true;
    api.capabilities=0;invoke("parser-unavailable",&api);
    api.capabilities=A2FO_CAP_GAME_OBJECT_CLASS_LOADED;
    runtime_ok=false;invoke("runtime-disabled",&api);runtime_ok=true;
    FreeLibrary(provider);
    DeleteFileA((base+"\\odf\\hbuild.odf").c_str());
    DeleteFileA((active+"\\odf\\hbuild.odf").c_str());
    DeleteFileA((active+"\\odf\\broken.odf").c_str());
    DeleteFileA((active+"\\odf\\nested\\other.odf").c_str());
    RemoveDirectoryA((active+"\\odf\\nested").c_str());
    RemoveDirectoryA((base+"\\odf").c_str());RemoveDirectoryA((active+"\\odf").c_str());
    RemoveDirectoryA(base.c_str());RemoveDirectoryA(active.c_str());
}
}
namespace a2fo {
bool initialize_hybrid_production_registry(const A2FO_ModuleApi*,HMODULE,HMODULE) noexcept {
    trace.emplace_back("runtime");return runtime_ok;
}
bool register_refit_ui_bridge(const A2FO_RefitUiBridge*) noexcept {return true;}
bool register_live_build_submenus(void*,const build_submenu::Config& config,
                                  const std::string& source) noexcept {
    trace.emplace_back("submenus:"+source+":"+config.pages[0].children[0]);
    return submenus_ok;
}
bool hybrid_production_has_queued_research_conflict(void*,void*) noexcept{return false;}
bool hybrid_production_is_evolve_target(void*,void*) noexcept{return false;}
bool hybrid_production_has_evolution_barrier(void*) noexcept{return false;}
bool hybrid_production_should_defer_construct_order(void*,void*) noexcept{return false;}
void finalize_hybrid_construct_order(void*,void*,bool) noexcept{}
void discard_hybrid_construct_placement(void*,std::uint32_t) noexcept{}
void clear_hybrid_construct_placements(void*) noexcept{}
void retain_hybrid_research_menu_after_order(void*,void*) noexcept{}
bool register_research_station_hybrid_lists(void*,void*) noexcept{return true;}
std::size_t publish_research_station_hybrid_items(void*) noexcept{return 0;}
void cleanup_hybrid_cocoon(void*) noexcept{}
void cleanup_hybrid_construction(void*) noexcept{}
}
int main(){run();for(const auto& line:trace)std::printf("%s\n",line.c_str());}
#endif
