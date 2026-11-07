/* Quarter-turn station placement through native synchronized build orders. */
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif
#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "rotation.hpp"
#include "footprint.hpp"
#include "../../rust/module_runtime_b2_ffi.h"
#include "../../rust/runtime_station_state_ffi.h"

#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <exception>

extern "C" void a2fo_station_position_bridge();
extern "C" void a2fo_station_add_esi();
extern "C" void a2fo_station_add_ebx();
extern "C" void a2fo_station_remove_esi();
extern "C" void a2fo_station_remove_edi();
extern "C" void a2fo_station_remove_ebx();

namespace {
using namespace a2fo::station_rotation;
constexpr char kModuleName[] = "A2FOStationRotation";
constexpr std::uintptr_t kKeyboardRva = 0x0dd570;
constexpr std::uintptr_t kBroadcastRva = 0x0fdca0;
constexpr std::uintptr_t kQueueRva = 0x0d42d0;
constexpr std::uintptr_t kSetCommandRva = 0x0d1cb0;
constexpr std::uintptr_t kPreviewRva = 0x0738a0;
constexpr std::uintptr_t kPositionCallRva = 0x031500;
constexpr std::uintptr_t kSetPositionRva = 0x0adc90;
constexpr std::uintptr_t kActiveModeRva = 0x364e48;
constexpr std::uintptr_t kTextInputActiveRva = 0x25f120;

constexpr std::uint8_t kKeyboardBytes[] = {0x55,0x8b,0xec,0x8b,0x4d,0x10};
constexpr std::uint8_t kBroadcastBytes[] = {0x55,0x8b,0xec,0x6a,0xff};
constexpr std::uint8_t kQueueBytes[] = {0x55,0x8b,0xec,0x56,0x8b,0xf1};
constexpr std::uint8_t kSetCommandBytes[] =
    {0x55,0x8b,0xec,0x64,0xa1,0x00,0x00,0x00,0x00};
constexpr std::uint8_t kPreviewBytes[] = {0x55,0x8b,0xec,0x83,0xec,0x0c};
constexpr std::uint8_t kPositionCallBytes[] = {0xe8,0x8b,0xc7,0x07,0x00};
constexpr std::uint8_t kSetPositionBytes[] = {0x55,0x8b,0xec,0x53,0x56};
constexpr std::uint8_t kTextInputActiveBytes[] = {0xa1,0xbc,0x0f,0x7b,0x00};

A2foRsStationState& checked_station_runtime_state() noexcept {
    if (!a2fo_station_state_layout_matches()) std::terminate();
    return *a2fo_rs_station_state();
}
auto& g_state = checked_station_runtime_state();
auto& g_api = g_state.api;
auto& g_armada = g_state.armada;
auto& g_fleet = g_state.fleet;
auto& g_ready = g_state.ready;
auto& g_control = g_state.control;
auto& g_keyboard = g_state.keyboard;
auto& g_broadcast = g_state.broadcast;
auto& g_queue = g_state.queue;
auto& g_set_command = g_state.set_command;
auto& g_preview = g_state.preview;
auto& g_sending_class = g_state.sending_class;
auto& g_sending_turns = g_state.sending_turns;
auto& g_sending_activation = g_state.sending_activation;
std::uint64_t runtime_activation() noexcept {
#ifdef A2FO_BUILTIN_FEATURE
    return g_ready ? a2fo::builtin_module_activation(A2FO_BUILTIN_FEATURE) : 0;
#else
    return g_ready ? 1 : 0;
#endif
}
bool runtime_enabled() noexcept {
    const auto activation=runtime_activation();
#ifdef A2FO_BUILTIN_FEATURE
    auto& previous=g_state.previous_activation;
    const auto reset=[](void*,std::uint32_t) noexcept ->std::uint64_t {g_control={};return 1;};
    return a2fo_rs_station_enabled(activation,&previous,nullptr,reset)!=0;
#else
    const auto reset=[](void*,std::uint32_t) noexcept ->std::uint64_t {return 1;};
    return a2fo_rs_station_enabled(activation,nullptr,nullptr,reset)!=0;
#endif
}


// Fastcall with a dummy EDX argument matches a native thiscall entry.
using Keyboard = int (__fastcall*)(void*, void*, void*, long*, std::uint32_t*);
using Broadcast = bool (__fastcall*)(void*, void*, std::uint32_t, const void*, void*);
using Queue = void (__fastcall*)(void*, void*, std::uint32_t, const void*, const float*);
using SetCommand = void (__fastcall*)(void*, void*, std::uint32_t, const void*, const float*, bool);
using Preview = void (__fastcall*)(void*, void*, void*, bool);
using SetPosition = void (__fastcall*)(void*, void*, const float*, const void*);

void log(const char* message) { if (g_api && g_api->log) g_api->log(kModuleName, message); }
void* at(std::uintptr_t rva) { return g_armada + rva; }

bool accessible(const void* p, std::size_t size, bool write = false) noexcept {
    auto cursor = reinterpret_cast<std::uintptr_t>(p);
    if (!cursor || size > UINTPTR_MAX - cursor) return false;
    const auto end = cursor + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const DWORD access = info.Protect & 0xff;
        const bool readable = access == PAGE_READONLY || access == PAGE_READWRITE ||
            access == PAGE_WRITECOPY || access == PAGE_EXECUTE_READ ||
            access == PAGE_EXECUTE_READWRITE || access == PAGE_EXECUTE_WRITECOPY;
        const bool writable = access == PAGE_READWRITE || access == PAGE_WRITECOPY ||
            access == PAGE_EXECUTE_READWRITE || access == PAGE_EXECUTE_WRITECOPY;
        if (!readable || (write && !writable)) return false;
        const auto next = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (next <= cursor) return false;
        cursor = next;
    }
    return true;
}

template<class T> T read(const void* p, std::size_t offset) noexcept {
    T result{};
    if (!p) return result;
    const auto* source = static_cast<const std::uint8_t*>(p) + offset;
    if (accessible(source, sizeof(T))) std::memcpy(&result, source, sizeof(T));
    return result;
}

void* placement_class() noexcept {
    void* mode = read<void*>(at(kActiveModeRva), 0);
    return read<std::uint32_t>(mode, 4) == 1 ? read<void*>(mode, 0x0c) : nullptr;
}

bool foreground() noexcept {
    DWORD process = 0;
    HWND window = GetForegroundWindow();
    return window && GetWindowThreadProcessId(window, &process) && process == GetCurrentProcessId();
}

void update_placement_keys(std::uint32_t* buttons, bool allowed, bool reverse) {
    // KeyboardDriver returns 128 scan-code bits, indexed by scan code - 1.
    // R is scan code 0x13. Consume only the returned game binding bit; do not
    // change OS input, the driver's physical state, or the character queue.
    constexpr std::uint32_t r_bit = 1u << (0x13-1);
    if (!accessible(buttons, sizeof(*buttons), true)) return;
    if (g_control.key(reinterpret_cast<std::uintptr_t>(placement_class()), allowed,
                      (*buttons & r_bit) != 0, reverse)) *buttons &= ~r_bit;
}

struct KeyboardContext {void* self;void* device;long* axes;std::uint32_t* buttons;};
std::uint64_t keyboard_operation(void* opaque,std::uint32_t op) noexcept {
    auto& c=*static_cast<KeyboardContext*>(opaque);
    try {switch(op) {
    case 0:return static_cast<std::uint32_t>(reinterpret_cast<Keyboard>(g_keyboard.gateway)(c.self,nullptr,c.device,c.axes,c.buttons));
    case 1:return runtime_enabled();
    case 2:update_placement_keys(c.buttons,foreground() &&
        !reinterpret_cast<void* (__cdecl*)()>(at(kTextInputActiveRva))() &&
        !(GetAsyncKeyState(VK_CONTROL)&0x8000) && !(GetAsyncKeyState(VK_MENU)&0x8000),
        (GetAsyncKeyState(VK_SHIFT)&0x8000)!=0);return 1;
    default:return 0;
    }}catch(...){return 0;}
}
int __fastcall keyboard_hook(void* self, void*, void* device, long* axes, std::uint32_t* buttons) {
    KeyboardContext context{self,device,axes,buttons};
    return a2fo_rs_station_keyboard(&context,keyboard_operation);
}

struct BroadcastContext {
    void* self;std::uint32_t action;const void* position;void* target;
    void* previous_class=nullptr;unsigned previous_turns=0;std::uint64_t previous_activation=0;
};
std::uint64_t broadcast_operation(void* opaque,std::uint32_t op) noexcept {
    auto& c=*static_cast<BroadcastContext*>(opaque);
    try {switch(op) {
    case 0:c.previous_class=g_sending_class;c.previous_turns=g_sending_turns;c.previous_activation=g_sending_activation;
        g_sending_class=nullptr;g_sending_turns=0;g_sending_activation=0;return 1;
    case 1: {
        void* selected_class=placement_class();g_control.observe(reinterpret_cast<std::uintptr_t>(selected_class));
        g_sending_class=selected_class;g_sending_turns=g_control.turns;g_sending_activation=runtime_activation();return 1;
    }
    case 2:return reinterpret_cast<Broadcast>(g_broadcast.gateway)(c.self,nullptr,c.action,c.position,c.target);
    case 3:g_sending_class=c.previous_class;g_sending_turns=c.previous_turns;g_sending_activation=c.previous_activation;return 1;
    case 4:return runtime_enabled();
    default:return 0;
    }}catch(...){return 0;}
}
bool __fastcall broadcast_hook(void* self, void*, std::uint32_t action,
                               const void* position, void* target) {
    BroadcastContext context{self,action,position,target};
    return a2fo_rs_station_broadcast(action,&context,broadcast_operation)!=0;
}

void __fastcall queue_hook(void* self, void*, std::uint32_t command,
                           const void* target_class, const float* position) {
    const bool enabled=runtime_enabled();
    const bool generation=enabled && g_sending_activation==runtime_activation();
    command=a2fo_rs_station_queue(command,enabled,generation,target_class==g_sending_class,target_class!=nullptr,g_sending_turns);
    reinterpret_cast<Queue>(g_queue.gateway)(self, nullptr, command, target_class, position);
}

struct SetContext {void* self;std::uint32_t command;const void* target_class;const float* position;bool user;unsigned turns;};
std::uint64_t set_operation(void* opaque,std::uint32_t op) noexcept {
    auto& c=*static_cast<SetContext*>(opaque);
    try {switch(op) {
    case 0:c.command=kBuildCommand;return 1;
    case 1:reinterpret_cast<SetCommand>(g_set_command.gateway)(c.self,nullptr,c.command,c.target_class,c.position,c.user);return 1;
    case 2: {
        auto* parameter=static_cast<std::uint8_t*>(c.self)+0x7c+0x10;
        const std::uint32_t value=rotation_parameter(c.turns);
        if(accessible(parameter,sizeof(value),true))std::memcpy(parameter,&value,sizeof(value));
        return 1;
    }
    default:return 0;
    }}catch(...){return 0;}
}
void __fastcall set_command_hook(void* self, void*, std::uint32_t command,
                                 const void* target_class, const float* position, bool user) {
    SetContext context{self,command,target_class,position,user,g_ready?decode_wire_turns(command):0};
    a2fo_rs_station_set_command(context.turns,&context,set_operation);
}

struct PreviewContext {void* self;void* camera;bool enabled;void* selected_class=nullptr;
    std::uint8_t* basis=nullptr;std::array<float,9> saved{};};
std::uint64_t preview_operation(void* opaque,std::uint32_t op) noexcept {
    auto& c=*static_cast<PreviewContext*>(opaque);
    try {switch(op) {
    case 0:c.selected_class=runtime_enabled()?placement_class():nullptr;
        g_control.observe(reinterpret_cast<std::uintptr_t>(c.selected_class));
        c.basis=static_cast<std::uint8_t*>(c.self)+0x38;return 1;
    case 1: {
        if(!runtime_enabled())return 0;
        if(!c.enabled)return 0;
        if(!c.selected_class)return 0;
        if(c.selected_class!=read<void*>(c.self,0x34))return 0;
        if(!g_control.turns)return 0;
        return accessible(c.basis,sizeof(c.saved),true)?31:0;
    }
    case 2: {
        std::memcpy(c.saved.data(),c.basis,sizeof(c.saved));
        std::array<float,12> matrix{};set_yaw(matrix.data(),g_control.turns);
        std::memcpy(c.basis,matrix.data(),sizeof(c.saved));return 1;
    }
    case 3:reinterpret_cast<Preview>(g_preview.gateway)(c.self,nullptr,c.camera,c.enabled);return 1;
    case 4:std::memcpy(c.basis,c.saved.data(),sizeof(c.saved));return 1;
    default:return 0;
    }}catch(...){return 0;}
}
void __fastcall preview_hook(void* self, void*, void* camera, bool enabled) {
    PreviewContext context{};context.self=self;context.camera=camera;context.enabled=enabled;
    a2fo_rs_station_preview(&context,preview_operation);
}

bool matches_image(std::uint8_t* base, std::uintptr_t rva, const std::uint8_t* bytes, std::size_t count) {
    if (accessible(base+rva, count) && std::memcmp(base+rva, bytes, count) == 0) return true;
    char message[160];
    std::snprintf(message, sizeof(message), "Station rotation signature mismatch: %s RVA 0x%08lX",
        base == g_armada ? "Armada" : "FleetOpsHook", static_cast<unsigned long>(rva));
    log(message);
    return false;
}
template<std::size_t N> bool matches(std::uintptr_t rva, const std::uint8_t (&bytes)[N]) {
    return matches_image(g_armada, rva, bytes, N);
}
template<std::size_t N> bool install(std::uintptr_t rva, void* replacement,
                                   const std::uint8_t (&bytes)[N], A2FO_InlineHook& hook) {
    return g_api->install_inline_hook(at(rva), replacement, N, bytes, &hook);
}

#include "footprint_runtime.inl"

std::uint64_t init_operation(void* opaque,std::uint32_t op) noexcept {
    const auto* api=static_cast<const A2FO_ModuleApi*>(opaque);
    try {switch(op) {
    case 0:
        g_api=api;g_armada=static_cast<std::uint8_t*>(api->armada_module());
        if(!g_armada)return 0;
        g_fleet=api->fleetops_module?static_cast<std::uint8_t*>(api->fleetops_module()):nullptr;
        return 1;
    case 1:return a2fo::supported_armada::uses_native_station_rotation(
        a2fo::supported_armada::identify(reinterpret_cast<HMODULE>(g_armada)));
    case 2:
    if (!matches(kKeyboardRva, kKeyboardBytes) || !matches(kBroadcastRva, kBroadcastBytes) ||
        !matches(kQueueRva, kQueueBytes) || !matches(kSetCommandRva, kSetCommandBytes) ||
        !matches(kPreviewRva, kPreviewBytes) || !matches(kPositionCallRva, kPositionCallBytes) ||
        !matches(kSetPositionRva, kSetPositionBytes) ||
        !matches(kTextInputActiveRva, kTextInputActiveBytes) || !accessible(at(kActiveModeRva), 4) ||
        !preflight_footprints()) {
        log("Station rotation disabled: native signature mismatch; no hooks installed");
        return 0;
    }
    return 1;
    case 3:return install(kSetCommandRva,reinterpret_cast<void*>(&set_command_hook),kSetCommandBytes,g_set_command);
    case 4:return g_api->patch_call(at(kPositionCallRva),reinterpret_cast<void*>(&a2fo_station_position_bridge),kPositionCallBytes,sizeof(kPositionCallBytes));
    case 5:return install(kPreviewRva,reinterpret_cast<void*>(&preview_hook),kPreviewBytes,g_preview);
    case 6:return install(kQueueRva,reinterpret_cast<void*>(&queue_hook),kQueueBytes,g_queue);
    case 7:return install(kBroadcastRva,reinterpret_cast<void*>(&broadcast_hook),kBroadcastBytes,g_broadcast);
    case 8:return install(kKeyboardRva,reinterpret_cast<void*>(&keyboard_hook),kKeyboardBytes,g_keyboard);
    case 9:return install_footprints();
    case 10:g_ready=true;return 1;
    case 11:g_ready=false;log("Using Roots native station rotation; extension rotation hooks not installed");return 1;
    case 13:log("Station rotation initialized: R / Shift+R during placement, 90 degree steps; matching module required on all peers");return 1;
    case 14:log("Station rotation runtime inactive; DLL retained for safe hook pass-through");return 1;
    case 15:g_ready=false;return 1;
    default:return 0;
    }}catch(...){return 0;}
}
} // namespace

extern "C" void __cdecl a2fo_station_add_footprint(
        void* cls, const float* position, const float* bounds, bool flag, const void* object) {
    path_footprint(kAddFootprintRva, cls, position, bounds, flag, object);
}
extern "C" void __cdecl a2fo_station_remove_footprint(
        void* cls, const float* position, const float* bounds, bool flag, const void* object) {
    path_footprint(kRemoveFootprintRva, cls, position, bounds, flag, object);
}

extern "C" void __cdecl a2fo_station_apply_position(
        void* build_interface, const float* position, const void* target_class, const void* command) {
    reinterpret_cast<SetPosition>(at(kSetPositionRva))(build_interface, nullptr, position, target_class);
    if (!g_ready || read<std::uint32_t>(command, 4) != kBuildCommand) return;
    auto* basis = static_cast<std::uint8_t*>(build_interface) + 4;
    if (!accessible(basis, 9 * sizeof(float), true)) return;
    std::array<float, 12> matrix{};
    set_yaw(matrix.data(), parameter_turns(read<std::uint32_t>(command, 0x10)));
    std::memcpy(basis, matrix.data(), 9 * sizeof(float));
}

extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    const bool valid=api && api->struct_size>=A2FO_MODULE_API_V4_BASE_SIZE &&
        api->api_version==A2FO_MODULE_API_VERSION && api->log && api->armada_module &&
        api->install_inline_hook && api->patch_call;
    return a2fo_rs_station_init(valid,const_cast<A2FO_ModuleApi*>(api),init_operation)!=0;
}

extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    // Patches live for the process lifetime. Keep their decode path resident.
    const auto clear=[](void*,std::uint32_t op) noexcept ->std::uint64_t {
        switch(op){case 0:g_control={};return 1;case 1:g_sending_class=nullptr;return 1;
            case 2:g_sending_turns=0;return 1;default:return 0;}
    };
    a2fo_rs_station_shutdown(nullptr,clear);
}
