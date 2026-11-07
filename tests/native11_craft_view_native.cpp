// Isolated original CPU distance/fade routines on synthetic object/game state.
// No game startup, native allocator, simulation or graphics call.
#include "../renderer/native11/craft_view_policy.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include "../core/native11_craft_view_profile.hpp"
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
extern "C" {
std::uint32_t craft_view_fixture_distance(void*,void*,const float*,const float*);
std::uint32_t craft_view_fixture_fade(void*,void*,float*);
}
asm(".text\n.globl _craft_view_fixture_distance\n_craft_view_fixture_distance:\n"
    "pushl %ebp\nmovl %esp,%ebp\nmovl 12(%ebp),%ecx\n"
    "pushl 20(%ebp)\npushl 16(%ebp)\ncall *8(%ebp)\nleave\nret\n"
    ".globl _craft_view_fixture_fade\n_craft_view_fixture_fade:\n"
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
    const bool dynamic_fade=argc==3&&std::strcmp(argv[2],"--dynamic-fade")==0;
    need((argc==2||dynamic_fade)&&pinned(argv[1]),"pinned Craft view CPU image");
    const auto image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(image!=nullptr,"private image without startup");auto* base=reinterpret_cast<BYTE*>(image);
    namespace profile=a2fo::native11_animation_clock::view_profile;
    const auto delta=ptr(image)-profile::preferred_base;
    for(const auto& region:profile::regions){
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];std::uint32_t expected_word=0,actual=0;
            std::memcpy(&expected_word,region.bytes+offset,4);std::memcpy(&actual,base+region.rva+offset,4);
            need(actual==expected_word||actual==expected_word+delta,"private view HIGHLOW before normalization");
            if(actual==expected_word)write(base+region.rva+offset,expected_word+delta);
        }
        std::vector<BYTE> code(region.size);std::memcpy(code.data(),base+region.rva,region.size);
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];std::uint32_t value=0;
            std::memcpy(&value,code.data()+offset,4);value-=delta;std::memcpy(code.data()+offset,&value,4);}
        need(std::memcmp(code.data(),region.bytes,region.size)==0,"exact private CPU view profile");
    }
    std::array<BYTE,0xa8> instance{};std::array<BYTE,0x90> game{};std::array<BYTE,0x44> settings{};
    std::array<std::uint32_t,0x2c/4> vtable{};vtable[0x28/4]=ptr(base+0x73c20);
    std::uint32_t old_game=0,old_mode=0;std::uint8_t old_skip=0;float old_distance=0;
    std::array<float,3> old_near_fade{};
    std::memcpy(&old_game,base+0x36b5ac,4);std::memcpy(&old_mode,base+0x36b610,4);
    std::memcpy(&old_skip,base+0x3637b5,1);std::memcpy(&old_distance,base+0x2fcac0,4);
    std::memcpy(old_near_fade.data(),base+0x30444c,sizeof(old_near_fade));
    put(instance,0,ptr(vtable.data()));put(game,0x80,ptr(settings.data()));write(base+0x36b5ac,ptr(game.data()));
    const auto saved=control();std::uint32_t random=0x4d5bb021u;unsigned cases=0;
    for(unsigned pc:{0u,2u,3u})for(unsigned rc=0;rc<4;++rc){
        const auto cw=static_cast<unsigned short>((saved&~0xf00u)|(pc<<8)|(rc<<10)|0x3fu);
        for(unsigned sample=0;sample<4096;++sample){
            CraftViewInput input;
            const auto next=[&](){random=random*1664525u+1013904223u;return random;};
            for(float& v:input.camera)v=float(static_cast<int>(next()%2000001u)-1000000)/128.f;
            for(float& v:input.position)v=float(static_cast<int>(next()%2000001u)-1000000)/128.f;
            input.radius=float(next()%1000000u)/1024.f;
            input.max_distance=100.f+float(next()%1000000u)/32.f;
            input.mode=next()%4;input.detailed_range=(next()&0x10)!=0;
            input.reveal_all=(next()&0x10)!=0;input.instance_cull_flag=(next()&0x10)!=0;
            input.skip_near_fade=(next()&0x10)!=0;input.fade_enabled=(next()&0x10)!=0;
            if(sample<32){
                input={};input.max_distance=2000;input.radius=50;input.detailed_range=true;input.fade_enabled=true;
                const float distances[]{0.f,25.f,100.f,500.f,1799.9999f,1800.f,1800.0001f,1999.9999f,2000.f,2000.0001f,2500.f};
                input.position[0]=distances[sample%11];input.mode=(sample/11)%3;
                if(sample==29){input.radius=101.f;input.max_distance=101.f;input.position[0]=102.01f;}
                if(sample==30){input.radius=std::nextafter(100.f,INFINITY);input.max_distance=2000.125f;input.position[0]=std::nextafter(2000.125f,INFINITY);}
                if(sample==31)input.radius=0;
            }
            if(sample>=32&&sample<1056){
                const unsigned key=sample-32,flags=key&31u;
                input={};input.max_distance=2000;input.radius=50;input.mode=(key>>5)&3u;
                input.detailed_range=(flags&1u)!=0;input.reveal_all=(flags&2u)!=0;
                input.instance_cull_flag=(flags&4u)!=0;input.skip_near_fade=(flags&8u)!=0;
                input.fade_enabled=(flags&16u)!=0;
                const float directed[]{0.f,25.f,100.f,1259.9999f,1400.f,1800.f,2000.f,2500.f};
                input.position[0]=directed[(key>>7)&7u];
            }
            if(sample==27){input.radius=0;input.position[0]=0;input.mode=0;}
            if(dynamic_fade){
                constexpr std::array<std::array<float,3>,4> choices{{
                    {{.5f,2.f,.875f}},{{.9f,.99f,.9f}},
                    {{.25f,1.5f,.5f}},{{.1f,3.f,1.f}}}};
                const auto& chosen=choices[sample%choices.size()];
                input.near_fade_min=chosen[0];input.near_fade_max=chosen[1];
                input.near_fade_amount=chosen[2];
                if(sample<32){
                    input.radius=50.f;input.fade_enabled=true;input.detailed_range=true;
                    input.mode=1;input.max_distance=2000.f;
                    input.position[0]=50.f/(sample&4u?chosen[1]:chosen[0]);
                    if(sample&1u)input.position[0]=std::nextafter(input.position[0],0.f);
                    if(sample&2u)input.position[0]=std::nextafter(input.position[0],INFINITY);
                    input.position[1]=input.position[2]=0.f;
                }
                write(base+0x30444c,chosen);
            }
            put(instance,0x40,input.radius);put(instance,0x84,std::uint8_t(input.instance_cull_flag));
            put(game,0x84,std::uint8_t(input.reveal_all));put(settings,0x40,std::uint32_t(input.detailed_range));
            write(base+0x2fcac0,input.max_distance);write(base+0x36b610,input.mode);write(base+0x3637b5,std::uint8_t(input.skip_near_fade));
            control(cw);
            const bool native_culled=(craft_view_fixture_distance(base+0xd5bb0,instance.data(),input.camera,input.position)&255u)!=0;
            const float native_weighted=get(instance.data()+0x88),native_distance=get(instance.data()+0x8c);
            float native_alpha=1;bool native_written=false;
            if(input.fade_enabled)native_written=(craft_view_fixture_fade(base+0xcaff0,instance.data(),&native_alpha)&255u)!=0;
            need(control()==cw,"native Craft CPU routines retain CW");control(saved);
            CraftViewResult result;
            need(native_craft_view(input,encode_native_x87(cw),result),"owned Craft view policy");
            need(control()==saved,"owned Craft view restores callback CW");
            if(bits(result.weighted_distance)!=bits(native_weighted)||bits(result.distance)!=bits(native_distance)||
               result.culled!=native_culled||result.fade_written!=native_written||
               bits(result.alpha)!=bits(native_alpha)){
                std::fprintf(stderr,"view mismatch cw=%04x sample=%u W=%08lx/%08lx D=%08lx/%08lx C=%u/%u F=%u/%u A=%08lx/%08lx\n",
                    cw,sample,(unsigned long)bits(result.weighted_distance),(unsigned long)bits(native_weighted),
                    (unsigned long)bits(result.distance),(unsigned long)bits(native_distance),result.culled,native_culled,
                    result.fade_written,native_written,(unsigned long)bits(result.alpha),(unsigned long)bits(native_alpha));return 1;}
            ++cases;
        }
    }
    control(saved);
    write(base+0x30444c,old_near_fade);
    write(base+0x2fcac0,old_distance);write(base+0x36b610,old_mode);
    write(base+0x3637b5,old_skip);write(base+0x36b5ac,old_game);
    need(FreeLibrary(image)!=0,"private Craft image released");
    std::printf("PASS native Craft view comparisons=%u across12 PC/RC modes%s; synthetic object/game only; no startup/simulation/graphics\n",
                cases,dynamic_fade?" dynamic near-fade parameters":"");
}
