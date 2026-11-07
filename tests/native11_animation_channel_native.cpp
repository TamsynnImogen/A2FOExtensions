// Executes only hash-pinned scalar/Matrix34 channel CPU routines in a private
// inspection mapping. Never starts the game, simulation or graphics.
#include "../renderer/native11/animation_channel.hpp"
#include "../renderer/native11/instance_animation_time.hpp"
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
    // target. Apply ONLY the reviewed HIGHLOW operands reachable from these
    // routine, accepting already-correct operands without a second relocation.
    const std::uint32_t fixes[][2]={{0x213927,0x7a8e74},{0x213931,0x7ad508},
        {0x213972,0x6ae6f0},{0x21398d,0x6ae6f0},{0x213a3d,0x6bc364},{0x273026,0x7b806c},
        {0x2146c2,0x6ae6f0},{0x2161e0,0x6ae6f0},
        {0x213678,0x7a8e78},{0x21367e,0x7a8e78},{0x213684,0x7ad508},
        {0x213692,0x7a8e78},{0x213698,0x7a8e74},{0x21369e,0x6ae15c},
        {0x2136ab,0x7a8e74},{0x2136c2,0x7a8e74}};
    const auto delta=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base))-0x400000u;
    for(const auto& fix:fixes){std::uint32_t actual=0;std::memcpy(&actual,base+fix[0],4);
        const std::uint32_t relocated=fix[1]+delta;
        need(actual==fix[1]||actual==relocated,"reviewed native operand before rebasing");
        if(actual!=relocated)write(base+fix[0],relocated);
    }
    need(FlushInstructionCache(GetCurrentProcess(),base+0x213910,0x16b)!=0&&
         FlushInstructionCache(GetCurrentProcess(),base+0x273024,6)!=0&&
         FlushInstructionCache(GetCurrentProcess(),base+0x2146a0,0x76)!=0&&
         FlushInstructionCache(GetCurrentProcess(),base+0x2161c0,0x58)!=0&&
         FlushInstructionCache(GetCurrentProcess(),base+0x213670,0x5b)!=0,"rebased fixture instruction cache");
}
template<class T> void put(std::vector<BYTE>& object,unsigned offset,const T& value){need(offset+sizeof(T)<=object.size(),"fake object bounds");std::memcpy(object.data()+offset,&value,sizeof(T));}
unsigned short control(){unsigned short cw=0;asm volatile("fnstcw %0":"=m"(cw));return cw;}
void control(unsigned short cw){asm volatile("fldcw %0"::"m"(cw):"memory");}
unsigned precision_digits(unsigned short cw){
    switch(cw&0x300u){case 0:return 24;case 0x200:return 53;case 0x300:return 64;default:return 0;}
}
std::uint32_t bits(float value){std::uint32_t result=0;std::memcpy(&result,&value,4);return result;}
__attribute__((noinline)) void invoke(void* target,void* channel,void* destination){
    // Original thiscall: ECX owns the channel, one stack target argument,
    // ret 4 in the isolated routine consumes that argument.
    asm volatile("pushl %[destination]; call *%[target]"
        : "+c"(channel) : [destination] "r"(destination),[target] "r"(target)
        : "eax","edx","cc","memory","st","st(1)","st(2)","st(3)",
          "st(4)","st(5)","st(6)","st(7)");
}
__attribute__((noinline)) void invoke_start(void* target,float start){
    const auto start_bits=bits(start);
    // SetStartTime is cdecl and returns the old global start in ST(0).
    asm volatile("pushl %[start]; call *%[target]; addl $4,%%esp; fstp %%st(0)"
        : : [start] "r"(start_bits),[target] "r"(target)
        : "eax","ecx","edx","cc","memory","st","st(1)","st(2)",
          "st(3)","st(4)","st(5)","st(6)","st(7)");
}
__attribute__((noinline)) void invoke_first(void* target){
    asm volatile("call *%0" : : "r"(target) : "eax","ecx","edx","cc","memory");
}
bool read_local(void*,std::uint32_t address,void* output,std::size_t size){
    SIZE_T copied=0;return ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<void*>(address),output,size,&copied)&&copied==size;
}
}
int main(int argc,char** argv){
    need(argc==2&&pinned(argv[1]),"exact isolated engine image hash");
    auto image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(image!=nullptr,"private image mapping without startup");
    auto* base=reinterpret_cast<BYTE*>(image);relocate_phase_fixture(base);
    auto crt=LoadLibraryExW(L"msvcrt.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    need(crt!=nullptr,"system CRT");
    auto ftol=GetProcAddress(crt,"_ftol");need(ftol!=nullptr,"native _ftol import");
    FARPROC old_ftol=nullptr;std::memcpy(&old_ftol,base+0x3b806c,4);write(base+0x3b806c,ftol);
    std::uint32_t old_engine=0;float old_elapsed=0,old_start=0;
    std::memcpy(&old_engine,base+0x3ad508,4);std::memcpy(&old_elapsed,base+0x3a8e74,4);
    std::memcpy(&old_start,base+0x3a8e78,4);
    std::vector<BYTE> engine(0x104),instance(0x84),database(4),channel(0x58);
    put(engine,0x100,static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(instance.data())));
    write(base+0x3ad508,static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(engine.data())));
    const auto scalar_play=base+0x2146a0,matrix_play=base+0x2161c0;
    const auto saved_cw=control();unsigned scalar_cases=0,matrix_cases=0,no_write=0;
    unsigned clock_cases=0;
    struct Precision {unsigned bits,digits;};
    constexpr Precision precisions[]={{0x000u,24u},{0x200u,53u},{0x300u,64u}};
    Object object;object.instance=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(instance.data()));
    object.database=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(database.data()));
    object.instance_vtable=0x11111111;object.database_vtable=0x22222222;
    put(instance,0,object.instance_vtable);put(database,0,object.database_vtable);
    put(instance,0x80,object.database);
    const InstanceAnimationTimeProfile time_profile{
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base))+0x3ad508,
        object.instance_vtable,object.database_vtable};
    // The independent reader must not use the last rendered instance slot.
    put(engine,0x100,std::uint32_t(0xdeadcafe));
    const float clock_values[]{0,.1f,1,7.25f,123456.25f,
        std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::max()};
    for(const auto precision:precisions){
      const auto before=clock_cases;
      for(unsigned rounding=0;rounding<4;++rounding){
        const auto cw=static_cast<unsigned short>((saved_cw&~0xf00u)|precision.bits|(rounding<<10));control(cw);
        need((control()&0x300u)==precision.bits,"requested x87 precision installed");
        for(unsigned enabled=0;enabled<2;++enabled)for(float now:clock_values)for(float start:clock_values){
            put(engine,0x8c,now);put(instance,0x18,start);put(instance,0xc,std::uint32_t(7));
            put(instance,0x76,static_cast<BYTE>(enabled));
            if(enabled)invoke_start(base+0x213670,start);else invoke_first(base+0x2136c0);
            need(control()==cw,"native elapsed helper CW restored");
            float native_elapsed=0;std::memcpy(&native_elapsed,base+0x3a8e74,4);
            InstanceAnimationTime owned;std::string error;
            need(capture_instance_animation_time(object,time_profile,nullptr,read_local,owned,error),error.c_str());
            need(control()==cw,"owned independent elapsed CW restored");
            if(bits(owned.elapsed)!=bits(native_elapsed)){
                std::fprintf(stderr,"elapsed mismatch precision=%u CW=%04x rc=%u enabled=%u now=%08lx start=%08lx own=%08lx native=%08lx\n",
                    precision.digits,cw,rounding,enabled,static_cast<unsigned long>(bits(now)),static_cast<unsigned long>(bits(start)),static_cast<unsigned long>(bits(owned.elapsed)),
                    static_cast<unsigned long>(bits(native_elapsed)));return 1;
            }
            need(owned.flags==7&&owned.animating==(enabled!=0),"independent instance timing identity");++clock_cases;
        }
      }
      std::printf("x87 precision=%u CW.PC=%u instance clocks=%u\n",
          precision_digits(control()),(control()>>8)&3u,clock_cases-before);
    }
    put(engine,0x100,object.instance);
    std::uint32_t random=0x61c34205;
    for(const auto precision:precisions){
      const auto before_scalar=scalar_cases,before_matrix=matrix_cases;
      for(unsigned rounding=0;rounding<4;++rounding){
        const auto cw=static_cast<unsigned short>((saved_cw&~0xf00u)|precision.bits|(rounding<<10));control(cw);
        need((control()&0x300u)==precision.bits,"requested x87 precision installed");
        for(unsigned flags:{0u,1u,3u,4u,5u,7u})for(unsigned count:{1u,2u,5u,16u})
        for(unsigned explicit_times=0;explicit_times<2;++explicit_times)for(unsigned mode=0;mode<4;++mode)
        for(unsigned sample=0;sample<24;++sample){
            AnimationPhaseInput timing;timing.flags=flags;timing.key_count=count;
            timing.duration=sample<8?1.f:7.25f;
            random=random*1664525u+1013904223u;
            const float choices[]{0,.25f,.5f,.51f,.95f,1,1.5f,2};
            timing.elapsed=sample<8?choices[sample]:static_cast<float>(random%50000)/1024.f;
            timing.offset=sample%3==0?.125f:0;
            if(explicit_times)for(unsigned i=0;i<count;++i)
                timing.timestamps.push_back(static_cast<float>(double(i)*timing.duration/count));
            const auto times=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(timing.timestamps.data()));
            put(channel,0x20,times);put(channel,0x24,times+static_cast<std::uint32_t>(timing.timestamps.size()*4));
            put(channel,0x2c,timing.duration);put(channel,0x30,std::uint32_t(0));
            put(channel,0x34,count);put(channel,0x38,timing.offset);put(channel,0x40,std::uint32_t(4));
            put(channel,0x44,mode);put(instance,0xc,flags);write(base+0x3a8e74,timing.elapsed);
            ScalarAnimationChannel scalar;scalar.timing=timing;scalar.interpolation=mode;
            for(unsigned i=0;i<count+(mode==3?1:0);++i){
                random=random*1664525u+1013904223u;
                scalar.keys.push_back(static_cast<float>(static_cast<int>(random%500001)-250000)/4096.f);
            }
            std::array<float,3> scalar_target{{-123.25f,23.75f,-456.5f}};
            const bool direct=(sample&1)!=0;
            put(channel,0x3c,direct?static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(scalar_target.data())):0u);
            put(channel,0x4c,static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(scalar.keys.data())));
            invoke(scalar_play,channel.data(),scalar_target.data());
            need(control()==cw,"native scalar control word restored");
            std::uint32_t native_key=0;std::memcpy(&native_key,channel.data()+0x30,4);
            ScalarAnimationSample owned;std::string error;
            need(evaluate_scalar_animation_channel(scalar,owned,error),error.c_str());
            need(control()==cw,"owned scalar control word restored");
            const float expected_value=owned.writes?owned.value:23.75f;
            if(bits(scalar_target[1])!=bits(expected_value)||native_key!=owned.phase.key){
                std::fprintf(stderr,"scalar mismatch precision=%u CW=%04x rc=%u flags=%u count=%u times=%u mode=%u sample=%u key=%u/%u value=%08lx/%08lx\n",
                    precision.digits,cw,rounding,flags,count,explicit_times,mode,sample,native_key,owned.phase.key,
                    static_cast<unsigned long>(bits(scalar_target[1])),static_cast<unsigned long>(bits(expected_value)));
                return 1;
            }
            need(scalar_target[0]==-123.25f&&scalar_target[2]==-456.5f,"scalar target bounds");
            no_write+=!owned.writes;++scalar_cases;
            MatrixAnimationChannel matrix;matrix.timing=timing;matrix.interpolation=mode;
            for(unsigned i=0;i<count;++i){std::array<float,12> key{};
                for(unsigned c=0;c<12;++c)key[c]=static_cast<float>(int(i*13+c)-25)/16.f;
                matrix.keys.push_back(key);
            }
            std::array<float,14> matrix_target;matrix_target.fill(23.75f);
            matrix_target.front()=-123.25f;matrix_target.back()=-456.5f;
            put(channel,0x30,std::uint32_t(0));
            put(channel,0x3c,direct?static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(matrix_target.data())):0u);
            put(channel,0x4c,static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(matrix.keys.data())));
            invoke(matrix_play,channel.data(),matrix_target.data());need(control()==cw,"native matrix CW restored");
            MatrixAnimationSample sampled;need(evaluate_matrix_animation_channel(matrix,sampled,error),error.c_str());
            need(control()==cw,"owned matrix CW restored");
            std::memcpy(&native_key,channel.data()+0x30,4);
            if(native_key!=sampled.phase.key){
                std::fprintf(stderr,"matrix key mismatch precision=%u CW=%04x rc=%u flags=%u count=%u times=%u mode=%u sample=%u native=%u owned=%u\n",
                    precision.digits,cw,rounding,flags,count,explicit_times,mode,sample,native_key,sampled.phase.key);return 1;
            }
            for(unsigned c=0;c<12;++c){
                const auto expected_value=sampled.writes?sampled.value[c]:23.75f;
                if(bits(matrix_target[c+1])!=bits(expected_value)){
                    std::fprintf(stderr,"matrix value mismatch precision=%u CW=%04x rc=%u flags=%u count=%u times=%u mode=%u sample=%u component=%u native=%08lx owned=%08lx\n",
                        precision.digits,cw,rounding,flags,count,explicit_times,mode,sample,c,
                        static_cast<unsigned long>(bits(matrix_target[c+1])),static_cast<unsigned long>(bits(expected_value)));return 1;
                }
            }
            need(matrix_target.front()==-123.25f&&matrix_target.back()==-456.5f,"matrix target bounds");
            ++matrix_cases;
        }
      }
      std::printf("x87 precision=%u CW.PC=%u scalar=%u Matrix34=%u\n",
          precision_digits(control()),(control()>>8)&3u,scalar_cases-before_scalar,matrix_cases-before_matrix);
    }
    control(saved_cw);write(base+0x3ad508,old_engine);write(base+0x3a8e74,old_elapsed);
    write(base+0x3a8e78,old_start);write(base+0x3b806c,old_ftol);
    need(FreeLibrary(image)&&FreeLibrary(crt),"private inspection mappings released");
    std::printf("PASS %u instance clocks, %u scalar and %u Matrix34 bit-exact native comparisons; %u scalar no-write cases; isolated CPU routines only\n",clock_cases,scalar_cases,matrix_cases,no_write);
}
