// Win32 structure/call adapters; enumeration and placement policy is Rust.
#include "game_monitor.hpp"
#include "../rust/monitor_ffi.h"
#include <cstring>
#include <exception>
namespace a2fo { namespace {
struct Context{std::vector<GameMonitorDescriptor>* monitors{};std::string* value{};std::exception_ptr failure;};
void info(A2foRsMonitor& o,HMONITOR h,const MONITORINFOEXA& i){o.handle=reinterpret_cast<uintptr_t>(h);std::memcpy(o.name,i.szDevice,sizeof(i.szDevice));o.flags=i.dwFlags;o.left=i.rcMonitor.left;o.top=i.rcMonitor.top;o.width=i.rcMonitor.right-i.rcMonitor.left;o.height=i.rcMonitor.bottom-i.rcMonitor.top;o.work_left=i.rcWork.left;o.work_top=i.rcWork.top;o.work_right=i.rcWork.right;o.work_bottom=i.rcWork.bottom;}
int32_t action(void* p,uint32_t op,uintptr_t a,uintptr_t b,A2foRsMonitor* out,const char* s,const char* t){auto& c=*static_cast<Context*>(p);try{switch(op){
case 0:return c.failure?1:0;
case 1:case 2:{DISPLAY_DEVICEA d{};d.cb=sizeof(d);if(!EnumDisplayDevicesA(op==1?nullptr:s,static_cast<DWORD>(a),&d,0))return 0;std::memcpy(out->name,d.DeviceName,sizeof(d.DeviceName));std::memcpy(out->label,d.DeviceString,sizeof(d.DeviceString));out->flags=d.StateFlags;return 1;}
case 3:{MONITORINFOEXA i{};i.cbSize=sizeof(i);if(!GetMonitorInfoA(reinterpret_cast<HMONITOR>(a),&i))return 0;info(*out,reinterpret_cast<HMONITOR>(a),i);return 1;}
case 4:{DEVMODEA m{};m.dmSize=sizeof(m);if(!EnumDisplaySettingsExA(s,ENUM_CURRENT_SETTINGS,&m,0))return 0;out->left=m.dmPosition.x;out->top=m.dmPosition.y;out->width=static_cast<int>(m.dmPelsWidth);out->height=static_cast<int>(m.dmPelsHeight);return 1;}
case 5:{GameMonitorDescriptor m;m.device_name=out->name;m.friendly_name=out->label;m.adapter_index=out->index;m.left=out->left;m.top=out->top;m.width=out->width;m.height=out->height;m.primary=out->flags!=0;c.monitors->push_back(std::move(m));return 1;}
case 6:{char value[128]{};GetPrivateProfileStringA("Display","GameMonitorDevice","<A2FO-unset>",value,sizeof(value),s);std::memcpy(out->name,value,sizeof(value));return 1;}
case 7:*c.value=s;return 1;
case 8:return WritePrivateProfileStringA("Display","GameMonitorDevice",t,s)!=FALSE;
case 9:return IsWindow(reinterpret_cast<HWND>(a))!=FALSE;
case 10:{RECT q{};if(!GetWindowRect(reinterpret_cast<HWND>(a),&q))return 0;out->left=q.left;out->top=q.top;out->width=q.right-q.left;out->height=q.bottom-q.top;return 1;}
case 11:out->flags=(GetWindowLongPtrA(reinterpret_cast<HWND>(a),GWL_STYLE)&(WS_CAPTION|WS_THICKFRAME))!=0;return 1;
case 12:return SetWindowPos(reinterpret_cast<HWND>(a),nullptr,out->left,out->top,0,0,SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOOWNERZORDER|SWP_NOZORDER)!=FALSE;
case 13:return MonitorFromWindow(reinterpret_cast<HWND>(a),MONITOR_DEFAULTTONULL)==reinterpret_cast<HMONITOR>(b);
case 14:return _stricmp(s,t)==0;
}}catch(...){c.failure=std::current_exception();}return 0;}
struct Visit{A2foRsMonitorVisit callback;void* context;};
BOOL CALLBACK visit(HMONITOR h,HDC,LPRECT,LPARAM p){auto& v=*reinterpret_cast<Visit*>(p);return v.callback(v.context,reinterpret_cast<uintptr_t>(h));}
void enumerate(void*,A2foRsMonitorVisit callback,void* context){Visit v{callback,context};EnumDisplayMonitors(nullptr,nullptr,visit,reinterpret_cast<LPARAM>(&v));}
A2foRsMonitorApi bridge(Context& c){return {&c,action,enumerate};}
const uint8_t* data(const std::string& s){return reinterpret_cast<const uint8_t*>(s.data());}
void checked(Context& c){if(c.failure)std::rethrow_exception(c.failure);}
}
std::vector<GameMonitorDescriptor> enumerate_game_monitors(){std::vector<GameMonitorDescriptor> out;Context c;c.monitors=&out;const auto a=bridge(c);a2fo_rs_monitor_list(&a);checked(c);return out;}
bool read_configured_game_monitor(const std::string& root,std::string& value){value.clear();Context c;c.value=&value;const auto a=bridge(c);const bool ok=a2fo_rs_monitor_read(&a,data(root),root.size())!=0;checked(c);return ok;}
bool write_configured_game_monitor(const std::string& root,const std::string& name){Context c;const auto a=bridge(c);return a2fo_rs_monitor_write(&a,data(root),root.size(),data(name),name.size())!=0;}
GameMonitorResolution resolve_configured_game_monitor(const std::string& root,const std::vector<GameMonitorDescriptor>& monitors,int fallback,std::string* configured){std::string name;read_configured_game_monitor(root,name);if(configured)*configured=name;return resolve_game_monitor(monitors,name,fallback);}
bool position_window_on_game_monitor(HWND window,const GameMonitorDescriptor& monitor){Context c;const auto a=bridge(c);return a2fo_rs_monitor_window(&a,reinterpret_cast<uintptr_t>(window),data(monitor.device_name),monitor.device_name.size(),0)!=0;}
bool is_window_on_game_monitor(HWND window,const GameMonitorDescriptor& monitor){Context c;const auto a=bridge(c);return a2fo_rs_monitor_window(&a,reinterpret_cast<uintptr_t>(window),data(monitor.device_name),monitor.device_name.size(),1)!=0;}
}
