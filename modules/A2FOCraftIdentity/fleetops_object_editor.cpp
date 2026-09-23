/*
 * Fleet Ops replaces the old Armada object dialog with a Delphi form and
 * TNextInspector. Register real property descriptors under its Craft node.
 * The form's OK handler validates a complete draft before native Apply/Close;
 * Cancel and window close never invoke this handler.
 */
#include "fleetops_object_editor.hpp"
#include "object_editor.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

extern "C" {
std::uintptr_t __cdecl a2fo_identity_call_delphi_2(
    void*, std::uintptr_t, std::uintptr_t);
void* __cdecl a2fo_fo_editor_add_property(
    void* function, void* form, std::uint32_t type, const char* caption,
    void* parent, void* value, std::uint32_t writable);
void a2fo_fo_editor_build_bridge();
void a2fo_fo_editor_ok_bridge();
void a2fo_fo_editor_destroy_bridge();
}

namespace a2fo::object_editor::fleetops {
namespace {

constexpr std::uintptr_t kBuildCraftRva = 0x1b90d8;
constexpr std::uintptr_t kAddPropertyRva = 0x1b8904;
constexpr std::uintptr_t kOkayRva = 0x1baf34;
constexpr std::uintptr_t kDestroyRva = 0x1b88c8;
constexpr std::uintptr_t kApplyRva = 0x1bacc0;
constexpr std::uintptr_t kCloseRva = 0x0b1b08;
constexpr std::uintptr_t kActiveObjectRva = 0x2447f8;
constexpr std::uintptr_t kWideStringClearRva = 0x005f38;

constexpr std::array<std::uint8_t, 5> kBuildSignature{
    0x55, 0x8b, 0xec, 0x33, 0xc9};
constexpr std::array<std::uint8_t, 5> kOkaySignature{
    0x53, 0x8b, 0xd8, 0x8b, 0xc3};
constexpr std::array<std::uint8_t, 5> kDestroySignature{
    0x53, 0x56, 0x57, 0x8b, 0xf8};
constexpr std::array<std::uint8_t, 5> kPropertySignature{
    0x55, 0x8b, 0xec, 0x51, 0x53};
constexpr std::array<std::uint8_t, 6> kApplySignature{
    0x53, 0x56, 0x57, 0x83, 0xc4, 0xd0};
constexpr std::array<std::uint8_t, 5> kOkayApplyCall{
    0xe8, 0x82, 0xfd, 0xff, 0xff};
constexpr std::array<std::uint8_t, 5> kOkayCloseCall{
    0xe8, 0xc3, 0x6b, 0xef, 0xff};

constexpr std::array<const char*, 12> kCaptions{
    "Captain", "Registry", "Reset captain to ODF", "Reset registry to ODF",
    "Forward current", "Forward maximum", "Aft current", "Aft maximum",
    "Port current", "Port maximum", "Starboard current", "Starboard maximum"};

struct Session {
    void* form = nullptr;
    void* craft = nullptr;
    void* object_class = nullptr;
    std::uint32_t handle = 0;
    SavedState initial{};
    std::array<char*, 2> text_pointers{};
    std::array<std::uint8_t, 2> reset{};
    std::array<void*, 12> rows{};
    bool shields = false;
    bool complete = false;
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_module = nullptr;
bool g_ready = false;
A2FO_InlineHook g_build_hook{};
A2FO_InlineHook g_okay_hook{};
A2FO_InlineHook g_destroy_hook{};
Session g_session{};

void log_line(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log("A2FOCraftIdentity", message);
}

void* at(std::uintptr_t rva) noexcept {
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uintptr_t>(g_module) + rva);
}

bool readable(const void* memory, std::size_t size) noexcept {
    auto cursor = reinterpret_cast<std::uintptr_t>(memory);
    if (!cursor || size > std::numeric_limits<std::uintptr_t>::max() - cursor)
        return false;
    const std::uintptr_t end = cursor + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<const void*>(cursor), &info,
                          sizeof(info)) || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
            return false;
        const DWORD protection = info.Protect & 0xff;
        if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
            protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
            protection != PAGE_EXECUTE_READWRITE &&
            protection != PAGE_EXECUTE_WRITECOPY)
            return false;
        const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if (info.RegionSize > std::numeric_limits<std::uintptr_t>::max() - base)
            return false;
        const std::uintptr_t next = base + info.RegionSize;
        if (next <= cursor) return false;
        cursor = std::min(end, next);
    }
    return true;
}

template <typename T>
T read_at(const void* memory, std::size_t offset) noexcept {
    T value{};
    if (!memory) return value;
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    if (offset > std::numeric_limits<std::uintptr_t>::max() - address)
        return value;
    const void* source = reinterpret_cast<const void*>(address + offset);
    if (readable(source, sizeof(value))) std::memcpy(&value, source, sizeof(value));
    return value;
}

std::uintptr_t call(void* function, void* self,
                    std::uintptr_t argument = 0) noexcept {
    return a2fo_identity_call_delphi_2(
        function, reinterpret_cast<std::uintptr_t>(self), argument);
}

void* row_method(void* row, std::size_t offset) noexcept {
    return read_at<void*>(read_at<void*>(row, 0), offset);
}

bool read_text(void* row, char* output, std::size_t capacity) noexcept {
    if (!row || !output || !capacity) return false;
    output[0] = '\0';
    void* getter = row_method(row, 0x48);
    if (!readable(getter, 1)) return false;
    // TNxPropertyItem.GetAsString returns a Delphi WideString through EDX.
    // Use Fleet Ops' own clear routine to match its allocator and ownership.
    wchar_t* wide = nullptr;
    call(getter, row, reinterpret_cast<std::uintptr_t>(&wide));
    bool result = true;
    if (wide) {
        const auto address = reinterpret_cast<std::uintptr_t>(wide);
        const std::uint32_t bytes = address >= 4
            ? read_at<std::uint32_t>(reinterpret_cast<void*>(address - 4), 0)
            : 0;
        result = bytes <= 65536 && (bytes % sizeof(wchar_t)) == 0 &&
            readable(wide, static_cast<std::size_t>(bytes) + sizeof(wchar_t));
        if (result) {
            const int characters = static_cast<int>(bytes / sizeof(wchar_t));
            if (characters) {
                const int written = WideCharToMultiByte(CP_ACP, 0, wide,
                    characters, output, static_cast<int>(capacity - 1),
                    nullptr, nullptr);
                result = written > 0;
                if (result) {
                    output[written] = '\0';
                    result = std::memchr(output, '\0', written) == nullptr;
                }
            }
        }
        call(at(kWideStringClearRva), &wide);
    }
    return result;
}

bool checked(void* row, bool* result) noexcept {
    void* getter = row_method(row, 0x38);
    if (!row || !result || !readable(getter, 1)) return false;
    *result = (call(getter, row) & 0xffu) != 0;
    return true;
}

void* add_row(void* form, void* parent, const char* caption,
              std::uint32_t type, void* value) noexcept {
    // Delphi AnsiString literals have {-1, byte_length} before the characters.
    // The native constructor copies this caption into its managed WideString.
    struct Caption {
        std::int32_t references = -1;
        std::int32_t length = 0;
        char text[80]{};
    } label;
    static_assert(offsetof(Caption, text) == 8);
    std::snprintf(label.text, sizeof(label.text), "%s", caption);
    label.length = static_cast<std::int32_t>(std::strlen(label.text));
    void* descriptor = a2fo_fo_editor_add_property(
        at(kAddPropertyRva), form, type, label.text, parent, value, 1);
    return read_at<void*>(descriptor, 8);
}

bool matching_session(void* form) noexcept {
    return g_session.form == form && g_session.craft &&
        g_session.craft == read_at<void*>(at(kActiveObjectRva), 0) &&
        g_session.handle == read_at<std::uint32_t>(g_session.craft, 0x28) &&
        g_session.object_class == read_at<void*>(g_session.craft, 0x40);
}

void warning(const char* text) noexcept {
    // Normal application dialog behavior; no external input automation.
    MessageBoxA(nullptr, text, "Fleet Ops object properties",
                MB_OK | MB_ICONWARNING | MB_TASKMODAL);
}

bool read_draft(SavedState* state) noexcept {
    *state = g_session.initial;
    if (!read_text(g_session.rows[0], state->captain.data(), kTextCapacity) ||
        !read_text(g_session.rows[1], state->registry.data(), kTextCapacity)) {
        warning("Captain and registry must fit within 511 bytes.\n"
                "No properties have been applied.");
        return false;
    }
    for (std::size_t i = 0; i < 2; ++i) {
        bool reset = false;
        if (!checked(g_session.rows[2 + i], &reset)) return false;
        const std::uint32_t flag = i == 0 ? kCaptain : kRegistry;
        const auto& text = i == 0 ? state->captain : state->registry;
        const auto& initial = i == 0 ? g_session.initial.captain
                                    : g_session.initial.registry;
        if (reset) state->flags &= ~flag;
        else if (std::strcmp(text.data(), initial.data()) != 0) state->flags |= flag;
    }
    if (g_session.shields) {
        for (std::size_t i = 0; i < 4; ++i) {
            char current[64]{};
            char maximum[64]{};
            if (!read_text(g_session.rows[4 + i * 2], current, sizeof(current)) ||
                !read_text(g_session.rows[5 + i * 2], maximum, sizeof(maximum)) ||
                !parse_shield_number(current, &state->current[i]) ||
                !parse_shield_number(maximum, &state->maximum[i])) {
                warning("Each directional shield value must be a finite number.\n"
                        "No properties have been applied.");
                return false;
            }
        }
        state->flags |= kShields;
    }
    if (!valid_state(*state)) {
        warning("Each shield maximum must be greater than zero.\n"
                "Current must be between zero and that facing's maximum,\n"
                "and the combined totals must be finite.\n"
                "No properties have been applied.");
        return false;
    }
    return true;
}

template <std::size_t Size>
bool signature(std::uintptr_t rva,
               const std::array<std::uint8_t, Size>& expected) noexcept {
    return readable(at(rva), Size) &&
        std::memcmp(at(rva), expected.data(), Size) == 0;
}

}  // namespace

void build(void* form, void* parent) noexcept {
    if (!g_build_hook.gateway) return;
    call(g_build_hook.gateway, form, reinterpret_cast<std::uintptr_t>(parent));
    if (!g_ready) return;
    g_session = Session{};
    void* craft = read_at<void*>(at(kActiveObjectRva), 0);
    SavedState values{};
    bool shields = false;
    if (!read_values(craft, &values, &shields)) {
        log_line("Fleet Ops inspector: could not snapshot this Craft's properties");
        return;
    }
    g_session.form = form;
    g_session.craft = craft;
    g_session.object_class = read_at<void*>(craft, 0x40);
    g_session.handle = read_at<std::uint32_t>(craft, 0x28);
    g_session.initial = values;
    g_session.shields = shields;
    g_session.text_pointers = {g_session.initial.captain.data(),
                              g_session.initial.registry.data()};
    g_session.rows[0] = add_row(form, parent, kCaptions[0], 5,
                                &g_session.text_pointers[0]);
    g_session.rows[1] = add_row(form, parent, kCaptions[1], 5,
                                &g_session.text_pointers[1]);
    g_session.rows[2] = add_row(form, parent, kCaptions[2], 3, &g_session.reset[0]);
    g_session.rows[3] = add_row(form, parent, kCaptions[3], 3, &g_session.reset[1]);
    if (shields) {
        for (std::size_t i = 0; i < 4; ++i) {
            g_session.rows[4 + i * 2] = add_row(form, parent,
                kCaptions[4 + i * 2], 1, &g_session.initial.current[i]);
            g_session.rows[5 + i * 2] = add_row(form, parent,
                kCaptions[5 + i * 2], 1, &g_session.initial.maximum[i]);
        }
    }
    g_session.complete = true;
    for (std::size_t i = 0; i < (shields ? 12u : 4u); ++i)
        g_session.complete = g_session.complete && g_session.rows[i];
    log_line(g_session.complete
        ? (shields ? "Fleet Ops inspector: added captain, registry and eight facing values under Craft"
                   : "Fleet Ops inspector: added captain and registry; directional shields are not enabled")
        : "Fleet Ops inspector: incomplete rows; OK is blocked to avoid partial edits");
}

void okay(void* form, void* sender) noexcept {
    if (!g_okay_hook.gateway) return;
    if (!g_ready || !matching_session(form)) {
        call(g_okay_hook.gateway, form, reinterpret_cast<std::uintptr_t>(sender));
        return;
    }
    if (!g_session.complete) {
        warning("The extension properties could not be created completely.\n"
                "Please cancel and reopen this object inspector.");
        return;
    }
    SavedState state{};
    if (!read_draft(&state)) return;
    void* craft = g_session.craft;
    const bool shields = g_session.shields;
    // Allocate per-Craft storage and verify the shield bridge before the
    // native apply loop can change ship name, health, position, or crew.
    if (!apply_values(craft, state, shields)) {
        warning("The per-ship properties could not be applied.\n"
                "The inspector has been left open.");
        return;
    }
    call(at(kApplyRva), form);
    // Native curShields/maxShields are aggregates. The four facing values
    // are authoritative, so reapply them after Fleet Ops writes those totals.
    if (shields && !apply_values(craft, state, true)) {
        log_line("Fleet Ops inspector: failed to restore facing totals after native Apply");
        warning("The native properties were applied, but restoring the directional\n"
                "shield totals failed. Please cancel and reopen the inspector.");
        return;
    }
    log_line("Fleet Ops inspector: committed per-ship properties");
    call(at(kCloseRva), form);
}

void destroy(void* form, void* sender) noexcept {
    if (g_destroy_hook.gateway)
        call(g_destroy_hook.gateway, form, reinterpret_cast<std::uintptr_t>(sender));
    if (g_session.form == form) g_session = Session{};
}

bool initialize(const A2FO_ModuleApi* api, HMODULE module) noexcept {
    if (g_ready) return true;
    g_api = api;
    g_module = module;
    if (!api || !module || !api->install_inline_hook ||
        !signature(kBuildCraftRva, kBuildSignature) ||
        !signature(kOkayRva, kOkaySignature) ||
        !signature(kDestroyRva, kDestroySignature) ||
        !signature(kAddPropertyRva, kPropertySignature) ||
        !signature(kApplyRva, kApplySignature) ||
        !signature(kOkayRva + 5, kOkayApplyCall) ||
        !signature(kOkayRva + 12, kOkayCloseCall)) {
        log_line("Fleet Ops object inspector signatures are unsupported; property-grid extension disabled");
        return false;
    }
    g_ready = api->install_inline_hook(at(kBuildCraftRva),
        reinterpret_cast<void*>(&a2fo_fo_editor_build_bridge),
        kBuildSignature.size(), kBuildSignature.data(), &g_build_hook) &&
        g_build_hook.gateway &&
        api->install_inline_hook(at(kOkayRva),
            reinterpret_cast<void*>(&a2fo_fo_editor_ok_bridge),
            kOkaySignature.size(), kOkaySignature.data(), &g_okay_hook) &&
        g_okay_hook.gateway &&
        api->install_inline_hook(at(kDestroyRva),
            reinterpret_cast<void*>(&a2fo_fo_editor_destroy_bridge),
            kDestroySignature.size(), kDestroySignature.data(), &g_destroy_hook) &&
        g_destroy_hook.gateway;
    log_line(g_ready
        ? "Fleet Ops advanced object inspector hooks installed (Craft rows / OK / destroy)"
        : "Fleet Ops object inspector installation incomplete; hooks remain pass-through");
    return g_ready;
}

}  // namespace a2fo::object_editor::fleetops

extern "C" void __cdecl a2fo_fo_editor_build_cpp(void* form, void* parent) noexcept {
    a2fo::object_editor::fleetops::build(form, parent);
}
extern "C" void __cdecl a2fo_fo_editor_ok_cpp(void* form, void* sender) noexcept {
    a2fo::object_editor::fleetops::okay(form, sender);
}
extern "C" void __cdecl a2fo_fo_editor_destroy_cpp(void* form, void* sender) noexcept {
    a2fo::object_editor::fleetops::destroy(form, sender);
}
