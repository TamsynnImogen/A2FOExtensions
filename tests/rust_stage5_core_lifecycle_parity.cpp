#include "extension_roots.hpp"
#include "native_reference/adapters/core/hook.hpp"
#include "module_loader.hpp"
#include "module_policy.hpp"
#include "builtin_modules.hpp"
#include "builtin_startup_modules.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cctype>
extern "C" bool A2FO_CALL A2FOBuiltinModule_StartupReady(const char*) noexcept;
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
namespace {
std::set<std::string> directories;
std::vector<std::string> trace;
std::string fail;
std::array<std::uint8_t,256> gateway{};
std::array<std::uint8_t,32> target{};
std::array<std::uint8_t,32> replacement{};
std::string hex(const std::string& s){std::ostringstream out;out<<std::hex<<std::setfill('0');for(unsigned char c:s)out<<std::setw(2)<<unsigned(c);return out.str();}
std::string hex(const std::uint8_t*p,std::size_t n){return hex(std::string(reinterpret_cast<const char*>(p),n));}
void line(const std::string& label,const std::string& value){std::cout<<label<<'|'<<hex(value);for(const auto&t:trace)std::cout<<'|'<<t;std::cout<<'\n';trace.clear();}
[[maybe_unused]] void discovered(const char* label,const a2fo::ExtensionRootDiscovery&d){
 std::string value="active="+hex(d.active_mod);for(auto&s:d.roots)value+=";root="+hex(s);
 for(auto&s:d.diagnostics)value+=";diag="+hex(s);
 line(label,value);
}
[[maybe_unused]] void make_info(const std::string& path,const std::string& text){std::ofstream f(path,std::ios::binary);f<<text;}
[[maybe_unused]] void roots_test(){using namespace a2fo;
 for(auto input:{"","fleet.exe /mod Alpha","fleet.exe -MoD=\\\"Beta Mod\\\"", "x /model Foo /mod='gamma'", "x /mod..","x /mod=../evil"})
   line(std::string("active:")+hex(input),active_mod_from_command_line(input));
 for(auto input:{"[mod]\nParentMod=Alpha\n","[MOD]\r\n ParentMod = '  Beta ' \r\n", "[x]\nParentMod=Ignore\n[mod]\nParentMod=Good\n", "[mod]\nParentMod=foo\\bar\n"}){
  line(std::string("parent:")+hex(input),parent_mod_from_info(input));
  auto d=fleet_ops_defaults_from_info(input);line("defaults:"+hex(input),d.settings_directory+":"+std::to_string(d.default_game_speed)+":"+std::to_string(d.has_default_game_speed));
 }
 for(auto input:{"[mod]\nSettingsDirectory=Shared\nDefaultGameSpeed=1\n","[mod]\nDefaultGameSpeed=6\n","[mod]\nDefaultGameSpeed=7\n","[mod]\nDefaultGameSpeed=0\n","[mod]\nDefaultGameSpeed=2x\n","[mod]\nDefaultGameSpeed=999999999999999999999\n","[mod]\nDefaultGameSpeed=+3\n"}){
  auto d=fleet_ops_defaults_from_info(input);line("speed:"+hex(input),d.settings_directory+":"+std::to_string(d.default_game_speed)+":"+std::to_string(d.has_default_game_speed));
 }
 const std::string normal="C:\\Games\\Data\\Mods\\Active\\";
 for(auto configured:{"","Options","C:/Other/Settings","relative/path","D:/Override","\\\\server\\share\\x","../legacy"})
   line(std::string("settings:")+hex(configured),resolve_fleet_ops_settings_directory(normal,configured,"C:\\Fallback\\","C:\\Shared\\"));
 for(auto name:{"Alpha"," A ","",".","..","a/b","a\\b","C:x","a?b", "a\x01b"})
   line(std::string("safe:")+hex(name),safe_mod_directory_name(name)?"1":"0");
 #if defined(_WIN32)
 const std::string data="build/rust-migration-stage5/core-work/Data";
 discovered("discover-none",discover_extension_roots(data,"fleet.exe"));
 discovered("discover-unsafe",discover_extension_roots(data,"fleet.exe /mod ../bad"));
 discovered("discover-missing",discover_extension_roots(data,"fleet.exe /mod Missing"));
 const std::string a=data+"\\Mods\\Alpha",b=data+"\\Mods\\Beta",c=data+"\\Mods\\Gamma";
 directories={a,b,c};make_info(a+"\\info.ini","[mod]\nParentMod=Beta\n");
 make_info(b+"\\info.ini","[mod]\nParentMod=Gamma\n");
 make_info(c+"\\info.ini","[mod]\nParentMod=Alpha\n");
 discovered("discover-chain-cycle",discover_extension_roots(data,"fleet.exe /mod Alpha"));
 make_info(c+"\\info.ini","[mod]\nParentMod=../unsafe\n");
 discovered("discover-unsafe-parent",discover_extension_roots(data,"fleet.exe /mod Alpha"));
 #endif
}
[[maybe_unused]] void hook_row(const char*label,bool result,std::size_t n){
 auto copy=target;if(n>=5&&(copy[0]==0xe9||copy[0]==0xe8))std::fill(copy.begin()+1,copy.begin()+5,0);
 std::string value=std::to_string(result)+":"+hex(copy.data(),n)+":"+hex(gateway.data(),n);
 if(n>=5&&(target[0]==0xe9||target[0]==0xe8)){
  std::int32_t rel=0;std::memcpy(&rel,target.data()+1,4);
  value+=":branch="+std::to_string(target.data()+5+rel==replacement.data());
 }
 if(gateway[8]==0xe9){
  std::int32_t rel=0;std::memcpy(&rel,gateway.data()+9,4);
  value+=":return="+std::to_string(gateway.data()+13+rel==target.data()+8);
 }
 line(label,value);
}
[[maybe_unused]] void hook_test(){using namespace a2fo;
 target.fill(0xcc);replacement.fill(0xdd);gateway.fill(0);
 const std::uint8_t expected[8]={0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc};
 const std::uint8_t bytes[8]={1,2,3,4,5,6,7,8};
 hook_row("patch-bytes",patch_bytes(target.data(),bytes,expected,8),8);
 hook_row("patch-bytes-mismatch",patch_bytes(target.data(),bytes,expected,8),8);
 target.fill(0xcc);fail="protect";hook_row("patch-protect-fail",patch_bytes(target.data(),bytes,expected,8),8);fail.clear();
 target.fill(0xcc);hook_row("patch-jump",patch_jump(target.data(),replacement.data(),expected,8),8);
 target.fill(0xcc);hook_row("patch-call",patch_call(target.data(),replacement.data(),expected,8),8);
 target.fill(0xcc);hook_row("patch-short",patch_jump(target.data(),replacement.data(),expected,4),4);
 target.fill(0xcc);InlineHook out{};hook_row("inline",install_inline_hook(target.data(),replacement.data(),8,expected,out),8);
 line("inline-fields",std::to_string(out.target==target.data())+":"+std::to_string(out.gateway==gateway.data())+":"+std::to_string(out.length));
 target.fill(0xcc);gateway.fill(0);out={};fail="alloc";
 hook_row("inline-alloc-fail",install_inline_hook(target.data(),replacement.data(),8,expected,out),8);fail.clear();
 target.fill(0xcc);gateway.fill(0);out={};fail="protect-first";
 hook_row("inline-gateway-protect-fail",install_inline_hook(target.data(),replacement.data(),8,expected,out),8);fail.clear();
}
}
DWORD GetFileAttributesA(const char*path){trace.push_back(std::string("attr:")+path);return directories.count(path)?FILE_ATTRIBUTE_DIRECTORY:INVALID_FILE_ATTRIBUTES;}
BOOL VirtualProtect(void*p,std::size_t n,DWORD flags,DWORD*old){trace.push_back("protect:"+std::to_string(n)+":"+std::to_string(flags)+":"+std::to_string(p==target.data()?1:p==gateway.data()+8?2:3));
 if(fail=="protect"||(fail=="protect-first"&&p==gateway.data()+8))return 0;
 *old=0x20;return 1;}
void* VirtualAlloc(void*,std::size_t n,DWORD,DWORD){trace.push_back("alloc:"+std::to_string(n));return fail=="alloc"?nullptr:gateway.data();}
BOOL VirtualFree(void*p,std::size_t n,DWORD flags){trace.push_back("free:"+std::to_string(p==gateway.data())+":"+std::to_string(n)+":"+std::to_string(flags));return 1;}
BOOL FlushInstructionCache(HANDLE,const void*p,std::size_t n){trace.push_back("flush:"+std::to_string(n)+":"+std::to_string(p==target.data()?1:p==gateway.data()+8?2:3));return 1;}
HANDLE GetCurrentProcess(){return reinterpret_cast<HANDLE>(1);}

#if defined(A2FO_CORE_LOADER)
namespace {
using namespace a2fo;
std::vector<std::string> logs;
ModulePolicy mock_policy;
std::vector<InstalledModule> inventory;
std::string loader_fault;
bool builtin_ready=true,producer_ready=true;
void logger(const std::string&v){logs.push_back(v);trace.push_back("log:"+v);}
void begin(const std::string&v){trace.push_back("begin:"+v);}
void finish(const std::string&v,bool ok){trace.push_back("finish:"+v+":"+std::to_string(ok));}
bool A2FO_CALL init(const A2FO_ModuleApi*){trace.push_back("native-init");if(loader_fault=="init-throw")throw 1;return loader_fault!="init-reject";}
void A2FO_CALL shutdown(){trace.push_back("native-shutdown");}
void A2FO_CALL builtin_shutdown(){trace.push_back("builtin-shutdown");}
void policy_entry(const std::string&name,ModulePolicyState state,bool builtin=false){
 mock_policy.entries.push_back({name,name,name+".dll",builtin?"builtin:"+name:"Data\\modules\\"+name+".dll",state,true});
}
void loader_row(const char*label,bool ok,const LoadedModules&loaded){
 std::string value=std::to_string(ok)+":"+std::to_string(loaded.size());
 for(const auto&entry:loaded)value+=";"+entry.path+":"+std::to_string(entry.handle!=nullptr)+":"+std::to_string(entry.shutdown!=nullptr);
 line(label,value);
}
void loader_scenario(const char*label,const char*fault){
 loader_fault=fault;trace.clear();logs.clear();inventory={{"BuiltinOne","BuiltinOne.dll","builtin:BuiltinOne"},
 {"NativeOne","NativeOne.dll","Data\\modules\\NativeOne.dll"},
 {"NativeSkip","NativeSkip.dll","Data\\modules\\NativeSkip.dll"}};
 mock_policy={};mock_policy.managed=true;policy_entry("BuiltinOne",ModulePolicyState::active,true);
 policy_entry("NativeOne",ModulePolicyState::active);policy_entry("NativeSkip",ModulePolicyState::inactive);
 if(loader_fault=="required")mock_policy.entries[1].state=ModulePolicyState::required;
 if(loader_fault=="invalid")mock_policy.valid=false;
 A2FO_ModuleApi api{};LoadedModules loaded;
 const bool ok=load_native_modules_from_roots({"Data","Child"},api,loaded,logger,{begin,finish});
 loader_row(label,ok,loaded);
 loader_row("producer",producer_dispatch_available(loaded),loaded);
 unload_native_modules(loaded,logger);loader_row("unload",loaded.empty(),loaded);
}
void loader_test(){A2FO_ModuleApi api{};LoadedModules loaded;
 trace.clear();loader_row("empty-roots",load_native_modules_from_roots({},api,loaded,logger),loaded);
 for(auto fault:{"","load","missing-init","init-reject","init-throw","required","invalid","builtin-fail"})loader_scenario(fault,fault);
}
}
namespace a2fo {
std::vector<InstalledModule> discover_installed_modules(const std::string&root,std::vector<std::string>*diagnostics){
 trace.push_back("discover:"+root);if(diagnostics)diagnostics->push_back("mock inventory");return inventory;}
ModulePolicy evaluate_module_policy(const std::vector<std::string>&roots,const std::vector<InstalledModule>&){
 trace.push_back("policy:"+std::to_string(roots.size()));return mock_policy;}
void set_builtin_module_roots(const std::vector<std::string>&roots){trace.push_back("roots:"+std::to_string(roots.size()));}
bool find_builtin_feature(const std::string&name,BuiltinFeature&out) noexcept{
 trace.push_back("find:"+name);if(name=="BuiltinOne"){out=BuiltinFeature::feature_pack;return true;}return false;}
bool builtin_requires_restart(BuiltinFeature) noexcept{return true;}
bool prepare_builtin_module(BuiltinFeature,const A2FO_ModuleApi&,bool request){trace.push_back("prepare:"+std::to_string(request));
 if(loader_fault=="builtin-throw")throw 2;
 return loader_fault!="builtin-fail";}
A2FO_ModuleShutdownFn startup_builtin_shutdown(BuiltinFeature) noexcept{return builtin_shutdown;}
}
extern "C" bool A2FO_CALL A2FOBuiltinModule_StartupReady(const char*) noexcept{return builtin_ready;}
extern "C" bool A2FO_CALL A2FO_ProducerEventsReady(){trace.push_back("producer-ready");return producer_ready;}
extern "C" bool A2FO_CALL native_init(const A2FO_ModuleApi*api){return init(api);}
extern "C" void A2FO_CALL native_shutdown(){shutdown();}
HMODULE LoadLibraryA(const char*path){trace.push_back(std::string("load:")+path);return loader_fault=="load"?nullptr:reinterpret_cast<HMODULE>(0x1234);}
FARPROC GetProcAddress(HMODULE,const char*name){trace.push_back(std::string("proc:")+name);
 if(std::strcmp(name,"A2FO_ModuleInit")==0)return loader_fault=="missing-init"?nullptr:reinterpret_cast<FARPROC>(&native_init);
 if(std::strcmp(name,"A2FO_ModuleShutdown")==0)return reinterpret_cast<FARPROC>(&native_shutdown);
 return nullptr;}
BOOL FreeLibrary(HMODULE){trace.push_back("free-library");return 1;}
DWORD GetLastError(){trace.push_back("last-error");return 5;}
int _stricmp(const char*a,const char*b){for(;;++a,++b){unsigned char ca=std::tolower(static_cast<unsigned char>(*a)),cb=std::tolower(static_cast<unsigned char>(*b));if(ca!=cb||!ca)return int(ca)-int(cb);}}
#endif


#if defined(A2FO_CORE_BUILTIN)
namespace {
using namespace a2fo;
std::vector<std::string> startup_trace;
void builtin_row(const std::string&label,BuiltinFeature f){
 bool requested=false;
 const bool known=builtin_module_initial_request(f,requested);
 std::string v=std::to_string(builtin_module_enabled(f))+":"+
  std::to_string(builtin_module_available(f))+":"+std::to_string(builtin_module_activation(f))+":"+
  std::to_string(known)+":"+std::to_string(requested)+":"+
  builtin_module_status(canonical_builtin_name(f),{"Data"});
 line(label,v);
}
void builtin_test(){
 A2FO_ModuleApi api{};
 set_builtin_module_roots({"Data"});
 builtin_row("initial-live",BuiltinFeature::build_tooltips);
 builtin_row("initial-startup",BuiltinFeature::feature_pack);
 const bool selected=startup_builtin_selected(BuiltinFeature::mission_selector,"Data","-mod Alpha");
 line("startup-policy",std::to_string(selected));
 const bool prepared_live=prepare_builtin_module(BuiltinFeature::build_tooltips,api,false);
 line("prepare-live",std::to_string(prepared_live));
 const bool prepared_camera=prepare_builtin_module(BuiltinFeature::camera_controls,api,true);
 line("prepare-camera",std::to_string(prepared_camera));
 const bool prepared_startup=prepare_builtin_module(BuiltinFeature::feature_pack,api,true);
 line("prepare-startup",std::to_string(prepared_startup));
 line("startup-ready",std::to_string(A2FOBuiltinModule_StartupReady("A2FOFeaturePack")));
 finish_builtin_module_preparation(true,true,true,true,true,true,true,true);
 builtin_row("finished-live",BuiltinFeature::build_tooltips);
 builtin_row("finished-camera",BuiltinFeature::camera_controls);
 builtin_row("finished-startup",BuiltinFeature::feature_pack);
 ModulePolicy policy;policy.valid=true;policy.entries={{"","A2FOBuildTooltips","","",ModulePolicyState::active,true},
 {"","A2FOCameraControls","","",ModulePolicyState::inactive,true},
 {"","A2FOFeaturePack","","",ModulePolicyState::rejected,true}};
 std::string error;
 line("apply",std::to_string(apply_builtin_module_policy({"Data"},policy,error))+":"+error);
 builtin_row("applied-live",BuiltinFeature::build_tooltips);
 builtin_row("applied-startup",BuiltinFeature::feature_pack);
 line("other-root",std::to_string(apply_builtin_module_policy({"Other"},policy,error))+":"+error);
 policy.valid=false;line("invalid-policy",std::to_string(apply_builtin_module_policy({"Data"},policy,error))+":"+error);
 for(int i=24;i<48;++i){auto f=static_cast<BuiltinFeature>(i);
  const auto fn=startup_builtin_shutdown(f);
  line("shutdown-slot:"+std::to_string(i),std::to_string(fn!=nullptr));
 }
 line("startup-select-after",std::to_string(startup_builtin_selected(BuiltinFeature::feature_pack,"Data","")));
}
}
namespace a2fo {
ExtensionRootDiscovery discover_extension_roots(const std::string&root,const std::string&cmd){
 trace.push_back("discover-roots:"+root+":"+cmd);return {{root,"Mod"},{},"Alpha"};}
std::vector<InstalledModule> discover_installed_modules(const std::string&root,std::vector<std::string>*){
 trace.push_back("discover-installed:"+root);return {{"A2FOMissionSelector","",""}};}
ModulePolicy evaluate_module_policy(const std::vector<std::string>&roots,const std::vector<InstalledModule>&){
 trace.push_back("eval-policy:"+std::to_string(roots.size()));ModulePolicy p;
 p.entries={{"","A2FOMissionSelector","","",ModulePolicyState::active,true}};return p;}
}
#endif

int main(){
#if defined(A2FO_CORE_ROOTS)
 roots_test();
#elif defined(A2FO_CORE_HOOK)
 hook_test();
#elif defined(A2FO_CORE_LOADER)
 loader_test();
#elif defined(A2FO_CORE_BUILTIN)
 builtin_test();
#endif
}
