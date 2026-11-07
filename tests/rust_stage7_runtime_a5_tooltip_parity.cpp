bool fixture_builtin_enabled=true;
#include MODULE_SOURCE
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef TEST_BUILTIN
namespace a2fo {bool builtin_module_enabled(BuiltinFeature) noexcept{return fixture_builtin_enabled;}}
#endif
namespace {
std::uint8_t* image=nullptr;
int hooks=0,patches=0,fail_hook=0;
std::string rendered;
float native_seconds=20.0f,modifier=1.0f;
void* setup_result=nullptr;
using Lookup=std::uintptr_t (__attribute__((fastcall)) *)(void*,void*,const char*);
Lookup lookup=nullptr;
void check(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
template<class T>void put(void* p,std::size_t offset,T value){
    std::memcpy(static_cast<std::uint8_t*>(p)+offset,&value,sizeof(value));
}
void bytes(std::size_t rva,std::initializer_list<std::uint8_t> value){
    std::memcpy(image+rva,value.begin(),value.size());
}
void jump(void* location,void* target){
    std::uint8_t code[]{0xb8,0,0,0,0,0xff,0xe0};
    std::memcpy(location,code,sizeof(code));put(location,1,reinterpret_cast<std::uintptr_t>(target));
}
void A2FO_CALL fake_log(const char*,const char* message){std::printf("log:%s\n",message);}
void* A2FO_CALL fake_armada(){return image;}
std::uintptr_t __attribute__((fastcall)) fake_lookup(void*,void*,const char* key){
    return reinterpret_cast<std::uintptr_t>(std::strcmp(key,"GUI_CP_END_EXTRA")==0?")":"/");
}
std::uintptr_t __attribute__((fastcall)) fake_formatter(void*,void*,void*){
    rendered=reinterpret_cast<const char*>(lookup(reinterpret_cast<void*>(1),nullptr,"GUI_CP_END_EXTRA"));
    return 1;
}
float __attribute__((fastcall)) fake_duration(void*,void*,std::uintptr_t){return native_seconds;}
void* __attribute__((fastcall)) fake_setup(void*,void*){return setup_result;}
float __attribute__((fastcall)) fake_modifier(void*,void*){return modifier;}
int __cdecl fake_team(){return 0;}
void* __cdecl fake_team_for_team(int){return reinterpret_cast<void*>(0x1234);}
bool A2FO_CALL fake_hook(void*,void*,std::size_t,const std::uint8_t*,A2FO_InlineHook* hook){
    ++hooks;if(hooks==fail_hook)return false;
    hook->gateway=reinterpret_cast<void*>(&fake_formatter);return true;
}
bool A2FO_CALL fake_patch(void*,void* replacement,const std::uint8_t*,std::size_t){
    ++patches;std::memcpy(&lookup,&replacement,sizeof(lookup));return true;
}
void seed(){
    image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x410000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    check(image!=nullptr,"image allocation");
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(image+0x80);
    nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.TimeDateStamp=0x3c4c76bd;nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage=0x403999;
    bytes(kModeInfoButtonTextRva,{0x55,0x8b,0xec,0x6a,0xff,0x68,0x5b,0xed,0x69,0});
    bytes(kModeInfoButtonVerboseRva,{0x55,0x8b,0xec,0x6a,0xff,0x68,0x80,0xed,0x69,0});
    bytes(kGameObjectClassGetBuildTimeRva,{0x55,0x8b,0xec,0x51,0x53,0x56,0x57,0x8b,0xf9});
    bytes(kLocalTeamRva,{0x8b,0x0d,0xd4,0xb8,0x76,0,0xe9,0x45,0x78,0x08});
    bytes(kTeamForTeamRva,{0x55,0x8b,0xec,0x8b,0x45,0x08,0x8b,0x04,0x85,0xb0,0x8d,0x73,0,0x5d,0xc3});
    bytes(kGetGameSetupRva,{0x8b,0x41,0x30,0xc3});
    bytes(kGameSetupGetBuildTimeModifierRva,{0x8b,0x41,0x10,0xd9,0x80,0x2c,0x03,0,0,0xc3});
    bytes(kEndExtraCallSites[0].rva,{0xe8,0x71,0xab,0xf9,0xff});
    bytes(kEndExtraCallSites[1].rva,{0xe8,0x61,0xa5,0xf9,0xff});
    jump(image+kLocalizationLookupRva,reinterpret_cast<void*>(&fake_lookup));
}
void run(){
    seed();
    A2FO_ModuleApi api{};api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
    api.log=fake_log;api.armada_module=fake_armada;api.install_inline_hook=fake_hook;api.patch_call=fake_patch;
    check(!A2FO_ModuleInit(nullptr),"null api");
    auto bad=api;bad.api_version=0;check(!A2FO_ModuleInit(&bad),"bad version");
    image[kModeInfoButtonTextRva]^=1;
    check(A2FO_ModuleInit(&api)&&!g_runtime_ready&&hooks==0&&patches==0,"signature mismatch");
    image[kModeInfoButtonTextRva]^=1;
    fail_hook=2;check(A2FO_ModuleInit(&api)&&!g_runtime_ready&&hooks==2&&patches==2,"partial install");
    lookup=&fake_lookup;
    check(a2fo_build_tooltips_button_text_hook_cpp(nullptr,nullptr)==1&&rendered==")","partial pass through");
    fail_hook=0;hooks=0;patches=0;
    check(A2FO_ModuleInit(&api)&&g_runtime_ready&&hooks==2&&patches==2,"full install");
    jump(image+kGameObjectClassGetBuildTimeRva,reinterpret_cast<void*>(&fake_duration));
    jump(image+kLocalTeamRva,reinterpret_cast<void*>(&fake_team));
    jump(image+kTeamForTeamRva,reinterpret_cast<void*>(&fake_team_for_team));
    std::array<std::uint8_t,0x100> shell{},setup{};setup_result=setup.data();
    put(image+kCurrentSetupShellRva,0,shell.data());
    jump(image+kGetGameSetupRva,reinterpret_cast<void*>(&fake_setup));
    jump(image+kGameSetupGetBuildTimeModifierRva,reinterpret_cast<void*>(&fake_modifier));
    std::array<std::uint8_t,0x100> klass{},mode{};
    put(klass.data(),kGameObjectClassRawBuildTimeOffset,20.0f);
    put(mode.data(),kModeInfoTypeOffset,kBuildItemType);
    put(mode.data(),kModeInfoTargetClassOffset,klass.data());
    HMODULE resources=LoadLibraryA("build/rust-migration-stage7/runtime-a-work/A2FOResources.dll");
    check(resources!=nullptr,"resources provider");
    for(float next:{1.0f,1.5f,1.0000001f}){
        modifier=next;native_seconds=20.0f;
        check(a2fo_build_tooltips_button_text_hook_cpp(mode.data(),nullptr)==1,"text hook");
        std::printf("text:%.8g:%s\n",next,rendered.c_str());
        check(a2fo_build_tooltips_button_verbose_hook_cpp(mode.data(),nullptr)==1,"verbose hook");
        std::printf("verbose:%.8g:%s\n",next,rendered.c_str());
    }
    put(mode.data(),kModeInfoTypeOffset,std::int32_t{2});
    check(a2fo_build_tooltips_button_text_hook_cpp(mode.data(),nullptr)==1&&rendered==")","non-build mode");
#ifdef TEST_BUILTIN
    put(mode.data(),kModeInfoTypeOffset,kBuildItemType);fixture_builtin_enabled=false;
    check(a2fo_build_tooltips_button_text_hook_cpp(mode.data(),nullptr)==1&&rendered==")","live builtin gate");
    fixture_builtin_enabled=true;
#endif
    A2FO_ModuleShutdown();check(!g_runtime_ready,"shutdown");
    check(a2fo_build_tooltips_button_text_hook_cpp(mode.data(),nullptr)==1&&rendered==")","shutdown pass through");
    std::printf("counts:%d:%d\n",hooks,patches);
    FreeLibrary(resources);VirtualFree(image,0,MEM_RELEASE);
}
}
int main(){try{run();return 0;}catch(const std::exception& error){std::fprintf(stderr,"FAIL:%s\n",error.what());return 1;}}
