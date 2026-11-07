// Headless smoke for the real DLL. Game images are mapped without entry points;
// all hook gateways and the native mode lookup are synthetic test functions.
#include "../sdk/include/a2fo_module_api.h"
#include "../modules/A2FOShellDisplay/policy.hpp"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
void* test_register_form{};
void* test_register_window{};
int test_register_left{}, test_register_top{}, test_register_width{}, test_register_height{};
unsigned test_register_calls{};
void* test_bounds_control{};
unsigned test_bounds_calls{};
unsigned test_stack_before{}, test_stack_after{};
unsigned test_ebx_after{}, test_esi_after{}, test_edi_after{};
}
extern "C" __attribute__((naked)) void fake_register_gateway() {
    __asm__ volatile(
        "movl %eax, _test_register_form\n"
        "movl %edx, _test_register_window\n"
        "movl %ecx, _test_register_left\n"
        "movl 4(%esp), %eax\n movl %eax, _test_register_height\n"
        "movl 8(%esp), %eax\n movl %eax, _test_register_width\n"
        "movl 12(%esp), %eax\n movl %eax, _test_register_top\n"
        "incl _test_register_calls\n"
        "xorl %ebx, %ebx\n xorl %esi, %esi\n xorl %edi, %edi\n"
        "ret $12\n");
}
extern "C" __attribute__((naked)) void test_call_register(
    void*, void*, HWND, int, int, int, int) {
    __asm__ volatile(
        "pushl %ebp\n movl %esp, %ebp\n"
        "pushl %ebx\n pushl %esi\n pushl %edi\n"
        "movl $0x13572468, %ebx\n movl $0x24681357, %esi\n movl $0x35792468, %edi\n"
        "movl %esp, _test_stack_before\n"
        "pushl 24(%ebp)\n pushl 28(%ebp)\n pushl 32(%ebp)\n"
        "movl 12(%ebp), %eax\n movl 16(%ebp), %edx\n movl 20(%ebp), %ecx\n"
        "call *8(%ebp)\n"
        "movl %esp, _test_stack_after\n"
        "movl %ebx, _test_ebx_after\n movl %esi, _test_esi_after\n movl %edi, _test_edi_after\n"
        "popl %edi\n popl %esi\n popl %ebx\n leave\n ret\n");
}
extern "C" __attribute__((naked)) void fake_bounds_gateway() {
    __asm__ volatile("movl %eax, _test_bounds_control\n incl _test_bounds_calls\n"
        "xorl %ebx, %ebx\n xorl %esi, %esi\n xorl %edi, %edi\n ret\n");
}
extern "C" __attribute__((naked)) void test_call_bounds(void*, void*) {
    __asm__ volatile(
        "pushl %ebp\n movl %esp, %ebp\n pushl %ebx\n pushl %esi\n pushl %edi\n"
        "movl $0x13572468, %ebx\n movl $0x24681357, %esi\n movl $0x35792468, %edi\n"
        "movl %esp, _test_stack_before\n movl 12(%ebp), %eax\n call *8(%ebp)\n"
        "movl %esp, _test_stack_after\n movl %ebx, _test_ebx_after\n"
        "movl %esi, _test_esi_after\n movl %edi, _test_edi_after\n"
        "popl %edi\n popl %esi\n popl %ebx\n leave\n ret\n");
}

namespace {
constexpr std::uintptr_t kActivate = 0x1d3520, kResize = 0x1d0310;
constexpr std::uintptr_t kLookup = 0x22c4e0, kPrimaryCall = 0x22c70b;
constexpr std::uintptr_t kOriginalPrimary = 0x22c3b0;
constexpr std::uintptr_t kNativeFind = 0x226bf0, kProfileSlot = 0x212e08;
constexpr std::uintptr_t kShellDepth = 0x22c4d5, kFoDepthPayload = 0x1cd680;
constexpr std::uintptr_t kRegister = 0x173a64;
constexpr std::uintptr_t kUpdateBounds = 0xcdbc8;
constexpr std::uintptr_t kCheckboxCursorCall = 0x166323;
HMODULE g_armada{}, g_fo{};
void* g_activate_replacement{};
void* g_resize_replacement{};
void* g_lookup_replacement{};
void* g_register_replacement{};
void* g_bounds_replacement{};
void* g_checkbox_replacement{};
unsigned g_hooks{}, g_calls{}, g_bad_bytes{}, g_native_calls{}, g_activate_calls{}, g_resize_calls{};
unsigned g_byte_patches{}, g_fail_byte_patch{};
std::vector<void*> g_canvas_slots;
bool g_fail_resize{};
bool g_fail_checkbox{};
void* g_native_result = reinterpret_cast<void*>(0x12345678);
void* g_lookup_result{};
void* g_expected_device{};
int g_width{}, g_height{}, g_depth{}, g_display{}, g_mode{};
int g_native_width{}, g_native_height{};
bool g_force{}, g_fullscreen{};
int g_activate_result{};
bool g_fake_gateway_clamp{};
int g_resize_hwnd_write{};
int g_forwarded_width{}, g_forwarded_height{};
unsigned g_adapter{};
HWND* g_window{};
HWND* g_test_window{};
void* g_height_form{};
void* g_width_form{};
int g_set_height{}, g_set_width{};
unsigned g_height_sets{}, g_width_sets{};
void* g_left_form{};
void* g_top_form{};
int g_set_left{}, g_set_top{};
unsigned g_left_sets{}, g_top_sets{};
unsigned g_monitor_calls{};
int g_monitor_api{};
void* g_monitor_object{};
unsigned g_monitor_adapter{};
HMONITOR g_monitor_result{};
RECT g_monitor_rect{0, 0, 1920, 1080};
RECT g_work_rect{0, 0, 1920, 1040};
RECT g_outer_rect{0, 0, 1280, 800};
RECT g_client_rect{0, 0, 1280, 800};
unsigned g_info_calls{}, g_outer_calls{}, g_client_calls{};
std::string g_log;

template<class T> T function(void* value) { return reinterpret_cast<T>(value); }
std::uint8_t* at(HMODULE image, std::uintptr_t rva) {
    return reinterpret_cast<std::uint8_t*>(image) + rva;
}
bool writable(void* address, std::size_t size) {
    DWORD old{};
    return VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old) != 0;
}
template<class T> bool put(void* address, T value) {
    if (!writable(address, sizeof(value))) return false;
    std::memcpy(address, &value, sizeof(value));
    FlushInstructionCache(GetCurrentProcess(), address, sizeof(value));
    return true;
}
bool primary_call(void* target) {
    auto* call = at(g_armada, kPrimaryCall);
    const std::int32_t displacement = static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(target) -
        reinterpret_cast<std::uintptr_t>(call + 5));
    return put(call, std::uint8_t{0xe8}) && put(call + 1, displacement);
}
void A2FO_CALL log_message(const char*, const char* message) {
    if (message) { g_log += message; g_log += '\n'; }
}
void* A2FO_CALL armada_module() { return g_armada; }
void* A2FO_CALL fleetops_module() { return g_fo; }

using Lookup = void* (__fastcall*)(void*, void*, int, int, int);
using Activate = int (__fastcall*)(void*, void*, int, int, bool);
using Resize = void (__cdecl*)(HWND*, int, int, bool, unsigned);

void* __fastcall fake_native_find(void* device, void*, int width, int height, int depth) {
    ++g_native_calls;
    g_expected_device = device;
    g_native_width = width; g_native_height = height; g_depth = depth;
    return g_native_result;
}
void __cdecl fake_resize(HWND* window, int width, int height, bool fullscreen, unsigned adapter) {
    ++g_resize_calls;
    g_window = window; g_forwarded_width = width; g_forwarded_height = height;
    g_width = g_fake_gateway_clamp ? 1200 : width;
    g_height = g_fake_gateway_clamp ? 900 : height;
    g_fullscreen = fullscreen; g_adapter = adapter;
    if (g_resize_hwnd_write && window)
        *window = g_resize_hwnd_write == 1 ? nullptr : reinterpret_cast<HWND>(1);
}
HMONITOR __stdcall fake_monitor8(void* object, unsigned adapter) {
    ++g_monitor_calls; g_monitor_api = 8;
    g_monitor_object = object; g_monitor_adapter = adapter;
    return g_monitor_result;
}
HMONITOR __stdcall fake_monitor9(void* object, unsigned adapter) {
    ++g_monitor_calls; g_monitor_api = 9;
    g_monitor_object = object; g_monitor_adapter = adapter;
    return g_monitor_result;
}
BOOL WINAPI fake_monitor_info(HMONITOR handle, LPMONITORINFO info) {
    ++g_info_calls;
    if (!handle || handle != g_monitor_result || !info || info->cbSize != sizeof(MONITORINFO)) return FALSE;
    info->rcMonitor = g_monitor_rect;
    info->rcWork = g_work_rect;
    return TRUE;
}
BOOL WINAPI fake_window_rect(HWND window, LPRECT rect) {
    ++g_outer_calls;
    if (window != reinterpret_cast<HWND>(1) || !rect) return FALSE;
    *rect = g_outer_rect;
    return TRUE;
}
BOOL WINAPI fake_client_rect(HWND window, LPRECT rect) {
    ++g_client_calls;
    if (window != reinterpret_cast<HWND>(1) || !rect) return FALSE;
    *rect = g_client_rect;
    return TRUE;
}
int __fastcall fake_activate(void* engine, void*, int display, int mode, bool force) {
    ++g_activate_calls;
    g_display = display; g_mode = mode; g_force = force;
    auto* config = *reinterpret_cast<std::uint8_t**>(static_cast<std::uint8_t*>(engine) + 0x104);
    void* device = *reinterpret_cast<void**>(config + 0x1c);
    g_lookup_result = function<Lookup>(g_lookup_replacement)(device, nullptr, 800, 600, 16);
    function<Resize>(g_resize_replacement)(g_test_window, 800, 600, false, 2);
    return g_activate_result;
}
bool A2FO_CALL install_inline(void* target, void* replacement, std::size_t length,
                              const std::uint8_t* expected, A2FO_InlineHook* hook) {
    ++g_hooks;
    if (!expected || !hook || std::memcmp(target, expected, length) != 0) {
        ++g_bad_bytes; return false;
    }
    if (target == at(g_fo, kActivate) && length == 7) {
        g_activate_replacement = replacement;
        hook->gateway = reinterpret_cast<void*>(&fake_activate);
    } else if (target == at(g_fo, kResize) && length == 6) {
        if (g_fail_resize) return false;
        g_resize_replacement = replacement;
        hook->gateway = reinterpret_cast<void*>(&fake_resize);
    } else if (target == at(g_fo, kRegister) && length == 6) {
        g_register_replacement = replacement;
        hook->gateway = reinterpret_cast<void*>(&fake_register_gateway);
    } else if (target == at(g_fo, kUpdateBounds) && length == 6) {
        g_bounds_replacement = replacement;
        hook->gateway = reinterpret_cast<void*>(&fake_bounds_gateway);
    } else return false;
    hook->target = target; hook->length = length;
    return true;
}
bool A2FO_CALL patch_call(void* target, void* replacement,
                          const std::uint8_t* expected, std::size_t length) {
    ++g_calls;
    if (length != 5 || !expected || std::memcmp(target, expected, length) != 0) {
        ++g_bad_bytes; return false;
    }
    if (target == at(g_armada, kLookup)) g_lookup_replacement = replacement;
    else if (target == at(g_fo, kCheckboxCursorCall)) {
        if (g_fail_checkbox) return false;
        g_checkbox_replacement = replacement;
    } else { ++g_bad_bytes; return false; }
    return true;
}
bool A2FO_CALL patch_bytes(void* target, const std::uint8_t* replacement,
                           const std::uint8_t* expected, std::size_t length) {
    ++g_byte_patches;
    bool known = false;
    for (void* slot : g_canvas_slots) if (slot == target) known = true;
    if (!known || length != sizeof(void*) || !replacement || !expected ||
        std::memcmp(target, expected, length) != 0) {
        ++g_bad_bytes; return false;
    }
    if (g_byte_patches == g_fail_byte_patch) return false;
    void* value{};
    std::memcpy(&value, replacement, sizeof(value));
    return put(target, value);
}
void reset() {
    g_activate_replacement = g_resize_replacement = g_lookup_replacement = g_register_replacement = nullptr;
    g_bounds_replacement = nullptr;
    g_checkbox_replacement = nullptr; g_fail_checkbox = false;
    g_hooks = g_calls = g_bad_bytes = g_native_calls = g_activate_calls = g_resize_calls = 0;
    g_fail_resize = false; g_lookup_result = nullptr; g_log.clear();
    g_activate_result = 0;
    g_test_window = nullptr;
    g_fake_gateway_clamp = false;
    g_resize_hwnd_write = 0;
    g_monitor_calls = 0;
    g_monitor_result = nullptr;
    g_info_calls = g_outer_calls = g_client_calls = 0;
    g_height_sets = g_width_sets = g_left_sets = g_top_sets = 0;
    g_byte_patches = g_fail_byte_patch = 0;
    test_register_calls = 0;
    test_bounds_calls = 0; test_bounds_control = nullptr;
}
bool check(bool condition, const char* label) {
    if (!condition) std::fprintf(stderr, "FAIL: %s (hooks=%u calls=%u native=%u)\n%s",
                                 label, g_hooks, g_calls, g_native_calls, g_log.c_str());
    return condition;
}
struct Module {
    HMODULE image{};
    A2FO_ModuleInitFn init{};
    A2FO_ModuleShutdownFn shutdown{};
    explicit Module(const char* path) {
        image = LoadLibraryA(path);
        if (image) {
            init = function<A2FO_ModuleInitFn>(reinterpret_cast<void*>(GetProcAddress(image, "A2FO_ModuleInit")));
            shutdown = function<A2FO_ModuleShutdownFn>(reinterpret_cast<void*>(GetProcAddress(image, "A2FO_ModuleShutdown")));
        }
    }
    ~Module() { if (shutdown) shutdown(); if (image) FreeLibrary(image); }
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;
};
// Patch only this loaded test module's USER32 imports, then restore them
// before Module unloads it. Game-image IATs belong to IatFixture below.
class ModuleGeometryIat {
    struct Slot { void* address{}; void* saved{}; };
    Slot info_{}, outer_{}, client_{};
    bool okay_{};
    static bool patch(Slot& slot, void* replacement) {
        if (!slot.address) return false;
        slot.saved = *reinterpret_cast<void**>(slot.address);
        return put(slot.address, replacement);
    }
public:
    explicit ModuleGeometryIat(HMODULE image) {
        if (!image) return;
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(at(image, dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE) return;
        const DWORD size = nt->OptionalHeader.SizeOfImage;
        const DWORD imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        if (!imports || imports >= size) return;
        const auto* descriptors = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(at(image, imports));
        for (const auto* descriptor = descriptors; descriptor->Name; ++descriptor) {
            if (descriptor->Name >= size || !descriptor->OriginalFirstThunk ||
                descriptor->OriginalFirstThunk >= size || !descriptor->FirstThunk ||
                descriptor->FirstThunk >= size) return;
            const char* library = reinterpret_cast<const char*>(at(image, descriptor->Name));
            if (lstrcmpiA(library, "USER32.dll") != 0) continue;
            const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(at(image, descriptor->OriginalFirstThunk));
            auto* addresses = reinterpret_cast<IMAGE_THUNK_DATA*>(at(image, descriptor->FirstThunk));
            for (; names->u1.AddressOfData; ++names, ++addresses) {
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
                if (names->u1.AddressOfData >= size) return;
                const auto* entry = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(at(image, names->u1.AddressOfData));
                const char* name = reinterpret_cast<const char*>(entry->Name);
                Slot* found = std::strcmp(name, "GetMonitorInfoA") == 0 ? &info_ :
                    std::strcmp(name, "GetWindowRect") == 0 ? &outer_ :
                    std::strcmp(name, "GetClientRect") == 0 ? &client_ : nullptr;
                if (found) found->address = &addresses->u1.Function;
            }
        }
        if (!info_.address || !outer_.address || !client_.address) return;
        okay_ = patch(info_, reinterpret_cast<void*>(&fake_monitor_info)) &&
                patch(outer_, reinterpret_cast<void*>(&fake_window_rect)) &&
                patch(client_, reinterpret_cast<void*>(&fake_client_rect));
    }
    ~ModuleGeometryIat() {
        for (const Slot& slot : {client_, outer_, info_})
            if (slot.address && slot.saved) put(slot.address, slot.saved);
    }
    ModuleGeometryIat(const ModuleGeometryIat&) = delete;
    ModuleGeometryIat& operator=(const ModuleGeometryIat&) = delete;
    bool okay() const { return okay_; }
};
A2FO_ModuleApi api() {
    A2FO_ModuleApi result{};
    result.struct_size = sizeof(result); result.api_version = A2FO_MODULE_API_VERSION;
    result.log = log_message; result.armada_module = armada_module;
    result.fleetops_module = fleetops_module;
    result.install_inline_hook = install_inline; result.patch_call = patch_call;
    return result;
}
bool jump_to_native_fake() {
    auto* target = at(g_armada, kNativeFind);
    const std::int32_t displacement = static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(&fake_native_find) -
        reinterpret_cast<std::uintptr_t>(target + 5));
    return put(target, std::uint8_t{0xe9}) && put(target + 1, displacement);
}
bool delphi_sink(std::uintptr_t rva, void** form, int* value, unsigned* calls) {
    // mov [form],eax; mov [value],edx; inc dword [calls]; ret.
    // The mapped PE is private copy-on-write memory, never changed on disk.
    std::uint8_t code[18]{0xa3, 0,0,0,0, 0x89,0x15, 0,0,0,0,
                          0xff,0x05, 0,0,0,0, 0xc3};
    const auto form_address = reinterpret_cast<std::uint32_t>(form);
    const auto value_address = reinterpret_cast<std::uint32_t>(value);
    const auto calls_address = reinterpret_cast<std::uint32_t>(calls);
    std::memcpy(code + 1, &form_address, 4);
    std::memcpy(code + 7, &value_address, 4);
    std::memcpy(code + 13, &calls_address, 4);
    auto* target = at(g_fo, rva);
    if (!writable(target, sizeof(code))) return false;
    std::memcpy(target, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(code));
    return true;
}
bool d3d9_stub(bool enabled) {
    const std::uint8_t code[6]{0xb8, static_cast<std::uint8_t>(enabled), 0, 0, 0, 0xc3};
    auto* target = at(g_fo, 0x1df754);
    if (!writable(target, sizeof(code))) return false;
    std::memcpy(target, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(code));
    return true;
}
struct IatSlot { void* address; void* saved; const char* name; };
class IatFixture {
    std::vector<IatSlot> slots_;
    bool okay_ = true;
    static bool canvas_name(const char* name) {
        constexpr const char* names[]{
            "GetClientRect", "MoveWindow", "SetWindowPos", "ScreenToClient",
            "ClientToScreen", "MapWindowPoints", "BeginPaint", "EndPaint",
            "GetDC", "GetDCEx", "ReleaseDC", "ScrollWindowEx", "SendMessageA",
            "DefWindowProcA", "CallWindowProcA", "InvalidateRect", "ValidateRect",
            "InvalidateRgn", "RedrawWindow"};
        for (const char* candidate : names)
            if (std::strcmp(name, candidate) == 0) return true;
        return false;
    }
    void resolve(HMODULE image, HMODULE user32) {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(at(image, dos->e_lfanew));
        const DWORD size = nt->OptionalHeader.SizeOfImage;
        const DWORD imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || nt->Signature != IMAGE_NT_SIGNATURE ||
            !imports || imports >= size) { okay_ = false; return; }
        const auto* descriptor = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(at(image, imports));
        for (; descriptor->Name; ++descriptor) {
            if (!descriptor->OriginalFirstThunk || !descriptor->FirstThunk ||
                descriptor->OriginalFirstThunk >= size || descriptor->FirstThunk >= size) {
                okay_ = false; return;
            }
            const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(at(image, descriptor->OriginalFirstThunk));
            auto* addresses = reinterpret_cast<IMAGE_THUNK_DATA*>(at(image, descriptor->FirstThunk));
            for (; names->u1.AddressOfData; ++names, ++addresses) {
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
                if (names->u1.AddressOfData >= size) { okay_ = false; return; }
                const auto* entry = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(at(image, names->u1.AddressOfData));
                const char* name = reinterpret_cast<const char*>(entry->Name);
                if (!canvas_name(name)) continue;
                auto* slot = &addresses->u1.Function;
                const auto original = reinterpret_cast<void*>(*slot);
                const auto resolved = reinterpret_cast<void*>(GetProcAddress(user32, name));
                if (!resolved || !put(slot, resolved)) { okay_ = false; return; }
                slots_.push_back({slot, original, name});
                g_canvas_slots.push_back(slot);
            }
        }
    }
public:
    IatFixture() {
        g_canvas_slots.clear();
        HMODULE user32 = GetModuleHandleA("user32.dll");
        if (!user32) { okay_ = false; return; }
        resolve(g_armada, user32);
        if (okay_) resolve(g_fo, user32);
    }
    ~IatFixture() {
        for (const auto& slot : slots_) put(slot.address, slot.saved);
        g_canvas_slots.clear();
    }
    IatFixture(const IatFixture&) = delete;
    IatFixture& operator=(const IatFixture&) = delete;
    bool okay() const { return okay_ && !slots_.empty(); }
    std::size_t size() const { return slots_.size(); }
    void* slot(const char* name) const {
        for (const auto& entry : slots_)
            if (std::strcmp(entry.name, name) == 0) return entry.address;
        return nullptr;
    }
    unsigned patch_index(const char* name) const {
        for (unsigned i = 0; i < slots_.size(); ++i)
            if (std::strcmp(slots_[i].name, name) == 0) return i + 1;
        return 0;
    }
};
struct Fixture {
    std::uint8_t engine[0x108]{};
    std::uint8_t config[0x28]{};
    std::uint8_t profile[0x85]{};
    void* profile_global{};
    void* profile_variable{};
    a2fo::shell_display::Mode selected{1920, 1080, 32};
    void* device = reinterpret_cast<void*>(0x13572468);
    Fixture() {
        std::memcpy(engine + 0x104, &config_pointer, sizeof(config_pointer));
        std::memcpy(config + 0x1c, &device, sizeof(device));
        void* selected_pointer = &selected;
        std::memcpy(config + 0x24, &selected_pointer, sizeof(selected_pointer));
        profile_global = profile;
        profile_variable = &profile_global;
        put(at(g_fo, kProfileSlot), &profile_variable);
    }
    void* config_pointer = config;
};
bool invoke(Fixture& fixture, int display, int mode, bool borderless, bool editor,
            bool selected, bool resized_fullscreen, bool incoming_force,
            bool expected_force, const char* label) {
    fixture.config[1] = static_cast<std::uint8_t>(borderless);
    fixture.profile[0x84] = static_cast<std::uint8_t>(editor);
    const unsigned prior_native = g_native_calls;
    const unsigned prior_resize = g_resize_calls;
    const int expected_width = selected ? fixture.selected.width : 800;
    const int expected_height = selected ? fixture.selected.height : 600;
    const int result = function<Activate>(g_activate_replacement)(fixture.engine, nullptr, display, mode, incoming_force);
    return check(result == g_activate_result && g_display == display && g_mode == mode &&
                 g_force == expected_force &&
                 g_lookup_result == (selected ? static_cast<void*>(&fixture.selected) : g_native_result) &&
                 g_native_calls == prior_native + (selected ? 0u : 1u) &&
                 (selected || g_expected_device == fixture.device) &&
                 (selected || (g_native_width == 800 && g_native_height == 600 && g_depth == 16)) &&
                 g_resize_calls == prior_resize + 1 && g_window == nullptr &&
                 g_forwarded_width == expected_width && g_forwarded_height == expected_height &&
                 g_width == expected_width && g_height == expected_height && g_adapter == 2 &&
                 g_fullscreen == resized_fullscreen, label);
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "Usage: shell_display_native_smoke.exe MODULE_DLL ARMADA_EXE FLEETOPS_DLL\n");
        return 2;
    }
    g_armada = LoadLibraryExA(argv[2], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    g_fo = LoadLibraryExA(argv[3], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!g_armada || !g_fo) {
        std::fprintf(stderr, "Cannot map game images as data (%lu)\n", static_cast<unsigned long>(GetLastError()));
        return 3;
    }
    // This fixture owns shell/canvas gateways only. Model an unavailable
    // movie call site so adding optional movie hooks cannot consume those
    // mocks; shell_intro_test validates the original movie signatures/hooks.
    if (!put(at(g_fo, 0x1d3d3d), std::uint8_t{0x90})) return 4;
    if (!check(*at(g_armada, kShellDepth) == 16,
               "on-disk shell depth starts at native 16")) return 1;
    if (!primary_call(at(g_fo, kActivate)) || !jump_to_native_fake()) return 4;
    const auto config = api();
    {
        reset(); Module module(argv[1]);
        if (!check(module.init && module.shutdown && module.init(&config) &&
                   g_hooks == 2 && g_calls == 1 && g_bad_bytes == 0 &&
                   g_activate_replacement && g_resize_replacement && g_lookup_replacement,
                   "supported preflight and three hooks")) return 1;
        Fixture fixture;
        if (!invoke(fixture, 0, 3, false, false, true, false, false, true,
                    "first primary shell forces recreation") ||
            !invoke(fixture, 0, 3, false, false, true, false, false, false,
                    "unchanged shell does not force again") ||
            !invoke(fixture, 0, 3, true, false, true, true, false, true,
                    "borderless toggle forces recreation") ||
            !invoke(fixture, 0, 3, true, false, true, true, false, false,
                    "unchanged borderless entry") ||
            !invoke(fixture, 1, 3, true, false, false, false, false, false,
                    "secondary display") ||
            !invoke(fixture, 0, 2, true, false, false, false, false, false,
                    "other mode") ||
            !invoke(fixture, 0, 3, true, true, false, false, true, true,
                    "editor keeps caller force") ) return 1;
        fixture.profile[0x84] = 0;
        fixture.selected.width = 1600;
        if (!invoke(fixture, 0, 3, true, false, true, true, false, true,
                    "changed selected mode forces recreation")) return 1;
        fixture.selected.height = 900;
        g_activate_result = 7;
        if (!invoke(fixture, 0, 3, true, false, true, true, false, true,
                    "failed activation forwards result and clears cache")) return 1;
        g_activate_result = 0;
        if (!invoke(fixture, 0, 3, true, false, true, true, false, true,
                    "retry after failure forces recreation")) return 1;
        fixture.selected.width = 10;
        if (!invoke(fixture, 0, 3, true, false, false, false, false, false,
                    "invalid selected mode")) return 1;
        function<Resize>(g_resize_replacement)(nullptr, 1024, 768, true, 4);
        if (!check(g_fullscreen && g_width == 1024 && g_height == 768 && g_adapter == 4,
                   "scope restored after activation")) return 1;
        function<Resize>(g_resize_replacement)(reinterpret_cast<HWND*>(1), 640, 480, false, 3);
        if (!check(g_window == reinterpret_cast<HWND*>(1) && g_width == 640,
                   "unscoped resize never reads caller window storage")) return 1;
        // Synthetic form setters capture Delphi EAX/EDX after a fake gateway.
        // Geometry calls are redirected through this module's own USER32 IAT;
        // the fake HWND never identifies a real window.
        int form{};
        void* form_pointer = &form;
        void* d3d_table[16]{};
        d3d_table[0x38 / sizeof(void*)] = reinterpret_cast<void*>(&fake_monitor8);
        d3d_table[0x3c / sizeof(void*)] = reinterpret_cast<void*>(&fake_monitor9);
        void* d3d_object[1]{d3d_table};
        void* d3d_global = d3d_object;
        void* d3d_pointer = &d3d_global;
        if (!put(at(g_fo, 0x24485c), form_pointer) ||
            !put(at(g_fo, 0x2125cc), &d3d_pointer) ||
            !put(at(g_fo, 0x213108), &d3d_pointer) ||
            !d3d9_stub(false)) return 4;
        if (!delphi_sink(0xad390, &g_height_form, &g_set_height, &g_height_sets) ||
            !delphi_sink(0xad35c, &g_width_form, &g_set_width, &g_width_sets) ||
            !delphi_sink(0xb2708, &g_left_form, &g_set_left, &g_left_sets) ||
            !delphi_sink(0xb2744, &g_top_form, &g_set_top, &g_top_sets)) return 4;
        ModuleGeometryIat geometry(module.image);
        if (!check(geometry.okay(), "module USER32 geometry IAT fixture")) return 1;
        HWND fake_window = reinterpret_cast<HWND>(1);
        g_test_window = &fake_window;
        g_fake_gateway_clamp = true;
        g_monitor_result = reinterpret_cast<HMONITOR>(2);
        fixture.selected = {1280, 800, 32};
        fixture.config[1] = 1;
        g_monitor_rect = {0, 0, 1920, 1080};
        g_work_rect = {0, 0, 1920, 1040};
        const int sized = function<Activate>(g_activate_replacement)(fixture.engine, nullptr, 0, 3, false);
        if (!check(sized == 0 && g_height_sets == 1 && g_width_sets == 1 &&
                   g_left_sets == 1 && g_top_sets == 1 &&
                   g_height_form == &form && g_width_form == &form &&
                   g_left_form == &form && g_top_form == &form &&
                   g_forwarded_width == 1280 && g_forwarded_height == 800 &&
                   g_width == 1200 && g_height == 900 &&
                   g_set_height == 1080 && g_set_width == 1920 &&
                   g_set_left == 0 && g_set_top == 0 &&
                   g_monitor_calls == 1 && g_monitor_api == 8 &&
                   g_monitor_object == d3d_object && g_monitor_adapter == 2 &&
                   g_info_calls == 1 && g_client_calls == 1,
                   "borderless uses full 1920x1080 monitor but forwards selected 1280x800")) return 1;
        if (!d3d9_stub(true)) return 4;
        g_monitor_rect = {-1920, -120, 0, 960};
        g_work_rect = {-1920, -120, 0, 920};
        const int sized9 = function<Activate>(g_activate_replacement)(fixture.engine, nullptr, 0, 3, false);
        if (!check(sized9 == 0 && g_height_sets == 2 && g_width_sets == 2 &&
                   g_left_sets == 2 && g_top_sets == 2 &&
                   g_set_height == 1080 && g_set_width == 1920 &&
                   g_set_left == -1920 && g_set_top == -120 &&
                   g_forwarded_width == 1280 && g_forwarded_height == 800 &&
                   g_monitor_calls == 2 && g_monitor_api == 9 &&
                   g_monitor_object == d3d_object && g_monitor_adapter == 2,
                   "D3D9 negative monitor origin")) return 1;
        fixture.selected = {2560, 1440, 32};
        const int oversized = function<Activate>(g_activate_replacement)(fixture.engine, nullptr, 0, 3, false);
        if (!check(oversized == 0 && g_height_sets == 3 && g_width_sets == 3 &&
                   g_left_sets == 3 && g_top_sets == 3 &&
                   g_set_height == 1080 && g_set_width == 1920 &&
                   g_set_left == -1920 && g_set_top == -120 &&
                   g_forwarded_width == 2560 && g_forwarded_height == 1440,
                   "oversized selected render mode keeps monitor sized borderless VCL form")) return 1;
        fixture.selected = {1280, 800, 32};
        fixture.config[1] = 0;
        g_outer_rect = {0, 0, 1280, 800};
        const int framed = function<Activate>(g_activate_replacement)(fixture.engine, nullptr, 0, 3, false);
        if (!check(framed == 0 && g_height_sets == 4 && g_width_sets == 4 &&
                   g_left_sets == 4 && g_top_sets == 4 &&
                   g_set_height == 800 && g_set_width == 1280 &&
                   g_set_left == -1600 && g_set_top == 0 &&
                   g_forwarded_width == 1280 && g_forwarded_height == 800 &&
                   g_outer_calls >= 1,
                   "framed selected mode centered in monitor work area")) return 1;
        fixture.config[1] = 1;
        g_monitor_result = nullptr;
        const int unavailable = function<Activate>(g_activate_replacement)(fixture.engine, nullptr, 0, 3, false);
        if (!check(unavailable == 0 && g_height_sets == 5 && g_width_sets == 5 &&
                   g_set_height == 800 && g_set_width == 1280 &&
                   g_left_sets == 4 && g_top_sets == 4 &&
                   g_forwarded_width == 1280 && g_forwarded_height == 800,
                   "unavailable monitor retains selected form size without positioning")) return 1;
        function<Activate>(g_activate_replacement)(fixture.engine, nullptr, 0, 2, false);
        if (!check(g_height_sets == 5 && g_width_sets == 5 &&
                   g_left_sets == 4 && g_top_sets == 4 && g_monitor_calls == 5,
                   "non-shell activation skips form setters")) return 1;
        const unsigned before_client = g_client_calls;
        const unsigned before_height = g_height_sets;
        g_resize_hwnd_write = 1;
        function<Activate>(g_activate_replacement)(fixture.engine, nullptr, 0, 3, false);
        if (!check(!fake_window && g_client_calls == before_client &&
                   g_height_sets == before_height,
                   "gateway-cleared HWND skips post-resize geometry")) return 1;
        g_resize_hwnd_write = 2;
        function<Activate>(g_activate_replacement)(fixture.engine, nullptr, 0, 3, false);
        if (!check(fake_window == reinterpret_cast<HWND>(1) &&
                   g_client_calls == before_client + 1 &&
                   g_height_sets == before_height + 1,
                   "gateway-created HWND enables post-resize geometry")) return 1;
        g_resize_hwnd_write = 0;
        g_test_window = nullptr;
        g_fake_gateway_clamp = false;
    }
    {
        reset();
        if (!primary_call(at(g_armada, kOriginalPrimary))) return 4;
        Module module(argv[1]);
        if (!check(module.init && module.init(&config) && g_hooks == 2 &&
                   g_calls == 1 && g_bad_bytes == 0,
                   "startup original primary CALL accepted")) return 1;
        if (!primary_call(at(g_fo, kActivate))) return 4;
    }
    {
        // FO commonMemoryFixes describes this patch as one payload byte. Test
        // its exact value without invoking the installer or changing a file.
        std::uint32_t payload_length{};
        std::memcpy(&payload_length, at(g_fo, kFoDepthPayload - 4), sizeof(payload_length));
        const std::uint8_t depth = *at(g_fo, kFoDepthPayload);
        if (!check(payload_length == 1 && depth == 32,
                   "FO commonMemoryFixes depth payload is one byte of 32")) return 1;
        const std::uint8_t original = *at(g_armada, kShellDepth);
        if (!put(at(g_armada, kShellDepth), depth)) return 4;
        reset(); Module module(argv[1]);
        const bool accepted = module.init && module.init(&config) &&
                              g_hooks == 2 && g_calls == 1 && g_bad_bytes == 0;
        if (!put(at(g_armada, kShellDepth), original) ||
            !check(accepted, "FO depth-32 runtime patch accepted")) return 1;
    }
    {
        const std::uint8_t original = *at(g_armada, kShellDepth);
        if (!put(at(g_armada, kShellDepth), std::uint8_t{24})) return 4;
        reset(); Module module(argv[1]);
        const bool rejected = module.init && !module.init(&config) &&
                              g_hooks == 0 && g_calls == 0;
        if (!put(at(g_armada, kShellDepth), original) ||
            !check(rejected, "unsupported depth-24 patch rejected before hooks")) return 1;
    }
    {
        reset();
        if (!primary_call(at(g_armada, kOriginalPrimary + 1))) return 4;
        Module module(argv[1]);
        const bool rejected = module.init && !module.init(&config) &&
                              g_hooks == 0 && g_calls == 0;
        if (!primary_call(at(g_fo, kActivate)) ||
            !check(rejected, "foreign primary CALL target rejected")) return 1;
    }
    {
        reset(); Module module(argv[1]);
        const std::uint8_t original = *at(g_fo, kActivate);
        if (!put(at(g_fo, kActivate), std::uint8_t{0x90})) return 4;
        const bool rejected = module.init && !module.init(&config) && g_hooks == 0 && g_calls == 0;
        if (!put(at(g_fo, kActivate), original) || !check(rejected, "unknown signature rejects before patches")) return 1;
    }
    {
        reset(); Module module(argv[1]);
        HMODULE real_fo = g_fo;
        g_fo = g_armada; // An existing, different PE file identity; no execution.
        const bool rejected = module.init && !module.init(&config) && g_hooks == 0 && g_calls == 0;
        g_fo = real_fo;
        if (!check(rejected, "unknown FleetOpsHook file identity rejects")) return 1;
    }
    {
        reset(); g_fail_resize = true; Module module(argv[1]);
        if (!check(module.init && module.init(&config) && g_hooks == 2 && g_calls == 0 &&
                   g_activate_replacement && !g_resize_replacement,
                   "partial install keeps disabled callback")) return 1;
        // The first inline callback is retained, but without g_ready it must
        // forward. Install only synthetic callbacks for its fake gateway.
        g_lookup_replacement = reinterpret_cast<void*>(at(g_armada, kNativeFind));
        g_resize_replacement = reinterpret_cast<void*>(&fake_resize);
        Fixture fixture;
        if (!invoke(fixture, 0, 3, true, false, false, false, false, false,
                    "partial install pass-through")) return 1;
    }
    {
        reset(); Module module(argv[1]); IatFixture iats;
        auto canvas_api = api(); canvas_api.patch_bytes = patch_bytes;
        if (!check(iats.okay() && module.init && module.init(&canvas_api) &&
                   g_hooks == 4 && g_calls == 2 && g_bad_bytes == 0 &&
                   g_byte_patches == iats.size() && g_register_replacement && g_bounds_replacement && g_checkbox_replacement,
                   "canvas installs registration, bounds, checkbox call and resolved USER32 imports")) return 1;
        using CursorQuery = BOOL (WINAPI*)(LPPOINT);
        if (!check(function<CursorQuery>(g_checkbox_replacement)(nullptr) == GetCursorPos(nullptr),
                   "checkbox cursor callback retains native failure and stdcall stack")) return 1;
        using ClientRect = BOOL (WINAPI*)(HWND, LPRECT);
        RECT rect{};
        void* client_slot = iats.slot("GetClientRect");
        if (!check(client_slot &&
                   function<ClientRect>(*reinterpret_cast<void**>(client_slot))(nullptr, &rect) ==
                       GetClientRect(nullptr, &rect),
                   "untracked window import forwards to USER32")) return 1;
        std::uint8_t form[0x1c8]{};
        test_call_register(g_register_replacement, form, reinterpret_cast<HWND>(1),
                           11, 22, 800, 600);
        if (!check(test_register_calls == 1 && test_register_form == form &&
                   test_register_window == reinterpret_cast<HWND>(1) &&
                   test_register_left == 11 && test_register_top == 22 &&
                   test_register_width == 800 && test_register_height == 600 &&
                   test_stack_before == test_stack_after &&
                   test_ebx_after == 0x13572468 && test_esi_after == 0x24681357 &&
                   test_edi_after == 0x35792468,
                   "Delphi registration forwards registers, stack and preserves nonvolatiles")) return 1;
        Fixture fixture;
        if (!invoke(fixture, 0, 3, false, false, true, false, false, true,
                    "canvas mode activation")) return 1;
        test_call_bounds(g_bounds_replacement, form);
        if (!check(test_bounds_calls == 1 && test_bounds_control == form &&
                   test_stack_before == test_stack_after && test_ebx_after == 0x13572468 &&
                   test_esi_after == 0x24681357 && test_edi_after == 0x35792468,
                   "Delphi UpdateBounds EAX control, no-argument stack and nonvolatiles")) return 1;
        test_call_register(g_register_replacement, nullptr, reinterpret_cast<HWND>(1),
                           -11, 23, 1200, 900);
        if (!check(test_register_calls == 2 && !test_register_form &&
                   test_register_window == reinterpret_cast<HWND>(1) &&
                   test_register_left == -11 && test_register_top == 23 &&
                   test_register_width == 1200 && test_register_height == 900 &&
                   test_stack_before == test_stack_after,
                   "scoped null form remains native pass-through")) return 1;
    }
    {
        reset(); Module module(argv[1]); IatFixture iats;
        auto canvas_api = api(); canvas_api.patch_bytes = patch_bytes;
        const unsigned index = iats.patch_index("GetClientRect");
        g_fail_byte_patch = index + 1;
        if (!check(iats.okay() && index && g_fail_byte_patch <= iats.size() &&
                   module.init && module.init(&canvas_api) && g_hooks == 4 &&
                   g_calls == 2 && g_byte_patches == g_fail_byte_patch &&
                   g_bad_bytes == 0,
                   "mid-import failure retains disabled canvas callbacks")) return 1;
        using ClientRect = BOOL (WINAPI*)(HWND, LPRECT);
        RECT rect{};
        void* client_slot = iats.slot("GetClientRect");
        if (!check(client_slot &&
                   function<ClientRect>(*reinterpret_cast<void**>(client_slot))(nullptr, &rect) ==
                       GetClientRect(nullptr, &rect),
                   "partially installed import remains pass-through")) return 1;
    }
    {
        reset(); Module module(argv[1]); IatFixture iats;
        auto canvas_api = api(); canvas_api.patch_bytes = patch_bytes;
        void* client_slot = iats.slot("GetClientRect");
        if (!check(iats.okay() && client_slot, "foreign IAT fixture prepared")) return 1;
        if (!put(client_slot, reinterpret_cast<void*>(&fake_register_gateway))) return 4;
        if (!check(module.init && module.init(&canvas_api) && g_hooks == 2 &&
                   g_calls == 1 && g_byte_patches == 0 && !g_register_replacement,
                   "foreign IAT owner rejects canvas before canvas mutation")) return 1;
    }
    {
        reset(); Module module(argv[1]); IatFixture iats;
        auto canvas_api = api(); canvas_api.patch_bytes = patch_bytes;
        auto* bounds = at(g_fo, kUpdateBounds);
        const std::uint8_t saved = *bounds;
        if (!put(bounds, std::uint8_t{0x90})) return 4;
        const bool rejected = module.init && module.init(&canvas_api) && g_hooks == 2 &&
            g_byte_patches == 0 && !g_bounds_replacement && !g_register_replacement;
        if (!put(bounds, saved) ||
            !check(rejected, "foreign VCL UpdateBounds signature disables canvas before mutation")) return 1;
    }
    {
        reset(); Module module(argv[1]); IatFixture iats;
        auto canvas_api = api(); canvas_api.patch_bytes = patch_bytes;
        auto* cursor = at(g_fo, kCheckboxCursorCall);
        const std::uint8_t saved = *cursor;
        if (!put(cursor, std::uint8_t{0x90})) return 4;
        const bool rejected = module.init && module.init(&canvas_api) && g_hooks == 2 &&
            g_calls == 1 && g_byte_patches == 0 && !g_checkbox_replacement && !g_register_replacement;
        if (!put(cursor, saved) ||
            !check(rejected, "foreign checkbox cursor CALL disables canvas before mutation")) return 1;
    }
    {
        reset(); Module module(argv[1]); IatFixture iats;
        auto canvas_api = api(); canvas_api.patch_bytes = patch_bytes;
        g_fail_checkbox = true;
        if (!check(iats.okay() && module.init && module.init(&canvas_api) &&
                   g_hooks == 4 && g_calls == 2 && g_byte_patches == 0 &&
                   !g_checkbox_replacement && g_register_replacement && g_bounds_replacement,
                   "checkbox CALL install failure leaves canvas disabled before IAT mutation")) return 1;
        std::uint8_t form[0x1c8]{};
        test_call_register(g_register_replacement, form, reinterpret_cast<HWND>(1), 0, 0, 1200, 900);
        test_call_bounds(g_bounds_replacement, form);
        if (!check(test_register_calls == 1 && test_bounds_calls == 1,
                   "checkbox failure retains native registration and bounds forwarding")) return 1;
    }
    FreeLibrary(g_fo); FreeLibrary(g_armada);
    std::puts("Shell display native smoke: PASS (headless, real DLL and data-only game images)");
    return 0;
}
