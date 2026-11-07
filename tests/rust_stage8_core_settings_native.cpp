#include <windows.h>
static void WINAPI settings_sleep(DWORD);
#define Sleep settings_sleep
#include CORE_SOURCE
#undef Sleep
#include <stdexcept>
static bool enabled=true,provider_ok=true,provider_throws=false,publish_winner=false,disable_on_call=false;
static std::uint32_t provider_has=1;
static std::int32_t provider_speed=4;
static unsigned calls=0,sleeps=0;
static std::string path="custom/path";
namespace a2fo {bool builtin_module_enabled(BuiltinFeature feature) noexcept{if(feature!=BuiltinFeature::info_ini)std::abort();return enabled;}}
static void WINAPI settings_sleep(DWORD value){if(value!=0)std::abort();++sleeps;g_mod_defaults_state=2;}
static bool A2FO_CALL provider(const char* normal,char* output,std::uint32_t capacity,std::uint32_t* has,std::int32_t* speed,void* data){
    if(data!=reinterpret_cast<void*>(0x55))std::abort();
    ++calls;
    std::printf("provider:%u:%u:%u:",calls,output!=nullptr,capacity);if(normal)for(const unsigned char* p=reinterpret_cast<const unsigned char*>(normal);*p;++p)std::printf("%02x",*p);std::puts("");
    if(output&&capacity){std::memset(output,'X',capacity);std::memcpy(output,path.data(),std::min<std::size_t>(path.size(),capacity));if(path.size()<capacity)output[path.size()]=0;}
    *has=provider_has;*speed=provider_speed;
    if(publish_winner)g_delphi_settings_directory="winner";
    if(disable_on_call)enabled=false;
    if(provider_throws)throw std::runtime_error("provider failure");
    return provider_ok;
}
static void hex(const std::string& value){for(unsigned char ch:value)std::printf("%02x",ch);}
static void reset(){enabled=provider_ok=true;provider_throws=publish_winner=disable_on_call=false;provider_has=1;provider_speed=4;calls=sleeps=0;path="custom/path";g_delphi_settings_directory=nullptr;g_mod_defaults_state=0;g_has_default_game_speed=false;g_default_game_speed=0;g_info_ini_defaults_handler=&provider;g_info_ini_defaults_user_data=reinterpret_cast<void*>(0x55);}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);InitializeCriticalSection(&g_state_lock);g_state_lock_ready=true;
    for(unsigned mode=0;mode<12;++mode){
        reset();std::string output="normal";bool has=true;int speed=99;
        if(mode==1)g_info_ini_defaults_handler=nullptr;
        if(mode==2)provider_ok=false;
        if(mode==3)provider_has=0;
        if(mode==4)provider_speed=0;
        if(mode==5)provider_speed=7;
        if(mode==6)provider_speed=-1;
        if(mode==7)provider_speed=6;
        if(mode==8)provider_throws=true;
        if(mode==9)path.clear();
        if(mode==10)path=std::string(kMaximumPathLength+50,'q');
        const bool result=query_info_ini_defaults(output.c_str(),mode==11?nullptr:&output,has,speed);
        std::printf("query:%u:%u:%u:%d:%u:",mode,result,has,speed,calls);hex(output);std::puts("");
    }
    for(unsigned mode=0;mode<8;++mode){
        reset();g_has_default_game_speed=true;g_default_game_speed=5;
        if(mode==1)enabled=false;
        if(mode==2)provider_ok=false;
        if(mode==3)provider_has=0;
        if(mode==4)provider_throws=true;
        if(mode==5)g_mod_defaults_state=2;
        if(mode==6)g_mod_defaults_state=1;
        if(mode==7)g_mod_defaults_state=-1;
        ensure_fleet_ops_info_defaults();std::printf("ensure:%u:%ld:%u:%d:%u:%u\n",mode,static_cast<long>(g_mod_defaults_state),g_has_default_game_speed,g_default_game_speed,calls,sleeps);
    }
    for(unsigned mode=0;mode<9;++mode){
        reset();g_mod_defaults_state=2;
        if(mode==1)enabled=false;
        if(mode==2)g_delphi_settings_directory="cached";
        if(mode==3)provider_ok=false;
        if(mode==4)path.clear();
        if(mode==5)publish_winner=true;
        if(mode==6)provider_throws=true;
        if(mode==7){g_mod_defaults_state=0;disable_on_call=true;}
        if(mode==8)path=std::string(kMaximumPathLength+50,'z');
        const char* value=resolved_settings_directory("normal");
        std::printf("resolve:%u:%u:%u:%u:",mode,value!=nullptr,g_delphi_settings_directory!=nullptr,calls);if(value)hex(value);std::puts("");
    }
    DeleteCriticalSection(&g_state_lock);std::puts("settings workflows: PASS");
}
