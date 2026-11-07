#include "../../sdk/include/a2fo_module_provider.hpp"
// A2FO adaptation of Refit's read-only shell-widget model. MIT attribution:
// third_party/armada2-refit/LICENSE.txt. No stock addresses are used unchecked.
// Re-layout forwards the user's existing messages with translated coordinates;
// it never generates input, changes focus, or invokes a second action.
#include "setup_native.hpp"
#include "loading_native.hpp"
#include "setup_presentation.hpp"
#include "setup_skin.hpp"
#include "menu_scene.hpp"
#include "mission_backdrop.hpp"
#include "../../rust/stage10_presentation_completion_ffi.h"
#include "../../rust/stage8_b_setup_ffi.h"
#include "../../sdk/include/a2fo_shell_canvas.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <vector>

namespace a2fo::refit_menu::setup_native {
namespace {
const A2FO_ModuleApi* api{};
HMODULE engine{};
std::atomic<bool> wanted{false};
bool started{};
DWORD owner_thread{};
std::atomic<void*> next_ctor{};
A2FO_InlineHook ctor_hook{};
A2FO_CopyShellCanvasV1Fn copy_canvas{};
using SetPresenter=BOOL(__cdecl*)(HWND,A2FO_ShellCanvasPresenterV1,void*);
SetPresenter set_presenter{};
A2FO_SetShellCanvasSeedV1Fn set_seed{};
A2FO_SetShellCanvasRefreshV1Fn set_refresh{};
A2FO_ShellDialogBridgeReadyV1Fn bridge_ready{};
UINT restore_message{};
std::mutex observed_mutex;
std::set<HWND> observed;
constexpr UINT_PTR subclass_id = 0xa2f05303;
constexpr std::uint32_t ctor_rva = 0x1a8060;
constexpr std::array<BYTE,21> ctor_bytes{{0x55,0x8b,0xec,0x8b,0xc1,0x8b,0x4d,0x08,
    0x89,0x48,0x04,0xc7,0x00,0xd4,0xb2,0x6b,0x00,0x5d,0xc2,0x04,0x00}};
struct Widget { void* object; HWND window; };
thread_local std::vector<Widget> widgets;
void log(const char* text) { if (api && api->log) api->log("A2FORefitMenus", text); }
BOOL __cdecl seed_canvas(HWND window, HDC logical, int width, int height, void*) {
    struct Context {
        HWND window;HDC logical;int width,height,saved{},drawn{};
        BITMAPINFO info{};std::shared_ptr<const Artwork> frame;
        std::exception_ptr error{};
        Context(HWND handle,HDC surface,int w,int h):window(handle),logical(surface),
            width(w),height(h){}
    } c(window,logical,width,height);
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 500:return wanted.load();
            case 501:return menu_scene::requested();
            case 502:return !mission_backdrop::active();
            case 503:return c.window!=nullptr;
            case 504:return c.logical!=nullptr;
            case 505:return c.width>0;
            case 506:return c.height>0;
            case 507:{char name[32]{};return GetClassNameA(c.window,name,sizeof(name))&&
                !std::strcmp(name,"#32770");}
            case 508:menu_scene::handover_destination(c.window);return 0;
            case 509:c.frame=menu_scene::retained_frame();return 0;
            case 510:return bool(c.frame);
            case 511:return c.frame->width>0;
            case 512:return c.frame->height>0;
            case 513:return static_cast<std::uint64_t>(c.frame->width)*c.frame->height==
                c.frame->pixels.size();
            case 514:c.info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
                c.info.bmiHeader.biWidth=c.frame->width;
                c.info.bmiHeader.biHeight=-c.frame->height;
                c.info.bmiHeader.biPlanes=1;c.info.bmiHeader.biBitCount=32;
                c.info.bmiHeader.biCompression=BI_RGB;
                c.saved=SaveDC(c.logical);return c.saved!=0;
            case 515:SetStretchBltMode(c.logical,COLORONCOLOR);
                c.drawn=StretchDIBits(c.logical,0,0,c.width,c.height,0,0,
                    c.frame->width,c.frame->height,c.frame->pixels.data(),
                    &c.info,DIB_RGB_COLORS,SRCCOPY);return 0;
            case 516:return RestoreDC(c.logical,c.saved)!=FALSE;
            case 517:return c.drawn!=0&&c.drawn!=static_cast<int>(GDI_ERROR);
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_setup_seed(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result?TRUE:FALSE;
}
void* at(std::uint32_t rva) { return reinterpret_cast<BYTE*>(engine)+rva; }
bool read(const void* source, void* dest, std::size_t size) {
    SIZE_T got{};
    return source && ReadProcessMemory(GetCurrentProcess(), source, dest, size, &got) && got == size;
}
template<class T> bool field(const void* object, std::size_t offset, T& value) {
    const auto base = reinterpret_cast<std::uintptr_t>(object);
    return base && offset <= UINTPTR_MAX-base && read(reinterpret_cast<void*>(base+offset), &value, sizeof(value));
}
template<class T> T value(const std::array<BYTE,0x470>& bytes, std::size_t offset) {
    T result{}; std::memcpy(&result, bytes.data()+offset, sizeof(result)); return result;
}
std::string text_at(const BYTE* source, std::size_t capacity) {
    const auto end = static_cast<const BYTE*>(std::memchr(source, 0, capacity));
    if (!end) return {};
    std::string text(reinterpret_cast<const char*>(source), static_cast<std::size_t>(end-source));
    return text == "NULL" ? std::string{} : text;
}
bool bounds(const std::array<BYTE,0x470>& bytes, std::size_t offset, Rect& rect) {
    const auto x=value<int>(bytes,offset), y=value<int>(bytes,offset+4);
    const auto right=value<int>(bytes,offset+8), bottom=value<int>(bytes,offset+12);
    if (x < -1024 || y < -2048 || right > 2048 || bottom > 4096 || right <= x || bottom <= y) return false;
    rect={x,y,right-x,bottom-y}; return true;
}
bool combo_text(HWND combo, int item, std::string& text) {
    const auto length=SendMessageA(combo,CB_GETLBTEXTLEN,item,0);
    if (length < 0 || length > 1023) return false;
    std::vector<char> buffer(static_cast<std::size_t>(length)+1);
    const auto copied=SendMessageA(combo,CB_GETLBTEXT,item,reinterpret_cast<LPARAM>(buffer.data()));
    if (copied != length) return false;
    text.assign(buffer.data(),static_cast<std::size_t>(copied)); return true;
}
bool control(const Widget& widget, setup::Control& out) {
    struct Context {
        const Widget& widget;setup::Control& out;std::uintptr_t vt{};
        std::array<std::uintptr_t,9> methods{};std::array<BYTE,0x470> bytes{};
        bool label{},combo{};HWND child{};char name[32]{};int next_item{};
        LRESULT count{};std::exception_ptr error{};
        Context(const Widget& value,setup::Control& result):widget(value),out(result){}
    } c(widget,out);
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 800:return field(c.widget.object,0,c.vt);
            case 801:return c.vt;
            case 802:return reinterpret_cast<std::uintptr_t>(at(0x2ae000));
            case 803:return reinterpret_cast<std::uintptr_t>(at(0x2ec000));
            case 804:return reinterpret_cast<std::uintptr_t>(at(0x2bb2d4));
            case 805:return read(reinterpret_cast<void*>(c.vt),c.methods.data(),sizeof(c.methods));
            case 806:c.combo=c.vt==reinterpret_cast<std::uintptr_t>(at(0x2bad2c));return c.combo;
            case 807:return c.methods[8]==reinterpret_cast<std::uintptr_t>(at(0x1a4af0));
            case 808:c.label=c.methods[1]==reinterpret_cast<std::uintptr_t>(at(0x1a7590));return c.label;
            case 809:return read(c.widget.object,c.bytes.data(),c.label?c.bytes.size():0x1b8);
            case 810:c.out.key=reinterpret_cast<std::uintptr_t>(c.widget.object);return 0;
            case 811:return value<HWND>(c.bytes,4)==c.widget.window;
            case 812:return bounds(c.bytes,0x408,c.out.source);
            case 813:c.out.kind=setup::Kind::label;
                c.out.visible=value<std::uint32_t>(c.bytes,0x45c)!=0;
                c.out.text=text_at(c.bytes.data()+8,1024);return 0;
            case 814:return c.vt==reinterpret_cast<std::uintptr_t>(at(0x2bbaa4));
            case 815:{const auto pointer=value<const char*>(c.bytes,0x46c);
                std::uint32_t length{};
                if(field(c.widget.object,0x470,length)&&length<4096&&pointer) {
                    std::string text(length,'\0');
                    if(read(pointer,text.data(),length)&&text.find('\0')==std::string::npos)
                        c.out.text=std::move(text);
                }return 0;}
            case 816:return value<HWND>(c.bytes,0x20)==c.widget.window;
            case 817:{const auto rectangle=c.vt==reinterpret_cast<std::uintptr_t>(at(0x2bb0e4))?
                    0x178u:0xc4u;return bounds(c.bytes,rectangle,c.out.source);}
            case 818:c.out.text=text_at(c.bytes.data()+0x3c,64);
                c.out.visible=value<std::uint32_t>(c.bytes,0xe4)!=0;
                c.out.enabled=value<std::uint32_t>(c.bytes,0xe8)!=0;
                c.out.mode=static_cast<int>(value<std::uint32_t>(c.bytes,0xc0));
                c.out.kind=c.out.text.empty()?setup::Kind::picture:setup::Kind::button;return 0;
            case 819:c.out.kind=setup::Kind::combo;
                c.child=value<HWND>(c.bytes,0x198);return 0;
            case 820:return IsWindow(c.child)!=FALSE;
            case 821:return GetParent(c.child)==c.widget.window;
            case 822:return GetClassNameA(c.child,c.name,sizeof(c.name))!=0;
            case 823:return lstrcmpiA(c.name,"COMBOBOX")==0;
            case 824:c.count=SendMessageA(c.child,CB_GETCOUNT,0,0);
                return static_cast<std::uint32_t>(c.count);
            case 825:c.out.selected=static_cast<int>(SendMessageA(c.child,CB_GETCURSEL,0,0));
                return static_cast<std::uint32_t>(c.out.selected);
            case 826:return combo_text(c.child,c.out.selected,c.out.text);
            case 827:c.out.open=value<BYTE>(c.bytes,0x190)!=0;
                c.out.down=value<BYTE>(c.bytes,0x1b4)!=0;
                c.out.item_height=value<int>(c.bytes,0x17c);return 0;
            case 828:return c.out.open;
            case 829:return static_cast<std::uint32_t>(c.out.item_height);
            case 830:return bounds(c.bytes,0x180,c.out.pull);
            case 831:{std::string text;
                if(!combo_text(c.child,c.next_item,text))return 0;
                c.out.items.push_back(std::move(text));++c.next_item;return 1;}
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_setup_control(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
struct Dib {
    HDC dc{}; HBITMAP bitmap{}; HGDIOBJ old{}; std::uint32_t* bits{}; int width{},height{};
    ~Dib() { reset(); }
    void reset() { if(dc&&old)SelectObject(dc,old); if(bitmap)DeleteObject(bitmap); if(dc)DeleteDC(dc); dc=nullptr;bitmap=nullptr;old=nullptr;bits=nullptr; }
    bool get(int w,int h) {
        if(dc&&w==width&&h==height)return true;
        reset(); if(w<1||h<1||static_cast<std::uint64_t>(w)*h>8'000'000)return false;
        dc=CreateCompatibleDC(nullptr); if(!dc)return false;
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;
        info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        void* data{};bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&data,nullptr,0);
        if(!bitmap||!data)return false;
        old=SelectObject(dc,bitmap);bits=static_cast<std::uint32_t*>(data);
        width=w;height=h;return old&&old!=HGDI_ERROR;
    }
};
struct Session {
    HWND window{}; Artwork fallback,native; Dib source,output;
    setup::Model model;setup::Layout geometry;
    struct Log { HWND window{};Rect source;bool hidden_by_us{}; };
    std::vector<Log> logs;
    int x=-1,y=-1,scroll{};std::uintptr_t opened{};bool painting{},active{},reported{};unsigned diagnosed{};
    unsigned presents{},timing_reports{};
    bool coalescing_hover{};
    setup::LaunchPress launch_pressed;
    std::shared_ptr<const Artwork> pending_launch_ui;
    bool launch_dispatch{};
    HWND parent{};std::uintptr_t identity{};ULONGLONG closed_at{};
    int canvas_width{},canvas_height{},widget_width{},widget_height{};
};
thread_local std::map<HWND,std::shared_ptr<Session>> sessions;
constexpr char retained_property[]="A2FORefitMenus.AcceptedLoading.v1";
thread_local std::shared_ptr<Session> retained_setup;
thread_local std::uintptr_t setup_generation{};
void forget_retained_setup(){
    if(!retained_setup)return;
    const std::initializer_list<HWND> windows{retained_setup->window,retained_setup->parent};
    a2fo_stage10_presentation_each(windows.begin(),windows.end(),[](const auto window){
        DWORD process{};
        if(window&&GetWindowThreadProcessId(window,&process)==GetCurrentThreadId()&&process==GetCurrentProcessId()&&
           reinterpret_cast<std::uintptr_t>(GetPropA(window,retained_property))==retained_setup->identity)
            RemovePropA(window,retained_property);
    });
    retained_setup.reset();
}
bool erase_retained(Session& s,HDC dc){
    struct Context {
        Session& session;HDC dc;RECT rect{};int saved{},drawn{};
        BITMAPINFO info{};std::shared_ptr<const Artwork> image;
        std::exception_ptr error{};
        Context(Session& value,HDC surface):session(value),dc(surface){}
    } c(s,dc);
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 520:return c.dc!=nullptr;
            case 521:return mission_backdrop::active();
            case 522:return GetClientRect(c.session.window,&c.rect)&&
                c.rect.right>0&&c.rect.bottom>0;
            case 523:return menu_scene::handover_active_for_erase();
            case 524:return c.session.active;
            case 525:return c.session.output.dc!=nullptr;
            case 526:return c.session.output.width==c.rect.right&&
                c.session.output.height==c.rect.bottom;
            case 527:return BitBlt(c.dc,0,0,c.rect.right,c.rect.bottom,
                c.session.output.dc,0,0,SRCCOPY)!=FALSE;
            case 528:return menu_scene::requested();
            case 529:c.image=menu_scene::retained_frame();return 0;
            case 530:return c.image&&c.image->width>0&&c.image->height>0&&
                c.image->pixels.size()==std::size_t(c.image->width)*c.image->height;
            case 531:c.info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
                c.info.bmiHeader.biWidth=c.image->width;
                c.info.bmiHeader.biHeight=-c.image->height;
                c.info.bmiHeader.biPlanes=1;c.info.bmiHeader.biBitCount=32;
                c.info.bmiHeader.biCompression=BI_RGB;
                c.saved=SaveDC(c.dc);return c.saved!=0;
            case 532:SetStretchBltMode(c.dc,COLORONCOLOR);
                c.drawn=StretchDIBits(c.dc,0,0,c.rect.right,c.rect.bottom,0,0,
                    c.image->width,c.image->height,c.image->pixels.data(),
                    &c.info,DIB_RGB_COLORS,SRCCOPY);return 0;
            case 533:return RestoreDC(c.dc,c.saved)!=FALSE;
            case 534:return c.drawn!=0&&c.drawn!=static_cast<int>(GDI_ERROR);
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_setup_erase(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
struct LaunchDispatch{
    Session& session;bool owns;
    LaunchDispatch(Session& s,bool release):session(s),owns(release&&bool(s.pending_launch_ui)&&!s.launch_dispatch){
        if(owns)session.launch_dispatch=true;
    }
    ~LaunchDispatch(){if(owns){session.launch_dispatch=false;session.pending_launch_ui.reset();}}
};
std::shared_ptr<const Artwork> take_launch_overlay(Session& s,bool closing){
    return closing&&s.launch_dispatch?std::move(s.pending_launch_ui):nullptr;
}
thread_local std::map<HWND,unsigned> admission_reports;
void admission(HWND window,unsigned reason,const char* detail){
    struct Context {HWND window;unsigned reason;const char* detail;
        std::exception_ptr error{};} c{window,reason,detail,{}};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 600:return admission_reports.size();
            case 601:return admission_reports.count(c.window);
            case 602:return admission_reports[c.window];
            case 603:admission_reports[c.window]|=c.reason;return 0;
            case 604:{char name[48]{};GetClassNameA(c.window,name,sizeof(name));
                char message[256]{};std::snprintf(message,sizeof(message),
                    "Instant Action admission: %s; HWND=%p class=%s",
                    c.detail,reinterpret_cast<void*>(c.window),name);log(message);return 0;}
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_setup_admission(&c,op,reason);
    if(c.error)std::rethrow_exception(c.error);
}
void restore_logs(Session& s) {
    struct Context {Session& session;HWND child{};std::exception_ptr error{};
        explicit Context(Session& value):session(value){}} c(s);
    const auto op=[](void* opaque,std::uint32_t id,std::uintptr_t index) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 300:return c.session.logs.size();
            case 301:c.child=c.session.logs[index].window;
                return c.session.logs[index].hidden_by_us;
            case 302:c.session.logs[index].hidden_by_us=false;return 0;
            case 303:return IsWindow(c.child)!=FALSE;
            case 304:return GetParent(c.child)==c.session.window;
            case 305:return SetWindowPos(c.child,nullptr,0,0,0,0,
                SWP_SHOWWINDOW|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE;
            case 306:return IsWindow(c.child)!=FALSE;
            case 307:return GetParent(c.child)==c.session.window;
            case 308:{const auto pending=a2fo_stage10_presentation_find(c.session.logs.begin(),
                c.session.logs.end(),[&](const auto& item){return item.window==c.child;});
                if(pending!=c.session.logs.end()){pending->hidden_by_us=true;}
                return 0;}
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_setup_restore_logs(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
struct ChildScan { Session* session;setup::Model* model;bool editable{};std::string reason; };
BOOL CALLBACK child_control(HWND child,LPARAM parameter) {
    auto& scan=*reinterpret_cast<ChildScan*>(parameter);
    try {
        auto& s=*scan.session;
        if(GetParent(child)!=s.window)return TRUE;
        auto existing=a2fo_stage10_presentation_find(s.logs.begin(),s.logs.end(),[child](const auto& item){return item.window==child;});
        if(!IsWindowVisible(child)&&(existing==s.logs.end()||!existing->hidden_by_us))return TRUE;
        char name[32]{};
        if(!GetClassNameA(child,name,sizeof(name)))return TRUE;
        const auto style=GetWindowLongPtrA(child,GWL_STYLE);
        const int id=GetDlgCtrlID(child);
        const auto reject=[&](const char* why){
            scan.editable=true;
            char message[192]{};std::snprintf(message,sizeof(message),"%s: class=%s id=%d style=0x%lx",why,name,id,static_cast<unsigned long>(style));
            scan.reason=message;return TRUE;
        };
        // Pinned resource 2096 and GameSetup init identify these as the two
        // read-only multiline status panes. IA keeps the right pane (2120).
        if(lstrcmpiA(name,"EDIT")||!(style&ES_READONLY)||!(style&ES_MULTILINE)||
           (id!=2093&&id!=2120))return reject("unsupported interactive/unknown child");
        RECT bounds{},client{};
        if(!GetWindowRect(child,&bounds)||!GetClientRect(s.window,&client)||client.right<1||client.bottom<1)
            return reject("unavailable child bounds");
        POINT point{bounds.left,bounds.top};
        if(!ScreenToClient(s.window,&point))return reject("unavailable child origin");
        Rect source{MulDiv(point.x,s.canvas_width,client.right),MulDiv(point.y,s.canvas_height,client.bottom),
            MulDiv(bounds.right-bounds.left,s.canvas_width,client.right),MulDiv(bounds.bottom-bounds.top,s.canvas_height,client.bottom)};
        if(source.x<0||source.y<0||source.width<1||source.height<1||
           source.x>s.canvas_width-source.width||source.y>s.canvas_height-source.height)
            return reject("child outside logical canvas");
        if(existing==s.logs.end()){
            s.logs.push_back({child,source,false});existing=std::prev(s.logs.end());
        }else existing->source=source;
        std::array<char,16384> text{};GetWindowTextA(child,text.data(),text.size());
        setup::Control label;label.key=reinterpret_cast<std::uintptr_t>(child)|1u;label.kind=setup::Kind::label;
        label.message=true;label.source=source;label.text=text.data();scan.model->controls.push_back(std::move(label));
        return TRUE;
    }catch(...){scan.editable=true;return FALSE;}
}
bool snapshot(Session& s) {
    struct Context {
        Session& session;int w{},h{};std::optional<setup::Model> model;std::string reason;
        bool editable{};std::uintptr_t key{};RECT rect{};
        std::exception_ptr error{};
        explicit Context(Session& value):session(value){}
    } c(s);
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        auto& s=c.session;
        try{
            switch(id){
            case 400:return bridge_ready&&bridge_ready(s.window);
            case 401:return copy_canvas(s.window,nullptr,0,0,&c.w,&c.h);
            case 402:return (c.w==800&&c.h==600)||(c.w==1200&&c.h==900);
            case 403:return s.source.get(c.w,c.h);
            case 404:return copy_canvas(s.window,s.source.dc,c.w,c.h,&c.w,&c.h);
            case 405:s.canvas_width=c.w;s.canvas_height=c.h;return 0;
            case 406:c.model.emplace();
                a2fo_stage10_presentation_walk(widgets.begin(),widgets.end(),
                    [&](const auto& widget){return widget.window==s.window;},
                    [&](const auto& widget){setup::Control item;
                        if(control(widget,item))c.model->controls.push_back(std::move(item));},3);return 0;
            case 407:{ChildScan children{&s,&*c.model,false,{}};
                EnumChildWindows(s.window,&child_control,reinterpret_cast<LPARAM>(&children));
                c.editable=children.editable;c.reason=std::move(children.reason);return 0;}
            case 408:return c.editable;
            case 409:if(!(s.diagnosed&1u)){
                    const auto detail=std::string("Instant Action layout passive: ")+c.reason;
                    log(detail.c_str());s.diagnosed|=1u;
                }return 0;
            case 410:{const auto opened=a2fo_stage10_presentation_find(c.model->controls.begin(),c.model->controls.end(),
                    [](const auto& item){return item.visible&&item.open;});
                c.key=opened==c.model->controls.end()?0:opened->key;return c.key;}
            case 411:return s.opened;
            case 412:s.scroll=0;s.opened=c.key;return 0;
            case 413:return GetClientRect(s.window,&c.rect)!=FALSE;
            case 414:{auto layout=setup::arrange_native(c.rect.right,c.rect.bottom,
                    c.w,c.h,*c.model,s.scroll);
                s.model=std::move(layout.model);s.geometry=std::move(layout.geometry);
                s.widget_width=layout.widget_width;s.widget_height=layout.widget_height;return 0;}
            case 415:return s.geometry.valid;
            case 416:s.native.width=s.widget_width;s.native.height=s.widget_height;
                s.native.pixels.resize(static_cast<std::size_t>(s.native.width)*s.native.height);
                for(int y=0;y<s.native.height;++y){
                    std::copy_n(s.source.bits+static_cast<std::size_t>(y)*c.w,s.native.width,
                        s.native.pixels.data()+static_cast<std::size_t>(y)*s.native.width);
                }
                loading_native::remember_setup_preview(
                    setup::loading_preview_snapshot(s.native,s.geometry));return 0;
            case 417:return !(s.diagnosed&2u);
            case 418:{const auto combos=a2fo_stage10_presentation_count(s.model.controls.begin(),s.model.controls.end(),
                    [](const auto& item){return item.visible&&item.kind==setup::Kind::combo;});
                char message[160]{};std::snprintf(message,sizeof(message),
                    "Instant Action layout passive: incomplete native model (%u controls, %u visible combos)",
                    static_cast<unsigned>(s.model.controls.size()),static_cast<unsigned>(combos));
                log(message);s.diagnosed|=2u;return 0;}
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_setup_snapshot(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
bool hide_logs(Session& s) {
    struct Context {Session& session;HWND child{};std::exception_ptr error{};
        explicit Context(Session& value):session(value){}} c(s);
    const auto op=[](void* opaque,std::uint32_t id,std::uintptr_t index) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 310:return c.session.logs.size();
            case 311:c.child=c.session.logs[index].window;return IsWindowVisible(c.child)!=FALSE;
            case 312:c.session.logs[index].hidden_by_us=true;return 0;
            case 313:return SetWindowPos(c.child,nullptr,0,0,0,0,
                SWP_HIDEWINDOW|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE;
            case 314:return c.session.logs[index].hidden_by_us;
            case 315:c.session.logs[index].hidden_by_us=true;return 0;
            case 316:restore_logs(c.session);return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_setup_hide_logs(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
bool present(Session& s) {
    struct Done {bool& value;bool completed{};
        explicit Done(bool& painting):value(painting){}
        ~Done(){value=false;if(!completed)menu_scene::end_handover();}};
    struct Context {
        Session& s;DWORD began{},captured{},composed{},painted{},finished{};
        RECT rect{};const Artwork* live{};HDC dc{};bool okay{};
        std::optional<Done> done;
        std::optional<setup_skin::DirectTargetScope> direct_target;
        std::exception_ptr error{};
        explicit Context(Session& session):s(session){}
    } c(s);
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 160:return c.s.painting;
            case 161:return menu_scene::launch_handover_active();
            case 162:return wanted.load();
            case 163:return IsWindowEnabled(c.s.window)!=FALSE;
            case 164:return IsWindowVisible(c.s.window)!=FALSE;
            case 165:c.s.active=false;restore_logs(c.s);return 0;
            case 166:c.s.painting=true;return 0;
            case 167:c.began=GetTickCount();return 0;
            case 168:c.done.emplace(c.s.painting);return 0;
            case 169:return snapshot(c.s);
            case 170:c.s.active=false;restore_logs(c.s);return 0;
            case 171:c.captured=GetTickCount();return 0;
            case 172:return GetClientRect(c.s.window,&c.rect)&&
                c.s.output.get(c.rect.right,c.rect.bottom);
            case 173:menu_scene::end_handover();return 0;
            case 174:c.live=menu_scene::frame(c.s.window,true,false,true);return 0;
            case 175:c.composed=GetTickCount();return 0;
            case 176:c.direct_target.emplace(c.s.output.dc,c.s.output.width,
                c.s.output.height,c.s.output.bits);return 0;
            case 177:return setup::paint(c.s.output.dc,c.rect.right,c.rect.bottom,
                c.s.model,c.s.geometry,c.live?c.live:&c.s.fallback,&c.s.native,c.s.x,c.s.y);
            case 178:c.painted=GetTickCount();return 0;
            case 179:return hide_logs(c.s);
            case 180:c.s.active=false;return 0;
            case 181:c.dc=GetDC(c.s.window);return c.dc!=nullptr;
            case 182:c.okay=BitBlt(c.dc,0,0,c.rect.right,c.rect.bottom,
                c.s.output.dc,0,0,SRCCOPY)!=0;return c.okay;
            case 183:menu_scene::frame_drawn(c.live,c.dc);return 0;
            case 184:ReleaseDC(c.s.window,c.dc);c.s.active=c.okay;return 0;
            case 185:return c.s.identity!=0;
            case 186:{const auto found=sessions.find(c.s.window);
                if(found!=sessions.end()&&found->second.get()==&c.s){
                    retained_setup=found->second;
                }
                return 0;}
            case 187:++c.s.presents;return 0;
            case 188:restore_logs(c.s);return 0;
            case 189:c.finished=GetTickCount();return 0;
            case 190:return c.s.timing_reports<3;
            case 191:return c.finished-c.began>=50&&c.s.timing_reports<12;
            case 192:{char message[240]{};std::snprintf(message,sizeof(message),
                "Instant Action present %lu ms: snapshot=%lu scene=%lu paint=%lu blit=%lu ms",
                static_cast<unsigned long>(c.finished-c.began),
                static_cast<unsigned long>(c.captured-c.began),
                static_cast<unsigned long>(c.composed-c.captured),
                static_cast<unsigned long>(c.painted-c.composed),
                static_cast<unsigned long>(c.finished-c.painted));
                log(message);++c.s.timing_reports;return 0;}
            case 193:return !c.s.reported;
            case 194:{char message[192]{};std::snprintf(message,sizeof(message),
                "Instant Action Refit layout active; native widgets/actions retained; widget space=%dx%d, canvas=%dx%d",
                c.s.widget_width,c.s.widget_height,c.s.canvas_width,c.s.canvas_height);
                log(message);c.s.reported=true;return 0;}
            case 195:c.done->completed=c.okay;return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_setup_present(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
BOOL __cdecl presenter(HWND window,HDC,void*) {
    const auto found=sessions.find(window);
    if(found==sessions.end())return FALSE;
    const auto session=found->second; // Keep alive across synchronous USER32 destruction.
    // Native hover updates can complete several canvas leases inside one real
    // mouse move. Keep the already presented UI until its coalesced paint,
    // rather than rebuilding it once per lease as well as after dispatch.
    if(session->coalescing_hover&&session->active){
        InvalidateRect(window,nullptr,FALSE);return TRUE;
    }
    try{return present(*session)?TRUE:FALSE;}catch(...){menu_scene::end_handover();session->active=false;restore_logs(*session);return FALSE;}
}
bool pointer_message(UINT message) {return message==WM_MOUSEMOVE||message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||message==WM_LBUTTONDBLCLK||message==WM_RBUTTONDOWN||message==WM_RBUTTONUP;}
LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR) {
    const auto found=sessions.find(window);
    if(found==sessions.end())return DefSubclassProc(window,message,wp,lp);
    auto s=found->second;
    if(message==WM_SHOWWINDOW&&wp)s->closed_at=0;
    if((message==WM_SHOWWINDOW&&!wp)||message==WM_DESTROY||message==WM_NCDESTROY)
        if(!s->closed_at)s->closed_at=GetTickCount64();
    if(message==WM_NCDESTROY){
        {const std::lock_guard<std::mutex> lock(observed_mutex);observed.erase(window);}
        admission_reports.erase(window);sessions.erase(found);RemoveWindowSubclass(window,&window_proc,subclass_id);return DefSubclassProc(window,message,wp,lp);
    }
    if(message==restore_message){restore_logs(*s);s->active=false;InvalidateRect(window,nullptr,TRUE);return 0;}
    if(!wanted)return DefSubclassProc(window,message,wp,lp);
    if(message==WM_ERASEBKGND)try{
        if(erase_retained(*s,reinterpret_cast<HDC>(wp)))return 1;
    }catch(...){} // Preserve native erase handling on presentation failure.
    const LPARAM original=lp;
    bool forwarded=false;
    LRESULT result{};
    const auto presents=s->presents;
    struct Context {
        HWND window;UINT message;WPARAM wp;LPARAM& lp;
        Session& session;
        bool& forwarded;LRESULT& result;unsigned presents;
        std::uintptr_t launch{};
        std::shared_ptr<const Artwork> overlay;
        std::exception_ptr error{};
        Context(HWND handle,UINT event,WPARAM wparam,LPARAM& parameter,
                Session& active,bool& sent,LRESULT& answer,unsigned count)
            :window(handle),message(event),wp(wparam),lp(parameter),
             session(active),forwarded(sent),result(answer),presents(count){}
    } c(window,message,wp,lp,*s,forwarded,result,presents);
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        auto& s=c.session;
        try{
            switch(id){
            case 200:return c.message==WM_CANCELMODE||c.message==WM_CAPTURECHANGED||
                c.message==WM_KILLFOCUS;
            case 201:s.launch_pressed.cancel();if(!s.launch_dispatch)s.pending_launch_ui.reset();return 0;
            case 202:return c.message==WM_KEYDOWN||c.message==WM_NCLBUTTONDOWN;
            case 203:s.launch_pressed.cancel();s.pending_launch_ui.reset();return 0;
            case 204:c.overlay=take_launch_overlay(s,(c.message==WM_SHOWWINDOW&&!c.wp)||
                c.message==WM_DESTROY);return bool(c.overlay);
            case 205:menu_scene::begin_launch_handover(c.window,GetParent(c.window),
                std::move(c.overlay));return 0;
            case 206:return menu_scene::launch_handover_active();
            case 207:return c.message==WM_PAINT;
            case 208:{PAINTSTRUCT paint{};if(BeginPaint(c.window,&paint))EndPaint(c.window,&paint);
                return 0;}
            case 209:return s.active;
            case 210:return bridge_ready&&bridge_ready(c.window);
            case 211:s.active=false;restore_logs(s);return 0;
            case 212:return s.active;
            case 213:return c.message==WM_MOUSEWHEEL;
            case 214:return s.opened!=0;
            case 215:s.scroll=std::max(0,s.scroll+(GET_WHEEL_DELTA_WPARAM(c.wp)>0?-1:1));
                present(s);return 0;
            case 216:return s.active;
            case 217:return pointer_message(c.message);
            case 218:s.x=GET_X_LPARAM(c.lp);s.y=GET_Y_LPARAM(c.lp);return 0;
            case 219:c.launch=setup::launch_at(s.model,s.geometry,s.x,s.y);return 0;
            case 220:return !s.launch_dispatch&&c.message==WM_LBUTTONDOWN;
            case 221:s.pending_launch_ui.reset();s.launch_pressed.key=c.launch;return 0;
            case 222:return !s.launch_dispatch&&c.message==WM_LBUTTONUP;
            case 223:return s.launch_pressed.release(c.launch);
            case 224:return s.output.bits&&s.output.width>0&&s.output.height>0;
            case 225:{auto image=std::make_shared<Artwork>();image->width=s.output.width;
                image->height=s.output.height;
                if(GdiFlush()){
                    image->pixels.assign(s.output.bits,s.output.bits+
                        std::size_t(image->width)*image->height);
                    s.pending_launch_ui=std::move(image);
                }return 0;}
            case 226:{int x=-300,y=-300;setup::map_point(s.geometry,s.x,s.y,x,y);
                RECT physical{};GetClientRect(c.window,&physical);
                c.lp=MAKELPARAM(static_cast<short>(MulDiv(MulDiv(x,s.widget_width,800),
                    physical.right,s.canvas_width)),
                    static_cast<short>(MulDiv(MulDiv(y,s.widget_height,600),
                    physical.bottom,s.canvas_height)));return 0;}
            case 227:c.forwarded=true;return 0;
            case 228:{struct HoverDispatch{
                    Session& session;bool previous;
                    HoverDispatch(Session& active,bool hover):session(active),
                        previous(active.coalescing_hover){active.coalescing_hover=previous||hover;}
                    ~HoverDispatch(){session.coalescing_hover=previous;}
                } hover{s,c.message==WM_MOUSEMOVE};
                LaunchDispatch dispatch{s,c.message==WM_LBUTTONUP};
                c.result=DefSubclassProc(c.window,c.message,c.wp,c.lp);return 0;}
            case 229:return IsWindow(c.window)&&s.presents==c.presents;
            case 230:return c.message==WM_MOUSEMOVE;
            case 231:InvalidateRect(c.window,nullptr,FALSE);return 0;
            case 232:return c.message==WM_PAINT||pointer_message(c.message)||
                c.message==WM_KEYUP||c.message==WM_CHAR||c.message==WM_COMMAND;
            case 233:present(s);return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    try {
        const bool handled=a2fo_rs_stage8_b_setup_window_active(&c,op)!=0;
        if(c.error)std::rethrow_exception(c.error);
        if(handled)return 0;
        return result;
    } catch(...) {
        s->active=false;
        restore_logs(*s);
        log("Instant Action presentation unavailable; native setup retained");
        // Never deliver a real action twice if presentation fails after it.
        if(forwarded)return result;
        return DefSubclassProc(window,message,wp,original);
    }
}
void* __fastcall __attribute__((force_align_arg_pointer)) widget_ctor(void* self,void*,HWND window) {
    void* gateway{};while(!(gateway=next_ctor.load(std::memory_order_acquire)))Sleep(0);
    using Original=void*(__fastcall*)(void*,void*,HWND);
    const auto result=reinterpret_cast<Original>(gateway)(self,nullptr,window);
    struct Context {void* object;HWND window;std::exception_ptr error{};} c{self,window,{}};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 700:return GetCurrentThreadId()==owner_thread;
            case 701:return wanted.load();
            case 702:widgets.erase(a2fo_stage10_presentation_remove(widgets.begin(),widgets.end(),
                [&](const auto& w){return w.object==c.object||!IsWindow(w.window);}),
                widgets.end());return 0;
            case 703:return widgets.size();
            case 704:widgets.push_back({c.object,c.window});return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_setup_widget(&c,op);
    // Native widget capture remains best effort, including allocation failures.
    return result;
}
}
void configure(const A2FO_ModuleApi* supplied,bool enabled){api=supplied;wanted=enabled;}
struct StartContext {
    HMODULE shell{};
    FARPROC address{},register_address{},bridge_address{},seed_address{},refresh_address{};
    std::exception_ptr error;
};
std::uint64_t start_op(void* opaque,std::uint32_t id) noexcept {
    auto& c=*static_cast<StartContext*>(opaque);
    try{
        switch(id){
        case 0:return !started&&wanted&&api;
        case 1:started=true;return 0;
        case 2:engine=static_cast<HMODULE>(api->armada_module());return 0;
        case 3:return a2fo::supported_armada::identify(engine)==a2fo::supported_armada::Identity::unsupported;
        case 4:c.shell=a2fo::startup_module_provider("A2FOShellDisplay.dll", {"A2FO_CopyShellCanvasV1", "A2FO_SetShellCanvasPresenterV1", "A2FO_ShellDialogBridgeReadyV1", "A2FO_SetShellCanvasSeedV1", "A2FO_SetShellCanvasRefreshV1"});return 0;
        case 5:c.address=c.shell?GetProcAddress(c.shell,"A2FO_CopyShellCanvasV1"):nullptr;return c.address!=nullptr;
        case 6:c.register_address=c.shell?GetProcAddress(c.shell,"A2FO_SetShellCanvasPresenterV1"):nullptr;return c.register_address!=nullptr;
        case 7:c.bridge_address=c.shell?GetProcAddress(c.shell,"A2FO_ShellDialogBridgeReadyV1"):nullptr;return c.bridge_address!=nullptr;
        case 8:log("Instant Action layout passive: compatible ShellDisplay canvas export unavailable");return 0;
        case 9:static_assert(sizeof(c.address)==sizeof(copy_canvas));std::memcpy(&copy_canvas,&c.address,sizeof(copy_canvas));
            static_assert(sizeof(c.register_address)==sizeof(set_presenter));std::memcpy(&set_presenter,&c.register_address,sizeof(set_presenter));
            static_assert(sizeof(c.bridge_address)==sizeof(bridge_ready));std::memcpy(&bridge_ready,&c.bridge_address,sizeof(bridge_ready));return 0;
        case 10:c.seed_address=GetProcAddress(c.shell,"A2FO_SetShellCanvasSeedV1");return c.seed_address!=nullptr;
        case 11:static_assert(sizeof(c.seed_address)==sizeof(set_seed));std::memcpy(&set_seed,&c.seed_address,sizeof(set_seed));
            owner_thread=GetCurrentThreadId();return 0;
        case 12:return set_seed(&seed_canvas,nullptr);
        case 13:set_seed=nullptr;log("Instant Action early canvas seed registration rejected");return 0;
        case 14:return set_seed!=nullptr;
        case 15:c.refresh_address=GetProcAddress(c.shell,"A2FO_SetShellCanvasRefreshV1");return c.refresh_address!=nullptr;
        case 16:static_assert(sizeof(c.refresh_address)==sizeof(set_refresh));std::memcpy(&set_refresh,&c.refresh_address,sizeof(set_refresh));return 0;
        case 17:return set_refresh(&seed_canvas,nullptr);
        case 18:log("Instant Action scene refresh registered for native show/resize");return 0;
        case 19:set_refresh=nullptr;log("Instant Action scene refresh registration rejected; seed cache retained");return 0;
        case 20:restore_message=RegisterWindowMessageA("A2FORefitMenus.SetupRestore.v1");return restore_message;
        case 21:{std::array<BYTE,21> bytes{};return read(at(ctor_rva),bytes.data(),bytes.size())&&bytes==ctor_bytes;}
        case 22:log("Instant Action layout passive: widget constructor differs");return 0;
        case 23:owner_thread=GetCurrentThreadId();return 0;
        case 24:return api->install_inline_hook(at(ctor_rva),reinterpret_cast<void*>(&widget_ctor),5,ctor_bytes.data(),&ctor_hook);
        case 25:log("Instant Action layout passive: checked widget observer could not be installed");return 0;
        case 26:next_ctor.store(ctor_hook.gateway,std::memory_order_release);return 0;
        case 27:log("Instant Action widget observer armed; no widget fields or game rules modified");return 0;
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
void start() {
    StartContext c{};
    a2fo_rs_stage8_b_setup_start(&c,start_op);
    if(c.error)std::rethrow_exception(c.error);
}
bool asset(std::string_view path) {
    struct AssetContext {std::string normalized;std::exception_ptr error;};
    AssetContext context{};
    const auto append=[](void* opaque,std::uint8_t value) noexcept -> std::uint8_t {
        auto& c=*static_cast<AssetContext*>(opaque);
        try{c.normalized.push_back(static_cast<char>(value));return 1;}
        catch(...){c.error=std::current_exception();return 0;}
    };
    const bool result=a2fo_rs_stage8_b_setup_asset(
        reinterpret_cast<const std::uint8_t*>(path.data()),path.size(),&context,append)!=0;
    if(context.error)std::rethrow_exception(context.error);
    return result;
}
void observe(HWND window,const Artwork& fallback) {
    struct Context {
        HWND window;const Artwork& fallback;int width{},height{};
        std::shared_ptr<Session> session;
        std::exception_ptr error{};
        Context(HWND handle,const Artwork& image):window(handle),fallback(image){}
    } c(window,fallback);
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 100:return wanted;
            case 101:return sessions.count(c.window);
            case 102:return GetCurrentThreadId()==owner_thread;
            case 103:admission(c.window,2,"not owning observer thread");return 0;
            case 104:{char name[32]{};return GetClassNameA(c.window,name,sizeof(name))&&
                !std::strcmp(name,"#32770");}
            case 105:admission(c.window,4,"expected native game dialog");return 0;
            case 106:forget_retained_setup();return 0;
            case 107:return copy_canvas&&next_ctor.load();
            case 108:admission(c.window,1,"observer/API not ready");return 0;
            case 109:return bridge_ready&&bridge_ready(c.window);
            case 110:admission(c.window,8,"verified dialog bridge unavailable");return 0;
            case 111:return copy_canvas(c.window,nullptr,0,0,&c.width,&c.height);
            case 112:admission(c.window,16,"logical canvas not ready");return 0;
            case 113:return (c.width==800&&c.height==600)||(c.width==1200&&c.height==900);
            case 114:{char message[96]{};std::snprintf(message,sizeof(message),
                "unsupported logical canvas %dx%d",c.width,c.height);
                admission(c.window,32,message);return 0;}
            case 115:c.session=std::make_shared<Session>();c.session->window=c.window;
                c.session->fallback=c.fallback;return 0;
            case 116:return SetWindowSubclass(c.window,&window_proc,subclass_id,0)!=FALSE;
            case 117:admission(c.window,64,"subclass installation failed");return 0;
            case 118:sessions.emplace(c.window,std::move(c.session));return 0;
            case 119:return set_presenter(c.window,&presenter,nullptr);
            case 120:sessions.erase(c.window);RemoveWindowSubclass(c.window,&window_proc,subclass_id);
                admission(c.window,128,"presenter registration rejected");return 0;
            case 121:c.session=sessions.at(c.window);c.session->parent=GetParent(c.window);return 0;
            case 122:return c.session->parent&&IsWindow(c.session->parent);
            case 123:{if(!++setup_generation)++setup_generation;
                const auto identity=reinterpret_cast<HANDLE>(setup_generation);
                if(SetPropA(c.session->parent,retained_property,identity)&&
                   SetPropA(c.window,retained_property,identity))c.session->identity=setup_generation;
                else{
                    if(GetPropA(c.session->parent,retained_property)==identity)
                        RemovePropA(c.session->parent,retained_property);
                    if(GetPropA(c.window,retained_property)==identity)
                        RemovePropA(c.window,retained_property);
                }return 0;}
            case 124:{const std::lock_guard<std::mutex> lock(observed_mutex);
                observed.insert(c.window);return 0;}
            case 125:admission(c.window,256,"native dialog presenter registered");return 0;
            case 126:InvalidateRect(c.window,nullptr,FALSE);return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_setup_observe(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
bool begin_loading_handover(){
    struct Context {
        std::shared_ptr<Session> session;DWORD process{};ULONGLONG now{};
        std::shared_ptr<Artwork> overlay;std::exception_ptr error{};
    } c;
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 130:return wanted;
            case 131:return GetCurrentThreadId()==owner_thread;
            case 132:return bool(retained_setup);
            case 133:log("Accepted IA loading: no admitted setup snapshot; native loading fallback retained");return 0;
            case 134:c.session=retained_setup;c.now=GetTickCount64();return 0;
            case 135:return c.session->identity&&c.session->parent&&IsWindow(c.session->parent)&&
                reinterpret_cast<std::uintptr_t>(GetPropA(c.session->parent,retained_property))==c.session->identity&&
                GetWindowThreadProcessId(c.session->parent,&c.process)==GetCurrentThreadId()&&
                c.process==GetCurrentProcessId()&&
                ((!c.session->closed_at&&IsWindow(c.session->window)&&
                  reinterpret_cast<std::uintptr_t>(GetPropA(c.session->window,retained_property))==c.session->identity)||
                 (c.session->closed_at&&c.now>=c.session->closed_at&&c.now-c.session->closed_at<=3000&&
                  (!IsWindow(c.session->window)||reinterpret_cast<std::uintptr_t>(GetPropA(c.session->window,retained_property))==c.session->identity)));
            case 136:return c.session->output.bits!=nullptr;
            case 137:return c.session->output.width>0&&c.session->output.height>0;
            case 138:log("Accepted IA loading: setup identity/lease unavailable; native loading fallback retained");
                forget_retained_setup();return 0;
            case 139:c.overlay=std::make_shared<Artwork>();c.overlay->width=c.session->output.width;
                c.overlay->height=c.session->output.height;return 0;
            case 140:return GdiFlush()!=FALSE;
            case 141:c.overlay->pixels.assign(c.session->output.bits,
                c.session->output.bits+std::size_t(c.overlay->width)*c.overlay->height);return 0;
            case 142:return menu_scene::begin_accepted_loading_handover(c.session->parent,
                std::move(c.overlay));
            case 143:forget_retained_setup();return 0;
            case 144:log("Accepted IA loading: retained setup fades into departure; native acceptance observed");return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_setup_handover(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
void shutdown(){
    struct Context {std::exception_ptr error{};} c;
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 150:wanted=false;return 0;
            case 151:return GetCurrentThreadId()==owner_thread;
            case 152:forget_retained_setup();return 0;
            case 153:return set_refresh!=nullptr;
            case 154:return GetCurrentThreadId()==owner_thread;
            case 155:set_refresh(nullptr,nullptr);set_refresh=nullptr;return 0;
            case 156:return set_seed!=nullptr;
            case 157:return GetCurrentThreadId()==owner_thread;
            case 158:set_seed(nullptr,nullptr);set_seed=nullptr;return 0;
            case 159:{const std::lock_guard<std::mutex> lock(observed_mutex);
                a2fo_stage10_presentation_each(observed.begin(),observed.end(),[](const auto window){PostMessageA(window,restore_message,0,0);});
                return 0;}
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_setup_shutdown(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
}
