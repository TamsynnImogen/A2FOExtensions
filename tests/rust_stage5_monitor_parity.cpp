#include "../build/rust-migration-stage5/monitor-work/mock_monitor.hpp"
#include <cstdio>
#include <cctype>
#include <utility>
namespace {
struct Child {std::string name,label;DWORD flags=0;};
struct Adapter {std::string name,label;DWORD flags=0;std::vector<Child> children;
 bool mode_ok=false;int x=0,y=0,width=0,height=0;};
struct Monitor {std::uintptr_t handle=0;std::string name;RECT full{},work{};
 DWORD flags=0;bool info_ok=true;};
struct Fixture {std::vector<Adapter> adapters;std::vector<Monitor> monitors;
 std::vector<std::string> calls;std::string config="<A2FO-unset>";
 bool write_ok=true,window_valid=true,rect_ok=true,setpos_ok=true;
 RECT window_rect{0,0,640,480};LONG_PTR style=0;std::uintptr_t current_monitor=1;
} g;
std::string safe(const char *p) {return p?p:"<null>";}
void log(const std::string &s) {g.calls.push_back(s);}
void reset() {g=Fixture{};}
void dump(const char *scenario) {
 std::printf("scenario %s\n",scenario);
 for(const auto &call:g.calls)std::printf("call %s\n",call.c_str());
 g.calls.clear();
}
void descriptors(const std::vector<a2fo::GameMonitorDescriptor> &list) {
 std::printf("monitors %zu\n",list.size());
 for(const auto &m:list)
  std::printf("monitor %s | %s | %d %d %d %d %d %d\n",m.device_name.c_str(),
   m.friendly_name.c_str(),m.adapter_index,m.left,m.top,m.width,m.height,m.primary);
}
void bool_result(const char *tag,bool ok) {std::printf("result %s %d\n",tag,ok);}
void ordinary() {
 reset();
 g.adapters={{"\\\\.\\DISPLAY1","Adapter One",DISPLAY_DEVICE_ATTACHED_TO_DESKTOP|DISPLAY_DEVICE_PRIMARY_DEVICE,
   {{"child0","",0},{"child1","Mirror",DISPLAY_DEVICE_MIRRORING_DRIVER},{"child2","Panel One",0}}},
  {"\\\\.\\DISPLAY2","Adapter Two",DISPLAY_DEVICE_ATTACHED_TO_DESKTOP,{},true,-1600,50,1600,900},
  {"DETACHED","Hidden",0,{}},
  {"MIRROR","Mirror",DISPLAY_DEVICE_ATTACHED_TO_DESKTOP|DISPLAY_DEVICE_MIRRORING_DRIVER,{}},
  {"","Empty",DISPLAY_DEVICE_ATTACHED_TO_DESKTOP,{}}};
 g.monitors={{1,"\\\\.\\DISPLAY1",{0,0,1920,1080},{0,0,1920,1040},MONITORINFOF_PRIMARY,true},
   {2,"MISSING",{-1600,50,0,950},{-1600,50,0,900},0,true}};
 auto list=a2fo::enumerate_game_monitors();descriptors(list);dump("ordinary");
 std::string configured="previous";
 bool_result("read_empty_root",a2fo::read_configured_game_monitor("",configured));
 std::printf("configured %s\n",configured.c_str());dump("read_empty_root");
 bool_result("read_sentinel",a2fo::read_configured_game_monitor("C:\\Data",configured));
 std::printf("configured %s\n",configured.c_str());dump("read_sentinel");
 g.config="";
 bool_result("read_blank",a2fo::read_configured_game_monitor("C:\\Data/",configured));
 std::printf("configured %s\n",configured.c_str());dump("read_blank");
 g.config="\\\\.\\display2";
 bool_result("read_value",a2fo::read_configured_game_monitor("C:\\Data",configured));
 std::printf("configured %s\n",configured.c_str());dump("read_value");
  g.config="<a2fo-unset>";
  bool_result("read_case_sentinel",a2fo::read_configured_game_monitor("C:\\Data\\",configured));
  std::printf("configured %s\n",configured.c_str());dump("read_case_sentinel");
  g.config="\\\\.\\display2";
 bool_result("write_empty_root",a2fo::write_configured_game_monitor("","X"));dump("write_empty_root");
 bool_result("write_empty_name",a2fo::write_configured_game_monitor("C:\\Data",""));dump("write_empty_name");
 bool_result("write_ok",a2fo::write_configured_game_monitor("C:\\Data/","\\\\.\\DISPLAY1"));dump("write_ok");
 g.write_ok=false;
 bool_result("write_fail",a2fo::write_configured_game_monitor("C:\\Data","\\\\.\\DISPLAY2"));dump("write_fail");
 std::string chosen;
 auto resolved=a2fo::resolve_configured_game_monitor("C:\\Data",list,3,&chosen);
 std::printf("resolution %d %d %d chosen=%s\n",resolved.monitor_index,
  resolved.configured,resolved.configured_device_present,chosen.c_str());dump("resolve");
 resolved=a2fo::resolve_configured_game_monitor("",list,-1,nullptr);
 std::printf("resolution2 %d %d %d\n",resolved.monitor_index,
  resolved.configured,resolved.configured_device_present);dump("resolve_empty");
 auto target=list.front();
 g.style=WS_CAPTION|WS_THICKFRAME;
 bool_result("framed",a2fo::position_window_on_game_monitor(reinterpret_cast<HWND>(9),target));dump("framed");
 g.style=0;
 bool_result("unframed",a2fo::position_window_on_game_monitor(reinterpret_cast<HWND>(9),target));dump("unframed");
  g.monitors.push_back({6,"OFFSCREEN",{-1600,-900,0,0},{-1550,-850,-50,-40},0,true});
  target.device_name="offscreen";g.style=WS_CAPTION;
  bool_result("offscreen_framed",a2fo::position_window_on_game_monitor(reinterpret_cast<HWND>(9),target));dump("offscreen_framed");
  target.device_name=list.front().device_name;
 g.style=WS_CAPTION;g.window_rect={0,0,2500,1800};
 bool_result("oversized",a2fo::position_window_on_game_monitor(reinterpret_cast<HWND>(9),target));dump("oversized");
 g.window_rect={10,20,10,120};
 bool_result("zero_width",a2fo::position_window_on_game_monitor(reinterpret_cast<HWND>(9),target));dump("zero_width");
 g.window_rect={0,0,640,480};g.rect_ok=false;
 bool_result("no_rect",a2fo::position_window_on_game_monitor(reinterpret_cast<HWND>(9),target));dump("no_rect");
 g.rect_ok=true;g.setpos_ok=false;
 bool_result("setpos_fail",a2fo::position_window_on_game_monitor(reinterpret_cast<HWND>(9),target));dump("setpos_fail");
 g.setpos_ok=true;g.window_valid=false;
 bool_result("invalid_window",a2fo::position_window_on_game_monitor(reinterpret_cast<HWND>(9),target));dump("invalid_window");
 bool_result("null_window",a2fo::position_window_on_game_monitor(nullptr,target));dump("null_window");
  bool_result("null_window_on",a2fo::is_window_on_game_monitor(nullptr,target));dump("null_window_on");
 g.window_valid=true;
 bool_result("on_monitor",a2fo::is_window_on_game_monitor(reinterpret_cast<HWND>(9),target));dump("on_monitor");
 g.current_monitor=2;
 bool_result("off_monitor",a2fo::is_window_on_game_monitor(reinterpret_cast<HWND>(9),target));dump("off_monitor");
 target.device_name="UNKNOWN";
 bool_result("missing_monitor",a2fo::position_window_on_game_monitor(reinterpret_cast<HWND>(9),target));dump("missing_monitor");
 bool_result("missing_monitor_on",a2fo::is_window_on_game_monitor(reinterpret_cast<HWND>(9),target));dump("missing_monitor_on");
}
void fallback() {
 reset();
 g.adapters={{"DETACHED","Hidden",0,{}},{"MIRROR","Mirror",DISPLAY_DEVICE_MIRRORING_DRIVER,{}}};
 g.monitors={{3,"FALLBACK-A",{-1920,0,0,1080},{-1920,0,0,1040},0,true},
  {4,"BROKEN",{0,0,100,100},{0,0,100,100},0,false},
  {5,"FALLBACK-B",{0,0,800,600},{0,0,800,580},MONITORINFOF_PRIMARY,true}};
 auto list=a2fo::enumerate_game_monitors();descriptors(list);dump("fallback");
 reset();list=a2fo::enumerate_game_monitors();descriptors(list);dump("empty");
 reset();g.adapters={{"ADAPTER","No mode",DISPLAY_DEVICE_ATTACHED_TO_DESKTOP,{}}};
 list=a2fo::enumerate_game_monitors();descriptors(list);dump("no_geometry");
  reset();g.adapters={{"SECONDARY","Second",DISPLAY_DEVICE_ATTACHED_TO_DESKTOP,{}}};
  g.monitors={{7,"SECONDARY",{-800,0,0,600},{-800,0,0,570},MONITORINFOF_PRIMARY,true}};
  list=a2fo::enumerate_game_monitors();descriptors(list);dump("info_primary");
  reset();
  for(unsigned i=0;i<65;++i)
   g.adapters.push_back({"ADAPTER"+std::to_string(i),"Device"+std::to_string(i),
     DISPLAY_DEVICE_ATTACHED_TO_DESKTOP,{}});
  list=a2fo::enumerate_game_monitors();
  std::printf("cap64 %zu first=%s last=%s\n",list.size(),
    list.empty()?"":list.front().device_name.c_str(),
    list.empty()?"":list.back().device_name.c_str());
  dump("cap64");
}
}
BOOL EnumDisplayDevicesA(const char *name,DWORD index,DISPLAY_DEVICEA *out,DWORD flags) {
 log("EnumDisplayDevices name="+safe(name)+" index="+std::to_string(index)+" flags="+std::to_string(flags));
 if(!name) {
  if(index>=g.adapters.size())return FALSE;
  const auto &a=g.adapters[index];
  std::strncpy(out->DeviceName,a.name.c_str(),sizeof out->DeviceName-1);
  std::strncpy(out->DeviceString,a.label.c_str(),sizeof out->DeviceString-1);
  out->StateFlags=a.flags;return TRUE;
 }
 for(const auto &a:g.adapters)if(a.name==name) {
  if(index>=a.children.size())return FALSE;
  const auto &c=a.children[index];
  std::strncpy(out->DeviceName,c.name.c_str(),sizeof out->DeviceName-1);
  std::strncpy(out->DeviceString,c.label.c_str(),sizeof out->DeviceString-1);
  out->StateFlags=c.flags;return TRUE;
 }
 return FALSE;
}
BOOL GetMonitorInfoA(HMONITOR handle,MONITORINFOEXA *out) {
 auto key=reinterpret_cast<std::uintptr_t>(handle);
 log("GetMonitorInfo handle="+std::to_string(key));
 for(const auto &m:g.monitors)if(m.handle==key&&m.info_ok) {
  out->rcMonitor=m.full;out->rcWork=m.work;out->dwFlags=m.flags;
  std::strncpy(out->szDevice,m.name.c_str(),sizeof out->szDevice-1);
  return TRUE;
 }
 return FALSE;
}
BOOL EnumDisplaySettingsExA(const char *name,DWORD setting,DEVMODEA *out,DWORD flags) {
 log("EnumDisplaySettings name="+safe(name)+" setting="+std::to_string(setting)+" flags="+std::to_string(flags));
 for(const auto &a:g.adapters)if(a.name==name&&a.mode_ok) {
  out->dmPosition={a.x,a.y};out->dmPelsWidth=a.width;out->dmPelsHeight=a.height;return TRUE;
 }
 return FALSE;
}
BOOL EnumDisplayMonitors(HDC,const RECT*,MonitorProc callback,LPARAM parameter) {
 log("EnumDisplayMonitors");
 for(const auto &m:g.monitors) {
  const BOOL keep=callback(reinterpret_cast<HMONITOR>(m.handle),nullptr,nullptr,parameter);
  log("MonitorVisit handle="+std::to_string(m.handle)+" keep="+std::to_string(keep));
  if(!keep)break;
 }
 return TRUE;
}
DWORD GetPrivateProfileStringA(const char *section,const char *key,const char *default_value,
 char *value,DWORD capacity,const char *path) {
 log("GetPrivateProfileString section="+safe(section)+" key="+safe(key)+
  " default="+safe(default_value)+" path="+safe(path)+" capacity="+std::to_string(capacity));
 const auto &source=g.config;
 if(capacity) {
  auto n=std::min<std::size_t>(source.size(),capacity-1);
  std::memcpy(value,source.data(),n);value[n]=0;
  return static_cast<DWORD>(n);
 }
 return 0;
}
BOOL WritePrivateProfileStringA(const char *section,const char *key,const char *value,const char *path) {
 log("WritePrivateProfileString section="+safe(section)+" key="+safe(key)+
  " value="+safe(value)+" path="+safe(path));
 return g.write_ok;
}
BOOL IsWindow(HWND window) {log("IsWindow window="+std::to_string(reinterpret_cast<std::uintptr_t>(window)));return g.window_valid;}
BOOL GetWindowRect(HWND window,RECT *out) {
 log("GetWindowRect window="+std::to_string(reinterpret_cast<std::uintptr_t>(window)));
 if(g.rect_ok)*out=g.window_rect;
 return g.rect_ok;
}
LONG_PTR GetWindowLongPtrA(HWND window,int index) {
 log("GetWindowLongPtr window="+std::to_string(reinterpret_cast<std::uintptr_t>(window))+
  " index="+std::to_string(index));return g.style;
}
BOOL SetWindowPos(HWND window,HWND insert,int left,int top,int width,int height,DWORD flags) {
 log("SetWindowPos window="+std::to_string(reinterpret_cast<std::uintptr_t>(window))+
  " insert="+std::to_string(reinterpret_cast<std::uintptr_t>(insert))+
  " rect="+std::to_string(left)+","+std::to_string(top)+","+
  std::to_string(width)+","+std::to_string(height)+" flags="+std::to_string(flags));
 return g.setpos_ok;
}
HMONITOR MonitorFromWindow(HWND window,DWORD flags) {
 log("MonitorFromWindow window="+std::to_string(reinterpret_cast<std::uintptr_t>(window))+
  " flags="+std::to_string(flags));return reinterpret_cast<HMONITOR>(g.current_monitor);
}
int mock_stricmp(const char *a,const char *b) {
 log("stricmp "+safe(a)+" | "+safe(b));
 while(*a&&*b) {
  const int left=std::tolower(static_cast<unsigned char>(*a));
  const int right=std::tolower(static_cast<unsigned char>(*b));
  if(left!=right)return left-right;
  ++a;++b;
 }
 return static_cast<unsigned char>(*a)-static_cast<unsigned char>(*b);
}
namespace a2fo {
GameMonitorResolution resolve_game_monitor(const std::vector<GameMonitorDescriptor>& monitors,
 const std::string &configured,int fallback) {
 log("resolve configured="+configured+" fallback="+std::to_string(fallback)+
  " count="+std::to_string(monitors.size()));
 return {fallback, !configured.empty(),
  std::any_of(monitors.begin(),monitors.end(),[&](const auto &m){return m.device_name==configured;})};
}
}
int main() {ordinary();fallback();}
