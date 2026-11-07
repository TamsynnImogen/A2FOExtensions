// Private PE inspection only: no Fleet callback or engine function is executed.
#include "../core/native11_fofs_stream.hpp"
#include "../core/native11_fofs_stream_profile.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
namespace fofs=a2fo::native11_fofs_stream;
namespace profile=a2fo::native11_fofs_stream::profile;
namespace {
HMODULE root_image=nullptr,fleet_image=nullptr;bool root_exact=true,fleet_exact=true;
void need(bool okay,const char* why){if(!okay){std::fprintf(stderr,"FAIL %s winerr=%lu\n",why,GetLastError());std::exit(1);}}
std::uint32_t address(const void* p){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));}
bool pinned(const char* path,const std::array<std::uint8_t,32>& expected){
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool okay=false;
    if(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&
       CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
        BYTE block[16384]{};DWORD count=0;bool good=true;
        while(true){if(!ReadFile(file,block,sizeof(block),&count,nullptr)){good=false;break;}
            if(!count)break;
            if(!CryptHashData(hash,block,count,0)){good=false;break;}}
        BYTE actual[32]{};DWORD size=32;
        okay=good&&CryptGetHashParam(hash,HP_HASHVAL,actual,&size,0)&&size==32&&
            std::memcmp(actual,expected.data(),32)==0;
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);return okay;
}
template<class T>void write(void* place,const T& value){
    DWORD prior=0,ignored=0;
    need(VirtualProtect(place,sizeof(T),PAGE_EXECUTE_READWRITE,&prior)!=0,"private image writable");
    std::memcpy(place,&value,sizeof(T));
    need(FlushInstructionCache(GetCurrentProcess(),place,sizeof(T))!=0,"private instruction cache flushed");
    need(VirtualProtect(place,sizeof(T),prior,&ignored)!=0,"private image protection restored");
}
struct Fix {BYTE* place;std::uint32_t old;};
void normalize_fleet(std::vector<Fix>& changed){
    const auto& region=profile::fopen_callback;
    auto* base=reinterpret_cast<BYTE*>(fleet_image);
    const auto delta=address(base)-profile::fleet_preferred_base;
    for(std::size_t i=0;i<region.relocation_count;++i){
        const auto off=region.relocations[i];std::uint32_t original=0,actual=0;
        std::memcpy(&original,region.bytes+off,4);
        std::memcpy(&actual,base+region.rva+off,4);
        need(actual==original||actual==original+delta,"Fleet HIGHLOW original or relocated");
        if(actual==original&&delta){changed.push_back({base+region.rva+off,actual});
            write(base+region.rva+off,original+delta);}
    }
}
}
namespace a2fo::native11_capture {
bool exact_engine_image(HMODULE image) noexcept{return root_exact&&image==root_image;}
bool exact_image_file(HMODULE image,const unsigned char expected[32]) noexcept {
    return fleet_exact&&image==fleet_image&&expected&&
        std::memcmp(expected,profile::fleet_sha256.data(),32)==0;
}
}
int main(int argc,char** argv){
    constexpr std::array<std::uint8_t,32> root_sha{{0x43,0x00,0xab,0x71,0x73,0x1e,0x32,0x3f,
        0x57,0x67,0xd8,0xee,0x05,0x45,0x7e,0x64,0x44,0x40,0x1b,0x82,0x32,0x53,
        0xae,0xbe,0xe8,0x6c,0x5a,0xc2,0x3c,0x2b,0x00,0xde}};
    need(argc==3&&pinned(argv[1],root_sha)&&pinned(argv[2],profile::fleet_sha256),
         "both exact PE inputs hash-pinned before mapping");
    root_image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    fleet_image=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(root_image&&fleet_image,"private Root and Fleet inspection mappings");
    std::vector<Fix> fleet_fixes;normalize_fleet(fleet_fixes);
    auto* site=reinterpret_cast<BYTE*>(root_image)+profile::open_call_rva;
    auto* callback=reinterpret_cast<BYTE*>(fleet_image)+profile::fopen_callback.rva;
    std::array<BYTE,6> original{};std::memcpy(original.data(),site,original.size());
    need((original[0]==0xff&&original[1]==0x15)&&
         !fofs::qualify(root_image,fleet_image)&&!fofs::owned(root_image,fleet_image),
         "pristine Root import call is not Fleet-owned");
    std::array<BYTE,6> redirected{{0xe8,0,0,0,0,0x90}};
    const auto displacement=address(callback)-address(site)-5;
    std::memcpy(redirected.data()+1,&displacement,4);write(site,redirected);
    root_exact=false;need(!fofs::qualify(root_image,fleet_image),"wrong Root identity refused");root_exact=true;
    fleet_exact=false;need(!fofs::qualify(root_image,fleet_image),"wrong Fleet identity refused");fleet_exact=true;
    auto bad=redirected;bad[1]^=1;write(site,bad);
    need(!fofs::qualify(root_image,fleet_image)&&!fofs::owned(root_image,fleet_image),
         "wrong call target refused");write(site,redirected);
    bad=redirected;bad[0]=0xe9;write(site,bad);
    need(!fofs::qualify(root_image,fleet_image)&&!fofs::owned(root_image,fleet_image),
         "wrong call opcode refused");write(site,redirected);
    bad=redirected;bad[5]=0xcc;write(site,bad);
    need(!fofs::qualify(root_image,fleet_image)&&!fofs::owned(root_image,fleet_image),
         "missing NOP refused");write(site,redirected);
    const BYTE first=*callback;write(callback,static_cast<BYTE>(first^1));
    need(!fofs::qualify(root_image,fleet_image)&&!fofs::owned(root_image,fleet_image),
         "Fleet callback byte mutation refused");write(callback,first);
    auto* operand=callback+profile::fopen_callback.relocations[0];
    const BYTE saved_operand=*operand;write(operand,static_cast<BYTE>(saved_operand^1));
    need(!fofs::qualify(root_image,fleet_image)&&!fofs::owned(root_image,fleet_image),
         "Fleet callback HIGHLOW mutation refused");write(operand,saved_operand);
    DWORD prior=0,ignored=0;
    need(VirtualProtect(callback,profile::fopen_callback.size,PAGE_READWRITE,&prior)!=0,
         "callback made nonexecutable in private image");
    need(!fofs::qualify(root_image,fleet_image)&&!fofs::owned(root_image,fleet_image),
         "nonexecutable Fleet callback refused");
    need(VirtualProtect(callback,profile::fopen_callback.size,prior,&ignored)!=0,
         "callback execute protection restored");
    need(fofs::owned(root_image,fleet_image)&&fofs::qualify(root_image,fleet_image),
         "exact Fleet redirect qualifies and pins callback module");
    constexpr auto call=profile::open_call_rva;
    std::array<BYTE,10> region{};
    std::memcpy(region.data(),site-2,region.size());
    auto normalized=region;
    need(fofs::normalize(root_image,fleet_image,call-2,normalized.data(),normalized.size()),
         "complete redirected call normalized");
    need(std::memcmp(normalized.data(),region.data(),2)==0&&
         std::memcmp(normalized.data()+2,profile::open_call_bytes.data(),6)==0&&
         std::memcmp(normalized.data()+8,region.data()+8,2)==0,
         "only pinned six call bytes replaced in comparison buffer");
    auto unchanged=region;
    need(!fofs::normalize(root_image,fleet_image,call-2,unchanged.data(),4)&&
         unchanged==region,"partial call overlap refuses atomically");
    need(!fofs::normalize(root_image,fleet_image,call+1,unchanged.data(),6)&&
         unchanged==region,"missing call prefix refuses atomically");
    need(!fofs::normalize(root_image,fleet_image,UINT32_MAX-1,unchanged.data(),4)&&
         unchanged==region,"32-bit region span overflow refuses atomically");
    need(fofs::normalize(root_image,fleet_image,call+8,unchanged.data(),6)&&
         unchanged==region,"unrelated region passes unchanged");
    write(site,bad); // Missing NOP revokes ownership after earlier qualification.
    need(!fofs::normalize(root_image,fleet_image,call-2,unchanged.data(),unchanged.size())&&
         unchanged==region,"ownership loss refuses normalization atomically");
    write(site,original);
    for(const auto& fix:fleet_fixes)write(fix.place,fix.old);
    // GetModuleHandleEx(PIN) may keep Fleet mapped; do not infer unload here.
    FreeLibrary(fleet_image);FreeLibrary(root_image);
    std::puts("PASS native11 FOFS stream guard");
}
