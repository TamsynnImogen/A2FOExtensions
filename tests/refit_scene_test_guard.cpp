#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../core/extension_roots.hpp"

// The CPU gate is tested against one deterministic extension root. No engine
// function bytes are executed: all five loaded-world prerequisites stay zero.
namespace a2fo {
ExtensionRootDiscovery discover_extension_roots(const std::string& root,const std::string&) {
    ExtensionRootDiscovery out;out.roots.push_back(root);return out;
}
}
namespace a2fo::native11_capture {
unsigned exact_calls=0;bool exact_allowed=false;
bool exact_engine_image(HMODULE) noexcept {++exact_calls;return exact_allowed;}
}
#include "../core/refit_scene_test.cpp"

namespace a2fo::refit_scene_test {
void fixture_reset() noexcept {
    image=nullptr;owner=0;logger=nullptr;enabled=false;attempted=false;frames=0;
}
bool fixture_enabled() noexcept {return enabled;}
bool fixture_attempted() noexcept {return attempted;}
unsigned fixture_frames() noexcept {return frames;}
}

namespace {
void need(bool yes,const char* why) {
    if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}
}
std::string last_log;unsigned log_calls=0;
void log(const char* message) {last_log=message;++log_calls;}
struct Env {
    std::string name,old;bool had=false;
    explicit Env(const char* key):name(key) {
        char buffer[512]{};DWORD n=GetEnvironmentVariableA(key,buffer,sizeof(buffer));
        had=n>0&&n<sizeof(buffer);if(had)old.assign(buffer,n);
    }
    ~Env(){SetEnvironmentVariableA(name.c_str(),had?old.c_str():nullptr);}
};
void set(const char* key,const char* value) {
    need(SetEnvironmentVariableA(key,value)!=FALSE,"set private fixture environment");
}
struct Fixture {
    unsigned char* base=nullptr;
    std::string root,ini;
    Fixture() {
        base=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x405000,
            MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        need(base!=nullptr,"allocate synthetic Root image");
        namespace p=a2fo::refit_scene_test::profile;
        const auto delta=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base))-0x400000u;
        for(const auto& region:p::regions) {
            std::array<std::uint8_t,128> bytes{};
            need(region.size<=bytes.size(),"profile region fits fixture");
            std::memcpy(bytes.data(),region.bytes,region.size);
            for(std::size_t i=0;i<region.relocation_count;++i) {
                const auto at=region.relocations[i];std::uint32_t value=0;
                std::memcpy(&value,bytes.data()+at,4);value+=delta;
                std::memcpy(bytes.data()+at,&value,4);
            }
            std::memcpy(base+region.rva,bytes.data(),region.size);
        }
        char path[MAX_PATH]{};DWORD n=GetTempPathA(sizeof(path),path);
        need(n>0&&n<sizeof(path),"private Wine temp path");
        char name[MAX_PATH]{};need(GetTempFileNameA(path,"rsg",0,name)!=0,"private INI path");
        need(DeleteFileA(name)!=FALSE&&CreateDirectoryA(name,nullptr)!=FALSE,"private INI directory");
        root=name;ini=root+"\\A2FORenderer.ini";
    }
    ~Fixture(){DeleteFileA(ini.c_str());RemoveDirectoryA(root.c_str());VirtualFree(base,0,MEM_RELEASE);}
    void ini_value(const char* value) {
        if(value)need(WritePrivateProfileStringA("Refit","HeadlessSceneTest",value,ini.c_str())!=FALSE,
                      "write private INI gate");
        else DeleteFileA(ini.c_str());
    }
    void reset() {
        a2fo::refit_scene_test::fixture_reset();
        a2fo::native11_capture::exact_calls=0;
        last_log.clear();log_calls=0;
    }
    void arm() {a2fo::refit_scene_test::initialize(reinterpret_cast<HMODULE>(base),root,log);}
    void inert(const char* why) {
        need(!a2fo::refit_scene_test::fixture_enabled()&&
             !a2fo::refit_scene_test::fixture_attempted()&&
             a2fo::refit_scene_test::fixture_frames()==0,why);
    }
};
DWORD WINAPI wrong_owner(void* camera) {
    a2fo::refit_scene_test::before_world(camera,1);return 0;
}
}
int main() {
    static_assert(sizeof(void*)==4,"fixture requires i686");
    Env marker("A2FO_REFIT_HEADLESS_SCENE"),prefix("WINEPREFIX"),child("A2FO_REFIT_CHILD");
    Fixture f;f.ini_value("1");
    a2fo::native11_capture::exact_allowed=true;
    set("A2FO_REFIT_HEADLESS_SCENE",nullptr);set("WINEPREFIX","/mnt/prefix");
    set("A2FO_REFIT_CHILD","FO4 Refit Live Private");
    f.reset();f.arm();f.inert("missing explicit scene marker inert");
    need(!a2fo::native11_capture::exact_calls,"marker checked before image hash");
    set("A2FO_REFIT_HEADLESS_SCENE","1");set("WINEPREFIX","/home/unrelated");
    f.reset();f.arm();f.inert("unrelated prefix inert");
    set("WINEPREFIX","/mnt/prefix");set("A2FO_REFIT_CHILD","unrelated child");
    f.reset();f.arm();f.inert("unrelated child inert");
    set("A2FO_REFIT_CHILD","FO4 Refit Live Private");f.ini_value(nullptr);
    f.reset();f.arm();f.inert("missing INI opt-in inert");
    f.ini_value("maybe");f.reset();f.arm();f.inert("invalid INI value inert");
    f.ini_value("1");a2fo::native11_capture::exact_allowed=false;
    f.reset();f.arm();f.inert("exact image refusal inert");
    need(a2fo::native11_capture::exact_calls==1,"exact image gate reached only after opt-in");
    a2fo::native11_capture::exact_allowed=true;
    f.base[a2fo::refit_scene_test::profile::regions[0].rva]^=1;
    f.reset();f.arm();f.inert("inconsistent native bytes inert");
    need(last_log.find("profile refused")!=std::string::npos,"profile refusal logged");
    f.base[a2fo::refit_scene_test::profile::regions[0].rva]^=1;
    f.reset();f.arm();need(a2fo::refit_scene_test::fixture_enabled(),"exact private opt-in armed");
    int camera=1;
    for(unsigned i=0;i<16;++i) {
        a2fo::refit_scene_test::before_world(nullptr,1);
        a2fo::refit_scene_test::before_world(&camera,0);
    }
    need(a2fo::refit_scene_test::fixture_frames()==0,"null camera and non-main inert");
    HANDLE worker=CreateThread(nullptr,0,wrong_owner,&camera,0,nullptr);
    need(worker&&WaitForSingleObject(worker,10000)==WAIT_OBJECT_0,"wrong-owner worker joined");
    CloseHandle(worker);
    need(a2fo::refit_scene_test::fixture_frames()==0,"wrong owner inert");
    for(unsigned i=0;i<119;++i)a2fo::refit_scene_test::before_world(&camera,1);
    need(a2fo::refit_scene_test::fixture_frames()==119&&
         !a2fo::refit_scene_test::fixture_attempted(),"missing globals retried through 119");
    a2fo::refit_scene_test::before_world(&camera,1);
    need(a2fo::refit_scene_test::fixture_frames()==120&&
         a2fo::refit_scene_test::fixture_attempted()&&
         last_log.find("prerequisites absent")!=std::string::npos,
         "missing globals refused at bound without native calls");
    a2fo::refit_scene_test::before_world(&camera,1);
    need(a2fo::refit_scene_test::fixture_frames()==120,"refused attempt cannot repeat");
    std::puts("PASS Refit scene test guard: explicit private opt-in and bounded CPU-only refusal");
}
