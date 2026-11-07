#include <windows.h>
#include <string>
#include <vector>
#include <cstdio>
#include <stdexcept>
static void fixture_open_log();
static void fixture_log(const std::string&);
static std::string fixture_directory();
static DWORD fixture_initialize();
static void fixture_monitor();
static HMODULE WINAPI fixture_get_module(LPCSTR);
static LONG WINAPI fixture_compare(volatile LONG*,LONG,LONG);
static LONG WINAPI fixture_exchange(volatile LONG*,LONG);
static void WINAPI fixture_sleep(DWORD);
static void WINAPI fixture_critical(LPCRITICAL_SECTION);
static DWORD WINAPI fixture_thread_id();
#undef InterlockedCompareExchange
#undef InterlockedExchange
#define GetModuleHandleA fixture_get_module
#define InterlockedCompareExchange fixture_compare
#define InterlockedExchange fixture_exchange
#define Sleep fixture_sleep
#define InitializeCriticalSection fixture_critical
#define GetCurrentThreadId fixture_thread_id
#include CORE_SOURCE
#undef GetModuleHandleA
#undef InterlockedCompareExchange
#undef InterlockedExchange
#undef Sleep
#undef InitializeCriticalSection
#undef GetCurrentThreadId
static std::vector<std::string> events;
static std::vector<LONG> states;
static unsigned state_index{},sleeps{},polls{},critical{},init_calls{},module_calls{},monitor_calls{};
static bool armada_present=true,init_success=true;
static unsigned throw_phase=0;
static void fixture_open_log(){events.push_back("open");g_log=reinterpret_cast<HANDLE>(0x12);if(throw_phase==1)throw std::runtime_error("open");}
static void fixture_log(const std::string& text){events.push_back(text);if(throw_phase==2)throw std::runtime_error("log");}
static std::string fixture_directory(){events.push_back("directory");return "fixture-root";}
static DWORD fixture_initialize(){++init_calls;events.push_back("initialize");if(throw_phase==3)throw std::runtime_error("initialize");return init_success?0:1;}
static void fixture_monitor(){++monitor_calls;events.push_back("monitor");}
static HMODULE WINAPI fixture_get_module(LPCSTR name){++module_calls;events.push_back(name?"fleet":"armada");return name?reinterpret_cast<HMODULE>(0x20000000):(armada_present?reinterpret_cast<HMODULE>(0x10000000):nullptr);}
static LONG WINAPI fixture_compare(volatile LONG* target,LONG exchange,LONG comparand){
    if(target==&g_initialize_state&&exchange==0&&comparand==0){++polls;if(!states.empty()){*target=states[state_index<states.size()?state_index++:states.size()-1];}}
    const LONG old=*target;if(old==comparand)*target=exchange;return old;
}
static LONG WINAPI fixture_exchange(volatile LONG* target,LONG value){const LONG old=*target;*target=value;return old;}
static void WINAPI fixture_sleep(DWORD value){if(value!=1)std::abort();++sleeps;}
static void WINAPI fixture_critical(LPCRITICAL_SECTION value){if(value!=&g_state_lock)std::abort();++critical;if(throw_phase==4)throw std::runtime_error("critical");}
static DWORD WINAPI fixture_thread_id(){return 0x44;}
static void hex(const std::string& value){for(unsigned char ch:value)std::printf("%02x",ch);}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    for(unsigned mode=0;mode<17;++mode){
        events.clear();states.clear();state_index=sleeps=polls=critical=init_calls=module_calls=monitor_calls=0;
        armada_present=init_success=true;throw_phase=0;g_initialize_state=0;g_log=INVALID_HANDLE_VALUE;g_root_directory.clear();g_armada=nullptr;g_fleet_ops=nullptr;g_state_lock_ready=false;g_initialize_thread_id=0;g_deferred_initialization_finished=0;
        if(mode==1)g_initialize_state=2;
        if(mode==2){g_initialize_state=1;states={1,1,2};}
        if(mode==3){g_initialize_state=1;states={0,2};}
        if(mode==4)g_initialize_state=1;
        if(mode==5)g_initialize_state=-1;
        if(mode==6)armada_present=false;
        if(mode==7)init_success=false;
        if(mode==8){g_log=reinterpret_cast<HANDLE>(0x12);g_root_directory="already";g_fleet_ops=reinterpret_cast<HMODULE>(0x22);g_state_lock_ready=true;}
        if(mode>=9&&mode<=12)throw_phase=mode-8;
        if(mode==14)g_initialize_state=2;
        if(mode==15)init_success=false;
        if(mode==16){g_initialize_state=1;states={0,0};}
        bool result=false,threw=false;DWORD worker_result=99;
        try{if(mode>=13&&mode<=15)worker_result=initialize_worker(nullptr);else result=a2fo_uninvoked_fixture_Initialize();}catch(const std::runtime_error&){threw=true;}
        std::printf("init:%u:%u:%u:%lu:%ld:%u:%u:%u:%u:%u:%lu:%ld:",mode,result,threw,static_cast<unsigned long>(worker_result),static_cast<long>(g_initialize_state),sleeps,polls,critical,init_calls,module_calls,static_cast<unsigned long>(g_initialize_thread_id),static_cast<long>(g_deferred_initialization_finished));hex(g_root_directory);std::puts("");
        for(const auto& event:events){std::printf("event:");hex(event);std::puts("");}
    }
    std::puts("core deferred lifecycle: PASS");
}
