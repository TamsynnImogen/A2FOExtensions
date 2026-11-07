#include "../core/native11_sprites.hpp"
#include "../core/native11_sprite_profile.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>
namespace s=a2fo::native11_sprites;
namespace p=a2fo::native11_sprites::profile;
namespace {
void need(bool good,const char* why){if(!good){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
std::uint32_t address(const void* value){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value));}
HMODULE root_image=nullptr,fleet_image=nullptr;bool allow_patch=true,bump_on_resolve=false,reenter=false;
bool fleet_identity=true,owner_approved=false;
unsigned patches=0,deny_after=~0u,logs=0;
std::uint32_t epoch=1;
std::map<std::uint32_t,std::shared_ptr<const a2fo::native11::TextureAsset>> owners;
void log(const char*){++logs;}
bool pinned(const char* path,const std::array<std::uint8_t,32>& expected){
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool good=false;
    if(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&
       CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
        BYTE block[16384]{};DWORD size=0;bool read_ok=true;
        while(true){if(!ReadFile(file,block,sizeof(block),&size,nullptr)){read_ok=false;break;}
            if(!size)break;
            if(!CryptHashData(hash,block,size,0)){read_ok=false;break;}}
        BYTE result[32]{};DWORD length=32;
        good=read_ok&&CryptGetHashParam(hash,HP_HASHVAL,result,&length,0)&&length==32&&
            std::memcmp(result,expected.data(),32)==0;
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);return good;
}
template<class T>void write(void* location,const T& value){
    DWORD old=0,ignored=0;
    need(VirtualProtect(location,sizeof(T),PAGE_EXECUTE_READWRITE,&old)!=0,"private map writable");
    std::memcpy(location,&value,sizeof(T));
    need(FlushInstructionCache(GetCurrentProcess(),location,sizeof(T))!=0,"private cache flushed");
    need(VirtualProtect(location,sizeof(T),old,&ignored)!=0,"private map protection restored");
}
template<class T>void put(std::vector<BYTE>& data,std::size_t offset,const T& value){
    need(offset+sizeof(T)<=data.size(),"synthetic field bounds");
    std::memcpy(data.data()+offset,&value,sizeof(T));
}
struct Relocation {BYTE* location;std::uint32_t original;};
void normalize(HMODULE image,std::uint32_t preferred,const p::Region& region,
    std::vector<Relocation>& saved){
    auto* base=reinterpret_cast<BYTE*>(image);const auto delta=address(base)-preferred;
    for(std::size_t i=0;i<region.relocation_count;++i){
        const auto offset=region.relocations[i];std::uint32_t original=0,actual=0;
        std::memcpy(&original,region.bytes+offset,4);
        std::memcpy(&actual,base+region.rva+offset,4);
        need(actual==original||actual==original+delta,"reviewed HIGHLOW original or relocated");
        if(actual==original){saved.push_back({base+region.rva+offset,actual});
            write(base+region.rva+offset,original+delta);}
    }
}
std::shared_ptr<const a2fo::native11::TextureAsset> texture(std::uint64_t serial,std::size_t size=3){
    auto result=std::make_shared<a2fo::native11::TextureAsset>();
    result->serial=serial;result->texture=0x7000;
    result->bytes=std::make_shared<const std::vector<std::uint8_t>>(
        std::vector<std::uint8_t>(size,0x5a));return result;
}
std::shared_ptr<const s::Trace> latest(){std::string error;return s::latest(error);}
// Fixture approval only; the renderer's live callback checks its own hook/gateway.
bool check_owner(HMODULE module) noexcept {return owner_approved&&module==root_image;}
}
namespace a2fo::native11_capture {
bool exact_engine_image(HMODULE module) noexcept{return module==root_image;}
bool exact_image_file(HMODULE module,const unsigned char expected[32]) noexcept {
    return fleet_identity&&module==fleet_image&&expected&&
        std::memcmp(expected,p::fleet_sha256.data(),32)==0;
}
}
namespace a2fo::native11_world_pipeline {
std::uint32_t source_epoch() noexcept {return epoch;}
}
namespace a2fo::native11_textures {
std::shared_ptr<const native11::TextureAsset> current(std::uint32_t id) noexcept {
    if(bump_on_resolve){bump_on_resolve=false;++epoch;}
    if(reenter){reenter=false;
        std::array<std::uint32_t,7> invalid{};
        a2fo_native11_observe_sprite(reinterpret_cast<void*>(0x1234),invalid.data(),0,nullptr);
    }
    const auto found=owners.find(id);return found==owners.end()?nullptr:found->second;
}
}
extern "C" bool a2fo_rs_windows_patch_call(void* site,void* tap,const std::uint8_t* expected,
    std::size_t count){
    need(root_image&&site&&tap&&expected&&count==6,"patch writer receives private call site");
    if(!allow_patch||patches>=deny_after||std::memcmp(site,expected,6)!=0)return false;
    std::array<BYTE,6> replacement{};replacement[0]=0xe8;replacement[5]=0x90;
    const auto displacement=static_cast<std::uint32_t>(address(tap)-address(site)-5);
    std::memcpy(replacement.data()+1,&displacement,4);write(site,replacement);++patches;return true;
}
int main(int argc,char** argv){
    constexpr std::array<std::uint8_t,32> root_sha{{0x43,0x00,0xab,0x71,0x73,0x1e,0x32,0x3f,
        0x57,0x67,0xd8,0xee,0x05,0x45,0x7e,0x64,0x44,0x40,0x1b,0x82,0x32,0x53,
        0xae,0xbe,0xe8,0x6c,0x5a,0xc2,0x3c,0x2b,0x00,0xde}};
    const bool coexistence=argc==4&&std::strcmp(argv[3],"--coexistence")==0;
    need((argc==2||argc==3||coexistence)&&pinned(argv[1],root_sha),"pinned Root image");
    if(argc>=3)need(pinned(argv[2],p::fleet_sha256),"pinned FleetOps DLL");
    root_image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(root_image!=nullptr,"private Root inspection map");
    if(argc>=3){fleet_image=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        need(fleet_image!=nullptr,"private FleetOps inspection map");}
    auto* root=reinterpret_cast<BYTE*>(root_image);
    std::vector<Relocation> root_reloc,fleet_reloc;
    for(const auto& region:p::regions)normalize(root_image,p::preferred_base,region,root_reloc);
    if(fleet_image)normalize(fleet_image,p::fleet_preferred_base,p::fleet_callback,fleet_reloc);
    std::array<std::uint32_t,p::slots.size()> old_slots{};
    for(std::size_t i=0;i<p::slots.size();++i){
        const auto& slot=p::slots[i];std::memcpy(&old_slots[i],root+slot.rva,4);
        write(root+slot.rva,address(root)+slot.target_rva);
    }
    std::array<BYTE,6> old_sprite{},old_end{};
    std::memcpy(old_sprite.data(),root+0x23b486,6);
    std::memcpy(old_end.data(),root+0x224719,6);
    std::array<BYTE,44> old_dimensions{};
    std::array<BYTE,7> old_bloom{};
    if(coexistence){
        std::memcpy(old_dimensions.data(),root+0x221636,old_dimensions.size());
        std::memcpy(old_bloom.data(),root+0x224710,old_bloom.size());
        std::array<BYTE,44> nops{};nops.fill(0x90);write(root+0x221636,nops);
        const std::array<BYTE,7> foreign_jump{{0xe9,0,0,0,0,0x90,0x90}};
        write(root+0x224710,foreign_jump);
        need(std::memcmp(root+0x224719,old_end.data(),old_end.size())==0,
             "coexistence leaves native EndScene call untouched");
        need(!s::initialize(root_image,fleet_image,true,log)&&patches==0,
             "coexistence refuses absent refresh owner");
        need(!s::initialize(root_image,fleet_image,true,log,check_owner)&&patches==0,
             "coexistence refuses unapproved refresh owner");
        owner_approved=true;fleet_identity=false;
        need(!s::initialize(root_image,fleet_image,true,log,check_owner)&&patches==0,
             "Fleet dimension NOPs require pinned Fleet identity");
        fleet_identity=true;
        const BYTE saved_nop=root[0x221636];
        write(root+0x221636,static_cast<BYTE>(saved_nop^1));
        need(!s::initialize(root_image,fleet_image,true,log,check_owner)&&patches==0,
             "one changed dimension NOP refuses admission");
        write(root+0x221636,saved_nop);
        owner_approved=false;
        const BYTE saved_jump=root[0x224710];
        write(root+0x224710,static_cast<BYTE>(saved_jump^1));
        need(!s::initialize(root_image,fleet_image,true,log,check_owner)&&patches==0,
             "unknown refresh jump lacks owner approval");
        write(root+0x224710,saved_jump);
        owner_approved=true;
    }
    auto initialize=[&](){return s::initialize(root_image,fleet_image,true,log,
        coexistence?check_owner:nullptr);};
    if(fleet_image&&!coexistence){
        const auto* callback=reinterpret_cast<BYTE*>(fleet_image)+p::fleet_callback.rva;
        std::array<BYTE,6> source{{0xe8,0,0,0,0,0x90}};
        const auto displacement=static_cast<std::uint32_t>(address(callback)-address(root+0x224719)-5);
        std::memcpy(source.data()+1,&displacement,4);write(root+0x224719,source);
        auto saved=*callback;write(const_cast<BYTE*>(callback),static_cast<BYTE>(saved^1));
        need(!initialize()&&patches==0,
             "Fleet callback mutation refuses initial qualification");
        write(const_cast<BYTE*>(callback),saved);
    }
    auto* code=root+p::regions[0].rva;const auto saved_code=*code;
    write(code,static_cast<BYTE>(saved_code^1));
    need(!initialize()&&patches==0,
         "mutated Root code refused before patch");write(code,saved_code);
    const auto& region=p::regions[0];need(region.relocation_count>0,"Root HIGHLOW fixture");
    auto* operand=root+region.rva+region.relocations[0];std::uint32_t saved_operand=0;
    std::memcpy(&saved_operand,operand,4);write(operand,saved_operand^1u);
    need(!initialize()&&patches==0,
         "mutated Root HIGHLOW refused before patch");write(operand,saved_operand);
    auto* slot=root+p::slots[0].rva;std::uint32_t saved_slot=0;
    std::memcpy(&saved_slot,slot,4);write(slot,saved_slot^1u);
    need(!initialize()&&patches==0,
         "mutated Root slot refused before patch");write(slot,saved_slot);
    auto* dimension_slot=root+p::slots.back().rva;
    std::uint32_t saved_dimension_slot=0;
    std::memcpy(&saved_dimension_slot,dimension_slot,4);
    write(dimension_slot,saved_dimension_slot^1u);
    need(!initialize()&&patches==0,
         "mutated device dimension vtable slot refused before patch");
    write(dimension_slot,saved_dimension_slot);
    allow_patch=false;
    need(!initialize()&&!s::healthy()&&patches==0,
         "denied patch leaves trace disabled");
    allow_patch=true;deny_after=1;
    need(!initialize()&&!s::healthy()&&patches==1,
         "partial install remains disabled");
    if(fleet_image&&!coexistence)need(a2fo_native11_sprite_end_scene_original==
        address(reinterpret_cast<BYTE*>(fleet_image)+p::fleet_callback.rva),
        "Fleet original target retained across partial install");
    deny_after=~0u;
    need(initialize()&&patches==2&&s::healthy(),
         "two checked private call hooks armed");
    std::vector<BYTE> engine(0x108),device(0x350),com(4),sprite(0x60),material(0x34);
    std::vector<BYTE> position(12),size(8);
    std::array<std::uint32_t,7> args{};
    auto* base=root;
    std::uint32_t old_engine=0;std::memcpy(&old_engine,root+0x3ad508,4);
    write(root+0x3ad508,address(engine.data()));
    auto frame=std::make_unique<a2fo::native11::Frame>();frame->camera=0x1234;frame->generation=1;
    frame->view.viewport[2]=800;frame->view.viewport[3]=600;
    put(engine,0xfc,frame->camera);put(engine,0xc0,std::uint32_t(0));
    put(engine,0xcc,address(device.data()));
    put(device,0,address(base)+0x2bc6ac);put(device,0x10,0.f);put(device,0x14,0.f);
    put(device,0x84,800.f);put(device,0x88,600.f);
    put(device,0x44,address(material.data()));put(device,0x90,address(com.data()));
    put(device,0x264,std::uint32_t(1));put(device,0x268,std::uint32_t(2));
    put(device,0x270,std::uint32_t(3));put(device,0x274,std::uint32_t(4));
    put(device,0x278,std::uint32_t(5));put(com,0,address(base)+0x2bc6ac);
    put(sprite,0,address(base)+0x2bcb34);put(sprite,0x24,1.f);
    put(sprite,0x28,0.f);put(sprite,0x2c,0.f);
    put(sprite,0x54,address(material.data()));put(sprite,0x58,std::uint32_t(0x7000));
    put(material,0x18,std::uint32_t(1));put(material,0x1c,std::uint32_t(2));
    put(material,0x20,std::uint32_t(3));put(material,0x24,.5f);
    put(material,0x28,std::uint32_t(4));
    put(position,0,10.f);put(position,4,20.f);put(position,8,.25f);
    put(size,0,30.f);put(size,4,40.f);
    args={address(sprite.data()),address(position.data()),address(size.data()),
        0x3e000000u,0x3e800000u,0x3f000000u,0x3e800000u};
    owners[0x7000]=texture(1);
    const auto cw=a2fo::native11::encode_native_x87(0x037f);
    s::begin_ui(*frame,epoch);
    need(!latest(),"unsealed sprite trace is unavailable");
    a2fo_native11_observe_sprite(device.data(),args.data(),cw,base+0x2bc6ac);
    a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
    auto captured=latest();need(captured&&captured->sprites.size()==1&&
        captured->camera==frame->camera&&captured->device==address(device.data())&&
        captured->native_target_size[0]==800.f&&captured->native_target_size[1]==600.f&&
        captured->sprites[0]->packed_argb==0x80ff0000u&&
        captured->sprites[0]->geometry.left==10.f&&
        captured->sprites[0]->geometry.uv.u1==.625f&&
        captured->sprites[0]->address_u==1&&captured->sprites[0]->address_v==2&&
        captured->sprites[0]->mag_filter==3&&captured->sprites[0]->min_filter==4&&
        captured->sprites[0]->mip_filter==5&&
        captured->sprites[0]->texture_owner==owners[0x7000],
        "sealed trace owns copied metadata and immutable texture version");
    put(position,0,999.f);args[3]=0;put(sprite,0x24,0.f);
    need(captured->sprites[0]->geometry.left==10.f&&
         captured->sprites[0]->geometry.uv.u0==.125f&&
         captured->sprites[0]->packed_argb==0x80ff0000u,
         "published trace ignores later source buffer mutation");
    owners[0x7000]=texture(2);need(!latest(),"texture replacement revokes getter");
    owners[0x7000]=captured->sprites[0]->texture_owner;
    put(position,0,10.f);args[3]=0x3e000000u;put(sprite,0x24,1.f);
    ++frame->generation;s::begin_ui(*frame,epoch);
    a2fo_native11_observe_sprite(device.data(),args.data(),cw,base+0x2bc6ac);
    a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
    need(latest()!=nullptr,"fresh frame recovers after texture replacement");
    ++epoch;need(!latest(),"world source epoch revokes published getter");
    ++frame->generation;s::begin_ui(*frame,epoch);
    a2fo_native11_observe_sprite(device.data(),args.data(),cw,base+0x2bc6ac);
    a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
    need(latest()!=nullptr,"fresh frame recovers after epoch revocation");
    ++frame->generation;s::begin_ui(*frame,epoch);bump_on_resolve=true;
    a2fo_native11_observe_sprite(device.data(),args.data(),cw,base+0x2bc6ac);
    a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
    need(!latest(),"epoch change during texture resolution aborts frame");
    ++frame->generation;s::begin_ui(*frame,epoch);
    a2fo_native11_observe_sprite(reinterpret_cast<void*>(0x1234),args.data(),cw,base+0x2bc6ac);
    a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
    need(!latest(),"wrong dispatch device rejects frame");
    ++frame->generation;s::begin_ui(*frame,epoch);
    put(sprite,0,address(base)+0x2bcb35);
    a2fo_native11_observe_sprite(device.data(),args.data(),cw,base+0x2bc6ac);
    a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
    need(!latest(),"unsupported sprite vtable rejects frame");
    put(sprite,0,address(base)+0x2bcb34);
    ++frame->generation;s::begin_ui(*frame,epoch);reenter=true;
    a2fo_native11_observe_sprite(device.data(),args.data(),cw,base+0x2bc6ac);
    a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
    need(!latest(),"reentrant observer invalidates pending frame");
    ++frame->generation;s::begin_ui(*frame,epoch);
    std::thread foreign([&]{a2fo_native11_observe_sprite(device.data(),args.data(),cw,base+0x2bc6ac);});
    foreign.join();a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
    need(!latest(),"foreign observer invalidates pending frame");
    ++frame->generation;s::begin_ui(*frame,epoch);
    for(unsigned i=0;i<9;++i)a2fo_native11_observe_sprite(device.data(),args.data(),cw,base+0x2bc6ac);
    a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
    need(!latest(),"testing draw budget rejects ninth sprite without publication");
    owners[0x7000]=texture(3,257);
    ++frame->generation;s::begin_ui(*frame,epoch);
    a2fo_native11_observe_sprite(device.data(),args.data(),cw,base+0x2bc6ac);
    a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
    need(!latest(),"testing texture byte budget rejects oversized owner");
    owners[0x7000]=texture(4);
    put(device,0x10,800.f);put(device,0x14,600.f);
    put(device,0x84,801.f);
    ++frame->generation;s::begin_ui(*frame,epoch);
    put(device,0x84,800.f);
    a2fo_native11_observe_sprite(device.data(),args.data(),cw,base+0x2bc6ac);
    a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
    need(!latest(),"changed actual device dimension rejects despite plausible old fields");
    put(device,0x84,0.f);
    ++frame->generation;s::begin_ui(*frame,epoch);
    a2fo_native11_observe_sprite(device.data(),args.data(),cw,base+0x2bc6ac);
    a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
    need(!latest(),"invalid actual dimension cannot be masked by old fields");
    put(device,0x84,800.f);put(device,0x10,0.f);put(device,0x14,0.f);
    ++frame->generation;s::begin_ui(*frame,epoch);
    a2fo_native11_observe_sprite(device.data(),args.data(),cw,base+0x2bc6ac);
    a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
    need(latest()!=nullptr,"healthy frame after rejection");
    if(coexistence){
        owner_approved=false;
        need(!s::healthy()&&!latest(),"revoked refresh owner invalidates published trace");
        owner_approved=true;
        need(!latest(),"restored owner cannot resurrect revoked trace");
        need(initialize()&&s::healthy(),"restored refresh owner readmits coexistence");
        ++frame->generation;s::begin_ui(*frame,epoch);
        a2fo_native11_observe_sprite(device.data(),args.data(),cw,base+0x2bc6ac);
        a2fo_native11_end_sprite_ui(device.data(),com.data(),base+0x2bc6ac);
        need(latest()!=nullptr,"fresh coexistence frame publishes after owner recovery");
    }
    write(code,static_cast<BYTE>(saved_code^1));
    need(!latest()&&!s::healthy(),"profile ownership loss revokes published trace");
    write(code,saved_code);
    need(!latest(),"restored profile cannot resurrect old trace without new begin");
    s::disable();need(!latest(),"disabled sprite trace unavailable");
    write(root+0x3ad508,old_engine);
    write(root+0x23b486,old_sprite);write(root+0x224719,old_end);
    if(coexistence){write(root+0x221636,old_dimensions);write(root+0x224710,old_bloom);}
    for(std::size_t i=0;i<p::slots.size();++i)write(root+p::slots[i].rva,old_slots[i]);
    for(const auto& item:root_reloc)write(item.location,item.original);
    for(const auto& item:fleet_reloc)write(item.location,item.original);
    if(fleet_image)need(FreeLibrary(fleet_image)!=0,"private FleetOps map released");
    need(FreeLibrary(root_image)!=0,"private Root map released");
    std::puts("PASS native11 sprite guard");
}
