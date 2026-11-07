#include <windows.h>
#include <cstdio>
static BOOL WINAPI fixture_write(HANDLE,const void* text,DWORD size,DWORD* written,OVERLAPPED*){
    std::fputs("log:",stdout);std::fwrite(text,1,size,stdout);if(written)*written=size;return TRUE;
}
#define WriteFile fixture_write
#include CORE_SOURCE
#undef WriteFile
#include <cstdlib>
#include <new>
static int budget=-1;
__attribute__((noinline)) void* operator new(std::size_t n){
    if(budget==0){budget=-1;throw std::bad_alloc();}if(budget>0)--budget;
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
__attribute__((noinline)) void operator delete(void* p)noexcept{std::free(p);}
__attribute__((noinline)) void operator delete(void* p,std::size_t)noexcept{std::free(p);}
__attribute__((noinline)) void* operator new[](std::size_t n){return ::operator new(n);}
__attribute__((noinline)) void operator delete[](void* p)noexcept{::operator delete(p);}
__attribute__((noinline)) void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static void typed(void* db,const char* key){
    int integer=-57;float real=-1.25f;std::uint8_t boolean=19;char text[32];std::memset(text,'?',sizeof(text));text[31]=0;
    const bool a=apply_integer_classlabel_default(db,key,&integer);
    const bool b=apply_float_classlabel_default(db,key,&real);
    const bool c=apply_bool_classlabel_default(db,key,&boolean);
    const bool d=apply_string_classlabel_default(db,key,text,sizeof(text));
    std::uint32_t bits=0;std::memcpy(&bits,&real,sizeof(bits));
    std::printf("typed:%u:%d:%u:%08x:%u:%u:%u:%s\n",a,integer,b,bits,c,boolean,d,text);
}
static DWORD WINAPI check_lock(void*){
    if(!TryEnterCriticalSection(&g_state_lock))return 3;
    LeaveCriticalSection(&g_state_lock);return 0;
}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    InitializeCriticalSection(&g_state_lock);g_log=reinterpret_cast<HANDLE>(1);
    void* db=reinterpret_cast<void*>(1);g_original_classlabels[db]="hybridbuild";
    auto& policy=g_classlabel_odf_defaults["hybridbuild"].defaults;
    for(const char* value:{"", "0", "-2", "  +12suffix", "TRUE", "yEs", "ON", "off", "false", "0x10", "1.23456789012345", "nan", "inf", "1e-46", "3.4028235e38", "abcdefghijklmnopqrstuvwxyz0123456789"}){
        policy["mixedkey"]=value;std::printf("value:%s\n",value);typed(db,"MiXeDkEy");
    }
    typed(nullptr,"mixedkey");typed(reinterpret_cast<void*>(2),"mixedkey");typed(db,nullptr);typed(db,"");typed(db,"missing");
    g_original_classlabels[reinterpret_cast<void*>(2)]="unknown";typed(reinterpret_cast<void*>(2),"mixedkey");
    char text[8];policy["s"]="1234567";
    for(std::uint32_t size:{0u,1u,7u,8u}){
        std::memset(text,'?',sizeof(text));const bool result=apply_string_classlabel_default(db,"s",text,size);
        std::printf("size:%u:%u:",size,result);for(unsigned char value:text)std::printf("%02x",value);std::puts("");
    }
    std::printf("nulls:%u:%u:%u:%u\n",apply_integer_classlabel_default(db,"s",nullptr),
        apply_float_classlabel_default(db,"s",nullptr),apply_bool_classlabel_default(db,"s",nullptr),
        apply_string_classlabel_default(db,"s",nullptr,8));
    std::string alias="before";const bool aliased=classlabel_odf_default(db,"s",alias,alias);
    std::printf("alias:%u:%s\n",aliased,alias.c_str());
    // Native allocation failures retain the same writes and lock release.
    policy["longkey"]="a long native default value which allocates storage outside small string optimization";
    for(int n=0;n<12;++n){
        char output[128]{};budget=n;bool result=false;bool caught=false;
        try{result=apply_string_classlabel_default(db,"longkey",output,sizeof(output));}
        catch(const std::bad_alloc&){caught=true;}
        budget=-1;std::printf("allocation:%d:%u:%u:%s\n",n,result,caught,output);
        // Each prior failed invocation must have released the native lock.
        HANDLE thread=CreateThread(nullptr,0,check_lock,nullptr,0,nullptr);if(!thread)return 4;
        if(WaitForSingleObject(thread,5000)!=WAIT_OBJECT_0)return 5;
        DWORD status=3;GetExitCodeThread(thread,&status);CloseHandle(thread);if(status)return 6;
    }
    DeleteCriticalSection(&g_state_lock);std::puts("core classdefaults fixture: PASS");
}
