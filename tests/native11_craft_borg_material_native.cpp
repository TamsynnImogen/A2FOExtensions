// Original CPU selector/code cave/fast setter on owned synthetic state only.
// Existing vectors are 4..8 slots, so no native allocator, startup, UI or GPU runs.
#include "../renderer/native11/craft_borg_material_policy.hpp"
#include "../renderer/native11/craft_borg_material_capture.hpp"
#include "../core/native11_borg_material_source.hpp"
#include "../core/native11_borg_material_profile.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
using namespace a2fo::native11;
namespace p=a2fo::native11_borg_material::profile;
namespace s=a2fo::native11_borg_material::source;
extern "C" void invoke_borg_material(void*,void*,std::uint32_t);
extern "C" void record_selector_this();
extern "C" void record_common_this();
extern "C" void record_init_role_this();
extern "C" void invoke_material_initializer(void*,void*);
asm(".text\n.globl _invoke_borg_material\n_invoke_borg_material:\n"
    "pushl %ebp\nmovl %esp,%ebp\nmovl 12(%ebp),%ecx\npushl 16(%ebp)\n"
    "call *8(%ebp)\nleave\nret\n");
asm(".text\n.globl _record_selector_this\n_record_selector_this:\n"
    "pushl 4(%esp)\npushl %ecx\ncall _record_selector\naddl $8,%esp\nret $4\n"
    ".globl _record_common_this\n_record_common_this:\n"
    "pushl 4(%esp)\npushl %ecx\ncall _record_common\naddl $8,%esp\nret $4\n");
asm(".text\n.globl _record_init_role_this\n_record_init_role_this:\n"
    "pushl 8(%esp)\npushl 8(%esp)\npushl %ecx\ncall _record_init_role\naddl $12,%esp\nret $8\n"
    ".globl _invoke_material_initializer\n_invoke_material_initializer:\n"
    "pushl %ebp\nmovl %esp,%ebp\nsubl $16,%esp\nmovl $0,-12(%ebp)\n"
    "pushl %ebx\npushl %esi\npushl %edi\nmovl 12(%ebp),%esi\nxorl %ebx,%ebx\n"
    "call *8(%ebp)\npopl %edi\npopl %esi\npopl %ebx\nleave\nret\n");
namespace {
void need(bool good,const char* why){if(!good){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
std::uint32_t address(const void* value){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value));}
constexpr std::array<BYTE,32> expected{{0x43,0x00,0xab,0x71,0x73,0x1e,0x32,0x3f,0x57,0x67,0xd8,0xee,0x05,0x45,0x7e,0x64,0x44,0x40,0x1b,0x82,0x32,0x53,0xae,0xbe,0xe8,0x6c,0x5a,0xc2,0x3c,0x2b,0x00,0xde}};
bool pinned(const char* path){
    const auto file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool okay=false;
    if(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
        std::array<BYTE,16384> block{};DWORD count=0;bool good=true;
        while(true){if(!ReadFile(file,block.data(),block.size(),&count,nullptr)){good=false;break;}
            if(!count)break;
            if(!CryptHashData(hash,block.data(),count,0)){good=false;break;}}
        std::array<BYTE,32> digest{};DWORD length=digest.size();
        okay=good&&CryptGetHashParam(hash,HP_HASHVAL,digest.data(),&length,0)&&length==digest.size()&&digest==expected;
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);return okay;
}
template<class T>void patch(void* location,const T& value){DWORD old=0,ignored=0;
    need(VirtualProtect(location,sizeof(value),PAGE_EXECUTE_READWRITE,&old)!=0,"private patch protection");
    std::memcpy(location,&value,sizeof(value));
    need(VirtualProtect(location,sizeof(value),old,&ignored)!=0,"private patch restore");
}
template<class T,std::size_t N>void put(std::array<BYTE,N>& bytes,std::size_t offset,T value){
    need(offset+sizeof(value)<=N,"owned native buffer span");std::memcpy(bytes.data()+offset,&value,sizeof(value));
}
std::uint32_t random_word(std::uint32_t& seed){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
struct Mesh {std::array<BYTE,0x140> native{};std::array<std::uint32_t,8> slots{};};
unsigned selector_calls=0,common_calls=0;
std::uint32_t selected_class=0,selected_flag=0,drawn_instance=0,drawn_camera=0;
extern "C" __attribute__((used)) void record_selector(void* klass,std::uint32_t flag){
    ++selector_calls;selected_class=address(klass);selected_flag=flag&255u;
}
extern "C" __attribute__((used)) void record_common(void* instance,std::uint32_t camera){
    ++common_calls;drawn_instance=address(instance);drawn_camera=camera;
}
bool read_owned(void*,std::uint32_t at,void* out,std::size_t size){
    SIZE_T copied=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(at),out,size,&copied)&&copied==size;
}
std::shared_ptr<const LightClassRecord> pin_owned_class(void* raw,std::uint64_t id){
    const auto& value=*static_cast<const std::shared_ptr<const LightClassRecord>*>(raw);
    return value&&value->class_id==id?value:nullptr;
}
unsigned route_comparisons(BYTE* base){
    std::array<BYTE,5> selector{},common{};std::memcpy(selector.data(),base+0xcb2ab,5);std::memcpy(common.data(),base+0xcb318,5);
    const auto redirect=[&](std::uint32_t at,std::uint32_t target){
        std::array<BYTE,5> call{{0xe8,0,0,0,0}};const auto displacement=target-(address(base)+at+5);
        std::memcpy(call.data()+1,&displacement,4);patch(base+at,call);
    };
    redirect(0xcb2ab,address(reinterpret_cast<const void*>(&record_selector_this)));
    redirect(0xcb318,address(reinterpret_cast<const void*>(&record_common_this)));
    std::uint32_t render_slot=0;std::memcpy(&render_slot,base+0x2b3e40,4);
    patch(base+0x2b3e40,address(base)+0xcb0e0);
    std::array<BYTE,0x44> entity{};
    std::array<BYTE,0xa8> instance{};
    std::array<BYTE,0x42c> klass{};
    std::array<BYTE,0x40> database{},other{};
    std::array<BYTE,0xc0> damage{};
    put(entity,4,address(instance.data()));put(entity,0x40,address(klass.data()));
    put(instance,0,address(base)+0x2b3e2c);put(instance,0x80,address(database.data()));
    put(instance,0x98,address(klass.data()));put(klass,0,std::uint32_t{0x12345678});
    auto owner=std::make_shared<LightClassRecord>();owner->class_id=address(klass.data());owner->version=1;
    std::shared_ptr<const LightClassRecord> held_owner=owner;
    auto model=std::make_shared<ModelAsset>();model->info.database=address(database.data());model->info.serial=1;
    CapturedCraftPreparation held;held.member={address(entity.data()),address(instance.data()),address(klass.data()),1};
    held.database=model->info.database;held.model=model;held.object_class=held_owner;
    unsigned comparisons=0;
    for(unsigned primary=0;primary<3;++primary)for(unsigned has_damage=0;has_damage<2;++has_damage)
      for(unsigned flag=0;flag<256;++flag){
        put(klass,0x1dc,primary==1?address(database.data()):primary==2?address(other.data()):0u);
        put(database,0x30,has_damage?address(damage.data()):0u);
        put(damage,0xbc,std::uint32_t{0x6abcdefc});instance[0xa1]=static_cast<BYTE>(flag);
        selector_calls=common_calls=0;selected_class=selected_flag=drawn_instance=drawn_camera=0;
        const auto before_class=klass;const auto before_instance=instance;
        const auto before_entity=entity;const auto before_database=database;
        const std::uint32_t camera=0xcafe0000u+flag;
        invoke_borg_material(base+0xcb0e0,instance.data(),camera);
        CraftBorgMaterialRoute route;std::string error;
        need(capture_craft_borg_material_route(held,{address(base)+0x2b3e2c,address(base)+0xcb0e0},
            nullptr,read_owned,&held_owner,pin_owned_class,route,error),error.c_str());
        need(route.selector_reached==(primary==1)&&selector_calls==(route.selector_reached?1u:0u),
             "native primary/alternate branch matches captured selector reachability");
        need(!selector_calls||(selected_class==address(klass.data())&&selected_flag==flag),
             "native selector receives instance class and full byte Borg argument");
        need(common_calls==1&&drawn_instance==address(instance.data())&&drawn_camera==camera,
             "native primary and bypass paths preserve final common render call identity");
        need(klass==before_class&&instance==before_instance&&entity==before_entity&&database==before_database,
             "route CPU call leaves logical identity and preparation bytes untouched");
        std::uint32_t damage_flags=0;std::memcpy(&damage_flags,damage.data()+0xbc,4);
        need(damage_flags==(has_damage?(0x6abcdefcu|(primary==1?0u:3u)):0x6abcdefcu),
             "only native database damage role flags change on bypass");
        ++comparisons;
    }
    patch(base+0xcb2ab,selector);patch(base+0xcb318,common);patch(base+0x2b3e40,render_slot);
    return comparisons;
}
std::array<std::array<std::uint32_t,2>,64> init_table{};
std::uint32_t init_database=0,init_image=0,init_roles=0,init_allocations=0,init_size=0;
bool init_bad_role=false;
extern "C" __attribute__((used)) std::uint32_t record_init_role(void* db,
    std::uint32_t name,std::uint32_t type){
    if(address(db)!=init_database||type!=4||name!=init_image+0x303294+8*init_roles)init_bad_role=true;
    ++init_roles;return 0;
}
extern "C" __attribute__((used)) void* record_init_allocation(std::uint32_t size){
    ++init_allocations;init_size=size;return init_table.data();
}
extern "C" __attribute__((used)) std::uint32_t init_nonmesh_type(){return 0;}
unsigned initializer_comparisons(BYTE* base){
    std::vector<std::pair<BYTE*,std::array<BYTE,5>>> saved;
    const auto redirect=[&](std::uint32_t at,std::uint32_t target){
        std::array<BYTE,5> old{},call{{0xe8,0,0,0,0}};std::memcpy(old.data(),base+at,5);
        saved.push_back({base+at,old});const auto displacement=target-(address(base)+at+5);
        std::memcpy(call.data()+1,&displacement,4);patch(base+at,call);
    };
    for(unsigned k=0;k<7;++k)redirect(0xbf7f5+20*k,address(reinterpret_cast<const void*>(&record_init_role_this)));
    redirect(0xbf8d1,address(reinterpret_cast<const void*>(&record_init_allocation)));
    // Execute the original inlined constructor table block, stopping immediately
    // after its count store. Its enclosing constructor/SEH epilogue is not run.
    const auto return_byte=base[0xbf945];patch(base+0xbf945,BYTE{0xc3});
    init_image=address(base);unsigned comparisons=0;
    struct InitNode {Mesh mesh;std::array<std::uint32_t,2> vtable{};bool eligible=false;};
    for(unsigned count:{0u,1u,2u,7u,24u,64u})for(unsigned slots:{0u,1u,4u,8u})for(unsigned trial=0;trial<16;++trial){
        std::uint32_t seed=0xd1c3a1u+count*31+slots*71+trial;
        std::vector<InitNode> nodes(count);std::vector<std::array<BYTE,0x140>> before;
        std::vector<std::array<std::uint32_t,8>> before_slots;
        std::array<BYTE,0x40> database{};std::array<BYTE,0x450> klass{};klass.fill(0xa5);
        for(unsigned k=0;k<count;++k){auto& node=nodes[k];auto& mesh=node.mesh;mesh.native.fill(0x5a);
            const bool is_mesh=(random_word(seed)%3)!=0;
            const auto alternate=(random_word(seed)%3)?random_word(seed)|1u:0u;
            node.eligible=is_mesh&&alternate;
            node.vtable[1]=is_mesh?address(base)+0x1968f0:address(reinterpret_cast<const void*>(&init_nonmesh_type));
            put(mesh.native,0,address(node.vtable.data()));
            put(mesh.native,0x114,address(mesh.slots.data()));put(mesh.native,0x11c,slots);put(mesh.native,0x13c,alternate);
            for(auto& slot:mesh.slots)slot=random_word(seed);
            put(mesh.native,0x1c,2*k+1<count?address(nodes[2*k+1].mesh.native.data()):0u);
            put(mesh.native,0x20,k&&k%2&&k+1<count?address(nodes[k+1].mesh.native.data()):0u);
            before.push_back(mesh.native);
            before_slots.push_back(mesh.slots);
        }
        put(database,0x3c,count?address(nodes[0].mesh.native.data()):0u);
        put(klass,0x1d8,address(database.data()));auto wanted=klass;
        for(unsigned offset:{0x408u,0x418u,0x41cu,0x414u,0x40cu,0x410u,0x420u})put(wanted,offset,0u);
        std::vector<std::array<std::uint32_t,2>> expected;std::vector<unsigned> pending;
        if(count)pending.push_back(0);
        while(!pending.empty()){const auto k=pending.back();pending.pop_back();const auto& node=nodes[k];
            if(node.eligible)expected.push_back({address(node.mesh.native.data()),slots?node.mesh.slots[0]:0u});
            if(2*k+1<count)pending.push_back(2*k+1);
            if(k&&k%2&&k+1<count)pending.push_back(k+1);
        }
        init_table.fill({{0xdeadbeef,0xdeadbeef}});init_roles=init_allocations=init_size=0;
        init_bad_role=false;init_database=address(database.data());const auto original_db=database;
        invoke_material_initializer(base+0xbf7e6,klass.data());
        put(wanted,0x424,static_cast<std::uint32_t>(expected.size()));
        put(wanted,0x428,expected.empty()?0u:address(init_table.data()));
        need(klass==wanted&&database==original_db,"original inlined initializer writes only saved table and role fields");
        need(init_roles==7&&!init_bad_role,"initializer performs seven exact named role lookups");
        need(init_allocations==(expected.empty()?0u:1u)&&init_size==expected.size()*8,
             "initializer requests exact saved row allocation, with no native allocator");
        for(std::size_t k=0;k<expected.size();++k)need(init_table[k]==expected[k],"native initialized row order and originals match owned topology policy");
        for(std::size_t k=expected.size();k<init_table.size();++k)
            need(init_table[k]==std::array<std::uint32_t,2>{{0xdeadbeef,0xdeadbeef}},"initializer never writes beyond saved rows");
        for(unsigned k=0;k<count;++k)need(nodes[k].mesh.native==before[k]&&nodes[k].mesh.slots==before_slots[k],
            "initializer does not modify mesh headers or current slots");
        ++comparisons;
    }
    for(const auto& old:saved)patch(old.first,old.second);
    patch(base+0xbf945,return_byte);return comparisons;
}
}
int main(int argc,char** argv){
    const bool route_only=argc==3&&std::strcmp(argv[2],"--route")==0;
    const bool init_only=argc==3&&std::strcmp(argv[2],"--init")==0;
    need((argc==2||route_only||init_only)&&pinned(argv[1]),"pinned original base/Borg CPU image");
    const auto image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(image!=nullptr,"private inspection image without startup");auto* base=reinterpret_cast<BYTE*>(image);
    const auto delta=address(image)-p::preferred_base;
    std::vector<std::pair<BYTE*,std::uint32_t>> originals;
    for(const auto& region:p::regions)for(std::size_t i=0;i<region.relocation_count;++i){
        const auto offset=region.relocations[i];std::uint32_t wanted=0,actual=0;
        std::memcpy(&wanted,region.bytes+offset,4);std::memcpy(&actual,base+region.rva+offset,4);
        need(actual==wanted||actual==wanted+delta,"original or relocated inspected HIGHLOW");
        if(delta&&actual==wanted){originals.emplace_back(base+region.rva+offset,actual);patch(base+region.rva+offset,wanted+delta);}
    }
    need(!s::matches(nullptr)&&!s::matches(GetModuleHandleW(nullptr))&&s::matches(image),"exact bounded source admission");
    unsigned mutations=0;
    for(const auto& region:p::regions){
        for(const auto offset:{std::size_t(0),region.size-1}){auto* location=base+region.rva+offset;const auto saved=*location;
            patch(location,static_cast<BYTE>(saved^1));need(!s::matches(image),"changed source byte rejected");
            patch(location,saved);++mutations;}
        for(std::size_t k=0;k<region.relocation_count;++k){auto* location=base+region.rva+region.relocations[k];std::uint32_t saved=0;
            std::memcpy(&saved,location,4);patch(location,saved^1u);need(!s::matches(image),"changed HIGHLOW rejected");
            patch(location,saved);++mutations;}
        DWORD old=0,ignored=0;need(VirtualProtect(base+region.rva,region.size,PAGE_READWRITE,&old)!=0,"nonexecuting source fixture");
        need(!s::matches(image),"nonexecuting code rejected");
        need(VirtualProtect(base+region.rva,region.size,old,&ignored)!=0,"source execution restored");++mutations;
    }
    need(s::matches(image),"all source bytes/protections restored before CPU comparisons");
    if(init_only){
        const auto comparisons=initializer_comparisons(base);
        need(s::matches(image),"initializer recorders restored original source");
        for(const auto& original:originals)patch(original.first,original.second);
        need(FreeLibrary(image)!=0,"initializer inspection image released");
        std::printf("PASS %u original inlined constructor material table comparisons; %u source mutations; no native allocation/startup/simulation/graphics\n",comparisons,mutations);
        return 0;
    }
    if(route_only){
        const auto comparisons=route_comparisons(base);
        need(s::matches(image),"route recorders restored original source");
        for(const auto& original:originals)patch(original.first,original.second);
        need(FreeLibrary(image)!=0,"route inspection image released");
        std::printf("PASS %u original Craft selector route comparisons; %u source mutations; recorded calls only, no startup/simulation/graphics\n",comparisons,mutations);
        return 0;
    }
    unsigned comparisons=0;
    for(unsigned count:{0u,1u,2u,7u,24u,64u})for(unsigned slots=4;slots<=8;++slots)
      for(unsigned flag:{0u,1u,2u,128u,255u})for(unsigned trial=0;trial<32;++trial){
        std::uint32_t seed=0x31a27u+trial+slots*71u+count*181u;
        std::vector<Mesh> meshes(count);std::vector<CraftBorgMaterialRow> rows;
        std::vector<std::array<std::uint32_t,2>> table(count);
        std::vector<std::array<BYTE,0x140>> before;
        std::vector<std::array<std::uint32_t,8>> wanted;
        for(unsigned i=0;i<count;++i){auto& mesh=meshes[i];mesh.native.fill(0x5a);
            for(auto& value:mesh.slots)value=random_word(seed);
            const auto original=trial%4?random_word(seed):0u;const auto alternate=random_word(seed)|1u;
            put(mesh.native,0x114,address(mesh.slots.data()));put(mesh.native,0x118,slots);
            put(mesh.native,0x11c,slots);put(mesh.native,0x13c,alternate);
            rows.push_back({i,original,alternate,slots});table[i]={{address(mesh.native.data()),original}};
            before.push_back(mesh.native);wanted.push_back(mesh.slots);
        }
        std::array<BYTE,0x42c> object_class{};object_class.fill(0x3c);
        put(object_class,0x424,count);put(object_class,0x428,address(table.data()));
        const auto original_class=object_class;const auto original_table=table;
        // Two consecutive selections prove that a Borg selection preserves the
        // caller's auxiliary slot, including a preceding base operation.
        for(const unsigned selected:{flag,flag?0u:255u}){
            std::vector<CraftMaterialSlotWrite> writes;std::string error;
            need(craft_borg_material_writes(rows,static_cast<std::uint8_t>(selected),writes,error),error.c_str());
            for(const auto& write:writes)wanted[write.node_index][write.slot]=write.texture;
            invoke_borg_material(base+0xbfb70,object_class.data(),0xa5000000u|selected);
            need(object_class==original_class&&table==original_table,"native class and saved rows remain untouched");
            for(unsigned i=0;i<count;++i){need(meshes[i].native==before[i],"native mesh header remains untouched");
                need(meshes[i].slots==wanted[i],"native slot bytes and preserved slots match owned write policy");}
            ++comparisons;
        }
    }
    need(s::matches(image),"CPU comparisons leave native source unchanged");
    for(const auto& original:originals)patch(original.first,original.second);
    need(FreeLibrary(image)!=0,"private image released");
    std::printf("PASS %u original CPU base/Borg state comparisons; %u source mutations; no startup/allocation/GPU\n",comparisons,mutations);
}
