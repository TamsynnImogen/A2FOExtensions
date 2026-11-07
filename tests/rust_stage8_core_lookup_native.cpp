#include <windows.h>
#include <cstdio>
namespace {void wait_for_deferred_initialization();}
static BOOL WINAPI fixture_write(HANDLE,const void* text,DWORD size,DWORD* written,OVERLAPPED*){
    std::fputs("log:",stdout);std::fwrite(text,1,size,stdout);if(written)*written=size;return TRUE;
}
#define WriteFile fixture_write
#include CORE_SOURCE
#undef WriteFile
#include <stdexcept>
static unsigned wait_mode=0,handler_mode=0;
static bool A2FO_CALL lookup_handler(void* fs,void* name,std::uint32_t flags,void** result,void* data){
    std::printf("handler:%u:%u:%u:%u\n",static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(fs)),
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(name)),flags,
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(data)));
    g_fleet_ops=reinterpret_cast<HMODULE>(0x2000000);
    if(handler_mode==3)throw std::runtime_error("handler");
    *result=handler_mode==2?nullptr:reinterpret_cast<void*>(0x321);
    return handler_mode!=0;
}
namespace {
void wait_for_deferred_initialization(){
    std::printf("wait:%u\n",wait_mode);
    if(wait_mode==2)throw std::runtime_error("wait");
    if(wait_mode==1){g_fofs_item_lookup_dispatch={lookup_handler,reinterpret_cast<void*>(44)};
        g_fofs_item_lookup_dispatch_ready.store(true,std::memory_order_release);}
}
}
extern "C" void* a2fo_call_get_file_from_hash(void* function,void*,void*,std::uint32_t flags){
    std::printf("native:%08x:%u\n",static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(function)),flags);
    if(handler_mode==4)throw std::runtime_error("native");
    return reinterpret_cast<void*>(0x654);
}
static bool A2FO_CALL defaults_handler(const char*,char*,std::uint32_t,std::uint32_t*,std::int32_t*,void*){return true;}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    InitializeCriticalSection(&g_state_lock);g_log=reinterpret_cast<HANDLE>(1);
    for(wait_mode=0;wait_mode<3;++wait_mode)for(unsigned ready=0;ready<2;++ready)
    for(unsigned present=0;present<2;++present)for(handler_mode=0;handler_mode<5;++handler_mode){
        g_fleet_ops=reinterpret_cast<HMODULE>(0x1000000);
        g_fofs_item_lookup_dispatch={present?lookup_handler:nullptr,reinterpret_cast<void*>(22)};
        g_fofs_item_lookup_handler=present?lookup_handler:nullptr;g_fofs_item_lookup_user_data=reinterpret_cast<void*>(33);
        g_fofs_item_lookup_dispatch_ready.store(ready!=0,std::memory_order_release);
        std::printf("lookup-case:%u:%u:%u:%u\n",wait_mode,ready,present,handler_mode);
        try{const auto result=fofs_item_get_hash_lookup_hook(reinterpret_cast<void*>(1),reinterpret_cast<void*>(2),0x12345);
            std::printf("lookup-result:%u\n",static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(result)));}
        catch(const std::exception& error){std::printf("caught:%s\n",error.what());}
    }
    for(unsigned kind=0;kind<2;++kind)for(unsigned mask=0;mask<32;++mask)
    for(const char* name:{static_cast<const char*>(nullptr),"","new owner"}){
        g_state_lock_ready=(mask&1)!=0;g_policy_registration_open=(mask&2)!=0;g_registration_transaction.active=(mask&4)!=0;
        g_fofs_item_lookup_handler=(mask&8)?lookup_handler:nullptr;g_fofs_item_lookup_user_data=reinterpret_cast<void*>(8);g_fofs_item_lookup_owner="prior";
        g_info_ini_defaults_handler=(mask&8)?defaults_handler:nullptr;g_info_ini_defaults_user_data=reinterpret_cast<void*>(8);g_info_ini_defaults_owner="prior";
        std::printf("registration-case:%u:%u:%s\n",kind,mask,name?name:"<null>");
        bool result=false;
        if(kind==0)result=api_register_fofs_item_lookup_handler(name,(mask&16)?lookup_handler:nullptr,reinterpret_cast<void*>(99));
        else result=api_register_info_ini_defaults_handler(name,(mask&16)?defaults_handler:nullptr,reinterpret_cast<void*>(99));
        const auto data=kind==0?g_fofs_item_lookup_user_data:g_info_ini_defaults_user_data;
        std::printf("registration-result:%u:%s:%u\n",result,
            (kind==0?g_fofs_item_lookup_owner:g_info_ini_defaults_owner).c_str(),
            static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(data)));
    }
    DeleteCriticalSection(&g_state_lock);std::puts("core lookup fixture: PASS");
}
