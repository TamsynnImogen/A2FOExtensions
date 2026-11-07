// Frozen/current full observer.cpp; only synthetic PE32 and fake COM.
#include A2FO_OBSERVER_SOURCE
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace a2fo {
std::string active_mod_from_command_line(const std::string&) {return {};}
bool safe_mod_directory_name(const std::string&) {return false;}
}
namespace a2fo::renderer {
HRESULT run_d3d11_window_probe(HWND,UINT,UINT,bool,RendererLog){std::abort();}
namespace game {
IDirect3D8 *create_factory(IDirect3D8 *,const Configuration&) noexcept {return nullptr;}
bool install_file_bridge(HMODULE,Log) noexcept {return false;}
bool install_d3dx_file_bridge(HMODULE,Log) noexcept {return false;}
}
}
namespace a2fo::renderer::shaders {
TokenParseResult parse_d3d8_vertex_declaration(const std::uint32_t*,std::size_t) noexcept {
    return {TokenParseStatus::invalid,0};
}
TokenParseResult parse_d3d8_shader_bytecode(const std::uint32_t*,std::size_t) noexcept {
    return {TokenParseStatus::invalid,0};
}
}
using namespace a2fo::renderer;
static unsigned mock_factory_calls=0;
static IDirect3D8* WINAPI mock_factory(UINT){++mock_factory_calls;return nullptr;}
int main(){
    runtime=new Runtime;
    auto *image=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!image)return 12;
    auto *dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=128;
    auto *nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(image+128);nt->Signature=IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage=4096;nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]={512,40};
    auto *imports=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image+512);
    imports[0].Name=600;imports[0].OriginalFirstThunk=640;imports[0].FirstThunk=680;
    std::memcpy(image+600,"d3d8.dll",9);
    auto *name=reinterpret_cast<DWORD*>(image+640);*name=720;
    auto *slot=reinterpret_cast<void**>(image+680);*slot=address(&mock_factory);
    std::memcpy(image+722,"Direct3DCreate8",16);
    auto show=[&](const char* label){std::printf("%s|%u|slot=%u|factory=%u\n",label,
        unsigned(patch_factory_import(reinterpret_cast<HMODULE>(image))),
        unsigned(*slot==address(&factory_hook)),unsigned(runtime->factory!=nullptr));};
    dos->e_magic=0;show("bad-dos");dos->e_magic=IMAGE_DOS_SIGNATURE;
    nt->Signature=0;show("bad-nt");nt->Signature=IMAGE_NT_SIGNATURE;
    *name=IMAGE_ORDINAL_FLAG32;show("ordinal");*name=720;
    std::memcpy(image+722,"OtherName______",16);show("other");
    std::memcpy(image+722,"Direct3DCreate8",16);
    *slot=nullptr;show("null-slot");*slot=address(&mock_factory);
    show("first");show("repeat");
    *slot=address(&mock_factory);show("wrong-repeat");
    dos->e_magic=0;show("after-invalid");
    dos->e_magic=IMAGE_DOS_SIGNATURE;
    delete runtime;runtime=nullptr;
    char temporary[MAX_PATH]{};
    if(!GetTempPathA(MAX_PATH,temporary))return 13;
    char root[MAX_PATH]{};
    std::snprintf(root,sizeof(root),"%sa2fo-observer-fixture-%lu",temporary,GetCurrentProcessId());
    if(!CreateDirectoryA(root,nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return 14;
    const std::string ini=std::string(root)+"\\A2FORenderer.ini";
    HANDLE file=CreateFileA(ini.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return 15;
    const char settings[]="[D3D8Compat]\r\nBackend=d3d8\r\nTrace=1\r\n";
    DWORD written=0;const bool settings_ok=WriteFile(file,settings,sizeof(settings)-1,&written,nullptr)!=FALSE;
    CloseHandle(file);if(!settings_ok||written!=sizeof(settings)-1)return 16;
    const bool installed=install_d3d8_observer(reinterpret_cast<HMODULE>(image),nullptr,root,nullptr);
    std::printf("install-trace|%u|%u|%u\n",unsigned(installed),
        unsigned(*slot==address(&factory_hook)),unsigned(runtime!=nullptr));
    delete runtime;runtime=nullptr;
    DeleteFileA(ini.c_str());RemoveDirectoryA(root);
    VirtualFree(image,0,MEM_RELEASE);
    runtime=new Runtime;
    void *native_table[16]{};
    struct FakeObject {void **vtable;};
    FakeObject fake{native_table};
    observe(&fake,Kind::IDirect3D8);
    std::printf("observe-first|%u|%u|%u\n",unsigned(fake.vtable!=native_table),
        unsigned(runtime->objects.size()),runtime->next_id);
    observe(&fake,Kind::IDirect3D8);
    std::printf("observe-repeat|%u|%u|%u\n",unsigned(fake.vtable!=native_table),
        unsigned(runtime->objects.size()),runtime->next_id);
    fake.vtable=native_table;
    observe(&fake,Kind::IDirect3D8);
    std::printf("observe-recycled|%u|%u|%u\n",unsigned(fake.vtable!=native_table),
        unsigned(runtime->objects.size()),runtime->next_id);
    runtime->factory=&mock_factory;
    runtime->game_test=false;runtime->game_ready=false;
    std::printf("factory-normal|%u|%u\n",unsigned(factory_hook(220)!=nullptr),mock_factory_calls);
    runtime->game_test=true;runtime->game_ready=false;
    std::printf("factory-refused|%u|%u\n",unsigned(factory_hook(220)!=nullptr),mock_factory_calls);
    runtime->game_ready=true;
    std::printf("factory-test|%u|%u\n",unsigned(factory_hook(220)!=nullptr),mock_factory_calls);
    runtime->game_test=false;
    runtime->trace=true;
    QueryPerformanceFrequency(&runtime->frequency);
    {Call call(&fake,Kind::IDirect3D8,0,"IDirect3D8","Mock");
        call.argument("index",3u);call.complete_result(E_FAIL,true);}
    {Call call(&fake,Kind::IDirect3D8,0,"IDirect3D8","Mock");
        call.argument("index",3u);call.complete_result(S_OK,true);}
    const auto observed=runtime->methods.find("IDirect3D8.Mock");
    std::printf("diagnostic|%llu|%llu|%u|%u\n",observed->second.calls,
        observed->second.failures,unsigned(runtime->signatures.size()),runtime->events);
    delete runtime;runtime=nullptr;
}
