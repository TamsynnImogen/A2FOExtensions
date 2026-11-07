// Native fastcall, Win32/COM and owned Artwork adapters. Rust owns workflow.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "mission_backdrop.hpp"
#include "menu_scene.hpp"
#include "../../rust/backdrop_ffi.h"
#include <d3d8.h>
#include <atomic>
#include <cstring>
#include <exception>
namespace a2fo::refit_menu::mission_backdrop { namespace {
HMODULE engine{};const A2FO_ModuleApi* api{};
std::atomic<bool> enabled{false};std::atomic<std::uintptr_t> rendered_chat{0};std::atomic<unsigned> sequence{0};std::atomic<DWORD> mission_thread{0};
unsigned captured_sequence{};std::uintptr_t captured_chat{};Artwork captured;bool reported{};
struct Context{const A2FO_ModuleApi* supplied{};HMODULE image{};Artwork image_data;std::exception_ptr failure;};
uint8_t read(void*,uintptr_t p,void* out,size_t n){SIZE_T copied{};return p&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),out,n,&copied)&&copied==n;}
uint8_t callable(void*,uintptr_t p){MEMORY_BASIC_INFORMATION i{};if(!p||!VirtualQuery(reinterpret_cast<void*>(p),&i,sizeof(i))||i.State!=MEM_COMMIT||(i.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return 0;const auto protection=i.Protect&0xff;return protection==PAGE_EXECUTE||protection==PAGE_EXECUTE_READ||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY;}
extern "C" __attribute__((force_align_arg_pointer)) void __fastcall a2fo_refit_mission_frame(void*,void*);
uintptr_t action(void* p,uint32_t op,A2foRsBackdropData* d,const uint8_t* bytes,size_t n){auto& c=*static_cast<Context*>(p);if(c.failure)return 0;try{switch(op){
case 0:engine=c.image;api=c.supplied;return 1;
case 1:return enabled.load();
case 2:return enabled.load(std::memory_order_acquire);
case 3:enabled.store(true,std::memory_order_release);return 1;
case 4:enabled.store(false,std::memory_order_release);return 1;
case 5:if(api&&api->log){const std::string s(reinterpret_cast<const char*>(bytes),n);api->log("A2FORefitMenus",s.c_str());}return 1;
case 6:return api->patch_call(reinterpret_cast<void*>(d->words[0]),reinterpret_cast<void*>(&a2fo_refit_mission_frame),bytes,n);
case 7:menu_scene::end_departure();return 1;
case 8:return GetCurrentThreadId();
case 9:mission_thread.store(static_cast<DWORD>(d->words[0]),std::memory_order_release);return 1;
case 10:rendered_chat.store(d->words[0],std::memory_order_release);return 1;
case 11:return sequence.fetch_add(1,std::memory_order_release);
case 12:return sequence.load(std::memory_order_acquire);
case 13:return rendered_chat.load(std::memory_order_acquire);
case 14:return mission_thread.load(std::memory_order_acquire);
case 15:return captured.pixels.empty();
case 16:d->words[0]=captured_sequence;d->words[1]=captured_chat;return 1;
case 17:captured_sequence=static_cast<unsigned>(d->words[0]);captured_chat=d->words[1];return 1;
case 18:captured={};captured_sequence=0;captured_chat=0;reported=false;return 1;
case 19:captured={1,1,{0xff080c12u}};return 1;
case 20:return reported;
case 21:reported=true;return 1;
case 22:{constexpr GUID iid{0x7385e5df,0x8fe8,0x41d5,{0x86,0xb6,0xd7,0xb4,0x85,0x47,0xb6,0xcf}};using Query=HRESULT(__stdcall*)(void*,REFIID,void**);void* device{};const HRESULT hr=reinterpret_cast<Query>(d->words[1])(reinterpret_cast<void*>(d->words[0]),iid,&device);d->words[2]=reinterpret_cast<uintptr_t>(device);return SUCCEEDED(hr);}
case 23:return SUCCEEDED(reinterpret_cast<IDirect3DDevice8*>(d->words[2])->TestCooperativeLevel());
case 24:{D3DDISPLAYMODE mode{};const HRESULT hr=reinterpret_cast<IDirect3DDevice8*>(d->words[2])->GetDisplayMode(&mode);d->words[3]=mode.Width;d->words[4]=mode.Height;return SUCCEEDED(hr);}
case 25:{D3DDEVICE_CREATION_PARAMETERS params{};const HRESULT hr=reinterpret_cast<IDirect3DDevice8*>(d->words[2])->GetCreationParameters(&params);d->words[5]=reinterpret_cast<uintptr_t>(params.hFocusWindow);d->words[6]=params.AdapterOrdinal;return SUCCEEDED(hr);}
case 26:{IDirect3DSurface8* s{};const HRESULT hr=reinterpret_cast<IDirect3DDevice8*>(d->words[2])->CreateImageSurface(static_cast<UINT>(d->words[3]),static_cast<UINT>(d->words[4]),D3DFMT_A8R8G8B8,&s);d->words[7]=reinterpret_cast<uintptr_t>(s);return SUCCEEDED(hr);}
case 27:return SUCCEEDED(reinterpret_cast<IDirect3DDevice8*>(d->words[2])->GetFrontBuffer(reinterpret_cast<IDirect3DSurface8*>(d->words[7])));
case 28:{RECT q{};const BOOL ok=GetClientRect(reinterpret_cast<HWND>(d->words[5]),&q);d->ints[0]=q.right-q.left;d->ints[1]=q.bottom-q.top;return ok!=FALSE;}
case 29:{POINT q{};const BOOL ok=ClientToScreen(reinterpret_cast<HWND>(d->words[5]),&q);d->ints[2]=q.x;d->ints[3]=q.y;return ok!=FALSE;}
case 30:{IDirect3D8* d3d{};const HRESULT hr=reinterpret_cast<IDirect3DDevice8*>(d->words[2])->GetDirect3D(&d3d);d->words[8]=reinterpret_cast<uintptr_t>(d3d);return SUCCEEDED(hr);}
case 31:d->words[9]=reinterpret_cast<uintptr_t>(reinterpret_cast<IDirect3D8*>(d->words[8])->GetAdapterMonitor(static_cast<UINT>(d->words[6])));return 1;
case 32:reinterpret_cast<IDirect3D8*>(d->words[8])->Release();return 1;
case 33:{MONITORINFO mi{};mi.cbSize=sizeof(mi);const BOOL ok=GetMonitorInfoA(reinterpret_cast<HMONITOR>(d->words[9]),&mi);d->ints[4]=mi.rcMonitor.left;d->ints[5]=mi.rcMonitor.top;return ok!=FALSE;}
case 34:{D3DLOCKED_RECT lock{};const HRESULT hr=reinterpret_cast<IDirect3DSurface8*>(d->words[7])->LockRect(&lock,nullptr,D3DLOCK_READONLY);d->pixels=lock.pBits;d->ints[6]=lock.Pitch;return SUCCEEDED(hr);}
case 35:reinterpret_cast<IDirect3DSurface8*>(d->words[7])->UnlockRect();return 1;
case 36:reinterpret_cast<IDirect3DSurface8*>(d->words[7])->Release();return 1;
case 37:reinterpret_cast<IDirect3DDevice8*>(d->words[2])->Release();return 1;
case 38:try{c.image_data.width=d->ints[0];c.image_data.height=d->ints[1];c.image_data.pixels.resize(d->count);d->words[10]=reinterpret_cast<uintptr_t>(c.image_data.pixels.data());return 1;}catch(...){return 0;}
case 39:captured=std::move(c.image_data);return 1;
}}catch(...){c.failure=std::current_exception();}return 0;}
A2foRsBackdropOps bridge(Context& c){return {&c,read,callable,action};}
void checked(Context& c){if(c.failure)std::rethrow_exception(c.failure);}
extern "C" __attribute__((force_align_arg_pointer)) void __fastcall a2fo_refit_mission_frame(void* chat,void*){using Render=void(__fastcall*)(void*,void*);reinterpret_cast<Render>(reinterpret_cast<BYTE*>(engine)+0x11d3c0)(chat,nullptr);Context c;const auto a=bridge(c);a2fo_rs_backdrop_observe(&a,reinterpret_cast<uintptr_t>(chat));checked(c);}
}
bool initialize(const A2FO_ModuleApi* supplied,HMODULE image,HMODULE fo){Context c;c.supplied=supplied;c.image=image;const auto a=bridge(c);const bool ok=a2fo_rs_backdrop_init(&a,reinterpret_cast<uintptr_t>(image),reinterpret_cast<uintptr_t>(fo),supplied&&supplied->patch_call)!=0;checked(c);return ok;}
bool active(){Context c;const auto a=bridge(c);return a2fo_rs_backdrop_active(&a,reinterpret_cast<uintptr_t>(engine))!=0;}
const Artwork* frame(){Context c;const auto a=bridge(c);const bool ok=a2fo_rs_backdrop_frame(&a,reinterpret_cast<uintptr_t>(engine))!=0;checked(c);return ok?&captured:nullptr;}
void reset(){Context c;const auto a=bridge(c);a2fo_rs_backdrop_reset(&a);}
void shutdown(){Context c;const auto a=bridge(c);a2fo_rs_backdrop_shutdown(&a);}
}
