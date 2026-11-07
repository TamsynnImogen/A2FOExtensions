#include "bink_video.hpp"
#include "presentation.hpp"
#include "native_intro.hpp"
#include "../../rust/stage8_b_bink_video_ffi.h"

#include <d3d9.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>

extern "C" void __cdecl a2fo_shell_set_position(void*, void*, int);

namespace a2fo::shell_display {
namespace {

constexpr const char* kModuleName = "A2FOShellDisplay";

// Fleet Ops TBinkIntro renderer.
constexpr std::uintptr_t kBinkRenderRva = 0x1aa470;
constexpr std::uintptr_t kBinkRenderCallRva = 0x1aa6ab;
// The constructor gets the native HWND before creating its auto-sized device.
// play is shared by both D3D9 and the original engine movie renderer.
constexpr std::uintptr_t kBinkWindowCallRva = 0x1aa3bb;
constexpr std::uintptr_t kBinkWindowRva = 0x1ddb94;
constexpr std::uintptr_t kBinkPlayCallRva = 0x1d3d3d;
constexpr std::uintptr_t kBinkPlayRva = 0x1aa634;
constexpr std::size_t kBinkDeviceOffset = 0x44;
constexpr std::size_t kBinkHandleOffset = 0x50;

// Armada movie paths.
constexpr std::uintptr_t kArmadaBinkSetDibitsCallRva = 0x0639ba;
constexpr std::uintptr_t kArmadaBinkTextureRenderRva = 0x064350;
constexpr std::uintptr_t kArmadaBinkTextureRenderCallRva1 = 0x0e5da5;
constexpr std::uintptr_t kArmadaBinkTextureRenderCallRva2 = 0x0e6153;
constexpr std::uintptr_t kArmadaBinkTextureSizeRva = 0x06406e;
constexpr std::uintptr_t kArmadaBinkTextureUvScaleRva = 0x2afb14;
// Bink texture setup's two FMUL instructions. The original reciprocal data
// is shared with radar and other UI code; redirect only these movie operands.
constexpr std::uintptr_t kArmadaBinkTextureWidthUvInstructionRva = 0x0640b6;
constexpr std::uintptr_t kArmadaBinkTextureHeightUvInstructionRva = 0x0640ce;
constexpr std::size_t kBinkTextureSetupPatchSize = 0x66;
constexpr float kBinkTextureWidthReciprocal = 1.0f / 512.0f;
constexpr float kBinkTextureHeightReciprocal = 1.0f / 256.0f;
constexpr std::uintptr_t kArmadaViewportOwnerGlobalRva = 0x365010;
constexpr std::size_t kArmadaViewportRectOffset = 0x1b0;
constexpr std::uintptr_t kArmadaShellStateRva = 0x003a8980;
constexpr std::int32_t kArmadaMainMenuShellState = 1;

constexpr std::uint8_t kExpectedBinkRenderCall[] =
    {0xe8, 0xc0, 0xfd, 0xff, 0xff};
constexpr std::uint8_t kExpectedBinkRender[] =
    {0x53, 0x83, 0xc4, 0x80, 0x8b, 0xd8};
constexpr std::uint8_t kExpectedBinkWindowCall[] =
    {0xe8, 0xd4, 0x37, 0x03, 0x00};
constexpr std::uint8_t kExpectedBinkWindow[] =
    {0x33, 0xc0, 0xba}; // following address is relocated by the loader
constexpr std::uint8_t kExpectedBinkPlayCall[] =
    {0xe8, 0xf2, 0x68, 0xfd, 0xff};
constexpr std::uint8_t kExpectedBinkPlay[] =
    {0x53, 0x56, 0x57, 0x55, 0x83, 0xc4, 0xe0, 0x8b, 0xf8};
constexpr std::uint8_t kExpectedArmadaBinkSetDibitsCall[] =
    {0xff, 0x15, 0x98, 0x7a, 0x7b, 0x00};
constexpr std::uint8_t kExpectedArmadaBinkTextureRender[] =
    {0x55, 0x8b, 0xec, 0x6a, 0xff};
constexpr std::uint8_t kExpectedArmadaBinkTextureRenderCall1[] =
    {0xe8, 0xa6, 0xe5, 0xf7, 0xff};
constexpr std::uint8_t kExpectedArmadaBinkTextureRenderCall2[] =
    {0xe8, 0xf8, 0xe1, 0xf7, 0xff};

constexpr std::uint8_t kExpectedArmadaBinkTextureSize[] =
    {0x68, 0x80, 0x00, 0x00, 0x00,
     0x68, 0x00, 0x01, 0x00, 0x00};
constexpr std::uint8_t kReplacementArmadaBinkTextureSize[] =
    {0x68, 0x00, 0x01, 0x00, 0x00,
     0x68, 0x00, 0x02, 0x00, 0x00};

constexpr std::uint8_t kExpectedArmadaBinkTextureUvScale[] =
    {0x00, 0x00, 0x00, 0x3c,
     0x00, 0x00, 0x80, 0x3b};
std::array<std::uint8_t,6> bink_uv_instruction(const void* reciprocal) noexcept {
    static_assert(sizeof(std::uintptr_t)==4,"Armada instruction operands are PE32 addresses");
    std::array<std::uint8_t,6> result{{0xd8,0x0d,0,0,0,0}};
    const auto address=reinterpret_cast<std::uintptr_t>(reciprocal);
    std::memcpy(result.data()+2,&address,sizeof(address));
    return result;
}

using BinkRenderFunction =
    void (__attribute__((regparm(1))) *)(void* bink_intro);

struct ArmadaFloatRect {
    float x;
    float y;
    float width;
    float height;
};

using ArmadaBinkTextureRenderFunction =
    void (__cdecl *)(const ArmadaFloatRect* rectangle);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
BinkRenderFunction g_bink_render = nullptr;
using BinkPlayFunction = bool (__attribute__((regparm(1))) *)(void*);
using BinkWindowFunction = HWND (__cdecl*)();
BinkPlayFunction g_bink_play = nullptr;
BinkWindowFunction g_bink_window = nullptr;
std::atomic<bool> g_movie_ready{false};
ArmadaBinkTextureRenderFunction g_armada_bink_texture_render = nullptr;
std::atomic<unsigned> g_viewport_log_count{0};
std::atomic<unsigned> g_window_log_count{0};
std::atomic<unsigned> g_gdi_log_count{0};
std::atomic<unsigned> g_texture_log_count{0};
std::atomic<unsigned> g_shell_bypass_log_count{0};
std::atomic<unsigned> g_embedded_bypass_log_count{0};
std::atomic<long> g_last_shell_state{0x7fffffffL};
std::atomic<std::uintptr_t> g_last_intro_window{0};
std::atomic<unsigned> g_last_intro_generation{0};

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return reinterpret_cast<std::uint8_t*>(module) + rva;
}

void log_message(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message);
}

template <typename T>
bool bytes_match(const T* address, const std::uint8_t* expected,
                 std::size_t size) noexcept {
    return address && expected && std::memcmp(address, expected, size) == 0;
}

bool sane_dimension(unsigned value) noexcept {
    return value >= 1 && value <= 16384;
}


bool seed_presentation_from_fo_settings(bool refresh = false) noexcept {
    struct Context {
        bool refresh;presentation::State existing{};bool have_existing{};
        const std::uint8_t* bytes{};int width{},height{},depth{},adapter{};
        bool windowed{};DEVMODEA mode{};DISPLAY_DEVICEA display{};
        std::exception_ptr error{};
    } c{refresh};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            const auto read_i32=[&](std::size_t offset) noexcept {
                std::int32_t value=0;
                std::memcpy(&value,c.bytes+offset,sizeof(value));
                return value;
            };
            switch(id) {
            case 100:c.have_existing=presentation::snapshot(c.existing);return 0;
            case 101:return c.have_existing&&!c.refresh;
            case 102:return g_fleet_ops!=nullptr;
            case 103:c.bytes=static_cast<const std::uint8_t*>(
                *reinterpret_cast<void**>(at(g_fleet_ops,0x20e644)));return 0;
            case 104:return c.bytes!=nullptr;
            case 105:c.width=read_i32(0x30);return 0;
            case 106:c.height=read_i32(0x34);return 0;
            case 107:c.depth=read_i32(0x38);return 0;
            case 108:c.adapter=read_i32(0x64);return 0;
            case 109:c.windowed=c.bytes[0x72]!=0;return 0;
            case 110:return c.width<=0;
            case 111:return c.height<=0;
            case 112:c.mode.dmSize=sizeof(c.mode);
                c.display.cb=sizeof(c.display);
                return c.adapter>=0;
            case 113:return EnumDisplayDevicesA(nullptr,
                static_cast<DWORD>(c.adapter),&c.display,0)!=FALSE;
            case 114:return EnumDisplaySettingsA(c.display.DeviceName,
                ENUM_CURRENT_SETTINGS,&c.mode)!=FALSE;
            case 115:c.width=static_cast<int>(c.mode.dmPelsWidth);return 0;
            case 116:c.height=static_cast<int>(c.mode.dmPelsHeight);return 0;
            case 117:return c.depth!=16&&c.depth!=32;
            case 118:c.depth=static_cast<int>(c.mode.dmBitsPerPel);return 0;
            case 119:c.width=GetSystemMetrics(SM_CXSCREEN);return 0;
            case 120:c.height=GetSystemMetrics(SM_CYSCREEN);return 0;
            case 121:return c.depth!=16&&c.depth!=32;
            case 122:c.depth=32;return 0;
            case 123:return valid_mode(Mode{c.width,c.height,c.depth});
            case 124:return c.have_existing&&c.existing.width==c.width&&
                c.existing.height==c.height&&c.existing.depth==c.depth&&
                c.existing.borderless==!c.windowed;
            case 125:presentation::publish(Mode{c.width,c.height,c.depth},
                !c.windowed,nullptr,c.width,c.height);return 0;
            case 126:return c.refresh&&c.have_existing;
            case 127:{char message[192]{};
                std::snprintf(message,sizeof(message),
                    "Intro saved resolution restored: temporary=%dx%d; selected=%dx%d; %s",
                    c.existing.width,c.existing.height,c.width,c.height,
                    c.windowed?"windowed":"borderless");
                log_message(message);return 0;}
            case 128:{static std::atomic<unsigned> logged{0};
                return logged.fetch_add(1,std::memory_order_relaxed)==0;}
            case 129:{char message[192]{};
                std::snprintf(message,sizeof(message),
                    "Intro presentation seeded from FoSettings: %dx%dx%d, %s, adapter=%d",
                    c.width,c.height,c.depth,c.windowed?"windowed":"borderless",
                    c.adapter);
                log_message(message);return 0;}
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_bink_seed_presentation(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}
void fit_dimensions(unsigned source_width, unsigned source_height,
                    unsigned target_width, unsigned target_height,
                    unsigned& fitted_width, unsigned& fitted_height) noexcept {
    a2fo_rs_stage8_b_bink_fit_dimensions(source_width,source_height,
        target_width,target_height,&fitted_width,&fitted_height);
}

bool preserve_native_shell_bink(std::int32_t* shell_state_out = nullptr) noexcept {
    struct Context {std::int32_t* output;std::int32_t state{};
        std::exception_ptr error{};} c{shell_state_out};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 90:return g_armada!=nullptr;
        case 91:c.state=*reinterpret_cast<const std::int32_t*>(
            at(g_armada,kArmadaShellStateRva));return 0;
        case 92:if(c.output)*c.output=c.state;return 0;
        case 93:return g_last_shell_state.exchange(static_cast<long>(c.state),
            std::memory_order_relaxed)!=static_cast<long>(c.state);
        case 94:{char message[160]{};
            std::snprintf(message,sizeof(message),
                "Armada Bink shell state changed to %ld; scaling=%s",
                static_cast<long>(c.state),
                c.state>kArmadaMainMenuShellState?"bypassed":"enabled");
            log_message(message);return 0;}
        case 95:return c.state>kArmadaMainMenuShellState;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_bink_preserve_shell(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}

void log_native_shell_bypass_once(std::int32_t shell_state) noexcept {
    struct Context {std::int32_t state;std::exception_ptr error{};} c{shell_state};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 600:return g_shell_bypass_log_count.fetch_add(1,
            std::memory_order_relaxed)==0;
        case 601:{char message[192]{};
            std::snprintf(message,sizeof(message),
                "Armada Bink scaling bypassed for native nested shell state %ld "
                "(Single Player/menu layout preserved)",
                static_cast<long>(c.state));log_message(message);return 0;}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_bink_log_once(&c,op);
    if(c.error)std::terminate();
}

bool apply_intro_window(HWND window) noexcept {
    struct Context {
        HWND window;presentation::State state{};
        std::uintptr_t window_value{};int client_width{},client_height{};
        HWND monitor_window{};HMONITOR monitor{};MONITORINFO info{};
        RECT cached_client{};LONG_PTR cached_style{},style{},ex_style{};
        LONG_PTR shell_style{},shell_ex_style{};
        void* form{};RECT outer{},area{},actual{},log_client{};
        int outer_width{},outer_height{},x{},y{};
        std::exception_ptr error{};
    } c{window};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            constexpr LONG_PTR frame_mask=WS_POPUP|WS_CAPTION|WS_THICKFRAME|
                WS_BORDER|WS_DLGFRAME|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX;
            constexpr LONG_PTR edge_mask=WS_EX_DLGMODALFRAME|WS_EX_WINDOWEDGE|
                WS_EX_CLIENTEDGE|WS_EX_STATICEDGE;
            switch(id) {
            case 500:return c.window!=nullptr;
            case 501:return presentation::snapshot(c.state);
            case 502:return seed_presentation_from_fo_settings();
            case 503:return presentation::snapshot(c.state);
            case 504:c.window_value=reinterpret_cast<std::uintptr_t>(c.window);
                c.client_width=c.state.client_width>0?c.state.client_width:
                    c.state.width;
                c.client_height=c.state.client_height>0?c.state.client_height:
                    c.state.height;return 0;
            case 505:return c.client_width>0;
            case 506:return c.client_height>0;
            case 507:return c.state.shell_window!=nullptr;
            case 508:return IsWindow(c.state.shell_window)!=FALSE;
            case 509:c.monitor_window=c.state.shell_window;return 0;
            case 510:c.monitor_window=c.window;return 0;
            case 511:c.monitor=MonitorFromWindow(
                c.monitor_window,MONITOR_DEFAULTTOPRIMARY);
                c.info.cbSize=sizeof(c.info);return 0;
            case 512:return c.monitor!=nullptr;
            case 513:return GetMonitorInfoA(c.monitor,&c.info)!=FALSE;
            case 514:return c.info.rcMonitor.right>c.info.rcMonitor.left&&
                c.info.rcMonitor.bottom>c.info.rcMonitor.top;
            case 515:return c.state.borderless;
            case 516:c.client_width=c.info.rcMonitor.right-c.info.rcMonitor.left;
                c.client_height=c.info.rcMonitor.bottom-c.info.rcMonitor.top;
                return 0;
            case 517:c.cached_style=GetWindowLongPtrA(c.window,GWL_STYLE);
                return 0;
            case 518:return GetClientRect(c.window,&c.cached_client)!=FALSE;
            case 519:return c.cached_client.right-c.cached_client.left==
                c.client_width;
            case 520:return c.cached_client.bottom-c.cached_client.top==
                c.client_height;
            case 521:return c.state.borderless
                ?(c.cached_style&WS_POPUP)&&!(c.cached_style&WS_CAPTION)
                :(c.cached_style&WS_CAPTION)!=0;
            case 522:return g_last_intro_window.load(
                std::memory_order_relaxed)==c.window_value;
            case 523:return g_last_intro_generation.load(
                std::memory_order_relaxed)==c.state.generation;
            case 524:return c.state.shell_window!=nullptr;
            case 525:return c.state.shell_window==c.window;
            case 526:g_last_intro_window.store(c.window_value,
                std::memory_order_relaxed);return 0;
            case 527:g_last_intro_generation.store(c.state.generation,
                std::memory_order_relaxed);return 0;
            case 528:c.style=GetWindowLongPtrA(c.window,GWL_STYLE);return 0;
            case 529:c.ex_style=GetWindowLongPtrA(c.window,GWL_EXSTYLE);
                return 0;
            case 530:return c.state.shell_window!=nullptr;
            case 531:return IsWindow(c.state.shell_window)!=FALSE;
            case 532:c.shell_style=GetWindowLongPtrA(
                c.state.shell_window,GWL_STYLE);return 0;
            case 533:c.shell_ex_style=GetWindowLongPtrA(
                c.state.shell_window,GWL_EXSTYLE);return 0;
            case 534:c.style=(c.style&~frame_mask)|
                    (c.shell_style&frame_mask);
                c.ex_style=(c.ex_style&~edge_mask)|
                    (c.shell_ex_style&edge_mask);return 0;
            case 535:return c.state.borderless;
            case 536:c.style=(c.style&~frame_mask)|WS_POPUP;
                c.ex_style&=~edge_mask;return 0;
            case 537:c.style=(c.style&~frame_mask)|WS_CAPTION|WS_SYSMENU|
                    WS_MINIMIZEBOX|WS_BORDER;
                c.ex_style|=WS_EX_WINDOWEDGE;return 0;
            case 538:SetWindowLongPtrA(c.window,GWL_STYLE,c.style);return 0;
            case 539:SetWindowLongPtrA(c.window,GWL_EXSTYLE,c.ex_style);
                return 0;
            case 540:c.form=*reinterpret_cast<void**>(at(g_fleet_ops,0x24485c));
                return 0;
            case 541:return c.form!=nullptr;
            case 542:return *reinterpret_cast<HWND*>(
                static_cast<std::uint8_t*>(c.form)+0x1b4)==c.window;
            case 543:a2fo_shell_set_position(at(g_fleet_ops,0xad390),
                c.form,c.client_height);return 0;
            case 544:a2fo_shell_set_position(at(g_fleet_ops,0xad35c),
                c.form,c.client_width);return 0;
            case 545:c.outer=RECT{0,0,c.client_width,c.client_height};return 0;
            case 546:return c.state.borderless;
            case 547:return AdjustWindowRectEx(&c.outer,
                static_cast<DWORD>(c.style),FALSE,
                static_cast<DWORD>(c.ex_style))!=FALSE;
            case 548:c.outer=RECT{0,0,c.client_width,c.client_height};return 0;
            case 549:c.outer_width=c.outer.right-c.outer.left;
                c.outer_height=c.outer.bottom-c.outer.top;
                c.area=RECT{0,0,c.client_width,c.client_height};return 0;
            case 550:c.area=c.state.borderless?c.info.rcMonitor:c.info.rcWork;
                return 0;
            case 551:c.x=c.area.left+
                    ((c.area.right-c.area.left)-c.outer_width)/2;
                c.y=c.area.top+
                    ((c.area.bottom-c.area.top)-c.outer_height)/2;return 0;
            case 552:return SetWindowPos(c.window,nullptr,c.x,c.y,
                c.outer_width,c.outer_height,
                SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_FRAMECHANGED);
            case 553:return GetClientRect(c.window,&c.actual)!=FALSE;
            case 554:return c.actual.right-c.actual.left==c.client_width;
            case 555:return c.actual.bottom-c.actual.top==c.client_height;
            case 556:return c.state.client_width!=c.client_width||
                c.state.client_height!=c.client_height;
            case 557:presentation::publish(
                {c.state.width,c.state.height,c.state.depth},
                c.state.borderless,c.state.shell_window,c.client_width,
                c.client_height);return 0;
            case 558:presentation::snapshot(c.state);return 0;
            case 559:g_last_intro_window.store(c.window_value,
                std::memory_order_relaxed);return 0;
            case 560:g_last_intro_generation.store(c.state.generation,
                std::memory_order_relaxed);return 0;
            case 561:return g_window_log_count.fetch_add(1,
                std::memory_order_relaxed)==0;
            case 562:GetClientRect(c.window,&c.log_client);return 0;
            case 563:{char message[256]{};
                std::snprintf(message,sizeof(message),
                    "Intro window matched shell: %s; selected=%dx%d; client=%ldx%ld; generation=%u",
                    c.state.borderless?"borderless":"windowed",
                    c.state.width,c.state.height,
                    c.log_client.right-c.log_client.left,
                    c.log_client.bottom-c.log_client.top,c.state.generation);
                log_message(message);return 0;}
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_bink_apply_window(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}
HWND __cdecl bink_window_hook() noexcept {
    struct Context {HWND window{};std::exception_ptr error{};} c;
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 50:c.window=g_bink_window();return 0;
        case 51:return g_movie_ready.load();
        case 52:return preserve_native_shell_bink();
        case 53:seed_presentation_from_fo_settings(true);return 0;
        case 54:apply_intro_window(c.window);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_bink_window(&c,op);
    if(c.error)std::terminate();
    return c.window;
}

bool __attribute__((regparm(1))) bink_play_hook(void* intro) noexcept {
    struct Context {void* intro;HWND movie_window{};bool native_movie{};
        void* handle{};std::exception_ptr error{};} c{intro};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 60:return g_movie_ready.load();
        case 61:return c.intro!=nullptr;
        case 62:return preserve_native_shell_bink();
        case 63:seed_presentation_from_fo_settings(true);return 0;
        case 64:c.movie_window=g_bink_window();return 0;
        case 65:apply_intro_window(c.movie_window);return 0;
        case 66:c.native_movie=static_cast<const std::uint8_t*>(c.intro)[4]==0;
            return 0;
        case 67:{char message[128]{};
            std::snprintf(message,sizeof(message),"Intro movie start: renderer=%s",
                static_cast<const std::uint8_t*>(c.intro)[4]?"D3D9":"native engine");
            log_message(message);return 0;}
        case 68:c.handle=c.intro?*reinterpret_cast<void**>(
            static_cast<std::uint8_t*>(c.intro)+kBinkHandleOffset):nullptr;
            return 0;
        case 69:{native_intro::Scope presentation_scope(
                c.handle,c.movie_window,c.native_movie);
            return g_bink_play(c.intro);}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_bink_play(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}

void prepare_bink_viewport(void* bink_intro) noexcept {
    struct Context {
        void* intro;std::uint8_t* object{};
        IDirect3DDevice9* device{};D3DPRESENT_PARAMETERS present{};
        IDirect3DSwapChain9* swap_chain{};
        unsigned client_width{},client_height{};RECT client{};
        IDirect3DSurface9* render_target{};D3DSURFACE_DESC target{};
        HRESULT desc_result{},get_result{},set_result{};
        unsigned source_width{},source_height{},fitted_width{},fitted_height{};
        void* bink_handle{};D3DVIEWPORT9 desired{},current{};
        std::exception_ptr error{};
    } c{bink_intro};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 400:return c.intro!=nullptr;
        case 401:c.object=static_cast<std::uint8_t*>(c.intro);
            c.device=*reinterpret_cast<IDirect3DDevice9**>(
                c.object+kBinkDeviceOffset);return 0;
        case 402:return c.device!=nullptr;
        case 403:return SUCCEEDED(c.device->GetSwapChain(0,&c.swap_chain))&&
            c.swap_chain!=nullptr;
        case 404:return SUCCEEDED(c.swap_chain->GetPresentParameters(&c.present));
        case 405:return c.present.hDeviceWindow!=nullptr;
        case 406:apply_intro_window(c.present.hDeviceWindow);return 0;
        case 407:return GetClientRect(c.present.hDeviceWindow,&c.client)!=FALSE;
        case 408:return c.client.right>c.client.left&&
            c.client.bottom>c.client.top;
        case 409:c.client_width=static_cast<unsigned>(
                c.client.right-c.client.left);
            c.client_height=static_cast<unsigned>(
                c.client.bottom-c.client.top);return 0;
        case 410:c.swap_chain->Release();return 0;
        case 411:return SUCCEEDED(c.device->GetRenderTarget(0,&c.render_target))&&
            c.render_target!=nullptr;
        case 412:c.desc_result=c.render_target->GetDesc(&c.target);return 0;
        case 413:c.render_target->Release();return 0;
        case 414:return SUCCEEDED(c.desc_result);
        case 415:return sane_dimension(c.target.Width);
        case 416:return sane_dimension(c.target.Height);
        case 417:c.source_width=c.target.Width;c.source_height=c.target.Height;
            return 0;
        case 418:c.bink_handle=*reinterpret_cast<void**>(
            c.object+kBinkHandleOffset);return 0;
        case 419:return c.bink_handle!=nullptr;
        case 420:return sane_dimension(static_cast<const std::uint32_t*>(
            c.bink_handle)[0]);
        case 421:return sane_dimension(static_cast<const std::uint32_t*>(
            c.bink_handle)[1]);
        case 422:c.source_width=static_cast<const std::uint32_t*>(
                c.bink_handle)[0];
            c.source_height=static_cast<const std::uint32_t*>(
                c.bink_handle)[1];return 0;
        case 423:c.fitted_width=c.target.Width;c.fitted_height=c.target.Height;
            fit_dimensions(c.source_width,c.source_height,c.target.Width,
                c.target.Height,c.fitted_width,c.fitted_height);return 0;
        case 424:c.desired.X=(c.target.Width-c.fitted_width)/2;
            c.desired.Y=(c.target.Height-c.fitted_height)/2;
            c.desired.Width=c.fitted_width;c.desired.Height=c.fitted_height;
            c.desired.MinZ=0.0f;c.desired.MaxZ=1.0f;return 0;
        case 425:c.get_result=c.device->GetViewport(&c.current);return 0;
        case 426:c.set_result=c.get_result;return 0;
        case 427:return SUCCEEDED(c.get_result);
        case 428:return c.current.X!=c.desired.X||
            c.current.Y!=c.desired.Y||
            c.current.Width!=c.desired.Width||
            c.current.Height!=c.desired.Height||
            c.current.MinZ!=c.desired.MinZ||
            c.current.MaxZ!=c.desired.MaxZ;
        case 429:c.set_result=c.device->SetViewport(&c.desired);return 0;
        case 430:return g_viewport_log_count.fetch_add(1,
            std::memory_order_relaxed)==0;
        case 431:{char message[320]{};
            std::snprintf(message,sizeof(message),
                "Intro Bink viewport: source=%ux%u; target=%lux%lu; fitted=%lux%lu at %lu,%lu; "
                "backbuffer=%lux%lu; client=%ux%u; windowed=%s; hr=%08lx",
                c.source_width,c.source_height,
                static_cast<unsigned long>(c.target.Width),
                static_cast<unsigned long>(c.target.Height),
                static_cast<unsigned long>(c.desired.Width),
                static_cast<unsigned long>(c.desired.Height),
                static_cast<unsigned long>(c.desired.X),
                static_cast<unsigned long>(c.desired.Y),
                static_cast<unsigned long>(c.present.BackBufferWidth),
                static_cast<unsigned long>(c.present.BackBufferHeight),
                c.client_width,c.client_height,c.present.Windowed?"yes":"no",
                static_cast<unsigned long>(c.set_result));
            log_message(message);return 0;}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_bink_prepare_viewport(&c,op);
    if(c.error)std::terminate();
}
void __attribute__((regparm(1))) bink_render_hook(void* bink_intro) noexcept {
    struct Context {void* intro;std::exception_ptr error{};} c{bink_intro};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 80:return g_movie_ready.load();
        case 81:prepare_bink_viewport(c.intro);return 0;
        case 82:g_bink_render(c.intro);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_bink_render(&c,op);
    if(c.error)std::terminate();
}

int WINAPI armada_bink_set_dibits_scaled(
    HDC device_context, int destination_x, int destination_y,
    DWORD width, DWORD height, int source_x, int source_y,
    UINT first_scan, UINT scan_count, const void* pixels,
    const BITMAPINFO* bitmap_info, UINT color_use) noexcept {
    struct Context {
        HDC dc;int destination_x,destination_y;DWORD width,height;
        int source_x,source_y;UINT first_scan,scan_count;
        const void* pixels;const BITMAPINFO* bitmap_info;UINT color_use;
        int result{};std::int32_t shell_state{};RECT target{};
        HWND window{};unsigned target_width{},target_height{};
        unsigned fitted_width{},fitted_height{};int fitted_x{},fitted_y{};
        std::exception_ptr error{};
    } c{device_context,destination_x,destination_y,width,height,source_x,
        source_y,first_scan,scan_count,pixels,bitmap_info,color_use};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 200:return g_movie_ready.load();
        case 201:return c.dc!=nullptr;
        case 202:return c.pixels!=nullptr;
        case 203:return c.bitmap_info!=nullptr;
        case 204:return c.width!=0;
        case 205:return c.height!=0;
        case 206:return c.first_scan==0;
        case 207:return c.scan_count==c.height;
        case 208:c.result=SetDIBitsToDevice(c.dc,c.destination_x,c.destination_y,
            c.width,c.height,c.source_x,c.source_y,c.first_scan,c.scan_count,
            c.pixels,c.bitmap_info,c.color_use);return 0;
        case 209:return preserve_native_shell_bink(&c.shell_state);
        case 210:log_native_shell_bypass_once(c.shell_state);return 0;
        case 211:c.window=WindowFromDC(c.dc);return 0;
        case 212:return c.window!=nullptr;
        case 213:apply_intro_window(c.window);return 0;
        case 214:return GetClientRect(c.window,&c.target)!=FALSE;
        case 215:return c.target.right>c.target.left&&
            c.target.bottom>c.target.top;
        case 216:return GetClipBox(c.dc,&c.target)!=ERROR;
        case 217:return c.target.right>c.target.left&&
            c.target.bottom>c.target.top;
        case 218:c.target_width=static_cast<unsigned>(
            c.target.right-c.target.left);
            c.target_height=static_cast<unsigned>(
                c.target.bottom-c.target.top);return 0;
        case 219:fit_dimensions(c.width,c.height,c.target_width,c.target_height,
            c.fitted_width,c.fitted_height);return 0;
        case 220:return c.fitted_width!=0;
        case 221:return c.fitted_height!=0;
        case 222:c.fitted_x=c.target.left+
            static_cast<int>((c.target_width-c.fitted_width)/2);
            c.fitted_y=c.target.top+
            static_cast<int>((c.target_height-c.fitted_height)/2);return 0;
        case 223:return g_gdi_log_count.fetch_add(1,
            std::memory_order_relaxed)==0;
        case 224:{char message[224]{};
            std::snprintf(message,sizeof(message),
                "Armada Bink GDI scaling: source=%lux%lu; client=%ux%u; "
                "destination=%ux%u at %d,%d",
                static_cast<unsigned long>(c.width),
                static_cast<unsigned long>(c.height),
                c.target_width,c.target_height,c.fitted_width,c.fitted_height,
                c.fitted_x,c.fitted_y);
            log_message(message);return 0;}
        case 225:c.result=StretchDIBits(c.dc,c.fitted_x,c.fitted_y,
            static_cast<int>(c.fitted_width),
            static_cast<int>(c.fitted_height),c.source_x,c.source_y,
            static_cast<int>(c.width),static_cast<int>(c.height),c.pixels,
            c.bitmap_info,c.color_use,SRCCOPY);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_bink_gdi(&c,op);
    if(c.error)std::terminate();
    return c.result;
}
bool preserve_embedded_bink(const ArmadaFloatRect& requested,
                            const ArmadaFloatRect& viewport) noexcept {
    constexpr float kInsetEpsilon = 1.0f;
    return std::fabs(requested.x - viewport.x) > kInsetEpsilon ||
           std::fabs(requested.y - viewport.y) > kInsetEpsilon;
}

void log_embedded_bypass_once(const ArmadaFloatRect& requested) noexcept {
    struct Context {const ArmadaFloatRect& requested;
        std::exception_ptr error{};} c{requested};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 600:return g_embedded_bypass_log_count.fetch_add(1,
            std::memory_order_relaxed)==0;
        case 601:{char message[224]{};
            std::snprintf(message,sizeof(message),
                "Armada Bink scaling bypassed for embedded gameplay movie: "
                "%.0fx%.0f at %.0f,%.0f",
                c.requested.width,c.requested.height,c.requested.x,
                c.requested.y);log_message(message);return 0;}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_bink_log_once(&c,op);
    if(c.error)std::terminate();
}

bool sane_viewport(const ArmadaFloatRect& rectangle) noexcept {
    return std::isfinite(rectangle.x) && std::isfinite(rectangle.y) &&
        std::isfinite(rectangle.width) && std::isfinite(rectangle.height) &&
        rectangle.width >= 1.0f && rectangle.height >= 1.0f &&
        rectangle.width <= 16384.0f && rectangle.height <= 16384.0f;
}

ArmadaFloatRect fit_rectangle(const ArmadaFloatRect& source,
                              const ArmadaFloatRect& target) noexcept {
    const unsigned source_width = static_cast<unsigned>(std::max(1.0f, source.width));
    const unsigned source_height = static_cast<unsigned>(std::max(1.0f, source.height));
    const unsigned target_width = static_cast<unsigned>(std::max(1.0f, target.width));
    const unsigned target_height = static_cast<unsigned>(std::max(1.0f, target.height));
    unsigned fitted_width = 0;
    unsigned fitted_height = 0;
    fit_dimensions(source_width, source_height, target_width, target_height,
                   fitted_width, fitted_height);

    ArmadaFloatRect result{};
    result.width = static_cast<float>(fitted_width);
    result.height = static_cast<float>(fitted_height);
    result.x = target.x + (target.width - result.width) * 0.5f;
    result.y = target.y + (target.height - result.height) * 0.5f;
    return result;
}

void __cdecl armada_bink_texture_render_scaled(
    const ArmadaFloatRect* requested_rectangle) noexcept {
    struct Context {
        const ArmadaFloatRect* requested;
        const ArmadaFloatRect* render_rectangle{};
        ArmadaFloatRect viewport{},fitted{};
        std::uint8_t* viewport_owner{};std::int32_t shell_state{};
        std::exception_ptr error{};
    } c{requested_rectangle};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 300:return g_movie_ready.load();
        case 301:g_armada_bink_texture_render(c.requested);return 0;
        case 302:return g_armada_bink_texture_render!=nullptr;
        case 303:return c.requested!=nullptr;
        case 304:return preserve_native_shell_bink(&c.shell_state);
        case 305:log_native_shell_bypass_once(c.shell_state);return 0;
        case 306:c.render_rectangle=c.requested;return 0;
        case 307:c.viewport=ArmadaFloatRect{};c.fitted=ArmadaFloatRect{};
            return 0;
        case 308:c.viewport_owner=*reinterpret_cast<std::uint8_t**>(
            at(g_armada,kArmadaViewportOwnerGlobalRva));
            return c.viewport_owner!=nullptr;
        case 309:std::memcpy(&c.viewport,
            c.viewport_owner+kArmadaViewportRectOffset,sizeof(c.viewport));
            return 0;
        case 310:return sane_viewport(c.viewport);
        case 311:return preserve_embedded_bink(*c.requested,c.viewport);
        case 312:log_embedded_bypass_once(*c.requested);return 0;
        case 313:c.fitted=fit_rectangle(*c.requested,c.viewport);return 0;
        case 314:c.render_rectangle=&c.fitted;return 0;
        case 315:return g_texture_log_count.fetch_add(1,
            std::memory_order_relaxed)==0;
        case 316:{char message[288]{};
            std::snprintf(message,sizeof(message),
                "Armada Bink texture scaling: requested=%.0fx%.0f at %.0f,%.0f; "
                "viewport=%.0fx%.0f at %.0f,%.0f; destination=%.0fx%.0f at %.0f,%.0f",
                c.requested->width,c.requested->height,c.requested->x,c.requested->y,
                c.viewport.width,c.viewport.height,c.viewport.x,c.viewport.y,
                c.render_rectangle->width,c.render_rectangle->height,
                c.render_rectangle->x,c.render_rectangle->y);
            log_message(message);return 0;}
        case 317:g_armada_bink_texture_render(c.render_rectangle);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_bink_texture(&c,op);
    if(c.error)std::terminate();
}
} // namespace

bool initialize_bink_video_scaling(const A2FO_ModuleApi* api,
                                   HMODULE armada) noexcept {
    struct Context {
        const A2FO_ModuleApi* api;HMODULE armada;HMODULE fleet_ops{};
        void* render{};void* call_site{};void* window_call{};
        void* window_getter{};void* play_call{};void* play{};
        void* armada_gdi_call{};void* armada_texture_render{};
        void* armada_texture_call1{};void* armada_texture_call2{};
        void* armada_texture_size{};void* armada_texture_uv_scale{};
        void* width_uv{};void* height_uv{};
        std::array<std::uint8_t,6> expected_width_uv{},expected_height_uv{};
        std::array<std::uint8_t,6> replacement_width_uv{},replacement_height_uv{};
        std::array<std::uint8_t,kBinkTextureSetupPatchSize> expected_setup{},
            replacement_setup{};
        std::exception_ptr error{};
    } c{api,armada};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 0:g_movie_ready=false;return 0;
        case 1:return c.api!=nullptr;
        case 2:return c.armada!=nullptr;
        case 3:return c.api->patch_call!=nullptr;
        case 4:return A2FO_MODULE_API_HAS(c.api,patch_bytes);
        case 5:return c.api->patch_bytes!=nullptr;
        case 6:return c.api->log!=nullptr;
        case 7:if(c.api&&c.api->log)c.api->log(kModuleName,
            "Bink presentation hooks unavailable in this core");return 0;
        case 8:c.fleet_ops=c.api->fleetops_module
            ?static_cast<HMODULE>(c.api->fleetops_module()):nullptr;
            return c.fleet_ops!=nullptr;
        case 9:g_fleet_ops=c.fleet_ops;return 0;
        case 10:{
            c.render=at(c.fleet_ops,kBinkRenderRva);
            c.call_site=at(c.fleet_ops,kBinkRenderCallRva);
            c.window_call=at(c.fleet_ops,kBinkWindowCallRva);
            c.window_getter=at(c.fleet_ops,kBinkWindowRva);
            c.play_call=at(c.fleet_ops,kBinkPlayCallRva);
            c.play=at(c.fleet_ops,kBinkPlayRva);
            c.armada_gdi_call=at(c.armada,kArmadaBinkSetDibitsCallRva);
            c.armada_texture_render=at(c.armada,kArmadaBinkTextureRenderRva);
            c.armada_texture_call1=at(c.armada,kArmadaBinkTextureRenderCallRva1);
            c.armada_texture_call2=at(c.armada,kArmadaBinkTextureRenderCallRva2);
            c.armada_texture_size=at(c.armada,kArmadaBinkTextureSizeRva);
            c.armada_texture_uv_scale=at(c.armada,kArmadaBinkTextureUvScaleRva);
            c.width_uv=at(c.armada,kArmadaBinkTextureWidthUvInstructionRva);
            c.height_uv=at(c.armada,kArmadaBinkTextureHeightUvInstructionRva);
            c.expected_width_uv=bink_uv_instruction(
                at(c.armada,kArmadaBinkTextureUvScaleRva+4));
            c.expected_height_uv=bink_uv_instruction(c.armada_texture_uv_scale);
            c.replacement_width_uv=bink_uv_instruction(&kBinkTextureWidthReciprocal);
            c.replacement_height_uv=bink_uv_instruction(&kBinkTextureHeightReciprocal);
            return 0;
        }
        case 11:return bytes_match(static_cast<const std::uint8_t*>(c.window_call),
            kExpectedBinkWindowCall,sizeof(kExpectedBinkWindowCall));
        case 12:return bytes_match(static_cast<const std::uint8_t*>(c.window_getter),
            kExpectedBinkWindow,sizeof(kExpectedBinkWindow));
        case 13:return bytes_match(static_cast<const std::uint8_t*>(c.play_call),
            kExpectedBinkPlayCall,sizeof(kExpectedBinkPlayCall));
        case 14:return bytes_match(static_cast<const std::uint8_t*>(c.play),
            kExpectedBinkPlay,sizeof(kExpectedBinkPlay));
        case 15:return bytes_match(static_cast<const std::uint8_t*>(c.render),
            kExpectedBinkRender,sizeof(kExpectedBinkRender));
        case 16:return bytes_match(static_cast<const std::uint8_t*>(c.call_site),
            kExpectedBinkRenderCall,sizeof(kExpectedBinkRenderCall));
        case 17:return bytes_match(static_cast<const std::uint8_t*>(c.armada_gdi_call),
            kExpectedArmadaBinkSetDibitsCall,sizeof(kExpectedArmadaBinkSetDibitsCall));
        case 18:return bytes_match(static_cast<const std::uint8_t*>(c.armada_texture_render),
            kExpectedArmadaBinkTextureRender,sizeof(kExpectedArmadaBinkTextureRender));
        case 19:return bytes_match(static_cast<const std::uint8_t*>(c.armada_texture_call1),
            kExpectedArmadaBinkTextureRenderCall1,sizeof(kExpectedArmadaBinkTextureRenderCall1));
        case 20:return bytes_match(static_cast<const std::uint8_t*>(c.armada_texture_call2),
            kExpectedArmadaBinkTextureRenderCall2,sizeof(kExpectedArmadaBinkTextureRenderCall2));
        case 21:return bytes_match(static_cast<const std::uint8_t*>(c.armada_texture_size),
            kExpectedArmadaBinkTextureSize,sizeof(kExpectedArmadaBinkTextureSize));
        case 22:return bytes_match(static_cast<const std::uint8_t*>(c.armada_texture_uv_scale),
            kExpectedArmadaBinkTextureUvScale,sizeof(kExpectedArmadaBinkTextureUvScale));
        case 23:return bytes_match(static_cast<const std::uint8_t*>(c.width_uv),
            c.expected_width_uv.data(),c.expected_width_uv.size());
        case 24:return bytes_match(static_cast<const std::uint8_t*>(c.height_uv),
            c.expected_height_uv.data(),c.expected_height_uv.size());
        case 25:c.api->log(kModuleName,
            "Bink renderer signatures mismatch; movie presentation fix disabled");
            return 0;
        case 26:g_api=c.api;g_armada=c.armada;
            g_bink_render=reinterpret_cast<BinkRenderFunction>(c.render);
            g_bink_window=reinterpret_cast<BinkWindowFunction>(c.window_getter);
            g_bink_play=reinterpret_cast<BinkPlayFunction>(c.play);
            g_armada_bink_texture_render=
                reinterpret_cast<ArmadaBinkTextureRenderFunction>(
                    c.armada_texture_render);return 0;
        case 27:return c.api->patch_call(c.window_call,
            reinterpret_cast<void*>(&bink_window_hook),
            kExpectedBinkWindowCall,sizeof(kExpectedBinkWindowCall));
        case 28:return c.api->patch_call(c.play_call,
            reinterpret_cast<void*>(&bink_play_hook),
            kExpectedBinkPlayCall,sizeof(kExpectedBinkPlayCall));
        case 29:return c.api->patch_call(c.call_site,
            reinterpret_cast<void*>(&bink_render_hook),
            kExpectedBinkRenderCall,sizeof(kExpectedBinkRenderCall));
        case 30:return c.api->patch_call(c.armada_gdi_call,
            reinterpret_cast<void*>(&armada_bink_set_dibits_scaled),
            kExpectedArmadaBinkSetDibitsCall,sizeof(kExpectedArmadaBinkSetDibitsCall));
        case 31:return c.api->patch_call(c.armada_texture_call1,
            reinterpret_cast<void*>(&armada_bink_texture_render_scaled),
            kExpectedArmadaBinkTextureRenderCall1,
            sizeof(kExpectedArmadaBinkTextureRenderCall1));
        case 32:return c.api->patch_call(c.armada_texture_call2,
            reinterpret_cast<void*>(&armada_bink_texture_render_scaled),
            kExpectedArmadaBinkTextureRenderCall2,
            sizeof(kExpectedArmadaBinkTextureRenderCall2));
        case 33:c.api->log(kModuleName,
            "Bink renderer call patch failed; movie presentation fix disabled");
            return 0;
        case 34:{
            std::memcpy(c.expected_setup.data(),c.armada_texture_size,
                c.expected_setup.size());
            c.replacement_setup=c.expected_setup;
            std::memcpy(c.replacement_setup.data(),kReplacementArmadaBinkTextureSize,
                sizeof(kReplacementArmadaBinkTextureSize));
            constexpr auto width_offset=kArmadaBinkTextureWidthUvInstructionRva-
                kArmadaBinkTextureSizeRva;
            constexpr auto height_offset=kArmadaBinkTextureHeightUvInstructionRva-
                kArmadaBinkTextureSizeRva;
            static_assert(height_offset+6==kBinkTextureSetupPatchSize,
                "complete final FMUL instruction");
            std::memcpy(c.replacement_setup.data()+width_offset,
                c.replacement_width_uv.data(),c.replacement_width_uv.size());
            std::memcpy(c.replacement_setup.data()+height_offset,
                c.replacement_height_uv.data(),c.replacement_height_uv.size());
            return 0;
        }
        case 35:return c.api->patch_bytes(c.armada_texture_size,
            c.replacement_setup.data(),c.expected_setup.data(),c.expected_setup.size());
        case 36:c.api->log(kModuleName,
            "Bink texture setup patch failed; backing and UV operands unchanged; installed movie callbacks remain native");
            return 0;
        case 37:g_movie_ready=true;return 0;
        case 38:c.api->log(kModuleName,
            "Intro/movie presentation hooks enabled: selected client size applied before "
            "D3D9 device creation and at movie start for both renderers; "
            "movie-only UV operands preserve shared radar/UI constants");return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_bink_initialize(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}
void shutdown_bink_video_scaling() noexcept {
    const auto op=[](void*,std::uint32_t) noexcept -> std::uint64_t {
        g_movie_ready=false;return 0;
    };
    a2fo_rs_stage8_b_bink_shutdown(nullptr,op);
}

} // namespace a2fo::shell_display
