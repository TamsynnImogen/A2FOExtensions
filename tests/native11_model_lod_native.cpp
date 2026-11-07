// Isolated CPU-only comparison against the exact supported engine image.
// No engine startup, graphics, native object traversal or installed writes.
#include "../renderer/native11/model_lod_selection.hpp"
#include "../renderer/native11/native_matrix34.hpp"
#include "../core/native11_template_profile.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool okay,const char* message){if(!okay){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
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
template<class T> void put(std::vector<BYTE>& bytes,std::size_t offset,T value){
    need(offset+sizeof(T)<=bytes.size(),"fake object bounds");std::memcpy(bytes.data()+offset,&value,sizeof(T));
}
std::uint32_t ptr(const void* value){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value));}
template<class T> void write(void* destination,const T& value){DWORD old=0;
    need(VirtualProtect(destination,sizeof(T),PAGE_READWRITE,&old)!=0,"private write admission");
    std::memcpy(destination,&value,sizeof(T));DWORD ignored=0;
    need(VirtualProtect(destination,sizeof(T),old,&ignored)!=0,"private protection restore");
}
unsigned short control(){unsigned short word=0;asm volatile("fnstcw %0":"=m"(word));return word;}
void control(unsigned short word){asm volatile("fldcw %0"::"m"(word):"memory");}
__attribute__((noinline)) void invoke_lod(void* target,void* node,std::int32_t requested){
    asm volatile("pushl %[requested]; call *%[target]"
        : "+c"(node):[target]"r"(target),[requested]"r"(requested)
        :"eax","edx","cc","memory","st","st(1)","st(2)","st(3)","st(4)");
}
__attribute__((noinline)) const float* invoke_scale(void* target,void* node){
    const float* result=nullptr;
    asm volatile("call *%[target]":"=a"(result),"+c"(node):[target]"r"(target)
        :"edx","cc","memory","st","st(1)");
    return result;
}
void relocate(BYTE* base){
    const std::uint32_t fixes[][2]={{0x230a6a,0x7ad508},{0x230a99,0x7ad634},
        {0x230a9f,0x7ad638},{0x230aa7,0x7ad63c},{0x230ace,0x7ad508}};
    const auto delta=ptr(base)-0x400000u;
    for(const auto& fix:fixes){std::uint32_t actual=0;std::memcpy(&actual,base+fix[0],4);
        need(actual==fix[1]||actual==fix[1]+delta,"reviewed absolute LOD operand");
        if(actual!=fix[1]+delta)write(base+fix[0],fix[1]+delta);
    }
    need(FlushInstructionCache(GetCurrentProcess(),base+0x230a50,0x160)!=0,"private instruction cache");
}
BYTE* camera_translation_fixture(BYTE* base){
    namespace profile=a2fo::native11_animation_clock::template_profile;
    // Only the reviewed straight-line CPU block, plus RET. The original block
    // has no calls, branches, frame accesses or object writes; EBX is camera.
    const auto& source=profile::camera_translation_0_bytes;
    auto* code=static_cast<BYTE*>(VirtualAlloc(nullptr,source.size()+1,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    need(code!=nullptr,"private camera CPU block allocation");
    std::memcpy(code,source.data(),source.size());code[source.size()]=0xc3;
    for(auto offset:profile::camera_translation_0_fixes){std::uint32_t address=0;
        std::memcpy(&address,code+offset,4);
        need(address>=0x7ad634&&address<=0x7ad66c,"camera block only reviewed world/result globals");
        write(code+offset,address-0x400000u+ptr(base));
    }
    DWORD old=0;need(VirtualProtect(code,source.size()+1,PAGE_EXECUTE_READ,&old)!=0&&
        FlushInstructionCache(GetCurrentProcess(),code,source.size()+1)!=0,"private camera CPU block RX");
    return code;
}
__attribute__((noinline)) void invoke_camera(void* target,void* camera){
    asm volatile("call *%[target]":"+b"(camera):[target]"r"(target)
        :"cc","memory","st","st(1)");
}
}
int main(int argc,char** argv){
    need(argc==2&&pinned(argv[1]),"exact isolated image hash");
    const auto image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(image!=nullptr,"private image mapping without startup");
    auto* base=reinterpret_cast<BYTE*>(image);relocate(base);
    auto* camera_code=camera_translation_fixture(base);
    const auto saved_cw=control();
    const std::uint32_t old_engine=*reinterpret_cast<std::uint32_t*>(base+0x3ad508);
    const auto old_translation=*reinterpret_cast<std::array<float,3>*>(base+0x3ad634);
    const auto old_world=*reinterpret_cast<NativeMatrix34*>(base+0x3ad640);
    std::vector<BYTE> camera(0x120);
    std::vector<BYTE> engine(0x108),context(0x44),node(0xe0),sentinel(12);
    put(engine,0x104,ptr(context.data()));write(base+0x3ad508,ptr(engine.data()));
    using Multiply=void(__cdecl*)(float*,const float*,const float*);
    const auto multiply=reinterpret_cast<Multiply>(base+0x65940);
    const unsigned pcs[]={0,0x200,0x300};
    std::uint32_t random=0x62e371ab;unsigned products=0,scales=0,lods=0;
    for(auto pc:pcs)for(unsigned rc=0;rc<4;++rc){
        const auto cw=static_cast<unsigned short>((saved_cw&~0xf00u)|pc|(rc<<10)|0x3fu);control(cw);
        for(unsigned sample=0;sample<2048;++sample){
            NativeMatrix34 local{},parent{},native{},owned{};std::string error;
            for(auto* matrix:{&local,&parent})for(float& value:*matrix){
                random=random*1664525u+1013904223u;
                value=static_cast<float>(static_cast<int>(random%2000001u)-1000000)/4096.f;
            }
            if(sample%4==0){local[0]=1048576.f;local[1]=-1048576.f;local[2]=.000001f;
                parent[0]=1;parent[3]=1;parent[6]=1;}
            multiply(native.data(),local.data(),parent.data());
            need(control()==cw,"original multiply retains CW");
            need(native_matrix34_multiply(local,parent,owned,error),error.c_str());
            if(std::memcmp(native.data(),owned.data(),sizeof(native))){
                std::fprintf(stderr,"Matrix34 mismatch CW=%04x sample=%u\n",cw,sample);return 1;}
            need(control()==cw,"owned multiply retains CW");++products;
            write(base+0x3ad640,native);std::memcpy(camera.data()+0xf0,parent.data(),sizeof(parent));
            invoke_camera(camera_code,camera.data());
            std::array<float,3> camera_owned{};
            need(native_matrix34_camera_translation(native,parent,camera_owned,error),error.c_str());
            need(std::memcmp(base+0x3ad634,camera_owned.data(),sizeof(camera_owned))==0,
                "camera translation exact original CPU association");
            need(control()==cw,"both camera kernels retain CW");
            const float scale_values[]={1,0,-1,.125f,1.33333337f,-10.75f};
            const float scale=scale_values[sample%6];
            std::memcpy(node.data()+0x24,local.data(),sizeof(local));put(node,0x54,scale);
            const float* result=invoke_scale(base+0x239460,node.data());
            need(result==reinterpret_cast<const float*>(node.data()+(scale==1?0x24:0x58)),"original scale return pointer");
            need(native_matrix34_scale(local,scale,owned,error),error.c_str());
            need(std::memcmp(result,owned.data(),sizeof(owned))==0,"native scaled local exact binary32");
            need(control()==cw,"both scale kernels retain CW");++scales;
        }
        const std::array<std::array<float,4>,3> thresholds{{{{1000,400,160,40}},{{0,0,0,0}},{{40,1000,0,160}}}};
        for(const auto& limits:thresholds)for(unsigned count:{0u,1u,2u,5u,7u}){
            std::vector<BYTE> cells(count*12),descriptors(count*8),levels(count*0xc0);
            put(sentinel,0,count?ptr(cells.data()):ptr(sentinel.data()));
            put(sentinel,4,count?ptr(cells.data()+(count-1)*12):ptr(sentinel.data()));
            for(unsigned i=0;i<count;++i){
                put(cells,i*12,i+1==count?ptr(sentinel.data()):ptr(cells.data()+(i+1)*12));
                put(cells,i*12+4,i?ptr(cells.data()+(i-1)*12):ptr(sentinel.data()));
                put(cells,i*12+8,ptr(descriptors.data()+i*8));
                put(descriptors,i*8,ptr(levels.data()+i*0xc0));put(descriptors,i*8+4,i*7u);
            }
            put(node,0xc8,ptr(sentinel.data()));put(node,0xcc,count);
            std::memcpy(context.data()+0x30,limits.data(),16);
            for(unsigned sample=0;sample<29;++sample)for(int quality:{-2,0,1,2,4,9})
            for(int requested:{INT_MIN,-2,-1,0,1,3,7,INT_MAX})for(bool force:{false,true}){
                ModelLodSelectionInput input;input.level_count=count;input.thresholds=limits;
                input.quality_cap=quality;input.requested=requested;input.force_highest=force;
                const float distance_values[]={0,40,std::nextafter(40.f,0.f),std::nextafter(40.f,100.f),
                    160,400,1000,1200,1e-20f,1e20f,3.0000002f,99.99999f,1.0000001f,20};
                input.translation={{distance_values[sample%14],sample>=14?.1f:0.f,sample>=14?.333f:0.f}};
                input.centre={{sample%3==0?.01f:0.f,sample%5==0?-.25f:0.f,0.f}};
                input.radius=sample%4==0?.1f:0.f;
                if(sample==28){
                    // Extended first comparison is below 40; the binary32
                    // nonpopping store rounds the same distance to 40.
                    input.translation={{std::nextafter(40.f,0.f),.014f,0.f}};
                    input.centre={{0.f,0.f,0.f}};input.radius=0;
                }
                write(base+0x3ad634,input.translation);put(engine,0xec,static_cast<BYTE>(force?0x80:0));
                put(context,0x40,quality);std::memcpy(node.data()+0xd0,input.centre.data(),12);
                put(node,0xdc,input.radius);
                std::uint32_t owned=123;std::string error;need(select_model_lod(input,owned,error),error.c_str());
                if(sample==28&&pc==0x300&&rc==0&&limits[3]==40&&count==5&&
                   quality==4&&requested==-1&&!force){
                    const float rounded=static_cast<float>(std::sqrt(
                        double(input.translation[0])*input.translation[0]+
                        double(input.translation[1])*input.translation[1]));
                    need(rounded==40.f&&owned==4,
                         "extended first threshold differs from rounded float-only selection");
                }
                const unsigned mode=sample%3;
                const bool has_prior=count&&(mode==1||(mode==2&&count>1));
                const unsigned prior=has_prior?(mode==1?owned:(owned+1)%count):0;
                const std::uint32_t before=has_prior?ptr(descriptors.data()+prior*8):0u;
                put(node,0xc0,before);
                std::vector<std::uint32_t> expected_flags(count,0xf7u);
                if(has_prior){
                    // Equal-selected is a no-op even with both hidden bits set.
                    expected_flags[prior]=mode==1&&((sample/3)%2==0)?0xf7u:0xf4u;
                }
                for(unsigned i=0;i<count;++i)put(levels,i*0xc0+0xbc,expected_flags[i]);
                invoke_lod(base+0x230a50,node.data(),requested);
                need(control()==cw,"original LOD retains CW");
                std::uint32_t selected=0;std::memcpy(&selected,node.data()+0xc0,4);
                const auto expected=count?ptr(descriptors.data()+owned*8):0u;
                if((count&&owned>=count)||selected!=expected){
                    std::fprintf(stderr,"LOD mismatch CW=%04x sample=%u count=%u quality=%d requested=%d force=%u owned=%u actual=%08lx expected=%08lx\n",
                        cw,sample,count,quality,requested,force,owned,static_cast<unsigned long>(selected),static_cast<unsigned long>(expected));return 1;}
                need(control()==cw,"owned LOD retains CW");
                if(before!=expected){
                    if(has_prior)expected_flags[prior]|=3u;
                    if(count)expected_flags[owned]&=~3u;
                }
                for(unsigned i=0;i<count;++i){std::uint32_t flags=0;std::memcpy(&flags,levels.data()+i*0xc0+0xbc,4);
                    need(flags==expected_flags[i],"native previous and selected flags in list order");}
                ++lods;
            }
        }
        std::printf("CW=%04x cumulative products=%u scales=%u LODs=%u\n",cw,products,scales,lods);
    }
    control(saved_cw);write(base+0x3ad508,old_engine);write(base+0x3ad634,old_translation);
    write(base+0x3ad640,old_world);VirtualFree(camera_code,0,MEM_RELEASE);
    FreeLibrary(image);std::printf("PASS native Matrix34 products=%u scaled locals=%u LOD selections=%u; no game startup\n",products,scales,lods);return 0;
}
