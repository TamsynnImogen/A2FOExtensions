// Refit-style presentation; Fleet Ops retains its original tip/bar/music path.
// Visual reference: Copyright (c) 2026 Cedric Haase, MIT ui/src/loading.rs.
// Licence/provenance: third_party/armada2-refit/{LICENSE.txt,SOURCE.md}.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "loading_native.hpp"
#include "loading_bindings.hpp"
#include "loading_presentation.hpp"
#include "loading_content.hpp"
#include "loading_gpu.hpp"
#include "menu_scene.hpp"
#include "setup_native.hpp"
#include "scene_budget.hpp"
#include "../../rust/stage10_presentation_completion_ffi.h"
#include "../../rust/stage8_b_loading_ffi.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <exception>
#include <memory>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace a2fo::refit_menu::loading_native {
namespace {
namespace b=loading_bindings;
using Original=void(__stdcall*)(float,void*,float);
const A2FO_ModuleApi* api{};
HMODULE engine{},fo{};
A2FO_InlineHook hook{};
std::atomic<void*> next{nullptr};
std::atomic<bool> enabled{false};
std::atomic<DWORD> owner{0};
thread_local bool inside=false;
unsigned reports{};
std::vector<std::string> roots;
std::string loaded_map;
bool content_loaded{};
bool data_reported{};
float previous=-1;
LoadingModel content;
Artwork preview,background;
std::mutex preview_mutex;
std::string preview_map;
Artwork setup_preview;
bool players_ready{};
std::string selected_race;
std::map<std::string,LoadingTheme> theme_cache;
thread_local std::string step_text;
ULONGLONG load_started{};
ULONGLONG painted_at{};
std::uint64_t last_menu_epoch{};
std::mutex mission_mutex;
A2FO_MenuMissionV1 mission_metadata{};
A2FO_MenuMissionV1 session_metadata{};
std::unique_ptr<Artwork> painted;
bool painted_layered{};
std::shared_ptr<const Artwork> uploaded_scene;
std::shared_ptr<const Artwork> pending_loading_scene;
std::uint64_t scene_revision{};
unsigned paint_reports{};
std::uint64_t paint_revision{};
A2FO_InlineHook show_hook{},kill_hook{},release_hook{};
std::atomic<void*> next_splash{nullptr},next_kill{nullptr};
std::atomic<void*> next_release{nullptr};
std::atomic<void*> next_pixels{nullptr};
std::atomic<bool> animation_armed{false};
std::atomic<bool> cache_armed{false};
bool loading_session{},native_frame_seen{};
bool departure_pending{};
void* loading_wrapper{};
float loading_first{};
ULONGLONG next_extra{},extra_cost{};
unsigned native_frames{},extra_frames{};
ULONGLONG first_loading_frame{},last_loading_frame{},longest_loading_gap{};
unsigned departure_frames{};
LoadingTextureCache texture_cache; // Explicit owner-thread reset at every kill-splash entry.
LoadingTextureCache scene_texture_cache;
void reset_textures(){texture_cache.reset();scene_texture_cache.reset();uploaded_scene.reset();pending_loading_scene.reset();}
void acknowledge_loading_scene(){
    if(pending_loading_scene)menu_scene::loading_presented(pending_loading_scene);
    pending_loading_scene.reset();
}
void record_loading_frame(){
    const auto now=GetTickCount64();
    if(!first_loading_frame)first_loading_frame=now;
    if(last_loading_frame)longest_loading_gap=std::max(longest_loading_gap,now-last_loading_frame);
    last_loading_frame=now;
    if(now-first_loading_frame<=3000)++departure_frames;
}
void log(const char* text){if(api&&api->log)api->log("A2FORefitMenus",text);}
void* at(HMODULE image,std::uint32_t rva){return reinterpret_cast<BYTE*>(image)+rva;}
bool read(const void* p,void* value,std::size_t size){
    SIZE_T copied{};
    return p&&ReadProcessMemory(GetCurrentProcess(),p,value,size,&copied)&&copied==size;
}
template<class T>bool field(const void* p,std::size_t offset,T& value){
    const auto address=reinterpret_cast<std::uintptr_t>(p);
    return address&&offset<=UINTPTR_MAX-address&&read(reinterpret_cast<void*>(address+offset),&value,sizeof(value));
}
template<std::size_t N>bool matches(HMODULE image,std::uint32_t rva,const std::array<std::uint8_t,N>& expected){
    std::array<std::uint8_t,N> bytes{};
    return read(at(image,rva),bytes.data(),N)&&bytes==expected;
}
bool callable(void* address){
    MEMORY_BASIC_INFORMATION info{};
    if(!address||!VirtualQuery(address,&info,sizeof(info))||info.State!=MEM_COMMIT||
       (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto p=info.Protect&0xff;
    return p==PAGE_EXECUTE||p==PAGE_EXECUTE_READ||p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY;
}
std::string cstring(const void* p){
    std::string result;
    struct Context {const void* source;std::string& result;char ch{};
        std::exception_ptr error{};std::uint8_t failed{};} c{p,result};
    const auto step=+[](void* opaque,std::uint32_t id,std::uint64_t index) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(id){
        case 0:c.ch=0;return field(c.source,static_cast<std::size_t>(index),c.ch);
        case 1:return c.ch==0;
        case 2:c.result+=c.ch;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    const bool terminated=a2fo_rs_stage10_presentation_flow(4,&c,step,&c.failed,260)!=0;
    if(c.error)std::rethrow_exception(c.error);
    if(!terminated)return {};
    return result;
}
std::vector<std::string> split_loading_lines(const char* text){
    std::vector<std::string> result;std::string line;
    struct Context {const char* text;std::vector<std::string>& result;std::string& line;
        std::exception_ptr error{};std::uint8_t failed{};} c{text,result,line};
    const auto step=+[](void* opaque,std::uint32_t id,std::uint64_t arg) noexcept ->std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(id){
        case 0:return static_cast<unsigned char>(c.text[static_cast<std::size_t>(arg)]);
        case 1:return !c.line.empty();
        case 2:c.result.push_back(std::move(c.line));return 0;
        case 3:c.line.clear();return 0;
        case 4:c.line+=static_cast<char>(arg);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    a2fo_rs_stage10_presentation_flow(5,&c,step,&c.failed,0);
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
std::pair<std::string,std::string> map_details(){
    void* transport{},*data{},*setup{};
    if(!read(at(engine,b::transport),&transport,sizeof(transport))||!transport||
       !field(transport,0x30,setup)||!setup||!field(setup,0x10,data)||!data)return {};
    std::array<BYTE,0xe1> verified{};
    if(!read(data,verified.data(),verified.size()))return {};
    // These read-only getters return engine-owned C strings, not C++ objects.
    // No stack arguments: fastcall's dummy EDX models the native ECX-self ABI.
    using Getter=const char*(__fastcall*)(void*,void*);
    auto name=reinterpret_cast<Getter>(at(engine,b::map_name))(setup,nullptr);
    auto title=reinterpret_cast<Getter>(at(engine,b::map_title))(setup,nullptr);
    return {sanitize_loading_map_name(cstring(name)),cstring(title)};
}
bool player_bindings(){
    struct Context {
        std::array<BYTE,5> branch{};
        HMODULE known{},core{},owner_module{};
        void* destination{};
        std::exception_ptr error{};
    } c;
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        // Full slot getter bodies establish the 0x44 stride, field and RET shape.
        const auto check=[](std::uint32_t rva,const char* bytes,std::size_t n){
            std::array<char,64> actual{};
            return n<=actual.size()&&read(at(engine,rva),actual.data(),n)&&
                !std::memcmp(actual.data(),bytes,n);
        };
        try{
            switch(id){
            case 232:return check(b::find_race,"\x55\x8b\xec\x53\x56\x8b\x75\x08\x57\x85\xf6",11);
            case 233:c.known=GetModuleHandleA("A2FOInstantActionSettings.dll");
                c.core=GetModuleHandleA("A2FOExtensions.dll");return c.known||c.core;
            case 234:return read(at(engine,b::find_race),c.branch.data(),c.branch.size())&&
                c.branch[0]==0xe9;
            case 235:return check(b::find_race+5,"\x8b\x75\x08\x57\x85\xf6",6);
            case 236:{std::int32_t displacement{};
                std::memcpy(&displacement,c.branch.data()+1,4);
                c.destination=static_cast<BYTE*>(at(engine,b::find_race))+5+displacement;return 0;}
            case 237:return callable(c.destination);
            case 238:return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(c.destination),&c.owner_module)!=FALSE;
            case 239:return c.known&&c.owner_module==c.known;
            case 240:return c.core&&c.owner_module==c.core;
            case 241:{const FARPROC exported=GetProcAddress(c.core,
                    "A2FOBuiltinInstantActionSettings_RaceById");
                void* entry{};static_assert(sizeof(entry)==sizeof(exported));
                std::memcpy(&entry,&exported,sizeof(entry));
                return entry&&c.destination==entry;}
            case 242:log("Loading race lookup: verified A2FOInstantActionSettings detour retained");return 0;
            case 243:return check(b::local_slot,"\x8b\x01\xc3",3);
            case 244:return check(b::player_team,"\x55\x8b\xec\x8b\x45\x08\x8b\xd0\xc1\xe2\x04\x03\xd0\x8b\x41\x10\x8b\x84\x90\x04\x01\x00\x00\x5d\xc2\x04\x00",27);
            case 245:return check(b::player_race,"\x55\x8b\xec\x8b\x45\x08\x83\xc0\x04\x8b\xd0\xc1\xe2\x04\x03\xd0\x8b\x41\x10\x8b\x04\x90\x5d\xc2\x04\x00",26);
            case 246:return check(b::player_vacant,"\x55\x8b\xec\x8b\x45\x08\x85\xc0\x7c\x23\x83\xf8\x08",13);
            case 247:return check(b::game_player,"\x55\x8b\xec\x8b\x45\x08\x85\xc0\x7c\x49\x83\xf8\x08",13);
            case 248:return check(b::player_name,"\x55\x8b\xec\x6a\xff\x68\x25\x1f\x6a\x00",10);
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_loading_player_bindings(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
// Bounded copies; no MSVC C++ type crosses into our MinGW runtime.
std::string player_name(void* setup,int slot){
    void* destroy{};HMODULE library{};
    if(!read(at(engine,b::string_dtor_iat),&destroy,sizeof(destroy))||!callable(destroy)||
       !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCSTR>(destroy),&library)||
       library!=GetModuleHandleA("msvcp60.dll"))return {};
    struct NativeString{std::uint32_t allocator{},pointer{},length{},capacity{};} value;
    using Getter=void*(__fastcall*)(void*,void*,void*,int);
    using Destroy=void(__fastcall*)(void*,void*);
    reinterpret_cast<Getter>(at(engine,b::player_name))(setup,nullptr,&value,slot);
    struct Cleanup{void* destroy;void* value;~Cleanup(){reinterpret_cast<Destroy>(destroy)(value,nullptr);}} cleanup{destroy,&value};
    if(value.length>512||value.length>value.capacity||(!value.pointer&&value.length))return {};
    std::string result(value.length,'\0');
    if(value.length&&!read(reinterpret_cast<void*>(value.pointer),result.data(),value.length))return {};
    return result.find('\0')==std::string::npos?result:std::string{};
}
std::string race_name(int index){
    if(index<0||index>1024)return {};
    using Find=void*(__cdecl*)(int);
    const auto race=reinterpret_cast<Find>(at(engine,b::find_race))(index);
    return race?cstring(static_cast<BYTE*>(race)+0x14):std::string{};
}
const LoadingTheme& theme_for(const std::string& race){
    const auto found=theme_cache.find(race);
    if(found!=theme_cache.end())return found->second;
    return theme_cache.emplace(race,loading_theme(roots,race)).first->second;
}
// Only call an engine-owned transport method. Whole-image admission is done
// by the parent module; foreign vtables remain presentation fallbacks.
void* transport_method(void* transport,unsigned offset){
    void* table{},*method{};HMODULE image{};
    if(!field(transport,0,table)||!field(table,offset,method)||!callable(method)||
       !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(method),&image)||image!=engine)return nullptr;
    return method;
}
void populate_players(LoadingModel& model){
    struct Context {
        LoadingModel& model;void* transport{},*setup{},*data{},*mission{};
        std::array<BYTE,0x338> details{};std::array<BYTE,0xa0> setup_bytes{};
        int local{},slot{};std::string race,name,local_race;
        const LoadingTheme* race_theme{};std::optional<Player> player;
        void* send{},*fraction{};float load_fraction{};
        std::exception_ptr error{};
        explicit Context(LoadingModel& output):model(output){}
    } c(model);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        using Zero=int(__fastcall*)(void*,void*);
        using Slot=int(__fastcall*)(void*,void*,int);
        const auto get=[&](std::uint32_t rva){return reinterpret_cast<Slot>(at(engine,rva))(
            c.setup,nullptr,static_cast<int>(arg));};
        const auto flag=[&](unsigned offset){const auto method=transport_method(c.transport,offset);
            return method?reinterpret_cast<Zero>(method)(c.transport,nullptr)&255:0;};
        try{
            switch(id){
            case 200:c.model.players.clear();return 0;
            case 201:return players_ready;
            case 202:return read(at(engine,b::transport),&c.transport,sizeof(c.transport))&&
                field(c.transport,0x30,c.setup)&&field(c.setup,0x10,c.data);
            case 203:return read(c.data,c.details.data(),c.details.size())&&
                read(c.setup,c.setup_bytes.data(),c.setup_bytes.size());
            case 204:return flag(0x10)!=0;
            case 205:return flag(0x18)!=0;
            case 206:c.model.kind=arg==1?LoadingMode::instant_action:
                arg==2?LoadingMode::campaign:LoadingMode::multiplayer;
                c.model.mode=arg==1?"INSTANT ACTION":arg==2?"SINGLE PLAYER":"MULTIPLAYER";return 0;
            case 207:c.local=reinterpret_cast<Zero>(at(engine,b::local_slot))(c.setup,nullptr);
                return c.local>=0&&c.local<8;
            case 208:c.slot=static_cast<int>(arg);return (get(b::player_vacant)&255)!=0;
            case 209:return (get(b::game_player)&255)!=0;
            case 210:c.race=race_name(get(b::player_race));return 0;
            case 211:c.name=player_name(c.setup,static_cast<int>(arg));return 0;
            case 212:return c.name.empty();
            case 213:c.name="Player "+std::to_string(arg+1);return 0;
            case 214:c.race_theme=&theme_for(c.race);return 0;
            case 215:return static_cast<std::uint32_t>(get(b::player_team));
            case 216:c.player.emplace(c.name,c.race_theme->race.empty()?c.race:c.race_theme->name,
                static_cast<int>(arg),c.slot==c.local);return 0;
            case 231:c.player->team=0;return 0;
            case 217:return flag(0x40)!=0;
            case 218:c.send=transport_method(c.transport,0x5c);
                c.fraction=transport_method(c.transport,0x44);return c.send&&c.fraction;
            case 219:return (reinterpret_cast<Slot>(c.send)(c.transport,nullptr,
                static_cast<int>(arg))&255)!=0;
            case 220:{using Fraction=float(__fastcall*)(void*,void*,int);
                c.load_fraction=reinterpret_cast<Fraction>(c.fraction)(c.transport,nullptr,
                    static_cast<int>(arg));std::uint32_t bits{};
                std::memcpy(&bits,&c.load_fraction,sizeof(bits));return bits;}
            case 221:c.player->fraction=std::clamp(c.load_fraction,0.f,1.f);return 0;
            case 222:return static_cast<int>(arg)==c.local;
            case 223:c.local_race=c.race;return 0;
            case 224:c.model.players.push_back(std::move(*c.player));c.player.reset();return 0;
            case 225:return read(at(engine,b::mission_race),&c.mission,sizeof(c.mission))&&c.mission;
            case 226:c.name=cstring(static_cast<BYTE*>(c.mission)+0x14);return !c.name.empty();
            case 227:c.local_race=c.name;return 0;
            case 228:return selected_race!=c.local_race;
            case 229:selected_race=c.local_race;content.theme=theme_for(c.local_race);return 0;
            case 230:c.model.theme=content.theme;return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_loading_players(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
bool protected_regions(LoadingModel& model,int w,int h){
    // Read exactly the existing FO values. Never write or re-parse its ODF.
    float cfg[13]{};
    std::uint8_t vertical{},invert{};
    if(!read(at(fo,b::bar_height),cfg,sizeof(cfg))||
       !read(at(fo,0x246aa4),&vertical,1)||!read(at(fo,0x246aa5),&invert,1))return false;
    if(a2fo_stage10_presentation_find(std::begin(cfg),std::end(cfg),
        [](float v){return !std::isfinite(v)||std::abs(v)>100;})!=std::end(cfg))return false;
    const auto add=[&](double x,double y,double rw,double rh){
        if(rw<0){x+=rw;rw=-rw;}if(rh<0){y+=rh;rh=-rh;}
        const int left=static_cast<int>(std::clamp(std::floor(x)-4.,0.,double(w)));
        const int top=static_cast<int>(std::clamp(std::floor(y)-4.,0.,double(h)));
        const int right=static_cast<int>(std::clamp(std::ceil(x+rw)+4.,0.,double(w)));
        const int bottom=static_cast<int>(std::clamp(std::ceil(y+rh)+4.,0.,double(h)));
        if(right>left&&bottom>top)model.protected_regions.push_back({left,top,right-left,bottom-top});
    };
    double x=cfg[2]*w,y=cfg[3]*h,rw=cfg[1]*w,rh=cfg[0]*h;
    if(vertical){x=cfg[2]*h;y=cfg[3]*w;rw=cfg[0]*w;rh=cfg[1]*h;}
    // FO also negates thickness when inverted. Preserve that native convention.
    if(invert){if(vertical)y+=rh;else x+=rw;rw=-rw;rh=-rh;}
    const double left=rw<0?x+rw:x,top=rh<0?y+rh:y;
    const int bx=static_cast<int>(std::clamp(std::floor(left),0.,double(w)));
    const int by=static_cast<int>(std::clamp(std::floor(top),0.,double(h)));
    model.bar_track={bx,by,
        std::max(0,static_cast<int>(std::clamp(std::ceil(left+std::abs(rw)),0.,double(w)))-bx),
        std::max(0,static_cast<int>(std::clamp(std::ceil(top+std::abs(rh)),0.,double(h)))-by)};
    add(x,y,rw,rh);
    char* tip{};void* sprite{};
    if(!read(at(fo,b::tip_text),&tip,sizeof(tip))||!read(at(fo,b::tip_sprite),&sprite,sizeof(sprite)))return false;
    char first{};
    if(tip&&read(tip,&first,1)&&first)add(cfg[7]*w,cfg[8]*h,cfg[9]*w,h-cfg[8]*h);
    if(sprite)add(cfg[11]*w,cfg[12]*h,cfg[10]*w,cfg[10]*w);
    return true;
}
struct Dib {
    HDC dc=CreateCompatibleDC(nullptr);
    HBITMAP bitmap{};HGDIOBJ old{};void* bits{};
    ~Dib(){if(old)SelectObject(dc,old);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
    bool create(int w,int h){
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        if(!dc)return false;
        bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
        if(!bitmap||!bits)return false;
        old=SelectObject(dc,bitmap);return old&&old!=HGDI_ERROR;
    }
};
IDirect3DDevice8* query_device(void* wrapper){
    // Only the checked D3D8 wrapper's Flush is supported. Unknown/direct-D3D9
    // implementations retain FO's whole loading screen, including all features.
    void* vtable{},*flush{},*unknown{};
    if(!field(wrapper,0,vtable)||!field(vtable,0x38,flush)||flush!=at(engine,b::device_flush)||
       !field(wrapper,0x90,unknown)||!unknown)return nullptr;
    void* com_vtable{},*query{};
    if(!field(unknown,0,com_vtable)||!field(com_vtable,0,query)||!callable(query))return nullptr;
    constexpr GUID iid8{0x7385e5df,0x8fe8,0x41d5,{0x86,0xb6,0xd7,0xb4,0x85,0x47,0xb6,0xcf}};
    IDirect3DDevice8* raw{};
    using Query=HRESULT(__stdcall*)(void*,REFIID,void**);
    if(FAILED(reinterpret_cast<Query>(query)(unknown,iid8,reinterpret_cast<void**>(&raw))))return nullptr;
    return raw;
}
bool present(void* wrapper,float progress){
    struct Release {IDirect3DDevice8* raw{};~Release(){if(raw)raw->Release();}} owned;
    struct Context {
        void* wrapper;float progress;IDirect3DDevice8* raw{};
        Release* owner;
        D3DSURFACE_DESC description{};
        std::pair<std::string,std::string> details{};
        std::uint64_t epoch{};
        std::optional<LoadingModel> model;
        std::shared_ptr<const Artwork> retained;
        const Artwork* backdrop{};
        bool layered{};
        ULONGLONG now{};
        std::exception_ptr error{};
        Context(void* source,float value,Release* release):wrapper(source),progress(value),owner(release){}
    } c(wrapper,progress,&owned);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        const int w=static_cast<int>(c.description.Width),h=static_cast<int>(c.description.Height);
        try{
            switch(id){
            case 112:c.raw=query_device(c.wrapper);c.owner->raw=c.raw;return c.raw!=nullptr;
            case 113:{IDirect3DSurface8* target{};
                if(FAILED(c.raw->GetRenderTarget(&target))||!target)return 0;
                const HRESULT result=target->GetDesc(&c.description);target->Release();
                return SUCCEEDED(result);}
            case 114:return c.description.Width;
            case 115:return c.description.Height;
            case 116:c.details=map_details();return 0;
            case 117:c.epoch=menu_scene::menu_epoch();return 0;
            case 118:return departure_pending;
            case 119:return content_loaded;
            case 120:return c.details.first!=loaded_map;
            case 121:return c.progress<previous;
            case 122:return c.epoch!=last_menu_epoch;
            case 123:content={};content.title=c.details.second.empty()?c.details.first:c.details.second;
                content.mode="LOADING";return 0;
            case 124:selected_race.clear();theme_cache.clear();content.theme=theme_for({});return 0;
            case 125:load_started=GetTickCount64();return 0;
            case 126:loaded_map=c.details.first;preview={};background={};return 0;
            case 127:load_loading_content(roots,loaded_map,content,preview,background);return 0;
            case 128:return preview.pixels.empty();
            case 129:{const std::lock_guard<std::mutex> lock(preview_mutex);
                if(!loaded_map.empty()&&!lstrcmpiA(loaded_map.c_str(),preview_map.c_str()))preview=setup_preview;
                return 0;}
            case 130:content.show_preview=!preview.pixels.empty();content_loaded=true;
                data_reported=false;painted.reset();last_menu_epoch=c.epoch;return 0;
            case 131:{const std::lock_guard<std::mutex> lock(mission_mutex);
                session_metadata=mission_metadata;mission_metadata={};return 0;}
            case 132:previous=c.progress;return 0;
            case 133:{float clamped{};std::memcpy(&clamped,&arg,sizeof(clamped));
                c.model.emplace(content);c.model->progress=clamped;return 0;}
            case 134:populate_players(*c.model);return 0;
            case 135:return c.model->kind==LoadingMode::campaign;
            case 136:return session_metadata.size&&
                !lstrcmpiA(sanitize_loading_map_name(session_metadata.map).c_str(),loaded_map.c_str());
            case 137:return *session_metadata.title!=0;
            case 138:c.model->title=session_metadata.title;return 0;
            case 139:return *session_metadata.briefing!=0;
            case 141:return *session_metadata.objectives!=0;
            case 140:case 142:{const char* text=id==140?session_metadata.briefing:session_metadata.objectives;
                auto result=split_loading_lines(text);
                if(id==140)c.model->briefing=std::move(result);
                else c.model->objectives=std::move(result);
                return 0;}
            case 143:return !data_reported;
            case 144:{char message[384]{};
                std::snprintf(message,sizeof(message),"Loading UI data: map=%s; players=%u; faction=%s; minimap=%dx%d",
                    loaded_map.c_str(),static_cast<unsigned>(c.model->players.size()),
                    selected_race.c_str(),preview.width,preview.height);
                log(message);data_reported=true;return 0;}
            case 145:c.model->status=step_text.empty()?"Preparing the game":step_text;
                c.model->time_seconds=(GetTickCount64()-load_started)/1000.0;return 0;
            case 146:return protected_regions(*c.model,w,h);
            case 147:c.retained=menu_scene::loading_frame(w,h,arg!=0);return 0;
            case 148:departure_pending=false;return 0;
            case 149:return bool(c.retained);
            case 150:c.retained=menu_scene::retained_frame();return 0;
            case 151:c.model->departure_shown=menu_scene::requested()&&menu_scene::departing();return 0;
            case 152:c.backdrop=c.retained?c.retained.get():background.pixels.empty()?nullptr:&background;
                c.layered=bool(c.retained)&&c.model->departure_shown;return c.layered;
            case 153:c.now=GetTickCount64();return 0;
            case 154:return !painted;
            case 155:return painted_layered!=c.layered;
            case 156:return painted->width!=w||painted->height!=h;
            case 157:return c.now-painted_at>=(c.layered?100u:67u);
            case 158:{const auto begin=GetTickCount64();
                Dib dib;if(!dib.create(w,h)||!paint_loading(dib.dc,w,h,*c.model,
                    c.layered?nullptr:c.backdrop,preview.pixels.empty()?nullptr:&preview,c.layered)||
                    !GdiFlush())return 0;
                auto image=std::make_unique<Artwork>();image->width=w;image->height=h;
                image->pixels.assign(static_cast<std::uint32_t*>(dib.bits),
                    static_cast<std::uint32_t*>(dib.bits)+std::size_t(w)*h);
                if(c.layered)a2fo_rs_stage10_presentation_layer_alpha(image->pixels.data(),image->pixels.size());
                painted=std::move(image);painted_layered=c.layered;painted_at=GetTickCount64();
                ++paint_revision;
                if(paint_reports++<3){char message[160]{};
                    std::snprintf(message,sizeof(message),
                        "Loading CPU %s: %llu ms; %dx%d; native callbacks unthrottled",
                        c.layered?"UI overlay":"presentation",
                        static_cast<unsigned long long>(painted_at-begin),w,h);log(message);}
                return 1;}
            case 159:{using Flush=void(__fastcall*)(void*,void*);
                reinterpret_cast<Flush>(at(engine,b::device_flush))(c.wrapper,nullptr);return 0;}
            case 160:return cache_armed.load(std::memory_order_acquire)&&loading_session;
            case 161:if(uploaded_scene!=c.retained){uploaded_scene=c.retained;++scene_revision;}
                return draw_loading_layers(c.raw,*c.retained,*painted,w,h,
                    arg?&scene_texture_cache:nullptr,arg?&texture_cache:nullptr,
                    scene_revision,paint_revision);
            case 162:pending_loading_scene=c.retained;return 0;
            case 163:return draw_loading_image(c.raw,*painted,texture_cache,paint_revision);
            case 164:return draw_loading_image(c.raw,*painted);
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_loading_present(&c,op,progress)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}

void* active_wrapper(){
    void* storm{},*wrapper{};unsigned index{};
    if(!read(at(engine,0x3ad508),&storm,sizeof(storm))||!field(storm,0xc0,index)||index>3||
       !field(storm,0xcc+index*4,wrapper))return nullptr;
    return wrapper;
}
void extra_frame(){
    struct Release {IDirect3DDevice8* p{};~Release(){if(p)p->Release();}} release;
    struct Context {
        Release* release;IDirect3DDevice8* device{};
        D3DVIEWPORT8 viewport{};DWORD state{};bool restore{};
        D3DSURFACE_DESC description{};ULONGLONG begin{};float remaining{};
        std::exception_ptr error{};
        explicit Context(Release* owner):release(owner){}
        ~Context(){if(restore){device->ApplyStateBlock(state);
            device->DeleteStateBlock(state);device->SetViewport(&viewport);}}
    } c(&release);
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 165:return enabled.load(std::memory_order_acquire);
            case 166:return animation_armed.load(std::memory_order_acquire);
            case 167:return loading_session;
            case 168:return native_frame_seen;
            case 169:return !inside;
            case 170:return owner.load()==GetCurrentThreadId();
            case 171:c.begin=GetTickCount64();return 0;
            case 172:return c.begin<next_extra;
            case 173:{BYTE visible{};return read(at(engine,b::status_visible),&visible,1)&&visible==1;}
            case 174:return read(at(engine,b::fraction_remaining),&c.remaining,4);
            case 175:{std::uint32_t bits{};std::memcpy(&bits,&c.remaining,sizeof(bits));return bits;}
            case 176:return active_wrapper()==loading_wrapper;
            case 177:c.device=query_device(loading_wrapper);c.release->p=c.device;return c.device!=nullptr;
            case 178:return SUCCEEDED(c.device->TestCooperativeLevel());
            case 179:return SUCCEEDED(c.device->GetViewport(&c.viewport));
            case 180:if(FAILED(c.device->CreateStateBlock(D3DSBT_ALL,&c.state)))return 0;
                c.restore=true;return 1;
            case 181:{IDirect3DSurface8* target{};
                if(FAILED(c.device->GetRenderTarget(&target))||!target)return 0;
                const auto described=target->GetDesc(&c.description);target->Release();
                return SUCCEEDED(described);}
            case 182:return SUCCEEDED(c.device->BeginScene());
            case 183:acknowledge_loading_scene();return 0;
            case 184:inside=true;return 0;
            case 185:try{D3DVIEWPORT8 full{0,0,c.description.Width,c.description.Height,0.f,1.f};
                return SUCCEEDED(c.device->SetViewport(&full))&&
                    present(loading_wrapper,1.f-c.remaining)?1u:0u;}
                catch(...){return 2;}
            case 186:try{reinterpret_cast<Original>(next.load(std::memory_order_acquire))(
                    loading_first,loading_wrapper,1.f-c.remaining);
                using Flush=void(__fastcall*)(void*,void*);
                reinterpret_cast<Flush>(at(engine,b::device_flush))(loading_wrapper,nullptr);
                return 1;}catch(...){return 2;}
            case 187:return SUCCEEDED(c.device->EndScene());
            case 188:return SUCCEEDED(c.device->Present(nullptr,nullptr,nullptr,nullptr));
            case 189:++extra_frames;record_loading_frame();acknowledge_loading_scene();return 0;
            case 190:pending_loading_scene.reset();return 0;
            case 191:inside=false;return 0;
            case 192:{const auto finished=GetTickCount64();extra_cost+=finished-c.begin;
                next_extra=finished+menu_scene::animation_pause(static_cast<std::uint32_t>(
                    std::min<ULONGLONG>(finished-c.begin,1000)));return 0;}
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_loading_extra_frame(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}

extern "C" __attribute__((force_align_arg_pointer)) void __cdecl a2fo_refit_splash_begin(){
    void* gateway{};while(!(gateway=next_splash.load(std::memory_order_acquire)))Sleep(0);
    struct Context {void* gateway,*transport{};std::exception_ptr error{};} c{gateway,nullptr,{}};
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 86:return !menu_scene::launch_handover_active();
            case 87:return animation_armed.load(std::memory_order_acquire);
            case 88:return enabled.load(std::memory_order_acquire);
            case 89:return players_ready;
            case 90:return read(at(engine,b::transport),&c.transport,sizeof(c.transport));
            case 91:{using Zero=int(__fastcall*)(void*,void*);
                const auto instant=transport_method(c.transport,0x10);
                return instant&&(reinterpret_cast<Zero>(instant)(c.transport,nullptr)&255);}
            case 92:try{setup_native::begin_loading_handover();}catch(...){}return 0;
            case 93:return menu_scene::launch_handover_active();
            case 94:menu_scene::end_handover();return 0;
            case 95:departure_pending=false;return 0;
            case 96:return animation_armed.load(std::memory_order_acquire)&&
                enabled.load(std::memory_order_acquire);
            case 97:{DWORD unowned=0;owner.compare_exchange_strong(unowned,GetCurrentThreadId());
                return owner.load()==GetCurrentThreadId();}
            case 98:reset_textures();loading_session=true;native_frame_seen=false;loading_wrapper=nullptr;
                native_frames=extra_frames=0;extra_cost=0;next_extra=0;content_loaded=false;painted.reset();
                first_loading_frame=last_loading_frame=longest_loading_gap=0;departure_frames=0;return 0;
            case 99:reinterpret_cast<void(__cdecl*)()>(c.gateway)();return 0;
            case 100:return animation_armed.load(std::memory_order_acquire)&&
                enabled.load(std::memory_order_acquire)&&owner.load()==GetCurrentThreadId();
            case 101:try{departure_pending=false;
                log("Loading animation continues the IA Launch departure without a camera reset");}catch(...){}return 0;
            case 102:try{menu_scene::end_departure();departure_pending=true;
                log("Loading animation queued: choreography starts at the first loading draw");}catch(...){}return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_loading_splash_begin(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
extern "C" __attribute__((force_align_arg_pointer)) void __cdecl a2fo_refit_splash_end(){
    void* gateway{};while(!(gateway=next_kill.load(std::memory_order_acquire)))Sleep(0);
    struct Context {void* gateway;std::exception_ptr error{};} c{gateway,{}};
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 103:return animation_armed.load(std::memory_order_acquire);
            case 104:return owner.load()==GetCurrentThreadId();
            case 105:loading_session=false;native_frame_seen=false;loading_wrapper=nullptr;
                departure_pending=false;return 0;
            case 106:menu_scene::end_handover();return 0;
            case 107:reset_textures();painted.reset();return 0;
            case 108:try{menu_scene::end_departure();}catch(...){}return 0;
            case 109:{char message[192]{};std::snprintf(message,sizeof(message),
                "Loading animation finished: native=%u extra=%u; extra frame API elapsed=%llu ms; cached loading texture released before native cleanup",
                native_frames,extra_frames,static_cast<unsigned long long>(extra_cost));log(message);
                std::snprintf(message,sizeof(message),"Loading cadence: %u callbacks/presents in first 3 seconds; longest gap=%llu ms; total=%llu ms",
                departure_frames,static_cast<unsigned long long>(longest_loading_gap),
                static_cast<unsigned long long>(first_loading_frame?GetTickCount64()-first_loading_frame:0));
                log(message);return 0;}
            case 110:reinterpret_cast<void(__cdecl*)()>(c.gateway)();return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_loading_splash_end(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
extern "C" __attribute__((force_align_arg_pointer)) void* __fastcall a2fo_refit_loaded_db(void* self,void*,void* name,void* path,float scale){
    using Load=void*(__fastcall*)(void*,void*,void*,void*,float);
    auto* result=reinterpret_cast<Load>(at(engine,b::load_db))(self,nullptr,name,path,scale);
    try{extra_frame();}catch(...){}
    return result;
}
extern "C" __attribute__((force_align_arg_pointer)) unsigned __fastcall a2fo_refit_loaded_pixels(void* self,void*,int level){
    using Load=unsigned(__fastcall*)(void*,void*,int);
    const auto destination=next_pixels.load(std::memory_order_acquire);
    const auto result=reinterpret_cast<Load>(destination?destination:at(engine,b::load_pixels))(self,nullptr,level);
    try{extra_frame();}catch(...){}
    return result;
}
extern "C" __attribute__((force_align_arg_pointer)) void __fastcall a2fo_refit_release_device(void* self,void*){
    void* gateway{};while(!(gateway=next_release.load(std::memory_order_acquire)))Sleep(0);
    // Native graphics teardown is serialized with native drawing. Release all
    // loading COM ownership before the engine releases/replaces its device,
    // including display changes inside GameOpen before KillSplash is reached.
    reset_textures();native_frame_seen=false;loading_wrapper=nullptr;
    using Release=void(__fastcall*)(void*,void*);
    reinterpret_cast<Release>(gateway)(self,nullptr);
}
bool install_release_observer(){
    struct Context {
        std::array<BYTE,6> transfer{};
        void* target{};
        std::array<BYTE,12> tail{{0x8b,0x4d,0xfc,0xff,0x15,0,0,0,0,0x59,0x5d,0xc3}};
        std::array<BYTE,7> entry{{0x55,0x8b,0xec,0x51,0x89,0x4d,0xfc}};
        std::array<BYTE,5> jump{};
        void* destination{};
        HMODULE core{},module{};
        std::exception_ptr error{};
    } c;
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 29:return matches(engine,b::release_device+0xaf,b::release_tail);
            case 30:return matches(engine,b::release_device,b::release_entry);
            case 31:return api->install_inline_hook(at(engine,b::release_device),
                reinterpret_cast<void*>(&a2fo_refit_release_device),b::release_entry.size(),
                b::release_entry.data(),&release_hook);
            case 32:next_release.store(release_hook.gateway,std::memory_order_release);return 0;
            case 33:return api->patch_jump!=nullptr;
            case 34:return read(at(engine,b::release_device),c.transfer.data(),c.transfer.size());
            case 35:return static_cast<std::uint64_t>(c.transfer[0])|
                (static_cast<std::uint64_t>(c.transfer[5])<<8);
            case 36:
                if(arg==1)std::memcpy(&c.target,c.transfer.data()+1,4);
                else {std::int32_t delta{};std::memcpy(&delta,c.transfer.data()+1,4);
                    c.target=static_cast<BYTE*>(at(engine,b::release_device))+5+delta;}
                return 0;
            case 37:return c.target==at(fo,b::fo_release_device);
            case 38:{const auto pointer=reinterpret_cast<std::uintptr_t>(at(fo,b::fo_release_gateway));
                std::memcpy(c.tail.data()+5,&pointer,4);
                return matches(fo,b::fo_release_device+7,c.tail);}
            case 39:return matches(fo,b::fo_release_device,c.entry);
            case 40:return read(c.target,c.jump.data(),c.jump.size());
            case 41:return c.jump[0]==0xe9;
            case 42:{std::int32_t delta{};std::memcpy(&delta,c.jump.data()+1,4);
                c.destination=static_cast<BYTE*>(c.target)+5+delta;return 1;}
            case 43:c.core=GetModuleHandleA("A2FOExtensions.dll");return c.core!=nullptr;
            case 44:return callable(c.destination)&&GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(c.destination),&c.module)&&c.module==c.core;
            case 45:next_release.store(c.target,std::memory_order_release);return 0;
            case 46:return api->patch_jump(at(engine,b::release_device),
                reinterpret_cast<void*>(&a2fo_refit_release_device),
                c.transfer.data(),c.transfer.size());
            case 47:next_release.store(nullptr);return 0;
            case 48:log("Loading device cleanup: verified Fleet Ops/core renderer chain retained");return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_loading_release(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
bool install_animation(){
    static_assert(b::load_db_sites.size()==3);
    struct Context {
        std::array<BYTE,7> kill{{0x8b,0x0d,0,0,0,0,0x53}};
        std::array<BYTE,6> visible{{0x88,0x1d,0,0,0,0}};
        std::array<BYTE,13> fraction{{0xd9,0x05,0,0,0,0,0xd8,0x25,0,0,0,0,0xc3}};
        std::array<BYTE,5> pixel_call{};
        void* pixel_target{};
        std::exception_ptr error{};
    } c;
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t arg) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 49:return api->patch_call!=nullptr;
            case 50:{const auto model=reinterpret_cast<std::uintptr_t>(at(engine,b::splash_model));
                const auto flag=reinterpret_cast<std::uintptr_t>(at(engine,b::status_visible));
                const auto one=reinterpret_cast<std::uintptr_t>(at(engine,b::float_one));
                const auto remain=reinterpret_cast<std::uintptr_t>(at(engine,b::fraction_remaining));
                std::memcpy(c.kill.data()+2,&model,4);
                std::memcpy(c.visible.data()+2,&flag,4);
                std::memcpy(c.fraction.data()+2,&one,4);
                std::memcpy(c.fraction.data()+8,&remain,4);return 0;}
            case 51:return matches(engine,b::show_splash,b::splash_entry);
            case 52:return matches(engine,b::kill_splash,c.kill);
            case 53:return matches(engine,b::kill_splash+11,c.visible);
            case 54:return matches(engine,b::compute_fraction,c.fraction);
            case 55:return api->install_inline_hook(at(engine,b::kill_splash),
                reinterpret_cast<void*>(&a2fo_refit_splash_end),c.kill.size(),c.kill.data(),&kill_hook);
            case 56:next_kill.store(kill_hook.gateway,std::memory_order_release);return 0;
            case 57:return api->install_inline_hook(at(engine,b::show_splash),
                reinterpret_cast<void*>(&a2fo_refit_splash_begin),b::splash_entry.size(),
                b::splash_entry.data(),&show_hook);
            case 58:next_splash.store(show_hook.gateway,std::memory_order_release);return 0;
            case 59:return install_release_observer();
            case 60:cache_armed.store(true,std::memory_order_release);return 0;
            case 61:log("Loading texture cache armed: native device cleanup verified before Release");return 0;
            case 62:log("Loading texture cache unavailable: native cleanup differs; transient textures retained");return 0;
            case 63:animation_armed.store(true,std::memory_order_release);return 0;
            case 64:{const auto site=b::load_db_sites[arg];
                std::array<BYTE,5> call{{0xe8,0,0,0,0}};
                const std::int32_t delta=static_cast<std::int32_t>(b::load_db-site-5);
                std::memcpy(call.data()+1,&delta,4);
                return matches(engine,site,call)&&api->patch_call(at(engine,site),
                    reinterpret_cast<void*>(&a2fo_refit_loaded_db),call.data(),call.size());}
            case 65:return read(at(engine,b::load_pixels_site),c.pixel_call.data(),c.pixel_call.size());
            case 66:return c.pixel_call[0]==0xe8;
            case 67:{std::int32_t delta{};std::memcpy(&delta,c.pixel_call.data()+1,4);
                c.pixel_target=static_cast<BYTE*>(at(engine,b::load_pixels_site))+5+delta;
                return c.pixel_target==at(engine,b::load_pixels)||
                    (c.pixel_target==at(fo,b::fo_load_pixels)&&matches(fo,b::fo_load_pixels,b::fo_pixels_body));}
            case 68:next_pixels.store(c.pixel_target,std::memory_order_release);return 0;
            case 69:return api->patch_call(at(engine,b::load_pixels_site),
                reinterpret_cast<void*>(&a2fo_refit_loaded_pixels),c.pixel_call.data(),c.pixel_call.size());
            case 70:next_pixels.store(nullptr,std::memory_order_release);return 0;
            case 71:{char message[160]{};std::snprintf(message,sizeof(message),
                "Loading animation armed: splash lifetime verified; %u/4 inner asset sites; native bar/tips preserved",arg);
                log(message);return 0;}
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_loading_animation(&c,op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
}
// The original cdecl status update still runs once with its untouched pointer.
// Copy before it renders: the caller owns a short-lived formatted buffer.
extern "C" __attribute__((force_align_arg_pointer)) void __cdecl a2fo_refit_loading_status(const char* text){
    struct Context {const char* text;std::exception_ptr error{};} c{text,{}};
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 249:return enabled.load(std::memory_order_acquire);
            case 250:try{step_text=cstring(c.text);}catch(...){}return 0;
            case 251:{using Update=void(__cdecl*)(const char*);
                reinterpret_cast<Update>(at(engine,b::status_update))(c.text);return 0;}
            case 252:return loading_session;
            case 253:return native_frame_seen;
            case 254:return !inside;
            case 255:return owner.load()==GetCurrentThreadId();
            case 256:acknowledge_loading_scene();return 0;
            case 257:try{extra_frame();}catch(...){}return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_loading_status(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
extern "C" __attribute__((force_align_arg_pointer)) void __stdcall a2fo_refit_loading_bar(float unused,void* wrapper,float progress){
    void* gateway{};
    // The checked installer publishes executable detour bytes before returning
    // its gateway. Match the existing menu bridge's publication handshake so a
    // concurrent native call cannot disappear during that short interval.
    while(!(gateway=next.load(std::memory_order_acquire)))Sleep(0);
    struct Context {float first,progress;void* wrapper,*gateway;std::exception_ptr error{};}
        c{unused,progress,wrapper,gateway,{}};
    const auto op=[](void* opaque,std::uint32_t id,std::uint32_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(id){
            case 72:return enabled.load(std::memory_order_acquire);
            case 73:return !inside;
            case 74:{const DWORD thread=GetCurrentThreadId();DWORD unowned=0;
                owner.compare_exchange_strong(unowned,thread);return owner.load()==thread;}
            case 75:inside=true;return 0;
            case 76:try{return present(c.wrapper,c.progress)?1u:0u;}catch(...){return 2;}
            case 77:try{if(!reports++)log("Refit loading presentation active; original FO loadscreen.odf bar/tips drawn afterwards");return 1;}
                catch(...){return 2;}
            case 78:try{if(reports++<3)log("Refit loading presentation unavailable; native FO callback forwarded");return 1;}
                catch(...){return 2;}
            case 79:if(reports++<3)log("Refit loading presentation failed; forwarding original FO loading callback");return 0;
            case 80:inside=false;return 0;
            case 81:return owner.load()==GetCurrentThreadId()&&menu_scene::launch_handover_active();
            case 82:menu_scene::end_handover();return 0;
            case 83:reinterpret_cast<Original>(c.gateway)(c.first,c.wrapper,c.progress);return 0;
            case 84:return animation_armed.load(std::memory_order_acquire)&&loading_session&&
                owner.load()==GetCurrentThreadId()&&!inside;
            case 85:loading_wrapper=c.wrapper;loading_first=c.first;native_frame_seen=true;
                ++native_frames;record_loading_frame();next_extra=GetTickCount64()+33;return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_loading_bar(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}
struct InitContext {
    const A2FO_ModuleApi* supplied;
    HMODULE armada, fleetops;
    bool animate;
    std::exception_ptr error;
};
std::uint64_t init_op(void* opaque,std::uint32_t id,std::uint32_t arg) noexcept {
    auto& c=*static_cast<InitContext*>(opaque);
    try{
        switch(id){
        case 0:return c.supplied&&c.supplied->install_inline_hook&&c.armada&&c.fleetops;
        case 1:return next.load()!=nullptr;
        case 2:api=c.supplied;engine=c.armada;fo=c.fleetops;return 0;
        case 3:return matches(fo,b::new_bar,b::new_bar_entry);
        case 4:return matches(engine,b::device_flush,b::flush_body);
        case 5:return matches(engine,b::flush_site,b::flush_site_code);
        case 6:return matches(engine,b::game_setup,b::game_setup_code);
        case 7:return matches(engine,b::map_name,b::map_name_code);
        case 8:return matches(engine,b::map_title,b::map_title_code);
        case 9:log("Loading bindings changed; preserving native loading screen");return 0;
        case 10:return A2FO_MODULE_API_HAS(api,extension_root)&&api->extension_root_count&&api->extension_root;
        case 11:return api->extension_root_count();
        case 12:{const auto root=api->extension_root(arg);if(root&&*root)roots.emplace_back(root);return 0;}
        case 13:return roots.empty()&&api->root_directory;
        case 14:{const auto root=api->root_directory();if(root&&*root)roots.emplace_back(root);return 0;}
        case 15:return api->install_inline_hook(at(fo,b::new_bar),
            reinterpret_cast<void*>(&a2fo_refit_loading_bar),b::new_bar_entry.size(),
            b::new_bar_entry.data(),&hook);
        case 16:next.store(hook.gateway,std::memory_order_release);return 0;
        case 17:players_ready=player_bindings();return players_ready;
        case 18:log("Loading player/race bindings verified; native names and transport percentages enabled");return 0;
        case 19:log("Loading player bindings differ; player panel omitted safely");return 0;
        case 20:return api->patch_call!=nullptr;
        case 21:return matches(engine,b::status_site,b::status_call);
        case 22:return api->patch_call(at(engine,b::status_site),
            reinterpret_cast<void*>(&a2fo_refit_loading_status),b::status_call.data(),
            b::status_call.size());
        case 23:log("Loading status observer armed; engine step text preserved");return 0;
        case 24:return c.animate;
        case 25:return install_animation();
        case 26:log("Loading animation lifetime bindings unavailable; native loading cadence/transient texture retained");return 0;
        case 27:enabled.store(true,std::memory_order_release);return 0;
        case 28:log("Armed: Refit loading presentation; FO owns progress, ODF bar, tips, sprites and loading music");return 0;
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
bool initialize(const A2FO_ModuleApi* supplied,HMODULE armada,HMODULE fleetops,bool animate_between_steps){
    InitContext c{supplied,armada,fleetops,animate_between_steps,{}};
    const bool result=a2fo_rs_stage8_b_loading_init(&c,init_op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
void remember_setup_preview(const Artwork& image){
    if(!enabled.load(std::memory_order_acquire)||image.width<1||image.height<1||
       image.width>1024||image.height>1024||
       image.pixels.size()!=std::size_t(image.width)*image.height)return;
    const auto map=map_details().first;
    if(map.empty())return;
    const std::lock_guard<std::mutex> lock(preview_mutex);
    setup_preview=image;preview_map=map;
}
struct MissionContext {const A2FO_MenuMissionV1* mission;std::exception_ptr error;};
std::uint64_t mission_op(void* opaque,std::uint32_t id,std::uint32_t) noexcept {
    auto& c=*static_cast<MissionContext*>(opaque);
    const auto* mission=c.mission;
    try{
        switch(id){
        case 0:return mission!=nullptr;
        case 1:return mission->size==sizeof(*mission);
        case 2:return mission->version==1;
        case 3:return std::memchr(mission->map,0,sizeof(mission->map))!=nullptr;
        case 4:return std::memchr(mission->title,0,sizeof(mission->title))!=nullptr;
        case 5:return std::memchr(mission->briefing,0,sizeof(mission->briefing))!=nullptr;
        case 6:return std::memchr(mission->objectives,0,sizeof(mission->objectives))!=nullptr;
        case 7:return !sanitize_loading_map_name(mission->map).empty();
        case 8:{const std::lock_guard<std::mutex> lock(mission_mutex);mission_metadata=*mission;return 0;}
        default:return 0;
        }
    }catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
bool set_mission(const A2FO_MenuMissionV1* mission){
    MissionContext c{mission,{}};
    const bool result=a2fo_rs_stage8_b_loading_mission(&c,mission_op)!=0;
    if(c.error)std::rethrow_exception(c.error);
    return result;
}
void clear_mission(){const std::lock_guard<std::mutex> lock(mission_mutex);mission_metadata={};}
void shutdown(){enabled.store(false,std::memory_order_release);}
}
