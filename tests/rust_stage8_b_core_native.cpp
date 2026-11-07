// Keep synthetic hook callbacks distinct from the real Rust archive symbols.
#define a2fo_rs_windows_hook_install fixture_windows_hook_install
#define a2fo_rs_windows_patch_jump fixture_windows_patch_jump
#define a2fo_rs_windows_patch_call fixture_windows_patch_call
#define a2fo_rs_windows_patch_bytes fixture_windows_patch_bytes
// Headless native mock for the frozen/current Mods menu sources.
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

HMODULE WINAPI mock_GetModuleHandleA(LPCSTR);
int WINAPI mock_MessageBoxA(HWND,LPCSTR,LPCSTR,UINT);
#define GetModuleHandleA mock_GetModuleHandleA
#define MessageBoxA mock_MessageBoxA
#include "../core/module_menu.cpp"
#undef GetModuleHandleA
#undef MessageBoxA

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
std::string active_mod_from_command_line(const std::string&) {return {};}
bool safe_mod_directory_name(const std::string& name) {return !name.empty();}
ExtensionRootDiscovery discover_extension_roots(const std::string& root,
                                                 const std::string&) {
    return {{root}, {}, {}};
}
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
    } else {require(false,"unknown case");}
    checkpoint("case complete");
    std::printf("core module menu %s: PASS\n",mode.c_str());
}
