// Synthetic hook fixture. The image is data; no keyboard or game code runs.
#include "../core/builtin_station_rotation.cpp"
#include "../core/module_loader.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
using namespace a2fo;
std::uint8_t* fixture = nullptr;
std::string root;
unsigned installs = 0, patches = 0, native_queues = 0, native_sets = 0;
bool fail_install = false, toggle_in_broadcast = false;
std::uint32_t queued = 0, set_command = 0;
void* selected = nullptr;
void check(bool yes, const char* why) { if (!yes) throw std::runtime_error(why); }
void A2FO_CALL logger(const char*, const char*) {}
void* A2FO_CALL armada() { return fixture; }
void policy(bool on) {
    ModulePolicy value; value.managed = true;
    ModulePolicyEntry entry; entry.key="a2fostationrotation"; entry.name="A2FOStationRotation";
    entry.filename="A2FOStationRotation.dll"; entry.state=on?ModulePolicyState::active:ModulePolicyState::inactive;
    entry.installed=true; value.entries.push_back(entry);
    std::string error;
    check(apply_builtin_module_policy({root},value,error),"station policy");
    check(builtin_module_enabled(BuiltinFeature::station_rotation)==on,"station gate");
}
void __attribute__((fastcall)) native_queue(void*,void*,std::uint32_t command,const void*,const float*) {
    ++native_queues; queued=command;
}
bool __attribute__((fastcall)) native_broadcast(void*,void*,std::uint32_t command,const void* position,void*) {
    if(toggle_in_broadcast) { toggle_in_broadcast=false; policy(false); policy(true); }
    queue_hook(nullptr,nullptr,command,selected,static_cast<const float*>(position));
    return true;
}
void __attribute__((fastcall)) native_set(void* self,void*,std::uint32_t command,const void*,const float*,bool) {
    ++native_sets; set_command=command;
    const std::uint32_t zero=0;
    std::memcpy(static_cast<std::uint8_t*>(self)+0x8c,&zero,4);
}
bool A2FO_CALL hook(void* target,void*,std::size_t size,const std::uint8_t* signature,A2FO_InlineHook* result) {
    check(std::memcmp(target,signature,size)==0,"station hook signature");
    ++installs;
    if(fail_install && installs==2)return false;
    void* gateway=reinterpret_cast<void*>(&native_queue);
    if(target==fixture+kBroadcastRva)gateway=reinterpret_cast<void*>(&native_broadcast);
    if(target==fixture+kSetCommandRva)gateway=reinterpret_cast<void*>(&native_set);
    *result={target,gateway,static_cast<std::uint32_t>(size)};
    return true;
}
bool A2FO_CALL patch(void* target,void*,const std::uint8_t* bytes,std::size_t size) {
    check(std::memcmp(target,bytes,size)==0,"station call signature"); ++patches;return true;
}
template<std::size_t N>void seed(std::uintptr_t rva,const std::uint8_t (&bytes)[N]) {
    std::memcpy(fixture+rva,bytes,N);
}
void seed_image(bool bad) {
    fixture=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x410000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    check(fixture,"station image");
    seed(kKeyboardRva,kKeyboardBytes); seed(kBroadcastRva,kBroadcastBytes);
    seed(kQueueRva,kQueueBytes); seed(kSetCommandRva,kSetCommandBytes);
    seed(kPreviewRva,kPreviewBytes); seed(kPositionCallRva,kPositionCallBytes);
    seed(kSetPositionRva,kSetPositionBytes); seed(kTextInputActiveRva,kTextInputActiveBytes);
    seed(kBuildRva,kBuildBytes);seed(kActionRva,kActionBytes);
    seed(kFootprintRva,kFootprintBytes);seed(kRemoveFootprintRva,kRemoveFootprintBytes);
    seed(kPlacementClearRva,kPlacementClearBytes);seed(kCanPlaceRva,kCanPlaceBytes);
    seed(kAddFootprintRva,kAddFootprintBytes);seed(kRenderFootprintsRva,kRenderFootprintsBytes);
    for(const auto& call:kFootprintCalls){const auto bytes=call_bytes(call);std::memcpy(fixture+call.rva,bytes.data(),bytes.size());}
    if(bad)fixture[kPreviewRva]^=1;
}
void run(const std::string& mode) {
    check(mode=="success"||mode=="signature"||mode=="partial","mode");
    char temp[MAX_PATH]{}, path[MAX_PATH]{};
    check(GetTempPathA(sizeof(temp),temp)&&GetTempFileNameA(temp,"bsr",0,path),"temp root");
    root=path;check(DeleteFileA(path)&&CreateDirectoryA(path,nullptr),"station private root");
    seed_image(mode=="signature"); fail_install=mode=="partial";
    A2FO_ModuleApi api{};api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
    api.log=logger;api.armada_module=armada;api.install_inline_hook=hook;api.patch_call=patch;
    set_builtin_module_roots({root});
    const bool ready=mode=="success";
    check(prepare_builtin_module(BuiltinFeature::station_rotation,api,false)==ready,"station preparation");
    finish_builtin_module_preparation(true,true,true,true,true,true,true,true);
    check(builtin_module_available(BuiltinFeature::station_rotation)==ready,"station availability");
    if(mode=="signature")check(installs==0&&patches==0,"signature preflight is atomic");
    if(mode=="partial")check(installs==2&&patches==1,"partial hook remained inert");
    if(ready) {
        check(prepare_builtin_module(BuiltinFeature::station_rotation,api,false)&&installs>2,"idempotent prepare");
        selected=fixture+0x380000;
        auto* active=fixture+kActiveModeRva;
        void* mode_object=fixture+0x381000;
        std::uint32_t build_mode=1;
        std::memcpy(active,&mode_object,4);std::memcpy(static_cast<std::uint8_t*>(mode_object)+4,&build_mode,4);
        std::memcpy(static_cast<std::uint8_t*>(mode_object)+0x0c,&selected,4);
        policy(true);
        check(runtime_enabled(),"observe enabled generation before seeding placement state");
        g_control.target=reinterpret_cast<std::uintptr_t>(selected);g_control.turns=2;
        policy(false);
        broadcast_hook(nullptr,nullptr,kBuildCommand,nullptr,nullptr);
        check(queued==kBuildCommand&&g_control.turns==0,
              "disabled broadcast preserves native order and clears placement control");
        policy(true);
        check(runtime_enabled(),"station active generation");
        g_control.target=reinterpret_cast<std::uintptr_t>(selected);g_control.turns=2;
        broadcast_hook(nullptr,nullptr,kBuildCommand,nullptr,nullptr);
        check(queued==encode_build(2),"enabled broadcast encodes placement turns");
        toggle_in_broadcast=true;
        broadcast_hook(nullptr,nullptr,kBuildCommand,nullptr,nullptr);
        check(queued==kBuildCommand,"stale in-flight broadcast never encodes");
        std::uint8_t self[0x100]{};
        policy(false);set_command_hook(self,nullptr,encode_build(3),selected,nullptr,true);
        check(set_command==kBuildCommand&&read<std::uint32_t>(self,0x8c)==rotation_parameter(3),
              "tagged network command decoded while policy off");
        check(native_queues==3&&native_sets==1,"native forwarding counts");
    }
    VirtualFree(fixture,0,MEM_RELEASE);fixture=nullptr;RemoveDirectoryA(root.c_str());
}
}
int main(int argc,char** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
try{check(argc==2,"usage: success|signature|partial");run(argv[1]);
    std::puts("builtin station rotation fixture: PASS");return 0;}
    catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
