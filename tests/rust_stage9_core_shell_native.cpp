// Actual frozen/current whole core with every invoked USER32 entry mocked.
#include <windows.h>
#include <cstdio>
static BOOL WINAPI fake_WriteFile(HANDLE, const void*, DWORD, DWORD*, OVERLAPPED*);
static BOOL WINAPI fake_IsWindowVisible(HWND);
static BOOL WINAPI fake_IsWindow(HWND);
static DWORD WINAPI fake_GetWindowThreadProcessId(HWND, LPDWORD);
static int WINAPI fake_GetClassNameA(HWND, LPSTR, int);
static BOOL WINAPI fake_GetWindowRect(HWND, LPRECT);
static BOOL WINAPI fake_GetClientRect(HWND, LPRECT);
static HWND WINAPI fake_GetWindow(HWND, UINT);
static HWND WINAPI fake_GetParent(HWND);
static HWND WINAPI fake_GetForegroundWindow();
static DWORD WINAPI fake_GetCurrentProcessId();
static BOOL WINAPI fake_EnumWindows(WNDENUMPROC, LPARAM);
static BOOL WINAPI fake_EnumChildWindows(HWND, WNDENUMPROC, LPARAM);
static BOOL WINAPI fake_EnumDisplayMonitors(HDC, LPCRECT, MONITORENUMPROC, LPARAM);
static HMONITOR WINAPI fake_MonitorFromWindow(HWND, DWORD);
static int WINAPI fake_GetWindowTextA(HWND, LPSTR, int);
static LONG WINAPI fake_GetWindowLongA(HWND, int);
static LONG WINAPI fake_SetWindowLongA(HWND, int, LONG);
static BOOL WINAPI fake_GetMonitorInfoA(HMONITOR, LPMONITORINFO);
static BOOL WINAPI fake_SetWindowPos(HWND, HWND, int, int, int, int, UINT);
static BOOL WINAPI fake_AdjustWindowRectEx(LPRECT, DWORD, BOOL, DWORD);
static void WINAPI fake_Sleep(DWORD);
#define WriteFile fake_WriteFile
#define IsWindowVisible fake_IsWindowVisible
#define IsWindow fake_IsWindow
#define GetWindowThreadProcessId fake_GetWindowThreadProcessId
#define GetClassNameA fake_GetClassNameA
#define GetWindowRect fake_GetWindowRect
#define GetClientRect fake_GetClientRect
#define GetWindow fake_GetWindow
#define GetParent fake_GetParent
#define GetForegroundWindow fake_GetForegroundWindow
#define GetCurrentProcessId fake_GetCurrentProcessId
#define EnumWindows fake_EnumWindows
#define EnumChildWindows fake_EnumChildWindows
#define EnumDisplayMonitors fake_EnumDisplayMonitors
#define MonitorFromWindow fake_MonitorFromWindow
#define GetWindowTextA fake_GetWindowTextA
#define GetWindowLongA fake_GetWindowLongA
#define SetWindowLongA fake_SetWindowLongA
#define GetMonitorInfoA fake_GetMonitorInfoA
#define SetWindowPos fake_SetWindowPos
#define AdjustWindowRectEx fake_AdjustWindowRectEx
#define Sleep fake_Sleep
#include CORE_SOURCE
#undef WriteFile
#undef IsWindowVisible
#undef IsWindow
#undef GetWindowThreadProcessId
#undef GetClassNameA
#undef GetWindowRect
#undef GetClientRect
#undef GetWindow
#undef GetParent
#undef GetForegroundWindow
#undef GetCurrentProcessId
#undef EnumWindows
#undef EnumChildWindows
#undef EnumDisplayMonitors
#undef MonitorFromWindow
#undef GetWindowTextA
#undef GetWindowLongA
#undef SetWindowLongA
#undef GetMonitorInfoA
#undef SetWindowPos
#undef AdjustWindowRectEx
#undef Sleep
#include <cstdlib>
static unsigned tick, fail_at, throw_at, mutate_at, mode, sleeps;
static int live;
static DWORD style_value, ex_value;
static RECT current_rect;
static ShellDisplayRuntime* active_runtime;
static MONITORINFO* active_monitor;
static ArmadaWindowSearch* active_search;
static bool walking;
struct Failure : std::exception {
    unsigned value;
    explicit Failure(unsigned v):value(v){++live;}
    Failure(const Failure& o) noexcept:value(o.value){++live;}
    ~Failure() noexcept override {--live;std::printf("destroy:%u:live=%d\n",value,live);}
};
static bool event(const char* name) {
    ++tick; std::printf("call:%u:%s:live=%d\n",tick,name,live);
    if(tick==mutate_at){
        std::puts("mutation");
        if(active_runtime){active_runtime->mode_applied=!active_runtime->mode_applied;active_runtime->windowed_style=WS_CHILD;active_runtime->windowed_extended_style=9;active_runtime->window_tree_logs=8;active_runtime->settings.width=777;}
        if(active_monitor){active_monitor->rcMonitor={-123,-27,700,511};active_monitor->rcWork={-71,19,641,419};}
        if(active_search){active_search->window=reinterpret_cast<HWND>(7);active_search->score=~std::uint64_t{};}
    }
    if(tick==throw_at) throw Failure{tick};
    return tick!=fail_at;
}
static unsigned id(const void* value){return static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(value));}
static BOOL WINAPI fake_WriteFile(HANDLE,const void* data,DWORD size,DWORD* written,OVERLAPPED*) {
    event("log");std::fwrite(data,1,size,stdout);if(written)*written=size;return TRUE;
}
static BOOL WINAPI fake_IsWindowVisible(HWND w){std::printf("visible:%u\n",id(w));return event("visible");}
static BOOL WINAPI fake_IsWindow(HWND w){std::printf("iswindow:%u\n",id(w));return event("iswindow");}
static DWORD WINAPI fake_GetWindowThreadProcessId(HWND w,LPDWORD out){bool ok=event("pid");if(out)*out=ok&&(id(w)!=3||mode==1)?42:77;return 99;}
static int WINAPI fake_GetClassNameA(HWND w,LPSTR out,int count){bool ok=event("class");const char* text=(id(w)==2&&mode!=1)?"Other":"TMainWindowForm";std::snprintf(out,count,"%s",text);return ok?static_cast<int>(std::strlen(out)):0;}
static BOOL WINAPI fake_GetWindowRect(HWND w,LPRECT out){std::printf("rect:%u\n",id(w));bool ok=event("rect");*out=current_rect;if(mode==1){out->right+=static_cast<LONG>(id(w)*13);out->bottom+=static_cast<LONG>(id(w)*7);}return ok;}
static BOOL WINAPI fake_GetClientRect(HWND,LPRECT out){bool ok=event("client");*out={0,0,300,200};return ok;}
static HWND WINAPI fake_GetWindow(HWND w,UINT code){std::printf("owner:%u:%u\n",id(w),code);event("owner");return id(w)==1?nullptr:reinterpret_cast<HWND>(8);}
static HWND WINAPI fake_GetParent(HWND){event("parent");return nullptr;}
static HWND WINAPI fake_GetForegroundWindow(){event("foreground");return mode==2?nullptr:reinterpret_cast<HWND>(mode==1?2:1);}
static DWORD WINAPI fake_GetCurrentProcessId(){event("current-pid");return 42;}
static BOOL WINAPI fake_EnumWindows(WNDENUMPROC proc,LPARAM p){event("enum-windows");if(walking)return TRUE;walking=true;for(unsigned i=1;i<=3;++i)if(!proc(reinterpret_cast<HWND>(i),p))break;walking=false;return TRUE;}
static BOOL WINAPI fake_EnumChildWindows(HWND w,WNDENUMPROC proc,LPARAM p){event("enum-children");proc(w,p);return TRUE;}
static BOOL WINAPI fake_EnumDisplayMonitors(HDC,LPCRECT,MONITORENUMPROC proc,LPARAM p){event("enum-monitors");RECT rect{};for(unsigned i=1;i<=2;++i)if(!proc(reinterpret_cast<HMONITOR>(i),nullptr,&rect,p))break;return TRUE;}
static HMONITOR WINAPI fake_MonitorFromWindow(HWND,DWORD flags){std::printf("nearest:%lu\n",static_cast<unsigned long>(flags));event("nearest");return reinterpret_cast<HMONITOR>(9);}
static int WINAPI fake_GetWindowTextA(HWND,LPSTR out,int count){event("title");std::snprintf(out,count,"Armada");return 6;}
static LONG WINAPI fake_GetWindowLongA(HWND w,int index){std::printf("getstyle:%u:%d\n",id(w),index);event("getstyle");return static_cast<LONG>(index==GWL_STYLE?style_value:ex_value);}
static LONG WINAPI fake_SetWindowLongA(HWND w,int index,LONG value){std::printf("setstyle:%u:%d:%08lx\n",id(w),index,static_cast<unsigned long>(value));bool ok=event("setstyle");if(!ok){SetLastError(123);return 0;}DWORD& slot=index==GWL_STYLE?style_value:ex_value;LONG old=static_cast<LONG>(slot);slot=static_cast<DWORD>(value);return old;}
static BOOL WINAPI fake_GetMonitorInfoA(HMONITOR h,LPMONITORINFO out){std::printf("monitor-info:%u:%lu\n",id(h),static_cast<unsigned long>(out->cbSize));bool ok=event("monitor-info");out->rcMonitor={-1920,0,0,1080};out->rcWork={-1920,27,0,1040};return ok;}
static BOOL WINAPI fake_SetWindowPos(HWND w,HWND after,int left,int top,int width,int height,UINT flags){std::printf("position:%u:%u:%d:%d:%d:%d:%u\n",id(w),id(after),left,top,width,height,flags);bool ok=event("position");if(ok)current_rect={left,top,left+width,top+height};return ok;}
static BOOL WINAPI fake_AdjustWindowRectEx(LPRECT rect,DWORD style,BOOL menu,DWORD ex){std::printf("adjust:%ld:%ld:%08lx:%d:%08lx\n",rect->right,rect->bottom,static_cast<unsigned long>(style),menu,static_cast<unsigned long>(ex));bool ok=event("adjust");rect->left-=8;rect->top-=30;rect->right+=8;rect->bottom+=8;return ok;}
static void WINAPI fake_Sleep(DWORD ms){std::printf("sleep:%lu\n",static_cast<unsigned long>(ms));event("sleep");if(++sleeps==2)throw Failure{999};}
static void reset(){tick=0;sleeps=0;walking=false;style_value=WS_CAPTION|WS_VISIBLE;ex_value=WS_EX_WINDOWEDGE;current_rect={20,30,820,630};active_runtime=nullptr;active_monitor=nullptr;active_search=nullptr;}
static void invoke(unsigned group,ShellDisplayRuntime& r,MONITORINFO& monitor){
    const auto window=reinterpret_cast<HWND>(1);
    ShellDisplaySettings settings{1,mode==1?639:1281,mode==1?479:721,true};
    switch(group){
    case 0:std::printf("result:%u\n",apply_shell_fullscreen(r,window,monitor));break;
    case 1:std::printf("result:%u\n",apply_shell_windowed_size(r,window,monitor,settings));break;
    case 2:poll_shell_display(r);break;
    case 3:{ArmadaWindowSearch search{42,nullptr,0};active_search=&search;for(unsigned i=1;i<=3;++i)find_armada_window(reinterpret_cast<HWND>(i),reinterpret_cast<LPARAM>(&search));std::printf("search:%u:%016llx\n",id(search.window),static_cast<unsigned long long>(search.score));active_search=nullptr;break;}
    case 4:std::printf("window:%u\n",id(get_armada_window()));break;
    case 5:std::printf("monitor:%u\n",id(get_configured_monitor(window,mode==1?9:1)));break;
    default:run_shell_display_monitor();break;
    }
}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    g_log=reinterpret_cast<HANDLE>(1);
    alignas(int) std::uint8_t foreign[0x80]{};
    *reinterpret_cast<int*>(foreign+0x30)=1;*reinterpret_cast<int*>(foreign+0x34)=1281;*reinterpret_cast<int*>(foreign+0x38)=721;foreign[0x72]=1;
    g_fo_settings_instance=foreign;
    for(unsigned group=0;group<7;++group)for(mode=0;mode<3;++mode)for(unsigned variant=0;variant<4;++variant)for(unsigned at=1;at<=24;++at){
        if(variant==0&&at!=1)continue;
        reset();fail_at=variant==1?at:0;throw_at=variant==2?at:0;mutate_at=variant==3?at:0;
        ShellDisplayRuntime runtime;runtime.windowed_style=WS_CAPTION|WS_VISIBLE;runtime.windowed_extended_style=WS_EX_WINDOWEDGE;
        if(mode==2){runtime.settings_logged=true;runtime.settings={1,1281,721,true};runtime.window=reinterpret_cast<HWND>(1);runtime.window_styles_captured=true;runtime.window_tree_logs=4;runtime.mode_applied=true;}
        MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);monitor.rcMonitor={-1920,0,0,1080};monitor.rcWork={-1920,27,0,1040};
        active_runtime=&runtime;active_monitor=&monitor;
        std::printf("case:%u:%u:%u:%u\n",group,mode,variant,at);
        try{invoke(group,runtime,monitor);}catch(const Failure& error){std::printf("caught:%u:live=%d\n",error.value,live);}catch(...){std::puts("wrong-exception");return 2;}
        std::printf("state:%u:%08lx:%08lx:%u:%u:%u:%u:%d:%d:%d:%d:%u\n",id(runtime.window),static_cast<unsigned long>(runtime.windowed_style),static_cast<unsigned long>(runtime.windowed_extended_style),runtime.window_styles_captured,runtime.settings_logged,runtime.mode_applied,runtime.apply_failure_logged,runtime.window_tree_logs,runtime.settings.display,runtime.settings.width,runtime.settings.height,runtime.settings.windowed);
        if(live)return 3;
    }
    for(int value:{-1,0,31,32,16384,16385}){
        *reinterpret_cast<int*>(foreign+0x30)=value;ShellDisplaySettings settings;
        std::printf("settings:%d:%u\n",value,read_shell_display_settings(settings));
    }
    g_fo_settings_instance=nullptr;ShellDisplaySettings settings;
    std::printf("null-settings:%u:null-search:%d:null-monitor:%d\n",read_shell_display_settings(settings),find_armada_window(nullptr,0),find_armada_monitor(nullptr,nullptr,nullptr,0));
    std::puts("core shell fixture: PASS");
}
