#include "../core/native11_lifecycle.hpp"
#include "../core/native11_lifecycle_profile.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
namespace life=a2fo::native11_lifecycle;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
HMODULE accepted=nullptr,fleet_image=nullptr;bool exact=true,exact_fleet=true,allow_patch=true;
unsigned deny_after=~0u;
bool make_entry_unreadable=false;
DWORD entry_restore_protection=0;
unsigned patches=0,model_calls=0,texture_calls=0,invalidations=0,logs=0;
std::uint32_t known_model=0x12345678u,known_texture=0x87654321u;
bool live_model=true,live_texture=true;
void log(const char*){++logs;}
std::uint32_t address(const void* p){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));}
template<class T>void write(void* p,const T& value){
    DWORD old=0,ignored=0;need(VirtualProtect(p,sizeof(T),PAGE_EXECUTE_READWRITE,&old)!=0,"private mapping writable");
    std::memcpy(p,&value,sizeof(T));
    need(FlushInstructionCache(GetCurrentProcess(),p,sizeof(T))!=0,"private mapping cache flush");
    need(VirtualProtect(p,sizeof(T),old,&ignored)!=0,"private mapping protection restored");
}
bool pinned_hash(const char* path,const BYTE expected[32]){
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool okay=false;
    if(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&
       CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
        BYTE block[16384]{};DWORD count=0;bool good=true;
        while(true){
            if(!ReadFile(file,block,sizeof(block),&count,nullptr)){good=false;break;}
            if(!count)break;
            if(!CryptHashData(hash,block,count,0)){good=false;break;}
        }
        BYTE digest[32]{};DWORD length=32;
        okay=good&&CryptGetHashParam(hash,HP_HASHVAL,digest,&length,0)&&length==32&&
            std::memcmp(digest,expected,32)==0;
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);
    return okay;
}
bool pinned(const char* path){
    const BYTE expected[32]{0x43,0x00,0xab,0x71,0x73,0x1e,0x32,0x3f,0x57,0x67,0xd8,0xee,
        0x05,0x45,0x7e,0x64,0x44,0x40,0x1b,0x82,0x32,0x53,0xae,0xbe,0xe8,0x6c,0x5a,0xc2,
        0x3c,0x2b,0x00,0xde};
    return pinned_hash(path,expected);
}
void notice(unsigned kind,std::uint32_t pointer){
    a2fo_native11_observe_destruction(kind,reinterpret_cast<void*>(static_cast<std::uintptr_t>(pointer)));
}
}
namespace a2fo::native11_capture {
bool exact_engine_image(HMODULE image) noexcept{return exact&&image==accepted;}
bool exact_image_file(HMODULE module,const unsigned char expected[32]) noexcept {
    return exact_fleet&&module==fleet_image&&expected&&
        std::memcmp(expected,life::lifecycle_profile::fleet_sha256.data(),32)==0;
}
}
namespace a2fo::native11_assets {
bool retire_model(std::uint32_t pointer) noexcept {
    ++model_calls;if(pointer!=known_model||!live_model)return false;
    live_model=false;return true;
}
}
namespace a2fo::native11_textures {
bool retire_texture(std::uint32_t pointer) noexcept {
    ++texture_calls;if(pointer!=known_texture||!live_texture)return false;
    live_texture=false;return true;
}
}
namespace a2fo::native11_world_pipeline {
void invalidate_sources() noexcept {++invalidations;}
}
extern "C" bool a2fo_rs_windows_patch_jump(void* site,void* tap,const std::uint8_t* expected,
    std::size_t count){
    need(accepted&&site&&tap&&expected&&count==5,"patch writer receives exact private prolog");
    const std::array<std::uint8_t,5> prolog{{0x55,0x8b,0xec,0x6a,0xff}};
    need(std::memcmp(expected,prolog.data(),5)==0,"patch writer expected bytes pinned");
    if(!allow_patch||patches>=deny_after||std::memcmp(site,expected,count)!=0)return false;
    auto* out=static_cast<std::uint8_t*>(site);
    DWORD old=0,ignored=0;if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old))return false;
    std::int32_t displacement=static_cast<std::int32_t>(address(tap)-address(site)-5);
    out[0]=0xe9;std::memcpy(out+1,&displacement,4);
    const bool flushed=FlushInstructionCache(GetCurrentProcess(),site,5)!=0;
    const bool protected_again=VirtualProtect(site,5,old,&ignored)!=0;
    if(flushed&&protected_again){
        ++patches;
        if(make_entry_unreadable){
            need(VirtualProtect(site,5,PAGE_NOACCESS,&entry_restore_protection)!=0,
                 "private patched entry made unreadable");
        }
    }
    return flushed&&protected_again;
}
void fleet_case(const char* root_path,const char* fleet_path){
    namespace p=life::lifecycle_profile;
    need(p::fleet_available&&pinned(root_path)&&
         pinned_hash(fleet_path,p::fleet_sha256.data()),"both Fleet profile inputs pinned");
    accepted=LoadLibraryExA(root_path,nullptr,DONT_RESOLVE_DLL_REFERENCES);
    fleet_image=LoadLibraryExA(fleet_path,nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(accepted&&fleet_image,"private Root and Fleet inspection mappings");
    auto* root=reinterpret_cast<std::uint8_t*>(accepted);
    auto* fleet=reinterpret_cast<std::uint8_t*>(fleet_image);
    const auto root_delta=address(root)-p::preferred_base;
    const auto fleet_delta=address(fleet)-p::fleet_preferred_base;
    for(const auto& region:p::regions){
        for(std::size_t i=0;i<region.relocation_count;++i){
            const auto off=region.relocations[i];std::uint32_t original=0,actual=0;
            std::memcpy(&original,region.bytes+off,4);
            std::memcpy(&actual,root+region.rva+off,4);
            need(actual==original||actual==original+root_delta,"Root relocation before normalization");
            if(actual==original)write(root+region.rva+off,original+root_delta);
        }
    }
    const auto& wrapper=p::fleet_wrapper;
    need(wrapper.size==50&&wrapper.relocation_count==1&&wrapper.relocations[0]==0x21,
         "Fleet wrapper profile shape");
    for(std::size_t i=0;i<wrapper.relocation_count;++i){
        const auto off=wrapper.relocations[i];std::uint32_t original=0,actual=0;
        std::memcpy(&original,wrapper.bytes+off,4);
        std::memcpy(&actual,fleet+wrapper.rva+off,4);
        need(actual==original||actual==original+fleet_delta,"Fleet relocation before normalization");
        if(actual==original)write(fleet+wrapper.rva+off,original+fleet_delta);
    }
    auto* model_site=root+p::regions[0].rva;
    auto* texture_site=root+p::regions[1].rva;
    std::array<std::uint8_t,6> native_entry{};
    std::memcpy(native_entry.data(),model_site,native_entry.size());
    std::array<std::uint8_t,5> texture_entry{};
    std::memcpy(texture_entry.data(),texture_site,texture_entry.size());
    auto* stub=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,16,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    need(stub,"private synthetic saved-original allocation");
    std::memcpy(stub,model_site,10);
    stub[10]=0x68;const auto continuation=address(model_site)+10;
    std::memcpy(stub+11,&continuation,4);stub[15]=0xc3;
    DWORD old=0;
    need(VirtualProtect(stub,16,PAGE_EXECUTE_READ,&old)!=0,"saved-original executable");
    auto* slot=fleet+p::fleet_original_slot_rva;
    MEMORY_BASIC_INFORMATION page{};
    need(VirtualQuery(slot,&page,sizeof(page))==sizeof(page)&&page.State==MEM_COMMIT&&
         page.Type==MEM_IMAGE&&page.AllocationBase==fleet&&
         (page.Protect==PAGE_READWRITE||page.Protect==PAGE_WRITECOPY),
         "Fleet original slot is writable image memory");
    std::uint32_t slot_original=0,engine_original=0;
    std::memcpy(&slot_original,slot,4);
    std::memcpy(&engine_original,root+0x3ad508,4);
    const auto stub_address=address(stub);write(slot,stub_address);
    std::array<std::uint8_t,6> fleet_entry{{0x68,0,0,0,0,0xc3}};
    const auto wrapper_address=address(fleet)+p::fleet_wrapper_rva;
    std::memcpy(fleet_entry.data()+1,&wrapper_address,4);write(model_site,fleet_entry);
    std::uint32_t zero=0;write(root+0x3ad508,zero);
    std::array<std::uint8_t,190> model_snapshot{};
    need(p::regions[0].size==model_snapshot.size(),"native model profile length");
    std::memcpy(model_snapshot.data(),model_site,model_snapshot.size());
    std::array<std::uint8_t,50> wrapper_snapshot{};
    std::memcpy(wrapper_snapshot.data(),fleet+wrapper.rva,wrapper_snapshot.size());
    auto slot_value=[&](){std::uint32_t value=0;std::memcpy(&value,slot,4);return value;};
    auto refused=[&](const char* why){
        need(!life::initialize(accepted,true,true,log,fleet_image)&&!life::healthy(3)&&
             slot_value()==stub_address&&patches==0,why);
    };
    exact_fleet=false;refused("wrong Fleet image identity refused before slot swap");exact_fleet=true;
    auto* wrapper_byte=fleet+wrapper.rva;
    const auto saved_wrapper=*wrapper_byte;write(wrapper_byte,static_cast<std::uint8_t>(saved_wrapper^1));
    refused("Fleet wrapper mutation refused before slot swap");write(wrapper_byte,saved_wrapper);
    auto* wrapper_operand=fleet+wrapper.rva+wrapper.relocations[0];
    const auto saved_operand=*wrapper_operand;
    write(wrapper_operand,static_cast<std::uint8_t>(saved_operand^1));
    refused("Fleet HIGHLOW operand mutation refused before slot swap");
    write(wrapper_operand,saved_operand);
    const auto saved_entry=*model_site;write(model_site,static_cast<std::uint8_t>(saved_entry^1));
    refused("Fleet entry mutation refused before slot swap");write(model_site,saved_entry);
    auto* body=model_site+10;const auto saved_body=*body;
    write(body,static_cast<std::uint8_t>(saved_body^1));
    refused("native destructor body mutation refused before slot swap");write(body,saved_body);
    const auto saved_stub=*stub;write(stub,static_cast<std::uint8_t>(saved_stub^1));
    refused("saved-original stub mutation refused before slot swap");write(stub,saved_stub);
    write(slot,zero);
    need(!life::initialize(accepted,true,true,log,fleet_image)&&slot_value()==0&&patches==0,
         "missing saved-original slot refused");write(slot,stub_address);
    const std::uint32_t engine=1;write(root+0x3ad508,engine);
    refused("constructed engine refused before slot swap");write(root+0x3ad508,zero);
    need(VirtualProtect(stub,16,PAGE_READWRITE,&old)!=0,"saved-original made nonexecutable");
    refused("nonexecutable saved-original refused before slot swap");
    need(VirtualProtect(stub,16,PAGE_EXECUTE_READ,&old)!=0,"saved-original execution restored");
    need(life::initialize(accepted,true,true,log,fleet_image)&&life::healthy(3)&&
         patches==1&&slot_value()==static_cast<std::uint32_t>(
             reinterpret_cast<std::uintptr_t>(a2fo_native11_model_destroy_tap))&&
         a2fo_native11_model_destroy_original==stub_address&&
         a2fo_native11_texture_destroy_original,
         "Fleet slot and native texture entry armed together");
    need(std::memcmp(model_site,fleet_entry.data(),6)==0&&
         std::memcmp(model_site+6,model_snapshot.data()+6,model_snapshot.size()-6)==0&&
         std::memcmp(fleet+wrapper.rva,wrapper_snapshot.data(),wrapper_snapshot.size())==0,
         "Fleet and native model executable bytes untouched");
    notice(life::model,known_model);notice(life::texture,known_texture);
    need(model_calls==1&&texture_calls==1&&invalidations==2,
         "Fleet observer notices retire known owners once");
    life::disable();need(!life::healthy(3)&&a2fo_native11_model_destroy_original==stub_address,
         "disable retains Fleet original pointer");
    need(life::initialize(accepted,true,true,log,fleet_image)&&life::healthy(3),
         "intact Fleet route reinitializes");
    const auto swapped=slot_value();write(slot,stub_address);
    need(!life::healthy(life::model)&&!life::initialize(accepted,true,true,log,fleet_image)&&
         a2fo_native11_model_destroy_original==stub_address,"Fleet slot ownership loss refused");
    write(slot,swapped);
    write(stub,static_cast<std::uint8_t>(saved_stub^1));
    need(!life::healthy(life::model)&&!life::initialize(accepted,true,true,log,fleet_image),
         "saved-original ownership loss refused");write(stub,saved_stub);
    write(wrapper_byte,static_cast<std::uint8_t>(saved_wrapper^1));
    need(!life::healthy(life::model)&&!life::initialize(accepted,true,true,log,fleet_image),
         "Fleet wrapper ownership loss refused");write(wrapper_byte,saved_wrapper);
    write(body,static_cast<std::uint8_t>(saved_body^1));
    need(!life::healthy(life::model)&&!life::initialize(accepted,true,true,log,fleet_image),
         "native destructor body ownership loss refused");write(body,saved_body);
    need(life::initialize(accepted,true,true,log,fleet_image)&&life::healthy(3),
         "restored Fleet route reinitializes");
    life::disable();
    write(slot,slot_original);write(root+0x3ad508,engine_original);
    write(model_site,native_entry);write(texture_site,texture_entry);
    need(FreeLibrary(fleet_image)!=0&&FreeLibrary(accepted)!=0,
         "private Fleet and Root images released after restoration");
    std::puts("PASS native11 lifecycle Fleet guard");
}
int main(int argc,char** argv){
    if(argc==3){fleet_case(argv[1],argv[2]);return 0;}
    need(argc==2&&pinned(argv[1]),"pinned 4300 image SHA-256");
    need(!life::initialize(nullptr,false,false,log)&&life::healthy(0)&&
         life::healthy(life::model|life::texture),"default disabled and unrequested domains healthy");
    accepted=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(accepted!=nullptr,"private inspection image map");
    auto* image=reinterpret_cast<std::uint8_t*>(accepted);
    const auto delta=address(image)-life::lifecycle_profile::preferred_base;
    for(const auto& region:life::lifecycle_profile::regions){
        for(std::size_t i=0;i<region.relocation_count;++i){
            const auto offset=region.relocations[i];std::uint32_t original=0,actual=0;
            std::memcpy(&original,region.bytes+offset,4);
            std::memcpy(&actual,image+region.rva+offset,4);
            need(actual==original||actual==original+delta,"HIGHLOW operand before normalization");
            if(actual==original)write(image+region.rva+offset,original+delta);
        }
    }
    for(const auto& region:life::lifecycle_profile::regions){
        for(const auto offset:{std::size_t(0),region.size-1}){
            auto* p=image+region.rva+offset;const auto saved=*p;
            write(p,static_cast<std::uint8_t>(saved^1));
            need(!life::initialize(accepted,true,true,log)&&!life::healthy(3)&&patches==0,
                 "mutated exact code refused before patch");write(p,saved);
        }
        for(std::size_t i=0;i<region.relocation_count;++i){
            auto* p=image+region.rva+region.relocations[i];std::uint32_t saved=0;
            std::memcpy(&saved,p,4);write(p,saved^1u);
            need(!life::initialize(accepted,true,true,log)&&patches==0,
                 "mutated HIGHLOW operand refused before patch");write(p,saved);
        }
    }
    exact=false;need(!life::initialize(accepted,true,true,log)&&patches==0,
                     "image verifier refusal before patch");exact=true;
    allow_patch=false;
    need(!life::initialize(accepted,true,true,log)&&!life::healthy(3)&&patches==0&&
         !a2fo_native11_model_destroy_original,"denied patch leaves no active hook or continuation");
    allow_patch=true;
    make_entry_unreadable=true;
    need(!life::initialize(accepted,true,false,log)&&patches==1&&
         !life::healthy(life::model)&&a2fo_native11_model_destroy_original,
         "unreadable patched entry remains inactive with retained continuation");
    notice(life::model,known_model);
    need(model_calls==0&&invalidations==0,"unreadable entry admits no notice");
    make_entry_unreadable=false;
    auto* model_site=image+life::lifecycle_profile::regions[0].rva;
    DWORD ignored=0;
    need(entry_restore_protection&&
         VirtualProtect(model_site,5,entry_restore_protection,&ignored)!=0,
         "restore private entry protection after failed readback");
    MEMORY_BASIC_INFORMATION trampoline_page{};
    const auto* retained_code=reinterpret_cast<const std::uint8_t*>(a2fo_native11_model_destroy_original);
    need(VirtualQuery(retained_code,&trampoline_page,sizeof(trampoline_page))==sizeof(trampoline_page)&&
         trampoline_page.State==MEM_COMMIT&&trampoline_page.Protect==PAGE_EXECUTE_READ&&
         model_site[0]==0xe9&&retained_code[0]==0x55&&retained_code[5]==0xe9,
         "private entry branch and executable original continuation remain intact");
    need(!life::healthy(life::model),"readback restoration does not silently activate observer");
    deny_after=1;
    need(!life::initialize(accepted,true,true,log)&&patches==1&&!life::healthy(3)&&
         a2fo_native11_model_destroy_original&&!a2fo_native11_texture_destroy_original,
         "partial install leaves requested lifecycle disabled and first continuation valid");
    notice(life::model,known_model);
    need(model_calls==0,"partial install does not admit model destruction");
    deny_after=~0u;
    need(life::initialize(accepted,true,true,log)&&patches==2&&life::healthy(3)&&
         a2fo_native11_model_destroy_original&&a2fo_native11_texture_destroy_original,
         "both destructor entries patched with valid continuations");
    need(life::healthy(0)&&!life::healthy(4),"health rejects invalid domain bits");
    notice(0,known_model);notice(4,known_model);notice(life::model,0);
    need(model_calls==0&&texture_calls==0&&invalidations==0,"invalid notices ignored");
    notice(life::model,0x11111111);notice(life::texture,0x22222222);
    need(model_calls==1&&texture_calls==1&&invalidations==0,"unknown pointers do not invalidate");
    notice(life::model,known_model);notice(life::model,known_model);
    notice(life::texture,known_texture);notice(life::texture,known_texture);
    need(model_calls==3&&texture_calls==3&&invalidations==2,
         "known model and texture retire once and invalidate once each");
    life::disable();need(!life::healthy(3),"disable revokes requested hooks");
    const auto before=model_calls;notice(life::model,known_model);
    need(model_calls==before,"disabled observer cannot retire");
    need(life::initialize(accepted,true,false,log)&&life::healthy(life::model|life::texture),
         "model-only request reuses owned model entry");
    live_texture=true;notice(life::texture,known_texture);
    need(texture_calls==3&&invalidations==2,"unrequested texture notice ignored");
    auto saved_jump=*model_site;write(model_site,static_cast<std::uint8_t>(saved_jump^1));
    need(!life::healthy(life::model)&&!life::initialize(accepted,true,false,log),
         "entry ownership loss prevents admission");
    notice(life::model,known_model);need(model_calls==before,"lost entry cannot observe");
    write(model_site,saved_jump);
    auto* trampoline=reinterpret_cast<std::uint8_t*>(a2fo_native11_model_destroy_original);
    const auto saved_trampoline=*trampoline;
    write(trampoline,static_cast<std::uint8_t>(saved_trampoline^1));
    need(!life::healthy(life::model)&&!life::initialize(accepted,true,false,log),
         "trampoline ownership loss prevents admission");
    write(trampoline,saved_trampoline);
    need(life::initialize(accepted,true,false,log)&&life::healthy(life::model),
         "restored private continuation admits model-only request");
    life::disable();
    for(const auto& region:life::lifecycle_profile::regions){
        if(region.rva==life::lifecycle_profile::regions[0].rva||
           region.rva==life::lifecycle_profile::regions[1].rva){
            std::array<std::uint8_t,5> original{};
            std::memcpy(original.data(),region.bytes,5);write(image+region.rva,original);
        }
    }
    need(FreeLibrary(accepted)!=0,"private image released after original entry bytes restored");
    std::puts("PASS native11 lifecycle guard");
}
