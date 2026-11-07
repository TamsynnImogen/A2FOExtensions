// Synthetic PE image and observer callbacks. No graphics provider or engine runs.
#include "../core/builtin_radar_diagnostics.cpp"
#include "../core/module_loader.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
using namespace a2fo;
using namespace radar_diagnostics;
std::uint8_t* fixture=nullptr;
std::string root;
unsigned patches=0, originals=0, logs=0;
bool fail_second=false, toggle_in_original=false;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void A2FO_CALL logger(const char*,const char*){++logs;}
void* A2FO_CALL armada(){return fixture;}
const char* A2FO_CALL root_directory(){return root.c_str();}
void policy(bool on){
    ModulePolicy value;value.managed=true;
    ModulePolicyEntry entry;entry.key="a2foradardiagnostics";entry.name="A2FORadarDiagnostics";
    entry.filename="A2FORadarDiagnostics.dll";entry.installed=true;
    entry.state=on?ModulePolicyState::active:ModulePolicyState::inactive;
    value.entries.push_back(entry);std::string error;
    check(apply_builtin_module_policy({root},value,error),"radar policy");
    check(builtin_module_enabled(BuiltinFeature::radar_diagnostics)==on,"radar gate");
}
void __attribute__((fastcall)) original(void*,void*,void*,const float*,int,int){
    ++originals;
    if(toggle_in_original){toggle_in_original=false;policy(false);policy(true);}
}
bool A2FO_CALL patch(void* target,void*,const std::uint8_t* bytes,std::size_t size){
    check(target==fixture+(patches?craft_call:clear_call),"radar patch site");
    check(std::memcmp(target,bytes,size)==0,"radar patch signature");
    ++patches;return !(fail_second&&patches==2);
}
void seed_image(bool signature){
    fixture=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x410000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    check(fixture,"radar image");
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(fixture);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(fixture+0x80);nt->Signature=IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.TimeDateStamp=supported_armada::kCanonicalTimestamp;
    nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage=supported_armada::kCanonicalImageSize;
    std::memcpy(fixture+craft_call,expected_call,sizeof(expected_call));
    const std::uint8_t prologue[]{0x55,0x8b,0xec,0x83,0xec,0x44};
    std::memcpy(fixture+map_object,prologue,sizeof(prologue));
    std::memcpy(fixture+clear_call,expected_clear,sizeof(expected_clear));
    if(signature)fixture[craft_call]^=1;
}
void run(const std::string& mode){
    check(mode=="passive"||mode=="success"||mode=="signature"||mode=="partial","mode");
    char temp[MAX_PATH]{},path[MAX_PATH]{};
    check(GetTempPathA(sizeof(temp),temp)&&GetTempFileNameA(temp,"brd",0,path),"radar temp");
    root=path;check(DeleteFileA(path)&&CreateDirectoryA(path,nullptr),"radar root");
    if(mode!="passive"){
        const auto ini=root+"\\A2FORadarDiagnostics.ini";
        HANDLE file=CreateFileA(ini.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        check(file!=INVALID_HANDLE_VALUE,"radar INI");
        DWORD wrote=0;const char content[]="[Diagnostics]\r\nEnabled=1\r\n";
        check(WriteFile(file,content,sizeof(content)-1,&wrote,nullptr)&&wrote==sizeof(content)-1,"radar optin");
        CloseHandle(file);
    }
    seed_image(mode=="signature");fail_second=mode=="partial";
    A2FO_ModuleApi sdk{};sdk.struct_size=sizeof(sdk);sdk.api_version=A2FO_MODULE_API_VERSION;
    sdk.log=logger;sdk.armada_module=armada;sdk.root_directory=root_directory;sdk.patch_call=patch;
    set_builtin_module_roots({root});
    const bool ready=mode=="passive"||mode=="success";
    check(prepare_builtin_module(BuiltinFeature::radar_diagnostics,sdk,false)==ready,"radar prepare");
    finish_builtin_module_preparation(true,true,true,true,true,true,true,true);
    check(builtin_module_available(BuiltinFeature::radar_diagnostics)==ready,"radar availability");
    if(mode=="passive"||mode=="signature")check(patches==0,"radar no patches");
    if(mode=="partial")check(patches==2,"radar partial patch attempted");
    if(mode=="success"){
        check(patches==2,"radar two patches");
        original_object=&original;
        policy(false);
        object_hook(nullptr,nullptr,nullptr,nullptr,0,0);
        check(originals==1&&objects[0].load()==0&&seen_count==0,"off forwards without observations");
        policy(true);
        object_hook(nullptr,nullptr,nullptr,nullptr,0,0);
        check(originals==2&&objects[0].load()==1&&seen_count==1,"on observes after original");
        toggle_in_original=true;
        object_hook(nullptr,nullptr,nullptr,nullptr,0,1);
        check(originals==3&&objects[1].load()==0&&seen_count==1,
              "activation changed inside original suppresses postcallback capture");
        policy(false);
        object_hook(nullptr,nullptr,nullptr,nullptr,0,1);
        check(originals==4&&objects[1].load()==0,"disabled still forwards original");
    }
    const auto ini=root+"\\A2FORadarDiagnostics.ini";DeleteFileA(ini.c_str());
    VirtualFree(fixture,0,MEM_RELEASE);fixture=nullptr;RemoveDirectoryA(root.c_str());
}
}
int main(int argc,char** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
try{check(argc==2,"usage: passive|success|signature|partial");run(argv[1]);
    std::puts("builtin radar diagnostics fixture: PASS");return 0;}
    catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
