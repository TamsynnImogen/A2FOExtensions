// Executes only the hash-pinned Craft damage visibility CPU helper in a
// private inspection mapping with owned fake nodes. Never starts the game.
#include "../renderer/native11/craft_visibility.hpp"
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
template<class T> void put(std::vector<BYTE>& object,unsigned offset,const T& value){need(offset+sizeof(T)<=object.size(),"fake object bounds");std::memcpy(object.data()+offset,&value,sizeof(T));}
unsigned short control(){unsigned short cw=0;asm volatile("fnstcw %0":"=m"(cw));return cw;}
void control(unsigned short cw){asm volatile("fldcw %0"::"m"(cw):"memory");}
std::uint32_t bits(float value){std::uint32_t result=0;std::memcpy(&result,&value,4);return result;}
__attribute__((noinline)) float invoke(void* target,void* node,float threshold){
    float result=0;const auto threshold_bits=bits(threshold);
    asm volatile("pushl %[threshold]; pushl %[node]; call *%[target]; addl $8,%%esp; fstps %[result]"
        : [result] "=m"(result)
        : [threshold] "r"(threshold_bits),[node] "r"(node),[target] "r"(target)
        : "eax","ecx","edx","cc","memory","st","st(1)","st(2)","st(3)",
          "st(4)","st(5)","st(6)","st(7)");
    return result;
}
}
int main(int argc,char** argv){
    need(argc==2&&pinned(argv[1]),"exact isolated engine image hash");
    auto image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(image!=nullptr,"private image mapping without startup");
    auto* base=reinterpret_cast<BYTE*>(image);
    const auto delta=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base))-0x400000u;
    const std::uint32_t fixes[][2]{{0xcb340,0x6ae8f0},{0xcb35f,0x6aea10}};
    for(const auto& fix:fixes){std::uint32_t actual=0;std::memcpy(&actual,base+fix[0],4);
        need(actual==fix[1]||actual==fix[1]+delta,"reviewed damage-helper constant operand");
        if(actual!=fix[1]+delta)write(base+fix[0],fix[1]+delta);
    }
    need(FlushInstructionCache(GetCurrentProcess(),base+0xcb330,0x60)!=0,"isolated helper instruction cache");
    const auto original_control=control();std::uint32_t random=0x53c7aa11;unsigned cases=0;
    for(unsigned rounding=0;rounding<4;++rounding){
        const auto cw=static_cast<unsigned short>((original_control&~0xc00u)|(rounding<<10));control(cw);
        for(unsigned count:{1u,2u,3u,7u,17u,64u})for(unsigned shape=0;shape<4;++shape){
            CraftVisibilityInput input;input.root=0;input.nodes.resize(count);
            for(unsigned i=0;i<count;++i){random=random*1664525u+1013904223u;
                input.nodes[i].flags=random&0xffffu;
                if(i){const unsigned parent=random%i;
                    input.nodes[i].parent=parent;
                    input.nodes[i].next_sibling=input.nodes[parent].first_child;
                    input.nodes[parent].first_child=i;
                }
            }
            input.damage_child=input.nodes[0].first_child;
            for(float health:{0.f,.1f,.2f,.3f,.4f,.6f,.8f,.99f,1.f,1.2f,2.f}){
                input.hull_fraction=health;
                std::vector<BYTE> native_nodes(count*0xc0);
                const auto address=[&](std::uint32_t index){return index==UINT32_MAX?0u:
                    static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(native_nodes.data()+index*0xc0));};
                for(unsigned i=0;i<count;++i){
                    put(native_nodes,i*0xc0+0x1c,address(input.nodes[i].first_child));
                    put(native_nodes,i*0xc0+0x20,address(input.nodes[i].next_sibling));
                    put(native_nodes,i*0xc0+0xbc,input.nodes[i].flags);
                }
                const auto native_threshold=invoke(base+0xcb330,
                    reinterpret_cast<void*>(address(input.damage_child)),health);
                need(control()==cw,"native damage helper CW preserved");
                CraftVisibilityResult result;std::string error;
                need(evaluate_craft_visibility(input,result,error),error.c_str());
                need(control()==cw,"owned damage policy CW preserved");
                need(bits(result.damage_threshold)==bits(native_threshold),"native damage threshold bit parity");
                for(unsigned i=0;i<count;++i){std::uint32_t flags=0;
                    std::memcpy(&flags,native_nodes.data()+i*0xc0+0xbc,4);
                    if(flags!=result.flags[i]){std::fprintf(stderr,
                        "damage flags mismatch rc=%u nodes=%u shape=%u health=%g node=%u native=%08lx own=%08lx\n",
                        rounding,count,shape,double(health),i,static_cast<unsigned long>(flags),
                        static_cast<unsigned long>(result.flags[i]));return 1;
                    }
                }
                ++cases;
            }
        }
    }
    control(original_control);need(FreeLibrary(image)!=0,"private inspection map released");
    std::printf("PASS %u native Craft damage visibility comparisons; isolated CPU helper only\n",cases);
}
