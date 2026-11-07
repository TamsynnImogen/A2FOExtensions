// Map without resolving imports or invoking any engine/provider entry point.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "../sdk/include/a2fo_supported_fleetops.hpp"
int main(int argc,char**argv){
 if(argc!=3)return 2;
 auto engine=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
 auto fleetops=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
 if(!engine||!fleetops){printf("map failed %lu\n",GetLastError());return 3;}
 auto identity=a2fo::supported_armada::identify(engine);
 bool engine_ok=identity==a2fo::supported_armada::Identity::roots_profile;
 bool fleet_ok=a2fo::supported_fleetops::approved_profile(fleetops);
 printf("engine_profile=%d fleetops_profile=%d identity=%d; no entry points invoked\n",int(engine_ok),int(fleet_ok),int(identity));
 FreeLibrary(fleetops);FreeLibrary(engine);return engine_ok&&fleet_ok?0:4;
}
