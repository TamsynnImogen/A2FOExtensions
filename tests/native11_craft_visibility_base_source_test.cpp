// Private pinned-image inspection only. No mapped game or hook code is called.
#include "../core/native11_craft_visibility_base_source.hpp"
#include "../core/native11_craft_visibility_base_profile.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace source=a2fo::native11_craft_visibility_base::source;
namespace profile=a2fo::native11_craft_visibility_base::profile;
namespace {
static_assert(sizeof(void*)==4,"this fixture inspects the 32-bit private image");
HMODULE image=nullptr;
bool admitted=true;
constexpr std::array<std::uint8_t,32> root_sha{{0x43,0x00,0xab,0x71,0x73,0x1e,0x32,0x3f,
    0x57,0x67,0xd8,0xee,0x05,0x45,0x7e,0x64,0x44,0x40,0x1b,0x82,0x32,0x53,
    0xae,0xbe,0xe8,0x6c,0x5a,0xc2,0x3c,0x2b,0x00,0xde}};
void need(bool okay,const char* reason){
    if(!okay){std::fprintf(stderr,"FAIL %s (winerr=%lu)\n",reason,GetLastError());std::exit(1);}
}
std::uint32_t address(const void* p){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));}
bool file_sha(const char* path,const std::uint8_t expected[32]){
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool okay=false;
    if(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&
       CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
        BYTE block[16384]{};DWORD size=0;bool good=true;
        for(;;){if(!ReadFile(file,block,sizeof(block),&size,nullptr)){good=false;break;}
            if(!size)break;
            if(!CryptHashData(hash,block,size,0)){good=false;break;}}
        BYTE actual[32]{};DWORD length=32;
        okay=good&&CryptGetHashParam(hash,HP_HASHVAL,actual,&length,0)&&length==32&&
            std::memcmp(actual,expected,32)==0;
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);return okay;
}
bool module_sha(HMODULE module){
    if(!module)return false;
    char path[32768]{};
    const DWORD count=GetModuleFileNameA(module,path,sizeof(path));
    return count&&count<sizeof(path)&&file_sha(path,root_sha.data());
}
void write_bytes(void* place,const void* data,std::size_t size){
    DWORD previous=0,ignored=0;
    need(VirtualProtect(place,size,PAGE_EXECUTE_READWRITE,&previous)!=0,"private page writable");
    std::memcpy(place,data,size);
    need(FlushInstructionCache(GetCurrentProcess(),place,size)!=0,"private instruction cache flushed");
    need(VirtualProtect(place,size,previous,&ignored)!=0,"private page protection restored");
}
template<class T>void write(void* place,const T& value){write_bytes(place,&value,sizeof(value));}
void rejected(const char* reason){need(!source::current(image),reason);}
}
namespace a2fo::native11_capture {
bool exact_engine_image(HMODULE candidate) noexcept {
    return admitted&&candidate==image&&module_sha(candidate);
}
}
int main(int argc,char** argv){
    need(argc==2&&file_sha(argv[1],root_sha.data()),"exact Root file pinned before mapping");
    image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(image,"private inspection mapping");
    auto* base=reinterpret_cast<BYTE*>(image);
    const std::uint32_t delta=address(base)-profile::preferred_base;
    for(const auto& region:profile::regions)for(std::size_t k=0;k<region.relocation_count;++k){
        const auto offset=region.relocations[k];need(offset+4u<=region.size,"declared HIGHLOW span");
        std::uint32_t preferred=0,actual=0;std::memcpy(&preferred,region.bytes+offset,4);
        std::memcpy(&actual,base+region.rva+offset,4);
        need(actual==preferred||actual==preferred+delta,"HIGHLOW is original or relocated");
        if(actual!=preferred+delta)write(base+region.rva+offset,preferred+delta);
    }
    for(const auto& slot:profile::slots){
        const std::uint32_t target=address(base)+slot[1];std::uint32_t actual=0;
        std::memcpy(&actual,base+slot[0],4);
        need(actual==profile::preferred_base+slot[1]||actual==target,"declared setup slot");
        if(actual!=target)write(base+slot[0],target);
    }
    need(source::current(image),"pinned unmodified base guard passes");
    admitted=false;need(!source::qualify(image),"qualify rejects failed file admission");admitted=true;
    need(source::qualify(image),"exact admitted base guard qualifies");

    for(const auto& region:profile::regions){
        auto* first=base+region.rva;const BYTE first_saved=*first;
        write(first,static_cast<BYTE>(first_saved^1));rejected("region first byte mutation rejected");write(first,first_saved);
        auto* last=first+region.size-1;const BYTE last_saved=*last;
        write(last,static_cast<BYTE>(last_saved^1));rejected("region last byte mutation rejected");write(last,last_saved);
        for(std::size_t k=0;k<region.relocation_count;++k){
            auto* operand=first+region.relocations[k];BYTE saved[4]{};std::memcpy(saved,operand,4);
            BYTE changed[4];std::memcpy(changed,saved,4);changed[0]^=1;
            write_bytes(operand,changed,4);rejected("HIGHLOW mutation rejected");write_bytes(operand,saved,4);
        }
    }
    for(const auto& slot:profile::slots){
        auto* place=base+slot[0];std::uint32_t saved=0;std::memcpy(&saved,place,4);
        write(place,saved^1u);rejected("setup slot mutation rejected");write(place,saved);
    }
    for(const auto& region:profile::regions)if(!region.code){
        auto* place=base+region.rva;const BYTE saved=*place;
        write(place,static_cast<BYTE>(saved^1));rejected("data constant mutation rejected");write(place,saved);
    }
    const auto& code_region=profile::regions[0];auto* code=base+code_region.rva;
    DWORD prior=0;need(VirtualProtect(code,code_region.size,PAGE_READWRITE,&prior)!=0,"code region made nonexecutable");
    rejected("nonexecutable code region rejected");DWORD ignored=0;
    need(VirtualProtect(code,code_region.size,prior,&ignored)!=0,"code protection restored");
    need(source::current(image),"guard recovers after mutations and protection restore");

    // Material callback and final-draw bytes are intentionally outside this base-policy profile.
    for(const std::uint32_t rva:{0x0cb2abu,0x0cb318u}){
        auto* place=base+rva;const BYTE saved=*place;
        write(place,static_cast<BYTE>(saved^1));
        need(source::current(image)&&source::qualify(image),"excluded callback/final-draw mutation leaves base guard valid");
        write(place,saved);
    }
    need(FreeLibrary(image)!=0,"private Root map released");
    std::puts("native11 craft visibility base source private inspection passed");
    return 0;
}
