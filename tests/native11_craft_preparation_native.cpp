// Original CPU-only Craft preparation on synthetic instance/entity/class/race state.
// Lookup cache is seeded; only cb390 and the af100 leaf run. No simulation/GPU.
#include "../renderer/native11/craft_preparation.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include "../core/native11_craft_preparation_profile.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
using namespace a2fo::native11;
extern "C" void craft_preparation_invoke(void*,void*,void*);
asm(".text\n.globl _craft_preparation_invoke\n_craft_preparation_invoke:\n"
    "pushl %ebp\nmovl %esp,%ebp\nmovl 12(%ebp),%ecx\n"
    "pushl 16(%ebp)\ncall *8(%ebp)\nleave\nret\n");
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
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

std::uint32_t ptr(const void* p){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));}
std::uint32_t bits(float f){std::uint32_t v=0;std::memcpy(&v,&f,4);return v;}
template<class T>void write(void* p,const T& value){
    DWORD old=0,ignored=0;need(VirtualProtect(p,sizeof(value),PAGE_EXECUTE_READWRITE,&old)!=0,"private writable span");
    std::memcpy(p,&value,sizeof(value));need(VirtualProtect(p,sizeof(value),old,&ignored)!=0,"private protection restore");
}
template<class T,std::size_t N>void put(std::array<BYTE,N>& out,std::size_t offset,const T& value){
    need(offset+sizeof(value)<=N,"synthetic object extent");std::memcpy(out.data()+offset,&value,sizeof(value));
}
float get(const BYTE* p){float f=0;std::memcpy(&f,p,4);return f;}
unsigned short control(){unsigned short cw=0;asm volatile("fnstcw %0":"=m"(cw));return cw;}
void control(unsigned short cw){asm volatile("fldcw %0"::"m"(cw):"memory");}
}
int main(int argc,char** argv){
    need(argc==2&&pinned(argv[1]),"pinned Craft preparation CPU image");
    const auto image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(image!=nullptr,"private image without startup");auto* base=reinterpret_cast<BYTE*>(image);
    namespace profile=a2fo::native11_craft_preparation::profile;
    const auto delta=ptr(image)-profile::preferred_base;
    for(const auto& region:profile::regions){
        for(std::size_t k=0;k<region.relocation_count;++k){const auto offset=region.relocations[k];std::uint32_t expected_word=0,actual=0;
            std::memcpy(&expected_word,region.bytes+offset,4);std::memcpy(&actual,base+region.rva+offset,4);
            need(actual==expected_word||actual==expected_word+delta,"private preparation HIGHLOW original or relocated");
            if(actual==expected_word&&delta)write(base+region.rva+offset,expected_word+delta);
        }
        std::vector<BYTE> code(region.size);std::memcpy(code.data(),base+region.rva,region.size);
        for(std::size_t k=0;k<region.relocation_count;++k){const auto offset=region.relocations[k];std::uint32_t word=0;
            std::memcpy(&word,code.data()+offset,4);word-=delta;std::memcpy(code.data()+offset,&word,4);}
        need(std::memcmp(code.data(),region.bytes,region.size)==0,"exact original preparation instructions and constants");
    }
    std::array<BYTE,0xa8> instance{};std::array<BYTE,0x220> entity{};
    std::array<BYTE,0x88> object_class{};std::array<BYTE,0xc4> systems{};
    std::array<BYTE,0x28c> race{},other_race{};
    std::array<std::uint32_t,0x10c/4> vtable{};vtable[0x108/4]=ptr(base+0xaf100);
    put(entity,0,ptr(vtable.data()));put(entity,0x40,ptr(object_class.data()));
    put(entity,0xfc,ptr(race.data()));put(entity,0x1e0,ptr(systems.data()));
    std::uint32_t old_cache=0;std::memcpy(&old_cache,base+0x340bac,4);
    const auto saved=control();std::uint32_t random=0xcb390101u;unsigned cases=0;
    constexpr std::array<std::size_t,5> raw_offsets{{0,0x30,0x60,0x90,0xc0}};
    constexpr std::array<std::size_t,5> result_offsets{{0x9c,0x9d,0x9e,0xa0,0x9f}};
    for(unsigned pc:{0u,2u,3u})for(unsigned rc=0;rc<4;++rc){
        const auto cw=static_cast<unsigned short>((saved&~0xf00u)|(pc<<8)|(rc<<10)|0x3fu);
        for(unsigned sample=0;sample<4096;++sample){
            const auto next=[&](){random=random*1664525u+1013904223u;return random;};
            CraftPreparationInput input;
            for(auto& state:input.system_state)state=static_cast<std::uint8_t>(next()>>24);
            input.suppress_system_hiding=(next()&0x10)?255:0;
            input.race_matches_borg_cache=(next()&0x20)!=0;input.use_hull_ratio=(next()&0x40)!=0;
            input.hull_current=float(next()%2000000u)/1024.f;
            input.hull_maximum=.125f+float(next()%1000000u)/1024.f;
            input.hull_divisor=.125f+float(next()%100000u)/8192.f;
            if(sample<64){
                for(unsigned k=0;k<5;++k)input.system_state[k]=(sample&(1u<<k))?255:0;
                input.suppress_system_hiding=(sample&32u)?255:0;
                input.use_hull_ratio=true;input.hull_maximum=1.f;
                const float health[]{0.f,.5f,1.f,std::nextafter(1.f,0.f),std::nextafter(1.f,INFINITY),2.f,100.f};
                input.hull_current=health[sample%7];
                const float divisors[]{.125f,.5f,1.f,2.f,3.1234567f};input.hull_divisor=divisors[sample%5];
            }
            for(unsigned k=0;k<5;++k)put(systems,raw_offsets[k],input.system_state[k]);
            put(entity,0x1bf,input.suppress_system_hiding);
            put(entity,0x1dc,input.hull_current);put(entity,0x1c4,input.hull_maximum);
            put(object_class,0x84,std::int32_t(input.use_hull_ratio?(sample&1u?-1:1):0));
            put(race,0x288,input.hull_divisor);
            write(base+0x340bac,input.race_matches_borg_cache?ptr(race.data()):ptr(other_race.data()));
            instance.fill(0xcc);control(cw);
            craft_preparation_invoke(base+0xcb390,instance.data(),entity.data());
            need(control()==cw,"original preparation retains CW");control(saved);
            PreparedCraftState result;std::string error;
            need(prepare_craft_state(input,encode_native_x87(cw),result,error),"owned preparation accepts valid inputs");
            need(control()==saved,"owned preparation restores caller CW");
            for(unsigned k=0;k<5;++k)need(instance[result_offsets[k]]==static_cast<unsigned>(result.systems_hidden[k]),"native system flags match");
            need(instance[0xa1]==static_cast<unsigned>(result.borg_variant),"native Borg variant flag matches");
            const float actual=get(instance.data()+0xa4);
            if(bits(actual)!=bits(result.hull_fraction)){
                std::fprintf(stderr,"hull mismatch cw=%04x sample=%u use=%u current=%.9g maximum=%.9g divisor=%.9g result=%08lx native=%08lx\n",
                    cw,sample,unsigned(input.use_hull_ratio),input.hull_current,input.hull_maximum,input.hull_divisor,
                    static_cast<unsigned long>(bits(result.hull_fraction)),static_cast<unsigned long>(bits(actual)));return 1;
            }
            ++cases;
        }
    }
    control(saved);write(base+0x340bac,old_cache);need(FreeLibrary(image)!=0,"private preparation image released");
    std::printf("PASS original Craft preparation comparisons=%u across12 PC/RC modes; synthetic state; seeded lookup; no simulation/graphics\n",cases);
}
