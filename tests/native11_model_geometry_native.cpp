// Isolated original mesh-scaling CPU routine with preallocated synthetic data.
// No engine startup, graphics, installed writes or real engine object mutation.
#include "../renderer/native11/model_geometry_policy.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace a2fo::native11;
extern "C" {
void geometry_fixture_invalidate();
std::uint32_t geometry_invalidations=0,geometry_last_mask=0,geometry_last_this=0;
}
asm(".text\n.globl _geometry_fixture_invalidate\n_geometry_fixture_invalidate:\n"
    "incl _geometry_invalidations\nmovl %ecx,_geometry_last_this\n"
    "movl 4(%esp),%eax\nmovl %eax,_geometry_last_mask\nret $4\n");
namespace {
void need(bool yes,const char* message){if(!yes){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
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
std::uint32_t ptr(const void* value){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value));}
std::uint32_t bits(float value){std::uint32_t result;std::memcpy(&result,&value,4);return result;}
template<class T> void put(std::vector<BYTE>& bytes,std::size_t offset,const T& value){
    need(offset+sizeof(T)<=bytes.size(),"synthetic mesh bounds");std::memcpy(bytes.data()+offset,&value,sizeof(value));
}
unsigned short control(){unsigned short cw=0;asm volatile("fnstcw %0":"=m"(cw));return cw;}
void control(unsigned short cw){asm volatile("fldcw %0"::"m"(cw):"memory");}
__attribute__((noinline)) void invoke_scale(void* target,void* mesh,std::uint32_t scale){
    asm volatile("pushl %[scale]; pushl %[scale]; pushl %[scale]; call *%[target]"
        :"+c"(mesh):[scale]"r"(scale),[target]"r"(target)
        :"eax","edx","cc","memory","st","st(1)","st(2)","st(3)","st(4)","st(5)","st(6)","st(7)");
}
}
int main(int argc,char** argv){
    need(argc==2&&pinned(argv[1]),"exact isolated image hash");
    const auto image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(image!=nullptr,"private image map without startup");
    auto* base=reinterpret_cast<BYTE*>(image);
    std::vector<BYTE> mesh(0x104);std::array<std::uint32_t,3> vtable{{0,0,ptr(reinterpret_cast<void*>(&geometry_fixture_invalidate))}};
    put(mesh,0,ptr(vtable.data()));
    const auto saved=control();unsigned cases=0,points=0;std::uint32_t random=0x6337a021;
    for(unsigned pc:{0u,2u,3u})for(unsigned rc=0;rc<4;++rc){
        const auto cw=static_cast<unsigned short>((saved&~0xf00u)|(pc<<8)|(rc<<10)|0x3fu);
        for(unsigned sample=0;sample<1024;++sample){
            const unsigned count=1+(sample%31);
            std::vector<float> original(count*3),native(count*3),owned;
            for(float& value:original){random=random*1664525u+1013904223u;
                value=static_cast<float>(static_cast<int>(random%2000001u)-1000000)/1024.f;}
            if(sample%5==0)original[0]=-0.f;
            owned=original;const auto immutable=original;
            put(mesh,0xc0,ptr(native.data()));put(mesh,0xc8,count);put(mesh,0xfc,ptr(original.data()));
            const float scales[]={1.f,.001f,.5f,1.33333337f,3.14159274f,16.f,1e-40f};
            const auto scalar_bits=bits(scales[sample%7]);
            control(cw);const auto calls=geometry_invalidations;
            invoke_scale(base+0x2337a0,mesh.data(),scalar_bits);
            need(control()==cw,"native scale retains CW");
            need(geometry_invalidations==calls+1&&geometry_last_mask==7&&geometry_last_this==ptr(mesh.data()),
                 "native scale makes one invalidation with mask7 on synthetic object only");
            control(saved);std::string error;
            need(apply_model_geometry_policy({scalar_bits,encode_native_x87(cw)},owned.data(),count,error),error.c_str());
            need(control()==saved,"owned scale restores callback CW");
            if(std::memcmp(owned.data(),native.data(),owned.size()*sizeof(float))){
                std::fprintf(stderr,"geometry mismatch CW=%04x sample=%u count=%u scalar=%08lx\n",cw,sample,count,
                    static_cast<unsigned long>(scalar_bits));return 1;}
            need(std::memcmp(original.data(),immutable.data(),original.size()*sizeof(float))==0,"original saved coordinates remain unchanged");
            // Native Mesh::Scale uses retained original positions on later
            // calls; it must not compound a previous scale into its source.
            const auto second_bits=bits(.75f);control(cw);
            invoke_scale(base+0x2337a0,mesh.data(),second_bits);control(saved);
            owned=original;
            need(apply_model_geometry_policy({second_bits,encode_native_x87(cw)},owned.data(),count,error),error.c_str());
            need(std::memcmp(owned.data(),native.data(),owned.size()*sizeof(float))==0,"repeat scales from original source, never previous output");
            cases+=2;points+=count*2;
        }
    }
    control(saved);FreeLibrary(image);
    std::printf("PASS native loader vertex scale comparisons=%u vertices=%u; preallocated buffers and synthetic invalidate only; no game startup\n",cases,points);
}
