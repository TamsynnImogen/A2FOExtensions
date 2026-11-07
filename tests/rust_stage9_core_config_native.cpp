// Actual whole core. Foreign ABI, atomics, allocation and diagnostics mocked.
#include <windows.h>
#include <cstdio>
static BOOL WINAPI fake_WriteFile(HANDLE,const void*,DWORD,DWORD*,OVERLAPPED*);
static LPVOID WINAPI fake_VirtualAlloc(LPVOID,SIZE_T,DWORD,DWORD);
static void WINAPI fake_Sleep(DWORD);
static DWORD WINAPI fake_GetCurrentThreadId();
static LONG WINAPI fake_InterlockedCompareExchange(LONG volatile*,LONG,LONG);
static LONG WINAPI fake_InterlockedExchange(LONG volatile*,LONG);
static PVOID WINAPI fake_InterlockedCompareExchangePointer(PVOID volatile*,PVOID,PVOID);
static PVOID WINAPI fake_InterlockedExchangePointer(PVOID volatile*,PVOID);
#undef WriteFile
#define WriteFile fake_WriteFile
#undef VirtualAlloc
#define VirtualAlloc fake_VirtualAlloc
#undef Sleep
#define Sleep fake_Sleep
#undef GetCurrentThreadId
#define GetCurrentThreadId fake_GetCurrentThreadId
#undef InterlockedCompareExchange
#define InterlockedCompareExchange fake_InterlockedCompareExchange
#undef InterlockedExchange
#define InterlockedExchange fake_InterlockedExchange
#undef InterlockedCompareExchangePointer
#define InterlockedCompareExchangePointer fake_InterlockedCompareExchangePointer
#undef InterlockedExchangePointer
#define InterlockedExchangePointer fake_InterlockedExchangePointer
#include CORE_SOURCE
#undef WriteFile
#undef VirtualAlloc
#undef Sleep
#undef GetCurrentThreadId
#undef InterlockedCompareExchange
#undef InterlockedExchange
#undef InterlockedCompareExchangePointer
#undef InterlockedExchangePointer
#include <cstdlib>
static unsigned tick, throw_at, mutate_at, mode, sleeps, bridges;
static bool enabled;
static int live;
alignas(16) static unsigned char foreign[0x100];
alignas(16) static unsigned char allocation[65536];
static std::string* active_string;
struct Failure : std::exception {
 unsigned value;explicit Failure(unsigned n):value(n){++live;}
 Failure(const Failure& other) noexcept:value(other.value){++live;}
 ~Failure() noexcept override{--live;std::printf("destroy:%u:live=%d\n",value,live);}
};
static void event(const char* name,bool can_throw=true){
 ++tick;std::printf("call:%u:%s:live=%d\n",tick,name,live);
 if(tick==mutate_at){g_default_game_speed=11;g_has_default_game_speed=!g_has_default_game_speed;g_delphi_settings_directory="mutated-directory";std::puts("mutation");if(active_string)*active_string="mutated-longer-string";}
 if(can_throw&&tick==throw_at)throw Failure{tick};
}
static const char* atomic_name(const volatile void* p){
 if(p==&g_deferred_initialization_finished)return "finished";
 if(p==&g_mod_defaults_state)return "defaults";
 if(p==&g_fo_settings_first_run)return "first-run";
 if(p==&g_fo_settings_default_applied)return "applied";
 if(p==&g_default_game_speed_logged)return "speed-log";
 if(p==&g_runtime_game_speed_logged)return "runtime-log";
 if(p==&g_user_profile_game_speed_logged)return "profile-log";
 if(p==&g_fo_settings_instance)return "instance";
 return "other";
}
static LONG WINAPI fake_InterlockedCompareExchange(LONG volatile* p,LONG desired,LONG expected){std::printf("compare:%s:%ld:%ld\n",atomic_name(p),desired,expected);event("compare");LONG old=*p;if(old==expected)*p=desired;return old;}
static LONG WINAPI fake_InterlockedExchange(LONG volatile* p,LONG desired){std::printf("exchange:%s:%ld\n",atomic_name(p),desired);event("exchange");LONG old=*p;*p=desired;return old;}
static PVOID WINAPI fake_InterlockedCompareExchangePointer(PVOID volatile* p,PVOID desired,PVOID expected){std::printf("compare-pointer:%s\n",atomic_name(p));event("compare-pointer");PVOID old=*p;if(old==expected)*p=desired;return old;}
static PVOID WINAPI fake_InterlockedExchangePointer(PVOID volatile* p,PVOID desired){std::printf("exchange-pointer:%s:%u\n",atomic_name(p),desired!=nullptr);event("exchange-pointer");PVOID old=*p;*p=desired;return old;}
static DWORD WINAPI fake_GetCurrentThreadId(){event("thread");return 17;}
static void WINAPI fake_Sleep(DWORD ms){std::printf("sleep:%lu\n",static_cast<unsigned long>(ms));event("sleep");if(++sleeps==3&&mode!=4)g_deferred_initialization_finished=1;}
static LPVOID WINAPI fake_VirtualAlloc(LPVOID,SIZE_T size,DWORD flags,DWORD protect){std::printf("allocate:%u:%lu:%lu\n",static_cast<unsigned>(size),static_cast<unsigned long>(flags),static_cast<unsigned long>(protect));event("allocate");if(size>sizeof(allocation))std::abort();return mode==3?nullptr:allocation;}
static BOOL WINAPI fake_WriteFile(HANDLE,const void* data,DWORD size,DWORD* written,OVERLAPPED*){event("log");std::fwrite(data,1,size,stdout);if(written)*written=size;return TRUE;}
namespace a2fo { bool builtin_module_enabled(BuiltinFeature feature) noexcept {std::printf("builtin:%u\n",static_cast<unsigned>(feature));event("builtin",false);return enabled;} }
extern "C" void* a2fo_call_game_object_method(void* fn,void* self){std::printf("game:%u:%u\n",fn==reinterpret_cast<void*>(2),self!=nullptr);event("game");return reinterpret_cast<void*>(mode==1?0x100:0x101);}
extern "C" void* a2fo_call_delphi_one_register(void* fn,void* self){std::printf("delphi-one:%u:%u\n",fn==reinterpret_cast<void*>(3),self!=nullptr);event("delphi-one");++bridges;return bridges==1?(mode==2?nullptr:foreign):nullptr;}
extern "C" void* a2fo_call_delphi_two_registers(void* fn,void* first,void* second){std::printf("delphi-two:%u:%u:%u\n",fn==reinterpret_cast<void*>(4),first!=nullptr,second!=nullptr);event("delphi-two");return nullptr;}
static void reset(){
 tick=0;sleeps=0;bridges=0;active_string=nullptr;enabled=mode!=1;std::memset(foreign,0,sizeof(foreign));std::memset(allocation,0,sizeof(allocation));
 *reinterpret_cast<int*>(foreign+4)=3;*reinterpret_cast<int*>(foreign+8)=4;foreign[0x70]=mode==2?0:1;
 g_deferred_initialization_finished=1;g_initialize_thread_id=19;g_mod_defaults_state=2;g_has_default_game_speed=mode!=2;g_default_game_speed=7;
 g_fo_settings_first_run=mode==2?0:1;g_fo_settings_default_applied=mode==3?1:0;g_default_game_speed_logged=mode==3?1:0;g_runtime_game_speed_logged=mode==3?1:0;g_user_profile_game_speed_logged=mode==3?1:0;
 g_delphi_settings_directory=mode==2?nullptr:"settings-directory";g_fo_settings_instance=nullptr;
 g_game_configuration_new_hook.gateway=reinterpret_cast<void*>(1);g_game_configuration_load_profile_hook.gateway=reinterpret_cast<void*>(2);g_fo_settings_get_instance_hook.gateway=reinterpret_cast<void*>(3);g_mod_user_directory_hook.gateway=reinterpret_cast<void*>(4);
}
static void invoke(unsigned group){
 switch(group){
 case 0:game_configuration_new_hook(mode==2?nullptr:foreign,nullptr);break;
 case 1:std::printf("result:%u\n",game_configuration_load_profile_hook(mode==2?nullptr:foreign,nullptr));break;
 case 2:std::printf("result:%d\n",default_user_profile_game_speed_hook());break;
 case 3:std::printf("result:%u\n",fo_settings_get_instance_hook(reinterpret_cast<void*>(5))!=nullptr);break;
 case 4:{const char* result="original";mod_user_directory_hook(reinterpret_cast<void*>(6),mode==2?nullptr:&result);break;}
 case 5:{char output[64];std::memset(output,'?',sizeof(output));output[63]='\0';bool result=api_get_settings_directory(mode==2?nullptr:output,mode==3?4:64);std::printf("result:%u:%s\n",result,output);break;}
 case 6:{std::string input=mode==1?"":mode==2?std::string(kMaximumPathLength+1,'x'):"directory";active_string=&input;const char* result=make_static_delphi_string(input);std::printf("result:%u",result!=nullptr);if(result){std::int32_t ref,length;std::memcpy(&ref,result-8,4);std::memcpy(&length,result-4,4);std::printf(":%ld:%ld:%s",static_cast<long>(ref),static_cast<long>(length),result);}std::puts("");active_string=nullptr;break;}
 default:g_deferred_initialization_finished=mode==1?1:0;g_initialize_thread_id=mode==2?17:19;wait_for_deferred_initialization();std::printf("wait-sleeps:%u\n",sleeps);break;
 }
}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);InitializeCriticalSection(&g_state_lock);g_log=reinterpret_cast<HANDLE>(1);
 for(unsigned group=0;group<8;++group)for(mode=0;mode<5;++mode)for(unsigned variant=0;variant<3;++variant)for(unsigned at=1;at<=20;++at){
  if(variant==0&&at!=1)continue;
  if(group==7&&mode==4&&variant!=0)continue;
  reset();throw_at=variant==1?at:0;mutate_at=variant==2?at:0;
  std::printf("case:%u:%u:%u:%u\n",group,mode,variant,at);
  try{invoke(group);}catch(const Failure& error){std::printf("caught:%u:live=%d\n",error.value,live);}catch(...){std::puts("wrong-exception");return 2;}
  std::printf("state:%d:%d:%d:%d:%ld:%ld:%ld:%ld:%ld:%ld:%u\n",*reinterpret_cast<int*>(foreign+4),*reinterpret_cast<int*>(foreign+8),g_default_game_speed,g_has_default_game_speed,g_fo_settings_first_run,g_fo_settings_default_applied,g_default_game_speed_logged,g_runtime_game_speed_logged,g_user_profile_game_speed_logged,g_mod_defaults_state,g_fo_settings_instance!=nullptr);
  if(live)return 3;
 }
 mode=0;throw_at=0;mutate_at=0;
 for(int speed:{-1,(-2147483647-1),0,2147483647})for(unsigned group=0;group<4;++group){
  reset();g_default_game_speed=speed;
  std::printf("signed-speed:%d:%u\n",speed,group);invoke(group);
  std::printf("signed-result:%d:%d:%d\n",g_default_game_speed,*reinterpret_cast<int*>(foreign+4),*reinterpret_cast<int*>(foreign+8));
 }
 for(unsigned size:{0u,1u,18u,19u,20u,64u}){
  reset();char output[64]{};std::memset(output,'?',63);
  std::printf("directory-fit:%u:%u:",size,api_get_settings_directory(output,size));
  std::printf("%s\n",output);
 }
 reset();std::printf("null-directory-buffer:%u\n",api_get_settings_directory(nullptr,64));
 DeleteCriticalSection(&g_state_lock);std::puts("core config fixture: PASS");
}
