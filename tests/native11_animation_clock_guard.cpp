#include "../core/native11_animation_clock.hpp"
#include "../core/native11_animation_clock_profile.hpp"
#include "../core/native11_template_profile.hpp"
#include "../core/native11_craft_view_profile.hpp"
#include "../core/native11_template_fleet_profile.hpp"
#include "../renderer/native11/craft_view_capture.hpp"
#include "../renderer/native11/model_node_template.hpp"
#include "../renderer/native11/model_material_baseline.hpp"
#include "../renderer/native11/model_lod_template.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include "native11_model_fixture.hpp"
#include "../renderer/native11/material_observation.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>
namespace c=a2fo::native11_animation_clock;
namespace {
void need(bool yes,const char* text){if(!yes){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
HMODULE accepted_image=nullptr,accepted_fleet=nullptr;bool owned=true;unsigned matched=0,unavailable=0,logs=0;
bool animation_fallback=false,fallback_available=true,fallback_custom=false;
bool fallback_change_second=false,fallback_change_third=false;
unsigned fallback_calls=0;std::uint64_t fallback_epoch=1;BYTE* fallback_gateway=nullptr;
void fallback_replacement() {}
bool fallback_source(HMODULE image,a2fo::animations::NativePlaybackProof& output) noexcept {
    ++fallback_calls;
    if(fallback_change_second&&fallback_calls==2)++fallback_epoch;
    if(fallback_change_third&&fallback_calls==3)++fallback_epoch;
    if(!fallback_available||fallback_custom||image!=accepted_image)return false;
    output={image,reinterpret_cast<BYTE*>(image)+0x2161c0,
        reinterpret_cast<void*>(fallback_replacement),fallback_gateway,6,fallback_epoch};
    return true;
}
a2fo::native11::Object object;bool probe_reentry=false;
std::shared_ptr<const a2fo::native11::ModelAsset> active_model;
std::map<std::uint32_t,std::shared_ptr<const a2fo::native11::TextureAsset>> active_textures;
unsigned pin_calls=0;bool revoke_model=false;
unsigned owned_matches=0,owned_differences=0,seeded_matches=0;bool asset_owned=true;unsigned short reported_cw=0;bool inspect_cw=false;
bool pinned(const char* path,const BYTE expected[32]){
    auto file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool okay=false;
    if(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
        BYTE block[16384]{};DWORD count=0;bool good=true;
        while(true){if(!ReadFile(file,block,sizeof(block),&count,nullptr)){good=false;break;}if(!count)break;
            if(!CryptHashData(hash,block,count,0)){good=false;break;}}
        BYTE digest[32]{};DWORD length=32;
        okay=good&&CryptGetHashParam(hash,HP_HASHVAL,digest,&length,0)&&length==32&&!std::memcmp(digest,expected,32);
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);return okay;
}
std::uint32_t addr(const void* p){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));}
template<class T>void write(void* address,const T& value){DWORD old=0;need(VirtualProtect(address,sizeof(T),PAGE_READWRITE,&old)!=0,"mapped writable");std::memcpy(address,&value,sizeof(T));DWORD ignored=0;need(VirtualProtect(address,sizeof(T),old,&ignored)!=0,"mapped protection restored");}
template<class T>void put(std::vector<BYTE>& value,unsigned offset,const T& word){need(offset+sizeof(T)<=value.size(),"fixture bounds");std::memcpy(value.data()+offset,&word,sizeof(T));}
void log(const char* text){if(inspect_cw)asm volatile("fnstcw %0":"=m"(reported_cw));
    ++logs;const std::string line=text;
    if(line.find("Native11 load materials:")!=std::string::npos)std::fprintf(stderr,"%s\n",text);
    if(line.find("status=matched")!=std::string::npos)++matched;
    if(line.find("status=unavailable")!=std::string::npos)++unavailable;
    if(line.find("Native11 owned transform:")!=std::string::npos){
        if(line.find("state=matched")!=std::string::npos)++owned_matches;
        if(line.find("state=differs")!=std::string::npos)++owned_differences;
        if(line.find("state=matched")!=std::string::npos&&line.find("seed=load_template")!=std::string::npos)++seeded_matches;
    }
    if(probe_reentry)c::observe(object,1);
}
}
namespace a2fo::native11_capture {
bool exact_engine_image(HMODULE image) noexcept{return image&&image==accepted_image;}
bool exact_image_file(HMODULE image,const unsigned char expected[32]) noexcept {
    return image&&image==accepted_fleet&&expected&&
        std::memcmp(expected,a2fo::native11_template_fleet::profile::fleet_sha256.data(),32)==0;
}
}
namespace a2fo::native11_nodes {
unsigned owner_checks=0,revoke_at=0;
bool owns_calls() noexcept {
    if(revoke_at&&++owner_checks==revoke_at)owned=false;
    return owned;
}
}
namespace a2fo::native11_assets {
bool owns_load_call(HMODULE module) noexcept{return asset_owned&&module==accepted_image;}
std::shared_ptr<const native11::ModelAsset> pin_model(std::uint32_t db,std::uint64_t serial) noexcept{
    if(revoke_model&&++pin_calls==2)active_model.reset();
    return active_model&&active_model->info.database==db&&active_model->info.serial==serial?active_model:nullptr;
}
}
namespace a2fo::native11_textures {
std::shared_ptr<const native11::TextureAsset> current(std::uint32_t address) noexcept {
    const auto found=active_textures.find(address);
    return found==active_textures.end()?nullptr:found->second;
}
}
int main(int argc,char** argv){
    const BYTE root_sha[32]{0x43,0x00,0xab,0x71,0x73,0x1e,0x32,0x3f,0x57,0x67,0xd8,0xee,
        0x05,0x45,0x7e,0x64,0x44,0x40,0x1b,0x82,0x32,0x53,0xae,0xbe,0xe8,0x6c,
        0x5a,0xc2,0x3c,0x2b,0x00,0xde};
    animation_fallback=argc==4&&std::strcmp(argv[3],"--animation-fallback")==0;
    need((argc==2||argc==3||animation_fallback)&&pinned(argv[1],root_sha),"pinned image bytes");
    if(argc>=3)need(pinned(argv[2],a2fo::native11_template_fleet::profile::fleet_sha256.data()),
                    "pinned Fleet image bytes");
    need(!c::initialize(nullptr,false,log)&&!c::enabled(),"default off");
    need(!c::initialize(GetModuleHandleW(nullptr),true,log),"foreign image rejected");
    accepted_image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);need(accepted_image!=nullptr,"private inspection map");
    if(argc>=3){accepted_fleet=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        need(accepted_fleet!=nullptr,"private Fleet inspection map");}
    auto* image=reinterpret_cast<BYTE*>(accepted_image);const auto base=addr(image);const auto delta=base-0x400000u;
    // Bring only the inspected regions/slots into normal loaded-address form.
    for(const auto& region:c::profile::regions)for(std::size_t i=0;i<region.relocation_count;++i){
        const auto offset=region.relocations[i];std::uint32_t original=0,actual=0;
        std::memcpy(&original,region.bytes+offset,4);std::memcpy(&actual,image+region.rva+offset,4);
        need(actual==original||actual==original+delta,"original or relocated inspected operand");
        if(actual==original)write(image+region.rva+offset,original+delta);
    }
    for(const auto& region:c::template_profile::regions)for(std::size_t i=0;i<region.relocation_count;++i){
        const auto offset=region.relocations[i];std::uint32_t original=0,actual=0;
        std::memcpy(&original,region.bytes+offset,4);std::memcpy(&actual,image+region.rva+offset,4);
        need(actual==original||actual==original+delta,"template original or relocated operand");
        if(actual==original)write(image+region.rva+offset,original+delta);
    }
    std::vector<std::pair<BYTE*,std::uint32_t>> old_view_relocations;
    for(const auto& region:c::view_profile::regions)for(std::size_t i=0;i<region.relocation_count;++i){
        const auto offset=region.relocations[i];std::uint32_t original=0,actual=0;
        std::memcpy(&original,region.bytes+offset,4);std::memcpy(&actual,image+region.rva+offset,4);
        need(actual==original||actual==original+delta,"view original or relocated operand");
        if(actual==original){
            old_view_relocations.emplace_back(image+region.rva+offset,actual);
            write(image+region.rva+offset,original+delta);
        }
    }
    for(const auto& slot:c::profile::slots)write(image+slot[0],base+slot[1]);
    // Wine's inspection-only EXE map does not relocate every data slot. The
    // new material guard checks this separate mesh type slot, so normalize it
    // explicitly just like the existing clock/view slots; restore it on exit.
    std::uint32_t old_material_type=0;std::memcpy(&old_material_type,image+0x2bca00,4);
    need(old_material_type==0x5968f0||old_material_type==base+0x1968f0,
         "original or relocated material mesh-type slot");
    write(image+0x2bca00,base+0x1968f0);
    std::array<std::uint32_t,2> old_view_slots{};
    for(std::size_t i=0;i<c::view_profile::slots.size();++i){
        const auto& slot=c::view_profile::slots[i];
        std::memcpy(&old_view_slots[i],image+slot[0],4);
        write(image+slot[0],base+slot[1]);
    }
    namespace fleet_profile=a2fo::native11_template_fleet::profile;
    std::vector<std::pair<BYTE*,std::uint32_t>> old_fleet_relocations;
    std::array<BYTE,20> old_ctor_patch{};std::array<BYTE,6> old_ctor_entry{};
    std::uint32_t old_chain_table=0,old_chain_target=0,old_chain_stub=0;
    BYTE* constructor_stub=nullptr;
    if(accepted_fleet){
        auto* fleet=reinterpret_cast<BYTE*>(accepted_fleet);
        const auto fleet_delta=addr(fleet)-fleet_profile::fleet_preferred_base;
        for(const auto& region:{fleet_profile::inner_ctor,fleet_profile::outer_ctor})
            for(std::size_t i=0;i<region.relocation_count;++i){
                const auto offset=region.relocations[i];std::uint32_t original=0,actual=0;
                std::memcpy(&original,region.bytes+offset,4);
                std::memcpy(&actual,fleet+region.rva+offset,4);
                need(actual==original||actual==original+fleet_delta,"Fleet wrapper HIGHLOW original or relocated");
                if(actual==original&&fleet_delta){
                    old_fleet_relocations.emplace_back(fleet+region.rva+offset,actual);
                    write(fleet+region.rva+offset,original+fleet_delta);
                }
            }
        auto* patch=image+fleet_profile::root_patch_rva;
        auto* ctor=image+fleet_profile::root_ctor_rva;
        std::memcpy(old_ctor_patch.data(),patch,old_ctor_patch.size());
        std::memcpy(old_ctor_entry.data(),ctor,old_ctor_entry.size());
        need(std::memcmp(old_ctor_patch.data(),fleet_profile::root_original_bytes.data(),20)==0,
             "private original constructor block pinned");
        auto* table=fleet+0x212870;auto* target=fleet+0x210b38;auto* slot=fleet+0x246a18;
        std::memcpy(&old_chain_table,table,4);std::memcpy(&old_chain_target,target,4);
        std::memcpy(&old_chain_stub,slot,4);
        constructor_stub=static_cast<BYTE*>(VirtualAlloc(nullptr,16,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        need(constructor_stub!=nullptr,"private saved constructor allocation");
        auto prefix=fleet_profile::root_ctor_prolog;
        std::uint32_t operand=0;std::memcpy(&operand,prefix.data()+6,4);
        operand+=base-fleet_profile::root_preferred_base;
        std::memcpy(prefix.data()+6,&operand,4);
        std::memcpy(constructor_stub,prefix.data(),10);
        constructor_stub[10]=0x68;const auto continuation=base+fleet_profile::root_ctor_rva+10;
        std::memcpy(constructor_stub+11,&continuation,4);constructor_stub[15]=0xc3;
        DWORD prior=0;need(VirtualProtect(constructor_stub,16,PAGE_EXECUTE_READ,&prior)!=0,
                          "saved constructor executable");
        write(table,addr(target));write(target,addr(ctor));write(slot,addr(constructor_stub));
        std::array<BYTE,6> entry{{0x68,0,0,0,0,0xc3}};
        const auto inner=addr(fleet)+fleet_profile::inner_ctor.rva;
        std::memcpy(entry.data()+1,&inner,4);write(ctor,entry);
        std::array<BYTE,20> patched{};
        std::memcpy(patched.data(),fleet_profile::root_patch_prefix.data(),15);patched[15]=0xe8;
        const auto relative=addr(fleet)+fleet_profile::outer_ctor.rva-addr(patch)-20;
        std::memcpy(patched.data()+16,&relative,4);write(patch,patched);
    }
    std::array<BYTE,6> old_fallback_entry{};
    if(animation_fallback){
        auto* site=image+0x2161c0;
        std::memcpy(old_fallback_entry.data(),site,old_fallback_entry.size());
        need(old_fallback_entry==std::array<BYTE,6>{{0x55,0x8b,0xec,0x53,0x8b,0xd9}},
             "pinned original evaluation entry");
        fallback_gateway=static_cast<BYTE*>(VirtualAlloc(nullptr,11,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        need(fallback_gateway!=nullptr,"synthetic fallback gateway allocated");
        std::array<BYTE,11> gateway{};
        std::memcpy(gateway.data(),old_fallback_entry.data(),6);gateway[6]=0xe9;
        const auto back=addr(site)+6-addr(fallback_gateway)-11;
        std::memcpy(gateway.data()+7,&back,4);
        std::memcpy(fallback_gateway,gateway.data(),gateway.size());
        DWORD previous=0;
        need(VirtualProtect(fallback_gateway,11,PAGE_EXECUTE_READ,&previous)!=0,
             "synthetic fallback gateway executable");
        std::array<BYTE,6> entry{{0xe9,0,0,0,0,0x90}};
        const auto forward=addr(reinterpret_cast<void*>(fallback_replacement))-addr(site)-5;
        std::memcpy(entry.data()+1,&forward,4);write(site,entry);
        need(!c::initialize(accepted_image,true,log,accepted_fleet),
             "patched entry without source predicate refused");
        fallback_available=false;
        need(!c::initialize(accepted_image,true,log,accepted_fleet,fallback_source),
             "unavailable native fallback source refused");fallback_available=true;
        fallback_custom=true;
        need(!c::initialize(accepted_image,true,log,accepted_fleet,fallback_source),
             "custom animation class refuses fallback source");fallback_custom=false;
        const BYTE gateway_first=*fallback_gateway;
        write(fallback_gateway,static_cast<BYTE>(gateway_first^1));
        need(!c::initialize(accepted_image,true,log,accepted_fleet,fallback_source),
             "wrong gateway prefix refused");write(fallback_gateway,gateway_first);
        std::uint32_t continuation=0;std::memcpy(&continuation,fallback_gateway+7,4);
        write(fallback_gateway+7,continuation^1u);
        need(!c::initialize(accepted_image,true,log,accepted_fleet,fallback_source),
             "wrong gateway continuation refused");write(fallback_gateway+7,continuation);
        need(VirtualProtect(fallback_gateway,11,PAGE_READWRITE,&previous)!=0,
             "synthetic gateway made nonexecutable");
        need(!c::initialize(accepted_image,true,log,accepted_fleet,fallback_source),
             "nonexecutable gateway refused");
        need(VirtualProtect(fallback_gateway,11,PAGE_EXECUTE_READ,&previous)!=0,
             "synthetic gateway execution restored");
        fallback_epoch=0;
        need(!c::initialize(accepted_image,true,log,accepted_fleet,fallback_source),
             "zero fallback epoch refused");fallback_epoch=1;
        fallback_calls=0;fallback_change_second=true;
        need(!c::initialize(accepted_image,true,log,accepted_fleet,fallback_source),
             "changing fallback epoch during qualification refused");
        fallback_change_second=false;fallback_epoch=1;fallback_calls=0;
    }
    auto initialize=[&](){return c::initialize(accepted_image,true,log,accepted_fleet,
        animation_fallback?fallback_source:nullptr);};
    for(const auto& region:c::profile::regions){
        for(const auto offset:{std::size_t(0),region.size-1}){
            auto* p=image+region.rva+offset;const auto before=*p;write(p,static_cast<BYTE>(before^1));
            need(!initialize()&&!c::enabled(),"mutated profile region rejected");write(p,before);
        }
        for(std::size_t i=0;i<region.relocation_count;++i){auto* p=image+region.rva+region.relocations[i];
            std::uint32_t before=0;std::memcpy(&before,p,4);write(p,before^1u);
            need(!initialize(),"mutated HIGHLOW operand rejected");write(p,before);
        }
    }
    for(const auto& slot:c::profile::slots){auto* p=image+slot[0];std::uint32_t before=0;std::memcpy(&before,p,4);write(p,before^1u);
        need(!initialize(),"foreign channel dispatch rejected");write(p,before);
    }
    owned=false;need(!initialize(),"node ownership required");owned=true;
    need(initialize()&&c::enabled(),"checked diagnostics admitted");
    std::vector<BYTE> engine(0x108),instance(0xa8),db(0x74),sentinel(12),node(12),channel(0x58);
    object.instance=addr(instance.data());object.database=addr(db.data());
    object.instance_vtable=base+0x2b3e2c;object.database_vtable=base+0x2bc56c;
    put(engine,0x100,object.instance);put(engine,0x8c,.25f);
    put(instance,0,object.instance_vtable);put(instance,0x80,object.database);
    put(instance,0xc,std::uint32_t(3));put(instance,0x76,BYTE(1));put(db,0,object.database_vtable);
    put(db,0x6c,addr(sentinel.data()));put(db,0x70,std::uint32_t(1));
    put(sentinel,0,addr(node.data()));put(sentinel,4,addr(node.data()));
    put(node,0,addr(sentinel.data()));put(node,4,addr(sentinel.data()));put(node,8,addr(channel.data()));
    put(channel,0,base+0x2bc39c);put(channel,4,object.database);put(channel,0xc,addr(node.data()));
    put(channel,0x2c,1.f);put(channel,0x30,std::uint32_t(0));put(channel,0x34,std::uint32_t(2));
    std::uint32_t old_engine=0;float old_elapsed=0;std::memcpy(&old_engine,image+0x3ad508,4);std::memcpy(&old_elapsed,image+0x3a8e74,4);
    write(image+0x3ad508,addr(engine.data()));write(image+0x3a8e74,.25f);
    a2fo::native11::InstanceAnimationTime fresh;std::string time_error;
    // Reading an instance's time must not depend on the last-rendered slot.
    put(engine,0x100,std::uint32_t(0xdeadcafe));
    need(c::sample_time(object,fresh,time_error)&&fresh.elapsed==.25f,
         "independent instance time ignores shared current-instance slot");
    if(animation_fallback){
        const auto before=fresh;fallback_calls=0;fallback_change_third=true;
        need(!c::sample_time(object,fresh,time_error)&&
             fresh.engine==before.engine&&fresh.instance==before.instance&&
             fresh.database==before.database&&fresh.flags==before.flags&&
             fresh.animating==before.animating&&fresh.engine_time==before.engine_time&&
             fresh.start_time==before.start_time&&fresh.elapsed==before.elapsed,
             "epoch change between profile reads preserves sampled time output");
        fallback_change_third=false;fallback_calls=0;
    }
    put(engine,0x100,object.instance);
    bool foreign_refused=false;
    std::thread foreign_time([&]{a2fo::native11::InstanceAnimationTime ignored;
        std::string error;foreign_refused=!c::sample_time(object,ignored,error);});
    foreign_time.join();need(foreign_refused,"foreign time sampling refused");
    auto asset=std::make_shared<a2fo::native11::ModelAsset>();
    asset->info.database=object.database;asset->info.serial=11;
    std::strcpy(asset->info.name,"fixture.sod");asset->bytes=native11_fixture::model();
    asset->info.byte_count=asset->bytes.size();active_model=asset;object.model_serial=11;
    std::vector<BYTE> entity(0xb8),camera(0x120),context(0x44);
    object.entity=addr(entity.data());put(entity,4,object.instance);
    object.model_identity=a2fo::native11::ModelIdentity::plain_database_name;
    object.placement_identity=a2fo::native11::PlacementIdentity::craft_runtime_scale;
    const std::array<float,12> identity_native{{1,0,0,0,1,0,0,0,1,0,0,0}};
    put(instance,0x44,identity_native);put(camera,0xf0,identity_native);
    object.world[0]=object.world[5]=object.world[10]=object.world[15]=1;
    put(engine,0xfc,addr(camera.data()));put(engine,0x104,addr(context.data()));
    put(engine,0xac,std::int32_t(-1));put(instance,0x14,std::int32_t(2));
    put(context,0x40,std::int32_t(4));
    unsigned short lod_cw=0;asm volatile("fnstcw %0":"=m"(lod_cw));
    const auto encoded_lod_cw=a2fo::native11::encode_native_x87(lod_cw);
    a2fo::native11::ModelLodContext lod_context;
    put(engine,0x100,std::uint32_t(0xdeadcafe));
    need(c::sample_lod_context(object,addr(camera.data()),addr(context.data()),encoded_lod_cw,lod_context,time_error)&&
         lod_context.requested==2&&lod_context.instance==object.instance,
         "owner-bound fresh LOD producer ignores last rendered engine instance");
    const auto retained_lod=lod_context;
    need(!c::sample_lod_context(object,addr(camera.data())+4,addr(context.data()),encoded_lod_cw,lod_context,time_error)&&
         lod_context.camera==retained_lod.camera,"wrong active LOD camera preserves output");
    revoke_model=true;pin_calls=0;
    need(!c::sample_lod_context(object,addr(camera.data()),addr(context.data()),encoded_lod_cw,lod_context,time_error)&&
         lod_context.instance==retained_lod.instance,"LOD model revocation preserves output");
    revoke_model=false;active_model=asset;
    bool foreign_lod=false;std::thread lod_other([&]{std::string error;
        foreign_lod=!c::sample_lod_context(object,addr(camera.data()),addr(context.data()),encoded_lod_cw,lod_context,error);});
    lod_other.join();need(foreign_lod,"foreign LOD sampling refused");
    put(db,0x70,std::uint32_t(0));put(sentinel,0,addr(sentinel.data()));put(sentinel,4,addr(sentinel.data()));
    put(engine,0x100,std::uint32_t(0xdeadcafe));
    a2fo::native11::RuntimePoseChannels channels;
    need(c::sample_channels(object,channels,time_error)&&channels.model.model==active_model&&
         channels.model.pose->nodes.size()==1&&channels.model.pose->channels.empty()&&
         channels.clock.elapsed==.25f,"real Rust metadata and independent runtime channel producer");
    const auto retained=channels.model.pose;
    revoke_model=true;pin_calls=0;
    need(!c::sample_channels(object,channels,time_error)&&channels.model.pose==retained,
         "model revocation before publication preserves caller packet");
    revoke_model=false;active_model=asset;
    put(db,0x70,std::uint32_t(1));put(sentinel,0,addr(node.data()));put(sentinel,4,addr(node.data()));
    put(engine,0x100,object.instance);
    probe_reentry=true;c::observe(object,1);probe_reentry=false;need(matched==1&&!unavailable,"read-only clock match with reentry refusal");
    const auto prior=logs;std::thread other([&]{c::observe(object,1);});other.join();need(logs==prior,"foreign thread ignored");
    put(channel,0x30,std::uint32_t(1));c::observe(object,1);need(unavailable==1&&c::enabled(),"native key mismatch diagnostic only");
    owned=false;c::observe(object,1);need(!c::enabled(),"lost observer ownership revokes diagnostics");owned=true;
    need(initialize(),"reinitialize diagnostics");
    auto* p=image+c::profile::regions[0].rva;const auto before=*p;write(p,static_cast<BYTE>(before^1));
    c::observe(object,1);need(!c::enabled(),"runtime profile mutation revokes diagnostics");write(p,before);
    need(initialize(),"reinitialize owned transform comparison");
    put(db,0x70,std::uint32_t(0));put(sentinel,0,addr(sentinel.data()));put(sentinel,4,addr(sentinel.data()));
    a2fo::native11::ModelMaterialSource material_source;material_source.model=asset;
    material_source.parts.resize(1);material_source.parts[0].node_name="mesh";
    a2fo::native11::MaterialObservation effective;effective.model=asset;effective.parts.resize(1);
    effective.parts[0].node_state_valid=true;
    c::observe(object,1,&material_source,&effective);
    need(owned_matches==1,"owned transforms compared with current effective part states");
    effective.parts[0].node_state.local_to_model[3]=1;
    c::observe(object,1,&material_source,&effective);
    need(owned_differences==1,"class or procedural transform discrepancy remains diagnostic");
    // Simulate only the observer input at a completed fresh database load.
    // No native loader or node method is executed in this fixture.
    std::vector<BYTE> mesh(0x140),mesh_vtable(8),native_texture(0x30),native_borg(0x30);
    const char mesh_name[]="mesh",model_name[]="fixture.sod";
    const char texture_name[]="base.tga",borg_name[]="borg.tga";
    put(native_texture,8,addr(texture_name));put(native_borg,8,addr(borg_name));
    auto base_texture=std::make_shared<a2fo::native11::TextureAsset>();
    base_texture->texture=addr(native_texture.data());base_texture->serial=1;
    base_texture->logical_name=texture_name;
    base_texture->bytes=std::make_shared<const std::vector<std::uint8_t>>(4,0x7f);
    auto borg_texture=std::make_shared<a2fo::native11::TextureAsset>(*base_texture);
    borg_texture->texture=addr(native_borg.data());borg_texture->serial=2;
    borg_texture->logical_name=borg_name;
    active_textures.emplace(base_texture->texture,base_texture);
    active_textures.emplace(borg_texture->texture,borg_texture);
    std::array<std::uint32_t,4> texture_slots{{base_texture->texture,0,borg_texture->texture,base_texture->texture}};
    put(mesh_vtable,4,base+0x1968f0);put(mesh,0,addr(mesh_vtable.data()));
    put(mesh,4,object.database);put(mesh,8,addr(mesh_name));
    const float identity34[]{1,0,0,0,1,0,0,0,1,0,0,0};
    std::memcpy(mesh.data()+0x24,identity34,sizeof(identity34));
    put(mesh,0x48,5.f);put(mesh,0x54,3.f);put(mesh,0xbc,std::uint32_t(4));
    put(mesh,0x114,addr(texture_slots.data()));put(mesh,0x11c,std::uint32_t(4));
    put(mesh,0x13c,borg_texture->texture);
    put(db,0x3c,addr(mesh.data()));put(db,0x38,addr(model_name));
    std::shared_ptr<const a2fo::native11::ModelNodeTemplate> baseline;
    need(!c::model_template(object.database,11,baseline,time_error)&&!baseline,
         "no late baseline is invented on cache miss");
    std::shared_ptr<const a2fo::native11::ModelMaterialBaseline> material_baseline;
    need(!c::model_material_baseline(object.database,11,material_baseline,time_error)&&!material_baseline,
         "material baseline cannot be invented on cache miss");
    asset_owned=false;need(!c::observe_model_loaded(asset),"load observer ownership required");asset_owned=true;
    for(const auto& region:c::template_profile::regions){
        for(const auto offset:{std::size_t(0),region.size-1}){
            auto* bytes=image+region.rva+offset;const auto saved=*bytes;
            write(bytes,static_cast<BYTE>(saved^1));
            need(!c::observe_model_loaded(asset),"mutated load-template code rejected");write(bytes,saved);
        }
        for(std::size_t i=0;i<region.relocation_count;++i){
            auto* bytes=image+region.rva+region.relocations[i];std::uint32_t saved=0;
            std::memcpy(&saved,bytes,4);write(bytes,saved^1u);
            need(!c::observe_model_loaded(asset),"mutated template HIGHLOW rejected");write(bytes,saved);
        }
    }
    need(c::observe_model_loaded(asset),"completed load captures real Rust mapped template");
    need(c::model_template(object.database,11,baseline,time_error)&&baseline->model==asset&&
         baseline->data->nodes.size()==1&&baseline->data->nodes[0].flags==4&&
         baseline->data->nodes[0].local[9]==5&&baseline->data->nodes[0].scale==3,
         "owned load-time baseline published");
    need(baseline->data->lods&&baseline->data->lods->controls.empty(),
         "completed load publishes explicit empty LOD metadata without inventing levels");
    const bool material_available=c::model_material_baseline(object.database,11,material_baseline,time_error);
    if(!material_available){std::uint32_t mesh_type_slot=0;std::memcpy(&mesh_type_slot,image+0x2bca00,4);
        std::fprintf(stderr,"material baseline unavailable: %s; type_slot=%08lx expected=%08lx\n",
            time_error.c_str(),static_cast<unsigned long>(mesh_type_slot),static_cast<unsigned long>(base+0x1968f0));}
    need(material_available&&
         material_baseline->model==asset&&material_baseline->authored==baseline->authored&&
         material_baseline->nodes==baseline->data&&material_baseline->data->meshes.size()==1&&
         material_baseline->data->meshes[0].slots.size()==4&&
         material_baseline->data->meshes[0].slots[0]==base_texture&&
         !material_baseline->data->meshes[0].slots[1]&&
         material_baseline->data->meshes[0].slots[2]==borg_texture&&
         material_baseline->data->meshes[0].slots[3]==base_texture&&
         material_baseline->data->meshes[0].borg_alternate==borg_texture,
         "completed loader publishes full raw slots and alternate with exact template owners");
    const auto retained_material=material_baseline;
    texture_slots[0]=borg_texture->texture;texture_slots[3]=borg_texture->texture;
    need(c::model_material_baseline(object.database,11,material_baseline,time_error)&&
         material_baseline->data==retained_material->data&&
         material_baseline->data->meshes[0].slots[0]==base_texture&&
         material_baseline->data->meshes[0].slots[3]==base_texture,
         "later native diffuse and auxiliary slot swaps cannot refresh material baseline");
    active_textures.erase(base_texture->texture);
    need(!c::model_material_baseline(object.database,11,material_baseline,time_error)&&
         material_baseline->data==retained_material->data&&
         c::model_template(object.database,11,baseline,time_error),
         "texture retirement revokes material access independently of node template");
    auto replacement_texture=std::make_shared<a2fo::native11::TextureAsset>(*base_texture);
    replacement_texture->serial=3;active_textures[base_texture->texture]=replacement_texture;
    need(!c::model_material_baseline(object.database,11,material_baseline,time_error),
         "same native texture address with new owner cannot revive material baseline");
    active_textures[base_texture->texture]=base_texture;
    need(c::model_material_baseline(object.database,11,material_baseline,time_error),
         "fixture restores exact retained texture owners");
    bool foreign_material=false;std::thread material_other([&]{
        std::shared_ptr<const a2fo::native11::ModelMaterialBaseline> ignored;std::string error;
        foreign_material=!c::model_material_baseline(object.database,11,ignored,error);
    });material_other.join();need(foreign_material,"foreign material-baseline access refused");
    const auto retained_baseline=baseline;
    effective.parts[0].node_state.local_to_model[0]=3;
    effective.parts[0].node_state.local_to_model[5]=3;
    effective.parts[0].node_state.local_to_model[10]=3;
    effective.parts[0].node_state.local_to_model[3]=5;
    c::observe(object,1,&material_source,&effective);
    need(seeded_matches==1,"owned diagnostic evaluates captured native defaults instead of authored defaults");
    put(mesh,0xbc,std::uint32_t(3));put(mesh,0x48,77.f);put(mesh,0x54,2.f);
    need(c::model_template(object.database,11,baseline,time_error)&&
         baseline->data==retained_baseline->data&&baseline->data->nodes[0].flags==4&&
         baseline->data->nodes[0].local[9]==5&&baseline->data->nodes[0].scale==3,
         "later instance flags and transforms cannot overwrite template");
    need(!c::observe_model_loaded(asset)&&
         c::model_template(object.database,11,baseline,time_error)&&
         baseline->data==retained_baseline->data,"duplicate notification cannot replace baseline owners");
    const auto before_revocation=baseline;
    if(accepted_fleet){
        auto* target=reinterpret_cast<BYTE*>(accepted_fleet)+0x210b38;
        const BYTE saved=*target;write(target,static_cast<BYTE>(saved^1));
        need(!c::model_template(object.database,11,baseline,time_error)&&
             baseline==before_revocation,"lost Fleet constructor table revokes cached template");
        write(target,saved);
        need(c::model_template(object.database,11,baseline,time_error)&&
             baseline->data==before_revocation->data,
             "restored constructor chain admits retained template");
    }
    const auto before_source_revocation=baseline;
    revoke_model=true;pin_calls=0;
    need(!c::model_template(object.database,11,baseline,time_error)&&baseline==before_source_revocation,
         "template source revocation preserves previous output");
    revoke_model=false;active_model=asset;
    asset_owned=false;need(!c::model_template(object.database,11,baseline,time_error),
         "lost load boundary ownership blocks cached baseline");asset_owned=true;
    put(db,0x3c,std::uint32_t(0));
    need(!c::model_template(object.database,11,baseline,time_error),"replaced native root blocks baseline");
    put(db,0x3c,addr(mesh.data()));
    const char foreign_name[]="foreign.sod";put(db,0x38,addr(foreign_name));
    need(!c::model_template(object.database,11,baseline,time_error),"renamed native database blocks baseline");
    put(db,0x38,addr(model_name));
    bool foreign_template=false;std::thread other_template([&]{
        std::shared_ptr<const a2fo::native11::ModelNodeTemplate> ignored;std::string error;
        foreign_template=!c::observe_model_loaded(asset)&&!c::model_template(object.database,11,ignored,error);
    });other_template.join();need(foreign_template,"foreign template capture and access refused");
    const auto before_restart=baseline;
    need(initialize()&&
         !c::model_template(object.database,11,baseline,time_error)&&baseline==before_restart,
         "restart clears cache without sampling last-instance state");
    unsigned short saved_cw=0;asm volatile("fnstcw %0":"=m"(saved_cw));
    for(unsigned pc:{0u,2u,3u})for(unsigned rounding=0;rounding<4;++rounding){
        need(initialize(),"arithmetic observation reinitialize");
        const auto native_cw=static_cast<unsigned short>((saved_cw&~0x0f00u)|(pc<<8)|(rounding<<10));
        inspect_cw=true;reported_cw=0;
        c::observe(object,1,nullptr,nullptr,a2fo::native11::encode_native_x87(native_cw));
        inspect_cw=false;unsigned short after=0;asm volatile("fnstcw %0":"=m"(after));
        need((reported_cw&0x0f3fu)==((native_cw&0x0f00u)|0x003fu)&&after==saved_cw,
             "diagnostic uses transported native precision/rounding and restores callback environment");
    }
    // The view reader uses fresh copied fields, not the native 78/88/8c caches.
    std::vector<BYTE> game(0x90),settings(0x44);
    std::strcpy(object.model_name,"fixture.sod");
    put(engine,0xe8,std::uint32_t(0));put(engine,0xfc,addr(camera.data()));
    put(game,0x80,addr(settings.data()));put(game,0x84,BYTE(0));
    put(settings,0x40,std::uint32_t(1));
    put(instance,0x40,1.f);put(instance,0x78,999.f);put(instance,0x84,BYTE(0));
    put(engine,0x88,999.f);put(engine,0x8c,999.f);
    for(unsigned i=0;i<3;++i){
        put(camera,0xe4+4*i,0.f);
        put(entity,0xac+4*i,i==0?3.f:i==1?4.f:0.f);
    }
    std::uint32_t old_game=0,old_mode=0;BYTE old_skip=0;float old_max_distance=0;
    std::memcpy(&old_game,image+0x36b5ac,4);std::memcpy(&old_mode,image+0x36b610,4);
    std::memcpy(&old_skip,image+0x3637b5,1);std::memcpy(&old_max_distance,image+0x2fcac0,4);
    write(image+0x36b5ac,addr(game.data()));write(image+0x36b610,std::uint32_t(1));
    write(image+0x3637b5,BYTE(0));write(image+0x2fcac0,10.f);
    a2fo::native11::CapturedCraftView view{};
    need(c::sample_view(object,addr(camera.data()),encoded_lod_cw,view,time_error)&&
         view.engine==addr(engine.data())&&view.game==addr(game.data())&&
         view.settings==addr(settings.data())&&view.camera==addr(camera.data())&&
         view.instance==object.instance&&view.database==object.database&&
         view.input.position[0]==3.f&&view.input.position[1]==4.f&&
         std::fabs(view.result.weighted_distance-std::sqrt(10.f))<1e-4f,
         "owner-bound Craft view uses fresh copied position and current camera");
    const auto stable_view=view;
    put(camera,0xe4,1.f);
    need(c::sample_view(object,addr(camera.data()),encoded_lod_cw,view,time_error)&&
         view.input.camera[0]==1.f&&view.result.distance!=stable_view.result.distance,
         "fresh camera movement changes Craft view result");
    const auto moving_view=view;
    need(!c::sample_view(object,addr(camera.data()),0,view,time_error)&&
         view.result.distance==moving_view.result.distance,"unknown Craft view CW preserves output");
    bool foreign_view=false;std::thread view_other([&]{
        a2fo::native11::CapturedCraftView ignored{};std::string error;
        foreign_view=!c::sample_view(object,addr(camera.data()),encoded_lod_cw,ignored,error);
    });view_other.join();need(foreign_view,"foreign Craft view sampling refused");
    auto* view_code=image+c::view_profile::regions[0].rva;
    const auto view_code_byte=*view_code;write(view_code,static_cast<BYTE>(view_code_byte^1));
    need(!c::sample_view(object,addr(camera.data()),encoded_lod_cw,view,time_error)&&
         view.result.distance==moving_view.result.distance&&c::enabled(),
         "mutated Craft view code rejects only sample");
    write(view_code,view_code_byte);
    const auto& view_region=c::view_profile::regions[0];
    need(view_region.relocation_count>0,"Craft view fixture has reviewed HIGHLOW operand");
    auto* view_operand=image+view_region.rva+view_region.relocations[0];
    std::uint32_t view_operand_value=0;std::memcpy(&view_operand_value,view_operand,4);
    write(view_operand,view_operand_value^1u);
    need(!c::sample_view(object,addr(camera.data()),encoded_lod_cw,view,time_error)&&
         view.result.distance==moving_view.result.distance&&c::enabled(),
         "mutated Craft view HIGHLOW rejected without disabling clock");
    write(view_operand,view_operand_value);
    auto* view_slot=image+c::view_profile::slots[0][0];
    std::uint32_t view_slot_value=0;std::memcpy(&view_slot_value,view_slot,4);
    write(view_slot,view_slot_value^1u);
    need(!c::sample_view(object,addr(camera.data()),encoded_lod_cw,view,time_error)&&
         view.result.distance==moving_view.result.distance&&c::enabled(),
         "mutated Craft view slot rejected without disabling clock");
    write(view_slot,view_slot_value);
    revoke_model=true;pin_calls=0;
    need(!c::sample_view(object,addr(camera.data()),encoded_lod_cw,view,time_error)&&
         view.result.distance==moving_view.result.distance,"second model pin revocation preserves view");
    revoke_model=false;active_model=asset;
    need(initialize()&&c::enabled(),"pristine clock and template admission before runtime loss");
    auto* matrix_play=image+0x2161c0;
    const BYTE original_matrix_play=*matrix_play;
    write(matrix_play,static_cast<BYTE>(original_matrix_play^1));
    c::observe(object,1);
    need(!c::enabled(),"matrix-play profile loss suspends runtime diagnostics");
    const auto retained_channels=channels.model.pose;
    need(!c::sample_channels(object,channels,time_error)&&
         channels.model.pose==retained_channels,
         "suspended runtime sampler preserves caller output");
    need(c::observe_model_loaded(asset),
         "completed load still captures a template after runtime suspension");
    need(c::model_template(object.database,11,baseline,time_error)&&
         baseline->model==asset&&baseline->data->nodes.size()==1,
         "load template publishes while runtime clock is suspended");
    const auto suspended_template=baseline;
    need(c::profile::regions[0].rva==0x23f850,"reviewed channel loader profile RVA");
    auto* loader=image+c::profile::regions[0].rva;
    const BYTE original_loader=*loader;
    write(loader,static_cast<BYTE>(original_loader^1));
    need(!c::model_template(object.database,11,baseline,time_error)&&
         baseline==suspended_template,
         "loader profile loss revokes template without changing caller output");
    write(loader,original_loader);
    need(c::model_template(object.database,11,baseline,time_error)&&
         baseline->data==suspended_template->data,"restored loader profile admits cached template");
    write(matrix_play,original_matrix_play);
    const auto before_explicit_disable=baseline;
    c::disable();
    need(!c::model_template(object.database,11,baseline,time_error)&&
         baseline==before_explicit_disable,"explicit disable revokes both producers");
    for(bool channel_capture:{false,true}){
        need(initialize()&&c::observe_model_loaded(asset),"initialize post-capture node-loss case");
        const auto previous_pose=channels.model.pose;
        const auto previous_elapsed=fresh.elapsed;
        a2fo::native11_nodes::owner_checks=0;a2fo::native11_nodes::revoke_at=2;
        const bool captured=channel_capture?c::sample_channels(object,channels,time_error):
            c::sample_time(object,fresh,time_error);
        a2fo::native11_nodes::revoke_at=0;
        need(!captured&&!owned&&!c::enabled()&&channels.model.pose==previous_pose&&
             fresh.elapsed==previous_elapsed,"post-capture node loss refuses sampler output");
        owned=true;
        need(!c::model_template(object.database,11,baseline,time_error)&&
             baseline==before_explicit_disable,
             "restoring node branch cannot revive revoked metadata admission");
    }
    put(mesh,0x11c,std::uint32_t(9));
    need(initialize()&&c::observe_model_loaded(asset)&&
         c::model_template(object.database,11,baseline,time_error)&&
         !c::model_material_baseline(object.database,11,material_baseline,time_error),
         "unavailable load materials do not prevent completed node template");
    put(mesh,0x11c,std::uint32_t(4));
    need(!c::model_material_baseline(object.database,11,material_baseline,time_error)&&
         !c::observe_model_loaded(asset),
         "missing materials cannot be backfilled by later slot repair or duplicate load notification");
    need(initialize()&&c::observe_model_loaded(asset)&&
         c::model_material_baseline(object.database,11,material_baseline,time_error),
         "fresh initialization and completed load can capture new material owners");
    for(const auto rva:{0x231380u,0x1968f0u}){
        const BYTE saved=image[rva];write(image+rva,static_cast<BYTE>(saved^1));
        const auto before=material_baseline;
        need(!c::model_material_baseline(object.database,11,material_baseline,time_error)&&
             material_baseline==before,"lost mesh slot/type source rejects material publication atomically");
        write(image+rva,saved);
    }
    std::uint32_t original_mesh_type=0;std::memcpy(&original_mesh_type,image+0x2bca00,4);
    write(image+0x2bca00,original_mesh_type^1u);
    need(!c::model_material_baseline(object.database,11,material_baseline,time_error),
         "lost mesh vtable source blocks material access");
    write(image+0x2bca00,original_mesh_type);
    asset_owned=false;
    need(!c::model_material_baseline(object.database,11,material_baseline,time_error),
         "lost completed-load observer ownership revokes material publication");asset_owned=true;
    c::disable();
    need(!c::model_material_baseline(object.database,11,material_baseline,time_error),
         "explicit disable revokes material baseline");
    need(initialize()&&!c::model_material_baseline(object.database,11,material_baseline,time_error),
         "restart clears materials without sampling mutable slots");
    c::disable();
    if(animation_fallback){
        write(image+0x2161c0,old_fallback_entry);
        need(VirtualFree(fallback_gateway,0,MEM_RELEASE)!=0,"synthetic fallback gateway released");
        fallback_gateway=nullptr;
    }
    if(accepted_fleet){
        auto* fleet=reinterpret_cast<BYTE*>(accepted_fleet);
        write(image+fleet_profile::root_patch_rva,old_ctor_patch);
        write(image+fleet_profile::root_ctor_rva,old_ctor_entry);
        write(fleet+0x212870,old_chain_table);write(fleet+0x210b38,old_chain_target);
        write(fleet+0x246a18,old_chain_stub);
        for(const auto& original:old_fleet_relocations)write(original.first,original.second);
        need(VirtualFree(constructor_stub,0,MEM_RELEASE)!=0,"private constructor stub released");
        FreeLibrary(accepted_fleet);accepted_fleet=nullptr;
    }
    write(image+0x36b5ac,old_game);write(image+0x36b610,old_mode);
    write(image+0x3637b5,old_skip);write(image+0x2fcac0,old_max_distance);
    for(std::size_t i=0;i<c::view_profile::slots.size();++i)
        write(image+c::view_profile::slots[i][0],old_view_slots[i]);
    write(image+0x2bca00,old_material_type);
    for(const auto& original:old_view_relocations)write(original.first,original.second);
    write(image+0x3ad508,old_engine);write(image+0x3a8e74,old_elapsed);
    need(FreeLibrary(accepted_image)!=0,"inspection map released");accepted_image=nullptr;
    std::puts("PASS animation clock exact-profile, lifetime and diagnostic integration guards");
}
