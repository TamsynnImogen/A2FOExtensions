// Data-only observer fixture. Native routine bodies and game code never run.
#include "../core/builtin_recovery_tests.cpp"
#include "../core/module_loader.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
using namespace a2fo;
std::string root;
unsigned patches=0,logs=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void A2FO_CALL logger(const char*,const char*){++logs;}
const char* A2FO_CALL root_directory(){return root.c_str();}
void* A2FO_CALL armada(){return GetModuleHandleA(nullptr);}
bool A2FO_CALL patch(void*,void*,const std::uint8_t*,std::size_t){++patches;return true;}
void policy(bool on){
    ModulePolicy selection;selection.managed=true;
    ModulePolicyEntry entry;entry.key="a2forecoverytests";entry.name="A2FORecoveryTests";
    entry.filename="A2FORecoveryTests.dll";entry.installed=true;
    entry.state=on?ModulePolicyState::active:ModulePolicyState::inactive;
    selection.entries.push_back(entry);std::string error;
    check(apply_builtin_module_policy({root},selection,error),"recovery policy");
    check(builtin_module_enabled(BuiltinFeature::recovery_tests)==on,"recovery gate");
}
void write_ini(const char* body){
    const auto path=root+"\\A2FORecoveryTests.ini";
    HANDLE file=CreateFileA(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(file!=INVALID_HANDLE_VALUE,"recovery INI");DWORD wrote=0;
    const auto length=static_cast<DWORD>(std::strlen(body));
    check(WriteFile(file,body,length,&wrote,nullptr)&&wrote==length,"write recovery INI");
    CloseHandle(file);
}
void setup_root(){
    char temp[MAX_PATH]{},path[MAX_PATH]{};
    check(GetTempPathA(sizeof(temp),temp)&&GetTempFileNameA(temp,"brc",0,path),"recovery temp");
    root=path;check(DeleteFileA(path)&&CreateDirectoryA(path,nullptr),"recovery root");
}
void remove_root(){
    DeleteFileA((root+"\\A2FORecoveryTests.ini").c_str());
    RemoveDirectoryA((root+"\\A2FORecoveryTests").c_str());
    RemoveDirectoryA(root.c_str());
}
void success_observer(){
    // Actual preparation is passive: the SHA/base guard is never bypassed.
    A2FO_ModuleApi sdk{};sdk.struct_size=sizeof(sdk);sdk.api_version=A2FO_MODULE_API_VERSION;
    sdk.log=logger;sdk.root_directory=root_directory;sdk.armada_module=armada;sdk.patch_jump=patch;
    set_builtin_module_roots({root});
    check(prepare_builtin_module(BuiltinFeature::recovery_tests,sdk,false),"passive recovery prepare");
    finish_builtin_module_preparation(true,true,true,true,true,true,true,true);
    check(builtin_module_available(BuiltinFeature::recovery_tests),"passive availability");
    check(patches==0,"passive preparation never patches");
    const auto directory=root+"\\A2FORecoveryTests";
    check(GetFileAttributesA(directory.c_str())==INVALID_FILE_ATTRIBUTES,"no inactive report directory");
    // Isolate observer callbacks with synthetic snapshots after the genuine
    // Init path has finished. No test calls install or any pinned native body.
    config.enabled=true;config.sample_every=1;config.max_samples=8;
    config.max_reports=2;config.capture_first_match=true;
    report_directory=directory;armed[0]=1;armed[1]=0;closing=0;busy=0;reports=0;
    counts[0]=Counts{};
    std::array<std::uint8_t,0x100> engine{}, selected{};
    const std::uint32_t index=0,value=0x12345678u;
    const auto selected_address=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(selected.data()));
    std::memcpy(engine.data()+0xc0,&index,4);
    std::memcpy(engine.data()+0xcc,&selected_address,4);
    std::memcpy(selected.data()+0x28,&value,4);
    const auto engine_address=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(engine.data()));
    std::array<std::uint8_t,512> fx{};fx[0]=0x7f;fx[1]=0x03;
    Frame frame{};
    policy(false);
    a2fo_recovery_before(0,engine_address,&frame,fx.data());
    check(!frame.admitted&&counts[0].visited==0&&busy==0,"off never admits snapshot");
    policy(true);
    a2fo_recovery_before(0,engine_address,&frame,fx.data());
    check(frame.admitted==1&&busy==1&&counts[0].attempted==1,"active callback admitted");
    policy(false);policy(true);
    a2fo_recovery_after(&frame,value);
    check(busy==0&&counts[0].matched==0&&counts[0].mismatched==0&&reports==0,
          "changed activation drops in-flight comparison and releases busy");
    check(GetFileAttributesA(directory.c_str())==INVALID_FILE_ATTRIBUTES,"stale frame made no directory");
    a2fo_recovery_before(0,engine_address,&frame,fx.data());
    a2fo_recovery_after(&frame,value);
    check(busy==0&&counts[0].matched==1&&reports==1,"current generation compared and saved");
    check((GetFileAttributesA(directory.c_str())&FILE_ATTRIBUTE_DIRECTORY)!=0,
          "active save creates report directory");
    WIN32_FIND_DATAA found{};HANDLE search=FindFirstFileA((directory+"\\*.json").c_str(),&found);
    check(search!=INVALID_HANDLE_VALUE,"active report exists");
    const auto report=directory+"\\"+found.cFileName;FindClose(search);DeleteFileA(report.c_str());
    policy(false);
    a2fo_recovery_before(0,engine_address,&frame,fx.data());
    check(!frame.admitted&&counts[0].visited==2,"off again does not count visits");
}
void run(const std::string& mode){
    check(mode=="passive"||mode=="unsupported"||mode=="success","mode");
    setup_root();
    if(mode=="unsupported")write_ini("[RecoveryTests]\r\nEnabled=1\r\nSampleEvery=1\r\n");
    if(mode=="success")success_observer();
    else {
        A2FO_ModuleApi sdk{};sdk.struct_size=sizeof(sdk);sdk.api_version=A2FO_MODULE_API_VERSION;
        sdk.log=logger;sdk.root_directory=root_directory;sdk.armada_module=armada;sdk.patch_jump=patch;
        set_builtin_module_roots({root});
        const bool ready=mode=="passive";
        check(prepare_builtin_module(BuiltinFeature::recovery_tests,sdk,false)==ready,"recovery preparation");
        finish_builtin_module_preparation(true,true,true,true,true,true,true,true);
        check(builtin_module_available(BuiltinFeature::recovery_tests)==ready,"recovery availability");
        check(patches==0,"identity rejection never patches");
        check(GetFileAttributesA((root+"\\A2FORecoveryTests").c_str())==INVALID_FILE_ATTRIBUTES,
              "inactive/unsupported creates no report directory");
    }
    remove_root();
}
}
int main(int argc,char** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
try{check(argc==2,"usage: passive|unsupported|success");run(argv[1]);
    std::puts("builtin recovery tests fixture: PASS");return 0;}
    catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
