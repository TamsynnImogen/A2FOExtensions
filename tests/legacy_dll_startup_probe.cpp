// Console fixture. Calls inert ABI functions directly, never installs input hooks.
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <cstring>
template<class T>T symbol(HMODULE module,const char* name) {
    const auto address=GetProcAddress(module,name);T function=nullptr;
    static_assert(sizeof(function)==sizeof(address));std::memcpy(&function,&address,sizeof(function));return function;
}

int wmain(int argc,wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    if(argc!=3)return 2;
    const bool missing=!std::wcscmp(argv[2],L"missing");
    const bool failed=!std::wcscmp(argv[2],L"failed");
    const bool original=!std::wcscmp(argv[2],L"baseline");
    HMODULE proxy=LoadLibraryW(argv[1]);
    if(missing) {
        if(proxy)return 3;
        std::puts("PASS missing required DLL refused");return 0;
    }
    if(!proxy){std::printf("FAIL proxy load %lu\n",GetLastError());return 4;}
    if((GetModuleHandleA("Win2kDisableTaskSwitch.original.dll")!=nullptr)!=original)return 5;
    const auto core=GetModuleHandleA("A2FOExtensions.dll");
    const auto fleet=GetModuleHandleA("FleetOpsHook.dll");
    using State=LONG(__cdecl*)();
    const auto core_state=core?symbol<State>(core,"FixtureState"):nullptr;
    const auto core_fleet=core?symbol<State>(core,"FixtureFleetReady"):nullptr;
    const auto fleet_state=fleet?symbol<State>(fleet,"FixtureState"):nullptr;
    if(!core_state || !core_fleet || !fleet_state || core_state()!=0 || core_fleet()!=1 || fleet_state()!=1)return 6;
    using Set=void(__cdecl*)(HHOOK);
    using Low=LRESULT(WINAPI*)(int,WPARAM,LPARAM);
    auto set=symbol<Set>(proxy,"?SetHookID@@YAXPAUHHOOK__@@@Z");
    auto low=symbol<Low>(proxy,"?LowLevelKeyboardProc@@YGJHIJ@Z");
    if(!set || !low)return 7;
    // The stock renamed DLL's callback has an ambiguous plain-ret ABI and the
    // supported game bypasses its setup. Compare startup through SetHookID;
    // test the replacement's documented stdcall return independently.
    for(unsigned i=0;i<8;++i) {set(nullptr);if(!original && low(0,0,0)!=0)return 8;}
    const LONG expected=!std::wcscmp(argv[2],L"no-init")?0:1;
    if(core_state()!=expected)return 9;
    if(failed || !std::wcscmp(argv[2],L"no-init"))std::puts("PASS initialization failure remains bounded");
    else std::puts("PASS fleet-before-core; deferred-on-export once; ABI return; process lifetime");
    // Retain all DLLs until process exit, matching the established product policy.
    return 0;
}
