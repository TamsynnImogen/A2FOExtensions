/* Quarter-turn station placement through native synchronized build orders. */
#include "../../sdk/include/a2fo_module_api.h"
#include "rotation.hpp"
#include "footprint.hpp"

#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>

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

const A2FO_ModuleApi* g_api = nullptr;
std::uint8_t* g_armada = nullptr;
std::uint8_t* g_fleet = nullptr;
bool g_ready = false;
PlacementControl g_control;
A2FO_InlineHook g_keyboard{}, g_broadcast{}, g_queue{}, g_set_command{}, g_preview{};
void* g_sending_class = nullptr;
unsigned g_sending_turns = 0;

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

int __fastcall keyboard_hook(void* self, void*, void* device, long* axes, std::uint32_t* buttons) {
    const int result = reinterpret_cast<Keyboard>(g_keyboard.gateway)(self, nullptr, device, axes, buttons);
    if (g_ready && result == 0) update_placement_keys(buttons, foreground() &&
        !reinterpret_cast<void* (__cdecl*)()>(at(kTextInputActiveRva))() &&
        !(GetAsyncKeyState(VK_CONTROL) & 0x8000) && !(GetAsyncKeyState(VK_MENU) & 0x8000),
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
    return result;
}

bool __fastcall broadcast_hook(void* self, void*, std::uint32_t action,
                               const void* position, void* target) {
    const auto previous_class = g_sending_class;
    const unsigned previous_turns = g_sending_turns;
    g_sending_class = nullptr;
    g_sending_turns = 0;
    if (g_ready && action == kBuildCommand) {
        void* selected_class = placement_class();
        g_control.observe(reinterpret_cast<std::uintptr_t>(selected_class));
        g_sending_class = selected_class;
        g_sending_turns = g_control.turns;
    }
    const bool result = reinterpret_cast<Broadcast>(g_broadcast.gateway)(self, nullptr, action, position, target);
    g_sending_class = previous_class;
    g_sending_turns = previous_turns;
    return result;
}

void __fastcall queue_hook(void* self, void*, std::uint32_t command,
                           const void* target_class, const float* position) {
    if (g_ready && command == kBuildCommand && target_class == g_sending_class && target_class)
        command = encode_build(g_sending_turns);
    reinterpret_cast<Queue>(g_queue.gateway)(self, nullptr, command, target_class, position);
}

void __fastcall set_command_hook(void* self, void*, std::uint32_t command,
                                 const void* target_class, const float* position, bool user) {
    const unsigned turns = g_ready ? decode_wire_turns(command) : 0;
    if (turns) command = kBuildCommand;
    reinterpret_cast<SetCommand>(g_set_command.gateway)(self, nullptr, command, target_class, position, user);
    // SetCommand initializes AiCmdInfo.param (+0x10) to zero. Keep the native
    // build command and record only a tagged angle in that saved integer.
    if (turns) {
        auto* parameter = static_cast<std::uint8_t*>(self) + 0x7c + 0x10;
        const std::uint32_t value = rotation_parameter(turns);
        if (accessible(parameter, sizeof(value), true)) std::memcpy(parameter, &value, sizeof(value));
    }
}

void __fastcall preview_hook(void* self, void*, void* camera, bool enabled) {
    void* selected_class = g_ready ? placement_class() : nullptr;
    g_control.observe(reinterpret_cast<std::uintptr_t>(selected_class));
    auto* basis = static_cast<std::uint8_t*>(self) + 0x38;
    std::array<float, 9> saved{};
    const bool rotate = g_ready && enabled && selected_class &&
        selected_class == read<void*>(self, 0x34) && g_control.turns &&
        accessible(basis, sizeof(saved), true);
    if (rotate) {
        std::memcpy(saved.data(), basis, sizeof(saved));
        std::array<float, 12> matrix{};
        set_yaw(matrix.data(), g_control.turns);
        std::memcpy(basis, matrix.data(), sizeof(saved));
    }
    reinterpret_cast<Preview>(g_preview.gateway)(self, nullptr, camera, enabled);
    if (rotate) std::memcpy(basis, saved.data(), sizeof(saved));
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

bool install_hooks() {
    if (!matches(kKeyboardRva, kKeyboardBytes) || !matches(kBroadcastRva, kBroadcastBytes) ||
        !matches(kQueueRva, kQueueBytes) || !matches(kSetCommandRva, kSetCommandBytes) ||
        !matches(kPreviewRva, kPreviewBytes) || !matches(kPositionCallRva, kPositionCallBytes) ||
        !matches(kSetPositionRva, kSetPositionBytes) ||
        !matches(kTextInputActiveRva, kTextInputActiveBytes) || !accessible(at(kActiveModeRva), 4) ||
        !preflight_footprints()) {
        log("Station rotation disabled: native signature mismatch; no hooks installed");
        return false;
    }
    // Leave any partially installed hooks resident as pass-throughs. Enable
    // sending tagged orders only after every receive/render hook has succeeded.
    return install(kSetCommandRva, reinterpret_cast<void*>(&set_command_hook), kSetCommandBytes, g_set_command) &&
        g_api->patch_call(at(kPositionCallRva), reinterpret_cast<void*>(&a2fo_station_position_bridge),
                          kPositionCallBytes, sizeof(kPositionCallBytes)) &&
        install(kPreviewRva, reinterpret_cast<void*>(&preview_hook), kPreviewBytes, g_preview) &&
        install(kQueueRva, reinterpret_cast<void*>(&queue_hook), kQueueBytes, g_queue) &&
        install(kBroadcastRva, reinterpret_cast<void*>(&broadcast_hook), kBroadcastBytes, g_broadcast) &&
        install(kKeyboardRva, reinterpret_cast<void*>(&keyboard_hook), kKeyboardBytes, g_keyboard) &&
        install_footprints();
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
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->install_inline_hook || !api->patch_call) return false;
    g_api = api;
    g_armada = static_cast<std::uint8_t*>(api->armada_module());
    if (!g_armada) return false;
    g_fleet = api->fleetops_module ? static_cast<std::uint8_t*>(api->fleetops_module()) : nullptr;
    g_ready = install_hooks();
    log(g_ready ? "Station rotation initialized: R / Shift+R during placement, 90 degree steps; matching module required on all peers"
                : "Station rotation runtime inactive; DLL retained for safe hook pass-through");
    return true;
}

extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    // Patches live for the process lifetime. Keep their decode path resident.
    g_control = {};
    g_sending_class = nullptr;
    g_sending_turns = 0;
}
