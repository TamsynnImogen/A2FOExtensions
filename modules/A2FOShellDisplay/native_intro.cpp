#include "native_intro.hpp"
#include "../../rust/intro_runtime_ffi.h"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

namespace a2fo::shell_display::native_intro {
// Owned by one synchronous movie scope. Reused between decoded frames and
// recreated only if the client dimensions change. No source pixels survive
// the native surface unlock; only this composed image does.
struct FrameBuffer {
    HDC dc{};
    HBITMAP bitmap{};
    HGDIOBJ previous{};
    int width{}, height{}, fitted_width{}, fitted_height{}, left{}, top{}, pitch{};
    unsigned source_width{}, source_height{};
    void clear() noexcept {
        if (dc && previous) SelectObject(dc, previous);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
        dc = nullptr; bitmap = nullptr; previous = nullptr; width = height = 0;
    }
    ~FrameBuffer() { clear(); }
    bool resize(int w, int h) noexcept {
        if (dc && width == w && height == h) return true;
        clear();
        dc = CreateCompatibleDC(nullptr);
        if (!dc) return false;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w;
        info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits{};
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap || !bits) { clear(); return false; }
        previous = SelectObject(dc, bitmap);
        if (!previous || previous == HGDI_ERROR) {
            previous = nullptr; clear(); return false;
        }
        width = w; height = h;
        return true;
    }
};
namespace {
constexpr std::uintptr_t kNextFrameRva = 0x637f0;
constexpr std::uintptr_t kNextFrameSlot = 0x210874;
constexpr std::uintptr_t kCopyCallRva = 0x638af;
constexpr std::uint8_t kNextFrameBytes[] = {0x55,0x8b,0xec,0x83,0xec,0x28};
constexpr std::uint8_t kCopyCallBytes[] = {0xff,0x15,0xa0,0x82,0x7b,0x00};
using NextFrame = int (__cdecl*)(void*, bool, int*, bool);
using CopyFrame = int (WINAPI*)(void*, void*, int, unsigned, unsigned, unsigned, unsigned);
const A2FO_ModuleApi* g_api{};
HMODULE g_armada{};
NextFrame g_next{};
CopyFrame g_copy{};
std::atomic<bool> g_ready{false};
std::atomic<unsigned> g_logs{0};
thread_local MovieState g_movie;
thread_local FrameBuffer* g_buffer{};
thread_local A2foRsIntroFrame g_frame{};

void* at(HMODULE image, std::uintptr_t rva) noexcept {
    return reinterpret_cast<std::uint8_t*>(image) + rva;
}
void log(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log("A2FOShellDisplay", message);
}
struct DrawContext {
    void* bink; const void* pixels; int pitch; BITMAPINFO bitmap{};
};
std::uintptr_t draw_step(void* value, std::uint32_t op, A2foRsIntroPlan* p) noexcept {
    auto& c=*static_cast<DrawContext*>(value);
    switch(op) {
    case 0: {const auto* d=static_cast<const std::uint32_t*>(c.bink);p->source_width=d[0];p->source_height=d[1];return 1;}
    case 1: {RECT client{};const auto okay=GetClientRect(g_movie.window,&client);p->width=client.right-client.left;p->height=client.bottom-client.top;return okay!=FALSE;}
    case 2: return g_buffer && g_buffer->resize(p->width,p->height);
    case 3:
        c.bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        c.bitmap.bmiHeader.biWidth=c.pitch/4;c.bitmap.bmiHeader.biHeight=-static_cast<LONG>(p->source_height);
        c.bitmap.bmiHeader.biPlanes=1;c.bitmap.bmiHeader.biBitCount=32;c.bitmap.bmiHeader.biCompression=BI_RGB;
        SetStretchBltMode(g_buffer->dc,HALFTONE);SetBrushOrgEx(g_buffer->dc,0,0,nullptr);return 1;
    case 4: return PatBlt(g_buffer->dc,0,0,p->width,p->height,BLACKNESS)!=FALSE;
    case 5: return static_cast<std::uint32_t>(StretchDIBits(g_buffer->dc,p->left,p->top,p->fitted_width,p->fitted_height,
        0,0,static_cast<int>(p->source_width),static_cast<int>(p->source_height),c.pixels,&c.bitmap,DIB_RGB_COLORS,SRCCOPY));
    case 6: return GdiFlush()!=FALSE;
    case 7:
        g_buffer->source_width=p->source_width;g_buffer->source_height=p->source_height;
        g_buffer->fitted_width=p->fitted_width;g_buffer->fitted_height=p->fitted_height;
        g_buffer->left=p->left;g_buffer->top=p->top;g_buffer->pitch=p->pitch;return 1;
    default:return 0;
    }
}
bool draw_frame(void* bink,const void* pixels,int pitch,unsigned height,unsigned x,unsigned y,unsigned flags) noexcept {
    DrawContext context{bink,pixels,pitch,{}};
    return a2fo_rs_intro_draw(bink!=nullptr,pixels!=nullptr,pitch,height,x,y,flags,&draw_step,&context)!=0;
}
struct PresentContext { HDC dc{}; };
std::uintptr_t present_step(void* value,std::uint32_t op,A2foRsIntroPlan*) noexcept {
    auto& c=*static_cast<PresentContext*>(value);
    switch(op) {
    case 0:c.dc=GetDC(g_movie.window);return reinterpret_cast<std::uintptr_t>(c.dc);
    case 1:return BitBlt(c.dc,0,0,g_buffer->width,g_buffer->height,g_buffer->dc,0,0,SRCCOPY)!=FALSE;
    case 2:return GdiFlush()!=FALSE;
    case 3:ReleaseDC(g_movie.window,c.dc);return 1;
    case 4:return g_logs.fetch_add(1,std::memory_order_relaxed);
    case 5: {
        char message[288]{};
        std::snprintf(message,sizeof(message),
            "Native intro frame fitted: source=%ux%u; client=%dx%d; destination=%dx%d at %d,%d; pitch=%d; buffered present after surface release",
            g_buffer->source_width,g_buffer->source_height,g_buffer->width,g_buffer->height,
            g_buffer->fitted_width,g_buffer->fitted_height,g_buffer->left,g_buffer->top,g_buffer->pitch);
        log(message);return 1;
    }
    default:return 0;
    }
}
bool present_frame() noexcept {
    PresentContext context;
    return a2fo_rs_intro_present(g_buffer && g_buffer->dc,&present_step,&context)!=0;
}
struct CopyContext {void* bink;const void* pixels;int pitch;unsigned height,x,y,flags;};
std::uint8_t copy_draw(void* value) noexcept {
    auto& c=*static_cast<CopyContext*>(value);
    return draw_frame(c.bink,c.pixels,c.pitch,c.height,c.x,c.y,c.flags)?1:0;
}
int WINAPI copy_hook(void* bink,void* buffer,int pitch,unsigned height,unsigned x,unsigned y,unsigned flags) noexcept {
    const int result=g_copy(bink,buffer,pitch,height,x,y,flags);
    CopyContext context{bink,buffer,pitch,height,x,y,flags};
    a2fo_rs_intro_copy(&g_frame,g_ready.load(),g_movie.enabled,bink==g_movie.bink,result,&copy_draw,&context);
    return result;
}

void native_flip() noexcept {
    // Same owner, active display and thiscall vtable entry used by
    // NextBinkFrame at Armada +6381b/+6382a/+638d8, after unlocking/releasing.
    auto* engine = *reinterpret_cast<std::uint8_t**>(at(g_armada, 0x3ad508));
    if (!engine) return;
    const unsigned display = *reinterpret_cast<unsigned*>(engine + 0xc0);
    if (display > 1) return;
    void* output = *reinterpret_cast<void**>(engine + 0xcc + display * sizeof(void*));
    if (!output) return;
    void** table = *static_cast<void***>(output);
    // Fastcall with an unused EDX slot expresses this no-stack-argument
    // thiscall method without a non-class thiscall compiler extension.
    using Flip = void (__fastcall*)(void*, void*);
    reinterpret_cast<Flip>(table[0x68 / sizeof(void*)])(output, nullptr);
}

struct NextContext {void* bink;bool show;int* repeats;bool decode;};
std::int32_t next_step(void* value,std::uint32_t op) noexcept {
    auto& c=*static_cast<NextContext*>(value);
    switch(op) {
    case 0:return g_next(c.bink,c.show,c.repeats,c.decode);
    case 1:return g_next(c.bink,false,c.repeats,c.decode);
    case 2:return present_frame()?1:0;
    case 3:native_flip();return 0;
    default:return 0;
    }
}
int __cdecl next_frame_hook(void* bink,bool show,int* repeats,bool decode) noexcept {
    NextContext context{bink,show,repeats,decode};
    return a2fo_rs_intro_next(&g_frame,g_ready.load(),g_movie.enabled,bink==g_movie.bink,show,&next_step,&context);
}
}

Scope::Scope(void* bink, HWND window, bool enabled) noexcept
    : previous_(g_movie), previous_buffer_(g_buffer) {
    g_movie = MovieState{bink, window, a2fo_rs_intro_scope(enabled,g_ready.load(),bink!=nullptr,window!=nullptr)!=0};
    if (g_movie.enabled) buffer_ = new (std::nothrow) FrameBuffer;
    g_movie.enabled = g_movie.enabled && buffer_;
    g_buffer = buffer_;
}
Scope::~Scope() {
    g_movie = previous_; g_buffer = previous_buffer_;
    delete buffer_;
}

namespace {
struct InitContext {
    const A2FO_ModuleApi* api; HMODULE armada,fleetops,bink{};
    void* next{};void* copy_call{};void* slot{};void* original{};void* imported{};
};
std::uintptr_t init_step(void* value,std::uint32_t op,A2foRsIntroPlan*) noexcept {
    auto& c=*static_cast<InitContext*>(value);
    switch(op) {
    case 0:
        g_api=c.api;g_armada=c.armada;c.next=at(c.armada,kNextFrameRva);
        c.copy_call=at(c.armada,kCopyCallRva);c.slot=at(c.fleetops,kNextFrameSlot);return 1;
    case 1:std::memcpy(&c.original,c.slot,sizeof(c.original));return reinterpret_cast<std::uintptr_t>(c.original);
    case 2:return std::memcmp(c.next,kNextFrameBytes,sizeof(kNextFrameBytes))==0;
    case 3:return std::memcmp(c.copy_call,kCopyCallBytes,sizeof(kCopyCallBytes))==0;
    case 4:
        c.bink=GetModuleHandleA("A2FOExtensions.dll");
        if (!c.bink || !GetProcAddress(c.bink,"_BinkCopyToBuffer@28")) c.bink=GetModuleHandleA("binkw32.dll");
        return reinterpret_cast<std::uintptr_t>(c.bink);
    case 5:return reinterpret_cast<std::uintptr_t>(GetProcAddress(c.bink,"_BinkCopyToBuffer@28"));
    case 6:std::memcpy(&c.imported,at(c.armada,0x3b82a0),sizeof(c.imported));return reinterpret_cast<std::uintptr_t>(c.imported);
    case 7:g_copy=reinterpret_cast<CopyFrame>(c.imported);g_next=reinterpret_cast<NextFrame>(c.next);return 1;
    case 8:return c.api->patch_call(c.copy_call,reinterpret_cast<void*>(&copy_hook),kCopyCallBytes,sizeof(kCopyCallBytes));
    case 9: {const auto replacement=reinterpret_cast<void*>(&next_frame_hook);return c.api->patch_bytes(c.slot,
        reinterpret_cast<const std::uint8_t*>(&replacement),reinterpret_cast<const std::uint8_t*>(&c.original),sizeof(c.original));}
    case 10:log("Native intro frame signatures/owner mismatch; existing movie presentation retained");return 1;
    case 11:log("Native Bink copy import has another owner; existing movie presentation retained");return 1;
    case 12:log("Native intro frame install incomplete; callbacks remain native");return 1;
    case 13:g_ready=true;log("Native engine intro frame fitting enabled; buffered 32-bit frames presented after native surface release");return 1;
    default:return 0;
    }
}
}
bool initialize(const A2FO_ModuleApi* api,HMODULE armada,HMODULE fleetops) noexcept {
    g_ready=false;
    const bool valid=api && armada && fleetops && api->patch_call &&
        A2FO_MODULE_API_HAS(api,patch_bytes) && api->patch_bytes;
    InitContext context{api,armada,fleetops};
    return a2fo_rs_intro_init(valid,reinterpret_cast<std::uintptr_t>(armada)+kNextFrameRva,&init_step,&context)!=0;
}
void shutdown() noexcept { g_ready = false; }
}
