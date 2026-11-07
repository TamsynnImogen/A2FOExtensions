// Headless SEC_IMAGE mapping only. Never initializes or executes the game.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
int main(int argc,char**argv){
 if(argc!=3)return 2;
 char*end=nullptr;auto rva=strtoul(argv[2],&end,0);if(!end||*end)return 2;
 HANDLE f=CreateFileA(argv[1],GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
 if(f==INVALID_HANDLE_VALUE)return 3;
 HANDLE m=CreateFileMappingA(f,nullptr,PAGE_READONLY|SEC_IMAGE,0,0,nullptr);CloseHandle(f);
 if(!m){printf("mapping error %lu\n",GetLastError());return 4;}
 auto base=static_cast<unsigned char*>(MapViewOfFile(m,FILE_MAP_READ,0,0,0));CloseHandle(m);
 if(!base)return 5;
 auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
 auto nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
 bool ok=rva+19<=nt->OptionalHeader.SizeOfImage && !memcmp(base+rva,"A2FOExtensions.dll",19);
 printf("SEC_IMAGE name RVA 0x%lx readable=%d\n",rva,int(ok));
 UnmapViewOfFile(base);return ok?0:6;
}
