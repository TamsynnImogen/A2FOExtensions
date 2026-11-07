// Production adapters with synthetic inputs. No engine entrypoint, UI or input.
#include "../core/builtin_modules.hpp"
#include "../core/builtin_startup_modules.hpp"
#include "../core/module_policy.hpp"
#include "../modules/A2FODirectionalShields/api.hpp"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#define DECLARE(name) bool A2FO_CALL A2FOBuiltin##name##_Init(const A2FO_ModuleApi*);
DECLARE(Cheats) DECLARE(CollectiveXP) DECLARE(DirectionalShields)
DECLARE(EnergySystems) DECLARE(HybridBuild) DECLARE(MultiYardBuild)
DECLARE(NebulaRenderer) DECLARE(PointDefenseCycles) DECLARE(RefitYards)
DECLARE(Turrets) DECLARE(RGBTextures) DECLARE(TextureVariants)
#undef DECLARE
float A2FO_CALL A2FOEnergySystems_GetPhotonTorpedoes(void*);
float A2FO_CALL A2FOEnergySystems_GetQuantumTorpedoes(void*);
float A2FO_CALL A2FOEnergySystems_GetShuttleCraft(void*);
float A2FO_CALL A2FOEnergySystems_GetMaximumPhotonTorpedoes(void*);
float A2FO_CALL A2FOEnergySystems_GetMaximumQuantumTorpedoes(void*);
float A2FO_CALL A2FOEnergySystems_GetMaximumShuttleCraft(void*);
float A2FO_CALL A2FOEnergySystems_GetPhotonTorpedoReloadSeconds(void*);
float A2FO_CALL A2FOEnergySystems_GetQuantumTorpedoReloadSeconds(void*);
float A2FO_CALL A2FOEnergySystems_GetShuttleCraftReloadSeconds(void*);
void A2FO_CALL A2FOEnergySystems_SetPhotonTorpedoes(void*,float);
void A2FO_CALL A2FOEnergySystems_SetQuantumTorpedoes(void*,float);
void A2FO_CALL A2FOEnergySystems_SetShuttleCraft(void*,float);
void A2FO_CALL A2FOEnergySystems_AddPhotonTorpedoes(void*,float);
void A2FO_CALL A2FOEnergySystems_AddQuantumTorpedoes(void*,float);
void A2FO_CALL A2FOEnergySystems_AddShuttleCraft(void*,float);
void* __cdecl A2FOCollectiveXP_ResolveBuildClass(void*,void*) noexcept;
bool __cdecl A2FOCollectiveXP_GetProgress(unsigned,unsigned*,double*,double*) noexcept;
bool __cdecl A2FOCollectiveXP_IsEnabledForObject(void*) noexcept;
bool A2FO_CALL A2FODirectionalShields_ConnectDamageBridge();
bool A2FO_CALL A2FODirectionalShields_IsEnabled(void*);
float A2FO_CALL A2FODirectionalShields_GetCurrent(void*,std::uint32_t);
float A2FO_CALL A2FODirectionalShields_GetMaximum(void*,std::uint32_t);
void A2FO_CALL A2FOTextureVariants_RegisterClass(void*,void*);
void A2FO_CALL A2FONebulaRenderer_RegisterClass(void*,void*);
}
namespace {
void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
const char* names[]{"A2FOCheats","A2FOCollectiveXP","A2FODirectionalShields",
"A2FOEnergySystems","A2FOHybridBuild","A2FOMultiYardBuild","A2FONebulaRenderer",
"A2FOPointDefenseCycles","A2FORefitYards","A2FOTurrets","A2FORGBTextures","A2FOTextureVariants"};
A2FO_ModuleInitFn initializers[]{A2FOBuiltinCheats_Init,A2FOBuiltinCollectiveXP_Init,
 A2FOBuiltinDirectionalShields_Init,A2FOBuiltinEnergySystems_Init,A2FOBuiltinHybridBuild_Init,
 A2FOBuiltinMultiYardBuild_Init,A2FOBuiltinNebulaRenderer_Init,A2FOBuiltinPointDefenseCycles_Init,
 A2FOBuiltinRefitYards_Init,A2FOBuiltinTurrets_Init,A2FOBuiltinRGBTextures_Init,A2FOBuiltinTextureVariants_Init};
void run() {
 using namespace a2fo;
 char early_temp[MAX_PATH]{},early_root[MAX_PATH]{};
 check(GetTempPathA(sizeof(early_temp),early_temp)&&GetTempFileNameA(early_temp,"b9e",0,early_root),"early temp root");
 check(DeleteFileA(early_root)&&CreateDirectoryA(early_root,nullptr),"early root");
 const auto selected=[&](){return startup_builtin_selected(BuiltinFeature::nebula_renderer,early_root,"");};
 const std::string info=std::string(early_root)+"\\info.ini";
 const auto configure=[&](const char* text){
  HANDLE f=CreateFileA(info.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);
  check(f!=INVALID_HANDLE_VALUE,"early policy file");DWORD written{};
  check(WriteFile(f,text,std::strlen(text),&written,nullptr)&&written==std::strlen(text),"early policy write");CloseHandle(f);
 };
 check(selected(),"early legacy policy includes builtin without standalone DLL");
 configure("[modules]\n");check(!selected(),"early managed off blocks renderer");
 configure("[modules]\nactive1=A2FONebulaRenderer\n");check(selected(),"early active allows renderer");
 configure("[modules]\nrequired1=A2FONebulaRenderer\n");check(selected(),"early required allows renderer");
 configure("[modules]\nrequired1=A2FONebulaRenderer\nreject1=A2FONebulaRenderer\n");check(!selected(),"early conflict blocks renderer");
 configure("[modules]\nreject1=A2FONebulaRenderer\n");check(!selected(),"early rejected blocks renderer");
 A2FO_ModuleApi invalid{};
 const std::vector<std::string> roots{"C:/Fixture"}; set_builtin_module_roots(roots);
 for(unsigned i=0;i<12;++i) {
  BuiltinFeature feature;
  check(find_builtin_feature(names[i],feature),"identity");
  check(builtin_requires_restart(feature),"startup classification");
  check(!initializers[i](nullptr) && !initializers[i](&invalid),"real initializer rejects invalid API");
  check(prepare_builtin_module(feature,invalid,false),"inactive preparation succeeds without native init");
 }
 finish_builtin_module_preparation(true,true,true,true,true,true,true,true);
 configure("[modules]\nactive1=A2FONebulaRenderer\n");
 check(!selected(),"startup off snapshot wins over later saved on policy");
 DeleteFileA(info.c_str());RemoveDirectoryA(early_root);
 ModulePolicy on;on.managed=true;
 on.entries.resize(12);
 for(unsigned i=0;i<12;++i) {on.entries[i].name=names[i];on.entries[i].state=ModulePolicyState::active;}
 std::string error;check(apply_builtin_module_policy(roots,on,error),"save pending selections");
 for(const char* name:names) {BuiltinFeature feature;find_builtin_feature(name,feature);
  check(!builtin_module_enabled(feature),"pending on stays inert");
  check(builtin_module_status(name,roots)=="Restart to turn on","pending status");}
 // Poison pointers ensure inactive bridge functions return before touching game data.
 void* poison=reinterpret_cast<void*>(1);void* base=reinterpret_cast<void*>(2);
 for(auto fn:{A2FOEnergySystems_GetPhotonTorpedoes,A2FOEnergySystems_GetQuantumTorpedoes,
 A2FOEnergySystems_GetShuttleCraft,A2FOEnergySystems_GetMaximumPhotonTorpedoes,
 A2FOEnergySystems_GetMaximumQuantumTorpedoes,A2FOEnergySystems_GetMaximumShuttleCraft})
  check(fn(poison)==0.0f,"inactive amount export");
 for(auto fn:{A2FOEnergySystems_GetPhotonTorpedoReloadSeconds,A2FOEnergySystems_GetQuantumTorpedoReloadSeconds,
 A2FOEnergySystems_GetShuttleCraftReloadSeconds})check(fn(poison)==-1.0f,"inactive reload export");
 for(auto fn:{A2FOEnergySystems_SetPhotonTorpedoes,A2FOEnergySystems_SetQuantumTorpedoes,
 A2FOEnergySystems_SetShuttleCraft,A2FOEnergySystems_AddPhotonTorpedoes,
 A2FOEnergySystems_AddQuantumTorpedoes,A2FOEnergySystems_AddShuttleCraft})fn(poison,1.0f);
 check(!A2FOCollectiveXP_IsEnabledForObject(poison) &&
 A2FOCollectiveXP_ResolveBuildClass(poison,base)==base &&
 !A2FOCollectiveXP_GetProgress(0,nullptr,nullptr,nullptr),"inactive XP bridge");
 check(!A2FODirectionalShields_ConnectDamageBridge() && !A2FODirectionalShields_IsEnabled(poison) &&
 A2FODirectionalShields_GetCurrent(poison,0)==0.0f &&
 A2FODirectionalShields_GetMaximum(poison,0)==0.0f,"inactive directional bridge");
 A2FOTextureVariants_RegisterClass(poison,poison); A2FONebulaRenderer_RegisterClass(poison,poison);
 // Inventory always supplies 36 builtins, even without module files.
 char temp[MAX_PATH]{},root[MAX_PATH]{};
 check(GetTempPathA(sizeof(temp),temp)&&GetTempFileNameA(temp,"b9t",0,root),"temp root");
 check(DeleteFileA(root)&&CreateDirectoryA(root,nullptr),"create root");
 const std::string directory=std::string(root)+"\\modules";check(CreateDirectoryA(directory.c_str(),nullptr),"create modules");
 for(const char* name:names) {const std::string path=directory+"\\"+name+".dll";
  HANDLE f=CreateFileA(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,0,nullptr);
  check(f!=INVALID_HANDLE_VALUE,"stale DLL fixture");CloseHandle(f);}
 std::vector<std::string> diagnostics;
 auto inventory=discover_installed_modules(root,&diagnostics);
 check(inventory.size()==static_cast<unsigned>(BuiltinFeature::count)&&diagnostics.size()==12,"deduplicate12stale DLLs");
 ModuleRules required,rejected;required.has_section=rejected.has_section=true;
 required.required={names[0]};rejected.rejected={names[0]};
 check(!evaluate_module_rules({required,rejected},inventory).valid,"required/reject conflict");
 for(const char* name:names) DeleteFileA((directory+"\\"+name+".dll").c_str());
 RemoveDirectoryA(directory.c_str());RemoveDirectoryA(root);
}
}
int main(){try{run();std::puts("batch9 production admission, inactive bridges, policy and dedup: PASS");return 0;}
catch(const std::exception& e){std::fprintf(stderr,"batch9: %s\n",e.what());return 1;}}
