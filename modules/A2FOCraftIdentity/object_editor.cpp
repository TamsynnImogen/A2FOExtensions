/*
 * Extend Armada's native object-properties dialog, not its editor input loop.
 * Keep drafts in controls until OK. Craft's own fields are still committed by
 * the native dialog destructor; reapply the four-facing snapshot afterwards.
 *
 * Persistence is an optional prefix at Craft's first scalar IO call. This
 * deliberately leaves Craft::Save/Load entry hooks to A2FOEnergySystems.
 */
#include "object_editor.hpp"
#include "fleetops_object_editor.hpp"
#include "../A2FODirectionalShields/api.hpp"
#include "../../sdk/include/a2fo_supported_armada.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#include <unordered_map>

extern "C" {
std::uintptr_t __cdecl a2fo_identity_call_thiscall_0(void*, void*);
void a2fo_editor_save_prefix_bridge();
bool __cdecl a2fo_editor_write_first_field(
    void*, float, const char*, void*) noexcept;
}

namespace a2fo::object_editor {
namespace {

constexpr std::uintptr_t kDialogProcRva = 0x00101560;
constexpr std::uintptr_t kDialogDestructorRva = 0x00100fe0;
constexpr std::uintptr_t kSaveFirstFieldCallRva = 0x000c299d;
constexpr std::uintptr_t kLoadFirstFieldCallRva = 0x000c2359;
constexpr std::uintptr_t kOutFloatRva = 0x0012ee70;
constexpr std::uintptr_t kInFloatRva = 0x0012efb0;
constexpr std::uintptr_t kOutBytesRva = 0x0012c680;
constexpr std::uintptr_t kActiveObjectRva = 0x00363fec;
constexpr std::uintptr_t kDialogCancelledRva = 0x00363f18;
constexpr std::uintptr_t kDialogLabelRva = 0x00363fbc;
constexpr std::size_t kObjectFlagsOffset = 0x14;
constexpr std::size_t kObjectHandleOffset = 0x28;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kFirstCraftFieldOffset = 0x1c0;
constexpr std::size_t kReaderCurrentOffset = 0x54;
constexpr std::size_t kReaderEndOffset = 0x58;
constexpr int kNativeLabelId = 0x3f2;
constexpr int kNativeShieldCurrentId = 0x3fb;
constexpr int kNativeShieldMaximumId = 0x3fd;
constexpr int kFirstControl = 0x6200;
constexpr int kCaptainEdit = kFirstControl + 1;
constexpr int kCaptainAuto = kFirstControl + 2;
constexpr int kRegistryEdit = kFirstControl + 3;
constexpr int kRegistryAuto = kFirstControl + 4;
constexpr int kShieldEdit = kFirstControl + 8;
constexpr int kLastControl = kFirstControl + 64;
constexpr std::array<std::uint8_t, 6> kDialogSignature{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x50};
constexpr std::array<std::uint8_t, 6> kDestructorSignature{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x3c};
constexpr std::array<std::uint8_t, 5> kSaveSignature{
    0xe8, 0xce, 0xc4, 0x06, 0x00};
constexpr std::array<std::uint8_t, 5> kLoadSignature{
    0xe8, 0x52, 0xcc, 0x06, 0x00};
constexpr std::array<const char*, 4> kFacingNames{
    "Forward", "Aft", "Port", "Starboard"};

using OutFloat = bool (__cdecl *)(void*, float, const char*);
using InFloat = bool (__cdecl *)(void*, float*);
using OutBytes = bool (__cdecl *)(void*, const void*, std::uint32_t,
                                   const char*);
using ShieldEnabled = bool (A2FO_CALL *)(void*);

struct Entry {
    SavedState state{};
    void* object_class = nullptr;
    std::uint32_t handle = 0;
    std::uint64_t revision = 0;
    bool loading = false;
    bool pending_shields = false;
};

struct DialogState {
    HWND window = nullptr;
    void* craft = nullptr;
    RECT original_bounds{};
    std::array<HWND, 32> controls{};
    std::size_t control_count = 0;
    std::array<bool, 2> aggregate_read_only{};
    std::array<char, kTextCapacity> default_captain{};
    std::array<char, kTextCapacity> default_registry{};
    bool active = false;
    bool changing_text = false;
    bool shields = false;
};

struct AcceptedDraft {
    void* craft = nullptr;
    A2FO_DirectionalShieldState shields{};
    bool has_shields = false;
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
ReadDefaults g_read_defaults = nullptr;
bool g_ready = false;
bool g_legacy_ready = false;
A2FO_InlineHook g_dialog_hook{};
A2FO_InlineHook g_destructor_hook{};
std::unordered_map<void*, Entry> g_entries;
std::uint64_t g_revision = 0;
DialogState g_dialog{};
AcceptedDraft g_accepted{};
A2FO_DirectionalShieldsGetStateFn g_get_shields = nullptr;
A2FO_DirectionalShieldsSetStateFn g_set_shields = nullptr;
ShieldEnabled g_shields_enabled = nullptr;

void log_line(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log("A2FOCraftIdentity.ObjectEditor", message);
    OutputDebugStringA("[A2FOCraftIdentity object editor] ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

void* at(std::uintptr_t rva) noexcept {
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uintptr_t>(g_armada) + rva);
}

bool accessible(const void* memory, std::size_t size,
                bool writable = false) noexcept {
    auto cursor = reinterpret_cast<std::uintptr_t>(memory);
    if (!cursor || size > std::numeric_limits<std::uintptr_t>::max() - cursor)
        return false;
    const auto end = cursor + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<const void*>(cursor), &info,
                          sizeof(info)) || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
            return false;
        const DWORD protection = info.Protect & 0xff;
        const bool can_write = protection == PAGE_READWRITE ||
            protection == PAGE_WRITECOPY ||
            protection == PAGE_EXECUTE_READWRITE ||
            protection == PAGE_EXECUTE_WRITECOPY;
        const bool can_read = can_write || protection == PAGE_READONLY ||
            protection == PAGE_EXECUTE_READ;
        if (!can_read || (writable && !can_write)) return false;
        const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if (info.RegionSize > std::numeric_limits<std::uintptr_t>::max() - base)
            return false;
        const auto next = base + info.RegionSize;
        if (next <= cursor) return false;
        cursor = std::min<std::uintptr_t>(end, next);
    }
    return true;
}

template <typename T>
T read_at(const void* memory, std::size_t offset, T fallback = T{}) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    if (!address || offset > std::numeric_limits<std::uintptr_t>::max() - address)
        return fallback;
    const void* source = reinterpret_cast<const void*>(address + offset);
    if (!accessible(source, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, source, sizeof(value));
    return value;
}

bool is_craft(void* craft) noexcept {
    return (read_at<std::uint32_t>(craft, kObjectFlagsOffset) & 8u) != 0 &&
        read_at<void*>(craft, kObjectClassOffset) != nullptr;
}

Entry* find_entry(void* craft) noexcept {
    auto found = g_entries.find(craft);
    if (found == g_entries.end()) return nullptr;
    Entry& entry = found->second;
    if (entry.object_class != read_at<void*>(craft, kObjectClassOffset) ||
        (!entry.loading &&
         entry.handle != read_at<std::uint32_t>(craft, kObjectHandleOffset))) {
        g_entries.erase(found);
        return nullptr;
    }
    return &entry;
}

Entry* prepare_entry(void* craft) noexcept {
    if (Entry* existing = find_entry(craft)) return existing;
    if (!is_craft(craft)) return nullptr;
    try {
        Entry entry{};
        entry.object_class = read_at<void*>(craft, kObjectClassOffset);
        entry.handle = read_at<std::uint32_t>(craft, kObjectHandleOffset);
        return &g_entries.emplace(craft, entry).first->second;
    } catch (...) {
        return nullptr;
    }
}

void assign_entry(Entry* entry, const SavedState& state, bool loading) noexcept {
    entry->state = state;
    entry->revision = ++g_revision;
    if (!entry->revision) entry->revision = ++g_revision;
    entry->loading = loading;
    entry->pending_shields = loading && (state.flags & kShields) != 0;
}

template <typename Function>
Function resolve_export(HMODULE module, const char* name) noexcept {
    FARPROC address = module ? GetProcAddress(module, name) : nullptr;
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(address));
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

bool resolve_shields() noexcept {
    if (g_get_shields && g_set_shields && g_shields_enabled) return true;
    HMODULE module = GetModuleHandleA("A2FODirectionalShields.dll");
    g_get_shields = resolve_export<A2FO_DirectionalShieldsGetStateFn>(
        module, "A2FODirectionalShields_GetState");
    g_set_shields = resolve_export<A2FO_DirectionalShieldsSetStateFn>(
        module, "A2FODirectionalShields_SetState");
    g_shields_enabled = resolve_export<ShieldEnabled>(
        module, "A2FODirectionalShields_IsEnabled");
    return g_get_shields && g_set_shields && g_shields_enabled;
}

void copy_shields(SavedState* state,
                  const A2FO_DirectionalShieldState& shields) noexcept {
    std::copy_n(shields.current, 4, state->current.begin());
    std::copy_n(shields.maximum, 4, state->maximum.begin());
    state->flags |= kShields;
}

A2FO_DirectionalShieldState shield_snapshot(const SavedState& state) noexcept {
    A2FO_DirectionalShieldState shields{};
    shields.struct_size = sizeof(shields);
    std::copy_n(state.current.begin(), 4, shields.current);
    std::copy_n(state.maximum.begin(), 4, shields.maximum);
    return shields;
}

bool make_save_state(void* craft, SavedState* state) noexcept {
    *state = SavedState{};
    if (Entry* entry = find_entry(craft)) *state = entry->state;
    if (resolve_shields() && g_shields_enabled(craft)) {
        A2FO_DirectionalShieldState shields{};
        shields.struct_size = sizeof(shields);
        if (!g_get_shields(craft, &shields)) return false;
        copy_shields(state, shields);
    }
    // Retain loaded shield metadata when the shield module is unavailable.
    return valid_state(*state);
}

bool __cdecl read_first_field(void* reader, float* destination) noexcept {
    auto original = reinterpret_cast<InFloat>(at(kInFloatRva));
    if (!g_ready) return original(reader, destination);
    const auto address = reinterpret_cast<std::uintptr_t>(destination);
    if (address < kFirstCraftFieldOffset ||
        !accessible(reader, kReaderEndOffset + sizeof(void*)) ||
        !accessible(static_cast<char*>(reader) + kReaderCurrentOffset,
                    sizeof(void*), true))
        return false;
    void* craft = reinterpret_cast<void*>(address - kFirstCraftFieldOffset);
    if (!is_craft(craft)) return false;
    g_entries.erase(craft);
    const char* cursor = read_at<const char*>(reader, kReaderCurrentOffset);
    const char* end = read_at<const char*>(reader, kReaderEndOffset);
    const auto begin_address = reinterpret_cast<std::uintptr_t>(cursor);
    const auto end_address = reinterpret_cast<std::uintptr_t>(end);
    if (!begin_address || end_address < begin_address) return false;
    const std::size_t available = std::min<std::size_t>(
        end_address - begin_address, 4096);
    if (!accessible(cursor, available)) return false;
    const bool binary = read_at<std::uint8_t>(reader, 6) != 0;
    const bool tagged = read_at<std::uint8_t>(reader, 7) != 0;
    SavedState state{};
    std::size_t consumed = 0;
    const PrefixResult result = decode_prefix(
        std::string_view(cursor, available), binary, tagged, &state, &consumed);
    if (result == PrefixResult::invalid) {
        log_line("Invalid object-editor save record; refusing to misalign Craft data");
        return false;
    }
    if (result == PrefixResult::present) {
        Entry* entry = prepare_entry(craft);
        if (!entry) return false;
        assign_entry(entry, state, true);
        cursor += consumed;
        std::memcpy(static_cast<char*>(reader) + kReaderCurrentOffset,
                    &cursor, sizeof(cursor));
    }
    if (!original(reader, destination)) {
        g_entries.erase(craft);
        return false;
    }
    return true;
}

void A2FO_CALL craft_event_handler(const A2FO_CraftEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event) || !event->craft) return;
    if (event->kind == A2FO_CRAFT_EVENT_CLEANUP) {
        g_entries.erase(event->craft);
        if (g_accepted.craft == event->craft) g_accepted = AcceptedDraft{};
        return;
    }
    if (!g_ready || event->kind != A2FO_CRAFT_EVENT_POST_LOAD) return;
    if (Entry* entry = find_entry(event->craft)) {
        entry->handle = read_at<std::uint32_t>(event->craft, kObjectHandleOffset);
        entry->loading = false;
        // DirectionalShields consumes its pending snapshot in its own
        // POST_LOAD handler, after clearing its old runtime stores.
    }
}

void set_number(HWND control, float number, bool aggregate = false) noexcept {
    char text[64]{};
    std::snprintf(text, sizeof(text), aggregate ? "%.0f" : "%.9g",
                  static_cast<double>(number));
    SetWindowTextA(control, text);
}

bool read_shield_controls(SavedState* state) noexcept {
    for (std::size_t i = 0; i < 4; ++i) {
        char current[64]{};
        char maximum[64]{};
        GetDlgItemTextA(g_dialog.window, kShieldEdit + static_cast<int>(i) * 2,
                        current, sizeof(current));
        GetDlgItemTextA(g_dialog.window, kShieldEdit + static_cast<int>(i) * 2 + 1,
                        maximum, sizeof(maximum));
        if (!parse_shield_number(current, &state->current[i]) ||
            !parse_shield_number(maximum, &state->maximum[i]))
            return false;
    }
    state->flags |= kShields;
    return valid_state(*state);
}

void mirror_shield_totals(const SavedState& state) noexcept {
    float current = 0.0f;
    float maximum = 0.0f;
    for (std::size_t i = 0; i < 4; ++i) {
        current += state.current[i];
        maximum += state.maximum[i];
    }
    set_number(GetDlgItem(g_dialog.window, kNativeShieldCurrentId), current, true);
    set_number(GetDlgItem(g_dialog.window, kNativeShieldMaximumId), maximum, true);
}

void undo_controls() noexcept {
    g_dialog.active = false;
    for (std::size_t i = 0; i < g_dialog.control_count; ++i) {
        if (g_dialog.controls[i]) DestroyWindow(g_dialog.controls[i]);
    }
    if (g_dialog.shields) {
        SendDlgItemMessageA(g_dialog.window, kNativeShieldCurrentId,
            EM_SETREADONLY, g_dialog.aggregate_read_only[0], 0);
        SendDlgItemMessageA(g_dialog.window, kNativeShieldMaximumId,
            EM_SETREADONLY, g_dialog.aggregate_read_only[1], 0);
    }
    const RECT& rect = g_dialog.original_bounds;
    SetWindowPos(g_dialog.window, nullptr, rect.left, rect.top,
                 rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    g_dialog = DialogState{};
}

bool add_controls(HWND window) noexcept {
    g_accepted = AcceptedDraft{};
    g_dialog = DialogState{};
    void* craft = read_at<void*>(at(kActiveObjectRva), 0);
    if (!is_craft(craft)) return false;
    g_dialog.window = window;
    g_dialog.craft = craft;
    g_dialog.changing_text = true;
    RECT client{};
    RECT units{0, 0, 4, 8};
    if (!GetWindowRect(window, &g_dialog.original_bounds) ||
        !GetClientRect(window, &client) || !MapDialogRect(window, &units)) {
        g_dialog = DialogState{};
        return false;
    }
    const auto x = [&](int value) { return MulDiv(value, units.right, 4); };
    const auto y = [&](int value) { return MulDiv(value, units.bottom, 8); };
    const int pane_width = x(220);
    const int pane_height = y(250);
    int pane_x = client.right + x(6);
    int pane_y = y(6);
    const RECT original = g_dialog.original_bounds;
    const int frame_width = original.right - original.left - client.right;
    const int frame_height = original.bottom - original.top - client.bottom;
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    const bool have_monitor = GetMonitorInfoA(
        MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor) != FALSE;
    if (have_monitor && pane_x + pane_width + x(6) + frame_width >
                            monitor.rcWork.right - monitor.rcWork.left) {
        pane_x = x(6);
        pane_y = client.bottom + y(6);
    }
    const int width = std::max<int>(client.right, pane_x + pane_width + x(6)) +
        frame_width;
    const int height = std::max<int>(client.bottom, pane_y + pane_height + y(6)) +
        frame_height;
    if (have_monitor && (width > monitor.rcWork.right - monitor.rcWork.left ||
                         height > monitor.rcWork.bottom - monitor.rcWork.top)) {
        g_dialog = DialogState{};
        log_line("Object-editor extension does not fit the current work area");
        return false;
    }
    const int left = have_monitor
        ? std::max<int>(monitor.rcWork.left,
            std::min<int>(original.left, monitor.rcWork.right - width))
        : original.left;
    const int top = have_monitor
        ? std::max<int>(monitor.rcWork.top,
            std::min<int>(original.top, monitor.rcWork.bottom - height))
        : original.top;
    if (!SetWindowPos(window, nullptr, left, top, width, height,
                      SWP_NOZORDER | SWP_NOACTIVATE)) {
        g_dialog = DialogState{};
        return false;
    }

    const HFONT font = reinterpret_cast<HFONT>(
        SendMessageA(window, WM_GETFONT, 0, 0));
    bool created = true;
    auto control = [&](const char* klass, const char* text, DWORD style,
                       int id, int cx, int cy, int cw, int ch,
                       DWORD extended = 0) -> HWND {
        if (!created || g_dialog.control_count == g_dialog.controls.size()) {
            created = false;
            return nullptr;
        }
        HWND child = CreateWindowExA(extended, klass, text,
            WS_CHILD | WS_VISIBLE | style, pane_x + x(cx), pane_y + y(cy),
            x(cw), y(ch), window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_armada, nullptr);
        if (!child) {
            created = false;
            return nullptr;
        }
        g_dialog.controls[g_dialog.control_count++] = child;
        if (font) SendMessageA(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return child;
    };
    const auto label = [&](const char* text, int cx, int cy, int cw, int ch) {
        return control("STATIC", text, SS_LEFT, -1, cx, cy, cw, ch);
    };
    const auto edit = [&](int id, int cx, int cy, int cw, int limit) {
        HWND child = control("EDIT", "", WS_TABSTOP | ES_AUTOHSCROLL,
            id, cx, cy, cw, 14, WS_EX_CLIENTEDGE);
        if (child) SendMessageA(child, EM_SETLIMITTEXT, limit, 0);
        return child;
    };
    control("BUTTON", "Per-ship properties", BS_GROUPBOX,
            -1, 0, 0, 220, 250);
    label("Captain", 8, 14, 200, 10);
    edit(kCaptainEdit, 8, 26, 204, static_cast<int>(kTextCapacity - 1));
    control("BUTTON", "Use ODF name list", WS_TABSTOP | BS_AUTOCHECKBOX,
            kCaptainAuto, 8, 42, 200, 12);
    label("Registry", 8, 58, 200, 10);
    edit(kRegistryEdit, 8, 70, 204, static_cast<int>(kTextCapacity - 1));
    control("BUTTON", "Use ODF name list", WS_TABSTOP | BS_AUTOCHECKBOX,
            kRegistryAuto, 8, 86, 200, 12);
    label("Directional shields", 8, 106, 204, 11);
    label("Facing", 8, 122, 66, 11);
    label("Current", 78, 122, 60, 11);
    label("Maximum", 146, 122, 66, 11);
    for (std::size_t i = 0; i < 4; ++i) {
        const int row_y = 136 + static_cast<int>(i) * 20;
        label(kFacingNames[i], 8, row_y + 2, 66, 11);
        edit(kShieldEdit + static_cast<int>(i) * 2, 78, row_y, 60, 63);
        edit(kShieldEdit + static_cast<int>(i) * 2 + 1, 146, row_y, 66, 63);
    }

    SavedState state{};
    if (const Entry* entry = find_entry(craft)) state = entry->state;
    if (g_read_defaults) {
        g_read_defaults(craft, g_dialog.default_captain.data(),
            g_dialog.default_registry.data(), kTextCapacity);
        g_dialog.default_captain.back() = '\0';
        g_dialog.default_registry.back() = '\0';
    }
    SetDlgItemTextA(window, kCaptainEdit, (state.flags & kCaptain)
        ? state.captain.data() : g_dialog.default_captain.data());
    SetDlgItemTextA(window, kRegistryEdit, (state.flags & kRegistry)
        ? state.registry.data() : g_dialog.default_registry.data());
    CheckDlgButton(window, kCaptainAuto,
        (state.flags & kCaptain) ? BST_UNCHECKED : BST_CHECKED);
    CheckDlgButton(window, kRegistryAuto,
        (state.flags & kRegistry) ? BST_UNCHECKED : BST_CHECKED);

    A2FO_DirectionalShieldState shields{};
    shields.struct_size = sizeof(shields);
    g_dialog.shields = resolve_shields() && g_shields_enabled(craft) &&
        g_get_shields(craft, &shields);
    for (std::size_t i = 0; i < 4; ++i) {
        HWND current = GetDlgItem(window, kShieldEdit + static_cast<int>(i) * 2);
        HWND maximum = GetDlgItem(window, kShieldEdit + static_cast<int>(i) * 2 + 1);
        if (g_dialog.shields) {
            set_number(current, shields.current[i]);
            set_number(maximum, shields.maximum[i]);
        }
        EnableWindow(current, g_dialog.shields);
        EnableWindow(maximum, g_dialog.shields);
    }
    label(g_dialog.shields
        ? "Current: 0 to maximum. Maximum must be positive.\r\nNative shield totals follow these four facings."
        : "Requires directionalShields = 1 and the updated\r\nA2FODirectionalShields module.",
        8, 218, 204, 26);
    if (g_dialog.shields) {
        HWND current = GetDlgItem(window, kNativeShieldCurrentId);
        HWND maximum = GetDlgItem(window, kNativeShieldMaximumId);
        g_dialog.aggregate_read_only[0] =
            (GetWindowLongPtrA(current, GWL_STYLE) & ES_READONLY) != 0;
        g_dialog.aggregate_read_only[1] =
            (GetWindowLongPtrA(maximum, GWL_STYLE) & ES_READONLY) != 0;
        if (!current || !maximum ||
            !SendMessageA(current, EM_SETREADONLY, TRUE, 0) ||
            !SendMessageA(maximum, EM_SETREADONLY, TRUE, 0))
            created = false;
    }
    if (!created) {
        undo_controls();
        log_line("Could not create object-editor controls; native dialog retained");
        return false;
    }
    if (g_dialog.shields) {
        copy_shields(&state, shields);
        mirror_shield_totals(state);
    }
    g_dialog.changing_text = false;
    g_dialog.active = true;
    return true;
}

bool accept_controls() noexcept {
    char label[46]{};
    GetDlgItemTextA(g_dialog.window, kNativeLabelId, label, sizeof(label));
    if (!label[0]) {
        MessageBoxA(g_dialog.window,
            "Enter an object label before applying these properties.\n"
            "The native editor does not commit an object with an empty label.",
            "Object properties", MB_OK | MB_ICONWARNING);
        return false;
    }
    SavedState state{};
    if (const Entry* entry = find_entry(g_dialog.craft)) state = entry->state;
    state.flags &= ~(kCaptain | kRegistry);
    state.captain.fill('\0');
    state.registry.fill('\0');
    if (IsDlgButtonChecked(g_dialog.window, kCaptainAuto) != BST_CHECKED) {
        state.flags |= kCaptain;
        GetDlgItemTextA(g_dialog.window, kCaptainEdit, state.captain.data(),
                        static_cast<int>(state.captain.size()));
    }
    if (IsDlgButtonChecked(g_dialog.window, kRegistryAuto) != BST_CHECKED) {
        state.flags |= kRegistry;
        GetDlgItemTextA(g_dialog.window, kRegistryEdit, state.registry.data(),
                        static_cast<int>(state.registry.size()));
    }
    if (g_dialog.shields && !read_shield_controls(&state)) {
        MessageBoxA(g_dialog.window,
            "Each shield value must be a finite number.\n"
            "Maximum must be greater than zero, and current must be between\n"
            "zero and its maximum. The combined totals must also be finite.",
            "Directional shields", MB_OK | MB_ICONWARNING);
        return false;
    }
    Entry* entry = prepare_entry(g_dialog.craft);
    if (!entry) {
        MessageBoxA(g_dialog.window, "Could not allocate the per-ship properties.",
                    "Object properties", MB_OK | MB_ICONERROR);
        return false;
    }
    const auto shields = shield_snapshot(state);
    if (g_dialog.shields && (!g_set_shields ||
                             !g_set_shields(g_dialog.craft, &shields))) {
        MessageBoxA(g_dialog.window,
            "The directional-shield state could not be applied.\n"
            "The dialog has been left open; no identity changes were committed.",
            "Directional shields", MB_OK | MB_ICONERROR);
        return false;
    }
    assign_entry(entry, state, false);
    g_accepted.craft = g_dialog.craft;
    g_accepted.shields = shields;
    g_accepted.has_shields = g_dialog.shields;
    if (g_dialog.shields) mirror_shield_totals(state);
    return true;
}

INT_PTR CALLBACK dialog_proc(HWND window, UINT message,
                             WPARAM wparam, LPARAM lparam) noexcept {
    auto original = reinterpret_cast<DLGPROC>(g_dialog_hook.gateway);
    if (!original) return FALSE;
    if (!g_ready || !g_legacy_ready)
        return original(window, message, wparam, lparam);
    if (message == WM_INITDIALOG) {
        const INT_PTR result = original(window, message, wparam, lparam);
        add_controls(window);
        return result;
    }
    if (g_dialog.window == window && message == WM_COMMAND) {
        const int id = LOWORD(wparam);
        const int notification = HIWORD(wparam);
        if (id >= kFirstControl && id < kLastControl) {
            if (!g_dialog.active || g_dialog.changing_text) return TRUE;
            if (notification == EN_CHANGE &&
                (id == kCaptainEdit || id == kRegistryEdit)) {
                CheckDlgButton(window,
                    id == kCaptainEdit ? kCaptainAuto : kRegistryAuto,
                    BST_UNCHECKED);
            } else if (notification == BN_CLICKED &&
                       (id == kCaptainAuto || id == kRegistryAuto) &&
                       IsDlgButtonChecked(window, id) == BST_CHECKED) {
                g_dialog.changing_text = true;
                SetDlgItemTextA(window,
                    id == kCaptainAuto ? kCaptainEdit : kRegistryEdit,
                    id == kCaptainAuto ? g_dialog.default_captain.data()
                                       : g_dialog.default_registry.data());
                g_dialog.changing_text = false;
            } else if (g_dialog.shields && notification == EN_CHANGE &&
                       id >= kShieldEdit && id < kShieldEdit + 8) {
                SavedState state{};
                if (read_shield_controls(&state)) mirror_shield_totals(state);
            }
            return TRUE;
        }
        if (g_dialog.active && id == IDOK && !accept_controls()) return TRUE;
        if (id == IDCANCEL) g_accepted = AcceptedDraft{};
    }
    if (g_dialog.window == window && message == WM_CLOSE)
        g_accepted = AcceptedDraft{};
    const INT_PTR result = original(window, message, wparam, lparam);
    if (g_dialog.window == window && message == WM_NCDESTROY)
        g_dialog = DialogState{};
    return result;
}

std::uintptr_t __attribute__((fastcall)) dialog_destructor(
    void* self, void*) noexcept {
    if (!g_destructor_hook.gateway) return 0;
    const bool accepted = g_ready && g_legacy_ready && g_accepted.craft &&
        g_accepted.craft == read_at<void*>(at(kActiveObjectRva), 0) &&
        read_at<std::uint8_t>(at(kDialogCancelledRva), 0, 1) == 0 &&
        read_at<char>(at(kDialogLabelRva), 0) != '\0';
    const auto result = a2fo_identity_call_thiscall_0(
        g_destructor_hook.gateway, self);
    if (accepted && g_accepted.has_shields && g_set_shields &&
        !g_set_shields(g_accepted.craft, &g_accepted.shields)) {
        log_line("Could not reapply facing values after the native object-dialog commit");
    }
    g_accepted = AcceptedDraft{};
    return result;
}

template <std::size_t Size>
bool signature_matches(std::uintptr_t rva,
                       const std::array<std::uint8_t, Size>& expected) noexcept {
    return accessible(at(rva), Size) &&
        std::memcmp(at(rva), expected.data(), Size) == 0;
}

bool supported_image() noexcept {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_armada);
    if (!accessible(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE ||
        dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000)
        return false;
    const auto* nt = static_cast<const IMAGE_NT_HEADERS32*>(at(dos->e_lfanew));
    return accessible(nt, sizeof(*nt)) &&
        a2fo::supported_armada::identify(g_armada) !=
            a2fo::supported_armada::Identity::unsupported;
}

}  // namespace

const SavedState* overrides(void* craft) noexcept {
    if (!g_ready) return nullptr;
    const Entry* entry = find_entry(craft);
    return entry ? &entry->state : nullptr;
}

std::uint64_t revision(void* craft) noexcept {
    if (!g_ready) return 0;
    const Entry* entry = find_entry(craft);
    return entry ? entry->revision : 0;
}

bool read_values(void* craft, SavedState* state, bool* directional) noexcept {
    if (!g_ready || !state || !directional || !is_craft(craft) ||
        !make_save_state(craft, state)) return false;
    *directional = resolve_shields() && g_shields_enabled(craft);
    SavedState defaults{};
    if (g_read_defaults) {
        g_read_defaults(craft, defaults.captain.data(), defaults.registry.data(),
                        kTextCapacity);
        defaults.captain.back() = '\0';
        defaults.registry.back() = '\0';
    }
    if (!(state->flags & kCaptain)) state->captain = defaults.captain;
    if (!(state->flags & kRegistry)) state->registry = defaults.registry;
    return valid_state(*state);
}

bool apply_values(void* craft, const SavedState& state,
                  bool apply_directional) noexcept {
    if (!g_ready || !is_craft(craft) || !valid_state(state)) return false;
    Entry* entry = prepare_entry(craft);
    if (!entry) return false;
    if (apply_directional) {
        if (!(state.flags & kShields) || !resolve_shields()) return false;
        const auto shields = shield_snapshot(state);
        if (!g_set_shields(craft, &shields)) return false;
    }
    assign_entry(entry, state, false);
    return true;
}

bool initialize(const A2FO_ModuleApi* api, HMODULE armada,
                ReadDefaults defaults) noexcept {
    if (g_ready) return true;
    g_api = api;
    g_armada = armada;
    g_read_defaults = defaults;
    if (!api || !armada || !api->install_inline_hook || !api->patch_call ||
        !A2FO_MODULE_API_HAS(api, register_craft_event_handler) ||
        !api->register_craft_event_handler ||
        (api->capabilities & A2FO_CAP_CRAFT_EVENTS) == 0 ||
        !supported_image() ||
        !signature_matches(kSaveFirstFieldCallRva, kSaveSignature) ||
        !signature_matches(kLoadFirstFieldCallRva, kLoadSignature)) {
        log_line("Craft persistence signatures or lifecycle API unavailable; extension disabled");
        return false;
    }
    if (!api->register_craft_event_handler(
            "A2FOCraftIdentity.ObjectEditor", &craft_event_handler, nullptr)) {
        log_line("Could not register per-ship property cleanup/load handling");
        return false;
    }
    g_ready = api->patch_call(
        at(kSaveFirstFieldCallRva),
        reinterpret_cast<void*>(&a2fo_editor_save_prefix_bridge),
        kSaveSignature.data(), kSaveSignature.size()) &&
        api->patch_call(at(kLoadFirstFieldCallRva),
            reinterpret_cast<void*>(&read_first_field),
            kLoadSignature.data(), kLoadSignature.size());
    if (!g_ready) {
        log_line("Object-editor persistence installation incomplete; hooks remain pass-through");
        return false;
    }
    // Fleet Ops' property-grid form does not use the legacy Armada dialog.
    // Its availability must not depend on optional legacy dialog signatures.
    g_legacy_ready = signature_matches(kDialogProcRva, kDialogSignature) &&
        signature_matches(kDialogDestructorRva, kDestructorSignature) &&
        api->install_inline_hook(at(kDialogDestructorRva),
            reinterpret_cast<void*>(&dialog_destructor),
            kDestructorSignature.size(), kDestructorSignature.data(),
            &g_destructor_hook) && g_destructor_hook.gateway &&
        api->install_inline_hook(at(kDialogProcRva),
            reinterpret_cast<void*>(&dialog_proc), kDialogSignature.size(),
            kDialogSignature.data(), &g_dialog_hook) && g_dialog_hook.gateway;
    log_line(g_legacy_ready
        ? "Legacy Armada object dialog and versioned per-ship persistence installed"
        : "Legacy Armada dialog unavailable; per-ship persistence remains enabled");
    fleetops::initialize(api, api->fleetops_module
        ? static_cast<HMODULE>(api->fleetops_module()) : nullptr);
    return true;
}

bool write_first_field(void* writer, float value, const char* label,
                       void* craft) noexcept {
    const auto original = reinterpret_cast<OutFloat>(at(kOutFloatRva));
    if (g_ready) {
        SavedState state{};
        if (!is_craft(craft) || !make_save_state(craft, &state)) return false;
        if (state.flags != 0) {
            const auto out_bytes = reinterpret_cast<OutBytes>(at(kOutBytesRva));
            if (!out_bytes(writer, &state, sizeof(state), kSaveLabel)) return false;
        }
    }
    return original(writer, value, label);
}

bool take_loaded_shields(void* craft,
                         A2FO_DirectionalShieldState* result) noexcept {
    if (!g_ready || !result || result->struct_size < sizeof(*result)) return false;
    Entry* entry = find_entry(craft);
    if (!entry || !entry->pending_shields || !(entry->state.flags & kShields))
        return false;
    *result = shield_snapshot(entry->state);
    entry->pending_shields = false;
    return true;
}

}  // namespace a2fo::object_editor

extern "C" bool __cdecl a2fo_editor_write_first_field(
    void* writer, float value, const char* label, void* craft) noexcept {
    return a2fo::object_editor::write_first_field(writer, value, label, craft);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FOCraftIdentity_TakeLoadedDirectionalShields(
    void* craft, A2FO_DirectionalShieldState* state) {
    return a2fo::object_editor::take_loaded_shields(craft, state);
}
