#include "../core/native11_capture.hpp"
#include "../core/native11_capture_scale_profile.hpp"
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
namespace {
unsigned patches=0,logs=0;bool allow_patch=false,model_seen=false;
void log(const char* message){++logs;if(std::strstr(message,"first=fed_venture.sod"))model_seen=true;}
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s (winerr=%lu)\n",why,GetLastError());std::exit(1);}}
}
// Record the production patch request without patching or invoking game code.
extern "C" bool a2fo_rs_windows_patch_call(void* site,void* target,const std::uint8_t* expected,std::size_t length){
    ++patches;const std::uint8_t signature[]{0x8b,0x11,0xff,0x52,0x14};
    need(site&&target&&length==5&&std::memcmp(expected,signature,5)==0,"guarded patch request contract");
    return allow_patch;
}
int main(int argc,char** argv){
    using namespace a2fo::native11_capture;
    need(!initialize(GetModuleHandleA(nullptr),false,log),"default disabled");
    need(patches==0&&logs==0,"disabled mode does not arm or report errors");
    need(!begin(reinterpret_cast<void*>(0x1000),1),"disabled view refused");
    need(!initialize(GetModuleHandleA(nullptr),true,log),"unknown executable refused");
    need(patches==0&&logs==1,"identity guard rejects before memory patch");
    need(!begin(reinterpret_cast<void*>(0x1000),1),"failed initialization cannot capture");
    if(argc==2){
        // Static mapping only: no imports resolved, no game entrypoint called.
        // This private fixture never jumps to any address in the engine image.
        HMODULE image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        need(image!=nullptr,"map pinned private engine image without execution");
        auto* base=reinterpret_cast<std::uint8_t*>(image);
        {
            namespace p=a2fo::native11_capture::profile;
            const auto numeric=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base));
            const auto delta=numeric-p::preferred_base;
            std::uint32_t slot=0;std::memcpy(&slot,base+0x2b3e38,4);
            // Wine maps an EXE with DONT_RESOLVE as an inspection image: its
            // HIGHLOW fields can remain at the preferred base. This is not a
            // loaded running executable. Prove rejection, then emulate only
            // the guarded loader fixups in these private non-executed pages.
            if(delta&&slot==p::preferred_base+0xcb6b0){
                need(!initialize(image,true,log)&&patches==0,"unrelocated inspection image refused");
                auto relocate=[&](std::uint32_t rva,std::uint32_t expected){
                    std::uint32_t value=0;std::memcpy(&value,base+rva,4);need(value==expected,"private relocation original value");
                    DWORD protection=0;need(VirtualProtect(base+rva,4,PAGE_READWRITE,&protection)!=0,"private loader fixup writable");
                    value+=delta;std::memcpy(base+rva,&value,4);
                    DWORD ignored=0;need(VirtualProtect(base+rva,4,protection,&ignored)!=0,"private loader fixup protection restored");
                };
                for(const auto& region:p::regions){
                    need(std::memcmp(base+region.rva,region.bytes,region.size)==0,"inspection profile matches preferred image");
                    for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
                        std::uint32_t value=0;std::memcpy(&value,region.bytes+offset,4);relocate(region.rva+offset,value);}
                }
                relocate(0x2b3e38,p::preferred_base+0xcb6b0);relocate(0x2b3e40,p::preferred_base+0xcb0e0);
                relocate(0x2bc56c,p::preferred_base+0x2209b0);
                std::puts("Private inspection image: guarded HIGHLOW and vtable loader fixups emulated; no engine code executed");
            }else need(slot==numeric+0xcb6b0,"mapped image has expected loader-relative slot");
        }
        allow_patch=true;
        for(unsigned offset:{0u,14u}) {
            auto* guard=base+0xcfdb0+offset;const auto original=*guard;DWORD protection=0;
            need(VirtualProtect(guard,1,PAGE_READWRITE,&protection)!=0,"private mapped context mutation");
            *guard^=1;
            need(!initialize(image,true,log)&&patches==0,"loaded context mutation rejected despite exact disk image");
            *guard=original;DWORD ignored=0;need(VirtualProtect(guard,1,protection,&ignored)!=0,"restore private context");
        }
        // Check the whole loaded scale profile after relocation, before the
        // checked patch. Only this private, non-executed image is mutated.
        for(const auto& region:a2fo::native11_capture::profile::regions)
            for(std::size_t offset:{std::size_t{0},region.size-1}){
                auto* guard=base+region.rva+offset;const auto original=*guard;DWORD protection=0;
                need(VirtualProtect(guard,1,PAGE_READWRITE,&protection)!=0,"private scale context mutation");*guard^=1;
                need(!initialize(image,true,log)&&patches==0,"loaded scale context mutation refused");
                *guard=original;DWORD ignored=0;need(VirtualProtect(guard,1,protection,&ignored)!=0,"restore scale context");
            }
        for(const auto& region:a2fo::native11_capture::profile::regions)
            for(std::size_t i=0;i<region.relocation_count;++i){
                auto* guard=base+region.rva+region.relocations[i];const auto original=*guard;DWORD protection=0;
                need(VirtualProtect(guard,1,PAGE_READWRITE,&protection)!=0,"private relocation field mutation");*guard^=1;
                need(!initialize(image,true,log)&&patches==0,"normalized relocation field mutation refused");
                *guard=original;DWORD ignored=0;need(VirtualProtect(guard,1,protection,&ignored)!=0,"restore relocation field");
            }
        for(const auto rva:{0x2b3e38u,0x2b3e40u,0x2bc56cu}){
            auto* guard=base+rva;const auto original=*guard;DWORD protection=0;
            need(VirtualProtect(guard,1,PAGE_READWRITE,&protection)!=0,"private vtable mutation");*guard^=1;
            need(!initialize(image,true,log)&&patches==0,"loaded scale vtable slot mutation refused");
            *guard=original;DWORD ignored=0;need(VirtualProtect(guard,1,protection,&ignored)!=0,"restore vtable");
        }
        need(initialize(image,true,log)&&patches==1,"exact target arms with one mocked checked patch");
        std::array<std::uint8_t,0x1c0> camera{};
        std::array<std::uint8_t,0x100> instance{};
        std::array<std::uint8_t,0x20> entity{};
        std::array<std::uint8_t,0x40> database{};
        const float screen[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        const float viewport[4]{0,0,640,480};
        const float world[12]{1,0,0,0,1,0,0,0,1,10,20,30};
        const char* name="fed_venture.sod";void* db=database.data();void* inst=instance.data();
        const auto database_type=reinterpret_cast<std::uintptr_t>(image)+0x2bc56c;
        const auto instance_type=reinterpret_cast<std::uintptr_t>(image)+0x2b3e2c;
        std::memcpy(camera.data()+0x160,screen,sizeof(screen));
        std::memcpy(camera.data()+0x1b0,viewport,sizeof(viewport));
        std::memcpy(instance.data()+0x44,world,sizeof(world));
        std::memcpy(instance.data(),&instance_type,4);
        std::memcpy(instance.data()+0x80,&db,4);std::memcpy(entity.data()+4,&inst,4);
        std::memcpy(database.data(),&database_type,4);std::memcpy(database.data()+0x38,&name,4);
        need(begin(camera.data(),1),"production owner begins");
        need(!begin(camera.data(),1),"production nested owner refused");
        a2fo_native11_observe_entity(entity.data(),camera.data());
        end(camera.data(),true);need(model_seen,"production collector publishes owned selected database name");
        need(!initialize(image,false,log),"configuration disable requires no view wrapper");
        need(!begin(camera.data(),1),"configuration disable refuses new capture");
        disable();
        need(!GetModuleHandleA("d3d8.dll")&&!GetModuleHandleA("d3d9.dll")&&!GetModuleHandleA("D3DX81ab.dll"),"static engine mapping resolved no legacy graphics imports");
        // The private mapped image is process-scoped; no target function ran.
        std::puts("PASS native11 exact engine static mapping, crypto identity, loaded-context mutation rejection, owner/model telemetry; patch mocked, no engine code executed");
    }else need(argc==1,"usage: native11_capture_guard_test.exe [PRIVATE_PINNED_ENGINE_EXE]");
    end(reinterpret_cast<void*>(0x1000),false);disable();
    std::puts("PASS native11 capture default-off and unreviewed-executable guard");
}
