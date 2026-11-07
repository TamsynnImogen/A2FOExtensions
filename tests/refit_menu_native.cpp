// Console-only real DLL admission and Delphi event forwarding fixture.
// No HWND creation, engine entrypoint or input automation. Real action bodies
// run only after their three side-effect helpers are isolated in the mapping.
#include "../sdk/include/a2fo_module_api.h"
#include "../sdk/include/a2fo_menu_scene.h"
#include "../rust/native_hooks_ffi.h"
#include "../modules/A2FORefitMenus/bindings.hpp"
#include "../modules/A2FORefitMenus/skin_bindings.hpp"
#include "../modules/A2FORefitMenus/status.hpp"
#include "../modules/A2FORefitMenus/loading_bindings.hpp"
#include "support/refit_fo_chat_fixture.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
std::uint32_t refit_menu_test_call(void*,void*,void*);
std::uint32_t refit_menu_test_call_unaligned(void*,void*,void*,unsigned);
void refit_menu_test_original();
void refit_menu_test_bitmap_original();
void refit_menu_test_bitmap_width();
void refit_menu_test_bitmap_height();
void refit_menu_test_bitmap_canvas();
void refit_menu_test_bitmap_dc();
void refit_menu_test_shell_original0();
void refit_menu_test_shell_original1();
void refit_menu_test_shell_original2();
void refit_menu_test_shell_call(void*,void*,HDC,unsigned);
void refit_menu_test_dispatch();
void refit_menu_test_running();
void refit_menu_test_close();
void* refit_menu_seen_self = nullptr;
void* refit_menu_seen_sender = nullptr;
unsigned refit_menu_seen_calls = 0;
std::uint16_t refit_menu_seen_dialog = 0;
std::uint32_t refit_menu_seen_running = 0;
void* refit_menu_seen_closed = nullptr;
void* refit_menu_skin_seen_bitmap = nullptr;
void* refit_menu_skin_seen_name = nullptr;
std::uintptr_t refit_menu_skin_seen_stack = 0;
std::uintptr_t refit_menu_skin_caller_stack = 0;
unsigned refit_menu_skin_seen_calls = 0;
std::uint32_t refit_menu_skin_return = 0;
int refit_menu_skin_width = 0, refit_menu_skin_height = 0;
void* refit_menu_skin_dc = nullptr;
void* refit_menu_shell_seen_self = nullptr;
HDC refit_menu_shell_seen_dc = nullptr;
std::uintptr_t refit_menu_shell_seen_stack = 0;
std::uintptr_t refit_menu_shell_caller_stack = 0;
std::uintptr_t refit_menu_shell_after_stack = 0;
unsigned refit_menu_shell_seen_calls[3]{};
}
namespace {
HMODULE engine = nullptr, fo = nullptr;
std::string root, diagnostic;
unsigned patches = 0;
bool refuse = false, refuse_skin_second = false;
void* replacement = nullptr;
void* bitmap_replacement = nullptr;
void* bitmap32_replacement = nullptr;
void* image_paint_replacement = nullptr;
std::array<void*,3> shell_draw_replacements{};
void* loading_replacement{};
unsigned loading_calls{};
void* loading_status_replacement{};
unsigned loading_status_patches{},loading_status_calls{};
const char* loading_status_pointer{};
void __cdecl loading_status_original(const char* text){++loading_status_calls;loading_status_pointer=text;}
void require(bool value,const char* text);
unsigned mission_patches{},mission_calls{};
void* mission_replacement{};
void* mission_receiver{};
void __fastcall mission_original(void* chat,void*){++mission_calls;mission_receiver=chat;}
bool A2FO_CALL patch_mission(void* target,void* to,const std::uint8_t* expected,std::size_t size){
    require(target==reinterpret_cast<BYTE*>(engine)+0x198a16&&size==5,"mission call site");
    ++mission_patches;
    const bool ok=a2fo_rs_windows_patch_call(target,to,expected,size);
    if(ok)mission_replacement=to;
    return ok;
}
bool A2FO_CALL patch_loading_status(void* target,void* to,const std::uint8_t* expected,std::size_t size){
    // This scenario supplies only the loading observer service; the parent's
    // independent mission-backdrop observer may also request an optional CALL.
    if(target!=reinterpret_cast<BYTE*>(engine)+a2fo::refit_menu::loading_bindings::status_site)return false;
    require(size==5,"loading status call site");
    ++loading_status_patches;
    const bool ok=a2fo_rs_windows_patch_call(target,to,expected,size);
    if(ok)loading_status_replacement=to;
    return ok;
}
void __stdcall loading_original(float first,void* device,float progress){
    require(first==-2.5f&&device==reinterpret_cast<void*>(1)&&progress==.375f,"loading raw arguments");
    ++loading_calls;
}
void require(bool value,const char* text) { if (!value) throw std::runtime_error(text); }
void A2FO_CALL logger(const char*,const char* text) { diagnostic += std::string(text) + '\n'; }
void* A2FO_CALL armada() { return engine; }
void* A2FO_CALL fleetops() { return fo; }
const char* A2FO_CALL directory() { return root.c_str(); }
bool A2FO_CALL install(void* site,void* to,std::size_t size,const std::uint8_t* expected,A2FO_InlineHook* out) {
    ++patches;
    const auto rva=static_cast<std::uint32_t>(reinterpret_cast<BYTE*>(site)-reinterpret_cast<BYTE*>(fo));
    if (refuse || (refuse_skin_second && rva==a2fo::refit_menu::native::bitmap32_load_rva)) return false;
    A2foRsInlineHook actual{};
    if (!a2fo_rs_windows_hook_install(site,to,size,expected,&actual)) return false;
    // Actual checked patch is installed in the data mapping. Never execute
    // its original FO gateway: the fixture supplies ABI-aware stubs instead.
    if(rva==a2fo::refit_menu::loading_bindings::new_bar){
        loading_replacement=to;
        *out={actual.target,reinterpret_cast<void*>(&loading_original),actual.length};
    } else if (rva==a2fo::refit_menu::native::show_rva) {
        replacement=to;
        *out={actual.target,reinterpret_cast<void*>(&refit_menu_test_original),actual.length};
    } else if (rva==a2fo::refit_menu::native::bitmap_load_rva ||
               rva==a2fo::refit_menu::native::bitmap32_load_rva) {
        (rva==a2fo::refit_menu::native::bitmap_load_rva?bitmap_replacement:bitmap32_replacement)=to;
        *out={actual.target,reinterpret_cast<void*>(&refit_menu_test_bitmap_original),actual.length};
    } else if (rva==a2fo::refit_menu::native::image_paint_rva) {
        image_paint_replacement=to;
        *out={actual.target,reinterpret_cast<void*>(&refit_menu_test_original),actual.length};
    } else {
        const auto& rvas=a2fo::refit_menu::native::shell_draw_rvas;
        const auto found=std::find(rvas.begin(),rvas.end(),rva);
        if (found==rvas.end()) throw std::runtime_error("unexpected hook site");
        const auto index=static_cast<std::size_t>(found-rvas.begin());
        const void* originals[]{reinterpret_cast<void*>(&refit_menu_test_shell_original0),
            reinterpret_cast<void*>(&refit_menu_test_shell_original1),
            reinterpret_cast<void*>(&refit_menu_test_shell_original2)};
        shell_draw_replacements[index]=to;
        *out={actual.target,const_cast<void*>(originals[index]),actual.length};
    }
    return true;
}
struct DelphiName {
    std::int32_t length;
    char text[96];
};
DelphiName name(const char* path) {
    DelphiName result{};
    result.length=static_cast<std::int32_t>(std::strlen(path));
    require(result.length<static_cast<std::int32_t>(sizeof(result.text)),"test path length");
    std::memcpy(result.text,path,result.length+1);
    return result;
}
// Discover a VMT from the mapped FO image using the native class-name field.
// The test does not invent or hardcode a TBitmap32 VMT address.
void* bitmap32_vmt() {
    const auto base=reinterpret_cast<std::uintptr_t>(fo);
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(fo);
    require(dos->e_magic==IMAGE_DOS_SIGNATURE,"mapped FO DOS header");
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
    require(nt->Signature==IMAGE_NT_SIGNATURE,"mapped FO NT header");
    const std::size_t span=std::min<std::size_t>(nt->OptionalHeader.SizeOfImage,0x300000);
    std::vector<BYTE> mapped(span);
    SIZE_T got{};
    require(ReadProcessMemory(GetCurrentProcess(),fo,mapped.data(),span,&got) && got==span,
        "read bounded FO image for VMT lookup");
    for (std::size_t offset=0;offset+10<span;++offset) {
        const auto* bytes=mapped.data()+offset;
        if (bytes[0]!=9 || std::memcmp(bytes+1,"TBitmap32",9)) continue;
        const auto address=base+offset;
        for (std::size_t field=0;field+4<span;field+=4) {
            std::uint32_t pointer{};
            std::memcpy(&pointer,mapped.data()+field,4);
            if (pointer==address && field+44<span)
                return reinterpret_cast<void*>(base+field+44);
        }
    }
    throw std::runtime_error("mapped FO TBitmap32 VMT not found");
}
void exercise_loader(void* bridge,void* bitmap,DelphiName& path,std::uint32_t expected) {
    refit_menu_skin_seen_bitmap=nullptr;refit_menu_skin_seen_name=nullptr;
    refit_menu_skin_return=expected;
    unsigned before=refit_menu_skin_seen_calls;
    require(refit_menu_test_call(bridge,bitmap,path.text)==expected,"native loader return register");
    require(refit_menu_skin_seen_calls==before+1 && refit_menu_skin_seen_bitmap==bitmap &&
        refit_menu_skin_seen_name==path.text,"native loader original EAX/EDX forwarding");
    const auto gateway_alignment=refit_menu_skin_seen_stack & 15u;
    ++before;
    unsigned alignments=0;
    for (unsigned padding : {0u,4u,8u,12u}) {
        require(refit_menu_test_call_unaligned(bridge,bitmap,path.text,padding)==expected,
            "native loader return with four stack alignments");
        require(refit_menu_skin_seen_calls==++before && refit_menu_skin_seen_bitmap==bitmap &&
            refit_menu_skin_seen_name==path.text,"native loader aligned entry forwarding");
        require((refit_menu_skin_seen_stack&15u)==gateway_alignment,"normalized original call alignment");
        alignments |= 1u << ((refit_menu_skin_caller_stack & 15u)/4u);
    }
    require(alignments==15,"fixture supplied four native stack alignments");
}
void exercise_shell_fallback(std::size_t index,void* object,HDC dc) {
    const unsigned before=refit_menu_shell_seen_calls[index];
    unsigned alignments=0;
    for (unsigned padding : {0u,4u,8u,12u}) {
        refit_menu_shell_seen_self=nullptr;refit_menu_shell_seen_dc=nullptr;
        refit_menu_test_shell_call(shell_draw_replacements[index],object,dc,padding);
        require(refit_menu_shell_seen_calls[index]==before+1+(padding/4) &&
            refit_menu_shell_seen_self==object && refit_menu_shell_seen_dc==dc,
            "native shell original ECX/stack-HDC forwarding");
        require(refit_menu_shell_after_stack==refit_menu_shell_caller_stack+sizeof(HDC),
            "native shell callee removes stack HDC");
        alignments |= 1u << ((refit_menu_shell_caller_stack&15u)/4u);
    }
    require(alignments==15,"fixture supplied four shell stack alignments");
}
template<class T> T symbol(HMODULE m,const char* name) {
    const auto p = GetProcAddress(m,name); T out=nullptr;
    static_assert(sizeof(out)==sizeof(p)); std::memcpy(&out,&p,sizeof(out)); return out;
}
void mutate(HMODULE m,std::uint32_t rva) {
    auto* p = reinterpret_cast<BYTE*>(m)+rva;
    DWORD old=0,unused=0;
    require(VirtualProtect(p,1,PAGE_EXECUTE_READWRITE,&old)!=0,"fixture mutation protection");
    *p ^= 1;
    require(VirtualProtect(p,1,old,&unused)!=0,"fixture restore protection");
}
void isolate_action_helpers() {
    // Redirect only the three side-effect helpers in the private FO mapping.
    // Real native action bodies execute; no form Close or engine global write
    // reaches the game. Lengths end at independently disassembled boundaries.
    struct Stub { std::uint32_t rva; std::size_t size; void* code; };
    const Stub stubs[]{{0x1ddcc0,9,reinterpret_cast<void*>(&refit_menu_test_dispatch)},
        {0x1ddcd0,6,reinterpret_cast<void*>(&refit_menu_test_running)},
        {0xb1b08,5,reinterpret_cast<void*>(&refit_menu_test_close)}};
    for (const auto& stub:stubs) {
        auto* target=reinterpret_cast<BYTE*>(fo)+stub.rva;
        std::array<BYTE,9> expected{};std::memcpy(expected.data(),target,stub.size);
        require(a2fo_rs_windows_patch_jump(target,stub.code,expected.data(),stub.size),"isolate native action helpers");
    }
}
void isolate_bitmap_helpers() {
    struct Stub { std::uint32_t rva; void* code; };
    const Stub stubs[]{
        {0x94000,reinterpret_cast<void*>(&refit_menu_test_bitmap_width)},
        {0x93e78,reinterpret_cast<void*>(&refit_menu_test_bitmap_height)},
        {0x93de0,reinterpret_cast<void*>(&refit_menu_test_bitmap_canvas)},
        {0x8f538,reinterpret_cast<void*>(&refit_menu_test_bitmap_dc)},
    };
    for (const auto& stub:stubs) {
        for (const auto& sig:a2fo::refit_menu::native::skin_signatures) {
            if (sig.rva!=stub.rva) continue;
            auto* target=reinterpret_cast<BYTE*>(fo)+stub.rva;
            require(a2fo_rs_windows_patch_jump(target,stub.code,reinterpret_cast<const BYTE*>(sig.bytes),sig.size),
                "isolate native TBitmap helper");
            break;
        }
    }
}
void exercise_tbitmap() {
    isolate_bitmap_helpers();
    HDC dc=CreateCompatibleDC(nullptr);
    require(dc!=nullptr,"fixture bitmap DC");
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=2; info.bmiHeader.biHeight=-2;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    void* storage=nullptr;
    HBITMAP dib=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&storage,nullptr,0);
    require(dib && storage,"fixture DIB section");
    HGDIOBJ prior=SelectObject(dc,dib);
    require(prior && prior!=HGDI_ERROR,"fixture DIB selection");
    auto* pixels=static_cast<std::uint32_t*>(storage);
    pixels[0]=0x00336699u; pixels[1]=0xffff00ffu;
    pixels[2]=0xff808080u; pixels[3]=0xffe8e8e8u;
    std::array<BYTE,0x60> bitmap{};
    void* vmt=reinterpret_cast<BYTE*>(fo)+a2fo::refit_menu::native::bitmap_vmt_rva;
    void* data=bitmap.data();
    std::memcpy(bitmap.data(),&vmt,sizeof(vmt));
    std::memcpy(bitmap.data()+0x28,&data,sizeof(data));
    refit_menu_skin_width=2;refit_menu_skin_height=2;refit_menu_skin_dc=dc;
    auto known=name("bitmaps/main/mainbkgr.bmp");
    exercise_loader(bitmap_replacement,bitmap.data(),known,0x55667788u);
    require((pixels[0]&0x00ffffffu)!=0x00336699u &&
        (pixels[1]&0x00ffffffu)==0x00ff00ffu,"TBitmap zero-alpha GDI styled and magenta key retained");
    SelectObject(dc,prior);DeleteObject(dib);DeleteDC(dc);
    refit_menu_skin_dc=nullptr;
}
}
int main(int argc,char** argv) {
    if (argc!=5) return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    const std::string requested_mode=argv[4];
    const bool loading=requested_mode=="loading"||requested_mode=="loading-signature";
    const bool loading_signature=requested_mode=="loading-signature";
    const bool mission_fo=requested_mode=="mission-fo"||requested_mode=="mission-fo-gateway";
    const bool mission=requested_mode=="mission"||requested_mode=="mission-signature"||mission_fo;
    const bool mission_signature=requested_mode=="mission-signature"||requested_mode=="mission-fo-gateway";
    const std::string mode=loading||mission?"success":requested_mode;
    char temp[MAX_PATH]{},folder[MAX_PATH]{};
    try {
        require(GetTempPathA(sizeof(temp),temp)!=0 && GetTempFileNameA(temp,"rfm",0,folder)!=0,"temporary path");
        require(DeleteFileA(folder) && CreateDirectoryA(folder,nullptr),"temporary root"); root=folder;
        const auto ini=root+"\\A2FORefitMenus.ini";
        if (mode!="missing") require(WritePrivateProfileStringA("RefitMenus","Enabled",mode=="disabled"?"0":mode=="invalid"?"yes":"1",ini.c_str())!=0,"temporary config");
        if(loading)require(WritePrivateProfileStringA("RefitMenus","LoadingScreen","1",ini.c_str())!=0,"loading config");
        std::string input=argv[3];
        if (mode=="profile") {
            input=root+"\\UnknownFleetOpsHook.dll";
            require(CopyFileA(argv[3],input.c_str(),TRUE)!=0,"private profile copy");
            HANDLE f=CreateFileA(input.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            require(f!=INVALID_HANDLE_VALUE,"private profile file");
            SetFilePointer(f,3043839,nullptr,FILE_BEGIN); const BYTE byte=0x73;DWORD written=0;
            require(WriteFile(f,&byte,1,&written,nullptr) && written==1,"alter private profile"); CloseHandle(f);
        }
        engine=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        fo=LoadLibraryExA(input.c_str(),nullptr,DONT_RESOLVE_DLL_REFERENCES);
        require(engine && fo,"map reference inputs without entrypoints");
        if(mission_fo){
            const auto gateway=refit_fo_chat_fixture::install(engine,fo,reinterpret_cast<void*>(&mission_original));
            if(mission_signature){const BYTE bad=0x90;refit_fo_chat_fixture::put(gateway,&bad,1);}
        }
        if(loading_signature)mutate(fo,a2fo::refit_menu::loading_bindings::new_bar);
        if(mission_signature&&!mission_fo)mutate(engine,0x198a0c);
        if (mode=="identity") mutate(fo,0);
        if (mode=="armada") mutate(engine,0);
        if (mode=="foreign") mutate(fo,a2fo::refit_menu::native::show_rva);
        if (mode=="action") mutate(fo,a2fo::refit_menu::native::action_rvas[2]);
        if (mode=="skin-signature") mutate(fo,a2fo::refit_menu::native::bitmap32_load_rva);
        refuse=mode=="patch-fail";
        refuse_skin_second=mode=="skin-patch-fail";
        const auto module=LoadLibraryA(argv[1]);require(module!=nullptr,"load real module");
        const auto init=symbol<A2FO_ModuleInitFn>(module,"A2FO_ModuleInit");
        const auto shutdown=symbol<A2FO_ModuleShutdownFn>(module,"A2FO_ModuleShutdown");
        using Query=bool (A2FO_CALL*)(a2fo::refit_menu::Status*);
        const auto query=symbol<Query>(module,"A2FORefitMenus_QueryStatus");
        const auto scene=symbol<A2FO_MenuScenePresentV1>(module,"A2FORefitMenus_PresentSceneV1");
        const auto depart=symbol<A2FO_MenuSceneBeginDepartureV1>(module,"A2FORefitMenus_BeginDepartureV1");
        const auto end_depart=symbol<A2FO_MenuSceneEndDepartureV1>(module,"A2FORefitMenus_EndDepartureV1");
        const auto mission_details=symbol<A2FO_MenuSceneMissionV1>(module,"A2FORefitMenus_SetMissionV1");
        require(init && shutdown && query&&scene&&depart&&end_depart&&mission_details,"module and optional presentation exports");
        require(!scene(nullptr,nullptr,1920,1080,"custom42",0)&&!depart("custom42")&&!mission_details(nullptr),
                "presentation bridge rejects unavailable inputs before init");
        A2FO_ModuleApi api{};api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
        api.log=logger;api.armada_module=armada;api.fleetops_module=fleetops;api.root_directory=directory;api.install_inline_hook=install;
        if(mission)api.patch_call=&patch_mission;
        if(loading)api.patch_call=&patch_loading_status;
        require(!init(nullptr),"reject null API");
        if (mode=="api") { api.struct_size=offsetof(A2FO_ModuleApi,install_inline_hook);require(!init(&api) && !patches,"reject short API"); }
        else {
            require(init(&api),"module init");
            a2fo::refit_menu::Status status{sizeof(status),0,0,0,0,0};
            require(!query(nullptr),"null status rejected");
            require(query(&status),"status query");
            const bool success=mode=="success" || mode=="skin-signature" || mode=="skin-patch-fail";
            require(status.installed==static_cast<unsigned>(success),"admission boundary");
            const unsigned expected_patches=mode=="success"?7u+(loading&&!loading_signature?1u:0u):mode=="skin-patch-fail"?3u:
                success||refuse?1u:0u;
            require(patches==expected_patches,"checked patch count");
            require(init(&api) && patches==expected_patches,"idempotent init");
            if (success) {
                if(loading){
                    require(bool(loading_replacement)==!loading_signature,"loading signature admission");
                    if(loading_replacement){
                        require(loading_status_patches==1&&loading_status_replacement,"real DLL status observer armed");
                        auto* status_entry=reinterpret_cast<BYTE*>(engine)+a2fo::refit_menu::loading_bindings::status_update;
                        BYTE jump[5]{0xe9};const auto distance=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&loading_status_original)-reinterpret_cast<std::uintptr_t>(status_entry)-5);
                        std::memcpy(jump+1,&distance,4);DWORD old{};
                        require(VirtualProtect(status_entry,5,PAGE_EXECUTE_READWRITE,&old),"status fixture entry writable");
                        std::memcpy(status_entry,jump,5);DWORD unused{};VirtualProtect(status_entry,5,old,&unused);
                        FlushInstructionCache(GetCurrentProcess(),status_entry,5);
                        const char* step="Loading native fixture stage";
                        reinterpret_cast<void(__cdecl*)(const char*)>(loading_status_replacement)(step);
                        require(loading_status_calls==1&&loading_status_pointer==step,"real DLL forwards exact status pointer once");
                        using Load=void(__stdcall*)(float,void*,float);
                        reinterpret_cast<Load>(loading_replacement)(-2.5f,reinterpret_cast<void*>(1),.375f);
                        require(loading_calls==1,"real DLL loading callback forwards native fallback");
                    }
                }
                require(replacement,"main replacement retained after skin hooks");
                if (mode=="success") require(bitmap_replacement && bitmap32_replacement && image_paint_replacement &&
                    std::all_of(shell_draw_replacements.begin(),shell_draw_replacements.end(),[](void* p){return p!=nullptr;}),
                    "all skin and paint hooks installed");
                else require(diagnostic.find("skin")!=std::string::npos || diagnostic.find("Skin")!=std::string::npos,
                    "skin failure logged while main remains active");
                if (mode=="skin-signature")
                    require(!bitmap_replacement && !bitmap32_replacement,"skin signature failure installs no loader hooks");
                if (mode=="skin-patch-fail") {
                    require(bitmap_replacement && !bitmap32_replacement,"partial loader installation boundary");
                    auto known=name("bitmaps/main/mainbkgr.png");
                    std::array<BYTE,0x60> dummy{};
                    exercise_loader(bitmap_replacement,dummy.data(),known,0x1234abcd);
                }
                if (mode=="success") {
                    std::array<BYTE,0x200> invalid_image{};
                    const auto before=refit_menu_seen_calls;
                    refit_menu_test_call(image_paint_replacement,invalid_image.data(),nullptr);
                    require(refit_menu_seen_calls==before+1 && refit_menu_seen_self==invalid_image.data(),"foreign image falls back through paint gateway");
                    refit_menu_seen_calls=before;
                    std::array<BYTE,0x40> invalid_shell{};
                    std::array<BYTE,0x40> loaded_shell{};
                    loaded_shell[0x2c]=1;
                    void* shell_bitmap=invalid_shell.data();
                    std::memcpy(loaded_shell.data()+8,&shell_bitmap,sizeof(shell_bitmap));
                    for (std::size_t i=0;i<shell_draw_replacements.size();++i) {
                        exercise_shell_fallback(i,invalid_shell.data(),reinterpret_cast<HDC>(0x1234u));
                        exercise_shell_fallback(i,loaded_shell.data(),nullptr);
                    }
                }
                std::array<BYTE,a2fo::refit_menu::native::form_size> fake_form{};
                static_assert(a2fo::refit_menu::native::handle_offset == 0x1b4);
                static_assert(a2fo::refit_menu::native::parent_window_offset == 0x1c4);
                // ParentWindow is deliberately different from the form HWND.
                const std::uint32_t fake_parent = 0x12345678;
                std::memcpy(fake_form.data()+a2fo::refit_menu::native::parent_window_offset,&fake_parent,sizeof(fake_parent));
                void* sender=reinterpret_cast<void*>(0x12345678u);
                refit_menu_test_call(replacement,fake_form.data(),sender);
                require(refit_menu_seen_self==fake_form.data() && refit_menu_seen_sender==sender && refit_menu_seen_calls==1,"Delphi self/sender original-event forwarding");
                for (const unsigned padding : {0u,4u,8u,12u}) {
                    refit_menu_test_call_unaligned(replacement,fake_form.data(),sender,padding);
                    require(refit_menu_seen_self==fake_form.data() && refit_menu_seen_sender==sender,"four-byte native stack alignment forwarding");
                }
                require(query(&status) && status.shown==0 && status.actions==0 && !status.active,"invalid HWND leaves native presentation");
                // Exercise the real GetHandle body with HandleNeeded isolated,
                // proving own HWND vs ParentWindow without creating a window.
                auto* needed=reinterpret_cast<BYTE*>(fo)+0xcd7f8;
                const BYTE before=*needed, ret=0xc3;
                require(a2fo_rs_windows_patch_bytes(needed,&ret,&before,1),"isolate HandleNeeded");
                const std::uint32_t own_window=0x13579bdf;
                std::memcpy(fake_form.data()+0x1b4,&own_window,sizeof(own_window));
                require(refit_menu_test_call(reinterpret_cast<BYTE*>(fo)+0xcd81c,fake_form.data(),sender)==own_window,
                    "real GetHandle returns own HWND, not ParentWindow");
                isolate_action_helpers();
                const std::array<std::uint16_t,7> dialogs{{2,258,13,25,300,30,0}};
                for (std::size_t i=0;i<dialogs.size();++i) {
                    refit_menu_seen_dialog=0xffff;refit_menu_seen_closed=nullptr;refit_menu_seen_running=0;
                    refit_menu_test_call(reinterpret_cast<BYTE*>(fo)+a2fo::refit_menu::native::action_rvas[i],fake_form.data(),sender);
                    require(refit_menu_seen_dialog==dialogs[i] && refit_menu_seen_closed==fake_form.data(),"real native action dialog/Close-self result");
                    require(refit_menu_seen_running==(i==6?2u:0u),"native exit running-state result");
                }
                if (mode=="success") {
                    // Unknown preview assets pass through without touching either
                    // bitmap object. The original stub receives raw registers.
                    auto unknown=name("MissionSelect/missionpreview.png");
                    std::array<BYTE,0x60> dummy{};
                    exercise_loader(bitmap_replacement,dummy.data(),unknown,0xa1b2c3d4u);
                    exercise_loader(bitmap32_replacement,dummy.data(),unknown,0u); // GR32 PNG false
                    auto known=name("bitmaps/main/mainbkgr.png");
                    std::array<std::uint32_t,4> pixels{{0xff336699u,0x00aabbccu,0xffff00ffu,0x80808080u}};
                    const auto original=pixels;
                    std::array<BYTE,0x60> bitmap32{};
                    const auto vmt=bitmap32_vmt();
                    const int width=2,height=2;
                    void* bits=pixels.data();
                    std::memcpy(bitmap32.data(),&vmt,sizeof(vmt));
                    std::memcpy(bitmap32.data()+a2fo::refit_menu::native::bitmap32_width,&width,sizeof(width));
                    std::memcpy(bitmap32.data()+a2fo::refit_menu::native::bitmap32_height,&height,sizeof(height));
                    std::memcpy(bitmap32.data()+a2fo::refit_menu::native::bitmap32_bits,&bits,sizeof(bits));
                    exercise_loader(bitmap32_replacement,bitmap32.data(),unknown,0u);
                    require(pixels==original,"unknown preview pixels retained");
                    exercise_loader(bitmap32_replacement,bitmap32.data(),known,0u);
                    require(pixels[0]!=original[0] && pixels[1]==original[1] && pixels[2]==original[2] &&
                        (pixels[3]&0xff000000u)==0x80000000u,"GR32 styling preserves alpha and transparent key");
                    pixels=original;
                    const int invalid=0;
                    std::memcpy(bitmap32.data()+a2fo::refit_menu::native::bitmap32_width,&invalid,sizeof(invalid));
                    exercise_loader(bitmap32_replacement,bitmap32.data(),known,0u);
                    require(pixels==original,"invalid GR32 dimensions retain pixels");
                    std::memcpy(bitmap32.data()+a2fo::refit_menu::native::bitmap32_width,&width,sizeof(width));
                    bits=nullptr;
                    std::memcpy(bitmap32.data()+a2fo::refit_menu::native::bitmap32_bits,&bits,sizeof(bits));
                    exercise_loader(bitmap32_replacement,bitmap32.data(),known,0u);
                    require(pixels==original,"null GR32 bits retain pixels");
                    // A decoded buffer must be writable native data. Do not
                    // let WriteProcessMemory relax a read-only page for us.
                    void* readonly=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
                    require(readonly!=nullptr,"read-only fixture allocation");
                    std::memcpy(readonly,original.data(),sizeof(original));
                    DWORD old{};
                    require(VirtualProtect(readonly,4096,PAGE_READONLY,&old)!=0,"read-only fixture protection");
                    std::memcpy(bitmap32.data()+a2fo::refit_menu::native::bitmap32_bits,&readonly,sizeof(readonly));
                    exercise_loader(bitmap32_replacement,bitmap32.data(),known,0u);
                    require(std::memcmp(readonly,original.data(),sizeof(original))==0,"read-only GR32 buffer retained");
                    require(VirtualFree(readonly,0,MEM_RELEASE)!=0,"read-only fixture release");
                    exercise_tbitmap();
                }
                if(mission){
                    require(mission_patches==(mission_signature?0u:1u),"real DLL mission signature admission");
                    if(mission_replacement){
                        const BYTE prologue[]{0x55,0x8b,0xec,0x83,0xec,0x18};
                        if(!mission_fo)require(a2fo_rs_windows_patch_jump(reinterpret_cast<BYTE*>(engine)+0x11d3c0,
                            reinterpret_cast<void*>(&mission_original),prologue,sizeof(prologue)),"mission fixture renderer stub");
                        using Render=void(__fastcall*)(void*,void*);
                        reinterpret_cast<Render>(mission_replacement)(sender,nullptr);
                        require(mission_calls==1&&mission_receiver==sender,"real DLL mission observer ECX forwarding");
                        if(mission_fo){
                            refit_fo_chat_fixture::counter(fo,1);
                            reinterpret_cast<Render>(mission_replacement)(sender,nullptr);
                            require(mission_calls==1,"real DLL preserves native Cleanshot suppression");
                            refit_fo_chat_fixture::counter(fo,0);
                        }
                    }
                }
                shutdown();refit_menu_test_call(replacement,fake_form.data(),sender);
                if(mission_replacement){
                    using Render=void(__fastcall*)(void*,void*);
                    reinterpret_cast<Render>(mission_replacement)(sender,nullptr);
                    require(mission_calls==2,"real DLL resident mission observer forwards after shutdown");
                }
                if(loading_replacement){
                    reinterpret_cast<void(__cdecl*)(const char*)>(loading_status_replacement)("after shutdown");
                    require(loading_status_calls==2,"real DLL status callback forwards after shutdown");
                    using Load=void(__stdcall*)(float,void*,float);
                    reinterpret_cast<Load>(loading_replacement)(-2.5f,reinterpret_cast<void*>(1),.375f);
                    require(loading_calls==2,"real DLL loading callback forwards after shutdown");
                }
                require(refit_menu_seen_calls==6 && query(&status) && !status.active,"resident bridge passes through after shutdown");
                if (mode=="success") {
                    auto known=name("bitmaps/main/mainbkgr.png");
                    std::array<BYTE,0x60> dummy{};
                    exercise_loader(bitmap_replacement,dummy.data(),known,0x87654321u);
                    exercise_loader(bitmap32_replacement,dummy.data(),known,0u);
                }
            } else shutdown();
        }
        require(DeleteFileA(ini.c_str()) || mode=="missing","remove temporary config");
        if (mode=="profile") { FreeLibrary(fo); require(DeleteFileA(input.c_str())!=0,"remove private profile copy"); }
        require(RemoveDirectoryA(root.c_str())!=0,"remove temporary root");
        std::printf("refit menu %s: PASS\n",mode.c_str());return 0;
    } catch(const std::exception& e) { std::fprintf(stderr,"FAIL %s: %s (Win32=%lu)\n%s",mode.c_str(),e.what(),GetLastError(),diagnostic.c_str());return 1; }
}
