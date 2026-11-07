// CPU-only fake IDirectDrawSurface. No window, input, drawing or audio device.
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
struct Format {std::uint32_t size,flags,fourcc,bits,red,green,blue,alpha;};
static_assert(sizeof(Format)==32);
struct Mock {std::uintptr_t* table;Format format;std::int32_t hr;bool fill;unsigned calls;unsigned initial_size;};
static std::int32_t __stdcall query(void* value,Format* output){
 auto& m=*static_cast<Mock*>(value);++m.calls;m.initial_size=output->size;
 if(m.fill)*output=m.format;return m.hr;
}
struct Spec {std::uint32_t r,g,b,a;};
int main(int argc,char**argv){
 if(argc!=4)return 2;bool legacy=std::strcmp(argv[3],"legacy")==0;
 HMODULE dll=LoadLibraryA(argv[1]);if(!dll){std::fprintf(stderr,"load error %lu\n",GetLastError());return 3;}
 auto proc=GetProcAddress(dll,argv[2]);if(!proc)return 3;
 using Call=std::uint32_t(__stdcall*)(void*);Call call;std::memcpy(&call,&proc,sizeof(call));
 std::array<std::uintptr_t,22> table{};table[21]=reinterpret_cast<std::uintptr_t>(&query);
 const Spec masks[]={{0,0,0,0},{0xff0000,0xff00,0xff,0},{0xff,0xff00,0xff0000,0},
 {0xff0000,0xff00,0xff,0xff000000},{0xff,0xff00,0xff0000,0xff000000},
 {0xf800,0x7e0,0x1f,0},{0x7c00,0x3e0,0x1f,0},{0x7c00,0x3e0,0x1f,0x8000},
 {0xfc00,0x3e0,0x1f,0},{0xfc00,0x3f0,0xf,0},{0xf00,0xf0,0xf,0xf000},
 {0xf00,0xf0,0xf,0},{0x7c00,0x3e1,0x1f,0x8000},{0xfc00,0x3e0,0x1e,0},
 {0xf800,0x7e0,0x1f,0x8000},{0xff0001,0,0,1}};
 unsigned index=0;bool okay=true;std::printf("{\"legacy\":%s,\"cases\":[",legacy?"true":"false");
 for(auto bits:{0u,8u,12u,16u,24u,32u})for(const auto& s:masks)for(auto flags:{0u,0xffffffffu}){
  Mock mock{table.data(),{32,flags,0,bits,s.r,s.g,s.b,s.a},0,true,0,0};
  auto result=call(&mock);okay=okay&&mock.calls==1&&mock.initial_size==32;
  std::printf("%s{\"i\":%u,\"bits\":%u,\"flags\":%u,\"fourcc\":0,\"r\":%u,\"g\":%u,\"b\":%u,\"a\":%u,\"hr\":0,\"fill\":true,\"result\":%u,\"calls\":%u,\"initial_size\":%u}",index?",":"",index,bits,flags,s.r,s.g,s.b,s.a,result,mock.calls,mock.initial_size);++index;
 }
 for(auto fourcc:{0x59565955u,0x32315659u,0x32595559u,0x12345678u})for(auto hr:{0,std::int32_t(0x80004005u)})for(bool fill:{false,true}){
  Mock mock{table.data(),{32,0xffffffffu,fourcc,32,0xff0000,0xff00,0xff,0xff000000},hr,fill,0,0};
  auto result=call(&mock);okay=okay&&mock.calls==1&&mock.initial_size==32;
  std::printf("%s{\"i\":%u,\"bits\":32,\"flags\":4294967295,\"fourcc\":%u,\"r\":16711680,\"g\":65280,\"b\":255,\"a\":4278190080,\"hr\":%d,\"fill\":%s,\"result\":%u,\"calls\":%u,\"initial_size\":%u}",index?",":"",index,fourcc,hr,fill?"true":"false",result,mock.calls,mock.initial_size);++index;
 }
 std::printf("],\"count\":%u,\"valid_query_contract\":%s",index,okay?"true":"false");
 // The older native helper dereferences null; do not invoke that invalid case.
 if(!legacy)std::printf(",\"null_result\":%u",call(nullptr));
 std::printf("}\n");return okay?0:4;
}
