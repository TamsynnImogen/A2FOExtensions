// Actual frozen/current LogMailer module linked into a headless Wine mock.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include MODULE_SOURCE
namespace {
std::vector<std::string> trace;
std::string root;
const char* A2FO_CALL root_directory(){return root.c_str();}
void A2FO_CALL logger(const char*,const char* message){trace.emplace_back(std::string("log:")+message);}
void setting(const wchar_t* key,const wchar_t* value){
    std::wstring path(root.begin(),root.end());path+=L"\\A2FOLogMailer.ini";
    WritePrivateProfileStringW(L"Reporting",key,value,path.c_str());
}
void invoke(const char* label,const A2FO_ModuleApi* api){
    trace.emplace_back(std::string(label)+":"+(A2FO_ModuleInit(api)?"1":"0")+":"+
        std::to_string(started)+":"+std::to_string(cancel_event!=nullptr));
}
void run(){
    char temp[MAX_PATH]{};GetTempPathA(MAX_PATH,temp);
    root=std::string(temp)+"a2fo-stage7-mailer-"+std::to_string(GetCurrentProcessId())+"-"+
        std::to_string(GetTickCount64());
    CreateDirectoryA(root.c_str(),nullptr);
    A2FO_ModuleApi api{};api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
    api.root_directory=&root_directory;api.log=&logger;
    invoke("null",nullptr);
    api.api_version=0;invoke("wrong-version",&api);api.api_version=A2FO_MODULE_API_VERSION;
    invoke("disabled",&api);
    setting(L"Enabled",L"1");setting(L"RelayUrl",L"http://bad.invalid/v1/crash-reports");
    setting(L"SubmissionToken",L"fixture-token");
    invoke("inactive",&api);
    setting(L"RelayUrl",L"https://example.test/v1/crash-reports");
    invoke("missing-helper",&api);
    std::string modules=root+"\\modules";CreateDirectoryA(modules.c_str(),nullptr);
    char executable[MAX_PATH]{};GetModuleFileNameA(nullptr,executable,MAX_PATH);
    std::string helper=modules+"\\A2FOLogMailerHelper.exe";
    CopyFileA(executable,helper.c_str(),FALSE);
    setting(L"Mode",L"crash_only");invoke("crash-only",&api);
    invoke("already-started",&api);
    A2FO_ModuleShutdown();
    trace.emplace_back("shutdown:"+std::to_string(started)+":"+
        std::to_string(cancel_event!=nullptr));
    setting(L"Mode",L"all_sessions");invoke("all-sessions",&api);
    A2FO_ModuleShutdown();
    trace.emplace_back("shutdown-again:"+std::to_string(started)+":"+
        std::to_string(cancel_event!=nullptr));
    DeleteFileA(helper.c_str());RemoveDirectoryA(modules.c_str());
    DeleteFileA((root+"\\A2FOLogMailer.ini").c_str());RemoveDirectoryA(root.c_str());
}
}
int main(int argc,char**){if(argc>1)return 0;run();for(const auto& t:trace)std::printf("%s\n",t.c_str());}
