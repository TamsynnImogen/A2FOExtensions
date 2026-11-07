// Execute only pinned native CPU sprite vertex construction on synthetic data.
// All virtual state/draw calls are assembly recorders; no game or native GPU.
#include "../renderer/native11/sprite_colour_policy.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include "../core/native11_sprite_profile.hpp"
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
void sprite_fixture_invoke(void*,void*,const std::uint32_t*);
void sprite_fixture_state0();void sprite_fixture_state1();void sprite_fixture_draw();
std::uint32_t sprite_state0_calls=0,sprite_state1_calls=0,sprite_draw_calls=0;
std::uint32_t sprite_state0_value=9,sprite_state1_value=9;
std::uint32_t sprite_draw_this=0,sprite_draw_vertices=0,sprite_draw_count=0;
}
asm(".text\n.globl _sprite_fixture_invoke\n_sprite_fixture_invoke:\n"
    "pushl %ebp\nmovl %esp,%ebp\npushl %ebx\nmovl 8(%ebp),%ebx\n"
    "movl 12(%ebp),%ecx\nmovl 16(%ebp),%edx\n"
    "pushl 24(%edx)\npushl 20(%edx)\npushl 16(%edx)\npushl 12(%edx)\n"
    "pushl 8(%edx)\npushl 4(%edx)\npushl 0(%edx)\ncall *%ebx\npopl %ebx\nleave\nret\n"
    ".globl _sprite_fixture_state0\n_sprite_fixture_state0:\n"
    "incl _sprite_state0_calls\nmovl 4(%esp),%eax\nmovl %eax,_sprite_state0_value\nret $4\n"
    ".globl _sprite_fixture_state1\n_sprite_fixture_state1:\n"
    "incl _sprite_state1_calls\nmovl 4(%esp),%eax\nmovl %eax,_sprite_state1_value\nret $4\n"
    ".globl _sprite_fixture_draw\n_sprite_fixture_draw:\n"
    "incl _sprite_draw_calls\nmovl %ecx,_sprite_draw_this\n"
    "movl 4(%esp),%eax\nmovl %eax,_sprite_draw_vertices\n"
    "movl 8(%esp),%eax\nmovl %eax,_sprite_draw_count\nret $8\n");
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
std::uint32_t bits(float v){std::uint32_t b=0;std::memcpy(&b,&v,4);return b;}
template<class T>void write(void* p,const T& value){
    DWORD old=0,ignored=0;need(VirtualProtect(p,sizeof(value),PAGE_EXECUTE_READWRITE,&old)!=0,"private write protect");
    std::memcpy(p,&value,sizeof(value));need(VirtualProtect(p,sizeof(value),old,&ignored)!=0,"private write restore");
}
template<class T,std::size_t N>void put(std::array<BYTE,N>& out,std::size_t offset,const T& value){
    need(offset+sizeof(value)<=N,"synthetic object extent");std::memcpy(out.data()+offset,&value,sizeof(value));
}
unsigned short control(){unsigned short cw=0;asm volatile("fnstcw %0":"=m"(cw));return cw;}
void control(unsigned short cw){asm volatile("fldcw %0"::"m"(cw):"memory");}
struct Vertex {float x,y,z,rhw;std::uint32_t colour;float u,v;};
static_assert(sizeof(Vertex)==28);
}
int main(int argc,char** argv){
    need(argc==2&&pinned(argv[1]),"exact pinned CPU sprite image");
    auto image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(image!=nullptr,"private image without game startup");auto* base=reinterpret_cast<BYTE*>(image);
    const auto delta=ptr(image)-0x400000u;
    namespace profile=a2fo::native11_sprites::profile;
    for(const auto& region:profile::regions){
        for(std::size_t i=0;i<region.relocation_count;++i){
            const auto offset=region.relocations[i];std::uint32_t original=0,actual=0;
            std::memcpy(&original,region.bytes+offset,4);std::memcpy(&actual,base+region.rva+offset,4);
            need(actual==original||actual==original+delta,"private sprite HIGHLOW before normalization");
            if(actual==original)write(base+region.rva+offset,original+delta);
        }
        std::vector<BYTE> normalized(region.size);std::memcpy(normalized.data(),base+region.rva,region.size);
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
            std::uint32_t value=0;std::memcpy(&value,normalized.data()+offset,4);value-=delta;
            std::memcpy(normalized.data()+offset,&value,4);}
        need(std::memcmp(normalized.data(),region.bytes,region.size)==0,"exact CPU sprite region");
    }
    std::array<std::uint32_t,0xa8/4> vtable{};
    vtable[0x88/4]=ptr(base+0x2254a0);vtable[0xa0/4]=ptr(reinterpret_cast<void*>(&sprite_fixture_state0));
    vtable[0xa4/4]=ptr(reinterpret_cast<void*>(&sprite_fixture_state1));
    vtable[0x80/4]=ptr(reinterpret_cast<void*>(&sprite_fixture_draw));
    std::array<BYTE,4> device{};put(device,0,ptr(vtable.data()));
    std::array<BYTE,0x60> sprite{};std::array<BYTE,0x34> material{};
    put(sprite,0x54,ptr(material.data()));
    // Texture zero deliberately bypasses only native padded-texture UV adjustment.
    put(sprite,0x58,std::uint32_t(0));
    std::array<Vertex,4> vertices{};write(base+0x3ad6f0,ptr(vertices.data()));
    const auto saved=control();std::uint32_t random=0x62559021u;unsigned comparisons=0;
    for(unsigned pc:{0u,2u,3u})for(unsigned rc=0;rc<4;++rc){
        const auto cw=static_cast<unsigned short>((saved&~0xf00u)|(pc<<8)|(rc<<10)|0x3fu);
        for(unsigned sample=0;sample<4096;++sample){
            std::array<float,4> rgba{};
            for(float& v:rgba){random=random*1664525u+1013904223u;v=float(random&65535u)/65535.f;}
            if(sample<16)for(unsigned i=0;i<4;++i)rgba[i]=(sample&(1u<<i))?1.f:0.f;
            if(sample==16)rgba.fill(.5f);
            if(sample>=17&&sample<782){
                const auto index=sample-17,n=index/3,side=index%3;
                float edge=float(double(n)+.5)/255.f;
                if(side==0)edge=std::nextafter(edge,0.f);
                if(side==2)edge=std::nextafter(edge,1.f);
                rgba.fill(edge);
            }
            put(sprite,0x24,rgba[0]);put(sprite,0x28,rgba[1]);put(sprite,0x2c,rgba[2]);put(material,0x24,rgba[3]);
            const std::array<float,3> position{{2.25f,3.5f,.25f}};const std::array<float,2> size{{13.75f,21.25f}};
            const std::array<std::uint32_t,7> args{{ptr(sprite.data()),ptr(position.data()),ptr(size.data()),bits(.125f),bits(.25f),bits(.5f),bits(.625f)}};
            const auto draws=sprite_draw_calls,s0=sprite_state0_calls,s1=sprite_state1_calls;
            control(cw);sprite_fixture_invoke(base+0x225590,device.data(),args.data());
            need(control()==cw,"native sprite retains original CW");control(saved);
            need(sprite_draw_calls==draws+1&&sprite_state0_calls==s0+1&&sprite_state1_calls==s1+1&&
                 sprite_state0_value==0&&sprite_state1_value==1&&sprite_draw_this==ptr(device.data())&&
                 sprite_draw_vertices==ptr(vertices.data())&&sprite_draw_count==4,"only expected synthetic state/draw calls");
            std::uint32_t colour=0;need(native_sprite_colour(rgba.data(),rgba[3],encode_native_x87(cw),colour),"owned native sprite colour");
            need(control()==saved,"owned colour restores callback CW");
            for(const auto& vertex:vertices){
                if(vertex.colour!=colour){std::fprintf(stderr,"RGBA mismatch cw=%04x sample=%u native=%08lx owned=%08lx\n",cw,sample,(unsigned long)vertex.colour,(unsigned long)colour);return 1;}
                need(vertex.z==.25f&&vertex.rhw==1.f,"native screen depth and RHW");}
            need(vertices[0].x==2.25f&&vertices[0].y==3.5f&&vertices[1].x==16.f&&vertices[1].y==3.5f&&
                 vertices[2].x==16.f&&vertices[2].y==24.75f&&vertices[3].x==2.25f&&vertices[3].y==24.75f,"native screen corner ordering");
            need(vertices[0].u==.125f&&vertices[0].v==.25f&&vertices[1].u==.625f&&vertices[1].v==.25f&&
                 vertices[2].u==.625f&&vertices[2].v==.875f&&vertices[3].u==.125f&&vertices[3].v==.875f,"native source UV deltas and corner order");
            ++comparisons;
        }
    }
    control(saved);need(FreeLibrary(image)!=0,"private image released");
    std::printf("PASS native sprite CPU ARGB comparisons=%u vertices=%u; synthetic state/draw only; no game or GPU startup\n",comparisons,comparisons*4);
}
