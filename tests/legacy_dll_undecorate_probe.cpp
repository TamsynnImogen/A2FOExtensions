// Console comparison of OS library contracts; does not load an engine or use UI.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <initializer_list>
using Decode=DWORD(WINAPI*)(const char*,char*,DWORD,DWORD);
Decode symbol(HMODULE module) {
    const auto address=GetProcAddress(module,"UnDecorateSymbolName");Decode function=nullptr;
    static_assert(sizeof(function)==sizeof(address));std::memcpy(&function,&address,sizeof(function));return function;
}
int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    wchar_t path[32768]{};
    const auto n=GetSystemDirectoryW(path,32768);
    if(!n || n+20>=32768)return 2;
    std::wcscpy(path+n,L"\\imagehlp.dll");const auto a=LoadLibraryW(path);
    std::wcscpy(path+n,L"\\dbghelp.dll");const auto b=LoadLibraryW(path);
    const auto old=a?symbol(a):nullptr;
    const auto replacement=b?symbol(b):nullptr;
    if(!old || !replacement)return 3;
    const char* names[]={"?SetHookID@@YAXPAUHHOOK__@@@Z","?LowLevelKeyboardProc@@YGJHIJ@Z",
      "?c_str@String@Network@@QBEPBDXZ","?VoiceTargetAll@Lobby@Network@@2HB",
      "??0VoiceSettings@Lobby@Network@@QAE@ABVVoiceSettingsAvailable@12@@Z",
      "?GetEvent@Lobby@Network@@QAEPAVEvent@12@XZ","?Delete@Event@Lobby@Network@@QAEXXZ",
      "?Create@Lobby@Network@@SAPAV12@ABVParameters@12@@Z","?not_a_valid_symbol", "plain", ""};
    unsigned checks=0;
    for(const auto name:names)for(DWORD flag:{0u,0x1000u,0x2000u})for(DWORD size:{0u,1u,8u,2047u}) {
        char x[2050],y[2050];std::memset(x,0x5a,sizeof(x));std::memset(y,0x5a,sizeof(y));
        const auto rx=old(name,x,size,flag),ry=replacement(name,y,size,flag);
        if(rx!=ry || (rx && std::strcmp(x,y)) || x[2049]!=0x5a || y[2049]!=0x5a) {
            std::printf("FAIL undecorate size=%lu flags=%lu name=%s\n",size,flag,name);return 4;
        }
        ++checks;
    }
    std::printf("PASS OS ImageHlp/DbgHelp undecoration: %u cases; shared target=%u\n",checks,old==replacement);
    return 0;
}
