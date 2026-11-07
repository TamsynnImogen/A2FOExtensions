// Headless replacement ABI check. No game, windows, input or audio device.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
template<class T>T get(HMODULE d,const char*n){auto p=GetProcAddress(d,n);if(!p){fprintf(stderr,"missing %s\n",n);exit(2);}T r;memcpy(&r,&p,sizeof(r));return r;}
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"failed line %d: %s\n",__LINE__,#x);return 3;}} while(0)
int main(int argc,char**argv){
 if(argc!=3)return 2;
 REQUIRE(GetModuleHandleA("binkw32.dll")==nullptr);
 HMODULE d=LoadLibraryA(argv[1]);if(!d){fprintf(stderr,"load error %lu\n",GetLastError());return 2;}
 auto open=get<void*(__stdcall*)(const char*,uint32_t)>(d,"_BinkOpen@8");
 auto close=get<void(__stdcall*)(void*)>(d,"_BinkClose@4");
 auto frame=get<int(__stdcall*)(void*)>(d,"_BinkDoFrame@4");
 auto next=get<void(__stdcall*)(void*)>(d,"_BinkNextFrame@4");
 auto go=get<void(__stdcall*)(void*,uint32_t,uint32_t)>(d,"_BinkGoto@12");
 auto wait=get<int(__stdcall*)(void*)>(d,"_BinkWait@4");
 auto copy=get<int(__stdcall*)(void*,void*,int32_t,uint32_t,uint32_t,uint32_t,uint32_t)>(d,"_BinkCopyToBuffer@28");
 auto volume=get<void(__stdcall*)(void*,uint32_t,int32_t)>(d,"_BinkSetVolume@8");
 auto err=get<const char*(__stdcall*)()>(d,"_BinkGetError@0");
 // Malformed foreign memory is rejected before dereference.
 REQUIRE(open(nullptr,0)==nullptr);REQUIRE(*err());
 void*h=open(argv[2],0);if(!h){fprintf(stderr,"open error %s\n",err());return 3;}
 auto p=static_cast<uint32_t*>(h);REQUIRE(p[0]&&p[1]&&p[2]>=4&&p[3]==1);
 uint32_t w=p[0],height=p[1];size_t count=size_t(w)*height*4;
 std::vector<unsigned char>a(count+128,0xcd),b(count+128,0xcd);
 REQUIRE(frame(h)==0);REQUIRE(p[4]==1);
 REQUIRE(copy(h,a.data()+64,w*4,height,0,0,0x13)==0);
 REQUIRE(frame(h)==0);REQUIRE(copy(h,b.data()+64,w*4,height,0,0,0x13)==0);REQUIRE(a==b);
 for(unsigned i=0;i<64;++i)REQUIRE(a[i]==0xcd&&a[count+64+i]==0xcd);
 volume(h,0,16384); // Intentionally calls ret12 despite the legacy export label.
 next(h);REQUIRE(p[3]==2);REQUIRE(wait(h)==1);
 Sleep(1200);REQUIRE(wait(h)==0);
 next(h);REQUIRE(p[3]==3);REQUIRE(frame(h)==0); // repair skipped frame two
 go(h,1,0);REQUIRE(p[3]==1);REQUIRE(frame(h)==0);
 REQUIRE(copy(h,b.data()+64,w*4,height,0,0,0x13)==0);REQUIRE(a==b);
 // A failure must leave guards and body unchanged.
 REQUIRE(copy(h,b.data()+64,-1,height,0,0,0x13)!=0);REQUIRE(a==b);
 // Unaccepted RGB16 experiments must remain disabled in ordinary core builds.
 std::vector<unsigned char> untouched(count+128,0xcd);
 for(auto format:{7u,11u,12u}) {
  REQUIRE(copy(h,untouched.data()+64,w*4,height,0,0,0x80000000u|format)!=0);
  for(auto byte:untouched)REQUIRE(byte==0xcd);
 }
 close(h);close(h);REQUIRE(frame(h)!=0);
 // BINKFROMMEMORY snapshots the caller's bytes; it must not retain that pointer.
 std::ifstream f(argv[2],std::ios::binary);std::vector<char>bytes((std::istreambuf_iterator<char>(f)),{});
 REQUIRE(!bytes.empty());h=open(bytes.data(),0x04000000);REQUIRE(h);
 std::fill(bytes.begin(),bytes.end(),0);REQUIRE(frame(h)==0);
 REQUIRE(copy(h,b.data()+64,w*4,height,0,0,0x13)==0);REQUIRE(a==b);close(h);
 h=open(argv[2],0x2000);REQUIRE(h);REQUIRE(frame(h)==0);close(h);
 REQUIRE(GetModuleHandleA("binkw32.dll")==nullptr);
 printf("PASS primary ABI: file/memory/preload, ret12 volume, copy guards, skipped frame, seek, wait, errors, close; native Bink absent\n");
 // A2FO is process-lifetime. Do not unload it while its initialization worker exits.
 return 0;
}
