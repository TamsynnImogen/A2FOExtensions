// Production adapters with synthetic data only. No engine, UI or input runs.
#include "../core/builtin_modules.hpp"
#include "../core/module_loader.hpp"
#include "../modules/A2FOResources/api.hpp"
#include "../modules/A2FOSquadrons/api.hpp"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace a2fo;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class Fn> Fn symbol(const char* name){
 auto address=GetProcAddress(GetModuleHandleA(nullptr),name);Fn fn=nullptr;
 static_assert(sizeof(fn)==sizeof(address));std::memcpy(&fn,&address,sizeof(fn));
 check(fn!=nullptr,name);return fn;
}
struct Module{const char* canonical;const char* init;};
const Module added[]{
 {"A1Compat","A2FOBuiltinA1Compat_Init"},{"A2FOCraftIdentity","A2FOBuiltinCraftIdentity_Init"},
 {"A2FOFeaturePack","A2FOBuiltinFeaturePack_Init"},{"A2FOLogMailer","A2FOBuiltinLogMailer_Init"},
 {"A2FOMissionSelector","A2FOBuiltinMissionSelector_Init"},{"A2FOMissions","A2FOBuiltinMissions_Init"},
 {"A2FORefitMenus","A2FOBuiltinRefitMenus_Init"},{"A2FOResources","A2FOBuiltinResources_Init"},
 {"A2FOSOLMaps","A2FOBuiltinSOLMaps_Init"},{"A2FOShellDisplay","A2FOBuiltinShellDisplay_Init"},
 {"A2FOSquadrons","A2FOBuiltinSquadrons_Init"},{"A2FOVideo","A2FOBuiltinVideo_Init"}};
void run(){
 A2FO_ModuleApi invalid{};set_builtin_module_roots({"C:/PrivateFixture"});
 auto ready=symbol<bool(A2FO_CALL*)(const char*)>("A2FOBuiltinModule_StartupReady");
 for(const auto& module:added){
  BuiltinFeature feature;check(find_builtin_feature(module.canonical,feature),"canonical identity");
  auto init=symbol<A2FO_ModuleInitFn>(module.init);
  check(!init(nullptr)&&!init(&invalid),"actual adapter rejects invalid API");
  check(prepare_builtin_module(feature,invalid,false),"off skips all feature initialization");
  check(!ready(module.canonical),"off public provider readiness");
 }
 finish_builtin_module_preparation(true,true,true,true,true,true,true,true);
 ModulePolicy on;on.managed=true;
 for(const auto& module:added){ModulePolicyEntry row;row.name=module.canonical;row.state=ModulePolicyState::active;on.entries.push_back(row);}
 std::string error;check(apply_builtin_module_policy({"C:/PrivateFixture"},on,error),"save pending selections");
 for(const auto& module:added){BuiltinFeature feature;find_builtin_feature(module.canonical,feature);
  check(!builtin_module_enabled(feature)&&!ready(module.canonical),"saved on remains inactive");
  check(builtin_module_status(module.canonical,{"C:/PrivateFixture"})=="Restart to turn on","pending restart");}
 void* poison=reinterpret_cast<void*>(1);
 check(symbol<A2FOResourcesGetFn>("A2FOResources_Get")(poison,6)==0,"inactive resource getter");
 check(!symbol<A2FOResourcesSetFn>("A2FOResources_Set")(poison,6,5),"inactive resource setter");
 check(!symbol<A2FOResourcesAddFn>("A2FOResources_Add")(poison,6,5),"inactive resource add");
 check(symbol<A2FOResourcesGetCostFn>("A2FOResources_GetCost")(poison,6)==0,"inactive cost getter");
 check(std::strcmp(symbol<A2FOResourcesGetPresentationTextFn>("A2FOResources_GetPresentationText")(poison,6,0),"")==0,"inactive presentation");
 std::int32_t costs[4]{12,13,14,15};
 check(!symbol<A2FOSquadronsGetAdditionalCostsFn>("A2FOSquadrons_GetAdditionalCosts")(poison,costs,4)&&costs[0]==12,"inactive squad costs");
 using Script=int(__cdecl*)(void*);
 check(symbol<Script>("A2FOMissions_ScriptExtendInterface")(poison)==51&&symbol<Script>("A2FOMissions_ScriptInit")(poison)==0,"inactive MMM callbacks");
 symbol<void(__cdecl*)()>("A2FOMissions_MarkMissionUnloaded")();
 check(!symbol<bool(A2FO_CALL*)()>("A2FO_ProducerEventsReady")(),"inactive actual Producer dispatch");
 check(!producer_dispatch_available({{nullptr,nullptr,"builtin:A2FOFeaturePack"}}),"core presence cannot supply Producer events");
 using Load=std::uintptr_t(__cdecl*)(void*);
 for(const char* name:{"A2FOSOLMaps_LoadGameObjects","A2FOSOLMaps_LoadA2CraftClassTable","A2FOSOLMaps_LoadGameMain","A2FOSOLMaps_LoadAiMission","A2FOSOLMaps_LoadPostMissionA","A2FOSOLMaps_LoadPostMissionB"})
  check(symbol<Load>(name)(poison)==0,"inactive SOL loader cannot call native code");
 check(symbol<int(__cdecl*)()>("A2FOSOLMaps_IsSelected")()==0,"no inactive SOL selection");
 char temp[MAX_PATH]{},root[MAX_PATH]{};
 check(GetTempPathA(sizeof(temp),temp)&&GetTempFileNameA(temp,"b10",0,root),"temp path");
 check(DeleteFileA(root)&&CreateDirectoryA(root,nullptr),"private root");
 const std::string directory=std::string(root)+"\\modules";check(CreateDirectoryA(directory.c_str(),nullptr),"private modules");
 for(const auto& module:added){auto path=directory+"\\"+module.canonical+".dll";HANDLE f=CreateFileA(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,0,nullptr);check(f!=INVALID_HANDLE_VALUE,"stale DLL fixture");CloseHandle(f);}
 std::vector<std::string> diagnostics;auto inventory=discover_installed_modules(root,&diagnostics);
 check(inventory.size()==48&&diagnostics.size()==12,"all48 providers,12 stale DLLs ignored");
 for(const auto& module:added)DeleteFileA((directory+"\\"+module.canonical+".dll").c_str());
 RemoveDirectoryA(directory.c_str());RemoveDirectoryA(root);
}
}
int main(){try{run();std::puts("final12 production adapters, inert exports, inactive loaders and inventory: PASS");return 0;}catch(const std::exception& e){std::fprintf(stderr,"final12: %s\n",e.what());return 1;}}
