#include "refit_scene_test.hpp"
#include "refit_scene_test_profile.hpp"
#include "native11_capture.hpp"
#include "extension_roots.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace a2fo::refit_scene_test {
namespace {
HMODULE image=nullptr;DWORD owner=0;Log logger=nullptr;
bool enabled=false,attempted=false;unsigned frames=0;
bool scene_ready=false;unsigned control=0;
bool light_probe=false;
void report(const char* text) noexcept {try{if(logger)logger(text);}catch(...){}}
bool read(std::uintptr_t p,void* out,std::size_t n) noexcept {
    SIZE_T got=0;return p&&n<=UINTPTR_MAX-p&&ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<void*>(p),out,n,&got)&&got==n;
}
bool profile_matches() noexcept {
    const auto base=reinterpret_cast<std::uintptr_t>(image);
    for(const auto& r:profile::regions){std::array<std::uint8_t,128> bytes{};
        if(r.size>bytes.size()||!read(base+r.rva,bytes.data(),r.size))return false;
        for(std::size_t i=0;i<r.relocation_count;++i){auto offset=r.relocations[i];
            std::uint32_t v=0;std::memcpy(&v,bytes.data()+offset,4);
            v-=static_cast<std::uint32_t>(base)-0x400000u;std::memcpy(bytes.data()+offset,&v,4);
        }
        if(std::memcmp(bytes.data(),r.bytes,r.size))return false;
    }
    return true;
}
}
void initialize(HMODULE module,const std::string& root,Log log) noexcept {
    logger=log;
    try{
        char marker[16]{},prefix[64]{},child[128]{};
        if(GetEnvironmentVariableA("A2FO_REFIT_HEADLESS_SCENE",marker,sizeof(marker))!=1||marker[0]!='1'||
           !GetEnvironmentVariableA("WINEPREFIX",prefix,sizeof(prefix))||std::strcmp(prefix,"/mnt/prefix")||
           !GetEnvironmentVariableA("A2FO_REFIT_CHILD",child,sizeof(child))||
           std::strncmp(child,"FO4 Refit Live ",15))return;
        bool requested=false;char proof_marker[16]{};
        const bool proof_requested=GetEnvironmentVariableA("A2FO_REFIT_HEADLESS_MATERIAL",proof_marker,sizeof(proof_marker))==1&&proof_marker[0]=='1';
        const auto roots=a2fo::discover_extension_roots(root,GetCommandLineA());
        for(const auto& r:roots.roots){char value[16]{};
            GetPrivateProfileStringA("Refit","HeadlessSceneTest",requested?"1":"0",value,sizeof(value),
                (r+"\\A2FORenderer.ini").c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1"))return;
            requested=std::strcmp(value,"1")==0;
            char light[16]{};
            GetPrivateProfileStringA("Refit","HeadlessLightProbe",light_probe?"1":"0",light,sizeof(light),
                (r+"\\A2FORenderer.ini").c_str());
            if(std::strcmp(light,"0")&&std::strcmp(light,"1"))return;
            light_probe=std::strcmp(light,"1")==0;
            if(proof_requested){char mode[16]{};
                GetPrivateProfileStringA("Refit","HeadlessMaterialControl",control==1?"white":control==2?"black":"none",
                    mode,sizeof(mode),(r+"\\A2FORenderer.ini").c_str());
                if(!std::strcmp(mode,"none"))control=0;
                else if(!std::strcmp(mode,"white"))control=1;
                else if(!std::strcmp(mode,"black"))control=2;
                else return;
            }
        }
        if(!requested||!native11_capture::exact_engine_image(module))return;
        image=module;owner=GetCurrentThreadId();enabled=profile_matches();
        report(enabled?"[RefitTestScene] isolated native scene fixture armed; no input/window operation":
            "[RefitTestScene] exact native call profile refused");
    }catch(...){enabled=false;}
}
void before_world(void* camera,unsigned main) noexcept {
    if(!enabled||attempted||!camera||!(main&0xffu)||GetCurrentThreadId()!=owner||++frames<8)return;
    if(!profile_matches()){attempted=true;report("[RefitTestScene] native call ownership changed; fixture refused");return;}
    const auto base=reinterpret_cast<std::uintptr_t>(image);
    // ForceUpdate dereferences all three scanners; defer fixture admission until
    // a loaded world owns them. No scanner field is written by the fixture.
    for(const auto rva:{0x338a54u,0x338a58u,0x338a5cu,0x36b8d4u,0x363370u}){
        std::uint32_t p=0;std::uint8_t byte=0;
        const bool scanner=rva==0x338a54u||rva==0x338a58u||rva==0x338a5cu;
        if(!read(base+rva,&p,4)||!p||!read(p,&byte,1)||
           (scanner&&(p>UINT32_MAX-0x19||!read(p+0x19,&byte,1)))){
            if(frames>=120){attempted=true;report("[RefitTestScene] loaded-world prerequisites absent; fixture refused");}
            return;
        }
    }
    attempted=true;
    try{
        using Fog=void(__cdecl*)(int);using Update=void(__cdecl*)();
        using Build=void*(__cdecl*)(char*,int,const float*);
        using Center=void(__fastcall*)(void*,void*,int);
        auto fog=reinterpret_cast<Fog>(base+0x935d0);
        auto update=reinterpret_cast<Update>(base+0x93600);
        auto build=reinterpret_cast<Build>(base+0x51990);
        auto center=reinterpret_cast<Center>(base+0x55190);
        fog(0);update();
        const char* names[]={light_probe?"refit_light_probe":"fed_venture","fed_mediterraneanY"};
        int first=0;
        for(unsigned i=0;i<2;++i){
            char odf[32]{};std::snprintf(odf,sizeof(odf),"%s",names[i]);
            const float world[12]={1,0,0,0,1,0,0,0,1,float(i)*300.f,0,0};
            void* object=build(odf,1,world);std::uint32_t handle=0;
            const auto address=reinterpret_cast<std::uintptr_t>(object);
            const bool valid=object&&address<=UINTPTR_MAX-0x2c&&read(address+0x28,&handle,4)&&handle!=0;
            char message[160]{};
            std::snprintf(message,sizeof(message),"[RefitTestScene] %s created=%u handle=%08x",names[i],valid?1u:0u,handle);
            report(message);if(valid&&!first){static_assert(sizeof(first)==sizeof(handle));std::memcpy(&first,&handle,sizeof(first));}
        }
        // The checked native body ignores ECX; use the documented script
        // singleton token and the engine's own camera command, never input.
        if(first){center(reinterpret_cast<void*>(base+0x335c40),nullptr,first);scene_ready=true;
            report("[RefitTestScene] fog/shroud disabled and native camera command issued for isolated fixture");}
        else report("[RefitTestScene] no usable craft handle; native camera command skipped");
    }catch(...){report("[RefitTestScene] native diagnostic fixture failed");}
}
unsigned material_control() noexcept {
    return enabled&&scene_ready&&control&&GetCurrentThreadId()==owner&&profile_matches()?control:0;
}
}
