#include "../../rust/stage10_ui_completion_ffi.h"
#include "canvas.hpp"
#include "../../sdk/include/a2fo_shell_canvas.h"
#include "../../rust/stage8_b_canvas_ffi.h"
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <vector>

extern "C" void a2fo_shell_register_entry();
extern "C" void a2fo_shell_bounds_entry();
extern "C" void __cdecl a2fo_shell_register_call(void*, void*, HWND, int, int, int, int);
extern "C" void __cdecl a2fo_shell_set_position(void*, void*, int);

namespace a2fo::shell_display::canvas {
namespace {
constexpr std::uintptr_t kRegister = 0x173a64;
constexpr std::uintptr_t kUpdateBounds = 0xcdbc8, kAdjustSize = 0xc4c20;
constexpr std::uintptr_t kCheckboxCursorCall = 0x166323;
// FO replaces only Armada's Admiral-log pane CreateDialogParam callback with
// this procedure (installed FO+1d0a14, operand at Armada+1e3bf9).
constexpr std::uintptr_t kScreenDialogProc = 0x1d0850;
constexpr std::uint8_t kRegisterBytes[]{0x55,0x8b,0xec,0x83,0xc4,0xec};
constexpr std::uint8_t kBoundsBytes[]{0x53,0x56,0x57,0x83,0xc4,0xc4};
constexpr std::uint8_t kCheckboxCursorBytes[]{0xe8,0x04,0x30,0xea,0xff};
const A2FO_ModuleApi* g_api{};
HMODULE g_armada{}, g_fleetops{};
std::atomic<bool> g_ready{false};
std::atomic<void*> g_register_next{nullptr};
std::atomic<void*> g_bounds_next{nullptr};
A2FO_InlineHook g_register_hook{};
A2FO_InlineHook g_bounds_hook{};
thread_local Mode g_mode{};
thread_local A2FO_ShellCanvasSeedV1 g_seed{};
thread_local void* g_seed_context{};
thread_local A2FO_ShellCanvasRefreshV1 g_refresh{};
thread_local void* g_refresh_context{};
thread_local A2FO_ShellCanvasCompletionV1 g_completion{};
thread_local void* g_completion_context{};
thread_local bool g_positioning = false;
thread_local bool g_representing_seed = false;
struct Dispatch { HWND window{}; UINT message{}; LPARAM original{}; Dispatch* previous{}; };
thread_local Dispatch* g_dispatch{};

struct Window {
    HWND handle{}, root{}, parent{}, owner{};
    WNDPROC next{};
    WNDPROC dialog_next{};
    Scale root_scale{};
    int left{}, top{}, width{}, height{}, border_width{}, border_height{};
    HFONT font{}, scaled_font{};
    int font_width{}, font_height{};
    bool scroll_logged{};
    bool dialog_mouse_logged{};
    bool root_fit_logged{};
    bool owned_pane{};
    bool native_presented{},early_seed_logged{},seed_valid{},visible_seed_logged{};
    DWORD registered_at=GetTickCount();
    A2FO_ShellCanvasPresenterV1 presenter{};
    void* presenter_context{};
    Surface surface;
};
// HWNDs and their native VCL objects belong to their UI thread. TLS prevents
// unrelated rendering threads from seeing or modifying this registry.
thread_local std::map<HWND, std::shared_ptr<Window>> g_windows;
struct Lease {
    std::shared_ptr<Window> window;
    HDC physical{}, logical{};
    PAINTSTRUCT paint{};
    bool painting{};
    int saved{};
    std::unique_ptr<Surface> surface;
};
thread_local std::vector<Lease> g_leases;

void log(const char* message) { if (g_api && g_api->log) g_api->log("A2FOShellDisplay", message); }
std::shared_ptr<Window> lookup(HWND window) {
    const auto entry = g_windows.find(window);
    return entry == g_windows.end() ? nullptr : entry->second;
}
std::shared_ptr<Window> scoped(HWND window) {
    return g_ready.load(std::memory_order_acquire) && valid_mode(g_mode) ? lookup(window) : nullptr;
}
std::shared_ptr<Window> nested_dialog_parent(HWND handle) {
    struct Context {HWND handle;std::shared_ptr<Window> parent{};
        LONG_PTR style{};char name[32]{};WNDPROC proc{};
        std::exception_ptr error{};} c{handle};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id){
        case 1100:c.parent=scoped(GetParent(c.handle));return c.parent!=nullptr;
        case 1101:c.style=GetWindowLongPtrA(c.handle,GWL_STYLE);return 0;
        case 1102:return (c.style&WS_CHILD)!=0;
        case 1103:c.proc=reinterpret_cast<WNDPROC>(
            GetWindowLongPtrA(c.handle,DWLP_DLGPROC));return 0;
        case 1104:return g_fleetops!=nullptr;
        case 1105:return (c.style&WS_POPUP)!=0;
        case 1106:return (c.style&DS_CONTROL)!=0;
        case 1107:return GetClassNameA(c.handle,c.name,sizeof(c.name))!=0;
        case 1108:return lstrcmpiA(c.name,"#32770")==0;
        case 1109:return c.proc==reinterpret_cast<WNDPROC>(
            reinterpret_cast<BYTE*>(g_fleetops)+kScreenDialogProc);
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool inherited=a2fo_rs_stage8_b_canvas_nested_parent(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return inherited?c.parent:nullptr;
}
void quiet_secondary(Window& record) {
    struct Context {Window& record;char name[64]{};
        LONG_PTR previous{},replacement{},set_result{};std::exception_ptr error{};} c{record};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id){
        case 1200:return c.record.handle==c.record.owner;
        case 1201:return (GetWindowLongPtrA(c.record.handle,GWL_STYLE)&WS_CHILD)!=0;
        case 1202:return GetClassNameA(c.record.handle,c.name,sizeof(c.name))!=0;
        case 1203:return GetParent(c.record.handle)!=nullptr;
        case 1204:return c.record.dialog_next||c.record.owned_pane;
        case 1205:c.previous=GetWindowLongPtrA(c.record.handle,GWL_EXSTYLE);return 0;
        case 1206:c.replacement=(c.previous|WS_EX_TOOLWINDOW)&
            ~static_cast<LONG_PTR>(WS_EX_APPWINDOW);return 0;
        case 1207:return c.previous==c.replacement;
        case 1208:SetLastError(0);return 0;
        case 1209:c.set_result=SetWindowLongPtrA(c.record.handle,GWL_EXSTYLE,c.replacement);return 0;
        case 1210:return c.set_result==0;
        case 1211:return GetLastError()!=0;
        case 1212:{char message[160]{};
            std::snprintf(message,sizeof(message),
                "Secondary shell window kept out of taskbar: class=%s; hwnd=%p; owner=%p",
                c.name,static_cast<void*>(c.record.handle),
                static_cast<void*>(GetParent(c.record.handle)));
            log(message);return 0;}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_quiet_secondary(&c,op,
        reinterpret_cast<const std::uint8_t*>(c.name));
    if(c.error)std::rethrow_exception(c.error);
}
Scale local_scale(const Window& window) {
    RECT client{};
    if (!GetClientRect(window.handle, &client)) return {};
    return {window.width - window.border_width, window.height - window.border_height,
            client.right, client.bottom};
}
void refresh_cached_seed(Window& record,HDC physical,const Scale& scale){
    struct Context {Window& record;HDC physical;const Scale& scale;
        HDC backing{},candidate{};Surface staging{};int saved{};bool refreshed{};
        std::exception_ptr error{};} c{record,physical,scale};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id){
        case 900:c.backing=c.record.surface.cached(c.scale.logical_width,c.scale.logical_height);return 0;
        case 901:return g_refresh!=nullptr;
        case 902:return c.record.seed_valid;
        case 903:return c.backing!=nullptr;
        case 904:c.candidate=c.staging.get(c.physical,c.scale.logical_width,c.scale.logical_height);return 0;
        case 906:return c.candidate!=nullptr;
        case 907:c.saved=SaveDC(c.candidate);return 0;
        case 908:return c.saved!=0;
        case 909:try{c.refreshed=g_refresh(c.record.handle,c.candidate,
            c.scale.logical_width,c.scale.logical_height,g_refresh_context)!=FALSE;}catch(...){}
            return 0;
        case 910:return RestoreDC(c.candidate,c.saved)!=FALSE;
        case 911:c.refreshed=false;return 0;
        case 912:return c.refreshed;
        case 913:BitBlt(c.backing,0,0,c.scale.logical_width,c.scale.logical_height,
            c.candidate,0,0,SRCCOPY);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_refresh_seed(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
bool present_early_seed(const std::shared_ptr<Window>& record,HDC physical,const char* phase){
    struct Context {const std::shared_ptr<Window>& record;HDC physical;const char* phase;
        Scale scale{};HDC backing{};int saved{};bool fresh{};
        std::exception_ptr error{};} c{record,physical,phase};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id){
        case 920:return c.record!=nullptr;
        case 921:return c.record->handle==c.record->root;
        case 922:return !c.record->native_presented;
        case 923:return g_seed!=nullptr;
        case 924:return c.physical!=nullptr;
        case 925:c.scale=local_scale(*c.record);return 0;
        case 926:return c.scale.valid();
        case 927:c.fresh=!c.record->surface.cached(c.scale.logical_width,c.scale.logical_height);return 0;
        case 928:c.backing=c.record->surface.get(c.physical,c.scale.logical_width,c.scale.logical_height);return 0;
        case 929:return c.backing!=nullptr;
        case 930:return c.fresh;
        case 931:c.saved=SaveDC(c.backing);return 0;
        case 932:c.record->seed_valid=false;return 0;
        case 933:return c.saved!=0;
        case 934:try{c.record->seed_valid=g_seed(c.record->handle,c.backing,
            c.scale.logical_width,c.scale.logical_height,g_seed_context)!=FALSE;}catch(...){}
            return 0;
        case 935:return RestoreDC(c.backing,c.saved)!=FALSE;
        case 936:c.record->seed_valid=false;return 0;
        case 937:return c.record->seed_valid;
        case 938:PatBlt(c.backing,0,0,c.scale.logical_width,c.scale.logical_height,BLACKNESS);return 0;
        case 939:refresh_cached_seed(*c.record,c.physical,c.scale);return 0;
        case 940:return c.record->seed_valid;
        case 941:return c.record->surface.present(c.physical,c.scale);
        case 942:return c.record->early_seed_logged;
        case 943:{char message[160]{};
            std::snprintf(message,sizeof(message),
                "Shell early scene presented: %s; %lu ms after root registration",c.phase,
                static_cast<unsigned long>(GetTickCount()-c.record->registered_at));
            log(message);return 0;}
        case 944:c.record->early_seed_logged=true;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool shown=a2fo_rs_stage8_b_canvas_present_seed(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return shown;
}
void represent_cached_seed(const std::shared_ptr<Window>& record){
    struct Context {const std::shared_ptr<Window>& record;Scale scale{};HDC physical{};
        bool shown{};std::exception_ptr error{};} c{record};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id){
        case 950:return !g_representing_seed;
        case 951:return c.record!=nullptr;
        case 952:return c.record->handle==c.record->root;
        case 953:return !c.record->native_presented;
        case 954:return c.record->seed_valid;
        case 955:return g_seed!=nullptr;
        case 956:return IsWindowVisible(c.record->handle)!=FALSE;
        case 957:c.scale=local_scale(*c.record);return 0;
        case 958:return c.scale.valid();
        case 959:return c.record->surface.cached(c.scale.logical_width,c.scale.logical_height)!=nullptr;
        case 960:g_representing_seed=true;return 0;
        case 961:c.physical=GetDC(c.record->handle);return 0;
        case 962:return c.physical!=nullptr;
        case 963:refresh_cached_seed(*c.record,c.physical,c.scale);return 0;
        case 964:c.shown=c.record->surface.present(c.physical,c.scale);return 0;
        case 965:ReleaseDC(c.record->handle,c.physical);return 0;
        case 966:return c.shown;
        case 967:return c.record->visible_seed_logged;
        case 968:{char message[160]{};
            std::snprintf(message,sizeof(message),
                "Shell retained scene replayed after visible show/resize: %lu ms after registration",
                static_cast<unsigned long>(GetTickCount()-c.record->registered_at));
            log(message);return 0;}
        case 969:c.record->visible_seed_logged=true;return 0;
        case 970:g_representing_seed=false;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_represent_seed(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
void update_font(Window& window, HFONT font) {
    struct Context {Window& window;HFONT font,previous{},replacement{};
        LOGFONTA description{};std::exception_ptr error{};} c{window,font};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id){
        case 820:return c.font==c.window.scaled_font;
        case 821:c.font=c.window.font;return 0;
        case 822:return c.font==c.window.font;
        case 823:return c.window.scaled_font!=nullptr;
        case 824:return c.window.font_width==c.window.root_scale.physical_width;
        case 825:return c.window.font_height==c.window.root_scale.physical_height;
        case 826:return c.font!=nullptr;
        case 827:return GetObjectA(c.font,sizeof(c.description),&c.description)==sizeof(c.description);
        case 828:{const Scale& scale=c.window.root_scale;
            const bool narrower=static_cast<long long>(scale.physical_width)*scale.logical_height<
                static_cast<long long>(scale.physical_height)*scale.logical_width;
            c.description.lfHeight=narrower?scale.x(c.description.lfHeight):scale.y(c.description.lfHeight);
            if(c.description.lfWidth)c.description.lfWidth=scale.x(c.description.lfWidth);
            return 0;}
        case 829:c.replacement=CreateFontIndirectA(&c.description);return 0;
        case 830:return c.replacement!=nullptr;
        case 831:c.previous=c.window.scaled_font;return 0;
        case 832:c.window.font=c.font;c.window.scaled_font=c.replacement;
            c.window.font_width=c.window.root_scale.physical_width;
            c.window.font_height=c.window.root_scale.physical_height;return 0;
        case 833:SendMessageA(c.window.handle,WM_SETFONT,
            reinterpret_cast<WPARAM>(c.replacement),FALSE);return 0;
        case 834:return c.previous!=nullptr;
        case 835:DeleteObject(c.previous);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_update_font(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK game_dialog_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
void bridge_dialog(Window& record) {
    struct Context {Window& record;char name[64]{};WNDPROC original{};
        HMODULE owner{};LONG_PTR previous{};std::exception_ptr error{};} c{record};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id){
        case 800:return GetClassNameA(c.record.handle,c.name,sizeof(c.name))!=0;
        case 801:return lstrcmpiA(c.name,"#32770")==0;
        case 802:c.original=reinterpret_cast<WNDPROC>(
            GetWindowLongPtrA(c.record.handle,DWLP_DLGPROC));return c.original!=nullptr;
        case 803:return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(c.original),&c.owner)!=FALSE;
        case 804:return c.owner!=nullptr;
        case 805:return c.owner==g_armada||c.owner==g_fleetops;
        case 806:log("Shell dialog input bridge skipped: callback owner is not a verified game image");return 0;
        case 807:c.record.dialog_next=c.original;return 0;
        case 808:SetLastError(0);return 0;
        case 809:c.previous=SetWindowLongPtrA(c.record.handle,DWLP_DLGPROC,
            reinterpret_cast<LONG_PTR>(&game_dialog_proc));return 0;
        case 810:return c.previous==0;
        case 811:return GetLastError()!=0;
        case 812:c.record.dialog_next=nullptr;return 0;
        case 813:log("Shell dialog input bridge installation failed; original callback retained");return 0;
        case 814:return c.previous==reinterpret_cast<LONG_PTR>(c.original);
        case 815:SetWindowLongPtrA(c.record.handle,DWLP_DLGPROC,c.previous);return 0;
        case 816:c.record.dialog_next=nullptr;return 0;
        case 817:log("Shell dialog input bridge skipped: callback changed during adoption");return 0;
        case 818:{char message[256]{};
            std::snprintf(message,sizeof(message),
                "Shell dialog input bridge installed: owner=%s; callback RVA=0x%lx",
                c.owner==g_armada?"ArmadaL":"FleetOpsHook",
                static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(c.original)-
                    reinterpret_cast<std::uintptr_t>(c.owner)));
            log(message);return 0;}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_bridge_dialog(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
std::shared_ptr<Window> adopt(
    HWND handle,const std::shared_ptr<Window>& root,bool is_root,
    int left,int top,int width,int height) {
    struct Context {
        HWND handle;const std::shared_ptr<Window>& root;bool is_root;
        int left,top,width,height;std::shared_ptr<Window> record{};
        RECT client{},outer{};LONG_PTR previous{};
        std::exception_ptr error{};
    } c{handle,root,is_root,left,top,width,height};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 700:c.record=lookup(c.handle);return c.record!=nullptr;
        case 701:return c.handle!=nullptr;
        case 702:return IsWindow(c.handle)!=FALSE;
        case 703:return c.root!=nullptr;
        case 704:return g_windows.size()<1024;
        case 705:return GetClientRect(c.handle,&c.client)!=FALSE;
        case 706:return GetWindowRect(c.handle,&c.outer)!=FALSE;
        case 707:c.record=c.is_root?c.root:std::make_shared<Window>();return 0;
        case 708:c.record->handle=c.handle;c.record->root=c.root->root;
            c.record->owner=c.root->owner;
            c.record->parent=GetParent(c.handle);
            c.record->root_scale=c.root->root_scale;
            c.record->left=c.left;c.record->top=c.top;
            c.record->width=c.width;c.record->height=c.height;
            c.record->border_width=(c.outer.right-c.outer.left)-c.client.right;
            c.record->border_height=(c.outer.bottom-c.outer.top)-c.client.bottom;
            return 0;
        case 709:c.record->next=reinterpret_cast<WNDPROC>(
            GetWindowLongPtrA(c.handle,GWLP_WNDPROC));return 0;
        case 710:return c.record->next!=nullptr;
        case 711:g_windows.emplace(c.handle,c.record);return 0;
        case 712:SetLastError(0);return 0;
        case 713:c.previous=SetWindowLongPtrA(c.handle,GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&window_proc));return 0;
        case 714:return c.previous==0;
        case 715:return GetLastError()!=0;
        case 716:g_windows.erase(c.handle);return 0;
        case 717:bridge_dialog(*c.record);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool adopted=a2fo_rs_stage8_b_canvas_adopt(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return adopted?c.record:nullptr;
}
BOOL CALLBACK adopt_child(HWND child, LPARAM root_value) {
    struct Context {HWND child;LPARAM root_value;
        std::shared_ptr<Window> root{};RECT outer{};POINT point{};
        std::exception_ptr error{};} c{child,root_value};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 780:c.root=lookup(reinterpret_cast<HWND>(c.root_value));return 0;
        case 781:return c.root!=nullptr;
        case 782:return lookup(c.child)!=nullptr;
        case 783:return GetWindowRect(c.child,&c.outer)!=FALSE;
        case 784:c.point=POINT{c.outer.left,c.outer.top};
            ScreenToClient(GetParent(c.child),&c.point);return 0;
        case 785:adopt(c.child,c.root,false,c.point.x,c.point.y,
            c.outer.right-c.outer.left,c.outer.bottom-c.outer.top);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_adopt_child(&c,op);
    if(c.error)std::rethrow_exception(c.error);
    return TRUE;
}
void physical_bounds(const Window& window, int& x, int& y, int& width, int& height) {
    const Scale& scale = window.root_scale;
    width = scale.x(x + width) - scale.x(x);
    height = scale.y(y + height) - scale.y(y);
    if (window.handle == window.root || window.owned_pane) {
        POINT origin{};
        ClientToScreen(window.owner, &origin);
        x = origin.x + scale.x(x - origin.x);
        y = origin.y + scale.y(y - origin.y);
    } else {
        x = scale.x(x); y = scale.y(y);
    }
}
bool descendant(HWND handle,HWND parent) {
    struct Context {HWND handle;std::shared_ptr<Window> record{};};
    Context context{handle};
    const auto step=+[](void* opaque,std::uint32_t operation,std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(operation){
        case 0:c.record=lookup(c.handle);return c.record!=nullptr;
        case 1:return reinterpret_cast<std::uintptr_t>(c.record->parent);
        case 2:return reinterpret_cast<std::uintptr_t>(c.handle);
        case 3:c.handle=c.record->parent;c.record.reset();return 0;
        default:return 0;
        }
    };
    return a2fo_rs_stage10_ui_descendant(&context,step,reinterpret_cast<std::uintptr_t>(parent))!=0;
}
void fit_children(HWND root,HWND subtree=nullptr) {
    struct Context {HWND root,subtree;std::vector<std::shared_ptr<Window>> children{};
        std::shared_ptr<Window> record{};int x{},y{},width{},height{};
        std::exception_ptr error{};} c{root,subtree};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            if(id>=0x50000){c.record=c.children.at(id-0x50000);
                c.x=c.record->left;c.y=c.record->top;
                c.width=c.record->width;c.height=c.record->height;return 0;}
            switch(id){
            case 760:
                // Snapshot before any move; a VCL layout callback can mutate g_windows.
                for(const auto& entry:g_windows)
                    if(entry.second->root==c.root&&entry.first!=c.root&&
                       (!c.subtree||descendant(entry.first,c.subtree)))c.children.push_back(entry.second);
                return 0;
            case 761:g_positioning=true;return 0;
            case 762:return c.children.size();
            case 763:physical_bounds(*c.record,c.x,c.y,c.width,c.height);return 0;
            case 764:SetWindowPos(c.record->handle,nullptr,c.x,c.y,c.width,c.height,
                SWP_NOACTIVATE|SWP_NOZORDER);return 0;
            case 765:update_font(*c.record,reinterpret_cast<HFONT>(
                SendMessageA(c.record->handle,WM_GETFONT,0,0)));return 0;
            case 766:g_positioning=false;return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_fit_children(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
std::shared_ptr<Window> ensure_child(HWND window) {
    struct Context {HWND window;std::shared_ptr<Window> record{},parent{};
        int x{},y{},width{},height{};std::exception_ptr error{};} c{window};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id){
        case 740:c.record=scoped(c.window);return c.record!=nullptr;
        case 741:return g_ready;
        case 742:return valid_mode(g_mode);
        case 743:return c.window!=nullptr;
        case 744:return (GetWindowLongPtrA(c.window,GWL_STYLE)&WS_CHILD)!=0;
        case 745:c.parent=scoped(GetParent(c.window));return c.parent!=nullptr;
        case 746:adopt_child(c.window,reinterpret_cast<LPARAM>(c.parent->root));return 0;
        case 747:c.record=scoped(c.window);return c.record!=nullptr;
        case 748:c.x=c.record->left;c.y=c.record->top;
            c.width=c.record->width;c.height=c.record->height;
            physical_bounds(*c.record,c.x,c.y,c.width,c.height);return 0;
        case 749:SetWindowPos(c.window,nullptr,c.x,c.y,c.width,c.height,
            SWP_NOACTIVATE|SWP_NOZORDER);return 0;
        case 750:update_font(*c.record,reinterpret_cast<HFONT>(
            SendMessageA(c.window,WM_GETFONT,0,0)));return 0;
        case 751:InvalidateRect(c.window,nullptr,TRUE);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool found=a2fo_rs_stage8_b_canvas_ensure_child(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return found?c.record:nullptr;
}

// All calls below enter through imports owned only by the two pinned game
// images. Our own imports remain genuine USER32, avoiding recursion and
// leaving responsive third-party modules outside this coordinate boundary.
BOOL WINAPI client_rect(HWND window, LPRECT rect) {
    auto record = ensure_child(window);
    const BOOL result = GetClientRect(window, rect);
    if (result && rect && record) {
        rect->right = record->width - record->border_width;
        rect->bottom = record->height - record->border_height;
    }
    return result;
}
// Registered native dialogs occasionally receive an owner-client *physical*
// fit request after logical registration. It must not replace authored bounds
// and then pass through the scale a second time (1200 -> 1920 -> 3072).
bool normalize_native_root_fit(Window& record,int& x,int& y,int& width,int& height,
                               UINT flags,const void* caller) {
    struct Context {Window& record;int& x;int& y;int& width;int& height;
        UINT flags;const void* caller;char name[32]{};RECT owner{};POINT origin{};
        std::exception_ptr error{};} c{record,x,y,width,height,flags,caller};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id){
        case 1000:return c.record.handle==c.record.root;
        case 1001:return c.record.dialog_next!=nullptr;
        case 1002:return c.record.border_width==0;
        case 1003:return c.record.border_height==0;
        case 1004:return (c.flags&SWP_NOSIZE)==0;
        case 1005:return c.record.root_scale.valid();
        case 1006:return GetClassNameA(c.record.handle,c.name,sizeof(c.name))!=0;
        case 1007:return lstrcmpiA(c.name,"#32770")==0;
        case 1008:return GetWindowLongPtrA(c.record.handle,DWLP_DLGPROC)==
            reinterpret_cast<LONG_PTR>(&game_dialog_proc);
        case 1009:return GetClientRect(c.record.owner,&c.owner)!=FALSE;
        case 1010:return ClientToScreen(c.record.owner,&c.origin)!=FALSE;
        case 1011:return c.owner.right==c.record.root_scale.physical_width;
        case 1012:return c.owner.bottom==c.record.root_scale.physical_height;
        case 1013:return c.width==c.owner.right;
        case 1014:return c.height==c.owner.bottom;
        case 1015:return (c.flags&SWP_NOMOVE)||c.x==c.origin.x;
        case 1016:return (c.flags&SWP_NOMOVE)||c.y==c.origin.y;
        case 1017:return (c.flags&SWP_NOMOVE)!=0;
        case 1018:c.x=c.record.left;c.y=c.record.top;return 0;
        case 1019:c.x=c.origin.x+c.record.root_scale.logical_x(c.x-c.origin.x);
            c.y=c.origin.y+c.record.root_scale.logical_y(c.y-c.origin.y);return 0;
        case 1020:c.width=c.record.root_scale.logical_width;
            c.height=c.record.root_scale.logical_height;return 0;
        case 1021:return c.record.root_fit_logged;
        case 1022:{char message[224]{};
            std::snprintf(message,sizeof(message),
                "Native dialog physical fit normalized: %dx%d -> authored %dx%d; origin=(%ld,%ld); caller=%p",
                static_cast<int>(c.owner.right),static_cast<int>(c.owner.bottom),
                c.width,c.height,c.origin.x,c.origin.y,c.caller);log(message);return 0;}
        case 1023:c.record.root_fit_logged=true;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool normalized=a2fo_rs_stage8_b_canvas_normalize_fit(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return normalized;
}

BOOL WINAPI move_window(HWND window, int x, int y, int width, int height, BOOL repaint) {
    struct Context {HWND window;int x,y,width,height;BOOL repaint,result{};
        const void* caller;std::shared_ptr<Window> record{};
        std::exception_ptr error{};} c{window,x,y,width,height,repaint,{},__builtin_return_address(0)};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id){
        case 1300:c.record=ensure_child(c.window);return 0;
        case 1301:return c.record!=nullptr;
        case 1302:return g_positioning;
        case 1303:normalize_native_root_fit(*c.record,c.x,c.y,c.width,c.height,0,c.caller);return 0;
        case 1304:c.record->left=c.x;c.record->top=c.y;
            c.record->width=c.width;c.record->height=c.height;return 0;
        case 1305:physical_bounds(*c.record,c.x,c.y,c.width,c.height);return 0;
        case 1306:c.result=MoveWindow(c.window,c.x,c.y,c.width,c.height,c.repaint);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_move_window(&c,op);
    if(c.error)std::rethrow_exception(c.error);
    return c.result;
}
BOOL WINAPI position_window(HWND window, HWND after, int x, int y, int width, int height, UINT flags) {
    struct Context {HWND window,after;int x,y,width,height;UINT flags;BOOL result{};
        const void* caller;std::shared_ptr<Window> record{};
        std::exception_ptr error{};} c{window,after,x,y,width,height,flags,{},__builtin_return_address(0)};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id){
        case 1320:c.record=ensure_child(c.window);return 0;
        case 1321:return c.record!=nullptr;
        case 1322:return g_positioning;
        case 1323:normalize_native_root_fit(*c.record,c.x,c.y,c.width,c.height,c.flags,c.caller);return 0;
        case 1324:return (c.flags&SWP_NOMOVE)!=0;
        case 1325:c.x=c.record->left;c.y=c.record->top;return 0;
        case 1326:c.record->left=c.x;c.record->top=c.y;return 0;
        case 1327:return (c.flags&SWP_NOSIZE)!=0;
        case 1328:c.width=c.record->width;c.height=c.record->height;return 0;
        case 1329:c.record->width=c.width;c.record->height=c.height;return 0;
        case 1330:physical_bounds(*c.record,c.x,c.y,c.width,c.height);return 0;
        case 1331:c.result=SetWindowPos(c.window,c.after,c.x,c.y,c.width,c.height,c.flags);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_position_window(&c,op);
    if(c.error)std::rethrow_exception(c.error);
    return c.result;
}
BOOL WINAPI screen_to_client(HWND window, LPPOINT point) {
    const BOOL result = ScreenToClient(window, point);
    if (result && point) if (auto record = scoped(window)) {
        const Scale scale = local_scale(*record);
        if (scale.valid()) { point->x = scale.logical_x(point->x); point->y = scale.logical_y(point->y); }
    }
    return result;
}
BOOL WINAPI client_to_screen(HWND window, LPPOINT point) {
    if (point) if (auto record = scoped(window)) {
        const Scale scale = local_scale(*record);
        if (scale.valid()) { point->x = scale.x(point->x); point->y = scale.y(point->y); }
    }
    return ClientToScreen(window, point);
}
int WINAPI map_points(HWND from, HWND to, LPPOINT points, UINT count) {
    auto source = scoped(from), target = scoped(to);
    if ((!source && !target) || !points || count > 4096) return MapWindowPoints(from, to, points, count);
    const Scale source_scale = source ? local_scale(*source) : Scale{};
    const Scale target_scale = target ? local_scale(*target) : Scale{};
    struct Context {HWND from,to;LPPOINT points;UINT count;
        const Scale& source;const Scale& target;int result=0;};
    Context context{from,to,points,count,source_scale,target_scale};
    const auto step=+[](void* opaque,std::uint32_t operation,std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(operation){
        case 0:return c.source.valid();
        case 1:c.points[index].x=c.source.x(c.points[index].x);c.points[index].y=c.source.y(c.points[index].y);return 0;
        case 2:c.result=MapWindowPoints(c.from,c.to,c.points,c.count);return 0;
        case 3:return c.target.valid();
        case 4:c.points[index].x=c.target.logical_x(c.points[index].x);c.points[index].y=c.target.logical_y(c.points[index].y);return 0;
        default:return 0;
        }
    };
    a2fo_rs_stage10_ui_map_points(&context,step,count);
    return context.result;
}
HRGN scaled_region(HRGN region, const Scale& scale) {
    if (!region || !scale.valid()) return nullptr;
    const DWORD size = GetRegionData(region, 0, nullptr);
    if (!size || size > 1048576) return nullptr;
    std::vector<BYTE> storage(size);
    auto* data = reinterpret_cast<RGNDATA*>(storage.data());
    if (GetRegionData(region, size, data) != size) return nullptr;
    const XFORM transform{static_cast<float>(scale.physical_width) / scale.logical_width, 0, 0,
        static_cast<float>(scale.physical_height) / scale.logical_height, 0, 0};
    return ExtCreateRegion(&transform, size, data);
}
BOOL WINAPI invalidate_rect(HWND window, const RECT* rect, BOOL erase) {
    RECT mapped{};
    if (rect) if (auto record = scoped(window)) {
        const Scale scale = local_scale(*record);
        if (scale.valid()) { mapped = scale.bounds(*rect); rect = &mapped; }
    }
    return InvalidateRect(window, rect, erase);
}
BOOL WINAPI validate_rect(HWND window, const RECT* rect) {
    RECT mapped{};
    if (rect) if (auto record = scoped(window)) {
        const Scale scale = local_scale(*record);
        if (scale.valid()) { mapped = scale.bounds(*rect); rect = &mapped; }
    }
    return ValidateRect(window, rect);
}
BOOL WINAPI invalidate_region(HWND window, HRGN region, BOOL erase) {
    auto record = scoped(window);
    const HRGN mapped = record ? scaled_region(region, local_scale(*record)) : nullptr;
    const BOOL result = InvalidateRgn(window, mapped ? mapped : region, erase);
    if (mapped) DeleteObject(mapped);
    return result;
}
BOOL WINAPI redraw_window(HWND window, const RECT* rect, HRGN region, UINT flags) {
    RECT mapped_rect{};
    auto record = scoped(window);
    const Scale scale = record ? local_scale(*record) : Scale{};
    if (rect && scale.valid()) { mapped_rect = scale.bounds(*rect); rect = &mapped_rect; }
    const HRGN mapped = record ? scaled_region(region, scale) : nullptr;
    const BOOL result = RedrawWindow(window, rect, mapped ? mapped : region, flags);
    if (mapped) DeleteObject(mapped);
    return result;
}
int WINAPI scroll_window_ex(HWND window,int dx,int dy,const RECT* scroll,
                            const RECT* clip,HRGN update,LPRECT update_rect,UINT flags) {
    struct Context {
        HWND window;int dx,dy;const RECT *scroll,*clip;
        HRGN update;LPRECT update_rect;UINT flags;
        std::shared_ptr<Window> record{};Scale scale{};
        RECT logical_scroll{},logical_clip{},physical_scroll{},
            physical_clip{},affected{},physical_affected{},exposed{},bounds{};
        HRGN physical_update{},uncovered{},logical_update{},mapped{};
        HDC cached{},output{};int result{},copied{},saved{};
        bool presented{};std::exception_ptr error{};
    } c{window,dx,dy,scroll,clip,update,update_rect,flags};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 200:c.record=scoped(c.window);return 0;
        case 201:c.scale=c.record?local_scale(*c.record):Scale{};return 0;
        case 202:return c.record!=nullptr;
        case 203:return c.scale.valid();
        case 204:return (c.flags&~(SW_INVALIDATE|SW_ERASE))==0;
        case 205:return c.update!=nullptr;
        case 206:return GetObjectType(c.update)==OBJ_REGION;
        case 207:c.result=ScrollWindowEx(c.window,c.dx,c.dy,c.scroll,c.clip,
            c.update,c.update_rect,c.flags);return 0;
        case 208:c.logical_scroll=c.scroll?*c.scroll:
            RECT{0,0,c.scale.logical_width,c.scale.logical_height};return 0;
        case 209:c.logical_clip=c.clip?*c.clip:
            RECT{0,0,c.scale.logical_width,c.scale.logical_height};return 0;
        case 210:c.physical_scroll=c.scale.bounds(c.logical_scroll);
            c.physical_clip=c.scale.bounds(c.logical_clip);
            IntersectRect(&c.affected,&c.logical_scroll,&c.logical_clip);
            c.physical_affected=c.scale.bounds(c.affected);return 0;
        case 211:c.physical_update=CreateRectRgn(0,0,0,0);
            return c.physical_update!=nullptr;
        case 212:InvalidateRect(c.window,&c.physical_affected,FALSE);return 0;
        case 213:SetLastError(ERROR_NOT_ENOUGH_MEMORY);c.result=ERROR;return 0;
        case 214:GdiFlush();return 0;
        case 215:c.result=ScrollWindowEx(c.window,c.scale.x(c.dx),
            c.scale.y(c.dy),c.scroll?&c.physical_scroll:nullptr,
            c.clip?&c.physical_clip:nullptr,c.physical_update,&c.exposed,
            c.flags);return 0;
        case 216:return c.result!=ERROR;
        case 217:return c.update_rect!=nullptr;
        case 218:*c.update_rect=c.scale.logical_bounds(c.exposed);return 0;
        case 219:return c.update!=nullptr;
        case 220:{const Scale inverse{c.scale.physical_width,
                c.scale.physical_height,c.scale.logical_width,
                c.scale.logical_height};
            c.logical_update=scaled_region(c.physical_update,inverse);
            return 0;}
        case 221:c.copied=c.logical_update
            ?CombineRgn(c.update,c.logical_update,nullptr,RGN_COPY):ERROR;
            return 0;
        case 222:if(c.logical_update)DeleteObject(c.logical_update);return 0;
        case 223:return c.copied!=ERROR;
        case 224:c.result=c.copied;return 0;
        case 225:c.bounds=c.scale.logical_bounds(c.exposed);return 0;
        case 226:return SetRectRgn(c.update,c.bounds.left,c.bounds.top,
            c.bounds.right,c.bounds.bottom)!=FALSE;
        case 227:{RECT ignored{};c.result=GetRgnBox(c.update,&ignored);
            return 0;}
        case 228:c.result=ERROR;InvalidateRect(c.window,
            &c.physical_affected,FALSE);SetLastError(ERROR_INVALID_HANDLE);
            return 0;
        case 229:c.cached=c.record->surface.cached(c.scale.logical_width,
            c.scale.logical_height);return 0;
        case 230:return c.cached!=nullptr;
        case 231:c.uncovered=CreateRectRgn(0,0,0,0);return 0;
        case 232:return c.uncovered!=nullptr;
        case 233:return ScrollDC(c.cached,c.dx,c.dy,
            c.scroll?&c.logical_scroll:nullptr,
            c.clip?&c.logical_clip:nullptr,c.uncovered,nullptr)!=FALSE;
        case 234:c.output=GetDC(c.window);return 0;
        case 235:return c.output!=nullptr;
        case 236:c.saved=SaveDC(c.output);return 0;
        case 237:return c.saved!=0;
        case 238:return IntersectClipRect(c.output,c.physical_affected.left,
            c.physical_affected.top,c.physical_affected.right,
            c.physical_affected.bottom)!=ERROR;
        case 239:c.presented=c.record->surface.present(c.output,c.scale);
            return 0;
        case 240:RestoreDC(c.output,c.saved);return 0;
        case 241:ReleaseDC(c.window,c.output);return 0;
        case 242:return c.presented;
        case 243:InvalidateRect(c.window,&c.physical_affected,FALSE);return 0;
        case 244:return (c.flags&(SW_INVALIDATE|SW_ERASE))!=0;
        case 245:c.mapped=scaled_region(c.uncovered,c.scale);return 0;
        case 246:return c.mapped!=nullptr;
        case 247:InvalidateRgn(c.window,c.mapped,
                (c.flags&SW_ERASE)!=0);
            DeleteObject(c.mapped);return 0;
        case 248:InvalidateRect(c.window,&c.physical_affected,
            (c.flags&SW_ERASE)!=0);return 0;
        case 249:return c.record->scroll_logged;
        case 250:{c.record->scroll_logged=true;
            char name[64]{},message[224]{};
            GetClassNameA(c.window,name,sizeof(name));
            std::snprintf(message,sizeof(message),
                "Shell scroll canvas synchronized: class=%s; logical=%dx%d; physical=%dx%d; delta=%d,%d",
                name,c.scale.logical_width,c.scale.logical_height,
                c.scale.physical_width,c.scale.physical_height,c.dx,c.dy);
            log(message);return 0;}
        case 251:InvalidateRect(c.window,&c.physical_affected,
            (c.flags&SW_ERASE)!=0);return 0;
        case 252:return c.uncovered!=nullptr;
        case 253:DeleteObject(c.uncovered);return 0;
        case 254:DeleteObject(c.physical_update);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_scroll(&c,op);
    if(c.error)std::rethrow_exception(c.error);
    return c.result;
}
HDC acquire(HWND window,HDC physical,const PAINTSTRUCT* paint) {
    struct Context {
        HWND window;HDC physical;const PAINTSTRUCT* paint;
        std::shared_ptr<Window> record{};Scale scale{};
        bool fresh{},seeded{};DWORD started{};
        HDC backing{},logical{};int seed_saved{},saved{};
        std::unique_ptr<Surface> surface{};RECT dirty{};
        std::exception_ptr error{};
    } c{window,physical,paint};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 300:c.record=scoped(c.window);return 0;
        case 301:c.scale=c.record?local_scale(*c.record):Scale{};return 0;
        case 302:return c.physical!=nullptr;
        case 303:return c.record!=nullptr;
        case 304:return c.scale.valid();
        case 305:c.fresh=!c.record->surface.cached(c.scale.logical_width,
            c.scale.logical_height);return 0;
        case 306:c.backing=c.record->surface.get(c.physical,
            c.scale.logical_width,c.scale.logical_height);return 0;
        case 307:return c.backing!=nullptr;
        case 308:return c.fresh;
        case 309:return c.record->root==c.window;
        case 310:return g_seed!=nullptr;
        case 311:c.started=GetTickCount();return 0;
        case 312:c.seed_saved=SaveDC(c.backing);return 0;
        case 313:return c.seed_saved!=0;
        case 314:try {c.seeded=g_seed(c.window,c.backing,
            c.scale.logical_width,c.scale.logical_height,
            g_seed_context)!=FALSE;}catch(...){c.seeded=false;}
            return 0;
        case 315:return RestoreDC(c.backing,c.seed_saved)!=FALSE;
        case 316:c.seeded=false;return 0;
        case 317:return c.seeded;
        case 318:PatBlt(c.backing,0,0,c.scale.logical_width,
            c.scale.logical_height,BLACKNESS);return 0;
        case 319:c.record->seed_valid=c.seeded;return 0;
        case 320:return c.seeded;
        case 321:{char name[64]{},message[192]{};
            GetClassNameA(c.window,name,sizeof(name));
            std::snprintf(message,sizeof(message),
                "Shell root canvas seeded: class=%s; logical=%dx%d; elapsed=%lu ms",
                name,c.scale.logical_width,c.scale.logical_height,
                static_cast<unsigned long>(GetTickCount()-c.started));
            log(message);return 0;}
        case 322:c.surface=std::make_unique<Surface>();return 0;
        case 323:c.logical=c.surface->share(c.physical,c.record->surface);
            return 0;
        case 324:return c.logical!=nullptr;
        case 325:c.saved=SaveDC(c.logical);return 0;
        case 326:return c.saved!=0;
        case 327:return c.paint!=nullptr;
        case 328:c.dirty=c.scale.logical_bounds(c.paint->rcPaint,1);return 0;
        case 329:return IntersectClipRect(c.logical,c.dirty.left,c.dirty.top,
            c.dirty.right,c.dirty.bottom)!=ERROR;
        case 330:RestoreDC(c.logical,c.saved);return 0;
        case 331:{Lease lease{c.record,c.physical,c.logical,{},
                c.paint!=nullptr,c.saved,std::move(c.surface)};
            if(c.paint)lease.paint=*c.paint;
            g_leases.push_back(std::move(lease));return 0;}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool acquired=a2fo_rs_stage8_b_canvas_acquire(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return acquired?c.logical:c.physical;
}
HDC WINAPI begin_paint(HWND window, LPPAINTSTRUCT paint) {
    ensure_child(window);
    HDC physical = BeginPaint(window, paint);
    if (!paint) return physical;
    HDC logical = acquire(window, physical, paint);
    if (logical != physical) {
        auto record = scoped(window);
        const Scale scale = local_scale(*record);
        paint->hdc = logical;
        // Keep partial paints partial. Outward rounding plus one logical pixel
        // covers scaled edges without asking VCL to redraw every grid row.
        paint->rcPaint = scale.logical_bounds(paint->rcPaint, 1);
    }
    return logical;
}
bool finish(HWND window,HDC logical,bool painting,PAINTSTRUCT* original,HDC* physical) {
    struct Context {
        HWND window;HDC logical;bool painting;PAINTSTRUCT* original;
        HDC* physical;std::size_t index{};std::optional<Lease> lease{};
        bool handled{};std::exception_ptr error{};
    } c{window,logical,painting,original,physical};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            if(id>=0x40000){c.index=id-0x40000;return 0;}
            switch(id) {
            case 340:return g_leases.size();
            case 341:{const Lease& candidate=g_leases[c.index];
                return candidate.window->handle==c.window&&
                    candidate.logical==c.logical&&
                    candidate.painting==c.painting;}
            case 342:c.lease.emplace(std::move(g_leases[c.index]));
                g_leases.erase(g_leases.begin()+
                    static_cast<std::ptrdiff_t>(c.index));return 0;
            case 343:RestoreDC(c.logical,c.lease->saved);return 0;
            case 344:return c.lease->window->presenter!=nullptr;
            case 345:try {c.handled=c.lease->window->presenter(
                    c.window,c.lease->physical,
                    c.lease->window->presenter_context)!=FALSE;
                }catch(...){c.handled=false;}
                return 0;
            case 346:return c.handled;
            case 347:c.handled=c.lease->surface->present(c.lease->physical,
                local_scale(*c.lease->window));return 0;
            case 348:return c.handled&&c.painting&&
                c.lease->window->handle==c.lease->window->root&&
                !c.lease->window->native_presented;
            case 349:{c.lease->window->native_presented=true;
                char message[160]{};
                std::snprintf(message,sizeof(message),
                    "Shell first root paint presented: %lu ms after registration",
                    static_cast<unsigned long>(
                        GetTickCount()-c.lease->window->registered_at));
                log(message);return 0;}
            case 350:return g_completion!=nullptr;
            case 351:try {g_completion(c.window,c.logical,
                    c.handled?TRUE:FALSE,g_completion_context);
                }catch(...){}return 0;
            case 352:return c.original!=nullptr;
            case 353:*c.original=c.lease->paint;return 0;
            case 354:return c.physical!=nullptr;
            case 355:*c.physical=c.lease->physical;return 0;
            case 356:c.lease.reset();return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool found=a2fo_rs_stage8_b_canvas_finish(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return found;
}
BOOL WINAPI end_paint(HWND window, const PAINTSTRUCT* paint) {
    PAINTSTRUCT original{};
    return paint && finish(window, paint->hdc, true, &original, nullptr)
        ? EndPaint(window, &original) : EndPaint(window, paint);
}
HDC WINAPI get_dc(HWND window) { ensure_child(window); return acquire(window, GetDC(window), nullptr); }
HDC WINAPI get_dc_ex(HWND window, HRGN region, DWORD flags) {
    // DCX_WINDOW is a non-client DC and has a different origin. Preserve it.
    if (!(flags & DCX_WINDOW)) ensure_child(window);
    HDC physical = GetDCEx(window, region, flags);
    return flags & DCX_WINDOW ? physical : acquire(window, physical, nullptr);
}
int WINAPI release_dc(HWND window, HDC dc) {
    HDC physical{};
    return finish(window, dc, false, nullptr, &physical) ? ReleaseDC(window, physical) : ReleaseDC(window, dc);
}
LRESULT WINAPI send_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_SETFONT) if (auto record = ensure_child(window)) {
        update_font(*record, reinterpret_cast<HFONT>(wparam));
        if (record->scaled_font) wparam = reinterpret_cast<WPARAM>(record->scaled_font);
    }
    return SendMessageA(window, message, wparam, lparam);
}

BOOL WINAPI checkbox_cursor_pos(LPPOINT point) {
    const BOOL result = GetCursorPos(point);
    if (!result || !point || !g_dispatch || g_dispatch->message != WM_LBUTTONUP) return result;
    auto record = scoped(g_dispatch->window);
    if (!record) return result;
    const Scale scale = local_scale(*record);
    POINT client = *point;
    if (scale.valid() && ScreenToClient(record->handle, &client)) {
        client.x = scale.logical_x(client.x);
        client.y = scale.logical_y(client.y);
        // TJvgCheckBox re-reads the cursor rather than using mouse lParam.
        // TControl.ScreenToClient subtracts a physical parent client origin
        // plus logical Left/Top; it never calls USER32.ScreenToClient. Give
        // that one query a logical offset anchored to the physical origin.
        if (ClientToScreen(record->handle, &client)) *point = client;
    }
    return result;
}

// VCL receives logical messages, but a native EDIT/LISTBOX/COMBOBOX's system
// procedure still paints and hit-tests a physical HWND. Restore the original
// message only at that final USER32/COMCTL32 forwarding boundary.
LPARAM system_parameter(HWND window, UINT message, LPARAM lparam) {
    if (g_dispatch && g_dispatch->window == window && g_dispatch->message == message)
        return g_dispatch->original;
    return lparam;
}
LRESULT WINAPI default_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcA(window, message, wparam, system_parameter(window, message, lparam));
}
bool system_proc(WNDPROC procedure, HWND window) {
    HMODULE module{};
    const bool known = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(procedure), &module) != FALSE;
    if (known) return module == GetModuleHandleA("user32.dll") || module == GetModuleHandleA("comctl32.dll") ||
        module == GetModuleHandleA("riched20.dll") || module == GetModuleHandleA("riched32.dll");
    // CallWindowProc accepts opaque system procedure tokens as well as code
    // addresses. Genuine builtin class names identify the direct-forward case.
    char name[64]{};
    if (!GetClassNameA(window, name, sizeof(name))) return false;
    for (const char* native : {"Edit", "ListBox", "ComboBox", "ComboLBox", "Button", "Static", "ScrollBar",
            "#32770", "SysListView32", "SysTreeView32", "SysHeader32", "msctls_trackbar32"})
        if (lstrcmpiA(name, native) == 0) return true;
    return false;
}
LRESULT WINAPI call_proc(WNDPROC procedure, HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    const auto caller = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
    const auto base = reinterpret_cast<std::uintptr_t>(g_fleetops);
    // Verified native DefaultHandler/ComboWndProc forwards to the original
    // Windows class procedure, which may be an opaque token for a TEdit class.
    const bool native_forward = caller == base + 0xcaeec || caller == base + 0x9f4cb;
    if (native_forward || system_proc(procedure, window))
        lparam = system_parameter(window, message, lparam);
    return CallWindowProcA(procedure, window, message, wparam, lparam);
}

bool mouse_message(UINT message) {
    return a2fo_rs_stage8_b_canvas_mouse_message(message)!=0;
}
LPARAM logical_parameter(const Window& record, UINT message, LPARAM lparam, WINDOWPOS& logical_position) {
    struct Context {const Window& record;UINT message;LPARAM value;
        WINDOWPOS& position;Scale scale{};std::exception_ptr error{};
    } c{record,message,lparam,logical_position};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 400:return scoped(c.record.handle)!=nullptr;
        case 401:c.scale=local_scale(c.record);return 0;
        case 402:return c.scale.valid();
        case 404:c.value=c.scale.mouse(c.value);return 0;
        case 405:return c.message==WM_SIZE;
        case 406:c.value=MAKELPARAM(c.record.width-c.record.border_width,
            c.record.height-c.record.border_height);return 0;
        case 407:return c.message==WM_WINDOWPOSCHANGED;
        case 408:return c.value!=0;
        case 409:c.position=*reinterpret_cast<const WINDOWPOS*>(c.value);
            c.position.x=c.record.left;c.position.y=c.record.top;
            c.position.cx=c.record.width;c.position.cy=c.record.height;
            return 0;
        case 410:c.value=reinterpret_cast<LPARAM>(&c.position);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_logical_parameter(&c,op,message);
    if(c.error)std::rethrow_exception(c.error);
    return c.value;
}
LRESULT CALLBACK game_dialog_proc(
    HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    struct Context {
        HWND window;UINT message;WPARAM wparam;LPARAM lparam;
        std::shared_ptr<Window> record{};LPARAM original_parameter{};
        WINDOWPOS logical_position{};Dispatch dispatch{};LRESULT result{};
        std::exception_ptr error{};
    } c{window,message,wparam,lparam};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 420:c.record=lookup(c.window);return 0;
        case 421:return c.record!=nullptr;
        case 422:return c.record->dialog_next!=nullptr;
        case 423:c.original_parameter=system_parameter(
            c.window,c.message,c.lparam);return 0;
        case 424:c.lparam=logical_parameter(*c.record,c.message,
            c.original_parameter,c.logical_position);return 0;
        case 425:return scoped(c.window)!=nullptr;
        case 426:return mouse_message(c.message);
        case 427:return !c.record->dialog_mouse_logged;
        case 428:return local_scale(*c.record).valid();
        case 429:{char diagnostic[256]{};
            std::snprintf(diagnostic,sizeof(diagnostic),
                "Shell dialog mouse mapped: physical=(%d,%d); logical=(%d,%d)",
                static_cast<short>(LOWORD(c.original_parameter)),
                static_cast<short>(HIWORD(c.original_parameter)),
                static_cast<short>(LOWORD(c.lparam)),
                static_cast<short>(HIWORD(c.lparam)));
            log(diagnostic);c.record->dialog_mouse_logged=true;return 0;}
        case 430:c.dispatch=Dispatch{c.window,c.message,
            c.original_parameter,g_dispatch};g_dispatch=&c.dispatch;return 0;
        case 431:c.result=CallWindowProcA(c.record->dialog_next,c.window,
            c.message,c.wparam,c.lparam);return 0;
        case 432:g_dispatch=c.dispatch.previous;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_dialog_dispatch(&c,op);
    if(c.error)std::rethrow_exception(c.error);
    return c.result;
}
LRESULT CALLBACK window_proc(
    HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    struct Context {
        HWND window;UINT message;WPARAM wparam;LPARAM lparam;
        std::shared_ptr<Window> record{};LPARAM original_parameter{};
        WINDOWPOS logical_position{};Dispatch dispatch{};LRESULT result{};
        std::exception_ptr error{};
    } c{window,message,wparam,lparam};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 440:c.record=lookup(c.window);return 0;
        case 441:return c.record!=nullptr;
        case 442:c.result=DefWindowProcA(c.window,c.message,c.wparam,c.lparam);
            return 0;
        case 443:return c.message==WM_ERASEBKGND;
        case 444:return scoped(c.window)!=nullptr;
        case 445:return present_early_seed(c.record,
            reinterpret_cast<HDC>(c.wparam),"background erase");
        case 446:c.original_parameter=c.lparam;return 0;
        case 447:c.lparam=logical_parameter(*c.record,c.message,
            c.lparam,c.logical_position);return 0;
        case 448:c.dispatch=Dispatch{c.window,c.message,
            c.original_parameter,g_dispatch};g_dispatch=&c.dispatch;return 0;
        case 449:return system_proc(c.record->next,c.window);
        case 450:c.lparam=c.original_parameter;return 0;
        case 451:c.result=CallWindowProcA(c.record->next,c.window,c.message,
            c.wparam,c.lparam);return 0;
        case 452:g_dispatch=c.dispatch.previous;return 0;
        case 453:return c.message==WM_SHOWWINDOW;
        case 454:return c.wparam!=0;
        case 455:return c.message==WM_WINDOWPOSCHANGED;
        case 456:return c.original_parameter!=0;
        case 457:return (reinterpret_cast<const WINDOWPOS*>(
            c.original_parameter)->flags&SWP_SHOWWINDOW)!=0;
        case 458:return (reinterpret_cast<const WINDOWPOS*>(
            c.original_parameter)->flags&SWP_NOSIZE)!=0;
        case 459:represent_cached_seed(c.record);return 0;
        case 460:return c.message==WM_NCDESTROY;
        case 461:return c.record->dialog_next!=nullptr;
        case 462:return GetWindowLongPtrA(c.window,DWLP_DLGPROC)==
            reinterpret_cast<LONG_PTR>(&game_dialog_proc);
        case 463:SetWindowLongPtrA(c.window,DWLP_DLGPROC,
            reinterpret_cast<LONG_PTR>(c.record->dialog_next));return 0;
        case 464:g_windows.erase(c.window);return 0;
        case 465:return c.record->scaled_font!=nullptr;
        case 466:DeleteObject(c.record->scaled_font);
            c.record->scaled_font=nullptr;return 0;
        case 467:c.result=1;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_window_dispatch(&c,op);
    if(c.error)std::rethrow_exception(c.error);
    return c.result;
}
struct Import { const char* name; void* original; void* replacement; unsigned found{}; };
struct Patch { void* slot; void* expected; void* replacement; };
bool collect(HMODULE image, std::vector<Patch>& patches, std::vector<Import>& imports) {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const BYTE*>(image) + dos->e_lfanew);
    const DWORD size = nt->OptionalHeader.SizeOfImage;
    const auto directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory.VirtualAddress || directory.VirtualAddress >= size) return false;
    const auto* descriptor = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(reinterpret_cast<BYTE*>(image) + directory.VirtualAddress);
    for (; descriptor->Name; ++descriptor) {
        if (!descriptor->OriginalFirstThunk || !descriptor->FirstThunk ||
            descriptor->OriginalFirstThunk >= size || descriptor->FirstThunk >= size) return false;
        auto* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(reinterpret_cast<BYTE*>(image) + descriptor->OriginalFirstThunk);
        auto* addresses = reinterpret_cast<IMAGE_THUNK_DATA*>(reinterpret_cast<BYTE*>(image) + descriptor->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++addresses) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            if (names->u1.AddressOfData >= size) return false;
            const auto* name = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(reinterpret_cast<BYTE*>(image) + names->u1.AddressOfData);
            for (auto& entry : imports) if (std::strcmp(entry.name, reinterpret_cast<const char*>(name->Name)) == 0) {
                if (reinterpret_cast<void*>(addresses->u1.Function) != entry.original) return false;
                patches.push_back({&addresses->u1.Function, entry.original, entry.replacement});
                ++entry.found;
            }
        }
    }
    return true;
}
} // namespace

extern "C" void __cdecl a2fo_shell_bounds_handler(void* control) {
    auto record = control ? scoped(*reinterpret_cast<HWND*>(static_cast<BYTE*>(control) + 0x1b4)) : nullptr;
    if (record) {
        // WM_SIZE/WM_WINDOWPOSCHANGED call UpdateBounds, which queries the
        // real GetWindowRect rather than trusting their lParam. Copying only
        // those messages left the VCL Width/Height physical in v3. Options'
        // OnShow then centred buttons in physical space before another scale.
        const int bounds[]{record->left, record->top, record->width, record->height};
        std::memcpy(static_cast<BYTE*>(control) + 0x40, bounds, sizeof(bounds));
        a2fo_shell_set_position(reinterpret_cast<BYTE*>(g_fleetops) + kAdjustSize, control, 0);
        return;
    }
    void* next{};
    while (!(next = g_bounds_next.load(std::memory_order_acquire))) Sleep(0);
    a2fo_shell_set_position(next, control, 0);
}

Scale output_scale(HWND owner, int width, int height) {
    RECT actual{};
    if (!GetClientRect(owner, &actual) || actual.right <= 0 || actual.bottom <= 0)
        return {width, height, g_mode.width, g_mode.height};
    // A framed desktop-sized window can have a client smaller than its D3D
    // mode after native work-area constraints. Fill that actual client.
    return {width, height, actual.right, actual.bottom};
}

extern "C" void __cdecl a2fo_shell_register_handler(
    void* form,HWND window,int left,int top,int width,int height) {
    struct Context {
        void* form;HWND window;int left,top,width,height;void* next{};
        std::shared_ptr<Window> root{},parent{};HWND owner{};HDC physical{};
        std::exception_ptr error{};
    } c{form,window,left,top,width,height};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 130:c.next=g_register_next.load(std::memory_order_acquire);
            return c.next!=nullptr;
        case 131:Sleep(0);return 0;
        case 132:return g_ready.load();
        case 133:return valid_mode(g_mode);
        case 134:return c.form!=nullptr;
        case 135:return IsWindow(c.window)!=FALSE;
        case 136:return c.width>=640;
        case 137:return c.height>=480;
        case 138:return c.width<=16384;
        case 139:return c.height<=16384;
        case 140:c.owner=*reinterpret_cast<HWND*>(
            static_cast<BYTE*>(c.form)+0x1b4);return 0;
        case 141:return c.owner!=nullptr;
        case 142:return IsWindow(c.owner)!=FALSE;
        case 143:return lookup(c.window)==nullptr;
        case 144:c.parent=nested_dialog_parent(c.window);
            return c.parent!=nullptr;
        case 145:c.root=adopt(c.window,c.parent,false,c.left,c.top,
            c.width,c.height);return c.root!=nullptr;
        case 146:c.root->owned_pane=!(
            GetWindowLongPtrA(c.window,GWL_STYLE)&WS_CHILD);return 0;
        case 147:c.root=std::make_shared<Window>();return 0;
        case 148:c.root->root=c.window;c.root->owner=c.owner;
            c.root->root_scale=output_scale(c.owner,c.width,c.height);
            return 0;
        case 149:c.root=adopt(c.window,c.root,true,c.left,c.top,
            c.width,c.height);return 0;
        case 150:return c.root!=nullptr;
        case 151:quiet_secondary(*c.root);return 0;
        case 152:EnumChildWindows(c.window,adopt_child,
            reinterpret_cast<LPARAM>(c.window));return 0;
        case 153:return c.root!=nullptr;
        case 154:c.physical=GetDC(c.window);return 0;
        case 155:return c.physical!=nullptr;
        case 156:present_early_seed(c.root,c.physical,
            "before native registration");return 0;
        case 157:ReleaseDC(c.window,c.physical);return 0;
        case 158:a2fo_shell_register_call(c.next,c.form,c.window,c.left,
            c.top,c.width,c.height);return 0;
        case 159:return c.root!=nullptr;
        case 160:fit_children(c.root->root,
            c.root->root==c.window?nullptr:c.window);return 0;
        case 161:c.physical=GetDC(c.window);return 0;
        case 162:return c.physical!=nullptr;
        case 163:present_early_seed(c.root,c.physical,
            "native registration");return 0;
        case 164:ReleaseDC(c.window,c.physical);return 0;
        case 165:InvalidateRect(c.window,nullptr,TRUE);return 0;
        case 166:{char message[160]{};char window_class[80]{};
            GetClassNameA(c.window,window_class,sizeof(window_class));
            std::snprintf(message,sizeof(message),c.root->root==c.window?
                "Shell canvas: %dx%d -> %dx%d; class=%s; logical VCL bounds preserved":
                "Nested shell pane: %dx%d; inherited %dx%d canvas; class=%s",
                c.width,c.height,c.root->root_scale.physical_width,
                c.root->root_scale.physical_height,window_class);
            log(message);return 0;}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_register(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
bool initialize(const A2FO_ModuleApi* api, HMODULE armada, HMODULE fleetops) {
    struct Context {
        const A2FO_ModuleApi* api;HMODULE armada,fleetops,user32{};
        BYTE *target{},*bounds_target{},*cursor_call{};
        std::vector<Import> imports{};std::vector<Patch> patches{};
        std::size_t index{};std::exception_ptr error{};
    } c{api,armada,fleetops};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            if(id>=0x20000){c.index=id-0x20000;return 0;}
            if(id>=0x10000){c.index=id-0x10000;return 0;}
            switch(id) {
            case 0:g_api=c.api;g_armada=c.armada;g_fleetops=c.fleetops;
                return 0;
            case 1:return c.api!=nullptr;
            case 2:return c.api->struct_size>=
                offsetof(A2FO_ModuleApi,patch_bytes)+sizeof(c.api->patch_bytes);
            case 3:return c.api->patch_bytes!=nullptr;
            case 4:return c.api->patch_call!=nullptr;
            case 5:c.target=reinterpret_cast<BYTE*>(c.fleetops)+kRegister;
                c.bounds_target=reinterpret_cast<BYTE*>(c.fleetops)+kUpdateBounds;
                c.cursor_call=reinterpret_cast<BYTE*>(c.fleetops)+
                    kCheckboxCursorCall;return 0;
            case 6:return std::memcmp(c.target,kRegisterBytes,
                sizeof(kRegisterBytes))==0;
            case 7:return std::memcmp(c.bounds_target,kBoundsBytes,
                sizeof(kBoundsBytes))==0;
            case 8:return std::memcmp(c.cursor_call,kCheckboxCursorBytes,
                sizeof(kCheckboxCursorBytes))==0;
            case 9:c.user32=GetModuleHandleA("user32.dll");return c.user32!=nullptr;
            case 10:{
#define IMPORT(name, callback) {#name, reinterpret_cast<void*>(GetProcAddress(c.user32, #name)), reinterpret_cast<void*>(&callback), 0}
                c.imports={
                    IMPORT(GetClientRect,client_rect),IMPORT(MoveWindow,move_window),
                    IMPORT(SetWindowPos,position_window),
                    IMPORT(ScreenToClient,screen_to_client),
                    IMPORT(ClientToScreen,client_to_screen),IMPORT(MapWindowPoints,map_points),
                    IMPORT(BeginPaint,begin_paint),IMPORT(EndPaint,end_paint),
                    IMPORT(GetDC,get_dc),IMPORT(GetDCEx,get_dc_ex),
                    IMPORT(ReleaseDC,release_dc),IMPORT(SendMessageA,send_message),
                    IMPORT(DefWindowProcA,default_proc),
                    IMPORT(CallWindowProcA,call_proc),
                    IMPORT(InvalidateRect,invalidate_rect),
                    IMPORT(ValidateRect,validate_rect),
                    IMPORT(InvalidateRgn,invalidate_region),
                    IMPORT(RedrawWindow,redraw_window),
                    IMPORT(ScrollWindowEx,scroll_window_ex)};
#undef IMPORT
                return 0;
            }
            case 11:return collect(c.armada,c.patches,c.imports);
            case 12:return collect(c.fleetops,c.patches,c.imports);
            case 13:return c.imports.size();
            case 14:return c.imports[c.index].original!=nullptr;
            case 15:return c.imports[c.index].found!=0||
                std::strcmp(c.imports[c.index].name,"GetDCEx")==0;
            case 16:return c.api->install_inline_hook(c.target,
                reinterpret_cast<void*>(&a2fo_shell_register_entry),
                sizeof(kRegisterBytes),kRegisterBytes,&g_register_hook);
            case 17:g_register_next.store(g_register_hook.gateway,
                std::memory_order_release);return 0;
            case 18:return c.api->install_inline_hook(c.bounds_target,
                reinterpret_cast<void*>(&a2fo_shell_bounds_entry),
                sizeof(kBoundsBytes),kBoundsBytes,&g_bounds_hook);
            case 19:g_bounds_next.store(g_bounds_hook.gateway,
                std::memory_order_release);return 0;
            case 20:return c.api->patch_call(c.cursor_call,
                reinterpret_cast<void*>(&checkbox_cursor_pos),
                kCheckboxCursorBytes,sizeof(kCheckboxCursorBytes));
            case 21:return c.patches.size();
            case 22:{const auto& patch=c.patches[c.index];
                return c.api->patch_bytes(patch.slot,
                    reinterpret_cast<const std::uint8_t*>(&patch.replacement),
                    reinterpret_cast<const std::uint8_t*>(&patch.expected),
                    sizeof(void*));}
            case 23:g_ready.store(true,std::memory_order_release);return 0;
            case 24:log("Full-window native shell canvas hooks installed; partial paints and logical scroll canvas synchronized");
                return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_canvas_initialize(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
void active_mode(const Mode* mode) {
    struct Context {
        const Mode* mode;std::vector<std::shared_ptr<Window>> roots{};
        std::size_t index{};int x{},y{},width{},height{};
        std::exception_ptr error{};
    } c{mode};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            if(id>=0x30000){c.index=id-0x30000;return 0;}
            switch(id) {
            case 40:g_mode=c.mode?*c.mode:Mode{};return 0;
            case 41:return g_ready.load();
            case 42:return valid_mode(g_mode);
            case 43:for(const auto& entry:g_windows){
                auto& record=*entry.second;
                if(record.handle==record.root)c.roots.push_back(entry.second);
                }return 0;
            case 44:return c.roots.size();
            case 45:{auto& root=c.roots[c.index];
                root->root_scale=output_scale(root->owner,
                    root->root_scale.logical_width,
                    root->root_scale.logical_height);return 0;}
            case 46:{const auto& root=c.roots[c.index];
                for(const auto& entry:g_windows)
                    if(entry.second->root==root->root)
                        entry.second->root_scale=root->root_scale;
                return 0;}
            case 47:{const auto& root=c.roots[c.index];
                c.x=root->left;c.y=root->top;
                c.width=root->width;c.height=root->height;
                physical_bounds(*root,c.x,c.y,c.width,c.height);return 0;}
            case 48:{const auto& root=c.roots[c.index];
                MoveWindow(root->handle,c.x,c.y,c.width,c.height,FALSE);
                return 0;}
            case 49:fit_children(c.roots[c.index]->handle);return 0;
            case 50:InvalidateRect(c.roots[c.index]->handle,nullptr,TRUE);
                return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_canvas_active_mode(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
void shutdown() {
    const auto op=[](void*,std::uint32_t id) noexcept -> std::uint64_t {
        if(id==60)g_ready.store(false,std::memory_order_release);
        if(id==61)g_mode={};
        return 0;
    };
    a2fo_rs_stage8_b_canvas_shutdown(nullptr,op);
}
BOOL copy_shell_canvas(HWND window, HDC destination, int capacity_width,
                       int capacity_height, int* width, int* height) {
    struct Context {
        HWND window;HDC destination;int capacity_width,capacity_height;
        int *width,*height;std::shared_ptr<Window> record{};
        Scale scale{};HDC source{};HGDIOBJ bitmap{};BITMAP details{};
        int object_size{},saved{};BOOL copied{},restored{};
        std::exception_ptr error{};
    } c{window,destination,capacity_width,capacity_height,width,height};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 70:return c.width!=nullptr;
        case 71:return c.height!=nullptr;
        case 72:*c.width=*c.height=0;return 0;
        case 73:c.record=scoped(c.window);return c.record!=nullptr;
        case 74:c.scale=local_scale(*c.record);return 0;
        case 75:return c.scale.valid();
        case 76:c.source=c.record->surface.cached(
            c.scale.logical_width,c.scale.logical_height);return 0;
        case 77:return c.source!=nullptr;
        case 78:*c.width=c.scale.logical_width;
            *c.height=c.scale.logical_height;return 0;
        case 79:return c.destination!=nullptr;
        case 80:return c.destination!=c.source;
        case 81:return c.capacity_width>=c.scale.logical_width;
        case 82:return c.capacity_height>=c.scale.logical_height;
        case 83:return GetObjectType(c.destination)==OBJ_MEMDC;
        case 84:c.bitmap=GetCurrentObject(c.destination,OBJ_BITMAP);return 0;
        case 85:return c.bitmap!=nullptr;
        case 86:c.object_size=GetObjectA(c.bitmap,sizeof(c.details),&c.details);
            return 0;
        case 87:return c.object_size==sizeof(c.details);
        case 88:return c.details.bmWidth>=c.capacity_width;
        case 89:return c.details.bmHeight>=c.capacity_height;
        case 90:c.saved=SaveDC(c.destination);return c.saved!=0;
        case 91:GdiFlush();return 0;
        case 92:c.copied=BitBlt(c.destination,0,0,c.scale.logical_width,
            c.scale.logical_height,c.source,0,0,SRCCOPY);return 0;
        case 93:c.restored=RestoreDC(c.destination,c.saved);return 0;
        case 94:return c.copied!=FALSE;
        case 95:return c.restored!=FALSE;
        case 96:GdiFlush();return 0;
        case 97:return c.copied&&c.restored;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const BOOL result=a2fo_rs_stage8_b_canvas_copy(&c,op);
    if(c.error)std::rethrow_exception(c.error);
    return result;
}

BOOL set_shell_canvas_presenter(HWND window,A2FO_ShellCanvasPresenterV1 presenter,
                                void* context) {
    struct Context {HWND window;A2FO_ShellCanvasPresenterV1 presenter;
        void* context;std::shared_ptr<Window> record{};std::exception_ptr error{};
    } c{window,presenter,context};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 110:c.record=scoped(c.window);return c.record!=nullptr;
        case 111:return c.presenter!=nullptr;
        case 112:return c.record->presenter_context==c.context;
        case 113:c.record->presenter=nullptr;return 0;
        case 114:c.record->presenter_context=nullptr;return 0;
        case 115:return c.record->presenter!=nullptr;
        case 116:return c.record->presenter==c.presenter;
        case 117:return c.record->presenter_context==c.context;
        case 118:c.record->presenter=c.presenter;return 0;
        case 119:c.record->presenter_context=c.context;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const BOOL result=a2fo_rs_stage8_b_canvas_presenter(&c,op);
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
} // namespace a2fo::shell_display::canvas

extern "C" __declspec(dllexport) BOOL __cdecl A2FO_SetShellCanvasSeedV1(
    A2FO_ShellCanvasSeedV1 seed, void* context) {
    using namespace a2fo::shell_display::canvas;
    g_seed = seed;
    g_seed_context = seed ? context : nullptr;
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL __cdecl A2FO_SetShellCanvasRefreshV1(
    A2FO_ShellCanvasRefreshV1 refresh, void* context) {
    using namespace a2fo::shell_display::canvas;
    g_refresh = refresh;
    g_refresh_context = refresh ? context : nullptr;
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL __cdecl A2FO_SetShellCanvasCompletionV1(
    A2FO_ShellCanvasCompletionV1 completion, void* context) {
    using namespace a2fo::shell_display::canvas;
    if(!g_ready.load(std::memory_order_acquire))return FALSE;
    g_completion=completion;
    g_completion_context=completion?context:nullptr;
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL __cdecl A2FO_CopyShellCanvasV1(
    HWND window, HDC destination, int capacity_width, int capacity_height,
    int* width, int* height) {
    try {
        return a2fo::shell_display::canvas::copy_shell_canvas(
            window, destination, capacity_width, capacity_height, width, height);
    } catch (...) {
        return FALSE;
    }
}

extern "C" __declspec(dllexport) BOOL __cdecl A2FO_SetShellCanvasPresenterV1(
    HWND window, A2FO_ShellCanvasPresenterV1 presenter, void* context) {
    try {
        return a2fo::shell_display::canvas::set_shell_canvas_presenter(
            window, presenter, context);
    } catch (...) {
        return FALSE;
    }
}

extern "C" __declspec(dllexport) BOOL __cdecl A2FO_ShellDialogBridgeReadyV1(HWND window) {
    using namespace a2fo::shell_display::canvas;
    try {
        const auto record=scoped(window);
        return record&&record->dialog_next&&
            GetWindowLongPtrA(window,DWLP_DLGPROC)==reinterpret_cast<LONG_PTR>(&game_dialog_proc);
    }catch(...){return FALSE;}
}

extern "C" __declspec(dllexport) HWND __cdecl A2FO_ShellCanvasWindowV1(HDC logical) {
    using namespace a2fo::shell_display::canvas;
    if(!logical)return nullptr;
    try {
        struct Context {HDC logical;decltype(g_leases)::reverse_iterator it;std::exception_ptr error{};};
        Context context{logical,g_leases.rbegin()};
        const auto step=+[](void* opaque,std::uint32_t operation,std::uintptr_t) noexcept -> std::uintptr_t {
            auto& c=*static_cast<Context*>(opaque);
            try {switch(operation){
            case 0:return c.it!=g_leases.rend();
            case 1:return c.it->logical==c.logical;
            case 2:return scoped(c.it->window->handle)==c.it->window;
            case 3:return reinterpret_cast<std::uintptr_t>(c.it->window->handle);
            case 4:++c.it;return 0;
            default:return 0;
            }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
        };
        const auto result=a2fo_rs_stage10_ui_lease(&context,step);
        if(context.error)std::rethrow_exception(context.error);
        return reinterpret_cast<HWND>(result);
    }catch(...){}
    return nullptr;
}
