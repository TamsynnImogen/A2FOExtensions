// Controlled mapped-image client of the core world-light sampler. No engine entry calls.
#include "../core/native11_world_lights.hpp"
#include "../core/native11_world_light_profile.hpp"
#include "../core/native11_light_classes.hpp"
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
namespace classes=a2fo::native11_light_classes;
namespace world=a2fo::native11_world_lights;
namespace profile=a2fo::native11_world_lights::profile;
namespace {
void need(bool good,const char* why){if(!good){std::fprintf(stderr,"FAIL %s winerr=%lu\n",why,GetLastError());std::exit(1);}}
std::uint32_t address(const void* value){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value));}
void bytes(void* destination,const void* source,std::size_t count){DWORD previous=0;
    need(VirtualProtect(destination,count,PAGE_EXECUTE_READWRITE,&previous)!=0,"mapped region writable");
    std::memcpy(destination,source,count);DWORD ignored=0;
    need(VirtualProtect(destination,count,previous,&ignored)!=0,"mapped region protection restored");}
template<class T>void put(std::uint32_t where,const T& value){bytes(reinterpret_cast<void*>(where),&value,sizeof(value));}
void word(std::uint32_t where,std::uint32_t value){put(where,value);}
void fix_inspection_relocations(HMODULE image){
    const auto base=address(image),delta=base-profile::preferred_base;
    // Wine's inspection-only EXE mapping may leave HIGHLOW operands preferred.
    // Correct only profiled operands in this private map, never execute it.
    for(const auto& region:profile::regions)for(std::size_t i=0;i<region.relocation_count;++i){
        const auto offset=region.relocations[i];std::uint32_t original=0,actual=0;
        std::memcpy(&original,region.bytes+offset,4);
        std::memcpy(&actual,reinterpret_cast<const void*>(base+region.rva+offset),4);
        need(actual==original||actual==original+delta,"profile inspection relocation");
        if(actual==original&&delta)word(base+region.rva+offset,original+delta);
    }
}
struct Block {std::uint8_t* memory=nullptr;Block(){memory=static_cast<std::uint8_t*>(
    VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));need(memory,"fixture allocation");}
    ~Block(){VirtualFree(memory,0,MEM_RELEASE);}std::uint32_t at(std::uint32_t offset)const{return address(memory+offset);}};
struct Fixture {
    HMODULE image;Block block;
    std::uint32_t base,container,sentinel,first,second,light,disabled,instance;
    static constexpr std::uint32_t light_class=0x17240,disabled_class=0x17280,unknown_class=0x172c0;
    explicit Fixture(HMODULE mapped):image(mapped),base(address(mapped)),container(block.at(0x100)),
        sentinel(block.at(0x200)),first(block.at(0x300)),second(block.at(0x400)),
        light(block.at(0x500)),disabled(block.at(0x600)),instance(block.at(0x700)){
        word(base+0x361084,container);word(container+4,sentinel);
        word(light+40,0xdeadbeef);word(light+0x40,light_class);word(light+4,instance);
        word(disabled+40,0xdeadbeef);word(disabled+0x40,disabled_class);
        pose(-.5f);linked();
    }
    void pose(float x){const float matrix[12]{1,0,0,0,1,0,0,0,1,x,0,0};
        bytes(reinterpret_cast<void*>(instance+0x44),matrix,sizeof(matrix));}
    void linked(){word(sentinel,first);word(sentinel+4,second);
        word(first,second);word(first+4,sentinel);word(first+8,light);
        word(second,sentinel);word(second+4,first);word(second+8,disabled);}
    void only_disabled(){word(sentinel,second);word(sentinel+4,second);
        word(second,sentinel);word(second+4,sentinel);}
};
std::map<std::string,std::string> light_fields(){return {{"lightSource","1"},{"lightType","point"},
    {"lightColor","1 0 0"},{"lightFalloffStart","0"},{"lightFalloffRange","3"}};}
void class_ready(){classes::configure(true,nullptr);classes::dispatch_available(true);}
bool same(const std::shared_ptr<const world::Snapshot>& a,const std::shared_ptr<const world::Snapshot>& b){return a==b;}
void run(const char* path){
    HMODULE image=LoadLibraryExA(path,nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(image,"pinned image mapped without entry calls");
    fix_inspection_relocations(image);
    Fixture fixture(image);
    class_ready();auto fields=light_fields();
    need(classes::observe_class(reinterpret_cast<void*>(Fixture::light_class),"light.odf",fields),
        "copied light class");
    need(classes::observe_class(reinterpret_cast<void*>(Fixture::disabled_class),"dark.odf",
        {{"lightSource","0"}}),"known disabled class");
    need(!world::initialize(image,false,nullptr)&&!world::enabled(),"request gate");
    need(world::initialize(image,true,nullptr)&&world::requested()&&world::enabled(),
        "exact mapped image and profile armed");
    const float ambient[3]{.04f,.04f,.04f};std::string error;
    std::shared_ptr<const world::Snapshot> current;
    need(world::sample(123,1,ambient,current,error)&&error.empty()&&current,
        "complete first sample");
    need(current->camera==123&&current->frame_generation==1&&
        current->class_generation==classes::generation()&&
        current->captured.sources.size()==2&&current->captured.class_owners.size()==2&&
        current->captured.sources[0].config.enabled&&
        !current->captured.sources[1].config.enabled&&
        current->lighting.lights.size()==1&&current->lighting.source_ids.size()==1&&
        current->lighting.source_ids[0]==fixture.light&&
        current->lighting.lights[0].position[0]==-.5f,
        "light pose and known disabled object captured");
    auto held=current;fields["lightColor"]="0 0 0";fixture.pose(.25f);
    need(held->lighting.lights[0].position[0]==-.5f&&
        held->captured.class_owners[0]->config.light.colour[0]==1,
        "snapshot owns copied class and pose");
    need(world::sample(123,2,ambient,current,error)&&current!=held&&
        current->lighting.lights[0].position[0]==.25f,"moved Matrix34 changes world point");
    fixture.only_disabled();need(world::sample(123,3,ambient,current,error)&&
        current->captured.sources.size()==1&&current->lighting.lights.empty()&&
        current->lighting.ambient[0]==.04f,"removed light leaves complete ambient frame");
    fixture.linked();word(fixture.disabled+0x40,Fixture::unknown_class);
    auto before=current;need(!world::sample(123,4,ambient,current,error)&&same(before,current)&&
        !error.empty(),"unknown class refuses without replacing output");
    word(fixture.disabled+0x40,Fixture::disabled_class);
    word(fixture.second+4,fixture.sentinel);
    need(!world::sample(123,4,ambient,current,error)&&same(before,current),
        "broken previous link refuses sample");
    fixture.linked();need(world::sample(123,4,ambient,current,error)&&
        current->lighting.lights.size()==1,"link repair recovers");
    for(const auto& region:profile::regions){
        auto* site=reinterpret_cast<std::uint8_t*>(fixture.base+region.rva);
        const std::uint8_t original=*site,corrupt=static_cast<std::uint8_t>(original^0x5a);
        bytes(site,&corrupt,1);
        need(!world::initialize(image,true,nullptr)&&!world::enabled(),
            "each profile mismatch blocks arming");
        bytes(site,&original,1);
        need(world::initialize(image,true,nullptr)&&world::enabled(),
            "restored profile re-arms");
    }
    fields["lightType"]="broken";
    need(!classes::observe_class(reinterpret_cast<void*>(Fixture::light_class),"light.odf",fields),
        "invalid reload faults class");
    before=current;need(!world::sample(123,5,ambient,current,error)&&same(before,current),
        "faulted class blocks publication and retains prior output");
    fields=light_fields();need(classes::observe_class(reinterpret_cast<void*>(Fixture::light_class),
        "light.odf",fields),"valid reload recovers class");
    need(world::sample(123,5,ambient,current,error)&&current->lighting.lights.size()==1,
        "class recovery permits sample");
    world::disable();before=current;
    need(!world::sample(123,6,ambient,current,error)&&same(before,current)&&!world::enabled(),
        "disabled sampler cannot publish");
    need(world::initialize(image,true,nullptr)&&world::sample(123,6,ambient,current,error)&&
        current->frame_generation==6,"rearmed sampler publishes fresh frame");
    need(held->lighting.lights[0].position[0]==-.5f&&
        held->captured.class_owners[0]->config.light.colour[0]==1,
        "old held snapshot survives reload and disable");
    current.reset();before.reset();held.reset();world::disable();classes::disable();
    word(fixture.base+0x361084,0);need(FreeLibrary(image)!=0,"mapped image released last");
}
}
int main(int argc,char** argv){need(argc==2,"usage: native11_world_light_native.exe PINNED_ArmadaL.exe");
    run(argv[1]);std::puts("PASS native11 core world-light sampler");}
