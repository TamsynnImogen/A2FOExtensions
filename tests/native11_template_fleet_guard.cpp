// Private pinned-image constructor-chain guard. No native function is called.
#include "../core/native11_template_fleet.hpp"
#include "../core/native11_template_fleet_profile.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
namespace chain=a2fo::native11_template_fleet;
namespace p=a2fo::native11_template_fleet::profile;
namespace {
HMODULE root_image=nullptr,fleet_image=nullptr;bool root_exact=true,fleet_exact=true;
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s winerr=%lu\n",why,GetLastError());std::exit(1);}}
std::uint32_t address(const void* value){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value));}
bool pinned(const char* path,const std::array<std::uint8_t,32>& expected){
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool okay=false;
    if(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&
       CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
        BYTE block[16384]{};DWORD size=0;bool good=true;
        while(true){
            if(!ReadFile(file,block,sizeof(block),&size,nullptr)){good=false;break;}
            if(!size)break;
            if(!CryptHashData(hash,block,size,0)){good=false;break;}
        }
        BYTE actual[32]{};DWORD length=32;
        okay=good&&CryptGetHashParam(hash,HP_HASHVAL,actual,&length,0)&&length==32&&
            std::memcmp(actual,expected.data(),32)==0;
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);
    return okay;
}
template<class T>void write(void* place,const T& value){
    DWORD old=0,ignored=0;
    need(VirtualProtect(place,sizeof(T),PAGE_EXECUTE_READWRITE,&old)!=0,"private image writable");
    std::memcpy(place,&value,sizeof(T));
    need(FlushInstructionCache(GetCurrentProcess(),place,sizeof(T))!=0,"private cache flushed");
    need(VirtualProtect(place,sizeof(T),old,&ignored)!=0,"private protection restored");
}
struct Fix {BYTE* place;std::uint32_t value;};
void normalize(const p::Region& region,std::vector<Fix>& changed){
    auto* base=reinterpret_cast<BYTE*>(fleet_image);
    const auto delta=address(base)-p::fleet_preferred_base;
    for(std::size_t i=0;i<region.relocation_count;++i){
        const auto off=region.relocations[i];std::uint32_t original=0,actual=0;
        std::memcpy(&original,region.bytes+off,4);
        std::memcpy(&actual,base+region.rva+off,4);
        need(actual==original||actual==original+delta,"Fleet wrapper HIGHLOW original or relocated");
        if(actual==original&&delta){changed.push_back({base+region.rva+off,actual});
            write(base+region.rva+off,original+delta);}
    }
}
}
namespace a2fo::native11_capture {
bool exact_engine_image(HMODULE image) noexcept{return root_exact&&image==root_image;}
bool exact_image_file(HMODULE image,const unsigned char expected[32]) noexcept {
    return fleet_exact&&image==fleet_image&&expected&&
        std::memcmp(expected,p::fleet_sha256.data(),32)==0;
}
}
int main(int argc,char** argv){
    constexpr std::array<std::uint8_t,32> root_sha{{0x43,0x00,0xab,0x71,0x73,0x1e,0x32,0x3f,
        0x57,0x67,0xd8,0xee,0x05,0x45,0x7e,0x64,0x44,0x40,0x1b,0x82,0x32,0x53,
        0xae,0xbe,0xe8,0x6c,0x5a,0xc2,0x3c,0x2b,0x00,0xde}};
    need(argc==3&&pinned(argv[1],root_sha)&&pinned(argv[2],p::fleet_sha256),
         "both exact PE inputs pinned before mapping");
    root_image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    fleet_image=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(root_image&&fleet_image,"private Root and Fleet maps");
    auto* root=reinterpret_cast<BYTE*>(root_image);
    auto* fleet=reinterpret_cast<BYTE*>(fleet_image);
    std::vector<Fix> fixes;normalize(p::inner_ctor,fixes);normalize(p::outer_ctor,fixes);
    auto* site=root+p::root_patch_rva;
    auto* entry=root+p::root_ctor_rva;
    std::array<BYTE,20> original_patch{};std::array<BYTE,6> original_entry{};
    std::memcpy(original_patch.data(),site,20);std::memcpy(original_entry.data(),entry,6);
    need(std::memcmp(site,p::root_original_bytes.data(),20)==0&&
         !chain::qualify(root_image,fleet_image),"pristine constructor chain not qualified");
    auto* table_slot=fleet+0x212870;auto* table_entry=fleet+0x210b38;
    auto* saved_slot=fleet+0x246a18;
    std::uint32_t old_table=0,old_target=0,old_saved=0;
    std::memcpy(&old_table,table_slot,4);std::memcpy(&old_target,table_entry,4);
    std::memcpy(&old_saved,saved_slot,4);
    auto* stub=static_cast<BYTE*>(VirtualAlloc(nullptr,16,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    need(stub,"synthetic saved constructor allocation");
    auto prefix=p::root_ctor_prolog;
    std::uint32_t relocated=0;std::memcpy(&relocated,prefix.data()+6,4);
    relocated+=address(root)-p::root_preferred_base;
    std::memcpy(prefix.data()+6,&relocated,4);
    std::memcpy(stub,prefix.data(),10);stub[10]=0x68;
    const auto continuation=address(entry)+10;std::memcpy(stub+11,&continuation,4);stub[15]=0xc3;
    DWORD old=0,ignored=0;
    need(VirtualProtect(stub,16,PAGE_EXECUTE_READ,&old)!=0,"saved constructor executable");
    write(table_slot,address(table_entry));write(table_entry,address(entry));write(saved_slot,address(stub));
    std::array<BYTE,6> redirected_entry{{0x68,0,0,0,0,0xc3}};
    const auto inner=address(fleet)+p::inner_ctor.rva;
    std::memcpy(redirected_entry.data()+1,&inner,4);write(entry,redirected_entry);
    std::array<BYTE,20> redirected_patch{};
    std::memcpy(redirected_patch.data(),p::root_patch_prefix.data(),15);
    redirected_patch[15]=0xe8;
    const auto relative=address(fleet)+p::outer_ctor.rva-address(site)-20;
    std::memcpy(redirected_patch.data()+16,&relative,4);write(site,redirected_patch);
    root_exact=false;need(!chain::qualify(root_image,fleet_image),"wrong Root identity refused");root_exact=true;
    fleet_exact=false;need(!chain::qualify(root_image,fleet_image),"wrong Fleet identity refused");fleet_exact=true;
    const BYTE first=*site;write(site,static_cast<BYTE>(first^1));
    need(!chain::owned(root_image,fleet_image),"Root parser prefix mutation refused");write(site,first);
    auto* call=site+16;const BYTE call_byte=*call;write(call,static_cast<BYTE>(call_byte^1));
    need(!chain::owned(root_image,fleet_image),"wrong outer call target refused");write(call,call_byte);
    const BYTE ctor_byte=*entry;write(entry,static_cast<BYTE>(ctor_byte^1));
    need(!chain::owned(root_image,fleet_image),"wrong Root constructor entry refused");write(entry,ctor_byte);
    for(const auto& region:{p::inner_ctor,p::outer_ctor}){
        auto* wrapper=fleet+region.rva;const BYTE first_byte=*wrapper;
        write(wrapper,static_cast<BYTE>(first_byte^1));
        need(!chain::owned(root_image,fleet_image),"Fleet wrapper body mutation refused");
        write(wrapper,first_byte);
        auto* operand=wrapper+region.relocations[0];const BYTE saved=*operand;
        write(operand,static_cast<BYTE>(saved^1));
        need(!chain::owned(root_image,fleet_image),"Fleet wrapper HIGHLOW mutation refused");
        write(operand,saved);
        DWORD prior=0;
        need(VirtualProtect(wrapper,region.size,PAGE_READWRITE,&prior)!=0,"wrapper made nonexecutable");
        need(!chain::owned(root_image,fleet_image),"nonexecutable Fleet wrapper refused");
        need(VirtualProtect(wrapper,region.size,prior,&ignored)!=0,"wrapper execute permission restored");
    }
    write(table_slot,std::uint32_t(0));
    need(!chain::owned(root_image,fleet_image),"wrong constructor table pointer refused");
    write(table_slot,address(table_entry));
    write(table_entry,std::uint32_t(0));
    need(!chain::owned(root_image,fleet_image),"wrong constructor table target refused");
    write(table_entry,address(entry));
    write(saved_slot,std::uint32_t(0));
    need(!chain::owned(root_image,fleet_image),"missing saved constructor refused");
    write(saved_slot,address(stub));
    const BYTE stub_byte=*stub;write(stub,static_cast<BYTE>(stub_byte^1));
    need(!chain::owned(root_image,fleet_image),"saved constructor bytes refused");write(stub,stub_byte);
    need(VirtualProtect(stub,16,PAGE_READWRITE,&old)!=0,"saved constructor made nonexecutable");
    need(!chain::owned(root_image,fleet_image),"nonexecutable saved constructor refused");
    need(VirtualProtect(stub,16,PAGE_EXECUTE_READ,&old)!=0,"saved constructor execution restored");
    need(chain::owned(root_image,fleet_image)&&chain::qualify(root_image,fleet_image),
         "exact constructor chain qualified");
    std::array<BYTE,24> comparison{};
    std::memcpy(comparison.data(),site-2,comparison.size());auto expected=comparison;
    need(chain::normalize(root_image,fleet_image,p::root_patch_rva-2,comparison.data(),comparison.size()),
         "full parser block normalized");
    need(std::memcmp(comparison.data(),expected.data(),2)==0&&
         std::memcmp(comparison.data()+2,p::root_original_bytes.data(),20)==0&&
         std::memcmp(comparison.data()+22,expected.data()+22,2)==0,
         "only original 20 parser bytes restored in comparison copy");
    auto unchanged=expected;
    need(chain::normalize(root_image,fleet_image,p::root_patch_rva-40,unchanged.data(),4)&&
         unchanged==expected,"unrelated comparison bytes remain unchanged");
    need(!chain::normalize(root_image,fleet_image,p::root_patch_rva-2,unchanged.data(),4)&&
         unchanged==expected,"partial overlap rejected atomically");
    need(!chain::normalize(root_image,fleet_image,UINT32_MAX-1,unchanged.data(),4)&&
         unchanged==expected,"span overflow rejected atomically");
    write(table_entry,std::uint32_t(0));
    need(!chain::normalize(root_image,fleet_image,p::root_patch_rva-2,unchanged.data(),unchanged.size())&&
         unchanged==expected,"lost table ownership rejects normalization atomically");
    write(table_entry,old_target);write(table_slot,old_table);write(saved_slot,old_saved);
    write(site,original_patch);write(entry,original_entry);
    for(const auto& fix:fixes)write(fix.place,fix.value);
    need(VirtualFree(stub,0,MEM_RELEASE)!=0,"synthetic saved constructor released");
    // Fleet is process-pinned by qualify; no unload claim follows FreeLibrary.
    FreeLibrary(fleet_image);FreeLibrary(root_image);
    std::puts("PASS native11 Fleet template chain guard");
}
