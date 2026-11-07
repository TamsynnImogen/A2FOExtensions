#include "../build/rust-migration-stage5/backdrop-work/mock_backdrop.hpp"
#include "../modules/A2FORefitMenus/loading_bindings.hpp"
#include <cstdio>
#include <map>
#include <set>
using a2fo::refit_menu::Artwork;
namespace backdrop=a2fo::refit_menu::mission_backdrop;
namespace bind=a2fo::refit_menu::loading_bindings;
namespace {
std::map<std::uintptr_t,BYTE> memory;
std::set<std::uintptr_t> executable;
std::vector<std::string> calls;
std::uintptr_t engine_base=0,fo_base=0x40000000,chat=0x61000000;
std::uintptr_t storm=0x51000000,wrapper=0x52000000,table=0x53000000;
std::uintptr_t unknown=0x54000000,unknown_table=0x55000000,gateway=0x56000000;
std::uintptr_t patch_hook=0;
bool patch_ok=true,query_ok=true,coop_ok=true,display_ok=true,creation_ok=true;
bool surface_ok=true,front_ok=true,client_ok=true,screen_ok=true,d3d_ok=true;
bool monitor_ok=true,lock_ok=true,pixels_present=true,release_ok=true;
UINT display_width=8,display_height=6,adapter_ordinal=2;
RECT client_rect{0,0,3,2};POINT screen_origin{11,21};RECT monitor_rect{10,20,18,26};
int forced_pitch=0;DWORD thread_id=41;std::vector<std::uint32_t> front_pixels;
IDirect3DSurface8 surface;IDirect3D8 d3d;IDirect3DDevice8 device;
void __fastcall original_render(void*,void*) {calls.push_back("original_render");}
HRESULT __stdcall query_device(void*,REFIID iid,void** out) {
 calls.push_back("QueryInterface iid="+std::to_string(iid.a));
 *out=query_ok?&device:nullptr;return query_ok?S_OK:E_FAIL;
}
std::string address(std::uintptr_t p) {
 if(p==reinterpret_cast<std::uintptr_t>(&original_render))return "RENDER";
 if(p==reinterpret_cast<std::uintptr_t>(&query_device))return "QUERY";
 if(p==storm)return "STORM";
 if(p==wrapper)return "WRAPPER";
 if(p==table)return "TABLE";
 if(p==unknown)return "UNKNOWN";
 if(p==unknown_table)return "UNKNOWN_TABLE";
 if(p==gateway)return "GATEWAY";
 if(p>=engine_base&&p-engine_base<0x400000)return "ARMADA+"+std::to_string(p-engine_base);
 if(p>=fo_base&&p-fo_base<0x300000)return "FO+"+std::to_string(p-fo_base);
 return "ADDR";
}
void put(std::uintptr_t p,const void *data,std::size_t n) {
 auto *bytes=static_cast<const BYTE*>(data);
 for(std::size_t i=0;i<n;++i)memory[p+i]=bytes[i];
}
template<class T>void put_value(std::uintptr_t p,T value) {put(p,&value,sizeof(value));}
void fill_front() {
 front_pixels.resize(std::size_t(display_width)*display_height);
 for(UINT y=0;y<display_height;++y)
  for(UINT x=0;x<display_width;++x)
   front_pixels[std::size_t(y)*display_width+x]=0xff000000u|(y<<8)|x;
}
void prepare_memory() {
 memory.clear();executable.clear();
 engine_base=reinterpret_cast<std::uintptr_t>(&original_render)-0x11d3c0;
 const BYTE receiver[]{0x8b,0x0d,0xe8,0x8d,0x76,0,0x85,0xc9,0x74,5};
 const BYTE creation[]{0x89,0x35,0xe8,0x8d,0x76,0};
 const BYTE deletion[]{0xc7,5,0xe8,0x8d,0x76,0,0,0,0,0};
 const BYTE prologue[]{0x55,0x8b,0xec,0x83,0xec,0x18};
 const BYTE call[]{0xe8,0xa5,0x49,0xf8,0xff};
 put(engine_base+0x198a0c,receiver,sizeof receiver);
 put(engine_base+0x8587a,creation,sizeof creation);
 put(engine_base+0x11d82b,deletion,sizeof deletion);
 put(engine_base+0x11d3c0,prologue,sizeof prologue);
 put(engine_base+0x198a16,call,sizeof call);
 put(engine_base+bind::flush_site,bind::flush_site_code.data(),bind::flush_site_code.size());
 put(engine_base+bind::device_flush,bind::flush_body.data(),bind::flush_body.size());
 put_value<std::uint32_t>(engine_base+0x368de8,static_cast<std::uint32_t>(chat));
 put_value<std::uint32_t>(engine_base+0x3ad508,static_cast<std::uint32_t>(storm));
 put_value<std::uint32_t>(storm+0xc0,0);
 put_value<std::uint32_t>(storm+0xcc,static_cast<std::uint32_t>(wrapper));
 put_value<std::uint32_t>(wrapper,static_cast<std::uint32_t>(table));
 put_value<std::uint32_t>(table+0x38,static_cast<std::uint32_t>(engine_base+bind::device_flush));
 put_value<std::uint32_t>(wrapper+0x90,static_cast<std::uint32_t>(unknown));
 put_value<std::uint32_t>(unknown,static_cast<std::uint32_t>(unknown_table));
 put_value<std::uint32_t>(unknown_table,static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&query_device)));
 executable.insert(reinterpret_cast<std::uintptr_t>(&query_device));
 fill_front();
}
void prepare_fo_chain() {
 BYTE transfer[]{0x68,0,0,0,0,0xc3};
 auto callback=static_cast<std::uint32_t>(fo_base+0x1cf0b4);
 std::memcpy(transfer+1,&callback,4);
 put(engine_base+0x11d3c0,transfer,sizeof transfer);
 BYTE expected[]{0x83,0x3d,0,0,0,0,0,0x75,0x06,0xff,0x15,0,0,0,0,0xc3};
 auto counter=static_cast<std::uint32_t>(fo_base+0x20fff4);
 auto next=static_cast<std::uint32_t>(fo_base+0x2469d4);
 std::memcpy(expected+2,&counter,4);std::memcpy(expected+11,&next,4);
 put(callback,expected,sizeof expected);
 put_value<std::uint32_t>(next,static_cast<std::uint32_t>(gateway));
 BYTE code[]{0x55,0x8b,0xec,0x83,0xec,0x18,0x68,0,0,0,0,0xc3};
 auto resume=static_cast<std::uint32_t>(engine_base+0x11d3c6);
 std::memcpy(code+7,&resume,4);put(gateway,code,sizeof code);
 executable.insert(callback);executable.insert(callback+15);
 executable.insert(gateway);executable.insert(gateway+11);
}
void reset_settings() {
 patch_ok=query_ok=coop_ok=display_ok=creation_ok=true;
 surface_ok=front_ok=client_ok=screen_ok=d3d_ok=monitor_ok=true;
 lock_ok=pixels_present=release_ok=true;
 display_width=8;display_height=6;adapter_ordinal=2;
 client_rect={0,0,3,2};screen_origin={11,21};monitor_rect={10,20,18,26};
 forced_pitch=0;thread_id=41;chat=0x61000000;patch_hook=0;
 prepare_memory();
}
void dump(const char *scenario) {
 std::printf("scenario %s\n",scenario);
 for(const auto &call:calls)std::printf("call %s\n",call.c_str());
 calls.clear();
}
void frame_result(const char *scenario) {
 auto image=backdrop::frame();
 if(image) {
  std::printf("frame %s %d %d %zu",scenario,image->width,image->height,image->pixels.size());
  for(auto pixel:image->pixels)std::printf(" %08x",pixel);
  std::putchar('\n');
 } else std::printf("frame %s null\n",scenario);
 dump(scenario);
}
void invoke_hook(const char *scenario) {
 if(!patch_hook){std::printf("hook %s missing\n",scenario);return;}
 using Hook=void(__fastcall*)(void*,void*);
 reinterpret_cast<Hook>(patch_hook)(reinterpret_cast<void*>(chat),nullptr);
 dump(scenario);
}
void init_result(const char *scenario,const A2FO_ModuleApi* api,HMODULE image,HMODULE fo) {
 bool ok=backdrop::initialize(api,image,fo);
 std::printf("initialize %s %d active=%d\n",scenario,ok,backdrop::active());
 dump(scenario);
}
}
void* GetCurrentProcess(){return reinterpret_cast<void*>(1);}
BOOL ReadProcessMemory(void*,const void* pointer,void* output,SIZE_T length,SIZE_T* copied) {
 const auto at=reinterpret_cast<std::uintptr_t>(pointer);
 bool ok=at!=0;
 for(SIZE_T i=0;ok&&i<length;++i)ok=memory.count(at+i)!=0;
 calls.push_back("ReadProcessMemory "+address(at)+" length="+std::to_string(length)+" ok="+std::to_string(ok));
 if(copied)*copied=ok?length:0;
 if(ok)for(SIZE_T i=0;i<length;++i)static_cast<BYTE*>(output)[i]=memory[at+i];
 return ok;
}
SIZE_T VirtualQuery(const void* pointer,MEMORY_BASIC_INFORMATION* out,SIZE_T) {
 const auto at=reinterpret_cast<std::uintptr_t>(pointer);
 const bool good=executable.count(at)!=0;
 calls.push_back("VirtualQuery "+address(at)+" ok="+std::to_string(good));
 if(good){out->BaseAddress=const_cast<void*>(pointer);out->RegionSize=16;
  out->State=MEM_COMMIT;out->Protect=PAGE_EXECUTE_READ;return sizeof(*out);}
 return 0;
}
DWORD GetCurrentThreadId(){calls.push_back("GetCurrentThreadId "+std::to_string(thread_id));return thread_id;}
BOOL GetClientRect(HWND window,RECT* out) {
 calls.push_back("GetClientRect window="+std::to_string(reinterpret_cast<std::uintptr_t>(window)));
 if(client_ok)*out=client_rect;
 return client_ok;
}
BOOL ClientToScreen(HWND window,POINT* out) {
 calls.push_back("ClientToScreen window="+std::to_string(reinterpret_cast<std::uintptr_t>(window)));
 if(screen_ok)*out=screen_origin;
 return screen_ok;
}
BOOL GetMonitorInfoA(HMONITOR monitor,MONITORINFO* out) {
 calls.push_back("GetMonitorInfo monitor="+std::to_string(reinterpret_cast<std::uintptr_t>(monitor)));
 if(monitor_ok)out->rcMonitor=monitor_rect;
 return monitor_ok;
}
HRESULT IDirect3DSurface8::LockRect(D3DLOCKED_RECT* out,const RECT*,DWORD flags) {
 calls.push_back("Surface.LockRect flags="+std::to_string(flags));
 if(!lock_ok)return E_FAIL;
 out->Pitch=forced_pitch?forced_pitch:static_cast<int>(display_width*4);
 out->pBits=pixels_present?front_pixels.data():nullptr;
 return S_OK;
}
HRESULT IDirect3DSurface8::UnlockRect(){calls.push_back("Surface.UnlockRect");return S_OK;}
unsigned IDirect3DSurface8::Release(){calls.push_back("Surface.Release");return 0;}
HMONITOR IDirect3D8::GetAdapterMonitor(UINT adapter) {
 calls.push_back("D3D8.GetAdapterMonitor adapter="+std::to_string(adapter));
 return reinterpret_cast<HMONITOR>(7);
}
unsigned IDirect3D8::Release(){calls.push_back("D3D8.Release");return 0;}
HRESULT IDirect3DDevice8::TestCooperativeLevel(){calls.push_back("Device.TestCooperativeLevel");return coop_ok?S_OK:E_FAIL;}
HRESULT IDirect3DDevice8::GetDisplayMode(D3DDISPLAYMODE* out){
 calls.push_back("Device.GetDisplayMode");
 if(display_ok){out->Width=display_width;out->Height=display_height;}
 return display_ok?S_OK:E_FAIL;
}
HRESULT IDirect3DDevice8::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* out){
 calls.push_back("Device.GetCreationParameters");
 if(creation_ok){out->hFocusWindow=reinterpret_cast<HWND>(11);out->AdapterOrdinal=adapter_ordinal;}
 return creation_ok?S_OK:E_FAIL;
}
HRESULT IDirect3DDevice8::CreateImageSurface(UINT width,UINT height,DWORD format,IDirect3DSurface8** out){
 calls.push_back("Device.CreateImageSurface "+std::to_string(width)+"x"+std::to_string(height)+" format="+std::to_string(format));
 *out=surface_ok?&surface:nullptr;return surface_ok?S_OK:E_FAIL;
}
HRESULT IDirect3DDevice8::GetFrontBuffer(IDirect3DSurface8* out){
 calls.push_back("Device.GetFrontBuffer surface="+std::to_string(out==&surface));
 return front_ok?S_OK:E_FAIL;
}
HRESULT IDirect3DDevice8::GetDirect3D(IDirect3D8** out){
 calls.push_back("Device.GetDirect3D");*out=d3d_ok?&d3d:nullptr;return d3d_ok?S_OK:E_FAIL;
}
unsigned IDirect3DDevice8::Release(){calls.push_back("Device.Release");return 0;}
namespace a2fo::refit_menu::menu_scene {
void end_departure(){calls.push_back("menu_scene.end_departure");}
}
namespace {
void module_log(const char* module,const char* message){calls.push_back("log "+std::string(module)+" | "+message);}
bool module_patch(void* target,void* replacement,const std::uint8_t* expected,std::size_t length){
 calls.push_back("patch_call target="+address(reinterpret_cast<std::uintptr_t>(target))+
  " length="+std::to_string(length)+" bytes="+
  (length?std::to_string(expected[0]):"empty")+" ok="+std::to_string(patch_ok));
 if(patch_ok)patch_hook=reinterpret_cast<std::uintptr_t>(replacement);
 return patch_ok;
}
A2FO_ModuleApi api{module_log,module_patch};
void init_cases(){
 reset_settings();backdrop::shutdown();backdrop::reset();calls.clear();
 init_result("null_api",nullptr,reinterpret_cast<HMODULE>(engine_base),nullptr);
 init_result("null_image",&api,nullptr,nullptr);
 A2FO_ModuleApi no_patch{module_log,nullptr};
 init_result("null_patch",&no_patch,reinterpret_cast<HMODULE>(engine_base),nullptr);
 constexpr std::pair<std::uintptr_t,const char*> mutations[]{
  {0x198a0c,"receiver"},{0x8587a,"creation"},{0x11d82b,"deletion"},
  {0x11d3c0,"entry"},{bind::flush_site,"selector"},{bind::device_flush,"flush"}};
 for(const auto &[rva,name]:mutations){
  reset_settings();backdrop::shutdown();backdrop::reset();calls.clear();
  memory[engine_base+rva]^=0xff;
  init_result(name,&api,reinterpret_cast<HMODULE>(engine_base),nullptr);
 }
 reset_settings();backdrop::shutdown();backdrop::reset();calls.clear();
 patch_ok=false;init_result("patch_fail",&api,reinterpret_cast<HMODULE>(engine_base),nullptr);
 reset_settings();backdrop::shutdown();backdrop::reset();calls.clear();
 prepare_fo_chain();init_result("fo_chain",&api,reinterpret_cast<HMODULE>(engine_base),reinterpret_cast<HMODULE>(fo_base));
 init_result("already_enabled",&api,reinterpret_cast<HMODULE>(engine_base),reinterpret_cast<HMODULE>(fo_base));
 reset_settings();backdrop::shutdown();backdrop::reset();calls.clear();
 prepare_fo_chain();memory[gateway]^=1;
 init_result("fo_bad_gateway",&api,reinterpret_cast<HMODULE>(engine_base),reinterpret_cast<HMODULE>(fo_base));
 reset_settings();backdrop::shutdown();backdrop::reset();calls.clear();
 prepare_fo_chain();executable.erase(fo_base+0x1cf0b4);
 init_result("fo_not_callable",&api,reinterpret_cast<HMODULE>(engine_base),reinterpret_cast<HMODULE>(fo_base));
}
void frame_cases(){
 reset_settings();backdrop::shutdown();backdrop::reset();calls.clear();
 init_result("ready",&api,reinterpret_cast<HMODULE>(engine_base),nullptr);
 invoke_hook("observe_one");
 bool ok=backdrop::active();std::printf("active %d\n",ok);dump("active_one");
 frame_result("captured");
 frame_result("reuse");
 thread_id=99;frame_result("wrong_thread");thread_id=41;
 invoke_hook("observe_two");frame_result("recapture");
 put_value<std::uint32_t>(engine_base+0x368de8,0);
 std::printf("active_zero %d\n",backdrop::active());dump("active_zero");
 frame_result("no_chat");
 put_value<std::uint32_t>(engine_base+0x368de8,static_cast<std::uint32_t>(chat));
 invoke_hook("observe_three");
 lock_ok=false;frame_result("neutral_lock_fail");
 frame_result("neutral_reuse");
 invoke_hook("observe_four");frame_result("neutral_again");
 backdrop::reset();dump("reset");frame_result("neutral_after_reset");
 lock_ok=true;backdrop::reset();dump("reset2");frame_result("capture_after_reset");
 backdrop::shutdown();dump("shutdown");
 invoke_hook("observe_disabled");
 std::printf("active_disabled %d\n",backdrop::active());dump("active_disabled");
 frame_result("disabled_frame");
}
void failure_cases(){
 struct Case {const char *name;void (*mutate)();};
 const Case cases[]{
  {"no_storm",[]{memory.erase(engine_base+0x3ad508);}},
  {"bad_index",[]{put_value<std::uint32_t>(storm+0xc0,4);}},
  {"bad_flush",[]{put_value<std::uint32_t>(table+0x38,0);}},
  {"query_fail",[]{query_ok=false;}},
  {"cooperative_fail",[]{coop_ok=false;}},
  {"display_fail",[]{display_ok=false;}},
  {"creation_fail",[]{creation_ok=false;}},
  {"zero_display",[]{display_width=0;}},
  {"oversize_display",[]{display_width=8193;}},
  {"surface_fail",[]{surface_ok=false;}},
  {"front_fail",[]{front_ok=false;}},
  {"client_fail",[]{client_ok=false;}},
  {"screen_fail",[]{screen_ok=false;}},
  {"monitor_fail",[]{monitor_ok=false;}},
  {"bad_crop",[]{screen_origin={100,100};}},
  {"zero_client",[]{client_rect={0,0,0,2};}},
  {"lock_fail",[]{lock_ok=false;}},
  {"null_pixels",[]{pixels_present=false;}},
  {"short_pitch",[]{forced_pitch=1;}},
 };
 for(const auto &test:cases){
  reset_settings();backdrop::shutdown();backdrop::reset();calls.clear();
  const bool armed=backdrop::initialize(&api,reinterpret_cast<HMODULE>(engine_base),nullptr);
  if(!armed){std::printf("failed_to_arm %s\n",test.name);continue;}
  test.mutate();calls.clear();
  invoke_hook(test.name);frame_result(test.name);
 }
}
}
int main(){init_cases();frame_cases();failure_cases();}
