// Keep synthetic hook callbacks distinct from the real Rust archive symbols.
#define a2fo_rs_windows_hook_install fixture_windows_hook_install
#define a2fo_rs_windows_patch_jump fixture_windows_patch_jump
#define a2fo_rs_windows_patch_call fixture_windows_patch_call
#define a2fo_rs_windows_patch_bytes fixture_windows_patch_bytes
// Headless full-source frozen/current module-menu fixture. No desktop or GUI calls.
#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

HMODULE WINAPI mock_GetModuleHandleA(LPCSTR);
int WINAPI mock_MessageBoxA(HWND,LPCSTR,LPCSTR,UINT);
HMODULE WINAPI mock_LoadLibraryA(LPCSTR);
FARPROC WINAPI mock_GetProcAddress(HMODULE,LPCSTR);
BOOL WINAPI mock_FreeLibrary(HMODULE);
HANDLE WINAPI mock_FindFirstFileA(LPCSTR,LPWIN32_FIND_DATAA);
BOOL WINAPI mock_FindNextFileA(HANDLE,LPWIN32_FIND_DATAA);
BOOL WINAPI mock_FindClose(HANDLE);
int WINAPI mock_MultiByteToWideChar(UINT,DWORD,LPCCH,int,LPWSTR,int);
LRESULT WINAPI mock_SendMessageA(HWND,UINT,WPARAM,LPARAM);
#define GetModuleHandleA mock_GetModuleHandleA
#define MessageBoxA mock_MessageBoxA
#define LoadLibraryA mock_LoadLibraryA
#define GetProcAddress mock_GetProcAddress
#define FreeLibrary mock_FreeLibrary
#define FindFirstFileA mock_FindFirstFileA
#define FindNextFileA mock_FindNextFileA
#define FindClose mock_FindClose
#define MultiByteToWideChar mock_MultiByteToWideChar
#define SendMessageA mock_SendMessageA
#include "../core/module_menu.cpp"
#undef GetModuleHandleA
#undef MessageBoxA
#undef LoadLibraryA
#undef GetProcAddress
#undef FreeLibrary
#undef FindFirstFileA
#undef FindNextFileA
#undef FindClose
#undef MultiByteToWideChar
#undef SendMessageA

namespace {
std::array<std::uint8_t,0x210000> image{};
std::array<std::uint8_t,0x500> form{};
std::array<std::uint8_t,0x300> grid{};
std::array<std::uint8_t,0x300> row{};
std::array<void*,0x160/sizeof(void*)> grid_vtable{};
std::vector<std::string> log_lines;
std::string last_command;
unsigned hooks{},gateway_calls{},message_boxes{},process_calls{};
bool process_success=true;

LONG WINAPI report_unhandled(EXCEPTION_POINTERS* details) {
    const auto* record=details?details->ExceptionRecord:nullptr;
    std::fprintf(stderr,"core native unhandled exception code=%08lx address=%p\n",
                 record?static_cast<unsigned long>(record->ExceptionCode):0ul,
                 record?record->ExceptionAddress:nullptr);
    std::fflush(stderr);
    ExitProcess(90);
}
void checkpoint(const char* label) {
    std::fprintf(stderr,"core native checkpoint: %s\n",label);
    std::fflush(stderr);
}

void require(bool condition,const char* label) {
    if(!condition){std::fprintf(stderr,"core native failure: %s\n",label);std::exit(1);}
}
void log_line(const std::string& line){log_lines.push_back(line);}
void fake_gateway(void*,void*){++gateway_calls;}
BOOL WINAPI fake_create_process_a(LPCSTR,LPSTR command,
    LPSECURITY_ATTRIBUTES,LPSECURITY_ATTRIBUTES,BOOL,DWORD,LPVOID,LPCSTR,
    LPSTARTUPINFOA,LPPROCESS_INFORMATION) {
    ++process_calls;
    last_command=command?command:"";
    return process_success?TRUE:FALSE;
}
template <std::size_t N>
void signature(std::uintptr_t rva,const std::uint8_t (&expected)[N]) {
    std::memcpy(image.data()+rva,expected,N);
}
void populate_signatures() {
    using namespace a2fo;
    signature(kModSettingsFormShowRva,kExpectedFormShow);
    signature(kModSettingsLaunchRva,kExpectedLaunch);
    signature(kJetButtonCreateRva,kExpectedJetButtonCreate);
    signature(kJetButtonClickRva,kExpectedJetButtonClick);
    signature(kJetButtonSetBoundsRva,kExpectedJetButtonSetBounds);
    signature(kJvgCheckBoxCreateRva,kExpectedJvgCheckBoxCreate);
    signature(kJvgCheckBoxSetCheckedRva,kExpectedJvgCheckBoxSetChecked);
    signature(kAssignJvgCheckBoxImagesRva,kExpectedAssignJvgCheckBoxImages);
    signature(kAssignButtonImagesLargeCenterRva,kExpectedAssignButtonImagesLargeCenter);
    signature(kControlSetParentRva,kExpectedControlSetParent);
    signature(kControlSetVisibleRva,kExpectedControlSetVisible);
    signature(kControlSetTextRva,kExpectedControlSetText);
    signature(kControlClickRva,kExpectedControlClick);
    signature(kControlBringToFrontRva,kExpectedControlBringToFront);
    signature(kControlRepaintRva,kExpectedControlRepaint);
    signature(kComponentInsertRva,kExpectedComponentInsert);
    signature(kLongStringClearRva,kExpectedLongStringClear);
    signature(kLongStringFromPcharRva,kExpectedLongStringFromPchar);
}
void setup_base_row() {
    grid_vtable[0x148/sizeof(void*)]=reinterpret_cast<void*>(0x1234);
    *reinterpret_cast<void***>(grid.data())=grid_vtable.data();
    *reinterpret_cast<void**>(form.data()+a2fo::kGridOffset)=grid.data();
}
}

HMODULE WINAPI mock_GetModuleHandleA(LPCSTR name) {
    if(!name)return reinterpret_cast<HMODULE>(image.data());
    return nullptr;
}
int WINAPI mock_MessageBoxA(HWND,LPCSTR,LPCSTR,UINT) {++message_boxes;return IDOK;}

namespace {
struct NativeFailure : std::exception {
    int code;
    explicit NativeFailure(int value):code(value) {}
};
struct FindRecord { const char* name; DWORD attributes; std::uint64_t time; };
std::vector<FindRecord> find_records;
std::size_t find_index;
int scenario, callback_tick, throw_tick, second_conversion_result=1;
int persist_releases, link_releases, search_closes, library_frees, coinits_uninitialized;
std::string release_order;
bool shell_loaded=true, ole_loaded=true, missing_proc=false, folder_ok=true;
bool create_ok=true, create_pointer=true, query_ok=true, query_pointer=true;
bool load_ok=true, get_args_ok=true, set_args_ok=true, save_ok=true;
HRESULT initialize_result=S_OK;
std::string source_arguments="game.exe /mod=alpha";
std::string desktop="C:\\Desktop";
int list_mutation=0;
a2fo::ModuleDialogContext* active_dialog=nullptr;
void trace(const char* name) {
    std::printf("native:%s\n",name);
    if (++callback_tick==throw_tick) throw NativeFailure(callback_tick);
}
void fill_find(WIN32_FIND_DATAA* data) {
    const auto& record=find_records.at(find_index);
    std::memset(data,0,sizeof(*data));
    std::snprintf(data->cFileName,sizeof(data->cFileName),"%s",record.name);
    data->dwFileAttributes=record.attributes;
    data->ftLastWriteTime.dwLowDateTime=static_cast<DWORD>(record.time);
    data->ftLastWriteTime.dwHighDateTime=static_cast<DWORD>(record.time>>32);
}
struct FakePersist final : IPersistFile {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {trace("persist-query-unexpected");return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {trace("persist-addref-unexpected");return 2;}
    ULONG STDMETHODCALLTYPE Release() override {trace("persist-release");++persist_releases;release_order+='P';return 1;}
    HRESULT STDMETHODCALLTYPE GetClassID(CLSID*) override {trace("persist-class-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE IsDirty() override {trace("persist-dirty-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Load(LPCOLESTR path,DWORD mode) override {
        trace("persist-load");std::printf("wide-path:%ls:mode=%lu\n",path,static_cast<unsigned long>(mode));return load_ok?S_OK:E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE Save(LPCOLESTR path,WINBOOL remember) override {
        trace("persist-save");std::printf("save-path:%ls:remember=%d\n",path,remember);return save_ok?S_OK:E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE SaveCompleted(LPCOLESTR) override {trace("persist-savecompleted-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetCurFile(LPOLESTR*) override {trace("persist-getcur-unexpected");return E_NOTIMPL;}
};
FakePersist fake_persist;
struct FakeLink final : IShellLinkA {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void** out) override {
        trace("link-query");*out=query_pointer?static_cast<IPersistFile*>(&fake_persist):nullptr;
        return query_ok?S_OK:E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {trace("link-addref-unexpected");return 2;}
    ULONG STDMETHODCALLTYPE Release() override {trace("link-release");++link_releases;release_order+='L';return 1;}
    HRESULT STDMETHODCALLTYPE GetPath(LPSTR,int,WIN32_FIND_DATAA*,DWORD) override {trace("link-getpath-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetIDList(PIDLIST_ABSOLUTE*) override {trace("link-getid-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetIDList(PCIDLIST_ABSOLUTE) override {trace("link-setid-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetDescription(LPSTR,int) override {trace("link-getdescription-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetDescription(LPCSTR) override {trace("link-setdescription-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetWorkingDirectory(LPSTR,int) override {trace("link-getworking-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetWorkingDirectory(LPCSTR) override {trace("link-setworking-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetArguments(LPSTR out,int capacity) override {
        trace("link-getargs");std::snprintf(out,static_cast<std::size_t>(capacity),"%s",source_arguments.c_str());return get_args_ok?S_OK:E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE SetArguments(LPCSTR text) override {
        trace("link-setargs");std::printf("new-args:%s\n",text);return set_args_ok?S_OK:E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE GetHotkey(WORD*) override {trace("link-gethotkey-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetHotkey(WORD) override {trace("link-sethotkey-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetShowCmd(int*) override {trace("link-getshow-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetShowCmd(int) override {trace("link-setshow-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetIconLocation(LPSTR,int,int*) override {trace("link-geticon-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetIconLocation(LPCSTR,int) override {trace("link-seticon-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetRelativePath(LPCSTR,DWORD) override {trace("link-setrelative-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Resolve(HWND,DWORD) override {trace("link-resolve-unexpected");return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetPath(LPCSTR) override {trace("link-setpath-unexpected");return E_NOTIMPL;}
};
FakeLink fake_link;
HRESULT WINAPI fake_create(REFCLSID,LPUNKNOWN,DWORD,REFIID,LPVOID* out) {
    trace("co-create");*out=create_pointer?static_cast<IShellLinkA*>(&fake_link):nullptr;
    return create_ok?S_OK:E_FAIL;
}
HRESULT WINAPI fake_initialize(LPVOID,DWORD) {trace("co-initialize");return initialize_result;}
void WINAPI fake_uninitialize() {trace("co-uninitialize");++coinits_uninitialized;}
HRESULT WINAPI fake_folder(HWND,int,HANDLE,DWORD,LPSTR out) {
    trace("folder");std::snprintf(out,MAX_PATH,"%s",desktop.c_str());return folder_ok?S_OK:E_FAIL;
}
void reset_native() {
    callback_tick=0;throw_tick=0;second_conversion_result=1;find_index=0;find_records.clear();
    persist_releases=link_releases=search_closes=library_frees=coinits_uninitialized=0;release_order.clear();
    shell_loaded=ole_loaded=true;missing_proc=false;folder_ok=true;
    create_ok=create_pointer=query_ok=query_pointer=load_ok=get_args_ok=set_args_ok=save_ok=true;
    initialize_result=S_OK;source_arguments="game.exe /mod=alpha";list_mutation=0;active_dialog=nullptr;
}
}

HMODULE WINAPI mock_LoadLibraryA(LPCSTR name) {
    trace("load-library");std::printf("library:%s\n",name);
    return std::strcmp(name,"shell32.dll")==0
        ? (shell_loaded?reinterpret_cast<HMODULE>(1):nullptr)
        : (ole_loaded?reinterpret_cast<HMODULE>(2):nullptr);
}
BOOL WINAPI mock_FreeLibrary(HMODULE module) {trace("free-library");++library_frees;std::printf("freed:%u\n",static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(module)));return TRUE;}
FARPROC WINAPI mock_GetProcAddress(HMODULE,LPCSTR name) {
    trace("get-proc");std::printf("proc:%s\n",name);
    if(missing_proc&&std::strcmp(name,"CoCreateInstance")==0)return nullptr;
    if(std::strcmp(name,"SHGetFolderPathA")==0)return reinterpret_cast<FARPROC>(&fake_folder);
    if(std::strcmp(name,"CoInitializeEx")==0)return reinterpret_cast<FARPROC>(&fake_initialize);
    if(std::strcmp(name,"CoCreateInstance")==0)return reinterpret_cast<FARPROC>(&fake_create);
    if(std::strcmp(name,"CoUninitialize")==0)return reinterpret_cast<FARPROC>(&fake_uninitialize);
    return nullptr;
}
HANDLE WINAPI mock_FindFirstFileA(LPCSTR path,LPWIN32_FIND_DATAA data) {
    trace("find-first");std::printf("search:%s\n",path);
    if(find_records.empty())return INVALID_HANDLE_VALUE;
    find_index=0;fill_find(data);return reinterpret_cast<HANDLE>(3);
}
BOOL WINAPI mock_FindNextFileA(HANDLE,LPWIN32_FIND_DATAA data) {
    trace("find-next");++find_index;if(find_index>=find_records.size())return FALSE;
    fill_find(data);return TRUE;
}
BOOL WINAPI mock_FindClose(HANDLE) {trace("find-close");++search_closes;return TRUE;}
int WINAPI mock_MultiByteToWideChar(UINT page,DWORD flags,LPCCH input,int length,LPWSTR output,int capacity) {
    trace(output?"wide-write":"wide-size");
    std::printf("wide-request:%u:%lu:%d:%d\n",page,static_cast<unsigned long>(flags),length,capacity);
    const int needed=static_cast<int>(std::strlen(input))+1;
    if(output){
        if(capacity<needed)return 0;
        for(int i=0;i<needed;++i)output[i]=static_cast<wchar_t>(static_cast<unsigned char>(input[i]));
        return second_conversion_result?needed:0;
    }
    return needed;
}
LRESULT WINAPI mock_SendMessageA(HWND,UINT message,WPARAM w,LPARAM l) {
    switch(message) {
    case LVM_INSERTCOLUMNA: {
        auto* column=reinterpret_cast<LVCOLUMNA*>(l);
        std::printf("list-column:%u:%u:%d:%d:%s\n",static_cast<unsigned>(w),column->mask,column->cx,column->iSubItem,column->pszText);break;
    }
    case LVM_INSERTITEMA: {
        auto* item=reinterpret_cast<LVITEMA*>(l);
        std::printf("list-item:%d:%u:%ld:%s\n",item->iItem,item->mask,static_cast<long>(item->lParam),item->pszText);
        if(list_mutation==1&&active_dialog&&active_dialog->policy.entries.size()==1)
            active_dialog->policy.entries.push_back({"next","Added","","",a2fo::ModulePolicyState::required,true});
        break;
    }
    case LVM_SETITEMTEXTA: {
        auto* item=reinterpret_cast<LVITEMA*>(l);
        std::printf("list-text:%u:%d:%s\n",static_cast<unsigned>(w),item->iSubItem,item->pszText);
        if(list_mutation==2&&active_dialog&&item->iSubItem==1)
            active_dialog->policy.entries.at(static_cast<std::size_t>(w)).state=a2fo::ModulePolicyState::rejected;
        break;
    }
    case LVM_SETITEMSTATE: {
        auto* item=reinterpret_cast<LVITEMA*>(l);
        std::printf("list-check:%u:%u:%u\n",static_cast<unsigned>(w),item->state,item->stateMask);break;
    }
    default: std::printf("list-unexpected:%u\n",message);break;
    }
    return 1;
}

namespace a2fo {
#if A2FO_DIRECT_HOOK_BINDINGS
extern "C" std::uint8_t a2fo_rs_windows_hook_install(void* target,void*,std::size_t length,
                         const std::uint8_t* expected,A2foRsInlineHook* hook) {
    require(std::memcmp(target,expected,length)==0,"checked inline signature");
    ++hooks;hook->gateway=reinterpret_cast<void*>(&fake_gateway);return 1;
}
extern "C" bool a2fo_rs_windows_patch_jump(void* target,void*,const std::uint8_t* expected,
                std::size_t length) {
    require(std::memcmp(target,expected,length)==0,"checked jump signature");
    ++hooks;return true;
}
#else
bool install_inline_hook(void* target,void*,std::size_t length,
                         const std::uint8_t* expected,InlineHook& hook) {
    require(std::memcmp(target,expected,length)==0,"checked inline signature");
    ++hooks;hook.gateway=reinterpret_cast<void*>(&fake_gateway);return true;
}
bool patch_jump(void* target,void*,const std::uint8_t* expected,
                std::size_t length) {
    require(std::memcmp(target,expected,length)==0,"checked jump signature");
    ++hooks;return true;
}
#endif
std::vector<InstalledModule> discover_installed_modules(const std::string&,
                                                        std::vector<std::string>*) {return {};}
ModulePolicy evaluate_module_policy(const std::vector<std::string>&,
                                    const std::vector<InstalledModule>&) {return {};}
bool save_active_module_selection(const std::string&,
                                  const std::vector<std::string>&,
                                  std::string&) {return true;}
bool apply_builtin_module_policy(const std::vector<std::string>&,
                                 const ModulePolicy&,std::string&) {return true;}
bool builtin_modules_match_roots(const std::vector<std::string>&) {return true;}
std::string builtin_module_status(const std::string&,
                                  const std::vector<std::string>&) {return "active";}
}

extern "C" void* a2fo_call_delphi_one_register(void* function,void*) {
    if(function==grid_vtable[0x148/sizeof(void*)])return nullptr;
    return nullptr;
}
extern "C" void* a2fo_call_delphi_two_registers(void* function,void*,void*) {
    if(function==a2fo::at(a2fo::kNextGridGetRowRva))return row.data();
    if(function==a2fo::g_launch_hook.gateway)++gateway_calls;
    return nullptr;
}
extern "C" void* a2fo_call_delphi_constructor(void*,void*,void*) {return nullptr;}
extern "C" void a2fo_call_delphi_set_bounds(void*,void*,int,int,int,int) {}
extern "C" void a2fo_mod_settings_form_show_bridge() {}
extern "C" void a2fo_mod_settings_launch_bridge() {}
extern "C" void a2fo_module_menu_jet_button_click_bridge() {}

int main(int argc,char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|
                 SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(report_unhandled);
    checkpoint("main entered");
    require(argc==2,"expected case name");
    const std::string mode=argv[1];
    checkpoint(mode.c_str());
    using namespace a2fo;
    if(mode=="missing") {
        checkpoint("missing before null image");
        require(!install_module_menu(nullptr,"Data",log_line),"missing image rejected");
        checkpoint("missing after null image");
        require(!install_module_menu(reinterpret_cast<HMODULE>(image.data()),"",log_line),
                "empty root rejected");
        checkpoint("missing after empty root");
        require(hooks==0,"no hooks before admission");
    } else if(mode=="signature") {
        require(!install_module_menu(reinterpret_cast<HMODULE>(image.data()),"Data",log_line),
                "bad signature rejected");
        require(hooks==0,"signature failure skips hooks");
    } else if(mode=="install") {
        populate_signatures();
        require(install_module_menu(reinterpret_cast<HMODULE>(image.data()),"Data",log_line),
                "valid signatures install");
        require(hooks==3,"all three checked hooks attempted");
    } else if(mode=="selection") {
        g_data_root="Data";
        setup_base_row();
        SelectedMod selected;std::string error;
        require(resolve_selected_mod(form.data(),selected,error),"synthetic Data row");
        require(selected.base&&selected.root=="Data"&&selected.info_path=="Data\\info.ini",
                "base mod root and info path");
    } else if(mode=="launch") {
        g_data_root="Data";g_launch_hook.gateway=reinterpret_cast<void*>(&fake_gateway);
        setup_base_row();
        g_nointro_fallback_state.checked=true;
        g_noai_fallback_state.checked=false;
        g_original_create_process_a=&fake_create_process_a;
        a2fo_mod_settings_launch_hook_cpp(form.data(),nullptr);
        require(gateway_calls==1,"native launch gateway invoked");
        require(InterlockedCompareExchange(&g_pending_nointro,0,0)==1,
                "launch flag armed before process call");
        char command[]="game.exe";
        require(a2fo_mod_launch_create_process_a(nullptr,command,nullptr,nullptr,FALSE,0,
                nullptr,nullptr,nullptr,nullptr),"mock process creation");
        require(last_command=="game.exe /nointro"&&process_calls==1,
                "switch appended once to native command");
        require(InterlockedCompareExchange(&g_pending_nointro,0,0)==0,
                "successful process call consumes pending flags");
    } else if(mode=="failed-launch") {
        g_original_create_process_a=&fake_create_process_a;
        process_success=false;
        InterlockedExchange(&g_pending_noai,1);
        char command[]="game.exe";
        require(!a2fo_mod_launch_create_process_a(nullptr,command,nullptr,nullptr,FALSE,0,
                nullptr,nullptr,nullptr,nullptr),"mock process failure");
        require(last_command=="game.exe /noai","switch passed on failure");
        require(InterlockedCompareExchange(&g_pending_noai,0,0)==1,
                "failed process call retains pending flags");
    } else if(mode=="wide") {
        for(const std::string& path:{std::string(),std::string("x"),std::string("C:\\Desktop\\a rather long shortcut.lnk")}) {
            reset_native();std::printf("wide-case:%s\n",path.c_str());
            const auto result=path_to_wide(path);std::printf("wide-result:%ls:%u\n",result.c_str(),static_cast<unsigned>(result.size()));
            require(result==std::wstring(path.begin(),path.end()),"wide path value");
        }
        reset_native();second_conversion_result=0;
        const auto ignored=path_to_wide("C:\\ignored-second-result.lnk");
        std::printf("wide-ignored:%ls:%u\n",ignored.c_str(),static_cast<unsigned>(ignored.size()));
        require(ignored==L"C:\\ignored-second-result.lnk","ignored second conversion result");
    } else if(mode=="shortcut") {
        for(int index=0;index<12;++index) {
            reset_native();scenario=index;
            bool nointro=true,noai=false;
            if(index==1)create_ok=false;
            if(index==2)create_pointer=false;
            if(index==3)query_ok=false;
            if(index==4)query_pointer=false;
            if(index==5)load_ok=false;
            if(index==6)get_args_ok=false;
            if(index==7)source_arguments="game.exe /mod=beta";
            if(index==8){source_arguments="game.exe /mod=alpha /nointro";nointro=false;}
            if(index==9){source_arguments="game.exe /mod=alpha /nointro";}
            if(index==10)set_args_ok=false;
            if(index==11)save_ok=false;
            std::printf("shortcut-case:%d\n",index);
            const bool result=update_shortcut_arguments("C:\\Desktop\\created.lnk","alpha",nointro,noai,&fake_create);
            std::printf("shortcut-result:%d\n",result);
            require(result==(index==0||index==9),"shortcut result contract");
            require(release_order==(index==1||index==2?"":(index==3||index==4?"L":"PL")),"shortcut release order");
        }
        reset_native();throw_tick=4;
        std::puts("shortcut-case:throw");
        bool caught=false;
        try {update_shortcut_arguments("C:\\Desktop\\created.lnk","alpha",true,false,&fake_create);}
        catch(const NativeFailure& failure){caught=true;std::printf("shortcut-caught:%d\n",failure.code);}
        require(caught,"typed native shortcut exception propagates");
        require(persist_releases==0&&link_releases==0,"shortcut exception has no added cleanup");
    } else if(mode=="recent") {
        for(int index=0;index<16;++index) {
            reset_native();scenario=index;
            SelectedMod selected;selected.folder="alpha";
            bool nointro=true,noai=false;
            if(index==0)selected.base=true;
            if(index==1)selected.folder.clear();
            if(index==2)nointro=false;
            if(index==3)shell_loaded=false;
            if(index==4)ole_loaded=false;
            if(index==5)missing_proc=true;
            if(index==6)folder_ok=false;
            if(index==7)initialize_result=E_FAIL;
            if(index==8)initialize_result=RPC_E_CHANGED_MODE;
            if(index==9)initialize_result=S_FALSE;
            if(index>=10) {
                find_records={{"directory.lnk",FILE_ATTRIBUTE_DIRECTORY,100000000ULL},
                              {"old.lnk",0,49999999ULL},
                              {"edge.lnk",0,50000000ULL},
                              {"new.lnk",0,100000001ULL}};
                if(index==11)source_arguments="game.exe /mod=beta";
                if(index==12)save_ok=false;
                if(index==13)find_records={{"old.lnk",0,49999999ULL}};
                if(index==14)find_records={{"directory.lnk",FILE_ATTRIBUTE_DIRECTORY,100000000ULL}};
                if(index==15)find_records={{"fresh.lnk",0,100000000ULL}};
            }
            std::printf("recent-case:%d\n",index);
            FILETIME start{100000000U,0};
            const bool result=update_recent_desktop_shortcut(selected,nointro,noai,start);
            std::printf("recent-result:%d\n",result);
            require(result==(index<=2||index==10||index==15),"recent shortcut result contract");
            require(coinits_uninitialized==(index==9||index>=10),"COM S_FALSE cleanup and changed-mode skip");
        }
        reset_native();find_records={{"fresh.lnk",0,100000000ULL}};throw_tick=11;
        std::puts("recent-case:throw");
        FILETIME start{100000000U,0};SelectedMod selected;selected.folder="alpha";
        bool caught=false;
        try {update_recent_desktop_shortcut(selected,true,false,start);}
        catch(const NativeFailure& failure){caught=true;std::printf("recent-caught:%d\n",failure.code);}
        require(caught,"typed native recent exception propagates");
        require(search_closes==0&&library_frees==0&&coinits_uninitialized==0,
                "recent exception has no added cleanup");
    } else if(mode=="list") {
        for(int variant=0;variant<4;++variant) {
            reset_native();list_mutation=variant==2?1:(variant==3?2:0);
            ModuleDialogContext dialog;dialog.list=reinterpret_cast<HWND>(7);
            dialog.policy.entries.reserve(4);
            if(variant!=0)dialog.policy.entries.push_back({"one","Primary","","",ModulePolicyState::active,true});
            if(variant==1)dialog.policy.entries.push_back({"two","Secondary","","",ModulePolicyState::inactive,false});
            dialog.selected.roots={"Data","Mods\\alpha"};active_dialog=&dialog;
            std::printf("list-case:%d\n",variant);
            populate_module_list(dialog);
            std::printf("list-end:%u:%u\n",static_cast<unsigned>(dialog.policy.entries.size()),dialog.initializing);
            require(!dialog.initializing,"list initialization completed");
            require(dialog.policy.entries.size()==static_cast<std::size_t>(variant==0?0:(variant==1||variant==2?2:1)),"live list size");
        }
    } else {require(false,"unknown case");}
    checkpoint("case complete");
    std::printf("stage9 module menu %s: PASS\n",mode.c_str());
}
