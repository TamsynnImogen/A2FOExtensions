// Executes ONLY the hash-pinned pure channel phase routine in an isolated
// mapped image. No game initialization, graphics function or simulation runs.
#include "../renderer/native11/animation_phase.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* text){if(!yes){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
constexpr std::array<BYTE,32> expected{{0x43,0x00,0xab,0x71,0x73,0x1e,0x32,0x3f,0x57,0x67,0xd8,0xee,0x05,0x45,0x7e,0x64,0x44,0x40,0x1b,0x82,0x32,0x53,0xae,0xbe,0xe8,0x6c,0x5a,0xc2,0x3c,0x2b,0x00,0xde}};
bool pinned(const char* path){
    auto file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool okay=false;
    if(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
        std::array<BYTE,16384> bytes{};DWORD n=0;bool good=true;
        while(true){if(!ReadFile(file,bytes.data(),bytes.size(),&n,nullptr)){good=false;break;}if(!n)break;
            if(!CryptHashData(hash,bytes.data(),n,0)){good=false;break;}}
        std::array<BYTE,32> digest{};DWORD length=digest.size();
        okay=good&&CryptGetHashParam(hash,HP_HASHVAL,digest.data(),&length,0)&&length==32&&digest==expected;
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);return okay;
}
template<class T> void write(void* address,const T& value){DWORD old=0;need(VirtualProtect(address,sizeof(T),PAGE_READWRITE,&old)!=0,"private mapped data write admission");std::memcpy(address,&value,sizeof(T));DWORD ignored=0;need(VirtualProtect(address,sizeof(T),old,&ignored)!=0,"private mapped protection restore");}
void relocate_phase_fixture(BYTE* base){
    // LoadLibraryEx does not relocate EXE inspection mappings on this Wine
    // target. Apply ONLY the six reviewed HIGHLOW operands reachable from this
    // routine, accepting already-correct operands without a second relocation.
    const std::uint32_t fixes[][2]={{0x213927,0x7a8e74},{0x213931,0x7ad508},
        {0x213972,0x6ae6f0},{0x21398d,0x6ae6f0},{0x213a3d,0x6bc364},{0x273026,0x7b806c}};
    const auto delta=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base))-0x400000u;
    for(const auto& fix:fixes){std::uint32_t actual=0;std::memcpy(&actual,base+fix[0],4);
        const std::uint32_t relocated=fix[1]+delta;
        need(actual==fix[1]||actual==relocated,"reviewed native operand before rebasing");
        if(actual!=relocated)write(base+fix[0],relocated);
    }
    need(FlushInstructionCache(GetCurrentProcess(),base+0x213910,0x16b)!=0&&
         FlushInstructionCache(GetCurrentProcess(),base+0x273024,6)!=0,"rebased fixture instruction cache");
}
template<class T> void put(std::vector<BYTE>& object,unsigned offset,const T& value){need(offset+sizeof(T)<=object.size(),"fake object bounds");std::memcpy(object.data()+offset,&value,sizeof(T));}
unsigned short control(){unsigned short cw=0;asm volatile("fnstcw %0":"=m"(cw));return cw;}
void control(unsigned short cw){asm volatile("fldcw %0"::"m"(cw):"memory");}
unsigned precision_digits(unsigned short cw){
    switch(cw&0x300u){case 0:return 24;case 0x200:return 53;case 0x300:return 64;default:return 0;}
}
std::uint32_t bits(float value){std::uint32_t result=0;std::memcpy(&result,&value,4);return result;}
__attribute__((noinline)) float invoke(void* target,void* object){
    float result=0;
    asm volatile("call *%2; fstps %0":"=m"(result),"+c"(object):"r"(target):
        "eax","edx","cc","memory","st","st(1)","st(2)","st(3)","st(4)","st(5)","st(6)","st(7)");
    return result;
}
}
int main(int argc,char** argv){
    need(argc==2&&pinned(argv[1]),"exact image hash");
    auto image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);need(image!=nullptr,"private image mapping without startup");
    auto* base=reinterpret_cast<BYTE*>(image);
    relocate_phase_fixture(base);
    std::uint32_t loaded_elapsed_address=0;
    std::memcpy(&loaded_elapsed_address,base+0x213927,4);
    std::printf("mapped image=%p elapsed instruction=%08lx expected=%08lx\n",static_cast<void*>(base),static_cast<unsigned long>(loaded_elapsed_address),static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(base)+0x3a8e74));std::fflush(stdout);
    need(loaded_elapsed_address==reinterpret_cast<std::uintptr_t>(base)+0x3a8e74,"mapped phase code relocations");
    auto crt=LoadLibraryExW(L"msvcrt.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);need(crt!=nullptr,"system CRT");
    auto ftol=GetProcAddress(crt,"_ftol");need(ftol!=nullptr,"native import _ftol");
    FARPROC old_ftol=nullptr;std::memcpy(&old_ftol,base+0x3b806c,4);write(base+0x3b806c,ftol);
    std::uint32_t original_engine=0;float original_elapsed=0;
    std::memcpy(&original_engine,base+0x3ad508,4);std::memcpy(&original_elapsed,base+0x3a8e74,4);
    std::vector<BYTE> engine(0x104),instance(0x14),channel(0x58);
    const auto engine_address=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(engine.data()));
    const auto instance_address=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(instance.data()));
    put(engine,0x100,instance_address);write(base+0x3ad508,engine_address);
    auto* entry=base+0x213910;
    const auto original_control=control();unsigned cases=0;
    struct Precision {unsigned bits,digits;};
    constexpr Precision precisions[]={{0x000u,24u},{0x200u,53u},{0x300u,64u}};
    std::printf("private phase image=%p entry=%p CRTftol=%p\n",static_cast<void*>(base),static_cast<void*>(entry),reinterpret_cast<void*>(ftol));std::fflush(stdout);
    std::uint32_t random=0x53c24b11;
    for(const auto precision:precisions){
      const auto before=cases;
      for(unsigned rounding=0;rounding<4;++rounding){
        const auto cw=static_cast<unsigned short>((original_control&~0xf00u)|precision.bits|(rounding<<10));control(cw);
        need((control()&0x300u)==precision.bits,"requested x87 precision installed");
        for(unsigned flags: {0u,1u,3u,4u,5u,7u})for(unsigned count: {1u,2u,5u,16u})for(unsigned explicit_times=0;explicit_times<2;++explicit_times){
            for(unsigned sample=0;sample<32;++sample){
                AnimationPhaseInput input;input.flags=flags;input.key_count=count;
                input.duration=sample<8?1.f:7.25f;
                random=random*1664525u+1013904223u;
                const float choices[]{0,.25f,.5f,.51f,.95f,1,1.5f,2};
                input.elapsed=sample<8?choices[sample]:static_cast<float>(random%50000)/1024.f;
                input.offset=(sample%3==0)?.125f:0;
                if(explicit_times)for(unsigned i=0;i<count;++i)input.timestamps.push_back(static_cast<float>(double(i)*input.duration/count));
                const auto start=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(input.timestamps.data()));
                const auto finish=start+static_cast<std::uint32_t>(input.timestamps.size()*4);
                put(channel,0x20,start);put(channel,0x24,finish);put(channel,0x2c,input.duration);
                put(channel,0x30,std::uint32_t(0));put(channel,0x34,input.key_count);put(channel,0x38,input.offset);
                put(instance,0xc,flags);write(base+0x3a8e74,input.elapsed);
                volatile float native_value=invoke(entry,channel.data());need(control()==cw,"native control word restored");
                std::uint32_t native_key=0;std::memcpy(&native_key,channel.data()+0x30,4);
                AnimationPhase result;std::string error;
                need(evaluate_animation_phase(input,result,error),error.c_str());need(control()==cw,"owned phase control word restored");
                if(result.key!=native_key||bits(result.fraction)!=bits(native_value)){
                    std::fprintf(stderr,"phase mismatch precision=%u CW=%04x rc=%u flags=%u keys=%u explicit=%u sample=%u elapsed=%08lx offset=%08lx native=%u:%08lx own=%u:%08lx\n",
                        precision.digits,cw,rounding,flags,count,explicit_times,sample,
                        static_cast<unsigned long>(bits(input.elapsed)),static_cast<unsigned long>(bits(input.offset)),
                        native_key,static_cast<unsigned long>(bits(native_value)),result.key,
                        static_cast<unsigned long>(bits(result.fraction)));return 1;
                }
                need(result.finished==((flags&1u)==0&&static_cast<long double>(input.elapsed)+input.offset>=input.duration),"finish sentinel policy");++cases;
            }
        }
      }
      std::printf("x87 precision=%u CW.PC=%u phase comparisons=%u\n",
          precision_digits(control()),(control()>>8)&3u,cases-before);
    }
    control(original_control);write(base+0x3ad508,original_engine);write(base+0x3a8e74,original_elapsed);write(base+0x3b806c,old_ftol);
    need(FreeLibrary(image)&&FreeLibrary(crt),"private mappings released");
    std::printf("PASS %u bit-exact native channel phase comparisons; isolated routine only\n",cases);
}
