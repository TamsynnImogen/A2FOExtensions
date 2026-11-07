// Native module ABI and storage adapters; lifecycle policy is owned by Rust.
#include "module_loader.hpp"
#include "module_policy.hpp"
#include "builtin_modules.hpp"
#include "builtin_startup_modules.hpp"
#include "../rust/loader_ffi.h"
#include "../rust/runtime_modules_ffi.h"
#include <cstring>
#include <exception>
#include <new>
#include <stdexcept>
#include <utility>
namespace a2fo {
LoadedModules::LoadedModules(std::initializer_list<LoadedModule> entries) {
 try { for (const auto& entry : entries) push_back(entry); }
 catch (...) { a2fo_rs_runtime_modules_drop(records_); records_=nullptr; throw; }
}
LoadedModules::LoadedModules(const LoadedModules& other) {
 if (other.records_) {
  records_=a2fo_rs_runtime_modules_clone(other.records_);
  if (!records_) throw std::bad_alloc();
 }
}
LoadedModules& LoadedModules::operator=(const LoadedModules& other) {
 if (this != &other) { LoadedModules copy(other); std::swap(records_,copy.records_); }
 return *this;
}
LoadedModules::LoadedModules(LoadedModules&& other) noexcept : records_(other.records_) { other.records_=nullptr; }
LoadedModules& LoadedModules::operator=(LoadedModules&& other) noexcept {
 if (this != &other) { a2fo_rs_runtime_modules_drop(records_); records_=other.records_; other.records_=nullptr; }
 return *this;
}
LoadedModules::~LoadedModules() { a2fo_rs_runtime_modules_drop(records_); }
std::size_t LoadedModules::size() const noexcept { return a2fo_rs_runtime_modules_len(records_); }
void LoadedModules::push_back(const LoadedModule& entry) {
 if (!records_) {
  records_=a2fo_rs_runtime_modules_new();
  if (!records_) throw std::bad_alloc();
 }
 uintptr_t shutdown=0;
 static_assert(sizeof(shutdown)==sizeof(entry.shutdown),"shutdown pointer width");
 std::memcpy(&shutdown,&entry.shutdown,sizeof(shutdown));
 if (!a2fo_rs_runtime_modules_push(records_,reinterpret_cast<uintptr_t>(entry.handle),shutdown,
      reinterpret_cast<const uint8_t*>(entry.path.data()),entry.path.size())) throw std::bad_alloc();
}
void LoadedModules::clear() noexcept { a2fo_rs_runtime_modules_clear(records_); }
LoadedModules::View LoadedModules::view(std::size_t index) const noexcept {
 A2foRsRuntimeModuleView raw{};
 if (!a2fo_rs_runtime_modules_get(records_,index,&raw)) return {};
 return {raw.handle,raw.shutdown,raw.path,raw.path_size};
}
LoadedModule LoadedModules::operator[](std::size_t index) const {
 const auto raw=view(index);
 if (!raw.path) throw std::out_of_range("loaded module index");
 LoadedModule result;
 result.handle=reinterpret_cast<HMODULE>(raw.handle);
 std::memcpy(&result.shutdown,&raw.shutdown,sizeof(result.shutdown));
 result.path.assign(reinterpret_cast<const char*>(raw.path),raw.path_size);
 return result;
}
extern "C" bool A2FO_CALL A2FO_ProducerEventsReady();
extern "C" bool A2FO_CALL A2FOBuiltinModule_StartupReady(const char*) noexcept;
namespace {
struct Context {
 const std::vector<std::string>* roots{}; const A2FO_ModuleApi* api{};
 LoadedModules* loaded{}; void (*log)(const std::string&){};
 ModuleRegistrationObserver observer{}; std::vector<InstalledModule> installed;
 std::vector<std::string> diagnostics; ModulePolicy policy; std::exception_ptr failure;
};
A2foRsLoaderText text(const std::string& s){return {reinterpret_cast<const uint8_t*>(s.data()),s.size()};}
size_t get(void* p,uint32_t kind,size_t i,A2foRsLoaderEntry* out){
 auto& c=*static_cast<Context*>(p);
 if(kind==0){if(out)out->path=text((*c.roots)[i]);return c.roots->size();}
 if(kind==1){if(out)out->path=text(c.diagnostics[i]);return c.diagnostics.size();}
 if(kind==2){if(out)out->path=text(c.policy.diagnostics[i]);return c.policy.diagnostics.size();}
 if(kind==3){if(out)*out={{},{},{},0,0,c.policy.valid?1u:0u,c.policy.managed?1u:0u};return c.policy.entries.size();}
 if(kind==4){if(out){const auto& e=c.policy.entries[i];*out={text(e.name),text(e.filename),text(e.path),0,0,static_cast<uint32_t>(e.state),e.installed?1u:0u};}return c.policy.entries.size();}
 if(kind==5){if(out){const auto e=c.loaded->view(i);*out={{},{},{e.path,e.path_size},e.handle,e.shutdown,0,0};}return c.loaded->size();}
 return 0;
}
uintptr_t act(void* p,uint32_t op,size_t i,uintptr_t a,uintptr_t b,const uint8_t* bytes,size_t n){
 auto& c=*static_cast<Context*>(p);if(op==0)return c.failure?1:0;
 try {
  switch(op){
   case 1:c.installed=discover_installed_modules(c.roots->front(),&c.diagnostics);return c.installed.size();
   case 2:c.policy=evaluate_module_policy(*c.roots,c.installed);return 0;
   case 3:c.log(std::string(reinterpret_cast<const char*>(bytes),n));return 0;
   case 4:{const std::string s(reinterpret_cast<const char*>(bytes),n);const DWORD v=GetFileAttributesA(s.c_str());return v!=INVALID_FILE_ATTRIBUTES&&(v&FILE_ATTRIBUTE_DIRECTORY);}
   case 5:set_builtin_module_roots(*c.roots);return 0;
   case 6:{BuiltinFeature f;return find_builtin_feature(c.policy.entries[i].name,f)?static_cast<uintptr_t>(f)+1:0;}
   case 7:if(c.observer.begin)c.observer.begin(c.policy.entries[i].path);return 0;
   case 8:if(c.observer.finish)c.observer.finish(c.policy.entries[i].path,a!=0);return 0;
   case 9:try{return prepare_builtin_module(static_cast<BuiltinFeature>(a),*c.api,b!=0)?1:0;}catch(...){return 2;}
   case 10:return builtin_requires_restart(static_cast<BuiltinFeature>(a));
   case 11:c.loaded->push_back({nullptr,startup_builtin_shutdown(static_cast<BuiltinFeature>(a)),c.policy.entries[i].path});return 0;
   case 12:return reinterpret_cast<uintptr_t>(LoadLibraryA(c.policy.entries[i].path.c_str()));
   case 13:return GetLastError();
   case 14:return reinterpret_cast<uintptr_t>(GetProcAddress(reinterpret_cast<HMODULE>(a),"A2FO_ModuleInit"));
   case 15:{A2FO_ModuleInitFn init=nullptr;std::memcpy(&init,&a,sizeof(init));try{return init(c.api)?1:0;}catch(...){return 2;}}
   case 16:FreeLibrary(reinterpret_cast<HMODULE>(a));return 0;
   case 17:{LoadedModule e;e.handle=reinterpret_cast<HMODULE>(a);const FARPROC f=GetProcAddress(e.handle,"A2FO_ModuleShutdown");std::memcpy(&e.shutdown,&f,sizeof(f));e.path=c.policy.entries[i].path;c.loaded->push_back(e);return 0;}
   case 19:return A2FOBuiltinModule_StartupReady("A2FOFeaturePack")&&A2FO_ProducerEventsReady();
   case 20:{const FARPROC address=GetProcAddress(reinterpret_cast<HMODULE>(a),"A2FO_ProducerEventsReady");bool(A2FO_CALL *ready)()=nullptr;std::memcpy(&ready,&address,sizeof(ready));if(!ready)return 0;try{return ready();}catch(...){return 0;}}
   case 21:{const auto entry=c.loaded->view(i);A2FO_ModuleShutdownFn shutdown=nullptr;std::memcpy(&shutdown,&entry.shutdown,sizeof(shutdown));try{shutdown();return 0;}catch(...){return 1;}}
   case 22:c.loaded->clear();return 0;
   case 23:throw std::bad_alloc();
  }
 }catch(...){if(!c.failure)c.failure=std::current_exception();}
 return 0;
}
A2foRsLoaderApi bridge(Context& c){return {&c,get,act};}
}
bool producer_dispatch_available(const LoadedModules& loaded) noexcept {
 Context c;c.loaded=const_cast<LoadedModules*>(&loaded);const auto a=bridge(c);return a2fo_rs_loader_producer(&a)!=0;
}
bool load_native_modules_from_roots(const std::vector<std::string>& roots,const A2FO_ModuleApi& api,LoadedModules& loaded,void(*log)(const std::string&),const ModuleRegistrationObserver& observer){
 Context c;c.roots=&roots;c.api=&api;c.loaded=&loaded;c.log=log;c.observer=observer;const auto a=bridge(c);const bool ok=a2fo_rs_loader_run(&a)!=0;if(c.failure)std::rethrow_exception(c.failure);return ok;
}
void unload_native_modules(LoadedModules& loaded,void(*log)(const std::string&)){
 // Detach this generation before callbacks. Reentrant unload sees the public
 // owner empty; callback clear or replacement cannot invalidate retiring paths.
 LoadedModules retiring(std::move(loaded));
 Context c;c.loaded=&retiring;c.log=log;const auto a=bridge(c);a2fo_rs_loader_unload(&a);if(c.failure)std::rethrow_exception(c.failure);
}
}
