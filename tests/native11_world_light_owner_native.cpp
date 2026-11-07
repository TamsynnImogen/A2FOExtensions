// Private mapped-image owner handoff. No engine function or graphics call runs.
#include "../core/native11_world_lights.hpp"
#include "../core/native11_world_light_profile.hpp"
#include "../core/native11_light_classes.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
namespace world=a2fo::native11_world_lights;
namespace classes=a2fo::native11_light_classes;
namespace profile=a2fo::native11_world_lights::profile;
namespace {
HMODULE accepted=nullptr;bool identity=true;
void need(bool good,const char* why){if(!good){std::fprintf(stderr,"FAIL %s winerr=%lu\n",why,GetLastError());std::exit(1);}}
std::uint32_t address(const void* p){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));}
bool pinned(const char* path){
    constexpr std::array<BYTE,32> expected{{0x43,0x00,0xab,0x71,0x73,0x1e,0x32,0x3f,
        0x57,0x67,0xd8,0xee,0x05,0x45,0x7e,0x64,0x44,0x40,0x1b,0x82,0x32,0x53,
        0xae,0xbe,0xe8,0x6c,0x5a,0xc2,0x3c,0x2b,0x00,0xde}};
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool good=false;
    if(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&
       CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
        BYTE block[16384]{};DWORD count=0;bool read_ok=true;
        while(true){if(!ReadFile(file,block,sizeof(block),&count,nullptr)){read_ok=false;break;}
            if(!count)break;
            if(!CryptHashData(hash,block,count,0)){read_ok=false;break;}}
        BYTE result[32]{};DWORD size=32;
        good=read_ok&&CryptGetHashParam(hash,HP_HASHVAL,result,&size,0)&&size==32&&
            std::memcmp(result,expected.data(),32)==0;
    }
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);return good;
}
template<class T>void write(void* destination,const T& value){
    DWORD old=0,ignored=0;
    need(VirtualProtect(destination,sizeof(T),PAGE_EXECUTE_READWRITE,&old)!=0,"private map writable");
    std::memcpy(destination,&value,sizeof(T));
    need(VirtualProtect(destination,sizeof(T),old,&ignored)!=0,"private map protection restored");
}
struct Fix {BYTE* place;std::uint32_t original;};
void normalize(std::vector<Fix>& fixes){
    auto* base=reinterpret_cast<BYTE*>(accepted);const auto delta=address(base)-profile::preferred_base;
    for(const auto& region:profile::regions)for(std::size_t i=0;i<region.relocation_count;++i){
        const auto off=region.relocations[i];std::uint32_t original=0,actual=0;
        std::memcpy(&original,region.bytes+off,4);
        std::memcpy(&actual,base+region.rva+off,4);
        need(actual==original||actual==original+delta,"profile HIGHLOW before normalization");
        if(actual==original&&delta){fixes.push_back({base+region.rva+off,actual});
            write(base+region.rva+off,original+delta);}
    }
}
struct Worker {
    HMODULE image=nullptr;
    bool sample=false,result=false;
    DWORD thread=0;
    std::shared_ptr<const world::Snapshot> output;
    std::string error;
};
DWORD WINAPI worker(void* parameter){
    auto& task=*static_cast<Worker*>(parameter);task.thread=GetCurrentThreadId();
    if(task.sample){const float ambient[3]{.04f,.04f,.04f};
        task.result=world::sample(123,2,ambient,task.output,task.error);
    }else task.result=world::initialize(task.image,false,nullptr);
    return 0;
}
void run_worker(Worker& task,DWORD main_thread){
    HANDLE thread=CreateThread(nullptr,0,worker,&task,0,nullptr);
    need(thread!=nullptr,"worker created");
    need(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0,"worker joined within five seconds");
    need(CloseHandle(thread)!=0,"worker handle closed");
    need(task.thread&&task.thread!=main_thread,"distinct Win32 worker thread");
}
}
namespace a2fo::native11_capture {
bool exact_engine_image(HMODULE image) noexcept{return identity&&image==accepted;}
}
int main(int argc,char** argv){
    need(argc==2&&pinned(argv[1]),"pinned 4300 image SHA-256");
    accepted=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(accepted!=nullptr,"private inspection image mapped");
    std::vector<Fix> fixes;normalize(fixes);
    auto* block=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    need(block!=nullptr,"private empty-list allocation");
    const auto base=address(accepted),container=address(block+0x100),sentinel=address(block+0x200);
    std::uint32_t old_list=0;std::memcpy(&old_list,reinterpret_cast<void*>(base+0x361084),4);
    write(reinterpret_cast<void*>(base+0x361084),container);
    write(block+0x104,sentinel);write(block+0x200,sentinel);write(block+0x204,sentinel);
    classes::configure(true,nullptr);classes::dispatch_available(true);
    const auto main_thread=GetCurrentThreadId();
    Worker starter{accepted,false,false,0,{},{}};run_worker(starter,main_thread);
    need(!starter.result&&!world::requested()&&!world::enabled(),
         "startup worker leaves world sampler unrequested and disabled");
    identity=false;
    need(!world::initialize(accepted,true,nullptr)&&!world::enabled(),"unsupported image identity refused");
    identity=true;
    auto* site=reinterpret_cast<BYTE*>(base+profile::regions[0].rva);
    const BYTE original=*site;write(site,static_cast<BYTE>(original^1));
    need(!world::initialize(accepted,true,nullptr)&&!world::enabled(),"changed profile refused");
    write(site,original);
    need(world::initialize(accepted,true,nullptr)&&world::requested()&&world::enabled(),
         "main thread binds checked reader");
    const float ambient[3]{.04f,.04f,.04f};std::string error;
    std::shared_ptr<const world::Snapshot> current;
    need(world::sample(123,1,ambient,current,error)&&error.empty()&&current&&
         current->captured.sources.empty()&&current->captured.members.empty()&&
         current->lighting.lights.empty()&&current->source_lifecycle&&world::current(*current),
         "empty owned world samples successfully with current lifecycle");
    auto held=current;
    Worker foreign{accepted,true,false,0,held,{}};run_worker(foreign,main_thread);
    need(!foreign.result&&!foreign.error.empty()&&foreign.output==held&&
         foreign.output->frame_generation==1,"foreign reader refuses without replacing held snapshot");
    world::disable();Worker reset{accepted,false,false,0,{},{}};run_worker(reset,main_thread);
    need(!reset.result&&!world::requested()&&!world::enabled()&&held->frame_generation==1&&
         !world::current(*held),
         "worker reset leaves old snapshot readable but sampler disabled");
    need(world::initialize(accepted,true,nullptr)&&world::sample(123,2,ambient,current,error)&&
         current&&current!=held&&current->frame_generation==2&&held->frame_generation==1&&
         world::current(*current)&&!world::current(*held),
         "main thread rebinds and recovers fresh sample");
    const auto before_class_change=current;
    const auto old_class_generation=classes::generation();
    need(classes::observe_class(reinterpret_cast<void*>(std::uintptr_t{0x12340}),
                                "fixture.odf",{})&&classes::generation()>old_class_generation&&
         !world::current(*before_class_change),
         "class replacement revokes old world snapshot");
    need(world::sample(123,3,ambient,current,error)&&current&&world::current(*current)&&
         current->class_generation==classes::generation(),
         "fresh class generation produces current world snapshot");
    world::disable();classes::disable();current.reset();held.reset();foreign.output.reset();
    write(reinterpret_cast<void*>(base+0x361084),old_list);
    for(const auto& fix:fixes)write(fix.place,fix.original);
    need(VirtualFree(block,0,MEM_RELEASE)!=0,"private empty list released");
    need(FreeLibrary(accepted)!=0,"private mapped image released");
    std::puts("PASS native11 world light owner handoff");
}
