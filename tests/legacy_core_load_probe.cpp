// Actual candidate DLL in an unsupported inert host; no game code is loaded.
#include "../rust/runtime_globals_ffi.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
int main(int argc,char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    if(argc!=3)return 2;
    auto dll=LoadLibraryA(argv[1]);if(!dll){std::printf("FAIL actual core load %lu\n",GetLastError());return 3;}
    const auto rva=std::strtoul(argv[2],nullptr,16);
    using Globals=A2foRuntimeGlobals*(__cdecl*)();Globals globals=nullptr;
    const auto address=reinterpret_cast<void*>(reinterpret_cast<unsigned char*>(dll)+rva);
    static_assert(sizeof(globals)==sizeof(address));std::memcpy(&globals,&address,sizeof(globals));
    auto state=globals();const auto started=GetTickCount();
    while(InterlockedCompareExchange(&state->deferred_finished,0,0)!=1){if(GetTickCount()-started>15000)return 4;Sleep(5);}
    using Init=bool(__cdecl*)();Init init=nullptr;
    const auto exported=GetProcAddress(dll,"A2FO_InitializeNoUnwind");
    static_assert(sizeof(init)==sizeof(exported));std::memcpy(&init,&exported,sizeof(init));
    if(!init || init())return 5;
    if(InterlockedCompareExchange(&state->initialize_state,0,0)!=0 || state->initialize_thread_id!=0)return 6;
    std::puts("PASS actual core load/worker; contained retry refuses unsupported host; original ABI retained");
    return 0;
}
