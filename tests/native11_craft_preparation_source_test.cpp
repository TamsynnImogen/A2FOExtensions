// Private pinned-image inspection only. No mapped game or hook code is called.
#include "../core/native11_craft_preparation_source.hpp"
#include "../core/native11_craft_preparation_profile.hpp"
#include "../core/native11_craft_preparation_fleet_profile.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace source=a2fo::native11_craft_preparation::source;
namespace root_profile=a2fo::native11_craft_preparation::profile;
namespace fleet_profile=a2fo::native11_craft_preparation::fleet_profile;
namespace {
static_assert(sizeof(void*)==4,"this fixture inspects the 32-bit private images");
HMODULE root_image=nullptr,fleet_image=nullptr;
bool root_exact=true,fleet_exact=true;
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
        for(;;){
            if(!ReadFile(file,block,sizeof(block),&size,nullptr)){good=false;break;}
            if(!size)break;
            if(!CryptHashData(hash,block,size,0)){good=false;break;}
        }
        BYTE actual[32]{};DWORD length=32;
        okay=good&&CryptGetHashParam(hash,HP_HASHVAL,actual,&length,0)&&length==32&&
            std::memcmp(actual,expected,32)==0;
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);return okay;
}
bool module_sha(HMODULE module,const std::uint8_t expected[32]){
    if(!module)return false;
    char path[32768]{};
    const DWORD count=GetModuleFileNameA(module,path,sizeof(path));
    return count&&count<sizeof(path)&&file_sha(path,expected);
}
void write_bytes(void* place,const void* data,std::size_t size){
    DWORD previous=0,ignored=0;
    need(VirtualProtect(place,size,PAGE_EXECUTE_READWRITE,&previous)!=0,"private page writable");
    std::memcpy(place,data,size);
    need(FlushInstructionCache(GetCurrentProcess(),place,size)!=0,"private instruction cache flushed");
    need(VirtualProtect(place,size,previous,&ignored)!=0,"private page protection restored");
}
template<class T>void write(void* place,const T& value){write_bytes(place,&value,sizeof(value));}
struct Fix {BYTE* place;std::uint32_t original;};
template<class Region>void normalize_region(BYTE* base,std::uint32_t preferred,const Region& region,std::vector<Fix>& fixes){
    const std::uint32_t delta=address(base)-preferred;
    for(std::size_t k=0;k<region.relocation_count;++k){
        const auto offset=region.relocations[k];
        need(offset+4u<=region.size,"declared relocation span");
        std::uint32_t expected=0,actual=0;
        std::memcpy(&expected,region.bytes+offset,4);
        std::memcpy(&actual,base+region.rva+offset,4);
        need(actual==expected||actual==expected+delta,"HIGHLOW is original or relocated");
        if(actual==expected&&delta){
            fixes.push_back({base+region.rva+offset,actual});
            write(base+region.rva+offset,expected+delta);
        }
    }
}
void refused(HMODULE fleet,const char* reason){
    const source::Proof sentinel{0x11223344,0x55667788,0x99aabbcc,true};
    auto output=sentinel;
    need(!source::current(root_image,fleet,output)&&source::same(output,sentinel),reason);
}
}
namespace a2fo::native11_capture {
bool exact_engine_image(HMODULE image) noexcept {
    return root_exact&&image==root_image&&module_sha(image,root_sha.data());
}
bool exact_image_file(HMODULE image,const unsigned char expected[32]) noexcept {
    return fleet_exact&&image==fleet_image&&expected&&
        std::memcmp(expected,fleet_profile::sha256.data(),32)==0&&module_sha(image,expected);
}
}
int main(int argc,char** argv){
    need(argc==3&&file_sha(argv[1],root_sha.data())&&file_sha(argv[2],fleet_profile::sha256.data()),
         "exact Root and Fleet files pinned before mapping");
    root_image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    fleet_image=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(root_image&&fleet_image,"private inspection mappings");
    auto* root=reinterpret_cast<BYTE*>(root_image);
    auto* fleet=reinterpret_cast<BYTE*>(fleet_image);
    std::vector<Fix> fixes;
    for(const auto& region:root_profile::logical_regions)
        normalize_region(root,root_profile::preferred_base,region,fixes);
    for(const auto& region:fleet_profile::regions)
        normalize_region(fleet,fleet_profile::preferred_base,region,fixes);
    for(const auto& slot:root_profile::logical_slots){
        std::uint32_t actual=0;auto* place=root+slot[0];std::memcpy(&actual,place,4);
        const auto preferred=root_profile::preferred_base+slot[1];
        const auto relocated=address(root)+slot[1];
        need(actual==preferred||actual==relocated,"declared Root setup slot");
        if(actual!=relocated){fixes.push_back({place,actual});write(place,relocated);}
    }
    auto* entry=root+root_profile::logical_regions[0].rva;
    auto* slot=root+root_profile::logical_slots[0][0];
    auto* callback=fleet+fleet_profile::callback_rva;
    auto* saved_cell=fleet+fleet_profile::saved_original_rva;
    std::array<BYTE,8> original_entry{};std::memcpy(original_entry.data(),entry,8);
    std::uint32_t original_cell=0;std::memcpy(&original_cell,saved_cell,4);
    source::Proof proof{};
    need(source::current(root_image,nullptr,proof)&&proof.image==address(root)&&
         proof.fleet==0&&!proof.fleet_base_wrapper&&proof.gateway==0,
         "unhooked Root setup proves base source");
    need(source::qualify(root_image,nullptr),"unhooked Root identity qualifies");
    root_exact=false;need(!source::qualify(root_image,nullptr),"wrong Root identity refused");root_exact=true;
    fleet_exact=false;need(!source::qualify(root_image,fleet_image),"wrong Fleet identity refused");fleet_exact=true;

    auto* gateway=static_cast<BYTE*>(VirtualAlloc(nullptr,14,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    need(gateway,"private saved-original gateway allocated");
    std::memcpy(gateway,entry,8);
    gateway[8]=0x68;
    const std::uint32_t continuation=address(root)+root_profile::logical_regions[0].rva+8;
    std::memcpy(gateway+9,&continuation,4);gateway[13]=0xc3;
    DWORD previous=0,ignored=0;
    need(VirtualProtect(gateway,14,PAGE_EXECUTE_READ,&previous)!=0,"gateway executable");
    write(saved_cell,address(gateway));
    std::array<BYTE,6> redirect{{0x68,0,0,0,0,0xc3}};
    const std::uint32_t callback_address=address(callback);
    std::memcpy(redirect.data()+1,&callback_address,4);
    write_bytes(entry,redirect.data(),redirect.size());
    need(source::current(root_image,fleet_image,proof)&&proof.image==address(root)&&
         proof.fleet==address(fleet)&&proof.gateway==address(gateway)&&proof.fleet_base_wrapper,
         "exact Fleet wrapper and saved gateway prove base source");
    need(source::qualify(root_image,fleet_image),"exact redirected source qualifies");
    root_exact=false;need(!source::qualify(root_image,fleet_image),"redirected wrong Root refused");root_exact=true;
    fleet_exact=false;need(!source::qualify(root_image,fleet_image),"redirected wrong Fleet refused");fleet_exact=true;

    auto bad=redirect;bad[1]^=1;write_bytes(entry,bad.data(),bad.size());
    refused(fleet_image,"unknown entry target preserves output");write_bytes(entry,redirect.data(),redirect.size());
    const BYTE body=* (entry+8);write(entry+8,static_cast<BYTE>(body^1));
    refused(fleet_image,"Root setup body mutation preserves output");write(entry+8,body);
    const BYTE retained=entry[6];write(entry+6,static_cast<BYTE>(retained^1));
    refused(fleet_image,"retained Root prefix byte preserves output");write(entry+6,retained);
    const BYTE closure=*callback;write(callback,static_cast<BYTE>(closure^1));
    refused(fleet_image,"Fleet closure body mutation preserves output");write(callback,closure);
    const auto& callback_region=fleet_profile::regions[0];
    need(callback_region.relocation_count>0,"Fleet callback has declared HIGHLOW");
    auto* operand=callback+callback_region.relocations[0];const BYTE relocation=*operand;
    write(operand,static_cast<BYTE>(relocation^1));
    refused(fleet_image,"Fleet closure relocation mutation preserves output");write(operand,relocation);
    write(saved_cell,std::uint32_t{0});refused(fleet_image,"zero saved gateway preserves output");
    write(saved_cell,address(gateway));
    const BYTE prefix=gateway[0];write(gateway,static_cast<BYTE>(prefix^1));
    refused(fleet_image,"wrong saved gateway prefix preserves output");write(gateway,prefix);
    const std::uint32_t wrong_continuation=continuation+1;
    write(gateway+9,wrong_continuation);
    refused(fleet_image,"wrong saved continuation preserves output");write(gateway+9,continuation);
    need(VirtualProtect(gateway,14,PAGE_READWRITE,&previous)!=0,"gateway made nonexecutable");
    refused(fleet_image,"nonexecutable gateway preserves output");
    need(VirtualProtect(gateway,14,PAGE_EXECUTE_READ,&ignored)!=0,"gateway execute restored");
    std::uint32_t setup_target=0;std::memcpy(&setup_target,slot,4);
    write(slot,setup_target^1);refused(fleet_image,"wrong setup slot preserves output");write(slot,setup_target);

    auto* display=root+root_profile::regions[0].rva;
    const BYTE display_original=*display;
    write(display,static_cast<BYTE>(display_original^1));
    need(source::current(root_image,fleet_image,proof)&&proof.gateway==address(gateway)&&
         source::qualify(root_image,fleet_image),
         "separate entity display does not invalidate bounded base proof");
    write(display,display_original);
    write_bytes(entry,original_entry.data(),redirect.size());
    write(saved_cell,original_cell);
    for(auto it=fixes.rbegin();it!=fixes.rend();++it)write(it->place,it->original);
    need(VirtualFree(gateway,0,MEM_RELEASE)!=0,"private gateway released");
    // qualify pins Fleet in this process; the private module remains mapped until exit.
    need(FreeLibrary(root_image)!=0,"private Root map released");
    std::puts("native11 craft preparation source private inspection passed");
    return 0;
}
