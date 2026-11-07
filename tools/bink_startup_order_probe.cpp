// Device-free loader-order fixture; compile each BUILD_* role separately.
#include <windows.h>
#include <cstdio>
#include <cstring>
#if defined(BUILD_CORE)
static int phase=0;
extern "C" __declspec(dllexport) int ProbeBinkOpen(){return phase;}
extern "C" __declspec(dllexport) int ProbePhase(){return phase;}
#if !defined(LEGACY_CORE)
extern "C" __declspec(dllexport) bool A2FO_BinkEarlyAttachNoUnwind(){
 auto f=GetModuleHandleA("FleetOpsHook.dll");if(!f||phase!=1)return false;
 auto ready=reinterpret_cast<int(__cdecl*)()>(GetProcAddress(f,"ProbeReady"));if(!ready||ready()!=1)return false;
 #ifdef FAIL_BRIDGE
 phase=9;return false;
 #else
 phase=2;return true;
 #endif
}
#endif
extern "C" __declspec(dllexport) bool A2FO_InitializeNoUnwind(){
 #ifdef LEGACY_CORE
 if(phase!=1)return false;
 #else
 if(phase!=2)return false;
 #endif
 phase=3;return true;
}
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)phase=1;return TRUE;}
#elif defined(BUILD_FLEETOPS)
extern "C" __declspec(dllimport) int ProbeBinkOpen();
static int ready=0;
extern "C" __declspec(dllexport) int ProbeReady(){return ready;}
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){if(ProbeBinkOpen()!=1)return FALSE;ready=1;}return TRUE;}
#else
extern "C" __declspec(dllimport) int ProbeBinkOpen();
int main(int argc,char**argv){
 if(argc!=3||ProbeBinkOpen()!=1)return 2;
 auto proxy=LoadLibraryA(argv[1]);
 auto core=GetModuleHandleA("A2FOExtensions.dll");
 auto phase=reinterpret_cast<int(__cdecl*)()>(GetProcAddress(core,"ProbePhase"));
 if(!strcmp(argv[2],"fail")){
   if(proxy||phase()!=9)return 3;puts("PASS refused failed early bridge");return 0;
 }
 if(!proxy){printf("proxy load failed %lu\n",GetLastError());return 3;}
 int expected=!strcmp(argv[2],"legacy")?1:2;
 if(phase()!=expected)return 4;
 auto init=reinterpret_cast<bool(__cdecl*)()>(GetProcAddress(core,"A2FO_InitializeNoUnwind"));
 if(!init()||phase()!=3)return 5;
 puts("PASS preloaded core -> FleetOpsHook attach -> optional early bridge -> deferred initialize");return 0;
}
#endif
