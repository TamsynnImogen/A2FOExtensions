#include <windows.h>
#include <cstdio>
#include <cstring>
int main(int argc,char** argv) {
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 for(int i=1;i<argc;++i) {
  auto module=LoadLibraryA(argv[i]);
  if(!module){std::printf("FAIL LoadLibrary: %s %lu\n",argv[i],GetLastError());return 1;}
  const bool core=std::strstr(argv[i],"A2FOExtensions.dll")!=nullptr;
  if(!GetProcAddress(module,core?"A2FO_Initialize":"A2FO_ModuleInit"))return 2;
  // Match production lifetime: core owns a deferred worker, so retain DLLs.
  std::printf("PASS load/exports: %s\n",argv[i]);
 }
 return 0;
}
